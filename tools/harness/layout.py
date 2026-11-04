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

    def constants(self, spec):
        """{NAME: (kind, class, member)} -> {NAME: offset} (kind: field, first, last, size)."""
        out = {}
        for name, (kind, cls, member) in spec.items():
            out[name] = self.size(cls) if kind == "size" else getattr(self, kind)(cls, member)
        return out


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
