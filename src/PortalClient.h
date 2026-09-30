#ifndef PORTALCLIENT_H
#define PORTALCLIENT_H

#include <QList>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class SongListExporter;

// Talks to the self-hosted singer portal (MaintecKJ_SongRequest).
//
// v1 only uploads the song catalog: the Artist/Title export the song-book
// workflow already produces is POSTed to /api/catalog/ingest, which upserts the
// portal's master song table. Connection details live in the QSettings category
// "Portal" so the Settings tab can edit them directly.
class PortalClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY statusChanged)
    Q_PROPERTY(QString lastSummary READ lastSummary NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)
    Q_PROPERTY(int pendingUpdates READ pendingUpdates NOTIFY statusChanged)

public:
    explicit PortalClient(QObject *parent = nullptr);

    void setSongListExporter(SongListExporter *exporter);

    QString lastError() const { return m_lastError; }
    QString lastSummary() const { return m_lastSummary; }
    bool busy() const { return m_busy; }
    // Status updates that failed to reach the portal and are waiting a retry.
    int pendingUpdates() const { return static_cast<int>(m_pendingStatusUpdates.size()); }

    // Builds the song-list export and POSTs it to the portal. Reports why it
    // did nothing when the bridge is disabled or not configured.
    Q_INVOKABLE void syncSongList();

    // GETs the portal health endpoint to confirm the base URL is reachable.
    Q_INVOKABLE void testConnection();

    // Pull model: claims new singer requests from the portal and emits
    // requestsReceived(...). Safe to call on a timer; no-op when disabled.
    Q_INVOKABLE void pollRequests();

    // Reports a triaged request's status back to the portal.
    Q_INVOKABLE void updateRequestStatus(const QString &portalRequestId, const QString &status);

    // True when an app singer name matches a portal account (the Request DB
    // indicator). Uses the cached singer directory, refreshed on a timer.
    Q_INVOKABLE bool isSingerInPortal(const QString &singerName) const;

    // Refreshes the cached portal singer directory (GET /api/host/singers).
    Q_INVOKABLE void refreshSingers();

    // Pushes one singer's whole queue to the portal, which reconciles its
    // requests for that singer. `songs` is a list of { title, artist, played }.
    Q_INVOKABLE void pushSingerQueue(const QString &singerName, const QVariantList &songs);

    // Same call with a dry run: the portal computes the add/update/remove counts
    // but writes nothing. Emits queuePreviewed() so the UI can confirm first.
    Q_INVOKABLE void previewSingerQueue(const QString &singerName, const QVariantList &songs);

signals:
    void statusChanged();
    void requestsReceived(const QVariantList &requests);
    void queueUpdatesReceived(const QVariantList &updates);
    // Portal request ids the singer deleted; drop the matching queue rows and
    // acknowledge with updateRequestStatus(id, "removed").
    void removalsReceived(const QStringList &portalRequestIds);
    // Dry-run result for a queue push, for the confirmation dialog.
    void queuePreviewed(const QVariantMap &summary);
    // The set of known portal singer names changed.
    void singersChanged();
    // A queue push completed; carries the portal's reconcile summary.
    void queuePushed(const QVariantMap &result);

private:
    void setBusy(bool busy);
    void setError(const QString &message);
    void setSummary(const QString &message);
    void finishSync(QNetworkReply *reply);

    // Refreshes the singer directory at most once a minute, from the poll tick.
    void maybeRefreshSingers();

    // Shared implementation of pushSingerQueue() / previewSingerQueue().
    void sendQueuePush(const QString &singerName, const QVariantList &songs, bool dryRun);

    // Canonical name key, mirroring the portal's normalizeText() so both sides
    // agree on whether "Alice Cooper" and "alice  cooper" are the same singer.
    static QString singerKey(const QString &value);

    // Sends queued status updates one at a time; called after a queue change and
    // on every poll tick so a failed update is retried until it lands.
    void drainStatusUpdates();

    // Trims, strips trailing slashes, and adds a default http:// scheme so a
    // bare host:port works for LAN testing (https:// is used in production).
    static QString normalizeBaseUrl(const QString &raw);

    SongListExporter *m_songListExporter = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QTimer *m_pollTimer = nullptr;
    bool m_pollBusy = false;
    // (portalRequestId, status) pairs waiting to be PATCHed; latest wins per id.
    QList<QPair<QString, QString>> m_pendingStatusUpdates;
    bool m_updatesInFlight = false;
    QPair<QString, QString> m_inFlightUpdate;
    QString m_lastError;
    QString m_lastSummary;
    bool m_busy = false;
    // Normalized names (display + stage) of every portal account.
    QSet<QString> m_portalSingerKeys;
    qint64 m_lastSingersRefreshMs = 0;
};

#endif // PORTALCLIENT_H
