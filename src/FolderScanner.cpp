#include "FolderScanner.h"
#include <QRegularExpression>
#include <QDebug>
#include <QFileInfo>
#include <QDirIterator>

namespace {

// Turns the literal text between two tokens (" - ", " / ", ...) into a regex
// fragment. Spacing is made flexible, so " - " also matches "-" or "  -  ".
QString patternSeparatorRegex(const QString &literal)
{
    // Built by hand rather than QRegularExpression::escape() on the whole
    // literal: escape() also escapes spaces, which then cannot be turned back
    // into a flexible \s*. Runs of whitespace collapse to a single \s*.
    QString regex;
    bool inSpace = false;
    for (const QChar &ch : literal) {
        if (ch.isSpace()) {
            if (!inSpace) {
                regex += QStringLiteral("\\s*");
                inSpace = true;
            }
            continue;
        }
        inSpace = false;
        regex += QRegularExpression::escape(QString(ch));
    }
    return regex;
}

} // namespace

FolderScanner::FolderScanner(QObject *parent)
    : QObject(parent)
{
    initializePatterns();
}

void FolderScanner::initializePatterns()
{
    // Pattern 1: {Track} - {Artist} - {Title} (most specific, tried first)
    m_patterns.append(QRegularExpression(QStringLiteral(R"(^(.*?)\s*-\s*(.*?)\s*-\s*(.*)$)")));
    
    // Pattern 2: {Artist} - {Title}
    m_patterns.append(QRegularExpression(QStringLiteral(R"(^(.*?)\s*-\s*(.*)$)")));
    
    // Pattern 3: {Title} - {Artist} (same textual shape as Pattern 2, kept for explicit selection)
    m_patterns.append(QRegularExpression(QStringLiteral(R"(^(.*?)\s*-\s*(.*)$)")));
}

QStringList FolderScanner::supportedExtensions() const
{
    return {".mp3", ".cdg", ".zip", ".mp4", ".mkv", ".avi"};
}

QStringList FolderScanner::supportedPatterns() const
{
    return {
        QStringLiteral("{Artist} - {Title}"),
        QStringLiteral("{Title} - {Artist}"),
        QStringLiteral("{Track} - {Artist} - {Title}")
    };
}

QVector<ParsedSong> FolderScanner::scanDirectory(const QString &directoryPath, const QString &pattern)
{
    QVector<ParsedSong> results;
    
    QDir dir(directoryPath);
    if (!dir.exists()) {
        qDebug() << "Directory does not exist:" << directoryPath;
        return results;
    }

    // Get all supported files
    QStringList supportedFiles = getAllFiles(directoryPath, true);
    
    // Process each file
    for (const QString &filePath : supportedFiles) {
        ParsedSong parsed = parseFileName(QFileInfo(filePath).fileName(), pattern);
        parsed.filePath = filePath;
        parsed.extension = QFileInfo(filePath).suffix().toLower();
        parsed.isCdgPair = isCdgPair(filePath);
        parsed.isVideoFile = isVideoFile(filePath);
        parsed.isZipArchive = isZipArchive(filePath);
        
        results.append(parsed);
    }

    return results;
}

QStringList FolderScanner::audioExtensions() const
{
    // Anything FFmpeg can decode is a candidate; what it cannot actually read is
    // rejected later by ffprobe. Keeping the list wide is the point - it should
    // not need revisiting every time FFmpeg learns a format.
    return {
        ".mp3", ".flac", ".m4a", ".m4b", ".m4p", ".aac", ".mp4", ".mka",
        ".ogg", ".oga", ".opus", ".spx",
        ".wav", ".w64", ".rf64", ".aiff", ".aif", ".aifc", ".caf", ".au", ".snd",
        ".wma", ".asf", ".ape", ".wv", ".mpc", ".tta", ".alac",
        ".amr", ".ac3", ".eac3", ".dts", ".dtshd", ".ra", ".gsm", ".voc"
    };
}

bool FolderScanner::isBackgroundAudioFile(const QString &filePath)
{
    const QString extension = QLatin1Char('.') + QFileInfo(filePath).suffix().toLower();
    return audioExtensions().contains(extension);
}

QVector<ParsedSong> FolderScanner::scanAudioDirectory(const QString &directoryPath)
{
    QVector<ParsedSong> results;

    QDir dir(directoryPath);
    if (!dir.exists()) {
        qDebug() << "Directory does not exist:" << directoryPath;
        return results;
    }

    QDirIterator iterator(directoryPath, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString filePath = iterator.next();
        if (!isBackgroundAudioFile(filePath))
            continue;

        ParsedSong parsed = parseFileName(QFileInfo(filePath).fileName());
        parsed.filePath = filePath;
        parsed.extension = QFileInfo(filePath).suffix().toLower();
        // Background files never take part in karaoke pairing or video output.
        parsed.isCdgPair = false;
        parsed.isVideoFile = false;
        parsed.isZipArchive = false;

        results.append(parsed);
    }

    return results;
}

