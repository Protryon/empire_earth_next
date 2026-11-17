"""The crate restructure (docs/progress/crate-restructure.md): today's ~40 crates under rust/crates into the owner's
layout (ee_base, ee_data, ee_sim, ee_render, ee_audio, ee_net, ee_ui, ee_won*, ee_game), mechanically.

  python -I tools/crate_move.py analyze [--markdown OUT]   every cross-crate path, the new layout's edges, rule breaks
  python -I tools/crate_move.py won [--markdown OUT]       ee_won's modules as net / gui / lobby and the edges between
  python -I tools/crate_move.py headers [--markdown OUT]   the per-module header plan: units, their items, their cycles
  python -I tools/crate_move.py move [--dry-run] [--step S] the move itself (S: crates, math, all); see `move -h`

Pure and read only except `move`. Rust sources are read through a lexer that blanks comments, strings and chars
(offsets kept), so paths in prose are never counted or rewritten; the move edits code only, then rustfmt.
"""
import argparse
import collections
import pathlib
import re
import subprocess
import sys
import tomllib

IDENT = r"[A-Za-z_][A-Za-z0-9_]*"
ROOT = pathlib.Path(__file__).resolve().parent.parent
# where the sources are read: the checkout, or (`move`) a snapshot of the commit before the restructure
SRC_ROOT = ROOT
CRATES = ROOT / "rust" / "crates"


def set_source_root(root):
    global SRC_ROOT, CRATES
    SRC_ROOT = pathlib.Path(root).resolve()
    CRATES = SRC_ROOT / "rust" / "crates"
    _split_cache.clear()
    _reexport_cache.clear()

# ---------------------------------------------------------------------------------------------------------------
# The layout: old crate -> (new crate, module path of its root in the new crate). [] = the new crate's root itself.
LAYOUT = {
    # ee_base: the platform, the leaves, the files, the input, and the math out of ee_ge (SPLITS)
    "ee_plat_core": ("ee_base", ["plat_core"]),
    "ee_plat_sys": ("ee_base", ["plat_sys"]),
    "ee_plat_ui": ("ee_base", ["plat_ui"]),
    "ee_plat_sdl": ("ee_base", ["plat_sdl"]),
    "ee_util": ("ee_base", ["util"]),
    "ee_error": ("ee_base", ["error"]),
    "ee_panic": ("ee_base", ["panic"]),
    "ee_random": ("ee_base", ["random"]),
    "ee_time": ("ee_base", ["time"]),
    "ee_ts": ("ee_base", ["ts"]),
    "ee_ustring": ("ee_base", ["ustring"]),
    "ee_codec": ("ee_base", ["codec"]),
    "ee_fs": ("ee_base", ["fs"]),
    "ee_input": ("ee_base", ["input"]),
    # ee_data: unchanged (ee_data_cabi becomes its staticlib crate type)
    "ee_data": ("ee_data", []),
    "ee_data_cabi": ("ee_data", None),     # dissolved: see DISSOLVED
    # ee_sim
    "ee_units": ("ee_sim", ["units"]),
    "ee_combat": ("ee_sim", ["combat"]),
    "ee_astar": ("ee_sim", ["astar"]),
    "ee_formations": ("ee_sim", ["formations"]),
    "ee_rmap": ("ee_sim", ["rmap"]),
    "ee_tech": ("ee_sim", ["tech"]),
    "ee_scenario": ("ee_sim", ["scenario"]),
    "ee_ai": ("ee_sim", ["ai"]),
    "ee_netmsg": ("ee_sim", ["netmsg"]),
    "ee_rmap_gatelib": ("ee_rmap_gatelib", []),   # stays a tooling staticlib (see the plan: no C++ imports pulled)
    # ee_render
    "ee_ge": ("ee_render", ["ge"]),
    "ee_gfx": ("ee_render", ["gfx"]),
    "ee_gx": ("ee_render", ["gx"]),
    "ee_dx7": ("ee_render", ["dx7"]),
    "ee_openformats": ("ee_render", ["openformats"]),
    # ee_audio, ee_net, ee_ui
    "ee_sound": ("ee_audio", ["sound"]),
    "ee_mixer": ("ee_audio", ["mixer"]),
    "ee_net": ("ee_net", ["net"]),
    "ee_net_tcp": ("ee_net", ["net_tcp"]),
    "ee_ui": ("ee_ui", []),
    # ee_won: see WON (one crate, three modules; or three crates with --won-crates)
    "ee_won": ("ee_won", []),
    # ee_game: the screens, the rest of ee_app, and ee_rust's own items at its root
    "ee_screens": ("ee_game", ["screens"]),
    "ee_app": ("ee_game", ["app"]),
    "ee_rust": ("ee_game", []),
    "xtask": ("xtask", []),
}
NEW_CRATES = ["ee_base", "ee_data", "ee_sim", "ee_render", "ee_audio", "ee_net", "ee_ui", "ee_won", "ee_game",
              "ee_rmap_gatelib", "xtask"]
# the layering: a new crate may use only these (besides itself)
ALLOWED = {
    "ee_base": set(),
    "ee_data": {"ee_base"},
    "ee_sim": {"ee_base", "ee_data"},
    "ee_render": {"ee_base", "ee_data", "ee_sim"},
    "ee_audio": {"ee_base", "ee_data"},
    "ee_net": {"ee_base"},
    "ee_ui": {"ee_base", "ee_data", "ee_render", "ee_audio"},
    "ee_won": {"ee_base"},
    "ee_won_net": {"ee_base"},
    "ee_won_gui": {"ee_base", "ee_won_net"},
    "ee_won_lobby": {"ee_base", "ee_won_net", "ee_won_gui"},
    "ee_game": set(NEW_CRATES) - {"ee_game", "xtask", "ee_rmap_gatelib"} | {"ee_won_net", "ee_won_gui", "ee_won_lobby"},
    "ee_rmap_gatelib": {"ee_base", "ee_sim"},
    "xtask": set(),
}

# Old crates whose top modules go to different places: old crate -> {top module: (new crate, new module path)}.
# Top modules not listed follow LAYOUT. A `_tests` / `_ffi` sibling follows its module unless listed itself.
MATH = ("types", "um", "transform", "hull", "vector", "point", "line", "bbox")
SPLITS = {
    # the GE math cluster: these eight use only each other; ffi.rs is cut at FILE_SPLITS
    "ee_ge": {m: ("ee_base", ["math", m]) for m in MATH},
    # map_ffi stays in ee_game: it edits the GE terrain mesh (ee_ge::tmobj, render)
    "ee_app": {m: ("ee_sim", ["app", m]) for m in ("map", "map_tests", "storm", "storm_ffi", "storm_tests",
                                                   "cheats", "cheats_ffi", "cheats_tests")},
    # the PNG reader (stb_image's port) goes down for ee_won's image decoder; it uses only Failure and fail
    "ee_openformats": {"png": ("ee_base", ["png"])},
}
# files cut in two at a line: (old crate, top module) -> (first line of the second part, first part's (crate,
# module path), second part's)
FILE_SPLITS = {
    # ee_ge's C ABI: the math (UMath, the points, vectors, lines, planes, transforms, boxes) above the line, the
    # terrain mesh's queries and the physical model's triangle tests below
    ("ee_ge", "ffi"): ("// ---- GETerrainMesh's queries ----", ("ee_base", ["math", "ffi"]), ("ee_render", ["ge", "ffi"])),
}
# single items moved out of a module: (old crate, top module or None, item name) -> (new crate, module path)
ITEM_MOVES = {
    ("ee_openformats", None, "Failure"): ("ee_base", ["png"]),
    ("ee_openformats", None, "fail"): ("ee_base", ["png"]),
}
# new modules the move creates to hold a split's parts: (new crate, module path) -> its doc line
SPLIT_PARENTS = {
    ("ee_base", ("math",)): "The Graphics Engine's math (from ee_ge): the GE value types, the table trigonometry (um), "
                            "the transforms and the hull.",
    ("ee_sim", ("app",)): "The simulation parts of the app and world lane (from ee_app): the map, the storms, the "
                          "cheats.",
}

# ee_won's modules by the WON library part they port. net: WONCommon, WONSocket, WONCrypt, WONAuth, WONServer,
# WONRouting, WONDir, WONDB, WONMisc, WONInternal, WONConfig, WONFile, WONStatus; gui: WONGUI (and its platform layer,
# the skins, the image decoders); lobby: WONLobby and the EE lobby glue. `_tests` / `_golden` follow their module.
WON_GROUPS = {
    "net": """admin adminop asyncop auth authcpp authctx authflow authlogin authobj bigint blowfish blowfish_tables
        buffer buffer_golden cachemap cdkey cdkeystore cert certhost config config_golden configobj cppcore crc16 db
        debug der dir dirchange dirops elgamal ffi ffi_net ffi_ops ffi_parse file file_golden filepush ftp ftpop fwhost
        fwop gamespy getcert golden hostauth hostbase hostcore hostnative hostop hostsock http httpcache httpcpp httpdoc
        httpengine httpget ipaddr ipobj md5 miscops multiping netstats opobj ops parser parser_golden patch path peerauth
        peerobj peerop perftimer qsock random regauth regkey reqdir reqhost reqlast reqmake reqop reqops routeconn
        routecpp routemsg routeop servconn servctx server smartroute socket sockobj sockop sockops sockthread status
        strcase text thread titlecfg udpmgr udpops userstatus verop wire wonfile
        chat plaintext""",
    "gui": """barobj btnobj cfgimg cfgtab comboobj compmem compobj ctobj dialogobj fontmap gfxobj grabobj guicfg guiconf
        guictl guidraw guiedit guievent guifocus guihost guilayout guilist guimenu guimulti guiscroll guistring
        guistrobj guitext guitree guiwin guiwrap imgdec imgfilter inputobj jpeg labelobj layoutobj listobj menuobj
        msbtnobj mspaint multiobj paneobj platcursor platfont platgfx platimg platwin platwm rootobj
        scrollerobj skinlay splitobj stretchobj surface tabobj textobj treeobj winobj wmcache wmloop wmobj wonbtnobj
        wrapobj""",
    "lobby": """chat_golden chatobj dialogs dlglogic eecolor eegame eeglue eeresource eesound eexfer ffi_lan flow lan
        lanlogic lanobj lanown lobbyctl lobbydlg lobbyhost lobbymap lobbypopup logics model modelobj persist
        prefsobj resource routelogic routing scrolltext staging stagingobj statusstr""",
}


def load_won_groups():
    WON_GROUP.clear()
    for group, names in WON_GROUPS.items():
        for n in names.split():
            WON_GROUP[n] = group
DISSOLVED = {"ee_data_cabi": "ee_data's own staticlib crate type", "ee_rust": "ee_game's root"}

EXTERNAL_ROOTS = {"std", "core", "alloc", "libc", "windows_sys", "wgpu", "naga", "log", "parking_lot",
                  "raw_window_handle", "mlua_sys", "cbindgen", "cc", "proc_macro"}


# ---------------------------------------------------------------------------------------------------------------
# The lexer: comments, strings and char literals blanked to spaces (newlines kept), offsets unchanged.
LEX = re.compile(r"""
    (?P<line>//[^\n]*)
  | (?P<block>/\*)
  | (?P<raw>(?<![A-Za-z0-9_])[bc]?r(?P<hashes>\#*)")
  | (?P<str>(?<![A-Za-z0-9_])[bc]?"(?:\\.|[^"\\])*")
  | (?P<chr>(?<![A-Za-z0-9_])b?'(?:\\(?:x[0-9a-fA-F]{2}|u\{[0-9a-fA-F]+\}|.)|[^\\'\n])')
""", re.X | re.S)


def blank(s):
    return re.sub(r"[^\n]", " ", s)


def strip_code(text):
    """The text with comments, strings and chars blanked; same length, same newlines."""
    out, pos = [], 0
    while True:
        m = LEX.search(text, pos)
        if not m:
            out.append(text[pos:])
            break
        out.append(text[pos:m.start()])
        if m.group("block"):
            depth, i = 1, m.end()
            while depth and i < len(text):
                if text.startswith("/*", i):
                    depth, i = depth + 1, i + 2
                elif text.startswith("*/", i):
                    depth, i = depth - 1, i + 2
                else:
                    i += 1
            out.append(blank(text[m.start():i]))
            pos = i
        elif m.group("raw"):
            close = '"' + m.group("hashes")
            end = text.find(close, m.end())
            end = len(text) if end < 0 else end + len(close)
            out.append(blank(text[m.start():end]))
            pos = end
        else:
            # keep the delimiters of a string, so `include_str!("x")` stays recognisable as a call with a string
            out.append(blank(m.group(0)))
            pos = m.end()
    return "".join(out)


# ---------------------------------------------------------------------------------------------------------------
# Crates, their files and their module trees.
class SourceFile:
    def __init__(self, krate, path, module, root_kind):
        self.krate = krate            # old crate name
        self.path = path              # absolute path
        self.module = module          # module path in the old crate, e.g. ["damage", "tests"]
        self.root_kind = root_kind    # "lib", "bin", "example", "test", "build"
        raw = path.read_bytes().decode("utf-8")
        self.crlf = "\r\n" in raw
        self.text = apply_fixups(self.rel(), raw.replace("\r\n", "\n"))
        self.code = strip_code(self.text)

    @property
    def top(self):
        return self.module[0] if self.module else None

    def rel(self):
        """The path in the repository (the snapshot mirrors it)."""
        return self.path.relative_to(SRC_ROOT).as_posix()


MOD_DECL = re.compile(r"(?<![A-Za-z0-9_])mod\s+(r#)?([A-Za-z_][A-Za-z0-9_]*)\s*([;{])")
PATH_ATTR = re.compile(r'#\[\s*path\s*=\s*"([^"]+)"\s*\]\s*$')


def inline_modules(code):
    """(start, end, name) of the inline `mod name { ... }` blocks of a file, nested ones included."""
    out = []
    for m in MOD_DECL.finditer(code):
        if m.group(3) != "{":
            continue
        depth, i = 1, m.end()
        while depth and i < len(code):
            c = code[i]
            depth += (c == "{") - (c == "}")
            i += 1
        out.append((m.start(), i, m.group(2)))
    return out


