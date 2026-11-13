// New (port): the game log (ee/EEGameLog.h). Reads the world on the physics thread; writes nothing back.
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <cstdarg>
#include <cerrno>
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
#include "ee/EEUGRust.h"
#include "ee/EEUnitAction.h"
#include <math.h>
#include <mutex>
#include <ctime>
#include "platform/PlatPath.h"
#include "platform/PlatFile.h"
#include "platform/PlatSim.h"
#include "ee/EEGamePreferences.h"
#include "ee/EECPTrace.h"
#include "ee/EECPEngine.h"
#include "ee/EEGameLog.h"
#include "ee/EEColorTable.h"

namespace {

const long kMaxPlayers = 64;
const unsigned long kSampleMs = 30000;

std::mutex sLock;
FILE* sFile = NULL;
unsigned long sLastTime = 0;
bool sStarted = false;
bool sEnded = false;
unsigned long sNextSample = 0;
long sEpoch[kMaxPlayers];
long sIncome[kMaxPlayers][7];
long sLost[kMaxPlayers][3];    // citizens, army, buildings since the last sample
long sLostBy[kMaxPlayers][3];
std::string sCiv[kMaxPlayers];   // the civilization name last written  // all losses by the killer: a human's unit, a computer player's, none or wild

bool IsBuilding(EEUnit* inUnit)
{
    long c = inUnit->mUnitClass->mCategory;
    return inUnit->mUnitClass->IsLandBuilding() || c == kUnitCategoryNavalBuilding || c == kUnitCategoryMiningBuilding
        || c == kUnitCategoryGate || c == kUnitCategoryFortress || c == kUnitCategoryBuildingPriest
        || c == kUnitCategorySpaceBuilding;
}

bool IsCitizen(EEUnit* inUnit)
{
    long c = inUnit->mUnitClass->mCategory;
    return c == kUnitCategoryLandCitizen || c == kUnitCategoryNavalCitizen;
}

bool IsCounted(EEUnit* inUnit)   // a unit of the player's (not an AI path point, a resource or a shot)
{
    long c = inUnit->mUnitClass->mCategory;
    return c != kUnitCategoryAmbient && c != kUnitCategoryResource && c != kUnitCategoryProjectile;
}

void Write(const char* inFormat, ...)
{
    if (sFile == NULL)
        return;
    va_list theArguments;
    va_start(theArguments, inFormat);
    vfprintf(sFile, inFormat, theArguments);
    va_end(theArguments);
    fflush(sFile);
}

std::string Narrow(const UWideString& inText)
{
    std::string s = (const char*)inText;
    for (char& c : s)
        if (c == ' ' || c == '\t' || c == '=' || c == '\n' || c == '\r')
            c = '_';
    return s.empty() ? "-" : s;
}

void Close()
{
    if (sFile != NULL) {
        if (!sEnded)
            Write("EE-GAME %lu p0 end\n", sLastTime);
        fclose(sFile);
        sFile = NULL;
    }
}

void Open()
{
    Close();
    if (!gGamePreferences.GetSettings().mGameLog || gPlatSimMode)
        return;   // off, or the headless simulation (its own report and trace)
    char theRoot[1024];
    if (PlatPathGetDataRoot(theRoot, sizeof(theRoot)) == 0)
        return;
    std::string theDir = std::string(theRoot) + "/Logs";
    PlatCreateDirectory(theDir.c_str());
    time_t theNow = time(NULL);
    struct tm theLocal;
#ifdef _WIN32
    localtime_s(&theLocal, &theNow);
#else
    localtime_r(&theNow, &theLocal);
#endif
    char theName[64];
    strftime(theName, sizeof(theName), "/game-%Y%m%d-%H%M%S.log", &theLocal);
    sFile = PlatFOpen((theDir + theName).c_str(), "w");   // the data root is in the game's C:\ form on POSIX too
    if (sFile == NULL) {
        fprintf(stderr, "EE-GAME: cannot write the game log %s%s: %s\n", theDir.c_str(), theName, strerror(errno));
        return;
    }
    fprintf(stderr, "EE-GAME: the game log is %s%s\n", theDir.c_str(), theName);
    sEnded = false;
    memset(sEpoch, -1, sizeof(sEpoch));
    memset(sIncome, 0, sizeof(sIncome));
    memset(sLost, 0, sizeof(sLost));
    memset(sLostBy, 0, sizeof(sLostBy));
    sNextSample = 0;
    const EEGameOptions& theOptions = gWorld.mLobbyOptions;
    Write("EE-GAME 0 p0 start map=%s size=%lu difficulty=%lu players=%lu\n", Narrow(theOptions.GetMapType()).c_str(),
          (unsigned long)theOptions.GetMapSize(), (unsigned long)theOptions.GetDifficulty(),
          (unsigned long)gWorld.mNumPlayers);
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        EEPlayer* p = gWorld.mPlayers[i];
        if (p == NULL)
            continue;
        EEComputerPlayer* ai = p->mComputerPlayer;
        // color: the dbcolortable row (tools/game_log.py names it from the data); rgb its colour
        Write("EE-GAME 0 p%lu player kind=%s name=%s color=%lu rgb=%.2f/%.2f/%.2f civ=%s personality=%s cheats=%s engine=%s\n", i,
              p->IsHuman() ? "human" : ai != NULL ? "computer" : "other", Narrow(p->mName).c_str(),
              (unsigned long)EEColorTable::FindIndex(p->mColor), p->mColor.mRed, p->mColor.mGreen, p->mColor.mBlue,
              Narrow(p->mCivilization.GetName()).c_str(), ai != NULL ? EECPPersonality(ai).mID.c_str() : "-",
              ai != NULL ? EECPCheatNames(ai) : "-", ai != NULL ? EECPEngineName(ai).c_str() : "-");
        sCiv[i] = Narrow(p->mCivilization.GetName());
    }
    // the computer players' decision trace into the log too (EECPTraceOut; the sim hash stays EE_AI_TRACE's)
    gCPTraceOn = true;
}

