#ifndef WEBREQUESTMODEL_H
#define WEBREQUESTMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class DatabaseManager;

// Singer requests claimed from the web portal (pull model). They sit here until
// the host triages them: add the song to a singer's queue, or reject it.
class WebRequestModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY pendingCountChanged)

public:
    enum WebRequestRoles {
        PortalRequestIdRole = Qt::UserRole + 1,
        SingerNameRole,
        StageNameRole,
        ArtistRole,
        TitleRole,
        NoteRole,
        RequestedAtRole
    };

    explicit WebRequestModel(QObject *parent = nullptr);
    void setDatabaseManager(DatabaseManager *databaseManager);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int pendingCount() const { return static_cast<int>(m_requests.size()); }

    // Adds newly claimed portal requests (payload from POST /requests/poll),
    // skipping any already held (unique by portal request id). Returns how many
    // were added.
    Q_INVOKABLE int ingest(const QVariantList &requests);

    // Triage outcomes. Both drop the row from the pending list and persist it;
    // the caller tells the portal the resulting status.
    Q_INVOKABLE bool resolve(const QString &portalRequestId);
    Q_INVOKABLE bool reject(const QString &portalRequestId);

signals:
    void pendingCountChanged();

private:
    struct WebRequest {
        QString portalRequestId;
        QString singerName;
        QString stageName;
        QString artist;
        QString title;
        QString note;
        QString requestedAt;
    };

    void reload();
    bool removeRequest(const QString &portalRequestId);

    QList<WebRequest> m_requests;
    DatabaseManager *m_databaseManager = nullptr;
};

#endif // WEBREQUESTMODEL_H
