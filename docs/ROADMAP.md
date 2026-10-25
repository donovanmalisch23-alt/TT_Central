# TT Central — Roadmap

Phased build. Each phase ends with something that runs, is accessible, and is
packaged. See [`ARCHITECTURE.md`](ARCHITECTURE.md) for the design.

## Phase 0 — Foundations (current)

- [x] Repository scaffolding, licensing (MIT), docs.
- [x] SDK fetch-at-build-time: `scripts/fetch_sdk.py` pulls the proprietary
      TeamTalk 5 SDK (pinned **v5.23a**, `TEAMTALK_VERSION 5.23.0.5209`) into
      `third_party/teamtalk-sdk/`, git-ignored. Verified: headers + `.lib` +
      `.dll` land and are excluded from the repo.
- [x] CI wired to fetch the SDK before configuring.
- [ ] Decide repo visibility (public ⇒ SDK/qtTeamTalk stay fetched + patches;
      private ⇒ may vendor directly). See `ARCHITECTURE.md`.
- [ ] Pin the `bearware/TeamTalk5` source at a known tag; build vanilla
      qtTeamTalk + server on Windows.
- [ ] CI green on a Windows runner (build enabled once module targets land).
- [ ] Accessibility baseline audit of the stock client.
- [ ] Confirm Qt version.

## Phase 1 — Unified shell + client

- Reshape qtTeamTalk into the TT Central main window with a panel framework.
- Client fully working (voice, text, channels, files); accessibility preserved.
- A basic Settings page.
- Portable zip + installer pipeline working with just the client.

## Phase 2 — Monitors + notifications + sound packs

- The watcher subsystem: multiple concurrent background logins, auto-start after
  sign-in, tagged by server.
- The shared notification subsystem: sound pack + TTS/screen-reader + toasts.
- Bundle the TeamTalk sound packs.
- Settings → Monitors / Sounds / Notifications categories, including the
  one-login-per-IP guidance.

## Phase 3 — Server (remote admin → local hosting)

- Remote admin first (reuses SDK admin APIs): accounts, channels, bans.
- Local `TeamTalkServer` management: start/stop, config, accounts, channels, logs.
- Note: building/hosting a server needs Professional Edition — a licensing gate
  for shipped distribution.

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
  obtain a TeamTalk license from BearWare (Professional for the server). See
  `../NOTICE`.
