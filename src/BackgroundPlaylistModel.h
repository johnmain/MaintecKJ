#ifndef BACKGROUNDPLAYLISTMODEL_H
#define BACKGROUNDPLAYLISTMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QString>

class DatabaseManager;

// The background music playlist. It deliberately shares nothing with
// SongQueueModel: those rows carry a singer, belong to the rotation and take
// part in played-marking, and a background track has none of that. Keeping them
// apart is what stops background music showing up as a singer's request.
class BackgroundPlaylistModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        ArtistRole,
        FilePathRole,
        DurationRole,
        SourceRole
    };

    explicit BackgroundPlaylistModel(QObject *parent = nullptr);
    void setDatabaseManager(DatabaseManager *databaseManager);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return rowCount(); }

    Q_INVOKABLE void addSong(const QString &artist, const QString &title,
                             const QString &path, int duration,
                             const QString &source = QString());
    Q_INVOKABLE void removeSong(int index);
    Q_INVOKABLE void moveSong(int fromIndex, int toIndex);
    Q_INVOKABLE void clearPlaylist();

    // Every song in the background library. Entries already on the list are
    // skipped, so pressing the button twice does not double the playlist.
    // Returns how many rows were actually added.
    Q_INVOKABLE int addAllFromLibrary();

    // One-shot reshuffle. The track that is currently loaded is followed to its
    // new position rather than being shuffled out from under the deck.
    Q_INVOKABLE void shuffle();

    Q_INVOKABLE QVariantMap songAt(int index) const;

    // Row to play after the current one, or -1 at the end of the list. The
    // playlist stops rather than looping, so something has to be chosen
    // deliberately.
    Q_INVOKABLE int nextIndex() const;

    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);

signals:
    void countChanged();
    void currentIndexChanged();
    void playlistChanged();

private:
    struct Entry {
        QString title;
        QString artist;
        QString filePath;
        int duration = 0;
        QString source;
    };

    QList<Entry> m_entries;
    int m_currentIndex = -1;

    void persist();
    DatabaseManager *m_databaseManager = nullptr;
};

#endif // BACKGROUNDPLAYLISTMODEL_H
