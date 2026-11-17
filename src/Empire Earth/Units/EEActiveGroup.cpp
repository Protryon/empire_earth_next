// C:\EEX\Empire Earth\Units\EEActiveGroup.cpp (file name inferred: no __FILE__). The group the unit commands act on:
// 0x1ebc47..0x1f1f19. One unit: the unit map's split at 0x1ed9d4
// (EEAGIncreasingVelocityUnitSorter.cpp, from the sorter's vtable 0x446d50) is not a unit edge: the sorter's operator()
// 0x1ebc47 opens the unit (as EESGUnitSorter's comparison opens EESelectedGroup.cpp), Fill at 0x1ed9d4 is an
// EEActiveGroup member that passes the sorter by value to its std::sort COMDAT 0x1f07a7, everything up to 0x1efd40 is
// EEActiveGroup's own members, and the std template COMDATs after it (map<long, GE3DPoint>, deque, set<UString>, the
// sorter's sort helpers) close the unit.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <set>
#include <vector>
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
#include "lle/U2DPoint.h"

#include "ee/EEUnitKind.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPASGFlags.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEGroupCluster.h"
#include "ee/EEActiveGroup.h"
#include "ee/EEMap.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEPathManager.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EEUnitStore.h"
#include "ee/EEGroupPathRequest.h"
#include "ee/EEPhysics.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousClient.h"
#include "ee/EEClient.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUGRust.h"
#include "ee/EEUGRust.h"
#include "ee/EEPPatrolGroup.h"
#include "lle/U2DSparseArray.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUnitVectorItem.h"
#include "lle/U2DRectangle.h"
#include "ee/EEFormation.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUnitClassKinds.h"   // EEObj
#include "rust/sim/combat.h"   // ee_units_keeps_ground_attacker, ee_units_ag_*
#include "rust/sim/units.h"
#include "rust/sim/combat.h"   // ee_combat_regions_size
#include "rust/sim/units.h"

// The order Fill sorts a slot's units in (RTTI EEAGIncreasingVelocityUnitSorter; vtable 0x446d50: only its
// user-declared empty virtual dtor, whose ??_G 0x1edaa8 follows Fill, its first user): slower first, the same
// stats by hit points. Defined here like EESelectedGroup.cpp's EESGUnitSorter (ee/EEAGIncreasingVelocityUnitSorter.h is
// an unused draft) (provisional names)
class EEAGIncreasingVelocityUnitSorter {
public:
    virtual ~EEAGIncreasingVelocityUnitSorter() {}
    bool operator()(EEComplexUnit* const& inFirst, EEComplexUnit* const& inSecond) const;
};


// Orders two units slower first; units with the same stats by fewer hit points first
bool EEAGIncreasingVelocityUnitSorter::operator()(EEComplexUnit* const& inFirst, EEComplexUnit* const& inSecond) const
{
    EEUnitClassUpgrade* theFirstStats = inFirst->mStats;
    EEUnitClassUpgrade* theSecondStats = inSecond->mStats;
    bool theResult;
    if (theFirstStats == theSecondStats)
        theResult = inFirst->mHitPoints < inSecond->mHitPoints;
    else if (theFirstStats->GetMaxVelocity() < theSecondStats->GetMaxVelocity())
        theResult = true;
    else
        theResult = false;
    return theResult;
}

// Creates an idle active group for the player with no goal and no units
EEActiveGroup::EEActiveGroup(EEPlayer* inPlayer) : EEGroup(inPlayer)
{
    this->mGoal = NULL;
    this->mAction = kActionIdle;
    this->mIsFinished = false;
    this->mIsQueued = false;
    this->mMovesTogether = false;
    this->mIsSpecialAttack = false;
    EEActiveGroup::ComputeValues();
}

// Adds a unit to the group without taking it out of its old active group
void EEActiveGroup::Add(EEComplexUnit* inUnit)
{
    this->AddMember(inUnit, false);
}

// Destroys the group, releasing its units and its goal and emptying mU48 and mU58
EEActiveGroup::~EEActiveGroup()
{
    this->Clear();
    this->ClearGoal();
    this->mU48.clear();
    this->mU58.clear();
}

// Adds a living unit (not a naval mine) to the group, first taking it out of its old active group when asked,
// and recomputes the group's speeds
void EEActiveGroup::AddMember(EEComplexUnit* inUnit, bool inRemoveFromOld)
{
    EEUnitClass* theClass = inUnit->mUnitClass;
    if (!(inUnit->mHitPoints > 0) || theClass == NULL || theClass->mUnitType == kUnitTypeS10NavalMine)
        return;
    if (inRemoveFromOld) {
        EEGroup* theGroup = inUnit->mPlayer->GetActiveGroupOf(inUnit);
        if (theGroup)
            theGroup->RemoveUnit(inUnit);
    }
    inUnit->SetActiveGroup(this);
    EEGroup::Add(inUnit);
    this->ComputeValues();
}

// Moves the unit to another active group, passing its reference from the old group to the new one
void EEComplexUnit::SetActiveGroup(EEActiveGroup* inGroup)
{
    if (this->mActiveGroup == inGroup)
        return;
    if (this->mActiveGroup)
        this->mActiveGroup->Dereference();
    this->mActiveGroup = inGroup;
    inGroup->Reference();
}

// Requests paths to the goal for all of the group's units
void EEActiveGroup::RequestAllPaths()
{
    this->RequestPaths(this->mUnits);
}

// Requests a path to the goal tile for each cluster of the given units; returns whether any cluster was made
bool EEActiveGroup::RequestPaths(std::vector<EEComplexUnit*>& inUnits)
{
    std::vector<EEGroupCluster*> theClusters;
    GE3DPoint theLocation;
    bool theResult = false;
    EETileCoordinate theTo;
    theTo.mX = 0;
    theTo.mY = 0;
    this->mGoal->GetGoalLocation(theLocation);
    long theY = UMFloor(theLocation.GetY());
    theTo.mX = UMFloor(theLocation.GetX());
    theTo.mY = theY;
    if (inUnits.size() > 0) {
        EEGroup::MakeClusters(inUnits, theClusters, 10, true);
        for (long i = theClusters.size() - 1; i >= 0; i--) {
            EEGroupCluster* theCluster = theClusters[i];
            EEComplexUnit* theUnit = *theCluster->mUnits.begin();
            EETileCoordinate theFrom;
            theFrom.mX = theUnit->GetTileX();
            theFrom.mY = theUnit->GetTileY();
            this->RequestPath(theFrom, theTo, theCluster);
            delete theClusters[i];
            theClusters[i] = NULL;
        }
        theResult = theClusters.size() > 0;
    }
    return theResult;
}

// The formation's placement (ee_units ag.rs lay_out_formation): the aircraft held in a container get the goal
// directly; the formation is centred on the goal (yawed by the location goal's yaw, or towards the unload
// destination) and arranged; the units then take the slots, either the nearest free slot (slower, then larger first:
// the goal is near the units) or by their order across the move direction; every unit gets a goal at its slot.
void EEActiveGroup::LayOutFormation()
{
    ee_units_ag_lay_out_formation(EEObj(this));
}

