// C:\EEX\Empire Earth\AI Thread\EECPASGAirTransport.cpp (file name inferred: no __FILE__). 0x440cd..0x4597f (unit map
// edges, not verified yet).
#include <windows.h>
#include <vector>
#include <set>
#include <math.h>
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
#include "lle/FSRestorable.h"
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EECPILocation.h"
#include "ee/EECPASTargetUnit.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPASGAir.h"
#include "ee/EECPASGLand.h"
#include "ee/EECPASGHelicopter.h"
#include "ee/EECPASGSpaceship.h"
#include "ee/EECPASGTransport.h"
#include "ee/EECPASGAirTransport.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIContinent.h"
#include "ee/EECPASGSea.h"
#include "ee/EEUGRust.h"
#include "ee/EECPAAirTargetFilters.h"

// 16-byte aircraft target predicate EECPAAirTargetFilter, built by EECPASGAir::CollectTargets at
// 0x441f64..0x441f89 and called by the area collector at 0x443eca (provisional names)


// 16-byte sea target predicate EECPASeaTargetFilter, built at 0x44de3b..0x44de71 (provisional names; header
// placement inferred)


// 24-byte land target predicate EECPALandTargetFilter, built at 0x4476ad..0x4476db (provisional names; header
// placement inferred)


// 16-byte helicopter and spaceship target predicates, built at 0x445a82..0x445aa3 and 0x44f460..0x44f486
// (provisional names)



#include "ee/EEAIArea.h"
#include "ee/EETown.h"

// Creates an air transport sub-group for the attack group, with no land group to carry yet
EECPASGAirTransport::EECPASGAirTransport(EECPAttackGroup* inGroup, long inKind) : EECPASGAir(inGroup, inKind)
{
    this->mLand = NULL;
    this->mOwner = this;
}

// Destroys the air transport sub-group (base members only)
EECPASGAirTransport::~EECPASGAirTransport()
{
}

// Sets the task as an air sub-group does and flags every unit as needing new orders
void EECPASGAirTransport::SetTask(const EECPAGTask& inTask)
{
    EECPASGAir::SetTask(inTask);
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        this->mUnits[i]->mNeedsOrders = true;
    }
}

// Counts the units carried by all transports in the sub-group
long EECPASGAirTransport::CountCargo()
{
    long theResult = 0;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        theResult += this->mUnits[i]->mUnit->GetNumCargoUnits();
    }
    return theResult;
}

// Picks a random player among itself and (if asked) its allies with towns, then a random point in a random town
// of the first one as the new rally point; returns the rally point (unchanged if that player has no town)
EETileLocation EECPASGAirTransport::GetRallyPoint(bool inIncludeAllies)
{
    std::vector<EEPlayer*> thePlayers;
    std::vector<EETown*> theTowns;
    thePlayers.push_back(this->mPlayer2);
    if (inIncludeAllies == true) {
        for (long i = this->mPlayer2->mAllies.size() - 1; i >= 0; --i) {
            if (this->mPlayer2->mAllies[i] != NULL &&
                !(this->mPlayer2->mAllies[i])->mObserverMode &&
                !(this->mPlayer2->mAllies[i])->mTowns.empty())
                thePlayers.push_back(this->mPlayer2->mAllies[i]);
        }
    }
    PermuteValues(thePlayers, UMFloatToLong(gPhysics->mRandom.GenerateUnit()));
    theTowns = thePlayers.front()->mTowns;
    if (theTowns.empty())
        return this->mRallyPoint.mTile;
    PermuteValues(theTowns, UMFloatToLong(gPhysics->mRandom.GenerateUnit()));
    EETown* theTown = theTowns.front();
    if (theTown != NULL) {
        EETileLocation theTile;
        theTile = theTown->GetRandomPoint();
        if (theTile.mX != 0 || theTile.GetY() != 0)
            this->mRallyPoint.mTile = theTile;
    }
    return this->mRallyPoint.mTile;
}

// Launches the transports: every unit retargets now and needs orders, flies to a location or attack task, and updates
void EECPASGAirTransport::Launch(unsigned long inTime)
{
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        this->mUnits[i]->mNextRetargetTime = inTime;
        this->mUnits[i]->mNeedsOrders = true;
    }
    if (this->mTask.mIsActive && (this->mTask.mType == kAGTaskLocation || (this->mTask.mType > 9 && this->mTask.mType <= kAGTaskAttackWonder)))
        this->SetDestination(this->mTask.mLocation);
    this->UpdateUnits(inTime);
}

