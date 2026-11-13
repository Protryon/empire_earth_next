"""TEST ONLY: the scripted scenario editor gate (EE-SIM.exe --editor-script; src/Empire Earth/Empire Earth/EESimEditor.cpp).

EE-SIM opens the scenario editor from the main menu as a person does, runs tools/sim_editor/script.txt (one editor
action per line: a blank and a random map, terrain, elevation and bulldozer paint, unit placement, selection,
rotation, deletion, moves, walls, gates, the trigger area's rectangle, a quick save) and prints a digest of the world
after each group: the map's tiles and mesh, every unit, the selection, the paint area and the game's save chunks, and
the quick saved file's chunks. The gate passes when the output equals tools/sim_editor/expected.txt line for line.
The physics never runs and the UI thread's clock is pinned while a step runs, so the output is the same on every run.

  python -I tools/sim_editor.py              # run and compare: PASS or the first differences
  python -I tools/sim_editor.py --update     # run and write expected.txt (a declared change of the editor's output)
  python -I tools/sim_editor.py --exe build/clang/exe/EE-SIM.exe --data DIR --out FILE
"""
import argparse
import difflib
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
HERE = ROOT / "tools" / "sim_editor"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0], formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__.split("\n", 2)[2])
    ap.add_argument("--update", action="store_true", help="write the output to expected.txt")
    ap.add_argument("--script", type=pathlib.Path, default=HERE / "script.txt")
    ap.add_argument("--expected", type=pathlib.Path, default=HERE / "expected.txt")
    ap.add_argument("--out", type=pathlib.Path, default=ROOT / "build" / "sim_editor" / "out.txt")
    ap.add_argument("--exe", help="EE-SIM.exe (tools/sim.py's default)")
    ap.add_argument("--data", default="open", help="the data tree (tools/sim.py --data)")
    ap.add_argument("--slot", default="sim-editor", help="the stage's name (one per concurrent run)")
    a = ap.parse_args()
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.unlink(missing_ok=True)
    cmd = [sys.executable, "-I", str(ROOT / "tools" / "sim.py"), "--data", a.data, "--slot", a.slot]
    if a.exe:
        cmd += ["--exe", a.exe]
    cmd += ["--", "--editor-script", str(a.script.resolve()), "--editor-out", str(a.out.resolve()),
            "--report", str(a.out.with_suffix(".json").resolve()), "--wall-timeout", "600"]
    code = subprocess.run(cmd, timeout=900).returncode
    if code != 0 or not a.out.exists():
        sys.exit(f"sim_editor: EE-SIM exited {code} (report {a.out.with_suffix('.json')})")
    got = a.out.read_text(encoding="utf-8").splitlines()
    if a.update:
        a.expected.write_text("\n".join(got) + "\n", encoding="utf-8", newline="\n")
        print(f"sim_editor: wrote {a.expected} ({len(got)} lines)")
        return
    want = a.expected.read_text(encoding="utf-8").splitlines()
    if got == want:
        print(f"sim_editor: PASS ({len(got)} lines)")
        return
    for line in list(difflib.unified_diff(want, got, "expected", "got", lineterm="", n=1))[:60]:
        print(line)
    sys.exit("sim_editor: FAIL")


if __name__ == "__main__":
    main()
