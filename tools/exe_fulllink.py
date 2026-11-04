"""Link EE-AOC.exe with lld-link: its compiled units, the image libraries, the import libraries and resources,
plus stubs.obj for the symbols the source does not define yet (pass 1 finds them, pass 2 links with the stubs, pass 3
places the member aliases).

The symbols the source does not define are classified by what they were in the original image (names.txt and the
other link tables): the WON library (stubbed: its source is not part of the build yet), ICF / vtable-slot / pure
aliases of defined functions, data the original placed inside other objects (member aliases), and the rest
(traps). Everything else of the game (engine, plug-ins, their library units) is linked in as objects: EXTRA_LIBS
(objects and libraries the caller adds to every link), IMPORT_MODULES (the modules whose original import tables
name the system and third-party DLLs to link against), EXTRA_RES (more compiled resources: the language strings)."""
import bisect
import collections
import pathlib
import re
import shutil
import struct
import subprocess
import sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import exe_roundtrip
import exe_units
import verify
from ee import LINK_DIR, LLD_LINK, LLVM_LIB, ROOT, RUNTIME_LIBS, lib_paths
from undname import undname
EXE_RC = ROOT / "src" / "Empire Earth" / "EE-AOC.rc"
GAME = (0x1000, 0x29d080)
WON = [(0x29deb0, 0x3fe220), (0x416e00, 0x437000)]
RDATA, DATA = (0x437000, 0x4b4000), (0x4b4000, 0x4b4000 + 0xb31e4)
DATA_RAW_END = 0x4b4000 + 0x32000
LINK_FLAGS = ("/INCREMENTAL:NO /OPT:REF /OPT:ICF /MACHINE:X86 /SUBSYSTEM:WINDOWS /NODEFAULTLIB /FIXED "
              "/ENTRY:WinMainCRTStartup /SAFESEH:NO /ERRORLIMIT:0 /DEMANGLE:NO")
EXTRA_LIBS = []
IMPORT_MODULES = ["exe"]
EXTRA_RES = []
ODS, IDP, EXITP = "__imp__OutputDebugStringA@4", "__imp__IsDebuggerPresent@0", "__imp__ExitProcess@4"
CLASSES = ("won", "engine", "icf", "vslot", "pure", "mismatch", "compile", "unwritten", "unregistered", "unbound", "data",
           "library")


class Obj:

    def __init__(self):
        self.secs = []
        self.syms = []
        self.weaks = []

    def weak(self, name, default):
        self.weaks.append((name, default))

    def section(self, name, chars, data=b""):
        self.secs.append([name, bytearray(data), chars, []])
        return len(self.secs)

    def define(self, name, sec, value, func=False, cls=2):
        self.syms.append((name, sec, value, cls, 0x20 if func else 0))

    def reloc(self, sec, off, name, typ):
        self.secs[sec - 1][3].append((off, name, typ))

    def write(self, path):
        names = [s[0] for s in self.syms]
        idx = {}
        for i, s in enumerate(self.syms):
            idx.setdefault(s[0], i)
        undef = []
        for _, _, _, rels in self.secs:
            for _, n, _ in rels:
                if n not in idx and n not in undef:
                    undef.append(n)
        for _, d in self.weaks:
            if d not in idx and d not in undef:
                undef.append(d)
        allsyms = list(self.syms) + [(n, 0, 0, 2, 0) for n in undef]
        for i, s in enumerate(allsyms):
            idx.setdefault(s[0], i)
        ptr = 20 + 40 * len(self.secs)
        hdrs, body = b"", b""
        for name, data, chars, rels in self.secs:
            bss = chars & 0x80
            rp = 0 if bss else ptr
            if not bss:
                ptr += len(data)
            relp = ptr if rels else 0
            ptr += 10 * len(rels)
            hdrs += name.encode().ljust(8, b"\0") + struct.pack("<IIIIIIHHI", 0, 0, len(data), rp, relp, 0,
                                                                len(rels), 0, chars)
            body += (b"" if bss else bytes(data)) + b"".join(struct.pack("<IIH", o, idx[n], t) for o, n, t in rels)
        strtab = bytearray(4)
        recs = b""
        for name, sec, value, cls, typ in allsyms:
            if len(name) <= 8:
                raw = name.encode().ljust(8, b"\0")
            else:
                raw = struct.pack("<II", 0, len(strtab))
                strtab += name.encode() + b"\0"
            recs += raw + struct.pack("<IhHBB", value, sec, typ, cls, 0)
        nsym = len(allsyms)
        for name, d in self.weaks:
            if len(name) <= 8:
                raw = name.encode().ljust(8, b"\0")
            else:
                raw = struct.pack("<II", 0, len(strtab))
                strtab += name.encode() + b"\0"
            recs += raw + struct.pack("<IhHBB", 0, 0, 0, 105, 1) + struct.pack("<II", idx[d], 3) + bytes(10)
            nsym += 2
        struct.pack_into("<I", strtab, 0, len(strtab))
        hdr = struct.pack("<HHIIIHH", 0x14C, len(self.secs), 0, ptr, nsym, 0, 0)
        pathlib.Path(path).write_bytes(hdr + hdrs + body + recs + bytes(strtab))
        del names


