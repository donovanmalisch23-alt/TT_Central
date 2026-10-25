# Integrating the TeamTalk 5 SDK

TT Central builds against BearWare's **TeamTalk 5 SDK**, which is **proprietary**
(© BearWare.dk) and runs in **trial mode** (~30 days) without a purchased License
Key. Because **this repository is public**, the SDK is **never committed** here —
BearWare's license allows redistributing *your application*, not the SDK itself.
Instead, the SDK is **fetched into place at build time**.

## Fetch it

```bat
python -m pip install py7zr
python scripts/fetch_sdk.py
```

That downloads the SDK from bearware.dk and populates:

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

Pinned version: **v5.23a** (`TEAMTALK_VERSION "5.23.0.5209"`). The build and CI
call `fetch_sdk.py`, so the SDK is in place before compilation — "integrated
directly," just not stored in git.

## Why not commit it?

The TeamTalk 5 SDK License Agreement (fetched to
`third_party/teamtalk-sdk/License.txt`) permits redistributing **end-user
applications** that use the DLLs (with a License Key), **not** the SDK's files
themselves. Committing the SDK headers/lib/DLL — or the proprietary client/server
source — to a public repository would redistribute the SDK and violate the
agreement. Keeping them fetched-and-git-ignored keeps TT Central a clean,
MIT-licensed public project.

> If TT Central were a **private** repository, the SDK could instead be vendored
> directly. That is a project decision — see `ARCHITECTURE.md`.

## Licensing reminder

Trial mode is fine for development and personal/community use. Shipping binaries
widely, or hosting a server as a product, requires a purchased license
(Professional Edition for the server). Keep all BearWare notices intact. Contact:
contact@bearware.dk.
