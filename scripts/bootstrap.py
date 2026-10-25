#!/usr/bin/env python3
"""Prepare a TT Central checkout for building.

Steps (each is safe to re-run):

1. Make sure the pinned upstream source (external/TeamTalk5, BearWare's
   TeamTalk5 repository at tag v5.23) is checked out as a git submodule.
2. Fetch the proprietary TeamTalk 5 SDK with scripts/fetch_sdk.py (only when it
   is missing, or with --refresh-sdk). It is never committed to this repo.
3. Copy the SDK runtime + import library into
   external/TeamTalk5/Library/TeamTalk_DLL, where the qtTeamTalk client build
   looks for them (upstream git-ignores *.dll and *.lib there).
4. Check that the upstream TeamTalk.h and the SDK's TeamTalk.h are the same
   version. The client refuses to start if the header and DLL differ.
5. Apply TT Central's patches (patches/TeamTalk5/*.patch, in order) to the
   submodule. Patches that are already applied are skipped.

Usage:
    python scripts/bootstrap.py
    python scripts/bootstrap.py --refresh-sdk
"""
from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
UPSTREAM = REPO_ROOT / "external" / "TeamTalk5"
UPSTREAM_DLL_DIR = UPSTREAM / "Library" / "TeamTalk_DLL"
SDK_DIR = REPO_ROOT / "third_party" / "teamtalk-sdk"
PATCH_DIR = REPO_ROOT / "patches" / "TeamTalk5"

SDK_FILES = (("bin", "TeamTalk5Pro.dll"), ("lib", "TeamTalk5Pro.lib"))


class BootstrapError(Exception):
    pass


def run(cmd: list[str], cwd: Path | None = None, check: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, cwd=cwd, check=check, text=True, capture_output=True)


def step(msg: str) -> None:
    print(f"==> {msg}", flush=True)


def ensure_submodule() -> None:
    step("Upstream source (external/TeamTalk5)")
    if not (UPSTREAM / "Client" / "qtTeamTalk" / "CMakeLists.txt").exists():
        print("    initializing submodule ...")
        res = run(["git", "submodule", "update", "--init", "--depth", "1", "external/TeamTalk5"],
                  cwd=REPO_ROOT, check=False)
        if res.returncode != 0:
            raise BootstrapError("could not initialize external/TeamTalk5:\n" + res.stderr.strip())
    head = run(["git", "rev-parse", "--short", "HEAD"], cwd=UPSTREAM).stdout.strip()
    print(f"    at commit {head}")


def ensure_sdk(refresh: bool) -> None:
    step("TeamTalk 5 SDK (proprietary; fetched, not committed)")
    have = all((SDK_DIR / sub / name).exists() for sub, name in SDK_FILES) and \
        (SDK_DIR / "include" / "TeamTalk.h").exists()
    if have and not refresh:
        print("    already present (use --refresh-sdk to download again)")
        return
    res = subprocess.run([sys.executable, str(REPO_ROOT / "scripts" / "fetch_sdk.py")], cwd=REPO_ROOT)
    if res.returncode != 0:
        raise BootstrapError("fetch_sdk.py failed")


def stage_sdk_for_client() -> None:
    step("Staging SDK files for the client build")
    UPSTREAM_DLL_DIR.mkdir(parents=True, exist_ok=True)
    for sub, name in SDK_FILES:
        src = SDK_DIR / sub / name
        if not src.exists():
            raise BootstrapError(f"missing {src}; run with --refresh-sdk")
        shutil.copy2(src, UPSTREAM_DLL_DIR / name)
        print(f"    {name} -> external/TeamTalk5/Library/TeamTalk_DLL/")


def header_version(path: Path) -> str:
    m = re.search(r'#define\s+TEAMTALK_VERSION\s+"([^"]+)"', path.read_text(encoding="utf-8", errors="replace"))
    if not m:
        raise BootstrapError(f"TEAMTALK_VERSION not found in {path}")
    return m.group(1)


def check_versions() -> None:
    step("Header / DLL version match")
    upstream_v = header_version(UPSTREAM_DLL_DIR / "TeamTalk.h")
    sdk_v = header_version(SDK_DIR / "include" / "TeamTalk.h")
    if upstream_v != sdk_v:
        raise BootstrapError(
            f"version mismatch: upstream TeamTalk.h is {upstream_v} but the SDK is {sdk_v}. "
            "Pin the submodule and the SDK to the same TeamTalk release.")
    print(f"    both {upstream_v}")


def apply_patches() -> None:
    step("Applying TT Central patches to external/TeamTalk5")
    patches = sorted(PATCH_DIR.glob("*.patch"))
    if not patches:
        print("    (no patches)")
        return
    for patch in patches:
        p = str(patch)
        # Already applied?
        if run(["git", "apply", "--reverse", "--check", p], cwd=UPSTREAM, check=False).returncode == 0:
            print(f"    already applied: {patch.name}")
            continue
        # Apply exactly; if line endings differ (e.g. core.autocrlf), retry
        # tolerating whitespace differences in context lines.
        for extra in ([], ["--ignore-whitespace"]):
            if run(["git", "apply", "--check", *extra, p], cwd=UPSTREAM, check=False).returncode == 0:
                run(["git", "apply", *extra, p], cwd=UPSTREAM)
                note = " (ignoring whitespace)" if extra else ""
                print(f"    applied: {patch.name}{note}")
                break
        else:
            raise BootstrapError(
                f"{patch.name} does not apply to external/TeamTalk5.\n"
                "    The submodule probably has local changes. To reset it (this DISCARDS\n"
                "    any edits inside external/TeamTalk5):\n"
                "        git -C external/TeamTalk5 checkout -- .\n"
                "    then run this script again.")


def main() -> int:
    ap = argparse.ArgumentParser(description="Prepare a TT Central checkout for building.")
    ap.add_argument("--refresh-sdk", action="store_true", help="download the SDK again even if present")
    args = ap.parse_args()
    try:
        ensure_submodule()
        ensure_sdk(args.refresh_sdk)
        stage_sdk_for_client()
        check_versions()
        apply_patches()
    except BootstrapError as e:
        print(f"\nerror: {e}", file=sys.stderr)
        return 1
    except subprocess.CalledProcessError as e:
        print(f"\nerror: {' '.join(e.cmd)} failed:\n{e.stderr}", file=sys.stderr)
        return 1
    print("\nBootstrap complete. Next:  cmake -S . -B build -A x64  &&  cmake --build build --config Release")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
