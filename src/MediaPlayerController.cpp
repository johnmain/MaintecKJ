#include "MediaPlayerController.h"
#include "RubberBandAudioEngine.h"

#include <QMediaPlayer>
#include <QAudioOutput>
#include <QMediaMetaData>
#include <QVideoSink>
#include <QFileInfo>
#include <QFile>
#include <QUrl>
#include <QDebug>

MediaPlayerController::MediaPlayerController(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
    , m_audio(new RubberBandAudioEngine(this))
{
    // The QMediaPlayer only supplies video frames, metadata and duration. Its own
    // audio is silenced; the Rubber Band engine produces the audible audio.
    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(0.0);

    // ~33 steps a second is smooth enough and keeps the timer cheap.
    m_fadeTimer.setInterval(30);
    connect(&m_fadeTimer, &QTimer::timeout, this, &MediaPlayerController::stepFade);

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia)
                    updateMetadata();
                emit hasVideoChanged();
            });

    connect(m_player, &QMediaPlayer::durationChanged, this,
            [this](qint64) { emit durationChanged(); });

    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &errorText) {
                m_errorString = errorText;
                emit errorChanged();
                qWarning() << "MediaPlayer error:" << errorText;
            });

    connect(m_audio, &RubberBandAudioEngine::positionChanged, this, [this]() {
        emit positionChanged();
        syncVideoPosition();
    });
    connect(m_audio, &RubberBandAudioEngine::durationChanged, this,
            [this]() { emit durationChanged(); });
    connect(m_audio, &RubberBandAudioEngine::stateChanged, this,
            [this]() { emit playingChanged(); });
    // Natural end of the song (raised on the audio thread; delivered here on
    // the main thread). Rotation listens to this to advance the next singer.
    connect(m_audio, &RubberBandAudioEngine::finished, this, [this]() {
        m_tempoTimer.stop();
        emit songFinished();
    });
    connect(m_audio, &RubberBandAudioEngine::errorOccurred, this,
            [this](const QString &message) {
                // Whatever went wrong, there is nothing coming out of the deck, so
                // it must stop claiming to play.
                m_audio->stop();
                m_errorString = message;
                emit playingChanged();
                emit errorChanged();
                qWarning() << "Audio engine error:" << message;
            });

    // Retune the (muted) video player's rate only after the tempo slider settles.
    m_tempoTimer.setSingleShot(true);
    m_tempoTimer.setInterval(250);
    connect(&m_tempoTimer, &QTimer::timeout, this, [this]() {
        if (m_playerIsVideo)
            m_player->setPlaybackRate(m_audio->tempo());
    });
}

int MediaPlayerController::position() const
{
    return m_audio->position();
}

int MediaPlayerController::duration() const
{
    const int d = m_audio->duration();
    return d > 0 ? d : static_cast<int>(m_player->duration());
}

bool MediaPlayerController::playing() const
{
    return m_audio->isPlaying();
}

bool MediaPlayerController::hasVideo() const
{
    return m_player->hasVideo();
}

int MediaPlayerController::volume() const
{
    return m_audio->volume();
}

qreal MediaPlayerController::tempo() const
{
    return m_audio->tempo();
}

int MediaPlayerController::pitch() const
{
    return m_audio->semitones();
}

int MediaPlayerController::audioDeviceIndex() const
{
    return m_audio->audioDeviceIndex();
}

void MediaPlayerController::fadeOutAndStop(int milliseconds)
{
    if (milliseconds <= 0) {
        cancelFade();
        stop();
        return;
    }

    // Asking again restarts the fade from wherever it has got to.
    cancelFade();

    m_volumeBeforeFade = m_audio->volume();
    m_fadeTotalMs = milliseconds;
    m_fadeElapsedMs = 0;
    m_fading = true;
    m_fadeTimer.start();
}

void MediaPlayerController::cancelFade()
{
    if (!m_fading)
        return;

    m_fadeTimer.stop();
    m_fading = false;

    // The dip was never reported as the volume, so restoring the engine's level
    // says nothing to anyone - the UI still shows exactly this value.
    m_fadeApplying = true;
    m_audio->setVolume(m_volumeBeforeFade);
    m_fadeApplying = false;
}

void MediaPlayerController::stepFade()
{
    if (!m_fading)
        return;

    m_fadeElapsedMs += m_fadeTimer.interval();
    const qreal remaining = 1.0 - (static_cast<qreal>(m_fadeElapsedMs) / m_fadeTotalMs);
    if (remaining <= 0.0) {
        finishFade();
        return;
    }

    // Straight to the engine: the ramp is an implementation detail, so it must
    // not drag the volume slider - or the DJ fader mirroring it - down with it.
    m_fadeApplying = true;
    m_audio->setVolume(static_cast<int>(m_volumeBeforeFade * remaining));
    m_fadeApplying = false;
}

void MediaPlayerController::finishFade()
{
    m_fadeTimer.stop();
    m_fading = false;

    m_fadeApplying = true;
    m_audio->setVolume(m_volumeBeforeFade);
    m_fadeApplying = false;

    stop();
}

void MediaPlayerController::updateMetadata()
{
    const QMediaMetaData meta = m_player->metaData();

    const QString metaTitle = meta.value(QMediaMetaData::Title).toString();
    const QString metaArtist = meta.value(QMediaMetaData::ContributingArtist).toString();
    const QString albumArtist = meta.value(QMediaMetaData::AlbumArtist).toString();

    if (!metaTitle.isEmpty())
        m_title = metaTitle;
    if (!metaArtist.isEmpty())
        m_artist = metaArtist;
    else if (!albumArtist.isEmpty())
        m_artist = albumArtist;

    emit metadataChanged();
}

