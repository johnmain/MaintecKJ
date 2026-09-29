#include "DatabaseManager.h"
#include "FolderScanner.h"
#include <QDebug>
#include <QDateTime>
#include <QRegularExpression>
#include <QFileInfo>
#include <QSet>
#include <QHash>
#include <QProcess>
#include <QtConcurrent>

namespace {

// Song lengths come from ffprobe (shipped with ffmpeg). Returns -1 when the
// length cannot be determined.
int probeDurationSeconds(const QString &filePath)
{
    QProcess probe;
    probe.start(QStringLiteral("ffprobe"),
                { QStringLiteral("-v"), QStringLiteral("error"),
                  QStringLiteral("-show_entries"), QStringLiteral("format=duration"),
                  QStringLiteral("-of"), QStringLiteral("csv=p=0"), filePath });
    if (!probe.waitForFinished(10000)) {
        probe.kill();
        probe.waitForFinished(1000);
        return -1;
    }

    bool ok = false;
    const double seconds = QString::fromLatin1(probe.readAllStandardOutput()).trimmed().toDouble(&ok);
    if (!ok || seconds <= 0.0)
        return -1;
    return static_cast<int>(seconds + 0.5);
}

// Lengths already stored for a directory, keyed by file path. Rows written by
// older versions hold 0, which counts as unknown so they get measured once.
QHash<QString, int> knownDurationsFor(QSqlDatabase &db, int directoryId,
                                      const QString &songTable = QStringLiteral("songs"))
{
    QHash<QString, int> durations;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT file_path, duration FROM %1 WHERE directory_id = ?")
                      .arg(songTable));
    query.addBindValue(directoryId);
    if (query.exec()) {
        while (query.next()) {
            const int seconds = query.value(1).toInt();
            if (seconds > 0)
                durations.insert(query.value(0).toString(), seconds);
        }
    }
    return durations;
}

int countSongs(QSqlDatabase &db, const QString &songTable = QStringLiteral("songs"))
{
    QSqlQuery query(db);
    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM %1 WHERE is_deleted = 0").arg(songTable))
        && query.next())
        return query.value(0).toInt();
    return 0;
}

bool insertSongRow(QSqlDatabase &db, const QString &artist, const QString &title,
                   const QString &filePath, int duration, int directoryId,
                   const QString &source = QString(),
                   const QString &songTable = QStringLiteral("songs"))
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO %1 (artist, title, file_path, duration, directory_id, source) "
        "VALUES (?, ?, ?, ?, ?, ?)").arg(songTable));
    query.addBindValue(artist);
    query.addBindValue(title);
    query.addBindValue(filePath);
    query.addBindValue(duration);
    if (directoryId >= 0)
        query.addBindValue(directoryId);
    else
        query.addBindValue(QVariant());
    query.addBindValue(source);

    return query.exec();
}

int directoryIdForPath(QSqlDatabase &db, const QString &absPath,
                       const QString &directoryTable = QStringLiteral("directories"))
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT id FROM %1 WHERE path = ?").arg(directoryTable));
    query.addBindValue(absPath);
    if (query.exec() && query.next())
        return query.value(0).toInt();
    return -1;
}

// The naming pattern stored for a folder, or the default when it has none.
QString patternForDirectory(QSqlDatabase &db, int directoryId,
                            const QString &directoryTable = QStringLiteral("directories"))
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT pattern FROM %1 WHERE id = ?").arg(directoryTable));
    query.addBindValue(directoryId);
    if (query.exec() && query.next()) {
        const QString pattern = query.value(0).toString().trimmed();
        if (!pattern.isEmpty())
            return pattern;
    }
    return QStringLiteral("{Artist} - {Title}");
}

// --- Off-thread measuring ---------------------------------------------------

// ffprobe is an external program. On a machine with no ffmpeg installed there
// is nothing to measure with, and files still have to be indexed - an unknown
// duration is a cosmetic problem, an empty library is not.
bool ffprobeAvailable()
{
    static const bool available =
        !QStandardPaths::findExecutable(QStringLiteral("ffprobe")).isEmpty();
    return available;
}

// Runs on a QtConcurrent worker thread, one file per job. Files that already
// have a length are passed straight through, so a rescan only pays for what is
// genuinely new. A file ffprobe cannot read comes back with duration 0.
ScannedFile measureFile(ScannedFile file)
{
    if (file.duration > 0 || !ffprobeAvailable())
        return file;

    const int probed = probeDurationSeconds(file.filePath);
    file.duration = probed > 0 ? probed : 0;
    return file;
}

// Karaoke scan results as rows: the .cdg half of a pair is dropped, as is a
// second file sharing a base name. Lengths already stored are carried over.
QVector<ScannedFile> karaokeFilesFrom(const QVector<ParsedSong> &found,
                                      const QHash<QString, int> &known)
{
    QVector<ScannedFile> files;
    QSet<QString> seenBasePaths;

    for (const ParsedSong &parsed : found) {
        const QFileInfo info(parsed.filePath);
        const QString extension = info.suffix().toLower();

        if (extension == QLatin1String("cdg") && parsed.isCdgPair)
            continue;

        const QString basePath = info.absolutePath() + QLatin1Char('/') + info.completeBaseName();
        if (seenBasePaths.contains(basePath))
            continue;
        seenBasePaths.insert(basePath);

        files.append(ScannedFile{ parsed.artist, parsed.title, parsed.filePath, parsed.source,
                                  known.value(parsed.filePath, 0) });
    }

    return files;
}

