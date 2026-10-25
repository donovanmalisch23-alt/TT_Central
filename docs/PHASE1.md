# Phase 1 — Unified shell + client

Goal: TT Central opens as one accessible window that is a fully working TeamTalk
5 client, built by extending BearWare's qtTeamTalk, with our changes carried as
patches over BearWare's GPLv3 client source, pinned as a submodule (see
[`ARCHITECTURE.md`](ARCHITECTURE.md)).

## Verification strategy

This project targets **Windows + Qt + MSVC**. Development happens on Linux too,
so the **build-and-test harness is GitHub Actions on a Windows runner** — every
milestone must leave CI green. Accessibility is verified by **manual NVDA (free)
and JAWS passes on Windows**, which cannot be fully automated; those are called
out per milestone.

## Milestones (each a small PR, CI green)

- **1.0 — SDK smoke test.** `ttc-sdkcheck` links the SDK and prints its version.
  Proves the toolchain + fetched-SDK wiring. *Status: **done** — green on
  Windows CI, printing `TeamTalk SDK version: 5.23.0.5209`.*
- **1.1 — Upstream integration.** Pin `BearWare/TeamTalk5` at tag `v5.23`
  (submodule `external/TeamTalk5`), add `patches/TeamTalk5/` and
  `scripts/bootstrap.py`, and build **qtTeamTalk with Prism screen-reader
  support** in CI with Qt 6.12, then launch-test it. With the Standard SDK the
  client builds from BearWare's source **unchanged** (no patch needed yet).
  *Status: implemented; bootstrap tested locally (fresh run, re-run, CRLF
  checkout, conflicting edit); awaiting a green Windows CI run.*
- **1.2 — `tt-core` wrapper.** Thin C++ wrapper over the SDK (init, connect,
  login, join, event pump) with unit tests. GPLv3, separable.
- **1.3 — TT Central shell.** Main window + panel framework hosting the client
  view; rebrand. Preserve qtTeamTalk accessibility (first NVDA pass).
- **1.4 — Client parity.** Voice, text, channels, file transfer working end to
  end against a real server; keyboard-only operation; NVDA/JAWS pass.
- **1.5 — Packaging.** Portable zip + installer that produce a runnable,
  accessible client. Closes Phase 1.

## Done when

TT Central launches, connects to a TeamTalk server, and works as a client at
parity with qtTeamTalk, keyboard-driven and screen-reader-verified, shipped as
both a portable zip and an installer — with CI green throughout.
