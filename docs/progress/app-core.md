# App core (src/Empire Earth/Empire Earth: EE-SIM's driver, EEWorld's bodies, the game, the settings)

The lane's state. Each round rewrites this file to the current state; each round's story goes in PORT_LEARNINGS.md
("App core, round N").

**Rust**:
- `ee_game::sim_driver` (round 1): EE-SIM's driver whole (TEST ONLY): `options.rs` (the command line, `--games`
  lines and their words, the C library's `atol`/`atof`/`strtoul(.., 0)`, the usage), `lobby.rs` (a combo's line by
  name, the lobby's (control, value) list in the harness's order), `report.rs` (the samples, the JSON report, JSON
  strings), `driver.rs` (the state; the output files in the C library's text mode; the debug strings' routing and
  the batch's preamble; the after-tick: save requests, dumps, results, epochs, defeats, the capture's schedules,
  samples over the world views, --flare, --volcano, the end conditions; the menus, the lobby, the load, scenario,
  campaign and editor paths; a game, the leave, the batch), `host.rs` (the C++ table `EeSimDriverHost` and the
  `Game` trait), `ffi.rs`.
- `ee_sim::app::statistics` (round 1): `EEWorld::RecordStatistics`' decisions (the recorded players, the computers'
  top-ups, the scores, the order, the local and human flags, the computers' play times) over a host.
- `ee_sim::app::active` (round 2): the world's active unit lists as one object (the activation and deactivation
  queues, the stepped set; the stepped list is `units::order`'s list itself); the C++ applies each reference change
  as it is reported.
- `ee_sim::app::teams` (round 2): the teams (EEWorld's AddTeam, AddAlliance, BreakAlliance, RemovePlayerFromTeams,
  RebuildTeams; EETeam's Merge, AddPlayer, RemovePlayer, Contains); `EEWorld::mTeams` is the C++ mirror the AI
  thread, the server and the save read.
- `ee_game::sim_driver::editor` (round 2): the editor script's runner and language (lines, words, comments, the
  steps' order and output, the errors); `EESimEditorStep` runs one line's command.

**C++ left**:
- `EESim.cpp` (1,393 lines, was 2,986): the host (the UI's controls under the UI lock, the world lock, the platform
  switches, the C library's streams, environment and stderr descriptors, the save request message, the idle human's
  set-up, --flare/--volcano's world calls, the capture's sweeps), the TEST ONLY parts that read C++ objects (the
  combat, unit-list search and missile sweeps, the checksum line's fields, the state dump, the random trace) and the
  fault handler.
- `EEWorld::RecordStatistics`: the history's fields, the players gathered, the records filled (names, colours, the
  statistics' copies, teams, resources).
- `EESimEditor.cpp` (2,151): every command's body (editor actions, UI controls) and the digests (UI and world reads);
  `EESimCivBuilder.cpp` (437) and `EETestFixture*.cpp` (2,900): unchanged in round 2.

**Gates**: the oracle (EE-AI lines), the reports (wall fields aside), the per-CT checksum lines and the traces
(EE-AIPROF lines aside) byte-identical to the unmodified exe at seeds 2-3; `--games` (three games, one with
`--engine boom,classic`) against single runs of the new and the unmodified exe: every report and trace equal.
WSTA (the app profile; ee/EEAppCapture.h): RecordStatistics at the app profile's end, goldens from the unmodified
C++ (without the history's start time: a wall clock).

**Measured, not moved** (round 1):
- `EESimEditor.cpp` 2,291, `EESimCivBuilder.cpp` 437, `EETestFixture*.cpp` 2,900: test drivers made of UI and
  editor calls; a Rust port would be a host per call.
- `EEWorld.cpp`: the active unit lists (deques of handles with reference counts, read by the physics, the save and
  the dump), the teams (EETeam with its C++ victory condition, read by the AI thread and the server),
  `StartGame`/`CreatePlayers`/`Clear` (object wiring).
- `EEGameState.cpp` (`Create`: file and archive wiring), `EEPreferences.cpp` and `EEGameOptions.cpp` (already shims
  over ee_game app), `EEPhysics.cpp` (the game log's order lines: TEST ONLY, human orders only, no EE-SIM game
  reaches them; the tick already Rust).
- Persistent: `EEPWorldHelper.cpp` (already over ee_units world_save.rs), `EESaveLoad.cpp`.

**Round 2 measure of the test drivers**: their commands are UI and editor calls; moving the dispatch alone moves
each body into a host function of the same size (net about zero). Real reductions need the reads in Rust: the world
digests over the views, the UI controls' state over ee_ui's records (`EeUiControlFields` and the list, combo and
check records), the fixture's ui-state writer and command parser over those (verified only by the harness's fixture
sessions).

**Next**: the fixture's ui-state writer and command parser over ee_ui's records; the editor's world digests
(units, map tiles) over the views; `EEGameState::Create`'s decisions.
