"""TEST ONLY: the SGVW trace (object storage plan, lane S; rust/crates/ee_game/src/screens/sgvw.rs): every read the
screens and the scenario editor make of the game's objects, recorded where the Rust asks (EE_SGVW=<file>).

  python -I tools/sgvw.py editor OUT [--data DIR]     # the editor gate (tools/sim_editor.py) with the trace on
  python -I tools/sgvw.py compare GOLDEN OUT          # line for line (the editor gate: EE-SIM, a pinned clock)
  python -I tools/sgvw.py compare GOLDEN OUT --set    # the distinct lines, repeat counts dropped (a live game: the
                                                      # frame count varies between runs, the values must not)
  python -I tools/sgvw.py distinct OUT                # the distinct lines in first-seen order (a --set golden)
  python -I tools/sgvw.py summary OUT                 # reads by kind and op, with the world lock's state

The goldens, recorded on the C++ before the move: tools/sgvw/editor.txt (the editor gate, exact),
tools/sgvw/tour.txt (the fixture tour: --exercise tour --play-seconds 60 --input-check) and tools/sgvw/tour_view.txt
(the same with --view-check --seed 7: the read view's in-game reads, the speed, camera, diplomacy, options and chat;
the seed fixes the players' civilizations). A tour is run through tools/harness/slot.py with EE_SGVW set.
"""
import argparse
import collections
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


# reads whose answer is the game's clock or the build, not a stored value: their answers are masked ("*") in a
# comparison (EE_SCREENS_GAME_CTS_PROCESSED: gCommunications->GetNumberOfCTsProcessedInPhysics(); the version
# text, EE_SCREENS_TEXT_VERSION, which carries the build's commit count)
TIMED = {"V 377 ", "T 1 "}


def mask(l):
    if any(l.startswith(t) for t in TIMED):
        head, _, rest = l.partition(" -> ")
        return head + " -> * |" + rest.partition(" |")[2]
    return l


def lines(path, distinct):
    out = [mask(l) for l in pathlib.Path(path).read_text(encoding="utf-8").splitlines() if l]
    if not distinct:
        return out
    seen, d = set(), []
    for l in out:
        if not l.startswith("R ") and l not in seen:
            seen.add(l)
            d.append(l)
    return d


def compare(a):
    want, got = lines(a.golden, a.set), lines(a.out, a.set)
    if a.set:
        missing = [l for l in want if l not in set(got)]
        extra = [l for l in got if l not in set(want)]
        for tag, ls in (("missing", missing), ("extra", extra)):
            for l in ls[:20]:
                print(f"{tag}: {l}")
        ok = not missing and not extra
    else:
        diff = next((i for i, (x, y) in enumerate(zip(want, got)) if x != y), None)
        ok = diff is None and len(want) == len(got)
        if not ok:
            i = diff if diff is not None else min(len(want), len(got))
            print(f"first difference at line {i + 1}:")
            print(f"  golden: {want[i] if i < len(want) else '<end>'}")
            print(f"  out:    {got[i] if i < len(got) else '<end>'}")
    print(f"SGVW {'PASS' if ok else 'FAIL'} ({len(got)} lines{' distinct' if a.set else ''}, golden {len(want)})")
    sys.exit(0 if ok else 1)


def summary(a):
    c = collections.Counter()
    for l in lines(a.out, False):
        if l.startswith("R "):
            continue
        body, _, tail = l.rpartition(" |")
        parts = body.split()
        key = parts[0] + (" " + parts[1] if parts[0] in ("V", "T", "C", "E") else "")
        c[(key, tail)] += 1
    for (k, t), n in sorted(c.items()):
        print(f"{k:12} {t:10} {n}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0], formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__.split("\n", 2)[2])
    sub = ap.add_subparsers(dest="cmd", required=True)
    e = sub.add_parser("editor")
    e.add_argument("out", type=pathlib.Path)
    e.add_argument("--data", default="open")
    e.add_argument("--exe")
    c = sub.add_parser("compare")
    c.add_argument("golden")
    c.add_argument("out")
    c.add_argument("--set", action="store_true")
    d = sub.add_parser("distinct")
    d.add_argument("out")
    s = sub.add_parser("summary")
    s.add_argument("out")
    a = ap.parse_args()
    if a.cmd == "editor":
        env = dict(os.environ, EE_SGVW=str(a.out.resolve()))
        cmd = [sys.executable, "-I", str(ROOT / "tools" / "sim_editor.py"), "--data", a.data]
        if a.exe:
            cmd += ["--exe", a.exe]
        sys.exit(subprocess.run(cmd, env=env).returncode)
    if a.cmd == "compare":
        compare(a)
    if a.cmd == "distinct":
        sys.stdout.write("".join(l + "\n" for l in lines(a.out, True)))
    if a.cmd == "summary":
        summary(a)


if __name__ == "__main__":
    main()
