// C:\EEX\Empire Earth\Tech Tree\EETechTreeNode.cpp (file name inferred: no __FILE__; RTTI EETechTreeNode >
// RTSTechTreeNode, vtables 0x446150 / 0x446174). One node of a player's tech tree: 0x1cedce..0x1cfeef (the ctor to the
// inline RTSTechTreeNode members that close the unit; the next unit, EEUIECampaignEditorEvents.cpp, opens with its
// globals chain 0x1cfeef). Classes in ee/EEPlayer.h. Unit flags: the exe's module set.

#include <windows.h>
#include <stdlib.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/UResourceFile.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGameVariant.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEEvent.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEButtonObject.h"
#include "ee/EEAEManager.h"
#include "ee/EmpireEarth.h"
#include "ee/EEStringIDs.h"
#include "ee/EETechCapture.h"
#include "ee/EETechRust.h"
// Creates an empty tech node, allowed and obsolete only after the space epoch
EETechTreeNode::EETechTreeNode()
    : mTree(NULL), mButton(NULL)
{
    // the record's values (allowed, obsolete after the space epoch): RTSTechTreeNode's ee_tech_node_fields_init
}

// Returns the node's research time
long EETechTreeNode::GetBuildTime()
{
    return this->mData->mBuildTime;
}

// Destroys the node, unloading its unit media and giving its unit classes their default buttons back
EETechTreeNode::~EETechTreeNode()
{
    if (this->mIsUnitMediaLoaded == true) {
        if (this->mTree->GetPlayer() != NULL)
            this->mTree->GetPlayer()->UnloadUnitClassMedia(this->mData->mUnitClass);
    }
    if (this->mData != NULL) {
        for (unsigned long i = 0; i < this->mData->mUnitClasses.size(); i++) {
            if (this->mTree->GetPlayer() != NULL)
                this->mTree->GetPlayer()->SwapClassButtonMedia(this->mData->mUnitClasses[i], this->mButton, NULL);
        }
    }
    this->ClearClassCosts();
}

// Frees the node's per-unit-class cost tables
void EETechTreeNode::ClearClassCosts()
{
    std::map<long, long*>::iterator theCosts;
    if (this->mData == NULL)
        return;
    theCosts = this->mData->mClassCosts.begin();
    while (theCosts != this->mData->mClassCosts.end()) {
        delete [] (*theCosts).second;
        ++theCosts;
    }
    this->mData->mClassCosts.clear();
}

// Deletes the node's data
long EETechTreeNode::Unload()
{
    if (this->mData != NULL) {
        delete this->mData;
        this->mData = NULL;
    }
    return 0;
}

// Returns a node's cost in a resource for a unit class: per-class cost, scaled for unit-making nodes by wonder
// epoch, cost set and upgrade cost scale, and by the just-in-time manufacturing factor for rally-point classes
long EETechTreeNode::GetCost(long inResource, long inUnitClass, float inScale)
{
    long theCost = this->GetCostUncaptured(inResource, inUnitClass, inScale);
    if (EESimCaptureRecord::Active())   // TEST ONLY (ee/EETechCapture.h)
        EETechCaptureCost(this, 0, inResource, inUnitClass, inScale, theCost);
    return theCost;
}

long EETechTreeNode::GetCostUncaptured(long inResource, long inUnitClass, float inScale)
{
    // the arithmetic is the Rust tech logic's (ee_tech tree.rs node_cost); this reads the factors where the C++ read
    // them (a unit-making node's only for one)
    EEPlayer* thePlayer = this->mTree->GetPlayer();
    std::map<long, long*>::iterator theCosts = this->mData->mClassCosts.find(inUnitClass);
    EeTechCostInput theInput{};
    if (theCosts != this->mData->mClassCosts.end())
        theInput.cost = (int32_t)theCosts->second[inResource];
    else
        theInput.cost = (int32_t)this->mData->mCosts[inResource];
    theInput.scale = inScale;
    theInput.makes_unit = this->mData->mMakesUnit;
    if (this->mData->mMakesUnit) {
        EEUnitStats* theStats = thePlayer->mUnitClassUpgrades[this->mData->mUnitClass];
        theInput.wonder_epoch_scaled = this->mData->mKind == 4 && this->mEpoch != 0;
        if (theInput.wonder_epoch_scaled) {
            theInput.epochs_since = (int32_t)(this->mTree->mCurrentEpoch - this->mEpoch);
            theInput.epoch_cost_factor = gWorldTable.GetFloatValue(kWorldUnitEpochCostFactor);
        }
        EEGameVariant* theVariant = EEGameVariant::GetGameVariant(gWorld.mGameOptions.mVariant);
        EEUnitSet* theSet = gUnitSetManager.GetSet(theVariant->mCostSet);
        theInput.in_cost_set = theSet != NULL && theSet->Contains(this->mData->mUnitClass);
        if (theInput.in_cost_set)
            theInput.cost_set_factor = theVariant->GetCostSetFactor();
        theInput.has_upgrade = theStats != NULL;
        if (theStats != NULL)
            theInput.cost_scale = theStats->GetCostScale();
        theInput.jit = thePlayer->mJITManufacturing && gUnitClassTable[inUnitClass] != NULL &&
                       gUnitClassTable[inUnitClass]->HasRallyPoint();
        if (theInput.jit)
            theInput.jit_factor = gWorld.mGameOptions.mResources == 4
                                      ? gWorldTable.GetFloatValue(kWorldJitManufacturingCostFactorDeathmatch)
                                      : gWorldTable.GetFloatValue(kWorldJitManufacturingCostFactor);
    }
    return ee_tech_node_cost(&theInput);
}

