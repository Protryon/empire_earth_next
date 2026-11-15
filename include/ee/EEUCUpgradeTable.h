// C:\EEX\Empire Earth\Database\EEUCUpgradeTable.h (RECONSTRUCTION; header and provisional names).
// The unit class upgrade table of db\dbupgrade.dat (RTTI EEUCUpgradeTable, no base; vtable 0x43f800, one slot: ??_G
// 0xdc8ca; sizeof 0x88 from Load's new). EEUCUpgradeTable.cpp 0xdc682..0xdc8e7.
#ifndef EE_EEUCUPGRADETABLE_H
#define EE_EEUCUPGRADETABLE_H

#include <vector>

class FSFileSpec;

// One upgraded value (no ctor: the table's ctor loops over them after the vptr store: 1.0f, 0.0f)
struct EEUCUpgradeFactor {
    float mScale;                           // +0x00
    float mOffset;                          // +0x04
};

class EEUCUpgradeTable {
public:
    EEUCUpgradeTable();                     // 0xdc6cb
    virtual ~EEUCUpgradeTable() {}          // +0x00 (??_G 0xdc8ca)
    static long Load(FSFileSpec* inFile);   // 0xdc6f0 (records with a negative +0xe0 stay NULL)
    static void Unload();                   // 0xdc88a

    EEUCUpgradeFactor mFactors[16];         // +0x04
    long mMaxLevel;                         // +0x84 (record +0xd8)

    static std::vector<EEUCUpgradeTable*> sTable;   // 0x8e8728 (guard 0x8e8720 bit 1; only its unit sees it)
};

#endif
