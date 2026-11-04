"""Compare the sizes of the records the game reads and writes raw (Read/Write(&x, sizeof(x))) between x86 and x64.

  python -I tools/record_sizes.py [-j N]

Every unit that does raw record I/O is compiled -fsyntax-only for both targets with clang's complete record-layout
dump; a record whose size differs would read or write a different number of bytes on x64 (a pointer, size_t or a
vptr inside it). Prints each record's two sizes; exits 1 if any differ.
"""
import argparse
import collections
import concurrent.futures
import os
import pathlib
import re
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import clang_build
import units
import verify
from ee import CLANG, ROOT

PRIM = r"(long|unsigned long|short|unsigned short|char|unsigned char|float|double|int|bool|DWORD|WORD|BYTE|wchar_t|" \
       r"unsigned int|GUID)"
SITE = re.compile(r"(Read|Write|ReadFile|WriteFile|fread|fwrite)\([^;]*sizeof\(([^()]*)\)")
LAYOUT = re.compile(r"^\s*0 \| (?:class|struct|union) (\S+)\n(?:.*\n)*?\s*\| \[sizeof=(\d+)", re.M)


def record_sites():
    """{unit source: {record type name}} for every raw sizeof-sized Read/Write of a non-primitive."""
    found = collections.defaultdict(set)
    for p in (ROOT / "src").rglob("*.cpp"):
        if "ThirdParty" in p.parts:
            continue
        lines = p.read_text(encoding="latin-1").splitlines()
        for i, s in enumerate(lines):
            for m in SITE.finditer(s):
                x = m.group(2).strip()
                if re.fullmatch(PRIM, x):
                    continue
                t = x
                if re.fullmatch(r"[a-z]\w*|this->m\w+|\*?\w+", x):   # a variable: find its declaration above
                    name = x.split("->")[-1].lstrip("*")
                    for j in range(i, max(i - 400, -1), -1):
                        d = re.search(r"([\w:<>, ]+?[\w>])\s*\*?\s*\b" + re.escape(name) + r"\s*(\[[^\]]*\])?\s*[;=(]",
                                      lines[j])
                        if d and not re.match(r"\s*(return|if|else)\b", lines[j]):
                            t = d.group(1).split()[-1]
                            break
                t = t.split("[")[0]
                if re.fullmatch(r"[A-Z]\w+", t):
                    found[p].add(t)
    return found


def layouts(src, target):
    jobs = {pathlib.Path(s).resolve(): (m, s, f) for m in units.MODULES for m, s, f in units.jobs_for(m)}
    mod, s, flags = jobs[pathlib.Path(src).resolve()]
    flags = verify.unit_flags(s, flags)
    staged = verify.stage_source(s)
    iflags, inc = verify.include_paths(staged)
    base = [("--target=" + target if f.startswith("--target=") else f) for f in clang_build.CLANG_FLAGS
            if f not in ("/c", "/arch:IA32")]
    cmd = [str(CLANG), *base, "/std:c++17", *clang_build.clang_flags(flags, "-O0"), *verify.DEFINES.split(), *iflags,
           "-fsyntax-only", "-Xclang", "-fdump-record-layouts-complete", str(staged)]
    r = subprocess.run(cmd, env=dict(os.environ, INCLUDE=inc), capture_output=True, text=True, errors="replace",
                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    return {name.split("::")[-1]: int(size) for name, size in LAYOUT.findall(r.stdout)}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("-j", type=int, default=os.cpu_count() or 4)
    a = ap.parse_args()
    sites = record_sites()
    rows, bad = {}, 0
    with concurrent.futures.ThreadPoolExecutor(a.j) as ex:
        futs = {ex.submit(lambda s=s: (layouts(s, "i686-pc-windows-msvc"), layouts(s, "x86_64-pc-windows-msvc"))): s
                for s in sites}
        for fut in concurrent.futures.as_completed(futs):
            x86, x64 = fut.result()
            for t in sites[futs[fut]]:
                rows[t] = (x86.get(t), x64.get(t), futs[fut].relative_to(ROOT / "src"))
    for t, (a32, a64, src) in sorted(rows.items()):
        flag = "" if a32 == a64 and a32 is not None else "  <-- DIFFERS" if a32 and a64 else "  (no layout)"
        bad += a32 != a64
        print(f"{t:32} x86 {a32!s:>6}  x64 {a64!s:>6}  {src}{flag}")
    print(f"\n{len(rows)} records, {bad} differ")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
