# The UI cluster lane (the main game HUD, the menus, the UI events, the LLE UI's hosts)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The UI cluster lane, round 1" .. "round 5", "The UI lane, round 6" .. "round 13").

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

## What is left (the lane's part)

Lane logic 2,718 (from 3,682), 1,274 of it in headers. The biggest: EEUIShell.h/.cpp (about 218: the `BaseCall`
switches, `EEShellText`, the host frame), EEUIFMainGame.h/.cpp (about 190), EEScreens.cpp (142: the statics holding
C++ objects, the settings X-macro, the callback tables), EEUIManager.cpp; the toolkit's host frames (`UI*Host::Op`
switches), the class heads, access specifiers and constructors that every control keeps; the C++ call records that
hold a `UWideString` (not done this round); `EECBUIListBox`'s `using` lines (the public accessors now exist);
the vtable-shell collapse of the derived controls' override declarations (T7 in the plan, not started).
- Left on purpose (as rounds 7-12): the printfs of a resource format (`EEAreaEffect::Dump`), qualified base-class
  calls, the collectors' predicates (C++ templates).
- The `--civbuilder` flake: see PORT_LEARNINGS "The UI lane, round 13".

## Checks

- Per piece: the build `--sim` 0/0; the moved modules' unit tests (`cargo test -p ee_game --lib screens::`).
- At the round's end: both builds and the test profile 0/0, the oracle seeds 2-3 per simulation commit, `rust_build.py header` and the Rust gate, the oracle
  seeds 2-7, the twelve capture profiles at seeds 2-3, the plain saveload and the matrix with `--expect`,
  scenario_check, sim_editor and `--civbuilder`, the tours (`--exercise tour`, `editor` with its campaign editor
  step, `random-game --box-select --orders --seed 7`) with their captures against the base build's, `bash
  tools/gates.sh`. The LLE toolkit's moves are exercised by every tour (the menus are its forms).
