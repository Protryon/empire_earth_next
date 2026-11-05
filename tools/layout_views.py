"""Compare class layouts between the header views of two units (one-definition-rule check for the one-image link).

  python -I tools/layout_views.py [--x64] <unit.cpp> <unit.cpp> [...]

Each unit is compiled -fsyntax-only with its own module's flags (its header view) and clang's record-layout dump;
every record name the units share is compared by size. A class whose size differs between views breaks the one
link: objects built by one module's inline code are used by another module's code with other offsets. --x64:
compare the x86_64 layouts (the x64 build).
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import record_sizes
from ee import ROOT


def main():
    args = sys.argv[1:]
    target = "x86_64-pc-windows-msvc" if "--x64" in args else "i686-pc-windows-msvc"
    units = [pathlib.Path(u) if pathlib.Path(u).is_absolute() else ROOT / "src" / u for u in args if u != "--x64"]
    dumps = {u: record_sizes.layouts(u, target) for u in units}
    common = set.intersection(*(set(d) for d in dumps.values()))
    bad = 0
    for name in sorted(common):
        sizes = {u.name: dumps[u][name] for u in units}
        if len(set(sizes.values())) > 1:
            bad += 1
            print(f"{name:40} " + "  ".join(f"{k}={v}" for k, v in sizes.items()))
    print(f"{len(common)} shared records, {bad} differ")


if __name__ == "__main__":
    main()
