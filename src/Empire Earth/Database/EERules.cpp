// EERules: the game rules that were literals in the code, as data (ee/EERules.h; docs/DATA_FORMATS.md 8.8). New unit
// (not in the original game). The fields and defaults are generated from tools/eedata/rules_schema.json; the reading,
// the checks and the dump are Rust (rust/crates/ee_data/src/rules.rs), which writes the C++ structs in place; this
// file holds the lookups the game uses. Portable C++17 (no platform or engine headers; the converter's gate
// builds it too).
// UNIT-FLAGS: -EHsc
#include "ee/EERules.h"

#include <cstdint>
#include <cstring>

#include "EEDataText.h"   // the ee_data C API

EERuleNumber EERuleNumber::Constant(float inValue)
{
    EERuleNumber n;
    n.mFloat = inValue;
    n.mLong = (long)inValue;
    return n;
}

EERuleNumber EERuleNumber::World(long inRow, int inValue)
{
    EERuleNumber n;
    n.mWorldRow = inRow;
    n.mWorldValue = inValue;
    return n;
}

bool EECombatRules::IsElevationAttacker(long inCategory) const { return this->mElevationAttackers.contains(inCategory); }
bool EECombatRules::IsElevationLauncher(long inCategory) const { return this->mElevationLaunchers.contains(inCategory); }
bool EECombatRules::IsElevationTarget(long inCategory) const { return this->mElevationTargets.contains(inCategory); }
bool EECombatRules::IsMoraleBuilding(long inUnitType) const { return this->mMoraleBuildings.contains(inUnitType); }

const EEEpochMedia& EERules::Epoch(long inEpoch) const
{
    static const EEEpochMedia kNone;
    if (this->mEpochs.empty())
        return kNone;
    size_t k = inEpoch < 0 ? 0 : (size_t)inEpoch;
    return this->mEpochs[k < this->mEpochs.size() ? k : this->mEpochs.size() - 1];
}

EERules::EERules() : mRust(ee_data_rules_new())
{
    this->Refresh();
}

EERules::~EERules()
{
    ee_data_rules_free(this->mRust);
}

void EERules::Refresh()
{
    ee_data_rules_mirror(this->mRust, static_cast<EERulesData*>(this), kEERulesMirrorSizes,
                         sizeof(kEERulesMirrorSizes) / sizeof(kEERulesMirrorSizes[0]));
}

void EERules::SetMusicFromFlags(long inSong, const bool* inFlags, unsigned long inCount)
{
    ee_data_rules_set_music_from_flags(this->mRust, (int32_t)inSong, inFlags, inCount);
    this->Refresh();
}

EERules& EEGetRules()
{
    static EERules sRules;
    return sRules;
}
