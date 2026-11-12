"""Point C++ includes of the Rust library's C interface at the crates' own headers (include/rust/<crate>.h).

  python -I tools/rust_includes.py [--dry-run] [FILE ...]

`include/rust/ee_rust.h` includes every crate's header, so a unit that includes it is rebuilt when any crate's C
interface changes. This replaces each `#include "rust/ee_rust.h"` with the headers of the crates whose names the file
uses (a header's names are its top-level declarations; comments and strings are not read). A file that names none
keeps `ee_rust.h` (it may need the names through a macro). Then every file that names a crate's declarations but
gets that crate from no include of its own and none of its includes (followed through the repository's headers)
gets the crate's header after its last include. With no FILE, all tracked C/C++ sources and headers.
Byte-preserving: line endings stay as they are.
"""
import argparse
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
HEADERS = ROOT / "include" / "rust"
UMBRELLA = "ee_rust"
COMMON = "ee_rust_common"
SUFFIXES = (".cpp", ".c", ".cc", ".h", ".hpp", ".inl", ".mm")
INCLUDE = re.compile(rb'^([ \t]*#[ \t]*include[ \t]*)"([^"]+)"(.*?)(\r?\n|$)', re.M)
WORD = re.compile(rb"[A-Za-z_][A-Za-z0-9_]*")
COMMENTS_AND_STRINGS = re.compile(rb'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S)


def chunks(lines):
    """cbindgen's top-level items: blocks between blank lines outside braces."""
    out, cur, depth = [], [], 0
    for line in lines:
        if not line and depth == 0:
            if cur:
                out.append(cur)
                cur = []
            continue
        code = line.split("//")[0]
        depth += code.count("{") - code.count("}")
        cur.append(line)
    if cur:
        out.append(cur)
    return out


def item_name(lines, function):
    code = " ".join(l.split("//")[0] for l in lines if not l.lstrip().startswith("//"))
    if code.lstrip().startswith("extern "):
        return None   # a C function a crate imports (`extern int tolower(int c);`), not one of its own
    if function:
        m = re.search(r"(\w+)\s*\(", code)
        return m.group(1) if m else None
    m = (re.match(r"\s*constexpr static const .*?(\w+)(?:\[\w*\])* = ", code)
         or re.match(r"\s*(?:struct|union|enum class|enum|using) (\w+)", code))
    return m.group(1) if m else None


def header_names(path):
    """The top-level names one generated header declares."""
    lines = path.read_text(encoding="utf-8").split("\n")
    try:
        open_at = lines.index('extern "C" {')
        close_at = lines.index('}  // extern "C"')
    except ValueError:
        open_at = close_at = len(lines)
    start = max(i for i, l in enumerate(lines[:open_at]) if l.startswith(("#include", "#define"))) + 1
    end = open_at if open_at < len(lines) else max(i for i, l in enumerate(lines) if l.startswith("#endif"))
    names = {item_name(c, False) for c in chunks(lines[start:end])}
    names |= {item_name(c, True) for c in chunks(lines[open_at + 1:close_at])}
    names.discard(None)
    return names


def symbol_table():
    """name -> crate header stem, and each header's includes of other generated headers."""
    table, deps = {}, {}
    for path in sorted(HEADERS.glob("*.h")):
        if path.stem == UMBRELLA:
            continue
        for name in header_names(path):
            table[name.encode()] = path.stem
        deps[path.stem] = set(re.findall(r'^#include "(\w+)\.h"', path.read_text(encoding="utf-8"), re.M))
    return table, deps


def closure(stems, deps):
    out, todo = set(), list(stems)
    while todo:
        s = todo.pop()
        if s not in out:
            out.add(s)
            todo.extend(deps.get(s, ()))
    return out


def tracked_sources():
    out = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT, capture_output=True, check=True).stdout
    return [ROOT / p for p in out.decode().split("\0") if p.endswith(SUFFIXES) and not p.startswith("include/rust/")]


def used_crates(data, table, own_declarations=False):
    """The crates whose names a file uses; with own_declarations, less the names it declares itself (a forward
    `struct EeX;`, an `extern "C"` prototype): such a file avoids the header on purpose."""
    code = COMMENTS_AND_STRINGS.sub(b" ", data)
    words = {w for w in set(WORD.findall(code)) if w in table}
    if own_declarations:
        words = {w for w in words
                 if not re.search(rb"\bstruct\s+" + w + rb"\s*;", code)
                 and not re.search(rb'extern\s+"C"[^;{]*\b' + w + rb"\s*\(", data)}
    return {table[w] for w in words}


