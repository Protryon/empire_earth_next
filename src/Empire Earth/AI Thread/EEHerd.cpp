// C:\EEX\Empire Earth\AI Thread\EEHerd.cpp (file name inferred: no __FILE__; RTTI class EEHerd > EEAIArea,
// vtable 0x43c2e8; the save key "EEHerd-SaveLoadData"). A herd of wild animals as an AI area: 0xa1910..0xa543d
// (EECPWallManager/EEHerd boundary 0xa1910; EEHumanPlayer's ctor starts at 0xa543d). Every member is written; the
// unit's template COMDATs (0xa4f09..0xa53e8: the media map's
// _Tree members, vector<EEFormationSlot> back/pop_back/erase, the unit collector and its filter, EEPlayer::GetHerds
// and its herd filter, EEAnimal::Get, HasUnit) follow from the source.
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
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"
#include "lle/UReferenceCounter.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMapDimensions.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEAnimal.h"
#include "ee/EEMedia.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIRandom.h"
#include "ee/EEPathManager.h"
#include "ee/EEFormation.h"
#include "ee/EEFCircle.h"
#include "ee/EEFInwardCircle.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEHerd.h"
#include "ee/EEUnitCollector.h"
#include "ee/EEGlobalConstants.h"
#include "LostDebug.h"

// The unit filter of Chase/GetPrey (by reference to the collector; operator() COMDAT 0xa5238 and its test 0xa526b
// after the collector): a living sea-going unit on the map, on mContinent (0: any), either a fishing boat (mHuntable:
// unit types 0x6c, 0x6d, 0xd3) or a non-gaia unit that is no submarine or sea animal (categories 10, 0x1b), moving
// when mMoving (provisional name)
struct EEHerdUnitFilter {
    EEHerdUnitFilter(unsigned short inContinent, bool inHuntable, bool inMoving)
        : mContinent(inContinent), mHuntable(inHuntable), mMoving(inMoving) {}
    bool operator()(EEComplexUnit* inUnit) const
    {
        return (inUnit->mUnitClass->mDomains & kMovementSea) && (unsigned long)inUnit->mTileX < gMapWidth &&
            (unsigned long)inUnit->mTileY < gMapHeight ? this->Accept(inUnit) : false;
    }
    bool Accept(EEComplexUnit* inUnit) const
    {
        if ((this->mHuntable ? inUnit->mUnitClass->mUnitType >= kUnitTypeS02FishingBoatStone && (inUnit->mUnitClass->mUnitType <= kUnitTypeS04FishingBoatBronze ||
                                                                      inUnit->mUnitClass->mUnitType == kUnitTypeS11FishingBoatModern)
                          : inUnit->mUnitClass->GetCategory() != kUnitCategorySubmergedNaval && inUnit->mUnitClass->GetCategory() != kUnitCategorySeaAnimal &&
                                inUnit->mPlayer->mPlayerID != 0) &&
            (this->mContinent == 0 || gContinentManager.GetContinentID(inUnit) == this->mContinent) &&
            (!this->mMoving || ((long)inUnit->mWaypoints.size() > 1) == true) && inUnit->mHitPoints > 0)
            return true;
        return false;
    }
    unsigned short mContinent;
    bool mHuntable;
    bool mMoving;
};

// The other-herd filter of FollowNearestHerd (by value to EEPlayer::GetHerds; COMDAT 0xa536b): another herd of another animal
// that is no predator, on the same continent (provisional name)
struct EEHerdOtherHerdFilter {
    EEHerdOtherHerdFilter(EEHerd* inHerd) : mHerd(inHerd) {}
    bool operator()(EEHerd* inHerd) const
    {
        bool theResult = false;
        if (inHerd != this->mHerd && inHerd->mAnimalID != this->mHerd->mAnimalID && inHerd->mAnimal != NULL && !inHerd->mAnimal->mIsPredator &&
            inHerd->mContinent == this->mHerd->mContinent)
            theResult = true;
        return theResult;
    }
    EEHerd* mHerd;
};

const UString EEHerd::sData("EEHerd-SaveLoadData");
// 0x4e5e40, right after sData (0x4e5e30 + 0x10) in the original's initialised .data, value 0; its only use clears it
// (EEWorld.cpp). Owner by placement only (EECalamity's statics start at 0x4e5e48): PROVISIONAL
long gL4E5E40 = 0;
// SYNTHETIC: EXE 0x4a191a   (wrapper)
// SYNTHETIC: EXE 0x4a192b   (registrar)

// FUNCTION: EXE 0x4a1953
// Creates an empty herd with no units, leader, target or path request
EEHerd::EEHerd()
{
    this->mLeader = NULL;
    this->mTarget = NULL;
    this->mPathRequest = NULL;
    this->Clear();
}

// SYNTHETIC: EXE 0x4a19e3
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4a19ff
// Creates a player's herd of one animal type, with its herd radius, birth timer and a random maximum size
EEHerd::EEHerd(EEPlayer* inPlayer, long inAnimal, unsigned short inID)
{
    this->mLeader = NULL;
    this->mPathRequest = NULL;
    this->mTarget = NULL;
    this->Clear();
    this->mPlayer = inPlayer;
    this->mID = inID;
    this->mAnimalID = inAnimal;
    this->mAnimal = EEAnimal::Get(inAnimal);
    this->mPathRequest = NULL;
    this->mRadiusSquared = this->mAnimal->GetHerdRadius() * this->mAnimal->GetHerdRadius();
    this->mBreeds = this->mAnimal->mBreeds;
    if (this->mBreeds)
        this->mBirthTimer = gPhysics->GetRandom(this->mAnimal->GetMinBirthInterval(), this->mAnimal->GetMaxBirthInterval());
    this->mMaxUnits = gPhysics->GetRandom(this->mAnimal->GetMinHerdSize(), this->mAnimal->GetMaxHerdSize());
}

// FUNCTION: EXE 0x4a1b1e
// Destroys the herd, releasing its units, looks, path request, leader and target
EEHerd::~EEHerd()
{
    this->Clear();
}

// NEAR: EEModel (mStats->mModel) is only forward-declared in ee/EEUnit.h, so its EEMedia base is a cast here
// FUNCTION: EXE 0x4a1b45
// Adds an animal (unchecked once the herd has tiles, else if CanTake accepts it) and sets its graze time; rebuilds
// the area when it is adult or the territory is small; returns whether it was added
bool EEHerd::AddUnit(EEComplexUnit* inUnit)
{
    std::vector<U2DPoint<long> > theTiles;
    bool theResult = false;
    if (this->mTiles.Size() != 0 || this->CanTake(inUnit, true)) {
        theResult = true;
        if (inUnit->mGrowth != 1.0f)
            this->mNumYoung++;
        this->mContinent = gContinentManager.GetContinentID(inUnit);
        inUnit->mReferenceCount++;
        inUnit->SetHerd(this);
        this->mUnits.push_back(inUnit);
        this->mIsActive = inUnit->mStats->mIsInitialized;
        if (gPhysics) {
            long theTime = gPhysics->mGameTime;
            inUnit->SetGrazeTime(theTime + gPhysics->GetRandom(this->mAnimal->GetMinGrazeTime(), this->mAnimal->GetMaxGrazeTime()));
        }
        else
            inUnit->SetGrazeTime(inUnit->mGrazeTime + this->mAnimal->GetMinGrazeTime());
        for (long i = this->mUnits.size() - 1; i >= 0; i--)
            theTiles.push_back(U2DPoint<long>(this->mUnits[i]->mTileX, this->mUnits[i]->mTileY));
        this->SetUnitMedia((EEMedia*)inUnit->mStats->mModel, inUnit->mUnitID);
        if (inUnit->mGrowth == 1.0f ||
            (this->mBounds2.mRight - this->mBounds2.mLeft <= 4 && this->mBounds2.mBottom - this->mBounds2.mTop <= 4))
            this->Create(theTiles);
        if (inUnit->mIsRegrowing)
            this->mState = 10;
    }
    return theResult;
}

