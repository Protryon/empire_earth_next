// C:\EEX\Empire Earth\Tech Tree\EETechTree.cpp (__FILE__ 0x4c5c08; RTTI EETechTree > RTSTechTree, vtables 0x446114 /
// 0x446120). A player's tech tree: 0x1cba2f..0x1cdf25 (the eight key chains and sUnitClassInfo's chain, the ctor ..
// EEEvent::GetEffect's COMDAT; EETechTreeEpoch.cpp's key chain 0x1cdf25 opens the next unit). Classes in
// ee/EEPlayer.h. Unit flags: the exe's module set.
// port (R5b.4, round 29): the tree's walks, its saved game, its checksum and the media table (sUnitClassInfo) are Rust
// (ee_tech nodes.rs) over the records the classes derive from and EETechHost below.

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
#include "ee/EETechCapture.h"
#include "ee/EETechRust.h"
const UString EETechTree::sNumEpochs("EETechTree-NumEpochs");
const UString EETechTree::sNumTechIDs("EETechTree-NumTechIDs");
const UString EETechTree::sTechNodeID("EETechTree-TechNodeID");
const UString EETechTree::sNodeIsResearched("EETechTree-NodeIsResearched");
const UString EETechTree::sNodeIsObsolete("EETechTree-NodeIsObsolete");
const UString EETechTree::sCurrentEpoch("EETechTree-CurrentEpoch");
const UString EETechTree::sStartEpoch("EETechTree-StartEpoch");
const UString EETechTree::sEndEpoch("EETechTree-EndEpoch");

