// C:\EEX\Empire Earth\Database\EEUnitBehavior.cpp (name from RTTI EEUnitBehavior; no __FILE__). 0xdc8e7..0xdd160:
// the class static table's init chain, EEUnitBehavior (ctor, ??_G, dtor, Load, Unload, CanMoveFreely), EEToHitTable::Load,
// EEWorldTable (ctor, Load, the clear thunk) and the vector<EEWorldTableEntry> COMDATs, EEWorldTable's ??_G.
// EEUCUpgradeTable.cpp ends at 0xdc8e7; the Graphics Thread group starts at 0xdd160. Unit flags: the exe's module
// set.

#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/FSFileSpec.h"

#include "ee/EmpireEarth.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitBehavior.h"

// 0x4e8750: the weapon to-hit factors (6 x 6; provisional name: not gToHitTable, which is Empire Earth.cpp's
// EEToHitTable 0x5318e8; EEPhysicsModel::Push reads both)
float gWeaponToHitFactors[6][6];
// 0x52fb54: world table entry 42's float, cached after the load (provisional name)
extern float gGameStateGameSpeed;

// One record of db\dbUnitBehavior.dat (0x7c bytes, read whole). Inferred layout.
struct EEUnitBehaviorRecord {
    char mLabel[100];                       // +0x000 the row name (the game never reads it; the text's label)
    int32_t mUnusedDatabaseID;              // +0x064 the database row id (not read)
    int32_t mIndex;                         // +0x68 (negative: no entry)
    bool mCanMoveFreely;                    // +0x6c -> +0x04
    bool mCanPursuePastInitialContactLOS;   // +0x6d -> +0x07
    bool mCanTargetEnemies;                 // +0x6e -> +0x08
    bool mReturnsToInitialContactLocation;  // +0x6f -> +0x09
    bool mCanFlee;                          // +0x70 -> +0x0a
    bool mMeleeCanMoveFreely;               // +0x71 -> +0x05
    bool mRangedCanMoveFreely;              // +0x72 -> +0x06
    char _gap73[0x09];
};
static_assert(sizeof(EEUnitBehaviorRecord) == 124, "on-disk record: 124 bytes on every target");

// One record of db\dbweapontohit.dat (0x84 bytes). Inferred layout.
struct EEToHitRecord {
    char mLabel[100];                       // +0x000 the row name (the game never reads it; the text's label)
    int32_t mUnusedDatabaseID;              // +0x064 the database row id (not read)
    int32_t mUnusedIndex;                   // +0x068 the 0-based row index (not read)
    int32_t mPercents[6];                   // +0x6c (hundredths)
};
static_assert(sizeof(EEToHitRecord) == 132, "on-disk record: 132 bytes on every target");

std::vector<EEUnitBehavior*> EEUnitBehavior::sTable;

// FUNCTION: EXE 0x4dc930
// Creates a unit behaviour with every permission off
EEUnitBehavior::EEUnitBehavior()
{
    this->mCanMoveFreely = false;
    this->mMeleeCanMoveFreely = false;
    this->mRangedCanMoveFreely = false;
    this->mCanPursuePastInitialContactLOS = false;
    this->mCanTargetEnemies = false;
    this->mReturnsToInitialContactLocation = false;
    this->mCanFlee = false;
}

// SYNTHETIC: EXE 0x4dc950
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4dc96c
// Destroys the unit behaviour (nothing to release)
EEUnitBehavior::~EEUnitBehavior()
{
}

// FUNCTION: EXE 0x4dc973
// Loads the unit behaviours (move, pursue, target, return and flee permissions) from db\dbUnitBehavior.dat; returns
// the last read status
long EEUnitBehavior::Load(FSFileSpec* inFile)
{
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    EEUnitBehaviorRecord theRecord;
    inFile->Open(kFileAccessRead);
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        EEUnitBehavior::sTable.resize(theCount, NULL);
        for (unsigned long i = 0; i < theCount; i++) {
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(theRecord), theRead);
            EEUnitBehavior* theBehavior;
            if (theRecord.mIndex >= 0) {
                theBehavior = new EEUnitBehavior;
                theBehavior->mCanMoveFreely = theRecord.mCanMoveFreely;
                theBehavior->mCanPursuePastInitialContactLOS = theRecord.mCanPursuePastInitialContactLOS;
                theBehavior->mCanTargetEnemies = theRecord.mCanTargetEnemies;
                theBehavior->mReturnsToInitialContactLocation = theRecord.mReturnsToInitialContactLocation;
                theBehavior->mCanFlee = theRecord.mCanFlee;
                theBehavior->mMeleeCanMoveFreely = theRecord.mMeleeCanMoveFreely;
                theBehavior->mRangedCanMoveFreely = theRecord.mRangedCanMoveFreely;
            } else {
                theBehavior = NULL;
            }
            EEUnitBehavior::sTable[i] = theBehavior;
        }
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x4dca3d
// Destroys every unit behaviour and empties the table
void EEUnitBehavior::Unload()
{
    for (unsigned long i = 0; i < EEUnitBehavior::sTable.size(); i++) {
        delete EEUnitBehavior::sTable[i];
        EEUnitBehavior::sTable[i] = NULL;
    }
    EEUnitBehavior::sTable.clear();
}

