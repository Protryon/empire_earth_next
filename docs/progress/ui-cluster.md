# The UI cluster lane (the main game HUD, the UI events, the scenario editor, the civilization builder)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The UI cluster lane, round 1", "The UI cluster lane, round 2").

**Scope.** `src/Empire Earth/User Interface Thread/*` (but `EEFOFindNewTargetNearOldTarget.cpp`, the units
cluster's), `src/Empire Earth/UI Events`, `src/Empire Earth/Scenario Editor`, `Scenario Editor Actions`,
`Civilization Builder`, their `include/ee` headers and the matching Rust (`ee_ui`, `ee_game` screens).

## The measure (round 2, at origin/rust 2bccb3e94)

About 37k C++ lines in the lane's files (24k in the UI thread, the UI events and the civilization builder, 13k in
the scenario editor). Every class's logic that is UI logic is Rust; what is left is of three kinds:

- **Host cases** (about 1,000): one C++ `case` per call the Rust makes into a C++ object. The ones that call the
  toolkit, the language strings, the sound manager, the thread system or the two UI objects are answered in Rust
  now (round 1 and this round); the rest call the game's C++ (the world's objects beyond the views, the picker,
  ghosts, walls and gates, the communications, the calamities, the media, the scenario manager, gMouse, gViewport,
  the graphics thread).
- **Class shells**: each game form and control is a C++ subclass of a toolkit class (`UIForm`, `UIControl`,
  `UIButton`, ...) for its virtual overrides (the toolkit's C++ calls them), its constructor (which makes its C++
  sub-objects) and its members' C++ objects.
- **C++ logic not yet ported** (about 9k lines in functions that call no Rust): the paint areas
  (`EEUIFSEPaint`, `EEUIFSEPaintElevation`), the tech list boxes (`EESEUITechListBox`: sorts with MSVC's
  `std::sort` order), the saved group buttons (`EEUIBSavedGroup`), the map backgrounds (`EEUIBGenericBackground`),
  the status bar (`EEUIFMGStatusBar`), the upgrade panel, the scenario editor's control factories (`SeplMake`, the
  effect screen's slots), the civilization builder's area effects (`EEAEManager`).

## What is in Rust (rounds 1 and 2)

- `ee_game screens/ui_objects.rs` (round 2): `EEUserInterface` is the record `EeScreensUserInterfaceFields`
  alone (no vtable): its constructor, destructor, every post, `PostStartLoading`, `PostMessage424Once`,
  `SetLoadProgress`; `EEUIManager`'s own members are the record `EeScreensUiManagerFields` (registered by its
  constructor; `gUIManager->GetMainGameForm()` / `GetScenarioEditorForm()` read them typed).
- `ui_objects_ffi.rs`, `ui_direct.rs` (round 2): the UI hosts' cases on the two records, the UI thread's queue,
  the manager's forms, the toolkit, the sound manager, the language strings and the posts:
  `ee_screenscpp_ui_game` (13), `_ui_message` (21), `_ui_event` (6), `_ui_manager` (FORMS), `_main_form` (11),
  `EEScreens.cpp` (19); `ShowEpochName` and `ShowLoadingText` whole.
- The editor's Undo and Redo actions are `EEARust` kinds (`game_actions.rs` 27, 28); the trigger screen's combo
  boxes' right-click jump is `editor_trigger.rs` `trigger_jump`.
- `ee_game screens/hud_direct.rs` (round 1): the main game form's host (labels, held text, strings, the dialog
  message, the form's and manager's calls, the chat lines), the HUD controls' base ops, the cursor's reads, the
  command buttons', the main group's and the status box's ops; their members as records (`main_form_fields.rs`).

## Checks

- The round's start test-profile exe and its harness runs (`build/uc2/pre`: the tour `--input-check --view-check
  --seed 7`, the orders game `--box-select --orders --scroll-check --seed 7`, the editor exercise, each with the 2D
  draw digest `EE_DRAW2D_DIGEST`); `build/uc2/masked.py A B` lists the commands only one digest has with the
  vertex alpha masked (fades).
- After each step: the build `--sim`, the Rust gate, the oracle seeds 2-3, the test-profile build, the three
  harness runs and their digests against the start's (`build/uc2/step.sh TAG`).

## Gates (round 2)

On the round's final tree (origin/rust 2bccb3e94 merged: already up to date; strip_markers removed two trailing
address comments, committed; `rust_build.py header` and `rust_includes.py` changed nothing): both builds `--sim` 0/0
(EE-AOC 623, EE-SIM 624 inputs) and the test profile 0/0; the oracle seeds 2-7 identical to the reference; the
editor gate PASS 14,448; scenario_check 27/27; the tour (`RESULT PASS tour`, `VIEW CHECK PASS`, F3 ok), the orders
game (`BOX SELECT PASS`, `ORDERS PASS`, `SCROLL CHECK PASS`, `RESULT PASS random-game`), the editor exercise (`RESULT
PASS editor`), each digest against the round's start's: the build label, the loading screen's texts at other
moments, a resource count; `gates.sh` GATES PASS (tour; ai-game time-cap, units [4, 43, 43], alive [2, 3],
classic_lua x 2; lan-game). The civilization gate failed there once: it prints the controls' classes, and the message
box's buttons had become one class; the second keeps its name now (`EEUIFEBCancel : EEUIFEBOK`), and on that tree
the builds 0/0, the civilization gate PASS 4,062, the editor gate PASS 14,448, the Rust gate PASS (4,651 tests, clippy, rustfmt, headers). The Rust
gate's first run on the final tree failed in `ee_base fs::golden_spec_replays` (an `rmdir` the system refused once:
not this lane's code; it passed on the next runs). Per step: the build, the Rust gate (4,649..4,651 tests), the
oracle seeds 2-3 identical, the three harness runs with digests. Not run: the sim_capture profiles (no simulation
code changed: every move is UI-thread code EE-SIM never builds; the oracle confirms).

## Commits (round 2)

565f42ac3 (0), 9b00fdd04 (-501), 07dbd18fe (-64), 7d6c9c414 (-136), 169f3f5c2 (-59), fe8fc173f (-153), 895575c61
(-18), 6b9f63d8a (-72), 7d4aa977d (-27), 90d2508a9 (0), and the two fix-ups (the button's class name, rustfmt):
**net -1,024** (round 1: -353).

## Behaviour changes

- None intended. `gUnitSelected` is a Rust static; the message box's OK and No/Cancel are one class (the OK one's
  close form is NULL, as its own was); the Undo and Redo buttons post `EEARust` actions that run the same undo and
  redo lists.

## Left

- The forms and controls as classes: a whole-class delete needs a generic C++ shell per toolkit base (a
  `UIForm`, a `UIControl` and a `UIButton` subclass whose virtuals call one Rust dispatcher by kind) and the
  classes' C++ sub-objects (`GEText`, `GEBitmap`, the toolkit controls they make) made by Rust or by one factory
  host. `EEUIRustControl` is half of that (its host; its subclasses still declare their overrides).
- The host cases that call the game's C++ (above): each needs that object's Rust (the units, graphics, app and
  calamity lanes' work) or one narrow host function.
- The C++ logic above; the WON boxes and events (`EEUICWonNet*`, `EEUIEWonNet*`,
  `EEWonNetAvailableGameFormManager.cpp`), to be ported with golden drivers.
