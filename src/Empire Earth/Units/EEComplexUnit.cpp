// C:\EEX\Empire Earth\Units\EEComplexUnit.cpp (file name inferred: no __FILE__; the keys "EEComplexUnit-*").
// The complex (mobile, goal-driven) unit: 0x1f57d3..0x2010fd. Unit flags: the exe's module set. Data: a GEColor
// 0x963228, the keys 0x963218..0x9631d0 and the pool 0x963198 (one guard byte 0x963210). Contains the helper
// classes EEUnitAreaEffectList, EEOrbital, EEMReplantFarms; the unit's template COMDATs close it.

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <math.h>
#include <typeinfo>
#include "rust/sim/units.h"
#include "ee/EEUnitKind.h"
#include "ee/EETechCapture.h"

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/UMemoryPool.h"
#include "lle/UList.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/GEModel.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EECombatFormulas.h"
#include "ee/EECombatGoals.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEUserInterface.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEWUnitsOffMapInfo.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEAEManager.h"
#include "ee/EEAIContinentManager.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIContinent.h"
#include "ee/EETown.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEAIRandom.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERules.h"
#include "ee/EEWheel.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPathManager.h"
#include "ee/EECPASGFlags.h"
#include "ee/EEGameSound.h"
#include "ee/EEUGRust.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEUnitAction.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEFleeFromLocationScreenBase.h"
#include "ee/EEUGRust.h"
#include "ee/EEAIPathSector.h"
#include "ee/EEComputerPlayer.h"

// Existing cdecl helper defined in EEWheel.cpp; original 0x23cd32 fills a real GE2DPoint.
bool EEGetRandomPointAhead(EEComplexUnit* inUnit, GE2DPoint* outPoint);

// The genuine existing free predicate at 0x2414d2 (defined in EEFOFindNewTargetNearOldTarget.cpp).

// chain at 0x5f57d3 (wrapper)
//   object 0x963228  ctor None  dtor None
//   inline ctor, fields (offset=value, declaration order unknown): +0=0.2, +4=0.2, +8=1
//   dtor stub is a bare `ret`: the class has a user-declared empty inline destructor
//   object bound as _kTimeWarpColor
// the colour of a time-warped unit (SetDisabled 0x1fe5b3: SetColor when +0x358 is 0x1a/0x1c) (provisional name)
const GEColor kTimeWarpColor(0.2f, 0.2f, 1.0f);

// 0x963238: the time of the last fanaticism colour update (UpdateFanaticismColor; only this unit) (provisional name)
static unsigned long sU563238;

// chain at 0x5f5805 (wrapper)
//   object 0x963218  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
//   guarded dtor (byte ptr [0x963210], 1): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
//   object bound as ?sData@EEComplexUnit@@2VUString@@B
const UString EEComplexUnit::sData("EEComplexUnit-Data");   // class static member (owner from the key prefix; EEComplexUnit declares `static const UString ...;`; provisional name)

// chain at 0x5f5848 (wrapper)
//   object 0x963200  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
//   guarded dtor (byte ptr [0x963210], 2): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
//   object bound as ?sCloakableOpacity@EEComplexUnit@@2VUString@@B
const UString EEComplexUnit::sCloakableOpacity("EEComplexUnit-CloakableOpacity");   // class static member (owner from the key prefix; EEComplexUnit declares `static const UString ...;`; provisional name)

// chain at 0x5f588b (wrapper)
//   object 0x9631f0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
//   guarded dtor (byte ptr [0x963210], 4): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
//   object bound as ?sIsCloaked@EEComplexUnit@@2VUString@@B
const UString EEComplexUnit::sIsCloaked("EEComplexUnit-IsCloaked");   // class static member (owner from the key prefix; EEComplexUnit declares `static const UString ...;`; provisional name)

// chain at 0x5f58ce (wrapper)
//   object 0x9631e0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
//   guarded dtor (byte ptr [0x963210], 8): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
//   object bound as ?sDrawAreaEffects@EEComplexUnit@@2VUString@@B
const UString EEComplexUnit::sDrawAreaEffects("EEComplexUnit-DrawAreaEffects");   // class static member (owner from the key prefix; EEComplexUnit declares `static const UString ...;`; provisional name)

// chain at 0x5f5911 (wrapper)
//   object 0x9631d0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
//   guarded dtor (byte ptr [0x963210], 0x10): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
//   object bound as ?sAreaEffectsTick@EEComplexUnit@@2VUString@@B
const UString EEComplexUnit::sAreaEffectsTick("EEComplexUnit-AreaEffectsTick");   // class static member (owner from the key prefix; EEComplexUnit declares `static const UString ...;`; provisional name)

// chain at 0x5f5954 (wrapper)
//   object 0x963198  ctor ??0?$UMemoryPool@VEEComplexUnit@@@@QAE@XZ  dtor ?Release@?$UMemoryPool@VEEComplexUnit@@@@QAE_NXZ
//   guarded dtor (byte ptr [0x963210], 0x20): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
//   object bound as ?sMemoryPool@EEComplexUnit@@2V?$UMemoryPool@VEEComplexUnit@@@@A
UMemoryPool<EEComplexUnit> EEComplexUnit::sMemoryPool;

// Creates a complex unit with every unit, mobile-unit and reference state reset to defaults
EEComplexUnit::EEComplexUnit()
{
    this->mObjectKind.mValue = 2;   // New (port): an EEComplexUnit (ee/EEUnitKind.h)
    // none of the resets below sets the disable reason, which saved games store: start a new unit at 0
    // (a unit reused from the pool keeps its last reason, as before)
    this->mDisabledReason = 0;
    this->Reset();
    this->ResetEEUnit();
    this->ResetEEComplexUnit(true, false);
    this->mReferenceCount = 0;
}

// Adds the unit's line of sight at its current tile
void EEComplexUnit::SightOnHere()
{
    this->SightOn(this->GetTileX(), this->GetTileY());
}

// Returns whether the unit is still a building foundation
bool EEComplexUnit::IsFoundation()
{
    return this->mIsFoundation;
}

// Resets the unit to its default state and marks it invalid (back in the pool)
void EEComplexUnit::Clear()
{
    this->ResetEEComplexUnit(true, false);
    this->ResetEEUnit();
    this->Reset();
    this->mIsValid = false;
}

// Removes the unit's line of sight at its current tile
void EEComplexUnit::SightOffHere()
{
    this->SightOff(this->GetTileX(), this->GetTileY());
}

// Destroys the unit, freeing its turret angles, orbitals, wheels and model
EEComplexUnit::~EEComplexUnit()
{
    this->FreeNode();
}

// Makes the unit hold another unit, keeping the held units sorted by growth
// the mHeldUnits order (std::sort 0x200df9: its heap code compares the units' mGrowth) (provisional name)
struct EEComplexUnitGrowthLess {
    bool operator()(EEComplexUnit* inA, EEComplexUnit* inB) const { return inA->GetGrowth() < inB->GetGrowth(); }
};

void EEComplexUnit::HoldUnit(EEComplexUnit* inUnit)
{
    inUnit->Reference();
    inUnit->SetHolder(this);
    this->mHeldUnits.push_back(inUnit);
    std::sort(this->mHeldUnits.begin(), this->mHeldUnits.end(), EEComplexUnitGrowthLess());
}

// Sets the unit that holds this one, swapping the references
void EEComplexUnit::SetHolder(EEComplexUnit* inUnit)
{
    if (this->mHolder == inUnit)
        return;
    if (this->mHolder)
        this->mHolder->Dereference();
    this->mHolder = inUnit;
    inUnit->Reference();
}

// Ignores a target for 30 seconds from both units' current tiles, or renews an existing ignore entry
void EEComplexUnit::IgnoreTarget(EEComplexUnit* inTarget)
{
    // ee_combat world.rs ignore_target over the world host (EECombatGoals.cpp); CBXR records the outcome
    EECombatWorld(EE_COMBAT_WORLD_IGNORE, this, inTarget);
    EECombatCaptureIgnored(EE_COMBAT_XR_IGNORE, this, inTarget, 0, 0);
}

// Creates an empty ignored-target entry
EEComplexUnitAttacker::EEComplexUnitAttacker()
{
    this->mTileX = 0;
    this->mTileY = 0;
    this->mFromX = 0;
    this->mFromY = 0;
    this->mUnitID = 0;
    this->mTime = 0;
}

// Loads a living unit as cargo if there is room (or room is ignored), taking it off the map; returns 0 or 7
long EEComplexUnit::LoadCargo(EEComplexUnit* inCargo, bool inIgnoreRoom, bool inArg3)
{
    return ee_units_cu_body(EE_UNITS_CU_DO_LOAD_CARGO, this, (int64_t)(uintptr_t)inCargo, inIgnoreRoom, inArg3);   // cu_cargo.rs
}

// Garrisons a unit if capacity allows: adds its weight and population, then removes it as a dead unit
bool EEComplexUnit::GarrisonUnit(EEComplexUnit* inCargo)
{
    return ee_units_cu_body(EE_UNITS_CU_DO_GARRISON_UNIT, this, (int64_t)(uintptr_t)inCargo, 0, 0) != 0;   // cu_cargo.rs
}

// Adds garrison weight (never below 0) and sets the garrisoned population, if capacity allows
bool EEComplexUnit::AddGarrison(long inAmount, long inGarrisonPopulation)
{
    return ee_units_cu_body(EE_UNITS_CU_DO_ADD_GARRISON, this, inAmount, inGarrisonPopulation, 0) != 0;   // cu_cargo.rs
}

// Queues and pays for a technology or unit at this building (front or back), honouring the hero limit
void EEComplexUnit::QueueResearch(long inTechnology, bool inFront)
{
    if (EESimCaptureRecord::Active()) {   // TEST ONLY: a research start (ee/EETechCapture.h)
        EESimCaptureRecord theRecord(EECaptureTag("TQRS"));
        theRecord.I32(this->GetPlayer()->mPlayerID);
        theRecord.I32((long)this->mUnitID);
        theRecord.I32(this->GetUnitClass()->GetUnitType());
        theRecord.I32(inTechnology);
        theRecord.U32(inFront ? 1 : 0);
        theRecord.U32((unsigned int)this->mBuildQueue.size());
        this->QueueResearchUncaptured(inTechnology, inFront);
        theRecord.U32((unsigned int)this->mBuildQueue.size());
        theRecord.Commit();
        return;
    }
    this->QueueResearchUncaptured(inTechnology, inFront);
}

void EEComplexUnit::QueueResearchUncaptured(long inTechnology, bool inFront)
{
    ee_units_cu_body(EE_UNITS_CU_DO_QUEUE_RESEARCH, this, inTechnology, inFront, 0);   // cu_cargo.rs
}

// Stops the citizen's goals and puts the target first in its build queue, keeping a current build target
void EEComplexUnit::OrderRepair(EEUnit* inTarget)
{
    ee_units_cu_body(EE_UNITS_CU_DO_ORDER_REPAIR, this, (int64_t)(uintptr_t)inTarget, 0, 0);   // cu_cargo.rs
}

// Appends a building to the citizen's build queue unless it is already queued
void EEComplexUnit::AddBuildTarget(EEUnit* inTarget)
{
    ee_units_cu_body(EE_UNITS_CU_DO_ADD_BUILD_TARGET, this, (int64_t)(uintptr_t)inTarget, 0, 0);   // cu_cargo.rs
}

// Adds a unit to the contact (collision) list unless it is already there
void EEComplexUnit::AddContact(EEComplexUnit* inUnit)
{
    ee_units_cu_body(EE_UNITS_CU_DO_ADD_CONTACT, this, (int64_t)(uintptr_t)inUnit, 0, 0);   // cu_cargo.rs
}

#include "ee/EEStringIDs.h"
#include "ee/EEEffectManager.h"
#include "ee/EESoundObject.h"
#include "lle/UResourceFile.h"
// Original cdecl sight-map family, retained as link calls (no helper implementations).
void EEAddSightCircle(float inX, float inY, float inRadius, EEPlayer* inPlayer);
void EERemoveSightCircle(float inX, float inY, float inRadius, EEPlayer* inPlayer);
void EEMoveSightCircle(float inOldX, float inOldY, float inNewX, float inNewY, float inRadius, EEPlayer* inPlayer);
bool gSightUpdatesEnabled = true;           // 0x4c76ec (.data = 1; just before this unit's 0x4c76f0 table; EEUserInterface/EEWorld/EEMap read it; provisional name)

extern char gEmptyString[];

// a float's bits as a cu_body.rs argument, and back
static int64_t EECuBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, sizeof(theBits));
    return theBits;
}

static float EECuFloat(int64_t inBits)
{
    uint32_t theBits = (uint32_t)inBits;
    float theValue;
    memcpy(&theValue, &theBits, sizeof(theValue));
    return theValue;
}

// Refunds part of the unit's cost as insurance and tells the local player when a refund threshold is passed
// (cu_body.rs; the thresholds there: 0x4c76f0)

void EEComplexUnit::PayInsurance(float inFraction)
{
    ee_units_cu_body(EE_UNITS_CU_DO_PAY_INSURANCE, this, EECuBits(inFraction), 0, 0);   // cu_body.rs
}


// Records as mutual contacts the living collidable units under the unit's footprint in the same layer
void EEComplexUnit::FindContacts()
{
    ee_units_cu_body(EE_UNITS_CU_DO_FIND_CONTACTS, this, 0, 0, 0);   // cu_body.rs
}

// Returns whether the unit accepts the resource type (any, or the one it carries)
bool EEComplexUnit::CanTake(EEComplexUnit* inCargo, unsigned long inValue)
{
    return this->mResourceType == kResourceAny || this->mResourceType == inValue ? true : false;
}

// Removes the unit from play: drops its path, goals, queues, sound and every unit reference it holds
void EEComplexUnit::Invalidate()
{
    ee_units_cu_body(EE_UNITS_CU_DO_INVALIDATE, this, 0, 0, 0);   // cu_body.rs
}

// Releases the unit's sound instance
void EEComplexUnit::ReleaseSound()
{
    if (this->mSound)
        this->mSound->Dereference();
    this->mSound = NULL;
}

// Clears every goal queue of the unit
void EEComplexUnit::ClearAllGoalQueues()
{
    for (unsigned long i = 0; i < this->GetNumGoalQueues(); i++)
        this->ClearGoalQueue(i);
}

// Releases every held unit, detaching those this unit holds
void EEComplexUnit::ReleaseHeldUnits()
{
    for (long i = this->mHeldUnits.size() - 1; i >= 0; i--) {
        if (this->mHeldUnits[i]->mHolder == this)
            this->mHeldUnits[i]->ClearHolder();
        this->mHeldUnits[i]->Dereference();
    }
    this->mHeldUnits.clear();
}

// Takes the unit out of every area effect acting on it and empties that list
void EEComplexUnit::RemoveAffectingAreaEffects()
{
    for (std::vector<EEAreaEffect*>::const_iterator theIterator = this->mAffectingAreaEffects.mItems.begin(); theIterator != this->mAffectingAreaEffects.mItems.end(); ++theIterator)
        (*theIterator)->RemovedFrom(this);
    EEUnitAreaEffectList& theList = this->mAffectingAreaEffects;
    theList.Clear();
    theList.mItems.clear();
}

// Refunds the current research and every queued item, re-allowing techs and updating the hero count
void EEComplexUnit::UpdateJITManufacturing()
{
    ee_units_cu_body(EE_UNITS_CU_DO_UPDATE_JIT, this, 0, 0, 0);   // cu_cargo.rs
}

// Empties the citizen's build queue, releasing each building
void EEComplexUnit::ClearBuildTargets()
{
    while (this->mCitizenBuildQueue.size() != 0) {
        this->mCitizenBuildQueue.front()->Dereference();
        this->mCitizenBuildQueue.pop_front();
    }
    this->mHasShiftQueuedBuild = false;
}

// Empties the contact list, removing this unit from each contact's list too
void EEComplexUnit::ClearContacts()
{
    for (UListNode<EEComplexUnit*>* theNode = this->mContacts.mHead; theNode; theNode = theNode->mNext) {
        theNode->mData->RemoveContactUnit(this);
        theNode->mData->Dereference();
    }
    this->mContacts.Clear();
}

// Destroys the unit's conversion graphic effect, if any
void EEComplexUnit::ReleaseConversionEffect()
{
    EEEffectHandle theHandle = this->mConversionEffect;
    if (!UIsNone32(theHandle.mValue)) {
        gEffectManager->DestroyEffect(theHandle);
        this->mConversionEffect.mValue = -1;
    }
}

// Clears one goal queue (abandoning the main queue's goals first) and frees the unit's airport slot
void EEComplexUnit::ClearGoalQueue(unsigned long inIndex)
{
    ee_units_cu_body(EE_UNITS_CU_DO_CLEAR_GOAL_QUEUE, this, inIndex, 0, 0);   // cu_misc.rs
}

// Destroys the unit's heal graphic effect, if any
void EEComplexUnit::ReleaseHealEffect()
{
    EEEffectHandle theHandle = this->mHealEffect;
    if (!UIsNone32(theHandle.mValue)) {
        gEffectManager->DestroyEffect(theHandle);
        this->mHealEffect.mValue = -1;
    }
}

// Destroys the fire effect of a citizen, gate or wall, or removes a cloak effect from a CP market
void EEComplexUnit::ReleaseFireEffect()
{
    ee_units_cu_body(EE_UNITS_CU_DO_RELEASE_FIRE_EFFECT, this, 0, 0, 0);   // cu_misc.rs
}

// Copies a replaced unit's state into this one: position, timers, scaled hit points, goals, cargo, links
void EEComplexUnit::CopyFrom(RTSUnit* inUnit)
{
    EEComplexUnit* theOther = EEUnitCast<EEComplexUnit>(inUnit);
    this->mProjectileStart = theOther->mProjectileStart;
    this->SetAnimationFrame(theOther->mPhysicalModel.GetFrame());
    this->SetAnimation(theOther->mAnimation, -1, 0);
    if (theOther->GetUnitClass()->mTileSize == this->GetUnitClass()->mTileSize) {
        this->mPhysicalModel.SetLocation(theOther->mPhysicalModel.GetLocation());
        this->mAngularVelocity = theOther->mAngularVelocity;
        this->mOrientation = theOther->mOrientation;
    }
    this->mInitialContactLocation = theOther->mInitialContactLocation;
    this->mInitialContactHeading = theOther->mInitialContactHeading;
    this->mUnitVariables = theOther->mUnitVariables;
    this->mBehavior = theOther->mBehavior;
    if (this->GetPlayer()->IsHuman()) {
        if (this->mBehavior == EEUnitBehavior::sTable[3] || this->mBehavior == EEUnitBehavior::sTable[4])
            this->mBehavior = NULL;
    }
    if (this->mBehavior == NULL) {
        if (this->GetUnitClass() != NULL)
            this->mBehavior = this->GetUnitClass()->mBehavior;
        if (this->mBehavior == NULL)
            this->mBehavior = EEUnitBehavior::sTable[5];
    }
    this->mHeight = theOther->mHeight;
    if (theOther->mOwner != NULL)
        this->SetOwner(theOther->mOwner);
    if (this->GetUnitClass()->GetUnitType() == theOther->GetUnitClass()->GetUnitType()) {
        unsigned long i;
        for (i = 0; i < theOther->GetModel()->GetNumParts() && i < this->GetModel()->GetNumParts(); ++i)
            this->mTurretAngles[i] = theOther->mTurretAngles[i];
        for (i = 0; i < theOther->mNumOrbitals && i < this->mNumOrbitals; ++i)
            this->mOrbitals[i]->mAngle = theOther->mOrbitals[i]->mAngle;
        for (i = 0; i < theOther->mNumWheels && i < this->mNumWheels; ++i)
            this->mWheels[i]->mAngle = theOther->mWheels[i]->mAngle;
    }
    if (this->mSound != NULL)
        gSoundManager->Stop(this->mSound, false);
    this->SetSound(theOther->mSound);
    // the record's copies and the fractions scaled to this unit's maxima (ee_units unit_bulk.rs copy_first)
    EEUnitClassUpgrade* theStats = this->GetPlayer()->GetUpgrade(this->GetUnitClass()->GetUnitType());
    EEUnitClassUpgrade* theOtherStats = theOther->GetPlayer()->GetUpgrade(theOther->GetUnitClass()->GetUnitType());
    EEUnitSet* theCamouflageSet = gCivPowersManager.HasPower(this->GetPlayer(), kCivPowerCamouflage)
                                      ? gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet)
                                      : NULL;
    EeUnitsComplexCopyIn theIn = {theOtherStats->mMaxHitPoints, theStats->mMaxHitPoints, theOtherStats->mPower,
                                  theStats->mPower, theOtherStats->mFlightTime, theStats->mFlightTime,
                                  theOtherStats->mReloadTime, theStats->mReloadTime,
                                  theCamouflageSet != NULL && theCamouflageSet->Contains(theOther->GetUnitClass()->GetUnitType())
                                      && theCamouflageSet->Contains(this->GetUnitClass()->GetUnitType())};
    ee_units_complex_copy(0, this, theOther, &theIn);
    this->UpdateCamouflage(this->mCommand != NULL ? this->mCommand->mType : 0);
    this->mLastMapPosition = theOther->mLastMapPosition;
    this->mU0DC = theOther->mU0DC;
    this->mCommand = theOther->mCommand;
    ee_units_complex_copy(1, this, theOther, NULL);   // (the original's mB34F: GetB34F's call, not its address)
    EEMovePassengers(this, theOther, theOther->GetPlayer() != this->GetPlayer());
    EETakeOverLoad(this, theOther);
    for (unsigned long i = 0; i < theOther->mGoalQueues.size() && i < this->mGoalQueues.size(); ++i) {
        this->mGoalQueues[i].Clear();
        for (unsigned long j = 0; j < theOther->mGoalQueues[i].mGoals.size(); ++j) {
            EEUnitGoal* theGoal = theOther->mGoalQueues[i].mGoals[j];
            if (theGoal->mKind == kGoalUnloadTransport && theOther->GetPlayer() == this->GetPlayer())
                EEUnloadGoalCast(theGoal)->SetUnloadTransport(this);
            theGoal = theGoal->Clone();
            this->mGoalQueues[i].mGoals.push_back(theGoal);
            theGoal->SetOwner(this->mGoalQueues[i].GetUnit());
        }
    }
    this->mWaypoints = theOther->mWaypoints;
    this->mBuildQueue = theOther->mBuildQueue;
    this->PlayEffect(theOther->mCurrentResearch);
    // the original assigns each of inUnit's containers to itself (0x1f7a9b/0x1f7aad: ecx and the argument are both
    // theOther + 0xe4 / + 0xf8), which clears them
    theOther->mAreaEffects = theOther->mAreaEffects;
    theOther->mAffectingAreaEffects = theOther->mAffectingAreaEffects;
    ee_units_complex_copy(2, this, theOther, NULL);   // the planet, production time, the AI's state
    this->mHomeBase = theOther->mHomeBase;
    if (this->mHomeBase != NULL)
        this->mHomeBase->Reference();
    this->mContainer = theOther->mContainer;
    if (this->mContainer != NULL) {
        this->mContainer->Reference();
        this->mContainer->ReplaceCargo(theOther, this);
    }
    this->RefreshGoals();
    this->RefreshActiveGoal();
    EEUnit::CopyFrom(inUnit);
}

