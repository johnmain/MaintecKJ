# MaintecKJ - Native Linux Karaoke Host System Specification

## 1. System Overview & Tech Stack
- **Target OS:** Linux (Arch/CachyOS, Fedora, Ubuntu)
- **Framework:** Qt 6.x (C++20 Backend + QML UI)
- **Audio Engine:** Qt Multimedia / PipeWire / PulseAudio with SoundTouch/RubberBand DSP pipeline
- **Video/Graphics:** Qt Quick Hardware-Accelerated Rendering (Vulkan/OpenGL)
- **Database:** SQLite3 / Qt SQL Module
* **Search Tools:** Use `rg` (ripgrep) and `fd` for searching files and code patterns. Do NOT use `find` or `grep`.
  * Search code: `rg "Pattern"`
  * Search filenames: `fd "FileName"`
* **Build Artifact Exclusion:** Always exclude `build/` and `.cache/` directories when searching or listing files.

---

## 2. Window Architecture
- **Dual-Window System:**
  - **Main Window:** Primary Host DJ Workspace (Controls, Singer List, Database Search, Queue, Secondary Preview Window).
  - **Secondary Window (Display/Canvas):** Displays CDG graphic output, MP4/MKV video streams, or static background image when idle.
  - **Window Toggle:** Dedicated toolbar/header button to launch, hide, or re-target the Secondary Window to a specific monitor output.

---

## 3. Singer Rotation & Queue Management
- **Singer Data Structure:**
  - `ID`, `Name`, `Status` (Active / Inactive), `OrderIndex`
  - `SongQueue` (List of ordered `SongItem` objects)
- **Song Data Structure:**
  - `ID`, `Title`, `Artist`, `FilePath`, `Duration`, `PlayedStatus` (Played / Unplayed)
- **Rotation Rules:**
  - Each Singer contains 1 or more songs in their personal queue.
  - Playing a song sets its state to `PlayedStatus = True`.
  - When all songs for a Singer are `PlayedStatus = True`, the Singer is automatically marked `Inactive`.
  - **Rotation Engine:** Auto-advance skips `Inactive` singers. Manual overrides allow re-ordering, adding, removing, or toggling active status manually.
  - **Queue Interaction:** The active Queue Panel displays the song queue of the currently selected singer or the active singer in rotation.

---

## 4. Song Database Manager
- **Directory Management:**
  - Add, remove, and rescan multiple root directories.
  - Support automatic recursive directory traversing.
- **Naming Pattern Parser:**
  - Token-based patterns:
    - `{Artist} - {Title}`
    - `{Title} - {Artist}`
    - `{Track} - {Artist} - {Title}`
  - Custom RegEx string parser for edge cases.
- **File Format Support:**
  - Paired formats: `.cdg` + `.mp3`
  - Compressed formats: `.zip` (containing matching `.cdg` + `.mp3`)
  - Video formats: `.mp4`, `.mkv`, `.avi`
- **Database Operations:**
  - Fast indexed search across Artist and Title.
  - Rescan engine: Adds new entries and removes stale/orphaned file records from the database.

---

## 5. Main UI Layout (OpenKJ-Inspired, Resizable Panels)

The main window uses nested `SplitView` components to allow full user control over panel proportions, with min/max size constraints to protect usability. Panel dimensions are persisted across sessions via `QSettings`.

