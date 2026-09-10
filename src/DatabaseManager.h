#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QVariantList>
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

class DatabaseManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString dataLocation READ dataLocation NOTIFY dataLocationChanged)
    Q_PROPERTY(int songCount READ songCount NOTIFY songCountChanged)
    Q_PROPERTY(int backgroundSongCount READ backgroundSongCount NOTIFY backgroundSongCountChanged)

public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();

    // Getters
    QString dataLocation() const { return m_dataLocation; }
    int songCount() const { return m_songCount; }
    int backgroundSongCount() const { return m_backgroundSongCount; }

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

    // Directory operations
    Q_INVOKABLE bool addDirectory(const QString &directoryPath);
    Q_INVOKABLE bool removeDirectory(const QString &directoryPath);
    Q_INVOKABLE void rescanDirectory(const QString &directoryPath);

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

    void ensureIndexes();
    QVariantList executeQuery(const QString &query);
    QVariantList executeQuery(QSqlQuery &query);
    bool executePreparedQuery(const QString &query, const QVariantList &bindings = QVariantList());
    
    // Signals section - MUST be inside the class, after private:
    signals:
        void dataLocationChanged();
        void songCountChanged();
        void backgroundSongCountChanged();
};

#endif // DATABASEMANAGER_H
