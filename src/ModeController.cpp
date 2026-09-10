#include "ModeController.h"

#include "BackgroundPlaylistModel.h"
#include "MediaPlayerController.h"
#include "RotationController.h"

#include <QSettings>
#include <QVariantMap>

namespace {
constexpr auto kModeSettingKey = "app/mode";

QString normaliseMode(const QString &mode)
{
    // QStringLiteral cannot take a const char* variable, so the literals are
    // spelled out here rather than reusing the header constants.
    return (mode == QLatin1String("background")) ? QStringLiteral("background")
                                                  : QStringLiteral("karaoke");
}
} // namespace

ModeController::ModeController(QObject *parent)
    : QObject(parent)
{
}

void ModeController::setPlayer(MediaPlayerController *player)
{
    m_player = player;
}

void ModeController::setRotation(RotationController *rotation)
{
    m_rotation = rotation;
}

void ModeController::setBackgroundPlaylist(BackgroundPlaylistModel *playlist)
{
    m_playlist = playlist;
}

void ModeController::setMode(const QString &mode)
{
    const QString requested = normaliseMode(mode);
    if (m_mode == requested)
        return;

    const bool leavingBackground = isBackground();
    m_mode = requested;

    // Background -> Karaoke hands the room back to the singers, so the music
    // gets out of the way. The other direction is deliberately seamless:
    // whatever is playing carries on.
    if (leavingBackground && m_player)
        m_player->fadeOutAndStop(kFadeOutMs);

    QSettings settings;
    settings.setValue(QString::fromLatin1(kModeSettingKey), m_mode);
    emit modeChanged();
}

void ModeController::restore()
{
    QSettings settings;
    const QString stored = normaliseMode(
        settings.value(QString::fromLatin1(kModeSettingKey),
                       QLatin1String("karaoke")).toString());

    if (m_mode == stored)
        return;

    m_mode = stored;
    emit modeChanged();
}

void ModeController::onSongFinished()
{
    if (!m_player)
        return;

    // Karaoke: pick the next singer, but never start their song for them.
    if (!isBackground()) {
        if (m_rotation)
            m_rotation->advance();
        return;
    }

    if (!m_playlist)
        return;

    const int next = m_playlist->nextIndex();
    if (next < 0)
        return; // End of the playlist: stop rather than loop round.

    startTrackAt(next);
}

void ModeController::playBackgroundRow(int row)
{
    startTrackAt(row);
}

void ModeController::startTrackAt(int row)
{
    if (!m_playlist || !m_player)
        return;

    const QVariantMap song = m_playlist->songAt(row);
    if (song.isEmpty())
        return;

    m_playlist->setCurrentIndex(row);

    m_player->load(song.value(QStringLiteral("filePath")).toString());

    // The per-song key shift belongs to a singer's request, so it must not leak
    // into background music. Tempo is left alone - that is the operator's.
    m_player->setPitch(0);

    m_player->play();
}