// Background scan results as rows. The scanner has already applied the audio
// extension list and left out the karaoke handling entirely.
QVector<ScannedFile> audioFilesFrom(const QVector<ParsedSong> &found,
                                    const QHash<QString, int> &known)
{
    QVector<ScannedFile> files;
    files.reserve(found.size());

    for (const ParsedSong &parsed : found) {
        files.append(ScannedFile{ parsed.artist, parsed.title, parsed.filePath, parsed.source,
                                  known.value(parsed.filePath, 0) });
    }

    return files;
}

} // namespace

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent), m_songCount(0)
{
    initializeDatabase();
}

DatabaseManager::~DatabaseManager()
{
    // The measuring jobs themselves touch neither the database nor this object,
    // but the watcher does, so let a run finish before tearing anything down.
    if (m_scanWatcher)
        m_scanWatcher->waitForFinished();

    if (m_db.isOpen())
        m_db.close();
}

bool DatabaseManager::initializeDatabase()
{
    m_dataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(m_dataLocation);
    if (!dir.exists())
        dir.mkpath(".");

    if (QSqlDatabase::contains(QStringLiteral("MaintecKJ")))
        m_db = QSqlDatabase::database(QStringLiteral("MaintecKJ"));
    else
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("MaintecKJ"));

    m_db.setDatabaseName(m_dataLocation + QStringLiteral("/songdatabase.db"));

    if (!m_db.open()) {
        qDebug() << "Database error:" << m_db.lastError().text();
        return false;
    }

    QSqlQuery query(m_db);

    // Create songs table
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS songs ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "artist TEXT, "
            "title TEXT, "
            "file_path TEXT UNIQUE, "
            "duration INTEGER)"))) {
        qDebug() << "Error creating songs table:" << query.lastError().text();
        return false;
    }

    // Create directories table (root folders tracked for rescan/remove)
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS directories ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "path TEXT UNIQUE, "
            "pattern TEXT)"))) {
        qDebug() << "Error creating directories table:" << query.lastError().text();
        return false;
    }

    // Migrate: the naming pattern each karaoke folder is indexed with.
    bool hasDirectoryPattern = false;
    if (query.exec(QStringLiteral("PRAGMA table_info(directories)"))) {
        while (query.next()) {
            if (query.value(1).toString() == QLatin1String("pattern")) {
                hasDirectoryPattern = true;
                break;
            }
        }
    }
    if (!hasDirectoryPattern) {
        if (!query.exec(QStringLiteral("ALTER TABLE directories ADD COLUMN pattern TEXT")))
            qDebug() << "Error adding directories.pattern column:" << query.lastError().text();
    }

    // Create singers table (persisted rotation list)
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS singers ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT, "
            "status TEXT, "
            "position INTEGER)"))) {
        qDebug() << "Error creating singers table:" << query.lastError().text();
        return false;
    }

    // Create queue table (persisted song queue / history)
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS queue ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "singer TEXT, "
            "title TEXT, "
            "artist TEXT, "
            "file_path TEXT, "
            "duration INTEGER, "
            "is_played INTEGER DEFAULT 0, "
            "position INTEGER, "
            "source TEXT, "
            "key_shift INTEGER DEFAULT 0)"))) {
        qDebug() << "Error creating queue table:" << query.lastError().text();
        return false;
    }

    // Background music (Phase 7) lives in its own tables rather than behind a
    // flag on `songs`, because the two libraries are entirely different folders.
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS background_songs ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "artist TEXT, "
            "title TEXT, "
            "file_path TEXT UNIQUE, "
            "duration INTEGER, "
            "directory_id INTEGER, "
            "is_deleted INTEGER DEFAULT 0, "
            "source TEXT)"))) {
        qDebug() << "Error creating background_songs table:" << query.lastError().text();
        return false;
    }

    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS background_directories ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "path TEXT UNIQUE)"))) {
        qDebug() << "Error creating background_directories table:" << query.lastError().text();
        return false;
    }

    // The background playlist belongs to the app, not to a singer, so it has no
    // singer column and never reaches the rotation.
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS background_playlist ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "title TEXT, "
            "artist TEXT, "
            "file_path TEXT, "
            "duration INTEGER, "
            "source TEXT, "
            "position INTEGER)"))) {
        qDebug() << "Error creating background_playlist table:" << query.lastError().text();
        return false;
    }

    // Requests claimed from the singer portal (pull model). The portal's request
    // uuid is unique, so re-inserting the same claim is a no-op.
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS web_requests ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "portal_request_id TEXT UNIQUE, "
            "singer_name TEXT, "
            "stage_name TEXT, "
            "artist TEXT, "
            "title TEXT, "
            "note TEXT, "
            "requested_at TEXT, "
            "received_at TEXT, "
            "status TEXT DEFAULT 'pending', "
            "resolved_file_path TEXT, "
            "resolved_source TEXT, "
            "resolved_duration INTEGER)"))) {
        qDebug() << "Error creating web_requests table:" << query.lastError().text();
        return false;
    }

    // Migrate: associate each song with the root directory it came from.
    bool hasDirectoryId = false;
    if (query.exec(QStringLiteral("PRAGMA table_info(songs)"))) {
        while (query.next()) {
            if (query.value(1).toString() == QLatin1String("directory_id")) {
                hasDirectoryId = true;
                break;
            }
        }
    }
    if (!hasDirectoryId) {
        if (!query.exec(QStringLiteral("ALTER TABLE songs ADD COLUMN directory_id INTEGER")))
            qDebug() << "Error adding directory_id column:" << query.lastError().text();
    }

    // Migrate: soft-delete flag so removed songs are not resurrected on rescan.
    bool hasIsDeleted = false;
    if (query.exec(QStringLiteral("PRAGMA table_info(songs)"))) {
        while (query.next()) {
            if (query.value(1).toString() == QLatin1String("is_deleted")) {
                hasIsDeleted = true;
                break;
            }
        }
    }
    if (!hasIsDeleted) {
        if (!query.exec(QStringLiteral("ALTER TABLE songs ADD COLUMN is_deleted INTEGER DEFAULT 0")))
            qDebug() << "Error adding is_deleted column:" << query.lastError().text();
    }

    // Migrate: karaoke source/disc tag (e.g. "Zoom", "#Z Karaoke").
    bool hasSource = false;
    if (query.exec(QStringLiteral("PRAGMA table_info(songs)"))) {
        while (query.next()) {
            if (query.value(1).toString() == QLatin1String("source")) {
                hasSource = true;
                break;
            }
        }
    }
    if (!hasSource) {
        if (!query.exec(QStringLiteral("ALTER TABLE songs ADD COLUMN source TEXT")))
            qDebug() << "Error adding source column:" << query.lastError().text();
    }

    // Migrate: queue source column.
    bool hasQueueSource = false;
    if (query.exec(QStringLiteral("PRAGMA table_info(queue)"))) {
        while (query.next()) {
            if (query.value(1).toString() == QLatin1String("source")) {
                hasQueueSource = true;
                break;
            }
        }
    }
    if (!hasQueueSource) {
        if (!query.exec(QStringLiteral("ALTER TABLE queue ADD COLUMN source TEXT")))
            qDebug() << "Error adding queue source column:" << query.lastError().text();
    }

    // Migrate: queue key_shift column (per-song transposition, in semitones).
    bool hasQueueKeyShift = false;
    if (query.exec(QStringLiteral("PRAGMA table_info(queue)"))) {
        while (query.next()) {
            if (query.value(1).toString() == QLatin1String("key_shift")) {
                hasQueueKeyShift = true;
                break;
            }
        }
    }
    if (!hasQueueKeyShift) {
        if (!query.exec(QStringLiteral("ALTER TABLE queue ADD COLUMN key_shift INTEGER DEFAULT 0")))
            qDebug() << "Error adding queue key_shift column:" << query.lastError().text();
    }

    // Migrate: link a queue row back to the portal request it came from, so
    // played/unplayed changes can be synced both ways.
    bool hasPortalRequestId = false;
    if (query.exec(QStringLiteral("PRAGMA table_info(queue)"))) {
        while (query.next()) {
            if (query.value(1).toString() == QLatin1String("portal_request_id")) {
                hasPortalRequestId = true;
                break;
            }
        }
    }
    if (!hasPortalRequestId) {
        if (!query.exec(QStringLiteral("ALTER TABLE queue ADD COLUMN portal_request_id TEXT")))
            qDebug() << "Error adding queue portal_request_id column:" << query.lastError().text();
    }

    ensureIndexes();

    m_songCount = countSongs(m_db);
    m_backgroundSongCount = countSongs(m_db, QStringLiteral("background_songs"));

    emit dataLocationChanged();
    emit songCountChanged();
    emit backgroundSongCountChanged();

    qDebug() << "Database initialized at:" << m_dataLocation << "songs:" << m_songCount
             << "background songs:" << m_backgroundSongCount;
    return true;
}

