"""Build by-name import libraries for a module from its import table (link/<module>/imports.txt).

  python -I tools/implib.py <module> <out_dir> <obj>...
"""
import collections
import os
import pathlib
import re
import struct
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import verify
from undname import undname
from ee import COMPILER

C_DATA = {"_adjust_fdiv"}
IMPLIB_PKG = {"sxlrt233": "msvc6.3"}


def object_symbols(objs, pkg):
    refs, defs = set(), set()
    for o in objs:
        out = subprocess.run([str(pkg / "Bin" / "DUMPBIN.EXE"), "/symbols", str(o)], capture_output=True,
                             text=True, encoding="latin-1").stdout
        for line in out.splitlines():
            m = re.search(r"(UNDEF|SECT\w+) .*External\s+\| (\S+)", line)
            if not m:
                continue
            s = m.group(2)
            if m.group(1) == "UNDEF":
                refs.add(s[len("__imp_"):] if s.startswith("__imp_") else s)
            else:
                defs.add(s)
    return refs, defs


def plain(sym):
    if sym.startswith("?"):
        return sym
    s = sym[1:] if sym.startswith("_") else sym
    return re.sub(r"@\d+$", "", s)


def is_decorated_import(name):
    return re.match(r"_[^@?]+@\d+$", name) is not None


def is_cpp_data(name):
    if re.match(r"\?[^?@][^@]*@(?:[^@?]+@)*@[0-4]", name):
        return True
    text = undname(name).rstrip()
    return not re.search(r"\)\s*(?:const)?\s*(?:volatile)?$", text)


Import = collections.namedtuple("Import", "name hint ordinal import_by_ordinal")


def read_imports(module):
    path = verify.LINK_DIR / module / "imports.txt"
    if not path.exists():
        sys.exit(f"{path} missing: python -I tools/gen_link_manifests.py {module}")
    out = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            out.append((line[1:-1], []))
            continue
        name, tail = line.split("\t")
        if tail.startswith("@"):
            out[-1][1].append(Import(name, None, int(tail[1:]), True))
        else:
            out[-1][1].append(Import(name, int(tail), None, False))
    return out


def main():
    module, out = sys.argv[1], pathlib.Path(sys.argv[2])
    objs = [pathlib.Path(p) for p in sys.argv[3:]]
    out.mkdir(parents=True, exist_ok=True)
    pkg = verify.TOOLCHAIN / COMPILER
    by_plain = {}
    refs, defs = object_symbols(objs, pkg)
    for s in refs:
        by_plain.setdefault(plain(s), s)
    for dll, imports in read_imports(module):
        lines = [f'LIBRARY "{dll}"', "EXPORTS"]
        hints, as_is = {}, set()
        for imp in imports:
            ordinal = f" @{imp.ordinal} NONAME" if imp.import_by_ordinal else ""
            name = imp.name
            if is_decorated_import(name):
                sym = name
                as_is.add(sym.encode())
            else:
                sym = by_plain.get(name, name if name.startswith("?") else "_" + name)
            entry = sym if sym.startswith("?") else sym[1:]
            data = name in C_DATA or sym in defs or (name.startswith("?") and is_cpp_data(name))
            lines.append(f"    {entry}{ordinal}{' DATA' if data else ''}")
            if not imp.import_by_ordinal:
                hints[sym.encode()] = imp.hint or 0
        stem = pathlib.Path(dll).stem
        (out / f"{stem}.def").write_text("\n".join(lines) + "\n")
        lpkg = verify.TOOLCHAIN / IMPLIB_PKG.get(stem.lower(), COMPILER)
        r = subprocess.run([str(lpkg / "Bin" / "LIB.EXE"), "/nologo", "/MACHINE:IX86", f"/DEF:{stem}.def",
                            f"/OUT:{stem}.lib"], cwd=out, capture_output=True, text=True,
                           env=dict(os.environ, PATH=str(lpkg / "Bin") + os.pathsep + os.environ["PATH"]),
                           creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        msg = (r.stdout + r.stderr).strip()
        print(f"{stem}.lib: {len(imports)} imports" + (f"\n{msg}" if msg else ""))
        if r.returncode:
            sys.exit(r.returncode)
        fix_members(out / f"{stem}.lib", hints, as_is)


def fix_members(lib, hints, as_is=()):
    b = bytearray(lib.read_bytes())
    i = 0
    while (i := b.find(b"\x00\x00\xff\xff", i)) >= 0:
        _, _, _, machine, _, _, _, typ = struct.unpack_from("<HHHHIIHH", b, i)
        if machine == 0x14C:
            name = bytes(b[i + 20:b.index(b"\0", i + 20)])
            by_ordinal = (typ >> 2) & 7 == 0
            if not by_ordinal and name in as_is:
                struct.pack_into("<H", b, i + 18, (typ & ~0x1C) | (1 << 2))
            elif not by_ordinal and not name.startswith(b"?") and re.search(rb"@\d+$", name):
                struct.pack_into("<H", b, i + 18, (typ & ~0x1C) | (3 << 2))
            if name in hints:
                struct.pack_into("<H", b, i + 16, hints[name])
        i += 4
    lib.write_bytes(bytes(b))


if __name__ == "__main__":
    main()
