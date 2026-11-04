# Bug fixes

This branch, `fixes`, is `main` plus fixes for bugs of the original game that keep it compatible with the original:
multiplayer games with players of the original game stay in sync, and saves go both ways. Simulation, AI, network
traffic and save files stay exactly the original's. What it fixes:

- crashes and memory-safety errors (out-of-bounds reads and writes, NULL pointers, use after free) on paths where the
  original either crashes or produces nothing observable;
- memory and handle leaks;
- dead code: tests that can never be true, values computed and never used, unreachable branches (removed or made
  to do what they evidently meant where that changes nothing observable);
- local display, interface and audio fixes: things only the local player sees or hears (drawing, interface controls,
  sound, error reporting, debug output), never anything sent over the network or written to a save;
- reads of uninitialised data made to read zero. The original reads memory that was never written, so it has no
  single behaviour to keep (the value is whatever the heap or stack held before). New memory pages are zero on every
  Windows, so reading zero matches the original wherever it read fresh memory, and is a defined choice everywhere
  else.

Every fix is one commit whose subject names the function it changes and the function's address in the original
image, so a fix can be found by name, for example `git log --oneline main..fixes --grep "EEWonNet::JoinGame"`.
Addresses without a module are in `EE-AOC.exe` (0x4xxxxx-0x6xxxxx) or `Low-Level Engine.dll` (0x10xxxxxx); others
name their module (`TCP Protocol.dll`, `Default.dll`, `Miles Sound System Mixer.dll`, `DX7HR` = `DX7HRDisplay.dll`,
`DX7HRTnL` = `DX7HRTnLDisplay.dll`, `LLE` = `Low-Level Engine.dll`).

## How it was tested

A `fixes` build (game, engine and plug-ins) played against the original game, compared tick for tick (simulation
and AI state at every game tick): single-player games with AI players to the end of the game, multiplayer games over
TCP/IP with an original host and a `fixes` client and the other way round (identical to the end, no out-of-sync),
and saves written by each loaded by the other and played on identically. The fixes in the DirectX 7 display plug-ins
and in the sound mixer were built and linked but not exercised in a running game.

## Classes

| Class | Meaning |
|---|---|
| crash/memory | a crash, hang or memory-safety error (out-of-bounds access, NULL pointer, use after free, missing error handling) |
| leak | memory or a handle that is never released |
| dead code | code that cannot have an effect as shipped: an impossible test, an unused result, an unreachable branch |
| local | a visible or audible change on the local machine only (display, interface, audio, error reporting, debug output) |
| zero-init | a read of uninitialised data, now reading zero |

## Fixes on `fixes` (246)

| Area | crash/memory | leak | dead code | local | zero-init | Total |
|---|---|---|---|---|---|---|
| Simulation and AI | 15 | 1 | 8 | 3 | 5 | 32 |
| Units, players, scenarios and economy | 22 | 1 | 6 | 14 | 2 | 45 |
| Random map generation | 0 | 0 | 1 | 0 | 0 | 1 |
| Network and multiplayer | 7 | 2 | 3 | 1 | 2 | 15 |
| Interface, audio, scenario editor and data files | 9 | 17 | 11 | 16 | 1 | 54 |
| Engine library and sound mixer | 8 | 5 | 13 | 20 | 1 | 47 |
| Rendering and graphic effects | 10 | 7 | 9 | 25 | 1 | 52 |
| **Total** | 71 | 33 | 51 | 79 | 12 | 246 |

### Simulation and AI (32)

| Function (original address) | What the original does | What the fix does | Class |
|---|---|---|---|
| EECPASGLand::FindCrossingTile (0x447f72) | Water mode indexed mIDsDC with mIDsBC's count, reading past DC's end. | An index past DC's end reads as id 0. | zero-init |
| EECPCombatManager::DefendUnit (0x47590f) | The kind filter's spaceship flag was left uninitialised when the target has no nearest town. | It starts false. | zero-init |
| EEComputerPlayer::GetObjectRegion (0x42e9e7) | With no path area around the unit it read the empty map's head node as the first entry and walked the tree head. | Returns region 0. | zero-init |
| EEComputerPlayer::UpdateGroups (0x43f3f1) | With exactly one resource center the gap-activation loop never met its end condition and walked past the vector. | The loop runs only with two or more centers. | zero-init |
| EEHerd::Chase (0x4a1efc) | The random waypoint index was clamped to the path's size, one past the last waypoint, and read past the end of the path. | That slot reads as the zero location. | zero-init |
| EEInfluenceTable::GetValue (0x40ff19, 0x481a71) | A player id outside the region's table (-1 from a route plan for a task without a player) read the heap header before the table, which on modern Windows wraps the path cost and can loop the path builder until it crashes. | Such a player's value is 0. | crash/memory |
| EEAIContinent::GetShoreInRegions (0x41dccf) | Read the path area's space-region/water-body id before testing the area for NULL. | The id is read only for an existing area. | crash/memory |
| EECPUGatherMilitaryUnitsCount::CountRequest (0x42b239), CountUnit (0x42b2b2), CountUnits (0x42b323) | The unit-type tests were &=-combined after the NULL test and so dereferenced a NULL class or unit. | They run only once the NULL and military tests passed. | crash/memory |
| EECPASGAir::CanReach (0x442000) | Measured each plane's fuel range from its container with no NULL check, so an airborne plane read its position through NULL. | Without a container it measures from the home base (as CountUnitsInRange does) and skips a plane with neither. | crash/memory |
| EECPAttackGroup::AssignLandTask (0x45502e) | After GetContinentByID it tested the continent id again instead of the returned continent and dereferenced NULL when the id no longer names one. | It tests the continent. | crash/memory |
| EECPCombatManager::AddAttackTask (0x471f16) | Read the target's complex unit outside the unit's NULL test, so a unit id that no longer resolves dereferenced NULL. | Such an id queues no task. | crash/memory |
| EECPResearchManager::ResearchCheapest (0x48a91e) | With nothing to research in the last epoch it called AddSOL on a NULL next epoch. | Nothing is credited when there is no next epoch. | crash/memory |
| EECPASGUAirBomber::Retarget (0x405db0) | A paratrooper plane's unload goal was neither given nor deleted when the plane cannot unload at the target. | The unused goal is freed (the plane keeps the reference the goal took, so unit reference counts stay as before). | leak |
| EECPPersonnel::Restore (0x487d12) | Each request's Restore result overwrote the previous one, so only the last request's error was returned. | Every request is restored and the first error is kept. | crash/memory |
| EECPASGLand::Load (0x44a186) and Save (0x44bab9) | In the newer format the avoided-tile count read/write was not guarded by the earlier result, so an earlier error was lost and Load resized the list from a count read after a failure. | The avoided tiles are read/written only when everything before succeeded. | crash/memory |
| EECPAttackGroup::Load (0x456cc8) and Save (0x458625) | The order-count read/write result replaced the sub-group loop's result and the task's Load/Save result was ignored (Load also created and referenced a NULL sub-group after a read error). | The first error is returned and Load stops at a failed sub-group. | crash/memory |
| EECPAttackSubGroup::Save (0x45f895) | The path count's write result replaced a unit write error. | The path is written only when the units were, and the first error is returned. | crash/memory |
| EECPResourceCenter::Save (0x48d306) | Every unit id write overwrote theResult without stopping, so a failed write was masked by a later successful one. | The loop stops at the first failed write. | crash/memory |
| EEComputerPlayer::Load (0x42fd50) and Save | The resource centers' Load/Save results were discarded and, in the oldest save format, the exploration manager's Load result replaced an earlier error. | These errors are reported. | local |
| EEHerd::Load (0x4a34dc) and Save (0x4a3e7a) | The unit count's read/write result replaced the result of the header fields, so an earlier error was lost and Load went on with partly read fields. | The first error is returned. | local |
| EEAIAStarGraphStorage::Clear (0x416394) | Each list-splice loop stopped on the last node before counting it, so the free-pool counters (diagnostics only) lost one node per spliced list. | The last node is counted. | dead code |
| EECPAttackSubGroup::RandomizeTaskLocation (0x45bcd0) | Returned false on every path, also after moving the task location. | Returns true when it moved the location (the only caller ignores the result). | dead code |
| EECPAttackSubGroupUnit::ApplyPrejudices (0x4659a0) | Searched the prejudices for one matching the target's class and discarded the result. | The dead search is removed. | dead code |
| EECPASGAir::SetDestination (0x442759) and SendUnitTo (0x442a47) | The same tile was tested for NULL twice and a parked plane got StartAction(kActionPrepareToMove) twice in a row (the second call changes nothing). | Each once. | dead code |
| EECPASGSpaceFighter::SetDestination (0x450433) | The same tile was tested for NULL twice. | Once. | dead code |
| EECPASGSpaceship::PickSplitUnits (0x44f236) | Built a multimap of the units ranked by AI kind and never read it. | The unused ranking is removed. | dead code |
| EEPathManager::DumpOutOfSynch (0x4a639b) | Both queue loops dynamic_cast the queued node itself to a machine type, so every search was dumped as an unknown pathfinder, and the graph loop read unit, tile and passes from mPaths. | The node's machine is used and the graph loop reads mAutoPaths. | local |
| EECPASGLand::AdvanceSubSector (0x449387) and the second route planner (0x449b5d) | The space-capable case cleared weights slot 15, one past the 15 path weights (the tile kind used as the slot), a stray stack write. | Clears the space weight slot 10, as GetPathCosts does. The wall-avoider modifier reads neither slot, so no path changes. | crash/memory |
| EECPASGLand::BoardTransports (0x44a3a9) | When the transport refused its EEUGLoadTransport goal the goal was deleted, then its tile was read and it was cloned for every unit (use after free: garbage rendezvous/goals or a crash). | The refused goal is deleted after the rendezvous tile and the units' copies are taken from it (where the original survives, the freed block still held the goal and the result is the same). | crash/memory |
| EECPITargetScoreGreater::operator() (0x42b0ba) | A category-0x13 target paired with another target compared 'greater' in both argument orders, and two 0x13 targets ordered by ascending score (not a strict weak ordering for the GetTargets sort). | 0x13 targets go first, each group by descending score. No retail dbobjects record has category 0x13, so shipped data is unaffected. | crash/memory |
| EEAIContinent::AddIDEC (0x418941) | The duplicate test compared find over mIDsEC with mIDsBC's end, never true, so mIDsEC stayed empty. | Compares with mIDsEC's end like its three siblings. Nothing reads the four id lists, so play is unchanged. | dead code |
| EEPathManager::FindBoundedPath (0x4a71c9) | Cleared *outReached and never set it, so callers always saw 'not reached'. | It is set to the returned reached flag. The only caller (EEUnitClass 0x636b69) does not read it, so play is unchanged. | dead code |

