// The computer-player decision trace and the cheat switches (ee/EECPTrace.h; docs/AI_RESEARCH.md §5 stage 0).
// New in this port: no original address.
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPUnitManager.h"
#include "ee/EECPBuildManager.h"
#include "ee/EECPResourceManager.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGGather.h"
#include "ee/EEUGBuildRepair.h"
#include "ee/EEUnitAction.h"
#include "ee/EECPTrace.h"

bool gCPTraceOn = false;
const char* gCPTraceManager = "-";

static bool sInitialized = false;
static long sCheats = kAICheatAll;         // EE_AI_CHEATS (TEST ONLY): the cheats it lets through
static char sCheatNames[128];
static std::vector<long> sPersonalities;   // EE_AI_PERSONALITY (TEST ONLY): positions in the rules' personalities
static const long kMaxTracedPlayers = 64;
static unsigned long sNextStats[kMaxTracedPlayers];
static unsigned long sNextProfile[kMaxTracedPlayers];
static long long sProfileSum[kMaxTracedPlayers];
static long long sProfileMax[kMaxTracedPlayers];
static unsigned long sProfileCount[kMaxTracedPlayers];
static long sLost[kMaxTracedPlayers][3];   // since the last stats line: citizens, buildings, other units

void EECPTraceInit()
{
    if (sInitialized)
        return;
    sInitialized = true;
    const char* theTrace = getenv("EE_AI_TRACE");
    gCPTraceOn = theTrace != NULL && theTrace[0] != '\0' && strcmp(theTrace, "0") != 0;
    const char* thePersonality = getenv("EE_AI_PERSONALITY");
    if (thePersonality != NULL && thePersonality[0] != 0) {
        std::string theList(thePersonality);
        size_t theStart = 0;
        while (theStart <= theList.size()) {
            size_t theEnd = theList.find(',', theStart);
            if (theEnd == std::string::npos)
                theEnd = theList.size();
            std::string theName = theList.substr(theStart, theEnd - theStart);
            long k = EEGetRules().mAIPersonalities.Find(theName);
            if (k >= 0)
                sPersonalities.push_back(k);
            else
                ESDebugOut("EE-AI EE_AI_PERSONALITY: unknown personality '%s' ignored\n", theName.c_str());
            theStart = theEnd + 1;
        }
    }
    const char* theCheats = getenv("EE_AI_CHEATS");
    if (theCheats != NULL && theCheats[0] != '\0' && strcmp(theCheats, "all") != 0) {
        sCheats = 0;
        std::string theList(theCheats);
        size_t theStart = 0;
        while (theStart <= theList.size()) {
            size_t theEnd = theList.find(',', theStart);
            if (theEnd == std::string::npos)
                theEnd = theList.size();
            std::string theName = theList.substr(theStart, theEnd - theStart);
            long theBit = EEAICheatBit(theName);
            if (theBit == 0 && theName != "none" && !theName.empty())
                ESDebugOut("EE-AI EE_AI_CHEATS: unknown cheat '%s' ignored\n", theName.c_str());
            sCheats |= theBit;
            theStart = theEnd + 1;
        }
    }
}

const EEAIPersonalityRule& EECPPersonality(const EEComputerPlayer* inPlayer)
{
    EECPTraceInit();
    const EEAIPersonalityRules& theRules = EEGetRules().mAIPersonalities;
    if (!sPersonalities.empty()) {
        unsigned long theID = inPlayer->mPlayerID;
        long k = sPersonalities[(theID > 0 ? theID - 1 : 0) % sPersonalities.size()];
        if (k >= 0 && (size_t)k < theRules.mPersonalities.size())
            return theRules.mPersonalities[k];
    }
    return theRules.ForPlayer(inPlayer->mPlayerID);
}

// The game's difficulty's cheats (gameoptions.yaml ai_cheats), narrowed by EE_AI_CHEATS
static long EECPCheats()
{
    EECPTraceInit();
    return EEDifficultyAICheats(gWorld.mLobbyOptions.mDifficulty) & sCheats;
}

bool EECPCheatOn(long inCheat)
{
    return (EECPCheats() & inCheat) != 0;
}