void MediaPlayerController::syncVideoPosition()
{
    if (!m_playerIsVideo || !m_player->hasVideo() || !playing())
        return;
    // Don't fight the player while a tempo change is settling.
    if (m_tempoTimer.isActive())
        return;

    const qint64 audioPos = m_audio->position();
    if (qAbs(m_player->position() - audioPos) > 500)
        m_player->setPosition(audioPos);
}

void MediaPlayerController::load(const QString &filePath)
{
    if (filePath.isEmpty())
        return;

    // A fade belongs to the outgoing song; it must not follow us into the next.
    cancelFade();

    // A karaoke pair can arrive as its .cdg half - an OpenKJ export lists them,
    // and a library scan stores one when the .mp3 was missing at the time. That
    // file holds graphics and no audio whatsoever, so playing it produces "the
    // media contains no audio stream". Play the .mp3 beside it instead; the CDG
    // renderer is handed the graphics separately, through companionCdg().
    QString playable = filePath;
    if (QFileInfo(filePath).suffix().compare(QLatin1String("cdg"), Qt::CaseInsensitive) == 0) {
        const QFileInfo cdgInfo(filePath);
        const QString companion = cdgInfo.absolutePath() + QLatin1Char('/')
                                  + cdgInfo.completeBaseName() + QStringLiteral(".mp3");
        playable = QFile::exists(companion) ? companion : QString();
    }

    m_filePath = playable.isEmpty() ? filePath : playable;
    m_errorString.clear();

    // Fall back to the filename until real tags are resolved.
    const QFileInfo info(m_filePath);
    m_title = info.completeBaseName();
    m_artist.clear();

    // A queue row can point at a file that is not on this machine - an OpenKJ
    // import done before the local library was indexed keeps the old computer's
    // path. Say so and stay stopped. Without this the deck reports the song as
    // loaded and playing while nothing comes out, which is baffling.
    if (playable.isEmpty() || !QFile::exists(playable)) {
        // Clear the engine rather than just stopping it: otherwise the previous
        // song stays primed and the Play button would start that instead.
        m_audio->setSource(QString());
        m_playerIsVideo = false;
        m_errorString = playable.isEmpty()
                            ? tr("No .mp3 beside %1").arg(QFileInfo(filePath).fileName())
                            : tr("File not found: %1").arg(playable);

        emit sourceChanged();
        emit metadataChanged();
        emit hasVideoChanged();
        emit playingChanged();
        emit errorChanged();
        return;
    }

    m_player->setSource(QUrl::fromLocalFile(playable));
    static const QStringList videoSuffixes = {
        QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"),
        QStringLiteral("mov"), QStringLiteral("webm"), QStringLiteral("mpg"),
        QStringLiteral("mpeg"), QStringLiteral("m4v"), QStringLiteral("wmv"),
        QStringLiteral("flv"), QStringLiteral("ts"), QStringLiteral("m2ts")
    };
    m_playerIsVideo = videoSuffixes.contains(info.suffix().toLower());

    // Only the video path keeps the (silent) QMediaPlayer audio output; for pure
    // audio files the player is metadata-only and must not contribute sound.
    m_player->setAudioOutput(m_playerIsVideo ? m_audioOutput : nullptr);
    m_audioOutput->setVolume(0.0);
    m_player->setPlaybackRate(m_audio->tempo());
    m_audio->setSource(playable);

    emit sourceChanged();
    emit metadataChanged();
    emit hasVideoChanged();
    emit durationChanged();
    emit errorChanged();

    play();
}

void MediaPlayerController::play()
{
    // Starting playback part way through a fade would come out silent.
    cancelFade();
    m_audio->play();
    if (m_playerIsVideo)
        m_player->play();
}

void MediaPlayerController::pause()
{
    m_audio->pause();
    if (m_playerIsVideo)
        m_player->pause();
}

void MediaPlayerController::stop()
{
    m_audio->stop();
    if (m_playerIsVideo)
        m_player->stop();
}

void MediaPlayerController::togglePlayPause()
{
    if (playing())
        pause();
    else
        play();
}

void MediaPlayerController::seek(int positionMs)
{
    m_audio->seek(positionMs);
    if (m_playerIsVideo)
        m_player->setPosition(positionMs);
}

void MediaPlayerController::seekFraction(qreal fraction)
{
    const int total = duration();
    if (total <= 0)
        return;
    seek(static_cast<int>(qBound<qreal>(0.0, fraction, 1.0) * total));
}

void MediaPlayerController::setVolume(int volume)
{
    m_audio->setVolume(volume);

    // If the operator moves the fader mid-fade, that becomes the level the fade
    // returns to, so the fade cannot undo the change a moment later.
    if (m_fading && !m_fadeApplying)
        m_volumeBeforeFade = qBound(0, volume, 100);

    emit volumeChanged();
}

void MediaPlayerController::setTempo(qreal tempo)
{
    m_audio->setTempo(tempo);
    // Debounce the video rate change: retuning it on every slider tick makes the
    // player jump/seek repeatedly.
    m_tempoTimer.start();
    emit tempoChanged();
}

void MediaPlayerController::setPitch(int semitones)
{
    m_audio->setSemitones(semitones);
    emit pitchChanged();
}

QStringList MediaPlayerController::audioDevices() const
{
    return m_audio->audioDevices();
}

void MediaPlayerController::setAudioDevice(int index)
{
    m_audio->setAudioDevice(index);
    emit audioDeviceChanged();
}

void MediaPlayerController::setVideoSink(QObject *sink)
{
    m_player->setVideoSink(qobject_cast<QVideoSink *>(sink));
    emit hasVideoChanged();
}
