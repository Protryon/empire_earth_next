// EERules: the game rules that were literals in the code, as data (ee/EERules.h; docs/DATA_FORMATS.md 8.8). New unit
// (not in the original game). The fields and defaults are generated from tools/eedata/rules_schema.json; the reading,
// the checks and the dump are Rust (rust/crates/ee_data/src/rules.rs), which writes the C++ structs in place; this
// file holds the lookups the game uses. Portable C++17 (no platform or engine headers; the converter's gate
// builds it too).
// UNIT-FLAGS: -EHsc
#include "ee/EERules.h"

#include <cstdint>
#include <cstring>


const EEEpochMedia& EERules::Epoch(long inEpoch) const
{
    static const EEEpochMedia kNone = [] { EEEpochMedia theNone; ee_data_rules_epoch_none(&theNone); return theNone; }();
    return this->mEpochs.empty() ? kNone : this->mEpochs[ee_data_rules_epoch_index(inEpoch, this->mEpochs.size())];
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
