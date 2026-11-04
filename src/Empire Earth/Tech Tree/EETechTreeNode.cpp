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
#include "ee/EETechTreeRecord.h"
#include "ee/EEStringIDs.h"
// FUNCTION: EXE 0x5cedce
// Creates an empty tech node, allowed and obsolete only after the space epoch
EETechTreeNode::EETechTreeNode()
    : mTree(NULL), mButton(NULL), mEpoch(0), mObsoleteEpoch(kTechEpochSpace), mResearchDelay(0), mIsAllowed(true), mScenEditorObsoleted(false), mIsUnitMediaLoaded(false)
{
}

// FUNCTION: EXE 0x5cee02
// Returns the node's research time
long EETechTreeNode::GetBuildTime()
{
    return this->mData->mBuildTime;
}

// SYNTHETIC: EXE 0x5cee09
// (the scalar deleting destructor, emitted with the vtable; slot EETechTreeNode+0x0)

// FUNCTION: EXE 0x5cee25
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

// FUNCTION: EXE 0x5ceeb3
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

// FUNCTION: EXE 0x5ceefc
// Deletes the node's data
long EETechTreeNode::Unload()
{
    if (this->mData != NULL) {
        delete this->mData;
        this->mData = NULL;
    }
    return 0;
}

// FUNCTION: EXE 0x5cef26
// Returns a node's cost in a resource for a unit class: per-class cost, scaled for unit-making nodes by wonder
// epoch, cost set and upgrade cost scale, and by the just-in-time manufacturing factor for rally-point classes
long EETechTreeNode::GetCost(long inResource, long inUnitClass, float inScale)
{
    EEPlayer* thePlayer = this->mTree->GetPlayer();
    std::map<long, long*>::iterator theCosts = this->mData->mClassCosts.find(inUnitClass);
    long theCost;
    if (theCosts != this->mData->mClassCosts.end())
        theCost = theCosts->second[inResource];
    else
        theCost = this->mData->mCosts[inResource];
    if (this->mData->mMakesUnit) {
        EEUnitStats* theStats = thePlayer->mUnitClassUpgrades[this->mData->mUnitClass];
        if (this->mData->mKind == 4 && this->mEpoch != 0) {
            float theFactor = gWorldTable.GetFloatValue(80);
            inScale = ((this->mTree->mCurrentEpoch - this->mEpoch) * theFactor + 1.0f) * inScale;
        }
        if (gUnitSetManager.GetSet(EEGameVariant::GetGameVariant(gWorld.mGameOptions.mVariant)->mCostSet) != NULL
            && gUnitSetManager.GetSet(EEGameVariant::GetGameVariant(gWorld.mGameOptions.mVariant)->mCostSet)
                   ->Contains(this->mData->mUnitClass))
            inScale = EEGameVariant::GetGameVariant(gWorld.mGameOptions.mVariant)->GetCostSetFactor() * inScale;
        if (theStats != NULL)
            theCost = (long)(theStats->GetCostScale() * theCost * inScale + 0.0001f);
        if (thePlayer->mJITManufacturing && gUnitClassTable[inUnitClass] != NULL && gUnitClassTable[inUnitClass]->HasRallyPoint()) {
            if (gWorld.mGameOptions.mResources == 4)
                theCost = (long)(gWorldTable.GetFloatValue(93) * theCost);
            else
                theCost = (long)(gWorldTable.GetFloatValue(90) * theCost);
        }
    } else {
        theCost = (long)(theCost * inScale + 0.0001f);
    }
    return theCost;
}

// FUNCTION: EXE 0x5cf0a2
// Tells whether the unit class has a rally point
// (EEUnitClass::HasRallyPoint, inline in ee/EEUnit.h)