const char* EECPCheatNames()
{
    long theCheats = EECPCheats();
    sCheatNames[0] = '\0';
    for (unsigned long i = 0; i < kEEAICheatCount; i++)
        if (theCheats & (1L << i)) {
            if (sCheatNames[0] != '\0')
                strcat(sCheatNames, ",");
            strcat(sCheatNames, kEEAICheatNames[i]);
        }
    if (sCheatNames[0] == '\0')
        strcpy(sCheatNames, "none");
    return sCheatNames;
}

void EECPTraceOut(EEComputerPlayer* inPlayer, const char* inEvent, const char* inFormat, ...)
{
    char theText[640];
    va_list theArguments;
    va_start(theArguments, inFormat);
    vsnprintf(theText, sizeof(theText), inFormat, theArguments);
    va_end(theArguments);
    const long* theResources = inPlayer->mResourceCaps;
    ESDebugOut("EE-AI %lu p%lu %s %s %s | f=%ld w=%ld s=%ld g=%ld i=%ld\n", gPhysics->mGameTime, inPlayer->mPlayerID,
        gCPTraceManager, inEvent, theText, theResources[kResourceFood], theResources[kResourceWood],
        theResources[kResourceStone], theResources[kResourceGold], theResources[kResourceIron]);
}

static bool EECPIsBuildingCategory(long inCategory)
{
    return inCategory == kUnitCategoryBuilding || inCategory == kUnitCategoryNavalBuilding
        || inCategory == kUnitCategoryMiningBuilding || inCategory == kUnitCategoryGate
        || inCategory == kUnitCategoryFortress || inCategory == kUnitCategoryBuildingPriest
        || inCategory == kUnitCategorySpaceBuilding;
}

// A unit leaving the player (not swapped: killed, converted or deleted), by kind, for the next stats line
void EECPTraceUnitLost(EEComputerPlayer* inPlayer, EEUnit* inUnit)
{
    unsigned long theID = inPlayer->mPlayerID;
    if (theID >= kMaxTracedPlayers || inUnit == NULL || inUnit->mUnitClass == NULL || inUnit->mComplexUnit == NULL)
        return;
    long theCategory = inUnit->mUnitClass->mCategory;
    if (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen)
        sLost[theID][0]++;
    else if (EECPIsBuildingCategory(theCategory))
        sLost[theID][1]++;
    else if (theCategory != kUnitCategoryProjectile)
        sLost[theID][2]++;
}

