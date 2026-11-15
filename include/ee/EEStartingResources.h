// C:\EEX\Empire Earth\Database\EEStartingResources.h (RECONSTRUCTION; file name inferred; provisional names). The
// starting resource sets of db\dbStartingResources.dat (RTTI EEStartingResources, no base; vtable 0x43f7d0, one
// slot: ??_G 0xda9e2; sizeof 0x1c from Load's new). EEStartingResources.cpp 0xda984..0xdae63.
#ifndef EE_EESTARTINGRESOURCES_H
#define EE_EESTARTINGRESOURCES_H

#include <vector>

class FSFileSpec;

class EEStartingResources {
public:
    EEStartingResources();                  // 0xda9cd (the five amounts zeroed with stosd)
    virtual ~EEStartingResources();         // +0x00 0xda9fe (??_G 0xda9e2)
    static long Load(FSFileSpec* inFile);   // 0xdaa05 (records with a negative +0x68 are skipped)
    static void Unload();                   // 0xdaae6

    long mAmounts[5];                       // +0x04 (record +0x70, +0x74, +0x78, +0x7c, +0x80)
    long mNameID;                           // +0x18 (record +0x6c; the entry's name string id)
    // inline (the lobby's LobbyOpen 0x1df5ef loads it before GetString's other argument; was EEU1318E6Entry::GetU18)
    long GetNameID() const { return this->mNameID; }

    static std::vector<EEStartingResources*> sTable;   // 0x8e8690 (guard 0x8e8688 bit 1; only its unit sees it)
    static EEStartingResources* Get(unsigned long inIndex) { return EEStartingResources::sTable[inIndex]; }  // inline (lobby 0x5dde02) (provisional name)
};

// EEGameState.cpp 0x1318e6: sTable[inIndex] (inline operator[] expanded; provisional name)
EEStartingResources* EEGetResourceLevel(long inIndex);
// EEGameOptions.cpp 0x1304cc: sTable.size()
unsigned long EEGetNumberOfResourceLevels();

#endif
