// C:\EEX\Empire Earth\Tech Tree\EETechTree.cpp (__FILE__ 0x4c5c08; RTTI EETechTree > RTSTechTree, vtables 0x446114 /
// 0x446120). A player's tech tree: 0x1cba2f..0x1cdf25 (the eight key chains and sUnitClassInfo's chain, the ctor ..
// EEEvent::GetEffect's COMDAT; EETechTreeEpoch.cpp's key chain 0x1cdf25 opens the next unit). Classes in
// ee/EEPlayer.h. Unit flags: the exe's module set.

#include <windows.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEEvent.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EmpireEarth.h"
#include "ee/EEStringIDs.h"
const UString EETechTree::sNumEpochs("EETechTree-NumEpochs");
const UString EETechTree::sNumTechIDs("EETechTree-NumTechIDs");
const UString EETechTree::sTechNodeID("EETechTree-TechNodeID");
const UString EETechTree::sNodeIsResearched("EETechTree-NodeIsResearched");
const UString EETechTree::sNodeIsObsolete("EETechTree-NodeIsObsolete");
const UString EETechTree::sCurrentEpoch("EETechTree-CurrentEpoch");
const UString EETechTree::sStartEpoch("EETechTree-StartEpoch");
const UString EETechTree::sEndEpoch("EETechTree-EndEpoch");

// gTechTree's event effects by unit class (BuildUnitClassInfo 0x1cbe4a fills it; GetUnitClassMediaMask/IsLatestEffect read it). The
// ctor, copy ctor and dtor are user-declared inline (COMDATs 0x1cd48d/0x1cd503/0x1cd5cf after map::operator[]): the
// copy ctor default-constructs the vectors and assigns them, the dtor clears them before their own dtors.
// One vector per effect kind (provisional names).
struct EETechTreeUnitClassInfo {
    EETechTreeUnitClassInfo()
    {
        this->mTechnology = kTechNone;
    }
    EETechTreeUnitClassInfo(const EETechTreeUnitClassInfo& inOther)
    {
        this->mModelTechs = inOther.mModelTechs;
        this->mAcknowledgeSound1Techs = inOther.mAcknowledgeSound1Techs;
        this->mAcknowledgeSound2Techs = inOther.mAcknowledgeSound2Techs;
        this->mCreationSoundTechs = inOther.mCreationSoundTechs;
        this->mDeathSoundTechs = inOther.mDeathSoundTechs;
        this->mSelectionSound1Techs = inOther.mSelectionSound1Techs;
        this->mSelectionSound2Techs = inOther.mSelectionSound2Techs;
        this->mTechnology = inOther.mTechnology;
    }
    ~EETechTreeUnitClassInfo()
    {
        this->mModelTechs.clear();
        this->mAcknowledgeSound1Techs.clear();
        this->mAcknowledgeSound2Techs.clear();
        this->mCreationSoundTechs.clear();
        this->mDeathSoundTechs.clear();
        this->mSelectionSound1Techs.clear();
        this->mSelectionSound2Techs.clear();
    }

    std::vector<long> mModelTechs;          // +0x00
    std::vector<long> mAcknowledgeSound1Techs;  // +0x10
    std::vector<long> mAcknowledgeSound2Techs;  // +0x20
    std::vector<long> mCreationSoundTechs;  // +0x30
    std::vector<long> mDeathSoundTechs;     // +0x40
    std::vector<long> mSelectionSound1Techs;  // +0x50
    std::vector<long> mSelectionSound2Techs;  // +0x60
    long mTechnology;                       // +0x70 the kind-9 effect's technology (100000: none)
};

static std::map<long, EETechTreeUnitClassInfo> sUnitClassInfo;    // 0x562d40 (unguarded atexit dtor: not a class static)

// FUNCTION: EXE 0x5cbc87
// Reads a tech's researched, obsolete and editor-obsoleted flags; false for an unknown tech id
bool EETechTree::GetTechIDState(long inTechID, bool& outIsResearched, bool& outIsObsolete,
    bool& outScenEditorObsoleted)
{
    bool theResult = false;
    EETechTreeNode* theNode = this->Get(inTechID);
    if (theNode != NULL) {
        outIsResearched = theNode->mIsResearched;
        outIsObsolete = theNode->GetIsObsolete();
        outScenEditorObsoleted = theNode->mScenEditorObsoleted;
        theResult = true;
    }
    return theResult;
}

long gEpochCount = 15;
// the epoch of each node id: the block gTechTree loaded it in
static std::map<long, long> sNodeEpochs;

long EETechTreeEpochOf(long inID)
{
    if (EEIsEpochID(inID))
        return inID;
    std::map<long, long>::const_iterator it = sNodeEpochs.find(inID);
    return it != sNodeEpochs.end() ? it->second : -1;
}

void EERegisterTechNode(long inID, long inEpoch)
{
    sNodeEpochs[inID] = inEpoch;
}

long EEEpochTitleID(long inEpoch)
{
    if (EEIsEpochID(inEpoch) && inEpoch < gTechTree.mEpochs.size() && gTechTree.mEpochs[inEpoch].mTitleID != 0)
        return gTechTree.mEpochs[inEpoch].mTitleID;
    if (inEpoch == EEEpochCount())   // the game options' random start epoch
        return IDS_RANDOM_EPOCH;
    return IDS_EPOCH_NAME_FIRST + inEpoch;
}

