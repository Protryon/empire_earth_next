# Lane: cycle cuts (the C++ dependency cycle)

Lane state (one file per lane). Round 2: branch `rust-cuts2` (round 1: `rust-cuts1`, from `origin/rust` b3ab7ba2c
plus `rust-cppdeps`, the census `tools/cpp_deps.py`). The measure: lines taken out of SCC 1, the biggest strongly
connected component of the C++ units' graph (`python -I tools/cpp_deps.py report`, its `sccs.txt` and
`summary.json`), after a full build.

## The cuts

See PORT_LEARNINGS "Cycle cuts" and "Cycle cuts, round 2" for the reasons. Each is its own commit; numbers are SCC 1's
units and lines after it.

Round 1 (branch `rust-cuts1`):

| cut | what | SCC 1 after |
|---|---|---|
| (base) | origin/rust b3ab7ba2c | 379 units, 133,985 lines |
| 1 | `gEmptyWideString` in a data-only WONGUI unit (`WONGUI/GUIEmptyWideString.cpp`) | 267, 122,971 |
| 2 | Empire Earth.cpp's globals in `EEGlobals.cpp`: the objects only storage (`ee/EEGlobalStorage.h`), constructed in Empire Earth.cpp where they were | 248, 120,016 |
| 3 | EECPEngineHelpers a leaf: `EEUnit::GetTechTreeNode` to EETechTree.cpp, `EEComplexUnit::GetMaxVelocity` to EEActiveGroup.cpp | 246, 119,730 |
| 4 | ee_sim's effect interface (`ee_sim::world::effects`), ee_render's implementation (`gfx/sim_effects.rs`) registered by `ee_rust_init` | (no C++ change) |
| 5 | WinMain names no test-driver symbol: `ee_sim_driver_start` / `ee_fixture_start` directly, EESimPreferences in Empire Earth.cpp, `gEETestFixtureConfigure` set by EETestFixture.cpp | 239, 113,529 |
| 6 | the physics' TEST ONLY hooks (`gPhysicsAfterTickHook`, ...) in EEGlobals.cpp | 237, 113,119 |
| 7 | EEUnitStore's captures compute the player index themselves | (with 8) |
| 8 | `gEERenderCaptureFill`: the renderer calls RFIL through a hook | 235, 112,046 |
| 9 | EEUnitClass.cpp's globals (gUnitClassTable, sUnitTypes, the heights) stored in EEGlobals.cpp | (with 10) |
| 10 | `gSeplScreenFont` in EEGlobals.cpp | 229, 111,335 |

Round 2 (branch `rust-cuts2`, from origin/rust 7779d7db1 + rust-cuts1 27cebd6c5: SCC 1 230 units, 109,421 lines):

