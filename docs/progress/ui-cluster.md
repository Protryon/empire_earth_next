# The UI cluster lane (the main game HUD, the UI events, the scenario editor, the civilization builder)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The UI cluster lane, round 1").

**Scope.** `src/Empire Earth/User Interface Thread/*` (but `EEFOFindNewTargetNearOldTarget.cpp`, the units
cluster's), `src/Empire Earth/UI Events`, `src/Empire Earth/Scenario Editor`, `Scenario Editor Actions`,
`Civilization Builder`, their `include/ee` headers and the matching Rust (`ee_ui`, `ee_game` screens). It takes
over the UI thread lane (`ui-thread.md`) and the HUD 2D lane (`hud-2d.md`).

## The measure (round 1, at origin/rust 104a6603d)

About 24k C++ lines in the lane's files. Every class's logic was already Rust (game screens 1-31, screens B, the
UI thread and HUD 2D lanes); what is left is host code: one C++ case per call the Rust makes into a C++ object.
The earlier rounds classed most cases as "a C++ object no Rust object stands for". That is no longer true for the
low-level engine: the toolkit (`UIForm`, `UIControl`, `UIButton`, `UITextLabel`, `UIManager`), `UWideString`,
`GEText`, `TMPoll`, the language strings, the keyboard and the sound manager are whole Rust objects behind C++
shells. A case that calls one of them can be answered by the same inward Rust call, and a class's members can be a
Rust record the class derives from, so that its own ops are field reads in Rust. What stays C++ is the calls into
the game's C++ (the world, the units, the picker, the ghosts, the communications, the tech tree's nodes and buttons,
the calamities, the media) and the toolkit's C++ helpers (`UIGetStringCutOffIndex`, `GERectangle::Contains`).

## What is in Rust (round 1)

- `ee_game screens/hud_direct.rs` (new), below the hosts' traits (every replay golden unchanged):
  - the main game form's host (`EEUIFMainGame::HostOp`): the held text and the chat line's strings (`TEXT_SET`,
    `TEXT_GET`, `TEXT_STRING` from the Rust language table, `CHAT_LENGTH`, `CHAT_REST`), the labels (`LABEL_*`,
    `STATUS_TEXT`), the error log's append, the dialog message's scrolling text, the form's and manager's calls
    (`BASE`, `BASE_COPY_DATA`, the mouse capture, dirty rectangles, entity groups, hot key groups, backgrounds,
    redraws, the visibility), the chat lines (`CHAT_DISPLAY`, `CHAT_COPY`, `CHAT_SHADOW`, on their records);
  - the HUD controls' base ops (`EEUIRustControl::HostOp`): dirty, display mode, opacity, the base `CopyData` and
    `DrawTextA`, the captures, the rectangles, the modifiers, the form's redraw and dirty rectangle;
  - the cursor's world reads (`ee_screenscpp_cursor`) on `ee_sim::world`'s views (25 of 41 ops);
  - the command buttons' ops (`EEStatusButtonOp`) on their record, their `UIButton` and the form's controls;
  - the main group's button record ops (counts, the tech, the bitmaps' copy);
  - the status box's host (`EEUIFMGStatusBox::HostOp`): its fields, its controls' display modes, dirt, labels,
    string ids, the formats and the cinematic;
  - the main game form's events' form record ops (`EEScreensMainGameCall`: the UI state, the cursor lock, the
    current group, the dialog test).
- Records the C++ classes derive from (`main_form_fields.rs`): `EeScreensStatusButtonFields` (the command
  buttons' plain members), `EeScreensStatusBoxFields` (the whole status box: its only base, its constructor and
  `GetBoxControl` in Rust), the chat line's record extended with its timer and texts (`chat_line.rs`; its host
  `ChatCpp` in `hud_ctl_ffi.rs` works the `TMPoll` and `GEText`s on their Rust objects).
- `ee_base::input::ffi::keyboard_modifiers()` (the system keyboard's modifiers for Rust callers).

## Checks (round 1)

- Pre-round test-profile exe and its harness runs kept in `build/uc1/pre` (`pre_tour`, `pre_orders`, with the 2D
  draw digest `EE_DRAW2D_DIGEST`); `build/uc1/digdiff.py` compares two digests.
- After each step: the fixture tour (`--input-check --view-check --seed 7`) or the orders game (`--box-select
  --orders --seed 7`) with the digest: every text command equal to the pre-round run's but the build label; the
  bitmaps differ only in fading alpha (and one timing bitmap); in the orders game the map's rhombi and lines differ
  between any two live games.
- Unit tests: `hud_direct_tests.rs` (the strings, `atoi`, the held text, the chat rest, the records' offsets and
  slots, the status box's fields and formats, the null paths).

## Gates (round 1)

On the result merged with origin/rust 8d68d2786 (merge 41dd3c702: clean; strip_markers, `rust_build.py header`
and `rust_includes.py` changed nothing): both builds 0/0 (EE-AOC 625, EE-SIM 626 inputs) and the test profile 0/0;
the Rust gate PASS (4,644 tests, clippy, rustfmt, headers); the oracle seeds 2-7 identical to the reference; the
editor gate PASS 14,448; the civilization gate PASS 4,062; scenario_check 27/27; the tour (`RESULT PASS tour`,
`VIEW CHECK PASS`, F3 ok); the orders game with box select and the scroll check (`BOX SELECT PASS`, `ORDERS PASS`,
`SCROLL CHECK PASS`, `RESULT PASS random-game`); `gates.sh` GATES PASS (tour; ai-game time-cap, units [4, 41, 40],
alive [2, 3], classic_lua x 2; lan-game). Before the merge: build `--sim` 0/0, the Rust gate PASS (4,625 tests), the
oracle seeds 2-3 identical, a tour or orders game per step. Not run: the sim_capture profiles (no simulation code
changed: every move is UI-thread code EE-SIM never builds).

## Commits (round 1)

0d39710e1 (`ee_base` keyboard modifiers: 0), 44c415f70 (the HUD hosts and records: +54 -407, **net -353**).

## Behaviour changes

- The command buttons' form ops now go through the form's Rust adapter (`main_form_ffi::op`): since the UI thread
  lane's round 1 the market's trade buttons read the gold and resources through a C++ case that had been deleted
  (always 0: no trade could be afforded) and the cursor lock of a press set nothing. Both work again.
- The status box's members start zeroed (`mC98` was left unset by the original's constructor).

## Left

- The remaining main form host cases call the game's C++ (the picker, ghosts, walls and gates, the communications,
  the calamities, the media, the tech tree's nodes, the scenario's dialog message) or `gMouse`, `gViewport`,
  `gUIManager->mMainGameForm` (the `EEUIManager` member: a Rust record of the manager would answer those).
- The portrait's, icon bar's, mini map's, saved group's, upgrade button's and error form's `RustOp`s: their members
  as records and their `GEText`/`GEBitmap` ops on the Rust objects, as the chat line's.
- `EEScreens.cpp`'s hosts (sessions, files, campaigns, preferences), the scenario editor's control factories
  (`SeplMake`, the effect and condition screens' slot ops) and the civilization builder's.
- The WON boxes and events (`EEUICWonNet*`, `EEUIEWonNet*`, `EEWonNetAvailableGameFormManager.cpp`, ~1.6k lines of
  plain C++ logic on `gWonNet`): to be ported with golden drivers (no WON server reaches them).
