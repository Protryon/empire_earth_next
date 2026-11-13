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
#include "ee/EECPResearchManager.h"
#include "ee/EECPResourceManager.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPILocation.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EECPASTargetUnit.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EECPAGTask.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUGGather.h"
#include "ee/EEUGBuildRepair.h"
#include "ee/EEUnitAction.h"
#include "ee/EECPTrace.h"
#include "ee/EECPExperimentTimers.h"
#include "ee/EECPNext.h"
#include "ee/EEGameLog.h"
#include "lle/U2DPoint.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"
#include <math.h>

bool gCPTraceOn = false;
bool gCPTraceHashOn = false;
const char* gCPTraceManager = "-";

void EECPRMDumpFoodCapacity(EECPResourceManager* inManager, FILE* inFile);   // EECPResourceManager.cpp (TEST ONLY)
static bool sInitialized = false;
static std::vector<std::string> sExperiments;   // EE_AI_EXP (TEST ONLY): ",a,b," per player in turn
static std::vector<bool> sDemandSplit;     // EE_AI_DEMAND (TEST ONLY): overrides economy.demand_split, per player in turn ('/' between)
static std::vector<long> sCheats;          // EE_AI_CHEATS (TEST ONLY): the cheats it lets through, per player in turn
static char sCheatNames[128];
static std::vector<long> sPersonalities;   // EE_AI_PERSONALITY (TEST ONLY): positions in the rules' personalities
static const long kMaxTracedPlayers = 64;
static unsigned long sNextStats[kMaxTracedPlayers];
static unsigned long sNextProfile[kMaxTracedPlayers];
static long long sProfileSum[kMaxTracedPlayers];
static long long sProfileMax[kMaxTracedPlayers];
static unsigned long sProfileCount[kMaxTracedPlayers];
static long sLost[kMaxTracedPlayers][3];   // since the last stats line: citizens, buildings, other units
static long sLostNear[kMaxTracedPlayers][3];   // citizens lost by distance from the town center: <15, 15-30, more tiles
static long sLostBy[kMaxTracedPlayers][3];     // citizens lost by their last attacker: a player's unit, a wild one (player 0), none
static long sIncome[kMaxTracedPlayers][kResourceIron + 1];   // resources got since the last income line
static long sSpend[kMaxTracedPlayers][8][5];                 // TEST ONLY: paid since the last spend line, by kind

void EECPTraceInit()
{
    if (sInitialized)
        return;
    sInitialized = true;
    const char* theTrace = getenv("EE_AI_TRACE");
    gCPTraceOn = theTrace != NULL && theTrace[0] != '\0' && strcmp(theTrace, "0") != 0;
    gCPTraceHashOn = gCPTraceOn;   // the sim hash only with EE_AI_TRACE (the game log turns gCPTraceOn on alone)
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
    const char* theDemand = getenv("EE_AI_DEMAND");
    if (theDemand != NULL && theDemand[0] != '\0') {
        std::string theList(theDemand);
        size_t theStart = 0;
        while (theStart <= theList.size()) {
            size_t theEnd = theList.find('/', theStart);
            if (theEnd == std::string::npos)
                theEnd = theList.size();
            std::string theValue = theList.substr(theStart, theEnd - theStart);
            sDemandSplit.push_back(!theValue.empty() && theValue != "0");
            theStart = theEnd + 1;
        }
    }
    const char* theCheats = getenv("EE_AI_CHEATS");
    // "a,b": one set for every computer player; "set/set/...": the sets taken in turn by player (as EE_AI_PERSONALITY)
    if (theCheats != NULL && theCheats[0] != '\0') {
        std::string theSets(theCheats);
        size_t theSetStart = 0;
        while (theSetStart <= theSets.size()) {
            size_t theSetEnd = theSets.find('/', theSetStart);
            if (theSetEnd == std::string::npos)
                theSetEnd = theSets.size();
            std::string theList = theSets.substr(theSetStart, theSetEnd - theSetStart);
            long theSet = 0;
            size_t theStart = 0;
            while (theStart <= theList.size()) {
                size_t theEnd = theList.find(',', theStart);
                if (theEnd == std::string::npos)
                    theEnd = theList.size();
                std::string theName = theList.substr(theStart, theEnd - theStart);
                long theBit = theName == "all" ? (long)kAICheatAll : EEAICheatBit(theName);
                if (theBit == 0 && theName != "none" && !theName.empty())
                    ESDebugOut("EE-AI EE_AI_CHEATS: unknown cheat '%s' ignored\n", theName.c_str());
                theSet |= theBit;
                theStart = theEnd + 1;
            }
            sCheats.push_back(theSet);
            theSetStart = theSetEnd + 1;
        }
    }
    // the named experiments, "a,b" for every computer player or "set/set/..." in turn by player ("-" or "": none)
    const char* theExperiments = getenv("EE_AI_EXP");
    if (theExperiments != NULL && theExperiments[0] != '\0') {
        std::string theSets(theExperiments);
        size_t theSetStart = 0;
        while (theSetStart <= theSets.size()) {
            size_t theSetEnd = theSets.find('/', theSetStart);
            if (theSetEnd == std::string::npos)
                theSetEnd = theSets.size();
            std::string theSet = "," + theSets.substr(theSetStart, theSetEnd - theSetStart) + ",";
            for (size_t c = 0; c < theSet.size(); c++)
                if (theSet[c] == '+')
                    theSet[c] = ',';   // "a+b" joins experiments too (a comma-separated --env-vary value cannot hold ',')
            sExperiments.push_back(theSet);
            theSetStart = theSetEnd + 1;
        }
    }
}

bool EECPExperiment(const EEComputerPlayer* inPlayer, const char* inName)
{
    EECPTraceInit();
    if (sExperiments.empty())
        return false;
    unsigned long theID = inPlayer->mPlayerID;
    const std::string& theSet = sExperiments[(theID > 0 ? theID - 1 : 0) % sExperiments.size()];
    return theSet.find("," + std::string(inName) + ",") != std::string::npos;
}

