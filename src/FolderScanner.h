#ifndef FOLDERSCANNER_H
#define FOLDERSCANNER_H

#include <QObject>
#include <QString>
#include <QDirIterator>
#include <QRegularExpression>
#include <QFileInfoList>
#include <QFileInfo>
#include <QVector>
#include <QDebug>

struct ParsedSong {
    QString artist;
    QString title;
    QString filePath;
    QString extension;
    bool isCdgPair;
    bool isVideoFile;
    bool isZipArchive;
};

class FolderScanner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList supportedExtensions READ supportedExtensions CONSTANT)
    Q_PROPERTY(QStringList supportedPatterns READ supportedPatterns CONSTANT)

public:
    explicit FolderScanner(QObject *parent = nullptr);

    // Getters
    QStringList supportedExtensions() const;
    QStringList supportedPatterns() const;

    // Core functionality
    QVector<ParsedSong> scanDirectory(const QString &directoryPath);
    ParsedSong parseFileName(const QString &fileName);
    bool isFileInSupportedPattern(const QString &fileName);

    // Pattern matching
    bool matchesPattern(const QString &fileName, const QRegularExpression &pattern);
    QString extractArtistFromPattern(const QString &fileName, const QRegularExpression &pattern);
    QString extractTitleFromPattern(const QString &fileName, const QRegularExpression &pattern);

    // File type detection
    bool isCdgPair(const QString &filePath);
    bool isVideoFile(const QString &filePath);
    bool isZipArchive(const QString &filePath);
    bool isAudioFile(const QString &filePath);

    // Directory operations
    QStringList getAllFiles(const QString &directoryPath, bool recursive = true);
    QStringList getFilesByExtension(const QString &directoryPath, const QString &extension, bool recursive = true);

    Q_INVOKABLE QString testParse(const QString &fileName);

private:
    QVector<QRegularExpression> m_patterns;

    void initializePatterns();
    bool areCdgFilesFound(const QString &directoryPath, const QString &baseName);
    QString getCompanionFilePath(const QString &directoryPath, const QString &baseName, const QString &targetExtension);
};

#endif // FOLDERSCANNER_H