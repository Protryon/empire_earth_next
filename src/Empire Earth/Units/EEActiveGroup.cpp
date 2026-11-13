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
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEGroupCluster.h"
#include "ee/EEActiveGroup.h"
#include "ee/EEUGLocation.h"
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
#include "ee/EEUGAttackUnit.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUGAttackGround.h"
#include "ee/EEUGRust.h"
#include "ee/EEUGRust.h"
#include "ee/EEPPatrolGroup.h"
#include "lle/U2DSparseArray.h"
#include "ee/EEUGHealUnit.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUnitVectorItem.h"
#include "lle/U2DRectangle.h"
#include "ee/EEFormation.h"
#include "ee/EEUnitUtilities.h"
#include "rust/ee_units.h"   // ee_units_keeps_ground_attacker

// The order Fill sorts a slot's units in (RTTI EEAGIncreasingVelocityUnitSorter; vtable 0x446d50: only its
// user-declared empty virtual dtor, whose ??_G 0x1edaa8 follows Fill, its first user): slower first, the same
// stats by hit points. Defined here like EESelectedGroup.cpp's EESGUnitSorter (ee/EEAGIncreasingVelocityUnitSorter.h is
// an unused draft) (provisional names)
class EEAGIncreasingVelocityUnitSorter {
public:
    virtual ~EEAGIncreasingVelocityUnitSorter() {}
    bool operator()(EEComplexUnit* const& inFirst, EEComplexUnit* const& inSecond) const;   // 0x1ebc47
};

// LayOutFormation's order for the units it places on the nearest free slots (a 1-byte predicate holding a bool,
// built as `theNear >= size / 2`; its operator() is the out-of-line 0x1f109a among the sort COMDATs): the slower
// class first, then the larger inner collision sphere, then the index (ascending or descending) (provisional names)
struct EEAGSlotUnitLess {
    EEAGSlotUnitLess(bool inAscending) : mAscending(inAscending) {}
    bool operator()(const std::pair<unsigned long, EEComplexUnit*>& inFirst,
                    const std::pair<unsigned long, EEComplexUnit*>& inSecond) const
    {
        EEUnitClass* theFirst = inFirst.second->mUnitClass;
        EEUnitClass* theSecond = inSecond.second->mUnitClass;
        bool theResult;
        if (theFirst->mMaxVelocity < theSecond->mMaxVelocity)
            theResult = true;
        else if (theFirst->mMaxVelocity != theSecond->mMaxVelocity)
            theResult = false;
        else if (theFirst->GetInnerCollisionSphere() == theSecond->GetInnerCollisionSphere()) {
            if (this->mAscending == true)
                theResult = inFirst.first < inSecond.first;
            else
                theResult = inFirst.first > inSecond.first;
        } else if (theFirst->GetInnerCollisionSphere() > theSecond->GetInnerCollisionSphere())
            theResult = true;
        else
            theResult = false;
        return theResult;
    }
    bool mAscending;
};

// LayOutFormation's order for the slots and units it matches along the move direction: the smaller weight first (an
// empty predicate, its operator() expanded inline by _Unguarded_insert 0x1f1527) (provisional name)
struct EEAGSecondLess {
    bool operator()(const std::pair<long, float>& inFirst, const std::pair<long, float>& inSecond) const
        { return inFirst.second < inSecond.second; }
};

// FUNCTION: EXE 0x5ebc47
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

// FUNCTION: EXE 0x5ebc86
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

// FUNCTION: EXE 0x5ebcfd
// Adds a unit to the group without taking it out of its old active group
void EEActiveGroup::Add(EEComplexUnit* inUnit)
{
    this->AddMember(inUnit, false);
}

// SYNTHETIC: EXE 0x5ebd0b
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x5ebd27
// Destroys the group, releasing its units and its goal and emptying mU48 and mU58
EEActiveGroup::~EEActiveGroup()
{
    this->Clear();
    this->ClearGoal();
    this->mU48.clear();
    this->mU58.clear();
}

// FUNCTION: EXE 0x5ebd73
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

// FUNCTION: EXE 0x5ebdcc
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

// FUNCTION: EXE 0x5ebdf2
// Requests paths to the goal for all of the group's units
void EEActiveGroup::RequestAllPaths()
{
    this->RequestPaths(this->mUnits);
}

// FUNCTION: EXE 0x5ebdfc
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

static const float kFullSlotSize = 1.0f;          // 0x446ce8 (LayOutFormation only; provisional name)
static const float kSlowSpeed = 0.5f;             // 0x446cec (LayOutFormation only; provisional name)
static const float kQuarterTurn = 1.5707964f;     // 0x446cf0 (LayOutFormation only; provisional name)

