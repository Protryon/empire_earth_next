// C:\EEX\Empire Earth\Database\EEUCUpgradeTable.cpp (name from RTTI EEUCUpgradeTable; no __FILE__). 0xdc682..0xdc8e7:
// the class static table's init chain, ctor, Load (db\dbupgrade.dat, Empire Earth.cpp 0x141ebf), Unload, ??_G.
// EETerrainTypeTable.cpp ends at 0xdc682, EEUnitBehavior.cpp's chains start at 0xdc8e7. Unit flags: the exe's
// module set.

#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"

#include "ee/EEUCUpgradeTable.h"

// One record of db\dbupgrade.dat (0xf0 bytes, read whole). Inferred layout.
struct EEUCUpgradeRecord {
    char _gap00[0x64];
    float mAttackStep;                      // +0x64
    float mHitPointsStep;                   // +0x68
    float mSpeedStep;                       // +0x6c
    float mRangeStep;                       // +0x70
    float mShockArmorStep;                  // +0x74
    float mArrowArmorStep;                  // +0x78
    float mPierceArmorStep;                 // +0x7c
    float mGunArmorStep;                    // +0x80
    float mLaserArmorStep;                  // +0x84
    float mMissileArmorStep;                // +0x88
    float mFlightTimeStep;                  // +0x8c
    float mLeadershipStep;                  // +0x90
    float mAreaEffectStep;                  // +0x94
    float mTransportStep;                   // +0x98
    float mAttackCost;                      // +0x9c
    float mHitPointsCost;                   // +0xa0
    float mSpeedCost;                       // +0xa4
    float mRangeCost;                       // +0xa8
    float mShockArmorCost;                  // +0xac
    float mArrowArmorCost;                  // +0xb0
    float mPierceArmorCost;                 // +0xb4
    float mGunArmorCost;                    // +0xb8
    float mLaserArmorCost;                  // +0xbc
    float mMissileArmorCost;                // +0xc0
    float mFlightTimeCost;                  // +0xc4
    float mLeadershipCost;                  // +0xc8
    float mRepairCost;                      // +0xcc
    float mAreaEffectCost;                  // +0xd0
    float mTransportCost;                   // +0xd4
    long mMaxLevel;                         // +0xd8
    long mLDC;                              // +0xdc
    long mIndex;                            // +0xe0 (negative: no entry)
    long mLE4;                              // +0xe4
    long mLE8;                              // +0xe8
    bool mHasRepairStep;                    // +0xec
};
static_assert(sizeof(EEUCUpgradeRecord) == 240, "on-disk record: 240 bytes on every target");

std::vector<EEUCUpgradeTable*> EEUCUpgradeTable::sTable;

// FUNCTION: EXE 0x4dc6cb
// Creates a unit upgrade entry: 16 factors of scale 1 and offset 0, max level 0
EEUCUpgradeTable::EEUCUpgradeTable()
{
    for (long i = 0; i < 16; i++) {
        this->mFactors[i].mScale = 1.0f;
        this->mFactors[i].mOffset = 0.0f;
    }
    this->mMaxLevel = 0;
}

// FUNCTION: EXE 0x4dc6f0
// Loads the unit upgrade costs and steps from db\dbupgrade.dat (the repair step as a 0/1 flag; factor 13 is never set);
// returns the last read status
long EEUCUpgradeTable::Load(FSFileSpec* inFile)
{
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    EEUCUpgradeRecord theRecord;
    inFile->Open(kFileAccessRead);
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        EEUCUpgradeTable::sTable.resize(theCount, NULL);
        for (unsigned long i = 0; i < theCount; i++) {
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(theRecord), theRead);
            if (theRecord.mIndex >= 0) {
                EEUCUpgradeTable* theTable = new EEUCUpgradeTable;
                theTable->mMaxLevel = theRecord.mMaxLevel;
                theTable->mFactors[0].mScale = theRecord.mAreaEffectCost;
                theTable->mFactors[0].mOffset = theRecord.mAreaEffectStep;
                theTable->mFactors[1].mScale = theRecord.mAttackCost;
                theTable->mFactors[1].mOffset = theRecord.mAttackStep;
                theTable->mFactors[2].mScale = theRecord.mArrowArmorCost;
                theTable->mFactors[2].mOffset = theRecord.mArrowArmorStep;
                theTable->mFactors[3].mScale = theRecord.mGunArmorCost;
                theTable->mFactors[3].mOffset = theRecord.mGunArmorStep;
                theTable->mFactors[4].mScale = theRecord.mLaserArmorCost;
                theTable->mFactors[4].mOffset = theRecord.mLaserArmorStep;
                theTable->mFactors[5].mScale = theRecord.mMissileArmorCost;
                theTable->mFactors[5].mOffset = theRecord.mMissileArmorStep;
                theTable->mFactors[6].mScale = theRecord.mPierceArmorCost;
                theTable->mFactors[6].mOffset = theRecord.mPierceArmorStep;
                theTable->mFactors[7].mScale = theRecord.mShockArmorCost;
                theTable->mFactors[7].mOffset = theRecord.mShockArmorStep;
                theTable->mFactors[8].mScale = theRecord.mFlightTimeCost;
                theTable->mFactors[8].mOffset = theRecord.mFlightTimeStep;
                theTable->mFactors[9].mScale = theRecord.mHitPointsCost;
                theTable->mFactors[9].mOffset = theRecord.mHitPointsStep;
                theTable->mFactors[10].mScale = theRecord.mLeadershipCost;
                theTable->mFactors[10].mOffset = theRecord.mLeadershipStep;
                theTable->mFactors[11].mScale = theRecord.mRangeCost;
                theTable->mFactors[11].mOffset = theRecord.mRangeStep;
                theTable->mFactors[12].mScale = theRecord.mRepairCost;
                // the original stores element 12's offset twice (element 13 is never set; a source slip): once in each
                // arm of the conditional and once with its result
                theTable->mFactors[12].mOffset = theRecord.mHasRepairStep == true ? (theTable->mFactors[12].mOffset = 1.0f)
                                                                        : (theTable->mFactors[12].mOffset = 0.0f);




                theTable->mFactors[14].mScale = theRecord.mSpeedCost;
                theTable->mFactors[14].mOffset = theRecord.mSpeedStep;
                theTable->mFactors[15].mScale = theRecord.mTransportCost;
                theTable->mFactors[15].mOffset = theRecord.mTransportStep;
                EEUCUpgradeTable::sTable[i] = theTable;
            } else {
                EEUCUpgradeTable::sTable[i] = NULL;
            }
        }
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x4dc88a
// Destroys every unit upgrade entry and empties the table
void EEUCUpgradeTable::Unload()
{
    for (unsigned long i = 0; i < EEUCUpgradeTable::sTable.size(); i++)
        delete EEUCUpgradeTable::sTable[i];
    EEUCUpgradeTable::sTable.resize(0);
}

// SYNTHETIC: EXE 0x4dc8ca
// (the scalar deleting destructor)

// SYNTHETIC: EXE 0x4dc682
// SYNTHETIC: EXE 0x4dc68c
// SYNTHETIC: EXE 0x4dc6a4
// SYNTHETIC: EXE 0x4dc6b0
