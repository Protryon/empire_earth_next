# R6 game screens B (the host, the dialogs, the UI manager and thread, the last events)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md.

**Scope.** The second game screens lane (crate `ee_screens`, branches `rust-screensb<N>`). The first lane
(`r6-game-screens-60k-lines-on-the-toolkit.md`) keeps the in-game HUD: `EEUIFMainGame`, `EEUIFMG*`, `EEUIFMGG*`,
the `EEUIC*` HUD controls, the `EEUIB*` buttons, the portrait, icon bar and mini map. This lane takes the rest of
the game's UI outside WON and the editors: the host every moved form goes through (`UI Events/EEScreens.cpp`), the
UI thread (`EEUserInterface.cpp`), the UI manager (`EEUIManager.cpp`), the message box (`EEUIFError.cpp`), the framed
background (`EEUIBGenericBackground.cpp`), the post-game graph (now `ee_ui graphobj.rs`), the file combo box
(`EEUIFileComboBox.cpp`), the scenario intro's form manager (`EEUIEScenarioIntroEvents.cpp`), the models of the
moved forms and the ids headers of the forms whose events moved (`ee/EEUIManager.h`, `ee/EEControlEventID.h`,
`ee/EEFormEventID.h`). WON's events and boxes (`EEUIEWonNet*`, `EEUICWonNet*`,
`EEWonNetAvailableGameFormManager`) are the WON lanes'.

**Round 1** (branch `rust-screensb1`; PORT_LEARNINGS "R6: game screens B, round 1"): the campaign editor's model
whole in Rust, the moved forms' dead ids, include lines. -1,414.

**Round 2** (branch `rust-screensb2`; PORT_LEARNINGS "R6: game screens B, round 2"): the direct path (`ee_ui
direct.rs`: every toolkit object's C++ pointer -> its Rust object; `ee_screens toolkit.rs` answers the toolkit ops
before the C++ hop). 51 host cases moved. -166.

**Round 3** (branch `rust-screensb3`; PORT_LEARNINGS "R6: game screens B, round 3"): `UIGraph`, `UILineGraph` and
`EEUIPGSLineGraph` one whole Rust object (`ee_ui graphobj.rs`; one C++ shell in `lle/UIGraph.h`, the summary's 31
stored graphs and its form manager's 8 inside it); the summary's graph ops and texts, the event and hot key group
(over `ee_screenscpp_event`), the list and combo boxes' lines (through each list's per-type host), the controls'
plain ops (rectangle, colours, opacity, the status bars) on the direct path; the direct path's lookup check
(`EE_DIRECT_CHECK=<file>`). -840 (`EEScreens.cpp` 3,389 -> 3,119; the graph units -613). A fixture step that
opened the post-game summary over a running game was tried and reverted: the summary needs the statistics' records,
which the game writes at its end, so a capture of it needs a game played to its end (a person's, or a fixture
that ends one).

**Round 4** (branch `rust-screensb4`; PORT_LEARNINGS "R6: game screens B, round 4"): the screens' read view of the
game (`ee_screens game_view.rs`: one C++ fill per group at the read, `ee_screenscpp_game_view` and
`ee_screenscpp_game_texts`; the settings, the session, the world, a player through its accessors, the colour table,
the campaign and intro managers). 88 host case labels moved over it (58 ops, 30 texts), among them the sound
manager's and the conductor's ops on their `ee_sound` objects, the chat log's reads, the named-form lookup
(`ee_screenscpp_form` gone) and the two player colour ops; `EEUserInterface::ShowLagStatus` in Rust
(`ui_game::lag_status`). -107 (`EEScreens.cpp` 3,119 -> 3,034, `EEUserInterface.cpp` -19): a read case is two or
three lines and its fill line one, so the view frees little by itself.

**Left** (C++ at the end of round 4): `EEScreens.cpp` 3,034 lines; the measure (`build/sb3/measure.py`) classes 240
game-call labels (1,176 lines, the view's fills among them: about 160) and 61 game texts (415): messages to
`gCommunications`, new servers, file and archive opens, saved-game headers, map backdrops, the WON calls, the
campaign and intro managers' file members. They move with the lanes that own those objects, or with
`EEUISCIFormManager` (`EEUIEScenarioIntroEvents.cpp`, 234 lines; its four `FSFileSpec*` and media strings; users
in the scenario, HUD and WON lanes' files) and `EECampaignFormManager` as Rust objects, which would take the SI_*,
CS_* and campaign texts with them. The toolkit-only cases of round 3's list are unchanged. `EEUserInterface.cpp`
1,248 (`SetLoadProgress`, `ResizeGameScreen`, `ChangeRasterizer`: game calls a host op each), `EEUIManager.cpp` 491,
`EEUIFError.cpp`, `EEUIBGenericBackground.cpp`, `EEUIFileComboBox.cpp`, `ee/EEUIManager.h`. The list boxes' lines
stay the C++ template's `std::vector<T>` (round 3's learnings).
