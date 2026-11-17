# Object storage, lane P: players, world, map and tiles

The lane's state (the object-storage plan, "Lane P"). Each round rewrites this file to the current state; each
round's story goes in PORT_LEARNINGS.md ("Object storage, lane P, round N").

The move waits for lane V (`ee_sim::world`: typed in-place views of units, classes, stats, players, the map, tiles
and the world). It covers EEPlayer.cpp's plain host cases, EEWorld.cpp's plain cases, the tile sight in Rust over
the visibility words, and `EEMap::mVisibilityWords` as a Rust-owned `Vec<u32>` (C++ keeps the pointer; the save bytes
stay the same). This round (branch rust-objP) is the preparation that does not need V.

## State

- **TSGT** (TEST ONLY; `include/ee/EESightCapture.h`, `src/Empire Earth/Map/EESightCapture.cpp`): every sight
  function opens an `EESightScope`; when the outermost one on the thread returns, one record holds its kind, the
  player, its arguments, the tile range it can reach and an FNV-1a of that range after the call (each tile's
  visibility words and the terrain mesh tile's reference count; a tile's call hashes that tile's words and records
  its index in `mTileArray`). Nested calls record nothing (the outer range covers them), so the record stays the
  same when the inner calls move to Rust: only the outermost boundary must keep recording (from Rust if it moves
  there). Kinds: `RTSTile::AddSight`/`RemoveSight`/`SetVisibility`, `EEPlayer::AddSharedSight`/`RemoveSharedSight`/
  `AddOwnSight`/`RemoveOwnSight`, `EEMap::AddSight`/`RemoveSight`, the four circle and ring-step walks and
  `EEMoveSightCircle` (EESightRings.cpp), `RTSMap::FogSeen`/`ExploreUnseen`, `EEMap::PrepareSave`, the world's reveal
  of the map at a game's start, and a whole-map snapshot each game minute (EE-SIM's tick hook).
- **MAPS and MAPK now have a game**: the app profile's end sweep (`EESightCaptureSweep`, EE-SIM `--app-sweep` at the
  game's end, only MAPS, MAPK and TSGT recorded, then the capture closed): for every player `EEMap::AddSight` at three
  points with the wonder sight radius (70) and half of it shared, then `RemoveSight`; `ExploreUnseen` and `FogSeen`
  of the last player; two full cycles of `ComputeChecksum`'s 64 x 64 windows.
- **Goldens**: TSGT in the oracle and app profiles, MAPS and MAPK in the app profile, seeds 2-7, from the unmodified
  C++ (the TEST ONLY commit), two runs identical (see "Gates").
- **Gathered** (behaviour-identical): RTSTile's `AddSight`/`RemoveSight` and EEPlayer's eight tile-sight functions
  moved into EESightRings.cpp next to the circles, so the move replaces one file's bodies.

## The measure (for the move once V lands)

### EEPlayerHost (EEPlayer.cpp, 111 cases) and EEPlayer.cpp's other switches (92 cases)

