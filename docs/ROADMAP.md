# TT Central — Roadmap

Phased build. Each phase ends with something that runs, is accessible, and is
packaged. See [`ARCHITECTURE.md`](ARCHITECTURE.md) for the design.

## Phase 0 — Foundations (done)

- [x] Repository scaffolding, licensing, docs.
- [x] SDK fetch-at-build-time: `scripts/fetch_sdk.py` pulls the proprietary
      TeamTalk 5 SDK (pinned **v5.23a**, `TEAMTALK_VERSION 5.23.0.5209`) into
      `third_party/teamtalk-sdk/`, git-ignored.
- [x] Repo visibility decided: **public** (SDK fetched, never committed).
- [x] CI green on a Windows runner.

## Phase 1 — Unified shell + client

- Reshape qtTeamTalk into the TT Central main window with a panel framework.
- Client fully working (voice, text, channels, files); accessibility preserved.
- A basic Settings page.
- Portable zip + installer pipeline working with just the client.

Milestones and status: see [`PHASE1.md`](PHASE1.md). 1.0 (SDK smoke test) is
green on Windows CI; 1.1 (BearWare's qtTeamTalk + our patches, built in CI) is
in progress.

## Phase 2 — Monitors + notifications + sound packs

- The watcher subsystem: multiple concurrent background logins, auto-start after
  sign-in, tagged by server.
- The shared notification subsystem: sound pack + TTS/screen-reader + toasts.
- Wire up the TeamTalk sound packs from the pinned upstream (Default,
  Majorly-G, Old).
- Settings → Monitors / Sounds / Notifications categories, including the
  one-login-per-IP guidance.

## Phase 3 — Server (remote admin → local hosting)

- Remote admin first (reuses SDK admin APIs): accounts, channels, bans.
- Local server hosting: start/stop, config, accounts, channels, logs (our own
  host on the SDK server API, or BearWare's prebuilt server — see
  `ARCHITECTURE.md`).
- Hosting uses BearWare's Standard server `tt5srv` (no WebLogin needed;
  unencrypted). Encrypted hosting needs Professional + a BearWare.dk WebLogin.

## Phase 4 — Media bot (native engine)

- Port the audio engine, queue/policy, and command grammar to C++.
- Bundle yt-dlp / ffmpeg / Deno; run the bot as a managed child process.
- Dedicated bot-account config; controls in the UI and over private message.

## Phase 5 — Polish & v1.0

- Unified settings; accessible first-run wizard; docs.
- Full NVDA/JAWS pass; signed installer; release.

## Licensing checkpoints

- Personal/community/trial use is unblocked now.
- Before wide binary distribution or hosting a server as a shipped product,
  obtain a TeamTalk license key from BearWare. See
  `../NOTICE`.