void DatabaseManager::ensureIndexes()
{
    QSqlQuery query(m_db);
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_artist ON songs(artist)"));
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_title ON songs(title)"));
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_file_path ON songs(file_path)"));
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_directory_id ON songs(directory_id)"));
}

void DatabaseManager::startScan(QVector<ScannedFile> files, const QString &songTable,
                                int directoryId, bool background)
{
    if (m_scanWatcher) {
        // Re-indexing while a run is in flight would interleave two result sets.
        // The panels disable their buttons while `scanning` is true.
        qDebug() << "A scan is already running; ignoring the new request.";
        return;
    }

    m_scanSongTable = songTable;
    m_scanDirectoryId = directoryId;
    m_scanIsBackground = background;
    m_scanProgress = 0;
    m_scanTotal = static_cast<int>(files.size());

    m_scanWatcher = new QFutureWatcher<ScannedFile>(this);

    connect(m_scanWatcher, &QFutureWatcher<ScannedFile>::finished,
            this, &DatabaseManager::finishScan);
    connect(m_scanWatcher, &QFutureWatcher<ScannedFile>::progressRangeChanged, this,
            [this](int, int) { emit scanProgressChanged(); });
    connect(m_scanWatcher, &QFutureWatcher<ScannedFile>::progressValueChanged, this,
            [this](int value) {
                m_scanProgress = value;
                emit scanProgressChanged();
            });

    m_scanWatcher->setFuture(QtConcurrent::mapped(files, &measureFile));

    emit scanProgressChanged();
}