// Replaces the unit's sound instance, referencing the new one
void EEComplexUnit::SetSound(SESMInstanceNode* inSound)
{
    this->ReleaseSound();
    this->mSound = inSound;
    if (inSound)
        inSound->Reference();
}

// New (debugging): EE_UNIT_POOL=0 takes every unit from operator new instead of the pool (UMemoryPool::Delete deletes
// what no chunk holds). A pool slot is reused at once, so a unit released one reference too early is read as the
// next unit given that slot, unseen by AddressSanitizer; from the heap, ASan reports the first stale read with the
// stack of the release that freed it (tools/play_posix.py sets it). The leak reports then list no unit.
static EEComplexUnit* EENewPoolUnit()
{
    static const bool sPoolOff = [] {
        const char* theValue = getenv("EE_UNIT_POOL");
        return theValue != NULL && theValue[0] == '0';
    }();
    return sPoolOff ? new EEComplexUnit : EEComplexUnit::sMemoryPool.New();
}

// Takes a unit from the pool and resets it to defaults
EEComplexUnit* EEComplexUnit::Create()
{
    EEComplexUnit* theUnit = EENewPoolUnit();
    theUnit->Reset();
    theUnit->ResetEEUnit();
    theUnit->ResetEEComplexUnit(true, false);
    return theUnit;
}

// Sizes the unit pool and the unit-list node pool (twice the chunk size); returns the unit pool's result
bool EEComplexUnit::SetPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks)
{
    UList<EEComplexUnit*>::sNodePool.SetSize(inChunkSize * 2, inMaxChunks, 3);
    return EEComplexUnit::sMemoryPool.SetSize(inChunkSize, inMaxChunks, 1);
}

// Takes a unit from the pool and initialises it for a class, placement, player and goal queue count
EEComplexUnit* EEComplexUnit::Create(EEUnitClass* inClass, const GETransformation& inTransformation,
                                     EEPlayer* inPlayer, unsigned long inGoalQueueCount)
{
    EEComplexUnit* theUnit = EENewPoolUnit();
    theUnit->Init(inClass, inTransformation, inPlayer, inGoalQueueCount);
    return theUnit;
}

// Reallocates the turret angle array with the given count, all zero
void EEComplexUnit::ResizeTurretAngles(unsigned long inCount)
{
    delete[] this->mTurretAngles;
    if (inCount == 0)
        this->mTurretAngles = NULL;
    else {
        this->mTurretAngles = new float[inCount];
        for (unsigned long i = 0; i < inCount; i++)
            this->mTurretAngles[i] = 0.0f;
    }
}

// Sets the water state of a sea or space unit from its class's water kind (and the tile's depth level)
void EEComplexUnit::UpdateWaterState(bool inArg)
{
    ee_units_cu_body(EE_UNITS_CU_DO_UPDATE_WATER_STATE, this, inArg, 0, 0);   // cu_misc.rs
}

// Adds the unit's sight at a tile for its player, sight sharers and allies, revealing it on the minimap (ee_units
// sight.rs over ee_units_sight_host)
void EEComplexUnit::SightOn(unsigned long inX, unsigned long inY)
{
    ee_units_sight(EEObj(this), (uint32_t)inX, (uint32_t)inY, 1);
}


// Abandons goals of a queue from the front until one that suits the unit starts
void EEComplexUnit::ClearGoals(long inIndex)
{
    this->SetGatheredResource(NULL);
    if (this->mGoalQueues.size() <= (unsigned long)inIndex)
        return;
    while (true) {
        if (!(this->mGoalQueues[inIndex].mGoals.size() > 0))
            break;
        this->mGoalQueues[inIndex].GetFront()->Abandon(this);
        this->mGoalQueues[inIndex].PopFront();
        if ((unsigned long)inIndex < 1)
            this->mActiveGoal = NULL;
        this->RefreshGoals();
        this->RefreshActiveGoal();
        if (this->mGoalQueues[inIndex].mGoals.size() <= 0)
            continue;
        EEUnitGoal* theGoal = this->mGoalQueues[inIndex].GetFront();
        if (theGoal->Suits(this)) {
            theGoal->Start(this);
            break;
        }
    }
    this->RefreshGoals();
    this->RefreshActiveGoal();
}

// Frees the unit's turret angles, orbitals, wheels and model, then the base unit's data
void EEComplexUnit::FreeNode()
{
    delete[] this->mTurretAngles;
    this->mTurretAngles = NULL;
    for (unsigned long i = 0; i < this->mNumOrbitals; i++)
        delete this->mOrbitals[i];
    delete[] this->mOrbitals;
    this->mOrbitals = NULL;
    this->mNumOrbitals = 0;
    for (unsigned long j = 0; j < this->mNumWheels; j++)
        delete this->mWheels[j];
    delete[] this->mWheels;
    this->mWheels = NULL;
    this->mNumWheels = 0;
    this->mPhysicalModel.SetModel(NULL);
    EEUnit::FreeNode();
}

// Kills every cargo unit and records each as a dead unit in its player's statistics
void EEComplexUnit::KillCargo()
{
    ee_units_cu_body(EE_UNITS_CU_DO_KILL_CARGO, this, 0, 0, 0);   // cu_cargo.rs
}

// Empties the garrison count and population
void EEComplexUnit::ClearGarrison()
{
    this->mGarrisonPopulation = 0;
    this->mGarrisonCount = 0;
}

// Dumps the unit pool's statistics and releases the pool
void EEComplexUnit::ReleasePool()
{
    UWideString theReport(1000);
    UWideString theLine;
    EEComplexUnit::sMemoryPool.DumpStats(false);
    EEComplexUnit::sMemoryPool.Release();
}

// Builds a "!Leaking Unit" line per used pool unit and clears its reference count (the report is gone)
// The reference-counter buckets are walked too, their report compiled away (provisional name, from Empire Earth.cpp)
void EEReportLeakingUnits()
{
    std::vector<EEComplexUnit*> theUnits;
    UReferenceCounter::CounterMap::iterator theCounter;
    UReferenceCounter::FileCountMap::iterator theFile;
    UWideString theText(1000);
    UWideString theLine;
    EEComplexUnit::sMemoryPool.GetUsed(theUnits);
    for (long i = (long)theUnits.size() - 1; i >= 0; i--) {
        theText.Clear();
        theLine = "!Leaking Unit: ";
        sprintf(theText, "%s at (%ld,%ld). UnitID: %ld with action:%s\n", (const char*)theUnits[i]->GetUnitClass()->mName,
                theUnits[i]->GetTileX(), theUnits[i]->GetTileY(), theUnits[i]->GetUnitID(),
                (const char*)EEUnitAction::GetActionName(theUnits[i]->mCommand->mType));
        theLine += (const char*)theText;
        theUnits[i]->mReferenceCount = 0;
    }
    UReferenceCounter::CounterMap* theCounters = UReferenceCounter::GetBucketMap(0);
    if (theCounters == NULL)
        return;
    for (theCounter = theCounters->begin(); theCounter != theCounters->end(); theCounter++) {
        UReferenceCounter::FileCountMap* theFiles = theCounter->second;
        EEUnit* theUnit = dynamic_cast<EEUnit*>(theCounter->first);
        if (theFiles != NULL && theUnit != NULL) {
            for (theFile = theFiles->begin(); theFile != theFiles->end(); theFile++) {
            }
        }
    }
}

// Writes every used pool unit to the dump file with its class, tile, id, action and growth
// Part of EEWorld's dump; the second copy of the arguments fed a trace the shipping build compiled away
// (provisional name)
void EEDumpLeakedUnits(FSFileSpec& inFile)
{
    std::vector<EEComplexUnit*> theUnits;
    EEComplexUnit::sMemoryPool.GetUsed(theUnits);
    fprintf(inFile, "\n\n**********LEAKED UNITS*********\n\n");
    for (long i = (long)theUnits.size() - 1; i >= 0; i--) {
        fprintf(inFile, "Leaking Unit: %s at (%ld,%ld). UnitID: %ld with action:%s, with %6.6f growth rate\n",
                (const char*)theUnits[i]->GetUnitClass()->GetName(), theUnits[i]->GetTileX(), theUnits[i]->GetTileY(),
                theUnits[i]->GetUnitID(), (const char*)EEUnitAction::GetActionName(theUnits[i]->mCommand->mType),
                theUnits[i]->GetGrowth());
    }
}

// the Rust dump's text into the out-of-sync file
static void EEDumpWrite(void* inFile, const char* inText, size_t inLength)
{
    fwrite(inText, 1, inLength, static_cast<FILE*>(inFile));
}

// Writes the unit's goals, physics, class stats, waypoints and area effects to the out-of-sync dump
void EEComplexUnit::OutOfSynchDump(FILE* inFile, unsigned long inFlags)
{
    GE3DPoint theLocation;
    std::deque<EEUnitGoal*> theGoals;
    // the record's lines (ee_units unit_bulk.rs dump, parts 0 and 1), the goals', waypoints' and area effects' here
    EeUnitsComplexDumpIn theIn;
    theIn.player_id = this->mPlayer->mPlayerID;
    theIn.tile_x = this->GetTileX();
    theIn.tile_y = this->GetTileY();
    theIn.angular_velocity[0] = this->mAngularVelocity.GetYaw();
    theIn.angular_velocity[1] = this->mAngularVelocity.GetPitch();
    theIn.angular_velocity[2] = this->mAngularVelocity.GetRoll();
    theIn.build_queue = this->mBuildQueue.size();
    theIn.citizen_build_queue = this->mCitizenBuildQueue.size();
    theIn.collision_tile[0] = this->mCollisionTile.mX;
    theIn.collision_tile[1] = this->mCollisionTile.GetY();
    theIn.frame = this->mPhysicalModel.GetFrame();
    theIn.projectile_start[0] = this->mProjectileStart.GetX();
    theIn.projectile_start[1] = this->mProjectileStart.GetY();
    theIn.projectile_start[2] = this->mProjectileStart.GetZ();
    for (unsigned long r = 0; r < 4; r++)
        for (unsigned long c = 0; c < 4; c++)
            theIn.location[r][c] = this->mPhysicalModel.GetLocation().GetElement(r, c);
    EEUnitClass* theClass = this->GetUnitClass();
    ee_units_complex_dump(0, this, this, this, theClass, this->GetStats(), &theIn, &EEDumpWrite, inFile);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Action:             %s(%ld)\n", (const char*)EEUnitAction::GetActionName(this->GetComplexUnit()->GetCommandType()),
        this->GetComplexUnit()->GetCommandType());
    if (this->GetComplexUnit()->mActiveGroup == NULL)
        fprintf(inFile, "OUT OF SYNCH DUMP:  This unit has no active group\n");
    else
        fprintf(inFile, "OUT OF SYNCH DUMP:  This unit has an active group with an action of %ld\n", this->GetComplexUnit()->mActiveGroup->mFields.mAction);
    fprintf(inFile, "OUT OF SYNCH DUMP:  **************    GOALS        *************\n");
    for (unsigned long i = 0; i < this->GetComplexUnit()->mGoalQueues.size(); ++i) {
        theGoals = this->GetComplexUnit()->mGoalQueues[i].mGoals.Get();
        fprintf(inFile, "OUT OF SYNCH DUMP:  ************** GOAL Queue(%6ld) Number Of Goals(%6zu) *************\n", i, theGoals.size());
        for (unsigned long j = 0; j < theGoals.size(); ++j) {
            fprintf(inFile, "OUT OF SYNCH DUMP:  Goal Number(%6ld) Has a type of:\t%6d\n", j, (theGoals[j])->mKind);
            (theGoals[j])->DumpOutOfSynch(inFile);
            (theGoals[j])->GetGoalLocation(theLocation);
            fprintf(inFile, "OUT OF SYNCH DUMP:  Goal Location (x): %6.6f\n", (double)theLocation.GetX());
            fprintf(inFile, "OUT OF SYNCH DUMP:  Goal Location (y): %6.6f\n", (double)theLocation.GetY());
            fprintf(inFile, "OUT OF SYNCH DUMP:  Goal Location (z): %6.6f\n", (double)theLocation.GetZ());
            if ((theGoals[j])->mKind == kGoalLocation) {
                if (static_cast<EEUGRust*>(theGoals[j])->GetIsAutoWaypoint() == true)
                    fprintf(inFile, "OUT OF SYNCH DUMP:  Location Goal Is An AutoWaypoint\n");
                else
                    fprintf(inFile, "OUT OF SYNCH DUMP:  Location Goal Is Not An AutoWaypoint\n");
            }
            (theGoals[j])->DumpOutOfSynch(inFile);
        }
    }
    if (this->GetComplexUnit()->mGoalState != NULL) {
        fprintf(inFile, "OUT OF SYNCH DUMP:  Current Goal:\t%6d\n", this->GetComplexUnit()->mGoalState->mKind);
        this->GetComplexUnit()->mGoalState->GetGoalLocation(theLocation);
        fprintf(inFile, "OUT OF SYNCH DUMP:  Goal Location (x): %6.6f\n", (double)theLocation.GetX());
        fprintf(inFile, "OUT OF SYNCH DUMP:  Goal Location (y): %6.6f\n", (double)theLocation.GetY());
        fprintf(inFile, "OUT OF SYNCH DUMP:  Goal Location (z): %6.6f\n", (double)theLocation.GetZ());
    }
    ee_units_complex_dump(1, this, this, this, theClass, this->GetStats(), &theIn, &EEDumpWrite, inFile);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Waypoint Size:%zu\n", this->GetComplexUnit()->mWaypoints.size());
    for (unsigned long theWaypoint = 0; theWaypoint < this->GetComplexUnit()->mWaypoints.size(); ++theWaypoint)
        fprintf(inFile, "OUT OF SYNCH DUMP:  Waypoint[%ld]: X:%f, Y:%f, Z:%f\n", theWaypoint,
            (double)this->GetComplexUnit()->mWaypoints[theWaypoint].mLocation.GetX(),
            (double)this->GetComplexUnit()->mWaypoints[theWaypoint].mLocation.GetY(),
            (double)this->GetComplexUnit()->mWaypoints[theWaypoint].mLocation.GetZ());
    fprintf(inFile, "OUT OF SYNCH DUMP:  **************************** Unit Area Effects ********************\n\n");
    if (this->mAffectingAreaEffects.GetSize() != 0) {
        fprintf(inFile, "OUT OF SYNCH DUMP:  **************************** Unit Area Effects Affected By ********************\n");
        for (std::vector<EEAreaEffect*>::iterator theEffect = this->mAffectingAreaEffects.mItems.begin();
             theEffect != this->mAffectingAreaEffects.mItems.end(); ++theEffect)
            (*theEffect)->Dump(inFile);
    }
    if (this->mAreaEffects.GetSize() == 0)
        return;
    fprintf(inFile, "\nOUT OF SYNCH DUMP:  **************************** Unit Area Effects Generated ********************\n");
    for (std::vector<EEAreaEffect*>::iterator theEffect = this->mAreaEffects.mItems.begin();
         theEffect != this->mAreaEffects.mItems.end(); ++theEffect)
        (*theEffect)->Dump(inFile);
}

// Applies the built unit's area effects to the units within the largest effect radius
void EEComplexUnit::ApplyAreaEffects()
{
    float theRadius = 0.0f;
    GE2DPoint theLocation(0.0f, 0.0f);
    unsigned long thePlayerID = this->mPlayer->mPlayerID;
    if (!(this->mAreaEffects.GetSize() > 0) || !this->mIsBuilt)
        return;
    for (std::vector<EEAreaEffect*>::iterator theIterator = this->mAreaEffects.mItems.begin(); theIterator != this->mAreaEffects.mItems.end(); ++theIterator) {
        float theEffectRadius = (*theIterator)->GetRadius();
        if (theEffectRadius > theRadius)
            theRadius = theEffectRadius;
    }
    GE3DPoint thePoint(this->mPhysicalModel.GetLocation().GetX(), this->mPhysicalModel.GetLocation().GetY(), this->mPhysicalModel.GetLocation().GetZ());
    theLocation.SetX(thePoint.GetX());
    theLocation.SetY(thePoint.GetY());
    ::gAEManager.CollectEffects(theRadius, theLocation, thePlayerID, &this->mAreaEffects, this);
}

// Adds the finished building's sight and, for other players' units, shows it on the minimap
void EEComplexUnit::PartFinished()
{
    this->SightOn(this->GetTileX(), this->GetTileY());
    if (::gWorld.GetCurrentPlayer() != this->mPlayer)
        gMiniMap->MoveUnit(this, -10000, -10000, this->GetTileX(), this->GetTileY());
}

// Returns the unit's line of sight: its upgraded stat plus its sight bonus
float EEComplexUnit::GetLineOfSight()
{
    return this->GetStats()->mLineOfSight + this->mSight;
}

// Returns whether an area effect of the given kind acts on the unit
bool EEComplexUnit::HasEffect(long inKind)
{
    EEGraphicEffect* theEffect = NULL;
    return this->GetEffect(inKind, theEffect);
}

// Finds the graphic effect of the first area effect of a kind acting on the unit; returns whether found
bool EEComplexUnit::GetEffect(long inKind, EEGraphicEffect*& outEffect)
{
    bool theResult = false;
    for (std::vector<EEAreaEffect*>::iterator theIterator = this->mAffectingAreaEffects.mItems.begin(); theIterator != this->mAffectingAreaEffects.mItems.end(); ++theIterator) {
        if ((*theIterator)->mTemplate->mKind != inKind)
            continue;
        outEffect = (*theIterator)->mTemplate->mGraphicEffect;
        theResult = true;
        break;
    }
    return theResult;
}

// Returns whether the unit may be deleted; a built unit of the citizen tech's types not with one capitol
bool EEComplexUnit::CanDelete()
{
    bool theResult = this->mIsDeletable;
    if (theResult && this->GetPlayer()->GetTechTree()->Get(kTechCitizen)->Contains(this->GetUnitClass()->GetUnitType())) {
        theResult = true;
        if (this->mIsBuilt && this->mPlayer->mNumCapitols == 1)
            theResult = false;
    }
    return theResult;
}

