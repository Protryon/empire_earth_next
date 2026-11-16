// C:\EEX\Empire Earth\...\EEGameVariant.h (RECONSTRUCTION; file path inferred; provisional names). A game variant
// (RTTI EEGameVariant, ctor/dtor around 0xd6910..0xd693c). The game keeps
// the variants in a vector<EEGameVariant*> at 0x4e7f78 (cleared at 0xd6b34); GetGameVariant 0xb0e20 returns entry i
// (an ICF-kept copy of an identical getter in the calamities' range).
#ifndef EE_EEGAMEVARIANT_H
#define EE_EEGAMEVARIANT_H

#include <vector>

#include "rust/ee_data.h"

class FSFileSpec;

// The record (EEGameVariantData) and its copy are Rust's: rust/crates/ee_data tables.rs (DbGameVariant).

// RTTI EEGameVariant (vtable 0x43e51c, one slot: ??_G 0xd6919; sizeof 0x84: the loader's `new`). The ctor 0xd6910 and
// dtor 0xd6935 only store the vptr (out of line, user-declared empty). Every member is copied from an
// EEGameVariantData record by the loader 0xd693c (types from the copy instructions: mov = long, fld/fstp = float).
// Layout gap-free; names are provisional unless a reader names them.
class EEGameVariant : public EeDataGameVariantFields {
public:
    EEGameVariant();
    virtual ~EEGameVariant();               // +0x00 0xd6935 (??_G 0xd6919)
    static EEGameVariant* GetGameVariant(long inIndex);
    static long Load(FSFileSpec* inFile);
    static void Unload();
    float GetFactor(long inUnitType);       // 0xd6b74: the float of the first (set, factor) pair whose set holds it
    // inline: the earthquake's Update loads it as an FP value before its fimul (provisional name)
    float GetEarthquakeDamageFactor() const { return this->mEarthquakeDamageFactor; }
    // inline: the tech tree's cost factors load first, before the other operands (provisional names)
    float GetEpochCostFactor() const { return this->mEpochCostFactor; }
    float GetFirstEpochCostFactor() const { return this->mFirstEpochCostFactor; }
    float GetSecondEpochCostFactor() const { return this->mSecondEpochCostFactor; }
    float GetCostSetFactor() const { return this->mCostSetFactor; }

    // the members are EeDataGameVariantFields (ee_data tables.rs): the pairs' factors and sets, the cost, work-time,
    // morale and earthquake factors, the index, name, cost set and selectable flag
};

// 0x4e7f78: every variant (GetGameVariant's table; 0xd6b34 deletes them; provisional name)
extern std::vector<EEGameVariant*> gGameVariants;
inline EEGameVariant* EEGetGameVariantAt(unsigned long inIndex) { return gGameVariants[inIndex]; }   // inline (lobby 0x5dde02) (provisional name)
// 0x1304b4: gGameVariants.size() (the lobby's variant box; provisional name)
unsigned long EEGetNumberOfGameVariants();

#endif
