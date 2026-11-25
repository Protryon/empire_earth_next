// C:\EEX\Empire Earth\Database\EEAnimal.cpp (name from RTTI EEAnimal; no __FILE__). 0xd490f..0xd5405: the class
// static table's init chain, ctor, ??_G, dtor, ClearSounds, Load (db\dbAnimals.dat), Unload, GetUnitClass, Find, the
// vector<EEMedia*> push_back and _Xlen COMDATs, EEMedia::Get. EEAIUnitTargetingTable.cpp ends at 0xd490f,
// EEButtonObject.cpp's chains start at 0xd5405. Unit flags: the exe's module set.
// port (game core lane, round 10): the load, the unload and the sounds' release are Rust (ee_game gamecpp
// db_media.rs); their C++ calls and members are EECore10OpB4's cases.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"

#include "ee/EEAnimal.h"
#include "ee/EEMedia.h"
#include "ee/EEDbRecords.h"
#include "rust/game/gamecpp.h"

extern std::vector<EEUnitClass*> gUnitClassTable;   // ee/EEUnit.h
// 0x4e8448: the media table (its init chain is EEGraphicObject.cpp's, guarded: probably a class static of EEMedia;
// provisional name and form)
extern std::vector<EEMedia*> gMediaTable;

std::vector<EEAnimal*> EEAnimal::sTable;

// Returns the unit class of the animal type's unit
EEUnitClass* EEAnimal::GetUnitClass()
{
    return gUnitClassTable[this->mUnitType];
}

// Returns the animal type of a unit type (searching from the end of the table), or NULL
EEAnimal* EEAnimal::Find(long inUnitType)
{
    EEAnimal* theResult = NULL;
    for (long i = EEAnimal::sTable.size() - 1; i >= 0; i--) {
        if (EEAnimal::sTable[i] != NULL && EEAnimal::sTable[i]->mUnitType == inUnitType) {
            theResult = EEAnimal::sTable[i];
            break;
        }
    }
    return theResult;
}

// Returns the media table entry at an index, or NULL when out of range
EEMedia* EEMedia::Get(unsigned long inIndex)
{
    if (inIndex < gMediaTable.size())
        return gMediaTable[inIndex];
    return NULL;
}

// ------------------------------------------------------------------------- round 10, lane B: the animals' C++


#define P64(x) ((int64_t)(intptr_t)(x))
#define ANIMAL ((EEAnimal*)p)

