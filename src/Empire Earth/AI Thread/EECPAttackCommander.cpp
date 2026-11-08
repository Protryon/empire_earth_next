// C:\EEX\Empire Earth\AI Thread\EECPAttackCommander.cpp (file name inferred: no __FILE__; RTTI classes
// EECPAttackCommander, vtable 0x43bc10, and EECPUnitRequirement, vtable 0x43bc18). A computer player's attack
// commander: 0x5198e..0x529e7 (EECPASTargetUnit.cpp ends at 0x5198e; EECPAttackFormation's ctor starts at 0x529e7).
// The commander's members, EECPUnitRequirement's (its ctor, Clear and ??_G after CreateGroup, their first user; copy ctor
// and operator= among the vector<EECPUnitRequirement> COMDATs), then the template COMDATs (deque resize, vector
// push_back/insert, deque push_front, vector insert(P, N, X) and its helpers)
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAGTask.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EECPUnitRequirement.h"
#include "ee/EECPCombatStrategy.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPTrace.h"
#include "ee/EETechTreeTechID.h"

extern long& gNumPlayers;                    // 0x530e0c (EECPAttackSubGroupUnit.cpp declares it too) (provisional name)

// FUNCTION: EXE 0x45198e
// Creates the computer player's attack commander with no groups and 35 group updates per tick
EECPAttackCommander::EECPAttackCommander(EEComputerPlayer* inPlayer, EECPPersonnel* inPersonnel,
    EECPIntelligence* inIntelligence)
    : mPlayer(inPlayer), mPersonnel(inPersonnel)
{
    this->mIntelligence = inIntelligence;
    this->mNextDropTime = 0;
    this->mL28 = 0;
    this->mMaxUpdatesPerTick = 0x23;
    this->mL38 = 0;
    this->mUpdatesThisTick = 0;
    this->mL34 = 0;
}

// SYNTHETIC: EXE 0x4519d9
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4519f5
// Destroys the commander, deleting every attack group
EECPAttackCommander::~EECPAttackCommander()
{
    for (long i = this->mGroups.size() - 1; i >= 0; i--)
        delete this->mGroups[i];
}

// FUNCTION: EXE 0x451a45
// Offers a replacement unit to each group's sub-groups in turn; returns true once one takes it
bool EECPAttackCommander::Offer(EEComplexUnit* inOld, EEComplexUnit* inUnit)
{
    bool theResult = false;
    for (long i = this->mGroups.size() - 1; i >= 0; i--) {
        theResult = this->mGroups[i]->JoinSubGroup(inOld, inUnit);
        if (theResult)
            break;
    }
    return theResult;
}

// FUNCTION: EXE 0x451a8a
// Places a unit (or its container) in a sub-group of a group on its continent, else in a new group added to
// the commander; returns false when it has no continent, the player observes, or no sub-group takes it
// NEAR: the original keeps theContinent at [ebp-4] and this at [ebp-0xc] (frame 0x2c), theResult in bl; ours
// keeps theContinent in ebx and spills theResult (frame 0x28)
bool EECPAttackCommander::TakeUnit(EEComplexUnit* inUnit)
{
    EEAIContinent* theContinent = gContinentManager.GetContinentAt(
        inUnit->mContainer != NULL ? inUnit->mContainer : inUnit);
    bool theResult = false;
    if (theContinent == NULL || this->mPlayer->mObserverMode)
        return theResult;
    for (long i = this->mGroups.size() - 1; i >= 0; i--) {
        if (this->mGroups[i]->mContinentID != theContinent->mID || !(this->mGroups[i]->GetNumSubGroups() > 0))
            continue;
        theResult = this->mGroups[i]->PlaceInSubGroup(inUnit);
        if (theResult)
            break;
    }
    if (theResult)
        return theResult;
    EECPAttackGroup* theGroup = new EECPAttackGroup(this);
    theGroup->mContinentID = theContinent->mID;
    theResult = theGroup->PlaceInSubGroup(inUnit);
    if (theResult)
        this->mGroups.push_back(theGroup);
    else
        delete theGroup;
    return theResult;
}

