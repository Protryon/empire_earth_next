"""AI upscaling of the game's textures (docs/UPSCALE_PLAN.md).

  <venv python> tools/upscale/upscale.py fetch                       models -> toolchain/upscale-models/
  <venv python> tools/upscale/upscale.py inventory [--json FILE]     units and categories
  <venv python> tools/upscale/upscale.py run --out DIR [--methods ultrasharp,siax,...] [--scale 2|4]
                                             [--category skin,...] [--units a,b] [--sample N] [--seed S]
  <venv python> tools/upscale/upscale.py sheet --out DIR             before/after sheets of a run (DIR/sheets/)
  <venv python> tools/upscale/upscale.py deploy --tree DIR/<method> --into PLAYDIR [--category skin,...]
                                             a play directory (EE_DATA_ROOT) with the upscaled textures in place

The venv: python3.12 -m venv toolchain/upscale-venv; pip install torch spandrel spandrel_extra_arches pillow
numpy pyyaml. Data root: --data, else $EE_DATA_ROOT, else <repo>/data. Toolchain: $EE_TOOLCHAIN, else
<repo>/toolchain.

`run` writes DIR/<method>/Data/Textures/<file>.png (an overlay tree: same names as the original) and
DIR/<method>/upscale.json (source hashes, model hash, parameters). With one method and no sampling it is the full
pass. Methods are model keys of models.yaml or lanczos / bicubic / nearest; `auto` picks per category
(CATEGORY_METHODS).
"""
import argparse
import hashlib
import json
import os
import pathlib
import random
import sys
import time
import urllib.request

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))
import inventory  # noqa: E402

TC = pathlib.Path(os.environ.get("EE_TOOLCHAIN", ROOT / "toolchain"))
MODELS_DIR = TC / "upscale-models"
PAD = 8   # source pixels of context around every image (wrapped for tileable ones)
COLOR_KEY_MIN = 0.005   # an RGB image with more exact pure blue (0,0,255) than this is colour-keyed (pointers)

# The pilot's verdict (2026-11-05, docs/UPSCALE_PLAN.md 6): skin, terrain, button, ui and background are a blend
# of 70% nomos8k_dat + 30% ultrasharp (two `run`s, then `blend`); fx and shadow Lanczos; exclude copied.
# FINAL_RECIPE is what tools/upscale/full_pass.sh does.
FINAL_RECIPE = {
    "blend": {"a": "nomos8k_dat", "b": "ultrasharp", "weight": 0.7,
              "categories": ["skin", "terrain", "button", "ui", "background"]},
    "lanczos": ["fx", "shadow"],
    "copy": ["exclude"],
}

# What `auto` uses per category (one method per category; the blend is not expressible here).
CATEGORY_METHODS = {
    "skin": "ultrasharp",
    "terrain": "ultrasharp",
    "button": "ultrasharp",
    "ui": "ultrasharp",
    "background": "ultrasharp",
    "fx": "lanczos",
    "shadow": "lanczos",
    "exclude": "copy",
}


def load_models():
    import yaml
    return yaml.safe_load(open(HERE / "models.yaml"))


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def data_root(args):
    return pathlib.Path(args.data or os.environ.get("EE_DATA_ROOT") or ROOT / "data")


# ------------------------------------------------------------------------------------------------- fetch --


def cmd_fetch(args):
    MODELS_DIR.mkdir(parents=True, exist_ok=True)
    models = load_models()
    bad = False
    for key, m in models.items():
        if args.only and key not in args.only.split(","):
            continue
        dst = MODELS_DIR / m["file"]
        if not dst.exists():
            print(f"fetch {key}: {m['url']}")
            tmp = dst.with_suffix(".part")
            req = urllib.request.Request(m["url"], headers={"User-Agent": "empire_earth_next-upscale"})
            with urllib.request.urlopen(req) as r, open(tmp, "wb") as f:
                while True:
                    b = r.read(1 << 20)
                    if not b:
                        break
                    f.write(b)
            tmp.rename(dst)
        h = sha256(dst)
        if m.get("sha256") and m["sha256"] != h:
            print(f"  {key}: sha256 MISMATCH {h} (pinned {m['sha256']}); refusing it")
            dst.rename(dst.with_suffix(".mismatch"))
            bad = True
        else:
            print(f"  {key}: {dst.stat().st_size >> 20} MB sha256 {h}{'' if m.get('sha256') else ' (not pinned yet)'}")
    return 1 if bad else 0


