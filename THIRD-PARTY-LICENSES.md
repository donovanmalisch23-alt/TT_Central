# Third-party licenses

TT Central's own new code is MIT-licensed (see `LICENSE`). The TT Central
**client application** is built from BearWare's GPLv3 qtTeamTalk, so the
combined application, as built and distributed, is subject to the **GPLv3**
(`LICENSES/GPL-3.0.txt`). The components below keep their own terms.

| Component | License | How it gets into the build |
|---|---|---|
| qtTeamTalk client (BearWare) | **GPLv3** | Pinned submodule `external/TeamTalk5` + our patches |
| TeamTalk 5 SDK (BearWare) | **Proprietary** | Fetched at build time; never committed |
| TeamTalk sound packs (BearWare) | BearWare assets | Used from the pinned submodule |
| Prism v0.17.1 | **MPL-2.0** | Downloaded and built during the client build |
| WinToast v1.3.2 | **MIT** | Downloaded during the client build |
| FFmpeg | LGPL/GPL | Planned (media bot) |
| yt-dlp | Unlicense | Planned (media bot) |
| Deno | MIT | Planned (media bot) |

## qtTeamTalk client — GPLv3 (© BearWare.dk)

- **Source:** `BearWare/TeamTalk5`, tag `v5.23` (`a0d26fd`), pinned as the
  submodule `external/TeamTalk5`. Its `Client/qtTeamTalk` sources carry GPLv3
  headers ("either version 3 of the License, or (at your option) any later
  version").
- **Our changes:** `patches/TeamTalk5/*.patch`, applied by
  `scripts/bootstrap.py`. They are GPLv3, like the code they modify.

## TeamTalk 5 SDK — PROPRIETARY (© BearWare.dk)

- **Files:** fetched into `third_party/teamtalk-sdk/` by `scripts/fetch_sdk.py`
  (`TeamTalk5.dll`, `TeamTalk5.lib`, `TeamTalk.h`, and the `tt5srv` server) and
  staged into `external/TeamTalk5/Library/TeamTalk_DLL/` by
  `scripts/bootstrap.py`. **Git-ignored**; never stored in this repo.
- **Terms:** TeamTalk 5 SDK License Agreement (fetched to
  `third_party/teamtalk-sdk/License.txt`).
- **Key points:**
  - Runs in **trial mode** and disables after ~30 days **without** a purchased
    License Key.
  - Redistributing an end-user application that uses the SDK requires a License
    Key. The Standard server (`tt5srv`) is unencrypted; encrypted hosting needs
    the **Professional Edition** and a BearWare.dk WebLogin.
  - Proprietary notices must not be removed.
- **Pricing (confirm with BearWare):** Standard ~€990, Professional ~€1,600,
  excl. VAT. Contact: contact@bearware.dk.
- BearWare's server *sample* (`Server/TeamTalkServer`) is SDK sample code under
  the same proprietary terms; TT Central does not build it.

## TeamTalk sound packs (© BearWare.dk)

- **Files:** `external/TeamTalk5/Setup/Client/Sounds` (Default, Majorly-G,
  Old). Used from the pinned submodule; not copied into this repo.

## Prism — Mozilla Public License 2.0

- Screen-reader output library used by the client for NVDA/JAWS/SAPI and other
  speech output. Fetched from https://github.com/ethindp/prism (v0.17.1) during
  the build. MPL-2.0 is GPLv3-compatible.

## WinToast — MIT License

- Windows toast notifications. Fetched from
  https://github.com/mohabouje/WinToast (v1.3.2) during the build.

## FFmpeg — LGPL/GPL (planned)

- **Use:** bundled executable, invoked as a subprocess by the media engine to
  decode audio. **Terms:** see https://ffmpeg.org/legal.html.

## yt-dlp — The Unlicense (planned)

- **Use:** bundled executable, invoked as a subprocess to resolve media sources.
  **Terms:** https://github.com/yt-dlp/yt-dlp/blob/master/LICENSE

## Deno — MIT License (planned)

- **Use:** runtime used by yt-dlp to solve YouTube challenges.
  **Terms:** https://github.com/denoland/deno/blob/main/LICENSE.md
