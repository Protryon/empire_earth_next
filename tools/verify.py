"""Compile helpers: stage a unit under its build path (C:\\EEX, as the original build tree), its include list, its
// UNIT-FLAGS, and a COFF object reader."""
import os
import pathlib
import re
import struct
import subprocess
import sys
import time
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from ee import LINK_DIR, EEX_INCLUDE_DIRS, ORIG_SRC_ROOT, ROOT, TOOLCHAIN, modern_dirs
DEFINES = "-DWIN32 -DNDEBUG -D_WINDOWS"


THIRD_PARTY = ROOT / "src" / "ThirdParty"


def stage_source(src):
    """The path a unit compiles from: the game's units are copied under C:\\EEX (with their sibling headers);
    third-party libraries compile in place (their own relative includes)."""
    src = pathlib.Path(src).resolve()
    if src.is_relative_to(THIRD_PARTY):
        return src
    try:
        rel = src.relative_to(ROOT / "src")
    except ValueError:
        return src
    staged = ORIG_SRC_ROOT / rel
    staged.parent.mkdir(parents=True, exist_ok=True)
    for f in [src, *sorted(src.parent.glob("*.h"))]:
        _stage_write(staged.parent / f.name, f.read_bytes())
    return staged


def _stage_write(dst, data):
    for i in range(120):
        try:
            if not dst.exists() or dst.read_bytes() != data:
                dst.write_bytes(data)
            return
        except PermissionError:
            time.sleep(0.25)
    if not dst.exists() or dst.read_bytes() != data:
        dst.write_bytes(data)


def include_paths(src=None):
    flags = []
    for d in EEX_INCLUDE_DIRS:
        dst = ORIG_SRC_ROOT / d
        dst.mkdir(parents=True, exist_ok=True)
        for f in sorted((ROOT / "src" / d).glob("*.h")):
            _stage_write(dst / f.name, f.read_bytes())
        flags.append(f"/I{dst}")   # absolute: a drive-relative \eex\... breaks when the build runs from another drive
    msvc, sdk, _, _ = modern_dirs()
    incdir = pathlib.Path(os.environ.get("EE_INCLUDE", str(ROOT / "include")))
    dx7 = pathlib.Path(os.environ.get("EE_DX7SDK", str(TOOLCHAIN / "dx7sdk" / "include")))
    use_dx7 = src is not None and "dx7sdk" in unit_includes(src) and dx7.exists()
    # the renderer's GX layer: wgpu-native's C headers (rust/vendor/wgpu-native; the headers must match the library that
    # is linked)
    import ee
    wgpu = [str(ee.wgpu_dir() / "include")] if src is not None and "DX7 Display Plug-ins" in str(src) \
        else []
    # the built-in fonts (#embed in src/Platform/Common/PlatFontData.c; tools/fetch_posix.py fonts)
    if src is not None and "Platform" in pathlib.Path(src).parts and "Common" in pathlib.Path(src).parts:
        flags.append(f"/clang:--embed-dir={TOOLCHAIN / 'fonts'}")
    # the window layer's SDL3 headers (tools/xbuild.py sdl3), for its implementation only
    if src is not None and "Platform" in pathlib.Path(src).parts and "SDL" in pathlib.Path(src).parts:
        wgpu.append(str(ee.sdl3_dir() / "include"))
    # a third-party library's own include directories (src/ThirdParty/<lib>/include_dirs.txt, relative to the library)
    own = []
    if src is not None and pathlib.Path(src).resolve().is_relative_to(THIRD_PARTY):
        lib = THIRD_PARTY / pathlib.Path(src).resolve().relative_to(THIRD_PARTY).parts[0]
        if (lib / "include_dirs.txt").exists():
            own = [str(lib / d.strip()) for d in (lib / "include_dirs.txt").read_text().splitlines() if d.strip()]
    # public headers of vendored libraries that live with their sources
    public = [str(THIRD_PARTY / "dmusic" / "include")]
    # include/winsdk last: headers the Windows SDK dropped (the 2001 iphlpapi.h)
    inc = os.pathsep.join([*own, str(incdir), *public, *wgpu, *([str(dx7)] if use_dx7 else []),
                           str(msvc / "include"), str(sdk / "ucrt"), str(sdk / "um"), str(sdk / "shared"),
                           str(ROOT / "include" / "winsdk")])
    return flags, inc