- **Outer Layout (Horizontal SplitView):**
  - **Left Panel — Singer Rotation List:**
    - `SplitView.minimumWidth: 200`
    - `SplitView.preferredWidth: 280`
    - `SplitView.maximumWidth: 450`
    - Contains: Active singer list (Add, Remove, Reorder up/down, Inactive status toggle).
  
  - **Center Panel — Main Workspace (Vertical SplitView):**
    - `SplitView.fillWidth: true` (Grows dynamically with window resize)
    - **Top Sub-Panel — Song Database View:**
      - `SplitView.minimumHeight: 200`
      - `SplitView.preferredHeight: 350`
      - `SplitView.fillHeight: true`
      - Contains: Search bar, Filter options, Track list (`Artist`, `Title`, `Duration`).
    - **Bottom Sub-Panel — Current Singer Queue View:**
      - `SplitView.minimumHeight: 150`
      - `SplitView.preferredHeight: 250`
      - Bound directly to the active singer or selected singer from the Left Panel.
      - Double-clicking a track loads it into the playback deck and begins playback.

  - **Right Panel — Deck & Secondary Preview:**
    - `SplitView.minimumWidth: 250`
    - `SplitView.preferredWidth: 320`
    - `SplitView.maximumWidth: 480`
    - Contains: Secondary Window Mini-Preview, Now Playing details, Media controls, Pitch/Key shift ($\pm 6$ semitones), and Tempo adjustments.

---

## 6. Settings & Configuration

### Audio Tab
- **Audio Output Device:** Dropdown selector for ALSA / PipeWire / PulseAudio sinks.
- **Channel Downmixing:** Mono Downmix toggle switch.
- **DSP Engine:** Master volume and Pitch/Tempo algorithm settings.

### Video / Graphics Tab
- **Hardware Acceleration:** Toggle enable/disable GPU acceleration for QML and video decoding (VA-API / NVDEC).
- **CDG Upscaling:** Option to apply bilinear/nearest-neighbor scaling filters to low-res CDG graphics.
- **Secondary Background:** Custom image selector for display when no karaoke track is playing.
- **Display Target Routing:** Monitor selection dropdown for Secondary Window output.

---

## 7. Code Editing Rules
* **Single-File Changes:** Edit or create ONLY ONE file per turn using native file-writing tools.
* **No Multi-File Bash Scripts:** NEVER execute monolithic `cat << 'EOF'` bash scripts to write multiple files simultaneously.
* **Relative Imports:** Standardize QML component imports using relative directory imports (e.g., `import "components"`) to maintain QML engine resolution stability.

---

## 8. Build & Verification Standard
* Mandatory Build Check: After every code change, immediately verify compilation by running:
  cmake -B build -S . && cmake --build build
* Verify Success: Check for [100%] Built target mainteckj-app before declaring a task complete.

---

## 9. Diagnostics & Failure Recovery
* Targeted Fixes: If the build fails:
  1. Inspect the exact compiler or QML engine error message.
  2. Modify ONLY the file causing the build failure.
  3. Do NOT rewrite unrelated QML components or reconfigure CMake URIs unless explicitly required.
* Two-Failure Stop Policy: If a build fails twice consecutively on the same issue, STOP immediately. Report the exact compiler log and request human guidance instead of attempting further automated fixes.

---

## 10. Singer Web Portal Integration (Bridge)

The desktop host is paired with a self-hosted singer portal (separate repo:
`MaintecKJ_SongRequest`, SvelteKit on the NAS, exposed through the Netbird
reverse proxy). Singers search the catalog and submit requests from their
phones; the host remains the source of truth for files and playback, and the
portal is the source of truth for singer identity (OAuth) and the request queue.

The integration is **host-initiated (pull)**, like OpenKJ: the portal never dials
the host, so the DJ machine needs no inbound port and works behind NAT.

### 10.1 Topology

```
singer phone ──HTTPS──▶ portal (NAS / Netbird)
                            ▲   │
   POST /api/host/requests/poll│  │ PATCH /api/host/requests/{id}
   POST /api/catalog/ingest    │  │
   GET  /api/host/singers      │  │
   POST /api/host/queue/push   │  │
                            └───┴── desktop host (polls on a timer)
```

All three calls are made by the host and authenticated with one shared bearer
secret, `bridgeToken`.

### 10.2 Configuration (QSettings, category `Portal`)

