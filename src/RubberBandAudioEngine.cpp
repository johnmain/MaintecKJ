#include "RubberBandAudioEngine.h"

#include <QAudioDecoder>
#include <QAudioSink>
#include <QAudio>
#include <QAudioBuffer>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QFileInfo>
#include <QIODevice>
#include <QUrl>
#include <QMutexLocker>
#include <QDebug>

#include <rubberband/RubberBandStretcher.h>

#include <cmath>

namespace {

// QAudioSink pull-mode source: the sink calls readAudio() at the device's real
// playback rate, which paces the whole pipeline.
class EngineIODevice : public QIODevice
{
public:
    explicit EngineIODevice(RubberBandAudioEngine *engine) : m_engine(engine) {}

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return 1 << 20; }

protected:
    qint64 readData(char *data, qint64 maxlen) override
    {
        return m_engine->readAudio(data, maxlen);
    }
    qint64 writeData(const char *, qint64) override { return 0; }

private:
    RubberBandAudioEngine *m_engine;
};

} // namespace

RubberBandAudioEngine::RubberBandAudioEngine(QObject *parent)
    : QObject(parent)
{
    m_decoder = new QAudioDecoder(this);

    connect(m_decoder, &QAudioDecoder::bufferReady, this, &RubberBandAudioEngine::onBufferReady);
    connect(m_decoder, &QAudioDecoder::finished, this, &RubberBandAudioEngine::onDecoderFinished);
    connect(m_decoder, &QAudioDecoder::durationChanged, this, [this](qint64 d) {
        if (d > 0) {
            m_durationMs = d;
            emit durationChanged();
        }
    });
    connect(m_decoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error), this,
            [this](QAudioDecoder::Error) { emit errorOccurred(m_decoder->errorString()); });

    m_pumpTimer.setInterval(33);
    connect(&m_pumpTimer, &QTimer::timeout, this, &RubberBandAudioEngine::pump);
}

RubberBandAudioEngine::~RubberBandAudioEngine()
{
    m_pumpTimer.stop();
    delete m_ioDevice;
    delete m_stretcher;
}

void RubberBandAudioEngine::setSource(const QString &filePath)
{
    m_pumpTimer.stop();
    m_decoder->stop();

    // Tear the sink down before taking m_mutex: stop() waits for the audio
    // thread, and that thread may be blocked on m_mutex inside readAudio().
    if (m_sink) {
        m_sink->stop();
        m_sink->deleteLater();
        m_sink = nullptr;
    }

    QMutexLocker locker(&m_mutex);
    delete m_stretcher;
    m_stretcher = nullptr;
    if (m_ioDevice) {
        m_ioDevice->close();
        delete m_ioDevice;
        m_ioDevice = nullptr;
    }
    m_sinkDevice = nullptr;
    m_configured = false;
    locker.unlock();

    m_sourcePath = filePath;
    m_sampleRate = 0;
    m_channels = 0;
    m_durationMs = 0;
    m_seekTargetMs = 0;
    m_outputBaseMs = 0;
    m_outputFrames = 0;
    m_framesToDiscard = 0;
    m_decoderFinished = false;
    m_flushed = false;
    m_finishedEmitted = false;
    m_playing = false;
    resetPipelineBuffers();

    if (!filePath.isEmpty()) {
        m_decoder->setSource(QUrl::fromLocalFile(filePath));

        // Decode at the output device's native sample rate so the stretcher and
        // the sink run at the same rate (otherwise playback speed/pitch drift).
        QAudioFormat want = m_decoder->audioFormat();
        const QAudioFormat preferred = QMediaDevices::defaultAudioOutput().preferredFormat();
        if (preferred.sampleRate() > 0)
            want.setSampleRate(preferred.sampleRate());
        if (want.channelCount() <= 0)
            want.setChannelCount(2);
        want.setSampleFormat(QAudioFormat::Float);
        m_decoder->setAudioFormat(want);

        m_decoder->start();
    }

    emit durationChanged();
    emit stateChanged();
}

