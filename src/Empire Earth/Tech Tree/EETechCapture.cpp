// New (port), TEST ONLY: the tech tree's, event effects' and calamities' records of the simulation capture
// (ee/EETechCapture.h; tools/sim_capture.py; docs/RUST_PLAN.md R5b.2). Reads the world, writes nothing to it.
#include <windows.h>
#include <algorithm>
#include <set>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamityCapture.h"
#include "ee/EECalamity.h"
#include "ee/EEMap.h"
#include "ee/EEContagionManager.h"
#include "rust/sim/tech.h"   // ee_tech_capture_* (ee_sim tech capture_rec.rs)
#include "rust/game/gamecpp.h"   // the calamities' records (ee_game gamecpp calamity_capture.rs)

// the node's, epoch's and tree's states: their Rust records (ee_sim tech capture_rec.rs)
static void EETechCaptureNodeImpl(EESimCaptureRecord& ioRecord, EETechTreeNode* inNode)
{
    ee_gamecpp_tech_capture_node_rec(&ioRecord, inNode);   // (gamecpp tech_hooks.rs)
}

static void EETechCaptureEpochImpl(EESimCaptureRecord& ioRecord, EETechTreeEpoch* inEpoch)
{
    ee_gamecpp_tech_capture_epoch_rec(&ioRecord, inEpoch);
}

static void EETechCaptureTreeImpl(EESimCaptureRecord& ioRecord, EETechTree* inTree)
{
    ee_gamecpp_tech_capture_tree_rec(&ioRecord, inTree);
}

static long EETechCapturePlayerIndexImpl(EETechTree* inTree)
{
    return inTree != NULL ? inTree->mPlayerIndex : -1;
}

static void EETechCaptureUpgradeImpl(EESimCaptureRecord& ioRecord, EEUnitClassUpgrade* inUpgrade)
{
    ee_gamecpp_tech_capture_upgrade(&ioRecord, inUpgrade);   // (gamecpp tech_capture.rs)
}

static void EETechCapturePlayerImpl(EESimCaptureRecord& ioRecord, EEPlayer* inPlayer)
{
    ee_gamecpp_tech_capture_player(&ioRecord, inPlayer);   // (gamecpp tech_capture.rs)
}

static void EETechCaptureCostImpl(EETechTreeNode* inNode, long inKind, long inResource, long inUnitClass,
                                  float inScale, long inCost)
{
    ee_tech_capture_cost(inKind, EETechCapturePlayerIndex(inNode->mTree), inNode->mData->mID,
                         inNode->mTree != NULL ? inNode->mTree->mCurrentEpoch : -1, inResource, inUnitClass, inScale, inCost);
}


// the calamities' game (EE-SIM --calamities: ee_game gamecpp calamity_capture.rs)
static void EETechCaptureCastCalamitiesImpl(long inCall)
{
    ee_gamecpp_capture_cast_calamities(inCall);
}

// TSWP: per player and node its availability, obsolescence, costs, SumCosts and prerequisite closure, and the tree's
// checksum (gamecpp tech_capture.rs)
static void EETechCaptureSweepImpl()
{
    ee_gamecpp_tech_capture_sweep();
}

// ---- CALW, CCAS, CALT, CONT: the calamities' runs (their records: ee_game gamecpp calamity_capture.rs) -----------

static void EECalamityCaptureEventImpl(long inOp, long inA, long inB, float inC, float inD)
{
    ee_gamecpp_capture_calamity_event(inOp, inA, inB, inC, inD);
}

static long EECalamityCaptureUpdateImpl(EECalamityClass* inClass, EECalamity* inCalamity, float inElapsedTime)
{
    return ee_gamecpp_capture_calamity_update(inClass, inCalamity, inElapsedTime);
}

static bool CanCastEnter()
{
    return ee_gamecpp_capture_can_cast_enter();
}

static void CanCastLeave()
{
    ee_gamecpp_capture_can_cast_leave();
}

static long CanCastRecordImpl(bool inOutermost, EECalamityClass* inClass, EEComplexUnit* inUnit,
                              const GE3DPoint& inLocation, unsigned long inX, unsigned long inY, bool inCheck,
                              EEUnit* inTargetUnit, long inResult)
{
    return ee_gamecpp_capture_can_cast_record(inOutermost, inClass, inUnit, (const float(*)[3])&inLocation.mX, inX, inY,
                                              inCheck, inTargetUnit, inResult);
}

static void EECalamityCaptureSweepImpl()
{
    ee_gamecpp_capture_calamity_sweep();
}

// ---- EVUN: the units an effect walks (the collection: ee_sim tech capture_rec.rs) ----------------------------------

static void EETechCaptureUnitStepImpl(EEUnit* inUnit, bool inApplies, long inValue)
{
    ee_gamecpp_tech_capture_unit_step(inUnit, inApplies, inValue);   // (gamecpp tech_hooks.rs)
}

static void EECalamityCaptureStartEventImpl(long inOp, long inA, long inB, float inC, float inD)
{
    ee_gamecpp_capture_start_event(inOp, inA, inB, inC, inD);
}

static void EECalamityCaptureStartPointImpl(long inWhich, float inX, float inY, float inZ)
{
    ee_gamecpp_capture_start_point(inWhich, inX, inY, inZ);
}

static bool EECalamityCaptureStartImpl(EECalamityClass* inClass, EECalamity* inCalamity)
{
    return ee_gamecpp_capture_calamity_start(inClass, inCalamity, false);
}