void DatabaseManager::finishScan()
{
    if (!m_scanWatcher)
        return;

    const QList<ScannedFile> measured = m_scanWatcher->future().results();

    m_scanWatcher->deleteLater();
    m_scanWatcher = nullptr;

    int added = 0;
    int unreadable = 0;

    // One transaction for the whole run. Without it each row is its own
    // auto-committed INSERT, which costs an fsync apiece.
    m_db.transaction();
    for (const ScannedFile &file : measured) {
        // With ffprobe present it doubles as the format gate: the wide extension
        // list decides what to look at, and a file it cannot decode is left out.
        // Without ffprobe nothing can be judged, so the row is kept with an
        // unknown length rather than the whole library coming up empty.
        if (file.duration <= 0 && ffprobeAvailable()) {
            ++unreadable;
            continue;
        }

        if (insertSongRow(m_db, file.artist, file.title, file.filePath, file.duration,
                          m_scanDirectoryId, file.source, m_scanSongTable)) {
            ++added;
        }
    }
    m_db.commit();

    const bool wasBackground = m_scanIsBackground;
    m_scanProgress = m_scanTotal;

    if (wasBackground) {
        m_backgroundSongCount = countSongs(m_db, QStringLiteral("background_songs"));
        emit backgroundSongCountChanged();
    } else {
        m_songCount = countSongs(m_db);
        emit songCountChanged();
    }

    // Reported after the watcher is gone, so `scanning` already reads false.
    emit scanProgressChanged();

    if (wasBackground)
        emit backgroundScanFinished(added, unreadable);
    else
        emit libraryScanFinished(added, unreadable);

    qDebug() << "Scan finished for" << m_scanSongTable << "added:" << added
             << "unreadable:" << unreadable;
}

bool DatabaseManager::addSong(const QString &artist, const QString &title, const QString &filePath, int duration, const QString &source)
{
    if (songExists(filePath))
        return false;

    if (!insertSongRow(m_db, artist, title, filePath, duration, -1, source)) {
        qDebug() << "Error adding song:" << m_db.lastError().text();
        return false;
    }

    m_songCount = countSongs(m_db);
    emit songCountChanged();
    return true;
}

bool DatabaseManager::removeSong(int id)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE songs SET is_deleted = 1 WHERE id = ?"));
    query.addBindValue(id);

    if (!query.exec()) {
        qDebug() << "Error removing song:" << query.lastError().text();
        return false;
    }

    m_songCount = countSongs(m_db);
    emit songCountChanged();
    return true;
}

bool DatabaseManager::restoreSong(int id)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE songs SET is_deleted = 0 WHERE id = ?"));
    query.addBindValue(id);

    if (!query.exec()) {
        qDebug() << "Error restoring song:" << query.lastError().text();
        return false;
    }

    m_songCount = countSongs(m_db);
    emit songCountChanged();
    return true;
}

bool DatabaseManager::purgeSong(int id)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM songs WHERE id = ?"));
    query.addBindValue(id);

    if (!query.exec()) {
        qDebug() << "Error purging song:" << query.lastError().text();
        return false;
    }

    m_songCount = countSongs(m_db);
    emit songCountChanged();
    return true;
}

bool DatabaseManager::updateSong(int id, const QString &artist, const QString &title, const QString &filePath, int duration, const QString &source)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE songs SET artist = ?, title = ?, file_path = ?, duration = ?, source = ? WHERE id = ?"));
    query.addBindValue(artist);
    query.addBindValue(title);
    query.addBindValue(filePath);
    query.addBindValue(duration);
    query.addBindValue(source);
    query.addBindValue(id);

    if (!query.exec()) {
        qDebug() << "Error updating song:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::songExists(const QString &filePath)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT id FROM songs WHERE file_path = ?"));
    query.addBindValue(filePath);

    return query.exec() && query.next();
}

QVariantList DatabaseManager::searchSongs(const QString &query, bool includeDeleted)
{
    if (query.trimmed().isEmpty()) {
        return getAllSongs(includeDeleted);
    }

    QSqlQuery queryObj(m_db);
    QVariantList results;

    // Use LIKE for partial matching on artist and title
    const QString searchTerm = QStringLiteral("%") + query + QStringLiteral("%");
    QString sql = QStringLiteral(
        "SELECT id, artist, title, file_path, duration, is_deleted, source FROM songs "
        "WHERE (artist LIKE ? OR title LIKE ? OR source LIKE ?)");
    if (!includeDeleted)
        sql += QStringLiteral(" AND is_deleted = 0");
    sql += QStringLiteral(" ORDER BY artist, title");

    queryObj.prepare(sql);
    queryObj.addBindValue(searchTerm);
    queryObj.addBindValue(searchTerm);
    queryObj.addBindValue(searchTerm);

    if (queryObj.exec()) {
        while (queryObj.next()) {
            QVariantMap song;
            song[QStringLiteral("id")] = queryObj.value(0).toInt();
            song[QStringLiteral("artist")] = queryObj.value(1).toString();
            song[QStringLiteral("title")] = queryObj.value(2).toString();
            song[QStringLiteral("filePath")] = queryObj.value(3).toString();
            song[QStringLiteral("duration")] = queryObj.value(4).toInt();
            song[QStringLiteral("isDeleted")] = queryObj.value(5).toInt() != 0;
            song[QStringLiteral("source")] = queryObj.value(6).toString();
            results.append(song);
        }
    } else {
        qDebug() << "Search query error:" << queryObj.lastError().text();
    }

    return results;
}

