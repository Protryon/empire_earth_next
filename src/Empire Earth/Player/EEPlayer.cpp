// C:\EEX\Empire Earth\Player\EEPlayer.cpp (__FILE__ 0x165cfa). The player: 0x161825..0x16d346 (the class static keys'
// init chains first, then EEWUnitsOffMapInfo, EEPlayer in definition order, the selected-group and functor helpers it
// defines, and its template COMDATs). Unit flags: the exe's module set. Data: the vtable 0x4429e0.
#include <windows.h>
#include <vector>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GETerrainMesh.h"
#include "ee/EETileLocation.h"
#include "lle/FSFile.h"
#include "lle/FSFileSpec.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEWUnitsOffMapInfo.h"
#include "ee/EEButtonObject.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/UReferenceContainer.h"
#include "ee/EEUserInterface.h"
#include "ee/EEPhysics.h"
#include "ee/EEAreaUtilities.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/UResourceFile.h"
#include "ee/EEAIArea.h"
#include "ee/EETown.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEUnitClassImprovement.h"
#include "lle/NEMessage.h"
#include "ee/EESavedGroup.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EEHerd.h"
#include "ee/EEVectorIO.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EETeam.h"
#include "ee/EEUUUnitFunctor.h"
#include "lle/UIEvent.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIEHotKeyEvents.h"
#include "lle/U2DRectangle.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEAnimal.h"
#include "LostDebug.h"   // synthetic, NOT original source (LOST-DEBUG: EE_LOST_DEBUG)
#include "ee/EEStringIDs.h"
#include "lle/ISKeyCode.h"
// EEUnitGoal.cpp's goal counter (0x964480; Save and Load write and read it: it is not file-static in the original)
// (sGoalCounter: ee/EmpireEarth.h)
// the main game form's blinking buttons (0x27fcf0, cdecl; EEPlayer::RepairPointers keeps its result, so not void as
// EEPhysics.cpp declares it; inferred)
// (EESetCivBuilderButtonBlinking: ee/EmpireEarth.h)

const UString EEPlayer::sClassObjectType("EEPlayer-ClassObjectType");
const UString EEPlayer::sUnitIsComplex("EEPlayer-UnitIsComplex");
const UString EEPlayer::sNumResources("EEPlayer-NumResources");
const UString EEPlayer::sResource("EEPlayer-Resource");
const UString EEPlayer::sPopLimit("EEPlayer-PopLimit");
const UString EEPlayer::sPatrolGroups("EEPlayer-PatrolGroups");
const UString EEPlayer::sHaveTechTree("EEPlayer-HaveTechTree");
const UString EEPlayer::sDiplomaticStance("EEPlayer-DiplomaticStance");
const UString EEPlayer::sStatistics("EEPlayer-Statistics");
const UString EEPlayer::sPopulationCount("EEPlayer-PopulationCount");
const UString EEPlayer::sEndOfGameCount("EEPlayer-EndOfGameCount");
const UString EEPlayer::sObserverMode("EEPlayer-ObserverMode");
const UString EEPlayer::sGeneric("EEPlayer-Generic");
const UString EEPlayer::sSharedLOSFlag("EEPlayer-SharedLOSFlag");
const UString EEPlayer::sSendChatMessage("EEPlayer-SendChatMessage");
const UString EEPlayer::sSendChatMessageType("EEPlayer-SendChatMessageType");

// Deletes each element of a vector of pointers, then clears it; returns the count (`mov eax, ebx` before ret)
// (cdecl template COMDATs 0x16d269.. after the template tail, first used by the dtor) (provisional name)
template <class V> long EEDeletePointers(V& ioVector)
{
    long i = 0;
    for (V::iterator theIterator = ioVector.begin(); theIterator != ioVector.end(); i++, theIterator++)
        delete *theIterator;
    ioVector.clear();
    return i;
}

// std::for_each's functor in the dtor: dereferences each patrol group of mPatrolGroups (0x16d2f9) (provisional name)
struct EEPDereferencer {
    void operator()(EEPPatrolGroup* inGroup) { inGroup->Dereference(); }
};

// the interval of the healing units' hit-point bonus (0x4429b8, just before kTributeFee; ComputeLoadDistance)
// (provisional name)
static const float kPlayerHealInterval = 10000.0f;

// the default tribute fee (0x4429bc, the unit's .rdata just before sPlayerEffects; ResetTechTree loads it as a dword)
// (provisional name)
static const float kTributeFee = 0.4f;

// the effects every player loads (0x4429c0) (provisional name)
static const unsigned long sPlayerEffects[3] = {271, 274, 275};

// FUNCTION: EXE 0x561c55
// Creates an off-map record with no unit, time or reason
EEWUnitsOffMapInfo::EEWUnitsOffMapInfo()
{
    this->mUnit = NULL;
    this->mTime = 0;
    this->mReason = 0;
}

// SYNTHETIC: EXE 0x561c6f
// (the scalar deleting destructor, emitted with the vtable; slot EEWUnitsOffMapInfo+0x0)

// FUNCTION: EXE 0x561c8b
// Destroys the off-map record, releasing its unit
EEWUnitsOffMapInfo::~EEWUnitsOffMapInfo()
{
    this->ReleaseUnit();
}

// FUNCTION: EXE 0x561c96
// Reads the off-map unit's ID, location, time and reason from the saved game; returns the first read error
long EEWUnitsOffMapInfo::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theUnitID = (unsigned long)-1;
    long theResult = inFile.Read(theUnitID);
    this->mUnit = reinterpret_cast<RTSUnit*>(theUnitID);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mLocation.Load(inFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mReason);
    return theResult;
}

// FUNCTION: EXE 0x561ce7
// Releases the reference to the off-map unit and clears it
void EEWUnitsOffMapInfo::ReleaseUnit()
{
    if (this->mUnit != NULL) {
        this->mUnit->Dereference();
        this->mUnit = NULL;
    }
}

// FUNCTION: EXE 0x561cfc
// Turns the loaded unit ID back into a referenced unit (NULL when it no longer exists); always succeeds
long EEWUnitsOffMapInfo::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    RTSUnit* theUnit = gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mUnit));
    if (theUnit != NULL) {
        this->mUnit = theUnit;
        theUnit->Reference();
    } else
        this->mUnit = NULL;
    return kErrorOK;
}

// FUNCTION: EXE 0x561d22
// Writes the off-map unit's ID (-1 for none), location, time and reason to the saved game;
// returns the first write error
long EEWUnitsOffMapInfo::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theUnitID = this->mUnit != NULL ? this->mUnit->GetUnitID() : (unsigned long)-1;
    long theResult = outFile.Write(theUnitID);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mLocation.Save(outFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mReason);
    return theResult;
}

// FUNCTION: EXE 0x561d84
// Replaces the off-map unit with a new one, referencing it and releasing the old one
void EEWUnitsOffMapInfo::SetUnit(RTSUnit* inUnit)
{
    if (inUnit == this->mUnit)
        return;
    this->ReleaseUnit();
    if (inUnit != NULL) {
        this->mUnit = inUnit;
        inUnit->Reference();
    }
}

// FUNCTION: EXE 0x561da5
// Creates a player with a name and ID: resets all state, binds its tech tree to it and creates
// its wonder and construction-site lists
EEPlayer::EEPlayer(const UWideString& inName, unsigned long inPlayerID)
    : RTSPlayer(inName, inPlayerID)
{
    this->mViewedUnit = NULL;
    this->mSelectedViewedUnit = NULL;
    this->Reset();
    this->mTechnologies->SetPlayerIndex(inPlayerID);
    this->mTechnologies->SetPlayer(this);
    this->mComputerPlayer = NULL;
    this->mHumanPlayer = NULL;
    this->mWonders = new UReferenceContainer<EEComplexUnit*>;
    this->mConstructionSites = new UReferenceContainer<EEComplexUnit*>;
}

// SYNTHETIC: EXE 0x562010
// (the scalar deleting destructor, emitted with the vtable; slot EEPlayer+0x0)

// SYNTHETIC: EXE 0x56202c
// (the scalar deleting destructor, emitted with the vtable; slot UReferenceContainer<EEComplexUnit *>+0x0)

// FUNCTION: EXE 0x562048
// Creates an unnamed player: resets all state and creates its wonder and construction-site lists
EEPlayer::EEPlayer()
{
    this->mViewedUnit = NULL;
    this->mSelectedViewedUnit = NULL;
    this->Reset();
    this->mComputerPlayer = NULL;
    this->mHumanPlayer = NULL;
    this->mWonders = new UReferenceContainer<EEComplexUnit*>;
    this->mConstructionSites = new UReferenceContainer<EEComplexUnit*>;
}

// FUNCTION: EXE 0x562297
// Destroys the player: clears and releases every unit group, its media, tech tree, air traffic, upgrades,
// towns, herds, off-map records, patrol groups and wonder/construction-site lists
EEPlayer::~EEPlayer()
{
    unsigned long theCount = this->mRunningGroups.size();
    unsigned long i;
    for (i = 0; i < theCount; i++) {
        this->mRunningGroups[i]->Clear();
        this->mRunningGroups[i]->Dereference();
    }
    this->mEvents.Clear(NULL);
    theCount = this->mFinishedGroups.size();
    for (i = 0; i < theCount; i++) {
        this->mFinishedGroups[i]->Clear();
        this->mFinishedGroups[i]->Dereference();
    }
    this->mActiveGroup->Clear();
    this->mActiveGroup->Dereference();
    this->mSelectedGroup->Clear();
    this->mSelectedGroup->Dereference();
    this->mL9B4->Clear();
    this->mL9B4->Dereference();
    for (long theGroup = 0; theGroup < 20; theGroup++) {
        this->mGroups[theGroup]->Clear();
        this->mGroups[theGroup]->Dereference();
    }
    UReferenceContainer<EEComplexUnit*>* theContainer = this->mConstructionSites;
    theContainer->Clear();
    theContainer->mItems.clear();
    if (this->mViewedUnit != NULL) {
        this->mViewedUnit->Dereference();
        this->mViewedUnit = NULL;
    }
    if (this->mSelectedViewedUnit != NULL) {
        this->mSelectedViewedUnit->Dereference();
        this->mSelectedViewedUnit = NULL;
    }
    gDoppelgangerManager.UnregisterPlayer(this);
    this->UnloadAllMedia();
    delete this->mTechnologies;
    this->mTechnologies = NULL;
    for (long theAirport = 0; theAirport < 4; theAirport++) {
        delete mAirTraffic[theAirport];
        mAirTraffic[theAirport] = NULL;
    }
    for (unsigned long theClass = 0; theClass < 0x367; theClass++)
        delete this->mUnitClassUpgrades.begin()[theClass];
    this->mUnitClassUpgrades.clear();
    EEDeletePointers(this->mTowns);
    this->mTownsByID.clear();
    this->ClearUnitsToRepatrol();
    EEDeletePointers(this->mHerds);
    EEDeletePointers(this->mUnitsOffMap);
    std::for_each(this->mPatrolGroups.begin(), this->mPatrolGroups.end(), EEPDereferencer());
    this->mPatrolGroups.clear();
    delete this->mWonders;
    delete this->mConstructionSites;
    this->UnloadPlayerEffects();
}

// FUNCTION: EXE 0x562677
// Destroys the civilization (members only)
EECivilization::~EECivilization()
{
}

// FUNCTION: EXE 0x562694
// Queues an improvement for a unit type (at the front or back) if it can be bought, paying for it
// unless told not to
// NEAR: frame layout; the original keeps inUnitType in eax and the class in edi, ours spills it ([ebp-4])
void EEPlayer::QueueImprovement(unsigned long inUnitType, unsigned long inImprovement, bool inFront, bool inAlreadyPaid)
{
    EEUnitClass* theClass = gUnitClassTable[inUnitType];
    if (theClass == NULL || !this->CanBuy(inUnitType, inImprovement, NULL))
        return;
    if (!inAlreadyPaid)
        this->PayImprovement(theClass, inImprovement);
    if (!inFront)
        this->mQueuedImprovements[inUnitType].second.push_back(inImprovement);
    else
        this->mQueuedImprovements[inUnitType].second.insert(this->mQueuedImprovements[inUnitType].second.begin(), inImprovement);
}

// FUNCTION: EXE 0x562740
// Records a media item as loaded by the player (ignores NULL)
void EEPlayer::AddLoadedMedia(EEMedia* inMedia)
{
    if (inMedia != NULL)
        this->mLoadedMedia.push_back(inMedia);
}

// FUNCTION: EXE 0x56275a
// Starts every pending command group running, then adds the pending units as the base player does
void EEPlayer::AddPendingUnits(bool inKeepID, bool inReleasePending)
{
    for (unsigned long i = 0; i < this->mPendingGroups.size(); i++)
        this->mRunningGroups.push_back(this->mPendingGroups[i]);
    RTSPlayer::AddPendingUnits(inKeepID, inReleasePending);
}

// FUNCTION: EXE 0x56279f
// Adds a patrol group to the player, keeping a reference to it
void EEPlayer::AddPatrolGroup(EEPPatrolGroup* inGroup)
{
    inGroup->Reference();
    this->mPatrolGroups.push_back(inGroup);
}

// FUNCTION: EXE 0x5627b9
// Starts sharing the player's sight with another player, refreshing its land buildings' sight around the change
void EEPlayer::AddSightSharer(EEPlayer* inPlayer)
{
    EEUnit* theUnit;
    for (theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->mUnitClass->IsLandBuilding())
            theUnit->SightOffHere();
    }
    this->mSightSharers.push_back(inPlayer);
    for (theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->mUnitClass->IsLandBuilding())
            theUnit->SightOnHere();
    }
}

// FUNCTION: EXE 0x562816
// Toggles units in the selection: adds built units not yet selected and removes the rest;
// plays the last added unit's selection sound during local selection
void EEPlayer::AddAndRemoveUnitsFromSelectedGroup(std::vector<long>& inUnitIDs)
{
    EEComplexUnit* theLastUnit = NULL;
    EESelectedGroup* theGroup = this->mActiveGroup;
    this->UpdateSelection(NULL, true);
    for (unsigned long i = 0; i < inUnitIDs.size(); i++) {
        long& theID = inUnitIDs[i];
        RTSUnit* theUnit = this->GetUnit(theID);
        if (theUnit == NULL)
            continue;
        EEComplexUnit* theComplexUnit = static_cast<EEUnit*>(theUnit)->mComplexUnit;
        if (!theGroup->Contains(theComplexUnit) && theComplexUnit->mIsBuilt != false
            && theComplexUnit->mUnitClass->mUnitType != kUnitTypePathPoint) {
            theGroup->Add(theComplexUnit);
            theLastUnit = theComplexUnit;
        } else
            theGroup->RemoveUnit(theComplexUnit);
    }
    if (theLastUnit != NULL && gWorld.mExecutingLocalSelection)
        EEPlayForLocalPlayer(this, theLastUnit->mStats->GetSelectionSound(-1));
}

// FUNCTION: EXE 0x5628cb
// Refunds a fraction of a technology's cost in each resource
void EEPlayer::RefundTechnology(EETechnology* inTechnology, long inUnitType, float inFraction)
{
    for (unsigned long i = 2; i < 7; i++) {
        long theAmount = this->ScaleCost(inTechnology->GetCost(i, inUnitType, 1.0f)) * inFraction;
        this->AddResource(i, theAmount);
    }
}

// FUNCTION: EXE 0x562915
// Refunds a fraction of the cost of a unit class's improvement at its current (queued) level
void EEPlayer::RefundImprovement(EEUnitClass* inClass, long inImprovement, float inFraction)
{
    long theTechnology = gUnitClassTable[inClass->mUnitType] ? gUnitClassTable[inClass->mUnitType]->mTechnology : 100000;
    if (inClass == NULL || theTechnology == kTechNone)
        return;
    EETechTreeNode* theNode = this->mTechnologies->Get(theTechnology);
    if (theNode == NULL || inClass->mImprovements == NULL)
        return;
    long theType = inClass->GetUnitType();
    long theLevel;
    float theScale = inClass->mImprovements->mEntries[inImprovement].mCostFactor;
    theScale = this->GetImprovementLevel(theType, inImprovement) * theScale;
    for (unsigned long i = 2; i < 7; i++) {
        long theCost = theNode->GetCost(i, 0, 1.0f) * theScale + 0.0001f;
        long theAmount = this->ScaleCost(theCost) * inFraction;
        this->AddResource(i, theAmount);
    }
}

// FUNCTION: EXE 0x5629e4
// Selects a unit by ID, starting a new selection when adding a unit of a player other than the selected one
void EEPlayer::AddUnitToSelectedGroup(unsigned long inUnitID, bool inFlag)
{
    RTSUnit* theUnit = ::gWorld.GetUnit(inUnitID);
    if (theUnit == NULL)
        return;
    unsigned long thePlayerID = theUnit->mPlayer->mPlayerID;
    bool theNewGroup;
    if (inFlag && (thePlayerID != this->mPlayerID || this->mActiveGroup->GetFirstUnit() == NULL
                   || this->mActiveGroup->GetFirstUnit()->mPlayer->mPlayerID != thePlayerID))
        theNewGroup = true;
    else
        theNewGroup = false;
    this->SelectUnit(inUnitID, inFlag, theNewGroup);
}

// FUNCTION: EXE 0x562a5d
// Adds the built units of a list of IDs to the selection; plays the last added unit's selection
// sound during local selection
void EEPlayer::AddUnitsToSelectedGroup(std::vector<long>& inUnitIDs)
{
    EEComplexUnit* theLastUnit = NULL;
    EESelectedGroup* theGroup = this->mActiveGroup;
    this->UpdateSelection(NULL, true);
    for (unsigned long i = 0; i < inUnitIDs.size(); i++) {
        long& theID = inUnitIDs[i];
        RTSUnit* theUnit = this->GetUnit(theID);
        if (theUnit == NULL)
            continue;
        EEComplexUnit* theComplexUnit = static_cast<EEUnit*>(theUnit)->mComplexUnit;
        if (!theGroup->Contains(theComplexUnit) && theComplexUnit->mIsBuilt != false
            && theComplexUnit->mUnitClass->mUnitType != kUnitTypePathPoint) {
            theGroup->Add(theComplexUnit);
            theLastUnit = theComplexUnit;
        }
    }
    if (theLastUnit != NULL && gWorld.mExecutingLocalSelection)
        EEPlayForLocalPlayer(this, theLastUnit->mStats->GetSelectionSound(-1));
}

// FUNCTION: EXE 0x562b08
// Pushes back the start time of every wonder's timer by an amount
void EEPlayer::DelayWonderTimers(long inAmount)
{
    for (std::vector<EEComplexUnit*>::iterator it = this->mWonders->mItems.begin(); it != this->mWonders->mItems.end(); ++it)
        (*it)->mCloaked += inAmount;
}

// FUNCTION: EXE 0x562b35
// Adds one of the player's own units to the repatrol list once, keeping a reference to it
void EEPlayer::AddUnitToRepatrol(EEComplexUnit* inUnit)
{
    if (inUnit == NULL || inUnit->mPlayer != this)
        return;
    if (std::find(this->mUnitsToRepatrol.begin(), this->mUnitsToRepatrol.end(), inUnit) == this->mUnitsToRepatrol.end()) {
        this->mUnitsToRepatrol.push_back(inUnit);
        inUnit->Reference();
    }
}

// (gNumPlayers 0x530e0c, which EECPAttackCommander.cpp and EECPAttackSubGroup.cpp also declare, is
// gWorld.mNumPlayers, unsigned: `jbe`)

// FUNCTION: EXE 0x562ba8
// Adds a unit to the player: counts its population, activates it in the world, marks its tech node (researched when
// built), counts vital units, unlocks the capitol technologies on the first capitol and records statistics
void EEPlayer::AddUnit(RTSUnit* inUnit, bool inKeepID)
{
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    EEComplexUnit* theComplexUnit = theUnit->GetComplexUnit();
    this->mPopulationCount += theUnit->mUnitClass->GetPopulationCost();
    RTSPlayer::AddUnit(inUnit, inKeepID);
    if (theComplexUnit != NULL && (!theComplexUnit->mUnitClass->mIsInactive || theComplexUnit->mHitPoints <= 0)
        && theComplexUnit->mPreviousPart == NULL && theComplexUnit->mTileX != -10000)
        gWorld.ActivateUnit(theComplexUnit);
    EETechTreeNode* theNode = this->mTechnologies->Get(theUnit->mUnitClass->mTechnology);
    bool theWasUnavailable = theNode != NULL && !theNode->mIsAvailable;
    this->mTechnologies->SetNodeAvailable(theUnit->mUnitClass->mTechnology, true);
    if (theUnit->GetComplexUnit() != NULL && theUnit->GetComplexUnit()->mIsBuilt && gWorld.mNumPlayers > 0)
        this->mTechnologies->Research(theUnit->mUnitClass->mTechnology, NULL);
    if (theWasUnavailable)
        this->mTechnologies->SetNodeAvailable(theUnit->mUnitClass->mTechnology, false);
    this->UnitPlaced(theUnit);
    if (!theUnit->mUnitClass->IsLandBuilding() && theUnit->mUnitClass->mIsVital
        && (theUnit->GetComplexUnit() == NULL || theUnit->GetComplexUnit()->mPreviousPart == NULL))
        this->mEndOfGameCount++;
    if (this->GetTechTree()->Get(kTechCitizen)->Contains(theUnit->mUnitClass->GetUnitType())) {
        this->mNumCapitols++;
        if (this->mNumCapitols == 1 && !UShutdownInProgress()) {
            gUserInterface->UpdateJITManufacturing();
            if (this->GetTechTree()->Get(kTechSettlement)->GetIsObsolete()) {
                this->GetTechTree()->Get(kTechTownCenter)->mIsAllowed = true;
            } else {
                this->GetTechTree()->Get(kTechSettlement)->mIsAllowed = true;
                this->GetTechTree()->Get(kTechTownCenter)->mIsAllowed = false;
            }
            this->GetTechTree()->Get(kTechCapitol)->mIsAllowed = false;
        }
    }
    if (theUnit->GetComplexUnit() == NULL || gPhysics->GetUnitSwapKind(theUnit->GetComplexUnit()) == 3)
        return;
    long theSeries = EEAddDeadUnitStatistic(theUnit);
    if (theSeries != 0)
        this->mSavedGroups.Reset(theSeries, 1);
}

// FUNCTION: EXE 0x562dab
// Records a unit leaving the map with its tile and the current game time
void EEPlayer::AddOffMapUnit(RTSUnit* inUnit)
{
    EEWUnitsOffMapInfo* theInfo = NULL;
    if (inUnit == NULL)
        return;
    theInfo = new EEWUnitsOffMapInfo;
    theInfo->SetUnit(inUnit);
    theInfo->mLocation = EETileLocation(inUnit->mTileX, inUnit->mTileY);
    theInfo->mTime = gPhysics != NULL ? gPhysics->mGameTime : 0;
    this->mUnitsOffMap.push_back(theInfo);
}