// FUNCTION: EXE 0x5ebf85
// Arranges the formation on the goal, gives every unit a move or unload goal at its slot and requests the paths
// The formation's placement: the aircraft held in
// a container get the goal directly; the formation is centred on the goal (yawed by the location goal's yaw, or
// towards the unload destination) and arranged; the units then take the slots, either the nearest free slot in the
// EEAGSlotUnitLess order (the goal is near the units) or by their order across the move direction; every unit gets
// a goal at its slot.
void EEActiveGroup::LayOutFormation()
{
    std::vector<EEFormationSlot> theSlots;
    std::vector<std::pair<long, float> > theSlotOrder;
    std::vector<std::pair<long, float> > theUnitOrder;
    std::vector<EEComplexUnit*> theQueued;
    std::vector<EEComplexUnit*> theReplaced;
    U2DRectangle<float> theBounds;
    EEFormation* theFormation = this->mFormation;
    EEUnitGoal* theGoal = NULL;
    U2DPoint<long> theDestination;
    U2DPoint<long> theFrom;
    GE3DPoint theSlotLocation;
    GE3DPoint theLocation;
    GE3DPoint theUnitLocation;
    U2DPoint<float> theTarget;
    U2DPoint<float> theClosest;
    U2DPoint<float> theCenter;
    float theSlotX = 0.0f;
    float theSlotY = 0.0f;
    float theSize = 0.0f;
    float theYaw = 0.0f;
    long theFree = 0;
    unsigned long theNear = 0;
    bool theHasYaw = false;
    bool theUnloads = false;
    this->mGoal->GetGoalLocation(theLocation);
    EEMapTile* theTile = ::gWorld.mMap->GetTile(UMFloorUnsigned(theLocation.GetX()), UMFloorUnsigned(theLocation.GetY()));
    switch (this->mGoal->mKind) {
    case kGoalLocation: {
        EEUGLocation* theLocationGoal = EEGoalCast<EEUGLocation>(this->mGoal);
        unsigned long theCount = this->GetNumUnits();
        for (unsigned long i = 0; i < theCount; i++) {
            EEComplexUnit* theUnit = this->GetUnit(i);
            long theCategory = theUnit->GetUnitClass()->GetCategory();
            if ((theCategory != kUnitCategoryAirplane && theCategory != kUnitCategoryPlanetSpaceship) || theUnit->mContainer == NULL)
                continue;
            if (this->mIsQueued && theUnit->GetGoal() != NULL) {
                theUnit->mGoalQueues[0].Add(new EEUGLocation(&theLocation, theLocationGoal->GetB1C(), theLocationGoal->GetIsForcedMove(),
                                                       false, 0.0f, false, 0));
                theUnit->RefreshGoals();
                theUnit->RefreshActiveGoal();
                theUnit->mGoalQueues[1].Add(new EEUGLocation(&theLocation, theLocationGoal->GetB1C(), theLocationGoal->GetIsForcedMove(),
                                                       false, 0.0f, false, 0));
                theUnit->RefreshGoals();
                theUnit->RefreshActiveGoal();
            } else {
                theUnit->ClearGoalQueue(0);
                theUnit->ClearGoalQueue(1);
                theUnit->mGoalQueues[0].Add(new EEUGLocation(&theLocation, theLocationGoal->GetB1C(), theLocationGoal->GetIsForcedMove(),
                                                       false, 0.0f, false, 0));
                theUnit->RefreshGoals();
                theUnit->RefreshActiveGoal();
                theUnit->mGoalQueues[1].Add(new EEUGLocation(&theLocation, theLocationGoal->GetB1C(), theLocationGoal->GetIsForcedMove(),
                                                       false, 0.0f, false, 0));
                theUnit->RefreshGoals();
                theUnit->RefreshActiveGoal();
            }
            theUnit->mGoalQueues[1].AddTime(1);
            this->RemoveUnit(theUnit);
            theCount = this->GetNumUnits();
            i--;
        }
        break;
    }
    case kGoalUnloadTransport: {
        unsigned long theCount = this->GetNumUnits();
        for (unsigned long i = 0; i < theCount; i++) {
            EEComplexUnit* theUnit = this->GetUnit(i);
            long theCategory = theUnit->GetUnitClass()->GetCategory();
            if ((theCategory != kUnitCategoryAirplane && theCategory != kUnitCategoryPlanetSpaceship) || theUnit->mContainer == NULL)
                continue;
            if (this->mIsQueued && theUnit->GetGoal() != NULL) {
                theUnit->QueueGoal(new EEUGLocation(&theLocation, this->mGoal->GetB1C(), true, false, 0.0f, false, 0), 0);
                theUnit->QueueGoal(new EEUGLocation(&theLocation, this->mGoal->GetB1C(), true, false, 0.0f, false, 0), 1);
            } else {
                theUnit->ClearGoalQueue(0);
                theUnit->ClearGoalQueue(1);
                theUnit->QueueGoal(new EEUGLocation(&theLocation, this->mGoal->GetB1C(), true, false, 0.0f, false, 0), 0);
                theUnit->QueueGoal(new EEUGLocation(&theLocation, this->mGoal->GetB1C(), true, false, 0.0f, false, 0), 1);
            }
            theUnit->mGoalQueues[1].AddTime(1);
            this->RemoveUnit(theUnit);
            theCount = this->GetNumUnits();
            i--;
        }
        break;
    }
    }
    if (this->GetNumUnits() == 0)
        return;
    switch (this->mGoal->mKind) {
    case kGoalLocation:
        theHasYaw = true;
        theYaw = EEGoalCast<EEUGLocation>(this->mGoal)->GetYaw();
        break;
    case kGoalUnloadTransport: {
        EEUGRust* theUnload = EEUnloadGoalCast(this->mGoal);
        if (this->GetNumUnits() > 0) {
            theUnload->GetUnloadTransport(&theDestination.mX, &theDestination.mY);
            theHasYaw = true;
            EEComplexUnit* theFirst = this->GetUnit(0);
            theFrom.mX = theFirst->GetTileX();
            theFrom.mY = theFirst->GetTileY();
            theYaw = theFrom.GetHeadingTo(theDestination);
        }
        break;
    }
    default:
        theHasYaw = false;
        theYaw = 0.0f;
    }
    theFormation->mCenter = theLocation;
    if (theHasYaw) {
        theFormation->mHeading = theYaw;
        theFormation->mOrientation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    }
    theFormation->Arrange(this, theSlots);
    unsigned long theNumSlots = theSlots.size();
    unsigned long i;
    i = 0;
    while (i < theNumSlots) {
        theSlotX += theSlots[i].mPosition.GetX();
        theSlotY += theSlots[i].mPosition.GetY();
        i++;
    }
    float theSlotCount = (float)theNumSlots;
    theSlotX /= theSlotCount;
    theSlotY /= theSlotCount;
    unsigned long theNumUnits = this->GetNumUnits();
    for (i = 0; i < theNumUnits; i++) {
        EEComplexUnit* theUnit = this->GetUnit(i);
        if (i != 0)
            theBounds.Extend(U2DPoint<float>(theUnit->mPhysicalModel.GetLocation().GetX(),
                                             theUnit->mPhysicalModel.GetLocation().GetY()));
        else {
            theBounds.mTop = theUnit->mPhysicalModel.GetLocation().GetY();
            theBounds.mLeft = theUnit->mPhysicalModel.GetLocation().GetX();
            theBounds.mBottom = theUnit->mPhysicalModel.GetLocation().GetY();
            theBounds.mRight = theUnit->mPhysicalModel.GetLocation().GetX();
        }
        if (theUnit->GetUnitClass()->GetInnerCollisionSphere() >= theSize)
            theSize = theUnit->GetUnitClass()->GetInnerCollisionSphere();
        theCenter.mX += theUnit->mPhysicalModel.GetLocation().GetX();
        theCenter.mY += theUnit->mPhysicalModel.GetLocation().GetY();
    }
    float theSpeed = this->mSpeed;
    theCenter.mX = theCenter.mX / (float)this->GetNumUnits();
    theCenter.mY = theCenter.GetY() / (float)this->GetNumUnits();
    if (!this->mMovesTogether || theSize >= kFullSlotSize || theSpeed <= kSlowSpeed) {
        theTarget.mX = theLocation.mX;
        theTarget.mY = theLocation.GetY();
        theClosest = theBounds.GetClosestPoint(theTarget);
        float theDistance = theClosest.DistanceToSquared(theTarget);
        theSize = max(theBounds.mBottom - theBounds.mTop, theBounds.mRight - theBounds.mLeft);
        if (!theBounds.Contains(theTarget) && theDistance > theSize * theSize) {
            // far from the units: slots and units both in their order across the move direction
            std::vector<float> theWeights;
            float theDX = theSlotX - theCenter.mX;
            float theDY = theCenter.GetY() - theSlotY;
            // both components are normalised by the same (original) |dx| + |dy|
            float theSum = UMAbs(theDX) + UMAbs(theDY);
            theDX = theDX / theSum;
            theDY = theDY / theSum;
            float theMin = 3.402823466e+38F;
            float theMax = 1.175494351e-38F;
            float theAcrossY = theDY / 10.0f;
            float theAcrossX = theDX / -10.0f;
            unsigned long theCount = theSlots.size();
            theSlotOrder.reserve(theCount);
            theWeights.reserve(theCount);
            for (i = 0; i < theCount; i++) {
                float theX = theSlots[i].mPosition.GetX();
                float theAlong = UMAbs(theX * theDY + theSlots[i].mPosition.GetY() * theDX);
                theX = theSlots[i].mPosition.GetX();
                float theAcross = UMAbs(theX * theAcrossX + theSlots[i].mPosition.GetY() * theAcrossY);
                theSlotOrder.push_back(std::pair<long, float>(i, theAcross + theAlong));
                theWeights.push_back(theAcross);
                if (theAcross <= theMin)
                    theMin = theAcross;
                if (theMax <= theAcross)
                    theMax = theAcross;
            }
            std::sort(theSlotOrder.begin(), theSlotOrder.end(), EEAGSecondLess());
            theMin = 3.402823466e+38F;
            theMax = 1.175494351e-38F;
            theWeights.clear();
            theCount = this->GetNumUnits();
            theUnitOrder.reserve(theCount);
            for (unsigned long j = 0; j < theCount; j++) {
                float theX = this->GetUnit(j)->mPhysicalModel.GetLocation().GetX();
                float theAlong = UMAbs(theX * theDY + this->GetUnit(j)->mPhysicalModel.GetLocation().GetY() * theDX);
                theX = this->GetUnit(j)->mPhysicalModel.GetLocation().GetX();
                float theAcross = UMAbs(theX * theAcrossX
                                        + this->GetUnit(j)->mPhysicalModel.GetLocation().GetY() * theAcrossY);
                theUnitOrder.push_back(std::pair<long, float>(j, theAcross + theAlong));
                theWeights.push_back(theAcross);
                if (theAcross <= theMin)
                    theMin = theAcross;
                if (theMax <= theAcross)
                    theMax = theAcross;
            }
            std::sort(theUnitOrder.begin(), theUnitOrder.end(), EEAGSecondLess());
            theCount = theSlotOrder.size();
            for (unsigned long k = 0; k < theCount; k++)
                theSlots[theSlotOrder[k].first].mUnit = this->GetUnit(theUnitOrder[k].first);
        } else {
            // near the units: each unit (in EEAGSlotUnitLess order) takes the nearest free slot
            std::deque<long> theFreeSlots;
            std::vector<std::pair<unsigned long, EEComplexUnit*> > theOrder;
            GE3DPoint thePosition;
            U2DPoint<long> theGoalTile(UMFloatToLong(theLocation.GetX()), UMFloatToLong(theLocation.GetY()));
            theOrder.resize(this->mUnits.size(), std::pair<unsigned long, EEComplexUnit*>(0, 0));
            for (long u = this->mUnits.size() - 1; u >= 0; u--) {
                theOrder[u].first = u;
                theOrder[u].second = this->mUnits[u];
                this->mUnits[u]->GetPosition(thePosition);
                if (EEGetAngleFromNorth(&thePosition, &theLocation) <= kQuarterTurn)
                    theNear++;
            }
            std::sort(theOrder.begin(), theOrder.end(), EEAGSlotUnitLess(theNear >= this->mUnits.size() / 2));
            long theSlotCount = theSlots.size();
            theFreeSlots.resize(theSlotCount, 0);
            for (long s = 0; s < theSlotCount; s++)
                theFreeSlots[s] = s;
            unsigned long theCount = theOrder.size();
            for (unsigned long k = 0; k < theCount; k++) {
                long theBest = -1;
                float theBestDistance = 3.402823466e+38F;
                theOrder[k].second->GetPosition(theUnitLocation);
                for (long f = theFreeSlots.size() - 1; f >= 0; f--) {
                    float theSquared = theUnitLocation.DistanceToSquared(theSlots[theFreeSlots[f]].mPosition);
                    if (theSquared < theBestDistance || theBest == -1) {
                        theBestDistance = theSquared;
                        theBest = f;
                    }
                }
                theSlots[theFreeSlots[theBest]].mUnit = theOrder[k].second;
                std::deque<long>::iterator theIt = theFreeSlots.begin() + theBest;
                theFreeSlots.erase(theIt, theIt + 1);
            }
        }
    }
    if (this->mIsQueued == true)
        theQueued.reserve(theNumSlots);
    for (i = 0; i < theNumSlots; i++) {
        theSlotLocation = theSlots[i].mPosition;
        EEComplexUnit* theUnit = theSlots[i].mUnit;
        if (theUnit == NULL)
            continue;
        if (theUnit->mFormationSlot != i)
            theUnit->mFormationSlot = i;
        theUnit->mFormationRank = (long)((float)i / theSlotCount * 4.0f);
        if (theUnit->GetGoal() == NULL)
            theFree++;
        switch (this->mGoal->mKind) {
        case kGoalLocation: {
            EEUGLocation* theLocationGoal = EEGoalCast<EEUGLocation>(this->mGoal);
            theGoal = new EEUGLocation(&theSlotLocation, theLocationGoal->GetB1C(), theLocationGoal->GetIsForcedMove(),
                                       theLocationGoal->GetShouldFace(), theSlots[i].mHeading, false, 1);
            break;
        }
        case kGoalUnloadTransport:
            if ((theUnit->GetUnitClass()->IsTransport() || theUnit->GetUnitClass()->mIsParatrooperBomber)
                && theUnit->GetComplexUnit()->GetNumCargoUnits() > 0) {
                theGoal = EEUGRust::NewUnloadTransport(theUnit, UMFloor(theSlotLocation.GetX()),
                                                  UMFloor(theSlotLocation.GetY()), true);
                if (EEUnloadGoalCast(theGoal)->CanUnloadAtDestination())
                    theUnloads = true;
                else {
                    delete theGoal;
                    theGoal = new EEUGLocation(&theSlotLocation, false, true, false, 0.0f, false, 0);
                }
            } else
                theGoal = new EEUGLocation(&theSlotLocation, false, true, false, 0.0f, false, 0);
            break;
        }
        if (!this->mIsQueued || theUnit->GetGoal() == NULL) {
            theGoal->mB1D = true;
            if (this->mIsQueued)
                theQueued.push_back(theUnit);
        } else if (theGoal->mKind == kGoalLocation)
            EEGoalCast<EEUGLocation>(theGoal)->SetGroupPathState(0);
        long theClassType = theUnit->GetUnitClass()->mAIKind;
        if (theClassType == kAIKindAircraftCarrier || theClassType == kAIKindSpaceCarrier) {
            theGoal->GetGoalLocation(theLocation);
            bool theIdle = false;
            if (this->mIsQueued) {
                if (theTile == NULL || theTile->GetHeight() < 0.0f || theTile->mTerrainType == kTerrainSpace) {
                    if (theUnit->GetQueuedGoal(0) == NULL)
                        theIdle = true;
                    theUnit->QueueGoal(theGoal, 0);
                    if (theIdle) {
                        theUnit->StartAction(kActionPrepareToMove);
                        theUnit->GetUnitClass()->ActionChanged(theUnit);
                    }
                } else {
                    if (theUnit->GetQueuedGoal(1) == NULL)
                        theIdle = true;
                    theUnit->QueueGoal(theGoal, 1);
                    if (theIdle) {
                        theUnit->StartAction(kActionAirportOpen);
                        theUnit->GetUnitClass()->ActionChanged(theUnit);
                    }
                }
            } else if (theTile == NULL || theTile->GetHeight() < 0.0f || theTile->mTerrainType == kTerrainSpace) {
                theUnit->ClearGoals(0);
                theUnit->ClearGoalQueue(0);
                theUnit->SetGoal(theGoal, 0, 0);
                theUnit->StartAction(kActionPrepareToMove);
                theUnit->GetUnitClass()->ActionChanged(theUnit);
            } else {
                theUnit->ClearGoalQueue(1);
                theUnit->SetGoal(theGoal, 0, 1);
                theUnit->StartAction(kActionAirportOpen);
                theUnit->GetUnitClass()->ActionChanged(theUnit);
            }
        } else {
            unsigned long theUnitType = theUnit->GetUnitClass()->GetUnitType();
            if (theUnitType == kUnitTypeAirport) {
                if (this->mIsQueued)
                    theUnit->QueueSquadronGoals(theGoal, false);
                else {
                    theUnit->SetSquadronGoals(theGoal, false);
                    theUnit->StartAction(kActionAirportOpen);
                    theUnit->GetUnitClass()->ActionChanged(theUnit);
                }
                theReplaced.push_back(theUnit);
            } else if (theUnitType == kUnitTypeNavyYard) {
                if (this->mIsQueued) {
                    if (theUnit->GetGoal() != NULL)
                        theUnit->QueueGoal(theGoal, 0);
                    else {
                        theUnit->ClearGoals(0);
                        theUnit->ClearGoalQueue(0);
                        theUnit->SetGoal(theGoal, 0, 0);
                    }
                    if (theUnit->GetQueuedGoal(2) != NULL)
                        theUnit->QueueGoal(theGoal->Clone(), 2);
                    else
                        theUnit->SetGoal(theGoal->Clone(), 0, 2);
                } else {
                    theUnit->ClearGoals(0);
                    theUnit->ClearGoalQueue(0);
                    theUnit->SetGoal(theGoal, 0, 0);
                    theUnit->ClearGoals(2);
                    theUnit->ClearGoalQueue(2);
                    theUnit->SetGoal(theGoal->Clone(), 0, 2);
                    theUnit->StartAction(kActionAirportOpen);
                    theUnit->GetUnitClass()->ActionChanged(theUnit);
                }
                theReplaced.push_back(theUnit);
            } else if (this->mIsQueued && theUnit->GetGoal() != NULL)
                theUnit->QueueGoal(theGoal, 0);
            else {
                theUnit->ClearGoals(0);
                theUnit->ClearGoalQueue(0);
                theUnit->SetGoal(theGoal, 0, 0);
                long theCategory = theUnit->GetUnitClass()->GetCategory();
                if ((theCategory == kUnitCategoryAirplane || theCategory == kUnitCategoryPlanetSpaceship)) {
                    theSlotLocation.SetZ(theUnit->GetUnitClass()->GetCruiseAltitude() + theSlotLocation.GetZ());
                    if (theUnit->GetCommandType() == kActionIdle) {
                        theUnit->StartAction(kActionTakeoff);
                        theUnit->GetUnitClass()->ActionChanged(theUnit);
                        continue;
                    }
                }
                theUnit->StartAction(kActionPrepareToMove);
                theUnit->GetUnitClass()->ActionChanged(theUnit);
            }
        }
    }
    for (i = 0; i < theReplaced.size(); i++)
        this->RemoveUnit(theReplaced[i]);
    if (this->mGoal->mKind == kGoalUnloadTransport)
        EESendGroupCommand(this, NULL, this->mPlayer->GetPlayerID(), theUnloads ? 0xbb : 0xb8, 0);
    if (this->mIsQueued && theFree == 0)
        return;
    if (!this->mIsQueued)
        this->RequestAllPaths();
    else if (theQueued.size() != 0)
        this->RequestPaths(theQueued);
}