void Sample(unsigned long inTime)
{
    for (unsigned long i = 1; i < gWorld.mNumPlayers && i < (unsigned long)kMaxPlayers; i++) {
        EEPlayer* p = gWorld.mPlayers[i];
        if (p == NULL)
            continue;
        std::string theCiv = Narrow(p->mCivilization.GetName());
        if (theCiv != sCiv[i]) {   // chosen after the start (the civilization screen opens in the game)
            Write("EE-GAME %lu p%lu civ name=%s\n", inTime, i, theCiv.c_str());
            sCiv[i] = theCiv;
        }
        long theCitizens = 0, theIdle = 0, theBuilders = 0, theArmy = 0, theBuildings = 0, theUnfinished = 0, theTowers = 0;
        long theBoats = 0;   // the citizens that are fishing (naval citizen) boats
        long theGather[7] = {0, 0, 0, 0, 0, 0, 0};
        for (EEUnit* u = p->mFirstUnit; u != NULL; u = static_cast<EEUnit*>(u->mNext)) {
            if (u->mComplexUnit == NULL || !(u->mHitPoints > 0) || !IsCounted(u))
                continue;
            if (IsCitizen(u)) {
                theCitizens++;
                if (u->mUnitClass->mCategory == kUnitCategoryNavalCitizen)
                    theBoats++;
                EEUnitGoal* g = u->mComplexUnit->mActiveGoal;
                if (g == NULL)
                    theIdle++;
                else if (g->mKind == kGoalGather) {
                    EEUnit* r = static_cast<EEUGGather*>(g)->mResource;
                    unsigned long t = r != NULL ? r->mResourceType : 0;
                    theGather[t < 7 ? t : 0]++;
                } else if (g->mKind == kGoalBuildRepair)
                    theBuilders++;
            } else if (IsBuilding(u)) {
                theBuildings++;
                if (!u->mComplexUnit->mIsBuilt)
                    theUnfinished++;
                else if (u->mUnitClass->mIsTower)
                    theTowers++;
            } else
                theArmy++;
        }
        const long* s = p->mResourceCaps;
        long* in = sIncome[i];
        long* lost = sLost[i];
        long* by = sLostBy[i];
        Write("EE-GAME %lu p%lu sample epoch=%ld pop=%ld citizens=%ld boats=%ld idle=%ld gather=%ld/%ld/%ld/%ld/%ld build=%ld army=%ld "
              "buildings=%ld unfinished=%ld towers=%ld stock=%ld/%ld/%ld/%ld/%ld got=%ld/%ld/%ld/%ld/%ld "
              "lost=%ld/%ld/%ld lost_by=%ld/%ld/%ld\n",
              inTime, i, p->mTechnologies != NULL ? p->mTechnologies->mCurrentEpoch : -1L, (long)p->mPopulationCount,
              theCitizens, theBoats, theIdle, theGather[kResourceFood], theGather[kResourceWood], theGather[kResourceStone],
              theGather[kResourceGold], theGather[kResourceIron], theBuilders, theArmy, theBuildings, theUnfinished, theTowers,
              s[kResourceFood], s[kResourceWood], s[kResourceStone], s[kResourceGold], s[kResourceIron],
              in[kResourceFood], in[kResourceWood], in[kResourceStone], in[kResourceGold], in[kResourceIron],
              lost[0], lost[1], lost[2], by[0], by[1], by[2]);
        memset(in, 0, sizeof(sIncome[i]));
        memset(lost, 0, sizeof(sLost[i]));
        memset(by, 0, sizeof(sLostBy[i]));
    }
}

}  // namespace