// FUNCTION: EXE 0x4a1d0f
// Reacts to a hit animal: a predator strikes back at an attacker it can hurt one time in four, otherwise the
// animal flees; then forgets its last attacker
void EEHerd::HandleUnitHit(EEComplexUnit* inUnit)
{
    EEComplexUnit* theAttacker = static_cast<EEComplexUnit*>(inUnit->mLastAttacker);
    std::vector<EEComplexUnit*> theUnits;
    theUnits.push_back(inUnit);
    if (inUnit->mHerd->mAnimal->mIsPredator && !(gPhysics->mRandom.Generate() < 0.75f)) {
        if (theAttacker && inUnit->mUnitClass->mAttack > 0) {
            long theTarget = inUnit->mUnitClass->mAmmoType;
            long theSource = theAttacker->mUnitClass->mFamily;
            if (theAttacker->mUnitClass->CanBeAttacked() && gU4E8708[theSource][theTarget] > 0.0f)
                this->Attack(theAttacker, theUnits);
        }
    } else
        inUnit->mUnitClass->Flee(inUnit);
    EEComplexUnit*& theAttackerRef = inUnit->mLastAttacker;
    if (theAttackerRef)
        theAttackerRef->Dereference();
    theAttackerRef = NULL;
}

// FUNCTION: EXE 0x4a1df7
// Orders each given animal to attack a target
void EEHerd::Attack(EEUnit* inTarget, const std::vector<EEComplexUnit*>& inUnits)
{
    for (long i = inUnits.size() - 1; i >= 0; i--) {
        EEUGAttackUnit* theGoal = new EEUGAttackUnit(inUnits[i], inTarget, true, false);
        if (!this->GiveGoal(inUnits[i], theGoal, 0, kActionInitialAttackState) && theGoal)
            delete theGoal;
    }
}

