#include "OpenKjImporter.h"

#include "DatabaseManager.h"
#include "FolderScanner.h"
#include "SingerModel.h"
#include "SongQueueModel.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QUrl>
#include <QVariantMap>
#include <QXmlStreamReader>
#include <utility>

namespace {
// The deck clamps to +/-6 semitones; OpenKJ allows a wider range.
constexpr int kMaxKeyShift = 6;
} // namespace

OpenKjImporter::OpenKjImporter(QObject *parent)
    : QObject(parent)
{
}

void OpenKjImporter::setDatabaseManager(DatabaseManager *databaseManager)
{
    m_databaseManager = databaseManager;
}

void OpenKjImporter::setSingerModel(SingerModel *singerModel)
{
    m_singerModel = singerModel;
}

void OpenKjImporter::setSongQueueModel(SongQueueModel *songQueueModel)
{
    m_songQueueModel = songQueueModel;
}

void OpenKjImporter::setError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit lastErrorChanged();
}

void OpenKjImporter::setSummary(const QString &message)
{
    if (m_lastSummary == message)
        return;
    m_lastSummary = message;
    emit lastSummaryChanged();
}

bool OpenKjImporter::parseJson(const QByteArray &data, QList<ImportedSinger> &out)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray())
        return false;

    const QJsonArray singerArray = document.array();
    for (const QJsonValue &singerValue : singerArray) {
        const QJsonObject singerObject = singerValue.toObject();
        const QString name = singerObject.value(QStringLiteral("name")).toString().trimmed();
        if (name.isEmpty())
            continue;

        ImportedSinger singer;
        singer.name = name;

        const QJsonArray songArray = singerObject.value(QStringLiteral("songs")).toArray();
        for (const QJsonValue &songValue : songArray) {
            const QJsonObject songObject = songValue.toObject();

            ImportedSong song;
            song.artist = songObject.value(QStringLiteral("artist")).toString().trimmed();
            song.title = songObject.value(QStringLiteral("title")).toString().trimmed();
            song.filePath = songObject.value(QStringLiteral("filepath")).toString().trimmed();
            song.keyShift = qBound(-kMaxKeyShift,
                                   songObject.value(QStringLiteral("keychange")).toInt(),
                                   kMaxKeyShift);

            if (song.artist.isEmpty() && song.title.isEmpty())
                continue;

            singer.songs.append(song);
        }

        out.append(singer);
    }

    return true;
}

bool OpenKjImporter::parseXml(const QByteArray &data, QList<ImportedSinger> &out)
{
    QXmlStreamReader reader(data);
    ImportedSinger singer;
    bool insideSinger = false;

    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.hasError())
            break;

        if (reader.isStartElement()) {
            if (reader.name() == QStringLiteral("singer")) {
                singer = ImportedSinger();
                singer.name = reader.attributes().value(QStringLiteral("name")).toString().trimmed();
                insideSinger = !singer.name.isEmpty();
            } else if (insideSinger && reader.name() == QStringLiteral("song")) {
                const QXmlStreamAttributes attributes = reader.attributes();

                ImportedSong song;
                song.artist = attributes.value(QStringLiteral("artist")).toString().trimmed();
                song.title = attributes.value(QStringLiteral("title")).toString().trimmed();
                // No path in the XML flavour: OpenKJ looks the disc id up in its
                // own database, which we do not have. Resolution falls back to
                // matching artist and title against our library.
                song.keyShift = qBound(-kMaxKeyShift,
                                       attributes.value(QStringLiteral("key")).toInt(),
                                       kMaxKeyShift);

                if (!song.artist.isEmpty() || !song.title.isEmpty())
                    singer.songs.append(song);
            }
        } else if (reader.isEndElement() && reader.name() == QStringLiteral("singer")) {
            if (insideSinger)
                out.append(singer);
            insideSinger = false;
        }
    }

    return !reader.hasError();
}

