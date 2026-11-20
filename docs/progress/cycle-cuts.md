# Lane: cycle cuts (the C++ dependency cycle)

Lane state (one file per lane). Round 3: branch `rust-cuts3`, from the lead's staging tree e4e72b984 (cuts rounds 1-2,
LLE 2, game leaves 2, UI 5; origin/rust 3449a04fd already in it). Round 1: `rust-cuts1`; round 2: `rust-cuts2`. The
measure: SCC 1, the biggest strongly connected component of the C++ units' graph (`python -I tools/cpp_deps.py
report`, its `sccs.txt` and `summary.json`), after a full build; this round also the UI units (User Interface
Thread, UI Events) in SCC 1 and in any cycle.

## Where it stands

- **SCC 1: 164 units, 82,924 lines** (round 3 start: 197 units, 98,059 lines; round 1 start: 379 units, 133,985).
- **No User Interface Thread or UI Events unit is in any cycle** but EEFOFindNewTargetNearOldTarget.cpp, which is the
  unit utilities (2,856 lines of simulation code in the UI folder: EEGetUnitsNear, EECanStandOn, EEFindPlacement,
  ...; every simulation unit calls it and it calls them), not UI. The round started with 21 of the 32 UI units in
  SCC 1 and none outside a cycle. The scenario editor units that were in SCC 1 with them are out of it too:
  EEUIFScenarioEditor, EESEUnit, EESERBUnitTypes, EEUIFSETerrain, EEUIFSETRBPaintType, EEUIFSEUpgradePanel,
  EESETSTrigger, EESELoadThread and EESEARust in no cycle; EESEEvents and EESETSEffect in a 2-unit cycle of their own.
- The census: 469 C++ units, 141,643 lines; leaves 56 (3,328 lines); near-leaves 120 (14,895); 10 cycles, 270 units,
  99,138 lines (round 3 start: 9 cycles, 301 units, 113,265).
- The UI units now ("no cycle": a node of the condensation's DAG, which moves whole once its C++ targets have hosts
  or are Rust; targets = the C++ units it names):
  - leaf: UI Events/EEScreenEvent.cpp (new this round, 47 lines);
  - near-leaf: EEUIEWonNetStatsEvents (EEGlobals, UIForm, UIManager);
  - 4 targets, all LLE UI or the game's globals: EEUICChatLine, EEUICWonNetRoomsBox, EEUICWonNetStatBox,
    EEUIRustControl (UIControl, GEBitmap, UIForm/UIControlsTable, UILBString/UIFontTable or RTSMouse), EEUIBuildGate,
    EEUIBuildWall (EEGlobals, EEPlayer, EEUnitClassBuildings or the tech tree, EEWorld);
  - 5-15 targets: EEUIEScenarioIntroEvents 5, EEUIEWonNetServerSelectEvents 6, EEUICSelectUnits 6,
    EEUICWonNetGamesBox 6, EEWonNetAvailableGameFormManager 6, EEUIFMGAttribute 6, EEUIEWonNetModifyUserAccountEvents
    7, EEUICCommandUnits 8, EEUIEWonNetArenaRoomEvents 9, EEUIFMGStatusBar 9, EEUIBGenericBackground 10,
    EEUIEWonNetArenaGameEvents 11, EEUIFError 11, EEUIMiniMap 12, EEUIFMGStatusBox 13, EEUICPortrait 14,
    EEUIBUpgrade 15;
  - the hubs: EEUIFMGMainGroup 21, EEUIFMGStatusButton 23, EEUIShell 33, EEUIManager 34, EEUserInterface 50,
    EEUIFMainGame 55, EEScreens 72.

## The cuts

Round 3 (this branch). Each is its own commit; build `--sim` 0/0 and the oracle at seeds 2-3 identical after each.
"Layer" is `cpp_deps.py layer --focus 'User Interface Thread/,UI Events/,Scenario Editor/,Scenario Editor
Actions/,!EEFOFind'`: the edges from the rest of SCC 1 into the UI and editor units, and their distinct symbols.
Lines changed: src and include.

