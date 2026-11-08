// C:\EEX\Empire Earth\Database\EEAnimal.h (RECONSTRUCTION; file name inferred; provisional names). The
// animal behaviour records of db\dbAnimals.dat (RTTI EEAnimal, no base; vtable 0x43e4ec, one slot: ??_G 0xd49da;
// sizeof 0x8c from Load's new). EEAnimal.cpp 0xd490f..0xd5405. Members from the ctor 0xd4958, the dtor 0xd49f6 and
// Load's copies (the record's seconds are stored as milliseconds: *1000).
#ifndef EE_EEANIMAL_H
#define EE_EEANIMAL_H

#include <vector>

class EEMedia;
class EEUnitClass;
class FSFileSpec;

class EEAnimal {
public:
    EEAnimal();                             // 0xd4958
    virtual ~EEAnimal();                    // +0x00 0xd49f6 (??_G 0xd49da)
    void ClearSounds();                     // 0xd4a22
    static long Load(FSFileSpec* inFile);   // 0xd4a5d
    static void Unload();                   // 0xd52b4
    EEUnitClass* GetUnitClass();            // 0xd52f6
    static EEAnimal* Find(long inUnitType); // 0xd5301 (the last record of that unit type)
    // inline (out of line COMDAT 0xa53da after its first user EEHerd.cpp: cdecl, sTable[inIndex]) (provisional name)
    static EEAnimal* Get(long inIndex) { return EEAnimal::sTable[inIndex]; }

    std::vector<long> mL04;                 // +0x04 (record +0x88..+0x94, the positive ones)
    std::vector<EEMedia*> mSounds;          // +0x14 (record +0xe8..+0xf0; LoadMedia'd)
    EEMedia* GetMedia() const { return this->mMedia; }
    EEMedia* mMedia;                        // +0x24 (record +0xe4)
    float mF28;                             // +0x28 (ctor 0.0f; Load copies it with mov)
    long mL2C;                              // +0x2c
    float mNewbornGrowth;                   // +0x30 (a float: EEHerd 0x4a3dfc fld; Load copies the record's with mov)
    float mGrowthStep;                      // +0x34 (a float: EEHerd 0x4a31fd fld; Load copies the record's with mov)
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
    long mUnitType;                         // +0x38 (gUnitClassTable index)
    long mAnimalID;                         // +0x3c
    long mMinHerdSize;                      // +0x40
    long mMaxHerdSize;                      // +0x44
    long mMinBirthInterval;                 // +0x48 (ms)
    long mMaxBirthInterval;                 // +0x4c (ms)
    long mMinHuntInterval;                  // +0x50 (ms)
    long mMaxHuntInterval;                  // +0x54 (ms)
    long mMinTerritoryMargin;               // +0x58
    long mMaxTerritoryMargin;               // +0x5c
    long mMinGrazeTime;                     // +0x60 (ms)
    long mMaxGrazeTime;                     // +0x64 (ms)
    long mHerdRadius;                       // +0x68
    long mGrowInterval;                     // +0x6c (ms)
    long mMinRestInterval;                  // +0x70 (ms)
    long mMaxRestInterval;                  // +0x74 (ms)
    long mMinFollowInterval;                // +0x78 (ms)
    long mMaxFollowInterval;                // +0x7c (ms)
    bool mHasParentalVengeance;             // +0x80
    bool mHasGroupAttack;                   // +0x81
    bool mHasGroupFlee;                     // +0x82
    bool mB83;                              // +0x83
    bool mB84;                              // +0x84
    bool mBreeds;                           // +0x85
    bool GetIsPredator() const { return this->mIsPredator; }
    bool mIsPredator;                       // +0x86
    bool GetHasLeader() const { return this->mHasLeader; }
    bool mHasLeader;                        // +0x87
    bool mB88;                              // +0x88
    bool mB89;                              // +0x89

    static std::vector<EEAnimal*> sTable;   // 0x8e60d0 (guard byte 0x8e60c8 bit 1)
};

#endif
