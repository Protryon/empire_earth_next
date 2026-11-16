# R6 game actions

The lane's state, as the other lanes' files in this folder. Each round rewrites this file to the current state;
the round's story goes in PORT_LEARNINGS.md ("R6: the game actions, round 1", "round 2").

**Round 1** (`ee_screens` `game_actions.rs`, branch `rust-actions1`): every class of `src/Empire Earth/Actions`
but `EEAWonNet` (17 classes) and the quick save/load pair of `Scenario Editor Actions` are one C++ class, `EEARust`
(`include/ee/EEARust.h`): an `EEAction` whose second base is the Rust record `EeScreensGaFields` (the original
members' names), its kind picking the original class's behaviour, over one host (`ee_screenscpp_ga` in
`Actions/EEARust.cpp`). 19 units and 19 headers deleted; net C++ -2,383. `golden_game_actions.txt` (817 cases).

**Round 2** (branch `rust-actions2`):
- Every class of `Scenario Editor Actions` is an `EEARust` kind too (kinds 20-26): the editor's click and lasso
  (`EESEAMouseClick`, `EESEALasso`), the trigger tab's (`EESETSAMouseClick`, `EESETSALasso`), the load text
  (`EESEAChangeLoadText`), the paints (`EESEAPaint`/`EESEAPaintRust`, their Rust box in the record: `mPaint`) and the
  whole actions (`EESEARust`: `mAction`). `Ga::run`/`free`/`click_execute`/`select_in_rectangle`/`paint_set_area`
  pick the editor's behaviour by kind and call `editor_actions.rs` / `editor_whole.rs` (`GaHost::editor_call`).
  Six headers deleted.
- The editor's two mouse-mode controls run the HUD's own Rust: `EESEUICCommandUnits` (`command_units.rs`, `mEditor`)
  and `EESEUICTriggerSelectUnits` (`select_units.rs`, `mTrigger`, `mRightButton`); their two units deleted.
- `Scenario Editor Actions/EESEARust.cpp`'s includes pruned (110 -> 57).
- Golden: the screens lane's round-29 HUD driver extended with the two editor controls (`build/ga2/hud` in the
  round's worktree): 0 differences; `golden_hud_editor.txt` (199 cases) replayed by cargo. Mutations: host 12/12,
  Rust 22/22 (one by the driver). Net C++ (src, include/ee): -430, then -54.
- The scroll check: round 1's and rust 03ab5fe8's test exes behave the same (both show the one-step edge scroll
  that failed round 1's final run); PORT_LEARNINGS has the numbers.

Gates that reach the actions: the editor gate (every editor action kind), the orders game and the box selection
(`fixture_game.py --orders --box-select`), the scroll check, the LAN game (the join and host), the calamity fixture
(`--cast`). Only a person: the editor's right drag (rotate, delete under the mouse) and the trigger tab's object
screen selection (pinned by the driver's golden).

**Next:** the input path's C++ left is hosts (`Actions/EEARust.cpp`, `EESEARust.cpp`'s four editor hosts, the HUD
controls' ops, the action queue's host, `EEUIFMainGame.cpp`'s mouse-mode host). `EEAWonNet` is the WON lanes'. Not
this lane's but on the input path: `EEUnitPicker.cpp` (525) and `EECameraControl.cpp` (894), both the gfx lane's;
`RTSMouse.cpp`. A sturdier scroll check (the fixture holding the pointer at the edge for the hold, TEST ONLY) would
remove the one-step flake.
