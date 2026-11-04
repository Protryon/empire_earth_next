"""Import library and COFF helpers for the executable's link."""
import os
import pathlib
import re
import struct
import subprocess
import sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))


def short_imports(path):
    b = pathlib.Path(path).read_bytes()
    out, i = {}, 0
    while (i := b.find(b"\x00\x00\xff\xff", i)) >= 0:
        _, _, ver, machine, _, _, hint, typ = struct.unpack_from("<HHHHIIHH", b, i)
        at, i = i, i + 4
        if machine != 0x14C or ver != 0:
            continue
        e = b.index(b"\0", at + 20)
        sym = b[at + 20:e].decode()
        dll = b[e + 1:b.index(b"\0", e + 1)].decode().lower()
        nt = (typ >> 2) & 7
        bare = sym[1:] if sym[:1] in "?@_" and nt in (2, 3) else sym
        name = f"#{hint}" if nt == 0 else re.sub(r"@.*$", "", bare) if nt == 3 else bare
        out.setdefault((dll, name), (sym, hint))
    return out


def write_order_object(path, symbols):
    syms, strtab = [], bytearray(4)
    for name in symbols:
        if len(name) <= 8:
            raw = name.encode().ljust(8, b"\0")
        else:
            raw = struct.pack("<II", 0, len(strtab))
            strtab.extend(name.encode() + b"\0")
        syms.append(raw + struct.pack("<IhHBB", 0, 0, 0, 2, 0))
    strtab[0:4] = struct.pack("<I", len(strtab))
    hdr = struct.pack("<HHIIIHH", 0x14C, 0, 0, 20, len(syms), 0, 0)
    pathlib.Path(path).write_bytes(hdr + b"".join(syms) + bytes(strtab))


def run(cmd, cwd, pkg=None):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    return r.returncode, (r.stdout + r.stderr).strip()
