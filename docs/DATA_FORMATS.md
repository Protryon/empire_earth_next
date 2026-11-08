# Empire Earth: The Art of Conquest: inventory of the data file formats

Research notes for converting the game's binary data tables to text (YAML/CSV). Sources: the clang port at
`C:\Users\max\Desktop\empire_earth_next` (loader functions, record structs pinned by `static_assert`), and the AoC
install's data (`Data\data.ssa`, unpacked and parsed with throwaway scripts in
`tools/datafmt/`). Every
table below was parsed end to end with the layout from the source: **all 39 db files are consumed to exactly their
full length**, so the containers and record sizes are confirmed. Field *meanings* are only as good as the source names. The
"class" column below says how far each field is decoded.

Field classes used in the tables (2026-11-04: every record member is named from its readers; a compile census,
`tools/eedata/eedata.py census`, lists every read of every member, and section 8.4 lists the evidence):

* **named** (shown as "named" in the tables, "read" in the summary): a member the game reads. Its name comes from its
  readers (what it is compared with, what it scales, which function consumes it). These are the text's fields.
* **label**: the row's name where no code reads it (`mLabel`). The text keeps it as `label` so that rows can be
  told apart; it is written back to the same bytes and has no effect on the game.
* **unused**: a member nothing reads (`mUnused*`, and the remaining unread gaps), including members a loader only
  copies into a class member that nothing reads (dead copies, section 8.4). Not in the text; the importer writes
  zero or the spec's default.
* **pad**: alignment padding (some explicit in the original source, now left to the compiler). Zero.

The "observed" column summarises the AoC retail values: the range and number of distinct values, and how many
records hold the `0xCC` fill (see section 2.4).

---

## 1. Summary table

| # | File (inside data.ssa) | Loader | Record type | Record bytes | AoC rows (real) | File bytes | read / label / unused / padding % | Suggested |
|---|---|---|---|---|---|---|---|---|
| 3.1 | `db\dbobjects.dat` | `EEUnitClass::CreateUnitClasses` 0x6358b1 -> `EEDatabase::Load` 0x4d60ae / `EEDbObject::Load` 0x4d689b | `EEUnitClassRecord` | 1960 | 848 (738) | 1,662,084 | 80 / 0 / 18 / 2 | YAML |
| 3.2 | `db\dbgraphics.dat` | `EEGraphicObject::LoadTable` 0x4d7833 + `EELoadGraphicObjectRecord` 0x4d7b37 | `EEGraphicObjectRecord` | 716 | 991 (968) | 936,188 | 68 / 14 / 7 / 12 | YAML |
| 3.3 | `db\dbsounds.dat` | `EESoundObject::LoadTable` 0x4da680 | `EESoundRecord` | 132 | 1087 (872) | 166,734 | 10 / 76 / 12 / 2 | CSV |
| 3.4 | `db\dbgfxeffects.dat` | `EEGraphicEffect::LoadTable` 0x4d6e9e, ctor 0x4d6ceb | `EEGraphicEffectRecord` | 232 | 292 (288) | 67,748 | 54 / 43 / 2 / 1 | CSV |
| 3.5 | `db\dbmusic.dat` | `EEMusicObject::LoadTable` 0x4d9f6c | `EEMusicRecord` | 196 | 9 (9) | 1,768 | 42 / 51 / 7 / 0 | CSV |
| 3.6 | `db\dbtechtree.dat` | `EETechTree::Load` 0x5cbdf0 -> `EETechTreeEpoch::Load` 0x5ce10e -> `EETechTreeNode::Load` 0x5cf370 | `EETechTreeRecord` | 268 | 4494 (793) | 1,226,932 | 50 / 39 / 11 / 0 | YAML |
| 3.7 | `db\dbevents.dat` | `EEEvent::LoadEvents` 0x5ca63b | `inline` | var | 1500 (317) | 166,680 | 0 / 96 / 4 / 0 | YAML |
| 3.8 | `db\dbeffects.dat` | same `EEEvent::LoadEvents` | `EEEventEffect` | 88 | 2000 (1193) | 176,000 | 65 / 0 / 35 / 0 | YAML (with events) or CSV |
| 3.9 | `db\dbbuttons.dat` | `EEButtonObject::Load` 0x4d5500 | `EEButtonRecord` | 224 | 1549 (1138) | 346,980 | 55 / 45 / 0 / 0 | CSV |
| 3.10 | `db\dbfamily.dat` | `EETerrainTypeTable::LoadDamageTables` 0x4dbf11 | `EEDamageTableRecord` | 380 | 60 (60) | 22,804 | 72 / 26 / 2 / 0 | CSV (matrix) |
| 3.11 | `db\dbweapontohit.dat` | `EEToHitTable::Load` 0x4dcaad | `EEToHitRecord` | 132 | 6 (6) | 796 | 18 / 76 / 6 / 0 | CSV (matrix) |
| 3.12 | `db\dbworld.dat` | `EEWorldTable::Load` 0x4dcb79 | `EEWorldTableEntry` | 28 | 95 (95) | 2,664 | 57 / 0 / 43 / 0 | CSV |
| 3.13 | `db\dbunitbehavior.dat` | `EEUnitBehavior::Load` 0x4dc973 | `EEUnitBehaviorRecord` | 124 | 8 (8) | 996 | 9 / 81 / 10 / 0 | CSV |
| 3.14 | `db\dbstartingresources.dat` | `EEStartingResources::Load` 0x4daa05 | `EEStartingResourcesRecord` | 132 | 6 (6) | 796 | 21 / 76 / 3 / 0 | CSV |
| 3.15 | `db\dbanimals.dat` | `EEAnimal::Load` 0x4d4a5d | `EEAnimalRecord` | 272 | 14 (14) | 3,812 | 38 / 37 / 22 / 4 | CSV |
| 3.16 | `db\dbcolortable.dat` | `EEColorTable::Load` 0x4d5d91 | `EEColorTableRecord` | 168 | 69 (69) | 11,596 | 21 / 60 / 19 / 0 | CSV |
| 3.17 | `db\dbaiunittargeting.dat` | `EEAIUnitTargetingTable::Load` 0x4d47a1 | `EEAIUnitTargetingRecord` | 160 | 3 (3) | 484 | 22 / 62 / 15 / 0 | CSV |
| 3.18 | `db\dbterrain.dat` | `EETerrainTable::Load` 0x4db2ce | `EETerrainRecord` | 204 | 93 (77) | 21,167 | 92 / 0 / 7 / 0 | CSV |
| 3.19 | `db\dbterraintype.dat` | `EETerrainTypeTable::Load` 0x4dbd2e | `EETerrainTypeRecord` | 128 | 68 (68) | 8,708 | 13 / 78 / 6 / 2 | CSV |
| 3.20 | `db\dbcliffterrain.dat` | `EECliffTerrainTable::Load` 0x4dbb22 | `EECliffTerrainRecord` | 124 | 3 (3) | 376 | 94 / 0 / 4 / 2 | CSV |
| 3.21 | `db\dbcivpowers.dat` | `EECivPowersManager::Load` 0x683124 | `EECivPower` | 152 | 23 (23) | 3,500 | 21 / 66 / 13 / 0 | CSV |
| 3.22 | `db\dbcivilization.dat` | `EECivilizationManager::Load` 0x681514 | `EECivilizationCategoryEntry` | 600 | 29 (27) | 17,404 | 83 / 17 / 0 / 0 | YAML or CSV (long format) |
| 3.23 | `db\dbpremadecivs.dat` | `EECivilizationManager::LoadPremadeCivilizations` 0x681672 | `EECivilizationEntry` | 120 | 23 (23) | 2,764 | 97 / 0 / 3 / 0 | CSV |
| 3.24 | `db\dbcalamity.dat` | `EECalamityManager::LoadCalamityClasses` 0x4ae070 | `EECalamityData` | 236 | 36 (36) | 8,500 | 42 / 42 / 14 / 2 | CSV |
| 3.25 | `db\dbunitset.dat` | `EEUnitSetManager::Load` 0x536fa9 | `EEUnitSetRecord` | 184 | 173 (170) | 31,836 | 98 / 0 / 0 / 2 | CSV (list columns) or YAML |
| 3.26 | `db\dbareaeffect.dat` | `EEAEManager::Load` 0x67b079 | `EEAreaEffectData` | 188 | 50 (46) | 9,404 | 87 / 0 / 11 / 2 | CSV |
| 3.27 | `db\dbupgrade.dat` | `EEUCUpgradeTable::Load` 0x4dc6f0 | `EEUCUpgradeRecord` | 240 | 76 (76) | 18,244 | 52 / 42 / 5 / 1 | CSV |
| 3.28 | `db\dbcpbehavior.dat` | `EECPBehavior::Load` 0x4d5f97 | `EECPBehavior` | 56 | 126 (126) | 7,060 | 80 / 0 / 14 / 5 | CSV |
| 3.29 | `db\dbgamevariant.dat` | `EEGameVariant::Load` 0x4d693c | `EEGameVariantData` | 276 | 3 (3) | 832 | 45 / 36 / 17 / 1 | CSV |
| 3.30 | `db\dbambientsounds.dat` | `EEAmbientSoundManager::Load` 0x525012 | `EEAmbientSoundRecord` | 152 | 28 (28) | 4,260 | 21 / 66 / 13 / 0 | CSV |
| 3.31 | `db\dbuifonts.dat` | `UIFontTable::Create` 0x1007011f | `UIDbDataFonts` | 128 | 61 (57) | 7,812 | 98 / 0 / 0 / 2 | CSV |
| 3.32 | `db\dbuiforms.dat` | `UIForm::Create` 0x100716c0 | `UIDbDataForms` | 268 | 40 (38) | 10,724 | 59 / 37 / 3 / 0 | CSV |
| 3.33 | `db\dbuiback.dat` | `UIBackgroundTable::Create` 0x1005c631 | `UIDbDataBackgrounds` | 308 | 167 (92) | 51,440 | 99 / 0 / 0 / 1 | CSV |
| 3.34 | `db\dbuicontrols.dat` | `UIControlsTable::Create` 0x10067ee6, record ctor 0x1006796f | `UIDbControl` | 860 | 3401 (1372) | 2,924,864 | 84 / 12 / 4 / 0 | CSV (wide) or YAML by form |
| 3.35 | `db\dbuicontrolevents.dat` | `UIEvent::CreateControlEvents` 0x1006f398 | `UIDbDataEvents` | 116 | 252 (234) | 29,236 | 10 / 86 / 3 / 0 | CSV |
| 3.36 | `db\dbuiformevents.dat` | `UIEvent::CreateFormEvents` 0x1006f42a | `UIDbDataEvents` | 116 | 21 (21) | 2,440 | 10 / 86 / 3 / 0 | CSV |
| 3.37 | `db\dbuihotkey.dat` | `UIHotKey::CreateHotKeys` 0x10076fa8 | `UIDbDataHotKeys` | 124 | 32 (32) | 3,972 | 16 / 81 / 3 / 0 | CSV |

Counts are AoC retail (`data.ssa` Timestamp 4035612). The % columns are bytes per record by field class (section header). For dbevents the % is per row structure. "real" = rows that are not hole/filler rows.

Not loaded although they are present or referenced:

* `db\dbterraingraytextures.dat` is in `data.ssa` (59 rows), but no AoC code opens it. Only the key
  `kDbTerrainGrayTexturesFile` exists. Its layout is decoded from the data in section 3.39.
* `db\dbrandommap.dat` is "loaded" by `EERandomMapTable::Load` (0x4da47b), but the loader frames it wrongly (see
  3.38). It reads the first dword as a count of 0x84-byte records and throws every record away. The file's real
  layout is decoded from the data.
* `db\dbterrainColor.dat` and `db\dbfonts.dat` are keys only. Neither file exists. Terrain colours come from the
  pixels of `textures\ColorPalette.tga` (`EETerrainColorTable::Load` 0x4daec4, 256 entries, 16 per row). The UI fonts
  come from `DB\dbuifonts.dat`.
* The base game's archive also has `db\converteroutput.log`, `db\converterverification.log` and `db\copy of
  dbgamevariant.dat`. These are left over from SSS's database-to-binary converter. The logs name every source table
  (section 2.6).

---

## 2. Containers and common conventions

### 2.1 Where the files live and how they are found

* All 39 `db\*.dat` files ship **only inside `Data\data.ssa`**. The install's `Data\db\` folder is empty.
* `gDataPath` is an `FSPath`: a list of directories plus archives. `FSPath::FindFile` searches **the directories
  first** (`FSPath.cpp` header comment), so a loose `Data\db\dbobjects.dat` overrides the copy in the archive. A
  converter can therefore write loose files and never needs to rebuild `data.ssa`.
* **This port** (branch nossa) reads one loose `Data` folder in the game's own directory: `tools/open_data.py build`
  merges both installs' archives and loose data into it (the original's search order; the tables as text, models as
  glTF, textures as PNG). The original game's install is no longer on the path; a `Data\data.ssa` is mounted only
  when present. With no archive mounted the checksum below gets no `Timestamp` (0): peers with the same tree agree.
* Multiplayer consistency: `Empire Earth.cpp` (around line 1067) sums `EEGetFileChecksum` over every loose
  `Data\Db\*.dat`, `Data\Unit AI Scripts\*.tai` and `Data\Models\*.cem` (file contents, sorted by name). For an
  archive it adds only its `Timestamp` tag (`data.ssa`: "4035612"). The sum becomes `gBinaryChecksum` and the
  `.%04d` suffix of the game version string. So changed db files change the version suffix, and peers must have the
  same files.
* **Texture packs** (port): an optional `Data\Textures-4x\` next to `Data\Textures\` holds upscaled textures with
  the same names and format (PNG) at 4x the original's pixel size. It may hold any subset; a missing folder or file
  means the original is used, with no error. config.yaml `graphics: texture_resolution` (0, 2 or 4; the settings' video
  tab "Textures: Original / 2x Upscale / 4x Upscale") picks the set: 0 loads only `Data\Textures`; 4 loads
  `textures-4x\<name>` when it is on the path, else `textures\<name>`; 2 loads the same pack file with one more MIP
  level dropped, so the GPU surface is half its size (a 2x2 box filter, `DX7Surface::CopyWithFilter`). The texture
  quality setting ("MIP Levels to Drop") stacks on top. Every texture the texture manager loads may come from the
  pack: world skins, terrain and effects, UI pieces, buttons and background tiles
  (`GETextureDescription::mUseTexturePack`, true by default). The mouse pointers come from it too
  (`RTSMouse::LoadPointer`). Pictures read as data (`textures\ColorPalette`, `WaterPalette`, `Volcano`) never do.
  They are read around the texture manager, and the pack lookup also refuses them by name.
  A pack surface has two sizes. Its **logical** size is the original's (`GESurface::GetWidth`/`GetHeight`,
  `GETexture::GetWidth`, ...), and every layout, source rectangle, sprite cell, hot spot and UV the game computes uses
  it, so the 2D code is unchanged. Its **pixel** size is 4x (2x at resolution 2): `GESurface::GetPixelWidth`/
  `GetPixelHeight`, `GetLogicalShift`. The conversion happens at the bottom. `DX7Surface::CopyTo`, `CopyWithFilter` and
  `Clear` scale logical rectangles to each surface's pixels, and `Lock`/`GetBytesPerRow` address pixels.
  `GETexture::GetNumMipLevelsToDrop` stays the caller's drop, because the 2D code rebuilds the original size as
  `GetWidth() << drop`. A pack texture always gets MIP levels, and a "no MIP filter" stage becomes point MIP
  filtering, so art drawn smaller than its pixels (most UI at window size) is minified from a box-filtered level,
  not point-sampled from the big one. Grid backgrounds keep their clamp addressing (no seams).
  The cache is keyed by name, so a texture's first load decides the file for every user of that name.
  The setting applies to textures loaded after it changes. Textures already cached keep theirs until they are
  released, so the settings screen says to restart. `GETextureManager::SetTexturePack`, `EESetTextureResolution`
  (EEPreferences.cpp); `EE_TEXTURE_PACK_LOG=1` prints every pack texture and pointer loaded with both sizes.

### 2.1a The settings file `config.yaml` (port)

The game's settings live in `config.yaml` in the data root (`EE_CONFIG` names another file; a read-only data root
moves it to the user's configuration directory). It replaces the original's registry keys and this port's earlier
`config.ini`; an old `config.ini` is not read. The format is a YAML subset (nested blocks of `key: value`, two-space
indents; strings, integers, `true`/`false`), specified in `include/platform/PlatConfig.h`. The game writes every
missing value with its default on start and rewrites the whole file on every change, so comments are lost. The keys
are the rows of `EESettingRows` (`src/Empire Earth/Empire Earth/EEPreferences.cpp`):

| block | keys |
|---|---|
| `player` | `name`, `computer_difficulty` (campaigns and the lobby's computer players) |
| `display` | `screen`, `window_mode` (0 full screen, 1 windowed), `resolution: {width, height}` (the game size), `ui_scale` (percent, 0 Auto), `render_resolution` (50-100 percent), `frame_rate_limit` (0 v-sync, -1 none), `brightness` (percent) |
| `graphics` | `texture_resolution` (0, 2, 4), `texture_filtering` (0 bilinear, 1 trilinear, 2 anisotropic), `trilinear_filter`, `mip_levels_to_drop: {textures, terrain}`, `model_detail`, `effect_detail` (0-2), `unit_lighting`, `animation_smoothing`, `shadows`, `clouds`, `battlefield_ambients`, `show_blood` |
| `audio` | `device` (empty: the system's default), `master_volume`, `sound_volume`, `music: {enabled, volume}`, `reverb`, `ambient_sounds`, `max_sounds` (percent), `mute_in_background` |
| `input` | `mouse_sensitivity`, `right_hand_mouse`, `scroll: {side_speed, grab_speed}` (percent) |
| `camera` | `maximum_zoom_out` (100-300 percent), `zoom_style`, `zoom_lock` |
| `autosave` | `interval_ms`, `count` |
| `network` | `host_game_name`, `adapter`, `pt_rate: {min, max, tuning_threshold}`, `ct_send_interval: {max, step_down, step_up}` |
| `screenshots` | `jpg`, `end_game` |
| `debug` | `game_log` (`Logs/game-*.log`), `oos_handling` |
| `recent` | `game_file`, `scenario_file`, `quick_save`, `auto_save`, `post_game_stats_page` |
| `game_options` | the last random map's options: `map: {size, type}`, `starting_resources`, `epochs: {start, end}`, `unit_limit`, `wonders_for_victory`, `variant`, `difficulty`, `speed`, `record_game`, `reveal_map`, `allow_custom_civs`, `lock_teams`, `lock_speed`, `cheat_codes`, `victory_allowed` |

A value out of its range is replaced by the default (or clamped, for the detail levels, volumes and speeds). Not
stored any more, so fixed at their defaults: the rasterizer name (one renderer), the bit depths (always 32),
`Wait for VSync` (from `frame_rate_limit`), the WON password and Sierra.com names, the IME candidate window, and the
values nothing read (unit scale factor, animal herds, area effect overlays, button MIP levels, auto waypoints,
keyboard layout, load game name). The harness writes a fresh file per run from `block.key=value` pins
(`tools/harness/config_yaml.py`; `posix_game.py --config audio.music.enabled=false`).

### 2.2 The `.ssa` archive (`FSArchive`, `src/Low-Level Engine/File System/FSArchive.cpp`)

All integers are little-endian u32.

```
u32 identifier  = 'ssar' (bytes "rass", 0x73736172)
u32 version     = 1
u32 reserved    = 0
u32 fatSize     = bytes of the FAT including this dword (4 + sum(len+5+12))
repeat until fatSize consumed:
    UString name     u32 len (incl. NUL) + chars + NUL; lower-case relative path, '\' separators ("db\dbobjects.dat")
    s32 start        absolute offset of the data
    s32 end          last byte (start + length - 1; == start when empty)
    s32 length       stored (compressed) length
