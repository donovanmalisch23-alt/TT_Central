# Accessibility

TT Central is built for people who use screen readers (NVDA, JAWS, Windows
Narrator) and the keyboard, as much as for anyone else. This page lists what
TT Central guarantees, how it is checked, and the manual checks to do
before a release.

## What TT Central guarantees

- **Everything works from the keyboard.** Every window and dialog can be
  reached from the TT Central menu (Alt+T) or a shortcut, Tab moves through
  every control in a logical order, every label has an Alt+letter key that
  is unique in its window, and Escape closes windows.
- **Every control has a name.** Screen readers announce each field by its
  label; lists, read-only status fields and the queue have names and, where
  useful, descriptions (for example, "Enter plays the selected track now;
  Delete removes it").
- **Status is text you can re-read.** Bot status, now playing, position and
  server status are read-only text fields, not just labels, so a screen
  reader can move to them and read them again.
- **Lists keep your place.** The media queue and the notification lists are
  updated in place, so a status update does not move the screen reader's
  focus or reset its position.
- **Explanations are reachable.** Introductory text on each settings page and
  window can be focused with Tab, so it is not skipped.
- **Notifications are spoken.** Monitor notifications go through the client's
  speech (Prism: NVDA, JAWS, SAPI and others, or Qt text-to-speech), can play
  the TeamTalk sound pack, and can show Windows notifications, each switchable.
  Every notification starts with the server name. "Read last notification"
  (Ctrl+Shift+N) repeats the latest one.
- **Shortcuts never clash** with the TeamTalk client's own.
- **Nothing depends on colour or sound alone.** Every sound has a spoken or
  written equivalent.

## How it is checked

Automatically, in CI on Windows, every build:

- **Accessibility audit** (`src/client/ttc/a11yaudit.cpp`): walks a window
  through Qt's accessibility layer, the same information NVDA and JAWS read,
  and fails on any keyboard-focusable control without an accessible name and
  any button or check box without text. It runs in the unit tests for every
  TT Central window and dialog, and inside the real `TTCentral.exe`
  (`TTC_SELFTEST`) for the Monitors, Media bot and Local server windows, all
  Settings pages and the Add Monitor and Add Account dialogs.
- **Shortcut clash check**: the self-test compares TT Central's shortcuts with
  every action in the client's main window.
- **Packaged builds**: the same self-test runs from the portable package and
  from a silent install.

## Manual checks before a release

Do these with NVDA (latest) and spot-check with JAWS and Narrator, keyboard
only, on Windows 10 and 11:

1. Start TT Central; the TT Central menu opens with Alt+T and every item is
   read with its shortcut.
2. Settings (Ctrl+Shift+P): move through every category with the arrow keys;
   Tab through every field of each page; every field is announced with its
   label; Alt+letter keys work; OK, Cancel and Apply behave as expected.
3. Add a monitor: the one-login-per-IP note is read (it is focusable and is
   also the description of the username and password fields); Import from
   saved server fills the fields; a validation error is announced.
4. Monitors window (Ctrl+Shift+O): monitor statuses are read; notifications
   arrive and are spoken, newest first; Read last, Copy and Clear work.
5. Media bot window (Ctrl+Shift+J): status, now playing and position are
   readable; add a search with Enter; the queue keeps its position while
   playing; Enter plays and Delete removes a queued track; volume changes
   with the arrow keys; replies are read in the log.
6. Local server window (Ctrl+Shift+K): every setting is announced; add,
   edit and remove accounts from the keyboard (Enter edits, Delete removes);
   start and stop the server and hear the status change; the log is
   readable.
7. Notifications: speech, sounds and toasts each switch on and off; Do not
   disturb silences all three.
8. High-contrast mode and 200% scaling: nothing is cut off (settings scroll).
