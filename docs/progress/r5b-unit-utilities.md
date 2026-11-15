# R5b unit utilities (src/Empire Earth/User Interface Thread/EEFOFindNewTargetNearOldTarget.cpp)

The lane's state. Each round rewrites this file to the current state; the rounds' stories go in PORT_LEARNINGS.md
("The unit utilities, round 1", "The unit utilities, round 2").

The file is the original's "EEUnitUtilities.cpp" (its asserts name `EEUUGetIsUnitClassTargetable`; the unit map named
it after the one class it defines on the stack): some 200 free functions the units, goals, AI, calamities, random
map and screens call, and the `EEFOFindNewTargetNearOldTarget` screen. Before round 1 the target searches, the line
of fire, the attack reach and range tests, the tile line and the ballistic and lead arithmetic were already
`ee_combat`'s (R5b.2); 6,380 lines of C++, 3,996 now.

**In Rust** (`ee_units` uu.rs, uu_tiles.rs, uu_place.rs, uu_spots.rs, uu_world.rs over uu_host.rs; entry points in
uu_ffi.rs; msvc_sort.rs):

- Round 1 (branch `rust-unitutil1`): the tile geometry (sub-tile steps, rings, squares, the compass, the wall
  pieces' frame sets), the world searches (rings with their wall tests, the units on tiles, footprint and resource
  spots, the farms, the wall lines, the site standing of units, `EECanStandOn`, `EEFindPlaceOnTile`,
  `EEPickTileNear`, `EEFindPlacement`), the placement and path leaves (`EECanPlaceUnitOnTile`, `EEBlockTakes`, the
  ship path smoothing, the dock launch), the spot searches.
- Round 2 (branch `rust-unitutil2`, uu_world.rs): the submarines' depth (`EEGetSeabedDepth`,
  `EEAdjustDiveDepth`, `EEGetSeabedUnder`), `EEGetUnitTiles`, `EEGetSelectionBounds`, `EEGetUnitRadius`,
  `EEGetAngleFromNorth`, the gather reach (`EEIsWithinReach`, `EEIsWithinReachOf`), `EEIsFootprintVisibleTo`, the
  escape point (`EEGetEscapePoint`, `EEGetEnemiesAround`, `EEGetUnitAngle` and its sort), the buildings' footprint
  and region registration (`EEAddToFootprintTiles`, `EERemoveFromFootprintTiles`, `EEAddBuildingRegion`,
  `EERemoveBuildingRegion`, `EEClearUnderBuilding`), `EEStepAsideFromSite`, the walls (`EEBuildWall` with
  `EEHasZeroCount`, `EEPlanWall`, `EEShapeWall`), the walls on a line (`EEIsTileTopAbove`, `EEIsLineBlocked`),
  `EEGetPathLengthTo`, the sound (`EEComputeSoundParameters`, `EEGetDopplerPitch`: pure, over the camera's view the
  C++ forwarder gathers), `EEDepositLoad`, the scenario editor's placement (`EEEditorCanPlace`,
  `EEIsEditorSpacingClear`).
- The C++ keeps every caller's signature (forwarders with their UUTL note); the world is `ee_units_uu_host_*` (round
  2: `ee_units_uu_host_world` / `_world_f` multiplexed by a selector, the units near a point, a unit region's units,
  a path's first tile).

**C++**: round 1 net -1,684; round 2 net -741 (the capture +73, the move -814); the file 6,380 -> 4,725 -> 3,996
lines.

**Gate**: UUTL (tools/sim_capture.py, the outermost call of 85 utilities per thread: kind, inputs, outputs and, for
the bodies that write to the world, their writes as events; folded per physics tick), recorded on the unmodified C++
(oracle, late, space, calamity; seeds 2-3). EE-UU-COUNT in each game's log gives the calls per kind.

**Not reached by the gate games** (unit tests and review only): the submarines' depth, the selection bounds, the
facing radius, the angle from north, the footprint's visibility, the escape point, the path length, the Doppler
pitch, the editor's placement; the walls' `EEDropDiagonalCorners`/`EEDropRepeatedTiles`/`EEPickWallPieces`/
`EEMakeWallJunctions` and the wall builders and plan only in late and space (a few calls a game); `EEIsLineBlocked`
as an outermost call only in space.

**Left** (C++): the target screens' and searches' tests (the `EESearch*`, `EENewTarget*` and line-of-fire hosts of
ee_combat's searches: virtual and goal reads, little arithmetic), `EEFindNewTargetNearOld` and the screen's
virtuals, the units' replacement and conversion (`EEReplaceUnit`, `EEMakeUnitCopy`, `EEConvertUnit`,
`EEMovePassengers`, `EEMakeBuilding`: object lifetimes, effects, statistics, chat; noted in UUTL already),
`EEHasPathToTarget` (a path request and the sub sectors' links), `EEUpdateUnitTile`, `EEGetHelicopterPriestDestination`,
`EEGetAimPoint`, the functors (`EEForEachUnitOf`, `EEHitPointUpgrader`), `EEGetWaitingGatherer`, `EEIsUnitMovable`,
the hosts.

**Next**: the replacement and conversion as Rust sequences over the host (their notes are recorded; little
arithmetic, so the C++ gain is small); `EEHasPathToTarget` once the path manager's requests are Rust; the target
screens' tests once the goals' casts and the units' virtual reads are Rust (then the three ee_combat hosts, ~500
lines, can go); a profile where computer units flee (the escape point) and one with submarines.