const char* EECPExperimentNames(const EEComputerPlayer* inPlayer)
{
    EECPTraceInit();
    if (sExperiments.empty())
        return "-";
    unsigned long theID = inPlayer->mPlayerID;
    const std::string& theSet = sExperiments[(theID > 0 ? theID - 1 : 0) % sExperiments.size()];
    return theSet.size() > 2 ? theSet.c_str() : "-";
}

bool EECPMilitiaOn(const EEComputerPlayer* inPlayer)
{
    return EECPPersonality(inPlayer).mMilitia && !EECPExperiment(inPlayer, "nomilitia");
}

bool EECPCoastFishingOn(const EEComputerPlayer* inPlayer)
{
    return EECPPersonality(inPlayer).mCoastFishing && !EECPExperiment(inPlayer, "nofish");
}

bool EECPMilitiaTowersOn(const EEComputerPlayer* inPlayer)
{
    return EECPPersonality(inPlayer).mMilitiaTowers && !EECPExperiment(inPlayer, "nomtower");
}

long EECPPrehistoricArmy(const EEComputerPlayer* inPlayer)
{
    if (EECPExperiment(inPlayer, "prearmy4"))
        return 4;   // TEST ONLY: the owner's "maybe 4 clubmen before stone"
    return EECPExperiment(inPlayer, "noprearmy") ? 0 : EECPPersonality(inPlayer).mPrehistoricArmy;
}

static bool EECPRuleOn(const EEComputerPlayer* inPlayer, bool inRule, const char* inOn, const char* inOff)
{
    if (EECPExperiment(inPlayer, inOn))
        return true;
    return inRule && !EECPExperiment(inPlayer, inOff);
}

bool EECPForagePatchesOn(const EEComputerPlayer* inPlayer)
{
    return EECPRuleOn(inPlayer, EECPPersonality(inPlayer).mForagePatches, "foodeco", "nofoodeco");
}

bool EECPClaimNeededOn(const EEComputerPlayer* inPlayer)
{
    return EECPRuleOn(inPlayer, EECPPersonality(inPlayer).mClaimNeeded, "goldfix", "nogoldfix");
}

bool EECPNavalFleeOn(const EEComputerPlayer* inPlayer)
{
    return EECPRuleOn(inPlayer, EECPPersonality(inPlayer).mNavalFlee, "navalflee", "nonavalflee");
}

long EECPEarlyTownCenters(const EEComputerPlayer* inPlayer)
{
    if (EECPExperiment(inPlayer, "noupgrade"))
        return 2;
    return EECPExperiment(inPlayer, "nonoupgrade") ? 0 : EECPPersonality(inPlayer).mEarlyTownCenters;
}

long EECPEarlyTrainers(const EEComputerPlayer* inPlayer)
{
    if (EECPExperiment(inPlayer, "fewbarracks"))
        return 2;
    return EECPExperiment(inPlayer, "manybarracks") ? 0 : EECPPersonality(inPlayer).mEarlyTrainers;
}

bool EECPPatchExpandOn(const EEComputerPlayer* inPlayer)
{
    return EECPRuleOn(inPlayer, EECPPersonality(inPlayer).mPatchExpand, "patchexpand", "nopatchexpand");
}

float EECPSafeBonus(const EEComputerPlayer* inPlayer)
{
    if (EECPExperiment(inPlayer, "safedash"))
        return 0.25f;
    return EECPExperiment(inPlayer, "nosafedash") ? 0.0f : EECPPersonality(inPlayer).mSafeBonus;
}

bool EECPStonePeaceOn(const EEComputerPlayer* inPlayer)
{
    return EECPRuleOn(inPlayer, !EECPPersonality(inPlayer).mStoneAttacks, "stonepeace", "nostonepeace");
}

bool EECPPrehistoricPeaceOn(const EEComputerPlayer* inPlayer)
{
    return EECPRuleOn(inPlayer, !EECPPersonality(inPlayer).mPrehistoricAttacks, "prepeace", "noprepeace");
}

// port: saved from 0.265 (ee/EECPExperimentTimers.h EECPSaveResourceState)
static long (&sClaimSteps)[64] = gCPExperimentTimers.mClaimSteps;
static unsigned long (&sClaimLast)[64] = gCPExperimentTimers.mClaimLast;

long EECPClaimRange(const EEComputerPlayer* inPlayer)
{
    if (EECPNextEngine(inPlayer))
        return EECPNextClaimRangeOf(inPlayer);   // the next engine: its claim_range (ee/EECPNext.h)
    unsigned long theID = inPlayer->mPlayerID < 64 ? inPlayer->mPlayerID : 63;
    if (gPhysics->mGameTime < sClaimLast[theID])
        sClaimSteps[theID] = 0, sClaimLast[theID] = 0;   // a new game (the clock started over)
    long theRange = kEECPClaimRange + sClaimSteps[theID] * kEECPClaimRangeStep;
    return theRange < kEECPClaimRangeMax ? theRange : kEECPClaimRangeMax;
}

void EECPClaimExpand(const EEComputerPlayer* inPlayer)
{
    if (EECPNextEngine(inPlayer))
        return;   // the next engine's strategy sets the range (claim_range)
    unsigned long theID = inPlayer->mPlayerID < 64 ? inPlayer->mPlayerID : 63;
    unsigned long theNow = gPhysics->mGameTime;
    if (theNow < sClaimLast[theID])
        sClaimSteps[theID] = 0, sClaimLast[theID] = 0;
    if (sClaimLast[theID] != 0 && theNow - sClaimLast[theID] < 120000)
        return;
    if (EECPClaimRange(inPlayer) >= kEECPClaimRangeMax)
        return;
    sClaimSteps[theID]++;
    sClaimLast[theID] = theNow;
    EE_CP_TRACE(const_cast<EEComputerPlayer*>(inPlayer), "claim.expand", "range=%ld", EECPClaimRange(inPlayer));
}

bool EECPClaimRangeOn(const EEComputerPlayer* inPlayer)
{
    return EECPForagePatchesOn(inPlayer) && !EECPExperiment(inPlayer, "oldpatches");
}

