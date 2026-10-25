#!/usr/bin/env python3
"""Fetch the TeamTalk 5 SDK into third_party/teamtalk-sdk/.

TT Central builds against BearWare's proprietary TeamTalk 5 SDK. The SDK must
NOT be committed to this (public) repository, so this script downloads it from
bearware.dk at build time and copies the headers, import library, and runtime
DLL into place. Without a purchased License Key the SDK runs in trial mode and
disables after ~30 days. See docs/SDK.md and NOTICE.

Requires py7zr to extract the .7z archive:  python -m pip install py7zr

Usage:
    python scripts/fetch_sdk.py                      # Professional, v5.23a, win64
    python scripts/fetch_sdk.py --edition standard   # Standard edition
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


def pkg_prefix(edition: str) -> str:
    return "tt5prosdk" if edition == "pro" else "tt5sdk"


def dll_lib_names(edition: str) -> tuple[str, str]:
    if edition == "pro":
        return "TeamTalk5Pro.dll", "TeamTalk5Pro.lib"
    return "TeamTalk5.dll", "TeamTalk5.lib"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--edition", choices=["pro", "standard"], default="pro",
                    help="SDK edition (pro adds the server API; default: pro)")
    ap.add_argument("--version", default="5.23a", help="SDK version (default: 5.23a)")
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
    dll_name, lib_name = dll_lib_names(args.edition)

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        archive = tmp / archive_name

        print(f"Downloading {url}")
        urllib.request.urlretrieve(url, archive)
        print(f"  {archive.stat().st_size:,} bytes")

        print("Extracting ...")
        with py7zr.SevenZipFile(archive, "r") as z:
            z.extractall(path=tmp)

        src = tmp / base / "Library" / "TeamTalk_DLL"
        if not src.is_dir():
            print(f"error: expected SDK layout not found at {src}", file=sys.stderr)
            return 1

        for sub in ("include", "lib", "bin"):
            (SDK_DIR / sub).mkdir(parents=True, exist_ok=True)

        copied = []
        for header in ("TeamTalk.h", "TeamTalkSrv.h"):
            p = src / header
            if p.exists():
                shutil.copy2(p, SDK_DIR / "include" / header)
                copied.append(f"include/{header}")

        for fname, sub in ((lib_name, "lib"), (dll_name, "bin")):
            p = src / fname
            if not p.exists():
                print(f"error: {fname} not found in the SDK package", file=sys.stderr)
                return 1
            shutil.copy2(p, SDK_DIR / sub / fname)
            copied.append(f"{sub}/{fname}")

        license_txt = tmp / base / "License.txt"
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