// One "stats" line per player every 30 game s: epoch, population and its caps, citizens (idle: no goal; free: the
// ones the unit manager would hand out to build), buildings, other units; the benchmark (tools/harness/ai_bench.py) reads them.
void EECPTraceStats(EEComputerPlayer* inPlayer, unsigned long inTime)
{
    unsigned long theID = inPlayer->mPlayerID;
    if (!gCPTraceOn || theID >= kMaxTracedPlayers)
        return;
    if (sNextStats[theID] > inTime + 30000)
        sNextStats[theID] = 0;   // a new game in the same process
    if (inTime < sNextStats[theID])
        return;
    sNextStats[theID] = inTime + 30000;
    long theCitizens = 0;
    long theBuildings = 0;
    long theOther = 0;
    long theUnfinished = 0;
    long theIdle = 0;
    long theGatherers[kResourceIron + 1] = {0};   // by the target's resource type (0: unknown)
    long theFood[5] = {0, 0, 0, 0, 0};            // food gatherers by source: forage bush, animal (hunting), fish, farm, other
    long theGoals[16] = {0};                      // citizens by active goal kind (EEUnitGoalKind; 0 none)
    long theBuilders[6] = {0, 0, 0, 0, 0, 0};     // build/repair goals: target unbuilt, built; action build, repair,
    long theBuildClass = -1;                      // move to the site, other; the first unbuilt target's class
    long theOtherAction = -1;                     // the first "other" builder's action and its target's hit points share
    long theOtherHealth = -1;
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->mComplexUnit == NULL || !(theUnit->mHitPoints > 0))
            continue;
        long theCategory = theUnit->mUnitClass->mCategory;
        if (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen) {
            theCitizens++;
            EEUnitGoal* theGoal = theUnit->mComplexUnit->mActiveGoal;
            theGoals[theGoal == NULL ? 0 : theGoal->mKind >= 0 && theGoal->mKind < 16 ? theGoal->mKind : 15]++;
            if (theGoal != NULL && theGoal->mKind == kGoalBuildRepair) {
                EEUnit* theTarget = static_cast<EEUGBuildRepair*>(theGoal)->mTarget;
                EEComplexUnit* theComplex = theTarget != NULL ? theTarget->mComplexUnit : NULL;
                bool theBuilt = theComplex != NULL && theComplex->mIsBuilt;
                theBuilders[theBuilt ? 1 : 0]++;
                if (!theBuilt && theBuildClass < 0 && theTarget != NULL)
                    theBuildClass = theTarget->mUnitClass->mUnitType;
                long theAction = theUnit->mComplexUnit->mCommand ? theUnit->mComplexUnit->mCommand->mType : 0;
                theBuilders[theAction == kActionBuildBuilding ? 2 : theAction == kActionRepairBuilding ? 3
                            : theAction == kActionMoveToRepairSite ? 4 : 5]++;
                if (theAction != kActionBuildBuilding && theAction != kActionRepairBuilding && theAction != kActionMoveToRepairSite
                    && theOtherAction < 0 && theTarget != NULL) {
                    theOtherAction = theAction;
                    theOtherHealth = UMFloatToLong(theTarget->mHitPoints);
                }
            }
            if (theGoal == NULL)
                theIdle++;
            else if (theGoal->mKind == kGoalGather) {
                EEUnit* theResource = static_cast<EEUGGather*>(theGoal)->mResource;
                unsigned long theType = theResource != NULL ? theResource->mResourceType : 0;
                theGatherers[theType >= kResourceFood && theType <= kResourceIron ? theType : 0]++;
                if (theType == kResourceFood) {
                    long theClass = theResource->mUnitClass->mUnitType;
                    unsigned long theFamily = theResource->mUnitClass->GetFamily();
                    theFood[theClass == kUnitTypeResForageBush ? 0 : theFamily == kUnitFamilyAnimal ? 1
                            : theFamily == kUnitFamilyFish || theClass == kUnitTypeResFish ? 2
                            : theClass == kUnitTypeFarm || theClass == kUnitTypeRoboticFarm ? 3 : 4]++;
                }
            } else if (theGoal->mKind == kGoalAttackUnit)
                theFood[1]++;   // a citizen attacking: hunting (the AI's citizens attack only animals)
        }
        else if (EECPIsBuildingCategory(theCategory)) {
            theBuildings++;
            if (!theUnit->mComplexUnit->mIsBuilt)
                theUnfinished++;
        } else
            theOther++;
    }
    std::vector<EEComplexUnit*> theFree;   // the citizens the build manager may take (idle or gathering)
    if (inPlayer->mCivilizationManager != NULL && inPlayer->mCivilizationManager->mUnitManager != NULL)
        inPlayer->mCivilizationManager->mUnitManager->GetIdleCitizens(theFree);
    long theEpoch = inPlayer->mTechnologies != NULL ? inPlayer->mTechnologies->mCurrentEpoch : -1;
    const char* theSaved = gCPTraceManager;
    gCPTraceManager = "cp";
    EECPTraceOut(inPlayer, "losses", "citizens=%ld buildings=%ld other=%ld", sLost[theID][0], sLost[theID][1], sLost[theID][2]);
    sLost[theID][0] = sLost[theID][1] = sLost[theID][2] = 0;
    EECPTraceOut(inPlayer, "stats", "epoch=%ld pop=%ld cap=%ld limit=%ld citizens=%ld idle=%ld free=%lu gather=%ld/%ld/%ld/%ld/%ld/%ld "
        "buildings=%ld unfinished=%ld other=%ld",
        theEpoch, inPlayer->GetTotalPopulation(true, true), inPlayer->mPopulationCap,
        inPlayer->EEPlayer::GetPopulationLimit(0), theCitizens, theIdle, (unsigned long)theFree.size(),
        theGatherers[kResourceFood], theGatherers[kResourceWood], theGatherers[kResourceStone],
        theGatherers[kResourceGold], theGatherers[kResourceIron], theGatherers[0], theBuildings, theUnfinished, theOther);
    // the resource manager's model: the gatherers it wants per resource, the ones it has listed, and its needs
    // ledger (queued costs earned minus what was gathered or started; UpdateWantedGatherers splits by it)
    if (inPlayer->mCivilizationManager != NULL && inPlayer->mCivilizationManager->mResourceManager != NULL) {
        EECPResourceManager* theRM = inPlayer->mCivilizationManager->mResourceManager;
        // the resource units the player's resource centers hold, by resource, and the gatherers they take
        // (mMiningLimit per unit); "centers" the resource centers
        unsigned long theSites[kResourceIron + 1] = {0};
        for (unsigned long k = 0; k < inPlayer->GetNumResourceCenters(); k++) {
            std::vector<EEUnit*>& theUnits = inPlayer->GetResourceCenter(k)->mUnits;
            for (unsigned long u = 0; u < theUnits.size(); u++) {
                unsigned long theType = theUnits[u]->mResourceType;
                if (theType >= kResourceFood && theType <= kResourceIron && theUnits[u]->mHitPoints > 0)
                    theSites[theType]++;
            }
        }
        EECPTraceOut(inPlayer, "sites", "units=%lu/%lu/%lu/%lu/%lu limit=%ld centers=%lu wanted_citizens=%ld workers=%lu",
            theSites[kResourceFood], theSites[kResourceWood], theSites[kResourceStone], theSites[kResourceGold],
            theSites[kResourceIron], inPlayer->mMiningLimit, (unsigned long)inPlayer->GetNumResourceCenters(),
            inPlayer->mCivilizationManager->mUnitManager != NULL ? inPlayer->mCivilizationManager->mUnitManager->GetWantedCitizens() : -1L,
            inPlayer->mCivilizationManager->mUnitManager != NULL ? inPlayer->mCivilizationManager->mUnitManager->GetNumWorkers() : 0UL);
        EECPTraceOut(inPlayer, "food", "bush=%ld hunt=%ld fish=%ld farm=%ld other=%ld goals gather=%ld build=%ld move=%ld attack=%ld garrison=%ld none=%ld",
            theFood[0], theFood[1], theFood[2], theFood[3], theFood[4], theGoals[kGoalGather], theGoals[kGoalBuildRepair],
            theGoals[kGoalLocation], theGoals[kGoalAttackUnit], theGoals[kGoalLoadGarrison], theGoals[0]);
        EECPTraceOut(inPlayer, "builders", "unbuilt=%ld built=%ld act build=%ld repair=%ld move=%ld other=%ld cls=%ld other_action=%ld other_hp=%ld",
            theBuilders[0], theBuilders[1], theBuilders[2], theBuilders[3], theBuilders[4], theBuilders[5], theBuildClass,
            theOtherAction, theOtherHealth);
        EECPTraceOut(inPlayer, "econ", "want=%lu/%lu/%lu/%lu/%lu listed=%lu/%lu/%lu/%lu/%lu/%lu need=%ld/%ld/%ld/%ld/%ld",
            theRM->mWantedGatherers[2], theRM->mWantedGatherers[3], theRM->mWantedGatherers[4],
            theRM->mWantedGatherers[5], theRM->mWantedGatherers[6], (unsigned long)theRM->mGatherers[2].size(),
            (unsigned long)theRM->mGatherers[3].size(), (unsigned long)theRM->mGatherers[4].size(),
            (unsigned long)theRM->mGatherers[5].size(), (unsigned long)theRM->mGatherers[6].size(),
            (unsigned long)theRM->mGatherers[0].size(), theRM->mNeeds[2], theRM->mNeeds[3], theRM->mNeeds[4],
            theRM->mNeeds[5], theRM->mNeeds[6]);
    }
    gCPTraceManager = theSaved;
}