// Returns whether a unit may pass through this gate (closed and locked rules, Trojan horse) or tile blocker
bool EEComplexUnit::IsPassableBy(EEComplexUnit* inUnit)
{
    if (this->GetUnitClass()->mCategory == kUnitCategoryGate && this->mHitPoints > 0) {
        EEComplexUnit* theCarrier = this;
        while (theCarrier->mPreviousPart != NULL)
            theCarrier = theCarrier->mPreviousPart;
        if (theCarrier != NULL && theCarrier->mIsDisabled && !theCarrier->IsProtected())
            return false;
        EEUnitClass* theGate = EEUnitClassOfKind<EE_CLASS_KIND_GATE>(this->GetUnitClass());
        if (theGate->IsSidePart(this))
            return false;
        if ((this->mCommand ? this->mCommand->mType : 0) == kActionClosed) {
            if (inUnit == NULL)
                return false;
            if ((inUnit->mPlayer != this->mPlayer && this->mPlayer->mEnemies[inUnit->mPlayer->mPlayerID] != 0) || this->mIsLocked) {
                if (inUnit->mPlayer == this->mPlayer || this->mPlayer->mEnemies[inUnit->mPlayer->mPlayerID] == 0
                    || inUnit->mUnitClass->GetUnitType() != kUnitTypeX04TrojanHorse)
                    return false;
            }
        }
    } else if (this->GetUnitClass()->mBlocksTile && !this->IsFoundation() && this->mHitPoints > 0)
        return false;
    return true;
}

// Returns the cargo unit with the given id, or NULL
EEComplexUnit* EEComplexUnit::FindPassenger(unsigned long inUnitID)
{
    EEComplexUnit* theResult = NULL;
    unsigned long theCount = this->GetNumCargoUnits();
    for (unsigned long i = 0; i < theCount; i++) {
        if (inUnitID == this->GetCargo(i)->mUnitID) {
            theResult = this->GetCargo(i);
            break;
        }
    }
    return theResult;
}

// Moves a tile position to the centre of the unit's footprint
void EEComplexUnit::AddCenterOffset(float& ioX, float& ioY)
{
    EEUnitClass* theClass = this->GetUnitClass();
    long theSize = theClass->GetTileSize();
    if (theSize <= 1) {
        ioX += 0.5f;
        ioY += 0.5f;
    } else {
        ioX += theSize * 0.5f;
        ioY += theClass->GetTileSize() * 0.5f;
    }
}

// Returns the unit's out-of-sync checksum for the selected parts (tile, physics, goals, area effects)
long EEComplexUnit::GetChecksum(unsigned long inFlags)
{
    // the float members are summed as their bit patterns
    std::deque<EEUnitGoal*> theGoals;
    long theSum = 0;
    if (inFlags & 1)
        theSum = this->mHitPoints + this->GetTileY() + this->GetTileX();
    if (inFlags & 2) {
        EEComplexUnit* theUnit = this->mComplexUnit;
        theSum += theUnit->mTimesUpdated;
        float theValue = theUnit->mPhysicalModel.GetLocation().GetX();
        theSum += (theUnit->mActiveGroup != NULL) + (theUnit->mCommand ? theUnit->mCommand->mType : 0) + *reinterpret_cast<int32_t*>(&theValue);
        theValue = theUnit->mPhysicalModel.GetLocation().GetY();
        theSum += *reinterpret_cast<int32_t*>(&theValue);
        theValue = theUnit->mPhysicalModel.GetLocation().GetZ();
        theSum += *reinterpret_cast<int32_t*>(&theValue);
        theValue = theUnit->mHeight;
        theSum += *reinterpret_cast<int32_t*>(&theValue);
    }
    if (inFlags & 0x20) {
        for (unsigned long i = 0; i < this->mComplexUnit->GetNumGoalQueues(); i++) {
            theGoals = this->mComplexUnit->GetQueueGoals(i).Get();
            for (unsigned long j = 0; j < theGoals.size(); j++)
                theSum += theGoals[j]->GetOutOfSynchChecksum();
        }
    }
    if (inFlags & 8) {
        std::vector<EEAreaEffect*>::iterator theEffect; for (theEffect = this->mAffectingAreaEffects.mItems.begin(); theEffect != this->mAffectingAreaEffects.mItems.end();
             ++theEffect)
            theSum += (*theEffect)->Checksum();
        for (theEffect = this->mAreaEffects.mItems.begin(); theEffect != this->mAreaEffects.mItems.end(); ++theEffect)
            theSum += (*theEffect)->Checksum();
    }
    return EEUnit::GetChecksum(inFlags) + theSum;
}

// Returns the goals of a goal queue
EEGoalDeque& EEComplexUnit::GetQueueGoals(unsigned long inIndex)
{
    return this->mGoalQueues[inIndex].mGoals;
}

// Appends a debug description: action, collision state, velocity and the current goal
void EEComplexUnit::Describe(UWideString& ioString)
{
    UWideString theString;
    EEUnitGoal* theGoal = this->mGoalState;
    EEUnit::Describe(theString);
    ioString += theString;
    if (this->mCommand) {
        sprintf(theString, "act %s:", (const char*)EEUnitAction::GetActionName(this->mCommand->mType));
        ioString += theString;
    }
    sprintf(theString, "coll %d", this->mCollisionState);
    ioString += theString;
    if (this->mCollisionState != 0) {
        sprintf(theString, " / tile(%ld,%ld)", this->mCollisionTile.GetX(), this->mCollisionTile.GetY());
        ioString += theString;
    }
    ioString += ":";
    sprintf(theString, "vel %.4f:", this->mVelocity);
    ioString += theString;
    if (!theGoal)
        return;
    theString.Clear();
    theGoal->GetDebugLine(theString);
    ioString += theString;
}

// Returns the unit's (or its container's) tile, or a nearby tile on a continent of the wanted kind
EETileCoordinate EEComplexUnit::FindAreaTile(bool inFlag, long inRange)
{
    EEComplexUnit* theUnit = this->mContainer;
    if (theUnit == NULL)
        theUnit = this;
    EEAIContinent* theContinent = ::gContinentManager.GetContinentAt(theUnit);
    long theSize = theUnit->mUnitClass->mTileSize;
    EETileCoordinate theResult;
    theResult = theUnit->GetTile();
    if (theSize > 1 && (theContinent == NULL || theContinent->GetCategory() != inFlag)) {
        long theEndX = this->GetTileX() + theSize + inRange;
        long theEndY = this->GetTileY() + theSize + inRange;
        for (long x = this->GetTileX() - inRange; x < theEndX; x++) {
            for (long y = this->GetTileY() - inRange; y < theEndY; y++) {
                if (!((unsigned long)x < gMapWidth) || !((unsigned long)y < gMapHeight))
                    continue;
                unsigned short theID = ::gContinentManager.GetContinentID(x, y);
                EEAIContinent* theOther = theID != 0 ? ::gContinentManager.GetContinentByID(theID) : NULL;
                if (theOther == NULL || theOther->GetCategory() != inFlag)
                    continue;
                theResult.mX = x;
                theResult.mY = y;
                x = theEndX;
                break;
            }
        }
    }
    return theResult;
}


// Returns the unit's model, or the player's foundation graphic for a foundation
EEModel* EEComplexUnit::GetModel()
{
    if (!this->mIsFoundation)
        return this->GetStats()->mModel;
    return EEGetPlayerClassGraphic(this->GetPlayer(), this->GetUnitClass());
}

// Returns the unit's world position
void EEComplexUnit::GetPosition(GE3DPoint& outPosition)
{
    this->mPhysicalModel.GetLocation().GetPosition(outPosition);
}

// Returns the height of the model's top for effects, scaled by build progress while unbuilt
float EEComplexUnit::GetEffectHeight(bool inUseCurrentFrame)
{
    GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
    if (inUseCurrentFrame)
        this->mPhysicalModel.GetLocation().Apply(
            this->GetModel()->mModels[this->GetAnimation()]->GetBoundingBox(this->mPhysicalModel.GetFrame()).GetTopRightBack(), thePoint);
    else
        this->mPhysicalModel.GetLocation().Apply(this->GetModel()->mModels[this->GetAnimation()]->GetBoundingBox(0).GetTopRightBack(), thePoint);
    float theHeight = this->GetStats()->GetScale() * thePoint.GetZ();
    if (!this->mIsBuilt)
        theHeight *= (float)this->mHitPoints / this->GetStats()->mMaxHitPoints;
    return theHeight;
}

// Returns whether a target is ignored (optionally only from the same tiles), dropping expired entries
bool EEComplexUnit::IsTargetIgnored(EEComplexUnit* inTarget, bool inCheckLocation)
{
    // ee_combat world.rs is_target_ignored over the world host; CBXR records the outcome
    bool theResult = EECombatWorld(inCheckLocation ? EE_COMBAT_WORLD_IS_IGNORED : EE_COMBAT_WORLD_IS_IGNORED_ANYWHERE, this,
                                   inTarget) != 0;
    EECombatCaptureIgnored(EE_COMBAT_XR_IS_IGNORED, this, inTarget, theResult, inCheckLocation);
    return theResult;
}

// Returns whether the unit is disabled by a time warp (reasons 0x1a / 0x1c)
bool EEComplexUnit::IsProtected()
{
    return this->mIsDisabled && (this->mDisabledReason == 0x1a || this->mDisabledReason == 0x1c);
}

// Returns whether the unit's current goal is gathering
bool EEComplexUnit::IsGathering()
{
    return this->mGoalState != NULL && this->mGoalState->mKind == kGoalGather;
}

// Returns whether another unit is on another continent (space docks and turrets by their planet)
bool EEComplexUnit::IsOnOtherContinentThan(EEComplexUnit* inUnit)
{
    if (this->IsOnOtherContinent(inUnit->GetTileX(), inUnit->GetTileY())) {
        unsigned long theType = inUnit->mUnitClass->GetUnitType();
        if (theType == kUnitTypeSpaceDock || theType == kUnitTypeSpaceTurret || theType == kUnitTypeOrbitalSpaceStation) {
            unsigned short theContinent = inUnit->GetComplexUnit()->mPlanetID;
            if (::gContinentManager.GetContinent(this, kAIPathNonSpace) != theContinent)
                return true;
            return false;
        }
        return true;
    }
    return false;
}

// Returns whether a tile is out of reach: unreachable for air, else on another planet in a space map
bool EEComplexUnit::IsOnOtherContinent(long inTileX, long inTileY)
{
    if (this->GetUnitClass()->mDomains & kMovementAir)
        return !::gContinentManager.CanGetTo(this, inTileX, inTileY);
    return ::gWorld.mMap->mTerrainMesh.GetIsSpaceMap() && !(this->GetUnitClass()->mDomains & kMovementSpace)
           && this->GetUnitClass()->GetUnitType() != kUnitTypeSpaceTurret
           && ::gContinentManager.GetContinent(this, kAIPathNonSpace)
                  != ::gContinentManager.GetContinent(EETileCoordinate(inTileX, inTileY), kAIPathNonSpace);
}

// Returns whether the unit can get to a target, directly or through connected continents
bool EEComplexUnit::CanReachUnit(EEUnit* inTarget)
{
    // ee_combat world.rs can_reach_unit over the world host; CBXR records the outcome
    bool theResult = EECombatWorld(EE_COMBAT_WORLD_CAN_REACH_UNIT, this, inTarget) != 0;
    EECombatCaptureXRead(EE_COMBAT_XR_CAN_REACH_UNIT, this, inTarget, theResult);
    return theResult;
}

// Returns the player's town containing the unit's tile, else the town whose outline is nearest
EETown* EEComplexUnit::FindNearestTown()
{
    U2DPoint<long> theTile(this->GetTileX(), this->GetTileY());
    U2DPoint<long> thePoint;
    EETown* theResult = NULL;
    long theBestDistance = 0x7fffffff;
    long theCount = this->mPlayer->GetNumTowns();
    for (long i = 0; i < theCount; ++i) {
        EETown* theTown = this->mPlayer->mTowns[i];
        if (theTown->Contains(theTile)) {
            theResult = theTown;
            break;
        }
        thePoint = theTown->GetHullClosestOutlinePoint(theTile);
        long theDistance = theTile.DistanceToSquared(thePoint);
        if (theDistance < theBestDistance) {
            theResult = theTown;
            theBestDistance = theDistance;
        }
    }
    return theResult;
}

// Makes the unit fanatic for a while: costs a share of its hit points (at least 1 left) and tints it
void EEComplexUnit::GoFanatic()
{
    if (this->IsCloaked())
        return;
    this->mCloaked = gPhysics->mGameTime + gWorldTable.GetLongValue(kWorldFanaticism);
    long theHitPoints = (1.0f - gWorldTable.GetFloatValue(kWorldFanaticism)) * this->mHitPoints;
    if (theHitPoints < 1)
        theHitPoints = 1;
    this->SetHitPoints(theHitPoints);
    this->SetIsTinted(true);
}

// Returns whether the unit's fanaticism timer is running
bool EEComplexUnit::IsCloaked()
{
    return this->mCloaked > 0;
}

// Removes the unit's colour tint
void EEComplexUnit::ClearTint()
{
    this->SetIsTinted(false);
}

// Pulses a fanatic unit's colour between the fanaticism effect's two colours; ends it when time runs out
void EEComplexUnit::UpdateFanaticismColor()
{
    static GEColor sCloakedColor(0.9f, 0.0f, 0.0f);
    static GEColor sBaseColor(0.6f, 0.5f, 0.5f);
    static GEColor sColor(0.5f, 0.5f, 0.5f);
    if (!this->IsCloaked())
        return;
    unsigned long theNow = gPhysics->mGameTime;
    if (theNow > this->mCloaked) {
        this->ClearTint();
        this->mCloaked = 0;
    } else {
        if (theNow != sU563238) {
            EEGraphicEffect* theEffect = EEGraphicEffect::Get(kGraphicEffectFanaticismEffect);
            long thePeriod = theEffect->mIntParam1;
            sCloakedColor.SetRed(theEffect->mParam1);
            sCloakedColor.SetGreen(theEffect->mParam2);
            sCloakedColor.SetBlue(theEffect->mParam3);
            sBaseColor.SetRed(theEffect->mParam4);
            sBaseColor.SetGreen(theEffect->mParam5);
            sBaseColor.SetBlue(theEffect->mParam6);
            long thePhase = (this->mCloaked - gPhysics->mGameTime) % (unsigned long)(thePeriod * 2);
            if (thePhase > thePeriod)
                thePhase = thePeriod * 2 - thePhase;
            sU563238 = theNow;
            float theFraction = (float)thePhase / thePeriod;
            float theRest = 1.0f - theFraction;
            sColor.mRed = sBaseColor.mRed * theRest + sCloakedColor.mRed * theFraction;
            sColor.mGreen = sBaseColor.mGreen * theRest + sCloakedColor.mGreen * theFraction;
            sColor.mBlue = sBaseColor.mBlue * theRest + sCloakedColor.mBlue * theFraction;
        }
        this->SetColor(sColor);
    }
}

// Returns whether a tile is the unit's tile or one of the side or corner tiles around its footprint
bool EEComplexUnit::IsNextToTile(long inX, long inY)
{
    std::vector<U2DPoint<long> > theTiles;
    long theX = this->GetTileX();
    long theY = this->GetTileY();
    bool theFound = false;
    if (theX == inX && theY == inY)
        theFound = true;
    else {
        ee_units_uu_square_ring(theX, theY, this->GetUnitClass()->mTileSize, this->GetUnitClass()->mTileSize, 1, &theTiles);
        std::vector<U2DPoint<long> >::iterator theTile = theTiles.begin();
        while (theTile != theTiles.end() && !theFound) {
            theFound = inX == theTile->GetX() && inY == theTile->GetY();
            ++theTile;
        }
        if (!theFound) {
            ee_units_uu_ring_corners(0, theX, theY, this->GetUnitClass()->mTileSize, this->GetUnitClass()->mTileSize, 1, &theTiles);
            theTile = theTiles.begin();
            while (theTile != theTiles.end() && !theFound) {
                theFound = inX == theTile->GetX() && inY == theTile->GetY();
                ++theTile;
            }
        }
    }
    return theFound;
}


// Resets every mobile-unit field to defaults with the given goal queue count, optionally a random heading
void EEComplexUnit::ResetEEComplexUnit(unsigned long inGoalQueueCount, bool inRandomHeading)
{
    this->mBuildQueue.clear();
    this->mCitizenBuildQueue.clear();
    this->mBoundedPathTiles.clear();
    this->mCargo.clear();
    this->mWaypoints.clear();
    this->mGoalQueues.clear();
    this->mAreaEffects.Clear();
    this->mAreaEffects.mItems.clear();
    this->mAffectingAreaEffects.Clear();
    this->mAffectingAreaEffects.mItems.clear();
    this->mUnitVariables.clear();
    this->mUnitVariables.resize(3, 0);
    this->mLastMapPosition = GE3DPoint(0.0f, 0.0f, 0.0f);
    this->mU0DC.SetX(-10000.0f);
    this->mU0DC.SetY(-10000.0f);
    // the record's members (every one but three; ee_units unit_bulk.rs reset), from the class and stats as the
    // original read them (the heading's draw is the original's only one here)
    EEUnitClass* theClass = this->GetUnitClass();
    bool theHasStats = theClass != NULL && this->mPlayer != NULL;
    EeUnitsComplexResetIn theIn = {inRandomHeading ? gPhysics->mRandom.Generate() * 6.2831855f : 6.2831855f,
                                   theHasStats ? this->GetStats()->mReloadTime : 0,
                                   theHasStats ? this->GetStats()->mPowerRegeneration : 0,
                                   theClass != NULL ? theClass->mInitialGarrisonCount : 0,
                                   theClass != NULL && theClass->mIsCloakable,
                                   theClass != NULL && !theClass->IsLandBuilding()};
    ee_units_complex_reset(this, &theIn);
    this->mBehavior = theClass != NULL ? theClass->mBehavior : EEUnitBehavior::Get(5);
    this->mLauncherClass = NULL;
    this->mOwner = NULL;
    this->mActiveGroup = NULL;
    this->mPreviousPart = NULL;
    this->mNextPart = NULL;
    this->mHomeBase = NULL;
    this->mContainer = NULL;
    this->mSound = NULL;
    this->mCollisionTile.mX = -10000;
    this->mCollisionTile.mY = -10000;
    this->mProjectileStart = GE3DPoint(0.0f, 0.0f, 0.0f);
    this->mActiveGoal = NULL;
    this->mGoalState = NULL;
    this->mConversionEffect.mValue = -1;
    this->mHealEffect.mValue = -1;
    this->mBlockingUnit = NULL;
    this->mPatrolGroup = NULL;
    this->mTurretAngles = NULL;
    this->mOrbitals = NULL;
    this->mWheels = NULL;
    this->mGatheredResource = NULL;
    this->mLastResource = NULL;
    this->mCommand = NULL;
    this->mLastAttacker = NULL;
    this->mPhysicalModel.SetModel(NULL);
    this->mPhysicalModel.SetScale(1.0f);
    this->mPhysicalModel.SetLocation(GETransformation());
    this->mPhysicalModel.SetFrame(0);
    this->mAngularVelocity.SetPitch(0.0f);
    this->mAngularVelocity.SetYaw(0.0f);
    this->mAngularVelocity.SetRoll(0.0f);
    this->mOrientation = this->mAngularVelocity;
    this->mColor.SetBlue(1.0f);
    this->mColor.SetGreen(1.0f);
    this->mColor.SetRed(1.0f);
    this->mMine = NULL;
    this->mGoalQueues.resize(inGoalQueueCount, EEUnitVectorItem(this));
    this->ReleaseHeldUnits();
    this->mHerd = NULL;
    this->mHolder = NULL;
    this->mFireEffect.mValue = 0;
}