QVariantList DatabaseManager::getAllSongs(bool includeDeleted)
{
    QSqlQuery query(m_db);
    if (includeDeleted) {
        query.prepare(QStringLiteral(
            "SELECT id, artist, title, file_path, duration, is_deleted, source FROM songs ORDER BY artist, title"));
    } else {
        query.prepare(QStringLiteral(
            "SELECT id, artist, title, file_path, duration, is_deleted, source FROM songs "
            "WHERE is_deleted = 0 ORDER BY artist, title"));
    }
    return executeQuery(query);
}

Song DatabaseManager::getSongById(int id)
{
    Song song;
    song.id = -1;
    song.duration = 0;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT id, artist, title, file_path, duration, source FROM songs WHERE id = ?"));
    query.addBindValue(id);

    if (query.exec() && query.next()) {
        song.id = query.value(0).toInt();
        song.artist = query.value(1).toString();
        song.title = query.value(2).toString();
        song.filePath = query.value(3).toString();
        song.duration = query.value(4).toInt();
        song.source = query.value(5).toString();
    }

    return song;
}

QVariantList DatabaseManager::getSongsByArtist(const QString &artist)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT id, artist, title, file_path, duration, is_deleted, source FROM songs WHERE artist = ? AND is_deleted = 0 ORDER BY title"));
    query.addBindValue(artist);
    return executeQuery(query);
}

QVariantList DatabaseManager::getSongsByTitle(const QString &title)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT id, artist, title, file_path, duration, is_deleted, source FROM songs WHERE title = ? AND is_deleted = 0 ORDER BY artist"));
    query.addBindValue(title);
    return executeQuery(query);
}

QVariantList DatabaseManager::findSongsByArtistTitle(const QString &artist, const QString &title)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, artist, title, file_path, duration, is_deleted, source FROM songs "
        "WHERE artist = ? COLLATE NOCASE AND title = ? COLLATE NOCASE AND is_deleted = 0 "
        "ORDER BY source, file_path"));
    query.addBindValue(artist);
    query.addBindValue(title);
    return executeQuery(query);
}

bool DatabaseManager::insertWebRequest(const QVariantMap &request)
{
    return executePreparedQuery(
        QStringLiteral("INSERT OR IGNORE INTO web_requests "
                       "(portal_request_id, singer_name, stage_name, artist, title, note, "
                       " requested_at, received_at, status) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'pending')"),
        { request.value(QStringLiteral("portalRequestId")).toString(),
          request.value(QStringLiteral("singerName")).toString(),
          request.value(QStringLiteral("stageName")).toString(),
          request.value(QStringLiteral("artist")).toString(),
          request.value(QStringLiteral("title")).toString(),
          request.value(QStringLiteral("note")).toString(),
          request.value(QStringLiteral("requestedAt")).toString(),
          QDateTime::currentDateTime().toString(Qt::ISODate) });
}

QVariantList DatabaseManager::loadWebRequests()
{
    QVariantList results;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT portal_request_id, singer_name, stage_name, artist, title, note, requested_at "
        "FROM web_requests WHERE status = 'pending' ORDER BY received_at ASC"));

    if (query.exec()) {
        while (query.next()) {
            QVariantMap row;
            row[QStringLiteral("portalRequestId")] = query.value(0).toString();
            row[QStringLiteral("singerName")] = query.value(1).toString();
            row[QStringLiteral("stageName")] = query.value(2).toString();
            row[QStringLiteral("artist")] = query.value(3).toString();
            row[QStringLiteral("title")] = query.value(4).toString();
            row[QStringLiteral("note")] = query.value(5).toString();
            row[QStringLiteral("requestedAt")] = query.value(6).toString();
            results.append(row);
        }
    } else {
        qDebug() << "Query error:" << query.lastError().text();
    }

    return results;
}

bool DatabaseManager::deleteWebRequest(const QString &portalRequestId)
{
    return executePreparedQuery(
        QStringLiteral("DELETE FROM web_requests WHERE portal_request_id = ?"),
        { portalRequestId });
}

bool DatabaseManager::addDirectory(const QString &directoryPath, const QString &pattern)
{
    QDir dir(directoryPath);
    if (!dir.exists())
        return false;

    const QString absPath = QDir::cleanPath(dir.absolutePath());
    const QString effectivePattern = pattern.trimmed().isEmpty()
                                         ? QStringLiteral("{Artist} - {Title}")
                                         : pattern.trimmed();

    QSqlQuery insertDir(m_db);
    insertDir.prepare(QStringLiteral("INSERT OR IGNORE INTO directories (path, pattern) VALUES (?, ?)"));
    insertDir.addBindValue(absPath);
    insertDir.addBindValue(effectivePattern);
    if (!insertDir.exec()) {
        qDebug() << "Error adding directory:" << insertDir.lastError().text();
        return false;
    }

    // Re-adding a folder under a different pattern should take effect, so the
    // stored pattern is overwritten rather than left as it was.
    QSqlQuery updatePattern(m_db);
    updatePattern.prepare(QStringLiteral("UPDATE directories SET pattern = ? WHERE path = ?"));
    updatePattern.addBindValue(effectivePattern);
    updatePattern.addBindValue(absPath);
    updatePattern.exec();

    const int dirId = directoryIdForPath(m_db, absPath);
    if (dirId < 0) {
        qDebug() << "Could not resolve directory id for:" << absPath;
        return false;
    }

    const QHash<QString, int> knownDurations = knownDurationsFor(m_db, dirId);

    FolderScanner scanner;
    const QVector<ParsedSong> found = scanner.scanDirectory(absPath, effectivePattern);

    // The measuring is handed off; the rows land when it reports back.
    startScan(karaokeFilesFrom(found, knownDurations), QStringLiteral("songs"), dirId, false);

    qDebug() << "Indexing directory:" << absPath << "pattern:" << effectivePattern
             << "candidates:" << found.size();
    return true;
}

