// C:\EEX\Empire Earth\Database\EEAnimal.h (RECONSTRUCTION; file name inferred; provisional names). The
// animal behaviour records of db\dbAnimals.dat (RTTI EEAnimal, no base; vtable 0x43e4ec, one slot: ??_G 0xd49da;
// sizeof 0x8c from Load's new). EEAnimal.cpp 0xd490f..0xd5405. Members from the ctor 0xd4958, the dtor 0xd49f6 and
// Load's copies (the record's seconds are stored as milliseconds: *1000). The plain members are EeDataAnimalFields
// (rust/crates/ee_data tables.rs, the record and its copy; R6 data round 2), the class's base.
#ifndef EE_EEANIMAL_H
#define EE_EEANIMAL_H

#include <vector>

#include "rust/data/records.h"
#include "rust/game/gamecpp.h"

class EEMedia;
class EEUnitClass;
class FSFileSpec;

class EEAnimal : public EeDataAnimalFields {
public:
    EEAnimal() : EeDataAnimalFields() {  }
    // +0x00 0xd49f6 (??_G 0xd49da)
    virtual ~EEAnimal() { ee_gamecpp_db_animal_free(this, true); }
    static long Load(FSFileSpec* inFile) { return ee_gamecpp_db_animal_load(inFile); }
    static void Unload() { ee_gamecpp_db_animal_unload(); }
    EEUnitClass* GetUnitClass();
    static EEAnimal* Find(long inUnitType); // 0xd5301 (the last record of that unit type)
    // inline (out of line COMDAT 0xa53da after its first user EEHerd.cpp: cdecl, sTable[inIndex]) (provisional name)
    static EEAnimal* Get(long inIndex) { return EEAnimal::sTable[inIndex]; }

    std::vector<long> mL04;                 // +0x04 (record +0x88..+0x94, the positive ones)
    std::vector<EEMedia*> mSounds;          // +0x14 (record +0xe8..+0xf0; LoadMedia'd)
    EEMedia* mMedia = nullptr;              // +0x24 (record +0xe4)
    // inline (EEHerd's GetRandom(min, max) calls load both before pushing: 0x4a475f) (provisional names)
    float GetNewbornGrowth() const { return this->mNewbornGrowth; }

    static std::vector<EEAnimal*> sTable;   // 0x8e60d0 (guard byte 0x8e60c8 bit 1)
};

#endif
