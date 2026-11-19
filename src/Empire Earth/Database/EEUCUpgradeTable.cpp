// C:\EEX\Empire Earth\Database\EEUCUpgradeTable.cpp (name from RTTI EEUCUpgradeTable; no __FILE__). 0xdc682..0xdc8e7:
// the class static table's init chain, ctor, Load (db\dbupgrade.dat, Empire Earth.cpp 0x141ebf), Unload, ??_G.
// EETerrainTypeTable.cpp ends at 0xdc682, EEUnitBehavior.cpp's chains start at 0xdc8e7. Unit flags: the exe's
// module set.

#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"

#include "ee/EEUCUpgradeTable.h"
#include "ee/EEDbRecords.h"

EEDbList<EEUCUpgradeTable, EeDataList::Upgrades> EEUCUpgradeTable::sTable;


// Loads the unit upgrade costs and steps from db\dbupgrade.dat into the Rust list (the record, its copy and the list:
// ee_data tables.rs; the repair step as a 0/1 flag, factor 13 never set); a row with a negative index stays NULL.
// Returns the last read status
long EEUCUpgradeTable::Load(FSFileSpec* inFile)
{
    return EEDbLoadList(inFile, EeDataList::Upgrades);
}

// Empties the table
void EEUCUpgradeTable::Unload()
{
    EEUCUpgradeTable::sTable.clear();
}