u32 tagCount
repeat tagCount: UWideString name, UWideString value   (u32 size incl. NUL + MBCS bytes + NUL)
file data, in FAT (std::map, i.e. sorted) order, contiguous
```

* `data.ssa` (AoC) holds 826 files with one tag, `Timestamp=4035612`. Campaign archives (`Data\Campaigns\*.ssa`)
  carry the tags `CampaignName`, `CampaignDescription`, `BackgroundImage`, `AuthorName`, `AuthorEmail` and
  `RequiredCivName`. They hold `campaigns\<name>.txt`, `scenarios\*.scn`, `scenarios\*.jpg` and `sounds\*.mp3`.
* `FSArchive::FindFile` lower-cases the name and looks it up in the map. `ReadFATFile` decompresses each entry
  separately (2.3).
* Contents of AoC `data.ssa`: db 39 `.dat`, civilizations 23 `.civ`, models 136 `.cem`, sounds 122 `.wav` + 7 `.mp3`,
  textures 405 `.tga` + 22 `.sst`, unit ai scripts 72 `.tai`.

### 2.3 Compression: "PK01" + PKWARE DCL implode (`UDataCompression.cpp`)

```
char  signature[4] = "PK01"
u32   uncompressedSize
u32   reserved = 0
...   PKWARE Data Compression Library "implode" stream (binary mode / ASCII flag byte, dictionary-size byte 4..6)
```

* A block counts as compressed when it is longer than 12 bytes and starts with "PK01". Otherwise it is stored raw.
* Every db file in `data.ssa` is compressed. The compression level is the DCL dictionary size passed by the archive
  builder.
* Save-game and scenario chunks use the same wrapper (`FSChunk::SavePersistentToChunk` calls `Compress(0x1000)`).
* Decompression is the standard DCL `explode`. zlib's `contrib/blast` is a compatible decoder; a 100-line Python port of it decoded every entry tried to its declared size: all 39 db files, all `.tai`
  and `.civ` files, a few models and textures, and a scenario.
* **Round trip:** the compressed bytes cannot be reproduced exactly unless the same implode implementation and
  dictionary size are used. The port ships its own implode (`src/ThirdParty/pkdcl`, documented as non-exact). That
  does not matter: the loaders accept stored (uncompressed) data, and loose files need no compression at all.

### 2.4 Conventions shared by the db tables

* **Byte order and types:** little-endian x86 (MSVC 6 layout). `long` and `unsigned long` are 4 bytes, `float` is
  IEEE-754 single, `bool` is 1 byte (0/1), and structs use natural alignment (4). The port pins every record with
  `static_assert(sizeof(T) == N)` and `tools/record_sizes.py` checks x86 against x64. No record contains a pointer.
* **Row layout of the database export:** almost every table is a dump of one SSS database table (Access-style). The
  column order is the database's, so it differs between tables. The common pattern is `char name[100]; s32 dbId;
  s32 index; ...`, with variants:
  * `name` first: most tables. The `+0x64` dword is the database's row id (unique, with gaps, often 1-based). The
    `+0x68` dword is the 0-based row index (or -1 for an unused row). The loaders test this index (`< 0`, `<= 0`) to
    skip unused rows.
  * `dbId, index` first and then `name` at `+0x08`: `dbcivilization`, `dbpremadecivs`, `dbmusic`, `dbuihotkey`,
    `dbcpbehavior` (which has no name), `dbrandommap`.
  * `name, <floats...>, dbId at +0x7c, index at +0x80`: `dbcalamity`.
  * `dbworld` has no name. Its 95 rows are referenced only by index.
* **Holes:** each table has one row per database id slot. A missing id is written as an *uninitialised* struct: the
  converter's MSVC debug heap fill `0xCC` with the id/index set to -1 or 0. Within one table all hole rows are
  byte-identical. Examples: dbtechtree 3,701 of 4,494 records, dbuicontrols 2,029/3,401, dbeffects 807/2,000,
  dbbuttons 411/1,549, dbsounds 215/1,087, dbobjects 110/848. Across the tables, `0xCC` bytes make up 1.84 MB of the
  7.79 MB unpacked db data.
* **Junk bytes:** besides the holes, real rows carry nondeterministic bytes in three places:
  * after the NUL of fixed `char[100]` strings: `0xCC`, or leftover text such as ":May:May:Jun:June" (CRT month
    names) in `dbunitbehavior` and `dbterraintype`;
  * in alignment padding: for example 3 bytes after a `bool` (`dbgraphics` +0x291 shows `0xCCCCCC`);
  * in some unread dwords: dbobjects +0x3b4..+0x47b holds stale heap-pointer-like values such as `0x00830200`.

  The game never reads any of these bytes. A converter that must be byte-exact has to keep them (section 6).
* **Strings:**
  * Fixed `char[N]` fields are NUL-terminated ASCII (no high-bit characters seen). N is 100 for names, 0x68 when
    padding follows, 52 for `mAIScriptName`, 52 for music file names.
  * Variable strings inside db files (`dbsounds`, `dbgraphics`, `dbterrain`, `dbterraingraytextures`) are `u32 length
    + bytes` **without a terminating NUL** (all 33,883 such strings checked). `UString::Load` reads them that way.
  * Elsewhere the engine writes `UString`/`UWideString` with the NUL included (`UString::Save`: length+1). So the
    exporter's strings and the engine's strings differ by that one byte, and a writer has to pick the right form.
* **Counts:** most files start with `u32 count`. Exceptions: `dbeffects` (raw array of 2000), `dbevents` (1500 rows,
  no count), `dbtechtree` (15 epoch blocks) and `dbrandommap` (two header dwords).
* **Indices vs ids:** the game keeps tables in vectors indexed by **file row position**. Fields that reference another
  table (`mButton`, `mGraphic`, sound ids, `mGraphicEffect`, unit-set ids...) are therefore row positions, which equal
  the `index` column. Holes keep the positions stable. Removing or reordering rows breaks every reference; renumbering
  needs a global rewrite. Unit "types" (`mUnitType`, up to 0x366) are a separate id space: `gUnitClassTable` has 0x367
  slots and is indexed by `mUnitType`, not by dbobjects row.
* **Language strings:** every `mNameID`/`mNameStringID`/`mStringTable*` field is a string id in `Language.dll`
  (source: `src/Empire Earth/Language/Language.rc`). Example: CP behaviour row i is named by string 28000+i.

### 2.5 How the `0xCC` and index columns let a converter shrink the text

| Table | rows | hole rows (identical) | text form (section 8) |
|---|---|---|---|
| dbaiunittargeting | 3 | 0 | rows as they are |
| dbambientsounds | 28 | 0 | rows as they are |
| dbanimals | 14 | 0 | rows as they are |
| dbareaeffect | 50 | 4 | not listed in the text (the importer writes the hole record) |
| dbbuttons | 1549 | 411 | not listed in the text (the importer writes the hole record) |
| dbcalamity | 36 | 0 | rows as they are |
| dbcivilization | 29 | 2 | not listed in the text (the importer writes the hole record) |
| dbcivpowers | 23 | 0 | rows as they are |
| dbcliffterrain | 3 | 0 | rows as they are |
| dbcolortable | 69 | 0 | rows as they are |
| dbcpbehavior | 126 | 0 | rows as they are |
| dbeffects | 2000 | 807 | not listed in the text (the importer writes the hole record) |
| dbfamily | 60 | 0 | rows as they are |
| dbgamevariant | 3 | 0 | rows as they are |
| dbgfxeffects | 292 | 4 | not listed in the text (the importer writes the hole record) |
| dbgraphics | 991 | 23 | not listed in the text (the importer writes the hole record) |
| dbmusic | 9 | 0 | rows as they are |
| dbobjects | 848 | 110 | not listed in the text (the importer writes the hole record) |
| dbpremadecivs | 23 | 0 | rows as they are |
| dbsounds | 1087 | 215 | not listed in the text (the importer writes the hole record) |
| dbstartingresources | 6 | 0 | rows as they are |
| dbtechtree | 4494 | 3701 | not listed in the text (the importer writes the hole record) |
| dbterrain | 93 | 16 | not listed in the text (the importer writes the hole record) |
| dbterraintype | 68 | 0 | rows as they are |
| dbuiback | 167 | 75 | not listed in the text (the importer writes the hole record) |
| dbuicontrolevents | 252 | 18 | not listed in the text (the importer writes the hole record) |
| dbuicontrols | 3401 | 2029 | not listed in the text (the importer writes the hole record) |
| dbuifonts | 61 | 4 | not listed in the text (the importer writes the hole record) |
| dbuiformevents | 21 | 0 | rows as they are |
| dbuiforms | 40 | 2 | not listed in the text (the importer writes the hole record) |
| dbuihotkey | 32 | 0 | rows as they are |
| dbunitbehavior | 8 | 0 | rows as they are |
| dbunitset | 173 | 3 | not listed in the text (the importer writes the hole record) |
| dbupgrade | 76 | 0 | rows as they are |
| dbweapontohit | 6 | 0 | rows as they are |
| dbworld | 95 | 0 | rows as they are |

### 2.6 Hints from SSS's converter logs (base game `data.ssa`)

`db\converteroutput.log` (512 KB) lists the source database tables in export order: Objects, Graphics, Sounds,
Tech Tree, Events, Effects, EventDetails, Buttons, Family, Color, AIUnitTargeting, RandomMap, Terrain, TerrainType,
World, Civilization, GfxEffect, Calamity, TerrainGrayTextures, UIBackground, Music, Upgrade, UIControls (mislabelled
"Calamity" in the log), UIControlEvents, UIFormEvents, UIForms, Fonts, AIBehavior, AreaEffect, UnitSet, WeaponToHit,
AmbientSounds, HotKey, Animals, UnitBehavior, StartingResources, GameVariant, Premade Civs, Cliff.

Useful details it confirms:

* dbobjects rows end with a "BldTechArray": the techs a unit can build or research. These are `mAttributes[]` with
  `mNumAttributes`.
* Tech-tree nodes carry "Parents": the unit types that offer the node. These are the trailing u32 lists in
  `dbtechtree`.
* `dbevents` is Events joined with EventDetails (EventID -> EffectID pairs).
* Effects have a free-text "Data" description that is not exported. The exe never sees effect names.

`db\converterverification.log` holds the SSS sanity rules, for example "collision sphere larger than tile size" and
"unused sound record". The base game's tables are an **older revision** of the same format: base dbobjects records
are 1948 bytes, not 1960, and the row counts differ. A converter must be per-game.

---

## 3. The tables

### 3.1 `db\dbobjects.dat`: Unit classes

* **Loader:** `EEUnitClass::CreateUnitClasses` 0x6358b1 -> `EEDatabase::Load` 0x4d60ae / `EEDbObject::Load` 0x4d689b (src/Empire Earth/Database/EEDatabase.cpp, EEDbObject.cpp); fields consumed by `EEUnitClass::EEUnitClass` 0x62f317 (src/Empire Earth/Units/EEUnitClass.cpp) and the `EEUC*` subclass ctors. Layout: include/ee/EEUnitClassRecord.h
* **Container:** `u32 count` + count x 1960-byte `EEUnitClassRecord` (read whole into `EEDbObject::mRecord[0x7a8]`).
* **AoC data:** 848 records, 1,662,084 bytes unpacked. Bytes per record: read 80%, label 0%, unused 18%, padding 2%.
* **Semantics:** Rows with `mIsEnabled == 0` or `mUnitType == -1` are skipped. The class factory switches on `mCategory` (0..0x22: ambient, resource, building, airplane, naval, land, citizen, helicopter, projectile, bomb, missile, priest, torpedo...). `gUnitClassTable[mUnitType]` is the class (0x367 slots). Bugs kept by the port: terrain slot 5 copies `mCanMoveOnLand` again, so `mUnusedFlag2`/`mUnusedFlag3` (+0x2e0/+0x2e1) are never read. +0x3b4..+0x47b (`mUnusedConverterScratch`, 200 bytes) is unread and holds stale converter heap values. In the "ctor ->" notes, the left side is the `EEUnitClass` member the field is copied into. Subclass ctors may read more fields.
* **Cross-references:** `mFamily` -> dbfamily row (damage table); `mButton` -> dbbuttons; `mGraphic` -> dbgraphics (media index); `mCreationSound`, `mAcknowledgeSound1/2`, `mSelectionSound1/2`, `mDeathSound` -> dbsounds; `mDeathEffect`..`mBloodStainEffect`, `mCreationEffect`, `mLeprosyEffect`, `mPlagueEffect`, `mMoraleEffect` -> dbgfxeffects; `mAreaEffectTemplate0..3` -> dbareaeffect; `mUpgradeTableIndex` -> dbupgrade; `mBehavior` -> dbunitbehavior; `mTechnology`, `mGarrisonTechnology`, `mAttributes[0..mNumAttributes)` -> dbtechtree node ids; `mOverlapUnitSet`, `mWonderUnitSet` -> dbunitset; `mAIScriptName` -> `Unit AI Scripts\<name>.tai`; `mNameStringID` -> Language.dll; `mUnitType`, `mDeathSpawnType`, `mPackedType` -> unit type ids.
* **Suggested text form:** YAML

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `mName` | named | +0x000 the unit name (UString::Copy into mName) | text 738/848, max len 39, junk after NUL in 124, e.g. "Priest" |
| 0x064 | s32 | `mUnusedRowID` | unused | +0x064 | 0..973 (739 distinct) |
| 0x068 | s32 | `mFamily` | named | +0x068; ctor -> mFamily | -1..59 (59 distinct) |
| 0x06c | s32 | `mUnusedUnitGroup` | unused | +0x06c; ctor -> mL208 | -1..32 (30 distinct) |
| 0x070 | s32 | `mEpoch` | named | +0x070; ctor -> mEpoch | 0..15 (16 distinct) |
| 0x074 | s32 | `mUnitType` | named | +0x074; ctor -> mUnitType | -1..847 (739 distinct) |
| 0x078 | s32 | `mHitPoints` | named | +0x078; ctor -> mHitPoints | 0..99000 (190 distinct) |
| 0x07c | s32 | `mWaterKind` | named | +0x07c; ctor -> mWaterKind | 0..2 (3 distinct) |
| 0x080 | f32 | `mMinRange` | named | +0x080; ctor -> mMinRange | 0..3 (4 distinct) |
| 0x084 | f32 | `mRange` | named | +0x084; ctor -> mRange | 0..99 (24 distinct) |
| 0x088 | f32 | `mLineOfSight` | named | +0x088; ctor -> mLineOfSight | 0..13 (13 distinct) |
| 0x08c | f32 | `mReloadTime` | named | +0x08c; ctor -> mReloadTime | 0..180 (23 distinct) |
| 0x090 | f32 | `mAreaEffect` | named | +0x090; ctor -> mAreaEffect | 0..3.5 (17 distinct) |
| 0x094 | f32 | `mMaxVelocity` | named | +0x094; ctor -> mMaxVelocity | 0..5 (31 distinct) |
| 0x098 | f32 | `mAcceleration` | named | +0x098 (EEUnitClass +0x13c); ctor -> mAcceleration | 0..50 (21 distinct) |
| 0x09c | f32 | `mAngularAcceleration` | named | +0x09c; ctor -> mAngularAcceleration | 0..12.2173 (25 distinct) |
| 0x0a0 | f32 | `mMaxAngularVelocity` | named | +0x0a0; ctor -> mMaxAngularVelocity | 0..13.9626 (28 distinct) |
| 0x0a4 | f32 | `mTurretMaxAngularVelocity` | named | +0x0a4; ctor -> mTurretMaxAngularVelocity | 0..12.2173 (12 distinct) |
| 0x0a8 | bool | `mCanFly` | named | +0x0a8; ctor -> mCanFly | 0..1 (2 distinct) |
| 0x0a9 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x0aa | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x0ab | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x0ac | s32 | `mFlightTime` | named | +0x0ac; ctor -> mFlightTime | 0..1000 (21 distinct) |
| 0x0b0 | f32 | `mFacingOffset` | named | +0x0b0; ctor -> mFacingOffset | 0..6.2832 (4 distinct) |
| 0x0b4 | f32 | `mFiringArc` | named | +0x0b4; ctor -> mFiringArc | 0..6.2832 (14 distinct) |
| 0x0b8 | bool | `mFiresBroadside` | named | +0x0b8; ctor -> mFiresBroadside | 0..1 (2 distinct) |
| 0x0b9 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x0ba | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x0bb | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x0bc | s32 | `mWeapon` | named | +0x0bc; ctor -> mWeapon | -1..5 (7 distinct) |
| 0x0c0 | s32 | `mAttack` | named | +0x0c0; ctor -> mAttack | 0..20000 (98 distinct) |
| 0x0c4 | s32 | `mShockArmor` | named | +0x0c4; ctor -> mArmor[0] | 0..75 (14 distinct) |
| 0x0c8 | s32 | `mArrowArmor` | named | +0x0c8; ctor -> mArmor[1] | 0..50 (18 distinct) |
| 0x0cc | s32 | `mPierceArmor` | named | +0x0cc; ctor -> mArmor[2] | 0..60 (16 distinct) |
| 0x0d0 | s32 | `mGunArmor` | named | +0x0d0; ctor -> mArmor[3] | 0..76 (27 distinct) |
| 0x0d4 | s32 | `mLaserArmor` | named | +0x0d4; ctor -> mArmor[4] | 0..82 (24 distinct) |
| 0x0d8 | s32 | `mMissileArmor` | named | +0x0d8; ctor -> mArmor[5] | 0..35 (15 distinct) |
| 0x0dc | s32 | `mProjectileType` | named | +0x0dc | -1..839 (44 distinct) |
| 0x0e0 | s32 | `mAmmoType` | named | +0x0e0; ctor -> mAmmoType | -1..65 (63 distinct) |
| 0x0e4 | bool | `mIsEnabled` | named | +0x0e4; ctor -> mIsEnabled | 0..1 (2 distinct) |
| 0x0e5 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..16 (3 distinct) |
| 0x0e6 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x0e7 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x0e8 | f32 | `mScale` | named | +0x0e8; ctor -> mScale | 0..7 (124 distinct) |
| 0x0ec | char[52] | `mAIScriptName` | named | +0x0ec a name (UString::Copy into mAIScriptName) | text 773/848, max len 52, junk after NUL in 7, e.g. "ÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌ" |
| 0x120 | s32 | `mCategory` | named | +0x120; ctor -> mCategory | -1..34 (32 distinct) |
| 0x124 | s32 | `mUnused1` | unused | +0x124; not read by the EEUnitClass ctor | const 0 |
| 0x128 | s32 | `mButton` | named | +0x128; ctor -> mButton via EEButtonObject::Get | -1..1548 (415 distinct) |
| 0x12c | s32 | `mGraphic` | named | +0x12c; ctor -> mModel via (EEModel*)EEMedia::Get | -1..990 (663 distinct) |
| 0x130 | f32 | `mWorkRate` | named | +0x130; ctor -> mWorkRate | 0..1000 (2 distinct) |
| 0x134 | f32 | `mIronGatherTime` | named | +0x134; ctor -> mGatherTimes[3] | 0..10000 (4 distinct) |
| 0x138 | f32 | `mFarmGatherTime` | named | +0x138; ctor -> mGatherTimes[6] | -1.07374e+08..10000 (6 distinct) |
| 0x13c | f32 | `mGoldGatherTime` | named | +0x13c; ctor -> mGatherTimes[2] | 0..6000 (2 distinct) |
| 0x140 | f32 | `mStoneGatherTime` | named | +0x140; ctor -> mGatherTimes[1] | 0..10500 (2 distinct) |
| 0x144 | f32 | `mWoodGatherTime` | named | +0x144; ctor -> mGatherTimes[0] | 0..4400 (2 distinct) |
| 0x148 | f32 | `mHuntGatherTime` | named | +0x148; ctor -> mGatherTimes[4] | 0..3500 (7 distinct) |
| 0x14c | f32 | `mForageGatherTime` | named | +0x14c; ctor -> mGatherTimes[5] | 0..5200 (2 distinct) |
| 0x150 | s32 | `mCreationSound` | named | +0x150; ctor -> mCreationSound via EESoundObject::Get | -1..991 (30 distinct) |
| 0x154 | s32 | `mAcknowledgeSound1` | named | +0x154; ctor -> mAcknowledgeSounds[0] via EESoundObject::Get | -1..1081 (192 distinct) |
| 0x158 | s32 | `mAcknowledgeSound2` | named | +0x158; ctor -> mAcknowledgeSounds[1] via EESoundObject::Get | -1..1082 (185 distinct) |
| 0x15c | s32 | `mSelectionSound1` | named | +0x15c; ctor -> mSelectionSounds[0] via EESoundObject::Get | -1..1083 (200 distinct) |
| 0x160 | s32 | `mSelectionSound2` | named | +0x160; ctor -> mSelectionSounds[1] via EESoundObject::Get | -1..1084 (197 distinct) |
| 0x164 | s32 | `mDeathSound` | named | +0x164; ctor -> mDeathSound via EESoundObject::Get | -1..990 (43 distinct) |
| 0x168 | s32 | `mRepairRate` | named | +0x168; ctor -> mRepairRate | 0..1 (2 distinct) |
| 0x16c | s32 | `mCommand0` | named | +0x16c; ctor -> mCommands[0] | -1..25 (8 distinct) |
| 0x170 | s32 | `mCommand1` | named | +0x170; ctor -> mCommands[1] | -1..26 (7 distinct) |
| 0x174 | s32 | `mCommand2` | named | +0x174; ctor -> mCommands[2] | -1..18 (3 distinct) |
| 0x178 | s32 | `mCommand3` | named | +0x178; ctor -> mCommands[3] | -1..35 (11 distinct) |
| 0x17c | s32 | `mCommand4` | named | +0x17c; ctor -> mCommands[4] | -1..6 (2 distinct) |
| 0x180 | s32 | `mCommand5` | named | +0x180; ctor -> mCommands[5] | const -1 |
| 0x184 | s32 | `mCommand6` | named | +0x184; ctor -> mCommands[6] | -1..29 (2 distinct) |
| 0x188 | s32 | `mCommand7` | named | +0x188; ctor -> mCommands[7] | -1..30 (2 distinct) |
| 0x18c | s32 | `mCommand8` | named | +0x18c; ctor -> mCommands[8] | const -1 |
| 0x190 | s32 | `mUnused2` | unused | +0x190; not read by the EEUnitClass ctor | const -1 |
| 0x194 | s32 | `mTileSize` | named | +0x194; ctor -> mTileSize | 0..6 (7 distinct) |
| 0x198 | s32 | `mResourceType` | named | +0x198; ctor -> mResourceType | 0..6 (7 distinct) |
| 0x19c | f32 | `mResourceAmount` | named | +0x19c; ctor -> mResourceAmount | 0..300000 (12 distinct) |
| 0x1a0 | bool | `mFacesCamera` | named | +0x1a0; ctor -> mB2F6 | 0..1 (2 distinct) |
| 0x1a1 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x1a2 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x1a3 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x1a4 | s32 | `mUnused3` | unused | +0x1a4; not read by the EEUnitClass ctor | -1..43 (3 distinct) |
| 0x1a8 | f32 | `mRottingTime` | named | +0x1a8; ctor -> mRottingTime | 0..40 (11 distinct) |
| 0x1ac | s32 | `mTransportSize` | named | +0x1ac; ctor -> mWeight | 0..5 (4 distinct) |
| 0x1b0 | s32 | `mTransportCapacity` | named | +0x1b0; ctor -> mTransportCapacity | 0..40 (12 distinct) |
| 0x1b4 | bool | `mCanUpgradeAreaEffect` | named | +0x1b4; ctor -> mUpgradesAreaEffect | 0..1 (2 distinct) |
| 0x1b5 | bool | `mCanUpgradeRepair` | named | +0x1b5; ctor -> mUpgradesRepair | const 0 |
| 0x1b6 | bool | `mCanUpgradeShockArmor` | named | +0x1b6; ctor -> mUpgradesShockArmor | 0..1 (2 distinct) |
| 0x1b7 | bool | `mCanUpgradePierceArmor` | named | +0x1b7; ctor -> mUpgradesPierceArmor | 0..1 (2 distinct) |
| 0x1b8 | bool | `mCanUpgradeArrowArmor` | named | +0x1b8; ctor -> mUpgradesArrowArmor | 0..1 (2 distinct) |
| 0x1b9 | bool | `mCanUpgradeLaserArmor` | named | +0x1b9; ctor -> mUpgradesLaserArmor | 0..1 (2 distinct) |
| 0x1ba | bool | `mCanUpgradeGunArmor` | named | +0x1ba; ctor -> mUpgradesGunArmor | 0..1 (2 distinct) |
| 0x1bb | bool | `mCanUpgradeMissileArmor` | named | +0x1bb; ctor -> mUpgradesMissileArmor | const 0 |
| 0x1bc | bool | `mCanUpgradeRange` | named | +0x1bc; ctor -> mUpgradesRange | 0..1 (2 distinct) |
| 0x1bd | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..16 (3 distinct) |
| 0x1be | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x1bf | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x1c0 | s32 | `mLeadership` | named | +0x1c0; ctor -> mLeadership | 0..5 (4 distinct) |
| 0x1c4 | s32 | `mUnused4` | unused | +0x1c4; not read by the EEUnitClass ctor | 0..3 (2 distinct) |
| 0x1c8 | bool | `mPitchesWithTerrain` | named | +0x1c8; ctor -> mPitchesWithTerrain | 0..1 (2 distinct) |
| 0x1c9 | bool | `mRollsWithTerrain` | named | +0x1c9; ctor -> mRollsWithTerrain | 0..1 (2 distinct) |
| 0x1ca | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..69 (3 distinct) |
| 0x1cb | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x1cc | f32 | `mMaxRoll` | named | +0x1cc; ctor -> mMaxRoll | 0..1.1345 (9 distinct) |
| 0x1d0 | s32 | `mUnused5` | unused | +0x1d0; not read by the EEUnitClass ctor | const -1 |
| 0x1d4 | f32 | `mCruiseAltitude` | named | +0x1d4; ctor -> mCruiseAltitude | -0.5..3 (17 distinct) |
| 0x1d8 | s32 | `mNameStringID` | named | +0x1d8; ctor -> mNameStringID | 0..999999 (642 distinct) |
| 0x1dc | s32 | `mTargetSearchInterval` | named | +0x1dc; ctor -> mTargetSearchInterval | 0..5000000 (14 distinct) |
| 0x1e0 | s32 | `mHealingPoints` | named | +0x1e0; ctor -> mHealingPoints | 0..275 (18 distinct) |
| 0x1e4 | s32 | `mRegeneration` | named | +0x1e4; ctor -> mRegeneration | 0..10 (5 distinct) |
| 0x1e8 | bool | `mCanEnterLandTransports` | named | +0x1e8; ctor -> mCanEnterLandTransports | 0..1 (2 distinct) |
| 0x1e9 | bool | `mCanEnterAirTransports` | named | +0x1e9; ctor -> mCanEnterAirTransports | 0..1 (2 distinct) |
| 0x1ea | bool | `mCanEnterSeaTransports` | named | +0x1ea; ctor -> mCanEnterSpaceTransports = mCanEnterSeaTransports | 0..1 (2 distinct) |
| 0x1eb | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x1ec | s32 | `mUnused6` | unused | +0x1ec; not read by the EEUnitClass ctor | 0..9 (10 distinct) |
| 0x1f0 | bool | `mFiresWhileMoving` | named | +0x1f0; ctor -> mFiresWhileMoving | 0..1 (2 distinct) |
| 0x1f1 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x1f2 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x1f3 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x1f4 | f32 | `mSelectionSize` | named | +0x1f4 (-> EEUnitClass::mSelectionSize, a mov copy); ctor -> mSelectionSize | 0..6 (32 distinct) |
| 0x1f8 | f32 | `mShadowScale` | named | +0x1f8 (EEUnitClass mL16C, a float); ctor -> mShadowScale | 0..9.5 (37 distinct) |
| 0x1fc | f32 | `mUnusedShadowOffset` | unused | +0x1fc (EEUnitClass mL170, a float); ctor -> mL170 | const 0 |
| 0x200 | f32 | `mOuterCollisionSphere` | named | +0x200; ctor -> mOuterCollisionSphere | 0..6 (32 distinct) |
| 0x204 | f32 | `mInnerCollisionSphere` | named | +0x204; ctor -> mInnerCollisionSphere | 0..6 (33 distinct) |
| 0x208 | bool | `mIsCollidable` | named | +0x208; ctor -> mIsCollidable | 0..1 (2 distinct) |
| 0x209 | bool | `mUnusedGroundFlag` | unused | +0x209; ctor -> mB305 | 0..1 (2 distinct) |
| 0x20a | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..77 (3 distinct) |
| 0x20b | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x20c | f32 | `mMissileDelayTime` | named | +0x20c; ctor -> mMissileDelayTime | 0..100 (13 distinct) |
| 0x210 | f32 | `mLaunchPitch` | named | +0x210; ctor -> mLaunchPitch | 0..90 (8 distinct) |
| 0x214 | s32 | `mAmmunition` | named | +0x214; ctor -> mAmmunition | 0..8 (6 distinct) |
| 0x218 | bool | `mUsesAmmunition` | named | +0x218; ctor -> mUsesAmmunition | 0..1 (2 distinct) |
| 0x219 | bool | `mCanFireOverWalls` | named | +0x219; ctor -> mCanFireOverWalls | 0..1 (2 distinct) |
| 0x21a | bool | `mIsAffectedByGravity` | named | +0x21a; ctor -> mIsAffectedByGravity | 0..1 (2 distinct) |
| 0x21b | bool | `mShootsHigh` | named | +0x21b; ctor -> mShootsHigh | 0..1 (2 distinct) |
| 0x21c | s32 | `mUnusedReference` | unused | +0x21c; ctor -> mL284 | -1..0 (2 distinct) |
| 0x220 | s32 | `mUnusedValue` | unused | +0x220; ctor -> mL180 | const 0 |
| 0x224 | s32 | `mAIKind` | named | +0x224; ctor -> mAIKind | 0..39 (38 distinct) |
| 0x228 | s32 | `mCounterKind1` | named | +0x228; ctor -> mCounterKind1 | 0..38 (23 distinct) |
| 0x22c | s32 | `mCounterKind2` | named | +0x22c; ctor -> mCounterKind2 | 0..40 (26 distinct) |
| 0x230 | s32 | `mCounterKind3` | named | +0x230; ctor -> mCounterKind3 | 0..37 (26 distinct) |
| 0x234 | bool | `mCanSetRallyPoint` | named | +0x234; ctor -> mCanSetRallyPoint | 0..1 (2 distinct) |
| 0x235 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x236 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x237 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x238 | s32 | `mUpgradeTableIndex` | named | +0x238 | 0..75 (65 distinct) |
| 0x23c | s32 | `mConversionRate` | named | +0x23c; ctor -> mConversionRate | 0..95 (5 distinct) |
| 0x240 | f32 | `mConversionResistance` | named | +0x240 (a float: EEUnitClass::mConversionResistance's source); ctor -> mConversionResistance | 0..1 (12 distinct) |
| 0x244 | bool | `mIsPlayerPlaceable` | named | +0x244; ctor -> mIsPlayerPlaceable | 0..1 (2 distinct) |
| 0x245 | bool | `mIsGaiaPlaceable` | named | +0x245; ctor -> mIsGaiaPlaceable | 0..1 (2 distinct) |
| 0x246 | bool | `mIsVital` | named | +0x246; ctor -> mIsVital | 0..1 (2 distinct) |
| 0x247 | s8 | `mUnusedFlag1` | unused | padding byte (0, or 0xCC in holes); not read | 0..1 (2 distinct) |
| 0x248 | bool | `mIsDeletable` | named | +0x248; ctor -> mIsDeletable | 0..1 (2 distinct) |
| 0x249 | bool | `mIsDecoration` | named | +0x249; ctor -> mIsDecoration | 0..1 (2 distinct) |
| 0x24a | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..85 (3 distinct) |
| 0x24b | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x24c | s32 | `mDeathEffect` | named | +0x24c; ctor -> mDeathEffect via EEGraphicEffect::Get | -1..290 (19 distinct) |
| 0x250 | s32 | `mDamageEffect` | named | +0x250; ctor -> mDamageEffect via EEGraphicEffect::Get | -1..278 (5 distinct) |
| 0x254 | s32 | `mMuzzleFlashEffect` | named | +0x254; ctor -> mMuzzleFlashEffect via EEGraphicEffect::Get | -1..205 (7 distinct) |
| 0x258 | s32 | `mTrailEffect` | named | +0x258; ctor -> mTrailEffect via EEGraphicEffect::Get | -1..22 (3 distinct) |
| 0x25c | s32 | `mRippleEffect` | named | +0x25c; ctor -> mRippleEffect via EEGraphicEffect::Get | -1..13 (4 distinct) |
| 0x260 | s32 | `mSplashEffect` | named | +0x260; ctor -> mSplashEffect via EEGraphicEffect::Get | -1..126 (3 distinct) |
| 0x264 | s32 | `mHealEffect` | named | +0x264; ctor -> mHealEffect via EEGraphicEffect::Get | -1..150 (3 distinct) |
| 0x268 | s32 | `mUnusedFireTrailEffect` | unused | +0x268; ctor -> mFireTrailEffect via EEGraphicEffect::Get | -1..7 (2 distinct) |
| 0x26c | s32 | `mImpactSparksEffect` | named | +0x26c; ctor -> mImpactSparksEffect via EEGraphicEffect::Get | -1..276 (3 distinct) |
| 0x270 | s32 | `mImpactBloodEffect` | named | +0x270; ctor -> mImpactBloodEffect via EEGraphicEffect::Get | -1..16 (2 distinct) |
| 0x274 | s32 | `mAttachedEffect` | named | +0x274; ctor -> mAttachedEffect via EEGraphicEffect::Get | -1..242 (8 distinct) |
| 0x278 | s32 | `mUnusedEffect1` | unused | +0x278; ctor -> mEffect090 via EEGraphicEffect::Get | const -1 |
| 0x27c | s32 | `mCreationEffect` | named | +0x27c; ctor -> mCreationEffect via EEGraphicEffect::Get | -1..252 (6 distinct) |
| 0x280 | s32 | `mUnusedEffect2` | unused | +0x280; ctor -> mEffect098 via EEGraphicEffect::Get | const -1 |
| 0x284 | s32 | `mUnusedEffect3` | unused | +0x284; ctor -> mEffect09C via EEGraphicEffect::Get | const -1 |
| 0x288 | s32 | `mUnusedEffect4` | unused | +0x288; ctor -> mEffect0A0 via EEGraphicEffect::Get | const -1 |
| 0x28c | s32 | `mUnusedEffect5` | unused | +0x28c; ctor -> mEffect0A4 via EEGraphicEffect::Get | const -1 |
| 0x290 | s32 | `mUnusedEffect6` | unused | +0x290; ctor -> mEffect0A8 via EEGraphicEffect::Get | const -1 |
| 0x294 | s32 | `mSpecialAttackEffect` | named | +0x294; ctor -> mSpecialAttackEffect via EEGraphicEffect::Get | -1..282 (2 distinct) |
| 0x298 | s32 | `mUnusedEffect7` | unused | +0x298; ctor -> mEffect0B0 via EEGraphicEffect::Get | -1..0 (2 distinct) |
| 0x29c | s32 | `mShotEffect` | named | +0x29c; ctor -> mShotEffect via EEGraphicEffect::Get | -1..251 (26 distinct) |
| 0x2a0 | s32 | `mLeprosyEffect` | named | +0x2a0; ctor -> mLeprosyEffect via EEGraphicEffect::Get | -1..190 (5 distinct) |
| 0x2a4 | s32 | `mPlagueEffect` | named | +0x2a4; ctor -> mPlagueEffect via EEGraphicEffect::Get | -1..190 (5 distinct) |
| 0x2a8 | s32 | `mMoraleEffect` | named | +0x2a8; ctor -> mMoraleEffect via EEGraphicEffect::Get | -1..153 (2 distinct) |
| 0x2ac | s32 | `mPulseCannonEffect` | named | +0x2ac; ctor -> mPulseCannonEffect via EEGraphicEffect::Get | -1..196 (3 distinct) |
| 0x2b0 | s32 | `mBattleCryEffect` | named | +0x2b0; ctor -> mBattleCryEffect via EEGraphicEffect::Get | -1..221 (2 distinct) |
| 0x2b4 | s32 | `mShellHoleEffect` | named | +0x2b4; ctor -> mShellHoleEffect via EEGraphicEffect::Get | -1..270 (3 distinct) |
| 0x2b8 | s32 | `mUnusedEffect8` | unused | +0x2b8; ctor -> mEffect0D0 via EEGraphicEffect::Get | const -1 |
| 0x2bc | s32 | `mBloodStainEffect` | named | +0x2bc; ctor -> mBloodStainEffect via EEGraphicEffect::Get | -1..261 (2 distinct) |
| 0x2c0 | s32 | `mUnused7` | unused | +0x2c0; not read by the EEUnitClass ctor | const -1 |
| 0x2c4 | s32 | `mUnused8` | unused | +0x2c4; not read by the EEUnitClass ctor | -1..0 (2 distinct) |
| 0x2c8 | f32 | `mBobPitch` | named | +0x2c8; ctor -> mBobPitch | 0..2 (2 distinct) |
| 0x2cc | s32 | `mUnused9` | unused | +0x2cc; not read by the EEUnitClass ctor | const 0 |
| 0x2d0 | s32 | `mUnused10` | unused | +0x2d0; not read by the EEUnitClass ctor | const 0 |
| 0x2d4 | s32 | `mUnused11` | unused | +0x2d4; not read by the EEUnitClass ctor | const 0 |
| 0x2d8 | s32 | `mUnused12` | unused | +0x2d8; not read by the EEUnitClass ctor | const 0 |
| 0x2dc | bool | `mCanMoveOnLand` | named | +0x2dc; ctor -> mTerrainTypes[0] | 0..1 (2 distinct) |
| 0x2dd | bool | `mCanMoveOnDeepWater` | named | +0x2dd; ctor -> mTerrainTypes[kTerrainDeepWater] | 0..1 (2 distinct) |
| 0x2de | bool | `mCanMoveOnRock` | named | +0x2de; ctor -> mTerrainTypes[2] | 0..1 (2 distinct) |
| 0x2df | bool | `mCanMoveOnTerrainType3` | named | +0x2df; ctor -> mTerrainTypes[3] | 0..1 (2 distinct) |
| 0x2e0 | s8 | `mUnusedFlag2` | unused | padding byte (0, or 0xCC in holes); not read | 0..1 (2 distinct) |
| 0x2e1 | s8 | `mUnusedFlag3` | unused | padding byte (0, or 0xCC in holes); not read | 0..1 (2 distinct) |
| 0x2e2 | bool | `mCanMoveOnMud` | named | +0x2e2; ctor -> mTerrainTypes[4] | 0..1 (2 distinct) |
| 0x2e3 | bool | `mCanEnterFortresses` | named | +0x2e3; ctor -> mCanEnterFortresses | 0..1 (2 distinct) |
| 0x2e4 | bool | `mCanMoveOnRoughGrass` | named | +0x2e4; ctor -> mTerrainTypes[6] | 0..1 (2 distinct) |
| 0x2e5 | bool | `mCanMoveOnTerrainType7` | named | +0x2e5; ctor -> mTerrainTypes[7] | 0..1 (2 distinct) |
| 0x2e6 | bool | `mCanMoveOnPavement` | named | +0x2e6; ctor -> mTerrainTypes[8] | 0..1 (2 distinct) |
| 0x2e7 | bool | `mCanMoveOnCliffs` | named | +0x2e7; ctor -> mTerrainTypes[kTerrainCliff] | 0..1 (2 distinct) |
| 0x2e8 | bool | `mCanMoveOnShallowWater` | named | +0x2e8; ctor -> mTerrainTypes[kTerrainShallowWater] | 0..1 (2 distinct) |
| 0x2e9 | s8 | `mUnusedFlag4` | unused | padding byte (0, or 0xCC in holes); not read | 0..1 (2 distinct) |
| 0x2ea | bool | `mCanMoveOnSand` | named | +0x2ea; ctor -> mTerrainTypes[11] | 0..1 (2 distinct) |
| 0x2eb | bool | `mCanMoveInSpace` | named | +0x2eb; ctor -> mTerrainTypes[kTerrainSpace] | 0..1 (2 distinct) |
| 0x2ec | bool | `mIsAttackUnit` | named | +0x2ec; ctor -> mIsAttackUnit | 0..1 (2 distinct) |
| 0x2ed | bool | `mIsDefenseUnit` | named | +0x2ed; ctor -> mIsDefenseUnit | 0..1 (2 distinct) |
| 0x2ee | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x2ef | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x2f0 | s32 | `mAreaEffectTemplate0` | named | +0x2f0; ctor -> mAreaEffectTemplates[0] | -1..49 (18 distinct) |
| 0x2f4 | s32 | `mAreaEffectTemplate1` | named | +0x2f4; ctor -> mAreaEffectTemplates[1] | -1..7 (3 distinct) |
| 0x2f8 | s32 | `mAreaEffectTemplate2` | named | +0x2f8; ctor -> mAreaEffectTemplates[2] | -1..0 (2 distinct) |
| 0x2fc | s32 | `mAreaEffectTemplate3` | named | +0x2fc; ctor -> mAreaEffectTemplates[3] | -1..0 (2 distinct) |
| 0x300 | s32 | `mUnused13` | unused | +0x300; not read by the EEUnitClass ctor | const 0 |
| 0x304 | s32 | `mUnused14` | unused | +0x304; not read by the EEUnitClass ctor | const 0 |
| 0x308 | s32 | `mUnused15` | unused | +0x308; not read by the EEUnitClass ctor | const 0 |
| 0x30c | f32 | `mCarryAmount` | named | +0x30c; ctor -> mCarryAmount | 0..20 (3 distinct) |
| 0x310 | s32 | `mUnused16` | unused | +0x310; not read by the EEUnitClass ctor | 0..2 (2 distinct) |
| 0x314 | s32 | `mMaxObjectCarry` | named | +0x314; ctor -> mMaxObjectCarry | const 0 |
| 0x318 | bool | `mIsScenarioSelectable` | named | +0x318; ctor -> mIsScenarioSelectable | 0..1 (2 distinct) |
| 0x319 | bool | `mUnusedFlag6` | unused | +0x319; ctor -> mB2FA | const 0 |
| 0x31a | bool | `mBuildsOnElevation` | named | +0x31a; ctor -> mBuildsOnElevation | 0..1 (2 distinct) |
| 0x31b | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x31c | s32 | `mUnused17` | unused | +0x31c; not read by the EEUnitClass ctor | 0..4 (5 distinct) |
| 0x320 | s32 | `mUnused18` | unused | +0x320; not read by the EEUnitClass ctor | 0..50 (4 distinct) |
| 0x324 | s32 | `mScaffoldingType` | named | +0x324 | 0..827 (5 distinct) |
| 0x328 | s32 | `mDeathType` | named | +0x328 | 0..364 (4 distinct) |
| 0x32c | s32 | `mUnusedSelectionValue` | unused | +0x32c; ctor -> mL294 | 0..250 (3 distinct) |
| 0x330 | s32 | `mCloakRadius` | named | +0x330; ctor -> mCloakRadius | 0..14 (5 distinct) |
| 0x334 | s32 | `mMiniMapColor` | named | +0x334; ctor -> mMiniMapColor | 0..68 (8 distinct) |
| 0x338 | s32 | `mGarrisonCapacity` | named | +0x338; ctor -> mGarrisonCapacity | 0..50 (6 distinct) |
| 0x33c | s32 | `mInitialGarrisonCount` | named | +0x33c; ctor -> mInitialGarrisonCount | 0..15 (4 distinct) |
| 0x340 | s32 | `mGarrisonTechnology` | named | +0x340; ctor -> mGarrisonTechnology | -1..1309 (5 distinct) |
| 0x344 | s32 | `mAirTrafficType` | named | +0x344; ctor -> mAirTrafficType | 0..4 (5 distinct) |
| 0x348 | s32 | `mOverlapUnitSet` | named | +0x348; ctor -> mOverlapUnitSet | 0..109 (3 distinct) |
| 0x34c | s32 | `mWonderBonus` | named | +0x34c; ctor -> mWonderBonus | 0..6 (7 distinct) |
| 0x350 | s32 | `mWonderUnitSet` | named | +0x350; ctor -> mWonderUnitSet | 0..101 (6 distinct) |
| 0x354 | f32 | `mSlopeSpeedPenalty` | named | +0x354; ctor -> mF18C | 0..1 (4 distinct) |
| 0x358 | f32 | `mBobHeight` | named | +0x358 (EEUnitClass +0x190); ctor -> mBobHeight | 0..0.12 (10 distinct) |
| 0x35c | f32 | `mFriendlyDamageFactor` | named | +0x35c (a float: EEUnitClass::mFriendlyDamageFactor's source); ctor -> mFriendlyDamageFactor | 0..1 (4 distinct) |
| 0x360 | f32 | `mGarrisonBonus` | named | +0x360 (-> EEUnitClass::mGarrisonBonus); ctor -> mGarrisonBonus | 0..0.05 (5 distinct) |
| 0x364 | s32 | `mUnused19` | unused | +0x364; not read by the EEUnitClass ctor | 0..1087373312 (17 distinct) |
| 0x368 | f32 | `mUnusedDefaultedValue` | unused | +0x368; ctor -> mF19C | const 0 |
| 0x36c | bool | `mIsInactive` | named | +0x36c; ctor -> mIsInactive | 0..1 (2 distinct) |
| 0x36d | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..16 (3 distinct) |
| 0x36e | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x36f | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x370 | s32 | `mInfluence` | named | +0x370; ctor -> mInfluence | 0..600000 (67 distinct) |
| 0x374 | s32 | `mNumAttributes` | named | +0x374 | 0..115 (26 distinct) |
| 0x378 | s32 | `mTechnology` | named | +0x378; ctor -> mTechnology | 92..100000 (468 distinct) |
| 0x37c | bool | `mBlocksTile` | named | +0x37c; ctor -> mBlocksTile | 0..1 (2 distinct) |
| 0x37d | bool | `mIsCloakable` | named | +0x37d; ctor -> mIsCloakable | 0..1 (2 distinct) |
| 0x37e | bool | `mIsExpansionUnit` | named | +0x37e; ctor -> mB30C | 0..1 (2 distinct) |
| 0x37f | bool | `mCanEnterAirports` | named | +0x37f; ctor -> mCanEnterAirports | 0..1 (2 distinct); 0xCC in 110 |
| 0x380 | bool | `mCanEnterCarriers` | named | +0x380; ctor -> mCanEnterCarriers | 0..1 (2 distinct); 0xCC in 110 |
| 0x381 | s8 | `mUnusedFlag5` | unused | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x382 | bool | `mCanGarrison` | named | +0x382; ctor -> mCanGarrison | 0..1 (2 distinct); 0xCC in 110 |
| 0x383 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x384 | s32 | `mDeathSpawnType` | named | +0x384 | 0..662 (2 distinct) |
| 0x388 | s32 | `mPower` | named | +0x388; ctor -> mPower | 0..150 (3 distinct) |
| 0x38c | s32 | `mPowerRegeneration` | named | +0x38c; ctor -> mPowerRegeneration | 0..6 (7 distinct) |
| 0x390 | s32 | `mBehavior` | named | +0x390; ctor -> mBehavior | 0..7 (6 distinct) |
| 0x394 | s32 | `mMinConversionTime` | named | +0x394; ctor -> mMinConversionTime | 0..6000 (5 distinct) |
| 0x398 | f32 | `mHealDelay` | named | +0x398; ctor -> mHealDelay | 0..2 (3 distinct) |
| 0x39c | f32 | `mHealRange` | named | +0x39c; ctor -> mHealRange | 0..7 (6 distinct) |
| 0x3a0 | f32 | `mHealThreshold` | named | +0x3a0; ctor -> mHealThreshold | 0..1 (2 distinct) |
| 0x3a4 | f32 | `mLoadRange` | named | +0x3a4; ctor -> mLoadRange | 0..3 (4 distinct) |
| 0x3a8 | f32 | `mUnloadRange` | named | +0x3a8; ctor -> mUnloadRange | 0..99 (6 distinct) |
| 0x3ac | bool | `mCanCrossForest` | named | +0x3ac; ctor -> mB315 | 0..1 (2 distinct) |
| 0x3ad | bool | `mCanDisband` | named | +0x3ad; ctor -> mCanDisband | const 0 |
| 0x3ae | bool | `mIsObstacle` | named | +0x3ae; ctor -> mIsObstacle | 0..1 (2 distinct) |
| 0x3af | bool | `mCanAttackGround` | named | +0x3af; ctor -> mCanAttackGround | 0..1 (2 distinct) |
| 0x3b0 | bool | `mIsMelee` | named | +0x3b0; ctor -> mIsMelee | 0..1 (2 distinct) |
| 0x3b1 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | 0..122 (3 distinct) |
| 0x3b2 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x3b3 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x3b4 | s32 | `mUnusedConverterScratch` | unused | +0x3b4; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x3b8 | s32 | `mUnusedConverterScratch+0x4` | unused | +0x3b8; not read by the EEUnitClass ctor | 0..8585728 (2 distinct); 0xCC in 110 |
| 0x3bc | s32 | `mUnusedConverterScratch+0x8` | unused | +0x3bc; not read by the EEUnitClass ctor | 0..4098 (3 distinct); 0xCC in 110 |
| 0x3c0 | s32 | `mUnusedConverterScratch+0xc` | unused | +0x3c0; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x3c4 | s32 | `mUnusedConverterScratch+0x10` | unused | +0x3c4; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x3c8 | s32 | `mUnusedConverterScratch+0x14` | unused | +0x3c8; not read by the EEUnitClass ctor | 0..8716800 (2 distinct); 0xCC in 110 |
| 0x3cc | s32 | `mUnusedConverterScratch+0x18` | unused | +0x3cc; not read by the EEUnitClass ctor | 0..4098 (3 distinct); 0xCC in 110 |
| 0x3d0 | s32 | `mUnusedConverterScratch+0x1c` | unused | +0x3d0; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x3d4 | s32 | `mUnusedConverterScratch+0x20` | unused | +0x3d4; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x3d8 | s32 | `mUnusedConverterScratch+0x24` | unused | +0x3d8; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x3dc | s32 | `mUnusedConverterScratch+0x28` | unused | +0x3dc; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x3e0 | s32 | `mUnusedConverterScratch+0x2c` | unused | +0x3e0; not read by the EEUnitClass ctor | 0..8913408 (2 distinct); 0xCC in 110 |
| 0x3e4 | s32 | `mUnusedConverterScratch+0x30` | unused | +0x3e4; not read by the EEUnitClass ctor | 0..4098 (3 distinct); 0xCC in 110 |
| 0x3e8 | s32 | `mUnusedConverterScratch+0x34` | unused | +0x3e8; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x3ec | s32 | `mUnusedConverterScratch+0x38` | unused | +0x3ec; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x3f0 | s32 | `mUnusedConverterScratch+0x3c` | unused | +0x3f0; not read by the EEUnitClass ctor | 0..9044480 (2 distinct); 0xCC in 110 |
| 0x3f4 | s32 | `mUnusedConverterScratch+0x40` | unused | +0x3f4; not read by the EEUnitClass ctor | 0..4098 (3 distinct); 0xCC in 110 |
| 0x3f8 | s32 | `mUnusedConverterScratch+0x44` | unused | +0x3f8; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x3fc | s32 | `mUnusedConverterScratch+0x48` | unused | +0x3fc; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x400 | s32 | `mUnusedConverterScratch+0x4c` | unused | +0x400; not read by the EEUnitClass ctor | 0..9175552 (2 distinct); 0xCC in 110 |
| 0x404 | s32 | `mUnusedConverterScratch+0x50` | unused | +0x404; not read by the EEUnitClass ctor | 0..4098 (3 distinct); 0xCC in 110 |
| 0x408 | s32 | `mUnusedConverterScratch+0x54` | unused | +0x408; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x40c | s32 | `mUnusedConverterScratch+0x58` | unused | +0x40c; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x410 | s32 | `mUnusedConverterScratch+0x5c` | unused | +0x410; not read by the EEUnitClass ctor | 0..9306624 (2 distinct); 0xCC in 110 |
| 0x414 | s32 | `mUnusedConverterScratch+0x60` | unused | +0x414; not read by the EEUnitClass ctor | -859045684..2038003968 (113 distinct) |
| 0x418 | s32 | `mUnusedConverterScratch+0x64` | unused | +0x418; not read by the EEUnitClass ctor | 0..544108388 (14 distinct); 0xCC in 110 |
| 0x41c | s32 | `mUnusedConverterScratch+0x68` | unused | +0x41c; not read by the EEUnitClass ctor | 0..1702259020 (2 distinct); 0xCC in 110 |
| 0x420 | s32 | `mUnusedConverterScratch+0x6c` | unused | +0x420; not read by the EEUnitClass ctor | 0..9437696 (3 distinct); 0xCC in 110 |
| 0x424 | s32 | `mUnusedConverterScratch+0x70` | unused | +0x424; not read by the EEUnitClass ctor | 0..4098 (3 distinct); 0xCC in 110 |
| 0x428 | s32 | `mUnusedConverterScratch+0x74` | unused | +0x428; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x42c | s32 | `mUnusedConverterScratch+0x78` | unused | +0x42c; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x430 | s32 | `mUnusedConverterScratch+0x7c` | unused | +0x430; not read by the EEUnitClass ctor | 0..9568768 (2 distinct); 0xCC in 110 |
| 0x434 | s32 | `mUnusedConverterScratch+0x80` | unused | +0x434; not read by the EEUnitClass ctor | 0..4098 (3 distinct); 0xCC in 110 |
| 0x438 | s32 | `mUnusedConverterScratch+0x84` | unused | +0x438; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x43c | s32 | `mUnusedConverterScratch+0x88` | unused | +0x43c; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x440 | s32 | `mUnusedConverterScratch+0x8c` | unused | +0x440; not read by the EEUnitClass ctor | 0..9699840 (2 distinct); 0xCC in 110 |
| 0x444 | s32 | `mUnusedConverterScratch+0x90` | unused | +0x444; not read by the EEUnitClass ctor | 0..4098 (3 distinct); 0xCC in 110 |
| 0x448 | s32 | `mUnusedConverterScratch+0x94` | unused | +0x448; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x44c | s32 | `mUnusedConverterScratch+0x98` | unused | +0x44c; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x450 | s32 | `mUnusedConverterScratch+0x9c` | unused | +0x450; not read by the EEUnitClass ctor | 0..9830912 (2 distinct); 0xCC in 110 |
| 0x454 | s32 | `mUnusedConverterScratch+0xa0` | unused | +0x454; not read by the EEUnitClass ctor | 0..4098 (3 distinct); 0xCC in 110 |
| 0x458 | s32 | `mUnusedConverterScratch+0xa4` | unused | +0x458; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x45c | s32 | `mUnusedConverterScratch+0xa8` | unused | +0x45c; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x460 | s32 | `mUnusedConverterScratch+0xac` | unused | +0x460; not read by the EEUnitClass ctor | 0..9961984 (2 distinct); 0xCC in 110 |
| 0x464 | s32 | `mUnusedConverterScratch+0xb0` | unused | +0x464; not read by the EEUnitClass ctor | 0..4098 (3 distinct); 0xCC in 110 |
| 0x468 | s32 | `mUnusedConverterScratch+0xb4` | unused | +0x468; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x46c | s32 | `mUnusedConverterScratch+0xb8` | unused | +0x46c; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x470 | s32 | `mUnusedConverterScratch+0xbc` | unused | +0x470; not read by the EEUnitClass ctor | 0..10093056 (2 distinct); 0xCC in 110 |
| 0x474 | s32 | `mUnusedConverterScratch+0xc0` | unused | +0x474; not read by the EEUnitClass ctor | 0..4098 (3 distinct); 0xCC in 110 |
| 0x478 | s32 | `mUnusedConverterScratch+0xc4` | unused | +0x478; not read by the EEUnitClass ctor | const 0; 0xCC in 110 |
| 0x47c | s32 | `mPackedType` | named | +0x47c | const 0 |
| 0x480 | s32 | `mRemainsType` | named | +0x480; not read by the EEUnitClass ctor | const 0 |
| 0x484 | u32[200] | `mAttributes` | named | +0x484 mNumAttributes technology ids (the ctor copies them into mResearchTechnologies); ctor -> mResearchTechnologies[i] | [0]: 1019..4294967295 (69 distinct) |
| 0x7a4 | bool | `mHasSpecialAttack` | named | +0x7a4; ctor -> mHasSpecialAttack | 0..1 (2 distinct) |
| 0x7a5 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x7a6 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |
| 0x7a7 | s8 | `(padding)` | pad | padding byte (0, or 0xCC in holes); not read | -52..0 (2 distinct) |

### 3.2 `db\dbgraphics.dat`: Graphic objects (models + textures + animation states)

* **Loader:** `EEGraphicObject::LoadTable` 0x4d7833 + `EELoadGraphicObjectRecord` 0x4d7b37 (src/Empire Earth/Data Pre-Loading Thread/EEGraphicObject.cpp)
* **Container:** `u32 count`; per entry: **33 strings** (`u32 len` + bytes, no NUL): [0] row name (discarded), [1] model file (`mFileName`, e.g. `models\gun_80mmAA_10`), [2] texture (`mTextureName`), then 14 x ([texture, model] for animation state k), then shadow model and shadow texture. These are followed by the 716-byte `EEGraphicObjectRecord`.
* **AoC data:** 991 records, 936,188 bytes unpacked. Bytes per record: read 68%, label 14%, unused 7%, padding 12%.
* **Semantics:** Entries with `mMediaIndex == 0` are deleted after loading (23 rows). The 14 state blocks are stored in the order 0, 13, 7, 8, 4, 5, 12, 11, 9, 10, 6, 3, 1, 2. Each block holds sharesFrames, firstFrame, lastFrame, frameTime, loops, holdTime, then 2 x (sound frame, sound index), then isSpeedScaled; blocks 4 and 5 have no sound fields. The flags of neighbouring blocks share dwords, so the field order is irregular. The meaning of the state slots is not decoded; the model names suggest: 0 hunt/attack, 1 build, 2 carry, 3 chop, 4/5 death (`gen_deathT`), 6 farm, 7/8 idle/fidget, 9 mine/gate, 10 forage/diagonal, 11 attack, 12 unused, 13 walk (walls reuse the slots for wall pieces). `mHoldTime0` is replaced by 300000 at load. Graphic-effect tag-point kinds are named in `gTagPointKindNames[0x48]`.
* **Cross-references:** `mSound1IndexN`/`mSound2IndexN` -> dbsounds (-1 = none); model/texture strings -> `models\*.cem`, `textures\*.sst|.tga`; referenced by dbobjects `mGraphic`, dbgfxeffects `mMediaIndex`, dbeffects `mMediaIndex`, dbareaeffect `mMedia`, dbanimals `mMedia`.
* **Suggested text form:** YAML

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | INFERRED from data: the row name (char[100], NUL-terminated) | text 968/991, max len 44, junk after NUL in 41, e.g. "AA Gun 88" |
| 0x064 | s32 | `mUnusedDatabaseID` | unused | INFERRED: unique per row, gappy: the database row id | 0..1328 (969 distinct) |
| 0x068 | s32 | `mMediaIndex` | named | +0x68 | 0..990 (969 distinct) |
| 0x06c | bool | `mIsDrawn` | named | +0x6c | 0..1 (2 distinct) |
| 0x06d | pad | `(padding)` | pad | alignment padding | bytes 000000x968, ccccccx23 |
| 0x070 | s32 | `mUnusedBuildingValue1` | unused | +0x70 | 0..127 (10 distinct) |
| 0x074 | s32 | `mUnusedBuildingValue2` | unused | +0x74 | 0..385 (8 distinct) |
| 0x078 | bool | `mSharesFrames0` | named | +0x78 | 0..1 (2 distinct) |
| 0x079 | pad | `(padding)` | pad | alignment padding | bytes 000000x960, ccccccx23 |
| 0x07c | u32 | `mFirstFrame0` | named | +0x7c | 0..46 (26 distinct) |
| 0x080 | u32 | `mLastFrame0` | named | +0x80 | 0..59 (44 distinct) |
| 0x084 | u32 | `mFrameTime0` | named | +0x84 | 0..300000 (45 distinct) |
| 0x088 | bool | `mLoops0` | named | +0x88 | 0..1 (2 distinct) |
| 0x089 | pad | `(padding)` | pad | alignment padding | bytes 000000x968, ccccccx23 |
| 0x08c | u32 | `mUnusedHoldTime0` | unused | +0x8c (not read: the loader sets mCamouflageTimer[0] to 300000) | 0..60000 (7 distinct) |
| 0x090 | u32 | `mSound1Frame0` | named | +0x90 | 0..54 (42 distinct) |
| 0x094 | u32 | `mSound2Frame0` | named | +0x94 | 0..39 (2 distinct) |
| 0x098 | u32 | `mSound1Index0` | named | +0x98 | 2..4294967295 (71 distinct) |
| 0x09c | u32 | `mSound2Index0` | named | +0x9c | 238..4294967295 (4 distinct) |
| 0x0a0 | bool | `mIsSpeedScaled0` | named | +0xa0 | 0..1 (2 distinct) |
| 0x0a1 | bool | `mSharesFrames13` | named | +0xa1 | 0..1 (2 distinct) |
| 0x0a2 | pad | `(padding)` | pad | alignment padding | bytes 0000x961, ccccx23 |
| 0x0a4 | u32 | `mFirstFrame13` | named | +0xa4 | 0..1 (2 distinct) |
| 0x0a8 | u32 | `mLastFrame13` | named | +0xa8 | 0..34 (16 distinct) |
| 0x0ac | u32 | `mFrameTime13` | named | +0xac | 0..420 (54 distinct) |
| 0x0b0 | bool | `mLoops13` | named | +0xb0 | 0..1 (2 distinct) |
| 0x0b1 | pad | `(padding)` | pad | alignment padding | bytes 000000x968, ccccccx23 |
| 0x0b4 | u32 | `mHoldTime13` | named | +0xb4 | const 0 |
| 0x0b8 | u32 | `mSound1Frame13` | named | +0xb8 | 0..7 (5 distinct) |
| 0x0bc | u32 | `mSound2Frame13` | named | +0xbc | 0..22 (6 distinct) |
| 0x0c0 | u32 | `mSound1Index13` | named | +0xc0 | 7..4294967295 (39 distinct) |
| 0x0c4 | u32 | `mSound2Index13` | named | +0xc4 | 166..4294967295 (3 distinct) |
| 0x0c8 | bool | `mIsSpeedScaled13` | named | +0xc8 | 0..1 (2 distinct) |
| 0x0c9 | bool | `mSharesFrames7` | named | +0xc9 | 0..1 (2 distinct) |
| 0x0ca | pad | `(padding)` | pad | alignment padding | bytes 0000x968, ccccx23 |
| 0x0cc | u32 | `mFirstFrame7` | named | +0xcc | 0..58 (21 distinct) |
| 0x0d0 | u32 | `mLastFrame7` | named | +0xd0 | 0..71 (34 distinct) |
| 0x0d4 | u32 | `mFrameTime7` | named | +0xd4 | 0..400 (30 distinct) |
| 0x0d8 | bool | `mLoops7` | named | +0xd8 | 0..1 (2 distinct) |
| 0x0d9 | pad | `(padding)` | pad | alignment padding | bytes 000000x968, ccccccx23 |
| 0x0dc | u32 | `mHoldTime7` | named | +0xdc | 0..50000 (25 distinct) |
| 0x0e0 | u32 | `mSound1Frame7` | named | +0xe0 | 0..43 (8 distinct) |
| 0x0e4 | u32 | `mSound2Frame7` | named | +0xe4 | const 0 |
| 0x0e8 | u32 | `mSound1Index7` | named | +0xe8 | 41..4294967295 (8 distinct) |
| 0x0ec | u32 | `mSound2Index7` | named | +0xec | const 4294967295 |
| 0x0f0 | bool | `mIsSpeedScaled7` | named | +0xf0 | const 0 |
| 0x0f1 | bool | `mSharesFrames8` | named | +0xf1 | 0..1 (2 distinct) |
| 0x0f2 | pad | `(padding)` | pad | alignment padding | bytes 0000x968, ccccx23 |
| 0x0f4 | u32 | `mFirstFrame8` | named | +0xf4 | 0..58 (26 distinct) |
| 0x0f8 | u32 | `mLastFrame8` | named | +0xf8 | 0..71 (37 distinct) |
| 0x0fc | u32 | `mFrameTime8` | named | +0xfc | 0..400 (30 distinct) |
| 0x100 | bool | `mLoops8` | named | +0x100 | 0..1 (2 distinct) |
| 0x101 | pad | `(padding)` | pad | alignment padding | bytes 000000x961, ccccccx23 |
| 0x104 | u32 | `mHoldTime8` | named | +0x104 | 0..50000 (20 distinct) |
| 0x108 | u32 | `mSound1Frame8` | named | +0x108 | 0..45 (19 distinct) |
| 0x10c | u32 | `mSound2Frame8` | named | +0x10c | const 0 |
| 0x110 | u32 | `mSound1Index8` | named | +0x110 | 9..4294967295 (16 distinct) |
| 0x114 | u32 | `mSound2Index8` | named | +0x114 | const 4294967295 |
| 0x118 | bool | `mIsSpeedScaled8` | named | +0x118 | const 0 |
| 0x119 | bool | `mSharesFrames4` | named | +0x119 | 0..1 (2 distinct) |
| 0x11a | pad | `(padding)` | pad | alignment padding | bytes 0000x968, ccccx23 |
| 0x11c | u32 | `mFirstFrame4` | named | +0x11c | 0..81 (49 distinct) |
| 0x120 | u32 | `mLastFrame4` | named | +0x120 | 0..95 (54 distinct) |
| 0x124 | u32 | `mFrameTime4` | named | +0x124 | 0..320 (53 distinct) |
| 0x128 | bool | `mLoops4` | named | +0x128 | 0..1 (2 distinct) |
| 0x129 | pad | `(padding)` | pad | alignment padding | bytes 000000x968, ccccccx23 |
| 0x12c | u32 | `mHoldTime4` | named | +0x12c | const 0 |
| 0x130 | bool | `mIsSpeedScaled4` | named | +0x130 | const 0 |
| 0x131 | bool | `mSharesFrames5` | named | +0x131 | 0..1 (2 distinct) |
| 0x132 | pad | `(padding)` | pad | alignment padding | bytes 0000x968, ccccx23 |
| 0x134 | u32 | `mFirstFrame5` | named | +0x134 | 0..98 (54 distinct) |
| 0x138 | u32 | `mLastFrame5` | named | +0x138 | 0..98 (55 distinct) |
| 0x13c | u32 | `mFrameTime5` | named | +0x13c | 0..1000 (9 distinct) |
| 0x140 | bool | `mLoops5` | named | +0x140 | 0..1 (2 distinct) |
| 0x141 | pad | `(padding)` | pad | alignment padding | bytes 000000x959, ccccccx23 |
| 0x144 | u32 | `mHoldTime5` | named | +0x144 | 0..100 (2 distinct) |
| 0x148 | bool | `mIsSpeedScaled5` | named | +0x148 | const 0 |
| 0x149 | bool | `mSharesFrames12` | named | +0x149 | 0..1 (2 distinct) |
| 0x14a | pad | `(padding)` | pad | alignment padding | bytes 0000x968, ccccx23 |
| 0x14c | u32 | `mFirstFrame12` | named | +0x14c | const 0 |
| 0x150 | u32 | `mLastFrame12` | named | +0x150 | const 0 |
| 0x154 | u32 | `mFrameTime12` | named | +0x154 | const 0 |
| 0x158 | bool | `mLoops12` | named | +0x158 | const 0 |
| 0x159 | pad | `(padding)` | pad | alignment padding | bytes 000000x968, ccccccx23 |
| 0x15c | u32 | `mHoldTime12` | named | +0x15c | const 0 |
| 0x160 | u32 | `mSound1Frame12` | named | +0x160 | const 0 |
| 0x164 | u32 | `mSound2Frame12` | named | +0x164 | const 0 |
| 0x168 | u32 | `mSound1Index12` | named | +0x168 | const 4294967295 |
| 0x16c | u32 | `mSound2Index12` | named | +0x16c | const 4294967295 |
| 0x170 | bool | `mIsSpeedScaled12` | named | +0x170 | const 0 |
| 0x171 | bool | `mSharesFrames11` | named | +0x171 | 0..1 (2 distinct) |
| 0x172 | pad | `(padding)` | pad | alignment padding | bytes 0000x968, ccccx23 |
| 0x174 | u32 | `mFirstFrame11` | named | +0x174 | 0..53 (16 distinct) |
| 0x178 | u32 | `mLastFrame11` | named | +0x178 | 0..61 (19 distinct) |
| 0x17c | u32 | `mFrameTime11` | named | +0x17c | 0..300 (24 distinct) |
| 0x180 | bool | `mLoops11` | named | +0x180 | 0..1 (2 distinct) |
| 0x181 | pad | `(padding)` | pad | alignment padding | bytes 000000x963, ccccccx23 |
| 0x184 | u32 | `mHoldTime11` | named | +0x184 | 0..100000 (14 distinct) |
| 0x188 | u32 | `mSound1Frame11` | named | +0x188 | 0..51 (17 distinct) |
| 0x18c | u32 | `mSound2Frame11` | named | +0x18c | const 0 |
| 0x190 | u32 | `mSound1Index11` | named | +0x190 | 22..4294967295 (17 distinct) |
| 0x194 | u32 | `mSound2Index11` | named | +0x194 | const 4294967295 |
| 0x198 | bool | `mIsSpeedScaled11` | named | +0x198 | const 0 |
| 0x199 | bool | `mSharesFrames9` | named | +0x199 | 0..1 (2 distinct) |
| 0x19a | pad | `(padding)` | pad | alignment padding | bytes 0000x968, ccccx23 |
| 0x19c | u32 | `mFirstFrame9` | named | +0x19c | 0..67 (6 distinct) |
| 0x1a0 | u32 | `mLastFrame9` | named | +0x1a0 | 0..80 (8 distinct) |
| 0x1a4 | u32 | `mFrameTime9` | named | +0x1a4 | 0..200 (8 distinct) |
| 0x1a8 | bool | `mLoops9` | named | +0x1a8 | 0..1 (2 distinct) |
| 0x1a9 | pad | `(padding)` | pad | alignment padding | bytes 000000x963, ccccccx23 |
| 0x1ac | u32 | `mHoldTime9` | named | +0x1ac | 0..1000 (8 distinct) |
| 0x1b0 | u32 | `mSound1Frame9` | named | +0x1b0 | 0..72 (9 distinct) |
| 0x1b4 | u32 | `mSound2Frame9` | named | +0x1b4 | const 0 |
| 0x1b8 | u32 | `mSound1Index9` | named | +0x1b8 | 172..4294967295 (6 distinct) |
| 0x1bc | u32 | `mSound2Index9` | named | +0x1bc | const 4294967295 |
| 0x1c0 | bool | `mIsSpeedScaled9` | named | +0x1c0 | const 0 |
| 0x1c1 | bool | `mSharesFrames10` | named | +0x1c1 | 0..1 (2 distinct) |
| 0x1c2 | pad | `(padding)` | pad | alignment padding | bytes 0000x967, ccccx23 |
| 0x1c4 | u32 | `mFirstFrame10` | named | +0x1c4 | 0..31 (2 distinct) |
| 0x1c8 | u32 | `mLastFrame10` | named | +0x1c8 | 0..41 (3 distinct) |
| 0x1cc | u32 | `mFrameTime10` | named | +0x1cc | 0..166 (8 distinct) |
| 0x1d0 | bool | `mLoops10` | named | +0x1d0 | 0..1 (2 distinct) |
| 0x1d1 | pad | `(padding)` | pad | alignment padding | bytes 000000x968, ccccccx23 |
| 0x1d4 | u32 | `mHoldTime10` | named | +0x1d4 | 0..1000 (2 distinct) |
| 0x1d8 | u32 | `mSound1Frame10` | named | +0x1d8 | 0..31 (3 distinct) |
| 0x1dc | u32 | `mSound2Frame10` | named | +0x1dc | const 0 |
| 0x1e0 | u32 | `mSound1Index10` | named | +0x1e0 | 236..4294967295 (2 distinct) |
| 0x1e4 | u32 | `mSound2Index10` | named | +0x1e4 | const 4294967295 |
| 0x1e8 | bool | `mIsSpeedScaled10` | named | +0x1e8 | const 0 |
| 0x1e9 | bool | `mSharesFrames6` | named | +0x1e9 | 0..1 (2 distinct) |
| 0x1ea | pad | `(padding)` | pad | alignment padding | bytes 0000x965, ccccx23 |
| 0x1ec | u32 | `mFirstFrame6` | named | +0x1ec | 0..48 (8 distinct) |
| 0x1f0 | u32 | `mLastFrame6` | named | +0x1f0 | 0..61 (8 distinct) |
| 0x1f4 | u32 | `mFrameTime6` | named | +0x1f4 | 0..500 (9 distinct) |
| 0x1f8 | bool | `mLoops6` | named | +0x1f8 | 0..1 (2 distinct) |
| 0x1f9 | pad | `(padding)` | pad | alignment padding | bytes 000000x968, ccccccx23 |
| 0x1fc | u32 | `mHoldTime6` | named | +0x1fc | 0..500 (5 distinct) |
| 0x200 | u32 | `mSound1Frame6` | named | +0x200 | 0..57 (9 distinct) |
| 0x204 | u32 | `mSound2Frame6` | named | +0x204 | const 0 |
| 0x208 | u32 | `mSound1Index6` | named | +0x208 | 175..4294967295 (2 distinct) |
| 0x20c | u32 | `mSound2Index6` | named | +0x20c | const 4294967295 |
| 0x210 | bool | `mIsSpeedScaled6` | named | +0x210 | const 0 |
| 0x211 | bool | `mSharesFrames3` | named | +0x211 | 0..1 (2 distinct) |
| 0x212 | pad | `(padding)` | pad | alignment padding | bytes 0000x968, ccccx23 |
| 0x214 | u32 | `mFirstFrame3` | named | +0x214 | 0..41 (6 distinct) |
| 0x218 | u32 | `mLastFrame3` | named | +0x218 | 0..57 (12 distinct) |
| 0x21c | u32 | `mFrameTime3` | named | +0x21c | 0..400 (22 distinct) |
| 0x220 | bool | `mLoops3` | named | +0x220 | 0..1 (2 distinct) |
| 0x221 | pad | `(padding)` | pad | alignment padding | bytes 000000x968, ccccccx23 |
| 0x224 | u32 | `mHoldTime3` | named | +0x224 | 0..50000 (13 distinct) |
| 0x228 | u32 | `mSound1Frame3` | named | +0x228 | 0..29 (7 distinct) |
| 0x22c | u32 | `mSound2Frame3` | named | +0x22c | const 0 |
| 0x230 | u32 | `mSound1Index3` | named | +0x230 | 34..4294967295 (5 distinct) |
| 0x234 | u32 | `mSound2Index3` | named | +0x234 | const 4294967295 |
| 0x238 | bool | `mIsSpeedScaled3` | named | +0x238 | const 0 |
| 0x239 | bool | `mSharesFrames1` | named | +0x239 | 0..1 (2 distinct) |
| 0x23a | pad | `(padding)` | pad | alignment padding | bytes 0000x968, ccccx23 |
| 0x23c | u32 | `mFirstFrame1` | named | +0x23c | 0..41 (6 distinct) |
| 0x240 | u32 | `mLastFrame1` | named | +0x240 | 0..57 (6 distinct) |
| 0x244 | u32 | `mFrameTime1` | named | +0x244 | 0..200 (11 distinct) |
| 0x248 | bool | `mLoops1` | named | +0x248 | 0..1 (2 distinct) |
| 0x249 | pad | `(padding)` | pad | alignment padding | bytes 000000x960, ccccccx23 |
| 0x24c | u32 | `mHoldTime1` | named | +0x24c | 0..250 (3 distinct) |
| 0x250 | u32 | `mSound1Frame1` | named | +0x250 | 0..43 (7 distinct) |
| 0x254 | u32 | `mSound2Frame1` | named | +0x254 | const 0 |
| 0x258 | u32 | `mSound1Index1` | named | +0x258 | 171..4294967295 (3 distinct) |
| 0x25c | u32 | `mSound2Index1` | named | +0x25c | const 4294967295 |
| 0x260 | bool | `mIsSpeedScaled1` | named | +0x260 | const 0 |
| 0x261 | bool | `mSharesFrames2` | named | +0x261 | 0..1 (2 distinct) |
| 0x262 | pad | `(padding)` | pad | alignment padding | bytes 0000x968, ccccx23 |
| 0x264 | u32 | `mFirstFrame2` | named | +0x264 | 0..18 (2 distinct) |
| 0x268 | u32 | `mLastFrame2` | named | +0x268 | 0..24 (3 distinct) |
| 0x26c | u32 | `mFrameTime2` | named | +0x26c | 0..180 (9 distinct) |
| 0x270 | bool | `mLoops2` | named | +0x270 | 0..1 (2 distinct) |
| 0x271 | pad | `(padding)` | pad | alignment padding | bytes 000000x968, ccccccx23 |
| 0x274 | u32 | `mHoldTime2` | named | +0x274 | 0..1 (2 distinct) |
| 0x278 | u32 | `mSound1Frame2` | named | +0x278 | 0..8 (2 distinct) |
| 0x27c | u32 | `mSound2Frame2` | named | +0x27c | 0..15 (3 distinct) |
| 0x280 | u32 | `mSound1Index2` | named | +0x280 | 39..4294967295 (6 distinct) |
| 0x284 | u32 | `mSound2Index2` | named | +0x284 | 39..4294967295 (5 distinct) |
| 0x288 | bool | `mIsSpeedScaled2` | named | +0x288 | const 0 |
| 0x289 | pad | `(padding)` | pad | alignment padding | bytes 000000x961, ccccccx23 |
| 0x28c | s32 | `mUnusedIsBuilding` | unused | +0x28c | 0..1 (2 distinct) |
| 0x290 | bool | `mUnusedIsSprite` | unused | +0x290 | 0..1 (2 distinct) |
| 0x291 | s32 | `(unused gap)` | unused | undecoded | -1778384896..1677721600 (8 distinct) |
| 0x295 | pad | `(unused gap)` | unused | undecoded | bytes 000000x990, 930400x1 |
| 0x298 | s32 | `mUnusedDummyValue` | unused | +0x298 | 0..100 (2 distinct) |
| 0x29c | s32 | `mUnusedBaseGraphic` | unused | +0x29c | -1..938 (78 distinct) |
| 0x2a0 | s32 | `mAlphaReference` | named | +0x2a0 | 0..100 (6 distinct) |
| 0x2a4 | s32 | `mUnusedBuildingIndex` | unused | +0x2a4 | 0..246 (46 distinct) |
| 0x2a8 | s32 | `(unused gap)` | unused | undecoded | const -1 |
| 0x2ac | s32 | `mKind` | named | +0x2ac | -1..70 (20 distinct) |
| 0x2b0 | bool | `mHasAlpha` | named | +0x2b0 | 0..1 (2 distinct) |
| 0x2b1 | pad | `(padding)` | pad | alignment padding | bytes 000000x961, ccccccx23 |
| 0x2b4 | f32 | `mOpacity` | named | +0x2b4 | 0.0001..416.81 (141 distinct) |
| 0x2b8 | s32 | `(unused gap)` | unused | undecoded | 0..4000000 (91 distinct) |
| 0x2bc | f32 | `mMipMapBias` | named | +0x2bc | -1..0 (4 distinct) |
| 0x2c0 | s32 | `mNumMipLevelsToDrop` | named | +0x2c0 | const 0 |
| 0x2c4 | s32 | `mMaxMipLevelsToDrop` | named | +0x2c4 | 0..12 (4 distinct) |
| 0x2c8 | bool | `mCreateMipMaps` | named | +0x2c8 | 0..1 (2 distinct) |
| 0x2c9 | pad | `(padding)` | pad | tail padding | bytes 000000x966, ccccccx23 |

### 3.3 `db\dbsounds.dat`: Sounds

* **Loader:** `EESoundObject::LoadTable` 0x4da680 (src/Empire Earth/Data Pre-Loading Thread/EESoundObject.cpp)
* **Container:** `u32 count`; per entry: one string (`u32 len` + bytes, no NUL) = sample file (e.g. `sounds\heligun1-8.wav`), then a 132-byte `EESoundRecord`.
* **AoC data:** 1087 records, 166,734 bytes unpacked. Bytes per record: read 10%, label 76%, unused 12%, padding 2%.
* **Semantics:** Every row becomes an object, holes included (215 rows with an empty file name). `mVolume` is a percentage (x 0.01 at load). `mCategory` is the sound-manager category.
* **Cross-references:** Referenced by dbobjects, dbgraphics, dbcalamity, dbambientsounds, dbeffects (`mSoundID`) and dbanimals by row position.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | INFERRED from data: the row name (char[100], NUL-terminated) | text 872/1087, max len 33, junk after NUL in 234, e.g. "Sci-fi4" |
| 0x064 | s32 | `mUnusedDatabaseID` | unused | INFERRED: unique per row, gappy: the database row id | 0..1183 (873 distinct) |
| 0x068 | s32 | `mUnusedIndex` | unused | INFERRED: equals the 0-based row index | -1..1086 (873 distinct) |
| 0x06c | s32 | `mMaxInstances` | named | +0x6c -> +0x1c | 0..4 (5 distinct) |
| 0x070 | s32 | `mCategory` | named | +0x70 -> +0x20 | 0..6 (7 distinct) |
| 0x074 | s32 | `mUnusedPriority` | unused | +0x74 -> +0x24 | 0..50 (5 distinct) |
| 0x078 | bool | `mLooping` | named | +0x78 -> +0x2c (looping) | 0..1 (2 distinct) |
| 0x079 | pad | `(padding)` | pad | alignment padding | bytes 000000x872, ccccccx215 |
| 0x07c | s32 | `mVolume` | named | +0x7c: percent -> +0x14 (float, * 0.01) | 0..100 (27 distinct) |
| 0x080 | s32 | `(unused gap)` | unused | undecoded | const 0 |

### 3.4 `db\dbgfxeffects.dat`: Graphic effects

* **Loader:** `EEGraphicEffect::LoadTable` 0x4d6e9e, ctor 0x4d6ceb (src/Empire Earth/Data Pre-Loading Thread/EEGraphicEffect.cpp)
* **Container:** `u32 count` + count x 232-byte `EEGraphicEffectRecord`.
* **AoC data:** 292 records, 67,748 bytes unpacked. Bytes per record: read 54%, label 43%, unused 2%, padding 1%.
* **Semantics:** A row with `mID > 0` makes an effect; row 0 ("None") and the holes stay NULL. `mLinkedEffectIndex1..3` are row indices, resolved to pointers after loading. `mKind` selects the effect class; `mParam1..12` and `mL90..mL98` are class-specific parameters (provisional).
* **Cross-references:** `mMediaIndex` -> dbgraphics; `mLinkedEffectIndex1..3` -> dbgfxeffects; `mNameStringId` -> Language.dll; `mTagPointKind` -> `gTagPointKindNames` (0x48 kinds). Referenced by dbobjects (20+ slots), dbcalamity, dbareaeffect.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | INFERRED from data: the row name (char[100], NUL-terminated) | text 288/292, max len 48, junk after NUL in 9, e.g. "None" |
| 0x064 | s32 | `mUnusedDatabaseID` | unused | INFERRED: unique per row, gappy: the database row id | 0..324 (289 distinct) |
| 0x068 | s32 | `mID` | named | +0x68 -> +0x60 (> 0: a real record) | -1..291 (289 distinct) |
| 0x06c | s32 | `mKind` | named | +0x6c -> +0x58 | -1..71 (60 distinct) |
| 0x070 | s32 | `mTagPointKind` | named | +0x70 -> +0x5c | -1..21 (3 distinct) |
| 0x074 | s32 | `mMediaIndex` | named | +0x74 -> +0x0c (EEMedia::Get) | -1..987 (110 distinct) |
| 0x078 | f32 | `mStartSize` | named | +0x78 -> +0x20 | 0..10 (39 distinct) |
| 0x07c | f32 | `mHeight` | named | +0x7c -> +0x1c | 0.0005..25 (44 distinct) |
| 0x080 | f32 | `mAlpha` | named | +0x80 -> +0x24 | 0..1 (10 distinct) |
| 0x084 | s32 | `mTime1` | named | +0x84 -> +0x70 | 0..10000 (19 distinct) |
| 0x088 | s32 | `mTime2` | named | +0x88 -> +0x74 | 0..20000 (18 distinct) |
| 0x08c | s32 | `mDuration` | named | +0x8c -> +0x78 | 0..1000000 (47 distinct) |
| 0x090 | s32 | `mIntParam1` | named | +0x90 -> +0x64 | 0..99999 (29 distinct) |
| 0x094 | s32 | `mIntParam2` | named | +0x94 -> +0x68 | 0..443 (13 distinct) |
| 0x098 | s32 | `mIntParam3` | named | +0x98 -> +0x6c | 0..442 (12 distinct) |
| 0x09c | s32 | `mLinkedEffectIndex1` | named | +0x9c -> +0x10 (an index until Load links it) | -1..283 (65 distinct) |
| 0x0a0 | s32 | `mLinkedEffectIndex2` | named | +0xa0 -> +0x14 | -1..285 (24 distinct) |
| 0x0a4 | s32 | `mLinkedEffectIndex3` | named | +0xa4 -> +0x18 | -1..291 (6 distinct) |
| 0x0a8 | s32 | `mNameStringId` | named | +0xa8 -> +0x7c | 0..8817 (116 distinct) |
| 0x0ac | f32 | `mParam1` | named | +0xac -> +0x28 | -30..20000 (41 distinct) |
| 0x0b0 | f32 | `mParam2` | named | +0xb0 -> +0x2c | 0..1200 (24 distinct) |
| 0x0b4 | f32 | `mParam3` | named | +0xb4 -> +0x30 | -2.8..3.1415 (23 distinct) |
| 0x0b8 | f32 | `mParam4` | named | +0xb8 -> +0x34 | 0..6.3 (21 distinct) |
| 0x0bc | f32 | `mParam5` | named | +0xbc -> +0x38 | 0..3 (23 distinct) |
| 0x0c0 | f32 | `mParam6` | named | +0xc0 -> +0x3c | 0..300 (16 distinct) |
| 0x0c4 | f32 | `mParam7` | named | +0xc4 -> +0x40 | 0..1000 (10 distinct) |
| 0x0c8 | f32 | `mParam8` | named | +0xc8 -> +0x44 | -0.4..2 (7 distinct) |
| 0x0cc | f32 | `mParam9` | named | +0xcc -> +0x48 | 0..250 (8 distinct) |
| 0x0d0 | f32 | `mParam10` | named | +0xd0 -> +0x4c | 0..3000 (4 distinct) |
| 0x0d4 | f32 | `mParam11` | named | +0xd4 -> +0x50 | 0..4 (6 distinct) |
| 0x0d8 | f32 | `mEmitPositionSpread` | named | +0xd8 -> +0x54 | 0..3 (10 distinct) |
| 0x0dc | bool | `mFlag1` | named | +0xdc -> +0x88 | 0..1 (2 distinct) |
| 0x0dd | bool | `mFlag2` | named | +0xdd -> +0x89 | 0..1 (2 distinct) |
| 0x0de | bool | `mIsScenarioListed` | named | +0xde -> +0x8b | 0..1 (2 distinct) |
| 0x0df | bool | `mIsEmitFlat` | named | +0xdf -> +0x8c | 0..1 (2 distinct) |
| 0x0e0 | bool | `mEndsWithUnitOrDuration` | named | +0xe0 -> +0x8a | 0..1 (2 distinct) |
| 0x0e1 | pad | `(padding)` | pad | alignment padding | bytes 000000x287, ccccccx4 |
| 0x0e4 | s32 | `mDetailLevel` | named | +0xe4 -> +0x84 | 0..2 (3 distinct) |

### 3.5 `db\dbmusic.dat`: Music (songs per epoch)

* **Loader:** `EEMusicObject::LoadTable` 0x4d9f6c (src/Empire Earth/Data Pre-Loading Thread/EEMusicObject.cpp)
* **Container:** `u32 count` + count x 196-byte `EEMusicRecord`. Row layout: dbId, index, name, then the file name.
* **AoC data:** 9 records, 1,768 bytes unpacked. Bytes per record: read 42%, label 51%, unused 7%, padding 0%.
* **Semantics:** Row 0 is a hole that still gets loaded (all epoch flags are 0, so it is never played). `mFileName` + ".sgt" is the DirectMusic segment. The space epoch reuses the nano flag. If `Data\Music\New Music.txt` exists, it replaces the table (section 4). +0xb4 is an unread bool, perhaps an epoch slot.
* **Cross-references:** none (file names point at `Data\Music`).
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | s32 | `mUnusedDatabaseID` | unused | INFERRED: unique per row, gappy: the database row id | -1..9 (9 distinct) |
| 0x004 | s32 | `mUnusedIndex` | unused | INFERRED: equals the 0-based row index | -1..8 (9 distinct) |
| 0x008 | char[100] | `label` | label | INFERRED from data: the row name (char[100], NUL-terminated) | text 8/9, max len 7, junk after NUL in 1, e.g. "Pawns" |
| 0x06c | char[52] | `mFileName` | named | +0x6c (a plain name; the song object appends ".sgt") | text 8/9, max len 7, junk after NUL in 1, e.g. "Pawns" |
| 0x0a0 | f32 | `mReverbGain` | named | +0xa0 | const 0 |
| 0x0a4 | f32 | `mReverbHighFreqRTRatio` | named | +0xa4 | 0..0.45 (8 distinct) |
| 0x0a8 | f32 | `mReverbMix` | named | +0xa8 | -9..0 (4 distinct) |
| 0x0ac | f32 | `mReverbTime` | named | +0xac | 0..2800 (9 distinct) |
| 0x0b0 | s32 | `(unused gap)` | unused | undecoded | const 0 |
| 0x0b4 | pad | `(unused gap)` | unused | undecoded | bytes 00x9 |
| 0x0b5 | bool[14] | `mInEpoch` | named | +0xb5..+0xc2 | [0]: 0..1 (2 distinct) |
| 0x0c3 | s8 | `(unused gap)` | unused |  | -52..0 (2 distinct) |

### 3.6 `db\dbtechtree.dat`: Tech tree (epochs and nodes)

* **Loader:** `EETechTree::Load` 0x5cbdf0 -> `EETechTreeEpoch::Load` 0x5ce10e -> `EETechTreeNode::Load` 0x5cf370 (src/Empire Earth/Tech Tree/); record in include/ee/EETechTreeRecord.h
* **Container:** No global count. **15 epoch blocks**, each: `u32 nodeCount`, the epoch's own 268-byte record, `u32 n` + n x `u32` parent unit types, then nodeCount x (268-byte record + `u32 n` + n x `u32`). In AoC the epochs have 929/55/201/111/202/901/103/93/95/205/921/113/210/301/39 node slots (4,494 records in all). Only 793 are real: each epoch is a dense id range (e.g. epoch 0 ids 1004..1928), and the empty ids are 0xCC hole records with id 0.
* **AoC data:** 4494 records, 1,226,932 bytes unpacked. Bytes per record: read 50%, label 39%, unused 11%, padding 0%.
* **Semantics:** Nodes with `mID <= 15` (`kTechEpochAll`) are dropped. Node epoch = id / 1000 - 1. The trailing list is the unit types offering the node (the converter log calls them "Parents"); costs are scaled per parent by `mFactor1..4` (parents 2..5). The `mCost2..6` names follow the resource slots: Food=mCost2, Wood=mCost3, Stone=mCost4, Gold=mCost5, Iron=mCost6. The 0xCC words at `mUD4..mUE0`, `mUF8`, `mFirstNodeID` and in `mBB6/mBB7` appear in hole rows only.
* **Cross-references:** `mResearchedEvent`/`mObsoleteEvent` -> dbevents; `mButton` -> dbbuttons; `mUnitClass`, `mDefaultUnitClass`, parent lists -> unit types; `mPrerequisite1..4` -> node ids; `mCalamity` -> dbcalamity; `mHotKey` -> dbuihotkey; `mNameID` -> Language.dll. Referenced by dbobjects `mAttributes[]`, dbeffects `mTechnology`, dbcivpowers `mTechnology`.
* **Suggested text form:** YAML

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[104] | `label` | label | +0x00 (not read: the name?) | text 793/4494, max len 41, junk after NUL in 4494, e.g. "Epoch Paleo" |
| 0x068 | s32 | `mID` | named | +0x68 -> mU08->mU4C | 0..15038 (793 distinct) |
| 0x06c | s32 | `mEpoch` | named | +0x6c -> the epoch's mL14 | 0..14 (15 distinct) |
| 0x070 | s32 | `mObsoleteEpoch` | named | +0x70 -> mL18 | 1..15 (13 distinct) |
| 0x074 | s32 | `mResearchedEvent` | named | +0x74 -> mU08->mResearchedEvent | -1..663 (296 distinct) |
| 0x078 | s32 | `mObsoleteEvent` | named | +0x78 -> mU08->mObsoleteEvent | -1..0 (2 distinct) |
| 0x07c | s32 | `mCost3` | named | +0x7c -> mU08->mCosts[3] | 0..100000 (48 distinct) |
| 0x080 | s32 | `mCost4` | named | +0x80 -> mU08->mCosts[4] | 0..100000 (14 distinct) |
| 0x084 | s32 | `mUnused1` | unused | +0x84 | 0..100000 (2 distinct) |
| 0x088 | s32 | `mCost5` | named | +0x88 -> mU08->mCosts[5] | 0..100000 (53 distinct) |
| 0x08c | s32 | `mUnused2` | unused | +0x8c | const 0 |
| 0x090 | s32 | `mCost6` | named | +0x90 -> mU08->mCosts[6] | 0..100000 (54 distinct) |
| 0x094 | s32 | `mCost2` | named | +0x94 -> mU08->mCosts[2] | 0..100000 (59 distinct) |
| 0x098 | s32 | `mBuildTime` | named | +0x98 -> mU08->mU58 | 0..900000 (52 distinct) |
| 0x09c | s32 | `mPrerequisite1` | named | +0x9c -> mU08->mU5C | -1..15035 (237 distinct) |
| 0x0a0 | s32 | `mPrerequisite2` | named | +0xa0 -> mU08->mU60 | -1..1049 (4 distinct) |
| 0x0a4 | s32 | `mPrerequisite3` | named | +0xa4 -> mU08->mU64 | -1..0 (2 distinct) |
| 0x0a8 | s32 | `mPrerequisite4` | named | +0xa8 -> mU08->mSelectedObjects | -1..0 (2 distinct) |
| 0x0ac | s32 | `mUnitClass` | named | +0xac -> mU08->mUnitClass | -1..847 (459 distinct) |
| 0x0b0 | s32 | `mButton` | named | +0xb0 EEButtonObject::Get -> mButton | 1..1542 (685 distinct) |
| 0x0b4 | bool | `mMakesUnit` | named | +0xb4 -> mU08->mB7C | 0..1 (2 distinct) |
| 0x0b5 | bool | `mCountsTowardEpoch` | named | +0xb5 -> mU08->mB7D | 0..1 (2 distinct) |
| 0x0b6 | bool | `mUnused3` | unused | +0xb6 | const 0; 0xCC in 3701 |
| 0x0b7 | bool | `mUnused4` | unused | +0xb7 | const 0; 0xCC in 3701 |
| 0x0b8 | s32 | `mNameID` | named | +0xb8 -> mU08->mU74 | 0..32836 (334 distinct) |
| 0x0bc | s32 | `mKind` | named | +0xbc -> mU08->mU48 | 0..9 (9 distinct) |
| 0x0c0 | s32 | `mCostResource` | named | +0xc0 -> mU08->mU38 | 0..4 (2 distinct) |
| 0x0c4 | s32 | `mRequiredStatistic` | named | +0xc4 -> mU08->mU3C | const 0 |
| 0x0c8 | s32 | `mRequiredStatisticValue` | named | +0xc8 -> mU08->mRequiredStatisticValue | const 0 |
| 0x0cc | s32 | `mCalamity` | named | +0xcc -> mU08->mFirstNodeID | 0..35 (29 distinct) |
| 0x0d0 | s32 | `mHotKey` | named | +0xd0 -> mU08->mHotKey | -1..31 (32 distinct) |
| 0x0d4 | s32 | `mUnused5` | unused | +0xd4 | 0..2163200 (2 distinct); 0xCC in 3701 |
| 0x0d8 | s32 | `mUnused6` | unused | +0xd8 | 0..4100 (2 distinct); 0xCC in 3701 |
| 0x0dc | s32 | `mUnused7` | unused | +0xdc | const 0; 0xCC in 3701 |
| 0x0e0 | s32 | `mUnused8` | unused | +0xe0 | const 0; 0xCC in 3701 |
| 0x0e4 | f32 | `mFactor1` | named | +0xe4 the node Load's cost factor of its second unit class | 0..1 (2 distinct) |
| 0x0e8 | f32 | `mFactor2` | named | +0xe8 (third) | 0..1 (2 distinct) |
| 0x0ec | f32 | `mFactor3` | named | +0xec (fourth) | 0..1 (2 distinct) |
| 0x0f0 | f32 | `mFactor4` | named | +0xf0 (fifth; the others: 1.0f) | 0..1 (2 distinct) |
| 0x0f4 | bool | `mShowInEditor` | named | +0xf4 -> mU08->mB7E | 0..1 (2 distinct) |
| 0x0f5 | bool | `mObsoletedByDefault` | named | +0xf5 -> mU08->mB7F | 0..1 (2 distinct) |
| 0x0f6 | bool | `mAvailableWithEpoch` | named | +0xf6 -> mU08->mHasParentalVengeance | 0..1 (2 distinct) |
| 0x0f7 | bool | `mResearchedWithEpoch` | named | +0xf7 -> mU08->mHasGroupAttack | 0..1 (2 distinct) |
| 0x0f8 | s32 | `mUnused9` | unused | +0xf8 | -872415232..0 (2 distinct) |
| 0x0fc | s32 | `mNextEpoch` | named | +0xfc -> the epoch's mU4C | 0..15 (16 distinct) |
| 0x100 | s32 | `mRequiredSOL` | named | +0x100 -> the epoch's mU40 | 0..2 (2 distinct) |
| 0x104 | s32 | `mFirstNodeID` | named | +0x104 -> the epoch's mFirstNodeID | 0..2556416 (17 distinct); 0xCC in 3701 |
| 0x108 | s32 | `mDefaultUnitClass` | named | +0x108 -> mU08->mU40 | 0..842 (73 distinct) |

### 3.7 `db\dbevents.dat`: Events (tech/effect triggers)

* **Loader:** `EEEvent::LoadEvents` 0x5ca63b (src/Empire Earth/Scenario Manager/EEEvent.cpp)
* **Container:** No count: **exactly 1500 rows**, each `char name[100]`, `u32 id`, `u32 numEffects`, numEffects x `s32 effect index`. (The first dword is the name's first bytes, not a count.)
* **AoC data:** 1500 records, 166,680 bytes unpacked. Bytes per record: read 4%, label 96%, unused 0%, padding 0%.
* **Semantics:** Event i is `EEEvent::sEvents[i]`. Triggers run the listed effects (`gEventEffects[idx]`). The names are never used by the game.
* **Cross-references:** effect indices -> dbeffects row; event ids <- dbtechtree `mResearchedEvent`/`mObsoleteEvent`, scenario triggers.
* **Suggested text form:** YAML

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | event name (unused by the game) | 317/1500 non-empty |
| 0x064 | u32 | `mUnusedID` | unused | stored in EEEvent::mID | |
| 0x068 | u32 | `mNumEffects` | named | count of the list that follows | 0..59, 1170 effect refs in all |
| 0x06c | s32[n] | `mEffects` | named | indices into dbeffects | |

### 3.8 `db\dbeffects.dat`: Effects (what an event does)

* **Loader:** same `EEEvent::LoadEvents` (one `Read` of `sizeof(gEventEffects)`)
* **Container:** No count: a raw array of **exactly 2000** 88-byte `EEEventEffect` (176,000 bytes).
* **AoC data:** 2000 records, 176,000 bytes unpacked. Bytes per record: read 65%, label 0%, unused 35%, padding 0%.
* **Semantics:** 1193 real effects; 807 holes (`mKind = -1`, 0xCC in padding and `mL54`). `mKind` selects the action in `EEEvent::RunEffect` 0x5ca7cf: 1 button, 2 area effect, 6 model, 7, 8, 9 tech, 10, 11, 12..14 tech-tree background, 15..18/20/21 sounds, 19 unit, 22. The value is the absolute value when offset and factor are 0. `mRunOnLoad` = run while loading a save.
* **Cross-references:** `mUnitClass`, `mNewUnitClass` -> unit types; `mUnitSet` -> dbunitset; `mMediaIndex` -> dbgraphics; `mTechnology` -> dbtechtree; `mSoundID` -> dbsounds; `mButton` -> dbbuttons; `mAreaEffect` -> dbareaeffect; `mAttribute` -> attribute id (0x28 = area effect).
* **Suggested text form:** YAML (with events) or CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | s32 | `mUnused1` | unused |  | 0..1809 (1194 distinct) |
| 0x004 | s32 | `mUnusedID` | unused |  | 0..1565 (1194 distinct) |
| 0x008 | f32 | `mValue` | named | +0x08 the absolute value (0x1ca591/0x1ca5df: used when +0x0c/+0x10 are 0) | 0..5544 (7 distinct) |
| 0x00c | f32 | `mOffset` | named | +0x0c the offset added to the base | -1.5..10 (10 distinct) |
| 0x010 | f32 | `mFactor` | named | +0x10 the factor of the value | -0.15..1.89 (18 distinct) |
| 0x014 | s32 | `mUnitClass` | named | +0x14 a gUnitClassTable index (< 0x367) | -1..752 (212 distinct) |
| 0x018 | s32 | `mNewUnitClass` | named | +0x18 a unit class (kind 0x13: its media) | -1..826 (178 distinct) |
| 0x01c | s32 | `mUnitSet` | named |  | -1..157 (11 distinct) |
| 0x020 | s32 | `mKind` | named | +0x20 | -1..22 (16 distinct) |
| 0x024 | s32 | `mMediaIndex` | named | +0x24 an EEMedia index (kind 6) | -1..956 (127 distinct) |
| 0x028 | s32 | `mTechnology` | named | +0x28 a tech tree id (kind 9) | -1..15036 (436 distinct) |
| 0x02c | s32 | `mAttribute` | named | +0x2c (kind 2: 0x28 = an area effect) | -1..45 (23 distinct) |
| 0x030 | s32 | `mSoundID` | named | +0x30 an EESoundObject index (kinds 0xf..0x12, 0x14, 0x15) | -1..1072 (63 distinct) |
| 0x034 | s32 | `mButton` | named | +0x34 an EEButtonObject index (kind 1) | -1..1534 (103 distinct) |
| 0x038 | s32 | `mAreaEffect` | named | +0x38 an EEAEManager index (kind 2) | -1..47 (19 distinct) |
| 0x03c | s32 | `mTerrainType` | named |  | 0..14 (3 distinct) |
| 0x040 | s32 | `mUnused2` | unused |  | const 0 |
| 0x044 | u8 | `mRunOnLoad` | named | +0x44 (RunEffect 0x1ca7cf: while loading (gLoadingSavedGame) only effects with 1 run) | 0..1 (2 distinct) |
| 0x045 | u8 | `mUnused3` | unused |  | 0..16 (2 distinct); 0xCC in 807 |
| 0x046 | u16 | `mUnused4` | unused |  | 0..52428 (2 distinct) |
| 0x048 | s32 | `mUnused5` | unused |  | const 0 |
| 0x04c | s32 | `mUnused6` | unused |  | const 0 |
| 0x050 | s32 | `mUnused7` | unused |  | const 0 |
| 0x054 | s32 | `mUnused8` | unused | (sizeof 0x58) | -859045888..0 (2 distinct) |

### 3.9 `db\dbbuttons.dat`: Buttons (UI icon cells)

* **Loader:** `EEButtonObject::Load` 0x4d5500 (src/Empire Earth/Database/EEButtonObject.cpp)
* **Container:** `u32 count` + count x 224-byte `EEButtonRecord`: name[100], texture file[0x68], id, row, column, position, hot key.
* **AoC data:** 1549 records, 346,980 bytes unpacked. Bytes per record: read 55%, label 45%, unused 0%, padding 0%.
* **Semantics:** Every row becomes an object (holes included). The texture is loaded on demand; ids below 50 get alpha. `mRow`/`mColumn` are the cell in a sheet (nearly always 0).
* **Cross-references:** `mHotKey` -> dbuihotkey index; referenced by dbobjects/dbtechtree `mButton`, dbeffects `mButton`.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | INFERRED from data: the row name (char[100], NUL-terminated) | text 1138/1549, max len 34, junk after NUL in 433, e.g. "Upgrade Citizen - Nano" |
| 0x064 | char[104] | `mFileName` | named | +0x64 -> mName | text 1138/1549, max len 45, junk after NUL in 1549, e.g. "textures\but_citizen_15t" |
| 0x0cc | s32 | `mID` | named | +0xcc -> +0x30 | -1..1548 (1139 distinct) |
| 0x0d0 | s32 | `mRow` | named | +0xd0 -> +0x2c | 0..5 (2 distinct) |
| 0x0d4 | s32 | `mColumn` | named | +0xd4 -> +0x28 | 0..5 (2 distinct) |
| 0x0d8 | s32 | `mPosition` | named | +0xd8 -> +0x20 | -1..50 (51 distinct) |
| 0x0dc | s32 | `mHotKey` | named | +0xdc -> +0x24 | -1..30 (29 distinct) |

### 3.10 `db\dbfamily.dat`: Damage table (families x attack kinds)

* **Loader:** `EETerrainTypeTable::LoadDamageTables` 0x4dbf11 (src/Empire Earth/Database/EETerrainTypeTable.cpp); lookup `EEToHitTable::GetAttackFactor` 0x4dbe44
* **Container:** `u32 count` (<= 60) + count x 380-byte `EEDamageTableRecord`: name, dbId, index, then `s32 damage[68]`.
* **AoC data:** 60 records, 22,804 bytes unpacked. Bytes per record: read 72%, label 26%, unused 2%, padding 0%.
* **Semantics:** The values are percentages (x 0.01) into `table[family][kind]`. Two scaled copies are made with world constant 0x59; which copy applies depends on difficulty and human/AI. The row is the unit's `mFamily`; the column is the attack kind. Which unit field picks the column is not traced (probably `mWeapon` or the projectile class).
* **Cross-references:** Rows <- dbobjects `mFamily`, dbunitset `mFamilies[]`.
* **Suggested text form:** CSV (matrix)

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | INFERRED from data: the row name (char[100], NUL-terminated) | text 60/60, max len 24, junk after NUL in 2, e.g. "No Family" |
| 0x064 | s32 | `mUnusedDatabaseID` | unused | INFERRED: unique per row, gappy: the database row id | 100..520 (60 distinct) |
| 0x068 | s32 | `mUnusedIndex` | unused | INFERRED: equals the 0-based row index | 0..59 (60 distinct) |
| 0x06c | s32[68] | `mDamage` | named | +0x6c (scaled by kHeightEpsilon into gU4E8708's row) | [0]: 0..100 (5 distinct) |

### 3.11 `db\dbweapontohit.dat`: Weapon to-hit matrix

* **Loader:** `EEToHitTable::Load` 0x4dcaad (src/Empire Earth/Database/EEUnitBehavior.cpp)
* **Container:** `u32 count` (**must be 6**) + 6 x 132-byte `EEToHitRecord`: name, dbId, index, `s32 percent[6]`.
* **AoC data:** 6 records, 796 bytes unpacked. Bytes per record: read 18%, label 76%, unused 6%, padding 0%.
* **Semantics:** Rows and columns: shock, arrow, pierce, gun, laser, missile (row 0 is named "Defender: Shock"). Percent x 0.01 into `gWeaponToHitFactors[6][6]`.
* **Cross-references:** none
* **Suggested text form:** CSV (matrix)

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | INFERRED from data: the row name (char[100], NUL-terminated) | text 6/6, max len 15, e.g. "Defender: Shock" |
| 0x064 | s32 | `mUnusedDatabaseID` | unused | INFERRED: unique per row, gappy: the database row id | 100..126 (6 distinct) |
| 0x068 | s32 | `mUnusedIndex` | unused | INFERRED: equals the 0-based row index | 0..5 (6 distinct) |
| 0x06c | s32[6] | `mPercents` | named | +0x6c (hundredths) | [0]: 100..150 (2 distinct) |

### 3.12 `db\dbworld.dat`: World constants

* **Loader:** `EEWorldTable::Load` 0x4dcb79 (src/Empire Earth/Database/EEUnitBehavior.cpp); struct in include/ee/EEDatabaseTables.h
* **Container:** `u32 count` + count x 28-byte `EEWorldTableEntry` (no name column).
* **AoC data:** 95 records, 2,664 bytes unpacked. Bytes per record: read 57%, label 0%, unused 43%, padding 0%.
* **Semantics:** There are 95 constants, read as `gWorldTable.mEntries[k].field` from about 65 sites. Examples: entry 42 float = game speed, entry 26 = lobby unit limit, 0x56 = civ-powers category cost, 0x59 = damage scale factors. The 61 rows with a reader are named in section 8.3 (the text's checked `label`); the other 34 rows are unread.
* **Cross-references:** Indexed by position from code.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | f32 | `mFloatValue` | named | +0x00 | 0..70 (21 distinct) |
| 0x004 | f32 | `mFloatValue2` | named | +0x04 | 0..3 (8 distinct) |
| 0x008 | s32 | `mUnusedDatabaseID` | unused | +0x08 | 0..99 (95 distinct) |
| 0x00c | s32 | `mUnusedIndex` | unused | +0x0c | -1..94 (95 distinct) |
| 0x010 | s32 | `mLongValue` | named | +0x10 (signed: EECPIntelligence 0x480f9b `fidiv dword [eax+0x10]`; TYPE CENSUS) | 0..90000 (23 distinct) |
| 0x014 | u32 | `mLongValue2` | named | +0x14 | 0..112500 (8 distinct) |
| 0x018 | s32 | `mUnused` | unused | +0x18 (entry 31: 1, entry 48: 0x50200; otherwise 0) | -858993664..328192 (4 distinct) |

### 3.13 `db\dbunitbehavior.dat`: Unit stances

* **Loader:** `EEUnitBehavior::Load` 0x4dc973 (src/Empire Earth/Database/EEUnitBehavior.cpp)
* **Container:** `u32 count` + count x 124-byte `EEUnitBehaviorRecord`.
* **AoC data:** 8 records, 996 bytes unpacked. Bytes per record: read 9%, label 81%, unused 10%, padding 0%.
* **Semantics:** Scout, Stand Ground, Guard, Harass, Cautious, Aggressive... Seven permission bools.
* **Cross-references:** <- dbobjects `mBehavior`.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | INFERRED from data: the row name (char[100], NUL-terminated) | text 8/8, max len 12, junk after NUL in 2, e.g. "Scout" |
| 0x064 | s32 | `mUnusedDatabaseID` | unused | INFERRED: unique per row, gappy: the database row id | 1..8 (8 distinct) |
| 0x068 | s32 | `mIndex` | named | +0x68 (negative: no entry) | 0..7 (8 distinct) |
| 0x06c | bool | `mCanMoveFreely` | named | +0x6c -> +0x04 | 0..1 (2 distinct) |
| 0x06d | bool | `mCanPursuePastInitialContactLOS` | named | +0x6d -> +0x07 | 0..1 (2 distinct) |
| 0x06e | bool | `mCanTargetEnemies` | named | +0x6e -> +0x08 | 0..1 (2 distinct) |
| 0x06f | bool | `mReturnsToInitialContactLocation` | named | +0x6f -> +0x09 | 0..1 (2 distinct) |
| 0x070 | bool | `mCanFlee` | named | +0x70 -> +0x0a | 0..1 (2 distinct) |
| 0x071 | bool | `mMeleeCanMoveFreely` | named | +0x71 -> +0x05 | 0..1 (2 distinct) |
| 0x072 | bool | `mRangedCanMoveFreely` | named | +0x72 -> +0x06 | const 0 |
| 0x073 | s32 | `(unused gap)` | unused | undecoded | const 0 |
| 0x077 | s32 | `(unused gap)` | unused | undecoded | const 0 |
| 0x07b | pad | `(unused gap)` | unused | undecoded | bytes 00x8 |

### 3.14 `db\dbstartingresources.dat`: Starting resources presets

* **Loader:** `EEStartingResources::Load` 0x4daa05 (src/Empire Earth/Database/EEStartingResources.cpp)
* **Container:** `u32 count` + count x 132-byte `EEStartingResourcesRecord`.
* **AoC data:** 6 records, 796 bytes unpacked. Bytes per record: read 21%, label 76%, unused 3%, padding 0%.
* **Semantics:** Rows with index < 0 are skipped.
* **Cross-references:** `mNameID` -> Language.dll.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | INFERRED from data: the row name (char[100], NUL-terminated) | text 5/6, max len 22, junk after NUL in 1, e.g. "Tournament - Low" |
| 0x064 | s32 | `mUnusedDatabaseID` | unused | INFERRED: unique per row, gappy: the database row id | -1..7 (6 distinct) |
| 0x068 | s32 | `mIndex` | named | +0x68 (negative: skipped) | -1..5 (6 distinct) |
| 0x06c | s32 | `mNameID` | named | +0x6c -> +0x18 | 0..7151 (6 distinct) |
| 0x070 | s32 | `mFood` | named | +0x70 -> +0x04 | 0..15000 (5 distinct) |
| 0x074 | s32 | `mWood` | named | +0x74 -> +0x08 | 0..15000 (5 distinct) |
| 0x078 | s32 | `mStone` | named | +0x78 -> +0x0c | 0..5000 (6 distinct) |
| 0x07c | s32 | `mGold` | named | +0x7c -> +0x10 | 0..10000 (3 distinct) |
| 0x080 | s32 | `mIron` | named | +0x80 -> +0x14 | 0..10000 (3 distinct) |

### 3.15 `db\dbanimals.dat`: Animals (herd AI)

* **Loader:** `EEAnimal::Load` 0x4d4a5d (src/Empire Earth/Database/EEAnimal.cpp)
* **Container:** `u32 count` + count x 272-byte `EEAnimalRecord`.
* **AoC data:** 14 records, 3,812 bytes unpacked. Bytes per record: read 38%, label 37%, unused 22%, padding 4%.
* **Semantics:** Every row is loaded. Intervals are in seconds (x 1000 at load). +0xf4..+0x10f is zero in every row.
* **Cross-references:** `mUnitType` -> unit type; `mMedia` -> dbgraphics; `mSound1..3` -> dbsounds.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | INFERRED from data: the row name (char[100], NUL-terminated) | text 14/14, max len 18, junk after NUL in 1, e.g. "Res - Deer" |
| 0x064 | s32 | `mUnusedDatabaseID` | unused | INFERRED: unique per row, gappy: the database row id | 1..19 (14 distinct) |
| 0x068 | s32 | `mAnimalID` | named | +0x68 -> +0x3c | 0..13 (14 distinct) |
| 0x06c | s32 | `mUnitType` | named | +0x6c -> +0x38 | 261..661 (14 distinct) |
| 0x070 | s32 | `mMinHerdSize` | named | +0x70 -> +0x40 | 1..2 (2 distinct) |
| 0x074 | s32 | `mMaxHerdSize` | named | +0x74 -> +0x44 | 2..8 (4 distinct) |
| 0x078 | bool | `mBreeds` | named | +0x78 -> +0x85 | 0..1 (2 distinct) |
| 0x079 | pad | `(padding)` | pad | alignment padding | bytes 000000x14 |
| 0x07c | s32 | `mMinBirthInterval` | named | +0x7c -> +0x48 (s) | 90..999999 (7 distinct) |
| 0x080 | s32 | `mMaxBirthInterval` | named | +0x80 -> +0x4c (s) | 90..9999999 (7 distinct) |
| 0x084 | bool | `mHasLeader` | named | +0x84 -> +0x87 | const 0 |
| 0x085 | bool | `mUnusedFlag1` | unused | +0x85 -> +0x88 | const 0 |
| 0x086 | pad | `(padding)` | pad | alignment padding | bytes 0000x14 |
| 0x088 | s32 | `mUnusedListValue1` | unused | +0x88 -> mL04 | 1..9 (8 distinct) |
| 0x08c | s32 | `mUnusedListValue2` | unused | +0x8c -> mL04 | -1..9 (5 distinct) |
| 0x090 | s32 | `mUnusedListValue3` | unused | +0x90 -> mL04 | -1..6 (4 distinct) |
| 0x094 | s32 | `mUnusedListValue4` | unused | +0x94 -> mL04 | -1..2 (2 distinct) |
| 0x098 | bool | `mUnusedIsPredator` | unused | +0x98 -> +0x89 | 0..1 (2 distinct) |
| 0x099 | pad | `(padding)` | pad | alignment padding | bytes 000000x14 |
| 0x09c | f32 | `mUnusedFactor` | unused | +0x9c -> +0x28 (copied with mov in the original) | 0.1..0.8 (7 distinct) |
| 0x0a0 | s32 | `mMinHuntInterval` | named | +0xa0 -> +0x50 (s) | 120..2000 (9 distinct) |
| 0x0a4 | s32 | `mMaxHuntInterval` | named | +0xa4 -> +0x54 (s) | 200..10000 (9 distinct) |
| 0x0a8 | s32 | `mMinRoamMargin` | named | +0xa8 -> +0x58 | 2..10 (2 distinct) |
| 0x0ac | s32 | `mMaxRoamMargin` | named | +0xac -> +0x5c | 2..50 (3 distinct) |
| 0x0b0 | s32 | `mMinGrazeTime` | named | +0xb0 -> +0x60 (s) | 0..90 (5 distinct) |
| 0x0b4 | s32 | `mMaxGrazeTime` | named | +0xb4 -> +0x64 (s) | 0..180 (3 distinct) |
| 0x0b8 | bool | `mUnusedIsFlightless` | unused | +0xb8 -> +0x84 | 0..1 (2 distinct) |
| 0x0b9 | bool | `mIsPredator` | named | +0xb9 -> +0x86 | 0..1 (2 distinct) |
| 0x0ba | bool | `mUnusedFightsBack` | unused | +0xba -> +0x83 | 0..1 (2 distinct) |
| 0x0bb | bool | `mHasParentalVengeance` | named | +0xbb -> +0x80 | const 0 |
| 0x0bc | bool | `mHasGroupAttack` | named | +0xbc -> +0x81 | 0..1 (2 distinct) |
| 0x0bd | bool | `mHasGroupFlee` | named | +0xbd -> +0x82 | const 0 |
| 0x0be | pad | `(padding)` | pad | alignment padding | bytes 0000x14 |
| 0x0c0 | f32 | `mNewbornGrowth` | named | +0xc0 -> +0x30 | 0.4..0.6 (5 distinct) |
| 0x0c4 | f32 | `mGrowthStep` | named | +0xc4 -> +0x34 | const 0.005 |
| 0x0c8 | s32 | `mGrowInterval` | named | +0xc8 -> +0x6c (s) | 3..15 (5 distinct) |
| 0x0cc | s32 | `mHerdRadius` | named | +0xcc -> +0x68 | const 20 |
| 0x0d0 | s32 | `mMinRestInterval` | named | +0xd0 -> +0x70 (s) | const 300 |
| 0x0d4 | s32 | `mMaxRestInterval` | named | +0xd4 -> +0x74 (s) | const 900 |
| 0x0d8 | s32 | `mUnusedPredatorFleeChance` | unused | +0xd8 -> +0x2c | const 1061158912 |
| 0x0dc | s32 | `mMinFollowInterval` | named | +0xdc -> +0x78 (s) | const 120 |
| 0x0e0 | s32 | `mMaxFollowInterval` | named | +0xe0 -> +0x7c (s) | const 260 |
| 0x0e4 | s32 | `mMedia` | named | +0xe4 -> +0x24 (EEMedia::Get) | const -1 |
| 0x0e8 | s32 | `mSound1` | named | +0xe8 -> mSounds | const -1 |
| 0x0ec | s32 | `mSound2` | named | +0xec -> mSounds | const -1 |
| 0x0f0 | s32 | `mSound3` | named | +0xf0 -> mSounds | const -1 |
| 0x0f4 | s32 | `(unused gap)` | unused | undecoded | const 0 |
| 0x0f8 | s32 | `(unused gap)` | unused | undecoded | const 0 |
| 0x0fc | s32 | `(unused gap)` | unused | undecoded | const 0 |
| 0x100 | s32 | `(unused gap)` | unused | undecoded | const 0 |
| 0x104 | s32 | `(unused gap)` | unused | undecoded | const 0 |
| 0x108 | s32 | `(unused gap)` | unused | undecoded | const 0 |
| 0x10c | s32 | `(unused gap)` | unused | undecoded | const 0 |

### 3.16 `db\dbcolortable.dat`: Player colours

* **Loader:** `EEColorTable::Load` 0x4d5d91 (src/Empire Earth/Database/EEColorTable.cpp)
* **Container:** `u32 count` + count x 168-byte `EEColorTableRecord`: name, dbId, index, three RGB float triples, 24 zero bytes.
* **AoC data:** 69 records, 11,596 bytes unpacked. Bytes per record: read 21%, label 60%, unused 19%, padding 0%.
* **Semantics:** Colour 2 also yields a 60% dark shade. "Color 3" is missing between colour 2 and colour 4; the gap at +0x90 (zero) may be it, or a 5th/6th colour.
* **Cross-references:** Index = colour slot (players, "Nocolor" 68).
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | INFERRED from data: the row name (char[100], NUL-terminated) | text 69/69, max len 31, junk after NUL in 2, e.g. "Player 1 (Blue)" |
| 0x064 | s32 | `mUnusedDatabaseID` | unused | INFERRED: unique per row, gappy: the database row id | 1..70 (69 distinct) |
| 0x068 | s32 | `mUnusedIndex` | unused | INFERRED: equals the 0-based row index | 0..68 (69 distinct) |
| 0x06c | f32 | `mColorRed` | named | +0x6c | 0..1 (11 distinct) |
| 0x070 | f32 | `mColorGreen` | named | +0x70 | 0..1 (10 distinct) |
| 0x074 | f32 | `mColorBlue` | named | +0x74 | 0..1 (11 distinct) |
| 0x078 | f32 | `mColor2Red` | named | +0x78 | 0..1 (21 distinct) |
| 0x07c | f32 | `mColor2Green` | named | +0x7c | 0..1 (22 distinct) |
| 0x080 | f32 | `mColor2Blue` | named | +0x80 | 0..1 (22 distinct) |
| 0x084 | f32 | `mColor4Red` | named | +0x84 | 0..1 (5 distinct) |
| 0x088 | f32 | `mColor4Green` | named | +0x88 | 0..1 (5 distinct) |
| 0x08c | f32 | `mColor4Blue` | named | +0x8c | 0..1 (3 distinct) |
| 0x090 | s32 | `(unused gap)` | unused | undecoded | const 0 |
| 0x094 | s32 | `(unused gap)` | unused | undecoded | const 0 |
| 0x098 | s32 | `(unused gap)` | unused | undecoded | const 0 |
| 0x09c | s32 | `(unused gap)` | unused | undecoded | const 0 |
| 0x0a0 | s32 | `(unused gap)` | unused | undecoded | const 0 |
| 0x0a4 | s32 | `(unused gap)` | unused | undecoded | const 0 |

### 3.17 `db\dbaiunittargeting.dat`: AI unit targeting weights

* **Loader:** `EEAIUnitTargetingTable::Load` 0x4d47a1 (src/Empire Earth/Database/EEAIUnitTargetingTable.cpp)
* **Container:** `u32 count` (**must be 3**) + 3 x 160-byte `EEAIUnitTargetingRecord`.
* **AoC data:** 3 records, 484 bytes unpacked. Bytes per record: read 22%, label 62%, unused 15%, padding 0%.
* **Semantics:** Rows: Melee, Short Range, Long Range. Each yields 10 weights. `mL90..mL98` are not kept.
* **Cross-references:** none
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | INFERRED from data: the row name (char[100], NUL-terminated) | text 3/3, max len 11, e.g. "Melee" |
| 0x064 | s32 | `mUnusedDatabaseID` | unused | INFERRED: unique per row, gappy: the database row id | 1..3 (3 distinct) |
| 0x068 | s32 | `mUnusedIndex` | unused | INFERRED: equals the 0-based row index | 0..2 (3 distinct) |
| 0x06c | s32 | `mAttackerBonus` | named | +0x6c | 2..15 (3 distinct) |
| 0x070 | s32 | `mUnusedWeight` | unused | +0x70 | 1..20 (3 distinct) |
| 0x074 | s32 | `mInRangeBonus` | named | +0x74 | 30..50 (2 distinct) |
| 0x078 | s32 | `mFasterTargetBonus` | named | +0x78 | 1..6 (3 distinct) |
| 0x07c | s32 | `mStrongerTargetBonus` | named | +0x7c | 1..6 (3 distinct) |
| 0x080 | s32 | `mPreferredUnitType` | named | +0x80 | const 0 |
| 0x084 | s32 | `mToHitWeight` | named | +0x84 | const 1 |
| 0x088 | s32 | `mProximityWeight` | named | +0x88 | 0..50 (3 distinct) |
| 0x08c | s32 | `mImmobileTargetBonus` | named | +0x8c | const -500 |
| 0x090 | s32 | `mUnused1` | unused | +0x90 | 0..2 (3 distinct) |
| 0x094 | s32 | `mUnused2` | unused | +0x94 | 0..2 (3 distinct) |
| 0x098 | s32 | `mUnused3` | unused | +0x98 | 0..2 (3 distinct) |
| 0x09c | s32 | `mMaxValue` | named | +0x9c | 15..50 (2 distinct) |

### 3.18 `db\dbterrain.dat`: Terrains

* **Loader:** `EETerrainTable::Load` 0x4db2ce (src/Empire Earth/Database/EETerrainTable.cpp)
* **Container:** `u32 count`; per entry: one string (texture base, e.g. `textures\BaseGrass_1.sst`, no NUL) + 204-byte `EETerrainRecord`.
* **AoC data:** 93 records, 21,167 bytes unpacked. Bytes per record: read 92%, label 0%, unused 7%, padding 0%.
* **Semantics:** Texture ids are numbered cumulatively (`columns x rows` per terrain). The first named terrain with `mID != -1` becomes the default terrain. Colour is 0..255 per channel (x 1/255). `mFirst[k]..mLast[k]` (k < 3) are inclusive unit-type ranges of ambient units.
* **Cross-references:** `mTerrainType` -> dbterraintype; `mFirst/mLast` -> unit types; `mColorType` -> terrain colour palette; `mNameID` -> Language.dll; referenced by dbcliffterrain.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[104] | `mName` | named | +0x00 -> mFamilies | text 77/93, max len 26, junk after NUL in 93, e.g. "RM Ambients - Marsh" |
| 0x068 | s32 | `mID` | named | +0x68 -> +0x50 | -1..92 (78 distinct) |
| 0x06c | s32 | `mTextureID` | named | +0x6c -> +0x4c | 0..58 (58 distinct) |
| 0x070 | s32 | `mNameID` | named | +0x70 -> +0x54 | 0..22024 (70 distinct) |
| 0x074 | s32 | `mTextureTileCoverage` | named | +0x74 -> +0x58 | 0..1 (2 distinct) |
| 0x078 | s32 | `mTerrainType` | named | +0x78 -> +0x5c | 0..12 (9 distinct) |
| 0x07c | s32 | `mColumns` | named | +0x7c -> +0x60 | 0..16 (4 distinct) |
| 0x080 | s32 | `mRows` | named | +0x80 -> +0x64 | 0..4 (4 distinct) |
| 0x084 | bool | `mIsWater` | named | +0x84 -> +0x84 | 0..1 (2 distinct) |
| 0x085 | bool | `mHasAlpha` | named | +0x85 -> +0x86 | const 0 |
| 0x086 | bool | `mIsInEditor` | named | +0x86 -> +0x85 | 0..1 (2 distinct) |
| 0x087 | pad | `(padding)` | pad | alignment padding | bytes 00x77, ccx16 |
| 0x088 | s32 | `mColorType` | named | +0x88 -> +0x68 | 0..67 (39 distinct) |
| 0x08c | u32[3] | `mFirst` | named | +0x8c: unit class ranges [mFirst[k], mLast[k]] -> mUnitClasses | [0]: 0..691 (16 distinct) |
| 0x098 | u32[3] | `mLast` | named | +0x98 | [0]: 0..696 (16 distinct) |
| 0x0a4 | s32 | `mAmbientUnitChance` | named | +0xa4 -> +0x7c | 0..100 (7 distinct) |
| 0x0a8 | s32 | `mWaterAnimationPeriod` | named | +0xa8 -> +0x44 | 0..100 (2 distinct) |
| 0x0ac | s32 | `mWaterDepth` | named | +0xac -> +0x80 | 0..4 (2 distinct) |
| 0x0b0 | s32 | `mRed` | named | +0xb0 (0..255) | 0..255 (23 distinct); 0xCC in 16 |
| 0x0b4 | s32 | `mGreen` | named | +0xb4 | 0..255 (27 distinct); 0xCC in 16 |
| 0x0b8 | s32 | `mBlue` | named | +0xb8 | 0..255 (29 distinct); 0xCC in 16 |
| 0x0bc | s32 | `(unused gap)` | unused | undecoded | const 0 |
| 0x0c0 | s32 | `(unused gap)` | unused | undecoded | 0..1102158234 (2 distinct) |
| 0x0c4 | s32 | `(unused gap)` | unused | undecoded | 0..1107637043 (2 distinct) |
| 0x0c8 | pad | `(unused gap)` | unused | undecoded | bytes 000000x93 |
| 0x0cb | bool | `mIsAmbientsOnly` | named | +0xcb -> +0x87 | 0..1 (2 distinct) |

### 3.19 `db\dbterraintype.dat`: Terrain types

* **Loader:** `EETerrainTypeTable::Load` 0x4dbd2e (src/Empire Earth/Database/EETerrainTypeTable.cpp)
* **Container:** `u32 count` + count x 128-byte `EETerrainTypeRecord`.
* **AoC data:** 68 records, 8,708 bytes unpacked. Bytes per record: read 13%, label 78%, unused 6%, padding 2%.
* **Semantics:** Every row is loaded. `mL78` is often 999999999.
* **Cross-references:** `mFirstColor..mLastColor` -> entries of the 256-colour `textures\ColorPalette.tga` palette; `mNameID` -> Language.dll.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | INFERRED from data: the row name (char[100], NUL-terminated) | text 68/68, max len 34, junk after NUL in 1, e.g. "Grass" |
| 0x064 | s32 | `mUnusedDatabaseID` | unused | INFERRED: unique per row, gappy: the database row id | 1..69 (68 distinct) |
| 0x068 | s32 | `mID` | named | +0x68 -> +0x08 | 0..67 (68 distinct) |
| 0x06c | s32 | `mFirstColor` | named | +0x6c -> +0x0c | 0..255 (48 distinct) |
| 0x070 | s32 | `mLastColor` | named | +0x70 -> +0x10 | 7..255 (49 distinct) |
| 0x074 | s32 | `mNameID` | named | +0x74 -> +0x14 | 5801..22017 (68 distinct) |
| 0x078 | s32 | `mUnusedPaletteIndex` | unused | +0x78 -> +0x18 | 0..999999999 (16 distinct) |
| 0x07c | bool | `mIsInEditor` | named | +0x7c -> +0x1c | const 1 |
| 0x07d | pad | `(padding)` | pad | tail padding | bytes 000000x67, 720000x1 |

### 3.20 `db\dbcliffterrain.dat`: Cliff terrains

* **Loader:** `EECliffTerrainTable::Load` 0x4dbb22 (src/Empire Earth/Database/EETerrainTable.cpp)
* **Container:** `u32 count` + count x 124-byte `EECliffTerrainRecord`.
* **AoC data:** 3 records, 376 bytes unpacked. Bytes per record: read 94%, label 0%, unused 4%, padding 2%.
* **Semantics:** Rows with a negative index stay NULL. The first valid row is `gFirstCliffTerrain`.
* **Cross-references:** `mFirstTerrain..mLastTerrain` -> dbterrain ids.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[104] | `mName` | named | +0x00 | text 3/3, max len 12, junk after NUL in 3, e.g. "DefaultCliff" |
| 0x068 | s32 | `mIndex` | named | +0x68 (negative: no entry) | 0..2 (3 distinct) |
| 0x06c | s32 | `mFirstTerrain` | named | +0x6c -> +0x14 | 50..77 (3 distinct) |
| 0x070 | s32 | `mLastTerrain` | named | +0x70 -> +0x18 | 51..78 (3 distinct) |
| 0x074 | s32 | `mUnusedValue` | unused | +0x74 -> +0x1c | const 0 |
| 0x078 | bool | `mUnusedFlag` | unused | +0x78 -> +0x20 | const 1 |
| 0x079 | pad | `(padding)` | pad | tail padding | bytes 000000x3 |

### 3.21 `db\dbcivpowers.dat`: Civilization powers

* **Loader:** `EECivPowersManager::Load` 0x683124 (src/Empire Earth/Civilization Builder/EECivPowersManager.cpp)
* **Container:** `u32 count` + count x 152-byte `EECivPower`.
* **AoC data:** 23 records, 3,500 bytes unpacked. Bytes per record: read 21%, label 66%, unused 13%, padding 0%.
* **Semantics:** Power ids start at 0x26 (`GetPower(id)` indexes id - 0x26). Each power's bonus becomes a level of the synthetic "special powers" category in dbcivilization.
* **Cross-references:** `mTechnology` -> dbtechtree; `mUnitSet` -> dbunitset; `mUnitType` -> unit type.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | +0x00 the power's name, NUL-terminated (dbcivpowers.dat: "Flaming | text 23/23, max len 26, e.g. "Flaming Arrows" |
| 0x064 | s32 | `mUnusedDatabaseID` | unused | +0x64 | 1..26 (23 distinct) |
| 0x068 | s32 | `mID` | named | +0x68 | 38..60 (23 distinct) |
| 0x06c | f32 | `mBonus` | named | +0x6c (EECivilizationManager::Load copies it as a level bonus: a dword move into a float) | 0.05..0.3 (6 distinct) |
| 0x070 | s32 | `mUnitSet` | named | +0x70 | 0..162 (4 distinct) |
| 0x074 | s32 | `mTechnology` | named | +0x74 (EEMResearchTechnology compares it with the node's +0x5c) | -1..1088 (14 distinct) |
| 0x078 | s32 | `mMiningLimit` | named | +0x78 | 0..7 (2 distinct) |
| 0x07c | s32 | `mChance` | named | +0x7c | 0..50 (2 distinct) |
| 0x080 | s32 | `mUnused1` | unused | +0x80 | const 0 |
| 0x084 | f32 | `mAmount` | named | +0x84 | 0..50 (3 distinct) |
| 0x088 | s32 | `mUnused2` | unused | +0x88 | const 0 |
| 0x08c | s32 | `mUnused3` | unused | +0x8c | const 0 |
| 0x090 | s32 | `mUnused4` | unused | +0x90 | const 0 |
| 0x094 | s32 | `mUnitType` | named | +0x94 | -1..725 (2 distinct) |

### 3.22 `db\dbcivilization.dat`: Civilization builder categories

* **Loader:** `EECivilizationManager::Load` 0x681514 (src/Empire Earth/Civilization Builder/EECivilizationManager.cpp)
* **Container:** `u32 count` + count x 600-byte `EECivilizationCategoryEntry`: dbId, index, name[100] (at +0x08), name id, cost, unit set, then 60 x (f32 value, f32 bonus).
* **AoC data:** 29 records, 17,404 bytes unpacked. Bytes per record: read 83%, label 17%, unused 0%, padding 0%.
* **Semantics:** Rows with `index <= 0` are skipped. A 36th "special powers" category is synthesised from dbcivpowers. Upgrade ids are `(category << 6) / level` (level 1..60). The hole rows hold 0xCC level pairs.
* **Cross-references:** `mUnitSet` -> dbunitset; `mNameID` -> Language.dll; `.civ` files store upgrade ids into this table.
* **Suggested text form:** YAML or CSV (long format)

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | s32 | `mDatabaseID` | named | +00, loader preserves/increments | 0..30 (28 distinct) |
| 0x004 | s32 | `mIndex` | named | +04, loader preserves/increments | 0..28 (28 distinct) |
| 0x008 | char[100] | `label` | label | INFERRED from data: the row name (char[100], NUL-terminated) | text 29/29, max len 100, junk after NUL in 1, e.g. "ÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌÌ" |
| 0x06c | s32 | `mNameID` | named | +6c, actual name reader281c92 | 0..31524 (28 distinct) |
| 0x070 | s32 | `mCost` | named | +70, actual long281873 | 0..6 (6 distinct) |
| 0x074 | s32 | `mUnitSet` | named | +74, actual loader writes102 | 0..172 (27 distinct) |
| 0x078 | pair_f32[60] | `mLevels` | named | +78, actual1..<61 float-pair enumeration | [0]: 0..0.3 (4 distinct) |

### 3.23 `db\dbpremadecivs.dat`: Premade civilizations

* **Loader:** `EECivilizationManager::LoadPremadeCivilizations` 0x681672
* **Container:** `u32 count` + count x 120-byte `EECivilizationEntry`: dbId, index, file name[100] (at +0x08), checksum, name id, era name id.
* **AoC data:** 23 records, 2,764 bytes unpacked. Bytes per record: read 97%, label 0%, unused 3%, padding 0%.
* **Semantics:** Sorted by index after loading; the distinct era ids form the era list. `mChecksum` identifies an unmodified premade `.civ`.
* **Cross-references:** `mFileName` -> `civilizations\<name>.civ`; `mNameID`, `mEraNameID` -> Language.dll.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | s32 | `mUnusedDatabaseID` | unused | +0x00 | 1..23 (23 distinct) |
| 0x004 | s32 | `mIndex` | named | +0x04 | 0..22 (23 distinct) |
| 0x008 | char[100] | `mFileName` | named | +0x08 the civilization's file (EEUIEPDCS: the selected entry's file) | text 23/23, max len 17, e.g. "Classical Greece" |
| 0x06c | s32 | `mChecksum` | named | +0x6c actual281bc5 filename/checksum record | 10254..19926 (23 distinct) |
| 0x070 | s32 | `mNameID` | named | +0x70 string id of the name (gResource->GetString) | 31750..31840 (23 distinct) |
| 0x074 | s32 | `mEraNameID` | named | +0x74 | 31816..31819 (4 distinct) |

### 3.24 `db\dbcalamity.dat`: Calamities / special powers

* **Loader:** `EECalamityManager::LoadCalamityClasses` 0x4ae070 (src/Empire Earth/Calamities/EECalamityManager.cpp); record include/ee/EECalamityClass.h
* **Container:** `u32 count` + count x 236-byte `EECalamityData`.
* **AoC data:** 36 records, 8,500 bytes unpacked. Bytes per record: read 42%, label 42%, unused 14%, padding 2%.
* **Semantics:** Here dbId (+0x7c) and index (+0x80, `mID`) follow the name and six floats. `mClassEnum` (kCalamity*: 3 earthquake, 4 hurricane, 7 plague, 8 leprosy, 9 volcano, 10 fire, 13 morale, 14 time warp, 15 teleport, 16 area effect, 17 AE extra damage, 18 special attack, 19 mind control, 20 conscription, 21 logic bomb, 22 drop object, 23 cloaking, 24 meteor storm, 25 line of sight, 26 special location) selects the C++ class. The parameter slots mean different things per class.
* **Cross-references:** `mGraphicEffectID1..4` -> dbgfxeffects; `mSoundID`, `mSecondarySoundID` -> dbsounds; `mUnitSet` -> dbunitset; `mUnitClassIndex` -> unit type; `mNameID` -> Language.dll; <- dbtechtree `mCalamity`.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | +0x00 ("Undefined", "Global Warming", ... "Special Location") | text 36/36, max len 21, junk after NUL in 1, e.g. "Undefined" |
| 0x064 | f32 | `mFloatParam1` | named | +0x64 (line of sight: the bonus) | 0..6 (8 distinct) |
| 0x068 | f32 | `mFloatParam2` | named | +0x68 (plague: functor argument) | 0..2 (6 distinct) |
| 0x06c | f32 | `mQuakeMinimumStrength` | named | +0x6c (earthquake Initialize 0xb0765) | 0..0.05 (2 distinct) |
| 0x070 | f32 | `mQuakeStrength` | named | +0x70 (earthquake Initialize) | 0..0.3 (2 distinct) |
| 0x074 | f32 | `mUnused1` | unused | +0x74 (0 in every record) | const 0 |
| 0x078 | f32 | `mRange` | named | +0x78 -> EECalamityClass +0x1c (rounded) | 0..10000 (9 distinct) |
| 0x07c | s32 | `mUnusedDatabaseID` | unused | +0x7c (128..: the record's id) | 127..165 (36 distinct) |
| 0x080 | s32 | `mID` | named | +0x80 -> +0x18 (an enum, as the class member it fills) | 0..35 (36 distinct) |
| 0x084 | s32 | `mClassEnum` | named | +0x84 -> +0x08 | 0..26 (27 distinct) |
| 0x088 | s32 | `mUnusedTargetKind` | unused | +0x88 | 0..3 (4 distinct) |
| 0x08c | s32 | `mLifeTime` | named | +0x8c -> +0x04 (as float) | 0..300000 (18 distinct) |
| 0x090 | s32 | `mUnused2` | unused | +0x90 (1 in every record) | const 1 |
| 0x094 | s32 | `mGraphicEffectID1` | named | +0x94 EEGraphicEffect table index (area effects: +0x28) | 0..254 (15 distinct) |
| 0x098 | s32 | `mGraphicEffectID2` | named | +0x98 (area effects: +0x2c) | 0..239 (12 distinct) |
| 0x09c | s32 | `mGraphicEffectID3` | named | +0x9c (mind control: +0x30) | 0..257 (8 distinct) |
| 0x0a0 | s32 | `mGraphicEffectID4` | named | +0xa0 (time warp: +0x34) | 0..201 (3 distinct) |
| 0x0a4 | s32 | `mUnused3` | unused | +0xa4 (0 in every record) | const 0 |
| 0x0a8 | s32 | `mUnused4` | unused | +0xa8 (0) | const 0 |
| 0x0ac | s32 | `mUnused5` | unused | +0xac (0) | const 0 |
| 0x0b0 | s32 | `mUnitClassIndex` | named | +0xb0 (drop object: the unit class to drop) | 0..805 (6 distinct) |
| 0x0b4 | s32 | `mIntParam1` | named | +0xb4 (area effects: +0x44) | 0..2000 (9 distinct) |
| 0x0b8 | s32 | `mIntParam2` | named | +0xb8 (area effects: +0x3c) | 0..10000 (13 distinct) |
| 0x0bc | s32 | `mIntParam3` | named | +0xbc (area effects: functor argument) | 0..2000 (9 distinct) |
| 0x0c0 | s32 | `mIntParam4` | named | +0xc0 (fire: functor argument) | 0..4000 (6 distinct) |
| 0x0c4 | s32 | `mIntParam5` | named | +0xc4 (earthquake Initialize) | 0..700 (4 distinct) |
| 0x0c8 | s32 | `mIntParam6` | named | +0xc8 (hurricane: the map area its location needs) | 0..6 (3 distinct) |
| 0x0cc | bool | `mUnused6` | unused | +0xcc (1 in 31 of 36 records) | 0..1 (2 distinct) |
| 0x0cd | bool | `mBoolParam1` | named | +0xcd (extra damage: functor argument) | 0..1 (2 distinct) |
| 0x0ce | bool | `mUnused7` | unused | +0xce (0 in every record) | const 0 |
| 0x0cf | bool | `mUnused8` | unused | +0xcf (0) | const 0 |
| 0x0d0 | bool | `mIsCastByAllSelected` | named | +0xd0 -> +0x26 | 0..1 (2 distinct) |
| 0x0d1 | bool | `mIsBlockedByImmunity` | named | +0xd1 -> +0x25 | 0..1 (2 distinct) |
| 0x0d2 | pad | `(padding)` | pad | alignment padding | bytes 0000x36 |
| 0x0d4 | s32 | `mSoundID` | named | +0xd4 EESoundObject table index (area effects: +0x34) | -1..1071 (19 distinct) |
| 0x0d8 | s32 | `mSecondarySoundID` | named | +0xd8 (plague: its second sound) | -1..516 (2 distinct) |
| 0x0dc | s32 | `mCost` | named | +0xdc -> +0x20 | 0..100 (10 distinct) |
| 0x0e0 | u32 | `mUnitSet` | named | +0xe0 -> +0x10 | 0..105 (16 distinct) |
| 0x0e4 | s32 | `mNameID` | named | +0xe4 -> +0x14 | 0..32818 (24 distinct) |
| 0x0e8 | bool | `mUnusedClassFlag` | unused | +0xe8 -> +0x24 | 0..1 (2 distinct) |
| 0x0e9 | pad | `(padding)` | pad | tail padding | bytes 000000x36 |

### 3.25 `db\dbunitset.dat`: Unit sets

* **Loader:** `EEUnitSetManager::Load` 0x536fa9 (src/Empire Earth/Empire Earth/EEUnitSetManager.cpp)
* **Container:** `u32 count` + count x 184-byte `EEUnitSetRecord`.
* **AoC data:** 173 records, 31,836 bytes unpacked. Bytes per record: read 98%, label 0%, unused 0%, padding 2%.
* **Semantics:** Rows with `mID <= 0` are NULL. A set is the union of 8 families and 8 unit types, plus an included set and an excluded set (row indices).
* **Cross-references:** `mFamilies[]` -> dbfamily rows; `mUnitTypes[]` -> unit types; `mIncludedSet`/`mExcludedSet` -> dbunitset; referenced by dbcalamity, dbareaeffect, dbcivilization, dbcivpowers, dbgamevariant, dbeffects, dbobjects.
* **Suggested text form:** CSV (list columns) or YAML

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[104] | `mName` | named | +0x00 | text 170/173, max len 49, junk after NUL in 173, e.g. "People Morale 1" |
| 0x068 | s32 | `mID` | named | +0x68 (0 or less: no set at this index) | -1..172 (171 distinct) |
| 0x06c | u32[8] | `mFamilies` | named | +0x6c (unit classes; all eight are added) | [0]: 0..4294967295 (31 distinct) |
| 0x08c | s32[8] | `mUnitTypes` | named | +0x8c (only the positive ones are added) | [0]: -1..803 (81 distinct) |
| 0x0ac | s32 | `mIncludedSet` | named | +0xac (index of the set's mL30) | -1..169 (65 distinct) |
| 0x0b0 | s32 | `mExcludedSet` | named | +0xb0 (index of the set's mL34) | -1..170 (8 distinct) |
| 0x0b4 | bool | `mShowInEditor` | named | +0xb4 (the set's mB39) | 0..1 (2 distinct) |
| 0x0b5 | pad | `(padding)` | pad | tail padding | bytes 000000x170, ccccccx3 |

### 3.26 `db\dbareaeffect.dat`: Area effect templates

* **Loader:** `EEAEManager::Load` 0x67b079 (src/Empire Earth/Civilization Builder/EEAEManager.cpp); record include/ee/EEAreaEffectTemplate.h
* **Container:** `u32 count` + count x 188-byte `EEAreaEffectData`.
* **AoC data:** 50 records, 9,404 bytes unpacked. Bytes per record: read 87%, label 0%, unused 11%, padding 2%.
* **Semantics:** Rows with `mID > 0` become templates; `mKind` picks the area-effect class (economy, healing, morale, cloaking...).
* **Cross-references:** `mGraphicEffect`, `mGraphicEffect18` -> dbgfxeffects; `mUnitSet` -> dbunitset; `mMedia` -> dbgraphics; <- dbobjects `mAreaEffectTemplate0..3`, dbeffects `mAreaEffect`.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[104] | `mName` | named | +0x00 | text 46/50, max len 28, junk after NUL in 50, e.g. "None (don't delete!!!!)" |
| 0x068 | s32 | `mID` | named | +0x68 | -1..49 (47 distinct) |
| 0x06c | s32 | `mKind` | named | +0x6c (> 0 for a template: EEAEManager::Load skips the others) | 0..8 (9 distinct) |
| 0x070 | s32 | `mShape` | named | +0x70 | 0..2 (3 distinct) |
| 0x074 | s32 | `mGraphicEffect` | named | +0x74 (EEGraphicEffect::Get index, none if <= 0) | 0..140 (4 distinct) |
| 0x078 | s32 | `mUnitSet` | named | +0x78 | 0..154 (20 distinct) |
| 0x07c | s32 | `mMode` | named | +0x7c | 0..2 (3 distinct) |
| 0x080 | s32 | `mGraphicEffect18` | named | +0x80 (EEGraphicEffect::Get index) | const 0 |
| 0x084 | s32 | `mMedia` | named | +0x84 (EEMedia::Get index) | 0..338 (2 distinct) |
| 0x088 | s32 | `mMaxLevel` | named | +0x88 | 0..5 (5 distinct) |
| 0x08c | s32 | `mLevel` | named | +0x8c | 0..40 (11 distinct) |
| 0x090 | s32 | `mUnusedValue1` | unused | +0x90 | const 0 |
| 0x094 | s32 | `mUnusedValue2` | unused | +0x94 | const 0 |
| 0x098 | s32 | `mUnusedValue3` | unused | +0x98 | const 0 |
| 0x09c | f32 | `mRadius` | named | +0x9c | -1..13 (15 distinct) |
| 0x0a0 | f32 | `mRed` | named | +0xa0 | 0..1 (3 distinct) |
| 0x0a4 | f32 | `mGreen` | named | +0xa4 | 0..1 (2 distinct) |
| 0x0a8 | f32 | `mBlue` | named | +0xa8 | 0..1 (3 distinct) |
| 0x0ac | f32 | `mAlpha` | named | +0xac | 0..0.14 (2 distinct) |
| 0x0b0 | f32 | `mUnusedFloat` | unused | +0xb0 | const 0 |
| 0x0b4 | bool | `mUnusedFlag1` | unused | +0xb4 | const 0 |
| 0x0b5 | bool | `mUnusedFlag2` | unused | +0xb5 | const 0 |
| 0x0b6 | bool | `mUnusedFlag3` | unused | +0xb6 | const 0 |
| 0x0b7 | bool | `mUnusedFlag4` | unused | +0xb7 | const 0 |
| 0x0b8 | bool | `mUnusedFlag5` | unused | +0xb8 | const 0 |
| 0x0b9 | pad | `(padding)` | pad | tail padding | bytes 000000x46, ccccccx4 |

### 3.27 `db\dbupgrade.dat`: Unit upgrade (improvement) tables

* **Loader:** `EEUCUpgradeTable::Load` 0x4dc6f0 (src/Empire Earth/Database/EEUCUpgradeTable.cpp)
* **Container:** `u32 count` + count x 240-byte `EEUCUpgradeRecord`: name[100], 14 step floats, 15 cost floats, max level, ?, index, ?, ?, repair flag.
* **AoC data:** 76 records, 18,244 bytes unpacked. Bytes per record: read 52%, label 42%, unused 5%, padding 1%.
* **Semantics:** This table puts its index late, at +0xe0, after the data. +0xdc (`mLDC`) looks like the db id. Factor 13 is never filled (a source slip kept by the port).
* **Cross-references:** <- dbobjects `mUpgradeTableIndex`.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | INFERRED from data: the row name (char[100], NUL-terminated) | text 75/76, max len 21, junk after NUL in 3, e.g. "Air Transport WW2" |
| 0x064 | f32 | `mAttackStep` | named | +0x64 | 0..0.15 (2 distinct) |
| 0x068 | f32 | `mHitPointsStep` | named | +0x68 | 0..0.15 (2 distinct) |
| 0x06c | f32 | `mSpeedStep` | named | +0x6c | 0..0.15 (3 distinct) |
| 0x070 | f32 | `mRangeStep` | named | +0x70 | 0..0.15 (2 distinct) |
| 0x074 | f32 | `mShockArmorStep` | named | +0x74 | 0..0.15 (2 distinct) |
| 0x078 | f32 | `mArrowArmorStep` | named | +0x78 | 0..0.4 (4 distinct) |
| 0x07c | f32 | `mPierceArmorStep` | named | +0x7c | 0..0.15 (2 distinct) |
| 0x080 | f32 | `mGunArmorStep` | named | +0x80 | 0..0.4 (6 distinct) |
| 0x084 | f32 | `mLaserArmorStep` | named | +0x84 | 0..0.15 (4 distinct) |
| 0x088 | f32 | `mMissileArmorStep` | named | +0x88 | 0..0.15 (3 distinct) |
| 0x08c | f32 | `mFlightTimeStep` | named | +0x8c | 0..0.25 (3 distinct) |
| 0x090 | f32 | `mLeadershipStep` | named | +0x90 | const 0 |
| 0x094 | f32 | `mAreaEffectStep` | named | +0x94 | 0..0.15 (2 distinct) |
| 0x098 | f32 | `mTransportStep` | named | +0x98 | const 0 |
| 0x09c | f32 | `mAttackCost` | named | +0x9c | 0..2 (14 distinct) |
| 0x0a0 | f32 | `mHitPointsCost` | named | +0xa0 | 0..2 (14 distinct) |
| 0x0a4 | f32 | `mSpeedCost` | named | +0xa4 | 0..2.5 (20 distinct) |
| 0x0a8 | f32 | `mRangeCost` | named | +0xa8 | 0..2 (13 distinct) |
| 0x0ac | f32 | `mShockArmorCost` | named | +0xac | 0..0.75 (4 distinct) |
| 0x0b0 | f32 | `mArrowArmorCost` | named | +0xb0 | 0..0.75 (6 distinct) |
| 0x0b4 | f32 | `mPierceArmorCost` | named | +0xb4 | 0..0.75 (7 distinct) |
| 0x0b8 | f32 | `mGunArmorCost` | named | +0xb8 | 0..2 (8 distinct) |
| 0x0bc | f32 | `mLaserArmorCost` | named | +0xbc | 0..2 (6 distinct) |
| 0x0c0 | f32 | `mMissileArmorCost` | named | +0xc0 | 0..0.75 (4 distinct) |
| 0x0c4 | f32 | `mFlightTimeCost` | named | +0xc4 | 0..0.5 (3 distinct) |
| 0x0c8 | f32 | `mLeadershipCost` | named | +0xc8 | const 0 |
| 0x0cc | f32 | `mRepairCost` | named | +0xcc | const 0 |
| 0x0d0 | f32 | `mAreaEffectCost` | named | +0xd0 | 0..1.5 (6 distinct) |
| 0x0d4 | f32 | `mTransportCost` | named | +0xd4 | const 0 |
| 0x0d8 | s32 | `mMaxLevel` | named | +0xd8 | 0..2 (3 distinct) |
| 0x0dc | s32 | `mUnused1` | unused | +0xdc | 0..420 (76 distinct) |
| 0x0e0 | s32 | `mIndex` | named | +0xe0 (negative: no entry) | 0..75 (76 distinct) |
| 0x0e4 | s32 | `mUnused2` | unused | +0xe4 | 0..475 (15 distinct) |
| 0x0e8 | s32 | `mUnused3` | unused | +0xe8 | 0..15 (15 distinct) |
| 0x0ec | bool | `mHasRepairStep` | named | +0xec | const 0 |
| 0x0ed | pad | `(padding)` | pad | tail padding | bytes 000000x75, ccccccx1 |

### 3.28 `db\dbcpbehavior.dat`: Computer-player behaviour values

* **Loader:** `EECPBehavior::Load` 0x4d5f97 (src/Empire Earth/Database/EECPBehaviorTable.cpp); record include/ee/EECPBehavior.h
* **Container:** `u32 count` + count x 56-byte `EECPBehavior` (**no name column**; +0x00 = dbId, +0x04 = slot index).
* **AoC data:** 126 records, 7,060 bytes unpacked. Bytes per record: read 80%, label 0%, unused 14%, padding 5%.
* **Semantics:** Stored into `gCPBehaviors[index]` (129 slots). The names come from Language.rc 28000+index and from the `EECPBehaviorID` enum (114 named ids). Slot 48 (maximum attack response range) is squared after loading.
* **Cross-references:** none
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | s32 | `mUnused1` | unused | +0x00 | 1..133 (126 distinct) |
| 0x004 | s32 | `mIndex` | named | +0x04 (the slot; records with index <= 0 are skipped) | 0..125 (126 distinct) |
| 0x008 | s32 | `mIntValue1Min` | named | +0x08 | 0..1 (2 distinct) |
| 0x00c | s32 | `mIntValue1Max` | named | +0x0c | 0..9999 (6 distinct) |
| 0x010 | s32 | `mIntValue1` | named | +0x10 | -1..600 (34 distinct) |
| 0x014 | s32 | `mIntValue2Min` | named | +0x14 | 0..1 (2 distinct) |
| 0x018 | s32 | `mIntValue2Max` | named | +0x18 | 0..9999 (5 distinct) |
| 0x01c | s32 | `mIntValue2` | named | +0x1c | -1..400 (27 distinct) |
| 0x020 | s32 | `mUnused2` | unused | +0x20 | -1..0 (2 distinct) |
| 0x024 | s32 | `mFloatSliderMin` | named | +0x24 | 0..1 (2 distinct) |
| 0x028 | s32 | `mFloatSliderMax` | named | +0x28 | 0..999 (4 distinct) |
| 0x02c | s32 | `mFloatSliderScale` | named | +0x2c | 0..100 (3 distinct) |
| 0x030 | f32 | `mFloatValue` | named | +0x30 (squared after loading, for record 48 only) | -1..200 (36 distinct) |
| 0x034 | bool | `mIsSettable` | named | +0x34 | 0..1 (2 distinct) |
| 0x035 | pad | `(padding)` | pad | tail padding | bytes 000000x126 |

### 3.29 `db\dbgamevariant.dat`: Game variants

* **Loader:** `EEGameVariant::Load` 0x4d693c (src/Empire Earth/Database/EEGameVariant.cpp); record include/ee/EEGameVariant.h
* **Container:** `u32 count` + count x 276-byte `EEGameVariantData`.
* **AoC data:** 3 records, 832 bytes unpacked. Bytes per record: read 45%, label 36%, unused 17%, padding 1%.
* **Semantics:** Standard / Tournament / Scenarios: 10 (unit set, factor) pairs plus cost and work-time factors.
* **Cross-references:** `mSet0..9`, `mCostSet` -> dbunitset; `mNameID` -> Language.dll.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | +0x00 | text 3/3, max len 10, e.g. "Scenarios" |
| 0x064 | s32 | `mUnusedDatabaseID` | unused | +0x64 | 1..5 (3 distinct) |
| 0x068 | s32 | `mIndex` | named | +0x68 (the loader skips a record whose index is negative) -> +0x70 | 0..2 (3 distinct) |
| 0x06c | f32 | `mFactor0` | named | +0x6c -> +0x04 (factor 0) | -0.5..0 (2 distinct) |
| 0x070 | s32 | `mSet0` | named | +0x70 -> +0x48 (then (long, float) pairs: +0x70/+0x74 .. +0xb8/+0xbc) | 42..141 (2 distinct) |
| 0x074 | f32 | `mFactor1` | named | +0x74 -> +0x08 | -0.7..0 (2 distinct) |
| 0x078 | s32 | `mSet1` | named | +0x78 -> +0x4c | -1..144 (2 distinct) |
| 0x07c | f32 | `mFactor2` | named | +0x7c -> +0x0c | -0.4..0 (2 distinct) |
| 0x080 | s32 | `mSet2` | named | +0x80 -> +0x50 | -1..143 (2 distinct) |
| 0x084 | f32 | `mFactor3` | named | +0x84 -> +0x10 | const 0 |
| 0x088 | s32 | `mSet3` | named | +0x88 -> +0x54 | const -1 |
| 0x08c | f32 | `mFactor4` | named | +0x8c -> +0x14 | const 0 |
| 0x090 | s32 | `mSet4` | named | +0x90 -> +0x58 | const -1 |
| 0x094 | f32 | `mFactor5` | named | +0x94 -> +0x18 | const 0 |
| 0x098 | s32 | `mSet5` | named | +0x98 -> +0x5c | const -1 |
| 0x09c | f32 | `mFactor6` | named | +0x9c -> +0x1c | const 0 |
| 0x0a0 | s32 | `mSet6` | named | +0xa0 -> +0x60 | const -1 |
| 0x0a4 | f32 | `mFactor7` | named | +0xa4 -> +0x20 | const 0 |
| 0x0a8 | s32 | `mSet7` | named | +0xa8 -> +0x64 | const -1 |
| 0x0ac | f32 | `mFactor8` | named | +0xac -> +0x24 | const 0 |
| 0x0b0 | s32 | `mSet8` | named | +0xb0 -> +0x68 | const -1 |
| 0x0b4 | f32 | `mFactor9` | named | +0xb4 -> +0x28 | const 0 |
| 0x0b8 | s32 | `mSet9` | named | +0xb8 -> +0x6c | const -1 |
| 0x0bc | f32 | `mDepositFactor` | named | +0xbc -> +0x2c | const 1 |
| 0x0c0 | f32 | `mEpochCostFactor` | named | +0xc0 -> +0x30 | 1..2.64 (2 distinct) |
| 0x0c4 | f32 | `mWorkTimeFactor` | named | +0xc4 -> +0x34 | 1..1.66 (2 distinct) |
| 0x0c8 | s32 | `mMoraleBonus` | named | +0xc8 -> +0x74 | 0..2 (2 distinct) |
| 0x0cc | s32 | `mNameID` | named | +0xcc -> +0x78 (gResource string id: 31255 / 31254) | 0..31255 (3 distinct) |
| 0x0d0 | f32 | `mFirstEpochCostFactor` | named | +0xd0 -> +0x38 | 0.2..1 (3 distinct) |
| 0x0d4 | f32 | `mSecondEpochCostFactor` | named | +0xd4 -> +0x3c | 0.5..1 (3 distinct) |
| 0x0d8 | f32 | `mCostSetFactor` | named | +0xd8 -> +0x40 | 1..2 (3 distinct) |
| 0x0dc | s32 | `mCostSet` | named | +0xdc -> +0x7c | 108..132 (2 distinct) |
| 0x0e0 | f32 | `mEarthquakeDamageFactor` | named | +0xe0 -> +0x44 | 1..2.86 (2 distinct) |
| 0x0e4 | s32 | `mUnused1` | unused | +0xe4 (0 in every record; not copied) | const 0 |
| 0x0e8 | s32 | `mUnused2` | unused | +0xe8 | const 0 |
| 0x0ec | s32 | `mUnused3` | unused | +0xec | const 0 |
| 0x0f0 | s32 | `mUnused4` | unused | +0xf0 | const 0 |
| 0x0f4 | s32 | `mUnused5` | unused | +0xf4 | const 0 |
| 0x0f8 | s32 | `mUnused6` | unused | +0xf8 | const 0 |
| 0x0fc | s32 | `mUnused7` | unused | +0xfc | const 0 |
| 0x100 | s32 | `mUnused8` | unused | +0x100 | const 0 |
| 0x104 | s32 | `mUnused9` | unused | +0x104 | const 0 |
| 0x108 | s32 | `mUnused10` | unused | +0x108 | const 0 |
| 0x10c | bool | `mSelectable` | named | +0x10c -> +0x80 | 0..1 (2 distinct) |
| 0x10d | pad | `(padding)` | pad | alignment padding | bytes 000000x3 |
| 0x110 | s32 | `mUnused11` | unused | +0x110 (0 in every record) | const 0 |

### 3.30 `db\dbambientsounds.dat`: Ambient sounds

* **Loader:** `EEAmbientSoundManager::Load` 0x525012 (src/Empire Earth/Empire Earth/EEAmbientSoundManager.cpp); record include/ee/EEAmbientSound.h
* **Container:** `u32 count` + count x 152-byte `EEAmbientSoundRecord`.
* **AoC data:** 28 records, 4,260 bytes unpacked. Bytes per record: read 21%, label 66%, unused 13%, padding 0%.
* **Semantics:** Each record is stored at `mSounds[mIndex]`. Index < 0 is skipped. Slots 7 (wind) and 9 (birds) are used by fixed index.
* **Cross-references:** `mSoundID` -> dbsounds.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | INFERRED from data: the row name (char[100], NUL-terminated) | text 27/28, max len 18, junk after NUL in 2, e.g. "Day Town" |
| 0x064 | s32 | `mUnusedDatabaseID` | unused | INFERRED: unique per row, gappy: the database row id | 0..28 (28 distinct) |
| 0x068 | s32 | `mIndex` | named | +0x68 (the slot; < 0: unused) | -1..27 (28 distinct) |
| 0x06c | s32 | `mSoundID` | named | +0x6c | -1..1070 (24 distinct) |
| 0x070 | f32 | `mMaximumVolume` | named | +0x70 | 0..0.7 (14 distinct) |
| 0x074 | f32 | `mStartVolume` | named | +0x74 | 0..0.3 (11 distinct) |
| 0x078 | f32 | `mFadeUp` | named | +0x78 (Init copies it to +0x28 with a plain mov) | 0..1.35 (4 distinct) |
| 0x07c | f32 | `mFadeDown` | named | +0x7c (to +0x2c) | 0..0.85 (2 distinct) |
| 0x080 | f32 | `mProbability` | named | +0x80 | 0..1 (12 distinct) |
| 0x084 | s32 | `(unused gap)` | unused | undecoded | const 0 |
| 0x088 | s32 | `(unused gap)` | unused | undecoded | const 0 |
| 0x08c | s32 | `(unused gap)` | unused | undecoded | const 0 |
| 0x090 | s32 | `mIntervalSeconds` | named | +0x90 | 0..40 (11 distinct) |
| 0x094 | s32 | `(unused gap)` | unused | undecoded | const 0 |

### 3.31 `db\dbuifonts.dat`: UI fonts

* **Loader:** `UIFontTable::Create` 0x1007011f (src/Low-Level Engine/User Interface/UIFontTable.cpp); loaded first by `UIManager::LoadDatabase` 0x1007a78a
* **Container:** `u32 count` + count x 128-byte `UIDbDataFonts`.
* **AoC data:** 61 records, 7,812 bytes unpacked. Bytes per record: read 98%, label 0%, unused 0%, padding 2%.
* **Semantics:** A row with an empty name is NULL. The face name comes from the UI string table (`mStringID`), not from the record name.
* **Cross-references:** <- dbuicontrols `mFontTableIndex`.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[108] | `mFontName` | named | +0x00 empty: no font | text 57/61, max len 29, junk after NUL in 61, e.g. "No Font" |
| 0x06c | u32 | `mStringID` | named | +0x6c face name in the string table | 0..4 (5 distinct) |
| 0x070 | u32 | `mSize` | named | +0x70 | 0..36 (14 distinct) |
| 0x074 | u32 | `mQuality` | named | +0x74 | 0..2 (2 distinct) |
| 0x078 | bool | `mBold` | named | +0x78 style bit 0x01 | 0..1 (2 distinct) |
| 0x079 | bool | `mItalic` | named | +0x79 style bit 0x02 | const 0 |
| 0x07a | bool | `mUnderline` | named | +0x7a style bit 0x04 | 0..1 (2 distinct) |
| 0x07b | bool | `mStyle10` | named | +0x7b style bit 0x10 | 0..1 (2 distinct) |
| 0x07c | bool | `mStyle08` | named | +0x7c style bit 0x08 | const 0 |
| 0x07d | char[3] | `(padding)` | pad |  | text 4/61, max len 3, e.g. "ÌÌÌ" |

### 3.32 `db\dbuiforms.dat`: UI forms (screens)

* **Loader:** `UIForm::Create` 0x100716c0 (src/Low-Level Engine/User Interface/UIForm.cpp); record include/lle/UIForm.h (exported type `UIDbDataForms`)
* **Container:** `u32 count` + count x 268-byte `UIDbDataForms`.
* **AoC data:** 40 records, 10,724 bytes unpacked. Bytes per record: read 59%, label 37%, unused 3%, padding 0%.
* **Semantics:** Here the form name and music file come first and the ids follow at +0xc8/+0xcc. `mFormID <= 0` means no form.
* **Cross-references:** <- dbuicontrols/dbuiback `mFormID`, dbuiformevents `mOwnerID`.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | +0x000 | text 38/40, max len 38, junk after NUL in 2, e.g. "Test form" |
| 0x064 | char[100] | `mMusicFile` | named | +0x064 "none" stops the music | text 38/40, max len 20, junk after NUL in 3, e.g. "sounds\G7_99.mp3" |
| 0x0c8 | s32 | `mUnusedID` | unused | +0x0c8 (provisional name) | 0..45 (39 distinct) |
| 0x0cc | s32 | `mFormID` | named | +0x0cc 0 or less: no form | -1..39 (39 distinct) |
| 0x0d0 | u32 | `mControlCount` | named | +0x0d0 | 4..2200 (24 distinct); 0xCC in 2 |
| 0x0d4 | u32 | `mControlGroupCount` | named | +0x0d4 | 1..11 (7 distinct); 0xCC in 2 |
| 0x0d8 | f32 | `mLeftOffset` | named | +0x0d8 | 0..0.2503 (7 distinct) |
| 0x0dc | f32 | `mRightOffset` | named | +0x0dc | 0..1 (9 distinct) |
| 0x0e0 | f32 | `mBottomOffset` | named | +0x0e0 | 0..1 (13 distinct) |
| 0x0e4 | f32 | `mTopOffset` | named | +0x0e4 | 0..0.6327 (8 distinct) |
| 0x0e8 | u32 | `mDesignedWidth` | named | +0x0e8 | 0..1600 (4 distinct) |
| 0x0ec | u32 | `mDesignedHeight` | named | +0x0ec | 0..1200 (4 distinct) |
| 0x0f0 | u32 | `mHelpTextTimeDelay` | named | +0x0f0 | 0..2000 (8 distinct) |
| 0x0f4 | u32 | `mUnused` | unused | +0x0f4 (provisional name) | const 0 |
| 0x0f8 | u32 | `mLeftOffsetType` | named | +0x0f8 | 0..4 (2 distinct) |
| 0x0fc | u32 | `mRightOffsetType` | named | +0x0fc | 0..4 (3 distinct) |
| 0x100 | u32 | `mTopOffsetType` | named | +0x100 | 0..4 (2 distinct) |
| 0x104 | u32 | `mBottomOffsetType` | named | +0x104 | 0..4 (3 distinct) |
| 0x108 | bool | `mTabbingOn` | named | +0x108 | 0..1 (2 distinct) |
| 0x109 | bool | `mUseToolTips` | named | +0x109 | 0..1 (2 distinct) |
| 0x10a | bool | `mUseExclusiveCoords` | named | +0x10a | 0..1 (2 distinct) |
| 0x10b | pad | `(padding)` | pad | tail padding | bytes 00x38, ccx2 |

### 3.33 `db\dbuiback.dat`: UI backgrounds

* **Loader:** `UIBackgroundTable::Create` 0x1005c631 (src/Low-Level Engine/User Interface/UIBackgroundTable.cpp); record include/lle/UIBackgroundTable.h
* **Container:** `u32 count` + count x 308-byte `UIDbDataBackgrounds`.
* **AoC data:** 167 records, 51,440 bytes unpacked. Bytes per record: read 99%, label 0%, unused 0%, padding 1%.
* **Semantics:** An empty name leaves the slot NULL (75 holes). The tile percentages hold `inf` in 39 rows.
* **Cross-references:** `mFormID` -> dbuiforms; texture file -> textures.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `mName` | named | +0x000 an empty name leaves the slot NULL | text 92/167, max len 40, junk after NUL in 77, e.g. "CB Main Form Background" |
| 0x064 | char[100] | `mTextureFile` | named | +0x064 | text 92/167, max len 31, junk after NUL in 76, e.g. "textures\civbuilder_back.sst" |
| 0x0c8 | f32 | `mTopOffset` | named | +0x0c8 | 0..0.9417 (32 distinct) |
| 0x0cc | f32 | `mBottomOffset` | named | +0x0cc | 0..1 (33 distinct) |
| 0x0d0 | f32 | `mLeftOffset` | named | +0x0d0 | 0..0.9987 (40 distinct) |
| 0x0d4 | f32 | `mRightOffset` | named | +0x0d4 | 0..1 (41 distinct) |
| 0x0d8 | f32 | `mTileWidthPercent` | named | +0x0d8 | -1.05..inf (20 distinct) |
| 0x0dc | f32 | `mTileHeightPercent` | named | +0x0dc | 0..inf (10 distinct) |
| 0x0e0 | f32 | `mOpacity` | named | +0x0e0 | 0..1 (9 distinct) |
| 0x0e4 | s32 | `mID` | named | +0x0e4 | 0..125 (93 distinct) |
| 0x0e8 | u32 | `mBackgroundID` | named | +0x0e8 | 14..4294967295 (93 distinct) |
| 0x0ec | u32 | `mFormID` | named | +0x0ec | 2..4294967295 (38 distinct) |
| 0x0f0 | s32 | `mTextureTop` | named | +0x0f0 | 0..211 (9 distinct) |
| 0x0f4 | s32 | `mTextureLeft` | named | +0x0f4 | 0..255 (16 distinct) |
| 0x0f8 | s32 | `mTextureBottom` | named | +0x0f8 | 0..768 (15 distinct) |
| 0x0fc | s32 | `mTextureRight` | named | +0x0fc | -1..1024 (17 distinct) |
| 0x100 | u32 | `mBackgroundType` | named | +0x100 | 0..101 (4 distinct) |
| 0x104 | u32 | `mTopOffsetType` | named | +0x104 | 0..4 (2 distinct) |
| 0x108 | u32 | `mBottomOffsetType` | named | +0x108 | 0..4 (3 distinct) |
| 0x10c | u32 | `mLeftOffsetType` | named | +0x10c | 0..4 (2 distinct) |
| 0x110 | u32 | `mRightOffsetType` | named | +0x110 | 0..4 (3 distinct) |
| 0x114 | s32 | `mNumCols` | named | +0x114 | 0..4 (3 distinct) |
| 0x118 | s32 | `mNumRows` | named | +0x118 | 0..3 (3 distinct) |
| 0x11c | s32 | `mDrawPriority` | named | +0x11c at least 1 | 0..6 (7 distinct) |
| 0x120 | s32 | `mRed` | named | +0x120 0..255 | 0..75 (2 distinct) |
| 0x124 | s32 | `mGreen` | named | +0x124 | 0..75 (2 distinct) |
| 0x128 | s32 | `mBlue` | named | +0x128 | 0..75 (3 distinct) |
| 0x12c | bool | `mVisible` | named | +0x12c | 0..1 (2 distinct) |
| 0x12d | bool | `mHasAlpha` | named | +0x12d | 0..1 (2 distinct) |
| 0x12e | bool | `mAttribute1` | named | +0x12e | 0..1 (2 distinct) |
| 0x12f | bool | `mAttribute2` | named | +0x12f | 0..1 (2 distinct) |
| 0x130 | bool | `mAttribute3` | named | +0x130 | 0..1 (2 distinct) |
| 0x131 | pad | `(padding)` | pad | tail padding | bytes 000000x91, ccccccx75 |

### 3.34 `db\dbuicontrols.dat`: UI controls

* **Loader:** `UIControlsTable::Create` 0x10067ee6, record ctor 0x1006796f (src/Low-Level Engine/User Interface/UIControlsTable.cpp)
* **Container:** `u32 count` + count x 860-byte `UIDbControl`.
* **AoC data:** 3401 records, 2,924,864 bytes unpacked. Bytes per record: read 84%, label 12%, unused 4%, padding 0%.
* **Semantics:** This is the largest file (2.9 MB, 60% holes). A row with `mID >= 0` is added to its form (`GetFormByID(mFormID)`). `mControlType` selects the control class (bitmap, button, list box, slider...). There are three rectangles each for the normal, pressed and focused texture. No static_assert pins it in this tree, but the parse confirms 860 bytes (0x35c, with one tail pad byte).
* **Cross-references:** `mFormID` -> dbuiforms; `mFontTableIndex` -> dbuifonts; `mStringTableBegin/End/ID` -> UI string table (Language.dll); file names -> textures, sounds; <- dbuicontrolevents `mOwnerID` (row index).
* **Suggested text form:** CSV (wide) or YAML by form

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | +0x000 | text 1372/3401, max len 40, junk after NUL in 2056, e.g. "Save Screen Title" |
| 0x064 | char[100] | `mNormalFileName` | named | +0x064 | text 234/3401, max len 33, junk after NUL in 2061, e.g. "textures\civ buttons_01.sst" |
| 0x0c8 | char[100] | `mPressedFileName` | named | +0x0c8 | text 202/3401, max len 34, junk after NUL in 2060, e.g. "textures\civ buttons_01.sst" |
| 0x12c | char[100] | `mFocusedFileName` | named | +0x12c | text 0/3401, max len 0, junk after NUL in 2065 |
| 0x190 | char[100] | `mButtonDownSound` | named | +0x190 | text 490/3401, max len 18, junk after NUL in 2065, e.g. "sounds\button3.wav" |
| 0x1f4 | char[100] | `mButtonUpSound` | named | +0x1f4 | text 0/3401, max len 0, junk after NUL in 2064 |
| 0x258 | f32 | `mBackgroundOpacity` | named | +0x258 | 0..1 (15 distinct) |
| 0x25c | f32 | `mTextBoxOpacity` | named | +0x25c | 0..1 (4 distinct) |
| 0x260 | f32 | `mScrollBarBgOpacity` | named | +0x260 | 0..1 (5 distinct) |
| 0x264 | u32[5] | `mUnused1` | unused | +0x264 | [0]: const 0 |
| 0x278 | f32 | `mLeftOffset` | named | +0x278 | 0..349 (315 distinct) |
| 0x27c | f32 | `mRightOffset` | named | +0x27c | 0..299 (347 distinct) |
| 0x280 | f32 | `mBottomOffset` | named | +0x280 | 0..504 (360 distinct) |
| 0x284 | f32 | `mTopOffset` | named | +0x284 | 0..421 (329 distinct) |
| 0x288 | s32 | `mUnusedDbRecordId` | unused | +0x288 | 0..1945 (1373 distinct) |
| 0x28c | s32 | `mID` | named | +0x28c | -1..3400 (1373 distinct) |
| 0x290 | s32 | `mFormID` | named | +0x290 | 0..39 (38 distinct) |
| 0x294 | u32 | `mGroup` | named | +0x294 | 0..9 (10 distinct) |
| 0x298 | u32 | `mControlType` | named | +0x298 | 0..4294967295 (42 distinct) |
| 0x29c | s32 | `mFormIndex` | named | +0x29c | 0..1531 (343 distinct) |
| 0x2a0 | s32 | `mBorderThickness` | named | +0x2a0 | 0..3 (4 distinct) |
| 0x2a4 | u32 | `mLeftOffsetType` | named | +0x2a4 | 0..4 (4 distinct) |
| 0x2a8 | u32 | `mRightOffsetType` | named | +0x2a8 | 0..4 (4 distinct) |
| 0x2ac | u32 | `mTopOffsetType` | named | +0x2ac | 0..4 (3 distinct) |
| 0x2b0 | u32 | `mBottomOffsetType` | named | +0x2b0 | 0..4 (3 distinct) |
| 0x2b4 | u32 | `mBackgroundRed` | named | +0x2b4 | 0..255 (10 distinct) |
| 0x2b8 | u32 | `mBackgroundGreen` | named | +0x2b8 | 0..255 (12 distinct) |
| 0x2bc | u32 | `mBackgroundBlue` | named | +0x2bc | 0..255 (8 distinct) |
| 0x2c0 | s32 | `mStringTableBegin` | named | +0x2c0 | -1..32813 (303 distinct) |
| 0x2c4 | s32 | `mStringTableEnd` | named | +0x2c4 | -1..32815 (5 distinct) |
| 0x2c8 | u32 | `mFontTableIndex` | named | +0x2c8 | 0..60 (28 distinct) |
| 0x2cc | u32 | `mTextForegroundRed` | named | +0x2cc | 0..255 (6 distinct) |
| 0x2d0 | u32 | `mTextForegroundGreen` | named | +0x2d0 | 0..255 (3 distinct) |
| 0x2d4 | u32 | `mTextForegroundBlue` | named | +0x2d4 | 0..255 (5 distinct) |
| 0x2d8 | s32 | `mTextAlignment` | named | +0x2d8 | 0..2 (3 distinct) |
| 0x2dc | s32 | `mNormalLeft` | named | +0x2dc | 0..199 (9 distinct) |
| 0x2e0 | s32 | `mNormalRight` | named | +0x2e0 | 0..253 (19 distinct) |
| 0x2e4 | s32 | `mNormalTop` | named | +0x2e4 | 0..211 (15 distinct) |
| 0x2e8 | s32 | `mNormalBottom` | named | +0x2e8 | 0..256 (26 distinct) |
| 0x2ec | s32 | `mPressedLeft` | named | +0x2ec | 0..174 (11 distinct) |
| 0x2f0 | s32 | `mPressedRight` | named | +0x2f0 | 0..256 (13 distinct) |
| 0x2f4 | s32 | `mPressedTop` | named | +0x2f4 | 0..211 (15 distinct) |
| 0x2f8 | s32 | `mPressedBottom` | named | +0x2f8 | 0..256 (21 distinct) |
| 0x2fc | s32 | `mFocusedLeft` | named | +0x2fc | const 0 |
| 0x300 | s32 | `mFocusedRight` | named | +0x300 | const 0 |
| 0x304 | s32 | `mFocusedTop` | named | +0x304 | const 0 |
| 0x308 | s32 | `mFocusedBottom` | named | +0x308 | const 0 |
| 0x30c | s32 | `mMaxChars` | named | +0x30c | 0..255 (7 distinct) |
| 0x310 | u32 | `mSelectedListBoxRed` | named | +0x310 | 0..255 (5 distinct) |
| 0x314 | u32 | `mSelectedListBoxGreen` | named | +0x314 | 0..255 (4 distinct) |
| 0x318 | u32 | `mSelectedListBoxBlue` | named | +0x318 | 0..255 (4 distinct) |
| 0x31c | s32 | `mStartCurrentSelection` | named | +0x31c | const 0 |
| 0x320 | u32 | `mFocusedLineRed` | named | +0x320 | 0..255 (5 distinct) |
| 0x324 | u32 | `mFocusedLineGreen` | named | +0x324 | 0..255 (4 distinct) |
| 0x328 | u32 | `mFocusedLineBlue` | named | +0x328 | 0..255 (4 distinct) |
| 0x32c | s32 | `mStartRange` | named | +0x32c | 0..150 (4 distinct) |
| 0x330 | s32 | `mEndRange` | named | +0x330 | 0..600 (6 distinct) |
| 0x334 | s32 | `mCurrentRange` | named | +0x334 | 0..200 (5 distinct) |
| 0x338 | s32 | `mPageSize` | named | +0x338 | 0..30 (6 distinct) |
| 0x33c | s32 | `mStringTableID` | named | +0x33c | 0..32816 (147 distinct) |
| 0x340 | s32 | `mBlinkRate` | named | +0x340 | 0..500 (2 distinct) |
| 0x344 | u32[3] | `mUnused2` | unused | +0x344 | [0]: 0..4294967295 (3 distinct) |
| 0x350 | bool | `mIsVisible` | named | +0x350 | 0..1 (2 distinct) |
| 0x351 | bool | `mStartChecked` | named | +0x351 | 0..1 (2 distinct) |
| 0x352 | bool | `mListBoxAlwaysDirty` | named | +0x352 | 0..1 (2 distinct) |
| 0x353 | bool | `mSortListBox` | named | +0x353 | 0..1 (2 distinct) |
| 0x354 | bool | `mFocusedHasAlpha` | named | +0x354 | const 0 |
| 0x355 | bool | `mPressedHasAlpha` | named | +0x355 | 0..1 (2 distinct) |
| 0x356 | bool | `mNormalHasAlpha` | named | +0x356 | 0..1 (2 distinct) |
| 0x357 | bool | `mAlwaysDirty` | named | +0x357 | 0..1 (2 distinct) |
| 0x358 | bool | `mFitRectToText` | named | +0x358 | 0..1 (2 distinct) |
| 0x359 | bool | `mScaleToControl` | named | +0x359 | 0..1 (2 distinct) |
| 0x35a | bool | `mLoopMovie` | named | +0x35a | 0..1 (2 distinct) |
| 0x35b | pad | `(padding)` | pad | tail padding | bytes 00x3401 |

### 3.35 `db\dbuicontrolevents.dat`: UI control events

* **Loader:** `UIEvent::CreateControlEvents` 0x1006f398 (src/Low-Level Engine/User Interface/UIEvent.cpp)
* **Container:** `u32 count` + count x 116-byte `UIDbDataEvents`: name[100], dbId, event id, owner, event type.
* **AoC data:** 252 records, 29,236 bytes unpacked. Bytes per record: read 10%, label 86%, unused 3%, padding 0%.
* **Semantics:** Rows with event id < 0 are skipped.
* **Cross-references:** `mOwnerID` -> dbuicontrols row index; `mEventID` -> the exe's event classes (EEUIManager switch).
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | INFERRED from data: the row name (char[100], NUL-terminated) | text 234/252, max len 48, junk after NUL in 23, e.g. "Load Delete" |
| 0x064 | s32 | `mUnusedDatabaseID` | unused | INFERRED: unique per row, gappy: the database row id | 0..368 (235 distinct) |
| 0x068 | s32 | `mEventID` | named | +0x68 negative: no event | -1..251 (235 distinct) |
| 0x06c | u32 | `mOwnerID` | named | +0x6c control table index / form ID | 133..4294967295 (191 distinct) |
| 0x070 | u32 | `mEventType` | named | +0x70 | 0..4294967295 (13 distinct) |

### 3.36 `db\dbuiformevents.dat`: UI form events

* **Loader:** `UIEvent::CreateFormEvents` 0x1006f42a
* **Container:** Same layout as dbuicontrolevents.
* **AoC data:** 21 records, 2,440 bytes unpacked. Bytes per record: read 10%, label 86%, unused 3%, padding 0%.
* **Semantics:** Rows with event id < 0 are skipped.
* **Cross-references:** `mOwnerID` -> form id (dbuiforms `mFormID`).
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | char[100] | `label` | label | INFERRED from data: the row name (char[100], NUL-terminated) | text 20/21, max len 23, junk after NUL in 2, e.g. "Main Screen Open" |
| 0x064 | s32 | `mUnusedDatabaseID` | unused | INFERRED: unique per row, gappy: the database row id | 0..21 (21 distinct) |
| 0x068 | s32 | `mEventID` | named | +0x68 negative: no event | -1..20 (21 distinct) |
| 0x06c | u32 | `mOwnerID` | named | +0x6c control table index / form ID | 5..29 (12 distinct); 0xCC in 1 |
| 0x070 | u32 | `mEventType` | named | +0x70 | 0..8 (4 distinct); 0xCC in 1 |

### 3.37 `db\dbuihotkey.dat`: UI hot keys

* **Loader:** `UIHotKey::CreateHotKeys` 0x10076fa8 (src/Low-Level Engine/User Interface/UIHotKey.cpp)
* **Container:** `u32 count` + count x 124-byte `UIDbDataHotKeys`: dbId, index, name[100] at +0x08, 3 modifier DIK codes (0x7fff = none), key DIK code.
* **AoC data:** 32 records, 3,972 bytes unpacked. Bytes per record: read 16%, label 81%, unused 3%, padding 0%.
* **Semantics:** Index < 0 = NULL.
* **Cross-references:** <- dbbuttons/dbtechtree `mHotKey`.
* **Suggested text form:** CSV

| Off | Type | Field | Class | Source comment | Observed (AoC) |
|---|---|---|---|---|---|
| 0x000 | s32 | `mUnused` | unused | INFERRED: unique per row, gappy: the database row id | 0..33 (32 distinct) |
| 0x004 | s32 | `mID` | named | +0x04 negative: no hot key | -1..31 (32 distinct) |
| 0x008 | char[100] | `label` | label | +0x08 | text 31/32, max len 13, junk after NUL in 2, e.g. "A key" |
| 0x06c | u32[3] | `mModifiers` | named | +0x6c DIK_* codes, 0x7fff: none | [0]: 32767..4294967295 (2 distinct) |
| 0x078 | u32 | `mKey` | named | +0x78 | 16..4294967295 (32 distinct) |

### 3.38 `db\dbrandommap.dat`: random map terrain table (mis-loaded, dead)

* **Loader:** `EERandomMapTable::Load` 0x4da47b (src/Empire Earth/Database/EERandomMapTable.cpp). It reads `u32 count` and then
  count x 0x84 bytes into a local buffer that is **thrown away**. It creates `count` zeroed `EERandomMapTable`
  objects; nothing reads them.
* **Real layout (decoded from the data; cross-checked with the converter log's 29 "RandomMap" rows):**
  `u32 a = 10` (probably the number of groups; the highest group id is 10), `u32 n = 29`, then n x 132-byte records.
  The file is 3836 = 8 + 29 x 132 bytes. The game treats `a` as the count, so it reads 10 records that are shifted by
  4 bytes.
* **Suggested:** CSV (only for completeness; editing it changes nothing in game).

| Off | Type | Field | Class | Comment | Observed |
|---|---|---|---|---|---|
| 0x00 | s32 | `(db id)` | inferred | 4..71, unique | |
| 0x04 | s32 | `(group)` | inferred | 1..10; two or three rows per group | |
| 0x08 | char[100] | `(name)` | inferred | terrain name ("Grass Tufts", "Snow", "Ice", "StoneWork") | |
| 0x6c | s32 | `(min?)` | unknown | -10, -2, 0 | elevation-like |
| 0x70 | s32 | `(max?)` | unknown | -4, -3, -1, 20 | elevation-like |
| 0x74 | s32[4] | `(terrain ids?)` | unknown | four equal values per row (2..71: dbterrain ids?) | |

### 3.39 `db\dbterraingraytextures.dat`: grey terrain textures (not loaded by AoC)

* **Loader:** none in the AoC exe (only the key `kDbTerrainGrayTexturesFile`). It is still in `data.ssa` and listed
  as "TerrainGrayTextures" in the converter log.
* **Container (decoded from the data, parses exactly to the end):** `u32 count = 59`; per entry: one string (`u32 len`
  + bytes, no NUL, e.g. `textures\GWildGrass_1.sst`), then a 120-byte record.
* **Suggested:** CSV.

| Off | Type | Field | Class | Comment |
|---|---|---|---|---|
| 0x00 | char[100] | `(name)` | inferred | "Grass Rough (x)" ... "Space" |
| 0x64 | s32 | `(db id)` | inferred | 2..60 |
| 0x68 | s32 | `(index)` | inferred | -1 (row 0) .. 58 |
| 0x6c | s32[3] | `(rgb?)` | inferred | 0..255 triples (e.g. 130,190,50), like dbterrain's colour |


---

## 4. Plain-text formats (already text)

| Format | Where | Parser | Syntax |
|---|---|---|---|
| `.tai` unit AI scripts | `data.ssa: unit ai scripts\*.tai` (72), override `Data\Unit AI Scripts\` | `EEUnitAction.cpp` ~828 (`"Unit AI Scripts\\" + name + ".tai"`); selected by `EEUnitClassRecord::mAIScriptName` | C-like state machine: `//` comments, `#include("Generic Attack.tai")`, blocks `StateName { Condition true(NextState) false(OtherState) ... }`. In the multiplayer checksum. |
| `.rmv` random map scripts | `Data\Random Map Scripts\*.rmv`, `Common\*.rmv` (94 files) | `EERandomMap.cpp` ~864 (enumerates `*.rmv`), the `Default.dll` generator (`Random Map Generators/Default`, `Unit10.cpp` ~862) | Preprocessor-style DSL: `UseMapGenerator "Default"`, `StringTableID 27000`, `#include "Common\Map Climate"`, `#define (kName, value)`, `#if NOTDEFINED(X)` / `DEFINED(X)`, then keyword/value statements. |
| `Data\Music\New Music.txt` (optional; not shipped) | read by `EEMusicObject::ReadNewMusic` 0x4d9c6b | `fscanf` | Line 1: song file name (`%s`, no spaces). Lines 2-5: floats reverb gain, high-frequency RT ratio, mix, time. Replaces the whole music table with this one song in every epoch. |
| WON lobby `.cfg` | `Data\WONLobby Resources\*.cfg` (~140), `WONLobby.cfg`, `zzWONVersion.cfg` | WONAPI lobby code (`EEWonNet.cpp` etc.) | Resource scripts: `SetResource: Name`, `Key: value` lines, `Type Name { ... }` blocks, `@ID` and `$Color$` references. `WONLobby.cfg` holds `Key: value` lines (Language, ProductName, DirServer...). Out of scope for the db converter. |
| `Data\dxm\...`, `dreXmod.config` | community mod | not first-party | ignore |