// FUNCTION: EXE 0x4a1e75
// Returns whether an animal may stand on a tile: walkable terrain, passable units and no other herd member there
bool EEHerd::CanStandOn(EEComplexUnit* inUnit, const U2DPoint<long>& inTile)
{
    bool theResult = false;
    EETile* theTile = gWorld.mMap->GetTileAtLocation(inTile);
    if (theTile) {
        long theType = theTile->mTerrainType;
        theResult = inUnit->GetUnitClass()->CanMoveOnTerrain(inUnit, theType);
        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode && theResult == true; theNode = theNode->mNext) {
            EEComplexUnit* theOwner = theNode->mUnit->mComplexUnit;
            theResult = theNode->mUnit->IsPassableBy(inUnit);
            if (theResult == true && theOwner && inUnit->mHolder != theOwner && this->HasUnit(theOwner) &&
                theOwner->mHolder != inUnit)
                theResult = false;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4a1efc
// On a water continent, picks a random moving ship if the target is lost, then sends the herd to circle it or
// to one of its next waypoints (index may be one past the last); returns whether there is a live moving target
bool EEHerd::Chase()
{
    std::vector<EEComplexUnit*> theTargets;
    std::vector<EEComplexUnit*> theUnits;
    std::vector<EEFormationSlot> theSlots;
    GE3DPoint thePosition;
    GE3DPoint theLocation;
    bool theResult;
    EEAIContinent* theContinent = gContinentManager.mContinents[this->mContinent];
    if (theContinent && theContinent->mCategory == kContinentCategoryWater &&
        !(this->mTarget && this->mTarget->mTileX != -10000 && (long)this->mTarget->mWaypoints.size() > 1 && this->mTarget->mHitPoints > 0)) {
        this->ReleaseTarget();
        EEHerdUnitFilter theFilter(this->mContinent, false, true);
        EEGetUnitsInRectangle(theContinent->mHull.GetBounds(), theTargets, theFilter, false);
        if (theTargets.size() > 0) {
            this->mTarget = theTargets[gPhysics->Fn82D66(theTargets)];
            this->mTarget->mReferenceCount++;
            this->mCenter = U2DPoint<long>(this->mTarget->mTileX, this->mTarget->mTileY);
        }
    }
    if (this->mTarget && this->mTarget->mTileX != -10000 && (long)this->mTarget->mWaypoints.size() > 1 && this->mTarget->mHitPoints > 0) {
        theResult = true;
        EEFInwardCircle* theFormation = dynamic_cast<EEFInwardCircle*>(EEFormation::Create(6));
        theUnits = this->mUnits;
        this->mTarget->GetPosition(thePosition);
        theFormation->mCenter.mX = thePosition.mX;
        theFormation->mCenter.mY = thePosition.mY;
        theFormation->mCenter.mZ = thePosition.mZ;
        theFormation->mMinRadius = gPhysics->GetRandom(1.25f, 2.75f);
        theFormation->Place(theUnits, theSlots);
        while (theUnits.size() != 0) {
            EEComplexUnit* theUnit = theUnits.back();
            if (theUnit->mTileX != -10000 && !theUnit->mIsDisabled) {
                float theHeading = 0.0f;
                bool theFace = false;
                if (theUnit->DistanceSquaredTo(this->mTarget) > 9.0f && theSlots.size() > 0) {
                    long theIndex = gPhysics->GetRandomIndex(theSlots);
                    theLocation = theSlots[theIndex].mPosition;
                    theFace = true;
                    theHeading = theSlots[theIndex].mHeading;
                } else {
                    long theWaypoint = gPhysics->GetRandom(1L, 7L);
                    if (theWaypoint >= (long)this->mTarget->mWaypoints.size())
                        theWaypoint = this->mTarget->mWaypoints.size();
                    // the clamp above lands one past the last waypoint: that slot holds no waypoint, so it reads
                    // as the zero location instead of past the end of the path
                    if (theWaypoint < (long)this->mTarget->mWaypoints.size())
                        theLocation = this->mTarget->mWaypoints[theWaypoint].mLocation;
                    else
                        theLocation = GE3DPoint(0.0f, 0.0f, 0.0f);
                }
                EEUGLocation* theGoal = new EEUGLocation(&theLocation, true, true, theFace, theHeading, false, false);
                if (!this->GiveGoal(theUnit, theGoal, 0, kActionPrepareToMove) && theGoal)
                    delete theGoal;
            }
            theUnits.pop_back();
        }
    } else {
        this->ReleaseTarget();
        theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x4a2275
// Queues for every herd animal a walk through all slots of a ring of the given radius around a tile on the
// herd's continent; returns whether any animal was ordered
bool EEHerd::MoveTo(const U2DPoint<long>& inTile, unsigned long inRadius)
{
    bool theResult = false;
    std::vector<EEComplexUnit*> theUnits;
    std::vector<EEFormationSlot> theSlots;
    EEFInwardCircle* theFormation = dynamic_cast<EEFInwardCircle*>(EEFormation::Create(6));
    GE3DPoint theCenter((float)inTile.mX + 0.5f, (float)inTile.GetY() + 0.5f, 0.0f);
    if (gWorld.mMap->GetTile(inTile.mX, inTile.GetY())) {
        float theZ = gWorld.mMap->mTerrainMesh.GetZ(inTile.mX, inTile.GetY());
        if (theZ < 0.0f)
            theCenter.mZ = theZ * 0.5f;
    }
    if ((unsigned long)inTile.mX < gMapWidth && (unsigned long)inTile.GetY() < gMapHeight &&
        gContinentManager.GetContinentID(inTile.mX, inTile.GetY()) == this->mContinent) {
        theUnits = this->mUnits;
        theFormation->mCenter = theCenter;
        theFormation->mMinRadius = (float)inRadius;
        theFormation->Place(theUnits, theSlots);
        while (theUnits.size() != 0) {
            if (theUnits.back()->mTileX != -10000 && !theUnits.back()->mIsDisabled) {
                theResult = true;
                for (long i = theSlots.size() - 1; i >= 0; i--) {
                    EEUGLocation* theGoal = new EEUGLocation(&theSlots[i].mPosition, true, false, true,
                                                             theSlots[i].mHeading, false, false);
                    if (!this->GiveGoal(theUnits.back(), theGoal, 1, kActionPrepareToMove) && theGoal)
                        delete theGoal;
                }
            }
            theUnits.pop_back();
        }
    }
    theFormation->Dereference();
    return theResult;
}

// FUNCTION: EXE 0x4a24d0
// On a water continent, picks a random fishing boat as target and circles the herd around it; returns whether
// the herd was ordered
bool EEHerd::Hunt()
{
    std::vector<EEComplexUnit*> theUnits;
    bool theResult = false;
    EEAIContinent* theContinent = gContinentManager.GetContinentByID(this->mContinent);
    if (!theContinent || theContinent->mCategory != kContinentCategoryWater)
        return theResult;
    this->ReleaseTarget();
    this->GetPrey(this->mContinent, theUnits);
    if (theUnits.size() <= 0)
        return theResult;
    this->mTarget = theUnits[gPhysics->Fn82D66(theUnits)];
    this->mTarget->mReferenceCount++;
    this->mCenter = this->mTarget->GetTile();
    U2DPoint<long> theTile(this->mTarget->GetTile());
    theResult = this->MoveTo(theTile, gPhysics->GetRandom(2L, 5L));
    return theResult;
}

// FUNCTION: EXE 0x4a25b6
// Resets the herd to empty: releases units, looks, path request, leader and target and zeroes timers and state
void EEHerd::Clear()
{
    this->ClearUnits();
    this->mBounds2.Clear();
    this->mCenter = U2DPoint<long>(0, 0);
    this->mAnimal = NULL;
    this->mPlayer = NULL;
    if (this->mPathRequest) {
        gPathManager.CancelAutoPathRequest(this->mPathRequest);
        this->mPathRequest->Dereference();
        this->mPathRequest = NULL;
    }
    this->ReleaseLeader();
    this->ReleaseTarget();
    this->mNumYoung = 0;
    this->mP94 = U2DPoint<long>(0, 0);
    this->mFB8 = 0.0f;
    this->mFollowTime = 0;
    this->mGrazeAreaID = 0;
    this->mState = 0;
    this->mBirthTimer = 0;
    this->mLE4 = 0;
    this->mNextUpdateTime = 0;
    this->mLD8 = 0;
    this->mMaxUnits = 0;
    this->mContinent = 0;
    this->mID = 0;
    this->mBreeds = false;
    this->mLC8 = 0;
    this->mChaseTime = 0;
    this->mHuntTime = 0;
    this->mAnimalID = 0x3e8;
    this->mRadiusSquared = 0x169;
    this->mAreaType = kAIAreaHerd;
    this->mIsActive = true;
}

// NEAR: EEModel (mStats->mModel) is only forward-declared in ee/EEUnit.h, so its EEMedia base is a cast here
// FUNCTION: EXE 0x4a26b9
// Makes the animal with the most hit points the leader, giving it the leader look and restoring the old one's
void EEHerd::PickLeader()
{
    // LOST-DEBUG: none: exact only (n=16 is the only exact n in 1..32; the orig refuses all three mUnits[i] op+)
    EE_LOST_DEBUG(16);
    EEComplexUnit* theBest = NULL;
    for (unsigned long i = 0; i < this->mUnits.size(); i++) {
        if (theBest) {
            if (this->mUnits[i]->mHitPoints > theBest->mHitPoints)
                theBest = this->mUnits[i];
        } else
            theBest = this->mUnits[i];
    }
    if (!theBest || theBest == this->mLeader)
        return;
    if (this->mAnimal->mMedia)
        this->SetUnitMedia(this->mAnimal->mMedia, theBest->mUnitID);
    if (this->mLeader)
        this->SetUnitMedia((EEMedia*)this->mLeader->mStats->mModel, this->mLeader->mUnitID);
    this->ReleaseLeader();
    this->mLeader = theBest;
    theBest->mReferenceCount++;
}

// NEAR: the margin point goes through EETileDistanceLess's copy ctor in the original (0x4d28b, the
// U2DPoint<long> copy family)
// FUNCTION: EXE 0x4a2775
// Rebuilds the herd's area from tiles and, unless loading a saved game with a big territory, widens the
// territory by a random margin; then picks the leader
void EEHerd::Create(const std::vector<U2DPoint<long> >& inTiles)
{
    U2DPoint<long> theCorner;
    EEAIArea::Create(inTiles);
    if (!gLoadingSavedGame || (this->mBounds2.mRight - this->mBounds2.mLeft > 4 || this->mBounds2.mBottom - this->mBounds2.mTop > 4)) {
        if (gLoadingSavedGame)
            goto done;
    }
    this->mBounds2 = this->mHull.GetBounds();
    {
        long theMargin = gPhysics->GetRandom((this->mAnimal->GetMaxTerritoryMargin() - this->mAnimal->GetMinTerritoryMargin()) / 2, this->mAnimal->GetMaxTerritoryMargin());
        U2DPoint<long> theLeftTop = this->mBounds2.GetTopLeft();
        long theY = theLeftTop.mY - theMargin;
        if (theY < 0)
            theY = 0;
        long theX = theLeftTop.mX - theMargin;
        if (theX < 0)
            theX = 0;
        theCorner.mY = theY;
        theCorner.mX = theX;
        this->mBounds2.Extend(theCorner);
        theCorner.mX = this->mBounds2.mRight + theMargin;
        theCorner.mY = this->mBounds2.mBottom + theMargin;
        this->mBounds2.Extend(theCorner);
    }
done:
    this->PickLeader();
}

// FUNCTION: EXE 0x4a286a
// Sends a free animal to a random standable tile (graze area, then territory) and sets its next graze time; its
// young follow to nearby tiles, but those tests and graze-time resets use the parent, not the young
void EEHerd::Graze(EEComplexUnit* inUnit, unsigned long inTime)
{
    std::vector<U2DPoint<long> > theTried;
    GE3DPoint theLocation;
    U2DPoint<long> theOther;
    U2DPoint<long> theTile;
    EEAIArea* theArea = gContinentManager.GetArea(this->mGrazeAreaID);
    bool theFound = false;
    if (inUnit->mHolder != NULL)
        return;
    theTile.mX = inUnit->mTileX;
    theTile.mY = inUnit->mTileY;
    unsigned short theContinent = gContinentManager.GetContinentID(theTile.mX, theTile.mY);
    if (theArea) {
        for (long i = 0; i < 15 && !theFound; i++) {
            theTile = theArea->GetRandomPoint();
            theFound = this->CanStandOn(inUnit, theTile);
        }
    }
    for (long j = 0; j < 15 && !theFound; j++) {
        theTile = this->GetRandomTile();
        theFound = this->CanStandOn(inUnit, theTile);
    }
    if (!((unsigned long)theTile.mX < gMapWidth) || !((unsigned long)theTile.mY < gMapHeight) ||
        gContinentManager.GetContinentID(theTile.mX, theTile.mY) != theContinent)
        return;
    theLocation.mX = (float)theTile.mX + gPhysics->GetRandom(kHeightEpsilon, 0.99f);
    theLocation.mY = (float)theTile.mY + gPhysics->GetRandom(kHeightEpsilon, 0.99f);
    EEUGLocation* theGoal = new EEUGLocation(&theLocation, true, !this->mAnimal->mIsPredator, false, 0.0f, false, false);
    bool theMoved = this->GiveGoal(inUnit, theGoal, 0, kActionPrepareToMove);
    if (!theMoved && theGoal)
        delete theGoal;
    inUnit->SetGrazeTime(gPhysics->GetRandom(this->mAnimal->GetMinGrazeTime(), this->mAnimal->GetMaxGrazeTime()) + inTime);
    if (!theMoved || inUnit->mHeldUnits.size() == 0)
        return;
    U2DPoint<long> theStart = theTile;
    long theTop = max(theStart.mY - 1, 0);
    long theLeft = max(theStart.mX - 1, 0);
    long theLimit = gWorld.mMap->mSize - 1;
    long theRight = min(theStart.mX + 1, theLimit);
    long theBottom = min(theStart.mY + 1, theLimit);
    for (long k = inUnit->mHeldUnits.size() - 1; k >= 0; k--) {
        EEComplexUnit* theYoung = inUnit->mHeldUnits[k];
        theTile = theStart;
        if (theYoung->mGrowth > 0.79f) {
            theFound = false;
            for (long m = 0; !theFound && m < 5; m++) {
                theOther.mX = gPhysics->GetRandom(0, theRight - theLeft) + theLeft;
                theOther.mY = gPhysics->GetRandom(0, theBottom - theTop) + theTop;
                if (std::find(theTried.begin(), theTried.end(), theOther) == theTried.end())
                    theFound = this->CanStandOn(theYoung, theOther);
            }
            if (theFound) {
                theTried.push_back(theOther);
                theTile = theOther;
            }
        }
        theLocation.mX = (float)theTile.mX + gPhysics->GetRandom(kHeightEpsilon, 0.99f);
        theLocation.mY = (float)theTile.mY + gPhysics->GetRandom(kHeightEpsilon, 0.99f);
        EEUGLocation* theYoungGoal = new EEUGLocation(&theLocation, true, !this->mAnimal->mIsPredator, false, 0.0f,
                                                      false, false);
        if (!this->GiveGoal(theYoung, theYoungGoal, 0, kActionPrepareToMove) && theYoungGoal)
            delete theYoungGoal;
        theYoung->SetGrazeTime(gPhysics->GetRandom(this->mAnimal->GetMinGrazeTime(), this->mAnimal->GetMaxGrazeTime()) + inTime);
    }
}

// FUNCTION: EXE 0x4a2d03
// Picks the parent for a newborn: rarely (6.5%) one with one or two young, else an adult with no young (or
// failing that one with young); NULL when the pick has more than two young
EEComplexUnit* EEHerd::FindMother()
{
    EEComplexUnit* theResult = NULL;
    EEComplexUnit* theBest = NULL;
    long theCount = -1;
    if (gPhysics->GetRandom() <= 0.065f) {
        for (long i = this->mUnits.size() - 1; i >= 0; i--) {
            if (this->mUnits[i]->GetTileX() == -10000 || this->mUnits[i]->mIsDisabled || this->mUnits[i]->mHeldUnits.size() <= 0 ||
                this->mUnits[i]->mHeldUnits.size() > 2)
                continue;
            theResult = this->mUnits[i];
            theCount = this->mUnits[i]->mHeldUnits.size();
            break;
        }
    }
    if (theResult == NULL) {
        for (long j = this->mUnits.size() - 1; j >= 0; j--) {
            if (this->mUnits[j]->mGrowth != 1.0f || this->mUnits[j]->GetTileX() == -10000 || this->mUnits[j]->mIsDisabled)
                continue;
            if (this->mUnits[j]->mHeldUnits.size() != 0) {
                theBest = this->mUnits[j];
                if (theCount < 0)
                    theCount = this->mUnits[j]->mHeldUnits.size();
                else if (theCount >= (long)this->mUnits[j]->mHeldUnits.size())
                    theCount = this->mUnits[j]->mHeldUnits.size();
            } else {
                theResult = this->mUnits[j];
                theCount = 0;
                break;
            }
        }
        if (theResult == NULL && theBest != NULL)
            theResult = theBest;
    }
    if (theCount > 2)
        theResult = NULL;
    return theResult;
}

// FUNCTION: EXE 0x4a302c
// Collects the herd animals that are not fully grown
void EEHerd::GetYoungUnits(std::vector<EEComplexUnit*>& outUnits)
{
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if (this->mUnits[i]->mGrowth < 1.0f)
            outUnits.push_back(this->mUnits[i]);
    }
}

// FUNCTION: EXE 0x4a3075
// Collects the living fishing boats within the herd's continent bounds (on inContinent unless it is 0)
void EEHerd::GetPrey(unsigned short inContinent, std::vector<EEComplexUnit*>& outUnits)
{
    EEHerdUnitFilter theFilter(inContinent, true, false);
    const std::vector<EEAIContinent*>& theContinents = gContinentManager.mContinents;
    EEAIContinent* theContinent = theContinents[this->mContinent];
    if (theContinent)
        EEGetUnitsInRectangle(theContinent->mHull.GetBounds(), outUnits, theFilter, false);
}

// FUNCTION: EXE 0x4a30b6
// Returns a random tile within the herd's territory rectangle
U2DPoint<long> EEHerd::GetRandomTile()
{
    U2DPoint<long> thePoint;
    long theX = gPhysics->GetRandom(0, this->mBounds2.mRight - this->mBounds2.mLeft);
    thePoint.mX = this->mBounds2.mLeft + theX;
    long theY = gPhysics->GetRandom(0, this->mBounds2.mBottom - this->mBounds2.mTop);
    thePoint.mY = this->mBounds2.mTop + theY;
    return thePoint;
}

// FUNCTION: EXE 0x4a3110
// Gives a live, enabled, placed animal a goal and starts an action; returns false (goal not taken) otherwise
bool EEHerd::GiveGoal(EEComplexUnit* inUnit, EEUnitGoal* inGoal, bool inKeepCurrent, long inAction)
{
    bool theResult = false;
    if (inUnit && inUnit->mHitPoints > 0 && !inUnit->mIsDisabled && inUnit->mTileX != -10000) {
        inUnit->SetGoal(inGoal, inKeepCurrent, 0);
        inUnit->StartAction(inAction);
        inUnit->mUnitClass->ActionChanged(inUnit);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x4a315b
// Sends the hit animal (or the whole herd) to attack its last attacker, when it can hurt it
void EEHerd::AttackBack(EEComplexUnit* inUnit, bool inAll)
{
    std::vector<EEComplexUnit*> theUnits;
    EEComplexUnit* theAttacker = static_cast<EEComplexUnit*>(inUnit->mLastAttacker);
    if (!theAttacker || !(inUnit->mUnitClass->mAttack > 0))
        return;
    long theTarget = inUnit->mUnitClass->mAmmoType;
    long theSource = theAttacker->mUnitClass->mFamily;
    if (!theAttacker->mUnitClass->CanBeAttacked() || !(gU4E8708[theSource][theTarget] > 0.0f))
        return;
    if (inAll)
        theUnits = this->mUnits;
    else
        theUnits.push_back(inUnit);
    this->Attack(theAttacker, theUnits);
}

// Exact with EEAnimal +0x34 as a float read through GetGrowthStep(), the unit's mGrowth through GetGrowth(), `1.0f < x`
// FUNCTION: EXE 0x4a31fd
// Grows a young animal by one growth step and schedules the next; once adult, its parent lets it go
void EEHerd::GrowUnit(EEComplexUnit* inUnit)
{
    float theGrowth = this->mAnimal->GetGrowthStep() + inUnit->GetGrowth();
    if (1.0f < theGrowth)
        theGrowth = 1.0f;
    inUnit->SetGrowth(theGrowth);
    inUnit->SetGrowTime(gPhysics->mGameTime + this->mAnimal->GetGrowInterval());
    if (!(inUnit->mGrowth >= 1.0f) || !inUnit->mHolder)
        return;
    this->mNumYoung--;
    inUnit->mHolder->ReleaseHeld(inUnit);
    if (inUnit->mHolder)
        inUnit->mHolder->Dereference();
    inUnit->mHolder = NULL;
}

// NEAR: EEPlayer::mHerds is a vector<long> in ee/EEPlayer.h (EEHerd* elements): the herds go through casts here
// FUNCTION: EXE 0x4a3295
// Surrounds an animal of the nearest other non-predator herd on the continent (always its first: the distances
// to its animals are computed but unused); returns whether the herd now has a target
bool EEHerd::FollowNearestHerd()
{
    std::vector<EEHerd*> theHerds;
    EEComplexUnit* theLeader = this->mLeader ? this->mLeader : this->mUnits[0];
    float theBest = 0.0f;
    EEHerd* theBestHerd = NULL;
    this->mPlayer->GetHerds(EEHerdOtherHerdFilter(this), theHerds);
    this->ReleaseTarget();
    for (long i = theHerds.size() - 1; i >= 0; i--) {
        float theDistance = theHerds[i]->GetHullDistanceSquared(this);
        if (theBest == 0.0f || theDistance <= theBest) {
            theBestHerd = theHerds[i];
            theBest = theDistance;
        }
    }
    if (theBestHerd) {
        EEComplexUnit* theTarget = NULL;
        float theNearest = 0.0f;
        for (long j = theBestHerd->mUnits.size() - 1; j >= 0; j--) {
            float theDistance = theLeader->DistanceSquaredTo(theBestHerd->mUnits[j]);
            if (theTarget == NULL || theDistance <= theNearest) {
                theTarget = theBestHerd->mUnits[j];
                theNearest = theDistance;
            }
        }
        if (theTarget)
            this->Surround(theTarget);
    }
    return this->mTarget != NULL;
}

// FUNCTION: EXE 0x4a33bd
// Updates the herd: grows the young that are due, runs the current state's handler and schedules the next update
void EEHerd::Update(unsigned long inTime)
{
    unsigned long theDelay = 1000;
    if (this->mNumYoung != 0) {
        this->mNumYoung = 0;
        for (long i = this->mUnits.size() - 1; i >= 0; i--) {
            EEComplexUnit* theUnit = this->mUnits[i];
            if (!(theUnit->mGrowth < 1.0f))
                continue;
            this->mNumYoung++;
            if (theUnit->mGrowTime <= inTime)
                this->GrowUnit(theUnit);
        }
    }
    switch (this->mState) {
    case 0:
        theDelay = this->UpdateResting(inTime);
        break;
    case 2:
        theDelay = this->UpdateSurrounding(inTime);
        break;
    case 3:
        theDelay = this->UpdateAttacking(inTime);
        break;
    case 4:
        theDelay = this->UpdateIdle(inTime);
        break;
    case 7:
        theDelay = this->UpdateChasing(inTime);
        break;
    case 9:
        theDelay = this->UpdateHunting(inTime);
        break;
    case 0xa:
        theDelay = this->UpdateLeaving(inTime);
        break;
    }
    this->mNextUpdateTime = theDelay + inTime;
}

// FUNCTION: EXE 0x4a349c
// Returns whether a unit is a placed animal or fish of a known animal type
bool EEHerd::IsAnimalOnMap(EEUnit* inUnit)
{
    return (inUnit->mUnitClass->mFamily == kUnitFamilyAnimal || inUnit->mUnitClass->mFamily == kUnitFamilyFish) &&
        EEAnimal::Find(inUnit->mUnitClass->mUnitType) && inUnit->mTileX != -10000 && inUnit->mTileY != -10000;
}

// FUNCTION: EXE 0x4a34dc
// Reads the herd from a saved game (ids for its animal, player, leader, target and units); returns the first read
// error
long EEHerd::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = 0;
    unsigned long theCount = 0;
    unsigned long theValue = 0;
    theResult = inFile.Read(theValue);
    this->mAnimal = reinterpret_cast<EEAnimal*>(theValue);
    theValue = 0;
    if (theResult == 0) {
        theResult = inFile.Read(theValue);
        this->mPlayer = reinterpret_cast<EEPlayer*>(theValue);
    }
    theValue = -1;
    if (theResult == 0) {
        theResult = inFile.Read(theValue);
        this->mLeader = reinterpret_cast<EEComplexUnit*>(theValue);
    }
    theValue = -1;
    if (theResult == 0) {
        theResult = inFile.Read(theValue);
        this->mTarget = reinterpret_cast<EEComplexUnit*>(theValue);
        if (theResult == 0)
            theResult = this->mBounds2.Load(inFile);
        if (theResult == 0)
            theResult = inFile.Read(this->mID);
        if (theResult == 0)
            theResult = inFile.Read(this->mFB8);
        if (theResult == 0)
            theResult = inFile.Read(this->mBreeds);
        if (theResult == 0)
            theResult = inFile.Read(this->mAnimalID);
        if (theResult == 0)
            theResult = inFile.Read(this->mGrazeAreaID);
        if (theResult == 0)
            theResult = inFile.Read(this->mState);
        if (theResult == 0)
            theResult = inFile.Read(this->mBirthTimer);
        if (theResult == 0)
            theResult = inFile.Read(this->mLE4);
        if (theResult == 0)
            theResult = inFile.Read(this->mNextUpdateTime);
        if (theResult == 0)
            theResult = inFile.Read(this->mLD8);
        if (theResult == 0)
            theResult = inFile.Read(this->mRadiusSquared);
        if (theResult == 0)
            theResult = inFile.Read(this->mMaxUnits);
        if (theResult == 0)
            theResult = inFile.Read(this->mContinent);
        if (theResult == 0)
            theResult = inFile.Read(this->mFollowTime);
        if (theResult == 0)
            theResult = this->mCenter.Load(inFile);
        if (theResult == 0)
            theResult = this->mP94.Load(inFile);
    }
    theCount = 0;
    // an earlier read error is kept (the unit count's result used to replace it)
    if (theResult == 0)
        theResult = inFile.Read(theCount);
    if (theResult != 0)
        return theResult;
    this->mUnits.resize(theCount, 0);
    for (unsigned long i = 0; i < theCount; i++) {
        theValue = 0;
        if (theResult == 0) {
            theResult = inFile.Read(theValue);
            this->mUnits[i] = reinterpret_cast<EEComplexUnit*>(theValue);
        }
    }
    if (theResult == 0)
        theResult = inFile.Read(this->mHuntTime);
    if (theResult == 0)
        theResult = inFile.Read(this->mChaseTime);
    if (theResult == 0)
        theResult = inFile.Read(this->mLC8);
    return theResult;
}

// FUNCTION: EXE 0x4a3749
// Releases every herd animal (detaching those still in this herd) and forgets their looks
void EEHerd::ClearUnits()
{
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if (this->mUnits[i]->mHerd == this) {
            this->mUnits[i]->SetHerd(NULL);
        }
        this->mUnits[i]->Dereference();
    }
    this->mUnits.clear();
    this->mMedia.clear();
}

// FUNCTION: EXE 0x4a37a7
// Unloads and forgets the look recorded for a unit
void EEHerd::RemoveUnitMedia(unsigned long inKey)
{
    std::map<unsigned long, EEMedia*>::iterator theEntry = this->mMedia.find(inKey);
    if (theEntry != this->mMedia.end()) {
        theEntry->second->UnloadMedia();
        this->mMedia.erase(theEntry);
    }
}

// FUNCTION: EXE 0x4a37e4
// Drops the herd's reference to its leader
void EEHerd::ReleaseLeader()
{
    if (this->mLeader)
        this->mLeader->Dereference();
    this->mLeader = NULL;
}

// FUNCTION: EXE 0x4a37fb
// Drops the herd's reference to its target
void EEHerd::ReleaseTarget()
{
    if (this->mTarget)
        this->mTarget->Dereference();
    this->mTarget = NULL;
}

// FUNCTION: EXE 0x4a3812
// Removes an animal from the herd (leader, herd link, look), rebuilding the area only once the herd is empty;
// returns whether it was in the herd
bool EEHerd::RemoveUnit(EEComplexUnit* inUnit)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<EEComplexUnit*>::iterator theEntry = std::find(this->mUnits.begin(), this->mUnits.end(), inUnit);
    bool theResult = theEntry != this->mUnits.end();
    if (theEntry == this->mUnits.end())
        return theResult;
    this->mUnits.erase(theEntry);
    inUnit->Dereference();
    for (long i = this->mUnits.size() - 1; i >= 0; i--)
        theTiles.push_back(U2DPoint<long>(this->mUnits[i]->GetTileX(), this->mUnits[i]->GetTileY()));
    if (inUnit == this->mLeader)
        this->ReleaseLeader();
    if (inUnit->mHerd == this && inUnit->mHerd != NULL)
        inUnit->mHerd = NULL;
    this->RemoveUnitMedia(inUnit->GetUnitID());
    if (!gWorldClearing && theTiles.size() == 0)
        this->Create(theTiles);
    return theResult;
}

// NEAR: EEModel (mStats->mModel) is only forward-declared in ee/EEUnit.h, so its EEMedia base is a cast here
// FUNCTION: EXE 0x4a3967
// After a load, turns the saved ids back into player, animal, units, leader and target, restores the looks,
// rebuilds the area and recounts the young; returns 0
long EEHerd::RepairPointers(long, long)
{
    std::vector<EEComplexUnit*> theYoung;
    std::vector<U2DPoint<long> > theTiles;
    this->mPlayer = gWorld.GetPlayer(reinterpret_cast<unsigned long>(this->mPlayer));
    this->mAnimal = EEAnimal::sTable[reinterpret_cast<unsigned long>(this->mAnimal)];
    if (this->mAnimal)
        this->mRadiusSquared = this->mAnimal->GetHerdRadius() * this->mAnimal->GetHerdRadius();
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if (this->mUnits[i] != (EEComplexUnit*)-1) {
            unsigned long theID = reinterpret_cast<unsigned long>(this->mUnits[i]);
            this->mUnits[i] = dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit(theID)));
            if (this->mUnits[i]) {
                this->mUnits[i]->mReferenceCount++;
                this->mUnits[i]->SetHerd(this);
            }
        } else
            this->mUnits[i] = NULL;
        if (this->mUnits[i] == NULL)
            this->mUnits.erase(this->mUnits.begin() + i);
    }
    if (this->mUnits.size() > 0) {
        if (!this->mAnimal || this->mAnimal->GetAnimalID() != this->mUnits[0]->GetUnitClass()->mAnimalID)
            this->mAnimal = EEAnimal::Get(this->mUnits[0]->GetUnitClass()->mAnimalID);
    }
    if (this->mLeader != (EEComplexUnit*)-1) {
        this->mLeader = dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mLeader))));
        if (this->mLeader)
            this->mLeader->mReferenceCount++;
    } else
        this->mLeader = NULL;
    if (this->mAnimal) {
        for (long j = this->mUnits.size() - 1; j >= 0; j--) {
            this->SetUnitMedia((EEMedia*)this->mUnits[j]->GetStats()->mModel, this->mUnits[j]->GetUnitID());
            if (this->mUnits[j]->mB34F && this->mAnimal->mMedia)
                this->SetUnitMedia(this->mAnimal->mMedia, this->mUnits[j]->GetUnitID());
        }
        if (!this->mLeader && this->mAnimal->mHasLeader)
            this->PickLeader();
    }
    if (this->mTarget != (EEComplexUnit*)-1) {
        this->mTarget = dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mTarget))));
        if (this->mTarget)
            this->mTarget->mReferenceCount++;
    } else
        this->mTarget = NULL;
    if (this->mUnits.size() > 0)
        this->mContinent = gContinentManager.GetContinentID(this->mUnits[0]);
    for (long k = this->mUnits.size() - 1; k >= 0; k--)
        theTiles.push_back(U2DPoint<long>(this->mUnits[k]->GetTileX(), this->mUnits[k]->GetTileY()));
    this->Create(theTiles);
    this->GetYoungUnits(theYoung);
    this->mNumYoung = theYoung.size();
    return 0;
}

