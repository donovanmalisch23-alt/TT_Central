# TeamTalk sound packs

This directory holds the **original TeamTalk sound packs** bundled with TT
Central (event sounds for logins, logouts, messages, etc.), used by the
notification subsystem (`src/notify`).

**Proprietary — © BearWare.dk.** These are assets from the official TeamTalk 5
client. They are bundled under the same trial/community posture as the SDK;
keep BearWare's notices and do not relicense them under MIT. See `../../NOTICE`.

## To add them

Copy the sound pack folders from an official TeamTalk 5 client installation (its
`Sounds/` / sound-pack directory) into this folder, preserving each pack's
structure so individual event sounds can be mapped in **Settings → Sounds**.

Layout (planned):

```
assets/soundpacks/
└── <pack name>/
    ├── <event>.wav
    └── ...
```
