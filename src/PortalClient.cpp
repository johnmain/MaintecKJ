#include "PortalClient.h"

#include "SongListExporter.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QUrl>

namespace {

// Everything the bridge needs lives under one QSettings group so the Settings
// tab and this client can never drift apart.
constexpr auto kPortalGroup = "Portal";
constexpr int kSyncTimeoutMs = 15000;
constexpr int kTestTimeoutMs = 10000;

} // namespace

PortalClient::PortalClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_pollTimer(new QTimer(this))
{
    // Poll for new requests. QSettings is re-read on every tick, so a change in
    // the Settings tab takes effect without a restart. pollRequests() is a no-op
    // while the portal is disabled.
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(kPortalGroup));
    const int configuredInterval = settings.value(QStringLiteral("pollIntervalMs"), 5000).toInt();
    settings.endGroup();
    m_pollTimer->setInterval(configuredInterval > 0 ? configuredInterval : 5000);

    connect(m_pollTimer, &QTimer::timeout, this, [this]() {
        QSettings tick;
        tick.beginGroup(QString::fromLatin1(kPortalGroup));
        const int interval = tick.value(QStringLiteral("pollIntervalMs"), 5000).toInt();
        tick.endGroup();
        if (interval > 0 && interval != m_pollTimer->interval())
            m_pollTimer->setInterval(interval);
        pollRequests();
        drainStatusUpdates();
        maybeRefreshSingers();
    });
    m_pollTimer->start();
}

void PortalClient::setSongListExporter(SongListExporter *exporter)
{
    m_songListExporter = exporter;
}

QString PortalClient::normalizeBaseUrl(const QString &raw)
{
    QString url = raw.trimmed();
    while (url.endsWith(QLatin1Char('/')))
        url.chop(1);
    // Testing convenience: "192.168.1.50:3000" becomes http://192.168.1.50:3000.
    if (!url.isEmpty() && !url.contains(QStringLiteral("://")))
        url.prepend(QStringLiteral("http://"));
    return url;
}

void PortalClient::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit statusChanged();
}

void PortalClient::setError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit statusChanged();
}

void PortalClient::setSummary(const QString &message)
{
    if (m_lastSummary == message)
        return;
    m_lastSummary = message;
    emit statusChanged();
}

void PortalClient::syncSongList()
{
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(kPortalGroup));
    const bool enabled = settings.value(QStringLiteral("enabled"), false).toBool();
    const QString portalUrl =
        normalizeBaseUrl(settings.value(QStringLiteral("portalUrl")).toString());
    const QString token = settings.value(QStringLiteral("bridgeToken")).toString().trimmed();
    settings.endGroup();

    if (!enabled) {
        setSummary(QString());
        setError(tr("The singer portal is disabled in Settings."));
        return;
    }
    if (portalUrl.isEmpty() || token.isEmpty()) {
        setSummary(QString());
        setError(tr("Set the portal URL and bridge token in Settings first."));
        return;
    }
    if (!m_songListExporter) {
        setSummary(QString());
        setError(tr("The portal client is not connected to the exporter."));
        return;
    }

    // The exact bytes the song-book export would write, uploaded unchanged.
    const QString payload = m_songListExporter->buildJsonString();
    if (payload.isEmpty()) {
        setSummary(QString());
        setError(tr("Could not build the song list export."));
        return;
    }

    QNetworkRequest request(QUrl(portalUrl + QStringLiteral("/api/catalog/ingest")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + token.toUtf8());
    request.setTransferTimeout(kSyncTimeoutMs);

    setError(QString());
    setSummary(tr("Syncing %1 songs…").arg(m_songListExporter->lastExportedCount()));
    setBusy(true);

    QNetworkReply *reply = m_network->post(request, payload.toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        finishSync(reply);
        reply->deleteLater();
    });
}

void PortalClient::finishSync(QNetworkReply *reply)
{
    setBusy(false);

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();

    // Qt reports 401 as "Host requires authentication"; say what it actually
    // means so a token mismatch is obvious.
    if (status == 401) {
        setError(tr("Portal rejected the bridge token (401). The token here must match "
                    "HOST_BRIDGE_TOKEN in the portal's .env."));
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        setError(tr("Portal sync failed: %1").arg(reply->errorString()));
        return;
    }
    if (status < 200 || status >= 300) {
        setError(tr("Portal sync failed (HTTP %1).").arg(status));
        return;
    }

    const QJsonObject object = QJsonDocument::fromJson(body).object();
    setError(QString());
    setSummary(tr("Synced: %1 new, %2 updated, %3 skipped.")
                   .arg(object.value(QStringLiteral("created")).toInt())
                   .arg(object.value(QStringLiteral("updated")).toInt())
                   .arg(object.value(QStringLiteral("skipped")).toInt()));
}