// FUNCTION: EXE 0x5cf0e2
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
        _itoa(this->GetCost(kResourceFood, inUnitClass, inScale), theNumber, 10);
        outString += theNumber;
        outString += " ";
    }
    if (this->GetCost(kResourceGold, inUnitClass, 1.0f) > 0) {
        gResource->GetString(IDS_RESOURCE_NAME_GOLD, theName);
        outString += theName;
        outString += ": ";
        _itoa(this->GetCost(kResourceGold, inUnitClass, inScale), theNumber, 10);
        outString += theNumber;
        outString += " ";
    }
    if (this->GetCost(kResourceIron, inUnitClass, 1.0f) > 0) {
        gResource->GetString(IDS_RESOURCE_NAME_IRON, theName);
        outString += theName;
        outString += ": ";
        _itoa(this->GetCost(kResourceIron, inUnitClass, inScale), theNumber, 10);
        outString += theNumber;
        outString += " ";
    }
    if (this->GetCost(kResourceStone, inUnitClass, 1.0f) > 0) {
        gResource->GetString(IDS_RESOURCE_NAME_STONE, theName);
        outString += theName;
        outString += ": ";
        _itoa(this->GetCost(kResourceStone, inUnitClass, inScale), theNumber, 10);
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
    _itoa(this->GetCost(kResourceWood, inUnitClass, inScale), theNumber, 10);
    outString += theNumber;
    outString += " ";
}

// FUNCTION: EXE 0x5cf370
// Reads the node's record from the tech tree file, with a cost table per unit class scaled by its factor
long EETechTreeNode::Load(FSFile& inFile, EETechTree* inTree)
{
    unsigned long theRead = 0;
    unsigned long theCount = 0;
    EETechTreeRecord theRecord;
    this->mData = new RTSTechTreeNodeData;
    for (long i = 0; i < 7; i++)
        this->mData->mCosts[i] = 0;
    this->mTree = inTree;
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
    this->mData->mDefaultUnitClass = theRecord.mDefaultUnitClass;
    this->mData->mCountsTowardEpoch = theRecord.mCountsTowardEpoch;
    this->mData->mNameID = theRecord.mNameID;
    this->mData->mKind = theRecord.mKind;
    this->mData->mCostResource = theRecord.mCostResource;
    this->mData->mRequiredStatistic = theRecord.mRequiredStatistic;
    this->mData->mRequiredStatisticValue = theRecord.mRequiredStatisticValue;
    this->mData->mCalamity = theRecord.mCalamity;
    this->mData->mHotKey = theRecord.mHotKey;
    this->mData->mObsoletedByDefault = theRecord.mObsoletedByDefault;
    this->mData->mShowInEditor = theRecord.mShowInEditor;
    this->mData->mAvailableWithEpoch = theRecord.mAvailableWithEpoch;
    this->mData->mResearchedWithEpoch = theRecord.mResearchedWithEpoch;
    for (unsigned long k = 0; k < this->mData->mUnitClasses.size(); k++) {
        long* theCosts = new long[7];
        float theFactor;
        switch (k) {
        case 1:
            theFactor = theRecord.mFactor1;
            break;
        case 2:
            theFactor = theRecord.mFactor2;
            break;
        case 3:
            theFactor = theRecord.mFactor3;
            break;
        case 4:
            theFactor = theRecord.mFactor4;
            break;
        default:
            theFactor = 1.0f;
            break;
        }
        theCosts[0] = (long)(this->mData->mCosts[0] * theFactor);
        theCosts[1] = (long)(this->mData->mCosts[1] * theFactor);
        theCosts[4] = (long)(this->mData->mCosts[kResourceStone] * theFactor);
        theCosts[5] = (long)(this->mData->mCosts[kResourceGold] * theFactor);
        theCosts[3] = (long)(this->mData->mCosts[kResourceWood] * theFactor);
        theCosts[2] = (long)(this->mData->mCosts[kResourceFood] * theFactor);
        theCosts[6] = (long)(this->mData->mCosts[kResourceIron] * theFactor);
        this->mData->mClassCosts.insert(std::map<long, long*>::value_type(this->mData->mUnitClasses[k], theCosts));
    }
    if (this->mData->mObsoletedByDefault == true)
        this->mScenEditorObsoleted = true;
    this->mEpoch = (this->mData->mID >= kTechEpochPrehistoric && this->mData->mID < kTechEpochAll) ? this->mData->mID : this->mData->mID / 1000 - 1;
    this->mButton = EEButtonObject::Get(theRecord.mButton);
    this->mPrerequisiteFlags = 0;
    if (this->mData->mPrerequisite1 == 0)
        this->mPrerequisiteFlags |= kPrerequisitePrerequisite1;
    if (this->mData->mPrerequisite2 == 0)
        this->mPrerequisiteFlags |= kPrerequisitePrerequisite2;
    if (this->mData->mPrerequisite3 == 0)
        this->mPrerequisiteFlags |= kPrerequisitePrerequisite3;
    if (this->mData->mPrerequisite4 == 0)
        this->mPrerequisiteFlags |= kPrerequisitePrerequisite4;
    this->mResearchDelay = 0;
    return theResult;
}

