# Third-party licenses

TT Central's own code is MIT-licensed (see `LICENSE`). The components below are
bundled or built upon and keep their own terms. This file is a summary; each
component's full license text ships with its files.

## TeamTalk 5 SDK — PROPRIETARY (© BearWare.dk)

- **Files:** fetched into `third_party/teamtalk-sdk/` by `scripts/fetch_sdk.py`
  (`TeamTalk5Pro.dll`, `TeamTalk5Pro.lib`, `TeamTalk.h`, `TeamTalkSrv.h`) and
  **git-ignored** — not stored in this public repo.
- **Terms:** TeamTalk 5 SDK License Agreement (fetched to
  `third_party/teamtalk-sdk/License.txt`).
- **Key points:**
  - Use requires agreeing to the SDK License Agreement.
  - Runs in **trial mode** and disables after ~30 days **without** a purchased
    License Key.
  - Redistributing an end-user application that uses the SDK requires a License
    Key. Building/running a server requires **Professional Edition**.
  - Proprietary notices must not be removed.
- **Pricing (confirm current figures with BearWare):** Standard ~€990,
  Professional ~€1,600, excl. VAT. Contact: contact@bearware.dk.

## TeamTalk sound packs — PROPRIETARY (© BearWare.dk)

- **Files:** `assets/soundpacks/` — BearWare assets, **not committed** to this
  public repo; supplied locally from a TeamTalk client install.
- **Terms:** proprietary BearWare assets; used under the same trial/community
  posture as the SDK; notices retained.

## qtTeamTalk-derived source — © BearWare.dk

- TT Central extends BearWare's qtTeamTalk client. Our modifications to that
  source are derivatives of proprietary code and are distributed as
  source-available patches under BearWare's terms, not MIT. New, separable
  modules we author (media engine, monitor/notification logic, server manager,
  build scripts) are MIT.

## FFmpeg — LGPL/GPL

- **Use:** bundled executable, invoked as a subprocess by the media engine to
  decode audio.
- **Terms:** LGPL-2.1+/GPL depending on the build used; see https://ffmpeg.org/legal.html.
  Keep FFmpeg's license and source-offer obligations with the bundled build.

## yt-dlp — The Unlicense (public domain)

- **Use:** bundled executable, invoked as a subprocess to resolve media sources.
- **Terms:** https://github.com/yt-dlp/yt-dlp/blob/master/LICENSE

## Deno — MIT License

- **Use:** bundled runtime used by yt-dlp to solve YouTube challenges.
- **Terms:** https://github.com/denoland/deno/blob/main/LICENSE.md
