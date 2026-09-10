#include "SongListExporter.h"

#include "DatabaseManager.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>

namespace {

struct ExportEntry {
    QString artist;
    QString title;
};

// Small JSON string encoder. QJsonDocument::Indented would work, but it indents
// with four spaces and this file is meant to be byte-comparable with the output
// of the existing song-book script, which uses two.
QString encodeString(const QString &value)
{
    QString out;
    out.reserve(value.size() + 2);
    out += QLatin1Char('"');

    for (const QChar ch : value) {
        switch (ch.unicode()) {
        case u'"':
            out += QLatin1String("\\\"");
            break;
        case u'\\':
            out += QLatin1String("\\\\");
            break;
        case u'\b':
            out += QLatin1String("\\b");
            break;
        case u'\f':
            out += QLatin1String("\\f");
            break;
        case u'\n':
            out += QLatin1String("\\n");
            break;
        case u'\r':
            out += QLatin1String("\\r");
            break;
        case u'\t':
            out += QLatin1String("\\t");
            break;
        default:
            if (ch.unicode() < 0x20) {
                out += QStringLiteral("\\u%1").arg(static_cast<uint>(ch.unicode()), 4, 16,
                                                   QLatin1Char('0'));
            } else {
                out += ch;
            }
            break;
        }
    }

    out += QLatin1Char('"');
    return out;
}

} // namespace

SongListExporter::SongListExporter(QObject *parent)
    : QObject(parent)
{
}

void SongListExporter::setDatabaseManager(DatabaseManager *databaseManager)
{
    m_databaseManager = databaseManager;
}

void SongListExporter::setError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit lastErrorChanged();
}

void SongListExporter::setSummary(const QString &message)
{
    if (m_lastSummary == message)
        return;
    m_lastSummary = message;
    emit lastSummaryChanged();
}

bool SongListExporter::exportToFile(const QString &filePath)
{
    setError(QString());
    setSummary(QString());
    m_lastExportedCount = 0;

    if (!m_databaseManager) {
        setError(tr("The exporter is not connected to the song database."));
        return false;
    }

    QString localPath = filePath;
    const QUrl url(filePath);
    if (url.isLocalFile())
        localPath = url.toLocalFile();
    if (localPath.isEmpty()) {
        setError(tr("No destination file was given."));
        return false;
    }

    // Deleted songs and duplicate Artist/Title pairs both stay out. The
    // comparison is case-insensitive, the way the library is searched.
    const QVariantList rows = m_databaseManager->getAllSongs(false);

    QList<ExportEntry> entries;
    QSet<QString> seen;

    for (const QVariant &value : rows) {
        const QVariantMap row = value.toMap();

        ExportEntry entry;
        entry.artist = row.value(QStringLiteral("artist")).toString().trimmed();
        entry.title = row.value(QStringLiteral("title")).toString().trimmed();

        if (entry.artist.isEmpty() && entry.title.isEmpty())
            continue;
        if (entry.artist.isEmpty())
            entry.artist = QStringLiteral("Unknown Artist");
        if (entry.title.isEmpty())
            entry.title = QStringLiteral("Unknown Title");

        const QString key = entry.artist.toLower() + QChar(0x1F) + entry.title.toLower();
        if (seen.contains(key))
            continue;

        seen.insert(key);
        entries.append(entry);
    }

    std::sort(entries.begin(), entries.end(),
              [](const ExportEntry &left, const ExportEntry &right) {
                  const int byArtist = QString::compare(left.artist, right.artist,
                                                       Qt::CaseInsensitive);
                  if (byArtist != 0)
                      return byArtist < 0;
                  return QString::compare(left.title, right.title, Qt::CaseInsensitive) < 0;
              });

    QString output;
    if (entries.isEmpty()) {
        output = QStringLiteral("[]");
    } else {
        output += QLatin1String("[\n");
        for (int i = 0; i < entries.size(); ++i) {
            const ExportEntry &entry = entries.at(i);
            output += QLatin1String("  {\n");
            output += QLatin1String("    \"Artist\": ") + encodeString(entry.artist)
                      + QLatin1String(",\n");
            output += QLatin1String("    \"Title\": ") + encodeString(entry.title)
                      + QLatin1String("\n");
            output += (i + 1 < entries.size()) ? QLatin1String("  },\n")
                                               : QLatin1String("  }\n");
        }
        output += QLatin1String("]");
    }

    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(tr("Could not write to %1").arg(QFileInfo(localPath).fileName()));
        return false;
    }

    if (file.write(output.toUtf8()) < 0) {
        file.close();
        setError(tr("Could not write to %1").arg(QFileInfo(localPath).fileName()));
        return false;
    }
    file.close();

    m_lastExportedCount = static_cast<int>(entries.size());
    setSummary(tr("Exported %1 unique songs to %2.")
                   .arg(m_lastExportedCount)
                   .arg(QFileInfo(localPath).fileName()));
    emit exportFinished();
    return true;
}
