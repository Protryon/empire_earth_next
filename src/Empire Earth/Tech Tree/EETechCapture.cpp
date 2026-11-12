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
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamity.h"
#include "ee/EEMap.h"
#include "ee/EEEffectManager.h"
#include "ee/EEContagionHelper.h"
#include "ee/EEContagionManager.h"

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

// ---- CALW: a calamity's update with its world calls --------------------------------------------------------------

namespace {

std::vector<unsigned char> sCalamityEvents;
unsigned int sCalamityEventCount = 0;
long sCalamityCollecting = 0;
long sCanCastDepth = 0;
std::vector<long> sEpochSteps;
std::vector<unsigned char> sUnitSteps;
unsigned int sUnitStepCount = 0;

void Append(std::vector<unsigned char>& ioBytes, const void* inData, unsigned int inSize)
{
    const unsigned char* theBytes = (const unsigned char*)inData;
    ioBytes.insert(ioBytes.end(), theBytes, theBytes + inSize);
}

unsigned int UnitID(EEUnit* inUnit)
{
    return inUnit != NULL ? inUnit->mUnitID : 0xffffffffu;
}

// the calamity's numeric state (no pointers: whether the units are set, and their ids)
void CalamityState(EESimCaptureRecord& ioRecord, EECalamity* inCalamity)
{
    ioRecord.F32(inCalamity->mTimeAlive);
    ioRecord.F32(inCalamity->mFloatPlaceHolder1);
    ioRecord.F32(inCalamity->mFloatPlaceHolder2);
    ioRecord.I32(inCalamity->mIntPlaceHolder1);
    ioRecord.I32(inCalamity->mIntPlaceHolder2);
    ioRecord.U32((inCalamity->mBoolPlaceHolder1 ? 1u : 0u) | (inCalamity->mBoolPlaceHolder2 ? 2u : 0u) |
                 (inCalamity->mInitialized ? 4u : 0u) | (inCalamity->mHasTargetLocation ? 8u : 0u) |
                 (inCalamity->mHasTargetUnit ? 16u : 0u) | (inCalamity->mTargetUnit != NULL ? 32u : 0u) |
                 (inCalamity->mCasterUnit != NULL ? 64u : 0u));
    ioRecord.F32(inCalamity->mTargetLocation.mX);
    ioRecord.F32(inCalamity->mTargetLocation.mY);
    ioRecord.F32(inCalamity->mTargetLocation.mZ);
    ioRecord.F32(inCalamity->mShakeLocation.mX);
    ioRecord.F32(inCalamity->mShakeLocation.mY);
    ioRecord.F32(inCalamity->mShakeLocation.mZ);
    ioRecord.U32(inCalamity->mEffectHandle.mValue);
    ioRecord.F32(inCalamity->mEpicenterFactor);
    ioRecord.F32(inCalamity->mHeight);
    ioRecord.F32(inCalamity->mDiscreteUpdateTime);
    ioRecord.F32(inCalamity->mTimeElapsedSinceLastUpdate);
    ioRecord.I32((long)inCalamity->mTilePlaceHolder1.mX);
    ioRecord.I32((long)inCalamity->mTilePlaceHolder1.mY);
    ioRecord.U32(UnitID(inCalamity->mTargetUnit));
    ioRecord.U32(UnitID(inCalamity->mCasterUnit));
    ioRecord.U32(inCalamity->mPlayerNumber);
}

}   // namespace

void EECalamityCaptureEvent(long inOp, long inA, long inB, float inC, float inD)
{
    if (sCalamityCollecting == 0)
        return;
    int theInts[3] = {(int)inOp, (int)inA, (int)inB};
    float theFloats[2] = {inC, inD};
    Append(sCalamityEvents, theInts, sizeof(theInts));
    Append(sCalamityEvents, theFloats, sizeof(theFloats));
    sCalamityEventCount++;
}

long EECalamityCaptureUpdate(EECalamityClass* inClass, EECalamity* inCalamity, float inElapsedTime)
{
    if (!EESimCaptureRecord::Active())
        return inClass->Update(inCalamity, inElapsedTime);
    EESimCaptureRecord theRecord(EECaptureTag("CALW"));
    theRecord.I32(inClass->mClassEnum);
    CalamityState(theRecord, inCalamity);
    theRecord.F32(inElapsedTime);
    sCalamityEvents.clear();
    sCalamityEventCount = 0;
    sCalamityCollecting++;
    long theResult = inClass->Update(inCalamity, inElapsedTime);
    sCalamityCollecting--;
    CalamityState(theRecord, inCalamity);
    theRecord.I32(theResult);
    theRecord.U32(sCalamityEventCount);
    if (!sCalamityEvents.empty())
        theRecord.Bytes(sCalamityEvents.data(), (unsigned int)sCalamityEvents.size());
    theRecord.Commit();
    sCalamityEvents.clear();
    sCalamityEventCount = 0;
    return theResult;
}

