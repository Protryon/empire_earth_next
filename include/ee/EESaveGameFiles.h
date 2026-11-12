// (RECONSTRUCTION; file name inferred; function and constant names are provisional).
// Saved-game file helpers next to EEPSaveGameHeader.cpp (0x14ce98, 0x14d5ac) and the save version constants the
// lobby stores into an EEPSaveGameHeader before LoadFromFile (.rdata 0x4420ac / 0x4420b0).
// Requires <vector>, lle/LowLevelEngine.h.
#ifndef EE_EESAVEGAMEFILES_H
#define EE_EESAVEGAMEFILES_H

#include <functional>
#include <vector>

class EEGameOptions;
class FSFile;
class FSFileSpec;

extern const unsigned long kSaveGameMajorVersion;   // 0x4420ac
extern const unsigned long kSaveGameMinorVersion;   // 0x4420b0
// port: from 0.254 a save holds the running game's own values where the original wrote others or nothing, so a game
// loaded from it plays on as the game that was saved ("load equals continue", docs/RUST_PLAN.md R5a.2). It was 0.253
// on the rust branch until the merge of main ce3df9e4, whose 0.253 is the next engine's AI id (EEPPlayerHelper)
inline bool EESaveHasLiveState(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 254;
}
// port (decision D6): from 0.255 a save's strings are UTF-8; an older save's are Windows-1252 and are converted as
// they load (USetLegacyTextLoad around the load: EEPSaveGameHeader::Load, EEGameStateLoader::LoadGame)
inline bool EESaveTextIsUtf8(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 255;
}
// port: from 0.256 a save holds more of the running state ("load equals continue", second round): the computer
// players' round-robin schedule (EEArtificialIntelligence's next player and next update time, EEPWorldHelper; an older
// save's loaded game updated its first computer player at its first tick), each computer player's own timers and
// flags (EEComputerPlayer: pop cap, enable-time flag, regroup flag, population and damage times), and per unit the
// contacts, the animation frame, the exact flight time, rotting time, research, last hit points, garrison and power
// (EEComplexUnit; the original saved fractions or rebuilt them at load); the resource manager's timers and the
// research manager's flags, the path manager's history, regions and timers, the area effects' locations and lists,
// the units' order on each tile (EEPWorldHelper) and each attack sub group's targets, timers and path request
// (EECPAttackSubGroup). 0.255 is the UTF-8 text (EESaveTextIsUtf8): a 0.255 save loads without these fields
inline bool EESaveHasRunningState(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 256;
}
// port: from 0.257 a save holds the order of the units in each map region (EEMap::mUnitRegions, EEPWorldHelper): a
// load added them in its own order, and the projectiles' impact test and the target searches take the first match
inline bool EESaveHasRegionOrder(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 257;
}
// port: from 0.258 a save holds the computer players' experiment timers (ee/EECPExperimentTimers.h, EEPWorldHelper):
// they were function statics, 0 in a loaded game's new process
inline bool EESaveHasExperimentTimers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 258;
}
// port: from 0.259 a save holds the computer players' count of units in production (EECPUnitManager): a loaded game
// counted from 0 and made room for more units
inline bool EESaveHasProductionCount(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 259;
}
// port: from 0.260 a save holds the towns' ids, order and counters (EESaveTowns): a load rebuilt the towns with new
// ids in its own order
inline bool EESaveHasTowns(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 260;
}
// the towns' block (EEPlayer.cpp): written by EEPWorldHelper::Save; a load keeps it for EEPlayer::RepairPointers,
// which gives the rebuilt towns their saved ids and order
long EESaveTowns(FSFile& outFile);
// inAreas: the save has the towns' areas (EESaveHasTownAreas)
long EELoadTowns(FSFile& inFile, bool inAreas);
// port: from 0.261 a save holds the computer players' gatherer lists (EECPResourceManager): a load filed the citizens
// again in its own order, most of them twice (their resource's list and the idle list)
inline bool EESaveHasGathererOrder(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 261;
}
// port: from 0.262 a save holds the dead units the units' last attackers still reference (EEPWorldHelper): a load
// read NULL where the running game kept the dead attacker
inline bool EESaveHasGhosts(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 262;
}
// port: from 0.263 a save holds the continent manager as it is (EEAIContinentSave.cpp): its areas with their serials,
// the sectors and sub sectors, the grids and counters, and the serials of the herds and towns. A load rebuilt it from
// the map as it then was, numbering the herds and towns before the regions: other path areas, sub sectors and choke
// points under other serials than the running game's, which the computer players' tasks name
inline bool EESaveHasContinentRegions(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 263;
}
// port: from 0.264 a save holds each attack sub group's position (EECPAttackSubGroup::mPosition: FindPosition keeps it
// between its refreshes and scans for targets around it)
inline bool EESaveHasSubGroupPosition(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 264;
}
// port: from 0.265 a save holds the computer players' claim ranges and danger zones after the continent manager
// (EECPSaveResourceState, ee/EECPExperimentTimers.h): FindResourceTarget reads them, a load started them empty
inline bool EESaveHasResourceState(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 265;
}
// port: from 0.266 a save holds the order of each class's units the computer players' unit managers track
// (EECPUnitManager::mUnits): a load filed them again in its own order
inline bool EESaveHasUnitManagerOrder(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 266;
}
// port: from 0.267 a save holds the computer players' repair, airport site and space dock site timers
// (EECPCivilizationManager): a load started them at 0
inline bool EESaveHasCivilizationTimers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 267;
}
// port: from 0.268 the towns' block holds each town's area as it is (its tiles, hull, bounds and continent): a load
// rebuilt the towns from their buildings in its own order, and a building the saved town's hull had taken could start
// a town of its own (seed 6's 2100 s save: two towns and an extra patrol group where the saved game had one)
inline bool EESaveHasTownAreas(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 268;
}
// port: from 0.269 a save holds the computer players' manager timers the original kept out of it (the attack
// commander's next drop of spent groups, the next exploration, the personnel's next update, the next builder and raid
// checks; EEComputerPlayer::GetManagerTimers): a load started them at 0
inline bool EESaveHasManagerTimers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 269;
}
// port: from 0.270 a save holds the order of each player's unit list (EEPWorldHelper): the load inserted the units by
// hash bucket index, where the running list holds the buckets in the order they last filled
inline bool EESaveHasPlayerUnitOrder(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 270;
}
// port: from 0.271 a save holds each computer player's intelligence files (EECPIntelligence::SaveFiles): a load made
// them again from the world at their first use, where the running game's files held what they saw up to 30 s before
inline bool EESaveHasIntelligence(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 271;
}
// port: from 0.272 each ghost (EESaveHasGhosts) also holds its resource type and amount, and the ghosts include the
// resources and drop sites the gather goals still hold out of the world (a hunted animal's carcass gone, its gatherer
// still walking to it): a load read NULL there, and the gatherer was counted with no resource (GetGatherCapacity)
inline bool EESaveHasGhostResources(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 272;
}
// port: from 0.273 a save holds the contagion manager's last helper id after its helpers (EEContagionManager): a load
// went on from the highest saved helper's id (0 with none), so a calamity cast after it kept another helper id in its
// placeholder than the running game's (the calamity's out-of-synch checksum differed from the next cast on)
inline bool EESaveHasContagionCounter(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 273;
}
// port: from 0.275 an attack goal writes its target while it holds one, dying or dead (the original wrote -1 for a
// target with no hit points left, and the loaded unit lost the goal: RepairPointers failed and cleared the queue), and
// the ghosts (EESaveHasGhosts) include the attack goals' targets and the gather goals' simple resources (a tree or a
// mine used up while its gatherer still holds it) out of the world, each record with a complex flag
inline bool EESaveHasGoalGhosts(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 275;
}
// port: from 0.276 a save holds the units' tile searches in progress (EEPathManager::SaveSearches: the machine, its
// goal and its storage as ee_astar's bytes): a load asked each of those units' path again (RequestPendingPaths), so
// the path came later, through a graph search first, or another one
inline bool EESaveHasPathSearches(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 276;
}
// port: from 0.277 a save holds the wall manager's path request (EECPWallManager::Save, after the runs): a load
// asked the wall being planned its path again at its first Update, which then waited a step (and found another path)
// where the continued game went on with the finished one
inline bool EESaveHasWallRequest(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 277;
}
// port: from 0.278 a save holds the graph searches in progress (EEPathManager::SaveGraphSearches: node, machine,
// goal, modifier, request and the storage as ee_astar's bytes), and an attack sub group's running request waits for
// its own (EEAwaitAutoPathRequest): a load asked each again at the first tick (the sub groups) or from the load (the
// units, RequestPendingPaths), so the path came later or another one
inline bool EESaveHasGraphSearches(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return inMajorVersion > 0 || inMinorVersion >= 278;
}
// an area's part of the continent block's format (serial, kind, tiles as they are, hull, bounds) as bytes
// (EEAIContinentSave.cpp; the towns' block)
class EEAIArea;
void EEWriteAreaBytes(const EEAIArea& inArea, std::vector<unsigned char>& outBytes);
bool EEReadAreaBytes(const std::vector<unsigned char>& inBytes, EEAIArea& outArea);
long EESaveContinentRegions(FSFile& outFile);   // EEPWorldHelper::Save
long EELoadContinentRegions(FSFile& inFile);    // EEPWorldHelper::Load: kept for the load's EEWorld::StartGame
// whether the save being loaded has the block (EEWorld::StartGame of a loaded game)
bool EEContinentRegionsPending();
// StartGame of such a game, in place of BuildRegions: the saved manager (EEAIContinentSave.cpp); false when the block
// could not be read (the regions are then built as for an older save)
bool EERestoreContinentRegions();
// TEST ONLY (EE-SIM's state dumps): a hash of the block a save would write now
unsigned long EEContinentRegionsDigest();
// the dead attacker of that id, made from the save as a unit out of the world (EEComplexUnit::RepairPointers); NULL
// when the save has none of that id. Each one is kept by its references from the loaded game's first tick on
class EEComplexUnit;
class EEUnit;
EEComplexUnit* EEFindLoadedGhost(unsigned long inID);
// the same for any unit, simple ones too (0.275: the goals' resources and targets out of the world)
EEUnit* EEFindLoadedGhostUnit(unsigned long inID);
// the units out of the world the computer players' gatherer lists still hold (EECPResourceManager::mGatherers). They
// are saved as ghosts too (EEPWorldHelper's SaveGhosts) and put back in their lists as saved (EECPResourceManager::Load).
// Before the port's fix in EECPCivilizationManager::UnitLost a lost unit of a citizen category but neither citizen type
// (the fishing boats) stayed filed, dead, for the rest of the game: the 0.272 saves of such a build hold those boats
// as ghosts and load them back. A fixed game's lists lose every unit the player loses, so it writes none of them; what
// is left is a unit lost while the computer player is inactive (SetActive: UnitTaken skips the managers then)
void EECPOutOfWorldGatherers(std::vector<EEComplexUnit*>& ioUnits);
// port: a loaded value to put back at the loaded game's first tick, after the load's own rebuilding (which sets some
// of them again: animations, collision radii, ...); EEPhysics::Tick runs them once (EEPhysics.cpp)
void EEAfterLoad(std::function<void()> inFixup);
// the same, run after every EEAfterLoad fix-up of that tick (port, 0.278: the graph searches' owners first)
void EEAfterLoadLast(std::function<void()> inFixup);

// 0x14ce98: opens inName in data\inDirectory (the lobby's load checks); keseOK and outFile on success
// (gDataPath->FindFile(inDirectory + inName, outFile), hence FSFileSpec*&)
long EEOpenSaveGameFile(const UString& inName, const UString& inDirectory, FSFileSpec*& outFile);
// 0x14d946: re-opens inFile, loads its header and checks its options (0x14da7c); outError gets the message when not
bool EEValidateSaveGameFile(FSFile& inFile, UWideString& outError);
// 0x14d5ac: compares a saved game's options with the current ones; fills outMissing with what does not match
bool EECheckSavedGameOptions(EEGameOptions& inSaved, EEGameOptions& inCurrent, bool inFlag,
                             std::vector<UWideString>& outMissing, long inCount);

#endif