// FUNCTION: EXE 0x5cf686
// Tells whether the node can be researched now (wonders only after enough epochs or in the last epoch)
bool EETechTreeNode::GetIsAvailable()
{
    return this->mIsAvailable && !this->GetIsObsolete() && !this->mScenEditorObsoleted && this->mEpoch <= this->mTree->mCurrentEpoch
        && (this->mData->mMakesUnit || this->mData->mKind == 3 || !this->mIsResearched)
        && (this->mData->mKind != 4 || this->mTree->mCurrentEpoch == this->mTree->mEndEpoch
            || this->mTree->mCurrentEpoch - this->mTree->mStartEpoch >= this->mTree->mEpochsBeforeWonders);
}

// FUNCTION: EXE 0x5cf6e5
// Tells whether the node lists a unit class
bool EETechTreeNode::Contains(long inUnitClass)
{
    std::vector<unsigned long>::iterator theUnitClass;
    theUnitClass = std::find(this->mData->mUnitClasses.begin(), this->mData->mUnitClasses.end(), inUnitClass);
    return theUnitClass != this->mData->mUnitClasses.end();
}

// FUNCTION: EXE 0x5cf742
// Tells whether the node is obsolete, by flag or because its obsolete epoch has passed
bool EETechTreeNode::GetIsObsolete()
{
    return this->mIsObsolete || this->mObsoleteEpoch < this->mTree->mCurrentEpoch;
}

// FUNCTION: EXE 0x5cf75b
// Tells whether the node makes a building or mine whose media this game needs
bool EETechTreeNode::NeedsUnitMedia()
{
    long theUnitClass = this->mData->mUnitClass;
    if (theUnitClass <= 0)
        return false;
    EEUnitClass* theClass = gUnitClassTable[theUnitClass];
    if (theClass == NULL)
        return false;
    long theKind = theClass->mFamily;
    if (theKind == kUnitFamilyBuilding || theKind == kUnitFamilyMines) {
        if (!this->mScenEditorObsoleted && this->mObsoleteEpoch >= this->mTree->mStartEpoch)
            return true;
    }
    return false;
}

