"""Clang build driver (port phase 1): compile every unit with clang-cl, 32-bit, MSVC ABI, x87.

  python -I tools/clang_build.py [-j N] [--out DIR] [--opt /Od] [module ...]     compile only (for now)

Same units, flags, defines and include order as tools/build.py (the VC6 reference build), with:
  - clang-cl --target=i686-pc-windows-msvc in MSVC 12.00 (VC6) compatibility mode, wchar_t = unsigned short
    (/Zc:wchar_t-, as VC6);
  - /arch:IA32 (x87 floating point, no SSE) while the clang build is compared against the VC6 build;
  - -fno-strict-aliasing -fwrapv (the code relies on both);
  - VC6-only flags dropped (-G6, -Zm, -Zi; /O1 replaced by --opt);
  - the stock VC6 headers the code includes are read through build/clang/vc6inc, a copy of toolchain/msvc6.0/Include
    with the patches in tools/clang_vc6inc.py applied (the toolchain itself stays untouched).

Objects go to <out>/obj/<module>/<unit>.obj (default out: build/clang). A failed unit's diagnostics go to
<out>/log/<module>/<unit>.log; the summary lists the first error of each.
"""
import argparse
import concurrent.futures
import os
import pathlib
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import build
import clang_vc6inc
import verify
from ee import COMPILER, ROOT, TOOLCHAIN

CLANG = pathlib.Path(os.environ.get("EE_CLANG", r"C:\Program Files\LLVM\bin\clang-cl.exe"))
CLANG_FLAGS = ["--target=i686-pc-windows-msvc", "/nologo", "/c", "-fms-compatibility", "-fms-extensions",
               "-fms-compatibility-version=12.00", "/Zc:wchar_t-", "-fdelayed-template-parsing", "/arch:IA32", "/GS-",
               "-fno-strict-aliasing", "-fwrapv", "-Wno-everything", "-ferror-limit=0", "-fno-color-diagnostics"]
DROP = re.compile(r"^[-/](G[3-7B]|Zm\d+|Zi|Z7|O[12dsxty]|Ob\d|Og|Gy|Gf|GF)$")
ERROR = re.compile(r"^(.*?)\((\d+),\d+\)\s*:\s*error: (.*)$", re.M)


def clang_flags(flags, opt):
    return [f for f in flags.split() if not DROP.match(f)] + opt.split()


def compile_to(src, flags, obj, opt, vc6inc):
    """Compile one source with clang-cl into obj -> (ok, log)."""
    flags = verify.unit_flags(src, flags)
    staged = verify.stage_source(src)
    iflags, inc = verify.include_paths(staged)
    inc = inc.replace(str(TOOLCHAIN / COMPILER / "Include"), str(vc6inc))
    pathlib.Path(obj).parent.mkdir(parents=True, exist_ok=True)
    cmd = [str(CLANG), *CLANG_FLAGS, *clang_flags(flags, opt), *verify.DEFINES.split(), *iflags, f"/Fo{obj}",
           str(staged)]
    r = subprocess.run(cmd, env=dict(os.environ, INCLUDE=inc), capture_output=True, text=True,
                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    return r.returncode == 0, (r.stdout + r.stderr).strip()


def compile_one(job, out, opt, vc6inc):
    mod, src, flags, pkg = job
    rel = build.rel_src(src)
    ok, log = compile_to(src, flags, out / "obj" / mod / rel.with_suffix(".obj"), opt, vc6inc)
    logf = out / "log" / mod / rel.with_suffix(".log")
    if ok:
        logf.unlink(missing_ok=True)
    else:
        logf.parent.mkdir(parents=True, exist_ok=True)
        logf.write_text(log)
    return job, ok, log


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("names", nargs="*", metavar="module", help=f"default: all ({' '.join(build.MODULES)})")
    ap.add_argument("-j", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--out", default=str(ROOT / "build" / "clang"))
    ap.add_argument("--opt", default="/Od", help="optimisation flags for clang-cl (default /Od)")
    ap.add_argument("--only", help="compile only units whose path contains this text")
    a = ap.parse_args()
    if not CLANG.exists():
        sys.exit(f"{CLANG} is missing: install LLVM (winget install LLVM.LLVM) or set EE_CLANG")
    mods = a.names or build.MODULES
    jobs = [j for m in mods for j in build.jobs_for(m)] + build.lib_jobs(mods)
    if a.only:
        jobs = [j for j in jobs if a.only in str(j[1])]
    out = pathlib.Path(a.out).resolve()
    vc6inc = clang_vc6inc.prepare(TOOLCHAIN / COMPILER / "Include", out / "vc6inc")
    for j in jobs:
        verify.include_paths(verify.stage_source(j[1]))
    t0 = time.time()
    done, errors = {}, {}
    with concurrent.futures.ThreadPoolExecutor(max(1, a.j)) as ex:
        for job, ok, log in ex.map(lambda j: compile_one(j, out, a.opt, vc6inc), jobs):
            done.setdefault(job[0], [0, 0])[0 if ok else 1] += 1
            if not ok:
                errs = ERROR.findall(log)
                first = errs[0] if errs else ("?", "?", log.splitlines()[-1] if log else "no output")
                print(f"FAILED {job[0]} {build.rel_src(job[1])}: {len(errs)} errors; first: "
                      f"{pathlib.Path(first[0]).name}({first[1]}): {first[2]}", flush=True)
                for f, _, msg in errs:
                    key = (pathlib.Path(f).name, re.sub(r"'[^']*'", "'X'", msg))
                    errors[key] = errors.get(key, 0) + 1
    print()
    for m, (ok, bad) in done.items():
        print(f"{m:9} {ok:4} compiled  {bad:3} failed")
    n_bad = sum(b for _, b in done.values())
    print(f"{len(jobs) - n_bad}/{len(jobs)} units compiled in {time.time() - t0:.0f} s -> {out / 'obj'}")
    if errors:
        print("\nmost frequent errors (file, message with names masked):")
        for (f, msg), n in sorted(errors.items(), key=lambda kv: -kv[1])[:40]:
            print(f"{n:6}  {f}: {msg}")
    sys.exit(1 if n_bad else 0)


if __name__ == "__main__":
    main()