bool DatabaseManager::removeDirectory(const QString &directoryPath)
{
    const QString absPath = QDir::cleanPath(QDir(directoryPath).absolutePath());
    const int dirId = directoryIdForPath(m_db, absPath);
    if (dirId < 0)
        return false;

    QSqlQuery deleteSongs(m_db);
    deleteSongs.prepare(QStringLiteral("DELETE FROM songs WHERE directory_id = ?"));
    deleteSongs.addBindValue(dirId);
    if (!deleteSongs.exec()) {
        qDebug() << "Error removing songs of directory:" << deleteSongs.lastError().text();
        return false;
    }

    QSqlQuery deleteDir(m_db);
    deleteDir.prepare(QStringLiteral("DELETE FROM directories WHERE id = ?"));
    deleteDir.addBindValue(dirId);
    if (!deleteDir.exec()) {
        qDebug() << "Error removing directory:" << deleteDir.lastError().text();
        return false;
    }

    m_songCount = countSongs(m_db);
    emit songCountChanged();

    qDebug() << "Removed directory:" << absPath;
    return true;
}

void DatabaseManager::rescanDirectory(const QString &directoryPath)
{
    const QString absPath = QDir::cleanPath(QDir(directoryPath).absolutePath());
    const int dirId = directoryIdForPath(m_db, absPath);
    if (dirId < 0)
        return;

    // The pattern the folder was added with is what it is re-indexed with.
    const QString pattern = patternForDirectory(m_db, dirId);

    // Remember what we already know before those rows are replaced, so the
    // rescan only has to measure genuinely new files.
    const QHash<QString, int> knownDurations = knownDurationsFor(m_db, dirId);

    // Drop stale records belonging to this root, then re-index from disk.
    QSqlQuery deleteSongs(m_db);
    deleteSongs.prepare(QStringLiteral("DELETE FROM songs WHERE directory_id = ? AND is_deleted = 0"));
    deleteSongs.addBindValue(dirId);
    if (!deleteSongs.exec()) {
        qDebug() << "Error clearing directory during rescan:" << deleteSongs.lastError().text();
        return;
    }

    FolderScanner scanner;
    const QVector<ParsedSong> found = scanner.scanDirectory(absPath, pattern);

    startScan(karaokeFilesFrom(found, knownDurations), QStringLiteral("songs"), dirId, false);

    qDebug() << "Rescanning directory:" << absPath << "pattern:" << pattern
             << "candidates:" << found.size();
}

bool DatabaseManager::setDirectoryPattern(const QString &directoryPath, const QString &pattern)
{
    const QString absPath = QDir::cleanPath(QDir(directoryPath).absolutePath());
    const int dirId = directoryIdForPath(m_db, absPath);
    if (dirId < 0)
        return false;

    const QString effectivePattern = pattern.trimmed().isEmpty()
                                         ? QStringLiteral("{Artist} - {Title}")
                                         : pattern.trimmed();

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE directories SET pattern = ? WHERE id = ?"));
    query.addBindValue(effectivePattern);
    query.addBindValue(dirId);
    if (!query.exec()) {
        qDebug() << "Error setting directory pattern:" << query.lastError().text();
        return false;
    }

    // Re-index straight away so the new split is visible without a second click.
    rescanDirectory(absPath);
    return true;
}

// --- Background music (Phase 7) --------------------------------------------
// Mirrors the karaoke scan, but against its own tables, with the audio-only
// extension list and no .cdg pairing: ffprobe decides whether a file is really
// readable, and anything it rejects is left out of the library entirely.

bool DatabaseManager::addBackgroundDirectory(const QString &directoryPath)
{
    QDir dir(directoryPath);
    if (!dir.exists())
        return false;

    const QString absPath = QDir::cleanPath(dir.absolutePath());

    QSqlQuery insertDir(m_db);
    insertDir.prepare(QStringLiteral("INSERT OR IGNORE INTO background_directories (path) VALUES (?)"));
    insertDir.addBindValue(absPath);
    if (!insertDir.exec()) {
        qDebug() << "Error adding background directory:" << insertDir.lastError().text();
        return false;
    }

    const int dirId = directoryIdForPath(m_db, absPath, QStringLiteral("background_directories"));
    if (dirId < 0) {
        qDebug() << "Could not resolve background directory id for:" << absPath;
        return false;
    }

    const QHash<QString, int> knownDurations =
        knownDurationsFor(m_db, dirId, QStringLiteral("background_songs"));

    FolderScanner scanner;
    const QVector<ParsedSong> found = scanner.scanAudioDirectory(absPath);

    // Measuring runs off the UI thread; the rows land when it reports back.
    startScan(audioFilesFrom(found, knownDurations), QStringLiteral("background_songs"),
              dirId, true);

    qDebug() << "Indexing background directory:" << absPath << "candidates:" << found.size();
    return true;
}

