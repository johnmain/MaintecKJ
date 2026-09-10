#ifndef SONGDATABASEMODEL_H
#define SONGDATABASEMODEL_H

#include <QAbstractTableModel>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>
#include "DatabaseManager.h"

class SongDatabaseModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        ArtistRole,
        TitleRole,
        FilePathRole,
        DurationRole
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

    // Custom methods
    void setFilter(const QString &filter);
    void refreshData();
    bool addSong(const QString &artist, const QString &title, const QString &filePath, int duration = 0);
    bool removeSong(int id);
    bool updateSong(int id, const QString &artist, const QString &title, const QString &filePath, int duration);

    Q_INVOKABLE void createTestData();
    Q_INVOKABLE QVariantList searchSongs(const QString &query);

private:
    DatabaseManager *m_databaseManager;
    QSqlQuery m_query;
    QString m_filter;
    QVariantList m_cachedData;

    void resetQuery();
};

#endif // SONGDATABASEMODEL_H