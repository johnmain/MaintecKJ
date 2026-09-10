#ifndef SONGQUEUEMODEL_H
#define SONGQUEUEMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QUuid>

class DatabaseManager;

struct SongItem {
    QString id;
    QString singerName;
    QString songTitle;
    QString artist;
    QString source;
    QString filePath;
    int duration = 0;        // seconds
    bool isPlayed = false;
    int keyShift = 0;   // semitones, -6..+6
};

class SongQueueModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString selectedSingerName READ selectedSingerName WRITE setSelectedSingerName NOTIFY selectedSingerNameChanged)
    Q_PROPERTY(int sortColumn READ sortColumn NOTIFY sortChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending NOTIFY sortChanged)

public:
    enum SongRoles {
        IdRole = Qt::UserRole + 1,
        SingerNameRole,
        SongTitleRole,
        ArtistRole,
        FilePathRole,
        DurationRole,
        IsPlayedRole,
        SourceRole,
        KeyShiftRole
    };

    explicit SongQueueModel(QObject *parent = nullptr);
    void setDatabaseManager(DatabaseManager *databaseManager);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addSong(const QString &singer, const QString &title, const QString &artist,
                             const QString &path, int duration, const QString &source = QString());
    Q_INVOKABLE void toggleSort(int column);
    Q_INVOKABLE void removeSong(int index);
    Q_INVOKABLE void moveSong(int fromIndex, int toIndex);
    Q_INVOKABLE void markAsPlayed(int index, bool played);
    Q_INVOKABLE void setKeyShift(int index, int semitones);
    Q_INVOKABLE int keyShiftAt(int index) const;
    // Path of the song on this visible row, so callers can tell whether it is
    // the one currently loaded in the deck.
    Q_INVOKABLE QString filePathAt(int index) const;
    Q_INVOKABLE void clearQueue();

    // Rotation helpers. The model only ever exposes the songs belonging to
    // selectedSingerName (or every song when it is empty).
    Q_INVOKABLE bool hasUnplayedFor(const QString &singer) const;
    Q_INVOKABLE QVariantMap firstUnplayedFor(const QString &singer) const;
    // Marks the first unplayed entry with this file path as played. When
    // `singer` is given, only that singer's entries are considered - two singers
    // may have queued the very same song, so the path alone is ambiguous.
    Q_INVOKABLE void markPlayedByPath(const QString &path,
                                      const QString &singer = QString());

    QString selectedSingerName() const { return m_selectedSingerName; }
    void setSelectedSingerName(const QString &name);

    int sortColumn() const { return m_sortColumn; }
    bool sortAscending() const { return m_sortAscending; }

signals:
    void selectedSingerNameChanged();
    void sortChanged();

private:
    QList<SongItem> m_songs;
    QList<int> m_visible;          // indices into m_songs currently shown
    QString m_selectedSingerName;

    void rebuildVisible();
    bool matchesFilter(const QString &singer) const;
    int sourceIndex(int visibleRow) const;
    int m_sortColumn = 0;      // 0 = queue order; 1 singer, 2 title, 3 artist, 4 source, 5 duration
    bool m_sortAscending = true;

    void persist();
    void applySort();
    DatabaseManager *m_databaseManager = nullptr;
};

#endif // SONGQUEUEMODEL_H