// Takes requested units and regroups; without an active task, releases targets, flags units for orders and zeroes
// the on-map units' flight time unless units were just taken
void EECPASGAirTransport::GoIdle()
{
    bool theHadRequest = this->mRequest != NULL;
    if (this->mRequest != NULL) {
        this->TakeRequestedUnits();
        EECPASGAir::RegroupHere();
    }
    if (this->mTask.mIsActive)
        return;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        EEComplexUnit* theUnit = this->mUnits[i]->mUnit;
        (*(this->mUnits.begin() + i))->ReleaseTarget();
        (*(this->mUnits.begin() + i))->mNeedsOrders = true;
        if (!theHadRequest && gWorld.GetMap()->GetTile(theUnit->mTileX, theUnit->mTileY) != NULL)
            theUnit->SetFlightTime(0);
    }
}

// Recalls every active transport that has a flight time (zeroes it or clears its holder's goals), clears its
// goals, then requests removal
void EECPASGAirTransport::RequestRemoval()
{
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if (this->mUnits[i]->IsDone())
            continue;
        if (this->mUnits[i]->GetUnit()->GetUnitClass()->mFlightTime == 0)
            continue;
        if (this->mUnits[i]->GetUnit()->mContainer == NULL)
            this->mUnits[i]->GetUnit()->SetFlightTime(0);
        else
            this->mUnits[i]->GetUnit()->mContainer->ClearGoalQueue(0);
        this->mUnits[i]->GetUnit()->ClearGoalQueue(0);
    }
    EECPAttackSubGroup::RequestRemoval();
}

// Switches state: remove flies to the rally point and recalls, load boards the land group, release frees the
// land group and sets a return task to a new rally point; other states go to the air sub-group
void EECPASGAirTransport::SetState(long inState, long inTime)
{
    unsigned long theOldState = this->mState;
    this->mState = inState;
    switch (this->mState) {
    case kASGStateRemove:
        this->ReturnToRallyPoint();
        this->RequestRemoval();
        break;
    case kASGStateLoad:
        this->LoadCargo();
        break;
    case kASGStateCarryCargo:
    case kASGStateRedirectCargo:
        break;
    case kASGStateReleaseCargo:
        if (this->mLand != NULL) {
            if (this->mLand->mTransport == this)
                this->mLand->ReleaseTransport();
            EECPASGTransport::ReleaseLand();
        }
        this->GetRallyPoint(gPhysics->GetRandom(0L, 1L) != 0);
        this->ReturnToRallyPoint();
        this->mTask.mLocation = this->mRallyPoint;
        this->mTask.mType = kAGTaskReturn;
        this->mTask.mIsActive = true;
        break;
    default:
        this->mState = theOldState;
        EECPASGAir::SetState(inState, inTime);
        break;
    }
}

// Asks the land group to board the active transports and marks the cargo stage as loading
void EECPASGAirTransport::LoadCargo()
{
    std::vector<EEComplexUnit*> theUnits;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if (!this->mUnits[i]->IsDone()) {
            EEComplexUnit* theUnit = this->mUnits[i]->mUnit;
            theUnits.push_back(theUnit);
        }
    }
    if (theUnits.size() > 0 && this->mLand != NULL)
        this->mLand->BoardTransports(&theUnits);
    this->mCargoStage = 1;
}

