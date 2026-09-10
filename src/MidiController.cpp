#include "MidiController.h"

#include "MediaPlayerController.h"
#include "SongQueueModel.h"

#include <QDebug>
#include <QSettings>
#include <QSocketNotifier>

#include <alsa/asoundlib.h>
#include <poll.h>

namespace {

// Set MAINTECKJ_MIDI_LOG=1 to dump every controller/note event the device sends.
// Useful for confirming a binding on hardware we have not seen before.
bool loggingEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("MAINTECKJ_MIDI_LOG");
    return enabled;
}

} // namespace

MidiController::MidiController(QObject *parent)
    : QObject(parent)
{
    m_faders[LeftFader].channel = 1;
    m_faders[LeftFader].cc = 0x08;
    m_faders[LeftFader].action = FaderBinding::Tempo;
    m_faders[LeftFader].softTakeover = true;

    m_faders[RightFader].channel = 2;
    m_faders[RightFader].cc = 0x08;
    m_faders[RightFader].action = FaderBinding::KeyShift;
    m_faders[RightFader].softTakeover = true;

    m_faders[VolumeFader].channel = 2;
    m_faders[VolumeFader].cc = 0x00;
    m_faders[VolumeFader].action = FaderBinding::Volume;
    m_faders[VolumeFader].softTakeover = false;

    m_playChannel = 2;
    m_playNote = 0x07;
    m_deviceFilter = QStringLiteral("Inpulse 200");

    loadBindings();

    m_keyShiftTimer.setSingleShot(true);
    connect(&m_keyShiftTimer, &QTimer::timeout, this, &MidiController::persistKeyShift);

    openSequencer();
}

MidiController::~MidiController()
{
    closeSequencer();
}

void MidiController::setPlayer(MediaPlayerController *player)
{
    if (m_player == player)
        return;
    if (m_player)
        m_player->disconnect(this);
    m_player = player;
    if (m_player) {
        connect(m_player, &MediaPlayerController::sourceChanged,
                this, &MidiController::onPlayerSourceChanged);
        connect(m_player, &MediaPlayerController::tempoChanged,
                this, &MidiController::onPlayerTempoChanged);
        connect(m_player, &MediaPlayerController::pitchChanged,
                this, &MidiController::onPlayerPitchChanged);
        connect(m_player, &MediaPlayerController::volumeChanged,
                this, &MidiController::onPlayerVolumeChanged);
    }
}

void MidiController::setQueueModel(SongQueueModel *queue)
{
    m_queue = queue;
}

void MidiController::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    if (enabled)
        resetAllFaders(); // re-arm, so re-enabling cannot teleport the deck
    emit enabledChanged();
}

void MidiController::rescan()
{
    if (!m_seq)
        openSequencer();
    attachDevice();
}

void MidiController::openSequencer()
{
    if (m_seq)
        return;

    int rc = snd_seq_open(&m_seq, "default", SND_SEQ_OPEN_INPUT | SND_SEQ_NONBLOCK, 0);
    if (rc < 0) {
        qWarning() << "MidiController: cannot open the ALSA sequencer:" << snd_strerror(rc);
        m_seq = nullptr;
        return;
    }

    snd_seq_set_client_name(m_seq, "MaintecKJ");
    m_port = snd_seq_create_simple_port(m_seq, "in",
                                        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                        SND_SEQ_PORT_TYPE_MIDI_GENERIC);
    if (m_port < 0) {
        qWarning() << "MidiController: cannot create an input port:" << snd_strerror(m_port);
        snd_seq_close(m_seq);
        m_seq = nullptr;
        return;
    }

    // Announcements are what let us attach when the controller is plugged in
    // after the app has already started.
    snd_seq_connect_from(m_seq, m_port, 0, 1);

    const int count = snd_seq_poll_descriptors_count(m_seq, POLLIN);
    if (count > 0) {
        struct pollfd descriptors[4];
        const int used = snd_seq_poll_descriptors(m_seq, descriptors, 4, POLLIN);
        for (int i = 0; i < used; ++i) {
            auto *notifier = new QSocketNotifier(descriptors[i].fd, QSocketNotifier::Read, this);
            connect(notifier, &QSocketNotifier::activated, this, &MidiController::readSequencer);
            m_notifiers.append(notifier);
        }
    }

    attachDevice();
}

