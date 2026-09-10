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
    bool addSong(const QString &artist, const QString &title, const QString &filePath, int duration = 0);
    bool removeSong(int id);
    bool updateSong(int id, const QString &artist, const QString &title, const QString &filePath, int duration);
    bool songExists(const QString &filePath);

    // Query operations
    QVariantList searchSongs(const QString &query = "");
    QVariantList getAllSongs();
    Song getSongById(int id);
    QVariantList getSongsByArtist(const QString &artist);
    QVariantList getSongsByTitle(const QString &title);

    // Directory operations
    bool addDirectory(const QString &directoryPath);
    bool removeDirectory(const QString &directoryPath);
    void rescanDirectory(const QString &directoryPath);

    // Utility functions
    QString parseSongTitle(const QString &fileName);
    QString parseArtistFromTitle(const QString &title);
    QString parseTitleFromArtist(const QString &artist);

    Q_INVOKABLE void createTestData();

private:
    QSqlDatabase m_db;
    QString m_dataLocation;
    int m_songCount;

    void ensureIndexes();
    QVariantList executeQuery(const QString &query);
    bool executePreparedQuery(const QString &query, const QVariantList &bindings = QVariantList());
};

#endif // DATABASEMANAGER_H