// FUNCTION: EXE 0x451bb9
// Creates an attack group for a task with a default strategy of the given kind (special plus counter units, or
// already-defined units for kind 3); keeps it unless the plan leaves it spent, then returns NULL
// the experiment "prepeace" (docs/AI_RESEARCH.md §8.27; the owner: "I don't focus at all on attacks" before the Stone
// Age): no attack group against an enemy in the Prehistoric; the defence commander's groups still form
static bool EECPACPrehistoricPeace(EEComputerPlayer* inPlayer, EECPAGTask& inTask)
{
    // (and military.stone_attacks false, docs/AI_RESEARCH.md §8.29: through the Stone Age as well)
    if (inPlayer->mTechnologies == NULL || inTask.mMode == kAGModeDefense)
        return false;
    long theEpoch = inPlayer->mTechnologies->mCurrentEpoch;
    bool thePeace = theEpoch <= kTechEpochPrehistoric ? EECPPrehistoricPeaceOn(inPlayer)
        : theEpoch == kTechEpochStone && EECPStonePeaceOn(inPlayer);
    if (!thePeace)
        return false;
    EEPlayer* theTarget = inTask.GetPlayer();
    return theTarget != NULL && theTarget != inPlayer && !inPlayer->IsAlly(theTarget);
}

EECPAttackGroup* EECPAttackCommander::CreateGroup(EECPAGTask& inTask, long inKind)
{
    if (EECPACPrehistoricPeace(this->mPlayer, inTask))
        return NULL;
    EECPCombatStrategy theStrategy;
    EECPUnitRequirement theRequirement;
    std::vector<long> theUnitIDs;
    EECPAttackGroup* theGroup = new EECPAttackGroup(this);
    theGroup->mTask = inTask;
    theStrategy.mKind = inKind;
    switch (inKind) {
    case 0:
    case 1:
    case 2:
        theStrategy.Reset();
        theStrategy.mAddFavorites = true;
        theStrategy.mKind = inKind;
        theRequirement.Clear();
        theRequirement.mKind = kRequirementSpecial;
        theStrategy.mRequirements.push_back(theRequirement);
        theRequirement.Clear();
        theRequirement.mKind = kRequirementCounter;
        break;
    case 3:
        theStrategy.Reset();
        theStrategy.mAddFavorites = true;
        theStrategy.mKind = inKind;
        theRequirement.Clear();
        theRequirement.mKind = kRequirementAlreadyDefined;
        break;
    default:
        theStrategy.Reset();
        theStrategy.mAddFavorites = true;
        theStrategy.mKind = inKind;
        theRequirement.Clear();
        theRequirement.mKind = kRequirementSpecial;
        theStrategy.mRequirements.push_back(theRequirement);
        theRequirement.Clear();
        theRequirement.mKind = kRequirementCounter;
        break;
    }
    theStrategy.mRequirements.push_back(theRequirement);
    theGroup->MakePlan(theStrategy, theUnitIDs);
    bool theKept = !theGroup->IsSpent();
    EE_CP_TRACE(this->mPlayer, "attack.group", "target=%ld type=%ld mode=%ld x=%ld y=%ld kind=%ld units=%lu kept=%d",
        inTask.GetPlayerID(), inTask.mType, inTask.mMode, (long)inTask.mLocation.mTile.mX, (long)inTask.mLocation.mTile.mY,
        inKind, (unsigned long)theUnitIDs.size(), theKept ? 1 : 0);
    if (theKept)
        this->mGroups.push_back(theGroup);
    else {
        delete theGroup;
        theGroup = NULL;
    }
    return theGroup;
}

// FUNCTION: EXE 0x451d19
// Creates an empty counter-unit requirement with nothing wanted
EECPUnitRequirement::EECPUnitRequirement()
    : mL04(-1), mL08(-1), mNumLeft(0), mNumWanted(0), mCategory(0x23), mUnitClass(0), mFamily(0x17), mUnitKind(0), mKind(kRequirementCounter)
{
}