// FUNCTION: EXE 0x5ed61b
// Appends a goal to the goal queue and makes the queue's unit its owner
void EEUnitVectorItem::Add(EEUnitGoal* inGoal)
{
    this->mGoals.push_back(inGoal);
    inGoal->SetOwner(this->mUnit);
}

// FUNCTION: EXE 0x5ed63b
// Returns the first goal in the unit's given goal queue, or NULL when that queue is missing or empty
// EEComplexUnit::GetQueuedGoal (inline in the real header, its out-of-line COMDAT emitted here by its first user
// 0x1ebf85; ee/EEUnit.h declares it out of line, so it is defined here)
EEUnitGoal* EEComplexUnit::GetQueuedGoal(unsigned long inIndex)
{
    if (this->mGoalQueues.size() > inIndex && this->mGoalQueues[inIndex].mGoals.size() > 0)
        return this->mGoalQueues[inIndex].mGoals.front();
    return NULL;
}

// FUNCTION: EXE 0x5ed692
// Recomputes the group's average, minimum and maximum speeds, turn speeds and accelerations and their inverses,
// capping each average at the world table's factor 0x31 times the slowest unit's value
void EEActiveGroup::ComputeValues()
{
    this->mCapFactor = gWorldTable.GetFloatValue(kWorldGroupSpeedCapFactor);
    this->mSpeed = 0.0f;
    this->mMaxSpeed = 0.0f;
    this->mMinSpeed = 3.402823466e+38F;
    this->mAcceleration = 0.0f;
    this->mMaxAcceleration = 0.0f;
    this->mMinAcceleration = 3.402823466e+38F;
    this->mTurnSpeed = 0.0f;
    this->mMaxTurnSpeed = 0.0f;
    this->mMinTurnSpeed = 3.402823466e+38F;
    this->mTurnAcceleration = 0.0f;
    this->mMaxTurnAcceleration = 0.0f;
    this->mMinTurnAcceleration = 3.402823466e+38F;
    this->mInverseSpeed = 0.0f;
    this->mInverseAcceleration = 0.0f;
    this->mInverseTurnSpeed = 0.0f;
    this->mInverseTurnAcceleration = 0.0f;
    if (!(this->GetNumUnits() > 0))
        return;
    for (unsigned long i = 0; i < this->GetNumUnits(); i++) {
        EEUnitClassUpgrade* theStats = this->GetUnit(i)->mStats;
        this->mSpeed += theStats->GetMaxVelocity();
        this->mAcceleration += theStats->GetAcceleration();
        this->mTurnSpeed += theStats->GetMaxAngularVelocity();
        this->mTurnAcceleration += theStats->GetAngularAcceleration();
        if (theStats->GetMaxVelocity() > this->mMaxSpeed)
            this->mMaxSpeed = theStats->GetMaxVelocity();
        if (theStats->GetMaxVelocity() < this->mMinSpeed)
            this->mMinSpeed = theStats->GetMaxVelocity();
        if (theStats->GetAcceleration() > this->mMaxAcceleration)
            this->mMaxAcceleration = theStats->GetAcceleration();
        if (theStats->GetAcceleration() < this->mMinAcceleration)
            this->mMinAcceleration = theStats->GetAcceleration();
        if (theStats->GetMaxAngularVelocity() > this->mMaxTurnSpeed)
            this->mMaxTurnSpeed = theStats->GetMaxAngularVelocity();
        if (theStats->GetMaxAngularVelocity() < this->mMinTurnSpeed)
            this->mMinTurnSpeed = theStats->GetMaxAngularVelocity();
        if (theStats->GetAngularAcceleration() > this->mMaxTurnAcceleration)
            this->mMaxTurnAcceleration = theStats->GetAngularAcceleration();
        if (theStats->GetAngularAcceleration() < this->mMinTurnAcceleration)
            this->mMinTurnAcceleration = theStats->GetAngularAcceleration();
    }
    this->mSpeed /= this->GetNumUnits();
    this->mAcceleration /= this->GetNumUnits();
    this->mTurnSpeed /= this->GetNumUnits();
    this->mTurnAcceleration /= this->GetNumUnits();
    float theA = this->mMinSpeed;
    theA *= this->mCapFactor;
    if (theA < this->mSpeed)
        this->mSpeed = theA;
    if (this->mCapFactor * this->mMinAcceleration < this->mAcceleration)
        this->mAcceleration = this->mCapFactor * this->mMinAcceleration;
    float theB = this->mMinTurnSpeed;
    theB *= this->mCapFactor;
    if (theB < this->mTurnSpeed)
        this->mTurnSpeed = theB;
    float theCap = this->mCapFactor * this->mMinTurnAcceleration;
    if (this->mTurnAcceleration > theCap)
        this->mTurnAcceleration = theCap;
    this->mInverseSpeed = this->mSpeed > 0.0f ? 1.0f / this->mSpeed : 0.0f;
    this->mInverseAcceleration = this->mAcceleration > 0.0f ? 1.0f / this->mAcceleration : 0.0f;
    this->mInverseTurnSpeed = this->mTurnSpeed > 0.0f ? 1.0f / this->mTurnSpeed : 0.0f;
    this->mInverseTurnAcceleration = this->mTurnAcceleration > 0.0f ? 1.0f / this->mTurnAcceleration : 0.0f;
}

