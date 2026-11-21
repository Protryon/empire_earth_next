# The world lane (Physics Thread, Random Map, Campaign, Data Pre-Loading Thread)

The lane's state. Each round rewrites this file to the current state; the rounds' stories go in PORT_LEARNINGS.md
("The world lane, round 1").

**In Rust**
- The random map whole (round 1, ee_sim rmap `object.rs`): the generator state (`EERandomMapData`, the Default PIMs'
  struct), a try's set-up (InitializeValues, the script read, CreateGrids, CreateValidMap, CreateTeams), Generate
  (the generator steps with exactly what Default.cpp's adapter copied back; the game passes over the object's own
  grid rows), PlaceWall, the retries of `CreateRandomMap`, the generator list's set-up (`Initialize`) and lookup.
  The C++ keeps the generator list (a `std::vector<EERMGenerator>` of `UWideString`s the lobby and the editor read,
  sorted by `std::sort`) and `ee_rmap_cpp`, one statement per world call. Default.cpp, ee/EERandomMap.h and
  ee/EERandomMapView.h live on only as the rmap gate's adapter (tools/rmap_gate/rmap_adapter.cpp, rmap_state.h,
  rmap_view.h); the "default" module is gone.
- `EEPhysics`' bodies (round 1, ee_game `physics.rs`): Tick, DrainInput, GetOutOfSynchChecksum and its OOS dump,
  EndGame, FlushWorld, the unit swaps, Init, HandleMessage, Idle, AutoSave, the civilization and resource timers,
  CheckShiftMode, ReportStatistics, UpdateWeather, UpdateTiming, the area effects clock, the load/save requests,
  GetTimeString, the posts; over the saved block (`EeUnitsPhysicsSaved`) in place and `ee_physcpp`.
- `EEDataPreLoading`'s bodies (round 1, ee_game `preload.rs`): the thread loop, the messages, the queue and its two
  flags (Rust state now), the media loads, LoadGame, WaitForMessageQueue; over `ee_preloadcpp`.
- `EECampaignProgress` whole (round 1, ee_game `campaign.rs`): the lists, Load, RestoreCivilization,
  SaveCompletedScenario, MakeKey; the C++ class is a handle. `EECampaign::Load`'s body (same module).

**Stays C++**
- Physics Thread: `UpdateUnits`' tick host, the after-load fixups (`std::function` lists), Load/Save's file host,
  EEPhysicsModel.cpp's model host (about 80 small functions: the unit, goal, tile and effect calls of pm.rs), the
  projectile and missile steps (their CBPJ/CBPS/CBMA/CBMH captures), PMStepNote (TEST ONLY PMST), EEPhysicsSweep.cpp
  (TEST ONLY: the units and groups sweeps' host and the AGPL record), EEPPatrolGroup.cpp's and
  EEAirTrafficControl.cpp's hosts.
- Campaign: EECampaign's editor side (Save, SaveArchive, CreateScenarioList, CopyScenarioFiles, ListScenarioFiles,
  ListScenarioMedia, ListEffectSound(s), NotArchived, the password crypt), EECampaignFormManager.
- Data Pre-Loading Thread: EEGraphicObject, EEGraphicEffect, EEMusicObject, EESoundObject (object lifetimes and
  table loads over C++ GE objects; their algorithms were already Rust).

**Gates of this lane**: the rmap gate (`tools/rmap_gate/rmap_gate.py check`, `check --reader`: 2088 maps each), the
oracle and the capture profiles, the saveload matrix, scenario_check (the Learning Campaign through the campaign
screens: EECampaign::Load, EECampaignProgress::Load and RestoreCivilization's built-in path), the tour and gates.sh.

**What only a live game exercises**: the campaign progress' save after a won campaign scenario
(SaveCompletedScenario), RestoreCivilization for a user campaign (the extracted civilization), EECampaign::Load with
a password; the physics' autosave (a server with autosaves on), the OOS dump (a dump request), the civilization
timer's warnings (a lobby game with civilization buying), the shift-mode exit (a human holding Shift), the
pre-loader's queueing messages.

**Next**: EECampaign's editor side over the same host (about 250 lines); EEPhysicsSweep.cpp's record and cases as
Rust (TEST ONLY, the units and groups captures check it); the projectile/missile steps and PMStepNote into pm_host.rs
(their captures check them); the model host's multi-statement cases.
