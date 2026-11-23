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
#include "ee/EEGameVariant.h"
#include "ee/EEButtonObject.h"
#include "ee/EETechTreeRecord.h"
#include "ee/EETechRust.h"
#include "ee/EEStringIDs.h"
#include <iterator>
#include <utility>
#include <stddef.h>
#include <stdlib.h>
#include "lle/UResourceFile.h"
#include "lle/UIHotKey.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEEvent.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEAEManager.h"
#include "ee/EmpireEarth.h"
#include "lle/FSRestorable.h"
#include "lle/GE3DPoint.h"
#include "ee/EEUnitKind.h"
#include "ee/EEEffect.h"
#include "ee/EECalamityUpdate.h"

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


#define P64(x) ((int64_t)(intptr_t)(x))
#define TC_UPGRADE ((EEUnitClassUpgrade*)p)
#define TC_PLAYER ((EEPlayer*)p)

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

int64_t EECore11OpC8(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEGameStateLoader.cpp

#define TCU ((EEUnit*)p)
#define TCX ((EEComplexUnit*)p)

#define BNODE ((EETechTreeNode*)p)
#define BEPOCH ((EETechTreeEpoch*)p)
#define BTREE ((EETechTree*)p)
#define TNODE ((EETechTreeNode*)p)
#define TEPOCH ((EETechTreeEpoch*)p)
#define TVARIANT EEGameVariant::GetGameVariant(gWorld.mGameOptions.mVariant)
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
    // (EECore10OpB6) the upgrade's and the player's record fields (gamecpp tech_capture.rs; p the record, the upgrade or the player): one
    // statement each
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
    // (EECore10OpB7) the tech tree's loads, copies, costs and sweep (gamecpp tech_load.rs, tech_capture.rs; p the node, the epoch, the
    // tree or the player): one statement each
    case O::TbNewData: return P64(BNODE->mData = new RTSTechTreeNodeData);
    case O::TbDeleteData: delete std::exchange(BNODE->mData, nullptr);
        break;
    case O::TbShareData: BNODE->mData = ((const EETechTreeNode*)a[0])->mData;
        break;
    case O::TbSetTree: BNODE->mTree = (EETechTree*)a[0];
        break;
    case O::TbNewNode: return P64(new EETechTreeNode);
    case O::TbClassesResize: BNODE->mData->mUnitClasses.resize((unsigned long)a[0], 0);
        break;
    case O::TbClassRead: return ((FSFile*)a[0])->Read(BNODE->mData->mUnitClasses[a[1]]);
    case O::TbClassCount: return (int64_t)BNODE->mData->mUnitClasses.size();
    case O::TbClassAt: return (int64_t)BNODE->mData->mUnitClasses[a[0]];
    case O::TbCostsInsert: BNODE->mData->mClassCosts.insert(std::map<long, long*>::value_type((long)BNODE->mData->mUnitClasses[a[0]], new long[7]{(long)a[1], (long)a[2], (long)a[3], (long)a[4], (long)a[5], (long)a[6], (long)a[7]}));
        break;
    case O::TbCostsCount: return (int64_t)BNODE->mData->mClassCosts.size();
    case O::TbCostsDelete: delete[] std::next(BNODE->mData->mClassCosts.begin(), a[0])->second;
        break;
    case O::TbCostsClear: BNODE->mData->mClassCosts.clear();
        break;
    case O::TbButton: return P64(BNODE->mButton);
    case O::TbCopyButton: BNODE->mButton = ((const EETechTreeNode*)a[0])->mButton;
        break;
    case O::TbSetButton: BNODE->mButton = EEButtonObject::Get((unsigned long)a[0]);
        break;
    case O::TbButtonGet: return P64(EEButtonObject::Get((unsigned long)a[0]));
    case O::TbSwapButton: ((EEPlayer*)p)->SwapClassButtonMedia((long)a[0], (EEButtonObject*)a[1], (EEButtonObject*)a[2]);
        break;
    case O::TbGlobalTree: return P64(&gTechTree);
    case O::TbEpochInsert: BEPOCH->mNodes.insert(std::map<long, EETechTreeNode*>::value_type(((EETechTreeNode*)a[0])->mData->mID, (EETechTreeNode*)a[0]));
        break;
    case O::TbSetPlayerIndex: BTREE->mPlayerIndex = (long)a[0];
        break;
    case O::TbCopyPlayer: BTREE->mPlayer = ((const EETechTree*)a[0])->mPlayer;
        break;
    case O::TbIsHuman: return ((EEPlayer*)p)->IsHuman();
    case O::TbClassFamily: return ((EEUnitClass*)p)->mFamily;
    case O::TbNodeIsObsolete: return BNODE->GetIsObsolete();
    case O::TbNodeDependsOn: return BNODE->DependsOn((long)a[0]);
    case O::TbNodeGetCost: return BNODE->GetCost((long)a[0], (long)a[1], f[0]);
    case O::TbTreeChecksum: return (int64_t)(uint32_t)EETechTreeWalk(BTREE, EeTechTreeOp::Checksum);
    case O::TbWorldPlayers: return (int64_t)gWorld.GetNumPlayers();
    case O::TbWorldPlayer: return P64(gWorld.GetPlayer((unsigned long)a[0]));
    case O::TbPlayerTree: return P64(((EEPlayer*)p)->GetTechTree());
    // (EECore9OpD7) the C++ calls and members of the nodes' and epochs' bodies in Rust (gamecpp tech.rs; p the node or the epoch, the
    // player for the Pl ops): one statement each
    case O::TkNodeFields: return P64(static_cast<EeTechNodeFields*>(TNODE));
    case O::TkNodeData: return P64(static_cast<EeTechNodeDataFields*>(TNODE->mData));
    case O::TkNodeTree: return P64(TNODE->mTree);
    case O::TkClassCosts: return P64(TNODE->mData->mClassCosts.count((long)a[0]) != 0 ? TNODE->mData->mClassCosts[(long)a[0]] : NULL);
    case O::TkUpgrade: return P64(static_cast<EEUnitStats*>(((EEPlayer*)p)->mUnitClassUpgrades[a[0]]));
    case O::TkInCostSet: return gUnitSetManager.GetSet(TVARIANT->mCostSet) != NULL && gUnitSetManager.GetSet(TVARIANT->mCostSet)->Contains((long)a[0]);
    case O::TkPlayerJit: return ((EEPlayer*)p)->mJITManufacturing;
    case O::TkClassHasRallyPoint: return gUnitClassTable[a[0]] != NULL && gUnitClassTable[a[0]]->HasRallyPoint();
    case O::TkNodeCount: return (int64_t)TEPOCH->mNodes.size();
    case O::TkNodeAt: return P64(std::next(TEPOCH->mNodes.begin(), a[0])->second);
    case O::TkNodeKeyAt: return std::next(TEPOCH->mNodes.begin(), a[0])->first;
    case O::TkNodeUnload: TNODE->Unload();
        break;
    case O::TkNodeDelete: delete TNODE;
        break;
    case O::TkNodesClear: TEPOCH->mNodes.clear();
        break;
    case O::TkEpochDeleteData: delete std::exchange(TEPOCH->mData, nullptr);
        break;
    case O::TkPushId: ((std::vector<long>*)a[0])->push_back((long)a[1]);
        break;
    case O::TkLoadedClassesPush: TEPOCH->mLoadedUnitClasses.push_back((long)a[0]);
        break;
    case O::TkLoadedClassesCount: return (int64_t)TEPOCH->mLoadedUnitClasses.size();
    case O::TkLoadedClassAt: return TEPOCH->mLoadedUnitClasses[a[0]];
    case O::TkLoadedClassesClear: TEPOCH->mLoadedUnitClasses.clear();
        break;
    case O::TkNodeLoadMedia: TEPOCH->EETechTreeNode::LoadMedia((EEPlayer*)a[0]);
        break;
    case O::TkNodeUnloadMedia: TEPOCH->EETechTreeNode::UnloadMedia((EEPlayer*)a[0]);
        break;
    // (EECore11OpC6) a class's run (gamecpp calamity_io.rs class_run): its host made and deleted around it (EEContagionManager.cpp's
    // EECore11OpC5 default); p the class, a0 the calamity, a1 and a2 the sounds
    case O::C11CalHostNew: return P64((new EECalamityUpdateHost{(EECalamityClass*)p, (EECalamity*)a[0], {(EESoundObject*)a[1], (EESoundObject*)a[2]}, {}, NULL, {}, {}}));
    case O::C11CalHostDelete: delete (EECalamityUpdateHost*)p;
        break;
    default: return EECore11OpC8(inOp, p, a, f);
    }
    return 0;
}
