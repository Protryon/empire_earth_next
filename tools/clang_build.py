"""Compile the game's units with clang-cl: 32-bit, MSVC ABI, the MSVC STL and the UCRT (VS 2022 Build Tools).

  python -I tools/clang_build.py [-j N] [--out DIR] [--opt X] [--only TEXT] [module ...]     compile census

The units, their order and their defines come from link/<module>/units.txt and tools/ee.py. Flags:
  - clang-cl --target=i686-pc-windows-msvc, MSVC 19.44 compatibility, C++17 (C units: C), native wchar_t;
  - /arch:IA32 (x87 floating point) with cmpxchg8b (+cx8: 64-bit atomics inline, as every Pentium has it),
    /GS- (no stack cookies);
  - -fno-strict-aliasing -fwrapv: the code relies on both (the original compiler never exploited that undefined
    behaviour; PORT_LEARNINGS.md has the plan to drop them; -fno-delete-null-pointer-checks is gone);
  - a unit's `// UNIT-FLAGS:` line adds its defines to the module flags (its VC6 code-generation switches are
    ignored).
Every unit is staged under C:\\EEX (tools/verify.py) before it is compiled.

Objects go to <out>/obj/<module>/<unit>.obj (default out: build/clang). A failed unit's diagnostics go to
<out>/log/<module>/<unit>.log; the summary ranks the most frequent errors (names masked). The full build
(compile + link) is tools/build.py.
"""
import argparse
import concurrent.futures
import os
import pathlib
import re
import subprocess
import sys
import threading
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import units
import verify
from ee import CLANG, ROOT

CLANG_FLAGS = ["--target=i686-pc-windows-msvc", "/nologo", "/c", "-fms-compatibility", "-fms-extensions",
               "-fms-compatibility-version=19.44", "/Zc:wchar_t", "-fdelayed-template-parsing", "/arch:IA32", "/GS-",
               "-Xclang", "-target-feature", "-Xclang", "+cx8",
               "-fno-strict-aliasing", "-fwrapv",
               "-Wno-everything", "-ferror-limit=0", "-fno-color-diagnostics"]
# old code-generation switches a `// UNIT-FLAGS:` line may still carry (the build's --opt decides)
DROP = re.compile(r"^[-/](G[3-7B]|Zm\d+|Zi|Z7|O[12dsxty]|Ob\d|Og|Gy|Gf|GF)$")
ERROR = re.compile(r"^(.*?)\((\d+),\d+\)\s*:\s*error: (.*)$", re.M)


def clang_flags(flags, opt):
    # the build links the static runtime only: a -MD left in a flag string becomes -MT
    return [{"-MD": "-MT", "/MD": "-MT"}.get(f, f) for f in flags.split() if not DROP.match(f)] + opt.split()


def apply_overlay(staged, rules):
    """A copy of the staged unit rewritten with (old, new, expected count) text rules, next to it (so its sibling
    headers resolve) under a per-compile name (two modules may compile the same unit in parallel) -> its path. Test
    profiles only; the source tree is never touched. Refuses when a rule does not match exactly its expected count."""
    staged = pathlib.Path(staged)
    raw = staged.read_bytes()
    crlf = b"\r\n" in raw
    text = raw.decode("latin-1").replace("\r\n", "\n")
    for old, new, want in rules:
        if old is None:   # append to the unit
            text = text.rstrip("\n") + "\n\n" + new
            continue
        n = text.count(old)
        if n != want:
            raise SystemExit(f"overlay {staged.name}: {old!r} matches {n} times, expected {want}")
        text = text.replace(old, new)
    copy = staged.with_name(f"{staged.stem}.test{os.getpid()}-{threading.get_ident()}{staged.suffix}")
    copy.write_bytes((text.replace("\n", "\r\n") if crlf else text).encode("latin-1"))
    return copy


