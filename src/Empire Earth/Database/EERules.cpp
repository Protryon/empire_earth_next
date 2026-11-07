// EERules: the game rules that were literals in the code, as data (ee/EERules.h; docs/DATA_FORMATS.md 8.8). New unit
// (not in the original game). Portable C++17: no platform or engine headers (the converter's gate builds it too).
// The defaults are the original's literals; tools/eedata/rules.py writes the same values as the vanilla documents,
// and eedata.py cppgate checks that both dump identically.
// UNIT-FLAGS: -EHsc
#include "ee/EERules.h"
#include "EEDataText.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ee/EECivPowerIDs.h"
#include "ee/EEStringIDs.h"
#include "ee/EETechTreeTechID.h"
#include "ee/EEUnitClassIDs.h"

using EEDataText::Failure;
using EEDataText::Node;

const char* const kEERuleCategoryNames[] = {
    "ambient", "resource", "building", "airplane", "naval", "land", "land_citizen", "helicopter", "land8",
    "projectile", "submerged_naval", "bomb", "missile", "land_priest", "naval_priest", "land15", "land_transport",
    "helicopter_transport", "naval_transport", "building19", "torpedo", "naval_building", "naval_citizen",
    "land_military_hero", "mining_building", "gate", "helicopter_priest", "sea_animal", "air_animal", "fortress",
    "building_priest", "spaceship", "space_building", "space_transport", "planet_spaceship"};
const unsigned long kEERuleCategoryCount = sizeof(kEERuleCategoryNames) / sizeof(kEERuleCategoryNames[0]);
static_assert(sizeof(kEERuleCategoryNames) / sizeof(kEERuleCategoryNames[0]) == kUnitCategoryPlanetSpaceship + 1,
              "a name per unit category");

static const char* const kStatNames[] = {
    "attack", "hit_points", "reload_time", "armor", "range", "line_of_sight", "speed", "flight_time", "area_effect",
    "gather_time", "build_time", "conversion", "repair_rate", "area_effect_scale", "high_ground", "population",
    "cost", "power"};
static_assert(sizeof(kStatNames) / sizeof(kStatNames[0]) == kCivStatCount, "a name per stat");
static const char* const kModeNames[] = {"share", "add", "multiply"};
static_assert(sizeof(kModeNames) / sizeof(kModeNames[0]) == kCivModeCount, "a name per mode");
static const char* const kActionNames[] = {
    "research_if_researched", "load_sound", "load_effect", "pulse_sight", "set_mining_limit", "enable_camouflage",
    "allow_tech", "disallow_tech"};
static_assert(sizeof(kActionNames) / sizeof(kActionNames[0]) == kPowerActionCount, "a name per action");

// ------------------------------------------------------------------------------------------------ defaults

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

EECombatRules::EECombatRules()
    : mElevationAttackers{kUnitCategoryLand, kUnitCategoryLandCitizen},
      mElevationLaunchers{kUnitCategoryProjectile, kUnitCategoryBomb, kUnitCategoryMissile, kUnitCategoryTorpedo},
      mElevationTargets{kUnitCategoryLand, kUnitCategoryBuilding, kUnitCategoryLandCitizen, kUnitCategoryLandPriest,
                        kUnitCategoryBuilding19, kUnitCategoryLand15, kUnitCategoryLandMilitaryHero,
                        kUnitCategoryLandTransport, kUnitCategoryFortress, kUnitCategoryMiningBuilding,
                        kUnitCategoryBuildingPriest, kUnitCategoryGate},
      mMoraleBuildings{kUnitTypeHouse}
{
}

bool EECombatRules::IsElevationAttacker(long inCategory) const { return Contains(this->mElevationAttackers, inCategory); }
bool EECombatRules::IsElevationLauncher(long inCategory) const { return Contains(this->mElevationLaunchers, inCategory); }
bool EECombatRules::IsElevationTarget(long inCategory) const { return Contains(this->mElevationTargets, inCategory); }
bool EECombatRules::IsMoraleBuilding(long inUnitType) const { return Contains(this->mMoraleBuildings, inUnitType); }

EEGameOptionRules::EEGameOptionRules()
{
    static const char* const kSizes[] = {"tiny", "small", "medium", "large", "huge", "gigantic"};
    static const long kTiles[] = {42, 58, 75, 105, 160, 220};
    for (long i = 0; i < 6; i++)
        this->mMapSizes.push_back(EEMapSizeRule{kSizes[i], IDS_MAP_SIZE_FIRST + i, kTiles[i], i});
    // the four speeds: dbworld's game_speed_slow_normal F / F2, game_speed_fast_fastest F / F2 (EEGameState::Create)
    this->mSpeeds.push_back(EESpeedRule{"slow", IDS_INGAME_SLOW, EERuleNumber::World(81, 0)});
    this->mSpeeds.push_back(EESpeedRule{"normal", IDS_STANDARD, EERuleNumber::World(81, 1)});
    this->mSpeeds.push_back(EESpeedRule{"fast", IDS_INGAME_FAST, EERuleNumber::World(82, 0)});
    this->mSpeeds.push_back(EESpeedRule{"very_fast", IDS_VERY_FAST, EERuleNumber::World(82, 1)});
    // the damage tables (EEToHitTable::GetAttackFactor): dbworld's difficulty_damage_scale F / F2, none on medium
    this->mDifficulties.push_back(EEDifficultyRule{"easy", IDS_LOBBY_EASY, 0, EERuleNumber::World(89, 0),
                                                   EERuleNumber::World(89, 1)});
    this->mDifficulties.push_back(EEDifficultyRule{"medium", IDS_LOBBY_MEDIUM, 1, EERuleNumber::Constant(1.0f),
                                                   EERuleNumber::Constant(1.0f)});
    this->mDifficulties.push_back(EEDifficultyRule{"hard", IDS_LOBBY_HARD, 2, EERuleNumber::World(89, 1),
                                                   EERuleNumber::World(89, 0)});
}

