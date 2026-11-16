# R6 data (the civilization builder lane, widened to the data side)

The lane's state, as the other lanes' files in this folder. Each round rewrites this file to the current state; the
round's story goes in PORT_LEARNINGS.md ("R6 data, round 1"). The civilization builder's own rounds 1-2 are in
`r6-civilization-builder.md`.

**Round 1** (branch `rust-data1`):
- The language strings: `tools/lang_strings.py` generates `rust/crates/ee_util/src/language_strings.rs` (it wrote
  `src/Empire Earth/Language/LanguageStrings.cpp`); `UResourceFile` reads them through `ee_util_resource_string` /
  `ee_util_resource_strings_language` (ee_util `strings.rs`). `include/lle/UResourceStrings.h` deleted. The ids stay
  `include/ee/EEStringIDs.h`. No data-tree change: the text stays compiled in.
- The `.rmv` reader: `Unit10.cpp`/`Unit10.h` deleted, `ee_rmap` `rmv.rs` whole (preprocessor, keyword tables, value
  parser, `SetDefaults`, `SetRandomMapValues`' rescaling, flat area and forest mapping). `Default.cpp` keeps a host
  (the script files through `FSFileSpec` + `PlatFOpen`, the map's two random streams, the copy of the lists).
- Gates: `tools/rmap_gate/rmap_gate.py check` (2,088 maps) and `check --reader` (the same maps' digest right after
  `SetRandomMapValues`, `golden_reader.txt`, recorded from the C++ reader before its deletion). The gate tool was
  repaired (it had rotted with the LLE moves: U2DFractal, FSFileSpec, TSThread are Rust now).
- Net C++ (src, include, tools; marker lines and include/rust excluded): -5,197 against rust. The language strings
  about -3,790 (a generated table); the reader about -1,400 hand-written (Unit10 1,506 without markers, the host
  +106). Rust +5,589 (the generated table 3,783 of it).
- Open: `eedata check` fails on rust itself (`struct UIDbDataBackgrounds not found`: UI round 20 moved the record to
  Rust and `tools/eedata/overlay.py` still names the C++ header); see PORT_LEARNINGS "R6 data, round 1".

**Measure of the rest** (lines on rust, markers excluded):
- `Database/` 2,600 in 17 units (EETerrainTable 419, EEButtonObject 307, EEAnimal 233, EETerrainTypeTable 216,
  EERules 216, ...): table classes over the Rust text compiler; reached by `eedata check`/`cppgate` (the loads), the
  oracle and the captures (the simulation reads every table).
- `Data Pre-Loading Thread/` 2,094 (EEGraphicObject 1,049, EEGraphicEffect 278, EEMusicObject 272, EESoundObject
  186, EEDataPreLoading 309): render and sound side, reached only by the tour.
- `Random Map/EERandomMap.cpp` 1,370: the game's side of map generation (grids, teams, features placed as units,
  terrain painting); simulation input, reached by the oracle and the random map captures, not by the rmap gate.
- `Empire Earth/EECivilization.cpp` 325; `FillCivilizationList` (EECivilizationManager.cpp).

**Next**: `EERandomMap.cpp` (the rmap gate's `rmap_dump.cpp` already mirrors its passes between the PIM steps: a
Rust port can be gated by the same matrix plus the oracle), then the Database table classes, largest first.
