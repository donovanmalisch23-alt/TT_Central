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
├── include/   TeamTalk.h                    (fetched; git-ignored)
├── lib/       TeamTalk5.lib                 (fetched; git-ignored)
├── bin/       TeamTalk5.dll                 (fetched; git-ignored)
├── server/    tt5srv.exe, tt5svc.exe        (fetched; git-ignored)
└── License.txt  TeamTalk 5 SDK License       (fetched; git-ignored)
```

## Which edition: Standard

TT Central uses the **Standard** edition (`TeamTalk5.dll`):

- It is the edition the official TeamTalk client ships with, so the client
  builds from BearWare's source **with no changes**.
- Encrypted client connections work in both editions (per `TeamTalk.h`: "As of
  TeamTalk v5.5 and onwards encryption is available in both TeamTalk SDK
  Standard and Professional").
- Its server, `tt5srv`, starts **without** a BearWare.dk WebLogin, which makes
  local hosting simple. (The Professional server, `tt5prosrv`, refuses to run
  until a BearWare.dk WebLogin authorized for it is entered.)
- A Standard license key is the cheaper one, if you buy a key later.

The Standard server does not support encryption. Encrypted *hosting* needs the
Professional edition and a BearWare.dk WebLogin; `fetch_sdk.py --edition pro`
downloads it.

Pinned version: **v5.23a** (`TEAMTALK_VERSION "5.23.0.5209"`), matching the
upstream client source pinned at tag `v5.23` (identical `TeamTalk.h`). CI runs
`bootstrap.py`, so the SDK is in place before compilation — "integrated
directly," just not stored in git.

## Why not commit it?

The TeamTalk 5 SDK License Agreement (fetched to
`third_party/teamtalk-sdk/License.txt`) permits redistributing **end-user
applications** that use the DLLs (with a License Key), **not** the SDK's files
themselves. Committing the SDK DLL, import library, headers, or server binaries
to this public repository would redistribute the SDK and violate the agreement.
Keeping them fetched and git-ignored keeps the public repo free of proprietary
files.

(The qtTeamTalk *client source* is different: it is GPLv3, and the repo pins it
as a submodule — see `ARCHITECTURE.md`.)

## Licensing reminder

Trial mode is fine for development and personal/community use. Shipping binaries
widely requires a purchased License Key. Keep all BearWare notices intact.
Contact: contact@bearware.dk.

## The trial notice on Windows

On Windows the trial SDK shows a one-time message box, "Activating TeamTalk
SDK Trial", the first time a client instance is created on a computer, and
then records the activation in the registry (`HKCU\Software\BearWare.dk`).
People running TT Central simply choose OK.

Unattended machines have nobody to press OK, so the box would block every
test that creates a client. CI therefore runs
`ttc-sdkcheck --activate-trial` before the tests: it creates one instance,
presses OK and prints the notice. Tests and the CI self-test also press it
if it appears (`ttc::dismissSdkNoticesInBackground()`, used only there).