float EECPStoneSpan(const EEComputerPlayer* inPlayer)
{
    if (EECPExperiment(inPlayer, "stone22"))
        return 22.0f;
    if (EECPExperiment(inPlayer, "stone30"))
        return 30.0f;
    return EECPExperiment(inPlayer, "nostone") ? 0.0f : EECPPersonality(inPlayer).mStoneSpan;
}

bool EECPDemandSplitOn(const EEComputerPlayer* inPlayer)
{
    EECPTraceInit();
    if (sDemandSplit.empty())
        return EECPPersonality(inPlayer).mDemandSplit;
    unsigned long theID = inPlayer->mPlayerID;
    return sDemandSplit[(theID > 0 ? theID - 1 : 0) % sDemandSplit.size()];
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

// The game's difficulty's cheats (gameoptions.yaml ai_cheats), narrowed by EE_AI_CHEATS (the player's set)
static long EECPCheats(const EEComputerPlayer* inPlayer)
{
    EECPTraceInit();
    long theMask = kAICheatAll;
    if (!sCheats.empty()) {
        unsigned long theID = inPlayer != NULL ? inPlayer->mPlayerID : 1;
        theMask = sCheats[(theID > 0 ? theID - 1 : 0) % sCheats.size()];
    }
    return EEDifficultyAICheats(gWorld.mLobbyOptions.mDifficulty) & theMask;
}

bool EECPCheatOn(const EEComputerPlayer* inPlayer, long inCheat)
{
    return (EECPCheats(inPlayer) & inCheat) != 0;
}

const char* EECPCheatNames(const EEComputerPlayer* inPlayer)
{
    long theCheats = EECPCheats(inPlayer);
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
    char theLine[800];
    snprintf(theLine, sizeof(theLine), "EE-AI %lu p%lu %s %s %s | f=%ld w=%ld s=%ld g=%ld i=%ld\n", gPhysics->mGameTime,
        inPlayer->mPlayerID, gCPTraceManager, inEvent, theText, theResources[kResourceFood], theResources[kResourceWood],
        theResources[kResourceStone], theResources[kResourceGold], theResources[kResourceIron]);
    ESDebugOut("%s", theLine);
    if (EEGameLogOn())
        EEGameLogLine(theLine);
}

static bool EECPIsBuildingCategory(long inCategory)
{
    return inCategory == kUnitCategoryBuilding || inCategory == kUnitCategoryNavalBuilding
        || inCategory == kUnitCategoryMiningBuilding || inCategory == kUnitCategoryGate
        || inCategory == kUnitCategoryFortress || inCategory == kUnitCategoryBuildingPriest
        || inCategory == kUnitCategorySpaceBuilding;
}

// A resource amount the player got (EEComputerPlayer::AddResource), for the next income line
void EECPTraceSpend(EEComputerPlayer* inPlayer, long inKind, const long inCost[5])
{
    unsigned long theID = inPlayer->mPlayerID;
    if (!gCPTraceOn || theID >= kMaxTracedPlayers || inKind < 0 || inKind > 7 || !EECPExperiment(inPlayer, "spending"))
        return;
    for (long r = 0; r < 5; r++)
        sSpend[theID][inKind][r] += inCost[r];
}

void EECPTraceIncome(EEComputerPlayer* inPlayer, long inResource, long inAmount)
{
    unsigned long theID = inPlayer->mPlayerID;
    if (theID < kMaxTracedPlayers && inResource >= 0 && inResource <= kResourceIron && inAmount > 0)
        sIncome[theID][inResource] += inAmount;
}

// A soldier, ship or other non-citizen unit lost (any computer seat, the classic's too): one "unit.death" line - its
// class, tile and distance from the town centre, its goal, and the killer (player:class:kind, its distance). The AI
// trace only (gCPTraceOn): it reads the dead unit and its last attacker and changes nothing
static void EECPTraceSoldierLost(EEComputerPlayer* inPlayer, EEUnit* inUnit)
{
    EEComplexUnit* c = inUnit->mComplexUnit;
    long theX = inUnit->mTileX, theY = inUnit->mTileY;
    long theTown = -1;
    U2DPoint<long> theCenter = inPlayer->mCivilizationManager != NULL && inPlayer->mCivilizationManager->mBuildManager != NULL
        ? inPlayer->mCivilizationManager->mBuildManager->mTownCenterTile : U2DPoint<long>(-10000, -10000);
    if (theCenter.mX != -10000 && theX != -10000) {
        long theDX = theX - theCenter.mX, theDY = theY - theCenter.mY;
        theTown = (long)sqrt((double)(theDX * theDX + theDY * theDY));
    }
    const char* theGoal = "none";
    if (c->mActiveGoal != NULL) {
        switch (c->mActiveGoal->mKind) {
        case kGoalLocation: theGoal = "move"; break;
        case kGoalAttackUnit: theGoal = "attack"; break;
        case kGoalAttackGround: theGoal = "attack_ground"; break;
        case kGoalPatrol: theGoal = "patrol"; break;
        case kGoalExplore: theGoal = "explore"; break;
        case kGoalGather: theGoal = "gather"; break;
        case kGoalLoadGarrison: theGoal = "garrison"; break;
        default: theGoal = "other"; break;
        }
    }
    EEComplexUnit* theKiller = c->mLastAttacker;
    long theKillerPlayer = -1, theKillerClass = -1, theKillerDistance = -1;
    char theKind = '-';
    if (theKiller != NULL && theKiller->mUnitClass != NULL) {
        theKillerPlayer = theKiller->mPlayer != NULL ? (long)theKiller->mPlayer->mPlayerID : -1;
        theKillerClass = theKiller->mUnitClass->mUnitType;
        if (theKiller->GetTileX() != -10000 && theX != -10000) {
            long theDX = theKiller->GetTileX() - theX, theDY = theKiller->GetTileY() - theY;
            theKillerDistance = (long)sqrt((double)(theDX * theDX + theDY * theDY));
        }
        EEUnitClass* theClass = theKiller->mUnitClass;
        theKind = theKillerPlayer == 0 ? 'a' : theClass->IsBuilding() ? (theClass->mIsTower ? 't' : 'b')
            : theClass->mCategory == kUnitCategoryLandCitizen ? 'c' : (theClass->mDomains & kMovementSea) ? 'n' : 's';
    }
    // the killer's own goal and its target's kind (s soldier, c citizen, b building, n ship, - none): whom it was after
    const char* theKillerGoal = "-";
    char theKillerTarget = '-';
    if (theKiller != NULL && theKiller->mActiveGoal != NULL) {
        theKillerGoal = theKiller->mActiveGoal->mKind == kGoalAttackUnit ? "attack"
            : theKiller->mActiveGoal->mKind == kGoalLocation ? "move" : "other";
        if (theKiller->mActiveGoal->mKind == kGoalAttackUnit) {
            EEUnit* theTarget = static_cast<EEUGRust*>(theKiller->mActiveGoal)->GetAttackTarget();
            if (theTarget != NULL && theTarget->mUnitClass != NULL) {
                EEUnitClass* theTargetClass = theTarget->mUnitClass;
                theKillerTarget = theTargetClass->IsBuilding() ? 'b'
                    : theTargetClass->mCategory == kUnitCategoryLandCitizen ? 'c'
                    : (theTargetClass->mDomains & kMovementSea) ? 'n' : 's';
            }
        }
    }
    // the classic's sub-group holding it (EECPAttackCommander::FindHolder, read only): its id and mission, its task
    // (type:mode) and that task's distance; the group's task (type:mode) - which of the classic's groups it served
    long theGroup = -1, theMission = -1, theTaskType = -1, theTaskMode = -1, theTaskDistance = -1;
    long theGroupType = -1, theGroupMode = -1;
    if (inPlayer->mCombatManager != NULL && inPlayer->mCombatManager->mAttackCommander != NULL) {
        EECPAttackSubGroup* theHolder = inPlayer->mCombatManager->mAttackCommander->FindHolder(inUnit);
        if (theHolder != NULL) {
            theGroup = (long)theHolder->mID;
            theMission = theHolder->mMission;
            theTaskType = theHolder->mTask.mIsActive ? theHolder->mTask.mType : -1;
            theTaskMode = theHolder->mTask.mMode;
            if (theHolder->mTask.mIsActive && theX != -10000) {
                long theDX = theHolder->mTask.mLocation.mTile.mX - theX, theDY = theHolder->mTask.mLocation.mTile.mY - theY;
                theTaskDistance = (long)sqrt((double)(theDX * theDX + theDY * theDY));
            }
            if (theHolder->mGroup != NULL) {
                theGroupType = theHolder->mGroup->mTask.mType;
                theGroupMode = theHolder->mGroup->mTask.mMode;
            }
        }
    }
    EECPTraceScope theScope("cp");
    EE_CP_TRACE(inPlayer, "unit.death", "unit=%s x=%ld y=%ld d=%ld goal=%s killer=%ld:%s:%c kd=%ld kgoal=%s:%c sub=%ld:%ld:%ld:%ld:%ld grp=%ld:%ld",
        EECPNextClassName(inUnit->mUnitClass->mUnitType).c_str(), theX, theY, theTown, theGoal, theKillerPlayer,
        theKillerClass >= 0 ? EECPNextClassName(theKillerClass).c_str() : "-", theKind, theKillerDistance,
        theKillerGoal, theKillerTarget, theGroup, theMission, theTaskType, theTaskMode, theTaskDistance, theGroupType, theGroupMode);
}

// A unit leaving the player (not swapped: killed, converted or deleted), by kind, for the next stats line
void EECPTraceUnitLost(EEComputerPlayer* inPlayer, EEUnit* inUnit)
{
    unsigned long theID = inPlayer->mPlayerID;
    if (theID >= kMaxTracedPlayers || inUnit == NULL || inUnit->mUnitClass == NULL || inUnit->mComplexUnit == NULL)
        return;
    long theCategory = inUnit->mUnitClass->mCategory;
    if (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen) {
        // (the braces were missing: every unit lost counted in the citizens' by-killer and by-distance fields, and
        // the buildings' and others' counts hung off the town-centre test; AI_RESEARCH §8.41)
        sLost[theID][0]++;
        EEComplexUnit* theKiller = inUnit->mComplexUnit->mLastAttacker;
        sLostBy[theID][theKiller == NULL ? 2 : theKiller->mPlayer != NULL && theKiller->mPlayer->mPlayerID != 0 ? 0 : 1]++;
        U2DPoint<long> theCenter = inPlayer->mCivilizationManager != NULL && inPlayer->mCivilizationManager->mBuildManager != NULL
            ? inPlayer->mCivilizationManager->mBuildManager->mTownCenterTile : U2DPoint<long>(-10000, -10000);
        if (theCenter.mX != -10000) {
            long theDX = inUnit->mTileX - theCenter.mX, theDY = inUnit->mTileY - theCenter.mY;
            long theSquared = theDX * theDX + theDY * theDY;
            sLostNear[theID][theSquared < 225 ? 0 : theSquared < 900 ? 1 : 2]++;
        }
        if (inPlayer->mNextState == NULL)
            EECPTraceSoldierLost(inPlayer, inUnit);   // (a script's seat has its next.death line)
    } else if (EECPIsBuildingCategory(theCategory))
        sLost[theID][1]++;
    else if (theCategory != kUnitCategoryProjectile) {
        sLost[theID][2]++;
        EECPTraceSoldierLost(inPlayer, inUnit);
    }
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
    std::vector<EEUnit*> theGathered;             // each gatherer's resource (the trips line)
    std::map<long, long> theOtherActs;            // the other actions by id (the gatheract line's most common)
    long theActs[7] = {0, 0, 0, 0, 0, 0, 0};      // gatherers by action: gather, drop off, to the resource, to storage,
                                                  // waiting, idle, other (the gatheract line)
    std::vector<EEComplexUnit*> theStores;        // the player's built drop sites
    long theDropSites[3] = {0, 0, 0};             // built settlements, town centers/capitols, granaries
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
                if (theResource != NULL && theType >= kResourceFood && theType <= kResourceIron)
                    theGathered.push_back(theResource);
                long theAct = theUnit->mComplexUnit->mCommand ? theUnit->mComplexUnit->mCommand->mType : 0;
                theActs[theAct == kActionGatherResource ? 0 : theAct == kActionDropoffResource ? 1
                        : theAct == kActionMoveToResource ? 2 : theAct == kActionMoveToStorage ? 3
                        : theAct == kActionMoveToWaitingArea ? 4 : theAct == kActionIdle || theAct == 0 ? 5 : 6]++;
                if (theAct != kActionGatherResource && theAct != kActionDropoffResource && theAct != kActionMoveToResource
                    && theAct != kActionMoveToStorage && theAct != kActionMoveToWaitingArea && theAct != kActionIdle && theAct != 0)
                    theOtherActs[theAct]++;
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
            long theType = theUnit->mUnitClass->mUnitType;
            if (theUnit->mComplexUnit->mIsBuilt && theUnit->mUnitClass->IsDropSite())
                theStores.push_back(theUnit->mComplexUnit);
            if (theUnit->mComplexUnit->mIsBuilt) {
                if (theType == kUnitTypeSettlement)
                    theDropSites[0]++;
                else if (theType == kUnitTypeTownCenter || theType == kUnitTypeCapitol)
                    theDropSites[1]++;
                else if (theType == kUnitTypeGranary)
                    theDropSites[2]++;
            }
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
    EECPTraceOut(inPlayer, "losses", "citizens=%ld buildings=%ld other=%ld citizens_by_distance=%ld/%ld/%ld citizens_by_killer=%ld/%ld/%ld",
        sLost[theID][0], sLost[theID][1], sLost[theID][2], sLostNear[theID][0], sLostNear[theID][1], sLostNear[theID][2],
        sLostBy[theID][0], sLostBy[theID][1], sLostBy[theID][2]);
    sLost[theID][0] = sLost[theID][1] = sLost[theID][2] = 0;
    sLostNear[theID][0] = sLostNear[theID][1] = sLostNear[theID][2] = 0;
    sLostBy[theID][0] = sLostBy[theID][1] = sLostBy[theID][2] = 0;
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
            std::vector<EEUnitHandle<EEUnit> >& theUnits = inPlayer->GetResourceCenter(k)->mUnits;
            for (unsigned long u = 0; u < theUnits.size(); u++) {
                unsigned long theType = theUnits[u]->mResourceType;
                if (theType >= kResourceFood && theType <= kResourceIron && theUnits[u]->mHitPoints > 0)
                    theSites[theType]++;
            }
        }
        EECPTraceOut(inPlayer, "sites", "units=%lu/%lu/%lu/%lu/%lu limit=%ld centers=%lu wanted_citizens=%ld workers=%lu in_production=%ld",
            theSites[kResourceFood], theSites[kResourceWood], theSites[kResourceStone], theSites[kResourceGold],
            theSites[kResourceIron], inPlayer->mMiningLimit, (unsigned long)inPlayer->GetNumResourceCenters(),
            inPlayer->mCivilizationManager->mUnitManager != NULL ? inPlayer->mCivilizationManager->mUnitManager->GetWantedCitizens() : -1L,
            inPlayer->mCivilizationManager->mUnitManager != NULL ? inPlayer->mCivilizationManager->mUnitManager->GetNumWorkers() : 0UL,
            inPlayer->mCivilizationManager->mUnitManager != NULL ? (long)inPlayer->mCivilizationManager->mUnitManager->mNumInProduction : -1L);
        // the mean distance from each gatherer's resource to the nearest built drop site that takes it, by resource, and
        // how many are past the drop-off distance (where BuildSettlementFor asks for a settlement); the drop sites
        double theTrip[kResourceIron + 1] = {0};
        long theTrips[kResourceIron + 1] = {0};
        long theFar[kResourceIron + 1] = {0};     // past the drop-off distance
        // §8.36: gathering with no finished drop site at hand (EECPNextUnitDropReady; patches, mines and trees: the
        // game, fish and farms are not counted)
        long theNoDrop[kResourceIron + 1] = {0};
        for (unsigned long g = 0; g < theGathered.size(); g++) {
            unsigned long theType = theGathered[g]->mResourceType;
            float theBest = -1.0f;
            for (unsigned long k = 0; k < theStores.size(); k++) {
                long theTakes = theStores[k]->mUnitClass->mResourceType;
                if (theTakes != kResourceAny && theTakes != (long)theType)
                    continue;
                float theDistance = theStores[k]->DistanceSquaredToUnit(theGathered[g]);
                if (theBest < 0.0f || theDistance < theBest)
                    theBest = theDistance;
            }
            if (theBest < 0.0f)
                continue;
            long theClass = theGathered[g]->mUnitClass->mUnitType;
            unsigned long theFamily = theGathered[g]->mUnitClass->GetFamily();
            if (theFamily != kUnitFamilyAnimal && theFamily != kUnitFamilyFish && theClass != kUnitTypeResFish
                && theClass != kUnitTypeFarm && theClass != kUnitTypeRoboticFarm
                && !(theType == kResourceWood
                         ? EECPNextDropReady(inPlayer, theGathered[g]->GetTileX(), theGathered[g]->GetTileY(), false, kNextWoodSlack)
                         : EECPNextUnitDropReady(inPlayer, theGathered[g])))
                theNoDrop[theType]++;
            double theLength = sqrt((double)theBest);
            theTrip[theType] += theLength;
            theTrips[theType]++;
            if (theLength > (double)inPlayer->mDropOffDistance)
                theFar[theType]++;
        }
        long theMean[kResourceIron + 1];
        for (long r = 0; r <= kResourceIron; r++)
            theMean[r] = theTrips[r] > 0 ? (long)(theTrip[r] / theTrips[r] + 0.5) : -1;
        EECPTraceOut(inPlayer, "trips", "mean=%ld/%ld/%ld/%ld/%ld n=%ld/%ld/%ld/%ld/%ld far=%ld/%ld/%ld/%ld/%ld dropoff=%ld settlements=%ld towncenters=%ld granaries=%ld nodrop=%ld/%ld/%ld/%ld/%ld",
            theMean[kResourceFood], theMean[kResourceWood], theMean[kResourceStone], theMean[kResourceGold], theMean[kResourceIron],
            theTrips[kResourceFood], theTrips[kResourceWood], theTrips[kResourceStone], theTrips[kResourceGold], theTrips[kResourceIron],
            theFar[kResourceFood], theFar[kResourceWood], theFar[kResourceStone], theFar[kResourceGold], theFar[kResourceIron],
            inPlayer->mDropOffDistance, theDropSites[0], theDropSites[1], theDropSites[2], theNoDrop[kResourceFood],
            theNoDrop[kResourceWood], theNoDrop[kResourceStone], theNoDrop[kResourceGold], theNoDrop[kResourceIron]);
        // what came in since the last income line, by resource (EEComputerPlayer::AddResource: the gatherers'
        // deposits, plus refunds and tributes), and the gatherers on each now
        long theTopOther = -1, theTopCount = 0;
        for (std::map<long, long>::iterator it = theOtherActs.begin(); it != theOtherActs.end(); ++it)
            if (it->second > theTopCount) { theTopCount = it->second; theTopOther = it->first; }
        EECPTraceOut(inPlayer, "gatheract", "gather=%ld dropoff=%ld to_resource=%ld to_storage=%ld waiting=%ld idle=%ld other=%ld top_other=%ld",
            theActs[0], theActs[1], theActs[2], theActs[3], theActs[4], theActs[5], theActs[6], theTopOther);
        EECPTraceOut(inPlayer, "income", "got=%ld/%ld/%ld/%ld/%ld gatherers=%ld/%ld/%ld/%ld/%ld",
            sIncome[theID][kResourceFood], sIncome[theID][kResourceWood], sIncome[theID][kResourceStone],
            sIncome[theID][kResourceGold], sIncome[theID][kResourceIron], theTrips[kResourceFood], theTrips[kResourceWood],
            theTrips[kResourceStone], theTrips[kResourceGold], theTrips[kResourceIron]);
        if (EECPExperiment(inPlayer, "spending")) {   // TEST ONLY: what was bought since the last line
            static const char* const kKinds[8] = {"citizens", "soldiers", "buildings", "techs", "epochs", "boats", "improve", "other"};
            char theLine[1024];
            long theAt = 0;
            for (long k = 0; k < 8 && theAt < 900; k++) {
                theAt += snprintf(theLine + theAt, sizeof(theLine) - theAt, "%s%s=%ld/%ld/%ld/%ld/%ld", k ? " " : "", kKinds[k],
                    sSpend[theID][k][0], sSpend[theID][k][1], sSpend[theID][k][2], sSpend[theID][k][3], sSpend[theID][k][4]);
                for (long r = 0; r < 5; r++)
                    sSpend[theID][k][r] = 0;
            }
            EECPTraceOut(inPlayer, "spend", "%s", theLine);
        }
        for (long r = 0; r <= kResourceIron; r++)
            sIncome[theID][r] = 0;
        EECPTraceOut(inPlayer, "food", "bush=%ld hunt=%ld fish=%ld farm=%ld other=%ld goals gather=%ld build=%ld move=%ld attack=%ld garrison=%ld none=%ld",
            theFood[0], theFood[1], theFood[2], theFood[3], theFood[4], theGoals[kGoalGather], theGoals[kGoalBuildRepair],
            theGoals[kGoalLocation], theGoals[kGoalAttackUnit], theGoals[kGoalLoadGarrison], theGoals[0]);
        EECPTraceOut(inPlayer, "builders", "unbuilt=%ld built=%ld act build=%ld repair=%ld move=%ld other=%ld cls=%ld other_action=%ld other_hp=%ld",
            theBuilders[0], theBuilders[1], theBuilders[2], theBuilders[3], theBuilders[4], theBuilders[5], theBuildClass,
            theOtherAction, theOtherHealth);
        // the pending unit requests other than citizens: class, count, affordable now, built producers, room
        if (inPlayer->mCivilizationManager->mUnitManager != NULL && inPlayer->mTechnologies != NULL) {
            EECPUnitManager* theUM = inPlayer->mCivilizationManager->mUnitManager;
            char theList[200];
            theList[0] = '\0';
            size_t theUsed = 0;
            long theShown = 0;
            for (long e = 0; e < EEEpochCount() && theShown < 4; e++)
                for (unsigned long k = 0; k < theUM->mRequests[e].size() && theShown < 4; k++) {
                    const EECPUMRequest& theRequest = theUM->mRequests[e][k];
                    long theClass = (long)theRequest.mUnitClass;
                    if (theClass == kUnitTypeCitizen || theClass == kUnitTypeFemaleCitizen || gUnitClassTable[theClass] == NULL)
                        continue;
                    EETechTreeNode* theNode = inPlayer->mTechnologies->Get(gUnitClassTable[theClass]->mTechnology);
                    long theProducers = 0;
                    if (theNode != NULL)
                        for (unsigned long u = 0; u < theNode->GetNumUnitClasses(); u++) {
                            std::deque<EEComplexUnit*>* theBuilt = theUM->GetUnits(theNode->GetUnitClass(u));
                            if (theBuilt != NULL && gUnitClassTable[theNode->GetUnitClass(u)]->IsBuilding())
                                for (unsigned long b = 0; b < theBuilt->size(); b++)
                                    theProducers += (*theBuilt)[b]->mIsBuilt ? 1 : 0;
                        }
                    int theAfford = theNode != NULL && inPlayer->CanAfford(theNode, theClass, 1) ? 1 : 0;
                    int n = snprintf(theList + theUsed, sizeof(theList) - theUsed, " %ld:%lu:%d:%ld", theClass,
                        (unsigned long)theRequest.mCount, theAfford, theProducers);
                    if (n > 0 && theUsed + n < sizeof(theList))
                        theUsed += n;
                    theShown++;
                }
            long theRoom = inPlayer->GetPopulationLimit(0) - theUM->mNumInProduction - inPlayer->GetTotalPopulation(true, false);
            EECPTraceOut(inPlayer, "pending", "room=%ld cls:n:afford:producers%s", theRoom, theList);
        }
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

// TEST ONLY (EE-SIM's --dump-at / --dump-load): the parts of EECPOutOfSynchChecksum per player and the computer
// player's own timers, one line each, so two dumps name the computer player and the part that differ
void EECPOutOfSynchDump(FILE* inFile)
{
    {   // the continent manager's areas (serial/kind), its serial counter and its lists' sizes
        fprintf(inFile, "AREAS digest=%08lx last=%lu continents=%zu hills=%zu pathareas=%zu sectors=%zu subsectors=%zu chokepoints=%zu forests=%zu areas=%zu:",
                EEContinentRegionsDigest(), (unsigned long)gContinentManager.mLastSerial, gContinentManager.mContinents.size(), gContinentManager.mHills.size(),
                gContinentManager.mPathAreas.size(), gContinentManager.mPathSectors.size(), gContinentManager.mPathSubSectors.size(),
                gContinentManager.mChokePoints.size(), gContinentManager.mForests.size(), gContinentManager.mAreas.size());
        for (std::map<unsigned long, EEAIArea*>::const_iterator theIt = gContinentManager.mAreas.begin();
             theIt != gContinentManager.mAreas.end(); ++theIt)
            fprintf(inFile, " %lu/%ld", (unsigned long)theIt->first, theIt->second != NULL ? (long)theIt->second->mAreaType : -1L);
        fprintf(inFile, "\n");
    }
    for (unsigned long i = 0; i < gWorld.mNumPlayers; i++) {
        EEPlayer* thePlayer = gWorld.mPlayers[i];
        if (thePlayer == NULL)
            continue;
        fprintf(inFile, "AI %lu res=", thePlayer->mPlayerID);
        for (long r = kResourceFood; r <= kResourceIron; r++)
            fprintf(inFile, "%s%ld", r != kResourceFood ? "," : "", (long)thePlayer->mResourceCaps[r]);
        EEComputerPlayer* theCP = thePlayer->mComputerPlayer;
        if (theCP != NULL) {
            fprintf(inFile, " popcap=%ld", (long)theCP->mPopulationCap);
            if (EECPCivilizationManager* theCiv = theCP->mCivilizationManager) {
                fprintf(inFile, " epochadv=%lu", (unsigned long)theCiv->mNextEpochAdvanceTime);
                // the epoch advance's guards (AttemptEpochAdvance's caller): advancing, scheduled, requested, the
                // later-epoch requests of the build, research and unit managers, behind a human
                fprintf(inFile, " advflags=%d%d%d", theCiv->mAdvancingEpoch ? 1 : 0, theCiv->mScheduleEpochAdvance ? 1 : 0,
                        theCiv->mEpochResearchRequested ? 1 : 0);
                if (theCiv->mBuildManager != NULL && theCiv->mResearchManager != NULL && theCiv->mUnitManager != NULL)
                    fprintf(inFile, " later=%d%d%d%d", theCiv->mBuildManager->HasLaterEpochRequests() ? 1 : 0,
                            theCiv->mResearchManager->HasLaterEpochIDs() ? 1 : 0,
                            theCiv->mUnitManager->HasLaterEpochRequests() ? 1 : 0, theCiv->IsBehindInEpoch() ? 1 : 0);
                if (theCiv->mResearchManager != NULL) {   // the research queues up to the manager's epoch
                    fprintf(inFile, " rq=");
                    for (long e = 0; e <= theCiv->mResearchManager->mEpoch && e < EEEpochCount(); e++) {
                        fprintf(inFile, "%s", e ? "|" : "");
                        for (size_t k = 0; k < theCiv->mResearchManager->mEpochIDs[e].size(); k++)
                            fprintf(inFile, "%s%ld", k ? "," : "", theCiv->mResearchManager->mEpochIDs[e][k]);
                    }
                }
                if (theCiv->mBuildManager != NULL) {
                    fprintf(inFile, " queues=");
                    for (long e = 0; e < EEEpochCount(); e++)
                        fprintf(inFile, "%s%lu", e ? "," : "", (unsigned long)theCiv->mBuildManager->mQueues[e].size());
                }
                if (theCiv->mUnitManager != NULL)
                    fprintf(inFile, " waiting=%lu", (unsigned long)theCiv->mUnitManager->mNumWaiting);
                if (theCiv->mResourceManager != NULL) {   // each resource's gatherers: filed/wanted
                    fprintf(inFile, " gath=");
                    for (int r = 0; r < 7; r++)
                        fprintf(inFile, "%s%zu/%lu", r ? "," : "", theCiv->mResourceManager->mGatherers[r].size(),
                                (unsigned long)theCiv->mResourceManager->mWantedGatherers[r]);
                    fprintf(inFile, " cap=");   // GetGatherCapacity's parts and the resource centers (active, units)
                    for (long r = 2; r < 7; r++)
                        fprintf(inFile, "%s%lu", r > 2 ? "," : "", (unsigned long)theCiv->mResourceManager->GetGatherCapacity(r));
                    EECPRMDumpFoodCapacity(theCiv->mResourceManager, inFile);
                    fprintf(inFile, " rc=");
                    for (unsigned long k = 0; k < theCP->GetNumResourceCenters(); k++) {
                        EECPResourceCenter* theCenter = theCP->GetResourceCenter(k);
                        fprintf(inFile, "%s%lu:%d:%zu", k ? "," : "", theCenter->GetUnit() != NULL ? theCenter->GetUnit()->mUnitID : 0UL,
                                theCenter->mIsActive ? 1 : 0, theCenter->mUnits.size());
                    }
                }
                if (theCiv->mResourceManager != NULL)   // the claim range and the danger zones (save 0.265)
                    fprintf(inFile, " claim=%ld/%lu danger=%zu", EECPClaimRange(theCP), (unsigned long)sClaimLast[theCP->mPlayerID < 64 ? theCP->mPlayerID : 63],
                            theCiv->mResourceManager->mDangerZones.size());
                if (theCiv->mUnitManager != NULL && theCiv->mResourceManager != NULL)   // the demand's first tier's inputs
                    fprintf(inFile, " wantcit=%ld workers=%lu gathercap=%lu",
                            (long)theCiv->mUnitManager->GetWantedCitizens(), (unsigned long)theCiv->mUnitManager->GetNumWorkers(),
                            (unsigned long)theCiv->mResourceManager->GetGatherCapacity());
            }
            if (theCP->mCombatManager != NULL && theCP->mCombatManager->mAttackCommander != NULL)
                fprintf(inFile, " attackgroups=%lu", (unsigned long)theCP->mCombatManager->mAttackCommander->mGroups.size());
            fprintf(inFile, " started=%d enableset=%d regroup=%d nextenable=%lu nextpop=%ld nextexp=%lu nextattack=%lu lastdamaged=%ld",
                    theCP->mIsStarted ? 1 : 0, theCP->mIsEnableTimeSet ? 1 : 0, theCP->mRegroupPending ? 1 : 0,
                    (unsigned long)theCP->mNextEnableTime, (long)theCP->mNextPopulationTime, (unsigned long)theCP->mNextExpansionTime,
                    (unsigned long)theCP->mNextAttackTime, (long)theCP->mLastDamagedTime);
            // EnableManagers' other inputs
            fprintf(inFile, " combat=%lu damaged=%d/%d attacks=%ld", (unsigned long)theCP->mCombatState,
                    theCP->mBuildingDamaged ? 1 : 0, theCP->mCitizenDamaged ? 1 : 0, (long)theCP->mNumAttacks);
            {   // the managers' timers (save 0.269)
                unsigned long theTimers[kEECPManagerTimers];
                theCP->GetManagerTimers(theTimers);
                fprintf(inFile, " timers=");
                for (long t = 0; t < kEECPManagerTimers; t++)
                    fprintf(inFile, "%s%lu", t ? "," : "", theTimers[t]);
            }
        }
        fprintf(inFile, "\n");
        // TEST ONLY (EE-SIM's state dumps): each attack sub group's state, timers, targets and path request
        if (theCP != NULL && theCP->mCombatManager != NULL && theCP->mCombatManager->mAttackCommander != NULL) {
            EECPAttackCommander* theCommander = theCP->mCombatManager->mAttackCommander;
            for (size_t g = 0; g < theCommander->mGroups.size(); g++) {
                EECPAttackGroup* theGroup = theCommander->mGroups[g];
                for (size_t s = 0; theGroup != NULL && s < theGroup->mSubGroups.size(); s++) {
                    EECPAttackSubGroup* theSub = theGroup->mSubGroups[s];
                    fprintf(inFile, "ASG %lu g%zu s%zu id=%lu kind=%ld state=%lu scan=%lu upd=%lu b0=%lu b4=%lu follow=%lu bc=%lu task=%lu pos=%lu gather=%lu splits=%ld split=%ld units=%zu path=%zu u08=%zu u14=%zu ae=%d shore=%d req=%s targets=",
                            (unsigned long)thePlayer->mPlayerID, g, s, (unsigned long)theSub->mID, (long)theSub->mKind,
                            (unsigned long)theSub->mState, (unsigned long)theSub->mNextScanTime,
                            (unsigned long)theSub->mNextUpdateTime, (unsigned long)theSub->mU1B0, (unsigned long)theSub->mU1B4,
                            (unsigned long)theSub->mNextFollowTime, (unsigned long)theSub->mU1BC,
                            (unsigned long)theSub->mNextTaskCheckTime, (unsigned long)theSub->mNextPositionTime,
                            (unsigned long)theSub->mGatherDeadline, (long)theSub->mNumSplits, (long)theSub->mSplitKind,
                            theSub->mUnits.size(), theSub->mPath.size(), theSub->mU08.size(), theSub->mU14.size(),
                            theSub->mHasAreaEffect ? 1 : 0, theSub->mCanAttackShore ? 1 : 0,
                            theSub->mPathRequest == NULL ? "none" : (theSub->mPathRequest->mIsDone ? "done" : "busy"));
                    for (size_t t = 0; t < theSub->mTargets.size(); t++)
                        fprintf(inFile, "%ld/%ld/%ld,", (long)theSub->mTargets[t]->mUnitID,
                                (long)theSub->mTargets[t]->mNumNearbyTargets, (long)theSub->mTargets[t]->mNumAttackers);
                    fprintf(inFile, " siege=");
                    for (size_t t = 0; t < theSub->mSiegeTargets.size(); t++)
                        fprintf(inFile, "%ld,", (long)theSub->mSiegeTargets[t]->mUnitID);
                    fprintf(inFile, " task=%ld/%d/%ld,%ld/%lu/%ld nwp=%ld,%ld",
                            (long)theSub->mTask.mType, theSub->mTask.mIsActive ? 1 : 0,
                            (long)theSub->mTask.mLocation.mTile.mX, (long)theSub->mTask.mLocation.mTile.mY,
                            theSub->mTask.mUnit != NULL ? (unsigned long)theSub->mTask.mUnit->mUnitID : 0UL,
                            theSub->mTask.mPlayer != NULL ? (long)theSub->mTask.mPlayer->mPlayerID : -1L,
                            (long)theSub->mNextWaypoint.mTile.mX, (long)theSub->mNextWaypoint.mTile.mY);
                    fprintf(inFile, " area=%ld/%lu/%d", (long)theSub->mTask.mLocation.mType,
                            (unsigned long)theSub->mTask.mLocation.mSerial,
                            theSub->mTask.mLocation.mSerial != 0 && gContinentManager.GetArea(theSub->mTask.mLocation.mSerial) != NULL ? 1 : 0);
                    {   // what a refresh (EECPILocation::Update, on a copy) would make of the task's area; its player
                        EECPILocation theCopy(theSub->mTask.mLocation);
                        theCopy.Update();
                        fprintf(inFile, " refresh=%ld/%lu player=%lu", (long)theCopy.mType, (unsigned long)theCopy.mSerial,
                                (unsigned long)theSub->mTask.mLocation.mPlayer);
                    }
                    fprintf(inFile, "\n");
                }
            }
        }
    }
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
        gPhysics->mInputMessagesProcessed, EECPOutOfSynchChecksum());
}
