# The game core lane (was the game's leaves): src/Empire Earth/Empire Earth, Persistent, Database, Player, Tech Tree, Civilization Builder, Calamities, Communications, Graphics Thread, Map; src/RTS Engine; src/Low-Level Engine but its User Interface

The lane's state. Each round rewrites this file to the current state; each round's story goes in PORT_LEARNINGS.md
("The game's leaves, round 1/2", "The game core lane, round 3/4/5"). The census is `python -I tools/cpp_deps.py report`
(after a build: it reads the objects): `leaves.txt`, `near_leaves.txt`, `waves.txt`; `cpp_deps.py unit FILE` names
one unit's edges. Since batch 6 (agent rules "Lane scopes") the lane owns the directories above and nothing else.

## Moved

| C++ (deleted or shrunk) | Rust home | What callers keep |
|---|---|---|
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
- **EECalamityManager, EECalamityClass, EECalamity**: the bodies are Rust (calamity_class.rs, calamity_start.rs,
  calamity_update.rs, calamity_record.rs); the C++ left is the lists of C++ objects and their virtual calls, the
  units by id, the capture hooks. A Rust manager would need as much host as the C++ has now.
- **RTSWorld, RTSPlayer, RTSPhysics, RTSGraphics, RTSMouse** (1,600): C++ virtuals of the game's classes and their
  lists; the same.
- **GERasterizer's setters** (130 lines of forwarders): rust/render/ge.h is 3,300 lines and GERasterizer.h has 100
  includers; they need a header of their own (a module of ee_render's ge split for them). **DX7Rasterizer's
  forwarders** (130): two of the draws use a .cpp helper and DX7Surface; DX7Rasterizer.h would include dx7.h.
- **The calamity captures** (CALW, CALT, CONT, CCAS, CALI, CALK: EETechCapture.cpp, EECalamityCapture.cpp), **AEFX**
  (EEAECapture.cpp), **the app sweeps** (EEAppCaptureWorld.cpp), **the message capture** (EEMessageCapture.cpp): they
  read C++ members (EECalamity's locations and units, the area effects' C++ lists, the messages) or call C++
  methods (CanCastAt, GetCost, Modify, the cheats).
- **EEWorldLayout** (Units lane), **EEUnitStore** (Units lane), the media objects (EESoundObject, EEAnimal,
  EEButtonObject, the terrain tables: C++ media and texture objects), **EEVCWonder** (the wonders are not in the
  views), the message classes (their vtables are the network engine's).

## Next

EEPlayer shrinks with the classes it calls (Left, above): the towns' and herds' host cases into ee_sim::cp once the
world layout has EeTownFields' and EeHerdFields' base offsets; the selection methods and the groups' parts once the
groups are Rust objects; the improvement cases once EEUnitClassUpgrade is a Rust record. In the lane meanwhile: the
camera's forwarders over a host Rust names, GERasterizer's setters over a small rast header (ee_render ge split),
the calamity captures once EECalamity's locations and units are in its Rust record, EEPChecksum's load and save over
FSMemoryFile::Ops, the media objects as Rust objects.
