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

**C++ left**:
- `EESim.cpp` (1,393 lines, was 2,986): the host (the UI's controls under the UI lock, the world lock, the platform
  switches, the C library's streams, environment and stderr descriptors, the save request message, the idle human's
  set-up, --flare/--volcano's world calls, the capture's sweeps), the TEST ONLY parts that read C++ objects (the
  combat, unit-list search and missile sweeps, the checksum line's fields, the state dump, the random trace) and the
  fault handler.
- `EEWorld::RecordStatistics`: the history's fields, the players gathered, the records filled (names, colours, the
  statistics' copies, teams, resources).

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

**Next**: the active unit lists as a Rust object (with the unit store's order list they mirror), the teams' logic
over the views, `EEGameState::Create`'s decisions.
