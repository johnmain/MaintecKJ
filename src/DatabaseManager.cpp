#include "DatabaseManager.h"
#include <QDebug>
#include <QRegularExpression>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent), m_songCount(0)
{
    initializeDatabase();
}

DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen())
        m_db.close();
}

bool DatabaseManager::initializeDatabase()
{
    m_dataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(m_dataLocation);
    if (!dir.exists())
        dir.mkpath(".");

    m_db = QSqlDatabase::addDatabase("QSQLITE", "MaintecKJ");
    m_db.setDatabaseName(m_dataLocation + "/songdatabase.db");

    if (!m_db.open()) {
        qDebug() << "Database error:" << m_db.lastError().text();
        return false;
    }

    QSqlQuery query;
    
    // Create songs table
    bool success = query.exec(
        "CREATE TABLE IF NOT EXISTS songs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "artist TEXT, "
        "title TEXT, "
        "file_path TEXT UNIQUE, "
        "duration INTEGER)"
    );
    if (!success) {
        qDebug() << "Error creating songs table:" << query.lastError().text();
        return false;
    }

    // Create indexes for fast searching
    ensureIndexes();

    // Load song count
    m_songCount = 0;
    if (query.exec("SELECT COUNT(*) FROM songs")) {
        if (query.next()) {
            m_songCount = query.value(0).toInt();
        }
    }

    emit dataLocationChanged();
    emit songCountChanged();
    
    qDebug() << "Database initialized at:" << m_dataLocation;
    return true;
}

void DatabaseManager::ensureIndexes()
{
    QSqlQuery query;
    query.exec("CREATE INDEX IF NOT EXISTS idx_artist ON songs(artist)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_title ON songs(title)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_file_path ON songs(file_path)");
}

bool DatabaseManager::addSong(const QString &artist, const QString &title, const QString &filePath, int duration)
{
    if (songExists(filePath))
        return false;

    QSqlQuery query;
    query.prepare("INSERT INTO songs (artist, title, file_path, duration) VALUES (?, ?, ?, ?)");
    query.addBindValue(artist);
    query.addBindValue(title);
    query.addBindValue(filePath);
    query.addBindValue(duration);

    if (!query.exec()) {
        qDebug() << "Error adding song:" << query.lastError().text();
        return false;
    }

    m_songCount++;
    emit songCountChanged();
    return true;
}

bool DatabaseManager::removeSong(int id)
{
    QSqlQuery query;
    query.prepare("DELETE FROM songs WHERE id = ?");
    query.addBindValue(id);

    if (!query.exec()) {
        qDebug() << "Error removing song:" << query.lastError().text();
        return false;
    }

    m_songCount--;
    emit songCountChanged();
    return true;
}

