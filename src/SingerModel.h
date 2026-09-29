#ifndef SINGERMODEL_H
#define SINGERMODEL_H

#include <QAbstractListModel>
#include <QObject>
#include <QString>
#include <QStringList>

class DatabaseManager;

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
    void setDatabaseManager(DatabaseManager *databaseManager);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addSinger(const QString &name);
    Q_INVOKABLE void removeSinger(int index);
    Q_INVOKABLE void moveSinger(int fromIndex, int toIndex);
    Q_INVOKABLE void toggleSingerStatus(int index);
    Q_INVOKABLE void clearAllSingers();  // FIXED: Added this declaration

    // Rotation helpers.
    Q_INVOKABLE int indexOfName(const QString &name) const;
    Q_INVOKABLE QString nameAt(int index) const;
    Q_INVOKABLE QString statusAt(int index) const;
    Q_INVOKABLE void moveToBottom(int index);
    Q_INVOKABLE void setStatus(int index, const QString &status);

    // Rotation order names, for the web-requests singer picker.
    Q_INVOKABLE QStringList singerNames() const;
    // Renames a singer. Emits singerRenamed so their queue rows follow.
    Q_INVOKABLE bool renameSinger(int index, const QString &newName);

signals:
    void singerRenamed(const QString &from, const QString &to);

private:
    struct Singer {
        int id;
        QString name;
        QString status;
    };
    QList<Singer> m_singers;
    int m_nextId;

    void persist();
    DatabaseManager *m_databaseManager = nullptr;
};

#endif // SINGERMODEL_H