void PortalClient::testConnection()
{
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(kPortalGroup));
    const QString portalUrl =
        normalizeBaseUrl(settings.value(QStringLiteral("portalUrl")).toString());
    settings.endGroup();

    if (portalUrl.isEmpty()) {
        setError(tr("Set the portal URL first."));
        return;
    }

    // /api/health is public and not part of the auth routes, so this works over
    // plain http on the LAN regardless of the portal's ORIGIN setting.
    QNetworkRequest request(QUrl(portalUrl + QStringLiteral("/api/health")));
    request.setTransferTimeout(kTestTimeoutMs);

    setError(QString());
    setSummary(tr("Contacting the portal…"));
    setBusy(true);

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        setBusy(false);
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            setError(tr("Could not reach the portal: %1").arg(reply->errorString()));
        } else if (status < 200 || status >= 300) {
            setError(tr("Portal responded with HTTP %1.").arg(status));
        } else {
            setError(QString());
            setSummary(tr("Portal reachable (%1).").arg(QString::fromUtf8(body).trimmed()));
        }
        reply->deleteLater();
    });
}

void PortalClient::pollRequests()
{
    if (m_pollBusy)
        return;

    QSettings settings;
    settings.beginGroup(QString::fromLatin1(kPortalGroup));
    const bool enabled = settings.value(QStringLiteral("enabled"), false).toBool();
    const QString portalUrl =
        normalizeBaseUrl(settings.value(QStringLiteral("portalUrl")).toString());
    const QString token = settings.value(QStringLiteral("bridgeToken")).toString().trimmed();
    const bool accepting = settings.value(QStringLiteral("accepting"), false).toBool();
    settings.endGroup();

    if (!enabled || portalUrl.isEmpty() || token.isEmpty())
        return;

    QNetworkRequest request(QUrl(portalUrl + QStringLiteral("/api/host/requests/poll")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + token.toUtf8());
    // The poll doubles as the heartbeat; X-Accepting feeds the public status.
    request.setRawHeader("X-Accepting", accepting ? "true" : "false");
    request.setTransferTimeout(kSyncTimeoutMs);

    m_pollBusy = true;
    QNetworkReply *reply = m_network->post(request, QByteArray());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_pollBusy = false;

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();

        if (status == 401) {
            setError(tr("Portal rejected the bridge token (401). The token here must match "
                        "HOST_BRIDGE_TOKEN in the portal's .env."));
        } else if (reply->error() != QNetworkReply::NoError) {
            setError(tr("Request poll failed: %1").arg(reply->errorString()));
        } else if (status >= 200 && status < 300) {
            const QJsonObject object = QJsonDocument::fromJson(body).object();
            const QVariantList requests =
                object.value(QStringLiteral("requests")).toArray().toVariantList();
            const QVariantList updates =
                object.value(QStringLiteral("updates")).toArray().toVariantList();
            const QJsonArray removals = object.value(QStringLiteral("removals")).toArray();

            setError(QString());
            if (!requests.isEmpty()) {
                setSummary(tr("Claimed %1 new request(s).").arg(requests.size()));
                emit requestsReceived(requests);
            }
            if (!updates.isEmpty())
                emit queueUpdatesReceived(updates);
            if (!removals.isEmpty()) {
                QStringList ids;
                ids.reserve(removals.size());
                for (const QJsonValue &value : removals)
                    ids.append(value.toString());
                emit removalsReceived(ids);
            }
        }

        reply->deleteLater();
    });
}

void PortalClient::updateRequestStatus(const QString &portalRequestId, const QString &status)
{
    if (portalRequestId.isEmpty() || status.isEmpty())
        return;

    // Latest status wins per request; keep the rest in order. The one currently
    // in flight is left alone (its own success handler removes it).
    for (int i = m_pendingStatusUpdates.size() - 1; i >= 0; --i) {
        if (m_pendingStatusUpdates.at(i).first != portalRequestId)
            continue;
        if (m_updatesInFlight && m_inFlightUpdate.first == portalRequestId)
            continue;
        m_pendingStatusUpdates.removeAt(i);
    }
    m_pendingStatusUpdates.append(qMakePair(portalRequestId, status));
    emit statusChanged();

    drainStatusUpdates();
}

