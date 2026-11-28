// C:\EEX\Empire Earth\...\EEGameVariant.h (RECONSTRUCTION; file path inferred; provisional names). A game variant
// (RTTI EEGameVariant, ctor/dtor around 0xd6910..0xd693c). The game keeps
// the variants in a vector<EEGameVariant*> at 0x4e7f78 (cleared at 0xd6b34); GetGameVariant 0xb0e20 returns entry i
// (an ICF-kept copy of an identical getter in the calamities' range).
#ifndef EE_EEGAMEVARIANT_H
#define EE_EEGAMEVARIANT_H

#include "ee/EEDbList.h"
#include "rust/data/records.h"
#include "rust/sim/app.h"   // the unit sets

// The record (EEGameVariantData) and its copy are Rust's: rust/crates/ee_data tables.rs (DbGameVariant).

class EEGameVariant;
// 0x4e7f78: every variant (GetGameVariant's table; 0xd6b34 deletes them; provisional name)
extern EEDbList<EEGameVariant, EeDataList::GameVariants> gGameVariants;   // Rust's list (ee/EEDbList.h)

// RTTI EEGameVariant (vtable 0x43e51c, one slot: ??_G 0xd6919; sizeof 0x84: the loader's `new`). The ctor 0xd6910 and
// dtor 0xd6935 only store the vptr (out of line, user-declared empty). Every member is copied from an
// EEGameVariantData record by the loader 0xd693c (types from the copy instructions: mov = long, fld/fstp = float).
// Layout gap-free; names are provisional unless a reader names them.
class EEGameVariant : public EeDataGameVariantFields {
public:
    // Returns the game variant with the given index
    // Probably inline in the real header (an out-of-line COMDAT right after its first user, EECCEarthquake's Update);
    // defined here, with the earthquake's update parameters (Update above)
    static EEGameVariant* GetGameVariant(long inIndex) { return gGameVariants[inIndex]; }
    // db\dbGameVariant.dat into the Rust list (rows with a negative index skipped); the last read status (requires
    // ee/EEDbRecords.h). Header-only since the game's leaves round 2 (EEGameVariant.cpp went).
    template <class F> static long Load(F* inFile) { return EEDbLoadList(inFile, EeDataList::GameVariants); }
    static void Unload() { ee_data_list_clear(EeDataList::GameVariants); }
    // 0xd6b74: the float of the first (set, factor) pair whose unit set holds the type, else 0
    float GetFactor(long inUnitType) const
    {
        for (unsigned long i = 0; i < 10; i++) {
            if (this->mSets[i] > 0 && ee_unit_sets_contains((size_t)this->mSets[i], (uint32_t)inUnitType))
                return this->mFactors[i];
        }
        return 0.0f;
    }
    // inline: the earthquake's Update loads it as an FP value before its fimul (provisional name)
    float GetEarthquakeDamageFactor() const { return this->mEarthquakeDamageFactor; }
    // inline: the tech tree's cost factors load first, before the other operands (provisional names)
    float GetCostSetFactor() const { return this->mCostSetFactor; }

    // the members are EeDataGameVariantFields (ee_data tables.rs): the pairs' factors and sets, the cost, work-time,
    // morale and earthquake factors, the index, name, cost set and selectable flag
};

inline EEGameVariant* EEGetGameVariantAt(unsigned long inIndex) { return gGameVariants[inIndex]; }   // inline (lobby 0x5dde02) (provisional name)
// 0x1304b4: gGameVariants.size() (the lobby's variant box; provisional name)
unsigned long EEGetNumberOfGameVariants();

#endif
