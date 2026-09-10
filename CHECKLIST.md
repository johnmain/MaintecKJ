# MaintecKJ Development Checklist

## Phase 1: Core Layout & Dual Windows
- [x] Create project `agent.md` specification
- [x] Create `CMakeLists.txt` with Qt 6 dependencies
- [x] Implement basic `main.cpp` entrypoint
- [x] Build `Main.qml` with 3-panel `SplitView` layout
- [x] Build `SecondaryWindow.qml` with display toggle button

## Phase 2: C++ Backend Models
- [x] Create `SingerModel` (QAbstractListModel) for singer list & status
- [x] Create `SongQueueModel` for singer-specific track ordering
- [x] Implement SQLite database manager (`DatabaseManager.cpp`)
- [x] Write directory scanner and file pattern parser (`{Artist} - {Title}`)

## Phase 3: Media & DSP Pipeline
- [x] Set up Qt Multimedia `QMediaPlayer` audio pipeline
- [x] Integrate pitch change ($\pm 6$ semitones) and pitch-preserving tempo via Rubber Band (`RubberBandAudioEngine`: `QAudioDecoder` → `RubberBandStretcher` (R3) → `QAudioSink` pull mode; live key/tempo changes during playback)
- [x] Per-song key shift in the Singer Queue (Key column + /- /Reset controls, persisted in the `queue.key_shift` column and applied on playback)
- [x] Implement CDG file reader and QuickItem renderer (decoder + `QQuickPaintedItem`, synced to player position)
- [x] Moving the tempo slider keeps the reported position continuous. `position()` is `m_outputBaseMs + (processedUSecs() - m_baseUs) * tempo`, and `setTempo()` re-anchored only `m_outputBaseMs`, so the new ratio was applied to the already-elapsed output time a second time and the position leapt by *(elapsed x newTempo)* - which yanked the CDG frame (and the video sync) back or forward mid-song. `setTempo()` now re-anchors `m_outputBaseMs` **and** `m_baseUs` together, so the new ratio applies only from that instant on (verified: 0 ms jump on three changes while the rate still tracks 0.75x / 1.5x / 1.0x)

## Phase 4: Host Controls & Settings
- [x] Connect queue double-click action to player load/play
- [x] Add audio device selector dropdown to Settings (device enumeration + selection in Deck panel)
- [x] Store panel split sizes and settings in `QSettings`

## Phase 6: Library + Panel Polish
- [x] Song lengths are read with `ffprobe` while indexing, so the Duration/Time columns are filled (`DatabaseManager::rescanDirectory` used to insert a hardcoded `0`)
- [x] A rescan only measures files it has never measured: durations already in the database are reused, so an unchanged folder rescans in milliseconds
- [x] Artist and Title are equal width in both the song list and the queue (equal `Layout.horizontalStretchFactor`)
- [x] Queue columns follow the song list order: Singer, Artist, Title, Source, Key, Time
- [x] Every list has alternating row shading (theme aware)
- [x] The singer rotation list sits directly under its title - the panel's `ColumnLayout` was pushing all its spare height into the gaps, so the list now absorbs it (`Layout.fillHeight`)
- [x] The singer queue panel is 150 px taller (kept out of the saved split fraction so it cannot creep on every launch)
- [x] Editing a queue entry's key shift only stores it: the deck (right column) is transposed only when the edited entry is the song actually loaded in it, so adjusting one singer's queue never shifts whoever is singing right now (`SongQueueModel::filePathAt` + `syncDeckKey`)

## Phase 5: Singer Rotation Engine
- [x] Queue panel shows only the current singer's queue (`SongQueueModel` filters on `selectedSingerName`; all index-based edits map through the visible rows)
- [x] Clicking a singer in the Rotation list makes them the current singer and loads their queue (`RotationController.currentSinger`)
- [x] The next singer is only *selected* on advance/skip - no song is ever loaded or played automatically, so the singer can pick a different song than the next one in their queue (the host double-clicks the chosen song)
- [x] A song counts as played only once it has actually finished (or when the host marks it), never when the rotation merely moves on to the next singer
- [x] Natural song end advances the rotation (`RubberBandAudioEngine::finished` → `MediaPlayerController::songFinished` → `RotationController::advance`; the Stop button does not advance). The final input chunk is now marked as Rubber Band's `final` chunk, so a partial tail can no longer stall the flush and block the end-of-song signal. Playback then stops, leaving the next singer selected and their queue on screen
- [x] Defect fixed: the auto-advance used to mark the *incoming* singer's first song as played the moment it started it, so one ended song marked other singers' songs played
- [x] Defect fixed: the finished song is credited to the singer who actually sang it - `markPlayedByPath()` now takes the singer as well, since two singers may have queued the identical file and matching on the path alone marked the wrong person's entry
- [x] "Skip Singer to Bottom" button moves the current singer to the bottom of the rotation and moves on without marking their song as played
- [x] `finished()` is posted to the event loop instead of emitted inline (`readAudio()` runs on the GUI thread while holding `m_mutex`, so an inline handler calling `stop()`/`load()` self-deadlocked); the sink is also torn down before taking the lock in `setSource()`
- [x] The singer who just finished is moved to the bottom of the rotation list
- [x] A singer with no unplayed songs is marked `Inactive` and skipped by auto-advance (including wrap-around); adding a new song to an Inactive singer reactivates them