def enclosing_inline(inlines, pos):
    """The names of the inline modules around an offset, outermost first."""
    return [name for (s, e, name) in sorted(inlines) if s < pos < e]


def walk_modules(krate, file, module, is_mod_rs, root_kind, out):
    sf = SourceFile(krate, file, module, root_kind)
    out.append(sf)
    inlines = inline_modules(sf.code)
    for m in MOD_DECL.finditer(sf.code):
        if m.group(3) != ";":
            continue
        name = m.group(2)
        inner = enclosing_inline(inlines, m.start())
        line_start = sf.text.rfind("\n", 0, m.start()) + 1
        before = sf.text[:line_start].rstrip("\n").rsplit("\n", 3)
        attr = None
        for prev in reversed(before[-3:]):
            pm = PATH_ATTR.search(prev.strip())
            if pm:
                attr = pm.group(1)
                break
            if not prev.strip().startswith("#["):
                break
        if attr:
            base = file.parent if not inner else (file.parent if is_mod_rs else file.parent / file.stem) / "/".join(inner)
            child = (base / attr).resolve()
            child_is_mod_rs = child.name == "mod.rs"
        else:
            base = file.parent if is_mod_rs else file.parent / file.stem
            for n in inner:
                base = base / n
            child = base / f"{name}.rs"
            child_is_mod_rs = False
            if not child.is_file():
                child = base / name / "mod.rs"
                child_is_mod_rs = True
        if not child.is_file():
            print(f"warning: {sf.rel()}: mod {name} has no file", file=sys.stderr)
            continue
        walk_modules(krate, child, module + inner + [name], child_is_mod_rs, root_kind, out)


def manifest(krate):
    return tomllib.loads((CRATES / krate / "Cargo.toml").read_text(encoding="utf-8"))


def crate_files(krate):
    """Every Rust source of an old crate: the library's module tree, its bins, examples, tests and build script."""
    d = CRATES / krate
    toml = manifest(krate)
    out = []
    lib = d / toml.get("lib", {}).get("path", "src/lib.rs")
    if lib.is_file():
        walk_modules(krate, lib, [], True, "lib", out)
    main = d / "src" / "main.rs"
    if main.is_file():
        walk_modules(krate, main, [], True, "bin", out)
    bins = {(d / b["path"]).resolve() for b in toml.get("bin", []) if "path" in b}
    bins |= {p.resolve() for p in (d / "src" / "bin").glob("*.rs")}
    for b in sorted(bins):
        walk_modules(krate, b, [f"<bin {b.stem}>"], True, "bin", out)
    for kind, sub in (("example", "examples"), ("test", "tests")):
        for f in sorted((d / sub).glob("*.rs")):
            walk_modules(krate, f, [f"<{kind} {f.stem}>"], True, kind, out)
    if (d / "build.rs").is_file():
        out.append(SourceFile(krate, d / "build.rs", ["<build>"], "build"))
    return out


def old_crates():
    return sorted(p.name for p in CRATES.iterdir() if (p / "Cargo.toml").is_file())


def top_modules(files):
    """crate -> the set of its top-level module names (from its library tree)."""
    tops = collections.defaultdict(set)
    for f in files:
        if f.root_kind == "lib" and len(f.module) >= 1:
            tops[f.krate].add(f.module[0])
    return tops


# ---------------------------------------------------------------------------------------------------------------
# Destinations.
WON_GROUP = {}        # ee_won top module -> "net" | "gui" | "lobby" (filled by load_won_groups)
WON_AS_CRATES = True  # three crates ee_won_net/gui/lobby (the plan's choice), else three modules of one ee_won

# The steps of the move, in commit order. A crate step moves every old crate of that new crate whole; "won" splits
# ee_won into its three crates; "down" applies SPLITS, FILE_SPLITS and ITEM_MOVES (the math, the PNG reader, the
# simulation parts of ee_app). The tree of a state is always made from the snapshot, with the steps up to it active.
STEPS = ["base", "data", "audio", "net", "render", "sim", "game", "won", "down"]
STEP_OF_CRATE = {"ee_base": "base", "ee_data": "data", "ee_audio": "audio", "ee_net": "net", "ee_render": "render",
                 "ee_sim": "sim", "ee_game": "game", "ee_won": "won"}
ACTIVE = set(STEPS)


def set_active(upto):
    """Activates the steps up to and including `upto`."""
    ACTIVE.clear()
    ACTIVE.update(STEPS[:STEPS.index(upto) + 1])


def unmoved(krate, top):
    return krate, ([top] if top is not None else [])


def won_group(top):
    base = top
    while base not in WON_GROUP and base.rsplit("_", 1)[-1] in ("tests", "golden") and "_" in base:
        base = base.rsplit("_", 1)[0]
    if base not in WON_GROUP:
        raise SystemExit(f"ee_won module {top} has no group in WON_GROUPS")
    return WON_GROUP[base]


def won_dest(top):
    group = won_group(top) if top else None
    if top is None:
        return ("ee_won_net", []) if WON_AS_CRATES else ("ee_won", [])
    if WON_AS_CRATES:
        return (f"ee_won_{group}", [top])
    return ("ee_won", [group, top])


def dest(krate, top):
    """Where an old crate's top module (None: its root, lib.rs) goes: (new crate, its new module path), in the state
    of the active steps."""
    step = STEP_OF_CRATE.get(LAYOUT[krate][0])
    if step is None or step not in ACTIVE:
        return unmoved(krate, top)
    if krate == "ee_won":
        return won_dest(top) if WON_GROUP else unmoved(krate, top)
    split = SPLITS.get(krate, {}) if "down" in ACTIVE else {}
    if top is not None and top in split:
        n, p = split[top]
        return n, list(p)
    if top is not None and top.endswith(("_tests", "_golden")):
        base = re.sub(r"_(tests|golden)$", "", top)
        if base in split:
            n, p = split[base]
            return n, p[:-1] + [top]
    n, pre = LAYOUT[krate]
    if pre is None:
        return n, []
    return n, pre + ([top] if top is not None else [])


ITEM_DEF = re.compile(r"^[ \t]*(?:pub(?:\([^)]*\))?\s+)?(?:unsafe\s+)?(?:extern\s+(?:\"C\"\s+)?)?(?:const\s+)?"
                      r"(?:fn|struct|enum|union|type|const|static|trait|macro_rules!)\s+(?:mut\s+)?(" + IDENT + ")", re.M)
_split_cache = {}


def file_split_parts(krate, top):
    """For a file of FILE_SPLITS: (offset of its cut, names defined above it, names defined below it)."""
    key = (krate, top)
    if key not in _split_cache:
        marker = FILE_SPLITS[key][0]
        path = CRATES / krate / "src" / f"{top}.rs"
        text = path.read_text(encoding="utf-8")
        cut = text.index(marker)
        names = [(m.start(), m.group(1)) for m in ITEM_DEF.finditer(text)]
        _split_cache[key] = (cut, {n for o, n in names if o < cut}, {n for o, n in names if o >= cut})
    return _split_cache[key]


_reexport_cache = {}


def root_reexports(krate):
    """A crate root's `pub use child::...::Name` re-exports: Name -> [child, ..., Name] (globs left out)."""
    if krate not in _reexport_cache:
        text = (CRATES / krate / "src" / "lib.rs").read_text(encoding="utf-8")
        code = strip_code(text)
        out = {}
        for start, end in use_spans(code):
            if not re.search(r"pub(\([^)]*\))?\s+use\s+$", code[max(0, start - 20):start]):
                continue
            for segs in parse_use_tree(code[start:end]):
                if segs and segs[-1] != "*" and segs[0] not in ("crate", "self", "super") and segs[0] not in LAYOUT:
                    leaf = segs[-1].split(" as ")
                    out[leaf[-1]] = segs[:-1] + [leaf[0]]
        _reexport_cache[krate] = out
    return _reexport_cache[krate]


def target_dest(krate, top, rest):
    """Where the item a path names goes: its module's destination, unless the item itself moves (ITEM_MOVES) or
    its file is cut in two (FILE_SPLITS)."""
    name = rest[0].split(" as ")[0] if rest else None
    if "down" not in ACTIVE:
        return dest(krate, top)
    if top is None and name and krate in SPLITS:
        moved = root_reexports(krate).get(name)
        # followed only into a module the split takes to another crate (the rest keep the root's re-export)
        if moved and moved[0] in SPLITS[krate]:
            return target_dest(krate, moved[0], moved[1:])
    if name and (krate, top, name) in ITEM_MOVES:
        n, p = ITEM_MOVES[(krate, top, name)]
        return n, list(p)
    if (krate, top) in FILE_SPLITS:
        _, first, second = FILE_SPLITS[(krate, top)]
        _, above, below = file_split_parts(krate, top)
        part = second if name in below and name not in above else first
        return part[0], list(part[1])
    return dest(krate, top)


def file_dest(f, offset=0):
    """A file's new crate and full module path (for a cut file, of its part at `offset`)."""
    if f.root_kind != "lib":
        return dest(f.krate, None)[0], f.module
    if "down" in ACTIVE and (f.krate, f.top) in FILE_SPLITS and len(f.module) == 1:
        cut = f.text.index(FILE_SPLITS[(f.krate, f.top)][0])
        n, p = FILE_SPLITS[(f.krate, f.top)][1 if offset < cut else 2]
        return n, list(p)
    n, p = dest(f.krate, f.top)
    return n, p + f.module[1:]


# ---------------------------------------------------------------------------------------------------------------
# Paths in code.
USE = re.compile(r"(?<![A-Za-z0-9_])use\s+")
USE_STMT = re.compile(r"(?<![A-Za-z0-9_])use\s+[^;]*;")
PATH = re.compile(r"(?<![A-Za-z0-9_:$])(::\s*)?(" + IDENT + r")((?:\s*::\s*(?:" + IDENT + r"|\*))+)")


def use_spans(code):
    """(start, end) of each `use ...;` tree (from after `use` to the `;`)."""
    for m in USE.finditer(code):
        depth, i = 0, m.end()
        while i < len(code):
            c = code[i]
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
            elif c == ";" and depth == 0:
                break
            i += 1
        yield m.end(), i


def parse_use_tree(s):
    """A use tree's text into its leaf paths, each a list of segments (`self`, `*`, `x as y` kept as segments)."""
    s = re.sub(r"\s+", " ", s).strip()
    pos = 0

    def tree(prefix):
        nonlocal pos
        out, seg = [], []
        while pos < len(s):
            c = s[pos]
            if c == " ":
                pos += 1
            elif s.startswith("::", pos):
                pos += 2
            elif c == "{":
                pos += 1
                out.extend(group(prefix + seg))
                seg = None
                return out
            elif c in ",}":
                break
            else:
                m = re.match(r"(" + IDENT + r"|\*)(\s+as\s+" + IDENT + r")?", s[pos:])
                if not m:
                    pos += 1
                    continue
                seg.append(m.group(1) + (m.group(2) or ""))
                pos += m.end()
        if seg is not None:
            out.append(prefix + seg)
        return out

    def group(prefix):
        nonlocal pos
        out = []
        while pos < len(s):
            if s[pos] == "}":
                pos += 1
                return out
            if s[pos] in ", ":
                pos += 1
                continue
            out.extend(tree(prefix))
        return out

    return tree([])


class Ref:
    """One path in code that names another crate's or module's item."""

    def __init__(self, file, offset, segs, in_use):
        self.file, self.offset, self.segs, self.in_use = file, offset, segs, in_use

    @property
    def line(self):
        return self.file.text.count("\n", 0, self.offset) + 1


def resolve(file, segs, tops, inline):
    """An absolute (old crate, top module or None, rest) for a path, or None for std/external/local names."""
    if not segs:
        return None
    head = segs[0].split(" as ")[0]
    module = file.module + inline
    if head in ("crate", "$crate"):
        krate, rest = file.krate, segs[1:]
    elif head in ("self", "super"):
        cur = list(module)
        i = 0
        while i < len(segs) and segs[i].split(" as ")[0] in ("self", "super"):
            if segs[i] == "super":
                if not cur:
                    return None
                cur.pop()
            i += 1
        krate, rest = file.krate, cur + segs[i:]
    elif head in LAYOUT:
        krate, rest = head, segs[1:]
    else:
        return None
    if file.root_kind != "lib" and krate == file.krate and head in ("crate", "self", "super"):
        return None   # inside a bin/example/test root: its own modules
    top = None
    if rest:
        first = rest[0].split(" as ")[0]
        if first in tops.get(krate, ()):
            top = first
            rest = rest[1:]
    return krate, top, rest


def collect_refs(file, tops):
    """Every path of a file that resolves into an ee_* crate (its own crate included), with its offset."""
    if getattr(file, "_refs", None) is None:
        file._refs = _collect_refs(file, tops)
    return file._refs


def _collect_refs(file, tops):
    code = file.code
    inlines = inline_modules(code)
    refs, covered = [], []
    for start, end in use_spans(code):
        covered.append((start, end))
        for segs in parse_use_tree(code[start:end]):
            segs = [x for x in segs if x]
            if not segs:
                continue
            if segs[0] not in ("crate", "self", "super", "$crate") and segs[0].split(" as ")[0] not in LAYOUT:
                # 2018 uniform paths: a relative `use child::x` names a child module of the current one
                inner = enclosing_inline(inlines, start)
                mod = file.module + inner
                if file.root_kind == "lib" and len(mod) == 0 and segs[0] in tops.get(file.krate, ()):
                    segs = ["crate"] + segs
                else:
                    continue
            refs.append(Ref(file, start, segs, True))
    for m in PATH.finditer(code):
        if any(s <= m.start() < e for s, e in covered):
            continue
        head = m.group(2)
        if head not in LAYOUT and head not in ("crate", "self", "super"):
            continue
        segs = [head] + [x.strip() for x in m.group(3).split("::") if x.strip()]
        refs.append(Ref(file, m.start(2), segs, False))
    # `$crate` in macro bodies
    for m in re.finditer(r"\$crate((?:\s*::\s*" + IDENT + r")+)", code):
        segs = ["$crate"] + [x.strip() for x in m.group(1).split("::") if x.strip()]
        refs.append(Ref(file, m.start(), segs, False))
    return refs, inlines