// Initialises a new unit from its class, placement and player: collision, hiding, tile, scale, area effects
void EEComplexUnit::Init(EEUnitClass* inClass, const GETransformation& inPlacement,
                         EEPlayer* inPlayer, unsigned long inGoalQueueCount)
{
    GETransformation thePlacement(inPlacement);
    EEUnit::Init(inClass, inPlayer);
    this->ResetEEComplexUnit(inGoalQueueCount, true);
    this->mIsCollidable = inClass->mIsCollidable &&
        (!(inClass->mDomains & kMovementAir) || inClass->FliesOver()) &&
        !inClass->mIsProjectile && inClass->mUnitType != kUnitTypeWaypoint;
    this->mCanBeHidden = (!(inClass->mDomains & kMovementAir) && !inClass->mIsObstacle &&
        !inClass->mIsProjectile && inClass->mMaxVelocity > 0.0f) || inClass->mFamily == kUnitFamilyAABuilding;
    this->MoveTo(UMFloor(thePlacement.GetX()), UMFloor(thePlacement.GetY()), false);
    this->UpdateWaterState(false);
    if (this->GetUnitClass()->mTileSize > 1) {
        float theOffset = inClass->mTileSize * 0.5f - 0.5f;
        thePlacement.SetX(thePlacement.GetX() + theOffset);
        thePlacement.SetY(thePlacement.GetY() + theOffset);
    }
    this->mPhysicalModel.SetLocation(thePlacement);
    this->mPhysicalModel.SetScale(inClass->mScale);
    this->mCargoRoom = inClass->mTransportCapacity;
    this->mCollisionRadius = inClass->GetInnerCollisionSphere();
    this->mIsCitizen = inClass->mCategory == kUnitCategoryLandCitizen || inClass->mCategory == kUnitCategoryNavalCitizen;
    this->mIsDeletable = inClass->mIsDeletable;
    for (long i = 0; i < 4; ++i) {
        long theTemplate = inPlayer->GetUpgrade(this->GetUnitClass()->GetUnitType())->mAreaEffectTemplates[i];
        if (theTemplate <= 0)
            continue;
        EEAreaEffect* theEffect = gAEManager.CreateAreaEffect(theTemplate, this);
        this->mAreaEffects.Add(theEffect);
        theEffect->Dereference();
    }
}

// Kills or removes the unit: records death statistics and insurance, releases cargo, garrison, holder, goals
// and effects, then kills its later parts and spawns its death and remains units
void EEComplexUnit::Kill(bool inKilled, bool inKeepResearch, bool inOther)
{
    ee_units_cu_body(EE_UNITS_CU_DO_KILL, this, inKilled | inKeepResearch << 1 | inOther << 2, 0, 0);   // cu_body.rs
}



// Returns the unit to the complex-unit memory pool when its last reference goes
void EEComplexUnit::LastDereference()
{
    this->FreeNode();
    this->mReferenceCount = 0;
    EEComplexUnit::sMemoryPool.Delete(this);
}

// Loads the unit's model (the foundation model, or the animation model with turret angles, orbitals and wheels)
// and, except for land buildings, the player's buttons for its class
void EEComplexUnit::LoadMedia(bool inIsFoundation)
{
    ee_units_cu_body(EE_UNITS_CU_DO_LOAD_MEDIA, this, inIsFoundation, 0, 0);   // cu_misc.rs
}

// Moves an idle or gathering unit to a random free tile around a footprint being placed (a waiting gatherer looks for
// another gather spot instead); returns true if it was sent away
bool EEComplexUnit::StepAside(EEPlayer* inPlayer, EEUnitClass* inClass, long inX, long inY, bool inForce)
{
    std::vector<EETileCoordinate> theTiles;
    GE2DPoint thePoint;
    EETileCoordinate theTile;
    long theCommand = this->mCommand != NULL ? this->mCommand->mType : 0;
    bool theUnarmed = this->GetUnitClass()->mFamily == kUnitFamilyAnimal && this->GetStats()->mAttack == 0.0f;
    bool theResult = false;
    if ((theCommand == kActionIdle || theCommand == kActionWaitForOpenPosition || theCommand == kActionGatherResource) && this->GetStats()->mMaxVelocity > 0.0f &&
        inPlayer != NULL && (inPlayer == this->mPlayer || this->mPlayer->mEnemies[inPlayer->mPlayerID] == 0 ||
                          theUnarmed || inForce)) {
        if (theCommand == kActionWaitForOpenPosition) {
            // (the port: a unit waiting for a spot whose goal is no longer a gather - the original dereferenced the
            // null cast; seen when a repair began beside such a unit, PORT_LEARNINGS)
            EEUGRust* theGather = EEGatherGoalCast(this->GetGoal());
            if (theGather != NULL && theGather->FindGatherSpot(this)) {
                this->StartAction(kActionMoveToWaitingArea);
                this->GetUnitClass()->ActionChanged(this);
            }
        } else {
            long theSize = inClass->mTileSize;
            long theMinX = inX - theSize - 1;
            long theMaxX = inX + theSize + 1;
            long theMinY = inY - theSize - 1;
            long theMaxY = inY + theSize + 1;
            for (long x = theMinX; x <= theMaxX; ++x) {
                if (x != theMinX && x != theMaxX) {
                    theTiles.push_back(EETileCoordinate(x, theMinY));
                    theTiles.push_back(EETileCoordinate(x, theMaxY));
                } else {
                    for (long y = theMinY; y <= theMaxY; ++y)
                        theTiles.push_back(EETileCoordinate(x, y));
                }
            }
            EEAIRandomShuffle(theTiles.begin(), theTiles.end());
            bool theFound = false;
            while (theTiles.size() != 0) {
                theTile = theTiles.back();
                theTiles.pop_back();
                theFound = ee_units_uu_can_stand_on(theTile.mX, theTile.mY, (uint64_t)(uintptr_t)this);
                if (theFound)
                    break;
            }
            if (theFound == true) {
                thePoint.SetX((float)theTile.mX + 0.5f);
                thePoint.SetY((float)theTile.GetY() + 0.5f);
                EEUnitGoal* theGoal = EEUGRust::NewLocation(thePoint, false, true, false, 0.0f, false, 0);
                this->SetGoal(theGoal, theCommand == kActionGatherResource, 0);
                this->StartAction(kActionPrepareToMove);
                this->GetUnitClass()->ActionChanged(this);
                theResult = true;
            }
        }
    }
    return theResult;
}

// Returns true if the unit is built, alive and below its maximum hit points
bool EEComplexUnit::IsRepairable()
{
    return this->mHitPoints > 0 && this->mHitPoints < this->GetStats()->mMaxHitPoints && this->mIsBuilt;
}

// Adjusts a value by the unit's morale: positive morale takes a share off, negative morale adds one (combat.yaml
// morale: 80% and 60% in the original)
void EEComplexUnit::ApplyMorale(long inValue, long& outValue)
{
    EeCombatMoraleIn theIn = {(int32_t)inValue, (int32_t)this->mPositiveMorale, (int32_t)this->mNegativeMorale,
                              EEGetRules().mCombat.mPositiveMorale, EEGetRules().mCombat.mNegativeMorale};
    outValue = EECombatMorale(theIn);
}

// Pops the citizen's build queue until a still-valid unit comes out; returns it, or NULL when the queue runs out
EEUnit* EEComplexUnit::PopBuildTarget()
{
    EEUnit* theResult = NULL;
    while (theResult == NULL && this->mCitizenBuildQueue.size() > 0) {
        EEUnit* theUnit = this->mCitizenBuildQueue.front();
        this->mCitizenBuildQueue.pop_front();
        if (theUnit->mIsValid)
            theResult = theUnit;
        theUnit->Dereference();
    }
    return theResult;
}

// Moves the unit away after a calamity: a computer player's prophet steps away from the cast, other AI units go idle,
// a human's unit walks to a random point ahead (unless inArg is set)
void EEComplexUnit::FleeAfterCalamity(long inArg)
{
    ee_units_cu_body(EE_UNITS_CU_DO_FLEE_AFTER_CALAMITY, this, inArg, 0, 0);   // cu_misc.rs
}

// Swaps replaced units (old -> new pairs) in the unit's goals, cargo, parts and build queue
void EEComplexUnit::ReplaceUnits(const std::vector<EEPUnitSwap>& inReplacements)
{
    std::vector<EEUnit*> theTargets;
    if (!inReplacements.empty()) {
        for (unsigned long i = 0; i < this->mGoalQueues.size(); ++i) {
            for (unsigned long j = 0; j < this->mGoalQueues[i].mGoals.size(); ++j) {
                EEUnitGoal* theGoal = this->mGoalQueues[i].mGoals[j];
                for (std::vector<EEPUnitSwap>::const_iterator thePair =
                    inReplacements.begin(); thePair != inReplacements.end(); ++thePair) {
                    theGoal->ReplaceUnit(thePair->mOld, thePair->mNew, this);
                    if (this->mGoalQueues[i].mGoals.size() <= j || this->mGoalQueues[i].mGoals[j] != theGoal)
                        break;
                }
            }
        }
        if (this->mCargo.size() > 0 || this->mContainer != NULL) {
            for (std::vector<EEPUnitSwap>::const_iterator thePair =
                inReplacements.begin(); thePair != inReplacements.end(); ++thePair)
                this->ReplaceUnit(thePair->mOld, thePair->mNew);
        }
        EEUnit* theTarget;
        while ((theTarget = this->PopBuildTarget()) != 0)
            theTargets.push_back(theTarget);
        for (unsigned long theIndex = 0; theIndex < theTargets.size(); ++theIndex) {
            theTarget = theTargets[theIndex];
            for (std::vector<EEPUnitSwap>::const_iterator thePair =
                inReplacements.begin(); thePair != inReplacements.end(); ++thePair) {
                if (thePair->mOld == theTarget) {
                    theTarget = thePair->mNew;
                    break;
                }
            }
            this->AddBuildTarget(theTarget);
        }
    }
}

// Places the unit on the ground at the location, pitching and rolling it with the terrain when its class does
void EEComplexUnit::PlaceOnGround(const GE3DPoint& inLocation)
{
    GETransformation theTransformation(this->mPhysicalModel.GetLocation());
    GE3DPoint theGround(0.0f, 0.0f, 0.0f);
    EEMap* theMap = ::gWorld.mMap;
    GEVector theUp(0.0f, 1.0f, 0.0f);
    GEVector theDirection(0.0f, 0.0f, 0.0f);
    float theRoll = 0.0f;
    float thePitch = 0.0f;
    theMap->GetGroundPoint(inLocation, theGround);
    this->GetUnitClass()->GroundLocation(theGround);
    theTransformation.SetX(theGround.GetX());
    theTransformation.SetY(theGround.GetY());
    theTransformation.SetZ(theGround.GetZ());
    theTransformation.Apply(theUp, theDirection);
    float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
    if (this->GetUnitClass()->mPitchesWithTerrain)
        thePitch = theMap->GetTerrainPitch(theTransformation);
    if (this->GetUnitClass()->mRollsWithTerrain)
        theRoll = theMap->GetTerrainRoll(theTransformation);
    theTransformation.SetOrientationYPR(theYaw, thePitch, theRoll);
    this->mPhysicalModel.SetLocation(theTransformation);
}

// Queues a goal on squadron queues 1..3 (1..4 with the paratroopers power), as the goal or clones of it
void EEComplexUnit::QueueSquadronGoals(EEUnitGoal* inGoal, bool inCloneGoal)
{
    long theLast = ::gCivPowersManager.HasPower(this->GetPlayer(), kCivPowerParatroopers) ? 4 : 3;
    for (long i = 1; i <= theLast; i++) {
        EEUnitGoal* theGoal;
        if (!inCloneGoal && (unsigned long)i <= 1)
            theGoal = inGoal;
        else
            theGoal = inGoal->Clone();
        if (this->GetQueuedGoal(i) != NULL)
            this->QueueGoal(theGoal, i);
        else
            this->SetGoal(theGoal, 0, i);
    }
}

// Removes a unit from the units this one holds; returns true if it was held
bool EEComplexUnit::ReleaseHeld(EEComplexUnit* inUnit)
{
    std::vector<EEComplexUnit*>::iterator theIterator = std::find(this->mHeldUnits.begin(), this->mHeldUnits.end(), inUnit);
    bool theFound = theIterator != this->mHeldUnits.end();
    if (theFound) {
        this->mHeldUnits.erase(theIterator);
        inUnit->Dereference();
    }
    return theFound;
}

// Removes a unit from the cargo and gives its room back; refreshes the icon bar if the transport is selected
void EEComplexUnit::ReleaseCargo(EEComplexUnit* inCargo)
{
    ee_units_cu_body(EE_UNITS_CU_DO_RELEASE_CARGO, this, (int64_t)(uintptr_t)inCargo, 0, 0);   // cu_cargo.rs
}

// Unloads every cargo unit that fits near the tile (teleporters show the teleport effect) and drops dead ones;
// posts an unload-failed event if any stay aboard
void EEComplexUnit::UnloadCargoAtTile(long inX, long inY, bool inArg)
{
    ee_units_cu_body(EE_UNITS_CU_DO_UNLOAD_CARGO_AT_TILE, this, inX, inY, inArg);   // cu_cargo.rs
}

// Empties the build queue, refunding each entry and allowing its technology again
void EEComplexUnit::CancelQueuedProduction()
{
    ee_units_cu_body(EE_UNITS_CU_DO_CANCEL_QUEUED_PRODUCTION, this, 0, 0, 0);   // cu_cargo.rs
}

// Unloads a cargo unit at the location (aircraft at cruise altitude) and sets it idle; returns it if something else
// still references it, else NULL
EEComplexUnit* EEComplexUnit::UnloadCargoAt(unsigned long inIndex, const GETransformation& inLocation)
{
    return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(
        ee_units_cu_body(EE_UNITS_CU_DO_UNLOAD_CARGO_AT, this, inIndex, (int64_t)(uintptr_t)&inLocation, 0)));   // cu_cargo.rs
}

// Unloads one cargo unit next to the transport now (a dead one is just released); returns true on success
bool EEComplexUnit::UnloadCargoNow(EEComplexUnit* inCargo)
{
    return ee_units_cu_body(EE_UNITS_CU_DO_UNLOAD_CARGO_NOW, this, (int64_t)(uintptr_t)inCargo, 0, 0) != 0;   // cu_cargo.rs
}

// Unloads the cargo unit with the given unit id at the location; returns what UnloadCargoAt returns, or NULL
EEComplexUnit* EEComplexUnit::UnloadCargoByID(unsigned long inUnitID, const GETransformation& inLocation)
{
    EEComplexUnit* theResult = NULL;
    unsigned long theCount = this->GetNumCargoUnits();
    for (unsigned long i = 0; i < theCount; i++) {
        if (inUnitID == this->GetCargo(i)->mUnitID) {
            theResult = this->UnloadCargoAt(i, inLocation);
            break;
        }
    }
    return theResult;
}

// Removes a unit from this unit's contact list
void EEComplexUnit::RemoveContactUnit(EEComplexUnit* inUnit)
{
    UListNode<EEComplexUnit*>* theNode = this->mContacts.Find(inUnit);
    if (theNode) {
        this->mContacts.Remove(theNode);
        inUnit->Dereference();
    }
}

// Removes the unit's line of sight at the tile for its player, its sight sharers, the minimap and terrain mesh
// (plus a wonder's extra sight; kind-4 units also clear it for every player): SightOn's statements with each Add a
// Remove, the tiles not tested for NULL (ee_units sight.rs)
void EEComplexUnit::SightOff(unsigned long inX, unsigned long inY)
{
    ee_units_sight(EEObj(this), (uint32_t)inX, (uint32_t)inY, 0);
}

// SightOn's and SightOff's host (ee_units sight.rs, sight_host.rs; EE_UNITS_SIGHT_*): the unit's and players' reads and
// the sight's world calls, each as the original made it (inOn: SightOn's Add, else SightOff's Remove)
extern "C" int64_t ee_units_sight_host(int32_t inOp, uint8_t inOn, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD,
                                       float (*ioF)[3], EeUnitsSightUnit* outUnit)
{
    EEComplexUnit* theUnit = (EEComplexUnit*)(uintptr_t)inA;
    EEPlayer* thePlayer = (EEPlayer*)(uintptr_t)inA;
    float* f = *ioF;
    // (the world's and the players' plain reads are Rust's: ee_units sight_host.rs)
    switch (inOp) {
    case EE_UNITS_SIGHT_UNIT: {
        EEUnitClass* theClass = theUnit->GetUnitClass();
        EETechTreeNode* theNode = theUnit->GetTechTreeNode();
        outUnit->line_of_sight = theUnit->GetLineOfSight();
        outUnit->player = EEObj(theUnit->mPlayer);
        outUnit->save_vis_map = theUnit->mPlayer != NULL && theUnit->mPlayer->mSaveVisMap;
        outUnit->hit_points = (int32_t)theUnit->mHitPoints;
        outUnit->is_built = theUnit->mIsBuilt;
        outUnit->land_building = theClass->IsLandBuilding();
        outUnit->tile_size = (int32_t)theClass->mTileSize;
        outUnit->wonder_bonus = (int32_t)theClass->mWonderBonus;
        outUnit->node_kind = theNode != NULL ? (int32_t)theNode->mData->mKind : -1;
        return 0;
    }
    case EE_UNITS_SIGHT_ENABLED:
        return gSightUpdatesEnabled;
    case EE_UNITS_SIGHT_FOUNDATION:
        return theUnit->IsFoundation();
    case EE_UNITS_SIGHT_CENTER:
        theUnit->AddCenterOffset(f[0], f[1]);
        return 0;
    case EE_UNITS_SIGHT_ALLIED:
        return thePlayer->IsAlliedWith((EEPlayer*)(uintptr_t)inB);
    case EE_UNITS_SIGHT_CIRCLE:
        if (inOn)
            EEAddSightCircle(f[0], f[1], f[2], thePlayer);
        else
            EERemoveSightCircle(f[0], f[1], f[2], thePlayer);
        return 0;
    case EE_UNITS_SIGHT_TILE_SIGHT:
        if (inOn)
            ((EEMapTile*)(uintptr_t)inA)->AddSight((EEPlayer*)(uintptr_t)inB);
        else
            ((EEMapTile*)(uintptr_t)inA)->RemoveSight((EEPlayer*)(uintptr_t)inB);
        return 0;
    case EE_UNITS_SIGHT_SHARED:
        if (inOn)
            thePlayer->AddSharedSight((EEMapTile*)(uintptr_t)inB, (long)inC, (long)inD);
        else
            thePlayer->RemoveSharedSight((EEMapTile*)(uintptr_t)inB);
        return 0;
    case EE_UNITS_SIGHT_SHOW:
        if (inOn) {
            gMiniMap->CountCellOn((long)inC, (long)inD);
            ::gWorld.mMap->mTerrainMesh.ReferenceTile((long)inC, (long)inD);
        } else {
            gMiniMap->CountCellOff((long)inC, (long)inD);
            ::gWorld.mMap->mTerrainMesh.DereferenceTile((long)inC, (long)inD);
        }
        return 0;
    case EE_UNITS_SIGHT_WONDER:
        if (inOn)
            ::gWorld.mMap->AddSight(thePlayer, f[0], f[1], gWorldTable.GetFloatValue(kWorldWonderSightRadius), false);
        else
            ::gWorld.mMap->RemoveSight(thePlayer, f[0], f[1], gWorldTable.GetFloatValue(kWorldWonderSightRadius), false);
        return 0;
    }
    return 0;
}


// Cancels the building's current research or production, refunding it and clearing the production time
void EEComplexUnit::ToggleJITManufacturing()
{
    ee_units_cu_body(EE_UNITS_CU_DO_TOGGLE_JIT, this, 0, 0, 0);   // cu_cargo.rs
}

// Removes a technology from the build queue with a refund (the first match only unless inRemoveAll); if none was
// queued, cancels it when it is the building's current research
void EEComplexUnit::RemoveResearch(long inTechnology, long inRemoveAll)
{
    ee_units_cu_body(EE_UNITS_CU_DO_REMOVE_RESEARCH, this, inTechnology, inRemoveAll, 0);   // cu_cargo.rs
}

// Takes the unit and its parts off the map (deselected, moved to -10000, deactivated) and sets it idle
void EEComplexUnit::TakeOffMap()
{
    ee_units_cu_body(EE_UNITS_CU_DO_TAKE_OFF_MAP, this, 0, 0, 0);   // cu_misc.rs
}

// Starts the next build-queue entry the built building can produce (a citizen may come out female), dropping and
// refunding unavailable entries
void EEComplexUnit::StartProduction()
{
    ee_units_cu_body(EE_UNITS_CU_DO_START_PRODUCTION, this, 0, 0, 0);   // cu_body.rs
}

// Returns true if the class uses air traffic control and is not a building
bool EEUnitClass::IsAircraft()
{
    return this->mAirTrafficType != 0 && !this->IsLandBuilding() && this->mCategory != kUnitCategoryNavalBuilding && this->mCategory != kUnitCategorySpaceBuilding;
}

// Switches the unit to an action and updates its camouflage; a dying unit refuses any non-dying action
void EEComplexUnit::StartAction(long inAction)
{
    ee_units_cu_body(EE_UNITS_CU_DO_START_ACTION, this, inAction, 0, 0);   // cu_body.rs
}