### Units, players, scenarios and economy (45)

| Function (original address) | What the original does | What the fix does | Class |
|---|---|---|---|
| EEMChangeFormation::ImportMessage (0x4bcb12) | A received Change Formation message left its formation flag uninitialised (only the selection and type are read), and Execute copied it into the new formation. | The received flag starts false. | zero-init |
| EEComplexUnit::EEComplexUnit (0x5f5999) | MDisabledReason (+0x358) was never initialised for a new unit and saved games stored the heap garbage. | A new unit starts with reason 0. | zero-init |
| EESMEffect::TaskUnitsToObject (0x5c2f55) and TaskUnitsToArea (0x5c42d2) | The NULL complex-unit guards tested the address of EEUnit::GetComplexUnit instead of calling it, so they always passed and a unit without a complex unit was dereferenced. | The guards call it. | crash/memory |
| EEGetSpaceDockYaw (0x63d008) | The second and third 2 x 2 loops read the terrain of an off-map tile through NULL (the counting loop checks it). | An off-map tile counts as space in all three loops. | crash/memory |
| EEComplexUnit::UpdateHidden (0x5f8357) | For a tile unit without a complex unit, the reveal test read the cloak and owner fields through the NULL complex unit. | Such a unit is treated as uncloaked and only its class decides. | dead code |
| EEUCMissile::CheckGroundImpact (0x617337) | A missile base's missile without a goal passed the entry test and its goal kind was read through NULL once its flight time ran out. | Such a missile reports no impact. | crash/memory |
| EEUGBuildRepair::Save (0x61f47f) | Wrote the building's id through mTarget without a NULL check, though ReplaceUnit clears it when an enemy takes the building over. | A cleared building is saved as id -1 (no building on load). | crash/memory |
| EEUGLoadContainer::Step (0x625f93) | On 'cannot reach container' it called RemoveUnit on the container's load goal without a NULL check. | It skips the container's goal when there is none. | crash/memory |
| EEUGPatrol::Step (0x628139) | Asked the patrol group for its point count before the group's NULL check, so a goal with a current point but no group crashed. | The NULL check comes first. | crash/memory |
| EEUGUnloadTransport::IsInUnloadRange (0x628683) | Tested the transport for NULL only in the teleporter test and then read its tile anyway. | A missing transport is reported as not in range. | crash/memory |
| EEUnitTrigger::GetTriggerName (0x62b837) and EEUnitAction::GetActionName (0x62c75a) | Find(id)->second with no end() check read the map's head node as a string for an unknown id (a misspelt name in a unit AI script). | An unknown id has an empty name. | crash/memory |
| EEUnitClass::UnloadButtons (0x638376) | The tech node's NULL test guarded only the button release and its data was then read unconditionally. | A technology missing from the player's tree is skipped. | crash/memory |
| EEUCGate::GetFourthPart (0x61175c), GetSecondPart (0x611735) and EEMLockUnlock::Execute (0x4c1fb6) | A gate created as a single piece by a scenario Create effect has no four-part chain, and Lock/Unlock took mPreviousPart of a NULL chain piece (access violation). | The part getters return NULL for a short chain and Lock/Unlock skips missing parts. | crash/memory |
| EESMEffect::SetUnitsStance (0x5c2811) | The lock/unlock stances used the gate's second and fourth parts without NULL checks, so a single-piece unit (a non-gate, or a created gate) dereferenced NULL. | Missing parts are skipped. | crash/memory |
| EESMCondition::Evaluate (0x5baabf) | The trigger-state condition read the trigger from GetTrigger without a NULL test. | A missing trigger counts as not fired and not on. | dead code |
| EEEvent::Trigger (0x5ca76c) and EEEvent::Get | The event id range test accepted 0..1500 for the 1500-entry event table. | It accepts 0..1499. | crash/memory |
| EEContagionHelper::RestorePointers (0x4b61ca) | Referenced and stored the unit returned by GetUnit without a NULL test, so a saved contagion holding a unit id that no longer resolves crashed on load. | Such an id is dropped. | crash/memory |
| EEMHackANewUnit::Execute (0x4bf543) | The ambient/resource class test read the class's category before the class's NULL check. | A unit type without a class returns first. | crash/memory |
| EEMLoadTransport::Execute (0x4c1b79) | With a passenger id it read gWorld.GetUnit(id)->mComplexUnit twice without a NULL check, crashing when the passenger was gone before the order ran. | A missing passenger is not loaded. | crash/memory |
| EEMSelectTheseUnits::Execute (0x4c5aaa) | Added gWorld.GetUnit(id)->GetComplexUnit() for every listed id without a NULL check, crashing when a unit died between sending and executing. | Missing units are skipped. | crash/memory |
| EEWorld::RecordStatistics (0x53a845) | The computer players' resource top-up read the best human's statistics with no NULL check, crashing when no human is among the recorded players. | The top-up is skipped then. | crash/memory |
| EEWorld::ReleaseResourceUser (0x538fe5) | Used mResourceUsers.find(unit) without an end() check, so releasing an untracked unit decremented the map's head node. | An untracked unit is ignored. | crash/memory |
| Fn1487A3 (0x5487a3) | When a graphic object's frame sound changes to a different sample, the reference taken on the old sound node at entry was never dropped, so the node leaked. | It is dropped, as in the same-sample branch. | leak |
| EEUGAttackUnit::Load (0x61d9dd) | The attacker id read's result replaced a failed base goal or target id read, and the goal loaded with a garbage target id. | The earlier error is returned. | local |
| EEUGLoadContainer::SaveContainer (0x6262f3) | Every passenger id write overwrote theResult unchecked, so only the last write's error was seen. | The loop stops at the first failed write. | local |
| EEUGUnloadTransport::Save (0x628990) | The results of the first-cargo and unload tile writes were ignored. | They are checked and returned. | local |
| EEUnit::Save (0x6299e5) | The result of RTSUnit::Save was ignored, so a failed write of the base unit part went unreported. | It is checked and returned. | local |
| RTSPlayer::SaveUnits (0x56f7b1) | Inside a bucket every SaveUnit result overwrote theResult and only the last was checked. | The bucket's units stop at the first failed write, which is returned. | local |
| EEEnvironmentManager::Load (0x52ef75) and Save (0x52f0ff) | Both returned success whatever the storm, state, wind and timing reads/writes returned. | They return the first error. | local |
| EETeam::Load (0x536ca3) | The player count/list reads' error was discarded (and the list resized by an unchecked count); only the victory condition's result was returned. | The first read error is returned. | local |
| EECivilization::Save (0x52c9ee) | The upgrade loop overwrote theResult on each write, so only the last upgrade write's status was checked. | The loop stops at the first failed write. | local |
| EEUCNavalPriest::AdjustPath (0x61884e) | I % theStep with theStep the rounded inner collision sphere had no zero clamp (its naval citizen twin has one), an integer divide by zero. | A zero step keeps every point. | crash/memory |
| EECCPlague::Initialize (0x4b3005) | Tests for class 19 (AE Mind Control), which is always built as EECCAEMindControl, never as a plague. | Removes the dead test. | dead code |
| EEPhysics::DrainInput (0x55355c) | A non-tick message at the head of the server queue made the loop continue without popping it, spinning for ever with the queue lock held. | Such a stray message is dropped. | dead code |
| EEUGExplore::FindNextTile (0x62155e) | The IsSeenBy arm of the seen test is unreachable (the ring search runs only while mNoUnexploredAreas is false). | Removes the unreachable arm. | dead code |
| EECheatCodes::AddResource (0x52a669) and its siblings (ExploreMap, Refuel, HealUnits, RestorePower, MakeWorldVisible, ResearchAll, AdvanceEpoch, ToggleInstantBuild, Clear/SetPhysicsFlagForPlayer, ClearResource) | The player check was inPlayer <= mNumPlayers, one past the last player. | InPlayer < mNumPlayers, as RevealMap has it. | dead code |
| EEUGCastCalamity::GetDebugText (0x620dd6) | The location/type line was passed the source id, target id, tile x, tile y and type for a three-value format. | It prints the tile and type. | local |
| EEUGHealUnit::GetDebugText (0x625432) | The health percentage printed the raw fraction (0.50%). | Multiplied by 100. | local |
| EEUGPatrol::GetDebugText (0x628044) | The point list started at GetNext(GetLast()), past the end, so no point was listed and the early return skipped the closing ' )'. | Every point is listed. | local |
| EEWheel::Update (0x63c8ff) | The wheel angle was wrapped with += pi, not 2 pi, so the wheel jumped half a turn at each wrap. | It wraps by a full turn (the angle is read only by rendering). | local |
| EECCTimeWarp::PlayEffect (0x4b3ede) | The departing branch played mSecondarySound into the calamity's main sound slot, so the time warp's main sound was never heard. | The departure plays mSound (the secondary sound when there is none). | local |
| EESMEffect::Execute (0x5bd5eb) send chat message | A chat message with duration 0 was shown in a message box with theMediaText, which only the Change Text action fills (an empty box). | The box shows the chat text. | local |
| EEUGUnloadTransport::Save (0x628990) | Wrote the transport's id through mTransport without a NULL check, though RepairPointers clears it when the saved transport no longer exists. | A cleared transport is saved as id -1 (no transport on load). | crash/memory |
| EEPlayer::PayImprovement (0x56b3f7) | Read inClass->mUnitType before its NULL test. | The class is tested first. Every caller passes a class, so nothing changes in play. | crash/memory |
| EEMSetRallyGoal::Execute (0x4c6dca) | A selected unit with a single goal queue (a ship that can set a rally point) took the building branch, which clears and fills goal queue 1, past the end of its queues (crash in EEUnitVectorItem::Clear 0x6057a7). | Such units are skipped (the original crashes or writes past the queues there). | crash/memory |

