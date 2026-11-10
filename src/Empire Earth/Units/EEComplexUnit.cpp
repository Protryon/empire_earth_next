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
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEUserInterface.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUCLandable.h"
#include "ee/EEUCTransport.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGBuildRepair.h"
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
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EEGameSound.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGCastCalamity.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EEGroupPathRequest.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEUnitAction.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEFleeFromLocationScreenBase.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGAttackGround.h"
#include "ee/EEUGUnloadTransport.h"
#include "ee/EEUGGather.h"
#include "ee/EEAIPathSector.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EECPASGUProphetCalamityAnalyzerBase.h"
#include "ee/EECPASGUProphet.h"

// Existing cdecl helper defined in EEWheel.cpp; original 0x23cd32 fills a real GE2DPoint.
bool EEGetRandomPointAhead(EEComplexUnit* inUnit, GE2DPoint* outPoint);

// The genuine existing free predicate at 0x2414d2 (defined in EEFOFindNewTargetNearOldTarget.cpp).
bool EEIsSameResearch(long inFirst, long inSecond);

// chain at 0x5f57d3 (wrapper)
//   object 0x963228  ctor None  dtor None
//   inline ctor, fields (offset=value, declaration order unknown): +0=0.2, +4=0.2, +8=1
//   dtor stub is a bare `ret`: the class has a user-declared empty inline destructor
//   object bound as _kTimeWarpColor
// the colour of a time-warped unit (SetDisabled 0x1fe5b3: SetColor when +0x358 is 0x1a/0x1c) (provisional name)
const GEColor kTimeWarpColor(0.2f, 0.2f, 1.0f);

// 0x963234: when set, DropPath moves the unit through its waypoints instead of dropping them (only this unit
// reads it) (provisional name)
static bool sB563234;
// 0x963238: the time of the last fanaticism colour update (UpdateFanaticismColor; only this unit) (provisional name)
static unsigned long sU563238;
// 0x963abc: gUnitClassTable[0x104], the unit class whose units mark the waypoints while sB563234 is set
// (only AddWaypoint reads it; the address is a gUnitClassTable slot, filled by CreateUnitClasses like every
// other class; no separate static)
// SYNTHETIC: EXE 0x5f57d3   (wrapper)
// SYNTHETIC: EXE 0x5f57dd   (ctor)
// SYNTHETIC: EXE 0x5f57f8   (registrar)
// SYNTHETIC: EXE 0x5f5804   (dtor)

// chain at 0x5f5805 (wrapper)
//   object 0x963218  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
//   guarded dtor (byte ptr [0x963210], 1): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
//   object bound as ?sData@EEComplexUnit@@2VUString@@B
const UString EEComplexUnit::sData("EEComplexUnit-Data");   // class static member (owner from the key prefix; EEComplexUnit declares `static const UString ...;`; provisional name)
// SYNTHETIC: EXE 0x5f5805   (wrapper)
// SYNTHETIC: EXE 0x5f580f   (ctor)
// SYNTHETIC: EXE 0x5f5820   (registrar)
// SYNTHETIC: EXE 0x5f582c   (dtor)

// chain at 0x5f5848 (wrapper)
//   object 0x963200  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
//   guarded dtor (byte ptr [0x963210], 2): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
//   object bound as ?sCloakableOpacity@EEComplexUnit@@2VUString@@B
const UString EEComplexUnit::sCloakableOpacity("EEComplexUnit-CloakableOpacity");   // class static member (owner from the key prefix; EEComplexUnit declares `static const UString ...;`; provisional name)
// SYNTHETIC: EXE 0x5f5848   (wrapper)
// SYNTHETIC: EXE 0x5f5852   (ctor)
// SYNTHETIC: EXE 0x5f5863   (registrar)
// SYNTHETIC: EXE 0x5f586f   (dtor)

// chain at 0x5f588b (wrapper)
//   object 0x9631f0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
//   guarded dtor (byte ptr [0x963210], 4): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
//   object bound as ?sIsCloaked@EEComplexUnit@@2VUString@@B
const UString EEComplexUnit::sIsCloaked("EEComplexUnit-IsCloaked");   // class static member (owner from the key prefix; EEComplexUnit declares `static const UString ...;`; provisional name)
// SYNTHETIC: EXE 0x5f588b   (wrapper)
// SYNTHETIC: EXE 0x5f5895   (ctor)
// SYNTHETIC: EXE 0x5f58a6   (registrar)
// SYNTHETIC: EXE 0x5f58b2   (dtor)

// chain at 0x5f58ce (wrapper)
//   object 0x9631e0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
//   guarded dtor (byte ptr [0x963210], 8): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
//   object bound as ?sDrawAreaEffects@EEComplexUnit@@2VUString@@B
const UString EEComplexUnit::sDrawAreaEffects("EEComplexUnit-DrawAreaEffects");   // class static member (owner from the key prefix; EEComplexUnit declares `static const UString ...;`; provisional name)
// SYNTHETIC: EXE 0x5f58ce   (wrapper)
// SYNTHETIC: EXE 0x5f58d8   (ctor)
// SYNTHETIC: EXE 0x5f58e9   (registrar)
// SYNTHETIC: EXE 0x5f58f5   (dtor)

// chain at 0x5f5911 (wrapper)
//   object 0x9631d0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
//   guarded dtor (byte ptr [0x963210], 0x10): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
//   object bound as ?sAreaEffectsTick@EEComplexUnit@@2VUString@@B
const UString EEComplexUnit::sAreaEffectsTick("EEComplexUnit-AreaEffectsTick");   // class static member (owner from the key prefix; EEComplexUnit declares `static const UString ...;`; provisional name)
// SYNTHETIC: EXE 0x5f5911   (wrapper)
// SYNTHETIC: EXE 0x5f591b   (ctor)
// SYNTHETIC: EXE 0x5f592c   (registrar)
// SYNTHETIC: EXE 0x5f5938   (dtor)

// chain at 0x5f5954 (wrapper)
//   object 0x963198  ctor ??0?$UMemoryPool@VEEComplexUnit@@@@QAE@XZ  dtor ?Release@?$UMemoryPool@VEEComplexUnit@@@@QAE_NXZ
//   guarded dtor (byte ptr [0x963210], 0x20): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
//   object bound as ?sMemoryPool@EEComplexUnit@@2V?$UMemoryPool@VEEComplexUnit@@@@A
UMemoryPool<EEComplexUnit> EEComplexUnit::sMemoryPool;
// SYNTHETIC: EXE 0x5f5954   (wrapper)
// SYNTHETIC: EXE 0x5f595e   (ctor)
// SYNTHETIC: EXE 0x5f5968   (registrar)
// SYNTHETIC: EXE 0x5f5974   (dtor)

// FUNCTION: EXE 0x5f5999
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

// FUNCTION: EXE 0x5f5b5b
// Adds the unit's line of sight at its current tile
void EEComplexUnit::SightOnHere()
{
    this->SightOn(this->GetTileX(), this->GetTileY());
}

// FUNCTION: EXE 0x5f5b67
// Returns whether the unit is still a building foundation
bool EEComplexUnit::IsFoundation()
{
    return this->mIsFoundation;
}

// FUNCTION: EXE 0x5f5b6e
// Resets the unit to its default state and marks it invalid (back in the pool)
void EEComplexUnit::Clear()
{
    this->ResetEEComplexUnit(true, false);
    this->ResetEEUnit();
    this->Reset();
    this->mIsValid = false;
}

// FUNCTION: EXE 0x5f5b8e
// Removes the unit's line of sight at its current tile
void EEComplexUnit::SightOffHere()
{
    this->SightOff(this->GetTileX(), this->GetTileY());
}

// FUNCTION: EXE 0x5f5b9a
// Sets whether the unit is a building foundation
void EEComplexUnit::SetFoundation(bool inIsFoundation)
{
    this->mIsFoundation = inIsFoundation;
}

// FUNCTION: EXE 0x5f5ba7
// Destroys the unit, freeing its turret angles, orbitals, wheels and model
EEComplexUnit::~EEComplexUnit()
{
    this->FreeNode();
}

// FUNCTION: EXE 0x5f5c42
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

// FUNCTION: EXE 0x5f5ca2
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

// FUNCTION: EXE 0x5f5cc7
// Ignores a target for 30 seconds from both units' current tiles, or renews an existing ignore entry
void EEComplexUnit::IgnoreTarget(EEComplexUnit* inTarget)
{
    std::deque<EEComplexUnitAttacker>::iterator theIterator;
    EEComplexUnitAttacker theAttacker;
    theIterator = std::find(this->mIgnoredTargets.begin(), this->mIgnoredTargets.end(), inTarget->GetUnitID());
    if (theIterator == this->mIgnoredTargets.end()) {
        theAttacker.mTileX = this->GetTileX();
        theAttacker.mTileY = this->GetTileY();
        theAttacker.mFromX = inTarget->GetTileX();
        theAttacker.mFromY = inTarget->GetTileY();
        theAttacker.mUnitID = inTarget->mUnitID;
        theAttacker.mTime = gPhysics->mGameTime + 30000;
        this->mIgnoredTargets.push_front(theAttacker);  // newest first: 0x2000f9 is deque::push_front (_Myoff ? _Myoff : _Mapsize * 0xaa, then --); IsTargetIgnored erases from an expired entry to the end
    } else {
        long theTime = gPhysics->mGameTime;
        (*theIterator).mTime = theTime + 30000;
    }
}

// FUNCTION: EXE 0x5f5d8b
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

// FUNCTION: EXE 0x5f5da1
// Loads a living unit as cargo if there is room (or room is ignored), taking it off the map; returns 0 or 7
long EEComplexUnit::LoadCargo(EEComplexUnit* inCargo, bool inIgnoreRoom, bool inArg3)
{
    long theResult;
    long theCost = inCargo->mUnitClass->mWeight;
    if ((this->mCargoRoom >= theCost || inIgnoreRoom) && inCargo->mHitPoints > 0) {
        this->mCargoRoom -= theCost;
        this->mCargo.push_back(inCargo);
        inCargo->Reference();
        if (inCargo->mContainer != this) {
            inCargo->mContainer = this;
            inCargo->mContainer->Reference();
        }
        inCargo->TakeOffMap();
        EEWUnitsOffMapInfo* theInfo = inCargo->mPlayer->GetOffMapInfo(inCargo);
        if (theInfo != NULL) {
            if (this->GetUnitClass()->IsTransport()) {
                if (this->GetUnitClass()->GetUnitType() != kUnitTypeFortress)
                    theInfo->mReason = 1;
                else
                    theInfo->mReason = 6;
            } else if (this->GetUnitClass()->GetUnitType() == kUnitTypeAirport)
                theInfo->mReason = 5;
        }
        this->GetUnitClass()->CargoAdded(this, inCargo, inArg3);
        theResult = 0;
        if (this->mPlayer == ::gWorld.GetCurrentPlayer()) {
            if (this->mPlayer->mActiveGroup->Contains(this) || this->mPlayer->mActiveGroup->Contains(inCargo))
                gUserInterface->PostMessage424Once();
        }
    } else
        theResult = 7;
    return theResult;
}

