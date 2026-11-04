// C:\EEX\Empire Earth\Database\EECPBehavior.h (RECONSTRUCTION; file name inferred; provisional class and member
// names). The computer player behaviour table of db\dbcpbehavior.dat: a file-scope array of 129 records of
// 0x38 bytes at 0x4e6118, indexed by the record's own index (EECPBehaviorTable.cpp 0xd5f7e..0xd6035; the AI reads
// it at 0x18c53d..). The struct's inline ctor clears the byte at +0x34; no dtor (the array's chain has no atexit).
#ifndef EE_EECPBEHAVIOR_H
#define EE_EECPBEHAVIOR_H

class FSFileSpec;

struct EECPBehavior {
    EECPBehavior() { this->mIsSettable = false; }

    static long Load(FSFileSpec* inFile);   // 0xd5f97

    long mL00;                              // +0x00
    long mIndex;                            // +0x04 (the slot; records with index <= 0 are skipped)
    // +0x08..+0x2c: the "set AI attribute" editor (EESETSETSetAIAttribute::ShowAttributeRanges 0x18d020)
    // reads three slider ranges (+0x08/+0x0c, +0x14/+0x18, +0x24/+0x28) and a scale (+0x2c, 0 -> 1; the third
    // slider's value is the float effect value times it); +0x10/+0x1c/+0x20 unread there. Names by offset.
    long mIntValue1Min;                     // +0x08
    long mIntValue1Max;                     // +0x0c
    long mIntValue1;                        // +0x10
    long mIntValue2Min;                     // +0x14
    long mIntValue2Max;                     // +0x18
    long mIntValue2;                        // +0x1c
    long mL20;                              // +0x20
    long mFloatSliderMin;                   // +0x24
    long mFloatSliderMax;                   // +0x28
    long mFloatSliderScale;                 // +0x2c
    float mFloatValue;                      // +0x30 (squared after loading, for record 48 only)
    bool mIsSettable;                       // +0x34
};

static_assert(sizeof(EECPBehavior) == 56, "on-disk record: 56 bytes on every target");

extern EECPBehavior gCPBehaviors[129];      // 0x4e6118

