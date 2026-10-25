# TT Central — Architecture

TT Central is one Qt Widgets (C++) application, built by extending BearWare's
official qtTeamTalk client, with new, separable modules for monitoring,
notifications, server management, and media. This document is the canonical
design reference. See [`ROADMAP.md`](ROADMAP.md) for phasing.

## Confirmed decisions

| Topic | Decision |
|---|---|
| Integration | Unified single app — one window, panels, shared config |
| Audience | Both server operators and everyday users |
| Base | Fork/extend BearWare's official C++/Qt client (qtTeamTalk) |
| Server role | Host a local server **and** administer remote servers |
| Monitors | Background notification watchers; multiple concurrent; auto-start after sign-in; each tagged by server |
| Media player | Native C++ port of Simple TT Media, as a dedicated bot account |
| Sound & notify | The original TeamTalk sound packs (Default, Majorly-G, Old, from the pinned upstream); event sounds + screen-reader/TTS + optional toasts |
| Settings | A central settings page with many categories |
| Accessibility | First-class — NVDA/JAWS, keyboard-only; drives UI choices |
| Packaging | Portable zip **and** installer; Windows 10/11 x64 |
| Licensing | Everything ours is GPL-3.0-or-later (plus a section 7 permission to link the TeamTalk SDK); the client is built from GPLv3 qtTeamTalk; TeamTalk SDK proprietary (trial mode, never committed) |

## As built

TT Central is BearWare's TeamTalk client with a **TT Central** menu and its
own windows, rather than a new main window: the client's accessibility (the
gold standard for blind TeamTalk users) stays exactly as it is, and upstream
updates stay easy to pick up.

| Part | Where | What it is |
|---|---|---|
| Client | `external/TeamTalk5` + `patches/TeamTalk5/0001` | BearWare's qtTeamTalk (GPLv3), branded TT Central, with three hooks: compile in our integration file, install the TT Central menu, pass client events |
| Integration | `src/client/integration/` | Fills `ClientHooks` from qtTeamTalk: settings store, speech (Prism/Qt TTS), sound packs, toasts, saved servers, preferences, admin actions |
| `ttc_core` | `src/core/` | UI-free logic: SDK wrapper (`TeamTalkClient`), monitor sessions, text reassembly, server config (`tt5srv.xml`), local server process manager |
| `ttc_client` | `src/client/ttc/` | Qt Widgets: the TT Central menu, monitor manager, notification center, Monitors / Media bot / Local server windows, Settings dialog, welcome, accessibility audit and self-test |
| `ttc_media` + `ttc-mediabot` | `src/media/` | The media bot: pipe-fed ffmpeg audio engine, player, sources (yt-dlp, Spotify, Apple Music, direct), commands, lock, bot session, control server |
| Tests | `tests/` | Unit tests with fakes; integration tests against the real `tt5srv`; `pcmgen` stands in for ffmpeg |
| Packaging | `scripts/package.py`, `packaging/windows/ttcentral.iss` | Portable folder/zip and Inno Setup installer |

Processes: `TTCentral.exe` (client, monitors, local server manager) starts
`ttc-mediabot.exe` and `tt5srv.exe` as child processes. The media bot talks
to TT Central over a per-user local socket (JSON lines) and exits if TT
Central does (`--parent-pid`). Each monitor and the bot are separate TeamTalk
logins.

### Media bot audio path

ffmpeg decodes to signed 16-bit 48 kHz stereo PCM on a pipe. A feeder thread
reads 20 ms frames, applies the volume and calls `TT_InsertAudioBlock`,
staying at most 350 ms ahead of real time so stop and skip respond at once.
Pause stops reading; the pipe fills and ffmpeg blocks, so memory stays flat
and the position is kept. (`QProcess` would buffer the whole decode, so the
decoder runs under a small `PipeProcess` with blocking reads.) Stream URLs
are resolved by yt-dlp just before a track plays, with the next one
prefetched; Spotify and Apple Music tracks are matched on YouTube by title
and duration.

## Original design notes

The sections below are the original plan; "As built" above supersedes them
where they differ (for example, panels became separate windows reached from
the TT Central menu).

## High-level

A main window hosts dockable/tabbed panels sharing one settings store and one
accessibility model:

- **Client panel** (reuse qtTeamTalk): servers, channels, users, voice, text
  chat, file transfer.
- **Monitors panel**: status of each background monitor + the notification feed.
- **Server panel**: local hosting controls + remote admin.
- **Media panel**: media-bot control, now-playing/queue; the private-message
  command interface is preserved.
- **Settings**: see below.

## Modules (prefer separable)

- **tt-core** — thin C++ wrapper over the TeamTalk SDK (client instances, event
  pump). qtTeamTalk already provides this; reuse it.
- **ttc-shell** — the extended qtTeamTalk app (our UI): the pinned upstream
  (GPLv3) plus our patches in `patches/TeamTalk5/`. Keep patches minimal and
  reviewable so upstream releases stay easy to pick up.
- **ttc-monitor** — the background watcher subsystem. A *monitor* is a
  lightweight, mostly-passive TeamTalk login to one server + channel. Several
  run concurrently across different servers, auto-starting after sign-in. Each
  raises notifications for: user logins, logouts, private messages sent to the
  monitor, and channel messages in its channel. Every notification is tagged
  with the originating server/monitor. A monitor may reuse the user's own
  account; where a server allows only one login per IP, the user must create a
  separate account (surfaced in Settings → Monitors).
- **ttc-notify** — shared notification subsystem used by monitors and the main
  client. Routes each event to a sound from the selected TeamTalk sound pack, a
  screen-reader/TTS announcement (verbosity-controlled, names the server), and
  optionally a Windows toast. Per-monitor and global rules decide what fires.
