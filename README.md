# TT Central

> A portable Windows app that unifies the TeamTalk 5 client, a local & remote
> server manager, background server/channel monitors with notifications, and a
> built-in media-player bot into one accessible, screen-reader-friendly program.
> Open-source (MIT); built on the TeamTalk 5 SDK.

TT Central brings the whole TeamTalk 5 experience together on Windows. Connect
and talk as a client, host your own server and administer remote ones, run
background monitors that notify you of logins, logouts, and messages across
multiple servers, and stream music into a channel with a built-in media-player
bot — all in one place, keyboard-driven and built for screen-reader users from
the ground up, with the familiar TeamTalk sound packs included. It runs portably
from a folder or from an installer, with no virtual audio cable and nothing extra
to wire up.

> **Status: Phase 1 in progress.** The SDK integration is verified on Windows
> CI, and the client is being built from BearWare's qtTeamTalk plus our patches.
> The rest of the application is built in phases — see
> [`docs/ROADMAP.md`](docs/ROADMAP.md).

---

## What's inside

- **Client** — the full TeamTalk 5 client (voice, text, channels, file transfer),
  built by extending BearWare's official Qt client.
- **Server** — host a local TeamTalk server and administer remote ones (accounts,
  channels, bans) from the same place.
- **Monitors** — lightweight background watchers that sit in a server/channel and
  notify you of logins, logouts, private messages sent to the monitor, and
  channel messages. Run several at once across different servers; every
  notification says which server it came from.
- **Media bot** — a dedicated media/DJ account that plays YouTube, SoundCloud,
  Spotify, Apple Music, and direct/radio streams into a channel, decoded with
  ffmpeg and injected straight into the voice stream (no virtual audio cable).
  A native port of the Simple TT Media project.
- **Sound packs & notifications** — the original TeamTalk sound packs are
  included, alongside screen-reader/TTS announcements and optional Windows toasts.
- **Settings** — one accessible settings area for monitors, sounds,
  notifications, the media bot, servers/accounts, accessibility & hotkeys, and
  general options.

## Monitors

A *monitor* is a background TeamTalk login that watches one server + channel and
raises a notification on each login, logout, private message to the monitor, and
channel message it sees. You can run several at once; each notification is tagged
with the server it came from, and monitors auto-start when you sign in.

A monitor can ride your own account. **However, some TeamTalk servers allow only
one login per IP address — on those servers, create a separate account for the
monitor.** TT Central surfaces this guidance in **Settings → Monitors**.

## Platform

Windows 10/11, 64-bit, for now. The base client is cross-platform, but the SDK
and media tooling TT Central relies on are tuned for Windows.

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
```

`bootstrap.py` fetches the proprietary TeamTalk SDK (never stored in this repo),
stages it for the client build, and applies our patches to the pinned upstream
source. The client is built to
`external\TeamTalk5\Client\qtTeamTalk\TeamTalk5.exe`, with its Qt runtime in the
`windeployqt` folder next to it. See [`docs/SDK.md`](docs/SDK.md),
[`docs/PHASE1.md`](docs/PHASE1.md), and
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## Licensing

- TT Central's **own new code is MIT-licensed** (see [`LICENSE`](LICENSE)).
- The **client application is built from BearWare's qtTeamTalk, which is GPLv3**.
  Our patches to it are GPLv3, and the combined application is distributed
  under the **GPLv3** ([`LICENSES/GPL-3.0.txt`](LICENSES/GPL-3.0.txt)).
- It links the **proprietary TeamTalk 5 SDK** (© BearWare.dk), which is fetched at
  build time (never committed to this public repo) and runs in **trial mode**
  without a purchased license key. Distributing binaries widely, or hosting a
  server as a shipped product, requires a license from BearWare.

See [`NOTICE`](NOTICE) and [`THIRD-PARTY-LICENSES.md`](THIRD-PARTY-LICENSES.md)
for every component (qtTeamTalk, TeamTalk SDK, sound packs, Prism, WinToast, and
the planned media tools).

## Credits

- [TeamTalk 5](https://bearware.dk) by BearWare.dk.
- The media bot is a native port of the Simple TT Media project.
