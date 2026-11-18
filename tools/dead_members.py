"""Dead-member census of the first-party C++ headers: the class members nothing names, and their deletion.

    python -I tools/dead_members.py report [--headers DIR ...] [--exclude NAME ...]
    python -I tools/dead_members.py delete NAME ... [--headers DIR ...]

The linker's census (tools/dead_census.py) cannot see two kinds of dead member: a virtual one (its vtable keeps it)
and an unused inline one (never emitted). This census reads the source instead. A member is every function declared
at class indent (four spaces) in a header under --headers (default: include/lle, include/dx7, include/ee and the
headers under src/). It is dead when its name appears nowhere else as a word: not in src, include (but include/rust),
tools or tests outside comments, its declaration lines and its definitions (`Type Class::Name(` at column 0). A name
taken by address, passed to a macro or used in POSIX-only code counts as a use, so the list errs toward keeping.

Rust code never calls a C++ member by name (only through a C++ host), so rust/ is not read. Overloads share their
name: a name is dead only when every overload is. Still build both profiles after a deletion: the build is the
proof (a use through a template or a macro's token paste shows there).

`delete` removes each name's declarations in the scanned headers (to the end of the statement or inline body) and its
out-of-line definitions in src and tools (with the comment lines right above), then prints what went. The comments
above a deleted header declaration stay: read the diff for orphans.
"""
import argparse
import collections
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SUFFIXES = (".cpp", ".h", ".c", ".inl", ".mm", ".hpp")
DEFAULT_HEADERS = ["include/lle", "include/dx7", "include/ee", "src"]
KEYWORDS = {"operator", "if", "while", "for", "return", "switch", "sizeof", "defined", "decltype", "static_assert"}
DECL = re.compile(r"^\s{4}(?!return\b|else\b|case\b|delete\b|new\b|throw\b|goto\b)(?:virtual\s+|static\s+|inline\s+|explicit\s+)*(?:const\s+)?[\w:<>~][\w:<>\*&, ]*?[\s\*&]+(\w+)\s*\(")
WORD = re.compile(r"\b[A-Za-z_]\w*\b")


def code_files():
    for d in ("src", "include", "tools", "tests"):
        for p in (ROOT / d).rglob("*"):
            rel = p.relative_to(ROOT).as_posix()
            if p.suffix in SUFFIXES and p.is_file() and not rel.startswith("include/rust/") and "/ThirdParty/" not in "/" + rel:
                yield p


def read(p):
    return p.read_bytes().decode("utf-8", "surrogateescape")


def headers_of(roots):
    for r in roots:
        base = ROOT / r
        for p in sorted(base.rglob("*.h")) if base.is_dir() else []:
            if "ThirdParty" not in p.parts and not p.relative_to(ROOT).as_posix().startswith("include/rust/"):
                yield p


def declarations(roots):
    """name -> {(file, line index)} of its member declarations."""
    decl = collections.defaultdict(set)
    for h in headers_of(roots):
        for i, line in enumerate(read(h).splitlines()):
            m = DECL.match(line)
            if m and m.group(1) not in KEYWORDS and not line.lstrip().startswith("//"):
                decl[m.group(1)].add((h.as_posix(), i))
    return decl


def strip(line):
    line = re.sub(r'"(\\.|[^"\\])*"', '""', line)
    return line.split("//", 1)[0]