// Runs the transport state machine (next update in 2 s): scans targets, updates units, then steps through idle,
// wait, launch, load, carry and release, ending in removal or idle
void EECPASGAirTransport::Update(long inTime)
{
    if (this->mNextScanTime <= inTime && this->mUnits.size() > 0)
        this->ScanTargets(1);
    this->UpdateUnits(inTime);
    bool theFailed = false;
    switch (this->mState) {
    case kASGStateIdle:
        theFailed = !this->TestCondition(kASGConditionHasUnits, inTime);
        if (theFailed)
            this->SetState(kASGStateWaitForUnits, inTime);
        if (!theFailed) {
            if (this->TestCondition(kASGConditionShouldReturn, inTime))
                this->SetState(kASGStateRemove, inTime);
            else if (this->TestCondition(kASGConditionTaskValid, inTime))
                this->SetState(kASGStateLaunch, inTime);
            else
                this->SetState(kASGStateIdle, inTime);
        }
        break;
    case kASGStateWaitForUnits:
        if (this->TestCondition(kASGConditionRequestEmpty, inTime))
            this->SetState(kASGStateRemove, inTime);
        else if (this->TestCondition(kASGConditionRequestFilled, inTime))
            this->SetState(kASGStateIdle, inTime);
        break;
    case kASGStateRemove:
         break;
    case kASGStateReturnToRallyPoint:
         break;
    case kASGStateLaunch:
        theFailed = !this->TestCondition(kASGConditionTaskValid, inTime);
        if (theFailed)
            this->SetState(kASGStateRemove, inTime);
        if (!theFailed && this->TestCondition(kASGConditionTaskValid, inTime))
            this->SetState(kASGStateLoad, inTime);
        break;
    case kASGStateLoad:
        theFailed = !this->TestCondition(kASGConditionHasCargo, inTime);
        if (theFailed)
            this->SetState(kASGStateIdle, inTime);
        if (!theFailed && this->TestCondition(kASGConditionAllAboard, inTime))
            this->SetState(kASGStateCarryCargo, inTime);
        break;
    case kASGStateCarryCargo:
    case kASGStateRedirectCargo:
        theFailed = !this->TestCondition(kASGConditionHasCargo, inTime);
        if (theFailed)
            this->SetState(kASGStateIdle, inTime);
        if (!theFailed && this->TestCondition(kASGConditionAllUnloaded, inTime))
            this->SetState(kASGStateReleaseCargo, inTime);
        break;
    case kASGStateReleaseCargo:
        if (this->TestCondition(kASGConditionShouldReturn, inTime))
            this->SetState(kASGStateRemove, inTime);
        else
            this->SetState(kASGStateIdle, inTime);
        break;
    }
    this->mNextUpdateTime = inTime + 2000;
}

// Loads the air sub-group and then the transport data from a saved game; returns the first error
long EECPASGAirTransport::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EECPASGAir::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == 0) {
        theResult = EECPASGTransport::Load(inMajorVersion, inMinorVersion, inFile);
    }
    return theResult;
}

// Sends the sub-group to its next waypoint if that tile is on the map
void EECPASGAir::MoveToDestination()
{
    if (gWorld.GetMap()->GetTileAtLocation(this->mNextWaypoint.mTile) != NULL)
        this->SetDestination(this->mNextWaypoint);
}

// Switches to the load state if the land group still has units not aboard
void EECPASGAirTransport::StartLoading()
{
    std::vector<EEComplexUnit*> theUnits;
    if (this->mLand == NULL || this->mUnits.size() <= 0)
        return;
    this->mLand->GetUnitsNotAboard(theUnits);
    if (theUnits.size() > 0)
        this->SetState(kASGStateLoad, gPhysics->mGameTime);
}

// Restores the air sub-group and then the transport links after a load; returns the first error
long EECPASGAirTransport::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    long theResult = EECPASGAir::Restore(inMajorVersion, inMinorVersion);
    if (theResult == 0) {
        theResult = EECPASGTransport::Restore(inMajorVersion, inMinorVersion);
    }
    return theResult;
}

// Sets the land group the transports carry
void EECPASGAirTransport::SetLand(EECPASGLand* inLand)
{
    EECPASGTransport::SetLand(inLand);
}

// Tests a transport condition: task valid and has-cargo mean the land group has units; also should-return, all
// aboard, all unloaded; any-aboard is always false; others go to the base
bool EECPASGAirTransport::TestCondition(long inState, long inTime)
{
    bool theResult = false;
    switch ((unsigned long)inState) {   // unsigned compares (ja), as the base 0x4604cf
    case kASGConditionRequestFilled:
        theResult = this->IsRequestFilled();
        break;
    case kASGConditionTaskValid:
    case 2000:
    case 2003:
    case kASGConditionHasCargo:
        theResult = this->mLand != NULL && this->mLand->GetNumUnits() > 0;
        break;
    case kASGConditionTaskInvalid:
        theResult = this->mLand == NULL;
        break;
    case kASGConditionShouldReturn:
        theResult = this->mLand == NULL || this->mUnits.size() == 0 || (this->mTask.mIsActive && this->mTask.mType == kAGTaskReturn);
        break;
    case kASGConditionAllAboard:
        theResult = this->mLand != NULL && this->mLand->CountUnitsAboard() == this->mLand->GetNumUnits();
        break;
    case 4001:
    case kASGConditionAnyAboard:
        break;
    case kASGConditionAllUnloaded:
        theResult = this->mLand == NULL || (this->mLand->CountUnitsAboard() == 0 && this->mCargoStage == 2);
        break;
    default:
        theResult = EECPAttackSubGroup::TestCondition(inState, inTime);
        break;
    }
    return theResult;
}

