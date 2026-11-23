// EERules: the game rules that were literals in the code, as data (ee/EERules.h; docs/DATA_FORMATS.md 8.8). New unit
// (not in the original game). The fields and defaults are generated from tools/eedata/rules_schema.json; the reading,
// the checks and the dump are Rust (rust/crates/ee_data/src/rules.rs): this file fills the C++ structs from its
// snapshot and holds the lookups the game uses. Portable C++17 (no platform or engine headers; the converter's gate
// builds it too).
// UNIT-FLAGS: -EHsc
#include "ee/EERules.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "EEDataText.h"   // TakeBytes

#include "EERulesGenDecode.h"

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

static bool Contains(const std::vector<long>& inList, long inValue)
{
    for (long v : inList)
        if (v == inValue)
            return true;
    return false;
}

bool EECombatRules::IsElevationAttacker(long inCategory) const { return Contains(this->mElevationAttackers, inCategory); }
bool EECombatRules::IsElevationLauncher(long inCategory) const { return Contains(this->mElevationLaunchers, inCategory); }
bool EECombatRules::IsElevationTarget(long inCategory) const { return Contains(this->mElevationTargets, inCategory); }
bool EECombatRules::IsMoraleBuilding(long inUnitType) const { return Contains(this->mMoraleBuildings, inUnitType); }

long EEDifficultyAILevel(unsigned long inDifficulty)
{
    const std::vector<EEDifficultyRule>& theList = EEGetRules().mGameOptions.mDifficulties;
    return inDifficulty < theList.size() ? theList[inDifficulty].mAILevel : (long)inDifficulty;
}

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
    EeDataBytes theBytes{nullptr, 0};
    ee_data_rules_encode(this->mRust, &theBytes);
    const unsigned char* p = theBytes.ptr;
    EERulesGen::DecodeAll(p, *this);
    ee_data_bytes_free(theBytes);
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

bool EERulesAddOverride(const std::string& inSpec, std::string& outError)
{
    EeDataBytes theError{nullptr, 0};
    if (!ee_data_rules_add_override(reinterpret_cast<const uint8_t*>(inSpec.data()), inSpec.size(), &theError)) {
        outError = EEDataText::TakeBytes(theError);
        return false;
    }
    return true;
}

void EERulesOverrideProblems(std::vector<std::string>& out)
{
    out.clear();
    EeDataBytes theBytes{nullptr, 0};
    ee_data_rules_override_problems(&theBytes);
    std::string theText = EEDataText::TakeBytes(theBytes);
    for (size_t theStart = 0; theStart < theText.size();) {
        size_t theEnd = theText.find('\n', theStart);
        out.push_back(theText.substr(theStart, theEnd - theStart));
        theStart = theEnd + 1;
    }
}
