#ifndef SINGERMODEL_H
#define SINGERMODEL_H

#include <QAbstractListModel>
#include <QObject>
#include <QString>

class SingerModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum SingerRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        StatusRole
    };

    explicit SingerModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addSinger(const QString &name);
    Q_INVOKABLE void removeSinger(int index);
    Q_INVOKABLE void moveSinger(int fromIndex, int toIndex);
    Q_INVOKABLE void toggleSingerStatus(int index);

private:
    struct Singer {
        int id;
        QString name;
        QString status;
    };
    QList<Singer> m_singers;
    int m_nextId;
};

#endif // SINGERMODEL_H