// Builds the "Food: n Gold: n ..." cost text of the node for a unit class (wood shown as carbon when used)
void EETechTreeNode::GetCostString(UWideString& outString, long inUnitClass, float inScale)
{
    UWideString theName;
    char theNumber[32];
    outString = "";
    if (this->GetCost(kResourceFood, inUnitClass, 1.0f) > 0) {
        gResource->GetString(IDS_RESOURCE_NAME_FOOD, theName);
        outString = theName;
        outString += ": ";
        PlatIToA(this->GetCost(kResourceFood, inUnitClass, inScale), theNumber, 10);
        outString += theNumber;
        outString += " ";
    }
    if (this->GetCost(kResourceGold, inUnitClass, 1.0f) > 0) {
        gResource->GetString(IDS_RESOURCE_NAME_GOLD, theName);
        outString += theName;
        outString += ": ";
        PlatIToA(this->GetCost(kResourceGold, inUnitClass, inScale), theNumber, 10);
        outString += theNumber;
        outString += " ";
    }
    if (this->GetCost(kResourceIron, inUnitClass, 1.0f) > 0) {
        gResource->GetString(IDS_RESOURCE_NAME_IRON, theName);
        outString += theName;
        outString += ": ";
        PlatIToA(this->GetCost(kResourceIron, inUnitClass, inScale), theNumber, 10);
        outString += theNumber;
        outString += " ";
    }
    if (this->GetCost(kResourceStone, inUnitClass, 1.0f) > 0) {
        gResource->GetString(IDS_RESOURCE_NAME_STONE, theName);
        outString += theName;
        outString += ": ";
        PlatIToA(this->GetCost(kResourceStone, inUnitClass, inScale), theNumber, 10);
        outString += theNumber;
        outString += " ";
    }
    if (!(this->GetCost(kResourceWood, inUnitClass, 1.0f) > 0))
        return;
    if (gWorld.mUsingCarbon)
        gResource->GetString(IDS_CARBON, theName);
    else
        gResource->GetString(IDS_RESOURCE_NAME_WOOD, theName);
    outString += theName;
    outString += ": ";
    PlatIToA(this->GetCost(kResourceWood, inUnitClass, inScale), theNumber, 10);
    outString += theNumber;
    outString += " ";
}

// Reads the node's record from the tech tree file, with a cost table per unit class scaled by its factor
long EETechTreeNode::Load(FSFile& inFile, EETechTree* inTree)
{
    unsigned long theRead = 0;
    unsigned long theCount = 0;
    EeTechTreeRecord theRecord;
    this->mData = new RTSTechTreeNodeData;
    this->mTree = inTree;
    inFile.Read(reinterpret_cast<char*>(&theRecord), sizeof(theRecord), theRead);
    long theResult = inFile.Read(theCount);
    this->mData->mUnitClasses.resize(theCount, 0);
    for (unsigned long j = 0; j < theCount; j++)
        inFile.Read(this->mData->mUnitClasses[j]);
    // the record fields, the obsolete epoch, the editor flag of a node obsoleted by default, the empty
    // prerequisites flags, no research delay (ee_tech nodes.rs fill_data, fill_node)
    ee_tech_fill_from_record(this, this->mData, &theRecord, false);
    for (unsigned long k = 0; k < this->mData->mUnitClasses.size(); k++) {
        // the k-th class's table: every cost times the record's factor of that class (ee_tech class_costs)
        long* theCosts = new long[7];
        int32_t theBase[7];
        int32_t theTable[7];
        for (long r = 0; r < 7; r++)
            theBase[r] = (int32_t)this->mData->mCosts[r];
        float theFactors[4] = {theRecord.mFactor1, theRecord.mFactor2, theRecord.mFactor3, theRecord.mFactor4};
        ee_tech_class_costs(theBase, (uint32_t)k, theFactors, theTable);
        for (long r = 0; r < 7; r++)
            theCosts[r] = theTable[r];
        this->mData->mClassCosts.insert(std::map<long, long*>::value_type(this->mData->mUnitClasses[k], theCosts));
    }
    this->mEpoch = EETechTreeEpochOf(this->mData->mID);   // a node's: set by its epoch's Load (the block holding it)
    this->mButton = EEButtonObject::Get(theRecord.mButton);
    return theResult;
}

