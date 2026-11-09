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
