#include "SongDatabaseModel.h"
#include <QDebug>
#include <algorithm>

SongDatabaseModel::SongDatabaseModel(DatabaseManager *databaseManager, bool background,
                                     QObject *parent)
    : QAbstractTableModel(parent), m_databaseManager(databaseManager), m_background(background)
{
    if (m_databaseManager) {
        refreshData();
    }
}

SongDatabaseModel::~SongDatabaseModel()
{
}

int SongDatabaseModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_cachedData.size(); // m_cachedData is a list of song maps
}

int SongDatabaseModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 6; // id, artist, title, file_path, duration, source
}

QVariant SongDatabaseModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_cachedData.size())
        return QVariant();

    const QVariantMap song = m_cachedData.at(index.row()).toMap();

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case 0: return song.value(QStringLiteral("id"));
        case 1: return song.value(QStringLiteral("artist"));
        case 2: return song.value(QStringLiteral("title"));
        case 3: return song.value(QStringLiteral("filePath"));
        case 4: return song.value(QStringLiteral("duration"));
        case 5: return song.value(QStringLiteral("source"));
        default: return QVariant();
        }
    }

    // Custom roles for QML
    switch (role) {
    case IdRole:       return song.value(QStringLiteral("id"));
    case ArtistRole:   return song.value(QStringLiteral("artist"));
    case TitleRole:    return song.value(QStringLiteral("title"));
    case FilePathRole: return song.value(QStringLiteral("filePath"));
    case DurationRole: return song.value(QStringLiteral("duration"));
    case IsDeletedRole: return song.value(QStringLiteral("isDeleted"));
    case SourceRole: return song.value(QStringLiteral("source"));
    default:           return QVariant();
    }
}

QVariant SongDatabaseModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            switch (section) {
                case 0: return "ID";
                case 1: return "Artist";
                case 2: return "Title";
                case 3: return "File Path";
                case 4: return "Duration";
                case 5: return "Source";
                default: return QVariant();
            }
        }
    }

    return QVariant();
}

bool SongDatabaseModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole)
        return false;

    // Primary key is not editable
    if (index.column() == 0)
        return false;

    const int row = index.row();
    if (row < 0 || row >= m_cachedData.size())
        return false;

    QVariantMap song = m_cachedData.at(row).toMap();

    switch (index.column()) {
    case 1: song[QStringLiteral("artist")] = value; break;
    case 2: song[QStringLiteral("title")] = value; break;
    case 3: song[QStringLiteral("filePath")] = value; break;
    case 4: song[QStringLiteral("duration")] = value; break;
    case 5: song[QStringLiteral("source")] = value; break;
    default: return false;
    }

    m_cachedData[row] = song;

    const bool ok = m_databaseManager->updateSong(
        song.value(QStringLiteral("id")).toInt(),
        song.value(QStringLiteral("artist")).toString(),
        song.value(QStringLiteral("title")).toString(),
        song.value(QStringLiteral("filePath")).toString(),
        song.value(QStringLiteral("duration")).toInt(),
        song.value(QStringLiteral("source")).toString());

    if (ok)
        emit dataChanged(index, index, {role});

    return ok;
}

Qt::ItemFlags SongDatabaseModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags flags = Qt::ItemIsSelectable;

    // Allow editing for artist, title, file_path, duration
    if (index.column() > 0) {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

QHash<int, QByteArray> SongDatabaseModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[ArtistRole] = "artist";
    roles[TitleRole] = "title";
    roles[FilePathRole] = "filePath";
    roles[DurationRole] = "duration";
    roles[IsDeletedRole] = "isDeleted";
    roles[SourceRole] = "source";
    roles[Qt::DisplayRole] = "display";
    return roles;
}

void SongDatabaseModel::setFilter(const QString &filter)
{
    if (m_filter != filter) {
        m_filter = filter;
        refreshData();
    }
}