// Tells whether the node can be researched now (wonders only after enough epochs or in the last epoch)
bool EETechTreeNode::GetIsAvailable()
{
    return EETechNodeWalk(this, EeTechNodeOp::IsAvailable) != 0;
}

// Tells whether the node lists a unit class
bool EETechTreeNode::Contains(long inUnitClass)
{
    std::vector<unsigned long>::iterator theUnitClass;
    theUnitClass = std::find(this->mData->mUnitClasses.begin(), this->mData->mUnitClasses.end(), inUnitClass);
    return theUnitClass != this->mData->mUnitClasses.end();
}

// Tells whether the node is obsolete, by flag or because its obsolete epoch has passed
bool EETechTreeNode::GetIsObsolete()
{
    return EETechNodeWalk(this, EeTechNodeOp::IsObsolete) != 0;
}

// Tells whether the node makes a building or mine whose media this game needs
bool EETechTreeNode::NeedsUnitMedia()
{
    bool theResult = this->NeedsUnitMediaUncaptured();
    EETechCaptureWalkBegin();   // TNWK (TEST ONLY)
    long theUnitClass = this->mData->mUnitClass;
    EEUnitClass* theClass = theUnitClass > 0 ? gUnitClassTable[theUnitClass] : NULL;
    EETechCaptureWalk(10, NULL, this, theUnitClass, theClass != NULL ? theClass->mFamily : -1, theResult ? 1 : 0);
    return theResult;
}

bool EETechTreeNode::NeedsUnitMediaUncaptured()
{
    // the rule is Rust (ee_tech tree.rs needs_unit_media): the node's class (none for an id 0 or less or an empty
    // table entry) and its family
    long theUnitClass = this->mData->mUnitClass;
    EEUnitClass* theClass = theUnitClass > 0 ? gUnitClassTable[theUnitClass] : NULL;
    return ee_tech_needs_unit_media(theClass != NULL, theClass != NULL ? (int32_t)theClass->mFamily : -1, this->mScenEditorObsoleted,
                                    (int32_t)this->mObsoleteEpoch, (int32_t)this->mTree->mStartEpoch);
}

// Tells whether the node depends on a tech, directly or through its prerequisites (ee_tech depends_on)
bool EETechTreeNode::DependsOn(long inID)
{
    return EETechNodeWalk(this, EeTechNodeOp::DependsOn, inID) != 0;
}

// Loads the node's media: the unit class and its buttons, and the media its research event uses
void EETechTreeNode::LoadMedia(EEPlayer* inPlayer)
{
    if (this->mData->mMakesUnit) {
        inPlayer->SetUnitClassMedia(this->mData->mUnitClass, 0x7f);
        inPlayer->LoadUnitClassButtons(this->mData->mUnitClass);
    }
    EEEvent* theEvent = EEEvent::Get(this->mData->mResearchedEvent);
    if (theEvent != NULL) {
        for (unsigned long i = 0; i < theEvent->GetNumEffects(); i++) {
            EEEventEffect* theEffect = EEEvent::GetEventEffect(theEvent->GetEffect(i));
            switch (theEffect->mKind) {
            case 1:
                EEButtonObject::Get(theEffect->mButton)->Acquire();
                break;
            case 2:
                if (theEffect->mAttribute == 0x28)
                    gAEManager.LoadTemplateMedia(theEffect->mAreaEffect);
                break;
            case 6:
                EEMedia::Get(theEffect->mMediaIndex)->LoadMedia();
                break;
            case 0xf:
            case 0x10:
            case 0x11:
            case 0x12:
            case 0x14:
            case 0x15:
                EESoundObject::Get(theEffect->mSoundID)->LoadMedia();
                break;
            case 0x13:
                inPlayer->SetUnitClassMedia(theEffect->mNewUnitClass, 0x7f);
                break;
            }
            PlatSleep(1);
        }
    }
    inPlayer->EpochMediaLoaded(this->mData->mID);
}

// Satisfies one prerequisite (an epoch or one of the four techs); with all met the node becomes available
// (event 0x7d1, statistics series 0x16)
void EETechTreeNode::SetPrerequisite(long inID)
{
    if (EESimCaptureRecord::Active()) {   // TEST ONLY (ee/EETechCapture.h)
        EESimCaptureRecord theRecord(EECaptureTag("TNPR"));
        theRecord.I32(EETechCapturePlayerIndex(this->mTree));
        EETechCaptureNode(theRecord, this);
        theRecord.I32(inID);
        EETechNodeWalk(this, EeTechNodeOp::SetPrerequisite, inID);
        EETechCaptureNode(theRecord, this);
        theRecord.Commit();
        return;
    }
    EETechNodeWalk(this, EeTechNodeOp::SetPrerequisite, inID);
}

