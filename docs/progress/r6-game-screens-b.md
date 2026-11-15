# R6 game screens B (the host, the dialogs, the UI manager and thread, the last events)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md.

**Scope.** The second game screens lane (crate `ee_screens`, branches `rust-screensb<N>`). The first lane
(`r6-game-screens-60k-lines-on-the-toolkit.md`) keeps the in-game HUD: `EEUIFMainGame`, `EEUIFMG*`, `EEUIFMGG*`,
the `EEUIC*` HUD controls, the `EEUIB*` buttons, the portrait, icon bar and mini map. This lane takes the rest of
the game's UI outside WON and the editors: the host every moved form goes through (`UI Events/EEScreens.cpp`), the
UI thread (`EEUserInterface.cpp`), the UI manager (`EEUIManager.cpp`), the message box (`EEUIFError.cpp`), the framed
background (`EEUIBGenericBackground.cpp`), the post-game line graph (`EEUIPGSLineGraph.cpp`, with the toolkit's
`UIGraph` and `UILineGraph` when it takes them), the file combo box (`EEUIFileComboBox.cpp`), the scenario intro's
form manager (`EEUIEScenarioIntroEvents.cpp`), the models of the moved forms and the ids headers of the forms whose
events moved (`ee/EEUIManager.h`, `ee/EEControlEventID.h`, `ee/EEFormEventID.h`). WON's events and boxes
(`EEUIEWonNet*`, `EEUICWonNet*`, `EEWonNetAvailableGameFormManager`) are the WON lanes'.

**Round 1** (branch `rust-screensb1`; PORT_LEARNINGS "R6: game screens B, round 1"): the campaign editor's model
whole in Rust (`campaign_model.rs`), the moved forms' dead ids, the include lines eight units compile without. The
scope's C++ 8,509 -> 7,095 (-1,414).

**Round 2** (branch `rust-screensb2`; PORT_LEARNINGS "R6: game screens B, round 2"): the direct path. Every toolkit
object the screens call records its C++ object in `ee_ui`'s registry (`direct.rs`: forms, the manager, entity
groups, backgrounds, control records, buttons, combo boxes, labels, edit boxes, sliders); `ee_screens`' `Form`
answers the plain toolkit ops in Rust before the C++ hop (`toolkit.rs`), the virtuals through the control record's
host (`UIControl::HostOp`); the backgrounds are named by their constructor (`ee_ui_direct_background`). 51 host cases (44 ops, 7 text cases) left `EEScreens.cpp` (3,654 -> 3,482 lines). The measure: of the host's
~580 case labels only ~140 (~500 lines) call the toolkit alone; the rest are game calls. The round took the
toolkit calls whose target is a whole Rust object.

**Left** (C++ lines at the end of round 2): `EEScreens.cpp` 3,482: the toolkit calls that need a C++ object (about
330 lines: the list boxes' lines and selections, combo appends, new `EEScreenEvent`s and hot keys, `GEText` colours
and fonts, bitmaps, the post-game summary's status bars and graphs, the civilization choice's list) and the game
calls; a small `ee_screenscpp_new_event` would let the event and hot key group (about 60 lines) move next.
`EEUserInterface.cpp` 1,380 (`SetLoadProgress`, `ShowLagStatus`, `UpdateIMECompositionString`'s attributes,
`ChangeRasterizer`, `ResizeGameScreen` can join `ui_game.rs`), `EEUIManager.cpp` 553, `EEUIFError.cpp` 421,
`EEUIBGenericBackground.cpp` 266, `EEUIEScenarioIntroEvents.cpp` 256, `EEUIPGSLineGraph.cpp` 110 (with `UIGraph.cpp`
405 and `UILineGraph.cpp` 143: one Rust graph object; the post-game summary is a person's only),
`EEUIFileComboBox.cpp` 85, `ee/EEUIManager.h` 345.
