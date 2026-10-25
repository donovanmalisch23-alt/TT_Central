#!/usr/bin/env python3
"""Assemble TT Central for Windows: a portable folder and zip, ready for the
Inno Setup installer (packaging/windows/ttcentral.iss).

Run after a Release build (and scripts/fetch_tools.py):

    python scripts/package.py                    # -> dist/TTCentral-<ver>-win64/
    python scripts/package.py --no-zip --no-tools

The folder contains the TT Central client, the media bot, BearWare's TeamTalk
server and SDK runtime, the Qt and MSVC runtimes, the TeamTalk sound packs and
translations, ffmpeg / yt-dlp / Deno, and every license. The zip adds an empty
TTCentral.ini, which makes TT Central keep its settings next to it (portable
mode); the installer leaves it out, so settings go to %APPDATA%\\TTCentral.

Packages contain the proprietary TeamTalk SDK (trial mode without a license
key): build them for your own use, and get a license from BearWare.dk before
distributing them.
"""
from __future__ import annotations

import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
UPSTREAM = REPO / "external" / "TeamTalk5"
CLIENT_DIR = UPSTREAM / "Client" / "qtTeamTalk"
SDK_DIR = REPO / "third_party" / "teamtalk-sdk"
TOOLS_DIR = REPO / "tools"

CRT_DLLS = ["msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll", "vcruntime140.dll", "vcruntime140_1.dll",
            "concrt140.dll"]


def project_version() -> str:
    text = (REPO / "CMakeLists.txt").read_text(encoding="utf-8")
    m = re.search(r"project\(\s*TTCentral\s+VERSION\s+([0-9.]+)", text)
    return m.group(1) if m else "0.0.0"


