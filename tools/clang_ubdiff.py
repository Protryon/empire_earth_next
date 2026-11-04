"""Find the functions whose code depends on a UB-taming clang flag (port phase 1).

  python -I tools/clang_ubdiff.py [--flag -fno-delete-null-pointer-checks] [-j N] [--opt=-O1] [module ...]

Every unit is compiled twice with clang-cl (tools/clang_build.py flags, one section per function, /Gy): with the
flag and without it. The functions whose code or relocations differ are the places where clang exploits the
undefined behaviour the flag tames: for -fno-delete-null-pointer-checks, a null test deleted after a dereference
or a member call through the pointer. Fix those in source until the list is empty, then drop the flag.
Objects go to build/ubdiff/{with,without}/; the list (unit, demangled function) is printed and written to
build/ubdiff/<flag>.txt.
"""
import argparse
import concurrent.futures
import os
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import build
import clang_build
import clang_vc6inc
import verify
from ee import COMPILER, ROOT, TOOLCHAIN
from undname import undname


def functions(path):
    """{function symbol: (code bytes, relocations by target name)} of a /Gy object."""
    c = verify.Coff(path)
    out = {}
    for s in c.functions():
        sec = s["sec"]
        data = c.section_data(sec)
        rel = tuple((va, t["name"], typ) for va, t, typ in c.relocs(sec))
        out[s["name"]] = (data, rel)
    return out


_MD = None


def null_tests(fn):
    """Number of null tests in a function's code: `test r, r` or `cmp x, 0` directly followed by je/jne/sete/setne.
    The flag changes code in thousands of functions (it also stops clang assuming `this` and references non-null);
    only those with fewer null tests without the flag lost a check."""
    global _MD
    if fn is None:
        return 0
    import capstone
    if _MD is None:
        _MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    ins = list(_MD.disasm(fn[0], 0))
    n = 0
    for i, j in zip(ins, ins[1:]):
        ops = i.op_str.replace(" ", "")
        is_test = (i.mnemonic == "test" and len(set(ops.split(","))) == 1) or (i.mnemonic == "cmp" and ops.endswith(",0"))
        if is_test and j.mnemonic in ("je", "jne", "sete", "setne", "cmove", "cmovne"):
            n += 1
    return n


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("names", nargs="*", metavar="module", help=f"default: all ({' '.join(build.MODULES)})")
    ap.add_argument("--flag", default="-fno-delete-null-pointer-checks")
    ap.add_argument("--opt", default="-O1")
    ap.add_argument("-j", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--all", dest="null_tests", action="store_false",
                    help="list every function whose code changes (default: only those that lose null tests)")
    a = ap.parse_args()
    mods = a.names or build.MODULES
    jobs = [j for m in mods for j in build.jobs_for(m)] + build.lib_jobs(mods)
    out = ROOT / "build" / "ubdiff"
    vc6inc = clang_vc6inc.prepare(TOOLCHAIN / COMPILER / "Include", ROOT / "build" / "clang" / "vc6inc")
    for j in jobs:
        verify.include_paths(verify.stage_source(j[1]))
    base = [f for f in clang_build.CLANG_FLAGS if f != a.flag]

    def one(job, variant):
        mod, src, flags, _ = job
        obj = out / variant / mod / build.rel_src(src).with_suffix(".obj")
        ok, log = clang_build.compile_to(src, flags, obj, f"{a.opt} /Gy" + (f" {a.flag}" if variant == "with" else ""),
                                         vc6inc)
        return job, variant, ok, obj, log

    clang_build.CLANG_FLAGS = base
    work = [(j, v) for j in jobs for v in ("with", "without")]
    objs = {}
    with concurrent.futures.ThreadPoolExecutor(a.j) as ex:
        for job, variant, ok, obj, log in ex.map(lambda w: one(*w), work):
            if not ok:
                sys.exit(f"FAILED {build.rel_src(job[1])} ({variant})\n{log[-2000:]}")
            objs[(job[0], str(build.rel_src(job[1])), variant)] = obj
    rows = []
    changed = 0
    for mod, rel, variant in sorted(objs):
        if variant != "with":
            continue
        fw, fo = functions(objs[(mod, rel, "with")]), functions(objs[(mod, rel, "without")])
        for name in sorted(fw.keys() | fo.keys()):
            if fw.get(name) != fo.get(name):
                changed += 1
                if not a.null_tests or null_tests(fw.get(name)) > null_tests(fo.get(name)):
                    rows.append((mod, rel, name))
    print(f"{changed} functions' code changes with {a.flag}; "
          f"{'those with fewer null tests without it' if a.null_tests else 'all listed'}:")
    lines = [f"{mod}\t{rel}\t{undname(n) if n.startswith('?') else n}\t{n}" for mod, rel, n in rows]
    report = out / (a.flag.lstrip("-") + ".txt")
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text("".join(l + "\n" for l in lines), encoding="utf-8")
    for l in lines:
        print(l.rsplit("\t", 1)[0])
    print(f"{len(rows)} functions depend on {a.flag} ({len({r[1] for r in rows})} units) -> {report}")
    sys.exit(1 if rows else 0)


if __name__ == "__main__":
    main()
