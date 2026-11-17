// C:\EEX\Empire Earth\Database\EEStartingResources.h (RECONSTRUCTION; file name inferred; provisional names). The
// starting resource sets of db\dbStartingResources.dat (RTTI EEStartingResources, no base; vtable 0x43f7d0, one
// slot: ??_G 0xda9e2; sizeof 0x1c from Load's new). EEStartingResources.cpp 0xda984..0xdae63.
#ifndef EE_EESTARTINGRESOURCES_H
#define EE_EESTARTINGRESOURCES_H

#include "ee/EEDbList.h"

class FSFileSpec;

// mAmounts and mNameID are EeDataStartingResourcesFields (rust/crates/ee_data tables.rs: the record and its copy); the
// table is Rust's list (ee/EEDbList.h)
class EEStartingResources : public EeDataStartingResourcesFields {
public:
    static long Load(FSFileSpec* inFile);   // 0xdaa05 (records with a negative +0x68 are skipped)
    static void Unload();

    // inline (the lobby's LobbyOpen 0x1df5ef loads it before GetString's other argument; was EEU1318E6Entry::GetU18)
    long GetNameID() const { return this->mNameID; }

    static EEDbList<EEStartingResources, EeDataList::StartingResources> sTable;
    static EEStartingResources* Get(unsigned long inIndex) { return EEStartingResources::sTable[inIndex]; }  // inline (lobby 0x5dde02) (provisional name)
};

// EEGameState.cpp 0x1318e6: sTable[inIndex] (inline operator[] expanded; provisional name)
EEStartingResources* EEGetResourceLevel(long inIndex);
// EEGameOptions.cpp 0x1304cc: sTable.size()
unsigned long EEGetNumberOfResourceLevels();

#endif