### Random map generation (1)

| Function (original address) | What the original does | What the fix does | Class |
|---|---|---|---|
| EvaluateCondition (Default.dll 0x100111dc) | The unknown-keyword test (< DEFINED && > NOTDEFINED) was never true, so unknown keywords fell into the DEFINED/NOTDEFINED block. | They take the unknown-keyword branch (result 0 either way). | dead code |

### Network and multiplayer (15)

| Function (original address) | What the original does | What the fix does | Class |
|---|---|---|---|
| EEWonNet::JoinGame (0x4cede5) | Returned with the routing data lock held when the group map was empty. | Releases it on that exit too. | leak |
| EEWonNet::ListRoomCallback (0x4cda04) | Every room description was read from the past-the-end group left over from the group loop. | Takes it from the room's own data object text. | dead code |
| EEWonNet::SendChat (0x4d0a14) | /r tested the reply target against 0, but no target is 0xffff, so /r before any whisper whispered to client 0xffff. | Tests against 0xffff. | dead code |
| EEWonNet::FnCFFF2 (0x4cfff2) | Tested mLobby but called through mLobbyWindow. | Tests the window it calls. | dead code |
| EEClient::HandleMessage connection response (0x4caad8 host side) | A host refuses a join into a running game with code 0x2751 (a string id) instead of 9, and the joining branch handled neither, so the joining player was never told. | The joining client accepts both codes and shows 'The Game Is In Progress'; the host still sends 0x2751 so the wire is unchanged. | local |
| EEGetFileChecksum (0x53f736) | The per-file read loop never ended when a read returned no data (and an unopenable file was read anyway), hanging the start-up network checksum. | Skips a file that cannot be opened and stops at a read that returns nothing. | crash/memory |
| LobbyRebuildPlayers (0x5dc6be) | Took an entry from the non-human list for every saved slot without checking that one was left, reading and writing before the list's start when a saved player matched no joined human. | Stops when the list runs out and fails the load with 9. | crash/memory |
| NESynchronousCommunications::GetCurrentReceiveBytesPerSec (0x10047375) / GetCurrentSendBytesPerSec (0x100473bc) | Divided by the milliseconds since the previous call with no zero check, crashing on two calls in one millisecond. | Report 0 and keep the previous baseline. | crash/memory |
| NEProtocolConfiguration::Load (0x10045037) | Freed the parameter array but left the pointer (and possibly an overwritten count) behind when the load failed early, so the destructor or the next Load freed it again. | Clears the pointer and count after freeing. | crash/memory |
| NPIMConnect (TCP Protocol.dll 0x1000153c) | A failed bind returned success without setting the connection and leaked the socket. | Records the error, closes the socket and returns kNPIMNativeError. | crash/memory |
| FindAdapterAddressIndex (TCP Protocol.dll 0x10001adb) | Freed the walked adapter pointer instead of the GetAdaptersInfo buffer (interior-pointer free for adapter 2+, leak when the walk ran off the list). | Frees the buffer start. | crash/memory |
| NPIMEnumerateNetworkAdapters (TCP Protocol.dll 0x10001bcd) | Freed the walked adapter pointer instead of the GetAdaptersInfo buffer (leak on every call, interior-pointer free with more than 10 adapters). | Frees the buffer start. | crash/memory |
| Encipher (TCP Protocol.dll 0x100023a7) | Returned with gCipherLock held when there was no key buffer or the key was 0. | Releases the lock on that exit. | leak |
| NPIMReceiveFrom (TCP Protocol.dll 0x10002539) | A join packet from an unknown sender while new connections are not allowed returned OK without setting the received size or client index. | Both are set to zero. | zero-init |
| NPIMSendSingle (TCP Protocol.dll 0x10002aab) | Copied the destination from the remote host's address list at the local adapter's index, reading past the end of a shorter list (NULL entry: crash; later: garbage). | An entry the remote list lacks gives address 0. | zero-init |

