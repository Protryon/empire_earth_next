// New (port): the world calls a Rust decision makes (rust/crates/ee_units/src/host.rs; docs/RUST_PLAN.md R5b.4
// leaf 5): a group's units as ids, goals built from a plain record (units by id, resolved through the registry) and
// handed back as opaque handles. Every call runs on the thread that called the decision (the physics thread, under
// the world's write lock). TEST ONLY: the dry host (ee/EEUnitHost.h) answers the same calls without acting.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <vector>
#include <algorithm>
#include <map>
#include <initializer_list>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGExplore.h"
#include "ee/EEUGAttackGround.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGGather.h"
#include "ee/EEUGHealUnit.h"
#include "ee/EEUGPatrol.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGUnloadTransport.h"
#include "ee/EEUnitAction.h"
#include "ee/EEUnitClassIDs.h"
#include "ee/EEPPatrolGroup.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIArea.h"
#include "ee/EETown.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousClient.h"
#include "ee/EEClient.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEActiveGroup.h"
#include "ee/EEPhysics.h"
#include "ee/EEUnitHandle.h"
#include "ee/EEUnitStore.h"
#include "ee/EEUnitHost.h"
#include "ee/EEUnitKind.h"
#include "rust/ee_rust.h"

static EEUnit* UnitOf(uint32_t inID)
{
    EEUnit* theUnit;
    EEUnitHandleResolve(inID, theUnit);
    return theUnit;
}

static EEComplexUnit* ComplexOf(uint32_t inID)
{
    EEComplexUnit* theUnit;
    EEUnitHandleResolve(inID, theUnit);
    return theUnit;
}

// ---- the dry host (TEST ONLY; ee/EEUnitHost.h) ----