bool DatabaseManager::removeBackgroundDirectory(const QString &directoryPath)
{
    const QString absPath = QDir::cleanPath(QDir(directoryPath).absolutePath());
    const int dirId = directoryIdForPath(m_db, absPath, QStringLiteral("background_directories"));
    if (dirId < 0)
        return false;

    QSqlQuery deleteSongs(m_db);
    deleteSongs.prepare(QStringLiteral("DELETE FROM background_songs WHERE directory_id = ?"));
    deleteSongs.addBindValue(dirId);
    if (!deleteSongs.exec()) {
        qDebug() << "Error removing songs of background directory:" << deleteSongs.lastError().text();
        return false;
    }

    QSqlQuery deleteDir(m_db);
    deleteDir.prepare(QStringLiteral("DELETE FROM background_directories WHERE id = ?"));
    deleteDir.addBindValue(dirId);
    if (!deleteDir.exec()) {
        qDebug() << "Error removing background directory:" << deleteDir.lastError().text();
        return false;
    }

    m_backgroundSongCount = countSongs(m_db, QStringLiteral("background_songs"));
    emit backgroundSongCountChanged();

    qDebug() << "Removed background directory:" << absPath;
    return true;
}

void DatabaseManager::rescanBackgroundDirectory(const QString &directoryPath)
{
    const QString absPath = QDir::cleanPath(QDir(directoryPath).absolutePath());
    const int dirId = directoryIdForPath(m_db, absPath, QStringLiteral("background_directories"));
    if (dirId < 0) {
        addBackgroundDirectory(absPath);
        return;
    }

    const QHash<QString, int> knownDurations =
        knownDurationsFor(m_db, dirId, QStringLiteral("background_songs"));

    FolderScanner scanner;
    const QVector<ParsedSong> found = scanner.scanAudioDirectory(absPath);

    startScan(audioFilesFrom(found, knownDurations), QStringLiteral("background_songs"),
              dirId, true);

    qDebug() << "Rescanning background directory:" << absPath << "candidates:" << found.size();
}

QVariantList DatabaseManager::getAllBackgroundSongs(bool includeDeleted)
{
    QVariantList results;

    QSqlQuery query(m_db);
    QString sql = QStringLiteral(
        "SELECT id, artist, title, file_path, duration, is_deleted, source "
        "FROM background_songs");
    if (!includeDeleted)
        sql += QStringLiteral(" WHERE is_deleted = 0");
    sql += QStringLiteral(" ORDER BY artist, title");

    if (!query.exec(sql))
        return results;

    while (query.next()) {
        QVariantMap song;
        song[QStringLiteral("id")] = query.value(0).toInt();
        song[QStringLiteral("artist")] = query.value(1).toString();
        song[QStringLiteral("title")] = query.value(2).toString();
        song[QStringLiteral("filePath")] = query.value(3).toString();
        song[QStringLiteral("duration")] = query.value(4).toInt();
        song[QStringLiteral("isDeleted")] = query.value(5).toInt() != 0;
        song[QStringLiteral("source")] = query.value(6).toString();
        results.append(song);
    }

    return results;
}

bool DatabaseManager::backgroundSongExists(const QString &filePath)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT 1 FROM background_songs WHERE file_path = ? LIMIT 1"));
    query.addBindValue(filePath);
    return query.exec() && query.next();
}

QVariantList DatabaseManager::loadBackgroundPlaylist()
{
    QVariantList items;

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral(
            "SELECT title, artist, file_path, duration, source FROM background_playlist "
            "ORDER BY position"))) {
        return items;
    }

    while (query.next()) {
        QVariantMap item;
        item[QStringLiteral("title")] = query.value(0).toString();
        item[QStringLiteral("artist")] = query.value(1).toString();
        item[QStringLiteral("filePath")] = query.value(2).toString();
        item[QStringLiteral("duration")] = query.value(3).toInt();
        item[QStringLiteral("source")] = query.value(4).toString();
        items.append(item);
    }

    return items;
}

void DatabaseManager::saveBackgroundPlaylist(const QVariantList &items)
{
    m_db.transaction();

    QSqlQuery clear(m_db);
    clear.exec(QStringLiteral("DELETE FROM background_playlist"));

    int position = 0;
    for (const QVariant &entry : items) {
        const QVariantMap item = entry.toMap();

        QSqlQuery insert(m_db);
        insert.prepare(QStringLiteral(
            "INSERT INTO background_playlist (title, artist, file_path, duration, source, position) "
            "VALUES (?, ?, ?, ?, ?, ?)"));
        insert.addBindValue(item.value(QStringLiteral("title")).toString());
        insert.addBindValue(item.value(QStringLiteral("artist")).toString());
        insert.addBindValue(item.value(QStringLiteral("filePath")).toString());
        insert.addBindValue(item.value(QStringLiteral("duration")).toInt());
        insert.addBindValue(item.value(QStringLiteral("source")).toString());
        insert.addBindValue(position++);
        insert.exec();
    }

    m_db.commit();
}

bool DatabaseManager::addFileToDatabase(const QString &filePath)
{
    if (songExists(filePath))
        return false;

    FolderScanner scanner;
    const ParsedSong parsed = scanner.parseFileName(QFileInfo(filePath).fileName());

    return addSong(parsed.artist, parsed.title, filePath, 0, parsed.source);
}

QString DatabaseManager::parseSongTitle(const QString &fileName)
{
    const QRegularExpression pattern(QStringLiteral("^(.*?)\\s*-\\s*(.*)$"));
    const QRegularExpressionMatch match = pattern.match(fileName);
    if (match.hasMatch())
        return match.captured(2).trimmed();
    return fileName;
}