// FUNCTION: EXE 0x4dca7d
// Returns whether a unit may move freely: always, or by the melee or ranged permission matching its class
bool EEUnitBehavior::CanMoveFreely(EEUnit* inUnit)
{
    return this->mCanMoveFreely || (inUnit->GetUnitClass()->mIsMelee && this->mMeleeCanMoveFreely) || (!inUnit->GetUnitClass()->mIsMelee && this->mRangedCanMoveFreely);
}

// FUNCTION: EXE 0x4dcaad
// Loads the 6 x 6 weapon to-hit factors (percent to fraction) from db\dbweapontohit.dat; returns the last read status
long EEToHitTable::Load(FSFileSpec* inFile)
{
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    EEToHitRecord theRecord;
    inFile->Open(kFileAccessRead);
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK && theCount == 6) {
        for (unsigned long i = 0; i < theCount; i++) {
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(theRecord), theRead);
            gWeaponToHitFactors[i][0] = theRecord.mPercents[0] * kHeightEpsilon;
            gWeaponToHitFactors[i][1] = theRecord.mPercents[1] * kHeightEpsilon;
            gWeaponToHitFactors[i][2] = theRecord.mPercents[2] * kHeightEpsilon;
            gWeaponToHitFactors[i][3] = theRecord.mPercents[3] * kHeightEpsilon;
            gWeaponToHitFactors[i][4] = theRecord.mPercents[4] * kHeightEpsilon;
            gWeaponToHitFactors[i][5] = theRecord.mPercents[5] * kHeightEpsilon;
        }
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x4dcb53
// Creates an empty world constants table
EEWorldTable::EEWorldTable()
{
    this->Clear();
}

// FUNCTION: EXE 0x4dcb79
// Loads the world constants from a file and caches entry 42 as the game speed; returns the first read error
long EEWorldTable::Load(FSFileSpec* inFile)
{
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    EEWorldTableEntry theEntry;
    inFile->Open(kFileAccessRead);
    long theResult = inFile->Read(theCount);
    this->mEntries.resize(theCount, theEntry);
    for (unsigned long i = 0; i < theCount && theResult == kErrorOK; i++) {
        theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theEntry), sizeof(theEntry), theRead);
        this->mEntries[i] = theEntry;
    }
    inFile->Close();
    gGameStateGameSpeed = gWorldTable.GetFloatValue(42);
    return theResult;
}

// FUNCTION: EXE 0x4dcc2b
// Empties the world constants table
void EEWorldTable::Clear()
{
    this->mEntries.clear();
}

// SYNTHETIC: EXE 0x4dc8e7
// SYNTHETIC: EXE 0x4dc8f1
// SYNTHETIC: EXE 0x4dc909
// SYNTHETIC: EXE 0x4dc915

// TEMPLATE: EXE 0x4dcc33
// TEMPLATE: EXE 0x4dcce7
// TEMPLATE: EXE 0x4dcd1e
// TEMPLATE: EXE 0x4dcfd9
// TEMPLATE: EXE 0x4dcfed
// TEMPLATE: EXE 0x4dd01f
// TEMPLATE: EXE 0x4dd042
// TEMPLATE: EXE 0x4dd071
// SYNTHETIC: EXE 0x4dd087
// TEMPLATE: EXE 0x4dd0b7
// TEMPLATE: EXE 0x4dd0f2
// TEMPLATE: EXE 0x4dd136
// (the vector<EEWorldTableEntry> COMDATs and EEWorldTable::`scalar deleting destructor`)