TEXT = 0x60500020
RDATA_S = 0x40400040
DATA_S = 0xC0400040
BSS_S = 0xC0400080
DIR32, REL32 = 6, 0x14


def read_names():
    at, of = collections.defaultdict(list), collections.defaultdict(set)
    for f in (LINK_DIR / "exe" / "names.txt", LINK_DIR / "exeimg" / "names.txt"):
        for line in open(f):
            line = line.split("#", 1)[0].strip()
            if line:
                r, n = line.split(None, 1)
                r, n = int(r, 16), n.strip()
                at[r].append(n)
                of[n].add(r)
    return at, of


def coff_symbols(path):
    c = exe_units.CoffEdit(path)
    sel = {}
    for s in c.syms.values():
        if s["cls"] == 3 and s["naux"] == 1 and s["sec"] > 0 and s["value"] == 0 and \
                s["name"] == c.secs[s["sec"] - 1]["name"] and c.secs[s["sec"] - 1]["chars"] & 0x1000:
            sel[s["sec"]] = c.b[s["off"] + 18 + 14]
    defined, undef = {}, {}
    for s in c.syms.values():
        if s["cls"] != 2:
            continue
        if s["sec"] > 0:
            defined[s["name"]] = sel.get(s["sec"], 0)
        elif s["sec"] == 0 and s["value"] > 0:
            defined[s["name"]] = "common"
        elif s["sec"] == 0:
            undef[s["name"]] = undef.get(s["name"], False) or s["type"] == 0x20
    # clang leaves the type of undefined symbols null (the original compiler typed functions 0x20): a C++ name is a
    # function when its demangled form has a parameter list, a C name when a call/jmp rel32 targets it
    import implib
    called = set()
    vc = verify.Coff(path)
    for i, sec in enumerate(vc.secs, 1):
        if sec["chars"] & 0x20:
            data = vc.section_data(i)
            for va, sym, typ in vc.relocs(i):
                if typ == 0x14 and va > 0 and data[va - 1] in (0xE8, 0xE9):
                    called.add(sym["name"])
    for n, f in undef.items():
        if not f and not n.startswith("__imp_"):
            undef[n] = (not implib.is_cpp_data(n)) if n.startswith("?") else n in called
    return defined, undef


def engine_lib(lib_dir, extra, pkg, functions=()):
    import implib
    lib = lib_dir / "Low-Level Engine.lib"
    hints = {sym.encode(): hint for (dll, _), (sym, hint) in exe_roundtrip.short_imports(lib).items()}
    deff = lib_dir / "Low-Level Engine.def"
    lines = deff.read_text().splitlines()
    undata, still = [], []
    for ln in lines:
        w = ln.split()
        if w and w[0] in functions and undname(w[0]).rstrip().endswith(")const"):
            (still if len(w) == 2 and w[1] == "DATA" else undata).append(w[0])
    if still:
        sys.exit(f"implib marks {len(still)} `)const` methods DATA again (no call thunk): {still[:3]}")
    have = {ln.split()[0] for ln in lines[2:] if ln.strip()}
    lines += [f"    {n}{' DATA' if implib.is_cpp_data(n) else ''}" for n in extra if n not in have]
    deff.write_text("\n".join(lines) + "\n")
    code, log = exe_roundtrip.run([str(LLVM_LIB), "/nologo", "/machine:x86", f"/def:{deff.name}",
                                   f"/out:{lib.name}"], lib_dir)
    if code:
        sys.exit(log)
    implib.fix_members(lib, hints)
    return undata


def link(out, objs, libs, res, debug, tag):
    target = out / "EE-AOC.exe"
    rsp = out / f"link_{tag}.rsp"
    # the engine's and the plug-ins' objects come first: global constructors run in object order, and the game's
    # (constructed after the engine DLL's in the original) use engine globals
    first = [p for p in EXTRA_LIBS if str(p).lower().endswith(".obj")]
    rest = [p for p in EXTRA_LIBS if p not in first]
    rsp.write_text("\n".join(f'"{pathlib.Path(p).relative_to(out) if pathlib.Path(p).is_relative_to(out) else p}"'
                             for p in [*first, *objs, *libs, *rest, *res, *RUNTIME_LIBS]) + "\n")
    cmd = [str(LLD_LINK), "/nologo", *LINK_FLAGS.split(), *(["/DEBUG"] if debug else []),
           f"/OUT:{target.name}", f"/MAP:{target.stem}.map", f"/PDB:{target.stem}.pdb", *lib_paths(), f"@{rsp.name}"]
    if target.exists():
        target.unlink()
    code, log = exe_roundtrip.run(cmd, out)
    log = "\n".join(l for l in log.splitlines() if "ignoring section .debug$S with unrecognized magic" not in l)
    (out / f"link_{tag}.log").write_text(log)
    import relink
    relink.link6_map(target.with_suffix(".map"))
    return code, log


