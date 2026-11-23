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

// Creates an empty animal type: no media, sounds, flags or intervals
EEAnimal::EEAnimal() : EeDataAnimalFields()
{
}

// Destroys the animal type, releasing its sounds and its media
EEAnimal::~EEAnimal()
{
    ee_gamecpp_db_animal_free(this, true);
}

// Releases the animal type's sounds (last first) and empties its sound list
void EEAnimal::ClearSounds()
{
    ee_gamecpp_db_animal_free(this, false);
}

// Loads the animal types from db\dbAnimals.dat (intervals from seconds to ms: the record and its copy are Rust's,
// ee_data tables.rs), loading each one's media and up to three sounds; returns the last read status
long EEAnimal::Load(FSFileSpec* inFile)
{
    return ee_gamecpp_db_animal_load(inFile);
}

// Destroys every loaded animal type and empties the table
void EEAnimal::Unload()
{
    ee_gamecpp_db_animal_unload();
}

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

int64_t EECore10OpB5(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEDataTextFile.cpp

#define P64(x) ((int64_t)(intptr_t)(x))
#define ANIMAL ((EEAnimal*)p)

// the animals' C++ calls and members (gamecpp db_media.rs; p the animal or the medium): one statement each
int64_t EECore10OpB4(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::DanFields: return P64(static_cast<EeDataAnimalFields*>(ANIMAL));
    case O::DanNew: return P64(new EEAnimal);
    case O::DanDelete: delete ANIMAL;
        break;
    case O::DanResize: EEAnimal::sTable.resize((unsigned long)a[0], NULL);
        break;
    case O::DanCount: return (int64_t)EEAnimal::sTable.size();
    case O::DanAt: return P64(EEAnimal::sTable[a[0]]);
    case O::DanSet: EEAnimal::sTable[a[0]] = (EEAnimal*)a[1];
        break;
    case O::DanClear: EEAnimal::sTable.clear();
        break;
    case O::DanMedia: return P64(&ANIMAL->mMedia);
    case O::DanSoundCount: return (int64_t)ANIMAL->mSounds.size();
    case O::DanSoundAt: return P64(ANIMAL->mSounds[a[0]]);
    case O::DanSoundPush: ANIMAL->mSounds.push_back((EEMedia*)a[0]);
        break;
    case O::DanSoundsClear: ANIMAL->mSounds.clear();
        break;
    case O::DanListPush: ANIMAL->mL04.push_back((long)a[0]);
        break;
    case O::DanMediaGet: return P64(EEMedia::Get((unsigned long)a[0]));
    case O::DanMediaLoad: ((EEMedia*)p)->LoadMedia();
        break;
    case O::DanMediaUnload: ((EEMedia*)p)->UnloadMedia();
        break;
    default: return EECore10OpB5(inOp, p, a, f);
    }
    return 0;
}
