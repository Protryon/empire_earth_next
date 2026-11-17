// C:\EEX\Empire Earth\Database\EEGameVariant.cpp (name from RTTI EEGameVariant; no __FILE__). 0xd68c7..0xd6bed: the
// variant table's init chain, ctor, ??_G, dtor, Load (db\dbGameVariant.dat), Unload, GetFactor, the table's
// push_back COMDAT. EEDbObject.cpp ends at 0xd68c7, EEGraphicEffect.cpp's chains start at 0xd6bed. Unit flags: the
// exe's module set.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"

#include "ee/EEGameVariant.h"
#include "ee/EEDbRecords.h"
#include "ee/EEUnitSetManager.h"

// NEAR (chain 0x4d68c7): the original's dtor registrar is guarded (a class static data member's guard byte
// 0x8e7f70); the header declares a plain global.
EEDbList<EEGameVariant, EeDataList::GameVariants> gGameVariants;   // Rust's list (ee/EEDbList.h)



// Loads the game variants (unit-set factors, cost, work-time, morale and earthquake factors) from db\dbGameVariant.dat
// into the Rust list (the record, its copy and the list: ee_data tables.rs); rows with a negative index are skipped.
// Returns the last read status
long EEGameVariant::Load(FSFileSpec* inFile)
{
    return EEDbLoadList(inFile, EeDataList::GameVariants);
}

// Empties the list
void EEGameVariant::Unload()
{
    gGameVariants.clear();
}

// Returns the factor of the first (set, factor) pair whose unit set holds the unit type, else 0
float EEGameVariant::GetFactor(long inUnitType)
{
    float theFactor = 0.0f;
    for (unsigned long i = 0; i < 10; i++) {
        if (!(this->mSets[i] > 0))
            continue;
        EEUnitSet* theSet = gUnitSetManager.GetSet(this->mSets[i]);
        if (theSet->Contains(inUnitType)) {
            theFactor = this->mFactors[i];
            break;
        }
    }
    return theFactor;
}