// FUNCTION: EXE 0x5cbcc2
// Creates an empty tech tree spanning every epoch
EETechTree::EETechTree()
    : mPlayer(NULL), mCurrentEpoch(kTechEpochPrehistoric), mStartEpoch(kTechEpochPrehistoric), mEndEpoch(gEpochCount - 1), mMediaID(0), mEpochNameID(IDS_PREHISTORIC_AGE), mIsLoaded(false)
{
    this->mEpochsBeforeWonders = 1;
    this->mEpochs.Resize(gEpochCount);
}

// SYNTHETIC: EXE 0x5cbd7d
// (the scalar deleting destructor, emitted with the vtable; slot EETechTree+0x0)

// FUNCTION: EXE 0x5cbd99
// Destroys the tech tree and its epochs
EETechTree::~EETechTree()
{
}

// FUNCTION: EXE 0x5cbdf0
// Reads every epoch block of the tech tree file (the original: exactly 15), sets the epoch count, and resets the
// epoch range to all epochs
long EETechTree::Load(FSFileSpec& inFile)
{
    long theResult = kErrorOK;
    inFile.Open(kFileAccessRead);
    unsigned long theLength = 0;
    inFile.GetLength(theLength);
    long thePosition = 0;
    long theCount = 0;
    sNodeEpochs.clear();
    while (theResult == kErrorOK && inFile.GetPosition(thePosition) == kErrorOK && (unsigned long)thePosition < theLength) {
        this->mEpochs.Resize(theCount + 1);
        theResult = this->mEpochs[theCount].Load(inFile, this);
        theCount++;
    }
    gEpochCount = theCount;
    this->mStartEpoch = kTechEpochPrehistoric;
    this->mIsLoaded = false;
    this->mEndEpoch = theCount - 1;
    inFile.Close();
    return theResult;
}

// FUNCTION: EXE 0x5cbe4a
// Indexes, per unit type, the techs whose research event changes its model or sounds or upgrades it into
// another class (the shared table the media-mask queries read)
void EETechTree::BuildUnitClassInfo()
{
    std::vector<long> theIDs;
    for (long theEpoch = 0; theEpoch < EEEpochCount(); theEpoch++) {
        theIDs.clear();
        gTechTree.GetEpochNodeIDs(theEpoch, theIDs);
        theIDs.push_back(theEpoch);
        for (unsigned long i = 0; i < theIDs.size(); i++) {
            EETechTreeNode* theNode = gTechTree.Get(theIDs[i]);
            if (theNode == NULL || !(theNode->mData->mResearchedEvent > 0))
                continue;
            EEEvent* theEvent = EEEvent::Get(theNode->mData->mResearchedEvent);
            if (theEvent == NULL)
                continue;
            for (unsigned long j = 0; j < theEvent->GetNumEffects(); j++) {
                long theUnitClass = EEEvent::GetEventEffect(theEvent->GetEffect(j))->mUnitClass;
                EEUnitClass* theClass = (theUnitClass > 0 && theUnitClass < EEUnitClassCount()) ? gUnitClassTable[theUnitClass] : NULL;
                if (theClass != NULL) {
                    switch (EEEvent::GetEventEffect(theEvent->GetEffect(j))->mKind) {
                    case 6:
                        sUnitClassInfo[theClass->mUnitType].mModelTechs.push_back(theIDs[i]);
                        break;
                    case 9: {
                        EETechTreeNode* theTechnology =
                            gTechTree.Get(EEEvent::GetEventEffect(theEvent->GetEffect(j))->mTechnology);
                        if (theTechnology != NULL && gUnitClassTable[theTechnology->mData->mUnitClass] != NULL)
                            sUnitClassInfo[gUnitClassTable[theTechnology->mData->mUnitClass]->mUnitType].mTechnology =
                                theIDs[i];
                        break;
                    }
                    case 0xf:
                        sUnitClassInfo[theClass->mUnitType].mAcknowledgeSound1Techs.push_back(theIDs[i]);
                        break;
                    case 0x10:
                        sUnitClassInfo[theClass->mUnitType].mCreationSoundTechs.push_back(theIDs[i]);
                        break;
                    case 0x11:
                        sUnitClassInfo[theClass->mUnitType].mDeathSoundTechs.push_back(theIDs[i]);
                        break;
                    case 0x12:
                        sUnitClassInfo[theClass->mUnitType].mSelectionSound1Techs.push_back(theIDs[i]);
                        break;
                    case 0x14:
                        sUnitClassInfo[theClass->mUnitType].mAcknowledgeSound2Techs.push_back(theIDs[i]);
                        break;
                    case 0x15:
                        sUnitClassInfo[theClass->mUnitType].mSelectionSound2Techs.push_back(theIDs[i]);
                        break;
                    }
                } else if (EEEvent::GetEventEffect(theEvent->GetEffect(j))->mKind == 9) {
                    EETechTreeNode* theTechnology =
                        gTechTree.Get(EEEvent::GetEventEffect(theEvent->GetEffect(j))->mTechnology);
                    if (theTechnology != NULL && gUnitClassTable[theTechnology->mData->mUnitClass] != NULL)
                        sUnitClassInfo[gUnitClassTable[theTechnology->mData->mUnitClass]->mUnitType].mTechnology =
                            theIDs[i];
                }
            }
        }
    }
}

