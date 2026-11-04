"""Fetch the DirectX 7.0a SDK headers the renderers compile against into toolchain/dx7sdk/include (gitignored).

  python -I tools/fetch_dx7sdk.py [--dl DIR ...]

The Windows SDK no longer has the DirectX 7 Direct3D headers (d3d.h, d3dtypes.h, ...). The download comes from
archive.org, is cached in toolchain/dl/ and is sha256-pinned, and so is the extracted include directory; refuses on a
mismatch. --dl DIR (repeatable) names directories already holding the download (read, never written). Needs 7-Zip:
`7z` on PATH, C:\\Program Files\\7-Zip\\7z.exe, or env EE_7Z. Extraction goes to a fresh temporary directory and
nothing from it is executed.
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
DOWNLOAD = ("dx7adxf.exe", "https://archive.org/download/dx7adxf/dx7adxf.exe",
            "c0b2bb400322a12e66d0c724825be7146583315fc08e54f3a287cfb30c363540")
DX7_INCLUDE_DIGEST = "73a7a4ae12eb155816fd428c9c0add6288e66e30d13f8d4ca4ccb60ae2999f6d"


def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def dir_digest(d):
    h = hashlib.sha256()
    for p in sorted(pathlib.Path(d).iterdir(), key=lambda p: p.name.lower()):
        h.update(p.name.lower().encode() + b"\0" + bytes.fromhex(sha(p)))
    return h.hexdigest()


def fetch(extra_dirs):
    name, url, pin = DOWNLOAD
    for d in extra_dirs:
        p = pathlib.Path(d) / name
        if p.exists() and sha(p) == pin:
            return p
    dst = TC / "dl" / name
    if not dst.exists():
        dst.parent.mkdir(parents=True, exist_ok=True)
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


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--dl", action="append", default=[], help="directory holding the download already (read only)")
    a = ap.parse_args()
    inc = TC / "dx7sdk" / "include"
    if inc.exists() and dir_digest(inc) == DX7_INCLUDE_DIGEST:
        print("dx7sdk headers: ok")
        return
    with tempfile.TemporaryDirectory() as td:
        subprocess.run([str(sevenzip()), "x", "-y", f"-o{td}", str(fetch(a.dl)), "DXF/include/*"], check=True,
                       capture_output=True)
        got = pathlib.Path(td) / "DXF" / "include"
        if dir_digest(got) != DX7_INCLUDE_DIGEST:
            sys.exit("dx7adxf.exe DXF/include: digest mismatch")
        if inc.exists():
            shutil.rmtree(inc)
        shutil.copytree(got, inc)
    print("dx7sdk headers: ok")


if __name__ == "__main__":
    main()