// FUNCTION: EXE 0x562e22
// Puts an animal into a herd it may join (a new herd if none, its holder's herd or a random one if
// several); returns false when it is no herd animal or no herd took it
bool EEPlayer::AddUnitToHerd(EEComplexUnit* inUnit)
{
    std::vector<EEHerd*> theHerds;
    long theAnimal = inUnit->mUnitClass->mAnimalID;
    bool theResult = false;
    if (EEHerd::IsAnimalOnMap(inUnit)) {
        theResult = true;
        for (long i = this->mHerds.size() - 1; i >= 0; i--) {
            if (this->mHerds[i]->CanTake(inUnit, true))
                theHerds.push_back(this->mHerds[i]);
            if (this->mHerds[i]->HasUnit(inUnit))
                return theResult;
        }
        EEHerd* theHerd;
        if (theHerds.size() == 0) {
            theHerd = new EEHerd(this, theAnimal, ++gWorld.mHerdIDCounter);
            theHerd->mSerial = ++gContinentManager.mLastSerial;
            gContinentManager.AddArea(theHerd);
            theResult = theHerd->AddUnit(inUnit);
            if (theResult)
                this->mHerds.push_back(theHerd);
            else
                delete theHerd;
        } else if (theHerds.size() == 1) {
            theHerd = *theHerds.begin();
            theResult = theHerd->AddUnit(inUnit);
        } else {
            long theIndex = -1;
            if (inUnit->mHolder != NULL) {
                for (theIndex = theHerds.size() - 1; theIndex >= 0; theIndex--)
                    if (theHerds[theIndex]->HasUnit(inUnit->mHolder))
                        break;
            }
            if (theIndex == -1)
                theIndex = gPhysics->GetRandom(0L, (long)theHerds.size() - 1);
            theHerd = *(theHerds.begin() + theIndex);
            theResult = theHerd->AddUnit(inUnit);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x563043
// Adds a unit to the stranded list once, keeping a reference to it
void EEPlayer::AddStrandedUnit(EEUnit* inUnit)
{
    std::vector<EEUnit*>::iterator theIterator = std::find(this->mStrandedUnits.begin(), this->mStrandedUnits.end(), inUnit);
    if (theIterator == this->mStrandedUnits.end()) {
        inUnit->Reference();
        this->mStrandedUnits.push_back(inUnit);
    }
}

// FUNCTION: EXE 0x5630ac
// Selects a unit into the active group or as the single viewed unit; returns false for a foreign unit
// while own units are selected
bool EEPlayer::TrySelectUnit(EEUnit* inUnit, bool inFlag)
{
    bool theResult = true;
    if (inFlag) {
        if (inUnit->mPlayer == this) {
            if (inUnit->mTileNode != NULL) {
                this->UpdateSelection(NULL, true);
                if (!this->mActiveGroup->Contains(inUnit->GetComplexUnit()))
                    this->mActiveGroup->Add(inUnit->GetComplexUnit());
            }
        } else if (this->mActiveGroup->GetFirstUnit() != NULL)
            theResult = false;
        else
            this->UpdateSelection(inUnit, true);
    } else if (inUnit->mTileNode != NULL) {
        this->UpdateSelection(NULL, true);
        if (!this->mActiveGroup->Contains(inUnit->GetComplexUnit()))
            this->mActiveGroup->Add(inUnit->GetComplexUnit());
    } else if (this->mActiveGroup->GetNumUnits() == 0)
        this->UpdateSelection(inUnit, true);
    return theResult;
}

// Town merge is defined after its existing patrol-goal include below.

// FUNCTION: EXE 0x563605
// Removes a town ID from the player's town lookup
void EEPlayer::RemoveTownID(long inID)
{
    std::map<long, EETown*>::iterator theIterator = this->mTownsByID.find(inID);
    if (theIterator != this->mTownsByID.end())
        this->mTownsByID.erase(theIterator);
}

// FUNCTION: EXE 0x56363e
// Returns the last unit of the patrol route, or NULL when it is empty
EEComplexUnit* EEPPatrolGroup::GetLast()
{
    if (this->mPatrolPoints.size() > 0)
        return this->mPatrolPoints.back();
    return NULL;
}

// (0x563659: EEUGPatrol::SetPatrolGroup, defined after ee/EEUGPatrol.h's include below)

// FUNCTION: EXE 0x56367b
// Detaches a unit from the computer player's control
void EEPlayer::DetachUnitFromAI(EEComplexUnit* inUnit)
{
    if (inUnit->mIsDetachedFromAI != true)
        inUnit->mIsDetachedFromAI = true;
}

// FUNCTION: EXE 0x56368f
// Adds a unit to the player's construction sites unless it is already there
void EEPlayer::AddConstructionSite(EEComplexUnit* inUnit)
{
    if (!this->mConstructionSites->Contains(inUnit))
        this->mConstructionSites->Add(inUnit);
}

// FUNCTION: EXE 0x5636cd
// Records a wonder (starting its timer unless told not to); the first wonder of a type grants its wonder power
void EEPlayer::AddWonder(EEComplexUnit* inUnit, bool inIsLoading)
{
    if (!inIsLoading) {
        this->mWonders->Add(inUnit);
        inUnit->mCloaked = gPhysics->mGameTime;
        this->mSavedGroups.Reset(0x10, 1);
    }
    if (this->mWonderTypeCounts.find(inUnit->mUnitClass->mUnitType) == this->mWonderTypeCounts.end()) {
        this->mWonderTypeCounts[inUnit->mUnitClass->mUnitType] = 1;
        this->AddWonderPower(inUnit);
    } else
        this->mWonderTypeCounts[inUnit->mUnitClass->mUnitType]++;
}

// FUNCTION: EXE 0x56379e
// Recounts the population, the vital units that keep the player alive and the vital buildings
void EEPlayer::RecountPopulation()
{
    long thePopulation = 0;
    long theCount = 0;
    long theOtherCount = 0;
    EEComplexUnit* theUnit = NULL;
    while ((theUnit = this->GetNextComplexUnit(theUnit)) != NULL) {
        if (!(theUnit->mHitPoints > 0))
            continue;
        if (theUnit->mContainer == NULL || theUnit->mContainer->mUnitClass->mCategory != kUnitCategoryFortress)
            thePopulation += theUnit->mUnitClass->GetPopulationCost();
        if (theUnit->mUnitClass->mIsVital && theUnit->mPreviousPart == NULL
            && (!theUnit->mUnitClass->IsLandBuilding() || theUnit->mIsBuilt == true))
            theCount++;
        if (theUnit->mUnitClass->IsLandBuilding() && theUnit->mUnitClass->mIsVital)
            theOtherCount++;
    }
    this->mPopulationCount = thePopulation;
    this->mEndOfGameCount = theCount;
    this->mNumVitalBuildings = theOtherCount;
}

// FUNCTION: EXE 0x56384d
// Returns whether an improvement for a unit type can be bought: allowed, within the improvement limits
// counting the queue, and affordable; otherwise sets the reason text
bool EEPlayer::CanBuy(long inUnitType, long inImprovement, UWideString* outText)
{
    bool theResult = false;
    EEUnitClass* theClass = gUnitClassTable[inUnitType];
    long theTechnology = theClass != NULL ? theClass->mTechnology : 100000;
    if (theClass != NULL && theTechnology != kTechNone) {
        EEUnitClassImprovementTable* theTable = theClass->mImprovements;
        EEUnitClassUpgrade* theUpgrade = this->mUnitClassUpgrades[inUnitType];
        EEUnitClassImprovement* theImprovement = &theUpgrade->mImprovement;
        if ((inImprovement == 12 ? 0.0f : theTable->mEntries[inImprovement].mStepFactor) > 0.0f &&
            theImprovement->CanUpgrade(theUpgrade->mUnitClass, inImprovement) == true) {
            long theQueued = theImprovement->mTotalCost;
            std::map<unsigned long, std::pair<float, std::vector<unsigned long> > >::iterator theIterator = this->mQueuedImprovements.find(inUnitType);
            if (theIterator != this->mQueuedImprovements.end()) {
                const std::vector<unsigned long>& theQueue = this->mQueuedImprovements[inUnitType].second;
                for (unsigned long i = 0; i < theQueue.size(); i++)
                    theQueued += theImprovement->GetCost(theQueue[i]);
            }
            if ((unsigned long)(theImprovement->GetCost(inImprovement) + theQueued) <=
                (unsigned long)gWorldTable.GetLongValue(47) &&
                this->GetImprovementLevel(inUnitType, inImprovement) <= theTable->GetMaxLevel()) {
                float theScale = theTable->mEntries[inImprovement].mCostFactor;
                long theLevel = this->GetImprovementLevel(inUnitType, inImprovement);
                theResult = this->CanAffordTechnology(theTechnology, theLevel * theScale, outText);
            } else if (outText != NULL)
                gResource->GetString(IDS_MAXIMUM_NUMBER_OF_IMPROVEMENTS_HAVE_BEEN, *outText);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5639e8
// Returns whether the player can produce a unit type (cost and wonder limits included); sets the reason
bool EEPlayer::CanProduceUnitType(long inUnitType, long inArg, UWideString* outReason)
{
    long theTechnology = gUnitClassTable[inUnitType] ? gUnitClassTable[inUnitType]->mTechnology : 100000;
    if (theTechnology != kTechNone)
        return this->CanProduce(theTechnology, inArg, 1, true, 3, outReason);
    return false;
}

// FUNCTION: EXE 0x563a25
// Returns whether a technology or unit can be produced: population, teleporter and wonder limits, cost
// (optional) and a researched producer class; sets the reason text when not
bool EEPlayer::CanProduce(long inTechnology, long inUnitClass, long inCount, bool inCheckCost, long inWonderMode, UWideString* outReason)
{
    EETechTreeNode* theNode = this->mTechnologies->Get(inTechnology);
    bool theAllowed = false;
    if (theNode != NULL && theNode->GetIsAvailable()) {
        theAllowed = true;
        if (theNode->mData->mMakesUnit) {
            EEUnitClass* theClass = gUnitClassTable[theNode->mData->mUnitClass];
            if (theClass != NULL) {
                if (theClass->mWeight == 0
                    || theClass->GetPopulationCost() + this->mPopulationCount <= this->GetExtraPopulation(theNode->mData->mUnitClass) + this->GetPopulationLimit(0)
                    || (this->mPopulationCount == this->GetPopulationLimit(0) - 1
                        && theClass->GetPopulationCost() + this->mPopulationCount <= this->GetExtraPopulation(theNode->mData->mUnitClass) + this->GetPopulationLimit(0))) {
                    theAllowed = true;
                    if (theClass->mUnitType == kUnitTypeTeleporter) {
                        theAllowed = this->mNumTeleporters < 1;
                        if (!theAllowed && outReason != NULL)
                            gResource->GetString(IDS_YOU_HAVE_ALREADY_BUILT_MAXIMUM, *outReason);
                    }
                    if (theAllowed == true && theClass->mAIKind == kAIKindHero && !gWorld.mGameOptions.mIsScenarioGame && inWonderMode != 0) {
                        if (inWonderMode == 3)
                            theAllowed = this->GetHeroCount(true, true) < this->mHeroLimit;
                        else if (inWonderMode == 1)
                            theAllowed = this->GetHeroCount(false, true) < this->mHeroLimit;
                        else if (inWonderMode == 2)
                            theAllowed = this->GetHeroCount(true, false) < this->mHeroLimit;
                        if (!theAllowed && outReason != NULL)
                            gResource->GetString(IDS_YOU_HAVE_REACHED_YOUR_POP_LIMIT, *outReason);
                    }
                } else {
                    theAllowed = false;
                    if (outReason != NULL)
                        gResource->GetString(IDS_UNIT_EXCEEDS_POP_LIMIT, *outReason);
                }
            }
        }
        if (inCheckCost == true && theAllowed) {
            theAllowed = this->CanAfford(theNode, inUnitClass, inCount);
            if (!theAllowed && outReason != NULL) {
                if (theNode->mData->mMakesUnit)
                    gResource->GetString(IDS_NOT_ENOUGH_RESOURCES_FOR_CONSTRUCTION, *outReason);
                else
                    gResource->GetString(IDS_NOT_ENOUGH_RESOURCES_TO_RESEARCH_THIS_TECHNOLOGY, *outReason);
            }
        }
        if (theAllowed) {
            theAllowed = theNode->GetNumUnitClasses() > 0;
            for (long i = theNode->GetNumUnitClasses() - 1; i >= 0 && !theAllowed; i--) {
                EEUnitClass* theClass = gUnitClassTable[theNode->GetUnitClass(i)];
                long theTechnology = theClass != NULL ? theClass->mTechnology : 100000;
                EETechTreeNode* theClassNode = this->mTechnologies->Get(theTechnology);
                theAllowed = theClassNode != NULL ? theClassNode->mIsResearched : false;
            }
        }
    }
    return theAllowed;
}

// FUNCTION: EXE 0x563c6d
// Returns whether a unit type's improvement is allowed and within the improvement limits, ignoring the queue
bool EEPlayer::CanImprove(long inUnitType, long inImprovement, UWideString* outReason)
{
    bool theResult = false;
    EEUnitClass* theClass = gUnitClassTable[inUnitType];
    long theTechnology = theClass != NULL ? theClass->mTechnology : 100000;
    if (theClass != NULL && theTechnology != kTechNone) {
        EEUnitClassUpgrade* theUpgrade = this->mUnitClassUpgrades[inUnitType];
        long theQueued = theUpgrade->mImprovement.mTotalCost;
        EEUnitClassImprovementTable* theTable = theClass->mImprovements;
        if ((inImprovement != 12 ? theTable->mEntries[inImprovement].mStepFactor : 0.0f) > 0.0f
            && theUpgrade->mImprovement.CanUpgrade(theUpgrade->mUnitClass, inImprovement) == true
            && theUpgrade->mImprovement.GetCost(inImprovement) + theQueued <= gWorldTable.GetLongValue(47)
            && theUpgrade->mImprovement.GetLevel(inImprovement) <= theTable->GetMaxLevel())
            theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x563d1f
// Returns whether the player can afford a technology at a cost scale; sets the reason text when not
bool EEPlayer::CanAffordTechnology(long inTechnology, float inScale, UWideString* outReason)
{
    EETechTreeNode* theNode = this->mTechnologies->Get(inTechnology);
    bool theResult = this->CanAffordScaled(theNode, 0, inScale, 1);
    if (!theResult && outReason != NULL)
        gResource->GetString(IDS_NOT_ENOUGH_RESOURCES_FOR_CONSTRUCTION, *outReason);
    return theResult;
}


// FUNCTION: EXE 0x563d6f
// Does nothing (the base player ignores being targeted by another player)
// (ICF-kept empty `ret 8`, shared by the slots below; FlareReceived below (slot +0x98) is the same empty body,
// folded into it: the base player ignores flares)
void EEPlayer::NoticeTargetPlayer(EEPlayer* inPlayer, bool inFlag)
{
}

void EEPlayer::FlareReceived(EEPlayer* inFrom, const U2DPoint<long>& inTile)
{
}

// (slots sharing the empty body: EECPAttackFormation+0x14; EEHumanPlayer+0x58, +0x98; EECalamityClass+0x28;
// EECCConscription, EECCLineOfSight, EECCSpecialAttack, EECCSpecialLocation +0x28; EEE3DFarm+0x20;
// EEPlayer+0x58, +0x98; EESETSCTAlwaysTrue+0x4, +0xc; EESETSETNoEffect, EESETSETRemoveObject, EESETSETKillObject,
// EESETSETFreezeObject, EESETSETObjectCreate +0x8, +0x10; EESETSETPVisibility, EESETSETPCameraFadeOut,
// EESETSETPCameraFadeIn, EESETSETPlayerVictory +0x8; EEUnit+0x3c, +0x44, +0x68)

// FUNCTION: EXE 0x563d72
// For a human player whose unit class has its button loaded, releases one button's media
// and acquires the other's
void EEPlayer::SwapClassButtonMedia(unsigned long inUnitClass, EEButtonObject* inFirst, EEButtonObject* inSecond)
{
    if (inUnitClass >= 0x367)
        return;
    EEUnitStats* theStats = this->mUnitClassUpgrades[inUnitClass];
    if (theStats == NULL || !(theStats->mButtonLoadCount > 0) || this->IsHuman() != true)
        return;
    if (inFirst != NULL)
        inFirst->Release();
    if (inSecond != NULL)
        inSecond->Acquire();
}

// FUNCTION: EXE 0x563db9
// Sets one of a unit class's acknowledgement sounds for the player
void EEPlayer::SetClassAcknowledgeSound(unsigned long inUnitClass, EESoundObject* inSound, long inIndex)
{
    if (inUnitClass >= 0x367)
        return;
    EEUnitStats* theStats = this->mUnitClassUpgrades[inUnitClass];
    if (theStats != NULL)
        theStats->SetAcknowledgeSound(inSound, inIndex);
}

// FUNCTION: EXE 0x563de1
// Sets a unit class's creation sound for the player
void EEPlayer::SetClassCreationSound(unsigned long inUnitClass, EESoundObject* inSound)
{
    if (inUnitClass >= 0x367)
        return;
    EEUnitStats* theStats = this->mUnitClassUpgrades[inUnitClass];
    if (theStats != NULL)
        theStats->SetCreationSound(inSound);
}

// FUNCTION: EXE 0x563e05
// Sets a unit class's death sound for the player
void EEPlayer::SetClassDeathSound(unsigned long inUnitClass, EESoundObject* inSound)
{
    if (inUnitClass >= 0x367)
        return;
    EEUnitStats* theStats = this->mUnitClassUpgrades[inUnitClass];
    if (theStats != NULL)
        theStats->SetDeathSound(inSound);
}

// FUNCTION: EXE 0x563e29
// Sets a unit class's model for the player
void EEPlayer::SetClassModel(unsigned long inUnitClass, EEModel* inModel)
{
    if (inUnitClass >= 0x367)
        return;
    EEUnitStats* theStats = this->mUnitClassUpgrades[inUnitClass];
    if (theStats != NULL)
        theStats->SetModel(inModel);
}

// FUNCTION: EXE 0x563e4d
// Sets a unit class's button for the player
void EEPlayer::SetClassButton(unsigned long inUnitClass, EEButtonObject* inButton)
{
    if (inUnitClass >= 0x367)
        return;
    EEUnitStats* theStats = this->mUnitClassUpgrades[inUnitClass];
    if (theStats != NULL)
        theStats->SetButton(inButton);
}

// FUNCTION: EXE 0x563e71
// Sets one of a unit class's selection sounds for the player
void EEPlayer::SetClassSelectionSound(unsigned long inUnitClass, EESoundObject* inSound, long inIndex)
{
    if (inUnitClass >= 0x367)
        return;
    EEUnitStats* theStats = this->mUnitClassUpgrades[inUnitClass];
    if (theStats != NULL)
        theStats->SetSelectionSound(inSound, inIndex);
}

// FUNCTION: EXE 0x563e99
// Deletes all of the player's herds
void EEPlayer::DeleteHerds()
{
    for (long i = this->mHerds.size() - 1; i >= 0; i--)
        delete this->mHerds[i];
    this->mHerds.clear();
}

// FUNCTION: EXE 0x563ee2
// Deletes all of the player's towns and empties the town lookup
void EEPlayer::ClearTowns()
{
    for (long i = this->mTowns.size() - 1; i >= 0; i--)
        delete this->mTowns[i];
    this->mTowns.clear();
    this->mTownsByID.clear();
}

// FUNCTION: EXE 0x563f36
// Drops every queued improvement of a unit type
// NEAR: esi/edi swapped (end() and the map pointer); tried: end() first, an end local, an early return
void EEPlayer::EraseQueuedImprovements(unsigned long inUnitType)
{
    if (this->mQueuedImprovements.find(inUnitType) != this->mQueuedImprovements.end())
        this->mQueuedImprovements.erase(this->mQueuedImprovements.find(inUnitType));
}

// FUNCTION: EXE 0x563f7e
// Drops the pending command groups, then releases the pending units as the base player does
void EEPlayer::ReleasePendingUnits()
{
    this->mPendingGroups.clear();
    RTSPlayer::ReleasePendingUnits();
}

// FUNCTION: EXE 0x563f95
// Empties a numbered control group and tells the user interface
void EEPlayer::ClearSavedGroup(long inGroup)
{
    this->mGroups[inGroup]->Clear();
    if (gUserInterface != NULL)
        gUserInterface->PostRefreshSavedGroup(inGroup + 1);
}

// FUNCTION: EXE 0x563fbb
// Releases every unit waiting to repatrol and empties the list
void EEPlayer::ClearUnitsToRepatrol()
{
    for (long i = this->mUnitsToRepatrol.size() - 1; i >= 0; i--) {
        if (this->mUnitsToRepatrol[i] != NULL)
            this->mUnitsToRepatrol[i]->Dereference();
    }
    this->mUnitsToRepatrol.clear();
}

// FUNCTION: EXE 0x564003
// Handles a finished unit: registers wonders (telling the others when victory is on), counts vital
// buildings, applies the civ-power sight pulse, and updates area effects, construction sites, towns and walls
void EEPlayer::UnitCompleted(EEComplexUnit* inUnit)
{
    EECivPower thePower;
    EETechTreeNode* theNode = this->mTechnologies->Get(inUnit->mUnitClass->mTechnology);
    if (theNode != NULL && theNode->mData->mKind == 4) {
        inUnit->mPlayer->AddWonder(inUnit, false);
        if (gWorld.mVictoryEnabled) {
            for (unsigned long i = 0; i < gWorld.mNumPlayers; i++)
                if (gWorld.mPlayers[i] != this)
                    gWorld.mPlayers[i]->WonderStarted(inUnit);
        }
    }
    if (inUnit->mUnitClass->IsLandBuilding() && inUnit->mPreviousPart == NULL && inUnit->mUnitClass->mIsVital)
        this->mEndOfGameCount++;
    if (inUnit->mUnitClass->IsLandBuilding() && inUnit->mUnitClass->mIsVital) {
        this->mNumVitalBuildings++;
        if (this->mNumVitalBuildings == 1)
            this->EnableCamouflagePower();
    }
    if (gCivPowersManager.HasPower(this, kCivPowerExploration) &&
        (inUnit->mUnitClass->mUnitType == kUnitTypeTownCenter || inUnit->mUnitClass->mUnitType == kUnitTypeCapitol)) {
        gCivPowersManager.GetPower(kCivPowerExploration, thePower);
        this->PulseSight(true, thePower.mAmount, inUnit->mUnitID);
    }
    this->ClearFootprint(inUnit);
    if (inUnit->mUnitClass->mUnitType == kUnitTypeHouse)
        this->SuggestRecomputeAllAreaEffects();
    if (inUnit->mUnitClass->mIsWall)
        this->RemoveConstructionSite(inUnit);
    if (gLoadingSavedGame || !gGameScreenActive)
        return;
    this->AddUnitToTown(inUnit);
    if (inUnit->mUnitClass->mBlocksTile || inUnit->mUnitClass->mCategory == kUnitCategoryGate)
        gContinentManager.UpdateWallUnit(inUnit, inUnit->mUnitClass->mCategory == kUnitCategoryGate);
}

// FUNCTION: EXE 0x56419b
// Moves the improvement queue of one unit type to another
void EEPlayer::MoveQueuedImprovements(unsigned long inFrom, unsigned long inTo)
{
    if (this->mQueuedImprovements.find(inFrom) == this->mQueuedImprovements.end())
        return;
    this->mQueuedImprovements[inTo].first = this->mQueuedImprovements[inFrom].first;
    this->mQueuedImprovements[inTo].second = this->mQueuedImprovements[inFrom].second;
    this->mQueuedImprovements.erase(this->mQueuedImprovements.find(inFrom));
}

// FUNCTION: EXE 0x564223
// Returns the number of living heroes and/or heroes in production
long EEPlayer::GetNumHeroes(bool inFirst, bool inSecond)
{
    long theResult = 0;
    if (inFirst)
        theResult = this->mNumHeroes;
    if (inSecond)
        theResult += this->mNumHeroesInProduction;
    return theResult;
}

// FUNCTION: EXE 0x564241
// Clears every goal queue of a complex unit leaving the player
void EEPlayer::UnitRemoved(RTSUnit* inUnit)
{
    EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(inUnit);
    if (theUnit != NULL) {
        for (unsigned long i = 0; i < theUnit->GetNumGoalQueues(); i++)
            theUnit->ClearGoalQueue(i);
    }
}

// FUNCTION: EXE 0x564287
// Removes the player's sight from a tile, and its allies' sight when line of sight is shared
void EEPlayer::RemoveSharedSight(RTSTile* inTile)
{
    if (inTile == NULL)
        return;
    inTile->RemoveSight(this);
    gDoppelgangerManager.TileHidden(inTile, this);
    if (this->mSharedLOS != true)
        return;
    for (long i = this->mAllies.size() - 1; i >= 0; i--) {
        if (this->mAllies[i]->mSharedLOS == true) {
            inTile->RemoveSight(this->mAllies[i]);
            gDoppelgangerManager.TileHidden(inTile, this->mAllies[i]);
        }
    }
}

// FUNCTION: EXE 0x564309
// Removes the player's and its sight-sharing allies' sight from the tile at a location
void EEPlayer::RemoveSharedSightAt(const EETileLocation& inLocation)
{
    this->RemoveSharedSight(gWorld.mMap->GetTileAtLocation(inLocation));
}

// FUNCTION: EXE 0x564327
// Removes only the player's own sight from a tile
void EEPlayer::RemoveOwnSight(RTSTile* inTile)
{
    if (inTile != NULL) {
        inTile->RemoveSight(this);
        gDoppelgangerManager.TileHidden(inTile, this);
    }
}

// FUNCTION: EXE 0x56434c
// Removes only the player's own sight from the tile at a location
void EEPlayer::RemoveOwnSightAt(const EETileLocation& inLocation)
{
    this->RemoveOwnSight(gWorld.mMap->GetTileAtLocation(inLocation));
}

// FUNCTION: EXE 0x56436a
// Writes the player's per-class media counts and media load warnings to a text file
void EEPlayer::DumpMedia(FSFileSpec& inFile)
{
    UWideString theString;
    fprintf(inFile, "Media Information For Player %d, The Players Name is %s\n\n", this->mPlayerID, (const char*)this->mName);
    for (unsigned long i = 0; i < this->mUnitClassUpgrades.size(); i++) {
        if (this->mUnitClassUpgrades[i] != NULL)
            this->mUnitClassUpgrades[i]->DumpMediaCount(inFile);
    }
    fprintf(inFile, "\n\n\t\tMedia that might not have been loaded correctly:\n");
    for (unsigned long j = 0; j < this->mMediaWarnings.size(); j++)
        fprintf(inFile, "\t\t%s", (const char*)this->mMediaWarnings[j]);
}

// FUNCTION: EXE 0x56442b
// Marks the player as having met another player when it first sees a unit of a non-gaia player
void EEPlayer::NoticeForeignUnit(EEUnit* inUnit)
{
    if (this->mCombatState == 0 && inUnit->mPlayer->mPlayerID != 0)
        this->mCombatState = 1;
}

// FUNCTION: EXE 0x564453
// Puts every herd animal of the player that is in no herd into one
void EEPlayer::AssignHerds()
{
    std::vector<EEComplexUnit*> theUnits;
    for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplex = theUnit->GetComplexUnit();
        bool theFound = false;
        if (theComplex == NULL || !EEHerd::IsAnimalOnMap(theComplex))
            continue;
        for (long i = this->mHerds.size() - 1; i >= 0 && !theFound; i--)
            theFound = this->mHerds[i]->HasUnit(theComplex);
        if (!theFound)
            theUnits.push_back(theComplex);
    }
    for (long j = theUnits.size() - 1; j >= 0; j--)
        this->AddUnitToHerd(theUnits[j]);
}

// FUNCTION: EXE 0x56451a
// Adds every land building outside the player's towns to a town, then gives each town without
// a patrol group one
void EEPlayer::AssignTowns()
{
    std::vector<EEComplexUnit*> theUnits;
    U2DPoint<long> theTile;
    for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplex = theUnit->GetComplexUnit();
        bool theFound = false;
        if (theComplex == NULL || !theComplex->mUnitClass->IsLandBuilding() || theComplex->mContainer != NULL)
            continue;
        if (theComplex->mTileX != -10000) {
            theTile.mX = theUnit->mTileX;
            theTile.mY = theUnit->mTileY;
        } else {
            long theTileY = UMFloor(theComplex->mLastMapPosition.GetY());
            long theTileX = UMFloor(theComplex->mLastMapPosition.GetX());
            theTile.mX = theTileX;
            theTile.mY = theTileY;
        }
        if (theTile.mX == -10000)
            continue;
        for (long i = this->mTowns.size() - 1; i >= 0 && !theFound; i--)
            theFound = this->mTowns[i]->Contains(theTile);
        if (!theFound)
            theUnits.push_back(theComplex);
    }
    for (long j = theUnits.size() - 1; j >= 0; j--)
        this->AddUnitToTown(theUnits[j]);
    for (long k = this->mTowns.size() - 1; k >= 0; k--) {
        if (this->mTowns[k] != NULL && this->mTowns[k]->mPatrolGroup == NULL)
            (*(this->mTowns.begin() + k))->RefreshPatrolGroup();
    }
}

// FUNCTION: EXE 0x5646fc
// Executes a selection message against the player's network selection, keeping its local
// active selection and viewed unit
void EEPlayer::ExecuteSelectionMessage(NEMessage* inMessage)
{
    RTSUnit* theUnit = this->mViewedUnit;
    EESelectedGroup* theGroup = this->mActiveGroup;
    if (theUnit != NULL)
        theUnit->Reference();
    else
        theUnit = NULL;
    this->mActiveGroup = this->mSelectedGroup;
    gWorld.mExecutingLocalSelection = true;
    inMessage->Execute();
    gWorld.mExecutingLocalSelection = false;
    this->mSelectedGroup = this->mActiveGroup;
    this->mActiveGroup = theGroup;
    this->SetSelectedViewedUnit(this->mViewedUnit);
    this->UpdateSelection(static_cast<EEUnit*>(theUnit), false);
    if (theUnit != NULL)
        theUnit->Dereference();
}

// FUNCTION: EXE 0x564781
// Returns the town nearest to a tile, optionally only on the tile's continent; NULL when none
EEAIArea* EEPlayer::GetNearestArea(const EETileCoordinate& inTile, bool inSameContinent)
{
    EETileDistanceLess theClosest(0, 0);
    unsigned short theContinent = gContinentManager.GetContinentID(inTile.GetX(), inTile.GetY());
    long theBest = 0;
    EEAIArea* theArea = NULL;
    for (long i = this->GetNumTowns() - 1; i >= 0; i--) {
        EETown* theTown = this->mTowns[i];
        if (theTown == NULL || (inSameContinent && theTown->mContinent != theContinent))
            continue;
        theClosest.mCenter = theTown->GetClosestPoint(inTile);
        long theDistance = theClosest.DistanceSquared(inTile);
        if (theArea == NULL || theDistance < theBest) {
            theBest = theDistance;
            theArea = theTown;
        }
    }
    return theArea;
}

// FUNCTION: EXE 0x564820
// Returns the construction site nearest to a unit within a squared distance of 100, or NULL
EEUnit* EEPlayer::GetNearestConstructionSite(EEComplexUnit* inUnit)
{
    EEUnit* theResult = NULL;
    long theBestDistance = 100;
    EETileCoordinate theTile;
    theTile.mX = 0;
    theTile.mY = 0;
    EETileDistanceLess theCenter(inUnit->mTileX, inUnit->mTileY);
    for (std::vector<EEComplexUnit*>::iterator theIterator = this->mConstructionSites->mItems.begin();
         theIterator != this->mConstructionSites->mItems.end(); ++theIterator) {
        theTile.mX = (*theIterator)->mTileX;
        theTile.mY = (*theIterator)->mTileY;
        long theDistance = theCenter.DistanceSquared(theTile);
        if (theDistance < theBestDistance) {
            theResult = *theIterator;
            theBestDistance = theDistance;
        }
    }
    return theResult;
}

#include "ee/EEGameVariant.h"
#include "ee/EEUnitUtilities.h"

// FUNCTION: EXE 0x564891
// Grants a wonder's power by its bonus kind: area conversion, a hit-point bonus for a unit set, the
// share bonus, sight of enemies, or healing of a unit set
void EEPlayer::AddWonderPower(EEComplexUnit* inUnit)
{
    switch (inUnit->mUnitClass->mWonderBonus) {
    case 1:
        this->mWonderPowers.push_back(1);
        this->mAreaConversion = true;
        break;
    case 2: {
        this->mWonderPowers.push_back(2);
        EEUnitSet* theSet = gUnitSetManager.GetSet(inUnit->mUnitClass->mWonderUnitSet);
        for (unsigned long i = 0; i < this->mUnitClassUpgrades.size(); i++) {
            if (this->GetUpgrade(i) == NULL || !theSet->Contains(this->GetUpgrade(i)->mUnitClass->mUnitType))
                continue;
            long theType = this->GetUpgrade(i)->mUnitClass->mUnitType;
            float theFactor = gWorld.GetGameVariant()->GetFactor(theType);
            float theAmount = gWorldTable.GetFloatValue(0x32);
            EEUpgradeHitPoints(this->mUnitClassUpgrades[i], this->mUnitClassUpgrades[i]->mUnitClass->mUnitType,
                       (theFactor + 1.0f) * theAmount, this);
        }
        break;
    }
    case 3:
        this->mWonderPowers.push_back(3);
        break;
    case 4:
        gWorld.RemoveShareBonus();
        this->mWonderPowers.push_back(4);
        gWorld.ApplyShareBonus();
        break;
    case 5: {
        this->mWonderPowers.push_back(5);
        for (unsigned long thePlayer = 1; thePlayer < gWorld.mNumPlayers; thePlayer++)
            if (thePlayer != this->GetPlayerID() && gWorld.mPlayers[thePlayer] != NULL && this->mEnemies[thePlayer] == 1)
                gWorld.mPlayers[thePlayer]->AddSightSharer(this);
        break;
    }
    case 6:
        this->mHealsUnits = true;
        this->mHealTimer = kPlayerHealInterval;
        this->mHealedUnitSet = gUnitSetManager.GetSet(inUnit->mUnitClass->mWonderUnitSet);
        this->mWonderPowers.push_back(6);
        break;
    }
}

// FUNCTION: EXE 0x564b4f
// Returns the running, or else pending, command group holding a unit; NULL when none
EEGroup* EEPlayer::GetActiveGroupOf(EEComplexUnit* inUnit)
{
    EEGroup* theResult = NULL;
    for (long i = this->mRunningGroups.size() - 1; i >= 0; i--) {
        if (this->mRunningGroups[i]->Contains(inUnit)) {
            theResult = this->mRunningGroups[i];
            break;
        }
    }
    if (theResult != NULL)
        return theResult;
    for (long j = this->mPendingGroups.size() - 1; j >= 0; j--) {
        if (this->mPendingGroups[j]->Contains(inUnit)) {
            theResult = this->mPendingGroups[j];
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x564bea
// Returns whether the player can afford to repair a number of a unit's hit points (cost relative to its maximum)
bool EEPlayer::CanAffordRepair(EEUnit* inUnit, EEUnit* inOther, unsigned long inValue)
{
    unsigned long theMaximum = inUnit->mStats->mMaxHitPoints;
    EETechTreeNode* theNode = this->GetTechTree()->Get(inUnit->mUnitClass->GetTechnology());
    return this->CanAffordScaled(theNode, inOther->mUnitClass->GetUnitType(), (float)inValue / theMaximum, 1);
}

// FUNCTION: EXE 0x564c4c
// Adds the player's resources, population and tech tree to the sync checksum
void EEPlayer::GetChecksum(ESChecksum& ioChecksum)
{
    unsigned long theSum = 0;
    RTSPlayer::GetChecksum(ioChecksum);
    for (unsigned char i = 0; i < 7; i++)
        theSum += this->mResourceCaps[i];
    theSum += this->mPopulationCount;
    this->mTechnologies->GetChecksum(ioChecksum);
    UString theName("EEPlayer");
    ioChecksum.AddChecksum(theName, theSum);
}

// FUNCTION: EXE 0x564cb0
// Returns the town containing a tile, or else the nearest town and its closest point to the tile
EETown* EEPlayer::GetNearestTown(const EETileLocation& inTile, EETileLocation& outTile)
{
    EETown* theTown = NULL;
    EETileLocation theCandidate;
    long theDistance = 0;
    for (long i = this->mTowns.size() - 1; i >= 0; i--) {
        if (!this->mTowns[i]->mHull.BoundedContains(inTile)) {
            if (theTown == NULL) {
                outTile = this->mTowns[i]->GetClosestPoint(inTile);
                theTown = this->mTowns[i];
                theDistance = outTile.DistanceToSquared(inTile);
            } else {
                theCandidate = this->mTowns[i]->GetClosestPoint(inTile);
                if (theCandidate.DistanceToSquared(inTile) < theDistance) {
                    outTile = theCandidate;
                    theDistance = theCandidate.DistanceToSquared(theCandidate);
                }
            }
        } else {
            theTown = this->mTowns[i];
            outTile = inTile;
            break;
        }
    }
    return theTown;
}

// FUNCTION: EXE 0x564e0c
// Returns whether the player still plays: not an observer, or an observer whose game has ended
bool EEPlayer::IsActive()
{
    return !this->mObserverMode || this->mGameResult != 0;
}

// FUNCTION: EXE 0x564e22
// Returns the town whose area contains a unit's tile, or NULL
EETown* EEPlayer::GetTownOf(EEUnit* inUnit)
{
    U2DPoint<long> theTile(inUnit->mTileX, inUnit->mTileY);
    EETown* theTown = NULL;
    for (long i = this->GetNumTowns() - 1; i >= 0; i--) {
        EETown* theCandidate = this->mTowns[i];
        if (theCandidate != NULL && theCandidate->mHull.BoundedContains(theTile)) {
            theTown = this->mTowns[i];
            break;
        }
    }
    return theTown;
}

// FUNCTION: EXE 0x564e78
// Appends the player's herds of a unit type's animal to a list
void EEPlayer::GetHerdsOfType(std::vector<EEHerd*>& outHerds, long inUnitType)
{
    EEAnimal* theAnimal = EEAnimal::Find(inUnitType);
    if (theAnimal == NULL)
        return;
    for (long i = this->mHerds.size() - 1; i >= 0; i--) {
        EEHerd*& theHerd = this->mHerds[i];
        if (theHerd->mAnimalID == theAnimal->mAnimalID)
            outHerds.push_back(theHerd);
    }
}

// FUNCTION: EXE 0x564ed7
// Returns a unit type's level in an improvement counting the queued ones; -1 for an unknown class
long EEPlayer::GetImprovementLevel(unsigned long inUnitType, long inImprovement) const
{
    long theLevel = -1;
    if (this->mUnitClassUpgrades[inUnitType] != NULL) {
        EEUnitClassUpgrade* theUpgrade = this->mUnitClassUpgrades[inUnitType];
        theLevel = theUpgrade->mImprovement.GetLevel(inImprovement);
        std::map<unsigned long, std::pair<float, std::vector<unsigned long> > >::const_iterator theIterator = this->mQueuedImprovements.find(inUnitType);
        if (theIterator != this->mQueuedImprovements.end()) {
            const std::vector<unsigned long>& theQueue = theIterator->second.second;
            for (unsigned long i = 0; i < theQueue.size(); i++)
                if (theQueue[i] == inImprovement)
                    theLevel++;
        }
    }
    return theLevel;
}

// FUNCTION: EXE 0x564f4a
// Counts a unit type's queued improvements of a kind, skipping the first queue entry
long EEPlayer::CountQueuedImprovements(unsigned long inUnitType, long inImprovement) const
{
    long theCount = 0;
    if (this->mUnitClassUpgrades[inUnitType] != NULL) {
        std::map<unsigned long, std::pair<float, std::vector<unsigned long> > >::const_iterator theIterator = this->mQueuedImprovements.find(inUnitType);
        if (theIterator != this->mQueuedImprovements.end()) {
            const std::vector<unsigned long>& theQueue = (*theIterator).second.second;
            for (unsigned long i = 1; i < theQueue.size(); i++)
                if (theQueue[i] == inImprovement)
                    theCount++;
        }
    }
    return theCount;
}

// FUNCTION: EXE 0x564fac
// Returns whether a unit type's improvement has reached its limits, counting the queued improvements
// NEAR: register choice; mUnitClassUpgrades[] through the out-of-line _Ptrit::operator+ 0x80ad in the original
bool EEPlayer::IsImprovementMaxed(unsigned long inUnitType, long inImprovement)
{
    bool theResult = true;
    EEUnitClass* theClass = gUnitClassTable[inUnitType];
    long theTechnology = theClass != NULL ? theClass->mTechnology : 100000;
    if (theClass != NULL && theTechnology != kTechNone) {
        EEUnitClassImprovementTable* theTable = theClass->mImprovements;
        EEUnitClassUpgrade* theUpgrade = this->GetUpgrade(inUnitType);
        long theQueued = theUpgrade->mImprovement.mTotalCost;
        std::map<unsigned long, std::pair<float, std::vector<unsigned long> > >::iterator theIterator = this->mQueuedImprovements.find(inUnitType);
        if (theIterator != this->mQueuedImprovements.end()) {
            const std::vector<unsigned long>& theQueue = (*theIterator).second.second;
            for (unsigned long i = 0; i < theQueue.size(); i++)
                theQueued += theUpgrade->mImprovement.GetCost(theQueue[i]);
        }
        if (theUpgrade->mImprovement.GetCost(inImprovement) + theQueued <= gWorldTable.GetLongValue(47)
            && this->GetImprovementLevel(inUnitType, inImprovement) <= theTable->GetMaxLevel())
            theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x5650a3
// Returns the terrain kinds the player's citizens can cross, as passability flags 1/2/4/8
long EEPlayer::GetCitizenPassability()
{
    EEUnitStats* theStats = this->mUnitClassUpgrades[kUnitTypeCitizen];
    long theResult = 0;
    if (theStats != NULL) {
        long theFlags = theStats->mTerrainMask;
        if (theFlags & 1)
            theResult = 4;
        if (theFlags & 0x200)
            theResult |= 1;
        if (theFlags & 2)
            theResult |= 2;
        if (theFlags & 0x1000)
            theResult |= 8;
    }
    return theResult;
}

// FUNCTION: EXE 0x5650dc
// Registers every herd of the player as an area with the continent manager under a new serial
void EEPlayer::RegisterHerds()
{
    for (long i = this->mHerds.size() - 1; i >= 0; i--) {
        if (this->mHerds[i] == NULL)
            continue;
        ++gContinentManager.mLastSerial;
        this->mHerds[i]->mSerial = gContinentManager.mLastSerial;
        gContinentManager.AddArea(this->mHerds[i]);
    }
}

// FUNCTION: EXE 0x56513d
// Registers every town of the player as an area with the continent manager under a new serial
void EEPlayer::RegisterTowns()
{
    for (long i = this->mTowns.size() - 1; i >= 0; i--) {
        if (this->mTowns[i] == NULL)
            continue;
        ++gContinentManager.mLastSerial;
        this->mTowns[i]->mSerial = gContinentManager.mLastSerial;
        gContinentManager.AddArea(this->mTowns[i]);
    }
}

// FUNCTION: EXE 0x56519e
// Returns the player's patrol group with an ID, or NULL
EEPPatrolGroup* EEPlayer::GetPatrolGroup(unsigned long inID)
{
    EEPPatrolGroup* theResult = NULL;
    for (unsigned long i = 0; i < this->mPatrolGroups.size(); i++) {
        if (this->mPatrolGroups[i]->mID == inID) {
            theResult = this->mPatrolGroups[i];
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5651e0
// Returns the population, optionally adding the pending units and the units inside category-0x1d containers
long EEPlayer::GetTotalPopulation(bool inFirst, bool inSecond)
{
    long theResult = this->mPopulationCount;
    if (inFirst) {
        for (unsigned long i = 0; i < this->mPendingUnits.size(); i++)
            theResult += dynamic_cast<EEUnit*>(this->mPendingUnits[i])->GetUnitClass()->GetPopulationCost();
    }
    if (!inSecond)
        return theResult;
    for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->GetComplexUnit() != NULL && theUnit->GetComplexUnit()->GetContainer() != NULL
            && theUnit->GetComplexUnit()->GetContainer()->GetUnitClass()->GetCategory() == kUnitCategoryFortress)
            theResult += theUnit->GetUnitClass()->GetPopulationCost();
    }
    return theResult;
}

// FUNCTION: EXE 0x56528b
// Returns the player's population limit: the game's limit plus the player's bonuses
long EEPlayer::GetPopulationLimit(long inArg)
{
    return this->mPopulationLimitBonus + this->mShareBonus + gWorld.mPlayerPopulationLimit;
}

// FUNCTION: EXE 0x5652a0
// Returns whether one resource alone (the node's cost resource or any of the five) covers
// the cost, and the required statistic is reached
// NEAR: the original keeps the result in the arg-1 slot byte (+0xb) and loads mResourceCaps[k] before the call
bool EEPlayer::CanAffordSingleResource(EETechTreeNode* inNode, long inUnitClass, long inCount)
{
    bool theResult = false;
    long theResource = inNode->mData->mCostResource;
    if (theResource != 0 && inNode->mData->mRequiredStatisticValue != 0) {
        if (theResource != 1 && this->mResourceCaps[theResource] >= this->ScaleCost(inNode->GetCost(theResource, inUnitClass, 1.0f)) * inCount)
            theResult = true;
        else {
            for (unsigned long i = 2; i < 7 && !theResult; i++) {
                long theAmount = this->mResourceCaps[i];
                theResult = theAmount >= this->ScaleCost(inNode->GetCost(i, inUnitClass, 1.0f)) * inCount;
                if (theResult || (i != 6 && i != 5) || !gCivPowersManager.HasPower(this, kCivPowerMetallurgy))
                    continue;
                long theCost = this->ScaleCost((long)(inNode->GetCost(kResourceIron, inUnitClass, 1.0f) + 0.0001f)) * inCount;
                theCost += this->ScaleCost((long)(inNode->GetCost(kResourceGold, inUnitClass, 1.0f) + 0.0001f)) * inCount;
                if (this->mResourceCaps[kResourceIron] + this->mResourceCaps[kResourceGold] >= theCost)
                    theResult = true;
            }
        }
        if (theResult == true) {
            if (this->mSavedGroups.GetLastValue(inNode->mData->mRequiredStatistic) < inNode->mData->mRequiredStatisticValue)
                theResult = false;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x56542e
// Returns whether the player has every resource for a scaled cost (civ power 0x2e pools resources 5 and 6)
bool EEPlayer::CanAffordScaled(EETechTreeNode* inNode, long inUnitClass, float inScale, long inCount)
{
    bool theResult = true;
    for (unsigned long i = 2; i < 7 && theResult; i++) {
        long theAmount = this->mResourceCaps[i];
        theResult = theAmount >= this->ScaleCost((long)(inNode->GetCost(i, inUnitClass, 1.0f) * inScale + 0.0001f)) * inCount;
        if (theResult || (i != 6 && i != 5) || !gCivPowersManager.HasPower(this, kCivPowerMetallurgy))
            continue;
        long theCost = this->ScaleCost((long)(inNode->GetCost(kResourceIron, inUnitClass, 1.0f) * inScale + 0.0001f)) * inCount;
        theCost += this->ScaleCost((long)(inNode->GetCost(kResourceGold, inUnitClass, 1.0f) * inScale + 0.0001f)) * inCount;
        if (this->mResourceCaps[kResourceIron] + this->mResourceCaps[kResourceGold] >= theCost)
            theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x565561
// Returns the index of the team holding the player (0 when none)
unsigned long EEPlayer::GetTeamIndex()
{
    unsigned long thePlayerID = this->mPlayerID;
    unsigned long theResult = 0;
    for (unsigned long i = 0; i < gWorld.GetNumTeams(); i++) {
        EETeam& theTeam = gWorld.mTeams[i];
        if (theTeam.ContainsPlayer(thePlayerID)) {
            theResult = i;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5655b5
// Returns the player's town with an ID, or NULL
EETown* EEPlayer::GetTownByID(long inID)
{
    EETown* theTown = NULL;
    for (unsigned long i = 0; i < this->mTowns.size() && theTown == NULL; i++) {
        if (this->mTowns[i]->mTownID == inID)
            theTown = this->mTowns[i];
    }
    return theTown;
}

// FUNCTION: EXE 0x5655f6
// Returns the off-map record of a unit, or NULL
EEWUnitsOffMapInfo* EEPlayer::GetOffMapInfo(EEComplexUnit* inUnit)
{
    EEWUnitsOffMapInfo* theResult = NULL;
    if (inUnit != NULL) {
        for (long i = this->mUnitsOffMap.size() - 1; i >= 0; i--) {
            if (inUnit == this->mUnitsOffMap[i]->mUnit) {
                theResult = this->mUnitsOffMap[i];
                break;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x565644
// Returns the timer start time of a wonder by index
long EEPlayer::GetWonderTime(long inIndex) const
{
    const std::vector<EEComplexUnit*>& theItems = this->mWonders->mItems;
    return theItems[inIndex]->mCloaked;
}

// FUNCTION: EXE 0x56565f
// Returns the number of wonders the player holds
unsigned long EEPlayer::GetNumWonderUnits() const
{
    return this->mWonders->GetSize();
}

// FUNCTION: EXE 0x56566a
// Returns whether the player can afford a technology in all resources or, failing that, from a single one
bool EEPlayer::CanAfford(EETechnology* inTechnology, long inUnitType, long inCount)
{
    bool theResult = this->CanAffordScaled(inTechnology, inUnitType, 1.0f, inCount);
    if (!theResult)
        theResult = this->CanAffordSingleResource(inTechnology, inCount, 1);
    return theResult;
}

// FUNCTION: EXE 0x56569c
// Adds damage taken from a player to its tally and enters combat when that player is an enemy
void EEPlayer::DamageTaken(long inPlayer, long inAmount, EEUnit* inUnit)
{
    this->mDamageFrom[inPlayer] += inAmount;
    if (inPlayer == 0)
        return;
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    if (thePlayer != this && this->mEnemies[thePlayer->mPlayerID] != 0)
        this->mCombatState = 2;
}

#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "lle/SESoundManager.h"

// FUNCTION: EXE 0x5656d7
// Under civ power 0x27, periodically makes a free unit (class 0x20 or 0x1e0) at a town center or capitol,
// playing a sound for the local player and lengthening the interval each time
void EEPlayer::UpdateFreeUnitPower()
{
    bool theCreated = false;
    if (!gCivPowersManager.HasPower(this, kCivPowerSlavery) || !(++this->mFreeUnitCounter >= this->mFreeUnitInterval))
        return;
    EEUnit* theUnit = this->mFirstUnit;
    while (theUnit != NULL && !theCreated) {
        if (theUnit->GetUnitClass()->mUnitType == kUnitTypeCapitol) {
            EEUnitClass* theClass = gPhysics->mRandom.Generate() >= 0.5f ? gUnitClassTable[kUnitTypeCitizen] : gUnitClassTable[kUnitTypeFemaleCitizen];
            theCreated = theUnit->GetUnitClass()->ProduceUnit(theUnit->GetComplexUnit(), theClass, false);
        }
        theUnit = static_cast<EEUnit*>(theUnit->mNext);
    }
    theUnit = this->mFirstUnit;
    while (theUnit != NULL && !theCreated) {
        if (theUnit->GetUnitClass()->mUnitType == kUnitTypeTownCenter) {
            EEUnitClass* theClass = gPhysics->mRandom.Generate() >= 0.5f ? gUnitClassTable[kUnitTypeCitizen] : gUnitClassTable[kUnitTypeFemaleCitizen];
            theCreated = theUnit->GetUnitClass()->ProduceUnit(theUnit->GetComplexUnit(), theClass, false);
        }
        theUnit = static_cast<EEUnit*>(theUnit->mNext);
    }
    if (theCreated != true)
        return;
    this->mFreeUnitCounter = 0;
    if (::gWorld.GetCurrentPlayer() == this) {
        EESoundObject* theSound = EESoundObject::Get(kSoundSlavery);
        gSoundManager->Play(theSound->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
    }
    this->mFreeUnitInterval++;
}

// FUNCTION: EXE 0x565808
// Advances the player to the next epoch
void EEPlayer::IncrementEpic()
{
    this->mTechnologies->AdvanceEpoch();
}

// FUNCTION: EXE 0x565813
// Resets the player to its starting state: new selection and control groups, upgrades, tech tree and
// air traffic, zeroed counters and default settings, then loads the player effects
void EEPlayer::Reset()
{
    this->mActiveGroup = new EESelectedGroup(this);
    this->mSelectedGroup = new EESelectedGroup(this);
    this->mL9B4 = new EESelectedGroup(this);
    for (unsigned long i = 0; i < 20; i++)
        this->mGroups[i] = new EESavedGroup(this, i + 1);
    unsigned long theCount = this->mRunningGroups.size();
    for (unsigned long j = 0; j < theCount; j++) {
        this->mRunningGroups[j]->Clear();
        this->mRunningGroups[j]->Dereference();
    }
    this->mRunningGroups.clear();
    this->mActiveGroup->Clear();
    for (unsigned long k = 0; k < 20; k++)
        this->mGroups[k]->Clear();
    if (this->mViewedUnit != NULL) {
        this->mViewedUnit->Dereference();
        this->mViewedUnit = NULL;
    }
    for (long theResource = 2; theResource < 7; theResource++)
        this->mResourceCaps[theResource] = 0;
    this->mSavedGroups.Clear();
    this->mSavedGroups.SetName(this->GetName());
    for (unsigned long thePlayer = 0; thePlayer <= 16; thePlayer++)
        this->mEnemies[thePlayer] = this->mPlayerID != thePlayer;
    this->mUnitClassUpgrades.resize(0x367);
    for (long theClass = 0; theClass < 0x367; theClass++) {
        if (gUnitClassTable[theClass] != NULL)
            this->mUnitClassUpgrades[theClass] = new EEUnitClassUpgrade(gUnitClassTable[theClass]);
        else
            this->mUnitClassUpgrades[theClass] = NULL;
    }
    this->mTechnologies = new EETechTree;
    *this->mTechnologies = gTechTree;
    for (long theAirport = 0; theAirport < 4; theAirport++)
        mAirTraffic[theAirport] = new EEAirTrafficControl(this);
    this->mConversionFactor = 1.0f;
    this->mHighGroundBonus = 0.0f;
    this->mPopulationCount = 0;
    this->mEndOfGameCount = 0;
    this->mNumHeroes = 0;
    this->mHeroLimit = 1;
    this->mNumHeroesInProduction = 0;
    this->mPopLimit = 0;
    this->mNumCapitols = 0;
    this->mSharedLOS = true;
    this->mChatType = 1;
    this->mTributeFee = kTributeFee;
    this->mShareBonus = 0;
    this->mPopulationLimitBonus = 0;
    this->mBB44 = false;
    this->mInstantBuild = false;
    this->mJITManufacturing = false;
    this->mLAF0 = 0;
    this->mHealsUnits = false;
    this->mHealedUnitSet = NULL;
    this->mGameResult = 0;
    this->mMaxStartingCitizens = gWorldTable.GetLongValue(48);
    this->mMiningLimit = gWorldTable.GetLongValue(22);
    this->mHealTimer = 0.0f;
    this->mAreaConversion = false;
    this->mBehindFirewall = false;
    this->mDisconnected = false;
    this->mBB42 = false;
    this->mCivilizationLocked = false;
    this->mCivilizationChosen = false;
    this->mCivUpgradesPurchased = false;
    this->mShiftMode = false;
    this->mLastAcknowledgementTime = 0;
    this->mLastAcknowledgedGroup = 0;
    this->mEarthquakeShaken = false;
    this->mFreeUnitCounter = 0;
    this->mFreeUnitInterval = 1;
    this->mNumTeleporters = 0;
    for (long theStat = 0; theStat < 10; theStat++)
        this->mTributes[theStat] = 0;
    for (unsigned long theOther = 0; theOther <= 16; theOther++) {
        this->mDamageFrom[theOther] = 0;
        this->mSendChatMessage[theOther] = true;
    }
    this->mNumAttacks = 0;
    this->mCombatState = 0;
    this->mHackUnitClass = 0x367;
    for (long theValue = 0; theValue < 10; theValue++)
        this->mScenarioVariables[theValue] = 0;
    this->mEvents.Clear(this);
    memset(this->mResourceTrade, 0, sizeof(this->mResourceTrade));
    memset(this->mInsuranceRefunds, 0, sizeof(this->mInsuranceRefunds));
    this->mNumVitalBuildings = 0;
    this->LoadPlayerEffects();
}

// FUNCTION: EXE 0x565bfe
// Returns the group's unit at an index
EEComplexUnit* EEGroup::GetUnit(long inIndex)
{
    const std::vector<EEComplexUnit*>& theUnits = this->mUnits;
    return theUnits[inIndex];
}

// FUNCTION: EXE 0x565c0d
// Returns the number of units in the group
unsigned long EEGroup::GetNumUnits()
{
    return this->mUnits.size();
}

// SYNTHETIC: EXE 0x565c20
// (the scalar deleting destructor, emitted with the vtable; slot EESelectedGroup+0x0)

// FUNCTION: EXE 0x565c3c
// Kills every unit of a herd
void EEPlayer::KillHerdUnits(EEHerd* inHerd, bool inKilled)
{
    std::vector<EEComplexUnit*> theUnits = inHerd->mUnits;
    while (theUnits.size() != 0) {
        theUnits.back()->Kill(inKilled, 0, 0);
        theUnits.pop_back();
    }
}

// FUNCTION: EXE 0x565c8c
// Kills every stranded unit, removes it from the player and empties the stranded list
void EEPlayer::KillStrandedUnits()
{
    for (long i = this->mStrandedUnits.size() - 1; i >= 0; i--) {
        if (this->mStrandedUnits[i] == NULL)
            continue;
        this->mStrandedUnits[i]->Kill(0, 0, 0);
        this->KillUnit(this->mStrandedUnits[i], false);
        this->mStrandedUnits[i]->Dereference();
    }
    this->mStrandedUnits.clear();
}

// FUNCTION: EXE 0x565cfa
// Loads the player's state from a saved game (the mirror of Save); returns the read status, some reads' errors ignored
// One reader per saved-game era (minor version < 0xd7, < 0xe1, < 0xe8, later), each with its own assertion
// sites, as in the original
long EEPlayer::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    std::vector<long> theUnused;
    long theRows = 0;
    long theColumns = 0;
    unsigned long theCount = 0;
    unsigned long theGoalCounter = 0;
    unsigned long theFormatVersion;
    unsigned long theID;
    long theResult = kErrorOK;
    unsigned long i;
    if (inMajorVersion == 0 && inMinorVersion < 0xd7) {
        for (i = 0; i < 10 && theResult == kErrorOK; i++)
            theResult = inFile.Read(this->mScenarioVariables[i]);
        inFile.Read(theCount);
        long theUpgradeResult = inFile.Read(theFormatVersion);
        EEUnitClassUpgrade* theDummyUpgrade = NULL;
        for (i = 0; i < 0x367; i++) {
            if (gUnitClassTable[i] != NULL) {
                theDummyUpgrade = new EEUnitClassUpgrade(gUnitClassTable[i]);
                break;
            }
        }

        ESVERIFY_AT(theDummyUpgrade != NULL, 4282);
        for (i = 0; theUpgradeResult == kErrorOK && i < theCount; i++) {
            theUpgradeResult = inFile.Read(theID);
            if (theUpgradeResult != kErrorOK)
                continue;
            if (theID < 0x367 && this->mUnitClassUpgrades[theID] != NULL)
                this->mUnitClassUpgrades[theID]->Load(inMajorVersion, inMinorVersion, inFile);
            else
                theUpgradeResult = theDummyUpgrade->Load(inMajorVersion, inMinorVersion, inFile);
        }
        delete theDummyUpgrade;
        RTSPlayer::Load(inMajorVersion, inMinorVersion, inFile);
        theResult = inFile.Read(theGoalCounter);
        if (theResult == kErrorOK)
            sGoalCounter = theGoalCounter;
        delete this->mTechnologies;
        this->mTechnologies = NULL;
        bool theHaveTechTree;
        theResult = inFile.Read(theHaveTechTree);
        if (theResult == kErrorOK) {
            theResult = kErrorOK;
            if (theHaveTechTree) {
                this->mTechnologies = new EETechTree;
                *this->mTechnologies = gTechTree;
                this->mTechnologies->SetPlayerIndex(this->mPlayerID);
                this->mTechnologies->SetPlayer(this);
                theResult = this->mTechnologies->Load(inMajorVersion, inMinorVersion, inFile);
            }
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mPopLimit);
            if (theResult == kErrorOK)
                theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                for (i = 0; i < theCount && theResult == kErrorOK; i++)
                    theResult = inFile.Read(this->mResourceCaps[i]);
                if (theResult == kErrorOK) {
                    long thePatrolResult = inFile.Read(theCount);
                    if (thePatrolResult == kErrorOK && theCount > 0) {
                        this->mPatrolGroups.clear();
                        for (i = 0; i < theCount && thePatrolResult == kErrorOK; i++) {
                            EEPPatrolGroup* theGroup = new EEPPatrolGroup;
                            thePatrolResult = theGroup->Load(inMajorVersion, inMinorVersion, &inFile);
                            if (thePatrolResult == kErrorOK)
                                this->AddPatrolGroup(theGroup);
                            theGroup->Dereference();
                        }
                    }
                }
            }
        }
        // These calls intentionally discard their results in the original, even after a prior record failed.
        this->mSavedGroups.Load(inMajorVersion, inMinorVersion, inFile);
        inFile.Read(this->mPopulationCount);
        theResult = inFile.Read(this->mEndOfGameCount);
        for (i = 0; i < 17 && theResult == kErrorOK; i++) {
            theResult = inFile.Read(this->mEnemies[i]);
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mSendChatMessage[i]);
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount && theResult == kErrorOK; i++)
                theResult = mAirTraffic[i]->Load(inMajorVersion, inMinorVersion, &inFile);
            if (theResult == kErrorOK)
                theResult = this->mActiveGroup->Load(inMajorVersion, inMinorVersion, inFile);
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mCombatState);
            if (theResult == kErrorOK)
                theResult = inFile.Read(theFormatVersion); // Original reads/discards the saved mNumCapitols word.
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mSharedLOS);
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mChatType);
            if (theResult == kErrorOK)
                theResult = this->mCivilization.Load(inMajorVersion, inMinorVersion, inFile);
        }
        for (i = 0; i < 20; i++)
            if (theResult == kErrorOK)
                theResult = this->mGroups[i]->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mCivilizationFile.Load(inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount; i++) {
                theResult = inFile.Read(theID);
                if (theResult != kErrorOK)
                    continue;
                theResult = inFile.Read(this->mQueuedImprovements[theID].first);
                if (theResult == kErrorOK)
                    theResult = FSLoadVector(inFile, this->mQueuedImprovements[theID].second, UString("Queued Improvements"));
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount && theResult == kErrorOK; i++) {
                long theUnitID;
                theResult = inFile.Read(theUnitID);
                if (theResult == kErrorOK)
                    this->mConstructionSiteIDs.push_back(theUnitID);
            }
        }
        if (theResult == kErrorOK) {
            theCount = 0;
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                this->mHerds.resize(theCount, NULL);
                for (i = 0; i < theCount && theResult == kErrorOK; i++) {
                    this->mHerds[i] = new EEHerd;
                    theResult = this->mHerds[i]->Load(inMajorVersion, inMinorVersion, inFile);
                }
            }
        }
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theRows);
            if (theResult == kErrorOK) {
                theResult = inFile.Read(theColumns);
                for (unsigned long r = 0; r < theRows; r++)
                    for (unsigned long c = 0; c < theColumns && theResult == kErrorOK; c++)
                        theResult = inFile.Read(this->mTributes[r * 5 + c]);
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGameResult);
        if (theResult == kErrorOK)
            theResult = this->mEvents.Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHighGroundBonus);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHealTimer);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDisconnected);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCivilizationLocked);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCivilizationChosen);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCivUpgradesPurchased);
        return theResult;
    }
    else if (inMajorVersion == 0 && inMinorVersion < 0xe1) {
        for (i = 0; i < 10 && theResult == kErrorOK; i++)
            theResult = inFile.Read(this->mScenarioVariables[i]);
        inFile.Read(theCount);
        long theUpgradeResult = inFile.Read(theFormatVersion);
        EEUnitClassUpgrade* theDummyUpgrade = NULL;
        for (i = 0; i < 0x367; i++) {
            if (gUnitClassTable[i] != NULL) {
                theDummyUpgrade = new EEUnitClassUpgrade(gUnitClassTable[i]);
                break;
            }
        }

        ESVERIFY_AT(theDummyUpgrade != NULL, 4610);
        for (i = 0; theUpgradeResult == kErrorOK && i < theCount; i++) {
            theUpgradeResult = inFile.Read(theID);
            if (theUpgradeResult != kErrorOK)
                continue;
            if (theID < 0x367 && this->mUnitClassUpgrades[theID] != NULL)
                this->mUnitClassUpgrades[theID]->Load(inMajorVersion, inMinorVersion, inFile);
            else
                theUpgradeResult = theDummyUpgrade->Load(inMajorVersion, inMinorVersion, inFile);
        }
        delete theDummyUpgrade;
        RTSPlayer::Load(inMajorVersion, inMinorVersion, inFile);
        theResult = inFile.Read(theGoalCounter);
        if (theResult == kErrorOK)
            sGoalCounter = theGoalCounter;
        delete this->mTechnologies;
        this->mTechnologies = NULL;
        bool theHaveTechTree;
        theResult = inFile.Read(theHaveTechTree);
        if (theResult == kErrorOK) {
            theResult = kErrorOK;
            if (theHaveTechTree) {
                this->mTechnologies = new EETechTree;
                *this->mTechnologies = gTechTree;
                this->mTechnologies->SetPlayerIndex(this->mPlayerID);
                this->mTechnologies->SetPlayer(this);
                theResult = this->mTechnologies->Load(inMajorVersion, inMinorVersion, inFile);
            }
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mPopLimit);
            if (theResult == kErrorOK)
                theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                for (i = 0; i < theCount && theResult == kErrorOK; i++)
                    theResult = inFile.Read(this->mResourceCaps[i]);
                if (theResult == kErrorOK) {
                    long thePatrolResult = inFile.Read(theCount);
                    if (thePatrolResult == kErrorOK && theCount > 0) {
                        this->mPatrolGroups.clear();
                        for (i = 0; i < theCount && thePatrolResult == kErrorOK; i++) {
                            EEPPatrolGroup* theGroup = new EEPPatrolGroup;
                            thePatrolResult = theGroup->Load(inMajorVersion, inMinorVersion, &inFile);
                            if (thePatrolResult == kErrorOK)
                                this->AddPatrolGroup(theGroup);
                            theGroup->Dereference();
                        }
                    }
                }
            }
        }
        // These calls intentionally discard their results in the original, even after a prior record failed.
        this->mSavedGroups.Load(inMajorVersion, inMinorVersion, inFile);
        inFile.Read(this->mPopulationCount);
        theResult = inFile.Read(this->mEndOfGameCount);
        for (i = 0; i < 17 && theResult == kErrorOK; i++) {
            theResult = inFile.Read(this->mEnemies[i]);
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mSendChatMessage[i]);
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount && theResult == kErrorOK; i++)
                theResult = mAirTraffic[i]->Load(inMajorVersion, inMinorVersion, &inFile);
            if (theResult == kErrorOK)
                theResult = this->mActiveGroup->Load(inMajorVersion, inMinorVersion, inFile);
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mCombatState);
            if (theResult == kErrorOK)
                theResult = inFile.Read(theFormatVersion); // Original reads/discards the saved mNumCapitols word.
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mSharedLOS);
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mChatType);
            if (theResult == kErrorOK)
                theResult = this->mCivilization.Load(inMajorVersion, inMinorVersion, inFile);
        }
        for (i = 0; i < 20; i++)
            if (theResult == kErrorOK)
                theResult = this->mGroups[i]->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mCivilizationFile.Load(inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount; i++) {
                theResult = inFile.Read(theID);
                if (theResult != kErrorOK)
                    continue;
                theResult = inFile.Read(this->mQueuedImprovements[theID].first);
                if (theResult == kErrorOK)
                    theResult = FSLoadVector(inFile, this->mQueuedImprovements[theID].second, UString("Queued Improvements"));
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount && theResult == kErrorOK; i++) {
                long theUnitID;
                theResult = inFile.Read(theUnitID);
                if (theResult == kErrorOK)
                    this->mConstructionSiteIDs.push_back(theUnitID);
            }
        }
        if (theResult == kErrorOK) {
            theCount = 0;
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                this->mHerds.resize(theCount, NULL);
                for (i = 0; i < theCount && theResult == kErrorOK; i++) {
                    this->mHerds[i] = new EEHerd;
                    theResult = this->mHerds[i]->Load(inMajorVersion, inMinorVersion, inFile);
                }
            }
        }
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theRows);
            if (theResult == kErrorOK) {
                theResult = inFile.Read(theColumns);
                for (unsigned long r = 0; r < theRows; r++)
                    for (unsigned long c = 0; c < theColumns && theResult == kErrorOK; c++)
                        theResult = inFile.Read(this->mTributes[r * 5 + c]);
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGameResult);
        if (theResult == kErrorOK)
            theResult = this->mEvents.Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHighGroundBonus);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHealTimer);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDisconnected);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCivilizationLocked);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCivilizationChosen);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCivUpgradesPurchased);
        if (theResult == kErrorOK) {
            theCount = 0;
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                this->mUnitsOffMap.resize(theCount, NULL);
                for (i = 0; i < theCount && theResult == kErrorOK; i++) {
                    this->mUnitsOffMap[i] = new EEWUnitsOffMapInfo;
                    theResult = this->mUnitsOffMap[i]->Load(inMajorVersion, inMinorVersion, inFile);
                }
            }
        }
        return theResult;
    }
    else if (inMajorVersion == 0 && inMinorVersion < 0xe8) {
        for (i = 0; i < 10 && theResult == kErrorOK; i++)
            theResult = inFile.Read(this->mScenarioVariables[i]);
        inFile.Read(theCount);
        long theUpgradeResult = inFile.Read(theFormatVersion);
        EEUnitClassUpgrade* theDummyUpgrade = NULL;
        for (i = 0; i < 0x367; i++) {
            if (gUnitClassTable[i] != NULL) {
                theDummyUpgrade = new EEUnitClassUpgrade(gUnitClassTable[i]);
                break;
            }
        }

        ESVERIFY_AT(theDummyUpgrade != NULL, 4960);
        for (i = 0; theUpgradeResult == kErrorOK && i < theCount; i++) {
            theUpgradeResult = inFile.Read(theID);
            if (theUpgradeResult != kErrorOK)
                continue;
            if (theID < 0x367 && this->mUnitClassUpgrades[theID] != NULL)
                this->mUnitClassUpgrades[theID]->Load(inMajorVersion, inMinorVersion, inFile);
            else
                theUpgradeResult = theDummyUpgrade->Load(inMajorVersion, inMinorVersion, inFile);
        }
        delete theDummyUpgrade;
        RTSPlayer::Load(inMajorVersion, inMinorVersion, inFile);
        theResult = inFile.Read(theGoalCounter);
        if (theResult == kErrorOK)
            sGoalCounter = theGoalCounter;
        delete this->mTechnologies;
        this->mTechnologies = NULL;
        bool theHaveTechTree;
        theResult = inFile.Read(theHaveTechTree);
        if (theResult == kErrorOK) {
            theResult = kErrorOK;
            if (theHaveTechTree) {
                this->mTechnologies = new EETechTree;
                *this->mTechnologies = gTechTree;
                this->mTechnologies->SetPlayerIndex(this->mPlayerID);
                this->mTechnologies->SetPlayer(this);
                theResult = this->mTechnologies->Load(inMajorVersion, inMinorVersion, inFile);
            }
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mPopLimit);
            if (theResult == kErrorOK)
                theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                for (i = 0; i < theCount && theResult == kErrorOK; i++)
                    theResult = inFile.Read(this->mResourceCaps[i]);
                if (theResult == kErrorOK) {
                    long thePatrolResult = inFile.Read(theCount);
                    if (thePatrolResult == kErrorOK && theCount > 0) {
                        this->mPatrolGroups.clear();
                        for (i = 0; i < theCount && thePatrolResult == kErrorOK; i++) {
                            EEPPatrolGroup* theGroup = new EEPPatrolGroup;
                            thePatrolResult = theGroup->Load(inMajorVersion, inMinorVersion, &inFile);
                            if (thePatrolResult == kErrorOK)
                                this->AddPatrolGroup(theGroup);
                            theGroup->Dereference();
                        }
                    }
                }
            }
        }
        // These calls intentionally discard their results in the original, even after a prior record failed.
        this->mSavedGroups.Load(inMajorVersion, inMinorVersion, inFile);
        inFile.Read(this->mPopulationCount);
        theResult = inFile.Read(this->mEndOfGameCount);
        for (i = 0; i < 17 && theResult == kErrorOK; i++) {
            theResult = inFile.Read(this->mEnemies[i]);
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mSendChatMessage[i]);
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount && theResult == kErrorOK; i++)
                theResult = mAirTraffic[i]->Load(inMajorVersion, inMinorVersion, &inFile);
            if (theResult == kErrorOK)
                theResult = this->mActiveGroup->Load(inMajorVersion, inMinorVersion, inFile);
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mCombatState);
            if (theResult == kErrorOK)
                theResult = inFile.Read(theFormatVersion); // Original reads/discards the saved mNumCapitols word.
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mSharedLOS);
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mChatType);
            if (theResult == kErrorOK)
                theResult = this->mCivilization.Load(inMajorVersion, inMinorVersion, inFile);
        }
        for (i = 0; i < 20; i++)
            if (theResult == kErrorOK)
                theResult = this->mGroups[i]->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mCivilizationFile.Load(inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount; i++) {
                theResult = inFile.Read(theID);
                if (theResult != kErrorOK)
                    continue;
                theResult = inFile.Read(this->mQueuedImprovements[theID].first);
                if (theResult == kErrorOK)
                    theResult = FSLoadVector(inFile, this->mQueuedImprovements[theID].second, UString("Queued Improvements"));
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount && theResult == kErrorOK; i++) {
                long theUnitID;
                theResult = inFile.Read(theUnitID);
                if (theResult == kErrorOK)
                    this->mConstructionSiteIDs.push_back(theUnitID);
            }
        }
        if (theResult == kErrorOK) {
            theCount = 0;
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                this->mHerds.resize(theCount, NULL);
                for (i = 0; i < theCount && theResult == kErrorOK; i++) {
                    this->mHerds[i] = new EEHerd;
                    theResult = this->mHerds[i]->Load(inMajorVersion, inMinorVersion, inFile);
                }
            }
        }
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theRows);
            if (theResult == kErrorOK) {
                theResult = inFile.Read(theColumns);
                for (unsigned long r = 0; r < theRows; r++)
                    for (unsigned long c = 0; c < theColumns && theResult == kErrorOK; c++)
                        theResult = inFile.Read(this->mTributes[r * 5 + c]);
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGameResult);
        if (theResult == kErrorOK)
            theResult = this->mEvents.Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHighGroundBonus);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHealTimer);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDisconnected);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCivilizationLocked);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCivilizationChosen);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCivUpgradesPurchased);
        if (theResult == kErrorOK) {
            theCount = 0;
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                this->mUnitsOffMap.resize(theCount, NULL);
                for (i = 0; i < theCount && theResult == kErrorOK; i++) {
                    this->mUnitsOffMap[i] = new EEWUnitsOffMapInfo;
                    theResult = this->mUnitsOffMap[i]->Load(inMajorVersion, inMinorVersion, inFile);
                }
            }
        }
        if (theResult == kErrorOK) {
            theCount = 0;
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                for (i = 0; i < theCount && theResult == kErrorOK; i++) {
                    long theUnitID;
                    theResult = inFile.Read(theUnitID);
                    if (theResult == kErrorOK)
                        this->mWonderIDs.push_back(theUnitID);
                }
            }
        }
        if (inMajorVersion == 0 && inMinorVersion < 0xe2) {
            this->mWonders->Clear();
            this->mWonders->mItems.clear();
        }
        return theResult;
    }
    else {
        for (i = 0; i < 10 && theResult == kErrorOK; i++)
            theResult = inFile.Read(this->mScenarioVariables[i]);
        inFile.Read(theCount);
        long theUpgradeResult = inFile.Read(theFormatVersion);
        EEUnitClassUpgrade* theDummyUpgrade = NULL;
        for (i = 0; i < 0x367; i++) {
            if (gUnitClassTable[i] != NULL) {
                theDummyUpgrade = new EEUnitClassUpgrade(gUnitClassTable[i]);
                break;
            }
        }

        ESVERIFY_AT(theDummyUpgrade != NULL, 5331);
        for (i = 0; theUpgradeResult == kErrorOK && i < theCount; i++) {
            theUpgradeResult = inFile.Read(theID);
            if (theUpgradeResult != kErrorOK)
                continue;
            if (theID < 0x367 && this->mUnitClassUpgrades[theID] != NULL)
                this->mUnitClassUpgrades[theID]->Load(inMajorVersion, inMinorVersion, inFile);
            else
                theUpgradeResult = theDummyUpgrade->Load(inMajorVersion, inMinorVersion, inFile);
        }
        delete theDummyUpgrade;
        RTSPlayer::Load(inMajorVersion, inMinorVersion, inFile);
        theResult = inFile.Read(theGoalCounter);
        if (theResult == kErrorOK)
            sGoalCounter = theGoalCounter;
        delete this->mTechnologies;
        this->mTechnologies = NULL;
        bool theHaveTechTree;
        theResult = inFile.Read(theHaveTechTree);
        if (theResult == kErrorOK) {
            theResult = kErrorOK;
            if (theHaveTechTree) {
                this->mTechnologies = new EETechTree;
                *this->mTechnologies = gTechTree;
                this->mTechnologies->SetPlayerIndex(this->mPlayerID);
                this->mTechnologies->SetPlayer(this);
                theResult = this->mTechnologies->Load(inMajorVersion, inMinorVersion, inFile);
            }
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mPopLimit);
            if (theResult == kErrorOK)
                theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                for (i = 0; i < theCount && theResult == kErrorOK; i++)
                    theResult = inFile.Read(this->mResourceCaps[i]);
                if (theResult == kErrorOK) {
                    long thePatrolResult = inFile.Read(theCount);
                    if (thePatrolResult == kErrorOK && theCount > 0) {
                        this->mPatrolGroups.clear();
                        for (i = 0; i < theCount && thePatrolResult == kErrorOK; i++) {
                            EEPPatrolGroup* theGroup = new EEPPatrolGroup;
                            thePatrolResult = theGroup->Load(inMajorVersion, inMinorVersion, &inFile);
                            if (thePatrolResult == kErrorOK)
                                this->AddPatrolGroup(theGroup);
                            theGroup->Dereference();
                        }
                    }
                }
            }
        }
        // These calls intentionally discard their results in the original, even after a prior record failed.
        this->mSavedGroups.Load(inMajorVersion, inMinorVersion, inFile);
        inFile.Read(this->mPopulationCount);
        theResult = inFile.Read(this->mEndOfGameCount);
        for (i = 0; i < 17 && theResult == kErrorOK; i++) {
            theResult = inFile.Read(this->mEnemies[i]);
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mSendChatMessage[i]);
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount && theResult == kErrorOK; i++)
                theResult = mAirTraffic[i]->Load(inMajorVersion, inMinorVersion, &inFile);
            if (theResult == kErrorOK)
                theResult = this->mActiveGroup->Load(inMajorVersion, inMinorVersion, inFile);
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mCombatState);
            if (theResult == kErrorOK)
                theResult = inFile.Read(theFormatVersion); // Original reads/discards the saved mNumCapitols word.
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mSharedLOS);
            if (theResult == kErrorOK)
                theResult = inFile.Read(this->mChatType);
            if (theResult == kErrorOK)
                theResult = this->mCivilization.Load(inMajorVersion, inMinorVersion, inFile);
        }
        for (i = 0; i < 20; i++)
            if (theResult == kErrorOK)
                theResult = this->mGroups[i]->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mCivilizationFile.Load(inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount; i++) {
                theResult = inFile.Read(theID);
                if (theResult != kErrorOK)
                    continue;
                theResult = inFile.Read(this->mQueuedImprovements[theID].first);
                if (theResult == kErrorOK)
                    theResult = FSLoadVector(inFile, this->mQueuedImprovements[theID].second, UString("Queued Improvements"));
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount && theResult == kErrorOK; i++) {
                long theUnitID;
                theResult = inFile.Read(theUnitID);
                if (theResult == kErrorOK)
                    this->mConstructionSiteIDs.push_back(theUnitID);
            }
        }
        if (theResult == kErrorOK) {
            theCount = 0;
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                this->mHerds.resize(theCount, NULL);
                for (i = 0; i < theCount && theResult == kErrorOK; i++) {
                    this->mHerds[i] = new EEHerd;
                    theResult = this->mHerds[i]->Load(inMajorVersion, inMinorVersion, inFile);
                }
            }
        }
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theRows);
            if (theResult == kErrorOK) {
                theResult = inFile.Read(theColumns);
                for (unsigned long r = 0; r < theRows; r++)
                    for (unsigned long c = 0; c < theColumns && theResult == kErrorOK; c++)
                        theResult = inFile.Read(this->mTributes[r * 5 + c]);
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGameResult);
        if (theResult == kErrorOK)
            theResult = this->mEvents.Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHighGroundBonus);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHealTimer);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDisconnected);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCivilizationLocked);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCivilizationChosen);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCivUpgradesPurchased);
        if (theResult == kErrorOK) {
            theCount = 0;
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                this->mUnitsOffMap.resize(theCount, NULL);
                for (i = 0; i < theCount && theResult == kErrorOK; i++) {
                    this->mUnitsOffMap[i] = new EEWUnitsOffMapInfo;
                    theResult = this->mUnitsOffMap[i]->Load(inMajorVersion, inMinorVersion, inFile);
                }
            }
        }
        if (theResult == kErrorOK) {
            theCount = 0;
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                for (i = 0; i < theCount && theResult == kErrorOK; i++) {
                    long theUnitID;
                    theResult = inFile.Read(theUnitID);
                    if (theResult == kErrorOK)
                        this->mWonderIDs.push_back(theUnitID);
                }
            }
        }
        if (inMajorVersion == 0 && inMinorVersion < 0xe2) {
            this->mWonders->Clear();
            this->mWonders->mItems.clear();
        }
        if (inMajorVersion == 0 && inMinorVersion < 0xe9)
            this->mFreeUnitCounter = 0;
        else
            theResult = inFile.Read(this->mFreeUnitCounter);
        if (theResult == kErrorOK) {
            if (inMajorVersion != 0 || inMinorVersion > 0xf0)
                for (i = 0; i < theColumns && theResult == kErrorOK; i++)
                    theResult = inFile.Read(this->mResourceTrade[i]);
            if (inMajorVersion != 0 || inMinorVersion > 0xf1)
                for (i = 0; i < theColumns && theResult == kErrorOK; i++)
                    theResult = inFile.Read(this->mInsuranceRefunds[i]);
            if (theResult == kErrorOK && (inMajorVersion != 0 || inMinorVersion >= 0xf5))
                theResult = inFile.Read(this->mNumVitalBuildings);
            if (theResult == kErrorOK && (inMajorVersion != 0 || inMinorVersion > 0xf5))
                theResult = inFile.Read(this->mFreeUnitInterval);
        }
        return theResult;
    }
}

