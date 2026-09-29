#include "SongQueueModel.h"
#include "DatabaseManager.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <algorithm>

namespace {

// The deck plays the .mp3 beside a .cdg, so anything handing it a path has to
// agree on which of the pair that is - otherwise a caller comparing the queue's
// path with the deck's sees two different songs.
QString withPlayableCompanion(const QString &filePath)
{
    if (filePath.isEmpty())
        return filePath;
    if (QFileInfo(filePath).suffix().compare(QLatin1String("cdg"), Qt::CaseInsensitive) != 0)
        return filePath;

    const QFileInfo info(filePath);
    const QString companion = info.absolutePath() + QLatin1Char('/')
                              + info.completeBaseName() + QStringLiteral(".mp3");
    return QFile::exists(companion) ? companion : filePath;
}

} // namespace

SongQueueModel::SongQueueModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

// The model only ever exposes the songs of the currently selected singer, so
// the queue panel always shows one singer's personal queue.
void SongQueueModel::rebuildVisible()
{
    m_visible.clear();
    for (int i = 0; i < m_songs.size(); ++i) {
        if (matchesFilter(m_songs.at(i).singerName))
            m_visible.append(i);
    }
}

bool SongQueueModel::matchesFilter(const QString &singer) const
{
    return m_selectedSingerName.isEmpty() || singer == m_selectedSingerName;
}

int SongQueueModel::sourceIndex(int visibleRow) const
{
    if (visibleRow < 0 || visibleRow >= m_visible.size())
        return -1;
    return m_visible.at(visibleRow);
}

int SongQueueModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_visible.size();
}

QVariant SongQueueModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visible.size())
        return QVariant();

    const SongItem &song = m_songs.at(m_visible.at(index.row()));
    
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
    case SourceRole:
        return song.source;
    case KeyShiftRole:
        return song.keyShift;
    case PortalRequestIdRole:
        return song.portalRequestId;
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
    roles[SourceRole] = "source";
    roles[KeyShiftRole] = "keyShift";
    roles[PortalRequestIdRole] = "portalRequestId";
    return roles;
}

void SongQueueModel::setDatabaseManager(DatabaseManager *databaseManager)
{
    m_databaseManager = databaseManager;
    if (!m_databaseManager)
        return;

    const QVariantList rows = m_databaseManager->loadQueue();
    if (rows.isEmpty())
        return;

    beginResetModel();
    m_songs.clear();
    for (const QVariant &entry : rows) {
        const QVariantMap item = entry.toMap();
        SongItem song;
        song.id = QUuid::createUuid().toString();
        song.singerName = item.value(QStringLiteral("singer")).toString();
        song.songTitle = item.value(QStringLiteral("title")).toString();
        song.artist = item.value(QStringLiteral("artist")).toString();
        song.source = item.value(QStringLiteral("source")).toString();
        song.filePath = item.value(QStringLiteral("filePath")).toString();
        song.duration = item.value(QStringLiteral("duration")).toInt();
        song.isPlayed = item.value(QStringLiteral("isPlayed")).toBool();
        song.keyShift = item.value(QStringLiteral("keyShift")).toInt();
        song.portalRequestId = item.value(QStringLiteral("portalRequestId")).toString();
        m_songs.append(song);
    }
    rebuildVisible();
    endResetModel();
}

void SongQueueModel::beginBulkInsert()
{
    m_bulkInsert = true;
    m_persistPending = false;
}

void SongQueueModel::endBulkInsert()
{
    if (!m_bulkInsert)
        return;

    m_bulkInsert = false;
    if (m_persistPending) {
        m_persistPending = false;
        persist();
    }
}

void SongQueueModel::persist()
{
    if (!m_databaseManager)
        return;

    // Deferred while a bulk insert is running, so adding many songs is a single
    // database write instead of one write per song.
    if (m_bulkInsert) {
        m_persistPending = true;
        return;
    }

    QVariantList list;
    for (const SongItem &song : m_songs) {
        QVariantMap item;
        item[QStringLiteral("singer")] = song.singerName;
        item[QStringLiteral("title")] = song.songTitle;
        item[QStringLiteral("artist")] = song.artist;
        item[QStringLiteral("source")] = song.source;
        item[QStringLiteral("filePath")] = song.filePath;
        item[QStringLiteral("duration")] = song.duration;
        item[QStringLiteral("isPlayed")] = song.isPlayed;
        item[QStringLiteral("keyShift")] = song.keyShift;
        item[QStringLiteral("portalRequestId")] = song.portalRequestId;
        list.append(item);
    }
    m_databaseManager->saveQueue(list);
}

