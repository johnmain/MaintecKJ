#include "SongQueueModel.h"
#include <QDebug>

SongQueueModel::SongQueueModel(QObject *parent) : QAbstractListModel(parent), m_nextId(1)
{
    addSong("Sample Song 1", "Sample Artist 1");
    addSong("Sample Song 2", "Sample Artist 2");
}

int SongQueueModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_songs.size();
}

QVariant SongQueueModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_songs.size())
        return QVariant();

    const QueueItem &song = m_songs.at(index.row());

    switch (role) {
    case IdRole:
        return song.id;
    case TitleRole:
        return song.title;
    case ArtistRole:
        return song.artist;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> SongQueueModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "songId";
    roles[TitleRole] = "songTitle";
    roles[ArtistRole] = "songArtist";
    return roles;
}

void SongQueueModel::addSong(const QString &title, const QString &artist)
{
    beginInsertRows(QModelIndex(), m_songs.size(), m_songs.size());
    
    QueueItem newSong;
    newSong.id = m_nextId++;
    newSong.title = title;
    newSong.artist = artist;
    
    m_songs.append(newSong);
    
    endInsertRows();
}
