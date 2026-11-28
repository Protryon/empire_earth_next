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
    EEGameStateLoader(EEGameState* inState);
    // the bodies (0x14a0a4 RestorePersistents, 0x14a765 ReadPersistent, RestorePlayers: ee_game gamecpp persist_save.rs)
    bool Load(FSFile& inFile);                                                  // 0x14a3eb (copies inFile to memory first)

    EEGameState* mState;                    // +0x00
};


#endif