- **ttc-server** — manages a local `TeamTalkServer` process (config, accounts,
  channels, logs) and performs remote admin via an admin SDK login.
- **ttc-media** — native C++ media engine:
  - a dedicated SDK client instance logged in as the bot account;
  - audio pipeline: source → ffmpeg decode → PCM (48 kHz / stereo / 20 ms
    frames) → real-time pacing → `InsertAudioBlock`; volume/pause/seek/skip
    (port of `simple_tt_media/.../teamtalk/audio.py`);
  - queue & policy: repeat (track/queue), shuffle, history, prefetch (port of
    `player.py`);
  - command parser: the Simple TT Media grammar over private messages;
  - **source resolution stays as bundled executables** — `yt-dlp.exe` (+ Deno)
    and ffmpeg driven as subprocesses; Spotify/Apple metadata matched to YouTube
    audio, as today. "Native port" means the engine/queue/commands become C++;
    yt-dlp and ffmpeg are **not** reimplemented.
  - runs as a managed child process, controlled over local IPC, to isolate a
    media crash from the client.

> Design rule: build new features as separable modules/processes rather than
> deep edits to qtTeamTalk, so upstream releases stay easy to pick up, our
> modules stay reusable, and the proprietary SDK stays isolated.

## Upstream source, licensing & the public repo

Two different BearWare licenses are involved:

- **The qtTeamTalk client source is GPLv3** (its files carry GPLv3 headers).
  TT Central builds its client from it, so the client application is GPLv3.
  Our own modules are GPLv3 too, with a section 7 permission to link the
  SDK (see `NOTICE`).
- **The TeamTalk 5 SDK is proprietary** (the DLL/import library and C API
  headers, plus BearWare's server sample code). It runs in trial mode without
  a license key, and its license does not allow redistributing the SDK itself.

How the repo carries each:

- **qtTeamTalk:** pinned as the git submodule `external/TeamTalk5` at tag
  `v5.23` (a reference, not a copy). Our changes live as GPLv3 patch files in
  `patches/TeamTalk5/`, applied by `scripts/bootstrap.py`. This keeps upstream
  updates easy: bump the tag, re-apply the patches.
- **SDK:** **fetched at build time** (`scripts/fetch_sdk.py`, git-ignored — see
  [`SDK.md`](SDK.md)), then staged into the submodule by `bootstrap.py`.
  Never committed.
- **Sound packs:** used from the submodule (`Setup/Client/Sounds`).

The repo is **public**. That's fine for everything above: the client source is
GPLv3 and the SDK never enters git. CI publishes no build artifacts, because
binaries contain the SDK, and redistributing them needs a BearWare license key.

## Local server hosting (Phase 3 note)

BearWare's `Server/TeamTalkServer` is SDK sample code under the proprietary
license, so TT Central won't build it. Instead, local hosting manages
BearWare's **prebuilt Standard server** `tt5srv.exe` (fetched with the SDK):
TT Central writes its `tt5srv.xml`, starts/stops the process, and shows its
log. It needs no BearWare.dk WebLogin, but it is unencrypted; encrypted
hosting needs the Professional `tt5prosrv` plus a WebLogin.

## Concurrency

The main client, the media bot, and every monitor are each a separate TeamTalk
login. Design for N concurrent instances (resource/thread budget), run the media
bot (and optionally heavy monitors) as child processes, and cap/deduplicate
connections.

## Accessibility (cross-cutting, from Phase 1)

- Build on Qt's UI Automation bridge; preserve qtTeamTalk's existing
  accessibility (the gold standard for blind VoIP users).
- Every control has an accessible name/role/value; logical tab order; nothing
  mouse-only.
- Notifications are an accessible, throttle-able announcements stream with
  configurable verbosity; spoken announcements name the server.
- Global shortcuts jump between panels; media controls work by keyboard and by
  private message.
- Manual NVDA passes each phase; JAWS spot-checks.

## Settings page

A central, fully-accessible settings area with categories:

- **Monitors** — add/edit/remove monitors (account, server, channel, which
  events, which sounds/announcements, enable on startup); shows the one-login-
  per-IP guidance.
- **Sounds** — choose a TeamTalk sound pack, per-event sound mapping, volume, mute.
- **Notifications** — screen-reader/TTS verbosity, toasts, per-event on/off,
  quiet hours.
- **Media bot** — bot account, default channel, sources, volume limits, admins.
- **Server** — local hosting config; saved remote servers for admin.
- **Connection / accounts** — servers, credentials, auto-connect.
- **Accessibility & hotkeys** — shortcuts, focus behavior, announcement style.
- **Appearance & general** — theme, startup behavior, logging.

## Config & first-run

One unified config (qtTeamTalk settings + our sections) and an accessible
first-run wizard (server details, bot account, channels, sources, admins),
echoing the Simple TT Media wizard.

## Build & packaging

- Toolchain: MSVC x64, CMake, Qt 6 (6.12 in CI, matching BearWare's own
  Windows CI).
- Build the client from BearWare's source pinned as the submodule
  `external/TeamTalk5` (tag `v5.23`), plus our patches; fetch the SDK at build
  time (see [`SDK.md`](SDK.md)).
- Pulled in at build time: TeamTalk SDK (trial), Prism (screen-reader output),
  WinToast (toasts); later ffmpeg, yt-dlp, Deno for the media bot. Sound packs
  come from the submodule.
- Outputs: portable zip (no install, no admin) + installer (Inno Setup or WiX).
- CI: GitHub Actions Windows runner (trial SDK; no licensed SDK committed).
