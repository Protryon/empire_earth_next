# The UI cluster lane (the main game HUD, the menus, the UI events, the LLE UI's hosts)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The UI cluster lane, round 1" .. "round 5", "The UI lane, round 6" .. "round 15").

**Scope (the lane scopes of 2026-11-19).** `src/Empire Earth/User Interface Thread/*` but the editor-only
controls (`EESE*`, `EEUIFSE*`), `EEFOFindNewTargetNearOldTarget.cpp` (the units' lane) and the WON boxes
(`EEUICWonNet*`, `EEWonNetAvailableGameFormManager.cpp`: the WON lanes'); `src/Empire Earth/UI Events` but the WON
events (`EEUIEWonNet*`); `src/Low-Level Engine/User Interface`; `include/lle/UI*`; their `include/ee` headers
(`EEUI*`, `EEScreens*`, `EEUserInterface*`); the shells (`ee/EEUIShell.h`, `EEUIShell.cpp`, ee_game
`screens/shells*.rs`) and the `ee_ui`/`ee_game` screens. From round 11 also `src/Empire Earth/Civilization Builder`
(the builder, the civilization manager and files, the area effects; the brief's).

## The method (rounds 7 to 13): C++ logic in Rust over one-statement C++ shims

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
- **A simulation host** (round 11: `EECivHostCall`, `EECivFileCall`, `EEAEHost::Call`) stays the function pointer
  the simulation's Rust is handed; its body is one statement passing the call to an `ee_screens_*` Rust host whose
  cases call the shims. A class's own op host (`RustOp`) is deleted and its Rust adapter answers the ops first.
- **A class's objects whole** (round 12: the area effects): every member a one-statement forward to an
  `ee_screens_*` export taking the object's address; the Rust reads the object's numbers through the shims' base
  pointers (`static_cast<EeAreaEffectFields*>`) and copies them per call; a host struct the simulation's Rust was
  handed (`EEAEHost`) goes once its caller is Rust (a Rust type implements the simulation's host trait). C++ walkers
  that take functor objects keep one functor class whose `Apply` forwards the pass (`EEAEPassFunctor`).
- The generator and the case cutter were scratch (build/ui8/gen, build/ui9/gen, build/ui11, build/ui12: `gen.py`
  spec lines `name(arg: T) [-> R] | C++ statement` -> the Rust list entry and the `extern "C"` one-liner).

## Round 13 (branches `rust-ui13a` toolkit, `rust-ui13b` game, merged on `rust-ui13`, 2026-11-22)

The theme: the toolkit's and the game's class declarations, which rounds 10-12 could not shrink. Plan:
build/ui13/plan.md (the classes in dependency order, the fields other code reads, the call records, the statics).

| Piece | What moved |
|---|---|
| inline members (both) | about 450 toolkit and 208 game one-statement out-of-line members are inline forwards in their classes; their `.cpp` definitions and the hosts' call helpers they used are gone |
| ee_ui `ids.rs` | the toolkit's enums (UIControl's trigger ids, types, display modes; UIControlsTable's db control types; UIForm's event types; UILayoutType, UITiling, UICheckState) as Rust `#[repr(u32)]` enums, `/// cbindgen:enum-class=false` (cbindgen emits an unscoped `enum X : uint32_t`, the C++ uses them unchanged) |
| ee_ui keys | UIControl's 24 key statics, UIForm's and UIManager's key and mouse statics: one Rust record read through `UIControl::Key(EeUiKey::...)`; the key predicates (`ShiftKeyPressed`...) in Rust |
| ee_ui records | `UILayoutUtility` over `EeUiLayout`; `UIDbDataForms` is the Rust `DbForm` (cbindgen rename; tools/eedata overlay.py and specs.json point at it) |
| UIListBox | the subclass accessors and lines public, `UILBString::GetCopiedGEText` (a host in another file needs no `using`) |
| ee_game `ui_ids.rs` | EEUIManager.h's form ids and control-index enums (Rust constants that duplicated them use them) |
| ee_game `hud_more.rs` | the attribute and status buttons' bodies, EEUserInterface's posts, the main game form's `Load`, `Save`, `SetErrorString` |
| dead code | unused members and forward declarations of the game UI's headers; `EEAreaEffectData` is a typedef of the Rust `EeAreaEffectData` |

The totals are in PORT_LEARNINGS.md "The UI lane, round 13".

## Round 14 (branches `rust-ui14a` toolkit, `rust-ui14b` game, merged on `rust-ui14`, 2026-11-22)

| Piece | What moved |
|---|---|
| ee_ui `lle_hosts.rs`, `lle_hosts_tables.rs`, `lle_hosts_shims.rs` | the toolkit's host switches (UIControl, UIObjHost, UIForm, the backgrounds, the list boxes, the file lists, the hot keys, the font and background tables) as Rust tables over one-statement `EE_UI_CASE`/`EE_UI_VOID` shims (lle/UIShims.h); the toolkit classes' members public |
| toolkit records | the float buffers one `UIF4`; UIControl's construction and its border colours Rust's; the UI database's file names (`DB_NAMES`) Rust's; UIListBox's constructor and destructor inline |
| ee_game `EEUIShellOf<B, D>` | the seven shell classes one template over the toolkit base: every override one statement, BaseCall's common cases once, the kind's extras `D::Extra`; `Make`, `Op` (by address) and the base-call routing in Rust (shells_ffi.rs); `EEShellText` a shell virtual |
| ee_game screens | the settings record's copy (settings_form_ffi.rs over `EeAppSettingsFields`), the question answers and their tables (question_answers_ffi.rs), the message box's Display inputs (dialog_inputs_ffi.rs), the lobby's option setter, the placements' Initialize/Terminate/ClearGhostUnits (placement_ffi.rs), `UpdateCursorKind`, the main form's constructor, the mini map's RustOp (mini_map_calls.rs), the selection box's constructor and destructor |
| call records | the main form's, status box's, improvement button's and UI manager's call records made at the call (a constructor and `Ptr()` on a temporary): every member one forward; the UI manager's and the action queues' hosts one-statement shims |
| static_asserts | the game UI's constant asserts as Rust tests reading the headers (hud_cpp_consts_tests.rs) |
| `EECBUIListBox` | its ten `using` lines gone |

The totals are in PORT_LEARNINGS.md "The UI lane, round 14".

## Round 15 (branches `rust-ui15a` toolkit, `rust-ui15b` game, merged on `rust-ui15`, 2026-11-23)

| Piece | What moved |
|---|---|
| ee_ui `listobj.rs` | the list boxes' lines are the Rust list's: `UIListBox<T>`'s `mData` and `mCopyLineArray` are views (`UILines<T>`: `size()`, `[i]`) of boxed Ts the Rust list owns, made, copied and freed through `UILineOps<T>`; the template host `UIListBoxRust` and the T-dependent line methods are gone; UILBString's draw and type-to-find in Rust |
| ee_ui `slider_change.rs` | `UIESliderChange` whole in Rust (the value texts, the label, the modifier and offset, ProcessEvent); `UIEPolledEvent` deleted (no user) |
| ee_ui hosts | UIForm's and UIManager's bodies of more than one call, the toolkit's remaining host switches and scratch statics, the combo box's C++ host (`UIComboBox::Rust`, `ee_uicpp_combo_host`) in Rust |
| ee_game `scratch.rs` | the game UI files' 154 scratch statics are slots of a Rust-owned table (`EEScratch<T>`, ee/EEScreensShims.h): zeroed memory Rust allocates on first use, the C++ object placement-constructed in it |
| ee_game `hud_fields.rs` | the game UI classes' members as Rust records under the C++ names (a second base): EEUIFMainGame (but `mGroups`, `mStrings`), the improvement button, status bar, mini map, portrait, attribute, status button, select and command units, generic background, unit hack, the SCI form manager's flags; the status box's 50 icons a Rust array (`EEUIFMGIconBitmap` gone) |
| call records | `EEUIFMGFormCall` derives from the Rust `EeScreensFormCallObjects` and keeps its three strings; the upgrade, UI manager and status box call records' pointers Rust records; the cursor's read state `EeScreensCursorRead` |
| ee_game `panel_groups.rs` | the panel groups' texts printed in Rust (`c_printf`: the CRT's `%s/%d/%u` with flags, width and precision), `shown_epoch` |

The totals are in PORT_LEARNINGS.md "The UI lane, round 15".

## What is left (the lane's part)

Lane logic 973 (from 1,553 at round 14's end), 629 of it in headers; toolkit 398, game 575. The biggest: EEUIShell.h's
switch frames (44), UIForm's, UIControl's and UIManager's class skeletons, UIControlsTable's three host switches,
UIControl's out-of-line lookups; EEUIFMainGame's `mStrings`, `mGroups` (other lanes read them typed), the unit-hack
and cursor op switches; EEUIBuildWall's and EEUIBuildGate's members (the scenario editor and sim_editor use them by
name: a cross-lane move); EECBCivilizationFiles' context and directory code; EEUserInterface's and EEUIManager's
prototypes and the ui-event switch; one-line prototypes of functions with no header.
- Left on purpose (as rounds 7-14): the printfs of a resource format (`EEAreaEffect::Dump`), qualified base-class
  calls, the collectors' predicates (C++ templates).

## Checks

- Per piece: the build `--sim` 0/0; the moved modules' unit tests (`cargo test -p ee_game --lib screens::`).
- At the round's end: both builds and the test profile 0/0, the oracle seeds 2-3 per simulation commit, `rust_build.py header` and the Rust gate, the oracle
  seeds 2-7, the twelve capture profiles at seeds 2-3, the plain saveload and the matrix with `--expect`,
  scenario_check, sim_editor and `--civbuilder`, the tours (`--exercise tour`, `editor` with its campaign editor
  step, `random-game --box-select --orders --seed 7`) with their captures against the base build's, `bash
  tools/gates.sh`. The LLE toolkit's moves are exercised by every tour (the menus are its forms).
