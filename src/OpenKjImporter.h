#ifndef OPENKJIMPORTER_H
#define OPENKJIMPORTER_H

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>

class DatabaseManager;
class SingerModel;
class SongQueueModel;

// Imports regular singers, and the songs they have sung, from an OpenKJ
// "Export regulars" file. OpenKJ writes JSON; its own importer also accepts an
// older XML flavour, so both are read here.
//
// Imported entries are history, so their queue rows are marked played - without
// that the rotation would treat every song a singer has ever sung as a pending
// request. Songs are matched against our own library by artist and title so the
// queue points at files we can actually play; OpenKJ's own path is only used
// when no match is found.
class OpenKjImporter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastSummary READ lastSummary NOTIFY lastSummaryChanged)

public:
    explicit OpenKjImporter(QObject *parent = nullptr);

    void setDatabaseManager(DatabaseManager *databaseManager);
    void setSingerModel(SingerModel *singerModel);
    void setSongQueueModel(SongQueueModel *songQueueModel);

    QString lastError() const { return m_lastError; }
    QString lastSummary() const { return m_lastSummary; }

    // Accepts a plain path or a file:// URL, so QML can hand over a FileDialog
    // result directly. Returns false when nothing was imported.
    Q_INVOKABLE bool importFromFile(const QString &filePath);

signals:
    void lastErrorChanged();
    void lastSummaryChanged();
    void importFinished();

private:
    struct ImportedSong {
        QString artist;
        QString title;
        QString filePath;   // OpenKJ's path; empty for XML entries
        int keyShift = 0;
    };

    struct ImportedSinger {
        QString name;
        QList<ImportedSong> songs;
    };

    bool parseJson(const QByteArray &data, QList<ImportedSinger> &out);
    bool parseXml(const QByteArray &data, QList<ImportedSinger> &out);

    // Looks the song up in our own library by artist and title.
    bool resolveInLibrary(const QString &artist, const QString &title,
                          QString &path, int &duration, QString &source) const;

    void setError(const QString &message);
    void setSummary(const QString &message);

    DatabaseManager *m_databaseManager = nullptr;
    SingerModel *m_singerModel = nullptr;
    SongQueueModel *m_songQueueModel = nullptr;
    QString m_lastError;
    QString m_lastSummary;
};

#endif // OPENKJIMPORTER_H
