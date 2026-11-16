// C:\EEX\Empire Earth\Database\EEUCUpgradeTable.cpp (name from RTTI EEUCUpgradeTable; no __FILE__). 0xdc682..0xdc8e7:
// the class static table's init chain, ctor, Load (db\dbupgrade.dat, Empire Earth.cpp 0x141ebf), Unload, ??_G.
// EETerrainTypeTable.cpp ends at 0xdc682, EEUnitBehavior.cpp's chains start at 0xdc8e7. Unit flags: the exe's
// module set.

#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"

#include "ee/EEUCUpgradeTable.h"
#include "ee/EEDbRecords.h"

std::vector<EEUCUpgradeTable*> EEUCUpgradeTable::sTable;

// Creates a unit upgrade entry: 16 factors of scale 1 and offset 0, max level 0
EEUCUpgradeTable::EEUCUpgradeTable()
{
    ee_data_upgrade_default(this);
}

// Loads the unit upgrade costs and steps from db\dbupgrade.dat (the record and its copy: ee_data tables.rs; the repair
// step as a 0/1 flag, factor 13 never set); a row with a negative index stays NULL. Returns the last read status
long EEUCUpgradeTable::Load(FSFileSpec* inFile)
{
    return EEDbReadRecords(inFile, ee_data_upgrade_record_size(),
        [](unsigned long inCount) { EEUCUpgradeTable::sTable.resize(inCount, NULL); return true; },
        [](unsigned long inIndex, const unsigned char* inRecord) {
            EeDataUpgradeFields theFields;
            EEUCUpgradeTable* theTable = NULL;
            if (ee_data_upgrade_from_record(inRecord, &theFields)) {
                theTable = new EEUCUpgradeTable;
                *static_cast<EeDataUpgradeFields*>(theTable) = theFields;
            }
            EEUCUpgradeTable::sTable[inIndex] = theTable;
        });
}

// Destroys every unit upgrade entry and empties the table
void EEUCUpgradeTable::Unload()
{
    for (unsigned long i = 0; i < EEUCUpgradeTable::sTable.size(); i++)
        delete EEUCUpgradeTable::sTable[i];
    EEUCUpgradeTable::sTable.resize(0);
}
