# src/ — module overview

TT Central is organized into separable modules so that as much as possible ships
under MIT and the proprietary TeamTalk dependency stays isolated. See
[`../docs/ARCHITECTURE.md`](../docs/ARCHITECTURE.md).

| Dir | Module | Responsibility | Phase |
|---|---|---|---|
| `core/`    | tt-core     | Thin C++ wrapper over the TeamTalk SDK (instances, event pump) | 0–1 |
| `app/`     | ttc-shell   | The extended qtTeamTalk Qt application (main window, panels) | 1 |
| `notify/`  | ttc-notify  | Notification subsystem: sound pack + screen-reader/TTS + toasts | 2 |
| `monitor/` | ttc-monitor | Background watcher logins; raises events tagged by server | 2 |
| `server/`  | ttc-server  | Host a local TeamTalk server + remote admin | 3 |
| `media/`   | ttc-media   | Native media engine (ffmpeg → InsertAudioBlock), queue, commands | 4 |

Each directory currently holds a placeholder; real sources and per-module
`CMakeLists.txt` files are added as their phase lands.