// FUNCTION: EXE 0x451d49
// Resets the requirement to an empty counter-unit requirement with nothing wanted
void EECPUnitRequirement::Clear()
{
    this->mL04 = -1;
    this->mL08 = -1;
    this->mCategory = 0x23;
    this->mUnitClass = 0;
    this->mFamily = 0x17;
    this->mUnitKind = 0;
    this->mKind = kRequirementCounter;
    this->mNumLeft = 0;
    this->mNumWanted = 0;
}

// SYNTHETIC: EXE 0x451d71
// (EECPUnitRequirement's scalar deleting destructor; the empty dtor is inline in ee/EECPUnitRequirement.h)

// FUNCTION: EXE 0x451d8e
// Creates an attack group for a task from a copy of a combat strategy, collecting the planned unit ids; keeps it
// unless the plan leaves it spent, then returns NULL
EECPAttackGroup* EECPAttackCommander::CreateGroupFromStrategy(EECPAGTask& inTask, const EECPCombatStrategy& inStrategy,
    std::vector<long>& outUnitIDs)
{
    if (EECPACPrehistoricPeace(this->mPlayer, inTask))
        return NULL;
    EECPCombatStrategy theStrategy(inStrategy);
    EECPAttackGroup* theGroup = new EECPAttackGroup(this);
    theGroup->mTask = inTask;
    theGroup->MakePlan(theStrategy, outUnitIDs);
    EE_CP_TRACE(this->mPlayer, "attack.group", "target=%ld type=%ld mode=%ld x=%ld y=%ld kind=strategy units=%lu kept=%d",
        inTask.GetPlayerID(), inTask.mType, inTask.mMode, (long)inTask.mLocation.mTile.mX, (long)inTask.mLocation.mTile.mY,
        (unsigned long)outUnitIDs.size(), theGroup->IsSpent() ? 0 : 1);
    if (!theGroup->IsSpent())
        this->mGroups.push_back(theGroup);
    else {
        delete theGroup;
        theGroup = NULL;
    }
    return theGroup;
}

// FUNCTION: EXE 0x451e14
// Tells every group about a change to a target player, then drops spent groups
void EECPAttackCommander::TargetPlayerNotice(EEPlayer* inPlayer, bool inFlag)
{
    for (long i = this->mGroups.size() - 1; i >= 0; i--)
        this->mGroups[i]->TargetPlayerNotice(inPlayer, inFlag);
    this->DropSpentGroups(this->mNextDropTime);
}

// FUNCTION: EXE 0x451e61
// Deletes every spent group and schedules the next drop 45 seconds later
void EECPAttackCommander::DropSpentGroups(unsigned long inTime)
{
    std::deque<EECPAttackGroup*>::iterator theIterator = this->mGroups.begin();
    while (theIterator != this->mGroups.end()) {
        if ((*theIterator)->IsSpent()) {
            EECPAttackGroup* theGroup = *theIterator;
            theIterator = this->mGroups.erase(theIterator, theIterator + 1);  // Dinkum erase(P), inline (include/dinkum/deque lacks it)
            delete theGroup;
        } else
            ++theIterator;
    }
    this->mNextDropTime = inTime + 45000;
}

// FUNCTION: EXE 0x451ef0
// Empties the group list, then deletes every group it held
void EECPAttackCommander::DeleteGroups()
{
    std::deque<EECPAttackGroup*> theGroups;
    theGroups.insert(theGroups.begin(), this->mGroups.begin(), this->mGroups.end());
    this->mGroups.clear();
    for (long i = theGroups.size() - 1; i >= 0; i--)
        delete theGroups[i];
    theGroups.clear();
}

// FUNCTION: EXE 0x451f7f
// Returns the sub-group holding a unit, or NULL
EECPAttackSubGroup* EECPAttackCommander::FindHolder(EEUnit* inUnit)
{
    EECPAttackSubGroup* theResult = NULL;
    for (long i = this->mGroups.size() - 1; i >= 0 && theResult == NULL; i--)
        theResult = this->mGroups[i]->FindHolder(inUnit);
    return theResult;
}

// FUNCTION: EXE 0x451fc0
// Appends every sub-group listing a unit to a list; returns the list's size
unsigned long EECPAttackCommander::ListUnit(EEComplexUnit* inUnit, std::vector<EECPAttackSubGroup*>& outGroups)
{
    for (long i = this->mGroups.size() - 1; i >= 0; i--)
        this->mGroups[i]->ListUnit(inUnit, outGroups);
    return outGroups.size();
}

