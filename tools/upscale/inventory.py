"""The texture inventory: every PNG of Data/Textures grouped into units and given a category.

A *unit* is what is upscaled as one image:
  * single     one PNG
  * frames     `name.png`, `name@1.png`, ...: the frames of one texture (same settings, processed one by one)
  * terrain    a dbterrain texture set: `base_1..base_N`, N = mColumns x mRows tiles that repeat as one block
               (tileable: processed toroidally so the block's own wrap stays seamless)
  * grid       a UI background cut into tiles `name_<row>_<col>` (1-based); assembled, processed whole, cut again

Categories decide the method (see CATEGORY_METHODS in upscale.py): skin (unit / building / ambient model
textures, dbgraphics), terrain, button (dbbuttons), ui (dbuicontrols, dbuiback and the leftovers), background
(grids), fx (effects), shadow (sha_*: blurry blobs, Lanczos only), exclude (data, not pictures: palettes,
swatches, solid fills, very small images).
"""
import csv
import os
import re
import struct
from dataclasses import dataclass, field

# Pictures that are read as data or are too small or flat to gain anything. Matched against the lowercase stem.
EXCLUDE_STEMS = {"colorpalette", "waterpalette", "dither", "blank"}
EXCLUDE_PREFIXES = ("white_",)          # solid fills (terrain "White_2" and the swatches)
MIN_SIDE = 16                            # images with a side below this are left alone


def png_info(path):
    with open(path, "rb") as f:
        b = f.read(26)
    w, h = struct.unpack(">II", b[16:24])
    return w, h, b[25]                   # colour type: 2 RGB, 6 RGBA, 3 palette


@dataclass
class Unit:
    id: str                              # the unit's name (lowercase stem without frame / tile suffix)
    kind: str                            # single | frames | terrain | grid
    category: str
    files: list                          # file names (in Textures/); for terrain / grid: row-major tiles
    cols: int = 1
    rows: int = 1
    tileable: bool = False
    source: str = ""                     # why it got its category (db table or rule)
    size: tuple = (0, 0)                 # one tile's / frame's size
    alpha: bool = False
    notes: list = field(default_factory=list)


def _read_db_csv(db, name):
    path = os.path.join(db, name)
    if not os.path.exists(path):
        return []
    with open(path, encoding="latin-1", newline="") as f:
        lines = [l for l in f if not l.startswith("#")]
    return list(csv.DictReader(lines))


def _tex_stem(s):
    s = s.strip().replace("\\\\", "\\").lower()
    if s.startswith("textures\\"):
        s = s[len("textures\\"):]
    for ext in (".sst", ".tga", ".png"):
        if s.endswith(ext):
            s = s[: -len(ext)]
    return s


def _referenced(db, table, columns=None):
    """Lowercase stems of every textures\\... string in a db text table."""
    path = os.path.join(db, table)
    if not os.path.exists(path):
        return set()
    text = open(path, encoding="latin-1").read()
    return {_tex_stem(m.group(1)) for m in re.finditer(r'(textures\\{1,2}[^",\n\]]+)', text, re.I)}