# ---------------------------------------------------------------------------------------------------------------
# extern "C" bridges between Rust crates (the linker's edges cargo does not see)
NO_MANGLE_FN = re.compile(r"#\[no_mangle\][^;{]*?\bfn\s+(" + IDENT + r")", re.S)
EXTERN_BLOCK = re.compile(r'extern\s+"C"\s*\{')   # matched on the text, checked against the code


def exported_symbols(files):
    """The `#[no_mangle]` functions and statics of the library code (test and example stand-ins left out)."""
    out = {}
    for f in files:
        if f.root_kind != "lib" or is_test_file(f):
            continue
        for m in NO_MANGLE_FN.finditer(f.code):
            out.setdefault(m.group(1), f)
        for m in re.finditer(r"#\[no_mangle\]\s*(?:pub\s+)?static\s+(?:mut\s+)?(" + IDENT + r")", f.code):
            out.setdefault(m.group(1), f)
    return out


def imported_symbols(f):
    """(name, offset) of every function or static an `extern "C" { }` block of the file declares."""
    for m in EXTERN_BLOCK.finditer(f.text):
        if f.code[m.start()] == " ":
            continue   # in a comment or a string
        depth, i = 1, m.end()
        while depth and i < len(f.code):
            depth += (f.code[i] == "{") - (f.code[i] == "}")
            i += 1
        body = f.code[m.end():i - 1]
        for d in re.finditer(r"\b(?:fn|static(?:\s+mut)?)\s+(" + IDENT + r")", body):
            yield d.group(1), m.end() + d.start()


# ---------------------------------------------------------------------------------------------------------------
# analyze
def all_files():
    files = []
    for c in old_crates():
        files.extend(crate_files(c))
    return files


def is_test_file(f, inline=()):
    return (f.root_kind in ("test", "example") or any(p.endswith(("_tests", "_golden")) or p in ("tests", "test") or
                                                       p.startswith("golden") for p in f.module + list(inline)))


def dependency_kind(f, ref_offset, inlines):
    inner = enclosing_inline(inlines, ref_offset)
    if f.root_kind == "build":
        return "build"
    if is_test_file(f, inner):
        return "dev"
    cfg_test = re.compile(r"#\[cfg\(test\)\]")
    # a `#[cfg(test)]` item right before the ref's line
    line_start = f.code.rfind("\n", 0, ref_offset) + 1
    prev = f.code[max(0, line_start - 200):line_start]
    if cfg_test.search(prev.rsplit("\n", 2)[-2] if "\n" in prev else prev):
        return "dev"
    return "normal"


def use_parts(f, r):
    """The offsets standing for the parts of a cut file (FILE_SPLITS) a ref belongs to: a `use` belongs to each part
    whose code names what it imports; anything else to the part it is in."""
    if "down" not in ACTIVE or (f.krate, f.top) not in FILE_SPLITS or len(f.module) != 1 or not r.in_use:
        return [r.offset]
    cut = f.text.index(FILE_SPLITS[(f.krate, f.top)][0])
    leaf = r.segs[-1].split(" as ")[-1]
    if leaf in ("*", "self"):
        leaf = r.segs[-2] if leaf == "self" and len(r.segs) > 1 else None
    if leaf is None:
        return [0, cut]
    body = USE_STMT.sub(lambda m: blank(m.group(0)), f.code)
    out = []
    if re.search(r"\b" + re.escape(leaf) + r"\b", body[:cut]):
        out.append(0)
    if re.search(r"\b" + re.escape(leaf) + r"\b", body[cut:]):
        out.append(cut)
    return out


def crate_key(n, p):
    return n


def won_key(n, p):
    """ee_won's three groups as units (the rest by crate)."""
    if n.startswith("ee_won_"):
        return "won::" + n[len("ee_won_"):]
    return f"won::{p[0]}" if n == "ee_won" and p else n


def edges_of(files, tops, key=crate_key):
    """(from unit, to unit) -> list of (file, line, path text, kind, target old crate, target top module); a unit is
    a new crate (or what `key` makes of a new crate and module path)."""
    edges = collections.defaultdict(list)
    for f in files:
        refs, inlines = collect_refs(f, tops)
        for r in refs:
            res = resolve(f, r.segs, tops, enclosing_inline(inlines, r.offset))
            if not res:
                continue
            krate, top, rest = res
            if krate not in LAYOUT:
                continue
            b = key(*target_dest(krate, top, rest))
            for at in use_parts(f, r):
                a = key(*file_dest(f, at))
                if a == b:
                    continue
                edges[(a, b)].append((f, r.line, "::".join(r.segs), dependency_kind(f, r.offset, inlines), krate, top))
    return edges


def link_edges(files):
    exported = exported_symbols(files)
    out = []
    for f in files:
        for name, off in imported_symbols(f):
            g = exported.get(name)
            if g is None or g is f:
                continue
            fn, _ = file_dest(f, off)
            gn, _ = file_dest(g)
            if (f.krate, f.top) != (g.krate, g.top):
                out.append((f, f.text.count("\n", 0, off) + 1, name, g, fn, gn))
    return out


def cfg_test_sites(files):
    """`cfg(test)` alternatives in library code (not in a test module): their behaviour changes for a sibling
    module's tests once two old crates share a crate (cfg(test) is per crate)."""
    out = []
    pat = re.compile(r"#\[cfg\((not\()?test\)?\)\]|cfg!\((not\()?test\)?\)|#\[cfg_attr\(test")
    for f in files:
        if f.root_kind != "lib" or is_test_file(f):
            continue
        inlines = inline_modules(f.code)
        for m in pat.finditer(f.code):
            after = f.code[m.end():m.end() + 300].lstrip()
            while after.startswith("#[") and "]" in after:
                after = after[after.index("]") + 1:].lstrip()
            mm = re.match(r"(?:pub(?:\([^)]*\))?\s+)?mod\s+(\w+)", after)
            if mm and (mm.group(1).endswith(("tests", "test")) or "golden" in mm.group(1)):
                continue
            if is_test_file(f, enclosing_inline(inlines, m.start())):
                continue
            out.append((f, f.text.count("\n", 0, m.start()) + 1, m.group(0), after.split("\n")[0][:70]))
    return out


SPECIAL = re.compile(r"\b(include_str|include_bytes|env|module_path|file|concat)!\s*\(|CARGO_MANIFEST_DIR|"
                     r"std::any::type_name|\btype_name::<")


def special_sites(files):
    """Macros whose meaning depends on where a file or its crate is: paths relative to the file or the manifest
    directory, module paths and type names (an output that names them changes)."""
    out = []
    for f in files:
        for m in SPECIAL.finditer(f.code):
            line_no = f.text.count("\n", 0, m.start()) + 1
            line = f.text.split("\n")[line_no - 1].strip()
            out.append((f, line_no, m.group(0).rstrip("( !"), line[:140]))
    return out


def fmt_file(f, line):
    return f"{f.rel()}:{line}"


def analyze(markdown):
    files = all_files()
    tops = top_modules(files)
    edges = edges_of(files, tops)
    out = []
    w = out.append
    w("## Destinations\n")
    w("| Old crate | New crate | Module | Files | Lines |")
    w("|---|---|---|---:|---:|")
    by_crate = collections.defaultdict(list)
    for f in files:
        by_crate[f.krate].append(f)
    for c in old_crates():
        n, pre = LAYOUT[c]
        lines = sum(f.text.count("\n") for f in by_crate[c])
        where = "`" + "::".join([n] + (pre or [])) + "`" if pre is not None else DISSOLVED.get(c, "")
        if c in SPLITS:
            parts = sorted({"::".join([dn] + dp[:-1] if len(dp) > 1 else [dn] + dp) for dn, dp in SPLITS[c].values()})
            where += " (split: " + ", ".join(f"`{p}`" for p in parts) + ")"
        if c in DISSOLVED:
            where = DISSOLVED[c]
        w(f"| {c} | {n} | {where} | {len(by_crate[c])} | {lines} |")
    w("")
    w("## The new crates' dependencies\n")
    w("Every path in code (comments and strings left out) from one new crate into another. `normal` edges are "
      "`[dependencies]`; `dev` edges come only from tests.\n")
    w("| From | To | Refs (normal / dev / build) | Allowed |")
    w("|---|---|---|---|")
    for (a, b), refs in sorted(edges.items()):
        kinds = collections.Counter(k for *_, k, _, _ in refs)
        ok = b in ALLOWED.get(a, set())
        w(f"| {a} | {b} | {kinds['normal']} / {kinds['dev']} / {kinds['build']} | {'yes' if ok else '**NO**'} |")
    w("")
    w("## Edges the layering forbids\n")
    bad = [(k, v) for k, v in sorted(edges.items()) if k[1] not in ALLOWED.get(k[0], set())]
    if not bad:
        w("None.\n")
    for (a, b), refs in bad:
        w(f"### {a} -> {b} ({len(refs)} refs)\n")
        per = collections.defaultdict(list)
        for f, line, text, kind, krate, top in refs:
            per[(f.krate, f.top, krate, top)].append((f, line, text, kind))
        for (fk, ft, tk, tt), items in sorted(per.items(), key=lambda kv: (kv[0][0], str(kv[0][1]), kv[0][2], str(kv[0][3]))):
            names = sorted({t for *_, t, _ in items})
            kinds = sorted({k for *_, k in items})
            w(f"- `{fk}::{ft or ''}` -> `{tk}::{tt or ''}` ({len(items)}, {'/'.join(kinds)}): "
              + ", ".join(f"`{n}`" for n in names[:8]) + (" ..." if len(names) > 8 else ""))
            for f, line, text, kind in items[:3]:
                w(f"  - {fmt_file(f, line)}")
        w("")
    w("## Rust-to-Rust `extern \"C\"` bridges\n")
    w("A crate that declares in an `extern \"C\"` block a function another Rust crate exports: an edge cargo does "
      "not see.\n")
    le = link_edges(files)
    w("| Importer (old) | Exporter (old) | Symbols | New crates |")
    w("|---|---|---:|---|")
    pairs = collections.defaultdict(list)
    for f, line, name, g, fn, gn in le:
        pairs[(f.krate, g.krate, fn, gn)].append((f, line, name))
    for (a, b, fn, gn), syms in sorted(pairs.items()):
        flag = "" if fn == gn or gn in ALLOWED.get(fn, set()) else " **upward**"
        w(f"| {a} | {b} | {len(syms)} | {fn} -> {gn}{flag} |")
    w("")
    w("## cfg(test) in library code\n")
    w("`cfg(test)` is set per crate: once two old crates share a crate, a module's tests see a sibling module "
      "compiled for test, and a sibling's tests no longer see it compiled for the game. The sites outside test "
      "modules, with the old crates of the same new crate that name the crate (only those can see a change).\n")
    sites = cfg_test_sites(files)
    users = collections.defaultdict(set)
    for f in files:
        refs, inl = collect_refs(f, tops)
        for r in refs:
            res = resolve(f, r.segs, tops, enclosing_inline(inl, r.offset))
            if res and res[0] != f.krate and res[0] in LAYOUT and dest(res[0], None)[0] == file_dest(f)[0]:
                users[res[0]].add(f.krate)
    for f, line, name, g, fn, gn in le:
        if fn == gn and f.krate != g.krate:
            users[g.krate].add(f.krate)
    per = collections.Counter(f.krate for f, *_ in sites)
    w("| Old crate | New crate | Sites | Named by (same new crate) |")
    w("|---|---|---:|---|")
    for k, v in sorted(per.items()):
        w(f"| {k} | {dest(k, None)[0]} | {v} | {', '.join(sorted(users.get(k, ()))) or '-'} |")
    w("")
    w("## Location-dependent macros\n")
    sp = special_sites(files)
    w("| Site | Macro | Line |")
    w("|---|---|---|")
    for f, line, mac, text in sp:
        if mac in ("concat", "env") and "CARGO_MANIFEST_DIR" not in text and "include" not in text:
            continue
        if mac == "file" and "file!()" not in text:
            continue
        w(f"| {fmt_file(f, line)} | `{mac}` | `{text.replace('|', '/')}` |")
    w("")
    text = "\n".join(out) + "\n"
    if markdown:
        pathlib.Path(markdown).write_text(text, encoding="utf-8", newline="\n")
    else:
        sys.stdout.write(text)
    return 0


# ---------------------------------------------------------------------------------------------------------------
# headers: one per module
HEADER_DEF = re.compile(r"^[ \t]*pub\s+(?:unsafe\s+)?(?:extern\s+)?(?:const\s+)?"
                        r"(?:fn|struct|enum|union|type|const|static)\s+(?:mut\s+)?(" + IDENT + ")", re.M)
SHORT = {"ee_base": "base", "ee_data": "data", "ee_sim": "sim", "ee_render": "render", "ee_audio": "audio",
         "ee_net": "net", "ee_ui": "ui", "ee_won": "won", "ee_won_net": "won_net", "ee_won_gui": "won_gui",
         "ee_won_lobby": "won_lobby", "ee_game": "game", "ee_rmap_gatelib": "rmap_gatelib"}


def header_unit(n, p):
    """The header a new module's C items go to: rust/<crate>/<top module>.h; a crate's root items in
    rust/<crate>/<crate>.h."""
    short = SHORT.get(n, n)
    return f"{short}/{p[0] if p else short}"


def header_items():
    """(C name, today's header stem, item text, is a function) of every item of the generated headers."""
    sys.path.insert(0, str(ROOT / "tools"))
    import rust_includes
    out = []
    for path in sorted((ROOT / "include" / "rust").glob("*.h")):
        if path.stem == rust_includes.UMBRELLA:
            continue
        lines = path.read_text(encoding="utf-8").split("\n")
        try:
            open_at, close_at = lines.index('extern "C" {'), lines.index('}  // extern "C"')
        except ValueError:
            open_at = close_at = len(lines)
        start = max(i for i, l in enumerate(lines[:open_at]) if l.startswith(("#include", "#define"))) + 1
        end = open_at if open_at < len(lines) else max(i for i, l in enumerate(lines) if l.startswith("#endif"))
        for function, part in ((False, lines[start:end]), (True, lines[open_at + 1:close_at])):
            for c in rust_includes.chunks(part):
                name = rust_includes.item_name(c, function)
                if name:
                    out.append((name, path.stem, "\n".join(c), function))
    return out