def insert_at(text):
    """Where a file's new includes go: after its last include outside any `#if`, else after its include guard or
    `#pragma once`, else at the top. None: nowhere safe (the caller reports the file)."""
    directive = re.compile(rb"[ \t]*#[ \t]*(\w+)[ \t]*(\S*)")
    pos, depth, base, guard, after_guard, last = 0, 0, 0, None, None, None
    first_directive = True
    for line in text.splitlines(keepends=True):
        end = pos + len(line)
        m = directive.match(line)
        if m:
            word, arg = m.group(1), m.group(2)
            if first_directive and word == b"ifndef":
                guard = arg
            elif guard is not None and depth == 1 and word == b"define" and arg == guard and after_guard is None:
                base, after_guard = 1, end
            first_directive = False
            if word in (b"if", b"ifdef", b"ifndef"):
                depth += 1
            elif word == b"endif":
                depth -= 1
            elif word == b"pragma" and arg == b"once" and after_guard is None:
                after_guard = end
            elif word == b"include" and depth == base:
                last = end
        pos = end
    if last is not None:
        return last if text[last - 1:last] == b"\n" else None
    return after_guard if after_guard is not None else 0


def rust_include(target):
    m = re.fullmatch(r"(?:.*/)?rust/(\w+)\.h", target)
    return m.group(1) if m else None


class Resolver:
    """`#include "x/y.h"` to repository files: the including file's directory first, else by path suffix."""

    def __init__(self, files):
        self.by_name = {}
        for f in files:
            self.by_name.setdefault(f.name.lower(), []).append(f)

    def resolve(self, includer, target):
        local = (includer.parent / target)
        if local.is_file():
            return [local.resolve()]
        tail = target.replace("\\", "/").lower()
        found = [f for f in self.by_name.get(tail.rsplit("/", 1)[-1], [])
                 if f.as_posix().lower().endswith("/" + tail)]
        return [f.resolve() for f in found]


def replacement(stems, prefix, rest, eol):
    """The include lines for a set of crate headers; the old line's trailing comment stays on the first."""
    lines = []
    for i, stem in enumerate(sorted(stems)):
        lines.append(prefix + b'"rust/' + stem.encode() + b'.h"' + (rest if i == 0 else b"") + eol)
    return b"".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("files", nargs="*", type=pathlib.Path)
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    table, deps = symbol_table()
    every = [f.resolve() for f in tracked_sources()]
    targets = [f.resolve() for f in a.files] if a.files else every
    data = {f: f.read_bytes() for f in every}
    for f in targets:
        data.setdefault(f, f.read_bytes())

    # 1. the umbrella's includes, narrowed
    switched, kept = [], []
    new = dict(data)
    for f in targets:
        text = data[f]

        def narrow(m):
            if rust_include(m.group(2).decode()) != UMBRELLA:
                return m.group(0)
            crates = used_crates(text, table)
            if not crates:
                kept.append(f)
                return m.group(0)
            switched.append(f)
            return replacement(crates, m.group(1), m.group(3), m.group(4) or b"\n")

        new[f] = INCLUDE.sub(narrow, text)

    # 2. what each file gets through its includes; a file short of a crate it names gets that crate's header
    resolver = Resolver(every)
    direct, edges = {}, {}
    for f, text in new.items():
        stems, files = set(), []
        for m in INCLUDE.finditer(text):
            target = m.group(2).decode()
            stem = rust_include(target)
            if stem is not None:
                stems |= set(table.values()) if stem == UMBRELLA else closure({stem}, deps)
            else:
                files.extend(resolver.resolve(f, target))
        direct[f], edges[f] = stems, files

    def provided(f):
        got, seen, todo = set(), {f}, [f]
        while todo:
            g = todo.pop()
            got |= direct.get(g, set())
            for h in edges.get(g, ()):
                if h not in seen:
                    seen.add(h)
                    todo.append(h)
        return got

    added = []
    for f in targets:
        missing = used_crates(new[f], table, own_declarations=True) - provided(f) - {COMMON}
        if not missing:
            continue
        at = insert_at(new[f])
        if at is None:
            print(f"{f.relative_to(ROOT)}: names {sorted(missing)}; its last line is an include", file=sys.stderr)
            continue
        eol = b"\r\n" if b"\r\n" in new[f] else b"\n"
        new[f] = new[f][:at] + replacement(missing, b"#include ", b"", eol) + new[f][at:]
        added.append((f, sorted(missing)))

    for f in targets:
        if new[f] != data[f] and not a.dry_run:
            f.write_bytes(new[f])
    rel = lambda f: f.relative_to(ROOT).as_posix()
    print(f"switched {len(switched)} includes of rust/ee_rust.h to the crates' headers")
    for f in kept:
        print(f"  kept rust/ee_rust.h (names no declaration): {rel(f)}")
    print(f"added crate headers to {len(added)} files that had them only through other includes")
    for f, stems in added:
        print(f"  {rel(f)}: {', '.join(stems)}")
    umbrella = [rel(f) for f in every if any(rust_include(m.group(2).decode()) == UMBRELLA
                                              for m in INCLUDE.finditer(new[f]))]
    print(f"{len(umbrella)} files include rust/ee_rust.h")
    return 0


if __name__ == "__main__":
    sys.exit(main())
