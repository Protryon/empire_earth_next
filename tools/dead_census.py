"""Dead-code census of the first-party C++: the functions no build keeps.

lld-link /OPT:REF drops every function that no live code reaches (clang-cl /O1 implies /Gy: one section per
function), and its /MAP lists every symbol it kept (ICF-folded copies included, at their leader's address). A text
symbol a first-party object defines that none of the given link maps lists is dead in every one of those builds.

    python -I tools/dead_census.py [--build build/clang --map exe/EE-AOC.map ...] [--out docs/dead_census]

The default maps are the three Windows links: build/clang/exe/EE-AOC.map, build/clang/exe/EE-SIM.map (build.py
--sim) and build/clang-test/exe/EE-AOC.map (--test-profile windowed-software); the objects are build/clang/obj's.
Code only the POSIX builds compile (#if !defined(_WIN32) ...) is in none of these objects, so a function only such
code calls looks dead here: grep every candidate over src, include, tests, tools and rust before deleting it.

Per module (exe, lle, the plug-ins) it writes <out>/<module>.txt: per source file the functions it defines, how many
no map keeps, and the lines of the dead ones' definitions found in the file (a brace match from `Class::Name(`), then
per class the dead functions by name. Out of scope: src/ThirdParty, WONAPI (the WON lanes') and include/ee/wonapi.
Compiler-generated symbols (deleting destructors, dynamic initializers, thunks) are left out of the counts.
"""
import argparse
import collections
import pathlib
import re
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import ee   # noqa: E402

ROOT = ee.ROOT
NM = ee.LLVM / "llvm-nm.exe" if (ee.LLVM / "llvm-nm.exe").exists() else ee.LLVM / "llvm-nm"
MODULES = ["exe", "lle", "default", "tcp", "mixer", "dx7hrtnl"]   # exeimg and jpeg are third-party code
MAP_ROW = re.compile(r"^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)")
GENERATED = ("??_G", "??_E", "??__E", "??__F", "??_R", "??_7", "??_8", "?dtor$", "?catch$", "?fin$", "$")


def first_party(rel):
    s = rel.as_posix()
    return not s.startswith("ThirdParty/") and "/WONAPI/" not in s and not s.startswith("WONAPI/")


def kept_symbols(maps):
    kept = set()
    for m in maps:
        for line in m.read_text(errors="replace").splitlines():
            r = MAP_ROW.match(line)
            if r:
                kept.add(r.group(1))
    return kept


def text_symbols(obj):
    """[(mangled, demangled)] of the functions an object defines (llvm-nm, and llvm-nm -C in the same order)."""
    rows = []
    for flags in ([], ["-C"]):
        out = subprocess.run([str(NM), "--defined-only", *flags, str(obj)], capture_output=True, text=True).stdout
        rows.append([l.split(None, 2) for l in out.splitlines()])
    syms = []
    for raw, dem in zip(*rows):
        if len(raw) >= 3 and raw[1] in ("T", "t") and not raw[2].startswith(GENERATED):
            syms.append((raw[2], dem[2] if len(dem) >= 3 else raw[2]))
    return syms


def qualified_name(dem):
    """'public: virtual long __cdecl A::B<C>::F(void)' -> 'A::B<C>::F'; a C name stays."""
    depth, cut = 0, len(dem)
    for i, c in enumerate(dem):
        if c == "<":
            depth += 1
        elif c == ">":
            depth -= 1
        elif c == "(" and depth == 0:
            cut = i
            break
    head = dem[:cut].strip()
    depth, start = 0, 0
    for i, c in enumerate(head):
        if c == "<":
            depth += 1
        elif c == ">":
            depth -= 1
        elif c == " " and depth == 0:
            start = i + 1
    name = head[start:]
    return name.replace("operator", "operator ") if name.endswith("operator") else name


def split_class(qname):
    depth, last = 0, -1
    for i in range(len(qname) - 1):
        c = qname[i]
        if c == "<":
            depth += 1
        elif c == ">":
            depth -= 1
        elif c == ":" and qname[i + 1] == ":" and depth == 0:
            last = i
    return (qname[:last], qname[last + 2:]) if last >= 0 else ("", qname)


