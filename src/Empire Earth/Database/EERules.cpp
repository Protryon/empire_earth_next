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

#include "EEDataText.h"
#include "rust/ee_data.h"

#include "EERulesGenDecode.h"

const char* const kEERuleCategoryNames[] = {
    "ambient", "resource", "building", "airplane", "naval", "land", "land_citizen", "helicopter", "land8",
    "projectile", "submerged_naval", "bomb", "missile", "land_priest", "naval_priest", "land15", "land_transport",
    "helicopter_transport", "naval_transport", "building19", "torpedo", "naval_building", "naval_citizen",
    "land_military_hero", "mining_building", "gate", "helicopter_priest", "sea_animal", "air_animal", "fortress",
    "building_priest", "spaceship", "space_building", "space_transport", "planet_spaceship"};
const unsigned long kEERuleCategoryCount = sizeof(kEERuleCategoryNames) / sizeof(kEERuleCategoryNames[0]);

const char* const kEEAICheatNames[] = {"cost", "reveal", "epochs", "extra", "popband", "catchup", "retire"};
const unsigned long kEEAICheatCount = sizeof(kEEAICheatNames) / sizeof(kEEAICheatNames[0]);

long EEAICheatBit(const std::string& inName)
{
    for (unsigned long i = 0; i < kEEAICheatCount; i++)
        if (inName == kEEAICheatNames[i])
            return 1L << i;
    return 0;
}

EERuleNumber EERuleNumber::Constant(float inValue)
{
    EERuleNumber n;
    n.mFloat = inValue;
    n.mLong = (long)inValue;
    return n;
}

EERuleNumber EERuleNumber::Integer(long inValue)
{
    EERuleNumber n;
    n.mFloat = (float)inValue;
    n.mLong = inValue;
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

const EECivAttribute* EECivBuilderRules::Find(long inLevel) const
{
    for (const EECivAttribute& a : this->mAttributes)
        if (a.mLevel == inLevel)
            return &a;
    return nullptr;
}

const EECivAttribute* EECivBuilderRules::FindID(const std::string& inID) const
{
    for (const EECivAttribute& a : this->mAttributes)
        if (a.mID == inID)
            return &a;
    return nullptr;
}

const EECivCategoryText* EECivBuilderRules::FindCategory(long inRow) const
{
    for (const EECivCategoryText& c : this->mCategories)
        if (c.mRow == inRow)
            return &c;
    return nullptr;
}

const EECivPowerRule* EECivPowerRules::Find(long inRow) const
{
    for (const EECivPowerRule& p : this->mPowers)
        if (p.mPower == inRow)
            return &p;
    return nullptr;
}

const EEAIPersonalityRule& EEAIPersonalityRules::ForPlayer(unsigned long inPlayerID) const
{
    static const EEAIPersonalityRule kNone = [] { EEAIPersonalityRule r; r.mID = "balanced"; return r; }();
    if (this->mPersonalities.empty())
        return kNone;
    long k = 0;
    if (!this->mAssign.empty())
        k = this->mAssign[(inPlayerID > 0 ? inPlayerID - 1 : 0) % this->mAssign.size()];
    return this->mPersonalities[k >= 0 && (size_t)k < this->mPersonalities.size() ? k : 0];
}

long EEAIPersonalityRules::Find(const std::string& inID) const
{
    for (size_t k = 0; k < this->mPersonalities.size(); k++)
        if (this->mPersonalities[k].mID == inID)
            return (long)k;
    return -1;
}

long EEDifficultyAICheats(unsigned long inDifficulty)
{
    const std::vector<EEDifficultyRule>& theList = EEGetRules().mGameOptions.mDifficulties;
    return inDifficulty < theList.size() ? theList[inDifficulty].mAICheats : 0L;   // none (the original: all)
}

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

bool EERules::Load(EEDataText::Content& inContent, std::string& outError)
{
    EeDataBytes theError{nullptr, 0};
    if (!ee_data_rules_load(this->mRust, inContent.Handle(), &theError)) {
        outError = EEDataText::TakeBytes(theError);
        return false;
    }
    this->Refresh();
    return true;
}

void EERules::SetMusicFromFlags(long inSong, const bool* inFlags, unsigned long inCount)
{
    ee_data_rules_set_music_from_flags(this->mRust, (int32_t)inSong, inFlags, inCount);
    this->Refresh();
}

bool EERules::SetMusicFromDbmusicFile(const unsigned char* inData, size_t inSize)
{
    bool theOK = ee_data_rules_music_from_dbmusic(this->mRust, inData, inSize);
    this->Refresh();
    return theOK;
}

void EERules::Dump(std::string& outJson) const
{
    EeDataBytes theBytes{nullptr, 0};
    ee_data_rules_dump(this->mRust, &theBytes);
    outJson += EEDataText::TakeBytes(theBytes);
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