void MidiController::closeSequencer()
{
    for (QSocketNotifier *notifier : m_notifiers)
        delete notifier;
    m_notifiers.clear();

    if (m_seq) {
        snd_seq_close(m_seq);
        m_seq = nullptr;
    }

    m_port = -1;
    m_attachedClient = -1;
    m_attachedPort = -1;
    m_deviceName.clear();
    setConnected(false);
}

bool MidiController::attachDevice()
{
    if (!m_seq || m_port < 0)
        return false;

    const int self = snd_seq_client_id(m_seq);
    snd_seq_client_info_t *clientInfo = nullptr;
    snd_seq_port_info_t *portInfo = nullptr;
    snd_seq_client_info_alloca(&clientInfo);
    snd_seq_port_info_alloca(&portInfo);

    snd_seq_client_info_set_client(clientInfo, -1);
    while (snd_seq_query_next_client(m_seq, clientInfo) >= 0) {
        const int client = snd_seq_client_info_get_client(clientInfo);
        if (client == 0 || client == self)
            continue;

        const QString clientName = QString::fromUtf8(snd_seq_client_info_get_name(clientInfo));
        if (!clientName.contains(m_deviceFilter, Qt::CaseInsensitive))
            continue;

        snd_seq_port_info_set_client(portInfo, client);
        snd_seq_port_info_set_port(portInfo, -1);
        while (snd_seq_query_next_port(m_seq, portInfo) >= 0) {
            const int port = snd_seq_port_info_get_port(portInfo);
            const unsigned int caps = snd_seq_port_info_get_capability(portInfo);
            if (!(caps & SND_SEQ_PORT_CAP_READ) || !(caps & SND_SEQ_PORT_CAP_SUBS_READ))
                continue;
            if (client == m_attachedClient && port == m_attachedPort)
                return true; // already listening

            if (snd_seq_connect_from(m_seq, m_port, client, port) < 0)
                continue;

            m_attachedClient = client;
            m_attachedPort = port;
            if (m_deviceName != clientName) {
                m_deviceName = clientName;
                emit connectedChanged();
            }
            if (loggingEnabled()) {
                qInfo() << "MidiController: attached to" << clientName
                        << QStringLiteral("port %1:%2").arg(client).arg(port);
            }
            setConnected(true);
            return true;
        }
    }
    return false;
}

void MidiController::readSequencer()
{
    if (!m_seq)
        return;

    while (snd_seq_event_input_pending(m_seq, 1) > 0) {
        snd_seq_event_t *event = nullptr;
        if (snd_seq_event_input(m_seq, &event) < 0 || !event)
            break;

        switch (event->type) {
        case SND_SEQ_EVENT_PORT_START:
        case SND_SEQ_EVENT_PORT_CHANGE:
            attachDevice();
            break;
        case SND_SEQ_EVENT_PORT_EXIT:
            if (event->data.addr.client == m_attachedClient) {
                m_attachedClient = -1;
                m_attachedPort = -1;
                m_deviceName.clear();
                setConnected(false);
                attachDevice();
            }
            break;
        case SND_SEQ_EVENT_CONTROLLER:
            if (m_enabled) {
                if (loggingEnabled()) {
                    qInfo() << "MIDI CC  ch=" << event->data.control.channel
                            << "num=" << event->data.control.param
                            << "val=" << event->data.control.value;
                }
                handleController(event->data.control.channel, event->data.control.param,
                                 event->data.control.value);
            }
            break;
        case SND_SEQ_EVENT_NOTEON:
        case SND_SEQ_EVENT_NOTEOFF: {
            const int velocity = event->type == SND_SEQ_EVENT_NOTEOFF
                                     ? 0
                                     : event->data.note.velocity;
            if (m_enabled) {
                if (loggingEnabled()) {
                    qInfo() << "MIDI NOTE ch=" << event->data.note.channel
                            << "num=" << event->data.note.note << "val=" << velocity;
                }
                handleNote(event->data.note.channel, event->data.note.note, velocity);
            }
            break;
        }
        default:
            break;
        }
    }
}

