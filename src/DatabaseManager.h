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

public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();

    // Getters
    QString dataLocation() const { return m_dataLocation; }
    int songCount() const { return m_songCount; }

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

    // Utility functions
    QString parseSongTitle(const QString &fileName);
    QString parseArtistFromTitle(const QString &title);
    QString parseTitleFromArtist(const QString &artist);

    // Persistent UI state (singer rotation + song queue)
    QVariantList loadSingers();
    QVariantList loadQueue();
    void saveSingers(const QVariantList &singers);
    void saveQueue(const QVariantList &items);

private:
    QSqlDatabase m_db;
    QString m_dataLocation;
    int m_songCount;

    void ensureIndexes();
    QVariantList executeQuery(const QString &query);
    QVariantList executeQuery(QSqlQuery &query);
    bool executePreparedQuery(const QString &query, const QVariantList &bindings = QVariantList());
    
    // Signals section - MUST be inside the class, after private:
    signals:
        void dataLocationChanged();
        void songCountChanged();
};

#endif // DATABASEMANAGER_H
