#include "FolderScanner.h"
#include <QRegularExpression>
#include <QDebug>
#include <QFileInfo>
#include <QDirIterator>

FolderScanner::FolderScanner(QObject *parent)
    : QObject(parent)
{
    initializePatterns();
}

void FolderScanner::initializePatterns()
{
    // Pattern 1: {Artist} - {Title}
    m_patterns.append(QRegularExpression(QStringLiteral("^(.*?)\s*-\s*(.*)$")));
    
    // Pattern 2: {Title} - {Artist}
    m_patterns.append(QRegularExpression(QStringLiteral("^(.*?)\s*-\s*(.*)$")));
    
    // Pattern 3: {Track} - {Artist} - {Title}
    m_patterns.append(QRegularExpression(QStringLiteral("^(.*?)\s*-\s*(.*?)\s*-\s*(.*)$")));
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

QVector<ParsedSong> FolderScanner::scanDirectory(const QString &directoryPath)
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
        ParsedSong parsed = parseFileName(QFileInfo(filePath).fileName());
        parsed.filePath = filePath;
        parsed.extension = QFileInfo(filePath).suffix().toLower();
        parsed.isCdgPair = isCdgPair(filePath);
        parsed.isVideoFile = isVideoFile(filePath);
        parsed.isZipArchive = isZipArchive(filePath);
        
        results.append(parsed);
    }

    return results;
}

ParsedSong FolderScanner::parseFileName(const QString &fileName)
{
    ParsedSong parsed;
    parsed.artist = "Unknown Artist";
    parsed.title = "Unknown Title";
    parsed.isCdgPair = false;
    parsed.isVideoFile = false;
    parsed.isZipArchive = false;

    QString baseName = QFileInfo(fileName).completeBaseName();
    
    // Try each pattern
    for (const QRegularExpression &pattern : m_patterns) {
        if (matchesPattern(baseName, pattern)) {
            parsed.artist = extractArtistFromPattern(baseName, pattern);
            parsed.title = extractTitleFromPattern(baseName, pattern);
            break;
        }
    }

    // If no pattern matched, use the whole baseName as title
    if (parsed.title == "Unknown Title") {
        parsed.title = baseName;
    }

    return parsed;
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
        int capCount = pattern.captureCount();
        if (capCount >= 2) {
            return match.captured(1).trimmed();
        }
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
            if (capCount >= 3 && pattern.pattern().contains("-")) {
                return match.captured(capCount).trimmed();
            } else {
                return match.captured(2).trimmed();
            }
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
    QDirIterator iterator(directoryPath, {extension}, recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
    
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