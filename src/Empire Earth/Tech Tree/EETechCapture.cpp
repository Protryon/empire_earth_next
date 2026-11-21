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
    unsigned char theBytes[EE_TECH_CAPTURE_NODE_SIZE];
    EETechTree* theTree = inNode->mTree;
    ioRecord.Bytes(theBytes, ee_tech_capture_node(inNode, inNode->mData, theTree, EEEpochCount(), theBytes));
}

static void EETechCaptureEpochImpl(EESimCaptureRecord& ioRecord, EETechTreeEpoch* inEpoch)
{
    unsigned char theBytes[EE_TECH_CAPTURE_EPOCH_SIZE];
    EETechCaptureNode(ioRecord, inEpoch);
    ioRecord.Bytes(theBytes, ee_tech_capture_epoch(inEpoch, theBytes));
}

static void EETechCaptureTreeImpl(EESimCaptureRecord& ioRecord, EETechTree* inTree)
{
    unsigned char theBytes[EE_TECH_CAPTURE_TREE_SIZE];
    ioRecord.Bytes(theBytes, ee_tech_capture_tree(inTree, inTree->mPlayerIndex, theBytes));
}

static long EETechCapturePlayerIndexImpl(EETechTree* inTree)
{
    return inTree != NULL ? inTree->mPlayerIndex : -1;
}

static void EETechCaptureUpgradeImpl(EESimCaptureRecord& ioRecord, EEUnitClassUpgrade* inUpgrade)
{
    if (inUpgrade == NULL) {
        ioRecord.U32(0);
        return;
    }
    const unsigned char* theFirst = (const unsigned char*)&inUpgrade->mAreaEffectScale;
    const unsigned char* theEnd = (const unsigned char*)&inUpgrade->mIsInitialized;   // the bools before it included
    unsigned int theSize = (unsigned int)(theEnd - theFirst);
    // mMediaCount and mButtonLoadCount count the presentation's media loads (EEPlayer::LoadUnitClassMedia,
    // SwapClassButtonMedia), which run when the loader gets to them: written as 0, so the record carries simulation
    // state only
    std::vector<unsigned char> theBytes(theFirst, theEnd);
    const unsigned char* theZeroed[] = {(const unsigned char*)&inUpgrade->mMediaCount,
                                        (const unsigned char*)&inUpgrade->mButtonLoadCount};
    for (const unsigned char* theField : theZeroed)
        memset(theBytes.data() + (theField - theFirst), 0, sizeof(long));
    ioRecord.U32(theSize);
    ioRecord.Bytes(theBytes.data(), theSize);
}

static void EETechCapturePlayerImpl(EESimCaptureRecord& ioRecord, EEPlayer* inPlayer)
{
    if (inPlayer == NULL) {
        ioRecord.I32(-1);
        return;
    }
    ioRecord.I32(inPlayer->mPlayerID);
    ioRecord.I32(inPlayer->mPopulationLimitBonus);
    ioRecord.F32(inPlayer->mTributeFee);
    ioRecord.U32(inPlayer->mBB44 ? 1 : 0);
}

static void EETechCaptureCostImpl(EETechTreeNode* inNode, long inKind, long inResource, long inUnitClass,
                                  float inScale, long inCost)
{
    ee_tech_capture_cost(inKind, EETechCapturePlayerIndex(inNode->mTree), inNode->mData->mID,
                         inNode->mTree != NULL ? inNode->mTree->mCurrentEpoch : -1, inResource, inUnitClass, inScale, inCost);
}

namespace {

// whether GetCost and SumCosts index gUnitClassTable and the upgrades in range for the node (the sweep calls them on
// every node; the game only on the ones it offers)
bool NodeIndexesSafe(EETechTreeNode* inNode)
{
    unsigned long theCount = EEUnitClassCount();
    if ((unsigned long)inNode->GetUnitClass(0) >= theCount)
        return false;
    return !inNode->mData->mMakesUnit || (unsigned long)inNode->mData->mUnitClass < theCount;
}

// whether SumCosts can walk from the id without meeting a missing node (it dereferences every id in 0..99999)
bool SumCostsSafe(EETechTree* inTree, long inID, std::set<long>& ioVisiting)
{
    if (!(inID >= kTechEpochPrehistoric && inID < kTechNone))
        return true;
    EETechTreeNode* theNode = inTree->Get(inID);
    if (theNode == NULL)
        return false;
    if (ioVisiting.count(inID))
        return true;
    if (!NodeIndexesSafe(theNode))
        return false;
    ioVisiting.insert(inID);
    RTSTechTreeNodeData* d = theNode->mData;
    bool theSafe = SumCostsSafe(inTree, d->mPrerequisite1, ioVisiting) && SumCostsSafe(inTree, d->mPrerequisite2, ioVisiting) &&
                   SumCostsSafe(inTree, d->mPrerequisite3, ioVisiting) && SumCostsSafe(inTree, d->mPrerequisite4, ioVisiting);
    EEUnitClass* theClass = gUnitClassTable[theNode->GetUnitClass(0)];
    if (theSafe && theClass != NULL)
        theSafe = SumCostsSafe(inTree, theClass->mTechnology, ioVisiting);
    ioVisiting.erase(inID);
    return theSafe;
}

}   // namespace

