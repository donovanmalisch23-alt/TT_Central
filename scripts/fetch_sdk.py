#!/usr/bin/env python3
"""Fetch the TeamTalk 5 SDK into third_party/teamtalk-sdk/.

TT Central builds against BearWare's proprietary TeamTalk 5 SDK. The SDK must
NOT be committed to this (public) repository, so this script downloads it from
bearware.dk at build time and copies the headers, import library, runtime DLL,
and BearWare's prebuilt TeamTalk server into place. Without a purchased License
Key the SDK runs in trial mode. See docs/SDK.md and NOTICE.

TT Central uses the **Standard** edition by default: it is what the official
TeamTalk client ships with, it supports encrypted client connections, and its
server (tt5srv) runs without a BearWare WebLogin.

Requires py7zr to extract the .7z archive:  python -m pip install py7zr

Usage:
    python scripts/fetch_sdk.py                      # Standard, v5.23a, win64
    python scripts/fetch_sdk.py --edition pro        # Professional edition
    python scripts/fetch_sdk.py --version 5.23a --platform win64
"""
from __future__ import annotations

import argparse
import shutil
import sys
import tempfile
import urllib.request
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SDK_DIR = REPO_ROOT / "third_party" / "teamtalk-sdk"
BASE_URL = "https://www.bearware.dk/teamtalksdk"
DEFAULT_VERSION = "5.23a"


def pkg_prefix(edition: str) -> str:
    return "tt5prosdk" if edition == "pro" else "tt5sdk"


def lib_base(edition: str) -> str:
    return "TeamTalk5Pro" if edition == "pro" else "TeamTalk5"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--edition", choices=["standard", "pro"], default="standard",
                    help="SDK edition (default: standard)")
    ap.add_argument("--version", default=DEFAULT_VERSION, help=f"SDK version (default: {DEFAULT_VERSION})")
    ap.add_argument("--platform", default="win64", help="SDK platform package (default: win64)")
    args = ap.parse_args()

    try:
        import py7zr
    except ImportError:
        print("error: this script needs py7zr to extract the .7z archive.\n"
              "       install it with:  python -m pip install py7zr", file=sys.stderr)
        return 2

    base = f"{pkg_prefix(args.edition)}_v{args.version}_{args.platform}"
    archive_name = f"{base}.7z"
    url = f"{BASE_URL}/v{args.version}/{archive_name}"
    lib = lib_base(args.edition)
    windows = args.platform.startswith("win")
    runtime = f"{lib}.dll" if windows else f"lib{lib}.so"
    implib = f"{lib}.lib" if windows else None

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        archive = tmp / archive_name

        print(f"Downloading {url}")
        urllib.request.urlretrieve(url, archive)
        print(f"  {archive.stat().st_size:,} bytes")

        print("Extracting ...")
        with py7zr.SevenZipFile(archive, "r") as z:
            z.extractall(path=tmp)

        root = tmp / base
        src = root / "Library" / "TeamTalk_DLL"
        if not src.is_dir():
            print(f"error: expected SDK layout not found at {src}", file=sys.stderr)
            return 1

        for sub in ("include", "lib", "bin", "server"):
            (SDK_DIR / sub).mkdir(parents=True, exist_ok=True)

        copied = []
        for header in ("TeamTalk.h", "TeamTalkSrv.h"):
            p = src / header
            if p.exists():
                shutil.copy2(p, SDK_DIR / "include" / header)
                copied.append(f"include/{header}")

        required = [(runtime, "bin")] + ([(implib, "lib")] if implib else [])
        for fname, sub in required:
            p = src / fname
            if not p.exists():
                print(f"error: {fname} not found in the SDK package", file=sys.stderr)
                return 1
            shutil.copy2(p, SDK_DIR / sub / fname)
            copied.append(f"{sub}/{fname}")

        # BearWare's prebuilt TeamTalk server (used by TT Central's local
        # hosting): tt5srv/tt5svc (Standard) or tt5prosrv/tt5prosvc (Pro).
        server_dir = root / "Server"
        for p in sorted(server_dir.glob("tt5*")):
            if p.is_file() and p.suffix.lower() in ("", ".exe"):
                shutil.copy2(p, SDK_DIR / "server" / p.name)
                copied.append(f"server/{p.name}")

        license_txt = root / "License.txt"
        if license_txt.exists():
            shutil.copy2(license_txt, SDK_DIR / "License.txt")
            copied.append("License.txt")

    print("Copied into third_party/teamtalk-sdk/:")
    for c in copied:
        print(f"  {c}")
    print("\nDone. The SDK runs in TRIAL MODE without a purchased license key.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