// Orders each transport to unload at the location; empty ones and those unable to unload there (the tile is marked
// to avoid) move to a random tile within 7 of it instead; marks the cargo stage as unloading
void EECPASGAirTransport::UnloadAt(const EECPILocation& inLocation)
{
    EEAIAStarBounds theBounds;
    theBounds.Set(inLocation.mTile);
    std::vector<EEComplexUnit*> theFailed;
    long i;
    for (i = this->mUnits.size() - 1; i >= 0; i--) {
        EEComplexUnit* theUnit = this->mUnits[i]->mUnit;
        EEUGRust* theGoal = EEUGRust::NewUnloadTransport(theUnit, inLocation.mTile.mX,
                                                              inLocation.mTile.GetY(), true);
        bool theSkip = theUnit->GetNumCargoUnits() == 0;
        if (!theSkip) {
            theSkip = !theGoal->CanUnloadAtDestination();
            if (theSkip && this->mLand != NULL)
                this->mLand->AvoidTile(inLocation.mTile);
        }
        if (theSkip) {
            if (theGoal != NULL)
                delete theGoal;
            theFailed.push_back(theUnit);
        } else if (!this->GiveGoal(theUnit, theGoal, 0, kActionPrepareToMove) && theGoal != NULL) {
            delete theGoal;
        }
    }
    if (theFailed.size() != 0) {
        theBounds.Grow(7, true);
        for (i = theFailed.size() - 1; i >= 0; i--) {
            EEComplexUnit* theUnit = theFailed[i];
            EEUnitGoal* theGoal = EEUGRust::NewLocation((float)gPhysics->GetRandom(theBounds.mLeft, theBounds.mRight),
                (float)gPhysics->GetRandom(theBounds.mTop, theBounds.mBottom), 0.0f, true, true, false, 0.0f,
                false, false);
            if (!this->GiveGoal(theUnit, theGoal, 0, kActionPrepareToMove) && theGoal != NULL)
                delete theGoal;
        }
    }
    this->mCargoStage = 2;
}

// Accepts a live unit of another player that passes the land target filter
bool EECPALandTargetFilter::operator()(EEComplexUnit* inUnit)
{
    if (inUnit->mPlayer == this->mPlayer || inUnit->mHitPoints <= 0)
        return false;
    return this->Accept(inUnit);
}

