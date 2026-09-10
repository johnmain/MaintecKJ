#ifndef ROTATIONCONTROLLER_H
#define ROTATIONCONTROLLER_H

#include <QObject>
#include <QString>

class SingerModel;
class SongQueueModel;
class MediaPlayerController;

// RotationController owns the karaoke rotation rules:
//   - it tracks the current singer (the one whose queue the panel shows);
//   - when a song ends naturally it rotates the finished singer to the bottom
//     of the list, marks them Inactive if they have nothing left to sing, and
//     advances to the next Active singer that still has unplayed songs.
class RotationController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentSinger READ currentSinger WRITE setCurrentSinger NOTIFY currentSingerChanged)

public:
    explicit RotationController(QObject *parent = nullptr);

    void setSingerModel(SingerModel *model);
    void setQueueModel(SongQueueModel *model);
    void setPlayer(MediaPlayerController *player);

    // Never hand QML a null QString: it maps to `undefined` (`undefined.length`
    // throws). Return a real empty string instead.
    QString currentSinger() const
    {
        return m_currentSinger.isNull() ? QStringLiteral("") : m_currentSinger;
    }
    void setCurrentSinger(const QString &name);

public slots:
    // Called when the current song reaches its natural end.
    void advance();
    // Sends the current singer to the bottom of the rotation without counting
    // their song as sung (their PlayedStatus is left untouched).
    void skipCurrentSinger();

signals:
    void currentSingerChanged();

private:
    int nextEligibleIndex(int fromIndex) const;

    SingerModel *m_singers = nullptr;
    SongQueueModel *m_queue = nullptr;
    MediaPlayerController *m_player = nullptr;
    QString m_currentSinger;
};

#endif // ROTATIONCONTROLLER_H
