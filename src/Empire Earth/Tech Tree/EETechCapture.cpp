// New (port), TEST ONLY: the tech tree's, event effects' and calamities' records of the simulation capture
// (ee/EETechCapture.h; tools/sim_capture.py; docs/RUST_PLAN.md R5b.2). Reads the world, writes nothing to it.
#include <windows.h>
#include <algorithm>
#include <set>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/ESChecksum.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EETechCapture.h"

void EETechCaptureNode(EESimCaptureRecord& ioRecord, EETechTreeNode* inNode)
{
    ioRecord.I32(inNode->mData != NULL ? inNode->mData->mID : -1);
    ioRecord.I32(inNode->mEpoch);
    ioRecord.I32(inNode->mObsoleteEpoch);
    ioRecord.U32((inNode->mIsResearched ? 1u : 0u) | (inNode->mIsObsolete ? 2u : 0u) | (inNode->mIsAvailable ? 4u : 0u) |
                 (inNode->mScenEditorObsoleted ? 8u : 0u) | (inNode->mIsAllowed ? 16u : 0u));
    ioRecord.I32(inNode->mResearchDelay);
    // an epoch's prerequisite flags are never set (nor read): uninitialized memory, not recorded
    bool theIsEpoch = inNode->mData != NULL && EEIsEpochID(inNode->mData->mID);
    ioRecord.U32(theIsEpoch ? 0 : inNode->mPrerequisiteFlags);
    EETechTree* theTree = inNode->mTree;
    ioRecord.I32(theTree != NULL ? theTree->mCurrentEpoch : -1);
    ioRecord.I32(theTree != NULL ? theTree->mStartEpoch : -1);
    ioRecord.I32(theTree != NULL ? theTree->mEndEpoch : -1);
    ioRecord.I32(theTree != NULL ? theTree->mEpochsBeforeWonders : -1);
    ioRecord.I32(gEpochCount);
    RTSTechTreeNodeData* d = inNode->mData;
    ioRecord.I32(d != NULL ? d->mKind : -1);
    ioRecord.U32(d != NULL ? (d->mMakesUnit ? 1u : 0u) | (d->mCountsTowardEpoch ? 2u : 0u) | (d->mAvailableWithEpoch ? 4u : 0u) |
                     (d->mResearchedWithEpoch ? 8u : 0u) : 0u);
    ioRecord.I32(d != NULL ? d->mPrerequisite1 : -1);
    ioRecord.I32(d != NULL ? d->mPrerequisite2 : -1);
    ioRecord.I32(d != NULL ? d->mPrerequisite3 : -1);
    ioRecord.I32(d != NULL ? d->mPrerequisite4 : -1);
}

void EETechCaptureEpoch(EESimCaptureRecord& ioRecord, EETechTreeEpoch* inEpoch)
{
    EETechCaptureNode(ioRecord, inEpoch);
    ioRecord.I32(inEpoch->mRequiredSOL);
    ioRecord.I32(inEpoch->mCurrentSOL);
    ioRecord.U32(inEpoch->mIsCountingSOL ? 1 : 0);
}

void EETechCaptureTree(EESimCaptureRecord& ioRecord, EETechTree* inTree)
{
    ioRecord.I32(inTree->mPlayerIndex);
    ioRecord.I32(inTree->mCurrentEpoch);
    ioRecord.I32(inTree->mStartEpoch);
    ioRecord.I32(inTree->mEndEpoch);
    ioRecord.I32(inTree->mEpochsBeforeWonders);
}

long EETechCapturePlayerIndex(EETechTree* inTree)
{
    return inTree != NULL ? inTree->mPlayerIndex : -1;
}

void EETechCaptureUpgrade(EESimCaptureRecord& ioRecord, EEUnitClassUpgrade* inUpgrade)
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

void EETechCapturePlayer(EESimCaptureRecord& ioRecord, EEPlayer* inPlayer)
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

void EETechCaptureCost(EETechTreeNode* inNode, long inKind, long inResource, long inUnitClass, float inScale, long inCost)
{
    EESimCaptureRecord theRecord(EECaptureTag("TCST"));
    theRecord.I32(inKind);
    theRecord.I32(EETechCapturePlayerIndex(inNode->mTree));
    theRecord.I32(inNode->mData->mID);
    theRecord.I32(inNode->mTree != NULL ? inNode->mTree->mCurrentEpoch : -1);
    theRecord.I32(inResource);
    theRecord.I32(inUnitClass);
    theRecord.F32(inScale);
    theRecord.I32(inCost);
    theRecord.Commit();
}

void EETechCaptureRescale(long inWhat, unsigned long inUnitID, long inValue, unsigned long inOldMaximum, long inNewMaximum,
                          float inAmount)
{
    EESimCaptureRecord theRecord(EECaptureTag("EVHP"));
    theRecord.I32(inWhat);
    theRecord.U32(inUnitID);
    theRecord.I32(inValue);
    theRecord.U32(inOldMaximum);
    theRecord.I32(inNewMaximum);
    theRecord.F32(inAmount);
    theRecord.Commit();
}

void EECalamityCapture(long inKind, unsigned long inClassEnum, unsigned long inCaster, unsigned long inTarget, long inX,
                       long inY, unsigned long inPlayer, bool inResult)
{
    EESimCaptureRecord theRecord(EECaptureTag("CALC"));
    theRecord.I32(inKind);
    theRecord.U32(inClassEnum);
    theRecord.U32(inCaster);
    theRecord.U32(inTarget);
    theRecord.I32(inX);
    theRecord.I32(inY);
    theRecord.U32(inPlayer);
    theRecord.U32(inResult ? 1 : 0);
    theRecord.Commit();
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

namespace {

EEUnit* FirstLivingUnit(EEPlayer* inPlayer)
{
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
        if (theUnit->mComplexUnit != NULL && theUnit->mHitPoints > 0)
            return theUnit;
    return NULL;
}

}   // namespace

void EETechCaptureCastCalamities(long inCall)
{
    EECalamityManager* theManager = EECalamityManager::GetInstance();
    unsigned long n = gWorld.GetNumPlayers();
    for (unsigned long theRow = 0; theRow < 64; theRow++) {
        if ((long)(theRow % 8) != inCall % 8)
            continue;
        EECalamityClass* theClass = theManager->GetCalamityClass(theRow);
        if (theClass == NULL)
            continue;
        long theKind = theClass->GetTargetKind();
        for (unsigned long p = 2; p < n; p++) {
            EEPlayer* thePlayer = gWorld.GetPlayer(p);
            EEPlayer* theOther = gWorld.GetPlayer(p + 1 < n ? p + 1 : 2);
            if (thePlayer == NULL || theOther == NULL)
                continue;
            EEUnit* theCaster = FirstLivingUnit(thePlayer);
            EEUnit* theTarget = FirstLivingUnit(theOther);
            if (theCaster == NULL || theTarget == NULL)
                continue;
            if (theKind == 3)
                theManager->CreateUnitCalamity(theRow, theCaster->mUnitID, theTarget->mUnitID, thePlayer->mPlayerID);
            else if (theKind == 2)
                theManager->CreateLocationCalamity(theRow, theCaster->mUnitID, theTarget->GetTileX(), theTarget->GetTileY(),
                                                   thePlayer->mPlayerID);
            else if (theKind == 1)
                theManager->CreateGlobalCalamity(theRow, theCaster->mUnitID, thePlayer->mPlayerID);
        }
    }
}

void EETechCaptureSweep()
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
