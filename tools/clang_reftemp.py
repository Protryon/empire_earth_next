"""Name the temporaries VC6 bound to non-const references (port phase 1 dialect fix).

  python -I tools/clang_reftemp.py [--apply] [log dir]     (default log dir: build/clang/log)

VC6 binds a temporary to a non-const reference parameter (f(UWideString(x)) with f(UWideString&)). The parameter
types are fixed by the original's mangled names, so the fix is at the call: the temporary becomes a named local,
declared just before the statement:

    UWideString theTemp1(x);
    f(theTemp1);

Handled: clang errors "non-const lvalue reference ... cannot bind to a temporary" (the caret is on the argument),
and "no matching constructor/member function" whose notes say "expects an lvalue for Nth argument" (the argument is
found by position). Only an argument of the form `Type(args)` is rewritten, and only in a statement that starts its
own line after a line ending in ; { } or a label, so hoisting the construction cannot change when it runs. Not in an
`else if`, `while`, `for` or `do` header, and not in a braceless if/else body. Everything else is listed for a manual
fix. The local lives to the end of the enclosing block, not just to the end of the statement as the temporary did.
"""
import argparse
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from ee import ROOT
from clang_forscope import source_of

ERR = re.compile(r"^(.*?)\((\d+),(\d+)\)\s*:\s*error: (.*)$")
NOTE_LVALUE = re.compile(r"note: candidate (?:function|constructor)[^:]*not viable: expects an lvalue for (\d+)\w\w argument")
ORD = {"1": 1, "2": 2, "3": 3, "4": 4, "5": 5, "6": 6, "7": 7, "8": 8, "9": 9}
TEMP = re.compile(r"((?:[A-Za-z_]\w*::)*[A-Za-z_]\w*(?:<[^()<>]*>)?)\s*\(")


def match_paren(text, i):
    """text[i] == '(' -> index of the matching ')'."""
    depth, j = 0, i
    while j < len(text):
        c = text[j]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return j
        elif c in "\"'":
            j += 1
            while j < len(text) and text[j] != c:
                j += 2 if text[j] == "\\" else 1
        j += 1
    return -1


def split_args(text, lo, hi):
    """Top-level comma split of text[lo:hi] -> [(start, end)]."""
    out, depth, s, j = [], 0, lo, lo
    while j < hi:
        c = text[j]
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c in "\"'":
            j += 1
            while j < hi and text[j] != c:
                j += 2 if text[j] == "\\" else 1
        elif c == "," and depth == 0:
            out.append((s, j))
            s = j + 1
        j += 1
    out.append((s, hi))
    return out


def parse_logs(logdir):
    """-> {source: [(line, col, kind, argno)]}"""
    want = {}
    for log in pathlib.Path(logdir).rglob("*.log"):
        lines = log.read_text(errors="replace").splitlines()
        for k, l in enumerate(lines):
            m = ERR.match(l)
            if not m:
                continue
            f, line, col, msg = m.group(1), int(m.group(2)), int(m.group(3)), m.group(4)
            src = source_of(f)
            if not src or not src.exists():
                continue
            if "cannot bind to a temporary" in msg:
                want.setdefault(src, set()).add((line, col, "arg", 0))
            elif msg.startswith("no matching constructor") or msg.startswith("no matching member function"):
                for l2 in lines[k + 1:k + 40]:
                    if ERR.match(l2):
                        break
                    n = NOTE_LVALUE.search(l2)
                    if n:
                        want.setdefault(src, set()).add((line, col, "call", int(n.group(1))))
                        break
    return want


