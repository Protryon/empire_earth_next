"""Dependency census of the C++ that links into the game: which units call only Rust, the OS/CRT or Lua.

    python -I tools/cpp_deps.py [report] [--build build/clang] [--out build/cpp_deps] [--top N] [--code-only] [--all-refs]
    python -I tools/cpp_deps.py unit FILE [--build ...]      # one unit's outgoing and incoming edges, with symbols
    python -I tools/cpp_deps.py selftest                     # compiles tiny objects with clang-cl and checks the graph

Build first (`python -I build.py -j 3 --sim`). The objects are the ones the Windows links name:
build/clang/exe/link.rsp (EE-AOC.exe) and link-EE-SIM.rsp (EE-SIM.exe), their union. `llvm-nm` (ee.LLVM) lists each
object's defined and undefined symbols, `llvm-readobj --symbols --relocations` its sections and relocations (both
cached in <out>/nm_cache.json by path, size and mtime). An undefined symbol resolves, as lld-link would, first to the
objects, then to ee_game.lib (the Rust library), else to the system (MSVC STL/CRT, Windows import libraries, SDL3).
A UNIT is one object and its source file (FILE: its path under src/, its file name or its stem).

Definers are classified as:
    cpp       a first-party unit (an object of src/, but src/ThirdParty);
    vendored  src/ThirdParty (Lua);
    rust      ee_game.lib's external definitions;
    system    anything else, plus every symbol of namespace std / the CRT (`std::`, `__std`, `_Init_thread`, ...) and the
              compiler's constant-pool and literal COMDATs (`__real@`, `__xmm@`, `??_C@`): an object that emits its own
              copy of an STL template still depends only on the STL for it.

What a unit needs: the objects hold far more than their own code. An object carries a COMDAT copy of every inline
member, template, vtable and RTTI record its headers make it emit (UString.obj, 73 source lines, holds 378 COMDAT
sections: the inline constructors of GE2DPoint, FSFile, URandom, ... from lle/LowLevelEngine.h). So the undefined
symbols counted are only those the unit's own code REACHES: from the sections the object owns (selection none or
NoDuplicates: its out-of-line functions, its data, its initialisers), through the relocations, into the COMDAT copies
they use (and those copies' unwind data), to the undefined symbols. An inline the unit calls is compiled from the
header into the unit, so its calls are the unit's (an inline getter in B's header calling B's out-of-line member is
an edge A -> B, which is right: moving A needs it); an inline it never calls is not the unit's code. `--all-refs`
counts every undefined symbol of the object instead (more edges, mostly false).

Where a symbol lives (the COMDAT attribution choice): a symbol defined in exactly one object belongs to that object.
A symbol defined in several objects (a COMDAT) belongs to its class's *home unit*, the unit that defines the most
single-definer functions of that class (its out-of-line members); a COMDAT whose class has no home (a header-only
class, a free inline function) is "inline": the referencing unit compiles it itself, so it is no edge.

An edge A -> B means unit A's code reaches an undefined symbol that resolves to C++ unit B (weight: distinct symbols).
References to B's data (a global, a vtable, RTTI) count: moving A to Rust needs an accessor for them. `--code-only`
counts only functions (the data references show as `data=` in the reports): the difference is mostly the globals
of Empire Earth.cpp (gWorld, gPhysics, gUIManager, ...). Calls through a vtable or a function pointer are invisible
(only the vtable's reference at construction shows), so a leaf can still call C++ virtually through its arguments.
The class view groups the units by their primary class (the class most of their single-definer functions belong to;
a unit of free functions is "(file) <stem>") and contracts the graph.

Reports (under --out, default build/cpp_deps; a summary on stdout):
    leaves.txt         units with no outgoing C++ edge (they can move whole), lines, and what they call instead
    near_leaves.txt    units with 1-3 C++ targets, each target with its symbols
    blockers.txt       C++ units ranked by the lines of the units that have them as a target: `all` (every unit that
                       names it, the ranking), `near` (units with at most 3 targets), `sole` (its only target)
    sccs.txt           the strongly connected components (cycles) with members and lines: clusters that move together
                       (or are cut); a CLUSTER LEAF calls nothing outside itself
    waves.txt          the move order bottom-up over the condensation (a cycle is one node): wave 1 calls no other C++,
                       each later wave only earlier waves
    keystones.txt      greedy, inside the biggest cycle: the unit whose move (its own C++ calls becoming host calls)
                       takes the most lines out of the cycle, then the next; each with the C++ symbols it would host
    classes.txt        the class view: per class its units, lines (+ its header's), outgoing classes
    dirs.txt           a per-directory roll-up
    edges.tsv          every edge: from, to, weight, symbols (demangled)
    summary.json       the numbers
Lines are the unit's source file (.cpp/.c) lines; the class view adds the class's header (include/ or src/) once.
Code only the POSIX builds compile is in none of these objects.
"""
import argparse
import collections
import concurrent.futures
import json
import os
import pathlib
import re
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import ee   # noqa: E402

ROOT = ee.ROOT
NM = ee.LLVM / "llvm-nm.exe" if (ee.LLVM / "llvm-nm.exe").exists() else ee.LLVM / "llvm-nm"
CLANG = ee.LLVM / "clang-cl.exe" if (ee.LLVM / "clang-cl.exe").exists() else ee.LLVM / "clang-cl"
RSPS = ["link.rsp", "link-EE-SIM.rsp"]
SRC_EXT = (".cpp", ".c", ".cc", ".mm")
# names that are never a first-party dependency: constant pools, string literals, CRT/compiler helpers
SYSTEM_PREFIX = ("__real@", "__xmm@", "__ymm@", "__zmm@", "??_C@", "_fltused", "__security_", "__chkstk", "_tls_",
                 "__guard", "__CxxFrameHandler", "_CxxThrowException", "__std_", "_Init_thread", "__imp_", "_purecall",
                 "__RTDynamicCast", "__report_", "_RTC_", "__C_specific_handler", "$", ".")
