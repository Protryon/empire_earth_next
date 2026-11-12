// C:\EEX\Empire Earth\Tech Tree\EETechTreeEpoch.cpp (file name inferred: no __FILE__; RTTI EETechTreeEpoch >
// EETechTreeNode > RTSTechTreeNode, vtable 0x44612c). One epoch of a player's tech tree: 0x1cdf25..0x1cedce (its key
// chain to EEWorld::GetGameVariant's COMDAT; EETechTreeNode's ctor 0x1cedce opens the next unit). Classes in
// ee/EEPlayer.h. Unit flags: the exe's module set.


#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGameVariant.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEEvent.h"
#include "ee/EEButtonObject.h"
#include "ee/EETechTreeRecord.h"
#include "ee/EETechCapture.h"
#include "ee/EETechRust.h"

const UString EETechTreeEpoch::sEpochsCurrentSOL("EETechTreeEpoch-EpochsCurrentSOL");

// FUNCTION: EXE 0x5cdf68
// Creates an empty epoch node (no techs counted toward it yet)
EETechTreeEpoch::EETechTreeEpoch()
{
    this->mRequiredSOL = 0;
    this->mFirstNodeID = 0;
    this->mCurrentSOL = 0;
    this->mNextEpoch = 0;
    this->mIsCountingSOL = false;
}

// SYNTHETIC: EXE 0x5cdfb3
// (the scalar deleting destructor, emitted with the vtable; slot EETechTreeEpoch+0x0)

// FUNCTION: EXE 0x5cdfcf
// Destroys the epoch and the tech nodes it owns
EETechTreeEpoch::~EETechTreeEpoch()
{
    this->DeleteNodes();
}

// FUNCTION: EXE 0x5cdff6
// Deletes every tech node of the epoch
void EETechTreeEpoch::DeleteNodes()
{
    std::map<long, EETechTreeNode*>::const_iterator theNode;
    theNode = this->mNodes.begin();
    while (theNode != this->mNodes.end()) {
        EETechTreeNode* theTreeNode = theNode->second;
        if (theTreeNode != NULL)
            delete theTreeNode;
        ++theNode;
    }
    this->mNodes.clear();
}

// FUNCTION: EXE 0x5ce038
// Returns an epoch's cost in a resource scaled by the game variant's epoch cost factors (first, second or
// later epoch advanced in this game)
long EETechTreeEpoch::GetCost(long inResource, long inUnitClass, float inScale)
{
    long theCost = this->GetCostUncaptured(inResource, inUnitClass, inScale);
    if (EESimCaptureRecord::Active())   // TEST ONLY (ee/EETechCapture.h)
        EETechCaptureCost(this, 1, inResource, inUnitClass, inScale, theCost);
    return theCost;
}

long EETechTreeEpoch::GetCostUncaptured(long inResource, long inUnitClass, float inScale)
{
    // the scale by the epochs advanced in this game is the Rust tech logic's (ee_tech epoch_cost_scale)
    EEGameVariant* theVariant = EEGameVariant::GetGameVariant(gWorld.mGameOptions.mVariant);
    unsigned long theEpochs = this->mTree->mCurrentEpoch - this->mTree->mStartEpoch;
    float theScale = ee_tech_epoch_cost_scale((uint32_t)theEpochs, theVariant->GetEpochCostFactor(),
        theVariant->mEpochCostFactor, theVariant->GetSecondEpochCostFactor(), theVariant->GetFirstEpochCostFactor(), inScale);
    return EETechTreeNode::GetCost(inResource, inUnitClass, theScale);
}

// FUNCTION: EXE 0x5ce0e7
// Returns how many techs counting toward this epoch were researched
long EETechTreeEpoch::GetCurrentSOL()
{
    return this->mCurrentSOL;
}

// FUNCTION: EXE 0x5ce0eb
// Returns the epoch's research time; computer players need half of it
long EETechTreeEpoch::GetBuildTime()
{
    long theResult;
    long theHuman = this->mTree->GetPlayer()->IsHuman();   // (TNWK, TEST ONLY: asked once, as the test did)
    if (theHuman == 1)
        theResult = this->mData->mBuildTime;
    else
        theResult = this->mData->mBuildTime / 2;
    EETechCaptureWalkBegin();   // TNWK (TEST ONLY)
    EETechCaptureWalk(6, NULL, NULL, theHuman, this->mData->mBuildTime, theResult);   // TNWK (TEST ONLY)
    return theResult;
}