// FUNCTION: EXE 0x5f5ebf
// Garrisons a unit if capacity allows: adds its weight and population, then removes it as a dead unit
bool EEComplexUnit::GarrisonUnit(EEComplexUnit* inCargo)
{
    long theCost = inCargo->mUnitClass->mWeight;
    long theValue = inCargo->mUnitClass->GetPopulationCost();
    bool theResult = false;
    if (this->GetUnitClass()->mGarrisonCapacity - this->mGarrisonCount >= theCost) {
        this->mGarrisonCount += theCost;
        this->mGarrisonPopulation += theValue;
        inCargo->TakeOffMap();
        EEWUnitsOffMapInfo* theInfo = inCargo->mPlayer->GetOffMapInfo(inCargo);
        if (theInfo != NULL)
            theInfo->mReason = 7;
        inCargo->Kill(0, 0, 0);
        long theSeries = EEAddDeadUnitStatistic(inCargo);
        if (theSeries != 0)
            this->mPlayer->mSavedGroups.Lower(theSeries, 1);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5f5f55
// Adds garrison weight (never below 0) and sets the garrisoned population, if capacity allows
bool EEComplexUnit::AddGarrison(long inAmount, long inGarrisonPopulation)
{
    bool theResult = false;
    if (this->GetUnitClass()->mGarrisonCapacity - this->mGarrisonCount >= inAmount) {
        this->mGarrisonCount += inAmount;
        if (this->mGarrisonCount < 0)
            this->mGarrisonCount = 0;
        this->mGarrisonPopulation = inGarrisonPopulation;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5f5f97
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
    // the queue entry (technology, flag): an EEProductionEntry, mBuildQueue's 8-byte element (inferred)
    EETechTreeNode* theNode = this->GetPlayer()->GetTechTree()->Get(inTechnology);
    if (!this->GetPlayer()->CanAfford(theNode, this->GetUnitClass()->GetUnitType(), true))
        return;
    bool theAllowed = theNode->mIsAllowed;
    if (theAllowed && theNode->mData->mUnitClass > 0 && gUnitClassTable[theNode->mData->mUnitClass] != NULL
        && gUnitClassTable[theNode->mData->mUnitClass]->mAIKind == kAIKindHero && !::gWorld.mGameOptions.mIsScenarioGame) {
        long theLimit = this->mPlayer->mHeroLimit;
        theAllowed = this->mPlayer->GetHeroCount(1, 1) < theLimit;
    }
    if (!theAllowed)
        return;
    EEProductionEntry theEntry;
    bool theFlag = this->mPlayer->mJITManufacturing;
    theEntry.mTechnology = inTechnology;
    theEntry.mFlag = theFlag != 0;
    if (inFront)
        this->mBuildQueue.insert(this->mBuildQueue.begin(), theEntry);
    else
        this->mBuildQueue.push_back(theEntry);
    if (theNode->mData->mUnitClass > 0 && gUnitClassTable[theNode->mData->mUnitClass] != NULL
        && gUnitClassTable[theNode->mData->mUnitClass]->mAIKind == kAIKindHero)
        this->mPlayer->mNumHeroesInProduction++;
    this->mPlayer->Pay(theNode, this->GetUnitClass()->GetUnitType(), 1.0f);
    if ((theNode->mData->mUnitClass < 0 || gUnitClassTable[theNode->mData->mUnitClass] == NULL) && theNode->mData->mKind != 3) {
        theNode->mIsAllowed = false;
        gUserInterface->UpdateJITManufacturing();
    }
}

// FUNCTION: EXE 0x5f60ea
// Stops the citizen's goals and puts the target first in its build queue, keeping a current build target
void EEComplexUnit::OrderRepair(EEUnit* inTarget)
{
    EEUnit* theTarget;
    if (this->mGoalState != NULL && this->mGoalState->mKind == kGoalBuildRepair) {
        theTarget = EEGoalCast<EEUGBuildRepair>(this->GetGoal())->mTarget;
        theTarget->Reference();
        this->mCitizenBuildQueue.push_front(theTarget);
    }
    this->DropFinishedGoals(0);
    this->ClearGoals(0);
    this->StartAction(kActionIdle);
    this->GetUnitClass()->ActionChanged(this);
    std::deque<EEUnit*>::iterator theIterator = std::find(this->mCitizenBuildQueue.begin(), this->mCitizenBuildQueue.end(), inTarget);
    if (theIterator == this->mCitizenBuildQueue.end())
        inTarget->Reference();
    else
        this->mCitizenBuildQueue.erase(theIterator);  // the single-iterator erase COMDAT 0x2001b5 (Dinkum deque erase(P))
    this->mCitizenBuildQueue.push_front(inTarget);
    // Port: an order at once (no shift) brings back the smart task after the build
    this->mHasShiftQueuedBuild = false;
}

// FUNCTION: EXE 0x5f61c8
// Appends a building to the citizen's build queue unless it is already queued
void EEComplexUnit::AddBuildTarget(EEUnit* inTarget)
{
    std::deque<EEUnit*>::iterator theIterator = std::find(this->mCitizenBuildQueue.begin(), this->mCitizenBuildQueue.end(), inTarget);
    if (theIterator == this->mCitizenBuildQueue.end()) {
        inTarget->Reference();
        this->mCitizenBuildQueue.push_back(inTarget);
    }
}

// FUNCTION: EXE 0x5f6226
// Adds a unit to the contact (collision) list unless it is already there
void EEComplexUnit::AddContact(EEComplexUnit* inUnit)
{
    if (this->mContacts.Find(inUnit) == 0) {
        inUnit->Reference();
        this->mContacts.AddHead(inUnit);
    }
}

// FUNCTION: EXE 0x5f625b
// Appends a waypoint with its running path distance (off-map points are snapped or refused)
void EEComplexUnit::AddWaypoint(const GE3DPoint& inLocation, bool inSnapToMap)
{
    EEComplexUnitWaypoint theWaypoint;
    theWaypoint.mDistance = 0.0f;
    theWaypoint.mStartDistance = 0.0f;
    unsigned long theSize = this->mWaypoints.size();
    long theLast = theSize - 1;
    EEMap* theMap = ::gWorld.mMap;
    long theX = UMFloor(inLocation.GetX());
    long theY = UMFloor(inLocation.GetY());
    EEMapTile* theTile = theMap ? theMap->GetTile(theX, theY) : 0;
    bool theValid;
    if (!inSnapToMap)
        theValid = !theMap->IsEdgeTile(theX, theY);
    else {
        if (theTile == NULL)
            theMap->GetClosestTile(const_cast<GE3DPoint*>(&inLocation), &theX, &theY);
        theValid = true;
    }
    if (!theValid)
        return;
    if (sB563234 && gUnitClassTable[kUnitTypeWaypoint] != NULL)
        gUnitClassTable[kUnitTypeWaypoint]->CreateUnit(theX, theY, this->mPlayer->mPlayerID, false, NULL)->SetHitPoints(this->mUnitID);
    if (theSize > 0) {
        float theDistance = sqrt((inLocation.GetX() - this->mWaypoints[theLast].mLocation.GetX())
                                     * (inLocation.GetX() - this->mWaypoints[theLast].mLocation.GetX())
                                 + (inLocation.GetY() - this->mWaypoints[theLast].mLocation.GetY())
                                     * (inLocation.GetY() - this->mWaypoints[theLast].mLocation.GetY())
                                 + (inLocation.GetZ() - this->mWaypoints[theLast].mLocation.GetZ())
                                     * (inLocation.GetZ() - this->mWaypoints[theLast].mLocation.GetZ()));
        theWaypoint.mStartDistance = this->mWaypoints[theLast].mDistance;
        theWaypoint.mDistance = theWaypoint.mStartDistance + theDistance;
    } else {
        theWaypoint.mStartDistance = 0.0f;
        theWaypoint.mDistance = sqrt((inLocation.GetX() - this->mPhysicalModel.GetLocation().GetX())
                                         * (inLocation.GetX() - this->mPhysicalModel.GetLocation().GetX())
                                     + (inLocation.GetY() - this->mPhysicalModel.GetLocation().GetY())
                                         * (inLocation.GetY() - this->mPhysicalModel.GetLocation().GetY())
                                     + (inLocation.GetZ() - this->mPhysicalModel.GetLocation().GetZ())
                                         * (inLocation.GetZ() - this->mPhysicalModel.GetLocation().GetZ()));
    }
    theWaypoint.mLocation = inLocation;
    this->mWaypoints.push_back(theWaypoint);
}
#include "ee/EEStringIDs.h"
#include "ee/EEEffectManager.h"
#include "ee/EESoundObject.h"
#include "lle/UResourceFile.h"
// Original cdecl sight-map family, retained as link calls (no helper implementations).
void EEAddSightCircle(float inX, float inY, float inRadius, EEPlayer* inPlayer);
void EERemoveSightCircle(float inX, float inY, float inRadius, EEPlayer* inPlayer);
void EEMoveSightCircle(float inOldX, float inOldY, float inNewX, float inNewY, float inRadius, EEPlayer* inPlayer);
static const float kNoHeading = -10000.0f;      // 0x446db4 (ResetEEComplexUnit's mInitialContactHeading; provisional name)
static const float kMinimumFootprint = 1.0f;   // 0x446db8 (FindContacts; provisional name)
static const float kPathSpeedScale = 5.0f;     // 0x446dbc (ShouldMoveToNextAutoWaypoint: mFormationRank / 5 region sizes; provisional name)
bool gSightUpdatesEnabled = true;           // 0x4c76ec (.data = 1; just before this unit's 0x4c76f0 table; EEUserInterface/EEWorld/EEMap read it; provisional name)

// FUNCTION: EXE 0x5f65bd
// Refunds part of the unit's cost as insurance and tells the local player when a refund threshold is passed
// Original 0x4c76f0: the notification thresholds, used in ascending order.
static const long kResourceRefundThresholds[11] = {100, 200, 300, 400, 500, 1000, 2000, 3000, 4000, 5000, 10000};
extern char gEmptyString[];

void EEComplexUnit::PayInsurance(float inFraction)
{
    EEPlayer* thePlayer = this->mPlayer;
    EETechTreeNode* theNode = thePlayer->mTechnologies->Get(this->GetUnitClass()->mTechnology);
    if (theNode == NULL)
        return;
    long theOldTotals[5];
    long theOldResources[5];
    memcpy(theOldTotals, thePlayer->mInsuranceRefunds, sizeof(theOldTotals));
    memcpy(theOldResources, &thePlayer->mResourceCaps[kResourceFood], sizeof(theOldResources));
    this->mPlayer->RefundTechnology(theNode, this->GetUnitClass()->GetUnitType(), inFraction);
    for (long i = 0; i < 5; i++)
        thePlayer->mInsuranceRefunds[i] += thePlayer->mResourceCaps[i + 2] - theOldResources[i];
    thePlayer = this->mPlayer;
    if (thePlayer != ::gWorld.GetCurrentPlayer())
        return;
    gEffectManager->CreateUnitEffect(this, EEGraphicEffect::Get(kGraphicEffectDisbandUnitEmitter), NULL, true, 0, 0);
    for (unsigned long theResource = 2; theResource < 7; theResource++) {
        long theValue = thePlayer->mInsuranceRefunds[theResource - 2];
        if (theValue == theOldTotals[theResource - 2])
            continue;
        bool theNotify = false;
        long theShownValue = theValue;
        if (theValue >= 20) {
            for (long i = 0; i < 11; i++) {
                long theThreshold = kResourceRefundThresholds[i];
                if (theOldTotals[theResource - 2] < theThreshold && theThreshold <= theValue) {
                    theNotify = true;
                    theShownValue = theThreshold;
                }
            }
            if (!theNotify)
                continue;
        }
        char theResourceNumber[64];
        char theAmount[64];
        sprintf(theResourceNumber, "%ld", theResource);
        sprintf(theAmount, "%ld", theShownValue);
        UWideString theText;
        UWideString thePart;
        gResource->GetString(IDS_YOU_HAVE_RECEIVED, theText);
        theText += theAmount;
        theText += " ";
        gResource->GetString(theResource + IDS_RESOURCE_NAME_BASE, thePart);
        theText += thePart;
        gResource->GetString(IDS_IN_INSURANCE, thePart);
        theText += thePart;
        gUserInterface->AddChatLogEntry(thePlayer->mColor, UWideString(gEmptyString), theText, 5000, 2);
        EESoundObject* theSound = EESoundObject::Get(kSoundInsuranceCollected);
        if (theSound != NULL)
            theSound->Play(theSound->GetVolume(), 0.0f, 1.0f);
    }
}


// FUNCTION: EXE 0x5f6817
// Records as mutual contacts the living collidable units under the unit's footprint in the same layer
void EEComplexUnit::FindContacts()
{
    EEMap* theMap = ::gWorld.mMap;
    EEUnitClass* theClass = this->GetUnitClass();
    float theSize = theClass->GetInnerCollisionSphere();
    float theRadius = theSize;
    EEComplexUnit* theThis = this;
    bool theFlag = theClass->FliesOver();
    bool theAir = theClass->IsAirCategory();
    float theX = this->mPhysicalModel.GetLocation().GetX();
    float theY = this->mPhysicalModel.GetLocation().GetY();
    if (theSize >= kMinimumFootprint)
        theRadius = theSize + theSize;
    theRadius = theRadius * 0.5;
    if (theRadius < 0.5)
        theRadius = 0.5f;
    long theMinX = UMFloor(theX - theRadius);
    long theMaxX = UMFloor(theX + theRadius);
    long theMinY = UMFloor(theY - theRadius);
    long theMaxY = UMFloor(theY + theRadius);
    for (long x = theMinX; x <= theMaxX; x++) {
        for (long y = theMinY; y <= theMaxY; y++) {
            EEMapTile* theTile = theMap->GetTile(x, y);
            if (theTile == NULL)
                continue;
            for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                EEComplexUnit* theUnit = theNode->mUnit->GetComplexUnit();
                if (theUnit == NULL || theUnit == theThis || theUnit->mIsCollidable == 0 || theUnit->mHitPoints <= 0
                    || theUnit->IsFoundation())
                    continue;
                EEUnitClass* theOther = theUnit->mUnitClass;
                if (theFlag != theOther->FliesOver())
                    continue;
                if (theAir != theOther->IsAirCategory())
                    continue;
                if ((theClass->mDomains ^ theOther->mDomains) & 0xc)
                    continue;
                if ((theClass->mDomains & kMovementSpace) && (theClass->mCategory == kUnitCategoryPlanetSpaceship || theOther->mCategory == kUnitCategoryPlanetSpaceship))
                    continue;
                theThis->AddContact(theUnit);
                theUnit->AddContact(theThis);
            }
        }
    }
}

// FUNCTION: EXE 0x5f6a1a
// Returns whether the unit accepts the resource type (any, or the one it carries)
bool EEComplexUnit::CanTake(EEComplexUnit* inCargo, unsigned long inValue)
{
    return this->mResourceType == kResourceAny || this->mResourceType == inValue ? true : false;
}

// FUNCTION: EXE 0x5f6a31
// Collects the other unit's calamities this unit could catch; returns only the last calamity's test
bool EEComplexUnit::GetCatchableContagions(EEComplexUnit* inUnit, unsigned long& outFlags)
{
    std::vector<unsigned long> theFlags;
    bool theResult = false;
    outFlags = 0;
    if (inUnit->mCalamityFlags != 0) {
        EESplitCalamityFlags(inUnit->mCalamityFlags, theFlags);
        for (long i = theFlags.size() - 1; i >= 0; i--) {
            if (this->HasCalamityFlag(theFlags[i]))
                continue;
            theResult = this->GetUnitClass()->IsAffectedByCalamity(theFlags[i]);
            if (theResult)
                outFlags |= theFlags[i];
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5f6acb
// Removes the unit from play: drops its path, goals, queues, sound and every unit reference it holds
void EEComplexUnit::Invalidate()
{
    if (this->mCurrentResearch != kTechNone)
        this->PlayEffect(100000);
    this->DropPath();
    gSoundManager->Stop(this->mSound, false);
    if (this->mSound)
        this->mSound->Dereference();
    this->mSound = NULL;
    for (unsigned long i = 0; i < this->mGoalQueues.size(); i++)
        this->ClearGoalQueue(i);
    for (unsigned long j = 0; j < this->mCitizenBuildQueue.size(); j++)
        this->mCitizenBuildQueue[j]->Dereference();
    this->mCitizenBuildQueue.clear();
    if (this->mOwner)
        this->mOwner->Dereference();
    this->mOwner = NULL;
    if (this->mLastAttacker)
        this->mLastAttacker->Dereference();
    this->mLastAttacker = NULL;
    if (this->mPatrolGroup) {
        this->mPatrolGroup->RemoveUnit(this);
        if (this->mPatrolGroup) {
            this->mPatrolGroup->UReferenceCounter::Dereference();
            this->mPatrolGroup = NULL;
        }
    }
    if (this->mPreviousPart) {
        this->mPreviousPart->Dereference();
        this->mPreviousPart = NULL;
    }
    if (this->mNextPart) {
        this->mNextPart->Dereference();
        this->mNextPart = NULL;
    }
    if (this->mHomeBase) {
        this->mHomeBase->Dereference();
        this->mHomeBase = NULL;
    }
    if (this->mContainer) {
        this->mContainer->Dereference();
        this->mContainer = NULL;
    }
    if (this->mMine) {
        this->mMine->Dereference();
        this->mMine = NULL;
    }
    if (this->mOwner)
        this->mOwner->Dereference();
    this->mOwner = NULL;
    if (this->mLastAttacker)
        this->mLastAttacker->Dereference();
    this->mLastAttacker = NULL;
    this->ClearContacts();
    this->mActiveGoal = NULL;
    this->mGoalState = NULL;
    if (this->mHolder) {
        this->mHolder->ReleaseHeld(this);
        this->ClearHolder();
    } else
        this->ReleaseHeldUnits();
    this->mIgnoredTargets.clear();
    if (this->mSound) {
        gSoundManager->Stop(this->mSound, false);
        this->ReleaseSound();
    }
    EEUnit::Remove(this->mIsFoundation);
    if (!this->GetUnitClass()->IsLandBuilding() || this->mIsBuilt == true || ((gWorldClearing == true || UShutdownInProgress() == true) && this->mIsFoundation))
        this->mPlayer->UnloadUnitClassButtons(this->GetUnitClass()->GetUnitType());
    this->mIsFoundation = false;
}

// FUNCTION: EXE 0x5f6d1a
// Releases the unit's sound instance
void EEComplexUnit::ReleaseSound()
{
    if (this->mSound)
        this->mSound->Dereference();
    this->mSound = NULL;
}

// FUNCTION: EXE 0x5f6d44
// Clears every goal queue of the unit
void EEComplexUnit::ClearAllGoalQueues()
{
    for (unsigned long i = 0; i < this->GetNumGoalQueues(); i++)
        this->ClearGoalQueue(i);
}

// FUNCTION: EXE 0x5f6d6a
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

// FUNCTION: EXE 0x5f6de2
// Takes the unit out of every area effect acting on it and empties that list
void EEComplexUnit::RemoveAffectingAreaEffects()
{
    for (std::vector<EEAreaEffect*>::const_iterator theIterator = this->mAffectingAreaEffects.mItems.begin(); theIterator != this->mAffectingAreaEffects.mItems.end(); ++theIterator)
        (*theIterator)->RemovedFrom(this);
    EEUnitAreaEffectList& theList = this->mAffectingAreaEffects;
    theList.Clear();
    theList.mItems.clear();
}

// FUNCTION: EXE 0x5f6e19
// Refunds the current research and every queued item, re-allowing techs and updating the hero count
void EEComplexUnit::UpdateJITManufacturing()
{
    bool theChanged = false;
    EETechTreeNode* theNode = this->GetPlayer()->GetTechTree()->Get(this->mCurrentResearch);
    if (theNode != NULL) {
        if (theNode->mData->mUnitClass <= 0) {
            theNode->mIsAllowed = true;
            theChanged = true;
        } else if (gUnitClassTable[theNode->mData->mUnitClass] != NULL && gUnitClassTable[theNode->mData->mUnitClass]->mAIKind == kAIKindHero
                   && this->GetPlayer()->mNumHeroesInProduction > 0)
            this->GetPlayer()->mNumHeroesInProduction--;
    }
    while (this->mBuildQueue.size() != 0) {
        theNode = this->GetPlayer()->GetTechTree()->Get(this->mBuildQueue.back().mTechnology);
        if (theNode != NULL) {
            if (theNode->mData->mUnitClass <= 0) {
                theNode->mIsAllowed = true;
                theChanged = true;
            } else if (gUnitClassTable[theNode->mData->mUnitClass] != NULL && gUnitClassTable[theNode->mData->mUnitClass]->mAIKind == kAIKindHero
                       && this->GetPlayer()->mNumHeroesInProduction > 0)
                this->GetPlayer()->mNumHeroesInProduction--;
        }
        this->GetPlayer()->RefundTechnology(theNode, this->GetUnitClass()->GetUnitType(), 1.0f);
        this->mBuildQueue.pop_back();
    }
    if (theChanged && !UShutdownInProgress())
        gUserInterface->UpdateJITManufacturing();
}

// FUNCTION: EXE 0x5f6f2a
// Empties the citizen's build queue, releasing each building
void EEComplexUnit::ClearBuildTargets()
{
    while (this->mCitizenBuildQueue.size() != 0) {
        this->mCitizenBuildQueue.front()->Dereference();
        this->mCitizenBuildQueue.pop_front();
    }
    this->mHasShiftQueuedBuild = false;
}

// FUNCTION: EXE 0x5f6f65
// Empties the contact list, removing this unit from each contact's list too
void EEComplexUnit::ClearContacts()
{
    for (UListNode<EEComplexUnit*>* theNode = this->mContacts.mHead; theNode; theNode = theNode->mNext) {
        theNode->mData->RemoveContactUnit(this);
        theNode->mData->Dereference();
    }
    this->mContacts.Clear();
}

// FUNCTION: EXE 0x5f6f99
// Destroys the unit's conversion graphic effect, if any
void EEComplexUnit::ReleaseConversionEffect()
{
    EEEffectHandle theHandle = this->mConversionEffect;
    if (!UIsNone32(theHandle.mValue)) {
        gEffectManager->DestroyEffect(theHandle);
        this->mConversionEffect.mValue = -1;
    }
}

// FUNCTION: EXE 0x5f6fbc
// Clears one goal queue (abandoning the main queue's goals first) and frees the unit's airport slot
void EEComplexUnit::ClearGoalQueue(unsigned long inIndex)
{
    if (inIndex == 0) {
        for (unsigned long i = 0; i < this->mGoalQueues[0].mGoals.size(); i++)
            this->mGoalQueues[0].GetGoalAt(i)->Abandon(this);
    }
    this->mGoalQueues[inIndex].Clear();
    this->SetGatheredResource(NULL);
    if (this->GetUnitClass() != NULL) {
        if (this->GetUnitClass()->IsAirBase() && this->GetUnitClass()->mAirTrafficType != 0)
            this->mPlayer->mAirTraffic[this->GetUnitClass()->mAirTrafficType - 1]->ClearAirportQueue(this, inIndex);
        this->RefreshGoals();
        this->RefreshActiveGoal();
    }
}

// FUNCTION: EXE 0x5f709c
// Destroys the unit's heal graphic effect, if any
void EEComplexUnit::ReleaseHealEffect()
{
    EEEffectHandle theHandle = this->mHealEffect;
    if (!UIsNone32(theHandle.mValue)) {
        gEffectManager->DestroyEffect(theHandle);
        this->mHealEffect.mValue = -1;
    }
}

// FUNCTION: EXE 0x5f70bf
// Destroys the fire effect of a citizen, gate or wall, or removes a cloak effect from a CP market
void EEComplexUnit::ReleaseFireEffect()
{
    EEUnitClass* theClass = this->GetUnitClass();
    long theCategory = theClass->mCategory;
    if (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryGate || theClass->mIsWall) {
        EEEffectHandle theHandle = this->mFireEffect;
        if (theHandle.mValue > 0) {
            gEffectManager->DestroyEffect(theHandle);
            this->mFireEffect.mValue = 0;
        }
    } else if (theClass->mUnitType == kUnitTypeMarketCP)
        EERemoveCloakEffect(this);
}

// FUNCTION: EXE 0x5f7112
// Drops the unit's waypoints (or steps through them all while waypoint marking is on)
void EEComplexUnit::DropPath()
{
    if (!sB563234) {
        this->mWaypoints.clear();
        this->mCollisionResolutionCount = 0.0f;
        return;
    }
    while (this->NextWaypoint() == 0)
        ;
}

// FUNCTION: EXE 0x5f713d
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
    this->mVelocity = theOther->mVelocity;
    this->mF220 = theOther->mF220;
    this->mF21C = theOther->mF21C;
    this->mOpacity = theOther->mOpacity;
    this->mFacing = theOther->mFacing;
    this->mCrewHitPoints = theOther->mCrewHitPoints;
    this->mRottingAwayTime = theOther->mRottingAwayTime;
    this->mTargetSearchTimer = theOther->mTargetSearchTimer;
    this->mWorkTimer = theOther->mWorkTimer;
    this->mRegenerationTimer = theOther->mRegenerationTimer;
    this->mHealSearchTimer = theOther->mHealSearchTimer;
    this->mWakeTimer = theOther->mWakeTimer;
    this->mTrailEffectTimer = theOther->mTrailEffectTimer;
    this->mGateDirection = theOther->mGateDirection;
    this->mOpenFraction = theOther->mOpenFraction;
    this->mExperience = theOther->mExperience;
    this->mNumAttackers = theOther->mNumAttackers;
    if (this->mSound != NULL)
        gSoundManager->Stop(this->mSound, false);
    this->SetSound(theOther->mSound);
    float theFraction = (float)theOther->mLastHitPoints /
        theOther->GetPlayer()->GetUpgrade(theOther->GetUnitClass()->GetUnitType())->mMaxHitPoints;
    this->mLastHitPoints = UMFloatToLong(this->GetPlayer()->GetUpgrade(this->GetUnitClass()->GetUnitType())->mMaxHitPoints * theFraction);
    float theMaximum = (float)theOther->GetPlayer()->GetUpgrade(theOther->GetUnitClass()->GetUnitType())->mPower;
    if (theMaximum > 0.0f)
        theFraction = (float)theOther->mPower / theMaximum;
    else
        theFraction = 0.0f;
    this->mPower = UMFloatToLong(this->GetPlayer()->GetUpgrade(this->GetUnitClass()->GetUnitType())->mPower * theFraction);
    this->mPowerRegenTimer = theOther->mPowerRegenTimer;
    theFraction = (float)theOther->mFlightTime /
        theOther->GetPlayer()->GetUpgrade(theOther->GetUnitClass()->GetUnitType())->mFlightTime;
    this->mFlightTime = UMFloatToLong(this->GetPlayer()->GetUpgrade(this->GetUnitClass()->GetUnitType())->mFlightTime * theFraction);
    theFraction = (float)theOther->mReloadTimer /
        theOther->GetPlayer()->GetUpgrade(theOther->GetUnitClass()->GetUnitType())->mReloadTime;
    this->mReloadTimer = UMFloatToLong(this->GetPlayer()->GetUpgrade(this->GetUnitClass()->GetUnitType())->mReloadTime * theFraction);
    this->mAmmunition = theOther->mAmmunition;
    this->mWeaponTagPoint = 0;
    this->mIsCommandable = theOther->mIsCommandable;
    this->mIsClearanceGranted = theOther->mIsClearanceGranted;
    this->mIsBuilt = theOther->mIsBuilt;
    this->mIsCloaked = theOther->mIsCloaked;
    this->mConversionStartTime = theOther->mConversionStartTime;
    this->mCloaked = theOther->mCloaked;
    this->mLaunchYaw = theOther->mLaunchYaw;
    this->mColor = theOther->mColor;
    this->mIsTinted = theOther->mIsTinted;
    this->mIsLocked = theOther->mIsLocked;
    this->mIsPacked = theOther->mIsPacked;
    this->mIsSpecialAttack = theOther->mIsSpecialAttack;
    if (gCivPowersManager.HasPower(this->GetPlayer(), kCivPowerCamouflage) &&
        gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet) != NULL &&
        gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet)->Contains(theOther->GetUnitClass()->GetUnitType()) &&
        gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet)->Contains(this->GetUnitClass()->GetUnitType()))
        this->mIsCloakable = theOther->mIsCloakable;
    this->mCamouflageTimer = theOther->mCamouflageTimer;
    this->mU32C = theOther->mU32C;
    this->UpdateCamouflage(this->mCommand != NULL ? this->mCommand->mType : 0);
    this->mIsFiringHigh = false;
    this->mNumPathRequests = theOther->mNumPathRequests;
    this->mLastMapPosition = theOther->mLastMapPosition;
    this->mU0DC = theOther->mU0DC;
    this->mIsDisabled = theOther->mIsDisabled;
    this->mIsDeletable = theOther->mIsDeletable;
    this->mIsPaidFor = theOther->mIsPaidFor;
    this->mCommand = theOther->mCommand;
    this->mCurrentPhysicsAction = theOther->mCurrentPhysicsAction;
    this->mCurrentPhysicsOrder = theOther->mCurrentPhysicsOrder;
    this->mGrowth = theOther->GetGrowth();
    this->mB34F = theOther->GetB34F();   // the call (was the member function's address: always true)
    this->mIsRegrowing = theOther->mIsRegrowing;
    this->mCalamityFlags = theOther->mCalamityFlags;
    this->mNumAreaEffects = 0;
    this->mU2FC = 0;
    EEMovePassengers(this, theOther, theOther->GetPlayer() != this->GetPlayer());
    EETakeOverLoad(this, theOther);
    for (unsigned long i = 0; i < theOther->mGoalQueues.size() && i < this->mGoalQueues.size(); ++i) {
        this->mGoalQueues[i].Clear();
        for (unsigned long j = 0; j < theOther->mGoalQueues[i].mGoals.size(); ++j) {
            EEUnitGoal* theGoal = theOther->mGoalQueues[i].mGoals[j];
            if (theGoal->mKind == kGoalUnloadTransport && theOther->GetPlayer() == this->GetPlayer())
                EEGoalCast<EEUGUnloadTransport>(theGoal)->SetTransport(this);
            theGoal = theGoal->Clone();
            this->mGoalQueues[i].mGoals.push_back(theGoal);
            theGoal->SetOwner(this->mGoalQueues[i].mUnit);
        }
    }
    this->mWaypoints = theOther->mWaypoints;
    this->mPlanetID = theOther->mPlanetID;
    this->mBuildQueue = theOther->mBuildQueue;
    this->PlayEffect(theOther->mCurrentResearch);
    this->mProductionTime = theOther->mProductionTime;
    // the original assigns each of inUnit's containers to itself (0x1f7a9b/0x1f7aad: ecx and the argument are both
    // theOther + 0xe4 / + 0xf8), which clears them
    theOther->mAreaEffects = theOther->mAreaEffects;
    theOther->mAffectingAreaEffects = theOther->mAffectingAreaEffects;
    this->mAIState = theOther->mAIState;
    this->mL334 = theOther->mL334;
    this->mL338 = theOther->mL338;
    this->mNumBuilders = theOther->mNumBuilders;
    this->mIsDetachedFromAI = theOther->mIsDetachedFromAI;
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

// FUNCTION: EXE 0x5f7b45
// Returns the unit's mB34F flag
bool EEComplexUnit::GetB34F()
{
    return this->mB34F;
}

// FUNCTION: EXE 0x5f7b4c
// Replaces the unit's sound instance, referencing the new one
void EEComplexUnit::SetSound(SESMInstanceNode* inSound)
{
    this->ReleaseSound();
    this->mSound = inSound;
    if (inSound)
        inSound->Reference();
}

// FUNCTION: EXE 0x5f7b69
// Returns whether the unit carries the given calamity
bool EEComplexUnit::HasCalamityFlag(unsigned long inFlag)
{
    return (this->mCalamityFlags & inFlag) != 0;
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

// FUNCTION: EXE 0x5f7b7c
// Takes a unit from the pool and resets it to defaults
EEComplexUnit* EEComplexUnit::Create()
{
    EEComplexUnit* theUnit = EENewPoolUnit();
    theUnit->Reset();
    theUnit->ResetEEUnit();
    theUnit->ResetEEComplexUnit(true, false);
    return theUnit;
}

// FUNCTION: EXE 0x5f7ba6
// Sizes the unit pool and the unit-list node pool (twice the chunk size); returns the unit pool's result
bool EEComplexUnit::SetPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks)
{
    UList<EEComplexUnit*>::sNodePool.SetSize(inChunkSize * 2, inMaxChunks, 3);
    return EEComplexUnit::sMemoryPool.SetSize(inChunkSize, inMaxChunks, 1);
}

// FUNCTION: EXE 0x5f7bd2
// Takes a unit from the pool and initialises it for a class, placement, player and goal queue count
EEComplexUnit* EEComplexUnit::Create(EEUnitClass* inClass, const GETransformation& inTransformation,
                                     EEPlayer* inPlayer, unsigned long inGoalQueueCount)
{
    EEComplexUnit* theUnit = EENewPoolUnit();
    theUnit->Init(inClass, inTransformation, inPlayer, inGoalQueueCount);
    return theUnit;
}

// FUNCTION: EXE 0x5f7bfa
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

// FUNCTION: EXE 0x5f7c47
// Sets the water state of a sea or space unit from its class's water kind (and the tile's depth level)
void EEComplexUnit::UpdateWaterState(bool inArg)
{
    if (inArg) {
        if (this->GetUnitClass()->mDomains & kMovementSeaOrSpace) {
            RTSTile* theTile = this->mTile;
            if (theTile != NULL) {
                long theType = theTile->mTerrainType;
                if (theType == kTerrainDeepWater || theType == kTerrainShallowWater || theType == kTerrainSpace) {
                    // NEAR: the original compares the byte signed (`jl`): the tile's +0x14 is a signed char there
                    char theLevel = theTile->mAIValue;
                    long theMode = this->GetUnitClass()->mWaterKind;
                    if (theLevel >= 0 && theMode != 0) {
                        if ((theLevel > 0 && theMode == 2) || theMode == 1) {
                            this->mWaterState = 2;
                            return;
                        }
                        if (theMode == 2) {
                            this->mWaterState = 3;
                            return;
                        }
                    }
                    this->mWaterState = 0;
                }
            }
        }
    } else if (this->GetUnitClass()->mDomains & kMovementSeaOrSpace) {
        if (this->GetUnitClass()->mWaterKind == 1)
            this->mWaterState = 2;
        else if (this->GetUnitClass()->mWaterKind == 2)
            this->mWaterState = 3;
        else
            this->mWaterState = 0;
    } else
        this->mWaterState = 0;
}

// FUNCTION: EXE 0x5f7ce7
// Adds the unit's sight at a tile for its player, sight sharers and allies, revealing it on the minimap
void EEComplexUnit::SightOn(unsigned long inX, unsigned long inY)
{
    EEPlayer* thePlayer = this->mPlayer;
    GETerrainMesh* theMesh = &::gWorld.mMap->mTerrainMesh;
    if (!(this->GetLineOfSight() > 0.0f) || thePlayer == NULL || !thePlayer->mSaveVisMap || inX == (unsigned long)-10000 || !gSightUpdatesEnabled)
        return;
    float theX = (float)(long)inX;
    float theY = (float)(long)inY;
    this->AddCenterOffset(theX, theY);
    if (this->GetUnitClass()->IsLandBuilding() && this->IsFoundation())
        return;
    if (this->mHitPoints > 0 && this->mIsBuilt) {
        EEAddSightCircle(theX, theY, this->GetLineOfSight(), thePlayer);
        if (this->GetUnitClass()->IsLandBuilding() == true) {
            for (unsigned long i = 0; i < thePlayer->mSightSharers.size(); i++) {
                bool theVisible = ::gWorld.GetCurrentPlayer() == thePlayer->mSightSharers[i] &&
                    ::gWorld.GetCurrentPlayer()->mSharedLOS && (thePlayer->mSightSharers[i])->mSharedLOS;
                if (this->GetUnitClass()->mTileSize <= 1) {
                    EEMapTile* theTile = ::gWorld.mMap->GetTile(inX, inY);
                    if (theTile != NULL) {
                        theTile->AddSight((thePlayer->mSightSharers[i]));
                        if (theVisible) {
                            gMiniMap->CountCellOn(inX, inY);
                            theMesh->ReferenceTile(inX, inY);
                        }
                    }
                } else {
                    for (long x = inX; x < (long)(inX + this->GetUnitClass()->mTileSize); x++) {
                        for (long y = inY; y < (long)(inY + this->GetUnitClass()->mTileSize); y++) {
                            EEMapTile* theTile = ::gWorld.mMap->GetTile(x, y);
                            if (theTile == NULL)
                                continue;
                            theTile->AddSight((thePlayer->mSightSharers[i]));
                            if (theVisible) {
                                gMiniMap->CountCellOn(x, y);
                                theMesh->ReferenceTile(x, y);
                            }
                        }
                    }
                }
            }
            if (this->GetUnitClass()->mWonderBonus == 3)
                ::gWorld.mMap->AddSight(thePlayer, theX, theY, gWorldTable.GetFloatValue(kWorldWonderSightRadius), false);
        }
    } else {
        bool theVisible = ::gWorld.GetCurrentPlayer() == thePlayer ||
            (::gWorld.GetCurrentPlayer()->IsAlliedWith(thePlayer) && ::gWorld.GetCurrentPlayer()->mSharedLOS && thePlayer->mSharedLOS);
        if (this->GetUnitClass()->mTileSize <= 1) {
            EEMapTile* theTile = ::gWorld.mMap->GetTile(inX, inY);
            if (theTile != NULL) {
                thePlayer->AddSharedSight(theTile, inX, inY);
                if (theVisible) {
                    gMiniMap->CountCellOn(inX, inY);
                    theMesh->ReferenceTile(inX, inY);
                }
            }
        } else {
            for (long x = inX; x < (long)(inX + this->GetUnitClass()->mTileSize); x++) {
                for (long y = inY; y < (long)(inY + this->GetUnitClass()->mTileSize); y++) {
                    EEMapTile* theTile = ::gWorld.mMap->GetTile(x, y);
                    if (theTile == NULL)
                        continue;
                    thePlayer->AddSharedSight(theTile, x, y);
                    if (theVisible) {
                        gMiniMap->CountCellOn(x, y);
                        theMesh->ReferenceTile(x, y);
                    }
                }
            }
        }
    }
    if (this->GetTechTreeNode() == NULL || this->GetTechTreeNode()->mData->mKind != 4)
        return;
    for (unsigned long i = 1; i < ::gWorld.mNumPlayers; i++) {
        EEPlayer* theOther = ::gWorld.mPlayers[i];
        if (theOther == NULL)
            continue;
        bool theVisible = theOther == ::gWorld.GetCurrentPlayer();
        for (long x = inX; x < (long)(inX + this->GetUnitClass()->mTileSize); x++) {
            for (long y = inY; y < (long)(inY + this->GetUnitClass()->mTileSize); y++) {
                EEMapTile* theTile = ::gWorld.mMap->GetTile(x, y);
                if (theTile == NULL)
                    continue;
                theOther->AddSharedSight(theTile, x, y);
                if (theVisible) {
                    gMiniMap->CountCellOn(x, y);
                    theMesh->ReferenceTile(x, y);
                }
            }
        }
    }
}


// FUNCTION: EXE 0x5f8357
// Updates whether the unit hides others or is hidden behind a building; returns whether it is hidden
bool EEComplexUnit::UpdateHidden()
{
    if (this->mCanBeHidden && (!(this->mIsCloakable && this->mIsCloaked) || ::gWorld.GetCurrentPlayer() == this->mPlayer ||
        this->mPlayer->mEnemies[::gWorld.GetCurrentPlayer()->GetPlayerID()] == 0) && this->GetTileX() != -10000) {
        if (!::gWorld.mInCinematic) {
            if ((this->mHiddenFlags & kHiddenFlagMoved) == kHiddenFlagMoved) {
                EEMap* theMap = ::gWorld.mMap;
                EETile* theTile = theMap->GetTile(this->GetTileX(), this->GetTileY());
                unsigned char thePhase = ((gPhysics->mGameTime + 2) / 1000) % 3;
                {
                    for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                        EEUnit* theUnit = theNode->mUnit;
                        if (theUnit == this || !theUnit->GetUnitClass()->mIsObstacle)
                            continue;
                        theUnit->mHiddenFlags |= kHiddenFlagHidden;
                        theUnit->mHiddenFlags ^= (theUnit->mHiddenFlags ^ thePhase) & kHiddenFlagPhaseMask;
                        EEComplexUnit* theComplex = theUnit->GetComplexUnit();
                        if (theComplex == NULL)
                            continue;
                        while ((theComplex = theComplex->mNextPart) != 0) {
                            theComplex->mHiddenFlags |= kHiddenFlagHidden;
                            theComplex->mHiddenFlags ^= (theComplex->mHiddenFlags ^ thePhase) & kHiddenFlagPhaseMask;
                        }
                    }
                }
                theTile = theMap->GetTile(this->GetTileX(), this->GetTileY() - 1);
                if (theTile != NULL) {
                    for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                        EEUnit* theUnit = theNode->mUnit;
                        if (theUnit == this || !theUnit->GetUnitClass()->mIsObstacle)
                            continue;
                        theUnit->mHiddenFlags |= kHiddenFlagHidden;
                        theUnit->mHiddenFlags ^= (theUnit->mHiddenFlags ^ thePhase) & kHiddenFlagPhaseMask;
                        EEComplexUnit* theComplex = theUnit->GetComplexUnit();
                        if (theComplex == NULL)
                            continue;
                        while ((theComplex = theComplex->mNextPart) != 0) {
                            theComplex->mHiddenFlags |= kHiddenFlagHidden;
                            theComplex->mHiddenFlags ^= (theComplex->mHiddenFlags ^ thePhase) & kHiddenFlagPhaseMask;
                        }
                    }
                }
                theTile = theMap->GetTile(this->GetTileX() + 1, this->GetTileY() - 1);
                if (theTile != NULL) {
                    for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                        EEUnit* theUnit = theNode->mUnit;
                        if (theUnit == this || !theUnit->GetUnitClass()->mIsObstacle)
                            continue;
                        theUnit->mHiddenFlags |= kHiddenFlagHidden;
                        theUnit->mHiddenFlags ^= (theUnit->mHiddenFlags ^ thePhase) & kHiddenFlagPhaseMask;
                        EEComplexUnit* theComplex = theUnit->GetComplexUnit();
                        if (theComplex == NULL)
                            continue;
                        while ((theComplex = theComplex->mNextPart) != 0) {
                            theComplex->mHiddenFlags |= kHiddenFlagHidden;
                            theComplex->mHiddenFlags ^= (theComplex->mHiddenFlags ^ thePhase) & kHiddenFlagPhaseMask;
                        }
                    }
                }
                theTile = theMap->GetTile(this->GetTileX() + 1, this->GetTileY());
                if (theTile != NULL) {
                    for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                        EEUnit* theUnit = theNode->mUnit;
                        if (theUnit == this || !theUnit->GetUnitClass()->mIsObstacle)
                            continue;
                        theUnit->mHiddenFlags |= kHiddenFlagHidden;
                        theUnit->mHiddenFlags ^= (theUnit->mHiddenFlags ^ thePhase) & kHiddenFlagPhaseMask;
                        EEComplexUnit* theComplex = theUnit->GetComplexUnit();
                        if (theComplex == NULL)
                            continue;
                        while ((theComplex = theComplex->mNextPart) != 0) {
                            theComplex->mHiddenFlags |= kHiddenFlagHidden;
                            theComplex->mHiddenFlags ^= (theComplex->mHiddenFlags ^ thePhase) & kHiddenFlagPhaseMask;
                        }
                    }
                }
                this->mHiddenFlags &= ~kHiddenFlagMoved;
            } else
                this->mHiddenFlags &= ~kHiddenFlagHidden;
        }
    } else if ((this->mHiddenFlags & kHiddenFlagHidden) == kHiddenFlagHidden &&
        (signed char)(this->mHiddenFlags & kHiddenFlagPhaseMask) <= (signed char)((gPhysics->mGameTime / 1000) % 3)) {
        if (!::gWorld.mInCinematic) {
            this->mHiddenFlags &= ~(kHiddenFlagHidden | kHiddenFlagPhaseMask);
            EEMap* theMap = ::gWorld.mMap;
            unsigned char thePhase = ((gPhysics->mGameTime + 2) / 1000) % 3;
            std::vector<EETileCoordinate> theTiles;
            long theSize = this->GetUnitClass()->GetTileSize();
            if (theSize > 1) {
                theTiles.reserve(theSize * theSize + 2);
                EETileCoordinate theTile;
                theTile.mX = this->GetTileX();
                theTile.mY = this->GetTileY();
                theTiles.push_back(theTile);
                for (long y = this->GetTileY() + theSize; y >= this->GetTileY(); --y) {
                    theTile.mX = this->GetTileX() - 1;
                    theTile.mY = y;
                    theTiles.push_back(theTile);
                }
                for (long x = this->GetTileX() - 1 + theSize; x >= this->GetTileX(); --x) {
                    theTile.mY = this->GetTileY() + theSize;
                    theTile.mX = x;
                    theTiles.push_back(theTile);
                }
            } else {
                theTiles.resize(4);
                theTiles[0].mX = this->mTileX;
                theTiles[0].mY = this->GetTileY();
                theTiles[1].mX = this->GetTileX();
                (*(theTiles.begin() + 1)).mY = this->GetTileY() + 1;
                (*(theTiles.begin() + 2)).mY = this->GetTileY() + 1;
                (*(theTiles.begin() + 2)).mX = this->GetTileX() - 1;
                (*(theTiles.begin() + 3)).mX = this->GetTileX() - 1;
                (*(theTiles.begin() + 3)).mY = this->GetTileY();
            }
            for (long theIndex = theTiles.size() - 1; theIndex >= 0; --theIndex) {
                EETile* theTile = theMap->GetTileAtLocation(*(theTiles.begin() + theIndex));
                if (theTile == NULL)
                    continue;
                for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                    EEComplexUnit* theUnit = theNode->mUnit->GetComplexUnit();
                    bool theBlocks;
                    if (theUnit != NULL) {
                        bool theResource = theUnit->mResourceType == kResourceFood && theUnit->mResourceAmount > 0 && theUnit->GetComplexUnit() != NULL &&
                            theUnit->GetUnitClass()->GetCategory() != kUnitCategoryLandCitizen && theUnit->GetUnitClass()->GetCategory() != kUnitCategoryNavalCitizen &&
                            theUnit->GetPlayer()->GetPlayerID() == 0;
                        theBlocks = theUnit->mCanBeHidden && (theUnit->GetHitPoints() > 0 || theResource);
                    } else {
                        EEUnitClass* theClass = theNode->mUnit->GetUnitClass();
                        // a unit without a complex unit cannot be cloaked: only its class decides (the cloak and
                        // owner tests used to be read through the NULL complex unit)
                        theBlocks = !theClass->mIsObstacle && !theClass->mIsProjectile && theClass->mMaxVelocity > 0.0f;
                    }
                    if (!theBlocks)
                        continue;
                    this->mHiddenFlags |= kHiddenFlagHidden;
                    this->mHiddenFlags ^= (this->mHiddenFlags ^ thePhase) & kHiddenFlagPhaseMask;
                    for (EEComplexUnit* theChild = this->mNextPart; theChild != NULL; theChild = theChild->mNextPart) {
                        theChild->mHiddenFlags |= kHiddenFlagHidden;
                        theChild->mHiddenFlags ^= (theChild->mHiddenFlags ^ thePhase) & kHiddenFlagPhaseMask;
                    }
                    theIndex = -1;
                    break;
                }
            }
        } else
            this->mHiddenFlags &= ~kHiddenFlagHidden;
    }
    return (this->mHiddenFlags & kHiddenFlagHidden) == kHiddenFlagHidden && !this->IsFoundation();
}

