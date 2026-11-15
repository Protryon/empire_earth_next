"""Deletes the comments that recorded where each function or global sat in the original retail binaries.

The reconstruction tagged every function with a reccmp-style marker (`// FUNCTION: EXE 0x4c1722`, `TEMPLATE`,
`SYNTHETIC`, `ICF-FOLDED`, `GLOBAL`, `LIBRARY`, `VTABLE`, `STRING`, `LINE`), often with a mangled name or a note under
it, and many declarations with a trailing `// 0x52f638`. The port is past the original exe, so they go:

  python -I tools/strip_markers.py [--check] [paths...]       # default: src include tests

What it removes (comment blocks are runs of whole-line `//` comments; a block is "attached" when the next line is
code, after any `#pragma` lines, and "orphan" when it is followed by a blank line, the end of the file, another
preprocessor line or a closing brace; a block followed by a preprocessor line keeps its last marker's lines as an
attached block does, since a file's or a section's description sits there):
- every marker line, with the continuation lines of a marker whose parenthesis or list (a trailing comma) runs on;
- in a block, every marker's own lines (the comment lines under it, up to the next marker or a `//` separator),
  except the last marker's in an attached block: those describe the code below and stay, less its name-only lines
  (a mangled name, an undecorated signature, "(compiler-generated; name in ...)", "(no address of its own: ...)");
- in an orphan block, the paragraph that introduces its markers (the lines right above the first one, up to a `//`
  separator); other paragraphs, and the comments above the markers of attached blocks, stay;
- a trailing comment that is only an address (`// 0x52f638`, `// LLE 0x1012d94c`, several joined by `,` or `/`,
  optionally with `(ret 4)`, `(??_G 0x...)`, `(provisional name)`, `(ICF)`, `(inline)` or `(cdecl)`), and a
  whole-line comment of that form that starts its comment block. Only values in the binaries' ranges count.
Never removed: `// UNIT-FLAGS:` and `// UNIT-COMPILER:` lines (the build reads them), and a sentence that mentions
an address. A blank run that a removal joins becomes the run that stood before it (or after it, when there was none).
Bytes are preserved except for the removed lines and comments (line endings, encodings). Rerunning changes nothing.
"""
import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_PATHS = ["src", "include", "tests"]
EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".inc", ".inl"}

MARKER = re.compile(rb"^[ \t]*//+[ \t]*(?:FUNCTION|TEMPLATE|SYNTHETIC|ICF-FOLDED|GLOBAL|LIBRARY|VTABLE|STRING|LINE):"
                    rb"[ \t]*[A-Za-z0-9_]+[ \t]+0x[0-9a-fA-F]+\b")
COMMENT = re.compile(rb"^[ \t]*//")
SEPARATOR = re.compile(rb"^[ \t]*//[ \t]*$")
PROTECTED = re.compile(rb"^[ \t]*//[ \t]*(?:UNIT-FLAGS|UNIT-COMPILER):")
NAME_ONLY = re.compile(rb"^[ \t]*//+[ \t]*(?:"
                       rb"[(\[]?\?[^ \t]*@@[^ \t]*[)\]]?"                       # a mangled name
                       rb"|\((?:public|protected|private):"                     # an undecorated member
                       rb"|\([^()]*__(?:thiscall|cdecl|stdcall|fastcall)\b"      # an undecorated function
                       rb"|\(compiler-generated; name"
                       rb"|\(no address of its own\b)")
MODULE = rb"(?:EXE|LLE|WON|JPEG|EXEIMG|DX7HR|DX7HRTNL|DEFAULT|TCP|MIXER)"
ADDR = rb"(?:" + MODULE + rb"[ \t]+)?0x[0-9a-fA-F]{4,}"
NOTE = rb"(?:\((?:ret (?:\d+|0x[0-9a-fA-F]+)|\?\?_G 0x[0-9a-fA-F]+|provisional names?|ICF|inline|cdecl)\)|PROVISIONAL name)"
ADDR_COMMENT = re.compile(rb"//[ \t]*(" + ADDR + rb"(?:[ \t]*[,/][ \t]*" + ADDR + rb")*)"
                          rb"(?:[ \t]+" + NOTE + rb"(?:[;,]?[ \t]+" + NOTE + rb")*)?[ \t]*$")