SYSTEM_DEM = ("std::", "class std::", "struct std::", "__std", "Concurrency::", "type_info", "`string'",
              "class type_info", "_Init_thread")
QUAL_SKIP = ("`anonymous namespace'", "<lambda", "`")


# ---------------------------------------------------------------- symbol names

def qualified_name(dem):
    """'public: virtual long __cdecl A::B<C>::F(void)' -> 'A::B<C>::F'; 'const A::`vftable'' -> 'A::`vftable''."""
    depth, cut = 0, len(dem)
    for i, c in enumerate(dem):
        if c in "<":
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
        elif c == " " and depth == 0 and not head[i + 1:i + 2] in ("<", ">"):
            start = i + 1
    return head[start:]


def class_of(dem):
    """The class (or namespace) part of a demangled name; '' for a free function or C name."""
    q = qualified_name(dem)
    for marker in ("::`vftable'", "::`vbtable'", "::`RTTI"):
        if marker in q:
            return q.split(marker)[0]
    if dem.startswith("struct `RTTI Type Descriptor'") or "`RTTI" in dem:
        m = re.search(r"(?:class|struct) ([\w:<>, ]+?)(?: `| \w+ )?`?RTTI", dem)
        return m.group(1).strip() if m else ""
    depth, last = 0, -1
    for i in range(len(q) - 1):
        c = q[i]
        if c == "<":
            depth += 1
        elif c == ">":
            depth -= 1
        elif c == ":" and q[i + 1] == ":" and depth == 0:
            last = i
    cls = q[:last] if last >= 0 else ""
    return re.sub(r"<.*", "", cls)   # a template's instances share their class


def is_system(raw, dem):
    if raw.startswith(SYSTEM_PREFIX):
        return True
    q = qualified_name(dem)
    return q.startswith(SYSTEM_DEM)


# ---------------------------------------------------------------- objects

def link_objects(build):
    """[(object path, module)] in link order: the union of the Windows links' response files."""
    seen, out = set(), []
    exe = build / "exe"
    for rsp in RSPS:
        p = exe / rsp
        if not p.exists():
            continue
        for line in p.read_text(errors="replace").splitlines():
            line = line.strip().strip('"')
            if line.lower().endswith(".obj"):
                obj = (exe / line).resolve()
                if obj not in seen:
                    seen.add(obj)
                    out.append(obj)
    if not out:
        sys.exit(f"no objects: build first (python -I build.py -j 3 --sim); looked for {exe}/{RSPS}")
    return out


def rust_lib(build):
    for rsp in RSPS:
        p = build / "exe" / rsp
        if p.exists():
            for line in p.read_text(errors="replace").splitlines():
                line = line.strip().strip('"')
                if line.lower().endswith("ee_game.lib"):
                    return (build / "exe" / line).resolve()
    return None


def source_of(obj, build):
    """(module, path relative to src/ without suffix) of an object under <build>/obj/<module>/."""
    rel = obj.relative_to((build / "obj").resolve())
    return rel.parts[0], pathlib.PurePosixPath(*rel.parts[1:]).with_suffix("")


def nm(path, defined_only=False):
    """[(kind, mangled, demangled)] for one object or library."""
    rows = []
    for flags in ([], ["-C"]):
        args = [str(NM), "--no-sort", *flags]
        if defined_only:
            args += ["--defined-only", "--extern-only"]
        out = subprocess.run(args + [str(path)], capture_output=True, text=True, errors="replace").stdout
        rows.append([l.split(None, 2) if l[:1] != " " else ["", *l.split(None, 1)] for l in out.splitlines()])
    syms = []
    for raw, dem in zip(*rows):
        if len(raw) == 3 and len(dem) == 3:
            syms.append((raw[1], raw[2], dem[2]))
    return syms


READOBJ = ee.LLVM / "llvm-readobj.exe" if (ee.LLVM / "llvm-readobj.exe").exists() else ee.LLVM / "llvm-readobj"
ROOT_SELECTIONS = ("0x0", "NoDuplicates")   # a section the object owns; Any/SameSize/ExactMatch/Largest: a COMDAT


