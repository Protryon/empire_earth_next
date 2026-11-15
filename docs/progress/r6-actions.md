# R6 game actions

The lane's state, as the other lanes' files in this folder. Each round rewrites this file to the current state;
the round's story goes in PORT_LEARNINGS.md ("R6: the game actions, round 1").

**Round 1** (`ee_screens` `game_actions.rs`, branch `rust-actions1`): every class of `src/Empire Earth/Actions`
but `EEAWonNet` (17 classes) and the quick save/load pair of `Scenario Editor Actions` (`EEAQuickSaveLoad`,
`EESEAQuickSaveLoad`) are one C++ class, `EEARust` (`include/ee/EEARust.h`): an `EEAction` whose second base is the
Rust record `EeScreensGaFields` (the original members' names, so the forms that set them compile unchanged), its
kind picking the original class's behaviour. The original class names are one-line constructors in the same header.
Every decision is Rust; every world read and call is one op of one host (`ee_screenscpp_ga`, plus
`ee_screenscpp_ga_record` for the record messages, in `src/Empire Earth/Actions/EEARust.cpp`). The C++ objects
(the strings, the poll, the mini map's point, the scroll's edges, the players list) stay C++ members of `EEARust`,
read by the host. 19 units and 19 headers deleted; net C++ (src, include/ee) -2,383.

The golden (`build/ga/drv` in the round's worktree, not committed): the 19 original units cut from 3114725d into
namespace O against `EEARust`, its host and the Rust library over one fake world: 0 differences on every kind.
`rust/crates/ee_screens/src/golden_game_actions.txt` (817 cases) is the new side's host log, replayed by
`game_actions_tests::replays_the_golden`. Mutations: host 28/28 caught; Rust 37/40 by the replay, 2 more by the
driver, 1 equivalent.

Gates that reach the actions: the orders game and the box selection (`fixture_game.py --orders --box-select`), the
scroll check, the LAN game (the join and host), the editor gate (the quick save), the calamity fixture (`--cast`).

**Next:** `EEAWonNet` (the WON lanes' area: its lobby calls are WON's); the editor's actions still in
`Scenario Editor Actions/EESEARust.cpp` (912 lines: the paints' and the whole actions' hosts, the selection host)
onto the same host shape; the click's and lasso's editor subclasses (`EESEAMouseClick`, `EESEALasso`, `EESETSA*`) as
`EEARust` kinds once the editor lane's trigger tab no longer subclasses them.
