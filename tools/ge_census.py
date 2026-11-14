"""The Graphics Engine's C++ census (R4.3's whole-class stage): per unit of src/Low-Level Engine/Graphics Engine, the
code lines, the functions, and the function bodies that are logic rather than one-line forwarders.

  python -I tools/ge_census.py [ROOT] [-v]

A body counts as a forwarder when it has at most two statements and calls Rust (`ee_ge...`, `Rust`) or has at most
one statement (a getter, a setter, a base call). Comments, string and character literals are stripped first; the
lines counted are the non-blank lines inside the body. `-v` lists each unit's logic bodies, longest first. It reads
text only (no compiler): a body the brace scan misreads (a macro opening a scope) is counted under the function
before it, so the numbers are for ranking, not exact.
"""
import pathlib
import re
import sys

STR = re.compile(r'"(?:\\.|[^"\\\n])*"')


def strip(s):
    s = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), s, flags=re.S)
    s = re.sub(r'//[^\n]*', '', s)
    s = STR.sub('""', s)
    return re.sub(r"'(?:\\.|[^'\\\n])'", "' '", s)


def census(path):
    """(code lines, [(function, body lines, is forwarder)]) of one unit."""
    t = strip(path.read_text(encoding='utf-8', errors='replace'))
    total = sum(1 for line in t.splitlines() if line.strip())
    depth, start, funcs = 0, None, []
    for m in re.finditer(r'[{}]', t):
        if m.group(0) == '{':
            if depth == 0:
                pre = t[max(0, m.start() - 300):m.start()].rstrip()
                isfn = bool(re.search(r'(\)|const|noexcept)\s*(:[^;{}]*)?$', pre, re.S))
                start = (m.start(), isfn, pre)
            depth += 1
            continue
        depth -= 1
        if depth == 0 and start:
            s, isfn, pre = start
            if isfn:
                body = t[s + 1:m.start()]
                lines = [line for line in body.splitlines() if line.strip()]
                hdr = pre.split(';')[-1].split('}')[-1]
                name = re.findall(r'([~\w:]+(?:operator\s*\S+)?)\s*\(', hdr)
                stmts = body.count(';')
                fwd = stmts <= 2 and ('ee_ge' in body or 'Rust' in body or stmts <= 1)
                funcs.append((name[0] if name else '?', len(lines), fwd))
            start = None
    return total, funcs


def main():
    args = [a for a in sys.argv[1:] if a != '-v']
    root = pathlib.Path(args[0] if args else '.')
    rows = []
    for f in sorted((root / 'src/Low-Level Engine/Graphics Engine').glob('*.cpp')):
        total, funcs = census(f)
        logic = [x for x in funcs if not x[2]]
        rows.append((f.name, total, funcs, logic))
    rows.sort(key=lambda r: -sum(n for _, n, _ in r[3]))
    print(f"{'unit':34} {'code':>5} {'fns':>4} {'logic fns':>9} {'logic lines':>11}")
    for name, total, funcs, logic in rows:
        print(f"{name:34} {total:5} {len(funcs):4} {len(logic):9} {sum(n for _, n, _ in logic):11}")
    print(f"{'total':34} {sum(r[1] for r in rows):5} {sum(len(r[2]) for r in rows):4} "
          f"{sum(len(r[3]) for r in rows):9} {sum(n for r in rows for _, n, _ in r[3]):11}")
    if '-v' in sys.argv:
        for name, _, _, logic in rows:
            if logic:
                print('==', name)
                for fn, n, _ in sorted(logic, key=lambda x: -x[1]):
                    print(f'   {n:4} {fn}')


if __name__ == '__main__':
    main()
