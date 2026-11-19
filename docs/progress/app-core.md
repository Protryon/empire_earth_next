# App core (src/Empire Earth/Empire Earth: EE-SIM's driver, the UI fixture, the game log, EEWorld's bodies, the game)

The lane's state. Each round rewrites this file to the current state; each round's story goes in PORT_LEARNINGS.md
("App core, round N"; the app cluster's rounds: "The app cluster, round N").

**Rust**:
- `ee_game::sim_driver` (app core round 1): EE-SIM's driver whole (TEST ONLY): `options.rs`, `lobby.rs`,
  `report.rs`, `driver.rs`, `host.rs` (the C++ table `EeSimDriverHost`), `ffi.rs`; `editor.rs` (round 2): the
  editor script's runner and language.
- `ee_game::fixture` (app cluster round 1): the in-process UI fixture whole (TEST ONLY, EE_TEST_UI): `poll.rs` (the
  test switches, the thread, the ui-state file, the telemetry over the world views, the frame request and BMP, the
  commands, the pointer hold), `boxsel.rs` (the box selection: the drag's stages, the lasso hook, the expected set over
  the views), `cast.rs` (the calamity casts), `orders.rs` (the orders: the plan, the scans, the click checks, the
  retries, the message hook's judges over the views), `text.rs` (the C library's scanning and numbers, the escaped
  texts, the BMP header, the text mode), `host.rs` (`EeFixtureOp`, `EeFixtureCall`: one C++ host function),
  `ffi.rs`.
- `ee_game::game_log` (app cluster round 1): the game log (ee/EEGameLog.h) whole: the open, the player lines, the
  epochs, the samples, the income and losses, the chat; a host for the file and the objects' methods.
- `ee_sim::app::statistics` (round 1): `EEWorld::RecordStatistics`' decisions over a host.
- `ee_sim::app::active`, `ee_sim::app::teams` (round 2): the active unit lists and the teams as Rust objects.
- The simulation capture, TEST ONLY (app cluster round 2): `ee_sim::app::capture` (the writer whole: the file, the
  filters, the records; ee/EESimCapture.h's class forwards), `ee_sim::app::player_capture` (PLST),
  `ee_sim::netmsg::decisions` (NMDC: the tables and the players' part), `ee_game::app::map_capture` (MAPC, MAPS),
  `ee_game::sim_driver::combat_sweep` (the combat formulas' sweep), `ee_game::sim_driver::digest` (the editor gate's
  map, mesh and unit lines; MSVC's `%a`), all over the world views and the terrain mesh's Rust object.

**C++ left**:
- `EETestFixture.cpp` (970 lines, was 1,460 + EETestFixtureOrders.cpp's 1,440): `EEFixtureHost` (about 65 operations:
  the UI's controls by RTTI kind, the input devices and the platform's events, the game's messages and UI actions,
  the picker, the units' and goals' methods, the calamity manager, the camera's mesh tests), the hooks' trampolines,
  the WON lobby's walk (on the lobby's own loop: the WON lane's component classes), the debug output, the fault
  handler and the legacy harness's communications mirror. About 140 of its lines are includes.
- `EEGameLog.cpp` (162, was 311): the log's host (PlatFOpen, the lobby options, a player's texts, a goal's
  resource, a killer's owner) and the header's six forwards.
- `EESim.cpp` (1,300): EE-SIM's host table and the TEST ONLY readers of C++ objects (the list search and missile
  sweeps, which call the units' methods; the combat sweep's host: EECanHeal, GetPosition, the projectile class; the
  checksum fields, the state dump, the random trace), the fault handler.
- `EESimEditor.cpp` (2,100), `EESimCivBuilder.cpp` (437): every command's body (editor actions, UI controls) and
  the digests of the editor's forms, the selection, the paint area, the players (`PlayersDigest`: names, civilization
  files, the computer players' switches are C++ objects' members) and the save's chunks.
- `EEAppCaptureWorld.cpp` (445), `EEMessageCapture.cpp` (330): the capture records of C++ objects (CHEF's upgrade
  levels and camera, the storms, the mini map, WSTA's records; the messages' members).
- `EEWorld.cpp` (StartGame, CreatePlayers, Clear, ReplacePlayer, PlaceQueuedUnits), `EEGameState.cpp` (Create),
  `Empire Earth.cpp` (the start-up and shut-down wiring; `EEInitializeDatabase`'s tables are now tables of loaders),
  Player, Persistent, Communications, Campaign: object wiring over C++ objects (measured in rounds 1 and 2).

**Gates for the fixture** (it is TEST ONLY: no EE-SIM game reaches it): the harness's fixture sessions only. The tour
with `--input-check --box-select --orders`, with `--scroll-check --zoom-captures 100 --cast earthquake,plague`, with
`--view-check`, the editor exercise, the lan game (gates.sh), the harness self test; the ui-state file compared line
for line with the unmodified exe's (the same lines but the run's values).

**Next**: EEWorld, EEPlayer and Persistent whole, Communications over ee_net and Empire Earth.cpp (the round's items
2 to 5: not started; PORT_LEARNINGS "The app cluster, round 2" says why); `PlayersDigest` and CHEF once the
players' names and upgrades and the computer players' switches are Rust records; the fixture's WON lobby walk over
the WON lane's component records once they are Rust; `EEGameState::Create`'s decisions.
