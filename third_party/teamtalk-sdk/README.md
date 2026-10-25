# TeamTalk 5 SDK (fetched, not committed)

**Proprietary — © BearWare.dk.** Governed by the TeamTalk 5 SDK License
Agreement, **not** by TT Central's GPLv3 license.

This repository is public, so the SDK is **not stored here**. It is downloaded
into this directory at build time by [`../../scripts/fetch_sdk.py`](../../scripts/fetch_sdk.py),
which [`../../scripts/bootstrap.py`](../../scripts/bootstrap.py) runs for you
(it also stages the DLL and import library for the client build):

```bat
python -m pip install py7zr
python scripts/bootstrap.py          REM from the repo root
```

After fetching (all git-ignored):

- `include/TeamTalk.h` — C API header
- `lib/TeamTalk5.lib` — import library
- `bin/TeamTalk5.dll` — runtime library
- `server/tt5srv.exe`, `server/tt5svc.exe` — BearWare's TeamTalk server (console / service)
- `License.txt` — the SDK License Agreement

See [`../../docs/SDK.md`](../../docs/SDK.md) for editions, versions, and why the
SDK is fetched rather than committed. Without a purchased License Key the SDK
runs in **trial mode** and disables after ~30 days. Do not remove any BearWare
notices.