ParsedSong FolderScanner::parseFileName(const QString &fileName, const QString &pattern)
{
    ParsedSong parsed;
    parsed.artist = "Unknown Artist";
    parsed.title = "Unknown Title";
    parsed.isCdgPair = false;
    parsed.isVideoFile = false;
    parsed.isZipArchive = false;

    const QString baseName = QFileInfo(fileName).completeBaseName().trimmed();

    auto cleanTitle = [](QString title) {
        static const QList<QRegularExpression> patterns = {
            QRegularExpression(QStringLiteral(R"(\s*[-–—]\s*karaoke\s+version\s+from\s+.+$)"),
                               QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(QStringLiteral(R"(\s*[-–—]\s*karaoke(\s+version)?\s*$)"),
                               QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(QStringLiteral(R"(\s*\(\s*karaoke(\s+version)?\s*\)\s*$)"),
                               QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(QStringLiteral(R"(\s*\[[^\]]*\]\s*$)")),
            QRegularExpression(QStringLiteral(R"(\s*[-–—]\s*(backing\s+track|instrumental|made\s+famous\s+by\s+.+)\s*$)"),
                               QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(QStringLiteral(R"(\s*\(\s*(backing\s+track|instrumental)\s*\)\s*$)"),
                               QRegularExpression::CaseInsensitiveOption)
        };

        bool changed = true;
        while (changed) {
            changed = false;
            for (const QRegularExpression &re : patterns) {
                const QString before = title;
                title.remove(re);
                title = title.trimmed();
                if (title != before)
                    changed = true;
            }
        }
        return title;
    };

    QString source;

    // Source hint 1: a trailing bracketed disc/tag, e.g. "... [#Z Karaoke]" or "... [Zoom]"
    {
        static const QRegularExpression bracketRe(QStringLiteral(R"(\[([^\]]+)\]\s*$)"));
        const QRegularExpressionMatch bracketMatch = bracketRe.match(baseName);
        if (bracketMatch.hasMatch())
            source = bracketMatch.captured(1).trimmed();
    }

    // An explicit pattern wins over the heuristics: it is what the operator
    // picked for this folder, so it is trusted to be the right split.
    QString patternArtist;
    QString patternTitle;
    QString patternSource;
    if (!pattern.isEmpty()
        && applyPattern(baseName, pattern, patternArtist, patternTitle, patternSource)) {
        parsed.artist = patternArtist;
        parsed.title = patternTitle;
        if (!patternSource.isEmpty())
            source = patternSource;
    } else {
        // Split on a spaced hyphen so artist/title separators work regardless of spacing.
        const QStringList parts = baseName.split(QRegularExpression(QStringLiteral(R"(\s+-\s+)")),
                                                 Qt::SkipEmptyParts);

        if (parts.size() >= 3) {
            // Only treat as {Track} - {Artist} - {Title} when the first part is a track number.
            static const QRegularExpression trackRe(QStringLiteral(R"(^\s*\d{1,3}\s*[\.,\)]?\s*$)"));
            const bool trackNumbered = trackRe.match(parts.at(0)).hasMatch();

            if (trackNumbered) {
                parsed.artist = parts.at(1).trimmed();
                parsed.title = parts.mid(2).join(QStringLiteral(" - ")).trimmed();
            } else {
                parsed.artist = parts.at(0).trimmed();
                parsed.title = parts.mid(1).join(QStringLiteral(" - ")).trimmed();

                // Source hint 2: a trailing " - <disc>" segment (e.g. "... - Karaoke Version from Zoom Karaoke")
                if (source.isEmpty()) {
                    const QString segment = parts.mid(2).join(QStringLiteral(" - ")).trimmed();
                    static const QRegularExpression fromRe(
                        QStringLiteral(R"(^\s*(?:karaoke\s+version\s+)?(?:from|by)\s+(.+)$)"),
                        QRegularExpression::CaseInsensitiveOption);
                    const QRegularExpressionMatch fromMatch = fromRe.match(segment);
                    if (fromMatch.hasMatch())
                        source = fromMatch.captured(1).trimmed();
                    else if (!segment.isEmpty())
                        source = segment;
                }
            }
        } else if (parts.size() == 2) {
            parsed.artist = parts.at(0).trimmed();
            parsed.title = parts.at(1).trimmed();
        } else {
            parsed.title = baseName;
        }
    }

    parsed.title = cleanTitle(parsed.title);
    parsed.artist = parsed.artist.trimmed();
    parsed.source = source;

    if (parsed.artist.isEmpty())
        parsed.artist = "Unknown Artist";
    if (parsed.title.isEmpty())
        parsed.title = baseName;

    return parsed;
}

bool FolderScanner::compilePattern(const QString &pattern, QRegularExpression &regex,
                                   QStringList &fields, QString &error) const
{
    static const QRegularExpression tokenRe(
        QStringLiteral(R"(\{(Artist|Title|Track|Source|Disc)\})"));

    fields.clear();
    error.clear();

    if (tokenRe.match(pattern).hasMatch()) {
        // Token pattern: each token becomes a capture group, the text between
        // them a separator.
        QString built = QStringLiteral("^");
        int last = 0;
        QRegularExpressionMatchIterator it = tokenRe.globalMatch(pattern);
        while (it.hasNext()) {
            const QRegularExpressionMatch token = it.next();
            built += patternSeparatorRegex(pattern.mid(last, token.capturedStart() - last));
            fields.append(token.captured(1).toLower());
            built += QStringLiteral("(.*?)");
            last = token.capturedEnd();
        }
        built += patternSeparatorRegex(pattern.mid(last));
        built += QStringLiteral("$");

        regex = QRegularExpression(built);
        if (!regex.isValid()) {
            error = regex.errorString();
            return false;
        }
        return true;
    }

    // Anything else is a regular expression: group 1 is the Artist and group 2
    // the Title, matching what the custom-pattern dialog tells the operator.
    regex = QRegularExpression(pattern);
    if (!regex.isValid()) {
        error = regex.errorString();
        return false;
    }
    if (regex.captureCount() < 2) {
        error = QStringLiteral("Needs two capture groups: (artist) and (title)");
        return false;
    }
    fields << QStringLiteral("artist") << QStringLiteral("title");
    return true;
}

bool FolderScanner::applyPattern(const QString &baseName, const QString &pattern,
                                 QString &artist, QString &title, QString &source)
{
    QRegularExpression regex;
    QStringList fields;
    QString error;
    if (!compilePattern(pattern, regex, fields, error))
        return false;

    const QRegularExpressionMatch match = regex.match(baseName);
    if (!match.hasMatch())
        return false;

    artist.clear();
    title.clear();
    source.clear();
    for (int i = 0; i < fields.size(); ++i) {
        const QString value = match.captured(i + 1).trimmed();
        const QString field = fields.at(i);
        if (field == QLatin1String("artist"))
            artist = value;
        else if (field == QLatin1String("title"))
            title = value;
        else if (field == QLatin1String("source") || field == QLatin1String("disc"))
            source = value;
    }

    return !artist.isEmpty() || !title.isEmpty();
}

bool FolderScanner::isFileInSupportedPattern(const QString &fileName)
{
    QString baseName = QFileInfo(fileName).completeBaseName();
    
    for (const QRegularExpression &pattern : m_patterns) {
        if (matchesPattern(baseName, pattern)) {
            return true;
        }
    }
    
    return false;
}

bool FolderScanner::matchesPattern(const QString &fileName, const QRegularExpression &pattern)
{
    return pattern.match(fileName).hasMatch();
}

QString FolderScanner::extractArtistFromPattern(const QString &fileName, const QRegularExpression &pattern)
{
    QRegularExpressionMatch match = pattern.match(fileName);
    if (match.hasMatch()) {
        const int capCount = pattern.captureCount();
        if (capCount >= 3)
            return match.captured(2).trimmed(); // {Track} - {Artist} - {Title}
        if (capCount >= 2)
            return match.captured(1).trimmed(); // {Artist} - {Title}
    }
    return "Unknown Artist";
}

QString FolderScanner::extractTitleFromPattern(const QString &fileName, const QRegularExpression &pattern)
{
    QRegularExpressionMatch match = pattern.match(fileName);
    if (match.hasMatch()) {
        int capCount = pattern.captureCount();
        if (capCount >= 2) {
            // For 3-part pattern (Track - Artist - Title), title is captured(3)
            if (capCount >= 3)
                return match.captured(3).trimmed();
            return match.captured(2).trimmed();
        }
    }
    return "Unknown Title";
}

bool FolderScanner::isCdgPair(const QString &filePath)
{
    QFileInfo info(filePath);
    QString baseName = info.completeBaseName();
    QString directory = info.dir().absolutePath();
    QString extension = info.suffix().toLower();
    
    if (extension == "cdg") {
        // Check for corresponding mp3 file
        return getCompanionFilePath(directory, baseName, "mp3") != "";
    } else if (extension == "mp3") {
        // Check for corresponding cdg file
        return getCompanionFilePath(directory, baseName, "cdg") != "";
    }
    
    return false;
}

bool FolderScanner::isVideoFile(const QString &filePath)
{
    QString extension = QFileInfo(filePath).suffix().toLower();
    return extension == "mp4" || extension == "mkv" || extension == "avi";
}

bool FolderScanner::isZipArchive(const QString &filePath)
{
    return QFileInfo(filePath).suffix().toLower() == "zip";
}

bool FolderScanner::isAudioFile(const QString &filePath)
{
    QString extension = QFileInfo(filePath).suffix().toLower();
    return extension == "mp3" || extension == "wav" || extension == "flac";
}

QStringList FolderScanner::getAllFiles(const QString &directoryPath, bool recursive)
{
    QStringList files;
    QDirIterator iterator(directoryPath, QDir::Files, recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
    
    while (iterator.hasNext()) {
        iterator.next();
        QString fileName = iterator.fileName();
        QString extension = QFileInfo(fileName).suffix().toLower();
        
        if (supportedExtensions().contains("." + extension)) {
            files.append(iterator.filePath());
        }
    }
    
    return files;
}

QStringList FolderScanner::getFilesByExtension(const QString &directoryPath, const QString &extension, bool recursive)
{
    QStringList files;
    QStringList extensions;
    extensions << extension;
    
    // Use QDirIterator with QDir::Files filter
    QDir::Filters filters = QDir::Files;
    if (extension == "mp3" || extension == "cdg") {
        filters = filters | QDir::Readable;
    }
    
    QDirIterator iterator(directoryPath, extensions, filters, QDirIterator::Subdirectories);
    
    while (iterator.hasNext()) {
        iterator.next();
        files.append(iterator.filePath());
    }
    
    return files;
}

bool FolderScanner::areCdgFilesFound(const QString &directoryPath, const QString &baseName)
{
    return getCompanionFilePath(directoryPath, baseName, "cdg") != "" &&
           getCompanionFilePath(directoryPath, baseName, "mp3") != "";
}

QString FolderScanner::getCompanionFilePath(const QString &directoryPath, const QString &baseName, const QString &targetExtension)
{
    QDir dir(directoryPath);
    
    // Check for exact match
    QString exactFile = baseName + "." + targetExtension;
    if (dir.exists(exactFile)) {
        return dir.absoluteFilePath(exactFile);
    }
    
    // Check for common variations
    QStringList possibleFiles = {
        baseName + "_" + targetExtension,
        baseName + "-" + targetExtension,
        baseName + targetExtension,
        baseName + "." + targetExtension
    };
    
    for (const QString &file : possibleFiles) {
        if (dir.exists(file)) {
            return dir.absoluteFilePath(file);
        }
    }
    
    return "";
}

QString FolderScanner::testParse(const QString &fileName)
{
    ParsedSong parsed = parseFileName(fileName);
    return parsed.artist + " - " + parsed.title;
}

QVariantMap FolderScanner::testPattern(const QString &fileName, const QString &pattern) const
{
    QVariantMap result;
    result[QStringLiteral("ok")] = false;
    result[QStringLiteral("artist")] = QString();
    result[QStringLiteral("title")] = QString();
    result[QStringLiteral("error")] = QString();

    if (pattern.trimmed().isEmpty()) {
        result[QStringLiteral("error")] = QStringLiteral("Enter a pattern");
        return result;
    }

    // Strip only a recognised media extension. completeBaseName() would treat
    // the dot in an extension-less example like "03. Artist - Title" as the
    // start of a suffix and throw the rest away.
    static const QStringList knownExtensions = {
        QStringLiteral("mp3"), QStringLiteral("cdg"), QStringLiteral("zip"),
        QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"),
        QStringLiteral("wav"), QStringLiteral("flac"), QStringLiteral("m4a"),
        QStringLiteral("ogg"), QStringLiteral("opus"), QStringLiteral("wma"),
        QStringLiteral("aiff"), QStringLiteral("aif"),
    };
    QString baseName = fileName.trimmed();
    const int dot = baseName.lastIndexOf(QLatin1Char('.'));
    if (dot > 0 && knownExtensions.contains(baseName.mid(dot + 1).toLower()))
        baseName = baseName.left(dot).trimmed();

    if (baseName.isEmpty()) {
        result[QStringLiteral("error")] = QStringLiteral("Enter an example filename");
        return result;
    }

    QRegularExpression regex;
    QStringList fields;
    QString error;
    if (!compilePattern(pattern, regex, fields, error)) {
        result[QStringLiteral("error")] = error;
        return result;
    }

    const QRegularExpressionMatch match = regex.match(baseName);
    if (!match.hasMatch()) {
        result[QStringLiteral("error")] = QStringLiteral("No match");
        return result;
    }

    QString artist;
    QString title;
    for (int i = 0; i < fields.size(); ++i) {
        const QString value = match.captured(i + 1).trimmed();
        const QString field = fields.at(i);
        if (field == QLatin1String("artist"))
            artist = value;
        else if (field == QLatin1String("title"))
            title = value;
    }

    result[QStringLiteral("ok")] = true;
    result[QStringLiteral("artist")] = artist;
    result[QStringLiteral("title")] = title;
    return result;
}