#include "RotationController.h"
#include "SingerModel.h"
#include "SongQueueModel.h"
#include "MediaPlayerController.h"

#include <QVariantMap>

RotationController::RotationController(QObject *parent)
    : QObject(parent)
{
}

void RotationController::setSingerModel(SingerModel *model)
{
    m_singers = model;
}

void RotationController::setQueueModel(SongQueueModel *model)
{
    m_queue = model;
}

void RotationController::setPlayer(MediaPlayerController *player)
{
    m_player = player;
}

void RotationController::setCurrentSinger(const QString &name)
{
    if (m_currentSinger == name)
        return;

    m_currentSinger = name;

    // The queue panel always shows the current singer's personal queue.
    if (m_queue)
        m_queue->setSelectedSingerName(name);

    emit currentSingerChanged();
}

int RotationController::nextEligibleIndex(int fromIndex) const
{
    if (!m_singers || !m_queue)
        return -1;

    const int count = m_singers->rowCount();
    if (count <= 0)
        return -1;

    if (fromIndex < 0)
        fromIndex = 0;

    for (int offset = 0; offset < count; ++offset) {
        const int i = (fromIndex + offset) % count;
        if (m_singers->statusAt(i) != QStringLiteral("Active"))
            continue;
        if (!m_queue->hasUnplayedFor(m_singers->nameAt(i)))
            continue;
        return i;
    }
    return -1;
}

void RotationController::skipCurrentSinger()
{
    if (!m_singers || !m_queue || !m_player)
        return;

    const QString skipped = m_currentSinger;
    if (skipped.isEmpty())
        return;

    // Move them to the bottom exactly as if they had sung...
    const int idx = m_singers->indexOfName(skipped);
    if (idx >= 0)
        m_singers->moveToBottom(idx);

    // ...but the song they were singing still counts as unplayed, and playing
    // stops without emitting songFinished (so no second advance).
    m_player->stop();

    // Whoever is now at the top of the rotation is selected, but their song is
    // never started for them: the host picks what they will actually sing.
    const int next = nextEligibleIndex(0);
    if (next < 0 || m_singers->nameAt(next) == skipped)
        return;

    setCurrentSinger(m_singers->nameAt(next));
}

void RotationController::advance()
{
    if (!m_singers || !m_queue || !m_player)
        return;

    const QString finishedSinger = m_currentSinger;
    if (finishedSinger.isEmpty())
        return;

    // Record the finished song as played even if it was started manually. The
    // singer is passed too: two singers can queue the same file, and marking by
    // path alone would credit the wrong person.
    m_queue->markPlayedByPath(m_player->filePath(), finishedSinger);

    // Nothing left to sing: the singer drops out of the rotation.
    if (!m_queue->hasUnplayedFor(finishedSinger)) {
        const int idx = m_singers->indexOfName(finishedSinger);
        if (idx >= 0)
            m_singers->setStatus(idx, QStringLiteral("Inactive"));
    }

    // The singer who just finished goes to the bottom of the rotation.
    const int finishedIdx = m_singers->indexOfName(finishedSinger);
    if (finishedIdx >= 0)
        m_singers->moveToBottom(finishedIdx);

    // Playback of the finished song is over. Note this happens after the song
    // above was recorded as played, because stop() clears the current path.
    m_player->stop();

    // Next up is the first Active singer (from the top) with songs remaining.
    // Only the singer is selected - nothing is loaded or played for them, since
    // they may want to sing a different song than the next one in their queue.
    // Their choice is marked as sung when it actually finishes (above).
    const int next = nextEligibleIndex(0);
    if (next < 0) {
        setCurrentSinger(QString());
        return;
    }

    setCurrentSinger(m_singers->nameAt(next));
}