# --------------------------------------------------------------------------------------------- inventory --


def cmd_inventory(args):
    import collections
    units = inventory.build(str(data_root(args)))
    c = collections.Counter()
    f = collections.Counter()
    for u in units:
        c[u.category] += 1
        f[u.category] += len(u.files)
    for k in sorted(c):
        print(f"{k:12} {c[k]:5} units {f[k]:5} files")
    if args.json:
        json.dump([u.__dict__ for u in units], open(args.json, "w"), indent=1)
    return 0


# ---------------------------------------------------------------------------------------------- assembly --

LAYOUTS = [(mx, my, tr) for tr in (False, True) for my in (False, True) for mx in (False, True)]


def _place(k, cols, rows, layout):
    """Tile k's (column, row) in the assembled block under a candidate layout."""
    mx, my, tr = layout
    x, y = k % cols, k // cols
    if tr:
        x, y = k // rows, k % rows
    if mx:
        x = cols - 1 - x
    if my:
        y = rows - 1 - y
    return x, y


def assemble(tiles, cols, rows, layout):
    import numpy as np
    th, tw, ch = tiles[0].shape
    out = np.zeros((th * rows, tw * cols, ch), np.float32)
    for k, t in enumerate(tiles):
        x, y = _place(k, cols, rows, layout)
        out[y * th:(y + 1) * th, x * tw:(x + 1) * tw] = t
    return out


def split(img, cols, rows, layout, count):
    th, tw = img.shape[0] // rows, img.shape[1] // cols
    out = []
    for k in range(count):
        x, y = _place(k, cols, rows, layout)
        out.append(img[y * th:(y + 1) * th, x * tw:(x + 1) * tw])
    return out


def seam_cost(img, cols, rows):
    """Mean colour jump across the tile seams (toroidally) relative to the jump between neighbouring pixel rows
    inside the tiles. ~1 means the seams are invisible; a wrong arrangement gives clearly more."""
    import numpy as np
    th, tw = img.shape[0] // rows, img.shape[1] // cols
    rgb = img[:, :, :3]
    dy = np.abs(rgb - np.roll(rgb, -1, 0)).mean(axis=(1, 2))   # row i vs i+1
    dx = np.abs(rgb - np.roll(rgb, -1, 1)).mean(axis=(0, 2))
    seam_r = [th * r - 1 for r in range(1, rows + 1)]
    seam_c = [tw * c - 1 for c in range(1, cols + 1)]
    inner_r = np.delete(dy, seam_r).mean()
    inner_c = np.delete(dx, seam_c).mean()
    return (dy[seam_r].mean() / max(inner_r, 1e-6) + dx[seam_c].mean() / max(inner_c, 1e-6)) / 2


def best_layout(tiles, cols, rows):
    if cols * rows == 1:
        return LAYOUTS[0], 1.0
    cands = [l for l in LAYOUTS if not l[2] or cols == rows]
    scored = sorted((seam_cost(assemble(tiles, cols, rows, l), cols, rows), l) for l in cands)
    return scored[0][1], scored[0][0]


# -------------------------------------------------------------------------------------------- processing --


def method_path(method, models):
    if method in ("lanczos", "bicubic", "nearest", "copy"):
        return method
    m = models[method]
    p = MODELS_DIR / m["file"]
    if not p.exists():
        raise SystemExit(f"model {method} missing: run `upscale.py fetch`")
    return str(p)


