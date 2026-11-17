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
#include "ee/EEDbRecords.h"

// 0x4e8750: the weapon to-hit factors (6 x 6; provisional name: not gToHitTable, which is Empire Earth.cpp's
// EEToHitTable 0x5318e8; EEPhysicsModel::Push reads both)
float gWeaponToHitFactors[6][6];
// 0x52fb54: world table entry 42's float, cached after the load (provisional name)
extern float& gGameStateGameSpeed;

EEDbList<EEUnitBehavior, EeDataList::UnitBehaviors> EEUnitBehavior::sTable;



// Loads the unit behaviours (move, pursue, target, return and flee permissions) from db\dbUnitBehavior.dat into the
// Rust list (ee_data tables.rs); a row with a negative index stays NULL. Returns the last read status
long EEUnitBehavior::Load(FSFileSpec* inFile)
{
    return EEDbLoadList(inFile, EeDataList::UnitBehaviors);
}

// Empties the table
void EEUnitBehavior::Unload()
{
    EEUnitBehavior::sTable.clear();
}

// Returns whether a unit may move freely: always, or by the melee or ranged permission matching its class
bool EEUnitBehavior::CanMoveFreely(EEUnit* inUnit)
{
    return this->mCanMoveFreely || (inUnit->GetUnitClass()->mIsMelee && this->mMeleeCanMoveFreely) || (!inUnit->GetUnitClass()->mIsMelee && this->mRangedCanMoveFreely);
}

// Loads the 6 x 6 weapon to-hit factors (percent to fraction: ee_data tables.rs) from db\dbweapontohit.dat; a table
// of another size is not read. Returns the last read status
long EEToHitTable::Load(FSFileSpec* inFile)
{
    return EEDbReadRecords(inFile, ee_data_to_hit_record_size(), [](unsigned long inCount) { return inCount == 6; },
        [](unsigned long inIndex, const unsigned char* inRecord) {
            ee_data_to_hit_from_record(inRecord, gWeaponToHitFactors[inIndex]);
        });
}

// Creates an empty world constants table
EEWorldTable::EEWorldTable()
{
    this->Clear();
}

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
    gGameStateGameSpeed = gWorldTable.GetFloatValue(kWorldGameSpeedDefault);
    return theResult;
}

// Empties the world constants table
void EEWorldTable::Clear()
{
    this->mEntries.clear();
}
