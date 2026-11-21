# The UI cluster lane (the main game HUD, the menus, the UI events, the LLE UI's hosts)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The UI cluster lane, round 1" .. "round 5", "The UI lane, round 6" .. "round 10").

**Scope (the lane scopes of 2026-11-19).** `src/Empire Earth/User Interface Thread/*` but the editor-only
controls (`EESE*`, `EEUIFSE*`), `EEFOFindNewTargetNearOldTarget.cpp` (the units' lane) and the WON boxes
(`EEUICWonNet*`, `EEWonNetAvailableGameFormManager.cpp`: the WON lanes'); `src/Empire Earth/UI Events` but the WON
events (`EEUIEWonNet*`); `src/Low-Level Engine/User Interface`; `include/lle/UI*`; their `include/ee` headers
(`EEUI*`, `EEScreens*`, `EEUserInterface*`); the shells (`ee/EEUIShell.h`, `EEUIShell.cpp`, ee_game
`screens/shells*.rs`) and the `ee_ui`/`ee_game` screens.

## The method (rounds 7 to 10): C++ logic in Rust over one-statement C++ shims

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
- **The toolkit's own list (round 10)**: `ee_ui` `lle_cpp.rs` is the same macro for the LLE toolkit (`ee_uicpp_*`
  shims in `src/Low-Level Engine/User Interface`, `EeUiShim*` types, C++ helpers `lle/UIShims.h`), gated
  `not(any(test, feature = "no_cpp"))`. The moved host cases plug in as Rust hosts: the class hands Rust
  `ee_ui_<class>_host` (`lle_host!`), which answers the moved ops ([`HostArgs`]) and passes the rest to the C++
  `HostOp` through one statement (`ee_uicpp_<class>_host`); the goldens build their objects over their own hosts.
- **A shim's callee that is Rust** (the sweep): the shim list entry names the Rust symbol itself (`fn world_pause(..)
  = ee_gamecpp_world_pause;`) and the C++ shim goes; a composition of Rust calls (`EEUISCIFormManager::Get()->X()`)
  is written in Rust.
- The generator and the case cutter were scratch (build/ui8/gen, build/ui9/gen: `gen.py` spec lines `name(arg: T) [-> R] | C++
  statement` -> the Rust list entry and the `extern "C"` one-liner; `cut.py` deletes cases by label).

## Round 10 (branch `rust-ui10`, 2026-11-20)

| Module | What moved (C++ deleted or now one forward) |
|---|---|
| ee_ui `lle_manager.rs` | `UIManager::HostOp`'s multi-call cases (window, blending, the UI texture's load, bitmaps, locks, events, callbacks, sound instances), `LoadTexture`, `LoadDatabase`, `RemoveDatabase`, `SetMouseLeftHanded`, the constructor's mouse buttons; `UIEvent::HostOp`'s file count, owner, add |
| ee_ui `lle_form.rs` | `UIForm::HostOp`'s multi-call cases (help string, hot keys' events, keys, events, mouse asks, lock, bitmap draws), `UIForm::Create`, the IME and typed-key passes, the hot key and layout packs |
| ee_ui `lle_control.rs` | `UIControl::HostOp`'s event case, the 24 shared keys, the mouse dispatch's calls (`ee_uicpp_control_mouse` deleted; `mouse_ffi.rs` calls the shims), the manager bitmap table |
| ee_ui `lle_table.rs` | `UIControlsTable`'s host's multi-call cases, `CreateBitmap`, `SetupStringArray` |
| ee_ui `lle_combo.rs` | the combo box's host's queries and actions of more than one call, its constructor's part making |
| ee_game `unit_hack_calls.rs`, `cursor_calls.rs`, `dialog_calls.rs` | `ee_screenscpp_unit_hack`'s, `ee_screenscpp_cursor`'s and `ee_screenscpp_dialog`'s multi-statement cases |
| ee_game `chat_log_ffi.rs`, `screens_more_calls.rs` | `ee_screenscpp_cl_read`, `_cl_last`, `_cl_write`, `_hot_key_target`, `_hot_key_keep`, `_selection` (deleted) |

The shim sweep: ten shims deleted (`sc_pause_game`, `tt_engine_ai`, `rc_left_handed`, `rc_delete_editor`,
`rc_create_editor` by their Rust symbols; `sc_pgs_sci_delete`, `rc_si_blink_off`, `rc_si_open_map`,
`rc_si_open_sound`, `intro_set_blinking` composed in Rust). The editor tour has a campaign editor step
(`fixture_game.py --exercise editor`, the fixture's `editnum`). The totals are in PORT_LEARNINGS.md "The UI lane,
round 10".

## What is left (the lane's part)

Logic lines per file at round 10's end (`logic_lines.py`'s heuristic): EEScreens.cpp 202 (the settings record's
X-macro list, `modes`, `current_mode`, `game_size`, the lobby's option setter and rules, the statics), EEUIFMainGame.cpp
177 (the destructor, `CreateGroup`, `IsInGameView`, the 32 default player names, the cursor read's set-up),
EEUIShell.cpp 166 (the one-line cases, the `BaseCall` members, `DrawBars`, the icon bar's and chat line's
constructors), EEUICCommandUnits 96, EEUICPortrait 95, EEUIBUpgrade 88, EEUserInterface 83, UIControl 82 (the 24
key statics, the key helpers), EEUIManager 75, UIBackground 75, UIForm 55 (the db constructor's lower-casing, the
temp hot key, the statics), EEUIFMGStatusBox 55, UIFileListBox 54, UIEditBox 53, UIManager 52 (the nested form
switches, the statics, the forwarders' frame), EEUIFMGStatusButton 50, UILBString 45, EEUIFError 40, EEUIFMGMainGroup
35, UIControlsTable 28, UIComboBox 20. The `include/lle/UI*` headers are declarations: they go only with a whole class.
- **Shims whose callee is now Rust**: swept once (ten). Most of the rest pass an object (a C++ shell's `mRust` or
  `this`) that only the C++ reaches; `gWorld.ClearGhostUnits()` and the others on `&gWorld` would need the world's
  address as a shim of its own.
- Left on purpose (as rounds 7-9): the printfs of a resource format, qualified base-class calls, the nested
  one-statement switches of `UIManager::HostOp` (the form's protected members: the manager is the form's friend).

## Checks

- Per piece: the build `--sim` 0/0; the moved modules' unit tests (`cargo test -p ee_game --lib screens::`).
- At the round's end: both builds and the test profile 0/0, `rust_build.py header` and the Rust gate, the oracle
  seeds 2-7, the twelve capture profiles at seeds 2-3, the plain saveload and the matrix with `--expect`,
  scenario_check, sim_editor and `--civbuilder`, the tours (`--exercise tour`, `editor` with its campaign editor
  step, `random-game --box-select --orders --seed 7`) with their captures against the base build's, `bash
  tools/gates.sh`. The LLE toolkit's moves are exercised by every tour (the menus are its forms).