// Accepts visible, targetable units of the player or its enemies except runways, foundations, harmless Gaia units and
// walls or gates (unless allowed and reachable); near a ready transport only badly hurt units; wonder tasks narrow it
bool EECPALandTargetFilter::Accept(EEComplexUnit* inUnit)
{
    bool theResult = false;
    EETileDistanceLess theDistance(this->mCenter);
    if (inUnit->mIsDisabled)
        return false;
    if (inUnit->mPlayer->mPlayerID != this->mPlayerID &&
        (!this->mIncludeEnemies || inUnit->mPlayer == this->mPlayer ||
         this->mPlayer->mEnemies[inUnit->mPlayer->mPlayerID] == 0))
        return false;
    if (!EEUUGetIsUnitClassTargetable(NULL, inUnit->mUnitClass))
        return false;
    bool theCanTarget = true;
    if (inUnit->mUnitClass->IsLandBuilding()) {
        if (inUnit->mUnitClass->mUnitType == kUnitTypeAirportRunway || inUnit->IsFoundation()) {
            theCanTarget = false;
        } else if (inUnit->mUnitClass->mIsWall && !inUnit->mUnitClass->mIsTower) {
            if (inUnit->mUnitClass->mCategory != kUnitCategoryGate) {
                if (!this->mAllowWalls) {
                    theCanTarget = false;
                } else if (this->mTileReady && theDistance.DistanceSquared(inUnit->GetTile()) >= 9) {
                    unsigned short theSector = gContinentManager.GetPathSubSectorID(
                        this->mCenter.mX, this->mCenter.GetY());
                    theCanTarget = theSector == gContinentManager.GetPathSubSectorID(inUnit);
                }
            } else {
                if (!this->mAllowGates) {
                    theCanTarget = false;
                } else if (this->mTileReady && theDistance.DistanceSquared(inUnit->GetTile()) > 9) {
                    unsigned short theSector = gContinentManager.GetPathSubSectorID(
                        this->mCenter.mX, this->mCenter.GetY());
                    theCanTarget = theSector == gContinentManager.GetPathSubSectorID(inUnit);
                }
            }
        }
    }
    if (theCanTarget && inUnit->mUnitClass->mIsGaiaPlaceable &&
        (inUnit->mUnitClass->mAttack == 0 || inUnit->GetGoal() == NULL || inUnit->GetGoal()->mKind != kGoalAttackUnit))
        theCanTarget = false;
    if (theCanTarget) {
        theResult = true;
        if ((inUnit->mIsCloakable ? inUnit->mIsCloaked : false) || this->mPlayer->mComputerPlayer->mDifficulty != kDifficultyHard)
            theResult = EEIsSeenBy(inUnit, this->mPlayer, true);
        if (theResult && this->mGroup->mTransport != NULL && this->mGroup->mTransportStage == 1) {
            EECPAttackSubGroup* theTransport = dynamic_cast<EECPAttackSubGroup*>(this->mGroup->mTransport);
            if (theTransport->IsRequestFilled() && !(this->mGroup->DistanceSquaredTo(theTransport) >= 100.0f)) {
                if ((float)inUnit->mHitPoints / inUnit->mStats->mMaxHitPoints > 0.15f)
                    theResult = false;
                else
                    theResult = true;
            }
        }
    }
    if (this->mGroup->mTask.mMode == kAGModeWonder && theResult && this->mGroup->mTask.mUnit != NULL) {
        theResult = inUnit == this->mGroup->mTask.mUnit;
        if (!theResult) {
            EETechTreeNode* theNode = this->mGroup->mTask.mUnit->GetTechTreeNode();
            if (theNode != NULL) {
                if (theNode->mData->mKind == 4) {
                    EEPlayer* theTaskPlayer = this->mGroup->mTask.mUnit->mPlayer;
                    if ((theTaskPlayer != inUnit->mPlayer &&
                         inUnit->mPlayer->mEnemies[theTaskPlayer->mPlayerID] != 0) ||
                        inUnit->mUnitClass->IsLandBuilding()) {
                        theResult = false;
                        if (inUnit->DistanceSquaredTo(this->mGroup->mTask.mUnit) > 36.0f) {
                            if (inUnit->mUnitClass->mIsWall && !inUnit->mUnitClass->mIsTower) {
                                if (inUnit->mUnitClass->mCategory != kUnitCategoryGate) {
                                    if (this->mAllowWalls &&
                                        (!this->mTileReady || theDistance.DistanceSquared(inUnit->GetTile()) < 9))
                                        theResult = true;
                                } else if (this->mAllowGates &&
                                           (!this->mTileReady || theDistance.DistanceSquared(inUnit->GetTile()) <= 9)) {
                                    theResult = true;
                                }
                            }
                        } else {
                            theResult = !inUnit->mIsCitizen ||
                                (inUnit->GetGoal() != NULL && inUnit->GetGoal()->mKind == kGoalBuildRepair);
                        }
                    } else {
                        theResult = !inUnit->mIsCitizen ||
                            (inUnit->GetGoal() != NULL && inUnit->GetGoal()->mKind == kGoalBuildRepair);
                    }
                } else {
                    theResult = true;
                }
            }
        }
    }
    return theResult;
}

// Accepts a live unit of another player that passes the air target filter
bool EECPAAirTargetFilter::operator()(EEComplexUnit* inUnit)
{
    if (inUnit->mPlayer == this->mPlayer || inUnit->mHitPoints <= 0)
        return false;
    return this->Accept(inUnit);
}