void MidiController::handleController(int channel, int number, int value)
{
    if (channel < 0 || channel > 15)
        return;

    for (int i = 0; i < FaderCount; ++i) {
        FaderBinding &fader = m_faders[i];
        if (channel != fader.channel)
            continue;
        // The MSB controller is paired with its LSB partner at cc + 0x20.
        if (number != fader.cc && number != fader.cc + 0x20)
            continue;

        if (number == fader.cc) {
            fader.msb = value;
            fader.haveMsb = true;
        } else {
            fader.lsb = value;
            fader.haveLsb = true;
        }
        if (!fader.haveMsb || !fader.haveLsb)
            return;

        // The two bytes arrive as a pair (MSB first on this hardware). Acting
        // only when the combined 14-bit value really moves keeps the pair from
        // being applied twice.
        const int raw = (fader.msb << 7) | fader.lsb;
        if (fader.applied && raw == fader.lastApplied)
            return;
        fader.lastApplied = raw;
        fader.applied = true;

        applyFader(fader, raw);
        return;
    }
}

void MidiController::handleNote(int channel, int note, int velocity)
{
    if (velocity <= 0) // releases arrive as a note-on with velocity 0
        return;
    if (channel != m_playChannel || note != m_playNote)
        return;
    if (m_player)
        m_player->togglePlayPause();
}

void MidiController::applyFader(FaderBinding &fader, int raw)
{
    if (!m_player)
        return;

    if (!takeOver(fader, raw, targetRawFor(fader)))
        return;

    switch (fader.action) {
    case FaderBinding::Tempo: {
        const qreal tempo = tempoFromRaw(raw);
        if (qFuzzyCompare(tempo, m_player->tempo()))
            return;
        m_applying = true;
        m_player->setTempo(tempo);
        m_applying = false;
        break;
    }
    case FaderBinding::KeyShift: {
        const int semitones = keyShiftFromRaw(raw);
        if (semitones == m_player->pitch())
            return;
        m_applying = true;
        m_player->setPitch(semitones);
        m_applying = false;

        // Record the key against the loaded song so it lands in the singer's
        // history - but only once the fader settles, otherwise a single sweep
        // would rewrite the queue table more than a hundred times.
        m_pendingKeyShift = semitones;
        m_keyShiftTimer.start(kKeyPersistDelayMs);
        break;
    }
    case FaderBinding::Volume: {
        const int volume = volumeFromRaw(raw);
        if (volume == m_player->volume())
            return;
        m_applying = true;
        m_player->setVolume(volume);
        m_applying = false;
        break;
    }
    }
}

int MidiController::targetRawFor(const FaderBinding &fader) const
{
    if (!m_player)
        return kFaderCentre;
    switch (fader.action) {
    case FaderBinding::Tempo:
        return rawFromTempo(m_player->tempo());
    case FaderBinding::KeyShift:
        return rawFromKeyShift(m_player->pitch());
    case FaderBinding::Volume:
        return rawFromVolume(m_player->volume());
    }
    return kFaderCentre;
}

bool MidiController::takeOver(FaderBinding &fader, int raw, int targetRaw)
{
    if (!fader.softTakeover || fader.armed) {
        fader.armed = true;
        fader.lastRaw = raw;
        return true;
    }

    // The app moves the tempo and key on its own (song loads, the on-screen
    // sliders, a per-song key shift), so the fader is normally out of step with
    // the value it is supposed to be driving. Stay disarmed until the fader
    // travels through the app's current value, otherwise the first touch would
    // snap the tempo or the key to wherever the fader happened to be parked.
    const int previous = fader.lastRaw;
    fader.lastRaw = raw;

    const bool crossed = previous >= 0
        && ((previous < targetRaw && raw >= targetRaw)
            || (previous > targetRaw && raw <= targetRaw));
    if (!crossed && qAbs(raw - targetRaw) > kTakeoverWindow)
        return false;

    fader.armed = true;
    return true;
}

void MidiController::resetFader(FaderBinding &fader)
{
    fader.armed = false;
    fader.lastRaw = -1;
}

void MidiController::resetAllFaders()
{
    for (int i = 0; i < FaderCount; ++i)
        resetFader(m_faders[i]);
}

void MidiController::persistKeyShift()
{
    if (!m_queue || !m_player)
        return;

    const QString path = m_player->filePath();
    if (path.isEmpty())
        return;

    for (int row = 0; row < m_queue->rowCount(); ++row) {
        if (m_queue->filePathAt(row) != path)
            continue;
        if (m_queue->keyShiftAt(row) != m_pendingKeyShift)
            m_queue->setKeyShift(row, m_pendingKeyShift);
    }

    emit keyShiftCommitted(m_pendingKeyShift);
}