bool OpenKjImporter::resolveInLibrary(const QString &artist, const QString &title,
                                      QString &path, int &duration, QString &source) const
{
    if (!m_databaseManager || title.isEmpty())
        return false;

    const QVariantList candidates = m_databaseManager->searchSongs(title);
    for (const QVariant &candidate : candidates) {
        const QVariantMap row = candidate.toMap();
        if (row.value(QStringLiteral("artist")).toString().compare(artist, Qt::CaseInsensitive) != 0)
            continue;
        if (row.value(QStringLiteral("title")).toString().compare(title, Qt::CaseInsensitive) != 0)
            continue;

        path = row.value(QStringLiteral("filePath")).toString();
        duration = row.value(QStringLiteral("duration")).toInt();
        source = row.value(QStringLiteral("source")).toString();
        return true;
    }

    return false;
}

bool OpenKjImporter::importFromFile(const QString &filePath)
{
    setError(QString());
    setSummary(QString());

    if (!m_singerModel || !m_songQueueModel) {
        setError(tr("The importer is not connected to the singer and queue models."));
        return false;
    }

    QString localPath = filePath;
    const QUrl url(filePath);
    if (url.isLocalFile())
        localPath = url.toLocalFile();

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(tr("Could not open %1").arg(QFileInfo(localPath).fileName()));
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();

    QList<ImportedSinger> singers;
    if (!parseJson(data, singers) && !parseXml(data, singers)) {
        setError(tr("That does not look like an OpenKJ regulars export."));
        return false;
    }
    if (singers.isEmpty()) {
        setError(tr("The export contains no singers."));
        return false;
    }

    // Adding a song rebuilds the visible index from the filter, and the row we
    // just added is only addressable while it is visible.
    const QString previousSelection = m_songQueueModel->selectedSingerName();
    m_songQueueModel->setSelectedSingerName(QString());
    m_songQueueModel->beginBulkInsert();

    int singersAdded = 0;
    int singersSkipped = 0;
    int songsAdded = 0;
    int songsNotInLibrary = 0;

    for (const ImportedSinger &singer : std::as_const(singers)) {
        // Default behaviour: a singer who is already on the roster is left
        // completely alone, history included.
        if (m_singerModel->indexOfName(singer.name) >= 0) {
            ++singersSkipped;
            continue;
        }

        m_singerModel->addSinger(singer.name);
        ++singersAdded;

        for (const ImportedSong &song : std::as_const(singer.songs)) {
            QString path;
            QString source;
            QString artist = song.artist;
            QString title = song.title;
            int duration = 0;

            if (!resolveInLibrary(song.artist, song.title, path, duration, source)) {
                path = song.filePath;
                ++songsNotInLibrary;

                // Not in our library, so the row keeps OpenKJ's own path and
                // fields. OpenKJ stores the title exactly as it found it, which
                // leaves the provider tag on the end of it - our own scan strips
                // that into the Source column, so an imported row should not look
                // different from a scanned one.
                FolderScanner scanner;
                const ParsedSong parsed =
                    scanner.parseFileName(QFileInfo(song.filePath).fileName());

                if (!parsed.title.isEmpty()
                    && parsed.title != QStringLiteral("Unknown Title")) {
                    if (source.isEmpty())
                        source = parsed.source;

                    // Only prefer the filename's title when OpenKJ's begins with
                    // it, in which case the difference is just the trailing tag.
                    // A title OpenKJ curated stays as it is.
                    if (title.isEmpty() || title.startsWith(parsed.title))
                        title = parsed.title;

                    if (artist.isEmpty())
                        artist = parsed.artist;
                }
            }

            m_songQueueModel->addSong(singer.name, title, artist, path, duration, source);

            const int row = m_songQueueModel->rowCount() - 1;
            if (row < 0)
                continue;

            if (song.keyShift != 0)
                m_songQueueModel->setKeyShift(row, song.keyShift);

            // History, not a pending request.
            m_songQueueModel->markAsPlayed(row, true);
            ++songsAdded;
        }
    }

    m_songQueueModel->endBulkInsert();
    m_songQueueModel->setSelectedSingerName(previousSelection);

    QString summary = tr("Imported %1 singers and %2 songs.")
                          .arg(singersAdded)
                          .arg(songsAdded);
    if (singersSkipped > 0)
        summary += tr(" %1 were already present and were skipped.").arg(singersSkipped);
    if (songsNotInLibrary > 0)
        summary += tr(" %1 songs were not found in your library.")
                       .arg(songsNotInLibrary);
    setSummary(summary);

    emit importFinished();
    return true;
}
