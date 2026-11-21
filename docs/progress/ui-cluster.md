# The UI cluster lane (the main game HUD, the menus, the UI events, the LLE UI's hosts)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The UI cluster lane, round 1" .. "round 5", "The UI lane, round 6" .. "round 8").

**Scope (the lane scopes of 2026-11-19).** `src/Empire Earth/User Interface Thread/*` but the editor-only
controls (`EESE*`, `EEUIFSE*`), `EEFOFindNewTargetNearOldTarget.cpp` (the units' lane) and the WON boxes
(`EEUICWonNet*`, `EEWonNetAvailableGameFormManager.cpp`: the WON lanes'); `src/Empire Earth/UI Events` but the WON
events (`EEUIEWonNet*`); `src/Low-Level Engine/User Interface`; `include/lle/UI*`; their `include/ee` headers
(`EEUI*`, `EEScreens*`, `EEUserInterface*`); the shells (`ee/EEUIShell.h`, `EEUIShell.cpp`, ee_game
`screens/shells*.rs`) and the `ee_ui`/`ee_game` screens.

## The method (rounds 7 and 8): C++ logic in Rust over one-statement C++ shims

- **`game_cpp.rs`** (ee_game screens): the `shims!` macro. One list of `fn name(args) -> ret = symbol;` makes the
  trait `Cpp` (the moved logic is generic over it), its FFI implementation `Ffi` (the `extern "C"` block; absent
  from a test binary) and the recording `Fake` (every call logged as `name(args)`, answers, out numbers and out
  texts from per-shim queues). Argument types: numbers, pointers as `usize`, `EeScreensShimText` (bytes handed in),
  `EeScreensShimOut` (a `Vec<u8>` the C++ appends to through `ee_screens_shim_out`), out numbers. The C++ side's
  helpers: `ee/EEScreensShims.h` (`EEShimText`, `EEShimString`, `EEShimOut`).
- **A shim** (`ee_screenscpp_<name>`) is one C++ statement in the host's own file, no choice: a two-way case is two
  shims and the `if` in Rust; a local the C++ built or wrote is a file static reset by a shim where the local was
  made; an array of members is a member-pointer table (`kMGBoxSlots`, `kMMLines`, `kSBAValues`). A protected member
  a shim needs became public in the class (`EEUIMiniMap`, `EEUICCommandUnits`) or a `using` (the base's
  `CaptureMouse`, `GetMouseLocation`). Pointers cross as the class C++ hands on: a control as `UIControl*`, an
  event as `UIEvent*` (`static_cast` in the shim, never a raw derived pointer).
- **Where the logic plugs in**: a host's Rust adapter asks the `*_calls.rs` module before (or instead of) the C++
  host; a C++ member's body is one forward to an `ee_screens_*` Rust function.
- The generator and the case cutter were scratch (build/ui8/gen: `gen.py` spec lines `name(arg: T) [-> R] | C++
  statement` -> the Rust list entry and the `extern "C"` one-liner; `cut.py` deletes cases by label).

## Round 8 (branch `rust-ui8`, 2026-11-20)

| Module (ee_game screens) | What moved (C++ deleted or now one forward) |
|---|---|
| `ui_manager_calls.rs` | `EEUIManager::HostOp`'s multi-call cases; `EECreateGameEvent`, `EECreateFormEvent`, `EECreateWonNetServerSelectEvent`; `CreateForm`, `CreateUserDefinedBackground`, `CreateUserDefinedControl`, `Create/DeleteScenarioEditor` |
| `status_box_calls.rs` | `EEUIFMGStatusBox::HostOp` whole (deleted with `ee_screenscpp_status_box`), `UnloadIcons`, `ReloadIcons`; `EEUIFMGStatusBar::SetIconBitmap` (deleted) |
| `status_button_calls.rs` | `EEStatusButtonOp` whole (deleted); the command button's constructor body, destructor, `ReleaseButton`, `DrawPolygons`, `DrawTextA`, `HotKeyUp`, `SetCommandButton`, `SetTechButton`, `SetLocation`; `EEUIManager::Fn26FA55/6A` |
| `main_group_calls.rs` | `ee_screenscpp_main_group` whole (deleted), `ShowCommands`, `EEMainGroupHotKey` (deleted), `EEUIEMGHotKeyStatusButton::ProcessEvent` |
| `mini_map_calls.rs` | `EEUIMiniMap`'s constructor body, destructor, `CopyData`, `LoadMedia`, `UnloadMedia`, `DrawPolygons`, `MouseRButtonDown`, `CenterCamera`, `SetLocation`, `Update`, `RustOp`'s multi-call cases |
| `backgrounds_calls.rs` | `EEUIBGenericBackground`'s constructor body, `Open`, the frame host (`ee_screenscpp_frame_fill`, `_piece` deleted: the fill calls ee_render's `ee_ge_draw2d_fill` directly); `EEUIBMapBackground::LoadBackground` |
| `campaign_model_calls.rs` | `ee_screenscpp_ce_campaign`, `_ce_campaign_text` whole (deleted) |
| `game_view_calls.rs` | `ee_screenscpp_game_view`, `_game_texts` whole (deleted) |
| `status_bar_calls.rs` | `EEUIFMGStatusBar`'s constructor layout, destructor, `CopyData`, `DrawPolygons`, `DrawTextA` |
| `command_units_calls.rs` | `EEUICCommandUnits`' constructor (the arrow model and texture) and destructor |

Each module has its unit tests over the fake (`*_calls_tests.rs` or an inline `tests`). The totals are in
PORT_LEARNINGS.md "The UI lane, round 8".

## What is left (the lane's part)

Logic lines per file at the round's end (`logic_lines.py`'s heuristic): EEScreens.cpp 683 (the big hosts
`ee_screenscpp_call`/`_text`/`_put_text`, mostly one-statement cases now; the post-game records, the lobby's saved
options and slots, the question callbacks, the camera's and the main game form's calls), EEUIFMainGame.cpp 359,
EEUIShell.cpp 311, the LLE UI (UIManager 272, UIForm 222, UIControl 156, UIControlsTable 153, UIComboBox 123,
UIBackground 75, UIFileListBox 54, UIEditBox 53, UILBString 45), EEUIBUpgrade 141 (its `HostOp`: the status box's
pattern applies), EEUIFError 126, EEUIEScenarioIntroEvents 110, EEUICCommandUnits 96 (`RustOp`'s arrow draw and
ground solve: float math, port with care), EEUICPortrait 95, EEUserInterface 83, EEUIManager 75, EEUIFMGStatusBox
55, EEUIFMGStatusButton 50, EEUIFMGMainGroup 35 (the panel groups' printf of a resource format).
- **Shims whose callee is now Rust**: not swept this round (the brief's other half) but the frame fill, which
  calls `ee_ge_draw2d_fill` from Rust now.
- Left on purpose (as round 7): a printf of a resource format (kept as one `sprintf` shim where moved: the campaign
  check's message), the float-to-unsigned cast of a mixed ternary, C++ containers walked in place where the walk is
  the case (round 8 snapshots a `std::set` into a vector shim instead).

## Checks

- Per piece: the build `--sim` 0/0; the moved modules' unit tests (`cargo test -p ee_game --lib screens::`); the
  oracle at seeds 2-3.
- At the round's end: both builds and the test profile 0/0, `rust_build.py header` and the Rust gate, the oracle
  seeds 2-7, the twelve capture profiles at seeds 2-3, the plain saveload and the matrix with `--expect`,
  scenario_check, sim_editor and `--civbuilder`, the tours (`--exercise tour`, `editor`, `random-game --box-select
  --orders --seed 7`) with their captures against the base build's, `bash tools/gates.sh`.