## DJ Controller (`MidiController`)
- [x] Hercules DJControl Inpulse 200 MK2 supported over the ALSA sequencer (Qt 6 ships no MIDI module, so `libasound` is driven directly and the fd is watched with a `QSocketNotifier` - no polling thread)
- [x] The device is found by name and re-found on hotplug via the `System:Announce` port, so it can be plugged in before or after launch
- [x] Left tempo fader drives the app tempo, right tempo fader drives the key shift, right volume fader the output volume, right play/pause toggles playback
- [x] Bindings were **measured from the hardware**, not guessed: left fader `CC 0x08`+`0x28` on MIDI channel 1, right fader the same on channel 2, right volume `CC 0x00`+`0x20` on channel 2, right play `note 0x07` on channel 2 (these match the Mixxx DJControl Inpulse 200 map, which the MK2 inherits rather than replaces - its script only remaps the jog/vinyl section). Mixxx declares the volume control `<normal/>` (7-bit) but the hardware really does send both bytes, so it is used at full 14-bit
- [x] Volume is an **absolute** control (the fader position is the volume, with no deadzone), unlike the tempo and key faders. It is also the one control that shares a channel with something else, so the fader state lives in the binding rather than a channel-indexed table - verified on hardware that a full volume sweep leaves the key and tempo untouched and vice versa
- [x] Both faders are 14-bit and centre-detented: `0` at the bottom, `16383` at the top, detent at `8192`. The MSB/LSB pair is combined and applied once, and a small deadzone around the detent snaps to exactly `1.00x` / `0` semitones
- [x] Soft takeover on both faders: because the app moves the tempo and key by itself (song loads, the on-screen sliders, a per-song key shift) the fader is normally out of step with the value it drives, so it stays disarmed until it travels through the app's current value. Without this, the first touch would snap the tempo or key to wherever the fader happened to be parked
- [x] The key fader writes the value to the loaded song's queue rows so it lands in the singer's history, debounced 400 ms so a single sweep is one write rather than a hundred
- [x] Deck panel shows the connected device, an enable/disable toggle and a rescan button; `MAINTECKJ_MIDI_LOG=1` dumps raw controller/note events

## Phase 7: Background Music Mode (NOT STARTED - specification agreed)

Goal: the app can double as a normal music player between singers, without the
singer rotation machinery getting involved.

### Modes & library
- [ ] Karaoke / Background Music selector at the top of the main window
- [ ] **A separate table in the database** (`background_songs`, with its own directory rows) - not a filtered view of `songs`. The folders are entirely different folders, so nothing is inferred from extensions or from a `.cdg` sibling
- [ ] **Add Folder** while the Background Music tab is selected indexes into that table only
- [ ] Files use the same `{Artist} - {Title}` naming convention as the karaoke library, so the existing filename parser is reused instead of reading embedded tags
- [ ] Formats: anything FFmpeg can decode. The scan should use a wide extension allow-list and let `ffprobe` reject what it cannot read, so the list is not hand-maintained and does not need updating when FFmpeg gains a format

### Playlist
- [ ] The background playlist is its **own model and table**, separate from the singer queue (`SongQueueModel`). Its rows carry no singer, so they must never reach the rotation, `hasUnplayedFor()`, `markPlayedByPath()` or the singer queue panel
- [ ] **Add All to Queue** button - adds every song in the background library (the library is a few hundred tracks, so no filter scoping is wanted)
- [ ] Double-clicking a background song adds it to the playlist
- [ ] Dragging a background song onto the queue adds it
- [ ] **Random** button reshuffles the playlist on every press (a one-shot shuffle, never a continuous/repeating one)