| cut | commit | what | lines | SCC 1 after | UI in SCC 1 | layer edges / symbols |
|---|---|---|---|---|---|---|
| (base) | e4e72b984 | | | 197, 98,059 | 21 | |
| 26 | 592b383fc | gMiniMap, gU56534C, sIconsLoaded stored in EEGlobals.cpp (16 units named EEUIFMainGame.cpp only for gMiniMap) | +9 -3 | 197, 98,059 | 21 | 52 / 66 |
| 27 | 448ea7e88 | EEScreenEvent, EECreateScreenEvent, the grab scroll's events in a leaf unit (UI Events/EEScreenEvent.cpp, units.EXTRA_UNITS) | +48 -33 | 197, 98,028 | 21 | 47 / 65 |
| 28 | 7ebd189cd | the chat log's entry points (EEUIECLEFormManager) inline over the Rust log | +23 -24 | 197, 98,012 | 21 | 45 / 62 |
| 29 | 05fb7ccad | EEUIFMainGame's Load, Save, SetUIState, SetErrorString, the backgrounds and Rust inline; EEUIFMGFormCall in ee/EEUIFMainGame.h | +82 -90 | 197, 97,931 | 21 | 39 / 56 |
| 30 | 16a7165a8 | EESMDialogMessage's Load and Save in EEScenarioManager.cpp with its constructors (they were the editor's paint type tab's) | +53 -51 | 194, 96,736 | 21 | 37 / 54 |
| 31 | 8ee3d7dd9 | the game's actions name no editor unit: EESELoadThread's posts inline, EESEAObjectSelection in EEARust.cpp, its one caller | +36 -87 | 194, 96,613 | 21 | 35 / 50 |
| (census) | dae1142ca | cpp_deps: a template instance belongs to its class's home only where the home defines that instance | tool | 191, 95,864 | 20 | 33 / 22 |
| 32 | dc5c8d49f | EEUserInterface's posts the game makes (PostEvent, ShowLoadedObject, PostDialogMessage, AddChatLogEntry x2), ResumeGraphics and HandleEvent inline; EEPumpWindowEvent and EENativeMessageHook in Empire Earth.cpp, which installs them | +110 -116 | 191, 95,864 | 20 | 16 / 15 |
| 33 | f4687cd61, 492edd59b | EEWonNetAvailableGameFormManager.cpp's game helpers (EEOrderAtCursor, EEMakeCalamityEffect, EEGetFirstSelectedUnit, EEPointUnitOrTile(XY), EEExitGame) in EEGameUtilities.cpp; the test profile's harness hook follows EEPointUnitOrTile | +117 -96 | 191, 95,885 | 20 | 13 / 10 |
| 34 | b30f48d7b | the game's UI table (ee/EEGameUI.h, storage in EEGlobals.cpp); the message box (EEUIFErrorDisplay) through it | +59 -7 | 190, 95,660 | 19 | 6 / 9 |
| 35 | 1f162b6f6 | the command buttons (ShowCommands), the status box's icons, the mini map's media and update and the UI manager's construction through the table | +42 -17 | **164, 82,913** | **1** | 0 / 0 |
| 36 | ff891341a | EEUserInterface::PostScreenShot inline | +9 -12 | 164, 82,913 | 1 | the UI's own cycle (13 units, 9,160 lines) splits into 2 + 3 + 5 units |
| 37 | 29d389e20 | gMultiplayerString stored in EEGlobals.cpp, constructed in EEScreens.cpp | +8 -2 | 164, 82,913 | 1 | EEScreens and EEUserInterface out of every cycle |
| 38 | 63076beaf | EEUIFMainGame's ButtonCall, BuildUnit and start modes inline; the gate and wall placements call the build action | +38 -79 | 164, 82,913 | 1 | EEUIFMainGame, EEUIBuildGate, EEUIBuildWall out |
| 39 | 31f17c0d2 | the editor's diplomacy button state through a pointer EESERBUnitTypes.cpp sets (gEEUIShellDiplomacyShow, ee/EEUIShell.h) | +7 -3 | 164, 82,913 | 1 | EEUIShell, EESERBUnitTypes, EESEUnit out |
| 40 | 0d384f309 | EETechTreeNode::GetHotKey and EEButtonObject::GetHotKey in their classes' units | +26 -16 | 164, 82,924 | 1 | EEUIFMGStatusButton, EEUIFMGMainGroup out: no UI cycle left |

