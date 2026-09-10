#ifndef SONGDATABASEMODEL_H
#define SONGDATABASEMODEL_H

#include <QAbstractTableModel>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include "DatabaseManager.h"

class SongDatabaseModel : public QAbstractTableModel
{
    Q_OBJECT
    Q_PROPERTY(int sortColumn READ sortColumn NOTIFY sortChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending NOTIFY sortChanged)
    Q_PROPERTY(bool includeDeleted READ includeDeleted NOTIFY includeDeletedChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        ArtistRole,
        TitleRole,
        FilePathRole,
        DurationRole,
        IsDeletedRole,
        SourceRole
    };

    explicit SongDatabaseModel(DatabaseManager *databaseManager, QObject *parent = nullptr);
    ~SongDatabaseModel();

    // QAbstractTableModel overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Custom methods
    Q_INVOKABLE void setFilter(const QString &filter);
    Q_INVOKABLE void clearFilter() { setFilter(QString()); }
    Q_INVOKABLE void refreshData();
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE bool removeSong(int id);
    Q_INVOKABLE bool restoreSong(int id);
    Q_INVOKABLE bool purgeSong(int id);
    Q_INVOKABLE void toggleSort(int column);
    Q_INVOKABLE void setIncludeDeleted(bool include);
    bool addSong(const QString &artist, const QString &title, const QString &filePath, int duration = 0);
    bool updateSong(int id, const QString &artist, const QString &title, const QString &filePath, int duration, const QString &source = QString());

    Q_INVOKABLE QVariantList searchSongs(const QString &query);

    int sortColumn() const { return m_sortColumn; }
    bool sortAscending() const { return m_sortAscending; }
    bool includeDeleted() const { return m_includeDeleted; }

signals:
    void sortChanged();
    void includeDeletedChanged();

private:
    DatabaseManager *m_databaseManager;
    QSqlQuery m_query;
    QString m_filter;
    QVariantList m_cachedData;
    int m_sortColumn = 1;      // 1 = artist, 2 = title
    bool m_sortAscending = true;
    bool m_includeDeleted = false;

    void resetQuery();
    void applySort();
};

#endif // SONGDATABASEMODEL_H