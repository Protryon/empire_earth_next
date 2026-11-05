// ee/EERules.h (new, not in the original game): the game rules that were literals in the code, as data
// (docs/DATA_FORMATS.md 8.8). Four rules documents next to the db tables, db\combat.yaml, db\gameoptions.yaml,
// db\civbuilder.yaml and db\civpowers.yaml, plus each epoch's media in db\epochs.yaml (music, citizen). Every member
// starts at the original game's value; a document on the data path sets every value it holds (references by id,
// resolved by the db tables' Content). Without the documents (the binary db tree) the game plays with these
// defaults, which equal the vanilla documents tools/eedata writes (eedata.py cppgate: the content dumps match).
// Portable C++17 (no engine headers): src/Empire Earth/Database/EERules.cpp, also built into the converter's gate.
#ifndef EE_EERULES_H
#define EE_EERULES_H

#include <string>
#include <vector>

namespace EEDataText {
class Content;
struct Node;
}

// A number that is a constant or one of dbworld's values: `fanaticism.F2` (the constant's id, then F, F2, L or L2:
// mFloatValue, mFloatValue2, mLongValue, mLongValue2). The game reads a world value through gWorldTable when it uses
// the number (EERulesWorld.h), so it follows the loaded dbworld.
struct EERuleNumber {
    float mFloat = 0.0f;                    // a constant (mWorldRow < 0): its value
    long mLong = 0;                         // ... as an integer (a float constant: truncated)
    long mWorldRow = -1;                    // a world value: the dbworld row
    int mWorldValue = 0;                    // 0 F, 1 F2, 2 L, 3 L2

    static EERuleNumber Constant(float inValue);
    static EERuleNumber Integer(long inValue);
    static EERuleNumber World(long inRow, int inValue);
};

// ------------------------------------------------------------------------------------------------ combat.yaml

// How a hit's damage is formed (EEUnitClass::ComputeDamage)
enum EEDamageForm {
    kDamageDirect,                          // a unit's or projectile's hit (EEUnitClass::ApplyHit)
    kDamageSplash,                          // splash damage around it (EEUnitClass::ApplyAreaDamage)
    kDamageTrample                          // a moving unit pushing a stopped enemy (EEPhysicsModel)
};

struct EECombatRules {
    // damage (one function for the three forms)
    long mMinimumDamage = 1;                // the least damage of a direct or splash hit that gets through
    EERuleNumber mFanaticFactor = EERuleNumber::World(88, 1);   // attack factor of a fanatic attacker (direct hits)
    float mElevationStep = 1.0f;            // the elevation factor moves damage by at least this much
    float mAllyFactor = 0.0f;               // a direct hit on an own or allied unit: ceil(damage x this) ...
    float mHerdAllyFactor = 0.1f;           // ... by a herd animal: ceil(damage x this)
    float mDeadFoodShare = 0.25f;           // a land food source a non-citizen kills keeps this share of its food
    long mTrampleDamage = 1;                // the trample's damage, when hit x (attack - armour) > mTrampleThreshold
    float mTrampleThreshold = 1.0f;
    // elevation (EEUnitClass::GetElevationFactor): unit categories
    std::vector<long> mElevationAttackers;  // attackers the height difference scales
    std::vector<long> mElevationLaunchers;  // projectiles that count as their launcher's category
    std::vector<long> mElevationTargets;    // targets it applies to
    // morale (EEComplexUnit::ApplyMorale)
    float mPositiveMorale = 0.8f;           // positive morale takes this share off a time
    float mNegativeMorale = 0.6f;           // negative morale adds this share
    std::vector<long> mMoraleBuildings;     // the unit types morale from buildings counts (dbobjects rows)
    // ticks, ms
    long mRegenerationInterval = 1000;      // hit point regeneration and garrison heals
    long mPowerRegenerationInterval = 5000; // power regeneration

    EECombatRules();
    bool IsElevationAttacker(long inCategory) const;
    bool IsElevationLauncher(long inCategory) const;
    bool IsElevationTarget(long inCategory) const;
    bool IsMoraleBuilding(long inUnitType) const;
};

