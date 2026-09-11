#!/bin/sh
# Checks the MaintecKJ database on the machine you run it on: what is in it, and
# whether the file paths it holds actually exist there.
#
# This matters because an OpenKJ regulars import matches songs against the local
# library by artist and title, and keeps OpenKJ's own file path only where it
# finds no match. Those fallback paths point at folders that existed on the old
# computer, so the singer appears, their queue appears, and nothing plays.
#
# Usage:
#   ./check-database.sh              # the usual location in $HOME
#   ./check-database.sh /path/to/songdatabase.db

set -u

DB="${1:-$HOME/.local/share/MaintecKJ/MaintecKJ/songdatabase.db}"

if [ ! -f "$DB" ]; then
    echo "No database at: $DB"
    echo "Pass the path as the first argument if it lives somewhere else."
    echo "The running app prints its location at startup, after 'Database initialized at:'."
    exit 1
fi

echo "Database: $DB"
echo

echo "=== Row counts ==="
sqlite3 -header -column "$DB" "
SELECT 'singers'           AS table_name, COUNT(*) AS rows FROM singers
UNION ALL SELECT 'queue',               COUNT(*) FROM queue
UNION ALL SELECT 'songs',               COUNT(*) FROM songs
UNION ALL SELECT 'directories',         COUNT(*) FROM directories
UNION ALL SELECT 'background_songs',    COUNT(*) FROM background_songs
UNION ALL SELECT 'background_playlist', COUNT(*) FROM background_playlist;"

echo
echo "=== Singers ==="
sqlite3 -header -column "$DB" "SELECT name, status FROM singers ORDER BY position;"

echo
echo "=== Indexed karaoke folders ==="
sqlite3 -header -column "$DB" "SELECT path FROM directories;"

echo
echo "=== Queue, as the app sees it ==="
sqlite3 -header -column "$DB" "
SELECT singer, artist, title, key_shift, is_played, file_path
FROM queue ORDER BY singer, position;"

echo
echo "=== Queue paths that do not exist on this machine ==="
echo "(nothing listed means they are all present)"
sqlite3 "$DB" "SELECT file_path FROM queue;" | while IFS= read -r path; do
    [ -z "$path" ] && continue
    [ -f "$path" ] || echo "MISSING: $path"
done

echo
echo "=== Library paths that do not exist on this machine ==="
echo "(nothing listed means they are all present)"
sqlite3 "$DB" "SELECT file_path FROM songs;" | while IFS= read -r path; do
    [ -z "$path" ] && continue
    [ -f "$path" ] || echo "MISSING: $path"
done

echo
echo "=== Queue rows that match nothing in the library ==="
echo "This is the one that explains a queue that will not play. A non-empty"
echo "result below is a queue pointing at files this machine does not have."
sqlite3 -header -column "$DB" "
SELECT q.singer, q.artist, q.title, q.file_path
FROM queue q
LEFT JOIN songs s ON s.file_path = q.file_path
WHERE s.file_path IS NULL
ORDER BY q.singer, q.position;"

echo
echo "=== Where the queue paths point ==="
echo "A prefix you do not recognise is a library that is not on this machine."
sqlite3 -header -column "$DB" "
SELECT COUNT(*) AS rows, substr(file_path, 1, 45) AS path_prefix
FROM queue GROUP BY path_prefix ORDER BY rows DESC;"