### Interface, audio, scenario editor and data files (54)

| Function (original address) | What the original does | What the fix does | Class |
|---|---|---|---|
| EEAmbientSoundManager::UpdateAnimals (0x52580b) and UpdateTerrain (0x525fcc) | Ambient sounds 10, 14, 18 and 12 played when mProbability < random, chance 1 - p, while every other ambient sound plays when random <= mProbability. | Chance p. The manager has its own random generator, so only local audio changes. | local |
| EECheatCodes::ListCheatCodes (0x52a68c) | Line 3 did not reload its first slot and line 4 has only four codes but reused the fifth, so 'boston food sucks' and 'boston rent' were listed twice. | The unused slots are empty. | local |
| RTSMouse::LoadPointer (0x51af66) | Used the file from FindFile (Load, Close) before its NULL check, so a missing pointer texture crashed, and returned Close's result ignoring Load's. | A missing file returns an error and a failed load is reported. | local |
| EESEAPlaceUnit::Place (0x5a98a2) | A building ghost at the map's low edge converted its negative corner tile to a huge unsigned coordinate and dereferenced the NULL tile. | Such ghosts are skipped. | crash/memory |
| EESMMediaStrings::GetMediaFile (0x5c5828) | A missing media file's name was sprintf'd into the error string's own buffer with no capacity check (heap overrun for long names). | The error string is assigned. | crash/memory |
| EEUIESaveScenarioTriggersOnly::ProcessEvent (0x59ccc0) | The looping-trigger performance check dereferenced a missing effect (NULL) that the listing above reports as BAD EFFECT. | Missing effects are skipped. | crash/memory |
| EESETSObject::Activate (0x591d63), EESETSCTPlayerResearch::Show (0x5802a4), EESETSETPlayerTechnology::Show (0x58adb4) | The class / technology lists were built only while a run-wide static flag was clear, but their owners are recreated with every scenario editor, so a second editor session in one run had empty combo boxes and Store indexed the empty lists. | Each list is built when its own (per-editor) vector is empty. | local |
| EEPPlayerHelper::Load (0x54b4e1) | A saved player of an unknown type gets no player from CreatePlayer and Load called through NULL. | The load fails with an error. | crash/memory |
| EEGameStateLoader::RestorePlayers (0x54a7ab) | When hosting it wrote the slots of thePlayers[1] and thePlayers[theLocal] even after reading the players failed (empty vector, theLocal unset). | The slots are set only after a successful read and for players that exist; the dead first store of mNumPlayersMade in the editor branch is removed. | crash/memory |
| LoadTeams (0x54c530) | Resized the team vector to the count even when reading the count failed (an unset local: a huge or garbage resize on a truncated save). | It returns the read error first. | crash/memory |
| EEButtonObject::SetFaceBitmap (0x4d584e) | Read the texture's width, surface and mip drop before its NULL test, so a button whose texture failed to load crashed when drawn. | The test comes first. | crash/memory |
| EEButtonObject::Reload (0x4d5c31) | The mip drop was 'width > 800 ? 0 : 0', so on screens 800 pixels wide or less reloaded button textures came back at full size unlike the first load. | One level is dropped there, as LoadTexture does. | local |
| EEInitializeGraphics (0x54265e) | The six bold Arial font checks tested the regular Arial fonts (copy-paste), so a failed bold font went unreported. | Each check tests the bold font just created. | crash/memory |
| EELoadGameFromCommandLine (0x52cb90) | Deleted gCommunications at entry without clearing it, then deleted it again before creating the server (double delete when a communications object existed). | The pointer is cleared after the first delete. | crash/memory |
| EEUIFSETerrainPaint::CreatePaintAction (0x5ab344) | Without a color selection the terrain's color type id indexed the filtered color list and could read past its end. | A position past the end gives color id 0 (in-range positions unchanged). | zero-init |
| EESMArea::ContainsUnit (0x5ba0d1) | Read the unit's tile before testing the unit for NULL. | The tile is read after the test. | dead code |
| EEUISCIFormManager::OpenScenario (0x5e7444) | Read the scenario file's name before its NULL check. | Returns false when no file is set. | dead code |
| EEScenarioManager::Load (0x5b404a) | The dropped 'object 0 exists' conditions were never deleted (leak per scenario load). | They are freed. | leak |
| EESEGamePage::~EESEGamePage (0x577b83) | Deleted the 120 diplomacy buttons but never the pointer array the ctor allocated (480-byte leak per editor player tab). | The array is freed. | leak |
| EESETriggerScreen::~EESETriggerScreen (0x57bf97) | Never deleted the description label and edit box the ctor created (two controls leaked per scenario editor). | Both are deleted with the other controls. | leak |
| EESETSETGameCalamity::Show (0x584db5), EESETSETGameGraphicEffect::Show (0x585790) | Appended every listed id to mCalamities / mGraphicEffects on each Show without clearing them (the vectors grew without bound). | Each is cleared with its combo box. | leak |
| EESETSETTrigger::Show (0x58eb78) | Attached a new change-effect event to the trigger combo without dropping its creation reference (SetTriggerEvent takes its own), so every Show leaked one event. | The creation reference is dropped. | leak |
| EESEIAImport::Virtual4 (0x570345) | Returned after the error message without closing the .lcl file when it was empty or its version header was missing or too new (the handle leaked). | The file is closed on the error paths too. | leak |
| EESEASaveScenario::Virtual4 (0x5aa816) | Created a scenario save screen event first and then overwrote the pointer with the one it posts (one event leaked on every editor Save). | Only the posted event is created. | leak |
| EESEACreateBlankMap::CreateMap (0x5a5ae7) | Never freed the elevation file's height buffer (new short[width * height]; leaked on every blank map made from an elevation map). | It is deleted on both exits. | leak |
| EECampaignProgress::RestoreCivilization (0x68f56d) | Released its FSPath only when a civilization was extracted (leak per user-campaign scenario start). | It is released on every path. | leak |
| EECampaign::ListEffectSounds (0x68d219) | Deleted the loaded scenario manager but never the EEPTPRestorable wrapper EELoadPersistent created (leak per scenario when listing or saving a campaign). | The wrapper is freed. | leak |
| EECampaign::CopyScenarioFiles (0x68c122) | A failed copy broke out of the loop before 'delete theSpec' (leak on a failed campaign save). | The spec is deleted first. | leak |
| EEDatabase::Load (0x4d60ae) | A record that failed to load broke out of the loop without being stored or deleted (leak on the read-error path). | It is deleted. | leak |
| SaveCampaign (0x5d022c) | Posted a new save-continued event without dropping its creation reference (PostEvent adds its own; every other caller dereferences), so one event leaked per campaign save. | The creation reference is dropped. | dead code |
| EEUIECSSEStartScenario::ProcessEvent (0x5d274a) | The FSFileSpec from FindFile was handed on only when the scenario header loaded and never deleted otherwise (leak per failed scenario start). | It is deleted on the failure path. | leak |
| EEUIECCCEFormOpen::ProcessEvent (0x5d3a77) | Never released the FSPath it creates for the campaign directory search (leak each time the custom campaign form opens). | It is released after the listing. | leak |
| EEUIEPCECustomCampaign::ProcessEvent (0x5e2866) | Installed new form open/close events without dropping their creation references (the form takes its own), so both leaked each time Custom Campaign was used. | The creation references are dropped. | leak |
| EEUIESIEOpen::ProcessEvent (0x5e7d65) | For a scenario with no media it queued a new start event without dropping its creation reference and never released the movie/briefing event it had created (two events leaked per such scenario). | Both creation references are dropped. | leak |
| EEUIESPLELoadFile::ProcessEvent (0x5e900e) | The FSFileSpec from EEOpenSaveGameFile was deleted only on the valid-file path, so an unreadable or invalid saved game leaked the spec and kept its file open. | It is deleted on the failure paths too. | leak |
| EESEPlayerScreen::Enter (0x575c48) | After checking player page 1 (which unchecks the other tabs) it still tested the tech tree and AI tabs, whose dead branches switched shifted groups. | The dead branches are removed. | dead code |
| EEScenarioManager::RenumberEffects (0x5b2d62) | After erasing a timed entry the loop ran to the cached old count and read a stale slot past the vector's end. | The loop is bounded by the current size. | dead code |
| EEScenarioManager::Save (0x5b4d1a) | Each section's count write replaced the previous section's error and mLastChatText's result was dropped, so a mid-save write error could be reported as success. | The first error is returned (the bytes written are unchanged). | local |
| EESMCondition::Save (0x5bb3b6) | Ignored the results of the text and chat text writes, so an error there went unreported. | Both results are checked like the other fields (Load is unchanged). | local |
| EESETSETCreateAIMilitaryGroup::VirtualC (0x583904) | Cleared the object lists' button-down event while Show attaches the add/remove handler to button-up, so the handler stayed attached to the hidden lists. | Button-up is cleared. | dead code |
| EESETSETAI::VirtualC (0x58301c) | Hid mPlayers[2] (never shown on this page) but not the 'Player' label mLabels[2] that Show displays, so the label stayed on screen after switching to another effect type. | The label is hidden too. | local |
| Fn14DA7C (0x54da7c) | Wrote its error texts to the global gLoadSaveErrorMessage instead of its outErrorMessage parameter (correct only for callers passing the global). | It writes outErrorMessage. | dead code |
| EEValidateSaveGameFile (0x54d946) | On a failed header check the generic 'save game invalid' text always overwrote the specific message just written (incompatible version / missing data / invalid data). | The generic text is used only when no specific one was set. | local |
| EEPMultiplayerHelper::Save (0x54aff1) | The branch for version 0.12 (counters without physics) could never run, as 'major <= 0 && minor <= 12' returns first. | The dead branch is removed. | dead code |
| EEInitializeSounds (0x543557) | The success path ended without a return value, so EEInitializePart1 tested whatever AL held after the last destructor. | It returns true. | dead code |
| EEUIEMSEMultiPlayer::ProcessEvent (0x5e1f8a) | EECheck2989A9(false) always returns true, so the branch opening the in-game multiplayer form was dead. | The dead branch is removed. | dead code |
| Fn2802EF (0x6802ef) | Summed a checksum of the civilization name and upgrades that was never written or used. | The dead computation is removed (the .civ file is unchanged). | dead code |
| EEUIELEOpenLoadScenarioGame::ProcessEvent (0x5da184) | The scenario list preselected gLoadSavedGameName (the last saved game). | It preselects gLoadScenarioName, the last scenario. | local |
| EESETSEScenarioComboBox::MouseRButtonUp (0x57add4) | On a condition combo the jump to the object tab checked the condition radio and the jump to the condition tab checked the object radio. | Each jump checks its own tab's radio. | local |
| EECalamity::DumpOutOfSynch (0x4ad377) | Printed 'Bool PlaceHolder 1' two more times after the other flags (copy-paste; the class has no further flags). | The duplicate lines are removed from the out-of-sync dump. | local |
| EEUIBColor::SetColor (0x6548ce) | The wrap below 0 tested 'mColor < 0' on an unsigned colour and never fired, so right-clicking colour 0 (0 - 1 = 0xffffffff) wrapped to 0 instead of 15. | One below 0 wraps to 15. | local |
| EEUIBSavedGroup::Refresh (0x65607c) | Tested mHasUnit after SetUnit(NULL) had cleared it, so a saved-group button whose group emptied was never marked dirty and could keep the old portrait. | The flag is read before SetUnit. | local |
| EECBUIListBox::SetUpgradesExcept (0x67e2bb) | Called std::remove_if without erasing the returned tail, so the vector's leftover last entries (duplicates or already-owned upgrades) were listed too. | The tail is erased. | local |
| EEUpdateSavedGames (0x52ce0c) | Deleted each original saved game and scenario after its conversion whether it succeeded or failed, losing the file on a failed conversion. | An original is deleted only after a successful conversion. | local |