// ------------------------------------------------------------------------------------------------ gameoptions.yaml

struct EEMapSizeRule {
    std::string mID;
    long mNameID;                           // the lobby's text (Language.rc string)
    long mTiles;                            // width and height in tiles
    long mScriptSize;                       // the random map scripts' size class (0 tiny .. 5 gigantic)
};

struct EESpeedRule {
    std::string mID;
    long mNameID;
    EERuleNumber mFactor;                   // the game speed
};

struct EEDifficultyRule {
    std::string mID;
    long mNameID;
    long mAILevel;                          // the computer players' level: 0 easy, 1 medium, 2 hard (their records)
    // the damage table scale when exactly one side is "human" (EEPlayer::IsHuman, not player 0; provisional name):
    EERuleNumber mScaleHumanTarget;         // ... the target
    EERuleNumber mScaleHumanAttacker;       // ... the attacker
};

struct EEGameOptionRules {
    std::vector<EEMapSizeRule> mMapSizes;   // the option is the position
    long mMapSizeDefault = 1;
    std::vector<EESpeedRule> mSpeeds;
    long mSpeedDefault = 1;
    std::vector<EEDifficultyRule> mDifficulties;
    EERuleNumber mUnitLimitMinimum = EERuleNumber::World(26, 2);
    EERuleNumber mUnitLimitMaximum = EERuleNumber::World(26, 3);
    long mUnitLimitStep = 50;
    EERuleNumber mUnitLimitDefault = EERuleNumber::World(27, 2);
    EERuleNumber mWondersMaximum = EERuleNumber::World(77, 2);
    long mWondersDefault = 3;

    EEGameOptionRules();
};

// ------------------------------------------------------------------------------------------------ civbuilder.yaml

// What a civilization builder attribute raises (EECivilizationManager::ApplyUpgradeLevel). The stat and the mode are
// C++ primitives; the data picks and combines them.
enum EECivStat {
    kCivStatAttack, kCivStatHitPoints, kCivStatReloadTime, kCivStatArmor, kCivStatRange, kCivStatLineOfSight,
    kCivStatSpeed, kCivStatFlightTime, kCivStatAreaEffect, kCivStatGatherTime, kCivStatBuildTime,
    kCivStatConversion, kCivStatRepairRate, kCivStatAreaEffectScale, kCivStatHighGround, kCivStatPopulation,
    kCivStatCost, kCivStatPower, kCivStatCount
};
enum EECivMode {
    kCivModeShare,                          // the stat's own rule: a share of the class's base value (the original)
    kCivModeAdd,                            // + the value
    kCivModeMultiply,                       // x (1 + the value)
    kCivModeCount
};
struct EECivApply {
    long mStat;                             // EECivStat
    long mMode = kCivModeShare;             // EECivMode
    long mSlot = -1;                        // gather_time: the gather time slot
    long mOnlyCategory = -1;                // apply only to classes of this unit category ...
    long mExceptCategory = -1;              // ... or to every class but this one
    long mPower = -1;                       // power: the civ power (dbcivpowers row)
};
struct EECivAttribute {
    std::string mID;
    long mLevel;                            // its level in every category (dbcivilization mLevels[level - 1]; the
                                            // upgrade id's low bits)
    std::vector<EECivApply> mApplies;
};
struct EECivBuilderRules {
    std::vector<EECivAttribute> mAttributes;
    // the special powers category EECivilizationManager::Load appends (one level per civ power)
    long mPowersNameID = 32850;             // IDS_SPECIAL_POWERS
    long mPowersUnitSet = 0x66;             // dbunitset row
    EERuleNumber mPowersCost = EERuleNumber::World(86, 2);
    float mPowersLevelValue = 0.01f;

    EECivBuilderRules();
    const EECivAttribute* Find(long inLevel) const;
};

// ------------------------------------------------------------------------------------------------ civpowers.yaml

