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

extern EEUnitClass* gUnitClassTable[];
// 0x4e8448: the media table (its init chain is EEGraphicObject.cpp's, guarded: probably a class static of EEMedia;
// provisional name and form)
extern std::vector<EEMedia*> gMediaTable;

// One record of db\dbAnimals.dat (0x110 bytes, read whole). Inferred layout (only the fields Load copies).
struct EEAnimalRecord {
    char mLabel[100];                       // +0x000 the row name (the game never reads it; the text's label)
    long mUnusedDatabaseID;                 // +0x064 the database row id (not read)
    long mAnimalID;                         // +0x68 -> +0x3c
    long mUnitType;                         // +0x6c -> +0x38
    long mMinHerdSize;                      // +0x70 -> +0x40
    long mMaxHerdSize;                      // +0x74 -> +0x44
    bool mBreeds;                           // +0x78 -> +0x85
    long mMinBirthInterval;                 // +0x7c -> +0x48 (s)
    long mMaxBirthInterval;                 // +0x80 -> +0x4c (s)
    bool mHasLeader;                        // +0x84 -> +0x87
    bool mUnusedFlag1;                      // +0x85 -> +0x88
    long mUnusedListValue1;                 // +0x88 -> mL04
    long mUnusedListValue2;                 // +0x8c -> mL04
    long mUnusedListValue3;                 // +0x90 -> mL04
    long mUnusedListValue4;                 // +0x94 -> mL04
    bool mUnusedIsPredator;                 // +0x98 -> +0x89
    float mUnusedFactor;                    // +0x9c -> +0x28 (copied with mov in the original)
    long mMinHuntInterval;                  // +0xa0 -> +0x50 (s)
    long mMaxHuntInterval;                  // +0xa4 -> +0x54 (s)
    long mMinRoamMargin;                    // +0xa8 -> +0x58
    long mMaxRoamMargin;                    // +0xac -> +0x5c
    long mMinGrazeTime;                     // +0xb0 -> +0x60 (s)
    long mMaxGrazeTime;                     // +0xb4 -> +0x64 (s)
    bool mUnusedIsFlightless;               // +0xb8 -> +0x84
    bool mIsPredator;                       // +0xb9 -> +0x86
    bool mUnusedFightsBack;                 // +0xba -> +0x83
    bool mHasParentalVengeance;             // +0xbb -> +0x80
    bool mHasGroupAttack;                   // +0xbc -> +0x81
    bool mHasGroupFlee;                     // +0xbd -> +0x82
    float mNewbornGrowth;                   // +0xc0 -> +0x30
    float mGrowthStep;                      // +0xc4 -> +0x34
    long mGrowInterval;                     // +0xc8 -> +0x6c (s)
    long mHerdRadius;                       // +0xcc -> +0x68
    long mMinRestInterval;                  // +0xd0 -> +0x70 (s)
    long mMaxRestInterval;                  // +0xd4 -> +0x74 (s)
    long mUnusedPredatorFleeChance;         // +0xd8 -> +0x2c
    long mMinFollowInterval;                // +0xdc -> +0x78 (s)
    long mMaxFollowInterval;                // +0xe0 -> +0x7c (s)
    long mMedia;                            // +0xe4 -> +0x24 (EEMedia::Get)
    long mSound1;                           // +0xe8 -> mSounds
    long mSound2;                           // +0xec -> mSounds
    long mSound3;                           // +0xf0 -> mSounds
    char _gapF4[0x1c];
};
static_assert(sizeof(EEAnimalRecord) == 272, "on-disk record: 272 bytes on every target");

std::vector<EEAnimal*> EEAnimal::sTable;

