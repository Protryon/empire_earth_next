"""Fetch wgpu-native (the static wgpu library and its C headers) into toolchain/wgpu/x86_64/ (gitignored).

  python -I tools/fetch_wgpu.py [--dl DIR ...]

wgpu-native (gfx-rs, MIT or Apache-2.0), the windows-x86_64-msvc-release package: include/webgpu/webgpu.h and
wgpu.h, lib/wgpu_native.lib (static). Pinned at v24.0.3.1: with v29.0.0.0 the game's buffer read-backs never
complete on DX12/WARP (the GX self-test passes; the game's workload does not). The download is cached in
toolchain/dl/ and sha256-pinned; refuses on a mismatch. --dl DIR (repeatable) names directories already holding the
download (read, never written). Needs 7-Zip (`7z` on PATH, C:\\Program Files\\7-Zip\\7z.exe, or env EE_7Z); nothing
from the package is executed.
"""
import argparse
import hashlib
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
TC = pathlib.Path(os.environ.get("EE_TOOLCHAIN", ROOT / "toolchain"))
PACKAGES = {   # arch: (release, package, sha256); the cache file name carries the release
    "x86_64": ("v24.0.3.1", "wgpu-windows-x86_64-msvc-release.zip",
               "72d3abf74e0b55f9474602e2ce7f20fd0f9b9c46be8405b45a697ec1ee343436"),
}
URL = "https://github.com/gfx-rs/wgpu-native/releases/download/{release}/{name}"
WANTED = ["include/webgpu/webgpu.h", "include/webgpu/wgpu.h", "lib/wgpu_native.lib"]


def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def fetch(release, name, pin, extra_dirs):
    for d in extra_dirs:
        for p in (pathlib.Path(d) / f"{release}-{name}", pathlib.Path(d) / name):
            if p.exists() and sha(p) == pin:
                return p
    dst = TC / "dl" / f"{release}-{name}"
    if not dst.exists():
        dst.parent.mkdir(parents=True, exist_ok=True)
        url = URL.format(release=release, name=name)
        print(f"downloading {url}", flush=True)
        urllib.request.urlretrieve(url, dst.with_suffix(".part"))
        dst.with_suffix(".part").replace(dst)
    if sha(dst) != pin:
        sys.exit(f"{name}: sha256 {sha(dst)} != pinned {pin}")
    return dst


def sevenzip():
    for c in (os.environ.get("EE_7Z"), shutil.which("7z"), r"C:\Program Files\7-Zip\7z.exe"):
        if c and pathlib.Path(c).exists():
            return pathlib.Path(c)
    sys.exit("7-Zip not found: install it (winget install 7zip.7zip) or set EE_7Z")


def install(arch, extra_dirs):
    release, name, pin = PACKAGES[arch]
    dst = TC / "wgpu" / arch
    stamp = dst / ".package"
    if stamp.exists() and stamp.read_text().strip() == pin and all((dst / w).exists() for w in WANTED):
        print(f"{arch}: {dst} is current")
        return dst
    archive = fetch(release, name, pin, extra_dirs)
    with tempfile.TemporaryDirectory() as tmp:
        r = subprocess.run([str(sevenzip()), "x", "-y", f"-o{tmp}", str(archive)], capture_output=True, text=True)
        if r.returncode:
            sys.exit(f"7z {archive}: {r.stdout}{r.stderr}")
        if dst.exists():
            shutil.rmtree(dst)
        for w in WANTED:
            src = pathlib.Path(tmp) / w
            if not src.exists():
                sys.exit(f"{name}: {w} missing")
            (dst / w).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst / w)
    stamp.write_text(pin + "\n")
    print(f"{arch}: wgpu-native {release} -> {dst}")
    return dst


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--dl", action="append", default=[], help="directory holding the download already (read only)")
    a = ap.parse_args()
    install("x86_64", a.dl)


if __name__ == "__main__":
    main()
