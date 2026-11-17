// C:\EEX\Empire Earth\Database\EEAnimal.h (RECONSTRUCTION; file name inferred; provisional names). The
// animal behaviour records of db\dbAnimals.dat (RTTI EEAnimal, no base; vtable 0x43e4ec, one slot: ??_G 0xd49da;
// sizeof 0x8c from Load's new). EEAnimal.cpp 0xd490f..0xd5405. Members from the ctor 0xd4958, the dtor 0xd49f6 and
// Load's copies (the record's seconds are stored as milliseconds: *1000). The plain members are EeDataAnimalFields
// (rust/crates/ee_data tables.rs, the record and its copy; R6 data round 2), the class's base.
#ifndef EE_EEANIMAL_H
#define EE_EEANIMAL_H

#include <vector>

#include "rust/data/tables.h"

class EEMedia;
class EEUnitClass;
class FSFileSpec;

class EEAnimal : public EeDataAnimalFields {
public:
    EEAnimal();
    virtual ~EEAnimal();                    // +0x00 0xd49f6 (??_G 0xd49da)
    void ClearSounds();
    static long Load(FSFileSpec* inFile);
    static void Unload();
    EEUnitClass* GetUnitClass();
    static EEAnimal* Find(long inUnitType); // 0xd5301 (the last record of that unit type)
    // inline (out of line COMDAT 0xa53da after its first user EEHerd.cpp: cdecl, sTable[inIndex]) (provisional name)
    static EEAnimal* Get(long inIndex) { return EEAnimal::sTable[inIndex]; }

    std::vector<long> mL04;                 // +0x04 (record +0x88..+0x94, the positive ones)
    std::vector<EEMedia*> mSounds;          // +0x14 (record +0xe8..+0xf0; LoadMedia'd)
    EEMedia* GetMedia() const { return this->mMedia; }
    EEMedia* mMedia;                        // +0x24 (record +0xe4)
    float GetGrowthStep() const { return this->mGrowthStep; }  // inline (EEHerd 0x4a31fd) (provisional name)
    // inline (EEHerd's GetRandom(min, max) calls load both before pushing: 0x4a475f) (provisional names)
    long GetMinHerdSize() const { return this->mMinHerdSize; }
    long GetMaxHerdSize() const { return this->mMaxHerdSize; }
    long GetMinBirthInterval() const { return this->mMinBirthInterval; }
    long GetMaxBirthInterval() const { return this->mMaxBirthInterval; }
    long GetMinHuntInterval() const { return this->mMinHuntInterval; }
    long GetMaxHuntInterval() const { return this->mMaxHuntInterval; }
    long GetMinGrazeTime() const { return this->mMinGrazeTime; }
    long GetMaxGrazeTime() const { return this->mMaxGrazeTime; }
    long GetMinRestInterval() const { return this->mMinRestInterval; }
    long GetMaxRestInterval() const { return this->mMaxRestInterval; }
    long GetMinFollowInterval() const { return this->mMinFollowInterval; }
    long GetMaxFollowInterval() const { return this->mMaxFollowInterval; }
    float GetNewbornGrowth() const { return this->mNewbornGrowth; }
    long GetAnimalID() const { return this->mAnimalID; }
    long GetMinTerritoryMargin() const { return this->mMinTerritoryMargin; }
    long GetMaxTerritoryMargin() const { return this->mMaxTerritoryMargin; }
    long GetHerdRadius() const { return this->mHerdRadius; }
    long GetGrowInterval() const { return this->mGrowInterval; }
    bool GetIsPredator() const { return this->mIsPredator; }
    bool GetHasLeader() const { return this->mHasLeader; }

    static std::vector<EEAnimal*> sTable;   // 0x8e60d0 (guard byte 0x8e60c8 bit 1)
};

#endif
