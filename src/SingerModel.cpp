#include "SingerModel.h"
#include <QDebug>

SingerModel::SingerModel(QObject *parent) : QAbstractListModel(parent), m_nextId(1)
{
    addSinger("Test Singer 1");
    addSinger("Test Singer 2");
}

int SingerModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_singers.size();
}

QVariant SingerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_singers.size())
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
    roles[IdRole] = "singerId";
    roles[NameRole] = "singerName";
    roles[StatusRole] = "singerStatus";
    return roles;
}

void SingerModel::addSinger(const QString &name)
{
    beginInsertRows(QModelIndex(), m_singers.size(), m_singers.size());
    
    Singer newSinger;
    newSinger.id = m_nextId++;
    newSinger.name = name;
    newSinger.status = "Active";
    
    m_singers.append(newSinger);
    
    endInsertRows();
}

void SingerModel::removeSinger(int index)
{
    if (index < 0 || index >= m_singers.size())
        return;

    beginRemoveRows(QModelIndex(), index, index);
    m_singers.remove(index);
    endRemoveRows();
}

void SingerModel::moveSinger(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_singers.size() ||
        toIndex < 0 || toIndex >= m_singers.size() ||
        fromIndex == toIndex)
        return;

    beginMoveRows(QModelIndex(), fromIndex, fromIndex, QModelIndex(), toIndex > fromIndex ? toIndex + 1 : toIndex);
    m_singers.move(fromIndex, toIndex);
    endMoveRows();
}

void SingerModel::toggleSingerStatus(int index)
{
    if (index < 0 || index >= m_singers.size())
        return;

    Singer &singer = m_singers[index];
    singer.status = (singer.status == "Active") ? "Inactive" : "Active";
    emit dataChanged(index, index);
}