void RubberBandAudioEngine::resetPipelineBuffers()
{
    QMutexLocker locker(&m_mutex);
    m_input.clear();
    m_inputFrames = 0;
    m_readIndex = 0;
}

void RubberBandAudioEngine::configure(const QAudioFormat &format)
{
    m_sampleRate = format.sampleRate();
    m_channels = format.channelCount();

    if (m_sampleRate <= 0 || m_channels <= 0) {
        emit errorOccurred(QStringLiteral("Unsupported audio format"));
        return;
    }

    createStretcher();

    createSink();

    if (m_seekTargetMs > 0)
        m_framesToDiscard = m_seekTargetMs * m_sampleRate / 1000;

    m_configured = true;
}

void RubberBandAudioEngine::createSink()
{
    const QList<QAudioDevice> devices = QMediaDevices::audioOutputs();
    const QAudioDevice device = (m_deviceIndex >= 0 && m_deviceIndex < devices.size())
                                    ? devices.at(m_deviceIndex)
                                    : QMediaDevices::defaultAudioOutput();

    QAudioFormat sinkFormat;
    sinkFormat.setSampleRate(m_sampleRate);
    sinkFormat.setChannelCount(m_channels);
    sinkFormat.setSampleFormat(QAudioFormat::Float);
    if (!device.isFormatSupported(sinkFormat)) {
        sinkFormat.setSampleFormat(QAudioFormat::Int16);
        if (!device.isFormatSupported(sinkFormat))
            sinkFormat = device.preferredFormat();
    }
    m_sinkOutputIsFloat = (sinkFormat.sampleFormat() == QAudioFormat::Float);

    m_sink = new QAudioSink(device, sinkFormat, this);
    m_sink->setBufferSize(m_sampleRate * m_channels * 4 / 2); // ~500 ms
    m_sink->setVolume(m_volume / 100.0);

    m_ioDevice = new EngineIODevice(this);
    m_ioDevice->open(QIODevice::ReadOnly);
    m_sink->start(m_ioDevice);
    m_baseUs = 0;
}

void RubberBandAudioEngine::createStretcher()
{
    m_stretcher = new RubberBand::RubberBandStretcher(
        static_cast<size_t>(m_sampleRate),
        static_cast<size_t>(m_channels),
        RubberBand::RubberBandStretcher::OptionProcessRealTime
            | RubberBand::RubberBandStretcher::OptionChannelsTogether
            | RubberBand::RubberBandStretcher::OptionEngineFiner);
    applyRatios();
    m_stretcher->setMaxProcessSize(4096);
}

void RubberBandAudioEngine::applyRatios()
{
    if (!m_stretcher)
        return;
    m_stretcher->setTimeRatio(1.0 / m_tempo);
    m_stretcher->setPitchScale(std::pow(2.0, m_semitones / 12.0));
}

void RubberBandAudioEngine::appendBuffer(const QAudioBuffer &buffer)
{
    if (!buffer.isValid())
        return;

    if (!m_configured)
        configure(buffer.format());
    if (!m_configured)
        return;

    const QAudioFormat format = buffer.format();
    const int channels = format.channelCount();
    const int frames = static_cast<int>(buffer.frameCount());
    if (channels <= 0 || frames <= 0)
        return;

    int startFrame = 0;
    if (m_framesToDiscard > 0) {
        const qint64 skip = qMin<qint64>(m_framesToDiscard, frames);
        startFrame = static_cast<int>(skip);
        m_framesToDiscard -= skip;
    }
    const int count = frames - startFrame;
    if (count <= 0)
        return;

    const QAudioFormat::SampleFormat sf = format.sampleFormat();
    const qint16 *raw16 = nullptr;
    const qint32 *raw32 = nullptr;
    const float *rawF = nullptr;
    if (sf == QAudioFormat::Int16)
        raw16 = buffer.constData<qint16>();
    else if (sf == QAudioFormat::Int32)
        raw32 = buffer.constData<qint32>();
    else if (sf == QAudioFormat::Float)
        rawF = buffer.constData<float>();

    QMutexLocker locker(&m_mutex);
    const int base = static_cast<int>(m_input.size());
    m_input.resize(base + count * m_channels);
    for (int f = 0; f < count; ++f) {
        for (int c = 0; c < m_channels; ++c) {
            const qsizetype idx = static_cast<qsizetype>(startFrame + f) * channels + c;
            float value = 0.0f;
            if (raw16)
                value = raw16[idx] / 32768.0f;
            else if (raw32)
                value = raw32[idx] / 2147483648.0f;
            else if (rawF)
                value = rawF[idx];
            m_input[base + f * m_channels + c] = value;
        }
    }
    m_inputFrames += count;
}

