# The UI thread lane (`User Interface Thread/`, `UI Events/`)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The UI thread lane, round 1").

**Scope.** `src/Empire Earth/User Interface Thread` and `src/Empire Earth/UI Events`: the HUD (the main game form,
its panel groups, the status box and buttons, the portrait, the icon bar, the mini map host), the UI thread
(`EEUserInterface.cpp`), the UI manager, the message box, the game screens' host (`EEScreens.cpp`) and the UI
event classes. Rust home: `ee_game` screens. Two parts of these directories are other lanes': the unit utilities
(`EEFOFindNewTargetNearOldTarget.cpp`, 3,670 lines: simulation code, R5b unit utilities lane,
`docs/progress/r5b-unit-utilities.md`) and the WON boxes and events (`EEUICWonNet*`, `EEUIEWonNet*`,
`EEWonNetAvailableGameFormManager.cpp`: the WON lanes').

## The measure (round 1, at origin/rust 4929244f3)

`build/ut1/measure.py` splits every `switch` of the lane's files into case blocks and classes them by what the body
names: world reads (`W`: gWorld, units, players, classes, the selection), toolkit calls (`T`: GetControl,
SetDisplayMode, entity groups, backgrounds, the manager's forms), both, or other (`O`: a C++ object no Rust object
stands for: GE bitmaps, rhombi, models and text, the rasterizer, sounds, the physics, the session, files, the
message factory).

| File | Lines | Case lines | W | T | O |
|---|---|---|---|---|---|
| `EEScreens.cpp` | 3,018 | 1,996 | 212 | 241 | 1,518 |
| `EEUIFMainGame.cpp` | 1,514 | 904 | 250 | 107 | 533 |
| `EEUserInterface.cpp` | 1,248 | 484 | 58 | 30 | 382 |
| `EEUIFMGStatusBox.cpp` | 517 | 230 | 33 | 31 | 166 |
| `EEUIManager.cpp` | 493 | 166 | 14 | 0 | 152 |
| `EEUIFMGStatusButton.cpp` | 489 | 154 | 20 | 7 | 127 |
| `EEUIFMGMainGroup.cpp` | 465 | 151 | 57 | 48 | 46 |
| the other HUD files (portrait, icon bar, mini map, upgrade, error form, ...) | ~2,700 | ~650 | ~130 | ~50 | ~470 |

Nearly every line left is host code: one case per call into a C++ object. Earlier rounds (game screens 1-31,
screens B 1-4, lane S) moved the logic; what remains moves only when its target object is Rust. This round took
what the world's views (`ee_sim::world`), the toolkit's direct path (`ee_ui direct.rs`) and the record messages
(`ee_screenscpp_ga_record`) can answer.

## What is in Rust (round 1)

- `ee_game screens/hud_views.rs`: in the FFI adapters, below the hosts' traits (the replay goldens unchanged):
  - the main panel group's world reads (in-cinematic, the selection's count and units, the viewed unit, a unit's
    hit points, id, kind bits, `mL294`, airport, owner, commands, epoch before WW2, on-map, worker, the production
    queue) and toolkit calls (display modes, the behaviour and formation panels, the hot key group) on its own
    forms (`EE_SCREENS_MGR_OP_FORMS`: `mOwner`, `gUIManager->mMainGameForm`);
  - the icon bar's (the selection, cargo, id, type, tile, hit points, button), the portrait's (owner, production
    time, research), the improvement button's (the selection), the command buttons' (the selection's count, unit
    and type; the class table's presence, gate, name id) and the main form's (current player, selection, viewed
    unit, resources, victory, cinematic, game result; the display modes, dirty flags and current form on the call's
    own form, `EEUIFMGFormCall::mForm`);
  - whole fills: the status box's view of the selected unit (`EEStatusBoxView`), the hot keys' unit snapshot
    (`ee_screenscpp_hot_key_units`; the aircraft target and `sHotKeyUnits` stay two C++ helpers), the diplomacy
    form's player (`IsHuman`, a virtual, one C++ helper) and the read view's airport question.
- `ee_game screens/hud_sends.rs`: the game messages whose case was one `ProcessLocalUserInput(new EEMRecord(...))`
  over the case's arguments: the screens' hot keys, chat type, rally, kill, attack, speed, behaviour, diplomacy's
  resources, stance, chat, tribute and resign; the icon bar's select, unload, scramble and remove; the portrait's
  two cancels; the command buttons' twelve; the mouse modes' airport, attack ground and flare. Built by
  `ee_screenscpp_ga_record` and sent by `ee_screenscpp_send_local` (one C++ function for all).
- `ee_sim::world` (lane V's module): `GroupView` (a selected group's `mUnits`, `Contains`), `PlayerView::
  selected_group`, `StatsView::class`/`button_handle`; layout fields `group_units`, `stats_unit_class`,
  `stats_button`; the `test_layout` feature (a thread's test layout for the dependents' unit tests).

## Goldens and checks (round 1)

- Recorded on the unmodified tree (origin/rust 4929244f3, `build/ut1/pre*`): the tour with `--view-check --seed 7`
  (SGVW 79 distinct lines = `tools/sgvw/tour_view.txt`, `VIEW CHECK PASS`, F3 ok), the orders game with box
  select (`BOX SELECT PASS`, `ORDERS PASS`), the editor gate under SGVW (25,136 lines = `tools/sgvw/editor.txt`,
  PASS 14,448), the civilization gate (PASS 4,062), the pre-round test-profile exe for the pixel comparison.
- The views: a scratch shadow (`EE_UT1_SHADOW`, never committed) answered every moved read both ways in the game:
  orders game 21,401 checks, tour 15,601, 0 mismatches; the fills exercised every 16th frame (hot keys 148
  snapshots, diplomacy 1,258 players, status box 182 views, airport 3).
- The sends: a scratch in-game driver (`EE_UT1_SENDS`, never committed) built each moved message by the original
  statements and by the Rust over a grid of arguments (32- and 64-bit edge values) and compared their wire bytes,
  records and types; then every Rust member mutated.
- Pixels: the tour's and orders game's menu frames equal the pre-round exe's but the build label (171 pixels);
  in game the status box, the resource bar and the icon bar regions of the box-selected frame are equal (the map
  and the mini map differ between any two live games).

## Gates (round 1, on the result merged with origin/rust ec39733e7, merge 8c0b98ab4)

Both builds 0/0 (EE-AOC 689, EE-SIM 690 inputs); the test profile 0/0; the oracle seeds 2-7 identical to the
reference; the Rust gate PASS (4,531 tests; `ee_base` `fs::golden_spec::golden_spec_replays` failed once in the full
run under load and passed alone and in its crate's run); the editor gate PASS 14,448 with SGVW equal to
`tools/sgvw/editor.txt` (25,136 lines); the civilization gate PASS 4,062; the tour with `--view-check --seed 7`
(`RESULT PASS tour`, F3 ok, `VIEW CHECK PASS`, SGVW 79 distinct lines equal to `tools/sgvw/tour_view.txt`); the
orders game (`BOX SELECT PASS`, `ORDERS PASS`, `RESULT PASS random-game`); the scroll check (`SCROLL CHECK PASS`);
`gates.sh` GATES PASS (tour; ai-game time-cap, alive [2, 3], classic_lua x 2; lan-game). Per commit: build `--sim`
0/0, oracle seeds 2-3 identical, the Rust gate PASS (commits 1-3; commit 4: unit tests, the test profile and the
orders game, then the merged tree's gates). Not run: the sim_capture profiles (no simulation code changed). Main
merge 4 had not landed on origin/rust when the round ended.

## What only a person reaches

The sends (no fixture clicks a command button, the icon bar, the portrait, a diplomacy control or a hot key), the
hot keys' snapshot (checked only by the shadow's per-frame exercise), the cargo and garrison reads.

## Commits (round 1)

07cab23f2 (the views and the direct path: -224), 06c020000 (the record messages: -85), dda53fa2f (the main form's
record, the UI thread's manager calls: -65), 8c79e834b (the main group's record: -30): **net C++ -404**.

## Left

See PORT_LEARNINGS "The UI thread lane, round 1" ("Left"): the HUD's drawing with a Rust 2D draw list, the tech
tree's buttons and the civilization powers (the command panel's set-up), `EEScreens.cpp`'s game calls with their
owners, the remaining HUD controls' members as records.