namespace {

// the dry log's events (the words after each code are its values)
enum DryEvent {
    kDryNewGroup = 1,     // tag
    kDryFill,             // tag, selection tag, sort, slot, the count, the ids
    kDryRelease,          // tag
    kDryPending,          // player id, tag
    kDryKeepMobile,       // tag, the count, the ids kept
    kDryKeepGround,       // tag, the count, the ids kept
    kDryClearGoal,        // tag
    kDryGoalNew,          // ctor, unit a, unit b, the point's three bits, flag a, flag b, the route (2 words), made
    kDryGoalRelease,      // ctor
    kDrySetGoal,          // tag, ctor, queued
    kDrySetAction,        // tag, action
    kDryPlan,             // tag
    kDryRandom,           // low, high, the answer
    kDryAcknowledge,      // player id, tag, index, the unit at it
    kDryMapHeight,        // x, y, the height (bits)
    kDrySendCommand,      // tag, target, player id, command, queue
    kDryAcknowledgeUnit,  // player id, unit
    kDryTownPoint,        // unit, found, the point
    kDryAddUnitTile,      // unit, target, player id, the target's tile x, y
    kDryRoute,            // unit, the route (2 words)
    kDryUnitType,         // unit, its class's type
    kDryQueued,           // tag, queued
    kDrySpreadResource,   // tag, target
    kDrySpreadTile,       // tag, x, y
    kDryAddBuildTarget,   // unit, target
    kDryOrderRepair,      // unit, target
    kDryAddMember,        // tag, unit
    kDryKeepNonCitizens,  // tag, the count, the ids kept
    kDryLonePatrol,       // player id, x, y
    kDryInsertPoint,      // unit, player id, x, y
    kDryGoalRoute,        // unit, the route (2 words)
    kDryAddPoint,         // the route (2 words), player id, x, y
    kDryGoalAddPoint,     // ctor, player id, x, y
    kDrySpreadUnit,       // tag, target, x, y
    kDryComplexOf,        // unit, its complex unit
    kDryPathPoint,        // unit, x, y in, x, y out, the height (bits)
    kDryResetPaths,       // tag
    kDryKeepCitizens,     // tag, the count, the ids kept
    kDryWallPlan,         // player id, technology, the start's and end's bits (the dry plan makes 2 pieces)
    kDryWallBuild,        // tag
    kDryWallRelease,      // -
    kDrySpreadUnload,     // tag, x, y
    kDryCanUnload,        // unit, the answer
    kDrySetTransport,     // ctor, unit
    kDryCanUnloadThere,   // ctor (the dry answer: yes)
    kDryUnloadWarning     // player id, x, y
};

struct DryGroup {
    uint32_t mTag;
    std::vector<EEComplexUnit*> mUnits;
};

struct DryGoal {
    uint32_t mCtor;
    uint8_t mKind;
};

struct DryState {
    std::vector<DryGroup*> mGroups;
    std::vector<DryGoal*> mGoals;
    std::vector<uint32_t> mLog;
    unsigned long mMismatches;
    uint32_t mDraws;
};

// the dry state of this thread (the sweep's); NULL: the game's host
thread_local DryState* sDry = NULL;

void Log(std::initializer_list<uint32_t> inWords)
{
    sDry->mLog.insert(sDry->mLog.end(), inWords.begin(), inWords.end());
}

void LogUnits(const std::vector<EEComplexUnit*>& inUnits)
{
    sDry->mLog.push_back((uint32_t)inUnits.size());
    for (size_t i = 0; i < inUnits.size(); i++)
        sDry->mLog.push_back(inUnits[i] != NULL ? static_cast<uint32_t>(inUnits[i]->mUnitID) : 0xffffffffu);
}

uint32_t Bits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

// a route as the log names it: its points' count + 1 (0: none) and its first point's id (heap addresses differ
// between runs)
void LogRoute(void* inRoute)
{
    EEPPatrolGroup* theRoute = static_cast<EEPPatrolGroup*>(inRoute);
    if (theRoute == NULL) {
        Log({0, 0xffffffffu});
        return;
    }
    uint32_t theFirst = theRoute->GetNumPoints() > 0 && theRoute->mPatrolPoints[0] != NULL
        ? static_cast<uint32_t>(theRoute->mPatrolPoints[0]->mUnitID) : 0xffffffffu;
    Log({(uint32_t)theRoute->GetNumPoints() + 1, theFirst});
}

uint32_t PlayerID(void* inPlayer)
{
    return static_cast<uint32_t>(static_cast<EEPlayer*>(inPlayer)->mPlayerID);
}

DryGroup* Dry(void* inGroup)
{
    return static_cast<DryGroup*>(inGroup);
}

// EEGroup::RemoveUnit over a dry group: its first place
void DryRemove(DryGroup* inGroup, EEComplexUnit* inUnit)
{
    std::vector<EEComplexUnit*>::iterator it = std::find(inGroup->mUnits.begin(), inGroup->mUnits.end(), inUnit);
    if (it != inGroup->mUnits.end())
        inGroup->mUnits.erase(it);
}

DryGroup* NewDryGroup()
{
    DryGroup* theGroup = new DryGroup();
    theGroup->mTag = (uint32_t)sDry->mGroups.size();
    sDry->mGroups.push_back(theGroup);
    return theGroup;
}

size_t DryGroupIDs(void* inGroup, uint32_t* outIDs, size_t inCapacity)
{
    DryGroup* theGroup = Dry(inGroup);
    for (size_t i = 0; i < theGroup->mUnits.size() && i < inCapacity; i++)
        outIDs[i] = static_cast<uint32_t>(theGroup->mUnits[i]->mUnitID);
    return theGroup->mUnits.size();
}

void* DryGoalNew(const EeGoalSpec* inSpec)
{
    uint8_t theKind = 0;
    bool theMade = true;
    switch (inSpec->ctor) {
    case EE_GOAL_CTOR_EXPLORE: theKind = kGoalExplore; break;
    case EE_GOAL_CTOR_ATTACK_GROUND: theKind = kGoalAttackGround; break;
    case EE_GOAL_CTOR_ATTACK_UNIT: theKind = kGoalAttackUnit; theMade = UnitOf(inSpec->unit_b) != NULL; break;
    case EE_GOAL_CTOR_GATHER: theKind = kGoalGather; break;
    case EE_GOAL_CTOR_HEAL_UNIT: theKind = kGoalHealUnit; theMade = ComplexOf(inSpec->unit_a) != NULL; break;
    case EE_GOAL_CTOR_PATROL:
    case EE_GOAL_CTOR_PATROL_ROUTE: theKind = kGoalPatrol; break;
    case EE_GOAL_CTOR_LOCATION: theKind = kGoalLocation; break;
    case EE_GOAL_CTOR_LOCATION_XY: theKind = kGoalLocation; break;
    case EE_GOAL_CTOR_UNLOAD_TRANSPORT: theKind = kGoalUnloadTransport; theMade = ComplexOf(inSpec->unit_a) != NULL; break;
    default: theMade = false; break;
    }
    Log({kDryGoalNew, inSpec->ctor, inSpec->unit_a, inSpec->unit_b, Bits(inSpec->point[0]), Bits(inSpec->point[1]),
         Bits(inSpec->point[2]), inSpec->flag_a, inSpec->flag_b});
    if (inSpec->ctor == EE_GOAL_CTOR_LOCATION)   // (the other constructors' records keep their words)
        Log({Bits(inSpec->value), inSpec->flag_c});
    LogRoute(inSpec->object);
    Log({theMade ? 1u : 0u});
    if (!theMade)
        return NULL;
    DryGoal* theGoal = new DryGoal();
    theGoal->mCtor = inSpec->ctor;
    theGoal->mKind = theKind;
    sDry->mGoals.push_back(theGoal);
    return theGoal;
}

void DryFill(void* inGroup, void* inSelection, int32_t inSort, int32_t inSlot)
{
    DryGroup* theGroup = Dry(inGroup);
    DryGroup* theSelection = Dry(inSelection);
    // EEActiveGroup::Fill: the units EEFillMembers takes, in the selection's order (the dry host sorts neither the
    // selection nor the group: the sort is logged)
    theGroup->mUnits.clear();
    size_t theCount = theSelection->mUnits.size();
    std::vector<uint32_t> theTaken(theCount + 1);
    size_t theTakenCount = EEFillMembers(theCount > 0 ? &theSelection->mUnits[0] : NULL, theCount, inSlot, &theTaken[0]);
    for (size_t k = 0; k < theTakenCount; k++)
        theGroup->mUnits.push_back(theSelection->mUnits[theTaken[k]]);
    Log({kDryFill, theGroup->mTag, theSelection->mTag, (uint32_t)inSort, (uint32_t)inSlot});
    LogUnits(theGroup->mUnits);
}

void DryKeepMobile(void* inGroup)
{
    // EEActiveGroup::KeepMobileUnits' loop and test over the objects
    DryGroup* theGroup = Dry(inGroup);
    unsigned long i = 0;
    while (i < theGroup->mUnits.size()) {
        EEComplexUnit* theUnit = theGroup->mUnits[i];
        if (theUnit->mIsCommandable && theUnit->mHitPoints > 0
            && (theUnit->mStats->GetMaxVelocity() != 0.0f || theUnit->mUnitClass->mUnitType == kUnitTypeAirport))
            i++;
        else
            DryRemove(theGroup, theUnit);
    }
    Log({kDryKeepMobile, theGroup->mTag});
    LogUnits(theGroup->mUnits);
}

void DryKeepGroundAttackers(void* inGroup)
{
    // EEActiveGroup::KeepGroundAttackers' loop over the store's test, compared with the object's
    DryGroup* theGroup = Dry(inGroup);
    for (unsigned long theIndex = 0; theIndex < theGroup->mUnits.size();) {
        EEComplexUnit* theUnit = theGroup->mUnits[theIndex];
        bool theKeeps = ee_units_keeps_ground_attacker(static_cast<uint32_t>(theUnit->mUnitID));
        if (theKeeps != theUnit->mUnitClass->mCanAttackGround)
            sDry->mMismatches++;
        if (!theKeeps)
            DryRemove(theGroup, theUnit);
        else
            theIndex++;
    }
    Log({kDryKeepGround, theGroup->mTag});
    LogUnits(theGroup->mUnits);
}

void DryKeepNonCitizens(void* inGroup)
{
    // EEActiveGroup::KeepNonCitizens' loop over the store's test, compared with the object's
    DryGroup* theGroup = Dry(inGroup);
    for (unsigned long theIndex = 0; theIndex < theGroup->mUnits.size();) {
        EEComplexUnit* theUnit = theGroup->mUnits[theIndex];
        bool theKeeps = ee_units_keeps_non_citizen(static_cast<uint32_t>(theUnit->mUnitID));
        bool theObject = theUnit->mIsCommandable && theUnit->mHitPoints > 0
            && theUnit->mUnitClass->mCategory != kUnitCategoryLandCitizen
            && theUnit->mUnitClass->mCategory != kUnitCategoryNavalCitizen;
        if (theKeeps != theObject)
            sDry->mMismatches++;
        if (!theKeeps)
            DryRemove(theGroup, theUnit);
        else
            theIndex++;
    }
    Log({kDryKeepNonCitizens, theGroup->mTag});
    LogUnits(theGroup->mUnits);
}

void DryKeepCitizens(void* inGroup)
{
    // EEActiveGroup::KeepCitizens' loop over the store's test, compared with the object's
    DryGroup* theGroup = Dry(inGroup);
    for (unsigned long theIndex = 0; theIndex < theGroup->mUnits.size();) {
        EEComplexUnit* theUnit = theGroup->mUnits[theIndex];
        bool theKeeps = ee_units_keeps_citizen(static_cast<uint32_t>(theUnit->mUnitID));
        bool theObject = theUnit->mIsCommandable && theUnit->mHitPoints > 0
            && (theUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen
                || theUnit->mUnitClass->mCategory == kUnitCategoryNavalCitizen);
        if (theKeeps != theObject)
            sDry->mMismatches++;
        if (!theKeeps)
            DryRemove(theGroup, theUnit);
        else
            theIndex++;
    }
    Log({kDryKeepCitizens, theGroup->mTag});
    LogUnits(theGroup->mUnits);
}

// the dry plan's pieces (never read: the dry build only logs)
int sDryPieces = 0;

} // namespace