// FUNCTION: EXE 0x452015
// Returns the sub-group with an id, or NULL
EECPAttackSubGroup* EECPAttackCommander::GetSubGroup(unsigned long inID)
{
    EECPAttackSubGroup* theResult = NULL;
    for (long i = this->mGroups.size() - 1; i >= 0 && theResult == NULL; i--)
        theResult = this->mGroups[i]->GetSubGroup(inID);
    return theResult;
}

// FUNCTION: EXE 0x452056
// Returns the player's combat manager
EECPCombatManager* EECPAttackCommander::GetCombatManager()
{
    return this->mPlayer->mCombatManager;
}

// FUNCTION: EXE 0x452060
// Appends the units of every group with no continent (defence groups) to a list
void EECPAttackCommander::CollectDefendingUnits(std::vector<EEComplexUnit*>& inUnits)
{
    for (long i = this->mGroups.size() - 1; i >= 0; i--)
        if (this->mGroups[i]->mContinentID == 0)
            this->mGroups[i]->CollectUnits(inUnits);
}

// FUNCTION: EXE 0x4520bd
// Returns true if any group keeps a unit
bool EECPAttackCommander::KeepsUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    for (long i = this->mGroups.size() - 1; i >= 0; i--) {
        theResult = this->mGroups[i]->KeepsUnit(inUnit);
        if (theResult)
            break;
    }
    return theResult;
}

// FUNCTION: EXE 0x4520ff
// Reads the groups (count, then each group) and a trailing value from the saved game; returns the first read
// error (groups are still created after one)
long EECPAttackCommander::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theCount = 0;
    long theResult = inFile.Read(theCount);
    if (theResult == kErrorOK) {
        this->mGroups.resize(theCount);
        for (unsigned long i = 0; i < theCount; i++) {
            this->mGroups[i] = new EECPAttackGroup(this);
            if (theResult == kErrorOK)
                theResult = this->mGroups[i]->Load(inMajorVersion, inMinorVersion, inFile);
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mL28);
    }
    return theResult;
}

// FUNCTION: EXE 0x4521d1
// Updates every group
void EECPAttackCommander::UpdateGroups()
{
    for (long i = this->mGroups.size() - 1; i >= 0; i--)
        this->mGroups[i]->Update();
}

// FUNCTION: EXE 0x45220a
// Replaces each swapped-out unit by its new unit in every group
void EECPAttackCommander::UnitsSwapped(std::vector<EEPUnitSwap>& ioSwaps)
{
    const std::vector<EEPUnitSwap>& theSwaps = ioSwaps;
    for (long i = this->mGroups.size() - 1; i >= 0; i--) {
        for (long j = theSwaps.size() - 1; j >= 0; j--)
            this->mGroups[i]->ReplaceUnit(theSwaps[j].mOld, theSwaps[j].mNew);
    }
}

// FUNCTION: EXE 0x452283
// Releases a unit from the first continent group holding it; returns true if one did
bool EECPAttackCommander::ReleaseFromAreaGroups(EEComplexUnit* inUnit)
{
    bool theResult = false;
    for (long i = this->mGroups.size() - 1; i >= 0; i--) {
        if (this->mGroups[i]->mContinentID == 0)
            continue;
        theResult = this->mGroups[i]->ReleaseUnit(inUnit);
        if (theResult)
            break;
    }
    return theResult;
}

// FUNCTION: EXE 0x4522ec
// Restores every group after loading; returns the first error
long EECPAttackCommander::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < this->mGroups.size(); i++)
        if (theResult == kErrorOK)
            theResult = this->mGroups[i]->Restore(inMajorVersion, inMinorVersion);
    return theResult;
}

// FUNCTION: EXE 0x452337
// Writes the groups (count, then each group) and a trailing value to the saved game; returns the first write error
long EECPAttackCommander::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theCount = this->mGroups.size();
    long theResult = outFile.Write(theCount);
    if (theResult != kErrorOK)
        return theResult;
    for (unsigned long i = 0; i < this->mGroups.size(); i++)
        if (theResult == kErrorOK)
            theResult = this->mGroups[i]->Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mL28);
    return theResult;
}