def reachable_refs(path):
    """The undefined symbols an object's own code reaches: from its own sections (not COMDAT: selection none or
    NoDuplicates), through the relocations, into the COMDAT copies it uses (and their associated unwind data),
    to the undefined symbols. A COMDAT copy nothing of the object reaches (an inline member a header emits in
    every object, e.g. a dllexport class's) is not the unit's code, so its calls are not the unit's."""
    out = subprocess.run([str(READOBJ), "--symbols", "--relocations", str(path)], capture_output=True, text=True,
                         errors="replace").stdout
    sym_sec, undef, sel, assoc, names = {}, set(), {}, collections.defaultdict(list), {}
    relocs = collections.defaultdict(list)
    idx = 0
    cur = None
    in_rel = None
    for line in out.splitlines():
        t = line.strip()
        if t == "Symbol {":
            cur = {"aux": 0}
            continue
        if cur is not None:
            if t.startswith("Name: ") and "name" not in cur:
                cur["name"] = t[6:]
            elif t.startswith("Section: ") and "sec" not in cur:
                m = re.search(r"\((-?\d+)\)$", t)
                cur["sec"] = int(m.group(1)) if m else 0
            elif t.startswith("AuxSymbolCount: "):
                cur["aux"] = int(t.split()[1])
            elif t.startswith("Number: "):
                cur["num"] = int(t.split()[1])
            elif t.startswith("Selection: "):
                cur["sel"] = t.split()[1]
            elif t == "}" and line.startswith("  }"):
                sec = cur.get("sec", 0)
                if sec > 0:
                    sym_sec[idx] = sec
                    if "sel" in cur and cur.get("name", "").startswith("."):
                        sel.setdefault(sec, cur["sel"])
                        if cur["sel"] == "Associative" and cur.get("num"):
                            assoc[cur["num"]].append(sec)
                elif sec == 0:
                    undef.add(idx)
                cur["idx"] = idx
                names[idx] = cur.get("name", "")
                idx += 1 + cur["aux"]
                cur = None
            continue
        m = re.match(r"^\s*Section \((\d+)\) ", line)
        if m:
            in_rel = int(m.group(1))
            continue
        if in_rel is not None and t.startswith("0x"):
            m = re.search(r"\((\d+)\)$", t)
            if m:
                relocs[in_rel].append(int(m.group(1)))
        elif t == "}":
            in_rel = None
    secs = set(relocs) | set(sel) | set(sym_sec.values())
    assoc_child = {c for cs in assoc.values() for c in cs}
    roots = [sc for sc in secs if sel.get(sc, "0x0") in ROOT_SELECTIONS and sc not in assoc_child]
    seen, work, refs = set(roots), list(roots), set()
    while work:
        sc = work.pop()
        for c in assoc.get(sc, ()):
            if c not in seen:
                seen.add(c)
                work.append(c)
        for si in relocs.get(sc, ()):
            if si in undef:
                refs.add(names[si])
            elif si in sym_sec and sym_sec[si] not in seen:
                seen.add(sym_sec[si])
                work.append(sym_sec[si])
    return sorted(refs)


class NmCache:
    def __init__(self, path):
        self.path = path
        try:
            self.data = json.loads(path.read_text())
        except (OSError, ValueError):
            self.data = {}
        self.dirty = False

    def get(self, p, defined_only=False, what="nm"):
        st = p.stat()
        key = f"{p}|{what}"
        stamp = [st.st_size, st.st_mtime_ns, defined_only]
        hit = self.data.get(key)
        if hit and hit[0] == stamp:
            return hit[1]
        syms = reachable_refs(p) if what == "reach" else nm(p, defined_only)
        self.data[key] = [stamp, syms]
        self.dirty = True
        return syms

    def save(self):
        if self.dirty:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            tmp = self.path.with_suffix(".tmp")
            tmp.write_text(json.dumps(self.data))
            os.replace(tmp, self.path)


# ---------------------------------------------------------------- the graph

class Unit:
    def __init__(self, name, kind, lines, src=None):
        self.name, self.kind, self.lines, self.src = name, kind, lines, src
        self.defs = []       # (mangled, demangled, kind letter)
        self.undef = []      # (mangled, demangled)
        self.cls = ""


def count_lines(p):
    try:
        return p.read_bytes().count(b"\n") + 1
    except OSError:
        return 0


def analyse(units, rust_defs, code_only=False):
    """units: [Unit] with defs/undef filled; rust_defs: set of mangled names. Returns the graph. code_only: a
    reference to a C++ unit's data (a global, a vtable, RTTI) is no edge (category "data")."""
    definers = collections.defaultdict(list)
    def_kind = {}
    for u in units:
        for raw, dem, k in u.defs:
            definers[raw].append(u)
            def_kind.setdefault(raw, k)
    # primary class and class homes from single-definer functions
    class_count = collections.defaultdict(collections.Counter)    # class -> unit name -> functions
    for u in units:
        if u.kind != "cpp":
            continue
        c = collections.Counter()
        for raw, dem, k in u.defs:
            if k in "Tt" and len(definers[raw]) == 1 and not is_system(raw, dem):
                cls = class_of(dem)
                if cls and not cls.startswith(QUAL_SKIP):
                    c[cls] += 1
                    class_count[cls][u.name] += 1
        u.cls = c.most_common(1)[0][0] if c else "(file) " + pathlib.PurePosixPath(u.name).stem
    home = {cls: max(cnt.items(), key=lambda kv: (kv[1], kv[0]))[0] for cls, cnt in class_count.items()}
    by_name = {u.name: u for u in units}

    def resolve(raw, dem):
        """(category, unit name or None)."""
        if is_system(raw, dem):
            return "system", None
        ds = definers.get(raw)
        if ds:
            if len(ds) == 1:
                return ds[0].kind, ds[0].name
            h = home.get(class_of(dem))
            if h:
                return by_name[h].kind, h
            if all(d.kind == ds[0].kind for d in ds) and ds[0].kind != "cpp":
                return ds[0].kind, ds[0].name
            return "inline", None
        if raw in rust_defs:
            return "rust", None
        return "system", None

    edges = collections.defaultdict(dict)        # a -> b -> {mangled: demangled}
    other = collections.defaultdict(collections.Counter)    # a -> category -> symbols
    for u in units:
        if u.kind != "cpp":
            continue
        for raw, dem in u.undef:
            cat, target = resolve(raw, dem)
            if cat == "cpp" and code_only and def_kind.get(raw, "T") not in "Tt":
                other[u.name]["data"] += 1
            elif cat == "cpp" and target != u.name:
                edges[u.name].setdefault(target, {})[raw] = dem
            elif cat != "cpp":
                other[u.name][cat] += 1
    return {"units": by_name, "edges": edges, "other": other, "home": home, "resolve": resolve}


