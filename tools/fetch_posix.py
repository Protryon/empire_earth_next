"""Fetch what the Linux and macOS builds need into toolchain/ (gitignored), sha256-pinned.

  python -I tools/fetch_posix.py [package ...] [--dl DIR ...]      packages: sysroot wgpu sdl3 fonts (default: all)

  sysroot  the Debian 11 (bullseye) x86_64 sysroot Chromium builds Linux against (glibc 2.31, libstdc++ 10, X11,
           Wayland, Vulkan headers): toolchain/sysroot/debian_bullseye_amd64/. Only for cross-compiling to Linux from
           another system; a Linux machine uses its own headers. Debian packages under their own (free) licenses;
           downloaded, never committed.
  wgpu     wgpu-native v24.0.3.1 (MIT or Apache-2.0) for linux-x86_64, macos-aarch64 and macos-x86_64 (the release the
           Windows build pins, tools/fetch_wgpu.py: PORT_LEARNINGS.md explains why): toolchain/wgpu/<platform>/
           include/webgpu/{webgpu,wgpu}.h and lib/libwgpu_native.a (static).
  sdl3     the SDL 3.4.18 source release (zlib license): toolchain/src/SDL3-3.4.18/ (tools/xbuild.py sdl3 builds it
           as a static library per target).
  fonts    Liberation Fonts 2.1.5 TTF (SIL Open Font License 1.1; metric-compatible with Arial, Times New Roman and
           Courier New): toolchain/fonts/*.ttf and LICENSE.

Downloads are cached in toolchain/dl/ and refused on a sha256 mismatch; --dl DIR (repeatable) names directories that
already hold them (read only). Archives are unpacked with Python's zipfile/tarfile only (no member is executed; links
inside the sysroot become copies where the file system has no symbolic links; members whose names Windows cannot
hold, or that differ only in case from another, are skipped and counted).
"""
import argparse
import hashlib
import os
import pathlib
import shutil
import sys
import tarfile
import tempfile
import urllib.request
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
TC = pathlib.Path(os.environ.get("EE_TOOLCHAIN", ROOT / "toolchain"))

WGPU_RELEASE = "v24.0.3.1"
WGPU = {   # platform: (package, sha256)
    "linux-x86_64": ("wgpu-linux-x86_64-release.zip",
                     "86f3eb9f74d7f1ac82ee52d9b2ab15e366ef86a932759c750b7472652836ee59"),
    "macos-aarch64": ("wgpu-macos-aarch64-release.zip",
                      "f140ff27234ebfa9fcca2b492d0cb499f2e197424b9edc45134bcbad0f8d3a78"),
    "macos-x86_64": ("wgpu-macos-x86_64-release.zip",
                     "1fbc6930e2811b7fde7f046e5300ae5dc20c451d0c3e42a10ff71efae1f565ac"),
}
WGPU_URL = "https://github.com/gfx-rs/wgpu-native/releases/download/{release}/{name}"
SYSROOT = ("debian_bullseye_amd64_sysroot.tar.xz",
           "https://commondatastorage.googleapis.com/chrome-linux-sysroot/"
           "f851add76bdae472fb6c94b4f920bc4a132d85ac607951391297f636af7e934e",
           "f851add76bdae472fb6c94b4f920bc4a132d85ac607951391297f636af7e934e")
SDL = ("SDL3-3.4.18.tar.gz", "https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz",
       "9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3")
FONTS = ("liberation-fonts-ttf-2.1.5.tar.gz",
         "https://github.com/liberationfonts/liberation-fonts/files/7261482/liberation-fonts-ttf-2.1.5.tar.gz",
         "7191c669bf38899f73a2094ed00f7b800553364f90e2637010a69c0e268f25d0")
RESERVED = {"con", "prn", "aux", "nul", *(f"com{i}" for i in range(1, 10)), *(f"lpt{i}" for i in range(1, 10))}


def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def fetch(name, url, pin, extra_dirs):
    for d in extra_dirs:
        p = pathlib.Path(d) / name
        if p.exists() and sha(p) == pin:
            return p
    dst = TC / "dl" / name
    if not dst.exists():
        dst.parent.mkdir(parents=True, exist_ok=True)
        print(f"downloading {url}", flush=True)
        part = dst.with_name(dst.name + ".part")
        urllib.request.urlretrieve(url, part)
        part.replace(dst)
    got = sha(dst)
    if got != pin:
        sys.exit(f"{name}: sha256 {got} != pinned {pin}")
    return dst


def stamp_ok(dst, pin):
    s = dst / ".package"
    return s.exists() and s.read_text().strip() == pin


def safe_name(rel):
    parts = pathlib.PurePosixPath(rel).parts
    if any(p in ("..", "") or p.startswith("/") for p in parts):
        return False
    if os.name == "nt" and any(p.split(".")[0].lower() in RESERVED or p.endswith((" ", ".")) or
                               any(c in p for c in '<>:"|?*') for p in parts):
        return False
    return True