// Appends a goal to the goal queue and makes the queue's unit its owner
void EEUnitVectorItem::Add(EEUnitGoal* inGoal)
{
    this->mGoals.push_back(inGoal);
    inGoal->SetOwner(this->mUnit);
}

// Returns the first goal in the unit's given goal queue, or NULL when that queue is missing or empty
// EEComplexUnit::GetQueuedGoal (inline in the real header, its out-of-line COMDAT emitted here by its first user
// 0x1ebf85; ee/EEUnit.h declares it out of line, so it is defined here)
EEUnitGoal* EEComplexUnit::GetQueuedGoal(unsigned long inIndex)
{
    if (this->mGoalQueues.size() > inIndex && this->mGoalQueues[inIndex].mGoals.size() > 0)
        return this->mGoalQueues[inIndex].mGoals.front();
    return NULL;
}

// Recomputes the group's average, minimum and maximum speeds, turn speeds and accelerations and their inverses,
// capping each average at the world table's factor 0x31 times the slowest unit's value (ee_units ag.rs)
void EEActiveGroup::ComputeValues()
{
    ee_units_ag_compute_values(EEObj(this));
}

// Refills the group with a selection's valid living units of the given slot kind and takes its formation;
// sort 0 sorts the selection first, sort 1 sorts the group slowest first
void EEActiveGroup::Fill(EESelectedGroup* inGroup, long inSort, long inSlot)
{
    unsigned long theCount = inGroup->GetNumUnits();
    this->ClearGoal();
    this->mU48.clear();
    this->mU58.clear();
    this->Clear();
    this->mAction = kActionIdle;
    this->mGoal = NULL;
    this->SetFormation(inGroup->mFormation);
    if (inSort == 0)
        inGroup->SortUnits();
    // New (R5b.4): the test of the units the group takes (valid, with hit points, in the slot) is EEFillMembers
    // (ee/EEUnitStore.h): their indices in the selection's order; each one is taken here in that order
    std::vector<uint32_t> theTaken(theCount + 1);
    EEComplexUnit* const* theUnits = theCount > 0 ? &inGroup->mUnits[0] : NULL;
    size_t theTakenCount = EEFillMembers(theUnits, theCount, inSlot, &theTaken[0]);
    EEFillCapture(100, inGroup->mPlayer, theUnits, theCount, inSlot, &theTaken[0], theTakenCount);   // TEST ONLY (off by default)
    for (size_t k = 0; k < theTakenCount; k++) {
        EEComplexUnit* theUnit = inGroup->GetUnit((long)theTaken[k]);
        if (theUnit->mActiveGroup)
            theUnit->mActiveGroup->RemoveUnit(theUnit);
        theUnit->SetActiveGroup(this);
        this->Add(theUnit);
    }
    this->ComputeValues();
    switch (inSort) {
    case 1:
        SortBy(EEAGIncreasingVelocityUnitSorter());
        break;
    }
}

// Clears the build targets of every unit in the group
void EEActiveGroup::ResetUnitsPaths()
{
    unsigned long theCount = this->GetNumUnits();
    for (unsigned long i = 0; i < theCount; i++) {
        EEComplexUnit* theUnit = this->GetUnit(i);
        if (theUnit)
            theUnit->ClearBuildTargets();
    }
}

// Marks the group finished: drops its goal and hands it, referenced, to the player's finished groups
void EEActiveGroup::Finish()
{
    this->ClearGoal();
    this->mPlayer->mFinishedGroups.push_back(this);
    this->Reference();
    this->mIsFinished = true;
}

// Returns the group's speed when it moves together, else the unit's own maximum velocity
float EEActiveGroup::GetMaxVelocity(EEComplexUnit* inUnit)
{
    return this->mMovesTogether ? this->mSpeed : inUnit->mStats->GetMaxVelocity();
}

// Returns the group's acceleration when it moves together, else the unit's own
float EEActiveGroup::GetAcceleration(EEComplexUnit* inUnit)
{
    return this->mMovesTogether ? this->mAcceleration : inUnit->mStats->GetAcceleration();
}

// Returns the group's turn speed when it moves together, else the unit's own maximum angular velocity
float EEActiveGroup::GetMaxAngularVelocity(EEComplexUnit* inUnit)
{
    return this->mMovesTogether ? this->mTurnSpeed : inUnit->mStats->GetMaxAngularVelocity();
}

// Returns the group's turn acceleration when it moves together, else the unit's own angular acceleration
float EEActiveGroup::GetAngularAcceleration(EEComplexUnit* inUnit)
{
    return this->mMovesTogether ? this->mTurnAcceleration : inUnit->mStats->mAngularAcceleration;
}

// Returns the group's inverse speed when it moves together, else the unit's own inverse maximum velocity
float EEActiveGroup::GetInverseMaxVelocity(EEComplexUnit* inUnit)
{
    return this->mMovesTogether ? this->mInverseSpeed : inUnit->mStats->mInverseMaxVelocity;
}

// Returns the group's inverse acceleration when it moves together, else the unit's own
float EEActiveGroup::GetInverseAcceleration(EEComplexUnit* inUnit)
{
    return this->mMovesTogether ? this->mInverseAcceleration : inUnit->mStats->mInverseAcceleration;
}

// Returns the group's inverse turn acceleration when it moves together, else the unit's own
float EEActiveGroup::GetInverseAngularAcceleration(EEComplexUnit* inUnit)
{
    return this->mMovesTogether ? this->mInverseTurnAcceleration : inUnit->mStats->mInverseAngularAcceleration;
}

// Sets the group's goal and whether it is queued; the group moves together for a location goal asking for it
void EEActiveGroup::SetGoal(EEUnitGoal* inGoal, bool inFlag)
{
    this->mGoal = inGoal;
    this->mIsQueued = inFlag;
    this->mMovesTogether = inGoal->mKind == kGoalLocation && static_cast<EEUGRust*>(inGoal)->GetUseGroupMovement();
}

// Hands the group's goal to its units by the group's action (drop off, gather, attack, heal, move, explore,
// patrol, disband); aircraft held in a container queue it on both goal queues (ee_units ag.rs plan)
void EEActiveGroup::Plan()
{
    ee_units_ag_plan(EEObj(this));
}

// Empties the group, unlinking every unit from its active group, and resets the group's speeds
void EEActiveGroup::Clear()
{
    unsigned long theCount = this->GetNumUnits();
    for (unsigned long i = 0; i < theCount; i++) {
        EEComplexUnit* theUnit = this->GetUnit(i);
        if (theUnit->mActiveGroup)
            theUnit->mActiveGroup->Dereference();
        theUnit->mActiveGroup = NULL;
    }
    EEGroup::Clear();
    this->ComputeValues();
}