// the calamities' game (EE-SIM --calamities: ee_game gamecpp calamity_capture.rs)
static void EETechCaptureCastCalamitiesImpl(long inCall)
{
    ee_gamecpp_capture_cast_calamities(inCall);
}

static void EETechCaptureSweepImpl()
{
    for (unsigned long p = 1; p < gWorld.GetNumPlayers(); p++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(p);
        if (thePlayer == NULL || thePlayer->GetTechTree() == NULL)
            continue;
        EETechTree* theTree = thePlayer->GetTechTree();
        std::vector<long> theIDs;
        theTree->GetNodeIDs(theIDs);
        for (long e = 0; e < theTree->mEpochs.size(); e++)
            theIDs.push_back(theTree->mEpochs[e].mData->mID);
        EESimCaptureRecord theHead(EECaptureTag("TSWP"));
        theHead.U32(0);
        EETechCaptureTree(theHead, theTree);
        theHead.U32((unsigned int)theIDs.size());
        ESChecksum theChecksum(true);
        theTree->GetChecksum(theChecksum);
        theHead.U32((unsigned int)theChecksum.GetTotalChecksum());
        theHead.Commit();
        for (size_t i = 0; i < theIDs.size(); i++) {
            EETechTreeNode* theNode = theTree->Get(theIDs[i]);
            if (theNode == NULL)
                continue;
            EESimCaptureRecord theRecord(EECaptureTag("TSWP"));
            theRecord.U32(1);
            theRecord.I32((long)p);
            EETechCaptureNode(theRecord, theNode);
            theRecord.U32(theNode->GetIsAvailable() ? 1 : 0);
            theRecord.U32(theNode->GetIsObsolete() ? 1 : 0);
            long theClass = theNode->GetUnitClass(0);
            theRecord.I32(theClass);
            bool theSafe = NodeIndexesSafe(theNode);
            theRecord.U32(theSafe ? 1 : 0);
            for (long r = 0; r < 7 && theSafe; r++) {
                theRecord.I32(theNode->GetCost(r, theClass, 1.0f));
                theRecord.I32(theNode->GetCost(r, theClass, 0.75f));
                theRecord.I32(theNode->GetCost(r, theClass, 1.5f));
            }
            std::set<long> theVisiting;
            if (SumCostsSafe(theTree, theIDs[i], theVisiting)) {
                long theCosts[7] = {0, 0, 0, 0, 0, 0, 0};
                theTree->SumCosts(theIDs[i], theClass, theCosts);
                theRecord.U32(1);
                for (long r = 0; r < 7; r++)
                    theRecord.I32(theCosts[r]);
            } else
                theRecord.U32(0);
            // the prerequisite closure, one bit per id of the list
            unsigned int theBits = 0;
            for (size_t j = 0; j < theIDs.size(); j++) {
                if (theNode->DependsOn(theIDs[j]))
                    theBits |= 1u << (j % 32);
                if (j % 32 == 31 || j + 1 == theIDs.size()) {
                    theRecord.U32(theBits);
                    theBits = 0;
                }
            }
            theRecord.Commit();
        }
    }
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
    if (!EESimCaptureRecord::Active())
        return;
    EEComplexUnit* theComplex = inUnit->mComplexUnit;
    ee_tech_capture_unit_step(inUnit->mUnitID, inUnit->mUnitClass->mUnitType, inUnit->GetHitPoints(),
                              (theComplex != NULL ? 1u : 0u) | (theComplex != NULL && theComplex->mIsBuilt ? 2u : 0u) |
                                  (inUnit->mTileNode != NULL ? 4u : 0u) |
                                  (theComplex != NULL && theComplex->mPreviousPart != NULL ? 8u : 0u),
                              theComplex != NULL ? (long)theComplex->mFlightTime : 0, inApplies, inValue);
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