void* EEUnitHostDryBegin(const std::vector<EEComplexUnit*>& inUnits)
{
    sDry = new DryState();
    sDry->mMismatches = 0;
    sDry->mDraws = 0;
    DryGroup* theSelection = NewDryGroup();
    theSelection->mUnits = inUnits;
    return theSelection;
}

void EEUnitHostDryEnd(std::vector<uint32_t>& outLog, unsigned long& outMismatches)
{
    outLog.swap(sDry->mLog);
    outMismatches = sDry->mMismatches;
    for (size_t i = 0; i < sDry->mGroups.size(); i++)
        delete sDry->mGroups[i];
    for (size_t i = 0; i < sDry->mGoals.size(); i++)
        delete sDry->mGoals[i];
    delete sDry;
    sDry = NULL;
}

// ---- the host's calls ----

extern "C" size_t ee_units_host_group_ids(void* inGroup, uint32_t* outIDs, size_t inCapacity)
{
    if (sDry != NULL)
        return DryGroupIDs(inGroup, outIDs, inCapacity);
    EEGroup* theGroup = static_cast<EEGroup*>(inGroup);
    size_t theCount = theGroup->mUnits.size();
    for (size_t i = 0; i < theCount && i < inCapacity; i++) {
        EEComplexUnit* theUnit = theGroup->mUnits[i];
        outIDs[i] = theUnit != NULL ? static_cast<uint32_t>(theUnit->mUnitID) : 0xffffffffu;
    }
    return theCount;
}

