#include "WebRequestModel.h"

#include "DatabaseManager.h"

#include <utility>

WebRequestModel::WebRequestModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void WebRequestModel::setDatabaseManager(DatabaseManager *databaseManager)
{
    m_databaseManager = databaseManager;
    reload();
}

int WebRequestModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_requests.size());
}

QVariant WebRequestModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_requests.size())
        return QVariant();

    const WebRequest &request = m_requests.at(index.row());
    switch (role) {
    case PortalRequestIdRole:
        return request.portalRequestId;
    case SingerNameRole:
        return request.singerName;
    case StageNameRole:
        return request.stageName;
    case ArtistRole:
        return request.artist;
    case TitleRole:
        return request.title;
    case NoteRole:
        return request.note;
    case RequestedAtRole:
        return request.requestedAt;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> WebRequestModel::roleNames() const
{
    return {
        { PortalRequestIdRole, "portalRequestId" },
        { SingerNameRole, "singerName" },
        { StageNameRole, "stageName" },
        { ArtistRole, "artist" },
        { TitleRole, "title" },
        { NoteRole, "note" },
        { RequestedAtRole, "requestedAt" }
    };
}

void WebRequestModel::reload()
{
    beginResetModel();
    m_requests.clear();

    if (m_databaseManager) {
        const QVariantList rows = m_databaseManager->loadWebRequests();
        for (const QVariant &value : rows) {
            const QVariantMap row = value.toMap();
            WebRequest request;
            request.portalRequestId = row.value(QStringLiteral("portalRequestId")).toString();
            request.singerName = row.value(QStringLiteral("singerName")).toString();
            request.stageName = row.value(QStringLiteral("stageName")).toString();
            request.artist = row.value(QStringLiteral("artist")).toString();
            request.title = row.value(QStringLiteral("title")).toString();
            request.note = row.value(QStringLiteral("note")).toString();
            request.requestedAt = row.value(QStringLiteral("requestedAt")).toString();
            m_requests.append(request);
        }
    }

    endResetModel();
    emit pendingCountChanged();
}

int WebRequestModel::ingest(const QVariantList &requests)
{
    int added = 0;

    for (const QVariant &value : requests) {
        const QVariantMap row = value.toMap();
        const QString id = row.value(QStringLiteral("id")).toString();
        if (id.isEmpty())
            continue;

        bool known = false;
        for (const WebRequest &existing : std::as_const(m_requests)) {
            if (existing.portalRequestId == id) {
                known = true;
                break;
            }
        }
        if (known)
            continue;

        const QVariantMap song = row.value(QStringLiteral("song")).toMap();
        const QVariantMap singer = row.value(QStringLiteral("singer")).toMap();

        WebRequest request;
        request.portalRequestId = id;
        request.singerName = singer.value(QStringLiteral("name")).toString();
        request.stageName = singer.value(QStringLiteral("stageName")).toString();
        request.artist = song.value(QStringLiteral("artist")).toString();
        request.title = song.value(QStringLiteral("title")).toString();
        request.note = row.value(QStringLiteral("note")).toString();
        request.requestedAt = row.value(QStringLiteral("requestedAt")).toString();

        if (m_databaseManager) {
            QVariantMap insert;
            insert[QStringLiteral("portalRequestId")] = request.portalRequestId;
            insert[QStringLiteral("singerName")] = request.singerName;
            insert[QStringLiteral("stageName")] = request.stageName;
            insert[QStringLiteral("artist")] = request.artist;
            insert[QStringLiteral("title")] = request.title;
            insert[QStringLiteral("note")] = request.note;
            insert[QStringLiteral("requestedAt")] = request.requestedAt;
            m_databaseManager->insertWebRequest(insert);
        }

        const int rowIndex = static_cast<int>(m_requests.size());
        beginInsertRows(QModelIndex(), rowIndex, rowIndex);
        m_requests.append(request);
        endInsertRows();
        ++added;
    }

    if (added > 0)
        emit pendingCountChanged();

    return added;
}

bool WebRequestModel::resolve(const QString &portalRequestId)
{
    return removeRequest(portalRequestId);
}

bool WebRequestModel::reject(const QString &portalRequestId)
{
    return removeRequest(portalRequestId);
}

bool WebRequestModel::removeRequest(const QString &portalRequestId)
{
    for (int i = 0; i < m_requests.size(); ++i) {
        if (m_requests.at(i).portalRequestId != portalRequestId)
            continue;

        beginRemoveRows(QModelIndex(), i, i);
        m_requests.removeAt(i);
        endRemoveRows();

        if (m_databaseManager)
            m_databaseManager->deleteWebRequest(portalRequestId);

        emit pendingCountChanged();
        return true;
    }

    return false;
}
