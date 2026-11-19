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
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GETransformation.h"
#include "lle/GEViewport.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEPhysicalModel.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/UList.h"

#include "ee/EmpireEarth.h"
#include "ee/EETileCoordinate.h"
#include "ee/EETileLocation.h"
#include "ee/RTSWorld.h"
#include "ee/EECameraControl.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/EEStatisticsHistory.h"
#include "ee/EEStatisticsRecord.h"
#include "ee/EEPhysics.h"
#include "ee/EECheatCodes.h"
#include "ee/EEEMStorm.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEStormType.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEEffect.h"
#include "rust/render/ge.h"
#include "rust/game/app.h"   // the map records
#include "rust/sim/units.h"
#include "ee/EESimCapture.h"
#include "ee/EETechCapture.h"
#include "ee/EEAppCapture.h"

namespace {

const uint64_t kFnvBasis = 0xcbf29ce484222325ull;

void Fnv(uint64_t& ioHash, const void* inData, size_t inSize)
{
    const unsigned char* theBytes = static_cast<const unsigned char*>(inData);
    for (size_t i = 0; i < inSize; i++)
        ioHash = (ioHash ^ theBytes[i]) * 0x100000001b3ull;
}

template <class T> void FnvValue(uint64_t& ioHash, T inValue)
{
    Fnv(ioHash, &inValue, sizeof(inValue));
}

void Hash64(EESimCaptureRecord& ioRecord, uint64_t inHash)
{
    ioRecord.U32((unsigned int)inHash);
    ioRecord.U32((unsigned int)(inHash >> 32));
}

}   // namespace

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

static void EEAppCaptureChecksumImpl(unsigned long inCursorBefore, long inResult, unsigned long inCursorAfter)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord r(EECaptureTag("MAPK"));
    r.U32((unsigned int)inCursorBefore);
    r.I32((int)inResult);
    r.U32((unsigned int)inCursorAfter);
    r.Commit();
}

static void EEAppCaptureSlopeImpl(unsigned int inKind, const GETransformation& inLocation, float inResult)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord r(EECaptureTag("MAPP"));
    r.U32(inKind);
    r.Bytes(&inLocation, sizeof(GETransformation));
    r.F32(inResult);
    r.Commit();
}

static void EEAppCaptureMiniMapPixelsImpl(long inLeft, long inRight, long inBottom, long inTop,
                                          const unsigned short* inBits, unsigned long inPitch)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord r(EECaptureTag("MMPX"));
    r.I32((int)inLeft);
    r.I32((int)inRight);
    r.I32((int)inBottom);
    r.I32((int)inTop);
    for (long y = inBottom; y < inTop; y++) {
        if (inRight > inLeft)
            r.Bytes(&inBits[inPitch * y + inLeft], (unsigned int)(inRight - inLeft) * 2);
    }
    r.Commit();
}

static void EEAppCaptureMiniMapSnapshotImpl()
{
    if (!EESimCaptureRecord::Active() || gMiniMap == NULL)
        return;
    EESimCaptureRecord r(EECaptureTag("MMSN"));
    std::vector<EEMiniMapDot> theDots;
    std::vector<EEMiniMapUnitDot> theUnits;
    for (long i = 0; i < 0x1000; i++) {
        unsigned short theCount = 0;
        unsigned short thePixel = 0;
        gMiniMap->GetCell(i, theCount, thePixel, theDots, theUnits);
        r.U32(theCount | ((unsigned int)thePixel << 16));
        if (theDots.empty() && theUnits.empty())
            continue;
        r.U32(0x80000000u | (unsigned int)i);
        r.U32((unsigned int)theDots.size());
        for (const EEMiniMapDot& theDot : theDots) {
            r.U32(theDot.mPlayer | ((unsigned int)theDot.mColor << 16));
            r.I32((int)theDot.mFlag);
        }
        r.U32((unsigned int)theUnits.size());
        for (const EEMiniMapUnitDot& theDot : theUnits) {
            r.U32(theDot.mPlayer | ((unsigned int)theDot.mColor << 16));
            r.I32((int)theDot.mFlag);
            r.I32((int)theDot.mX);
            r.I32((int)theDot.mY);
        }
    }
    r.Commit();
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

static void EEAppCaptureCheatListImpl(const UWideString& inLine)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord r(EECaptureTag("CHLS"));
    const char* theText = (const char*)inLine;
    r.Bytes(theText, (unsigned int)strlen(theText));
    r.Commit();
}