// FUNCTION: EXE 0x4523b0
// Removes a unit from the first group holding it; returns true if one did
bool EECPAttackCommander::RemoveUnit(EEUnit* inUnit)
{
    bool theResult = false;
    for (long i = this->mGroups.size() - 1; i >= 0; i--) {
        theResult = this->mGroups[i]->RemoveUnit(inUnit);
        if (theResult)
            break;
    }
    return theResult;
}

// FUNCTION: EXE 0x4523f2
// Ticks groups round-robin until the per-tick update budget is used up (at least size / players groups), then
// drops spent groups when due, or at once if the player has no units or observes
void EECPAttackCommander::Update(unsigned long inTime)
{
    EECPAttackGroup* theGroup = NULL;
    long i = 0;
    long theCount = this->mGroups.size();
    long theLimit = this->mGroups.size() / gNumPlayers;
    this->mUpdatesThisTick = 0;
    for (; i < theCount && (this->mUpdatesThisTick < this->mMaxUpdatesPerTick || i < theLimit); i++) {
        theGroup = this->mGroups.front();
        theGroup->Tick(inTime);
        this->mGroups.push_back(theGroup);
        this->mGroups.pop_front();
    }
    if (this->mPlayer->mFirstUnit == NULL || this->mPlayer->mObserverMode || this->mNextDropTime < inTime)
        this->DropSpentGroups(inTime);
}

// TEMPLATE: EXE 0x452497
// (deque<EECPAttackGroup*>::resize, from Load)

// TEMPLATE: EXE 0x4524dc
// (vector<EECPUnitRequirement>::push_back, from CreateGroup)

// TEMPLATE: EXE 0x452508
// (vector<EECPUnitRequirement>::insert(P, X))

// TEMPLATE: EXE 0x452562
// (deque<EECPAttackGroup*>::push_front)

// TEMPLATE: EXE 0x4525df
// (vector<EECPUnitRequirement>::insert(P, N, X))

// FUNCTION: EXE 0x4528b1
// Creates a copy of a unit requirement
EECPUnitRequirement::EECPUnitRequirement(const EECPUnitRequirement& inOther)
{
    this->mL04 = inOther.mL04;
    this->mL08 = inOther.mL08;
    this->mUnitClass = inOther.mUnitClass;
    this->mCategory = inOther.mCategory;
    this->mFamily = inOther.mFamily;
    this->mKind = inOther.mKind;
    this->mNumLeft = inOther.mNumLeft;
    this->mNumWanted = inOther.mNumWanted;
    this->mUnitKind = inOther.mUnitKind;
}

// TEMPLATE: EXE 0x4528f6
// (vector<EECPUnitRequirement>::_Ufill)

// TEMPLATE: EXE 0x452921
// (vector<EECPUnitRequirement>::_Ucopy)

// TEMPLATE: EXE 0x45294d
// (std::copy over EECPUnitRequirement)

// FUNCTION: EXE 0x45296a
// Copies a unit requirement
EECPUnitRequirement& EECPUnitRequirement::operator=(const EECPUnitRequirement& inOther)
{
    this->mL04 = inOther.mL04;
    this->mL08 = inOther.mL08;
    this->mUnitClass = inOther.mUnitClass;
    this->mCategory = inOther.mCategory;
    this->mFamily = inOther.mFamily;
    this->mKind = inOther.mKind;
    this->mNumLeft = inOther.mNumLeft;
    this->mNumWanted = inOther.mNumWanted;
    this->mUnitKind = inOther.mUnitKind;
    return *this;
}

// TEMPLATE: EXE 0x4529a9
// (std::copy_backward over EECPUnitRequirement)

// TEMPLATE: EXE 0x4529d5
// (std::_Construct(EECPUnitRequirement*, const EECPUnitRequirement&))

// Template and inline COMDATs this unit's code instantiates (the bodies come from the headers; each was compared
// against the original by name)
// TEMPLATE: EXE 0x451cf8