// FUNCTION: EXE 0x567bff
// Loads the class media of every unit class that has media and records those classes for a later unload
void EEPlayer::LoadAllClassMedia()
{
    for (long i = 0; i < 0x367; i++) {
        EEUnitStats* theStats = this->mUnitClassUpgrades[i];
        if (theStats != NULL && theStats->GetMediaCount() > 0) {
            theStats->mUnitClass->LoadClassMedia();
            this->mSoundLoadedClasses.push_back(this->mUnitClassUpgrades[i]->mUnitClass->mUnitType);
        }
    }
}

// FUNCTION: EXE 0x567c5a
// Loads the command buttons of one unit class for the player, if the class index is valid
void EEPlayer::LoadUnitClassButtons(long inUnitClass)
{
    if ((unsigned long)inUnitClass >= 0x367)
        return;
    EEUnitStats* theStats = this->mUnitClassUpgrades[inUnitClass];
    if (theStats != NULL)
        theStats->LoadButtons(this);
}

// FUNCTION: EXE 0x567c7d
// Loads the media of one unit class for the player, if the class index is valid
void EEPlayer::SetUnitClassMedia(long inUnitClass, long inMediaFlags)
{
    if ((unsigned long)inUnitClass >= 0x367)
        return;
    EEUnitStats* theStats = this->mUnitClassUpgrades[inUnitClass];
    if (theStats != NULL)
        theStats->LoadMedia(this, inMediaFlags);
}

