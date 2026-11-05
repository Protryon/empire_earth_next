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
#include "ee/EEUnitGoal.h"
#include "ee/EEUGGather.h"
#include "ee/EECPTrace.h"

bool gCPTraceOn = false;
const char* gCPTraceManager = "-";

static bool sInitialized = false;
static long sCheats = kCPCheatAll;
static char sCheatNames[128];
static const struct { const char* mName; long mBit; } kCheatNames[] = {
    {"cost", kCPCheatCost}, {"reveal", kCPCheatReveal}, {"epochs", kCPCheatEpochs}, {"extra", kCPCheatExtra},
    {"popband", kCPCheatPopBand}, {"catchup", kCPCheatCatchUp}, {"retire", kCPCheatRetire},
};
static const long kMaxTracedPlayers = 64;
static unsigned long sNextStats[kMaxTracedPlayers];
static unsigned long sNextProfile[kMaxTracedPlayers];
static long long sProfileSum[kMaxTracedPlayers];
static long long sProfileMax[kMaxTracedPlayers];
static unsigned long sProfileCount[kMaxTracedPlayers];

void EECPTraceInit()
{
    if (sInitialized)
        return;
    sInitialized = true;
    const char* theTrace = getenv("EE_AI_TRACE");
    gCPTraceOn = theTrace != NULL && theTrace[0] != '\0' && strcmp(theTrace, "0") != 0;
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
            bool theKnown = theName == "none" || theName.empty();
            for (unsigned long i = 0; i < sizeof(kCheatNames) / sizeof(kCheatNames[0]); i++)
                if (theName == kCheatNames[i].mName) {
                    sCheats |= kCheatNames[i].mBit;
                    theKnown = true;
                }
            if (!theKnown)
                ESDebugOut("EE-AI EE_AI_CHEATS: unknown cheat '%s' ignored\n", theName.c_str());
            theStart = theEnd + 1;
        }
    }
    sCheatNames[0] = '\0';
    for (unsigned long i = 0; i < sizeof(kCheatNames) / sizeof(kCheatNames[0]); i++)
        if (sCheats & kCheatNames[i].mBit) {
            if (sCheatNames[0] != '\0')
                strcat(sCheatNames, ",");
            strcat(sCheatNames, kCheatNames[i].mName);
        }
    if (sCheatNames[0] == '\0')
        strcpy(sCheatNames, "none");
}

bool EECPCheatOn(long inCheat)
{
    EECPTraceInit();
    return (sCheats & inCheat) != 0;
}

const char* EECPCheatNames()
{
    EECPTraceInit();
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
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->mComplexUnit == NULL || !(theUnit->mHitPoints > 0))
            continue;
        long theCategory = theUnit->mUnitClass->mCategory;
        if (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen) {
            theCitizens++;
            EEUnitGoal* theGoal = theUnit->mComplexUnit->mActiveGoal;
            if (theGoal == NULL)
                theIdle++;
            else if (theGoal->mKind == kGoalGather) {
                EEUnit* theResource = static_cast<EEUGGather*>(theGoal)->mResource;
                unsigned long theType = theResource != NULL ? theResource->mResourceType : 0;
                theGatherers[theType >= kResourceFood && theType <= kResourceIron ? theType : 0]++;
            }
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
    EECPTraceOut(inPlayer, "stats", "epoch=%ld pop=%ld cap=%ld limit=%ld citizens=%ld idle=%ld free=%lu gather=%ld/%ld/%ld/%ld/%ld/%ld "
        "buildings=%ld unfinished=%ld other=%ld",
        theEpoch, inPlayer->GetTotalPopulation(true, true), inPlayer->mPopulationCap,
        inPlayer->EEPlayer::GetPopulationLimit(0), theCitizens, theIdle, (unsigned long)theFree.size(),
        theGatherers[kResourceFood], theGatherers[kResourceWood], theGatherers[kResourceStone],
        theGatherers[kResourceGold], theGatherers[kResourceIron], theGatherers[0], theBuildings, theUnfinished, theOther);
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
