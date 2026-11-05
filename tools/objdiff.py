"""Compare the code of two object trees function by function (the same source compiled before and after a change).

  python -I tools/objdiff.py OLD_OBJ_DIR NEW_OBJ_DIR [-v] [--names]

Both directories are clang_build.py outputs (<out>/obj). For every object present in both, every function (a
code-section symbol) is compared by its bytes and its relocations' offsets and types (relocated fields hold no
target address in an object file, so a renamed callee does not count; a different call target with the same name
is not seen). A change meant to keep behaviour (retyping long as int32_t where both are 32 bits) shows up here as
"0 changed"; anything else is listed. --names also lists the functions present on one side only (a changed
mangled name: a parameter's type changed).
"""
import argparse
import collections
import pathlib
import re
import struct
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import verify

CODE = 0x20   # IMAGE_SCN_CNT_CODE
ANON = re.compile(r"\?A0x[0-9A-Fa-f]+@")


def functions(path):
    """{symbol: (bytes, ((offset, type), ...))} of an object's code symbols; a symbol's extent runs to the next
    symbol of its section (or the section's end)."""
    c = verify.Coff(path)
    by_sec = collections.defaultdict(list)
    for s in c.syms.values():
        if s["sec"] > 0 and c.secs[s["sec"] - 1]["chars"] & CODE and s["cls"] in (2, 3) and \
                not s["name"].startswith((".", "$")):
            by_sec[s["sec"]].append(s)
    out = {}
    for secno, syms in by_sec.items():
        sec = c.secs[secno - 1]
        data = c.section_data(secno)
        relocs = []
        for i in range(sec["nrel"]):
            va, _, typ = struct.unpack_from("<IIH", c.b, sec["relptr"] + 10 * i)
            relocs.append((va, typ))
        starts = sorted({s["value"] for s in syms})
        ends = {a: b for a, b in zip(starts, starts[1:] + [len(data)])}
        for s in syms:
            a, b = s["value"], ends[s["value"]]
            # the anonymous namespace's number comes from the file name and the compile time: not a difference
            out[ANON.sub("?A0x@", s["name"])] = (data[a:b], tuple((o - a, t) for o, t in relocs if a <= o < b))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("old")
    ap.add_argument("new")
    ap.add_argument("-v", action="store_true", help="list every changed function")
    ap.add_argument("--names", action="store_true", help="list the functions present on one side only")
    a = ap.parse_args()
    old, new = pathlib.Path(a.old), pathlib.Path(a.new)
    objs = sorted(p.relative_to(old) for p in old.rglob("*.obj") if (new / p.relative_to(old)).exists())
    missing = sorted(p.relative_to(old) for p in old.rglob("*.obj") if not (new / p.relative_to(old)).exists())
    same = changed = only_old = only_new = 0
    changed_by_obj = collections.Counter()
    for rel in objs:
        fo, fn = functions(old / rel), functions(new / rel)
        for name in sorted(set(fo) & set(fn)):
            if fo[name] == fn[name]:
                same += 1
            else:
                changed += 1
                changed_by_obj[rel] += 1
                if a.v:
                    print(f"changed  {rel}  {name}  ({len(fo[name][0])} -> {len(fn[name][0])} bytes)")
        for name in sorted(set(fo) - set(fn)):
            only_old += 1
            if a.names:
                print(f"old only {rel}  {name}")
        for name in sorted(set(fn) - set(fo)):
            only_new += 1
            if a.names:
                print(f"new only {rel}  {name}")
    print(f"\n{len(objs)} objects ({len(missing)} only in the old tree); functions: {same} identical, {changed} "
          f"changed, {only_old} only before, {only_new} only after")
    for rel, n in changed_by_obj.most_common(40):
        print(f"{n:6}  {rel}")


if __name__ == "__main__":
    main()