### Engine library and sound mixer (47)

| Function (original address) | What the original does | What the fix does | Class |
|---|---|---|---|
| SESoundManager::GetVolume (0x1005034f) | Entered the sound lock twice and never left it. | Leaves it after reading the volume. | leak |
| SESoundManager::SetCategoryPercentage (0x1005154b) | Created categories with an uninitialised priority. | Priority -1 (lowest), as Load creates them. | local |
| ESEndRecording (0x1000161d) and ESShutdown (0x100019cc) | Closed the error log only in debugger-only mode, where it is never opened, so the log stayed open. | Close it whenever it is open. | leak |
| FSFileSpec::GetLengthFromClosedFile (0x10009627) | Left outLength unset when the file could not be opened. | Length 0. | zero-init |
| ESProfile::GetProfileInformation (0x10003a01) | Returned true for a missing profile. | Returns false. | local |
| FSArchive::ListFiles (0x10005b81) | Never advanced its iterator, so it looped forever on a non-empty archive. | Advances it. | dead code |
| FSArchive::WriteFAT (0x100061d9) | Ignored every write error and returned kErrorOK. | Returns the first write error. | local |
| FSChunk::SavePersistentToChunk (0x10007fce) | Reported a failed persistent save as success. | Returns the save's error. | local |
| FSDebugFile::ReadText(unsigned char&) (0x100083ec) | Fscanf %u stored four bytes into a one-byte local, overwriting the saved frame pointer. | Reads into an unsigned int. | crash/memory |
| FSDebugFile::ReadPossibleEOL (0x10008c87) | Tested fscanf's conversion count, always 0 for a line-end format, so it always failed. | Fails only on a stream error. | dead code |
| FSMemoryFile::Close (0x10009fbe) | Kept the closed temporary stream pointer, so a second Close closed the same FILE again. | Clears it. | dead code |
| FSMemoryFile::Decompress (0x1000a20b) | Leaked the output buffer when decompression failed. | Frees it. | leak |
| ISKeyboard::Refresh (0x1003b099) | An unsigned char event counter compared with up to 256 buffered events looped forever at exactly 256. | Unsigned long counter. | crash/memory |
| ISDevice::~ISDevice (0x10039791) | Left the destroyed device in ISDevice::sDevices (dangling pointer for GetDevice). | Removes it. | crash/memory |
| SPIMSetChannelSample (Miles Sound System Mixer.dll 0x10001871) | Read the sample's data before testing it for NULL, so a NULL sample (stop the channel) faulted. | Reads it only for a sample. | dead code |
| SEConductor::GetCurrentlyPlayingScore (0x1004cb52) and EventHandler (0x1004c973) | Never released the segment reference GetSegment adds, and the lookup dereferenced a score whose load failed. | Releases it and skips missing scores. | leak |
| SEConductor::InitializePorts (0x1004cf65) | The port enumeration looped while SUCCEEDED, but EnumPort ends with S_FALSE, so it never ended without a DLS output port (and a NULL port was then used). | Stops at S_FALSE and fails cleanly. | crash/memory |
| SEConductor::LoadScore (0x1004d1d5) | A failed segment download leaked the segment and returned success with outID unset. | Releases it and returns an error. | local |
| SEConductor::SetIntensity (0x1004d5de) | While the music was not running, the requested intensity was replaced by 0. | Keeps it for the start. | local |
| TMPoll::TMPoll (0x1005384f) | Without a performance counter the delay became inTimeDelay / divisor / 1000 (0), and Poll divided by it. | Converts only microsecond delays to milliseconds, at least 1. | dead code |
| TMPoll::Resume (0x10053a56) | Added the paused period's elapsed part to the start time, so the period restarted. | Subtracts it (in counter ticks), so the period continues. | local |
| TSThread::StartThread (0x100544cd) | Returned kErrorOK after 100 failed CreateThread attempts, with no thread handle. | Returns kErrorNativeError. | crash/memory |
| UGGetValueAt (0x10057919) | Stored the found point's y, then overwrote it with the next point's. | Keeps the found point's value. | local |
| URegistry::SetRegistryValue (0x10082443, 0x1008257a) | Opened the key and queried the old value in one test, so a missing value (or a string over 500 bytes) failed and leaked the opened key. | Sets the value in the opened key and always closes it. | local |
| UWideString::InsertString (0x1008436b, 0x10084498) | When the result did not fit, the capacity was set to the free space instead of the needed size, so the copies overflowed the new buffer (and the old one was freed with scalar delete). | Allocates the needed size, frees with delete[]. | crash/memory |
| UString::Copy(const char*, ...) (0x1008319d) | Clamped the copy to the free space without room for the terminator, so a copy that filled the buffer wrote one byte past it. | Grows the buffer by that byte, keeping the copied text. | crash/memory |
| UICheckBox::SetDisabled (0x1006009b) | Showed the box pressed when enabling as well as disabling. | An enabled box returns to the normal look. | local |
| UIButton::ButtonUp (0x1005e832) | Tested the button-up sound but played the button-down sound. | Plays the button-up sound. | local |
| UIButton::UpdateKeyboard (0x1005eea6) | Operator precedence made an Enter release click the button even without a key press on it. | Requires the key press for both Space and Enter. | local |
| UIControlsTable::CreateButton (0x10068137), CreateCheckBox (0x100683ca), CreateRadioButton (0x100690f0) | Gave SetButtonUpSound the button-down sound name. | The record's button-up sound. | local |
| UIFileListBox::DeleteSelectedFile (0x1006fe5f) | Appended the valid extensions cumulatively (name.e1.e2), so a file with any but the first extension could not be deleted. | Tries the name with each extension. | local |
| UIFileComboBox::DeleteSelectedFile (0x1006f848) | Appended extensions to listed names that already carry theirs, so it never found the file. | Deletes the listed file. | dead code |
| UIEditBox::Paste (0x1006c9f3) and Copy (0x1006b122) | Paste leaked its copy of the clipboard text (and added an uninitialised buffer when GlobalLock failed), Copy leaked its block when OpenClipboard failed. | Frees both, adds text only when locked. | leak |
| UIFontTable::DeleteAll (0x100702d2) | Released every shared font but kept the shared font list. | Empties it. | dead code |
| UIForm::GetControlById (0x100724c6) | With no matching control it returned mControls[-1], the heap word before the array. | Returns NULL. | dead code |
| UIGraph::CopyData (0x10075f8a) | Measured the grid-value margins with the axes and formatters swapped. | The height from the X values, the width from the Y values. | dead code |
| UIManager::AddToVisibleForms (0x10079e1f) | With no visible forms it compared back() of the empty list (a read before its start). | Adds the form directly. | dead code |
| UIManager::GetFormByID (0x1007a72d) | An unknown ID returned the word one past the end of the form list. | Returns NULL (CreateBackgrounds skips a background without a form). | crash/memory |
| UIManager::CreateBackgrounds (0x1007a216) | The tiled-background case passed the row ID instead of the background ID. | Passes the background ID. | local |
| UIManager::StartMusic (0x1007b616) | Stored the track name as given but compared lower-cased names, so restarting the current track with a mixed-case name reloaded it. | Stores the lower-cased name. | local |
| UILayoutUtility::OffsetRight (0x1007820b) | Took the direction from the left edge's offset type. | Uses the right edge's. | local |
| UIScrollBar::UIScrollBar (0x1007ca21) | A precedence slip sized the bar's rectangle 19 x (length - 1) for both styles. | A horizontal bar is (length - 1) x 19 (vertical bars unchanged). | local |
| UISlider::UISlider (0x1007d259) | A horizontal slider's last pixel was floor(right), without the + 1 of the vertical case and SetLocation. | Floor(right) + 1. | local |
| UILineGraph::DrawSeries (0x10078793) | The clip tests had empty bodies, so segments outside the displayed range were drawn outside the graph. | Clips each segment to the range. | local |
| UICreateMultilineTextFromString (0x100829f5) | UIGetStringCutOffIndex stops before a newline without counting it, so text with a newline after the first line looped forever pushing empty lines. | Skips the newline that ends a line. | dead code |
| FSFileSpec::SetPosition (0x10009c9f) | The past-the-end guard tested mFileAccess \| kFileAccessRead (always true) and ignored inExtend, so no seek could extend a file. | A seek past the end is allowed when inExtend is set on a file opened for writing (no caller extends a writable file). | dead code |
| FSPath::GetFileArchiveStatus (0x1000adfa, and the wide-name overload 0x1000aea1) | Returned false on every path, also after finding the file and setting outArchived. | Returns true when the file was found (the only caller ignores the result). | dead code |