UNIT_FLAGS = re.compile(r"^// UNIT-FLAGS: (.+?)\s*$", re.M)
UNIT_INCLUDE = re.compile(r"^// UNIT-INCLUDE: (.+?)\s*$", re.M)
UNIT_INCLUDE_KNOWN = {"dx7sdk"}


def unit_includes(src):
    try:
        head = "".join(pathlib.Path(src).read_text(errors="replace").splitlines(keepends=True)[:30])
    except OSError:
        return set()
    names = {n for m in UNIT_INCLUDE.finditer(head) for n in m.group(1).split()}
    unknown = names - UNIT_INCLUDE_KNOWN
    if unknown:
        raise SystemExit(f"{src}: unknown // UNIT-INCLUDE: {' '.join(sorted(unknown))}")
    return names


def unit_flags(src, flags):
    """The module flags plus the defines (and an exception-handling switch, -EHsc) of a unit's `// UNIT-FLAGS:`
    line. The rest of such a line was the VC6 build's per-unit code generation (optimisation, runtime): the build
    decides those for every unit now."""
    try:
        head = "".join(pathlib.Path(src).read_text(errors="replace").splitlines(keepends=True)[:30])
    except OSError:
        return flags
    m = UNIT_FLAGS.search(head)
    if not m:
        return flags
    have = flags.split()
    return " ".join(have + [f for f in m.group(1).split() if f.startswith(("-D", "/D", "-EH", "/EH")) and f not in have])


class Coff:
    def __init__(self, path):
        b = self.b = pathlib.Path(path).read_bytes()
        nsec, _, symptr, nsym, opt, _ = struct.unpack_from("<HIIIHH", b, 2)
        self.secs = []
        for i in range(nsec):
            o = 20 + opt + 40 * i
            name = b[o:o + 8].rstrip(b"\0").decode(errors="replace")
            size, rawptr, relptr, _, nrel, _, chars = struct.unpack_from("<IIIIHHI", b, o + 16)
            self.secs.append({"name": name, "size": size, "raw": rawptr, "relptr": relptr, "nrel": nrel,
                              "chars": chars})
        strtab = symptr + 18 * nsym
        self.syms = {}
        i = 0
        while i < nsym:
            o = symptr + 18 * i
            raw = b[o:o + 8]
            name = (raw.rstrip(b"\0") if raw[:4] != b"\0\0\0\0" else
                    b[strtab + struct.unpack_from("<I", raw, 4)[0]:].split(b"\0", 1)[0]).decode(errors="replace")
            value, secno, typ, cls, naux = struct.unpack_from("<IhHBB", b, o + 8)
            self.syms[i] = {"name": name, "value": value, "sec": secno, "type": typ, "cls": cls}
            i += 1 + naux

    def functions(self):
        return [s for s in self.syms.values() if s["sec"] > 0 and s["type"] == 0x20 and s["cls"] in (2, 3)]

    def section_data(self, secno):
        s = self.secs[secno - 1]
        return self.b[s["raw"]:s["raw"] + s["size"]] if s["raw"] else bytes(s["size"])

    def relocs(self, secno):
        s = self.secs[secno - 1]
        out = []
        for k in range(s["nrel"]):
            va, si, typ = struct.unpack_from("<IIH", self.b, s["relptr"] + 10 * k)
            out.append((va, self.syms[si], typ))
        return out

    def extract(self, sym):
        secno, start = sym["sec"], sym["value"]
        data = self.section_data(secno)
        later = [s["value"] for s in self.functions() if s["sec"] == secno and s["value"] > start]
        end = min(later) if later else len(data)
        rel = [(va - start, t, typ) for va, t, typ in self.relocs(secno) if start <= va < end]
        last_rel = max((va + 4 for va, _, _ in rel), default=0)
        trimmed = end
        while trimmed - start > last_rel and end - trimmed < 15 and data[trimmed - 1] in (0x90, 0xCC):
            trimmed -= 1
        if (later or end == len(data)) and trimmed != end and end % 16 == 0:
            end = trimmed
        return data[start:end], start, rel