SYM_RE = re.compile(r'"[^"]*" \((\S+)\)$')


def parse_log(log):
    """lld-link output (/demangle:no) -> ({undefined: [referencing objects]}, [(duplicate, object, object)],
    [other errors])."""
    unres, dups, other = {}, [], []
    lines = log.splitlines()
    for k, ln in enumerate(lines):
        m = re.match(r"^lld-link: error: (undefined symbol|duplicate symbol): (\S+)", ln)
        if not m:
            if "error:" in ln and "too many errors" not in ln:
                other.append(ln)
            continue
        where = []
        for l2 in lines[k + 1:]:
            if not l2.startswith(">>> "):
                break
            mm = re.match(r">>> (?:referenced by|defined at) (?:.*?\()?([^():]+\.obj|[^():]+\.lib\([^)]*\))", l2)
            where.append(mm.group(1) if mm else l2[4:])
        if m.group(1) == "undefined symbol":
            unres.setdefault(m.group(2), []).extend(where)
        else:
            dups.append((m.group(2), *(where + ["?", "?"])[:2]))
    return unres, dups, other


def scope_of(name):
    m = re.match(r"(\?[^@]*@(?:[^@]+@)*@)", name)
    return m.group(1) if m else None


def own_scope(sym):
    t = undname(sym) if sym.startswith("?") else sym
    out, depth = "", 0
    for ch in t:
        if ch == "<":
            depth += 1
        elif ch == ">":
            depth -= 1
        elif depth == 0:
            out += ch
    out = re.sub(r"\s*const\s*$", "", out.rstrip())
    if out.endswith(")"):
        depth, i = 0, len(out) - 1
        while i >= 0:
            depth += {")": 1, "(": -1}.get(out[i], 0)
            if depth == 0:
                break
            i -= 1
        out = out[:i]
    toks = [x for x in re.split(r"\s+", out) if "::" in x]
    return toks[-1] if toks else out.split()[-1] if out.split() else sym


def virtual_tag(sym):
    if sym.startswith("??0"):
        return "ctor"
    if sym.startswith("??1"):
        return "dtor"
    m = re.match(r"\?[^?@][^@]*@(?:[^@]+@)*@([A-Z])", sym) or re.match(r"\?\?[^@]+@(?:[^@]+@)*@([A-Z])", sym)
    return "virtual" if m and m.group(1) in "EFMNUV" else "function"


def classify(sym, is_func, ctx):
    c, d = classify1(sym, is_func, ctx)
    if not ctx["of"].get(sym) and sym in ctx["inferred"]:
        d += f" [address {ctx['inferred'][sym][0]:#x} from {ctx['inferred'][sym][1]}]"
    return c, d


def classify1(sym, is_func, ctx):
    rvas = sorted(ctx["of"].get(sym, ()))
    if not rvas and sym in ctx["inferred"]:
        rvas = [ctx["inferred"][sym][0]]
    defined = ctx["defined"]
    bare = sym[6:] if sym.startswith("__imp_") else sym
    if bare in ctx["engine_exports"]:
        return "engine", "Low-Level Engine.dll exports it; the original exe does not import it" + \
            (" (dllimport)" if bare != sym else f" ({virtual_tag(sym)}, referenced directly)")
    if any(lo <= r < hi for r in rvas for lo, hi in WON):
        return "won", ", ".join(f"{r:#x}" for r in rvas)
    own = own_scope(sym)
    slot = not ctx["of"].get(sym) and sym in ctx["inferred"] and ctx["inferred"][sym][1].startswith("slot")
    for r in rvas:
        imp = ctx["thunk_at"](r)
        if imp:
            ctx["alias"][sym] = imp
            return ("pure" if imp == "__purecall" else "library"), f"{r:#x} is the import thunk {imp}"
        alias = [n for n in ctx["at"].get(r, ()) if n != sym and n in defined]
        if alias:
            ctx["alias"][sym] = alias[0]
            if own_scope(alias[0]) == own:
                return "mismatch", f"{r:#x}: defined as {alias[0]} in {defined[alias[0]]}"
            if slot:
                return "vslot", f"{r:#x}: the original slot runs {alias[0]} ({defined[alias[0]]})"
            return "icf", f"{r:#x}: ICF twin {alias[0]} ({defined[alias[0]]}) is written, this body is not"
    sc = scope_of(sym)
    twins = sorted(n for n in ctx["by_scope"].get(sc, ()) if n != sym) if sc and is_func else []
    if twins and not rvas:
        sg = signature(sym)
        if len(twins) == 1 and sg[1] is not None and signature(twins[0])[:2] == sg[:2]:
            ctx["alias"][sym] = twins[0]
        return "mismatch", f"no address; same scope defined as {twins[0]} in {defined[twins[0]]}" + \
            (f" (+{len(twins) - 1})" if len(twins) > 1 else "") + \
            ("" if sym in ctx["alias"] else " (different stack cleanup or several overloads: trap)")
    if sym.startswith("__imp_"):
        return "library", "import not in the original import table"
    for r in rvas:
        if GAME[0] <= r < GAME[1]:
            u = ctx["unit_at"](r)
            if u is None:
                return "unregistered", f"{r:#x} (gap between registered units)"
            if u["obj"] in ctx["fails"]:
                return "compile", f"{r:#x} in {u['rel']} (did not compile)"
            if own.startswith("WONAPI::"):
                return "won", f"{r:#x} in {u['rel']}: a WONAPI type's member, its game-range copy is unwritten"
            return "unwritten", f"{r:#x} in {u['rel']} ({virtual_tag(sym)})"
    if own.startswith("WONAPI::"):
        return "won", f"by name ({own})"
    for r in rvas:
        if RDATA[0] <= r < DATA[1]:
            return "data", f"{r:#x} ({'.rdata' if r < RDATA[1] else '.data' if r < DATA_RAW_END else '.bss'})"
        if r >= GAME[1]:
            return "library", f"{r:#x} (library range)" + (": shim" if sym in LIB_SHIMS else "")
    if not is_func:
        return "data", "no address bound"
    return "unbound", virtual_tag(sym) + ("; game-side code over WONAPI types" if "WONAPI" in sym else "")