def scc(nodes, deps):
    """Tarjan's strongly connected components (iterative), each a sorted list."""
    index, low, on, stack, out, counter = {}, {}, set(), [], [], [0]
    for root in sorted(nodes):
        if root in index:
            continue
        work = [(root, iter(sorted(deps.get(root, ()))))]
        index[root] = low[root] = counter[0]
        counter[0] += 1
        stack.append(root)
        on.add(root)
        while work:
            v, it = work[-1]
            nxt = next(it, None)
            if nxt is not None:
                if nxt not in index:
                    index[nxt] = low[nxt] = counter[0]
                    counter[0] += 1
                    stack.append(nxt)
                    on.add(nxt)
                    work.append((nxt, iter(sorted(deps.get(nxt, ())))))
                elif nxt in on:
                    low[v] = min(low[v], index[nxt])
                continue
            work.pop()
            if work:
                low[work[-1][0]] = min(low[work[-1][0]], low[v])
            if low[v] == index[v]:
                comp = []
                while True:
                    x = stack.pop()
                    on.discard(x)
                    comp.append(x)
                    if x == v:
                        break
                out.append(sorted(comp))
    return out


def header_plan():
    """Every header item's owner unit, the units' dependencies and their cycles."""
    files = [f for f in all_files() if f.root_kind == "lib" and not is_test_file(f)]
    cfg = tomllib.loads((ROOT / "rust" / "cbindgen.toml").read_text(encoding="utf-8"))
    renames = cfg.get("export", {}).get("rename", {})
    unrename = collections.defaultdict(set)
    for rust, c in renames.items():
        unrename[c].add(rust)
    defs = collections.defaultdict(list)
    for f in files:
        for m in HEADER_DEF.finditer(f.code):
            defs[m.group(1)].append((f, m.start()))
    items = header_items()
    owner, unowned = {}, []
    for name, stem, text, function in items:
        cands = []
        for rust in {name} | unrename.get(name, set()):
            cands.extend(defs.get(rust, []))
        in_crate = [(f, o) for f, o in cands if f.krate == stem] or cands
        if not in_crate:
            unowned.append((name, stem))
            owner[name] = (stem, header_unit("ee_game", []))
            continue
        f, o = in_crate[0]
        owner[name] = (stem, header_unit(*file_dest(f, o)))
    deps = collections.defaultdict(set)
    names = set(owner)
    for name, stem, text, function in items:
        a = owner[name][1]
        code = "\n".join(line.split("//")[0] for line in text.split("\n"))
        for x in set(re.findall(IDENT, code)):
            if x in names and x != name and owner[x][1] != a:
                deps[a].add(owner[x][1])
    units = sorted({u for _, u in owner.values()})
    comps = [c for c in scc(units, deps) if len(c) > 1]
    return items, owner, unowned, deps, units, comps


def headers(markdown):
    items, owner, unowned, deps, units, comps = header_plan()
    out = []
    w = out.append
    per_unit = collections.Counter(u for _, u in owner.values())
    per_old = collections.defaultdict(set)
    for name, (stem, unit) in owner.items():
        per_old[stem].add(unit)
    w(f"## The headers\n\n{len(items)} items of today's {len(per_old)} generated headers go to {len(units)} "
      f"module headers.\n")
    w("| Today's header | Module headers | Items |")
    w("|---|---|---:|")
    for stem in sorted(per_old):
        n = sum(1 for s, _ in owner.values() if s == stem)
        w(f"| rust/{stem}.h | " + ", ".join(f"rust/{u}.h" for u in sorted(per_old[stem])) + f" | {n} |")
    w("")
    w("| Module header | Items | Includes |")
    w("|---|---:|---|")
    for u in units:
        w(f"| rust/{u}.h | {per_unit[u]} | " + ", ".join(sorted(deps.get(u, ()))) + " |")
    w("")
    w("## Cycles between module headers\n")
    if not comps:
        w("None: every module header can include the ones it names.\n")
    for c in comps:
        w("- " + ", ".join(c))
        for a in c:
            for b in sorted(deps.get(a, ())):
                if b in c:
                    used = set()
                    for nm, st, t, fn in items:
                        if owner[nm][1] == a:
                            code = "\n".join(line.split("//")[0] for line in t.split("\n"))
                            used |= {x for x in re.findall(IDENT, code) if x in owner and owner[x][1] == b}
                    w(f"  - {a} -> {b}: {', '.join(sorted(used)[:6])}")
    w("")
    if unowned:
        w("## Items with no Rust definition found (they go to the root header of ee_game)\n")
        w(", ".join(f"`{n}` ({s})" for n, s in unowned[:40]) + (" ..." if len(unowned) > 40 else ""))
        w("")
    text = "\n".join(out) + "\n"
    if markdown:
        pathlib.Path(markdown).write_text(text, encoding="utf-8", newline="\n")
    else:
        sys.stdout.write(text)
    return 0


# ---------------------------------------------------------------------------------------------------------------
# Fixups: the edits the move makes besides paths and files, applied to the snapshot's text of a file when their step
# is active (every state is made from the snapshot alone). Each must apply exactly once.
FAILURE_DEFS = """/// Why a conversion failed: the message (bytes, as names in it may not be UTF-8).
#[derive(Debug, PartialEq, Eq)]
pub struct Failure(pub Vec<u8>);

pub(crate) fn fail<T>(message: &str) -> Result<T, Failure> {
    Err(Failure(message.as_bytes().to_vec()))
}

"""
# Test and example programs that link a merged crate get the objects of its other modules too, and with them their
# calls into the game's C++ (the units' host, the Lua runtime, the DX7 trampolines, SDL), which a test never makes:
# as separate crates those objects were never pulled in. Their linker is told to leave such symbols unresolved (the
# game's staticlib is linked by tools/build.py, never by cargo, so it is untouched). The crates that need it:
LOOSE_LINK = {"ee_sim", "ee_ui", "ee_won_gui", "ee_won_lobby", "ee_game", "ee_rmap_gatelib", "ee_render", "ee_audio",
              "ee_net", "ee_won_net", "ee_base"}
LOOSE_LINK_RS = """    // cargo's test and example programs of this crate link objects whose calls into the game's C++ they never
    // make (docs/progress/crate-restructure.md R1): the linker leaves those unresolved. The game's static library is
    // linked by tools/build.py and tools/xbuild.py, which this does not reach.
    let triple = std::env::var("TARGET").unwrap_or_default();
    let ignore = if triple.contains("msvc") {
        "/FORCE:UNRESOLVED"
    } else if triple.contains("apple") {
        "-Wl,-undefined,dynamic_lookup"
    } else {
        "-Wl,--unresolved-symbols=ignore-all"
    };
    println!("cargo:rustc-link-arg={ignore}");
    println!("cargo:rerun-if-changed=build.rs");
"""
PLAT_CORE_SEAM = """/// Stand-ins for `seam` in this crate's tests, which link no C++.
#[cfg(all(test, unix))]
pub(crate) mod test_seam {
    use std::ffi::{c_int, c_void};
    use std::sync::atomic::{AtomicU32, Ordering};

    pub static LAST_ERROR: AtomicU32 = AtomicU32::new(0);
    pub static EVENTS_SET: AtomicU32 = AtomicU32::new(0);

    #[no_mangle]
    pub extern "C" fn ee_plat_posix_set_last_error(error: u32) {
        LAST_ERROR.store(error, Ordering::SeqCst);
    }
    #[no_mangle]
    pub extern "C" fn ee_plat_posix_set_last_error_errno(error: c_int) {
        LAST_ERROR.store(0x2000_0000 | error as u32, Ordering::SeqCst);
    }
    #[no_mangle]
    pub extern "C" fn PlatEventSet(_event: *mut c_void) -> c_int {
        EVENTS_SET.fetch_add(1, Ordering::SeqCst);
        1
    }
}

"""
ALL = "<every>"   # a fixup whose old text starts with this replaces every occurrence
FIXUPS = [
    # the PNG reader's failure type goes down with it (ee_openformats names it as ee_base::png::Failure)
    ("down", "rust/crates/ee_openformats/src/lib.rs", FAILURE_DEFS, ""),
    ("down", "rust/crates/ee_openformats/src/png.rs", "use crate::{fail, Failure};\n",
     FAILURE_DEFS.replace("pub(crate) fn fail", "pub fn fail").rstrip("\n") + "\n"),
    # ee_won's items the three crates share (pub(crate) -> pub)
    # every pub(crate) item of chat.rs (the chat's character types and command numbers: gui and lobby name them);
    # the constants kept out of the headers, where bare names (BLOCK, ALERT) would reach every C++ includer
    ("won", "rust/crates/ee_won/src/chat.rs", ALL + "pub(crate) const ", "/// cbindgen:ignore\npub const "),
    ("won", "rust/crates/ee_won/src/chat.rs", ALL + "pub(crate) ", "pub "),
    # CrtChat's C-locale stand-in, which the tests of ee_won_gui and ee_won_lobby saw (cfg(test) is per crate)
    ("won", "rust/crates/ee_won/src/chat.rs", "/// cbindgen:ignore\n#[cfg(not(test))]\nmod crt {",
     "/// cbindgen:ignore\n#[cfg(not(any(test, feature = \"c_locale\")))]\nmod crt {"),
    ("won", "rust/crates/ee_won/src/chat.rs", "#[cfg(not(test))]\nimpl ChatCtype for CrtChat {",
     "#[cfg(not(any(test, feature = \"c_locale\")))]\nimpl ChatCtype for CrtChat {"),
    ("won", "rust/crates/ee_won/src/chat.rs", "#[cfg(test)]\nimpl ChatCtype for CrtChat {",
     "#[cfg(any(test, feature = \"c_locale\"))]\nimpl ChatCtype for CrtChat {"),
    # gx's module gx inside the module gx
    ("render", "rust/crates/ee_gx/src/lib.rs", "\npub mod gx;\n",
     "\n#[allow(clippy::module_inception)] // the module was the crate ee_gx\npub mod gx;\n"),
    ("audio", "rust/crates/ee_mixer/src/lib.rs", "/// cbindgen:ignore\npub mod mixer;\n",
     "/// cbindgen:ignore\n#[allow(clippy::module_inception)] // the module was the crate ee_mixer\npub mod mixer;\n"),
    ("sim", "rust/crates/ee_ai/build.rs", "fn main() {\n", "fn main() {\n" + LOOSE_LINK_RS),
    # the two functions that became pub need their safety sections
    ("won", "rust/crates/ee_won/src/chat.rs", "pub unsafe fn units(",
     "/// # Safety\n/// `t` is a live text of `t.length` characters of `wchar_size` bytes each.\npub unsafe fn units("),
    ("won", "rust/crates/ee_won/src/ffi.rs", "pub(crate) unsafe fn give(",
     "/// # Safety\n/// `sink` may be called with `context`.\npub unsafe fn give("),
    # ee_plat_core's POSIX test stand-ins for three functions ee_plat_sys defines (R3): one crate now, so the tests
    # read the real ones (the thread's last error, a real event)
    ("base", "rust/crates/ee_plat_core/src/lib.rs", PLAT_CORE_SEAM, ""),
    ("base", "rust/crates/ee_plat_core/src/path.rs",
     "        use crate::test_seam::LAST_ERROR;\n        use std::sync::atomic::Ordering;\n",
     "        extern \"C\" {\n            fn PlatGetLastError() -> u32;\n        }\n"),
    ("base", "rust/crates/ee_plat_core/src/path.rs", ALL + "LAST_ERROR.load(Ordering::SeqCst)",
     "unsafe { PlatGetLastError() }"),
    ("base", "rust/crates/ee_plat_core/src/time.rs", "        use crate::test_seam::EVENTS_SET;\n",
     "        extern \"C\" {\n"
     "            fn PlatEventCreate(manual_reset: i32, initially_set: i32) -> *mut PlatEvent;\n"
     "            fn PlatEventWait(event: *mut PlatEvent, timeout_ms: u32) -> i32;\n"
     "            fn PlatEventDestroy(event: *mut PlatEvent);\n"
     "        }\n"),
    ("base", "rust/crates/ee_plat_core/src/time.rs",
     "            let before = EVENTS_SET.load(Ordering::SeqCst);\n"
     "            let id = unsafe { PlatTimerStartEvent(5, 0, 0x10 as *mut PlatEvent) };\n"
     "            assert_ne!(id, 0);\n"
     "            PlatSleep(100);\n"
     "            assert_eq!(EVENTS_SET.load(Ordering::SeqCst), before + 1);\n",
     "            let event = unsafe { PlatEventCreate(0, 0) };\n"
     "            let id = unsafe { PlatTimerStartEvent(5, 0, event) };\n"
     "            assert_ne!(id, 0);\n"
     "            PlatSleep(100);\n"
     "            assert_eq!(unsafe { PlatEventWait(event, 0) }, 0, \"the timer set the event\");\n"
     "            unsafe { PlatEventDestroy(event) };\n"),
    # a timed wait of ee_plat_sys's tests, now in one binary with ee_plat_core's tests that set the harness clock
    ("base", "rust/crates/ee_plat_sys/src/thread.rs",
     "    fn auto_reset_event_releases_one_wait_and_clears() {\n        unsafe {",
     "    fn auto_reset_event_releases_one_wait_and_clears() {\n"
     "        // a timed wait: not while a plat_core test sets the harness clock (one test binary since the restructure)\n"
     "        let _guard = ee_plat_core::test_globals();\n        unsafe {"),
    # a test that reads its crate's sources by the manifest directory
    ("sim", "rust/crates/ee_units/src/header_consts.rs", 'let dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("src");',
     'let dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("src").join("units");'),
    # a tool that names a crate path by its parts
    ("base", "tools/lang_strings.py", 'OUT = ROOT / "rust" / "crates" / "ee_util" / "src" / "language_strings.rs"',
     'OUT = ROOT / "rust" / "crates" / "ee_base" / "src" / "util" / "language_strings.rs"'),
    # the tools that name a package or the static library
    ("game", "tools/rust_build.py", '"ee_rust.lib" if target.startswith("windows") else "libee_rust.a"',
     '"ee_game.lib" if target.startswith("windows") else "libee_game.a"'),
    ("game", "tools/rust_build.py", '["build", "--release", "-p", "ee_rust",', '["build", "--release", "-p", "ee_game",'),
    ("game", "tools/rust_build.py", "build [--target T]     ee_rust for a target", "build [--target T]     ee_game for a target"),
    ("game", "tools/rust_build.py", '"""Builds ee_rust for the target', '"""Builds ee_game for the target'),
    ("data", "tools/eedata/cppgate.py", "'-p', 'ee_data_cabi']", "'-p', 'ee_data', '--lib']"),
    ("data", "tools/eedata/cppgate.py", "cargo build of ee_data_cabi failed", "cargo build of the ee_data staticlib failed"),
    ("data", "tools/eedata/cppgate.py", "'ee_data_cabi.lib' if sys.platform == 'win32' else 'libee_data_cabi.a'",
     "'ee_data.lib' if sys.platform == 'win32' else 'libee_data.a'"),
    ("render", "tools/eemodel/cppgate.py", "'-p', 'ee_openformats', '--bin'", "'-p', 'ee_render', '--bin'"),
    ("sim", "tools/sim_capture.py", '(("ee_formations", "formation_replay"), ("ee_combat", "combat_replay"),\n'
     '                                    ("ee_tech", "tech_replay"))',
     '(("ee_sim", "formation_replay"), ("ee_sim", "combat_replay"),\n'
     '                                    ("ee_sim", "tech_replay"))'),
]
# what the fixups above need in the new crates' manifests: new crate -> manifest additions
MANIFEST_EXTRA = {
    "ee_won_net": {"features": {"c_locale": []}},
    "ee_won_gui": {"dev-dependencies": {"ee_won_net": {"path": "../ee_won_net", "features": ["c_locale"]}}},
    "ee_won_lobby": {"dev-dependencies": {"ee_won_net": {"path": "../ee_won_net", "features": ["c_locale"]}}},
}