def stmt_start(lines, i):
    """Line index where the statement containing line i starts, or None if hoisting before it is not safe."""
    j = i
    while j > 0:
        prev = lines[j - 1].split("//")[0].rstrip()
        if prev == "" and j - 1 > 0:
            j -= 1
            continue
        if prev.endswith((";", "{", "}")) or re.match(r"^\s*(case\b.*|default\s*):$", prev):
            break
        if prev.endswith(":") and not prev.endswith("::"):
            break
        j -= 1
    head = lines[j].strip()
    if j == 0 or re.match(r"(else\b|while\b|for\b|do\b|\}\s*else\b|\}\s*while\b|case\b|default\b|[:,])", head):
        return None
    # a constructor's initializer list or a function header between the statement start and the site
    if any(re.match(r"\s*:\s*\w", l) or re.match(r"[A-Za-z_]", l) for l in lines[j:i + 1]):
        return None
    # a braceless if/else body (the statement before ends with ')' or 'else'), or right after a case label (a
    # declaration there would be jumped over by the other labels)
    k = j - 1
    while k >= 0 and lines[k].split("//")[0].strip() == "":
        k -= 1
    if k >= 0 and re.search(r"(\)|\belse|^\s*(case\b.*|default\s*):)\s*$", lines[k].split("//")[0]):
        return None
    return j


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs", nargs="?", default=str(ROOT / "build" / "clang" / "log"))
    ap.add_argument("--apply", action="store_true")
    a = ap.parse_args()
    total = manual = 0
    for src, sites in sorted(parse_logs(a.logs).items()):
        raw = src.read_bytes()
        crlf = b"\r\n" in raw
        text = raw.decode("latin-1").replace("\r\n", "\n")
        lines = text.split("\n")
        offs = [0]
        for l in lines:
            offs.append(offs[-1] + len(l) + 1)
        n_tmp = len(re.findall(r"\btheTemp\d+\b", text))
        repl = []   # (start, end, newtext, insert_line, decl)
        seen = set()
        rel = src.relative_to(ROOT)
        for line, col, kind, argno in sorted(sites):
            pos = offs[line - 1] + col - 1
            if kind == "call":
                p = text.find("(", pos)
                q = match_paren(text, p)
                args = split_args(text, p + 1, q)
                if argno > len(args):
                    print(f"  manual: {rel}:{line}: argument {argno} not found")
                    manual += 1
                    continue
                s, e = args[argno - 1]
                while text[s] in " \t\n":
                    s += 1
                pos = s
            m = TEMP.match(text, pos)
            if not m or (pos, kind) in seen:
                if not m:
                    print(f"  manual: {rel}:{line}: {text[pos:pos + 60].splitlines()[0]}")
                    manual += 1
                continue
            seen.add((pos, kind))
            p = m.end() - 1
            q = match_paren(text, p)
            typ, inner = m.group(1), text[p + 1:q]
            ln = text.count("\n", 0, pos)
            st = stmt_start(lines, ln)
            if st is None or typ in ("static_cast", "reinterpret_cast", "const_cast"):
                print(f"  manual: {rel}:{line}: {lines[ln].strip()[:110]}")
                manual += 1
                continue
            n_tmp += 1
            name = f"theTemp{n_tmp}"
            ind = re.match(r"[ \t]*", lines[st]).group(0)
            decl = f"{ind}{typ} {name}({inner});" if inner.strip() else f"{ind}{typ} {name};"
            repl.append((pos, q + 1, name, st, decl))
            total += 1
            if not a.apply:
                print(f"{rel}:{line}: {text[pos:q + 1][:80]}  ->  {decl.strip()[:80]}")
        if a.apply and repl:
            for s, e, name, _, _ in sorted(repl, reverse=True):
                text = text[:s] + name + text[e:]
            lines = text.split("\n")
            for _, _, _, st, decl in sorted(repl, key=lambda r: (r[3], r[0]), reverse=True):
                lines.insert(st, decl)
            text = "\n".join(lines)
            src.write_bytes((text.replace("\n", "\r\n") if crlf else text).encode("latin-1"))
    print(f"{total} temporaries {'named' if a.apply else 'to name'}, {manual} for a manual fix")


if __name__ == "__main__":
    main()