EECivBuilderRules::EECivBuilderRules()
{
    // the original's switch (EECivilizationManager::ApplyUpgradeLevel): the level is the attribute
    struct Default {
        const char* mID;
        long mLevel;
        long mStat, mSlot, mOnly, mExcept;
        long mStat2;   // a second stat, or -1
    };
    static const Default kDefaults[] = {
        {"attack", 1, kCivStatAttack, -1, -1, -1, -1},
        {"hit_points", 2, kCivStatHitPoints, -1, -1, -1, -1},
        {"reload_time", 3, kCivStatReloadTime, -1, -1, -1, -1},
        {"armor", 4, kCivStatArmor, -1, -1, -1, -1},
        {"range", 5, kCivStatRange, -1, -1, -1, kCivStatLineOfSight},
        {"speed", 6, kCivStatSpeed, -1, -1, -1, -1},
        {"flight_time", 7, kCivStatFlightTime, -1, -1, -1, -1},
        {"area_effect", 8, kCivStatAreaEffect, -1, -1, -1, -1},
        {"gather_stone_gold", 9, kCivStatGatherTime, 5, -1, kUnitCategoryNavalCitizen, -1},
        {"gather_slot_0", 10, kCivStatGatherTime, 0, -1, -1, -1},
        {"gather_slot_2", 11, kCivStatGatherTime, 2, -1, -1, -1},
        {"gather_slot_6", 12, kCivStatGatherTime, 6, -1, -1, -1},
        {"gather_slot_1", 13, kCivStatGatherTime, 1, -1, -1, -1},
        {"gather_slot_3", 14, kCivStatGatherTime, 3, -1, -1, -1},
        {"build_time", 15, kCivStatBuildTime, -1, -1, -1, -1},
        {"conversion", 16, kCivStatConversion, -1, -1, -1, -1},
        {"fishing", 17, kCivStatGatherTime, 4, kUnitCategoryNavalCitizen, -1, -1},
        {"repair_rate", 18, kCivStatRepairRate, -1, -1, -1, -1},
        {"area_effect_scale", 19, kCivStatAreaEffectScale, -1, -1, -1, -1},
        {"high_ground", 20, kCivStatHighGround, -1, -1, -1, -1},
        {"population", 21, kCivStatPopulation, -1, -1, -1, -1},
        {"cost", 23, kCivStatCost, -1, -1, -1, -1},
    };
    for (const Default& d : kDefaults) {
        EECivAttribute a;
        a.mID = d.mID;
        a.mLevel = d.mLevel;
        a.mNameID = IDS_CIV_BONUS_BASE + d.mLevel;
        EECivApply x;
        x.mStat = d.mStat;
        x.mSlot = d.mSlot;
        x.mOnlyCategory = d.mOnly;
        x.mExceptCategory = d.mExcept;
        a.mApplies.push_back(x);
        if (d.mLevel == 9) {   // stone and gold: gather time slots 5 and 4, except naval citizens
            x.mSlot = 4;
            a.mApplies.push_back(x);
        }
        if (d.mStat2 >= 0) {
            EECivApply y;
            y.mStat = d.mStat2;
            a.mApplies.push_back(y);
        }
        this->mAttributes.push_back(a);
    }
    // levels 0x26..0x3c: the civ powers (dbcivpowers rows 0..22; a power's id is its row + 0x26)
    static const char* const kPowers[] = {
        "flaming_arrows", "slavery", "priest_tower", "crusaders", "fanaticism", "emissaries", "exploration",
        "insurance", "metallurgy", "advanced_mining", "pathfinding", "camouflage", "bundeswehr", "sas", "paratroopers",
        "market", "cyber_ninja", "missile_base", "cloaking", "adaptation", "expansionism", "conquistadors",
        "just_in_time_manufacturing"};
    for (long i = 0; i < 23; i++) {
        EECivAttribute a;
        a.mID = kPowers[i];
        a.mLevel = kCivPowerFlamingArrows + i;
        a.mNameID = IDS_CIV_POWER_BASE + a.mLevel;
        EECivApply x;
        x.mStat = kCivStatPower;
        x.mPower = i;
        a.mApplies.push_back(x);
        this->mAttributes.push_back(a);
    }
}

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

EECivPowerRules::EECivPowerRules()
{
    // the original's switch (EECivPowersManager::ApplyPower); sounds and effects by dbsounds / dbgfxeffects row
    // (EEDataTextFile.cpp static_asserts them against EESoundObject.h / EEGraphicEffect.h)
    const long kSoundSlavery = 0x422, kSoundPriestTowerConvert = 0x41c, kSoundLightning = 0x176,
               kSoundThunder2 = 0x1ba;
    const long kEffectPriestTower1 = 247, kEffectPriestTower2 = 256, kEffectLightningUnit = 0x15,
               kEffectDust = 0xab, kEffectDisintegration = 0xec;
    auto research = [](long inIf, long inTech) {
        EECivPowerEffect e;
        e.mAction = kPowerResearchIfResearched;
        e.mIfTech = inIf;
        e.mTech = inTech;
        return e;
    };
    auto sound = [](long inSound) {
        EECivPowerEffect e;
        e.mAction = kPowerLoadSound;
        e.mSound = inSound;
        return e;
    };
    auto effect = [](long inEffect) {
        EECivPowerEffect e;
        e.mAction = kPowerLoadEffect;
        e.mEffect = inEffect;
        return e;
    };
    auto simple = [](long inAction, long inTech = -1, long inUnitType = -1) {
        EECivPowerEffect e;
        e.mAction = inAction;
        e.mTech = inTech;
        e.mUnitType = inUnitType;
        return e;
    };
    const long base = kCivPowerFlamingArrows;
    this->mPowers.push_back({kCivPowerFlamingArrows - base,
                             {research(kTechSimpleBowmanResearch, kTechSimpleBowmanFlamingResearch),
                              research(kTechCompositeBowUpgrade, kTechCompositeBowFlamingUpgrade),
                              research(kTechLongBowUpgrade, kTechLongBowFlamingUpgrade)}});
    this->mPowers.push_back({kCivPowerSlavery - base, {sound(kSoundSlavery)}});
    this->mPowers.push_back({kCivPowerPriestTower - base,
                             {sound(kSoundPriestTowerConvert), effect(kEffectPriestTower1), effect(kEffectPriestTower2)}});
    this->mPowers.push_back({kCivPowerCrusaders - base,
                             {research(kTechShortSwordUpgrade, kTechShortSwordUpgradeCrusaders),
                              research(kTechLongSwordUpgrade, kTechLongSwordUpgradeCrusaders),
                              research(kTechKnightUpgrade, kTechKnightUpgradeCrusaders)}});
    this->mPowers.push_back({kCivPowerExploration - base,
                             {simple(kPowerPulseSight, -1, kUnitTypeTownCenter),
                              simple(kPowerPulseSight, -1, kUnitTypeCapitol)}});
    this->mPowers.push_back({kCivPowerAdvancedMining - base, {simple(kPowerSetMiningLimit)}});
    this->mPowers.push_back({kCivPowerCamouflage - base, {simple(kPowerEnableCamouflage)}});
    this->mPowers.push_back({kCivPowerBundeswehr - base,
                             {sound(kSoundLightning), sound(kSoundThunder2), effect(kEffectLightningUnit),
                              effect(kEffectDust), effect(kEffectDisintegration)}});
    this->mPowers.push_back({kCivPowerExpansionism - base,
                             {simple(kPowerDisallowTech, kTechSettlement), simple(kPowerAllowTech, kTechTownCenter)}});
}

const EECivPowerRule* EECivPowerRules::Find(long inRow) const
{
    for (const EECivPowerRule& p : this->mPowers)
        if (p.mPower == inRow)
            return &p;
    return nullptr;
}