// FUNCTION: EXE 0x5f88fe
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

// FUNCTION: EXE 0x5f8a26
// Returns the queue's first goal
EEUnitGoal* EEUnitVectorItem::GetFront()
{
    return this->mGoals.front();
}

// FUNCTION: EXE 0x5f8a43
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

// FUNCTION: EXE 0x5f8af6
// Kills every cargo unit and records each as a dead unit in its player's statistics
void EEComplexUnit::KillCargo()
{
    // NEAR: the original keeps this in a frame slot and &mCargo in ebx (a frame: push ebp/leave)
    while (this->mCargo.size() != 0) {
        EEComplexUnit* theCargo = this->mCargo.back();
        theCargo->Kill(0, 0, 0);
        if (theCargo->mPreviousPart != NULL || UShutdownInProgress())
            continue;
        long theKind = gPhysics->GetUnitSwapKind(this);
        if (theKind == 1)
            continue;
        if (theCargo->mUnitClass->GetUnitType() != kUnitTypePathPoint && theKind != 5 && theKind != 6 && theKind != 7
            && theKind != 8) {
            long theCategory = theCargo->mUnitClass->mCategory;
            if (theCategory == kUnitCategoryBuilding || theCategory == kUnitCategoryBuilding19 || theCategory == kUnitCategoryGate || theCategory == kUnitCategoryMiningBuilding
                || theCategory == kUnitCategoryFortress)
                theCargo->mPlayer->mSavedGroups.Reset(1, 1);
            else
                theCargo->mPlayer->mSavedGroups.Reset(4, 1);
        }
        long theSeries = EEAddDeadUnitStatistic(theCargo);
        if (theSeries != 0)
            theCargo->mPlayer->mSavedGroups.Lower(theSeries, 1);
    }
}

// FUNCTION: EXE 0x5f8bd8
// Empties the garrison count and population
void EEComplexUnit::ClearGarrison()
{
    this->mGarrisonPopulation = 0;
    this->mGarrisonCount = 0;
}

// FUNCTION: EXE 0x5f8be7
// Dumps the unit pool's statistics and releases the pool
void EEComplexUnit::ReleasePool()
{
    UWideString theReport(1000);
    UWideString theLine;
    EEComplexUnit::sMemoryPool.DumpStats(false);
    EEComplexUnit::sMemoryPool.Release();
}

// FUNCTION: EXE 0x5f8c2d
// Completes the unit's pending group path and, with at most two waypoints left, its own path
void EEComplexUnit::RefreshPath()
{
    if (this->mActiveGroup) {
        EEGroupPathRequest* theRequest = this->mActiveGroup->FindPathRequest(this);
        if (theRequest && theRequest->mPath && !theRequest->mPath->mIsDone)
            ::gPathManager.CompleteAutoPath(theRequest->mPath);
    }
    if (this->mWaypoints.size() <= 2)
        ::gPathManager.CompletePath(this);
}

// FUNCTION: EXE 0x5f8c73
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

// FUNCTION: EXE 0x5f8e7c
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