// Removes dead, uncommandable and citizen units from the group
void EEActiveGroup::KeepNonCitizens()
{
    unsigned long i = 0;
    while (i < this->GetNumUnits()) {
        EEComplexUnit* theUnit = this->GetUnit(i);
        // New (R5b.4): the test is Rust over the unit store (ee_units group.rs keeps_non_citizen: commandable, living,
        // not a land or naval citizen)
        if (ee_units_keeps_non_citizen(static_cast<uint32_t>(theUnit->mUnitID)))
            i++;
        else
            this->RemoveUnit(theUnit);
    }
}

// Keeps only commandable, living, built units that can attack: an attack value, an air traffic type, a priest,
// a storm, a balloon or a missile base (ee_units ag.rs)
void EEActiveGroup::KeepAttackers()
{
    ee_units_ag_keep_attackers(EEObj(this));
}

// Keeps the attackers able to hit the target unit (ranged ones only if they see it); the other mobile
// non-priests split off into a group moving to the target's tile (ee_units ag.rs)
void EEActiveGroup::SpreadAroundUnit(EEUnit* inTarget, long inTileX, long inTileY)
{
    ee_units_ag_spread_around_unit(EEObj(this), EEObj(inTarget), (int32_t)inTileX, (int32_t)inTileY);
}

// Removes the units that cannot attack the ground
// New (R5b.4): the test is the unit store's (ee_units group.rs keeps_ground_attacker: the class's mCanAttackGround)
void EEActiveGroup::KeepGroundAttackers()
{
    for (unsigned long theIndex = 0; theIndex < this->GetNumUnits();) {
        EEComplexUnit* theUnit = this->GetUnit(theIndex);
        if (!ee_units_keeps_ground_attacker(static_cast<uint32_t>(theUnit->mUnitID)))
            this->RemoveUnit(theUnit);
        else
            theIndex++;
    }
}

// Keeps only the commandable, living citizens
void EEActiveGroup::KeepCitizens()
{
    unsigned long i = 0;
    while (i < this->GetNumUnits()) {
        EEComplexUnit* theUnit = this->GetUnit(i);
        // New (R5b.4): the test is Rust over the unit store (ee_units group.rs keeps_citizen: commandable, living, a
        // land or naval citizen)
        if (ee_units_keeps_citizen(static_cast<uint32_t>(theUnit->mUnitID)))
            i++;
        else
            this->RemoveUnit(theUnit);
    }
}

// Keeps the citizens; the other mobile units split off to attack the resource (an animal they can attack)
// or into a group moving next to it (ee_units ag.rs)
void EEActiveGroup::SpreadAroundResource(EEUnit* inTarget)
{
    ee_units_ag_spread_around_resource(EEObj(this), EEObj(inTarget));
}

// Keeps the citizens; the other mobile units split off into a group moving to the tile (or beside a building on it)
void EEActiveGroup::SpreadAroundTile(long inTileX, long inTileY)
{
    ee_units_ag_spread_around_tile(EEObj(this), (int32_t)inTileX, (int32_t)inTileY, 0);
}

// Keeps only the commandable, living units that can move (airports included) (ee_units ag.rs)
void EEActiveGroup::KeepMobileUnits()
{
    ee_units_ag_keep_mobile(EEObj(this));
}

// Keeps the transports; the other mobile units split off into a group moving to the tile (or beside a building on it)
void EEActiveGroup::SpreadForUnload(long inTileX, long inTileY)
{
    ee_units_ag_spread_around_tile(EEObj(this), (int32_t)inTileX, (int32_t)inTileY, 1);
}

// Keeps the units able to cast the calamity (special attack, enough power, in the tech node); the other
// living attackers split off into a group attacking the target (ee_units ag.rs)
void EEActiveGroup::SplitCalamityCasters(EECalamityClass* inCalamity, EETechTreeNode* inNode, EEUnit* inTarget)
{
    ee_units_ag_split_calamity_casters(EEObj(this), EEObj(inCalamity), EEObj(inNode), EEObj(inTarget));
}

// Removes a unit from the group, its reserved destination and its path request (cancelling a request left empty)
// Fix: EEGroup::RemoveUnit releases the group's reference, the last one of a dead unit (Validate removes those), and
// the original went on reading the unit (an ASan use-after-free with EE_UNIT_POOL=0, 2026-11-07): held to the end.
void EEActiveGroup::RemoveUnit(EEComplexUnit* inUnit)
{
    inUnit->Reference();
    EEGroup::RemoveUnit(inUnit);
    if (inUnit->mActiveGroup)
        inUnit->mActiveGroup->Dereference();
    inUnit->mActiveGroup = NULL;
    this->mDestinations.erase(inUnit->mUnitID);
    this->ComputeValues();
    if (this->mPathRequests.size() > 0) {
        EEGroupPathRequest* theRequest = this->FindPathRequest(inUnit);
        if (theRequest) {
            theRequest->RemoveUnit(inUnit->mUnitID);
            if (theRequest->mUnitIDs.size() == 0)
                this->CancelPathRequest(theRequest->mPath->mRequestID);
        }
    }
    inUnit->Dereference();
}

// Removes a unit id from the path request's units
void EEGroupPathRequest::RemoveUnit(long inUnitID)
{
    std::vector<long>::iterator it = std::find(this->mUnitIDs.begin(), this->mUnitIDs.end(), inUnitID);
    if (it != this->mUnitIDs.end())
        this->mUnitIDs.erase(it);
}

// Returns whether a location is a reserved destination or within half that unit's inner collision radius of one
bool EEActiveGroup::IsReserved(const GE3DPoint& inLocation)
{
    return ee_units_ag_reserved(EEObj(this), (const float(*)[3])&inLocation.mX, 0) != 0;
}

// Returns whether a location is a reserved destination or in the same unit-region cell as one
bool EEActiveGroup::IsCellTaken(const GE3DPoint& inLocation)
{
    return ee_units_ag_reserved(EEObj(this), (const float(*)[3])&inLocation.mX, 1) != 0;
}

