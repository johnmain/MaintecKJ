#include "SingerModel.h"
#include "DatabaseManager.h"

SingerModel::SingerModel(QObject *parent)
    : QAbstractListModel(parent), m_nextId(1)
{
}

int SingerModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_singers.size();
}

QVariant SingerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_singers.size())
        return QVariant();

    const Singer &singer = m_singers.at(index.row());

    switch (role) {
    case IdRole:
        return singer.id;
    case NameRole:
        return singer.name;
    case StatusRole:
        return singer.status;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> SingerModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[NameRole] = "singerName";
    roles[StatusRole] = "singerStatus";
    return roles;
}

void SingerModel::setDatabaseManager(DatabaseManager *databaseManager)
{
    m_databaseManager = databaseManager;
    if (!m_databaseManager)
        return;

    const QVariantList rows = m_databaseManager->loadSingers();
    if (rows.isEmpty())
        return;

    beginResetModel();
    m_singers.clear();
    for (const QVariant &entry : rows) {
        const QVariantMap item = entry.toMap();
        Singer singer;
        singer.id = item.value(QStringLiteral("id")).toInt();
        singer.name = item.value(QStringLiteral("name")).toString();
        singer.status = item.value(QStringLiteral("status")).toString();
        if (singer.status.isEmpty())
            singer.status = QStringLiteral("Active");
        m_singers.append(singer);
        m_nextId = qMax(m_nextId, singer.id + 1);
    }
    endResetModel();
}

void SingerModel::persist()
{
    if (!m_databaseManager)
        return;

    QVariantList list;
    for (const Singer &singer : m_singers) {
        QVariantMap item;
        item[QStringLiteral("id")] = singer.id;
        item[QStringLiteral("name")] = singer.name;
        item[QStringLiteral("status")] = singer.status;
        list.append(item);
    }
    m_databaseManager->saveSingers(list);
}

void SingerModel::addSinger(const QString &name)
{
    beginInsertRows(QModelIndex(), m_singers.size(), m_singers.size());
    
    Singer newSinger;
    // A monotonically increasing handle. This used to be an 8-hex-digit slice of
    // a UUID pushed through QString::toInt(), which silently returns 0 whenever
    // the slice exceeded INT_MAX - so about half of all singers were handed id 0.
    // The id is only an in-memory handle (persist() stores name/status/position),
    // so a counter is both simpler and collision free.
    newSinger.id = m_nextId++;
    newSinger.name = name;
    newSinger.status = "Active";
    
    m_singers.append(newSinger);
    endInsertRows();
    persist();
}

void SingerModel::removeSinger(int index)
{
    if (index < 0 || index >= m_singers.size())
        return;
        
    beginRemoveRows(QModelIndex(), index, index);
    m_singers.removeAt(index);
    endRemoveRows();
    persist();
}

void SingerModel::moveSinger(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_singers.size() || 
        toIndex < 0 || toIndex >= m_singers.size() || 
        fromIndex == toIndex)
        return;
        
    beginMoveRows(QModelIndex(), fromIndex, fromIndex, QModelIndex(), toIndex > fromIndex ? toIndex + 1 : toIndex);
    
    if (toIndex > fromIndex) {
        m_singers.move(fromIndex, toIndex);
    } else {
        m_singers.move(fromIndex, toIndex);
    }
    endMoveRows();
    persist();
}

void SingerModel::toggleSingerStatus(int index)
{
    if (index < 0 || index >= m_singers.size())
        return;
        
    m_singers[index].status = (m_singers[index].status == "Active") ? "Inactive" : "Active";
    
    QModelIndex modelIndex = this->index(index, 0);
    emit dataChanged(modelIndex, modelIndex, {StatusRole});
    persist();
}

int SingerModel::indexOfName(const QString &name) const
{
    for (int i = 0; i < m_singers.size(); ++i) {
        if (m_singers.at(i).name == name)
            return i;
    }
    return -1;
}

QString SingerModel::nameAt(int index) const
{
    if (index < 0 || index >= m_singers.size())
        return QString();
    return m_singers.at(index).name;
}

QString SingerModel::statusAt(int index) const
{
    if (index < 0 || index >= m_singers.size())
        return QString();
    return m_singers.at(index).status;
}

void SingerModel::moveToBottom(int index)
{
    if (index < 0 || index >= m_singers.size() || m_singers.size() < 2)
        return;

    const int last = m_singers.size() - 1;
    if (index == last)
        return;

    beginMoveRows(QModelIndex(), index, index, QModelIndex(), m_singers.size());
    m_singers.move(index, last);
    endMoveRows();
    persist();
}

void SingerModel::setStatus(int index, const QString &status)
{
    if (index < 0 || index >= m_singers.size())
        return;
    if (m_singers.at(index).status == status)
        return;

    m_singers[index].status = status;

    const QModelIndex modelIndex = this->index(index, 0);
    emit dataChanged(modelIndex, modelIndex, {StatusRole});
    persist();
}

QStringList SingerModel::singerNames() const
{
    QStringList names;
    names.reserve(m_singers.size());
    for (const Singer &singer : m_singers)
        names.append(singer.name);
    return names;
}

bool SingerModel::renameSinger(int index, const QString &newName)
{
    if (index < 0 || index >= m_singers.size())
        return false;

    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty() || m_singers.at(index).name == trimmed)
        return false;

    const QString oldName = m_singers.at(index).name;
    m_singers[index].name = trimmed;

    const QModelIndex modelIndex = this->index(index, 0);
    emit dataChanged(modelIndex, modelIndex, {NameRole});
    persist();
    emit singerRenamed(oldName, trimmed);
    return true;
}

void SingerModel::clearAllSingers()
{
    if (m_singers.isEmpty())
        return;
        
    beginRemoveRows(QModelIndex(), 0, m_singers.size() - 1);
    m_singers.clear();
    endRemoveRows();
    m_nextId = 1;
    persist();
}