// FUNCTION: EXE 0x5f908d
// Writes the unit's goals, physics, class stats, waypoints and area effects to the out-of-sync dump
void EEComplexUnit::OutOfSynchDump(FILE* inFile, unsigned long inFlags)
{
    GE3DPoint theLocation;
    std::deque<EEUnitGoal*> theGoals;
    fprintf(inFile, "\n");
    fprintf(inFile, "OUT OF SYNCH DUMP:  **************  Unit ID: %08lx  *************\n", this->GetComplexUnit()->mUnitID);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Times Updated:      %ld\n", this->GetComplexUnit()->mTimesUpdated);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Owner:              %2ld\n", this->mPlayer->mPlayerID);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Object Type:        %4ld\n", this->GetUnitClass()->GetUnitType());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Tile (x):           %4ld\n", this->GetTileX());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Tile (y):           %4ld\n", this->GetTileY());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Hit Points:         %6ld\n", this->mHitPoints);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Action:             %s(%ld)\n", (const char*)EEUnitAction::GetActionName(this->GetComplexUnit()->GetCommandType()),
        this->GetComplexUnit()->GetCommandType());
    if (this->GetComplexUnit()->mActiveGroup == NULL)
        fprintf(inFile, "OUT OF SYNCH DUMP:  This unit has no active group\n");
    else
        fprintf(inFile, "OUT OF SYNCH DUMP:  This unit has an active group with an action of %ld\n", this->GetComplexUnit()->mActiveGroup->mAction);
    fprintf(inFile, "OUT OF SYNCH DUMP:  **************    GOALS        *************\n");
    for (unsigned long i = 0; i < this->GetComplexUnit()->mGoalQueues.size(); ++i) {
        theGoals = this->GetComplexUnit()->mGoalQueues[i].mGoals;
        fprintf(inFile, "OUT OF SYNCH DUMP:  ************** GOAL Queue(%6ld) Number Of Goals(%6zu) *************\n", i, theGoals.size());
        for (unsigned long j = 0; j < theGoals.size(); ++j) {
            fprintf(inFile, "OUT OF SYNCH DUMP:  Goal Number(%6ld) Has a type of:\t%6d\n", j, (theGoals[j])->mKind);
            (theGoals[j])->DumpOutOfSynch(inFile);
            (theGoals[j])->GetGoalLocation(theLocation);
            fprintf(inFile, "OUT OF SYNCH DUMP:  Goal Location (x): %6.6f\n", (double)theLocation.GetX());
            fprintf(inFile, "OUT OF SYNCH DUMP:  Goal Location (y): %6.6f\n", (double)theLocation.GetY());
            fprintf(inFile, "OUT OF SYNCH DUMP:  Goal Location (z): %6.6f\n", (double)theLocation.GetZ());
            if ((theGoals[j])->mKind == kGoalLocation) {
                if (static_cast<EEUGLocation*>(theGoals[j])->mIsAutoWaypoint == true)
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
    fprintf(inFile, "OUT OF SYNCH DUMP:  CurrentPhysicsAction: %2ld\n", this->GetComplexUnit()->mCurrentPhysicsAction);
    fprintf(inFile, "OUT OF SYNCH DUMP:  CurrentPhysicsOrder:  %2ld\n", this->GetComplexUnit()->mCurrentPhysicsOrder);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Queued for delete:  %2ld\n", (unsigned long)(this->mIsQueuedForDelete == true));
    fprintf(inFile, "OUT OF SYNCH DUMP:  Ammunition:         %6ld\n", this->GetComplexUnit()->mAmmunition);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Animation:          %6ld\n", this->GetComplexUnit()->mAnimation);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Yaw Velocity:       %6.6f\n", (double)this->GetComplexUnit()->mAngularVelocity.GetYaw());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Pitch Velocity:     %6.6f\n", (double)this->GetComplexUnit()->mAngularVelocity.GetPitch());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Roll Velocity:      %6.6f\n", (double)this->GetComplexUnit()->mAngularVelocity.GetRoll());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Blg Queue Count:    %6zu\n", this->GetComplexUnit()->mBuildQueue.size());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Cit Build Que Size: %6zu\n", this->GetComplexUnit()->mCitizenBuildQueue.size());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Clearance granted:  %2ld\n", (unsigned long)(this->GetComplexUnit()->mIsClearanceGranted == true));
    fprintf(inFile, "OUT OF SYNCH DUMP:  Coll Resolut Cnt:   %6.6f\n", (double)this->GetComplexUnit()->mCollisionResolutionCount);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Collision State:    %6ld\n", (unsigned long)this->GetComplexUnit()->mCollisionState);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Collision Tile (x): %6ld\n", this->GetComplexUnit()->mCollisionTile.mX);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Collision Tile (y): %6ld\n", this->GetComplexUnit()->mCollisionTile.GetY());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Crew Hit Points:    %6ld\n", this->GetComplexUnit()->mCrewHitPoints);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Current Frame:      %6ld\n", this->GetComplexUnit()->mPhysicalModel.GetFrame());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Flight Time:        %6ld\n", this->GetComplexUnit()->mFlightTime);
    fprintf(inFile, "OUT OF SYNCH DUMP:  ProjectileStart X:  %6.6f\n", (double)this->GetComplexUnit()->mProjectileStart.GetX());
    fprintf(inFile, "OUT OF SYNCH DUMP:  ProjectileStart Y:  %6.6f\n", (double)this->GetComplexUnit()->mProjectileStart.GetY());
    fprintf(inFile, "OUT OF SYNCH DUMP:  ProjectileStart Z:  %6.6f\n", (double)this->GetComplexUnit()->mProjectileStart.GetZ());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Rotting Away Time:  %6ld\n", this->GetComplexUnit()->mRottingAwayTime);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Velocity:           %6.6f\n", (double)this->GetComplexUnit()->mVelocity);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Affected by Grav:   %2ld\n", (unsigned long)this->GetUnitClass()->mIsAffectedByGravity);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Ammunition:         %6ld\n", this->GetUnitClass()->mAmmunition);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Ammo Type:          %6ld\n", this->GetUnitClass()->mAmmoType);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Base Accel:         %6.6f\n", (double)this->GetUnitClass()->mAcceleration);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Ang Accel:\t\t\t%6.6f\n", (double)this->GetStats()->mAngularAcceleration);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Base Area Effect:   %6.6f\n", (double)this->GetUnitClass()->mAreaEffect);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Base Attack         %6ld\n", this->GetUnitClass()->mAttack);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Base Build On Elev: %2ld\n", (unsigned long)this->GetUnitClass()->mBuildsOnElevation);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Base Convert Blg:   %2ld\n", (unsigned long)this->GetUnitClass()->mConvertsBuildings);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Base Conver Priest: %2ld\n", (unsigned long)this->GetUnitClass()->mConvertsPriests);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Base Carry Ammt:    %6ld\n", this->GetUnitClass()->mCarryAmount);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Base Flight Time:   %6ld\n", this->GetUnitClass()->mFlightTime);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Base Healing Pts:   %6ld\n", this->GetUnitClass()->mHealingPoints);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Base Hit Points:    %6ld\n", this->GetUnitClass()->mHitPoints);
    fprintf(inFile, "OUT OF SYNCH DUMP:  LOS:\t\t\t\t%6.6f\n", (double)this->GetStats()->mLineOfSight);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Max Ang Vel:\t\t%6.6f\n", (double)this->GetStats()->mMaxAngularVelocity);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Base Max Obj Carry: %6ld\n", this->GetUnitClass()->mMaxObjectCarry);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Max Vel:\t\t\t%6.6f\n", (double)this->GetStats()->mMaxVelocity);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Base Range:         %6.6f\n", (double)this->GetUnitClass()->mRange);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Base Reload Time:   %6ld\n", this->GetUnitClass()->mReloadTime);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Base Repair Rate:   %6ld\n", this->GetUnitClass()->mRepairRate);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Base Turret MaxAV:  %6.6f\n", (double)this->GetUnitClass()->mTurretMaxAngularVelocity);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Base Work Rate:     %6ld\n", this->GetUnitClass()->mWorkRate);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Fire While Moving:  %2ld\n", (unsigned long)this->GetUnitClass()->mFiresWhileMoving);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Combat Reaction:    %6ld\n", this->GetUnitClass()->mCombatReaction);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Cnvert %% per sec:   %6.6f\n", (double)this->GetStats()->mConversionRate);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Cruise Altitude:    %6.6f\n", (double)this->GetComplexUnit()->mHeight);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Inner Coll Sphere:  %6.6f\n", (double)this->GetUnitClass()->GetInnerCollisionSphere());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Min Range:          %6.6f\n", (double)this->GetUnitClass()->mMinRange);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Missle Delay Time:  %6ld\n", this->GetUnitClass()->mMissileDelayTime);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Outer Coll Sphere:  %6.6f\n", (double)this->GetUnitClass()->mOuterCollisionSphere);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Shoots High:        %2ld\n", (unsigned long)this->GetUnitClass()->mShootsHigh);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Model Location (x): %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetX());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Model Location (y): %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetY());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Model Location (z): %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetZ());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Trans (0,0):        %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetElement(0, 0));
    fprintf(inFile, "OUT OF SYNCH DUMP:  Trans (0,1):        %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetElement(0, 1));
    fprintf(inFile, "OUT OF SYNCH DUMP:  Trans (0,2):        %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetElement(0, 2));
    fprintf(inFile, "OUT OF SYNCH DUMP:  Trans (0,3):        %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetElement(0, 3));
    fprintf(inFile, "OUT OF SYNCH DUMP:  Trans (1,0):        %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetElement(1, 0));
    fprintf(inFile, "OUT OF SYNCH DUMP:  Trans (1,1):        %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetElement(1, 1));
    fprintf(inFile, "OUT OF SYNCH DUMP:  Trans (1,2):        %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetElement(1, 2));
    fprintf(inFile, "OUT OF SYNCH DUMP:  Trans (1,3):        %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetElement(1, 3));
    fprintf(inFile, "OUT OF SYNCH DUMP:  Trans (2,0):        %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetElement(2, 0));
    fprintf(inFile, "OUT OF SYNCH DUMP:  Trans (2,1):        %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetElement(2, 1));
    fprintf(inFile, "OUT OF SYNCH DUMP:  Trans (2,2):        %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetElement(2, 2));
    fprintf(inFile, "OUT OF SYNCH DUMP:  Trans (2,3):        %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetElement(2, 3));
    fprintf(inFile, "OUT OF SYNCH DUMP:  Trans (3,0):        %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetElement(3, 0));
    fprintf(inFile, "OUT OF SYNCH DUMP:  Trans (3,1):        %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetElement(3, 1));
    fprintf(inFile, "OUT OF SYNCH DUMP:  Trans (3,2):        %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetElement(3, 2));
    fprintf(inFile, "OUT OF SYNCH DUMP:  Trans (3,3):        %6.6f\n", (double)this->GetComplexUnit()->mPhysicalModel.GetLocation().GetElement(3, 3));
    fprintf(inFile, "OUT OF SYNCH DUMP:  Is Cloaked?:\t\t%2ld\n", (unsigned long)this->mIsCloaked);
    fprintf(inFile, "OUT OF SYNCH DUMP:\tPlanet ID:          %6ld\n", (unsigned long)this->mPlanetID);
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

// FUNCTION: EXE 0x5f9b72
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

// FUNCTION: EXE 0x5f9c12
// Returns this unit's waypoint marker at a tile, on the map or in the world's queued units
EEComplexUnit* EEComplexUnit::FindWaypointMarker(long inX, long inY)
{
    EEComplexUnit* theResult = NULL;
    EEMapTile* theTile = ::gWorld.mMap->GetTile(inX, inY);
    if (theTile != NULL) {
        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
            if (theResult != NULL)
                return theResult;
            EEUnit* theUnit = theNode->mUnit;
            if (theUnit != NULL && theUnit->mUnitClass->GetUnitType() == kUnitTypeWaypoint && (unsigned long)theUnit->mHitPoints == this->mUnitID
                && theUnit->mIsQueuedForDelete == false)
                theResult = static_cast<EEComplexUnit*>(theUnit);
        }
        if (theResult != NULL)
            return theResult;
    }
    return ::gWorld.FindQueuedUnitAt(inX, inY, this->mUnitID);
}

// FUNCTION: EXE 0x5f9c9b
// Tries random points away from a threat until the screen accepts one; returns whether one was found
bool EEComplexUnit::FindFleePoint(GE3DPoint* inTarget, GE3DPoint* outPoint, long inTries, float inMinAngle,
                            float inMaxAngle, float inMinDistance, float inMaxDistance,
                            EEFleeFromLocationScreenBase* inScreen)
{
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
    bool theResult = false;
    this->GetPosition(thePosition);
    float theAngle = EEGetHeadingBetween(inTarget, &thePosition);
    theAngle = EEJitterHeading(theAngle, inMinAngle, inMaxAngle);
    for (long i = inTries - 1; i >= 0 && !theResult; i--) {
        float theDistance = gPhysics->GetRandom(inMinDistance, inMaxDistance);
        thePoint.SetX(UMCos(theAngle) * theDistance + thePosition.mX);
        thePoint.SetY(UMCos(1.5707964f - theAngle) * theDistance + thePosition.GetY());
        if (inScreen->Screen(thePoint)) {
            *outPoint = thePoint;
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5f9d91
// Adds the finished building's sight and, for other players' units, shows it on the minimap
void EEComplexUnit::PartFinished()
{
    this->SightOn(this->GetTileX(), this->GetTileY());
    if (::gWorld.GetCurrentPlayer() != this->mPlayer)
        gMiniMap->MoveUnit(this, -10000, -10000, this->GetTileX(), this->GetTileY());
}

// FUNCTION: EXE 0x5f9dcb
// Returns the unit's line of sight: its upgraded stat plus its sight bonus
float EEComplexUnit::GetLineOfSight()
{
    return this->GetStats()->mLineOfSight + this->mSight;
}

// FUNCTION: EXE 0x5f9ddb
// Returns whether an area effect of the given kind acts on the unit
bool EEComplexUnit::HasEffect(long inKind)
{
    EEGraphicEffect* theEffect = NULL;
    return this->GetEffect(inKind, theEffect);
}

// FUNCTION: EXE 0x5f9df3
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

// FUNCTION: EXE 0x5f9e2d
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

// FUNCTION: EXE 0x5f9e81
// Returns whether a unit may pass through this gate (closed and locked rules, Trojan horse) or tile blocker
bool EEComplexUnit::IsPassableBy(EEComplexUnit* inUnit)
{
    if (this->GetUnitClass()->mCategory == kUnitCategoryGate && this->mHitPoints > 0) {
        EEComplexUnit* theCarrier = this;
        while (theCarrier->mPreviousPart != NULL)
            theCarrier = theCarrier->mPreviousPart;
        if (theCarrier != NULL && theCarrier->mIsDisabled && !theCarrier->IsProtected())
            return false;
        EEUCGate* theGate = EEUnitClassCast<EEUCGate>(this->GetUnitClass());
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

// FUNCTION: EXE 0x5f9f85
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

// FUNCTION: EXE 0x5f9fbb
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

// FUNCTION: EXE 0x5fa013
// Returns the first waypoint and the remaining path length; 7 when there is no path
long EEComplexUnit::GetPathStart(GE3DPoint& outLocation, float& outLength)
{
    if (this->mWaypoints.size() > 0) {
        EEComplexUnitWaypoint& theFirst = *this->mWaypoints.begin();
        outLength = this->mWaypoints.back().mDistance - theFirst.mStartDistance;
        outLocation = (*this->mWaypoints.begin()).mLocation;
        return 0;
    }
    return 7;
}

// FUNCTION: EXE 0x5fa07f
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
            theGoals = this->mComplexUnit->GetQueueGoals(i);
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

// FUNCTION: EXE 0x5fa1cd
// Returns the goals of a goal queue
std::deque<EEUnitGoal*>& EEComplexUnit::GetQueueGoals(unsigned long inIndex)
{
    return this->mGoalQueues[inIndex].mGoals;
}

// FUNCTION: EXE 0x5fa1e1
// Returns the remaining path length; 7 when there is no path
long EEComplexUnit::GetPathLength(float& outDistance)
{
    if (this->mWaypoints.size() > 0) {
        EEComplexUnitWaypoint& theFirst = *this->mWaypoints.begin();
        outDistance = this->mWaypoints.back().mDistance - theFirst.mStartDistance;
        return 0;
    }
    return 7;
}

// FUNCTION: EXE 0x5fa229
// Returns the next waypoint's location; 7 when there is no path
long EEComplexUnit::GetNextWaypointPoint(GE3DPoint& outLocation)
{
    if (this->mWaypoints.size() > 0) {
        outLocation = (*this->mWaypoints.begin()).mLocation;
        return 0;
    }
    return 7;
}

// FUNCTION: EXE 0x5fa269
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

// FUNCTION: EXE 0x5fa385
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
    if (theSize > 1 && (theContinent == NULL || theContinent->mCategory != inFlag)) {
        long theEndX = this->GetTileX() + theSize + inRange;
        long theEndY = this->GetTileY() + theSize + inRange;
        for (long x = this->GetTileX() - inRange; x < theEndX; x++) {
            for (long y = this->GetTileY() - inRange; y < theEndY; y++) {
                if (!((unsigned long)x < gMapWidth) || !((unsigned long)y < gMapHeight))
                    continue;
                unsigned short theID = ::gContinentManager.GetContinentID(x, y);
                EEAIContinent* theOther = theID != 0 ? ::gContinentManager.GetContinentByID(theID) : NULL;
                if (theOther == NULL || theOther->mCategory != inFlag)
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


// FUNCTION: EXE 0x5fa473
// Returns the unit's model, or the player's foundation graphic for a foundation
EEModel* EEComplexUnit::GetModel()
{
    if (!this->mIsFoundation)
        return this->GetStats()->mModel;
    return EEGetPlayerClassGraphic(this->GetPlayer(), this->GetUnitClass());
}

// FUNCTION: EXE 0x5fa493
// Returns the unit's world position
void EEComplexUnit::GetPosition(GE3DPoint& outPosition)
{
    this->mPhysicalModel.GetLocation().GetPosition(outPosition);
}

// FUNCTION: EXE 0x5fa4b4
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

// FUNCTION: EXE 0x5fa563
// Returns whether a target is ignored (optionally only from the same tiles), dropping expired entries
bool EEComplexUnit::IsTargetIgnored(EEComplexUnit* inTarget, bool inCheckLocation)
{
    bool theResult = false;
    unsigned long theUnitID = inTarget->mUnitID;
    std::deque<EEComplexUnitAttacker>::iterator theIterator = std::find(this->mIgnoredTargets.begin(), this->mIgnoredTargets.end(), theUnitID);
    if (theIterator != this->mIgnoredTargets.end()) {
        long theX = this->GetTileX();
        long theY = this->GetTileY();
        long theFromX = inTarget->GetTileX();
        long theFromY = inTarget->GetTileY();
        unsigned long theNow = gPhysics->mGameTime;
        if ((*theIterator).mTime <= theNow)
            this->mIgnoredTargets.erase(theIterator, this->mIgnoredTargets.end());
        else if (inCheckLocation) {
            if ((*theIterator).mTileX == theX && (*theIterator).mTileY == theY && (*theIterator).mFromX == theFromX
                && (*theIterator).mFromY == theFromY)
                theResult = true;
            else {
                theResult = false;
                this->mIgnoredTargets.erase(theIterator, theIterator + 1);
            }
        } else
            theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5fa661
// Returns whether the unit is disabled by a time warp (reasons 0x1a / 0x1c)
bool EEComplexUnit::IsProtected()
{
    return this->mIsDisabled && (this->mDisabledReason == 0x1a || this->mDisabledReason == 0x1c);
}

// FUNCTION: EXE 0x5fa67f
// Returns whether the unit's current goal is gathering
bool EEComplexUnit::IsGathering()
{
    return this->mGoalState != NULL && this->mGoalState->mKind == kGoalGather;
}

// FUNCTION: EXE 0x5fa696
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

// FUNCTION: EXE 0x5fa6f9
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

// FUNCTION: EXE 0x5fa785
// Returns whether the unit can get to a target, directly or through connected continents
bool EEComplexUnit::CanReachUnit(EEUnit* inTarget)
{
    bool theResult = gContinentManager.CanGetToUnit(this, inTarget);
    if (theResult)
        return theResult;
    EEAIContinent* theFrom = gContinentManager.GetContinentAt(this);
    EEAIContinent* theTo = gContinentManager.GetContinentAt(inTarget);
    if (theFrom != NULL && theTo != NULL) {
        theResult = gContinentManager.mTileFlags[theFrom->mID * gContinentManager.mFlagsWidth + theTo->mID];
        if (!theResult)
            theResult = gContinentManager.AreConnectedForUnit(inTarget, theFrom, theTo);
    }
    return theResult;
}

// FUNCTION: EXE 0x5fa7f6
// Returns the first continent of the given kind under the unit's footprint, or 0
unsigned short EEComplexUnit::GetFootprintContinent(long inKind)
{
    std::vector<EETileCoordinate> theTiles;
    unsigned short theResult = 0;
    EEGetFootprintTiles(this, theTiles);
    for (std::vector<EETileCoordinate>::iterator theIterator = theTiles.begin(); theIterator != theTiles.end(); ++theIterator) {
        unsigned short theContinent = ::gContinentManager.GetContinent(*theIterator, inKind);
        if (theContinent != 0 && theResult == 0)
            theResult = theContinent;
    }
    return theResult;
}

// FUNCTION: EXE 0x5fa856
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
        thePoint = theTown->mHull.GetClosestOutlinePoint(theTile);
        long theDistance = theTile.DistanceToSquared(thePoint);
        if (theDistance < theBestDistance) {
            theResult = theTown;
            theBestDistance = theDistance;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5fa906
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

// FUNCTION: EXE 0x5fa96c
// Returns whether the unit's fanaticism timer is running
bool EEComplexUnit::IsCloaked()
{
    return this->mCloaked > 0;
}

// FUNCTION: EXE 0x5fa979
// Removes the unit's colour tint
void EEComplexUnit::ClearTint()
{
    this->SetIsTinted(false);
}

// FUNCTION: EXE 0x5fa98c
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

// FUNCTION: EXE 0x5fab53
// Destroys UpdateFanaticismColor's third static colour at exit (an empty destructor)

// FUNCTION: EXE 0x5fab54
// Destroys UpdateFanaticismColor's second static colour at exit (an empty destructor)

// FUNCTION: EXE 0x5fab55
// Destroys UpdateFanaticismColor's first static colour at exit (an empty destructor)

// FUNCTION: EXE 0x5fab56
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
        EEGetSideTiles(theX, theY, this->GetUnitClass()->mTileSize, theTiles);
        std::vector<U2DPoint<long> >::iterator theTile = theTiles.begin();
        while (theTile != theTiles.end() && !theFound) {
            theFound = inX == theTile->GetX() && inY == theTile->GetY();
            ++theTile;
        }
        if (!theFound) {
            EEGetCornerTiles(0, theX, theY, this->GetUnitClass()->mTileSize, theTiles);
            theTile = theTiles.begin();
            while (theTile != theTiles.end() && !theFound) {
                theFound = inX == theTile->GetX() && inY == theTile->GetY();
                ++theTile;
            }
        }
    }
    return theFound;
}


// FUNCTION: EXE 0x5fac37
// Resets every mobile-unit field to defaults with the given goal queue count, optionally a random heading
void EEComplexUnit::ResetEEComplexUnit(unsigned long inGoalQueueCount, bool inRandomHeading)
{
    this->mBuildQueue.clear();
    this->mCitizenBuildQueue.clear();
    this->mBoundedPathTiles.clear();
    this->mCargo.clear();
    this->mWaypoints.clear();
    this->mGoalQueues.clear();
    EEUnitAreaEffectList& theEffects = this->mAreaEffects;
    theEffects.Clear();
    theEffects.mItems.clear();
    EEUnitAreaEffectList& theOthers = this->mAffectingAreaEffects;
    theOthers.Clear();
    theOthers.mItems.clear();
    this->mUnitVariables.clear();
    this->mUnitVariables.resize(3, 0);
    this->mLastMapPosition = GE3DPoint(0.0f, 0.0f, 0.0f);
    this->mU0DC.SetX(0.0f);
    this->mU0DC.SetY(0.0f);
    this->mCollisionRadius = 0.0f;
    this->mInitialContactHeading = kNoHeading;
    this->mBehavior = this->GetUnitClass() != NULL ? this->GetUnitClass()->mBehavior : EEUnitBehavior::Get(5);
    this->mLauncherClass = NULL;
    this->mOwner = NULL;
    this->mActiveGroup = NULL;
    this->mPreviousPart = NULL;
    this->mNextPart = NULL;
    this->mHomeBase = NULL;
    this->mContainer = NULL;
    this->mVelocity = 0.0f;
    this->mF21C = 0.0f;
    this->mOpacity = 0.0f;
    this->mIsCollidable = 1;
    if (inRandomHeading)
        this->mF220 = gPhysics->mRandom.Generate() * 6.2831855f;
    else
        this->mF220 = 6.2831855f;
    this->mNextTriggerTime = (unsigned long)-1;
    this->mSound = NULL;
    this->mExitingUnitID = -1;
    this->mLastHitPoints = 0;
    this->mWeaponTagPoint = 0;
    this->mFlightTime = 0;
    this->mWaterState = 0;
    this->mReloadTimer = this->GetUnitClass() != NULL && this->mPlayer != NULL ? this->GetStats()->mReloadTime : 0;
    this->mPowerRegenTimer = this->GetUnitClass() != NULL && this->mPlayer != NULL ? this->GetStats()->mPowerRegeneration : 0;
    this->mPower = 0;
    this->mProductionTime = 0;
    this->mAnimationTime = 0;
    this->mTargetSearchTimer = 0;
    this->mRegenerationTimer = 0;
    this->mHealSearchTimer = 0.0f;
    this->mWorkTimer = 0;
    this->mAmmunition = 0;
    this->mCanBeHidden = false;
    this->mNextFrame = 0;
    this->mCollisionState = 0;
    this->mCollisionTile.mX = -10000;
    this->mCollisionTile.mY = -10000;
    this->mCollisionResolutionCount = 0.0f;
    this->mF24C = 0.0f;
    this->mSight = 0.0f;
    this->mIsDisabled = false;
    this->mIsCommandable = true;
    this->mIsAnimationPaused = false;
    this->mHasArrived = false;
    this->mIsPathDone = true;
    this->mIsClearanceGranted = false;
    this->mIsBuilt = true;
    this->mAnimation = 7;
    this->mHeight = 0.0f;
    this->mIsDetachedFromAI = false;
    this->mIsCitizen = false;
    this->mIsTinted = false;
    this->mIsLocked = false;
    this->mIsPacked = false;
    this->mIsSpecialAttack = false;
    this->mIsFoundation = false;
    this->mProjectileStart.SetX(0.0f);
    this->mProjectileStart.SetY(0.0f);
    this->mProjectileStart.SetZ(0.0f);
    this->mActiveGoal = NULL;
    this->mGoalState = NULL;
    this->mPushImmuneUntil = 0;
    this->mIsCloakable = this->GetUnitClass() != NULL && this->GetUnitClass()->mIsCloakable;
    this->mNumPathRequests = 0;
    this->mU0DC.SetX(-10000.0f);
    this->mCargoRoom = 0;
    this->mU0DC.SetY(-10000.0f);
    this->mConversionEffect.mValue = -1;
    this->mHealEffect.mValue = -1;
    this->mBlockingUnit = NULL;
    this->mGateDirection = 0;
    this->mFormationSlot = 1;
    this->mIsFiringHigh = false;
    if (this->GetUnitClass() != NULL)
        this->mGarrisonCount = this->GetUnitClass()->mInitialGarrisonCount;
    else
        this->mGarrisonCount = 0;
    this->mGarrisonPopulation = 0;
    this->mIsCloaked = false;
    this->mIsDeletable = true;
    if (this->GetUnitClass() != NULL)
        this->mIsPaidFor = !this->GetUnitClass()->IsLandBuilding();
    else
        this->mIsPaidFor = false;
    this->mCurrentResearch = kTechNone;
    this->mCurrentPhysicsAction = 6;
    this->mCurrentPhysicsOrder = 5;
    this->mFacing = 0;
    this->mCrewHitPoints = 0;
    this->mRottingAwayTime = 0;
    this->mTrailEffectTimer = 0;
    this->mWakeTimer = 0;
    this->mTimesUpdated = 0;
    this->mPatrolGroup = NULL;
    this->mTurretAngles = NULL;
    this->mNumOrbitals = 0;
    this->mOrbitals = NULL;
    this->mNumWheels = 0;
    this->mWheels = NULL;
    this->mGatheredResource = NULL;
    this->mLastResource = NULL;
    this->mConversionStartTime = 0;
    this->mCloaked = 0;
    this->mNumAreaEffects = 0;
    this->mU2FC = 0;
    this->mCommand = NULL;
    this->mLastAttacker = NULL;
    this->mLaunchYaw = 0.0f;
    this->mAnimationFraction = 0.0f;
    this->mOpenFraction = 0.0f;
    this->mNumTurretAngles = 0;
    this->mExperience = 0;
    this->mFormationRank = 0;
    this->mPositiveMorale = 0;
    this->mNegativeMorale = 0;
    this->mPhysicalModel.SetModel(NULL);
    this->mPhysicalModel.SetScale(1.0f);
    this->mPhysicalModel.SetLocation(GETransformation());
    this->mPhysicalModel.SetFrame(0);
    this->mAngularVelocity.SetPitch(0.0f);
    this->mAngularVelocity.SetYaw(0.0f);
    this->mAngularVelocity.SetRoll(0.0f);
    this->mOrientation = this->mAngularVelocity;
    this->mPushVelocityX = 0.0f;
    this->mPushVelocityY = 0.0f;
    this->mColor.SetBlue(1.0f);
    this->mColor.SetGreen(1.0f);
    this->mColor.SetRed(1.0f);
    this->mAIState = 300000;
    this->mL334 = 0;
    this->mL338 = 0;
    this->mNumBuilders = 0;
    this->mMine = NULL;
    this->mGoalQueues.resize(inGoalQueueCount, EEUnitVectorItem(this));
    this->ReleaseHeldUnits();
    this->mB34F = false;
    this->mIsRegrowing = false;
    this->mCalamityFlags = 0;
    this->mHerd = NULL;
    this->mHolder = NULL;
    this->mGrowth = 1.0f;
    this->mGrazeTime = 0;
    this->mFireEffect.mValue = 0;
    this->mNumAttackers = 0;
    this->mGrowTime = 0;
    this->mCamouflageTimer = 0;
    this->mU32C = 0;
    this->mPlanetID = 0;
    this->mRoll = 0.0f;
    this->mHasShiftQueuedBuild = false;
}

// FUNCTION: EXE 0x5fb18f
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

// FUNCTION: EXE 0x5fb383
// Kills or removes the unit: records death statistics and insurance, releases cargo, garrison, holder, goals
// and effects, then kills its later parts and spawns its death and remains units
void EEComplexUnit::Kill(bool inKilled, bool inKeepResearch, bool inOther)
{
    std::vector<EETileCoordinate> theTiles;
    bool theSuppress = inKeepResearch || inOther;
    if (this->mHitPoints == 0)
        return;
    this->mPlayer->UnitTaken(this, theSuppress);
    if (inKilled && !gWorldClearing) {
        EEUnitSet* theSet = gUnitSetManager.GetSet(0x92);
        if (theSet != NULL && theSet->Contains(this->GetUnitClass()->GetUnitType())) {
            for (unsigned long i = 0; (unsigned short)i < ::gWorld.mNumPlayers; i++) {
                EEPlayer* thePlayer = ::gWorld.mPlayers[(unsigned short)i];
                if (thePlayer != NULL && thePlayer->mEnemies[this->mPlayer->mPlayerID] == 1)
                    thePlayer->UpdateFreeUnitPower();
            }
            ESDebugOut("EEComplexUnit::Kill - Increment dead citizen count - type - %ld\n", this->GetUnitClass()->GetUnitType());
        }
    }
    if (this->GetUnitClass()->mIsInactive)
        ::gWorld.ActivateUnit(this);
    bool theRefund = true;
    if (this->mPreviousPart == NULL && !UShutdownInProgress()) {
        long theState = gPhysics->GetUnitSwapKind(this);
        bool theSpecial = theState == 5 || theState == 6 || theState == 7 || theState == 8;
        if (theState != 1) {
            if (inKilled || theSpecial) {
                if (this->GetUnitClass()->GetUnitType() != kUnitTypePathPoint && !theSpecial) {
                    long theCategory = this->GetUnitClass()->mCategory;
                    long theSeries = theCategory == kUnitCategoryBuilding || theCategory == kUnitCategoryBuilding19 || theCategory == kUnitCategoryGate ||
                        theCategory == kUnitCategoryMiningBuilding || theCategory == kUnitCategoryFortress ? 1 : 4;
                    this->mPlayer->mSavedGroups.Reset(theSeries, 1);
                }
                long theSeries = EEAddDeadUnitStatistic(this);
                if (theSeries != 0)
                    this->mPlayer->mSavedGroups.Lower(theSeries, 1);
            } else
                theRefund = false;
        }
    }
    if (theRefund && inKilled && !theSuppress && gCivPowersManager.HasPower(this->GetPlayer(), kCivPowerInsurance)) {
        EECivPower thePower;
        if (gCivPowersManager.GetPower(kCivPowerInsurance, thePower) &&
            gUnitSetManager.Contains(this->GetUnitClass()->GetUnitType(), thePower.mUnitSet) &&
            (this->GetUnitClass()->GetUnitType() != kUnitTypeFarm || this->mIsBuilt))
            this->PayInsurance(thePower.mAmount);
    }
    if (this->mLastAttacker != NULL)
        this->mLastAttacker->Dereference();
    this->mLastAttacker = NULL;
    if (this->mOwner != NULL)
        this->mOwner->Dereference();
    this->mOwner = NULL;
    this->UpdateJITManufacturing();
    this->ClearContacts();
    this->KillCargo();
    this->ClearGarrison();
    if (this->mHolder != NULL) {
        this->mHolder->ReleaseHeld(this);
        if (this->mHolder != NULL)
            this->mHolder->Dereference();
        this->mHolder = NULL;
    } else
        this->ReleaseHeldUnits();
    gPathManager.CancelPaths(this, true);
    this->DropPath();
    if (this->GetUnitClass()->mAirTrafficType != 0) {
        if (this->GetUnitClass()->IsAirBase())
            this->mPlayer->mAirTraffic[this->GetUnitClass()->mAirTrafficType - 1]->RemoveAirport(this);
        else
            this->mPlayer->mAirTraffic[this->GetUnitClass()->mAirTrafficType - 1]->RemoveCargoWeight(this);
    }
    this->ClearCollisionState();
    this->StartAction(inKilled ? 6000 : 6003);
    this->GetUnitClass()->ActionChanged(this);
    if (this->mNextPart != NULL)
        this->mNextPart->Kill(inKilled, false, false);
    if (!theSuppress) {
        if (this->GetUnitClass()->mDeathSpawnClass != NULL && !gWorldClearing && !gFreeCamera)
            this->GetUnitClass()->CreateDeathUnit(this);
        if (this->GetUnitClass()->mRemainsClass != NULL && !this->GetUnitClass()->mIsProjectile && !gWorldClearing && !gFreeCamera)
            this->GetUnitClass()->CreateRemainsUnit(this);
    }
    if (this->mContainer != NULL)
        this->mContainer->ReleaseCargo(this);
    this->SetGatheredResource(NULL);
    this->SetBlockingUnit(NULL);
    this->SetLastResource(NULL);
    if (!inKeepResearch)
        this->CancelQueuedProduction();
    for (unsigned long i = 0; i < this->mGoalQueues.size(); i++)
        this->ClearGoalQueue(i);
    this->mAffectingAreaEffects.KeepStrongest();
    this->mAffectingAreaEffects.Clear();
    this->mAffectingAreaEffects.mItems.clear();
    this->mAreaEffects.Clear();
    this->mAreaEffects.mItems.clear();
    this->ReleaseConversionEffect();
    this->ReleaseHealEffect();
    this->ClearTint();
    this->ReleaseFireEffect();
    this->mGoalState = NULL;
    this->mActiveGoal = NULL;
    EEUnit::Kill(inKilled, inKeepResearch, inOther);
}



// FUNCTION: EXE 0x5fb7fb
// Returns the unit to the complex-unit memory pool when its last reference goes
void EEComplexUnit::LastDereference()
{
    this->FreeNode();
    this->mReferenceCount = 0;
    EEComplexUnit::sMemoryPool.Delete(this);
}

// FUNCTION: EXE 0x5fb814
// Loads the unit's model (the foundation model, or the animation model with turret angles, orbitals and wheels)
// and, except for land buildings, the player's buttons for its class
void EEComplexUnit::LoadMedia(bool inIsFoundation)
{
    EEUnit::LoadMedia(inIsFoundation);
    if (!this->GetUnitClass()->IsLandBuilding())
        this->GetPlayer()->LoadUnitClassButtons(this->GetUnitClass()->GetUnitType());
    this->mIsFoundation = inIsFoundation;
    if (inIsFoundation == true) {
        this->mPhysicalModel.SetModel(EEGetPlayerClassGraphic(this->GetPlayer(), this->GetUnitClass())->mModel);
    } else {
        const EEGraphicObject* theModel = this->GetModel();
        this->mPhysicalModel.SetModel(theModel->mModels[this->mAnimation]);
        this->mTurretAngles = NULL;
        this->ResizeTurretAngles(theModel->GetNumParts());
        this->mNumOrbitals = theModel->GetOrbitalCount();
        if (this->mNumOrbitals == 0)
            this->mOrbitals = NULL;
        else {
            this->mOrbitals = new EEOrbital*[this->mNumOrbitals];
            for (unsigned long i = 0; i < this->mNumOrbitals; ++i)
                this->mOrbitals[i] = new EEOrbital(*theModel->mOrbitals[i]);
        }
        this->mNumWheels = theModel->GetWheelCount();
        if (this->mNumWheels == 0)
            this->mWheels = NULL;
        else {
            this->mWheels = new EEWheel*[this->mNumWheels];
            for (unsigned long i = 0; i < this->mNumWheels; ++i) {
                this->mWheels[i] = new EEWheel(*theModel->mWheels[i]);
                if (i < this->mNumWheels)
                    this->mWheels[i]->mAngle = 0.0f;
            }
        }
    }
}

// FUNCTION: EXE 0x5fb9a0
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
            EEUGGather* theGather = EEGoalCast<EEUGGather>(this->GetGoal());
            if (theGather->FindGatherSpot(this)) {
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
                theFound = EECanStandOn(theTile, this);
                if (theFound)
                    break;
            }
            if (theFound == true) {
                thePoint.SetX((float)theTile.mX + 0.5f);
                thePoint.SetY((float)theTile.GetY() + 0.5f);
                EEUnitGoal* theGoal = new EEUGLocation(thePoint, false, true, false, 0.0f, false, 0);
                this->SetGoal(theGoal, theCommand == kActionGatherResource, 0);
                this->StartAction(kActionPrepareToMove);
                this->GetUnitClass()->ActionChanged(this);
                theResult = true;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5fbc3b
// Finds the unit's waypoint on the given tile (off-map waypoints count at their closest tile); returns true if found
bool EEComplexUnit::FindWaypointAtTile(long inX, long inY, EEComplexUnitWaypoint*& outWaypoint)
{
    // gWorld.GetMap() is read twice, with no local: the original keeps it in eax, not in a callee-saved register
    bool theFound = false;
    for (unsigned long i = 0; i < this->mWaypoints.size(); i++) {
        long theX = UMFloor(this->mWaypoints[i].mLocation.GetX());
        long theY = UMFloor(this->mWaypoints[i].mLocation.GetY());
        if (::gWorld.GetMap()->GetTile(theX, theY) == NULL)
            ::gWorld.GetMap()->GetClosestTile(&this->mWaypoints[i].mLocation, &theX, &theY);
        if (theX != inX || theY != inY)
            continue;
        theFound = true;
        outWaypoint = &this->mWaypoints[i];
        break;
    }
    return theFound;
}

// FUNCTION: EXE 0x5fbd5f
// Returns true if the unit is built, alive and below its maximum hit points
bool EEComplexUnit::IsRepairable()
{
    return this->mHitPoints > 0 && this->mHitPoints < this->GetStats()->mMaxHitPoints && this->mIsBuilt;
}

// FUNCTION: EXE 0x5fbd81
// Adjusts a value by the unit's morale: positive morale takes a share off, negative morale adds one (combat.yaml
// morale: 80% and 60% in the original)
void EEComplexUnit::ApplyMorale(long inValue, long& outValue)
{
    EeCombatMoraleIn theIn = {(int32_t)inValue, (int32_t)this->mPositiveMorale, (int32_t)this->mNegativeMorale,
                              EEGetRules().mCombat.mPositiveMorale, EEGetRules().mCombat.mNegativeMorale};
    outValue = EECombatMorale(theIn);
}

// FUNCTION: EXE 0x5fbdd1
// Drops the unit's first waypoint (and its marker while waypoint markers are on); returns 0, or 7 if there was none
long EEComplexUnit::NextWaypoint()
{
    long theResult;
    if (this->mWaypoints.size() > 0) {
        long theX = UMFloor((*this->mWaypoints.begin()).mLocation.GetX());
        long theY = UMFloor((*this->mWaypoints.begin()).mLocation.GetY());
        EEMap* theMap = ::gWorld.mMap;
        if (theMap->GetTile(theX, theY) == NULL)
            theMap->GetClosestTile(&(*this->mWaypoints.begin()).mLocation, &theX, &theY);
        if (sB563234)
            this->RemoveWaypointMarker(theX, theY);
        this->mWaypoints.pop_front();
        theResult = 0;
    } else
        theResult = 7;
    this->mCollisionResolutionCount = 0.0f;
    return theResult;
}

// FUNCTION: EXE 0x5fbecf
// Gets the location of the unit's second waypoint; returns 0, or 7 if it has fewer than two
long EEComplexUnit::GetSecondWaypoint(GE3DPoint& outLocation)
{
    if (this->mWaypoints.size() > 1) {
        outLocation = this->mWaypoints[1].mLocation;
        return 0;
    }
    return 7;
}

// FUNCTION: EXE 0x5fbf16
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

// FUNCTION: EXE 0x5fbf66
// Drops goals from the front of a goal queue while they no longer suit the unit, then refreshes its current goals
void EEComplexUnit::DropUnsuitedGoals(unsigned long inIndex)
{
    // NEAR: the original calls an out-of-line EEUnitVectorItem front accessor (0x1f8a26, inline in the real header)
    bool thePopped = false;
    if (this->mGoalQueues.size() <= inIndex)
        return;
    while (this->mGoalQueues[inIndex].mGoals.size() > 0 && !this->mGoalQueues[inIndex].mGoals.front()->Suits(this)) {
        this->mGoalQueues[inIndex].PopFront();
        thePopped = true;
    }
    this->RefreshGoals();
    if (thePopped)
        this->RefreshActiveGoal();
}

// FUNCTION: EXE 0x5fbfdd
// Moves the unit away after a calamity: a computer player's prophet steps away from the cast, other AI units go idle,
// a human's unit walks to a random point ahead (unless inArg is set)
void EEComplexUnit::FleeAfterCalamity(long inArg)
{
    if (this->mPlayer->mComputerPlayer != NULL) {
        EECPAttackSubGroup* theGroup = this->mPlayer->mComputerPlayer->mCombatManager->mAttackCommander->FindHolder(this);
        if (theGroup != NULL) {
            EECPASGUProphet* theProphet = dynamic_cast<EECPASGUProphet*>(theGroup->FindUnit(this));
            if (theProphet != NULL) {
                theProphet->MoveAwayFromCast();
                return;
            }
        }
        this->StartAction(kActionIdle);
    } else {
        if ((unsigned char)inArg != 0)
            return;
        GE2DPoint thePoint(0.0f, 0.0f);
        if (!EEGetRandomPointAhead(this, &thePoint))
            return;
        this->SetGoal(new EEUGLocation(thePoint, true, true, false, 0.0f, false, false), 0, 0);
        this->StartAction(kActionPrepareToMove);
    }
    this->GetUnitClass()->ActionChanged(this);
}

// FUNCTION: EXE 0x5fc0a8
// Turns a group path into location goals for the unit (replacing its location goal, or after its attack goal),
// skipping up to three points near its start sector, and sets it moving
// inPath's constness is inferred (non-const is the least the body needs)
void EEComplexUnit::SetGroupPathGoals(std::vector<GE3DPoint>& inPath)
{
    std::vector<GE3DPoint> thePath;
    EEUnitVectorItem* theQueue = &this->mGoalQueues[0];
    EEUGAttackUnit* theAttack = EEGoalCast<EEUGAttackUnit>(this->GetGoal());
    EEUGLocation* theLocation = EEGoalCast<EEUGLocation>(this->GetGoal());
    EEAIPathSector* theStartSector = gContinentManager.GetPathSector(this);
    long theSkipped = 0;
    bool theKeepAttack = false;
    bool theReplaced = false;
    if (this->mHitPoints < 1 || this->mIsDisabled || this->mTile == NULL)
        goto done;
    if (theLocation == NULL) {
        if (theAttack == NULL) {
            if (this->GetCommandType() != kActionIdle)
                goto done;
        } else if (!theAttack->mB1C) {
            theKeepAttack = true;
            if (theQueue->mGoals.size() < 2)
                goto done;
        }
    } else
        theLocation->mGroupPathState = 3;
    if (!theKeepAttack && (theQueue->mGoals.size() == 0
        || (theQueue->mGoals.back()->mKind == 1 && static_cast<EEUGLocation*>(theQueue->mGoals.back())->mIsAutoWaypoint)))
        goto done;
    if (inPath.size() >= 2) {
        thePath.reserve(inPath.size());
        bool theSkip = true;
        for (long i = 0; i < (long)inPath.size(); i++) {
            if (theSkip) {
                EETileCoordinate theTile;
                theTile.mX = UMFloor(inPath[i].GetX());
                theTile.mY = UMFloor(inPath[i].GetY());
                EEAIPathSector* theSector = gContinentManager.GetPathSector(theTile);
                if (theSkipped >= 3)
                    theSkip = false;
                else {
                    U2DPoint<long> theStart(theStartSector->mCenter.mX, theStartSector->mCenter.mY);
                    U2DPoint<long> theEnd(theSector->mCenter.mX, theSector->mCenter.mY);
                    if (!(theStart.DistanceToSquared(theEnd) >= 2))
                        theSkipped++;
                    else
                        theSkip = false;
                }
            }
            if (!theSkip)
                thePath.push_back(inPath[i]);
        }
        for (long thePathIndex = thePath.size() - 1; thePathIndex >= 0; thePathIndex--) {
            EEUnitGoal* theGoal = new EEUGLocation(&thePath[thePathIndex], this->mGoalState != NULL && this->mGoalState->mB1C,
                theLocation != NULL && theLocation->mIsForcedMove, false, 0.0f, true, false);
            if (!theKeepAttack) {
                this->SetGoal(theGoal, 1, 0);
                theReplaced = true;
            } else
                this->mGoalQueues[0].Insert(1, theGoal);
        }
    }
    this->RefreshGoals();
    this->RefreshActiveGoal();
    if (!theReplaced) {
        EEUGLocation* theCurrent = EEGoalCast<EEUGLocation>(this->GetGoal());
        if (theCurrent == NULL)
            goto done;
        long theY = UMFloor(theCurrent->mLocation.GetY());
        long theX = UMFloor(theCurrent->mLocation.GetX());
        if (this->GetTileX() == theX && this->GetTileY() == theY)
            goto done;
        if (this->mWaypoints.size() != 0 && UMFloor(this->mWaypoints.back().mLocation.GetX()) == theX
            && UMFloor(this->mWaypoints.back().mLocation.GetY()) == theY)
            goto done;
        theCurrent->mGroupPathState = 4;
    }
    this->StartAction(kActionPrepareToMove);
    this->GetUnitClass()->ActionChanged(this);
done:
    this->RefreshGoals();
    this->RefreshActiveGoal();
}

// FUNCTION: EXE 0x5fc507
// Snaps the last waypoint to the goal's exact location when both share a tile (location, attack, build, load goals)
void EEComplexUnit::SnapLastWaypoint()
{
    // NEAR: the waypoint copy goes through an out-of-line operator= (0x1a95d6) in the original
    GE3DPoint theGoalLocation(0.0f, 0.0f, 0.0f);
    EEComplexUnitWaypoint theLast;
    theLast.mLocation = GE3DPoint(0.0f, 0.0f, 0.0f);
    theLast.mDistance = 0.0f;
    theLast.mStartDistance = 0.0f;
    if (this->mWaypoints.size() <= 0 || this->mGoalState == NULL)
        return;
    unsigned char theKind = this->mGoalState->mKind;
    if (theKind <= 0 || (!(theKind <= kGoalAttackUnit) && theKind != kGoalBuildRepair && (theKind <= 5 || (!(theKind <= kGoalLoadTransport) && theKind != kGoalLoadGarrison))))
        return;
    this->mGoalState->GetGoalLocation(theGoalLocation);
    theLast = this->mWaypoints.back();
    if (UMFloor(theLast.mLocation.GetX()) == UMFloor(theGoalLocation.GetX())
        && UMFloor(theLast.mLocation.GetY()) == UMFloor(theGoalLocation.GetY())) {
        this->mWaypoints.pop_back();
        this->AddWaypoint(theGoalLocation, false);
    }
}

// FUNCTION: EXE 0x5fc61b
// Adjusts a path for the unit's class and adds it as waypoints (stored end first), skipping the start tile it stands on
void EEComplexUnit::TakePath(std::vector<GE3DPoint>& ioPath, bool inSmooth)
{
    long theSkip = 1;
    if (ioPath.size() > 1 && this->GetTileX() == UMFloor(ioPath.back().GetX()) && this->GetTileY() == UMFloor(ioPath.back().GetY()))
        theSkip = 2;
    this->GetUnitClass()->AdjustPath(this, ioPath, inSmooth);
    for (long i = ioPath.size() - theSkip; i >= 0; i--)
        this->AddWaypoint(ioPath[i], false);
}

// FUNCTION: EXE 0x5fc6e8
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

// FUNCTION: EXE 0x5fc90c
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

// FUNCTION: EXE 0x5fca21
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

// FUNCTION: EXE 0x5fca95
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

// FUNCTION: EXE 0x5fcb0e
// Pops auto-waypoint goals off the main queue, starting each following goal that suits the unit
void EEComplexUnit::DropFinishedGoals(bool inStartAll)
{
    if (this->mGoalState != NULL && this->mGoalState->mKind == kGoalLocation && static_cast<EEUGLocation*>(this->mGoalState)->mIsAutoWaypoint == true) {
        EEUGLocation* theGoal = static_cast<EEUGLocation*>(this->mGoalState);
        while (true) {
            if (theGoal->mIsAutoWaypoint != true)
                break;
            this->mGoalQueues[0].PopFront();
            theGoal = NULL;
            EEUnitGoal* theNext = this->RefreshGoals();
            if (theNext->mKind == kGoalLocation)
                theGoal = static_cast<EEUGLocation*>(theNext);
            if (theGoal == NULL || inStartAll || !theGoal->mIsAutoWaypoint) {
                if (theNext->Suits(this))
                    theNext->Start(this);
            }
            if (theGoal == NULL)
                break;
        }
    }
    this->RefreshGoals();
    this->RefreshActiveGoal();
}

// FUNCTION: EXE 0x5fcb8c
// Removes a unit from the cargo and gives its room back; refreshes the icon bar if the transport is selected
void EEComplexUnit::ReleaseCargo(EEComplexUnit* inCargo)
{
    long theIndex = this->mCargo.size() - 1;
    while (theIndex >= 0 && this->mCargo[theIndex] != inCargo)
        theIndex--;
    if (theIndex < 0)
        return;
    EEComplexUnit* theCargo = this->mCargo[theIndex];
    this->GetUnitClass()->CargoRemoved(this, theCargo);
    this->mCargo.erase(this->mCargo.begin() + theIndex);
    this->mCargoRoom += theCargo->GetUnitClass()->mWeight;
    if (this->mPlayer == ::gWorld.GetCurrentPlayer() && this->mPlayer->mActiveGroup->Contains(this))
        gUserInterface->PostMessage424Once();
    theCargo->Dereference();
    theCargo->mContainer->Dereference();
    theCargo->mContainer = NULL;
}

// FUNCTION: EXE 0x5fcc91
// Unloads every cargo unit that fits near the tile (teleporters show the teleport effect) and drops dead ones;
// posts an unload-failed event if any stay aboard
void EEComplexUnit::UnloadCargoAtTile(long inX, long inY, bool inArg)
{
    GETransformation theTransformation;
    GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
    long theEvent = -1;
    EEUCTransport* theTransportClass = NULL;
    if (this->GetUnitClass()->IsTransport())
        theTransportClass = EEUnitClassCast<EEUCTransport>(this->GetUnitClass());
    for (long i = this->mCargo.size() - 1; i >= 0; i--) {
        EEComplexUnit* theCargo = this->GetCargo(i);
        if (this->GetUnitClass()->IsTransport() && (theTransportClass == NULL
            || !theTransportClass->HasRoom(this, theCargo->GetComplexUnit())))
            continue;
        if (!(theCargo->mHitPoints < 1)) {
            long theRings = UMFloatToLong(theTransportClass != NULL ? theTransportClass->mUnloadRange : this->GetStats()->mRange);
            if (EEFindPlacement(&thePoint, theCargo, NULL, inX, inY, 1, theRings, inArg,
                true, false, false, &theTransformation, NULL)) {
                this->UnloadCargoAt(i, theTransformation);
                if (this->GetUnitClass()->GetUnitType() == kUnitTypeTeleporter)
                    gWorld.GetEffectManager()->CreateUnitEffect(theCargo, EEGraphicEffect::Get(kGraphicEffectCalamityTeleportIn), NULL, false, 0, 0);
            }
        } else {
            this->ReleaseCargo(theCargo);
        }
    }
    if (this->GetNumCargoUnits() != 0) {
        if (theTransportClass != NULL)
            theEvent = theTransportClass->GetUnloadFailedTextID();
        U2DPoint<long> theTile(this->GetTileX(), this->GetTileY());
        this->mPlayer->mEvents.AddEvent(theTile, 2, this->mPlayer, theEvent, false);
    }
}

// FUNCTION: EXE 0x5fce47
// Empties the build queue, refunding each entry and allowing its technology again
void EEComplexUnit::CancelQueuedProduction()
{
    if (this->mBuildQueue.size() <= 0)
        return;
    while (this->mBuildQueue.size() > 0) {
        long theTechnology = this->mBuildQueue.front().mTechnology;
        this->mBuildQueue.erase(this->mBuildQueue.begin());
        EETechTreeNode* theNode = this->mPlayer->mTechnologies->Get(theTechnology);
        theNode->mIsAllowed = true;
        if (theNode->mData->mUnitClass > 0 && gUnitClassTable[theNode->mData->mUnitClass] != NULL
            && gUnitClassTable[theNode->mData->mUnitClass]->mAIKind == kAIKindHero && this->mPlayer->mNumHeroesInProduction > 0)
            this->mPlayer->mNumHeroesInProduction--;
        this->mPlayer->RefundTechnology(this->mPlayer->mTechnologies->Get(theTechnology), this->GetUnitClass()->mUnitType, 1.0f);
    }
}

// FUNCTION: EXE 0x5fcf16
// Unloads a cargo unit at the location (aircraft at cruise altitude) and sets it idle; returns it if something else
// still references it, else NULL
EEComplexUnit* EEComplexUnit::UnloadCargoAt(unsigned long inIndex, const GETransformation& inLocation)
{
    EEComplexUnit* theCargo = this->GetCargo(inIndex);
    if (theCargo->mHitPoints > 0) {
        this->GetUnitClass()->CargoUnloaded(this, theCargo);
        theCargo->SetTransformation(inLocation, true);
        if (theCargo->mUnitClass->mDomains & kMovementAir) {
            theCargo->mPlanetID = EEGetLandContinentOf(theCargo);
            if (!this->GetUnitClass()->IsAirBase()) {
                GETransformation theLocation(inLocation);
                theLocation.SetZ(theLocation.GetZ() + theCargo->mUnitClass->GetCruiseAltitude());
                theCargo->SetTransformation(theLocation, true);
            }
        }
        if (theCargo->mPreviousPart == NULL) {
            if (theCargo->GetTileX() != -10000)
                ::gWorld.ActivateUnit(theCargo);
            else
                ::gWorld.DeactivateUnit(theCargo);
        }
    }
    long theCategory = theCargo->mUnitClass->mCategory;
    if (!theCargo->mPlayer->IsHuman() && (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen)) {
        if (theCargo->mContainer != NULL)
            theCargo->mContainer->Dereference();
        theCargo->mContainer = NULL;
    }
    if (theCargo->mHitPoints > 0 && (theCargo->mPlayer->IsHuman()
        || (!(theCargo->mUnitClass->mDomains & kMovementAir) && theCargo->mUnitClass->GetUnitType() != kUnitTypeSp15SpaceFighter))) {
        theCargo->StartAction(kActionIdle);
        theCargo->mUnitClass->ActionChanged(theCargo);
    }
    theCategory = theCargo->mUnitClass->mCategory;
    if (theCargo->mPlayer->IsHuman() || (theCategory != kUnitCategoryLandCitizen && theCategory != kUnitCategoryNavalCitizen)) {
        if (theCargo->mContainer != NULL)
            theCargo->mContainer->Dereference();
        theCargo->mContainer = NULL;
    }
    this->mCargo.erase(this->mCargo.begin() + inIndex);
    this->mCargoRoom += theCargo->mUnitClass->mWeight;
    unsigned long theReferences = theCargo->mReferenceCount;
    if (this->mPlayer == ::gWorld.GetCurrentPlayer() && this->mPlayer->mActiveGroup->Contains(this))
        gUserInterface->PostMessage424Once();
    theCargo->Dereference();
    return theReferences > 1 ? theCargo : 0;
}

// FUNCTION: EXE 0x5fd0fe
// Unloads one cargo unit next to the transport now (a dead one is just released); returns true on success
bool EEComplexUnit::UnloadCargoNow(EEComplexUnit* inCargo)
{
    GETransformation theTransformation;
    GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
    EEUnitSet* theSet = gUnitSetManager.mSets.size() > 99 ? gUnitSetManager.mSets[99] : 0;
    bool theResult = false;
    EEUCTransport* theTransportClass = NULL;
    if (this->GetUnitClass()->IsTransport())
        theTransportClass = EEUnitClassCast<EEUCTransport>(this->GetUnitClass());
    long theIndex = this->mCargo.size() - 1;
    while (theIndex >= 0 && this->GetCargo(theIndex) != inCargo)
        theIndex--;
    if (theIndex >= 0
        && (!this->GetUnitClass()->IsTransport() || (theTransportClass != NULL && theTransportClass->HasRoom(this, inCargo)))) {
        if (inCargo->mHitPoints > 0) {
            long theRings = theTransportClass != NULL ? UMFloatToLong(theTransportClass->mUnloadRange)
                                                   : UMFloatToLong(this->GetStats()->mRange);
            bool theFlag = theTransportClass != NULL ? (theTransportClass->mDomains >> 2) & 1 : false;
            if (EEFindPlacement(&thePoint, inCargo, NULL, this->mTileX, this->GetTileY(), this->GetUnitClass()->GetTileSize(), theRings,
                theSet->Contains(this->GetUnitClass()->GetUnitType()), true, false, theFlag, &theTransformation, NULL)) {
                this->UnloadCargoAt(theIndex, theTransformation);
                theResult = true;
            }
        } else {
            this->ReleaseCargo(inCargo);
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5fd2ba
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

// FUNCTION: EXE 0x5fd2ff
// Removes a unit from this unit's contact list
void EEComplexUnit::RemoveContactUnit(EEComplexUnit* inUnit)
{
    UListNode<EEComplexUnit*>* theNode = this->mContacts.Find(inUnit);
    if (theNode) {
        this->mContacts.Remove(theNode);
        inUnit->Dereference();
    }
}

// FUNCTION: EXE 0x5fd336
// Removes the unit's line of sight at the tile for its player, its sight sharers, the minimap and terrain mesh
// (plus a wonder's extra sight; kind-4 units also clear it for every player)
void EEComplexUnit::SightOff(unsigned long inX, unsigned long inY)
{
    EEPlayer* thePlayer = this->mPlayer;
    GETerrainMesh* theMesh = &::gWorld.GetMap()->mTerrainMesh;
    if (!(this->GetLineOfSight() > 0.0f) || thePlayer == NULL || !thePlayer->mSaveVisMap || inX == (unsigned long)-10000 || !gSightUpdatesEnabled)
        return;
    float theX = (float)(long)inX;
    float theY = (float)(long)inY;
    this->AddCenterOffset(theX, theY);
    if (this->GetUnitClass()->IsLandBuilding() && this->IsFoundation())
        return;
    if (this->mHitPoints > 0 && this->mIsBuilt) {
        EERemoveSightCircle(theX, theY, this->GetLineOfSight(), thePlayer);
        if (this->GetUnitClass()->IsLandBuilding() == true) {
            for (unsigned long i = 0; i < thePlayer->mSightSharers.size(); i++) {
                bool theVisible = ::gWorld.GetCurrentPlayer() == thePlayer->mSightSharers[i] &&
                    ::gWorld.GetCurrentPlayer()->mSharedLOS && (thePlayer->mSightSharers[i])->mSharedLOS;
                if (this->GetUnitClass()->mTileSize <= 1) {
                    EEMapTile* theTile = ::gWorld.GetMap()->GetTile(inX, inY);
                    theTile->RemoveSight((thePlayer->mSightSharers[i]));
                    if (theVisible) {
                        gMiniMap->CountCellOff(inX, inY);
                        theMesh->DereferenceTile(inX, inY);
                    }
                } else {
                    for (long x = inX; x < (long)(inX + this->GetUnitClass()->mTileSize); x++) {
                        for (long y = inY; y < (long)(inY + this->GetUnitClass()->mTileSize); y++) {
                            EEMapTile* theTile = ::gWorld.GetMap()->GetTile(x, y);
                            theTile->RemoveSight((thePlayer->mSightSharers[i]));
                            if (theVisible) {
                                gMiniMap->CountCellOff(x, y);
                                theMesh->DereferenceTile(x, y);
                            }
                        }
                    }
                }
            }
            if (this->GetUnitClass()->mWonderBonus == 3)
                ::gWorld.GetMap()->RemoveSight(thePlayer, theX, theY, gWorldTable.GetFloatValue(kWorldWonderSightRadius), false);
        }
    } else {
        bool theVisible = ::gWorld.GetCurrentPlayer() == thePlayer ||
            (::gWorld.GetCurrentPlayer()->IsAlliedWith(thePlayer) && ::gWorld.GetCurrentPlayer()->mSharedLOS && thePlayer->mSharedLOS);
        if (this->GetUnitClass()->mTileSize <= 1) {
            EEMapTile* theTile = ::gWorld.GetMap()->GetTile(inX, inY);
            thePlayer->RemoveSharedSight(theTile);
            if (theVisible) {
                gMiniMap->CountCellOff(inX, inY);
                theMesh->DereferenceTile(inX, inY);
            }
        } else {
            for (long x = inX; x < (long)(inX + this->GetUnitClass()->mTileSize); x++) {
                for (long y = inY; y < (long)(inY + this->GetUnitClass()->mTileSize); y++) {
                    EEMapTile* theTile = ::gWorld.GetMap()->GetTile(x, y);
                    thePlayer->RemoveSharedSight(theTile);
                    if (theVisible) {
                        gMiniMap->CountCellOff(x, y);
                        theMesh->DereferenceTile(x, y);
                    }
                }
            }
        }
    }
    if (this->GetTechTreeNode() == NULL || this->GetTechTreeNode()->mData->mKind != 4)
        return;
    for (unsigned long i = 1; i < ::gWorld.mNumPlayers; i++) {
        EEPlayer* theOther = ::gWorld.mPlayers[i];
        if (theOther == NULL)
            continue;
        bool theVisible = theOther == ::gWorld.GetCurrentPlayer();
        for (long x = inX; x < (long)(inX + this->GetUnitClass()->mTileSize); x++) {
            for (long y = inY; y < (long)(inY + this->GetUnitClass()->mTileSize); y++) {
                EEMapTile* theTile = ::gWorld.GetMap()->GetTile(x, y);
                theOther->RemoveSharedSight(theTile);
                if (theVisible) {
                    gMiniMap->CountCellOff(x, y);
                    theMesh->DereferenceTile(x, y);
                }
            }
        }
    }
}


// FUNCTION: EXE 0x5fd8f3
// Cancels the building's current research or production, refunding it and clearing the production time
void EEComplexUnit::ToggleJITManufacturing()
{
    if (this->mCurrentResearch == kTechNone)
        return;
    EETechTreeNode* theNode = this->mPlayer->GetTechTree()->Get(this->mCurrentResearch);
    theNode->mIsAllowed = true;
    if (theNode->mData->mUnitClass > 0 && gUnitClassTable[theNode->mData->mUnitClass] != NULL
        && gUnitClassTable[theNode->mData->mUnitClass]->mAIKind == kAIKindHero && this->mPlayer->mNumHeroesInProduction > 0)
        this->mPlayer->mNumHeroesInProduction--;
    this->mPlayer->RefundTechnology(theNode, this->GetUnitClass()->GetUnitType(), 1.0f);
    this->PlayEffect(100000);
    this->SetProductionTime(0);
}

// FUNCTION: EXE 0x5fd984
// Removes a technology from the build queue with a refund (the first match only unless inRemoveAll); if none was
// queued, cancels it when it is the building's current research
void EEComplexUnit::RemoveResearch(long inTechnology, long inRemoveAll)
{
    std::vector<EEProductionEntry> theKept;
    bool theRetained = false;
    bool theKeepNext = true;
    if (this->mBuildQueue.size() > 0) {
        while (this->mBuildQueue.size() > 0) {
            EEProductionEntry theTechnology = this->mBuildQueue.front();
            this->mBuildQueue.erase(this->mBuildQueue.begin());
            if (!EEIsSameResearch(theTechnology.mTechnology, inTechnology) || !theKeepNext)
                theKept.push_back(theTechnology);
            else {
                EETechTreeNode* theNode = this->mPlayer->mTechnologies->Get(theTechnology.mTechnology);
                theNode->mIsAllowed = true;
                long theType = theNode->mData->mUnitClass;
                theKeepNext = (unsigned char)inRemoveAll != 0;
                theRetained = true;
                if (theType > 0 && gUnitClassTable[theType] != NULL
                    && gUnitClassTable[theType]->mAIKind == kAIKindHero && this->mPlayer->mNumHeroesInProduction > 0)
                    --this->mPlayer->mNumHeroesInProduction;
                this->mPlayer->RefundTechnology(this->mPlayer->mTechnologies->Get(theTechnology.mTechnology), this->GetUnitClass()->mUnitType, 1.0f);
            }
        }
        this->mBuildQueue = theKept;
        if (theRetained && (unsigned char)inRemoveAll == 0)
            return;
    }
    if (this->GetUnitClass()->IsLandBuilding() && EEIsSameResearch(this->mCurrentResearch, inTechnology))
        this->ToggleJITManufacturing();
}

// FUNCTION: EXE 0x5fdaf3
// Takes the unit and its parts off the map (deselected, moved to -10000, deactivated) and sets it idle
void EEComplexUnit::TakeOffMap()
{
    this->GetPosition(this->mLastMapPosition);
    this->DropPath();
    this->ClearCollisionState();
    for (EEComplexUnit* theUnit = this; theUnit != NULL; theUnit = theUnit->mNextPart) {
        this->mPlayer->DeselectUnit(theUnit);
        for (unsigned long i = 1; i < ::gWorld.mNumPlayers; i++) {
            EEPlayer* thePlayer = ::gWorld.mPlayers[i];
            if (thePlayer->mViewedUnit == theUnit)
                thePlayer->UpdateSelection(NULL, true);
            if (thePlayer->mSelectedViewedUnit == theUnit)
                thePlayer->SetSelectedViewedUnit(NULL);
        }
        theUnit->GetPosition(theUnit->mLastMapPosition);
        GE3DPoint theAway(-10000.0f, -10000.0f, theUnit->mPhysicalModel.GetLocation().GetZ());
        theUnit->SetLocation(theAway, true);
        ::gWorld.DeactivateUnit(theUnit);
    }
    if (this->mGoalState != NULL
        && (this->mGoalState->mKind != kGoalCastCalamity || EEGoalCast<EEUGCastCalamity>(this->GetGoal())->mCalamityType != 0x77c)) {
        this->StartAction(kActionIdle);
        this->GetUnitClass()->ActionChanged(this);
    }
    this->SetVelocity(0.0f);
    this->ReleaseConversionEffect();
    this->ReleaseHealEffect();
    this->ClearTint();
    this->ReleaseFireEffect();
}

// FUNCTION: EXE 0x5fdc39
// Kills the waypoint marker unit on the given tile
void EEComplexUnit::RemoveWaypointMarker(long inX, long inY)
{
    EEComplexUnit* theUnit = this->FindWaypointMarker(inX, inY);
    if (theUnit)
        theUnit->mPlayer->KillUnit(theUnit, false);
}

// FUNCTION: EXE 0x5fdc58
// Starts the next build-queue entry the built building can produce (a citizen may come out female), dropping and
// refunding unavailable entries
void EEComplexUnit::StartProduction()
{
    float theTimeScale = 1.0f;
    do {
        EEPlayer* thePlayer = this->GetPlayer();
        if (this->mIsBuilt != true)
            return;
        long theTechnology = this->mBuildQueue.front().mTechnology;
        unsigned long theFlags = this->mBuildQueue.front().mFlag;
        EETechTreeNode* theNode = this->GetPlayer()->GetTechTree()->Get(theTechnology);
        if (theNode->GetIsAvailable() == true) {
            bool theAllowed = thePlayer->CanProduce(theTechnology, this->GetUnitClass()->GetUnitType(), 1, false, 2, NULL);
            if (!theAllowed)
                return;
            if (theNode->mData->mMakesUnit) {
                long theType = theNode->mData->mUnitClass;
                EEUnitClass* theClass = gUnitClassTable[theType];
                theTimeScale = this->GetPlayer()->GetUpgrade(theType)->mBuildTimeScale;
                if (theClass->IsAircraft()) {
                    EEAirTrafficControl* theTraffic = this->GetPlayer()->mAirTraffic[theClass->mAirTrafficType - 1];
                    theAllowed = theClass->mWeight <= theTraffic->mMaxCargo - theTraffic->mUsedCargo;
                }
            }
            if (!theAllowed)
                return;
            if (theNode->mData->mMakesUnit == true && gUnitClassTable[theNode->mData->mUnitClass]->mUnitType == kUnitTypeCitizen
                && gPhysics->mRandom.Generate() >= 0.5f) {
                // gUnitClassTable[kUnitTypeFemaleCitizen] is a slot of the class-pointer table, not a separate global
                EEUnitClass* theOther = gUnitClassTable[kUnitTypeFemaleCitizen];
                if (theOther != NULL && this->GetPlayer()->GetTechTree()->Get(theOther->GetTechnology()) != NULL
                    && this->GetPlayer()->GetTechTree()->Get(theOther->GetTechnology())->GetIsAvailable())
                    theTechnology = theOther->GetTechnology();
            }
            this->PlayEffect(theTechnology);
            if (!this->GetPlayer()->mInstantBuild) {
                if (!(theFlags & 1) || !theNode->mData->mMakesUnit)
                    this->mProductionTime = UMFloatToLong((float)theNode->GetBuildTime() * theTimeScale);
                else
                    this->mProductionTime = 2;
            } else
                this->mProductionTime = 1;
            if (theNode->mData->mUnitClass <= 0)
                gUserInterface->UpdateJITManufacturing();
            this->mBuildQueue.erase(this->mBuildQueue.begin());
            return;
        }
        while (!theNode->GetIsAvailable()) {
            // every dropped entry is refunded, the last one too
            this->mBuildQueue.erase(this->mBuildQueue.begin());
            if (theNode->mData->mUnitClass <= 0)
                gUserInterface->UpdateJITManufacturing();
            this->GetPlayer()->RefundTechnology(theNode, this->GetUnitClass()->GetUnitType(), 1.0f);
            if (this->mBuildQueue.empty())
                break;
            theNode = this->GetPlayer()->GetTechTree()->Get(this->mBuildQueue.front().mTechnology);
        }
        if (this->mBuildQueue.empty())
            return;
    } while (true);
}

// FUNCTION: EXE 0x5fdf06
// Returns true if the class uses air traffic control and is not a building
bool EEUnitClass::IsAircraft()
{
    return this->mAirTrafficType != 0 && !this->IsLandBuilding() && this->mCategory != kUnitCategoryNavalBuilding && this->mCategory != kUnitCategorySpaceBuilding;
}

// FUNCTION: EXE 0x5fdf31
// Sets the unit's current goal to the front of its main goal queue and returns it
EEUnitGoal* EEComplexUnit::RefreshGoals()
{
    return this->mGoalState = this->GetQueuedGoal(0);
}

// FUNCTION: EXE 0x5fdf43
// Sets the active goal to the first main-queue goal that is not an auto waypoint
void EEComplexUnit::RefreshActiveGoal()
{
    EEUnitGoal* theResult = NULL;
    std::deque<EEUnitGoal*>& theGoals = this->mGoalQueues[0].mGoals;
    unsigned long theCount = theGoals.size();
    for (unsigned long i = 0; i < theCount; i++) {
        EEUnitGoal* theGoal = theGoals[i];
        if (theGoal->mKind != kGoalLocation || !static_cast<EEUGLocation*>(theGoal)->mIsAutoWaypoint) {
            theResult = theGoal;
            break;
        }
    }
    this->mActiveGoal = theResult;
}

// FUNCTION: EXE 0x5fdfa5
// Switches the unit to an action and updates its camouflage; a dying unit refuses any non-dying action
void EEComplexUnit::StartAction(long inAction)
{
    if (this->mCommand != NULL && !this->GetUnitClass()->IsLandBuilding()) {
        unsigned long theAction = this->GetCommandType();
        if (theAction >= kActionDying && theAction <= kActionKill &&
            ((unsigned long)inAction < kActionDying || (unsigned long)inAction > kActionKill)) {
            return;
        }
    }
    this->UpdateCamouflage(inAction);
    this->mCommand = this->GetUnitClass()->FindAction(inAction);
}

// FUNCTION: EXE 0x5fe02a
// Switches the unit's animation and frame, stops its sound and plays the new frame's sound
void EEComplexUnit::SetAnimation(long inPart, long inFrame, bool inIsAnimationPaused)
{
    EESoundObject* theSoundObject;
    float theVolume;
    float thePan;
    float thePitch;
    // The frame sounds: EEGraphicObject mSounds1/mSounds2 play at the frames mSound1Frames/mSound2Frames
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    EEModel* theModel = this->GetModel();
    this->GetPosition(thePosition);
    long theFrame = inFrame == -1 ? theModel->mStartFrames[inPart] : inFrame;
    if (this->mAnimation == 0xd && inPart == this->mAnimation)
        return;
    if (this->mAnimation == 2 && inPart == 2)
        return;
    if (this->mAnimation == 2 && inPart == 2)  // the original tests this twice
        return;
    this->mAnimation = inPart;
    if (!this->mIsFoundation) {
        this->mPhysicalModel.SetModel(theModel->mModels[inPart]);
        this->mPhysicalModel.SetFrame(theFrame);
    }
    this->mAnimationTime = 0;
    this->mIsAnimationPaused = inIsAnimationPaused;
    this->SetNextFrame(theFrame == (long)theModel->mLastFrames[inPart] ? theModel->mStartFrames[inPart] : theFrame + 1);
    SESMInstanceNode* theSound = this->mSound;
    if (theSound) {
        gSoundManager->Stop(theSound, !gSoundManager->GetIsLooping(theSound));
        this->ReleaseSound();
    }
    if (inIsAnimationPaused)
        return;
    if (this->mPhysicalModel.GetFrame() == theModel->mSound1Frames[inPart])
        theSoundObject = theModel->mSounds1[inPart];
    else if (this->mPhysicalModel.GetFrame() == theModel->mSound2Frames[inPart])
        theSoundObject = theModel->mSounds2[inPart];
    else
        return;
    if (theSoundObject == NULL)
        return;
    this->GetUnitClass()->GetSoundParameters(this, theSoundObject, &theVolume, &thePan, &thePitch);
    this->SetSound(gSoundManager->PlayAndReturnReferencedNode(theSoundObject->GetSampleID(), theVolume, thePan,
                                                        thePitch, theSoundObject->GetLooping(),
                                                        const_cast<char*>((const char*)this->GetUnitClass()->mName)));
    if (this->mSound != NULL)
        this->mSound->Dereference();
}

// FUNCTION: EXE 0x5fe1df
// Sets the unit's mine (reference counted)
void EEComplexUnit::SetMine(EEUnit* inUnit)
{
    if (this->mMine)
        this->mMine->Dereference();
    this->mMine = inUnit;
    if (inUnit)
        inUnit->Reference();
}

// FUNCTION: EXE 0x5fe202
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

// FUNCTION: EXE 0x5fe230
// Marks the unit and its parts built or unbuilt; finishing one updates sight, airports and completion stats
// (a granary replants farms) and pays for it
void EEComplexUnit::SetBuilt(long inIsBuilt, long inUpdateSight, long inLoadButtons)
{
    // NEAR: the arguments are read as bytes (bools in the original; EEUnit.h declares them long)
    EEComplexUnit* theUnit = this;
    do {
        RTSPlayer* theOwner = theUnit->mPlayer;
        EEPlayer* thePlayer = dynamic_cast<EEPlayer*>(theOwner);
        if (inIsBuilt && !theUnit->mIsBuilt) {
            if (theUnit->mUnitClass->IsLandBuilding() && inUpdateSight)
                theUnit->SightOff(theUnit->GetTileX(), theUnit->GetTileY());
            theUnit->mIsBuilt = inIsBuilt != 0;
            if (inUpdateSight)
                theUnit->SightOn(theUnit->GetTileX(), theUnit->GetTileY());
            if (thePlayer) {
                if (theUnit->mUnitClass->mAirTrafficType != 0)
                    thePlayer->mAirTraffic[theUnit->mUnitClass->mAirTrafficType - 1]->AddAirport(theUnit);
                thePlayer->UnitCompleted(theUnit);
                long theType = theUnit->mUnitClass->GetUnitType();
                if (theType == kUnitTypeGranaryImproved || theType == kUnitTypeGranary)
                    gCommunications->ProcessLocalUserInput(new EEMReplantFarms(theUnit->mUnitID), kSlotNone);
            }
            if (inLoadButtons == 1)
                theUnit->mPlayer->LoadUnitClassButtons(theUnit->mUnitClass->GetUnitType());
        } else
            theUnit->mIsBuilt = inIsBuilt != 0;
        if (inIsBuilt)
            theUnit->mIsPaidFor = true;
        theUnit = theUnit->mNextPart;
    } while (theUnit);
}

// FUNCTION: EXE 0x5fe345
// Creates a replant-farms message for the given granary
EEMReplantFarms::EEMReplantFarms(long inFarmID)
    : EEMessage(kMessageReplantFarms, false), mFarmID(inFarmID)
{
}

// FUNCTION: EXE 0x5fe372
// Sets the resource the unit gathers, releasing the old one and sending a gatherer waiting for it
void EEComplexUnit::SetGatheredResource(EEUnit* inUnit)
{
    if (this->mGatheredResource != NULL) {
        ::gWorld.ReleaseResourceUser(this->mGatheredResource);
        if (this->mGatheredResource->mIsValid && this->mCommand != NULL && this->mCommand->mType != kActionWaitForOpenPosition) {
            EEComplexUnit* theUnit = EEGetWaitingGatherer(this->mGatheredResource);
            if (theUnit != NULL) {
                theUnit->StartAction(kActionMoveToResource);
                theUnit->mUnitClass->ActionChanged(theUnit);
            }
        }
        this->mGatheredResource->Dereference();
        this->mGatheredResource = NULL;
    }
    if (inUnit == NULL)
        return;
    this->mGatheredResource = inUnit;
    inUnit->Reference();
    ::gWorld.AddResourceUser(this->mGatheredResource);
}

// FUNCTION: EXE 0x5fe405
// Sets the unit's model, scaled by its class tile size
void EEComplexUnit::SetScaledModel(GEModel* inModel)
{
    this->mPhysicalModel.SetModel(inModel);
    this->mPhysicalModel.SetScale(this->GetUnitClass()->GetTileSize());
}

// FUNCTION: EXE 0x5fe42b
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

// FUNCTION: EXE 0x5fe49a
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

// FUNCTION: EXE 0x5fe536
// Disables or re-enables the unit for a reason, updating its airport and the time-warp tint
void EEComplexUnit::SetDisabled(bool inAway, long inReason)
{
    if (this->mIsDisabled == inAway)
        return;
    long theAirport = this->GetUnitClass()->mAirTrafficType;
    this->mIsDisabled = inAway;
    this->mDisabledReason = inReason;
    if (theAirport != 0 && this->GetUnitClass()->IsAirBase()) {
        if (this->mIsDisabled)
            this->mPlayer->mAirTraffic[theAirport - 1]->RemoveAirport(this);
        else
            this->mPlayer->mAirTraffic[theAirport - 1]->AddAirport(this);
    }
    if (inAway) {
        if (inReason == 0x1a || inReason == 0x1c) {
            this->SetIsTinted(true);
            this->SetColor(kTimeWarpColor);
        }
    } else if (!this->IsCloaked())
        this->SetIsTinted(false);
}

// FUNCTION: EXE 0x5fe5e3
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

// FUNCTION: EXE 0x5fe611
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

// FUNCTION: EXE 0x5fe73b
// Moves the unit and its model to an x/y point
void EEComplexUnit::SetLocationXY(float inX, float inY, bool inUpdateTiles)
{
    this->MoveToLocation(inX, inY, inUpdateTiles);
    this->mPhysicalModel.SetLocationXY(inX, inY);
}

// FUNCTION: EXE 0x5fe76e
// Moves the unit and its model to a 3D point
void EEComplexUnit::SetLocation(const GE3DPoint& inLocation, bool inUpdateTiles)
{
    this->MoveToLocation(inLocation.GetX(), inLocation.GetY(), inUpdateTiles);
    this->mPhysicalModel.SetLocationXYZ(inLocation);
}

// FUNCTION: EXE 0x5fe7a8
// Moves the unit and sets its model's transformation
void EEComplexUnit::SetTransformation(const GETransformation& inTransformation, bool inUpdateTiles)
{
    this->MoveToLocation(inTransformation.GetX(), inTransformation.GetY(), inUpdateTiles);
    this->mPhysicalModel.SetLocation(inTransformation);
}

// FUNCTION: EXE 0x5fe7d9
// Links the unit as the next part of another unit, unlinking it from its previous part
void EEComplexUnit::LinkPart(EEComplexUnit* inUnit)
{
    if (this->mPreviousPart) {
        if (this->mPreviousPart->mNextPart)
            this->mPreviousPart->mNextPart->Dereference();
        this->mPreviousPart->mNextPart = NULL;
        this->mPreviousPart->Dereference();
    }
    this->mPreviousPart = inUnit;
    if (!inUnit)
        return;
    inUnit->Reference();
    this->mPreviousPart->mNextPart = this;
    this->mPreviousPart->mNextPart->Reference();
}

// FUNCTION: EXE 0x5fe83f
// Sets the unit's patrol group (reference counted)
void EEComplexUnit::SetPatrolGroup(EEPPatrolGroup* inGroup)
{
    if (this->mPatrolGroup)
        this->mPatrolGroup->Dereference();
    this->mPatrolGroup = inGroup;
    if (inGroup)
        inGroup->Reference();
}

inline void EEUnitVectorItem::AddFront(EEUnitGoal* inGoal)
{
    this->mGoals.push_front(inGoal);
    inGoal->SetOwner(this->mUnit);
}

// FUNCTION: EXE 0x5fe863
// Puts a goal at the front of a goal queue, replacing the current goal and its leading auto waypoints unless
// inKeepCurrent; on the main queue it becomes the current goal
void EEComplexUnit::SetGoal(EEUnitGoal* inGoal, bool inKeepCurrent, long inIndex)
{
    // inKeepCurrent is a bool (read as a byte)
    if (this->mGoalQueues.size() <= (unsigned long)inIndex)
        return;
    if (this->mGoalQueues[inIndex].mGoals.size() > 0) {
        if (inKeepCurrent) {
            EEUnitGoal* theFront = this->mGoalQueues[inIndex].GetFront();
            theFront->mAction = this->mCommand->mType;
            theFront->SetPathAttempts(0);
        } else {
            if ((unsigned long)inIndex < 1)
                this->mActiveGoal = NULL;
            for (;;) {
                EEUGLocation* theLocation = EEGoalCast<EEUGLocation>(this->GetQueuedGoal(inIndex));
                if (theLocation == NULL || !theLocation->mIsAutoWaypoint)
                    break;
                this->mGoalQueues[inIndex].PopFront();
            }
            if (this->mGoalQueues[inIndex].mGoals.size() > 0)
                this->mGoalQueues[inIndex].PopFront();
        }
    }
    this->mGoalQueues[inIndex].AddFront(inGoal);
    this->SetIsFiringHigh(false);
    if (inIndex != 0)
        return;
    if (inGoal->mKind != kGoalLocation || !static_cast<EEUGLocation*>(inGoal)->mIsAutoWaypoint)
        this->mActiveGoal = inGoal;
    this->mGoalState = inGoal;
}

// FUNCTION: EXE 0x5fe9fb
// Sets the unit's home base (reference counted)
void EEComplexUnit::SetHomeBase(EEComplexUnit* inBase)
{
    if (this->mHomeBase)
        this->mHomeBase->Dereference();
    if (inBase)
        inBase->Reference();
    this->mHomeBase = inBase;
}

// FUNCTION: EXE 0x5fea22
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

// FUNCTION: EXE 0x5fea99
// Sets the building's current research, unloading the old one's media and preloading the new one's (100000 = none)
void EEComplexUnit::PlayEffect(long inTechnology)
{
    if (this->mCurrentResearch != kTechNone) {
        EETechTreeNode* theNode = this->GetPlayer()->GetTechTree()->Get(this->mCurrentResearch);
        if (theNode != NULL && theNode->mData->mUnitClass > 0 && gUnitClassTable[theNode->mData->mUnitClass] != NULL
            && gUnitClassTable[theNode->mData->mUnitClass]->mAIKind == kAIKindHero && this->mPlayer->mNumHeroesInProduction > 0)
            this->mPlayer->mNumHeroesInProduction--;
        if (!gWorldClearing)
            gDataPreLoading->PostUnloadTechnology(this->mPlayer->mPlayerID, this->mCurrentResearch);
        else
            this->GetPlayer()->GetTechTree()->Get(this->mCurrentResearch)->UnloadMedia(this->GetPlayer());
    }
    this->mCurrentResearch = inTechnology;
    if (inTechnology != 100000)
        gDataPreLoading->PostLoadTechnology(this->mPlayer->mPlayerID, inTechnology);
}

// FUNCTION: EXE 0x5feb61
// Returns true when a unit on an auto waypoint has a finished path that is short or ends in a neighbouring sector,
// so it may move on to the next goal
bool EEComplexUnit::ShouldMoveToNextAutoWaypoint()
{
    EEUGLocation* theLocation = EEGoalCast<EEUGLocation>(this->GetGoal());
    if (theLocation == NULL || !theLocation->mIsAutoWaypoint || this->mGoalQueues[0].mGoals.size() <= 1
        || !gPathManager.IsPathDone(this) || gPathManager.HasAutoPath(this))
        return false;
    bool theResult = (long)this->mWaypoints.size() <= gEEAIRegionSize;
    if (theResult || (long)this->mWaypoints.size() <= 0)
        return theResult;
    EEAIPathSector* theFrom = gContinentManager.GetPathSector(this);
    if (theFrom != NULL) {
        EETileCoordinate theTile;
        theTile.mX = UMFloor(this->mWaypoints.back().mLocation.GetX());
        theTile.mY = UMFloor(this->mWaypoints.back().mLocation.GetY());
        EEAIPathSector* theTo = gContinentManager.GetPathSector(theTile);
        if (theTo != NULL) {
            long theLimit;
            if (!(this->GetUnitClass()->mDomains & kMovementSea))
                theLimit = gEEAIRegionSize * 2;
            else {
                theLimit = gEEAIRegionSize;
                if (this->mFormationRank != 0)
                    theLimit = UMCeil(this->mFormationRank / kPathSpeedScale * theLimit);
            }
            if ((long)this->mWaypoints.size() <= theLimit && theFrom->mCenter.DistanceToSquared(theTo->mCenter) < 2)
                theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5fecd8
// Replaces a cargo unit with another, adjusting the cargo room
void EEComplexUnit::ReplaceCargo(EEComplexUnit* inOld, EEComplexUnit* inNew)
{
    std::vector<EEComplexUnit*>::iterator theIterator = std::find(this->mCargo.begin(), this->mCargo.end(), inOld);
    long theCost = inNew->mUnitClass->mWeight;
    if (theIterator == this->mCargo.end())
        return;
    EEComplexUnit* theOld = *theIterator;
    this->mCargoRoom += theOld->mUnitClass->mWeight;
    theOld->Dereference();
    theOld->mContainer->Dereference();
    theOld->mContainer = NULL;
    *theIterator = inNew;
    this->mCargoRoom -= theCost;
    this->mCargoRoom = this->mCargoRoom > 0 ? this->mCargoRoom : 0;
    inNew->Reference();
    if (inNew->mContainer != this) {
        inNew->mContainer = this;
        this->Reference();
    }
}

// FUNCTION: EXE 0x5fed98
// Turns the unit level to face a point
void EEComplexUnit::FaceToward(GE3DPoint inPoint)
{
    GETransformation theTransformation(this->mPhysicalModel.GetLocation());
    float theDY = inPoint.GetY() - theTransformation.GetY();
    float theYaw = UMArcTan2(-(inPoint.GetX() - theTransformation.GetX()), theDY);
    theTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    this->SetTransformation(theTransformation, true);
}

// FUNCTION: EXE 0x5fedfb
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

// FUNCTION: EXE 0x5feeb9
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

// FUNCTION: EXE 0x5fef90
// Stops the unit: clears its build targets, goals and airport queue and sets it idle (aircraft circle)
void EEComplexUnit::Halt()
{
    this->ClearBuildTargets();
    this->ClearGoalQueue(0);
    this->ClearGoals(0);
    if (this->GetUnitClass()->IsAirBase())
        this->mPlayer->mAirTraffic[this->GetUnitClass()->mAirTrafficType - 1]->ClearAirportQueue(this, 0);
    if (this->GetUnitClass()->mCategory == kUnitCategoryAirplane || this->GetUnitClass()->mCategory == kUnitCategoryPlanetSpaceship)
        this->StartAction(kActionCircle);
    else
        this->StartAction(kActionIdle);
    this->GetUnitClass()->ActionChanged(this);
}

// FUNCTION: EXE 0x5fefff
// Returns true if the unit can hit a target from where it stands (melee contact, range, or a ballistic shot from
// any weapon tag point; a legionnaire's special attack uses its heal range)
bool EEComplexUnit::IsInReach(EEUnit* inTarget)
{
    bool theResult = false;
    EEUnitClass* theClass = this->GetUnitClass();
    float theRange = this->GetStats()->GetRange();
    bool theSpecial = false;
    if (theClass->mUnitType == kUnitTypeX04RomanLegionnaire && this->mIsSpecialAttack) {
        theSpecial = true;
        theRange = theClass->mHealRange;
    }
    if (theRange != 0.0f && inTarget->mHitPoints > 0) {
        if (!theClass->mIsMelee || theSpecial) {
            if (theClass->mProjectileClass == NULL ||
                (theClass->mProjectileClass->mCategory != kUnitCategoryProjectile && theClass->mProjectileClass->mCategory != kUnitCategoryBomb))
                theResult = EEIsInRangeToHit(this, inTarget);
            else {
                float theExtraRange = this->mCollisionRadius;
                if (EEGetHitDistanceSquared(this, inTarget, -10000.0f, -10000.0f) <=
                    (theExtraRange + theRange) * (theExtraRange + theRange)) {
                    EETagPointList* theTags = this->GetModel()->mTagPoints[this->mAnimation];
                    unsigned long theCount = theTags != NULL ? theTags->GetNumTagPoints() : 0;
                    unsigned long theFirst = this->mWeaponTagPoint;
                    unsigned long theIndex = theFirst;
                    do {
                        if (this->mWeaponTagPoint != theIndex)
                            this->mWeaponTagPoint = theIndex;
                        theResult = EECanBallisticHit(this, inTarget);
                        if (theCount <= 1)
                            break;
                        theIndex = (theIndex + 1) % theCount;
                    } while (!theResult && theIndex != theFirst);
                } else
                    theResult = false;
            }
        } else {
            float theDistance = EEGetHitDistanceSquared(this, inTarget, -10000.0f, -10000.0f);
            theResult = EEIsInContactRange(this, inTarget, theDistance);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5ff18a
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


// FUNCTION: EXE 0x5ff2e8
// Evaluates the unit's current action and switches to the next one (dropping finished goals); an idle unit stops
// evaluating 350 ticks after it went idle until its trigger time is reset
void EEComplexUnit::AIStep()
{
    unsigned long theAction;
    EEUnitAction* theCommand;
    if (this->mIsCitizen && this->mGoalState != NULL && this->mGoalState->mKind == kGoalGather && !this->mGoalState->IsActive(&theAction)) {
        this->StartAction(theAction);
        this->GetUnitClass()->ActionChanged(this);
        return;
    }
    if (this->mCommand == NULL)
        return;
    if (this->mCommand->mType == kActionIdle && this->mNextTriggerTime < gPhysics->mGameTime)
        return;
    long theResult = this->mCommand->Evaluate(this, theCommand);
    if (theResult != 0) {
        if (theResult == 2) {
            this->DropFinishedGoals(false);
            if (this->mGoalState != NULL) {
                theCommand = NULL;
                theResult = this->mCommand->Evaluate(this, theCommand);
            } else {
                this->StartAction(kActionIdle);
                this->GetUnitClass()->ActionChanged(this);
                theResult = 0;
            }
        }
        if (theResult == 1) {
            this->UpdateCamouflage(theCommand->mType);
            this->mCommand = theCommand;
            this->GetUnitClass()->ActionChanged(this);
        }
    }
    if ((this->mCommand ? this->mCommand->mType : 0) == kActionIdle)
        this->mNextTriggerTime = gPhysics->mGameTime + 350;
}

// FUNCTION: EXE 0x5ff400
// Returns true if a range-1 unit stands on the target's tile or a tile along its sides or corners
bool EEComplexUnit::IsOnTileBorderingTarget(EEUnit* inTarget)
{
    std::vector<U2DPoint<long> > theTiles;
    long theY = this->GetTileY();
    long theX = this->GetTileX();
    long theTargetX = inTarget->GetTileX();
    long theTargetY = inTarget->GetTileY();
    long theSize = inTarget->mUnitClass->mTileSize;
    bool theFound = false;
    if (this->GetStats()->mRange == 1.0f) {
        if (theX == theTargetX && theY == theTargetY)
            theFound = true;
        else {
            EEGetSideTiles(theTargetX, theTargetY, theSize, theTiles);
            std::vector<U2DPoint<long> >::iterator it;
            for (it = theTiles.begin(); it != theTiles.end() && !theFound; it++)
                theFound = theX == it->mX && theY == it->GetY();
            if (!theFound) {
                EEGetCornerTiles(0, theTargetX, theTargetY, theSize, theTiles);
                for (it = theTiles.begin(); it != theTiles.end() && !theFound; it++)
                    theFound = theX == it->mX && theY == it->GetY();
            }
        }
    }
    return theFound;
}

// FUNCTION: EXE 0x5ff4f9
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

// FUNCTION: EXE 0x5ff58b
// Returns the last attacker while it is still attacking this unit (in reach with a clear line, or a ground attack
// covering it); otherwise forgets it and returns NULL
EEComplexUnit* EEComplexUnit::GetConditionTarget()
{
    if (this->mLastAttacker == NULL)
        return NULL;
    EEUGAttackUnit* theAttack = EEGoalCast<EEUGAttackUnit>(this->mLastAttacker->GetGoal());
    if (theAttack != NULL) {
        if (theAttack->mTarget == this && this->mLastAttacker->IsInReach(this)) {
            EEUnit* theArg1;
            long theArg2, theArg3;
            if (this->GetUnitClass()->mShootsHigh || this->GetStats()->mCanFireOverWalls
                || !EEIsLineToUnitBlocked(this->mLastAttacker, this, &theArg1, &theArg2, &theArg3))
                return this->mLastAttacker;
        }
    } else {
        EEUGAttackGround* theGround = EEGoalCast<EEUGAttackGround>(this->mLastAttacker->GetGoal());
        if (theGround != NULL) {
            float theDX = this->mPhysicalModel.GetLocation().GetX() - theGround->mTarget.GetX();
            float theDY = this->mPhysicalModel.GetLocation().GetY() - theGround->mTarget.GetY();
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

// FUNCTION: EXE 0x5ff6b4
// Updates the camouflage power's timers when the unit changes action (idling starts it); without vital buildings
// the unit loses its cloak
void EEComplexUnit::UpdateCamouflage(long inAction)
{
    if (gCivPowersManager.HasPower(this->GetPlayer(), kCivPowerCamouflage) &&
        gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet) != NULL &&
        gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet)->Contains(this->GetUnitClass()->GetUnitType())) {
        if (this->mPlayer->mNumVitalBuildings < 1) {
            bool theCloaked = this->mIsCloakable ? this->mIsCloaked : false;
            if (theCloaked && EEIsCloakedEnemyOf(this, ::gWorld.GetCurrentPlayer()))
                gMiniMap->MoveUnit(this, -10000, -10000, this->GetTileX(), this->GetTileY());
            this->mIsCloakable = false;
            this->mIsCloaked = false;
            this->mCamouflageTimer = 0;
            this->mU32C = 0;
        } else if (this->GetCommandType() == kActionIdle && inAction != 5000) {
            this->mCamouflageTimer = 0;
            this->mU32C = gWorldTable.GetLongValue2(kWorldCamouflageTiming);
        } else if (this->GetCommandType() != kActionIdle && inAction == 5000) {
            this->mCamouflageTimer = gWorldTable.GetLongValue(kWorldCamouflageTiming);
            this->mU32C = 0;
        }
    } else {
        this->mCamouflageTimer = 0;
        this->mU32C = 0;
    }
}

// FUNCTION: EXE 0x5ff7f8
// Moves the unit's area effects to its position and updates them for the elapsed time
void EEComplexUnit::UpdateAreaEffects(unsigned long inValue)
{
    GE2DPoint thePoint(this->mPhysicalModel.GetLocation().GetX(), this->mPhysicalModel.GetLocation().GetY());
    this->mAreaEffects.MoveAndUpdate(thePoint, (float)inValue);
}

// FUNCTION: EXE 0x5ff834
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


// FUNCTION: EXE 0x5ff928
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

// FUNCTION: EXE 0x5ffbc2
// Replaces a technology in the current research and the build queue with another (100000 removes it)
void EEComplexUnit::ReplaceResearch(long inTechnology, long inNewTechnology)
{
    if (this->mCurrentResearch == inTechnology) {
        this->PlayEffect(inNewTechnology);
        if (inNewTechnology == 100000)
            this->SetProductionTime(0);
        else {
            EETechTreeNode* theNode = this->mPlayer->GetTechTree()->Get(inNewTechnology);
            if (theNode != NULL && theNode->mData->mUnitClass > 0 &&
                gUnitClassTable[theNode->mData->mUnitClass] != NULL &&
                gUnitClassTable[theNode->mData->mUnitClass]->mAIKind == kAIKindHero)
                this->mPlayer->mNumHeroesInProduction++;
        }
    }
    if (this->mBuildQueue.size() <= 0)
        return;
    for (long i = this->mBuildQueue.size() - 1; i >= 0; i--) {
        if (this->mBuildQueue[i].mTechnology != inTechnology)
            continue;
        if (inNewTechnology != 100000)
            this->mBuildQueue[i].mTechnology = inNewTechnology;
        else
            this->mBuildQueue.erase(this->mBuildQueue.begin() + i);
    }
}

// FUNCTION: EXE 0x5ffcb2
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

// FUNCTION: EXE 0x5ffd23
// Destroys an area-effect reference container (UReferenceContainer<EEAreaEffect*> destructor)

// SYNTHETIC: EXE 0x5ffd44
// (the scalar deleting destructor, emitted with the vtable; slot EEUnitAreaEffectList+0x0, slot UReferenceContainer<EEAreaEffect *>+0x0)

// TEMPLATE: EXE 0x5ffd60
// ??4?$UReferenceContainer@PAVEEAreaEffect@@@@QAEXABV0@@Z

// TEMPLATE: EXE 0x5ffda1

// TEMPLATE: EXE 0x5ffe45

// FUNCTION: EXE 0x5ffe7f
// Assigns one waypoint deque to another (deque<EEComplexUnitWaypoint>::operator=)

// FUNCTION: EXE 0x5fff23
// Returns a waypoint by index (deque<EEComplexUnitWaypoint>::operator[])

// FUNCTION: EXE 0x5fff45
// Inserts a waypoint at the front of a waypoint deque (push_front)

// TEMPLATE: EXE 0x5fffd6

// FUNCTION: EXE 0x600002
// Resizes the unit's goal-queue vector (vector<EEUnitVectorItem>::resize)

// TEMPLATE: EXE 0x6000be

// FUNCTION: EXE 0x6000f9
// Inserts an attacker record at the front of an attacker deque (push_front)

// FUNCTION: EXE 0x60018a
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

// TEMPLATE: EXE 0x6001b5

// TEMPLATE: EXE 0x60025e

// FUNCTION: EXE 0x600292
// Returns the number of wheels on the graphic object
// Declared out of line in ee/EEGraphicObject.h like GetOrbitalCount; its only out-of-line copy follows this
// unit's template block (provisional name)
unsigned long EEGraphicObject::GetWheelCount() const
{
    return this->mWheels.size();
}

// SYNTHETIC: EXE 0x6002a5
// (the scalar deleting destructor, emitted with the vtable; slot UMemoryPool<EEComplexUnit>+0x0, stores UMemoryPool<EEComplexUnit> vtable)

// TEMPLATE: EXE 0x6002c7

// FUNCTION: EXE 0x6002ec
// Returns an iterator to the first unit of a deque<EEComplexUnit*> (begin)
// Not emitted by ours (0xf bytes): the original calls this begin out of line, ours expands it (inline budget, depth)

// TEMPLATE: EXE 0x6002fb

// TEMPLATE: EXE 0x6003a4

// FUNCTION: EXE 0x6003bd
// Erases a range of waypoints from a waypoint deque (erase)

// FUNCTION: EXE 0x600466
// Inserts copies of a goal queue into the goal-queue vector (vector<EEUnitVectorItem>::insert)

// TEMPLATE: EXE 0x600740

// TEMPLATE: EXE 0x6007ab

// TEMPLATE: EXE 0x6007d7

// TEMPLATE: EXE 0x600806

// TEMPLATE: EXE 0x600914
// ?push_back@?$vector@PAVEEAreaEffect@@V?$allocator@PAVEEAreaEffect@@@std@@@std@@QAEXABQAVEEAreaEffect@@@Z

// FUNCTION: EXE 0x600940
// Copy-constructs a run of goal queues in raw storage (vector<EEUnitVectorItem>::_Ufill)

// TEMPLATE: EXE 0x60096b

// TEMPLATE: EXE 0x6009d8

// TEMPLATE: EXE 0x600a3b
// (UMemoryPool<EEComplexUnit>::GetUsed, template COMDAT: the leak dumps)

// TEMPLATE: EXE 0x600a61

// TEMPLATE: EXE 0x600b4c

// TEMPLATE: EXE 0x600c59

// TEMPLATE: EXE 0x600d66

// TEMPLATE: EXE 0x600d8b

// TEMPLATE: EXE 0x600dbf

// TEMPLATE: EXE 0x600df9

// TEMPLATE: EXE 0x600eb6

// TEMPLATE: EXE 0x600ef2

// TEMPLATE: EXE 0x600f35

// TEMPLATE: EXE 0x600f78

// FUNCTION: EXE 0x600fbb
// Copies a range of waypoints backward (std::copy_backward on deque<EEComplexUnitWaypoint>)

// FUNCTION: EXE 0x600ffe
// Copies a range of waypoints (std::copy on deque<EEComplexUnitWaypoint>)

// FUNCTION: EXE 0x601041
// Copy-constructs goal queues from a range into raw storage (vector<EEUnitVectorItem>::_Ucopy)

// FUNCTION: EXE 0x60106d
// Assigns a goal queue to every element of a range (std::fill on EEUnitVectorItem)

// FUNCTION: EXE 0x60108a
// Copies goal queues backward (std::copy_backward on EEUnitVectorItem)

// FUNCTION: EXE 0x6010b6
// Copies goal queues (std::copy on EEUnitVectorItem)

// FUNCTION: EXE 0x6010da
// Copy-constructs a goal queue in place (std::_Construct on EEUnitVectorItem)

// TEMPLATE: EXE 0x6010ec

// SYNTHETIC: EXE 0x6010fd
// SYNTHETIC: EXE 0x601107
// The chain of UList<EEComplexUnit*>::sNodePool (object 0x96700c): wrapper and registrar; its ctor and dtor are the
// copies at 0x54624d / 0x5462a6.

// TEMPLATE: EXE 0x601113

// TEMPLATE: EXE 0x601199

// TEMPLATE: EXE 0x6011b3
// (UMemoryPoolChunk<EEComplexUnit>::GetUsed, template COMDAT: the leak dumps)

// TEMPLATE: EXE 0x6012c9

// FUNCTION: EXE 0x601335
// Destroys a complex unit (or an array of them) and frees the memory (vector deleting destructor)

// TEMPLATE: EXE 0x601396

// TEMPLATE: EXE 0x601409

// TEMPLATE: EXE 0x601468

// TEMPLATE: EXE 0x601551

// TEMPLATE: EXE 0x60157a

// TEMPLATE: EXE 0x60164d

// FUNCTION: EXE 0x60167e
// Inserts a range of waypoints into a waypoint deque (deque<EEComplexUnitWaypoint>::_Insert)

// TEMPLATE: EXE 0x6018e2

// TEMPLATE: EXE 0x601955

// TEMPLATE: EXE 0x6019f5

// TEMPLATE: EXE 0x601a2f

// TEMPLATE: EXE 0x601a70

// TEMPLATE: EXE 0x601ba3

// TEMPLATE: EXE 0x601bd3

// TEMPLATE: EXE 0x601c17

// TEMPLATE: EXE 0x601c40

// TEMPLATE: EXE 0x601cae

// TEMPLATE: EXE 0x601d01

// TEMPLATE: EXE 0x601d51

// ICF-FOLDED: EXE 0x557269
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEComplexUnit::DescribeLong(class UWideString &)
{
}

// ICF-FOLDED: EXE 0x4e123d
// (no address of its own: folded into the shared `ret` body of EEE2DMarchingIcons::PrepareQueue)
void EEComplexUnit::FnE123D()
{
}

// ICF-FOLDED: EXE 0x557269
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEComplexUnit::ResearchCompleted(unsigned long)
{
}