EEEpochMedia EEDefaultEpochMedia(long inEpoch)
{
    // the original's per-epoch citizen rules (EEPlayUnitEffect, EEUGGather, EEUnitClass::Attack); epoch positions
    // (kTechEpoch*); effects: lightning (unit) 0x15, dust 0xab, disintegration 0xec (dbgfxeffects rows)
    EEEpochMedia m;
    m.mHighTechTools = inEpoch >= kTechEpochNano;
    if (inEpoch <= kTechEpochBronze) {
        m.mRemovalEffects = {0x15, 0xab, 0xab, 0xab};
        m.mRemovalThunderEvery = 2;
    } else if (inEpoch <= kTechEpochIndustrial) {
        m.mRemovalEffects = {0xec, 0xab};
        m.mRemovalThunderEvery = 3;
    } else if (inEpoch <= kTechEpochAtomicModern) {
        m.mRemovalEffects = {0xec, 0xab};
        m.mRemovalPitchOffset = -0.2f;
    } else {
        m.mRemovalEffects = {0xec, 0xab};
    }
    return m;
}

EERules::EERules()
{
    for (long k = 0; k <= kTechEpochSpace; k++)
        this->mEpochs.push_back(EEDefaultEpochMedia(k));
}

EERules& EEGetRules()
{
    static EERules sRules;
    return sRules;
}

