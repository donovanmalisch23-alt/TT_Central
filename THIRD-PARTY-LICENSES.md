# Third-party licenses

TT Central is licensed under the **GNU GPL, version 3 or later** (see
[`LICENSE`](LICENSE)), with an additional permission (GPLv3 section 7) to link
the proprietary TeamTalk 5 SDK (see [`NOTICE`](NOTICE)). The components below
keep their own terms.

| Component | License | How it gets into the build |
|---|---|---|
| qtTeamTalk client (BearWare) | **GPLv3** | Pinned submodule `external/TeamTalk5` + our patches |
| TeamTalk 5 SDK (BearWare) | **Proprietary** | Fetched at build time; never committed |
| TeamTalk sound packs (BearWare) | BearWare assets | Used from the pinned submodule |
| Prism v0.17.1 | **MPL-2.0** | Downloaded and built during the client build |
| WinToast v1.3.2 | **MIT** | Downloaded during the client build |
| FFmpeg (BtbN GPL build) | **GPLv3** | Fetched by `scripts/fetch_tools.py`; bundled in packages |
| yt-dlp | Unlicense | Fetched by `scripts/fetch_tools.py`; bundled in packages |
| Deno | MIT | Fetched by `scripts/fetch_tools.py`; bundled in packages |
| Simple TT Media | MIT | The media bot is a C++ port of its design and command set |

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

## FFmpeg — GNU GPL version 3

- **Use:** `tools/ffmpeg/ffmpeg.exe`, run as a separate program by the media
  bot to decode audio. The GPL build from https://github.com/BtbN/FFmpeg-Builds
  is fetched by `scripts/fetch_tools.py` and bundled in packages.
- **Terms:** https://ffmpeg.org/legal.html. Its source is available from
  https://ffmpeg.org/download.html and from the BtbN build repository.

## yt-dlp — The Unlicense

- **Use:** `tools/yt-dlp/yt-dlp.exe`, run as a separate program to find media
  streams. **Terms:** https://github.com/yt-dlp/yt-dlp/blob/master/LICENSE

## Deno — MIT License

- **Use:** `tools/deno/deno.exe`, the JavaScript runtime yt-dlp uses for
  YouTube. **Terms:** https://github.com/denoland/deno/blob/main/LICENSE.md

## Simple TT Media — MIT License

- **Use:** TT Central's media bot (`src/media/`) is a native C++ port of Simple
  TT Media's audio engine, player, sources, lock and command set. Copyright (c)
  2026 Simple TT Media contributors; MIT license (permission notice: "Permission
  is hereby granted, free of charge, to any person obtaining a copy of this
  software ... THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND").