"Plain" here: the body only reads or writes fields of a unit, class, player, world or tech node (trivial accessors
included), so a view read replaces the host call and the case goes. "Call": the body calls a C++ object's member
(a virtual, a manager, a town or herd, the tech tree's lookups, the RNG, the sound, the UI), so the case stays.

EEPlayerHost: **33 plain, 78 calls**.
- Unit and class (UnitView, ClassView): `CLASS` (mTechnology, mWeight, GetPopulationCost, mUnitType, mAIKind,
  mImprovements != 0), `UNIT_INFO` (27 fields of the unit, its class and complex unit: the largest), 
  `CONTAINER_CATEGORY` (container's class mCategory), `NEXT_UNIT` (mFirstUnit / mNext), `NEXT_COMPLEX`
  (GetNextComplexUnit's walk), `FOUNDATION` (IsFoundation), `BUILD_QUEUE_TECH` (mBuildQueue[i].mTechnology),
  `PENDING_POPULATION` (pending unit's class GetPopulationCost), `UPGRADE_VALUES` (class mFlightTime; the upgrade's
  mFlightTime, mPower, mMaxHitPoints: a StatsView), `COMPLEX_VALUES` (mFlightTime, mPower), `VIEWED_OF`
  (mViewedUnit->mPlayer), `HOLDER` (mHolder), `ASSIGN_TILE` (mTileX/Y, mLastMapPosition).
- Player and world (PlayerView, WorldView): `SCENARIO` (mGameOptions.mIsScenarioGame), `IS_LOCAL`, `IS_CURRENT`
  (GetCurrentPlayer), `NUM_PLAYERS`, `AT` (mPlayers[i]), `ID` (mPlayerID), `VIEW` (mEnemies[i], mPlayerID,
  mSaveVisMap), `PENDING_COUNT` (mPendingUnits.size()), `AREA_COUNT`, `AREA_AT` (mTowns/mHerds: std::vector reads),
  `STATE` (six globals: gLoadingSavedGame, gGameScreenActive, gWorldClearing, UShutdownInProgress,
  mVictoryEnabled, gUserInterface).
- Statistics (Rust already, player_stats.rs: a direct call, no view): `STATS_LOWER`, `STATS_RESET`.
- Tech node (no view in V's list: a NodeView or the tech lane's): `NODE_INFO`, `NODE_AVAILABLE`, `NODE_UNIT_CLASS`,
  `NODE_IS_AVAILABLE`, `TECH_OBSOLETE`.
- The world table (ee_data's records, not a view): `IMPROVEMENT_TIME`, `WONDER_FACTORS`.
- Calls (stay): NODE, NODE_COST (TCST pins its order), SCALE_COST, TAKE, ADD, CAN_AFFORD, EXTRA_POPULATION,
  POPULATION_LIMIT, PAY_IMPROVEMENT, UNIT_PLACED, WONDER_STARTED, RECOMPUTE_AREAS (virtuals the computer player
  overrides); METALLURGY, CIV_POWER, PULSE_EXPLORATION (civ powers); HERO_COUNT; REASON (strings); IMPROVEMENT,
  IMPROVEMENT_COST, LEVEL, CHANGE_LEVEL, UPGRADE_TYPE, UPGRADE_HIT_POINTS (EEUnitClassUpgrade's members); UPDATE_JIT,
  UNVIEW, REFRESH_TERRAIN, MINIMAP_REDRAW, SLAVERY_SOUND (UI, sound); RTS_ADD_UNIT, ACTIVATE, SWAP_KIND,
  DEAD_STATISTIC, LOG_LOST, UNIT_NODE, PRODUCE, SET_VALUE, GENERATE, RANDOM (RNG order); SET_NODE_AVAILABLE,
  RESEARCH, CITIZEN_CONTAINS, TECH_ALLOW, SET_CONTAINS, HEALED_SET (tech tree, unit sets); the towns and herds (19:
  ADD_TO_TOWN ... TOWN_PATROL); the wonders and diplomacy's list edits (OWNER_ADD_WONDER, WONDER_LIST, SHARE_BONUS,
  SIGHT_SHARER, ALLY, SHARERS, STORE_TABLE, RESET_WONDER_TIMERS); the sight walks (COMPLEX_SIGHT, UNITS_SIGHT,
  EVENTS_SIGHT); CAMOUFLAGE, UPDATE_WALL, REMOVE_FOREST, REMOVE_FROM_TOWN/HERD, REMOVE_SITE, CLEAR_FOOTPRINT.

The other switches: `EEPlayerTickOperation` 16 (plain: HERD_COUNT, GAME_TIME, TICK_TIME, TICK_MS, HEAL_VALUE,
UNIT_HIT_POINTS; the herds' fields need the herd objects), `EEPlayerSaveOperation` 49 (plain: 15 container sizes,
presences and ids, e.g. UPGRADE_SLOTS, WONDER_ID, the goal counter; the rest are the sub-objects' Load/Save),
`EEPlayerUnitOperation` 5 (plain: UNIT_FITS, UNIT_KIND), `EETownSaveHost` 12 and `EESavePrimitive` 10 (the file:
stay). **In all about 56 plain of 203 cases**, not the plan's 97: the rest call C++ objects. The plain bodies are
about 150 lines (UNIT_INFO's 27); the C++ net for EEPlayer.cpp's host is about -150 to -200, not -550.

### EEWorld.cpp (2,097 lines, 65 functions)

Plain (decisions over players', units' and tiles' fields, movable over WorldView/PlayerView/UnitView/TileView):
- `RecordStatistics` (277 lines): the post-game scores over the statistics (Rust), the players' mObserverMode,
  mGameResult, mDisconnected, IsHuman, mName, mResourceCaps, mColor, mSlot, the game options' resources and teams,
  and `gPhysics->GetRandom` draws (their order). Reached only at `EEWorld::Clear`: needs a capture of the records it
  fills (`mStatistics`) first; no profile records it yet.
- `CheckGameOver` (86) and `EEWorldCanRecover` (17): the players' mObserverMode, mEndOfGameCount, mGameResult,
  IsHuman, the units' and classes' fields; calls `RemovePlayerFromGame`, `SetGameResult`, the teams'
  `GetVictoryState`, `CanAfford` (host calls).
- `AddTeam` (33), `AddAlliance` (26), `RebuildTeams` (34), `SetDiplomaticStance` (13): the mEnemies tests (plain);
  the teams are C++ objects (EETeam holds an EEVCWonder), so the team edits stay host calls.
- Small: `IsTileBlocked` (a tile's height), `GetPlayerBySlot`, `GetPlayerIndex`, `MarkPlayerOut`,
  `RemoveDisconnectedPlayer`, `SetPopulationLimit`/`SetNumPlayers`' loops, `StartGame`'s reveal loop (sight).
- Not plain (stay): the unit lists (`ResortUnit`, `PlaceQueuedUnits`, `UpdateActiveUnits`, `RebuildActiveUnits`:
  the units lane's list nodes and handles), the media, the managers' lifetimes, `StartGame`/`CreatePlayers`/`Clear`
  (object wiring), `ReplacePlayer`, the resource users (a std::map of handles), `SetTimeOfDay` (math, no views).
About 550 lines of plain bodies; RecordStatistics is half of them and needs its capture first.

### The sight functions: callers and the fields they read

| Function | Callers | Reads (and writes) |
|---|---|---|
| `RTSTile::AddSight(player)` | EEPlayer::AddSharedSight (self, sharing allies), AddOwnSight; the unit sight host's `TILE_SIGHT` (sight.rs: a land building's sharers) | player mPlayerID; tile mVisibility[id >> 1] (adds 1 in the player's half; 0 -> SetVisibility(2)) |
| `RTSTile::RemoveSight(player)` | EEPlayer::RemoveSharedSight, RemoveOwnSight; `TILE_SIGHT` off | as AddSight (subtracts 1 while > 1) |
| `RTSTile::SetVisibility(value, player)` (inline, EEMap.h) | RTSTile::AddSight, RTSMap::FogSeen (-> 1), ExploreUnseen (-> 1), EEMap::PrepareSave (1 -> 0) | player mPlayerID; the word's other half kept |
| `RTSTile::GetVisibility(id)` (inline) | 82 readers: the AI (EECP*, EEComputerPlayer), the UI thread (EEFOFindNewTargetNearOldTarget 11, EEUIFMainGame), the renderer (EERenderObject, EEEffect, EEEffectHost), the units (EECombatGoals, EEUnitClass, EEUnitHost, EEActiveGroup), EEDoppelgangerManager, EEMiniMap's host, EESMObject, EEARust, the camera | the low byte of the player's half. Readers keep reading through the tile's `mVisibility` pointer, which stays valid when the words are a Rust Vec |
| `EEPlayer::AddSharedSight(tile, x, y)` | EEAddSightCircle, EEAddSightRingStep; AddSharedSightAt (EEMap host `SIGHT_AT`, map.rs; EEPPlayerEventList's event sight); the unit sight host's `SHARED` (sight.rs: an unbuilt unit's footprint, a kind-4 node's every player) | tile visibility (== 0: statistics series 0x18 + 1, Rust), mPlayerID, mSharedLOS, mAllies (std::vector), each ally's mSharedLOS; calls gDoppelgangerManager.TileShown(tile, x, y, player) (C++) |
| `EEPlayer::RemoveSharedSight(tile)` | EERemoveSightCircle, EERemoveSightRingStep; RemoveSharedSightAt (map host, events); `SHARED` off | as AddSharedSight; TileHidden |
| `EEPlayer::AddOwnSight` / `RemoveOwnSight` | AddOwnSightAt / RemoveOwnSightAt (map host `SIGHT_AT` unshared); EEWorld::StartGame's reveal (every tile, every player: add then remove) | as above without the allies |
| `EEMap::AddSight` / `RemoveSight` (body Rust: map.rs `sight`) | the unit sight host's `WONDER` (sight.rs: a land building with wonder bonus 3, radius kWorldWonderSightRadius); the app sweep | gSightUpdatesEnabled; the local player, IsAlliedWith, mSharedLOS (LOCAL_SEES); EEGetTilesNearPoint; the mesh's water flag; then SIGHT_AT, the mini map's CountCell, the mesh's Reference/DereferenceTile |
| `EEAddSightCircle` / `EERemoveSightCircle` / ring steps / `EEMoveSightCircle` (EESightRings.cpp) | the unit sight host's `CIRCLE` (sight.rs: a built unit); EEComplexUnit::SightMoved (Move) | gWorld.mMap, the current player, IsAlliedWith, both mSharedLOS; gSightRings; map GetTile; then AddSharedSight/RemoveSharedSight, gMiniMap CountCellOn/Off, the mesh's Reference/DereferenceTile |

For the move: the circles and the tile and player functions read the map (GetTile: TileView), the tile words
(TileView, or the Rust Vec directly), the player's mPlayerID, mSharedLOS and mAllies (PlayerView; mAllies is a
std::vector<EEPlayer*> V must expose) and the current player (WorldView). The world calls that stay C++ host calls:
the doppelganger manager's TileShown/TileHidden, the mini map's CountCellOn/Off (Rust already: minimap.rs, a direct
call), the mesh's reference counts (ee_ge, a direct call), the statistics series 0x18 (Rust, direct).

## Gates

TSGT (this round's tag), and the plan's list for the move: PLST, PLEC, TCST, MAPC, MAPP, MAPS, MMSN, MMPX, the app
profile, saveload, LAN (MAPK). The round's numbers are in PORT_LEARNINGS.md ("Object storage, lane P, round 1").

The goldens (seeds 2-7, two runs each, identical): TSGT in the oracle profile (31-40 thousand records a seed, about
3 MB of a 0.5 GB capture), TSGT, MAPS (30 records a seed) and MAPK (9) in the app profile (13-17 thousand TSGT). Every
other tag of both profiles still equal to its golden with the sweep added (the sweep runs after the game's last
record, exclusive, then closes the capture). Gate the move with `sim_capture.py run/check --profile oracle` and
`--profile app`. The digest says only that TSGT differs; to find the call, compare the two capture files' TSGT
records in order (each holds the kind, player, arguments and range).

## Left (waits for V)

- The tile sight in Rust over the views (RTSTile's, EEPlayer's, the circles), `mVisibilityWords` a Rust Vec.
- EEPlayerHost's 33 plain cases and the other switches' 23; EEWorld.cpp's plain bodies (RecordStatistics after a
  capture of its records).
- No game reaches a sight shared between allies (mSharedLOS with allies): a random map has no alliance; only a
  scenario or a lobby team game does. TSGT covers those code paths only where a game reaches them.