// FUNCTION: EXE 0x567ca4
// Reads one unit from a saved game and creates it on the player; a type/kind mismatch loads into a discarded unit
// whose ID is kept in the loaded-ID list; returns the read status
long EEPlayer::LoadUnit(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theType = 0;
    bool theComplex = false;
    bool theHasData = true;
    long theResult;
    EEUnitClass* theClass;
    EEUnit* theUnit;
    if (inMajorVersion == 0 && inMinorVersion < 0xdf) {
        theResult = inFile.Read(theType);
        if (theResult != kErrorOK)
            goto LoadComplete;
        theResult = inFile.Read(theComplex);
        if (theResult != kErrorOK)
            goto LoadComplete;
        theClass = gUnitClassTable[theType];
        if (theClass != NULL && ((theClass->mUnitType != kUnitTypePathPoint && theClass->mIsComplex == 0 && !theComplex)
            || ((theClass->mUnitType == kUnitTypePathPoint || theClass->mIsComplex == 1) && theComplex))) {
            theUnit = theClass->CreateUnitAtTile(this, -10000, -10000, NULL, 3.1415927f);
            theResult = theUnit->Load(0, inMinorVersion, inFile);
            if (theUnit->mIsQueuedForDelete)
                this->KillUnit(theUnit, true);
            goto LoadComplete;
        }
        if (theComplex)
            theUnit = EEComplexUnit::Create();
        else
            theUnit = EEUnit::Create();
        theResult = theUnit->Load(0, inMinorVersion, inFile);
        this->mLoadedUnitIDs.push_back(theUnit->GetUnitID());
        theUnit->RepairPointers(0, inMinorVersion);
        theUnit->Clear();
        theUnit->Dereference();
        goto LoadComplete;
    } else {
        theResult = inFile.Read(theType);
        if (theResult != kErrorOK)
            goto LoadComplete;
        theResult = inFile.Read(theHasData);
        if (!theHasData || theResult != kErrorOK)
            goto LoadComplete;
        theResult = inFile.Read(theComplex);
        if (theResult != kErrorOK)
            goto LoadComplete;
        theClass = gUnitClassTable[theType];
        if (theClass != NULL && ((theClass->mUnitType != kUnitTypePathPoint && theClass->mIsComplex == 0 && !theComplex)
            || ((theClass->mUnitType == kUnitTypePathPoint || theClass->mIsComplex == 1) && theComplex))) {
            theUnit = theClass->CreateUnitAtTile(this, -10000, -10000, NULL, 3.1415927f);
            theResult = theUnit->Load(inMajorVersion, inMinorVersion, inFile);
            if (theUnit->mIsQueuedForDelete)
                this->KillUnit(theUnit, true);
            goto LoadComplete;
        }
        if (theComplex)
            theUnit = EEComplexUnit::Create();
        else
            theUnit = EEUnit::Create();
        theResult = theUnit->Load(inMajorVersion, inMinorVersion, inFile);
        this->mLoadedUnitIDs.push_back(theUnit->GetUnitID());
        theUnit->RepairPointers(inMajorVersion, inMinorVersion);
        theUnit->Clear();
        theUnit->Dereference();
        goto LoadComplete;
    }
LoadComplete:
    return theResult;
}