// Accepts enabled, targetable units of the player or its enemies except walls, runways, foundations and harmless
// Gaia units; cloaked units, or any unit below hard difficulty, must be seen by the player
bool EECPAAirTargetFilter::Accept(EEComplexUnit* inUnit)
{
    bool theResult = false;
    EEUnitClass* theClass = inUnit->mUnitClass;
    if (!inUnit->mIsDisabled &&
        (inUnit->mPlayer->mPlayerID == this->mPlayerID ||
         (this->mIncludeEnemies && inUnit->mPlayer != this->mPlayer &&
          this->mPlayer->mEnemies[inUnit->mPlayer->mPlayerID] != 0)) &&
        EEUUGetIsUnitClassTargetable(NULL, theClass)) {
        if (theClass->IsLandBuilding() &&
            ((theClass->mIsWall && !theClass->mIsTower) ||
             theClass->mUnitType == kUnitTypeAirportRunway || inUnit->IsFoundation())) {
            theResult = false;
        } else if (theClass->mIsGaiaPlaceable &&
                   (theClass->mAttack == 0 || inUnit->GetGoal() == NULL ||
                    inUnit->GetGoal()->mKind != kGoalAttackUnit)) {
            theResult = false;
        } else {
            theResult = true;
            if ((inUnit->mIsCloakable ? inUnit->mIsCloaked : false) ||
                this->mPlayer->mComputerPlayer->mDifficulty != kDifficultyHard)
                theResult = EEIsSeenBy(inUnit, this->mPlayer, true);
        }
    }
    return theResult;
}

// Accepts a live unit of another player that passes the sea target filter
bool EECPASeaTargetFilter::operator()(EEComplexUnit* inUnit)
{
    if (inUnit->mPlayer == this->mPlayer || inUnit->mHitPoints <= 0)
        return false;
    return this->Accept(inUnit);
}

// Accepts enabled, targetable units of the player or its enemies except foundations, runways, harmless Gaia units and
// walls (towers and, above easy, shore walls only with shore attacks); visibility as for air; or the alternate rule
bool EECPASeaTargetFilter::Accept(EEComplexUnit* inUnit)
{
    bool theResult = false;
    EEUnitClass* theClass = inUnit->mUnitClass;
    if (!this->mAlternate) {
        if (!inUnit->mIsDisabled &&
            (inUnit->mPlayer->mPlayerID == this->mPlayerID ||
             (this->mIncludeEnemies && inUnit->mPlayer != this->mPlayer &&
              this->mPlayer->mEnemies[inUnit->mPlayer->mPlayerID] != 0)) &&
            EEUUGetIsUnitClassTargetable(NULL, theClass)) {
            theResult = true;
            if (theClass->IsLandBuilding()) {
                theResult = !inUnit->IsFoundation();
                if (theResult) {
                    if (theClass->mIsWall) {
                        if (!theClass->mIsTower || !this->mAllowShore) {
                            theResult = false;
                            if (this->mAllowShore && this->mGroup->mPlayer2->mDifficulty != kDifficultyEasy) {
                                const EEAIContinent* theContinent = gContinentManager.GetContinentAt(inUnit);
                                if (theContinent != NULL) {
                                    U2DPoint<long> theTile(inUnit->mTileX, inUnit->mTileY);
                                    theResult = std::binary_search(theContinent->GetShoreTiles().begin(),
                                        theContinent->GetShoreTiles().end(), theTile);
                                }
                            }
                        }
                    } else
                        theResult = theClass->mUnitType != kUnitTypeAirportRunway;
                }
            }
            if (theResult && !(theClass->mIsGaiaPlaceable &&
                    (theClass->mAttack == 0 || inUnit->GetGoal() == NULL || inUnit->GetGoal()->mKind != kGoalAttackUnit))) {
                theResult = true;
                if ((inUnit->mIsCloakable ? inUnit->mIsCloaked : false) ||
                    this->mPlayer->mComputerPlayer->mDifficulty != kDifficultyHard)
                    theResult = EEIsSeenBy(inUnit, this->mPlayer, true);
            } else
                theResult = false;
        }
    } else
        theResult = this->AcceptAlternate(inUnit);
    return theResult;
}