// Switches the unit's animation and frame, stops its sound and plays the new frame's sound
void EEComplexUnit::SetAnimation(long inPart, long inFrame, bool inIsAnimationPaused)
{
    ee_units_cu_body(EE_UNITS_CU_DO_SET_ANIMATION, this, inPart, inFrame, inIsAnimationPaused);   // cu_body.rs
}

// Sets the unit's mine (reference counted)
void EEComplexUnit::SetMine(EEUnit* inUnit)
{
    if (this->mMine)
        this->mMine->Dereference();
    this->mMine = inUnit;
    if (inUnit)
        inUnit->Reference();
}

// Sets the unit that blocks this one (reference counted)
void EEComplexUnit::SetBlockingUnit(RTSUnit* inUnit)
{
    if (this->mBlockingUnit) {
        this->mBlockingUnit->Dereference();
        this->mBlockingUnit = NULL;
    }
    if (inUnit) {
        this->mBlockingUnit = inUnit;
        inUnit->Reference();
    }
}

// Marks the unit and its parts built or unbuilt; finishing one updates sight, airports and completion stats
// (a granary replants farms) and pays for it
void EEComplexUnit::SetBuilt(long inIsBuilt, long inUpdateSight, long inLoadButtons)
{
    ee_units_cu_body(EE_UNITS_CU_DO_SET_BUILT, this, inIsBuilt, inUpdateSight, inLoadButtons);   // cu_body.rs
}

// Sets the resource the unit gathers, releasing the old one and sending a gatherer waiting for it
void EEComplexUnit::SetGatheredResource(EEUnit* inUnit)
{
    ee_units_cu_body(EE_UNITS_CU_DO_SET_GATHERED_RESOURCE, this, (int64_t)(uintptr_t)inUnit, 0, 0);   // cu_misc.rs
}

// Sets the unit's model, scaled by its class tile size
void EEComplexUnit::SetScaledModel(GEModel* inModel)
{
    this->mPhysicalModel.SetModel(inModel);
    this->mPhysicalModel.SetScale(this->GetUnitClass()->GetTileSize());
}

// Replaces the goals on squadron queues 1..3 (1..4 with the paratroopers power) with the goal or clones of it
void EEComplexUnit::SetSquadronGoals(EEUnitGoal* inGoal, bool inCloneGoal)
{
    long theLast = ::gCivPowersManager.HasPower(this->GetPlayer(), kCivPowerParatroopers) ? 4 : 3;
    for (long i = 1; i <= theLast; i++) {
        this->ClearGoals(i);
        this->ClearGoalQueue(i);
        if (!inCloneGoal && (unsigned long)i <= 1)
            this->SetGoal(inGoal, 0, i);
        else
            this->SetGoal(inGoal->Clone(), 0, i);
    }
}

// Sets the unit's growth scale, scaling its model and collision radius (at least 0.05)
void EEComplexUnit::SetGrowth(float inScale)
{
    if (inScale == 1.0f && this->mGrowth == 1.0f)
        return;
    this->mGrowth = inScale;
    if (inScale >= 1.0f) {
        this->mPhysicalModel.SetScale(this->GetStats()->GetScale());
        this->mCollisionRadius = this->GetUnitClass()->GetInnerCollisionSphere();
    } else {
        this->mPhysicalModel.SetScale(this->GetStats()->GetScale() * inScale);
        if (this->GetUnitClass()->GetInnerCollisionSphere() > 0.0f) {
            this->mCollisionRadius = this->GetUnitClass()->GetInnerCollisionSphere() * this->mGrowth;
            if (this->mCollisionRadius < 0.05f)
                this->mCollisionRadius = 0.05f;
        }
    }
}

// Disables or re-enables the unit for a reason, updating its airport and the time-warp tint
void EEComplexUnit::SetDisabled(bool inAway, long inReason)
{
    ee_units_cu_body(EE_UNITS_CU_DO_SET_DISABLED, this, inAway, inReason, 0);   // cu_misc.rs
}

// Sets the last resource the unit gathered from (reference counted)
void EEComplexUnit::SetLastResource(RTSUnit* inUnit)
{
    if (this->mLastResource) {
        this->mLastResource->Dereference();
        this->mLastResource = NULL;
    }
    if (inUnit) {
        this->mLastResource = inUnit;
        inUnit->Reference();
    }
}

// Moves the unit to the tile under a point, refreshing its contacts, flight height and swim animation
void EEComplexUnit::MoveToLocation(float inX, float inY, bool inUpdateTiles)
{
    RTSTile* theOldTile = this->mTile;
    long theX = UMFloor(inX);
    long theY = UMFloor(inY);
    bool theMoved = this->mIsCollidable != 0 && (theX != this->GetTileX() || theY != this->GetTileY());
    this->MoveTo(theX, theY, inUpdateTiles);
    if (theMoved) {
        this->ClearContacts();
        this->FindContacts();
    }
    if (theOldTile != this->mTile && this->mWaypoints.size() != 0 && ((this->GetUnitClass()->mDomains & kMovementAir) || this->GetUnitClass()->GetUnitType() == kUnitTypeSp15SpaceFighter)) {
        GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
        this->GetNextWaypointPoint(theLocation);
        this->SetHeight(this->GetUnitClass()->GetHeightAt(this, theLocation));
    }
    if (!this->HasSwimAnimation())
        return;
    if (!this->IsSwimming() && this->mAnimation == 2)
        this->SetAnimation(0xd, -1, 0);
    else if (this->IsSwimming() && this->mAnimation == 0xd)
        this->SetAnimation(2, -1, 0);
}

// Moves the unit and its model to an x/y point
void EEComplexUnit::SetLocationXY(float inX, float inY, bool inUpdateTiles)
{
    this->MoveToLocation(inX, inY, inUpdateTiles);
    this->mPhysicalModel.SetLocationXY(inX, inY);
}

// Moves the unit and its model to a 3D point
void EEComplexUnit::SetLocation(const GE3DPoint& inLocation, bool inUpdateTiles)
{
    this->MoveToLocation(inLocation.GetX(), inLocation.GetY(), inUpdateTiles);
    this->mPhysicalModel.SetLocationXYZ(inLocation);
}

// Moves the unit and sets its model's transformation
void EEComplexUnit::SetTransformation(const GETransformation& inTransformation, bool inUpdateTiles)
{
    this->MoveToLocation(inTransformation.GetX(), inTransformation.GetY(), inUpdateTiles);
    this->mPhysicalModel.SetLocation(inTransformation);
}

// Links the unit as the next part of another unit, unlinking it from its previous part
void EEComplexUnit::LinkPart(EEComplexUnit* inUnit)
{
    ee_units_cu_body(EE_UNITS_CU_DO_LINK_PART, this, (int64_t)(uintptr_t)inUnit, 0, 0);   // cu_misc.rs
}

// Sets the unit's patrol group (reference counted)
void EEComplexUnit::SetPatrolGroup(EEPPatrolGroup* inGroup)
{
    if (this->mPatrolGroup)
        ee_units_pg_dereference(this->mPatrolGroup);
    this->mPatrolGroup = inGroup;
    if (inGroup)
        ee_units_pg_reference(inGroup);
}

// Sets the unit's home base (reference counted)
void EEComplexUnit::SetHomeBase(EEComplexUnit* inBase)
{
    if (this->mHomeBase)
        this->mHomeBase->Dereference();
    if (inBase)
        inBase->Reference();
    this->mHomeBase = inBase;
}

// Sets the reload timer to a reload time scaled by a random factor from world table entry 87
void EEComplexUnit::SetRandomReloadTimer(long inReloadTime)
{
    static bool sInitialized;
    static float sMinimum;
    static float sMaximum;
    if (!sInitialized) {
        sMinimum = gWorldTable.GetFloatValue(kWorldReloadTimeRandomRange);
        sMaximum = gWorldTable.GetFloatValue2(kWorldReloadTimeRandomRange);
        sInitialized = true;
    }
    this->SetReloadTimer(UMFloatToLong(gPhysics->GetRandom(sMinimum, sMaximum) * inReloadTime));
}

// Sets the building's current research, unloading the old one's media and preloading the new one's (100000 = none)
void EEComplexUnit::PlayEffect(long inTechnology)
{
    ee_units_cu_body(EE_UNITS_CU_DO_PLAY_EFFECT, this, inTechnology, 0, 0);   // cu_misc.rs
}

// Replaces a cargo unit with another, adjusting the cargo room
void EEComplexUnit::ReplaceCargo(EEComplexUnit* inOld, EEComplexUnit* inNew)
{
    ee_units_cu_body(EE_UNITS_CU_DO_REPLACE_CARGO, this, (int64_t)(uintptr_t)inOld, (int64_t)(uintptr_t)inNew, 0);   // cu_cargo.rs
}

// Turns the unit level to face a point
void EEComplexUnit::FaceToward(GE3DPoint inPoint)
{
    GETransformation theTransformation(this->mPhysicalModel.GetLocation());
    float theDY = inPoint.GetY() - theTransformation.GetY();
    float theYaw = UMArcTan2(-(inPoint.GetX() - theTransformation.GetX()), theDY);
    theTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    this->SetTransformation(theTransformation, true);
}

// Turns the unit level to face the center of a square of tiles
void EEComplexUnit::FaceTowardSquare(long inX, long inY, long inSize)
{
    GETransformation theTransformation(this->mPhysicalModel.GetLocation());
    float theHalf = inSize * 0.5f;
    long theX = UMFloor(inX + theHalf);
    long theY = UMFloor(inY + theHalf);
    EEMap* theMap = ::gWorld.mMap;
    theTransformation.SetOrientationYPR(UMArcTan2(-((theX + 0.5f) - theTransformation.GetX()),
                                                  (theY + 0.5f) - theTransformation.GetY()),
                                        0.0f, 0.0f);
    this->SetTransformation(theTransformation, true);
}

// Turns the unit to face a target: a large target's footprint center, else its scaled top point or position
void EEComplexUnit::FaceTarget(EEUnit* inTarget)
{
    GE3DPoint theLocation;
    GE3DPoint theTop;
    long theSize = inTarget->mUnitClass->mTileSize;
    if (theSize > 1)
        this->FaceTowardSquare(inTarget->GetTileX(), inTarget->GetTileY(), theSize);
    else {
        if (inTarget->GetComplexUnit() != NULL && inTarget->GetModel()->mModel != NULL) {
            theTop = inTarget->GetModel()->mTopPoint;
            float theScale = inTarget->GetComplexUnit()->mPhysicalModel.GetScale();
            theTop.SetX(theTop.GetX() * theScale);
            theTop.SetY(theTop.GetY() * theScale);
            inTarget->GetComplexUnit()->mPhysicalModel.GetLocation().Apply(theTop, theLocation);
        } else
            inTarget->GetPosition(theLocation);
        this->FaceToward(theLocation);
    }
}

// Stops the unit: clears its build targets, goals and airport queue and sets it idle (aircraft circle)
void EEComplexUnit::Halt()
{
    ee_units_cu_body(EE_UNITS_CU_DO_HALT, this, 0, 0, 0);   // cu_misc.rs
}

// Returns true if the unit can hit a target from where it stands (melee contact, range, or a ballistic shot from
// any weapon tag point; a legionnaire's special attack uses its heal range)
bool EEComplexUnit::IsInReach(EEUnit* inTarget)
{
    EECombatHostReadScope theScope;
    uint32_t theTagBefore = (uint32_t)this->mWeaponTagPoint;
    // ee_combat reads.rs in_reach over the C++ host (EECombatGoals.cpp); CBQR records the outcome
    bool theReach = EECombatIsInReach(this, inTarget);
    if (theScope.mOuter)
        EECombatCaptureHostRead(EE_COMBAT_HR_IN_REACH, this, inTarget, theReach, theTagBefore, (uint32_t)this->mWeaponTagPoint);
    return theReach;
}

// Returns true if the unit can attack a tile from where it stands (melee: adjacent; ballistic: a clear arc;
// else in range and on the same continent)
bool EEComplexUnit::CanReachTile(long inX, long inY)
{
    float theArgument;
    EEUnit* theArgument2;
    long theArgument3;
    long theArgument4;
    GE3DPoint theUnused(0.0f, 0.0f, 0.0f);
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    if (this->GetUnitClass()->mIsMelee)
        return this->IsNextToTile(inX, inY);
    EEUnitClass* theClass = this->GetUnitClass()->mProjectileClass;
    if (theClass != NULL && (theClass->mCategory == kUnitCategoryProjectile || theClass->mCategory == kUnitCategoryBomb)) {
        GE3DPoint theDestination((float)inX, (float)inY, ::gWorld.mMap->GetTile(inX, inY)->mHeight);
        if (this->IsOnOtherContinent(inX, inY))
            return false;
        EEApproachAttachPoint(this, theUnused);
        this->GetPosition(thePosition);
        if (!EEGetBallisticPitch(this, this->GetStats(), &thePosition, &theDestination, &theArgument))
            return false;
        if (this->GetStats()->mCanFireOverWalls)
            return true;
        return !EEIsLineBlocked(this->GetTileX(), this->GetTileY(), inX, inY, &theArgument2, &theArgument3, &theArgument4);
    }
    float theRange = this->GetStats()->mRange;
    float theDistance = this->DistanceSquaredToTileXY(inX, inY);
    if (theDistance > theRange * theRange)
        return false;
    return !this->IsOnOtherContinent(inX, inY);
}


// TEST ONLY (USST): AIStep's call as words: the inputs, the path, the results, the outputs
static void NoteAIStep(EEComplexUnit* inUnit, uint32_t* ioWords)
{
    ioWords[7] = (uint32_t)(inUnit->mCommand ? inUnit->mCommand->mType : 0);
    ioWords[8] = (uint32_t)inUnit->mNextTriggerTime;
    ee_units_note_state(kUnitStateAIStep, (uint32_t)inUnit->mUnitID, ioWords, 9);
}

// Evaluates the unit's current action and switches to the next one (dropping finished goals); an idle unit stops
// evaluating 350 ticks after it went idle until its trigger time is reset
// New (R5b.4 leaf 6): Rust over the unit store (ee_units ai.rs ai_step), the goal's activity, the actions' starts and
// changes, the finished goals' drop, the camouflage and the command's write host calls (EEUnitAction.cpp)
void EEComplexUnit::AIStep()
{
    bool theNoting = ee_units_note_noting();   // TEST ONLY (USST)
    uint32_t theWords[9] = {(uint32_t)(this->mCommand ? this->mCommand->mType : 0), (uint32_t)this->mNextTriggerTime,
                            (uint32_t)gPhysics->mGameTime, 0, 0, 0xffffffffu, 0xffffffffu, 0, 0};
    EeUnitsAIHost theHost;
    EEUnitAIHostFor(this, theHost);
    EeUnitsAIStepOut theOut = ee_units_ai_step(&theHost, static_cast<uint32_t>(this->mUnitID), reinterpret_cast<uintptr_t>(this),
                                               this->mIsCitizen ? 1 : 0, gPhysics->mGameTime);
    if (theNoting) {
        theWords[3] = theOut.path;
        theWords[4] = theOut.action;
        theWords[5] = theOut.first;
        theWords[6] = theOut.second;
        NoteAIStep(this, theWords);
    }
}

// Returns true if a range-1 unit stands on the target's tile or a tile along its sides or corners
bool EEComplexUnit::IsOnTileBorderingTarget(EEUnit* inTarget)
{
    // ee_combat world.rs on_tile_bordering over the world host; CBXR records the outcome
    bool theResult = EECombatWorld(EE_COMBAT_WORLD_BORDERING, this, inTarget) != 0;
    EECombatCaptureXRead(EE_COMBAT_XR_BORDERING, this, inTarget, theResult);
    return theResult;
}

// Replaces a unit in the cargo, and relinks this unit to the new one if the old one was its previous part
void EEComplexUnit::ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew)
{
    for (long i = this->mCargo.size() - 1; i >= 0; i--) {
        if (this->mCargo[i] != inOld)
            continue;
        this->mCargo[i]->Dereference();
        this->mCargo[i] = inNew;
        inNew->Reference();
        if (inNew->mContainer != this) {
            if (inNew->mContainer)
                inNew->mContainer->Dereference();
            inNew->mContainer = this;
            this->Reference();
        }
        break;
    }
    if (this->mPreviousPart == inOld) {
        this->LinkPart(inNew);
        ::gWorld.DeactivateUnit(this);
    }
}

// Returns the last attacker while it is still attacking this unit (in reach with a clear line, or a ground attack
// covering it); otherwise forgets it and returns NULL
EEComplexUnit* EEComplexUnit::GetConditionTarget()
{
    if (this->mLastAttacker == NULL)
        return NULL;
    EEUGRust* theAttack = EEAttackUnitCast(this->mLastAttacker->GetGoal());
    if (theAttack != NULL) {
        if (theAttack->GetAttackTarget() == this && this->mLastAttacker->IsInReach(this)) {
            EEUnit* theArg1;
            long theArg2, theArg3;
            if (this->GetUnitClass()->mShootsHigh || this->GetStats()->mCanFireOverWalls
                || !EEIsLineToUnitBlocked(this->mLastAttacker, this, &theArg1, &theArg2, &theArg3))
                return this->mLastAttacker;
        }
    } else {
        EEUGRust* theGround = EEAttackGroundCast(this->mLastAttacker->GetGoal());
        if (theGround != NULL) {
            float theDX = this->mPhysicalModel.GetLocation().GetX() - theGround->GetGroundTarget().GetX();
            float theDY = this->mPhysicalModel.GetLocation().GetY() - theGround->GetGroundTarget().GetY();
            float theRadius = this->mLastAttacker->mStats->mAreaEffect;
            if (theDX * theDX + theDY * theDY <= theRadius * theRadius)
                return this->mLastAttacker;
        }
    }
    if (this->mLastAttacker != NULL)
        this->mLastAttacker->Dereference();
    this->mLastAttacker = NULL;
    return this->mLastAttacker;
}

// Updates the camouflage power's timers when the unit changes action (idling starts it); without vital buildings
// the unit loses its cloak
void EEComplexUnit::UpdateCamouflage(long inAction)
{
    ee_units_cu_body(EE_UNITS_CU_DO_UPDATE_CAMOUFLAGE, this, inAction, 0, 0);   // cu_misc.rs
}

// Moves the unit's area effects to its position and updates them for the elapsed time
void EEComplexUnit::UpdateAreaEffects(unsigned long inValue)
{
    GE2DPoint thePoint(this->mPhysicalModel.GetLocation().GetX(), this->mPhysicalModel.GetLocation().GetY());
    this->mAreaEffects.MoveAndUpdate(thePoint, (float)inValue);
}

// Moves the unit's line of sight from the old tile to the new one (adds or removes it when either is off the map)
void EEComplexUnit::SightMoved(unsigned long inOldX, unsigned long inOldY, unsigned long inNewX, unsigned long inNewY)
{
    EEUnitClass* theClass = this->GetUnitClass();
    bool theLarge = theClass->mTileSize > 1;
    if (this->mPlayer == NULL || this->mPlayer->mSaveVisMap != true || !(this->GetLineOfSight() > 0.0f) || !gSightUpdatesEnabled)
        return;
    if (inOldX == (unsigned long)-10000) {
        this->SightOn(inNewX, inNewY);
    } else {
        if (inNewX != (unsigned long)-10000) {
            if (this->mHitPoints > 0) {
                float theOffset;
                if (!theLarge)
                theOffset = 0.5f;
                else
                theOffset = theClass->mTileSize * 0.5f;
                EEMoveSightCircle((float)(long)inOldX + theOffset, (float)(long)inOldY + theOffset,
                (float)(long)inNewX + theOffset, (float)(long)inNewY + theOffset, this->GetLineOfSight(), this->mPlayer);
                return;
            }
            this->SightOn(inNewX, inNewY);
        }
        this->SightOff(inOldX, inOldY);
    }
}


