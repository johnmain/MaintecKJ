#ifndef SONGQUEUEMODEL_H
#define SONGQUEUEMODEL_H

#include <QAbstractListModel>
#include <QObject>
#include <QString>

class SongQueueModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum SongRoles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole
    };

    explicit SongQueueModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addSong(const QString &title, const QString &artist);

private:
    struct QueueItem {
        int id;
        QString title;
        QString artist;
    };
    QList<QueueItem> m_songs;
    int m_nextId;
};

#endif // SONGQUEUEMODEL_H