void RubberBandAudioEngine::onBufferReady()
{
    const QAudioBuffer buffer = m_decoder->read();
    if (buffer.isValid())
        appendBuffer(buffer);
}

void RubberBandAudioEngine::onDecoderFinished()
{
    m_decoderFinished = true;
}

// Called on the audio thread at the device's playback rate.
qint64 RubberBandAudioEngine::readAudio(char *data, qint64 maxlen)
{
    QMutexLocker locker(&m_mutex);

    if (!m_configured || !m_stretcher || m_channels <= 0)
        return 0;

    const int bytesPerFrame = m_channels * (m_sinkOutputIsFloat ? 4 : 2);
    int requestedFrames = static_cast<int>(maxlen / bytesPerFrame);
    if (requestedFrames <= 0)
        return 0;

    int produced = 0;
    while (produced < requestedFrames) {
        // Feed the stretcher with decoded input. When the decoder is finished
        // and this is the last buffered chunk, mark that chunk as final so
        // Rubber Band flushes. (A partial tail used to sit here unflushed
        // forever: the stretcher asked for more input than was left.)
        while (!m_flushed && m_inputFrames > 0) {
            const size_t required = m_stretcher->getSamplesRequired();
            if (required == 0)
                break;   // it already has enough input for now

            const int need = static_cast<int>(qMin<size_t>(required, m_inputFrames));
            const bool final = m_decoderFinished && (need == m_inputFrames);

            m_planarIn.resize(m_channels);
            for (int c = 0; c < m_channels; ++c) {
                m_planarIn[c].resize(need);
                float *dst = m_planarIn[c].data();
                for (int f = 0; f < need; ++f)
                    dst[f] = m_input[m_readIndex + f * m_channels + c];
            }
            QVector<const float *> pointers(m_channels);
            for (int c = 0; c < m_channels; ++c)
                pointers[c] = m_planarIn[c].constData();
            m_stretcher->process(pointers.constData(), need, final);

            m_readIndex += static_cast<qsizetype>(need) * m_channels;
            m_inputFrames -= need;
            if (m_readIndex >= m_input.size()) {
                m_input.clear();
                m_readIndex = 0;
            } else if (m_readIndex > (1 << 20)) {
                m_input.remove(0, m_readIndex);
                m_readIndex = 0;
            }

            if (final)
                m_flushed = true;
        }

        if (m_decoderFinished && m_inputFrames == 0 && !m_flushed) {
            m_stretcher->process(nullptr, 0, true);
            m_flushed = true;
        }

        const int available = m_stretcher->available();
        if (available <= 0)
            break;

        const int chunk = qMin(available, requestedFrames - produced);
        m_planarOut.resize(m_channels);
        for (int c = 0; c < m_channels; ++c)
            m_planarOut[c].resize(chunk);
        QVector<float *> outPtrs(m_channels);
        for (int c = 0; c < m_channels; ++c)
            outPtrs[c] = m_planarOut[c].data();
        const size_t got = m_stretcher->retrieve(outPtrs.data(), chunk);
        if (got == 0)
            break;

        for (int f = 0; f < static_cast<int>(got); ++f) {
            for (int c = 0; c < m_channels; ++c) {
                const float s = qBound(-1.0f, m_planarOut[c][f], 1.0f);
                if (m_sinkOutputIsFloat)
                    reinterpret_cast<float *>(data)[(produced + f) * m_channels + c] = s;
                else
                    reinterpret_cast<qint16 *>(data)[(produced + f) * m_channels + c] =
                        static_cast<qint16>(s * 32767.0f);
            }
        }
        produced += static_cast<int>(got);
    }

    m_outputFrames += produced;

    // Natural end of stream: decoder done, stretcher flushed and drained, and
    // the sink is asking for data we no longer have. Raise it exactly once.
    //
    // readAudio() may be called on the GUI thread and always holds m_mutex, so
    // the signal must never be delivered inline: handlers call stop()/load()
    // which lock the very same (non-recursive) mutex -> self-deadlock. Post it
    // to the event loop instead, by which time the lock is released.
    if (produced == 0 && m_flushed && m_decoderFinished && m_inputFrames == 0
        && !m_finishedEmitted) {
        m_finishedEmitted = true;
        QMetaObject::invokeMethod(this, [this]() {
            // Skip if the host pressed Stop / re-seeked before the event ran.
            if (m_finishedEmitted)
                emit finished();
        }, Qt::QueuedConnection);
    }

    return static_cast<qint64>(produced) * bytesPerFrame;
}