QString DatabaseManager::parseArtistFromTitle(const QString &title)
{
    const QRegularExpression pattern(QStringLiteral("^(.*?)\\s*-\\s*(.*)$"));
    const QRegularExpressionMatch match = pattern.match(title);
    if (match.hasMatch())
        return match.captured(1).trimmed();
    return QStringLiteral("Unknown Artist");
}

QString DatabaseManager::parseTitleFromArtist(const QString &artist)
{
    const QRegularExpression pattern(QStringLiteral("^(.*?)\\s*-\\s*(.*)$"));
    const QRegularExpressionMatch match = pattern.match(artist);
    if (match.hasMatch())
        return match.captured(2).trimmed();
    return artist;
}

QVariantList DatabaseManager::loadSingers()
{
    QVariantList results;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT id, name, status FROM singers ORDER BY position"));
    if (query.exec()) {
        while (query.next()) {
            QVariantMap singer;
            singer[QStringLiteral("id")] = query.value(0).toInt();
            singer[QStringLiteral("name")] = query.value(1).toString();
            singer[QStringLiteral("status")] = query.value(2).toString();
            results.append(singer);
        }
    }
    return results;
}

void DatabaseManager::saveSingers(const QVariantList &singers)
{
    m_db.transaction();

    QSqlQuery clear(m_db);
    clear.exec(QStringLiteral("DELETE FROM singers"));

    int position = 0;
    for (const QVariant &entry : singers) {
        const QVariantMap singer = entry.toMap();
        QSqlQuery insert(m_db);
        insert.prepare(QStringLiteral("INSERT INTO singers (name, status, position) VALUES (?, ?, ?)"));
        insert.addBindValue(singer.value(QStringLiteral("name")).toString());
        insert.addBindValue(singer.value(QStringLiteral("status")).toString());
        insert.addBindValue(position++);
        insert.exec();
    }

    m_db.commit();
}

QVariantList DatabaseManager::loadQueue()
{
    QVariantList results;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT singer, title, artist, file_path, duration, is_played, source, key_shift, portal_request_id FROM queue ORDER BY position"));
    if (query.exec()) {
        while (query.next()) {
            QVariantMap item;
            item[QStringLiteral("singer")] = query.value(0).toString();
            item[QStringLiteral("title")] = query.value(1).toString();
            item[QStringLiteral("artist")] = query.value(2).toString();
            item[QStringLiteral("filePath")] = query.value(3).toString();
            item[QStringLiteral("duration")] = query.value(4).toInt();
            item[QStringLiteral("isPlayed")] = query.value(5).toInt() != 0;
            item[QStringLiteral("source")] = query.value(6).toString();
            item[QStringLiteral("keyShift")] = query.value(7).toInt();
            item[QStringLiteral("portalRequestId")] = query.value(8).toString();
            results.append(item);
        }
    }
    return results;
}

void DatabaseManager::saveQueue(const QVariantList &items)
{
    m_db.transaction();

    QSqlQuery clear(m_db);
    clear.exec(QStringLiteral("DELETE FROM queue"));

    int position = 0;
    for (const QVariant &entry : items) {
        const QVariantMap item = entry.toMap();
        QSqlQuery insert(m_db);
        insert.prepare(QStringLiteral(
            "INSERT INTO queue (singer, title, artist, file_path, duration, is_played, source, key_shift, portal_request_id, position) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
        insert.addBindValue(item.value(QStringLiteral("singer")).toString());
        insert.addBindValue(item.value(QStringLiteral("title")).toString());
        insert.addBindValue(item.value(QStringLiteral("artist")).toString());
        insert.addBindValue(item.value(QStringLiteral("filePath")).toString());
        insert.addBindValue(item.value(QStringLiteral("duration")).toInt());
        insert.addBindValue(item.value(QStringLiteral("isPlayed")).toBool() ? 1 : 0);
        insert.addBindValue(item.value(QStringLiteral("source")).toString());
        insert.addBindValue(item.value(QStringLiteral("keyShift")).toInt());
        insert.addBindValue(item.value(QStringLiteral("portalRequestId")).toString());
        insert.addBindValue(position++);
        insert.exec();
    }

    m_db.commit();
}

bool DatabaseManager::executePreparedQuery(const QString &query, const QVariantList &bindings)
{
    QSqlQuery prepared(m_db);
    if (!prepared.prepare(query)) {
        qDebug() << "Prepare error:" << prepared.lastError().text();
        return false;
    }

    for (const QVariant &binding : bindings)
        prepared.addBindValue(binding);

    if (!prepared.exec()) {
        qDebug() << "Query error:" << prepared.lastError().text();
        return false;
    }

    return true;
}

QVariantList DatabaseManager::executeQuery(QSqlQuery &query)
{
    QVariantList results;

    if (query.exec()) {
        while (query.next()) {
            QVariantMap song;
            song[QStringLiteral("id")] = query.value(0).toInt();
            song[QStringLiteral("artist")] = query.value(1).toString();
            song[QStringLiteral("title")] = query.value(2).toString();
            song[QStringLiteral("filePath")] = query.value(3).toString();
            song[QStringLiteral("duration")] = query.value(4).toInt();
            song[QStringLiteral("isDeleted")] = query.value(5).toInt() != 0;
            song[QStringLiteral("source")] = query.value(6).toString();
            results.append(song);
        }
    } else {
        qDebug() << "Query error:" << query.lastError().text();
    }

    return results;
}