// FUNCTION: EXE 0x5ed9d4
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

// SYNTHETIC: EXE 0x5edaa8
// (the sorter's scalar deleting destructor, vtable 0x446d50; comes with Fill)

// FUNCTION: EXE 0x5edac5
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

// FUNCTION: EXE 0x5edaf3
// Marks the group finished: drops its goal and hands it, referenced, to the player's finished groups
void EEActiveGroup::Finish()
{
    this->ClearGoal();
    this->mPlayer->mFinishedGroups.push_back(this);
    this->Reference();
    this->mIsFinished = true;
}

// FUNCTION: EXE 0x5edb21
// Returns whether a unit belongs to a slot kind: 0 any, 1 air, 2 land or land building, 3 surface naval,
// 4 submerged naval, 5 movement domain bit 3
bool EEActiveGroup::IsInFormationSlot(EEComplexUnit* inUnit, long inSlot)
{
    bool theResult = false;
    switch (inSlot) {
    case 0:
        theResult = true;
        break;
    case 1:
        theResult = inUnit->mUnitClass->mDomains & kMovementAir;
        break;
    case 2:
        theResult = (inUnit->mUnitClass->mDomains & kMovementLand) || inUnit->mUnitClass->IsLandBuilding();
        break;
    case 3:
        theResult = (inUnit->mUnitClass->mDomains & kMovementSea) && inUnit->mUnitClass->mCategory != kUnitCategorySubmergedNaval;
        break;
    case 4:
        theResult = (inUnit->mUnitClass->mDomains & kMovementSea) && inUnit->mUnitClass->mCategory == kUnitCategorySubmergedNaval;
        break;
    case 5:
        theResult = (inUnit->mUnitClass->mDomains >> 3) & 1;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x5edbb2
// Returns the group's speed when it moves together, else the unit's own maximum velocity
float EEActiveGroup::GetMaxVelocity(EEComplexUnit* inUnit)
{
    return this->mMovesTogether ? this->mSpeed : inUnit->mStats->GetMaxVelocity();
}

// FUNCTION: EXE 0x5edbd0
// Returns the group's acceleration when it moves together, else the unit's own
float EEActiveGroup::GetAcceleration(EEComplexUnit* inUnit)
{
    return this->mMovesTogether ? this->mAcceleration : inUnit->mStats->GetAcceleration();
}

// FUNCTION: EXE 0x5edbee
// Returns the group's turn speed when it moves together, else the unit's own maximum angular velocity
float EEActiveGroup::GetMaxAngularVelocity(EEComplexUnit* inUnit)
{
    return this->mMovesTogether ? this->mTurnSpeed : inUnit->mStats->GetMaxAngularVelocity();
}

// FUNCTION: EXE 0x5edc0f
// Returns the group's turn acceleration when it moves together, else the unit's own angular acceleration
float EEActiveGroup::GetAngularAcceleration(EEComplexUnit* inUnit)
{
    return this->mMovesTogether ? this->mTurnAcceleration : inUnit->mStats->mAngularAcceleration;
}

// FUNCTION: EXE 0x5edc30
// Returns the group's inverse speed when it moves together, else the unit's own inverse maximum velocity
float EEActiveGroup::GetInverseMaxVelocity(EEComplexUnit* inUnit)
{
    return this->mMovesTogether ? this->mInverseSpeed : inUnit->mStats->mInverseMaxVelocity;
}

// FUNCTION: EXE 0x5edc51
// Returns the group's inverse acceleration when it moves together, else the unit's own
float EEActiveGroup::GetInverseAcceleration(EEComplexUnit* inUnit)
{
    return this->mMovesTogether ? this->mInverseAcceleration : inUnit->mStats->mInverseAcceleration;
}

// FUNCTION: EXE 0x5edc72
// Returns the group's inverse turn acceleration when it moves together, else the unit's own
float EEActiveGroup::GetInverseAngularAcceleration(EEComplexUnit* inUnit)
{
    return this->mMovesTogether ? this->mInverseTurnAcceleration : inUnit->mStats->mInverseAngularAcceleration;
}

// FUNCTION: EXE 0x5edc93
// Sets the group's goal and whether it is queued; the group moves together for a location goal asking for it
void EEActiveGroup::SetGoal(EEUnitGoal* inGoal, bool inFlag)
{
    this->mGoal = inGoal;
    this->mIsQueued = inFlag;
    this->mMovesTogether = inGoal->mKind == kGoalLocation && static_cast<EEUGLocation*>(inGoal)->GetUseGroupMovement();
}

// FUNCTION: EXE 0x5edcc0
// Hands the group's goal to its units by the group's action (drop off, gather, attack, heal, move, explore,
// patrol, disband); aircraft held in a container queue it on both goal queues
void EEActiveGroup::Plan()
{
    GE3DPoint theLocation;
    unsigned long theCount = this->GetNumUnits();
    bool theFlag = false;
    if (theCount == 0)
        return;
    switch ((unsigned long)this->mAction) {
    case kActionDisband:
        this->Finish();
        break;
    case kActionDropoffResource: {
        EEUGRust* theGoal = EEGatherGoalCast(this->mGoal);
        EEComplexUnit* theStorage = static_cast<EEComplexUnit*>(theGoal->GetGatherStorage());
        for (unsigned long i = 0; i < theCount; i++) {
            EEComplexUnit* theUnit = this->GetUnit(i);
            if (!theUnit->mIsCommandable)
                continue;
            if (this->mIsQueued && theUnit->mGoalState)
                theUnit->QueueGoal(theGoal->Clone(), 0);
            else {
                if (theUnit->mResourceAmount > 0 && theStorage->CanTake(theUnit, theUnit->mResourceType)) {
                    EEUGRust* theCurrent = EEGatherGoalCast(theUnit->mGoalState);
                    theUnit->SetGatheredResource(NULL);
                    if (theCurrent == NULL)
                        theUnit->SetGoal(theGoal->Clone(), 0, 0);
                    else
                        theCurrent->SetGatherStorage(theStorage);
                    theUnit->StartAction(kActionMoveToStorage);
                } else {
                    long theX = theStorage->GetTileX();
                    long theY = theStorage->GetTileY();
                    if (theStorage->mUnitClass->mTileSize > 1)
                        theUnit->GetNearestFootprintTile(theStorage, &theX, &theY);
                    if (gWorld.mMap->GetTile(theX, theY) == NULL) {
                        theX = theStorage->GetTileX();
                        theY = theStorage->GetTileY();
                    }
                    theUnit->SetGoal(new EEUGLocation(GE2DPoint((float)theX, (float)theY), true, true,
                                                      false, 0.0f, false, false), 0, 0);
                    theUnit->StartAction(kActionPrepareToMove);
                }
                theUnit->mUnitClass->ActionChanged(theUnit);
            }
        }
        break;
    }
    case kActionGatherResource: {
        EEUGRust* theGoal = EEGatherGoalCast(this->mGoal);
        EEUnit* theResource = theGoal->GetGatherResource();
        for (unsigned long i = 0; i < theCount; i++) {
            EEComplexUnit* theUnit = this->GetUnit(i);
            if (!theUnit->mIsCommandable)
                continue;
            if (gContinentManager.CanGetTo(theUnit, theResource->GetTileX(), theResource->GetTileY())) {
                EEUGRust* theCurrent = EEGatherGoalCast(theUnit->mGoalState);
                if (theCurrent == NULL || theCurrent->GetGatherResource() != theResource) {
                    if (this->mIsQueued && theUnit->mGoalState)
                        theUnit->QueueGoal(theGoal->Clone(), 0);
                    else {
                        if (theCurrent && theCurrent->GetGatherResource() && theCurrent->GetGatherResource()->mResourceType != kResourceWood
                            && theCurrent->GetGatherResource()->mUnitClass->mFamily != kUnitFamilyAnimal)
                            theUnit->SetGatheredResource(NULL);
                        theUnit->SetGoal(theGoal->Clone(), 0, 0);
                        theUnit->StartAction(kActionMoveToWaitingArea);
                        theUnit->mUnitClass->ActionChanged(theUnit);
                    }
                }
            } else if (this->mIsQueued && theUnit->mGoalState)
                theUnit->QueueGoal(new EEUGLocation(GE2DPoint((float)theResource->GetTileX(),
                                                            (float)theResource->GetTileY()),
                                                  true, true, false, 0.0f, false, false), 0);
            else {
                theUnit->SetGoal(new EEUGLocation(GE2DPoint((float)theResource->GetTileX(), (float)theResource->GetTileY()),
                                                  true, true, false, 0.0f, false, false), 0, 0);
                theUnit->StartAction(kActionPrepareToMove);
                theUnit->mUnitClass->ActionChanged(theUnit);
            }
        }
        break;
    }
    case kActionAttackLocation: {
        EEUGAttackGround* theGoal = EEGoalCast<EEUGAttackGround>(this->mGoal);
        for (unsigned long i = 0; i < theCount; i++) {
            EEComplexUnit* theUnit = this->GetUnit(i);
            bool theTransport = theUnit->mUnitClass->mCategory == kUnitCategoryAirplane || theUnit->mUnitClass->mCategory == kUnitCategoryPlanetSpaceship;
            if (!theUnit->mIsCommandable || !theUnit->mUnitClass->mCanAttackGround)
                continue;
            if (theTransport && theUnit->mContainer) {
                if (this->mIsQueued && theUnit->mGoalState) {
                    theUnit->QueueGoal(theGoal->Clone(), 0);
                    theUnit->QueueGoal(theGoal->Clone(), 1);
                    theUnit->mGoalQueues[1].AddTime(1);
                } else {
                    theUnit->ClearGoalQueue(0);
                    theUnit->ClearGoalQueue(1);
                    theUnit->QueueGoal(theGoal->Clone(), 0);
                    theUnit->QueueGoal(theGoal->Clone(), 1);
                    theUnit->mGoalQueues[1].AddTime(1);
                }
            } else if (this->mIsQueued && theUnit->mGoalState)
                theUnit->QueueGoal(theGoal->Clone(), 0);
            else {
                theUnit->mIsSpecialAttack = this->mIsSpecialAttack;
                theUnit->ClearGoalQueue(0);
                theUnit->SetGoal(theGoal->Clone(), 0, 0);
                theUnit->StartAction(kActionPrepareToMove);
                theUnit->mUnitClass->ActionChanged(theUnit);
            }
        }
        break;
    }
    case kActionAttackEnemyUnit: {
        EEUGAttackUnit* theGoal = EEGoalCast<EEUGAttackUnit>(this->mGoal);
        EEUnit* theTarget = theGoal->mTarget;
        for (unsigned long i = 0; i < theCount; i++) {
            EEComplexUnit* theUnit = this->GetUnit(i);
            if (!theUnit->mIsCommandable)
                continue;
            bool theHero = theUnit->mUnitClass->mUnitType == kUnitTypeAirport;
            bool theTransport = theUnit->mUnitClass->mCategory == kUnitCategoryAirplane || theUnit->mUnitClass->mCategory == kUnitCategoryPlanetSpaceship;
            bool theLanded = theUnit->mVelocity == 0.0f
                || theUnit->mTile->GetHeight() == theUnit->mPhysicalModel.GetLocation().GetZ();
            bool theRanged = theUnit->mUnitClass->IsPriest();
            bool theSiege = theUnit->mUnitClass->mAIKind == kAIKindAircraftCarrier || theUnit->mUnitClass->mAIKind == kAIKindSpaceCarrier;
            bool theContained = theUnit->mContainer != NULL;
            if (theHero) {
                if (this->mIsQueued)
                    theUnit->QueueSquadronGoals(theGoal, true);
                else {
                    theUnit->SetSquadronGoals(theGoal, true);
                    theUnit->StartAction(kActionAirportOpen);
                    theUnit->mUnitClass->ActionChanged(theUnit);
                }
                continue;
            }
            if (theTransport && theContained) {
                if (this->mIsQueued && theUnit->mGoalState) {
                    theUnit->QueueGoal(theGoal->Clone(), 0);
                    theUnit->QueueGoal(theGoal->Clone(), 1);
                    theUnit->mGoalQueues[1].AddTime(1);
                } else {
                    theUnit->ClearGoalQueue(0);
                    theUnit->ClearGoalQueue(1);
                    theUnit->QueueGoal(theGoal->Clone(), 0);
                    theUnit->QueueGoal(theGoal->Clone(), 1);
                    theUnit->mGoalQueues[1].AddTime(1);
                }
                continue;
            }
            if (theTransport && theLanded) {
                if (this->mIsQueued && theUnit->mGoalState)
                    theUnit->QueueGoal(theGoal->Clone(), 0);
                else {
                    theUnit->ClearGoalQueue(0);
                    theUnit->SetGoal(theGoal->Clone(), 0, 0);
                    theUnit->StartAction(kActionTakeoff);
                    theUnit->mUnitClass->ActionChanged(theUnit);
                }
                continue;
            }
            if (!theRanged) {
                if (theSiege) {
                    if (!this->mIsQueued || theUnit->GetQueuedGoal(1) == NULL) {
                        theUnit->ClearGoalQueue(1);
                        theUnit->SetGoal(theGoal->Clone(), 0, 1);
                        theUnit->StartAction(kActionAirportOpen);
                        theUnit->mUnitClass->ActionChanged(theUnit);
                    } else
                        theUnit->QueueGoal(theGoal->Clone(), 1);
                } else if (this->mIsQueued && theUnit->mGoalState)
                    theUnit->QueueGoal(theGoal->Clone(), 0);
                else {
                    theUnit->ClearGoalQueue(0);
                    theUnit->SetGoal(theGoal->Clone(), 0, 0);
                    theUnit->StartAction(kActionInitialAttackState);
                    theUnit->mUnitClass->ActionChanged(theUnit);
                    theUnit->mIsSpecialAttack = this->mIsSpecialAttack;
                }
            } else if (theTarget->mComplexUnit == NULL || !EECanConvert(theUnit, theTarget->mComplexUnit, theFlag)
                       || (theUnit->mUnitClass->IsCrusader() && !theGoal->mIsConvert)) {
                if (theUnit->CanAttackClass(theTarget->mUnitClass)) {
                    if (this->mIsQueued && theUnit->mGoalState)
                        theUnit->QueueGoal(theGoal->Clone(), 0);
                    else {
                        theUnit->ClearGoalQueue(0);
                        theUnit->SetGoal(theGoal->Clone(), 0, 0);
                        theUnit->StartAction(kActionInitialAttackState);
                        theUnit->mUnitClass->ActionChanged(theUnit);
                    }
                }
            } else if (this->mIsQueued && theUnit->mGoalState)
                theUnit->QueueGoal(theGoal->Clone(), 0);
            else {
                theUnit->ClearGoalQueue(0);
                theUnit->SetGoal(theGoal->Clone(), 0, 0);
                theUnit->StartAction(kActionConvertEnemyUnit);
                theUnit->mUnitClass->ActionChanged(theUnit);
            }
        }
        theGoal->mAttacker = NULL;
        break;
    }
    case kActionHealUnit: {
        EEUGHealUnit* theGoal = EEGoalCast<EEUGHealUnit>(this->mGoal);
        theGoal->GetTarget()->mPhysicalModel.GetLocation().GetPosition(theLocation);
        for (unsigned long i = 0; i < theCount; i++) {
            EEComplexUnit* theUnit = this->GetUnit(i);
            if ((theUnit->mUnitClass->mCategory == kUnitCategoryAirplane || theUnit->mUnitClass->mCategory == kUnitCategoryPlanetSpaceship)
                && theUnit->mContainer) {
                theUnit->ClearGoalQueue(0);
                theUnit->ClearGoalQueue(1);
                theUnit->QueueGoal(new EEUGLocation(&theLocation, false, true, false, 0.0f, false, false), 0);
                theUnit->QueueGoal(new EEUGLocation(&theLocation, false, true, false, 0.0f, false, false), 1);
                theUnit->mGoalQueues[1].AddTime(1);
            } else if (theUnit->mIsCommandable && theGoal->Suits(theUnit)) {
                theUnit->SetGoal(theGoal->Clone(), 0, 0);
                theUnit->StartAction(kActionTurnToFaceHealee);
                theUnit->mUnitClass->ActionChanged(theUnit);
                theFlag = true;
            } else if (theUnit->mStats->GetMaxVelocity() > 0.0f) {
                theUnit->SetGoal(new EEUGLocation(&theLocation, false, true, false, 0.0f, false, false), 0, 0);
                theUnit->StartAction(kActionPrepareToMove);
                theUnit->mUnitClass->ActionChanged(theUnit);
            }
//
//
        }
        if (theFlag)
            EESendGroupCommand(this, theGoal->GetTarget(), this->mPlayer->mPlayerID, 0xba, 0);
        else
            EESendGroupCommand(this, theGoal->GetTarget(), this->mPlayer->GetPlayerID(), 0xb8, 0);
        break;
    }
    case kActionUnloadTransport:
        this->mMovesTogether = true;
    case kActionPrepareToMove:
        this->LayOutFormation();
        break;
    case kActionFindUnexploredArea: {
        EEUGRust* theGoal = EEExploreGoalCast(this->mGoal);
        for (unsigned long i = 0; i < theCount; i++) {
            EEComplexUnit* theUnit = this->GetUnit(i);
            if (!theUnit->mIsCommandable)
                continue;
            bool theHero = theUnit->mUnitClass->mUnitType == kUnitTypeAirport;
            bool theTransport = theUnit->mUnitClass->mCategory == kUnitCategoryAirplane || theUnit->mUnitClass->mCategory == kUnitCategoryPlanetSpaceship;
            bool theLanded = theUnit->mVelocity == 0.0f
                || theUnit->mTile->GetHeight() == theUnit->mPhysicalModel.GetLocation().GetZ();
            if (theTransport && theUnit->mContainer) {
                theUnit->ClearGoalQueue(0);
                theUnit->ClearGoalQueue(1);
                theUnit->QueueGoal(theGoal->Clone(), 0);
                theUnit->QueueGoal(theGoal->Clone(), 1);
                theUnit->mGoalQueues[1].AddTime(1);
            } else {
                if (theHero) {
                    long theX = theUnit->GetTileX();
                    long theY = theUnit->GetTileY();
                    EEUGRust* theCopy = EEExploreGoalCast(theGoal->Clone());
                    theCopy->SetExploreTile(theX, theY);
                    theUnit->SetSquadronGoals(theCopy, false);
                } else {
                    theUnit->ClearGoals(0);
                    theUnit->SetGoal(theGoal->Clone(), 0, 0);
                }
                theUnit->StartAction(kActionFindUnexploredArea);
                theUnit->mUnitClass->ActionChanged(theUnit);
                if (theHero) {
                    theUnit->StartAction(kActionAirportOpen);
                    theUnit->mUnitClass->ActionChanged(theUnit);
                } else if (theTransport && theLanded) {
                    theUnit->StartAction(kActionTakeoff);
                    theUnit->mUnitClass->ActionChanged(theUnit);
                }
            }
        }
        break;
    }
    case kActionMoveToNextPatrolPoint: {
        EEUGRust* theGoal = EEPatrolGoalCast(this->mGoal);
        for (unsigned long i = 0; i < theCount; i++) {
            EEComplexUnit* theUnit = this->GetUnit(i);
            bool theTransport = theUnit->mUnitClass->mCategory == kUnitCategoryAirplane || theUnit->mUnitClass->mCategory == kUnitCategoryPlanetSpaceship;
            if (!theUnit->mIsCommandable)
                continue;
            if (theTransport && theUnit->mContainer) {
                EEComplexUnit* thePoint = theGoal->GetPatrolTarget() ? theGoal->GetPatrolTarget() : theGoal->GetPatrolGroup()->GetLast();
                thePoint->mPhysicalModel.GetLocation().GetPosition(theLocation);
                theUnit->ClearGoalQueue(0);
                theUnit->ClearGoalQueue(1);
                theUnit->QueueGoal(new EEUGLocation(&theLocation, true, true, false, 0.0f, false, false), 0);
                theUnit->QueueGoal(new EEUGLocation(&theLocation, true, true, false, 0.0f, false, false), 1);
                theUnit->mGoalQueues[1].AddTime(1);
            } else {
                if (!theUnit->mUnitClass->mCanPatrol
                    && (!theUnit->mIsDetachedFromAI || theUnit->mUnitClass->FindAction(kActionGetNextPatrolPoint) == NULL)) {
                    EEComplexUnit* thePoint = theGoal->GetPatrolTarget() ? theGoal->GetPatrolTarget() : theGoal->GetPatrolGroup()->GetLast();
                    thePoint->mPhysicalModel.GetLocation().GetPosition(theLocation);
                    theUnit->SetGoal(new EEUGLocation(&theLocation, true, true, false, 0.0f, false, false), 0, 0);
                    theUnit->StartAction(kActionPrepareToMove);
                } else {
                    theUnit->SetGoal(theGoal->Clone(), 0, 0);
                    theUnit->StartAction(kActionGetNextPatrolPoint);
                }
                theUnit->mUnitClass->ActionChanged(theUnit);
            }
        }
        break;
    }
    }
}

// FUNCTION: EXE 0x5eeb6d
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

// FUNCTION: EXE 0x5eebb9
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

// FUNCTION: EXE 0x5eec0b
// Keeps only commandable, living, built units that can attack: an attack value, an air traffic type, a priest,
// a storm, a balloon or a missile base
void EEActiveGroup::KeepAttackers()
{
    unsigned long i = 0;
    while (i < this->GetNumUnits()) {
        EEComplexUnit* theUnit = this->GetUnit(i);
        EEUnitClass* theClass = theUnit->mUnitClass;
        unsigned long theType = theClass->mUnitType;
        if (theUnit->mIsCommandable && theUnit->mHitPoints > 0 && theUnit->mIsBuilt
            && (theUnit->mStats->mAttack != 0.0f || theClass->mAirTrafficType != 0 || theType == kUnitTypeHurricane || theType == kUnitTypeAntiMatterStorm
                || theType == kUnitTypeBalloon09HotAirBalloon || theType == kUnitTypeBalloon10ObservationBalloon || theClass->IsPriest() || theType == kUnitTypeCpMissileBase))
            i++;
        else
            this->RemoveUnit(theUnit);
    }
}

// FUNCTION: EXE 0x5eecb2
// Keeps the attackers able to hit the target unit (ranged ones only if they see it); the other mobile
// non-priests split off into a group moving to the target's tile
// NEAR: the heading: the original computes x + 0.5 and the x difference before the second GetFirstUnit (ours
// forward-substitutes them into the UMArcTan2 arguments); one dead frame slot
void EEActiveGroup::SpreadAroundUnit(EEUnit* inTarget, long inTileX, long inTileY)
{
    EEUnitClass* theTargetClass = inTarget->mUnitClass;
    EEActiveGroup* theGroup = NULL;
    this->KeepAttackers();
    unsigned long i = 0;
    while (i < this->GetNumUnits()) {
        EEComplexUnit* theUnit = this->GetUnit(i);
        bool theSlow = theUnit->mStats->GetRange() > theUnit->mStats->GetLineOfSight();
        if (theUnit->mHitPoints <= 0)
            this->RemoveUnit(theUnit);
        else if ((theUnit->mUnitClass->mUnitType == kUnitTypeAirport || theUnit->mUnitClass->mAIKind == kAIKindAircraftCarrier
                  || theUnit->mUnitClass->mAIKind == kAIKindSpaceCarrier || theUnit->mUnitClass->mUnitType == kUnitTypeCpMissileBase
                  || theUnit->CanAttackClass(theTargetClass)
                  || (theUnit->mUnitClass->IsPriest() && EECanPriestConvert(theUnit, inTarget->mComplexUnit)))
                 && (!theSlow || EEIsSeenBy(inTarget, theUnit->GetPlayer(), true)))
            i++;
        else {
            this->RemoveUnit(theUnit);
            if (!theUnit->mUnitClass->IsPriest() && !theSlow) {
                if (theGroup == NULL)
                    theGroup = new EEActiveGroup(this->mPlayer);
                theGroup->Add(theUnit);
            }
        }
    }
    if (!theGroup)
        return;
    theGroup->KeepMobileUnits();
    if (!(theGroup->GetNumUnits() > 0))
        return;
    theGroup->SetFormation(this->mFormation);
    float theX = (float)inTileX;
    float theY = (float)inTileY;
    float theDX = theX + 0.5f - theGroup->GetFirstUnit()->mPhysicalModel.GetLocation().GetX();
    float theHeading = UMArcTan2(-theDX,
                                 theY + 0.5f - theGroup->GetFirstUnit()->mPhysicalModel.GetLocation().GetY());
    theGroup->SetGoal(new EEUGLocation(GE2DPoint(theX, theY), true, true, true, theHeading, false, false), this->mIsQueued);
    theGroup->mAction = kActionPrepareToMove;
    this->mPlayer->mPendingGroups.push_back(theGroup);
    theGroup->Plan();
    EESendGroupCommand(theGroup, NULL, this->mPlayer->GetPlayerID(), 0xb8, 0);
    if (theGroup->GetNumUnits() > 0)
        this->mPlayer->PlayAcknowledgement(theGroup->GetUnit(gPhysics->GetRandom(0L, (long)theGroup->GetNumUnits() - 1)));
}

// FUNCTION: EXE 0x5eef30
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

// FUNCTION: EXE 0x5eef6c
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

// FUNCTION: EXE 0x5eefbe
// Keeps the citizens; the other mobile units split off to attack the resource (an animal they can attack)
// or into a group moving next to it
// NEAR: register choice (the original keeps theAttackers in ebx and a dead zeroed slot at [ebp-4]); the heading as
// in SpreadAroundUnit
void EEActiveGroup::SpreadAroundResource(EEUnit* inTarget)
{
    EEActiveGroup* theAttackers = NULL;
    float theHeading = 0.0f;
    EEActiveGroup* theOthers = NULL;
    EEComplexUnit* theUnit = NULL;
    unsigned long i = 0;
    while (i < this->GetNumUnits()) {
        theUnit = this->GetUnit(i);
        if (theUnit->mIsCommandable && theUnit->mHitPoints > 0
            && (theUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen || theUnit->mUnitClass->mCategory == kUnitCategoryNavalCitizen))
            i++;
        else {
            this->RemoveUnit(theUnit);
            if (theUnit->mHitPoints > 0 && theUnit->mStats->GetMaxVelocity() > 0.0f) {
                if (inTarget->mUnitClass->mFamily == kUnitFamilyAnimal && theUnit->CanAttackClass(inTarget->mUnitClass)) {
                    if (theAttackers == NULL)
                        theAttackers = new EEActiveGroup(this->mPlayer);
                    theAttackers->Add(theUnit);
                } else {
                    if (theOthers == NULL)
                        theOthers = new EEActiveGroup(this->mPlayer);
                    theOthers->Add(theUnit);
                }
            }
        }
    }
    if (theOthers) {
        theOthers->SetFormation(this->mFormation);
        float theDX = (inTarget->GetTileX() + 0.5f) - theOthers->GetFirstUnit()->mPhysicalModel.GetLocation().GetX();
        theHeading = UMArcTan2(-theDX, (inTarget->GetTileY() + 0.5f)
                                     - theOthers->GetFirstUnit()->mPhysicalModel.GetLocation().GetY());
        theOthers->SetGoal(new EEUGLocation(GE2DPoint(inTarget->GetTileX() + 0.5f, inTarget->GetTileY() + 0.5f), true, true,
                                            true, theHeading, false, false), this->mIsQueued);
        theOthers->mAction = kActionPrepareToMove;
        this->mPlayer->mPendingGroups.push_back(theOthers);
        theOthers->Plan();
        EESendGroupCommand(theOthers, NULL, this->mPlayer->GetPlayerID(), 0xb8, 0);
        if (theOthers->GetNumUnits() > 0)
            this->mPlayer->PlayAcknowledgement(theOthers->GetUnit(gPhysics->GetRandom(0L, (long)theOthers->GetNumUnits() - 1)));
    }
    if (!theAttackers)
        return;
    theAttackers->SetGoal(new EEUGAttackUnit(theAttackers->GetFirstUnit(), inTarget, true, false), this->mIsQueued);
    theAttackers->mAction = kActionAttackEnemyUnit;
    this->mPlayer->mPendingGroups.push_back(theAttackers);
    theAttackers->Plan();
    EESendGroupCommand(theAttackers, inTarget, this->mPlayer->GetPlayerID(), 0xb9, 0);
    if (theAttackers->GetNumUnits() > 0)
        this->mPlayer->PlayAcknowledgement(theAttackers->GetUnit(gPhysics->GetRandom(0L, (long)theAttackers->GetNumUnits() - 1)));
}

// FUNCTION: EXE 0x5ef2d4
// Keeps the citizens; the other mobile units split off into a group moving to the tile (or beside a building on it)
// NEAR: the first GetTile is called out of line in the original (inline budget), the heading as in SpreadAroundUnit
void EEActiveGroup::SpreadAroundTile(long inTileX, long inTileY)
{
    GE2DPoint theLocation(0.0f, 0.0f);
    long theX = inTileX;
    long theY = inTileY;
    EEActiveGroup* theGroup = NULL;
    unsigned long i = 0;
    while (i < this->GetNumUnits()) {
        EEComplexUnit* theUnit = this->GetUnit(i);
        if (theUnit->mIsCommandable && theUnit->mHitPoints > 0
            && (theUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen || theUnit->mUnitClass->mCategory == kUnitCategoryNavalCitizen))
            i++;
        else {
            this->RemoveUnit(theUnit);
            if (theUnit->mHitPoints > 0 && theUnit->mStats->GetMaxVelocity() > 0.0f) {
                theLocation.SetX((float)inTileX);
                theLocation.SetY((float)inTileY);
                EEMapTile* theTile = gWorld.mMap->GetTile(inTileX, inTileY);
                if (theTile && theUnit->GetTileX() != -10000) {
                    for (RTSTileUnitNode* theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                        if (!(theNode->mUnit->mUnitClass->mTileSize > 1) || theNode->mUnit->IsPassableBy(theUnit))
                            continue;
                        theUnit->GetNearestFootprintTile(theNode->mUnit, &theX, &theY);
                        if (!gWorld.mMap->GetTile(theX, theY))
                            continue;
                        theLocation.SetX((float)theX);
                        theLocation.SetY((float)theY);
                        break;
                    }
                }
                if (theGroup == NULL)
                    theGroup = new EEActiveGroup(this->mPlayer);
                theGroup->Add(theUnit);
            }
        }
    }
    if (!theGroup)
        return;
    theGroup->SetFormation(this->mFormation);
    float theDX = (inTileX + 0.5f) - theGroup->GetFirstUnit()->mPhysicalModel.GetLocation().GetX();
    float theHeading = UMArcTan2(-theDX, (inTileY + 0.5f)
                                 - theGroup->GetFirstUnit()->mPhysicalModel.GetLocation().GetY());
    theGroup->SetGoal(new EEUGLocation(theLocation, true, true, true, theHeading, false, false), this->mIsQueued);
    theGroup->mAction = kActionPrepareToMove;
    this->mPlayer->mPendingGroups.push_back(theGroup);
    theGroup->Plan();
    EESendGroupCommand(theGroup, NULL, this->mPlayer->GetPlayerID(), 0xb8, 0);
    if (theGroup->GetNumUnits() > 0)
        this->mPlayer->PlayAcknowledgement(theGroup->GetUnit(gPhysics->GetRandom(0L, (long)theGroup->GetNumUnits() - 1)));
}

// FUNCTION: EXE 0x5ef56d
// Keeps only the commandable, living units that can move (airports included)
void EEActiveGroup::KeepMobileUnits()
{
    unsigned long i = 0;
    while (i < this->GetNumUnits()) {
        EEComplexUnit* theUnit = this->GetUnit(i);
        if (theUnit->mIsCommandable && theUnit->mHitPoints > 0
            && (theUnit->mStats->GetMaxVelocity() != 0.0f || theUnit->mUnitClass->mUnitType == kUnitTypeAirport))
            i++;
        else
            this->RemoveUnit(theUnit);
    }
}

// FUNCTION: EXE 0x5ef5ce
// Keeps the transports; the other mobile units split off into a group moving to the tile (or beside a building on it)
// NEAR: as SpreadAroundTile (the heading's operands; GetTile expanded where the original calls it out of line)
void EEActiveGroup::SpreadForUnload(long inTileX, long inTileY)
{
    GE2DPoint theLocation(0.0f, 0.0f);
    long theX = inTileX;
    long theY = inTileY;
    EEActiveGroup* theGroup = NULL;
    unsigned long i = 0;
    while (i < this->GetNumUnits()) {
        EEComplexUnit* theUnit = this->GetUnit(i);
        if (theUnit->mIsCommandable && theUnit->mHitPoints > 0
            && (theUnit->mUnitClass->IsTransport() || theUnit->mUnitClass->mIsParatrooperBomber))
            i++;
        else {
            this->RemoveUnit(theUnit);
            if (theUnit->mHitPoints > 0 && theUnit->mStats->GetMaxVelocity() > 0.0f) {
                theLocation.SetX((float)inTileX);
                theLocation.SetY((float)inTileY);
                EEMapTile* theTile = gWorld.mMap->GetTile(inTileX, inTileY);
                if (theTile && theUnit->GetTileX() != -10000) {
                    for (RTSTileUnitNode* theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                        if (!(theNode->mUnit->mUnitClass->mTileSize > 1) || theNode->mUnit->IsPassableBy(theUnit))
                            continue;
                        theUnit->GetNearestFootprintTile(theNode->mUnit, &theX, &theY);
                        if (!gWorld.mMap->GetTile(theX, theY))
                            continue;
                        theLocation.SetX((float)theX);
                        theLocation.SetY((float)theY);
                        break;
                    }
                }
                if (theGroup == NULL)
                    theGroup = new EEActiveGroup(this->mPlayer);
                theGroup->Add(theUnit);
            }
        }
    }
    if (!theGroup)
        return;
    theGroup->SetFormation(this->mFormation);
    float theDX = (inTileX + 0.5f) - theGroup->GetFirstUnit()->mPhysicalModel.GetLocation().GetX();
    float theHeading = UMArcTan2(-theDX, (inTileY + 0.5f)
                                 - theGroup->GetFirstUnit()->mPhysicalModel.GetLocation().GetY());
    theGroup->SetGoal(new EEUGLocation(theLocation, true, true, true, theHeading, false, false), this->mIsQueued);
    theGroup->mAction = kActionPrepareToMove;
    this->mPlayer->mPendingGroups.push_back(theGroup);
    theGroup->Plan();
    EESendGroupCommand(theGroup, NULL, this->mPlayer->GetPlayerID(), 0xb8, 0);
    if (theGroup->GetNumUnits() > 0)
        this->mPlayer->PlayAcknowledgement(theGroup->GetUnit(gPhysics->GetRandom(0L, (long)theGroup->GetNumUnits() - 1)));
}

// FUNCTION: EXE 0x5ef86c
// Keeps the units able to cast the calamity (special attack, enough power, in the tech node); the other
// living attackers split off into a group attacking the target
void EEActiveGroup::SplitCalamityCasters(EECalamityClass* inCalamity, EETechTreeNode* inNode, EEUnit* inTarget)
{
    EEActiveGroup* theGroup = NULL;
    unsigned long i = 0;
    while (i < this->GetNumUnits()) {
        EEComplexUnit* theUnit = this->GetUnit(i);
        if (theUnit->mIsCommandable && theUnit->mHitPoints > 0 && theUnit->mUnitClass->mHasSpecialAttack
            && theUnit->GetPower() >= inCalamity->GetCost() && inNode->Contains(theUnit->mUnitClass->mUnitType))
            i++;
        else {
            theUnit->Reference();   // Fix: RemoveUnit may release a dead unit's last reference (RemoveUnit)
            this->RemoveUnit(theUnit);
            if (theUnit->mHitPoints > 0 && theUnit->mStats->mAttack > 0.0f) {
                if (theGroup == NULL)
                    theGroup = new EEActiveGroup(this->mPlayer);
                theGroup->Add(theUnit);
            }
            theUnit->Dereference();
        }
    }
    if (!theGroup)
        return;
    theGroup->SetFormation(this->mFormation);
    theGroup->SetGoal(new EEUGAttackUnit(theGroup->GetFirstUnit(), inTarget, true, false), this->mIsQueued);
    theGroup->mAction = kActionAttackEnemyUnit;
    this->mPlayer->mPendingGroups.push_back(theGroup);
    theGroup->Plan();
    EESendGroupCommand(theGroup, NULL, this->mPlayer->GetPlayerID(), 0xb8, 0);
    if (theGroup->GetNumUnits() > 0)
        this->mPlayer->PlayAcknowledgement(theGroup->GetUnit(gPhysics->GetRandom(0L, (long)theGroup->GetNumUnits() - 1)));
}

// FUNCTION: EXE 0x5efa05
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

// FUNCTION: EXE 0x5efa8e
// Removes a unit id from the path request's units
void EEGroupPathRequest::RemoveUnit(long inUnitID)
{
    std::vector<long>::iterator it = std::find(this->mUnitIDs.begin(), this->mUnitIDs.end(), inUnitID);
    if (it != this->mUnitIDs.end())
        this->mUnitIDs.erase(it);
}

// FUNCTION: EXE 0x5efaf0
// Returns whether a location is a reserved destination or within half that unit's inner collision radius of one
// NEAR: the radius is copied into two frame slots (and its address formed) before the product: an inline accessor
// returning a reference, or a squaring helper, not found
bool EEActiveGroup::IsReserved(const GE3DPoint& inLocation)
{
    bool theResult = false;
    std::map<unsigned int, GE3DPoint>::iterator it;
    for (it = this->mDestinations.begin(); it != this->mDestinations.end(); ++it) {
        if (it->second == inLocation) {
            theResult = true;
            break;
        }
        EEUnit* theUnit = static_cast<EEUnit*>(gWorld.GetUnit(it->first));
        if (!theUnit)
            continue;
        float theRadius = theUnit->mUnitClass->GetInnerCollisionSphere();
        if (inLocation.DistanceToSquared(it->second) < theRadius * 0.5f * (theRadius * 0.5f))
            theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5efba8
// Returns whether a location is a reserved destination or in the same unit-region cell as one
// NEAR: the original converts all four coordinates first, keeps this/the iterator/the result in the frame and calls
// pair<long, int>'s ctor out of line
bool EEActiveGroup::IsCellTaken(const GE3DPoint& inLocation)
{
    bool theResult = false;
    std::map<unsigned int, GE3DPoint>::iterator it;
    for (it = this->mDestinations.begin(); it != this->mDestinations.end(); ++it) {
        if (it->second == inLocation) {
            theResult = true;
            break;
        }
        long theX = (long)it->second.GetX();
        long theY = (long)it->second.GetY();
        long theInX = (long)inLocation.GetX();
        long theInY = (long)inLocation.GetY();
        if (std::pair<long, int>(theX / gWorld.mMap->mUnitRegions->mRegionSize,
                                 theY / gWorld.mMap->mUnitRegions->mRegionSize)
            == std::pair<long, int>(theInX / gWorld.GetMap()->mUnitRegions->mRegionSize,
                                    theInY / gWorld.mMap->mUnitRegions->mRegionSize))
            theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5efca8
// Records a unit's destination among the group's reserved destinations
void EEActiveGroup::TellGroup(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{
    this->mDestinations[inUnit->mUnitID] = inLocation;
}

// FUNCTION: EXE 0x5efcd7
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

// TEMPLATE: EXE 0x5efd40
// ?GetHeadingTo@?$U2DPoint@J@@QBEMABV1@@Z (lle/U2DPoint.h exe view, out of line here: LayOutFormation's unload heading)

// TEMPLATE: EXE 0x5efd6c

// TEMPLATE: EXE 0x5efdac

// TEMPLATE: EXE 0x5efdd7

// FUNCTION: EXE 0x5efe28
// Appends an (index, weight) pair (std::vector<pair<long, float> >::push_back)

// FUNCTION: EXE 0x5efe54
// Returns whether the rectangle contains a point (U2DRectangle<float>::Contains)
// Not emitted by our compile (0x37 bytes; exe-view inline, lle/U2DRectangle.h): the original refused its inline
// expansion in 0x1ebf85, our compile expands it (inline budget, depth)

// FUNCTION: EXE 0x5efe8b
// Resizes a deque of slot indices (std::deque<long>::resize)

// FUNCTION: EXE 0x5efed0
// Returns a deque element by index (std::deque operator[])
// Not emitted by our compile (0x22 bytes): deque<long>::operator[] (0x1ebf85 theFreeSlots[i]) is refused inline in
// the original, expanded in our compile (inline budget, depth)

// TEMPLATE: EXE 0x5efef2

// TEMPLATE: EXE 0x5effa8

// TEMPLATE: EXE 0x5f0019

// TEMPLATE: EXE 0x5f02c7

// TEMPLATE: EXE 0x5f02f9

// TEMPLATE: EXE 0x5f032e

// TEMPLATE: EXE 0x5f04c2

// TEMPLATE: EXE 0x5f04eb

// TEMPLATE: EXE 0x5f052d

// FUNCTION: EXE 0x5f0556
// Steps a history-entry vector iterator back (std::_Ptrit<EECPRMHistoryEntry>::operator-)

// FUNCTION: EXE 0x5f056a
// Returns the rectangle's point closest to a point (U2DRectangle<float>::GetClosestPoint, exe-view template)

// FUNCTION: EXE 0x5f0627
// Sorts (index, unit) pairs (std::sort, EEAGSlotUnitLess)

// FUNCTION: EXE 0x5f06e7
// Sorts (index, weight) pairs by weight (std::sort, EEAGSecondLess)

// FUNCTION: EXE 0x5f07a7
// Sorts the group's units slowest first (EEGroup::SortBy with the velocity sorter)

// FUNCTION: EXE 0x5f07ee
// Copies a range of a unit deque backwards (std::copy_backward)

// FUNCTION: EXE 0x5f0831
// Returns the rectangle corner closest to a point and its two neighbours (U2DRectangle<float>::GetClosestCorner)

// FUNCTION: EXE 0x5f0a3c
// Returns the point of a segment closest to this point (U2DPoint<float>::GetClosestPointOnSegment, exe-view template)

// FUNCTION: EXE 0x5f0b60
// Insertion-sorts (index, unit) pairs (std::_Insertion_sort, EEAGSlotUnitLess)

// FUNCTION: EXE 0x5f0b89
// Quick-sorts (index, unit) pairs down to short runs (std::_Sort, EEAGSlotUnitLess)

// FUNCTION: EXE 0x5f0c61
// Insertion-sorts (index, unit) pairs without bounds checks (std::_Unguarded_insertion_sort, EEAGSlotUnitLess)

// FUNCTION: EXE 0x5f0c95
// Insertion-sorts (index, weight) pairs (std::_Insertion_sort, EEAGSecondLess)

// FUNCTION: EXE 0x5f0cbe
// Quick-sorts (index, weight) pairs down to short runs (std::_Sort, EEAGSecondLess)

// FUNCTION: EXE 0x5f0d96
// Insertion-sorts (index, weight) pairs without bounds checks (std::_Unguarded_insertion_sort, EEAGSecondLess)

// TEMPLATE: EXE 0x5f0dca

// FUNCTION: EXE 0x5f0eaa
// Copies the velocity sorter (EEAGIncreasingVelocityUnitSorter copy constructor)

// FUNCTION: EXE 0x5f0eb5
// Returns the squared distance between two points (U2DPoint<float>::DistanceToSquared)

// FUNCTION: EXE 0x5f0ed4
// Returns the rectangle's bottom-left corner (U2DRectangle<float>::GetBottomLeft, exe-view template)

// FUNCTION: EXE 0x5f0ee9
// Returns the rectangle's bottom-right corner (U2DRectangle<float>::GetBottomRight, exe-view template)

// FUNCTION: EXE 0x5f0efe
// Returns the rectangle's top-left corner (U2DRectangle<float>::GetTopLeft, exe-view template)

// FUNCTION: EXE 0x5f0f14
// Returns the rectangle's top-right corner (U2DRectangle<float>::GetTopRight, exe-view template)

// FUNCTION: EXE 0x5f0f2a
// Returns the point along a segment at a share of the distance to its start (U2DPoint<float>::ProjectOnSegment)

// FUNCTION: EXE 0x5f0fe8
// Insertion-sorts (index, unit) pairs (std::_Insertion_sort_1, EEAGSlotUnitLess)

// FUNCTION: EXE 0x5f109a
// Orders (index, unit) pairs: the slower class first, then the larger inner collision sphere, then by index

// FUNCTION: EXE 0x5f112b
// Advances a (long, long) pair vector iterator (std::_Ptrit::operator++)

// FUNCTION: EXE 0x5f1131
// Builds a heap of (index, unit) pairs (std::make_heap, EEAGSlotUnitLess)

// FUNCTION: EXE 0x5f116b
// Sorts a heap of (index, unit) pairs (std::sort_heap, EEAGSlotUnitLess)

// FUNCTION: EXE 0x5f11ac
// Partitions (index, unit) pairs around a median pivot (std::_Unguarded_partition, EEAGSlotUnitLess)

// FUNCTION: EXE 0x5f12c3
// Moves one of the (index, unit) pairs back to its sorted place (std::_Unguarded_insert, EEAGSlotUnitLess)

// FUNCTION: EXE 0x5f12ff
// Insertion-sorts (index, weight) pairs (std::_Insertion_sort_1, EEAGSecondLess)

// FUNCTION: EXE 0x5f13a8
// Builds a heap of (index, weight) pairs (std::make_heap, EEAGSecondLess)

// FUNCTION: EXE 0x5f13e2
// Sorts a heap of (index, weight) pairs (std::sort_heap, EEAGSecondLess)

// FUNCTION: EXE 0x5f1423
// Partitions (index, weight) pairs around a median pivot (std::_Unguarded_partition, EEAGSecondLess)

// FUNCTION: EXE 0x5f1527
// Moves one of the (index, weight) pairs back to its sorted place (std::_Unguarded_insert, EEAGSecondLess)

// TEMPLATE: EXE 0x5f1558

// TEMPLATE: EXE 0x5f158a

// TEMPLATE: EXE 0x5f1674

// FUNCTION: EXE 0x5f16b0
// Returns the distance between two points (U2DPoint<float>::DistanceTo; not emitted by our compile: inline budget)

// FUNCTION: EXE 0x5f16db
// Builds a heap of (index, unit) pairs (std::_Make_heap, EEAGSlotUnitLess)

// FUNCTION: EXE 0x5f1726
// Moves the top of a heap of (index, unit) pairs to its end (std::pop_heap, EEAGSlotUnitLess)

// FUNCTION: EXE 0x5f174f
// Builds a heap of (index, weight) pairs (std::_Make_heap, EEAGSecondLess)

// FUNCTION: EXE 0x5f179a
// Moves the top of a heap of (index, weight) pairs to its end (std::pop_heap, EEAGSecondLess)

// TEMPLATE: EXE 0x5f17c3

// TEMPLATE: EXE 0x5f1874

// TEMPLATE: EXE 0x5f18b7

// TEMPLATE: EXE 0x5f18fe

// TEMPLATE: EXE 0x5f1a0f

// FUNCTION: EXE 0x5f1a3f
// Sifts a value down a heap of (index, unit) pairs (std::_Adjust_heap, EEAGSlotUnitLess)

// FUNCTION: EXE 0x5f1ac7
// Moves the top of a heap of (index, unit) pairs to its end (std::_Pop_heap_0, EEAGSlotUnitLess)

// FUNCTION: EXE 0x5f1b1d
// Sifts a value down a heap of (index, weight) pairs (std::_Adjust_heap, EEAGSecondLess)

// FUNCTION: EXE 0x5f1b9b
// Moves the top of a heap of (index, weight) pairs to its end (std::_Pop_heap_0, EEAGSecondLess)

// TEMPLATE: EXE 0x5f1bf1

// TEMPLATE: EXE 0x5f1c48

// FUNCTION: EXE 0x5f1c7a
// Sifts a value up a heap of (index, unit) pairs (std::_Push_heap, EEAGSlotUnitLess)

// FUNCTION: EXE 0x5f1cda
// Moves the top of a heap of (index, unit) pairs to a given place and re-heaps (std::_Pop_heap, EEAGSlotUnitLess)

// FUNCTION: EXE 0x5f1d17
// Sifts a value up a heap of (index, weight) pairs (std::_Push_heap, EEAGSecondLess)

// FUNCTION: EXE 0x5f1d6b
// Moves the top of a heap of (index, weight) pairs to a given place and re-heaps (std::_Pop_heap, EEAGSecondLess)

// TEMPLATE: EXE 0x5f1da8

// TEMPLATE: EXE 0x5f1e1b

// TEMPLATE: EXE 0x5f1e8a

// TEMPLATE: EXE 0x5f1edb

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
        EEAwaitAutoPathRequest(thePath->mRequestID, NULL, [theGroup, theKey](EEAutoPathRequest* inRequest) {
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