PLACEHOLDER = [re.compile(r"^\?g[A-Z]{1,2}([0-9A-F]{6})@@"), re.compile(r"Fn([0-9A-F]{5,6})@")]


def read_vtable_slots():
    out = {}
    for line in (LINK_DIR / "exe" / "vtable_slots.tsv").read_text(encoding="utf-8").splitlines():
        if line.startswith("#") or not line.strip():
            continue
        cols = line.split("\t")
        vrva, slots = cols[0], cols[-1]
        out[int(vrva, 16)] = [None if t == "-" else int(t, 16) for t in slots.split(",")] if slots else []
    return out


def read_import_thunks():
    out = {}
    for line in (LINK_DIR / "exe" / "import_thunks.tsv").read_text(encoding="utf-8").splitlines():
        if line.startswith("#") or not line.strip():
            continue
        rva, sym = line.split("\t")
        out[int(rva, 16)] = sym
    return out


def infer_addresses(unresolved, objs, at, of, vtables):
    out = {}
    for s in unresolved:
        if of.get(s):
            continue
        for rx in PLACEHOLDER:
            m = rx.search(s)
            if m:
                out[s] = (int(m.group(1), 16), "placeholder name")
                break
    want = {s for s in unresolved if not of.get(s) and s not in out}
    seen = collections.defaultdict(dict)
    for it in objs:
        c = exe_units.CoffEdit(it["obj"])
        vts = [(sy["sec"], sy["value"], sy["name"]) for sy in c.syms.values()
               if sy["cls"] == 2 and sy["sec"] > 0 and sy["name"].startswith("??_7") and of.get(sy["name"])]
        for secno, value, vt in vts:
            sec = c.secs[secno - 1]
            later = [sy["value"] for sy in c.syms.values() if sy["sec"] == secno and sy["value"] > value]
            end = min(later) if later else sec["size"]
            vrva = min(of[vt])
            for _, va, si, typ in c.relocs(sec):
                tgt = c.syms[si]["name"]
                if typ != 6 or not value <= va < end or tgt not in want:
                    continue
                k, rem = divmod(va - value, 4)
                if vrva not in vtables:
                    VTABLES_MISSING.add(vt)
                slot = vtables.get(vrva, [])[k] if not rem and k < len(vtables.get(vrva, [])) else None
                if slot is not None:
                    seen[tgt].setdefault(slot, []).append((vt, f"slot {(va - value) // 4} of {vt} at {vrva:#x}"))
    VSLOT_CONFLICTS.clear()
    for tgt, slots in seen.items():
        m = re.match(r"\?[^@?]+@([^@?]+)@@", tgt)
        own = m.group(1) if m else None
        mine = [s for s, v in slots.items() if any(vt == f"??_7{own}@@6B@" for vt, _ in v)] if own else []
        pick = mine[0] if mine else next(iter(slots))
        out[tgt] = (pick, slots[pick][0][1])
        if len(slots) > 1:
            VSLOT_CONFLICTS[tgt] = {f"{s:#x}": [vt for vt, _ in v] for s, v in slots.items()} | {"chosen": f"{pick:#x}"}
    return out


VSLOT_CONFLICTS = {}
VTABLES_MISSING = set()
LIB_SHIMS = {"??2@YAPAXIABUnothrow_t@std@@@Z": (b"\xff\x25\0\0\0\0", 2, "__imp__malloc")}


def c_string(s):
    return s.encode("ascii", "replace") + b"\0"