// FUNCTION: EXE 0x4a3d9c
// Spawns a newborn held by a chosen parent (when one is found) and schedules the next birth; returns whether born
bool EEHerd::GiveBirth(unsigned long inTime)
{
    EEComplexUnit* theParent = this->FindMother();
    bool theResult = false;
    if (theParent) {
        EEUnitClass* theClass = this->mAnimal->GetUnitClass();
        EEUnit* theUnit = theClass->CreateUnit(theParent->GetTileX(), theParent->GetTileY(),
                                               theParent->mPlayer->GetPlayerID(), 0, NULL);
        EEComplexUnit* theYoung = dynamic_cast<EEComplexUnit*>(theUnit);
        theYoung->SetGrowth(this->mAnimal->GetNewbornGrowth());
        theYoung->SetGrowTime(gPhysics->mGameTime + 4000);
        theYoung->SetTargetSearchTimer(theClass->mTargetSearchInterval);
        theParent->HoldUnit(theYoung);
        theResult = true;
    }
    this->mBirthTimer = gPhysics->GetRandom(this->mAnimal->GetMinBirthInterval(), this->mAnimal->GetMaxBirthInterval()) + inTime;
    return theResult;
}

// FUNCTION: EXE 0x4a3e7a
// Writes the herd to the saved game (ids for its animal, player, leader, target and units); returns the first write
// error
long EEHerd::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = 0;
    unsigned long theCount = 0;
    unsigned long theValue = this->mAnimal->GetAnimalID();
    theResult = outFile.Write(theValue);
    theValue = this->mPlayer->mPlayerID;
    if (theResult == 0)
        theResult = outFile.Write(theValue);
    theValue = this->mLeader == NULL ? -1 : this->mLeader->GetUnitID();
    if (theResult == 0)
        theResult = outFile.Write(theValue);
    theValue = this->mTarget == NULL ? -1 : this->mTarget->GetUnitID();
    if (theResult == 0) {
        theResult = outFile.Write(theValue);
        if (theResult == 0)
            theResult = this->mBounds2.Save(outFile);
        if (theResult == 0)
            theResult = outFile.Write(this->mID);
        if (theResult == 0)
            theResult = outFile.Write(this->mFB8);
        if (theResult == 0)
            theResult = outFile.Write(this->mBreeds);
        if (theResult == 0)
            theResult = outFile.Write(this->mAnimalID);
        if (theResult == 0)
            theResult = outFile.Write(this->mGrazeAreaID);
        if (theResult == 0)
            theResult = outFile.Write(this->mState);
        if (theResult == 0)
            theResult = outFile.Write(this->mBirthTimer);
        if (theResult == 0)
            theResult = outFile.Write(this->mLE4);
        if (theResult == 0)
            theResult = outFile.Write(this->mNextUpdateTime);
        if (theResult == 0)
            theResult = outFile.Write(this->mLD8);
        if (theResult == 0)
            theResult = outFile.Write(this->mRadiusSquared);
        if (theResult == 0)
            theResult = outFile.Write(this->mMaxUnits);
        if (theResult == 0)
            theResult = outFile.Write(this->mContinent);
        if (theResult == 0)
            theResult = outFile.Write(this->mFollowTime);
        if (theResult == 0)
            theResult = this->mCenter.Save(outFile);
        if (theResult == 0)
            theResult = this->mP94.Save(outFile);
    }
    theCount = this->mUnits.size();
    // an earlier write error is kept (the unit count's result used to replace it)
    if (theResult == 0)
        theResult = outFile.Write(theCount);
    if (theResult != 0)
        return theResult;
    for (unsigned long i = 0; i < theCount; i++) {
        theValue = this->mUnits[i]->mUnitID;
        if (theResult == 0)
            theResult = outFile.Write(theValue);
    }
    if (theResult == 0)
        theResult = outFile.Write(this->mHuntTime);
    if (theResult == 0)
        theResult = outFile.Write(this->mChaseTime);
    if (theResult == 0)
        theResult = outFile.Write(this->mLC8);
    return theResult;
}

