#include "BackgroundPlaylistModel.h"

#include "DatabaseManager.h"

#include <QRandomGenerator>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>

#include <utility>

BackgroundPlaylistModel::BackgroundPlaylistModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void BackgroundPlaylistModel::setDatabaseManager(DatabaseManager *databaseManager)
{
    m_databaseManager = databaseManager;
    if (!m_databaseManager)
        return;

    beginResetModel();

    m_entries.clear();
    const QVariantList stored = m_databaseManager->loadBackgroundPlaylist();
    for (const QVariant &value : stored) {
        const QVariantMap row = value.toMap();

        Entry entry;
        entry.title = row.value(QStringLiteral("title")).toString();
        entry.artist = row.value(QStringLiteral("artist")).toString();
        entry.filePath = row.value(QStringLiteral("filePath")).toString();
        entry.duration = row.value(QStringLiteral("duration")).toInt();
        entry.source = row.value(QStringLiteral("source")).toString();
        m_entries.append(entry);
    }

    m_currentIndex = -1;

    endResetModel();

    emit countChanged();
    emit currentIndexChanged();
}

int BackgroundPlaylistModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_entries.size());
}

QVariant BackgroundPlaylistModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(m_entries.size())) {
        return QVariant();
    }

    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case TitleRole:
        return entry.title;
    case ArtistRole:
        return entry.artist;
    case FilePathRole:
        return entry.filePath;
    case DurationRole:
        return entry.duration;
    case SourceRole:
        return entry.source;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> BackgroundPlaylistModel::roleNames() const
{
    return {
        { TitleRole, "title" },
        { ArtistRole, "artist" },
        { FilePathRole, "filePath" },
        { DurationRole, "duration" },
        { SourceRole, "source" }
    };
}

void BackgroundPlaylistModel::addSong(const QString &artist, const QString &title,
                                      const QString &path, int duration,
                                      const QString &source)
{
    Entry entry;
    entry.artist = artist;
    entry.title = title;
    entry.filePath = path;
    entry.duration = duration;
    entry.source = source;

    const int row = static_cast<int>(m_entries.size());
    beginInsertRows(QModelIndex(), row, row);
    m_entries.append(entry);
    endInsertRows();

    persist();
    emit countChanged();
    emit playlistChanged();
}

void BackgroundPlaylistModel::removeSong(int index)
{
    if (index < 0 || index >= static_cast<int>(m_entries.size()))
        return;

    beginRemoveRows(QModelIndex(), index, index);
    m_entries.removeAt(index);
    endRemoveRows();

    // Keep the deck pointing at the same song rather than the same row number.
    if (m_currentIndex == index)
        setCurrentIndex(-1);
    else if (m_currentIndex > index)
        setCurrentIndex(m_currentIndex - 1);

    persist();
    emit countChanged();
    emit playlistChanged();
}

void BackgroundPlaylistModel::moveSong(int fromIndex, int toIndex)
{
    const int rows = static_cast<int>(m_entries.size());
    if (fromIndex < 0 || fromIndex >= rows || toIndex < 0 || toIndex >= rows
        || fromIndex == toIndex) {
        return;
    }

    if (!beginMoveRows(QModelIndex(), fromIndex, fromIndex, QModelIndex(),
                       toIndex > fromIndex ? toIndex + 1 : toIndex)) {
        return;
    }
    m_entries.move(fromIndex, toIndex);
    endMoveRows();

    if (m_currentIndex >= 0) {
        if (m_currentIndex == fromIndex)
            setCurrentIndex(toIndex);
        else if (fromIndex < m_currentIndex && toIndex >= m_currentIndex)
            setCurrentIndex(m_currentIndex - 1);
        else if (fromIndex > m_currentIndex && toIndex <= m_currentIndex)
            setCurrentIndex(m_currentIndex + 1);
    }

    persist();
    emit playlistChanged();
}

void BackgroundPlaylistModel::clearPlaylist()
{
    if (m_entries.isEmpty())
        return;

    beginResetModel();
    m_entries.clear();
    endResetModel();

    setCurrentIndex(-1);

    persist();
    emit countChanged();
    emit playlistChanged();
}

