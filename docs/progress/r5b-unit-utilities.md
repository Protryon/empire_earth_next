# R5b unit utilities (src/Empire Earth/User Interface Thread/EEFOFindNewTargetNearOldTarget.cpp)

The lane's state. Each round rewrites this file to the current state; the rounds' stories go in PORT_LEARNINGS.md
("The unit utilities, round 1", "round 2", "round 3").

The file is the original's "EEUnitUtilities.cpp": some 200 free functions the units, goals, AI, calamities, random
map and screens call, and the `EEFOFindNewTargetNearOldTarget` screen. 6,380 lines before round 1; 4,725 after it;
3,996 after round 2; 3,496 at the start of round 3; 2,945 after it; 2,876 now.

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

- Round 4 (branch `rust-unitutil-r4`):
  - `uu_direct.rs`: the goals' host (rust_goal.rs `OpHost`) and the AI placement's host (cp/place_game.rs) call the
    noted Rust entry points (natives in uu_ffi.rs: `tile_units_standing`, `tiles_around`, `pick_tile_near`,
    `unit_tiles`, `units_on`, `player_units_of_type`, `units_in_radius`, `square_ring`, `square_ring_tiles`,
    `tiles_in_radius`, `block_ring`, `ring_corners`, `collect_units`). 18 goal host cases (EEUGRust.cpp: can stand,
    line blocked, attack part, deposit load, next free tile, resource spot, free tile, resource reach, path length,
    tile standing, blocking/movable units, pick tile near, units in radius, footprint tiles/units, owns lighthouse,
    clear site) and 10 placement cases (EECPPlacement.cpp: units of type, block takes, tile unit of type, units in a
    rectangle with no filter, the six tile lists) are gone, with the forwarders no C++ calls any more
    (`EEFindNextFreeTile`, `EEGetTileUnitStanding`, `EEGetMovableTileUnits`, `EEIsWithinReachOf`,
    `EEGetTileUnitOfType`).
  - The target searches' plain reads: the candidate's position and values, the tests NATURE, WALL, ALIVE and the
    new-target walk's NATURE and multiple-tile test (combat search_ffi.rs, walks_ffi.rs).

**C++**: round 1 net -1,684; round 2 net -741; round 3 net -553; round 4 net -229 (EEUGRust.cpp -94,
EECPPlacement.cpp -61, this file 2,945 -> 2,876, the headers -5).

**Left** (C++, 2,876 lines):
- the forwarders the C++ callers need (about 90, one Rust call each). The ones the Units lane could free (their
  callers are Rust hosts in its files): EEUnitHost.cpp's `EEPlanWall`, `EEBuildWall`, `EECanPlaceOnTile`,
  `EEMakeBuilding`, `EEFindFreeFarm`, `EEGetFarmDropSites`, `EEWorkFarm`, `EEIsSeenBy`, `EEUUGetIsUnitClassTargetable`
  cases, and EEUGRustCombat.cpp's `EEGetAttackPart`, `EEUUGetIsUnitClassTargetable`, `EECanHeal`,
  `EEGetFiringPosition`, `EEGetLeadPosition` cases: their Rust callers can call `units::uu_direct` / `uu_ffi`
  (or ee_combat) directly; `EEGetAttackPart`'s forwarder goes with the last;
- EEEffectHost.cpp (`StepToward`, `WallPiece`, `FarmDropSite`, the confirm goal's stand test) and EEARust.cpp (seen,
  footprint visible, targetable, farm drop sites, can heal): left, their Rust sides (ee_render effects, ee_game
  screens) run over fake hosts in their tests;
- the ee_combat search, new-target and line-of-fire hosts' tests that call C++ (virtuals, goals, ignore lists, the
  model's tag points and boxes, the CB* capture records);
- the hosts that call C++ objects (continents, terrain mesh, path manager, goals, effects, class virtuals, writes);
- the lifetime sequences (`EEReplaceUnit`, `EEMakeUnitCopy`, `EEConvertUnit`, `EEMakeBuilding`, `EEMovePassengers`,
  `EEReplaceQueuedTechnology`, `EEUpgradeHitPoints`), `EEFindNewTargetNearOld` and the screen virtuals,
  `EEUpdateUnitTile`, `EEGetHelicopterPriestDestination`, `EEGetWaitingGatherer` (a Rust goal's gather resource, the
  physics random, `PermuteValues`), `EEFindTargetNear` (the influence table, `EESearchTarget`), `EEWorkFarm` (C++
  goal construction and actions), the sound's camera view, `EEPreviewWall`, the civilization upgrade text.

**Next**: the Units lane's host cases above; `EEIsInWorkReach`/`EEIsInGatherReach` once the CB* capture records can be
written from Rust; the remaining search tests once the goals and class virtuals they read are Rust.
