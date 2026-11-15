// C:\EEX\Empire Earth\AI Thread\EEAIAStarPathfindingGoalBase.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarPathfindingGoalBase, vtable 0x43a04c). The path goals' common part: 0x19a93..0x1a64b
// (EEAIAStarPathfindingGoal.cpp's base ??_G ends at 0x19a93; EEAIAStarPathGraphGoal's ctor starts at 0x1a64b). ctor,
// ??_G, dtor, then the members in address order, Virtual14 0x19ccb, the blocker collector 0x19da3..0x19eb0 and the
// COMDATs.
#include <windows.h>
#include <string.h>
#include <vector>
#include <map>
#include <set>
#include <math.h>
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

#include "ee/EmpireEarth.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIAStarFastStorage.h"
#include "ee/EEAIAStarTerrainStorage.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIAStarPathfindingGoalBase.h"

void EEGetUnitTiles(EEUnit* inUnit, std::vector<U2DPoint<long> >& ioPoints, float inArg);  // 0x248fe4 (cdecl) (provisional name)

// The units blocking a path near the goal (0x19ccb's local; sizeof 0x3c; ctor 0x19da3 out of line, members in
// address order after Virtual14) (provisional name)
class EEAIPathBlockerCollector {
public:
    EEAIPathBlockerCollector(EEComplexUnit* inUnit, std::vector<U2DPoint<long> >& inBlocked,
                             const U2DPoint<long>& inGoal, bool inNaval, bool inWide);
    bool Add(EEComplexUnit* inUnit);        // 0x19e38: a unit of the walker's kind (naval or not), not in its group
    bool AddTiles(EEComplexUnit* inUnit);   // 0x19eb0: inUnit's tiles not yet blocked, except the goal

    std::vector<U2DPoint<long> > mTiles;    // +0x00 (the new blocked tiles)
    std::set<U2DPoint<long> > mBlocked;     // +0x10 (the walker's own blocked tiles)
    U2DPoint<long> mPoint1C;                // +0x1c
    U2DPoint<long> mGoal;                   // +0x24
    EEComplexUnit* mUnit;                   // +0x2c (the walker)
    bool mNaval;                            // +0x30
    bool mWide;                             // +0x31 (the tile footprint margin 2.0f, else 1.0f)
};

// NEAR: the +0x54/+0x58 init-list stores sink below the float stores
// FUNCTION: EXE 0x419a93
// Creates the path goal base with no unit or modifier, then resets it
EEAIAStarPathfindingGoalBase::EEAIAStarPathfindingGoalBase()
    : mOffsetX(0.0f), mOffsetY(0.0f)
{
    this->mUnit = NULL;
    this->mModifier = NULL;
    this->Reset();
}

// SYNTHETIC: EXE 0x419b06
// (the scalar deleting destructor)

// FUNCTION: EXE 0x419b22
// Destroys the goal, releasing its unit and its modifier
EEAIAStarPathfindingGoalBase::~EEAIAStarPathfindingGoalBase()
{
    this->SetUnit(NULL);
    this->ReleaseModifier();
}

// FUNCTION: EXE 0x419b4b
// Collects the tiles a sea unit stands on into the goal's unit tile list (other units: none)
void EEAIAStarPathfindingGoalBase::CollectUnitTiles()
{
    if (this->mUnit->mUnitClass->mDomains & kMovementSea)
        EEGetUnitTiles(this->mUnit, this->mUnitTiles, 0.0f);
}

// FUNCTION: EXE 0x419b6e
// Dereferences the goal's cost/entry modifier and clears it
void EEAIAStarPathfindingGoalBase::ReleaseModifier()
{
    if (this->mModifier)
        this->mModifier->Dereference();
    this->mModifier = NULL;
}

// FUNCTION: EXE 0x419b84
// Collects the unit's own tiles (when there is a unit) and marks the blocked tiles; always returns true
bool EEAIAStarPathfindingGoalBase::Prepare()
{
    if (this->mUnit)
        this->CollectUnitTiles();
    this->MarkBlockedTiles();
    return true;
}

