# The UI cluster lane (the main game HUD, the menus, the UI events, the LLE UI's hosts)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The UI cluster lane, round 1" .. "round 5", "The UI lane, round 6" .. "round 12").

**Scope (the lane scopes of 2026-11-19).** `src/Empire Earth/User Interface Thread/*` but the editor-only
controls (`EESE*`, `EEUIFSE*`), `EEFOFindNewTargetNearOldTarget.cpp` (the units' lane) and the WON boxes
(`EEUICWonNet*`, `EEWonNetAvailableGameFormManager.cpp`: the WON lanes'); `src/Empire Earth/UI Events` but the WON
events (`EEUIEWonNet*`); `src/Low-Level Engine/User Interface`; `include/lle/UI*`; their `include/ee` headers
(`EEUI*`, `EEScreens*`, `EEUserInterface*`); the shells (`ee/EEUIShell.h`, `EEUIShell.cpp`, ee_game
`screens/shells*.rs`) and the `ee_ui`/`ee_game` screens. From round 11 also `src/Empire Earth/Civilization Builder`
(the builder, the civilization manager and files, the area effects; the brief's).

## The method (rounds 7 to 12): C++ logic in Rust over one-statement C++ shims

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

## Round 12 (branch `rust-ui12`, 2026-11-21)

| Module | What moved (C++ deleted or now one forward) |
|---|---|
| `area_effect_obj.rs` | `EEAreaEffect` whole (constructor, destructor, `AppliedTo`, `RemovedFrom`, `Covers`, `GetStrength`, `GetLevel`, `GetRadius`, `GetScale`, `Modify`, `MarkDirty`, `Recount`, `Tick`, `ShowOn`, `SetUnit`, `IsInRange`), `EEAreaEffectTemplate`'s constructor, destructor, player and area tests and `Init`; `EEAEManager`'s `Tick`, `Clear`, `ClearEffects`, `CreateAreaEffect`, the template media and its seven unit functors (one C++ `EEAEPassFunctor` left); `EEUnitAreaEffectList::KeepStrongest`; the C++ `EEAEHost` deleted |
| `area_effect_capture.rs`, `civ_capture.rs` | TEST ONLY: `EEAECapture.cpp`'s AETP/AEFX and `EECivCapture.cpp`'s CVMG/CVAP/CVLD/CVAF records (the units keep their shims and hooks) |
| `civ_manager_obj.rs`, `civ_load.rs` | `EECivilizationManager`'s and `EECivPowersManager`'s db reads, loads, premade copies, `ApplyUpgrade` (the C++ `EECivHostCall` deleted), `HasPower` and out-parameter members; `EELoadCivilization` |
| `shell_factories.rs` | `EEUIShellNewIconBar`, `EEUIShellNewChatLine`, `EEUIShellList::DrawBars` (deleted) |
| `hud_bodies.rs` | `EEUIFMainGame`'s destructor, `CreateGroup`, `IsInGameView`; `EEUIBUpgrade`'s constructor body and draw, dirty, mouse and press overrides; `EECBUIListBox`'s constructor body, `AppendStringArray`, `CopyData` |
| `display_modes.rs` | `EEScreens.cpp`'s `ee_screenscpp_modes`, `_current_mode`, `_game_size` (deleted) |
| ee_ui `lle_bg_edit.rs` | the backgrounds' texture size and load and the edit boxes' keys, scroll bar, form mouse, lines and text colour (Rust hosts before `UIBackgroundHost::Op`, `UIEditBoxRust::Op`) |

Dead code deleted: `gDefaultPlayerNames` (EEUIFMainGame.cpp), two unused declarations of EEUIManager.cpp. The sweep: 12
shim list entries now name the Rust area-effect symbols; 17 C++ shims deleted. The totals are in PORT_LEARNINGS.md
"The UI lane, round 12".

## What is left (the lane's part)

Logic lines per file at round 12's end (`logic_lines.py`'s heuristic; headers count every declaration): EEScreens.cpp
174 (the settings record's X-macro list, the statics, the message records' shims whose initializer lists the
heuristic counts), EEUIFMainGame.cpp 116 (the call records' setups, `UpdateCursorKind`, the placements' `Update`, the
unit hack's and cursor's hosts' frames, `EEKey`), EEUIShell.cpp 92 (the `BaseCall` switches, `EEShellText`, the
shell host's frame and statics), UIControl 82 (the 24 key statics, `UIObjHost`'s shared cases), EEUIManager.cpp 73,
UIBackground 58, UIForm 55, EEUIFMGStatusBox 55, UIFileListBox 54, UIManager 52, EEUIFMGStatusButton 50,
EEUIBUpgrade 46 (the call records' members), UILBString 45, EEUIFError 40, EEUIFMGAttribute 39, EEUserInterface 37,
EECBCivilizationFiles 37 (`EEApplyCivilizationFile`, `EECBRequestUpgrades`' contexts, the directory), UIButton 36,
EEUIFMGMainGroup 35, UIEditBox 33, EEUIMiniMap 32; EEAreaEffect.cpp 27 (the morale buildings' collectors, `Dump`),
EECivilizationManager.cpp 22 (the string sinks), EEAEManager.cpp 16. The headers (`EEUIManager.h` 151 with its
control-index enums, `EEUIShell.h` 150, `UIForm.h` 141, `UIControl.h` 137, `EEUIFMainGame.h` 117, `UIListBox.h` 115,
`UIManager.h` 101, ...) go only with whole classes: the toolkit's bases are every control's.
- **Shims whose callee is now Rust**: swept in rounds 10 and 12 (the area effects' twelve).
- Left on purpose (as rounds 7-11): the printfs of a resource format (`EEAreaEffect::Dump`), qualified base-class
  calls, the collectors' predicates (C++ templates), the C++ call records holding a `UWideString` (the main form's,
  the status box's, the improvement button's).
- The `--civbuilder` flake (round 11): not reproduced in round 12; the gate's `cb state` now digests a message box
  left on top, so the next failure names it (PORT_LEARNINGS "The UI lane, round 12").

## Checks

- Per piece: the build `--sim` 0/0; the moved modules' unit tests (`cargo test -p ee_game --lib screens::`).
- At the round's end: both builds and the test profile 0/0, the oracle seeds 2-3 per simulation commit, `rust_build.py header` and the Rust gate, the oracle
  seeds 2-7, the twelve capture profiles at seeds 2-3, the plain saveload and the matrix with `--expect`,
  scenario_check, sim_editor and `--civbuilder`, the tours (`--exercise tour`, `editor` with its campaign editor
  step, `random-game --box-select --orders --seed 7`) with their captures against the base build's, `bash
  tools/gates.sh`. The LLE toolkit's moves are exercised by every tour (the menus are its forms).
