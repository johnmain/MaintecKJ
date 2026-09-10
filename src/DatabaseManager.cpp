#include "DatabaseManager.h"
#include "FolderScanner.h"
#include <QDebug>
#include <QRegularExpression>
#include <QFileInfo>
#include <QSet>
#include <QHash>
#include <QProcess>

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
QHash<QString, int> knownDurationsFor(QSqlDatabase &db, int directoryId)
{
    QHash<QString, int> durations;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT file_path, duration FROM songs WHERE directory_id = ?"));
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

// Only files we have never measured are handed to ffprobe, so a rescan that
// finds no new songs probes nothing.
int durationSecondsFor(const QHash<QString, int> &known, const QString &filePath)
{
    const int knownSeconds = known.value(filePath, 0);
    if (knownSeconds > 0)
        return knownSeconds;

    const int probed = probeDurationSeconds(filePath);
    return probed > 0 ? probed : 0;
}

int countSongs(QSqlDatabase &db)
{
    QSqlQuery query(db);
    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM songs WHERE is_deleted = 0")) && query.next())
        return query.value(0).toInt();
    return 0;
}

bool insertSongRow(QSqlDatabase &db, const QString &artist, const QString &title,
                   const QString &filePath, int duration, int directoryId, const QString &source = QString())
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO songs (artist, title, file_path, duration, directory_id, source) "
        "VALUES (?, ?, ?, ?, ?, ?)"));
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

int directoryIdForPath(QSqlDatabase &db, const QString &absPath)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT id FROM directories WHERE path = ?"));
    query.addBindValue(absPath);
    if (query.exec() && query.next())
        return query.value(0).toInt();
    return -1;
}

} // namespace

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent), m_songCount(0)
{
    initializeDatabase();
}

DatabaseManager::~DatabaseManager()
{
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
            "path TEXT UNIQUE)"))) {
        qDebug() << "Error creating directories table:" << query.lastError().text();
        return false;
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

    ensureIndexes();

    m_songCount = countSongs(m_db);

    emit dataLocationChanged();
    emit songCountChanged();

    qDebug() << "Database initialized at:" << m_dataLocation << "songs:" << m_songCount;
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

bool DatabaseManager::addDirectory(const QString &directoryPath)
{
    QDir dir(directoryPath);
    if (!dir.exists())
        return false;

    const QString absPath = QDir::cleanPath(dir.absolutePath());

    QSqlQuery insertDir(m_db);
    insertDir.prepare(QStringLiteral("INSERT OR IGNORE INTO directories (path) VALUES (?)"));
    insertDir.addBindValue(absPath);
    if (!insertDir.exec()) {
        qDebug() << "Error adding directory:" << insertDir.lastError().text();
        return false;
    }

    const int dirId = directoryIdForPath(m_db, absPath);
    if (dirId < 0) {
        qDebug() << "Could not resolve directory id for:" << absPath;
        return false;
    }

    const QHash<QString, int> knownDurations = knownDurationsFor(m_db, dirId);

    FolderScanner scanner;
    const QVector<ParsedSong> found = scanner.scanDirectory(absPath);

    int addedCount = 0;
    QSet<QString> seenBasePaths;
    for (const ParsedSong &parsed : found) {
        const QFileInfo info(parsed.filePath);
        const QString extension = info.suffix().toLower();

        // Paired .cdg files are represented by their .mp3 companion.
        if (extension == QLatin1String("cdg") && parsed.isCdgPair)
            continue;

        const QString basePath = info.absolutePath() + QLatin1Char('/') + info.completeBaseName();
        if (seenBasePaths.contains(basePath))
            continue;
        seenBasePaths.insert(basePath);

        if (insertSongRow(m_db, parsed.artist, parsed.title, parsed.filePath,
                          durationSecondsFor(knownDurations, parsed.filePath), dirId, parsed.source))
            addedCount++;
    }

    m_songCount = countSongs(m_db);
    emit songCountChanged();

    qDebug() << "Added directory:" << absPath << "new songs:" << addedCount;
    return addedCount > 0;
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
    const QVector<ParsedSong> found = scanner.scanDirectory(absPath);

    int addedCount = 0;
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

        if (insertSongRow(m_db, parsed.artist, parsed.title, parsed.filePath,
                          durationSecondsFor(knownDurations, parsed.filePath), dirId, parsed.source))
            addedCount++;
    }

    m_songCount = countSongs(m_db);
    emit songCountChanged();

    qDebug() << "Rescanned directory:" << absPath << "songs:" << addedCount;
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
        "SELECT singer, title, artist, file_path, duration, is_played, source, key_shift FROM queue ORDER BY position"));
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
            "INSERT INTO queue (singer, title, artist, file_path, duration, is_played, source, key_shift, position) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
        insert.addBindValue(item.value(QStringLiteral("singer")).toString());
        insert.addBindValue(item.value(QStringLiteral("title")).toString());
        insert.addBindValue(item.value(QStringLiteral("artist")).toString());
        insert.addBindValue(item.value(QStringLiteral("filePath")).toString());
        insert.addBindValue(item.value(QStringLiteral("duration")).toInt());
        insert.addBindValue(item.value(QStringLiteral("isPlayed")).toBool() ? 1 : 0);
        insert.addBindValue(item.value(QStringLiteral("source")).toString());
        insert.addBindValue(item.value(QStringLiteral("keyShift")).toInt());
        insert.addBindValue(position++);
        insert.exec();
    }

    m_db.commit();
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