---

## 5. Other binary formats (not tables; brief)

The models and textures are described in full, with their open forms (glTF, PNG), in docs/MODEL_FORMATS.md.

| Format | Where | Parsed in | Structure (short) |
|---|---|---|---|
| `.civ` civilization | `data.ssa: civilizations\*.civ` (23 premade), user `Users\default\Civilizations\*.civ` | `EELoadCivilization` / save in `src/Empire Earth/Civilization Builder/EECBCivilizationFiles.cpp` (~288, ~421) | `u32 nameLen` + name bytes (no NUL), `u32 count`, `count` x `u32` upgrade ids. An id is `(category << 6) / index` into `dbcivilization` (`EECivilizationManager::SplitAttribute`). Tiny and flat: easy to put in YAML. `dbpremadecivs.mChecksum` must match a premade file. |
| `.scn` scenario, `.ees` saved game | `Data\Scenarios`, `Data\Saved Games`, campaign `.ssa` | `src/Empire Earth/Persistent/` (`EEPSaveGameHeader.cpp`, `EESaveLoad.cpp`, `EEGameStateLoader.cpp`), `src/Low-Level Engine/File System/FSChunk.cpp`, `FSPersistentList.cpp` | `u32 headerLength` + header (UString name, UString description, u32 major, u32 minor (minor >= 0xc5 when major 0), UString player names, UString date, u32, `EEGameOptions` (versioned), u32 numPlayers, u32 chunkCount + tag ids, media strings, checksum), then FSChunk records (`u32 major, u32 minor, u32 tag, u32 size` + "PK01" payload). Each payload is one registered persistent (checksum, map, scenario manager, players, world, effects, calamities, environment) with its own versioned `Load(major, minor, file)`. This is an object graph, not a table, so it is a poor fit for CSV/YAML. The original writes version 0.250; the port writes 0.251, which adds the computer seats' engine (docs/AI_LUA_PLAN.md S0): a `u32 engine` after each of the 16 `EEGameOptionsPlayer` slots' fields (0 classic, 1 next, 2 random; the game's options hold the resolved engine) and a `u8 engine` after each computer player in the players chunk (0 classic, 1 next). 0.250 files load with every engine classic. |
| `campaigns\<name>.txt` (inside campaign `.ssa`) | campaign archives | `src/Empire Earth/Campaign/EECampaign*.cpp` | Not text despite the extension: a list of `UString`s (u32 len incl. NUL + chars) naming the scenarios in order. |
| `.cem` model | `data.ssa: models\*.cem`, `Data\Models\` | `src/Low-Level Engine/Graphics Engine/GEModel Load.cpp` (writer `GEModel Save.cpp`) | Tag "SSMF", version 2.x (1.0-1.3 also accepted), then chunked model data: sub-models (named, e.g. "Scene Root", "turret_*", orbitals, wheels), frames, vertices, materials, tag points. In the multiplayer checksum. |
| `.sst` texture | `data.ssa: textures\*.sst`, `Data\Textures\` | `GETexture.cpp` (`GETextureFileHeader`, packed 14 bytes) | `u8 isCompressed, u8 mipLevelCount, u32 frameCount, u32 width, u32 height`, then the frames. `.tga` files have no such header (`GESurface.cpp` reads the 18-byte `GETGAHeader`; type 2 = uncompressed true colour, descriptor bits 4/5 = mirrored/top-down). |
| `.wav` / `.mp3` | sounds, music, campaign voice | sound engine (this port: miniaudio) | standard formats |
| `.sgt` | DirectMusic segments named by `dbmusic.mFileName + ".sgt"` | `EEMusicObject` (this port: dmusic replacement) | DirectMusic RIFF |
| `.bik` / `.mpg` movies, `.jpg` scenario images, `.ttf` | `Data\Movies`, scenarios | Bink / DX7 movie / image loaders | standard formats |
| `Language.dll` | install root | `UResourceFile` (`gResource->GetString`) | Win32 STRINGTABLE; the source is `src/Empire Earth/Language/Language.rc`. Every `*NameID` in the db points here. |

---

## 6. Recommendation for the text conversion

This section is the plan as written before the converter existed (it still describes an exact mode). What was built,
and the owner's decision for clean text only (2026-11-04), is section 8.

### 6.1 CSV or YAML per table

**CSV (flat fixed records, one row per record, header row = field names):**

* Small flat tables: dbaiunittargeting, dbambientsounds, dbanimals, dbareaeffect, dbbuttons, dbcalamity, dbcivpowers,
  dbcliffterrain, dbcolortable, dbcpbehavior, dbgamevariant, dbgfxeffects, dbmusic, dbpremadecivs,
  dbstartingresources, dbterraintype, dbunitbehavior, dbupgrade, dbweapontohit, dbworld.
* UI tables: dbuiback, dbuicontrolevents, dbuiformevents, dbuiforms, dbuifonts, dbuihotkey, dbuicontrols.
  dbuicontrols is wide (~110 columns) but flat. Keep it CSV, or YAML grouped by form.
* Matrix tables: dbfamily (60 x 68 damage percent) and dbweapontohit (6 x 6). These are best as a matrix CSV: one row
  per family/weapon, one column per attack kind.
* dbsounds and dbterrain: flat once the leading string becomes a column.
* dbeffects: flat, but CSV only with the hole rows dropped (keep the `id` = row position).

**YAML (nested, variable-length or very wide):**

* dbobjects: 1960 bytes, about 290 fields, and an embedded list `mAttributes[0..mNumAttributes)`. Use YAML with
  grouped sub-maps (combat, movement, gathering, sounds, effects, terrain passability, flags) and the tech list as a
  sequence.
* dbgraphics: 33 strings (14 texture/model pairs) and 14 animation-state blocks. YAML with a `states:` list fits
  well.
* dbtechtree: 15 epochs, each holding its node records and the per-node "parents" lists. A natural YAML tree
  (`epochs: - name, record, parents, nodes: {id: {...}}`). Write only the 793 real records; regenerate the 3,701
  hole slots from the id range of each epoch.
* dbevents together with dbeffects: an event has a variable list of effect indices. YAML `events: {id: {name,
  effects: [..]}}` and `effects: {index: {kind, ...}}`. Optionally inline the effects under their events (they are
  shared, so keep them referenced).
* dbcivilization: each category holds 60 (value, bonus) level pairs. Use YAML, or CSV in long format.
* dbunitset: 8 + 8 id arrays plus links. Either YAML or CSV with list columns.

### 6.2 What a lossless round-trip converter needs

1. **One spec per table** generated from the source structs: offset, type, count, name, class. The survey
   `structs.py` already parses all 36 structs and reproduces every static_assert size. Keep the field order exactly
   as on disk. Emit fields in offset order and never rely on the text order when writing back.
2. **A container model per file:** count prefix, `u32 len + chars (no NUL)` strings before the record
   (dbsounds/dbterrain: 1; dbgraphics: 33; graytextures: 1 before), the dbtechtree epoch blocks with their trailing
   u32 lists, the fixed arrays of dbevents and dbeffects, the dbrandommap double header.
3. **Exact numbers:**
   * Integers in decimal (or hex for flag-like values), with signedness as in the spec (`0xFFFFFFFF` is -1 in s32
     fields).
   * Floats written so they parse back to the same 32-bit pattern: the shortest repr that round-trips as float32,
     e.g. `0.15` is stored as 0x3E19999A. Never round through double formatting with fixed digits.
   * Keep the non-finite and odd values that exist: dbuiback has `inf` tile percents (39 rows). Keep `-0.0` if it
     occurs. Either write `inf`/`-inf`/`nan:0x7fc00000`, or fall back to hex (`0x7f800000`) for anything that does
     not round-trip.
   * Bools: 0/1, but keep the raw byte when it is not 0/1. dbtechtree `mBB6/mBB7` hold `0xCC`.
4. **Unknown and junk bytes:** a "remainder" for each record:
   * Write every unknown or provisional field explicitly (by offset name, e.g. `u_0x3b4`).
   * Strings: store the text up to the NUL, plus a `tail` (hex of the bytes after the NUL) whenever the tail is not
     all zero.
   * Padding: store it only when it is not zero.
   * Hole rows: store a single per-table `hole_template` (hex) and mark holes as `~`, or leave them out with the row
     index implied by the id.

   Then verify with "parse -> text -> binary == original bytes" over all 39 files. The survey scripts already
   parse every file exactly to its end, which is the base for that test.
5. **Two output modes:**
   * *exact*: keeps tails, holes and padding. It reproduces the retail files byte for byte, so the multiplayer
     checksum stays the same.
   * *clean*: zero-fills junk and regenerates holes from a canonical template. The output is functionally identical
     and the game does not read the dropped bytes. Only the checksum (2.1) differs.
6. **References as symbols (optional, on top of the indices):** resolve `mButton -> dbbuttons[i].name`,
   `mGraphic -> dbgraphics[i].name` and so on. The text can then show names, and the writer resolves them back to
   positions. Because the game indexes by row position, keep the numeric index authoritative and the name as a
   comment or a checked alias. Rows that share a name (e.g. "Grass Tufts" twice in dbrandommap, many "x ..." units)
   make names unusable as keys.
7. **Encoding:** the strings are ASCII/CP-1252. Use UTF-8 in YAML/CSV and convert back to CP-1252. Fail on
   characters that cannot be encoded, and on names longer than 99 bytes (or the field size minus 1).
8. **Write-back target:** loose `Data\db\*.dat` (directories win over the archive). Optionally repack `data.ssa`:
   the loaders accept stored entries, so no implode is needed.

### 6.3 Risks

* **Undecoded fields.** In dbobjects, 16% of each record (`mU*`/`mC*`, 322 bytes) is never read by the class ctor, and
  another 8% (`mL*`/`mB*`/`mF*`) is read but has no meaning yet. Many are copied into
  class members whose use is not yet traced, e.g. the terrain-passability bools `mB2DC..mB2EB` (fed into
  `mTerrainTypes[]`) and the 20+ graphic-effect slots `mL24C..mL2BC` (named in `EEUnitClass` as `mDeathEffect`,
  `mMuzzleFlashEffect`, ...). The same applies to dbgfxeffects (`mParam1..12`, `mL90..mL98`), dbcalamity (`mIntParam*`,
  `mFloatParam*`), dbanimals, dbgraphics (`mL70`, `mL74`, `mL28C..mL2A4`, `mKind`) and dbupgrade (`mLDC`, `mLE4`,
  `mLE8`). Text names for these are guesses. Keep the offset in the name so a later rename does not break the files.
* **Implicit array lengths and counts:**
  * dbeffects is a fixed 2000-entry array with no count. The game reads `sizeof(gEventEffects)`; a shorter file
    leaves the rest zero and a longer one is ignored.
  * dbevents is a fixed 1500 rows (`EEEvent::sEvents[1500]`).
  * dbfamily may have at most 60 rows (`theCount <= 60`). dbweapontohit and dbaiunittargeting must have exactly 6 and
    3 rows, or the loader silently leaves its table empty.
  * dbcpbehavior is indexed into `gCPBehaviors[129]` by its own index (an index of 129 or more overflows).
  * `gUnitClassTable` has 0x367 slots (unit types 0..0x366). The tech tree's epoch node arrays are dense id ranges.
  * dbobjects `mAttributes[200]` has `mNumAttributes` valid entries. The rest is zero, or junk in holes.
  * dbgraphics needs exactly 14 animation states and 14 texture/model string pairs.
* **Count ↔ position coupling:** every cross-table reference is a row position (2.4). Inserting a row in the middle
  shifts every later reference in every table. Append-only editing is the safe default; renumbering needs a tool.
* **Loader quirks the text format must keep:**
  * dbmusic row 0 is a hole (id -1) and still gets loaded.
  * dbcpbehavior row 48 is squared at load time, so store the file value, not the effective one.
  * dbtechtree `mFactor1..4` scale the per-unit-class costs of "parents" 2..5.
  * dbrandommap is mis-framed by the loader (dead data).
  * dbanimals multiplies its intervals by 1000 (s -> ms) and dbsounds divides the volume by 100. Store raw file values
    and document the units; do not store the converted ones.
* **Junk bytes vs checksum:** a "clean" conversion changes the bytes and therefore `gBinaryChecksum`. Players with
  different db files get a different version suffix.
* **Base game vs AoC:** the record sizes differ (dbobjects 1948 vs 1960) and so do the counts. Never apply the AoC
  spec to base-game files.
* **Floating-point edge cases:** `inf` values in dbuiback. The ranges include tiny values (`0.0005`, `0.0001`) and
  large ones (`999999`, `1e6`). Round-trip by bit pattern.

---

## 7. Open questions

1. **dbobjects provisional block.** Answered (section 8.4): every read member is named from its readers; the rest
   (+0x124, +0x190, +0x1a4, +0x1c4, +0x1d0, +0x1ec, +0x2c0..+0x2d8, +0x300..+0x310, +0x31c/+0x320, +0x364, the
   200-byte block +0x3b4..+0x47b and the dead copies such as +0x06c -> `EEUnitClass::mL208`) is unread.
2. **Animation state slots** of dbgraphics: the 14 indices have no enum. Their order in the record (0, 13, 7, 8, 4, 5,
   12, 11, 9, 10, 6, 3, 1, 2) suggests database column order. The names from model files are a guess.
3. **Attack-kind columns of dbfamily (68).** Which unit field chooses the column (`mWeapon`, `mProjectileType` or
   `mAmmoType`)? What is each column's name? The source DB had them as columns, but the export keeps only numbers.
4. **dbworld index names.** Answered (section 8.3): the 61 rows with a reader are named from it
   (`tools/eedata/overlay.py` WORLD_ROWS); rows 0..20, 23, 24, 28..33, 38..41 and 73 have no reader (source and
   retail binary checked, the only computed index is improvement + 57), and +0x08, +0x0c, +0x18 of every row are
   unread.
5. **Effect kinds** in dbeffects: kinds 7, 8, 10, 11, 22 are not decoded. `mL00`, `mID`, `mL40`, `mB45`, `mU46`,
   `mL48..mL54` are unread (now `mUnused1..8`, `mUnusedID`).
6. **dbgfxeffects class parameters**: kind-dependent by design; the per-kind meanings are in section 8.4. Only
   `mEmitPositionSpread`, `mIsEmitFlat` and `mEndsWithUnitOrDuration` have one reader and so one name.
7. **dbcalamity parameter slots**: kind-dependent; per-calamity meanings in section 8.4 (`mFloatParam3/4` are the
   earthquake's alone: `mQuakeMinimumStrength`, `mQuakeStrength`; `mBCC..`, `mBE8` are unread).
8. **dbrandommap**: the meaning of the last six columns and of the header dword `10`. Is the table dead in AoC because
   of the framing bug, or does `Default.dll` have another reader? (No reader was found in `src/`.)
9. **dbcolortable +0x90..+0xa7** (24 zero bytes): a missing colour 3 and a 5th colour? dbambientsounds +0x84..+0x8f
   and +0x94. dbunitbehavior +0x73.. These are zero everywhere, so they are harmless as defaults.
10. **dbtechtree `mU84`, `mU8C`, `mBB6/mBB7`, `mUD4..mUE0`, `mUF8`.** Unread (now `mUnused1..9`). In real rows
    they are 0 except for two rows with 100000 and two with 2163200/4100: Access columns that were never used.
11. **Byte-exactness of `0xCC` and junk.** Decided (owner, 2026-11-04): clean only. Compatibility with retail data
    and the multiplayer checksum is no longer a goal; exact mode is gone (section 8).
12. **Base-game format.** The base game's `dbobjects.dat` uses 1948-byte records. If base-game data is ever wanted,
    its layout has to be derived separately (the base exe is out of scope).

---

## 8. The converter (`tools/eedata`) and the text loader

Built from section 6 (2026-11-04); clean text only since the owner's decision of the same day (compatibility with
retail data and the multiplayer checksum is no longer a goal). `python -I tools/eedata/eedata.py -h` lists the
commands; the scratch scripts in `tools/datafmt/` are superseded.

### 8.1 What the text carries

The text carries what the game reads, and nothing else:

* **Fields:** every record member the game reads, under its source name (`mHitPoints`), in record order. Members
  no code reads (`mUnused*`, the unread gaps) are not in the text; the importer writes zero, or the spec's default
  (`overlay.DEFAULTS`: dbobjects' eight unread effect slots and `mUnusedFireTrailEffect` are -1, because the unit
  class still loads the media of the effect row they name). Padding is zero.
* **Labels:** where the game never reads a row's name, the name is kept as `label` (text only: it is written back to
  the same unread bytes, so the round trip holds, and has no effect on the game). dbworld's labels are the
  constants' names (8.3) and are checked: a label must match its row.
* **Rows:** `row` is the row position, the row's runtime integer (it must increase; a record without one takes the
  next free position above every listed row and the `rows` count: new rows go above). Every row has an `id` (8.6);
  references between tables are ids. Empty database rows (holes) are not listed. The importer writes the table's hole record at every
  unlisted position below `rows`: the struct's base record plus `overlay.HOLES`, the values the retail empty rows
  hold in the fields the game reads (-1 ids, 1.0 opacities, ...), so a hole loads exactly as before and every
  position and reference stays valid. A row the game sees exactly as the hole record is written as a hole.
* **Lead strings** (dbsounds `mSampleName`, dbterrain `mTextureName`, dbgraphics' 33 file names) and **lists**
  (dbevents `effects`, dbtechtree `parents`) as before.
* **Gone:** exact mode, string tails, `~raw` fields, padding and `u_0x` fields, the hole template, the `#mode` and
  `#hole` directives, the junk bytes (text after a string's NUL, the converter's 0xCC fill). The files therefore
  differ from retail; every value the game reads does not (8.2).
* **Tables without a text form:** dbrandommap (its loader throws every record away, 3.38) and dbterraingraytextures
  (no AoC code opens it, 3.39). The game reads them from data.ssa as before. 37 tables have a text form.

Format (`#format 3`; 8.6 adds the ids and references to format 2): one file per db file, named like it with `.csv` or `.yaml` (YAML: dbobjects, dbgraphics,
dbtechtree, dbevents, dbcivilization). UTF-8, LF. CSV files start with `#eedata <table>`, `#format 3`, `#rows N`;
YAML files have `eedata`, `format`, `rows` at the top and `records:` (dbtechtree: `epochs: - slots: N, epoch: {...},
nodes: [...]`). Arrays are YAML flow sequences (trailing zeros left out) or CSV columns `name[i]` (pairs
`name[i][j]`). A missing key or an empty cell is zero. Values: integers in decimal (0x... accepted as the bit
pattern); floats as the shortest decimal that rounds back to the same bits, `.inf`/`-.inf`/`.nan`, other NaNs as
`0x<8 hex>`; bools 0/1 (true/false accepted); strings as CP-1252 text (the five undefined bytes as the C1 code points
of the same value). YAML subset: block mappings and sequences, one-line flow sequences, plain/single/double-quoted
scalars, comments; no anchors, aliases, tags, block scalars, flow mappings or multi-line scalars; duplicate keys and
tabs in indentation are errors (`tools/eedata/textio.py`).

### 8.2 Gates (AoC retail data)

* `eedata.py cppgate` also runs gate G1, the content dump (8.6): the retail .dat files and the text tables compiled by
  the game's loader (ids resolved) dump to the same canonical JSON, 17.4 MB, byte-identical.

* `eedata.py check`: (a) 37/37 tables: retail -> text -> db -> text gives the same text, and the db file is a fixed
  point; (b) 37/37 tables: the compiled db file holds the retail value of every field the game reads in every row,
  and the same lead strings and lists; hole rows compile to the hole record, whose game-read fields equal the retail
  ones except 4,303 fields that are the converter's 0xCC fill in rows the loaders skip (dbobjects: `mIsEnabled == 0
  || mUnitType == -1` before the class ctor; dbtechtree `mFirstNodeID` is read from epoch records only; dbuiforms:
  `mFormID > 0`; dbterrain: `EETerrainTable::Get` refuses `mID < 0`);
  dbworld: the values each constant's readers use (dbworld's ids are the constants' names, 8.3, as snake_case:
  `unit_limit_range`; an id must match its row); (c) 35/35 struct sizes match their sources, specs.json and the
  C++ `EEDataTextSpecs.h` are current.
* `eedata.py cppgate`: the game's C++ loader compiles the text of all 37 tables to the Python importer's bytes; 117
  edited texts (valid and broken) give the same bytes or an error in both implementations.
* `eedata.py census` (about 6 minutes): renames every record member, compiles every unit syntax-only and collects
  each use site; fails when a field the text carries has no reader.
* `specgen.py` refuses an offset-only or placeholder name (`mL70`, `mU124`, `mUnknown*`, `mBoolPlaceholder*`) on a
  field the text carries.
* Field classes over the 36 layouts: named (read) 848 fields (6,201 bytes), label 25 (2,504), unused 172 (900), pad 69
  (175). Before (exact mode): named 799, inferred 43, provisional 282, unknown 41, pad 52. Of the old fields 779 named
  kept their class, 69 provisional became named (50 renamed from their readers, 19 kept as kind-dependent parameter
  names), 25 became the label, 241 became unused (172 provisional, 37 unknown, 20 inferred, 12 named) and 41
  explicit padding bytes of dbobjects became compiler padding; the 10 fields of the two dropped tables are gone.
  Text size: 10.4 MB for 7.8 MB of binary (dbobjects 5.2 MB, dbgraphics 3.6 MB of YAML).

### 8.3 dbworld constants

The 95 rows are read only as `gWorldTable.Get*Value(k)` / `gWorldTable.mEntries[k]` (source and retail binary
checked; the only computed index is improvement kind + 57). 61 rows have a reader and are named
(`tools/eedata/overlay.py` WORLD_ROWS, with the reader of each); the text gives each only the values its readers use
(F `mFloatValue`, F2 `mFloatValue2`, L `mLongValue`, L2 `mLongValue2`). Rows 0..20, 23, 24, 28..33, 38..41 and 73 have
no reader: not in the text, zero. `+0x08` (database id), `+0x0c` (index) and `+0x18` are unread in every row.

The code names a constant by its id: `include/ee/EEWorldConstants.h` (generated by `eedata.py cppgen` from the
ids, checked by `eedata.py check` (c)) has `enum EEWorldConstant { kWorldUnitLimitRange = 26, ... }`, and the four
accessors (`gWorldTable.GetFloatValue/GetFloatValue2/GetLongValue/GetLongValue2`) take an `EEWorldConstant`, so a
raw row number does not compile; the table's entries are private. The one computed index is
`EEWorldImprovementTime(kind)` (improvement_time_area_effect + kind). All 120 reads go through the ids.

| Row | Name | Values: meaning (reader) |
|---|---|---|
| 21 | conversionsStatisticMax | L: the "max" of the statistics panel's conversions n/max (EEUIFMGGStatistics::Refresh) |
| 22 | gatherersPerResourceMax | L -> EEPlayer::mMiningLimit, gatherers on one resource (EEPlayer::Reset) |
| 25 | chatLogDisplayTime | L: chat log entry lifetime, ms; x2 notices, x3 console lines (EEUserInterface, EEClient) |
| 26 | unitLimitRange | L / L2: smallest / largest unit limit choice, step 50 (EEGameOptions, lobby) |
| 27 | unitLimitDefault | L: the default unit limit (EEGameOptions::SetDefaults) |
| 34 | wonderVictoryTime | L: wonder victory countdown, s; the AI's victory percentage (EEVCWonder, EECPIntelligence) |
| 35 | areaConversionRadius | F: radius of the extra conversions around a converted unit, wonder power 1 (priests) |
| 36 | wonderPopulationBonus | F / F2: population limit fraction added / removed, wonder power 4 (EEWorld) |
| 37 | wonderHealAmount | L: hit points healed per interval by wonder power 6 (EEPlayer::UpdatePlayer) |
| 42 | gameSpeedDefault | F -> gGameStateGameSpeed (EEWorldTable::Load) |
| 43 | gameSpeedRange | F / F2: lowest / highest in-game speed (EEUIEMGChangeGameSpeed) |
| 44 | gameSpeedStep | F: speed change per key press |
| 45 | deleteRefundFactor | F: cost share refunded when an unfinished unit is deleted (EEMKillUnit::Execute) |
| 46 | conversionFaithCost | L: faith a conversion costs (EEUCBasicPriest, EEGetConversionCost) |
| 47 | unitImprovementPointsMax | L: cap on a unit type's improvement points (EEPlayer::CanImprove, EEUICPortrait) |
| 48 | startingCitizensDefault | L -> mStartingCitizens / mMaxStartingCitizens (EEServer, EEPlayer::Reset) |
| 49 | groupSpeedCapFactor | F -> EEActiveGroup::mCapFactor: group averages capped at F x the slowest unit's |
| 50 | wonderHitPointBonus | F: hit points added by wonder power 2 (EEPlayer::AddWonderPower) |
| 51 | wonderHealEpochFactor | F: heal = L37 + (epoch / L52) x F x L37 (EEPlayer::UpdatePlayer) |
| 52 | wonderHealEpochStep | L: epochs per heal step |
| 53, 54 | highGroundHeight, lowGroundHeight | F x tile height: counts as high / low ground (EEUnitClass::GetElevationFactor) |
| 55, 56 | highGroundAttackFactor, lowGroundAttackFactor | F: attack factor from high / low ground |
| 57..72 | improvementTime<kind> | L / L2: research ms of an improvement's first / later levels (EEPlayer::AdvanceImprovement: kind + 57; kinds AreaEffect, Attack, ArrowArmor, GunArmor, LaserArmor, MissileArmor, PierceArmor, ShockArmor, FlightTimeOrPower, HitPoints, Leadership, Range, Repair, Kind13, Speed, Transport) |
| 74 | wonderSightRadius | F: sight radius around a wonder with bonus 3 (EEComplexUnit::SightOn/SightOff) |
| 75 | refusalMessageInterval | L: ms between repeated "cannot" messages (EEUIBUpgrade, EEUIFMGStatusButton) |
| 76 | civilizationBuilderPoints | L: the civilization builder's point budget (EEGameState::Create, editors) |
| 77 | wondersForVictoryMax | L: the wonders-for-victory choices 1..n (EEGameOptions, lobby) |
| 78 | tileStrayDistance | F: LocationWayOffMap fires when a unit is farther than F from its tile centre (EEUnitAI); MoveToTileCentre's offset (EEUnitClass) |
| 79 | trampleMinDistanceSquared | F -> gPhysicsModelsF: a moving unit pushing a stopped enemy tramples it only beyond this squared distance (EEPhysicsModel) |
| 80 | unitEpochCostFactor | F: unit cost growth per epoch past the node's own (EETechTreeNode::GetCost) |
| 81, 82 | gameSpeedSlowNormal, gameSpeedFastFastest | F / F2: the speeds of game speed options 0, 1 / 2, 3 (EEGameState::Create) |
| 83 | acknowledgementDelay | L: ms between acknowledgement sounds (EEPlayer::PlayAcknowledgement) |
| 84 | aiSpecialAttackChance | F: chance an AI flaming-arrow archer attacks a building specially; F2: chance an AI unit goes fanatic (EECPAttackSubGroup) |
| 85 | camouflageTiming | L: idle ms before Camouflage cloaks a unit; L2: ms the cloak lasts; F: detection radius x range (EEPlayer, EEComplexUnit, EEUnitClass) |
| 86 | specialPowersCategoryCost | L: the civ builder's special powers category cost (EECivilizationManager::Load) |
| 87 | reloadTimeRandomRange | F / F2: min / max random factor on each reload time (EEComplexUnit) |
| 88 | fanaticism | L: fanatic duration ms; F: hit point share lost on going fanatic; F2: damage factor while fanatic |
| 89 | difficultyDamageScale | F / F2: the two scaled damage tables, picked by difficulty and side (EETerrainTypeTable::LoadDamageTables) |
| 90 | jitManufacturingCostFactor | F: rally-point unit cost factor with Just In Time Manufacturing (EETechTreeNode::GetCost) |
| 91, 92 | tradeRateStoneIron, tradeRateFoodWood | F / F2: base market rates (EEGetTradePrice) |
| 93 | jitManufacturingCostFactorDeathmatch | F: the same factor with resources option 4 |
| 94 | deathmatchJitManufacturingDelay | L, s: deathmatch Just In Time buttons stay closed until then (EEPhysics::Tick) |

### 8.4 Field names: evidence and what stays uncertain

A compile census renamed every member of every record struct and compiled all 858 units syntax-only: 1,595 use
sites. A member with no use site is unused. A member whose only use is the loader's copy into a class member was
followed one step further with a second census (the class members renamed): when nothing reads that class member
either, the field is a dead copy and unused too. Read members got names from their readers; the class members they
fill were renamed where they had offset names (`mFacesCamera`, `mSlopeSpeedPenalty`, `mIsExpansionUnit`,
`mCanCrossForest`, `mEmitPositionSpread`, `mIsEmitFlat`, `mEndsWithUnitOrDuration`, `UIControlsTable`'s
`mFitRectToText`/`mScaleToControl`/`mLoopMovie`).

New names of read fields (record member: evidence):

* dbobjects: `mCreationSound` (played when a unit is created or a building finished: EEMessage, EEUGBuildRepair),
  `mFacesCamera` (the model turns to the camera yaw: EERenderObject, EECameraControl), the effect slots
  `mDeathEffect`, `mDamageEffect` (below the effect's health fraction: EEUnit), `mMuzzleFlashEffect`, `mTrailEffect`,
  `mRippleEffect`, `mSplashEffect`, `mHealEffect`, `mImpactSparksEffect`, `mImpactBloodEffect`, `mAttachedEffect`
  (attached at creation), `mSpecialAttackEffect` (Capital Ship / Yamato beam), `mShotEffect` (tracers),
  `mPulseCannonEffect`, `mBattleCryEffect` (EEAEExtraDamageFunctor), `mShellHoleEffect`, `mBloodStainEffect`
  (ground decals); the terrain passability flags feeding `mTerrainTypes[]` (`mCanMoveOnLand` type 0, also copied to
  type 5 snow (an original quirk: `mUnusedFlag2`/`3` at +0x2e0/+0x2e1, which look like the intended snow flags, are
  never read), `mCanMoveOnDeepWater`, `mCanMoveOnRock`, `mCanMoveOnMud`, `mCanMoveOnRoughGrass`, `mCanMoveOnPavement`,
  `mCanMoveOnCliffs`, `mCanMoveOnShallowWater`, `mCanMoveOnSand`, `mCanMoveInSpace`), `mSlopeSpeedPenalty` (SpeedMode
  brakes above (1 - slope x value) x max velocity: EEPhysicsModel), `mIsExpansionUnit` (the units Art of Conquest
  added; EECPUnitSelector), `mCanCrossForest` (terrain bit 14, wood passable: EEUnit::IsPassableBy, pathfinding),
  `mRemainsType` (the unit type CreateRemainsUnit places; retail 0 everywhere, so never used by the shipped data).
* dbgfxeffects: `mIntParam1..3` (the record's +0x90..+0x98 land in EEGraphicEffect::mIntParam1..3), `mFlag1/2`,
  `mEmitPositionSpread`, `mIsEmitFlat`, `mEndsWithUnitOrDuration` (the particle emitter is their only reader).
* dbuicontrols: `mFitRectToText` (UITextLabel::SetFitRectToText), `mScaleToControl` (fonts scaled to the control;
  check box bitmaps), `mLoopMovie` (CreateMovie's loop argument).
* dbaiunittargeting: `mPreferredUnitType` (a target of this unit type is taken at once: EEFOFindNewTargetNearOldTarget).
* dbanimals: `mMinRestInterval`, `mMaxRestInterval` (the herd's random rest after an attack: EEHerd::UpdateAttacking).
* dbcalamity: `mQuakeMinimumStrength`, `mQuakeStrength` (the earthquake is the only reader of +0x6c/+0x70).

Uncertain or kind-dependent (the name says what the code does with the field):

* `mCanMoveOnTerrainType3`, `mCanMoveOnTerrainType7` (dbobjects +0x2df, +0x2e5): passability of terrain types 3
  and 7. No AoC dbterrain record has those types; only the event terrain-mask effect (EEEvent.cpp) can set them.
* `improvementTimeKind13` (dbworld row 70): read through improvement kind + 57, but the improvement enum has no
  kind 13.
* dbgfxeffects `mIntParam1..3`, `mParam1..11`, `mFlag1/2`: one slot, a meaning per effect class (EEGraphicEffect copies
  them 1:1; each EEE* class reads the slot its own way). The particle classes dominate: `mParam1` opacity, `mParam2`
  size (also the damage effects' health threshold), `mParam3` size spread, `mParam4` vertical drift, `mParam5..7`
  colour red/green/blue, `mParam8` gravity, `mParam9` wind factor, `mParam10` bounce elasticity, `mParam11` drag;
  `mIntParam1` spawn interval (emitters), `mIntParam2` its spread, `mIntParam3` particles per burst; `mFlag1`
  constant opacity / attached to the tag point / loops, `mFlag2` bounces off terrain / ends on unit death. Other
  classes reuse the slots (tracer beam fade times, rain fall speed and density, icon offsets, cloak colours).
* dbcalamity `mFloatParam1/2`, `mIntParam1..6`: per calamity class (each class's Initialize). `mFloatParam1`: spread
  or reach radius (fire, plague, hurricane/AMS), scatter size (meteor storm), the earthquake's minimum strength
  before the cracks, the sight bonus (line of sight), the hit point factor after a teleport. `mFloatParam2`: spread
  chance (fire, plague), the earthquake's approach strength. `mIntParam1`: radius in tiles (area effects, mind
  control, leprosy), the placement search range (teleport, time warp), damage per tick (hurricane, plague), the
  earthquake's falloff distance. `mIntParam2`: damage per tick (area effect, leprosy, earthquake), burn % (fire),
  tick time (hurricane), damage interval (plague), minimum launch interval (meteor storm). `mIntParam3`: update
  interval (area effect), spread interval (leprosy), maximum launch interval (meteor storm), camera shake (earthquake),
  damage per update (fire). `mIntParam4`: shakes per second (earthquake), spread interval (fire), minimum eruption
  radius (volcano). `mIntParam5`: damage tick (earthquake), maximum eruption radius (volcano). `mIntParam6`: crack
  count (earthquake), the half size an area needs on the map (hurricane/AMS).
* dbworld 78 `tileStrayDistance`: the second reader (MoveToTileCentre's offset 1 - F) is not understood; 89
  `difficultyDamageScale`: which side gets which table depends on what EEPlayer::IsHuman returns.

Unused, with the evidence (all named `mUnused*`; the guessed meaning is in the name where the data shows one):

* Dead copies (the loader copies the field into a class member nothing reads; second census): dbobjects
  `mUnusedUnitGroup` (+0x06c -> EEUnitClass::mL208), `mUnusedShadowOffset` (+0x1fc -> mL170 -> render lists nothing
  reads), `mUnusedGroundFlag` (+0x209 -> mB305), `mUnusedReference`, `mUnusedValue` (+0x21c/+0x220 -> mL284/mL180),
  `mUnusedFlag6` (+0x319 -> mB2FA), `mUnusedSelectionValue` (+0x32c -> mL294 -> EEUIFMGMainGroup::mL20, only
  written), `mUnusedDefaultedValue` (+0x368 -> mF19C), and nine effect slots whose effect is only loaded and unloaded,
  never played: `mUnusedFireTrailEffect`, `mUnusedEffect1..8` (default -1, see 8.1); dbgraphics +0x70, +0x74,
  +0x28c, +0x290, +0x298, +0x29c, +0x2a4 (-> EEGraphicObject mL318, mL31C, mL4B4, mB4CE, mL4B0, mL2FC, mL320);
  dbsounds `mUnusedPriority` (-> EESoundObject::mL24); dbareaeffect +0x90..+0xb8 (-> EEAreaEffectTemplate
  mL44..mB68); dbcliffterrain +0x74/+0x78 (-> mL1C/mB20); dbterraintype `mUnusedPaletteIndex` (-> mL18);
  dbuicontrols `mUnusedDbRecordId` (-> mUnknown100); dbaiunittargeting `mUnusedWeight` (-> sTable[i][1], never
  indexed); dbanimals +0x85, +0x88..+0x94 (-> the never-read vector mL04), +0x98, +0x9c, +0xb8, +0xba, +0xd8;
  dbcalamity `mUnusedClassFlag` (-> EECalamityClass::mB24).
* dbevents: the row's id is read into `EEEvent::mID`, a private member nothing reads (`mUnusedID`).
* No use site at all: every other `mUnused*` member, the database ids and row indices of every table, the names
  where `mLabel` is used, dbobjects +0x3b4..+0x47b (`mUnusedConverterScratch`, stale converter heap values), and
  dbgraphics `mUnusedHoldTime0`: the loader hard-codes animation slot 0's hold time (`mHoldTimes[0] = 300000`,
  EEGraphicObject.cpp; the original's loader 0x4d7b37 does the same), so the 50000 that 185 retail rows set is
  never used.

### 8.5 The game reads the text

`EEDbFindFile` (`src/Empire Earth/Database/EEDataTextFile.cpp`) replaces `FSPath::FindFile` at the 38 db lookups
(EEInitializeDatabase, EEUnitClass::CreateUnitClasses, UIManager::LoadDatabase). When `db\<table>.csv|.yaml` is on
the data path, `EEDataText.cpp` compiles it to the binary record stream and serves it as an `FSMemoryFile`; the
loaders are unchanged. Every record starts from the struct's base record (zero plus the defaults of unread fields),
an unlisted row is the table's hole record (both generated into `EEDataTextSpecs.h`). A loose `Data\db\` file wins
over the archive, and a text table over a loose .dat. A text that does not compile stops the game with its message
(file, row, field). `eedata.py install GAMEDIR` writes the text tables into a copy of the game. The multiplayer
checksum adds the loose `Data\Db\*.csv`/`*.yaml` (clean text gives a different checksum than retail; every peer
needs the same tables).

### 8.6 Ids and references (format 3)

Every listed row of every table has an `id`: a stable, unique, lowercase ASCII key (`[a-z][a-z0-9_]*`). The
converter (`tools/eedata/ids.py`) generates it once from the row's name, with `_2`, `_3` ... where two rows would get
the same one; from then on the text owns it, and a row's name or label is display text. Sources: dbobjects `mName`
(`priest`, `citizen`, `female_citizen`, `inf02_spearman`), dbgraphics `rowName`, dbsounds/dbtechtree/most tables the
label (`human_anatomy_researched`, `flaming_arrows`), dbworld the constants' names (`unit_limit_range`; they become
the define names), dbcpbehavior the `kCPBehavior*` enumerators (`initial_attack_delay`), dbeffects the first event
that lists the effect (`human_anatomy_researched_1`); a row with no name `<table>_<key>` (`startingresources_0`);
overrides in `overlay.ID_OVERRIDES` (dbweapontohit `shock`, dbareaeffect `none`).

**Keys.** A row's runtime integer is its key: its row position (`row`), except dbtechtree, whose key is `mID` (the
tech tree finds nodes by id). Vanilla rows keep their retail positions, so every code constant (`kUnitType*`, tech
ids, `kCPBehavior*`, sound and effect rows) keeps working. A table's self field (`mUnitType`, `mID`, `mIndex`, ...;
dbcivpowers: `mID` = row + 0x26) is left out of the text where it equals the key; the importer writes it.

**References** (`overlay.REFS`, 196 fields over 25 tables) are written as the target row's id. An integer stays only
where it names no listed row of the target (the "none" values: -1, 0 where row 0 is a hole, 100000, 0xFFFFFFFF), or
is one of the field's declared none values (dbtechtree `mPrerequisite1..4`: 0 is "pre-ticked", although 0 is also the
first epoch's id). On import every id is resolved to the target's key; an unknown id, an integer that names a row
("write its id"), a missing, malformed or duplicate id are load errors naming the file, line and id
(`dbcivpowers line 5 (id flaming_arrows): mUnitSet: 'no_such_set' is not an id of dbunitset`).

`tables.Content` (Python) and `EEDataText::Content` (C++, the game) are the single place the tables meet: they hold
every table's text, parse each once, assign the keys, index the ids and resolve the references. The game's
`EEDbFindFile` serves every db table through one Content, which reads a referenced table's text through the data path
when it first needs its ids. A later mod merge step feeds the Content merged entries instead of files.

**Gates.** (a) and (b) of `eedata.py check` hold with ids; the compiled .dat files are byte-identical to format 2's
(checked against the previous importer); `cppgate`: 37/37 tables compile in C++ to the Python bytes, 137 parity cases
(ids, dangling references, row numbers where an id belongs, duplicates, new rows) agree, and G1: the content dump
(`EEDataDump.cpp`: every game-read field of every row, floats as bits, rows the loaders skip as skipped) of the retail
.dat files equals the dump of the text tables. The game writes the same dump of every table it loads, text or
binary, with `EE_DUMP_CONTENT=<file>` (test use).

Saves keep integer unit and tech ids: runtime keys are deterministic (vanilla keeps the retail numbers, new rows go
above in file order), so a save made with the same tables loads; a save made with other tables is not protected.

The references, with the evidence (loader -> consumer; `EE` = src/Empire Earth):

| Table.field | Target (key) | None | Evidence |
|---|---|---|---|
| dbobjects mProjectileType, mScaffoldingType, mDeathType, mDeathSpawnType, mPackedType, mRemainsType | dbobjects | 0, -1 (row 0 is a hole) | EE/Units/EEUnitClass.cpp CreateUnitClasses link pass |
| dbobjects mFamily | dbfamily | - | EETerrainTypeTable.cpp damage table row; EEUnitSet.cpp |
| dbobjects mWeapon | dbweapontohit (also the armour slot) | -1 | EEPhysicsModel.cpp, EEUnitClass.cpp ApplyHit |
| dbobjects mButton, dbtechtree mButton, dbeffects mButton | dbbuttons | -1 | EEUnitClass.cpp, EETechTreeNode.cpp, EEEvent.cpp kind 1 |
| dbobjects mGraphic, dbeffects mMediaIndex, dbgfxeffects mMediaIndex, dbareaeffect mMedia, dbanimals mMedia, mSound1..3 | dbgraphics | -1, 0 | EEUnitClass.cpp; EEEvent.cpp kinds 6, 12-14; EEGraphicEffect.cpp; EEAnimal.cpp (EEMedia::Get) |
| dbobjects the six sounds, dbeffects mSoundID, dbgraphics mSound1/2IndexN, dbambientsounds, dbcalamity mSoundID, mSecondarySoundID | dbsounds | -1 | EEUnitClass.cpp, EEEvent.cpp, EEGraphicObject.cpp, EEAmbientSound.cpp, EECC*.cpp |
| dbobjects the 20 *Effect slots, dbgfxeffects mLinkedEffectIndex1..3, dbareaeffect mGraphicEffect, mGraphicEffect18, dbcalamity mGraphicEffectID1..4 | dbgfxeffects | -1 | EEUnitClass.cpp, EEGraphicEffect.cpp, EEAreaEffectTemplate.cpp, EECCAreaEffect.cpp |
| dbobjects mUpgradeTableIndex | dbupgrade | 0 (a hole) | EEUnitClass.cpp |
| dbobjects mBehavior | dbunitbehavior | - | EEUnitClass.cpp |
| dbobjects mAreaEffectTemplate0..3, dbeffects mAreaEffect | dbareaeffect | - | EEComplexUnit.cpp, EEAEManager.cpp, EEEvent.cpp kind 2 attr 40 |
| dbobjects mTechnology, mGarrisonTechnology, mAttributes[], dbeffects mTechnology, dbcivpowers mTechnology | dbtechtree (mID) | 100000, -1 | EEUnitClass.cpp, EEUGLoadGarrison.cpp, EEEvent.cpp kinds 1, 8-10, EECivPowersManager.cpp |
| dbobjects mOverlapUnitSet, mWonderUnitSet, dbeffects mUnitSet, dbareaeffect, dbcalamity, dbcivpowers, dbcivilization mUnitSet, dbgamevariant mSet0..9, mCostSet, dbunitset mIncludedSet, mExcludedSet | dbunitset | 0, -1 (row 0 is a hole) | EEGhostUnit.cpp, EEPlayer.cpp, EEEvent.cpp, EEUnitSetManager.cpp, EEGameVariant.cpp, EETechTreeNode.cpp |
| dbobjects mMiniMapColor | dbcolortable | - | EEMiniMap.cpp |
| dbtechtree mResearchedEvent, mObsoleteEvent | dbevents | 0, -1 | EETechTree.cpp, EETechTreeEpoch.cpp |
| dbtechtree mPrerequisite1..4 | dbtechtree (mID; compared with the firing node's id) | 0 (declared) | EETechTreeNode.cpp |
| dbtechtree mUnitClass, mDefaultUnitClass, parents; dbeffects mUnitClass, mNewUnitClass; dbanimals mUnitType; dbaiunittargeting mPreferredUnitType; dbcalamity mUnitClassIndex; dbcivpowers mUnitType; dbunitset mUnitTypes[]; dbterrain mFirst[], mLast[] | dbobjects | 0, -1 | EETechTreeNode.cpp, EEEvent.cpp, EEAnimal.cpp, EEFOFindNewTargetNearOldTarget.cpp, EECCDropObject.cpp, EEUnitSet.cpp, EETerrainTable.cpp |
| dbtechtree mCalamity | dbcalamity | - | EECPASGUProphet.cpp, EECalamityManager.cpp |
| dbtechtree mHotKey, dbbuttons mHotKey | dbuihotkey | -1 | EEUIFMGMainGroup.cpp |
| dbevents effects | dbeffects | out of range | EEEvent.h GetEffect |
| dbunitset mFamilies[] | dbfamily | 0xFFFFFFFF | EEUnitSet.cpp |
| dbterrain mColorType | dbterraintype | - | EEMap.cpp |
| dbcliffterrain mFirstTerrain, mLastTerrain | dbterrain | - | stored only |
| dbuicontrols mFormID, dbuiback mFormID, dbuiformevents mOwnerID | dbuiforms (key = mFormID = row) | - | UIManager.cpp, UIEvent.cpp |
| dbuicontrols mFontTableIndex | dbuifonts | - | UIControlsTable.cpp |
| dbuicontrolevents mOwnerID | dbuicontrols | - | UIEvent.cpp |

Not references (they stay integers): enums with fixed C++ meanings (unit categories, AI kinds, `mCommand0..8` command
ids, effect kinds and attributes, calamity class kinds, UI event ids, `mTerrainType` movement-terrain bits),
Language.rc string ids, and two implicit spaces without a table: the attack kinds (dbobjects `mAmmoType`, the
dbfamily column) and the epochs (dbobjects `mEpoch`, dbtechtree `mEpoch`/`mObsoleteEpoch`/`mNextEpoch`).

### 8.7 The epoch registry: epochs.yaml (format 3)

The tech tree's text is two files that compile together into `db\dbtechtree.dat` (`tools/eedata/techtree.py`, and
the game's `EEDataText::Content`):

* **`Data\db\epochs.yaml`**: every epoch, in order; the number of entries is the number of epochs (the game reads
  as many epoch blocks as the file has: there is no 15 in the code any more). An entry: `id` (`prehistoric_age`,
  `stone_age`, `copper_age`, `bronze_age`, `dark_age`, `middle_ages`, `renaissance`, `imperial_age`,
  `industrial_age`, `atomic_age_ww1`, `atomic_age_ww2`, `atomic_age_modern`, `digital_age`, `nano_age`,
  `space_age`), `label`, `slots` (its block's node slots), `music` and `citizen` (the epoch's media, 8.8), then the
  epoch record: `mTitleID` (the title string, the start-epoch option's label: the original data has 0 and
  the code used 30145 + the epoch, the text writes the string), the advance's costs `mCost2..6`, `mBuildTime`,
  `mPrerequisite1..4`, `mResearchedEvent`, `mButton`, `mHotKey`, `mNameID`, `mRequiredSOL` (techs needed),
  `mDefaultUnitClass`, `parents` ... `mID`, `mEpoch` (= position), `mNextEpoch` (+ 1) and `mFirstNodeID` ((position
  + 1) x 1000) follow from the position and are left out.
* **`Data\db\dbtechtree.yaml`**: the techs, each with `epoch: <epoch id>` (the block it goes in), `row` (its slot
  there), `id` and its fields; `mID` is its key (a new tech without one gets (position + 1) x 1000 + slot).

Runtime ids: an epoch's tech id is its position (0..count - 1, as the original's 0..14); techs keep their mID, and
a tech's epoch is the block it was loaded in (`EETechTreeEpochOf`), no longer `id / 1000 - 1`: an epoch inserted in
the middle moves the later epochs up one place and keeps every tech's id, so the code's tech constants (`kTech*`)
keep working. References to a tech may name an epoch (`mPrerequisite1: bronze_age`); tech and unit epoch fields
(`mEpoch`, `mObsoleteEpoch`) reference epochs, `never` is the epoch count (the original's 15). The code's epoch
constants (`kTechEpochBronze` ...) are the vanilla positions: an inserted epoch shifts what the code's remaining
per-epoch rules (the AI's tables) mean for the epochs after it; the epoch media are data (8.8).

`eedata.py doc epochs TEXTDIR [OUT.md]` writes a readable table of it (the YAML is the source of truth);
`eedata.py extra --epoch` inserts a 16th epoch for the tests.

### 8.8 The rules documents (format 3)

Named settings that were literals in the game code are data: four YAML documents next to the tables
(`Data\db\combat.yaml`, `gameoptions.yaml`, `civbuilder.yaml`, `civpowers.yaml`) and two keys of every
`epochs.yaml` entry (`music`, `citizen`). The game reads them in `src/Empire Earth/Database/EERules.cpp`
(`ee/EERules.h`, `EEGetRules()`), through the same `EEDataText::Content` as the tables, at the start of
`EEInitializeDatabase` (`EELoadRules`); references are ids of the tables' rows, resolved to their keys. Every value
has the original's literal as its default (a key left out keeps it; a document that is not there keeps them all:
the binary db tree plays with the defaults). `tools/eedata/rules.py` writes the vanilla documents (`eedata.py
export`, `install`, `open_data.py build`: `tables.export_tree`) from the same code constants, which it reads from the
C++ headers (`kUnitType*`, `kTech*`, `kSound*`, `kGraphicEffect*`, `IDS_*`), and validates documents (`check`,
`import`, `cppgate`). The game rejects an unknown key, a bad value or a dangling id with the place
(`db\combat line 9: minimum: 'x' is not an integer`).

**Numbers** are constants or one of dbworld's values: `<constant id>.F|F2|L|L2` (`fanaticism.F2`: row 88's
`mFloatValue2`); the value must be one the row's readers use (8.3). A world value is read when it is used
(`ee/EERulesWorld.h`), so it follows the loaded dbworld. **Unit categories** are named (`land`, `land_citizen`,
`projectile` ... `planet_spaceship`: the 35 `kUnitCategory*` values, `kEERuleCategoryNames`).

**combat.yaml** (`EECombatRules`; the code: `EEUnitClass::ComputeDamage`, the one damage function of a direct hit
(ApplyHit), splash damage (ApplyAreaDamage) and the trample (EEPhysicsModel); `GetElevationFactor`;
`EEComplexUnit::ApplyMorale`; `EEAEMoraleFromBuildings`; the regeneration ticks):

    damage:
      minimum: 1                       # the least damage of a direct or splash hit that gets through
      fanatic_factor: fanaticism.F2    # a fanatic attacker's factor (direct hits)
      elevation_step: 1.0              # the elevation factor moves damage by at least this much
      ally_factor: 0.0                 # a direct hit on an own or allied unit: ceil(damage x this) ...
      herd_ally_factor: 0.1            # ... by a herd animal
      dead_food_share: 0.25            # a land food source killed by a non-citizen keeps this share
      trample_damage: 1                # the trample's damage when to-hit x (attack - armour) > trample_threshold
      trample_threshold: 1.0
    elevation:                         # who the high/low ground factor (dbworld high_ground_*) applies to
      attackers: [land, land_citizen]
      launchers: [projectile, bomb, missile, torpedo]   # count as their launcher's category
      targets: [land, building, land_citizen, land_priest, building19, land15, ...]
    morale:
      positive_factor: 0.8
      negative_factor: 0.6
      buildings: [b_house]             # the unit types morale from buildings counts (dbobjects ids)
    ticks:
      regeneration: 1000               # ms: hit point regeneration, garrison heals
      power_regeneration: 5000

Each damage form keeps the original's arithmetic (the order of the float operations, the rounding: direct hits round
the dead-food share with `UMFloatToLong`, splash damage with `UMCeil`; the trample rounds the armour).

**gameoptions.yaml** (`EEGameOptionRules`; `EEGameOptions` operator=, `GetMapDimensions`, `SetDefaults`, the lobby's
and the scenario editor's lists, `EEGameState::Create`, `EERandomMap`, `EEToHitTable::GetAttackFactor`,
`EEComputerPlayer`): an option is its entry's position in the list.

    map_sizes:                         # the lobby's list; GetMapDimensions; the random map scripts' size class
      - id: tiny
        name: 29700                    # Language.rc string
        tiles: 42
        script_size: 0                 # MapSize of the .rmv scripts (0 .. 5): the smallest size at least the map's height
      ...                              # small 58, medium 75, large 105, huge 160, gigantic 220
    map_size_default: small
    speeds:                            # factor: game_speed_slow_normal.F, .F2, game_speed_fast_fastest.F, .F2
      - id: slow ...
    speed_default: normal
    difficulties:
      - id: easy
        name: 30269
        ai_level: 0                    # the computer players' records: 0 easy, 1 medium, 2 hard
        damage_scale_human_target: difficulty_damage_scale.F     # the damage table scale when only the target is
        damage_scale_human_attacker: difficulty_damage_scale.F2  # "human" (EEPlayer::IsHuman) / only the attacker
        ai_cheats: []                  # the computer players' cheats kept (none; any of cost, reveal, epochs,
                                       # extra, popband, catchup, retire)
      ...                              # medium: 1.0 and 1.0 (the base table), hard: .F2 and .F
    unit_limit: {minimum: unit_limit_range.L, maximum: unit_limit_range.L2, step: 50, default: unit_limit_default.L}
    wonders_for_victory: {maximum: wonders_for_victory_max.L, default: 3}
    defeat_without_recovery: true      # out with no citizens, no other units and no built town centre or capitol

(`unit_limit` and `wonders_for_victory` are block mappings in the file.) `defeat_without_recovery` (new) defeats a
player who cannot come back: no citizens, no other mobile units, and no finished town centre or capitol with a
citizen's cost in stock, whatever other buildings are left. `false` keeps the original rule (every vital unit and finished vital building gone,
`EEPlayer::mEndOfGameCount`). Scenarios keep the original rule. The counts of the lists replace the
original's limits (map size < 6, speed < 4, difficulty < 3); an out-of-range option falls back to the default entry
(difficulty: the first). The damage table by difficulty is computed at the lookup (the scale x the base table) instead
of two pre-scaled copies: the same float product. `ai_cheats` lists the computer players' cheats a difficulty keeps
(`EEAICheat`, ee/EERules.h; none by default, a fair game; the original kept every one at every difficulty): `cost` (the unit
cost modifier, dbcpbehavior rows 67/96, 1% stock), `reveal` (the map explored at the start), `epochs` (free epoch
advances), `extra` (row 101's gold and iron), `popband` (the pop cap pulled toward the largest human population),
`catchup` (the epoch rubber band: hurried when behind a human, held back and slowed when ahead of every human) and `retire` (unseen obsolete units deleted); see docs/AI_RESEARCH.md. Every difficulty
is fair by default; a cheating one lists its cheats (all seven: the original's). Load types stay the code's (`kLoadTypeCount`, 6: new game,
scenario, saved game ...: each is a code path).

**civbuilder.yaml** (`EECivBuilderRules`; `EECivilizationManager::ApplyUpgradeLevel`, `Load`'s special powers
category, `EECivPowersManager::HasPower`): an upgrade is a category (a dbcivilization row) and a level; the level is
an attribute here, and the category's `mLevels[level - 1]` its value and bonus.

    attributes:
      - id: attack
        level: 1
        applies:
          - stat: attack               # a C++ primitive: attack, hit_points, reload_time, armor, range, line_of_sight,
                                       # speed, flight_time, area_effect, gather_time (slot 0 .. 6), build_time,
                                       # conversion, repair_rate, area_effect_scale, high_ground, population, cost, power
            mode: share                # share (the stat's own rule, the original's: a share of the class's base value),
                                       # add (+ value) or multiply (x (1 + value)); hit_points, armor, power: share only
      - id: gather_stone_gold
        level: 9
        applies:
          - stat: gather_time
            slot: 5
            except_category: naval_citizen   # only_category / except_category: the unit class's category
          ...
      - id: flaming_arrows             # levels 38 .. 60: the civ powers
        level: 38
        applies:
          - stat: power
            power: flaming_arrows      # a dbcivpowers id
    powers_category: {name: 32850, unit_set: civ_general_perplayer_dont_change_ever_cgc,
                      cost: special_powers_category_cost.L, level_value: 0.01}

An attribute that raises the line of sight takes the class's units' sight off and on around the change (the original
did so for level 5); one that applies a power applies only the power. Level 22 has no attribute (nothing, as in the
original). An attribute has a `name` (the builder's text; the original's IDS_CIV_BONUS_BASE / IDS_CIV_POWER_BASE +
level), and `powers_category` an `id` (`special_powers`).

**dbcivilization.yaml's levels** (`overlay.KEYED_LEVELS`): a category writes its level pairs keyed by attribute,

    levels:
      - attribute: attack
        value: 0.19999999
        bonus: 0.04

(`level: n` instead of `attribute` names a level that has no attribute). Both compilers map the attributes to levels
through civbuilder.yaml (the vanilla attributes without it) and fill the record's `mLevels[60]`; levels past 60 are
not in the record: the game reads every category's levels from the text (`EECivBuilderRules::mCategories`, any
number) and the content dump lists them (`rules.civbuilder` `levels_past_60`). Upgrade ids are 32 bits, `(category
<< 16) | level` (`ee/EECivUpgrade.h`; the original's `(category << 6) | level` in 16 bits capped the categories at
35 and the levels at 60).

**Keyed .civ files.** The civilization builder writes

    eedata: civ
    format: 3
    name: "Rome"
    upgrades: [archers_foot.attack, archers_foot.range, special_powers.flaming_arrows]

(`<dbcivilization id>.<civbuilder attribute id>`) when every upgrade has a key (the text tables give the ids; with
the binary tables it writes the legacy form). Legacy binary .civ files (the premade civilizations) still load: their
`(category << 6) | level` ids are converted, and the premade checksums are computed on that legacy form.

**civpowers.yaml** (`EECivPowerRules`; `EECivPowersManager::ApplyPower`): what granting a power does after its
`mTechnology` is researched, as C++ primitives the data selects and parameterises:

    powers:
      - power: flaming_arrows
        effects:
          - do: research_if_researched
            if_researched: simple_bowman_research    # dbtechtree ids
            tech: simple_bowman_flaming_research
      - power: exploration
        effects:
          - do: pulse_sight                # a sight pulse of the power's mAmount around each unit of the type
            unit_type: b_town_center
      ...                                  # load_sound (sound), load_effect (effect), set_mining_limit (the power's
                                           # mMiningLimit), enable_camouflage, allow_tech, disallow_tech (tech)

The powers' other behaviour stays where it is (C++ primitives that test `HasPower`: fanaticism, insurance, ...).

**aipersonalities.yaml** (`EEAIPersonalityRules`; the computer players: `EECPPersonality`, docs/AI_RESEARCH.md
§8.8-8.12): the tuning of a computer player's decisions that is not in dbcpbehavior, by personality, and which
computer player gets which. New in this port (no original values: the defaults are what the benchmarks settled on).

    personalities:
      - id: balanced
        epoch_pressure:            # the elapsed share of the epoch timer (dbcpbehavior rows 28/29); paid epochs only
          start: 0.3               # the advance is asked for from here (1: the original timer) ...
          full: 0.8                # ... and its cost is wholly reserved from here (the reserve grows in between)
          stone_span: 30           # the Prehistoric instead: the game time over this many minutes (the Stone Age at
                                   # about 25 min; 0: the epoch timer; docs/AI_RESEARCH.md §8.27)
          safe_bonus: 0.25         # from the Stone Age, while no stronger enemy army is near the town: this much
                                   # more pressure (the advance asked for sooner, its reserve filled faster); a
                                   # threat drops it back (§8.29)
        economy:                   # EECPResourceManager
          surplus: 1000            # a resource with more in stock gets surplus_cut of its gatherer share
          surplus_cut: 0.5
          low_stock: 150           # one with less gets at least its floor share of the gatherers
          floor: {food, wood, stone, gold, iron shares}   # (block mapping in the file) 0.1, 0.25, 0.05, 0.05, 0.05
          max_citizen_share: 0.7   # the citizen target (the held sites' capacity) is at most this share of the pop cap
          wood_slots: 15           # wood's gatherer capacity
          moves_per_cycle: 3       # busy gatherers moved to a short resource per 10 s
          demand_split: true       # the gatherers by the tiered demand less the stock (docs/AI_RESEARCH.md §8.21);
                                   # false: the original's needs ledger with the stock correction above
          coast_fishing: true      # from the Stone Age, fish within 30 tiles of the town centre: a dock and up to
                                   # 2 boats a fish (8 at most); the build lists' docks capped at 2 (§8.26)
          forage_patches: true     # the food capacity: mining_limit a forage patch (not every food gatherer), the
                                   # workers on farms, fish and game, 1 hunter per animal within 45 tiles (12 at
                                   # most); a settlement on a patch with no drop site within 4 tiles; 3 patches kept
                                   # claimed in the Prehistoric, 5 later; 16 boats at most (§8.27). The claim
                                   # range: patches, mines (those within it first), the claims and granaries within
                                   # 25 tiles of the town centre, 5 more each time a farm, granary or settlement has
                                   # no room (45 at most); a settlement on each claimed patch and worked mine with no
                                   # drop site within 3 tiles; a tower at each worked site without one within 8; no
                                   # houses in the Prehistoric (§8.28)
          claim_needed: true       # the next epoch's stone, gold or iron short with no active centre holding any:
                                   # the nearest centre with it is claimed; an advance that cannot be reached (a
                                   # resource short with no gatherer) reserves nothing (§8.27)
          early_town_centers: 2    # before the Bronze Age a settlement becomes a town centre only below this many
                                   # town centres, and no town centre a capitol; 0: the original's timers (§8.29)
          early_trainers: 2        # before the Copper Age at most this many barracks and archery ranges each
                                   # (dbcpbehavior asks for 7 and 6); 0: no cap (§8.29)
          patch_expand: true       # before the Copper Age (no farms): the claim range grows when no forage patch is
                                   # left in it (§8.29)
        military:
          standing_army: 0.25      # the army kept per worker (every 30 game s, the favourite land unit; up to the
                                   # original's military share of the pop cap)
          army_floor: 0.1          # while the army is short: up to this much more floor share of the gatherers on
                                   # its unit's costs (by the shortfall)
          militia: true            # soldiers hit a citizen: the soldiers within 30 tiles and the nearest citizens
                                   # the odds need fight (strength 1.5 x the attackers'), else the citizens run
                                   # (docs/AI_RESEARCH.md §8.25)
          militia_towers: true     # the built towers within 12 tiles count in those odds, and citizens that run go
                                   # to the nearest tower within 30 tiles (§8.26)
          prehistoric_army: 4      # the Prehistoric army: at most this many soldiers unless enemy soldiers within
                                   # 40 tiles of the town are stronger (then 4 min uncapped); 0: no cap (§8.26)
          prehistoric_attacks: false   # attack groups against an enemy in the Prehistoric (false: none; the
                                   # defence still forms) (§8.27)
          stone_attacks: false     # ... and in the Stone Age (§8.29)
          naval_flee: true         # hit by a warship it cannot answer: boats go to the dock farthest from it,
                                   # citizens and melee units within 12 tiles to the town centre (§8.27)
      - id: tall ...               # epoch_pressure 0.15 / 0.5, standing_army 0.2
      - id: wide ...               # epoch_pressure 0.6 / 1.0, max_citizen_share 0.8, standing_army 0.15
      - id: aggressive ...         # epoch_pressure 0.4 / 0.9, standing_army 0.5, prehistoric_army 0,
                                   # prehistoric_attacks and stone_attacks true, safe_bonus 0, early_trainers 0
      - id: classic ...            # TEMPORARY (until the new AI beats it): every rule at the original's setting
                                   # (epoch_pressure 1.0 / 100, stone_span 0, no stock correction, max_citizen_share
                                   # 0, moves_per_cycle 0, the needs ledger, every new behaviour off, no standing
                                   # army); EE_AI_PERSONALITY gives it to a seat (docs/AI_RESEARCH.md §8.28)
    assign: [balanced]             # computer player n takes entry (n - 1) mod the count

A key left out keeps the default; `assign` names personalities. TEST ONLY: `EE_AI_PERSONALITY` in the environment
overrides the assignment (one id, or a comma list taken in turn).

**epochs.yaml media** (`EEEpochMedia`; `EEMusicObject::LoadTable`, `EEPlayUnitEffect`, `EEUGGather`,
`EEUnitClass`): per epoch

    music: [pawns, shadows, balance, fallen, enmity, time, honor, rivers]   # dbmusic ids: the epoch's songs
    citizen:
      high_tech_tools: 0               # the mining beam and shot effects (the original: the nano age on)
      removal_effects: [lightning_unit, dust_effect, dust_effect, dust_effect]   # a unit a research removes
      removal_sound: lightning
      removal_pitch_offset: 0.0        # added to the sound's pitch, not below -1 (the atomic ages: -0.2)
      removal_thunder: thunder_2
      removal_thunder_every: 2         # a thunder when the unit id is a multiple (0: never)

dbmusic's per-epoch flags (`mInEpoch[14]`) are no longer in dbmusic's text: both compilers set them from the first
14 epochs' music lists (`overlay.EPOCH_FLAGS`, specs `epoch_flags`), so the compiled table and its content dump are
unchanged; the game reads the lists, any number of epochs (the original's space age played the nano age's songs:
the export writes that into space_age's list).

**Gates.** `eedata.py check` validates the vanilla documents and epochs.yaml's media; `cppgate`: the content dump
(G1) gains one entry per document (`"rules.combat":{...}`, `"rules.gameoptions"`, `"rules.civbuilder"`,
`"rules.civpowers"`, `"rules.epochs"`; values resolved, floats as bits), and the dump of the binary tables (the C++
defaults, the songs from dbmusic's flags) equals the dump of the text (the vanilla documents read by the game's
reader). The game writes the same entries with `EE_DUMP_CONTENT` after the database is loaded (`EEDumpRules`).
