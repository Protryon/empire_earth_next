// C:\EEX\Empire Earth\Database\EEAIUnitTargetingTable.cpp (file name inferred: no __FILE__; the class of the table that
// Empire Earth.cpp loads from db\dbaiunittargeting.dat with it). 0xd47a1..0xd490f: Load, Unload; EEDataPreLoading.cpp
// ends at 0xd47a1 (the Data Pre-Loading Thread group), EEAnimal.cpp's init chain starts at 0xd490f. Unit flags: the
// exe's module set. Data: the table pointer 0x4e60c0.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"

#include "ee/EEAIUnitTargetingTable.h"

// One record of the file (0xa0 bytes, read whole; Load keeps ten of its fields; inferred layout, provisional names)
struct EEAIUnitTargetingRecord {
    char mLabel[100];                       // +0x000 the row name (the game never reads it; the text's label)
    int32_t mUnusedDatabaseID;              // +0x064 the database row id (not read)
    int32_t mUnusedIndex;                   // +0x068 the 0-based row index (not read)
    int32_t mAttackerBonus;                 // +0x6c
    int32_t mUnusedWeight;                  // +0x70
    int32_t mInRangeBonus;                  // +0x74
    int32_t mFasterTargetBonus;             // +0x78
    int32_t mStrongerTargetBonus;           // +0x7c
    int32_t mPreferredUnitType;             // +0x80
    int32_t mToHitWeight;                   // +0x84
    int32_t mProximityWeight;               // +0x88
    int32_t mImmobileTargetBonus;           // +0x8c
    int32_t mUnused1;                       // +0x90
    int32_t mUnused2;                       // +0x94
    int32_t mUnused3;                       // +0x98
    int32_t mMaxValue;                      // +0x9c
};
static_assert(sizeof(EEAIUnitTargetingRecord) == 160, "on-disk record: 160 bytes on every target");

long** EEAIUnitTargetingTable::sTable;      // 0x4e60c0: three rows of ten values (provisional name)

// FUNCTION: EXE 0x4d47a1
// Loads the AI unit-targeting weights (three rows of ten) from the file; the table stays empty unless it has 3 records
long EEAIUnitTargetingTable::Load(FSFileSpec* inFile)
{
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    EEAIUnitTargetingRecord theRecord;
    long theResult;
    if (EEAIUnitTargetingTable::sTable != NULL)
        EEAIUnitTargetingTable::Unload();
    inFile->Open(kFileAccessRead);
    theResult = inFile->Read(theCount);
    if (theResult == kErrorOK && theCount == 3) {
        EEAIUnitTargetingTable::sTable = new long*[3];
        for (unsigned long i = 0; i < theCount; i++) {
            EEAIUnitTargetingTable::sTable[i] = new long[10];
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(theRecord), theRead);
            EEAIUnitTargetingTable::sTable[i][0] = theRecord.mAttackerBonus;
            EEAIUnitTargetingTable::sTable[i][1] = theRecord.mUnusedWeight;
            EEAIUnitTargetingTable::sTable[i][2] = theRecord.mInRangeBonus;
            EEAIUnitTargetingTable::sTable[i][3] = theRecord.mFasterTargetBonus;
            EEAIUnitTargetingTable::sTable[i][4] = theRecord.mStrongerTargetBonus;
            EEAIUnitTargetingTable::sTable[i][5] = theRecord.mPreferredUnitType;
            EEAIUnitTargetingTable::sTable[i][6] = theRecord.mToHitWeight;
            EEAIUnitTargetingTable::sTable[i][7] = theRecord.mProximityWeight;
            EEAIUnitTargetingTable::sTable[i][8] = theRecord.mImmobileTargetBonus;
            EEAIUnitTargetingTable::sTable[i][9] = theRecord.mMaxValue;
        }
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x4d48d8
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
