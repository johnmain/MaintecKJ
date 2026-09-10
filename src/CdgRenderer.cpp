#include "CdgRenderer.h"

#include <QPainter>
#include <QFile>
#include <QDebug>
#include <QtGlobal>

namespace {
constexpr int TileColumns = 48;   // 48 * 6px = 288px active width
constexpr int TileRows = 18;      // 18 * 12px = 216px active height
constexpr int TileWidth = 6;
constexpr int TileHeight = 12;
constexpr int BorderX = 6;        // left/right border width inside 300px screen
constexpr int BorderY = 12;       // top/bottom border height inside 216px screen
}

CdgRenderer::CdgRenderer(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    m_screen = QImage(ScreenWidth, ScreenHeight, QImage::Format_RGB32);
    m_indices.resize(ScreenWidth * ScreenHeight);

    for (int i = 0; i < 16; ++i)
        m_colorTable[i] = qRgb(0, 0, 0);

    clearScreen(0);

    m_timer.setInterval(33); // ~30 fps CDG playout
    connect(&m_timer, &QTimer::timeout, this, &CdgRenderer::onTick);
}

void CdgRenderer::setSource(const QString &path)
{
    if (m_source == path)
        return;

    m_source = path;
    m_raw.clear();
    m_packetCount = 0;
    m_nextPacket = 0;
    m_positionMs = 0;
    m_transparentColor = -1;
    m_hscroll = 0;
    m_vscroll = 0;

    for (int i = 0; i < 16; ++i)
        m_colorTable[i] = qRgb(0, 0, 0);

    if (!path.isEmpty()) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            m_raw = file.readAll();
            m_packetCount = m_raw.size() / PacketSize;
        } else {
            qWarning() << "CdgRenderer: cannot open" << path;
        }
    }

    clearScreen(0);

    emit sourceChanged();
    emit positionChanged();
    update();
}

void CdgRenderer::clearScreen(int colorIndex)
{
    const quint8 index = quint8(colorIndex & 0x0f);
    m_indices.fill(index);
    m_screen.fill(m_colorTable[index]);
}

void CdgRenderer::plot(int x, int y, int colorIndex, bool xorMode)
{
    if (x < 0 || y < 0 || x >= ScreenWidth || y >= ScreenHeight)
        return;

    const int pos = y * ScreenWidth + x;
    int newIndex;

    if (xorMode) {
        newIndex = (m_indices[pos] ^ (colorIndex & 0x0f)) & 0x0f;
    } else {
        newIndex = colorIndex & 0x0f;
    }

    m_indices[pos] = quint8(newIndex);
    reinterpret_cast<QRgb *>(m_screen.scanLine(y))[x] = m_colorTable[newIndex];
}

void CdgRenderer::drawTile(const unsigned char *data, bool xorMode)
{
    const int color0 = data[0] & 0x0f;
    const int color1 = data[1] & 0x0f;
    const int row = data[2] & 0x1f;
    const int column = data[3] & 0x3f;

    if (row < 0 || row >= TileRows || column < 0 || column >= TileColumns)
        return;

    const int x0 = column * TileWidth + m_hscroll;
    const int y0 = row * TileHeight + m_vscroll;

    if (x0 < 0 || y0 < 0 || x0 + TileWidth > ScreenWidth || y0 + TileHeight > ScreenHeight)
        return;

    // Each tile row is one byte; the low 6 bits hold the 6 pixels (MSB = leftmost).
    // A set bit selects color1 and a clear bit selects color0 (matches FFmpeg cdgraphics).
    for (int r = 0; r < TileHeight; ++r) {
        const int bits = data[4 + r];
        for (int c = 0; c < TileWidth; ++c) {
            const bool set = (bits >> (5 - c)) & 1;
            plot(x0 + c, y0 + r, set ? color1 : color0, xorMode);
        }
    }
}

void CdgRenderer::memoryPreset(const unsigned char *data)
{
    // Only the first packet of a repeated command (repeat == 0) is executed.
    if ((data[1] & 0x0f) != 0)
        return;

    clearScreen(data[0] & 0x0f);
}

void CdgRenderer::borderPreset(const unsigned char *data)
{
    if ((data[1] & 0x0f) != 0)
        return;

    const int color = data[0] & 0x0f;

    // Top and bottom borders span the full width.
    for (int x = 0; x < ScreenWidth; ++x) {
        for (int y = 0; y < BorderY; ++y)
            plot(x, y, color, false);
        for (int y = ScreenHeight - BorderY; y < ScreenHeight; ++y)
            plot(x, y, color, false);
    }

    // Side borders cover the vertical middle only.
    for (int y = BorderY; y < ScreenHeight - BorderY; ++y) {
        for (int x = 0; x < BorderX; ++x)
            plot(x, y, color, false);
        for (int x = ScreenWidth - BorderX; x < ScreenWidth; ++x)
            plot(x, y, color, false);
    }
}