// FUNCTION: EXE 0x4d4958
// Creates an empty animal type: no media, sounds, flags or intervals
EEAnimal::EEAnimal()
{
    this->mF28 = 0.0f;
    this->mAnimalID = 0;
    this->mUnitType = 0;
    this->mMinHerdSize = 0;
    this->mMaxHerdSize = 0;
    this->mMinBirthInterval = 0;
    this->mMaxBirthInterval = 0;
    this->mMinHuntInterval = 0;
    this->mMaxHuntInterval = 0;
    this->mMinTerritoryMargin = 0;
    this->mMaxTerritoryMargin = 0;
    this->mMinGrazeTime = 0;
    this->mMaxGrazeTime = 0;
    this->mMedia = NULL;
    this->mB84 = false;
    this->mBreeds = false;
    this->mIsPredator = false;
    this->mHasLeader = false;
    this->mB88 = false;
    this->mB89 = false;
}

// SYNTHETIC: EXE 0x4d49da
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4d49f6
// Destroys the animal type, releasing its sounds and its media
EEAnimal::~EEAnimal()
{
    this->ClearSounds();
    if (this->mMedia != NULL)
        this->mMedia->UnloadMedia();
}

// FUNCTION: EXE 0x4d4a22
// Releases the animal type's sounds (last first) and empties its sound list
void EEAnimal::ClearSounds()
{
    for (long i = this->mSounds.size() - 1; i >= 0; i--)
        this->mSounds[i]->UnloadMedia();
    this->mSounds.clear();
}