EEAIPersonalityRules::EEAIPersonalityRules()
{
    EEAIPersonalityRule theBalanced;
    theBalanced.mID = "balanced";
    EEAIPersonalityRule theTall;            // tech: saves for the next epoch early and hard
    theTall.mID = "tall";
    theTall.mEpochPressureStart = 0.15f;
    theTall.mEpochPressureFull = 0.5f;
    theTall.mStandingArmy = 0.2f;
    EEAIPersonalityRule theWide;            // boom: grows its workforce, advances late
    theWide.mID = "wide";
    theWide.mEpochPressureStart = 0.6f;
    theWide.mEpochPressureFull = 1.0f;
    theWide.mMaxCitizenShare = 0.8f;
    theWide.mStandingArmy = 0.15f;
    EEAIPersonalityRule theAggressive;      // rush: a large standing army, a middling epoch pace
    theAggressive.mID = "aggressive";
    theAggressive.mEpochPressureStart = 0.4f;
    theAggressive.mEpochPressureFull = 0.9f;
    theAggressive.mStandingArmy = 0.5f;
    theAggressive.mArmyFloor = 0.3f;
    theAggressive.mPrehistoricArmy = 0;     // the rush keeps its early army
    this->mPersonalities.push_back(theBalanced);
    this->mPersonalities.push_back(theTall);
    this->mPersonalities.push_back(theWide);
    this->mPersonalities.push_back(theAggressive);
    this->mAssign.push_back(0);
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

const char* const kEEAICheatNames[] = {"cost", "reveal", "epochs", "extra", "popband", "catchup", "retire"};
const unsigned long kEEAICheatCount = sizeof(kEEAICheatNames) / sizeof(kEEAICheatNames[0]);

long EEAICheatBit(const std::string& inName)
{
    for (unsigned long i = 0; i < kEEAICheatCount; i++)
        if (inName == kEEAICheatNames[i])
            return 1L << i;
    return 0;
}

long EEDifficultyAICheats(unsigned long inDifficulty)
{
    const std::vector<EEDifficultyRule>& theList = EEGetRules().mGameOptions.mDifficulties;
    return inDifficulty < theList.size() ? theList[inDifficulty].mAICheats : (long)kAICheatAll;
}

long EEDifficultyAILevel(unsigned long inDifficulty)
{
    const std::vector<EEDifficultyRule>& theList = EEGetRules().mGameOptions.mDifficulties;
    return inDifficulty < theList.size() ? theList[inDifficulty].mAILevel : (long)inDifficulty;
}

const EEEpochMedia& EERules::Epoch(long inEpoch) const
{
    static const EEEpochMedia kNone = EEDefaultEpochMedia(0);
    if (this->mEpochs.empty())
        return kNone;
    size_t k = inEpoch < 0 ? 0 : (size_t)inEpoch;
    return this->mEpochs[k < this->mEpochs.size() ? k : this->mEpochs.size() - 1];
}

void EERules::SetMusicFromFlags(long inSong, const bool* inFlags, unsigned long inCount)
{
    if (this->mEpochMusicFromText)
        return;
    for (unsigned long k = 0; k < this->mEpochs.size(); k++) {
        // the space age (past the flags) plays the last flag's songs (the original's EEMusicObject ctor)
        unsigned long f = k < inCount ? k : inCount - 1;
        if (inCount > 0 && k <= inCount && inFlags[f])
            this->mEpochs[k].mMusic.push_back(inSong);
    }
}

// ------------------------------------------------------------------------------------------------ reading

namespace {

[[noreturn]] void Fail(const std::string& inText)
{
    throw Failure{inText};
}

// a mapping read key by key; a key nobody asked for is an error
struct Map {
    const Node& mNode;
    std::string mDoc;
    std::vector<bool> mUsed;

    Map(const Node& inNode, const std::string& inDoc, const char* inWhat) : mNode(inNode), mDoc(inDoc)
    {
        if (inNode.mKind != Node::kMap)
            Fail(this->Where(inNode) + ": " + inWhat + " must be a mapping");
        this->mUsed.resize(inNode.mKeys.size());
    }
    std::string Where(const Node& inAt) const { return this->mDoc + " line " + std::to_string(inAt.mLine); }
    std::string Where() const { return this->Where(this->mNode); }
    const Node* Get(const char* inKey)
    {
        for (size_t i = 0; i < this->mNode.mKeys.size(); i++)
            if (this->mNode.mKeys[i] == inKey) {
                this->mUsed[i] = true;
                return this->mNode.mItems[i].IsMissing() ? nullptr : &this->mNode.mItems[i];
            }
        return nullptr;
    }
    void Done() const
    {
        for (size_t i = 0; i < this->mUsed.size(); i++)
            if (!this->mUsed[i])
                Fail(this->Where(this->mNode.mItems[i]) + ": unknown key '" + this->mNode.mKeys[i] + "'");
    }
    // typed values: the default stays when the key is absent
    void Int(const char* inKey, long& ioValue)
    {
        if (const Node* v = this->Get(inKey))
            ioValue = EEDataText::NodeInt(v, this->Where(*v), inKey);
    }
    void Float(const char* inKey, float& ioValue)
    {
        if (const Node* v = this->Get(inKey))
            ioValue = EEDataText::NodeFloat(v, this->Where(*v), inKey);
    }
    void Bool(const char* inKey, bool& ioValue)
    {
        if (const Node* v = this->Get(inKey))
            ioValue = EEDataText::NodeBool(v, this->Where(*v), inKey);
    }
    void Number(const char* inKey, EERuleNumber& ioValue)
    {
        const Node* v = this->Get(inKey);
        if (!v)
            return;
        const std::string& t = EEDataText::NodeScalar(v, this->Where(*v), inKey);
        if (!t.empty() && ((t[0] >= 'a' && t[0] <= 'z') && t != "inf" && t != "nan")) {
            EERuleNumber n;
            EEDataText::WorldValue(t, this->Where(*v), inKey, n.mWorldRow, n.mWorldValue);
            ioValue = n;
        } else if (t.find_first_of(".eE") == std::string::npos || t.compare(0, 2, "0x") == 0) {
            ioValue = EERuleNumber::Integer(EEDataText::NodeInt(v, this->Where(*v), inKey));
        } else {
            ioValue = EERuleNumber::Constant(EEDataText::NodeFloat(v, this->Where(*v), inKey));
        }
    }
    const Node* List(const char* inKey)
    {
        const Node* v = this->Get(inKey);
        if (v && v->mKind != Node::kList)
            Fail(this->Where(*v) + ": " + inKey + " must be a list");
        return v;
    }
    const Node* Sub(const char* inKey)
    {
        const Node* v = this->Get(inKey);
        if (v && v->mKind != Node::kMap)
            Fail(this->Where(*v) + ": " + inKey + " must be a mapping");
        return v;
    }
};

long Named(const Node* inValue, const std::string& inWhere, const char* inKey, const char* const* inNames,
           unsigned long inCount, const char* inWhat)
{
    const std::string& t = EEDataText::NodeScalar(inValue, inWhere, inKey);
    for (unsigned long i = 0; i < inCount; i++)
        if (t == inNames[i])
            return (long)i;
    Fail(inWhere + ": " + inKey + ": '" + t + "' is not " + inWhat);
}

long Category(const Node* inValue, const std::string& inWhere, const char* inKey)
{
    return Named(inValue, inWhere, inKey, kEERuleCategoryNames, kEERuleCategoryCount, "a unit category");
}

std::vector<long> Categories(Map& m, const char* inKey, const std::vector<long>& inDefault)
{
    const Node* v = m.List(inKey);
    if (!v)
        return inDefault;
    std::vector<long> out;
    for (const Node& x : v->mItems)
        out.push_back(Category(&x, m.Where(x), inKey));
    return out;
}

std::vector<long> Refs(Map& m, EEDataText::Content& c, const char* inKey, const char* inTable,
                       const std::vector<long>& inDefault)
{
    const Node* v = m.List(inKey);
    if (!v)
        return inDefault;
    std::vector<long> out;
    for (const Node& x : v->mItems)
        out.push_back(c.Key(inTable, &x, m.Where(x), inKey));
    return out;
}

long Ref(Map& m, EEDataText::Content& c, const char* inKey, const char* inTable, long inDefault)
{
    const Node* v = m.Get(inKey);
    return v ? c.Key(inTable, v, m.Where(*v), inKey) : inDefault;
}

std::string Id(Map& m)
{
    const Node* v = m.Get("id");
    const std::string& t = EEDataText::NodeScalar(v, m.Where(), "id");
    bool ok = !t.empty() && t[0] >= 'a' && t[0] <= 'z';
    for (char ch : t)
        ok = ok && ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_');
    if (!ok)
        Fail(m.Where() + ": id '" + t + "': an id is a lowercase letter, then a-z, 0-9 or _");
    return t;
}

// an option's default: the id of one of the list's entries
template <class T>
long DefaultOf(Map& m, const char* inKey, const std::vector<T>& inList, long inDefault)
{
    const Node* v = m.Get(inKey);
    if (!v)
        return inDefault;
    const std::string& t = EEDataText::NodeScalar(v, m.Where(*v), inKey);
    for (size_t i = 0; i < inList.size(); i++)
        if (inList[i].mID == t)
            return (long)i;
    Fail(m.Where(*v) + ": " + inKey + ": '" + t + "' is not an entry of the list");
}

template <class T>
void CheckUnique(const std::vector<T>& inList, const std::string& inWhere, const char* inWhat)
{
    for (size_t i = 0; i < inList.size(); i++)
        for (size_t j = 0; j < i; j++)
            if (inList[i].mID == inList[j].mID)
                Fail(inWhere + ": duplicate " + inWhat + " id '" + inList[i].mID + "'");
}

void Head(Map& m)
{
    m.Get("eedata");
    m.Get("format");
}

void LoadCombat(EEDataText::Content& c, const Node& d, EECombatRules& r)
{
    Map m(d, "combat", "the document");
    Head(m);
    if (const Node* s = m.Sub("damage")) {
        Map x(*s, "combat", "damage");
        x.Int("minimum", r.mMinimumDamage);
        x.Number("fanatic_factor", r.mFanaticFactor);
        x.Float("elevation_step", r.mElevationStep);
        x.Float("ally_factor", r.mAllyFactor);
        x.Float("herd_ally_factor", r.mHerdAllyFactor);
        x.Float("dead_food_share", r.mDeadFoodShare);
        x.Int("trample_damage", r.mTrampleDamage);
        x.Float("trample_threshold", r.mTrampleThreshold);
        x.Done();
    }
    if (const Node* s = m.Sub("elevation")) {
        Map x(*s, "combat", "elevation");
        r.mElevationAttackers = Categories(x, "attackers", r.mElevationAttackers);
        r.mElevationLaunchers = Categories(x, "launchers", r.mElevationLaunchers);
        r.mElevationTargets = Categories(x, "targets", r.mElevationTargets);
        x.Done();
    }
    if (const Node* s = m.Sub("morale")) {
        Map x(*s, "combat", "morale");
        x.Float("positive_factor", r.mPositiveMorale);
        x.Float("negative_factor", r.mNegativeMorale);
        r.mMoraleBuildings = Refs(x, c, "buildings", "dbobjects", r.mMoraleBuildings);
        x.Done();
    }
    if (const Node* s = m.Sub("ticks")) {
        Map x(*s, "combat", "ticks");
        x.Int("regeneration", r.mRegenerationInterval);
        x.Int("power_regeneration", r.mPowerRegenerationInterval);
        x.Done();
    }
    m.Done();
}

void LoadGameOptions(const Node& d, EEGameOptionRules& r)
{
    const char* doc = "gameoptions";
    Map m(d, doc, "the document");
    Head(m);
    if (const Node* v = m.List("map_sizes")) {
        r.mMapSizes.clear();
        for (const Node& e : v->mItems) {
            Map x(e, doc, "a map size");
            EEMapSizeRule s{Id(x), 0, 0, 0};
            x.Int("name", s.mNameID);
            x.Int("tiles", s.mTiles);
            x.Int("script_size", s.mScriptSize);
            if (s.mTiles < 1)
                Fail(x.Where() + ": tiles must be at least 1");
            if (s.mScriptSize < 0 || s.mScriptSize > 5)
                Fail(x.Where() + ": script_size is a random map script size class, 0 .. 5");
            x.Done();
            r.mMapSizes.push_back(s);
        }
        CheckUnique(r.mMapSizes, m.Where(*v), "map size");
        if (r.mMapSizes.empty())
            Fail(m.Where(*v) + ": map_sizes is empty");
    }
    r.mMapSizeDefault = DefaultOf(m, "map_size_default", r.mMapSizes, r.mMapSizeDefault);
    if ((size_t)r.mMapSizeDefault >= r.mMapSizes.size())
        Fail(m.Where() + ": map_size_default: name one of the map_sizes");
    if (const Node* v = m.List("speeds")) {
        r.mSpeeds.clear();
        for (const Node& e : v->mItems) {
            Map x(e, doc, "a speed");
            EESpeedRule s{Id(x), 0, EERuleNumber::Constant(1.0f)};
            x.Int("name", s.mNameID);
            x.Number("factor", s.mFactor);
            x.Done();
            r.mSpeeds.push_back(s);
        }
        CheckUnique(r.mSpeeds, m.Where(*v), "speed");
        if (r.mSpeeds.empty())
            Fail(m.Where(*v) + ": speeds is empty");
    }
    r.mSpeedDefault = DefaultOf(m, "speed_default", r.mSpeeds, r.mSpeedDefault);
    if ((size_t)r.mSpeedDefault >= r.mSpeeds.size())
        Fail(m.Where() + ": speed_default: name one of the speeds");
    if (const Node* v = m.List("difficulties")) {
        r.mDifficulties.clear();
        for (const Node& e : v->mItems) {
            Map x(e, doc, "a difficulty");
            EEDifficultyRule s{Id(x), 0, 0, EERuleNumber::Constant(1.0f), EERuleNumber::Constant(1.0f)};
            x.Int("name", s.mNameID);
            x.Int("ai_level", s.mAILevel);
            if (s.mAILevel < 0 || s.mAILevel > 2)
                Fail(x.Where() + ": ai_level is 0 (easy), 1 (medium) or 2 (hard): the computer players' records");
            x.Number("damage_scale_human_target", s.mScaleHumanTarget);
            x.Number("damage_scale_human_attacker", s.mScaleHumanAttacker);
            if (const Node* c = x.List("ai_cheats")) {
                s.mAICheats = 0;
                for (const Node& n : c->mItems) {
                    const std::string& t = EEDataText::NodeScalar(&n, x.Where(n), "ai_cheats");
                    long theBit = EEAICheatBit(t);
                    if (theBit == 0)
                        Fail(x.Where(n) + ": ai_cheats: '" + t + "' is not cost, reveal, epochs, extra, popband, catchup or retire");
                    s.mAICheats |= theBit;
                }
            }
            x.Done();
            r.mDifficulties.push_back(s);
        }
        CheckUnique(r.mDifficulties, m.Where(*v), "difficulty");
        if (r.mDifficulties.empty())
            Fail(m.Where(*v) + ": difficulties is empty");
    }
    if (const Node* s = m.Sub("unit_limit")) {
        Map x(*s, doc, "unit_limit");
        x.Number("minimum", r.mUnitLimitMinimum);
        x.Number("maximum", r.mUnitLimitMaximum);
        x.Int("step", r.mUnitLimitStep);
        if (r.mUnitLimitStep < 1)
            Fail(x.Where() + ": step must be at least 1");
        x.Number("default", r.mUnitLimitDefault);
        x.Done();
    }
    if (const Node* s = m.Sub("wonders_for_victory")) {
        Map x(*s, doc, "wonders_for_victory");
        x.Number("maximum", r.mWondersMaximum);
        x.Int("default", r.mWondersDefault);
        x.Done();
    }
    m.Bool("defeat_without_recovery", r.mDefeatWithoutRecovery);
    m.Done();
}

void LoadCivBuilder(EEDataText::Content& c, const Node& d, EECivBuilderRules& r)
{
    const char* doc = "civbuilder";
    Map m(d, doc, "the document");
    Head(m);
    if (const Node* v = m.List("attributes")) {
        r.mAttributes.clear();
        for (const Node& e : v->mItems) {
            Map x(e, doc, "an attribute");
            EECivAttribute a;
            a.mID = Id(x);
            a.mLevel = 0;
            x.Int("level", a.mLevel);
            if (a.mLevel < 1 || a.mLevel > 0xffff)
                Fail(x.Where() + ": level must be 1 .. 65535");
            a.mNameID = 0;
            x.Int("name", a.mNameID);
            for (const EECivAttribute& b : r.mAttributes)
                if (b.mID == a.mID || b.mLevel == a.mLevel)
                    Fail(x.Where() + ": attribute '" + a.mID + "': its id or level " + std::to_string(a.mLevel) +
                         " is also '" + b.mID + "'");
            if (const Node* l = x.List("applies")) {
                for (const Node& ap : l->mItems) {
                    Map y(ap, doc, "an apply");
                    EECivApply p;
                    const Node* stat = y.Get("stat");
                    p.mStat = Named(stat, y.Where(), "stat", kStatNames, kCivStatCount, "a stat");
                    if (const Node* mode = y.Get("mode"))
                        p.mMode = Named(mode, y.Where(*mode), "mode", kModeNames, kCivModeCount, "a mode");
                    if (p.mMode != kCivModeShare &&
                        (p.mStat == kCivStatHitPoints || p.mStat == kCivStatArmor || p.mStat == kCivStatPower))
                        Fail(y.Where() + ": " + kStatNames[p.mStat] + " has only the share mode");
                    y.Int("slot", p.mSlot);
                    if ((p.mStat == kCivStatGatherTime) != (p.mSlot >= 0))
                        Fail(y.Where() + ": slot belongs to gather_time (0 .. 6), and only there");
                    if (p.mSlot > 6)
                        Fail(y.Where() + ": slot is a gather time slot, 0 .. 6");
                    if (const Node* only = y.Get("only_category"))
                        p.mOnlyCategory = Category(only, y.Where(*only), "only_category");
                    if (const Node* except = y.Get("except_category"))
                        p.mExceptCategory = Category(except, y.Where(*except), "except_category");
                    p.mPower = Ref(y, c, "power", "dbcivpowers", -1);
                    if ((p.mStat == kCivStatPower) != (p.mPower >= 0))
                        Fail(y.Where() + ": power names the civ power of stat power, and only there");
                    y.Done();
                    a.mApplies.push_back(p);
                }
            }
            x.Done();
            r.mAttributes.push_back(a);
        }
    }
    if (const Node* s = m.Sub("powers_category")) {
        Map x(*s, doc, "powers_category");
        if (x.Get("id"))
            r.mPowersID = Id(x);
        x.Int("name", r.mPowersNameID);
        r.mPowersUnitSet = Ref(x, c, "unit_set", "dbunitset", r.mPowersUnitSet);
        x.Number("cost", r.mPowersCost);
        x.Float("level_value", r.mPowersLevelValue);
        x.Done();
    }
    m.Done();
}

// dbcivilization.yaml: each category's id and its levels, keyed by attribute (the table compiler puts the first 60
// into the record; the game takes them all from here)
void LoadCivCategories(EEDataText::Content& c, const Node& d, EECivBuilderRules& r)
{
    const Node* recs = d.Find("records");
    if (!recs || recs->mKind != Node::kList)
        return;
    r.mCategories.clear();
    long next = 0;
    const Node* rows = d.Find("rows");
    if (rows && !rows->IsMissing())
        next = EEDataText::NodeInt(rows, "dbcivilization", "rows");
    for (const Node& e : recs->mItems) {
        std::string where = "dbcivilization line " + std::to_string(e.mLine);
        if (e.mKind != Node::kMap)
            continue;
        EECivCategoryText t;
        const Node* row = e.Find("row");
        t.mRow = row && !row->IsMissing() ? EEDataText::NodeInt(row, where, "row") : next;
        next = (t.mRow + 1 > next ? t.mRow + 1 : next);
        const Node* id = e.Find("id");
        t.mID = id && id->mKind == Node::kScalar ? id->mText : std::string();
        if (const Node* levels = e.Find("levels")) {
            if (levels->mKind != Node::kList && !levels->IsMissing())
                Fail(where + ": levels must be a list");
            for (const Node& l : levels->mItems) {
                Map x(l, "dbcivilization", "a level");
                const Node* attribute = x.Get("attribute");
                const std::string& name = EEDataText::NodeScalar(attribute, x.Where(), "attribute");
                const EECivAttribute* a = r.FindID(name);
                if (!a)
                    Fail(x.Where() + ": attribute: '" + name + "' is not an attribute of civbuilder");
                float value = 0.0f, bonus = 0.0f;
                x.Float("value", value);
                x.Float("bonus", bonus);
                x.Done();
                if ((long)t.mValues.size() < a->mLevel) {
                    t.mValues.resize(a->mLevel, 0.0f);
                    t.mBonuses.resize(a->mLevel, 0.0f);
                }
                t.mValues[a->mLevel - 1] = value;
                t.mBonuses[a->mLevel - 1] = bonus;
            }
        }
        r.mCategories.push_back(t);
    }
    (void)c;
}

void LoadCivPowers(EEDataText::Content& c, const Node& d, EECivPowerRules& r)
{
    const char* doc = "civpowers";
    Map m(d, doc, "the document");
    Head(m);
    if (const Node* v = m.List("powers")) {
        r.mPowers.clear();
        for (const Node& e : v->mItems) {
            Map x(e, doc, "a power");
            EECivPowerRule p;
            p.mPower = Ref(x, c, "power", "dbcivpowers", -1);
            if (p.mPower < 0)
                Fail(x.Where() + ": power: missing");
            if (r.Find(p.mPower))
                Fail(x.Where() + ": a second entry for the same power");
            if (const Node* l = x.List("effects")) {
                for (const Node& ef : l->mItems) {
                    Map y(ef, doc, "an effect");
                    EECivPowerEffect f;
                    const Node* act = y.Get("do");
                    f.mAction = Named(act, y.Where(), "do", kActionNames, kPowerActionCount, "a power effect");
                    f.mTech = Ref(y, c, "tech", "dbtechtree", -1);
                    f.mIfTech = Ref(y, c, "if_researched", "dbtechtree", -1);
                    f.mSound = Ref(y, c, "sound", "dbsounds", -1);
                    f.mEffect = Ref(y, c, "effect", "dbgfxeffects", -1);
                    f.mUnitType = Ref(y, c, "unit_type", "dbobjects", -1);
                    bool needTech = f.mAction == kPowerResearchIfResearched || f.mAction == kPowerAllowTech ||
                                    f.mAction == kPowerDisallowTech;
                    if (needTech != (f.mTech >= 0) || (f.mAction == kPowerResearchIfResearched) != (f.mIfTech >= 0) ||
                        (f.mAction == kPowerLoadSound) != (f.mSound >= 0) ||
                        (f.mAction == kPowerLoadEffect) != (f.mEffect >= 0) ||
                        (f.mAction == kPowerPulseSight) != (f.mUnitType >= 0))
                        Fail(y.Where() + ": " + kActionNames[f.mAction] + " takes " +
                             (f.mAction == kPowerResearchIfResearched ? "if_researched and tech" :
                              needTech ? "tech" : f.mAction == kPowerLoadSound ? "sound" :
                              f.mAction == kPowerLoadEffect ? "effect" : f.mAction == kPowerPulseSight ?
                              "unit_type" : "nothing"));
                    y.Done();
                    p.mEffects.push_back(f);
                }
            }
            x.Done();
            r.mPowers.push_back(p);
        }
    }
    m.Done();
}

// epochs.yaml: each epoch's music and citizen (the rest of the entry is the tech tree's)
void LoadEpochs(EEDataText::Content& c, const Node& d, EERules& r)
{
    const Node* recs = d.Find("records");
    if (!recs || recs->mKind != Node::kList)
        return;
    std::vector<EEEpochMedia> out;
    for (size_t k = 0; k < recs->mItems.size(); k++) {
        const Node& e = recs->mItems[k];
        std::string where = "epochs line " + std::to_string(e.mLine);
        EEEpochMedia m = EEDefaultEpochMedia((long)k);
        if (e.mKind != Node::kMap)
            Fail(where + ": an epoch must be a mapping");
        if (const Node* music = e.Find("music")) {
            if (!music->IsMissing()) {
                if (music->mKind != Node::kList)
                    Fail(where + ": music must be a list");
                for (const Node& s : music->mItems)
                    m.mMusic.push_back(c.Key("dbmusic", &s, "epochs line " + std::to_string(s.mLine), "music"));
            }
        }
        if (const Node* cit = e.Find("citizen")) {
            if (!cit->IsMissing()) {
                Map x(*cit, "epochs", "citizen");
                x.Bool("high_tech_tools", m.mHighTechTools);
                m.mRemovalEffects = Refs(x, c, "removal_effects", "dbgfxeffects", m.mRemovalEffects);
                m.mRemovalSound = Ref(x, c, "removal_sound", "dbsounds", m.mRemovalSound);
                x.Float("removal_pitch_offset", m.mRemovalPitchOffset);
                m.mRemovalThunder = Ref(x, c, "removal_thunder", "dbsounds", m.mRemovalThunder);
                x.Int("removal_thunder_every", m.mRemovalThunderEvery);
                if (m.mRemovalThunderEvery < 0)
                    Fail(x.Where() + ": removal_thunder_every must be 0 (never) or more");
                x.Done();
            }
        }
        out.push_back(m);
    }
    r.mEpochs = out;
    r.mEpochMusicFromText = true;
}

void LoadAIPersonalities(const Node& d, EEAIPersonalityRules& r)
{
    const char* doc = "aipersonalities";
    Map m(d, doc, "the document");
    Head(m);
    if (const Node* v = m.List("personalities")) {
        r.mPersonalities.clear();
        for (const Node& e : v->mItems) {
            Map x(e, doc, "a personality");
            EEAIPersonalityRule p;
            p.mID = Id(x);
            if (const Node* s = x.Sub("epoch_pressure")) {
                Map y(*s, doc, "epoch_pressure");
                y.Float("start", p.mEpochPressureStart);
                y.Float("full", p.mEpochPressureFull);
                y.Done();
                if (!(p.mEpochPressureStart >= 0.0f && p.mEpochPressureFull > p.mEpochPressureStart))
                    Fail(y.Where() + ": epoch_pressure: 0 <= start < full");
            }
            if (const Node* s = x.Sub("economy")) {
                Map y(*s, doc, "economy");
                y.Int("surplus", p.mSurplus);
                y.Float("surplus_cut", p.mSurplusCut);
                y.Int("low_stock", p.mLowStock);
                if (const Node* f = y.Sub("floor")) {
                    Map z(*f, doc, "floor");
                    const char* kNames[5] = {"food", "wood", "stone", "gold", "iron"};
                    for (int i = 0; i < 5; i++)
                        z.Float(kNames[i], p.mFloor[i]);
                    z.Done();
                }
                y.Float("max_citizen_share", p.mMaxCitizenShare);
                y.Int("wood_slots", p.mWoodSlots);
                y.Int("moves_per_cycle", p.mMovesPerCycle);
                y.Bool("demand_split", p.mDemandSplit);
                y.Bool("coast_fishing", p.mCoastFishing);
                y.Done();
            }
            if (const Node* s = x.Sub("military")) {
                Map y(*s, doc, "military");
                y.Float("standing_army", p.mStandingArmy);
                y.Float("army_floor", p.mArmyFloor);
                y.Bool("militia", p.mMilitia);
                y.Bool("militia_towers", p.mMilitiaTowers);
                y.Int("prehistoric_army", p.mPrehistoricArmy);
                y.Done();
                if (!(p.mStandingArmy >= 0.0f))
                    Fail(y.Where() + ": military: standing_army must be 0 or more");
                if (p.mPrehistoricArmy < 0)
                    Fail(y.Where() + ": military: prehistoric_army must be 0 or more");
            }
            x.Done();
            r.mPersonalities.push_back(p);
        }
        CheckUnique(r.mPersonalities, m.Where(*v), "personality");
        if (r.mPersonalities.empty())
            Fail(m.Where(*v) + ": personalities is empty");
    }
    if (const Node* v = m.List("assign")) {
        r.mAssign.clear();
        for (const Node& e : v->mItems) {
            const std::string& t = EEDataText::NodeScalar(&e, m.Where(e), "assign");
            long k = r.Find(t);
            if (k < 0)
                Fail(m.Where(e) + ": assign: '" + t + "' is not a personality");
            r.mAssign.push_back(k);
        }
    }
    m.Done();
}

}  // namespace

bool EERules::Load(EEDataText::Content& inContent, std::string& outError)
{
    try {
        Node d;
        if (inContent.Document("combat", d))
            LoadCombat(inContent, d, this->mCombat);
        if (inContent.Document("gameoptions", d))
            LoadGameOptions(d, this->mGameOptions);
        if (inContent.Document("civbuilder", d))
            LoadCivBuilder(inContent, d, this->mCivBuilder);
        if (inContent.Document("dbcivilization", d))
            LoadCivCategories(inContent, d, this->mCivBuilder);
        if (inContent.Document("civpowers", d))
            LoadCivPowers(inContent, d, this->mCivPowers);
        if (inContent.Document("epochs", d))
            LoadEpochs(inContent, d, *this);
        if (inContent.Document("aipersonalities", d))
            LoadAIPersonalities(d, this->mAIPersonalities);
    } catch (Failure& e) {
        outError = e.mText;
        return false;
    } catch (std::exception& e) {
        outError = std::string("rules: ") + e.what();
        return false;
    }
    return true;
}

// ------------------------------------------------------------------------------------------------ the dump

namespace {

struct Json {
    std::string& mOut;
    bool mFirst = true;

    void Key(const char* inKey)
    {
        if (!this->mFirst)
            this->mOut += ',';
        this->mFirst = false;
        this->mOut += '"';
        this->mOut += inKey;
        this->mOut += "\":";
    }
    void Int(const char* inKey, long inValue)
    {
        this->Key(inKey);
        this->mOut += std::to_string(inValue);
    }
    void Float(const char* inKey, float inValue)   // as its bits, like the table dump
    {
        uint32_t bits;
        memcpy(&bits, &inValue, 4);
        this->Int(inKey, (long)bits);
    }
    void Str(const char* inKey, const std::string& inValue)
    {
        this->Key(inKey);
        this->mOut += '"' + inValue + '"';
    }
    void Number(const char* inKey, const EERuleNumber& n)
    {
        this->Key(inKey);
        static const char* const kValues[] = {"F", "F2", "L", "L2"};
        char buf[64];
        uint32_t bits;
        memcpy(&bits, &n.mFloat, 4);
        if (n.mWorldRow >= 0)
            snprintf(buf, sizeof(buf), "{\"world\":%ld,\"value\":\"%s\"}", n.mWorldRow, kValues[n.mWorldValue & 3]);
        else
            snprintf(buf, sizeof(buf), "{\"float\":%lu,\"long\":%ld}", (unsigned long)bits, n.mLong);
        this->mOut += buf;
    }
    void List(const char* inKey, const std::vector<long>& inValues)
    {
        this->Key(inKey);
        this->mOut += '[';
        for (size_t i = 0; i < inValues.size(); i++)
            this->mOut += (i ? "," : "") + std::to_string(inValues[i]);
        this->mOut += ']';
    }
    void Open(const char* inKey, char inBracket)
    {
        if (inKey)
            this->Key(inKey);
        else if (!this->mFirst)
            this->mOut += ',';
        this->mOut += inBracket;
        this->mFirst = true;
    }
    void Close(char inBracket)
    {
        this->mOut += inBracket;
        this->mFirst = false;
    }
};

}  // namespace

void EERules::Dump(std::string& outJson) const
{
    std::string s;
    {
        Json j{s};
        const EECombatRules& r = this->mCombat;
        s += "\"rules.combat\":{";
        j.Int("minimum_damage", r.mMinimumDamage);
        j.Number("fanatic_factor", r.mFanaticFactor);
        j.Float("elevation_step", r.mElevationStep);
        j.Float("ally_factor", r.mAllyFactor);
        j.Float("herd_ally_factor", r.mHerdAllyFactor);
        j.Float("dead_food_share", r.mDeadFoodShare);
        j.Int("trample_damage", r.mTrampleDamage);
        j.Float("trample_threshold", r.mTrampleThreshold);
        j.List("elevation_attackers", r.mElevationAttackers);
        j.List("elevation_launchers", r.mElevationLaunchers);
        j.List("elevation_targets", r.mElevationTargets);
        j.Float("positive_morale", r.mPositiveMorale);
        j.Float("negative_morale", r.mNegativeMorale);
        j.List("morale_buildings", r.mMoraleBuildings);
        j.Int("regeneration_interval", r.mRegenerationInterval);
        j.Int("power_regeneration_interval", r.mPowerRegenerationInterval);
        s += "}\n";
    }
    {
        Json j{s};
        const EEGameOptionRules& r = this->mGameOptions;
        s += "\"rules.gameoptions\":{";
        j.Open("map_sizes", '[');
        for (const EEMapSizeRule& m : r.mMapSizes) {
            j.Open(nullptr, '{');
            j.Str("id", m.mID);
            j.Int("name", m.mNameID);
            j.Int("tiles", m.mTiles);
            j.Int("script_size", m.mScriptSize);
            j.Close('}');
        }
        j.Close(']');
        j.Int("map_size_default", r.mMapSizeDefault);
        j.Open("speeds", '[');
        for (const EESpeedRule& m : r.mSpeeds) {
            j.Open(nullptr, '{');
            j.Str("id", m.mID);
            j.Int("name", m.mNameID);
            j.Number("factor", m.mFactor);
            j.Close('}');
        }
        j.Close(']');
        j.Int("speed_default", r.mSpeedDefault);
        j.Open("difficulties", '[');
        for (const EEDifficultyRule& m : r.mDifficulties) {
            j.Open(nullptr, '{');
            j.Str("id", m.mID);
            j.Int("name", m.mNameID);
            j.Int("ai_level", m.mAILevel);
            j.Number("damage_scale_human_target", m.mScaleHumanTarget);
            j.Number("damage_scale_human_attacker", m.mScaleHumanAttacker);
            j.Int("ai_cheats", m.mAICheats);
            j.Close('}');
        }
        j.Close(']');
        j.Number("unit_limit_minimum", r.mUnitLimitMinimum);
        j.Number("unit_limit_maximum", r.mUnitLimitMaximum);
        j.Int("unit_limit_step", r.mUnitLimitStep);
        j.Number("unit_limit_default", r.mUnitLimitDefault);
        j.Number("wonders_maximum", r.mWondersMaximum);
        j.Int("wonders_default", r.mWondersDefault);
        j.Int("defeat_without_recovery", r.mDefeatWithoutRecovery ? 1 : 0);
        s += "}\n";
    }
    {
        Json j{s};
        const EECivBuilderRules& r = this->mCivBuilder;
        s += "\"rules.civbuilder\":{";
        j.Open("attributes", '[');
        for (const EECivAttribute& a : r.mAttributes) {
            j.Open(nullptr, '{');
            j.Str("id", a.mID);
            j.Int("level", a.mLevel);
            j.Int("name", a.mNameID);
            j.Open("applies", '[');
            for (const EECivApply& p : a.mApplies) {
                j.Open(nullptr, '{');
                j.Int("stat", p.mStat);
                j.Int("mode", p.mMode);
                j.Int("slot", p.mSlot);
                j.Int("only_category", p.mOnlyCategory);
                j.Int("except_category", p.mExceptCategory);
                j.Int("power", p.mPower);
                j.Close('}');
            }
            j.Close(']');
            j.Close('}');
        }
        j.Close(']');
        j.Str("powers_id", r.mPowersID);
        j.Int("powers_name", r.mPowersNameID);
        j.Int("powers_unit_set", r.mPowersUnitSet);
        j.Number("powers_cost", r.mPowersCost);
        j.Float("powers_level_value", r.mPowersLevelValue);
        // the levels past the record's 60 (only dbcivilization.yaml can have them; the rest are in its dump)
        j.Open("levels_past_60", '[');
        for (const EECivCategoryText& c : r.mCategories) {
            for (size_t k = 60; k < c.mValues.size(); k++) {
                if (c.mValues[k] == 0.0f && c.mBonuses[k] == 0.0f)
                    continue;
                j.Open(nullptr, '{');
                j.Int("row", c.mRow);
                j.Int("level", (long)k + 1);
                j.Float("value", c.mValues[k]);
                j.Float("bonus", c.mBonuses[k]);
                j.Close('}');
            }
        }
        j.Close(']');
        s += "}\n";
    }
    {
        Json j{s};
        s += "\"rules.civpowers\":{";
        j.Open("powers", '[');
        for (const EECivPowerRule& p : this->mCivPowers.mPowers) {
            j.Open(nullptr, '{');
            j.Int("power", p.mPower);
            j.Open("effects", '[');
            for (const EECivPowerEffect& f : p.mEffects) {
                j.Open(nullptr, '{');
                j.Int("do", f.mAction);
                j.Int("tech", f.mTech);
                j.Int("if_researched", f.mIfTech);
                j.Int("sound", f.mSound);
                j.Int("effect", f.mEffect);
                j.Int("unit_type", f.mUnitType);
                j.Close('}');
            }
            j.Close(']');
            j.Close('}');
        }
        j.Close(']');
        s += "}\n";
    }
    {
        Json j{s};
        s += "\"rules.epochs\":[";
        for (const EEEpochMedia& m : this->mEpochs) {
            j.Open(nullptr, '{');
            j.List("music", m.mMusic);
            j.Int("high_tech_tools", m.mHighTechTools ? 1 : 0);
            j.List("removal_effects", m.mRemovalEffects);
            j.Int("removal_sound", m.mRemovalSound);
            j.Float("removal_pitch_offset", m.mRemovalPitchOffset);
            j.Int("removal_thunder", m.mRemovalThunder);
            j.Int("removal_thunder_every", m.mRemovalThunderEvery);
            j.Close('}');
        }
        s += "]\n";
    }
    {
        Json j{s};
        s += "\"rules.aipersonalities\":{";
        j.Open("personalities", '[');
        for (const EEAIPersonalityRule& p : this->mAIPersonalities.mPersonalities) {
            j.Open(nullptr, '{');
            j.Str("id", p.mID);
            j.Float("epoch_pressure_start", p.mEpochPressureStart);
            j.Float("epoch_pressure_full", p.mEpochPressureFull);
            j.Int("surplus", p.mSurplus);
            j.Float("surplus_cut", p.mSurplusCut);
            j.Int("low_stock", p.mLowStock);
            for (int i = 0; i < 5; i++)
                j.Float(("floor" + std::to_string(i)).c_str(), p.mFloor[i]);
            j.Float("max_citizen_share", p.mMaxCitizenShare);
            j.Int("wood_slots", p.mWoodSlots);
            j.Int("moves_per_cycle", p.mMovesPerCycle);
            j.Int("demand_split", p.mDemandSplit ? 1 : 0);
            j.Int("coast_fishing", p.mCoastFishing ? 1 : 0);
            j.Float("standing_army", p.mStandingArmy);
            j.Float("army_floor", p.mArmyFloor);
            j.Int("militia", p.mMilitia ? 1 : 0);
            j.Int("militia_towers", p.mMilitiaTowers ? 1 : 0);
            j.Int("prehistoric_army", p.mPrehistoricArmy);
            j.Close('}');
        }
        j.Close(']');
        j.List("assign", this->mAIPersonalities.mAssign);
        s += "}\n";
    }
    outJson += s;
}
