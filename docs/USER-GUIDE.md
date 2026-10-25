# TT Central User Guide

TT Central is a TeamTalk 5 client for Windows built on BearWare's own
TeamTalk client, with three additions:

- **Monitors** — background logins that tell you who logs in and out, and
  about messages, on any number of servers, while you do other things.
- **Media bot** — a music bot account that plays YouTube, SoundCloud,
  Spotify, Apple Music and internet radio into a channel. People control it
  by private message; you can also control it from TT Central.
- **Local server** — host your own TeamTalk server on this computer.

Everything works from the keyboard and with screen readers (NVDA, JAWS and
Windows Narrator). Every TT Central control has a name your screen reader
announces, and notifications are spoken, played as sounds, or shown as
Windows notifications, as you choose.

## Contents

- [Getting started](#getting-started)
- [Keyboard shortcuts](#keyboard-shortcuts)
- [Monitors](#monitors)
- [Notifications](#notifications)
- [The media bot](#the-media-bot)
- [Media bot commands](#media-bot-commands)
- [Hosting a local server](#hosting-a-local-server)
- [Server administration](#server-administration)
- [Where TT Central keeps its files](#where-tt-central-keeps-its-files)
- [Troubleshooting](#troubleshooting)
- [Licenses](#licenses)

## Getting started

There are two ways to run TT Central:

- **Portable zip.** Unzip `TTCentral-<version>-win64-portable.zip` anywhere
  (a USB stick works) and run `TTCentral.exe`. Settings are kept in
  `TTCentral.ini` next to it, so the folder is self-contained. Nothing is
  installed and no administrator rights are needed.
- **Installer.** Run `TTCentral-<version>-setup.exe`. It installs for your
  user account without administrator rights (or for all users, if you
  choose), adds TT Central to the Start menu, and keeps settings in
  `%APPDATA%\TTCentral`. Uninstalling keeps your settings.

The first time you start TT Central it works like the TeamTalk 5 client:
use **Client > Connect** (F2) to choose a server. Everything TT Central adds
is in the **TT Central** menu (Alt+T), just before Help.

> **About the TeamTalk SDK.** TeamTalk itself is made by BearWare.dk. Without
> a TeamTalk license key, the TeamTalk SDK that TT Central uses runs in trial
> mode. See [Licenses](#licenses).

## Keyboard shortcuts

| Keys | What it does |
|---|---|
| Alt+T | Open the TT Central menu |
| Ctrl+Shift+O | Monitors and notifications window |
| Ctrl+Shift+N | Read the last notification aloud |
| Ctrl+Shift+J | Media bot window |
| Ctrl+Shift+K | Local server window |
| Ctrl+Shift+P | TT Central settings |

All of the TeamTalk client's own shortcuts work as before; see
**TT Central settings > Accessibility and shortcuts** for the list of TT
Central's keys, and the client's Preferences for its hotkeys.

In every TT Central window, the letter underlined in a label (Alt+letter)
moves to that field, Tab moves between controls, Enter in a text field does
the obvious thing (adds a track, sends a command), and Escape closes the
window.

## Monitors

A monitor is a small, separate login that sits in one channel of a server
and tells you about:

- people logging in and out of that server;
- private messages sent to the monitor;
- messages written in the monitor's channel;
- optionally, broadcast messages from the server's administrators.

Every notification starts with the name you gave the monitor, so with
several monitors on several servers you always know where it came from —
for example "Home: Alice logged in".

### Adding a monitor

1. Open **TT Central > TT Central settings** (Ctrl+Shift+P) and choose the
   **Monitors** category.
2. Choose **Add...**. If you have saved servers, **Import from saved server**
   fills in the address, account and channel for you.
3. Give the monitor a **Name shown in notifications**, check the server
   address and ports, and enter the account it should log in with.
4. Choose which channel it sits in (`/` is the root channel) and which events
   it reports.
5. Choose **OK**, then **OK** again to save.

> **One login per IP address.** A monitor can use your own account. But if a
> server allows only one login per IP address, the monitor and your own
> client can't both be logged in from this computer: make a separate account
> for the monitor. TT Central tells you when a server refuses a monitor for
> this reason.

### When monitors run

In **Settings > Monitors**, **Start monitors** chooses:

- **After I log in to a server** (the default) — monitors start the first
  time you log in after starting TT Central;
- **When TT Central starts**;
- **Only when I start them** — use **TT Central > Start all monitors**.

A monitor that loses its connection reconnects by itself, waiting a little
longer each time, and tells you once per outage rather than on every
attempt. If a server refuses its username or password, it stops and says so.

### The Monitors window

**TT Central > Monitors and notifications** (Ctrl+Shift+O) lists every
monitor and its status, and every notification, newest first. You can read
the last one aloud, copy the selected ones, clear the list, and start or
stop all monitors.

## Notifications

**Settings > Notifications** chooses how notifications reach you:

- **Speak** them with your screen reader or the client's text-to-speech;
- **Play sounds** from the TeamTalk sound pack chosen in the client's
  Preferences (Default, Majorly-G or Old);
- **Show Windows notifications**;
- **Do not disturb** silences all of them (they are still listed);
- how many notifications to keep in the list.

**Settings > Sounds** opens the client's sound preferences and can play a
test notification.

## The media bot

The media bot is a separate TeamTalk login (a "bot account") that sits in a
channel and plays music into it. It is a native port of Simple TT Media.

### Setting it up

1. Ask the server's administrator for an account for the bot, or create one
   on your own [local server](#hosting-a-local-server). Give the bot its own
   account: it is online at the same time as you.
2. Open **Settings > Media bot**. Enter the server, the bot's username and
   password, its nickname and the channel it should join.
3. Optionally: list **Bot administrators** (TeamTalk usernames, separated by
   commas) who may use the bot's administrator commands — server
   administrators always can. Set a **Channel command prefix** such as `!`
   if people should be able to type commands in the channel (`!p song`);
   leave it empty to take commands only by private message.
4. Choose **Start the bot when TT Central starts** if you want it always on.
5. Choose **OK**. Then **TT Central > Start the media bot**.

### Controlling it

Anyone in the bot's channel can send it a private message; `h` lists the
commands. You can also use **TT Central > Media bot** (Ctrl+Shift+J):

- **Bot status**, **Now playing** and **Position** are read-only fields your
  screen reader can read at any time;
- **Pause**/**Resume**, **Previous**, **Next**, **Stop playback** and
  **Volume**;
- **Search or link**: type words or paste a link and press Enter to add it
  to the queue;
- **Queue**: press Enter on a track to play it now, Delete to remove it;
- **Command**: any command from the list below;
- **Replies and log**: what the bot answered, and what it announced.

### Sources

- **YouTube** and **SoundCloud**: search by words, or paste a link to a
  track or playlist.
- **Spotify** and **Apple Music**: paste a link (Apple Music can also be
  searched). Their audio is copy-protected, so the bot plays the closest
  match from YouTube. Spotify albums and playlists need a free app from
  developer.spotify.com; enter its client ID and secret in the settings.
- **Direct links and internet radio**: links to audio files and streams
  (`url <link>`).

The bot uses three helper programs that come with TT Central in the
`tools` folder: **ffmpeg** (decodes audio), **yt-dlp** (finds the audio
streams) and **Deno** (which yt-dlp needs for YouTube). If YouTube links
stop working, a bot administrator can send `update` to update yt-dlp.

If the bot stops unexpectedly, TT Central starts it again (at most five
times in ten minutes). Its log is in `mediabot.log` (see
[Where TT Central keeps its files](#where-tt-central-keeps-its-files)).

## Media bot commands

Send these to the bot in a private message (or in its channel with the
prefix, if one is set). `h <command>` explains any one of them.

| Command | What it does |
|---|---|
| `p <words or link>` | Play or queue the top search result, or a link (track, playlist, album). `p` alone pauses or resumes; `p 3` plays result 3 of your last search. |
| `s <words>` | Search and list numbered results |
| `sel <number>` | Play a numbered result from your last search |
| `url <link>` | Play a direct media file or radio stream |
| `pause`, `resume`, `pp` | Pause, resume, or toggle |
| `next` (`n`), `prev` (`b`) | Next or previous track |
| `stop` | Stop and clear the queue |
| `seek <m:ss>`, `ff [s]`, `rw [s]` | Jump to a position, or 15 (or s) seconds forward or back |
| `v [0-max]` | Show or set the volume (percent) |
| `q` | Show what's playing and the queue |
| `np` | Now playing, with position and link |
| `go <n>` | Play queued track n now |
| `rm <n>`, `mv <from> <to>`, `clear` | Remove, move, or clear queued tracks |
| `repeat off/track/queue` | Repeat mode |
| `shuffle [on/off/queue]` | Toggle shuffle, or shuffle the queue once |
| `sv [name]`, `services` | Show or change which service searches use |
| `lock` | Administrators lock the bot with a password; anyone unlocks it with that password |
| `about`, `h` | About the bot, and help |
| `say <text>` | (administrators) Post a message in the channel |
| `update` | (administrators) Update yt-dlp |
| `quit` | (administrators) Shut the bot down |

## Hosting a local server

**TT Central > Local server** (Ctrl+Shift+K) hosts a TeamTalk server on this
computer, using BearWare's TeamTalk server program.

1. Enter a **Server name** and, if you like, a **Welcome message**.
2. Keep the default ports (10333) unless you need others.
3. Under **Accounts**, choose **Add account...** and create at least one
   **Administrator**, then accounts for the people who will connect.
4. Choose **Start server**. The status says when it is accepting
   connections.
5. Choose **Add to my server list** to connect to it yourself.

Good to know:

- People on your own network connect to this computer's address. For people
  on the internet, your router must forward the TCP and UDP ports to this
  computer, or turn on **Ask my router to open the ports (UPnP)**.
- Windows may ask whether to let the TeamTalk server through the firewall.
  Allow it for the networks people will connect from.
- This server does not encrypt connections.
- Saving settings while the server runs restarts it, which briefly
  disconnects everyone.
- The server keeps its own state (channels, bans, statistics) in
  `tt5srv.xml` in the server folder; TT Central leaves those untouched.
- **Limits and security** sets the most users online, logins per IP
  address, how many wrong passwords lead to a ban, and a delay between
  logins. **File sharing** chooses a folder for files people upload.
- If the server stops unexpectedly, TT Central restarts it. **Start the
  server when TT Central starts** keeps it running whenever TT Central is.

## Server administration

When you are logged in to a server as an administrator,
**TT Central > Server administration** opens the client's own tools for it:
server properties, user accounts, banned users, online users, statistics,
broadcast messages, and saving the server's configuration. Items the client
can't use right now (for example, when you are not connected, or not an
administrator) are unavailable.

## Where TT Central keeps its files

| What | Portable | Installed |
|---|---|---|
| Settings (`TTCentral.ini`) | next to `TTCentral.exe` | `%APPDATA%\TTCentral` |
| Media bot settings copy and log (`mediabot.ini`, `mediabot.log`) | same folder as the settings | same |
| Local server (`server\tt5srv.xml`, `server\tt5srv-console.log`) | `server` in that folder | `%APPDATA%\TTCentral\server` |

Monitors, notifications and the media bot's settings are all in
`TTCentral.ini`. Passwords there are stored as written, like the TeamTalk
client does; keep the file private.

## Troubleshooting

**A monitor says the server allows only one login per IP address.** Create a
separate account for the monitor (or the media bot) and use it in its
settings.

**The media bot says ffmpeg was not found.** The `tools` folder next to
`TTCentral.exe` is missing or incomplete. Use the portable zip or installer
again, or set the paths in **Settings > Media bot > Helper programs**.

**YouTube links fail.** Send the bot `update` (as an administrator), or
replace `tools\yt-dlp\yt-dlp.exe` with the latest release.

**The local server says the port is in use.** Another program (maybe another
TeamTalk server) uses that port. Stop it or choose another port.

**People outside my network can't reach my server.** Forward the TCP and UDP
ports on your router to this computer (or turn on UPnP), and allow the
server through Windows Firewall.

**Nothing is spoken.** Check **Settings > Notifications > Speak** and the
client's text-to-speech settings in **Client > Preferences**.

## Licenses

TT Central is free software under the GNU General Public License, version 3
or later (`LICENSE.txt`), with an additional permission to link the TeamTalk
5 SDK (`NOTICE.txt`). It is built on BearWare.dk's TeamTalk 5 client (GPLv3)
and the proprietary TeamTalk 5 SDK, which runs in trial mode without a
license key from BearWare.dk (`TeamTalk-SDK-License.txt`). ffmpeg (GPL),
yt-dlp (Unlicense), Deno (MIT), Prism (MPL-2.0) and WinToast (MIT) keep their
own licenses (`THIRD-PARTY-LICENSES.md`). The media bot is a port of Simple
TT Media (MIT).