def compile_to(src, flags, obj, opt, overlays=None, extra=()):
    """Compile one source with clang-cl into obj -> (ok, log). overlays: {source path relative to src/: rules}
    (apply_overlay) for a test profile; extra: more clang-cl arguments."""
    flags = verify.unit_flags(src, flags)
    staged = verify.stage_source(src)
    rules = overlays.get(str(units.rel_src(src))) if overlays else None
    iflags, inc = verify.include_paths(staged)
    if rules:
        staged = apply_overlay(staged, rules)
    std = [] if str(src).lower().endswith(".c") else ["/std:c++17"]
    pathlib.Path(obj).parent.mkdir(parents=True, exist_ok=True)
    cmd = [str(CLANG), *CLANG_FLAGS, *std, *clang_flags(flags, opt), *extra, *verify.DEFINES.split(), *iflags,
           f"/Fo{obj}", str(staged)]
    r = subprocess.run(cmd, env=dict(os.environ, INCLUDE=inc), capture_output=True, text=True,
                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    if rules:
        staged.unlink(missing_ok=True)
    return r.returncode == 0, (r.stdout + r.stderr).strip()


def compile_one(job, out, opt, overlays=None, extra=()):
    mod, src, flags = job
    rel = units.rel_src(src)
    ok, log = compile_to(src, flags, out / "obj" / mod / rel.with_suffix(".obj"), opt, overlays, extra)
    logf = out / "log" / mod / rel.with_suffix(".log")
    if ok:
        logf.unlink(missing_ok=True)
    else:
        logf.parent.mkdir(parents=True, exist_ok=True)
        logf.write_text(log)
    return job, ok, log


# the 64-bit census: casts that lose a pointer's upper half, integers turned into pointers, and 64-bit values
# (size_t, ptrdiff_t, pointer differences) narrowed into 32-bit integers
X64_WARNINGS = ["-Wpointer-to-int-cast", "-Wvoid-pointer-to-int-cast", "-Wint-to-pointer-cast",
                "-Wint-to-void-pointer-cast", "-Wshorten-64-to-32"]
DIAG = re.compile(r"^(.*?)\((\d+),\d+\)\s*:\s*(warning|error): (.*?)(?: \[(-W[\w-]+)\])?$", re.M)


def x64_census(out, logs):
    """Every diagnostic of a --x64 compile -> <out>/x64_census.tsv (file, line, kind, flag, message), deduplicated
    (headers are seen by many units), and a summary by warning class and by file."""
    rows = set()
    for job, log in logs:
        for f, line, kind, msg, flag in DIAG.findall(log):
            rel = pathlib.Path(f)
            try:
                rel = rel.resolve().relative_to(verify.ORIG_SRC_ROOT.resolve())
            except ValueError:
                try:
                    rel = rel.resolve().relative_to(ROOT)
                except ValueError:
                    pass
            rows.add((str(rel).replace("\\", "/"), int(line), kind, flag or kind, msg))
    path = out / "x64_census.tsv"
    path.write_text("".join("\t".join(map(str, r)) + "\n" for r in sorted(rows)), encoding="utf-8")
    by_flag, by_file = {}, {}
    for f, _, _, flag, _ in rows:
        by_flag[flag] = by_flag.get(flag, 0) + 1
        by_file[(flag, f)] = by_file.get((flag, f), 0) + 1
    print(f"\nx64 census: {len(rows)} distinct sites -> {path}")
    for flag, n in sorted(by_flag.items(), key=lambda kv: -kv[1]):
        print(f"{n:7}  {flag}")
        for (fl, f), k in sorted(((k, v) for k, v in by_file.items() if k[0] == flag), key=lambda kv: -kv[1])[:5]:
            print(f"           {k:5} {f}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("names", nargs="*", metavar="module", help=f"default: all ({' '.join(units.MODULES)})")
    ap.add_argument("-j", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--out", default=str(ROOT / "build" / "clang"))
    ap.add_argument("--opt", default="-O1", help="clang-cl optimisation (default -O1)")
    ap.add_argument("--only", help="compile only units whose path contains this text")
    ap.add_argument("--x64", action="store_true",
                    help="64-bit census (default out build/clang-x64): compile for x86_64 with the pointer-size "
                         "warnings on; every diagnostic goes to <out>/x64_census.tsv (see X64_WARNINGS)")
    a = ap.parse_args()
    if not CLANG.exists():
        sys.exit(f"{CLANG} is missing: install LLVM (winget install LLVM.LLVM) or set EE_LLVM")
    if a.x64:
        global CLANG_FLAGS
        CLANG_FLAGS = [("--target=x86_64-pc-windows-msvc" if f.startswith("--target=") else f)
                       for f in CLANG_FLAGS if f != "/arch:IA32"] + X64_WARNINGS
        if a.out == str(ROOT / "build" / "clang"):
            a.out = str(ROOT / "build" / "clang-x64")
    mods = a.names or units.MODULES
    jobs = [j for m in mods for j in units.jobs_for(m)] + units.lib_jobs(mods)
    if a.only:
        jobs = [j for j in jobs if a.only in str(j[1])]
    out = pathlib.Path(a.out).resolve()
    for j in jobs:
        verify.include_paths(verify.stage_source(j[1]))
    t0 = time.time()
    done, errors, logs = {}, {}, []
    with concurrent.futures.ThreadPoolExecutor(max(1, a.j)) as ex:
        for job, ok, log in ex.map(lambda j: compile_one(j, out, a.opt), jobs):
            done.setdefault(job[0], [0, 0])[0 if ok else 1] += 1
            logs.append((job, log))
            if not ok:
                errs = ERROR.findall(log)
                first = errs[0] if errs else ("?", "?", log.splitlines()[-1] if log else "no output")
                print(f"FAILED {job[0]} {units.rel_src(job[1])}: {len(errs)} errors; first: "
                      f"{pathlib.Path(first[0]).name}({first[1]}): {first[2]}", flush=True)
                for f, _, msg in errs:
                    key = (pathlib.Path(f).name, re.sub(r"'[^']*'", "'X'", msg))
                    errors[key] = errors.get(key, 0) + 1
    print()
    for m, (ok, bad) in done.items():
        print(f"{m:9} {ok:4} compiled  {bad:3} failed")
    n_bad = sum(b for _, b in done.values())
    print(f"{len(jobs) - n_bad}/{len(jobs)} units compiled in {time.time() - t0:.0f} s -> {out / 'obj'}")
    if a.x64:
        x64_census(out, logs)
    if errors:
        print("\nmost frequent errors (file, message with names masked):")
        for (f, msg), n in sorted(errors.items(), key=lambda kv: -kv[1])[:40]:
            print(f"{n:6}  {f}: {msg}")
    sys.exit(1 if n_bad else 0)


if __name__ == "__main__":
    main()