// FUNCTION: EXE 0x567eda
// Returns the player's name
const UWideString& EEPlayer::GetName()
{
    return this->mName;
}

// FUNCTION: EXE 0x567ede
// Returns the player's next complex unit after inUnit (the first one for NULL), or NULL at the end
EEComplexUnit* EEPlayer::GetNextComplexUnit(EEUnit* inUnit)
{
    EEComplexUnit* theResult = NULL;
    EEUnit* theUnit = inUnit;
    if (this->mLastUnit != NULL) {
        if (theUnit == NULL) {
            theUnit = this->mFirstUnit;
            theResult = theUnit->GetComplexUnit();
        }
        while (theUnit != NULL && theResult == NULL) {
            theUnit = static_cast<EEUnit*>(theUnit->mNext);
            if (theUnit != NULL)
                theResult = theUnit->GetComplexUnit();
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x567f10
// Plays a unit's acknowledgement sound, at most once per delay for the same selection
// NEAR (71.9): registers only (the original caches TMGetTime in edi and the delay in ebx)
void EEPlayer::PlayAcknowledgement(EEComplexUnit* inUnit)
{
    if (this->mLastAcknowledgedGroup == this->mActiveGroup->mChangeTime) {
        unsigned long theDelay = gWorldTable.GetLongValue(83);
        if (TMGetTime() - this->mLastAcknowledgementTime <= theDelay)
            return;
    } else
        this->mLastAcknowledgedGroup = this->mActiveGroup->mChangeTime;
    this->mLastAcknowledgementTime = TMGetTime();
    EEPlayForLocalPlayer(this, inUnit->GetStats()->GetAcknowledgeSound(-1));
}

// FUNCTION: EXE 0x567f70
// Replaces each swapped old unit by its new one in all units, air traffic, the selection, control groups, wonders
// Called when the physics replaced units (EEPhysics 0x1534d1); a new unit of another player is not added to
// the groups, and an unbuilt wall becomes a construction site
void EEPlayer::UnitsSwapped(std::vector<EEPUnitSwap>& ioSwaps)
{
    EEComplexUnit* theOld = NULL;
    EEComplexUnit* theNew = NULL;
    EESelectedGroup* theGroup = this->mActiveGroup;
    for (EEComplexUnit* theUnit = this->GetNextComplexUnit(NULL); theUnit != NULL; theUnit = this->GetNextComplexUnit(theUnit))
        theUnit->ReplaceUnits(ioSwaps);
    for (long a = 0; a < 4; a++)
        mAirTraffic[a]->ReplaceUnits(ioSwaps);
    for (std::vector<EEPUnitSwap>::iterator it = ioSwaps.begin(); it != ioSwaps.end(); it++) {
        theOld = it->mOld;
        theNew = it->mNew;
        if (theOld == NULL || theNew == NULL)
            continue;
        if (theGroup->Contains(theOld)) {
            theGroup->RemoveUnit(theOld);
            if (theOld->mPlayer == theNew->mPlayer)
                theGroup->Add(theNew);
            this->mSelectedGroup->CopySelection(this->mActiveGroup);
        }
        for (long g = 0; g < 20; g++) {
            if (!this->mGroups[g]->Contains(theOld))
                continue;
            this->mGroups[g]->RemoveUnit(theOld);
            if (theOld->mPlayer == theNew->mPlayer)
                this->mGroups[g]->Add(theNew);
        }
        if (this == gWorld.GetCurrentPlayer())
            gUserInterface->PostMessage424Once();
        if (theNew->mUnitClass->mIsWall && !theNew->mIsBuilt)
            this->AddConstructionSite(theNew);
        std::vector<EEComplexUnit*>::iterator theIterator;
        if (this->mWonders->Contains(theOld, theIterator))
            this->mWonders->Replace(theOld, theNew);
    }
}

// FUNCTION: EXE 0x5680e4
// Makes every town of the player recheck whether it is near water
void EEPlayer::RestartTowns()
{
    for (long i = this->mTowns.size() - 1; i >= 0; i--) {
        if (this->mTowns[i] != NULL)
            this->mTowns[i]->UpdateNearWater();
    }
}

// FUNCTION: EXE 0x568120
// Reveals a tile to the player and, with shared line of sight, to every ally that shares it too
void EEPlayer::AddSharedSight(EETile* inTile, long inX, long inY)
{
    if (inTile == NULL)
        return;
    if (inTile->GetVisibility(this->mPlayerID) == 0)
        this->mSavedGroups.Reset(0x18, 1);
    inTile->AddSight(this);
    gDoppelgangerManager.TileShown(inTile, inX, inY, this);
    if (this->mSharedLOS != true)
        return;
    for (long i = this->mAllies.size() - 1; i >= 0; i--) {
        if (this->mAllies[i]->mSharedLOS) {
            inTile->AddSight(this->mAllies[i]);
            gDoppelgangerManager.TileShown(inTile, inX, inY, this->mAllies[i]);
        }
    }
}

// FUNCTION: EXE 0x5681e8
// Reveals the tile at a map location to the player and its sight-sharing allies
void EEPlayer::AddSharedSightAt(const EETileLocation& inLocation)
{
    EETile* theTile = gWorld.mMap->GetTileAtLocation(inLocation);
    this->AddSharedSight(theTile, inLocation.GetX(), inLocation.GetY());
}

// FUNCTION: EXE 0x56820e
// Reveals a tile to the player only (a first sighting adds to statistics series 0x18)
void EEPlayer::AddOwnSight(EETile* inTile, long inX, long inY)
{
    if (inTile == NULL)
        return;
    if (inTile->GetVisibility(this->mPlayerID) == 0)
        this->mSavedGroups.Reset(0x18, 1);
    inTile->AddSight(this);
    gDoppelgangerManager.TileShown(inTile, inX, inY, this);
}

// FUNCTION: EXE 0x568266
// Reveals the tile at a map location to the player only
void EEPlayer::AddOwnSightAt(const EETileLocation& inLocation)
{
    EETile* theTile = gWorld.mMap->GetTileAtLocation(inLocation);
    this->AddOwnSight(theTile, inLocation.GetX(), inLocation.GetY());
}

// FUNCTION: EXE 0x56828c
// Empties the player: running and control groups, viewed unit, events, towns, herds, off-map units, patrols
void EEPlayer::Clear()
{
    gDoppelgangerManager.UnregisterPlayer(this);
    unsigned long theCount = this->mRunningGroups.size();
    for (unsigned long i = 0; i < theCount; i++) {
        this->mRunningGroups[i]->Clear();
        this->mRunningGroups[i]->Dereference();
    }
    this->mRunningGroups.clear();
    this->mActiveGroup->Clear();
    for (long j = 0; j < 20; j++)
        this->mGroups[j]->Clear();
    if (this->mViewedUnit != NULL) {
        this->mViewedUnit->Dereference();
        this->mViewedUnit = NULL;
    }
    this->mEvents.Clear(this);
    EEDeletePointers(this->mTowns);
    this->mTownsByID.clear();
    EEDeletePointers(this->mHerds);
    this->ClearUnitsToRepatrol();
    EEDeletePointers(this->mUnitsOffMap);
    std::for_each(this->mPatrolGroups.begin(), this->mPatrolGroups.end(), EEPDereferencer());
    this->mPatrolGroups.clear();
    RTSPlayer::Clear();
}

// FUNCTION: EXE 0x5683da
// Removes a player from the ally list
void EEPlayer::RemoveAlly(EEPlayer* inPlayer)
{
    for (unsigned long i = 0; i < this->mAllies.size(); i++) {
        if (this->mAllies[i] == inPlayer) {
            this->mAllies.erase(this->mAllies.begin() + i);
            break;
        }
    }
}

// FUNCTION: EXE 0x56843f
// Removes a media item from the player's loaded-media list; returns whether it was there
bool EEPlayer::RemoveLoadedMedia(EEMedia* inMedia)
{
    std::vector<EEMedia*>::iterator theIterator = std::find(this->mLoadedMedia.begin(), this->mLoadedMedia.end(), inMedia);
    bool theResult = false;
    if (theIterator != this->mLoadedMedia.end()) {
        this->mLoadedMedia.erase(theIterator);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5684ab
// Stops sharing the player's building sight with another player, refreshing every land building's sight
void EEPlayer::RemoveSightSharer(EEPlayer* inPlayer)
{
    EEUnit* theUnit;
    for (theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->mUnitClass->IsLandBuilding())
            theUnit->SightOffHere();
    }
    this->mSightSharers.erase(std::find(this->mSightSharers.begin(), this->mSightSharers.end(), inPlayer));
    for (theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->mUnitClass->IsLandBuilding())
            theUnit->SightOnHere();
    }
}

// FUNCTION: EXE 0x56854e
// Finishes running groups left without units, then releases and drops every finished group
void EEPlayer::RemovalsDone()
{
    unsigned long theCount = this->mRunningGroups.size();
    for (unsigned long i = 0; i < theCount; i++) {
        EEGroup* theGroup = this->mRunningGroups[i];
        if (theGroup->GetNumUnits() == 0 && std::find(this->mFinishedGroups.begin(), this->mFinishedGroups.end(), theGroup) == this->mFinishedGroups.end()) {
            // Virtual04 copies these entries from mPendingGroups; these are EEActiveGroup objects.
            static_cast<EEActiveGroup*>(theGroup)->Finish();
        }
    }
    theCount = this->mFinishedGroups.size();
    for (unsigned long j = 0; j < theCount; j++) {
        this->mFinishedGroups[j]->Clear();
        this->mFinishedGroups[j]->Dereference();
        std::vector<EEGroup*>::iterator theIterator = std::find(this->mRunningGroups.begin(), this->mRunningGroups.end(), this->mFinishedGroups[j]);
        if (theIterator != this->mRunningGroups.end()) {
            (*theIterator)->Dereference();
            this->mRunningGroups.erase(theIterator);
        }
    }
    this->mFinishedGroups.clear();
}

// FUNCTION: EXE 0x56870e
// Cancels a queued improvement of a unit class (inFlag: never the one in progress) and refunds its cost
void EEPlayer::RemoveQueuedImprovement(long inUnitType, long inImprovement, bool inFlag)
{
    std::map<unsigned long, std::pair<float, std::vector<unsigned long> > >::iterator theIterator = this->mQueuedImprovements.find(inUnitType);
    if (theIterator == this->mQueuedImprovements.end())
        return;
    std::vector<unsigned long>& theQueue = (*theIterator).second.second;
    std::vector<unsigned long>::iterator theImprovement;
    if (inFlag == true) {
        if (theQueue.size() > 1)
            theImprovement = std::find(theQueue.begin() + 1, theQueue.end(), inImprovement);
        else
            theImprovement = theQueue.end();
    } else {
        theImprovement = std::find(theQueue.begin(), theQueue.end(), inImprovement);
    }
    if (theImprovement == theQueue.end())
        return;
    theQueue.erase(theImprovement);
    this->RefundImprovement(gUnitClassTable[inUnitType], inImprovement, 1.0f);
    if (theIterator == this->mQueuedImprovements.begin())
        this->mQueuedImprovements[inUnitType].first = 0.0f;
    if (theQueue.size() == 0)
        this->mQueuedImprovements.erase(theIterator);
}

// FUNCTION: EXE 0x568889
// Removes a patrol group from the player and releases it
void EEPlayer::RemovePatrolGroup(EEPPatrolGroup* inGroup)
{
    std::vector<EEPPatrolGroup*>::iterator theIterator = std::find(this->mPatrolGroups.begin(), this->mPatrolGroups.end(), inGroup);
    this->mPatrolGroups.erase(theIterator);
    inGroup->Dereference();
}

// FUNCTION: EXE 0x5688ed
// Deselects the unit with the given ID, if it exists
void EEPlayer::RemoveUnitFromSelectedGroup(long inUnitID)
{
    RTSUnit* theUnit = ::gWorld.GetUnit(inUnitID);
    if (theUnit != NULL)
        this->DeselectUnit(static_cast<EEUnit*>(theUnit));
}

// FUNCTION: EXE 0x56890e
// Takes away the power of a lost wonder (conversion, hit-point bonus, shared bonus, enemy sight, healing ...)
void EEPlayer::RemoveWonderPower(EEComplexUnit* inUnit)
{
    switch (inUnit->mUnitClass->mWonderBonus) {
    case 1:
        this->mWonderPowers.erase(std::find(this->mWonderPowers.begin(), this->mWonderPowers.end(), (long)1));
        this->mAreaConversion = false;
        break;
    case 2: {
        this->mWonderPowers.erase(std::find(this->mWonderPowers.begin(), this->mWonderPowers.end(), (long)2));
        EEUnitSet* theSet = gUnitSetManager.GetSet(inUnit->mUnitClass->mWonderUnitSet);
        for (unsigned long i = 0; i < this->mUnitClassUpgrades.size(); i++) {
            if (this->mUnitClassUpgrades[i] == NULL || !theSet->Contains(this->mUnitClassUpgrades[i]->mUnitClass->mUnitType))
                continue;
            long theType = this->mUnitClassUpgrades[i]->mUnitClass->mUnitType;
            float theFactor = EEGameVariant::GetGameVariant(gWorld.mGameOptions.GetVariant())->GetFactor(theType);
            float theAmount = gWorldTable.GetFloatValue(0x32);
            EEUpgradeHitPoints(this->mUnitClassUpgrades[i], this->mUnitClassUpgrades[i]->mUnitClass->GetUnitType(),
                       -((theFactor + 1.0f) * theAmount), this);
        }
        break;
    }
    case 3:
        this->mWonderPowers.erase(std::find(this->mWonderPowers.begin(), this->mWonderPowers.end(), (long)3));
        break;
    case 4:
        gWorld.RemoveShareBonus();
        this->mWonderPowers.erase(std::find(this->mWonderPowers.begin(), this->mWonderPowers.end(), (long)4));
        gWorld.ApplyShareBonus();
        break;
    case 5: {
        this->mWonderPowers.erase(std::find(this->mWonderPowers.begin(), this->mWonderPowers.end(), (long)5));
        for (unsigned long thePlayer = 1; thePlayer < gWorld.mNumPlayers; thePlayer++)
            if (thePlayer != this->mPlayerID && gWorld.mPlayers[thePlayer] != NULL && this->mEnemies[thePlayer] == 1)
                gWorld.mPlayers[thePlayer]->RemoveSightSharer(this);
        break;
    }
    case 6:
        this->mHealsUnits = false;
        this->mHealedUnitSet = NULL;
        this->mWonderPowers.erase(std::find(this->mWonderPowers.begin(), this->mWonderPowers.end(), (long)6));
        break;
    }
}

// FUNCTION: EXE 0x568d33
// Removes an animal from its herds, deleting any herd left with no tiles
void EEPlayer::RemoveUnitFromHerd(EEComplexUnit* inUnit)
{
    if (!EEHerd::IsAnimalOnMap(inUnit))
        return;
    for (long i = this->mHerds.size() - 1; i >= 0; i--) {
        EEHerd* theHerd = this->mHerds[i];
        if (!theHerd->HasUnit(inUnit))
            continue;
        theHerd->RemoveUnit(inUnit);
        if (theHerd->mTiles.Size() != 0)
            continue;
        gContinentManager.RemoveArea(theHerd);
        this->mHerds.erase(this->mHerds.begin() + i);
        delete theHerd;
    }
}

// FUNCTION: EXE 0x568deb
// Takes a unit being removed off its tile, its region and the influence map
void EEPlayer::RemovingUnit(RTSUnit* inUnit)
{
    EEUnit* theUnit = static_cast<EEUnit*>(inUnit);
    theUnit->mUnitClass->RemoveFromTile(theUnit);
    if (theUnit->mTileNode == NULL)
        return;
    theUnit->mUnitClass->RemoveFromRegion(theUnit);
    if (theUnit->mUnitClass->mInfluence != 0)
        gWorld.mInfluenceTable->RemoveUnit(theUnit, theUnit->mTileX, theUnit->mTileY);
}

// FUNCTION: EXE 0x568e30
// Deletes the off-map record of a unit, if it has one
void EEPlayer::RemoveOffMapUnit(RTSUnit* inUnit)
{
    if (inUnit == NULL)
        return;
    for (long i = this->mUnitsOffMap.size() - 1; i >= 0; i--) {
        if (inUnit != this->mUnitsOffMap[i]->mUnit)
            continue;
        EEWUnitsOffMapInfo* theInfo = this->mUnitsOffMap[i];
        this->mUnitsOffMap.erase(this->mUnitsOffMap.begin() + i);
        delete theInfo;
        break;
    }
}

// FUNCTION: EXE 0x568eb9
// Removes a unit from the selection, or clears the viewed unit when that is the unit
void EEPlayer::DeselectUnit(EEUnit* inUnit)
{
    if (inUnit != this->mViewedUnit && inUnit->GetComplexUnit() != NULL) {
        this->mActiveGroup->RemoveUnit(inUnit->GetComplexUnit());
        this->mSelectedGroup->RemoveUnit(inUnit->GetComplexUnit());
    } else if (this->mViewedUnit != NULL) {
        this->mViewedUnit->Dereference();
        this->mViewedUnit = NULL;
    }
}

// FUNCTION: EXE 0x568f03
// Removes a town building from the towns containing it, deleting any town left with no tiles
void EEPlayer::RemoveUnitFromTown(EEComplexUnit* inUnit)
{
    U2DPoint<long> theTile;
    if (!EETown::IsTownUnit(inUnit))
        return;
    theTile = inUnit->FindAreaTile(true, 1);
    for (long i = this->mTowns.size() - 1; i >= 0; i--) {
        EETown* theTown = this->mTowns[i];
        if (!theTown->Contains(theTile))
            continue;
        theTown->RemoveUnit(inUnit);
        if (theTown->mTiles.Size() != 0)
            continue;
        gContinentManager.RemoveArea(theTown);
        this->mTowns.erase(this->mTowns.begin() + i);
        delete theTown;
    }
}

// FUNCTION: EXE 0x568fdf
// Removes a unit from the player's construction sites
void EEPlayer::RemoveConstructionSite(EEComplexUnit* inUnit)
{
    this->mConstructionSites->Remove(inUnit);
}

// FUNCTION: EXE 0x568ff2
// Removes a wonder from the player; when the last of its type goes, its wonder power is taken away
void EEPlayer::RemoveWonder(EEComplexUnit* inUnit)
{
    this->mWonders->Remove(inUnit);
    inUnit->mCloaked = 0;
    this->mSavedGroups.Lower(0x10, 1);
    this->mWonderTypeCounts[inUnit->mUnitClass->mUnitType]--;
    if (this->mWonderTypeCounts[inUnit->mUnitClass->mUnitType] == 0) {
        this->RemoveWonderPower(inUnit);
        this->mWonderTypeCounts.erase(inUnit->mUnitClass->mUnitType);
    }
}

// FUNCTION: EXE 0x5690a5
// Lets every unit of the player finish loading; returns 0
long EEPlayer::FinishUnitsLoad(long inMajorVersion, long inMinorVersion)
{
    for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
        theUnit->FinishLoad(inMajorVersion, inMinorVersion);
    return 0;
}

// FUNCTION: EXE 0x5690ca
// Relinks the player after a load: towns, construction sites, wonders, groups, herds, events, queued improvements
// and off-map units are rebuilt from saved IDs; returns the status of the last step
// NEAR: inline budget (the original calls begin() out of line for mPatrolGroups[i] and mConstructionSiteIDs[j]), register allocation
// (the GetAvailablePoints test is != 0 here; the original compares unsigned, jbe)
long EEPlayer::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    std::map<unsigned long, std::pair<float, std::vector<unsigned long> > >::iterator theIterator;
    std::vector<unsigned long> theImprovements;
    std::vector<long> theUnused;
    long theResult;
    unsigned long i;
    this->mCivilization.Reapply(this, inFirst, inSecond);
    RTSPlayer::RepairPointers(inFirst, inSecond);
    for (i = 0; i < this->mPatrolGroups.size(); i++)
        this->mPatrolGroups[i]->RepairPointers(inFirst, inSecond);
    for (long theAirport = 0; theAirport < 4; theAirport++)
        theResult = mAirTraffic[theAirport]->RepairPointers(inFirst, inSecond);
    this->mLoadedUnitIDs.clear();
    if (this->GetPlayerID() >= 1) {
        for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
            if (theUnit->GetComplexUnit() != NULL)
                this->AddUnitToTown(theUnit->GetComplexUnit());
    }
    if (!this->mCivilization.GetNoPointsAdded() && this->mCivilization.GetAvailablePoints() != 0)
        theResult = EESetCivBuilderButtonBlinking(true);
    unsigned long j;
    for (j = 0; j < this->mConstructionSiteIDs.size(); j++) {
        RTSUnit* theUnit = gWorld.GetUnit(this->mConstructionSiteIDs[j]);
        if (theUnit != NULL && theUnit->mTileNode != NULL)
            this->mConstructionSites->Add(static_cast<EEUnit*>(theUnit)->mComplexUnit);
    }
    this->mConstructionSiteIDs.clear();
    for (j = 0; j < this->mWonderIDs.size(); j++) {
        RTSUnit* theUnit = gWorld.GetUnit(this->mWonderIDs[j]);
        if (theUnit != NULL && theUnit->mTileNode != NULL)
            this->mWonders->Add(static_cast<EEUnit*>(theUnit)->mComplexUnit);
    }
    this->mWonderIDs.clear();
    this->RecountPopulation();
    if (theResult == kErrorOK)
        this->mActiveGroup->RepairPointers(inFirst, inSecond);
    for (long theGroup = 0; theGroup < 20; theGroup++)
        this->mGroups[theGroup]->RepairPointers(inFirst, inSecond);
    for (unsigned long theHerd = 0; theHerd < this->mHerds.size(); theHerd++)
        this->mHerds[theHerd]->RepairPointers(inFirst, inSecond);
    this->RegisterHerds();
    theResult = this->mEvents.RepairEvents(this, inFirst, inSecond);
    theIterator = this->mQueuedImprovements.begin();
    while (theIterator != this->mQueuedImprovements.end()) {
        theImprovements.clear();
        theImprovements = (*theIterator).second.second;
        (*theIterator).second.second.clear();
        for (unsigned long k = 0; k < theImprovements.size(); k++)
            this->QueueImprovement((*theIterator).first, theImprovements[k], false, true);
        if ((*theIterator).second.second.size() > 0)
            theIterator++;
        else
            theIterator = this->mQueuedImprovements.erase(theIterator);
    }
    this->mSelectedGroup->CopySelection(this->mActiveGroup);
    this->mTechnologies->Get(kTechCapitol)->mIsAllowed = this->mNumCapitols == 0;
    if (this->mTechnologies->Get(kTechSettlement)->GetIsObsolete())
        this->mTechnologies->Get(kTechTownCenter)->mIsAllowed = this->mNumCapitols > 0;
    else {
        this->mTechnologies->Get(kTechSettlement)->mIsAllowed = this->mNumCapitols > 0;
        this->mTechnologies->Get(kTechTownCenter)->mIsAllowed = false;
    }
    for (long theTown = this->mTowns.size() - 1; theTown >= 0; theTown--)
        if (this->mTowns[theTown] != NULL && this->mTowns[theTown]->mPatrolGroup == NULL)
            this->mTowns[theTown]->RefreshPatrolGroup();
    if (inFirst == 0 && inSecond < 0xd7) {
        for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
            if (theUnit->GetTileX() == -10000)
                this->AddOffMapUnit(theUnit);
    } else {
        for (long theInfo = this->mUnitsOffMap.size() - 1; theInfo >= 0; theInfo--) {
            this->mUnitsOffMap[theInfo]->RepairPointers(inFirst, inSecond);
            EEWUnitsOffMapInfo* theOffMap = this->mUnitsOffMap[theInfo];
            if (theOffMap->mUnit == NULL) {
                this->mUnitsOffMap.erase(this->mUnitsOffMap.begin() + theInfo);
                delete theOffMap;
            }
        }
    }
    this->RecountHeroesAndTeleporters();
    return theResult;
}

// FUNCTION: EXE 0x5695e7
// When a human player reaches a valid epoch, lets every computer player catch up to it
void EEPlayer::EpochReached(long inEpoch)
{
    if (!this->IsHuman() || !(inEpoch >= kTechEpochPrehistoric) || !(inEpoch < kTechEpochAll))
        return;
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        EEPlayer* theOther = gWorld.mPlayers[i];
        EEComputerPlayer* thePlayer = dynamic_cast<EEComputerPlayer*>(theOther);
        if (thePlayer != NULL)
            thePlayer->CatchUpEpoch(this);
    }
}

// FUNCTION: EXE 0x56964a
// Starts the timer of a unit class's next queued improvement if it can be done; returns true when its queue is empty
bool EEPlayer::AdvanceImprovement(unsigned long inUnitType)
{
    bool theResult = false;

    if (this->mQueuedImprovements.find(inUnitType) != this->mQueuedImprovements.end()) {
        EEUnitClass* theClass = gUnitClassTable[inUnitType];
        if (theClass != NULL && this->mQueuedImprovements[inUnitType].second.size() > 0) {
            long theImprovement = *this->mQueuedImprovements[inUnitType].second.begin();
            if (this->CanImprove(theClass->GetUnitType(), theImprovement, NULL) == true) {
            if (this->mInstantBuild) {
                this->mQueuedImprovements[inUnitType].first = 1.0f;
            } else {
                EEUnitClassUpgrade* theUpgrade = this->mUnitClassUpgrades[inUnitType];
                if (theUpgrade->mImprovement.GetLevel(theImprovement) == 1) {
                    float theTime = (long)gWorldTable.GetLongValue(theImprovement + 57);
                    this->mQueuedImprovements[inUnitType].first = theTime;
                } else {
                    float theTime = (long)(*(gWorldTable.mEntries.begin() + theImprovement + 57)).mLongValue2;
                    this->mQueuedImprovements[inUnitType].first = theTime;
                }
            }
            if (this == gWorld.GetCurrentPlayer())
                gUserInterface->UpdateJITManufacturing();
            }
        } else {
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5697b4
// Resets the player's tech tree to the global one and clears the population bonus and tribute fee
void EEPlayer::ResetTechTree()
{
    *this->mTechnologies = gTechTree;
    this->mTechnologies->SetPlayerIndex(this->mPlayerID);
    this->mTechnologies->SetPlayer(this);
    this->mBB44 = false;
    this->mPopulationLimitBonus = 0;
    this->mTributeFee = kTributeFee;
}

#include "lle/FSMemoryFile.h"
#include "ee/EESaveGameFiles.h"

// FUNCTION: EXE 0x5697ff
// Rebuilds the player's tech tree and upgrades from scratch, keeping its state and epoch range, and unloads the
// unit media its nodes had loaded
void EEPlayer::ReloadTechTree(bool inFlag)
{
    std::vector<long> theMedia;
    std::vector<long> theIDs;
    long theStartEpoch = this->mTechnologies->mStartEpoch;
    long theEndEpoch = this->mTechnologies->mEndEpoch;
    FSMemoryFile theFile(1000, true, 1);
    this->mTechnologies->GetNodeIDs(theIDs);
    for (unsigned long i = 0; i < theIDs.size(); i++) {
        EETechTreeNode* theNode = this->mTechnologies->Get(theIDs[i]);
        if (theNode != NULL && theNode->mIsUnitMediaLoaded == true) {
            long theType = theNode->mData->mUnitClass;
            theMedia.push_back(theType);
        }
    }
    this->mTechnologies->Save(kSaveGameMajorVersion, kSaveGameMinorVersion, theFile);
    gWorld.RemoveShareBonus();
    this->ResetTechTree();
    this->RefreshUpgrades();
    theFile.SetPosition(0, kFileOriginBegin, false);
    if (this->mTechnologies->Load(kSaveGameMajorVersion, kSaveGameMinorVersion, theFile) == kErrorOK) {
        this->mTechnologies->ResetEpochRange(theStartEpoch, theEndEpoch);
        this->mTechnologies->ApplySavedState(true, !inFlag);
    }
    for (unsigned long j = 0; j < theMedia.size(); j++)
        this->UnloadUnitClassMedia(theMedia[j]);
}

// FUNCTION: EXE 0x569990
// Recomputes every unit class upgrade and rescales the hit points, flight time and area effects of existing units
void EEPlayer::RefreshUpgrades()
{
    for (unsigned long i = 0; i < 0x367; i++) {
        EEUnitClassUpgrade* theUpgrade = this->GetUpgrade(i);
        if (theUpgrade == NULL)
            continue;
        long theOldValue = theUpgrade->mFlightTime;
        long theOldHitPoints = theUpgrade->GetMaxHitPoints();
        long theOldKind = theUpgrade->mAreaEffectTemplates[0];
        theUpgrade->Init();
        if (theOldHitPoints == theUpgrade->mMaxHitPoints
            && theOldValue == theUpgrade->mFlightTime && theOldKind == theUpgrade->mAreaEffectTemplates[0])
            continue;
        for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            if (theUnit->mUnitClass->GetUnitType() != theUpgrade->mUnitClass->GetUnitType())
                continue;
            EEComplexUnit* theComplex = theUnit->GetComplexUnit();
            long theNewHitPoints = theUpgrade->mMaxHitPoints;
            if (theOldHitPoints != theNewHitPoints && (theComplex == NULL || theComplex->GetIsBuilt() == true)) {
                float theScaled = ((float)theUnit->GetHitPoints() / theOldHitPoints) * theNewHitPoints;
                float theHitPoints = (float)UMCeil(theScaled);
                if ((long)theHitPoints > 0)
                    theUnit->SetHitPoints((long)theHitPoints);
            }
            if (theComplex == NULL)
                continue;
            long theNewValue = theUpgrade->mFlightTime;
            if (theOldValue != theNewValue) {
                float theScaled = ((float)theComplex->mFlightTime / theOldValue) * theNewValue;
                float theValue = (float)UMCeil(theScaled);
                if ((long)theValue > 0)
                    theComplex->SetFlightTime((long)theValue);
            }
            if (theOldKind != theUpgrade->mAreaEffectTemplates[0])
                theComplex->MakeAreaEffects();
        }
    }
    this->mCivilization.AddUpgrades(this->mCivilization.GetPurchasedUpgrades(), this->GetPlayerID(), false);
}

// FUNCTION: EXE 0x569b26
// Briefly reveals the area around one unit (by ID) or all units of a type, with the given sight range
void EEPlayer::PulseSight(bool inByID, float inSight, long inID)
{
    std::vector<EEUnit*> theUnits;
    if (inByID) {
        if (gWorld.GetUnit(inID) != NULL) {
            EEUnit* theUnit = gWorld.GetUnit(inID);
            theUnits.push_back(theUnit);
        }
    } else {
        for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
            if (theUnit->mTileNode != NULL && theUnit->mUnitClass->mUnitType == inID)
                theUnits.push_back(theUnit);
    }
    for (unsigned long i = 0; i < theUnits.size(); i++) {
        EEUnit* theUnit = theUnits[i];
        float theSight = inSight - theUnit->mStats->mLineOfSight;
        theUnit->GetComplexUnit()->SightOffHere();
        theUnit->GetComplexUnit()->mSight = theSight;
        theUnit->SightOnHere();
        theUnit->GetComplexUnit()->SightOffHere();
        theUnit->GetComplexUnit()->mSight = 0;
        theUnit->SightOnHere();
    }
}

// FUNCTION: EXE 0x569c46
// Writes the player's state to a saved game; returns the write status (some writes' errors are ignored)
// NEAR: mQueuedImprovements value is pair<float, vector<unsigned long> > (Write slot 0x7c, FSSaveVector<unsigned long>); +0xb64 is a
// long[5]; the sGoalCounter extern; the patrol loop's dead lea and the cloaked-unit loop's schedule
long EEPlayer::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    std::map<unsigned long, std::pair<float, std::vector<unsigned long> > >::iterator theIterator;
    std::vector<long> theUnused;
    bool theHaveTechTree;
    long theResult = kErrorOK;
    unsigned long i;
    for (i = 0; i < 10 && theResult == kErrorOK; i++)
        theResult = outFile.Write(this->mScenarioVariables[i]);
    unsigned long theCount = 0;
    for (unsigned long j = 0; j < this->mUnitClassUpgrades.size(); j++)
        if (this->mUnitClassUpgrades[j] != NULL)
            theCount++;
    outFile.Write(theCount);
    unsigned long theVersion = 0x10;
    theResult = outFile.Write(theVersion);
    if (theResult == kErrorOK) {
        for (theCount = 0; theCount < this->mUnitClassUpgrades.size() && theResult == kErrorOK; theCount++) {
            if (this->mUnitClassUpgrades[theCount] == NULL)
                continue;
            theResult = outFile.Write(theCount);
            if (theResult == kErrorOK)
                theResult = this->mUnitClassUpgrades[theCount]->Save(inMajorVersion, inMinorVersion, outFile);
        }
    }
    RTSPlayer::Save(inMajorVersion, inMinorVersion, outFile);
    {
        unsigned long theGoalCounter = sGoalCounter;
        theResult = outFile.Write(theGoalCounter);
    }
    if (theResult == kErrorOK) {
        theHaveTechTree = this->mTechnologies != NULL;
        theResult = outFile.Write(theHaveTechTree);
        if (theResult == kErrorOK) {
            if (this->mTechnologies != NULL)
                theResult = this->mTechnologies->Save(inMajorVersion, inMinorVersion, outFile);
            if (theResult == kErrorOK) {
                theResult = outFile.Write(this->mPopLimit);
                if (theResult == kErrorOK) {
                    theCount = 7;
                    theResult = outFile.Write(theCount);
                    if (theResult == kErrorOK && theCount > 0) {
                        for (unsigned long r = 0; r < theCount && theResult == kErrorOK; r++)
                            theResult = outFile.Write(this->mResourceCaps[r]);
                    }
                    if (theResult == kErrorOK) {
                        if (theResult == kErrorOK) {
                            theCount = 0;
                            long p;
                            for (p = this->mPatrolGroups.size() - 1; p >= 0; p--)
                                if (this->mPatrolGroups[p] != NULL && !this->mPatrolGroups[p]->mIsTownPatrol)
                                    theCount++;
                            theResult = outFile.Write(theCount);
                            if (theResult == kErrorOK && theCount > 0) {
                                for (p = this->mPatrolGroups.size() - 1; p >= 0 && theResult == kErrorOK; p--) { EEPPatrolGroup*& theGroup = this->mPatrolGroups[p];
                                    if (theGroup != NULL && !theGroup->mIsTownPatrol)
                                        theResult = this->mPatrolGroups[p]->Save(inMajorVersion, inMinorVersion, &outFile); }
                            }
                        }
                    }
                }
            }
        }
    }
    this->mSavedGroups.Save(inMajorVersion, inMinorVersion, outFile);
    outFile.Write(this->mPopulationCount);
    outFile.Write(this->mEndOfGameCount);
    for (theCount = 0; theCount < 17; theCount++) {
        outFile.Write(this->mEnemies[theCount]);
        outFile.Write(this->mSendChatMessage[theCount]);
    }
    theCount = 4;
    theResult = outFile.Write(theCount);
    if (theResult != kErrorOK)
        return theResult;
    for (i = 0; i < 4 && theResult == kErrorOK; i++)
        theResult = mAirTraffic[i]->Save(inMajorVersion, inMinorVersion, &outFile);
    if (theResult == kErrorOK)
        theResult = this->mActiveGroup->Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCombatState);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNumCapitols);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mSharedLOS);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mChatType);
    if (theResult == kErrorOK)
        theResult = this->mCivilization.Save(outFile);
    if (theResult == kErrorOK) {
        for (i = 0; i < 20; i++)
            if (theResult == kErrorOK)
                theResult = this->mGroups[i]->Save(inMajorVersion, inMinorVersion, outFile);
    }
    if (theResult == kErrorOK)
        theResult = this->mCivilizationFile.Save(outFile);
    if (theResult == kErrorOK) {
        unsigned long theSize = this->mQueuedImprovements.size();
        theResult = outFile.Write(theSize);
    }
    if (theResult == kErrorOK) {
        for (theIterator = this->mQueuedImprovements.begin(); theIterator != this->mQueuedImprovements.end(); ++theIterator) {
            theResult = outFile.Write((*theIterator).first);
            if (theResult != kErrorOK)
                continue;
            theResult = outFile.Write((*theIterator).second.first);
            if (theResult == kErrorOK)
                theResult = FSSaveVector(outFile, (*theIterator).second.second, UString("Queued Improvements"));
        }
    }
    if (theResult == kErrorOK) {
        unsigned long theSize = this->mConstructionSites->GetSize();
        theResult = outFile.Write(theSize);
    }
    if (theResult == kErrorOK) {
        std::vector<EEComplexUnit*>::iterator theUnit;
        for (theUnit = this->mConstructionSites->mItems.begin(); theUnit != this->mConstructionSites->mItems.end(); theUnit++) {
            unsigned long theID = (*theUnit)->mUnitID;
            theResult = outFile.Write(theID);
        }
    }
    if (theResult == kErrorOK) {
        theCount = this->mHerds.size();
        theResult = outFile.Write(theCount);
    }
    if (theResult == kErrorOK) {
        for (i = 0; i < theCount && theResult == kErrorOK; i++)
            theResult = this->mHerds[i]->Save(inMajorVersion, inMinorVersion, outFile);
    }
    if (theResult == kErrorOK)
        theResult = outFile.Write((unsigned long)2);
    if (theResult == kErrorOK) {
        theResult = outFile.Write((unsigned long)5);
        for (long theDirection = 0; theDirection < 2; theDirection++)
            for (theCount = 0; theCount < 5 && theResult == kErrorOK; theCount++)
                theResult = outFile.Write(this->mTributes[theDirection * 5 + theCount]);
    }
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mGameResult);
    if (theResult == kErrorOK)
        theResult = this->mEvents.Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mHighGroundBonus);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mHealTimer);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDisconnected);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCivilizationLocked);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCivilizationChosen);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCivUpgradesPurchased);
    if (theResult == kErrorOK) {
        theCount = this->mUnitsOffMap.size();
        theResult = outFile.Write(theCount);
    }
    if (theResult == kErrorOK) {
        for (i = 0; i < theCount && theResult == kErrorOK; i++)
            theResult = this->mUnitsOffMap[i]->Save(inMajorVersion, inMinorVersion, outFile);
    }
    if (theResult == kErrorOK) {
        theCount = this->mWonders->GetSize();
        theResult = outFile.Write(theCount);
    }
    if (theResult == kErrorOK) {
        std::vector<EEComplexUnit*>::iterator theCloaked;
        theCloaked = this->mWonders->mItems.begin();
        while (true) {
            if (theCloaked == this->mWonders->mItems.end() || theResult != kErrorOK)
                break;
            unsigned long theID = (*theCloaked)->mUnitID;
            theResult = outFile.Write(theID);
            theCloaked++;
        }
    }
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mFreeUnitCounter);
    if (theResult == kErrorOK) {
        for (theCount = 0; theCount < 5 && theResult == kErrorOK; theCount++)
            theResult = outFile.Write(this->mResourceTrade[theCount]);
    }
    if (theResult == kErrorOK) {
        for (theCount = 0; theCount < 5 && theResult == kErrorOK; theCount++)
            theResult = outFile.Write(this->mInsuranceRefunds[theCount]);
    }
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNumVitalBuildings);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mFreeUnitInterval);
    return theResult;
}