// FUNCTION: EXE 0x5ce10e
// Reads the epoch's record and its tech nodes from the tech tree file (nodes with epoch ids are dropped)
void EERegisterTechNode(long inID, long inEpoch);   // EETechTree.cpp

long EETechTreeEpoch::Load(FSFile& inFile, EETechTree* inTree)
{
    unsigned long theRead = 0;
    unsigned long theNodeCount = 0;
    unsigned long theCount = 0;
    EETechTreeRecord theRecord;
    this->mTree = inTree;
    this->mData = new RTSTechTreeNodeData;
    for (long i = 0; i < 7; i++)
        this->mData->mCosts[i] = 0;
    inFile.Read(theNodeCount);
    inFile.Read(reinterpret_cast<char*>(&theRecord), sizeof(theRecord), theRead);
    long theResult = inFile.Read(theCount);
    this->mData->mUnitClasses.resize(theCount, 0);
    for (unsigned long j = 0; j < theCount; j++)
        inFile.Read(this->mData->mUnitClasses[j]);
    this->mData->mCosts[kResourceStone] = theRecord.mCost4;
    this->mData->mCosts[kResourceGold] = theRecord.mCost5;
    this->mData->mCosts[kResourceWood] = theRecord.mCost3;
    this->mData->mCosts[kResourceFood] = theRecord.mCost2;
    this->mData->mCosts[kResourceIron] = theRecord.mCost6;
    this->mData->mID = theRecord.mID;
    this->mEpoch = theRecord.mEpoch;
    this->mObsoleteEpoch = theRecord.mObsoleteEpoch;
    this->mData->mResearchedEvent = theRecord.mResearchedEvent;
    this->mData->mObsoleteEvent = theRecord.mObsoleteEvent;
    this->mData->mBuildTime = theRecord.mBuildTime;
    this->mData->mPrerequisite1 = theRecord.mPrerequisite1;
    this->mData->mPrerequisite2 = theRecord.mPrerequisite2;
    this->mData->mPrerequisite3 = theRecord.mPrerequisite3;
    this->mData->mPrerequisite4 = theRecord.mPrerequisite4;
    this->mData->mUnitClass = theRecord.mUnitClass;
    this->mData->mMakesUnit = theRecord.mMakesUnit;
    this->mRequiredSOL = theRecord.mRequiredSOL;
    this->mFirstNodeID = theRecord.mFirstNodeID;
    this->mData->mDefaultUnitClass = theRecord.mDefaultUnitClass;
    this->mNextEpoch = theRecord.mNextEpoch;
    this->mTitleID = theRecord.mTitleID;
    this->mData->mNameID = theRecord.mNameID;
    this->mData->mCountsTowardEpoch = theRecord.mCountsTowardEpoch;
    this->mCurrentSOL = 0;
    this->mData->mKind = theRecord.mKind;
    this->mData->mCalamity = theRecord.mCalamity;
    this->mData->mHotKey = theRecord.mHotKey;
    this->mData->mCostResource = theRecord.mCostResource;
    this->mData->mRequiredStatistic = theRecord.mRequiredStatistic;
    this->mData->mRequiredStatisticValue = theRecord.mRequiredStatisticValue;
    this->mIsCountingSOL = false;
    this->mData->mObsoletedByDefault = theRecord.mObsoletedByDefault;
    this->mData->mShowInEditor = theRecord.mShowInEditor;
    this->mData->mAvailableWithEpoch = theRecord.mAvailableWithEpoch;
    this->mData->mResearchedWithEpoch = theRecord.mResearchedWithEpoch;
    this->mButton = EEButtonObject::Get(theRecord.mButton);
    if (theResult == kErrorOK) {
        for (unsigned long k = 0; k < theNodeCount; k++) {
            if (theResult != kErrorOK)
                break;
            EETechTreeNode* theNode = new EETechTreeNode;
            theResult = theNode->Load(inFile, this->mTree);
            if (theResult == kErrorOK && theNode->mData->mID >= kTechFirstNodeID) {
                // the node belongs to the epoch whose block holds it (the original: id / 1000 - 1)
                theNode->mEpoch = this->mData->mID;
                EERegisterTechNode(theNode->mData->mID, this->mData->mID);
                this->mNodes.insert(std::map<long, EETechTreeNode*>::value_type(theNode->mData->mID, theNode));
            } else {
                theNode->Unload();
                delete theNode;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5ce39e
// Creates empty tech node data
// (RTSTechTreeNodeData's implicit ctor, ee/EEPlayer.h: its first user is Load)

// FUNCTION: EXE 0x5ce3c9
// Unloads and deletes every tech node and the epoch's data
long EETechTreeEpoch::Unload()
{
    std::map<long, EETechTreeNode*>::const_iterator theNode;
    theNode = this->mNodes.begin();
    while (theNode != this->mNodes.end()) {
        EETechTreeNode* theTreeNode = theNode->second;
        theTreeNode->Unload();
        if (theTreeNode != NULL)
            delete theTreeNode;
        ++theNode;
    }
    this->mNodes.clear();
    if (this->mData != NULL) {
        delete this->mData;
        this->mData = NULL;
    }
    return 0;
}

// FUNCTION: EXE 0x5ce439
// Appends the ids of the epoch's tech nodes
void EETechTreeEpoch::GetNodeIDs(std::vector<long>& outIDs)
{
    std::map<long, EETechTreeNode*>::const_iterator theNode;
    theNode = this->mNodes.begin();
    while (theNode != this->mNodes.end()) {
        outIDs.push_back(theNode->first);
        ++theNode;
    }
}

// FUNCTION: EXE 0x5ce474
// Tells whether the epoch can be researched: available, not obsolete, not researched
bool EETechTreeEpoch::GetIsAvailable()
{
    EeTechNode theEpoch = EETechNodeView(this);
    return ee_tech_epoch_is_available(&theEpoch);
}

// FUNCTION: EXE 0x5ce494
// Tells whether the epoch is obsolete
bool EETechTreeEpoch::GetIsObsolete()
{
    return this->mIsObsolete;
}

// FUNCTION: EXE 0x5ce498
// Counts one more tech toward the epoch; when enough are researched the epoch becomes available (event 0x7d2)
void EETechTreeEpoch::AddSOL()
{
    if (EESimCaptureRecord::Active()) {   // TEST ONLY (ee/EETechCapture.h)
        EESimCaptureRecord theRecord(EECaptureTag("TSOL"));
        theRecord.U32(0);
        theRecord.I32(EETechCapturePlayerIndex(this->mTree));
        EETechCaptureEpoch(theRecord, this);
        this->AddSOLUncaptured();
        EETechCaptureEpoch(theRecord, this);
        theRecord.Commit();
        return;
    }
    this->AddSOLUncaptured();
}

void EETechTreeEpoch::AddSOLUncaptured()
{
    if (this->mTree == NULL)
        return;
    EeTechEpochCounter theCounter = EETechEpochCounterView(this);
    EeTechTreeState theTree = EETechTreeView(this->mTree);
    EeTechEpochUpdate theUpdate = ee_tech_add_sol((int32_t)this->mData->mID, &theCounter, &theTree);
    this->mCurrentSOL = theUpdate.current;
    if (!theUpdate.becomes_available)
        return;
    this->mIsAvailable = true;
    EEEvent::Trigger(kEventHookEpochAvailable, this->mTree->mPlayerIndex, this->mData->mID, this->mTree->mPlayer, NULL);
    this->mTree->UpdateInterface(false);
}

// FUNCTION: EXE 0x5ce4f2
// Reads the epoch's researched-tech count from the saved game, to be applied later
long EETechTreeEpoch::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EETileCoordinate theEntry(0, 0);
    long theResult = inFile.Read(this->mCurrentSOL);
    theEntry.mX = this->mData->mID;
    theEntry.mY = this->mCurrentSOL;
    this->mTree->mSavedSOLs.push_back(theEntry);
    this->mIsCountingSOL = false;
    return theResult;
}

// FUNCTION: EXE 0x5ce53f
// Does nothing: the original walked the nodes needing unit media and computed each one's media mask, then dropped
// it (the check was never finished); the unused computation is removed
void EETechTreeEpoch::CheckUnitMedia(EEPlayer* inPlayer)
{
}

// FUNCTION: EXE 0x5ce5b5
// Loads the media of every unit class the epoch's buildings make and the epoch's own media
void EETechTreeEpoch::LoadMedia(EEPlayer* inPlayer)
{
    std::map<long, EETechTreeNode*>::const_iterator theNode;
    theNode = this->mNodes.begin();
    while (theNode != this->mNodes.end()) {
        EETechTreeNode* theTreeNode = theNode->second;
        if (theTreeNode->NeedsUnitMedia() == true) {
            inPlayer->SetUnitClassMedia(theTreeNode->mData->mUnitClass, 0x7f);
            long theUnitClass = theTreeNode->mData->mUnitClass;
            this->mLoadedUnitClasses.push_back(theUnitClass);
        }
        ++theNode;
    }
    EETechTreeNode::LoadMedia(inPlayer);
}

// FUNCTION: EXE 0x5ce624
// Loads the media of the epoch's nodes of one kind
void EETechTreeEpoch::LoadMediaOfKind(long inKind)
{
    std::map<long, EETechTreeNode*>::const_iterator theNode;
    theNode = this->mNodes.begin();
    while (theNode != this->mNodes.end()) {
        EETechTreeNode* theTreeNode = theNode->second;
        if (theTreeNode->mData->mKind == inKind)
            theTreeNode->LoadMedia(this->mTree->GetPlayer());
        ++theNode;
    }
}

// FUNCTION: EXE 0x5ce675
// Makes a node of the epoch obsolete, or the epoch itself when it is still ahead (its obsolete event fires)
void EETechTreeEpoch::MakeObsolete(long inID)
{
    EETechCaptureWalkBegin();   // TNWK (TEST ONLY)
    long theAction = 0;   // TNWK (TEST ONLY): 0 no node, 1 the node's SetObsolete, 2 the epoch obsoleted, 3 nothing
    if (inID != this->mData->mID) {
        std::map<long, EETechTreeNode*>::iterator theNode = this->mNodes.find(inID);
        if (theNode != this->mNodes.end()) {
            theAction = 1;
            theNode->second->SetObsolete(true, false);
        }
    } else {
        theAction = 3;
        if (this->mIsObsolete == false && this->mTree->mCurrentEpoch < inID) {
            theAction = 2;
            this->mIsObsolete = true;
            EEEvent::Trigger(this->mData->mObsoleteEvent, this->mTree->mPlayerIndex, this->mData->mID, this->mTree->mPlayer, NULL);
            this->mTree->UpdateInterface(false);
        }
    }
    EETechCaptureWalk(4, this->mTree, this, inID, 0, theAction);   // TNWK (TEST ONLY)
}

// FUNCTION: EXE 0x5ce6e9
// Makes the nodes whose obsolete epoch has passed obsolete
void EETechTreeEpoch::ObsoleteOutdatedNodes()
{
    std::map<long, EETechTreeNode*>::const_iterator theNode;
    long theEpoch = this->mTree->mCurrentEpoch;
    EETechCaptureWalkBegin();   // TNWK (TEST ONLY)
    theNode = this->mNodes.begin();
    while (theNode != this->mNodes.end()) {
        EETechTreeNode* theTreeNode = theNode->second;
        bool theOutdated = theTreeNode->mIsObsolete == false && theTreeNode->mObsoleteEpoch < theEpoch;
        EETechCaptureWalkStep(theNode->first, theOutdated ? 1 : 0);   // TNWK (TEST ONLY)
        if (theOutdated)
            theTreeNode->SetObsolete(true, false);
        ++theNode;
    }
    EETechCaptureWalk(3, this->mTree, this, theEpoch, 0, 0);   // TNWK (TEST ONLY)
}

// FUNCTION: EXE 0x5ce738
// Satisfies one prerequisite of a node of the epoch
void EETechTreeEpoch::SetPrerequisite(long inID, long inPrerequisite)
{
    std::map<long, EETechTreeNode*>::iterator theNode = this->mNodes.find(inID);
    if (theNode != this->mNodes.end())
        theNode->second->SetPrerequisite(inPrerequisite);
}

// FUNCTION: EXE 0x5ce766
// Unlocks the epoch: available with its count complete; its nodes become available or get the epoch prerequisite
void EETechTreeEpoch::Unlock()
{
    std::map<long, EETechTreeNode*>::const_iterator theNode;
    EETechCaptureWalkBegin();   // TNWK (TEST ONLY)
    this->mIsObsolete = false;
    this->mCurrentSOL = this->mRequiredSOL;
    this->mIsAvailable = true;
    this->mTree->UpdateInterface(false);
    theNode = this->mNodes.begin();
    while (theNode != this->mNodes.end()) {
        // what the node gets is Rust (ee_tech tree.rs unlock_action), asked as the walk reaches it
        EeTechNode theView = EETechNodeView(theNode->second);
        EeTechTreeState theTree = EETechTreeView(this->mTree);
        EeTechUnlockAction theAction = ee_tech_unlock_action(&theView, &theTree, theNode->second->mData->mAvailableWithEpoch);
        EETechCaptureWalkStep(theNode->first, (long)theAction);   // TNWK (TEST ONLY)
        switch (theAction) {
        case EeTechUnlockAction::MakeAvailable:
            theNode->second->MakeAvailable();
            break;
        case EeTechUnlockAction::SetPrerequisite:
            theNode->second->SetPrerequisite(this->mData->mID);
            break;
        case EeTechUnlockAction::Nothing:
            break;
        }
        ++theNode;
    }
    EETechCaptureWalk(0, this->mTree, this, this->mData->mID, 0, 0);   // TNWK (TEST ONLY)
}

// FUNCTION: EXE 0x5ce7db
// Makes the epoch available once enough techs count toward it, and gives its nodes the epoch prerequisite
void EETechTreeEpoch::UpdateAvailability()
{
    if (EESimCaptureRecord::Active()) {   // TEST ONLY (ee/EETechCapture.h)
        EESimCaptureRecord theRecord(EECaptureTag("TSOL"));
        theRecord.U32(1);
        theRecord.I32(EETechCapturePlayerIndex(this->mTree));
        EETechCaptureEpoch(theRecord, this);
        this->UpdateAvailabilityUncaptured();
        EETechCaptureEpoch(theRecord, this);
        theRecord.Commit();
        return;
    }
    this->UpdateAvailabilityUncaptured();
}

void EETechTreeEpoch::UpdateAvailabilityUncaptured()
{
    std::map<long, EETechTreeNode*>::const_iterator theNode;
    if (this->mTree == NULL)
        return;
    EeTechEpochCounter theCounter = EETechEpochCounterView(this);
    EeTechTreeState theTree = EETechTreeView(this->mTree);
    EeTechEpochUpdate theUpdate = ee_tech_update_availability((int32_t)this->mData->mID, &theCounter, &theTree);
    if (!theUpdate.considered)
        return;
    if (theUpdate.becomes_available) {
        this->mIsAvailable = true;
        EEEvent::Trigger(kEventHookEpochAvailable, this->mTree->mPlayerIndex, this->mData->mID, this->mTree->mPlayer, NULL);
        this->mTree->UpdateInterface(false);
    }
    theNode = this->mNodes.begin();
    while (theNode != this->mNodes.end()) {
        theNode->second->SetPrerequisite(this->mData->mID);
        ++theNode;
    }
}

// FUNCTION: EXE 0x5ce862
// Researches the epoch itself
long EETechTreeEpoch::ResearchNode(EEComplexUnit* inUnit)
{
    long theID = this->mData->mID;
    return this->Research(theID, inUnit);
}

// FUNCTION: EXE 0x5ce875
// Researches the epoch and its nodes researched with it (every node when asked)
void EETechTreeEpoch::ResearchAll(bool inAllNodes)
{
    std::map<long, EETechTreeNode*>::const_iterator theNode;
    EETechCaptureWalkBegin();   // TNWK (TEST ONLY)
    this->ResearchSelf();
    theNode = this->mNodes.begin();
    while (theNode != this->mNodes.end()) {
        EETechTreeNode* theTreeNode = theNode->second;
        bool theTakes = ee_tech_research_all_takes(theTreeNode->mData->mResearchedWithEpoch, inAllNodes);   // (ee_tech tree.rs)
        EETechCaptureWalkStep(theNode->first, theTakes ? 1 : 0);   // TNWK (TEST ONLY)
        if (theTakes)
            theTreeNode->ResearchNode(NULL);
        ++theNode;
    }
    EETechCaptureWalk(1, this->mTree, this, this->mData->mID, inAllNodes ? 1 : 0, 0);   // TNWK (TEST ONLY)
}

// FUNCTION: EXE 0x5ce8c5
// Researches the epoch's nodes of one kind (only those researched with the epoch when asked)
void EETechTreeEpoch::ResearchKind(long inKind, bool inEpochOnly)
{
    std::map<long, EETechTreeNode*>::const_iterator theNode;
    EETechCaptureWalkBegin();   // TNWK (TEST ONLY)
    theNode = this->mNodes.begin();
    while (theNode != this->mNodes.end()) {
        EETechTreeNode* theTreeNode = theNode->second;
        bool theTakes = theTreeNode->mData->mKind == inKind && (!inEpochOnly || theTreeNode->mData->mResearchedWithEpoch);
        EETechCaptureWalkStep(theNode->first, theTakes ? 1 : 0);   // TNWK (TEST ONLY)
        if (theTakes)
            theTreeNode->ResearchNode(NULL);
        ++theNode;
    }
    EETechCaptureWalk(2, this->mTree, this, inKind, inEpochOnly ? 1 : 0, 0);   // TNWK (TEST ONLY)
}

// FUNCTION: EXE 0x5ce918
// Makes the epoch available and researches it
void EETechTreeEpoch::ResearchSelf()
{
    if (this->mIsAvailable == false)
        this->mIsAvailable = true;
    this->Research(this->mData->mID, NULL);
}

// FUNCTION: EXE 0x5ce930
// Researches a node of the epoch, or the epoch itself: fires its event, makes it current, obsoletes outdated nodes;
// returns 2 when already researched, 9 when not possible
long EETechTreeEpoch::Research(long inID, EEComplexUnit* inUnit)
{
    if (EESimCaptureRecord::Active()) {   // TEST ONLY (ee/EETechCapture.h)
        EESimCaptureRecord theRecord(EECaptureTag("TNRS"));
        theRecord.U32(1);
        theRecord.I32(EETechCapturePlayerIndex(this->mTree));
        theRecord.I32(inID);
        EETechCaptureEpoch(theRecord, this);
        long theResult = this->ResearchUncaptured(inID, inUnit);
        theRecord.I32(theResult);
        EETechCaptureEpoch(theRecord, this);
        theRecord.Commit();
        return theResult;
    }
    return this->ResearchUncaptured(inID, inUnit);
}

long EETechTreeEpoch::ResearchUncaptured(long inID, EEComplexUnit* inUnit)
{
    std::map<long, EETechTreeNode*>::iterator theNode;
    long theResult;
    if (inID != this->mData->mID) {
        theNode = this->mNodes.find(inID);
        if (theNode != this->mNodes.end())
            theResult = theNode->second->ResearchNode(inUnit);
        else
            theResult = 9;
    } else {
        // the epoch itself: ee_tech research_epoch decides (2 already, 0 research it, 9 not possible)
        EeTechNode theEpoch = EETechNodeView(this);
        EeTechTreeState theTree = EETechTreeView(this->mTree);
        EeTechResearch theDecision = ee_tech_research_epoch(&theEpoch, &theTree);
        theResult = theDecision.result;
        if (!theDecision.research)
            return theResult;
        this->mIsResearched = true;
        theNode = this->mNodes.begin();
        while (theNode != this->mNodes.end()) {
            theNode->second->SetPrerequisite(this->mData->mID);
            ++theNode;
        }
        EEEvent::Trigger(this->mData->mResearchedEvent, this->mTree->mPlayerIndex, this->mData->mID, this->mTree->mPlayer, inUnit);
        this->mTree->mCurrentEpoch = this->mData->mID;
        this->mTree->ObsoleteOutdatedNodes();
        this->mTree->UpdateInterface(true);
        theResult = 0;
        this->mTree->OnResearched(this->mData->mID, NULL);
        this->CheckUnitMedia(this->mTree->GetPlayer());
    }
    return theResult;
}

// FUNCTION: EXE 0x5cea3e
// Writes the epoch's researched-tech count to the saved game
long EETechTreeEpoch::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return outFile.Write(this->mCurrentSOL);
}

// FUNCTION: EXE 0x5cea53
// Unloads the unit class media the epoch loaded and the epoch's own media
void EETechTreeEpoch::UnloadMedia(EEPlayer* inPlayer)
{
    unsigned long theIndex;
    for (theIndex = 0; theIndex < this->mLoadedUnitClasses.size(); ++theIndex) {
        inPlayer->UnloadUnitClassMedia(this->mLoadedUnitClasses[theIndex]);
    }
    this->mLoadedUnitClasses.clear();
    EETechTreeNode::UnloadMedia(inPlayer);
}

// FUNCTION: EXE 0x5cea9a
// Unloads the media of the epoch's nodes of one kind
void EETechTreeEpoch::UnloadMediaOfKind(long inKind)
{
    std::map<long, EETechTreeNode*>::const_iterator theNode;
    theNode = this->mNodes.begin();
    while (theNode != this->mNodes.end()) {
        EETechTreeNode* theTreeNode = theNode->second;
        if (theTreeNode->mData->mKind == inKind)
            theTreeNode->UnloadMedia(this->mTree->GetPlayer());
        ++theNode;
    }
}

// FUNCTION: EXE 0x5ceaeb
// Returns the epoch (for its own id) or one of its nodes, or NULL
EETechTreeNode* EETechTreeEpoch::Get(long inID)
{
    EETechTreeNode* theResult = NULL;
    if (inID == this->mData->mID) {
        theResult = this;
    } else {
        std::map<long, EETechTreeNode*>::iterator theNode = this->mNodes.find(inID);
        if (theNode != this->mNodes.end())
            theResult = theNode->second;
    }
    return theResult;
}

// FUNCTION: EXE 0x5ceb26
// Copies another epoch's state and deep-copies its nodes for this tree (the node data stays shared)
EETechTreeEpoch& EETechTreeEpoch::operator=(const EETechTreeEpoch& inOther)
{
    std::map<long, EETechTreeNode*>::const_iterator theNode;
    this->mData = inOther.mData;
    this->mEpoch = inOther.mEpoch;
    this->mObsoleteEpoch = inOther.mObsoleteEpoch;
    this->mIsResearched = inOther.mIsResearched;
    this->mIsObsolete = inOther.mIsObsolete;
    this->mIsAvailable = inOther.mIsAvailable;
    this->mFirstNodeID = inOther.mFirstNodeID;
    this->mRequiredSOL = inOther.mRequiredSOL;
    this->mIsAllowed = inOther.mIsAllowed;
    this->mButton = inOther.mButton;
    this->mCurrentSOL = inOther.mCurrentSOL;
    this->mNextEpoch = inOther.mNextEpoch;
    this->mPrerequisiteFlags = inOther.mPrerequisiteFlags;
    this->mIsResearched = inOther.mIsResearched;
    this->mIsObsolete = inOther.mIsObsolete;
    this->mIsAvailable = inOther.mIsAvailable;
    this->mIsCountingSOL = false;
    this->DeleteNodes();
    theNode = inOther.mNodes.begin();
    while (theNode != inOther.mNodes.end()) {
        EETechTreeNode* theCopy = new EETechTreeNode;
        *theCopy = *theNode->second;
        theCopy->mTree = this->mTree;
        this->mNodes.insert(std::map<long, EETechTreeNode*>::value_type(theCopy->mData->mID, theCopy));
        ++theNode;
    }
    return *this;
}

// FUNCTION: EXE 0x5cec0e
// Destroys a class-cost map
// (std::map<long, long*>'s dtor: _Tree::~_Tree, Dinkum <xtree>)

// FUNCTION: EXE 0x5cec39
// Destroys a node map
// (std::map<long, EETechTreeNode*>'s dtor: _Tree::~_Tree, Dinkum <xtree>)

// FUNCTION: EXE 0x5cec64
// Empties a node map
// (std::map<long, EETechTreeNode*>::clear, Dinkum <xtree>)

// FUNCTION: EXE 0x5cec78
// Erases a range of a class-cost map
// (std::map<long, long*>'s _Tree::erase(first, last), Dinkum <xtree>)

// FUNCTION: EXE 0x5cece9
// Erases a range of a node map
// (std::map<long, EETechTreeNode*>'s _Tree::erase(first, last), Dinkum <xtree>)

// FUNCTION: EXE 0x5ced5a
// Erases a subtree of a class-cost map
// (std::map<long, long*>'s _Tree::_Erase, Dinkum <xtree>)

// FUNCTION: EXE 0x5ced8c
// Erases a subtree of a node map
// (std::map<long, EETechTreeNode*>'s _Tree::_Erase, Dinkum <xtree>)

// FUNCTION: EXE 0x5cedbe
// Returns the game variant of the current game options
// (EEWorld::GetGameVariant, inline in ee/EEWorld.h)

// SYNTHETIC: EXE 0x5cdf25
// SYNTHETIC: EXE 0x5cdf2f
// SYNTHETIC: EXE 0x5cdf40
// SYNTHETIC: EXE 0x5cdf4c