// Makes the node available with every prerequisite met
void EETechTreeNode::MakeAvailable()
{
    EETechNodeWalk(this, EeTechNodeOp::MakeAvailable);
}

// Researches the node if available: fires its event; returns 2 when already researched (repeatable kind 3 excepted),
// 9 when not possible; counts down a research delay
long EETechTreeNode::ResearchNode(EEComplexUnit* inUnit)
{
    if (EESimCaptureRecord::Active()) {   // TEST ONLY (ee/EETechCapture.h)
        EESimCaptureRecord theRecord(EECaptureTag("TNRS"));
        theRecord.U32(0);
        theRecord.I32(EETechCapturePlayerIndex(this->mTree));
        EETechCaptureNode(theRecord, this);
        long theResult = EETechNodeWalk(this, EeTechNodeOp::Research, 0, false, inUnit);
        theRecord.I32(theResult);
        EETechCaptureNode(theRecord, this);
        theRecord.Commit();
        return theResult;
    }
    return EETechNodeWalk(this, EeTechNodeOp::Research, 0, false, inUnit);
}

// Sets the scenario editor's obsoleted flag; recursively obsoletes the nodes depending on it, or un-obsoletes
// its prerequisites
void EETechTreeNode::SetScenEditorObsoleted(bool inValue, bool inRecurse)
{
    EETechNodeWalk(this, EeTechNodeOp::SetEditorObsoleted, inValue ? 1 : 0, inRecurse);
}

// Makes the node obsolete (obsolete event, unloading its unit media unless kept) or clears the flag
void EETechTreeNode::SetObsolete(bool inValue, bool inKeepMedia)
{
    EETechNodeWalk(this, EeTechNodeOp::SetObsolete, inValue ? 1 : 0, inKeepMedia);
}

// Gives the node a new button and swaps it into its unit classes
void EETechTreeNode::SetButton(unsigned long inButton)
{
    for (unsigned long i = 0; i < this->mData->mUnitClasses.size(); i++) {
        this->mTree->GetPlayer()->SwapClassButtonMedia(this->mData->mUnitClasses[i], this->mButton, EEButtonObject::Get(inButton));
    }
    this->mButton = EEButtonObject::Get(inButton);
}

// Unloads the node's media: the unit class and its buttons, and the media its research event uses
void EETechTreeNode::UnloadMedia(EEPlayer* inPlayer)
{
    if (this->mData->mMakesUnit) {
        inPlayer->UnloadUnitClassMedia(this->mData->mUnitClass);
        inPlayer->UnloadUnitClassButtons(this->mData->mUnitClass);
    }
    EEEvent* theEvent = EEEvent::Get(this->mData->mResearchedEvent);
    if (theEvent != NULL) {
        for (unsigned long i = 0; i < theEvent->GetNumEffects(); i++) {
            EEEventEffect* theEffect = EEEvent::GetEventEffect(theEvent->GetEffect(i));
            switch (theEffect->mKind) {
            case 1:
                EEButtonObject::Get(theEffect->mButton)->Release();
                break;
            case 2:
                // Release the area-effect template media LoadMedia loaded (this called LoadTemplateMedia again).
                if (theEffect->mAttribute == 0x28)
                    gAEManager.UnloadTemplateMedia(theEffect->mAreaEffect);
                break;
            case 6:
                EEMedia::Get(theEffect->mMediaIndex)->UnloadMedia();
                break;
            case 0xf:
            case 0x10:
            case 0x11:
            case 0x12:
            case 0x14:
            case 0x15:
                EESoundObject::Get(theEffect->mSoundID)->UnloadMedia();
                break;
            case 0x13:
                inPlayer->UnloadUnitClassMedia(theEffect->mNewUnitClass);
                break;
            }
        }
    }
    inPlayer->EpochMediaUnloaded(this->mData->mID);
}

// Copies another node's state and shares its data (with its class cost tables)
EETechTreeNode& EETechTreeNode::operator=(const EETechTreeNode& inOther)
{
    if (&inOther != this) {
        // the record's fields: ee_tech nodes.rs assign (the original's copy of mData->mMakesUnit onto the same
        // shared data was a self-assignment, dropped)
        this->mData = inOther.mData;
        ee_tech_node_assign(this, &inOther, false);
        // mData is now inOther's (shared) block, which already holds the class cost tables: clearing and
        // re-copying them into the same map would empty them for every node sharing the block
        this->mButton = inOther.mButton;
    }
    return *this;
}
