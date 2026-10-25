# Phase 1 — Unified shell + client

Goal: TT Central opens as one accessible window that is a fully working TeamTalk
5 client, built by extending BearWare's qtTeamTalk, with our changes carried as
patches over a pinned upstream (public-repo posture — see
[`ARCHITECTURE.md`](ARCHITECTURE.md)).

## Verification strategy

This project targets **Windows + Qt + MSVC**. Development happens on Linux too,
so the **build-and-test harness is GitHub Actions on a Windows runner** — every
milestone must leave CI green. Accessibility is verified by **manual NVDA (free)
and JAWS passes on Windows**, which cannot be fully automated; those are called
out per milestone.

## Milestones (each a small PR, CI green)

- **1.0 — SDK smoke test.** `ttc-sdkcheck` links the SDK and prints its version.
  Proves the toolchain + fetched-SDK wiring. *Status: implemented; source
  compiles clean (`-Werror`); CI builds and runs it on Windows.*
- **1.1 — Upstream integration.** Add `bearware/TeamTalk5` at a pinned tag
  (git submodule) + a `patches/` dir + a bootstrap script; build **vanilla
  qtTeamTalk** in CI. Qt returns to the workflow here.
- **1.2 — `tt-core` wrapper.** Thin C++ wrapper over the SDK (init, connect,
  login, join, event pump) with unit tests. MIT, separable.
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
