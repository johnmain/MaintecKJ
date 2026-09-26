# Third-party components

MaintecKJ itself is licensed under the **GNU General Public License v3.0** — see
[LICENSE](LICENSE).

The deployment bundle also ships the libraries below. Each remains under its own
licence; the table points at the upstream project, where the full text and the
corresponding source live.

## Principal components

| Component | Licence | Upstream |
| --- | --- | --- |
| Qt 6 (Core, Gui, Qml, Quick, QuickControls2, Sql, Multimedia, Network, Svg, Widgets) | GPL-3.0-only **or** LGPL-3.0-only, with the Qt GPL exception; commercial licensing available | <https://www.qt.io> · <https://code.qt.io> |
| Rubber Band | GPL-2.0-or-later (commercial licensing available) | <https://breakfastquay.com/rubberband/> |
| FFmpeg (libavcodec, libavformat, libavutil, libswscale, libswresample) | GPL-3.0-only (as built here) | <https://ffmpeg.org> |
| x264 | GPL-2.0-only | <https://www.videolan.org/developers/x264.html> |
| x265 | GPL-2.0-or-later | <https://www.videolan.org/developers/x265.html> |
| SQLite | Public domain | <https://sqlite.org> |
| ALSA (libasound) | LGPL-2.1-or-later | <https://www.alsa-project.org> |
| PulseAudio (libpulse) | LGPL-2.1-or-later | <https://www.freedesktop.org/wiki/Software/PulseAudio/> |
| GStreamer | LGPL-2.1-or-later | <https://gstreamer.freedesktop.org> |
| GTK, GLib, GObject | LGPL-2.1-or-later | <https://www.gtk.org> |
| Pango | LGPL-2.0-or-later | <https://pango.gnome.org> |
| Cairo | LGPL-2.1-only or MPL-1.1 | <https://cairographics.org> |
| KDE Frameworks | LGPL-2.1-or-later | <https://invent.kde.org/frameworks> |
| OpenSSL (libcrypto, libssl) | Apache-2.0 | <https://www.openssl.org> |
| X11 / XCB libraries | MIT / X11 | <https://www.x.org> |
| Wayland (libwayland) | MIT | <https://wayland.freedesktop.org> |
| ICU | Unicode-3.0, BSD-2-Clause, BSD-3-Clause, NAIST-2003 | <https://icu.unicode.org> |
| zlib | Zlib | <https://zlib.net> |
| libpng | libpng-2.0 | <http://www.libpng.org> |
| libjpeg-turbo | BSD-3-Clause, IJG | <https://libjpeg-turbo.org> |
| libvpx | BSD-3-Clause | <https://www.webmproject.org> |
| dav1d | BSD-2-Clause | <https://code.videolan.org/videolan/dav1d> |
| Opus | BSD-3-Clause | <https://opus-codec.org> |
| Vorbis | BSD-3-Clause | <https://xiph.org/vorbis/> |
| FLAC | BSD-3-Clause (or GPL-2.0-or-later) | <https://xiph.org/flac/> |

## Other bundled libraries

The bundle also carries the transitive dependencies of the components above —
image and video codecs, the TLS stack, and the X11/Wayland client libraries.
They stay under their own licences, and their full texts ship with the
corresponding upstream projects.

## Source code

The complete corresponding source for the GPL and LGPL components is available
from the upstream projects linked above. This project's own source is at
<https://github.com/johnmain/MaintecKJ>.