def upscale_image(img, method, scale, wrap, args):
    """One RGB(A) image -> scale x. Alpha is bled under, upscaled with the same method and back-projected."""
    import numpy as np
    import process as P
    h, w, ch = img.shape
    if method == "copy":
        return img
    # Colour-keyed images (the mouse pointers: RTSMouse draws them with pure blue as transparent) must keep the
    # key exact: any upscaled blue that is off by one shows as a blue box. The key becomes alpha for processing
    # and is written back exactly where the result is transparent (a hard edge, as a colour key needs).
    # (`rally point pointer` has both an alpha channel and the key: the key also forces alpha 0 there.)
    keyed = (img[:, :, 0] == 0) & (img[:, :, 1] == 0) & (img[:, :, 2] == 1.0)
    keyed_alpha = ch == 4
    if keyed.mean() > COLOR_KEY_MIN:
        a0 = img[:, :, 3] if keyed_alpha else np.ones((h, w), np.float32)
        a0 = np.where(keyed, 0.0, a0).astype(np.float32)
        img = np.concatenate([img[:, :, :3], a0[:, :, None]], axis=2)
        ch = 4
    else:
        keyed = None
    rgb = img[:, :, :3]
    alpha = img[:, :, 3:4] if ch == 4 else None
    if alpha is not None:
        rgb = P.bleed(rgb, alpha[:, :, 0])
    p = min(PAD, h, w)

    def up(x3):
        x = P.upscale_rgb(P.pad(x3, p, wrap), method, scale)
        s = x.shape[0] // (h + 2 * p)
        x = P.crop(x, p, s)
        if s != scale:
            x = P.resize(x, w * scale, h * scale)
        return x

    out = up(rgb)
    if args.back_project:
        out = P.back_project(out, rgb)
    out = np.clip(out, 0, 1)
    if alpha is not None:
        if args.alpha == "same":
            a = up(np.repeat(alpha, 3, axis=2)).mean(axis=2, keepdims=True)
        else:
            a = P.upscale_rgb(alpha, "lanczos", scale)
        if args.back_project:
            a = P.back_project(a, alpha)
        out = np.concatenate([out, np.clip(a, 0, 1)], axis=2)
    if keyed is not None:
        solid = out[:, :, 3] >= 0.5
        res = out[:, :, :3].copy()
        res[~solid] = (0.0, 0.0, 1.0)
        # an opaque pixel must never be the key by accident
        clash = solid & (res[:, :, 0] < 0.5 / 255) & (res[:, :, 1] < 0.5 / 255) & (res[:, :, 2] > 254.5 / 255)
        res[clash, 2] = 254 / 255
        if keyed_alpha:
            return np.concatenate([res, np.where(solid, out[:, :, 3], 0.0)[:, :, None]], axis=2)
        return res
    return out


def process_unit(u, method, tex, outdir, models, args, log):
    import process as P
    mp = method_path(method, models)
    scale = args.scale
    paths = [os.path.join(tex, f) for f in u.files]
    imgs = [P.load(p) for p in paths]
    note = {}
    if u.kind in ("terrain", "grid"):
        if any(i.shape != imgs[0].shape for i in imgs):
            imgs = [i[:, :, :3] if i.shape[2] == 4 else i for i in imgs]   # mixed alpha: drop it (never in data)
        if u.kind == "terrain":
            layout, cost = best_layout(imgs, u.cols, u.rows)
            note["layout"], note["seam"] = list(layout), round(float(cost), 3)
        else:
            layout = (False, False, False)                   # name_<row>_<col>: row-major, as named
        whole = assemble(imgs, u.cols, u.rows, layout)
        res = upscale_image(whole, mp, scale, u.tileable, args)
        outs = split(res, u.cols, u.rows, layout, len(imgs))
    else:
        outs = [upscale_image(i, mp, scale, u.tileable, args) for i in imgs]
    for f, o in zip(u.files, outs):
        P.save(os.path.join(outdir, f), o, {"ee:upscale": f"{method} x{scale}"})
    log[u.id] = {"method": method, "category": u.category, "kind": u.kind, "files": u.files, **note}


def select(units, args):
    us = [u for u in units if not args.category or u.category in args.category.split(",")]
    if args.units:
        want = set(args.units.split(","))
        us = [u for u in us if u.id in want]
    if args.sample:
        rnd = random.Random(args.seed)
        by = {}
        for u in us:
            by.setdefault(u.category, []).append(u)
        us = [u for c in sorted(by) for u in rnd.sample(by[c], min(args.sample, len(by[c])))]
    return us