void SongQueueModel::addSong(const QString &singer, const QString &title, const QString &artist,
                             const QString &path, int duration, const QString &source,
                             const QString &portalRequestId)
{
    SongItem newSong;
    newSong.id = QUuid::createUuid().toString();
    newSong.singerName = singer;
    newSong.songTitle = title;
    newSong.artist = artist;
    newSong.source = source;
    newSong.filePath = path;
    newSong.duration = duration;
    newSong.isPlayed = false;
    newSong.portalRequestId = portalRequestId;

    if (matchesFilter(singer)) {
        const int insertAt = m_visible.size();
        beginInsertRows(QModelIndex(), insertAt, insertAt);
        m_songs.append(newSong);
        rebuildVisible();
        endInsertRows();
    } else {
        m_songs.append(newSong);
        rebuildVisible();
    }
    persist();
}

void SongQueueModel::removeSong(int index)
{
    if (index < 0 || index >= m_visible.size())
        return;

    beginRemoveRows(QModelIndex(), index, index);
    m_songs.removeAt(m_visible.at(index));
    rebuildVisible();
    endRemoveRows();
    persist();
}

void SongQueueModel::moveSong(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_visible.size() ||
        toIndex < 0 || toIndex >= m_visible.size() ||
        fromIndex == toIndex)
        return;

    const SongItem item = m_songs.at(m_visible.at(fromIndex));
    m_songs.removeAt(m_visible.at(fromIndex));
    rebuildVisible();
    const int target = (toIndex < m_visible.size()) ? m_visible.at(toIndex) : m_songs.size();
    m_songs.insert(qBound(0, target, m_songs.size()), item);

    beginResetModel();
    rebuildVisible();
    endResetModel();
    persist();
}

void SongQueueModel::markAsPlayed(int index, bool played)
{
    const int source = sourceIndex(index);
    if (source < 0)
        return;

    m_songs[source].isPlayed = played;

    const QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {IsPlayedRole});

    if (!m_songs.at(source).portalRequestId.isEmpty())
        emit portalPlayedChanged(m_songs.at(source).portalRequestId, played);

    persist();
}

void SongQueueModel::setKeyShift(int index, int semitones)
{
    const int source = sourceIndex(index);
    if (source < 0)
        return;

    const int clamped = qBound(-6, semitones, 6);
    if (m_songs[source].keyShift == clamped)
        return;

    m_songs[source].keyShift = clamped;

    const QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {KeyShiftRole});
    persist();
}

int SongQueueModel::keyShiftAt(int index) const
{
    const int source = sourceIndex(index);
    if (source < 0)
        return 0;
    return m_songs[source].keyShift;
}

QString SongQueueModel::filePathAt(int index) const
{
    const int source = sourceIndex(index);
    if (source < 0)
        return QString();
    return m_songs[source].filePath;
}

QString SongQueueModel::playablePathAt(int index) const
{
    const int source = sourceIndex(index);
    if (source < 0)
        return QString();

    const SongItem &song = m_songs.at(source);
    if (!song.filePath.isEmpty() && QFile::exists(song.filePath))
        return withPlayableCompanion(song.filePath);

    // The stored path belongs to another machine. Look the song up in the local
    // library by artist and title, the same way the OpenKJ import matches, and
    // hand back a path that is actually here.
    if (!m_databaseManager)
        return song.filePath;

    const QVariantList candidates = m_databaseManager->searchSongs(song.songTitle);
    for (const QVariant &candidate : std::as_const(candidates)) {
        const QVariantMap row = candidate.toMap();
        if (row.value(QStringLiteral("artist")).toString().compare(song.artist, Qt::CaseInsensitive) != 0)
            continue;
        if (row.value(QStringLiteral("title")).toString().compare(song.songTitle, Qt::CaseInsensitive) != 0)
            continue;

        const QString path = row.value(QStringLiteral("filePath")).toString();
        if (!path.isEmpty() && QFile::exists(path))
            return withPlayableCompanion(path);
    }

    // Nothing to substitute; hand back what was stored and let the deck report
    // that the file is missing.
    return withPlayableCompanion(song.filePath);
}

void SongQueueModel::clearQueue()
{
    if (m_visible.isEmpty())
        return;

    beginResetModel();
    if (m_selectedSingerName.isEmpty()) {
        m_songs.clear();
    } else {
        QList<SongItem> keep;
        for (const SongItem &song : m_songs) {
            if (song.singerName != m_selectedSingerName)
                keep.append(song);
        }
        m_songs = keep;
    }
    rebuildVisible();
    endResetModel();
    persist();
}

bool SongQueueModel::hasUnplayedFor(const QString &singer) const
{
    for (const SongItem &song : m_songs) {
        if (song.singerName == singer && !song.isPlayed)
            return true;
    }
    return false;
}

