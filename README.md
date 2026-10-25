# TT Central

> A portable Windows app that unifies the TeamTalk 5 client, a local & remote
> server manager, background server/channel monitors with notifications, and a
> built-in media-player bot into one accessible, screen-reader-friendly program.
> Free software (GPLv3); built on the TeamTalk 5 SDK.

TT Central brings the whole TeamTalk 5 experience together on Windows. Connect
and talk as a client, host your own server and administer remote ones, run
background monitors that notify you of logins, logouts, and messages across
multiple servers, and stream music into a channel with a built-in media-player
bot — all in one place, keyboard-driven and built for screen-reader users from
the ground up, with the familiar TeamTalk sound packs included. It runs portably
from a folder or from an installer, with no virtual audio cable and nothing extra
to wire up.

> **Status: 1.0.** The client, monitors, media bot and local server are
> complete and tested on Windows CI: unit tests, integration tests against
> BearWare's real TeamTalk server, an in-app self-test with an accessibility
> audit, and tests of the packaged app and installer. See
> [`docs/USER-GUIDE.md`](docs/USER-GUIDE.md) to use it and
> [`docs/ROADMAP.md`](docs/ROADMAP.md) for what was built.

---

## What's inside

- **Client** — the full TeamTalk 5 client (voice, text, channels, file
  transfer), built from BearWare's official Qt client, with a **TT Central**
  menu (Alt+T) for everything below.
- **Monitors** — background logins that each sit in a server channel and
  notify you of logins, logouts, private messages to the monitor, and channel
  messages. Run several at once across servers; every notification says which
  server it came from. They start after you log in (or at startup, or by
  hand), reconnect by themselves, and explain when a server allows only one
  login per IP address.
- **Notifications** — spoken through your screen reader or text-to-speech,
  played with the original TeamTalk sound packs (Default, Majorly-G, Old),
  and optionally shown as Windows notifications; with a history and a
  do-not-disturb switch.
- **Media bot** — a dedicated bot account that plays YouTube, SoundCloud,
  Spotify, Apple Music and direct or radio streams into a channel, decoded
  with ffmpeg and sent straight into the voice stream (no virtual audio
  cable). A native C++ port of Simple TT Media with all its chat commands,
  run as its own process and controlled from the Media bot window.
- **Local server** — host a TeamTalk server on this computer with BearWare's
  server: settings, accounts, limits, file sharing, start/stop, its log, and
  restart if it stops. **Server administration** opens the client's own
  admin tools for the connected server.
- **Settings** — Monitors, Notifications, Sounds, Media bot, and
  Accessibility and shortcuts, alongside the client's own Preferences.

## Accessibility

Accessibility is a requirement, not a feature. Every TT Central window is
fully usable from the keyboard, every control has a name a screen reader
announces, and CI fails if one doesn't: an audit walks every TT Central
window through Qt's accessibility layer (what NVDA and JAWS see) inside the
real client. Status fields are read-only text boxes a screen reader can
re-read at any time, lists update in place so your position is kept, and
the client speaks through Prism (NVDA, JAWS, SAPI and more). See
[`docs/ACCESSIBILITY.md`](docs/ACCESSIBILITY.md).

## Platform

Windows 10/11, 64-bit. The base client is cross-platform, but TT Central is
built and tested for Windows. (TT Central's own code also builds on Linux,
which is used for development and tests.)

## Getting TT Central

Build it (below), then make the packages:

```bat
python scripts\fetch_tools.py      & rem ffmpeg, yt-dlp and Deno for the media bot
python scripts\package.py          & rem dist\TTCentral-<ver>-win64 and the portable zip
iscc /DSourceDir=dist\TTCentral-<ver>-win64 /DAppVersion=<ver> /DOutputDir=dist packaging\windows\ttcentral.iss
```

- **Portable zip** — unzip and run `TTCentral.exe`; settings stay in the
  folder (`TTCentral.ini`).
- **Installer** — per-user by default (no administrator rights needed);
  settings in `%APPDATA%\TTCentral`.

CI builds and tests both on every run but does not publish them, because
they contain the proprietary TeamTalk SDK (see Licensing).

## Building (Windows)

Requirements: Visual Studio with MSVC x64, CMake 3.21+, Qt 6.4+ with Qt
Multimedia and Qt Speech (CI uses Qt 6.12), Python 3, and Git.

```bat
git clone --recurse-submodules https://github.com/donovanmalisch23-alt/TT_Central.git
cd TT_Central
python -m pip install py7zr
python scripts\bootstrap.py
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH=C:\Qt\6.12.0\msvc2022_64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

`bootstrap.py` fetches the proprietary TeamTalk SDK and BearWare's server
(never stored in this repo), stages them for the client build, and applies
our patches to the pinned upstream source. The client is built as
`external\TeamTalk5\Client\qtTeamTalk\TTCentral.exe` with the media bot
(`ttc-mediabot.exe`) next to it. See [`docs/SDK.md`](docs/SDK.md) and
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

### Tests

- **Unit tests** — monitor sessions, notifications, settings, the media
  player, sources, chat commands, the audio engine (with a generated tone and
  real ffmpeg), and every dialog's accessibility.
- **Integration tests** — against BearWare's real `tt5srv`: monitors
  (logins, messages, long messages, one login per IP, reconnecting), the
  media bot (commands, music heard as voice by another client, pause, admins,
  lock, control connection, reconnecting) and the local server (hosting,
  logins, settings changes, crash recovery, port in use).
- **Self-test** — `TTC_SELFTEST=<file>` makes `TTCentral.exe` check its menu,
  shortcut clashes with the client's, every window, and the accessibility
  audit, then exit. CI runs it in the build, in the portable package with a
  clean PATH, and in a silent install.

## Licensing

- TT Central is free software under the **GNU GPL, version 3 or later** (see
  [`LICENSE`](LICENSE)). That covers our own code, our patches to BearWare's
  qtTeamTalk (itself GPLv3), and the client application built from them.
- Our code carries an additional permission (GPLv3 section 7) to link the
  proprietary TeamTalk 5 SDK; see [`NOTICE`](NOTICE).
- It links the **proprietary TeamTalk 5 SDK** (© BearWare.dk), which is fetched at
  build time (never committed to this public repo) and runs in **trial mode**
  without a purchased license key. Distributing binaries widely, or hosting a
  server as a shipped product, requires a license from BearWare.

See [`NOTICE`](NOTICE) and [`THIRD-PARTY-LICENSES.md`](THIRD-PARTY-LICENSES.md)
for every component (qtTeamTalk, TeamTalk SDK, sound packs, Prism, WinToast,
ffmpeg, yt-dlp and Deno).

## Credits

- [TeamTalk 5](https://bearware.dk) by BearWare.dk.
- The media bot is a native port of the Simple TT Media project.