extern "C" void* ee_units_host_goal_new(const EeGoalSpec* inSpec)
{
    if (sDry != NULL) {
        return DryGoalNew(inSpec);
    }
    bool theFlagA = inSpec->flag_a != 0;
    bool theFlagB = inSpec->flag_b != 0;
    switch (inSpec->ctor) {
    case EE_GOAL_CTOR_EXPLORE:
        return static_cast<EEUnitGoal*>(new EEUGExplore(theFlagA));
    case EE_GOAL_CTOR_ATTACK_GROUND:
        return static_cast<EEUnitGoal*>(new EEUGAttackGround(GE3DPoint(inSpec->point[0], inSpec->point[1], inSpec->point[2]), theFlagA));
    case EE_GOAL_CTOR_ATTACK_UNIT: {
        EEUnit* theTarget = UnitOf(inSpec->unit_b);
        if (theTarget == NULL)
            return NULL;
        return static_cast<EEUnitGoal*>(new EEUGAttackUnit(ComplexOf(inSpec->unit_a), theTarget, theFlagA, theFlagB));
    }
    case EE_GOAL_CTOR_GATHER:
        return static_cast<EEUnitGoal*>(new EEUGGather(UnitOf(inSpec->unit_a), UnitOf(inSpec->unit_b), theFlagA));
    case EE_GOAL_CTOR_HEAL_UNIT: {
        EEComplexUnit* theTarget = ComplexOf(inSpec->unit_a);
        if (theTarget == NULL)
            return NULL;
        return static_cast<EEUnitGoal*>(new EEUGHealUnit(theTarget, theFlagA));
    }
    case EE_GOAL_CTOR_PATROL:
        return static_cast<EEUnitGoal*>(new EEUGPatrol(theFlagA));
    case EE_GOAL_CTOR_PATROL_ROUTE:
        return static_cast<EEUnitGoal*>(new EEUGPatrol(static_cast<EEPPatrolGroup*>(inSpec->object), ComplexOf(inSpec->unit_a), theFlagA));
    case EE_GOAL_CTOR_LOCATION: {   // EEMPathTo's: new EEUGLocation(&point, true, forced, rotating, facing, false, 0)
        GE3DPoint theLocation(inSpec->point[0], inSpec->point[1], inSpec->point[2]);
        EEUGLocation* theGoal = new EEUGLocation(&theLocation, true, theFlagA, theFlagB, inSpec->value, false, 0);
        theGoal->mUseGroupMovement = inSpec->flag_c != 0;
        return static_cast<EEUnitGoal*>(theGoal);
    }
    case EE_GOAL_CTOR_UNLOAD_TRANSPORT: {   // EEMUnloadTransport's: the first unit's complex unit, the tile
        EEComplexUnit* theUnit = ComplexOf(inSpec->unit_a);
        if (theUnit == NULL)
            return NULL;
        return static_cast<EEUnitGoal*>(new EEUGUnloadTransport(theUnit->GetComplexUnit(), (long)inSpec->point[0],
                                                                (long)inSpec->point[1], theFlagA));
    }
    case EE_GOAL_CTOR_LOCATION_XY:   // EEMUnloadTransport's move
        return static_cast<EEUnitGoal*>(new EEUGLocation(inSpec->point[0], inSpec->point[1], 0.0f, false, true, false,
                                                         0.0f, false, 0));
    default:
        return NULL;
    }
}

extern "C" uint8_t ee_units_host_goal_kind(void* inGoal)
{
    if (sDry != NULL) {
        return static_cast<DryGoal*>(inGoal)->mKind;
    }
    return static_cast<EEUnitGoal*>(inGoal)->mKind;
}

extern "C" void ee_units_host_goal_release(void* inGoal)
{
    if (sDry != NULL) {
        Log({kDryGoalRelease, static_cast<DryGoal*>(inGoal)->mCtor});
        return;
    }
    delete static_cast<EEUnitGoal*>(inGoal);
}

// a group as Rust holds it: its EEGroup base (the address group_ids reads)
static EEActiveGroup* ActiveOf(void* inGroup)
{
    return static_cast<EEActiveGroup*>(static_cast<EEGroup*>(inGroup));
}

extern "C" void* ee_units_host_active_group_new(void* inPlayer)
{
    if (sDry != NULL) {
        DryGroup* theGroup = NewDryGroup();
        Log({kDryNewGroup, theGroup->mTag});
        return theGroup;
    }
    return static_cast<EEGroup*>(new EEActiveGroup(static_cast<EEPlayer*>(inPlayer)));
}

extern "C" void ee_units_host_group_fill(void* inGroup, void* inSelection, int32_t inSort, int32_t inSlot)
{
    if (sDry != NULL) {
        DryFill(inGroup, inSelection, inSort, inSlot);
        return;
    }
    ActiveOf(inGroup)->Fill(static_cast<EESelectedGroup*>(static_cast<EEGroup*>(inSelection)), inSort, inSlot);
}

extern "C" void ee_units_host_group_release(void* inGroup)
{
    if (sDry != NULL) {
        Log({kDryRelease, Dry(inGroup)->mTag});
        return;
    }
    ActiveOf(inGroup)->Dereference();
}

extern "C" void ee_units_host_pending_push(void* inPlayer, void* inGroup)
{
    if (sDry != NULL) {
        Log({kDryPending, PlayerID(inPlayer), Dry(inGroup)->mTag});
        return;
    }
    static_cast<EEPlayer*>(inPlayer)->mPendingGroups.push_back(static_cast<EEGroup*>(inGroup));
}