# external crates two merged crates name at different versions: new crate -> {old crate: {package: new key}}
EXTERNAL_RENAMES = {"ee_base": {"ee_plat_core": {"windows-sys": "windows-sys-059"}}}
# old crates whose items cbindgen never read (not in cbindgen.toml's extra_bindings): their module gets
# `/// cbindgen:ignore` in a merged crate, so the headers stay as they are
_fixups_used = collections.Counter()


# ee_sound's golden fakes of what ee_base defines for real (the Thread System, the shutdown list, three platform
# calls): as separate crates the fakes were the only definitions in the test binary; with ee_base linked next to them
# they would be two. The test build gives the fakes their own symbol names and points sys.rs's imports at them.
SOUND_FAKES = ["ee_util_register_shutdown", "ee_ts_section_new", "ee_ts_section_free", "ee_ts_section_enter",
               "ee_ts_section_leave", "ee_ts_event_new", "ee_ts_event_free", "ee_ts_event_set", "ee_ts_event_clear",
               "ee_ts_event_wait", "ee_ts_event_wait_any", "ee_ts_thread_new", "ee_ts_thread_free",
               "ee_ts_thread_set_name", "PlatSleep", "PlatGetLastError", "PlatFOpen"]


def sound_fakes_sys(text):
    for name in SOUND_FAKES:
        old = f"    pub fn {name}("
        if text.count(old) != 1:
            raise SystemExit(f"sys.rs: {name} is not declared once")
        text = text.replace(old, f'    #[cfg_attr(test, link_name = "ee_sound_fake_{name}")]\n' + old)
    return text


def sound_fakes_golden(text):
    for name in SOUND_FAKES:
        m = re.search(r"#\[no_mangle\]\n((?:unsafe )?extern \"C\" fn " + name + r"\()", text)
        if not m:
            raise SystemExit(f"golden_objects.rs: no fake of {name}")
        allow = "#[allow(non_snake_case)]\n" if not name.islower() else ""
        text = text[:m.start()] + allow + f'#[export_name = "ee_sound_fake_{name}"]\n' + m.group(1) + text[m.end():]
    return text


FIXUP_FUNCS = [
    ("audio", "rust/crates/ee_sound/src/sys.rs", sound_fakes_sys),
    ("audio", "rust/crates/ee_sound/src/golden_objects.rs", sound_fakes_golden),
]


def apply_fixups(rel, text):
    for step, path, func in FIXUP_FUNCS:
        if path == rel and step in ACTIVE:
            text = func(text)
    for i, (step, path, old, new) in enumerate(FIXUPS):
        if path != rel or step not in ACTIVE:
            continue
        if old.startswith(ALL):
            if old[len(ALL):] not in text:
                raise SystemExit(f"fixup {i} for {path} applies nowhere: {old[:70]!r}")
            text = text.replace(old[len(ALL):], new)
            _fixups_used[i] += 1
            continue
        if text.count(old) != 1:
            raise SystemExit(f"fixup {i} for {path} does not apply once: {old[:70]!r}")
        text = text.replace(old, new, 1)
        _fixups_used[i] += 1
    return text


# ---------------------------------------------------------------------------------------------------------------
# move: one state's tree
NEW_DOCS = {
    "ee_base": "The platform layer, the leaves (utilities, errors, time, threads, strings, codecs, files, input), the "
               "GE math and the PNG reader: what every other crate of the game builds on.",
    "ee_sim": "The simulation: the units, combat, path finding, formations, the random maps, the tech tree, the "
              "scenarios, the computer players, the game messages and the simulation parts of the app.",
    "ee_render": "The renderer: the Graphics Engine, the graphics thread, GX (wgpu), the DX7 plug-in and the open "
                 "formats (glTF, PNG frames).",
    "ee_audio": "The sound manager and the mixer.",
    "ee_net": "The Network Engine and its TCP plug-in.",
    "ee_won_net": "The WON library's network half: WONCommon, WONSocket, WONCrypt, WONAuth, WONServer, WONRouting, "
                  "WONDir, WONDB, WONMisc, WONInternal, WONConfig, WONFile and WONStatus.",
    "ee_won_gui": "WONGUI: the lobby's GUI toolkit, its platform layer, skins and image decoders.",
    "ee_won_lobby": "WONLobby: the lobby's screens, logic and data model, and the Empire Earth lobby glue.",
}


def old_extra_bindings():
    cfg = tomllib.loads((SRC_ROOT / "rust" / "cbindgen.toml").read_text(encoding="utf-8"))
    return set(cfg["parse"].get("extra_bindings", [])), cfg


def relpath(target, start_dir):
    import os
    return os.path.relpath(target, start_dir).replace("\\", "/")


def normpath(p):
    import posixpath
    return posixpath.normpath(p)


SEG_TOKEN = re.compile(IDENT + r"|\*")
SEG_SEP = re.compile(r"\s*::\s*")


def seg_spans(code, start):
    """(start, end) of each segment of the path at `start` (`a :: b :: c`)."""
    out, i = [], start
    while True:
        m = SEG_TOKEN.match(code, i)
        if not m:
            break
        out.append((m.start(), m.end()))
        n = SEG_SEP.match(code, m.end())
        if not n or "::" not in n.group(0):
            break
        i = n.end()
    return out


def crate_dir(n):
    return f"rust/crates/{n}"


def new_file_path(f, offset=0):
    """A source file's (or a cut file's part's) repo path in the new tree. A module's files keep their place under
    the directory of the module's parent: an old crate root becomes `<module>/mod.rs`."""
    n, p = file_dest(f, offset)
    if f.root_kind != "lib":
        return f"{crate_dir(n)}/" + f.path.relative_to(CRATES / f.krate).as_posix()
    if f.top is None:
        n, p = dest(f.krate, None)
        return f"{crate_dir(n)}/src/" + ("/".join(p) + "/mod.rs" if p else "lib.rs")
    rel = f.path.relative_to(CRATES / f.krate / "src").as_posix()
    if "down" in ACTIVE and (f.krate, f.top) in FILE_SPLITS and len(f.module) == 1:
        top_path = p
    else:
        top_path = dest(f.krate, f.top)[1]
    return f"{crate_dir(n)}/src/" + "".join(d + "/" for d in top_path[:-1]) + rel


class State:
    """The new layout's facts a rewrite needs: which crates exist, where each file goes."""

    def __init__(self, files):
        self.files = files
        self.tops = top_modules(files)
        self.crates = {file_dest(f)[0] for f in files}


def climb(module, segs):
    """`self`/`super` segments applied to a module path: (base path, index of the first other segment) or None."""
    cur, i = list(module), 0
    while i < len(segs) and segs[i].split(" as ")[0] in ("self", "super"):
        if segs[i].split(" as ")[0] == "super":
            if not cur:
                return None
            cur.pop()
        i += 1
    return cur, i


def new_path_for(st, f, segs, inline, offset):
    """For a path of a file: (k, new segments) to replace its first k segments, or None to leave it."""
    res = resolve(f, segs, st.tops, inline)
    if not res:
        return None
    krate, top, rest = res
    if krate not in LAYOUT:
        return None
    n_f, m_f = file_dest(f, offset)
    m_f = m_f + inline
    n_t, p_t = target_dest(krate, top, rest)
    if "down" in ACTIVE and (krate, top) in FILE_SPLITS and rest and rest[0] == "*":
        # a glob of a cut file: the part in the importer's crate
        _, first, second = FILE_SPLITS[(krate, top)]
        part = [x for x in (first, second) if x[0] == n_f]
        if len(part) != 1:
            raise SystemExit(f"{f.rel()}: a glob import of the cut file {krate}::{top}: name its items")
        n_t, p_t = part[0][0], list(part[0][1])
    want = (n_t, p_t + rest)
    head = segs[0].split(" as ")[0]
    alias = segs[0].split(" as ")[1] if " as " in segs[0] and len(segs) == 1 else None
    # what the path's text means in the new layout
    if head in ("self", "super"):
        c = climb(m_f, segs)
        got = (n_f, c[0] + segs[c[1]:]) if c else None
    elif head in ("crate", "$crate"):
        got = (n_f, segs[1:])
    elif head in st.crates:
        got = (head, segs[1:])
    else:
        got = None
    if got is not None:
        got = (got[0], [s.split(" as ")[0] if i == len(got[1]) - 1 else s for i, s in enumerate(got[1])])
        w = (want[0], [s.split(" as ")[0] if i == len(want[1]) - 1 else s for i, s in enumerate(want[1])])
        if got == w:
            return None
    k = len(segs) - len(rest)
    if k <= 0:
        raise SystemExit(f"{f.rel()}: cannot rewrite {'::'.join(segs)} (into {n_t}::{'::'.join(p_t)})")
    # a bin, example, test or build script is a crate of its own: it names the library by its name
    same = n_t == n_f and f.root_kind == "lib"
    new = (["$crate" if head == "$crate" else "crate"] if same else [n_t]) + p_t
    if alias is not None:
        if new == ["crate"]:
            raise SystemExit(f"{f.rel()}: `use {segs[0]}` would import the crate itself")
        new = new[:-1] + [new[-1] + " as " + alias]
    return k, new