| cut | commit | what | lines changed | SCC 1 after |
|---|---|---|---|---|
| 11 | f1b11b923 | EEWonNet's six UI calls through `ee/EEWonNetUI.h`, a table EEUIEWonNetArenaRoomEvents.cpp fills at static initialization (storage in EEGlobals.cpp); its own `""` for gEmptyString | +80 -29 | 226, 107,107 (-2,314) |
| 12 | a0521c0f7 | EEScenarioCapture: the 15 functions the game calls are header hook pointers (`inline R (*N)(...) = nullptr;`), set by the capture unit before main | +68 -37 | 225, 106,514 (-593) |
| 13 | 1b4264903 | EERenderListsView (the renderer's view of the render lists) moved from EERenderCapture.cpp to EEROUnits.cpp | +77 -70 | 224, 106,374 (-140) |
| 14 | df4ffd66c | EEMessageCapture hooks (3) | +14 -6 | 223, 106,045 (-329) |
| 15 | 3eb36c0ae | EECivCapture hooks (4) | +19 -10 | 222, 105,748 (-297) |
| 16 | 18090b59a | EEAECapture hook (1) | +8 -2 | 221, 105,555 (-193) |
| 17 | 4bdfe4761 | gEmptyString in EEGlobals.cpp (was the renderer's EEROUnits.cpp; 21 units name it) | +4 -3 | 221, 105,552 (-3; needed by 18) |
| 18 | ab406d597 | EEOpenSaveGameFile in a leaf unit of its own (`Persistent/EEOpenSaveGameFile.cpp`, units.EXTRA_UNITS) | +21 -7 | 218, 104,814 (-738) |
| 19 | 8088ac505 | EEWheel's constructors, destructor and EEIsWheelModel in EEGraphicObject.cpp, which makes the wheels (Update stays in EEWheel.cpp) | +32 -28 | 215, 104,252 (-562) |
| 20 | 5592b54d4 | EECalamityCapture hooks (2) | +12 -4 | 215, 104,258 (+6; needed by 21) |
| 21 | 40cffaef6 | EETechCapture hooks (27; default arguments through inline forwarders over `NHook`; EECalamityCanCastScope inline over three hooks) | +137 -68 | 213, 103,041 (-1,217) |
| 22 | f67dc5abc | EEAppCaptureWorld hooks (10; EEAppMapScope inline over two hooks) | +57 -32 | 212, 102,595 (-446) |
| 23 | 8377ce9e7 | EEGameLogOn/Tick/Line/Chat, one-line forwarders to ee_game_log, inline in ee/EEGameLog.h | +11 -8 | 210, 102,260 (-335) |
| 24 | 9729f0bb0 | EECPNextAI / EECPNextSetAI in EECPEngine.cpp, their one caller | +20 -13 | 209, 101,841 (-419) |
| 25 | 58166cf4b | EECombatGoals calls `ee_units_cls_base_condition` directly (EEUnitClass::BaseEvaluateCondition is its one-line forwarder) | +5 -1 | 208, 101,447 (-394) |

Round 2 in all: 22 units and 7,974 lines out of SCC 1. Also in the round: `tools/cpp_deps.py cuts` (7f316b38c) and
the fix of the Rust gate's `fs::golden_spec_replays` flake (51c6b499f).

On the tree merged with origin/rust 645799684 (whose own changes move SCC 1 too): 521 C++ units, 149,764 lines; SCC 1
199 units, 98,529 lines; leaves 66 (3,878 lines), near-leaves 132 (14,615).

Gates on the merged tree (2ca079a6d): build `--sim` 0/0 and the test profile 0/0; the oracle at seeds 2-7 identical;
the Rust gate PASS (4,881 tests); sim_capture oracle (212 digests), late (214) and app (26) at seeds 2-3 PASS;
scenario_check 27/27; sim_editor PASS (14,448 lines); `gates.sh` GATES PASS (tour; ai-game time-cap, units
[4, 39, 40], alive [2, 3], classic_lua x 2; lan-game). Per cut: build `--sim` 0/0 and the oracle at seeds 2-3; on the
first stack (cuts 11-19) also the tour, the Rust gate and the oracle, scenario and app captures.

## What to cut next

From `tools/cpp_deps.py cuts` after cut 25 (SCC 1: 208 units, 101,447 lines). The cheap kinds (a global's storage,
a TEST ONLY hook, a one-line forwarder, a helper in the wrong unit) are mostly used up; what is left ranks as real
calls:

- EECalamityClass -> EECalamityUpdate (1,292) and EEAIAStarView::Callbacks (1,238): one implementation split over
  units, not cuts (round 1).
- New on the merged tree: EEUIShell.cpp -> EESERBUnitTypes.cpp, `EESEDiplomacyShow` (877 lines, one symbol): the
  shells' op dispatch (`EE_SCREENS_SH_SE_DIPLOMACY`) calls the editor's diplomacy button. A handler the editor unit
  registers would cut it; it is the scenario editor lane's code of this round, so it is left to that lane or the
  next round. EEUIFSETerrain -> 2 units (749): its shell calls and the load thread's posts.
- EECalamityManager -> 4 units (705): the manager constructs calamities and units (EECalamity::EECalamity,
  EECalamityClass::EECalamityClass, EEUnitClass::CreateUnitAt). A factory registration would hide a real call.
- RTSPlayer -> 2 units (485): EESavePrimitive, RTSRegisterUnit, RTSUnit's list. The save primitives
  (EESavePrimitive) are named from several units: a leaf unit for them may be the next cheap cut.
- EEWonNetAvailableGameFormManager -> 2 units (456): the unit is the WON form's manager plus free UI helpers (map
  picks, screenshots); splitting the helpers out is a unit split, not an edge.
- EESEInstructions (413), EEUIFMGStatusBox (385), EEUIMiniMap (376), EEUnitClassUpgrade (372), EEMiniMap (371):
  units whose few out-edges are real calls into the units and UI.
- The WON lobby (LobbyLifecycle's globals 327, gLobbyData 269, LobbyModelHost 337, InitLogic 318): left to the WON
  lanes, which are moving those classes to Rust.
- EESightCapture: the capture unit also defines EESightScope, the game's sight bookkeeping: a move, not a hook.

## Tools of the lane

- `tools/cpp_deps.py report [--code-only]`, `unit FILE`.
- `tools/cpp_deps.py cuts [--max-weight N] [--top N]` (round 2): the what-if over SCC 1, ranked by the lines SCC 1
  would lose: one edge, one symbol (every edge it alone makes), a unit's globals, a unit's out-edges; each classed
  `data`, `hook`, `vtable` or `call` by its symbols. Writes `cuts.txt` and `cuts.json` under `--out`. It reads the
  objects: run it after a build, never during one. It weighs single cuts: a cut that pays only with another (17 with
  18, 20 with 21) shows up after the first is made, or in a scratch what-if over `edges.tsv` with both removed.

## The effect interface (cut 4)

`ee_sim::world::effects`: `Effects` (create by `EffectRequest { graphic_effect, place }`, destroy, kind,
set_local_to, finish_damaged, exists), `EffectPlace` (one variant per `EEEffectManager::Create*`), `EffectHandle`,
`register`, `effects()` (`NoEffects` until registered: every entry point answers 10 and makes nothing). The
implementation (`ee_render::gfx::sim_effects::RENDER_EFFECTS`) works on the game's current manager
(`EeGfxManagerWorld::current`, i.e. `gEffectManager->mRust`) and builds the same `EeGfxEffectArgs` as
ee/EEEffectManager.h. No unit body uses it yet: a body that moves into ee_sim calls `effects().create(...)` where the
C++ calls `gEffectManager->Create...`.