static void EECalamityCaptureStopImpl(EECalamityClass* inClass, EECalamity* inCalamity)
{
    ee_gamecpp_capture_calamity_start(inClass, inCalamity, true);
}

static void EECalamityCaptureContagionEventImpl(long inOp, long inA, long inB, float inC, float inD)
{
    ee_gamecpp_capture_contagion_event(inOp, inA, inB, inC, inD);
}

static void EECalamityCaptureContagionImpl(EeContagionHelper* inHelper, float inElapsedTime)
{
    ee_gamecpp_capture_contagion(inHelper, inElapsedTime);
}

// ------------------------------------------------------------------------- round 10, lane B: the records' C++

int64_t EECore10OpB7(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EETechTreeEpoch.cpp

#define P64(x) ((int64_t)(intptr_t)(x))
#define TC_UPGRADE ((EEUnitClassUpgrade*)p)
#define TC_PLAYER ((EEPlayer*)p)

// the upgrade's and the player's record fields (gamecpp tech_capture.rs; p the record, the upgrade or the player): one
// statement each
int64_t EECore10OpB6(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::TcBytes: ((EESimCaptureRecord*)p)->Bytes((const void*)a[0], (unsigned int)a[1]);
        break;
    case O::TcUpgradeFirst: return P64(&TC_UPGRADE->mAreaEffectScale);
    case O::TcUpgradeEnd: return P64(&TC_UPGRADE->mIsInitialized);   // (the bools before it included)
    case O::TcUpgradeMediaCount: return P64(&TC_UPGRADE->mMediaCount);
    case O::TcUpgradeButtonCount: return P64(&TC_UPGRADE->mButtonLoadCount);
    case O::TcPlayerId: return TC_PLAYER->mPlayerID;
    case O::TcPlayerPopulationBonus: return TC_PLAYER->mPopulationLimitBonus;
    case O::TcPlayerTributeFee: return P64(&TC_PLAYER->mTributeFee);
    case O::TcPlayerBB44: return TC_PLAYER->mBB44;
    default: return EECore10OpB7(inOp, p, a, f);
    }
    return 0;
}

// The game calls these through the header's hook pointers, set here before main: the game's units name no
// symbol of this TEST ONLY unit (tools/cpp_deps.py cuts; PORT_LEARNINGS "Cycle cuts")
static const bool sCaptureHooksSet = (EECalamityCanCastEnter = CanCastEnter, EECalamityCanCastLeave = CanCastLeave,
                                      EECalamityCanCastRecord = CanCastRecordImpl,
                                      EECalamityCaptureStart = EECalamityCaptureStartImpl,
                                      EECalamityCaptureUpdate = EECalamityCaptureUpdateImpl,
                                      EETechCapturePlayerIndex = EETechCapturePlayerIndexImpl,
                                      EECalamityCapture = ee_tech_capture_calamity,
                                      EECalamityCaptureContagion = EECalamityCaptureContagionImpl,
                                      EECalamityCaptureContagionEventHook = EECalamityCaptureContagionEventImpl,
                                      EECalamityCaptureEventHook = EECalamityCaptureEventImpl,
                                      EECalamityCaptureStartEventHook = EECalamityCaptureStartEventImpl,
                                      EECalamityCaptureStartPoint = EECalamityCaptureStartPointImpl,
                                      EECalamityCaptureStop = EECalamityCaptureStopImpl,
                                      EECalamityCaptureSweep = EECalamityCaptureSweepImpl,
                                      EETechCaptureCastCalamities = EETechCaptureCastCalamitiesImpl,
                                      EETechCaptureCost = EETechCaptureCostImpl,
                                      EETechCaptureEpoch = EETechCaptureEpochImpl,
                                      EETechCaptureNode = EETechCaptureNodeImpl,
                                      EETechCapturePlayer = EETechCapturePlayerImpl,
                                      EETechCaptureRescale = ee_tech_capture_rescale,
                                      EETechCaptureSweep = EETechCaptureSweepImpl,
                                      EETechCaptureTree = EETechCaptureTreeImpl,
                                      EETechCaptureUnitStep = EETechCaptureUnitStepImpl,
                                      EETechCaptureUnitWalkHook = ee_tech_capture_unit_walk,
                                      EETechCaptureUpgrade = EETechCaptureUpgradeImpl, true);

// ------------------------------------------------------------------------- round 11, lane C: the hooks' C++

int64_t EECore11OpE(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

#define TCU ((EEUnit*)p)
#define TCX ((EEComplexUnit*)p)

// the EVUN unit step's members (gamecpp tech_hooks.rs; p the unit or its complex unit; EECalamityUpdate.cpp's
// EECore11OpC6 default): one statement each
int64_t EECore11OpC7(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::C11TcUnitComplex: return P64(TCU->mComplexUnit);
    case O::C11TcUnitId: return (int64_t)TCU->mUnitID;
    case O::C11TcUnitType: return (int64_t)TCU->mUnitClass->mUnitType;
    case O::C11TcUnitHitPoints: return (long)TCU->GetHitPoints();
    case O::C11TcUnitTileNode: return TCU->mTileNode != NULL;
    case O::C11TcComplexBuilt: return TCX->mIsBuilt;
    case O::C11TcComplexPrevious: return TCX->mPreviousPart != NULL;
    case O::C11TcComplexFlightTime: return (long)TCX->mFlightTime;
    default: return EECore11OpE(inOp, p, a, f);
    }
    return 0;
}