// NEAR: store schedule of the 0.5f pair
// FUNCTION: EXE 0x419b9d
// Resets the goal: releases the unit, clears the bounds, blocked and unit tiles and destination, offsets back to 0.5
void EEAIAStarPathfindingGoalBase::Reset()
{
    this->SetUnit(NULL);
    this->mL64 = 0;
    this->mGoalBounds.Clear();
    this->mBounds24.Clear();
    this->mBlockedTiles.clear();
    this->mUnitTiles.clear();
    this->mDestination = U2DPoint<long>(0, 0);
    this->mOffsetY = this->mOffsetX = 0.5f;
    this->mContinentID = 0;
    this->mIsBounded = false;
    this->mAvoidUnits = false;
}

// FUNCTION: EXE 0x419bf3
// Replaces the goal's modifier, referencing the new one and pointing it back at this goal
void EEAIAStarPathfindingGoalBase::SetModifier(EEAIPathGoalModifierBase* inModifier)
{
    this->ReleaseModifier();
    this->mModifier = inModifier;
    inModifier->Reference();
    this->mModifier->mGoal = this;
}

// NEAR: the GetContinentID argument push is scheduled earlier
// FUNCTION: EXE 0x419c0f
// Sets the unit to path for (moving the reference), takes its continent, and avoids other units unless it is a
// computer player's non-transport unit
void EEAIAStarPathfindingGoalBase::SetUnit(EEComplexUnit* inUnit)
{
    if (inUnit == this->mUnit)
        return;
    if (this->mUnit)
        this->mUnit->Dereference();
    this->mUnit = inUnit;
    if (!inUnit)
        return;
    inUnit->Reference();
    this->mOffsetY = this->mOffsetX = 0.5f;
    this->mContinentID = gContinentManager.GetContinentID(this->mUnit);
    if (this->mUnit->mPlayer->mComputerPlayer && !this->mUnit->mUnitClass->IsTransport())
        this->mAvoidUnits = false;
    else
        this->mAvoidUnits = true;
}

// FUNCTION: EXE 0x419c77
// Sets the goal bounds to a tile grown by inMargin on every side
void EEAIAStarPathfindingGoalBase::SetGoalBounds(const U2DPoint<long>& inTile, long inMargin)
{
    this->mGoalBounds.Set(inTile);
    this->mGoalBounds.Grow(inMargin, true);
}

// FUNCTION: EXE 0x419ca1
// Sets the start bounds to a tile grown by inMargin on every side
void EEAIAStarPathfindingGoalBase::SetStartBounds(const U2DPoint<long>& inTile, long inMargin)
{
    this->mStartBounds.Set(inTile);
    this->mStartBounds.Grow(inMargin, true);
}

// FUNCTION: EXE 0x419ccb
// Copies the unit's bounded-path tiles as blocked and, for sea units, adds the tiles of stationary contacts in
// the way (not the goal)
void EEAIAStarPathfindingGoalBase::CollectBlockedTiles(const U2DPoint<long>& inGoal)
{
    if (!this->mUnit)
        return;
    this->mBlockedTiles = this->mUnit->mBoundedPathTiles;
    if (this->mUnit->mUnitClass->mDomains & kMovementSea) {
        EEAIPathBlockerCollector theCollector(this->mUnit, this->mBlockedTiles, inGoal,
                                              this->mUnit->mUnitClass->mCategory == kUnitCategorySubmergedNaval || this->mUnit->mUnitClass->mCategory == kUnitCategorySeaAnimal,
                                              this->mIsBounded);
        for (UListNode<EEComplexUnit*>* theNode = this->mUnit->mContacts.mHead; theNode; theNode = theNode->mNext)
            theCollector.Add(theNode->mData);
        this->mBlockedTiles.insert(this->mBlockedTiles.end(), theCollector.mTiles.begin(), theCollector.mTiles.end());
    }
}

