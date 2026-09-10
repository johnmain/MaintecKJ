#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

class QSocketNotifier;
class MediaPlayerController;
class SongQueueModel;

struct _snd_seq;

// Connects a Hercules DJControl Inpulse 200 MK2 to the deck.
//
// The control numbers were measured from the hardware (they also match the
// DJControl Inpulse 200 Mixxx map, which the MK2 inherits rather than replaces):
//
//   left  tempo fader  -> CC 0x08 (MSB) + 0x28 (LSB), MIDI channel 1
//   right tempo fader  -> CC 0x08 (MSB) + 0x28 (LSB), MIDI channel 2
//   right volume fader -> CC 0x00 (MSB) + 0x20 (LSB), MIDI channel 2
//   right play/pause   -> note 0x07,                   MIDI channel 2
//
// All three faders are 14-bit and centre-detented: 0 at the bottom, 16383 at the
// top and the detent sitting at 8192. The left fader drives the tempo, the right
// one the key shift and the right volume fader the output volume.
class MidiController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY connectedChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit MidiController(QObject *parent = nullptr);
    ~MidiController() override;

    void setPlayer(MediaPlayerController *player);
    void setQueueModel(SongQueueModel *queue);

    bool connected() const { return m_connected; }
    QString deviceName() const { return m_deviceName; }
    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    Q_INVOKABLE void rescan();

signals:
    void connectedChanged();
    void enabledChanged();
    // Emitted once the key fader has stopped moving, carrying the value that was
    // written to the queue rows for the loaded song.
    void keyShiftCommitted(int semitones);

private slots:
    void readSequencer();
    void persistKeyShift();
    void onPlayerSourceChanged();
    void onPlayerTempoChanged();
    void onPlayerPitchChanged();
    void onPlayerVolumeChanged();

private:
    // A 14-bit fader: an MSB controller number plus its LSB partner (cc + 0x20)
    // on one MIDI channel. The runtime state lives in the binding rather than in
    // a channel-indexed table because two controls legitimately share a channel:
    // the right pitch fader (CC 0x08) and the right volume fader (CC 0x00) are
    // both on channel 2.
    struct FaderBinding {
        enum Action { Tempo, KeyShift, Volume };

        int channel = 0;
        int cc = 0x08;
        Action action = Tempo;
        // Volume is an absolute control on a mixer, so the fader position simply
        // becomes the volume. The tempo and key faders instead have to be swept
        // through the app's current value first, so a fader left somewhere else
        // cannot throw the deck the moment it is touched.
        bool softTakeover = true;

        int msb = -1;
        int lsb = -1;
        bool haveMsb = false;
        bool haveLsb = false;
        int lastApplied = -1;
        bool applied = false;
        int lastRaw = -1;
        bool armed = false;
    };

    enum FaderIndex { LeftFader = 0, RightFader = 1, VolumeFader = 2, FaderCount = 3 };

    void openSequencer();
    void closeSequencer();
    bool attachDevice();
    void handleController(int channel, int number, int value);
    void handleNote(int channel, int note, int velocity);
    void applyFader(FaderBinding &fader, int raw);
    bool takeOver(FaderBinding &fader, int raw, int targetRaw);
    int targetRawFor(const FaderBinding &fader) const;
    void resetFader(FaderBinding &fader);
    void resetAllFaders();
    void loadBindings();
    void setConnected(bool connected);

    static int rawFromTempo(qreal tempo);
    static qreal tempoFromRaw(int raw);
    static int rawFromKeyShift(int semitones);
    static int keyShiftFromRaw(int raw);
    static int rawFromVolume(int volume);
    static int volumeFromRaw(int raw);

    static constexpr int kFaderCentre = 8192;   // both detents land here
    static constexpr int kFaderMax = 16383;
    static constexpr int kDeadzone = 200;       // detent snaps to "no change"
    static constexpr int kTakeoverWindow = 128; // soft-takeover catch radius
    static constexpr int kKeyPersistDelayMs = 400;
    static constexpr qreal kTempoMin = 0.5;
    static constexpr qreal kTempoMax = 2.0;
    static constexpr int kMaxSemitones = 6;
    static constexpr int kMaxVolume = 100;

    MediaPlayerController *m_player = nullptr;
    SongQueueModel *m_queue = nullptr;

    _snd_seq *m_seq = nullptr;
    int m_port = -1;
    QList<QSocketNotifier *> m_notifiers;

    FaderBinding m_faders[FaderCount];
    int m_playChannel = 2;
    int m_playNote = 0x07;
    QString m_deviceFilter;

    bool m_applying = false;
    bool m_connected = false;
    bool m_enabled = true;
    int m_attachedClient = -1;
    int m_attachedPort = -1;
    QString m_deviceName;

    QTimer m_keyShiftTimer;
    int m_pendingKeyShift = 0;
};
