"""Check the sizes of the records the game reads and writes raw (Read/Write(&x, sizeof(x))) against their pins.

  python -I tools/record_sizes.py [-j N]

A raw record's size is its file format: every one is pinned in source by a `static_assert(sizeof(T) == N, ...)`
(the size the original's files have). Every unit that does raw record I/O is compiled -fsyntax-only with the
build's flags and clang's complete record-layout dump; a record whose size is not its pin, or that has no pin,
would read or write another number of bytes than the files hold (a pointer, size_t, long on LP64 or a vptr inside
it). Prints each record's size and pin; exits 1 if any differs or is unpinned.
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
PIN = re.compile(r"static_assert\(sizeof\((\w+)\)\s*==\s*(\d+)\s*,")
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


def pins():
    """{record type name: pinned size} of every `static_assert(sizeof(T) == N, ...)` in src/ and include/."""
    out = {}
    for d in ("src", "include"):
        for p in (ROOT / d).rglob("*"):
            if p.suffix.lower() in (".cpp", ".h", ".c") and "ThirdParty" not in p.parts:
                for name, size in PIN.findall(p.read_text(encoding="latin-1")):
                    out[name] = int(size)
    return out


def layouts(src):
    jobs = {pathlib.Path(s).resolve(): (m, s, f) for m in units.MODULES for m, s, f in units.jobs_for(m)}
    mod, s, flags = jobs[pathlib.Path(src).resolve()]
    flags = verify.unit_flags(s, flags)
    staged = verify.stage_source(s)
    iflags, inc = verify.include_paths(staged)
    base = [f for f in clang_build.CLANG_FLAGS if f != "/c"]
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
    pinned = pins()
    rows, bad = {}, 0
    with concurrent.futures.ThreadPoolExecutor(a.j) as ex:
        futs = {ex.submit(layouts, s): s for s in sites}
        for fut in concurrent.futures.as_completed(futs):
            sizes = fut.result()
            for t in sites[futs[fut]]:
                rows[t] = (sizes.get(t), pinned.get(t), futs[fut].relative_to(ROOT / "src"))
    for t, (size, pin, src) in sorted(rows.items()):
        flag = "" if size == pin and size is not None else "  (no layout)" if size is None else \
            "  <-- NOT PINNED" if pin is None else "  <-- DIFFERS"
        bad += size != pin
        print(f"{t:32} size {size!s:>6}  pin {pin!s:>6}  {src}{flag}")
    print(f"\n{len(rows)} records, {bad} not at their pinned size")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
