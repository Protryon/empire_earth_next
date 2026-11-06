"""Prepare a directory to play the build: the exe, the unified open game data (tools/open_data.py) and the movies
converted for the build's player.

  python -I tools/make_play_dir.py OUT [--exe PATH | --posix-exe PATH] [--data TREE|BUNDLE.zip|DIR ...] [--movies DIR ...]
                                       [--force]

OUT gets:
  - EE-AOC.exe (default: build/clang/exe/EE-AOC.exe; --exe for another);
  - the game data: Data (one loose tree: db tables as text, models as glTF, textures as PNG, the rest as shipped),
    Users, WONLobby.cfg and zzWONVersion.cfg, from --data: a tree made by `tools/open_data.py build` or the bundle
    made by `tools/open_data.py pack` (default: build/open_data/tree, built from the two installs when missing).
    --data repeats, each copied over the ones before: the media, 4x textures and db parts of the releases, or the
    folder tools/fetch_data.py unzipped them into. No
    data.ssa and nothing of the original Empire Earth's install is needed or referenced;
  - Data/Movies: every Bink movie (.bik) found in the --movies directories (an extracted retail disc, an installation
    that has Data\\Movies) converted to MPEG-1 (.mpg, tools/convert_movies.py, needs ffmpeg). Encodes are cached in
    build/play_cache/movies (keyed by file name and size), so a second play directory costs a copy. The build plays
    X.mpg when the game asks for X.bik.
Run OUT\\EE-AOC.exe. The game keeps its settings in OUT\\config.ini (it creates the file). OUT must be outside the
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
CACHE = ROOT / "build" / "play_cache" / "movies"


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
        print("data: a --binary tree (db tables, models and textures in their binary forms)")
    return info


def movies(out, sources):
    """Convert every .bik in the sources into OUT/Data/Movies/<name>.mpg (through the cache)."""
    found = {}
    for d in sources:
        for p in pathlib.Path(d).rglob("*"):
            if p.suffix.lower() == ".bik" and p.is_file():
                found.setdefault(p.name.lower(), p)
    if not found:
        print("movies: none (pass --movies with an extracted retail disc or an installation that has "
              "Data\\Movies): the game skips its movies")
        return 0
    CACHE.mkdir(parents=True, exist_ok=True)
    todo = []
    for key, src in sorted(found.items()):
        cached = CACHE / f"{src.stem}.{src.stat().st_size}.mpg"
        if not cached.exists():
            todo.append((src, cached))
    if todo:
        work = CACHE / "work"
        if work.exists():
            shutil.rmtree(work)
        work.mkdir()
        for src, _ in todo:
            shutil.copy2(src, work / src.name)
        print(f"movies: encoding {len(todo)} of {len(found)} (the rest are cached)", flush=True)
        r = subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "convert_movies.py"), str(work)])
        if r.returncode:
            sys.exit("movies: tools/convert_movies.py failed")
        for src, cached in todo:
            (work / f"{src.stem}.mpg").replace(cached)
        shutil.rmtree(work)
    dest = out / "Data" / "Movies"
    dest.mkdir(parents=True, exist_ok=True)
    for key, src in sorted(found.items()):
        shutil.copy2(CACHE / f"{src.stem}.{src.stat().st_size}.mpg", dest / f"{src.stem}.mpg")
    print(f"movies: {len(found)} converted into {dest}")
    return len(found)


def posix_files(exe, out):
    """The POSIX game and a launcher (the game's directory is its data root; its settings go to config.ini there)"""
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
                    help="the unified data: a tools/open_data.py tree or bundle (default build/open_data/tree); "
                         "repeatable, each over the ones before")
    ap.add_argument("--movies", type=pathlib.Path, action="append", default=[],
                    help="a directory with .bik movies (repeatable)")
    ap.add_argument("--force", action="store_true", help="replace an existing OUT")
    ap.add_argument("--posix-exe", type=pathlib.Path,
                    help="the Linux/macOS game (tools/xbuild.py game) in place of EE-AOC.exe")
    a = ap.parse_args()
    exe = a.posix_exe or a.exe or ROOT / "build" / "clang" / "exe" / "EE-AOC.exe"
    if not exe.exists():
        sys.exit(f"{exe} does not exist: build it first (python -I build.py)")
    datas = a.data or [TREE]
    if a.data is None and not (TREE / "open_data.json").exists():
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
    if a.posix_exe:
        posix_files(exe, out)
    else:
        shutil.copy2(exe, out / "EE-AOC.exe")
    count = movies(out, a.movies)
    (out / "play.json").write_bytes((json.dumps(dict(
        exe=str(exe), data=[str(d) for d in datas], data_binary=bool(info.get("binary")), movies=count,
        made=time.strftime("%Y-%m-%d %H:%M:%S")), indent=2) + "\n").encode())
    print(f"ready in {time.time() - t0:.0f} s: run {out / ('run.sh' if a.posix_exe else 'EE-AOC.exe')}")


if __name__ == "__main__":
    main()
