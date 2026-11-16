// C:\EEX\Empire Earth\Database\EEAnimal.cpp (name from RTTI EEAnimal; no __FILE__). 0xd490f..0xd5405: the class
// static table's init chain, ctor, ??_G, dtor, ClearSounds, Load (db\dbAnimals.dat), Unload, GetUnitClass, Find, the
// vector<EEMedia*> push_back and _Xlen COMDATs, EEMedia::Get. EEAIUnitTargetingTable.cpp ends at 0xd490f,
// EEButtonObject.cpp's chains start at 0xd5405. Unit flags: the exe's module set.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"

#include "ee/EEAnimal.h"
#include "ee/EEMedia.h"
#include "ee/EEDbRecords.h"

extern std::vector<EEUnitClass*> gUnitClassTable;   // ee/EEUnit.h
// 0x4e8448: the media table (its init chain is EEGraphicObject.cpp's, guarded: probably a class static of EEMedia;
// provisional name and form)
extern std::vector<EEMedia*> gMediaTable;

std::vector<EEAnimal*> EEAnimal::sTable;

// Creates an empty animal type: no media, sounds, flags or intervals
EEAnimal::EEAnimal() : EeDataAnimalFields()
{
    this->mMedia = NULL;
}

// Destroys the animal type, releasing its sounds and its media
EEAnimal::~EEAnimal()
{
    this->ClearSounds();
    if (this->mMedia != NULL)
        this->mMedia->UnloadMedia();
}

// Releases the animal type's sounds (last first) and empties its sound list
void EEAnimal::ClearSounds()
{
    for (long i = this->mSounds.size() - 1; i >= 0; i--)
        this->mSounds[i]->UnloadMedia();
    this->mSounds.clear();
}

// Loads the animal types from db\dbAnimals.dat (intervals from seconds to ms: the record and its copy are Rust's,
// ee_data tables.rs), loading each one's media and up to three sounds; returns the last read status
long EEAnimal::Load(FSFileSpec* inFile)
{
    if (EEAnimal::sTable.size() > 0)
        EEAnimal::Unload();
    return EEDbReadRecords(inFile, ee_data_animal_record_size(),
        [](unsigned long inCount) { EEAnimal::sTable.resize(inCount, NULL); return true; },
        [](unsigned long inIndex, const unsigned char* inRecord) {
            EEAnimal* theAnimal = new EEAnimal;
            EeDataAnimalMedia theMedia;
            ee_data_animal_from_record(inRecord, theAnimal, &theMedia);
            EEAnimal::sTable[inIndex] = theAnimal;
            if (theMedia.media >= 0) {
                theAnimal->mMedia = EEMedia::Get(theMedia.media);
                if (theAnimal->mMedia != NULL)
                    theAnimal->mMedia->LoadMedia();
            }
            for (int k = 0; k < 3; k++) {
                EEMedia* theSound = theMedia.sounds[k] >= 0 ? EEMedia::Get(theMedia.sounds[k]) : NULL;
                if (theSound != NULL) {
                    theSound->LoadMedia();
                    theAnimal->mSounds.push_back(theSound);
                }
            }
            for (int k = 0; k < 4; k++)
                if (theMedia.list[k] > 0)
                    theAnimal->mL04.push_back(theMedia.list[k]);
        });
}

// Destroys every loaded animal type and empties the table
void EEAnimal::Unload()
{
    for (long i = EEAnimal::sTable.size() - 1; i >= 0; i--)
        delete EEAnimal::sTable[i];
    EEAnimal::sTable.clear();
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
