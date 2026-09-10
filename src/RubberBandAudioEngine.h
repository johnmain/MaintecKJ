#ifndef RUBBERBANDAUDIOENGINE_H
#define RUBBERBANDAUDIOENGINE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QAudioFormat>
#include <QVector>
#include <QTimer>
#include <QMutex>
#include <atomic>

QT_BEGIN_NAMESPACE
class QAudioDecoder;
class QAudioSink;
class QIODevice;
class QAudioBuffer;
QT_END_NAMESPACE

namespace RubberBand {
class RubberBandStretcher;
}

// RubberBandAudioEngine decodes a local media file to PCM, runs it through the
// Rubber Band Library (time stretch + pitch shift) and plays the result through
// a QAudioSink. It is the audio path for every media type (mp3+cdg and video);
// the video picture is handled separately.
class RubberBandAudioEngine : public QObject
{
    Q_OBJECT

public:
    explicit RubberBandAudioEngine(QObject *parent = nullptr);
    ~RubberBandAudioEngine() override;

    void setSource(const QString &filePath);

    void play();
    void pause();
    void stop();
    void seek(int sourceMs);

    bool isPlaying() const { return m_playing; }
    bool hasMedia() const { return !m_sourcePath.isEmpty(); }
    int position() const;
    int duration() const { return static_cast<int>(m_durationMs); }

    QStringList audioDevices() const;
    void setAudioDevice(int index);
    int audioDeviceIndex() const;

    // Pull-mode entry point: called by the audio sink (audio thread) for PCM.
    qint64 readAudio(char *data, qint64 maxlen);

    double tempo() const { return m_tempo; }
    int semitones() const { return m_semitones; }
    int volume() const { return m_volume; }
    void setTempo(double tempo);
    void setSemitones(int semitones);
    void setVolume(int volume);

signals:
    void positionChanged();
    void durationChanged();
    void stateChanged();
    void errorOccurred(const QString &message);
    // Emitted once when playback reaches the natural end of the stream (not on
    // stop()/seek()). Used to drive singer rotation.
    void finished();

private:
    void onBufferReady();
    void onDecoderFinished();
    void pump();
    void configure(const QAudioFormat &format);
    void createSink();
    void createStretcher();
    void resetPipelineBuffers();
    void appendBuffer(const QAudioBuffer &buffer);
    void applyRatios();

    QAudioDecoder *m_decoder = nullptr;
    QAudioSink *m_sink = nullptr;
    QIODevice *m_sinkDevice = nullptr;
    RubberBand::RubberBandStretcher *m_stretcher = nullptr;

    QTimer m_pumpTimer;

    QString m_sourcePath;
    int m_sampleRate = 0;
    int m_channels = 0;
    bool m_configured = false;

    double m_tempo = 1.0;
    int m_semitones = 0;
    int m_volume = 80;

    bool m_playing = false;
    bool m_decoderFinished = false;
    bool m_flushed = false;
    bool m_finishedEmitted = false;

    qint64 m_durationMs = 0;
    qint64 m_seekTargetMs = 0;
    qint64 m_outputBaseMs = 0;
    qint64 m_framesToDiscard = 0;
    std::atomic<qint64> m_outputFrames{0};
    qint64 m_baseUs = 0;
    int m_deviceIndex = -1;
    QIODevice *m_ioDevice = nullptr;
    bool m_sinkOutputIsFloat = false;
    mutable QMutex m_mutex;

    // Interleaved float input FIFO (frames at the front).
    QVector<float> m_input;
    int m_inputFrames = 0;
    qsizetype m_readIndex = 0;

    QVector<QVector<float>> m_planarIn;
    QVector<QVector<float>> m_planarOut;
};

#endif // RUBBERBANDAUDIOENGINE_H