def cmd_run(args):
    models = load_models()
    root = data_root(args)
    tex = os.path.join(root, "Data", "Textures")
    units = select(inventory.build(str(root)), args)
    ids = [u.id for u in units]
    if len(set(ids)) != len(ids):
        raise SystemExit("duplicate unit ids: " + ", ".join(sorted({i for i in ids if ids.count(i) > 1})))
    out = pathlib.Path(args.out)
    for method in args.methods.split(","):
        mdir = out / method
        tdir = mdir / "Data" / "Textures"
        tdir.mkdir(parents=True, exist_ok=True)
        log = {}
        t0 = time.time()
        for i, u in enumerate(units):
            m = CATEGORY_METHODS[u.category] if method == "auto" else method
            if u.category in ("exclude",) or (u.category == "shadow" and method != "auto" and not args.force_all):
                m = CATEGORY_METHODS[u.category]
            t = time.time()
            process_unit(u, m, tex, str(tdir), models, args, log)
            print(f"[{method}] {i + 1}/{len(units)} {u.category:10} {u.id} ({time.time() - t:.1f}s)", flush=True)
        manifest = {
            "method": method,
            "scale": args.scale,
            "alpha": args.alpha,
            "back_project": args.back_project,
            "models": {k: {"file": v["file"], "sha256": sha256(MODELS_DIR / v["file"])}
                       for k, v in models.items() if (MODELS_DIR / v["file"]).exists()
                       and k in {e["method"] for e in log.values()}},
            "sources": {f: sha256(os.path.join(tex, f)) for e in log.values() for f in e["files"]},
            "units": log,
            "seconds": round(time.time() - t0, 1),
        }
        json.dump(manifest, open(mdir / "upscale.json", "w"), indent=1)
        print(f"[{method}] {len(units)} units in {manifest['seconds']}s -> {mdir}")
    json.dump({"units": [u.id for u in units], "methods": args.methods.split(","), "scale": args.scale,
               "data": str(root)}, open(out / "run.json", "w"), indent=1)
    return 0


# ------------------------------------------------------------------------------------------------- sheet --