// Records a unit's destination among the group's reserved destinations
void EEActiveGroup::TellGroup(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{
    this->mDestinations[inUnit->mUnitID] = inLocation;
}

// Removes invalid and dead units, then follows the found paths, or finishes the group when it is left empty
void EEActiveGroup::Validate()
{
    unsigned long i = 0;
    while (i < this->GetNumUnits()) {
        if (this->GetUnit(i)->mIsValid && this->GetUnit(i)->mHitPoints > 0)
            i++;
        else
            this->RemoveUnit(this->GetUnit(i));
    }
    if (this->mPathRequests.size() > 0)
        this->MoveAlongFoundPaths();
    else if (this->GetNumUnits() == 0)
        this->Finish();
}

// The active group bodies' host (ee_units ag_host.rs; EE_UNITS_AG_*): the group's and its units' reads, the goals'
// constructors and accessors, and every statement with a world effect, each as the original made it
extern "C" int64_t ee_units_ag_host(int32_t inOp, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD, EeUnitsAgIo* io)
{
    EEActiveGroup* theGroup = (EEActiveGroup*)(uintptr_t)inA;
    EEComplexUnit* theUnit = (EEComplexUnit*)(uintptr_t)inA;
    EEUGRust* theGoal = (EEUGRust*)(uintptr_t)inA;
    EEUnitGoal* theOther = (EEUnitGoal*)(uintptr_t)inB;
    switch (inOp) {
    case EE_UNITS_AG_GROUP: {
        EeUnitsAgGroup& g = io->group;
        g.goal = EEObj(theGroup->mGoal);
        g.goal_kind = theGroup->mGoal != NULL ? (int32_t)theGroup->mGoal->mKind : -1;
        g.action = (int32_t)theGroup->mAction;
        g.count = (uint32_t)theGroup->GetNumUnits();
        g.speed = theGroup->mSpeed;
        g.queued = theGroup->mIsQueued;
        g.moves_together = theGroup->mMovesTogether;
        g.special_attack = theGroup->mIsSpecialAttack;
        return 0;
    }
    case EE_UNITS_AG_GROUP_UNIT:
        return EEObj(theGroup->GetUnit((long)inC));
    case EE_UNITS_AG_REMOVE:
        theGroup->RemoveUnit((EEComplexUnit*)(uintptr_t)inB);
        return 0;
    case EE_UNITS_AG_FINISH:
        theGroup->Finish();
        return 0;
    case EE_UNITS_AG_MOVES_TOGETHER:
        theGroup->mMovesTogether = true;
        return 0;
    case EE_UNITS_AG_REQUEST_ALL:
        theGroup->RequestAllPaths();
        return 0;
    case EE_UNITS_AG_REQUEST_PATHS: {
        const uint64_t* theObjects = (const uint64_t*)io->list;
        std::vector<EEComplexUnit*> theUnits;
        for (int64_t i = 0; i < inC; i++)
            theUnits.push_back((EEComplexUnit*)(uintptr_t)theObjects[i]);
        theGroup->RequestPaths(theUnits);
        return 0;
    }
    case EE_UNITS_AG_SEND:
        EESendGroupCommand(theGroup, (EEUnit*)(uintptr_t)inB, theGroup->mPlayer->GetPlayerID(), (long)inC, 0);
        return 0;
    case EE_UNITS_AG_ARRANGE: {
        EEFormation* theFormation = theGroup->mFormation;
        std::vector<EEFormationSlot> theSlots;
        theFormation->mCenter = GE3DPoint(io->p[0], io->p[1], io->p[2]);
        if (inC) {
            theFormation->mHeading = io->f;
            theFormation->mOrientation.SetOrientationYPR(io->f, 0.0f, 0.0f);
        }
        theFormation->Arrange(theGroup, theSlots);
        for (size_t i = 0; i < theSlots.size(); i++)
            ee_units_ag_push((EeUnitsAgListHandle*)io->list, EEObj(theSlots[i].mUnit),
                             (const float(*)[3])&theSlots[i].mPosition.mX, theSlots[i].mHeading);
        return 0;
    }
    case EE_UNITS_AG_NEW_GROUP:
        return EEObj(new EEActiveGroup(theGroup->mPlayer));
    case EE_UNITS_AG_ADD:
        theGroup->Add((EEComplexUnit*)(uintptr_t)inB);
        return 0;
    case EE_UNITS_AG_SET_FORMATION:
        theGroup->SetFormation(((EEActiveGroup*)(uintptr_t)inB)->mFormation);
        return 0;
    case EE_UNITS_AG_LAUNCH: {
        EEActiveGroup* theNew = (EEActiveGroup*)(uintptr_t)inB;
        theNew->SetGoal((EEUnitGoal*)(uintptr_t)io->a, inC != 0);
        theNew->mAction = (long)io->c;
        theGroup->mPlayer->mPendingGroups.push_back(theNew);
        theNew->Plan();
        EESendGroupCommand(theNew, (EEUnit*)(uintptr_t)io->b, theGroup->mPlayer->GetPlayerID(), (long)io->d, 0);
        if (theNew->GetNumUnits() > 0)
            theGroup->mPlayer->PlayAcknowledgement(theNew->GetUnit(gPhysics->GetRandom(0L, (long)theNew->GetNumUnits() - 1)));
        return 0;
    }
    case EE_UNITS_AG_DESTINATIONS:
        for (std::map<unsigned int, GE3DPoint>::iterator it = theGroup->mDestinations.begin(); it != theGroup->mDestinations.end(); ++it)
            ee_units_ag_push((EeUnitsAgListHandle*)io->list, it->first, (const float(*)[3])&it->second.mX, 0.0f);
        return 0;
    case EE_UNITS_AG_WORLD_SPHERE: {
        EEUnit* theFound = static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)inC));
        if (theFound == NULL)
            return 0;
        io->f = theFound->mUnitClass->GetInnerCollisionSphere();
        return 1;
    }
    case EE_UNITS_AG_REGIONS_SIZE:
        return ee_combat_regions_size(gWorld.mMap->mUnitRegions);
    case EE_UNITS_AG_CAP_FACTOR:
        io->f = gWorldTable.GetFloatValue(kWorldGroupSpeedCapFactor);
        return 0;
    case EE_UNITS_AG_SET_VALUES:
        memcpy(&theGroup->mSpeed, io->list, 17 * sizeof(float));   // mSpeed .. mCapFactor, in their order
        return 0;
    case EE_UNITS_AG_UNIT: {
        EEUnit* u = (EEUnit*)(uintptr_t)inA;
        EeUnitsAgUnit& o = io->unit;
        EEUnitClass* c = u->mUnitClass;
        o.obj = inA;
        o.class_obj = EEObj(c);
        o.complex = EEObj(u->mComplexUnit);
        o.player = EEObj(u->mPlayer);
        o.tile_x = (int32_t)u->GetTileX();
        o.tile_y = (int32_t)u->GetTileY();
        o.hit_points = (int32_t)u->mHitPoints;
        o.resource_amount = (int32_t)u->mResourceAmount;
        o.resource_type = (uint32_t)u->mResourceType;
        o.is_valid = u->mIsValid;
        o.tile_height = u->mTile != NULL ? u->mTile->GetHeight() : 0.0f;
        if (EEComplexUnit* x = u->mComplexUnit) {
            const GE3DPoint& l = x->mPhysicalModel.GetLocation();
            o.goal_state = EEObj(x->mGoalState);
            o.container = EEObj(x->mContainer);
            o.power = (int32_t)x->mPower;
            o.formation_slot = (uint32_t)x->mFormationSlot;
            o.velocity = x->mVelocity;
            o.location[0] = l.mX; o.location[1] = l.mY; o.location[2] = l.mZ;
            o.commandable = x->mIsCommandable;
            o.is_built = x->mIsBuilt;
            o.detached = x->mIsDetachedFromAI;
        }
        if (c != NULL) {
            o.category = (uint32_t)c->mCategory; o.unit_type = (uint32_t)c->mUnitType;
            o.ai_kind = (uint32_t)c->mAIKind; o.family = (uint32_t)c->mFamily;
            o.tile_size = (int32_t)c->mTileSize; o.air_traffic_type = (int32_t)c->mAirTrafficType;
            o.class_max_velocity = c->mMaxVelocity; o.inner_sphere = c->GetInnerCollisionSphere();
            o.can_attack_ground = c->mCanAttackGround; o.paratrooper_bomber = c->mIsParatrooperBomber;
            o.can_patrol = c->mCanPatrol; o.has_special_attack = c->mHasSpecialAttack;
            o.is_priest = c->IsPriest(); o.is_crusader = c->IsCrusader(); o.is_transport = c->IsTransport();
        }
        if (EEUnitClassUpgrade* s = u->mStats) {
            o.max_velocity = s->GetMaxVelocity(); o.acceleration = s->GetAcceleration();
            o.max_angular_velocity = s->GetMaxAngularVelocity(); o.angular_acceleration = s->mAngularAcceleration;
            o.range = s->GetRange(); o.line_of_sight = s->GetLineOfSight(); o.attack = s->mAttack;
        }
        return 0;
    }
    case EE_UNITS_AG_QUEUED_GOAL:
        return EEObj(theUnit->GetQueuedGoal((unsigned long)inC));
    case EE_UNITS_AG_CLEAR_QUEUE:
        theUnit->ClearGoalQueue((unsigned long)inC);
        return 0;
    case EE_UNITS_AG_CLEAR_GOALS:
        theUnit->ClearGoals((long)inC);
        return 0;
    case EE_UNITS_AG_QUEUE_GOAL:
        theUnit->QueueGoal(theOther, (long)inC);
        return 0;
    case EE_UNITS_AG_SET_GOAL:
        theUnit->SetGoal(theOther, 0, (long)inC);
        return 0;
    case EE_UNITS_AG_QUEUE_ADD:
        theUnit->mGoalQueues[(size_t)inC].Add(theOther);
        theUnit->RefreshGoals();
        theUnit->RefreshActiveGoal();
        return 0;
    case EE_UNITS_AG_ADD_TIME:
        theUnit->mGoalQueues[1].AddTime(1);
        return 0;
    case EE_UNITS_AG_ACT:
        theUnit->StartAction((long)inC);
        theUnit->GetUnitClass()->ActionChanged(theUnit);
        return 0;
    case EE_UNITS_AG_SQUADRON:
        if (inC & 2)
            theUnit->QueueSquadronGoals(theOther, (inC & 1) != 0);
        else
            theUnit->SetSquadronGoals(theOther, (inC & 1) != 0);
        return 0;
    case EE_UNITS_AG_CLEAR_GATHERED:
        theUnit->SetGatheredResource(NULL);
        return 0;
    case EE_UNITS_AG_SET_SPECIAL:
        theUnit->mIsSpecialAttack = inC != 0;
        return 0;
    case EE_UNITS_AG_SET_PLACE:
        if (inC >= 0)
            theUnit->mFormationSlot = (unsigned long)inC;
        theUnit->mFormationRank = (long)inD;
        return 0;
    case EE_UNITS_AG_CAN_TAKE:
        return theUnit->CanTake((EEComplexUnit*)(uintptr_t)inB, (unsigned long)inC);
    case EE_UNITS_AG_FOOTPRINT: {
        long theX = (long)io->a, theY = (long)io->b;
        theUnit->GetNearestFootprintTile((EEUnit*)(uintptr_t)inB, &theX, &theY);
        io->a = theX;
        io->b = theY;
        return 0;
    }
    case EE_UNITS_AG_HAS_TILE:
        return gWorld.mMap->GetTile((long)inC, (long)inD) != NULL;
    case EE_UNITS_AG_TILE_AT: {
        EEMapTile* theTile = ::gWorld.mMap->GetTile(UMFloorUnsigned(io->p[0]), UMFloorUnsigned(io->p[1]));
        if (theTile == NULL)
            return 0;
        io->f = theTile->GetHeight();
        io->a = theTile->mTerrainType;
        return 1;
    }
    case EE_UNITS_AG_CAN_GET_TO:
        return gContinentManager.CanGetTo(theUnit, (long)inC, (long)inD);
    case EE_UNITS_AG_CAN_CONVERT: {
        bool theFlag = io->a != 0;
        bool theResult = EECanConvert(theUnit, (EEComplexUnit*)(uintptr_t)inB, theFlag);
        io->a = theFlag;
        return theResult;
    }
    case EE_UNITS_AG_CAN_ATTACK_CLASS:
        return theUnit->CanAttackClass((EEUnitClass*)(uintptr_t)inB);
    case EE_UNITS_AG_HAS_ACTION:
        return ((EEUnitClass*)(uintptr_t)inA)->FindAction((long)inC) != NULL;
    case EE_UNITS_AG_COMMAND_TYPE:
        return (int64_t)theUnit->GetCommandType();
    case EE_UNITS_AG_NUM_CARGO:
        return (int64_t)theUnit->GetComplexUnit()->GetNumCargoUnits();
    case EE_UNITS_AG_POSITION: {
        GE3DPoint thePosition;
        theUnit->GetPosition(thePosition);
        io->p[0] = thePosition.mX; io->p[1] = thePosition.mY; io->p[2] = thePosition.mZ;
        return 0;
    }
    case EE_UNITS_AG_ANGLE: {
        GE3DPoint theFrom(io->p[0], io->p[1], io->p[2]);
        GE3DPoint theTo(io->q[0], io->q[1], io->q[2]);
        io->f = EEGetAngleFromNorth(&theFrom, &theTo);
        return 0;
    }
    case EE_UNITS_AG_TILE_UNITS: {
        EEMapTile* theTile = gWorld.mMap->GetTile((long)inC, (long)inD);
        float theNone[3] = {0.0f, 0.0f, 0.0f};
        for (RTSTileUnitNode* theNode = theTile != NULL ? theTile->mUnits : NULL; theNode; theNode = theNode->mNext)
            ee_units_ag_push((EeUnitsAgListHandle*)io->list, EEObj(theNode->mUnit), &theNone, 0.0f);
        return 0;
    }
    case EE_UNITS_AG_PASSABLE:
        return ((EEUnit*)(uintptr_t)inA)->IsPassableBy((EEComplexUnit*)(uintptr_t)inB);
    case EE_UNITS_AG_SEEN_BY:
        return EEIsSeenBy((EEUnit*)(uintptr_t)inA, (EEPlayer*)(uintptr_t)inB, true);
    case EE_UNITS_AG_PRIEST_CONVERT:
        return EECanPriestConvert(theUnit, (EEComplexUnit*)(uintptr_t)inB);
    case EE_UNITS_AG_CAN_CAST:
        return theUnit->GetPower() >= ((EECalamityClass*)(uintptr_t)inB)->GetCost()
            && ((EETechTreeNode*)(uintptr_t)inC)->Contains(theUnit->mUnitClass->mUnitType);
    case EE_UNITS_AG_REFERENCE:
        theUnit->Reference();
        return 0;
    case EE_UNITS_AG_DEREFERENCE:
        theUnit->Dereference();
        return 0;
    case EE_UNITS_AG_RECT_CLOSEST: {
        U2DRectangle<float> theBounds;
        theBounds.mTop = io->p[0];
        theBounds.mLeft = io->p[1];
        theBounds.mBottom = io->p[2];
        theBounds.mRight = io->q[0];
        U2DPoint<float> theClosest = theBounds.GetClosestPoint(U2DPoint<float>(io->q[1], io->q[2]));
        io->p[0] = theClosest.mX;
        io->p[1] = theClosest.mY;
        return 0;
    }
    case EE_UNITS_AG_GOAL_KIND:
        return theGoal != NULL ? (int64_t)theGoal->mKind : -1;
    case EE_UNITS_AG_CLONE:
        return EEObj(theGoal->Clone());
    case EE_UNITS_AG_DELETE:
        delete theGoal;
        return 0;
    case EE_UNITS_AG_LOCATION_FIELDS:
        io->a = theGoal->GetB1C();
        io->b = theGoal->GetIsForcedMove();
        io->c = theGoal->GetShouldFace();
        io->f = theGoal->GetYaw();
        return 0;
    case EE_UNITS_AG_GOAL_B1C:
        return theGoal->GetB1C();
    case EE_UNITS_AG_GOAL_LOCATION: {
        GE3DPoint theLocation;
        theGoal->GetGoalLocation(theLocation);
        io->p[0] = theLocation.mX; io->p[1] = theLocation.mY; io->p[2] = theLocation.mZ;
        return 0;
    }
    case EE_UNITS_AG_NEW_LOCATION: {
        GE3DPoint theLocation(io->p[0], io->p[1], io->p[2]);
        return EEObj(static_cast<EEUnitGoal*>(EEUGRust::NewLocation(&theLocation, (io->a & 1) != 0, (io->a & 2) != 0,
            (io->a & 4) != 0, io->f, (io->a & 8) != 0, (unsigned char)io->b)));
    }
    case EE_UNITS_AG_NEW_UNLOAD:
        return EEObj(static_cast<EEUnitGoal*>(EEUGRust::NewUnloadTransport(theUnit, (long)inC, (long)inD, true)));
    case EE_UNITS_AG_CAN_UNLOAD_DEST:
        return theGoal->CanUnloadAtDestination();
    case EE_UNITS_AG_UNLOAD_DEST: {
        long theX = 0, theY = 0;
        theGoal->GetUnloadTransport(&theX, &theY);
        io->a = theX;
        io->b = theY;
        return 0;
    }
    case EE_UNITS_AG_SET_B1D:
        theGoal->mB1D = true;
        return 0;
    case EE_UNITS_AG_CLEAR_PATH_STATE:
        theGoal->SetGroupPathState(0);
        return 0;
    case EE_UNITS_AG_GATHER_STORAGE:
        return EEObj(theGoal->GetGatherStorage());
    case EE_UNITS_AG_GATHER_RESOURCE:
        return EEObj(theGoal->GetGatherResource());
    case EE_UNITS_AG_SET_GATHER_STORAGE:
        theGoal->SetGatherStorage((EEUnit*)(uintptr_t)inB);
        return 0;
    case EE_UNITS_AG_ATTACK_TARGET:
        return EEObj(theGoal->GetAttackTarget());
    case EE_UNITS_AG_IS_CONVERT_ATTACK:
        return theGoal->IsConvertAttack();
    case EE_UNITS_AG_CLEAR_ATTACKER:
        theGoal->SetAttacker(NULL);
        return 0;
    case EE_UNITS_AG_HEAL_TARGET:
        return EEObj(theGoal->GetHealTarget());
    case EE_UNITS_AG_SUITS:
        return theGoal->Suits((EEUnit*)(uintptr_t)inB);
    case EE_UNITS_AG_SET_EXPLORE_TILE:
        theGoal->SetExploreTile((long)inC, (long)inD);
        return 0;
    case EE_UNITS_AG_PATROL_POINT:
        return EEObj(theGoal->GetPatrolTarget() ? theGoal->GetPatrolTarget() : theGoal->GetPatrolGroup()->GetLast());
    case EE_UNITS_AG_NEW_ATTACK_UNIT:
        return EEObj(static_cast<EEUnitGoal*>(EEUGRust::NewAttackUnit((EEComplexUnit*)(uintptr_t)inA, (EEUnit*)(uintptr_t)inB,
                                                                       true, false)));
    }
    return 0;
}