// ---- CCAS: CanCastAt ----------------------------------------------------------------------------------------------

EECalamityCanCastScope::EECalamityCanCastScope() : mOutermost(sCanCastDepth == 0)
{
    sCanCastDepth++;
}

EECalamityCanCastScope::~EECalamityCanCastScope()
{
    sCanCastDepth--;
}

namespace {

void CanCastRecord(long inKind, EECalamityClass* inClass, EEComplexUnit* inUnit, const GE3DPoint& inLocation,
                   unsigned long inX, unsigned long inY, bool inCheck, EEUnit* inTargetUnit, long inResult)
{
    EESimCaptureRecord theRecord(EECaptureTag("CCAS"));
    theRecord.I32(inKind);
    theRecord.I32(inClass->mClassEnum);
    theRecord.U32(UnitID(inUnit));
    theRecord.F32(inLocation.mX);
    theRecord.F32(inLocation.mY);
    theRecord.F32(inLocation.mZ);
    theRecord.U32((unsigned int)inX);
    theRecord.U32((unsigned int)inY);
    theRecord.U32(inCheck ? 1 : 0);
    theRecord.U32(UnitID(inTargetUnit));
    theRecord.I32(inResult);
    theRecord.Commit();
}

EEUnit* LivingUnit(EEPlayer* inPlayer, long inSkip)
{
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
        if (theUnit->mComplexUnit != NULL && theUnit->mHitPoints > 0 && inSkip-- == 0)
            return theUnit;
    return NULL;
}

}   // namespace

long EECalamityCanCastScope::Record(EECalamityClass* inClass, EEComplexUnit* inUnit, const GE3DPoint& inLocation,
                                    unsigned long inX, unsigned long inY, bool inCheck, EEUnit* inTargetUnit,
                                    long inResult)
{
    if (this->mOutermost && EESimCaptureRecord::Active())
        CanCastRecord(0, inClass, inUnit, inLocation, inX, inY, inCheck, inTargetUnit, inResult);
    return inResult;
}

void EECalamityCaptureSweep()
{
    EECalamityManager* theManager = EECalamityManager::GetInstance();
    unsigned long n = gWorld.GetNumPlayers();
    long theSize = gWorld.mMap->GetSize();
    for (unsigned long theRow = 0; theRow < theManager->GetCalamityClassCount(); theRow++) {
        EECalamityClass* theClass = theManager->GetCalamityClass(theRow);
        if (theClass == NULL)
            continue;
        for (unsigned long p = 1; p < n; p++) {
            EEPlayer* thePlayer = gWorld.GetPlayer(p);
            EEPlayer* theOther = gWorld.GetPlayer(p + 1 < n ? p + 1 : 1);
            if (thePlayer == NULL || theOther == NULL)
                continue;
            EEUnit* theCaster = LivingUnit(thePlayer, 0);
            if (theCaster == NULL)
                continue;
            EEUnit* theTargets[4] = {NULL, LivingUnit(theOther, 0), theCaster, LivingUnit(thePlayer, 1)};
            EEUnit* theFar = LivingUnit(theOther, 3);
            long thePoints[6][2] = {
                {theCaster->GetTileX(), theCaster->GetTileY()},
                {theTargets[1] != NULL ? theTargets[1]->GetTileX() : theSize / 2,
                 theTargets[1] != NULL ? theTargets[1]->GetTileY() : theSize / 2},
                {theSize / 2, theSize / 2},
                {theCaster->GetTileX() + 3, theCaster->GetTileY() - 2},
                {theFar != NULL ? theFar->GetTileX() : 1, theFar != NULL ? theFar->GetTileY() : theSize / 2},
                {theSize + 5, theSize + 5},
            };
            for (long i = 0; i < 6; i++) {
                for (long t = 0; t < 4; t++) {
                    unsigned long theX = (unsigned long)thePoints[i][0];
                    unsigned long theY = (unsigned long)thePoints[i][1];
                    GE3DPoint theLocation((long)theX + 0.5f, (long)theY + 0.5f, 0.0f);
                    EEComplexUnit* theUnit = theCaster->mComplexUnit;
                    long theResult = theClass->CanCastAt(theUnit, theLocation, theX, theY, true, theTargets[t]);
                    CanCastRecord(1, theClass, theUnit, theLocation, theX, theY, true, theTargets[t], theResult);
                    if (t == 1 && i < 2) {
                        theResult = theClass->CanCastAt(theUnit, theLocation, theX, theY, false, theTargets[t]);
                        CanCastRecord(1, theClass, theUnit, theLocation, theX, theY, false, theTargets[t], theResult);
                    }
                    // nature's cast (no caster): only the classes cast at no location or unit read no caster
                    if (t == 0 && theClass->GetTargetKind() == 1) {
                        theResult = theClass->CanCastAt(NULL, theLocation, theX, theY, true, NULL);
                        CanCastRecord(1, theClass, NULL, theLocation, theX, theY, true, NULL, theResult);
                    }
                }
            }
        }
    }
}

