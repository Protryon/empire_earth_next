# AI upscaling of the textures

Goal: higher-resolution versions of the game's textures (Data/Textures, 2,666 PNGs of the open data bundle), made
offline by machine-learning upscalers, shipped as an overlay tree with the same file names. The tool is
`tools/upscale/upscale.py`.

## 1. What the data is

* Sizes: 64² (1,148), 128² (690), 256² (581), 32² (193), 16² (54), a few others. 56 megapixels in all (~300 MB
  RGBA with mips at 1x; 2x: ~1.2 GB, 4x: ~4.8 GB if everything were resident at once).
* `tools/upscale/inventory.py` groups the files into **units** (what is upscaled as one image) and categories:

| Category | Units | What | Method (until the pilot decides) |
|---|---|---|---|
| skin | ~730 | unit / building / ambient model textures (dbgraphics, or bld_/men_/gun_/nav_/air_/amb_ prefix); 52 with `@N` frames | ESRGAN-family model |
| terrain | ~55 sets | dbterrain texture sets: `base_1..N`, mColumns x mRows tiles repeating as one block; plus loose terrain pieces | model, toroidal (wrap) padding |
| button | ~735 | dbbuttons icons | model |
| ui | ~200 | dbuicontrols / dbuiback / cursors / the rest | model |
| background | ~25 grids | UI backgrounds cut into 256² tiles `name_<row>_<col>` (1024 x 768 = 3 x 4) | assembled, model, cut again |
| fx | ~100 | effects (fx_, sfx_) | Lanczos for now |
| shadow | ~150 | blob shadows (sha_) | Lanczos (nothing for a model to add) |
| exclude | ~30 | data read as pixels (colorpalette, waterpalette), solid fills (white_*), images under 16 px | copied |

* Terrain tiling: `EETerrainTable::AttachTextures` places tile k = cols*y + x at column cols-1-x, row y. The tool
  does not rely on that: it picks the arrangement whose seams are least visible (`seam_cost`, recorded per unit
  in upscale.json) and processes the assembled block toroidally, so both the inner seams and the block's own
  wrap stay seamless.
* Player colour is a material colour ("player color" materials), not a texture mask: nothing special for it.

## 2. Engine constraints

1. **3D textures need no code change.** The SST header's size is the GPU surface size (GETexture.cpp ~280) and
   model UVs are normalised: a 4x PNG makes a 4x surface. The game converts PNGs at load (GEOpenFormats
   PngToSst), so an overlay of PNGs is enough.
2. **2D / UI code uses pixel sizes** for layout: RTSMouse (cursor), UIManager, EEButtonObject, GEBitmap,
   EEUIFMGStatusButton/Box, the scenario editor. A 4x button would draw 4x large. Needed: a logical size apart
   from the pixel size (the PNG `ee:surface` chunk today *becomes* the surface size, so a 4x image with it would
   be shrunk at load; it has to mean "layout size, keep the pixels"). Until then UI, buttons and backgrounds are
   upscaled but not deployed. Check first that the UI is drawn larger than 1:1 at high resolutions at all.
3. **Memory:** 2x by default, 4x only where it shows; `mNumMipLevelsToDrop` (texture quality) is the fallback;
   BC7 in GX later.

## 3. Models

Run through spandrel (the loader of chaiNNer) on PyTorch MPS. `tools/upscale/models.yaml` pins them:

* **4x-UltraSharp**: painted game art (skins, terrain). CC-BY-NC-SA.
* **4x_NMKD-Siax_200k**: painted art, softer.
* **4xNomos8kDAT**: transformer, slower, clean.
* **RealESRGAN x4plus** (baseline) and **x4plus_anime_6B** (flat UI art).
* Not for skins: diffusion upscalers (SUPIR, SDXL/Flux tile ControlNet) invent detail and are inconsistent
  between assets; candidates for the large background images only, with review.

## 4. Per-image processing (`upscale_image`)

1. Alpha: the colour under transparent pixels is bled from opaque neighbours (no halos); alpha is upscaled with
   the same model (or Lanczos, `--alpha lanczos`).
2. 8 source pixels of padding: wrapped for tileable sets, edge-replicated otherwise; cropped after.
3. Model at 4x in 256-pixel tiles with feathered overlap; `--scale 2` downsamples the 4x result (Lanczos).
4. Back-projection: the result, box-downscaled, is corrected to equal the original (low-frequency difference
   added back). Removes the models' colour / brightness drift without losing their detail. `--no-back-project`
   to compare.
5. Written with an `ee:upscale` text chunk naming the method.

Each run writes `upscale.json`: source sha256 per file, model sha256, parameters, per-unit notes (layout, seam
cost), so a tree is reproducible and a stale one is detectable.

## 4b. Shipping (decided 2026-11-05)

One optional pack, `Data/Textures-4x/`: same file names as `Data/Textures/`, partial (every category but
`exclude`; `upscale.py pack`). UI textures included: the engine keeps a pack texture's logical (layout) size apart
from its pixel size, so UI layout, hit-testing and pixel rects stay in original units. An in-game setting picks Original / 2x / 4x. 4x reads the pack's file when it exists, else the
original. 2x reads the same 4x file at half size (made at load time, e.g. through the mip-drop path), so 2x costs no
install space. Originals without a 4x file load as today.

## 5. Order of work

1. **Pilot** (now): a sample per category through every candidate model; before/after sheets
   (`upscale.py sheet`); pick a model per category into `CATEGORY_METHODS`.
2. Full pass of the 3D textures (skin, terrain, fx, shadow) at 2x as an overlay; in-game check
   (tools/harness/posix_game.py screenshots at fixed spots).
3. Engine: logical vs pixel size for 2D; then deploy ui / button / background.
4. Large backgrounds: optionally a diffusion upscaler, reviewed by hand.
5. Optional: BC7 in GX, a 4x tier, the textures inside the campaign `.ssa` packages.

The output is derived from retail data: private, like the data bundle. Model licences vary (several are
non-commercial).

## 6. Pilot results and the recipe (2026-11-05)

40 units (5 per category, `--sample 5 --seed 7`), all candidates at 4x on an M5 Pro (MPS): UltraSharp 57 s,
Siax 58 s, Real-ESRGAN 59 s, Real-ESRGAN anime 23 s, Nomos8k DAT 266 s, Lanczos 6 s.

* **Nomos8k DAT**: the most faithful; adds clean detail without inventing texture. Weakness: smooths fine regular
  patterns (the stripes of `fx_arrowt` disappear).
* **4x-UltraSharp**: crisp and punchy; invents grain and scratches, bright halos on hard edges (coins of
  `but_sellfood`), a garbled small face on `men_pilum_05t`.
* **Siax**: crunchy; turns `overgrowngrass` into a cross-hatch, which repeats visibly on tiling terrain. Dropped.
* **Real-ESRGAN x4plus**: plastic. **anime 6B**: flattens and rewrites painted art (invented cracks on `rock_1`),
  though it is the cleanest on flat glyphs. Dropped.
* Every model turns the noise of effects such as `fx_shieldbattery` into blotchy cracks: effects stay Lanczos.

**Recipe** (`FINAL_RECIPE` in upscale.py, `tools/upscale/full_pass.sh`): skin, terrain, button, ui, background
= 70% DAT + 30% UltraSharp per pixel (`blend`; equal to blending inside the pipeline, as back-projection is affine);
fx, shadow = Lanczos; exclude = copied. Chosen on a comparison page with a live mix slider; the 70/30 blend keeps
DAT's faithfulness and some of UltraSharp's bite.
