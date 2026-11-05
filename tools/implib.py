"""Import libraries for a module's imports: by name from its original import table (link/<module>/imports.txt).

  python -I tools/implib.py <module> <out_dir> <obj>...      (also make_import_libs(module, out_dir, objs))

System DLLs are linked against the Windows SDK's import libraries (ee.SDK_IMPORT_LIBS) and the Dinkum C++ DLL is not
linked (ee.DROPPED_IMPORTS). For every other DLL (DirectInput 7, Miles, Bink, the engine) a .def is written from the
import table and turned into an import library by llvm-lib: C names are matched to the objects' references for
their decoration, C++ data is marked DATA, and the import-name type and hint of each member are set as the table
records them (fix_members). x64 C names carry no decoration, so a decorated name of the table (name@n, an x86
stdcall export of the original) is listed by its plain name.
"""
import collections
import os
import pathlib
import re
import struct
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import ee
import verify
from undname import undname
from ee import DROPPED_IMPORTS, LLVM_LIB, SDK_IMPORT_LIBS

C_DATA = {"_adjust_fdiv"}


def object_symbols(objs):
    """(undefined external names without __imp_, defined external names) of COFF objects."""
    refs, defs = set(), set()
    for o in objs:
        for s in verify.Coff(o).syms.values():
            if s["cls"] != 2:
                continue
            n = s["name"]
            if s["sec"] == 0 and s["value"] == 0:
                refs.add(n[len("__imp_"):] if n.startswith("__imp_") else n)
            elif s["sec"] > 0:
                defs.add(n)
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
    text = re.sub(r"(\s*__ptr64)+$", "", undname(name).rstrip())   # x64: the this pointer's qualifier
    return not re.search(r"\)\s*(?:const)?\s*(?:volatile)?$", text)


Import = collections.namedtuple("Import", "name hint ordinal import_by_ordinal")


def read_imports(module):
    path = verify.LINK_DIR / module / "imports.txt"
    if not path.exists():
        sys.exit(f"{path} missing")
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


def make_import_libs(module, out, objs):
    """-> [library argument per imported DLL, in import-table order] (SDK library names, or generated paths)."""
    out = pathlib.Path(out)
    out.mkdir(parents=True, exist_ok=True)
    refs, defs = object_symbols(objs)
    libs = []
    for dll, imports in read_imports(module):
        stem = pathlib.Path(dll).stem
        if stem.upper() in DROPPED_IMPORTS:
            continue
        if stem.upper() in SDK_IMPORT_LIBS:
            libs.append(SDK_IMPORT_LIBS[stem.upper()])
            continue
        lines = [f'LIBRARY "{dll}"', "EXPORTS"]
        hints = {}
        for imp in imports:
            ordinal = f" @{imp.ordinal} NONAME" if imp.import_by_ordinal else ""
            name = imp.name
            sym = entry = plain(name) if is_decorated_import(name) else name
            data = name in C_DATA or sym in defs or (name.startswith("?") and is_cpp_data(name))
            lines.append(f"    {entry}{ordinal}{' DATA' if data else ''}")
            if not imp.import_by_ordinal:
                hints[sym.encode()] = imp.hint or 0
        (out / f"{stem}.def").write_text("\n".join(lines) + "\n")
        r = subprocess.run([str(LLVM_LIB), "/nologo", f"/machine:{ee.ARCH}", f"/def:{stem}.def", f"/out:{stem}.lib"],
                           cwd=out, capture_output=True, text=True,
                           creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        if r.returncode:
            sys.exit(f"llvm-lib {stem}.def: {r.stdout}{r.stderr}")
        fix_members(out / f"{stem}.lib", hints)
        libs.append(str(out / f"{stem}.lib"))
    return libs


def fix_members(lib, hints):
    """Short import members: the hint the original import table records (x64 names carry no decoration)."""
    b = bytearray(lib.read_bytes())
    i = 0
    while (i := b.find(b"\x00\x00\xff\xff", i)) >= 0:
        if struct.unpack_from("<H", b, i + 6)[0] == 0x8664:
            name = bytes(b[i + 20:b.index(b"\0", i + 20)])
            if name in hints:
                struct.pack_into("<H", b, i + 16, hints[name])
        i += 4
    lib.write_bytes(bytes(b))
