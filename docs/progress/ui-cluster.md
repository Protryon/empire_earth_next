# The UI cluster lane (the main game HUD, the menus, the UI events, the LLE UI's hosts)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The UI cluster lane, round 1" .. "round 5", "The UI lane, round 6", "The UI lane, round 7").

**Scope (the lane scopes of 2026-11-19).** `src/Empire Earth/User Interface Thread/*` but the editor-only
controls (`EESE*`, `EEUIFSE*`), `EEFOFindNewTargetNearOldTarget.cpp` (the units' lane) and the WON boxes
(`EEUICWonNet*`, `EEWonNetAvailableGameFormManager.cpp`: the WON lanes'); `src/Empire Earth/UI Events` but the WON
events (`EEUIEWonNet*`); `src/Low-Level Engine/User Interface`; `include/lle/UI*`; their `include/ee` headers
(`EEUI*`, `EEScreens*`, `EEUserInterface*`); the shells (`ee/EEUIShell.h`, `EEUIShell.cpp`, ee_game
`screens/shells*.rs`) and the `ee_ui`/`ee_game` screens.

## Round 7: the host cases' logic in Rust over one-statement C++ shims (branch `rust-ui7`, 2026-11-20)

The owner's decision of 2026-11-20 ("calling C++ from Rust is fine") lifted round 6's obstacle: a host case whose
callees are other lanes' C++ objects now moves to Rust with each call one C++ statement.

- **`game_cpp.rs`** (ee_game screens): the `shims!` macro. One list of `fn name(args) -> ret = symbol;` makes the
  trait `Cpp` (the moved logic is generic over it), its FFI implementation `Ffi` (the `extern "C"` block; absent
  from a test binary) and the recording `Fake` (every call logged as `name(args)`, answers, out numbers and out
  texts from per-shim queues). Argument types: numbers, pointers as `usize`, `EeScreensShimText` (bytes handed in),
  `EeScreensShimOut` (a `Vec<u8>` the C++ appends to through `ee_screens_shim_out`), out numbers
  (`EeScreensShimULongOut` for `unsigned long&`, `EeScreensShimLongOut` for `long&`, `EeScreensShimI32Out`,
  `EeScreensShimPtrOut` for an `FSFileSpec*&`). The C++ side's types and helpers: `ee/EEScreensShims.h`
  (`EEShimText`, `EEShimString`, `EEShimOut`).
- **The moved logic**, each module asked by its host's adapter before the C++ host (below the hosts' traits: the
  screens' replay goldens are unchanged), each with a test file over the fake:
  - `game_calls.rs`, `game_calls_more.rs`: `ee_screenscpp_call`'s cases (`menus_ffi.rs` `Form::op`);
  - `game_texts.rs`: `ee_screenscpp_text` and `ee_screenscpp_put_text` (`Form::get_checked`, `Form::put`);
  - `ui_calls.rs` (+ `ui_calls_ffi.rs`): the members `EEUserInterface::ChangeRasterizer`, `ResizeGameScreen`,
    `ShowMessageOfTheDay`, `OpenSelectServer`, `PauseGraphics`, `UpdateInterface` (their C++ bodies forward to
    `ee_screens_ui_*`), and `ee_screenscpp_ui_game` / `_ui_message` / `_ui_event` / `_action`'s cases;
  - `main_form_calls.rs`: `EEUIFMainGame::HostOp`'s and `EEMouseModeOp`'s cases (`main_form_ffi.rs`);
  - `shell_calls.rs`: `ee_screenscpp_shell`'s cases of the toolkit shells, the saved groups, the icon bar, the chat
    line and the paint areas (`shells_ffi.rs`; the editor-only kinds' ops untouched).
- **The shims**: 605 one-statement `extern "C"` functions (`ee_screenscpp_<name>`) in the host's own file
  (EEScreens.cpp 306, EEUserInterface.cpp 116, EEUIFMainGame.cpp 132, EEUIShell.cpp 51), each a call on another
  lane's object, no logic (two-way choices are two shims; the call order of every case is the C++ case's).
  The generator and its inputs were scratch (build/ui7/gen); a new shim is one line in a `shims!` list and one
  `extern "C"` function.

| Commit | What | Logic (lane files) |
|---|---|---|
| 56a9f7e56 | `ee_screenscpp_call`'s cases (game_calls.rs, game_cpp.rs) | EEScreens.cpp 2004 -> 1713 |
| 25d4044d1 | the screens' texts (game_texts.rs) | -> 1496 |
| e152dbfdb | more of the calls (game_calls_more.rs) | -> 1235 |
| cc8e670dc | the UI thread's members and hosts (ui_calls.rs) | EEUserInterface.cpp 447 -> 166 |
| 6dc3daa5c | the main game form and the mouse modes (main_form_calls.rs) | EEUIFMainGame.cpp 709 -> 431 |
| 9bf340d65 | the shells' host (shell_calls.rs) | EEUIShell.cpp 499 -> 408 |

The totals (`logic_lines.py` over the whole tree from base 840aeb3c0) are in PORT_LEARNINGS.md "The UI lane,
round 7".

## What is left (the lane's part)

- **Multi-statement cases still in C++**, by file (lines in cases of 3+ lines at the round's end): EEScreens.cpp
  (the campaign editor's model host `ee_screenscpp_ce_campaign`, the read views `ee_screenscpp_game_view`/
  `_game_texts`' field fills, the post-game records, the lobby's saved options, the questions' callback tables,
  the std::set/vector list cases), EEUIFMGStatusBox.cpp (157), EEUIManager.cpp (129), EEUIFError.cpp (111),
  EEUIBUpgrade.cpp (100), the LLE UI hosts (UIManager 96, UIForm 74, UIControlsTable 50, UIComboBox 52),
  EEUICCommandUnits (80), EEUIFMGStatusButton (62), EEUICPortrait (53), EEUIFMGMainGroup (52). Each goes the same
  way: its adapter asks a `*_calls.rs` first, the case's calls become shims in its own file.
- **One-statement cases** (`case X: call(); break;`, about a hundred in EEScreens.cpp alone) are shims already by
  the owner's definition; `logic_lines.py` counts their three lines as logic. Moving them moves the dispatch, not
  logic, so this round left them.
- **Cases left on purpose**: a printf-style format of a language string (`sprintf` of a resource format:
  CS_PROGRESS, TEXT_WONDER, APPEND_NUMBERED), the float-to-unsigned cast of a mixed ternary (CAM_MOUSE_VALUE), the
  copy-constructed options (LE_APPLY_SAVED), C++ containers built and walked in place (CAMPAIGN_FILES, ADAPTERS,
  CE_APPEND, PREVIEW_WALL).
- From round 6, still true: `EEUIRustControl`'s subclasses (the select/command units, portrait, mini map) own C++
  sub-objects; the improvement buttons have no shell base; `EEUISCIFormManager` is file and surface C++.

## Checks

- Per piece: the build `--sim` 0/0; the unit tests of the moved modules (`cargo test -p ee_game --lib screens::`);
  the oracle at seeds 2-3; a tour or the orders game where the moved code runs per frame.
- At the round's end: both builds and the test profile 0/0, `rust_build.py header` and the Rust gate, the oracle
  seeds 2-7, the twelve capture profiles at seeds 2-3, the plain saveload and the matrix with `--expect`,
  scenario_check, sim_editor and `--civbuilder`, the tours (`--exercise tour`, `editor`, `random-game --box-select
  --orders --seed 7`), `bash tools/gates.sh`.