// The walks' host (ee_tech nodes.rs EE_TECH_HOST_*): the tree's containers, the entry points the simulation capture
// wraps (called here so their records keep their order), the world and the capture's walk records
int32_t EETechHost(void* inTree, int32_t inOp, EeTechHostIo* io)
{
    EETechTree* theTree = static_cast<EETechTree*>(inTree);
    EETechTreeNode* theNode = static_cast<EETechTreeNode*>(io->node.obj);
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(io->unit);
    int32_t* v = io->v;
    switch (inOp) {
    case EE_TECH_HOST_EPOCHS: v[0] = (int32_t)theTree->mEpochs.size(); return 0;
    case EE_TECH_HOST_EPOCH: io->node = EETechRef(&theTree->mEpochs[v[0]]); return 0;
    case EE_TECH_HOST_EPOCH_NODES: {
        std::map<long, EETechTreeNode*>& theNodes = static_cast<EETechTreeEpoch*>(theNode)->mNodes;
        uint32_t k = 0;
        for (std::map<long, EETechTreeNode*>::iterator it = theNodes.begin(); it != theNodes.end() && k < io->count; ++it)
            io->nodes[k++] = EETechRef(it->second);
        return (int32_t)theNodes.size();
    }
    case EE_TECH_HOST_NODE: {
        EETechTreeNode* theFound = theTree->Get(v[0]);
        if (theFound == NULL)
            return 0;
        io->node = EETechRef(theFound);
        return 1;
    }
    case EE_TECH_HOST_EPOCH_OF: v[0] = (int32_t)EETechTreeEpochOf(v[0]); return 0;
    case EE_TECH_HOST_EPOCH_COUNT: v[0] = (int32_t)gEpochCount; return 0;
    case EE_TECH_HOST_RESEARCH_NODE: return (int32_t)theNode->ResearchNode(theUnit);
    case EE_TECH_HOST_SET_PREREQUISITE: theNode->SetPrerequisite(v[0]); return 0;
    case EE_TECH_HOST_EPOCH_RESEARCH: return (int32_t)static_cast<EETechTreeEpoch*>(theNode)->Research(v[0], theUnit);
    case EE_TECH_HOST_ADD_SOL: static_cast<EETechTreeEpoch*>(theNode)->AddSOL(); return 0;
    case EE_TECH_HOST_UPDATE_AVAILABILITY: static_cast<EETechTreeEpoch*>(theNode)->UpdateAvailability(); return 0;
    case EE_TECH_HOST_NEEDS_UNIT_MEDIA: return theNode->NeedsUnitMedia() ? 1 : 0;
    case EE_TECH_HOST_TRIGGER:
        EEEvent::Trigger(v[0], theTree->mPlayerIndex, v[1], theTree->mPlayer, theUnit);
        return 0;
    case EE_TECH_HOST_UPDATE_INTERFACE: theTree->UpdateInterface(v[0] != 0); return 0;
    case EE_TECH_HOST_ON_RESEARCHED: {
        // OnResearched 0x1ccdac: the player's epoch hook and statistics series 0x13 (its unit: always NULL)
        EEPlayer* thePlayer = theTree->GetPlayer();
        if (thePlayer != NULL) {
            thePlayer->EpochReached(v[0]);
            thePlayer->mSavedGroups.Reset(0x13, 1);
        }
        return 0;
    }
    case EE_TECH_HOST_STATISTIC:
        if (v[0] == 2)
            theTree->mPlayer->mSavedGroups.Set(v[1], v[2]);
        else
            (v[0] == 0 ? theTree->mPlayer : theTree->GetPlayer())->mSavedGroups.Reset(v[1], 1);
        return 0;
    case EE_TECH_HOST_UNLOAD_UNIT_MEDIA: theTree->GetPlayer()->UnloadUnitClassMedia(v[0]); return 0;
    case EE_TECH_HOST_EVENTS_ENABLED: gEventsEnabled = v[0] != 0; return 0;
    case EE_TECH_HOST_IS_LOCAL: return gWorld.mPlayers[theTree->mPlayerIndex] == gWorld.GetCurrentPlayer() ? 1 : 0;
    case EE_TECH_HOST_SHOW_EPOCH_NAME:
        theTree->mEpochNameID = EEEpochTitleID(v[0]);
        gUserInterface->PostShowEpochName();
        if (gUIManager->mMainGameForm != NULL)
            gUIManager->mMainGameForm->mRefreshGroup = true;
        return 0;
    case EE_TECH_HOST_ASSERT: ESVERIFY_AT(false, v[0]); return 0;
    case EE_TECH_HOST_WALK_BEGIN: EETechCaptureWalkBegin(); return 0;
    case EE_TECH_HOST_WALK_STEP: EETechCaptureWalkStep(v[0], v[1]); return 0;
    case EE_TECH_HOST_WALK: EETechCaptureWalk(v[0], v[4] ? theTree : NULL, theNode, v[1], v[2], v[3]); return 0;
    case EE_TECH_HOST_CAPTURE_EPOCHS:
        if (EESimCaptureRecord::Active()) {   // TEPO (TEST ONLY): SetEpoch 0, ResearchToEpoch 1, SetEpochRange 2
            EESimCaptureRecord theRecord(EECaptureTag("TEPO"));
            theRecord.U32((unsigned int)v[0]);
            theRecord.I32(v[1]);
            if (v[0] == 2)
                theRecord.I32(v[2]);
            EETechCaptureTree(theRecord, theTree);
            theRecord.Commit();
        }
        return 0;
    case EE_TECH_HOST_CAPTURE_EPOCH_STEP: EETechCaptureEpochStep(v[0], v[1]); return 0;
    case EE_TECH_HOST_CAPTURE_RESEARCH_TO_EPOCH: EETechCaptureResearchToEpoch(theTree, v[0], v[1] != 0); return 0;
    case EE_TECH_HOST_EFFECT_COUNT:
    case EE_TECH_HOST_EFFECT: {
        EEEvent* theEvent = EEEvent::Get(theNode->mData->mResearchedEvent);
        if (inOp == EE_TECH_HOST_EFFECT_COUNT)
            return theEvent != NULL ? (int32_t)theEvent->GetNumEffects() : 0;
        EEEventEffect* theEffect = EEEvent::GetEventEffect(theEvent->GetEffect(v[0]));
        v[0] = (int32_t)theEffect->mKind;
        v[1] = (int32_t)theEffect->mUnitClass;
        v[2] = (int32_t)theEffect->mTechnology;
        return 0;
    }
    case EE_TECH_HOST_CLASS_TYPE: {
        EEUnitClass* theClass = v[0] >= 0 && v[0] < (int32_t)EEUnitClassCount() ? gUnitClassTable[v[0]] : NULL;
        if (theClass == NULL)
            return 0;
        v[0] = (int32_t)theClass->mUnitType;
        return 1;
    }
    }
    return 0;
}

