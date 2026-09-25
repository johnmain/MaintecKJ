#ifndef FOLDERSCANNER_H
#define FOLDERSCANNER_H

#include <QObject>
#include <QString>
#include <QDirIterator>
#include <QRegularExpression>
#include <QFileInfoList>
#include <QFileInfo>
#include <QVector>
#include <QVariantMap>
#include <QDebug>

struct ParsedSong {
    QString artist;
    QString title;
    QString filePath;
    QString extension;
    QString source;
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

    // Core functionality. `pattern` is a token pattern such as
    // "{Artist} - {Title}"; an empty pattern falls back to the heuristic parser.
    QVector<ParsedSong> scanDirectory(const QString &directoryPath, const QString &pattern = QString());
    ParsedSong parseFileName(const QString &fileName, const QString &pattern = QString());
    bool isFileInSupportedPattern(const QString &fileName);

    // Background music (Phase 7). Same {Artist} - {Title} parsing, but a wide
    // audio-only extension list and none of the karaoke handling: no .cdg
    // pairing and no video.
    QVector<ParsedSong> scanAudioDirectory(const QString &directoryPath);
    QStringList audioExtensions() const;

    // Pattern matching
    bool matchesPattern(const QString &fileName, const QRegularExpression &pattern);
    QString extractArtistFromPattern(const QString &fileName, const QRegularExpression &pattern);
    QString extractTitleFromPattern(const QString &fileName, const QRegularExpression &pattern);

    // File type detection
    bool isCdgPair(const QString &filePath);
    bool isVideoFile(const QString &filePath);
    bool isZipArchive(const QString &filePath);
    bool isAudioFile(const QString &filePath);
    bool isBackgroundAudioFile(const QString &filePath);

    // Directory operations
    QStringList getAllFiles(const QString &directoryPath, bool recursive = true);
    QStringList getFilesByExtension(const QString &directoryPath, const QString &extension, bool recursive = true);

    Q_INVOKABLE QString testParse(const QString &fileName);

    // Parses `fileName` with `pattern` for the UI's live preview, returning
    // { ok, artist, title, error }. A pattern containing {Artist}/{Title} tokens
    // is a token pattern; anything else is a regular expression whose group 1 is
    // the Artist and group 2 the Title.
    Q_INVOKABLE QVariantMap testPattern(const QString &fileName, const QString &pattern) const;

private:
    QVector<QRegularExpression> m_patterns;

    // Fills artist/title/source from a token pattern such as "{Artist} - {Title}",
    // or from a plain regular expression (group 1 = Artist, group 2 = Title).
    // Returns false when the pattern cannot be used or the name does not fit.
    bool applyPattern(const QString &baseName, const QString &pattern,
                      QString &artist, QString &title, QString &source);

    // Compiles a pattern to a regex plus the field each capture group maps to.
    // Returns false and fills `error` when the pattern cannot be used.
    bool compilePattern(const QString &pattern, QRegularExpression &regex,
                        QStringList &fields, QString &error) const;

    void initializePatterns();
    bool areCdgFilesFound(const QString &directoryPath, const QString &baseName);
    QString getCompanionFilePath(const QString &directoryPath, const QString &baseName, const QString &targetExtension);
};

#endif // FOLDERSCANNER_H