def tarjan(nodes, succ):
    index, low, on, stack, out = {}, {}, set(), [], []
    counter = [0]
    for root in nodes:
        if root in index:
            continue
        work = [(root, iter(sorted(succ.get(root, ()))))]
        index[root] = low[root] = counter[0]
        counter[0] += 1
        stack.append(root)
        on.add(root)
        while work:
            v, it = work[-1]
            advanced = False
            for w in it:
                if w not in index:
                    index[w] = low[w] = counter[0]
                    counter[0] += 1
                    stack.append(w)
                    on.add(w)
                    work.append((w, iter(sorted(succ.get(w, ())))))
                    advanced = True
                    break
                if w in on:
                    low[v] = min(low[v], index[w])
            if advanced:
                continue
            work.pop()
            if work:
                low[work[-1][0]] = min(low[work[-1][0]], low[v])
            if low[v] == index[v]:
                comp = []
                while True:
                    w = stack.pop()
                    on.discard(w)
                    comp.append(w)
                    if w == v:
                        break
                out.append(sorted(comp))
    return out


def waves(nodes, succ, pred, removed=()):
    """Peel the graph from the bottom: wave 1 is the leaves; moving a wave frees the units whose every C++ target has
    moved. Returns ([wave, ...], stuck): the stuck units are in a cycle or depend on one."""
    removed = set(removed)
    left = {n: sum(1 for b in succ.get(n, ()) if b not in removed) for n in nodes if n not in removed}
    layer = sorted(n for n, k in left.items() if k == 0)
    out, done = [], set()
    while layer:
        out.append(layer)
        done.update(layer)
        nxt = set()
        for b in layer:
            for a in pred.get(b, ()):
                if a in left and a not in done:
                    left[a] -= 1
                    if left[a] == 0:
                        nxt.add(a)
        layer = sorted(nxt)
    return out, sorted(n for n in left if n not in done)


def cluster_waves(nodes, succ, comps):
    """Waves over the condensation (each SCC one node): wave 1 is the units and cycles that call no other C++;
    each later wave calls only earlier waves. [[component, ...], ...] (a component: a sorted list of units)."""
    comp_of = {n: i for i, c in enumerate(comps) for n in c}
    csucc = collections.defaultdict(set)
    cpred = collections.defaultdict(set)
    for n in nodes:
        for b in succ.get(n, ()):
            if comp_of[b] != comp_of[n]:
                csucc[comp_of[n]].add(comp_of[b])
                cpred[comp_of[b]].add(comp_of[n])
    left = {i: len(csucc[i]) for i in range(len(comps))}
    layer = sorted(i for i, k in left.items() if k == 0)
    out = []
    while layer:
        out.append([comps[i] for i in layer])
        nxt = set()
        for b in layer:
            for a in cpred[b]:
                left[a] -= 1
                if left[a] == 0:
                    nxt.add(a)
        layer = sorted(nxt)
    return out


def keystones(nodes, succ, pred, lines, steps=20, candidates=60):
    """Greedy, inside the biggest cycle: the unit whose move (to Rust, its own C++ calls becoming host calls) takes the
    most lines out of that cycle (they become orderable units or smaller cycles); repeated.
    Returns ([(unit, lines taken out, [units taken out])], the cycle left)."""
    comps = tarjan(nodes, succ)
    core = set(max(comps, key=lambda c: sum(lines(n) for n in c)))
    out = []
    for _ in range(steps):
        if len(core) < 2:
            break
        sub = {n: {b for b in succ.get(n, ()) if b in core} for n in core}
        cand = sorted(core, key=lambda n: (-sum(lines(a) for a in pred.get(n, ()) if a in core), n))[:candidates]
        best = None
        for c in cand:
            rest = core - {c}
            cs = tarjan(sorted(rest), {n: sub[n] - {c} for n in rest})
            cs = [x for x in cs if len(x) > 1]
            big = set(max(cs, key=lambda x: sum(lines(n) for n in x))) if cs else set()
            freed = sorted(rest - big)
            score = sum(lines(n) for n in freed)
            if best is None or score > best[1]:
                best = (c, score, freed, big)
        if not best or best[1] == 0:
            break
        out.append(best[:3])
        core = best[3]
    return out, sorted(core)


# ---------------------------------------------------------------- loading the build

def header_index():
    """class name -> header path (first `class|struct Name` definition in include/ or src/ headers)."""
    idx = {}
    pat = re.compile(r"^\s*(?:class|struct)\s+(?:__declspec\([^)]*\)\s+|\w+_API\s+)?(\w+)\b[^;]*$")
    for d in ("include", "src"):
        for p in sorted((ROOT / d).rglob("*.h")):
            rel = p.relative_to(ROOT).as_posix()
            if "/ThirdParty/" in "/" + rel or rel.startswith("include/rust/"):
                continue
            try:
                text = p.read_bytes().decode("utf-8", "replace")
            except OSError:
                continue
            for line in text.splitlines():
                m = pat.match(line)
                if m:
                    idx.setdefault(m.group(1), rel)
    return idx


def load(build, out, all_refs=False):
    objs = link_objects(build)
    cache = NmCache(out / "nm_cache.json")
    units = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        syms = list(pool.map(cache.get, objs))
        reach = [None] * len(objs) if all_refs else list(pool.map(lambda o: set(cache.get(o, what="reach")), objs))
    for obj, ss, rs in zip(objs, syms, reach):
        try:
            mod, rel = source_of(obj, build)
        except ValueError:
            mod, rel = "?", pathlib.PurePosixPath(obj.stem)
        src = None
        for ext in SRC_EXT:
            cand = ROOT / "src" / (str(rel) + ext)
            if cand.exists():
                src = cand
                break
        name = rel.as_posix() + (src.suffix if src else ".obj")
        kind = "vendored" if rel.parts[0] == "ThirdParty" else "cpp"
        u = Unit(name, kind, count_lines(src) if src else 0, src)
        u.module = mod
        for k, raw, dem in ss:
            if k == "U":
                if rs is None or raw in rs:
                    u.undef.append((raw, dem))
            elif k not in ("w", "v"):
                u.defs.append((raw, dem, k))
        units.append(u)
    lib = rust_lib(build)
    rust_defs = set()
    if lib and lib.exists():
        rust_defs = {raw for k, raw, dem in cache.get(lib, True)}
    else:
        print(f"warning: no ee_game.lib under {build}: Rust symbols count as system", file=sys.stderr)
    cache.save()
    return units, rust_defs


