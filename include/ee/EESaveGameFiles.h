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

extern const unsigned long kSaveGameMajorVersion;
extern const unsigned long kSaveGameMinorVersion;
// The save versions' predicates (EESaveHasLiveState .. EESaveHasTileTypes, EESaveTextIsUtf8): Rust's (ee_sim
// save_versions.rs, each with the version it starts at and what it adds), under these names.
#include "rust/sim/save_versions.h"
// the towns' block (EEPlayer.cpp): written by EEPWorldHelper::Save; a load keeps it for EEPlayer::RepairPointers,
// which gives the rebuilt towns their saved ids and order
long EESaveTowns(FSFile& outFile);
// inAreas: the save has the towns' areas (EESaveHasTownAreas)
long EELoadTowns(FSFile& inFile, bool inAreas);
// an area's part of the continent block's format (serial, kind, tiles as they are, hull, bounds) as bytes
// (EEAIContinentSave.cpp; the towns' block)
class EEAIArea;
void EEWriteAreaBytes(const EEAIArea& inArea, std::vector<unsigned char>& outBytes);
bool EEReadAreaBytes(const std::vector<unsigned char>& inBytes, EEAIArea& outArea);
// StartGame of such a game, in place of BuildRegions: the saved manager (EEAIContinentSave.cpp); false when the block
// could not be read (the regions are then built as for an older save)
bool EERestoreContinentRegions();
// the dead attacker of that id, made from the save as a unit out of the world (EEComplexUnit::RepairPointers); NULL
// when the save has none of that id. Each one is kept by its references from the loaded game's first tick on
class EEComplexUnit;
class EEUnit;
EEComplexUnit* EEFindLoadedGhost(unsigned long inID);
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