// FUNCTION: EXE 0x5cc282
// Unloads every epoch's media
void EETechTree::UnloadAll()
{
    for (long i = 0; i < this->mEpochs.size(); i++) {
        this->mEpochs[i].Unload();
    }
}

// FUNCTION: EXE 0x5cc29a
// Reapplies the saved obsolete techs, then researches the saved researched techs
void EETechTree::ApplySavedResearch()
{
    std::vector<long> theUnused1;
    std::vector<long> theUnused2;
    std::vector<long>::iterator theID;
    for (theID = this->mObsoleteIDs.begin(); theID != this->mObsoleteIDs.end(); theID++) {
        EETechTreeNode* theNode = this->Get(*theID);
        if (theNode != NULL)
            theNode->SetObsolete(true, true);
    }
    for (theID = this->mResearchedIDs.begin(); theID != this->mResearchedIDs.end(); theID++) {
        EETechTreeNode* theNode = this->Get(*theID);
        if (theNode != NULL) {
            theNode->mIsAvailable = true;
            theNode->ResearchNode(NULL);
        }
    }
}

// FUNCTION: EXE 0x5cc330
// Reapplies the scenario editor's obsoleted techs and clears that flag from default-obsoleted techs not in the list
void EETechTree::ApplySavedScenEditorObsoleted()
{
    std::vector<long> theIDs;
    std::vector<long> theCleared;
    this->GetNodeIDs(theIDs);
    for (long i = theIDs.size() - 1; i >= 0; i--) {
        EETechTreeNode* theNode = this->Get(theIDs[i]);
        if (theNode == NULL || theNode->mData->mObsoletedByDefault != true)
            continue;
        long theID = theNode->mData->mID;
        std::vector<long>::iterator theFound;
        theFound = std::find(this->mScenEditorObsoletedIDs.begin(), this->mScenEditorObsoletedIDs.end(), theID);
        if (theFound == this->mScenEditorObsoletedIDs.end())
            theCleared.push_back(theIDs[i]);
    }
    for (std::vector<long>::iterator theObsoleted = this->mScenEditorObsoletedIDs.begin(); theObsoleted != this->mScenEditorObsoletedIDs.end(); theObsoleted++) {
        EETechTreeNode* theNode = this->Get(*theObsoleted);
        if (theNode != NULL)
            theNode->SetScenEditorObsoleted(true, true);
    }
    for (long j = theCleared.size() - 1; j >= 0; j--) {
        EETechTreeNode* theNode = this->Get(theCleared[j]);
        if (theNode != NULL)
            theNode->SetScenEditorObsoleted(false, true);
    }
}

// FUNCTION: EXE 0x5cc4af
// Collects the ids of every node of every epoch
void EETechTree::GetNodeIDs(std::vector<long>& outIDs)
{
    for (long i = 0; i < this->mEpochs.size(); i++) {
        this->GetEpochNodeIDs(i, outIDs);
    }
}

// FUNCTION: EXE 0x5cc4cc
// Collects the node ids of one epoch
void EETechTree::GetEpochNodeIDs(long inEpoch, std::vector<long>& outIDs)
{
    if (inEpoch >= kTechEpochPrehistoric && inEpoch < this->mEpochs.size()) {
        this->mEpochs[inEpoch].GetNodeIDs(outIDs);
    }
}

// FUNCTION: EXE 0x5cc4ec
// Adds a sync checksum of every tech's and epoch's researched/obsolete state, weighted by position
void EETechTree::GetChecksum(ESChecksum& ioChecksum)
{
    std::vector<long> theIDs;
    unsigned long theChecksum = 0;
    bool theCurrentNodeIsObsolete = false;
    bool theCurrentNodeIsResearched = false;
    bool theScenEditorObsoleted = false;
    this->GetNodeIDs(theIDs);
    long theWeight = 15;   // the original's start weight (its epoch count; kept: the checksum's values)
    for (long i = 0; i < this->mEpochs.size(); i++) {
        long theID = this->mEpochs[i].mData->mID;
        theIDs.push_back(theID);
    }
    for (std::vector<long>::iterator theIterator = theIDs.begin(); theIterator != theIDs.end(); theIterator++, theWeight++) {
        ESVERIFY_AT(this->GetTechIDState(*theIterator, theCurrentNodeIsResearched, theCurrentNodeIsObsolete, theScenEditorObsoleted), 512);
        long theState = 0;
        if (theCurrentNodeIsResearched)
            theState = 1;
        if (theCurrentNodeIsObsolete)
            theState += 2;
        theChecksum += *theIterator * theState * theWeight;
    }
    ioChecksum.AddChecksum("EETechTree", theChecksum);
}