def strip_comments(text):
    """The text with comments and string literals blanked (newlines kept, so offsets and lines stay)."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join(ch if ch == "\n" else " " for ch in text[i:j]))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(c + " " * (j - i - 2) + c if j - i >= 2 else text[i:j])
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def definitions(code, cls, name):
    """[(first line, last line)] of every definition of cls::name (or of the free function name) in code."""
    base = name.split("<")[0]
    if base.startswith("operator"):
        return []
    short = split_class(cls)[1].split("<")[0] if cls else ""
    pat = (re.escape(short) + r"\s*(?:<[^;{}()]*>)?\s*::\s*" if short else r"(?<![\w:.>])") + re.escape(base) + r"\s*\("
    spans = []
    for m in re.finditer(pat, code):
        if not short:   # a free function: the name starts a definition, not a call (a type or * & before it)
            pre = code[max(0, m.start() - 80):m.start()].rstrip()
            if not pre or pre[-1] in ";{}=(,!&|+-?:<" and not pre.endswith(("*", "&")) or pre.endswith(("return", "new")):
                if not (pre and (pre[-1].isalnum() or pre[-1] in "*&_")):
                    continue
        depth, i = 0, m.end() - 1
        while i < len(code):
            if code[i] == "(":
                depth += 1
            elif code[i] == ")":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        j = i + 1
        while j < len(code) and code[j] not in "{;":
            j += 1
        if j >= len(code) or code[j] == ";":
            continue
        depth, k = 0, j
        while k < len(code):
            if code[k] == "{":
                depth += 1
            elif code[k] == "}":
                depth -= 1
                if depth == 0:
                    break
            k += 1
        line0 = code.count("\n", 0, m.start()) + 1
        # the return type's line, when the name starts a line of its own
        ls = code.rfind("\n", 0, m.start()) + 1
        if not code[ls:m.start()].strip() and ls > 0:
            prev = code.rfind("\n", 0, ls - 1) + 1
            if code[prev:ls].strip() and not code[prev:ls].strip().endswith((";", "}", "{")):
                line0 -= 1
        spans.append((line0, code.count("\n", 0, k) + 1))
    return spans


def _rel(p):
    try:
        return p.relative_to(ROOT).as_posix()
    except ValueError:
        return p.as_posix()


def census(build, maps, out, modules):
    kept = kept_symbols(maps)
    objdir = build / "obj"
    units = []   # (module, source, [symbols])
    dem = {}
    defined = collections.Counter()
    for mod in modules:
        for obj in sorted((objdir / mod).rglob("*.obj")):
            rel = obj.relative_to(objdir / mod).with_suffix("")
            srcs = [ROOT / "src" / rel.with_suffix(s) for s in (".cpp", ".c")]
            src = next((s for s in srcs if s.exists()), None)
            if src is None or not first_party(rel):
                continue
            syms = text_symbols(obj)
            dem.update(syms)
            syms = [m for m, _ in syms]
            defined.update(set(syms))
            units.append((mod, src, syms))
    out.mkdir(parents=True, exist_ok=True)
    total = collections.Counter()
    for mod in modules:
        rows, classes = [], collections.defaultdict(list)
        for m, src, syms in units:
            if m != mod:
                continue
            own = [s for s in syms if defined[s] == 1]   # inline and template copies are counted where defined
            dead = [s for s in own if s not in kept]
            code = strip_comments(src.read_text(errors="replace"))
            nlines = code.count("\n") + 1
            lines, found = set(), 0
            seen = set()
            for s in dead:
                q = qualified_name(dem[s])
                cls, name = split_class(q)
                classes[cls or "(free functions)"].append((src, name))
                if q in seen:
                    continue
                seen.add(q)
                spans = definitions(code, cls, name)
                # overloads: count the lines only when every overload of the name is dead
                live_overloads = any(qualified_name(dem[k]) == q for k in own if k in kept)
                if spans and not live_overloads:
                    found += 1
                    for a, b in spans:
                        lines.update(range(a, b + 1))
            rows.append((src, nlines, len(own), len(dead), len(lines)))
        rows.sort(key=lambda r: (-r[4], -r[3]))
        dead_lines = sum(r[4] for r in rows)
        total.update(files=len(rows), lines=sum(r[1] for r in rows), dead=dead_lines,
                     funcs=sum(r[2] for r in rows), deadfuncs=sum(r[3] for r in rows))
        with open(out / f"{mod}.txt", "w", newline="\n") as f:
            f.write(f"# dead-code census of module {mod} (tools/dead_census.py)\n")
            f.write(f"# maps: {', '.join(_rel(m) for m in maps)}\n")
            f.write(f"# {len(rows)} units, {sum(r[1] for r in rows)} lines; {sum(r[3] for r in rows)} of "
                    f"{sum(r[2] for r in rows)} functions no map keeps; {dead_lines} lines in their definitions\n")
            f.write("# columns: dead lines, unit lines, dead/defined functions, unit ('ALL' = no function kept)\n\n")
            for src, nlines, nown, ndead, nl in rows:
                if ndead:
                    tag = " ALL" if ndead == nown else ""
                    f.write(f"{nl:6d} {nlines:6d} {ndead:4d}/{nown:<4d} {src.relative_to(ROOT / 'src').as_posix()}{tag}\n")
            f.write("\n# dead functions by class\n")
            for cls in sorted(classes, key=lambda c: (-len(classes[c]), c)):
                names = sorted({n for _, n in classes[cls]})
                f.write(f"\n{cls} ({len(classes[cls])}): {', '.join(names)}\n")
        print(f"{mod:9} {len(rows):4d} units {sum(r[1] for r in rows):7d} lines  "
              f"{sum(r[3] for r in rows):5d}/{sum(r[2] for r in rows):<6d} functions dead  {dead_lines:6d} dead lines")
    # inline functions and template instances: emitted by every unit that uses them, dead when no map keeps any copy
    shared = collections.defaultdict(set)
    where = collections.defaultdict(set)
    for m, src, syms in units:
        for s in syms:
            if defined[s] > 1 and s not in kept:
                q = qualified_name(dem[s])
                shared[split_class(q)[0] or "(free functions)"].add(split_class(q)[1])
                where[s].add(src.relative_to(ROOT / "src").as_posix())
    with open(out / "inline.txt", "w", newline="\n") as f:
        f.write("# inline functions and template instances that first-party units emit and no map keeps "
                "(tools/dead_census.py)\n")
        f.write(f"# {len(where)} symbols; the class, then its dead members\n")
        for cls in sorted(shared, key=lambda c: (-len(shared[c]), c)):
            f.write(f"\n{cls} ({len(shared[cls])}): {', '.join(sorted(shared[cls]))}\n")
    scratch = ROOT / "build" / "dead_census"
    scratch.mkdir(parents=True, exist_ok=True)
    with open(scratch / "dead.tsv", "w", newline="\n") as f:
        for m, src, syms in units:
            for s in syms:
                if s not in kept:
                    f.write(f"{m}\t{src.relative_to(ROOT / 'src').as_posix()}\t{defined[s]}\t{s}\t{dem[s]}\n")
    print(f"{'total':9} {total['files']:4d} units {total['lines']:7d} lines  {total['deadfuncs']:5d}/"
          f"{total['funcs']:<6d} functions dead  {total['dead']:6d} dead lines -> {out}")


def first_party_lines():
    """The first-party C++ line count: src (but ThirdParty and WONAPI) and include (but the generated rust headers,
    the third-party copies and include/ee/wonapi)."""
    n = 0
    skip_inc = {"rust", "miniaudio", "pl_mpeg", "jpeglib"}
    for base, skip in ((ROOT / "src", {"ThirdParty"}), (ROOT / "include", skip_inc)):
        for p in base.rglob("*"):
            if p.suffix not in (".cpp", ".c", ".h", ".inl", ".hpp") or not p.is_file():
                continue
            rel = p.relative_to(base).parts
            if rel[0] in skip or "WONAPI" in rel or "wonapi" in rel:
                continue
            n += p.read_bytes().count(b"\n")
    return n


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--build", type=pathlib.Path, default=ROOT / "build" / "clang")
    ap.add_argument("--map", type=pathlib.Path, action="append",
                    help="a link map (default: EE-AOC and EE-SIM of --build, the test profile's EE-AOC)")
    ap.add_argument("--out", type=pathlib.Path, default=ROOT / "docs" / "dead_census")
    ap.add_argument("--module", action="append", choices=MODULES)
    ap.add_argument("--lines", action="store_true", help="print the first-party C++ line count and stop")
    a = ap.parse_args()
    if a.lines:
        print(first_party_lines())
        return 0
    maps = a.map or [a.build / "exe" / "EE-AOC.map", a.build / "exe" / "EE-SIM.map",
                     ROOT / "build" / "clang-test" / "exe" / "EE-AOC.map"]
    for m in maps:
        if not m.exists():
            sys.exit(f"{m} is missing: build.py -j 3 --sim and --test-profile windowed-software first")
    census(a.build, [m.resolve() for m in maps], a.out, a.module or MODULES)
    print(f"first-party C++ lines: {first_party_lines()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
