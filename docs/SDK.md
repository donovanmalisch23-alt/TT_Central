# Integrating the TeamTalk 5 SDK

TT Central builds against BearWare's **TeamTalk 5 SDK**, which is **proprietary**
(© BearWare.dk) and runs in **trial mode** (~30 days) without a purchased License
Key. Because **this repository is public**, the SDK is **never committed** here —
BearWare's license allows redistributing *your application*, not the SDK itself.
Instead, the SDK is **fetched into place at build time**.

## Fetch it

```bat
python -m pip install py7zr
python scripts/bootstrap.py
```

`bootstrap.py` runs `fetch_sdk.py` when the SDK is missing, then stages the DLL
and import library into `external/TeamTalk5/Library/TeamTalk_DLL/` (where the
qtTeamTalk client build expects them) and checks that the header and DLL
versions match. `fetch_sdk.py` alone downloads the SDK from bearware.dk and
populates:

```
third_party/teamtalk-sdk/
├── include/   TeamTalk.h, TeamTalkSrv.h     (fetched; git-ignored)
├── lib/       TeamTalk5Pro.lib              (fetched; git-ignored)
├── bin/       TeamTalk5Pro.dll              (fetched; git-ignored)
└── License.txt  TeamTalk 5 SDK License       (fetched; git-ignored)
```

Options:

```bat
python scripts/fetch_sdk.py --edition standard    REM Standard edition (no server API)
python scripts/fetch_sdk.py --version 5.23a --platform win64
```

- **Professional** (default) provides the client API (`TeamTalk.h`) **and** the
  server API (`TeamTalkSrv.h`) used for local hosting (Phase 3), via
  `TeamTalk5Pro.dll` / `TeamTalk5Pro.lib`.
- **Standard** provides the client API only, via `TeamTalk5.dll` /
  `TeamTalk5.lib`.

Pinned version: **v5.23a** (`TEAMTALK_VERSION "5.23.0.5209"`), matching the
upstream client source pinned at tag `v5.23` (identical `TeamTalk.h`). CI runs
`bootstrap.py`, so the SDK is in place before compilation — "integrated
directly," just not stored in git.

## Why not commit it?

The TeamTalk 5 SDK License Agreement (fetched to
`third_party/teamtalk-sdk/License.txt`) permits redistributing **end-user
applications** that use the DLLs (with a License Key), **not** the SDK's files
themselves. Committing the SDK DLL, import library, or headers to this public
repository would redistribute the SDK and violate the agreement. Keeping them
fetched and git-ignored keeps the public repo free of proprietary files.

(The qtTeamTalk *client source* is different: it is GPLv3, and the repo pins it
as a submodule — see `ARCHITECTURE.md`.)

## Licensing reminder

Trial mode is fine for development and personal/community use. Shipping binaries
widely, or hosting a server as a product, requires a purchased license
(Professional Edition for the server). Keep all BearWare notices intact. Contact:
contact@bearware.dk.