// The saved game's walks: the file's long and bool (EESavePrimitive), the rest EETechHost's
struct EETechSaveContext {
    EETechTree* mTree;
    FSFile* mFile;
};

static int32_t EETechSaveHost(void* inContext, int32_t inOp, EeTechHostIo* io)
{
    EETechSaveContext& theContext = *static_cast<EETechSaveContext*>(inContext);
    static const int32_t kOps[4] = {EE_UNITS_PLAYER_SAVE_READ_LONG, EE_UNITS_PLAYER_SAVE_READ_BOOL,
                                    EE_UNITS_PLAYER_SAVE_WRITE_LONG, EE_UNITS_PLAYER_SAVE_WRITE_BOOL};
    if (inOp < EE_TECH_HOST_READ_LONG || inOp > EE_TECH_HOST_WRITE_BOOL)
        return EETechHost(theContext.mTree, inOp, io);
    EeUnitsPlayerSaveIo theIo = {io->v[0], 0.0f, (uint8_t)(io->v[0] != 0 ? 1 : 0)};
    int32_t theResult = EESavePrimitive(*theContext.mFile, kOps[inOp - EE_TECH_HOST_READ_LONG], &theIo);
    io->v[0] = inOp == EE_TECH_HOST_READ_BOOL ? theIo.b : (int32_t)theIo.i;
    return theResult;
}

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
    : mPlayer(NULL)
{
    this->mCurrentEpoch = kTechEpochPrehistoric;
    this->mStartEpoch = kTechEpochPrehistoric;
    this->mEndEpoch = gEpochCount - 1;
    this->mMediaID = 0;
    this->mEpochNameID = IDS_PREHISTORIC_AGE;
    this->mIsLoaded = false;
    this->mSaved = NULL;
    this->mEpochsBeforeWonders = 1;
    this->mEpochs.Resize(gEpochCount);
}

// SYNTHETIC: EXE 0x5cbd7d
// (the scalar deleting destructor, emitted with the vtable; slot EETechTree+0x0)

