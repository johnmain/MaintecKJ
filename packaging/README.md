# MaintecKJ

A karaoke host system. Two windows: the host UI, and a second window that goes to
the projector or TV.

## Running it

```
./bin/mainteckj-app
```

If that will not start, try the launcher, which additionally sets the library
path:

```
./run.sh
```

Unpack it anywhere. Qt, Rubber Band, the FFmpeg libraries and the QML modules
are all inside this folder, and the app locates them relative to itself, so the
tree can be moved after unpacking.

## Requirements

- x86_64 Linux with glibc 2.44 or newer. This was built on CachyOS; it runs on
  Arch and other distributions at least as new, but not on older releases such
  as Ubuntu 24.04 or Debian 12. Nothing can bundle around that - libc is not
  relocatable.
- An audio device that Qt can open.
- **ffmpeg, optional but recommended.** It supplies `ffprobe`, which measures
  track lengths while a folder is being indexed. Without it songs still appear,
  but their durations show as `--:--`, and a corrupt file can no longer be told
  apart from a good one so it will be listed too. Install with
  `sudo pacman -S ffmpeg`.

ffmpeg is the only thing expected from the system. Everything else is
self-contained.

## Where your data lives

- Library, queue, singers and the background playlist:
  `~/.local/share/MaintecKJ/MaintecKJ/songdatabase.db` - a SQLite file. Back this
  up to keep your singers and their history.
- Window layout, chosen background image, deck settings:
  `~/.config/MaintecKJ/MaintecKJ.conf`

## Using it

- **Karaoke tab** - add songs to a singer's queue from the library. Double-click a
  singer to select them, double-click a song to queue it for them. The rotation
  advances automatically when a song ends, but it will not start the next
  singer's track for you.
- **Background Music tab** - a completely separate library and playlist. *Add
  Folder* indexes audio files, *Add All to Playlist* fills the playlist, *Random*
  reshuffles it once per press, and tracks auto-advance as they finish.
- Switching back to the Karaoke tab fades the background music out over five
  seconds and stops it. Going the other way leaves whatever is playing alone.

The deck panel on the right holds the transport, tempo and key sliders, the
audio output selector, and the secondary display. The secondary window closes
with Esc.

A background image can be set from the deck panel. It fills the projector output
whenever there is no karaoke video or CDG to show, which covers the gap between
songs and the whole time background music is playing.

## DJ controller

A Hercules DJControl Inpulse 200 MK2 is recognised automatically, before or
after launch. The deck panel shows whether it has been detected.

| Control | Action |
| --- | --- |
| Left tempo fader | Tempo |
| Right tempo fader | Key shift, also written to the loaded singer's queue row |
| Right play/pause | Toggle playback |
| Right volume fader | Output volume |

Both faders use soft takeover, so they stay inert until moved through the value
the app is already using - otherwise the first touch would jump the tempo or key
to wherever the fader happened to be parked.

## Troubleshooting

- **Wrong output device** - pick one in the deck panel's audio output list. The
  choice is remembered.
- **No sound** - check the volume fader on the deck panel; the DJ controller's
  volume fader is absolute and will overwrite it.
- **Startup fails with a platform plugin error** - run it from a desktop session.
  The bundled plugins are xcb (normal desktop) and offscreen (headless testing).
- **Songs have no duration** - install ffmpeg, then use Rescan on the folder.

## License

MaintecKJ is released under the GNU General Public License v3.0 - see `LICENSE`
in this folder. The libraries bundled alongside it are listed, with their
licenses, in `THIRD-PARTY.md`.
