# Empire Earth: The Art of Conquest: models and textures, and their open forms

The game's 3D models (`.cem`, tag "SSMF") and textures (`.sst`, `.tga`): what the files hold, what the game reads
of them (with the reading code), and the open formats that replace them: glTF 2.0 for models, PNG for textures.
`tools/eemodel` converts in both directions; the game reads the open forms only (`GEOpenFormatsFile.cpp`; RUST_PLAN.md
D9): sections 2 and 3 describe the retail files the importers read, and the loaders that read them before D9. Sources: this repository's engine (`src/Low-Level Engine/Graphics Engine/`), the AoC
install's `Data\data.ssa` and the original game's `Data\data.ssa` (the AoC exe searches both, section 1), parsed end to
end by `eemodel.py check`.

Field classes, as in DATA_FORMATS.md: **read** (a reader exists; named after it), **unread** (loaded or skipped, never
used), **derived** (read, but always equal to a value computed from other fields).

---

## 1. Where the files are and how the game finds them

`EEShowSinglePlayerScreen` (Empire Earth.cpp) builds `gDataPath`: the AoC `Data\data.ssa`, the original game's
`Data\data.ssa` (the install path from the preferences), then the two `Data\` folders. `FSPath::FindFile` searches the
**folders first**, then the archives, each in that order. Models and textures are both found through it
(`gModelManager`, `gTextureManager` share `gDataPath`). That was the original; this port (branch nossa) searches
only its own `Data\` folder (and a `Data\data.ssa` when one is present), which `tools/open_data.py build` fills from
both installs in the order above, with every model as glTF and every texture as PNG. The table describes the retail
sources.

| Kind | Name the game asks for | Loader | AoC data.ssa | Original data.ssa | Loose |
|---|---|---|---|---|---|
| model | `<dbgraphics mFileName>.cem`, e.g. `models\air_catalina_11.cem` (`GEModelManager::LoadModel` adds `.cem`) | `GEModel::Load` | 136 (`models\*.cem`, all 2.0) | 730 (719 v2.0, 11 v1.3) | `Data\Models\amb_rock.cem` in both installs: a 24-byte text stub ("9E527E...") that is no model: the game fails to load it (no archive copy exists) |
| texture | `<name>.tga`, else `<name>.sst` (`GETextureManager::LoadTexture`; a name ending in `.sst` only the .sst) | `GETexture::Load` | 405 `.tga`, 22 `.sst` | 31 `.tga`, 2095 `.sst` | original `Data\Textures\`: 239 `.sst` |

Which texture a unit's model is drawn with comes from **dbgraphics** (`mTextureName`, `mTextureNames[]`), not from the
model: `EEROUnits` sets the dbgraphics texture on every material (`GEModel::SetTexture`), and a material's
`texture index` picks a frame of it. The texture names stored in a `.cem` are never read (2.2).

Other readers of `.tga` files go around the texture manager: `textures\ColorPalette.tga` (EETerrainColorTable),
`textures\Volcano.tga` (EEE3DVolcano), the mouse pointers (RTSMouse), scenario pictures (EEUIEScenarioIntroEvents),
campaign map backgrounds (EEUIBMapBackground). They read the `.tga` through `GESurface`. The first three look it up
through `GEFindTargaFile`, which serves `<name>.png` as the Targa when the path has one; the scenario and campaign
pictures come from scenario files and campaign archives, which are not converted.

The multiplayer checksum (`EEInitializeNetwork`) sums the loose `Data\Models\*.glb`, `*.gltf` and `*.bin` (the original
summed `*.cem`): models carry tag points and bounds the simulation uses.

---

## 2. `.cem` models ("SSMF")

### 2.1 Container

A file is a model tree: the root model, then each sub-model as a complete model (with its own header), depth first in
file order (`LoadCurrentVersion` / `LoadLegacyVersion` call `GEModel::Load` once per sub-model). Little-endian.
`UString` = `u32 length` + that many bytes; the game keeps the bytes up to the first NUL (`UString::Load`: `strlen`);
`UString::Save` writes length = characters + 1 and the NUL. Floats are IEEE single.

Header (`GEModel::LoadHeaderChunk`): `char[4] "SSMF"`, `u16 major`, `u16 minor`. Major 1 or 2, minor 0..3, else the
load fails. Major < 2 goes to the legacy loader (2.4). `GEModel::Save` writes 2.0 only.

The retail data: 855 files are version 2.0, 11 are 1.3 (original game: `air_balloon_09`, `amb_foundation1..3`,
`amb_streetlamp_12`, `amb_walrus_00`, `bld_towncenter_10`, `fx_debrisflat`, `gun_80mmaa_10`, `men_priest_11`,
`nav_dardocruiser_11`; all referenced by AoC's dbgraphics). Trees are at most two deep (root + sub-models). 3 files
carry bytes after the tree (`bld_tankfactory_10`, `bld_temple_02`, `nav_bismarck_10`; unread). Every string is in the
form `UString::Save` writes (no bytes after the NUL).

### 2.2 Version 2.0 (`GEModel Load.cpp` and the writer `GEModel Save.cpp` before D9)

| Chunk | Field | Type | Class | Readers (what the game does with it) |
|---|---|---|---|---|
| counts (`LoadCountsChunk`) | polygonCount | u32 > 0 | read | size of the LOD 0 polygon array (`AllocateInternalData`), `GetPolygonCount`; always equals LOD 0's own count in the data |
| | vertexCount | u32 > 0 | read | vertices per frame |
| | tagPointCount | u32 | read | |
| | materialCount | u32 > 0 | read | |
| | frameCount | u32 > 0 | read | |
| | subModelCount | u32 | read | |
| | lodCount | u32 | read | number of polygon arrays (retail: 1 or 10) |
| misc (`LoadMiscellaneousChunk`) | name | UString | read | the sub-model's role by prefix: `turret_` (EEGraphicObject), `orbital_cw_<speed>_`/`orbital_ccw_` (EEOrbital), the wheel name (EEWheel); roots are "Scene Root" |
| | center | 3 f32 | read | `GetCenterPoint`: camera, picking (EEUnitPicker, GEPhysicalModel), effects (EEE3DVolcano, EEE3DDamageVent); close to but not always equal to the box centre `ComputeBoundingVolumes` gives (393 of 1212 models differ in the last bits, 2 by more than 1e-4) |
| polygons (`LoadPolygonsChunk`) | per LOD: count, count × 3 u32 vertex indices | | read | `DX7Rasterizer::DrawModelHierarchy`: the root draws the LOD its on-screen size picks, sub-models LOD 0. **An index is relative to its material's first vertex** (the draw passes the material's vertex range as the vertex buffer) |
| materials (`LoadMaterialsChunk`) | name | UString | read | compared with "player color": those materials take the player's colour (EEGraphicObject::CollectTagPoints, `GEModel LOD.cpp`); other names are labels |
| | textureIndex | u32 | read | the frame of the unit's (dbgraphics) texture the material shows (EEROUnits; past the frame count: frame 0) |
| | per LOD: polygonStart, polygonCount | 2 u32 | read | the material's range of the LOD's polygon array (draw, picking) |
| | vertexStart, vertexCount | 2 u32 | read | the material's vertex range (draw, picking); ranges may overlap (92 models) |
| | textureFileName | UString | **unread** | `LoadTexturesForScreen` would load it, but nothing calls it; units use dbgraphics' texture |
| tag names (`LoadTagPointsChunk`) | name | UString × tagPointCount | read | by prefix: the 0x48 `gTagPointKindNames` (effects, lights `light_r_g_b_...`, fire, smoke...), `weapon_mount`, `attack`, `status bar` (EEGraphicObject::CollectTagPoints) |
| frames (`LoadFrameChunk`), per frame | radius | f32 | read | `GetBoundingRadius`: culling, picking, effects; approximately the `ComputeBoundingVolumes` radius |
| | vertices | vertexCount × (3 f32 position, 3 f32 normal, 2 f32 u v) | read | drawn; between two frames positions and normals blend by the fractional frame position (`CopyAndSmooth*Vertices`), texture coordinates come from the current frame |
| | tag points | tagPointCount × 3 f32 | read | positions in the model's own space (the model's transform applies) |
| | transform | 16 f32, `mMatrix[4][4]` row-major, column vectors (translation in column 3; bottom row 0 0 0 1 in all data) | read | the model's transform relative to its parent: world = parent world × this (`GEModel::PrepareTransforms`, `DrawModelHierarchy`, `CollectTagPoints`); per frame, no blending |
| | bounding box | 6 f32: min xyz, max xyz | derived | `GetBoundingBox` (camera, picking, culling); equal in every frame of every file to the box of that frame's vertices (`ComputeBoundingBoxes`, its else-if order included) |

Retail facts that shape the open form: LOD counts 1 (173 models) or 10 (1039); 306 models have 10+ frames; texture
coordinates never differ between frames, normals do (288 models); frame transforms differ between frames in 66
sub-models (rotating parts); every material has polygons in LOD 0, 148 have none in some higher LOD; the materials'
polygon ranges tile each LOD (in 17 LODs not in material order); one model has NaN normals (`bld_market_10`).

### 2.3 Shared frames (runtime, not in the file)

`LoadModel`'s map argument (`ioTextures`, really "shared frames", from dbgraphics' frame sharing) makes some frames
reuse another frame's vertex array; the file is read the same way. The open form needs nothing for it.

### 2.4 Versions 1.0-1.3 (`GEModel Legacy.cpp`, retired)

Read by `LoadLegacyVersion`, then turned into the 2.0 model by `ConvertLegacyModel` (`tools/eemodel/legacy.py` is a
step-by-step port; the converter writes the result as 2.0):

* counts: frameCount, materialCount, pointCount, polygonCount, groupCount, vertexCount, (not 1.0) tagPointCount,
  (1.2+) subModelCount;
* misc: name, center, a flag (**unread**), (1.3) one u32 per point (**unread**);
* polygons: per corner a u32 vertex index, u v, then colour (3 f32), alpha, ambient, diffuse, specular (**unread**);
* polygon groups: name, count, indices; only groups named "player color" are kept;
* materials: polygon count + indices, a bool, if set the texture file name and a u32 texture index (**unread**);
* vertices: u32 point index, f32 (**unread**);
* (not 1.0) tag point names;
* frames: radius, the points, one normal per vertex (before 1.3: 3 f32; 1.3: a u16 index into
  `GEVector::QuantizedVectorArray`, a 41 × 246 table of unit vectors on the cube faces), (before 1.3) one normal per
  polygon (**unread**), (not 1.0) tag points, (1.2+) transform, (1.3) bounding box.

The conversion splits vertices whose corners disagree on the texture coordinate, groups polygons by material (a
material whose polygons are only partly "player color" becomes two), copies a vertex used by two materials into both,
reorders polygons and vertices by material and makes polygon indices material-relative. Its quirk is kept: in a
polygon whose corners repeat a vertex, only the first matching corner is renumbered. `legacy.check_geometry` checks
the result independently (every converted corner carries the legacy corner's position, normal and texture
coordinate in every frame): all 11 retail 1.3 models pass. The quantized normal table is computed with float32
steps (the x64 build's arithmetic; the i686 build's x87 intermediates can differ in the last bit).

---

## 3. Textures: `.sst` and `.tga` (`GETexture.cpp` before D9, `GESurface.cpp`)

Before D9, `GETexture::Load` read a 14-byte header unless the file name ends in `.tga`:

| Field | Type | Class | Reader |
|---|---|---|---|
| isCompressed | u8 | **unread** | (0 in all data) |
| mipLevelCount | u8 | **unread** | read only by frame format 1, which no caller uses (every `GETextureDescription` asks for format 0); retail: 1, or 3/4/7/8 in 44 single-frame files |
| frameCount | u32 | read | number of frames (separate textures, `GetFrameTexture`; a material's texture index picks one); retail 1..16 |
| width, height | 2 u32 | read | the surface size; equals frame 0's image size except in `iwo_jimat.sst` (256 × 256 header, 64 × 64 image) |

Then per frame a `u8` image type (0 Targa, 1 BMP (fails), 2 JPEG, 3 "JPG2"; every retail frame is 0) and the image
(`GESurface::Load`). In format 0 only the first image of a frame is read; stored mip levels after it are **unread**
(the mip chain is filtered from level 0).

A `.tga` file is one such Targa (the header is made from its bytes 12-15). `GESurface::LoadTGA` reads the 18-byte
header, skips the ID, reads `depth/8 × width × height` bytes and stores them in the order the descriptor's bit 5
(top-down) and bit 4 (right-to-left) give. Read: width, height, pixel depth (24: opaque; 32: alpha, unless the
surface has no alpha), descriptor bits 4/5, the pixels. **Unread**: the image type (not checked: everything is
treated as uncompressed true colour), colour map fields (set in 3 files), origin, the ID bytes, anything after the
pixels (a TGA 2.0 footer in 299 files). Retail: all type 2, 24 or 32 bits, descriptors 0x00, 0x08, 0x20, 0x24.

---

## 4. Models as glTF 2.0 (`tools/eemodel/gltf.py`)

### 4.1 Mapping

* `models\<name>.cem` becomes `Models\<name>.gltf` + `<name>.bin` (or one `<name>.glb`); the game takes either.
* Scene: node 0 "EE axes (Z up)", a rotation turning the game's Z-up axes into glTF's Y-up (the game's data is
  right-handed: 164,152 of 165,596 triangles wind counter-clockwise about their normals), parent of the root model.
* One node per model, children: its sub-models (nodes with a mesh), then its tag points (nodes without a mesh, named
  by the tag, translation = the frame 0 position). Names are the game's strings (CP-1252 as in tools/eedata).
* Node transform: a model's frame transform as `matrix` when it is the same in every frame and the node is not
  animated; otherwise translation/rotation/scale (glTF forbids a matrix on an animated node) with the exact matrices
  in EE_model.
* Mesh: one primitive per material, in material order. Its attributes are the material's vertex range of frame 0
  (POSITION, NORMAL, TEXCOORD_0 accessors into one interleaved buffer view, 32-byte stride: the file's own vertex
  layout), its indices the material's LOD 0 range (material-relative, as in the file, which is exactly glTF's
  meaning). Overlapping vertex ranges share the buffer.
* Frames 1..n-1: morph targets (POSITION and NORMAL deltas to frame 0); `mesh.extras.targetNames` "frame i"; an
  animation "frames" with key i at time i/10 s (weights one-hot, LINEAR: glTF's blend is the game's). Moving tag
  points and frame transforms: translation/rotation/scale channels (STEP). A delta is chosen so that
  float32(frame 0 + delta) is the frame's value; the 290 sub-models where no float32 delta exists for some values get
  them in `frameFixes`.
* Material: name (the game reads "player color"), `extras.ee_textureIndex` when not 0, a base colour texture whose
  image `name` is the model's own (unread) texture name and whose `uri` is the PNG of the texture the game draws the
  model with (dbgraphics; the frame the texture index picks: `../Textures/<texture>@<i>.png`), so viewers show the
  game's look.

### 4.2 The EE_model node extension (declared in `extensionsUsed`, not required)

Everything the game reads that glTF's core cannot hold exactly, per model node:

| Key | Value | Without it (an editor dropped the extension) |
|---|---|---|
| `center` | 3 numbers | computed as `ComputeBoundingVolumes` does |
| `radii` | one number per frame | computed as `ComputeBoundingVolumes` does |
| `vertexStarts` | each material's first vertex | materials' vertices one after the other (overlaps duplicated) |
| `lod0Ranges` | `[start, count]` per material, when not consecutive in material order | consecutive |
| `polygonCount` | when it differs from LOD 0's triangle count (never in retail data) | LOD 0's count |
| `lods` | per LOD 1..: `{"ranges": [[start, count], ...] per material, "indices": accessor}` | one LOD |
| `transforms` | the exact frame matrices (glTF column-major) of an animated node | sampled from the node's animation |
| `frameFixes` | per frame 1..: null or a sparse float accessor over the frame's 6 × vertexCount position+normal values: the exact values | the delta's value |

JSON numbers are the exact decimal value of the float32 (`repr` of the widened double), so a float32 or float64
reader gets the same bits back.

### 4.3 Import rules (gltf.py `import_`; GEOpenFormats.cpp implements the same)

The root model is the first node with a mesh, depth first from the scene's roots (no scene: the parentless nodes).
The transform above it must be the Z-up wrapper; any other signed axis permutation (e.g. no wrapper: a Y-up file from
an editor) is multiplied into the root's frame transforms; anything else is an error. Sub-models are child nodes with
a mesh, tag points child nodes without one (in child order; the game's own save sorts them by name, 4 retail files
are not sorted). A name loses a trailing `.NNN` (three digits: Blender's duplicate suffix; no retail name has one).
Primitives must be triangle lists with the same number of morph targets; missing NORMAL/TEXCOORD_0 are zero. Frame
f's values are float32(base + delta), then `frameFixes`. Each EE_model entry is used only when it is well-formed and
consistent (the vertex starts reproduce the same vertices where ranges overlap; `lod0Ranges` counts equal the
primitives'; every LOD range lies inside its index array and indexes inside its material's vertices; all LODs or
none); a malformed `frameFixes` or `ee_textureIndex` is an error. Tag positions and transforms of frame f are sampled
at time float32(f/10): STEP holds a key, LINEAR interpolates in double precision, CUBICSPLINE uses its key values.
Bounding boxes are always computed. The texture name is the image's `name`, else its URI's file stem.

---

## 5. Textures as PNG (`tools/eemodel/sst.py`, `png.py`)

`<name>.png` is frame 0, `<name>@<i>.png` frame i: 8-bit RGB for a 24-bit image, RGBA for a 32-bit one, top row
first. A surface size other than frame 0's image size goes into frame 0's tEXt chunk `ee:surface` ("256x256"; only
`iwo_jimat`). Any PNG an editor writes is accepted (grey, grey+alpha, palette, tRNS, 16-bit, interlaced): images with
an alpha channel or tRNS become 32-bit, others 24-bit; 16-bit samples keep their high byte (libpng's and stb_image's
`strip_16`). The game is given a `.sst`: the header (not compressed, 1 mip level, the frame count, the surface size)
and per frame type 0 and a Targa (no ID, bottom-up, descriptor 0 or 8). Unread data is not carried: stored mip
levels, Targa footers, IDs, colour map and origin fields, the compressed flag.

---

## 6. The game reading the open forms (`GEOpenFormatsFile.cpp`)

Since D9 (RUST_PLAN.md) the game reads glTF models and PNG textures only; no `.cem`, `.sst` or `.tga` is looked up
through the model and texture managers, and their readers and writers are gone (`GEModel::Load`/`Save`, the
version-1 conversion, `GETexture`'s `.sst`/`.tga` reading and `Save`).

* `GEModelManager::LoadModel("models\x", frames)` asks `GEFindModelFile` for `models\x.glb`, else `models\x.gltf`
  (external buffers next to the `.gltf`, on the path). Rust (`ee_openformats`, `ee_of_gltf_to_model`) decodes it
  straight into the model the `.cem` reader produced (counts, names, polygon lists, material ranges, frames with their
  vertices, tag points, transforms and boxes, sub-models; the frames the caller shares go into the last frame's arrays
  as `LoadFrameChunk` put them), and `GEModel::StoreLoaded` stores it as before. A model the reader refused (no
  polygons, vertices, materials or frames) still fails, and a refused sub-model makes every later one in file order
  fail as the `.cem` reader's rewind did. The cache key is the name as given (`models\x`; it was `models\x.cem`).
* `GETextureManager::LoadTexture`, `ReloadTexture` and `RefreshAllTextures` ask `GEFindTextureFile` for `<stem>.png`
  and its frames `<stem>@1.png`, ...; Rust (`ee_of_png_to_texture`) decodes them into the surface size and each frame's
  RGB or RGBA pixels, and `GETexture::Load` stores them where the Targa reader stored a `.sst` frame (same surfaces,
  MIP levels, dropped levels, frame formats and pixel packing). The stem and cache key is the name as given, or
  without its last extension when it contains `.sst` (`textures\x` and `textures\x.sst` are one texture, as they
  were one cached `textures\x.sst` before); the texture pack maps the stem (`textures-4x\x`).
* `GEFindTargaFile` (pictures read as Targa data around the texture manager) still serves `<name>.png` as a Targa
  (`ee_of_png_to_tga`), else the `.tga` file.
* A file that does not convert stops the game with its message (`ESVerify`). Nothing in a saved game names a model or
  texture file: the keys changed without a save format change.

---

## 7. Gates (`eemodel.py check`, `eemodel.py cppgate`)

`check` (AoC and original data, loose files and both archives; about 6 minutes):

* models: 866 read (855 v2.0: reader/writer byte-identical to the file; 11 v1.3: converted, geometry check passes;
  2 copies of the `amb_rock` stub refused as the game refuses them); 866/866 glTF pass the structural validator
  (`validate.py`); 866/866 cem -> glTF -> cem byte-identical to the canonical 2.0 form (= the file for 852 of the 855
  v2.0 files; the 3 others lose only their unread trailing bytes), also through `.glb`. Without EE_model (informative)
  the game-visible differences are the expected ones (LODs, centre, radii, overlapping vertex ranges, frame fixes,
  frame transforms of 28 models approximated by TRS).
* textures: 2792 (2356 `.sst`, 436 `.tga`): PNG -> the game's view equal (surface size, every frame's pixels and depth)
  2792/2792; PNG -> .sst -> PNG byte-identical 2792/2792; the canonical .sst/.tga is the file itself for 2446 (the rest
  differ only in unread bytes: mip levels, footers, descriptors).
* `render` draws a model through the game's transform composition (from the .cem) and through glTF semantics (from
  the .gltf): identical pictures for static frames, the same picture up to sub-pixel differences for animated frames
  (TRS animation instead of the exact matrices).

`cppgate` (about 10 minutes; `tools/eemodel/cppgate.py`): what the game stores from the open tree (default the main
checkout's `build/open_data/tree`, read only) equals what it stored before D9 (section 6):

* the store gate (`storegate.py`, about 1 minute after its build): a console driver linked from a game build's own
  objects (`python -I build.py` first; the renderer is memory objects, `storegate/fakes.h`) loads every model through
  `GEModelManager::LoadModel` with three shared-frame maps (861 x 3) and every texture through
  `GETextureManager::LoadTexture` with seven descriptions (2,505 x 7: frame formats 0 and 1, 16 and 32 bits, alpha or
  not, MIP maps, dropped levels, the texture pack's path), then reloads and refreshes them, and digests the stored
  `GEModel` (every array, material and sub-model) and `GETexture` (every frame's surfaces and pixels): 25,140 lines
  equal to `storegate/digests.json`, recorded from the loaders before D9 (glTF -> `.cem` bytes -> `GEModel::Load`,
  PNG -> `.sst` bytes -> `GETexture::Load`), which the native loaders matched line for line;
* the decoder gate: the game's decoder (`ee_openformats`, its `eemodel_gate` program) over the tree's 861 models,
  the same without EE_model, 34 as `.glb` and 2,568 edited models (TRS instead of matrices, `.NNN` names, no wrapper,
  a rotated wrapper, broken EE_model data, animation-sampled transforms with STEP and LINEAR keys between frames: 1929
  load, 639 refused), its 2,505 textures and 275 PNG variants (225 load, 50 refused: bad `ee:surface`, truncated
  files); 7,104 cases, each loaded or refused as the Python converter (`gltf.import_`, `sst.from_png`) decides, and
  its digest (the stored model with three shared-frame maps, the texture's frames, or the refusal message) equal to
  `cppgate/golden_store.json`, recorded by a run that also decoded every case the old way (7,104 of 7,104 equal).

`--subset N` runs every Nth model and texture (both parts), `--no-store` skips the store gate, `--build` names the
game build. The recorded digests apply to that tree (its `open_data.json` hash) and the current case generator: a
new tree or a deliberate change of what is stored needs `storegate.py --record` and `cppgate --record-golden FILE`,
checked by hand against what changed.