// FUNCTION: EXE 0x5cc5db
// Adds up the resource costs (kinds 2..6) of a tech and every unresearched prerequisite and upgrade path to it
void EETechTree::SumCosts(long inID, long inUnitClass, long* ioCosts)
{
    while (inID >= kTechEpochPrehistoric && inID < kTechNone) {
        EETechTreeNode* theNode = this->Get(inID);
        if (!theNode->GetIsAvailable() || theNode->mIsResearched)
            break;
        for (unsigned long i = 2; i < 7; i++)
            ioCosts[i] += theNode->GetCost(i, inUnitClass, 1.0f);
        EEUnitClass* theClass = gUnitClassTable[theNode->GetUnitClass(0)];
        EETechTreeNode* theUpgrade = this->Get(theClass != NULL ? theClass->mTechnology : 100000);
        if (theUpgrade != NULL && theUpgrade->GetUnitClass(0) != 0) {
            theClass = gUnitClassTable[theUpgrade->GetUnitClass(0)];
            if ((theClass != NULL ? theClass->mTechnology : 100000) != inID)
                this->SumCosts(theUpgrade->mData->mID, inUnitClass, ioCosts);
        }
        this->SumCosts(theNode->mData->mPrerequisite1, inUnitClass, ioCosts);
        this->SumCosts(theNode->mData->mPrerequisite2, inUnitClass, ioCosts);
        this->SumCosts(theNode->mData->mPrerequisite3, inUnitClass, ioCosts);
        inID = theNode->mData->mPrerequisite4;
    }
}