namespace {

// every player's state a cheat changes: the resources, instant build, the epoch, seeing all, the units' hit points,
// flight time, power and commandable flag, the improvement levels, and the camera
void CheatEffect(const char* inLine)
{
    EESimCaptureRecord r(EECaptureTag("CHEF"));
    r.Bytes(inLine, (unsigned int)strlen(inLine));
    r.U32(0);
    for (unsigned long p = 0; p < gWorld.mNumPlayers; p++) {
        EEPlayer* thePlayer = gWorld.mPlayers[p];
        if (thePlayer == NULL)
            continue;
        for (long i = kResourceFood; i <= kResourceIron; i++)   // (0 and 1 are no resources: never set)
            r.I32((int)thePlayer->GetResource(i));
        r.U32((thePlayer->mInstantBuild ? 1u : 0u) | (thePlayer->GetSeesAll() ? 2u : 0u));
        r.I32(thePlayer->mTechnologies != NULL ? (int)thePlayer->mTechnologies->mCurrentEpoch : -1);
        uint64_t theHash = kFnvBasis;
        unsigned int theUnits = 0;
        for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            theUnits++;
            FnvValue(theHash, (int32_t)theUnit->mUnitID);
            FnvValue(theHash, theUnit->mHitPoints);
            FnvValue(theHash, (int32_t)theUnit->mResourceAmount);
            EEComplexUnit* theComplex = theUnit->mComplexUnit;
            if (theComplex != NULL) {
                FnvValue(theHash, theComplex->mFlightTime);
                FnvValue(theHash, theComplex->mPower);
                FnvValue(theHash, (int32_t)theComplex->mIsCommandable);
            }
        }
        r.U32(theUnits);
        Hash64(r, theHash);
        uint64_t theLevels = kFnvBasis;
        for (size_t i = 0; i < thePlayer->mUnitClassUpgrades.size(); i++) {
            EEUnitClassUpgrade* theUpgrade = thePlayer->mUnitClassUpgrades[i];
            if (theUpgrade == NULL)
                continue;
            for (unsigned long j = 0; j < 0x10; j++)
                FnvValue(theLevels, (int32_t)theUpgrade->mImprovement.GetLevel(j));
        }
        Hash64(r, theLevels);
    }
    GE3DPoint theCamera;
    gWorld.mCameraControl.GetLocation(gWorld.mCameraControl.mFraction, theCamera);
    r.F32(theCamera.mX);
    r.F32(theCamera.mY);
    r.F32(theCamera.mZ);
    r.Commit();
}

// the chat lines and whether each is applied (for the local player; a second id applies it for that player too).
// The two that end the game ("somebody set up us the bomb", "ahhhcool") are parsed only.
struct CheatLine {
    const char* mText;
    bool mApply;
    long mOther;   // another player to apply it for (-1: none)
};
const CheatLine kCheatLines[] = {
    {"atm", true, 2},
    {"ATM", true, -1},
    {"boston food sucks", true, -1},
    {"you said wood", true, 3},
    {"rock&roll", true, -1},
    {"creatine", true, -1},
    {"all your base are belong to us", true, -1},
    {"bam", true, -1},
    {"bam 3", true, 2},
    {"bam 12", true, -1},
    {"brainstorm", true, -1},
    {"BrainStorm", true, 3},
    {"boston rent", true, -1},
    {"uh, smoke?", true, -1},
    {"slimfast", true, 2},
    {"mine your own business", true, -1},
    {"girlyman", true, -1},
    {"the big dig", true, 3},
    {"columbus", true, -1},
    {"asus drivers", true, -1},
    {"the quotable patella", true, 2},
    {"display cheats", true, -1},
    {"i have the power", true, 2},
    {"coffee train", true, 3},
    {"friendly skies", true, -1},
    {"atm 0", true, -1},
    {"atm 77", true, -1},
    {"goto 40 50", true, -1},
    {"goto 100000 5", true, -1},
    {"goto 12", true, -1},
    {"find 2 5", true, -1},
    {"find 33554440", true, -1},
    {"find", true, -1},
    {"find x y z", true, -1},
    {"nothing here", true, -1},
    {"", true, -1},
    {"a", true, -1},
    {"9", true, -1},
    {"somebody set up us the bomb", false, -1},
    {"ahhhcool", false, -1},
    {"headshot 10", true, -1},
    {"my name is methos", true, -1},
};

}   // namespace

static void EEAppCaptureCheatSweepImpl()
{
    if (!EESimCaptureRecord::Active() || gCheatCodes == NULL)
        return;
    bool theEnabled = gWorld.mCheatsEnabled;
    gWorld.mCheatsEnabled = true;
    long theLocal = gWorld.GetCurrentPlayer()->mPlayerID;
    for (const CheatLine& theLine : kCheatLines) {
        long theCode = 0;
        long theParameter = 0;
        UWideString theText(theLine.mText);
        gCheatCodes->ParseCheatCode(theText, theCode, theParameter);
        EESimCaptureRecord r(EECaptureTag("CHET"));
        r.Bytes(theLine.mText, (unsigned int)strlen(theLine.mText));
        r.U32(0);
        r.I32((int)theCode);
        r.I32((int)theParameter);
        r.Commit();
        if (!theLine.mApply || theCode == 0)
            continue;
        gCheatCodes->ApplyCheatCode(theCode, theLocal, theParameter);
        if (theLine.mOther >= 0)
            gCheatCodes->ApplyCheatCode(theCode, theLine.mOther, theParameter);
        CheatEffect(theLine.mText);
    }
    // a code applied with cheats off does nothing
    gWorld.mCheatsEnabled = false;
    gCheatCodes->ApplyCheatCode(5, theLocal, 0);
    CheatEffect("(cheats off) atm");
    gWorld.mCheatsEnabled = theEnabled;
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
                                      EEAppCaptureCheatList = EEAppCaptureCheatListImpl,
                                      EEAppCaptureCheatSweep = EEAppCaptureCheatSweepImpl,
                                      EEAppCaptureChecksum = EEAppCaptureChecksumImpl,
                                      EEAppCaptureMiniMapPixels = EEAppCaptureMiniMapPixelsImpl,
                                      EEAppCaptureMiniMapSnapshot = EEAppCaptureMiniMapSnapshotImpl,
                                      EEAppCaptureSight = EEAppCaptureSightImpl,
                                      EEAppCaptureSlope = EEAppCaptureSlopeImpl,
                                      EEAppCaptureStatisticsSweep = EEAppCaptureStatisticsSweepImpl,
                                      EEAppCaptureStorm = EEAppCaptureStormImpl,
                                      EEAppCaptureStormSweep = EEAppCaptureStormSweepImpl, true);
