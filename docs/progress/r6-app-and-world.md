# R6 app and world (src/Empire Earth/Empire Earth: the application glue and the world services)

The lane's state. Each round rewrites this file to the current state; each round's story goes in PORT_LEARNINGS.md
("R6 app and world, round N").

**Rust** (`ee_app`, the lane's crate; no FFI imports, every C++ part through a host callback the C++ passes in; the
terrain mesh is ee_ge's object, read and edited directly):
- `game_options.rs`: `EEGameOptions` and `EEGameOptionsPlayer` whole (records the classes derive from, defaults,
  validating copy, `SetDefaults`, the saved game's `Load`/`Save` over `save_io.rs`).
- `settings.rs`: `EEGameSettings`' plain members, their defaults, config.yaml's 75 rows.
- `camera.rs`: `EECameraControl` whole.
- `ambient.rs`: `EEAmbientSoundManager`, `EEAmbientSound` and the world analyzer's counts whole.
- `map.rs` (round 2): `EEMap`'s sight (`AddSight`/`RemoveSight`), cliffs (`UpdateCliffs`, `RemoveFlatCliffs`),
  painting (`PaintTerrain`, `PaintTerrainRect`, `SetCliffColorSets`), `UpdateWater`, `CreateMap`'s fill,
  `ComputeChecksum`, `GetTileCount`.
- `minimap.rs` (round 2): `EEMiniMap`'s cells whole (the sight counts, the building and unit dots, the unit layer's
  pixels, `Recolor`, `Redraw`'s walk), the terrain pixels' colours (`GetPixelColor`, `RedrawPixels`, `RedrawRect`'s
  rectangle). The object the C++ class owns (`mRust`).
- `cheats.rs` (round 2): `EECheatCodes`' table, parse, lookup, list and dispatch (the object the class owns).
- `storm.rs` (round 2): `EEEMStorm`'s record (`EeAppStormFields`), name, fades, step and the two route planners'
  costs.

**C++ left** in the moved classes: the hosts (one switch each), the forwarders; `EEMap`'s lifetime, tile and
visibility arrays, `SetPlayerCount`, `RegisterTerrainSets`, `SetTileTerrain` (the texture loads), the pick lines,
`GetTerrainPitch`/`Roll`, the save; `EEMiniMap`'s surfaces, textures, transformations (ee_gfx), the view trapezoid and
the items a tile gives (its doppelgangers and units: `EEMiniMapBuilding`/`EEMiniMapUnit`); the cheats' world walks
(kill, research all, power, heal, refuel, epoch, explore, the visible-world message) and the camera moves; the storm's
`Begin` (towns, continents, the path manager, the effect), its route request, `Load`/`Save`/`Restore`.

**Gates** (sim_capture, recorded from the unmodified C++ in round 2's first commit): MAPC (the map around each outer
terrain edit), MAPP, MMPX (the mini map's terrain pixels), MMSN (each game minute: every mini map cell's count, pixel
and dots) in the oracle, late, space, calamity, scenario and app profiles; the app profile (`--app-sweep`: 42 cheat
lines typed at five game minutes, CHET/CHEF/CHLS, and a storm of each kind at ten, STRM). MAPS (sight) is recorded
but no capture game builds a wonder (its only caller); MAPK (the out-of-synch checksum) only a multiplayer game runs.
Under EE_SIM_CAPTURE the mesh's colour fractal is seeded 1 (it was the clock).

**Round 2** (branch rust-app2): net -1,007 C++ for the moves (map -203, mini map -309, cheats -338, storm -157), the
capture hook +577 (TEST ONLY); ee_app 56 tests. On the merged tree: both builds 0/0, the oracle identical on seeds
2-7, the six capture profiles PASS at seeds 2-3, the saveload matrix PASS, the editor gate as the unmodified exe, the
Rust gate PASS; the tour, the orders and scroll check, the legacy save and load and a LAN game PASS (PORT_LEARNINGS
"R6 app and world, round 2").

**Measured, not moved**:
- `Empire Earth.cpp` 3,053: start-up, the main loop, shutdown; calls into C++ objects in a fixed order.
- `EEGameState.cpp` 501, `EEGameStateLoader.cpp` 431, `EESaveLoad.cpp` 636: the game's creation and the saved game's
  persistents: dynamic_casts of the persistent templates and object wiring, little decision code.
- `RTSMap.cpp` 783 (the terrain raise, spread and smooth, the closest tile, fog and explore; RTS Engine, last moved by
  world-and-units), `EETile.cpp` 167 / `RTSTile.cpp` 157 (tile sight counts, checksum, terrain type).

**Only a person reaches**: the mini map as seen (the tour draws it; EE-SIM's capture reads its pixels), a cheat typed
in a real chat line (the app profile calls the parser and the dispatch as the chat line does), a storm (no game makes
one: the environment manager never creates a storm; only the sweep does), the ambient sounds heard, the camera's mouse
revolve.

**Next**: `RTSMap`'s terrain edits with world-and-units' agreement (they pair with the cliffs), the tiles' sight,
the storm's `Begin` and save, the app's start-up order as data.