// FUNCTION: EXE 0x419da3
// Creates a blocker collector for a walker, its goal tile and naval/wide flags, seeded with the already blocked tiles
EEAIPathBlockerCollector::EEAIPathBlockerCollector(EEComplexUnit* inUnit, std::vector<U2DPoint<long> >& inBlocked,
                                                   const U2DPoint<long>& inGoal, bool inNaval, bool inWide)
    : mGoal(inGoal), mUnit(inUnit), mNaval(inNaval), mWide(inWide)
{
    for (long i = inBlocked.size() - 1; i >= 0; i--)
        this->mBlocked.insert(inBlocked[i]);
}

// NEAR: the original compares the two naval tests as bytes (an inline bool helper?) and keeps this in ecx
// FUNCTION: EXE 0x419e38
// Adds a contact's tiles as blockers when it is a stopped sea unit of the walker's naval kind, not the walker and
// not in its group (naval buildings with air traffic always count); always returns false
bool EEAIPathBlockerCollector::Add(EEComplexUnit* inUnit)
{
    if (inUnit->GetVelocity() != 0.0f || (!(inUnit->mUnitClass->mDomains & kMovementSea)) || this->mUnit == inUnit)
        return false;
    // STAND-IN (owner 2026-11-01): the (bool) cast on the naval comparison (docs/LOST_DEBUG.md; 65 -> 74)
    if ((bool)(inUnit->mUnitClass->mCategory == kUnitCategorySubmergedNaval || inUnit->mUnitClass->mCategory == kUnitCategorySeaAnimal) != this->mNaval)
        return false;
    if (this->mUnit->mActiveGroup == NULL || this->mUnit->mActiveGroup != inUnit->mActiveGroup
        || (inUnit->mUnitClass->mCategory == kUnitCategoryNavalBuilding && inUnit->mUnitClass->mAirTrafficType != 0))
        return this->AddTiles(inUnit);
    return false;
}

// NEAR: the original loads both F178s (fld/fld/fcompp) and calls _Ptrit::operator+ out of line
// FUNCTION: EXE 0x419eb0
// Adds a unit's footprint tiles (not the goal, not already blocked) when it is at least as big as the walker or
// push-immune; always returns false
bool EEAIPathBlockerCollector::AddTiles(EEComplexUnit* inUnit)
{
    std::vector<U2DPoint<long> > theTiles;
    if (inUnit->mUnitClass->GetInnerCollisionSphere() >= this->mUnit->mUnitClass->GetInnerCollisionSphere()
        || inUnit->mPushImmuneUntil > gPhysics->mGameTime) {
        EEGetUnitTiles(inUnit, theTiles, !this->mWide ? 1.0f : 2.0f);
        for (long i = theTiles.size() - 1; i >= 0; i--) {
            if (theTiles[i] != this->mGoal && this->mBlocked.find(theTiles[i]) == this->mBlocked.end())
                this->mTiles.push_back(theTiles[i]);
        }
    }
    return false;
}

// U2DRectangle<long>::Grow (the bounds' out-of-line COMDAT, first used here; lle/U2DRectangle.h exe view)
// TEMPLATE: EXE 0x419f9d

// Compiler-generated for this unit (first users here): set<U2DPoint<long> > dtor/insert/find/erase/_Erase/_Lbound
// 0x19fd0..0x1a19f, vector<U2DPoint<long> >::insert(P, F, L) 0x1a1d1 / its _Insert 0x1a20a / _Distance 0x1a4e2, and
// operator= 0x1a4f4 (Virtual14's mBlockedTiles = mUnit->mBoundedPathTiles).
// TEMPLATE: EXE 0x419fd0
// TEMPLATE: EXE 0x419ffb
// TEMPLATE: EXE 0x41a0bb
// TEMPLATE: EXE 0x41a0fc
// TEMPLATE: EXE 0x41a16d
// TEMPLATE: EXE 0x41a19f
// TEMPLATE: EXE 0x41a1d1
// TEMPLATE: EXE 0x41a20a
// TEMPLATE: EXE 0x41a4e2
// TEMPLATE: EXE 0x41a4f4
