#ifndef SONGLISTEXPORTER_H
#define SONGLISTEXPORTER_H

#include <QObject>
#include <QString>

class DatabaseManager;

// Writes the library out as a plain Artist/Title list, in the shape the
// song-book workflow already uses:
//
//   [ { "Artist": "+44", "Title": "When Your Heart Stops Beating" } ]
//
// Artists and titles are already normalised at scan time - FolderScanner peels
// the karaoke provider tag off the filename into the `source` column - so this
// only has to drop duplicates and sort. The `source` column is deliberately not
// part of the output.
class SongListExporter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastSummary READ lastSummary NOTIFY lastSummaryChanged)

public:
    explicit SongListExporter(QObject *parent = nullptr);

    void setDatabaseManager(DatabaseManager *databaseManager);

    QString lastError() const { return m_lastError; }
    QString lastSummary() const { return m_lastSummary; }

    // Number of unique Artist/Title pairs the last export wrote.
    Q_INVOKABLE int lastExportedCount() const { return m_lastExportedCount; }

    // Accepts a plain path or a file:// URL, so QML can hand over a FileDialog
    // result directly. Returns false when nothing was written.
    Q_INVOKABLE bool exportToFile(const QString &filePath);

    // Builds the same Artist/Title JSON exportToFile() writes, without touching
    // disk. PortalClient uploads these bytes to the singer portal unchanged.
    Q_INVOKABLE QString buildJsonString();

signals:
    void lastErrorChanged();
    void lastSummaryChanged();
    void exportFinished();

private:
    void setError(const QString &message);
    void setSummary(const QString &message);

    DatabaseManager *m_databaseManager = nullptr;
    QString m_lastError;
    QString m_lastSummary;
    int m_lastExportedCount = 0;
};

#endif // SONGLISTEXPORTER_H
