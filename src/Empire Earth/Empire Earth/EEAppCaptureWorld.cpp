// New (port), TEST ONLY: the app and world lane's records of the map, the mini map, the cheat codes and the storms
// (ee/EEAppCapture.h; tools/sim_capture.py). Reads the objects; the sweeps (EE-SIM --app-sweep) play cheat lines and
// start storms as a player would.
#include <windows.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <list>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/EEStatisticsRecord.h"
#include "ee/EEPhysics.h"
#include "ee/EECheatCodes.h"
#include "ee/EEEMStorm.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEStormType.h"
#include "rust/render/ge.h"
#include "rust/game/app.h"   // the map records
#include "rust/sim/units.h"
#include "ee/EETechCapture.h"
#include "ee/EEAppCapture.h"

// MAPS, MAPC: Rust (ee_game app map_capture.rs)
static void EEAppCaptureSightImpl(unsigned int inKind, EEPlayer* inPlayer, float inX, float inY, float inRadius,
                                  bool inShared)
{
    if (EESimCaptureRecord::Active() && gWorld.mMap != NULL)
        ee_app_capture_sight(inKind, inPlayer != NULL ? (uint32_t)inPlayer->mPlayerID : 0xffffffffu, inX, inY, inRadius,
                             inShared, gWorld.mMap->mTerrainMesh.GetRust());
}

// EEAppMapScope's construction (whether it is the outermost) and destruction (the record)
static bool AppMapScopeEnter()
{
    return ee_app_capture_map_enter();
}

static void AppMapScopeLeave(EEMap* inMap, bool inOuter, unsigned int inKind, const long* inArgs, const long* inRect)
{
    ee_app_capture_map_leave(inOuter, (uint64_t)(uintptr_t)inMap, inMap != NULL ? inMap->mTerrainMesh.GetRust() : NULL,
                             inKind, inArgs, inRect);
}

static void EEAppCaptureStormImpl(const EEEMStorm& inStorm, unsigned int inPhase, unsigned long inTime)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord r(EECaptureTag("STRM"));
    r.U32(inPhase);
    r.U32((unsigned int)inTime);
    r.I32((int)inStorm.mType);
    r.I32((int)inStorm.mState);
    r.F32(inStorm.mTargetIntensity);
    r.F32(inStorm.mIntensity);
    r.F32(inStorm.mStep);
    r.U32((unsigned int)inStorm.mNextMoveTime);
    r.U32((unsigned int)inStorm.mEndTime);
    r.U32(inStorm.mIsSevere ? 1 : 0);
    r.I32((int)inStorm.mStart.mX);
    r.I32((int)inStorm.mStart.mY);
    r.I32((int)inStorm.mGoal.mX);
    r.I32((int)inStorm.mGoal.mY);
    r.I32((int)inStorm.mArea.mTop);
    r.I32((int)inStorm.mArea.mLeft);
    r.I32((int)inStorm.mArea.mBottom);
    r.I32((int)inStorm.mArea.mRight);
    r.U32((unsigned int)inStorm.mPath.size());
    for (const U2DPoint<long>& thePoint : inStorm.mPath) {
        r.I32((int)thePoint.mX);
        r.I32((int)thePoint.mY);
    }
    r.U32(inStorm.mRequest != NULL ? 1 : 0);
    if (inStorm.mStorm != NULL) {
        U2DPoint<long> theCenter = inStorm.mStorm->GetCenterPoint();
        r.I32((int)theCenter.mX);
        r.I32((int)theCenter.mY);
        r.U32((unsigned int)inStorm.mStorm->GetTileCount());
    } else
        r.U32(0xffffffffu);
    r.Commit();
}

// the cheat sweep's C++ calls (CHET, CHEF: ee_game app cheat_capture.rs)
static void EEAppCaptureCheatSweepImpl()
{
    if (!EESimCaptureRecord::Active() || gCheatCodes == NULL)
        return;
    EeAppCheatSweepHost theHost = {
        NULL,
        [](void*, const char* inLine, long* outCode, long* outParameter) {
            gCheatCodes->ParseCheatCode(UWideString(inLine), *outCode, *outParameter);
        },
        [](void*, long inCode, long inPlayer, long inParameter) { gCheatCodes->ApplyCheatCode(inCode, inPlayer, inParameter); },
        [](void*, bool inOn) {
            bool theOld = gWorld.mCheatsEnabled;
            gWorld.mCheatsEnabled = inOn;
            return theOld;
        },
        [](void*, uint64_t inPlayer, uint32_t inIndex, int32_t* outLevels) -> int32_t {
            EEPlayer* thePlayer = reinterpret_cast<EEPlayer*>((uintptr_t)inPlayer);
            if (inIndex >= thePlayer->mUnitClassUpgrades.size())
                return -1;
            EEUnitClassUpgrade* theUpgrade = thePlayer->mUnitClassUpgrades[inIndex];
            if (theUpgrade == NULL)
                return 0;
            for (unsigned long j = 0; j < 0x10; j++)
                outLevels[j] = (int32_t)theUpgrade->mImprovement.GetLevel(j);
            return 1;
        },
        [](void*, float* outPoint) {
            GE3DPoint theCamera;
            gWorld.mCameraControl.GetLocation(gWorld.mCameraControl.mFraction, theCamera);
            outPoint[0] = theCamera.mX;
            outPoint[1] = theCamera.mY;
            outPoint[2] = theCamera.mZ;
        },
    };
    ee_app_capture_cheat_sweep(&theHost);
}