extern "C" void ee_units_host_group_keep_mobile(void* inGroup)
{
    if (sDry != NULL) {
        DryKeepMobile(inGroup);
        return;
    }
    ActiveOf(inGroup)->KeepMobileUnits();
}

extern "C" void ee_units_host_group_clear_goal(void* inGroup)
{
    if (sDry != NULL) {
        Log({kDryClearGoal, Dry(inGroup)->mTag});
        return;
    }
    ActiveOf(inGroup)->ClearGoal();
}

extern "C" void ee_units_host_group_set_goal(void* inGroup, void* inGoal, uint8_t inQueued)
{
    if (sDry != NULL) {
        Log({kDrySetGoal, Dry(inGroup)->mTag, inGoal != NULL ? static_cast<DryGoal*>(inGoal)->mCtor : 0u, inQueued});
        return;
    }
    ActiveOf(inGroup)->SetGoal(static_cast<EEUnitGoal*>(inGoal), inQueued != 0);
}

extern "C" void ee_units_host_group_set_action(void* inGroup, int32_t inAction)
{
    if (sDry != NULL) {
        Log({kDrySetAction, Dry(inGroup)->mTag, (uint32_t)inAction});
        return;
    }
    ActiveOf(inGroup)->mAction = inAction;
}

extern "C" void ee_units_host_group_plan(void* inGroup)
{
    if (sDry != NULL) {
        Log({kDryPlan, Dry(inGroup)->mTag});
        return;
    }
    ActiveOf(inGroup)->Plan();
}

extern "C" int32_t ee_units_host_random(int32_t inLow, int32_t inHigh)
{
    if (sDry != NULL) {
        // no draw: the answers walk the range
        int32_t theAnswer = inHigh >= inLow ? inLow + (int32_t)(sDry->mDraws++ % (uint32_t)(inHigh - inLow + 1)) : inLow;
        Log({kDryRandom, (uint32_t)inLow, (uint32_t)inHigh, (uint32_t)theAnswer});
        return theAnswer;
    }
    return (int32_t)gPhysics->GetRandom((long)inLow, (long)inHigh);
}

extern "C" void ee_units_host_play_acknowledgement(void* inPlayer, void* inGroup, int32_t inIndex)
{
    if (sDry != NULL) {
        DryGroup* theGroup = Dry(inGroup);
        uint32_t theUnit = inIndex >= 0 && (size_t)inIndex < theGroup->mUnits.size()
            ? static_cast<uint32_t>(theGroup->mUnits[inIndex]->mUnitID) : 0xffffffffu;
        Log({kDryAcknowledge, PlayerID(inPlayer), theGroup->mTag, (uint32_t)inIndex, theUnit});
        return;
    }
    static_cast<EEPlayer*>(inPlayer)->PlayAcknowledgement(ActiveOf(inGroup)->GetUnit(inIndex));
}

extern "C" float ee_units_host_map_height(float inX, float inY)
{
    GE3DPoint theTarget;
    theTarget.SetX(inX);
    theTarget.SetY(inY);
    float theHeight = gWorld.GetMap()->GetHeight(theTarget);
    if (sDry != NULL)
        Log({kDryMapHeight, Bits(inX), Bits(inY), Bits(theHeight)});
    return theHeight;
}

extern "C" void ee_units_host_group_keep_ground_attackers(void* inGroup)
{
    if (sDry != NULL) {
        DryKeepGroundAttackers(inGroup);
        return;
    }
    ActiveOf(inGroup)->KeepGroundAttackers();
}

extern "C" void ee_units_host_send_group_command(void* inGroup, uint32_t inTarget, void* inPlayer, int32_t inCommand, int32_t inQueue)
{
    if (sDry != NULL) {
        Log({kDrySendCommand, Dry(inGroup)->mTag, inTarget, PlayerID(inPlayer), (uint32_t)inCommand, (uint32_t)inQueue});
        return;
    }
    EEUnit* theTarget = inTarget != 0xffffffffu ? UnitOf(inTarget) : NULL;
    EESendGroupCommand(static_cast<EEGroup*>(inGroup), theTarget, static_cast<EEPlayer*>(inPlayer)->mPlayerID, inCommand, inQueue);
}

extern "C" void ee_units_host_acknowledge_unit(void* inPlayer, uint32_t inUnit)
{
    if (sDry != NULL) {
        Log({kDryAcknowledgeUnit, PlayerID(inPlayer), inUnit});
        return;
    }
    static_cast<EEPlayer*>(inPlayer)->PlayAcknowledgement(ComplexOf(inUnit));
}

// EEMPatrol's target -1: the unit's nearest town (FindNearestTown) and, when it has a patrol group, that group's point
// nearest to the unit's tile (GetNearest; NULL when it has none). Reads only (the dry host's too).
extern "C" uint32_t ee_units_host_town_patrol_point(uint32_t inUnit, uint8_t* outFound)
{
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    EETown* theSite = theUnit->FindNearestTown();
    uint32_t thePoint = 0xffffffffu;
    *outFound = 0;
    if (theSite && theSite->mPatrolGroup) {
        *outFound = 1;
        EEComplexUnit* theNearest = theSite->mPatrolGroup->GetNearest(theUnit->GetTileX(), theUnit->GetTileY());
        thePoint = theNearest != NULL ? static_cast<uint32_t>(theNearest->mUnitID) : 0xffffffffu;
    }
    if (sDry != NULL)
        Log({kDryTownPoint, inUnit, *outFound, thePoint});
    return thePoint;
}

