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

**Left** (C++ at the end of round 3): `EEScreens.cpp` 3,119 lines, of which the measure (`build/sb3/measure.py`)
classes 281 game-call labels (1,178 lines) and 89 game texts (509): the sessions, options, campaigns, saves and
messages, which go with the game's own lanes. The toolkit-only cases left (19 ops, 23 texts, about 175 lines) each
need a C++ object no Rust object stands for: the chat's stack `UIHotKey` (`CH_SEND_KEYS`), `GEText`s copied or read
(`PGS_TEXT_STYLE`, `DS_BUTTON_NUMBER`, the name colours), `GEBitmap` coordinates (`DS_UV`), new controls
(`PGS_NEW_BAR`, `PGS_NEW_STAR`, `PGS_REMOVE`), `UIFileComboBox`/`EECBUIListBox`/`EEUICPing` members, the background
rectangle (`SS_BACKGROUND`), the CRT's `rand`. `EEUserInterface.cpp` 1,267 (`SetLoadProgress`, `ShowLagStatus`,
`UpdateIMECompositionString`'s attributes, `ChangeRasterizer`, `ResizeGameScreen`: each a handful of game calls, so
a move to `ui_game.rs` frees a few lines each; not taken), `EEUIManager.cpp` 491, `EEUIFError.cpp`,
`EEUIBGenericBackground.cpp`, `EEUIEScenarioIntroEvents.cpp`, `EEUIFileComboBox.cpp`, `ee/EEUIManager.h`. The
game-call cases on plain records (players, options) need those records reachable from `ee_screens` (they are C++
objects it holds no pointer to): a lane that gives the screens a view of `EEGameSettings` and the players would
let about 60 such cases move. The list boxes' lines stay the C++ template's `std::vector<T>` (see round 3's
learnings for why the Rust store was not taken).