// Computer player behaviour record ids (db\dbcpbehavior.dat; the gCPBehaviors index = EECPBehavior::mIndex). Names: Language.rc 28000 + id (EESETSETSetAIAttribute::Show 0x58c96c), else the Init member the record fills.
enum EECPBehaviorID {
    kCPBehaviorNone = 0,                                    // Language.rc 28000 "None" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorInitialAttackDelay = 1,                      // Language.rc 28001 "Initial Attack Delay" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorAttackDelayModifier = 2,                     // Language.rc 28002 "Attack Delay Modifier" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorCitizenPercentage = 3,                       // Language.rc 28003 "Citizen Percentage" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorTroopsToAssistAllies = 4,                    // Language.rc 28004 "Troops to Assist Allies" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorShipsToMaintain = 5,                         // Language.rc 28005 "Ships to Maintain" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorAttackFrequency = 6,                         // Language.rc 28006 "Attack Frequency" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorAttackFrequencyModifier = 7,                 // Language.rc 28007 "Attack Frequency Modifier" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorNumberOfLandExplorers = 8,                   // Language.rc 28008 "Number of Land Explorers" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorNumberOfSeaExplorers = 9,                    // Language.rc 28009 "Number of Sea Explorers" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorNumberOfAirExplorers = 10,                   // Language.rc 28010 "Number of Air Explorers" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorAttackGroupModifier = 11,                    // Language.rc 28011 "Attack Group Modifier" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorCountWeights = 12,                           // no string 28012; EEComputerPlayer::Init 0x42f1c8 mMilitaryCountWeight = value1*0.01, mWorkerCountWeight = value2*0.01 (EECPIntelligencePlayerFile 0x480f9b count weights) [merged: cp named it keecpbMilitaryWorkerWeights] (provisional name)
    kCPBehaviorAttackGroupSize = 13,                        // no string 28013; EEComputerPlayer::Init 0x42f1c8 mAttackGroupSizeMin/Max = value1/value2 (EECPUnitSelector 0x49ad60 attack group share bounds) (provisional name)
    kCPBehaviorResourceDropOffDistance = 14,                // Language.rc 28014 "Resource Drop-off Distance" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorResearchFrequency = 15,                      // Language.rc 28015 "Research Frequency" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorInitialAttackDelayPerEpoch = 18,             // no string 28018; EEComputerPlayer::Init 0x42f1c8 mInitialAttackDelayPerEpoch = value1 (Fn3D4CD 0x43d4cd minutes per epoch span) (provisional name)
    kCPBehaviorAttackFrequencyBoundaries = 19,              // Language.rc 28019 "Attack Frequency Boundaries" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDefensiveUnitCap = 20,                       // Language.rc 28020 "Defensive Unit Cap" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorAttackFrequencyBoundary = 21,                // Language.rc 28021 "Attack Frequency Boundary" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorAttackFrequencyPerEpoch = 22,                // no string 28022; EEComputerPlayer::Init 0x42f1c8 mAttackFrequencyPerEpoch = float value (Fn3D4CD 0x43d4cd) (provisional name)
    kCPBehaviorResearchFrequencyModifier = 23,              // no string 28023 (help 28523 on research timing); EEComputerPlayer::Init 0x42f1c8 mResearchFrequencyModifierEasy/Hard = value1/value2 (provisional name)
    kCPBehaviorAttackWonder = 24,                           // Language.rc 28024 "Attack Wonder" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorAttackEnemy = 25,                            // Language.rc 28025 "Attack Enemy" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorAttackToClaimResource = 26,                  // Language.rc 28026 "Attack to Claim Resource" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorAttackForRevenge = 27,                       // Language.rc 28027 "Attack for Revenge" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorEpochFrequency = 28,                         // Language.rc 28028 "Epoch Frequency" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorEpochFrequencyModifier = 29,                 // Language.rc 28029 "Epoch Frequency Modifier" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDesiredNumberOfBarracks = 30,                // Language.rc 28030 "Desired Number of Barracks" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDesiredNumberOfArcheryRanges = 31,           // Language.rc 28031 "Desired Number of Archery Ranges" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDesiredNumberOfStables = 32,                 // Language.rc 28032 "Desired Number of Stables" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDesiredNumberOfDocks = 33,                   // Language.rc 28033 "Desired Number of Docks" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDesiredNumberOfSiegeFactories = 34,          // Language.rc 28034 "Desired Number of Siege Factories" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDesiredNumberOfNavalYards = 35,              // Language.rc 28035 "Desired Number of Naval Yards" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDesiredNumberOfAirports = 36,                // Language.rc 28036 "Desired Number of Airports" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDesiredNumberOfCyberFactories = 37,          // Language.rc 28037 "Desired Number of Cyber Factories" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDesiredNumberOfUniversities = 38,            // Language.rc 28038 "Desired Number of Universities" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDesiredNumberOfTemples = 39,                 // Language.rc 28039 "Desired Number of Temples" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDesiredNumberOfHospitals = 40,               // Language.rc 28040 "Desired Number of Hospitals" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDesiredNumberOfGranaries = 41,               // Language.rc 28041 "Desired Number of Granaries" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDesiredNumberOfCyberLaboratories = 42,       // Language.rc 28042 "Desired Number of Cyber Laboratories" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorNumberOfMilitaryUnitsToTrain = 43,           // Language.rc 28043 "Number of Military Units to Train" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorShipsToMaintainDeathmatch = 44,              // Language.rc 28044 "Ships to Maintain (Deathmatch)" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorSpecializedUnitPercentage = 45,              // Language.rc 28045 "Specialized Unit Percentage" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorUnitSelectedForImprovements = 46,            // Language.rc 28046 "Unit Selected for Improvements" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorUnitAttributesSelectedForImprovements = 47,  // Language.rc 28047 "Unit Attributes Selected for Improvements" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorMaximumAttackResponseRange = 48,             // Language.rc 28048 "Maximum Attack Response Range" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorPriestUsage = 49,                            // Language.rc 28049 "Priest Usage" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorProphetUsage = 50,                           // Language.rc 28050 "Prophet Usage" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorHeroUsage = 51,                              // Language.rc 28051 "Hero Usage" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorCarrierUsage = 52,                           // Language.rc 28052 "Carrier Usage" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorRPSSelectionNonIslands = 53,                 // Language.rc 28053 "RPS Selection Non-Islands" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorSpecializedUnitSelectionChance = 54,         // Language.rc 28054 "Specialized Unit Selection Chance" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorEnableStockCivilization = 55,                // setrilo enableStockCivilization_ (record 55); EEComputerPlayer::Init 0x42f1c8 mEnableStockCivilization; CanEnableManager 0x43eeca kind 0
    kCPBehaviorEnableStockResource = 56,                    // setrilo enableStockResource_ (record 56); EEComputerPlayer::Init 0x42f1c8 mEnableStockResource; CanEnableManager kind 1
    kCPBehaviorEnableStockResearch = 57,                    // setrilo enableStockResearch_ (record 57); EEComputerPlayer::Init 0x42f1c8 mEnableStockResearch; CanEnableManager kind 2
    kCPBehaviorEnableStockProduction = 58,                  // setrilo enableStockProduction_ (record 58); EEComputerPlayer::Init 0x42f1c8 mEnableStockProduction; CanEnableManager kind 3
    kCPBehaviorEnableStockBuild = 59,                       // setrilo enableStockBuild_ (record 59); EEComputerPlayer::Init 0x42f1c8 mEnableStockBuild; CanEnableManager kinds 4/9
    kCPBehaviorEnableStockDefense = 61,                     // setrilo enableStockDefense_ (record 61); EEComputerPlayer::Init 0x42f1c8 mEnableStockDefense; CanEnableManager kind 6
    kCPBehaviorEnableStockExploration = 62,                 // setrilo enableStockExploration_ (record 62); EEComputerPlayer::Init 0x42f1c8 mEnableStockExploration; CanEnableManager kind 7
    kCPBehaviorEnableStockCombat = 63,                      // setrilo enableStockCombat_ (record 63); EEComputerPlayer::Init 0x42f1c8 mEnableStockCombat; CanEnableManager kind 8
    kCPBehaviorManagerEpochAdvanceWait = 64,                // no string 28064; EEComputerPlayer::Init 0x42f1c8 mManagerEpochAdvanceWait (EnableManagers 0x42d334: seconds before the next epoch advance) (provisional name)
    kCPBehaviorManagerAttackWait = 65,                      // no string 28065; EEComputerPlayer::Init 0x42f1c8 mManagerAttackWait (EnableManagers 0x42d334: seconds before the next attack) (provisional name)
    kCPBehaviorMaxEnabledManagers = 66,                     // no string 28066; EEComputerPlayer::Init 0x42f1c8 mMaxEnabledManagers (EnableManagers 0x42d334 enables at most it + 1) (provisional name)
    kCPBehaviorUnitCostReductionMedium = 67,                // Language.rc 28067 "Unit Cost Reduction - Medium" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorEnableInterval = 68,                         // setrilo enableInterval_ (+C5C); EEComputerPlayer::Init 0x42f1c8 mEnableInterval (Fn3D492 0x43d492 next manager-enable pass)
    kCPBehaviorAidThreatThreshold = 69,                     // no string 28069; EEComputerPlayer::Init 0x42f1c8 mAidThreatThreshold (EECPMilitaryAidPackage 0x485892 threat limit) (provisional name)
    kCPBehaviorGuardChokepointPercentage = 70,              // Language.rc 28070 "Guard Chokepoint Percentage" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorGuardContinentPercentage = 71,               // Language.rc 28071 "Guard Continent Percentage" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorGuardHighGroundPercentage = 72,              // Language.rc 28072 "Guard High Ground Percentage" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorGuardTownLandPercentage = 73,                // no string 28073; EEComputerPlayer::Init 0x42f1c8 mGuardTownLandPercentage (EECPASGLand::VirtualB4 0x44760b; beside 28078 "Guard Town Percentage") (provisional name)
    kCPBehaviorGuardChokepointAirPercentage = 77,           // Language.rc 28077 "Guard Chokepoint-Air Percentage" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorGuardTownPercentage = 78,                    // Language.rc 28078 "Guard Town Percentage" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorGuardWaterwayPercentage = 82,                // Language.rc 28082 "Guard Waterway Percentage" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorNumChokePointsToGuard = 86,                  // no string 28086; EEComputerPlayer::Init 0x42f1c8 mNumChokePointsToGuard (EECPDefenseCommander keeps at most this many choke points) (provisional name)
    kCPBehaviorDefenseGroupSizeModifierPercentage = 87,     // Language.rc 28087 "Defense Group Size Modifier Percentage" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDefensiveUnitUpdate = 88,                    // Language.rc 28088 "Defensive Unit Update" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorInitialDelayInReplenishmentDefenseUnits = 89,  // Language.rc 28089 "Initial Delay in Replenishment (Defense Units)" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorTownGuardPercentage = 90,                    // Language.rc 28090 "Town Guard Percentage" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorComputerVsComputerAggression = 91,           // Language.rc 28091 "Computer vs Computer Aggression" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorPopulationCapOtherWeight = 92,               // no string 28092; EEComputerPlayer::WorkOutPopulation: weight of the largest other population in the population cap average (provisional name)
    kCPBehaviorPopulationCapOwnWeight = 93,                 // no string 28093; EEComputerPlayer::WorkOutPopulation: weight of the own population in the population cap average (provisional name)
    kCPBehaviorEpochAdvanceTimeScale = 94,                  // no string 28094; EECPCivilizationManager::SetNextEpochAdvanceTime 0x470b04 scales the epoch advance time by its float value (provisional name)
    kCPBehaviorDesiredNumberOfTowers = 95,                  // Language.rc 28095 "Desired Number of Towers" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorUnitCostReductionEasyAndHard = 96,           // Language.rc 28096 "Unit Cost Reduction - Easy & Hard" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorExpansionTimer = 97,                         // Language.rc 28097 "Expansion Timer" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorPopulationLimitAdjustment = 98,              // Language.rc 28098 "Population Limit Adjustment" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorTownCenterUpgradeTimer = 99,                 // Language.rc 28099 "Town Center Upgrade Timer" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorCapitolUpgradeTimer = 100,                   // Language.rc 28100 "Capitol Upgrade Timer" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorComputerPlayerExtraResources = 101,          // Language.rc 28101 "Computer Player Extra Resources" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorEpochCatchUp = 102,                          // setrilo epochCatchUpMin_/Max_ (record 102); EEComputerPlayer::Init 0x42f1c8 mEpochCatchUpMin/Max (Fn2F064 0x42f064: minutes when another player is in a later epoch)
    kCPBehaviorEasyMinNumberMilitaryBuildings = 103,        // Language.rc 28103 "(Easy) Min Number Military Buildings" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i) [merged: cp named it keecpbMinNumberMilitaryBuildingsEasy]
    kCPBehaviorHardMinNumberMilitaryBuildings = 104,        // Language.rc 28104 "(Hard) Min Number Military Buildings" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i) [merged: cp named it keecpbMinNumberMilitaryBuildingsHard]
    kCPBehaviorDMEarlyAttack = 105,                         // no string 28105; EEComputerPlayer::Init 0x42f1c8 mDMEarlyAttackScale = value1*0.01, mDMEarlyAttackTime = value2 minutes (EECPUnitSelector 0x49ad60, Deathmatch) (provisional name)
    kCPBehaviorMediumMinNumberMilitaryBuildings = 106,      // Language.rc 28106 "(Med.) Min Number Military Buildings" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i) [merged: cp named it keecpbMinNumberMilitaryBuildingsMedium]
    kCPBehaviorEpochCatchUpModifier = 107,                  // setrilo epochCatchUpEasy_/Hard_/Deathmatch_ (record 107); EEComputerPlayer::Init 0x42f1c8 mEpochCatchUpEasy/Hard/Deathmatch (Fn2F064 0x42f064 scales the record-102 delay)
    kCPBehaviorDesiredNumberOfTankFactories = 108,          // Language.rc 28108 "Desired Number of Tank Factories" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDesiredNumberOfAntiAirBuildings = 109,       // Language.rc 28109 "Desired Number of Anti-Air Buildings" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorRPSSelectionIslands = 110,                   // Language.rc 28110 "RPS Selection Islands" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorNavalUnitWeight = 111,                       // Language.rc 28111 "Naval Unit Weight" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorAirUnitWeight = 112,                         // Language.rc 28112 "Air Unit Weight" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDesiredNumberOfFlameThrowers = 113,          // Language.rc 28113 "Desired Number of Flame Throwers" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorTownGuardUpdate = 114,                       // Language.rc 28114 "Town Guard Update" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorTownGuardUpdateDeathmatch = 115,             // Language.rc 28115 "Town Guard Update - Deathmatch" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDefensiveUnitUpdateDeathmatch = 116,         // Language.rc 28116 "Defensive Unit Update - Deathmatch" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDesiredNumberOfSpaceDocks = 117,             // no string 28117 (AoC record); EEComputerPlayer::Init 0x42f1c8 mSpaceDockMax = (long)float value, mLEB8 = value1, the shape of records 30..42 "Desired Number of ..."; mMaxCounts[0x2d8] (provisional name)
    kCPBehaviorMinimumShipMaintenanceTimer = 118,           // Language.rc 28118 "Minimum Ship Maintenance Timer" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorSiegeWeaponPercentage = 119,                 // Language.rc 28119 "Siege Weapon Percentage" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorBonzaiAttackLevel = 120,                     // Language.rc 28120 "Bonzai Attack Level" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorIslandExpansionTimer = 121,                  // Language.rc 28121 "Island Expansion Timer" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDMAttackFrequencyModifier = 122,             // Language.rc 28122 "DM Attack Frequency Modifier" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorDMInitialAttackModifier = 123,               // Language.rc 28123 "DM Initial Attack Modifier" (EESETSETSetAIAttribute::Show 0x58c96c names record i by string 28000 + i)
    kCPBehaviorCarrierFightersToMaintain = 124,             // no string 28124; EECPDefenseCommander::CheckNaval 0x47b502: wanted carrier fighters by difficulty (value1/value2/float = Easy/Medium/Hard), not Deathmatch (provisional name)
    kCPBehaviorCarrierFightersToMaintainDM = 125,           // no string 28125; EECPDefenseCommander::CheckNaval 0x47b502: the Deathmatch counterpart of record 124 (provisional name)
};

#endif