// The AI's real time per player: every 60 game s the sum and the largest single update (microseconds).
void EECPTraceProfile(EEComputerPlayer* inPlayer, unsigned long inTime, long long inMicroseconds)
{
    unsigned long theID = inPlayer->mPlayerID;
    if (!gCPTraceOn || theID >= kMaxTracedPlayers)
        return;
    sProfileSum[theID] += inMicroseconds;
    sProfileCount[theID]++;
    if (inMicroseconds > sProfileMax[theID])
        sProfileMax[theID] = inMicroseconds;
    if (sNextProfile[theID] > inTime + 60000)
        sNextProfile[theID] = 0;   // a new game in the same process
    if (inTime < sNextProfile[theID])
        return;
    sNextProfile[theID] = inTime + 60000;
    ESDebugOut("EE-AIPROF %lu p%lu updates=%lu sum_us=%lld max_us=%lld\n", inTime, theID, sProfileCount[theID],
        sProfileSum[theID], sProfileMax[theID]);
    sProfileSum[theID] = 0;
    sProfileMax[theID] = 0;
    sProfileCount[theID] = 0;
}

// The computer players' share of the out-of-synch checksum (EEPhysics::GetOutOfSynchChecksum; docs/AI_RESEARCH.md
// Q2). The original's checksum covers the units, the map, the continents and the shared random seed, but none of the
// AI's own state, so AI state that drifts between peers showed only later, through the random seed or the units.
// This adds every player's resources and, per computer player, its pop cap, epoch timer, queue lengths and groups.
unsigned long EECPOutOfSynchChecksum()
{
    unsigned long theChecksum = 0;
    for (unsigned long i = 0; i < gWorld.mNumPlayers; i++) {
        EEPlayer* thePlayer = gWorld.mPlayers[i];
        if (thePlayer == NULL)
            continue;
        unsigned long theSum = 0;
        for (long r = kResourceFood; r <= kResourceIron; r++)
            theSum = theSum * 31 + (unsigned long)thePlayer->mResourceCaps[r];
        EEComputerPlayer* theCP = thePlayer->mComputerPlayer;
        if (theCP != NULL) {
            theSum = theSum * 31 + (unsigned long)theCP->mPopulationCap;
            if (EECPCivilizationManager* theCiv = theCP->mCivilizationManager) {
                theSum = theSum * 31 + theCiv->mNextEpochAdvanceTime;
                if (theCiv->mBuildManager != NULL)
                    for (long e = 0; e < EEEpochCount(); e++)
                        theSum = theSum * 31 + (unsigned long)theCiv->mBuildManager->mQueues[e].size();
                if (theCiv->mUnitManager != NULL)
                    theSum = theSum * 31 + (unsigned long)theCiv->mUnitManager->mNumWaiting;
            }
            if (theCP->mCombatManager != NULL && theCP->mCombatManager->mAttackCommander != NULL)
                theSum = theSum * 31 + (unsigned long)theCP->mCombatManager->mAttackCommander->mGroups.size();
        }
        theChecksum += theSum * (thePlayer->mPlayerID + 1);
    }
    return theChecksum;
}