def untar(archive, dst, strip=0):
    """Unpacks a tar archive into dst (links become copies when they cannot be made) -> (files, skipped)."""
    seen = set()
    skipped = 0
    files = 0
    links = []
    with tarfile.open(archive) as tf:
        for m in tf:
            parts = pathlib.PurePosixPath(m.name.lstrip("./")).parts[strip:]
            if not parts:
                continue
            rel = "/".join(parts)
            key = rel.lower()
            if not safe_name(rel) or (key in seen and os.name == "nt"):
                skipped += 1
                continue
            seen.add(key)
            out = dst / rel
            if m.isdir():
                out.mkdir(parents=True, exist_ok=True)
            elif m.isreg():
                out.parent.mkdir(parents=True, exist_ok=True)
                with tf.extractfile(m) as src, open(out, "wb") as f:
                    shutil.copyfileobj(src, f)
                files += 1
            elif m.issym() or m.islnk():
                links.append((m, out, parts))
    # links: a copy of the target (the sysroot's lib*.so -> lib*.so.N chains, header aliases)
    for _ in range(4):
        left = []
        for m, out, parts in links:
            if m.islnk():
                target = dst / "/".join(pathlib.PurePosixPath(m.linkname.lstrip("./")).parts[strip:])
            elif m.linkname.startswith("/"):
                target = dst / m.linkname.lstrip("/")
            else:
                target = (out.parent / m.linkname)
            try:
                target = target.resolve()
            except OSError:
                left.append((m, out, parts))
                continue
            if not target.exists() or not target.is_relative_to(dst.resolve()):
                left.append((m, out, parts))
                continue
            out.parent.mkdir(parents=True, exist_ok=True)
            if target.is_dir():
                if not out.exists():
                    shutil.copytree(target, out)
            else:
                shutil.copyfile(target, out)
            files += 1
        if len(left) == len(links):
            break
        links = left
    return files, skipped + len(links)


def install_sysroot(extra):
    dst = TC / "sysroot" / "debian_bullseye_amd64"
    if stamp_ok(dst, SYSROOT[2]):
        print(f"sysroot: {dst} is current")
        return
    archive = fetch(*SYSROOT, extra)
    if dst.exists():
        shutil.rmtree(dst)
    dst.mkdir(parents=True)
    files, skipped = untar(archive, dst)
    (dst / ".package").write_text(SYSROOT[2] + "\n")
    print(f"sysroot: {files} files -> {dst} ({skipped} skipped: names this file system cannot hold, dangling links)")


def install_wgpu(extra):
    for plat, (name, pin) in WGPU.items():
        dst = TC / "wgpu" / plat
        if stamp_ok(dst, pin):
            print(f"wgpu {plat}: {dst} is current")
            continue
        archive = fetch(f"{WGPU_RELEASE}-{name}", WGPU_URL.format(release=WGPU_RELEASE, name=name), pin, extra)
        if dst.exists():
            shutil.rmtree(dst)
        wanted = ["include/webgpu/webgpu.h", "include/webgpu/wgpu.h", "lib/libwgpu_native.a"]
        with zipfile.ZipFile(archive) as z:
            for w in wanted:
                (dst / w).parent.mkdir(parents=True, exist_ok=True)
                with z.open(w) as src, open(dst / w, "wb") as f:
                    shutil.copyfileobj(src, f)
        (dst / ".package").write_text(pin + "\n")
        print(f"wgpu {plat}: wgpu-native {WGPU_RELEASE} -> {dst}")


def install_sdl(extra):
    dst = TC / "src" / "SDL3-3.4.18"
    if stamp_ok(dst, SDL[2]):
        print(f"sdl3: {dst} is current")
        return
    archive = fetch(*SDL, extra)
    if dst.exists():
        shutil.rmtree(dst)
    dst.mkdir(parents=True)
    files, skipped = untar(archive, dst, strip=1)
    (dst / ".package").write_text(SDL[2] + "\n")
    print(f"sdl3: {files} files -> {dst} ({skipped} skipped)")


def install_fonts(extra):
    dst = TC / "fonts"
    if stamp_ok(dst, FONTS[2]):
        print(f"fonts: {dst} is current")
        return
    archive = fetch(*FONTS, extra)
    if dst.exists():
        shutil.rmtree(dst)
    with tempfile.TemporaryDirectory() as tmp:
        untar(archive, pathlib.Path(tmp), strip=1)
        dst.mkdir(parents=True)
        for p in sorted(pathlib.Path(tmp).iterdir()):
            if p.suffix.lower() == ".ttf" or p.name in ("LICENSE", "AUTHORS"):
                shutil.copyfile(p, dst / p.name)
    (dst / ".package").write_text(FONTS[2] + "\n")
    print(f"fonts: Liberation 2.1.5 -> {dst}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("packages", nargs="*", help="sysroot wgpu sdl3 fonts (default: all)")
    ap.add_argument("--dl", action="append", default=[], help="directory holding the downloads already (read only)")
    a = ap.parse_args()
    todo = a.packages or ["sysroot", "wgpu", "sdl3", "fonts"]
    bad = [p for p in todo if p not in ("sysroot", "wgpu", "sdl3", "fonts")]
    if bad:
        sys.exit(f"unknown package(s): {' '.join(bad)}")
    for p in todo:
        {"sysroot": install_sysroot, "wgpu": install_wgpu, "sdl3": install_sdl, "fonts": install_fonts}[p](a.dl)


if __name__ == "__main__":
    main()
