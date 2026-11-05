"""LP64 census: what changes when `long` is 64-bit (Linux/macOS) instead of 32-bit (Windows, x86 and x64).

  python -I tools/lp64_census.py format [-j N] [--fix] [--only TEXT]     printf/scanf format census (-Wformat, x64)
  python -I tools/lp64_census.py ast [-j N] [--only TEXT]                AST census (clang-query, x64)

Windows keeps `long` at 32 bits on x64 (LLP64), so a Windows build cannot show an LP64 difference by running. The
census finds the constructs whose meaning depends on the width of `long`, over every first-party unit (the vendored
libraries are LP64-clean upstream), compiled for x86_64 with the build's own flags:

format   every -Wformat diagnostic (a printf/scanf argument whose type does not match its conversion: `%d` of a
         long reads half of it on LP64, `sscanf("%d", long*)` writes half of it). --fix inserts the length modifier
         the argument's type needs (`l` for long/unsigned long, `z` for size_t) and keeps the conversion letter (so
         `%d` of an unsigned long becomes `%ld`, printing what Windows printed); an `l` given to an int (a field
         retyped as int32_t) is dropped. Sites -> <out>/format.tsv.
ast      clang-query matchers (QUERIES): sizeof of long, memcpy/memset/memcmp through a long*, casts to and from
         long pointers (type punning a byte buffer), shifts and bitwise operators on long, `~` on long, comparisons
         with 0xffffffff, unions holding a long, mixed long/unsigned int arithmetic (unsigned long on Windows, long
         on LP64), and sizeof of a record in an I/O call (memcpy/Read/Write/fread/fwrite/send/recv). Sites ->
         <out>/ast.tsv (kind, file, line, the dump's first line).

Default out: build/lp64. Locations are first-party files only (src/, include/).
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

CLANG_QUERY = CLANG.parent / "clang-query.exe"
LONG = 'qualType(hasCanonicalType(anyOf(asString("long"), asString("unsigned long"))))'
LONGISH = ('qualType(hasCanonicalType(anyOf(asString("long"), asString("unsigned long"), '
           'constantArrayType(hasElementType(qualType(hasCanonicalType(anyOf(asString("long"), '
           'asString("unsigned long")))))))))')
IO = ('callee(functionDecl(hasAnyName("memcpy", "memmove", "memcmp", "memset", "Read", "Write", "fread", "fwrite", '
      '"ReadFile", "WriteFile", "send", "recv", "sendto", "recvfrom", "ReadPayload", "WritePayload")))')
MINUS1 = 'unaryOperator(hasOperatorName("-"), hasUnaryOperand(integerLiteral(equals(1))))'
ALLONES = (f'hasEitherOperand(ignoringParenImpCasts(anyOf(integerLiteral(equals(4294967295)), {MINUS1}, '
           f'explicitCastExpr(hasSourceExpression(ignoringParenImpCasts({MINUS1}))))))')
QUERIES = {
    "sizeof-long": f'unaryExprOrTypeTraitExpr(ofKind("UETT_SizeOf"), hasArgumentOfType({LONGISH}))',
    "sizeof-record-io": f'unaryExprOrTypeTraitExpr(ofKind("UETT_SizeOf"), '
                        f'hasArgumentOfType(qualType(hasCanonicalType(hasDeclaration(cxxRecordDecl())))), '
                        f'hasAncestor(callExpr({IO})))',
    "mem-long-ptr": f'callExpr(callee(functionDecl(hasAnyName("memcpy", "memmove", "memcmp", "memset"))), '
                    f'hasAnyArgument(ignoringParenImpCasts(hasType(pointerType(pointee({LONG}))))))',
    "cast-to-long-ptr": f'explicitCastExpr(hasDestinationType(anyOf(pointerType(pointee({LONG})), '
                        f'referenceType(pointee({LONG})))), unless(hasSourceExpression(hasType({LONG}))))',
    "cast-from-long-ptr": f'explicitCastExpr(hasSourceExpression(ignoringParenImpCasts(hasType(pointerType('
                          f'pointee({LONG}))))), unless(hasDestinationType(pointerType(pointee({LONG})))))',
    "shift-long": f'binaryOperator(hasAnyOperatorName("<<", ">>", "<<=", ">>="), hasLHS(hasType({LONG})))',
    "bitop-long": f'binaryOperator(hasAnyOperatorName("^", "&", "|", "^=", "&=", "|="), hasType({LONG}))',
    "not-long": f'unaryOperator(hasOperatorName("~"), hasType({LONG}))',
    # all-ones sentinels: an unsigned long set to -1 in memory has 64 bits set on LP64, one read from a file or
    # message (4 bytes) 32; a long compared with 0xffffffff is converted to unsigned long on Windows, the literal to
    # long on LP64
    "sentinel-ulong": f'binaryOperator(isComparisonOperator(), hasEitherOperand(ignoringParenImpCasts(hasType('
                      f'qualType(hasCanonicalType(asString("unsigned long")))))), {ALLONES})',
    "sentinel-long": f'binaryOperator(isComparisonOperator(), hasEitherOperand(ignoringParenImpCasts(hasType('
                     f'qualType(hasCanonicalType(asString("long")))))), '
                     f'hasEitherOperand(ignoringParenImpCasts(integerLiteral(equals(4294967295)))))',
    "union-long": f'fieldDecl(hasType({LONGISH}), hasParent(recordDecl(isUnion())))',
    "long-uint-mix": 'binaryOperator(unless(hasAnyOperatorName("=", ",")), '
                     'hasEitherOperand(ignoringParenImpCasts(hasType(qualType(hasCanonicalType(asString("long")))))), '
                     'hasEitherOperand(ignoringParenImpCasts(hasType(qualType(hasCanonicalType('
                     'asString("unsigned int")))))))',
}

FMT = re.compile(r"^(.*?)\((\d+),(\d+)\)\s*:\s*warning: (.*?) \[(-Wformat[\w-]*)\]$", re.M)
ERRLINE = re.compile(r"^.*?\(\d+,\d+\)\s*:\s*error: .*$", re.M)
FIXIT = re.compile(r'^fix-it:"(.*?)":\{(\d+):(\d+)-(\d+):(\d+)\}:"(.*)"$', re.M)
BIND = re.compile(r'^Binding for "([\w-]+)":\n(\S+) 0x[0-9a-f]+ <(.*?):(\d+):(\d+)(?:, [^>]*)?>(.*)$((?:\n[|` ].*){0,3})', re.M)


def first_party(path):
    """A diagnostic's file -> its path relative to the repository (src/... or include/...), or None."""
    p = pathlib.Path(path)
    try:
        rel = p.resolve().relative_to(verify.ORIG_SRC_ROOT.resolve())
        p = ROOT / "src" / rel
    except ValueError:
        pass
    try:
        rel = p.resolve().relative_to(ROOT)
    except ValueError:
        return None
    if rel.parts[0] not in ("src", "include") or "ThirdParty" in rel.parts or "winsdk" in rel.parts:
        return None
    return rel.as_posix()


