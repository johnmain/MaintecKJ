#ifndef MODECONTROLLER_H
#define MODECONTROLLER_H

#include <QObject>
#include <QString>

class BackgroundPlaylistModel;
class MediaPlayerController;
class RotationController;

// Karaoke or background music. Owns the only two things that actually depend on
// which one is showing:
//
//   - what happens when a track ends. In karaoke the rotation advances but
//     nothing is auto-played, because the next singer has to be fetched. In
//     background music the next playlist entry is loaded and played.
//   - what the deck does when the operator switches away from background music:
//     the track fades out rather than being cut off. Going the other way is
//     seamless, so whatever is playing simply carries on.
class ModeController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(bool background READ isBackground NOTIFY modeChanged)
    Q_PROPERTY(int fadeOutMs READ fadeOutMs CONSTANT)

public:
    static constexpr int kFadeOutMs = 5000;
    static constexpr auto kKaraokeMode = "karaoke";
    static constexpr auto kBackgroundMode = "background";

    explicit ModeController(QObject *parent = nullptr);

    void setPlayer(MediaPlayerController *player);
    void setRotation(RotationController *rotation);
    void setBackgroundPlaylist(BackgroundPlaylistModel *playlist);

    QString mode() const { return m_mode; }
    bool isBackground() const { return m_mode == QLatin1String(kBackgroundMode); }
    int fadeOutMs() const { return kFadeOutMs; }

    void setMode(const QString &mode);

    // Loads and plays a playlist row. Used by double-click in the playlist panel;
    // the same path auto-advance takes.
    Q_INVOKABLE void playBackgroundRow(int row);

    // Loads the stored mode at start-up. Unlike setMode() this is silent: there
    // is no deck to fade out yet.
    Q_INVOKABLE void restore();

public slots:
    // Wired to MediaPlayerController::songFinished.
    void onSongFinished();

signals:
    void modeChanged();

private:
    void startTrackAt(int row);

    MediaPlayerController *m_player = nullptr;
    RotationController *m_rotation = nullptr;
    BackgroundPlaylistModel *m_playlist = nullptr;
    QString m_mode = QLatin1String(kKaraokeMode);
};

#endif // MODECONTROLLER_H