def signature(sym):
    if not sym.startswith("?"):
        m = re.match(r"_[^@]+@(\d+)$", sym)
        if m:
            return "stdcall", int(m.group(1)), "int"
        return "cdecl", 0, "int"
    text = undname(sym)
    conv = next((c for c in ("__thiscall", "__cdecl", "__stdcall", "__fastcall") if f" {c} " in f" {text} "), None)
    t = re.sub(r"\s*const\s*$", "", text.rstrip())
    if conv is None or not t.endswith(")"):
        return conv, None, "int"
    depth, i = 0, len(t) - 1
    while i >= 0:
        depth += {")": 1, "(": -1}.get(t[i], 0)
        if depth == 0:
            break
        i -= 1
    params, head = t[i + 1:-1], t[:i]
    ret = head.split(conv)[0]
    ret = re.sub(r"^(public|private|protected):\s*", "", ret.strip())
    ret = re.sub(r"^(virtual|static)\s+", "", ret).strip()
    parts, depth, cur = [], 0, ""
    for ch in params:
        if ch in "<(":
            depth += 1
        elif ch in ">)":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        parts.append(cur.strip())
    size = 0
    for p in parts:
        if p in ("void", "..."):
            continue
        if "*" in p or "&" in p:
            size += 4
        elif re.search(r"\b(double|__int64)\b", p):
            size += 8
        elif re.match(r"(class|struct|union)\s", p):
            return conv, None, "int"
        else:
            size += 4
    name_part = head.split(conv)[-1].strip()
    segs = name_part.split("::")
    if len(segs) >= 2 and segs[-1] == segs[-2].split("<")[0]:
        kind = "this"
    elif not ret or ret == "void":
        kind = "void"
    elif "*" in ret or "&" in ret:
        kind = "int"
    elif re.search(r"\b(float|double)\b", ret):
        kind = "float"
    elif re.match(r"(class|struct|union)\s", ret):
        kind = "struct"
    else:
        kind = "int"
    if conv == "__fastcall":
        return conv, None, kind
    return conv, size, kind


class Stubs:

    def __init__(self, initialised=None):
        self.o = Obj()
        self.initialised = initialised
        self.text = self.o.section(".text", TEXT)
        self.rdata = self.o.section(".rdata", RDATA_S)
        self.flags = self.o.section(".data", DATA_S)
        self.ids = []
        t = self.o.secs[self.text - 1][1]
        self.o.define("$ee_trap", self.text, 0, func=True, cls=3)
        t += b"\x8b\x44\x24\x04\x50\xff\x15\0\0\0\0"
        self.o.reloc(self.text, 7, ODS, DIR32)
        t += b"\xff\x15\0\0\0\0"
        self.o.reloc(self.text, 13, IDP, DIR32)
        t += b"\x85\xc0\x74\x01\xcc\xff\x74\x24\x08\xff\x15\0\0\0\0"
        self.o.reloc(self.text, len(t) - 4, EXITP, DIR32)
        t += b"\xcc"
        self._align()
        self.zero = None
        self.code_traps = {}

    def _align(self, n=16):
        t = self.o.secs[self.text - 1][1]
        while len(t) % n:
            t += b"\xcc"

    def string(self, s):
        r = self.o.secs[self.rdata - 1][1]
        at = len(r)
        r += c_string(s)
        while len(r) % 4:
            r += b"\0"
        name = f"$ee_s{at:x}"
        self.o.define(name, self.rdata, at, cls=3)
        return name

    def trap(self, sym, why, public=True):
        tid = len(self.ids) + 1
        self.ids.append((tid, sym, why))
        msg = self.string(f"EE-TRAP {tid} {sym} ({why})\n")
        t = self.o.secs[self.text - 1][1]
        at = len(t)
        name = sym if public else f"$ee_t{tid}"
        self.o.define(name, self.text, at, func=True, cls=2 if public else 3)
        t += b"\x68" + struct.pack("<I", 0xEE700000 + tid) + b"\x68\0\0\0\0\xe8\0\0\0\0"
        self.o.reloc(self.text, at + 6, msg, DIR32)
        self.o.reloc(self.text, at + 11, "$ee_trap", REL32)
        self._align()
        return name

    def won(self, sym):
        conv, args, kind = signature(sym)
        if args is None or kind == "struct" or conv is None:
            self.trap(sym, "WON, no graceful stub: by-value class or unknown convention")
            return "trap"
        flag = len(self.o.secs[self.flags - 1][1])
        self.o.secs[self.flags - 1][1].extend(b"\0\0\0\0")
        fname = f"$ee_f{flag:x}"
        self.o.define(fname, self.flags, flag, cls=3)
        msg = self.string(f"EE-WON-STUB {sym}\n")
        t = self.o.secs[self.text - 1][1]
        at = len(t)
        self.o.define(sym, self.text, at, func=True)
        if kind == "this":
            t += b"\x51"
        p = len(t)
        t += b"\x80\x3d\0\0\0\0\x00\x75\x12\xc6\x05\0\0\0\0\x01\x68\0\0\0\0\xff\x15\0\0\0\0"
        self.o.reloc(self.text, p + 2, fname, DIR32)
        self.o.reloc(self.text, p + 11, fname, DIR32)
        self.o.reloc(self.text, p + 17, msg, DIR32)
        self.o.reloc(self.text, p + 23, ODS, DIR32)
        t += b"\x58\xc7\x00\0\0\0\0" if kind == "this" else b"\x33\xc0"
        t += b"\x33\xd2"
        if kind == "float":
            t += b"\xd9\xee"
        pop = args if conv in ("__thiscall", "__stdcall", "stdcall") else 0
        t += struct.pack("<BH", 0xC2, pop) if pop else b"\xc3"
        self._align()
        return "won"

    def alias(self, sym, target):
        t = self.o.secs[self.text - 1][1]
        at = len(t)
        self.o.define(sym, self.text, at, func=True)
        t += b"\xe9\0\0\0\0"
        self.o.reloc(self.text, at + 1, target, REL32)
        self._align()

    def shim(self, sym, code, field, target):
        t = self.o.secs[self.text - 1][1]
        at = len(t)
        self.o.define(sym, self.text, at, func=True)
        t += code
        self.o.reloc(self.text, at + field, target, DIR32)
        self._align()

    def zero_block(self):
        if self.zero is None:
            self.zero = self.o.section(".bss", BSS_S, bytes(0x1000))
            self.o.define("$ee_zero", self.zero, 0, cls=3)
        return "$ee_zero"

    def data(self, sym, rva, size, resolve, vtable_slots=0):
        fb = collections.Counter()
        if rva is None:
            if vtable_slots:
                sec = self.o.section(".rdata", RDATA_S, bytes(4 * vtable_slots))
                tr = self.trap(sym, "virtual call through a vtable with no address", public=False)
                for k in range(vtable_slots):
                    self.o.reloc(sec, 4 * k, tr, DIR32)
                self.o.define(sym, sec, 0)
                fb["vtable slots trap"] += vtable_slots
                return fb
            sec = self.o.section(".bss", BSS_S, bytes(256))
            self.o.define(sym, sec, 0)
            fb["zero, size guessed"] += 1
            return fb
        if rva >= DATA_RAW_END:
            sec = self.o.section(".bss", BSS_S, bytes(size))
            self.o.define(sym, sec, 0)
            return fb
        if self.initialised is None:
            raise SystemExit(f"data stub {sym} at {rva:#x}: initialised contents unknown (define the global in its "
                             f"unit)")
        return self.initialised(self, sym, rva, size, resolve)