# ---------------------------------------------------------------- reports

def build_reports(g, out, top, quiet=False):
    units, edges, other = g["units"], g["edges"], g["other"]
    cpp = sorted(n for n, u in units.items() if u.kind == "cpp")
    succ = {a: set(bs) for a, bs in edges.items()}
    pred = collections.defaultdict(set)
    for a, bs in succ.items():
        for b in bs:
            pred[b].add(a)
    L = lambda n: units[n].lines   # noqa: E731
    out.mkdir(parents=True, exist_ok=True)
    total_lines = sum(L(n) for n in cpp)

    def others(n):
        o = other.get(n, {})
        return " ".join(f"{k}={o[k]}" for k in ("rust", "system", "vendored", "inline", "data") if o.get(k))

    leaves = sorted((n for n in cpp if not succ.get(n)), key=lambda n: (-L(n), n))
    near = sorted((n for n in cpp if 1 <= len(succ.get(n, ())) <= 3), key=lambda n: (-L(n), n))
    with open(out / "leaves.txt", "w", encoding="utf-8", newline="\n") as f:
        f.write(f"# {len(leaves)} leaf units, {sum(L(n) for n in leaves)} lines (of {len(cpp)} units, "
                f"{total_lines} lines): no symbol they use resolves to another C++ unit\n")
        f.write("# lines  unit  [class]  incoming-units  calls-instead\n")
        for n in leaves:
            f.write(f"{L(n):6d}  {n}  [{units[n].cls}]  in={len(pred.get(n, ()))}  {others(n)}\n")
    with open(out / "near_leaves.txt", "w", encoding="utf-8", newline="\n") as f:
        f.write(f"# {len(near)} units with 1-3 C++ targets, {sum(L(n) for n in near)} lines\n")
        for n in near:
            f.write(f"{L(n):6d}  {n}  [{units[n].cls}]  {others(n)}\n")
            for b in sorted(succ[n], key=lambda b: (-len(edges[n][b]), b)):
                syms = sorted(qualified_name(d) for d in edges[n][b].values())
                f.write(f"          -> {b} ({len(syms)}): {', '.join(syms[:12])}{' ...' if len(syms) > 12 else ''}\n")

    score = collections.defaultdict(lambda: [0, 0, 0, 0])   # all lines, near lines, sole lines, units
    for a in cpp:
        ts = succ.get(a, ())
        for b in ts:
            s = score[b]
            s[0] += L(a)
            s[3] += 1
            if len(ts) <= 3:
                s[1] += L(a)
            if len(ts) == 1:
                s[2] += L(a)
    blockers = sorted(score, key=lambda b: (-score[b][0], b))
    with open(out / "blockers.txt", "w", encoding="utf-8", newline="\n") as f:
        f.write("# C++ units by the lines of the units that name them: all / near (<=3 targets) / sole (only target)\n")
        f.write("#    all   near   sole units  own  unit [class]\n")
        for b in blockers:
            s = score[b]
            f.write(f"{s[0]:7d} {s[1]:6d} {s[2]:6d} {s[3]:5d} {L(b):5d}  {b} [{units[b].cls}]\n")
        f.write("\n# the same, ranked by near\n")
        for b in sorted(score, key=lambda b: (-score[b][1], b))[:100]:
            s = score[b]
            if s[1]:
                f.write(f"{s[0]:7d} {s[1]:6d} {s[2]:6d} {s[3]:5d} {L(b):5d}  {b} [{units[b].cls}]\n")

    comps = tarjan(cpp, succ)
    comp_of = {n: i for i, c in enumerate(comps) for n in c}
    big = sorted((c for c in comps if len(c) > 1), key=lambda c: -sum(L(n) for n in c))
    cluster_leaves = []
    for c in big:
        outs = {b for n in c for b in succ.get(n, ()) if comp_of[b] != comp_of[n]}
        if not outs:
            cluster_leaves.append(c)
    with open(out / "sccs.txt", "w", encoding="utf-8", newline="\n") as f:
        f.write(f"# {len(big)} cycles (components of more than one unit), "
                f"{sum(len(c) for c in big)} units, {sum(L(n) for c in big for n in c)} lines\n")
        for i, c in enumerate(big):
            outs = sorted({b for n in c for b in succ.get(n, ()) if comp_of[b] != comp_of[n]})
            f.write(f"\nSCC {i + 1}: {len(c)} units, {sum(L(n) for n in c)} lines, "
                    f"{len(outs)} targets outside{' (CLUSTER LEAF)' if not outs else ''}\n")
            for n in sorted(c, key=lambda n: (-L(n), n)):
                f.write(f"  {L(n):6d}  {n}  [{units[n].cls}]\n")
            if outs:
                f.write(f"  outside: {', '.join(outs[:30])}{' ...' if len(outs) > 30 else ''}\n")

    # class view
    hdr = header_index()
    groups = collections.defaultdict(list)
    for n in cpp:
        groups[units[n].cls].append(n)
    cls_of = {n: units[n].cls for n in cpp}
    csucc = collections.defaultdict(collections.Counter)
    for a, bs in edges.items():
        for b, syms in bs.items():
            if cls_of[a] != cls_of[b]:
                csucc[cls_of[a]][cls_of[b]] += len(syms)

    def clines(c):
        h = hdr.get(c)
        return sum(L(n) for n in groups[c]) + (count_lines(ROOT / h) if h else 0)
    with open(out / "classes.txt", "w", encoding="utf-8", newline="\n") as f:
        cleaves = [c for c in groups if not csucc.get(c)]
        f.write(f"# {len(groups)} classes (units grouped by primary class); {len(cleaves)} with no outgoing C++ class\n")
        f.write("# lines(cpp+header)  class  units  -> targets (symbols)\n")
        for c in sorted(groups, key=lambda c: (len(csucc.get(c, ())), -clines(c), c)):
            ts = csucc.get(c, collections.Counter())
            f.write(f"{clines(c):6d}  {c}  [{len(groups[c])} units{', ' + hdr[c] if c in hdr else ''}]  "
                    f"-> {len(ts)}: {', '.join(f'{t}({k})' for t, k in ts.most_common(8))}"
                    f"{' ...' if len(ts) > 8 else ''}\n")
            if len(groups[c]) > 1:
                f.write(f"          units: {', '.join(sorted(groups[c]))}\n")

    # directories
    dirs = collections.defaultdict(lambda: collections.Counter())
    for n in cpp:
        d = str(pathlib.PurePosixPath(n).parent)
        s = dirs[d]
        s["units"] += 1
        s["lines"] += L(n)
        k = len(succ.get(n, ()))
        if k == 0:
            s["leaf_units"] += 1
            s["leaf_lines"] += L(n)
        elif k <= 3:
            s["near_units"] += 1
            s["near_lines"] += L(n)
        for b in succ.get(n, ()):
            if str(pathlib.PurePosixPath(b).parent) != d:
                s["out_edges"] += 1
        if comp_of[n] in {comp_of[c[0]] for c in big}:
            s["scc_lines"] += L(n)
    with open(out / "dirs.txt", "w", encoding="utf-8", newline="\n") as f:
        f.write("#  units   lines  leaf(u/lines)  near(u/lines)  in-SCC-lines  edges-out-of-dir  directory\n")
        for d in sorted(dirs, key=lambda d: -dirs[d]["lines"]):
            s = dirs[d]
            f.write(f"{s['units']:6d} {s['lines']:7d}  {s['leaf_units']:4d}/{s['leaf_lines']:7d}  "
                    f"{s['near_units']:4d}/{s['near_lines']:7d}  {s['scc_lines']:7d}  {s['out_edges']:6d}  {d}\n")

    ws, stuck = waves(cpp, succ, pred)
    cws = cluster_waves(cpp, succ, comps)
    big_no = {tuple(c): i + 1 for i, c in enumerate(big)}
    ks, core_after = keystones(cpp, succ, pred, L)
    with open(out / "waves.txt", "w", encoding="utf-8", newline="\n") as f:
        f.write("# the move order bottom-up over the condensation: a cycle (SCC) is one node and moves as one cluster.\n"
                "# wave 1 calls no other C++; each later wave calls only earlier waves' units.\n")
        f.write(f"# unit-only peel (no cycle moved): {' '.join(f'{len(w)}/{sum(L(n) for n in w)}' for w in ws)} "
                f"units/lines, then {len(stuck)} units ({sum(L(n) for n in stuck)} lines) are in or above a cycle\n")
        for i, w in enumerate(cws):
            f.write(f"\nwave {i + 1}: {sum(len(c) for c in w)} units, {sum(L(n) for c in w for n in c)} lines\n")
            for c in sorted(w, key=lambda c: (-sum(L(n) for n in c), c)):
                if len(c) > 1:
                    f.write(f"  {sum(L(n) for n in c):6d}  [SCC {big_no[tuple(c)]}: {len(c)} units, see sccs.txt]\n")
                else:
                    f.write(f"  {L(c[0]):6d}  {c[0]}\n")
    with open(out / "keystones.txt", "w", encoding="utf-8", newline="\n") as f:
        core0 = big[0] if big else []
        f.write(f"# greedy keystones of the biggest cycle (SCC 1: {len(core0)} units, {sum(L(n) for n in core0)} lines):\n"
                "# moving each in turn (to Rust, its own C++ calls becoming host calls) takes the listed units out of\n"
                "# the cycle (they become orderable units or smaller cycles). Cumulative, in order.\n")
        for i, (c, freed_lines, freed) in enumerate(ks):
            hosts = sum(len(v) for v in edges.get(c, {}).values())
            f.write(f"\n{i + 1}. {c} ({L(c)} lines, {hosts} C++ symbols to host, "
                    f"{len(edges.get(c, {}))} targets): takes out {len(freed)} units, {freed_lines} lines\n")
            for n in sorted(freed, key=lambda n: (-L(n), n))[:40]:
                f.write(f"       {L(n):6d}  {n}\n")
            if len(freed) > 40:
                f.write(f"       ... {len(freed) - 40} more\n")
        f.write(f"\nthe cycle left after {len(ks)} keystones: {len(core_after)} units, "
                f"{sum(L(n) for n in core_after)} lines\n")
        for n in sorted(core_after, key=lambda n: (-L(n), n)):
            f.write(f"  {L(n):6d}  {n}\n")

    with open(out / "edges.tsv", "w", encoding="utf-8", newline="\n") as f:
        f.write("from\tto\tweight\tsymbols\n")
        for a in sorted(edges):
            for b in sorted(edges[a]):
                f.write(f"{a}\t{b}\t{len(edges[a][b])}\t{' | '.join(sorted(edges[a][b].values()))}\n")

    summary = {
        "units": len(cpp), "lines": total_lines,
        "edges": sum(len(v) for v in edges.values()),
        "leaves": len(leaves), "leaf_lines": sum(L(n) for n in leaves),
        "near_leaves": len(near), "near_lines": sum(L(n) for n in near),
        "sccs": len(big), "scc_units": sum(len(c) for c in big), "scc_lines": sum(L(n) for c in big for n in c),
        "cluster_leaves": len(cluster_leaves),
        "unit_waves": [[len(w), sum(L(n) for n in w)] for w in ws],
        "stuck": len(stuck), "stuck_lines": sum(L(n) for n in stuck),
        "waves": [[sum(len(c) for c in w), sum(L(n) for c in w for n in c)] for w in cws],
        "keystones": [[c, sc] for c, sc, _ in ks],
        "core_after_keystones": [len(core_after), sum(L(n) for n in core_after)],
    }
    (out / "summary.json").write_text(json.dumps(summary, indent=1) + "\n")
    if not quiet:
        print(f"{summary['units']} C++ units, {summary['lines']} lines, {summary['edges']} edges")
        print(f"leaves: {summary['leaves']} units, {summary['leaf_lines']} lines")
        print(f"near-leaves (1-3 targets): {summary['near_leaves']} units, {summary['near_lines']} lines")
        print(f"cycles: {summary['sccs']} SCCs, {summary['scc_units']} units, {summary['scc_lines']} lines "
              f"(largest {sum(L(n) for n in big[0]) if big else 0} lines, {len(big[0]) if big else 0} units)")
        print(f"\ntop {top} leaves:")
        for n in leaves[:top]:
            print(f"  {L(n):6d}  {n}")
        print(f"\ntop {top} near-leaves:")
        for n in near[:top]:
            print(f"  {L(n):6d}  {n}  -> {', '.join(sorted(succ[n]))}")
        print(f"\ntop {top} blockers (all / near / sole lines):")
        for b in blockers[:top]:
            s = score[b]
            print(f"  {s[0]:7d} {s[1]:6d} {s[2]:6d}  {b}")
        print("\nunit waves (units/lines): " + ", ".join(f"{k}/{n}" for k, n in summary["unit_waves"])
              + f"; then {summary['stuck']}/{summary['stuck_lines']} in or above a cycle")
        print("cluster waves (cycles moved whole; units/lines): "
              + ", ".join(f"{k}/{n}" for k, n in summary["waves"]))
        print("keystones of SCC 1 (greedy, cumulative; lines taken out of the cycle):")
        for c, freed in summary["keystones"]:
            print(f"  {freed:7d}  {c}")
        print(f"  SCC 1 left: {summary['core_after_keystones'][0]} units, {summary['core_after_keystones'][1]} lines")
        print(f"\nreports in {out}")
    return summary