// FUNCTION: EXE 0x5cc710
// Tells which media of a unit class a tree starting at a later epoch still needs: false when the class is
// upgraded away before the start epoch, else a mask without models and sounds replaced before it
bool EETechTree::GetUnitClassMediaMask(EETechTree* inTree, long inUnitClass, unsigned long* outMask)
{
    bool theResult = true;
    *outMask = kMediaAll;
    if (sUnitClassInfo.find(inUnitClass) != sUnitClassInfo.end()) {
        if (sUnitClassInfo[inUnitClass].mTechnology != kTechNone) {
            EETechTreeNode* theNode = inTree->Get(sUnitClassInfo[inUnitClass].mTechnology);
            if (theNode != NULL && theNode->mEpoch < inTree->mStartEpoch && !theNode->mScenEditorObsoleted)
                theResult = false;
        } else {
            if (sUnitClassInfo[inUnitClass].mModelTechs.size() > 0
                && inTree->Get(sUnitClassInfo[inUnitClass].mModelTechs[0])->mEpoch < inTree->mStartEpoch)
                *outMask &= ~kMediaModel;
            if (sUnitClassInfo[inUnitClass].mAcknowledgeSound1Techs.size() > 0
                && inTree->Get(sUnitClassInfo[inUnitClass].mAcknowledgeSound1Techs[0])->mEpoch < inTree->mStartEpoch)
                *outMask &= ~kMediaAcknowledgeSound1;
            if (sUnitClassInfo[inUnitClass].mAcknowledgeSound2Techs.size() > 0
                && inTree->Get(sUnitClassInfo[inUnitClass].mAcknowledgeSound2Techs[0])->mEpoch < inTree->mStartEpoch)
                *outMask &= ~kMediaAcknowledgeSound2;
            if (sUnitClassInfo[inUnitClass].mCreationSoundTechs.size() > 0
                && inTree->Get(sUnitClassInfo[inUnitClass].mCreationSoundTechs[0])->mEpoch < inTree->mStartEpoch)
                *outMask &= ~kMediaCreationSound;
            if (sUnitClassInfo[inUnitClass].mDeathSoundTechs.size() > 0
                && inTree->Get(sUnitClassInfo[inUnitClass].mDeathSoundTechs[0])->mEpoch < inTree->mStartEpoch)
                *outMask &= ~kMediaDeathSound;
            if (sUnitClassInfo[inUnitClass].mSelectionSound1Techs.size() > 0
                && inTree->Get(sUnitClassInfo[inUnitClass].mSelectionSound1Techs[0])->mEpoch < inTree->mStartEpoch)
                *outMask &= ~kMediaSelectionSound1;
            if (sUnitClassInfo[inUnitClass].mSelectionSound2Techs.size() > 0
                && inTree->Get(sUnitClassInfo[inUnitClass].mSelectionSound2Techs[0])->mEpoch < inTree->mStartEpoch)
                *outMask &= ~kMediaSelectionSound2;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5cc9e8
// Tells whether a tech's model or sound effect for a unit class is the latest one the tree still uses
bool EETechTree::IsLatestEffect(EETechTree* inTree, long inID, long inUnitClass, long inKind)
{
    std::vector<long>* theIDs = NULL;
    bool theResult = true;
    std::vector<long>::iterator theID;
    switch (inKind) {
    case 6:
        theIDs = &sUnitClassInfo[inUnitClass].mModelTechs;
        break;
    case 0xf:
        theIDs = &sUnitClassInfo[inUnitClass].mAcknowledgeSound1Techs;
        break;
    case 0x10:
        theIDs = &sUnitClassInfo[inUnitClass].mCreationSoundTechs;
        break;
    case 0x11:
        theIDs = &sUnitClassInfo[inUnitClass].mDeathSoundTechs;
        break;
    case 0x12:
        theIDs = &sUnitClassInfo[inUnitClass].mSelectionSound1Techs;
        break;
    case 0x14:
        theIDs = &sUnitClassInfo[inUnitClass].mAcknowledgeSound2Techs;
        break;
    case 0x15:
        theIDs = &sUnitClassInfo[inUnitClass].mSelectionSound2Techs;
        break;
    }
    theID = std::find(theIDs->begin(), theIDs->end(), inID);
    for (theID++; theID != theIDs->end(); theID++) {
        EETechTreeNode* theNode = inTree->Get(*theID);
        if (theNode->mEpoch < inTree->mStartEpoch && !theNode->mScenEditorObsoleted) {
            theResult = false;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5ccb1f
// Returns the tree's player (by index when no player pointer is set)
EEPlayer* EETechTree::GetPlayer()
{
    if (this->mPlayer != NULL)
        return this->mPlayer;
    return gWorld.mPlayers[this->mPlayerIndex];
}

// FUNCTION: EXE 0x5ccb34
// Advances to the next epoch unless the end epoch is reached
void EETechTree::AdvanceEpoch()
{
    long theEpoch = this->mCurrentEpoch + 1;
    if (theEpoch <= this->mEndEpoch) {
        this->SetEpoch(theEpoch);
    }
}

// FUNCTION: EXE 0x5ccb4a
// Reapplies a loaded tree's state: editor-obsoleted techs, the epoch range, research, each epoch's counters,
// then refreshes the interface
void EETechTree::ApplySavedState(bool inApplyObsoleted, bool inApplyResearch)
{
    if (inApplyObsoleted == true)
        this->ApplySavedScenEditorObsoleted();
    this->SetEpochRange(this->mStartEpoch, this->mEndEpoch);
    if (inApplyResearch)
        this->ApplySavedResearch();
    for (std::vector<EETileCoordinate>::iterator theEntry = this->mSavedSOLs.begin(); theEntry != this->mSavedSOLs.end(); theEntry++) {
        if (theEntry->mX >= 0 && theEntry->mX < this->mEpochs.size())
            this->mEpochs[theEntry->mX].mCurrentSOL = theEntry->mY;
    }
    for (long i = 0; i < this->mEpochs.size(); i++) {
        this->mEpochs[i].UpdateAvailability();
        this->mEpochs[i].mIsCountingSOL = true;
    }
    this->UpdateInterface(true);
}

// FUNCTION: EXE 0x5ccbc9
// Reads the saved research, obsolete and editor-obsoleted tech ids, the current/start/end epochs and each epoch's
// state (14 epochs before version 0.0xea); marks every epoch up to the current one researched
long EETechTree::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theNumEpochs = 0;
    long theNumTechIDs = 0;
    long theTechID;
    long theEpoch;
    bool theIsResearched;
    bool theIsObsolete;
    bool theScenEditorObsoleted;
    this->mObsoleteIDs.clear();
    this->mResearchedIDs.clear();
    this->mScenEditorObsoletedIDs.clear();
    this->mSavedSOLs.clear();
    long theResult = inFile.Read(theNumEpochs);
    if (theResult == kErrorOK) {
        theResult = inFile.Read(this->mEpochsBeforeWonders);
        if (theResult == kErrorOK) {
            this->mIsLoaded = true;
            ESVERIFY_AT(theNumEpochs <= this->mEpochs.size(), 926);
            theResult = inFile.Read(theNumTechIDs);
            if (theResult == kErrorOK) {
                for (long i = 0; i < theNumTechIDs; i++) {
                    if (inFile.Read(theTechID) != kErrorOK || inFile.Read(theIsResearched) != kErrorOK
                        || inFile.Read(theIsObsolete) != kErrorOK)
                        continue;
                    if (theIsResearched)
                        this->AddResearchedID(theTechID);
                    if (theIsObsolete)
                        this->AddObsoleteID(theTechID);
                    if (inFile.Read(theScenEditorObsoleted) == kErrorOK && theScenEditorObsoleted == true)
                        this->AddScenEditorObsoletedID(theTechID);
                }
                theResult = inFile.Read(this->mCurrentEpoch);
                if (theResult == kErrorOK) {
                    theResult = inFile.Read(this->mStartEpoch);
                    if (theResult == kErrorOK)
                        theResult = inFile.Read(this->mEndEpoch);
                }
                long theEpochs = theNumEpochs;   // the saved tree's epochs (the original: 15)
                if (inMajorVersion == 0 && inMinorVersion < 0xea)
                    theEpochs = 14;
                for (theEpoch = 0; theEpoch < theEpochs && theEpoch < this->mEpochs.size(); theEpoch++)
                    this->mEpochs[theEpoch].Load(inMajorVersion, inMinorVersion, inFile);
                for (theEpoch = this->mCurrentEpoch; theEpoch >= 0; theEpoch--)
                    this->mResearchedIDs.insert(this->mResearchedIDs.begin(), theEpoch);
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5ccdac
// Reports a researched tech or epoch to the player (epoch hook, statistics series 0x13) and to the researching
// unit
void EETechTree::OnResearched(long inID, EEComplexUnit* inUnit)
{
    EEPlayer* thePlayer = this->GetPlayer();
    if (thePlayer == NULL)
        return;
    thePlayer->EpochReached(inID);
    thePlayer->mSavedGroups.Reset(0x13, 1);
    if (inUnit != NULL)
        inUnit->ResearchCompleted(inID);
}

// FUNCTION: EXE 0x5ccdea
// Makes a tech or epoch obsolete in its epoch
void EETechTree::MakeObsolete(long inID)
{
    long theEpoch = EETechTreeEpochOf(inID);
    if (theEpoch >= kTechEpochPrehistoric && theEpoch < this->mEpochs.size()) {
        this->mEpochs[theEpoch].MakeObsolete(inID);
    }
}

// FUNCTION: EXE 0x5cce23
// Makes the outdated nodes of every epoch up to the current one obsolete
void EETechTree::ObsoleteOutdatedNodes()
{
    for (long i = 0; i <= this->mCurrentEpoch; i++) {
        this->mEpochs[i].ObsoleteOutdatedNodes();
    }
}

// FUNCTION: EXE 0x5cce4c
// Sets a prerequisite of a tech in its epoch
void EETechTree::SetPrerequisite(long inID, long inPrerequisite)
{
    long theEpoch = EETechTreeEpochOf(inID);
    if (theEpoch >= kTechEpochPrehistoric && theEpoch < this->mEpochs.size()) {
        this->mEpochs[theEpoch].SetPrerequisite(inID, inPrerequisite);
    }
}

// FUNCTION: EXE 0x5cce89
// Remembers a tech id to make obsolete when the saved state is applied
void EETechTree::AddObsoleteID(long inID)
{
    this->mObsoleteIDs.push_back(inID);
}

// FUNCTION: EXE 0x5cce9c
// Remembers a tech id to research when the saved state is applied
void EETechTree::AddResearchedID(long inID)
{
    this->mResearchedIDs.push_back(inID);
}

// FUNCTION: EXE 0x5cceaf
// Remembers a tech id the scenario editor obsoleted
void EETechTree::AddScenEditorObsoletedID(long inID)
{
    this->mScenEditorObsoletedIDs.push_back(inID);
}

// FUNCTION: EXE 0x5ccec2
// Researches a tech or epoch; a researched tech that counts toward the next epoch adds to its count, an epoch
// updates statistics series 0xc
void EETechTree::Research(long inID, EEComplexUnit* inUnit)
{
    long theEpoch = EETechTreeEpochOf(inID);
    long theNextEpoch = this->mCurrentEpoch + 1;
    if (!(theEpoch >= kTechEpochPrehistoric) || !(theEpoch < this->mEpochs.size()))
        return;
    long theResult = this->mEpochs[theEpoch].Research(inID, inUnit);
    if (theResult != 2 && theResult != 0)
        return;
    if (EEIsEpochID(inID)) {
        this->mPlayer->mSavedGroups.Reset(0xc, 1);
    } else {
        EETechTreeNode* theNode = this->Get(inID);
        if (theNode != NULL && theNode->mData->mCountsTowardEpoch && theNextEpoch >= 0 && theNextEpoch < this->mEpochs.size())
            this->mEpochs[theNextEpoch].AddSOL();
    }
}

// FUNCTION: EXE 0x5ccf5d
// Researches every epoch up to the given one (events disabled) and its kind-7 nodes; shows the epoch name to
// the local player
void EETechTree::ResearchToEpoch(long inEpoch)
{
    gEventsEnabled = false;
    if (inEpoch >= kTechEpochPrehistoric && inEpoch < this->mEpochs.size()) {
        if (inEpoch != kTechEpochPrehistoric) {
            if (inEpoch > kTechEpochPrehistoric) {
                for (long i = 0; i < inEpoch; i++) {
                    this->mEpochs[i].Unlock();
                    this->mEpochs[i].ResearchAll(false);
                    if (i + 1 == inEpoch && i + 1 < this->mEpochs.size()) {
                        this->mEpochs[i + 1].Unlock();
                        this->mEpochs[i + 1].ResearchSelf();
                    }
                }
            }
        } else {
            this->mEpochs[0].Unlock();
            this->mEpochs[0].ResearchSelf();
        }
        this->mEpochs[inEpoch].ResearchKind(7, true);
        if (gWorld.mPlayers[this->mPlayerIndex] == gWorld.GetCurrentPlayer()) {
            this->mEpochNameID = EEEpochTitleID(this->mEpochs[inEpoch].mData->mID);
            gUserInterface->PostShowEpochName();
            if (gUIManager->mMainGameForm != NULL)
                gUIManager->mMainGameForm->mRefreshGroup = true;
        }
    }
    gEventsEnabled = true;
}

// FUNCTION: EXE 0x5cd046
// Writes the tree to the saved game: the ids of researched, obsolete or editor-obsoleted techs with their flags,
// the epochs, then each epoch's state; returns the last epoch's write result
long EETechTree::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    std::vector<long> theIDs;

    bool theCurrentNodeIsResearched = false;
    bool theCurrentNodeIsObsolete = false;
    bool theScenEditorObsoleted = false;
    long theCurrentTechID = 0;
    long theNumEpochs = this->mEpochs.size();
    std::vector<long>::iterator theIterator;
    long theResult = outFile.Write(theNumEpochs);
    if (theResult == 0) {
        theResult = outFile.Write(this->mEpochsBeforeWonders);
        if (theResult == 0) {
            this->GetNodeIDs(theIDs);
            for (long i = 0; i < this->mEpochs.size(); i++) {
                theCurrentTechID = this->mEpochs[i].mData->mID;
                theIDs.push_back(theCurrentTechID);
            }
            theIterator = theIDs.begin();
            while (theIterator != theIDs.end()) {
                ESVERIFY_AT(this->GetTechIDState(*theIterator, theCurrentNodeIsResearched, theCurrentNodeIsObsolete,theScenEditorObsoleted), 1319);
                if ((!theCurrentNodeIsResearched
                        || EETechTreeEpochOf(*theIterator)
                            < this->mStartEpoch)
                    && !theCurrentNodeIsObsolete && !theScenEditorObsoleted)
                    theIterator = theIDs.erase(theIterator);
                else
                    theIterator++;
            }

            theResult = outFile.Write((long)theIDs.size());
            if (theResult == 0) {
                for (theIterator = theIDs.begin(); theIterator != theIDs.end(); theIterator++) {
                    if (theResult != 0)
                        break;
                    theCurrentTechID = *theIterator;
                    ESVERIFY_AT(this->GetTechIDState(theCurrentTechID, theCurrentNodeIsResearched, theCurrentNodeIsObsolete, theScenEditorObsoleted), 1342);
                    theResult = outFile.Write(theCurrentTechID);
                    if (theResult != 0)
                        continue;
                    theResult = outFile.Write(theCurrentNodeIsResearched);
                    if (theResult != 0)
                        continue;
                    theResult = outFile.Write(theCurrentNodeIsObsolete);
                    if (theResult == 0)
                        theResult = outFile.Write(theScenEditorObsoleted);
                }
                if (theResult == 0) {
                    theResult = outFile.Write(this->mCurrentEpoch);
                    if (theResult == 0) {
                        theResult = outFile.Write(this->mStartEpoch);
                        if (theResult == 0)
                            theResult = outFile.Write(this->mEndEpoch);
                    }
                }
            }
        }
    }

    for (theCurrentTechID = 0; theCurrentTechID < this->mEpochs.size(); theCurrentTechID++) {
        theResult = this->mEpochs[theCurrentTechID].Save(inMajorVersion, inMinorVersion, outFile);
        ESVERIFY_AT(theResult == keseOK, 1381);
    }
    return theResult;
}

// FUNCTION: EXE 0x5cd2a7
// Moves the tree forward to an epoch (never back), recording it in statistics series 0xc and researching up to it
void EETechTree::SetEpoch(long inEpoch)
{
    if (!(inEpoch >= kTechEpochPrehistoric) || !(inEpoch < this->mEpochs.size()) || !(inEpoch >= this->mCurrentEpoch))
        return;
    this->mCurrentEpoch = inEpoch;
    if (inEpoch != kTechEpochPrehistoric)
        this->mPlayer->mSavedGroups.Set(0xc, inEpoch);
    this->ResearchToEpoch(inEpoch);
}

// FUNCTION: EXE 0x5cd2ed
// Sets the start and end epochs and researches up to the start
void EETechTree::SetEpochRange(long inStart, long inEnd)
{
    this->mStartEpoch = inStart;
    this->mEndEpoch = inEnd;
    this->ResearchToEpoch(inStart);
}

// FUNCTION: EXE 0x5cd30a
// Sets the epoch range and the current epoch to its start, without researching
void EETechTree::ResetEpochRange(long inStart, long inEnd)
{
    this->mStartEpoch = inStart;
    this->mEndEpoch = inEnd;
    this->mCurrentEpoch = inStart;
    this->mPlayer->mSavedGroups.Set(0xc, inStart);
}

// FUNCTION: EXE 0x5cd33b
// Makes a node available or unavailable unless it has a research delay
void EETechTree::SetNodeAvailable(long inID, bool inValue)
{
    EETechTreeNode* theNode = this->Get(inID);
    if (theNode != NULL && theNode->mResearchDelay == 0) {
        theNode->mIsAvailable = inValue;
    }
}

// FUNCTION: EXE 0x5cd358
// Sets the tree's player and player index
void EETechTree::SetPlayer(EEPlayer* inPlayer)
{
    this->mPlayer = inPlayer;
    if (inPlayer != NULL)
        this->SetPlayerIndex(inPlayer->mPlayerID);
}

// FUNCTION: EXE 0x5cd374
// For the local player's tree, shows the new epoch's name and refreshes the main game form's group panel
void EETechTree::UpdateInterface(bool inEpochChanged)
{
    EEPlayer* theCurrentPlayer = gWorld.GetCurrentPlayer();
    if (theCurrentPlayer == NULL || this->mPlayerIndex != theCurrentPlayer->mPlayerID)
        return;
    if (inEpochChanged) {
        this->mEpochNameID = EEEpochTitleID(this->mCurrentEpoch);
        gUserInterface->PostShowEpochName();
    }
    if (gUIManager->mMainGameForm != NULL)
        gUIManager->mMainGameForm->mRefreshGroup = true;
}

// FUNCTION: EXE 0x5cd3cb
// Copies another tree's epochs, epoch range and player; the copy is not marked loaded
EETechTree& EETechTree::operator=(const EETechTree& inOther)
{
    this->mCurrentEpoch = inOther.mCurrentEpoch;
    this->mStartEpoch = inOther.mStartEpoch;
    this->mEndEpoch = inOther.mEndEpoch;
    this->mPlayerIndex = inOther.mPlayerIndex;
    this->mPlayer = inOther.mPlayer;
    this->mEpochs.Resize(inOther.mEpochs.size());
    for (long i = 0; i < inOther.mEpochs.size(); i++) {
        this->mEpochs[i].mTree = this;
        this->mEpochs[i] = inOther.mEpochs[i];
    }
    this->mIsLoaded = false;
    return *this;
}

// FUNCTION: EXE 0x5cd439
// Looks up or inserts a unit type's entry in the media table (std::map::operator[])
// (std::map<long, EETechTreeUnitClassInfo>::operator[], Dinkum <map>)

// FUNCTION: EXE 0x5cd48d
// Creates an empty media table entry (no upgrade tech)
// (EETechTreeUnitClassInfo's inline ctor, above)

// FUNCTION: EXE 0x5cd503
// Copies a media table entry
// (EETechTreeUnitClassInfo's inline copy ctor, above)

// FUNCTION: EXE 0x5cd5cf
// Destroys a media table entry
// (EETechTreeUnitClassInfo's inline dtor, above)

// FUNCTION: EXE 0x5cd649
// Destroys the media table's tree
// (sUnitClassInfo's _Tree::~_Tree, Dinkum <xtree>)

// FUNCTION: EXE 0x5cd674
// Finds a unit type in the media table
// (sUnitClassInfo's _Tree::find, Dinkum <xtree>)

// FUNCTION: EXE 0x5cd6ae
// Inserts an entry into the media table
// (sUnitClassInfo's _Tree::insert, Dinkum <xtree>)

// FUNCTION: EXE 0x5cd76a
// Erases a range of media table entries
// (sUnitClassInfo's _Tree::erase(first, last), Dinkum <xtree>)

// FUNCTION: EXE 0x5cd7db
// Erases one media table entry
// (sUnitClassInfo's _Tree::erase(iterator), Dinkum <xtree>)

// FUNCTION: EXE 0x5cdb33
// Erases a subtree of the media table
// (sUnitClassInfo's _Tree::_Erase, Dinkum <xtree>)

// FUNCTION: EXE 0x5cdb73
// Initializes the media table's shared nil node
// (sUnitClassInfo's _Tree::_Init, Dinkum <xtree>)

// FUNCTION: EXE 0x5cdbb7
// Inserts a node into the media table's red-black tree
// (sUnitClassInfo's _Tree::_Insert, Dinkum <xtree>)

// FUNCTION: EXE 0x5cdd79
// Finds the first media table node not before a key
// (sUnitClassInfo's _Tree::_Lbound, Dinkum <xtree>)

// FUNCTION: EXE 0x5cdda8
// Rotates a media table node left
// (sUnitClassInfo's _Tree::_Lrotate, Dinkum <xtree>)

// FUNCTION: EXE 0x5cdded
// Rotates a media table node right
// (sUnitClassInfo's _Tree::_Rrotate, Dinkum <xtree>)

// FUNCTION: EXE 0x5cde34
// Steps a media table iterator back
// (sUnitClassInfo's _Tree::iterator::_Dec, Dinkum <xtree>)

// FUNCTION: EXE 0x5cde84
// Steps a media table iterator forward
// (sUnitClassInfo's _Tree::iterator::_Inc, Dinkum <xtree>)

// FUNCTION: EXE 0x5cdec8
// Constructs a media table value in place
// (std::_Construct of sUnitClassInfo's value_type, Dinkum <xmemory>)

// FUNCTION: EXE 0x5cdee5
// Sets the tree's player index
// (RTSTechTree::SetPlayerIndex, inline in ee/EEPlayer.h)

// SYNTHETIC: EXE 0x5cdeef
// (RTSTechTree's scalar deleting destructor: its inline empty dtor)

// FUNCTION: EXE 0x5cdf0c
// Returns an event's effect id at an index
// (EEEvent::GetEffect, inline in ee/EEEvent.h: the copy BuildUnitClassInfo calls)

// SYNTHETIC: EXE 0x5cba2f
// SYNTHETIC: EXE 0x5cba39
// SYNTHETIC: EXE 0x5cba4a
// SYNTHETIC: EXE 0x5cba56
// SYNTHETIC: EXE 0x5cba72
// SYNTHETIC: EXE 0x5cba7c
// SYNTHETIC: EXE 0x5cba8d
// SYNTHETIC: EXE 0x5cba99
// SYNTHETIC: EXE 0x5cbab5
// SYNTHETIC: EXE 0x5cbabf
// SYNTHETIC: EXE 0x5cbad0
// SYNTHETIC: EXE 0x5cbadc
// SYNTHETIC: EXE 0x5cbaf8
// SYNTHETIC: EXE 0x5cbb02
// SYNTHETIC: EXE 0x5cbb13
// SYNTHETIC: EXE 0x5cbb1f
// SYNTHETIC: EXE 0x5cbb3b
// SYNTHETIC: EXE 0x5cbb45
// SYNTHETIC: EXE 0x5cbb56
// SYNTHETIC: EXE 0x5cbb62
// SYNTHETIC: EXE 0x5cbb7e
// SYNTHETIC: EXE 0x5cbb88
// SYNTHETIC: EXE 0x5cbb99
// SYNTHETIC: EXE 0x5cbba5
// SYNTHETIC: EXE 0x5cbbc1
// SYNTHETIC: EXE 0x5cbbcb
// SYNTHETIC: EXE 0x5cbbdc
// SYNTHETIC: EXE 0x5cbbe8
// SYNTHETIC: EXE 0x5cbc04
// SYNTHETIC: EXE 0x5cbc0e
// SYNTHETIC: EXE 0x5cbc1f
// SYNTHETIC: EXE 0x5cbc2b
// SYNTHETIC: EXE 0x5cbc47
// SYNTHETIC: EXE 0x5cbc51
// SYNTHETIC: EXE 0x5cbc71
// SYNTHETIC: EXE 0x5cbc7d