Round 3 in all: 33 units and 15,135 lines out of SCC 1; every UI unit out of every cycle; +853 -650 lines in src,
include and tools. Also: `cpp_deps.py cuts --focus`, `why`, `layer`, `--of` (19635a0e2, 3ce700885, 61552cb39) and
the census fix dae1142ca (3 of the 33 units: EEUIEWonNetServerSelectEvents, EEUIFSETerrain, EECBUIListBox were in
SCC 1 only through a false edge).

Rounds 1-2 (PORT_LEARNINGS "Cycle cuts" and "Cycle cuts, round 2"): cuts 1-10 took SCC 1 from 379 units, 133,985
lines to 229, 111,335 (gEmptyWideString's data unit, Empire Earth.cpp's globals in EEGlobals.cpp under
ee/EEGlobalStorage.h, WinMain off the test drivers, the physics' and render capture's test hooks, EEUnitClass's
data); cuts 11-25 from 230, 109,421 to 208, 101,447 (EEWonNet's UI through gEEWonNetUI, the TEST ONLY captures behind
header hook pointers, misplaced functions moved to their one caller or class, one-line Rust forwarders inline).

## What to cut next

- **For the UI lane: nothing left in SCC 1.** The UI units sit above SCC 1 and move bottom-up: the near-leaves and
  the 4-target units first (their targets are LLE UI controls and a few game globals), the hubs (EEScreens,
  EEUIFMainGame, EEUserInterface) last.
- **EEFOFindNewTargetNearOldTarget.cpp** (the unit utilities) is the simulation's, not the UI's: the units lanes'.
- **EESEEvents <-> EESETSEffect** (a 2-unit editor cycle, 1,008 lines) and EESEIAExport <-> EESEInstructions: the
  scenario editor lane's.
- **SCC 1's own cheap cuts** (`cpp_deps.py cuts` after cut 40) are what rounds 1-2 listed: EECalamityClass ->
  EECalamityUpdate and EEAIAStarView's callbacks (one implementation split over units, not cuts), EECalamityManager's
  constructions, RTSPlayer's save primitives, EEARust's home of EEGroup/EETile/EEUnitClass members (EEARust.cpp
  defines EEGroup::GetFirstUnit, EETile::IsVisibleTo and EEUnitClass::IsTransport/IsDropSite/IsAirBase, which 20
  simulation units call), the WON lobby's globals (the WON lanes').
- The UI table (ee/EEGameUI.h) is the place for any further UI entry the game calls: add a field, fill it in the
  UI unit's static initializer, call it from the game. `cpp_deps.py layer --focus 'User Interface Thread/,UI
  Events/,!EEFOFind'` lists any new edge from the game into the UI.

## Tools of the lane

- `tools/cpp_deps.py report [--code-only]`, `unit FILE`.
- `cuts [--max-weight N] [--top N] [--focus P,...] [--of FILE]` (rounds 2-3): the what-if over SCC 1 (or, with
  `--of`, over the cycle holding FILE), ranked by the lines it would lose; with `--focus` (unit name substrings,
  `!` to exclude) first by the focus units a cut frees, keeping only those. Classes `data`, `hook`, `vtable`, `call`.
- `why FILE [--focus ...]` (round 3): why a unit is in SCC 1: its shortest cycle with each edge's symbols, and the
  units of SCC 1 naming it (outside the focus).
- `layer --focus ... [--of FILE]` (round 3): the focus units as a layer above the rest of SCC 1: every edge into it
  by symbol (the back edges to cut) and SCC 1 with all of them cut. It was this round's plan: its first run listed
  the symbols of cuts 26-35.
- All read the objects: run them after a build, never during one.

## The effect interface (cut 4)

`ee_sim::world::effects`: `Effects` (create by `EffectRequest { graphic_effect, place }`, destroy, kind,
set_local_to, finish_damaged, exists), `EffectPlace` (one variant per `EEEffectManager::Create*`), `EffectHandle`,
`register`, `effects()` (`NoEffects` until registered: every entry point answers 10 and makes nothing). The
implementation (`ee_render::gfx::sim_effects::RENDER_EFFECTS`) works on the game's current manager
(`EeGfxManagerWorld::current`, i.e. `gEffectManager->mRust`) and builds the same `EeGfxEffectArgs` as
ee/EEEffectManager.h. No unit body uses it yet: a body that moves into ee_sim calls `effects().create(...)` where the
C++ calls `gEffectManager->Create...`.
