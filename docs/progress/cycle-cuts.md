# Lane: cycle cuts (the C++ dependency cycle)

Lane state (one file per lane). Branch `rust-cuts1`, from `origin/rust` b3ab7ba2c plus `rust-cppdeps` (the census,
`tools/cpp_deps.py`). The measure: lines taken out of SCC 1, the biggest strongly connected component of the C++
units' graph (`python -I tools/cpp_deps.py report`, its `sccs.txt` and `summary.json`), after a full build.

## The cuts

See PORT_LEARNINGS "Cycle cuts" for the reasons. Each is its own commit; numbers are SCC 1's units and lines after it.

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

In all 150 units and 22,650 lines out of SCC 1. On the tree merged with origin/rust 7779d7db1 (whose own
changes move SCC 1 too): SCC 1 230 units, 109,421 lines; leaves 65 (3,846 lines), near-leaves 133 (14,286);
`--code-only` 223 units, 108,815 lines, leaves 79 (5,645).

## What to cut next

From the what-if over `build/cpp_deps_*/edges.tsv` after cut 10 (lines that would leave SCC 1):

- EEWonNet.cpp's six UI calls (EEGamePreferences::Save, the two arena screens' ProcessEvent, EEUIManager's
  DisplayMessage/DisplayCancel, gEmptyString): about 2,300 lines (EEWonNet and the WON net glue). Not cheap: the
  network code drives the screens; it wants a UI-callback table.
- EEWheel.cpp, EEMessage.cpp, EEPlayer.cpp: the new top keystones (about 700 lines each), whole-unit moves.
- EECalamityClass -> EECalamityUpdate (1,292) and the A* view's callbacks are one implementation split over
  units: not cuts.

## Tools of the lane

- `tools/cpp_deps.py report [--code-only]`, `unit FILE`.
- What-if analysis over a report's `edges.tsv` (not committed: scratch scripts): delete one edge (or every outgoing
  edge of a unit with few symbols) and recompute SCC 1. This found the cheap cuts after the first three: the edges
  from WinMain to the test drivers, the TEST ONLY hooks in EEPhysics.cpp.

## The effect interface (cut 4)

`ee_sim::world::effects`: `Effects` (create by `EffectRequest { graphic_effect, place }`, destroy, kind,
set_local_to, finish_damaged, exists), `EffectPlace` (one variant per `EEEffectManager::Create*`), `EffectHandle`,
`register`, `effects()` (`NoEffects` until registered: every entry point answers 10 and makes nothing). The
implementation (`ee_render::gfx::sim_effects::RENDER_EFFECTS`) works on the game's current manager
(`EeGfxManagerWorld::current`, i.e. `gEffectManager->mRust`) and builds the same `EeGfxEffectArgs` as
ee/EEEffectManager.h. No unit body uses it yet: a body that moves into ee_sim calls `effects().create(...)` where the
C++ calls `gEffectManager->Create...`.
