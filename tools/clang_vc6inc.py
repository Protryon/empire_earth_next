"""Patched copy of the stock VC6 headers for clang-cl (the toolchain package itself stays untouched).

prepare(src, dst) mirrors src (toolchain/msvc6.0/Include) into dst and applies PATCHES: per header, a list of
(regex, replacement, expected count). A patch that no longer matches its expected count stops the build, so a changed
toolchain cannot silently skip one. Each patch fixes something clang rejects that VC6 accepted; none changes what the
code means.
"""
import pathlib
import re

REDEFAULT = r"template<class _E, class _Tr = char_traits<_E> >\n(\tclass (?:{names})\b)"
# ios_base flag constants whose names are also global manipulator functions (std::skipws(ios_base&) ...): inside the
# stream templates they live in a dependent base, so clang's lookup finds the function. Qualify them.
MANIP_FLAGS = r"(?<![\w:.])(skipws|unitbuf|boolalpha|showbase|showpoint|showpos|uppercase|left|right|internal|dec|hex|oct|fixed|scientific)\b(?!\s*\()"
PATCHES = {
    # Class templates declared in <iosfwd> with their default argument: repeating the default on the definition is
    # ill-formed (VC6 accepted it). {names} is filled in from <iosfwd>.
    "utility": [(REDEFAULT, r"template<class _E, class _Tr>\n\1", 2)],
    "streambuf": [(REDEFAULT, r"template<class _E, class _Tr>\n\1", 1)],
    "ios": [(REDEFAULT, r"template<class _E, class _Tr>\n\1", 1)],
    "ostream": [(REDEFAULT, r"template<class _E, class _Tr>\n\1", 1), (MANIP_FLAGS, r"ios_base::\1", None)],
    "istream": [(REDEFAULT, r"template<class _E, class _Tr>\n\1", 2), (MANIP_FLAGS, r"ios_base::\1", None)],
    # explicit specialisations written without template<>
    "xlocale": [(r"^class _CRTIMP (\w+<)", r"template<> class _CRTIMP \1", None)],
}


def iosfwd_names(src):
    text = (src / "iosfwd").read_text(encoding="latin-1")
    return sorted(set(re.findall(r"template<class _E, class _Tr = char_traits<_E> >\s*class (\w+);", text)))


def prepare(src, dst):
    src, dst = pathlib.Path(src), pathlib.Path(dst)
    dst.mkdir(parents=True, exist_ok=True)
    names = "|".join(iosfwd_names(src))
    for f in sorted(src.rglob("*")):
        if not f.is_file():
            continue
        data = f.read_bytes()
        if f.name.lower() in PATCHES:
            text = data.decode("latin-1").replace("\r\n", "\n")
            for pat, rep, want in PATCHES[f.name.lower()]:
                pat = pat.replace("{names}", names)
                text, n = re.subn(pat, rep, text, flags=re.M)
                if n == 0 if want is None else n != want:
                    raise SystemExit(f"clang_vc6inc: {f.name}: patch {pat!r} matched {n} times, expected {want}")
            crlf = b"\r\n" in data
            data = (text.replace("\n", "\r\n") if crlf else text).encode("latin-1")
        g = dst / f.relative_to(src)
        g.parent.mkdir(parents=True, exist_ok=True)
        if not g.exists() or g.read_bytes() != data:
            g.write_bytes(data)
    return dst