// port (save version 0.279, EESaveHasActiveGroups): the players' active groups (mRunningGroups, mPendingGroups), which
// the original did not save: a loaded game had none, so their units moved at their own speeds (not the group's when
// it moves together), took no reserved destinations, and the groups' path requests and finishes were gone. Each group
// is written whole but for its goal, which only Plan, LayOutFormation and RequestPaths read (when the command made the
// group): the loaded group has none. Its running path requests wait for their graph searches (EEAwaitAutoPathRequest)
#include <memory>
#include <functional>
#include <stddef.h>
#include "lle/FSFile.h"
#include "ee/EESaveGameFiles.h"
#include "rust/sim/combat.h"
#include "rust/sim/units.h"

namespace {
struct EESavedGroupRequest {
    long mKey = 0;
    std::vector<long> mUnitIDs;
    EECPASGFlags mFlags;
    long mL30 = 0;
    EEAutoPathRequest* mRequest = NULL;     // the load's (one reference)
};

struct EESavedActiveGroup {
    std::vector<std::pair<long, bool> > mUnits;   // the id, whether the unit's mActiveGroup is this group
    unsigned long mFormationType = 8;       // 8: none
    float mOrientation[16] = {};
    GE3DPoint mCenter;
    float mHeading = 0.0f;
    bool mB5C = true;
    std::vector<std::pair<long, long> > mTypeCounts;
    long mKindFlags = 0, mAction = 0;
    std::vector<std::pair<unsigned int, GE3DPoint> > mDestinations;
    std::vector<GE3DPoint> mU48, mU58;
    float mValues[17] = {};                 // mSpeed .. mInverseTurnAcceleration, mCapFactor
    bool mFlags[4] = {false, false, false, false};
    std::vector<EESavedGroupRequest> mRequests;
};

typedef std::vector<std::vector<EESavedActiveGroup> > EESavedPlayerGroups;   // per player: running, then pending

static_assert(sizeof(GETransformation) == 16 * sizeof(float), "GETransformation");

float* EEGroupValues(EEActiveGroup* inGroup)
{
    return &inGroup->mSpeed;   // the 17 floats mSpeed .. mCapFactor, in their order
}

void EEWritePoint(FSFile& outFile, const GE3DPoint& inPoint)
{
    outFile.Write(inPoint.mX);
    outFile.Write(inPoint.mY);
    outFile.Write(inPoint.mZ);
}

long EEReadPoint(FSFile& inFile, GE3DPoint& outPoint)
{
    long theResult = inFile.Read(outPoint.mX);
    if (theResult == 0)
        theResult = inFile.Read(outPoint.mY);
    if (theResult == 0)
        theResult = inFile.Read(outPoint.mZ);
    return theResult;
}

void EEReleaseSaved(std::vector<EESavedActiveGroup>& ioGroups)
{
    for (size_t i = 0; i < ioGroups.size(); i++)
        for (size_t r = 0; r < ioGroups[i].mRequests.size(); r++)
            if (ioGroups[i].mRequests[r].mRequest != NULL) {
                ioGroups[i].mRequests[r].mRequest->Dereference();
                ioGroups[i].mRequests[r].mRequest = NULL;
            }
}

void EESaveActiveGroup(FSFile& outFile, EEActiveGroup* inGroup)
{
    outFile.Write((unsigned long)inGroup->mUnits.size());
    for (size_t i = 0; i < inGroup->mUnits.size(); i++) {
        outFile.Write((long)inGroup->mUnits[i]->mUnitID);
        outFile.Write(inGroup->mUnits[i]->GetActiveGroup() == inGroup);
    }
    EEFormation* theFormation = inGroup->mFormation;
    outFile.Write(theFormation != NULL ? (unsigned long)theFormation->mType : 8UL);
    if (theFormation != NULL) {
        float theMatrix[16];
        memcpy(theMatrix, &theFormation->mOrientation, sizeof(theMatrix));
        for (int f = 0; f < 16; f++)
            outFile.Write(theMatrix[f]);
        EEWritePoint(outFile, theFormation->mCenter);
        outFile.Write(theFormation->mHeading);
        outFile.Write(theFormation->mB5C);
    }
    outFile.Write((unsigned long)inGroup->mUnitTypeCounts.size());
    for (std::map<long, long>::iterator it = inGroup->mUnitTypeCounts.begin(); it != inGroup->mUnitTypeCounts.end(); it++) {
        outFile.Write(it->first);
        outFile.Write(it->second);
    }
    outFile.Write(inGroup->mKindFlags);
    outFile.Write(inGroup->mAction);
    outFile.Write((unsigned long)inGroup->mDestinations.size());
    for (std::map<unsigned int, GE3DPoint>::iterator it = inGroup->mDestinations.begin(); it != inGroup->mDestinations.end(); it++) {
        outFile.Write((unsigned long)it->first);
        EEWritePoint(outFile, it->second);
    }
    const std::vector<GE3DPoint>* theLists[2] = {&inGroup->mU48, &inGroup->mU58};
    for (const std::vector<GE3DPoint>* theList : theLists) {
        outFile.Write((unsigned long)theList->size());
        for (size_t i = 0; i < theList->size(); i++)
            EEWritePoint(outFile, (*theList)[i]);
    }
    const float* theValues = EEGroupValues(inGroup);
    for (int v = 0; v < 17; v++)
        outFile.Write(theValues[v]);
    outFile.Write(inGroup->mIsFinished);
    outFile.Write(inGroup->mIsQueued);
    outFile.Write(inGroup->mMovesTogether);
    outFile.Write(inGroup->mIsSpecialAttack);
    unsigned long theCount = 0;
    std::map<long, EEGroupPathRequest*>::iterator it;
    for (it = inGroup->mPathRequests.begin(); it != inGroup->mPathRequests.end(); it++)
        if (it->second->mPath != NULL)
            theCount++;
    outFile.Write(theCount);
    for (it = inGroup->mPathRequests.begin(); it != inGroup->mPathRequests.end(); it++) {
        EEGroupPathRequest* theRequest = it->second;
        if (theRequest->mPath == NULL)
            continue;
        outFile.Write(it->first);
        outFile.Write((unsigned long)theRequest->mUnitIDs.size());
        for (size_t i = 0; i < theRequest->mUnitIDs.size(); i++)
            outFile.Write(theRequest->mUnitIDs[i]);
        outFile.Write(theRequest->mFlags.mPlayerID);
        for (int f = 0; f < 17; f++)
            outFile.Write(theRequest->mFlags.mFlags[f]);
        outFile.Write(theRequest->mL30);
        EESaveAutoPathRequest(outFile, *theRequest->mPath);
    }
}

long EELoadActiveGroup(FSFile& inFile, EESavedActiveGroup& g)
{
    unsigned long theCount = 0;
    long theResult = inFile.Read(theCount);
    for (unsigned long i = 0; i < theCount && theResult == 0; i++) {
        long theID = 0;
        bool theMember = false;
        theResult = inFile.Read(theID);
        if (theResult == 0)
            theResult = inFile.Read(theMember);
        g.mUnits.push_back(std::make_pair(theID, theMember));
    }
    if (theResult == 0)
        theResult = inFile.Read(g.mFormationType);
    if (theResult == 0 && g.mFormationType != 8) {
        for (int f = 0; f < 16 && theResult == 0; f++)
            theResult = inFile.Read(g.mOrientation[f]);
        if (theResult == 0)
            theResult = EEReadPoint(inFile, g.mCenter);
        if (theResult == 0)
            theResult = inFile.Read(g.mHeading);
        if (theResult == 0)
            theResult = inFile.Read(g.mB5C);
    }
    if (theResult == 0)
        theResult = inFile.Read(theCount);
    for (unsigned long i = 0; i < theCount && theResult == 0; i++) {
        long theType = 0, theNumber = 0;
        theResult = inFile.Read(theType);
        if (theResult == 0)
            theResult = inFile.Read(theNumber);
        g.mTypeCounts.push_back(std::make_pair(theType, theNumber));
    }
    if (theResult == 0)
        theResult = inFile.Read(g.mKindFlags);
    if (theResult == 0)
        theResult = inFile.Read(g.mAction);
    if (theResult == 0)
        theResult = inFile.Read(theCount);
    for (unsigned long i = 0; i < theCount && theResult == 0; i++) {
        unsigned long theID = 0;
        GE3DPoint thePoint;
        theResult = inFile.Read(theID);
        if (theResult == 0)
            theResult = EEReadPoint(inFile, thePoint);
        g.mDestinations.push_back(std::make_pair((unsigned int)theID, thePoint));
    }
    std::vector<GE3DPoint>* theLists[2] = {&g.mU48, &g.mU58};
    for (std::vector<GE3DPoint>* theList : theLists) {
        if (theResult == 0)
            theResult = inFile.Read(theCount);
        for (unsigned long i = 0; i < theCount && theResult == 0; i++) {
            GE3DPoint thePoint;
            theResult = EEReadPoint(inFile, thePoint);
            theList->push_back(thePoint);
        }
    }
    for (int v = 0; v < 17 && theResult == 0; v++)
        theResult = inFile.Read(g.mValues[v]);
    for (int f = 0; f < 4 && theResult == 0; f++)
        theResult = inFile.Read(g.mFlags[f]);
    if (theResult == 0)
        theResult = inFile.Read(theCount);
    for (unsigned long i = 0; i < theCount && theResult == 0; i++) {
        g.mRequests.push_back(EESavedGroupRequest());
        EESavedGroupRequest& r = g.mRequests.back();
        unsigned long theIDs = 0;
        theResult = inFile.Read(r.mKey);
        if (theResult == 0)
            theResult = inFile.Read(theIDs);
        for (unsigned long u = 0; u < theIDs && theResult == 0; u++) {
            long theID = 0;
            theResult = inFile.Read(theID);
            r.mUnitIDs.push_back(theID);
        }
        if (theResult == 0)
            theResult = inFile.Read(r.mFlags.mPlayerID);
        for (int f = 0; f < 17 && theResult == 0; f++)
            theResult = inFile.Read(r.mFlags.mFlags[f]);
        if (theResult == 0)
            theResult = inFile.Read(r.mL30);
        if (theResult == 0)
            theResult = EELoadAutoPathRequest(inFile, r.mRequest);
    }
    return theResult;
}

EEActiveGroup* EERestoreActiveGroup(EEPlayer* inPlayer, EESavedActiveGroup& g)
{
    EEActiveGroup* theGroup = new EEActiveGroup(inPlayer);
    for (size_t i = 0; i < g.mUnits.size(); i++) {
        EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit(g.mUnits[i].first)));
        if (theUnit == NULL)
            continue;
        theGroup->EEGroup::Add(theUnit);
        if (g.mUnits[i].second)
            theUnit->SetActiveGroup(theGroup);
    }
    EEFormation* theFormation = g.mFormationType != 8 ? EEFormation::Create(g.mFormationType) : NULL;
    if (theFormation != NULL) {
        memcpy(&theFormation->mOrientation, g.mOrientation, sizeof(g.mOrientation));
        theFormation->mCenter = g.mCenter;
        theFormation->mHeading = g.mHeading;
        theFormation->mB5C = g.mB5C;
    }
    if (theGroup->mFormation != NULL)
        theGroup->mFormation->Dereference();
    theGroup->mFormation = theFormation;    // (Create's reference)
    theGroup->mUnitTypeCounts.clear();
    for (size_t i = 0; i < g.mTypeCounts.size(); i++)
        theGroup->mUnitTypeCounts[g.mTypeCounts[i].first] = g.mTypeCounts[i].second;
    theGroup->mKindFlags = g.mKindFlags;
    theGroup->mAction = g.mAction;
    for (size_t i = 0; i < g.mDestinations.size(); i++)
        theGroup->mDestinations[g.mDestinations[i].first] = g.mDestinations[i].second;
    theGroup->mU48 = g.mU48;
    theGroup->mU58 = g.mU58;
    memcpy(EEGroupValues(theGroup), g.mValues, sizeof(g.mValues));
    theGroup->mIsFinished = g.mFlags[0];
    theGroup->mIsQueued = g.mFlags[1];
    theGroup->mMovesTogether = g.mFlags[2];
    theGroup->mIsSpecialAttack = g.mFlags[3];
    for (size_t i = 0; i < g.mRequests.size(); i++) {
        EESavedGroupRequest& r = g.mRequests[i];
        EEAutoPathRequest* thePath = r.mRequest;
        r.mRequest = NULL;
        EEGroupPathRequest* theRequest = new EEGroupPathRequest(thePath, r.mUnitIDs, r.mL30);
        thePath->Dereference();   // (the group request's own reference keeps it)
        theRequest->mFlags = r.mFlags;
        long theKey = r.mKey;
        theGroup->mPathRequests[theKey] = theRequest;
        if (thePath->mIsDone)
            continue;
        // its graph search: the restored request takes the loaded copy's place, else the request is dropped
        theGroup->Reference();
        EEAwaitAutoPathRequest(thePath->mRequestID, [theGroup, theKey](EEAutoPathRequest* inRequest) {
            std::map<long, EEGroupPathRequest*>::iterator it = theGroup->mPathRequests.find(theKey);
            if (it != theGroup->mPathRequests.end()) {
                if (inRequest != NULL) {
                    it->second->mPath->Dereference();
                    it->second->mPath = inRequest;   // the owner's reference
                    inRequest = NULL;
                } else {
                    delete it->second;
                    theGroup->mPathRequests.erase(it);
                }
            }
            if (inRequest != NULL)
                inRequest->Dereference();
            theGroup->Dereference();
        });
    }
    return theGroup;
}
}   // namespace

