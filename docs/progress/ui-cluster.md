# The UI cluster lane (the main game HUD, the menus, the UI events, the LLE UI's hosts)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The UI cluster lane, round 1" .. "round 5", "The UI lane, round 6" .. "round 16").

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

## Round 17 (the LLE and UI floor lane `lu1`, branch `rust-lleui1`, forks `rust-lleui1-a` (LLE), `rust-lleui1-b` (UI), 2026-11-24)

One lane for the floor of both: include/lle, src/Low-Level Engine, src/Platform/Common, the UI thread, include/ee/EEUI*,
the UI events, src/RTS Engine/Graphics. Lane logic (build/lu1/perfile.py, logic_lines.py's count over those files)
2,162 at fba26cc96 -> 1,572 (-590; target -1,000): the UI half 945 -> 708 (-237), the LLE half 1,217 -> 864 (-353;
docs/progress/lle-shims.md "Round 14"). The story is PORT_LEARNINGS.md "The LLE and UI floor lane, round 1".

| Commit | What | Lane logic |
|---|---|---|
| 23bc3f1b1 | lle/LLEForward.h: one forward list for the LLE and UI headers (168 scattered declarations); no access sections or class friends; `URust<T>` (the owned Rust object) and `URustView` (a view) instead of each class's deleted copy/ctor/dtor | 2,162 -> 1,920 |
| 91047865e (b) | the WON screens' 29 event classes one template by their front-end call; four headers and three units gone | -29 |
| 4c7e1c68a (b) | the game's free UI helpers in Rust (ee_game screens ui_helpers.rs): the string reference, EETranslateString, EEDisplayScenarioIntro, EEPostF9Key | -53 |
| d18522a61 (b) | the toolkit's eight hosts declared by their Rust modules' headers (explicit exports, not `lle_host!`'s macro-made ones cbindgen cannot see) | -8 |
| fcae35047 (b) | the UI units' declarations: two shared globals declared once (ee/EmpireEarth.h), the rectangle's layout one assert, initializer lists, the games box's unread tail | -31 |
| 764602bc0 | the WON events one class holding its call (`EEWonFrontEvent`), made from the event by `ee_won_front_event_call`: the 26 typedefs and FrontNewEvent's switch gone | 1,622 -> 1,596 |
| 0658720d8 | the three WON list boxes' members inline; EEUICWonNetGamesBox.cpp, RoomsBox.cpp, StatBox.cpp deleted | -16 |
| e786ba628 | EEUIFMGStatusBox's five calls inline with their call record in the header | -5 |
| e25c2b9a2 | EEUIManager's keyboard, mouse and editor start inline with `EEUIManagerCall` in the header | -3 |

The LLE half's commits (3f7593ca5, b97ed3c87) are in lle-shims.md. The two forks' pass-1 work was merged on
1400c5d29 with origin/rust bbb0654d2 (batch 24); pass 2 was the lead's alone (no fork could be started: the session's
agent limit).

## What is left after round 17 (the UI half, 708)

EEFOFindNewTargetNearOldTarget.cpp 135 (the units lane's file in this directory); EEUIShell.h 42 and EEUIShell.cpp 10
(as round 16); EEUIFScenarioEditor.h 42 (the editor form's members and the scenario editor's shared externs, read by
37 units of the editor lane); EEUIFMainGame.h 24 and .cpp 12 (`mStrings`, `EEUIFMGFormCall`); the three WON list
boxes' line structs and members (UIListBox<T> templates over C++ strings: a Rust record cannot hold a UWideString's
destructor); EEUIFSETerrain.h 17, UIListBox.h 16, UIObjHost.h 16 (UIF4), EEScreens.cpp 14; the status button's four
members (they need EEUIFMainGame complete: ButtonCall), the main group's constructor (it news the status box).

## Round 15 (LLE/UI round 2, fork B: the UI toolkit; branch `rust-lleui2-b`, 2026-11-25)

The measure is total C++ lines (`tools/port_metrics.py`). The toolkit (src/Low-Level Engine/User Interface,
include/lle/UI*.h): **-909 C++ lines, -20 compiled files** against a102c738e (95,507 / 275; logic -1, shim -104;
classes unchanged). Behaviour: none intended (oracle seeds 2-7 identical; a harness tour with `EE_DIRECT_CHECK` found
every control record by its C++ pointer, 78,691 lookups, none missing).

| Commit | What | cpp_lines |
|---|---|---|
| bbd4fa7b8 | UICheckBox.cpp and UIRadioButton.cpp deleted: they compiled to nothing (includes and a comment their headers carry) | -102 (-2 files) |
| 6cb005e72 | the 18 other units one, `User Interface.cpp`: each a section under its unit's own comment (a reference elsewhere to UIForm.cpp's host or UIButton.cpp's shims names its section), one include list of 40 headers found by trial compiles (the units had 610 include lines); the three `LLE_UI*_CPP` switches gone with the units; `tools/harness/layout_gen.py` dumps the one unit (every class its two units saw, checked) | -668 (-18 files) |
| eef347435 | the controls' and forms' polls (`TMPoll`, whose only member is ee_base's `EeTimePoll`) made, run and freed in Rust (lle_hosts.rs `polls`), and the hot keys' static 50 ms poll | -47 |
| 042eb1a06 | lle_cpp.rs's `shims!` takes a `rust:` part, calls that were one statement on another crate's Rust object (the fake logs them alike, so the goldens stay): the input entities, the shared key slots and the key predicates, Num Lock, the texts' colours (`own`) | -29 |
| 8011fedfc | the tables' ops on the locks (ee_base sections), the clock, the form's Tab, Shift and wheel, the clipboard and the texts' setters (lle_hosts.rs `own`); the UI lock's creation | -23 |
| 43f231d39 | a control's own members (border thickness, type and opacity, index, always-dirty, the button-down flags, a form slot's) read and written in its record, which direct.rs finds by the C++ control (lle_hosts.rs `fields`) | -16 |
| 702a53cf6 | UIButtonState.h (one include of rust/ui/ids.h) gone; UIEMouseWheelScroll and UITiledBackground, included only by the toolkit's unit, in it | -24 |

Lessons: the include lists were most of what was left (610 of 2,100 lines in 21 units): with the classes whole in
Rust, the units are their host shims, and one unit needs one list. A shim of one call on another crate's Rust object
(`TMPoll`, `ISEntity`, `TSCriticalSection`, `GEText`: each a C++ class over one Rust record or pointer) is a Rust call
of that crate's C ABI; the host tables' `x` entries and `shims!`'s `rust:` part take it without changing what a
golden sees (the objects still make the op).

What is left: the shims that call a C++ virtual (the controls', forms', backgrounds' and events' overridable members:
the game's classes override them), the constructors (`new UIButton`, `new GEText(font)`, `new GERhombus`: the C++
object is the host owner the Rust object calls back), the `gISKeyboard`/`gISMouse` lookups (C++ globals of the
input system, fork A's), the strings and string vectors (`UWideString` has a vtable: its record is not at the
object's address), the fonts (a `GEFont` is a `UReferenceCounter`), and the non-virtual members of forms and
managers that Rust could call through direct.rs (`GetManager()->GetScreen()` chains: next).

### Pass 2 (branch `rust-lleui2-d`, from rust-lleui2 61544a7d6 with origin/rust batch 29)

**-462 C++ lines** against 61544a7d6 (87,963 -> 87,501; compiled files unchanged, 251; logic unchanged, shim -381;
classes unchanged; extern_c +5: the harness's six EETestUI* are Rust's now, the 200 removed shims were `EE_UI_CASE`
macros the metric does not count). Behaviour: none intended (oracle seeds 2-7 identical after the background fix
below).

| Commit | What | cpp_lines |
|---|---|---|
| a122d23cf | ee_ui lle_chains.rs: the ~200 host ops whose shim was a chain of non-virtual C++ forwarders (`theForm->GetManager()->GetScreen()`, `ctl->GetForm()->GetTabStop()`, a control's, background's, hot key's or table's member, a scroll bar's slider, the database loaders' statics) make the forwarders' inward calls themselves; direct.rs finds each Rust object by the C++ pointer (new kinds: ScrollBar, Table, HotKey; the hot key's constructors hand Rust the C++ object). The forwarders nothing calls any more and the members no C++ ever called went with them | -311 |
| 3311121dd | the combo box's parts (PartOf's rectangle and border, the edit box's text, UIControl's CopyData/SetRectangle/SetLocation, the base rectangle); the harness's EETestUI* in direct.rs (the unit keeps the `/include:` pragmas); the macros the moved shims used; 21 more members no C++ calls | -81 |
| 9ee87014c | the controls table's setters whose C++ member is a non-virtual forwarder (lle_hosts.rs SET takes a shim or a Rust op), the combo box's list queries, the file lists' selections, the radio button's checked state and bitmaps, the label's fit; five ChangeWidth members no C++ calls | -59 |
| 773698328 | SetupText's texts (the list box's `mText` keeps its shim), the combo box's label and edit texts, the graph text's colour and transparency | -11 |
| 1a2932dd6 | bgobj.rs: a background is registered for direct.rs before its constructor's calls out: its `BgAdd` (now Rust: `AddBackground` of the registry's `UIBackground*`) ran before the registration and handed the form NULL, which the first form Open dereferenced (EE-SIM faulted in `ee_uicpp_form_bg_open` on every seed) | 0 |