class Rewriter:
    def __init__(self, st, f, lo, hi, ctx=None):
        self.st, self.f, self.lo, self.hi = st, f, lo, hi
        self.ctx = ctx   # the offset whose place the text takes (a cut file's preamble copied to its second part)
        self.edits = []

    def at(self, offset):
        return offset if self.ctx is None else self.ctx

    def add(self, s, e, new):
        self.edits.append((s, e, new))

    def paths(self):
        f, code, text = self.f, self.f.code, self.f.text
        inlines = inline_modules(code)
        covered = []
        for start, end in use_spans(code):
            covered.append((start, end))
            if self.lo <= start < self.hi:
                self.use_tree(start, end, enclosing_inline(inlines, start))
        for m in PATH.finditer(code):
            s = m.start(2)
            if not (self.lo <= s < self.hi) or any(a <= s < b for a, b in covered):
                continue
            head = m.group(2)
            if head not in LAYOUT and head not in ("crate", "self", "super"):
                continue
            if code[max(0, s - 1)] == "$":
                continue
            spans = seg_spans(code, s)
            segs = [text[a:b] for a, b in spans]
            r = new_path_for(self.st, f, segs, enclosing_inline(inlines, s), self.at(s))
            if r:
                self.add(spans[0][0], spans[r[0] - 1][1], "::".join(r[1]))
        for m in re.finditer(r"\$crate(?=\s*::)", code):
            if not (self.lo <= m.start() < self.hi):
                continue
            spans = [(m.start(), m.end())] + seg_spans(code, SEG_SEP.match(code, m.end()).end())
            segs = ["$crate"] + [text[a:b] for a, b in spans[1:]]
            r = new_path_for(self.st, f, segs, enclosing_inline(inlines, m.start()), self.at(m.start()))
            if r:
                self.add(spans[0][0], spans[r[0] - 1][1], "::".join(r[1]))
        for m in re.finditer(r"\bextern\s+crate\s+(" + IDENT + r")", code):
            if self.lo <= m.start() < self.hi and m.group(1) in LAYOUT:
                n = dest(m.group(1), None)[0]
                if n != m.group(1):
                    self.add(m.start(1), m.end(1), n)

    def use_tree(self, start, end, inline):
        f, code, text = self.f, self.f.code, self.f.text
        leaves = [x for x in parse_use_tree(code[start:end]) if x]
        if len(leaves) == 1 and len(leaves[0]) == 1 and re.fullmatch(r"ee_\w+ as _", leaves[0][0]):
            # `use ee_x as _;` links a crate in: the new crate's, or nothing inside that crate itself
            old = leaves[0][0].split(" as ")[0]
            if old not in LAYOUT:
                return
            n = dest(old, None)[0]
            if n == old:
                return
            if f.root_kind == "lib" and n == file_dest(f, self.at(start))[0]:
                s, _ = statement_start(text, code, start)
                e = text.find("\n", end)
                self.add(s, len(text) if e < 0 else e + 1, "")
                return
            m = re.compile(IDENT).search(code, start)
            self.add(m.start(), m.end(), n)
            if old == "ee_won" and n == "ee_won_net":
                # ee_won is three crates now: the other two are linked in the same way
                e = text.find(";", end)
                self.add(e + 1, e + 1, "\nuse ee_won_gui as _;\nuse ee_won_lobby as _;")
            return
        out = []
        changed = False
        for segs in leaves:
            probe = segs
            if segs[0].split(" as ")[0] not in ("crate", "self", "super", "$crate") and \
                    segs[0].split(" as ")[0] not in LAYOUT:
                mod = f.module + inline
                if f.root_kind == "lib" and not mod and segs[0] in self.st.tops.get(f.krate, ()):
                    probe = ["self"] + segs   # a 2018 uniform path to a child of the crate root
                else:
                    out.append((segs, segs))
                    continue
            res = resolve(f, probe, self.st.tops, inline)
            if res and "down" in ACTIVE and (res[0], res[1]) in FILE_SPLITS and res[2] == ["*"]:
                # a glob of a cut file: a glob of each part
                n_f = file_dest(f, self.at(start))[0]
                _, above, below = file_split_parts(res[0], res[1])
                words = set(re.findall(IDENT, code))
                for part, names in zip(FILE_SPLITS[(res[0], res[1])][1:], (above, below - above)):
                    if words & names:   # the parts whose items the file names
                        out.append((segs, (["crate"] if part[0] == n_f else [part[0]]) + list(part[1]) + ["*"]))
                out.append((segs, None))
                changed = True
                continue
            r = new_path_for(self.st, f, probe, inline, self.at(start))
            if r is None:
                out.append((segs, segs))
                continue
            k, new = r
            if probe is not segs:
                k -= 1
            out.append((segs, list(new) + segs[k:]))
            changed = True
        if not changed:
            return
        split_forced = any(new is None for _, new in out)
        out = [(o, n) for o, n in out if n is not None]
        brace = code.find("{", start, end)
        if brace < 0 and not split_forced:
            (old, new), = out
            spans = seg_spans(code, start + len(code[start:end]) - len(code[start:end].lstrip()))
            keep_tail = len(old) - 1   # the old path's segments after its first, unchanged at the end of `new`
            while keep_tail > 0 and new[len(new) - keep_tail:] != old[len(old) - keep_tail:]:
                keep_tail -= 1
            k = len(old) - keep_tail
            head = new[:len(new) - keep_tail]
            if k == len(old) and " as " in old[-1]:
                head = head[:-1] + [head[-1].split(" as ")[0]]   # the alias stays where it is in the text
            self.add(spans[0][0], spans[k - 1][1], "::".join(head))
            return
        head_spans = seg_spans(code, start + len(code[start:end]) - len(code[start:end].lstrip()))
        head = [text[a:b] for a, b in head_spans]
        heads = set()
        for old, new in out:
            tail = old[len(head):]
            if old[:len(head)] != head or (tail and new[len(new) - len(tail):] != tail):
                heads.add(None)
            else:
                heads.add(tuple(new[:len(new) - len(tail)]))
        if len(heads) == 1 and None not in heads and not split_forced:
            new_head = list(next(iter(heads)))
            if new_head:
                self.add(head_spans[0][0], head_spans[-1][1], "::".join(new_head))
                return
        # one statement per leaf, the attributes and the visibility repeated
        s, prefix = statement_start(text, code, start)
        indent = text[text.rfind("\n", 0, s) + 1:s]
        if indent.strip():
            indent = ""
        stmts = []
        for old, new in out:
            if new[-1] == "self":
                new = new[:-1]
            elif new[-1].startswith("self as "):
                new = new[:-2] + [new[-2] + " as " + new[-1].split(" as ")[1]]
            stmts.append(prefix + "::".join(new) + ";")
        self.add(s, end + 1, ("\n" + indent).join(stmts))

    def includes(self, here_new, moved):
        f, text, code = self.f, self.f.text, self.f.code
        for m in INCLUDE_MACRO.finditer(text):
            if not (self.lo <= m.start() < self.hi) or code[m.start()] == " ":
                continue
            old_target = normpath((pathlib.PurePosixPath(f.rel()).parent / m.group(2)).as_posix())
            new_target = moved.get(old_target, old_target)
            lit = relpath(new_target, str(pathlib.PurePosixPath(here_new).parent))
            if lit != m.group(2):
                self.add(m.start(2), m.end(2), lit)

    def externals(self):
        n_f = file_dest(self.f, self.at(self.lo))[0]
        for pkg, key in EXTERNAL_RENAMES.get(n_f, {}).get(self.f.krate, {}).items():
            name, new = pkg.replace("-", "_"), key.replace("-", "_")
            for m in re.finditer(r"(?<![A-Za-z0-9_:])" + name + r"(?=\s*::)", self.f.code):
                if self.lo <= m.start() < self.hi:
                    self.add(m.start(), m.end(), new)

    def result(self, extra=()):
        edits = sorted(set(self.edits) | set(extra))
        text = self.f.text
        out, pos = [], self.lo
        for s, e, new in edits:
            if s < pos:
                raise SystemExit(f"{self.f.rel()}: overlapping edits at {s}: {text[s:e]!r}")
            out.append(text[pos:s])
            out.append(new)
            pos = e
        out.append(text[pos:self.hi])
        return "".join(out)


INCLUDE_MACRO = re.compile(r'(include_str|include_bytes|include)!\(\s*"([^"]+)"\s*\)')


def statement_start(text, code, tree_start):
    """The start of a `use` statement (with the attribute lines just above it) and its text up to the tree."""
    kw = code.rfind("use", 0, tree_start)
    vis = re.search(r"pub(?:\([^)]*\))?\s+$", code[:kw])
    s = vis.start() if vis else kw
    line_start = text.rfind("\n", 0, s) + 1
    a = line_start
    while a > 0:
        prev_start = text.rfind("\n", 0, a - 1) + 1
        line = text[prev_start:a - 1].strip()
        if line.startswith("#[") and line.endswith("]"):
            a = prev_start
            continue
        break
    attrs = [l.strip() for l in text[a:line_start].split("\n") if l.strip()]
    indent = text[line_start:s] if not text[line_start:s].strip() else ""
    pre = text[s:tree_start]
    if attrs:
        return a, ("\n" + indent).join(attrs) + "\n" + indent + pre
    return s, pre


# ---- module declarations
def mod_decls(f):
    """The `mod name;` declarations at the top level of a file: (name, start, end) with their doc comment and
    attribute lines (the whole lines)."""
    out = []
    inlines = inline_modules(f.code)
    for m in MOD_DECL.finditer(f.code):
        if m.group(3) != ";" or enclosing_inline(inlines, m.start()):
            continue
        line_start = f.text.rfind("\n", 0, m.start()) + 1
        end = f.text.find("\n", m.end())
        end = len(f.text) if end < 0 else end + 1
        s = line_start
        while s > 0:
            prev = f.text.rfind("\n", 0, s - 1) + 1
            line = f.text[prev:s - 1].strip()
            if line.startswith(("#[", "///")) and not line.startswith("//!"):
                s = prev
                continue
            break
        out.append((m.group(2), s, end))
    return out


def crate_root_attrs(f):
    """A crate root's inner attributes (`#![...]`, which may span lines)."""
    out, i = [], 0
    code = f.code
    for m in re.finditer(r"#!\[", code):
        depth, j = 0, m.start() + 2
        while j < len(code):
            depth += (code[j] == "[") - (code[j] == "]")
            j += 1
            if depth == 0:
                break
        out.append(f.text[m.start():j])
    return out


# ---- the plan
class Plan:
    def __init__(self):
        self.out = {}          # new repo path -> text (LF)
        self.crlf = set()      # new repo paths written CRLF
        self.moved = {}        # old repo path -> new repo path (every file under rust/crates)
        self.copied = set()    # old repo paths copied as they are (data, include!d sources)
        self.notes = []


def build_plan(steps_upto, only_rust=False):
    set_active(steps_upto)
    load_won_groups()
    files = all_files()
    st = State(files)
    plan = Plan()
    old_extra, cbcfg = old_extra_bindings()
    dissolved = {k for k, (n, p) in LAYOUT.items() if p is None and STEP_OF_CRATE.get(n) in ACTIVE}
    keep = [f for f in files if f.krate not in dissolved]
    tree = {f.rel() for f in files}
    # 1. where each source file (or part) goes
    parts = {}    # file -> [(lo, hi, new path)]
    for f in keep:
        key = (f.krate, f.top)
        if "down" in ACTIVE and key in FILE_SPLITS and len(f.module) == 1 and f.root_kind == "lib":
            cut = f.text.index(FILE_SPLITS[key][0])
            parts[f] = [(0, cut, new_file_path(f, 0)), (cut, len(f.text), new_file_path(f, cut))]
        else:
            parts[f] = [(0, len(f.text), new_file_path(f, 0))]
    for f, ps in parts.items():
        plan.moved.setdefault(f.rel(), ps[-1][2])
    # 2. the other files of the crate directories, placed by the file that includes them (else with the root)
    for k in old_crates():
        if k in dissolved:
            continue
        d = CRATES / k
        for p in sorted(d.rglob("*")):
            if not p.is_file() or p.relative_to(d).as_posix() == "Cargo.toml":
                continue
            rel = p.relative_to(SRC_ROOT).as_posix()
            if rel not in tree:
                plan.moved[rel] = None
                plan.copied.add(rel)
    for f, ps in parts.items():
        for m in INCLUDE_MACRO.finditer(f.text):
            if f.code[m.start()] == " ":
                continue
            target = normpath((pathlib.PurePosixPath(f.rel()).parent / m.group(2)).as_posix())
            if target in plan.moved and plan.moved[target] is None:
                new_dir = pathlib.PurePosixPath(ps[0][2]).parent
                plan.moved[target] = normpath((new_dir / m.group(2)).as_posix())
    for rel, new in list(plan.moved.items()):
        if new is None:
            k = rel.split("/")[2]
            sub = rel.split("/", 4)[3:]
            n, p = dest(k, None)
            if sub and sub[0] == "src":
                plan.moved[rel] = f"{crate_dir(n)}/src/" + "".join(x + "/" for x in p) + rel.split("/", 4)[4]
            else:
                plan.moved[rel] = f"{crate_dir(n)}/" + "/".join(sub)
    # 3. module declarations: each module's parent file in the new layout
    module_file = {}   # (crate, module path) -> new path of the file that defines it
    for f, ps in parts.items():
        if f.root_kind != "lib":
            continue
        for lo, hi, new in ps:
            n, p = file_dest(f, lo)
            module_file[(n, tuple(p))] = new
    removals = collections.defaultdict(list)   # (file, part lo) -> [(start, end)]
    additions = collections.defaultdict(list)  # (crate, parent module path) -> [declaration text]
    child_of = {}
    for f in keep:
        if f.root_kind == "lib":
            child_of[(f.krate, tuple(f.module))] = f
    for f in keep:
        if f.root_kind != "lib":
            continue
        for name, s, e in mod_decls(f):
            c = child_of.get((f.krate, tuple(f.module + [name])))
            if c is None:
                continue
            n_p, p_p = file_dest(f, s)
            for lo, hi, _ in parts[c]:
                n_c, p_c = file_dest(c, lo)
                if (n_c, p_c[:-1]) != (n_p, p_p):
                    if lo == 0 and not (len(parts[c]) > 1 and file_dest(c, parts[c][-1][0]) == (n_p, p_p + [name])):
                        removals[f].append((s, e))
                    additions[(n_c, tuple(p_c[:-1]))].append(decl_text(f.text[s:e], c, lo, old_extra))
    # old crate roots that become modules, and the split parents
    roots = {}
    for k in old_crates():
        if k in dissolved:
            continue
        n, p = dest(k, None)
        if p:
            roots[k] = (n, p)
    for k, (n, p) in sorted(roots.items(), key=lambda kv: kv[1]):
        ignore = k not in old_extra and k not in ("ee_rust",)
        doc = f"/// The crate {k} before the restructure.\n"
        additions[(n, tuple(p[:-1]))].append(doc + ("/// cbindgen:ignore\n" if ignore else "") + f"pub mod {p[-1]};\n")
    # the parents the move creates (ee_base::math, ee_sim::app), declared in theirs
    declared = {(n, tuple(p)) for n, p in roots.values()}
    for (n, p) in sorted(list(additions)):
        for i in range(1, len(p) + 1):
            sub = (n, p[:i])
            if sub not in module_file and sub not in declared:
                additions[(n, p[:i - 1])].append(f"pub mod {p[i - 1]};\n")
                declared.add(sub)
    # 4. the rewritten sources
    for f, ps in parts.items():
        for lo, hi, new in ps:
            rw = Rewriter(st, f, lo, hi)
            rw.paths()
            rw.includes(new, plan.moved)
            rw.externals()
            extra = [(s, e, "") for s, e in removals.get(f, []) if lo <= s < hi]
            text = rw.result(extra)
            if lo > 0:
                text = cut_preamble(f, lo, st, plan) + text
            elif len(ps) > 1:
                text = filter_unused_uses(text)
            if len(ps) > 1 and lo > 0:
                text = filter_unused_uses(text)
            plan.out[new] = text
            if f.crlf:
                plan.crlf.add(new)
    for rel in plan.copied:
        plan.out[plan.moved[rel]] = None   # copied as bytes
    # 5. declarations into existing parents, or new parent files
    for (n, p), decls in sorted(additions.items()):
        target = module_file.get((n, p))
        if target is None:
            target = f"{crate_dir(n)}/src/" + ("lib.rs" if not p else "/".join(p) + "/mod.rs")
            plan.out[target] = generated_parent(n, p, decls, files)
            module_file[(n, p)] = target
        else:
            plan.out[target] = insert_decls(plan.out[target], decls)
    # 6. manifests
    for n in sorted({file_dest(f)[0] for f in keep}):
        plan.out[f"{crate_dir(n)}/Cargo.toml"] = manifest_text(n, keep, st, dissolved)
        build = f"{crate_dir(n)}/build.rs"
        if n in LOOSE_LINK and build not in plan.out:
            plan.out[build] = ("//! The build script: the link of this crate's test programs (see below).\n\n"
                               "fn main() {\n" + LOOSE_LINK_RS + "}\n")
    plan.cbindgen = cbindgen_text(cbcfg)
    return plan, files


