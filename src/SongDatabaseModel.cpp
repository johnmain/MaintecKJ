#include "SongDatabaseModel.h"
#include <QDebug>

SongDatabaseModel::SongDatabaseModel(DatabaseManager *databaseManager, QObject *parent)
    : QAbstractTableModel(parent), m_databaseManager(databaseManager)
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
    return m_cachedData.size() / 5; // Each song has 5 fields
}

int SongDatabaseModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 5; // id, artist, title, file_path, duration
}

QVariant SongDatabaseModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return QVariant();

    int row = index.row();
    int col = index.column();
    int dataIndex = row * 5 + col;

    if (dataIndex >= m_cachedData.size())
        return QVariant();

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        return m_cachedData[dataIndex];
    }

    // Custom roles for QML
    if (role == IdRole) {
        return m_cachedData[row * 5];
    } else if (role == ArtistRole) {
        return m_cachedData[row * 5 + 1];
    } else if (role == TitleRole) {
        return m_cachedData[row * 5 + 2];
    } else if (role == FilePathRole) {
        return m_cachedData[row * 5 + 3];
    } else if (role == DurationRole) {
        return m_cachedData[row * 5 + 4];
    }

    return QVariant();
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

    int row = index.row();
    int col = index.column();
    int dataIndex = row * 5 + col;

    if (dataIndex >= m_cachedData.size())
        return false;

    m_cachedData[dataIndex] = value;

    // If it's an ID field, don't update (primary key)
    if (col == 0) {
        return true;
    }

    // Get the actual song ID
    int songId = m_cachedData[row * 5].toInt();
    if (songId <= 0)
        return false;

    // Map column to database field
    QString fieldName;
    switch (col) {
        case 1: fieldName = "artist"; break;
        case 2: fieldName = "title"; break;
        case 3: fieldName = "file_path"; break;
        case 4: fieldName = "duration"; break;
        default: return false;
    }

    return m_databaseManager->updateSong(
        songId,
        m_cachedData[row * 5 + 1].toString(),
        m_cachedData[row * 5 + 2].toString(),
        m_cachedData[row * 5 + 3].toString(),
        m_cachedData[row * 5 + 4].toInt()
    );
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

    if (m_filter.isEmpty()) {
        m_cachedData = m_databaseManager->getAllSongs();
    } else {
        m_cachedData = m_databaseManager->searchSongs(m_filter);
    }

    endResetModel();
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

bool SongDatabaseModel::updateSong(int id, const QString &artist, const QString &title, const QString &filePath, int duration)
{
    return m_databaseManager->updateSong(id, artist, title, filePath, duration);
}

void SongDatabaseModel::createTestData()
{
    m_databaseManager->createTestData();
    refreshData();
}

QVariantList SongDatabaseModel::searchSongs(const QString &query)
{
    return m_databaseManager->searchSongs(query);
}

void SongDatabaseModel::resetQuery()
{
    // Reset the query if needed
}