Lessons: an op that finds its object through direct.rs needs the object registered before the first call out that
can reach it, and a constructor's own calls out come before `made()`'s registration (the form's and manager's
`new` make none; the background's `init` adds itself to its form; the control's record and the hot key are
registered first). The sim oracle opens the forms, so a NULL from the registry shows there; the lookup check
(`EE_DIRECT_CHECK`) names the op. A virtual stays a shim when any class overrides it (`GetType`: the check, radio and
tri-state boxes; `UnCheck`: the editor's tabs).

What is left (the unit is 1,037 lines): the virtuals (the controls', forms', backgrounds', events' and the manager's
overridable members), the constructors and deletes of C++ objects (controls, texts, rhombi, lines, strings, hot keys,
events, the wheel event), the GE objects' calls (bitmaps, textures, the rasterizer's windows and blending, GEText's
font and string, the rhombi: fork A's), the string table and the C++ strings (`UWideString`, `std::vector`), the file
system's calls (`FSFileSpec`, `FSPath`, the file lists' searches), the sound manager's, `gISKeyboard`/`gISMouse` and
the list box's members over its C++ line views (`mData`, `mCopyLineArray`, `mText`).

## Round 16 (branches `rust-ui16a` toolkit then civilization builder and UI thread, `rust-ui16b` game UI then HUD, two passes each, merged on `rust-ui16`, 2026-11-23)

