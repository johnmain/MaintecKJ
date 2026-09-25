#ifndef CDGRENDERER_H
#define CDGRENDERER_H

#include <QQuickPaintedItem>
#include <QImage>
#include <QByteArray>
#include <QVector>
#include <QTimer>

// CdgRenderer decodes a CD+G (.cdg) subchannel file and paints the resulting
// 300x216 frame. Frames are advanced externally (syncToPosition) so the host can
// keep graphics locked to the audio player position.
class CdgRenderer : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY sourceChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int positionMs READ positionMs NOTIFY positionChanged)
    Q_PROPERTY(QSize frameSize READ frameSize CONSTANT)

public:
    static constexpr int ScreenWidth = 300;
    static constexpr int ScreenHeight = 216;
    static constexpr int PacketSize = 24;
    static constexpr int PacketsPerSecond = 300;

    explicit CdgRenderer(QQuickItem *parent = nullptr);

    QString source() const { return m_source; }
    Q_INVOKABLE void setSource(const QString &path);

    bool loaded() const { return m_packetCount > 0; }
    bool running() const { return m_running; }
    int positionMs() const { return m_positionMs; }
    QSize frameSize() const { return QSize(ScreenWidth, ScreenHeight); }

    void paint(QPainter *painter) override;

public slots:
    Q_INVOKABLE void reset();
    Q_INVOKABLE void syncToPosition(int ms);
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void unload();

private slots:
    void onTick();

signals:
    void sourceChanged();
    void positionChanged();
    void runningChanged();

private:
    void processPacket(const unsigned char *packet);
    void drawTile(const unsigned char *data, bool xorMode);
    void memoryPreset(const unsigned char *data);
    void borderPreset(const unsigned char *data);
    void scrollScreen(int colorIndex, int hscroll, int vscroll);
    void loadColorTable(const unsigned char *data, int offset);
    void clearScreen(int colorIndex);
    void plot(int x, int y, int colorIndex, bool xorMode);
    void advanceTo(int ms);

    QByteArray m_raw;
    qsizetype m_packetCount = 0;
    qsizetype m_nextPacket = 0;

    QImage m_screen;
    QVector<quint8> m_indices;
    QRgb m_colorTable[16];
    int m_transparentColor = -1;

    QString m_source;
    int m_positionMs = 0;
    bool m_running = false;
    int m_hscroll = 0;
    int m_vscroll = 0;
    QTimer m_timer;
};

#endif // CDGRENDERER_H