// Switches the unit to another graphic, keeping its animation progress and turret, orbital and wheel angles
void EEComplexUnit::SwapGraphic(EEGraphicObject* inOldModel, EEGraphicObject* inNewModel)
{
    unsigned long theState = this->mAnimation;
    float theFraction = (float)(unsigned long)(this->mPhysicalModel.GetFrame() - inOldModel->mStartFrames[theState]) /
        (float)inOldModel->mFrameCounts[theState];
    if (theFraction < 0.0f)
        theFraction = 0.0f;
    else if (theFraction > 1.0f)
        theFraction = 1.0f;
    unsigned long theFrame = UMFloatToLong(inNewModel->mFrameCounts[theState] * theFraction) +
        inNewModel->mStartFrames[theState];
    if (theFrame > inNewModel->mLastFrames[theState])
        theFrame = inNewModel->mLastFrames[theState];
    this->SetAnimationFrame(theFrame);
    if (++theFrame > inNewModel->mLastFrames[theState])
        theFrame = inNewModel->mStartFrames[theState];
    this->SetNextFrame(theFrame);
    unsigned long thePartCount = inNewModel->GetNumParts();
    float* theParts = new float[thePartCount];
    for (unsigned long i = 0; i < thePartCount; ++i)
        theParts[i] = i < inOldModel->GetNumParts() ? this->mTurretAngles[i] : 0.0f;
    delete[] this->mTurretAngles;
    this->mTurretAngles = theParts;

    unsigned long theOrbitalCount = inNewModel->GetOrbitalCount();
    EEOrbital** theOrbitals = NULL;
    if (theOrbitalCount != 0) {
        theOrbitals = new EEOrbital*[theOrbitalCount];
        for (unsigned long i = 0; i < theOrbitalCount; ++i) {
            theOrbitals[i] = new EEOrbital(*inNewModel->mOrbitals[i]);
            if (i < this->mNumOrbitals)
                theOrbitals[i]->mAngle = this->mOrbitals[i]->mAngle;
        }
    }
    for (unsigned long theOldOrbital = 0; theOldOrbital < this->mNumOrbitals; ++theOldOrbital)
        delete this->mOrbitals[theOldOrbital];
    delete[] this->mOrbitals;
    this->mOrbitals = theOrbitals;
    this->mNumOrbitals = theOrbitalCount;

    unsigned long theWheelCount = inNewModel->GetWheelCount();
    EEWheel** theWheels = NULL;
    if (theWheelCount != 0) {
        theWheels = new EEWheel*[theWheelCount];
        for (unsigned long i = 0; i < theWheelCount; ++i) {
            theWheels[i] = new EEWheel(*inNewModel->mWheels[i]);
            if (i < this->mNumWheels)
                theWheels[i]->mAngle = this->mWheels[i]->mAngle;
        }
    }
    for (unsigned long theOldWheel = 0; theOldWheel < this->mNumWheels; ++theOldWheel)
        delete this->mWheels[theOldWheel];
    delete[] this->mWheels;
    this->mWheels = theWheels;
    this->mNumWheels = theWheelCount;
}

// Replaces a technology in the current research and the build queue with another (100000 removes it)
void EEComplexUnit::ReplaceResearch(long inTechnology, long inNewTechnology)
{
    ee_units_cu_body(EE_UNITS_CU_DO_REPLACE_RESEARCH, this, inTechnology, inNewTechnology, 0);   // cu_cargo.rs
}

// Recreates the unit's area effects from the four area-effect templates in its stats
void EEComplexUnit::MakeAreaEffects()
{
    this->mAreaEffects.Clear();
    this->mAreaEffects.mItems.clear();
    this->mNumAreaEffects = 0;
    for (long i = 0; i < 4; i++) {
        long theTemplate = this->GetStats()->mAreaEffectTemplates[i];
        if (theTemplate <= 0)
            continue;
        EEAreaEffect* theEffect = gAEManager.CreateAreaEffect(theTemplate, this);
        this->mAreaEffects.Add(theEffect);
        theEffect->Dereference();
        this->mNumAreaEffects++;
    }
}

// Copies an attacker record (tile, origin, unit id and time)
// (ICF-folded with EESEAPlaceWallEntry's copy; the deque copies call it)
EEComplexUnitAttacker& EEComplexUnitAttacker::operator=(const EEComplexUnitAttacker& inOther)
{
    this->mTileX = inOther.mTileX;
    this->mTileY = inOther.mTileY;
    this->mFromX = inOther.mFromX;
    this->mFromY = inOther.mFromY;
    this->mUnitID = inOther.mUnitID;
    this->mTime = inOther.mTime;
    return *this;
}

// Returns the number of wheels on the graphic object
// Declared out of line in ee/EEGraphicObject.h like GetOrbitalCount; its only out-of-line copy follows this
// unit's template block (provisional name)
unsigned long EEGraphicObject::GetWheelCount() const
{
    return this->mWheels.size();
}

void EEComplexUnit::DescribeLong(class UWideString &)
{
}

void EEComplexUnit::FnE123D()
{
}

// PayInsurance's notice: "You have received <amount> <resource> in insurance" in the player's colour
static void EEInsuranceChat(EEPlayer* inPlayer, unsigned long inResource, long inShownValue)
{
    char theAmount[64];
    sprintf(theAmount, "%ld", inShownValue);
    UWideString theText;
    UWideString thePart;
    gResource->GetString(IDS_YOU_HAVE_RECEIVED, theText);
    theText += theAmount;
    theText += " ";
    gResource->GetString(inResource + IDS_RESOURCE_NAME_BASE, thePart);
    theText += thePart;
    gResource->GetString(IDS_IN_INSURANCE, thePart);
    theText += thePart;
    gUserInterface->AddChatLogEntry(inPlayer->mColor, UWideString(gEmptyString), theText, 5000, 2);
}

// FleeAfterCalamity's point ahead is the body's first two floats
static_assert(sizeof(GE2DPoint) == 2 * sizeof(float), "a GE2DPoint is two floats");

// Kill's civilization power (EE_UNITS_CU_GET_INSURANCE fills it for the two reads after it)
static EECivPower sInsurancePower;

// a build queue entry of cu_cargo.rs's (technology, flag)
static EEProductionEntry EECuEntry(int64_t inTechnology, int64_t inFlag)
{
    EEProductionEntry theEntry;
    theEntry.mTechnology = (long)inTechnology;
    theEntry.mFlag = inFlag != 0;
    return theEntry;
}

// the index of a unit in a deque (std::find; -1: not there)
static int64_t EECuFind(const std::deque<EEUnit*>& inUnits, EEUnit* inUnit)
{
    std::deque<EEUnit*>::const_iterator theIterator = std::find(inUnits.begin(), inUnits.end(), inUnit);
    return theIterator == inUnits.end() ? -1 : (int64_t)(theIterator - inUnits.begin());
}

// UnloadCargoAt's aircraft at cruise altitude: a copy of the location at height inZ
static void EECuSetTransformationAtZ(EEComplexUnit* inUnit, const GETransformation& inLocation, float inZ)
{
    GETransformation theLocation(inLocation);
    theLocation.SetZ(inZ);
    inUnit->SetTransformation(theLocation, true);
}

template <class T> static T* EECuObject(uint64_t inHandle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle));
}

