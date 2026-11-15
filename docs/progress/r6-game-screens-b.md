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

**Round 1** (branch `rust-screensb1`; PORT_LEARNINGS "R6: game screens B, round 1"): the measure (the scope's
decisions were Rust already, rounds 13 and 21-28 of the first lane; what was left was host code, the C++ classes the
toolkit makes, include blocks and dead ids), then three commits: the campaign editor's model whole in Rust
(`campaign_model.rs`: `EECampaignEditorBehavior.cpp` and its header deleted; golden driver over the original unit,
35,000 ops identical, 15 of 15 Rust and 11 of 11 host mutations caught), the moved forms' dead ids (444
enumerators), the include lines eight units compile without (377). The scope's C++ 8,509 -> 7,095 (-1,414: -534,
-504, -376). Only a person reaches the campaign editor; the other two commits change no code.

**Left** (C++ lines at the end of round 1): `EEScreens.cpp` 3,654 (one `case` per toolkit or simulation call, all
live; it shrinks when the toolkit's controls are reachable from Rust without a C++ hop or the forms' C++ objects go),
`EEUserInterface.cpp` 1,380 (the thread's hosts and posts; `SetLoadProgress`, `ShowLagStatus`,
`UpdateIMECompositionString`'s attributes, `ChangeRasterizer`, `ResizeGameScreen` can join `ui_game.rs`),
`EEUIManager.cpp` 553 (the factories' and the control creation's `new`s), `EEUIFError.cpp` 421 (the form, its two
button classes, the dialog's host), `EEUIBGenericBackground.cpp` 266 (the frame's host, the campaign map's cell
loader), `EEUIEScenarioIntroEvents.cpp` 256 (`EEUISCIFormManager`'s file opens), `EEUIPGSLineGraph.cpp` 110 (with
`UIGraph.cpp` 405 and `UILineGraph.cpp` 143 of the toolkit: one Rust graph object is the next whole move; the
post-game summary is a person's only), `EEUIFileComboBox.cpp` 85, `ee/EEUIManager.h` 345 (the class, the HUD's and
WON's ids).