def copy(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def copy_tree(src: Path, dst: Path, exclude: tuple[str, ...] = ()) -> None:
    for p in src.rglob("*"):
        if p.is_file() and p.name not in exclude:
            copy(p, dst / p.relative_to(src))


def find_crt() -> list[Path]:
    """The MSVC runtime DLLs from Visual Studio's redist folder (app-local)."""
    roots = []
    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / \
        "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if vswhere.exists():
        out = subprocess.run([str(vswhere), "-latest", "-products", "*", "-property", "installationPath"],
                             capture_output=True, text=True).stdout.strip()
        if out:
            roots.append(Path(out))
    if os.environ.get("VCToolsRedistDir"):
        roots.append(Path(os.environ["VCToolsRedistDir"]))
    for root in roots:
        hits = sorted(glob.glob(str(root / "**" / "x64" / "Microsoft.VC*.CRT" / "vcruntime140.dll"), recursive=True))
        hits = [h for h in hits if "onecore" not in h.lower() and "debug" not in h.lower()]
        if hits:
            crt = Path(hits[-1]).parent
            return [crt / n for n in CRT_DLLS if (crt / n).exists()]
    return []


def require(path: Path, what: str) -> Path:
    if not path.exists():
        sys.exit(f"error: {what} not found: {path}")
    return path


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=Path, default=REPO / "dist")
    ap.add_argument("--version", default=project_version())
    ap.add_argument("--no-zip", action="store_true", help="only assemble the folder")
    ap.add_argument("--no-tools", action="store_true", help="leave out ffmpeg, yt-dlp and Deno")
    args = ap.parse_args()

    exe = ".exe" if os.name == "nt" else ""
    name = f"TTCentral-{args.version}-win64"
    out: Path = args.out.resolve() / name
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    print(f"Assembling {out}")

    # Programs and the TeamTalk SDK runtime.
    copy(require(CLIENT_DIR / f"TTCentral{exe}", "the TT Central client (build it first)"), out / f"TTCentral{exe}")
    copy(require(CLIENT_DIR / f"ttc-mediabot{exe}", "the media bot"), out / f"ttc-mediabot{exe}")
    dll = "TeamTalk5.dll" if os.name == "nt" else "libTeamTalk5.so"
    copy(require(UPSTREAM / "Library" / "TeamTalk_DLL" / dll, "the TeamTalk SDK runtime"), out / dll)
    server = SDK_DIR / "server"
    for srv in ("tt5srv", "tt5svc"):
        if (server / f"{srv}{exe}").exists():
            copy(server / f"{srv}{exe}", out / "server" / f"{srv}{exe}")
    require(out / "server" / f"tt5srv{exe}", "BearWare's TeamTalk server (run scripts/bootstrap.py)")

    # Qt runtime: what windeployqt gathered for the client during the build,
    # plus anything the media bot needs (Qt Core/Network are already there).
    deploy = CLIENT_DIR / "windeployqt"
    if deploy.is_dir():
        copy_tree(deploy, out, exclude=("vc_redist.x64.exe",))
    elif os.name == "nt":
        sys.exit("error: the client's windeployqt folder is missing (build in Release with windeployqt on PATH)")

    # MSVC runtime, app-local, so nothing needs installing.
    if os.name == "nt":
        crt = find_crt()
        if not crt:
            sys.exit("error: the Visual C++ runtime DLLs were not found (is Visual Studio installed?)")
        for d in crt:
            copy(d, out / d.name)

    # Translations, sound packs and the client's default settings.
    for qm in sorted((CLIENT_DIR / "languages").glob("*.qm")):
        copy(qm, out / "languages" / qm.name)
    sounds = UPSTREAM / "Setup" / "Client" / "Sounds"
    copy_tree(require(sounds, "the TeamTalk sound packs"), out / "Sounds")
    copy(UPSTREAM / "Setup" / "Client" / "Windows" / "TeamTalk5.ini", out / "TTCentral.ini.default")

    # Media tools.
    if not args.no_tools:
        for tool in ("ffmpeg", "yt-dlp", "deno"):
            src = TOOLS_DIR / tool
            if not (src / f"{tool}{exe}").exists():
                sys.exit(f"error: {tool} is missing; run: python scripts/fetch_tools.py")
            copy_tree(src, out / "tools" / tool)

    # Licenses and documentation.
    copy(REPO / "LICENSE", out / "LICENSE.txt")
    copy(REPO / "NOTICE", out / "NOTICE.txt")
    copy(REPO / "THIRD-PARTY-LICENSES.md", out / "THIRD-PARTY-LICENSES.md")
    if (SDK_DIR / "License.txt").exists():
        copy(SDK_DIR / "License.txt", out / "TeamTalk-SDK-License.txt")
    guide = REPO / "docs" / "USER-GUIDE.md"
    if guide.exists():
        copy(guide, out / "User Guide.md")
        try:
            import markdown  # optional: an HTML copy reads best in a browser and with screen readers
            html = markdown.markdown(guide.read_text(encoding="utf-8"), extensions=["toc", "tables"])
            (out / "User Guide.html").write_text(
                "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
                "<title>TT Central User Guide</title></head><body>" + html + "</body></html>", encoding="utf-8")
        except ImportError:
            print("  (python-markdown not installed; shipping the guide as Markdown only)")
    (out / "VERSION.txt").write_text(f"TT Central {args.version}\n", encoding="utf-8")

    files = sum(1 for p in out.rglob("*") if p.is_file())
    size = sum(p.stat().st_size for p in out.rglob("*") if p.is_file())
    print(f"  {files} files, {size / 1048576:.1f} MB")

    if not args.no_zip:
        zpath = args.out.resolve() / f"{name}-portable.zip"
        with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for p in sorted(out.rglob("*")):
                if p.is_file():
                    z.write(p, Path(name) / p.relative_to(out))
            # Portable mode: settings live next to TTCentral.exe.
            z.writestr(f"{name}/TTCentral.ini", "")
        print(f"  {zpath} ({zpath.stat().st_size / 1048576:.1f} MB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
