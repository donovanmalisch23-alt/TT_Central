#!/usr/bin/env python3
"""Fetch the media bot's helper tools into tools/ (git-ignored).

TT Central's media bot decodes audio with ffmpeg and finds streams with yt-dlp,
which needs a JavaScript runtime (Deno) for YouTube. They are separate,
independently licensed programs, so they are downloaded rather than committed:

  tools/ffmpeg/ffmpeg[.exe]   FFmpeg, GPL build by BtbN (GPLv3)
  tools/yt-dlp/yt-dlp[.exe]   yt-dlp (The Unlicense)
  tools/deno/deno[.exe]       Deno (MIT)

The media bot looks for them there (and next to its executable in a packaged
build), then on PATH. Downloads are cached in tools/.cache/ and re-used unless
--refresh is given.

Usage:
    python scripts/fetch_tools.py                  # all tools, for this OS
    python scripts/fetch_tools.py --only ffmpeg yt-dlp
    python scripts/fetch_tools.py --platform win64 --refresh
"""
from __future__ import annotations

import argparse
import io
import os
import shutil
import stat
import subprocess
import sys
import tarfile
import urllib.request
import zipfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_DEST = REPO_ROOT / "tools"

TOOLS = {
    "win64": {
        "ffmpeg": ("https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/"
                   "ffmpeg-master-latest-win64-gpl.zip", "zip", "bin/ffmpeg.exe", "ffmpeg.exe"),
        "yt-dlp": ("https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe",
                   "file", None, "yt-dlp.exe"),
        "deno": ("https://github.com/denoland/deno/releases/latest/download/deno-x86_64-pc-windows-msvc.zip",
                 "zip", "deno.exe", "deno.exe"),
    },
    "linux64": {
        "ffmpeg": ("https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/"
                   "ffmpeg-master-latest-linux64-gpl.tar.xz", "tar", "bin/ffmpeg", "ffmpeg"),
        "yt-dlp": ("https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp_linux",
                   "file", None, "yt-dlp"),
        "deno": ("https://github.com/denoland/deno/releases/latest/download/deno-x86_64-unknown-linux-gnu.zip",
                 "zip", "deno", "deno"),
    },
}

LICENSE_NOTES = {
    "ffmpeg": "FFmpeg (https://ffmpeg.org), GPL build from https://github.com/BtbN/FFmpeg-Builds.\n"
              "Licensed under the GNU GPL version 3. Source: https://ffmpeg.org/download.html\n",
    "yt-dlp": "yt-dlp (https://github.com/yt-dlp/yt-dlp), released under The Unlicense.\n",
    "deno": "Deno (https://deno.land), MIT License. https://github.com/denoland/deno/blob/main/LICENSE.md\n",
}


def default_platform() -> str:
    return "win64" if sys.platform == "win32" else "linux64"


def download(url: str, target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    tmp = target.with_suffix(target.suffix + ".part")
    print(f"  downloading {url}")
    req = urllib.request.Request(url, headers={"User-Agent": "TT-Central-fetch-tools"})
    with urllib.request.urlopen(req, timeout=120) as resp, open(tmp, "wb") as out:  # noqa: S310
        total = int(resp.headers.get("Content-Length") or 0)
        done = 0
        while True:
            chunk = resp.read(1 << 20)
            if not chunk:
                break
            out.write(chunk)
            done += len(chunk)
            if total and sys.stdout.isatty():
                print(f"\r  {done * 100 // total:3d}% of {total // 1048576} MB", end="", flush=True)
    if total and sys.stdout.isatty():
        print()
    if total and done != total:
        tmp.unlink(missing_ok=True)
        raise RuntimeError(f"incomplete download of {url} ({done} of {total} bytes)")
    tmp.replace(target)


def extract_member(archive: Path, kind: str, member_suffix: str, out_file: Path) -> None:
    """Copy the one file whose path ends with member_suffix out of the archive."""
    out_file.parent.mkdir(parents=True, exist_ok=True)
    if kind == "zip":
        with zipfile.ZipFile(archive) as z:
            names = [n for n in z.namelist() if n.replace("\\", "/").endswith(member_suffix)]
            if not names:
                raise RuntimeError(f"{member_suffix} not found in {archive.name}")
            with z.open(sorted(names, key=len)[0]) as src, open(out_file, "wb") as dst:
                shutil.copyfileobj(src, dst)
    elif kind == "tar":
        with tarfile.open(archive) as t:
            members = [m for m in t.getmembers() if m.isfile() and m.name.endswith(member_suffix)]
            if not members:
                raise RuntimeError(f"{member_suffix} not found in {archive.name}")
            src = t.extractfile(sorted(members, key=lambda m: len(m.name))[0])
            with open(out_file, "wb") as dst:
                shutil.copyfileobj(src, dst)
    else:
        raise ValueError(kind)


def make_executable(p: Path) -> None:
    if os.name != "nt":
        p.chmod(p.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def tool_version(exe: Path, name: str) -> str:
    args = {"ffmpeg": ["-version"], "yt-dlp": ["--version"], "deno": ["--version"]}[name]
    try:
        out = subprocess.run([str(exe), *args], capture_output=True, text=True, timeout=60)
        return (out.stdout or out.stderr).strip().splitlines()[0]
    except Exception as exc:  # noqa: BLE001
        return f"(could not run: {exc})"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dest", type=Path, default=DEFAULT_DEST, help="target folder (default: tools/)")
    ap.add_argument("--platform", choices=sorted(TOOLS), default=default_platform())
    ap.add_argument("--only", nargs="+", choices=["ffmpeg", "yt-dlp", "deno"], help="fetch only these tools")
    ap.add_argument("--refresh", action="store_true", help="download again even if cached")
    args = ap.parse_args()

    dest: Path = args.dest.resolve()
    cache = dest / ".cache" / args.platform
    names = args.only or ["ffmpeg", "yt-dlp", "deno"]
    native = args.platform == default_platform()
    versions = []
    for name in names:
        url, kind, member, exe_name = TOOLS[args.platform][name]
        print(f"{name}:")
        out_dir = dest / name
        exe = out_dir / exe_name
        cached = cache / url.rsplit("/", 1)[-1]
        if args.refresh or not cached.exists():
            download(url, cached)
        else:
            print(f"  using cached {cached.name}")
        out_dir.mkdir(parents=True, exist_ok=True)
        if kind == "file":
            shutil.copyfile(cached, exe)
        else:
            extract_member(cached, kind, member, exe)
        make_executable(exe)
        (out_dir / "LICENSE-NOTE.txt").write_text(LICENSE_NOTES[name], encoding="utf-8")
        version = tool_version(exe, name) if native else "(not runnable on this OS)"
        print(f"  -> {exe}\n     {version}")
        versions.append(f"{name}: {version}")
    (dest / "VERSIONS.txt").write_text("\n".join(versions) + "\n", encoding="utf-8")
    print("done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