void PortalClient::drainStatusUpdates()
{
    if (m_updatesInFlight || m_pendingStatusUpdates.isEmpty())
        return;

    QSettings settings;
    settings.beginGroup(QString::fromLatin1(kPortalGroup));
    const bool enabled = settings.value(QStringLiteral("enabled"), false).toBool();
    const QString portalUrl =
        normalizeBaseUrl(settings.value(QStringLiteral("portalUrl")).toString());
    const QString token = settings.value(QStringLiteral("bridgeToken")).toString().trimmed();
    settings.endGroup();

    if (!enabled || portalUrl.isEmpty() || token.isEmpty())
        return;

    m_updatesInFlight = true;
    m_inFlightUpdate = m_pendingStatusUpdates.first();

    QJsonObject payload;
    payload[QStringLiteral("status")] = m_inFlightUpdate.second;

    QNetworkRequest request(
        QUrl(portalUrl + QStringLiteral("/api/host/requests/") + m_inFlightUpdate.first));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + token.toUtf8());
    request.setTransferTimeout(kSyncTimeoutMs);

    QNetworkReply *reply = m_network->sendCustomRequest(
        request, QByteArrayLiteral("PATCH"), QJsonDocument(payload).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool ok =
            reply->error() == QNetworkReply::NoError && httpStatus >= 200 && httpStatus < 300;

        m_updatesInFlight = false;

        if (ok) {
            // Drop exactly the pair that just went through.
            for (int i = 0; i < m_pendingStatusUpdates.size(); ++i) {
                if (m_pendingStatusUpdates.at(i) == m_inFlightUpdate) {
                    m_pendingStatusUpdates.removeAt(i);
                    break;
                }
            }

            if (m_pendingStatusUpdates.isEmpty()) {
                setError(QString());
                setSummary(tr("Portal up to date."));
            } else {
                setSummary(
                    tr("%1 status update(s) still pending.").arg(m_pendingStatusUpdates.size()));
            }
        } else {
            setError(tr("Could not report request status (%1) — %2 update(s) pending retry.")
                         .arg(reply->errorString())
                         .arg(m_pendingStatusUpdates.size()));
        }

        emit statusChanged();
        reply->deleteLater();

        // More to send? Keep going; a failure waits for the next poll tick.
        if (ok)
            drainStatusUpdates();
    });
}

QString PortalClient::singerKey(const QString &value)
{
    // Mirrors the portal's normalizeText(): NFKD, strip combining marks, NFC,
    // lower-case, and collapse runs of non-alphanumerics to a single space.
    const QString decomposed = value.normalized(QString::NormalizationForm_KD);

    QString stripped;
    stripped.reserve(decomposed.size());
    for (const QChar ch : decomposed) {
        const ushort u = ch.unicode();
        const bool combining = (u >= 0x0300 && u <= 0x036f) || (u >= 0x1ab0 && u <= 0x1aff)
                               || (u >= 0x1dc0 && u <= 0x1dff);
        if (!combining)
            stripped.append(ch);
    }

    const QString composed = stripped.normalized(QString::NormalizationForm_C).toLower();

    QString result;
    result.reserve(composed.size());
    bool pendingSpace = false;
    for (const QChar ch : composed) {
        if (ch.isLetterOrNumber()) {
            if (pendingSpace && !result.isEmpty())
                result.append(QLatin1Char(' '));
            pendingSpace = false;
            result.append(ch);
        } else {
            pendingSpace = true;
        }
    }
    return result;
}

bool PortalClient::isSingerInPortal(const QString &singerName) const
{
    const QString key = singerKey(singerName);
    return !key.isEmpty() && m_portalSingerKeys.contains(key);
}

void PortalClient::maybeRefreshSingers()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastSingersRefreshMs < 60000)
        return;
    m_lastSingersRefreshMs = now;
    refreshSingers();
}

void PortalClient::refreshSingers()
{
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(kPortalGroup));
    const bool enabled = settings.value(QStringLiteral("enabled"), false).toBool();
    const QString portalUrl =
        normalizeBaseUrl(settings.value(QStringLiteral("portalUrl")).toString());
    const QString token = settings.value(QStringLiteral("bridgeToken")).toString().trimmed();
    settings.endGroup();

    if (!enabled || portalUrl.isEmpty() || token.isEmpty())
        return;

    QNetworkRequest request(QUrl(portalUrl + QStringLiteral("/api/host/singers")));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + token.toUtf8());
    request.setTransferTimeout(kSyncTimeoutMs);

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();

        if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300) {
            const QJsonArray singers = QJsonDocument::fromJson(body)
                                           .object()
                                           .value(QStringLiteral("singers"))
                                           .toArray();
            QSet<QString> keys;
            for (const QJsonValue &value : singers) {
                const QJsonObject singer = value.toObject();
                const QString name =
                    singerKey(singer.value(QStringLiteral("name")).toString());
                const QString stage =
                    singerKey(singer.value(QStringLiteral("stageName")).toString());
                if (!name.isEmpty())
                    keys.insert(name);
                if (!stage.isEmpty())
                    keys.insert(stage);
            }
            if (keys != m_portalSingerKeys) {
                m_portalSingerKeys = keys;
                emit singersChanged();
            }
        }
        reply->deleteLater();
    });
}