// What granting a civ power does after researching its technology (EECivPowersManager::ApplyPower). C++ primitives.
enum EECivPowerAction {
    kPowerResearchIfResearched,             // research mTech when mIfTech is researched (flaming/crusader upgrades)
    kPowerLoadSound,                        // load mSound's media for the player
    kPowerLoadEffect,                       // load mEffect's media for the player
    kPowerPulseSight,                       // a sight pulse of the power's mAmount around every mUnitType
    kPowerSetMiningLimit,                   // the player's gatherers per resource: the power's mMiningLimit
    kPowerEnableCamouflage,                 // EEPlayer::EnableCamouflagePower
    kPowerAllowTech,                        // mTech becomes allowed
    kPowerDisallowTech,                     // mTech becomes disallowed and obsolete
    kPowerActionCount
};
struct EECivPowerEffect {
    long mAction;                           // EECivPowerAction
    long mTech = -1, mIfTech = -1;          // dbtechtree ids
    long mSound = -1;                       // dbsounds row
    long mEffect = -1;                      // dbgfxeffects row
    long mUnitType = -1;                    // dbobjects row
};
struct EECivPowerRule {
    long mPower;                            // dbcivpowers row
    std::vector<EECivPowerEffect> mEffects;
};
struct EECivPowerRules {
    std::vector<EECivPowerRule> mPowers;
    EECivPowerRules();
    const EECivPowerRule* Find(long inRow) const;
};

// ------------------------------------------------------------------------------------------------ epochs.yaml media

struct EEEpochMedia {
    // music: the songs of the epoch (dbmusic rows). Without epochs.yaml the songs come from dbmusic's per-epoch
    // flags (EEMusicObject::LoadTable fills this; the space age plays the nano age's songs, as the original did)
    std::vector<long> mMusic;
    // the citizen of the epoch
    bool mHighTechTools = false;            // beams: the mining beam, the shot effect (EEUGGather, EEUnitClass)
    // a unit removed by a research (EEPlayUnitEffect): effects and a sound at its place, a thunder every n-th unit
    std::vector<long> mRemovalEffects;      // dbgfxeffects rows
    long mRemovalSound = 0x176;             // dbsounds row (Lightning)
    float mRemovalPitchOffset = 0.0f;       // added to the sound's pitch (not below -1)
    long mRemovalThunder = 0x1ba;           // dbsounds row (Thunder 2)
    long mRemovalThunderEvery = 0;          // a thunder when the unit id is a multiple of this (0: never)
};

// ------------------------------------------------------------------------------------------------ all of them

struct EERules {
    EECombatRules mCombat;
    EEGameOptionRules mGameOptions;
    EECivBuilderRules mCivBuilder;
    EECivPowerRules mCivPowers;
    std::vector<EEEpochMedia> mEpochs;      // by epoch position (the original's 15 by default)
    bool mEpochMusicFromText = false;       // the songs came from epochs.yaml (else dbmusic's flags)

    EERules();
    // reads the rules documents and epochs.yaml's media from the Content (references by id); a document the Content
    // has no text for keeps its defaults. False with a message ("combat line 12: ...") on the first problem.
    bool Load(EEDataText::Content& inContent, std::string& outError);
    // the songs from dbmusic's per-epoch flags (the binary load): a song row's flag k -> epoch k (k < 14), the 15th
    // epoch plays the 14th's songs
    void SetMusicFromFlags(long inSong, const bool* inFlags, unsigned long inCount);
    // the content dump of the rules (docs/MODDING_PLAN.md gate G1): `"rules.<doc>":{...}` lines, values resolved
    void Dump(std::string& outJson) const;
    // the media of an epoch position (past the list: the last epoch's)
    const EEEpochMedia& Epoch(long inEpoch) const;
};

// The default epoch media of an epoch position (the original's per-epoch rules)
EEEpochMedia EEDefaultEpochMedia(long inEpoch);

// The game's rules (constructed on first use; filled by EELoadRules at database load, EEDataTextFile.cpp)
EERules& EEGetRules();

// the computer players' level (0 easy .. 2 hard: their records' values) of a game option difficulty
long EEDifficultyAILevel(unsigned long inDifficulty);

// unit category names (the data's category ids): kUnitCategory* by value
extern const char* const kEERuleCategoryNames[];
extern const unsigned long kEERuleCategoryCount;

#endif
