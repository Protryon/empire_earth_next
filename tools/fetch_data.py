"""Pull the game data from this repository's private releases: the three parts, unzipped into one folder.

  python -I tools/fetch_data.py [OUT] [--without textures-4x] [--tag PART=TAG ...] [--list]

The data ships in three parts that unzip into one data root (next to the exe, or EE_DATA_ROOT):
  media        open-media-<sha>        ee_open_media-<sha>.zip    models, the original textures, sounds, music,
                                                                  scripts, fonts (tools/open_data.py pack --part media)
  textures-4x  open-textures-4x-<sha>  ee_textures_4x-<sha>.zip   Data/Textures-4x, the AI-upscaled textures
                                                                  (tools/upscale; optional, 1.3 GB)
  db           open-db-<sha>           ee_open_db-<sha>.zip       Data/db: the tables and the rules documents, the
                                                                  tweakables (tools/open_data.py pack --part db)
Each part is released on its own, so a format change republishes only the db part. OUT (default data/, gitignored)
gets them in that order; OUT/bundles.json records which release each came from, and a part already at its newest
release is skipped. A part's newest release is the latest one whose tag has the part's prefix (--tag pins one).
Downloads are kept in build/data_cache. Needs the gh CLI, logged in with access to the repository.
"""
import argparse
import json
import pathlib
import shutil
import subprocess
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
CACHE = ROOT / "build" / "data_cache"
PARTS = [("media", "open-media-"), ("textures-4x", "open-textures-4x-"), ("db", "open-db-")]


def repo():
    """owner/name of the origin remote (gh would otherwise pick a repository itself: this checkout also has the
    upstream remote, whose releases are not ours)"""
    url = subprocess.run(["git", "remote", "get-url", "origin"], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    name = url.split("github.com")[-1].lstrip(":/")
    return name[:-4] if name.endswith(".git") else name


def gh(*args):
    r = subprocess.run(["gh", *args, "-R", repo()], cwd=ROOT, capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"gh {' '.join(args)} failed: {r.stderr.strip()}")
    return r.stdout


def releases():
    """[(tag, published)] newest first"""
    rows = json.loads(gh("release", "list", "--limit", "200", "--json", "tagName,publishedAt"))
    return sorted(((r["tagName"], r["publishedAt"] or "") for r in rows), key=lambda r: r[1], reverse=True)


def newest(tags, prefix):
    for tag, _ in tags:
        if tag.startswith(prefix):
            return tag
    return None


def download(tag):
    """the release's zip in the cache (downloaded once)"""
    assets = json.loads(gh("release", "view", tag, "--json", "assets"))["assets"]
    zips = [x for x in assets if x["name"].lower().endswith(".zip")]
    if len(zips) != 1:
        sys.exit(f"{tag}: expected one .zip asset, found {[x['name'] for x in assets]}")
    name, size = zips[0]["name"], zips[0]["size"]
    dest = CACHE / tag / name
    if dest.exists() and dest.stat().st_size == size:
        return dest
    dest.parent.mkdir(parents=True, exist_ok=True)
    print(f"{tag}: downloading {name} ({size / 1e6:.0f} MB)", flush=True)
    gh("release", "download", tag, "--pattern", name, "--dir", str(dest.parent), "--clobber")
    if dest.stat().st_size != size:
        sys.exit(f"{dest}: {dest.stat().st_size} bytes, the release says {size}")
    return dest


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0], epilog=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?", default=str(ROOT / "data"))
    ap.add_argument("--without", action="append", default=[], choices=[p for p, _ in PARTS],
                    help="leave a part out (repeatable)")
    ap.add_argument("--tag", action="append", default=[], metavar="PART=TAG", help="pin a part to a release")
    ap.add_argument("--list", action="store_true", help="print each part's newest release and exit")
    a = ap.parse_args()
    pins = dict(t.split("=", 1) for t in a.tag)
    tags = releases()
    chosen = []
    for part, prefix in PARTS:
        tag = pins.get(part) or newest(tags, prefix)
        if tag is None:
            sys.exit(f"no release with a tag starting {prefix} ({part})")
        chosen.append((part, tag))
    if a.list:
        for part, tag in chosen:
            print(f"{part:12} {tag}")
        return 0
    out = pathlib.Path(a.out).resolve()
    if out.is_relative_to(ROOT) and not (out.is_relative_to(ROOT / "build") or out.is_relative_to(ROOT / "data")):
        sys.exit(f"{out} is inside the repository: use data/, build/ or a folder outside it")
    record = out / "bundles.json"
    have = json.loads(record.read_text(encoding="utf-8")) if record.exists() else {}
    want = [(p, t) for p, t in chosen if p not in a.without]
    # an older part under a newer one would mix two builds' files: when the media part changes, start clean
    if out.exists() and have.get("media") not in (None, dict(want).get("media")):
        print(f"{out}: the media part changes ({have.get('media')} -> {dict(want)['media']}): replacing the folder")
        if not record.exists():
            sys.exit(f"{out} has no bundles.json: not made by this script, refusing to delete it")
        shutil.rmtree(out)
        have = {}
    out.mkdir(parents=True, exist_ok=True)
    for part, tag in want:
        if have.get(part) == tag:
            print(f"{part:12} {tag} (current)")
            continue
        z = download(tag)
        if part == "textures-4x" and (out / "Data" / "Textures-4x").exists():
            shutil.rmtree(out / "Data" / "Textures-4x")   # a pack replaces the one before whole
        if part == "db" and (out / "Data" / "db").exists():
            shutil.rmtree(out / "Data" / "db")            # so a table the new release dropped goes too
        print(f"{part:12} {tag}: unzipping into {out}", flush=True)
        with zipfile.ZipFile(z) as f:
            f.extractall(out)
        have[part] = tag
        record.write_text(json.dumps(have, indent=2) + "\n", encoding="utf-8")
    if "textures-4x" in a.without and have.pop("textures-4x", None) and (out / "Data" / "Textures-4x").exists():
        shutil.rmtree(out / "Data" / "Textures-4x")
    record.write_text(json.dumps(have, indent=2) + "\n", encoding="utf-8")
    print(f"data ready in {out}: run the game with EE_DATA_ROOT={out} (or make_play_dir.py --data {out})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