void MidiController::onPlayerSourceChanged()
{
    resetAllFaders();
}

void MidiController::onPlayerTempoChanged()
{
    if (!m_applying)
        resetFader(m_faders[LeftFader]);
}

void MidiController::onPlayerPitchChanged()
{
    if (!m_applying)
        resetFader(m_faders[RightFader]);
}

void MidiController::onPlayerVolumeChanged()
{
    if (!m_applying)
        resetFader(m_faders[VolumeFader]);
}

void MidiController::setConnected(bool connected)
{
    if (m_connected == connected)
        return;
    m_connected = connected;
    if (!connected)
        resetAllFaders();
    emit connectedChanged();
}

void MidiController::loadBindings()
{
    QSettings settings;

    m_faders[LeftFader].channel = settings.value(QStringLiteral("midi/leftFaderChannel"),
                                                 m_faders[LeftFader].channel).toInt();
    m_faders[LeftFader].cc = settings.value(QStringLiteral("midi/leftFaderController"),
                                            m_faders[LeftFader].cc).toInt();
    m_faders[RightFader].channel = settings.value(QStringLiteral("midi/rightFaderChannel"),
                                                  m_faders[RightFader].channel).toInt();
    m_faders[RightFader].cc = settings.value(QStringLiteral("midi/rightFaderController"),
                                             m_faders[RightFader].cc).toInt();
    m_faders[VolumeFader].channel = settings.value(QStringLiteral("midi/volumeFaderChannel"),
                                                   m_faders[VolumeFader].channel).toInt();
    m_faders[VolumeFader].cc = settings.value(QStringLiteral("midi/volumeFaderController"),
                                              m_faders[VolumeFader].cc).toInt();
    m_faders[VolumeFader].softTakeover = settings.value(QStringLiteral("midi/volumeSoftTakeover"),
                                                        m_faders[VolumeFader].softTakeover).toBool();

    m_playChannel = settings.value(QStringLiteral("midi/rightPlayChannel"),
                                   m_playChannel).toInt();
    m_playNote = settings.value(QStringLiteral("midi/rightPlayNote"),
                                m_playNote).toInt();
    m_deviceFilter = settings.value(QStringLiteral("midi/deviceFilter"),
                                    m_deviceFilter).toString();
}

int MidiController::rawFromTempo(qreal tempo)
{
    const qreal clamped = qBound(kTempoMin, tempo, kTempoMax);
    if (clamped <= 1.0)
        return qRound((clamped - kTempoMin) / (1.0 - kTempoMin) * kFaderCentre);
    return qRound(kFaderCentre
                  + (clamped - 1.0) / (kTempoMax - 1.0) * (kFaderMax - kFaderCentre));
}

qreal MidiController::tempoFromRaw(int raw)
{
    if (qAbs(raw - kFaderCentre) <= kDeadzone)
        return 1.0; // the detent means "no change"
    if (raw < kFaderCentre)
        return kTempoMin + qreal(raw) / kFaderCentre * (1.0 - kTempoMin);
    return 1.0 + qreal(raw - kFaderCentre) / (kFaderMax - kFaderCentre) * (kTempoMax - 1.0);
}

int MidiController::rawFromKeyShift(int semitones)
{
    return qRound(kFaderCentre
                  + qreal(semitones) / kMaxSemitones * (kFaderMax - kFaderCentre));
}

int MidiController::keyShiftFromRaw(int raw)
{
    if (qAbs(raw - kFaderCentre) <= kDeadzone)
        return 0;
    const int semitones = qRound(qreal(raw - kFaderCentre) / (kFaderMax - kFaderCentre)
                                 * kMaxSemitones);
    return qBound(-kMaxSemitones, semitones, kMaxSemitones);
}

int MidiController::rawFromVolume(int volume)
{
    return qRound(qBound(0, volume, kMaxVolume) / qreal(kMaxVolume) * kFaderMax);
}

int MidiController::volumeFromRaw(int raw)
{
    // Absolute - no deadzone, the fader position is the volume.
    return qBound(0, qRound(raw / qreal(kFaderMax) * kMaxVolume), kMaxVolume);
}
