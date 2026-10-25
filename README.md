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

> **Status: early scaffolding (Phase 0).** The repository layout, build
> groundwork, and vendored SDK are in place. The application itself is being
> built in phases — see [`docs/ROADMAP.md`](docs/ROADMAP.md).

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

## Building

The TeamTalk 5 SDK is proprietary and is **not** stored in this public repo; it
is fetched into place at build time:

```bat
python -m pip install py7zr
python scripts/fetch_sdk.py
```

See [`docs/SDK.md`](docs/SDK.md) for the details and
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the overall design. Toolchain:
MSVC x64, CMake, Qt 6.

## Licensing

TT Central's **own source code is MIT-licensed** (see [`LICENSE`](LICENSE)).

It is **not** a wholly open-source product: it builds on the **proprietary
TeamTalk 5 SDK** (© BearWare.dk), which is fetched at build time (not committed
to this public repo) and runs in **trial mode** without a purchased license key.
The TeamTalk sound packs are likewise proprietary and are not committed here.
Distributing binaries widely, or hosting a server as a shipped product, requires
a license from BearWare. See [`NOTICE`](NOTICE) and
[`THIRD-PARTY-LICENSES.md`](THIRD-PARTY-LICENSES.md) for the full breakdown and
the terms of every bundled component (TeamTalk SDK, sound packs, ffmpeg, yt-dlp,
Deno).

## Credits

- [TeamTalk 5](https://bearware.dk) by BearWare.dk.
- The media bot is a native port of the Simple TT Media project.
