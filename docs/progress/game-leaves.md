# The game core lane (was the game's leaves): src/Empire Earth/Empire Earth, Persistent, Database, Player, Tech Tree, Civilization Builder, Calamities, Communications, Graphics Thread, Map; src/RTS Engine; src/Low-Level Engine but its User Interface

The lane's state. Each round rewrites this file to the current state; each round's story goes in PORT_LEARNINGS.md
("The game's leaves, round 1/2", "The game core lane, round 3"). The census is `python -I tools/cpp_deps.py report`
(after a build: it reads the objects): `leaves.txt`, `near_leaves.txt`, `waves.txt`; `cpp_deps.py unit FILE` names
one unit's edges. Since batch 6 (agent rules "Lane scopes") the lane owns the directories above and nothing else.

## Moved

| C++ (deleted or shrunk) | Rust home | What callers keep |
|---|---|---|
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

GERasterizer's setters over a small rast header (ee_render ge split); the calamity captures once EECalamity's
locations and units are in its Rust record; EEPChecksum's load and save over FSMemoryFile::Ops; the media objects
as Rust objects.