def decl_text(text, child, lo, old_extra):
    """A module declaration moved to another parent: its `#[path]` dropped (the file sits where its name says)."""
    lines = [l.strip() if l.strip() else l for l in text.split("\n") if not re.match(r"\s*#\[path\s*=", l)]
    if not any(l.startswith("#[cfg(test)]") for l in lines):
        # another crate (or a sibling module) names it now
        lines = [("pub " + l) if re.match(r"mod\s+\w+\s*;", l) else l for l in lines]
    return "\n".join(lines)


def insert_decls(text, decls):
    """Declarations appended after a file's last top-level `mod` declaration (else at its end)."""
    last = None
    for m in re.finditer(r"^(?:pub(?:\([^)]*\))?\s+)?mod\s+\w+\s*;[^\n]*\n", text, re.M):
        last = m.end()
    block = "".join(d if d.endswith("\n") else d + "\n" for d in decls)
    if last is None:
        return text.rstrip("\n") + "\n\n" + block
    return text[:last] + block + text[last:]


def generated_parent(n, p, decls, files):
    """A module file the move creates: a new crate's root, or the parent of a split's parts."""
    if not p:
        doc = NEW_DOCS.get(n, f"The crate {n}.")
        head = "//! " + "\n//! ".join(wrap(doc, 116)) + "\n"
        if n.startswith("ee_won_"):
            won_root = next(f for f in files if f.krate == "ee_won" and f.top is None)
            attrs = crate_root_attrs(won_root)
            head += "".join(a + "\n" for a in attrs)
        return head + "\n" + "".join(d if d.endswith("\n") else d + "\n" for d in decls)
    doc = SPLIT_PARENTS.get((n, tuple(p)), f"The module {'::'.join(p)}.")
    head = "//! " + "\n//! ".join(wrap(doc, 116)) + "\n"
    # the inner attributes of the crate the parts came from
    src = {("ee_base", ("math",)): "ee_ge", ("ee_sim", ("app",)): "ee_app"}.get((n, tuple(p)))
    if src:
        root = next(f for f in files if f.krate == src and f.top is None)
        head += "".join(a + "\n" for a in crate_root_attrs(root))
    return head + "\n" + "".join(d if d.endswith("\n") else d + "\n" for d in decls)


def wrap(text, width):
    import textwrap
    return textwrap.wrap(text, width)


def cut_preamble(f, cut, st, plan):
    """The head of the second part of a cut file: the first part's doc comment, `use` lines and the private helpers
    it names (rewritten for the second part's place), filtered to what it uses later."""
    key = (f.krate, f.top)
    text = f.text
    first_item = re.search(r"^(///|pub |unsafe |fn |#\[)", text, re.M)
    pre_end = first_item.start() if first_item else 0
    rw = Rewriter(st, f, 0, pre_end, ctx=cut)
    rw.paths()
    rw.externals()
    pre = rw.result()
    # the doc comment for the second part
    doc = FILE_SPLIT_DOCS.get(key)
    if doc:
        pre = re.sub(r"\A(//![^\n]*\n)+", "//! " + "\n//! ".join(wrap(doc, 116)) + "\n", pre)
    helpers = ""
    for name in FILE_SPLIT_SHARED.get(key, ()):
        m = re.search(r"(?:///[^\n]*\n)*(?:unsafe )?fn " + name + r"\b", text)
        if m:
            depth, i = 0, text.index("{", m.end())
            j = i
            while j < len(text):
                depth += (text[j] == "{") - (text[j] == "}")
                j += 1
                if depth == 0:
                    break
            helpers += text[m.start():j] + "\n\n"
    return pre + helpers


FILE_SPLIT_DOCS = {("ee_ge", "ffi"): "The C ABI of the Graphics Engine's terrain mesh queries and the physical model's "
                                     "triangle tests (the math's is ee_base::math::ffi)."}
FILE_SPLIT_SHARED = {("ee_ge", "ffi"): ("put",)}


def filter_unused_uses(text):
    """A cut file's `use` lines without the names its code no longer uses."""
    code = strip_code(text)
    out = text
    for start, end in reversed(list(use_spans(code))):
        kw = code.rfind("use", 0, start)
        stmt_end = end + 1
        tree = code[start:end]
        body = code[:kw] + " " * (stmt_end - kw) + code[stmt_end:]
        leaves = [x for x in parse_use_tree(tree) if x]
        used = []
        for segs in leaves:
            leaf = segs[-1].split(" as ")[-1]
            if leaf == "self":
                leaf = segs[-2] if len(segs) > 1 else None
            if leaf in (None, "*") or re.search(r"(?<![A-Za-z0-9_])" + re.escape(leaf) + r"(?![A-Za-z0-9_])", body):
                used.append(segs)
        if len(used) == len(leaves):
            continue
        vis = re.search(r"pub(?:\([^)]*\))?\s+$", code[:kw])
        s = vis.start() if vis else kw
        if not used:
            line_end = text.find("\n", stmt_end)
            out = out[:s] + out[(line_end + 1 if line_end >= 0 and not out[stmt_end:line_end].strip() else stmt_end):]
            continue
        prefix = out[s:start]
        stmts = [prefix + "::".join(u) + ";" for u in used]
        out = out[:s] + "\n".join(stmts) + out[stmt_end:]
    return out


# ---- manifests
def dep_kinds(st, keep, n):
    """The new crates crate `n`'s code names: normal (library and bin code), dev (tests, examples), build."""
    normal, dev, build = set(), set(), set()
    for f in keep:
        refs, inlines = collect_refs(f, st.tops)
        for r in refs:
            res = resolve(f, r.segs, st.tops, enclosing_inline(inlines, r.offset))
            if not res or res[0] not in LAYOUT:
                continue
            for at in use_parts(f, r):
                if file_dest(f, at)[0] != n:
                    continue
                t = target_dest(*res)[0]
                if t == n:
                    continue
                kind = dependency_kind(f, r.offset, inlines)
                if f.root_kind == "build":
                    build.add(t)
                elif f.root_kind in ("test", "example") or (kind == "dev" and is_test_file(f, enclosing_inline(inlines, r.offset))):
                    dev.add(t)
                else:
                    normal.add(t)
                    if r.segs == ["ee_won as _"] and t == "ee_won_net":
                        normal |= {"ee_won_gui", "ee_won_lobby"}   # linked in with it (Rewriter.use_tree)
    return normal, dev - normal, build


def toml_value(v):
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, str):
        return '"' + v.replace("\\", "\\\\").replace('"', '\\"') + '"'
    if isinstance(v, list):
        return "[" + ", ".join(toml_value(x) for x in v) + "]"
    if isinstance(v, dict):
        return "{ " + ", ".join(f"{toml_key(k)} = {toml_value(x)}" for k, x in v.items()) + " }"
    return str(v)


def toml_key(k):
    return k if re.fullmatch(r"[A-Za-z0-9_-]+", k) else toml_value(k)


def merge_dep(a, b):
    """Two specs of one external dependency, merged (features unioned; the more precise version kept)."""
    if isinstance(a, str):
        a = {"version": a}
    if isinstance(b, str):
        b = {"version": b}
    out = dict(a)
    for k, v in b.items():
        if k == "features":
            out[k] = sorted(set(out.get(k, [])) | set(v))
        elif k == "version" and k in out and out[k] != v:
            out[k] = max(out[k], v, key=lambda s: (s.split(".")[:2], len(s)))
            if out[k].split(".")[:2] != v.split(".")[:2] or a.get("version", v).split(".")[:2] != v.split(".")[:2]:
                raise SystemExit(f"two versions of one dependency: {a} and {b}")
        else:
            out.setdefault(k, v)
    return out


PATH_DEP_LINE = re.compile(r'^(ee_[a-z0-9_]+) = \{ path = "\.\./([a-z0-9_]+)"(.*)\}\n', re.M)


def kept_manifest_text(n, st, keep, dissolved):
    """The manifest of a crate that keeps its name: its own text, its path dependencies mapped to the new crates
    (and the ones its code now names added)."""
    text = (CRATES / n / "Cargo.toml").read_text(encoding="utf-8").replace("\r\n", "\n")
    seen = {}

    def sub(m):
        new = dest(m.group(2), None)[0] if m.group(2) in LAYOUT else m.group(2)
        section = text[:m.start()].rsplit("\n[", 1)[-1].split("]")[0]
        if new == n or (section, new) in seen:
            return ""
        seen[(section, new)] = True
        return f'{new} = {{ path = "../{new}"{m.group(3)}}}\n'
    text = PATH_DEP_LINE.sub(sub, text)
    if n == "ee_data" and "ee_data_cabi" in dissolved:
        text = text.replace('[lib]\npath = "src/lib.rs"\n', '[lib]\npath = "src/lib.rs"\n# and the staticlib of the C++ gate '
                            'tool (tools/eedata/cppgate.py; it was the crate ee_data_cabi)\ncrate-type = ["rlib", "staticlib"]\n')
    if (CRATES / n / "src" / "lib.rs").is_file():
        normal, dev, build = dep_kinds(st, keep, n)
        have = {new for (sec, new) in seen if sec == "dependencies"}
        missing = sorted(normal - have)
        if missing:
            add = "".join(f'{t} = {{ path = "../{t}" }}\n' for t in missing)
            text = re.sub(r"^\[dependencies\]\n", "[dependencies]\n" + add, text, count=1, flags=re.M) \
                if re.search(r"^\[dependencies\]\n", text, re.M) else text.rstrip("\n") + "\n\n[dependencies]\n" + add
    return text


def manifest_text(n, keep, st, dissolved):
    olds = sorted({f.krate for f in keep if file_dest(f)[0] == n})
    if olds == [n]:
        return kept_manifest_text(n, st, keep, dissolved)
    sections = collections.OrderedDict()   # section name -> {key: spec}
    bins, features, crate_types = [], {}, None
    for k in olds:
        m = manifest(k)
        lib = m.get("lib", {})
        if "crate-type" in lib and dest(k, None) == (n, []):
            crate_types = lib["crate-type"]
        root_here = dest(k, None)[0] == n
        for b in m.get("bin", []) if root_here else ():
            bins.append(dict(b))   # bins keep their path inside the crate (src/bin/...)
        for fk, fv in m.get("features", {}).items() if root_here else ():
            features[fk] = fv
        for sec in ("dependencies", "dev-dependencies", "build-dependencies"):
            for dk, dv in m.get(sec, {}).items():
                add_external(sections, sec, dk, dv, k, n)
        for tgt, tv in m.get("target", {}).items():
            for sec in ("dependencies", "dev-dependencies", "build-dependencies"):
                for dk, dv in tv.get(sec, {}).items():
                    add_external(sections, f"target.{toml_value(tgt) if not tgt.startswith('cfg') else repr_cfg(tgt)}.{sec}", dk, dv, k, n)
    if n == "ee_data" and "ee_data_cabi" in dissolved:
        crate_types = ["rlib", "staticlib"]
    if n == "ee_game":
        crate_types = ["staticlib", "rlib"]
    normal, dev, build = dep_kinds(st, keep, n)
    # path dev-dependencies with features (ee_ui's no_cpp for the screens' tests)
    for k in olds:
        for dk, dv in manifest(k).get("dev-dependencies", {}).items():
            if isinstance(dv, dict) and "path" in dv and dk in LAYOUT and dv.get("features"):
                t = dest(dk, None)[0]
                if t != n:
                    sections.setdefault("dev-dependencies", {})[t] = {"path": f"../{t}", "features": dv["features"]}
    lines = ["[package]", f'name = "{n}"', "version.workspace = true", "edition.workspace = true",
             "publish.workspace = true", ""]
    if olds and (len(olds) > 1 or olds[0] != n):
        lines.insert(0, f"# {n}: " + ", ".join(olds) + " (docs/progress/crate-restructure.md)")
    lines += ["[lib]", f"crate-type = {toml_value(crate_types or ['rlib'])}", ""]
    for b in bins:
        lines += ["[[bin]]"] + [f"{k} = {toml_value(v)}" for k, v in b.items()] + [""]
    extra = MANIFEST_EXTRA.get(n, {})
    for fk, fv in extra.get("features", {}).items():
        features[fk] = fv
    if features:
        lines += ["[features]"] + [f"{toml_key(k)} = {toml_value(v)}" for k, v in features.items()] + [""]
    deps = {t: {"path": f"../{t}"} for t in sorted(normal)}
    deps.update(sections.get("dependencies", {}))
    lines += ["[dependencies]"] + [f"{toml_key(k)} = {toml_value(v)}" for k, v in deps.items()] + [""]
    devs = {t: {"path": f"../{t}"} for t in sorted(dev)}
    devs.update(sections.get("dev-dependencies", {}))
    devs.update(extra.get("dev-dependencies", {}))
    if devs:
        lines += ["[dev-dependencies]"] + [f"{toml_key(k)} = {toml_value(v)}" for k, v in devs.items()] + [""]
    bds = {t: {"path": f"../{t}"} for t in sorted(build)}
    bds.update(sections.get("build-dependencies", {}))
    if bds:
        lines += ["[build-dependencies]"] + [f"{toml_key(k)} = {toml_value(v)}" for k, v in bds.items()] + [""]
    for sec, specs in sections.items():
        if sec.startswith("target."):
            lines += [f"[{sec}]"] + [f"{toml_key(k)} = {toml_value(v)}" for k, v in specs.items()] + [""]
    return "\n".join(lines).rstrip("\n") + "\n"


def repr_cfg(tgt):
    return "'" + tgt + "'"