// FUNCTION: EXE 0x5cf79a
// Tells whether the node depends on a tech, directly or through its prerequisites
bool EETechTreeNode::DependsOn(long inID)
{
    bool theResult = inID == this->mData->mID;
    if (!theResult) {
        long thePrerequisite = this->mData->mPrerequisite1;
        theResult = inID == thePrerequisite;
        if (!theResult && thePrerequisite != this->mData->mID) {
            EETechTreeNode* theNode = this->mTree->Get(thePrerequisite);
            if (theNode != NULL) {
                theResult = theNode->DependsOn(inID);
                if (!theResult) {
                    thePrerequisite = this->mData->mPrerequisite2;
                    if (thePrerequisite != this->mData->mID) {
                        theResult = inID == thePrerequisite;
                        if (!theResult) {
                            theNode = this->mTree->Get(thePrerequisite);
                            if (theNode != NULL) {
                                theResult = theNode->DependsOn(inID);
                                if (!theResult) {
                                    thePrerequisite = this->mData->mPrerequisite3;
                                    if (thePrerequisite != this->mData->mID) {
                                        theResult = inID == thePrerequisite;
                                        if (!theResult) {
                                            theNode = this->mTree->Get(thePrerequisite);
                                            if (theNode != NULL) {
                                                theResult = theNode->DependsOn(inID);
                                                if (!theResult) {
                                                    thePrerequisite = this->mData->mPrerequisite4;
                                                    if (thePrerequisite != this->mData->mID) {
                                                        theResult = inID == thePrerequisite;
                                                        if (!theResult) {
                                                            theNode = this->mTree->Get(thePrerequisite);
                                                            if (theNode != NULL)
                                                                theResult = theNode->DependsOn(inID);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5cf882
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
            Sleep(1);
        }
    }
    inPlayer->EpochMediaLoaded(this->mData->mID);
}

// FUNCTION: EXE 0x5cf995
// Satisfies one prerequisite (an epoch or one of the four techs); with all met the node becomes available
// (event 0x7d1, statistics series 0x16)
void EETechTreeNode::SetPrerequisite(long inID)
{
    if (this->mIsAvailable || this->mIsObsolete)
        return;
    if (inID >= kTechEpochPrehistoric && inID < kTechEpochAll)
        this->mPrerequisiteFlags |= kPrerequisiteEpoch;
    else if (inID == this->mData->mPrerequisite1)
        this->mPrerequisiteFlags |= kPrerequisitePrerequisite1;
    else if (inID == this->mData->mPrerequisite2)
        this->mPrerequisiteFlags |= kPrerequisitePrerequisite2;
    else if (inID == this->mData->mPrerequisite3)
        this->mPrerequisiteFlags |= kPrerequisitePrerequisite3;
    else if (inID == this->mData->mPrerequisite4)
        this->mPrerequisiteFlags |= kPrerequisitePrerequisite4;
    if (this->mPrerequisiteFlags != kPrerequisiteAll)
        return;
    this->mIsAvailable = true;
    EEEvent::Trigger(0x7d1, this->mTree->mPlayerIndex, this->mData->mID, this->mTree->mPlayer, NULL);
    EEPlayer* thePlayer = this->mTree->GetPlayer();
    thePlayer->mSavedGroups.Reset(0x16, 1);
    this->mTree->UpdateInterface(false);
}

// FUNCTION: EXE 0x5cfa3c
// Makes the node available with every prerequisite met
void EETechTreeNode::MakeAvailable()
{
    this->mIsObsolete = false;
    this->mPrerequisiteFlags = kPrerequisiteAll;
    this->mIsAvailable = true;
    this->mTree->UpdateInterface(false);
}

// FUNCTION: EXE 0x5cfa53
// Researches the node if available: fires its event; returns 2 when already researched (repeatable kind 3 excepted),
// 9 when not possible; counts down a research delay
long EETechTreeNode::ResearchNode(EEComplexUnit* inUnit)
{
    long theResult;
    if (this->mData->mKind != 5 && this->mData->mID >= 1000 && this->mData->mID < 16000 && !this->mScenEditorObsoleted && this->mResearchDelay == 0) {
        if (this->mIsResearched && this->mData->mKind != 3) {
            theResult = 2;
        } else if (this->mIsAvailable) {
            this->mIsResearched = true;
            EEEvent::Trigger(this->mData->mResearchedEvent, this->mTree->mPlayerIndex, this->mData->mID, this->mTree->mPlayer, inUnit);
            theResult = 0;
            this->mTree->UpdateInterface(false);
            this->mTree->OnResearched(this->mData->mID, NULL);
        } else {
            theResult = 9;
        }
    } else {
        theResult = 9;
    }
    if (this->mResearchDelay > 0)
        this->mResearchDelay--;
    return theResult;
}

// FUNCTION: EXE 0x5cfae9
// Sets the scenario editor's obsoleted flag; recursively obsoletes the nodes depending on it, or un-obsoletes
// its prerequisites
void EETechTreeNode::SetScenEditorObsoleted(bool inValue, bool inRecurse)
{
    std::vector<long> theIDs;
    if (inValue != this->mScenEditorObsoleted) {
        if (inRecurse == true && this->mData->mID != kTechEpochPrehistoric) {
            this->mTree->GetNodeIDs(theIDs);
            if (inValue == true) {
                for (unsigned long i = 0; i < theIDs.size(); i++) {
                    EETechTreeNode* theNode = this->mTree->Get(theIDs[i]);
                    if (theNode == NULL)
                        continue;
                    RTSTechTreeNodeData* theData = theNode->mData;
                    long theID = this->mData->mID;
                    if (theData->mID != theID
                        && (theData->mPrerequisite1 == theID || theData->mPrerequisite2 == theID
                            || theData->mPrerequisite3 == theID || theData->mPrerequisite4 == theID)) {
                        theNode->SetScenEditorObsoleted(true, true);
                    }
                }
            } else {
                for (unsigned long i = 0; i < theIDs.size(); i++) {
                    EETechTreeNode* theNode = this->mTree->Get(theIDs[i]);
                    if (theNode == NULL)
                        continue;
                    RTSTechTreeNodeData* theData = this->mData;
                    long theID = theNode->mData->mID;
                    if (theID != theData->mID
                        && (theData->mPrerequisite1 == theID || theData->mPrerequisite2 == theID
                            || theData->mPrerequisite3 == theID || theData->mPrerequisite4 == theID)) {
                        theNode->SetScenEditorObsoleted(inValue, true);
                    }
                }
            }
        }
        this->mScenEditorObsoleted = inValue;
    }
}

// FUNCTION: EXE 0x5cfbfe
// Makes the node obsolete (obsolete event, unloading its unit media unless kept) or clears the flag
void EETechTreeNode::SetObsolete(bool inValue, bool inKeepMedia)
{
    if (inValue) {
        if (this->mIsObsolete == false) {
            this->mIsObsolete = true;
            EEEvent::Trigger(this->mData->mObsoleteEvent, this->mTree->mPlayerIndex, this->mData->mID, this->mTree->mPlayer, NULL);
            this->mTree->UpdateInterface(false);
            if (!inKeepMedia) {
                if (this->NeedsUnitMedia() == true && this->mIsUnitMediaLoaded == true) {
                    this->mTree->GetPlayer()->UnloadUnitClassMedia(this->mData->mUnitClass);
                    this->mIsUnitMediaLoaded = false;
                }
            }
        }
    } else {
        this->mIsObsolete = false;
    }
}

// FUNCTION: EXE 0x5cfc72
// Gives the node a new button and swaps it into its unit classes
void EETechTreeNode::SetButton(unsigned long inButton)
{
    for (unsigned long i = 0; i < this->mData->mUnitClasses.size(); i++) {
        this->mTree->GetPlayer()->SwapClassButtonMedia(this->mData->mUnitClasses[i], this->mButton, EEButtonObject::Get(inButton));
    }
    this->mButton = EEButtonObject::Get(inButton);
}

// FUNCTION: EXE 0x5cfcc9
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

// FUNCTION: EXE 0x5cfde2
// Copies another node's state and shares its data (with its class cost tables)
EETechTreeNode& EETechTreeNode::operator=(const EETechTreeNode& inOther)
{
    if (&inOther != this) {
        this->mData = inOther.mData;
        this->mEpoch = inOther.mEpoch;
        this->mObsoleteEpoch = inOther.mObsoleteEpoch;
        this->mPrerequisiteFlags = inOther.mPrerequisiteFlags;
        this->mData->mMakesUnit = inOther.mData->mMakesUnit;
        this->mIsResearched = inOther.mIsResearched;
        this->mIsObsolete = inOther.mIsObsolete;
        this->mIsAvailable = inOther.mIsAvailable;
        this->mIsAllowed = inOther.mIsAllowed;
        this->mIsUnitMediaLoaded = inOther.mIsUnitMediaLoaded;
        this->mScenEditorObsoleted = inOther.mScenEditorObsoleted;
        this->mResearchDelay = inOther.mResearchDelay;
        // mData is now inOther's (shared) block, which already holds the class cost tables: clearing and
        // re-copying them into the same map would empty them for every node sharing the block
        this->mButton = inOther.mButton;
    }
    return *this;
}

// FUNCTION: EXE 0x5cfeb6
// Empties a class-cost map
// (std::map<long, long*>::clear, Dinkum <xtree>)

// FUNCTION: EXE 0x5cfeca
// Tells whether the base node is available
// (RTSTechTreeNode::GetIsAvailable, inline in ee/EEPlayer.h)

// FUNCTION: EXE 0x5cfece
// Tells whether the base node is obsolete
// (RTSTechTreeNode::GetIsObsolete, inline in ee/EEPlayer.h)

// SYNTHETIC: EXE 0x5cfed2
// (RTSTechTreeNode's scalar deleting destructor: its inline empty dtor)