def show_unit(g, name):
    units, edges = g["units"], g["edges"]
    matches = [n for n in units if n == name or n.endswith("/" + name) or pathlib.PurePosixPath(n).stem == name]
    if len(matches) != 1:
        sys.exit(f"{name}: {'no unit' if not matches else 'ambiguous: ' + ', '.join(matches)}")
    n = matches[0]
    u = units[n]
    print(f"{n}: {u.lines} lines, class {u.cls}, {len(u.defs)} defined / {len(u.undef)} undefined symbols")
    o = g["other"].get(n, {})
    print("  other: " + " ".join(f"{k}={v}" for k, v in sorted(o.items())))
    print(f"outgoing ({len(edges.get(n, {}))} units):")
    for b, syms in sorted(edges.get(n, {}).items(), key=lambda kv: (-len(kv[1]), kv[0])):
        print(f"  -> {b} ({len(syms)})")
        for d in sorted(syms.values()):
            print(f"       {d}")
    inc = [(a, bs[n]) for a, bs in edges.items() if n in bs]
    print(f"incoming ({len(inc)} units):")
    for a, syms in sorted(inc, key=lambda kv: (-len(kv[1]), kv[0])):
        print(f"  <- {a} ({len(syms)})")
        for d in sorted(syms.values()):
            print(f"       {d}")