def add_external(sections, sec, dk, dv, old, n):
    if isinstance(dv, dict) and "path" in dv:
        return    # a path crate: from the references
    key = EXTERNAL_RENAMES.get(n, {}).get(old, {}).get(dk)
    if key:
        dv = dict(dv) if isinstance(dv, dict) else {"version": dv}
        dv["package"] = dk
        dk = key
    s = sections.setdefault(sec, {})
    s[dk] = merge_dep(s[dk], dv) if dk in s else dv


def plan_path_of_bin(k, path):
    n, p = dest(k, None)
    return path   # bins keep their path inside the crate (src/bin/...)


def cbindgen_text(cfg_old):
    """rust/cbindgen.toml for the state: the crate lists mapped to the new crates (renames unchanged)."""
    text = (SRC_ROOT / "rust" / "cbindgen.toml").read_text(encoding="utf-8")
    inc = cfg_old["parse"]["include"]
    extra = cfg_old["parse"]["extra_bindings"]

    def mapped(names, drop_binding):
        out = []
        for k in names:
            n = dest(k, None)[0] if k in LAYOUT else k
            if n not in out:
                out.append(n)
        return out
    new_inc = mapped(inc, False)
    binding = dest("ee_rust", None)[0]
    new_extra = [n for n in mapped(extra, True) if n != binding]
    if WON_AS_CRATES and "won" in ACTIVE:
        for x in ("ee_won_gui", "ee_won_lobby"):
            for lst in (new_inc, new_extra):
                if "ee_won_net" in lst and x not in lst:
                    lst.insert(lst.index("ee_won_net") + 1, x)
    text = re.sub(r"^include = \[[^\]]*\]$", "include = " + toml_value(new_inc), text, count=1, flags=re.M)
    text = re.sub(r"^extra_bindings = \[[^\]]*\]$", "extra_bindings = " + toml_value(new_extra), text, count=1, flags=re.M)
    return text


# ---- the repository's other files
PATH_IN_TEXT = re.compile(r"rust/crates/(ee_[a-z0-9_]+|xtask)((?:/[A-Za-z0-9_.\-]+)*)")
# documents keep their history: only these are rewritten (PORT_LEARNINGS, RUST_PLAN and the lanes' progress files
# describe the tree as it was; the plan's PORT_LEARNINGS section maps the old paths)
DOC_ALLOW = {"CLAUDE.md", "README.md", "PORT.md", "tools/harness/README.md", "docs/DATA_FORMATS.md",
             "docs/MODEL_FORMATS.md", "docs/MODDING_PLAN.md", "docs/AI_RESEARCH.md"}


def map_repo_path(old_crate, sub, plan):
    """A `rust/crates/<old>/<sub>` path of the old tree in the new tree."""
    full = f"rust/crates/{old_crate}{sub}"
    if full in plan.moved:
        return plan.moved[full]
    if old_crate not in LAYOUT or not (CRATES / old_crate).exists():
        return full
    n, p = dest(old_crate, None)
    parts = [x for x in sub.split("/") if x]
    if not parts:
        return crate_dir(n) if not p else f"{crate_dir(n)}/src/" + "/".join(p)
    if parts[0] == "src":
        if len(parts) > 1:
            top = parts[1][:-3] if parts[1].endswith(".rs") else parts[1]
            if top in top_modules_cache().get(old_crate, ()):
                tn, tp = dest(old_crate, top)
                return f"{crate_dir(tn)}/src/" + "".join(x + "/" for x in tp[:-1]) + "/".join(parts[1:])
        return f"{crate_dir(n)}/src/" + "".join(x + "/" for x in p) + "/".join(parts[1:])
    return f"{crate_dir(n)}/" + "/".join(parts)


_tops = {}


def top_modules_cache():
    if not _tops:
        _tops.update(top_modules(all_files()))
    return _tops


def rewrite_repo_paths(text, plan):
    def sub(m):
        new = map_repo_path(m.group(1), m.group(2), plan)
        if new.endswith("/mod.rs") and not m.group(2).endswith(".rs"):
            new = new[:-len("/mod.rs")]
        return new
    return PATH_IN_TEXT.sub(sub, text)


XTASK_HEADER_OF = '''/// The crates the restructure merged (docs/progress/crate-restructure.md). Until the headers are split by module, a
/// merged crate's module keeps the header of the crate it was (`src/units/` of ee_sim: `ee_units`), the root of ee_game
/// is ee_rust's (the common header), and the three WON crates share `ee_won`.
const MERGED: &[&str] = &["ee_base", "ee_sim", "ee_render", "ee_audio", "ee_net", "ee_game"];

/// The header a source file's items go to (see [`MERGED`]).
fn header_of(crates_dir: &Path, krate: &str, file: &Path) -> String {
    if krate.starts_with("ee_won_") {
        return "ee_won".to_string();
    }
    let src = crates_dir.join(krate).join("src");
    // ee_net is also the name of the crate before (its root then has no module "before the restructure")
    let merged = MERGED.contains(&krate)
        && std::fs::read_to_string(src.join("lib.rs")).is_ok_and(|t| t.contains("before the restructure."));
    if !merged {
        return krate.to_string();
    }
    let first = file.strip_prefix(&src).ok().and_then(|p| p.components().next()).and_then(|c| c.as_os_str().to_str());
    match first {
        Some("lib.rs") | None if krate == "ee_game" => "ee_rust".to_string(),
        Some("lib.rs") | None => krate.to_string(),
        Some(name) => format!("ee_{}", name.strip_suffix(".rs").unwrap_or(name)),
    }
}

'''
FIXUPS += [
    ("base", "rust/crates/xtask/src/split.rs", "            scan_text(krate, &text, &mut defs);",
     "            scan_text(&header_of(crates_dir, krate, file), &text, &mut defs);"),
    ("base", "rust/crates/xtask/src/split.rs",
     "/// Every definition in the sources of the named crates (`rust/crates/<name>/src`), test sources left out.",
     XTASK_HEADER_OF + "/// Every definition in the sources of the named crates (`rust/crates/<name>/src`), test sources left out."),
]


# ---- writing a state
TEXT_SUFFIXES = {".py", ".sh", ".toml", ".json", ".cpp", ".h", ".c", ".inl", ".hpp", ".mm", ".txt", ".md", ".yaml",
                 ".yml", ".cmake", ".rs", ".ps1", ".bat", ".cfg", ".ini", ".swift", ".tsv"}


def git(*args, cwd=None, data=None):
    r = subprocess.run(["git", *args], cwd=cwd or ROOT, input=data, capture_output=True)
    if r.returncode:
        raise SystemExit(f"git {' '.join(args)}: {r.stderr.decode(errors='replace')}")
    return r.stdout


def snapshot(rev):
    """The snapshot's rust/ tree, extracted once into build/crates/snap-<sha>; returns (root, sha)."""
    sha = git("rev-parse", rev).decode().strip()
    root = ROOT / "build" / "crates" / f"snap-{sha[:12]}"
    if not (root / "rust" / "Cargo.toml").is_file():
        root.mkdir(parents=True, exist_ok=True)
        tar = git("archive", "--format=tar", sha, "rust")
        import io
        import tarfile
        with tarfile.open(fileobj=io.BytesIO(tar)) as t:
            t.extractall(root, filter="data")
    return root, sha


def snapshot_files(sha):
    """Every file of the snapshot outside rust/crates, as {path: bytes}, text files only."""
    names = git("ls-tree", "-r", "--name-only", sha).decode().split("\n")
    names = [n for n in names if n and not n.startswith("rust/crates/") and pathlib.PurePosixPath(n).suffix in
             TEXT_SUFFIXES]
    req = "".join(f"{sha}:{n}\n" for n in names).encode()
    out = git("cat-file", "--batch", data=req)
    files, pos = {}, 0
    for n in names:
        nl = out.index(b"\n", pos)
        header = out[pos:nl].split()
        size = int(header[2])
        files[n] = out[nl + 1:nl + 1 + size]
        pos = nl + 1 + size + 1
    return files


def write_state(upto, out_root, rev, use_git, dry_run):
    snap, sha = snapshot(rev)
    set_source_root(snap)
    _tops.clear()
    plan, files = build_plan(upto)
    out_root = pathlib.Path(out_root).resolve()
    unused = [FIXUPS[i][:2] for i in range(len(FIXUPS)) if FIXUPS[i][0] in ACTIVE and not _fixups_used[i]
              and FIXUPS[i][1].startswith("rust/crates/")]
    changes = {}
    # rust/crates: every file of the state
    for new, text in plan.out.items():
        if text is None:
            continue
        text = rewrite_repo_paths(text, plan)
        if new in plan.crlf:
            text = text.replace("\n", "\r\n")
        changes[new] = text.encode("utf-8")
    for old in plan.copied:
        changes[plan.moved[old]] = (snap / old).read_bytes()
    # the other files: fixups and paths
    other = snapshot_files(sha)
    for path, data in other.items():
        try:
            raw = data.decode("utf-8")
        except UnicodeDecodeError:
            continue
        crlf = "\r\n" in raw
        text = raw.replace("\r\n", "\n")
        new = apply_fixups(path, text)
        md = path.endswith(".md")
        if not md or path in DOC_ALLOW:
            if not path.startswith(("tools/crate_move.py", "include/rust/")):   # generated headers: xtask writes them
                new = rewrite_repo_paths(new, plan)
        if path == "rust/cbindgen.toml":
            new = plan.cbindgen
        if new != text:
            changes[path] = (new.replace("\n", "\r\n") if crlf else new).encode("utf-8")
    for i in range(len(FIXUPS)):
        if FIXUPS[i][0] in ACTIVE and not _fixups_used[i] and not FIXUPS[i][1].startswith("rust/crates/"):
            unused.append(FIXUPS[i][:2])
    if unused:
        raise SystemExit(f"fixups that applied nowhere: {unused}")
    print(f"state {upto}: {len(plan.out)} files under rust/crates, {len(changes)} written in all")
    if dry_run:
        for p in sorted(changes)[:40]:
            print("  ", p)
        return 0
    # remove the old crate tree, write the new one
    crates_out = out_root / "rust" / "crates"
    if use_git:
        git("rm", "-r", "-q", "--cached", "rust/crates", cwd=out_root)
    for p in sorted(crates_out.rglob("*"), reverse=True):
        if p.is_file():
            p.unlink()
        elif p.is_dir() and not any(p.iterdir()):
            p.rmdir()
    for path, data in changes.items():
        dst = out_root / path
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(data)
    if use_git:
        git("add", "-A", "rust/crates", cwd=out_root)
        others = sorted(p for p in changes if not p.startswith("rust/crates/"))
        for i in range(0, len(others), 100):
            git("add", "--", *others[i:i + 100], cwd=out_root)
    return 0


WON_ORDER = {"won::net": 0, "won::gui": 1, "won::lobby": 2}


def won(markdown):
    files = [f for f in all_files()]
    tops = top_modules(files)
    won_files = [f for f in files if f.krate == "ee_won"]
    out = []
    w = out.append
    w("## ee_won's groups\n")
    w("| Group | Modules | Files | Lines |")
    w("|---|---:|---:|---:|")
    for g in ("net", "gui", "lobby"):
        fs = [f for f in won_files if f.top and won_group(f.top) == g]
        w(f"| {g} | {len({f.top for f in fs})} | {len(fs)} | {sum(f.text.count(chr(10)) for f in fs)} |")
    root = [f for f in won_files if not f.top]
    w(f"| (lib.rs) | - | {len(root)} | {sum(f.text.count(chr(10)) for f in root)} |")
    w("")
    edges = edges_of(won_files, tops, won_key)
    w("## Paths between the groups and out of ee_won\n")
    w("| From | To | Refs (normal / dev) | Modules -> modules | Direction |")
    w("|---|---|---|---:|---|")
    for (a, b), refs in sorted(edges.items()):
        kinds = collections.Counter(r[3] for r in refs)
        pairs = {(r[0].top, r[5]) for r in refs}
        if a in WON_ORDER and b in WON_ORDER:
            d = "down" if WON_ORDER[b] < WON_ORDER[a] else "**up**"
        else:
            d = "out"
        w(f"| {a} | {b} | {kinds['normal']} / {kinds['dev']} | {len(pairs)} | {d} |")
    w("")
    w("## The upward paths (each would be a cycle between crates)\n")
    for (a, b), refs in sorted(edges.items()):
        if not (a in WON_ORDER and b in WON_ORDER and WON_ORDER[b] > WON_ORDER[a]):
            continue
        w(f"### {a} -> {b}\n")
        per = collections.defaultdict(list)
        for r in refs:
            per[(r[0].top, r[5])].append(r)
        for (src, dst), rs in sorted(per.items(), key=lambda kv: (str(kv[0][0]), str(kv[0][1]))):
            names = sorted({r[2] for r in rs})
            kinds = "/".join(sorted({r[3] for r in rs}))
            w(f"- `{src}` -> `{dst}` ({len(rs)}, {kinds}): " + ", ".join(f"`{n}`" for n in names[:6])
              + (" ..." if len(names) > 6 else "") + f" ({fmt_file(rs[0][0], rs[0][1])})")
        w("")
    text = "\n".join(out) + "\n"
    if markdown:
        pathlib.Path(markdown).write_text(text, encoding="utf-8", newline="\n")
    else:
        sys.stdout.write(text)
    return 0


def main():
    load_won_groups()
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="command", required=True)
    for name in ("analyze", "won", "headers"):
        p = sub.add_parser(name)
        p.add_argument("--markdown")
    p = sub.add_parser("move", help="write the tree of a state of the move, made from the snapshot")
    p.add_argument("--to", choices=STEPS, default=STEPS[-1], help="the last step to apply (default: all)")
    p.add_argument("--from", dest="rev", required=True, help="the commit before the restructure (the snapshot)")
    p.add_argument("--out", default=str(ROOT), help="the tree to write (default: this checkout)")
    p.add_argument("--no-git", action="store_true", help="write files only (no git rm/add)")
    p.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    if a.command == "move":
        return write_state(a.to, a.out, a.rev, not a.no_git, a.dry_run)
    if a.command == "analyze":
        return analyze(a.markdown)
    if a.command == "won":
        return won(a.markdown)
    if a.command == "headers":
        return headers(a.markdown)
    raise SystemExit(f"{a.command}: not yet")


if __name__ == "__main__":
    sys.exit(main())