// FUNCTION: EXE 0x56a4b5
// Writes one unit's type, kind and data to a saved game (type 0xdf only as a type); returns the write status
// NEAR: frame slots of the two bools and the complex unit
long EEPlayer::SaveUnit(RTSUnit* inUnit, unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    bool theIsComplex;
    EEComplexUnit* theComplexUnit = static_cast<EEUnit*>(inUnit)->mComplexUnit;
    EEUnitClass* theClass = static_cast<EEUnit*>(inUnit)->mUnitClass;
    bool theIsUnit = true;
    unsigned long theType = theClass->mUnitType;
    long theResult = outFile.Write(theType);
    if (theResult != kErrorOK)
        return theResult;
    theIsUnit = theClass->mUnitType != kUnitTypePathPoint;
    theResult = outFile.Write(theIsUnit);
    if (!theIsUnit)
        return theResult;
    theIsComplex = theComplexUnit != NULL;
    theResult = outFile.Write(theIsComplex);
    if (theResult == kErrorOK)
        theResult = inUnit->Save(inMajorVersion, inMinorVersion, outFile);
    return theResult;
}

// FUNCTION: EXE 0x56a53b
// Selects the unit with the given ID (optionally clearing the selection), playing its sound for own units
void EEPlayer::SelectUnit(unsigned long inUnitID, bool inArg1, bool inNewGroup)
{
    EEUnit* theUnit = static_cast<EEUnit*>(::gWorld.GetUnit(inUnitID));
    this->UpdateSelection(NULL, true);
    if (inNewGroup)
        this->mActiveGroup->Clear();
    if (theUnit == NULL || !theUnit->mUnitClass->IsSelectable(theUnit))
        return;
    unsigned long theType = theUnit->mUnitClass->mUnitType;
    if (theType == kUnitTypePathPoint)
        this->TrySelectUnit(theUnit, inArg1);
    else if (theType != kUnitTypeVolcano) {
        if (theUnit->mHitPoints > 0
            && (!inArg1 || this->mSeesAll || theUnit->mPlayer == this
                || !gDoppelgangerManager.HasDoppelganger(static_cast<EETile*>(theUnit->mTile), theUnit, this->GetPlayerID()))
            && this->TrySelectUnit(theUnit, inArg1)) {
            EEPlayer* thePlayer = theUnit->mPlayer;
            if (thePlayer == this || thePlayer->mPlayerID == 0) {
                if (gWorld.mExecutingLocalSelection && thePlayer == this)
                    EEPlayForLocalPlayer(this, theUnit->mStats->GetSelectionSound(-1));
                return;
            }
        }
        if (theUnit->mResourceAmount > 0 && theUnit->mUnitClass->mUnitType != kUnitTypeFarm
            && !gDoppelgangerManager.HasDoppelganger(static_cast<EETile*>(theUnit->mTile), theUnit, this->GetPlayerID()))
            this->TrySelectUnit(theUnit, inArg1);
    }
}

// FUNCTION: EXE 0x56a658
// Replaces the selection with the built units of the given IDs and plays the last one's selection sound
void EEPlayer::SelectUnits(std::vector<long>& inUnitIDs)
{
    EEComplexUnit* theLastUnit = NULL;
    EESelectedGroup* theGroup = this->mActiveGroup;
    this->UpdateSelection(NULL, true);
    theGroup->Clear();
    for (unsigned long i = 0; i < inUnitIDs.size(); i++) {
        long& theID = inUnitIDs[i];
        RTSUnit* theUnit = this->GetUnit(theID);
        if (theUnit == NULL)
            continue;
        EEComplexUnit* theComplexUnit = static_cast<EEUnit*>(theUnit)->mComplexUnit;
        if (!theGroup->Contains(theComplexUnit) && theComplexUnit->mIsBuilt != false
            && theComplexUnit->mUnitClass->mUnitType != kUnitTypePathPoint) {
            theGroup->Add(theComplexUnit);
            theLastUnit = theComplexUnit;
        }
    }
    if (theLastUnit != NULL && gWorld.mExecutingLocalSelection)
        EEPlayForLocalPlayer(this, theLastUnit->mStats->GetSelectionSound(-1));
}

// FUNCTION: EXE 0x56a70a
// Moves movable units out of a new building's footprint, onto a waypoint outside it or the nearest free tile
void EEPlayer::ClearFootprint(EEComplexUnit* inUnit)
{
    GE3DPoint theWaypoint;
    GERectangle theBounds;
    GE2DPoint thePoint;
    std::vector<EEUnit*> theUnits;
    std::vector<EETileCoordinate> theTiles;
    long theX = inUnit->mTileX;
    long theY = inUnit->mTileY;
    long theSize = inUnit->mUnitClass->mTileSize;
    long theType = inUnit->mUnitClass->mUnitType;
    if (theType == kUnitTypeRoboticFarm || gUnitSetManager.Contains(theType, 0x4c))
        return;
    long theRight = theSize - 1 + theX;
    theBounds.mBottom = (float)(theSize - 1 + theY);
    theBounds.mTop = (float)theY;
    theBounds.mLeft = (float)theX;
    theBounds.mRight = (float)theRight;
    EEGetUnitsInRectangle(theBounds, theUnits);
    unsigned long theCount = theUnits.size();
    for (unsigned long i = 0; i < theCount; i++) {
        EEComplexUnit* theUnit = theUnits[i]->mComplexUnit;
        if (theUnit == NULL || theUnit == inUnit || !EEIsUnitMovable(inUnit, theUnit))
            continue;
        bool theMoved = false;
        long thePathCount = theUnit->mWaypoints.size();
        if (thePathCount > 0) {
            while (theUnit->NextWaypoint() == 0) {
                float thePathArg;
                if (theUnit->GetPathStart(theWaypoint, thePathArg) != 0)
                    continue;
                thePoint.mX = theWaypoint.GetX();
                thePoint.mY = theWaypoint.GetY();
                if (theBounds.Contains(thePoint))
                    continue;
                GETransformation theLocation(false);
                theLocation = theUnit->mPhysicalModel.GetLocation();
                theLocation.SetPosition(theWaypoint.GetX(), theWaypoint.GetY(), theWaypoint.mZ);
                theUnit->SetTransformation(theLocation, true);
                theMoved = true;
                break;
            }
        }
        if (theMoved)
            continue;
        EEGetSquareBlockRing(0, theX, theY, theSize, theTiles, false);
        std::sort(theTiles.begin(), theTiles.end(), EETileDistanceLess(theRight, theY));
        unsigned long theTileCount = theTiles.size();
        for (unsigned long j = 0; j < theTileCount; j++) {
            EETile* theTile = gWorld.mMap->GetTile(theTiles[j].mX,
                                                 (*(theTiles.begin() + j)).mY);
            if (theTile == NULL || theTile->mUnits != NULL ||
                !EECanStandOn(theTiles[j], theUnit))
                continue;
            GETransformation theLocation(false);
            theLocation = theUnit->mPhysicalModel.GetLocation();
            float theZ = theLocation.GetZ();
            float theNewX = (float)(theTiles[j]).mX + 0.5f;
            float theNewY = (float)(theTiles[j]).mY + 0.5f;
            if (gWorld.mMap->mTerrainMesh.GetIsValidTile(theTiles[j].mX,
                                                      (*(theTiles.begin() + j)).mY))
                theZ = gWorld.mMap->mTerrainMesh.GetMeshZ(theNewX, theNewY);
            theLocation.SetPosition(theNewX, theNewY, theZ);
            theUnit->SetTransformation(theLocation, true);
            theUnit->DropPath();
            theUnit->StartAction(kActionIdle);
            theUnit->mUnitClass->ActionChanged(theUnit);
        }
    }
}

// FUNCTION: EXE 0x56ab1b
// Sets every resource amount of the player to the same value
void EEPlayer::SetAllResources(long inValue)
{
    for (long i = 2; i < 7; i++)
        this->mResourceCaps[i] = inValue;
}

// FUNCTION: EXE 0x56ab2f
// Sets the player's civilization points (current and maximum) for an epoch
void EEPlayer::SetCivilizationPoints(long inEpoch)
{
    this->mCivilization.SetPointsAndMax(inEpoch);
}

