"""Pull the game's media from this repository's releases into data/, beside the committed data/db and data/ai: a
custom git LFS on GitHub Releases.

  python -I tools/fetch_data.py [--without textures-4x] [--tag PART=TAG ...] [--list] [--root DIR]

data/ is the game's Data folder. data/db (the tables and the rules documents) and data/ai (the computer players'
scripts) are in git; everything else in it is gitignored and comes from two release parts:
  media        open-media-<sha>        ee_open_media-<sha>.zip    models, the original textures, sounds, music,
                                                                  scripts (tools/open_data.py pack --part media)
  textures-4x  open-textures-4x-<sha>  ee_textures_4x-<sha>.zip   Data/Textures-4x, the AI-upscaled textures
                                                                  (tools/upscale; optional, 1.3 GB)
A part's Data/... files go into data/; its other files (Users/, WONLobby.cfg, zzWONVersion.cfg, fonts/ with the
font licence, its README) into build/data-root, the data root to run the game with: build/data-root/Data is a link
to data/ (a junction on Windows, a symlink elsewhere), so EE_DATA_ROOT=build/data-root plays the checkout's data/
as it is (tools/sim.py --data open, make_play_dir.py, posix_game.py and play_posix.py find it by themselves). The
game writes its Logs and config.yaml there and its saved games into data/ (gitignored), never into the repository
root.

data/.fetched.json records each part's release and the files and folders it installed. A part already at its newest
release, with all its files present, is skipped; a newer release first removes only that part's own old files.
--without removes a part's files. Nothing under data/db or data/ai is ever written or removed (a zip entry there is
skipped). A part's newest release is the latest one whose tag has the part's prefix (--tag pins one). Downloads are
cached in the main checkout's build/data_cache (shared by its worktrees). Needs the gh CLI.

--root DIR: fill another checkout layout instead of this one (DIR/data with its db and ai; DIR/build/data-root).
An old download from before this layout (data/Data, data/bundles.json, build/data) is not used and can be deleted.
"""
import argparse
import json
import os
import pathlib
import shutil
import subprocess
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
PARTS = [("media", "open-media-"), ("textures-4x", "open-textures-4x-")]
TRACKED = ("db", "ai")          # data/db and data/ai: the repository's, never touched here
MANIFEST = ".fetched.json"      # in data/ (gitignored)


def main_checkout():
    """the main checkout of this repository (a worktree shares its download cache)"""
    out = subprocess.run(["git", "-C", str(ROOT), "worktree", "list", "--porcelain"], capture_output=True, text=True)
    first = out.stdout.split("\n")[0]
    return pathlib.Path(first.split(" ", 1)[1]) if first.startswith("worktree ") else ROOT


CACHE = main_checkout() / "build" / "data_cache"


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


class Layout:
    """a checkout's data/ (the Data folder) and build/data-root (the data root: Data -> data/, the root files)"""

    def __init__(self, root):
        self.root = pathlib.Path(root).resolve()
        self.data = self.root / "data"
        self.game = self.root / "build" / "data-root"

    def target(self, name):
        """a zip member -> (its path in the layout, the path recorded) or None for one that is not ours to write"""
        parts = [p for p in name.split("/") if p]
        if not parts or any(p in ("..", ".") or ":" in p or "\\" in p for p in parts):
            return None
        if parts[0] == "Data":
            if len(parts) == 1 or parts[1].lower() in TRACKED:
                return None
            rel = "data/" + "/".join(parts[1:])
        elif parts[0].lower() in ("open_data.json", "data"):
            return None
        else:
            rel = "build/data-root/" + "/".join(parts)
        return self.root / rel, rel

    @staticmethod
    def tracked(rel):
        parts = rel.split("/")
        return parts[0] == "data" and len(parts) > 1 and parts[1].lower() in TRACKED

    def link(self):
        """build/data-root/Data -> data/ (a junction on Windows: no privileges needed)"""
        self.game.mkdir(parents=True, exist_ok=True)
        link = self.game / "Data"
        if os.path.lexists(link):
            try:
                if link.resolve() == self.data.resolve():
                    return link
            except OSError:
                pass
            if link.is_symlink():
                link.unlink()
            elif getattr(link, "is_junction", lambda: False)():
                os.rmdir(link)   # the junction itself, not its target
            else:
                sys.exit(f"{link} exists and is not a link to {self.data}: move it away")
        if sys.platform == "win32":
            import _winapi
            _winapi.CreateJunction(str(self.data), str(link))
        else:
            os.symlink(os.path.relpath(self.data, self.game), link)
        return link


