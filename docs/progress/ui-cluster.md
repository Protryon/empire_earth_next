# The UI cluster lane (the main game HUD, the menus, the UI events, the LLE UI's hosts)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The UI cluster lane, round 1" .. "round 5", "The UI lane, round 6" .. "round 11").

**Scope (the lane scopes of 2026-11-19).** `src/Empire Earth/User Interface Thread/*` but the editor-only
controls (`EESE*`, `EEUIFSE*`), `EEFOFindNewTargetNearOldTarget.cpp` (the units' lane) and the WON boxes
(`EEUICWonNet*`, `EEWonNetAvailableGameFormManager.cpp`: the WON lanes'); `src/Empire Earth/UI Events` but the WON
events (`EEUIEWonNet*`); `src/Low-Level Engine/User Interface`; `include/lle/UI*`; their `include/ee` headers
(`EEUI*`, `EEScreens*`, `EEUserInterface*`); the shells (`ee/EEUIShell.h`, `EEUIShell.cpp`, ee_game
`screens/shells*.rs`) and the `ee_ui`/`ee_game` screens. From round 11 also `src/Empire Earth/Civilization Builder`
(the builder, the civilization manager and files, the area effects; the brief's).

## The method (rounds 7 to 11): C++ logic in Rust over one-statement C++ shims

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
- The generator and the case cutter were scratch (build/ui8/gen, build/ui9/gen, build/ui11: `gen.py` spec lines `name(arg: T) [-> R] | C++
  statement` -> the Rust list entry and the `extern "C"` one-liner; `cut.py` deletes cases by label).

## Round 11 (branch `rust-ui11`, 2026-11-21)

| Module (ee_game screens) | What moved (C++ deleted or now one forward) |
|---|---|
| `civ_host.rs` | `EECivilizationBuilder.cpp`'s builder host `ee_screenscpp_cb` whole (deleted: every op, the edit boxes, labels, file lists, form events, strings, the request's message, the players' reload, the civilization load) and `EECBUIListBox::HostOp` whole (the list's ops, the line draw); the civilization tables' queries read from `ee_sim::tech::civ` directly |
| `civ_manager_calls.rs` | `EECivilizationManager.cpp`'s simulation host (`EECivHostCall`, now one forward: the class upgrades' stats read and put, the sight, the fields, the nodes, the media) and `FillCivilizationList` |
| `civ_file_calls.rs` | `EECBCivilizationFiles.cpp`'s `EECivFileCall` (one forward: the file's load, the civilization reset and free buys, the grant, the refusal's chat and draw) |
| `portrait_calls.rs` | `EEUICPortrait::RustOp` (deleted), the constructor's, destructor's, `DrawTextA`'s and `MouseMove`'s bodies |
| `command_units_ops.rs` | `EEUICCommandUnits::RustOp` (deleted: the arrow's draw, the ground point, the pick, the group's centre, the click; the editor's `EESEUICCommandUnits::RustOp` falls back to `EEUIRustControl`'s) |
| `icon_bar_ops.rs` | `EEUIShellControl::RustOp` (deleted: the icon bar kind's ops) |
| `area_effect_host.rs` | `EEAreaEffect.cpp`'s `EEAEHost::Call` (one forward: every world call of the area effects; the morale buildings' functors stay) |
| `area_effect_manager.rs` | `EEAEManager::CollectEffects`, `FindEffect` |
| `area_effect_running.rs` | `EEAEManager::Load`, `EEAESaveRunningState`, `EEAELoadRunningState` and its fix-up (the anonymous `EEAERefOf`, `EEAEEffectOf`, `EEAEComplexUnit` deleted) |
| `area_effect_lists.rs` | `EEAreaEffectTemplate::Load`, `Unload`; `EEUnitAreaEffectList::AddAreaEffect`, `ApplyModifiers`, `Remove`, `MarkDirty`, `MoveAndUpdate`, `Tick` |
| `ui_message_calls.rs`, `ui_game_calls.rs` | `EEUserInterface.cpp`'s `ee_screenscpp_ui_message` and `ee_screenscpp_ui_game` whole (deleted) |

Shims: 581 one-statement `extern "C"` shims added and 2 shim list entries naming Rust symbols (`ug_civ_edit`,
`ug_civ_create`); none deleted. The totals are in PORT_LEARNINGS.md "The UI lane, round 11".

## What is left (the lane's part)

Logic lines per file at round 11's end (`logic_lines.py`'s heuristic): EEScreens.cpp 202 (the settings record's
X-macro list, `modes`, `current_mode`, `game_size`, the lobby's option setter and rules, the statics), EEUIFMainGame.cpp
177 (the destructor, `CreateGroup`, `IsInGameView`, the 32 default player names, `EEPostGraphicsAll`), EECivCapture.cpp
149 and EEAECapture.cpp 130 (TEST ONLY capture records: their units are cut from the game's units by hook pointers,
so a Rust caller would need the same cut), EEUIShell.cpp 137 (the one-line cases, the `BaseCall` members, `DrawBars`,
the icon bar's and chat line's constructors), EEAreaEffect.cpp 120 (the morale buildings' functors, the effect's and
template's constructors, `Init`, the forwarders' frames), EEAEManager.cpp 101 (the unit passes' functors, `Clear`,
the I/O op), EECivilizationManager.cpp 91 (the db reads, the forwarders' sinks), EEUIBUpgrade 88, UIControl 82,
EEUIManager 75, UIBackground 73, EECBCivilizationFiles 71 (`EELoadCivilizationUncaptured`, the directory), UIForm 55,
EEUIFMGStatusBox 55, UIFileListBox 54, UIManager 52, EEUIFMGStatusButton 50, UIEditBox 50, UILBString 45, EEUIFError
40, EEUIFMGAttribute 39, EEUserInterface 37, UIButton 36, EEUIFMGMainGroup 35. The `include/lle/UI*` and `include/ee`
headers are declarations: they go only with a whole class.
- **Shims whose callee is now Rust**: swept once (round 10). Round 11's shims call C++ only, but `ug_civ_edit` and
  `ug_civ_create`, which name the builder's Rust entry points.
- Left on purpose (as rounds 7-10): the printfs of a resource format, qualified base-class calls, the unit passes'
  functors over `EEForEachPlayersUnit` and the collectors' predicates (C++ templates).

## Checks

- Per piece: the build `--sim` 0/0; the moved modules' unit tests (`cargo test -p ee_game --lib screens::`).
- At the round's end: both builds and the test profile 0/0, the oracle seeds 2-3 per simulation commit, `rust_build.py header` and the Rust gate, the oracle
  seeds 2-7, the twelve capture profiles at seeds 2-3, the plain saveload and the matrix with `--expect`,
  scenario_check, sim_editor and `--civbuilder`, the tours (`--exercise tour`, `editor` with its campaign editor
  step, `random-game --box-select --orders --seed 7`) with their captures against the base build's, `bash
  tools/gates.sh`. The LLE toolkit's moves are exercised by every tour (the menus are its forms).
