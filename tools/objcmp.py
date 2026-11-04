"""Compare two trees of COFF objects (e.g. VC6 builds before and after a source edit) by code and data contents.

  python -I tools/objcmp.py <dir A> <dir B> [-v]

For every object present in both trees, compares the raw bytes and relocation targets (by symbol name) of each
code/data section, keyed by section name + COMDAT symbol, ignoring .debug$* sections and the object time stamp.
Prints the objects that differ (with -v, the differing sections). Exit status 1 if any object differs or is missing.
Used to show that a dialect edit made for clang leaves the VC6 build's code unchanged.
"""
import argparse
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from verify import Coff


def sections(path):
    c = Coff(path)
    comdat = {}
    for s in c.syms.values():
        if s["sec"] > 0 and s["cls"] == 2 and s["sec"] not in comdat and not s["name"].startswith("."):
            comdat[s["sec"]] = s["name"]
    out = {}
    for i, s in enumerate(c.secs, 1):
        if s["name"].startswith(".debug") or s["name"] == ".drectve":
            continue
        rel = tuple((va, t["name"], typ) for va, t, typ in c.relocs(i))
        key = (s["name"], comdat.get(i, ""))
        out.setdefault(key, []).append((c.section_data(i), rel))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("-v", action="store_true")
    a = ap.parse_args()
    ra, rb = pathlib.Path(a.a), pathlib.Path(a.b)
    objs = sorted(p.relative_to(ra) for p in ra.rglob("*.obj"))
    bad = 0
    for rel in objs:
        pb = rb / rel
        if not pb.exists():
            print(f"MISSING {rel}")
            bad += 1
            continue
        sa, sb = sections(ra / rel), sections(pb)
        diff = sorted(k for k in sa.keys() | sb.keys() if sa.get(k) != sb.get(k))
        if diff:
            bad += 1
            print(f"DIFF {rel}: {len(diff)} sections")
            if a.v:
                for k in diff[:20]:
                    print(f"    {k[0]} {k[1]}")
    print(f"{len(objs) - bad}/{len(objs)} objects identical")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
