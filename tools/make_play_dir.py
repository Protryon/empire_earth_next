"""Prepare a directory to play the build: the exe, a copy of the game data, the db tables as text and the movies
converted for the build's player.

  python -I tools/make_play_dir.py OUT [--exe PATH] [--install DIR] [--movies DIR ...] [--binary-db]
                                       [--force]

OUT gets:
  - EE-AOC.exe (default: build/clang/exe/EE-AOC.exe; --exe for another);
  - a copy of the installed game's files the build reads (Data, Users, redist, WONLobby.cfg, zzWONVersion.cfg) from
    --install (default: the Art of Conquest install); the install is only read;
  - Data/db: every db table as clean text (tools/eedata install), which the game prefers over data.ssa
    (--binary-db: leave the tables in data.ssa);
  - Data/Movies: every Bink movie (.bik) found in OUT or in the --movies directories (an extracted retail disc, an
    installation that has Data\\Movies) converted to MPEG-1 (.mpg, tools/convert_movies.py, needs ffmpeg). Encodes
    are cached in build/play_cache/movies (keyed by file name and size), so a second play directory costs a copy.
    The .bik files themselves are not copied: the build plays X.mpg when the game asks for X.bik.
Run OUT\\EE-AOC.exe. OUT must be outside the repository or under its build/ folder (git-ignored), and never the
install itself. --force replaces an existing OUT.
"""
import argparse
import json
import pathlib
import shutil
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
INSTALL = pathlib.Path(r"C:\Program Files (x86)\Empire Earth\Empire Earth - The Art of Conquest")
GAME_FILES = ["Data", "Users", "redist", "WONLobby.cfg", "zzWONVersion.cfg"]   # as tools/harness/exe_boot.py stages
CACHE = ROOT / "build" / "play_cache" / "movies"


def check_out(out, install):
    out = out.resolve()
    if out.is_relative_to(ROOT) and not out.is_relative_to(ROOT / "build"):
        sys.exit(f"{out} is inside the repository: use a folder outside it or under build/")
    if out == install.resolve() or install.resolve() in out.parents or out in install.resolve().parents:
        sys.exit(f"{out} overlaps the install {install}: the install is never written")
    return out


def copy_game(install, out):
    for name in GAME_FILES:
        src = install / name
        if src.is_dir():
            shutil.copytree(src, out / name)
        elif src.exists():
            shutil.copy2(src, out / name)
        else:
            print(f"  (not in the install: {name})")


def movies(out, sources):
    """Convert every .bik in OUT and the sources into OUT/Data/Movies/<name>.mpg (through the cache)."""
    found = {}
    for d in [out, *sources]:
        for p in pathlib.Path(d).rglob("*"):
            if p.suffix.lower() == ".bik" and p.is_file():
                found.setdefault(p.name.lower(), p)
    if not found:
        print("movies: no .bik files found (the install has none; pass --movies with an extracted retail disc or "
              "an installation that has Data\\Movies): the game skips its movies")
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
    for p in out.rglob("*"):   # the build plays the .mpg; a .bik in the copy is dead weight
        if p.suffix.lower() == ".bik":
            p.unlink()
    print(f"movies: {len(found)} converted into {dest}")
    return len(found)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("out", type=pathlib.Path)
    ap.add_argument("--exe", type=pathlib.Path)
    ap.add_argument("--x64", action="store_true", help=argparse.SUPPRESS)   # x86_64 is the only target
    ap.add_argument("--install", type=pathlib.Path, default=INSTALL)
    ap.add_argument("--movies", type=pathlib.Path, action="append", default=[],
                    help="a directory with .bik movies (repeatable)")
    ap.add_argument("--binary-db", action="store_true", help="keep the db tables in data.ssa (no text tables)")
    ap.add_argument("--force", action="store_true", help="replace an existing OUT")
    a = ap.parse_args()
    exe = a.exe or ROOT / "build" / "clang" / "exe" / "EE-AOC.exe"
    if not exe.exists():
        sys.exit(f"{exe} does not exist: build it first (python -I build.py)")
    if not (a.install / "Data").is_dir():
        sys.exit(f"{a.install} has no Data folder: pass --install")
    out = check_out(a.out, a.install)
    if out.exists():
        if not a.force:
            sys.exit(f"{out} exists: pass --force to replace it")
        if not (out / "play.json").exists():
            sys.exit(f"{out} exists and was not made by this script (no play.json): refusing to delete it")
        shutil.rmtree(out)
    out.mkdir(parents=True)
    t0 = time.time()
    print(f"game files from {a.install}", flush=True)
    copy_game(a.install, out)
    shutil.copy2(exe, out / "EE-AOC.exe")
    tables = 0
    if not a.binary_db:
        r = subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "eedata" / "eedata.py"), "install", str(out)],
                           capture_output=True, text=True)
        if r.returncode:
            sys.exit(f"eedata install failed:\n{r.stdout}{r.stderr}")
        tables = len([p for p in (out / "Data" / "db").glob("*") if p.suffix in (".csv", ".yaml")])
        print(f"db: {tables} text tables in {out / 'Data' / 'db'}")
    count = movies(out, a.movies)
    (out / "play.json").write_bytes((json.dumps(dict(
        exe=str(exe), install=str(a.install), text_tables=tables, movies=count,
        made=time.strftime("%Y-%m-%d %H:%M:%S")), indent=2) + "\n").encode())
    print(f"ready in {time.time() - t0:.0f} s: run {out / 'EE-AOC.exe'}")


if __name__ == "__main__":
    main()