The collapse phase: classes deleted, not only bodies moved. Lane classes 115 -> 96 (+2 outside the lane).

| Piece | What moved |
|---|---|
| ee_ui table host | UIControlsTableHost's Make/Text/Set/Op switches as Rust tables over one-statement shims; SetupGEText Rust's |
| ee_ui buttons | UIScalableButton, UIDualButton, UIScrollButton folded into UIButton (dispatch on the Rust record, `Virt*` calls); their headers deleted |
| toolkit skeletons | UIControl's lookups inline, DrawPolygons/MouseLButtonDoublePressed/GetBgOpacity Rust's; a dozen ctors and dtors inline; UICDatabase, UIEventData, UWSNoCaseAlphaSort, UIScrollBarRust gone; the forward declarations one list (lle/UIObjHost.h) |
| ee_game panel groups | EEUIFMGGroup, EEUIFMGGRust deleted; `mMainGroup` plus five Rust kinds by `mCurrentGroup` |
| ee_game `cpp_events.rs` | EEUIECancel, EEUIELoadUnit, EEUIEMGHotKeyStatusButton, EEUIEProcessTimerExpired, EEUIEndCinematicModeEvent as EEScreenEvent codes; EEUIECLEFormManager and EEChatLogEntry gone (the chat line `EeScreensChatEntry`) |
| ee_game placements | EEUIBuildWall/EEUIBuildGate typedefs of the Rust records; EESEUIBuildWall/Gate deleted (cross-lane: EESEUnit, EESimEditor) |
| host switches | the unit hack's, cursor's, UI events', select control's (`select_units_ops.rs`) and improvement button's (`upgrade_ops.rs`) switches as one-statement shims the Rust hosts call |
| HUD constructors | seven HUD classes' constructors inline forwards with their records filled in the initializers; access toggles gone |
| UI thread prototypes | six callees named in Rust by the shim list; three declarations in their callees' own headers; two externs read through their owners |

The totals are in PORT_LEARNINGS.md "The UI lane, round 16".

## What is left (the lane's part)

Lane logic 628 (from 973 at round 15's end), 385 of it in headers. The biggest: EEUIShell.h (44: the shells' Make
cannot move into a shared template constructor, the kind would be built before the derived shell's virtuals exist) and
EEUIShell.cpp (10); lle/UIObjHost.h's forward-declaration list and UIF4 (42); EECBCivilizationFiles' context and
directory code (25); EEUIFMainGame's `mStrings` and `EEUIFMGFormCall` (24 + 12); EEAreaEffect's predicate structs and
Dump (23); UIListBox.h's op switch and UILBNoSort (17, EEUIShell.cpp uses it); UIShims.h's hand-written host
declarations (15, cbindgen cannot see macro-made functions); EEUIManager.h, EEUserInterface.h, EEScreens.cpp's media and
lobby-rule helpers; the status box's call record, the status button's declarations, the command units' and main
group's constructors; the WON glue prototypes (EEIsLobbyOpen, EESetQuitFlag, EELobbyShutdown: WON-lane C++).
- Left on purpose (as rounds 7-15): the printfs of a resource format (`EEAreaEffect::Dump`), qualified base-class
  calls, the collectors' predicates (C++ templates).

## Checks

- Per piece: the build `--sim` 0/0; the moved modules' unit tests (`cargo test -p ee_game --lib screens::`).
- At the round's end: both builds and the test profile 0/0, the oracle seeds 2-3 per simulation commit, `rust_build.py header` and the Rust gate, the oracle
  seeds 2-7, the twelve capture profiles at seeds 2-3, the plain saveload and the matrix with `--expect`,
  scenario_check, sim_editor and `--civbuilder`, the tours (`--exercise tour`, `editor` with its campaign editor
  step, `random-game --box-select --orders --seed 7`) with their captures against the base build's, `bash
  tools/gates.sh`. The LLE toolkit's moves are exercised by every tour (the menus are its forms).
