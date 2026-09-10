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
#ifndef SONGQUEUEMODEL_H
#define SONGQUEUEMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QString>

struct SongItem {
    QString id;
    QString singerName;
    QString songTitle;
    QString artist;
    QString filePath;
    int duration;
    bool isPlayed;
};

class SongQueueModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum SongRoles {
        IdRole = Qt::UserRole + 1,
        SingerNameRole,
        SongTitleRole,
        ArtistRole,
        FilePathRole,
        DurationRole,
        IsPlayedRole
    };

    explicit SongQueueModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addSong(const QString &singer, const QString &title, const QString &artist, 
                             const QString &path, int duration);
    Q_INVOKABLE void removeSong(int index);
    Q_INVOKABLE void moveSong(int fromIndex, int toIndex);
    Q_INVOKABLE void markAsPlayed(int index, bool played);
    Q_INVOKABLE void clearQueue();

private:
    QList<SongItem> m_songs;
    int m_nextId;
};

#endif // SONGQUEUEMODEL_H