extern "C" void ee_units_host_patrol_add_unit_tile(uint32_t inUnit, uint32_t inTarget, void* inPlayer)
{
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    EEComplexUnit* theTarget = ComplexOf(inTarget);
    GE2DPoint thePoint;
    thePoint.SetX((float)theTarget->mTileX);
    thePoint.SetY((float)theTarget->mTileY);
    if (sDry != NULL) {
        Log({kDryAddUnitTile, inUnit, inTarget, PlayerID(inPlayer), Bits(thePoint.GetX()), Bits(thePoint.GetY())});
        return;
    }
    theUnit->mPatrolGroup->AddPoint(thePoint, static_cast<EEPlayer*>(inPlayer)->mPlayerID);
}

extern "C" void* ee_units_host_unit_patrol_group(uint32_t inUnit)
{
    EEPPatrolGroup* theRoute = ComplexOf(inUnit)->mPatrolGroup;
    if (sDry != NULL) {
        Log({kDryRoute, inUnit});
        LogRoute(theRoute);
    }
    return theRoute;
}

// a read (the dry host does not log it)
extern "C" int64_t ee_units_host_object_unit_type(uint32_t inUnit)
{
    EEUnit* theUnit = UnitOf(inUnit);
    EEUnitClass* theClass = theUnit != NULL ? theUnit->mUnitClass.Get() : NULL;
    return theClass != NULL ? (int64_t)theClass->mUnitType : -1;
}

extern "C" void ee_units_host_group_set_queued(void* inGroup, uint8_t inQueued)
{
    if (sDry != NULL) {
        Log({kDryQueued, Dry(inGroup)->mTag, inQueued});
        return;
    }
    ActiveOf(inGroup)->mIsQueued = inQueued != 0;
}

// the dry host does not spread: the group keeps its units (logged)
extern "C" void ee_units_host_group_spread_around_resource(void* inGroup, uint32_t inTarget)
{
    if (sDry != NULL) {
        Log({kDrySpreadResource, Dry(inGroup)->mTag, inTarget});
        return;
    }
    ActiveOf(inGroup)->SpreadAroundResource(UnitOf(inTarget));
}

extern "C" void ee_units_host_group_spread_around_tile(void* inGroup, int64_t inX, int64_t inY)
{
    if (sDry != NULL) {
        Log({kDrySpreadTile, Dry(inGroup)->mTag, (uint32_t)inX, (uint32_t)inY});
        return;
    }
    ActiveOf(inGroup)->SpreadAroundTile((long)inX, (long)inY);
}

extern "C" void ee_units_host_unit_add_build_target(uint32_t inUnit, uint32_t inTarget)
{
    if (sDry != NULL) {
        Log({kDryAddBuildTarget, inUnit, inTarget});
        return;
    }
    EEComplexUnit* theUnit = UnitOf(inUnit)->mComplexUnit;
    theUnit->AddBuildTarget(UnitOf(inTarget));
    // Port: no smart task after a shift-queued repair (EEUGBuildRepair::Step)
    theUnit->mHasShiftQueuedBuild = true;
}

extern "C" void ee_units_host_unit_order_repair(uint32_t inUnit, uint32_t inTarget)
{
    if (sDry != NULL) {
        Log({kDryOrderRepair, inUnit, inTarget});
        return;
    }
    EEComplexUnit* theUnit = UnitOf(inUnit)->mComplexUnit;
    theUnit->OrderRepair(UnitOf(inTarget));
    theUnit->StartAction(kActionIdle);
    theUnit->mUnitClass->ActionChanged(theUnit);
}

extern "C" void ee_units_host_group_add_member(void* inGroup, uint32_t inUnit)
{
    if (sDry != NULL) {
        Log({kDryAddMember, Dry(inGroup)->mTag, inUnit});
        return;
    }
    EEComplexUnit* theUnit = UnitOf(inUnit)->mComplexUnit;
    ActiveOf(inGroup)->AddMember(theUnit->mComplexUnit, true);
}

extern "C" void ee_units_host_group_keep_non_citizens(void* inGroup)
{
    if (sDry != NULL) {
        DryKeepNonCitizens(inGroup);
        return;
    }
    ActiveOf(inGroup)->KeepNonCitizens();
}

// EEMAddPatrolPoint with nothing selected: a lone patrol group with the point, registered with the player
extern "C" void ee_units_host_patrol_new_lone(void* inPlayer, float inX, float inY)
{
    if (sDry != NULL) {
        Log({kDryLonePatrol, PlayerID(inPlayer), Bits(inX), Bits(inY)});
        return;
    }
    EEPlayer* thePlayer = static_cast<EEPlayer*>(inPlayer);
    EEPPatrolGroup* thePatrolGroup = new EEPPatrolGroup();
    thePatrolGroup->AddPoint(GE2DPoint(inX, inY), thePlayer->mPlayerID);
    thePlayer->AddPatrolGroup(thePatrolGroup);
    thePatrolGroup->Dereference();
}