def cmd_sheet(args):
    """One PNG per unit: the original (nearest, at the output size), then each method; and an index.html."""
    import numpy as np
    from PIL import Image, ImageDraw
    import process as P
    out = pathlib.Path(args.out)
    run = json.load(open(out / "run.json"))
    tex = os.path.join(run["data"], "Data", "Textures")
    sdir = out / "sheets"
    sdir.mkdir(exist_ok=True)
    units = {u.id: u for u in inventory.build(run["data"])}
    html = ["<!doctype html><meta charset=utf-8><title>Upscale pilot</title><style>body{background:#222;color:#ddd;"
            "font:13px sans-serif}img{max-width:100%;image-rendering:pixelated}</style>"]
    for uid in run["units"]:
        u = units[uid]
        f = u.files[0] if u.kind in ("single", "frames") else None
        cols = []

        def whole(d):
            imgs = [P.load(os.path.join(d, x)) for x in u.files]
            if u.kind == "grid":
                return assemble(imgs, u.cols, u.rows, (False, False, False))
            if u.kind == "terrain":
                lay = best_layout([P.load(os.path.join(tex, x)) for x in u.files], u.cols, u.rows)[0]
                return assemble(imgs, u.cols, u.rows, lay)
            return imgs[0]
        orig = whole(tex)
        labels = ["original (nearest)"]
        cols.append(P.resize(orig, orig.shape[1] * run["scale"], orig.shape[0] * run["scale"], Image.NEAREST))
        for m in run["methods"]:
            d = out / m / "Data" / "Textures"
            if (d / u.files[0]).exists():
                cols.append(whole(str(d)))
                labels.append(m)
        H = max(c.shape[0] for c in cols)
        cells = []
        for c, lab in zip(cols, labels):
            if c.shape[2] == 4:   # alpha over a checkerboard
                yy, xx = np.mgrid[: c.shape[0], : c.shape[1]]
                chk = (((yy // 16 + xx // 16) % 2) * 0.25 + 0.5)[:, :, None]
                c = c[:, :, :3] * c[:, :, 3:] + chk * (1 - c[:, :, 3:])
            im = Image.fromarray(np.clip(c * 255, 0, 255).astype(np.uint8)[:, :, :3])
            canvas = Image.new("RGB", (im.width, H + 18), (34, 34, 34))
            canvas.paste(im, (0, 18))
            ImageDraw.Draw(canvas).text((2, 2), lab, fill=(230, 230, 230))
            cells.append(canvas)
        W = sum(c.width for c in cells) + 4 * (len(cells) - 1)
        sheet = Image.new("RGB", (W, H + 18), (34, 34, 34))
        x = 0
        for c in cells:
            sheet.paste(c, (x, 0))
            x += c.width + 4
        name = f"{u.category}__{uid.replace(' ', '_')}.png"
        sheet.save(sdir / name)
        html.append(f"<h3>{u.category}: {uid} ({u.kind}, {u.size[0]}x{u.size[1]}"
                    f"{', alpha' if u.alpha else ''})</h3><img src='{name}'>")
    (sdir / "index.html").write_text("\n".join(html))
    print(f"{len(run['units'])} sheets -> {sdir}")
    return 0


# ------------------------------------------------------------------------------------------------- blend --


def cmd_blend(args):
    """A weighted mix of two method trees of one run: out = w * A + (1 - w) * B, per pixel (alpha included).
    Equal to blending the model outputs inside the pipeline: back-projection is affine in the upscaled image, so
    it commutes with a convex mix (up to the final clip). Files only one tree has are skipped."""
    import numpy as np
    import process as P
    a, b = pathlib.Path(args.a), pathlib.Path(args.b)
    out = pathlib.Path(args.out)
    (out / "Data" / "Textures").mkdir(parents=True, exist_ok=True)
    ma, mb = json.load(open(a / "upscale.json")), json.load(open(b / "upscale.json"))
    w = args.weight
    name = out.name
    units = {}
    for uid, e in ma["units"].items():
        if uid not in mb["units"]:
            continue
        for f in e["files"]:
            ia = P.load(a / "Data" / "Textures" / f)
            ib = P.load(b / "Data" / "Textures" / f)
            if ia.shape != ib.shape:
                raise SystemExit(f"{f}: the trees differ in size or channels")
            mix = np.clip(w * ia + (1 - w) * ib, 0, 1)
            # colour-keyed (pointers, see upscale_image): A decides where the exact key is; where only B is keyed,
            # A's pixel (a mix with the key's blue would show as a blue fringe)
            ka = (ia[:, :, 0] == 0) & (ia[:, :, 1] == 0) & (ia[:, :, 2] == 1.0)
            kb = (ib[:, :, 0] == 0) & (ib[:, :, 1] == 0) & (ib[:, :, 2] == 1.0)
            if ka.mean() > COLOR_KEY_MIN:
                mix[ka] = ia[ka]
                mix[~ka & kb] = ia[~ka & kb]
            P.save(out / "Data" / "Textures" / f, mix,
                   {"ee:upscale": f"{ma['method']} {w:g} + {mb['method']} {1 - w:g} x{ma['scale']}"})
        same = e["method"] == mb["units"][uid]["method"]
        units[uid] = {**e, "method": e["method"] if same else f"{e['method']}*{w:g}+{mb['units'][uid]['method']}"}
    man = {**ma, "method": name, "blend": {"a": ma["method"], "b": mb["method"], "weight": w},
           "models": {**ma["models"], **mb["models"]}, "units": units,
           "seconds": round(ma["seconds"] + mb["seconds"], 1)}
    json.dump(man, open(out / "upscale.json", "w"), indent=1)
    print(f"{len(units)} units -> {out}")
    return 0


def cmd_compose(args):
    """One tree from several: each unit is taken from the first --tree that has it (in the given order). The
    result is a complete overlay with one upscale.json naming, per unit, the tree and method it came from."""
    import shutil
    out = pathlib.Path(args.out)
    tdir = out / "Data" / "Textures"
    tdir.mkdir(parents=True, exist_ok=True)
    units, sources, models, scale = {}, {}, {}, None
    for t in args.tree:
        t = pathlib.Path(t)
        man = json.load(open(t / "upscale.json"))
        if scale is None:
            scale = man["scale"]
        elif man["scale"] != scale:
            raise SystemExit(f"{t}: scale {man['scale']}, the others {scale}")
        models.update(man.get("models", {}))
        for uid, e in man["units"].items():
            if all(f in sources for f in e["files"]):   # first tree wins, per file set (ids can differ by tree)
                continue
            if any(f in sources for f in e["files"]):
                raise SystemExit(f"{t}: unit {uid} overlaps an earlier tree's unit in some files only")
            for f in e["files"]:
                shutil.copy2(t / "Data" / "Textures" / f, tdir / f)
                sources[f] = man["sources"].get(f)
            key = uid if uid not in units else f"{uid}#{t.name}"
            units[key] = {**e, "tree": t.name}
    json.dump({"method": "composed", "scale": scale, "trees": [pathlib.Path(t).name for t in args.tree],
               "models": models, "sources": sources, "units": units}, open(out / "upscale.json", "w"), indent=1)
    print(f"{len(units)} units, {len(sources)} files -> {out}")
    return 0


# ------------------------------------------------------------------------------------------------ deploy --

DEPLOY_DEFAULT = "skin,terrain,fx,shadow"   # deploy swaps files in place: the game sees 2D textures at their pixel size
# The pack: everything but the excluded data textures. The game keeps a pack texture's layout (logical) size
# apart from its pixel size, so UI textures can be upscaled too.
PACK_DEFAULT = "skin,terrain,fx,shadow,button,ui,background"


def cmd_deploy(args):
    """A play directory for EE_DATA_ROOT: symlinks to the original data root's entries, Data/ and Data/Textures/
    as real directories, and the upscaled textures of the chosen categories in place of the originals. Users/ and
    config.ini are copied (the game writes them). Rebuilt from scratch each time."""
    import shutil
    src = data_root(args).resolve()
    tree = pathlib.Path(args.tree).resolve()
    man = json.load(open(tree / "upscale.json"))
    cats = set(args.category.split(","))
    swap = {f for e in man["units"].values() if e["category"] in cats for f in e["files"]}
    dst = pathlib.Path(args.into)
    if dst.exists():
        if not (dst / ".upscale-deploy").exists():
            raise SystemExit(f"{dst} exists and is not a deploy directory; refusing to replace it")
        shutil.rmtree(dst)
    (dst / "Data" / "Textures").mkdir(parents=True)
    (dst / ".upscale-deploy").write_text(json.dumps({"data": str(src), "tree": str(tree), "categories": sorted(cats)}))
    for e in src.iterdir():
        if e.name == "Data":
            continue
        if e.name in ("Users",) or e.name.endswith(".ini"):
            (shutil.copytree if e.is_dir() else shutil.copy2)(e, dst / e.name)
        else:
            (dst / e.name).symlink_to(e)
    for e in (src / "Data").iterdir():
        if e.name != "Textures":
            (dst / "Data" / e.name).symlink_to(e)
    n = 0
    tscale = man["scale"]
    if args.scale and tscale % args.scale:
        raise SystemExit(f"--scale {args.scale} does not divide the tree's scale {tscale}")
    for e in (src / "Data" / "Textures").iterdir():
        up = tree / "Data" / "Textures" / e.name
        d = dst / "Data" / "Textures" / e.name
        if e.name in swap and up.exists():
            if args.scale and args.scale != tscale:
                # a lower scale from the master: box filter (exact averages of the master's pixels)
                import process as P
                from PIL import Image
                a = P.load(up)
                P.save(d, P.resize(a, a.shape[1] * args.scale // tscale, a.shape[0] * args.scale // tscale,
                                   Image.BOX), {"ee:upscale": f"{man['method']} x{args.scale} (from x{tscale})"})
            else:
                d.symlink_to(up)
            n += 1
        else:
            d.symlink_to(e)
    print(f"{n} upscaled textures x{args.scale or tscale} ({','.join(sorted(cats))}) -> {dst}; "
          f"run with EE_DATA_ROOT={dst.resolve()}")
    return 0


def cmd_pack(args):
    """The shipped pack: DATA_ROOT/Data/Textures-4x/ (or --into DIR) with the tree's textures of the chosen
    categories, copied (a pack must stand alone), plus pack.json naming what is in it. The game reads it when the
    texture resolution setting is 2x or 4x (config.ini [Game] Texture Resolution; 2x halves the 4x file at load)
    and falls back to Data/Textures per file. All categories but `exclude` by default (PACK_DEFAULT): the excluded
    textures are data (palettes) or not worth it, and must never come from the pack."""
    import shutil
    tree = pathlib.Path(args.tree).resolve()
    man = json.load(open(tree / "upscale.json"))
    if man["scale"] != 4:
        raise SystemExit(f"the pack holds 4x textures; this tree is x{man['scale']}")
    cats = set(args.category.split(","))
    dst = pathlib.Path(args.into) if args.into else data_root(args) / "Data" / "Textures-4x"
    if dst.exists():
        if not (dst / "pack.json").exists():
            raise SystemExit(f"{dst} exists and is not a texture pack; refusing to replace it")
        shutil.rmtree(dst)
    dst.mkdir(parents=True)
    files = sorted(f for e in man["units"].values() if e["category"] in cats and e.get("method") != "copy"
                   for f in e["files"])
    size = 0
    for f in files:
        shutil.copy2(tree / "Data" / "Textures" / f, dst / f)
        size += (dst / f).stat().st_size
    json.dump({"scale": 4, "categories": sorted(cats), "files": len(files), "bytes": size,
               "recipe": FINAL_RECIPE, "models": man.get("models", {}),
               "sources": {f: man["sources"].get(f) for f in files}}, open(dst / "pack.json", "w"), indent=1)
    print(f"{len(files)} textures, {size / 1e6:.0f} MB -> {dst}")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--data")
    sub = ap.add_subparsers(dest="cmd", required=True)
    f = sub.add_parser("fetch")
    f.add_argument("--only")
    i = sub.add_parser("inventory")
    i.add_argument("--json")
    r = sub.add_parser("run")
    r.add_argument("--out", required=True)
    r.add_argument("--methods", default="auto")
    r.add_argument("--scale", type=int, default=4, choices=(2, 4))
    r.add_argument("--category")
    r.add_argument("--units")
    r.add_argument("--sample", type=int)
    r.add_argument("--seed", type=int, default=1)
    r.add_argument("--alpha", default="same", choices=("same", "lanczos"))
    r.add_argument("--no-back-project", dest="back_project", action="store_false")
    r.add_argument("--force-all", action="store_true", help="also run the method on shadows")
    s = sub.add_parser("sheet")
    s.add_argument("--out", required=True)
    bl = sub.add_parser("blend")
    bl.add_argument("--a", required=True, help="method tree A (holding upscale.json)")
    bl.add_argument("--b", required=True, help="method tree B")
    bl.add_argument("--weight", type=float, default=0.7, help="weight of A")
    bl.add_argument("--out", required=True, help="the blend's tree; its directory name is the method name")
    co = sub.add_parser("compose")
    co.add_argument("--tree", action="append", required=True, help="method trees, first wins (repeat)")
    co.add_argument("--out", required=True)
    pk = sub.add_parser("pack")
    pk.add_argument("--tree", required=True, help="a 4x tree, normally the full pass's final/")
    pk.add_argument("--into", help="default: <data root>/Data/Textures-4x")
    pk.add_argument("--category", default=PACK_DEFAULT)
    d = sub.add_parser("deploy")
    d.add_argument("--tree", required=True, help="a run's method directory (holding upscale.json)")
    d.add_argument("--into", required=True)
    d.add_argument("--category", default=DEPLOY_DEFAULT)
    d.add_argument("--scale", type=int, help="deploy at this scale (a divisor of the tree's), made from the tree")
    args = ap.parse_args()
    return {"fetch": cmd_fetch, "inventory": cmd_inventory, "run": cmd_run, "sheet": cmd_sheet,
            "blend": cmd_blend, "compose": cmd_compose, "pack": cmd_pack, "deploy": cmd_deploy}[args.cmd](args)


if __name__ == "__main__":
    sys.exit(main())