// FUNCTION: EXE 0x4d4a5d
// Loads the animal types from db\dbAnimals.dat (intervals from seconds to ms), loading each one's media and up to three
// sounds; returns the last read status
long EEAnimal::Load(FSFileSpec* inFile)
{
    unsigned long theCount = 0;
    long theResult;
    unsigned long theRead = 0;
    EEAnimalRecord theRecord;
    EEMedia* theMedia;
    if (EEAnimal::sTable.size() > 0)
        EEAnimal::Unload();
    inFile->Open(kFileAccessRead);
    theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        EEAnimal::sTable.resize(theCount, NULL);
        for (unsigned long i = 0; i < theCount; i++) {
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(theRecord), theRead);
            EEAnimal::sTable[i] = new EEAnimal;
            EEAnimal::sTable[i]->mF28 = theRecord.mUnusedFactor;
            EEAnimal::sTable[i]->mAnimalID = theRecord.mAnimalID;
            EEAnimal::sTable[i]->mUnitType = theRecord.mUnitType;
            EEAnimal::sTable[i]->mMinHerdSize = theRecord.mMinHerdSize;
            EEAnimal::sTable[i]->mMaxHerdSize = theRecord.mMaxHerdSize;
            EEAnimal::sTable[i]->mMinBirthInterval = theRecord.mMinBirthInterval * 1000;
            EEAnimal::sTable[i]->mMaxBirthInterval = theRecord.mMaxBirthInterval * 1000;
            EEAnimal::sTable[i]->mMinHuntInterval = theRecord.mMinHuntInterval * 1000;
            EEAnimal::sTable[i]->mMaxHuntInterval = theRecord.mMaxHuntInterval * 1000;
            EEAnimal::sTable[i]->mMinTerritoryMargin = theRecord.mMinRoamMargin;
            EEAnimal::sTable[i]->mMaxTerritoryMargin = theRecord.mMaxRoamMargin;
            EEAnimal::sTable[i]->mMinGrazeTime = theRecord.mMinGrazeTime * 1000;
            EEAnimal::sTable[i]->mMaxGrazeTime = theRecord.mMaxGrazeTime * 1000;
            EEAnimal::sTable[i]->mB84 = theRecord.mUnusedIsFlightless;
            EEAnimal::sTable[i]->mBreeds = theRecord.mBreeds;
            EEAnimal::sTable[i]->mIsPredator = theRecord.mIsPredator;
            EEAnimal::sTable[i]->mHasLeader = theRecord.mHasLeader;
            EEAnimal::sTable[i]->mB88 = theRecord.mUnusedFlag1;
            EEAnimal::sTable[i]->mB89 = theRecord.mUnusedIsPredator;
            EEAnimal::sTable[i]->mB83 = theRecord.mUnusedFightsBack;
            EEAnimal::sTable[i]->mHasParentalVengeance = theRecord.mHasParentalVengeance;
            EEAnimal::sTable[i]->mHasGroupAttack = theRecord.mHasGroupAttack;
            EEAnimal::sTable[i]->mHasGroupFlee = theRecord.mHasGroupFlee;
            EEAnimal::sTable[i]->mNewbornGrowth = theRecord.mNewbornGrowth;
            EEAnimal::sTable[i]->mGrowthStep = theRecord.mGrowthStep;
            EEAnimal::sTable[i]->mGrowInterval = theRecord.mGrowInterval * 1000;
            EEAnimal::sTable[i]->mHerdRadius = theRecord.mHerdRadius;
            EEAnimal::sTable[i]->mMinRestInterval = theRecord.mMinRestInterval * 1000;
            EEAnimal::sTable[i]->mMaxRestInterval = theRecord.mMaxRestInterval * 1000;
            EEAnimal::sTable[i]->mL2C = theRecord.mUnusedPredatorFleeChance;
            EEAnimal::sTable[i]->mMinFollowInterval = theRecord.mMinFollowInterval * 1000;
            EEAnimal::sTable[i]->mMaxFollowInterval = theRecord.mMaxFollowInterval * 1000;
            if (theRecord.mMedia >= 0) {
                EEAnimal::sTable[i]->mMedia = EEMedia::Get(theRecord.mMedia);
                if (EEAnimal::sTable[i]->mMedia != NULL)
                    EEAnimal::sTable[i]->mMedia->LoadMedia();
            }
            if (theRecord.mSound1 >= 0) {
                theMedia = EEMedia::Get(theRecord.mSound1);
                if (theMedia != NULL) {
                    theMedia->LoadMedia();
                    EEAnimal::sTable[i]->mSounds.push_back(theMedia);
                }
            }
            if (theRecord.mSound2 >= 0) {
                theMedia = EEMedia::Get(theRecord.mSound2);
                if (theMedia != NULL) {
                    theMedia->LoadMedia();
                    EEAnimal::sTable[i]->mSounds.push_back(theMedia);
                }
            }
            if (theRecord.mSound3 >= 0) {
                theMedia = EEMedia::Get(theRecord.mSound3);
                if (theMedia != NULL) {
                    theMedia->LoadMedia();
                    EEAnimal::sTable[i]->mSounds.push_back(theMedia);
                }
            }
            if (theRecord.mUnusedListValue1 > 0)
                EEAnimal::sTable[i]->mL04.push_back(theRecord.mUnusedListValue1);
            if (theRecord.mUnusedListValue2 > 0)
                EEAnimal::sTable[i]->mL04.push_back(theRecord.mUnusedListValue2);
            if (theRecord.mUnusedListValue3 > 0)
                EEAnimal::sTable[i]->mL04.push_back(theRecord.mUnusedListValue3);
            if (theRecord.mUnusedListValue4 > 0)
                EEAnimal::sTable[i]->mL04.push_back(theRecord.mUnusedListValue4);
        }
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x4d52b4
// Destroys every loaded animal type and empties the table
void EEAnimal::Unload()
{
    for (long i = EEAnimal::sTable.size() - 1; i >= 0; i--)
        delete EEAnimal::sTable[i];
    EEAnimal::sTable.clear();
}

// FUNCTION: EXE 0x4d52f6
// Returns the unit class of the animal type's unit
EEUnitClass* EEAnimal::GetUnitClass()
{
    return gUnitClassTable[this->mUnitType];
}

// FUNCTION: EXE 0x4d5301
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

// FUNCTION: EXE 0x4d53e2
// Returns the media table entry at an index, or NULL when out of range
EEMedia* EEMedia::Get(unsigned long inIndex)
{
    if (inIndex < gMediaTable.size())
        return gMediaTable[inIndex];
    return NULL;
}

// TEMPLATE: EXE 0x4d5345
// std::vector<EEMedia*>::push_back

// TEMPLATE: EXE 0x4d5371
// std::vector<EEMedia*>::_Xlen (ICF-named for vector<EEUnit*>)

// SYNTHETIC: EXE 0x4d490f
// SYNTHETIC: EXE 0x4d4919
// SYNTHETIC: EXE 0x4d4931
// SYNTHETIC: EXE 0x4d493d