### Playback
- [ ] Double-clicking a playlist entry plays it and auto-advances to the next entry when the song finishes
- [ ] Auto-advance belongs to Background Music mode only - the singer rotation keeps its "select the next singer, never auto-play" behaviour

### Tab switching
- [ ] Karaoke -> Background Music: the karaoke song keeps playing
- [ ] Background Music -> Karaoke: the playing track **fades out over 5 s and stops**
- [ ] The fade must not clobber the stored volume: ramp the sink and restore the previous level afterwards, so the user's volume setting - and the DJ controller's volume fader position - still mean what they did before the fade (volume is owned by `RubberBandAudioEngine` via `m_sink->setVolume()`)

### Persistence
- [ ] The background playlist survives a restart, the way the singer queue does

### Layout
- [ ] The Singer Rotation panel stays on screen while the Background Music tab is selected

## Phase 8: OpenKJ Singer Import (COMPLETE)

Goal: bring regular singers and their song history over from OpenKJ in one step
instead of retyping them.

- [x] Import button in the singer panel, reading a JSON file from OpenKJ's "Export regulars" dialog (legacy XML accepted too)
- [x] Each singer becomes a row in `singers` (Active), and each of their songs a row in `queue` for that singer
- [x] `keychange` maps to `key_shift`
- [x] Imported songs are marked **played**, otherwise the rotation treats a singer's entire history as a pending queue
- [x] `SongQueueModel::beginBulkInsert()`/`endBulkInsert()` added: `persist()` rewrites the whole queue, so adding songs one at a time was quadratic - 200 singers and 4000 songs now import in **732 ms**
- [x] Verified by `/tmp/okj_test.cpp`: 27 assertions covering JSON, XML, re-import, key clamping, bulk scale and a missing file, all passing

### Format (JSON - top level is a bare array, no wrapper and no version field)

```json
[
  {
    "name": "Singer Name",
    "songs": [
      {
        "filepath": "/media/karaoke/SC1234/Artist - Title.mp3",
        "artist": "Artist",
        "title": "Title",
        "songid": "SC1234-05",
        "keychange": 2,
        "plays": 7,
        "lastplay": "Mon Jan 15 11:50:00 2024"
      }
    ]
  }
]
```

- Taken from OpenKJ `src/dlgregularexport.cpp`; its importer reads back exactly these keys, and also accepts the older `<singer name="..."><song discid="" artist="" title="" key=""/></singer>` XML
- `lastplay` is `QDateTime::toString()` with default arguments (`Qt::TextDate`): `ddd MMM d HH:mm:ss yyyy`, English C-locale, day **not** zero-padded, and an **empty string** when never played (verified by running it)
- `songid` is OpenKJ's internal disc id (e.g. `SC1234-05`) and means nothing outside OpenKJ
- `keychange` is in semitones and can exceed our ±6 clamp

### Decisions
- [x] **Path mapping**: artist and title are matched against our own `songs` table first (case-insensitive) and OpenKJ's `filepath` is kept only as the fallback, so the queue points at files we can actually play. Songs that match nothing are still imported, with their path left empty when OpenKJ had none, and are counted in the summary
- [x] **Duplicates**: a singer already on the roster is skipped entirely, history included
- [x] **`keychange` outside ±6**: clamped with `qBound`, not dropped
- [x] `plays`/`lastplay` are discarded - there are no columns for them

## Phase 9: Song List Export (COMPLETE)

Goal: produce the flat Artist/Title list the song-book workflow consumes, without
running a separate script over an OpenKJ filename dump.

- [x] `SongListExporter`, and an "Export Song List..." button in the library panel
- [x] Writes the whole library, leaving soft-deleted songs out
- [x] Distinct on Artist + Title, compared case-insensitively
- [x] The karaoke provider is absent from the output. It does not have to be
      regexed off: the scanner already peels a trailing `[...]` tag into the
      `source` column, and `source` never reaches the export. A stored title that
      still carries a tag would need the old `\s*\[.*?\]` rule added
- [x] Sorted by Artist then Title, both case-insensitive
- [x] Byte-identical to Python's `json.dump(..., indent=2, ensure_ascii=False)`:
      two-space indent, `Artist` before `Title`, non-ASCII kept as UTF-8
- [x] An empty library writes `[]`
- [x] Verified by `/tmp/export_test.cpp` (13 assertions) and by diffing against
      Python's output for the same six songs - the files match exactly

```json
[
  {
    "Artist": "+44",
    "Title": "When Your Heart Stops Beating"
  }
]
```
