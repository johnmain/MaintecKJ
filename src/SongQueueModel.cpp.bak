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
#include "SongQueueModel.h"
#include <QUuid>

SongQueueModel::SongQueueModel(QObject *parent)
    : QAbstractListModel(parent), m_nextId(0)
{
}

int SongQueueModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_songs.size();
}

QVariant SongQueueModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_songs.size())
        return QVariant();

    const SongItem &song = m_songs.at(index.row());
    
    switch (role) {
    case IdRole:
        return song.id;
    case SingerNameRole:
        return song.singerName;
    case SongTitleRole:
        return song.songTitle;
    case ArtistRole:
        return song.artist;
    case FilePathRole:
        return song.filePath;
    case DurationRole:
        return song.duration;
    case IsPlayedRole:
        return song.isPlayed;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> SongQueueModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[SingerNameRole] = "singerName";
    roles[SongTitleRole] = "songTitle";
    roles[ArtistRole] = "artist";
    roles[FilePathRole] = "filePath";
    roles[DurationRole] = "duration";
    roles[IsPlayedRole] = "isPlayed";
    return roles;
}

void SongQueueModel::addSong(const QString &singer, const QString &title, const QString &artist, 
                            const QString &path, int duration)
{
    beginInsertRows(QModelIndex(), m_songs.size(), m_songs.size());
    
    SongItem newSong;
    newSong.id = QUuid::createUuid().toString();
    newSong.singerName = singer;
    newSong.songTitle = title;
    newSong.artist = artist;
    newSong.filePath = path;
    newSong.duration = duration;
    newSong.isPlayed = false;
    
    m_songs.append(newSong);
    endInsertRows();
}

void SongQueueModel::removeSong(int index)
{
    if (index < 0 || index >= m_songs.size())
        return;
        
    beginRemoveRows(QModelIndex(), index, index);
    m_songs.removeAt(index);
    endRemoveRows();
}

void SongQueueModel::moveSong(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_songs.size() || 
        toIndex < 0 || toIndex >= m_songs.size() || 
        fromIndex == toIndex)
        return;
        
    beginMoveRows(QModelIndex(), fromIndex, fromIndex, QModelIndex(), toIndex > fromIndex ? toIndex + 1 : toIndex);
    
    if (toIndex > fromIndex) {
        m_songs.move(fromIndex, toIndex);
    } else {
        m_songs.move(fromIndex, toIndex);
    }
    endMoveRows();
}

void SongQueueModel::markAsPlayed(int index, bool played)
{
    if (index < 0 || index >= m_songs.size())
        return;
        
    m_songs[index].isPlayed = played;
    
    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {IsPlayedRole});
}

void SongQueueModel::clearQueue()
{
    if (m_songs.isEmpty())
        return;
        
    beginRemoveRows(QModelIndex(), 0, m_songs.size() - 1);
    m_songs.clear();
    endRemoveRows();
}
