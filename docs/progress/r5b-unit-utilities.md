# R5b unit utilities (src/Empire Earth/User Interface Thread/EEFOFindNewTargetNearOldTarget.cpp)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The unit utilities, round 1").

The file is the original's "EEUnitUtilities.cpp" (its asserts name `EEUUGetIsUnitClassTargetable`; the unit map named
it after the one class it defines on the stack): some 200 free functions the units, goals, AI, calamities, random
map and screens call, and the `EEFOFindNewTargetNearOldTarget` screen. Before round 1 the target searches, the line
of fire, the attack reach and range tests, the tile line and the ballistic and lead arithmetic were already
`ee_combat`'s (R5b.2); 6,380 lines of C++.

**Round 1** (branch `rust-unitutil1`, `ee_units` uu.rs, uu_tiles.rs, uu_place.rs, uu_spots.rs, uu_host.rs,
uu_ffi.rs, msvc_sort.rs; 2026-11-14):

- **The tile geometry, pure** (`uu_tiles.rs`): the step pattern inside a tile (`EEGetSubTileSteps`, 338 lines of
  switch), the square rings (`EEGetSquareRing`, `EEGetSquareRingTiles`), the tiles in a radius and near a point, the
  diagonal-corner and repeat drops of a wall line, the compass (`EETurnDirection`, `EEGetTileInDirection`,
  `EEMoveInDirection`, `EEGetTileDirection`, `EEGetSlotTile`), the wall piece's frame set and yaw
  (`EEOrientWallPiece`), the jittered heading.
- **The world searches** (`uu.rs` over `UuHost`): the corner and block rings with their wall tests, the ring tiles
  (the C++'s fixed orders; MSVC's `std::sort` for the sorted ring), the units on tiles (`EEGetUnitsOn`,
  `EEGetUnitsOnTiles` with its sort-unique by id, the units of a type, the wall at a tile, a player's units of a type),
  the footprint and resource-spot tiles, the farm drop sites and free farm, the wall lines (`EEGetWallLine`,
  `EEExtendWallLine`, `EEPickWallPieces`, `EEMakeWallJunctions`), the site standing of units (`EEGetTileUnitStanding`,
  the blocking and movable units, `EEIsTileUnblocked`), `EECanStandOn`, `EEFindPlaceOnTile`, `EEGetTilesAround`,
  `EEPickTileNear`, `EEFindPlacement`, `EETurnAway`.
- **The placement and path leaves** (`uu_place.rs`): `EECanPlaceUnitOnTile`, `EEBlockTakes`/`EEPlayerBlockTakes`, the
  ship path smoothing (`EEAdjustNavalPath`), the dock launch place and facing (`EEGetDockShipPlacement`, with
  `EEGetFanTiles`).
- **The spot searches** (`uu_spots.rs`): `EEFindOpenSpot`, `EEFindResourceSpot`, `EEFindNextFreeTile`,
  `EEGetWallSideTiles`, `EEGetWallSpot`, `EEGetAttackPart`, `EEGetUnitsNearTile`.
- The C++ keeps every caller's signature (forwarders with the UUTL note); a unit's plain members are read in place in
  its records (`EeUnitsUuUnit`: the four Rust records the objects derive from, the pointer members); the world is
  `ee_units_uu_host_*` (several multiplexed by a selector). Deleted with no caller left: `EEGetTilesAround`,
  `EEGetRingTiles`, `EERemoveEdgeTiles`, `EEGetTownCenterLastRing`, `EEExtendWall`, `EEGetUnitsOfTypeOnTiles`,
  `EEGetTileInDirection`, `EEGetNearestFootprintTile`, `EEGetFanTiles`, `EEGetUnitsInFan`, `EEGetUnitsNearTile`,
  `EEFindOpenSpot`, `EEGetWallSideTiles` and the comparators only they used.

**Gate**: UUTL (tools/sim_capture.py): the outermost call of 50 utilities per thread (kind, inputs, outputs) folded
per physics tick, recorded on the unmodified C++ (oracle, late, space, calamity; seeds 2-3; two oracle runs equal).
EE-UU-COUNT in each game's log gives the calls per kind.

**Not reached by the gate games** (unit tests and review only): `EEGetEscapePoint` (stays C++), `EEIsTileUnblocked`
and `EEGetBlockRing` as outermost calls (their bodies run nested), the walls' `EEDropDiagonalCorners`/
`EEDropRepeatedTiles`/`EEPickWallPieces`/`EEMakeWallJunctions` (only the random map's walls and the wall planner,
which the profiles' AIs rarely build: `EEExtendWallLine` 15 calls, `EEGetWallLine` 3,021).

**Left** (C++): the target screens and searches' tests (`EESearch*`, `EENewTarget*`, the line of fire's model
reads: their arithmetic is ee_combat's), the units' replacement and conversion (`EEReplaceUnit`, `EEMakeUnitCopy`,
`EEConvertUnit`, `EEMovePassengers`, `EEDepositLoad`, `EEMakeBuilding`, `EEPlanWall`, `EEBuildWall`: object
lifetimes, effects, statistics), the sound parameters (`EEComputeSoundParameters`, `EEGetDopplerPitch`: the camera),
the scenario editor's placement (`EEEditorCanPlace`), the regions and footprint registration of buildings, the
escape point, the seabed depth, the path test (`EEHasPathToTarget`: a path request), `EEUpdateUnitTile`,
`EEGetHelicopterPriestDestination`, the selection bounds, the functors (`EEForEachUnitOf`, `EEHitPointUpgrader`),
`EEStepAsideFromSite`.