// FUNCTION: EXE 0x5cbd99
// Destroys the tech tree and its epochs (and the saved game's lists, Rust's)
EETechTree::~EETechTree()
{
    ee_tech_tree_fields_free(this);
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
// another class (the shared table the media-mask queries read; ee_tech nodes.rs build_unit_class_info)
void EETechTree::BuildUnitClassInfo()
{
    EETechTreeWalk(&gTechTree, EeTechTreeOp::BuildUnitClassInfo);
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
// Reapplies the saved obsolete techs, then researches the saved researched techs (ee_tech tree_apply_saved_research)

// FUNCTION: EXE 0x5cc330
// Reapplies the scenario editor's obsoleted techs and clears that flag from default-obsoleted techs not in the list
// (ee_tech tree_apply_saved_editor_obsoleted)

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
// Adds a sync checksum of every tech's and epoch's researched/obsolete state, weighted by position (ee_tech
// tree_checksum)
void EETechTree::GetChecksum(ESChecksum& ioChecksum)
{
    ioChecksum.AddChecksum("EETechTree", (unsigned long)(uint32_t)EETechTreeWalk(this, EeTechTreeOp::Checksum));
}

// FUNCTION: EXE 0x5cc5db
// Adds up the resource costs (kinds 2..6) of a tech and every unresearched prerequisite and upgrade path to it
void EETechTree::SumCosts(long inID, long inUnitClass, long* ioCosts)
{
    // TEST ONLY (ee/EETechCapture.h): the outermost call (the walk's own recursion is not a call of the game's)
    static thread_local long sDepth = 0;
    if (EESimCaptureRecord::Active() && sDepth == 0) {
        sDepth++;
        EESimCaptureRecord theRecord(EECaptureTag("TSUM"));
        theRecord.I32(this->mPlayerIndex);
        theRecord.I32(inID);
        theRecord.I32(inUnitClass);
        for (long i = 0; i < 7; i++)
            theRecord.I32(ioCosts[i]);
        this->SumCostsUncaptured(inID, inUnitClass, ioCosts);
        for (long i = 0; i < 7; i++)
            theRecord.I32(ioCosts[i]);
        theRecord.Commit();
        sDepth--;
        return;
    }
    this->SumCostsUncaptured(inID, inUnitClass, ioCosts);
}

// the tree for the Rust SumCosts (ee_tech sum_costs): a node, its cost, a unit class's technology
static bool EETechTreeSumNode(void* inTree, int32_t inID, EeTechSumNode* outNode)
{
    EETechTreeNode* theNode = static_cast<EETechTree*>(inTree)->Get(inID);
    if (theNode == NULL)
        return false;
    outNode->id = (int32_t)theNode->mData->mID;
    outNode->unit_class = (int32_t)theNode->GetUnitClass(0);
    outNode->prerequisites[0] = (int32_t)theNode->mData->mPrerequisite1;
    outNode->prerequisites[1] = (int32_t)theNode->mData->mPrerequisite2;
    outNode->prerequisites[2] = (int32_t)theNode->mData->mPrerequisite3;
    outNode->prerequisites[3] = (int32_t)theNode->mData->mPrerequisite4;
    outNode->available = theNode->GetIsAvailable();
    outNode->researched = theNode->mIsResearched;
    outNode->pad[0] = outNode->pad[1] = 0;
    return true;
}

static int32_t EETechTreeSumCost(void* inTree, int32_t inID, int32_t inResource, int32_t inUnitClass)
{
    return (int32_t)static_cast<EETechTree*>(inTree)->Get(inID)->GetCost(inResource, inUnitClass, 1.0f);
}

static int32_t EETechTreeClassTechnology(void* inTree, int32_t inUnitClass)
{
    EEUnitClass* theClass = gUnitClassTable[inUnitClass];
    return (int32_t)(theClass != NULL ? theClass->mTechnology : 100000);
}

void EETechTree::SumCostsUncaptured(long inID, long inUnitClass, long* ioCosts)
{
    EeTechSumCostsCallbacks theCallbacks;
    theCallbacks.ctx = this;
    theCallbacks.node = EETechTreeSumNode;
    theCallbacks.cost = EETechTreeSumCost;
    theCallbacks.class_technology = EETechTreeClassTechnology;
    int32_t theCosts[7];
    for (long i = 0; i < 7; i++)
        theCosts[i] = (int32_t)ioCosts[i];
    ee_tech_sum_costs(&theCallbacks, (int32_t)inID, (int32_t)inUnitClass, theCosts);
    for (long i = 0; i < 7; i++)
        ioCosts[i] = theCosts[i];
}

// FUNCTION: EXE 0x5cc9e8
// Tells whether a tech's model or sound effect for a unit class is the latest one the tree still uses (ee_tech
// is_latest_effect over the media table)
bool EETechTree::IsLatestEffect(EETechTree* inTree, long inID, long inUnitClass, long inKind)
{
    return ee_tech_is_latest_effect(inTree, &EETechHost, inTree, (int32_t)inID, (int32_t)inUnitClass, (int32_t)inKind);
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
    EETechTreeWalk(this, EeTechTreeOp::AdvanceEpoch);
}

// FUNCTION: EXE 0x5ccb4a
// Reapplies a loaded tree's state: editor-obsoleted techs, the epoch range, research, each epoch's counters,
// then refreshes the interface
void EETechTree::ApplySavedState(bool inApplyObsoleted, bool inApplyResearch)
{
    EETechTreeWalk(this, EeTechTreeOp::ApplySavedState, inApplyObsoleted ? 1 : 0, inApplyResearch ? 1 : 0);
}

// FUNCTION: EXE 0x5ccbc9
// Reads the saved research, obsolete and editor-obsoleted tech ids, the current/start/end epochs and each epoch's
// state (14 epochs before version 0.0xea); marks every epoch up to the current one researched (ee_tech tree_load)
long EETechTree::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EETechSaveContext theContext = {this, &inFile};
    return ee_tech_walk_tree(&theContext, &EETechSaveHost, EeTechTreeOp::Load, this, (int32_t)inMajorVersion,
                             (int32_t)inMinorVersion, NULL);
}

// FUNCTION: EXE 0x5ccdac
// Reports a researched tech or epoch to the player (epoch hook, statistics series 0x13) and to the researching
// unit (EE_TECH_HOST_ON_RESEARCHED)

// FUNCTION: EXE 0x5ccdea
// Makes a tech or epoch obsolete in its epoch
void EETechTree::MakeObsolete(long inID)
{
    EETechTreeWalk(this, EeTechTreeOp::MakeObsolete, inID);
}

// FUNCTION: EXE 0x5cce23
// Makes the outdated nodes of every epoch up to the current one obsolete (ee_tech tree_obsolete_outdated)

// FUNCTION: EXE 0x5cce4c
// Sets a prerequisite of a tech in its epoch
void EETechTree::SetPrerequisite(long inID, long inPrerequisite)
{
    EETechTreeWalk(this, EeTechTreeOp::SetPrerequisite, inID, inPrerequisite);
}

// FUNCTION: EXE 0x5cce89
// Remembers a tech id to make obsolete when the saved state is applied (the Rust saved lists)

// FUNCTION: EXE 0x5cce9c
// Remembers a tech id to research when the saved state is applied (the Rust saved lists)

// FUNCTION: EXE 0x5cceaf
// Remembers a tech id the scenario editor obsoleted (the Rust saved lists)

// FUNCTION: EXE 0x5ccec2
// Researches a tech or epoch; a researched tech that counts toward the next epoch adds to its count, an epoch
// updates statistics series 0xc
void EETechTree::Research(long inID, EEComplexUnit* inUnit)
{
    if (EESimCaptureRecord::Active()) {   // TEST ONLY (ee/EETechCapture.h)
        EESimCaptureRecord theRecord(EECaptureTag("TTRS"));
        theRecord.I32(inID);
        theRecord.I32(inUnit != NULL ? (long)inUnit->mUnitID : -1);
        EETechCaptureTree(theRecord, this);
        long theNext = this->mCurrentEpoch + 1;
        theRecord.I32(theNext < this->mEpochs.size() ? this->mEpochs[theNext].mCurrentSOL : -1);
        EETechTreeWalk(this, EeTechTreeOp::Research, inID, 0, inUnit);
        EETechCaptureTree(theRecord, this);
        theRecord.I32(theNext < this->mEpochs.size() ? this->mEpochs[theNext].mCurrentSOL : -1);
        theRecord.Commit();
        return;
    }
    EETechTreeWalk(this, EeTechTreeOp::Research, inID, 0, inUnit);
}

// FUNCTION: EXE 0x5ccf5d
// Researches every epoch up to the given one (events disabled) and its kind-7 nodes; shows the epoch name to
// the local player (ee_tech tree_research_to_epoch)

// FUNCTION: EXE 0x5cd046
// Writes the tree to the saved game: the ids of researched, obsolete or editor-obsoleted techs with their flags,
// the epochs, then each epoch's state; returns the last epoch's write result (ee_tech tree_save)
long EETechTree::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EETechSaveContext theContext = {this, &outFile};
    return ee_tech_walk_tree(&theContext, &EETechSaveHost, EeTechTreeOp::Save, this, 0, 0, NULL);
}

// FUNCTION: EXE 0x5cd2a7
// Moves the tree forward to an epoch (never back), recording it in statistics series 0xc and researching up to it
void EETechTree::SetEpoch(long inEpoch)
{
    EETechTreeWalk(this, EeTechTreeOp::SetEpoch, inEpoch);
}

// FUNCTION: EXE 0x5cd2ed
// Sets the start and end epochs and researches up to the start
void EETechTree::SetEpochRange(long inStart, long inEnd)
{
    EETechTreeWalk(this, EeTechTreeOp::SetEpochRange, inStart, inEnd);
}

// FUNCTION: EXE 0x5cd30a
// Sets the epoch range and the current epoch to its start, without researching
void EETechTree::ResetEpochRange(long inStart, long inEnd)
{
    EETechTreeWalk(this, EeTechTreeOp::ResetEpochRange, inStart, inEnd);
}

// FUNCTION: EXE 0x5cd33b
// Makes a node available or unavailable unless it has a research delay
void EETechTree::SetNodeAvailable(long inID, bool inValue)
{
    EETechTreeWalk(this, EeTechTreeOp::SetNodeAvailable, inID, inValue ? 1 : 0);
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