// Accepts enabled, targetable units of the player or its enemies except runways, foundations, buildings when naval
// targets are off and harmless Gaia units; cloaked units, or any unit below hard difficulty, must be seen
bool EECPASeaTargetFilter::AcceptAlternate(EEComplexUnit* inUnit)
{
    bool theResult = false;
    EEUnitClass* theClass = inUnit->mUnitClass;
    if (!inUnit->mIsDisabled &&
        (inUnit->mPlayer->mPlayerID == this->mPlayerID ||
         (this->mIncludeEnemies && inUnit->mPlayer != this->mPlayer &&
          this->mPlayer->mEnemies[inUnit->mPlayer->mPlayerID] != 0)) &&
        EEUUGetIsUnitClassTargetable(NULL, theClass)) {
        if (theClass->IsLandBuilding() &&
            (theClass->mUnitType == kUnitTypeAirportRunway || inUnit->IsFoundation() || !this->mAllowNaval)) {
            theResult = false;
        } else if (theClass->mIsGaiaPlaceable &&
                   (theClass->mAttack == 0 || inUnit->GetGoal() == NULL ||
                    inUnit->GetGoal()->mKind != kGoalAttackUnit)) {
            theResult = false;
        } else {
            theResult = true;
            if ((inUnit->mIsCloakable ? inUnit->mIsCloaked : false) ||
                this->mPlayer->mComputerPlayer->mDifficulty != kDifficultyHard)
                theResult = EEIsSeenBy(inUnit, this->mPlayer, true);
        }
    }
    return theResult;
}

// Accepts a live unit of another player that passes the helicopter target filter
bool EECPAHelicopterTargetFilter::operator()(EEComplexUnit* inUnit)
{
    if (inUnit->mPlayer == this->mPlayer || inUnit->mHitPoints <= 0)
        return false;
    return this->Accept(inUnit);
}

// Accepts targets as the air filter does; for a wonder task (kind-4 tech node) only the wonder itself and
// non-building units not hostile to its owner
bool EECPAHelicopterTargetFilter::Accept(EEComplexUnit* inUnit)
{
    bool theResult = false;
    EEUnitClass* theClass = inUnit->mUnitClass;
    if (!inUnit->mIsDisabled &&
        (inUnit->mPlayer->mPlayerID == this->mPlayerID ||
         (this->mIncludeEnemies && inUnit->mPlayer != this->mPlayer &&
          this->mPlayer->mEnemies[inUnit->mPlayer->mPlayerID] != 0)) &&
        EEUUGetIsUnitClassTargetable(NULL, theClass)) {
        if (theClass->IsLandBuilding() &&
            ((theClass->mIsWall && !theClass->mIsTower) ||
             theClass->mUnitType == kUnitTypeAirportRunway || inUnit->IsFoundation())) {
            theResult = false;
        } else if (theClass->mIsGaiaPlaceable &&
                   (theClass->mAttack == 0 || inUnit->GetGoal() == NULL ||
                    inUnit->GetGoal()->mKind != kGoalAttackUnit)) {
            theResult = false;
        } else {
            theResult = true;
            if ((inUnit->mIsCloakable ? inUnit->mIsCloaked : false) ||
                this->mPlayer->mComputerPlayer->mDifficulty != kDifficultyHard)
                theResult = EEIsSeenBy(inUnit, this->mPlayer, true);
        }
    }
    if (this->mGroup->mTask.mMode == kAGModeWonder && theResult && this->mGroup->mTask.mUnit != NULL) {
        theResult = inUnit == this->mGroup->mTask.mUnit;
        if (!theResult) {
            EETechTreeNode* theNode = this->mGroup->mTask.mUnit->GetTechTreeNode();
            if (theNode != NULL) {
                if (theNode->mData->mKind == 4) {
                    EEPlayer* theTaskPlayer = this->mGroup->mTask.mUnit->mPlayer;
                    theResult = !((theTaskPlayer != inUnit->mPlayer &&
                                   inUnit->mPlayer->mEnemies[theTaskPlayer->mPlayerID] != 0) ||
                                  theClass->IsLandBuilding());
                } else {
                    theResult = true;
                }
            }
        }
    }
    return theResult;
}

// Accepts a live unit of another player that passes the spaceship target filter
bool EECPASpaceshipTargetFilter::operator()(EEComplexUnit* inUnit)
{
    if (inUnit->mPlayer == this->mPlayer || inUnit->mHitPoints <= 0)
        return false;
    return this->Accept(inUnit);
}