static void EEAppCaptureStormSweepImpl()
{
    EEEnvironmentManager* theManager = gWorld.mEnvironmentManager;
    if (!EESimCaptureRecord::Active() || theManager == NULL)
        return;
    theManager->SetEnabled(true);
    const long kTypes[] = {kStormRain, kStormSnow, kStormHurricane, kStormBlizzard};
    for (long theType : kTypes) {
        EEEMStorm* theStorm = new EEEMStorm(theType, theManager);
        EEAppCaptureStorm(*theStorm, 0, 0);
        theManager->mStorms.push_back(theStorm);
    }
}

namespace {

// a wide string's bytes, counted (UWideString holds UTF-8 bytes: D6)
void Text(EESimCaptureRecord& ioRecord, const UWideString& inText)
{
    const char* theBytes = (const char*)inText;
    unsigned int n = theBytes != NULL ? (unsigned int)strlen(theBytes) : 0;
    ioRecord.U32(n);
    ioRecord.Bytes(theBytes, n);
}

// a player's statistics: each series' largest and last value, its count of increases and of points, the end time
void Statistics(EESimCaptureRecord& ioRecord, EEPlayerStatistics& inStatistics)
{
    for (unsigned long s = 0; s < 31; s++) {
        ioRecord.U32((unsigned int)inStatistics.GetValue(s));
        ioRecord.U32((unsigned int)inStatistics.GetLastValue(s));
        ioRecord.U32((unsigned int)inStatistics.CountIncreases(s));
        ioRecord.U32((unsigned int)inStatistics.GetSeries(s).mPoints.size());
    }
    ioRecord.U32((unsigned int)inStatistics.mEndTime);
}

}   // namespace

// WSTA (the app profile's end, after EEMap's sight sweep): EEWorld::RecordStatistics, the end-of-game scores (EEWorld::Clear
// runs it when a game is left: no capture game reaches it). One record: the history's fields (but the start time, a wall clock), every record (scores,
// colour, name and suffix, slot, result, end time, team, resources, flags, statistics), then every player's resources
// and statistics after it (the top-ups of the computers' gathered totals) and the physics random seed (its draws)
static void EEAppCaptureStatisticsSweepImpl()
{
    if (!EESimCaptureRecord::Active() || gWorld.mMap == NULL)
        return;
    gWorld.RecordStatistics();
    EEStatisticsHistory& h = gWorld.mStatistics;
    EESimCaptureRecord r(EECaptureTag("WSTA"));
    // (not mStartTime: the wall clock at the game's start)
    r.I32((int)h.mCurrentRecordIndex);
    r.I32((int)h.mEndTime);
    r.I32((int)h.mTileCount);
    r.U32((h.mTeamsLocked ? 1u : 0u) | (h.mVictoryEnabled ? 2u : 0u) | (h.mIsScenario ? 4u : 0u) | (h.mIsCampaign ? 8u : 0u));
    Text(r, h.mCampaignName);
    r.U32((unsigned int)h.mRecords.size());
    for (EEStatisticsRecord& e : h.mRecords) {
        r.U32((unsigned int)e.mScore);
        r.U32((unsigned int)e.mEconomyScore);
        r.U32((unsigned int)e.mEpochScore);
        r.U32((unsigned int)e.mMilitaryScore);
        r.U32((unsigned int)e.mTechScore);
        r.Bytes(&e.mColor, sizeof(e.mColor));
        Text(r, e.mName);
        Text(r, e.mNameSuffix);
        r.U32(e.mSlot);
        r.I32((int)e.mResult);
        r.U32((unsigned int)e.mEndTime);
        r.U32((unsigned int)e.mTeam);
        for (unsigned long k = 2; k < 7; k++)
            r.U32((unsigned int)e.mResources[k]);
        r.U32((e.mIsLocal ? 1u : 0u) | (e.mIsHuman ? 2u : 0u));
        Statistics(r, e.mStatistics);
    }
    r.U32((unsigned int)gWorld.mNumPlayers);
    for (unsigned long i = 0; i < gWorld.mNumPlayers; i++) {
        EEPlayer* p = gWorld.mPlayers[i];
        if (p == NULL) {
            r.U32(0);
            continue;
        }
        r.U32(1);
        for (unsigned long k = 2; k < 7; k++)
            r.I32((int)p->mResourceCaps[k]);
        Statistics(r, p->mSavedGroups);
    }
    r.U32((unsigned int)gPhysics->mRandom.GetSeed());
    r.Commit();
}

// The game calls these through the header's hook pointers, set here before main: the game's units name no
// symbol of this TEST ONLY unit (tools/cpp_deps.py cuts; PORT_LEARNINGS "Cycle cuts")
static const bool sCaptureHooksSet = (EEAppMapScopeEnter = AppMapScopeEnter, EEAppMapScopeLeave = AppMapScopeLeave,
                                      EEAppCaptureCheatSweep = EEAppCaptureCheatSweepImpl,
                                      EEAppCaptureSight = EEAppCaptureSightImpl,
                                      EEAppCaptureStatisticsSweep = EEAppCaptureStatisticsSweepImpl,
                                      EEAppCaptureStorm = EEAppCaptureStormImpl,
                                      EEAppCaptureStormSweep = EEAppCaptureStormSweepImpl, true);