void CdgRenderer::scrollScreen(int colorIndex, int hscroll, int vscroll)
{
    if (hscroll == 0 && vscroll == 0)
        return;

    const QImage previous = m_screen.copy();
    const QVector<quint8> previousIndices = m_indices;

    clearScreen(colorIndex);

    QPainter painter(&m_screen);
    painter.drawImage(hscroll, vscroll, previous);
    painter.end();

    // Mirror the scroll into the index buffer used by XOR tiles.
    for (int y = 0; y < ScreenHeight; ++y) {
        const int srcY = y - vscroll;
        if (srcY < 0 || srcY >= ScreenHeight)
            continue;
        for (int x = 0; x < ScreenWidth; ++x) {
            const int srcX = x - hscroll;
            if (srcX < 0 || srcX >= ScreenWidth)
                continue;
            m_indices[y * ScreenWidth + x] = previousIndices[srcY * ScreenWidth + srcX];
        }
    }
}

void CdgRenderer::loadColorTable(const unsigned char *data, int offset)
{
    for (int i = 0; i < 8; ++i) {
        const int hi = data[i * 2] & 0x3f;
        const int lo = data[i * 2 + 1] & 0x3f;
        const int value = (hi << 6) | lo;

        const int r = (value >> 8) & 0x0f;
        const int g = (value >> 4) & 0x0f;
        const int b = value & 0x0f;

        m_colorTable[offset + i] = qRgb(r * 17, g * 17, b * 17);
    }

    // Repaint existing pixels with the new palette.
    for (int y = 0; y < ScreenHeight; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(m_screen.scanLine(y));
        for (int x = 0; x < ScreenWidth; ++x)
            line[x] = m_colorTable[m_indices[y * ScreenWidth + x]];
    }
}

void CdgRenderer::processPacket(const unsigned char *packet)
{
    if (packet[0] != 0x09)
        return;

    const int instruction = packet[1] & 0x3f;
    const unsigned char *data = packet + 4;

    switch (instruction) {
    case 1: // Memory Preset
        memoryPreset(data);
        break;
    case 2: // Border Preset
        borderPreset(data);
        break;
    case 6: // Tile Block (Normal)
        drawTile(data, false);
        break;
    case 38: // Tile Block (XOR)
        drawTile(data, true);
        break;
    case 20: // Scroll Preset
    case 24: { // Scroll Copy
        const int color = data[0] & 0x0f;
        const int hscmd = (data[1] & 0x30) >> 4;
        const int vscmd = (data[2] & 0x30) >> 4;
        const int hOff = qMin(data[1] & 0x07, BorderX - 1);
        const int vOff = qMin(data[2] & 0x0f, BorderY - 1);

        int hinc = hOff - m_hscroll;
        int vinc = m_vscroll - vOff;
        m_hscroll = hOff;
        m_vscroll = vOff;

        if (vscmd == 2) vinc -= TileHeight; // up
        if (vscmd == 1) vinc += TileHeight; // down
        if (hscmd == 2) hinc -= TileWidth;  // left
        if (hscmd == 1) hinc += TileWidth;  // right

        scrollScreen(color, hinc, vinc);
        break;
    }
    case 28: // Define Transparent Color
        m_transparentColor = data[0] & 0x0f;
        break;
    case 30: // Load Color Table (0-7)
        loadColorTable(data, 0);
        break;
    case 31: // Load Color Table (8-15)
        loadColorTable(data, 8);
        break;
    default:
        break;
    }
}

void CdgRenderer::reset()
{
    m_nextPacket = 0;
    m_positionMs = 0;
    m_transparentColor = -1;
    m_hscroll = 0;
    m_vscroll = 0;

    for (int i = 0; i < 16; ++i)
        m_colorTable[i] = qRgb(0, 0, 0);

    clearScreen(0);

    emit positionChanged();
    update();
}

void CdgRenderer::start()
{
    if (m_running)
        return;
    m_running = true;
    m_timer.start();
    emit runningChanged();
}

void CdgRenderer::stop()
{
    if (!m_running)
        return;
    m_running = false;
    m_timer.stop();
    emit runningChanged();
}

void CdgRenderer::onTick()
{
    if (m_packetCount <= 0)
        return;

    m_positionMs += m_timer.interval();
    advanceTo(m_positionMs);
    update();
}

void CdgRenderer::advanceTo(int ms)
{
    if (m_packetCount <= 0)
        return;

    const qint64 target = qBound<qint64>(
        0, static_cast<qint64>(ms) * PacketsPerSecond / 1000, static_cast<qint64>(m_packetCount));

    if (target < static_cast<qint64>(m_nextPacket)) {
        clearScreen(0);
        m_nextPacket = 0;
    }

    const unsigned char *base = reinterpret_cast<const unsigned char *>(m_raw.constData());
    while (static_cast<qint64>(m_nextPacket) < target) {
        processPacket(base + m_nextPacket * PacketSize);
        ++m_nextPacket;
    }

    m_positionMs = ms;
}

void CdgRenderer::syncToPosition(int ms)
{
    if (m_packetCount <= 0)
        return;

    // While running, the internal clock keeps playout smooth; only correct the
    // position when the audio has drifted noticeably from it.
    if (m_running && qAbs(ms - m_positionMs) < 100)
        return;

    advanceTo(ms);

    emit positionChanged();
    update();
}

void CdgRenderer::paint(QPainter *painter)
{
    const QRectF target = boundingRect();
    painter->fillRect(target, Qt::black);

    QSizeF scaled(ScreenWidth, ScreenHeight);
    scaled.scale(target.size(), Qt::KeepAspectRatio);

    QRectF destination(0, 0, scaled.width(), scaled.height());
    destination.moveCenter(target.center());

    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->drawImage(destination, m_screen);
}