def report(roots, exclude):
    decl = declarations(roots)
    names = set(decl) - set(exclude)
    uses = collections.Counter()
    for p in code_files():
        key = p.as_posix()
        in_block = False
        for i, raw in enumerate(read(p).splitlines()):
            line = raw
            if in_block:
                if "*/" not in line:
                    continue
                line, in_block = line.split("*/", 1)[1], False
            if "/*" in line and "*/" not in line.split("/*", 1)[1]:
                line, in_block = line.split("/*", 1)[0], True
            line = strip(line)
            for m in WORD.finditer(line):
                n = m.group(0)
                if n not in names:
                    continue
                if (key, i) in decl[n]:
                    continue
                # a definition at column 0: `Type Class::Name(`
                if raw[:1] not in (" ", "\t") and re.search(r"\w::" + n + r"\s*\(", line) and \
                        not re.search(r"(return|=|->|\.|\()\s*[\w:]*::" + n + r"\b", line):
                    continue
                uses[n] += 1
    dead = sorted(n for n in names if uses[n] == 0)
    for n in dead:
        where = sorted(f"{pathlib.Path(f).relative_to(ROOT).as_posix()}:{i + 1}" for f, i in decl[n])
        print(f"{n:40s} {' '.join(where)}")
    print(f"# {len(dead)} of {len(names)} member names have no use", file=sys.stderr)


def span_end(lines, i):
    """The last line of the declaration or definition that starts at line i: its ';' or its brace block."""
    depth = paren = 0
    braced = False
    for j in range(i, len(lines)):
        for ch in strip(lines[j]):
            if ch == "(":
                paren += 1
            elif ch == ")":
                paren -= 1
            elif ch == "{":
                depth += 1
                braced = True
            elif ch == "}":
                depth -= 1
                if depth == 0 and braced:
                    return j
            elif ch == ";" and depth == 0 and paren == 0 and not braced:
                return j
    raise SystemExit(f"no end of the statement from line {i + 1}")


def delete(names, roots):
    removed = 0
    headers = list(headers_of(roots))
    srcs = [p for p in code_files() if p.suffix != ".h" or p.relative_to(ROOT).as_posix().startswith("tools/")]
    alt = "(" + "|".join(re.escape(n) for n in names) + ")"
    decl = re.compile(r"^\s{4}(?:virtual\s+|static\s+|inline\s+|explicit\s+)*(?:const\s+)?[\w:<>~][\w:<>\*&, ]*?[\s\*&]+"
                      + alt + r"\s*\(")
    defn = re.compile(r"^(?:inline\s+|template\s*<[^>]*>\s*)?(?:const\s+)?[\w:<>\*&, ]*?\b\w+(?:<[^>]*>)?::"
                      + alt + r"\s*\(")
    if True:
        for group, pat, is_header in ((headers, decl, True), (srcs, defn, False)):
            for p in group:
                text = read(p)
                if not any(n in text for n in names):
                    continue
                eol = "\r\n" if "\r\n" in text else "\n"
                lines = text.split(eol)
                changed = False
                i = 0
                while i < len(lines):
                    m = pat.match(lines[i])
                    if m and not lines[i].lstrip().startswith("//"):
                        j = span_end(lines, i)
                        k = i
                        while not is_header and k > 0 and lines[k - 1].strip().startswith("//"):
                            k -= 1
                        print(f"{p.relative_to(ROOT).as_posix()}:{i + 1}-{j + 1} {m.group(1)}")
                        removed += j - k + 1
                        del lines[k:j + 1]
                        if not is_header and 0 < k < len(lines) and not lines[k].strip() and not lines[k - 1].strip():
                            del lines[k]
                        changed = True
                        i = k
                        continue
                    i += 1
                if changed:
                    p.write_bytes(eol.join(lines).encode("utf-8", "surrogateescape"))
    print(f"lines removed: {removed}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("report", help="list the members nothing names")
    d = sub.add_parser("delete", help="delete the named members' declarations and definitions")
    d.add_argument("names", nargs="+")
    for s in (r, d):
        s.add_argument("--headers", nargs="+", default=DEFAULT_HEADERS, help="the header roots to scan")
    r.add_argument("--exclude", nargs="*", default=[], help="names to keep")
    a = ap.parse_args()
    if a.cmd == "report":
        report(a.headers, a.exclude)
    else:
        delete(a.names, a.headers)


if __name__ == "__main__":
    main()