def build(data_root):
    tex = os.path.join(data_root, "Data", "Textures")
    db = os.path.join(data_root, "Data", "db")
    files = {}                                       # lowercase stem -> file name
    for n in sorted(os.listdir(tex)):
        if n.lower().endswith(".png"):
            files[n[:-4].lower()] = n
    taken = set()
    units = []

    def take(stems):
        for s in stems:
            taken.add(s)

    # --- terrain sets (dbterrain): base_1..base_N
    for row in _read_db_csv(db, "dbterrain.csv"):
        base = _tex_stem(row["mTextureName"])
        cols, rows = int(row["mColumns"]), int(row["mRows"])
        stems = [f"{base}_{k + 1}" for k in range(cols * rows)]
        if any(s in taken for s in stems) or not all(s in files for s in stems):
            continue
        cat = "exclude" if base.startswith(EXCLUDE_PREFIXES) else "terrain"
        kind = "terrain"
        if base.startswith("water_ocean"):          # 16 x 1: animation frames of the water, each a tile
            kind = "frames"
        units.append(Unit(base, kind, cat, [files[s] for s in stems], cols, rows, tileable=True,
                          source=f"dbterrain {row['id']}"))
        take(stems)

    # --- UI background grids: name_<r>_<c>, complete R x C
    grid = {}
    for s in files:
        m = re.fullmatch(r"(.+)_(\d+)_(\d+)", s)
        if m and s not in taken:
            grid.setdefault(m.group(1), {})[(int(m.group(2)), int(m.group(3)))] = s
    for base, cells in sorted(grid.items()):
        R = max(r for r, _ in cells)
        C = max(c for _, c in cells)
        if len(cells) != R * C or min(r for r, _ in cells) != 1 or min(c for _, c in cells) != 1 or R < 2:
            continue                             # 1-row groups (desert_grass_1_1..3, side_1_1..8) are terrain pieces
        if png_info(os.path.join(tex, files[cells[(1, 1)]]))[:2] != (256, 256):
            continue
        stems = [cells[(r, c)] for r in range(1, R + 1) for c in range(1, C + 1)]
        units.append(Unit(base, "grid", "background", [files[s] for s in stems], C, R, source="name_r_c grid"))
        take(stems)

    # --- the rest: single images, @N frames grouped with their base
    graphics = _referenced(db, "dbgraphics.yaml")
    buttons = _referenced(db, "dbbuttons.csv")
    ui = _referenced(db, "dbuicontrols.csv") | _referenced(db, "dbuiback.csv") | _referenced(db, "dbuicontrolevents.csv")
    frames = {}
    for s in files:
        if s in taken:
            continue
        base = s.split("@")[0]
        frames.setdefault(base, []).append(s)
    for base, stems in sorted(frames.items()):
        stems.sort(key=lambda s: int(s.split("@")[1]) if "@" in s else 0)
        if base in buttons or base.startswith(("but_", "button")):
            cat, src = "button", "dbbuttons" if base in buttons else "prefix"
        elif base.startswith("sha_") or base.endswith("shadowbitmap"):
            cat, src = "shadow", "prefix"
        elif base.startswith(("fx_", "sfx_")):
            cat, src = "fx", "prefix"
        elif base in graphics:
            cat, src = "skin", "dbgraphics"
        elif base in ui:
            cat, src = "ui", "dbui*"
        elif base.startswith(("bld_", "men_", "gun_", "nav_", "air_", "amb_", "spc_")):
            cat, src = "skin", "prefix"
        elif re.fullmatch(r"(desert|marsh|rock|dwildgrass|gwildgrass|side|cliffedge|border)(_[a-z]+)?_\d+(_\d+)?", base):
            cat, src = "terrain", "terrain piece (not tileable on its own)"
        else:
            cat, src = "ui", "default"
        if base in EXCLUDE_STEMS or base.startswith(EXCLUDE_PREFIXES):
            cat, src = "exclude", "data or fill"
        units.append(Unit(base, "frames" if len(stems) > 1 else "single", cat, [files[s] for s in stems], source=src))
        take(stems)

    # Ids key the manifests, so they must be unique: a grid and a frames unit can share a base name
    # (splash_<r>_<c> and splash.png / splash@N.png). A later unit with a taken id gets "#<kind>".
    seen = set()
    for u in units:
        if u.id in seen:
            u.id = f"{u.id}#{u.kind}"
        seen.add(u.id)

    for u in units:
        w, h, ct = png_info(os.path.join(tex, u.files[0]))
        u.size = (w, h)
        u.alpha = any(png_info(os.path.join(tex, f))[2] in (4, 6) for f in u.files)
        if u.category != "exclude" and min(w, h) < MIN_SIDE:
            u.category, u.source = "exclude", f"smaller than {MIN_SIDE}px"
    return units