MAP_RE = re.compile(r"^ ([0-9a-f]{4}):([0-9a-f]{8})\s+(\S+)\s+([0-9a-f]{8})\s+(?:f\s+)?(?:i\s+)?(\S+)?", re.I)


def read_members(path, at, known):
    out = {}
    for line in pathlib.Path(path).read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        f = line.split("\t")
        alias, addr, short, off = f[0], int(f[1], 16), f[2], int(f[3].lstrip("+"), 16)
        rva = addr - off
        parts = short.split("::")
        prefix = "?" + "@".join(reversed(parts)) + "@@"
        names = [n for n in at.get(rva, ()) if n.startswith(prefix) and known(n)]
        if len(names) != 1:
            raise SystemExit(f"members: container {short} of {alias} at {rva:#x}: {names or 'no known symbol'}")
        out[alias] = (names[0], off, rva + off)
    return out


def read_map(path):
    by, by_obj = {}, {}
    for line in pathlib.Path(path).read_text(encoding="latin-1").splitlines():
        m = MAP_RE.match(line)
        if m:
            va = int(m.group(4), 16)
            by.setdefault(m.group(3), va)
            by_obj[(m.group(3), (m.group(5) or "").lower())] = va
    return by, by_obj


def member_alias_pass(out, objs, libs, res, debug, stubs, member_alias):
    def resolve(by, name, seen=()):
        if name in member_alias:
            if name in seen:
                raise SystemExit(f"member alias cycle at {name}")
            cont, off, _ = member_alias[name]
            return resolve(by, cont, seen + (name,)) + off
        if name not in by:
            raise SystemExit(f"member alias container {name} is not in the map")
        return by[name]
    by2, _ = read_map(out / "EE-AOC.map")
    containers = sorted({c for c, _, _ in member_alias.values()} - set(member_alias))
    for attempt in range(3):
        values = {s: resolve(by2, s) for s in member_alias}
        syms = stubs.o.syms
        for k, (name, sec, value, cls, typ) in enumerate(syms):
            if name in values:
                syms[k] = (name, sec, values[name], cls, typ)
        stubs.o.write(out / "stubs.obj")
        code, log = link(out, objs + [out / "stubs.obj"], libs, res, debug, "pass3")
        unres, dups, other = parse_log(log)
        by3, by3_obj = read_map(out / "EE-AOC.map")
        moved = {c: [f"{by2.get(c, 0):#x}", f"{by3.get(c, 0):#x}"] for c in containers if by2.get(c) != by3.get(c)}
        if not moved or code:
            break
        by2 = by3
    map_bad = {s: f"{by3[s]:#x} != {v:#x}" for s, v in values.items() if s in by3 and by3[s] != v}
    return ({"count": len(member_alias), "exit": code, "links": attempt + 1, "unresolved": unres,
             "duplicates_link": dups, "other": other, "values": {s: f"{v:#x}" for s, v in sorted(values.items())},
             "containers": {c: f"{by3.get(c, 0):#x}" for c in containers}, "moved": moved, "map_bad": map_bad},
            values, by3_obj)