# ---------------------------------------------------------------- self test

SELFTEST = {
    "selftest.h": "struct B { int f(); int g() { return f() + 1; } virtual int v(); };\n"
                  "inline int free_inline(int x) { return x * 3; }\n",
    "a.cpp": '#include "selftest.h"\nextern "C" int puts(const char*);\nextern "C" int ee_selftest_rust(int);\n'
             "struct A { int run(B& b); };\n"
             'int A::run(B& b) { return b.g() + puts("x") + free_inline(2) + ee_selftest_rust(1); }\n',
    "b.cpp": '#include "selftest.h"\nint B::f() { return 1; }\nint B::v() { return g(); }\n'
             "int b_free(B& b) { return b.g() + free_inline(1); }\n",
    "d.h": "struct __declspec(dllexport) D { int h(); int unused() { return h(); } };\n",
    "c.cpp": '#include "d.h"\nextern "C" int puts(const char*);\nint c_only() { return puts("c"); }\n',
    "d.cpp": '#include "d.h"\nint D::h() { return 4; }\n',
    "rust.c": "int ee_selftest_rust(int x) { return x; }\n",
}


def selftest():
    work = ROOT / "build" / "cpp_deps" / "selftest"
    work.mkdir(parents=True, exist_ok=True)
    for name, text in SELFTEST.items():
        (work / name).write_text(text, newline="\n")
    objs = {}
    for name in ("a.cpp", "b.cpp", "c.cpp", "d.cpp", "rust.c"):
        obj = work / (pathlib.Path(name).stem + ".obj")
        r = subprocess.run([str(CLANG), "/nologo", "/c", "/Od", "/GS-", "/Zl", str(work / name), f"/Fo{obj}"],
                           capture_output=True, text=True)
        if r.returncode:
            sys.exit(f"selftest: compiling {name} failed:\n{r.stdout}{r.stderr}")
        objs[name] = obj
    units = []
    for name in ("a.cpp", "b.cpp", "c.cpp", "d.cpp"):
        u = Unit(name, "cpp", count_lines(work / name))
        rs = set(reachable_refs(objs[name]))
        for k, raw, dem in nm(objs[name]):
            if k != "U":
                u.defs.append((raw, dem, k))
            elif raw in rs:
                u.undef.append((raw, dem))
        units.append(u)
    rust_defs = {raw for k, raw, dem in nm(objs["rust.c"], True)}
    g = analyse(units, rust_defs)
    e = g["edges"]
    checks = []

    def check(what, ok):
        checks.append(ok)
        print(f"{'ok  ' if ok else 'FAIL'} {what}")
    a_to_b = e.get("a.cpp", {}).get("b.cpp", {})
    check("a.cpp -> b.cpp through B::f (called by its own COMDAT copy of B::g)",
          any("B::f" in d for d in a_to_b.values()))
    check("B::g (a COMDAT in a.obj and b.obj) belongs to b.cpp, B's home",
          g["home"].get("B") == "b.cpp" and g["resolve"]("?g@B@@QEAAHXZ", "public: int __cdecl B::g(void)")[1] == "b.cpp")
    check("b.cpp and c.cpp are leaves", not e.get("b.cpp") and not e.get("c.cpp"))
    check("c.obj holds D::unused (dllexport) but never reaches it: no c.cpp -> d.cpp edge",
          any("D::unused" in d for _, d, _ in g["units"]["c.cpp"].defs)
          and not any("D::h" in d for _, d in g["units"]["c.cpp"].undef)
          and any(k == "U" and "D::h" in d for k, _, d in nm(objs["c.cpp"])))
    check("a.cpp's only C++ target is b.cpp", set(e.get("a.cpp", {})) == {"b.cpp"})
    check("ee_selftest_rust resolves to Rust", g["other"]["a.cpp"]["rust"] == 1)
    check("puts resolves to the system", g["other"]["c.cpp"]["system"] >= 1)
    check("free_inline (header-only) is no edge", not any("free_inline" in d for bs in e.values()
                                                          for syms in bs.values() for d in syms.values()))
    check("b.cpp's primary class is B", g["units"]["b.cpp"].cls == "B")
    check("a.cpp's primary class is A", g["units"]["a.cpp"].cls == "A")
    comps = tarjan(["a.cpp", "b.cpp", "c.cpp"], {a: set(bs) for a, bs in e.items()})
    check("no cycle", all(len(c) == 1 for c in comps))
    cyc = tarjan(["x", "y", "z"], {"x": {"y"}, "y": {"x", "z"}})
    check("tarjan finds {x, y}", sorted(map(tuple, cyc)) == [("x", "y"), ("z",)])
    ws, st = waves(["x", "y", "z", "w"], {"x": {"y"}, "y": {"x", "z"}, "w": {"z"}}, {"y": {"x"}, "x": {"y"}, "z": {"y", "w"}})
    check("waves: z, then w; x and y stuck", ws == [["z"], ["w"]] and st == ["x", "y"])
    # a cycle p <-> q <-> r <-> p with q the hub: moving q breaks it
    sq = {"p": {"q"}, "q": {"p", "r"}, "r": {"q"}, "z": set()}
    pq = {"q": {"p", "r"}, "p": {"q"}, "r": {"q"}}
    ks, st = keystones(["p", "q", "r", "z"], sq, pq, lambda n: 10)
    check("keystones: moving the hub q takes p and r out of the cycle",
          [k[0] for k in ks] == ["q"] and ks[0][1] == 20 and len(st) <= 1)
    cw = cluster_waves(["x", "y", "z", "w"], {"x": {"y"}, "y": {"x", "z"}, "w": {"z"}}, [["x", "y"], ["z"], ["w"]])
    check("cluster waves: z, then {x, y} and w together", cw == [[["z"]], [["x", "y"], ["w"]]])
    print("SELFTEST " + ("PASS" if all(checks) else "FAIL"))
    return 0 if all(checks) else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("cmd", nargs="?", default="report", choices=["report", "unit", "selftest"])
    ap.add_argument("file", nargs="?", help="unit: the unit (path under src/, file name or stem)")
    ap.add_argument("--build", default=str(ROOT / "build" / "clang"))
    ap.add_argument("--out", default=str(ROOT / "build" / "cpp_deps"))
    ap.add_argument("--top", type=int, default=30)
    ap.add_argument("--all-refs", action="store_true",
                    help="count every undefined symbol of an object, also those only unreached COMDAT copies make")
    ap.add_argument("--code-only", action="store_true",
                    help="count only calls (functions) as edges, not references to C++ globals, vtables or RTTI")
    a = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if a.cmd == "selftest":
        return selftest()
    build, out = pathlib.Path(a.build).resolve(), pathlib.Path(a.out).resolve()
    units, rust_defs = load(build, out, a.all_refs)
    g = analyse(units, rust_defs, a.code_only)
    if a.cmd == "unit":
        if not a.file:
            sys.exit("unit: name a unit")
        show_unit(g, a.file)
        return 0
    build_reports(g, out, a.top)
    return 0


if __name__ == "__main__":
    sys.exit(main())