| Key               | Default | Meaning                                                  |
| ----------------- | ------- | -------------------------------------------------------- |
| `enabled`         | `false` | Master switch for polling and outbound calls             |
| `portalUrl`       | `""`    | Portal base URL, e.g. `https://karaoke.example.org`       |
| `bridgeToken`     | `""`    | Shared secret; sent as `Authorization: Bearer <token>`    |
| `pollIntervalMs`  | `5000`  | How often the host polls for new requests                 |
| `accepting`       | `false` | Toggle: accept song requests (reported via the poll)      |
| `autoSyncAfterExport` | `false` | Push the song list after each export (debounced)     |

`MAINTECKJ_PORTAL_TOKEN` may override `bridgeToken` for testing. For LAN testing
an `http://` URL is fine — a bare `host:port` defaults to `http://`; production
should use `https://`.

### 10.3 Poll for requests (host → portal)

`POST {portalUrl}/api/host/requests/poll`, on a `QTimer` while `enabled`.

```json
{
  "count": 1,
  "requests": [
    {
      "id": "<portal request uuid>",
      "song": { "id": "<portal song uuid>", "title": "Bohemian Rhapsody", "artist": "Queen" },
      "singer": { "id": "<portal user uuid>", "name": "Ada", "stageName": "Diva" },
      "note": "table 5",
      "requestedAt": "2026-09-28T21:58:46.083Z"
    }
  ],
  "updates": [{ "id": "<portal request uuid>", "played": false }]
}
```

- Auth: `Authorization: Bearer <bridgeToken>`; reply `401` on a mismatch.
- The portal **claims** every returned request (`delivered_at`), so the next poll
  returns nothing — a request is handed out exactly once. Persist each one (see
  §10.5) before the next poll.
- `song.files` is not sent. **Resolve the playable file(s) locally** by trimmed,
  case-insensitive `artist` + `title` against `songs` (the same rule
  `OpenKjImporter` uses for path mapping). If several files match, show them all
  at triage and let the host pick.
- Keep the poll handler cheap: store the payload; never run `ffprobe` there.
- `updates` carries singer-requested played/unplayed toggles. Apply each to the
  queue row whose `portal_request_id` matches, then report the outcome back
  (§10.4) — the portal clears the pending toggle once it agrees.
- `X-Accepting: true|false` on the poll carries the app's "Accepting requests"
  toggle. The poll is the heartbeat; the portal exposes the result publicly at
  `GET {portalUrl}/api/status` so the website can show/hide the request link.

### 10.4 Outbound calls (host → portal)

- **Catalog sync** — `POST {portalUrl}/api/catalog/ingest`, body is exactly the
  `SongListExporter` output (`[{ "Artist": ..., "Title": ... }]`,
  `Authorization: Bearer <bridgeToken>`). Response:
  `{ received, imported, created, updated, skipped }`. There is **no separate
  export format** to maintain — this is the same file the song-book workflow
  already writes, so `PortalClient::syncSongList()` calls `SongListExporter` and
  posts its bytes unchanged.
- **Status update** — `PATCH {portalUrl}/api/host/requests/{id}`, body
  `{ "status": "approved" | "playing" | "played" | "rejected" }`. Response:
  `{ request, historyRecorded }`. Marking `played` records the song in the
  singer's history on the portal, exactly once.
- **Played sync** — when a toggle from `updates` is applied, or the rotation
  marks a queue row played/unplayed, `PATCH` `played` / `approved`. The portal
  stores `host_played` and clears the singer's pending toggle.
- **Health** — `GET {portalUrl}/api/health` (public) backs the "Test" button.
- **Singer directory** — `GET {portalUrl}/api/host/singers`, response
  `{ singers: [{ id, name, stageName }] }`. Cached in `PortalClient` (refreshed
  on a 60 s timer and after a push) to drive the "In Request DB" indicator.
- **Push a singer's queue** — `POST {portalUrl}/api/host/queue/push`, body
  `{ singerName, songs: [{ title, artist, played }] }`. See §10.9.

Use `QNetworkAccessManager` (async) with a request timeout. Outbound status
updates are queued in `PortalClient` (latest wins per request) and retried on
every poll tick; the Settings status line shows the pending count while the
portal is unreachable, so a status change is never silently dropped.