void RubberBandAudioEngine::pump()
{
    emit positionChanged();
}

int RubberBandAudioEngine::position() const
{
    if (!m_sink || m_sampleRate <= 0)
        return static_cast<int>(m_outputBaseMs);
    // processedUSecs() is the audio the sink has actually played (excludes the
    // buffer lead); source time scales by the tempo ratio.
    const qint64 outUs = m_sink->processedUSecs() - m_baseUs;
    return static_cast<int>(m_outputBaseMs + (outUs / 1000.0) * m_tempo);
}

void RubberBandAudioEngine::play()
{
    if (m_sourcePath.isEmpty())
        return;
    m_playing = true;
    m_finishedEmitted = false;
    if (m_decoder && !m_decoder->isDecoding())
        m_decoder->start();
    if (m_sink) {
        if (m_sink->state() == QAudio::StoppedState)
            m_sink->start(m_ioDevice);
        else
            m_sink->resume();
    }
    m_pumpTimer.start();
    emit stateChanged();
}

void RubberBandAudioEngine::pause()
{
    m_playing = false;
    m_pumpTimer.stop();
    if (m_sink)
        m_sink->suspend();
    emit stateChanged();
}

void RubberBandAudioEngine::stop()
{
    m_playing = false;
    m_pumpTimer.stop();
    if (m_sink)
        m_sink->stop();
    if (m_decoder)
        m_decoder->stop();

    QMutexLocker locker(&m_mutex);
    m_outputBaseMs = 0;
    m_outputFrames = 0;
    m_baseUs = 0;
    m_seekTargetMs = 0;
    m_framesToDiscard = 0;
    m_decoderFinished = false;
    m_flushed = false;
    m_finishedEmitted = false;
    m_input.clear();
    m_inputFrames = 0;
    m_readIndex = 0;
    if (m_stretcher)
        m_stretcher->reset();
    locker.unlock();

    emit stateChanged();
    emit positionChanged();
}

