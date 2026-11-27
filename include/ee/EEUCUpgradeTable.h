// C:\EEX\Empire Earth\Database\EEUCUpgradeTable.h (RECONSTRUCTION; header and provisional names).
// The unit class upgrade table of db\dbupgrade.dat (RTTI EEUCUpgradeTable, no base; vtable 0x43f800, one slot: ??_G
// 0xdc8ca; sizeof 0x88 from Load's new). EEUCUpgradeTable.cpp 0xdc682..0xdc8e7.
#ifndef EE_EEUCUPGRADETABLE_H
#define EE_EEUCUPGRADETABLE_H

#include "ee/EEDbList.h"
#include "ee/EEDbRecords.h"
#include "rust/data/records.h"

class FSFileSpec;

// One upgraded value, scale and offset (Rust's: rust/crates/ee_data tables.rs)
typedef EeDataUpgradeFactor EEUCUpgradeFactor;

// mFactors (16) and mMaxLevel are EeDataUpgradeFields (ee_data tables.rs: the record, its copy and the defaults)
class EEUCUpgradeTable : public EeDataUpgradeFields {
public:
    // 0xdc6f0: db\dbupgrade.dat into the Rust list (ee_data tables.rs: the repair step as a 0/1 flag, factor 13 never
    // set; a record with a negative +0xe0 stays NULL); the last read status. (EEUCUpgradeTable.cpp went in the game's
    // leaves round 1: the class is its record and the list.)
    template <class F> static long Load(F* inFile) { return EEDbLoadList(inFile, EeDataList::Upgrades); }
    static void Unload() { EEUCUpgradeTable::sTable.clear(); }

    static constexpr EEDbList<EEUCUpgradeTable, EeDataList::Upgrades> sTable{};   // Rust's list (ee/EEDbList.h)
};

#endif