// FUNCTION: EXE 0x56ab41
// Changes the player's stance toward another player, forming or breaking the alliance and its shared sight
void EEPlayer::SetDiplomaticStance(EEPlayer* inOther, long inStance, EEMap* inMap)
{
    EEPlayer* theCurrentPlayer = gWorld.GetCurrentPlayer();
    unsigned long& theStance = this->mEnemies[inOther->mPlayerID];
    if (theStance == inStance)
        return;
    if (this->mSaveVisMap == true && inOther->mSaveVisMap == true) {
        if (inStance == 0) {
            if (inMap != NULL) {
                for (unsigned long i = 0; i < this->mWonderPowers.size(); i++)
                    if (this->mWonderPowers[i] == 5)
                        inOther->RemoveSightSharer(this);
                if (inOther->mEnemies[this->mPlayerID] == 0) {
                    this->ResetWonderTimers(inOther);
                    for (RTSUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext)
                        theUnit->SightOffHere();
                    this->mEvents.HideEvents(this);
                    for (theUnit = inOther->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext)
                        theUnit->SightOffHere();
                    inOther->mEvents.HideEvents(inOther);
                    EEPlayer* thePlayer = inOther;
                    this->mAllies.push_back(thePlayer);
                    thePlayer = this;
                    inOther->mAllies.push_back(thePlayer);
                    theStance = 0;
                    for (theUnit = this->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext)
                        theUnit->SightOnHere();
                    this->mEvents.ShowEvents(this);
                    for (theUnit = inOther->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext)
                        theUnit->SightOnHere();
                    inOther->mEvents.ShowEvents(inOther);
                }
            } else {
                EEPlayer* thePlayer = inOther;
                this->mAllies.push_back(thePlayer);
            }
        } else if (theStance == 0) {
            for (unsigned long j = 0; j < this->mWonderPowers.size(); j++)
                if (this->mWonderPowers[j] == 5)
                    inOther->AddSightSharer(this);
            if (inOther->mEnemies[this->mPlayerID] == 0) {
                for (RTSUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext)
                    theUnit->SightOffHere();
                for (theUnit = inOther->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext)
                    theUnit->SightOffHere();
                this->RemoveAlly(inOther);
                inOther->RemoveAlly(this);
                theStance = inStance;
                for (theUnit = this->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext)
                    theUnit->SightOnHere();
                for (theUnit = inOther->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext)
                    theUnit->SightOnHere();
                if (this->mViewedUnit != NULL && this->mViewedUnit->mPlayer == inOther)
                    this->UpdateSelection(NULL, true);
            }
        }
    }
    if (inOther == theCurrentPlayer || this == theCurrentPlayer)
        theCurrentPlayer->RefreshTerrainVisibility();
    theStance = inStance;
    gWorld.RemoveShareBonus();
    gWorld.ApplyShareBonus();
    if (gMiniMap != NULL)
        gMiniMap->Redraw();
}

// FUNCTION: EXE 0x56addc
// Sets the player's current epoch in its tech tree
void EEPlayer::SetEpoch(long inEpoch)
{
    this->mTechnologies->SetEpoch(inEpoch);
}

// FUNCTION: EXE 0x56adee
// Sets the viewed unit of the selection, moving the reference
void EEPlayer::SetSelectedViewedUnit(RTSUnit* inUnit)
{
    if (this->mSelectedViewedUnit != NULL)
        this->mSelectedViewedUnit->Dereference();
    this->mSelectedViewedUnit = inUnit;
    if (inUnit != NULL)
        inUnit->Reference();
}

// FUNCTION: EXE 0x56ae11
// Sets the viewed unit, moving the reference; with a unit and inFlag the selection is cleared first
void EEPlayer::UpdateSelection(EEUnit* inUnit, bool inFlag)
{
    if (inUnit != NULL && inFlag == true)
        this->mActiveGroup->Clear();
    if (this->mViewedUnit != NULL)
        this->mViewedUnit->Dereference();
    this->mViewedUnit = inUnit;
    if (inUnit != NULL)
        inUnit->Reference();
}

// FUNCTION: EXE 0x56ae52
// Sets the player's override unit class
void EEPlayer::SetHackUnitClass(long inUnitClass)
{
    this->mHackUnitClass = inUnitClass;
}

// FUNCTION: EXE 0x56ae5f
// Turns the player's shared line of sight on or off, recomputing all its units' sight and events
void EEPlayer::SetSharedLOS(bool inFlag, EEMap* inArg)
{
    if (this->mSharedLOS == inFlag)
        return;
    EEUnit* theUnit;
    for (theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
        theUnit->SightOffHere();
    this->mEvents.HideEvents(this);
    this->mSharedLOS = inFlag;
    for (theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
        theUnit->SightOnHere();
    this->mEvents.ShowEvents(this);
}

// FUNCTION: EXE 0x56aec0
// Copies the player's visibility of every tile into the terrain mesh
void EEPlayer::RefreshTerrainVisibility()
{
    EEMap* theMap = gWorld.mMap;
    unsigned long theSize = theMap->mSize;
    for (unsigned long x = 0; x < theSize; x++) {
        for (unsigned long y = 0; y < theSize; y++) {
            RTSTile* theTile = theMap->GetTile(x, y);
            if (theTile)
                theMap->mTerrainMesh.SetTileReferenceCount(x, y, theTile->GetVisibility(this->mPlayerID));
        }
    }
}

// FUNCTION: EXE 0x56af5c
// Sets the player's game result; a new victory or defeat of another player is announced in the chat log
void EEPlayer::SetGameResult(long inState)
{
    GEColor theColor(1.0f, 1.0f, 1.0f);
    UWideString theText;
    UWideString theReason;
    UWideString theTitle;
    UWideString thePlayerName;
    if (inState == 1) {
        if (this->mGameResult == 1)
            goto SaveState;
        gResource->GetString(IDS_PLAYER_IS_VICTORIOUS, theReason);
    } else {
        if (inState != 2 || this->mGameResult == 2)
            goto SaveState;
        gResource->GetString(IDS_PLAYER_HAS_BEEN_DEFEATED, theReason);
    }
    gResource->GetString(IDS_MSG_GAME, theTitle);
    if (this != gWorld.GetCurrentPlayer()) {
        if (this->IsHuman())
            sprintf(theText, theReason, (const char*)this->mName);
        else {
            sprintf(thePlayerName, "%s - (%s)", (const char*)this->mCivilization.mName, (const char*)this->mName);
            sprintf(theText, theReason, (const char*)thePlayerName);
        }
        gUserInterface->AddChatLogEntry(theColor, theTitle, theText, gWorldTable.GetLongValue(25) * 2, 0);
    }
SaveState:
    this->mGameResult = inState;
}

// FUNCTION: EXE 0x56b08a
// Subtracts an amount from one of the player's resources
void EEPlayer::TakeResource(long inIndex, long inAmount)
{
    this->mResourceCaps[inIndex] -= inAmount;
}

// FUNCTION: EXE 0x56b09e
// Pays a technology's cost (scaled by inFraction); with civ power 0x2e resources 5 and 6 can cover each other
void EEPlayer::Pay(EETechnology* inTechnology, long inUnitType, float inFraction)
{
    unsigned long i; bool theInsufficient = false;
    if (this->CanAffordSingleResource(inTechnology, inUnitType, 1) == true) {
        if (inTechnology->mData->mCostResource == 1) {
            for (i = 2; i < 7; i++) {
                long theCost = this->ScaleCost((long)(inTechnology->GetCost(i, inUnitType, 1.0f) * inFraction));
                if (this->mResourceCaps[i] <= theCost) {
                    this->TakeResource(i, theCost);
                    break;
                }
            }
        } else {
            long theCost = this->ScaleCost((long)(inTechnology->GetCost(inTechnology->mData->mCostResource, inUnitType, 1.0f) * inFraction));
            unsigned long theResource = inTechnology->mData->mCostResource;
            if (theCost > this->mResourceCaps[theResource]) {
                if ((theResource == 5 || theResource == 6) && this->mResourceCaps[kResourceGold] + this->mResourceCaps[kResourceIron] >= theCost
                    && gCivPowersManager.HasPower(this, kCivPowerMetallurgy)) {
                    if (this->mResourceCaps[kResourceGold] >= this->mResourceCaps[kResourceIron]) {
                        this->TakeResource(kResourceGold, theCost - this->mResourceCaps[kResourceIron]);
                        this->TakeResource(kResourceIron, this->mResourceCaps[kResourceIron]);
                    } else {
                        this->TakeResource(kResourceIron, theCost - this->mResourceCaps[kResourceGold]);
                        this->TakeResource(kResourceGold, this->mResourceCaps[kResourceGold]);
                    }
                }
            } else
                this->TakeResource(theResource, theCost);
        }
        this->mSavedGroups.Lower(inTechnology->mData->mRequiredStatistic, inTechnology->mData->mRequiredStatisticValue);
    } else {
        bool theOnlyInterchangeable = true;
        for (i = 2; i < 7; i++) {
            long theCost = this->ScaleCost((long)(inTechnology->GetCost(i, inUnitType, 1.0f) * inFraction));
            if (!(theCost > this->mResourceCaps[i]))
                continue;
            theInsufficient = true;
            if (i != 6 && i != 5) {
                theOnlyInterchangeable = false;
                break;
            }
        }
        if (theInsufficient) {
            if (theOnlyInterchangeable && gCivPowersManager.HasPower(this, kCivPowerMetallurgy)) {
                long theFood = this->ScaleCost((long)(inTechnology->GetCost(kResourceGold, inUnitType, 1.0f) * inFraction));
                long theWood = this->ScaleCost((long)(inTechnology->GetCost(kResourceIron, inUnitType, 1.0f) * inFraction));
                long theCost = theFood + theWood;
                if (this->mResourceCaps[kResourceGold] + this->mResourceCaps[kResourceIron] >= theCost) {
                    if (this->mResourceCaps[kResourceGold] >= this->mResourceCaps[kResourceIron]) {
                        this->TakeResource(kResourceGold, theCost - this->mResourceCaps[kResourceIron]);
                        this->TakeResource(kResourceIron, this->mResourceCaps[kResourceIron]);
                    } else {
                        this->TakeResource(kResourceIron, theCost - this->mResourceCaps[kResourceGold]);
                        this->TakeResource(kResourceGold, this->mResourceCaps[kResourceGold]);
                    }
                    for (i = 2; i < 7; i++) {
                        if (i != 5 && i != 6) {
                            long theOtherCost = this->ScaleCost((long)(inTechnology->GetCost(i, inUnitType, 1.0f) * inFraction));
                            this->TakeResource(i, theOtherCost);
                        }
                    }
                }
            }
        } else {
            for (i = 2; i < 7; i++) {
                long theCost = this->ScaleCost((long)(inTechnology->GetCost(i, inUnitType, 1.0f) * inFraction));
                if (theCost > this->mResourceCaps[i])
                    return;
                this->TakeResource(i, theCost);
            }
        }
    }
}

// FUNCTION: EXE 0x56b3f7
// Pays the cost of a unit class improvement at its current level, resources 5 and 6 covering each other
void EEPlayer::PayImprovement(EEUnitClass* inClass, long inImprovement)
{
    if (inClass == NULL)
        return;
    EEUnitClass* theClass = gUnitClassTable[inClass->mUnitType];
    long theTechnology = theClass != NULL ? theClass->mTechnology : 100000;
    if (theTechnology == kTechNone)
        return;
    EETechTreeNode* theNode = this->mTechnologies->Get(theTechnology);
    if (theNode == NULL || inClass->mImprovements == NULL)
        return;
    unsigned long theType = inClass->mUnitType;
    float theScale = inClass->mImprovements->mEntries[inImprovement].mCostFactor;
    theScale *= this->GetImprovementLevel(theType, inImprovement);
    for (unsigned long i = 2; i < 7; i++) {
        long theCost = this->ScaleCost((long)(theNode->GetCost(i, 0, 1.0f) * theScale + 0.0001f));
        if (this->mResourceCaps[i] - theCost < 0) {
            if (i == 6) {
                this->TakeResource(kResourceGold, theCost - this->mResourceCaps[kResourceIron]);
                this->TakeResource(i, this->mResourceCaps[kResourceIron]);
            } else if (i == 5) {
                this->TakeResource(kResourceIron, theCost - this->mResourceCaps[kResourceGold]);
                this->TakeResource(i, this->mResourceCaps[kResourceGold]);
            }
        } else {
            this->TakeResource(i, theCost);
        }
    }
}

// VirtualBC's functor (RTTI EEPSuggestRecomputeAllAreaEffects; vtable 0x442b08: the shared ??_G 0x16b555, its
// Virtual04 0x16b543, the base's empty Virtual08): every area effect of each complex unit recomputed
class EEPSuggestRecomputeAllAreaEffects : public EEUUUnitFunctor {
public:
    virtual void Apply(EEComplexUnit* inUnit) { inUnit->mAreaEffects.MarkDirty(); }
};

// FUNCTION: EXE 0x56b527
// Marks the area effects of every unit of the player for recomputation
void EEPlayer::SuggestRecomputeAllAreaEffects()
{
    EEPSuggestRecomputeAllAreaEffects theFunctor;
    EEForEachUnitOf(this, &theFunctor, false);
}

// FUNCTION: EXE 0x56b543
// Marks a complex unit's area effects for recomputation
// (EEPSuggestRecomputeAllAreaEffects::Apply, inline: emitted after its first user)

// FUNCTION: EXE 0x56b555
// Destroys a unit functor and optionally frees it
// (EEPSuggestRecomputeAllAreaEffects's ??_G, ICF-shared by the unit functors; slot EEPSuggestRecomputeAllAreaEffects+0x0; slot EEUUUnitFunctor+0x0; slot EEHitPointUpgrader+0x0; slot ApplyGeneratedEffectsToOthersFunctor+0x0; slot SetAffectedByAreaEffectFunctor+0x0; slot ClearAreaEffectsFunctor+0x0; slot RebuildAreaEffectRegionListFunctor+0x0; slot RemoveRedundantAreaEffectsFunctor+0x0; slot UpdateGeneratedAreaEffectsFunctor+0x0; slot UpdateAffectedByAreaEffectsFunctor+0x0; stores EEUUUnitFunctor vtable

// FUNCTION: EXE 0x56b572
// Unloads every media item the player loaded and empties the list
void EEPlayer::UnloadAllMedia()
{
    for (long i = this->mLoadedMedia.size() - 1; i >= 0; i--)
        this->mLoadedMedia[i]->UnloadMedia();
    this->mLoadedMedia.clear();
}

// FUNCTION: EXE 0x56b5b6
// Unloads the class media of every unit class recorded as loaded and empties the record
void EEPlayer::UnloadAllClassMedia()
{
    for (unsigned long i = 0; i < this->mSoundLoadedClasses.size(); i++)
        gUnitClassTable[this->mSoundLoadedClasses[i]]->UnloadClassMedia();
    this->mSoundLoadedClasses.clear();
}

// FUNCTION: EXE 0x56b5f9
// Unloads the command buttons of one unit class for the player, if the class index is valid
void EEPlayer::UnloadUnitClassButtons(long inUnitClass)
{
    if ((unsigned long)inUnitClass >= 0x367)
        return;
    EEUnitStats* theStats = this->mUnitClassUpgrades[inUnitClass];
    if (theStats != NULL)
        theStats->UnloadButtons(this);
}

// FUNCTION: EXE 0x56b61c
// Unloads the media of one unit class for the player, if the class index is valid
void EEPlayer::UnloadUnitClassMedia(long inUnitClass)
{
    if ((unsigned long)inUnitClass >= 0x367)
        return;
    EEUnitStats* theStats = this->mUnitClassUpgrades[inUnitClass];
    if (theStats != NULL)
        theStats->UnloadMedia(this);
}

// FUNCTION: EXE 0x56b63f
// Updates the player's four air traffic controllers
void EEPlayer::UpdateAirTraffic()
{
    for (long i = 0; i < 4; i++)
        mAirTraffic[i]->Update();
}

// FUNCTION: EXE 0x56b65a
// Raises (or with inFlag lowers) a unit class improvement and rescales existing units' power, hit points or sight
void EEPlayer::ApplyImprovement(long inUnitType, long inImprovement, bool inFlag)
{
    EEUnitClass* theClass = gUnitClassTable[inUnitType];
    EEUnitClassUpgrade* theUpgrade = this->mUnitClassUpgrades[inUnitType];
    long theOldValue = 0;
    if (theClass == NULL || theUpgrade == NULL)
        return;
    bool theUpdate = true;
    switch (inImprovement) {
    case 8:
        theOldValue = theClass->mFlightTime > 0 ? theUpgrade->mFlightTime : theUpgrade->mPower;
        break;
    case 9:
        theOldValue = theUpgrade->mMaxHitPoints;
        break;
    case 11: {
        for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
            if (theUnit->GetUnitClass()->mUnitType == inUnitType)
                theUnit->GetComplexUnit()->SightOffHere();
        break;
    }
    default:
        theUpdate = false;
        break;
    }
    theUpgrade->ChangeLevel(inImprovement, inFlag ? -1 : 1);
    if (theUpdate != true)
        return;
    switch (inImprovement) {
    case 8:
        if (theOldValue != 0) {
            if (theClass->mFlightTime > 0) {
                for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                    if (theUnit->GetUnitClass()->mUnitType == inUnitType) {
                        EEComplexUnit* theComplex = theUnit->GetComplexUnit();
                        theComplex->SetFlightTime((long)((float)theComplex->mFlightTime / theOldValue * theUpgrade->mFlightTime));
                    }
                }
            } else {
                for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                    if (theUnit->GetUnitClass()->mUnitType == inUnitType) {
                        EEComplexUnit* theComplex = theUnit->GetComplexUnit();
                        theComplex->SetPower((long)((float)theComplex->mPower / theOldValue * theUpgrade->mPower));
                    }
                }
            }
        }
        break;
    case 9:
        if (theOldValue != 0) {
            for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                if (theUnit->GetUnitClass()->mUnitType != inUnitType || !(theUnit->mHitPoints > 0) ||
                    (theUnit->GetComplexUnit() != NULL && theUnit->GetComplexUnit()->mIsBuilt != true))
                    continue;
                float theScaled = (float)theUnit->mHitPoints / theOldValue * theUpgrade->mMaxHitPoints;
                float theHitPoints = (float)UMCeil(theScaled);
                theUnit->SetHitPoints((long)theHitPoints);
            }
        }
        break;
    case 11:
        for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
            if (theUnit->GetUnitClass()->mUnitType == inUnitType)
                theUnit->GetComplexUnit()->SightOnHere();
        break;
    }
}

// FUNCTION: EXE 0x56b88f
// Registers a unit placed in the game: wonder/teleporter counts, town, herd, sight power, walls
void EEPlayer::UnitPlaced(EEComplexUnit* inUnit)
{
    EECivPower thePower;
    if (gLoadingSavedGame || !gGameScreenActive)
        return;
    if (inUnit->GetComplexUnit() != NULL) {
        if (inUnit->mUnitClass->mAIKind == kAIKindHero)
            this->mNumHeroes++;
        if (inUnit->mUnitClass->mUnitType == kUnitTypeTeleporter)
            this->mNumTeleporters++;
        if (!inUnit->IsFoundation())
            this->AddUnitToTown(inUnit->GetComplexUnit());
        this->AddUnitToHerd(inUnit->GetComplexUnit());
        if (gCivPowersManager.HasPower(this, kCivPowerExploration) &&
            (inUnit->mUnitClass->mUnitType == kUnitTypeTownCenter || inUnit->mUnitClass->mUnitType == kUnitTypeCapitol)) {
            gCivPowersManager.GetPower(kCivPowerExploration, thePower);
            this->PulseSight(true, thePower.mAmount, inUnit->mUnitID);
        }
    }
    if (!inUnit->IsFoundation() && (inUnit->mUnitClass->mBlocksTile || inUnit->mUnitClass->mCategory == kUnitCategoryGate))
        gContinentManager.UpdateWallUnit(inUnit, false);
}

// FUNCTION: EXE 0x56b987
// Unregisters a unit leaving the player: counts, wonders, town, herd, construction site, heroes, capitol, walls/forest
// EEUnit::DepleteResource passes (this, 0); inArg is not read. Losing the last capitol re-enables the
// capitol technology and blocks the JIT-manufacturing ones
void EEPlayer::UnitTaken(RTSUnit* inUnit, bool inSwapped)
{
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    this->mPopulationCount -= theUnit->mUnitClass->GetPopulationCost();
    if (!theUnit->mUnitClass->IsLandBuilding() || theUnit->GetComplexUnit() == NULL || theUnit->GetComplexUnit()->mIsBuilt) {
        if (theUnit->mUnitClass->mIsVital
            && (theUnit->GetComplexUnit() == NULL
                || theUnit->GetComplexUnit()->mPreviousPart == NULL
                   && (!theUnit->mUnitClass->IsLandBuilding() || theUnit->GetComplexUnit()->mIsBuilt)))
            this->mEndOfGameCount--;
        EETechTreeNode* theNode = theUnit->GetTechTreeNode();
        if (theNode != NULL && theNode->mData->mKind == 4)
            this->RemoveWonder(theUnit->GetComplexUnit());
        if (theUnit->mUnitClass->IsLandBuilding() && theUnit->mUnitClass->mIsVital) {
            if (--this->mNumVitalBuildings == 0)
                this->DisableCamouflagePower();
        }
    }
    if (theUnit->GetComplexUnit() != NULL && !gWorldClearing)
        this->RemoveUnitFromTown(theUnit->GetComplexUnit());
    if (theUnit->GetComplexUnit() != NULL)
        this->RemoveUnitFromHerd(theUnit->GetComplexUnit());
    if (theUnit->GetComplexUnit() != NULL && theUnit->GetComplexUnit()->mUnitClass->mAIKind == kAIKindHero && this->mNumHeroes > 0)
        this->mNumHeroes--;
    if (theUnit->mUnitClass->mUnitType == kUnitTypeTeleporter)
        this->mNumTeleporters--;
    if (theUnit->mUnitClass->mIsWall && theUnit->mTileNode != NULL && !theUnit->GetComplexUnit()->mIsBuilt)
        this->RemoveConstructionSite(theUnit->GetComplexUnit());
    if (this->GetTechTree()->Get(kTechCitizen)->Contains(theUnit->mUnitClass->GetUnitType())) {
        if (--this->mNumCapitols == 0) {
            if (gUserInterface != NULL)
                gUserInterface->UpdateJITManufacturing();
            this->mTechnologies->Get(kTechCapitol)->mIsAllowed = true;
            if (this->GetTechTree()->Get(kTechSettlement)->GetIsObsolete()) this->GetTechTree()->Get(kTechTownCenter)->mIsAllowed = false; else this->GetTechTree()->Get(kTechSettlement)->mIsAllowed = false;
        }
    }
    if (theUnit->mUnitClass->mUnitType == kUnitTypeHouse)
        this->SuggestRecomputeAllAreaEffects();
    if (!(!gWorldClearing && (theUnit->mUnitClass->mBlocksTile || theUnit->mUnitClass->mCategory == kUnitCategoryGate)))
        return;
    gContinentManager.UpdateWallUnit(theUnit, true);
    if (!theUnit->mUnitClass->mIsTree || !gGameScreenActive)
        return;
    if (theUnit->mResourceAmount != 0)
        theUnit->mResourceAmount = 0;
    gContinentManager.RemoveForestTile(theUnit);
}

#include "ee/EEUnitGoal.h"
#include "ee/EEUnitVectorItem.h"

// FUNCTION: EXE 0x56bba4
// Reattaches a unit to the AI and drops it from its group when it has no further goals or builds
void EEPlayer::UnitGoalDone(EEComplexUnit* inUnit)
{
    EE_LOST_DEBUG(16);  // LOST-DEBUG: none: exact only (lostdebug_search, smallest n)
    if (inUnit->mIsDetachedFromAI)
        inUnit->mIsDetachedFromAI = false;
    if (inUnit->mGoalQueues[0].mGoals.size() <= 1 && inUnit->mCitizenBuildQueue.size() == 0 && inUnit->mActiveGroup != NULL)
        inUnit->mActiveGroup->RemoveUnit(inUnit);
}

// FUNCTION: EXE 0x56bbf8
// Returns whether a unit ID is in the list of IDs loaded into discarded units
bool EEPlayer::HasLoadedUnitID(unsigned long inUnitID)
{
    std::vector<unsigned long>::iterator theIterator = std::find(this->mLoadedUnitIDs.begin(), this->mLoadedUnitIDs.end(), inUnitID);
    return theIterator != this->mLoadedUnitIDs.end();
}

// FUNCTION: EXE 0x56bc55
// Clears the viewed unit and the selection
void EEPlayer::UnselectAllUnits()
{
    this->UpdateSelection(NULL, true);
    this->mActiveGroup->Clear();
}

#include "ee/EEUGPatrol.h"

// FUNCTION: EXE 0x563659
// Sets the patrol group the patrol goal follows, moving the reference
// EEUGPatrol's member, emitted in this unit (out of line among EEPlayer's members)
void EEUGPatrol::SetPatrolGroup(EEPPatrolGroup* inGroup)
{
    if (this->mPatrolGroup != NULL)
        this->mPatrolGroup->Dereference();
    this->mPatrolGroup = inGroup;
    if (inGroup != NULL)
        inGroup->Reference();
}

