"""The game's language strings as a C++ table, generated from Language.rc's STRINGTABLE blocks.

  python -I tools/lang_strings.py            writes src/Empire Earth/Language/LanguageStrings.cpp
  python -I tools/lang_strings.py --check    fails when that file is not what Language.rc gives (build.py runs it)
  python -I tools/lang_strings.py --check-res FILE.res
                                             also compares the table with the string tables rc.exe compiled

UResourceFile::GetString used to read them with LoadStringA from the exe's resources (Language.dll's, linked in);
the table holds each string as UTF-8 (docs/RUST_PLAN.md decision D6: the game's text is UTF-8; LoadStringA gave the
ANSI code page's bytes), with an empty string counting as missing (LoadStringA returns 0 for it). The table's language
is the script's LANGUAGE statement (MAKELANGID). Strings are sorted by id for a binary search. The same table serves
every system, so the POSIX game needs no resource reader.
"""
import argparse
import pathlib
import re
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
RC = ROOT / "src" / "Empire Earth" / "Language" / "Language.rc"
OUT = ROOT / "src" / "Empire Earth" / "Language" / "LanguageStrings.cpp"
ENTRY = re.compile(r'^\s*(-?\d+|0x[0-9a-fA-F]+)\s*,\s*L?"((?:[^"]|"")*)"\s*$')


def unescape(s):
    """An rc string literal's body -> its text (rc.exe's escapes: "" \\n \\t \\r \\a \\\\ \\" \\xHHHH \\ooo)."""
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c == '"' and s[i + 1:i + 2] == '"':
            out.append('"')
            i += 2
        elif c == "\\" and i + 1 < len(s):
            n = s[i + 1]
            if n in "nrtab\\\"":
                out.append({"n": "\n", "r": "\r", "t": "\t", "a": "\a", "b": "\b", "\\": "\\", '"': '"'}[n])
                i += 2
            elif n in "xX":
                m = re.match(r"[0-9a-fA-F]{1,4}", s[i + 2:])
                out.append(chr(int(m.group(0), 16)))
                i += 2 + len(m.group(0))
            elif n in "01234567":
                m = re.match(r"[0-7]{1,3}", s[i + 1:])
                out.append(chr(int(m.group(0), 8)))
                i += 1 + len(m.group(0))
            else:
                out.append(n)
                i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def parse_rc(path=RC):
    """-> (language id, {id: text})"""
    text = path.read_text(encoding="utf-8")
    m = re.search(r"^LANGUAGE\s+(0x[0-9a-fA-F]+|\d+)\s*,\s*(0x[0-9a-fA-F]+|\d+)", text, re.M)
    language = (int(m.group(2), 0) << 10) | int(m.group(1), 0) if m else 0x409
    strings, inside = {}, False
    lines = text.splitlines()
    for i, line in enumerate(lines):
        s = line.strip()
        if s == "STRINGTABLE":
            inside = None
            continue
        if inside is None and s == "BEGIN":
            inside = True
            continue
        if inside and s == "END":
            inside = False
            continue
        if inside:
            e = ENTRY.match(line)
            if not e:
                sys.exit(f"{path.name}:{i + 1}: not a string entry: {line!r}")
            sid = int(e.group(1), 0) & 0xffff
            if sid in strings:
                sys.exit(f"{path.name}:{i + 1}: string {sid} twice")
            strings[sid] = unescape(e.group(2))
    return language, strings


def parse_res(path):
    """The RT_STRING entries of a compiled .res -> {id: text}"""
    data = pathlib.Path(path).read_bytes()
    strings, pos = {}, 0
    while pos + 8 <= len(data):
        size, header = struct.unpack_from("<II", data, pos)
        p = pos + 8

        def ordinal_or_name(p):
            if struct.unpack_from("<H", data, p)[0] == 0xffff:
                return struct.unpack_from("<H", data, p + 2)[0], p + 4
            q = p
            while struct.unpack_from("<H", data, q)[0]:
                q += 2
            return data[p:q].decode("utf-16-le"), q + 2
        rtype, p = ordinal_or_name(p)
        name, p = ordinal_or_name(p)
        body = pos + header
        if rtype == 6 and isinstance(name, int):
            q = body
            for k in range(16):
                n = struct.unpack_from("<H", data, q)[0]
                if n:
                    strings[((name - 1) << 4) | k] = data[q + 2:q + 2 + 2 * n].decode("utf-16-le")
                q += 2 + 2 * n
        pos = (body + size + 3) & ~3
    return strings


def utf8(text):
    return text.encode("utf-8")


def c_literal(b):
    out = []
    for x in b:
        c = chr(x)
        if c == "\\":
            out.append("\\\\")
        elif c == '"':
            out.append('\\"')
        elif c == "\n":
            out.append("\\n")
        elif c == "\t":
            out.append("\\t")
        elif c == "\r":
            out.append("\\r")
        elif 0x20 <= x < 0x7f and c != "?":   # '?' escaped: no trigraphs
            out.append(c)
        else:
            out.append(f"\\{x:03o}")
    return '"' + "".join(out) + '"'


def generate():
    language, strings = parse_rc()
    rows = [(sid, utf8(t)) for sid, t in sorted(strings.items()) if t]
    lines = ["// GENERATED by tools/lang_strings.py from Language.rc: do not edit (python -I tools/lang_strings.py).",
             "// The language strings UResourceFile::GetString returns (UTF-8, decision D6), sorted by id.",
             '#include "lle/UResourceStrings.h"',
             "",
             f"extern const unsigned short gResourceStringsLanguage = 0x{language:x};",
             f"extern const unsigned long gResourceStringCount = {len(rows)};",
             "extern const UResourceString gResourceStrings[] = {"]
    for sid, b in rows:
        lines.append(f"    {{{sid}, {c_literal(b)}}},")
    lines.append("};")
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--check-res", type=pathlib.Path)
    a = ap.parse_args()
    text = generate()
    if a.check_res:
        _, rc = parse_rc()
        res = parse_res(a.check_res)
        rc = {k: v for k, v in rc.items() if v}
        bad = [k for k in sorted(set(rc) | set(res)) if rc.get(k) != res.get(k)]
        if bad:
            sys.exit(f"{len(bad)} strings differ from {a.check_res}: first {bad[:10]}")
        print(f"{len(rc)} strings equal to {a.check_res.name}'s")
    if a.check:
        have = OUT.read_bytes().decode("latin-1") if OUT.exists() else ""
        if have != text:
            sys.exit(f"{OUT} is stale: run python -I tools/lang_strings.py")
        return
    if not a.check_res:
        OUT.write_bytes(text.encode("latin-1"))
        print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