void EESaveActiveGroups(FSFile& outFile)
{
    outFile.Write((unsigned long)gWorld.mNumPlayers);
    for (unsigned long p = 0; p < gWorld.mNumPlayers; p++) {
        EEPlayer* thePlayer = gWorld.mPlayers[p];
        for (int l = 0; l < 2; l++) {
            std::vector<EEActiveGroup*> theGroups;
            if (thePlayer != NULL) {
                std::vector<EEGroup*>& theList = l == 0 ? thePlayer->mRunningGroups : thePlayer->mPendingGroups;
                for (size_t i = 0; i < theList.size(); i++)
                    if (EEActiveGroup* theGroup = dynamic_cast<EEActiveGroup*>(theList[i]))
                        theGroups.push_back(theGroup);
            }
            outFile.Write((unsigned long)theGroups.size());
            for (size_t i = 0; i < theGroups.size(); i++)
                EESaveActiveGroup(outFile, theGroups[i]);
        }
    }
}

long EELoadActiveGroups(FSFile& inFile)
{
    std::shared_ptr<EESavedPlayerGroups> theLists = std::make_shared<EESavedPlayerGroups>();
    unsigned long theCount = 0;
    long theResult = inFile.Read(theCount);
    for (unsigned long p = 0; p < theCount * 2 && theResult == 0; p++) {
        theLists->push_back(std::vector<EESavedActiveGroup>());
        unsigned long theGroups = 0;
        theResult = inFile.Read(theGroups);
        for (unsigned long i = 0; i < theGroups && theResult == 0; i++) {
            theLists->back().push_back(EESavedActiveGroup());
            theResult = EELoadActiveGroup(inFile, theLists->back().back());
        }
    }
    if (theResult != 0) {
        for (size_t l = 0; l < theLists->size(); l++)
            EEReleaseSaved((*theLists)[l]);
        return theResult;
    }
    // at the first tick (the units are back), before the graph searches are resolved (EEAfterLoadLast)
    EEAfterLoad([theLists]() {
        for (size_t l = 0; l < theLists->size(); l++) {
            unsigned long p = (unsigned long)(l / 2);
            EEPlayer* thePlayer = p < gWorld.mNumPlayers ? gWorld.mPlayers[p] : NULL;
            std::vector<EESavedActiveGroup>& theSaved = (*theLists)[l];
            if (thePlayer == NULL) {
                EEReleaseSaved(theSaved);
                continue;
            }
            for (size_t i = 0; i < theSaved.size(); i++) {
                EEActiveGroup* theGroup = EERestoreActiveGroup(thePlayer, theSaved[i]);
                (l % 2 == 0 ? thePlayer->mRunningGroups : thePlayer->mPendingGroups).push_back(theGroup);
            }
        }
    });
    return theResult;
}
