# R5b unit utilities (src/Empire Earth/User Interface Thread/EEFOFindNewTargetNearOldTarget.cpp)

The lane's state. Each round rewrites this file to the current state; the rounds' stories go in PORT_LEARNINGS.md
("The unit utilities, round 1", "round 2", "round 3").

The file is the original's "EEUnitUtilities.cpp": some 200 free functions the units, goals, AI, calamities, random
map and screens call, and the `EEFOFindNewTargetNearOldTarget` screen. 6,380 lines before round 1; 4,725 after it;
3,996 after round 2; 3,496 at the start of round 3; 2,945 now.

**In Rust** (`ee_sim::units` uu.rs, uu_tiles.rs, uu_place.rs, uu_spots.rs, uu_world.rs, uu_reads.rs over uu_host.rs;
entry points uu_ffi.rs; the record uu_note.rs; msvc_sort.rs):

- Rounds 1 and 2: the tile geometry and searches, the placement and path leaves, the spot searches, the units'
  frames, footprints, walls, the line's walls, the sound, the deposit (see the round sections).
- Round 3 (branch `rust-unitutil-r3`):
  - **UUTL is Rust's** (uu_note.rs): the note, the per-thread depth, the events, the tick fold and EE-UU-COUNT. Every
    noted entry point notes its own call in the C++ notes' word order; the C++ bodies still noted (EEReplaceUnit,
    EEConvertUnit, EEUpdateUnitTile, EEGetHelicopterPriestDestination) use a 20-line wrapper over
    `ee_units_uu_note_begin/word/end`. The forwarders lost their notes (one call each).
  - New Rust entry points for C++ compositions: `EEGetSideTiles`, `EEGetSquareBlockRing`, `EEGetCornerTiles`,
    `EEGetUnitsOnTile`, `EEGetUnitsInRectangle` (screen rectangle), `EEGetHeadingBetween`, `EEBlockTakes` (the unit's
    player read in Rust).
  - The plain reads (uu_reads.rs): `EEIsUnitMovable`, `EEIsSameResearch`, `EEGetResourceIndex`,
    `EEGetDistanceSquared`, `EEIsCloakedEnemyOf`, `EEGetLeadUnitID`, `EEGetAimPoint`, `EECanTargetClass` /
    `EEUUGetIsUnitClassTargetable`, and the collector under `EEGetUnitsNear` / `EEGetUnitsInRadius` (the map's unit
    regions through the new layout field `map_unit_regions`).
  - The host's reads through ee_sim::world's views: the map, tiles (terrain, height, units), the unit and class
    records (`EeUnitsUuUnit`, `EeUnitsUuClass`, with `IsLandBuilding`/`FliesOver` from the class dispatch table),
    parts, player units, enemies, the visibility-saving flag, goal kind, stats range, line of sight, the active
    group's units, the map size, the unit regions. Their C++ host functions and switch cases are gone.

**C++**: round 1 net -1,684; round 2 net -741; round 3 net -553 (the file 3,496 -> 2,945 lines, its header -2,
EEWorldLayout.cpp +1).

**Gate**: UUTL (tools/sim_capture.py), now written by Rust; its golden lines unchanged from round 2 (equal at every
step of round 3 in oracle, late, space and calamity, seeds 2-3). Mutations: a note's two words swapped (EECanStandOn)
fails UUTL only; the radius rectangle's far side changed fails 41 tags of the oracle capture.

**Left** (C++, 2,945 lines):
- the forwarders the C++ callers need (about 95, one Rust call each; most callers are in src/Empire Earth/Units and
  the Rust hosts EEUGRust.cpp, EEUnitHost.cpp, EEEffectHost.cpp, EEARust.cpp, EECPPlacement.cpp: a Rust caller could
  call the Rust body directly and its host case and forwarder would go, now that the note is Rust's);
- the ee_combat search, new-target and line-of-fire hosts (~550 lines: virtual and goal reads, the CB* capture
  records);
- the hosts that call C++ objects (continents, terrain mesh, path manager, goals, effects, class virtuals, writes);
- the lifetime sequences (`EEReplaceUnit`, `EEMakeUnitCopy`, `EEConvertUnit`, `EEMakeBuilding`, `EEMovePassengers`,
  `EEReplaceQueuedTechnology`, `EEUpgradeHitPoints`), `EEFindNewTargetNearOld` and the screen virtuals,
  `EEUpdateUnitTile`, `EEGetHelicopterPriestDestination`, `EEGetWaitingGatherer`, `EEFindTargetNear`, the sound's camera
  view, `EEWorkFarm`, `EEPreviewWall`, the civilization upgrade text.

**Next**: the Rust hosts' callers (goals, actions, effects, the AI's placement) call the Rust bodies directly (their
host cases and ~25 forwarders go); the target searches' tests once the goals and class virtuals they ask are Rust.
