"""Offsets of game classes for the harness, by member name, for the build profile under test.

  import layout as L
  lay = L.Layout("msvc")            # build/harness/layout-msvc.json (tools/harness/layout_gen.py)
  lay.field("UIManager", "mFormStack")        # offset of the member
  lay.first("UIManager", "mFormStack")        # offset of a std::vector's first-element pointer
  lay.last("UIManager", "mFormStack")         # ... its end pointer
  lay.size("UIForm")

Dinkum's vector is {allocator, _First, _Last, _End}; MSVC's is {_Myfirst, _Mylast, _Myend} inside a compressed pair.
first()/last() find the pointer under the member whatever the library.
"""
import json
import pathlib
import re

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
FIRST = ("_First", "_Myfirst")
LAST = ("_Last", "_Mylast")


class Layout:
    def __init__(self, stl):
        path = ROOT / "build" / "harness" / f"layout-{stl}.json"
        if not path.exists():
            raise SystemExit(f"{path} is missing: python -I tools/harness/layout_gen.py --stl {stl}")
        self.stl, self.records = stl, json.loads(path.read_text())

    def _fields(self, cls):
        if cls not in self.records:
            raise KeyError(f"no layout for class {cls} (add a unit that includes it to layout_gen.UNITS)")
        return self.records[cls]["fields"]

    def field(self, cls, path):
        f = self._fields(cls)
        if path not in f:
            raise KeyError(f"{cls} has no member {path}")
        return f[path]

    def size(self, cls):
        self._fields(cls)
        return self.records[cls]["size"]

    def _sub(self, cls, member, names):
        hits = [off for p, off in self._fields(cls).items()
                if p.startswith(member + ".") and p.rsplit(".", 1)[-1] in names]
        if len(hits) != 1:
            raise KeyError(f"{cls}::{member}: expected one of {names} under it, found {len(hits)}")
        return hits[0]

    def first(self, cls, member):
        return self._sub(cls, member, FIRST)

    def last(self, cls, member):
        return self._sub(cls, member, LAST)

    def translate(self, cls, off, ref=None):
        """A VC6/Dinkum offset into cls (as ee's harness writes them) -> the same member's offset in this build. The
        member is identified in the Dinkum layout (identical to VC6's, layout_gen.py --check): an exact member
        (the shortest path at that offset; a container's _First/_Last become this library's first/last), else the
        top-level member the offset falls in, plus the delta (an array element)."""
        if self.stl == "dinkum":
            return off
        ref = ref or _reference()
        f = ref._fields(cls)
        for p in sorted((p for p, v in f.items() if v == off), key=lambda p: (p.count("."), len(p))):
            leaf = p.rsplit(".", 1)
            if len(leaf) == 2 and leaf[1] in FIRST:
                return self.first(cls, leaf[0])
            if len(leaf) == 2 and leaf[1] in LAST:
                return self.last(cls, leaf[0])
            if p in self._fields(cls):
                return self._fields(cls)[p]
        tops = [(v, p) for p, v in f.items() if "." not in p and v <= off]
        if not tops:
            raise KeyError(f"{cls}+{off:#x}: no member at or before it in the Dinkum layout")
        v, p = max(tops)
        return self.field(cls, p) + (off - v)

    def constants(self, spec):
        """{NAME: (kind, class, member)} -> {NAME: offset} (kind: field, first, last, size)."""
        out = {}
        for name, (kind, cls, member) in spec.items():
            out[name] = self.size(cls) if kind == "size" else getattr(self, kind)(cls, member)
        return out


_REF = None
_CURRENT = None


def _reference():
    global _REF
    if _REF is None:
        _REF = Layout("dinkum")
    return _REF


def use(stl):
    """Select the build profile the harness runs (game_session.py); off() translates for it."""
    global _CURRENT
    _CURRENT = Layout(stl) if stl != "dinkum" else None


def off(cls, vc6_offset):
    """ee's VC6 offset into cls, translated for the build profile in use (identity for Dinkum/VC6 layouts)."""
    return _CURRENT.translate(cls, vc6_offset) if _CURRENT else vc6_offset


CLASS = re.compile(r"^(?:template\s*<[^>]*>\s*)?(class|struct)\s+(?:\w+\s+)?(\w+)\b[^;]*$")
MEMBER = re.compile(r"^\s+[^/(]*?\b(\w+)(\[[^\]]*\])?\s*;\s*//\s*\+0x([0-9a-fA-F]+)\b")


def header_offsets():
    """{class: {member: VC6 offset}} from the `// +0x..` comments in include/ (members declared on one line)."""
    out = {}
    for h in (ROOT / "include").rglob("*.h"):
        if "dinkum" in h.parts:
            continue
        cls, depth = None, 0
        for line in h.read_text(encoding="latin-1").splitlines():
            code = line.split("//")[0]
            if depth == 0:
                m = CLASS.match(line)
                if m and "{" in code:
                    cls = m.group(2)
            if cls and depth == 1:
                m = MEMBER.match(line)
                if m and "(" not in code:
                    out.setdefault(cls, {})[m.group(1)] = int(m.group(3), 16)
            depth += code.count("{") - code.count("}")
            if depth <= 0:
                depth, cls = 0, None
    return out
