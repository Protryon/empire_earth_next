// C:\EEX\Empire Earth\Database\EEAIUnitTargetingTable.cpp (file name inferred: no __FILE__; the class of the table that
// Empire Earth.cpp loads from db\dbaiunittargeting.dat with it). 0xd47a1..0xd490f: Load, Unload; EEDataPreLoading.cpp
// ends at 0xd47a1 (the Data Pre-Loading Thread group), EEAnimal.cpp's init chain starts at 0xd490f. Unit flags: the
// exe's module set. Data: the table pointer 0x4e60c0.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"

#include "ee/EEAIUnitTargetingTable.h"
#include "ee/EEDbRecords.h"
#include "rust/data/tables.h"

long** EEAIUnitTargetingTable::sTable;      // 0x4e60c0: three rows of ten values (provisional name)

// Loads the AI unit-targeting weights (three rows of ten: the record and its copy are ee_data tables.rs) from the file;
// the table stays empty unless it has 3 records
long EEAIUnitTargetingTable::Load(FSFileSpec* inFile)
{
    if (EEAIUnitTargetingTable::sTable != NULL)
        EEAIUnitTargetingTable::Unload();
    return EEDbReadRecords(inFile, ee_data_ai_targeting_record_size(),
        [](unsigned long inCount) {
            if (inCount != 3)
                return false;
            EEAIUnitTargetingTable::sTable = new long*[3];
            return true;
        },
        [](unsigned long inIndex, const unsigned char* inRecord) {
            EEAIUnitTargetingTable::sTable[inIndex] = new long[10];
            ee_data_ai_targeting_from_record(inRecord, EEAIUnitTargetingTable::sTable[inIndex]);
        });
}

// Frees the AI unit-targeting weight table
void EEAIUnitTargetingTable::Unload()
{
    if (EEAIUnitTargetingTable::sTable != NULL) {
        for (int i = 0; i < 3; i++)
            if (EEAIUnitTargetingTable::sTable[i] != NULL)
                delete[] EEAIUnitTargetingTable::sTable[i];
        delete[] EEAIUnitTargetingTable::sTable;
    }
    EEAIUnitTargetingTable::sTable = NULL;
}