// FUNCTION: EXE 0x563138
// Adds a town building to a town: a new one, the only fitting one, or the largest fitting one after merging
// the others into it (their patrols move over); returns whether it was added
bool EEPlayer::AddUnitToTown(EEComplexUnit* inUnit)
{
    std::vector<EEPPatrolGroup*> theOldGroups;
    std::vector<EETown*> theTowns;
    bool theResult = false;
    if (EETown::IsTownUnit(inUnit)) {
        inUnit->FindAreaTile(true, 1);
        for (long i = this->mTowns.size() - 1; i >= 0; i--)
            if (this->mTowns[i]->CanTake(inUnit))
                theTowns.push_back(this->mTowns[i]);
        if (theTowns.size() == 0) {
            EETown* theTown = new EETown(this, ++gWorld.mTownIDCounter);
            theTown->mSerial = ++gContinentManager.mLastSerial;
            gContinentManager.AddArea(theTown);
            theResult = theTown->AddUnit(inUnit);
            if (theResult) {
                this->mTownsByID.insert(std::pair<long, EETown*>(theTown->mTownID, theTown));
                this->mTowns.push_back(theTown);
            } else
                delete theTown;
        } else if (theTowns.size() == 1) {
            EETown* theTown = *theTowns.begin();
            theResult = theTown->AddUnit(inUnit);
        } else {
            EETown* theTown = theTowns.back();
            long theSize = theTown->mTiles.Size();
            for (long j = theTowns.size() - 2; j >= 0; j--) {
                if (theSize < theTowns[j]->mTiles.Size()) {
                    theTown = theTowns[j];
                    theSize = theTown->mTiles.Size();
                }
            }
            for (long k = theTowns.size() - 1; k >= 0; k--) {
                if (theTowns[k] == theTown)
                    continue;
                theTown->Merge(theTowns[k]);
                gContinentManager.RemoveArea(theTowns[k]);
                std::vector<EETown*>::iterator theIterator =
                    std::find(this->mTowns.begin(), this->mTowns.end(), theTowns[k]);
                if (theIterator != this->mTowns.end()) {
                    EETown* theOldTown = *theIterator;
                    this->RemoveTownID(theOldTown->mTownID);
                    this->mTowns.erase(theIterator);
                    EEPPatrolGroup* theGroup = theOldTown->mPatrolGroup;
                    if (theGroup != NULL) {
                        EEComplexUnit* theUnit;
                        while ((theUnit = theGroup->GetLast()) != NULL) {
                            theGroup->RemoveUnit(theUnit);
                            theUnit->SetPatrolGroup(NULL);
                            theUnit->Kill(true, false, false);
                            theUnit->mPlayer->KillUnit(theUnit, false);
                        }
                        theGroup->Reference();
                        theOldGroups.push_back(theGroup);
                    }
                    delete theOldTown;
                }
                theTowns[k] = NULL;
            }
            theResult = theTown->AddUnit(inUnit);
            if (theTown->mPatrolGroup != NULL && theOldGroups.size() > 0) {
                for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                    if (theUnit->GetComplexUnit() == NULL)
                        continue;
                    EEUGPatrol* theGoal = dynamic_cast<EEUGPatrol*>(theUnit->GetComplexUnit()->mGoalState);
                    if (theGoal == NULL)
                        continue;
                    EEPPatrolGroup* theGroup = theGoal->mPatrolGroup;
                    if (theGroup != NULL &&
                        std::find(theOldGroups.begin(), theOldGroups.end(), theGroup) != theOldGroups.end())
                        theGoal->SetPatrolGroup(theTown->mPatrolGroup);
                }
                std::for_each(theOldGroups.begin(), theOldGroups.end(), EEPDereferencer());
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x56bc6e
// Sends each enabled unit waiting to repatrol to the nearest point of its nearest town's patrol, then clears the list
void EEPlayer::RestorePatrols()
{
    for (long i = this->mUnitsToRepatrol.size() - 1; i >= 0; i--) {
        if (this->mUnitsToRepatrol[i]->mIsDisabled)
            continue;
        EETown* theTown = this->mUnitsToRepatrol[i]->FindNearestTown();
        if (theTown == NULL || theTown->mPatrolGroup == NULL)
            continue;
        EEComplexUnit* thePoint = theTown->mPatrolGroup->GetNearest(this->mUnitsToRepatrol[i]->mTileX, (*(this->mUnitsToRepatrol.begin() + i))->mTileY);
        if (thePoint == NULL || thePoint->mPatrolGroup == NULL)
            continue;
        EEPPatrolGroup* thePatrol = thePoint->mPatrolGroup;
        EEActiveGroup* theGroup = new EEActiveGroup(this);
        theGroup->Add(*(this->mUnitsToRepatrol.begin() + i));
        EEGroup* theBase = theGroup;
        this->mPendingGroups.push_back(theBase);
        theGroup->KeepMobileUnits();
        EEUGPatrol* theGoal = new EEUGPatrol(thePatrol, thePoint, true);
        theGroup->ClearGoal();
        theGroup->SetGoal(theGoal, false);
        theGroup->mAction = kActionMoveToNextPatrolPoint;
        theGroup->Plan();
    }
    this->ClearUnitsToRepatrol();
}

// FUNCTION: EXE 0x56bdfd
// Gives every town without a patrol group a new one
void EEPlayer::RefreshTownGroups()
{
    for (long i = this->mTowns.size() - 1; i >= 0; i--) {
        EETown* theTown = this->mTowns[i];
        if (theTown != NULL && theTown->mPatrolGroup == NULL)
            theTown->RefreshPatrolGroup();
    }
}

// FUNCTION: EXE 0x56be3f
// Updates the player each tick: validates groups, herds, air traffic, events, improvement timers and wonder healing
// NEAR: inline budget (the original calls begin() / _Ptrit::operator+ out of line for mRunningGroups and the four world-table
// reads), frame slots
void EEPlayer::UpdatePlayer()
{
    std::map<unsigned long, std::pair<float, std::vector<unsigned long> > >::iterator theIterator;
    float theElapsed = gPhysics->mTickTime;
    long theTicks = gPhysics->mTickTimeMs;
    if (theTicks <= 0)
        theTicks = 0;
    if (this->mViewedUnit != NULL && this->mViewedUnit->mReferenceCount == 1)
        this->UpdateSelection(NULL, true);
    if (this->mSelectedViewedUnit != NULL && this->mSelectedViewedUnit->mReferenceCount == 1)
        this->SetSelectedViewedUnit(NULL);
    this->mActiveGroup->Validate();
    this->mSelectedGroup->Validate();
    for (long theGroup = 0; theGroup < 20; theGroup++)
        this->mGroups[theGroup]->Validate();
    unsigned long theCount = this->GetNumRunningGroups();
    for (unsigned long i = 0; i < theCount; i++)
        this->mRunningGroups[i]->Validate();
    for (long theHerd = this->mHerds.size() - 1; theHerd >= 0; theHerd--) {
        unsigned long theTime = gPhysics->mGameTime;
        EEHerd* theHerdObject = this->mHerds[theHerd];
        if (theHerdObject->mNextUpdateTime <= theTime && theHerdObject->mIsActive)
            theHerdObject->Update(theTime);
        if (this->mHerds[theHerd]->mState == 0xb)
            this->KillHerdUnits(this->mHerds[theHerd], 0);
    }
    this->UpdateAirTraffic();
    this->mEvents.TickEvents(this, theElapsed);
    theIterator = this->mQueuedImprovements.begin();
    while (theIterator != this->mQueuedImprovements.end()) {
        if ((*theIterator).second.first != 0.0f) {
            (*theIterator).second.first -= gPhysics->GetTickTime();
            if (this->mInstantBuild && (*theIterator).second.first > 1.0f)
                (*theIterator).second.first = 1.0f;
            if ((*theIterator).second.first > 0.0f) {
                theIterator++;
                continue;
            }
            this->ApplyImprovement((*theIterator).first, (*theIterator).second.second.front(), false);
            (*theIterator).second.second.erase((*theIterator).second.second.begin());
            (*theIterator).second.first = 0.0f;
        }
        if (this->AdvanceImprovement((*theIterator).first))
            theIterator = this->mQueuedImprovements.erase(theIterator);
        else
            theIterator++;
    }
    if (this->mHealsUnits != true)
        return;
    float theTime = this->mHealTimer - theTicks;
    if (theTime <= 0.0f) {
        long theBase = gWorldTable.GetLongValue(37);
        long theFactor = gWorldTable.GetLongValue(37);
        long theEpoch = this->mTechnologies->mCurrentEpoch;
        long theDivisor = gWorldTable.GetLongValue(52);
        long theHeal = theBase + (long)((theEpoch / theDivisor) * gWorldTable.GetFloatValue(51) * theFactor);
        for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            if (this->mHealedUnitSet->Contains(theUnit->mUnitClass->mUnitType) != true)
                continue;
            long theHitPoints = theUnit->mHitPoints + theHeal;
            if (theHitPoints > theUnit->mStats->mMaxHitPoints)
                theHitPoints = theUnit->mStats->mMaxHitPoints;
            if (theUnit->mHitPoints > 0)
                theUnit->SetHitPoints(theHitPoints);
        }
        theTime = kPlayerHealInterval;
    }
    this->mHealTimer = theTime;
}

// FUNCTION: EXE 0x56c1e1
// Restarts the timers of both players' wonders at the current game time
void EEPlayer::ResetWonderTimers(EEPlayer* inPlayer)
{
    std::vector<EEComplexUnit*>::iterator theIterator;
    for (theIterator = inPlayer->mWonders->mItems.begin(); theIterator != inPlayer->mWonders->mItems.end(); ++theIterator)
        (*theIterator)->mCloaked = gPhysics->mGameTime;
    for (theIterator = this->mWonders->mItems.begin(); theIterator != this->mWonders->mItems.end(); ++theIterator)
        (*theIterator)->mCloaked = gPhysics->mGameTime;
}

// FUNCTION: EXE 0x56c249
// Disallows the technologies a map forbids (by map flag; space units off space maps) and sets the selection hotkey
void EEPlayer::ApplyMapRestrictions()
{
    if (gWorld.mPlayers[gWorld.mCurrentPlayer] == this)
        gUIManager->mMainGameForm->RemoveHotKey(kKeyD, kKeyControl, kKeyNone, kKeyNone, 0);
    if ((gWorld.mMap->mMapCharacteristics & kRMMapWaterlessMap) == kRMMapWaterlessMap) {
        this->GetTechTree()->Get(kTechDock)->mIsAllowed = false;
        this->GetTechTree()->Get(kTechNavyYard)->mIsAllowed = false;
        this->GetTechTree()->Get(kTechLighthouseOfAlexandria)->mIsAllowed = false;
        this->GetTechTree()->Get(kTechHurricane)->mIsAllowed = false;
    } else if (gWorld.mPlayers[gWorld.mCurrentPlayer] == this)
        gUIManager->mMainGameForm->AddHotKey(kKeyD, kKeyControl, kKeyNone, kKeyNone, new EEUIEHKUnitSelection(kUnitTypeDock, false, false, false),
                                             NULL, NULL, NULL, 0, false);
    if (!gWorld.mMap->mTerrainMesh.GetIsSpaceMap()) {
        this->GetTechTree()->Get(kTechSpaceDock)->mIsAllowed = false;
        this->GetTechTree()->Get(kTechSpaceTurret)->mIsAllowed = false;
        this->GetTechTree()->Get(kTechMeteorStorm)->mIsAllowed = false;
        this->GetTechTree()->Get(kTechOrbitalSpaceStation)->mIsAllowed = false;
    } else if (gWorld.mPlayers[gWorld.mCurrentPlayer] == this)
        gUIManager->mMainGameForm->AddHotKey(kKeyD, kKeyControl, kKeyNone, kKeyNone, new EEUIEHKUnitSelection(kUnitTypeSpaceDock, false, false, false),
                                             NULL, NULL, NULL, 0, false);
}

// FUNCTION: EXE 0x56c3fc
// Destroys a unit-selection hotkey event and optionally frees it
// (EEUIEHKUnitSelection's ??_G, ee/EEUIEHotKeyEvents.h: emitted after its first user ApplyMapRestrictions; slot EEUIEHKUnitSelection+0x0)

// FUNCTION: EXE 0x56c427
// Recounts the player's living heroes, heroes being researched or queued, and teleporters
void EEPlayer::RecountHeroesAndTeleporters()
{
    this->mNumHeroes = 0;
    this->mNumHeroesInProduction = 0;
    this->mNumTeleporters = 0;
    for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplex = theUnit->GetComplexUnit();
        if (theComplex == NULL || !(theComplex->mHitPoints > 0))
            continue;
        if (theComplex->mUnitClass->mAIKind == kAIKindHero)
            this->mNumHeroes++;
        if (theUnit->mUnitClass->mUnitType == kUnitTypeTeleporter)
            this->mNumTeleporters++;
        if (theComplex->mCurrentResearch != kTechNone) {
            EETechTreeNode* theNode = this->mTechnologies->Get(theComplex->mCurrentResearch);
            if (theNode != NULL) {
                long theType = theNode->mData->mUnitClass;
                if (theType > 0 && gUnitClassTable[theType] != NULL && gUnitClassTable[theType]->mAIKind == kAIKindHero)
                    this->mNumHeroesInProduction++;
            }
        }
        for (long i = theComplex->GetBuildQueueSize() - 1; i >= 0; i--) {
            EETechTreeNode* theNode = this->mTechnologies->Get(theComplex->mBuildQueue[i].mTechnology);
            if (theNode == NULL)
                continue;
            long theType = theNode->mData->mUnitClass;
            if (theType > 0 && gUnitClassTable[theType] != NULL && gUnitClassTable[theType]->mAIKind == kAIKindHero)
                this->mNumHeroesInProduction++;
        }
    }
}

// FUNCTION: EXE 0x56c548
// With the Camouflage civ power, sets the cloak timer of each idle unit (command 5000)
void EEPlayer::EnableCamouflagePower()
{
    if (!gCivPowersManager.HasPower(this, kCivPowerCamouflage)
        || gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet) == NULL)
        return;
    EEUnit* theUnit = this->mFirstUnit;
    EEUnitSet* theSet = gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet);
    for (; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplex = theUnit->GetComplexUnit();
        // The original forwards Contains' bool as a unit-set index, as its paired reset routine does.
        if (theComplex == NULL
            || gUnitSetManager.GetSet(theSet->Contains(theUnit->mUnitClass->mUnitType)) == NULL)
            continue;
        unsigned long theCommand = theComplex->mCommand ? theComplex->mCommand->mType : 0;
        if (theCommand == kActionIdle)
            theComplex->mCamouflageTimer = gWorldTable.GetLongValue(0x55);
    }
}

// FUNCTION: EXE 0x56c5f9
// With the Camouflage civ power, uncloaks every unit and makes it uncloakable, clearing its cloak timers
void EEPlayer::DisableCamouflagePower()
{
    if (!gCivPowersManager.HasPower(this, kCivPowerCamouflage)
        || gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet) == NULL)
        return;
    EEUnit* theUnit = this->mFirstUnit;
    EEUnitSet* theSet = gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet);
    for (; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplexUnit = theUnit->GetComplexUnit();
        if (theComplexUnit == NULL || !theSet->Contains(theUnit->mUnitClass->mUnitType))   // membership (was GetSet(bool))
            continue;
        theComplexUnit->SetCloakableChain(false);
        theComplexUnit->SetCloakedChain(false);
        theComplexUnit->mCamouflageTimer = 0;
        theComplexUnit->mU32C = 0;
    }
}

// FUNCTION: EXE 0x56c6a1
// Sets whether the unit and its following parts can cloak; turning it off also uncloaks them
void EEComplexUnit::SetCloakableChain(bool inValue)
{
    if (this->mIsCloakable != inValue) {
        this->mIsCloakable = inValue;
        if (this->mNextPart != NULL)
            this->mNextPart->SetCloakableChain(inValue);
    }
    if (!inValue)
        this->SetCloakedChain(false);
}

// FUNCTION: EXE 0x56c6db
// Cloaks or uncloaks the unit and its following parts
void EEComplexUnit::SetCloakedChain(bool inValue)
{
    if (this->mIsCloaked == inValue)
        return;
    this->mIsCloaked = inValue;
    if (this->mNextPart != NULL)
        this->mNextPart->SetCloakedChain(inValue);
}

// FUNCTION: EXE 0x56c6fa
// Records a media warning when a unit class's buttons were not preloaded for the player
void EEPlayer::CheckButtonsPreloaded(long inUnitClass)
{
    if (this->mUnitClassUpgrades[inUnitClass]->mButtonLoadCount != 0)
        return;
    UString* theMessage = new UString;
    sprintf(*theMessage, "The unit %s BUTTONS might not have been preloaded for player %d - have a programmer verify this.\n",
            (const char*)gUnitClassTable[inUnitClass]->mName, this->mPlayerID);
    this->mMediaWarnings.push_back(*theMessage);
    delete theMessage;
}

// FUNCTION: EXE 0x56c776
// Records a media warning when a unit class's media was not preloaded for the player (loads nothing)
void EEPlayer::LoadUnitClassMedia(long inUnitClass)
{
    if (this->mUnitClassUpgrades[inUnitClass]->mMediaCount != 0)
        return;
    UString* theMessage = new UString;
    sprintf(*theMessage, "The unit %s might not have been preloaded for player %d - have a programmer verify this.\n",
            (const char*)gUnitClassTable[inUnitClass]->mName, this->mPlayerID);
    this->mMediaWarnings.push_back(*theMessage);
    delete theMessage;
}

// FUNCTION: EXE 0x56c7f2
// Loads the three player graphic effects and adds them to the loaded media
void EEPlayer::LoadPlayerEffects()
{
    for (unsigned char i = 0; i < 3; i++) {
        EEGraphicEffect* theEffect = EEGraphicEffect::Get(sPlayerEffects[i]);
        if (theEffect == NULL)
            continue;
        EEMedia* theMedia = theEffect;
        theMedia->LoadMedia();
        this->AddLoadedMedia(theMedia);
    }
}

// FUNCTION: EXE 0x56c829
// Unloads the three player graphic effects that are in the loaded media
void EEPlayer::UnloadPlayerEffects()
{
    for (unsigned char i = 0; i < 3; i++) {
        EEGraphicEffect* theEffect = EEGraphicEffect::Get(sPlayerEffects[i]);
        if (theEffect == NULL)
            continue;
        EEMedia* theMedia = theEffect;
        if (this->RemoveLoadedMedia(theMedia))
            theMedia->UnloadMedia();
    }
}

// TEMPLATE: EXE 0x56c864
// (map<long, long>::erase(const long&), instantiated by RemoveWonder: mWonderTypeCounts)

// TEMPLATE: EXE 0x56c8b5
// (void vector<EEWUnitsOffMapInfo * >::push_back(EEWUnitsOffMapInfo * const &)    [?push_back@?$vector@PAVEEWUnitsOffMapInfo@@V?$allocator@PAVEEWUnitsOff; instantiated here)

// TEMPLATE: EXE 0x56c8e1
// (void vector<EEMedia * >::push_back(EEMedia * const &)    [?push_back@?$vector@PAVEEMedia@@V?$allocator@PAVEEMedia@@@std@@@std@@QAEXABQAVEEMedia@@@Z]; instantiated here)

// TEMPLATE: EXE 0x56c90d

// TEMPLATE: EXE 0x56c938

// TEMPLATE: EXE 0x56c94c
// (_Tree<longpair<long constEETechTreeNode *>map<longEETechTreeNode * >::_Kfn0>::iterator _Tree<longpair<long constEETechTreeNode *>map<longEETechTreeNod; instantiated here)

// TEMPLATE: EXE 0x56c986
// (void vector<EEPPatrolGroup * >::push_back(EEPPatrolGroup * const &)    [?push_back@?$vector@PAVEEPPatrolGroup@@V?$allocator@PAVEEPPatrolGroup@@@std@@@; instantiated here)

// TEMPLATE: EXE 0x56c9b2
// (pair<longvector<long > > & map<unsigned longpair<longvector<long > >allocator<pair<unsigned long constpair<longvector<long > > > > >::operator[](unsig; instantiated here)

// TEMPLATE: EXE 0x56ca13

// TEMPLATE: EXE 0x56ca3e
// (_Tree<unsigned longpair<unsigned long constpair<longvector<long > > >map<unsigned longpair<longvector<long > >allocator<pair<unsigned long constpair<l; instantiated here)

// TEMPLATE: EXE 0x56ccf6
// (_Tree<unsigned longpair<unsigned long constpair<longvector<long > > >map<unsigned longpair<longvector<long > >allocator<pair<unsigned long constpair<l; instantiated here)

// FUNCTION: EXE 0x56cd30
// Destroys the unit container, releasing every unit it holds
// (UReferenceContainer<EEComplexUnit*>'s dtor, ee/UReferenceContainer.h: Clear() then mItems.clear(); the
// EEAreaEffect twin 0x1ffd23 has the same clear call)

// FUNCTION: EXE 0x56cd51
// Removes a unit from the container, releasing it; returns whether it was there
// (UReferenceContainer<EEComplexUnit*>::Remove, ee/UReferenceContainer.h)

// TEMPLATE: EXE 0x56cd7f
// (UReferenceContainer<EEComplexUnit*>::Replace, ee/UReferenceContainer.h; Virtual9C calls it)

// TEMPLATE: EXE 0x56cdb8

// FUNCTION: EXE 0x56ce29
// Sets the name of a statistics record
// (EEPlayerStatistics::SetName, inline in ee/EEPlayer.h; emitted here for Reset)

// TEMPLATE: EXE 0x56ce44
// (pair<_Tree<unsigned longpair<unsigned long constpair<longvector<long > > >map<unsigned longpair<longvector<long > >allocator<pair<unsigned long constp; instantiated here)

// TEMPLATE: EXE 0x56cefa

// TEMPLATE: EXE 0x56cf6b
// (void _Tree<unsigned longpair<unsigned long constpair<longvector<long > > >map<unsigned longpair<longvector<long > >allocator<pair<unsigned long constp; instantiated here)

// TEMPLATE: EXE 0x56cfad
// (void UReferenceContainer<EEComplexUnit *>::Clear(void)    [?Clear@?$UReferenceContainer@PAVEEComplexUnit@@@@QAEXXZ]; instantiated here)

// FUNCTION: EXE 0x56cfd0
// Erases the unit at an iterator, releasing it; returns false at the end
// (UReferenceContainer<EEComplexUnit*>::Erase, ee/UReferenceContainer.h)

// TEMPLATE: EXE 0x56d009
// (void _Tree<longpair<long constlong>map<longlong >::_Kfn0>::_Erase(_Tree<longpair<long constlong>map<longlong >::_Kfn0>::_Node *)    [?_Erase@?$_Tree@J; instantiated here)

// TEMPLATE: EXE 0x56d03b

// TEMPLATE: EXE 0x56d077

// TEMPLATE: EXE 0x56d0ac
// (_Tree<unsigned longpair<unsigned long constpair<longvector<long > > >map<unsigned longpair<longvector<long > >allocator<pair<unsigned long constpair<l; instantiated here)

// TEMPLATE: EXE 0x56d240
// (_Tree<unsigned longpair<unsigned long constpair<longvector<long > > >map<unsigned longpair<longvector<long > >allocator<pair<unsigned long constpair<l; instantiated here)

// FUNCTION: EXE 0x56d269
// Deletes every town in a vector and empties it; returns the count
// (EEDeletePointers<vector<EETown*> >, a template COMDAT defined at the top of this file)

// FUNCTION: EXE 0x56d299
// Deletes every herd in a vector and empties it; returns the count
// (EEDeletePointers for mHerds's EEHerd* in the original; see the dtor)

// FUNCTION: EXE 0x56d2c9
// Deletes every off-map unit record in a vector and empties it; returns the count
// (EEDeletePointers<vector<EEWUnitsOffMapInfo*> >)

// FUNCTION: EXE 0x56d2f9
// Releases a reference to each patrol group in a range
// (std::for_each<_Ptrit<EEPPatrolGroup*>, EEPDereferencer>)

// TEMPLATE: EXE 0x56d317
// (void __cdecl _Destroy(pair<unsigned long constpair<longvector<long > > > *)    [?_Destroy@std@@YAXPAU?$pair@$$CBKU?$pair@JV?$vector@JV?$allocator@J@st; instantiated here)

// TEMPLATE: EXE 0x56d323
// (void __cdecl _Construct(pair<unsigned long constpair<longvector<long > > > *pair<unsigned long constpair<longvector<long > > > const &)    [?_Construc; instantiated here)

// SYNTHETIC: EXE 0x561825
// SYNTHETIC: EXE 0x56182f
// SYNTHETIC: EXE 0x561840
// SYNTHETIC: EXE 0x56184c
// SYNTHETIC: EXE 0x561868
// SYNTHETIC: EXE 0x561872
// SYNTHETIC: EXE 0x561883
// SYNTHETIC: EXE 0x56188f
// SYNTHETIC: EXE 0x5618ab
// SYNTHETIC: EXE 0x5618b5
// SYNTHETIC: EXE 0x5618c6
// SYNTHETIC: EXE 0x5618d2
// SYNTHETIC: EXE 0x5618ee
// SYNTHETIC: EXE 0x5618f8
// SYNTHETIC: EXE 0x561909
// SYNTHETIC: EXE 0x561915
// SYNTHETIC: EXE 0x561931
// SYNTHETIC: EXE 0x56193b
// SYNTHETIC: EXE 0x56194c
// SYNTHETIC: EXE 0x561958
// SYNTHETIC: EXE 0x561974
// SYNTHETIC: EXE 0x56197e
// SYNTHETIC: EXE 0x56198f
// SYNTHETIC: EXE 0x56199b
// SYNTHETIC: EXE 0x5619b7
// SYNTHETIC: EXE 0x5619c1
// SYNTHETIC: EXE 0x5619d2
// SYNTHETIC: EXE 0x5619de
// SYNTHETIC: EXE 0x5619fa
// SYNTHETIC: EXE 0x561a04
// SYNTHETIC: EXE 0x561a15
// SYNTHETIC: EXE 0x561a21
// SYNTHETIC: EXE 0x561a3d
// SYNTHETIC: EXE 0x561a47
// SYNTHETIC: EXE 0x561a58
// SYNTHETIC: EXE 0x561a64
// SYNTHETIC: EXE 0x561a80
// SYNTHETIC: EXE 0x561a8a
// SYNTHETIC: EXE 0x561a9b
// SYNTHETIC: EXE 0x561aa7
// SYNTHETIC: EXE 0x561ac3
// SYNTHETIC: EXE 0x561acd
// SYNTHETIC: EXE 0x561ade
// SYNTHETIC: EXE 0x561aea
// SYNTHETIC: EXE 0x561b06
// SYNTHETIC: EXE 0x561b10
// SYNTHETIC: EXE 0x561b21
// SYNTHETIC: EXE 0x561b2d
// SYNTHETIC: EXE 0x561b49
// SYNTHETIC: EXE 0x561b53
// SYNTHETIC: EXE 0x561b64
// SYNTHETIC: EXE 0x561b70
// SYNTHETIC: EXE 0x561b8c
// SYNTHETIC: EXE 0x561b96
// SYNTHETIC: EXE 0x561ba7
// SYNTHETIC: EXE 0x561bb3
// SYNTHETIC: EXE 0x561bcf
// SYNTHETIC: EXE 0x561bd9
// SYNTHETIC: EXE 0x561bea
// SYNTHETIC: EXE 0x561bf6
// SYNTHETIC: EXE 0x561c12
// SYNTHETIC: EXE 0x561c1c
// SYNTHETIC: EXE 0x561c2d
// SYNTHETIC: EXE 0x561c39

// EETechTreeNode 0x1cf989 / 0x1cfcc9 call these through slots +0x88 / +0xcc; both slots hold the shared empty
// `ret 4` 0x557269 (ICF), so each has an empty body (this also removes two unresolved virtuals)
void EEPlayer::EpochMediaLoaded(long inEpoch)
{
}

void EEPlayer::EpochMediaUnloaded(long inEpoch)
{
}