void SongDatabaseModel::refreshData()
{
    beginResetModel();

    if (m_background) {
        // The background collection has no SQL search of its own, so the filter
        // is applied in memory - a few hundred rows at most.
        m_cachedData = m_databaseManager->getAllBackgroundSongs(m_includeDeleted);

        if (!m_filter.isEmpty()) {
            const QString needle = m_filter.toLower();
            QVariantList filtered;
            filtered.reserve(m_cachedData.size());
            for (const QVariant &entry : std::as_const(m_cachedData)) {
                const QVariantMap row = entry.toMap();
                const QString haystack = (row.value(QStringLiteral("artist")).toString()
                                          + QLatin1Char(' ')
                                          + row.value(QStringLiteral("title")).toString()
                                          + QLatin1Char(' ')
                                          + row.value(QStringLiteral("source")).toString())
                                             .toLower();
                if (haystack.contains(needle))
                    filtered.append(entry);
            }
            m_cachedData = filtered;
        }
    } else if (m_filter.isEmpty()) {
        m_cachedData = m_databaseManager->getAllSongs(m_includeDeleted);
    } else {
        m_cachedData = m_databaseManager->searchSongs(m_filter, m_includeDeleted);
    }

    applySort();

    endResetModel();
}

QVariantMap SongDatabaseModel::get(int row) const
{
    if (row < 0 || row >= m_cachedData.size())
        return QVariantMap();
    return m_cachedData.at(row).toMap();
}

void SongDatabaseModel::toggleSort(int column)
{
    if (column != 1 && column != 2 && column != 3)
        return;

    if (m_sortColumn == column) {
        m_sortAscending = !m_sortAscending;
    } else {
        m_sortColumn = column;
        m_sortAscending = true;
    }

    refreshData();
    emit sortChanged();
}

void SongDatabaseModel::applySort()
{
    if (m_cachedData.size() < 2)
        return;

    const QString key = (m_sortColumn == 2) ? QStringLiteral("title")
                                            : (m_sortColumn == 3 ? QStringLiteral("source") : QStringLiteral("artist"));
    const bool asc = m_sortAscending;

    // Decorate, sort, undecorate: pull each sort key out once. The comparator
    // used to call toMap().value(key) on both operands of every comparison,
    // which is O(n log n) map copies - and this runs on every keystroke in the
    // search box (setFilter() -> refreshData() -> applySort()).
    struct SortEntry {
        QString key;
        QVariant row;
    };

    QList<SortEntry> entries;
    entries.reserve(m_cachedData.size());
    for (const QVariant &row : std::as_const(m_cachedData))
        entries.append(SortEntry{row.toMap().value(key).toString(), row});

    std::stable_sort(entries.begin(), entries.end(),
                     [asc](const SortEntry &a, const SortEntry &b) {
                         const int cmp = QString::compare(a.key, b.key, Qt::CaseInsensitive);
                         return asc ? (cmp < 0) : (cmp > 0);
                     });

    m_cachedData.clear();
    m_cachedData.reserve(entries.size());
    for (const SortEntry &entry : std::as_const(entries))
        m_cachedData.append(entry.row);
}

bool SongDatabaseModel::addSong(const QString &artist, const QString &title, const QString &filePath, int duration)
{
    bool success = m_databaseManager->addSong(artist, title, filePath, duration);
    if (success) {
        refreshData();
    }
    return success;
}

bool SongDatabaseModel::removeSong(int id)
{
    bool success = m_databaseManager->removeSong(id);
    if (success) {
        refreshData();
    }
    return success;
}

bool SongDatabaseModel::restoreSong(int id)
{
    bool success = m_databaseManager->restoreSong(id);
    if (success) {
        refreshData();
    }
    return success;
}

bool SongDatabaseModel::purgeSong(int id)
{
    bool success = m_databaseManager->purgeSong(id);
    if (success) {
        refreshData();
    }
    return success;
}

void SongDatabaseModel::setIncludeDeleted(bool include)
{
    if (m_includeDeleted == include)
        return;

    m_includeDeleted = include;
    refreshData();
    emit includeDeletedChanged();
}

bool SongDatabaseModel::updateSong(int id, const QString &artist, const QString &title, const QString &filePath, int duration, const QString &source)
{
    return m_databaseManager->updateSong(id, artist, title, filePath, duration, source);
}

QVariantList SongDatabaseModel::searchSongs(const QString &query)
{
    return m_databaseManager->searchSongs(query);
}

void SongDatabaseModel::resetQuery()
{
    // Reset the query if needed
}