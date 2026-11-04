"""Hoist VC6 for-scope leaks out of their for statements (port phase 1 dialect fix).

  python -I tools/clang_forscope.py [--apply] [log dir]     (default log dir: build/clang/log)

VC6 scopes a variable declared in a for-init statement to the enclosing block, so code uses it after the loop. Clang
reports those uses as "use of undeclared identifier 'X'". For every such error in the clang logs, this finds the
nearest preceding `for (T X = ...` or `for (T X; ...` in the same file that declares X and rewrites it as

    T X;
    for (X = ...

on the for's own line, so no line number moves (asserts record __LINE__). VC6 gives the variable the same scope
either way. Declarations it cannot rewrite safely (several declarators, a for that is not alone on its line) are
listed, not changed. Without --apply it only lists what it would change.
"""
import argparse
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from ee import ORIG_SRC_ROOT, ROOT

ERR = re.compile(r"^(.*?)\((\d+),\d+\)\s*:\s*error: use of undeclared identifier '(\w+)'", re.M)


def for_decl(var):
    # for ( <type tokens> var = | ; )   type tokens: identifiers, ::, *, &, <...> without parentheses
    return re.compile(r"^(?P<ind>[ \t]*)(?P<pre>(?:\}\s*)?)for\s*\(\s*(?P<type>(?:(?:const|volatile|unsigned|signed|short|long|struct|class)\s+)*[\w:]+(?:\s*<[^;()]*?>(?:::\w+)*)?"
                      r"(?:\s*[*&])*)\s+(?P<star>[*&]*)" + re.escape(var) + r"\s*(?P<rest>[=;].*)$")


def source_of(staged):
    p = pathlib.Path(staged)
    try:
        rel = p.relative_to(ORIG_SRC_ROOT)
    except ValueError:
        return p if p.is_relative_to(ROOT) else None
    return ROOT / "src" / rel


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs", nargs="?", default=str(ROOT / "build" / "clang" / "log"))
    ap.add_argument("--apply", action="store_true")
    a = ap.parse_args()
    want = {}
    for log in pathlib.Path(a.logs).rglob("*.log"):
        for f, line, var in ERR.findall(log.read_text(errors="replace")):
            src = source_of(f)
            if src and src.exists():
                want.setdefault(src, set()).add((int(line), var))
    changed = skipped = 0
    for src, errs in sorted(want.items()):
        raw = src.read_bytes()
        crlf = b"\r\n" in raw
        lines = raw.decode("latin-1").replace("\r\n", "\n").split("\n")
        edits = {}
        for line, var in sorted(errs):
            rx = for_decl(var)
            for i in range(line - 2, -1, -1):
                m = rx.match(lines[i])
                if m:
                    break
            else:
                print(f"  no for-decl: {src.relative_to(ROOT)}:{line} {var}")
                skipped += 1
                continue
            rest = m.group("rest")
            init = rest.split(";", 1)[0]
            if re.search(r",\s*[*&]*\w+\s*(=|;|$)", init) and "(" not in init:
                print(f"  several declarators: {src.relative_to(ROOT)}:{i + 1}: {lines[i].strip()}")
                skipped += 1
                continue
            edits[i] = (m, var)
        for i in sorted(edits, reverse=True):
            m, var = edits[i]
            ind, pre = m.group("ind"), m.group("pre")
            decl = f"{ind}{m.group('type')} {m.group('star')}{var};"
            loop = f"{ind}{pre}for ({var} {m.group('rest')}" if m.group("rest").startswith("=") else \
                f"{ind}{pre}for ({m.group('rest')}"
            if pre:
                print(f"  for after a brace: {src.relative_to(ROOT)}:{i + 1}: {lines[i].strip()}")
                skipped += 1
                continue
            if a.apply:
                lines[i] = decl + " " + loop.lstrip()  # one line: __LINE__ (asserts) stays the same below
            else:
                print(f"{src.relative_to(ROOT)}:{i + 1}: {lines[i].strip()}  ->  {decl.strip()} {loop.strip()}")
            changed += 1
        if a.apply and edits:
            text = "\n".join(lines)
            src.write_bytes((text.replace("\n", "\r\n") if crlf else text).encode("latin-1"))
    print(f"{changed} for statements {'rewritten' if a.apply else 'to rewrite'}, {skipped} skipped")


if __name__ == "__main__":
    main()