// Accepts targets by the same rules as the sea filter's normal mode
bool EECPASpaceshipTargetFilter::Accept(EEComplexUnit* inUnit)
{
    bool theResult = false;
    EEUnitClass* theClass = inUnit->mUnitClass;
    if (!inUnit->mIsDisabled &&
        (inUnit->mPlayer->mPlayerID == this->mPlayerID ||
         (this->mIncludeEnemies && inUnit->mPlayer != this->mPlayer &&
          this->mPlayer->mEnemies[inUnit->mPlayer->mPlayerID] != 0)) &&
        EEUUGetIsUnitClassTargetable(NULL, theClass)) {
        theResult = true;
        if (theClass->IsLandBuilding()) {
            theResult = !inUnit->IsFoundation();
            if (theResult) {
                if (theClass->mIsWall) {
                    if (!theClass->mIsTower || !this->mAllowShore) {
                        theResult = false;
                        if (this->mAllowShore && this->mGroup->mPlayer2->mDifficulty != kDifficultyEasy) {
                            const EEAIContinent* theContinent = gContinentManager.GetContinentAt(inUnit);
                            if (theContinent != NULL) {
                                U2DPoint<long> theTile(inUnit->mTileX, inUnit->mTileY);
                                theResult = std::binary_search(theContinent->GetShoreTiles().begin(),
                                    theContinent->GetShoreTiles().end(), theTile);
                            }
                        }
                    }
                } else
                    theResult = theClass->mUnitType != kUnitTypeAirportRunway;
            }
        }
        if (theResult && !(theClass->mIsGaiaPlaceable &&
                (theClass->mAttack == 0 || inUnit->GetGoal() == NULL || inUnit->GetGoal()->mKind != kGoalAttackUnit))) {
            theResult = true;
            if ((inUnit->mIsCloakable ? inUnit->mIsCloaked : false) ||
                this->mPlayer->mComputerPlayer->mDifficulty != kDifficultyHard)
                theResult = EEIsSeenBy(inUnit, this->mPlayer, true);
        } else
            theResult = false;
    }
    return theResult;
}

// Does nothing: transports do not attack
void EECPASGAirTransport::AttackWith(EECPAttackSubGroupUnit*,EEComplexUnit*,EETileCoordinate*)
{
}

// Saves the sub-group and then the transport data; returns the first error
long EECPASGAirTransport::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EECPAttackSubGroup::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == 0)
        theResult = EECPASGTransport::Save(inMajorVersion, inMinorVersion, outFile);
    return theResult;
}

// Sums the transport capacity of the sub-group's units
long EECPASGAirTransport::GetCargoCapacity()
{
    long theSum = 0;
    for (long i = this->mUnits.size() - 1; i >= 0; i--)
        theSum += this->mUnits[i]->GetUnit()->GetUnitClass()->mTransportCapacity;
    return theSum;
}

// Removes the unit; while loading, restarts loading once if any active unit remains (not while the world is cleared
// or a game loads); returns whether the unit was removed
bool EECPASGAirTransport::RemoveUnit(EEComplexUnit* inUnit)
{
    bool theResult = EECPAttackSubGroup::RemoveUnit(inUnit);
    if (theResult && this->mCargoStage == 1 && gPhysics != NULL) {
        for (unsigned long i = 0; i < this->mUnits.size(); i++) {
            if (!this->mUnits[i]->IsDone() && !gWorldClearing && !gLoadingSavedGame) {
                this->StartLoading();   // once, not once per remaining unit
                break;
            }
        }
    }
    return theResult;
}

// Releases the land group and shuts the sub-group down
void EECPASGAirTransport::Shutdown()
{
    EECPASGTransport::ReleaseLand();
    EECPAttackSubGroup::Shutdown();
}

// Returns the share of units in danger, at least 0.75 if any of them carries cargo; 0 while redirecting cargo
float EECPASGAirTransport::GetDangerRatio(long inArg1)
{
    float theResult = 0.0f;
    long theCount = 0;
    long theCargo = 0;
    long theTotalCargo = 0;
    if (this->mState != kASGStateRedirectCargo) {
        for (long i = this->mUnits.size() - 1; i >= 0; i--) {
            // The original also adds up the total cargo (0x450a3f, 0x450a44) before testing each unit, then never uses it
            theTotalCargo += this->mUnits[i]->GetUnit()->GetNumCargoUnits();
            if (this->mUnits[i]->IsInDanger()) {
                theCount++;
                theCargo += this->mUnits[i]->GetUnit()->GetNumCargoUnits();
            }
        }
        if (theCount > 0) {
            theResult = (float)theCount / this->mUnits.size();
            if (theCargo > 0 && 0.75f > theResult)
                theResult = 0.75f;
        }
    }
    return theResult;
}

// Sets every path cost weight to zero
void EECPASGAirTransport::GetPathCosts(void* inFrom, void* inTo, EECPASGWeights& outWeights)
{
    outWeights.Fill(0.0f);
}