// FUNCTION: EXE 0x4a412e
// Makes herd animals graze at once: all of them, or the first inIndex after an optional shuffle
void EEHerd::ResetGrazeTimes(long inIndex, bool inShuffle)
{
    std::vector<EEComplexUnit*> theUnits(this->mUnits);
    if (inShuffle)
        EEAIRandomShuffle(theUnits.begin(), theUnits.end());
    if (inIndex >= 0 && inIndex < (long)theUnits.size())
        theUnits.erase(theUnits.begin() + inIndex, theUnits.end());
    for (long i = theUnits.size() - 1; i >= 0; i--)
        theUnits[i]->SetGrazeTime(0);
}

// FUNCTION: EXE 0x4a41fb
// Records the look for a unit, loading it and unloading the one it replaces
void EEHerd::SetUnitMedia(EEMedia* inMedia, unsigned long inKey)
{
    std::map<unsigned long, EEMedia*>::iterator theEntry = this->mMedia.find(inKey);
    if (theEntry != this->mMedia.end()) {
        if (inMedia != theEntry->second) {
            theEntry->second->UnloadMedia();
            theEntry->second = inMedia;
            inMedia->LoadMedia();
        }
    } else {
        inMedia->LoadMedia();
        this->mMedia.insert(std::make_pair(inKey, inMedia));
    }
}

