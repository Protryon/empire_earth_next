"""Offsets of game classes, by member name, for a build (and VC6 offsets translated into it).

  import layout as L
  lay = L.Layout(build_dir)                   # <build_dir>/layout.json (layout_gen.py; tools/build.py writes it)
  lay.field("UIManager", "mFormStack")        # offset of the member
  lay.first("UIManager", "mFormStack")        # offset of a std::vector's first-element pointer
  lay.last("UIManager", "mFormStack")         # ... its end pointer
  lay.size("UIForm")
  lay.translate("EEWorld", 0xcc)              # the member at VC6 offset 0xcc, in this build

The reference for VC6 offsets (the original image's layout, which ee's harness and the exe's member table use) is
tools/harness/vc6_layout.json.gz: the record layouts of the last build with VC6's layout (Dinkum 2.33 headers),
which reproduced all 3,386 member offsets documented in include/. Dinkum's vector there is {allocator, _First,
_Last, _End}; MSVC's is {_Myfirst, _Mylast, _Myend} inside a compressed pair.
"""
import gzip
import json
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
REFERENCE = HERE / "vc6_layout.json.gz"
FIRST = ("_First", "_Myfirst")
LAST = ("_Last", "_Mylast")


class Layout:
    def __init__(self, build_dir=None, records=None):
        if records is None:
            path = pathlib.Path(build_dir) / "layout.json"
            if not path.exists():
                raise SystemExit(f"{path} is missing: python -I tools/harness/layout_gen.py --out {build_dir}")
            records = json.loads(path.read_text())
        self.records = records

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

    def translate(self, cls, off):
        """A VC6 offset into cls -> the same member's offset in this build. The member is identified in the VC6
        reference: an exact member (the shortest path at that offset; a container's _First/_Last become this
        library's first/last), else the top-level member the offset falls in, plus the delta (an array element)."""
        f = reference()._fields(cls)
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
            raise KeyError(f"{cls}+{off:#x}: no member at or before it in the VC6 layout")
        v, p = max(tops)
        return self.field(cls, p) + (off - v)

    def constants(self, spec):
        """{NAME: (kind, class, member)} -> {NAME: offset} (kind: field, first, last, size)."""
        return {name: self.size(cls) if kind == "size" else getattr(self, kind)(cls, member)
                for name, (kind, cls, member) in spec.items()}


_REF = None
_CURRENT = None


def reference():
    global _REF
    if _REF is None:
        _REF = Layout(records=json.loads(gzip.decompress(REFERENCE.read_bytes())))
    return _REF


def use(build_dir):
    """Select the build the harness runs (game_session.py); off() translates for it."""
    global _CURRENT
    _CURRENT = Layout(build_dir)


def off(cls, vc6_offset):
    """A VC6 offset into cls (as ee's harness writes them), translated for the build in use."""
    return _CURRENT.translate(cls, vc6_offset) if _CURRENT else vc6_offset
