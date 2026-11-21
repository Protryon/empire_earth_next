# The UI cluster lane (the main game HUD, the menus, the UI events, the LLE UI's hosts)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The UI cluster lane, round 1" .. "round 5", "The UI lane, round 6" .. "round 9").

**Scope (the lane scopes of 2026-11-19).** `src/Empire Earth/User Interface Thread/*` but the editor-only
controls (`EESE*`, `EEUIFSE*`), `EEFOFindNewTargetNearOldTarget.cpp` (the units' lane) and the WON boxes
(`EEUICWonNet*`, `EEWonNetAvailableGameFormManager.cpp`: the WON lanes'); `src/Empire Earth/UI Events` but the WON
events (`EEUIEWonNet*`); `src/Low-Level Engine/User Interface`; `include/lle/UI*`; their `include/ee` headers
(`EEUI*`, `EEScreens*`, `EEUserInterface*`); the shells (`ee/EEUIShell.h`, `EEUIShell.cpp`, ee_game
`screens/shells*.rs`) and the `ee_ui`/`ee_game` screens.

## The method (rounds 7 to 9): C++ logic in Rust over one-statement C++ shims

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
  host; a C++ member's body is one forward to an `ee_screens_*` Rust function. When every case of a host is in
  Rust the C++ host goes and the adapter answers 0 for an op that is none of them (round 9: `ee_screenscpp_call`,
  `_text`, `_put_text`, `_main_form`).
- Private members of another lane's class (`EESMMediaStrings`' strings) are read through its public accessors
  (`GetSection`) or a one-statement accessor in the lane's own class (`EEUISCIFormManager::MapFileName`): a free
  shim is no friend.
- The generator and the case cutter were scratch (build/ui8/gen, build/ui9/gen: `gen.py` spec lines `name(arg: T) [-> R] | C++
  statement` -> the Rust list entry and the `extern "C"` one-liner; `cut.py` deletes cases by label).

## Round 9 (branch `rust-ui9`, 2026-11-20)

| Module (ee_game screens) | What moved (C++ deleted or now one forward) |
|---|---|
| `lobby_calls.rs` | `EEScreensLobbyOptions`, `EEScreensLobbySlot`, `EEScreensLobbyLoad`; `ee_screenscpp_lobby_player`, `_put_lobby_player`, `_lobby_options`, `_lobby_fill` (deleted: the records are built from field shims); `EE_SCREENS_GAME_LE_APPLY_SAVED`, `_BOOT_FREE`, `TEXT_LE_CHECK_LOAD`, `TEXT_LE_MAP_DISPLAY` |
| `post_game_calls.rs` | `ee_screenscpp_pgs_history`, `_pgs_record` (deleted), `PGS_ADD_SERIES`, `PGS_WINDOW` |
| `screens_calls.rs` | the dispatcher chain under `ee_screenscpp_call` whole (`EEScreensHotKeyCall`, `_ChatCall`, `_MainGameCall`, `_MouseEntity`, `_CameraCall`, `_DiplomacyCall`, `_PostGameCall`, `_SplashCall`, `_CivChoiceCall`, `_DebugSoundCall`, `_CampaignEditorCall`) and `ee_screenscpp_call` itself (deleted) |
| `texts_calls.rs` | `ee_screenscpp_text` and `ee_screenscpp_put_text` whole (deleted), `Parts`, `Float`, `Bits` |
| `main_form_calls.rs` | the rest of `EEUIFMainGame::HostOp` and the mouse modes' `EEMouseModeOp` (both deleted, with `ee_screenscpp_main_form`) |
| `shell_calls.rs` | `ee_screenscpp_shell`'s 31 multi-statement cases, `EEShellTree`, `EEShellRotate` (deleted) |
| `sci_calls.rs` | `EEUISCIFormManager`'s `Clear`, `DeleteObjects`, `Get`, `OpenMap`, `OpenScenario`, `OpenSound`, `GetMap`, `GetScenario`, `GetSound`, `HasMedia`, `SetBlinking` (each one forward) |
| `upgrade_calls.rs` | `EEUIBUpgrade::HostOp`'s 14 multi-statement cases |

The totals are in PORT_LEARNINGS.md "The UI lane, round 9".

## What is left (the lane's part)

Logic lines per file at the round's end (`logic_lines.py`'s heuristic): the LLE UI (UIManager 272, UIForm 222,
UIControl 156, UIControlsTable 153, UIComboBox 123, UIBackground 75, UIFileListBox 54, UIEditBox 53, UILBString 45;
`include/lle/UI*` headers: their declarations, gone only with a whole class) - the next big piece, and it needs a
shims list in `ee_ui` (its objects call `Host::op_f`; the moved logic would plug into the object's `op` before the C++
host, out of the goldens' test builds); EEUIFMainGame.cpp 268 (the unit hack host, the cursor host, the destructor,
`CreateGroup`, `IsInGameView`, the 32 default player names), EEScreens.cpp 256 (the settings record's X-macro list,
`modes`, `selection`, the chat log's reads and writes, the hot keys' target, the callbacks, the statics), EEUIShell.cpp
166 (the one-line cases, the `BaseCall` members, `DrawBars`, the icon bar's and chat line's constructors),
EEUIFError 126 (the dialog host), EEUICCommandUnits 96, EEUICPortrait 95, EEUIBUpgrade 88, EEUserInterface 83,
EEUIManager 75, EEUIFMGStatusBox 55, EEUIFMGStatusButton 50, EEUIFMGMainGroup 35.
- **Shims whose callee is now Rust**: not swept this round. Two moved cases call Rust directly instead of a shim
  (the loading screen's decrypt, `ee_ue_decrypt_ustring`; the map tab's `ee_screens_segr_tab`/`_semp_call`).
- Left on purpose (as rounds 7, 8): the printfs of a resource format (one `sprintf` shim each), qualified base-class
  calls (`UIButton::MouseLButtonDown`, `UIControl::GetHelpString`: they stay in the member hosts).

## Checks

- Per piece: the build `--sim` 0/0; the moved modules' unit tests (`cargo test -p ee_game --lib screens::`).
- At the round's end: both builds and the test profile 0/0, `rust_build.py header` and the Rust gate, the oracle
  seeds 2-7, the twelve capture profiles at seeds 2-3, the plain saveload and the matrix with `--expect`,
  scenario_check, sim_editor and `--civbuilder`, the tours (`--exercise tour`, `editor`, `random-game --box-select
  --orders --seed 7`) with their captures against the base build's, `bash tools/gates.sh`.