// ---- TRTE: ResearchToEpoch's steps --------------------------------------------------------------------------------

void EETechCaptureEpochStep(long inStep, long inEpoch)
{
    if (!EESimCaptureRecord::Active())
        return;
    if (inStep < 0) {
        sEpochSteps.clear();
        return;
    }
    sEpochSteps.push_back(inStep);
    sEpochSteps.push_back(inEpoch);
}

void EETechCaptureResearchToEpoch(EETechTree* inTree, long inEpoch, bool inLocal)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord theRecord(EECaptureTag("TRTE"));
    theRecord.I32(inEpoch);
    theRecord.I32((long)inTree->mEpochs.size());
    theRecord.U32(inLocal ? 1 : 0);
    theRecord.U32((unsigned int)sEpochSteps.size() / 2);
    for (size_t i = 0; i < sEpochSteps.size(); i++)
        theRecord.I32(sEpochSteps[i]);
    EETechCaptureTree(theRecord, inTree);
    for (long e = 0; e < (long)inTree->mEpochs.size(); e++)
        EETechCaptureEpoch(theRecord, &inTree->mEpochs[e]);
    theRecord.Commit();
    sEpochSteps.clear();
}

// ---- EVUN: the units an effect walks ------------------------------------------------------------------------------

void EETechCaptureUnitStep(EEUnit* inUnit, bool inApplies, long inValue)
{
    if (!EESimCaptureRecord::Active())
        return;
    EEComplexUnit* theComplex = inUnit->mComplexUnit;
    int theValues[7] = {(int)inUnit->mUnitID, (int)inUnit->mUnitClass->mUnitType, (int)inUnit->GetHitPoints(),
                        (int)((theComplex != NULL ? 1u : 0u) | (theComplex != NULL && theComplex->mIsBuilt ? 2u : 0u) |
                              (inUnit->mTileNode != NULL ? 4u : 0u) |
                              (theComplex != NULL && theComplex->mPreviousPart != NULL ? 8u : 0u)),
                        theComplex != NULL ? (int)theComplex->mFlightTime : 0, inApplies ? 1 : 0, (int)inValue};
    Append(sUnitSteps, theValues, sizeof(theValues));
    sUnitStepCount++;
}

void EETechCaptureUnitWalk(long inLoop, long inEffect, long inUnitType, long inA, long inB)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord theRecord(EECaptureTag("EVUN"));
    theRecord.I32(inLoop);
    theRecord.I32(inEffect);
    theRecord.I32(inUnitType);
    theRecord.I32(inA);
    theRecord.I32(inB);
    theRecord.U32(sUnitStepCount);
    if (!sUnitSteps.empty())
        theRecord.Bytes(sUnitSteps.data(), (unsigned int)sUnitSteps.size());
    theRecord.Commit();
    sUnitSteps.clear();
    sUnitStepCount = 0;
}

// ---- CALT: a calamity's Start and Stop ----------------------------------------------------------------------------

