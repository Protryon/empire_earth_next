"""Prepare a directory to play the build: the exe and the unified open game data (tools/open_data.py).

  python -I tools/make_play_dir.py OUT [--exe PATH | --posix-exe PATH] [--data TREE|BUNDLE.zip|DIR ...] [--force]

OUT gets:
  - EE-AOC.exe (default: build/clang/exe/EE-AOC.exe; --exe for another);
  - the game data: Data (one loose tree: db tables as text, models as glTF, textures as PNG, the rest as shipped),
    Users, WONLobby.cfg and zzWONVersion.cfg, from --data: a tree made by `tools/open_data.py build` or the bundle
    made by `tools/open_data.py pack` (default: build/open_data/tree, built from the two installs when missing);
    Data/db and Data/ai always from this checkout's data/db and data/ai.
    --data repeats, each copied over the ones before: the media, 4x textures and db parts of the releases, or the
    folder tools/fetch_data.py unzipped them into. No
    data.ssa and nothing of the original Empire Earth's install is needed or referenced;
  - no movies: the game has no movie player (2026-11-17) and skips every movie (the intro, a scenario's).
Run OUT\\EE-AOC.exe. The game keeps its settings in OUT\\config.yaml (it creates the file). OUT must be outside the
repository or under its build/ folder (git-ignored). --force replaces an existing OUT made by this script.

Linux and macOS (--posix-exe PATH: the game built by tools/xbuild.py game, e.g.
build/xbuild/linux-x86_64/game/ee-aoc): OUT gets that program as ee-aoc instead of EE-AOC.exe and OUT/run.sh, which
runs it with EE_DATA_ROOT=OUT (the program's own directory is the default data root anyway). Prepare OUT anywhere and
copy it to the other system.
"""
import argparse
import json
import pathlib
import shutil
import subprocess
import sys
import time
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
TREE = ROOT / "build" / "open_data" / "tree"


def check_out(out):
    out = out.resolve()
    if out.is_relative_to(ROOT) and not out.is_relative_to(ROOT / "build"):
        sys.exit(f"{out} is inside the repository: use a folder outside it or under build/")
    install = pathlib.Path(r"C:\Program Files (x86)\Empire Earth")
    if out == install or out.is_relative_to(install):
        sys.exit(f"{out} is inside {install}: the installs are never written")
    return out


def copy_data(data, out):
    """the tree's or the bundle's game files into OUT -> what they were built from (open_data.json)"""
    if data.suffix.lower() == ".zip":
        with zipfile.ZipFile(data) as z:
            z.extractall(out)
    else:
        for item in data.iterdir():
            if item.is_dir():
                shutil.copytree(item, out / item.name, dirs_exist_ok=True)
            else:
                shutil.copy2(item, out / item.name)
    made = out / "open_data.json"
    info = json.loads(made.read_text(encoding="utf-8")) if made.exists() else {}
    if info.get("binary"):
        sys.exit("data: a --binary tree: the game reads only the text db tables now (python -I tools/open_data.py build)")
    return info


def drop_binary_forms(out):
    """The binary forms in OUT/Data (an older release's db .dat files, a .cem that is no model) removed -> count"""
    sys.path.insert(0, str(ROOT / "tools"))
    import open_data   # noqa: E402
    data = out / "Data"
    gone = [p for p in data.rglob("*") if p.is_file()
            and open_data.binary_form("\\".join(p.relative_to(data).parts).lower())]
    for p in gone:
        p.unlink()
    if gone:
        print(f"data: {len(gone)} binary forms left out (the game reads only the open forms): "
              f"{', '.join(p.relative_to(data).as_posix() for p in gone[:5])}{' ...' if len(gone) > 5 else ''}")
    return len(gone)


def posix_files(exe, out):
    """The POSIX game and a launcher (the game's directory is its data root; its settings go to config.yaml there)"""
    shutil.copy2(exe, out / "ee-aoc")
    (out / "run.sh").write_bytes(b'#!/bin/sh\n# Empire Earth: The Art of Conquest (tools/make_play_dir.py --posix-exe)\n'
                                 b'here="$(cd "$(dirname "$0")" && pwd)"\n'
                                 b'EE_DATA_ROOT="$here" exec "$here/ee-aoc" "$@"\n')
    for p in (out / "ee-aoc", out / "run.sh"):
        p.chmod(0o755)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("out", type=pathlib.Path)
    ap.add_argument("--exe", type=pathlib.Path)
    ap.add_argument("--data", type=pathlib.Path, action="append",
                    help="the unified data: a tools/open_data.py tree or bundle (default build/open_data/tree, "
                         "else build/data-root when tools/fetch_data.py filled data/); "
                         "repeatable, each over the ones before")
    ap.add_argument("--force", action="store_true", help="replace an existing OUT")
    ap.add_argument("--posix-exe", type=pathlib.Path,
                    help="the Linux/macOS game (tools/xbuild.py game) in place of EE-AOC.exe")
    a = ap.parse_args()
    exe = a.posix_exe or a.exe or ROOT / "build" / "clang" / "exe" / "EE-AOC.exe"
    if not exe.exists():
        sys.exit(f"{exe} does not exist: build it first (python -I build.py)")
    sys.path.insert(0, str(ROOT / "tools"))
    import open_data   # noqa: E402
    fetched = open_data.fetched_root()
    datas = a.data or [TREE if (TREE / "open_data.json").exists() or fetched is None else fetched]
    if a.data is None and datas == [TREE] and not (TREE / "open_data.json").exists():
        print(f"data: building {TREE} from the installs (tools/open_data.py build)", flush=True)
        r = subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "open_data.py"), "build", str(TREE), "--force"])
        if r.returncode:
            sys.exit("tools/open_data.py build failed")
    for data in datas:
        if not data.exists():
            sys.exit(f"{data} does not exist: make it with tools/open_data.py build or pack")
    out = check_out(a.out)
    if out.exists():
        if not a.force:
            sys.exit(f"{out} exists: pass --force to replace it")
        if not (out / "play.json").exists():
            sys.exit(f"{out} exists and was not made by this script (no play.json): refusing to delete it")
        shutil.rmtree(out)
    out.mkdir(parents=True)
    t0 = time.time()
    info = {}
    for data in datas:
        print(f"game data from {data}", flush=True)
        info = copy_data(data, out) or info
    drop_binary_forms(out)
    # the exe's own tables and scripts: this checkout's, whatever the data brought (a release lags)
    open_data.refresh_scripts(out)
    open_data.refresh_db(out)
    print("Data/db and Data/ai from this checkout's data/db and data/ai", flush=True)
    if a.posix_exe:
        posix_files(exe, out)
    else:
        shutil.copy2(exe, out / "EE-AOC.exe")
    (out / "play.json").write_bytes((json.dumps(dict(
        exe=str(exe), data=[str(d) for d in datas],
        made=time.strftime("%Y-%m-%d %H:%M:%S")), indent=2) + "\n").encode())
    print(f"ready in {time.time() - t0:.0f} s: run {out / ('run.sh' if a.posix_exe else 'EE-AOC.exe')}")


if __name__ == "__main__":
    main()
