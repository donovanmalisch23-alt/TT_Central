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
| Sound & notify | Bundle the original TeamTalk sound packs; event sounds + screen-reader/TTS + optional toasts |
| Settings | A central settings page with many categories |
| Accessibility | First-class — NVDA/JAWS, keyboard-only; drives UI choices |
| Packaging | Portable zip **and** installer; Windows 10/11 x64 |
| Licensing | Our code MIT; TeamTalk SDK + assets proprietary (trial mode) |

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

## Modules (prefer separable, MIT where ours)

- **tt-core** — thin C++ wrapper over the TeamTalk SDK (client instances, event
  pump). qtTeamTalk already provides this; reuse it.
- **ttc-shell** — the extended qtTeamTalk app (our UI). Source-available fork;
  keep changes as minimal, reviewable diffs.
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
> deep edits to qtTeamTalk, so as much as possible ships under MIT and the
> proprietary dependency stays isolated.

## Proprietary dependencies & repo visibility

The TeamTalk 5 SDK and the qtTeamTalk client source are proprietary
(© BearWare.dk). BearWare's license permits redistributing *your application*,
not the SDK or its source. That interacts with whether this repo is public:

- **Public repo (current).** No BearWare files may be committed. The SDK is
  **fetched at build time** (`scripts/fetch_sdk.py`, git-ignored — see
  [`SDK.md`](SDK.md)). The qtTeamTalk extension is therefore built by fetching
  the upstream client source at a pinned tag and applying **our changes as patch
  files / separable modules**, rather than committing a copy of BearWare's
  source. Only TT Central's own MIT code lives in the repo.
- **Private repo.** The SDK and a qtTeamTalk fork could be vendored directly
  (simplest), still under the trial/community posture. Wider distribution would
  still require a BearWare license.

This is an open project decision (see [`ROADMAP.md`](ROADMAP.md) Phase 0). The
design above holds either way; only *how the upstream source is carried* changes.

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

- Toolchain: MSVC x64, CMake, Qt 6.
- Pin the `bearware/TeamTalk5` source at a known tag (submodule) for the client
  fork and server; vendor the SDK (see [`SDK.md`](SDK.md)).
- Bundle at build time: TeamTalk SDK (trial), TeamTalk sound packs, ffmpeg,
  yt-dlp, Deno.
- Outputs: portable zip (no install, no admin) + installer (Inno Setup or WiX).
- CI: GitHub Actions Windows runner (trial SDK; no licensed SDK committed).