void RubberBandAudioEngine::seek(int sourceMs)
{
    if (m_sourcePath.isEmpty())
        return;

    m_seekTargetMs = qMax<qint64>(0, sourceMs);
    m_outputBaseMs = m_seekTargetMs;
    m_outputFrames = 0;
    m_baseUs = 0;
    m_framesToDiscard = m_sampleRate > 0 ? (m_seekTargetMs * m_sampleRate / 1000) : -1;
    m_decoderFinished = false;
    m_flushed = false;
    m_finishedEmitted = false;

    {
        QMutexLocker locker(&m_mutex);
        m_input.clear();
        m_inputFrames = 0;
        m_readIndex = 0;
        // Recreate the stretcher so any "after final chunk" engine state is cleared.
        delete m_stretcher;
        m_stretcher = nullptr;
        if (m_sampleRate > 0 && m_channels > 0)
            createStretcher();
    }

    if (m_decoder) {
        m_decoder->stop();
        m_decoder->start();
    }
    if (m_sink) {
        m_sink->reset();
        if (m_ioDevice) {
            m_ioDevice->close();
            m_ioDevice->open(QIODevice::ReadOnly);
        }
        m_sink->start(m_ioDevice);
        if (m_playing)
            m_sink->resume();
    }

    if (m_playing)
        m_pumpTimer.start();

    emit positionChanged();
}

void RubberBandAudioEngine::setTempo(double tempo)
{
    tempo = qBound(0.5, tempo, 2.0);
    if (qFuzzyCompare(tempo, m_tempo))
        return;

    // position() is  m_outputBaseMs + (processedUSecs() - m_baseUs) * m_tempo.
    // Both terms have to be re-anchored together. Moving only m_outputBaseMs
    // left the already-elapsed output time in the formula, so the new ratio was
    // applied to it a second time and the position leapt forward by
    // (elapsed * newTempo) - which is what threw the CDG graphics back/forward
    // the moment the tempo slider moved. Pinning m_baseUs to "now" makes the new
    // ratio take effect from this instant onwards, keeping the position - and so
    // the CDG frame and the video sync - continuous.
    const qint64 outUsNow = (m_sink && m_sampleRate > 0) ? m_sink->processedUSecs() : m_baseUs;
    m_outputBaseMs += static_cast<qint64>((outUsNow - m_baseUs) / 1000.0 * m_tempo);
    m_baseUs = outUsNow;
    m_outputFrames = 0;
    {
        QMutexLocker locker(&m_mutex);
        m_tempo = tempo;
        applyRatios();
    }
    emit positionChanged();
}

void RubberBandAudioEngine::setSemitones(int semitones)
{
    const int clamped = qBound(-6, semitones, 6);
    if (clamped == m_semitones)
        return;
    QMutexLocker locker(&m_mutex);
    m_semitones = clamped;
    applyRatios();
}

void RubberBandAudioEngine::setVolume(int volume)
{
    const int clamped = qBound(0, volume, 100);
    if (clamped == m_volume)
        return;
    m_volume = clamped;
    if (m_sink)
        m_sink->setVolume(m_volume / 100.0);
}

QStringList RubberBandAudioEngine::audioDevices() const
{
    QStringList names;
    const QList<QAudioDevice> devices = QMediaDevices::audioOutputs();
    for (const QAudioDevice &device : devices)
        names.append(device.description());
    return names;
}

void RubberBandAudioEngine::setAudioDevice(int index)
{
    const QList<QAudioDevice> devices = QMediaDevices::audioOutputs();
    if (index < 0 || index >= devices.size())
        return;
    if (index == m_deviceIndex)
        return;

    m_deviceIndex = index;

    if (m_sink && m_configured) {
        const bool wasPlaying = m_playing;
        m_sink->stop();
        m_sink->deleteLater();
        m_sink = nullptr;
        if (m_ioDevice) {
            m_ioDevice->close();
            delete m_ioDevice;
            m_ioDevice = nullptr;
        }
        m_sinkDevice = nullptr;
        createSink();
        if (wasPlaying)
            m_sink->resume();
    }
}

int RubberBandAudioEngine::audioDeviceIndex() const
{
    if (m_deviceIndex >= 0)
        return m_deviceIndex;

    const QList<QAudioDevice> devices = QMediaDevices::audioOutputs();
    const QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
    for (int i = 0; i < devices.size(); ++i)
        if (devices.at(i).id() == defaultDevice.id())
            return i;
    return -1;
}