HEX = re.compile(rb"0x([0-9a-fA-F]+)")


def body(line):
    """The line without its ending."""
    return line.rstrip(b"\r\n")


def ending(line):
    return line[len(body(line)):]


def is_blank(line):
    return body(line).strip(b" \t\f\v\r") == b""


def is_address(value):
    """An address of the original exe (VA or RVA) or of the engine DLL, not a constant."""
    return 0x1000 <= value < 0xA00000 or 0x10000000 <= value < 0x10200000


def address_comment(text):
    """The start of a trailing comment that is only an address (with a short note), or None."""
    pos = 0
    while True:
        i = text.find(b"//", pos)
        if i < 0:
            return None
        m = ADDR_COMMENT.match(text, i)
        if m and all(is_address(int(h, 16)) for h in HEX.findall(m.group(1))):
            code = text[:i]
            if code.count(b'"') % 2 == 0 and b"/*" not in code:
                return i
            return None
        if text.count(b'"', 0, i) % 2 == 0:   # an earlier comment that is not only an address
            return None
        pos = i + 2


def paren_depth(text):
    return text.count(b"(") - text.count(b")")


def follower(lines, j):
    """What follows a comment block (index j): 'code' it describes, a 'directive' (#include, #define, #if...: the
    block heads a file or a section) or 'nothing' (a blank line, the end of the file, a closing brace)."""
    while j < len(lines) and body(lines[j]).lstrip().startswith(b"#pragma"):
        j += 1
    if j >= len(lines) or is_blank(lines[j]):
        return "nothing"
    t = body(lines[j]).lstrip()
    if t.startswith(b"#"):
        return "directive"
    return "nothing" if t.startswith(b"}") else "code"


def block_deletions(lines, i, j):
    """The indices of a marker block lines[i:j] to delete."""
    drop = set()
    kinds = [None] * (j - i)   # 'marker', 'tail' (with the marker index), 'free', 'pre'
    owner = [None] * (j - i)
    k = i
    current = None   # the marker whose lines we are in
    depth = 0
    while k < j:
        t = body(lines[k])
        n = k - i
        if MARKER.match(t):
            current = n
            kinds[n] = "marker"
            depth = paren_depth(t[MARKER.match(t).end():])
            open_end = t.rstrip().endswith(b",")
            k += 1
            while ((depth > 0 or open_end) and k < j and not MARKER.match(body(lines[k]))
                   and not SEPARATOR.match(body(lines[k])) and k - i - current <= 3):
                kinds[k - i] = "marker"
                depth += paren_depth(body(lines[k]))
                open_end = body(lines[k]).rstrip().endswith(b",")
                k += 1
            continue
        if current is None:
            kinds[n] = "pre"
        elif SEPARATOR.match(t):
            current = None
            kinds[n] = "free"
        else:
            kinds[n] = "tail"
            owner[n] = current
        if kinds[n] == "pre" and any(x is not None and x != "pre" for x in kinds[:n]):
            kinds[n] = "free"
        k += 1
    follows = follower(lines, j)
    is_attached = follows == "code"
    keep_last = follows != "nothing"   # the last marker's lines describe the code or the file below
    markers = [n for n, x in enumerate(kinds) if x == "marker" and MARKER.match(body(lines[i + n]))]
    last = max(markers)
    for n, x in enumerate(kinds):
        if x == "marker":
            drop.add(i + n)
        elif x == "tail":
            if not (keep_last and owner[n] == last) or NAME_ONLY.match(body(lines[i + n])):
                drop.add(i + n)
    if not is_attached:   # the paragraph introducing the markers
        first = min(markers)
        n = first - 1
        while n >= 0 and kinds[n] == "pre" and not SEPARATOR.match(body(lines[i + n])):
            drop.add(i + n)
            n -= 1
        if n >= 0 and SEPARATOR.match(body(lines[i + n])) and n < first - 1:
            drop.add(i + n)   # its separator, unless the paragraph above was everything
    # separators left doubled or at an edge of the block by the removals
    removed = set(drop)
    kept = [k for k in range(i, j) if k not in drop]
    prev_sep = True
    for idx, k in enumerate(kept):
        if SEPARATOR.match(body(lines[k])):
            if (k - 1 in removed or k + 1 in removed) and (prev_sep or idx == len(kept) - 1):
                drop.add(k)
                continue
            prev_sep = True
        else:
            prev_sep = False
    for k in range(i, j):
        if PROTECTED.match(body(lines[k])):
            drop.discard(k)
    return drop