namespace {

std::vector<unsigned char> sStartEvents;
unsigned int sStartEventCount = 0;
long sStartCollecting = 0;
std::vector<unsigned char> sContagionEvents;
unsigned int sContagionEventCount = 0;
long sContagionCollecting = 0;
// per thread: NeedsUnitMedia also runs on the media loader's thread
thread_local std::vector<std::vector<long> > sWalks;

void Event(std::vector<unsigned char>& ioEvents, unsigned int& ioCount, long inOp, long inA, long inB, float inC,
           float inD)
{
    int theInts[3] = {(int)inOp, (int)inA, (int)inB};
    float theFloats[2] = {inC, inD};
    Append(ioEvents, theInts, sizeof(theInts));
    Append(ioEvents, theFloats, sizeof(theFloats));
    ioCount++;
}

// what a start or stop changes on a unit (none: its id 0xffffffff)
void UnitState(EESimCaptureRecord& ioRecord, EEUnit* inUnit)
{
    ioRecord.U32(UnitID(inUnit));
    if (inUnit == NULL)
        return;
    ioRecord.I32(inUnit->mHitPoints);
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    ioRecord.U32(theUnit != NULL ? 1 : 0);
    if (theUnit == NULL)
        return;
    ioRecord.U32((theUnit->mIsDisabled ? 1u : 0u) | (theUnit->mIsTinted ? 2u : 0u) | (theUnit->mIsDeletable ? 4u : 0u));
    ioRecord.U32(theUnit->mDisabledReason);
    ioRecord.F32(theUnit->mSight);
    ioRecord.I32(theUnit->mPositiveMorale);
    ioRecord.I32(theUnit->mNegativeMorale);
    ioRecord.U32(theUnit->mCloaked);
    ioRecord.U32(theUnit->mCalamityFlags);
    ioRecord.U32(theUnit->mFireEffect.mValue);
    ioRecord.U32(theUnit->mNumAreaEffects);
}

void StartState(EESimCaptureRecord& ioRecord, EECalamity* inCalamity)
{
    CalamityState(ioRecord, inCalamity);
    ioRecord.F32(inCalamity->mRectangle.GetLeft());
    ioRecord.F32(inCalamity->mRectangle.GetTop());
    ioRecord.F32(inCalamity->mRectangle.GetRight());
    ioRecord.F32(inCalamity->mRectangle.GetBottom());
    ioRecord.U32((inCalamity->mSound != NULL ? 1u : 0u) | (inCalamity->mSecondarySound != NULL ? 2u : 0u));
    UnitState(ioRecord, inCalamity->mTargetUnit);
    UnitState(ioRecord, inCalamity->mCasterUnit);
    EEContagionManager* theManager = ::gWorld.GetContagionManager();
    ioRecord.U32((unsigned int)theManager->mHelpers.size());
    ioRecord.I32(theManager->mNextHelperID);
    ioRecord.U32(gEffectManager != NULL ? (unsigned int)gEffectManager->mEffectsByHandle.size() : 0u);
    ioRecord.U32(gEffectManager != NULL ? (unsigned int)gEffectManager->mDestroyedHandles.size() : 0u);
}

void StartRecord(long inKind, EECalamityClass* inClass, EECalamity* inCalamity, EESimCaptureRecord& ioRecord)
{
    ioRecord.I32(inKind);
    ioRecord.I32(inClass->mClassEnum);
    StartState(ioRecord, inCalamity);
    sStartEvents.clear();
    sStartEventCount = 0;
}

void StartCommit(EECalamity* inCalamity, EESimCaptureRecord& ioRecord, long inResult)
{
    StartState(ioRecord, inCalamity);
    ioRecord.I32(inResult);
    ioRecord.U32(sStartEventCount);
    if (!sStartEvents.empty())
        ioRecord.Bytes(sStartEvents.data(), (unsigned int)sStartEvents.size());
    ioRecord.Commit();
    sStartEvents.clear();
    sStartEventCount = 0;
}

}   // namespace

void EECalamityCaptureStartEvent(long inOp, long inA, long inB, float inC, float inD)
{
    if (sStartCollecting == 0)
        return;
    Event(sStartEvents, sStartEventCount, inOp, inA, inB, inC, inD);
}

void EECalamityCaptureStartPoint(long inWhich, float inX, float inY, float inZ)
{
    if (sStartCollecting == 0)
        return;
    int theZ;
    memcpy(&theZ, &inZ, 4);
    Event(sStartEvents, sStartEventCount, kCalamityStartEffect, inWhich, theZ, inX, inY);
}

bool EECalamityCaptureStart(EECalamityClass* inClass, EECalamity* inCalamity)
{
    if (!EESimCaptureRecord::Active() || sStartCollecting != 0)
        return inClass->Start(inCalamity);
    EESimCaptureRecord theRecord(EECaptureTag("CALT"));
    StartRecord(0, inClass, inCalamity, theRecord);
    sStartCollecting++;
    bool theResult = inClass->Start(inCalamity);
    sStartCollecting--;
    StartCommit(inCalamity, theRecord, theResult ? 1 : 0);
    return theResult;
}

void EECalamityCaptureStop(EECalamityClass* inClass, EECalamity* inCalamity)
{
    if (!EESimCaptureRecord::Active() || sStartCollecting != 0) {
        inClass->Stop(inCalamity);
        return;
    }
    EESimCaptureRecord theRecord(EECaptureTag("CALT"));
    StartRecord(1, inClass, inCalamity, theRecord);
    sStartCollecting++;
    inClass->Stop(inCalamity);
    sStartCollecting--;
    StartCommit(inCalamity, theRecord, 0);
}