// a selected path point: the point into its route after it
extern "C" void ee_units_host_patrol_insert_point_at(uint32_t inUnit, float inX, float inY, void* inPlayer)
{
    if (sDry != NULL) {
        Log({kDryInsertPoint, inUnit, PlayerID(inPlayer), Bits(inX), Bits(inY)});
        return;
    }
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    theUnit->mPatrolGroup->InsertPatrolPointAt(GE2DPoint(inX, inY), static_cast<EEPlayer*>(inPlayer)->mPlayerID, theUnit);
}

// the route of the unit's current goal when it is a patrol (EEGoalCast<EEUGPatrol>(GetGoal())), else NULL; a read
extern "C" void* ee_units_host_unit_goal_patrol_route(uint32_t inUnit)
{
    EEUGPatrol* theGoal = EEGoalCast<EEUGPatrol>(ComplexOf(inUnit)->GetGoal());
    EEPPatrolGroup* theRoute = theGoal != NULL ? theGoal->mPatrolGroup : NULL;
    if (sDry != NULL) {
        Log({kDryGoalRoute, inUnit});
        LogRoute(theRoute);
    }
    return theRoute;
}

extern "C" void ee_units_host_patrol_add_point(void* inRoute, float inX, float inY, void* inPlayer)
{
    if (sDry != NULL) {
        Log({kDryAddPoint});
        LogRoute(inRoute);
        Log({PlayerID(inPlayer), Bits(inX), Bits(inY)});
        return;
    }
    static_cast<EEPPatrolGroup*>(inRoute)->AddPoint(GE2DPoint(inX, inY), static_cast<EEPlayer*>(inPlayer)->mPlayerID);
}

// a new patrol goal's route: the point added, the route registered with the player
extern "C" void ee_units_host_goal_patrol_add_point(void* inGoal, float inX, float inY, void* inPlayer)
{
    if (sDry != NULL) {
        Log({kDryGoalAddPoint, static_cast<DryGoal*>(inGoal)->mCtor, PlayerID(inPlayer), Bits(inX), Bits(inY)});
        return;
    }
    EEPlayer* thePlayer = static_cast<EEPlayer*>(inPlayer);
    EEUGPatrol* theGoal = static_cast<EEUGPatrol*>(static_cast<EEUnitGoal*>(inGoal));
    theGoal->mPatrolGroup->AddPoint(GE2DPoint(inX, inY), thePlayer->mPlayerID);
    thePlayer->AddPatrolGroup(theGoal->mPatrolGroup);
}

// the dry host does not spread: the group keeps its units (logged)
extern "C" void ee_units_host_group_spread_around_unit(void* inGroup, uint32_t inTarget, int64_t inX, int64_t inY)
{
    if (sDry != NULL) {
        Log({kDrySpreadUnit, Dry(inGroup)->mTag, inTarget, (uint32_t)inX, (uint32_t)inY});
        return;
    }
    ActiveOf(inGroup)->SpreadAroundUnit(UnitOf(inTarget), (long)inX, (long)inY);
}

// the unit's complex unit (EEUnit::GetComplexUnit) as an id, 0xffffffff for none; a read
extern "C" uint32_t ee_units_host_unit_complex_id(uint32_t inUnit)
{
    EEUnit* theUnit = UnitOf(inUnit);
    EEComplexUnit* theComplex = theUnit != NULL ? theUnit->GetComplexUnit() : NULL;
    uint32_t theID = theComplex != NULL ? static_cast<uint32_t>(theComplex->mUnitID) : 0xffffffffu;
    if (sDry != NULL)
        Log({kDryComplexOf, inUnit, theID});
    return theID;
}

// EEMPathTo's point (its statements before the loop, as they were): a point off the map goes to the closest tile's
// centre; a tile under a building the first selected unit cannot pass goes to the building's nearest footprint tile;
// the height is the final tile's (at least 0). *ioX, *ioY: the message's point in, the point the body uses out (the
// message keeps it). A read (the dry host's too)
extern "C" float ee_units_host_path_to_point(uint32_t inFirst, float* ioX, float* ioY)
{
    EEComplexUnit* theFirstUnit = ComplexOf(inFirst);
    float theInX = *ioX, theInY = *ioY;
    long theX = (long)*ioX;
    long theY = (long)*ioY;
    EETile* theTile = gWorld.GetMap()->GetTile(theX, theY);
    if (theTile == NULL) {
        GE3DPoint thePoint((float)theX + 0.5f, (float)theY + 0.5f, 0.0f);
        theTile = gWorld.GetMap()->GetClosestTile(&thePoint, &theX, &theY);
        *ioX = (float)theX + 0.5f;
        *ioY = (float)theY + 0.5f;
    }
    for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
        if (theNode->mUnit->mUnitClass->mTileSize > 1 && !theNode->mUnit->IsPassableBy(theFirstUnit->GetComplexUnit())) {
            theFirstUnit->GetNearestFootprintTile(theNode->mUnit, &theX, &theY);
            theTile = gWorld.GetMap()->GetTile(theX, theY);
            if (theTile != NULL) {
                *ioX = (float)theX + 0.5f;
                *ioY = (float)theY + 0.5f;
                break;
            }
            theX = (long)*ioX;
            theY = (long)*ioY;
            theTile = gWorld.GetMap()->GetTile(theX, theY);
        }
    }
    float theZ = theTile->mHeight >= 0.0f ? theTile->mHeight : 0.0f;
    if (sDry != NULL)
        Log({kDryPathPoint, inFirst, Bits(theInX), Bits(theInY), Bits(*ioX), Bits(*ioY), Bits(theZ)});
    return theZ;
}