// The C++ of EEComplexUnit's Rust bodies (ee_units cu_body.rs; EE_UNITS_CU_*): one statement of the original bodies
// each, in their order; inA an object, inB and inC values, ioF floats in and out
extern "C" int64_t ee_units_cu_host(int32_t inOp, void* inUnit, uint64_t inA, int64_t inB, int64_t inC, float (*ioF)[4])
{
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    EEComplexUnit* theOther = EECuObject<EEComplexUnit>(inA);
    EEUnitClass* theClass = inA != 0 ? EECuObject<EEUnitClass>(inA) : theUnit->GetUnitClass();
    EEModel* theModel = EECuObject<EEModel>(inA);
    EETechTreeNode* theNode = EECuObject<EETechTreeNode>(inA);
    EEPlayer* thePlayer = EECuObject<EEPlayer>(inA);
    float* f = *ioF;
    switch (inOp) {
    // Kill's
    case EE_UNITS_CU_HIT_POINTS: return theUnit->mHitPoints;
    case EE_UNITS_CU_UNIT_TAKEN: theUnit->mPlayer->UnitTaken(theUnit, inB != 0); return 0;
    case EE_UNITS_CU_WORLD_CLEARING: return gWorldClearing;
    case EE_UNITS_CU_UNIT_SET: return (uint64_t)(uintptr_t)gUnitSetManager.GetSet((unsigned long)inB);
    case EE_UNITS_CU_SET_HAS_TYPE: return EECuObject<EEUnitSet>(inA)->Contains(theUnit->GetUnitClass()->GetUnitType());
    case EE_UNITS_CU_NUM_PLAYERS: return ::gWorld.mNumPlayers;
    case EE_UNITS_CU_PLAYER_AT: return (uint64_t)(uintptr_t)::gWorld.mPlayers[(unsigned short)inB];
    case EE_UNITS_CU_ENEMY_STATE: return thePlayer->mEnemies[theUnit->mPlayer->mPlayerID];
    case EE_UNITS_CU_UPDATE_FREE_UNIT_POWER: thePlayer->UpdateFreeUnitPower(); return 0;
    case EE_UNITS_CU_DEBUG_DEAD_CITIZEN:
        ESDebugOut("EEComplexUnit::Kill - Increment dead citizen count - type - %ld\n", theUnit->GetUnitClass()->GetUnitType());
        return 0;
    case EE_UNITS_CU_CLASS_INACTIVE: return theUnit->GetUnitClass()->mIsInactive;
    case EE_UNITS_CU_ACTIVATE: ::gWorld.ActivateUnit(theUnit); return 0;
    case EE_UNITS_CU_PREVIOUS_PART: return (uint64_t)(uintptr_t)theUnit->mPreviousPart;
    case EE_UNITS_CU_SHUTDOWN: return UShutdownInProgress();
    case EE_UNITS_CU_SWAP_KIND: return gPhysics->GetUnitSwapKind(theUnit);
    case EE_UNITS_CU_UNIT_TYPE: return theUnit->GetUnitClass()->GetUnitType();
    case EE_UNITS_CU_CATEGORY: return theUnit->GetUnitClass()->mCategory;
    case EE_UNITS_CU_STATS_RESET: theUnit->mPlayer->mSavedGroups.Reset((long)inB, 1); return 0;
    case EE_UNITS_CU_ADD_DEAD_STATISTIC: return EEAddDeadUnitStatistic(theUnit);
    case EE_UNITS_CU_STATS_LOWER: theUnit->mPlayer->mSavedGroups.Lower((long)inB, 1); return 0;
    case EE_UNITS_CU_HAS_INSURANCE: return gCivPowersManager.HasPower(theUnit->GetPlayer(), kCivPowerInsurance);
    case EE_UNITS_CU_GET_INSURANCE: return gCivPowersManager.GetPower(kCivPowerInsurance, sInsurancePower);
    case EE_UNITS_CU_INSURANCE_COVERS:
        return gUnitSetManager.Contains(theUnit->GetUnitClass()->GetUnitType(), sInsurancePower.mUnitSet);
    case EE_UNITS_CU_INSURANCE_AMOUNT: return EECuBits(sInsurancePower.mAmount);
    case EE_UNITS_CU_IS_BUILT: return theUnit->mIsBuilt;
    case EE_UNITS_CU_PAY_INSURANCE: theUnit->PayInsurance(EECuFloat(inB)); return 0;
    case EE_UNITS_CU_LAST_ATTACKER: return (uint64_t)(uintptr_t)theUnit->mLastAttacker;
    case EE_UNITS_CU_DEREFERENCE: EECuObject<RTSUnit>(inA)->Dereference(); return 0;
    case EE_UNITS_CU_CLEAR_LAST_ATTACKER: theUnit->mLastAttacker = NULL; return 0;
    case EE_UNITS_CU_OWNER: return (uint64_t)(uintptr_t)theUnit->mOwner;
    case EE_UNITS_CU_CLEAR_OWNER: theUnit->mOwner = NULL; return 0;
    case EE_UNITS_CU_UPDATE_JIT: theUnit->UpdateJITManufacturing(); return 0;
    case EE_UNITS_CU_CLEAR_CONTACTS: theUnit->ClearContacts(); return 0;
    case EE_UNITS_CU_KILL_CARGO: theUnit->KillCargo(); return 0;
    case EE_UNITS_CU_CLEAR_GARRISON: theUnit->ClearGarrison(); return 0;
    case EE_UNITS_CU_HOLDER: return (uint64_t)(uintptr_t)theUnit->mHolder;
    case EE_UNITS_CU_HOLDER_RELEASE: theUnit->mHolder->ReleaseHeld(theUnit); return 0;
    case EE_UNITS_CU_CLEAR_HOLDER_FIELD: theUnit->mHolder = NULL; return 0;
    case EE_UNITS_CU_RELEASE_HELD_UNITS: theUnit->ReleaseHeldUnits(); return 0;
    case EE_UNITS_CU_CANCEL_PATHS: gPathManager.CancelPaths(theUnit, true); return 0;
    case EE_UNITS_CU_DROP_PATH: theUnit->DropPath(); return 0;
    case EE_UNITS_CU_AIR_TRAFFIC_TYPE: return theUnit->GetUnitClass()->mAirTrafficType;
    case EE_UNITS_CU_IS_AIR_BASE: return theUnit->GetUnitClass()->IsAirBase();
    case EE_UNITS_CU_ATC_REMOVE_AIRPORT: ee_units_atc_remove_airport(theUnit->mPlayer->mAirTraffic[inB - 1], theUnit); return 0;
    case EE_UNITS_CU_ATC_REMOVE_CARGO: ee_units_atc_remove_cargo_weight(theUnit->mPlayer->mAirTraffic[inB - 1], theUnit); return 0;
    case EE_UNITS_CU_CLEAR_COLLISION: theUnit->ClearCollisionState(); return 0;
    case EE_UNITS_CU_START_ACTION: theUnit->StartAction((long)inB); return 0;
    case EE_UNITS_CU_ACTION_CHANGED: theUnit->GetUnitClass()->ActionChanged(theUnit); return 0;
    case EE_UNITS_CU_NEXT_PART: return (uint64_t)(uintptr_t)theUnit->mNextPart;
    case EE_UNITS_CU_KILL_PART: theOther->Kill(inB != 0, false, false); return 0;
    case EE_UNITS_CU_HAS_DEATH_SPAWN: return theUnit->GetUnitClass()->mDeathSpawnClass != NULL;
    case EE_UNITS_CU_FREE_CAMERA: return gFreeCamera;
    case EE_UNITS_CU_CREATE_DEATH_UNIT: theUnit->GetUnitClass()->CreateDeathUnit(theUnit); return 0;
    case EE_UNITS_CU_HAS_REMAINS: return theUnit->GetUnitClass()->mRemainsClass != NULL;
    case EE_UNITS_CU_IS_PROJECTILE: return theUnit->GetUnitClass()->mIsProjectile;
    case EE_UNITS_CU_CREATE_REMAINS_UNIT: theUnit->GetUnitClass()->CreateRemainsUnit(theUnit); return 0;
    case EE_UNITS_CU_CONTAINER: return (uint64_t)(uintptr_t)theUnit->mContainer;
    case EE_UNITS_CU_CONTAINER_RELEASE: theUnit->mContainer->ReleaseCargo(theUnit); return 0;
    case EE_UNITS_CU_CLEAR_GATHERED: theUnit->SetGatheredResource(NULL); return 0;
    case EE_UNITS_CU_CLEAR_BLOCKING: theUnit->SetBlockingUnit(NULL); return 0;
    case EE_UNITS_CU_CLEAR_LAST_RESOURCE: theUnit->SetLastResource(NULL); return 0;
    case EE_UNITS_CU_CANCEL_QUEUED_PRODUCTION: theUnit->CancelQueuedProduction(); return 0;
    case EE_UNITS_CU_GOAL_QUEUE_COUNT: return theUnit->mGoalQueues.size();
    case EE_UNITS_CU_CLEAR_GOAL_QUEUE: theUnit->ClearGoalQueue((unsigned long)inB); return 0;
    case EE_UNITS_CU_AFFECTING_KEEP_STRONGEST: theUnit->mAffectingAreaEffects.KeepStrongest(); return 0;
    case EE_UNITS_CU_AFFECTING_CLEAR: theUnit->mAffectingAreaEffects.Clear(); return 0;
    case EE_UNITS_CU_AFFECTING_ITEMS_CLEAR: theUnit->mAffectingAreaEffects.mItems.clear(); return 0;
    case EE_UNITS_CU_AREA_EFFECTS_CLEAR: theUnit->mAreaEffects.Clear(); return 0;
    case EE_UNITS_CU_AREA_EFFECTS_ITEMS_CLEAR: theUnit->mAreaEffects.mItems.clear(); return 0;
    case EE_UNITS_CU_RELEASE_CONVERSION_EFFECT: theUnit->ReleaseConversionEffect(); return 0;
    case EE_UNITS_CU_RELEASE_HEAL_EFFECT: theUnit->ReleaseHealEffect(); return 0;
    case EE_UNITS_CU_CLEAR_TINT: theUnit->ClearTint(); return 0;
    case EE_UNITS_CU_RELEASE_FIRE_EFFECT: theUnit->ReleaseFireEffect(); return 0;
    case EE_UNITS_CU_CLEAR_GOAL_STATE: theUnit->mGoalState = NULL; return 0;
    case EE_UNITS_CU_CLEAR_ACTIVE_GOAL: theUnit->mActiveGoal = NULL; return 0;
    case EE_UNITS_CU_UNIT_KILL: theUnit->EEUnit::Kill((inB & 1) != 0, (inB & 2) != 0, (inB & 4) != 0); return 0;
    // Invalidate's
    case EE_UNITS_CU_CURRENT_RESEARCH: return theUnit->mCurrentResearch;
    case EE_UNITS_CU_PLAY_EFFECT: theUnit->PlayEffect((long)inB); return 0;
    case EE_UNITS_CU_SOUND_STOP: gSoundManager->Stop(theUnit->mSound, false); return 0;
    case EE_UNITS_CU_SOUND: return (uint64_t)(uintptr_t)theUnit->mSound;
    case EE_UNITS_CU_SOUND_DEREFERENCE: theUnit->mSound->Dereference(); return 0;
    case EE_UNITS_CU_CLEAR_SOUND: theUnit->mSound = NULL; return 0;
    case EE_UNITS_CU_BUILD_TARGET_COUNT: return theUnit->mCitizenBuildQueue.size();
    case EE_UNITS_CU_BUILD_TARGET_DEREFERENCE: theUnit->mCitizenBuildQueue[(size_t)inB]->Dereference(); return 0;
    case EE_UNITS_CU_BUILD_TARGETS_CLEAR: theUnit->mCitizenBuildQueue.clear(); return 0;
    case EE_UNITS_CU_PATROL_GROUP: return (uint64_t)(uintptr_t)theUnit->mPatrolGroup;
    case EE_UNITS_CU_PATROL_REMOVE: ee_units_pg_remove(theUnit->mPatrolGroup, theUnit); return 0;
    case EE_UNITS_CU_PATROL_DEREFERENCE: ee_units_pg_dereference(theUnit->mPatrolGroup); return 0;
    case EE_UNITS_CU_CLEAR_PATROL_GROUP: theUnit->mPatrolGroup = NULL; return 0;
    case EE_UNITS_CU_CLEAR_PREVIOUS_PART: theUnit->mPreviousPart = NULL; return 0;
    case EE_UNITS_CU_CLEAR_NEXT_PART: theUnit->mNextPart = NULL; return 0;
    case EE_UNITS_CU_HOME_BASE: return (uint64_t)(uintptr_t)theUnit->mHomeBase;
    case EE_UNITS_CU_CLEAR_HOME_BASE: theUnit->mHomeBase = NULL; return 0;
    case EE_UNITS_CU_CLEAR_CONTAINER: theUnit->mContainer = NULL; return 0;
    case EE_UNITS_CU_MINE: return (uint64_t)(uintptr_t)theUnit->mMine;
    case EE_UNITS_CU_CLEAR_MINE: theUnit->mMine = NULL; return 0;
    case EE_UNITS_CU_CLEAR_HOLDER: theUnit->ClearHolder(); return 0;
    case EE_UNITS_CU_IGNORED_CLEAR: theUnit->mIgnoredTargets.clear(); return 0;
    case EE_UNITS_CU_RELEASE_SOUND: theUnit->ReleaseSound(); return 0;
    case EE_UNITS_CU_UNIT_REMOVE: theUnit->EEUnit::Remove(theUnit->mIsFoundation); return 0;
    case EE_UNITS_CU_IS_LAND_BUILDING: return theUnit->GetUnitClass()->IsLandBuilding();
    case EE_UNITS_CU_FOUNDATION_FIELD: return theUnit->mIsFoundation;
    case EE_UNITS_CU_UNLOAD_BUTTONS: theUnit->mPlayer->UnloadUnitClassButtons(theUnit->GetUnitClass()->GetUnitType()); return 0;
    case EE_UNITS_CU_CLEAR_FOUNDATION: theUnit->mIsFoundation = false; return 0;
    // StartProduction's
    case EE_UNITS_CU_QUEUE_FRONT_TECHNOLOGY: return theUnit->mBuildQueue.front().mTechnology;
    case EE_UNITS_CU_QUEUE_FRONT_FLAG: return theUnit->mBuildQueue.front().mFlag;
    case EE_UNITS_CU_TECH_NODE: return (uint64_t)(uintptr_t)theUnit->GetPlayer()->GetTechTree()->Get((long)inB);
    case EE_UNITS_CU_NODE_AVAILABLE: return theNode->GetIsAvailable() == true;
    case EE_UNITS_CU_CAN_PRODUCE: return theUnit->GetPlayer()->CanProduce((long)inB, theUnit->GetUnitClass()->GetUnitType(), 1, false, 2, NULL);
    case EE_UNITS_CU_NODE_MAKES_UNIT: return theNode->mData->mMakesUnit;
    case EE_UNITS_CU_NODE_UNIT_CLASS: return theNode->mData->mUnitClass;
    case EE_UNITS_CU_CLASS_TABLE: return (uint64_t)(uintptr_t)gUnitClassTable[(long)inB];
    case EE_UNITS_CU_BUILD_TIME_SCALE: return EECuBits(theUnit->GetPlayer()->GetUpgrade((long)inB)->mBuildTimeScale);
    case EE_UNITS_CU_CLASS_IS_AIRCRAFT: return theClass->IsAircraft();
    case EE_UNITS_CU_CLASS_WEIGHT: return theClass->mWeight;
    case EE_UNITS_CU_ATC_CARGO: return ee_units_atc_cargo(theUnit->GetPlayer()->mAirTraffic[theClass->mAirTrafficType - 1], (int32_t)inB);
    case EE_UNITS_CU_CLASS_UNIT_TYPE: return theClass->mUnitType;
    case EE_UNITS_CU_RANDOM: return EECuBits(gPhysics->mRandom.Generate());
    case EE_UNITS_CU_CLASS_TECHNOLOGY: return theClass->GetTechnology();
    case EE_UNITS_CU_INSTANT_BUILD: return theUnit->GetPlayer()->mInstantBuild;
    case EE_UNITS_CU_NODE_BUILD_TIME: return theNode->GetBuildTime();
    case EE_UNITS_CU_SET_PRODUCTION_TIME: theUnit->mProductionTime = (long)inB; return 0;
    case EE_UNITS_CU_UI_UPDATE_JIT: gUserInterface->UpdateJITManufacturing(); return 0;
    case EE_UNITS_CU_QUEUE_POP_FRONT: theUnit->mBuildQueue.erase(theUnit->mBuildQueue.begin()); return 0;
    case EE_UNITS_CU_REFUND_TECHNOLOGY:
        theUnit->GetPlayer()->RefundTechnology(theNode, theUnit->GetUnitClass()->GetUnitType(), EECuFloat(inB));
        return 0;
    case EE_UNITS_CU_QUEUE_EMPTY: return theUnit->mBuildQueue.empty();
    // SetBuilt's (inA the part)
    case EE_UNITS_CU_EE_PLAYER: return (uint64_t)(uintptr_t)dynamic_cast<EEPlayer*>(theOther->mPlayer);
    case EE_UNITS_CU_PART_IS_BUILT: return theOther->mIsBuilt;
    case EE_UNITS_CU_PART_IS_LAND_BUILDING: return theOther->mUnitClass->IsLandBuilding();
    case EE_UNITS_CU_PART_SIGHT_OFF: theOther->SightOff(theOther->GetTileX(), theOther->GetTileY()); return 0;
    case EE_UNITS_CU_PART_SET_BUILT: theOther->mIsBuilt = inB != 0; return 0;
    case EE_UNITS_CU_PART_SIGHT_ON: theOther->SightOn(theOther->GetTileX(), theOther->GetTileY()); return 0;
    case EE_UNITS_CU_PART_AIR_TRAFFIC_TYPE: return theOther->mUnitClass->mAirTrafficType;
    case EE_UNITS_CU_PART_ATC_ADD_AIRPORT:
        ee_units_atc_add_airport(EECuObject<EEPlayer>((uint64_t)inB)->mAirTraffic[inC - 1], theOther);
        return 0;
    case EE_UNITS_CU_PART_UNIT_COMPLETED: EECuObject<EEPlayer>((uint64_t)inB)->UnitCompleted(theOther); return 0;
    case EE_UNITS_CU_PART_UNIT_TYPE: return theOther->mUnitClass->GetUnitType();
    case EE_UNITS_CU_PART_REPLANT:
        gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageReplantFarms, {theOther->mUnitID}), kSlotNone);
        return 0;
    case EE_UNITS_CU_PART_LOAD_BUTTONS: theOther->mPlayer->LoadUnitClassButtons(theOther->mUnitClass->GetUnitType()); return 0;
    case EE_UNITS_CU_PART_PAID: theOther->mIsPaidFor = true; return 0;
    case EE_UNITS_CU_PART_NEXT: return (uint64_t)(uintptr_t)theOther->mNextPart;
    // StartAction's
    case EE_UNITS_CU_HAS_COMMAND: return theUnit->mCommand != NULL;
    case EE_UNITS_CU_COMMAND_TYPE: return theUnit->GetCommandType();
    case EE_UNITS_CU_UPDATE_CAMOUFLAGE: theUnit->UpdateCamouflage((long)inB); return 0;
    case EE_UNITS_CU_SET_COMMAND: theUnit->mCommand = theUnit->GetUnitClass()->FindAction((long)inB); return 0;
    // SetAnimation's (inA the model)
    case EE_UNITS_CU_MODEL: return (uint64_t)(uintptr_t)theUnit->GetModel();
    case EE_UNITS_CU_MODEL_START_FRAME: return (long)theModel->mStartFrames[inB];
    case EE_UNITS_CU_MODEL_LAST_FRAME: return (long)theModel->mLastFrames[inB];
    case EE_UNITS_CU_ANIMATION: return theUnit->mAnimation;
    case EE_UNITS_CU_SET_ANIMATION_FIELD: theUnit->mAnimation = (long)inB; return 0;
    case EE_UNITS_CU_PM_SET_MODEL: theUnit->mPhysicalModel.SetModel(theModel->mModels[inB]); return 0;
    case EE_UNITS_CU_PM_SET_FRAME: theUnit->mPhysicalModel.SetFrame((long)inB); return 0;
    case EE_UNITS_CU_CLEAR_ANIMATION_TIME: theUnit->mAnimationTime = 0; return 0;
    case EE_UNITS_CU_SET_PAUSED: theUnit->mIsAnimationPaused = inB != 0; return 0;
    case EE_UNITS_CU_SET_NEXT_FRAME: theUnit->SetNextFrame((long)inB); return 0;
    case EE_UNITS_CU_SOUND_STOP_UNLESS_LOOPING:
        gSoundManager->Stop(theUnit->mSound, !gSoundManager->GetIsLooping(theUnit->mSound));
        return 0;
    case EE_UNITS_CU_PM_FRAME: return (long)theUnit->mPhysicalModel.GetFrame();
    case EE_UNITS_CU_MODEL_SOUND1_FRAME: return (long)theModel->mSound1Frames[inB];
    case EE_UNITS_CU_MODEL_SOUND2_FRAME: return (long)theModel->mSound2Frames[inB];
    case EE_UNITS_CU_MODEL_SOUND1: return (uint64_t)(uintptr_t)theModel->mSounds1[inB];
    case EE_UNITS_CU_MODEL_SOUND2: return (uint64_t)(uintptr_t)theModel->mSounds2[inB];
    case EE_UNITS_CU_SOUND_PARAMETERS:
        theUnit->GetUnitClass()->GetSoundParameters(theUnit, EECuObject<EESoundObject>(inA), &f[0], &f[1], &f[2]);
        return 0;
    case EE_UNITS_CU_PLAY_FRAME_SOUND:
        theUnit->SetSound(gSoundManager->PlayAndReturnReferencedNode(
            EECuObject<EESoundObject>(inA)->GetSampleID(), f[0], f[1], f[2], EECuObject<EESoundObject>(inA)->GetLooping(),
            const_cast<char*>((const char*)theUnit->GetUnitClass()->mName)));
        return 0;
    // PayInsurance's (inA the player)
    case EE_UNITS_CU_PLAYER: return (uint64_t)(uintptr_t)theUnit->mPlayer;
    case EE_UNITS_CU_CLASS_TECH_NODE: return (uint64_t)(uintptr_t)thePlayer->mTechnologies->Get(theUnit->GetUnitClass()->mTechnology);
    case EE_UNITS_CU_REFUND_TOTAL: return thePlayer->mInsuranceRefunds[inB];
    case EE_UNITS_CU_RESOURCE: return thePlayer->mResourceCaps[inB];
    case EE_UNITS_CU_PLAYER_REFUND_TECHNOLOGY:
        theUnit->mPlayer->RefundTechnology(theNode, theUnit->GetUnitClass()->GetUnitType(), EECuFloat(inB));
        return 0;
    case EE_UNITS_CU_SET_REFUND_TOTAL: thePlayer->mInsuranceRefunds[inB] = (long)inC; return 0;
    case EE_UNITS_CU_CURRENT_PLAYER: return (uint64_t)(uintptr_t)::gWorld.GetCurrentPlayer();
    case EE_UNITS_CU_DISBAND_EFFECT:
        gEffectManager->CreateUnitEffect(theUnit, EEGraphicEffect::Get(kGraphicEffectDisbandUnitEmitter), NULL, true, 0, 0);
        return 0;
    case EE_UNITS_CU_INSURANCE_CHAT: EEInsuranceChat(thePlayer, (unsigned long)inB, (long)inC); return 0;
    case EE_UNITS_CU_INSURANCE_SOUND: return (uint64_t)(uintptr_t)EESoundObject::Get(kSoundInsuranceCollected);
    case EE_UNITS_CU_PLAY_AT_VOLUME:
        EECuObject<EESoundObject>(inA)->Play(EECuObject<EESoundObject>(inA)->GetVolume(), 0.0f, 1.0f);
        return 0;
    // FindContacts' (inA a class, 0: the unit's; a tile, a node or another unit)
    case EE_UNITS_CU_INNER_SPHERE: return EECuBits(theUnit->GetUnitClass()->GetInnerCollisionSphere());
    case EE_UNITS_CU_FLIES_OVER: return theClass->FliesOver();
    case EE_UNITS_CU_IS_AIR_CATEGORY: return theClass->IsAirCategory();
    case EE_UNITS_CU_LOCATION_X: return EECuBits(theUnit->mPhysicalModel.GetLocation().GetX());
    case EE_UNITS_CU_LOCATION_Y: return EECuBits(theUnit->mPhysicalModel.GetLocation().GetY());
    case EE_UNITS_CU_MAP_TILE: return (uint64_t)(uintptr_t)::gWorld.mMap->GetTile((long)inB, (long)inC);
    case EE_UNITS_CU_TILE_FIRST_NODE: return (uint64_t)(uintptr_t)EECuObject<EEMapTile>(inA)->mUnits;
    case EE_UNITS_CU_NODE_NEXT: return (uint64_t)(uintptr_t)EECuObject<RTSTileUnitNode>(inA)->mNext;
    case EE_UNITS_CU_NODE_COMPLEX: return (uint64_t)(uintptr_t)EECuObject<RTSTileUnitNode>(inA)->mUnit->GetComplexUnit();
    case EE_UNITS_CU_OTHER_COLLIDABLE: return theOther->mIsCollidable;
    case EE_UNITS_CU_OTHER_HIT_POINTS: return theOther->mHitPoints;
    case EE_UNITS_CU_OTHER_IS_FOUNDATION: return theOther->IsFoundation();
    case EE_UNITS_CU_OTHER_CLASS: return (uint64_t)(uintptr_t)theOther->mUnitClass;
    case EE_UNITS_CU_DOMAINS: return theClass->mDomains;
    case EE_UNITS_CU_CLASS_CATEGORY: return theClass->mCategory;
    case EE_UNITS_CU_ADD_CONTACT: theOther->AddContact(EECuObject<EEComplexUnit>((uint64_t)inB)); return 0;
    // the production queue's (cu_cargo.rs; inA a node or class)
    case EE_UNITS_CU_CURRENT_NODE: return (uint64_t)(uintptr_t)theUnit->GetPlayer()->GetTechTree()->Get(theUnit->mCurrentResearch);
    case EE_UNITS_CU_NODE_CLASS_INDEX: return theNode->mData->mUnitClass;
    case EE_UNITS_CU_SET_NODE_ALLOWED: theNode->mIsAllowed = inB != 0; return 0;
    case EE_UNITS_CU_NODE_ALLOWED: return theNode->mIsAllowed;
    case EE_UNITS_CU_CLASS_AI_KIND: return theClass->mAIKind;
    case EE_UNITS_CU_HEROES_IN_PRODUCTION: return theUnit->GetPlayer()->mNumHeroesInProduction;
    case EE_UNITS_CU_SET_HEROES_IN_PRODUCTION: theUnit->GetPlayer()->mNumHeroesInProduction = (long)inB; return 0;
    case EE_UNITS_CU_QUEUE_SIZE: return theUnit->mBuildQueue.size();
    case EE_UNITS_CU_QUEUE_BACK_TECHNOLOGY: return theUnit->mBuildQueue.back().mTechnology;
    case EE_UNITS_CU_QUEUE_POP_BACK: theUnit->mBuildQueue.pop_back(); return 0;
    case EE_UNITS_CU_PLAYER_NODE: return (uint64_t)(uintptr_t)theUnit->mPlayer->mTechnologies->Get((long)inB);
    case EE_UNITS_CU_REFUND_ONE: theUnit->mPlayer->RefundTechnology(theNode, theUnit->GetUnitClass()->mUnitType, 1.0f); return 0;
    case EE_UNITS_CU_SET_PRODUCTION_TIME_FIELD: theUnit->SetProductionTime((long)inB); return 0;
    case EE_UNITS_CU_QUEUE_CLEAR: theUnit->mBuildQueue.clear(); return 0;
    case EE_UNITS_CU_QUEUE_PUSH_BACK: theUnit->mBuildQueue.push_back(EECuEntry(inB, inC)); return 0;
    case EE_UNITS_CU_QUEUE_PUSH_FRONT: theUnit->mBuildQueue.insert(theUnit->mBuildQueue.begin(), EECuEntry(inB, inC)); return 0;
    case EE_UNITS_CU_QUEUE_TECHNOLOGY_AT: return theUnit->mBuildQueue[(size_t)inB].mTechnology;
    case EE_UNITS_CU_QUEUE_SET_TECHNOLOGY_AT: theUnit->mBuildQueue[(size_t)inB].mTechnology = (long)inC; return 0;
    case EE_UNITS_CU_QUEUE_ERASE_AT: theUnit->mBuildQueue.erase(theUnit->mBuildQueue.begin() + (size_t)inB); return 0;
    case EE_UNITS_CU_CAN_AFFORD: return theUnit->GetPlayer()->CanAfford(theNode, theUnit->GetUnitClass()->GetUnitType(), true);
    case EE_UNITS_CU_SCENARIO_GAME: return ::gWorld.mGameOptions.mIsScenarioGame;
    case EE_UNITS_CU_HERO_LIMIT: return theUnit->mPlayer->mHeroLimit;
    case EE_UNITS_CU_HERO_COUNT: return theUnit->mPlayer->GetHeroCount(1, 1);
    case EE_UNITS_CU_JIT_MANUFACTURING: return theUnit->mPlayer->mJITManufacturing;
    case EE_UNITS_CU_PAY: theUnit->mPlayer->Pay(theNode, theUnit->GetUnitClass()->GetUnitType(), 1.0f); return 0;
    case EE_UNITS_CU_NODE_KIND: return theNode->mData->mKind;
    // the cargo's (inA another unit)
    case EE_UNITS_CU_UNIT_WEIGHT: return theOther->mUnitClass->mWeight;
    case EE_UNITS_CU_CARGO_ROOM: return theUnit->mCargoRoom;
    case EE_UNITS_CU_SET_CARGO_ROOM: theUnit->mCargoRoom = (long)inB; return 0;
    case EE_UNITS_CU_CARGO_PUSH: theUnit->mCargo.push_back(theOther); return 0;
    case EE_UNITS_CU_REFERENCE: EECuObject<RTSUnit>(inA)->Reference(); return 0;
    case EE_UNITS_CU_UNIT_CONTAINER: return (uint64_t)(uintptr_t)theOther->mContainer;
    case EE_UNITS_CU_SET_CONTAINER_THIS: theOther->mContainer = theUnit; return 0;
    case EE_UNITS_CU_TAKE_OFF_MAP: theOther->TakeOffMap(); return 0;
    case EE_UNITS_CU_OFF_MAP_INFO: return (uint64_t)(uintptr_t)theOther->mPlayer->GetOffMapInfo(theOther);
    case EE_UNITS_CU_SET_OFF_MAP_REASON: EECuObject<EEWUnitsOffMapInfo>(inA)->mReason = (long)inB; return 0;
    case EE_UNITS_CU_IS_TRANSPORT: return theUnit->GetUnitClass()->IsTransport();
    case EE_UNITS_CU_CARGO_ADDED: theUnit->GetUnitClass()->CargoAdded(theUnit, theOther, inB != 0); return 0;
    case EE_UNITS_CU_ACTIVE_GROUP_HAS: return theUnit->mPlayer->mActiveGroup->Contains(theOther);
    case EE_UNITS_CU_POST_424: gUserInterface->PostMessage424Once(); return 0;
    case EE_UNITS_CU_UNIT_HIT_POINTS: return theOther->mHitPoints;
    case EE_UNITS_CU_UNIT_POPULATION_COST: return theOther->mUnitClass->GetPopulationCost();
    case EE_UNITS_CU_GARRISON_CAPACITY: return theUnit->GetUnitClass()->mGarrisonCapacity;
    case EE_UNITS_CU_GARRISON_COUNT: return theUnit->mGarrisonCount;
    case EE_UNITS_CU_SET_GARRISON_COUNT: theUnit->mGarrisonCount = (long)inB; return 0;
    case EE_UNITS_CU_GARRISON_POPULATION: return theUnit->mGarrisonPopulation;
    case EE_UNITS_CU_SET_GARRISON_POPULATION: theUnit->mGarrisonPopulation = (long)inB; return 0;
    case EE_UNITS_CU_KILL_UNIT: theOther->Kill(0, 0, 0); return 0;
    case EE_UNITS_CU_UNIT_DEAD_STATISTIC: return EEAddDeadUnitStatistic(theOther);
    case EE_UNITS_CU_CARGO_COUNT: return theUnit->mCargo.size();
    case EE_UNITS_CU_CARGO_AT: return (uint64_t)(uintptr_t)theUnit->GetCargo((unsigned long)inB);
    case EE_UNITS_CU_UNIT_PREVIOUS_PART: return (uint64_t)(uintptr_t)theOther->mPreviousPart;
    case EE_UNITS_CU_UNIT_UNIT_TYPE: return theOther->mUnitClass->GetUnitType();
    case EE_UNITS_CU_UNIT_CATEGORY: return theOther->mUnitClass->mCategory;
    case EE_UNITS_CU_UNIT_STATS_RESET: theOther->mPlayer->mSavedGroups.Reset((long)inB, 1); return 0;
    case EE_UNITS_CU_UNIT_STATS_LOWER: theOther->mPlayer->mSavedGroups.Lower((long)inB, 1); return 0;
    case EE_UNITS_CU_CARGO_REMOVED: theUnit->GetUnitClass()->CargoRemoved(theUnit, theOther); return 0;
    case EE_UNITS_CU_CARGO_ERASE_AT: theUnit->mCargo.erase(theUnit->mCargo.begin() + (size_t)inB); return 0;
    case EE_UNITS_CU_UNIT_CONTAINER_DEREFERENCE: theOther->mContainer->Dereference(); return 0;
    case EE_UNITS_CU_UNIT_CLEAR_CONTAINER: theOther->mContainer = NULL; return 0;
    case EE_UNITS_CU_CARGO_SET_AT: theUnit->mCargo[(size_t)inB] = theOther; return 0;
    case EE_UNITS_CU_CARGO_UNLOADED: theUnit->GetUnitClass()->CargoUnloaded(theUnit, theOther); return 0;
    case EE_UNITS_CU_UNIT_SET_TRANSFORMATION: theOther->SetTransformation(*EECuObject<GETransformation>((uint64_t)inB), true); return 0;
    case EE_UNITS_CU_UNIT_DOMAINS: return theOther->mUnitClass->mDomains;
    case EE_UNITS_CU_UNIT_SET_PLANET: theOther->mPlanetID = EEGetLandContinentOf(theOther); return 0;
    case EE_UNITS_CU_UNIT_CRUISE_ALTITUDE: return EECuBits(theOther->mUnitClass->GetCruiseAltitude());
    case EE_UNITS_CU_UNIT_SET_TRANSFORMATION_AT_Z:
        EECuSetTransformationAtZ(theOther, *EECuObject<GETransformation>((uint64_t)inB), EECuFloat(inC));
        return 0;
    case EE_UNITS_CU_UNIT_TILE_X: return theOther->GetTileX();
    case EE_UNITS_CU_ACTIVATE_UNIT: ::gWorld.ActivateUnit(theOther); return 0;
    case EE_UNITS_CU_DEACTIVATE_UNIT: ::gWorld.DeactivateUnit(theOther); return 0;
    case EE_UNITS_CU_UNIT_IS_HUMAN: return theOther->mPlayer->IsHuman();
    case EE_UNITS_CU_UNIT_START_ACTION: theOther->StartAction((long)inB); return 0;
    case EE_UNITS_CU_UNIT_ACTION_CHANGED: theOther->mUnitClass->ActionChanged(theOther); return 0;
    case EE_UNITS_CU_UNIT_REFERENCE_COUNT: return theOther->mReferenceCount;
    case EE_UNITS_CU_STATS_RANGE: return EECuBits(theUnit->GetStats()->mRange);
    case EE_UNITS_CU_TRANSPORT_HAS_ROOM:
        return EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theUnit->GetUnitClass())->HasRoom(theUnit, theOther->GetComplexUnit());
    case EE_UNITS_CU_TRANSPORT_UNLOAD_RANGE: return EECuBits(EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theUnit->GetUnitClass())->mUnloadRange);
    case EE_UNITS_CU_UNLOAD_CARGO_AT:
        return (uint64_t)(uintptr_t)theUnit->UnloadCargoAt((unsigned long)inB, *EECuObject<GETransformation>(inA));
    case EE_UNITS_CU_TELEPORT_IN_EFFECT:
        ::gWorld.GetEffectManager()->CreateUnitEffect(theOther, EEGraphicEffect::Get(kGraphicEffectCalamityTeleportIn), NULL, false, 0, 0);
        return 0;
    case EE_UNITS_CU_RELEASE_CARGO: theUnit->ReleaseCargo(theOther); return 0;
    case EE_UNITS_CU_UNLOAD_FAILED_TEXT: return EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theUnit->GetUnitClass())->GetUnloadFailedTextID();
    case EE_UNITS_CU_ADD_EVENT:
        theUnit->mPlayer->mEvents.AddEvent(U2DPoint<long>(theUnit->GetTileX(), theUnit->GetTileY()), 2, theUnit->mPlayer, (long)inB, false);
        return 0;
    case EE_UNITS_CU_SET99_HAS_TYPE: return gUnitSetManager.GetSet(99)->Contains(theUnit->GetUnitClass()->GetUnitType());
    case EE_UNITS_CU_TRANSPORT_DOMAINS: return EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theUnit->GetUnitClass())->mDomains;
    case EE_UNITS_CU_TILE_X_FIELD: return theUnit->mTileX;
    case EE_UNITS_CU_TILE_Y: return theUnit->GetTileY();
    case EE_UNITS_CU_TILE_SIZE: return theUnit->GetUnitClass()->GetTileSize();
    case EE_UNITS_CU_TRANSPORT_CLASS: return (uint64_t)(uintptr_t)EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theUnit->GetUnitClass());
    // the build queue's and contacts'
    case EE_UNITS_CU_GOAL_IS_BUILD: return theUnit->mGoalState != NULL && theUnit->mGoalState->mKind == kGoalBuildRepair;
    case EE_UNITS_CU_BUILD_GOAL_TARGET: return (uint64_t)(uintptr_t)EEBuildRepairGoalCast(theUnit->GetGoal())->GetBuildTarget();
    case EE_UNITS_CU_BUILD_QUEUE_PUSH_FRONT: theUnit->mCitizenBuildQueue.push_front(EECuObject<EEUnit>(inA)); return 0;
    case EE_UNITS_CU_BUILD_QUEUE_PUSH_BACK: theUnit->mCitizenBuildQueue.push_back(EECuObject<EEUnit>(inA)); return 0;
    case EE_UNITS_CU_BUILD_QUEUE_FIND: return EECuFind(theUnit->mCitizenBuildQueue, EECuObject<EEUnit>(inA));
    case EE_UNITS_CU_BUILD_QUEUE_ERASE_AT:
        theUnit->mCitizenBuildQueue.erase(theUnit->mCitizenBuildQueue.begin() + (size_t)inB);   // (Dinkum deque erase(P))
        return 0;
    case EE_UNITS_CU_DROP_FINISHED_GOALS: theUnit->DropFinishedGoals(0); return 0;
    case EE_UNITS_CU_CLEAR_GOALS: theUnit->ClearGoals(0); return 0;
    case EE_UNITS_CU_CLEAR_SHIFT_QUEUED: theUnit->mHasShiftQueuedBuild = false; return 0;
    case EE_UNITS_CU_CONTACT_MISSING: return theUnit->mContacts.Find(theOther) == 0;
    case EE_UNITS_CU_CONTACT_ADD_HEAD: theUnit->mContacts.AddHead(theOther); return 0;
    case EE_UNITS_CU_REFERENCE_THIS: theUnit->Reference(); return 0;
    // cu_misc.rs's (inA a part or a model, inB a player index)
    case EE_UNITS_CU_KEEP_MAP_POSITION: theUnit->GetPosition(theUnit->mLastMapPosition); return 0;
    case EE_UNITS_CU_DESELECT: theUnit->mPlayer->DeselectUnit(theOther); return 0;
    case EE_UNITS_CU_PLAYER_VIEWS: return EECuObject<EEPlayer>((uint64_t)inB)->mViewedUnit == theOther;
    case EE_UNITS_CU_PLAYER_UPDATE_SELECTION: EECuObject<EEPlayer>((uint64_t)inB)->UpdateSelection(NULL, true); return 0;
    case EE_UNITS_CU_PLAYER_SELECTED_VIEWS: return EECuObject<EEPlayer>((uint64_t)inB)->mSelectedViewedUnit == theOther;
    case EE_UNITS_CU_PLAYER_CLEAR_SELECTED_VIEWED: EECuObject<EEPlayer>((uint64_t)inB)->SetSelectedViewedUnit(NULL); return 0;
    case EE_UNITS_CU_PART_KEEP_MAP_POSITION: theOther->GetPosition(theOther->mLastMapPosition); return 0;
    case EE_UNITS_CU_PART_SET_AWAY:
        theOther->SetLocation(GE3DPoint(-10000.0f, -10000.0f, theOther->mPhysicalModel.GetLocation().GetZ()), true);
        return 0;
    case EE_UNITS_CU_HAS_GOAL_STATE: return theUnit->mGoalState != NULL;
    case EE_UNITS_CU_GOAL_KIND: return theUnit->mGoalState->mKind;
    case EE_UNITS_CU_CAST_CALAMITY_TYPE: return EECastCalamityGoalCast(theUnit->GetGoal())->GetCalamityType();
    case EE_UNITS_CU_STOP: theUnit->SetVelocity(0.0f); return 0;
    case EE_UNITS_CU_CLASS_DOMAINS: return theUnit->GetUnitClass()->mDomains;
    case EE_UNITS_CU_TILE: return (uint64_t)(uintptr_t)theUnit->mTile;
    case EE_UNITS_CU_TILE_TERRAIN: return EECuObject<RTSTile>(inA)->mTerrainType;
    case EE_UNITS_CU_TILE_LEVEL: return (char)EECuObject<RTSTile>(inA)->mAIValue;   // (the original's signed compare)
    case EE_UNITS_CU_WATER_KIND: return theUnit->GetUnitClass()->mWaterKind;
    case EE_UNITS_CU_SET_WATER_STATE: theUnit->mWaterState = (long)inB; return 0;
    case EE_UNITS_CU_UNIT_LOAD_MEDIA: theUnit->EEUnit::LoadMedia(inB != 0); return 0;
    case EE_UNITS_CU_LOAD_BUTTONS: theUnit->GetPlayer()->LoadUnitClassButtons(theUnit->GetUnitClass()->GetUnitType()); return 0;
    case EE_UNITS_CU_SET_FOUNDATION: theUnit->mIsFoundation = inB != 0; return 0;
    case EE_UNITS_CU_PM_SET_FOUNDATION_MODEL:
        theUnit->mPhysicalModel.SetModel(EEGetPlayerClassGraphic(theUnit->GetPlayer(), theUnit->GetUnitClass())->mModel);
        return 0;
    case EE_UNITS_CU_PM_SET_ANIMATION_MODEL: theUnit->mPhysicalModel.SetModel(theModel->mModels[theUnit->mAnimation]); return 0;
    case EE_UNITS_CU_CLEAR_TURRET_ANGLES: theUnit->mTurretAngles = NULL; return 0;
    case EE_UNITS_CU_RESIZE_TURRET_ANGLES: theUnit->ResizeTurretAngles(theModel->GetNumParts()); return 0;
    case EE_UNITS_CU_MODEL_ORBITAL_COUNT: return theModel->GetOrbitalCount();
    case EE_UNITS_CU_SET_ORBITAL_COUNT: theUnit->mNumOrbitals = (unsigned long)inB; return 0;
    case EE_UNITS_CU_CLEAR_ORBITALS: theUnit->mOrbitals = NULL; return 0;
    case EE_UNITS_CU_NEW_ORBITALS: theUnit->mOrbitals = new EEOrbital*[theUnit->mNumOrbitals]; return 0;
    case EE_UNITS_CU_NEW_ORBITAL: theUnit->mOrbitals[inB] = new EEOrbital(*theModel->mOrbitals[inB]); return 0;
    case EE_UNITS_CU_MODEL_WHEEL_COUNT: return theModel->GetWheelCount();
    case EE_UNITS_CU_SET_WHEEL_COUNT: theUnit->mNumWheels = (unsigned long)inB; return 0;
    case EE_UNITS_CU_CLEAR_WHEELS: theUnit->mWheels = NULL; return 0;
    case EE_UNITS_CU_NEW_WHEELS: theUnit->mWheels = new EEWheel*[theUnit->mNumWheels]; return 0;
    case EE_UNITS_CU_NEW_WHEEL: theUnit->mWheels[inB] = new EEWheel(*theModel->mWheels[inB]); return 0;
    case EE_UNITS_CU_WHEEL_ANGLE_ZERO: theUnit->mWheels[inB]->mAngle = 0.0f; return 0;
    case EE_UNITS_CU_DISABLED: return theUnit->mIsDisabled;
    case EE_UNITS_CU_SET_DISABLED_FIELD: theUnit->mIsDisabled = inB != 0; return 0;
    case EE_UNITS_CU_SET_DISABLED_REASON: theUnit->mDisabledReason = (long)inB; return 0;
    case EE_UNITS_CU_ATC_ADD_AIRPORT: ee_units_atc_add_airport(theUnit->mPlayer->mAirTraffic[inB - 1], theUnit); return 0;
    case EE_UNITS_CU_SET_TINTED: theUnit->SetIsTinted(inB != 0); return 0;
    case EE_UNITS_CU_TIME_WARP_COLOR: theUnit->SetColor(kTimeWarpColor); return 0;
    case EE_UNITS_CU_IS_CLOAKED: return theUnit->IsCloaked();
    case EE_UNITS_CU_HAS_CAMOUFLAGE: return gCivPowersManager.HasPower(theUnit->GetPlayer(), kCivPowerCamouflage);
    case EE_UNITS_CU_CAMOUFLAGE_SET:
        return (uint64_t)(uintptr_t)gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet);
    case EE_UNITS_CU_VITAL_BUILDINGS: return theUnit->mPlayer->mNumVitalBuildings;
    case EE_UNITS_CU_CLOAKABLE: return theUnit->mIsCloakable;
    case EE_UNITS_CU_CLOAKED_FIELD: return theUnit->mIsCloaked;
    case EE_UNITS_CU_CLOAKED_ENEMY:
        return ee_units_uu_cloaked_enemy((uint64_t)(uintptr_t)theUnit, (uint64_t)(uintptr_t)(::gWorld.GetCurrentPlayer()));
    case EE_UNITS_CU_MINIMAP_REMOVE: gMiniMap->MoveUnit(theUnit, -10000, -10000, theUnit->GetTileX(), theUnit->GetTileY()); return 0;
    case EE_UNITS_CU_CLEAR_CLOAKABLE: theUnit->mIsCloakable = false; return 0;
    case EE_UNITS_CU_CLEAR_CLOAKED: theUnit->mIsCloaked = false; return 0;
    case EE_UNITS_CU_SET_CAMOUFLAGE_TIMER: theUnit->mCamouflageTimer = (unsigned long)inB; return 0;
    case EE_UNITS_CU_SET_U32C: theUnit->mU32C = (unsigned long)inB; return 0;
    case EE_UNITS_CU_CAMOUFLAGE_TIMING2: return gWorldTable.GetLongValue2(kWorldCamouflageTiming);
    case EE_UNITS_CU_CAMOUFLAGE_TIMING: return gWorldTable.GetLongValue(kWorldCamouflageTiming);
    case EE_UNITS_CU_GATHERED: return (uint64_t)(uintptr_t)theUnit->mGatheredResource;
    case EE_UNITS_CU_RELEASE_RESOURCE_USER: ::gWorld.ReleaseResourceUser(theUnit->mGatheredResource); return 0;
    case EE_UNITS_CU_GATHERED_VALID: return theUnit->mGatheredResource->mIsValid;
    case EE_UNITS_CU_WAITING_GATHERER: return (uint64_t)(uintptr_t)EEGetWaitingGatherer(theUnit->mGatheredResource);
    case EE_UNITS_CU_SET_GATHERED_FIELD: theUnit->mGatheredResource = EECuObject<EEUnit>(inA); return 0;
    case EE_UNITS_CU_ADD_RESOURCE_USER: ::gWorld.AddResourceUser(theUnit->mGatheredResource); return 0;
    case EE_UNITS_CU_POST_UNLOAD_TECHNOLOGY:
        gDataPreLoading->PostUnloadTechnology(theUnit->mPlayer->mPlayerID, theUnit->mCurrentResearch);
        return 0;
    case EE_UNITS_CU_UNLOAD_TECHNOLOGY_MEDIA:
        theUnit->GetPlayer()->GetTechTree()->Get(theUnit->mCurrentResearch)->UnloadMedia(theUnit->GetPlayer());
        return 0;
    case EE_UNITS_CU_SET_CURRENT_RESEARCH: theUnit->mCurrentResearch = (long)inB; return 0;
    case EE_UNITS_CU_POST_LOAD_TECHNOLOGY: gDataPreLoading->PostLoadTechnology(theUnit->mPlayer->mPlayerID, (long)inB); return 0;
    case EE_UNITS_CU_IS_WALL: return theUnit->GetUnitClass()->mIsWall;
    case EE_UNITS_CU_FIRE_EFFECT: return theUnit->mFireEffect.mValue;
    case EE_UNITS_CU_DESTROY_FIRE_EFFECT: gEffectManager->DestroyEffect(theUnit->mFireEffect); return 0;
    case EE_UNITS_CU_CLEAR_FIRE_EFFECT: theUnit->mFireEffect.mValue = 0; return 0;
    case EE_UNITS_CU_CLASS_TYPE_FIELD: return theUnit->GetUnitClass()->mUnitType;
    case EE_UNITS_CU_REMOVE_CLOAK_EFFECT: EERemoveCloakEffect(theUnit); return 0;
    case EE_UNITS_CU_CLEAR_BUILD_TARGETS: theUnit->ClearBuildTargets(); return 0;
    case EE_UNITS_CU_ATC_CLEAR_AIRPORT_QUEUE:
        ee_units_atc_clear_airport_queue(theUnit->mPlayer->mAirTraffic[theUnit->GetUnitClass()->mAirTrafficType - 1], theUnit, (uint32_t)inB);
        return 0;
    case EE_UNITS_CU_HAS_COMPUTER_PLAYER: return theUnit->mPlayer->mComputerPlayer != NULL;
    case EE_UNITS_CU_RANDOM_POINT_AHEAD: return EEGetRandomPointAhead(theUnit, reinterpret_cast<GE2DPoint*>(f));
    case EE_UNITS_CU_FLEE_GOAL:
        theUnit->SetGoal(EEUGRust::NewLocation(GE2DPoint(f[0], f[1]), true, true, false, 0.0f, false, false), 0, 0);
        return 0;
    case EE_UNITS_CU_PREVIOUS_NEXT: return (uint64_t)(uintptr_t)theUnit->mPreviousPart->mNextPart;
    case EE_UNITS_CU_CLEAR_PREVIOUS_NEXT: theUnit->mPreviousPart->mNextPart = NULL; return 0;
    case EE_UNITS_CU_SET_PREVIOUS_PART: theUnit->mPreviousPart = theOther; return 0;
    case EE_UNITS_CU_SET_PREVIOUS_NEXT_THIS: theUnit->mPreviousPart->mNextPart = theUnit; return 0;
    case EE_UNITS_CU_FIRST_QUEUE_LEN: return theUnit->mGoalQueues[0].mGoals.size();
    case EE_UNITS_CU_ABANDON_GOAL: theUnit->mGoalQueues[0].GetGoalAt((unsigned long)inB)->Abandon(theUnit); return 0;
    case EE_UNITS_CU_GOAL_QUEUE_CLEAR: theUnit->mGoalQueues[(size_t)inB].Clear(); return 0;
    case EE_UNITS_CU_HAS_CLASS: return theUnit->GetUnitClass() != NULL;
    case EE_UNITS_CU_REFRESH_GOALS: theUnit->RefreshGoals(); return 0;
    case EE_UNITS_CU_REFRESH_ACTIVE_GOAL: theUnit->RefreshActiveGoal(); return 0;
    case EE_UNITS_CU_CIRCLES:
        return theUnit->GetUnitClass()->mCategory == kUnitCategoryAirplane || theUnit->GetUnitClass()->mCategory == kUnitCategoryPlanetSpaceship;
    }
    return 0;
}