bool DatabaseManager::updateSong(int id, const QString &artist, const QString &title, const QString &filePath, int duration)
{
    QSqlQuery query;
    query.prepare("UPDATE songs SET artist = ?, title = ?, file_path = ?, duration = ? WHERE id = ?");
    query.addBindValue(artist);
    query.addBindValue(title);
    query.addBindValue(filePath);
    query.addBindValue(duration);
    query.addBindValue(id);

    if (!query.exec()) {
        qDebug() << "Error updating song:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::songExists(const QString &filePath)
{
    QSqlQuery query;
    query.prepare("SELECT id FROM songs WHERE file_path = ?");
    query.addBindValue(filePath);

    return query.exec() && query.next();
}

QVariantList DatabaseManager::searchSongs(const QString &query)
{
    if (query.isEmpty()) {
        return getAllSongs();
    }

    QSqlQuery queryObj;
    QVariantList results;
    
    // Use LIKE for partial matching on artist and title
    QString searchTerm = "%" + query + "%";
    queryObj.prepare(
        "SELECT id, artist, title, file_path, duration FROM songs "
        "WHERE artist LIKE ? OR title LIKE ? "
        "ORDER BY artist, title"
    );
    queryObj.addBindValue(searchTerm);
    queryObj.addBindValue(searchTerm);

    if (queryObj.exec()) {
        while (queryObj.next()) {
            QVariantMap song;
            song["id"] = queryObj.value("id").toInt();
            song["artist"] = queryObj.value("artist").toString();
            song["title"] = queryObj.value("title").toString();
            song["filePath"] = queryObj.value("file_path").toString();
            song["duration"] = queryObj.value("duration").toInt();
            results.append(song);
        }
    } else {
        qDebug() << "Search query error:" << queryObj.lastError().text();
    }

    return results;
}

QVariantList DatabaseManager::getAllSongs()
{
    QSqlQuery query;
    query.prepare("SELECT id, artist, title, file_path, duration FROM songs ORDER BY artist, title");
    return executeQuery(query);
}

Song DatabaseManager::getSongById(int id)
{
    Song song;
    QSqlQuery query;
    query.prepare("SELECT id, artist, title, file_path, duration FROM songs WHERE id = ?");
    query.addBindValue(id);

    if (query.exec() && query.next()) {
        song.id = query.value("id").toInt();
        song.artist = query.value("artist").toString();
        song.title = query.value("title").toString();
        song.filePath = query.value("file_path").toString();
        song.duration = query.value("duration").toInt();
    }

    return song;
}

QVariantList DatabaseManager::getSongsByArtist(const QString &artist)
{
    QSqlQuery query;
    query.prepare("SELECT id, artist, title, file_path, duration FROM songs WHERE artist = ? ORDER BY title");
    query.addBindValue(artist);
    return executeQuery(query);
}

QVariantList DatabaseManager::getSongsByTitle(const QString &title)
{
    QSqlQuery query;
    query.prepare("SELECT id, artist, title, file_path, duration FROM songs WHERE title = ? ORDER BY artist");
    query.addBindValue(title);
    return executeQuery(query);
}

bool DatabaseManager::addDirectory(const QString &directoryPath)
{
    QDir dir(directoryPath);
    if (!dir.exists())
        return false;

    QStringList supportedExtensions = {".mp3", ".cdg", ".zip", ".mp4", ".mkv"};
    
    // Simple directory scan implementation
    QList<QString> filesToProcess;
    for (const QString &file : dir.entryList(QDir::Files)) {
        for (const QString &ext : supportedExtensions) {
            if (file.endsWith(ext, Qt::CaseInsensitive)) {
                filesToProcess.append(dir.filePath(file));
                break;
            }
        }
    }

    int addedCount = 0;
    for (const QString &filePath : filesToProcess) {
        if (addFileToDatabase(filePath)) {
            addedCount++;
        }
    }

    m_songCount += addedCount;
    emit songCountChanged();
    return addedCount > 0;
}

void DatabaseManager::rescanDirectory(const QString &directoryPath)
{
    QDir dir(directoryPath);
    if (!dir.exists())
        return;

    // Remove existing songs from this directory
    // In a real implementation, we'd need to track directory origins
    qDebug() << "Rescanning directory (placeholder):" << directoryPath;
}

bool DatabaseManager::removeDirectory(const QString &directoryPath)
{
    Q_UNUSED(directoryPath)
    // In a real implementation, we'd remove songs from this directory
    qDebug() << "Remove directory (placeholder):" << directoryPath;
    return true;
}

bool DatabaseManager::addFileToDatabase(const QString &filePath)
{
    if (songExists(filePath))
        return false;

    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.baseName();
    
    // Parse artist and title from filename
    QString artist = "Unknown Artist";
    QString title = "Unknown Title";
    
    // Try different naming patterns
    QRegularExpression pattern1("(.*?)\s*-\s*(.*)");
    QRegularExpressionMatch match = pattern1.match(fileName);
    if (match.hasMatch()) {
        artist = match.captured(1).trimmed();
        title = match.captured(2).trimmed();
    }

    // If no match, use filename as title
    if (artist == "Unknown Artist") {
        title = fileName;
    }

    int duration = 0; // Would need to extract actual audio duration

    return addSong(artist, title, filePath, duration);
}

QString DatabaseManager::parseSongTitle(const QString &fileName)
{
    // Implementation for parsing song title from filename
    QRegularExpression pattern1("(.*?)\s*-\s*(.*)");
    QRegularExpressionMatch match = pattern1.match(fileName);
    if (match.hasMatch()) {
        return match.captured(2).trimmed();
    }
    return fileName;
}

QString DatabaseManager::parseArtistFromTitle(const QString &title)
{
    // Implementation for parsing artist from title
    QRegularExpression pattern1("(.*?)\s*-\s*(.*)");
    QRegularExpressionMatch match = pattern1.match(title);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }
    return "Unknown Artist";
}

QString DatabaseManager::parseTitleFromArtist(const QString &artist)
{
    // Implementation for parsing title from artist pattern
    QRegularExpression pattern1("(.*?)\s*-\s*(.*)");
    QRegularExpressionMatch match = pattern1.match(artist);
    if (match.hasMatch()) {
        return match.captured(2).trimmed();
    }
    return artist;
}

void DatabaseManager::createTestData()
{
    // Clear existing data
    QSqlQuery clearQuery;
    clearQuery.exec("DELETE FROM songs");

    // Add test songs
    QStringList artists = {"Artist 1", "Artist 2", "Artist 3"};
    QStringList titles = {"Song Title 1", "Song Title 2", "Song Title 3"};
    
    for (int i = 0; i < 9; ++i) {
        QString artist = artists[i % artists.size()];
        QString title = titles[i % titles.size()];
        QString filePath = "/test/songs/" + QString::number(i) + ".mp3";
        addSong(artist, title, filePath, 180 + i * 30);
    }

    qDebug() << "Test data created:" << songCount() << "songs";
}

QVariantList DatabaseManager::executeQuery(QSqlQuery &query)
{
    QVariantList results;

    if (query.exec()) {
        while (query.next()) {
            QVariantMap song;
            song["id"] = query.value("id").toInt();
            song["artist"] = query.value("artist").toString();
            song["title"] = query.value("title").toString();
            song["filePath"] = query.value("file_path").toString();
            song["duration"] = query.value("duration").toInt();
            results.append(song);
        }
    } else {
        qDebug() << "Query error:" << query.lastError().text();
    }

    return results;
}