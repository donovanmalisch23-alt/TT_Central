# TeamTalk sound packs

TT Central uses the **original TeamTalk sound packs** for its event sounds
(logins, logouts, private and channel messages, server lost, and more). They are
used by the notification subsystem (`src/notify`) and chosen in
**Settings → Sounds**.

The packs are **not copied into this repository**. They come from BearWare's
upstream source, pinned as the submodule `external/TeamTalk5`:

```
external/TeamTalk5/Setup/Client/Sounds/
├── *.wav          Default pack
├── Majorly-G/     alternative pack
└── Old/           classic pack
```

The packs are BearWare assets (© BearWare.dk); keep their notices. See
`../../NOTICE`.

This folder is reserved for packaging (milestone 1.5) and for any future
TT Central-specific sounds.