// The C++ calls of the Rust path bodies (ee_sim::units waypoints.rs, ee_sim::astar pm_apply.rs): one statement each
extern "C" int64_t ee_units_wp_host(int32_t inOp, uint64_t inUnit, uint64_t inArg, float* ioPoint)
{
    EEComplexUnit* theUnit = (EEComplexUnit*)(uintptr_t)inUnit;
    EEUnitGoal* theGoal = (EEUnitGoal*)(uintptr_t)inArg;
    switch (inOp) {
    case EE_UNITS_WP_SET_GOAL: theUnit->SetGoal(theGoal, 1, 0); return 0;
    case EE_UNITS_WP_INSERT_GOAL: theUnit->mGoalQueues[0].Insert(1, theGoal); return 0;
    case EE_UNITS_WP_REFRESH_GOALS: theUnit->RefreshGoals(); return 0;
    case EE_UNITS_WP_REFRESH_ACTIVE_GOAL: theUnit->RefreshActiveGoal(); return 0;
    case EE_UNITS_WP_START_ACTION: theUnit->StartAction((unsigned long)inArg); return 0;
    case EE_UNITS_WP_ACTION_CHANGED: theUnit->GetUnitClass()->ActionChanged(theUnit); return 0;
    case EE_UNITS_WP_QUEUE_LENGTH: return (int64_t)theUnit->mGoalQueues[0].mGoals.size();
    case EE_UNITS_WP_QUEUE_BACK: return (int64_t)(uintptr_t)theUnit->mGoalQueues[0].mGoals.back();
    case EE_UNITS_WP_IS_HUMAN: return theUnit->GetPlayer()->IsHuman();
    case EE_UNITS_WP_CLEAR_BOUNDED: theUnit->mBoundedPathTiles.clear(); return 0;
    case EE_UNITS_WP_REGION_SIZE: return gEEAIRegionSize;
    case EE_UNITS_WP_GOAL_POINT: { GE3DPoint thePoint = EELocationGoalPoint(theGoal); memcpy(ioPoint, &thePoint.mX, 12); return 0; }
    case EE_UNITS_WP_CLOSEST_TILE: {
        GE3DPoint thePoint(ioPoint[0], ioPoint[1], ioPoint[2]);
        long theX = 0, theY = 0;
        ::gWorld.GetMap()->GetClosestTile(&thePoint, &theX, &theY);
        return (int64_t)(uint32_t)theX | ((int64_t)(uint32_t)theY << 32);
    }
    }
    return 0;
}