def remove_part(lay, entry):
    """a part's recorded files, then its folders that are empty -> the number of files removed"""
    n = 0
    for rel in entry.get("files", []):
        if lay.tracked(rel):
            continue
        p = lay.root / rel
        if p.is_file() or p.is_symlink():
            p.unlink()
            n += 1
    for rel in sorted(entry.get("dirs", []), key=lambda r: r.count("/"), reverse=True):
        p = lay.root / rel
        if not lay.tracked(rel) and p.is_dir() and not p.is_symlink() and not any(p.iterdir()):
            p.rmdir()
    return n


def install_part(lay, zpath):
    """a part's zip into the layout -> its entry (the files and folders it wrote)"""
    files, dirs = [], set()
    skipped = 0
    with zipfile.ZipFile(zpath) as z:
        for info in z.infolist():
            t = lay.target(info.filename)
            if t is None:
                skipped += 1
                continue
            path, rel = t
            if info.is_dir():
                path.mkdir(parents=True, exist_ok=True)
                dirs.add(rel.rstrip("/"))
                continue
            path.parent.mkdir(parents=True, exist_ok=True)
            parent = rel.rsplit("/", 1)[0]
            while parent.count("/") >= 1 and parent != "build/data-root":
                dirs.add(parent)
                parent = parent.rsplit("/", 1)[0]
            if path.is_symlink() or path.exists():
                path.unlink()   # never write through a hard link into another tree
            with z.open(info) as src, open(path, "wb") as dst:
                shutil.copyfileobj(src, dst, 1 << 20)
            files.append(rel)
    if skipped:
        print(f"  {skipped} entries left out (data/db, data/ai, open_data.json: the repository's)")
    return dict(files=sorted(files), dirs=sorted(dirs))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0], epilog=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--without", action="append", default=[], choices=[p for p, _ in PARTS],
                    help="leave a part out, removing its files (repeatable)")
    ap.add_argument("--tag", action="append", default=[], metavar="PART=TAG", help="pin a part to a release")
    ap.add_argument("--list", action="store_true", help="print each part's newest release and exit")
    ap.add_argument("--root", default=str(ROOT), help="the checkout layout to fill (default: this checkout)")
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
    lay = Layout(a.root)
    if not (lay.data / "db").is_dir():
        sys.exit(f"{lay.data}: no db folder (not a checkout's data/)")
    record = lay.data / MANIFEST
    have = json.loads(record.read_text(encoding="utf-8")) if record.exists() else {}

    def save():
        record.write_bytes((json.dumps(have, indent=1) + "\n").encode())

    for part, tag in chosen:
        old = have.get(part)
        if part in a.without:
            if old:
                print(f"{part:12} removed ({remove_part(lay, old)} files of {old['tag']})")
                del have[part]
                save()
            continue
        if old and old.get("tag") == tag and all((lay.root / f).exists() for f in old["files"]):
            print(f"{part:12} {tag} (current)")
            continue
        z = download(tag)
        if old:
            print(f"{part:12} {old['tag']} -> {tag}: {remove_part(lay, old)} old files removed", flush=True)
            del have[part]
            save()
        print(f"{part:12} {tag}: unpacking into {lay.data}", flush=True)
        entry = install_part(lay, z)
        have[part] = dict(tag=tag, **entry)
        save()
        print(f"{part:12} {tag}: {len(entry['files'])} files")
    lay.link()
    for old in (lay.data / "Data", lay.data / "bundles.json", lay.root / "build" / "data"):
        if old.exists():
            print(f"note: {old} is an old download (before data/ was the Data folder): not used, it can be deleted")
    print(f"data ready: {lay.data} (data/db and data/ai as committed); run the game with EE_DATA_ROOT={lay.game}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
