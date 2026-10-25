# TT Central — Roadmap

What was built, phase by phase, and what could come next. See
[`ARCHITECTURE.md`](ARCHITECTURE.md) for the design and
[`USER-GUIDE.md`](USER-GUIDE.md) for using it.

## 1.0 — done

### Foundations
- [x] Public repository; GPL-3.0-or-later with a section 7 permission to link
      the proprietary TeamTalk SDK (`NOTICE`).
- [x] The TeamTalk 5 SDK (Standard, v5.23a) and BearWare's server are fetched
      at build time (`scripts/fetch_sdk.py`, `scripts/bootstrap.py`), never
      committed.
- [x] Windows CI (MSVC, Qt 6.12): build, unit and integration tests, in-app
      self-test, packages.

### Client
- [x] Built from BearWare's qtTeamTalk (pinned submodule, tag v5.23) plus one
      small patch (`patches/TeamTalk5/0001`): TT Central branding and
      settings file, no BearWare update check, and hooks for the TT Central
      menu and client events.
- [x] Prism screen-reader output; the TeamTalk sound packs.

### Monitors and notifications
- [x] Any number of background monitors, each in one server channel,
      reporting logins, logouts, private messages, channel messages and
      (optionally) broadcasts, every notification naming its server.
- [x] Start after login, at startup, or by hand; reconnect with backoff,
      announcing each outage once; stop on permanent login errors; explain
      one-login-per-IP refusals; fall back to the root channel.
- [x] Notifications spoken, played with the sound pack, shown as Windows
      notifications; history; do not disturb.

### Media bot
- [x] `ttc-mediabot`, a native C++ port of Simple TT Media, as its own
      process: ffmpeg decoding, paced voice injection, queue and repeat and
      shuffle, YouTube/SoundCloud/Spotify/Apple Music/radio, every chat
      command, admins, lock, reconnects.
- [x] Media bot window, settings page, auto-start and restart from TT Central;
      ffmpeg, yt-dlp and Deno fetched by `scripts/fetch_tools.py`.

### Local server and administration
- [x] Host BearWare's `tt5srv` from TT Central: settings, accounts, limits,
      file sharing, log, restart on crash, port checks.
- [x] Server administration menu for the client's own admin tools.

### Settings, accessibility, packaging, documentation
- [x] Settings: Monitors, Notifications, Sounds, Media bot, Accessibility and
      shortcuts.
- [x] Accessibility audit of every window in CI; no shortcut clashes; first-run
      welcome.
- [x] Portable zip and per-user installer, both tested in CI with a clean
      PATH (not published: they contain the SDK).
- [x] User guide, accessibility checklist.

## Ideas for later

- Encrypted local hosting (needs BearWare's Professional server and a
  BearWare.dk WebLogin).
- Per-monitor sounds and quiet hours.
- Saved media playlists, and running several media bots at once.
- Code-signed releases, once a TeamTalk license allows distributing
  binaries.
- Translations of TT Central's own text (the client is already translated).