extern "C" void ee_units_host_group_reset_paths(void* inGroup)
{
    if (sDry != NULL) {
        Log({kDryResetPaths, Dry(inGroup)->mTag});
        return;
    }
    ActiveOf(inGroup)->ResetUnitsPaths();
}

extern "C" void ee_units_host_group_keep_citizens(void* inGroup)
{
    if (sDry != NULL) {
        DryKeepCitizens(inGroup);
        return;
    }
    ActiveOf(inGroup)->KeepCitizens();
}

// EEMBuildWall's plan: the wall of the technology's unit class from start to end as paid foundations (EEPlanWall(..,
// true, false, pieces)); the new pieces in a list the caller releases. The dry host plans nothing (it would place
// units): two pieces, logged
extern "C" void* ee_units_host_wall_plan(void* inPlayer, int32_t inTechnology, float inStartX, float inStartY, float inEndX,
                                         float inEndY, uintptr_t* outCount)
{
    if (sDry != NULL) {
        Log({kDryWallPlan, PlayerID(inPlayer), (uint32_t)inTechnology, Bits(inStartX), Bits(inStartY), Bits(inEndX),
             Bits(inEndY)});
        *outCount = 2;
        return &sDryPieces;
    }
    EEPlayer* thePlayer = static_cast<EEPlayer*>(inPlayer);
    std::vector<EEUnit*>* thePieces = new std::vector<EEUnit*>();
    EEUnitClass* theClass = gUnitClassTable[thePlayer->mTechnologies->Get(inTechnology)->mData->mUnitClass];
    EEPlanWall(GE2DPoint(inStartX, inStartY), GE2DPoint(inEndX, inEndY), theClass, thePlayer, 1, 0, *thePieces);
    *outCount = thePieces->size();
    return thePieces;
}

extern "C" void ee_units_host_wall_build(void* inGroup, void* inPieces)
{
    if (sDry != NULL) {
        Log({kDryWallBuild, Dry(inGroup)->mTag});
        return;
    }
    EEBuildWall(ActiveOf(inGroup), *static_cast<std::vector<EEUnit*>*>(inPieces));
}

// the dry host does not spread: the group keeps its units (logged)
extern "C" void ee_units_host_group_spread_for_unload(void* inGroup, int64_t inX, int64_t inY)
{
    if (sDry != NULL) {
        Log({kDrySpreadUnload, Dry(inGroup)->mTag, (uint32_t)inX, (uint32_t)inY});
        return;
    }
    ActiveOf(inGroup)->SpreadForUnload((long)inX, (long)inY);
}

// a transport (or paratrooper bomber) with cargo; a read (the dry host's too)
extern "C" uint8_t ee_units_host_unit_can_unload(uint32_t inUnit)
{
    EEComplexUnit* theUnit = ComplexOf(inUnit);
    bool theResult = (theUnit->GetUnitClass()->IsTransport() || theUnit->GetUnitClass()->mIsParatrooperBomber)
        && theUnit->mComplexUnit->GetNumCargoUnits() > 0;
    if (sDry != NULL)
        Log({kDryCanUnload, inUnit, theResult ? 1u : 0u});
    return theResult ? 1 : 0;
}

extern "C" void ee_units_host_goal_set_transport(void* inGoal, uint32_t inUnit)
{
    if (sDry != NULL) {
        Log({kDrySetTransport, static_cast<DryGoal*>(inGoal)->mCtor, inUnit});
        return;
    }
    static_cast<EEUGUnloadTransport*>(static_cast<EEUnitGoal*>(inGoal))->SetTransport(ComplexOf(inUnit)->mComplexUnit);
}

// the dry goal is a record: it answers yes (logged)
extern "C" uint8_t ee_units_host_goal_can_unload(void* inGoal)
{
    if (sDry != NULL) {
        Log({kDryCanUnloadThere, static_cast<DryGoal*>(inGoal)->mCtor});
        return 1;
    }
    return static_cast<EEUGUnloadTransport*>(static_cast<EEUnitGoal*>(inGoal))->CanUnloadAtDestination() ? 1 : 0;
}

extern "C" void ee_units_host_unload_warning(void* inPlayer, int64_t inX, int64_t inY)
{
    if (sDry != NULL) {
        Log({kDryUnloadWarning, PlayerID(inPlayer), (uint32_t)inX, (uint32_t)inY});
        return;
    }
    EEPlayer* thePlayer = static_cast<EEPlayer*>(inPlayer);
    U2DPoint<long> theTile((long)inX, (long)inY);
    thePlayer->mEvents.AddEvent(theTile, 2, thePlayer, 0x7c3b, false);
}

extern "C" void ee_units_host_wall_release(void* inPieces)
{
    if (sDry != NULL) {
        Log({kDryWallRelease});
        return;
    }
    delete static_cast<std::vector<EEUnit*>*>(inPieces);
}
