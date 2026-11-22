# The game core lane (was the game's leaves): src/Empire Earth/Empire Earth, Persistent, Database, Player, Tech Tree, Civilization Builder, Calamities, Communications, Graphics Thread, Map (src/Low-Level Engine, include/lle, gx, dx7 and src/RTS Engine went to the LLE lane on 2026-11-20)

The lane's state. Each round rewrites this file to the current state; each round's story goes in PORT_LEARNINGS.md
("The game's leaves, round 1/2", "The game core lane, round 3/4/5/6/7/8/9/10"). The census is `python -I tools/cpp_deps.py report`
(after a build: it reads the objects): `leaves.txt`, `near_leaves.txt`, `waves.txt`; `cpp_deps.py unit FILE` names
one unit's edges. Since batch 6 (agent rules "Lane scopes") the lane owns the directories above and nothing else.

## Moved

| C++ (deleted or shrunk) | Rust home | What callers keep |
|---|---|---|
| **Round 10** (branch `rust-core10`; five lanes again, chained: EECommandLineOp -> EECore10OpA (EEGraphics.cpp -> EEROUnits.cpp -> EERenderSnapshot.cpp -> EEMouse.cpp) -> B (EEDbObject.cpp -> B2..B9 over the Database and Tech Tree units, EEPCalamityHelper.cpp) -> C (EEMessageCapture.cpp) -> D (EEGameUtilities.cpp -> EEEMStorm.cpp, EEEnvironmentManager.cpp, EECheatCodes.cpp, EECivilization.cpp, EEVCWonder.cpp, EECameraControl.cpp) -> E (Empire Earth.cpp -> EECommandLine.cpp -> EEWorld.cpp: the end)) | | |
| EEGraphics::RenderTo whole, the graphics object's ctor and dtor; EEROUnits' media, draws and three raster entries; EEROUserInterface, EEROWater::RenderScene, EEROGroundShadows, EEROTerrain; EERenderSnapshot.cpp; EEMouse's pointers; RFIL's fill (EERenderCapture.cpp); EEEffect::Move and GetBounds; gRenderEnvironment (lane A) | gamecpp `render_to.rs`, `ro_units.rs`, `ro_views.rs`, `render_object.rs` | EERenderUnit derives from ee_gfx's EeGfxFillUnit, EERenderShadow is EeGfxFillObject (the mirror asserts gone) |
| EETerrainTable, EECliffTerrainTable, EEButtonObject, EEAnimal (all but Find), EETerrainColorTable; EEDataTextFile.cpp (EEDbFindFile, the row ids, the rules' load and dump); the tech tree's ctor, loads, operator=s, the saved game's walk host, TSUM, TNMT, TSWP, EVAT's parts; EEPCalamityHelper's ctor, dtor, Reset, operator= (lane B) | gamecpp `db_media.rs`, `db_text.rs`, `tech_load.rs`, `tech_capture.rs`, `persist_more.rs` | EERules::Refresh public; NULL ctor stores became member initializers |
| EEMessageCapture.cpp (NMIM, NMEX, NMSW, NMCU, NMNE); EEMessage's ExportMessage, ExportSize, ImportMessage; EEMRecord's ctors, Field, the wait id, IsResponse, GetCodecSlots, ImportMessage (lane C) | gamecpp `msg_capture.rs`, `mrecord.rs` | the hooks forward; EEMRecordValue static_asserted against `EeGamecppMrValue` |
| EEEMStorm (all but the ctor/dtor/virtual forwarders), EEEnvironmentManager (all but the ctor), STRM and the storm sweep; EECheatCodes' host and actions, the cheat sweep's host; EECivilization; EEVCWonder; EEGameUtilities.cpp whole; EECameraControl's GetUnitPoint and Update's tail, CAMU (lane D) | gamecpp `weather.rs`, `cheats.rs`, `civilization.rs`, `utilities.rs`, `camera_host.rs` | ee_sim's StormHost and CheatHost implemented in Rust over cases (the C++ host functions gone); the storm's route is paths 2's Rust request |
| EECommandLine.cpp's EEUpdateSavedGames and EEConvertGameFile; Empire Earth.cpp's EEStartScreen (the class, its Win32 window and thread); EEWorld::RecordStatistics; EETestFixture.cpp's WON lobby walk, commands and snapshot (lane E) | gamecpp `saved_games.rs`, `start_screen.rs` (windows-sys 0.59 Gdi in ee_game), `world_stats.rs`; fixture `lobby.rs` | the lobby's UserEvent is a C++ trampoline to `ee_fixture_lobby_work`; world_stats.rs is `cbindgen:ignore` (its statistics imports name a generic) and its export is declared by hand in EEWorld.cpp |
| **Round 9** (branch `rust-core9`; five lanes in parallel worktrees, each a chained switch: EEGraphicsOpNext -> EECore9OpA (EERenderObject.cpp, EEUnitPicker.cpp) -> B (EEEffectHost.cpp, EEMap.cpp, EEMiniMap.cpp, EETile.cpp, EEContagionManager.cpp) -> C (EESaveLoad.cpp and the Persistent helpers) -> D (EEServer.cpp, EEClient.cpp, EEAEManager.cpp, EEAreaEffect.cpp, EETechTree.cpp, EETechTreeNode.cpp) -> E (Empire Earth.cpp, EECommandLine.cpp: the end)) | | |
| EERenderObject.cpp: FillRenderLists, IsUnitDrawn, FillUnit, FillObject, the doppelgangers' fills, FillGhostRecords, the Unload helpers, DrawGroundCircle, QueueEffects, Fn10C036, Fn10E8D1, LoadCliffTextures, EEInitialiseScene, EEReleaseScene, the fades and letterbox, ApplyLighting, the raster table's compound entries, RFIL's world key; EEUnitPicker.cpp: Contains, the pickers, Clear, Reset, AddUnit, GetFogFactor, the walks' host | ee_game gamecpp `render_object.rs`, `picker.rs` (a Rust `GameFill` implements ee_gfx's `FillHost`: the C++ fill table is gone) | the functions forward; GetUnitOpacity a static C++ helper through one case; FindUnit and PickBox C++ over `ee_gamecpp_picker_host` |
| EEEffectHost.cpp's compound answers (the storm, the volcano's build, the rain's sounds, the goal markers, the damaged tick, the quad batches, the decals); EEMap.cpp (the tile array, the screen line, pitch and roll, the terrain sets, the save operation's compound cases, the map host whole); EEMiniMap.cpp (the dots, the host whole, the redraws); EETile.cpp (checksum, load, dump, save, terrain type); the contagion world host's and EECalamityIoCall's compound cases | gamecpp `effect_host.rs`, `map.rs`, `minimap.rs`, `tile.rs`, `calamity_hosts.rs` | the host tables name Rust `extern "C"` functions directly |
| Persistent: EEPWorldHelper (and world_save.rs's host, now `ee_gamecpp_pworld_host`), EESaveLoad, EEGameStateLoader, EEPPlayerHelper, EEPSaveGameHeader, EEPMultiplayerHelper, EEPCalamityHelper's Load/Repair/Save, EEPChecksum | gamecpp `persist_world.rs`, `persist_save.rs`, `persist_parts.rs` over `persistent.rs` (each FSFile Read/Write overload one case) | the methods forward; RestorePersistents, ReadPersistent, RestorePlayers gone from ee/EEGameStateLoader.h |
| EEServer.cpp's session host; EEClient's CreateXferFile, HandleSlotUpdate, HandleConnectionTimeoutWarning; EETechHost, EETechTree's Load, UnloadAll, the node ids, UpdateInterface, SumCostsUncaptured; EETechTreeEpoch's DeleteNodes, Unload, node ids, media; EETechTreeNode::GetCostUncaptured | gamecpp `server.rs`, `tech.rs` (round 9's `civ.rs`, the area effects, dropped at the merge with UI round 11, which owns them) | EETechHost a one-statement forwarder (EEWorldLayout takes its address); EETechMedia deleted |
| Empire Earth.cpp: EEGetFileChecksum, EEInitializeNetwork, EERun; EECommandLine.cpp: EEParseCommandLine, loadgame, hostgame, joinbyip, EEIsCommand | gamecpp `entry.rs` | WinMain, main, EECreateInstanceMutex stay C++ (the test profile's patch, the harness hook) |
| EETestFixture.cpp's EEFixtureHost: 30 compound cases | ee_game fixture `compound.rs` over finer `Fx` operations (callers unchanged: `Call::run` answers a compound operation) | the WON lobby walk, Input, State, the lobby post and snapshot stay C++ |
| the sweep: 21 host cases whose callee was a C++ forwarder to Rust (the calamity's sounds and target, the class's location and target checks, the scene's release, the game view, the sounds' stop, the data paths, the map's player count, the restorables, the world's counts and time of day, the tile's checksum and dump, the deferred managers, the epoch count, the attack on a player) | the Rust calls the Rust | the cases and their EeGameCpp variants deleted |
| **Round 8** (branch `rust-core8`; the same host, its cases chained by file: EEGameOps.cpp -> EEWorld.cpp -> EEGameState.cpp -> EEMessage.cpp -> Empire Earth.cpp -> EEGraphics.cpp) | | |
| EEGameState.cpp: Create (the saved game's, the scenario's and the campaign's loads, a new game's players, teams, managers and random map), Init, Clear, Reset, RepairRestorables, SetCalamityHelper, SetEnvironmentManager | ee_game gamecpp `state.rs` (`EeGameStateTail`: the members mMap..mRevealMap read in place) | the methods forward; `EEGameStateOp` (EEGameState.cpp) |
| EEMessage.cpp: the Execute bodies the C++ kept (kNetMsgBodies: the civilization's upgrades, the cargo unload, the improvement, the main screen, the load or save, the cheat unit, the host migration's slots, the saved group, the flare, the stance, the loading's start, the formation), the host's world, unit and player actions, the fast selection, the select lists, the resources, the disband and delete refunds, the placement test, EESendGroupCommand's markers, EELoadOrSave | ee_game gamecpp `message.rs` | EENetMsgHost's cases are one call each; EEStartLoading stays a C++ symbol (the legacy harness's seed pin); `EEMessageOp` |
| Empire Earth.cpp: the start-up (EEInitializePart1/2, the display, the graphics and its 38 fonts, the sounds, the database's 26 tables, the game objects, the threads, the persistents, the data paths, the menus' screen, the video mode, a new screen size, the game view), the shutdown routines (the graphics, the threads, the sound manager, the game objects, the database), EEClearGame, EEAttackPlayers, EELoadNetworkSettings, EELoadSound, the media loads | ee_game gamecpp `startup.rs` | the functions forward (one statement); `EEStartupOp` (the font and table names as two C++ tables) |
| EEGraphics.cpp: FrameStarting, FrameDone, HandleMessage, RestoreSurfaces, SaveScreenShot | ee_game gamecpp `graphics.rs` | the methods forward; `EEGraphicsOp` |
| **Round 7** (branch `rust-core7`; the game's own bodies over one host, EEGameOps.cpp's `ee_gamecpp`, `EeGameCpp`) | | |
| EECalamityUpdate.cpp's host (the calls every class shares, each class's world steps: the quake's cracks, the storm, the volcano and its cleared footprint, the time warp, the teleport, the logic bomb, the cloak and its capitols, the meteors, the area effects, the leprosy) | ee_game gamecpp `calamity.rs` | EECalamityUpdate.cpp: the run's C++ objects (`EECalamityUpdateHost`, its state in and out); `EeCalamityHostFfi::call` is `ee_gamecpp_calamity_call` |
| EECalamity, EECalamityManager, EECalamityClass (all but the ctor, GetCost, GetUnitSet, GetContagionFunctor and the save Io host), the contagion functors' ctor, Update, Infect, Cure, Apply; EECalamityCast.cpp (deleted) | ee_game gamecpp `calamity_objects.rs` | the methods forward (one statement each); `DestroyCalamity(iterator)` gone (Update erases by index) |
| EECalamityCapture.cpp (CALI, CALK), EETechCapture.cpp's calamity part (CALW, CCAS and its sweep, CALT, CONT, the calamities' game) | ee_game gamecpp `calamity_capture.rs` | the hooks are one-line C++ wrappers over the Rust; the event lists are Rust statics |
| EEWorld.cpp: the ctor's members, StartGame, CreatePlayers, AddComputerPlayers, Clear, RemoveAllUnits, Reset, ReplacePlayer, PlaceQueuedUnits, ResortUnit, the active queues, the resource users, the stances and RebuildTeams' stances, the slot moves, LoadMedia, UnloadMedia, DumpMedia, the pauses, SetTimeOfDay, SetPopulationLimit, SetNumPlayers, MarkPlayerOut, ScreenToWorld, GetPlayerIndex, GetPlayerBySlot | ee_game gamecpp `world.rs` | EEWorld's methods forward; its C++ calls and members are `EEWorldOp`'s cases (in EEWorld.cpp, beside the file statics they read) |
| **Round 6** (branch `rust-core6`; the owner's rule of 2026-11-20: a C++ call from Rust is a one-statement shim) | | |
| EESimEditor.cpp's steps (EESimEditorStep's 70 commands) and digests (the save's and a file's chunks, the world, the scenario lists and texts, the area screen, the trigger screens' controls, the form's controls, the players, the terrain tab, the tech lists, the form state, the trigger-only report, the open trigger and object) | ee_game sim_driver `sed_steps.rs`, `sed_digest.rs` | EESimEditor.cpp: the step's run on the UI thread (the step action, the marker, the clock pin) and one host case per C++ call or member (`ee_simcpp_sed`, `_text`, `_float`; `EeSimDriverSed`) |
| EESimCivBuilder.cpp (the "cb" steps, deleted), ee/EESimEditor.h (deleted) | ee_game sim_driver `sed_civ.rs` | the same host |
| EESim.cpp's host bodies: the form on top and its class, a click, the lobby's combos, the screens' lists, the control of a screen event, the physics' clock, the editor's presence, the idle human, the save request, the platform switches, --flare, --volcano and the boulders, the rules' overrides, the AI ids, a unit class's value; HostSweep, the combat sweep's host, the unit-list search sweep, the missile sweep, EESightCaptureSweep | ee_game sim_driver `sim_host.rs` over `sim_ops.rs` | EESim.cpp: the shorter host table and one case per C++ call (`ee_simcpp_sim`, `_text`, `_float`; `EeSimDriverSim`) |
| **Round 5** (branch `rust-core5`) | | |
| EEPlayer's private lists (mStrandedUnits, mUnitsToRepatrol, mLoadedUnitIDs, mConstructionSiteIDs, mWonderIDs, mSoundLoadedClasses, mLoadedMedia, mUnitsOffMap) | ee_sim units `player_lists.rs` (EeUnitsPlayerLists, EEPlayer::mLists) | the methods over them are one-line calls; the units' reference counts and ids through `ee_units_lists_host` (EEPlayer.cpp) |
| EEWUnitsOffMapInfo (the class, its Load, Save, RepairPointers, SetUnit, ReleaseUnit) | ee_sim units `player_lists.rs` (EeUnitsOffMapInfo) | ee/EEWUnitsOffMapInfo.h: `typedef EeUnitsOffMapInfo EEWUnitsOffMapInfo`; the callers set `mReason` through GetOffMapInfo as before |
| EEPlayer::mTownsByID (written, never read), mL9B4 (made, never used), RemoveTownID | (gone) | |
| 88 dead class-static save keys (EEPlayer, RTSPlayer, RTSUnit, EETechTree, EETechTreeEpoch, RTSMap, RTSTile, RTSVisibleMap, EEMap, EECivilization, the persistent helpers) | (gone: dead code, not a move) | |
| **Round 4** (branch `rust-core4`) | | |
| EETechTree.cpp's gEpochCount, sNodeEpochs, EETechTreeEpochOf, EERegisterTechNode | ee_sim tech `tech_ids.rs` | EEEpochCount, EEIsEpochID, EETechTreeEpochOf inline in ee/EETechTreeTechID.h over two hand-declared Rust calls |
| the tech tree's captured entry points (TNRS, TNPR, TSOL, TTRS), TEPO, TRTE, the TNWK walk records (EETechTree/Epoch/Node.cpp, EETechCapture.cpp) | ee_sim tech `nodes_capture.rs` (the FfiHost answers the wrapped host operations) | the methods are walk forwarders; NeedsUnitMedia and GetBuildTime call ee_tech_capture_node_walk |
| EETechTreeNode::LoadMedia, UnloadMedia, GetCostString; EETechTreeEpoch::LoadMediaOfKind, UnloadMediaOfKind | ee_sim tech `node_media.rs` | EETechHost's MEDIA_EFFECT and MEDIA operations (EETechMedia) |
| EETechTreeEpoch::Get | (inline in ee/EEPlayer.h) | the epoch's std::map |
| EEAppCaptureWorld.cpp's cheat sweep (CHET, CHEF, the 42 lines), MAPK, MAPP, MMPX, MMSN, CHLS; EEMiniMap::GetCell | ee_game app `cheat_capture.rs`, `world_capture.rs` | a five-call host (parse, apply, the switch, the upgrades' levels, the camera); the map, mini map and cheat units call the records directly |
| GEWindow's getters and Resize (GEBitmap.cpp); the GE objects' hosted constructors in the LLE headers | (inline in GEWindow.h); ee_render ge `game_hosts.rs` | GEVertexHost, GEObjectHost have C linkage and are named by Rust only |
| **Round 3** (branch `rust-core3`) | | |
| FSMemoryFile.cpp, FSChunk.cpp, FSArchive.cpp, FSRustOps.h | (ee_base fs memfile.rs, chunk.rs, archive_obj.rs already) | lle/FSMemoryFile.h, FSChunk.h, FSArchive.h inline; `FSMemoryFile::Ops` the FSFile table; Close/Copy/FOpen (stdio) in File System.cpp |
| SESoundManager.cpp, SEMixer.cpp, SEScore.cpp | (ee_audio sound already) | lle/SESoundManager.h, SESample.h, SESMInstanceNode.h, SEMixer.h, SEScore.h inline; `ee_soundcpp_*` in Sound Engine.cpp |
| ESChecksum.cpp | (ee_base error checksum.rs) | lle/ESChecksum.h inline |
| NESynchronousClient.cpp, NESynchronousServer.cpp (the forwarders) | (ee_net client.rs, server.rs) | inline in their headers; the units keep the constructors and hosts |
| GX/GX.cpp | (ee_render gx ffi.rs) | gx/GX.h inline with the extern "C" declarations, the ABI checks, the harness /include pragmas; GXGameSize in EEPreferences.cpp |
| Map/EESightCapture.cpp (TSGT) | ee_game `sight_capture.rs` (rust/game/sight_capture.h) | ee/EESightCapture.h: `EESightScope` inline; `EESightRecord` (EEMap.cpp); the end sweep in EESim.cpp |
| EETechCapture.cpp's node/epoch/tree states, TCST, EVHP, CALC, TRTE, EVUN, TNWK; EECivCapture.cpp's CVMG head | ee_sim tech `capture_rec.rs` | the hooks (EVHP's and CALC's point at Rust); C++ wrappers pass the Rust records and the C++ members |
| 1,527 #include lines of 71 units (Empire Earth 591, Graphics Thread 244, Civilization Builder 221, Communications 193, Persistent 150, the rest 128) | | each unit's object symbol for symbol HEAD's (llvm-nm); EEWorld, EEPlayer, EEGlobals and five LLE emission units untouched |
| **Rounds 1-2** (branches `rust-leaves1`, `rust-leaves2`) | | |
| EECPPlaceCapture, EEAIArea, EEColorTable, EECPBehaviorTable, EEUnitBehavior, EEDatabase, EEUnitSet(Manager), EEGameVariant, EETeam, EEHumanPlayer, EECDKeyStrings; EESimCapture, EEAIAStarCapture, EEAIContinentCapture, EEWorldViewCheck, EEVoting, EECPCombatManager, EEStartingResources, EEUCUpgradeTable, EEAIUnitTargetingTable, EECPEngine | PORT_LEARNINGS "The game's leaves, round 1" / "round 2" | |

## Left, and why

- **What round 10 left** (the forks' reports, PORT_LEARNINGS "The game core lane, round 10"): EEEffectHost.cpp's
  layout table and world tables (142: offsetof over C++ classes, C++ lambdas); EERenderListsView and MakeLayout
  (EEROUnits.cpp, 76: C++ multimaps, offsetof); EEUnitPicker.cpp (52) and EERenderObject.cpp's globals and raster table
  (54); EERules.cpp, EERulesGenDecode.h, EEDataText.h, EEDataTextSpecs.h (cppgate compiles them standalone; the decoder
  is generated); the hot lookups (EETerrainTable::Get, EEAnimal::Find, the tech nodes' GetCost, EEToHitTable); the
  Persistent text names (GetSaveTextName, GetDateText: UString lengths and asctime); EEServer.cpp (150) and EEClient.cpp
  (30) (LAN-only); EEMessage.cpp's rest (the sort comparators, EENewSelectUnits); the calamity I/O switches and
  EECalamityRun; EEMap's TEST ONLY scopes and context, EETile's pool; EECameraHost's compound cases; EEAmbientSoundManager,
  EEMusicManager, EEPreferences, EEGameOptions, EEGameLog, WSTA; EESim.cpp's DumpState (157, --dump-at only),
  ChecksumText, RngTraceHook and the fault handlers; EESimEditor.cpp's step classes (EEAction/UIEvent vtables); the
  fixture's Input case; Empire Earth.cpp's globals' storage, EESimPreferences, the window pump, the POSIX smoke test.
- **Taken by other lanes this round**: the area effects (EEAEManager, EEAreaEffect, EEAreaEffectTemplate) and the
  civilization captures (EECivCapture.cpp) are the UI lane's (rounds 11 and 12); round 10's lane C versions were
  dropped at the merge.
- **What round 9 left in its files** (each fork's report, PORT_LEARNINGS "The game core lane, round 9"):
  the Graphics Thread's other units (EEROUnits.cpp 157, EEROTerrain 50, EEROUserInterface 47, EEROGroundShadows 39,
  EEMouse 32, EERenderSnapshot 30, EEROWater::RenderScene); EERenderObject.cpp's globals, layout asserts and raster
  table entries (about 113); EEEffectHost.cpp's layout registration and world table fills (159: they name C++
  lambdas); EEMap.cpp's TEST ONLY scopes, Load/Save's context, SetCliffTexture, the constructors; EETile's AddUnit and
  FreeUnitNode (a hot path); EEContagionManager's FILE switch and EECalamity.cpp (about 30 each); the Persistent
  helpers' rest (EEPCalamityHelper's constructors, destructor, Reset, operator=; EESaveLoad's GetSaveTextName,
  GetDateText, the .txt twin's delete; EEIsScenarioFile); EECivilizationManager's EECivHostCall (100: it needs the
  upgrade's and unit class's Rust record field names), EECivilizationBuilder, EECBCivilizationFiles, EECBUIListBox,
  EEMRecord, the tech tree's epoch and node Load and operator=, SumCosts' TSUM capture; the Database directory;
  EEGraphics::RenderTo (the post-game map capture: no gate reaches it); the start screen's Win32 window
  (EEStartScreen::ThreadMain, WindowProc), EESimPreferences, EEUpdateSavedGames and EEConvertGameFile; the POSIX
  main's smoke test (EESmokeTest: no Windows build compiles it).
- **What round 7 left in the moved files**: EEWorld::RecordStatistics (the records' strings and the Rust statistics'
  host callbacks), the teams' mirror (EEMirrorTeams) and the setters that delete and replace a manager; EECalamity's
  save Io host (EECalamityIoCall) and the contagion world host (ee_contagion_world: the Rust contagion manager's C++
  calls, 60 lines); EECalamityClass's ctor and EEGameVariant::GetGameVariant.
- **EEPlayer** (round 5's brief: the player whole in Rust): EEPlayer.cpp is 3,100 lines of hosts and orchestration
  over other lanes' classes; every decision is Rust already (ee_sim units player*.rs, 7,900 lines). The parts, by
  size: EEPlayerHost 340 (a case per C++ call of the Rust bodies: tech nodes, unit classes, the towns and herds, the
  groups, the patrol groups, the UI), the save operations 160 (a C++ sub-object's Load or Save each), the towns' save
  host 98, the tick's operations 69, the selection methods 180 (EESelectedGroup), ClearFootprint 69, the TEST ONLY
  PLST/PLEC capture 80, the constructors, destructor, Reset and Clear 150, the class media forwarders (EEUnitStats).
  The member containers of C++ objects are read in place by 13-38 files each and by the world views. What moves
  next, in order: the groups as Rust objects (Units lane), then the selection methods and the groups' parts of
  Reset, the destructor, Clear, the tick and the save; the towns' and herds' host cases straight into ee_sim::cp's
  town and herd bodies (Rust already; needs EeTownFields' and EeHerdFields' base offsets in the world layout,
  EEWorldLayout.cpp: Units lane); EEUnitClassUpgrade as a Rust record (Units lane) for the improvement cases,
  RefreshUpgrades and the media forwarders.
- **The tech tree's containers** (EETechTreeEpochList, each epoch's node map): the walks are Rust already over
  EETechHost; the containers hold C++ nodes (virtual GetCost, GetIsAvailable) the trees deep-copy (operator=), so a
  Rust map of them saves the four container cases of EETechHost and the epoch's map code (about 60 lines); not done.
- **The round's census** (tools/cpp_deps.py, the UI's units: User Interface Thread, UI Events, LLE User Interface,
  the scenario editor; EEFOFindNewTargetNearOldTarget aside): 325 edges (950 symbols) into this lane at the start,
  287 (859) after the GE constructors and the window's getters; GEBitmap.cpp 37 UI units -> 3, the tech ids 13 -> 9.
  The rest: EEGlobals.cpp (45 units: the globals' storage), the tech tree's EEEpochTitleID and EETechTreeEpoch::Get,
  EEPlayer's class media calls (they forward to the Units lane's EEUnitStats), EERenderObject's globals, the camera's
  forwarders (their host is EECameraControl.cpp's), the GE texture's stage setters, RTSMouse::ClampPosition.
- **What the lane's C++ is now**: forwarders that need another class complete or a Rust header too big for their
  header's includers, and hosts: the Rust objects' calls on C++ objects (the calamity classes' world steps,
  EECalamityUpdate.cpp; the players' and world's walks, EEPlayer.cpp, EEWorld.cpp; the render objects' fills,
  EERenderObject.cpp, EEEffectHost.cpp; the network's message ops). A host goes when its callees are Rust.
- **EERules** (208): the converter's cppgate compiles EERules.cpp (tools/eedata/cppgate.py line 45). Its eight
  lookups and the rules' load are 100 lines; the C++ structs are decoded from the Rust's bytes (EERulesGenDecode.h)
  and read as members by 20 units, so the lookups could only become inline readers (EECivBuilderRules::FindID and
  FindCategory have no caller, but EERulesGen.h is generated: tools/eedata/rules.py).
- **RTSWorld, RTSPlayer, RTSPhysics, RTSGraphics, RTSMouse** (1,600; the LLE lane's since 2026-11-20): C++ virtuals of the game's classes and their
  lists; the same.
- **GERasterizer's setters** (the LLE lane's since 2026-11-20; 130 lines of forwarders): rust/render/ge.h is 3,300 lines and GERasterizer.h has 100
  includers; they need a header of their own (a module of ee_render's ge split for them). **DX7Rasterizer's
  forwarders** (130): two of the draws use a .cpp helper and DX7Surface; DX7Rasterizer.h would include dx7.h.
- **AEFX** (EEAECapture.cpp), **the app sweeps** (EEAppCaptureWorld.cpp), **the message capture**
  (EEMessageCapture.cpp): they read C++ members (the area effects' C++ lists, the messages) or call C++ methods
  (GetCost, Modify, the cheats); round 7's calamity_capture.rs shows the shape (a Rust `Record` over the
  EEGameOps.cpp reads).
- **EEWorldLayout** (Units lane), **EEUnitStore** (Units lane), the media objects (EESoundObject, EEAnimal,
  EEButtonObject, the terrain tables: C++ media and texture objects), **EEVCWonder** (the wonders are not in the
  views), the message classes (their vtables are the network engine's).

## Next

Round 10's "Left" by size: EEEffectHost's tables (a Rust layout of the effect classes from the Units lane's records),
EESim.cpp's DumpState (over the world views), EEServer.cpp and EEClient.cpp (LAN-only: the LAN game and the session
golden check them), the managers' rest (ambient sounds, music, preferences, game options, the game log), the camera
host's compound cases. tests/net's ee_session_test needs a stand-in `ee_gamecpp` (its units' bodies call the gamecpp
host now) before its golden can run again. The sweep (build/core9/sweep.py) goes on as other lanes move callees.
