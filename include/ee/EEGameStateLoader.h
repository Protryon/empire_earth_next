// EEGameStateLoader.h (RECONSTRUCTION; file name inferred; class and member names are provisional: no RTTI, no
// __FILE__). The saved-game loader of Persistent/EEGameStateLoader.cpp (0x14a009..0x14ac35): a stack object holding
// the EEGameState it fills (EEGameState::Create 0x1306f2 builds one on gGameState, EESaveLoad 0x14d4fd another). It
// reads the save header, then each persistent chunk, and hands the objects to the state by tag.
// Requires lle/LowLevelEngine.h, <vector>.
#ifndef EE_EEGAMESTATELOADER_H
#define EE_EEGAMESTATELOADER_H

class EEGameState;
class EEPPlayerHelper;
class FSFile;
class FSPersistent;
class UString;

class EEGameStateLoader {
public:
    EEGameStateLoader(EEGameState* inState);                                    // 0x14a009
    // 0x14a0a4: reads one persistent per entry of inTags from inFile and restores it into the state (3: bad tag)
    long RestorePersistents(std::vector<long>& inTags, FSFile& inFile);
    bool Load(FSFile& inFile);                                                  // 0x14a3eb (copies inFile to memory first)
    // 0x14a4ef: the header from inHeaderFile, then the persistents from inFile; false (and the state cleared) on failure
    bool LoadGame(FSFile& inHeaderFile, FSFile& inFile);
    long ReadPersistent(FSFile& inFile, FSPersistent*& outPersistent);          // 0x14a765 (one chunk)
    long RestorePlayers(EEPPlayerHelper* inHelper);                             // 0x14a7ab

    EEGameState* mState;                    // +0x00
};

// 0x14a014 (cdecl): whether inFile ends in gScenarioExtension (case-insensitive)
bool EEIsScenarioFile(const UString& inFile);

#endif