QVariantMap SongQueueModel::firstUnplayedFor(const QString &singer) const
{
    for (const SongItem &song : m_songs) {
        if (song.singerName != singer || song.isPlayed)
            continue;
        QVariantMap map;
        map[QStringLiteral("title")] = song.songTitle;
        map[QStringLiteral("artist")] = song.artist;
        map[QStringLiteral("filePath")] = song.filePath;
        map[QStringLiteral("duration")] = song.duration;
        map[QStringLiteral("keyShift")] = song.keyShift;
        map[QStringLiteral("source")] = song.source;
        return map;
    }
    return QVariantMap();
}

void SongQueueModel::markPlayedByPath(const QString &path, const QString &singer)
{
    if (path.isEmpty())
        return;

    for (int i = 0; i < m_songs.size(); ++i) {
        if (m_songs.at(i).filePath != path || m_songs.at(i).isPlayed)
            continue;
        if (!singer.isEmpty() && m_songs.at(i).singerName != singer)
            continue;

        m_songs[i].isPlayed = true;
        const int visibleRow = m_visible.indexOf(i);
        if (visibleRow >= 0) {
            const QModelIndex modelIndex = createIndex(visibleRow, 0);
            emit dataChanged(modelIndex, modelIndex, {IsPlayedRole});
        }
        if (!m_songs.at(i).portalRequestId.isEmpty())
            emit portalPlayedChanged(m_songs.at(i).portalRequestId, true);
        // Only write when something actually changed: saveQueue() rewrites the
        // whole table, and this is called on every song end - including songs
        // that were never queued.
        persist();
        return;
    }
}

void SongQueueModel::setSelectedSingerName(const QString &name)
{
    if (m_selectedSingerName == name)
        return;

    m_selectedSingerName = name;

    beginResetModel();
    rebuildVisible();
    endResetModel();

    emit selectedSingerNameChanged();
}

void SongQueueModel::renameSinger(const QString &from, const QString &to)
{
    if (from.isEmpty() || to.isEmpty() || from == to)
        return;

    bool changed = false;
    for (SongItem &item : m_songs) {
        if (item.singerName == from) {
            item.singerName = to;
            changed = true;
        }
    }
    if (!changed)
        return;

    if (m_selectedSingerName == from)
        m_selectedSingerName = to;

    beginResetModel();
    rebuildVisible();
    endResetModel();

    emit selectedSingerNameChanged();
    persist();
}

void SongQueueModel::applyPortalPlayed(const QString &portalRequestId, bool played)
{
    if (portalRequestId.isEmpty())
        return;

    bool changed = false;
    for (SongItem &song : m_songs) {
        if (song.portalRequestId != portalRequestId)
            continue;
        if (song.isPlayed != played) {
            song.isPlayed = played;
            changed = true;
        }
    }
    if (!changed)
        return;

    beginResetModel();
    rebuildVisible();
    endResetModel();

    emit portalPlayedChanged(portalRequestId, played);
    persist();
}

void SongQueueModel::toggleSort(int column)
{
    if (column < 1 || column > 5)
        return;

    if (m_sortColumn == column)
        m_sortAscending = !m_sortAscending;
    else {
        m_sortColumn = column;
        m_sortAscending = true;
    }

    beginResetModel();
    applySort();
    endResetModel();

    emit sortChanged();
    persist();
}

void SongQueueModel::applySort()
{
    if (m_sortColumn < 1 || m_sortColumn > 5)
        return;

    if (m_visible.size() < 2)
        return;

    const int column = m_sortColumn;
    const bool ascending = m_sortAscending;

    // Sort only the rows the user can see, leaving other singers' slots alone.
    QList<int> order = m_visible;
    std::stable_sort(order.begin(), order.end(), [this, column, ascending](int a, int b) {
        const SongItem &sa = m_songs.at(a);
        const SongItem &sb = m_songs.at(b);

        int cmp = 0;
        switch (column) {
        case 1: cmp = QString::compare(sa.singerName, sb.singerName, Qt::CaseInsensitive); break;
        case 2: cmp = QString::compare(sa.songTitle, sb.songTitle, Qt::CaseInsensitive); break;
        case 3: cmp = QString::compare(sa.artist, sb.artist, Qt::CaseInsensitive); break;
        case 4: cmp = QString::compare(sa.source, sb.source, Qt::CaseInsensitive); break;
        case 5: cmp = (sa.duration < sb.duration) ? -1 : (sa.duration > sb.duration ? 1 : 0); break;
        default: return false;
        }
        return ascending ? (cmp < 0) : (cmp > 0);
    });

    QList<SongItem> picked;
    picked.reserve(order.size());
    for (int source : order)
        picked.append(m_songs.at(source));
    for (int i = 0; i < m_visible.size(); ++i)
        m_songs[m_visible.at(i)] = picked.at(i);

    rebuildVisible();
}