void PortalClient::pushSingerQueue(const QString &singerName, const QVariantList &songs)
{
    sendQueuePush(singerName, songs, false);
}

void PortalClient::previewSingerQueue(const QString &singerName, const QVariantList &songs)
{
    sendQueuePush(singerName, songs, true);
}

void PortalClient::sendQueuePush(const QString &singerName, const QVariantList &songs, bool dryRun)
{
    const QString name = singerName.trimmed();
    if (name.isEmpty()) {
        setSummary(QString());
        setError(tr("Select a singer first."));
        return;
    }

    QSettings settings;
    settings.beginGroup(QString::fromLatin1(kPortalGroup));
    const bool enabled = settings.value(QStringLiteral("enabled"), false).toBool();
    const QString portalUrl =
        normalizeBaseUrl(settings.value(QStringLiteral("portalUrl")).toString());
    const QString token = settings.value(QStringLiteral("bridgeToken")).toString().trimmed();
    settings.endGroup();

    if (!enabled) {
        setSummary(QString());
        setError(tr("The singer portal is disabled in Settings."));
        return;
    }
    if (portalUrl.isEmpty() || token.isEmpty()) {
        setSummary(QString());
        setError(tr("Set the portal URL and bridge token in Settings first."));
        return;
    }

    QJsonArray songArray;
    for (const QVariant &value : songs) {
        const QVariantMap row = value.toMap();
        QJsonObject song;
        song[QStringLiteral("title")] = row.value(QStringLiteral("title")).toString();
        song[QStringLiteral("artist")] = row.value(QStringLiteral("artist")).toString();
        song[QStringLiteral("played")] = row.value(QStringLiteral("played")).toBool();
        songArray.append(song);
    }

    QJsonObject payload;
    payload[QStringLiteral("singerName")] = name;
    payload[QStringLiteral("songs")] = songArray;
    if (dryRun)
        payload[QStringLiteral("dryRun")] = true;

    QNetworkRequest request(QUrl(portalUrl + QStringLiteral("/api/host/queue/push")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + token.toUtf8());
    request.setTransferTimeout(kSyncTimeoutMs);

    setError(QString());
    setSummary(dryRun ? tr("Checking the queue for %1…").arg(name)
                      : tr("Pushing %1 song(s) for %2…").arg(songArray.size()).arg(name));
    setBusy(true);

    QNetworkReply *reply =
        m_network->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, name, dryRun]() {
        setBusy(false);

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const QJsonObject object = QJsonDocument::fromJson(body).object();

        if (status == 401) {
            setSummary(QString());
            setError(tr("Portal rejected the bridge token (401). The token here must match "
                        "HOST_BRIDGE_TOKEN in the portal's .env."));
        } else if (status == 404 || status == 409) {
            setSummary(QString());
            setError(object.value(QStringLiteral("message"))
                         .toString(status == 404
                                       ? tr("That singer is not in the Request DB.")
                                       : tr("Several portal singers match that name.")));
        } else if (reply->error() != QNetworkReply::NoError) {
            setError(tr("Queue push failed: %1").arg(reply->errorString()));
        } else if (status < 200 || status >= 300) {
            setError(tr("Queue push failed (HTTP %1).").arg(status));
        } else if (dryRun) {
            setError(QString());
            setSummary(QString());
            emit queuePreviewed(object.toVariantMap());
        } else {
            setError(QString());
            setSummary(tr("Pushed %1 song(s) for %2: %3 added, %4 updated, %5 removed.")
                           .arg(object.value(QStringLiteral("received")).toInt())
                           .arg(name)
                           .arg(object.value(QStringLiteral("created")).toInt())
                           .arg(object.value(QStringLiteral("updated")).toInt())
                           .arg(object.value(QStringLiteral("removed")).toInt()));
            // Carry the pushed app singer name alongside the server's per-song
            // request ids so the queue rows can be linked back to the portal.
            QVariantMap result = object.toVariantMap();
            result.insert(QStringLiteral("appSingerName"), name);
            emit queuePushed(result);
            refreshSingers();
        }

        reply->deleteLater();
    });
}