bool EEGameLogOn()
{
    return sFile != NULL;
}

void EEGameLogTick(unsigned long inGameTime)
{
    std::lock_guard<std::mutex> theGuard(sLock);
    if (!sStarted || inGameTime + 1000 < sLastTime) {   // the first game, or a new one (the clock went back)
        Open();
        sStarted = true;
    }
    sLastTime = inGameTime;
    if (sFile == NULL)
        return;
    for (unsigned long i = 1; i < gWorld.mNumPlayers && i < (unsigned long)kMaxPlayers; i++) {
        EEPlayer* p = gWorld.mPlayers[i];
        long e = p != NULL && p->mTechnologies != NULL ? p->mTechnologies->mCurrentEpoch : -1;
        if (e > sEpoch[i]) {
            if (sEpoch[i] >= 0)
                Write("EE-GAME %lu p%lu epoch to=%ld\n", inGameTime, i, e);
            sEpoch[i] = e;
        }
    }
    if (gPhysics->mGameOver && !sEnded) {   // the game's result: a last sample and the end line
        Sample(inGameTime);
        Write("EE-GAME %lu p0 end result=over\n", inGameTime);
        sEnded = true;
        return;
    }
    if (sEnded)
        return;
    if (inGameTime >= sNextSample) {
        Sample(inGameTime);
        sNextSample = inGameTime - inGameTime % kSampleMs + kSampleMs;
    }
}

void EEGameLogLine(const char* inText)
{
    std::lock_guard<std::mutex> theGuard(sLock);
    if (sFile != NULL) {
        fputs(inText, sFile);
        fflush(sFile);
    }
}

void EEGameLogIncome(EEPlayer* inPlayer, long inResource, long inAmount)
{
    if (sFile == NULL || inPlayer == NULL || inAmount <= 0 || inResource < 0 || inResource > 6)
        return;
    unsigned long theID = inPlayer->mPlayerID;
    if (theID < (unsigned long)kMaxPlayers)
        sIncome[theID][inResource] += inAmount;
}

void EEGameLogChat(unsigned long inPlayer, const char* inText)
{
    std::lock_guard<std::mutex> theGuard(sLock);
    if (sFile == NULL || inText == NULL)
        return;
    std::string theText(inText);
    for (char& c : theText)
        if (c == '\n' || c == '\r')
            c = ' ';
    Write("EE-GAME %lu p%lu chat text=%s\n", gPhysics != NULL ? gPhysics->mGameTime : 0UL, inPlayer, theText.c_str());
}

void EEGameLogUnitLost(EEPlayer* inPlayer, EEUnit* inUnit)
{
    if (sFile == NULL || inPlayer == NULL || inUnit == NULL || inUnit->mUnitClass == NULL || inUnit->mComplexUnit == NULL)
        return;
    unsigned long theID = inPlayer->mPlayerID;
    if (theID == 0 || theID >= (unsigned long)kMaxPlayers || !IsCounted(inUnit))
        return;
    sLost[theID][IsCitizen(inUnit) ? 0 : IsBuilding(inUnit) ? 2 : 1]++;
    EEComplexUnit* theKiller = inUnit->mComplexUnit->mLastAttacker;
    EEPlayer* theOwner = theKiller != NULL ? theKiller->mPlayer : NULL;
    sLostBy[theID][theOwner == NULL || theOwner->mPlayerID == 0 ? 2 : theOwner->IsHuman() ? 0 : 1]++;
}