// FUNCTION: EXE 0x4a4263
// Returns whether an animal may join: same player and animal type, room left, and no tiles yet or (inCheckRange)
// on the herd's continent within the herd radius of its hull
bool EEHerd::CanTake(EEComplexUnit* inUnit, bool inCheckRange)
{
    U2DPoint<long> theClosest;
    U2DPoint<long> theTile(inUnit->mTileX, inUnit->mTileY);
    bool theResult = false;
    if (inUnit->mPlayer == this->mPlayer && EEHerd::IsAnimalOnMap(inUnit) && this->mAnimalID == inUnit->mUnitClass->mAnimalID && this->mUnits.size() < this->mMaxUnits) {
        theResult = this->mTiles.Size() == 0;
        if (!theResult && inCheckRange == true) {
            unsigned short theID = gContinentManager.GetContinentID(theTile.mX, theTile.mY);
            if (theID == this->mContinent || this->mContinent == 0) {
                theResult = this->mHull.BoundedContains(theTile);
                if (!theResult) {
                    theClosest = this->GetClosestHullPoint(theTile);
                    theResult = theClosest.DistanceToSquared(theTile) <= this->mRadiusSquared;
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4a4351
// Places the herd on a ring around a target, sending the nearest animal to each slot, and makes it the target
void EEHerd::Surround(EEComplexUnit* inTarget)
{
    std::vector<EEComplexUnit*> theUnits;
    std::vector<EEFormationSlot> theSlots;
    EEFInwardCircle* theFormation = dynamic_cast<EEFInwardCircle*>(EEFormation::Create(6));
    GE3DPoint thePosition;
    GE3DPoint theUnitPosition;
    inTarget->GetPosition(thePosition);
    theUnits = this->mUnits;
    theFormation->mCenter.mX = thePosition.mX;
    theFormation->mCenter.mY = thePosition.GetY();
    theFormation->mCenter.mZ = thePosition.mZ;
    theFormation->mMinRadius = gPhysics->GetRandom(1.25f, 2.75f);
    theFormation->Place(theUnits, theSlots);
    while (theUnits.size() > 0 && theSlots.size() > 0) {
        EEComplexUnit* theBest = NULL;
        long theBestIndex = 0;
        float theBestDistance = -1.0f;
        for (long i = theUnits.size() - 1; i >= 0; i--) {
            theUnits[i]->GetPosition(theUnitPosition);
            float theDistance = theSlots.back().mPosition.DistanceToSquared(theUnitPosition);
            if (theBestDistance != -1.0f && !(theDistance < theBestDistance))
                continue;
            theBest = theUnits[i];
            theBestIndex = i;
            theBestDistance = theDistance;
        }
        theUnits.erase(theUnits.begin() + theBestIndex);
        EEUGLocation* theGoal = new EEUGLocation(&theSlots.back().mPosition, true, false, true,
                                                 theSlots.back().mHeading, false, false);
        if (!this->GiveGoal(theBest, theGoal, 0, kActionPrepareToMove) && theGoal)
            delete theGoal;
        theSlots.pop_back();
    }
    inTarget->mReferenceCount++;
    this->ReleaseTarget();
    this->mTarget = inTarget;
    this->mCenter = U2DPoint<long>(inTarget->mTileX, inTarget->mTileY);
    theFormation->Dereference();
}

// FUNCTION: EXE 0x4a45b7
// Chase state: while the target lives and the chase timer has run out, re-chases it once it moves 3 tiles;
// otherwise ends the chase, sends the herd grazing and restarts the timer; returns 1000 ms
unsigned long EEHerd::UpdateChasing(unsigned long inTime)
{
    U2DPoint<long> theTile;
    bool theFollowing = true;
    if (this->mTarget && this->mTarget->mHitPoints > 0 && this->mChaseTime <= inTime) {
        theTile = this->mTarget->GetTile();
        if (theTile.DistanceToSquaredF(this->mCenter) > 9.0f) {
            theFollowing = this->Chase();
            this->mCenter = theTile;
        }
    } else
        theFollowing = false;
    if (theFollowing)
        return 1000;
    this->ReleaseTarget();
    this->ResetGrazeTimes(this->mUnits.size(), true);
    this->mState = 0;
    this->mChaseTime = gPhysics->GetRandom(this->mAnimal->GetMinHuntInterval(), this->mAnimal->GetMaxHuntInterval()) + inTime;
    return 1000;
}

// FUNCTION: EXE 0x4a467b
// Hunt state: while the target lives and the hunt timer runs, re-circles it when it changes tile; ends the hunt
// (grazing, timer restarted) only when circling fails, otherwise the state stays; returns 1000 ms
unsigned long EEHerd::UpdateHunting(unsigned long inTime)
{
    U2DPoint<long> theTile;
    if (!this->mTarget || !(this->mTarget->mHitPoints > 0) || !(this->mHuntTime > inTime))
        return 1000;
    long theY = this->mTarget->mTileY;
    theTile.mX = this->mTarget->mTileX;
    theTile.mY = theY;
    if (theTile == this->mCenter)
        return 1000;
    bool theFollowing = this->MoveTo(theTile, gPhysics->GetRandom(2L, 5L));
    this->mCenter = theTile;
    if (theFollowing)
        return 1000;
    this->ReleaseTarget();
    this->ResetGrazeTimes(this->mUnits.size(), true);
    this->mState = 0;
    this->mHuntTime = gPhysics->GetRandom(this->mAnimal->GetMinHuntInterval(), this->mAnimal->GetMaxHuntInterval()) + inTime;
    return 1000;
}

// FUNCTION: EXE 0x4a4757
// State 4 handler: does nothing; returns 1000 ms
unsigned long EEHerd::UpdateIdle(unsigned long inTime)
{
    return 1000;
}

// FUNCTION: EXE 0x4a475f
// Attack state: orders animals without a goal to attack the live target; else sends some grazing, sets the
// follow timer to the rest interval from now and rests; returns 1000 ms
unsigned long EEHerd::UpdateAttacking(unsigned long inTime)
{
    if (this->mTarget && this->mTarget->mHitPoints > 0) {
        for (long i = this->mUnits.size() - 1; i >= 0; i--) {
            if (this->mUnits[i]->GetGoal() != NULL)
                continue;
            EEUGAttackUnit* theGoal = new EEUGAttackUnit(this->mUnits[i], this->mTarget, true, false);
            if (!this->GiveGoal(this->mUnits[i], theGoal, 0, kActionInitialAttackState) && theGoal)
                delete theGoal;
        }
    } else {
        this->ResetGrazeTimes(gPhysics->Fn82D66(this->mUnits), true);
        this->mFollowTime = gPhysics->GetRandom(this->mAnimal->GetMinRestInterval(), this->mAnimal->GetMaxRestInterval()) + inTime;
        this->mState = 0;
    }
    return 1000;
}

// FUNCTION: EXE 0x4a4854
// Resting state: grazes idle animals, gives birth when due and may start a chase (animal 0xb), a hunt
// (animal 0xa) or, for predators, following the nearest herd; returns 1000 ms
unsigned long EEHerd::UpdateResting(unsigned long inTime)
{
    if (!this->mIsActive)
        return 1000;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        EEComplexUnit* theUnit = this->mUnits[i];
        long theCommand = theUnit->mCommand ? theUnit->mCommand->mType : 0;
        if (!theUnit->mIsDisabled && theUnit->mTileX != -10000 && (theCommand == kActionIdle || theCommand == kActionCircle) &&
            this->mState == 0 && theUnit->mStats->mIsInitialized && theUnit->mGrazeTime < inTime && theUnit->mHolder == NULL)
            this->Graze(theUnit, inTime);
    }
    if (this->mBreeds && this->mUnits.size() < this->mMaxUnits && this->mBirthTimer < inTime)
        this->GiveBirth(inTime);
    if (this->mState == 0 && this->mAnimal->GetAnimalID() == 0xb && this->mChaseTime <= inTime) {
        if (gPhysics->mRandom.Generate() <= 0.45f && this->Chase() && this->mTarget) {
            this->mCenter = U2DPoint<long>(this->mTarget->mTileX, this->mTarget->mTileY);
            this->mState = 7;
        }
        this->mChaseTime = gPhysics->GetRandom(this->mAnimal->GetMinHuntInterval(), this->mAnimal->GetMaxHuntInterval()) + inTime;
    }
    if (this->mState == 0 && this->mAnimal->GetAnimalID() == 0xa && this->mHuntTime <= inTime) {
        if (gPhysics->mRandom.Generate() <= 0.45f && this->Hunt() && this->mTarget) {
            this->mCenter = U2DPoint<long>(this->mTarget->mTileX, this->mTarget->mTileY);
            this->mState = 9;
        }
        this->mHuntTime = gPhysics->GetRandom(this->mAnimal->GetMinHuntInterval(), this->mAnimal->GetMaxHuntInterval()) + inTime;
    }
    if (!this->mAnimal->mIsPredator || !(this->mFollowTime <= inTime))
        return 1000;
    if (this->FollowNearestHerd())
        this->mState = 2;
    this->mFollowTime = gPhysics->GetRandom(this->mAnimal->GetMinFollowInterval(), this->mAnimal->GetMaxFollowInterval()) + inTime;
    return 1000;
}

// FUNCTION: EXE 0x4a4aa2
// Leaving state: sends each animal still on the map to an off-map tile near a random map-edge area; switches
// to state 0xb once all are off the map; returns 1000 ms
unsigned long EEHerd::UpdateLeaving(unsigned long inTime)
{
    EEAIAStarBounds theBounds;
    EEMap* theMap = gWorld.mMap;
    long theOffMap = 0;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        EEComplexUnit* theUnit = this->mUnits[i];
        bool theLeaving = false;
        if (theMap->GetTile(UMFloor(theUnit->mPhysicalModel.GetLocation().GetX()),
                            UMFloor(theUnit->mPhysicalModel.GetLocation().GetY()))) {
            for (long j = theUnit->mGoalQueues[0].mGoals.size() - 1; j >= 0 && !theLeaving; j--) {
                EEUGLocation* theGoal = dynamic_cast<EEUGLocation*>(theUnit->mGoalQueues[0].mGoals[j]);
                if (theGoal)
                    theLeaving = theMap->GetTile(UMFloor(theGoal->mLocation.GetX()), UMFloor(theGoal->mLocation.GetY())) == NULL;
            }
            if (!theLeaving) {
                U2DPoint<long> theExit;
                EEAIArea* theArea = gContinentManager.GetRandomContinent(kContinentCategoryMapEdge, gPhysics->mRandom);
                if (theArea) {
                    U2DPoint<long> thePoint = theArea->GetRandomPoint();
                    theBounds.mLeft = thePoint.GetX();
                    theBounds.mRight = thePoint.GetX();
                    theBounds.mTop = thePoint.GetY();
                    theBounds.mBottom = thePoint.GetY();
                    theBounds.Grow(10, false);
                    U2DPoint<long> theCorner = theBounds.GetTopLeft();
                    if (!theMap->GetTile(theCorner.GetX(), theCorner.GetY()))
                        theExit = theBounds.GetTopLeft();
                    else {
                        theCorner = theBounds.GetTopRight();
                        if (!theMap->GetTile(theCorner.GetX(), theCorner.GetY()))
                            theExit = theBounds.GetTopRight();
                        else {
                            theCorner = theBounds.GetBottomRight();
                            if (!theMap->GetTile(theCorner.GetX(), theCorner.GetY()))
                                theExit = theBounds.GetBottomRight();
                            else {
                                theCorner = theBounds.GetBottomLeft();
                                if (!theMap->GetTile(theCorner.GetX(), theCorner.GetY()))
                                    theExit = theBounds.GetBottomLeft();
                                else
                                    theExit.mX = theExit.mY = -10000;
                            }
                        }
                    }
                } else
                    theExit.mX = theExit.mY = -10000;
                EEUGLocation* theExitGoal = new EEUGLocation((float)theExit.GetX() + 0.5f, (float)theExit.GetY() + 0.5f, 0.0f,
                                                             true, true, false, 0.0f, false, false);
                if (!this->GiveGoal(theUnit, theExitGoal, 0, kActionPrepareToMove) && theExitGoal)
                    delete theExitGoal;
            }
        } else
            theOffMap++;
    }
    if (theOffMap == this->mUnits.size())
        this->mState = 0xb;
    return 1000;
}

// FUNCTION: EXE 0x4a4e76
// Surround state: follows the target as it moves and attacks once the follow timer expires; rests again when
// the target is gone; returns 2000 ms
unsigned long EEHerd::UpdateSurrounding(unsigned long inTime)
{
    if (this->mTarget && this->mTarget->mHitPoints > 0) {
        if (U2DPoint<long>(this->mTarget->mTileX, this->mTarget->mTileY) != this->mCenter)
            this->Surround(this->mTarget);
        if (this->mFollowTime <= inTime) {
            this->Attack(this->mTarget, this->mUnits);
            this->mState = 3;
        }
    } else {
        this->ReleaseTarget();
        this->ResetGrazeTimes(this->mUnits.size(), true);
        this->mState = 0;
    }
    return 2000;
}

// Returns a random map tile inside the hull (16 tries), else EEAIArea's random point
// The vtable's +0x20 (0x43c2e8): EETown's body, ICF-folded into EETown::GetRandomPoint 0x4ac203 (no address of its own;
// earlier attributed to EEAIArea). Its place in the unit is unknown.
U2DPoint<long> EEHerd::GetRandomPoint()
{
    U2DPoint<long> theResult;
    bool theFound = false;
    EEMap* theMap = gWorld.mMap;
    if (theMap != NULL) {
        for (long i = 15; i >= 0 && !theFound; i--) {
            theResult.mX = this->mHull.GetBounds().mLeft +
                           gPhysics->GetRandom(0L, this->mHull.GetBounds().mRight - this->mHull.GetBounds().mLeft);
            theResult.mY = this->mHull.GetBounds().mTop +
                           gPhysics->GetRandom(0L, this->mHull.GetBounds().mBottom - this->mHull.GetBounds().mTop);
            if (theMap->GetTileAtLocation(theResult))
                theFound = this->mHull.BoundedContains(theResult);
        }
    }
    if (!theFound)
        theResult = EEAIArea::GetRandomPoint();
    return theResult;
}

// Compiler-generated for this unit (template and inline COMDATs after the last member; names in analysis/exe/names.txt).
// Not paired yet: 0xa4f48 (vector<EEUnitVectorItem>::operator[], expanded inline here).
// TEMPLATE: EXE 0x4a4f09
// TEMPLATE: EXE 0x4a4f34
// TEMPLATE: EXE 0x4a4f58
// TEMPLATE: EXE 0x4a4f5f
// TEMPLATE: EXE 0x4a4f93
// TEMPLATE: EXE 0x4a5004
// TEMPLATE: EXE 0x4a5068
// TEMPLATE: EXE 0x4a509a
// TEMPLATE: EXE 0x4a5238
// TEMPLATE: EXE 0x4a526b
// TEMPLATE: EXE 0x4a52eb
// TEMPLATE: EXE 0x4a5313
// TEMPLATE: EXE 0x4a536b
// TEMPLATE: EXE 0x4a53da
// TEMPLATE: EXE 0x4a53e8
// Named COMDATs of this unit (compiled here; names in analysis/exe/names.txt)
// TEMPLATE: EXE 0x4a1910
// TEMPLATE: EXE 0x4a1937
// TEMPLATE: EXE 0x4a53ae