// ---- CONT: a contagion helper's Update ----------------------------------------------------------------------------

namespace {

void HelperState(EESimCaptureRecord& ioRecord, EEContagionHelper* inHelper)
{
    ioRecord.I32(inHelper->mHelperID);
    ioRecord.F32(inHelper->mTimeSinceLastSpread);
    ioRecord.F32(inHelper->mTimeSinceLastUpdate);
    ioRecord.F32(inHelper->mCalamity != NULL ? inHelper->mCalamity->mTimeElapsedSinceLastUpdate : 0.0f);
    ioRecord.F32(inHelper->mCalamity != NULL ? inHelper->mCalamity->mDiscreteUpdateTime : 0.0f);
    ioRecord.U32((unsigned int)inHelper->mUnits.size());
    for (size_t i = 0; i < inHelper->mUnits.size(); i++) {
        EEUnit* theUnit = inHelper->mUnits[i];
        ioRecord.U32(theUnit->mUnitID);
        ioRecord.I32(theUnit->mHitPoints);
        ioRecord.U32(theUnit->mComplexUnit != NULL ? theUnit->mComplexUnit->mCalamityFlags : 0xffffffffu);
    }
}

}   // namespace

void EECalamityCaptureContagionEvent(long inOp, long inA, long inB, float inC, float inD)
{
    if (sContagionCollecting == 0)
        return;
    Event(sContagionEvents, sContagionEventCount, inOp, inA, inB, inC, inD);
}

void EECalamityCaptureContagion(EEContagionHelper* inHelper, float inElapsedTime)
{
    if (!EESimCaptureRecord::Active() || sContagionCollecting != 0) {
        inHelper->Update(inElapsedTime);
        return;
    }
    EESimCaptureRecord theRecord(EECaptureTag("CONT"));
    theRecord.I32(inHelper->mCalamity != NULL ? inHelper->mCalamity->mClass->mClassEnum : -1);
    HelperState(theRecord, inHelper);
    theRecord.F32(inElapsedTime);
    sContagionEvents.clear();
    sContagionEventCount = 0;
    sContagionCollecting++;
    inHelper->Update(inElapsedTime);
    sContagionCollecting--;
    HelperState(theRecord, inHelper);
    theRecord.U32(sContagionEventCount);
    if (!sContagionEvents.empty())
        theRecord.Bytes(sContagionEvents.data(), (unsigned int)sContagionEvents.size());
    theRecord.Commit();
    sContagionEvents.clear();
    sContagionEventCount = 0;
}

// ---- TNWK: the tech tree's walks ------------------------------------------------------------------------------------

void EETechCaptureWalkBegin()
{
    if (!EESimCaptureRecord::Active())
        return;
    sWalks.push_back(std::vector<long>());
}

void EETechCaptureWalkStep(long inID, long inAction)
{
    if (!EESimCaptureRecord::Active() || sWalks.empty())
        return;
    sWalks.back().push_back(inID);
    sWalks.back().push_back(inAction);
}

void EETechCaptureWalk(long inKind, EETechTree* inTree, EETechTreeNode* inNode, long inA, long inB, long inResult)
{
    if (!EESimCaptureRecord::Active())
        return;
    // GetBuildTime and NeedsUnitMedia are asked from other threads too (the media loader's): their records' order
    // against the game thread's is timing's, so they are TNMT, apart from the game thread's walks
    bool theOtherThreads = inKind == 6 || inKind == 10;
    EESimCaptureRecord theRecord(theOtherThreads ? EECaptureTag("TNMT") : EECaptureTag("TNWK"));
    theRecord.I32(inKind);
    theRecord.I32(inTree != NULL ? EETechCapturePlayerIndex(inTree) : -1);
    theRecord.I32(inA);
    theRecord.I32(inB);
    theRecord.I32(inResult);
    std::vector<long> theSteps;
    if (!sWalks.empty()) {
        theSteps.swap(sWalks.back());
        sWalks.pop_back();
    }
    theRecord.U32((unsigned int)theSteps.size() / 2);
    for (size_t i = 0; i < theSteps.size(); i++)
        theRecord.I32(theSteps[i]);
    if (inNode != NULL)
        EETechCaptureNode(theRecord, inNode);
    if (inTree != NULL)
        EETechCaptureTree(theRecord, inTree);
    theRecord.Commit();
}