### 10.5 Local data model

New table, owned by the host:

```sql
CREATE TABLE IF NOT EXISTS web_requests (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  portal_request_id TEXT UNIQUE,   -- portal uuid, echoed by status callbacks
  singer_name TEXT,                -- portal display name
  stage_name TEXT,
  artist TEXT,
  title TEXT,
  note TEXT,
  requested_at TEXT,
  received_at TEXT,
  status TEXT DEFAULT 'pending',   -- pending/approved/playing/played/rejected
  resolved_file_path TEXT,
  resolved_source TEXT,
  resolved_duration INTEGER
);
```

Requests land **pending** and are surfaced for host triage — they are never
added to the rotation automatically (consistent with the existing rule that
nothing auto-plays).

Queue rows additionally carry `portal_request_id` (added by migration) so a
portal request maps to its queue entry for the played/unplayed sync. The triage
panel passes it into `SongQueueModel::addSong`.

### 10.6 Status lifecycle

| Host action                          | Callback status |
| ------------------------------------ | --------------- |
| Request claimed on poll              | (none — portal already knows `pending`) |
| Added to a singer's queue            | `approved`      |
| Host starts the song, or it finishes | `played` (no separate `playing` state is sent) |
| Song finished / marked played        | `played`        |
| Host rejects the request             | `rejected`      |
| Singer toggles played/unplayed       | Host applies it on the next poll, then PATCHes `played` / `approved` |

### 10.7 Triage UI

A "Requests" tab/panel (like the existing library/queue panels) listing pending
web requests with the singer, artist/title, note and the matching local file
versions (`source`, `file_path`, duration). Actions: **Add to Queue** (choose a
file) and **Reject**. Add to Queue also offers a **singer picker**: queue under a
new singer named after the portal user, or pick an existing rotation singer and
rename them to the portal name (their queue rows follow, via
`SingerModel::renameSinger` → `SongQueueModel::renameSinger`) so one person keeps
a single identity. Show a pending-count badge, and reflect polling/disabled
state from the Settings tab.

### 10.8 Implementation notes

- `Qt6::Network` is already in `find_package` and `target_link_libraries`.
- Classes: `PortalClient` (`QNetworkAccessManager` + poll `QTimer`;
  `syncSongList()`, `updateRequestStatus()`, `testConnection()`,
  `refreshSingers()`, `pushSingerQueue()`, `isSingerInPortal()`),
  `WebRequestModel` (list model over `web_requests`), plus `DatabaseManager`
  methods for the table.
- Never log the token. Debounce catalog auto-sync so a rescan does not upload
  per song. The mandatory build check in §8 still applies after every change
  (`[100%] Built target mainteckj-app`).

### 10.9 Push a singer's queue (manual)

A walk-up singer who never opened the portal, or a song the host added straight
to a singer's queue, has no request in the Request DB. The queue panel shows an
**In Request DB / Not in Request DB** badge for the selected singer (driven by
the §10.4 directory) and a **Push to Portal** button, enabled only when the
singer is known and has queued songs.

`PortalClient::pushSingerQueue(singerName, songs)` sends the singer's whole
queue (`SongQueueModel::songsForSinger`) and the portal **fully reconciles** its
requests for that singer: each queued song becomes an `approved`, already-
delivered request (with `host_played` set from the queue), and active requests
whose song is no longer queued are removed. `played`/`rejected` requests are
never touched. Unknown name → `404`, ambiguous name → `409`; the Settings status
line shows the message and the reconcile counts. The push is manual — there is
no automatic queue sync.

Name matching is punctuation/case-insensitive on both sides; `PortalClient`
mirrors the portal's `normalizeText()` so the badge and the push agree.

### 10.10 Non-goals (v1)

- The host does not authenticate singers; it trusts the bearer token.
- No inbound HTTP listener or WebSocket: the transport is host-initiated polling
  only. A future push transport can reuse the same request payload shape.
