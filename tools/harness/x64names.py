"""The x64 build's name for a symbol the harness knows by its 32-bit (i686) MSVC decoration.

The harness names the game's functions and globals by their i686 decorated names (`?Enter@TSCriticalSection@@QAEXXZ`,
`?gWorld@@3VEEWorld@@A`, `_GXTestReadFrame`). An x64 build decorates the same declarations differently: `__ptr64`
pointers and `this` (`PEAV`, `QEAA`), one calling convention, no leading underscore or `@N` suffix on C names. LinkMap
(fulllink_locator.py) asks x64_name for a name it does not find in an x64 map:

  1. the mechanical rewrite (rules below); taken when the map has it;
  2. otherwise the map's names with the same leading identifier whose undecorated signature equals the i686 one's
     after dropping `__ptr64` and the calling conventions (dbghelp UnDecorateSymbolName); exactly one must match.

  python -I tools/harness/x64names.py <i686 name>.. --names FILE   (check against a list of x64 names, one per line)
"""
import ctypes
import re
import sys

_UND = None


def undecorate(name):
    """dbghelp's full undecoration (UNDNAME_COMPLETE); the name itself when it is not decorated."""
    global _UND
    if _UND is None:
        _UND = ctypes.WinDLL("dbghelp").UnDecorateSymbolName
        _UND.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32]
        _UND.restype = ctypes.c_uint32
    out = ctypes.create_string_buffer(4096)
    n = _UND(name.encode("latin-1"), out, len(out), 0)
    return out.value[:n].decode("latin-1") if n else name


def signature(name):
    """The undecorated signature with what differs between i686 and x64 removed."""
    s = undecorate(name)
    s = re.sub(r"\b__(ptr64|cdecl|thiscall|stdcall|fastcall|vectorcall)\b", "", s)
    return re.sub(r"\s+", " ", s).replace(" ,", ",").replace(" )", ")").replace("( ", "(").strip()


def identifier(name):
    """The leading identifier of a decorated name (`?Enter@...` -> `Enter`, `??0X@@` -> `??0X`), for the index."""
    if name.startswith("??"):
        m = re.match(r"(\?\?[0-9A-Z_]?[^@]*)@", name)
    elif name.startswith("?"):
        m = re.match(r"\?([^@]+)@", name)
    else:
        return c_name(name)
    return m.group(1) if m else name


def c_name(name):
    """i686 C decoration -> x64: `_foo` / `_foo@12` -> `foo`; `__imp__foo@4` -> `__imp_foo`."""
    imp = name.startswith("__imp_")
    base = name[6:] if imp else name
    if base.startswith("_"):
        base = re.sub(r"@\d+$", "", base[1:])
    return ("__imp_" if imp else "") + base


# i686 -> x64 pieces of a C++ decoration (a small parser of the type grammar the harness's names use). Member
# functions: access letter + this cv + E (thiscall) -> access + E (__ptr64 this) + cv + A (cdecl); free and static
# functions have one convention (A); every pointer/reference type gains E (__ptr64) before its cv; pointer data gains E
# before its storage cv. Anything the parser does not know leaves the name as it is (the signature match decides).
def _name(s, i, out):
    """A qualified name inside a type (after V/U/T/W4), through its terminating '@'."""
    while s[i] != "@":
        if s.startswith("?$", i):
            j = s.index("@", i)
            out.append(s[i:j + 1])
            i = _types(s, j + 1, out)
        elif s[i].isdigit():
            out.append(s[i])
            i += 1
        else:
            j = s.index("@", i)
            out.append(s[i:j + 1])
            i = j + 1
    out.append("@")
    return i + 1


def _type(s, i, out):
    c = s[i]
    if c in "PAQRS" and s[i + 1:i + 2] in ("A", "B", "C", "D"):
        out.append(c + "E" + s[i + 1])
        return _type(s, i + 2, out)
    if c in "VUT":
        out.append(c)
        return _name(s, i + 1, out)
    if c == "W":
        out.append(s[i:i + 2])
        return _name(s, i + 2, out)
    if c == "_":
        out.append(s[i:i + 2])
        return i + 2
    if c == "?":
        out.append(s[i:i + 2])
        return _type(s, i + 2, out)
    if c in "$6":
        raise ValueError("unsupported type code")
    out.append(c)
    return i + 1


def _types(s, i, out):
    """Types up to and including the terminating '@'."""
    while s[i] != "@":
        i = _type(s, i, out)
    out.append("@")
    return i + 1


def mechanical(name):
    if not name.startswith("?"):
        return c_name(name)
    try:
        start = (4 if name[2] == "_" else 3) if name.startswith("??") else 1   # ??0 ctor, ??_7 vftable, ?name
        if name.startswith(("??$", "?$")):
            return name
        end = _name(name, start, [])                         # the qualified name, through its '@'
        head, sep, tail = name[:end - 1], "@", name[end:]
        out = []
        if tail[0] in "23":                                  # data: type + storage cv
            i = _type(tail, 1, out)
            cv = tail[i:]
            pointer = tail[1] in "PAQRS" and tail[2:3] in ("A", "B", "C", "D")
            return head + sep + tail[0] + "".join(out) + ("E" + cv if pointer else cv)
        if tail[0] in "QAIUEMRSV" and tail[1:2] in ("A", "B") and tail[2:3] == "E":   # member functions
            prefix, i = tail[0] + "E" + tail[1] + "A", 3
        elif tail[:2] in ("YA", "YG", "SA", "SG", "KA", "CA", "CG", "KG"):              # free / static
            prefix, i = tail[0] + "A", 2
        else:
            return name
        if tail[i] == "@":                                   # constructors/destructors: no return type
            out.append("@")
            i += 1
        else:
            i = _type(tail, i, out)
        if tail[i:] == "XZ":
            out.append("XZ")
        else:
            i = _types(tail, i, out)
            if tail[i:] != "Z":
                return name
            out.append("Z")
        return head + sep + prefix + "".join(out)
    except (ValueError, IndexError):
        return name


class Resolver:
    """x64 names of i686 ones within one set of x64 names (a map's)."""

    def __init__(self, names):
        self.names = set(names)
        self._index = None

    def _ident_index(self):
        if self._index is None:
            self._index = {}
            for n in self.names:
                self._index.setdefault(identifier(n), []).append(n)
        return self._index

    def resolve(self, name):
        """The x64 name, or raises KeyError listing the candidates."""
        if name in self.names:
            return name
        m = mechanical(name)
        if m in self.names:
            return m
        cands = self._ident_index().get(identifier(name), [])
        want = signature(name)
        hits = [c for c in cands if signature(c) == want]
        if len(hits) == 1:
            return hits[0]
        raise KeyError(f"{name}: no single x64 name (mechanical {m}; {len(hits)} of {len(cands)} candidates "
                       f"match '{want}': {hits[:4] or cands[:4]})")


def main():
    args = sys.argv[1:]
    if "--names" not in args:
        sys.exit(__doc__)
    i = args.index("--names")
    names = open(args[i + 1], encoding="latin-1").read().split()
    r = Resolver(names)
    bad = 0
    for n in args[:i] + args[i + 2:]:
        try:
            x = r.resolve(n)
            print(f"{n} -> {x}{'' if x == mechanical(n) else '   (by signature)'}")
        except KeyError as e:
            print(f"MISSING {e}")
            bad += 1
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