def strip(data):
    """(new bytes, counts) for one file's bytes."""
    lines = re.split(rb"(?<=\n)", data)
    if lines and lines[-1] == b"":
        lines.pop()
    counts = dict(markers=0, notes=0, trailing=0, address_lines=0, blank=0)
    drop = set()
    k = 0
    while k < len(lines):
        if COMMENT.match(lines[k]):
            j = k
            while j < len(lines) and COMMENT.match(lines[j]):
                j += 1
            if any(MARKER.match(body(lines[x])) for x in range(k, j)):
                d = block_deletions(lines, k, j)
                counts["markers"] += sum(1 for x in d if MARKER.match(body(lines[x])))
                counts["notes"] += sum(1 for x in d if not MARKER.match(body(lines[x])))
                drop |= d
            k = j
        else:
            k += 1
    out = list(lines)
    prev_comment = False   # whether the previous kept line is a comment
    for k, line in enumerate(lines):
        if k in drop:
            continue
        t = body(line)
        i = address_comment(t)
        if i is not None:
            code = t[:i].rstrip(b" \t")
            if code == b"":
                if not prev_comment:
                    drop.add(k)
                    counts["address_lines"] += 1
                    continue
            else:
                out[k] = code + ending(line)
                counts["trailing"] += 1
        prev_comment = COMMENT.match(t) is not None and not is_blank(line)
    # blank runs joined by the removals
    result = []
    k = 0
    while k < len(lines):
        if k in drop or is_blank(lines[k]):
            g = k
            while g < len(lines) and (g in drop or is_blank(lines[g])):
                g += 1
            span = range(k, g)
            blanks = [x for x in span if x not in drop]
            if any(x in drop for x in span):
                first_drop = min(x for x in span if x in drop)
                before = [x for x in blanks if x < first_drop]
                if k == 0 or g == len(lines):
                    want = before if k == 0 else [x for x in blanks if x > max(x for x in span if x in drop)]
                elif before:
                    want = before
                else:
                    runs, run = [], []
                    for x in span:
                        if x in drop:
                            if run:
                                runs.append(run)
                            run = []
                        else:
                            run.append(x)
                    if run:
                        runs.append(run)
                    want = max(runs, key=len) if runs else []
                counts["blank"] += len(blanks) - len(want)
                result.extend(out[x] for x in want)
            else:
                result.extend(out[x] for x in span)
            k = g
        else:
            result.append(out[k])
            k += 1
    return b"".join(result), counts


def files(paths):
    for p in paths:
        p = pathlib.Path(p)
        if not p.is_absolute():
            p = ROOT / p
        if p.is_file():
            yield p
        elif p.is_dir():
            for f in sorted(p.rglob("*")):
                if f.is_file() and f.suffix.lower() in EXTENSIONS:
                    yield f


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("paths", nargs="*", default=DEFAULT_PATHS, help="files or folders (default: src include tests)")
    ap.add_argument("--check", action="store_true", help="change nothing; exit 1 when a file would change")
    a = ap.parse_args()
    total = dict(files=0, markers=0, notes=0, trailing=0, address_lines=0, blank=0)
    lines_before = lines_after = 0
    for f in files(a.paths):
        data = f.read_bytes()
        if b"//" not in data:
            continue
        new, counts = strip(data)
        if new == data:
            continue
        total["files"] += 1
        for key, value in counts.items():
            total[key] += value
        lines_before += data.count(b"\n")
        lines_after += new.count(b"\n")
        if not a.check:
            f.write_bytes(new)
    print("%s %d files: %d marker lines, %d marker notes, %d address-only comment lines, %d trailing address comments, "
          "%d blank lines; %d lines removed"
          % ("would change" if a.check else "changed", total["files"], total["markers"], total["notes"],
             total["address_lines"], total["trailing"], total["blank"], lines_before - lines_after))
    return 1 if a.check and total["files"] else 0


if __name__ == "__main__":
    sys.exit(main())