### Rendering and graphic effects (52)

| Function (original address) | What the original does | What the fix does | Class |
|---|---|---|---|
| GEModel::RemoveModelScale (0x1001fc00) | Read the vertex normal back (GetNormal) where it meant to store the scaled one. | Stores it with SetNormal. | local |
| EEE3DTimeWarp::Save and EEE3DTeleport::Save (0x4fc297, one body shared by both) | The major and minor save versions were passed to EEEffect3D::Save swapped. | Passed in order (the base ignores them, so saves are unchanged). | dead code |
| RTSGraphics::HandleMessage (0x51a4e1, message 0x40d) and Render (0x51a71c) | Both released mMovieDoneEvent without clearing it, so a stop after the movie had ended in Render posted and released the already released event again. | The event is cleared after its release. | local |
| EEE2DMarchingIcons::Step (0x4e41ef) | The town center/capitol branch called HasUnitType on the player's active group without the NULL check the line above makes. | The branch requires an active group. | crash/memory |
| EEGraphicalGoal::Update (0x5076f9) | Moved the marker's graphic effect without checking that it exists (it is not created when its detail level is above the effect detail setting). | A missing effect is skipped. | crash/memory |
| EEROUnits::DrawObjectBars (0x5157b6) | Projected only the object's scaled bar offset, never adding the object's location, so a selected map object's bar was drawn near the map origin, and the back part of a partial bar was shortened by the filled width twice. | The offset is taken from the object's location and the back part runs to the full bar width. | local |
| EEROUnits::DrawObjectSelection (0x515b63) | The Draw call sat outside the 'mSelectionSize > 0' test that sets the ring's transform and scale, so a selected object without a ring size redrew the previous ring. | The ring is drawn only inside the test. | local |
| EEE2DImpact::Step (0x4e166b) | A timed impact created at a point (no unit) read mUnit->mHitPoints and followed a NULL unit's tag points, and its life fraction was never initialised. | The unit tests are skipped without a unit and the point constructor starts the life fraction at zero. | crash/memory |
| EEE2DPulseWave::DrawPass6 (0x4e5dc6) | Past 90% of its life the wave's light was (1 - age) * 2 of the fog value, an abrupt drop to 20%. | (1 - age) * 10, a continuous fade to zero. | local |
| EEE2DVortex::DrawPass5 (0x4e7949) | The vortex was drawn only from 90% of its life on, so it was invisible until it faded out. | It is drawn for its whole life and fades over the last 10%. | local |
| EEE2DVortex point constructor (0x4e777f) | The orbital count, frame and age were left uninitialised, so Step and DrawPass5 looped over a heap-garbage count of an empty orbital vector. | They start at zero. | crash/memory |
| EEE3DDamageSmoke::Step (0x4ea75c) | The unit's type-0 tag list was scaled before its null test, a member call on a NULL list for models without such tag points. | The list is scaled only after the test. (Scale only touches its transformation argument, so the original did not crash here; the call is now well-defined.). | crash/memory |
| EEE3DDisease::Load (0x4ec3aa) | The age was read even after a failed time or frame read and its result replaced the error, so a failed load could report success. | The age is read only after the earlier reads succeed, and the first error is returned. | crash/memory |
| EEE3DDisease::Step (0x4ec4d7) | The animation-start test compared the frame with animation 0's start frame while the cloud plays animation 13, so the first-frame sound could be missed or misfire. | It compares with animation 13's start frame. | local |
| EEE3DHurricane::Step (0x4f1d90) | The looping sound was positioned at a never-set (zero) point, the map origin, and the animation-start test used animation 0's start frame while the hurricane plays animation 13. | The sound is placed at the unit (as EEE3DDisease::Step does) and the start test uses animation 13. | local |
| EEE3DVirus::Step (0x4fe328) | The frame sound was positioned at a never-set (zero) point, the map origin, and the animation-start test used animation 0's start frame while the virus plays animation 13. | The sound is placed at the unit and the start test uses animation 13. | local |
| EEE3DGoal/EEE3DIcon/EEE3DMarchingLine/EEE3DStorm/EEE3DRain/EEE3DSnow::Save (0x4f15d0, 0x4f2a57, 0x4f3f56, 0x4fb83f, 0x4f7d7f, 0x4fa663) | The major and minor versions were passed to the base Save swapped (Goal/Icon and Rain/Snow/Storm swapping twice), and the marching line's from-unit id write result was overwritten by the next write. | The versions are passed in order everywhere and a failed id write is returned. The base Save ignores the versions, so saved data is unchanged. | dead code |
| EEE3DIcon::Clear (0x4f238a) / SetUp (0x4f23e2) | Clear emptied the orbital list without deleting the orbital copies, and SetUp dropped a previously loaded graphic without UnloadMedia. | Clear deletes the orbitals and SetUp releases the earlier graphic's media after loading the new one (the unit constructor now starts with no graphic). | leak |
| EEE3DTimeWarp constructor (0x4fbec6) | The age (+0x84) was never initialised, so the warp's lifetime depended on the memory's previous contents. | It starts at zero, as EEE3DTeleport::SetUp does. | zero-init |
| EEE3DVolcano::DrawPass4 (0x500a47) | After drawing, the shared viewport's depth range was left at 0..0 (every other effect restores 0..1), so later draws in the frame could run with a collapsed depth range. | The depth range is restored to 0..1. | local |
| EEE3DRain::AddDrop (0x4f75b7) | The map tile's terrain type was read before the tile pointer's null test. | The water test (with its null check) comes first and the ground height follows from it. | dead code |
| EEE3DTracerBeam unit constructor (0x4fc3c5) | MTarget was null-tested before Reference() but then dereferenced unconditionally for PickPoint. | The target point is picked only when there is a target. | crash/memory |
| EEOrbital::Init (0x608ce6) | IsOrbitalModel accepts the orbital_cw_/orbital_ccw_ prefix ignoring case, but Init parsed the speed only after a case-sensitive test, so a mixed-case orbital name orbited at speed 0. | Init ignores case too. | local |
| EEROGroundShadows::DrawUnitShadows (0x5122bb) | The height fade 1 - (z - ground) * 0.1 was clamped only above, so a unit more than 10 units above the ground got a negative, mirrored and growing shadow. | No shadow is drawn once the fade reaches zero. | local |
| EETechTreeNode::UnloadMedia (0x5cfcc9) | For an area-effect research effect (kind 2, attribute 0x28) it called gAEManager.LoadTemplateMedia, the same call as LoadMedia, so unloading loaded the template media again. | It calls UnloadTemplateMedia. | dead code |
| EETechTreeEpoch::CheckUnitMedia (0x5ce53f) | It walked the nodes that need unit media and computed each one's media mask with EETechTree::GetUnitClassMediaMask, then dropped the mask, so it had no effect. | The dead computation is removed. | dead code |
| EEUIBMapBackground::LoadBackground (0x6556e6) | When loading the campaign map image failed, the created system-memory image surface was never dereferenced. | It is released on every path. | leak |
| DX7Rasterizer::Draw3DLine (DX7HR 0x10003ea7, shared by DX7HRTnL) | For an empty point vector the unsigned vertex count size * 2 - 2 wrapped, was capped at 0x960, and 1200 point pairs were read past the vector's storage and drawn. | A polyline with fewer than two points draws nothing. | local |
| DX7Screen constructor (DX7HR 0x1000d271, DX7HRTnL twin) | The DDCAPS filter tests used dwFXCaps \| DDFXCAPS_... where & was meant, so mCanMinFilterSurfaces and mCanMagFilterSurfaces were always true whatever the card reported. | The capability bits are tested with &. | local |
| DX7DirectDrawEnumerateCallback (DX7HR 0x1000934b, DX7HRTnL twin) | The validity test was the inverse of the Ex callback's, so on the Windows 95 enumeration path screens with a hardware rasterizer were deleted and those without one added. | Valid screens are kept. | local |
| DX7Rasterizer::SetAlphaTestingCompareState (DX7HR 0x1000b26c, DX7HRTnL twin) | The unchanged-state early return compared the requested compare function with the alpha-testing on/off state instead of the current compare function, so some compare functions were never sent and others re-sent on every call. | It compares with the current compare function. | local |
| DX7Surface::Copy16BitWithFilter / Copy32BitWithFilter (DX7HR 0x1000f03f / 0x1000f4d0, DX7HRTnL twins) | The source pixel came from a round-to-nearest conversion and a negative fraction only got +1 without stepping the index back, so the filtered copy was shifted by up to a pixel; the four taps read index+1 and the next row with no edge clamp, reading outside the source rectangle (past the locked surface at its edge). | A negative fraction steps back one pixel and the taps are clamped to the source rectangle. | local |
| DX7Rasterizer::ActivateTheScreen (DX7HR 0x10009fa2) / DX7TnLRasterizer::ActivateTheScreen (DX7HRTnL 0x1000fedd) | When QueryInterface(IID_IDirect3D7) succeeded but CreateDevice failed, the error path returned without releasing the IDirect3D7. | It is released. | leak |
| GEFontBitmap::GEFontBitmap (LLE 0x10015409) | The glyph loop ran theCharacter < tmLastChar although mCharacters is sized for tmLastChar - tmFirstChar + 1 entries, so the font's last character (usually 0xFF) was never rendered and drew with a NULL texture. | The loop includes tmLastChar. | local |
| GEFontBitmap::Draw (LLE 0x10015b99) / Draw(clipped) (LLE 0x10015dcd) | The glyph index was (char)*theCharacter - mFirstCharacter with a signed char, so any byte >= 0x80 (accented letters, DBCS lead bytes) read an entry before mCharacters. | The byte is used unsigned and bytes outside the font's range are skipped. | local |
| GEFontBitmap::Draw(clipped) (LLE 0x10015dcd) | A glyph entirely outside the clip rectangle was skipped with continue before the pen advance, so every following glyph was drawn one glyph width too far left. | The pen advances past skipped glyphs. | local |
| GEModel::ComputeLODReduction (LLE 0x1001ce5b) | The cheapest-collapse search compared cost * weight (2 for the player-colour material) but stored the unweighted cost as the best, so later materials competed against a halved cost and the result depended on material order. | The weighted cost is stored. No caller in the game or engine. | dead code |
| GEModel::ComputeLODReduction (LLE 0x1001ce5b) | The per-material arrays thePolygonLists, theVertexLists and theVertices were never freed and the last set of meta vertices was not deleted. | They are freed at the end. No caller in the game or engine. | dead code |
| GELODMetaVertex::ComputeEdgeCollapseCost (LLE 0x1001dd9e) | The crease test theSum < 0.001 per component had no fabs, so any shared-face normal sum with all components negative counted as a crease. | The components' absolute values are tested. No caller of the reduction in the game or engine. | dead code |
| GEModel::OptimizeMemoryCoherency (LLE 0x10017912) | There was no check of the material's vertex count before using theGenomes[1], so a material with 0 or 1 vertices wrote past the genome arrays. | Such materials are skipped, as OptimizeVertexCaching does. No caller in the game or engine. | dead code |
| GEModel::ConvertLegacyModel (LLE 0x10018b2f) | The first temporary GEVertexArray* table was overwritten by the second new[] and never freed, and the version-1 material index arrays (mLegacyMaterials[i].mIndices) were never freed. | Both are freed. | leak |
| GEModel::ComputeModelNormals (LLE 0x1001f80c) | The per-vertex normal sums were allocated once and not cleared between frames, so each frame added to the previous frame's normalised normals and frames 1 and later were biased. | The sums are cleared for each frame. | local |
| GEModel::LoadHeaderChunk (LLE 0x1001c74a) | When the 4-byte tag read returned kErrorOK with fewer than 4 bytes, the function returned kErrorOK, so a truncated .cem file passed the header check with the previous file's versions. | A short tag read fails. | crash/memory |
| GESurface::SaveAsBMP (LLE 0x10025351) | BITMAPFILEHEADER.bfSize was computed from row size * width, so non-square screenshots had a wrong file size in the header. | Row size * height. | local |
| GESurface::SaveAsTGA (LLE 0x10026c57) | The header's width field got the surface height and the height field the width, and each row was padded with width % 4 bytes BMP-style, so non-square or odd-width Targas read back garbled. | The fields are in order and rows are not padded. Not used by the game's own saves (screenshots are JPG/BMP). | local |
| GESurface::SaveAsJPG2 (LLE 0x1002619b), SaveAsJPG, SaveAsTGA | The JPG2 alpha pass read the surface memory without checking that the earlier Lock succeeded, and all three called Unlock after a failed Lock. | The surface is read and unlocked only when locked; without the lock the JPEG passes write blank rows so libjpeg gets a complete image. | crash/memory |
| GESurface::LoadJPG (LLE 0x100256da) / LoadJPG2 (LLE 0x1002599d) | After jpeg_create_decompress, a failed surface Lock skipped jpeg_destroy_decompress, leaking libjpeg's memory pools once per failed load. | The decompressor is destroyed on that path too. | leak |
| GEText::ComputeTextExtent (LLE 0x10032ae2) | For text of 256 bytes or more, each full 256-byte chunk measured the whole text instead of the chunk, so long text was overmeasured by about chunks x its whole width. | Each chunk is measured on its own. | local |
| GETerrainMesh::Load (LLE 0x1002d3c1) / Save (LLE 0x1002d656) | Load skipped a failed texture-set read and a failed water-info or colour read only left the inner loop, so later successful reads overwrote the error; Save's point loop ran after a failed tile write and replaced its error. | Both stop at the first failure and return it. | crash/memory |
| GETexture::Load (LLE 0x10032ec4), per-frame path | A failed CreateSurface still fell through to SetSurface(theSurface) and Dereference with the previous frame's (or an uninitialised) surface pointer, and a failed reduced-surface creation swapped in an unset pointer. | A frame without a surface reports kErrorOutOfMemory and is left empty, and the reduced surface is used only when created. | leak |
| GETexture::Load (LLE 0x10032ec4), frame-grid path (format 1) | The single-frame staging surface used to load every frame into the grid was never dereferenced, and a failed frame read or copy returned without releasing it or the grid, leaking texture memory per load and per RefreshAllTextures/ReloadTexture. | Both are released on every path. | leak |
| GEViewport::SetViewRectangle (LLE 0x10037a1d) | It recomputed the half sizes, centre, aspect ratio and clipping planes but not the projection matrices, so after a resize the projections kept the old aspect ratio until a camera, field-of-view or clipping change. | The projections are recomputed too. | local |
