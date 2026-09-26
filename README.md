# MaintecKJ

A native karaoke host for Linux — a singer rotation, a searchable song library,
CD+G and video playback, and a separate background-music mode, with a second
window for the projector or TV.

Built with Qt 6 (C++20 backend, QML UI). Inspired by
[OpenKJ](https://github.com/OpenKJ/OpenKJ), the long-standing open-source
karaoke host, and shaped to a workflow I wanted for running a show.

---

## Features

### The show

- **Singer rotation** — add, remove and drag to reorder singers; mark them
  active or inactive; skip the current singer to the bottom of the list.
- **Per-singer queue** — queue songs from the library, reorder them, mark them
  played or unplayed, and set a key shift per song. When every song a singer has
  queued is played, they drop out of the rotation automatically.
- **Background music mode** — a completely separate library and playlist, so
  break music never bleeds into the karaoke library. Tracks auto-advance, and
  switching back to Karaoke fades the music out over five seconds.
- **Multi-select everywhere** — `Ctrl`-click, `Shift`-click or `Ctrl+A` to
  select rows, then act on the whole selection (add to queue/playlist,
  delete/undelete, mark played).

### The library

- **Folder scanning** with a background indexer; track lengths are measured
  with `ffprobe` so a few hundred files don't freeze the UI.
- **Per-folder naming patterns**, defaulting to `{Artist} - {Title}`, with
  `{Title} - {Artist}` and `{Track} - {Artist} - {Title}` presets — or supply
  your own regular expression through a dialog that previews the result.
- **Soft delete** — remove a song from view without touching the file, and
  restore or purge it later.
- **OpenKJ import** — bring a regulars list across, singers and play history
  included.
- **Song list export** — plain, deduplicated Artist/Title pairs for a song book.

### The playback

- **CD+G renderer** written to the format spec, so `.cdg` graphics line up with
  the audio frame for frame.
- **Video** — `.mp4`, `.mkv` and `.avi`, sent to the secondary window.
- **Rubber Band DSP** — tempo from `0.5x` to `2.0x` and key shift of ±6
  semitones, applied live.
- **Idle background image** for the projector output whenever there's no karaoke
  graphics to show.

### The controls

- **MIDI DJ controller** support — a Hercules DJControl Inpulse 200 MK2 is
  detected automatically, with soft takeover on the faders. See
  [DJ controller](#dj-controller) below.

---

## Requirements

### To run a built bundle

- x86_64 Linux. The published bundle is built on CachyOS and needs **glibc 2.44
  or newer** (Arch and newer; not Ubuntu 24.04 or Debian 12 — libc is not
  relocatable). Building from source on your own machine sidesteps this.
- An audio device Qt can open.
- **ffmpeg, optional but recommended.** It provides `ffprobe`, which measures
  track lengths while a folder is indexed. Without it songs still appear, but
  durations show as `--:--`.

Everything else — Qt, Rubber Band, the FFmpeg libraries and the QML modules —
is bundled for you.

### To build from source

- A C++20 compiler and CMake 3.16+
- Qt 6: Core, Gui, Qml, Quick, QuickControls2, Sql, Multimedia, Concurrent
- [Rubber Band](https://breakfastquay.com/rubberband/) (tempo/key DSP)
- ALSA (DJ controller input)
- `pkg-config`
- ffmpeg (optional, for track durations)

On Arch / CachyOS:

```sh
sudo pacman -S --needed base-devel cmake qt6-base qt6-declarative \
    qt6-multimedia rubberband alsa-lib ffmpeg
```

On Debian / Ubuntu (package names vary by release):

```sh
sudo apt install build-essential cmake qt6-base-dev qt6-declarative-dev \
    qt6-multimedia-dev librubberband-dev libasound2-dev ffmpeg
```

---

## Building and running

```sh
cmake -B build -S .
cmake --build build
./build/mainteckj-app
```

A development build reads the QML straight from `qml/`, so UI edits show up
without a rebuild.

### Packaging a deployment bundle

A **Release** build is required for packaging: only a Release build draws its
QML from the installed location rather than the source tree.

```sh
cmake -B build-release -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
cpack --config build-release/CPackConfig.cmake -B dist
```

This produces `dist/MaintecKJ-<version>-linux-x86_64.tar.gz`. Unpack it anywhere
and run `./bin/mainteckj-app` (or `./run.sh`, which also sets the library path).

---

## Using it

The window has three tabs across the top, a singer list on the left, the library
and queue in the middle, and the deck on the right.

### Karaoke

Add songs to a singer's queue from the library. Double-click a singer to make
them current, then double-click a song to queue it for them. The rotation
advances when a song ends, but it never starts the next singer's track for you —
that's the host's cue.

### Background Music

A separate library and playlist. *Add Folder* indexes audio files, *Add All to
Playlist* fills the playlist, *Random* reshuffles it, and tracks auto-advance as
they finish. Dragging a library row down onto the playlist also queues it.

### Settings

Folder management (Add / Remove / Update / Update All), the naming pattern for
each folder, song-list export, the OpenKJ singer import, and the secondary
display's background image.

### The deck

The right-hand panel holds the mini preview of the secondary window, the
transport (Play/Pause, Stop), the progress bar, the tempo, key and volume
sliders, the audio-output selector, and the DJ controller status.

*Launch Secondary Window* opens the output window for the projector; it closes
with `Esc`.

### DJ controller

A Hercules DJControl Inpulse 200 MK2 is recognised automatically, before or
after launch; the deck panel shows whether it has been detected.

| Control             | Action                                                     |
| ------------------- | ---------------------------------------------------------- |
| Left tempo fader    | Tempo                                                      |
| Right tempo fader   | Key shift (also written to the loaded singer's queue row)  |
| Right play/pause    | Toggle playback                                            |
| Right volume fader  | Output volume                                              |

Both faders use soft takeover, so they stay inert until moved through the value
already in use — otherwise the first touch would jump the tempo or key.

---

## File formats and naming

Songs are matched to their media by name. Supported inputs:

| Input                | Notes                                                        |
| -------------------- | ------------------------------------------------------------ |
| `.cdg` + `.mp3` pair | The classic karaoke pair; the `.cdg` must share the audio name |
| `.zip`               | Containing a matching `.cdg` and audio file                   |
| `.mp4` / `.mkv` / `.avi` | Video karaoke                                             |
| Audio (background)   | A wide, FFmpeg-decodable list — `ffprobe` decides what is real |

Artist and title come from the filename, using the folder's **naming pattern**:

| Pattern                       | Example filename                    | Artist | Title              |
| ----------------------------- | ----------------------------------- | ------ | ------------------ |
| `{Artist} - {Title}` (default) | `Queen - Bohemian Rhapsody`        | Queen  | Bohemian Rhapsody  |
| `{Title} - {Artist}`          | `Bohemian Rhapsody - Queen`         | Queen  | Bohemian Rhapsody  |
| `{Track} - {Artist} - {Title}` | `01 - Queen - Bohemian Rhapsody`   | Queen  | Bohemian Rhapsody  |

For anything else, choose **Custom regex…** and supply a regular expression
whose **capture group 1 is the Artist and group 2 the Title**, e.g.
`^(\d+)\.\s*(.*?) - (.*)$`. The dialog previews the result live and lists worked
examples while you type. Separators ignore spacing, so ` - ` also matches
`-` and `  -  `.

---

## Where your data lives

- **Library, queue, singers and background playlist** —
  `~/.local/share/MaintecKJ/MaintecKJ/songdatabase.db`, a SQLite file. Back this
  up to keep your singers and their history.
- **Window layout, background image, deck settings** —
  `~/.config/MaintecKJ/MaintecKJ.conf`.

`tools/check-database.sh` prints what's in the database and whether the file
paths it holds actually exist on this machine — handy after moving a library or
importing from another computer.

---

## Troubleshooting

- **Wrong output device** — pick one in the deck panel's audio-output list; the
  choice is remembered.
- **No sound** — check the deck's volume slider. The DJ controller's volume
  fader is absolute and will overwrite it.
- **Startup fails with a platform plugin error** — run it from a desktop
  session. The bundled plugins are `xcb` (normal desktop) and `offscreen`
  (headless testing).
- **Songs have no duration** — install ffmpeg, then *Update* the folder in
  Settings.
- **A song won't play** — the row can point at a file that has moved or been
  renamed; *Update* the folder to re-index it.

---

## Project layout

```
src/            C++ backend: models, database, rotation, scanning, playback, MIDI
qml/            QML UI — Main.qml and the panels under components/
packaging/      Launcher and the readme shipped inside the deployment bundle
tools/          Helper scripts (database inspection)
CMakeLists.txt  Build, QML module and packaging configuration
```

---

## Credits and inspiration

MaintecKJ is inspired by **[OpenKJ](https://github.com/OpenKJ/OpenKJ)**, the
cross-platform open-source karaoke hosting application by the OpenKJ project —
its approach to singer rotation, song databases and CD+G playback informed much
of the design here. It is an independent project and shares no code with OpenKJ.

Also standing on:

- [Qt 6](https://www.qt.io/) — application framework
- [Rubber Band](https://breakfastquay.com/rubberband/) — time-stretching and
  pitch-shifting
- [FFmpeg](https://ffmpeg.org/) — media probing and decoding
- [SQLite](https://sqlite.org/) — the song database

---

## License

MaintecKJ is released under the **GNU General Public License v3.0**. See
[LICENSE](LICENSE) for the full text.

Copyright (C) 2026 John Main.

GPL-3.0 is the licence this project has to use in practice: it links
[Rubber Band](https://breakfastquay.com/rubberband/) (GPL-2.0-or-later) and ships
Qt Multimedia with GPL FFmpeg, so a distributed binary must be GPL-compatible.
