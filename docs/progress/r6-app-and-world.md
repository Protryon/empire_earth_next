# R6 app and world (src/Empire Earth/Empire Earth: the application glue and the world services)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("R6 app and world, round 1").

**Rust** (`ee_app`, the lane's crate; no FFI imports, every C++ part through a host callback the C++ passes in):
- `game_options.rs`: `EEGameOptions` and `EEGameOptionsPlayer` whole: the plain members are records the classes
  derive from (`EeAppOptionsFields`, `EeAppOptionsPlayerFields`), the constructors' defaults, the validating copy
  (`operator=`), `SetDefaults`, and the saved game's `Load`/`Save` over `EESavePrimitive`'s operations
  (`save_io.rs`); the strings, the slots' array and the lock stay C++.
- `settings.rs`: `EEGameSettings`' plain members (`EeAppSettingsFields`) and their defaults, config.yaml's 75 rows
  (58 settings, 17 last game options) read with the default written back and written in the rows' order through
  ee_plat_core's `PlatConfig*`, `Load`'s settings that are not stored and the port's switches; the string members
  stay C++ (one callback).
- `camera.rs`: `EECameraControl` whole: the plain members (`EeAppCameraFields`), every body (the moves, the follow,
  the scroll, the turns, the path, the per-tick `Update`, the clipping distances) over a host of the followed
  units' handles, the key list, the units, the terrain mesh, the viewport and the switches; the math is ee_gfx
  camera.rs's and ee_ge's. `Update` returns what the C++ applies after it (the view to the world and the viewport,
  the mini map).
- `ambient.rs`: `EEAmbientSoundManager`, `EEAmbientSound` and the world analyzer's counts whole: records the classes
  derive from, every decision (the day phases, the terrain, animal and forest sounds, the fades, enabling and
  stopping) over a host of the sound objects and instances, the manager's generator and the world's reads.

**C++ left** in the moved classes: the hosts (one switch each), the forwarders, `EEAmbientSoundManager::Load` (the
file), the analyzer's `CountUnit`/`CountTree` (the rendered units' classes, goals and towns), `EECameraControl::
GetUnitPoint` (the host's unit point), the preferences' string members and `EEDefaultSettings`.

**Measured, not moved** (lines on rust at the round's start):
- `Empire Earth.cpp` 3,053: start-up, the main loop, shutdown; mostly calls into C++ objects in a fixed order and the
  globals other units declare as references into `gGamePreferences`/`gWorld`. A move needs the subsystems' starts as
  host operations; the globals stay.
- `EECheatCodes.cpp` 595: the table, the parser, `LookUp`, `ListCheatCodes` and `ApplyCheatCode`'s switch can be
  Rust (~-300 C++); the actions are world walks (kill, refuel, heal, research all) that would stay a host switch.
  Reached only through a chat line (a cheat message): no gate types one.
- `EEGameState.cpp` 526, `EEGameStateLoader.cpp` 453, `EESaveLoad.cpp` 731: the game's creation and the saved game's
  persistents; mostly C++ object wiring, reached by every game and the saveload matrix.
- `EEEMStorm.cpp` 646 (the storm manager and its AI route modifiers), `EEMap.cpp` 983 (sight, cliffs, terrain
  painting; simulation), `EEMiniMap.cpp` 797 (the mini map's pixels; the UI's half is ee_screens mini_map.rs).

**Round 1** (branch rust-app1): net -1,883 C++ by the commits (-2,656 measured without the address markers);
ee_app 27 tests. **Gates**: the oracle; CAMU (the camera's whole state at each `Update`'s start and end, recorded from the unmodified
C++ in the oracle and scenario profiles at seeds 2-3: `tools/sim_capture/golden_cpp.txt`, `golden_cpp_scenario.txt`);
the saveload matrix; the config round trips (an EE-SIM game over a missing config.yaml and over one with bad values:
the written files byte for byte the unmodified build's); the tour, the orders game and the scroll check (the camera,
the ambient sounds, the settings tabs).
**Only a person reaches**: the ambient sounds' audible result (EE-SIM has no sound; the tour plays them unheard), the
camera's mouse revolve and rotate, the cinematic keys outside the scenario profile's campaigns.

**Next**: `EECheatCodes` (the table, parse and dispatch), `EEMiniMap`'s pixel colours and cell counts, `EEEMStorm`'s
step and route costs, `EEMap`'s sight and cliffs (simulation: CAMU-like captures first), the app's start-up order as
data.