def first_party_jobs(only):
    jobs = [j for m in units.MODULES for j in units.jobs_for(m)]
    jobs = [j for j in jobs if "ThirdParty" not in pathlib.Path(j[1]).parts]
    if only:
        jobs = [j for j in jobs if only in str(j[1])]
    seen, out = set(), []
    for j in jobs:   # a unit linked by two modules (none today) is censused once
        if str(j[1]) not in seen:
            seen.add(str(j[1]))
            out.append(j)
    return out


def compile_args(job):
    mod, src, flags = job
    flags = verify.unit_flags(src, flags)
    staged = verify.stage_source(src)
    iflags, inc = verify.include_paths(staged)
    std = [] if str(src).lower().endswith(".c") else ["/std:c++17"]
    args = [f for f in clang_build.CLANG_FLAGS if f != "/c"] + std + clang_build.clang_flags(flags, "-O0") + \
        verify.DEFINES.split() + iflags
    return staged, args, inc


def run_format(job):
    staged, args, inc = compile_args(job)
    cmd = [str(CLANG), *args, "/Zs", "-Wformat", "-fdiagnostics-parseable-fixits", str(staged)]
    r = subprocess.run(cmd, env=dict(os.environ, INCLUDE=inc), capture_output=True, text=True, errors="replace",
                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    return job, r.returncode, r.stdout + r.stderr


def run_ast(job, script):
    staged, args, inc = compile_args(job)
    cmd = [str(CLANG_QUERY), "-f", str(script), str(staged), "--", "--driver-mode=cl", *args]
    r = subprocess.run(cmd, env=dict(os.environ, INCLUDE=inc), capture_output=True, text=True, errors="replace",
                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    return job, r.returncode, r.stdout + r.stderr


def parse_format(log):
    """-> [(file, line, col, message, flag, fix)] with fix = (line, col0, col1, replacement) or None."""
    out = []
    lines = log.splitlines()
    for i, s in enumerate(lines):
        m = FMT.match(s)
        if not m:
            continue
        f = first_party(m.group(1))
        fix = None
        for t in lines[i + 1:i + 8]:
            fm = FIXIT.match(t)
            if fm:
                if first_party(fm.group(1)) == f:
                    fix = (int(fm.group(2)), int(fm.group(3)), int(fm.group(5)), fm.group(6))
                break
            if FMT.match(t):
                break
        if f:
            out.append((f, int(m.group(2)), int(m.group(3)), m.group(4), m.group(5), fix))
    return out


ARG = re.compile(r"format specifies type '([^']*)' but the argument has type '([^']*)'")


def category(msg):
    m = ARG.search(msg)
    if not m:
        return "other"
    want, have = m.groups()
    aka = re.search(r"aka '([^']*)'", msg)
    canon = aka.group(1) if aka else have
    if "long long" in canon or "size_t" in have:
        return f"{want} <- 64-bit ({have.split(' ')[0]})"
    if "long" in canon:
        return f"{want} <- long"
    return f"{want} <- {canon}"


CONV = re.compile(r"%([-+ #0]*)(\*|\d+)?(?:\.(\*|\d+))?(hh|h|ll|l|I64|I32|z|j|t|L)?([diouxXcsSpnfeEgGaA\[])")


def modifier_for(msg):
    """The length modifier the argument's type needs ("l", "z", or "" for none), or None when the diagnostic is not
    a pure width mismatch between int and long (or size_t)."""
    m = ARG.search(msg)
    if not m:
        return None
    want, have = m.groups()
    aka = re.search(r"aka '([^']*)'", msg)
    canon = (aka.group(1) if aka else have).replace(" *", "*")
    base = want.replace(" *", "*")
    if re.fullmatch(r"(unsigned )?(long )?int\*?|(unsigned )?long\*?", base) is None:
        return None
    ptr = base.endswith("*")
    if canon.endswith("*") != ptr:
        return None
    arg = canon.rstrip("*")
    if arg in ("long", "unsigned long"):
        return "l"
    if arg in ("int", "unsigned int"):
        return ""
    if have.startswith(("size_t", "__size_t")) and not ptr:
        return "z"
    return None


def apply_fixes(rows):
    """Insert the needed length modifier at each fix-it range (the conversion the diagnostic points at)."""
    edits = collections.defaultdict(dict)
    manual = []
    for f, line, col, msg, flag, fix in rows:
        mod = modifier_for(msg)
        if mod is None or fix is None:
            manual.append((f, line, col, msg))
            continue
        fl, c0, c1, _ = fix
        key = (fl, c0, c1)
        if key in edits[f] and edits[f][key] != mod:
            manual.append((f, line, col, msg + " (conflicting fixes at one place: a macro?)"))
            continue
        edits[f][key] = mod
    changed = 0
    for f, eds in edits.items():
        path = ROOT / f
        raw = path.read_bytes()
        crlf = b"\r\n" in raw
        text = raw.decode("latin-1").split("\r\n" if crlf else "\n")
        for (fl, c0, c1), mod in sorted(eds.items(), key=lambda kv: (-kv[0][0], -kv[0][1])):
            s = text[fl - 1]
            spec = s[c0 - 1:c1 - 1]
            m = CONV.fullmatch(spec)
            if not m or (m.group(4) or "") not in ("", "l") or m.group(5) not in "diouxXn":
                manual.append((f, fl, c0, f"unexpected conversion {spec!r}"))
                continue
            conv = "u" if mod == "z" and spec[-1] in "di" else spec[-1]   # size_t is unsigned: %zu
            new = spec[:m.start(4) if m.group(4) else -1] + mod + conv
            text[fl - 1] = s[:c0 - 1] + new + s[c1 - 1:]
            changed += 1
        path.write_bytes(("\r\n" if crlf else "\n").join(text).encode("latin-1"))
    return changed, manual


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("mode", choices=["format", "ast"])
    ap.add_argument("-j", type=int, default=3)
    ap.add_argument("--out", default=str(ROOT / "build" / "lp64"))
    ap.add_argument("--only", help="only units whose path contains this text")
    ap.add_argument("--fix", action="store_true", help="format: insert the length modifiers (see the docstring)")
    a = ap.parse_args()
    clang_build.set_target(x64=True)
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    jobs = first_party_jobs(a.only)
    for j in jobs:
        verify.include_paths(verify.stage_source(j[1]))
    failed, errors = [], set()
    if a.mode == "format":
        rows = set()
        with concurrent.futures.ThreadPoolExecutor(a.j) as ex:
            for job, rc, log in ex.map(run_format, jobs):
                if rc:
                    failed.append(units.rel_src(job[1]))
                    errors.update(ERRLINE.findall(log))
                rows.update(parse_format(log))
        rows = sorted(rows)
        (out / "format.tsv").write_text("".join(f"{f}\t{l}\t{c}\t{fl}\t{category(m)}\t{m}\n"
                                                for f, l, c, m, fl, _ in rows), encoding="utf-8")
        by = collections.Counter(category(r[3]) for r in rows)
        print(f"{len(rows)} distinct -Wformat sites in {len({r[0] for r in rows})} files -> {out / 'format.tsv'}")
        for k, n in by.most_common():
            print(f"{n:6}  {k}")
        if a.fix:
            changed, manual = apply_fixes(rows)
            print(f"\n{changed} conversions given a length modifier; {len(manual)} left for review:")
            for f, l, c, m in manual:
                print(f"  {f}:{l}:{c}: {m}")
    else:
        script = out / "queries.txt"
        script.write_text("set output dump\nset bind-root false\n" +
                          "".join(f'match {q}.bind("{k}")\n' for k, q in QUERIES.items()), encoding="utf-8")
        rows = set()
        with concurrent.futures.ThreadPoolExecutor(a.j) as ex:
            for job, rc, log in ex.map(lambda j: run_ast(j, script), jobs):
                if rc or "error:" in log and "Error parsing" in log:
                    failed.append(units.rel_src(job[1]))
                for kind, node, f, line, col, rest, more in BIND.findall(log):
                    rel = first_party(f)
                    if rel:
                        more = " ".join(re.sub(r"0x[0-9a-f]+ <[^>]*>", "", t).strip(" |`-") for t in more.splitlines() if t)
                        rows.add((kind, rel, int(line), int(col), (node + rest.strip() + " :: " + more)[:300]))
        rows = sorted(rows)
        (out / "ast.tsv").write_text("".join("\t".join(map(str, r)) + "\n" for r in rows), encoding="utf-8")
        by = collections.Counter(r[0] for r in rows)
        print(f"{len(rows)} distinct sites -> {out / 'ast.tsv'}")
        for k in QUERIES:
            files = collections.Counter(r[1] for r in rows if r[0] == k)
            print(f"{by.get(k, 0):6}  {k}" + "".join(f"\n           {n:5} {f}" for f, n in files.most_common(4)))
    if failed:
        print(f"\n{len(failed)} units did not compile:", *failed[:20], sep="\n  ")
    if errors:
        (out / "errors.txt").write_text("".join(f"{e}\n" for e in sorted(errors)), encoding="utf-8")
        print(f"\n{len(errors)} distinct errors -> {out / 'errors.txt'}")
    print(f"\n{len(jobs)} units")


if __name__ == "__main__":
    main()