int BackgroundPlaylistModel::addAllFromLibrary()
{
    if (!m_databaseManager)
        return 0;

    QSet<QString> alreadyOnTheList;
    for (const Entry &entry : std::as_const(m_entries))
        alreadyOnTheList.insert(entry.filePath);

    const QVariantList library = m_databaseManager->getAllBackgroundSongs(false);

    QList<Entry> additions;
    for (const QVariant &value : library) {
        const QVariantMap row = value.toMap();

        Entry entry;
        entry.title = row.value(QStringLiteral("title")).toString();
        entry.artist = row.value(QStringLiteral("artist")).toString();
        entry.filePath = row.value(QStringLiteral("filePath")).toString();
        entry.duration = row.value(QStringLiteral("duration")).toInt();
        entry.source = row.value(QStringLiteral("source")).toString();

        if (entry.filePath.isEmpty() || alreadyOnTheList.contains(entry.filePath))
            continue;

        alreadyOnTheList.insert(entry.filePath);
        additions.append(entry);
    }

    if (additions.isEmpty())
        return 0;

    const int first = static_cast<int>(m_entries.size());
    beginInsertRows(QModelIndex(), first, first + static_cast<int>(additions.size()) - 1);
    m_entries.append(additions);
    endInsertRows();

    persist();
    emit countChanged();
    emit playlistChanged();

    return static_cast<int>(additions.size());
}

void BackgroundPlaylistModel::shuffle()
{
    if (m_entries.size() < 2)
        return;

    const bool haveCurrent = m_currentIndex >= 0
                             && m_currentIndex < static_cast<int>(m_entries.size());
    const QString currentPath = haveCurrent ? m_entries.at(m_currentIndex).filePath : QString();

    beginResetModel();

    // Fisher-Yates. A single pass per press - the list is never reshuffled on
    // its own, otherwise the running order would change under the operator.
    for (qsizetype i = m_entries.size() - 1; i > 0; --i) {
        const quint32 bound = static_cast<quint32>(i + 1);
        const qsizetype j = static_cast<qsizetype>(QRandomGenerator::global()->bounded(bound));
        m_entries.swapItemsAt(i, j);
    }

    if (haveCurrent) {
        m_currentIndex = -1;
        for (int i = 0; i < static_cast<int>(m_entries.size()); ++i) {
            if (m_entries.at(i).filePath == currentPath) {
                m_currentIndex = i;
                break;
            }
        }
    }

    endResetModel();

    persist();
    emit playlistChanged();
    emit currentIndexChanged();
}

QVariantMap BackgroundPlaylistModel::songAt(int index) const
{
    QVariantMap song;
    if (index < 0 || index >= static_cast<int>(m_entries.size()))
        return song;

    const Entry &entry = m_entries.at(index);
    song[QStringLiteral("title")] = entry.title;
    song[QStringLiteral("artist")] = entry.artist;
    song[QStringLiteral("filePath")] = entry.filePath;
    song[QStringLiteral("duration")] = entry.duration;
    song[QStringLiteral("source")] = entry.source;
    return song;
}

int BackgroundPlaylistModel::nextIndex() const
{
    if (m_entries.isEmpty())
        return -1;
    if (m_currentIndex < 0)
        return 0;
    if (m_currentIndex + 1 < static_cast<int>(m_entries.size()))
        return m_currentIndex + 1;
    return -1;
}

void BackgroundPlaylistModel::setCurrentIndex(int index)
{
    if (index < -1 || index >= static_cast<int>(m_entries.size()))
        index = -1;
    if (m_currentIndex == index)
        return;

    m_currentIndex = index;
    emit currentIndexChanged();
}

void BackgroundPlaylistModel::persist()
{
    if (!m_databaseManager)
        return;

    QVariantList items;
    items.reserve(m_entries.size());
    for (const Entry &entry : std::as_const(m_entries)) {
        QVariantMap item;
        item[QStringLiteral("title")] = entry.title;
        item[QStringLiteral("artist")] = entry.artist;
        item[QStringLiteral("filePath")] = entry.filePath;
        item[QStringLiteral("duration")] = entry.duration;
        item[QStringLiteral("source")] = entry.source;
        items.append(item);
    }

    m_databaseManager->saveBackgroundPlaylist(items);
}