extern long gU531CF0;   // EEPhysics.cpp: the user-input messages processed (part of the out-of-synch checksum)

// TEST (with the trace): every 10 game s, the parts of the out-of-synch checksum, as a compared trace line
// ("EE-AI <ms> p0 sim hash ..."): when two runs of a seed diverge, the first differing line says when (to 10 s) and
// what first (the units, the shared random seed, the input count, or the players' resources and AI state)
void EECPTraceHash(unsigned long inTime)
{
    static unsigned long sNextHash = 0;
    if (!gCPTraceOn)
        return;
    if (sNextHash > inTime + 10000)
        sNextHash = 0;   // a new game in the same process
    if (inTime < sNextHash)
        return;
    sNextHash = inTime + 10000;
    unsigned long theUnits = 0;
    unsigned long theCount = 0;
    for (RTSUnit* theUnit = gWorld.mUnits.mFirst; theUnit != NULL; theUnit = theUnit->mTileNode->mNext) {
        theUnits += static_cast<EEUnit*>(theUnit)->GetChecksum(3);   // hit points, tiles, positions, commands
        theCount++;
    }
    ESDebugOut("EE-AI %lu p0 sim hash pt=%lu units=%lu/%08lx rng=%08lx inputs=%ld ai=%08lx | f=0 w=0 s=0 g=0 i=0\n", inTime,
        (unsigned long)gPhysics->mCurrentTick, theCount, theUnits, (unsigned long)gPhysics->mRandom.GetSeed(),
        gU531CF0, EECPOutOfSynchChecksum());
}
