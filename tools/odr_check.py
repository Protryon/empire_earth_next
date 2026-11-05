"""Find inline functions whose copies differ between the objects of one link (ODR divergence).

  python -I tools/odr_check.py [--out DIR] [--all]

Every module is linked into one exe, and the linker keeps one copy of each inline function (a COMDAT) for the whole
program. Copies compiled from the same definition with the same code-generation flags are identical; a COMDAT
whose code or relocation targets differ between objects was compiled from different definitions, typically the
per-module header views of the reconstruction (LLE_EXE_HEADERS, LLE_CURRENT_HEADERS, LOWLEVELENGINE_EXPORTS). The
linker's pick then decides which view every module gets. Reads the objects of a build (default build/clang/obj);
prints each divergent symbol with the modules of each variant. --all lists variants that differ only in byte
values that are not relocations (otherwise those count too: they are real code differences).
"""
import argparse
import collections
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from ee import ROOT
from undname import undname
from verify import Coff

IMAGE_SCN_LNK_COMDAT = 0x1000


def comdat_functions(path):
    """{symbol: (bytes with relocated fields zeroed, (relocation offset, target) tuple)} of the object's COMDAT
    function sections."""
    c = Coff(path)
    out = {}
    for s in c.syms.values():
        if s["sec"] <= 0 or s["type"] != 0x20 or s["cls"] != 2:
            continue
        sec = c.secs[s["sec"] - 1]
        if not sec["chars"] & IMAGE_SCN_LNK_COMDAT or s["value"] != 0:
            continue
        data = bytearray(c.section_data(s["sec"]))
        rel = []
        for va, target, typ in c.relocs(s["sec"]):
            name = target["name"]
            if name.startswith(("$", ".")) or target["cls"] == 3:   # local labels / static symbols: position only
                name = f"<local {typ}>"
            rel.append((va, name))
            data[va:va + 4] = b"\0\0\0\0"
        out[s["name"]] = (bytes(data), tuple(rel))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default=str(ROOT / "build" / "clang"))
    a = ap.parse_args()
    objdir = (pathlib.Path(a.out) / "obj").resolve()
    rsp = pathlib.Path(a.out) / "exe" / "link.rsp"   # the objects the exe really links (obj/ keeps stale ones)
    linked = [pathlib.Path(ln.strip().strip('"')) for ln in rsp.read_text().splitlines() if ln.strip()]
    linked = [p if p.is_absolute() else rsp.parent / p for p in linked]
    variants = collections.defaultdict(lambda: collections.defaultdict(list))
    for obj in sorted(p for p in linked if p.suffix.lower() == ".obj" and p.is_relative_to(objdir)):
        mod = obj.relative_to(objdir).parts[0]
        for sym, body in comdat_functions(obj).items():
            variants[sym][body].append(f"{mod}:{obj.stem}")
    bad = {s: v for s, v in variants.items() if len(v) > 1}
    for sym, v in sorted(bad.items(), key=lambda kv: kv[0]):
        mods = [sorted({u.split(":")[0] for u in units}) for units in v.values()]
        print(f"{undname(sym).strip()[:150]}")
        for (data, rel), units in v.items():
            print(f"    {len(data):5} bytes {len(rel):3} relocs  {len(units):4} objects  modules {','.join(sorted({u.split(':')[0] for u in units}))}  e.g. {units[0]}")
    print(f"\n{len(variants)} inline functions in more than zero objects; {len(bad)} with divergent copies")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
