# Changelog

## 1.0.0

The first complete release of TT Central.

- **Client**: BearWare's TeamTalk 5 client (v5.23), branded TT Central, with
  the TT Central menu (Alt+T), Prism screen-reader output and the TeamTalk
  sound packs. Settings in `TTCentral.ini` (portable) or `%APPDATA%\TTCentral`.
- **Monitors**: any number of background logins, each in a server channel,
  reporting logins, logouts, private messages, channel messages and
  (optionally) broadcasts, every notification naming its server. Start after
  login, at startup or by hand; reconnect by themselves; explain servers that
  allow one login per IP address.
- **Notifications**: speech, TeamTalk sounds and Windows notifications, a
  history, Read last notification (Ctrl+Shift+N), do not disturb.
- **Media bot** (`ttc-mediabot`): a native C++ port of Simple TT Media that
  plays YouTube, SoundCloud, Spotify, Apple Music, direct links and radio
  into a channel as voice; every chat command, admins, lock, channel prefix,
  reconnects; a Media bot window (Ctrl+Shift+J) and settings page; restarts
  if it stops.
- **Local server**: host BearWare's TeamTalk server from TT Central
  (Ctrl+Shift+K): settings, accounts, limits, file sharing, log, crash
  restart, port checks; add it to your server list.
- **Server administration** menu for the client's own admin tools.
- **Accessibility**: every window keyboard-operable and named for screen
  readers, checked in CI by an audit inside the real client; no shortcut
  clashes with the client; first-run welcome; user guide.
- **Packages**: portable zip and per-user installer, both tested in CI.
