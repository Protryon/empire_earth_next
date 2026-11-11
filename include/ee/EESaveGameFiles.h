// (RECONSTRUCTION; file name inferred; function and constant names are provisional).
// Saved-game file helpers next to EEPSaveGameHeader.cpp (0x14ce98, 0x14d5ac) and the save version constants the
// lobby stores into an EEPSaveGameHeader before LoadFromFile (.rdata 0x4420ac / 0x4420b0).
// Requires <vector>, lle/LowLevelEngine.h.
#ifndef EE_EESAVEGAMEFILES_H
#define EE_EESAVEGAMEFILES_H

#include <functional>

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
long EELoadTowns(FSFile& inFile);
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
EEComplexUnit* EEFindLoadedGhost(unsigned long inID);
// port: a loaded value to put back at the loaded game's first tick, after the load's own rebuilding (which sets some
// of them again: animations, collision radii, ...); EEPhysics::Tick runs them once (EEPhysics.cpp)
void EEAfterLoad(std::function<void()> inFixup);

// 0x14ce98: opens inName in data\inDirectory (the lobby's load checks); keseOK and outFile on success
// (gDataPath->FindFile(inDirectory + inName, outFile), hence FSFileSpec*&)
long EEOpenSaveGameFile(const UString& inName, const UString& inDirectory, FSFileSpec*& outFile);
// 0x14d946: re-opens inFile, loads its header and checks its options (0x14da7c); outError gets the message when not
bool EEValidateSaveGameFile(FSFile& inFile, UWideString& outError);
// 0x14d5ac: compares a saved game's options with the current ones; fills outMissing with what does not match
bool EECheckSavedGameOptions(EEGameOptions& inSaved, EEGameOptions& inCurrent, bool inFlag,
                             std::vector<UWideString>& outMissing, long inCount);

#endif
