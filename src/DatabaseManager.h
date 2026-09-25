#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QFutureWatcher>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QStandardPaths>
#include <QDir>

struct Song {
    int id;
    QString artist;
    QString title;
    QString filePath;
    QString source;
    int duration;
};

// A file found by a scan, with its length filled in off the UI thread. A
// duration of 0 means "not measured yet" before the scan, and "ffprobe could
// not read it" afterwards - unreadable files are dropped rather than stored.
struct ScannedFile {
    QString artist;
    QString title;
    QString filePath;
    QString source;
    int duration = 0;
};

class DatabaseManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString dataLocation READ dataLocation NOTIFY dataLocationChanged)
    Q_PROPERTY(int songCount READ songCount NOTIFY songCountChanged)
    Q_PROPERTY(int backgroundSongCount READ backgroundSongCount NOTIFY backgroundSongCountChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanProgressChanged)
    Q_PROPERTY(int scanProgress READ scanProgress NOTIFY scanProgressChanged)
    Q_PROPERTY(int scanTotal READ scanTotal NOTIFY scanProgressChanged)

public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();

    // Getters
    QString dataLocation() const { return m_dataLocation; }
    int songCount() const { return m_songCount; }
    int backgroundSongCount() const { return m_backgroundSongCount; }
    bool scanning() const { return m_scanWatcher != nullptr; }
    int scanProgress() const { return m_scanProgress; }
    int scanTotal() const { return m_scanTotal; }

    // Database operations
    bool initializeDatabase();
    bool addSong(const QString &artist, const QString &title, const QString &filePath, int duration = 0, const QString &source = QString());
    bool removeSong(int id);
    bool restoreSong(int id);
    bool purgeSong(int id);
    bool updateSong(int id, const QString &artist, const QString &title, const QString &filePath, int duration, const QString &source = QString());
    bool songExists(const QString &filePath);
    bool addFileToDatabase(const QString &filePath);

    // Query operations
    QVariantList searchSongs(const QString &query = "", bool includeDeleted = false);
    QVariantList getAllSongs(bool includeDeleted = false);
    Song getSongById(int id);
    QVariantList getSongsByArtist(const QString &artist);
    QVariantList getSongsByTitle(const QString &title);

    // Directory operations. `pattern` is the naming pattern the folder is
    // indexed with; empty means the default {Artist} - {Title}.
    Q_INVOKABLE bool addDirectory(const QString &directoryPath, const QString &pattern = QString());
    Q_INVOKABLE bool removeDirectory(const QString &directoryPath);
    Q_INVOKABLE void rescanDirectory(const QString &directoryPath);
    // Re-indexes a folder under a new naming pattern, so a mis-parsed library
    // can be corrected without removing and re-adding it.
    Q_INVOKABLE bool setDirectoryPattern(const QString &directoryPath, const QString &pattern);

    // The karaoke search directories, each with the number of live songs it
    // holds. Read-only and short enough to sit here with the other small
    // accessors; the settings panel lists them OpenKJ-style.
    Q_INVOKABLE QVariantList getDirectories() const
    {
        QVariantList result;
        QSqlQuery query(m_db);
        if (query.exec(QStringLiteral(
                "SELECT d.id, d.path, "
                "COALESCE(NULLIF(d.pattern, ''), '{Artist} - {Title}'), "
                "(SELECT COUNT(*) FROM songs s "
                " WHERE s.directory_id = d.id AND s.is_deleted = 0) "
                "FROM directories d ORDER BY d.path"))) {
            while (query.next()) {
                QVariantMap row;
                row[QStringLiteral("id")] = query.value(0).toInt();
                row[QStringLiteral("path")] = query.value(1).toString();
                row[QStringLiteral("pattern")] = query.value(2).toString();
                row[QStringLiteral("songCount")] = query.value(3).toInt();
                result.append(row);
            }
        }
        return result;
    }

    // Background music (Phase 7). Deliberately separate tables, not a flag on
    // `songs`: the folders are different folders, so the karaoke library and the
    // background library can never bleed into one another.
    Q_INVOKABLE bool addBackgroundDirectory(const QString &directoryPath);
    Q_INVOKABLE bool removeBackgroundDirectory(const QString &directoryPath);
    Q_INVOKABLE void rescanBackgroundDirectory(const QString &directoryPath);
    QVariantList getAllBackgroundSongs(bool includeDeleted = false);
    bool backgroundSongExists(const QString &filePath);

    // Utility functions
    QString parseSongTitle(const QString &fileName);
    QString parseArtistFromTitle(const QString &title);
    QString parseTitleFromArtist(const QString &artist);

    // Persistent UI state (singer rotation + song queue)
    QVariantList loadSingers();
    QVariantList loadQueue();
    void saveSingers(const QVariantList &singers);
    void saveQueue(const QVariantList &items);

    // The background playlist belongs to the app rather than to a singer, so it
    // carries no singer column and never reaches the rotation.
    QVariantList loadBackgroundPlaylist();
    void saveBackgroundPlaylist(const QVariantList &items);

private:
    QSqlDatabase m_db;
    QString m_dataLocation;
    int m_songCount;
    int m_backgroundSongCount = 0;

    QFutureWatcher<ScannedFile> *m_scanWatcher = nullptr;
    QString m_scanSongTable;
    int m_scanDirectoryId = -1;
    bool m_scanIsBackground = false;
    int m_scanProgress = 0;
    int m_scanTotal = 0;

    void ensureIndexes();

    // Measuring a file means spawning ffprobe, which is far too slow to run on
    // the UI thread - a few hundred tracks is a fifteen-second freeze. The scan
    // and the measurements are handed to QtConcurrent, and the rows are written
    // once the results come back, in a single transaction.
    void startScan(QVector<ScannedFile> files, const QString &songTable,
                   int directoryId, bool background);
    void finishScan();
    QVariantList executeQuery(const QString &query);
    QVariantList executeQuery(QSqlQuery &query);
    bool executePreparedQuery(const QString &query, const QVariantList &bindings = QVariantList());
    
    // Signals section - MUST be inside the class, after private:
    signals:
        void dataLocationChanged();
        void songCountChanged();
        void backgroundSongCountChanged();
        // Emitted when an asynchronous index run has written its rows. `added`
        // counts the songs stored, `unreadable` those ffprobe refused.
        void libraryScanFinished(int added, int unreadable);
        void backgroundScanFinished(int added, int unreadable);
        void scanProgressChanged();
};

#endif // DATABASEMANAGER_H
