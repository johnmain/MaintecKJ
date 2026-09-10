#ifndef MEDIAPLAYERCONTROLLER_H
#define MEDIAPLAYERCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

QT_BEGIN_NAMESPACE
class QMediaPlayer;
class QAudioOutput;
QT_END_NAMESPACE

class RubberBandAudioEngine;

// MediaPlayerController owns the playback pipeline:
//   - audio (all media: mp3+cdg and video) runs through RubberBandAudioEngine
//     so key shift and pitch-preserving tempo can be applied;
//   - QMediaPlayer is kept (with its audio muted) for metadata, video frames
//     and hasVideo detection, and its position is nudged to follow the audio.
class MediaPlayerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString filePath READ filePath NOTIFY sourceChanged)
    Q_PROPERTY(QString title READ title NOTIFY metadataChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY metadataChanged)
    Q_PROPERTY(int position READ position NOTIFY positionChanged)
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY sourceChanged)
    Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY hasVideoChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(qreal tempo READ tempo WRITE setTempo NOTIFY tempoChanged)
    Q_PROPERTY(int pitch READ pitch WRITE setPitch NOTIFY pitchChanged)
    Q_PROPERTY(int audioDeviceIndex READ audioDeviceIndex NOTIFY audioDeviceChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)

public:
    explicit MediaPlayerController(QObject *parent = nullptr);

    QString filePath() const { return m_filePath; }
    QString title() const { return m_title; }
    QString artist() const { return m_artist; }
    int position() const;
    int duration() const;
    bool playing() const;
    bool hasMedia() const { return !m_filePath.isEmpty(); }
    bool hasVideo() const;
    int volume() const;
    qreal tempo() const;
    int pitch() const;
    int audioDeviceIndex() const;
    QString errorString() const { return m_errorString; }

    Q_INVOKABLE void setVolume(int volume);
    Q_INVOKABLE void setTempo(qreal tempo);
    Q_INVOKABLE void setPitch(int semitones);

public slots:
    Q_INVOKABLE void load(const QString &filePath);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void togglePlayPause();
    Q_INVOKABLE void seek(int positionMs);
    Q_INVOKABLE void seekFraction(qreal fraction);

    Q_INVOKABLE QStringList audioDevices() const;
    Q_INVOKABLE void setAudioDevice(int index);

    // Routes video frames to a QML VideoOutput's sink (Qt 6 removed VideoOutput.source).
    Q_INVOKABLE void setVideoSink(QObject *sink);

signals:
    void sourceChanged();
    void metadataChanged();
    void hasVideoChanged();
    void positionChanged();
    void durationChanged();
    void playingChanged();
    void volumeChanged();
    void tempoChanged();
    void pitchChanged();
    void audioDeviceChanged();
    void errorChanged();
    // Emitted when the current song reaches its natural end (not on stop()).
    void songFinished();

private:
    void updateMetadata();
    void syncVideoPosition();

    QMediaPlayer *m_player = nullptr;         // video + metadata only (audio muted)
    QAudioOutput *m_audioOutput = nullptr;    // muted clock for the video player
    RubberBandAudioEngine *m_audio = nullptr; // audible audio path (Rubber Band)

    QString m_filePath;
    QString m_title;
    QString m_artist;
    QString m_errorString;
    bool m_playerIsVideo = false;
    QTimer m_tempoTimer;
};

#endif // MEDIAPLAYERCONTROLLER_H