def link_exe(out, items, fails, debug=True, members=None, stubs_pass=True, patch=None, initialised=None,
             member_offsets=None):
    """member_offsets(container symbol, VC6 offset) -> this build's offset of the same member (the member table
    records VC6 layouts; classes holding STL containers changed size), or None to keep the table's offsets."""
    pkg = None
    ok = [it for it in items if it["obj"] not in fails]
    game = [it for it in ok if it["mod"] == "exe"]
    img = [it for it in ok if it["mod"] == "exeimg"]

    defined = {}
    dup_pre = collections.defaultdict(list)
    refs = collections.defaultdict(lambda: [False, []])
    for it in ok:
        d, u = coff_symbols(it["obj"])
        it["defs"] = d
        for n, sel in d.items():
            dup_pre[n].append((it["rel"], sel))
            defined.setdefault(n, it["rel"])
        for n, f in u.items():
            refs[n][0] |= f
            refs[n][1].append(it["rel"])
    by_scope = collections.defaultdict(set)
    for n in defined:
        sc = scope_of(n)
        if sc:
            by_scope[sc].add(n)
    lib_dir = out / "lib"
    shutil.rmtree(lib_dir, ignore_errors=True)
    lib_dir.mkdir(parents=True)
    exe_roundtrip.write_order_object(lib_dir / "refs.obj", sorted(refs))
    import implib
    exe_imports = implib.read_imports("exe")
    libs = []
    for m in IMPORT_MODULES:
        extra_objs = [x for x in EXTRA_LIBS if str(x).lower().endswith(".obj")]   # their decorated names too
        for lib in implib.make_import_libs(m, lib_dir / m, [lib_dir / "refs.obj", *extra_objs]):
            if pathlib.Path(lib).name.lower() not in {pathlib.Path(x).name.lower() for x in libs}:
                libs.append(lib)
    imglib = lib_dir / "exeimg.lib"
    (lib_dir / "exeimg.rsp").write_text("\n".join(f'"{it["obj"]}"' for it in img) + "\n")
    code, log = exe_roundtrip.run([str(LLVM_LIB), "/nologo", f"/out:{imglib.name}", "@exeimg.rsp"], lib_dir)
    if code:
        sys.exit(log)
    libs.insert(1, imglib)
    undata = []
    import rsrc_rebuild
    res = [rsrc_rebuild.compile_rc(EXE_RC, out / "rsrc")] + list(EXTRA_RES)

    objs = [it["obj"] for it in game]
    dedupe = patch(out, objs, dup_pre, game) if patch else []
    code1, log1 = link(out, objs, libs, res, debug, "pass1")
    unres1, dups1, other1 = parse_log(log1)
    print(f"pass 1: link exit {code1}: {len(unres1)} unresolved symbols, {len(dups1)} duplicates, "
          f"{len(other1)} other errors", flush=True)

    at, of = read_names()
    units_sorted = sorted((it for it in items if it["mod"] == "exe"), key=lambda it: it["lo"])
    ulo = [it["lo"] for it in units_sorted]

    def unit_at(rva):
        k = bisect.bisect_right(ulo, rva) - 1
        return units_sorted[k] if k >= 0 and rva < units_sorted[k]["hi"] else None
    imported = {i.name for _, imps in exe_imports for i in imps}
    engine_exports = {ln.rsplit(" @", 1)[0] for ln in (LINK_DIR / "lle" / "exports.txt").read_text(encoding="utf-8")
                      .splitlines() if ln.strip() and not ln.startswith("#")} - imported
    ctx = {"at": at, "of": of, "defined": defined, "by_scope": by_scope, "unit_at": unit_at, "fails": fails,
           "engine_exports": engine_exports, "alias": {}}
    inferred = infer_addresses(unres1, game, at, of, read_vtable_slots())
    ctx["inferred"] = inferred
    thunks = read_import_thunks()

    def thunk_at(rva):
        return thunks.get(rva)
    ctx["thunk_at"] = thunk_at
    cls1 = {}
    for s in unres1:
        is_func = refs[s][0] if s in refs else not s.startswith("??_7")
        cls1[s] = classify(s, is_func, ctx) + (is_func,)

    dup_tab = {n: v for n, v in dup_pre.items() if len(v) > 1 and any(sel in (0, 1) for _, sel in v)}
    report = {"pass1": {"exit": code1, "unresolved": {s: {"class": c, "detail": d, "function": f, "refs": unres1[s]}
                                                      for s, (c, d, f) in sorted(cls1.items())},
                        "duplicates_link": dups1, "other": other1},
              "duplicates_symtab": {n: v for n, v in sorted(dup_tab.items())}}
    counts1 = collections.Counter(c for c, _, _ in cls1.values())
    print("pass 1 unresolved by class: " + ", ".join(f"{k} {counts1[k]}" for k in CLASSES if counts1[k]))

    stub_info = {}
    values, by3_obj, data_stub_rvas = {}, {}, {}
    if stubs_pass:
        bounds = sorted(set(at))
        stubs = Stubs(initialised)
        stubbed = set(unres1)

        def resolve(tgt):
            names = [n for n in at.get(tgt, ()) if n in defined or n in stubbed]
            if names:
                return names[0], 0
            return None, 0

        kinds = collections.Counter()
        fallback = collections.Counter()
        engine = set()
        members = read_members(members, at, lambda n: n in defined or n in stubbed) if members else {}
        if member_offsets:
            for alias, (cont, off, va) in list(members.items()):
                new = member_offsets(cont, off)
                members[alias] = (cont, new, va - off + new)
        member_alias = {}
        data_stub_rvas = {}
        for s, (c, d, is_func) in sorted(cls1.items()):
            rv = sorted(of.get(s, ())) or ([inferred[s][0]] if s in inferred else [])
            if c == "engine":
                engine.add(s[6:] if s.startswith("__imp_") else s)
                kinds["engine import"] += 1
                continue
            if s in LIB_SHIMS:
                stubs.shim(s, *LIB_SHIMS[s])
                kinds["library shim"] += 1
                continue
            if s in ctx["alias"]:
                if is_func and not s.startswith("??_7"):
                    stubs.alias(s, ctx["alias"][s])
                    kinds["alias " + c] += 1
                else:
                    stubs.o.weak(s, ctx["alias"][s])
                    kinds["data alias (weak external) " + c] += 1
                continue
            if is_func and not s.startswith("??_7"):
                if c == "won":
                    kinds["won " + stubs.won(s)] += 1
                else:
                    stubs.trap(s, f"{c}: {d}")
                    kinds["trap " + c] += 1
                continue
            if s in members and not is_func:
                member_alias[s] = members[s]
                stubs.o.define(s, -1, 0)
                kinds["member alias (absolute)"] += 1
                continue
            rva = next((r for r in rv if RDATA[0] <= r < DATA[1]), None)
            size = 0
            if rva is not None:
                k = bisect.bisect_right(bounds, rva)
                nxt = bounds[k] if k < len(bounds) else DATA[1]
                sec_end = RDATA[1] if rva < RDATA[1] else DATA_RAW_END if rva < DATA_RAW_END else DATA[1]
                size = max(4, min(nxt, sec_end, rva + 0x10000) - rva)
            slots = 64 if s.startswith("??_7") else 0
            data_stub_rvas[s] = rva
            fallback.update(stubs.data(s, rva, size, resolve, slots))
            kinds["data " + (("carved" if rva < DATA_RAW_END else "zero bss") if rva is not None
                             else "vtable-trap" if slots else "zero")] += 1
        stubs.o.write(out / "stubs.obj")
        (out / "trap_ids.txt").write_text("".join(f"{i}\t{0xEE700000 + i:#x}\t{s}\t{w}\n" for i, s, w in stubs.ids))
        stub_info = {"kinds": dict(kinds), "data_fallbacks": dict(fallback), "traps": len(stubs.ids)}
        print("stubs: " + ", ".join(f"{k} {v}" for k, v in sorted(kinds.items())) +
              (("; data pointer fallbacks: " + ", ".join(f"{k} {v}" for k, v in fallback.items())) if fallback
               else ""), flush=True)

        if engine:
            print(f"exe: {len(engine)} engine symbols unresolved (no engine DLL any more): {sorted(engine)[:5]}")
        code2, log2 = link(out, objs + [out / "stubs.obj"], libs, res, debug, "pass2")
        unres2, dups2, other2 = parse_log(log2)
        print(f"pass 2: link exit {code2}: {len(unres2)} unresolved, {len(dups2)} duplicates, "
              f"{len(other2)} other errors -> {out / 'EE-AOC.exe' if code2 == 0 else '(no image)'}", flush=True)
        report["pass2"] = {"exit": code2, "unresolved": unres2, "duplicates_link": dups2, "other": other2}
        if member_alias and code2 == 0:
            report["member_alias"], values, by3_obj = member_alias_pass(out, objs, libs, res, debug, stubs,
                                                                         member_alias)
    report["stubs"] = stub_info
    report["implib_const_data_fixed"] = undata
    report["counts1"] = counts1
    return report, {"ok": ok, "img": img, "objs": objs, "values": values, "by3_obj": by3_obj,
                    "data_stub_rvas": data_stub_rvas, "of": of, "defined": defined, "patched": dedupe}
