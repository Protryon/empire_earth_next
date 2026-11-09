// C:\EEX\Empire Earth\Units\EEUCLandable.cpp (file name inferred: no __FILE__; the class name).
// The unit class of the
// aircraft that land (airplanes, bombers, the animals' and planet ships' bases): 0x213c60..0x216c48 (after
// EEUCLand.cpp; EEUCLandCitizen.cpp follows). One unit per EEUC* class in alphabetical order. Unit flags: the exe's
// module set. Its ctor is inline (EEUnitClass.cpp's factory). Order: the landing (ReturnToAirport), the take-off point
// (RouteDockWaypoint), Virtual24, Virtual2C, the approach (FlyToGoal), the ground height (GetGroundHeight), GetNextFlightPoint, Virtual74,
// EEComplexUnit::SetInitialContactLocation, Virtual78, the bomb drop (IsBombOnTarget), the carrier approach (ApproachCarrier, TakeOffFromCarrier), the
// flight state (GetNextFlightState), Virtual88, the runway approaches (ApproachRunway and its four wrappers, TakeOffFromRunway and its four),
// IsAwayFromGoal, VirtualA0, VirtualE0, VirtualB8, EEComplexUnit::SetOpacityChain. EEUCAirAnimal.cpp's bodies are the twins of
// several of these (same sizes).
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <typeinfo>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/SESoundManager.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicObject.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGExplore.h"
#include "ee/EEUGPatrol.h"
#include "ee/EEUGAttackGround.h"
#include "ee/EEUGUnloadTransport.h"
#include "ee/EEUCTransport.h"
#include "ee/EEUCLandable.h"

// 0x447038..0x447050: the unit's named float constants (one .rdata block per unit) (provisional names)
static const float kLandableNone = -10000.0f;   // 0x447038 (mInitialContactHeading: no landing height)
static const float kLandableCeiling = 12.0f;    // 0x44703c
static const float kLandableAngle = 0.5235988f; // 0x447040 (the approach's 30 degrees)
static const float kLandableAhead = 2.0f;       // 0x447044
static const float kLandableLow = 0.1f;         // 0x447048 (the touch-down height)

// cdecl helpers (EEUCHelicopter.cpp declares the first the same way) (provisional names)
void EEFindLandingPoint(EEComplexUnit* inUnit, GE3DPoint* ioTarget);
// 0x24485b: the enemy unit near (inX, inY) within inRange that inUnit can attack (provisional name)
EEUnit* EEFindTargetNear(EEComplexUnit* inUnit, long inX, long inY, float inRange);

// FUNCTION: EXE 0x613c60
// Takes off from the ground: rolls 3 ahead, climbs 7 ahead to 3/4 cruising height (plus the ground height when the
// ground is higher), then flies to the goal; returns true, doing nothing without an air traffic control
bool EEUCLandable::ReturnToAirport(EEComplexUnit* inUnit)
{
    GE3DPoint thePoint;
    GEOrientation theOrientation;
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    if (inUnit == NULL || inUnit->mPlayer->mAirTraffic[inUnit->mUnitClass->mAirTrafficType - 1] == 0)
        return true;
    float theGround = inUnit->mTile != NULL ? inUnit->mTile->mHeight : 0.0f;
    inUnit->DropPath();
    theTransformation.Apply(GE3DPoint(0.0f, 3.0f, 0.0f), thePoint);
    this->AddWaypointAt(inUnit, thePoint.GetX(), thePoint.GetY(), theGround, true);
    theTransformation.Apply(GE3DPoint(0.0f, 7.0f, 0.0f), thePoint);
    float theHeight = this->GetCruiseAltitude() * 0.75f;
    if (theGround > theHeight)
        theHeight = theHeight + theGround;
    this->AddWaypointAt(inUnit, thePoint.GetX(), thePoint.GetY(), theHeight, true);
    thePoint.SetZ(theHeight);
    theTransformation.ConvertToOrientation(theOrientation);
    inUnit->mOrientation = theOrientation;
    this->FlyToGoal(thePoint, inUnit);
    return true;
}

// FUNCTION: EXE 0x613db5
// Drops the path and adds a waypoint at the home carrier's take-off point (its tag point 2; its centre without tags)
void EEUnitClass::RouteDockWaypoint(EEComplexUnit* inUnit)
{
    GE3DPoint theTarget;
    EEComplexUnit* theCarrier = inUnit->mHomeBase;
    EEUnitClassUpgrade* theStats = theCarrier->GetStats();
    EETagPointList* theTags = theCarrier->GetModel()->mTagPoints[theCarrier->mAnimation];
    float theX = theCarrier->mPhysicalModel.GetLocation().GetX();
    float theY = theCarrier->mPhysicalModel.GetLocation().GetY();
    float theScale = theStats->GetScale();
    long theIndex = UMFloatToLong(inUnit->GetStats()->GetMaxVelocity() / inUnit->GetStats()->GetMaxAngularVelocity());
    inUnit->DropPath();
    if (theTags == NULL || theTags->mPoints.size() == 0) {
        theTarget.SetX(theX);
        theTarget.SetZ(1.0f);
        theTarget.SetY(theY);
    } else {
        GETransformation theTransformation(false);
        theTransformation = theCarrier->mPhysicalModel.GetLocation();
        EETagPointList::Scale(theTransformation, theScale);
        theTags->mPoints[2]->GetPoint(inUnit->mPhysicalModel.GetFrame(), theTransformation, theTarget);
    }
    this->AddWaypointAt(inUnit, theTarget.GetX(), theTarget.GetY(), theTarget.GetZ(), true);
}

// FUNCTION: EXE 0x613ed2
// Returns the height to fly at towards a point: its height above the ground, over the highest tile corner ahead
// (the turning circle while circling, else the line to it), at most 12
float EEUCLandable::GetHeightAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{
    GE3DPoint thePosition;
    GEVector theStep;
    GEVector theOffset;
    std::vector<EETileCoordinate> theTiles;
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    thePosition.SetX(theLocation.GetX());
    EEMap* theMap = ::gWorld.mMap;
    EEUnitClassUpgrade* theStats = inUnit->GetStats();
    thePosition.SetY(theLocation.GetY());
    thePosition.SetZ(theLocation.GetZ());
    float theHeight = inLocation.GetZ();
    float theBase;
    EEMapTile* theTile = ::gWorld.mMap->GetTile(UMFloor(inLocation.GetX()), UMFloor(inLocation.GetY()));
    if (theTile != NULL && theTile->GetHeight() >= 0.0f)
        theBase = theHeight - theTile->GetHeight();
    else
        theBase = theHeight;
    unsigned long theState = inUnit->mCurrentPhysicsOrder;
    if (theState == 0 || theState == 1) {
        float theDistance = theStats->GetMaxVelocity() / theStats->GetMaxAngularVelocity();
        theStep.SetX(theState != 0 ? -theDistance : theDistance);
        theLocation.Apply(theStep, theOffset);
        thePosition += theOffset;
        EEGetTilesNearPoint(thePosition, kLandableAhead + theDistance, theTiles);
    } else {
        EEGetTileLine(UMFloor(thePosition.GetX()), UMFloor(thePosition.GetY()), UMFloor(inLocation.GetX()),
                   UMFloor(inLocation.GetY()), (std::vector<U2DPoint<long> >*)&theTiles, 1, 0);
    }
    for (std::vector<EETileCoordinate>::iterator theCoordinate = theTiles.begin(); theCoordinate != theTiles.end();
         theCoordinate++) {
        unsigned long theX = theCoordinate->mX;
        unsigned long theY = theCoordinate->mY;
        if (theMap->GetTile(theX, theY) == NULL)
            continue;
        float theZ = theMap->mTerrainMesh.GetZ(theX, theY) + theBase;
        if (theZ > theHeight)
            theHeight = theZ;
        theZ = theMap->mTerrainMesh.GetZ(theX + 1, theY) + theBase;
        if (theZ > theHeight)
            theHeight = theZ;
        theZ = theMap->mTerrainMesh.GetZ(theX, theY + 1) + theBase;
        if (theZ > theHeight)
            theHeight = theZ;
        theZ = theMap->mTerrainMesh.GetZ(theX + 1, theY + 1) + theBase;
        if (theZ > theHeight)
            theHeight = theZ;
    }
    if (theHeight > kLandableCeiling)
        theHeight = kLandableCeiling;
    return theHeight;
}

// FUNCTION: EXE 0x6141a3
// Gets the sound parameters, giving a moving plane's sound a Doppler pitch shift against the camera
void EEUCLandable::GetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
                              float* outPitch)
{
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    GE3DPoint thePosition;
    GE3DPoint theCamera(::gWorld.mTransformation.GetX(), ::gWorld.mTransformation.GetY(),
                        ::gWorld.mTransformation.GetZ());
    float theSpeed = theUnit->mVelocity;
    GEVector theVelocity(0.0f, theSpeed, 0.0f);
    GEVector theHeading;
    GEVector theToCamera;
    EEUnitClass::GetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    if (!(*outVolume > 0.0f) || !(theSpeed > 0.0f))
        return;
    thePosition.SetX(theUnit->mPhysicalModel.GetLocation().GetX());
    thePosition.SetY(theUnit->mPhysicalModel.GetLocation().GetY());
    thePosition.SetZ(theUnit->mPhysicalModel.GetLocation().GetZ());
    theToCamera.SetX(theCamera.GetX() - thePosition.GetX());
    theToCamera.SetY(theCamera.GetY() - thePosition.GetY());
    theToCamera.SetZ(theCamera.GetZ() - thePosition.GetZ());
    if (theToCamera.GetX() >= -0.0001f && theToCamera.GetX() <= 0.0001f && theToCamera.GetY() >= -0.0001f &&
        theToCamera.GetY() <= 0.0001f && theToCamera.GetZ() >= -0.0001f && theToCamera.GetZ() <= 0.0001f) {
        theToCamera.SetX(0.0f);
        theToCamera.SetY(0.0f);
        theToCamera.SetZ(0.0f);
    } else
        theToCamera.Normalize();
    theUnit->mPhysicalModel.GetLocation().Apply(theVelocity, theHeading);
    thePosition += theHeading;
    if (theHeading.GetX() >= -0.0001f && theHeading.GetX() <= 0.0001f && theHeading.GetY() >= -0.0001f &&
        theHeading.GetY() <= 0.0001f && theHeading.GetZ() >= -0.0001f && theHeading.GetZ() <= 0.0001f) {
        theHeading.SetX(0.0f);
        theHeading.SetY(0.0f);
        theHeading.SetZ(0.0f);
    } else
        theHeading.Normalize();
    float theCosine = theHeading.DotProduct(theToCamera);
    float theRatio = theVelocity.GetY() * 0.05f;
    float theGamma = (float)sqrt(1.0f - theVelocity.GetY() * theVelocity.GetY() * 0.0025f);
    *outPitch = theGamma / (1.0f - theRatio * ((theRatio + theCosine) / (1.0f - theRatio * theCosine)));
}

// FUNCTION: EXE 0x614402
// Paths a plane from a point to its goal's location (move, patrol, explore, attack ground, unload, attack unit)
// at cruising height; an explore goal without a target picks a new area first
void EEUCLandable::FlyToGoal(GE3DPoint& inPoint, EEComplexUnit* inUnit)
{
    GE3DPoint theTarget;
    EEUnit* theUnit;
    EEUnitGoal* theGoal = inUnit->mGoalState;
    EEUGLocation* theLocation = dynamic_cast<EEUGLocation*>(theGoal);
    EEUGAttackUnit* theAttack = dynamic_cast<EEUGAttackUnit*>(theGoal);
    EEUGExplore* theExplore = dynamic_cast<EEUGExplore*>(theGoal);
    EEUGPatrol* thePatrol = dynamic_cast<EEUGPatrol*>(theGoal);
    EEUGAttackGround* theGround = dynamic_cast<EEUGAttackGround*>(theGoal);
    EEUGUnloadTransport* theUnload = dynamic_cast<EEUGUnloadTransport*>(theGoal);
    if (theLocation != NULL) {
        theTarget.SetX(theLocation->mLocation.GetX());
        theTarget.SetY(theLocation->mLocation.GetY());
    } else if (thePatrol != NULL) {
        theTarget.SetX((float)thePatrol->mTarget->GetTileX() + 0.5f);
        theTarget.SetY((float)thePatrol->mTarget->GetTileY() + 0.5f);
    } else if (theExplore != NULL) {
        theExplore->GetTileCenter(theTarget);
        if (theTarget.GetX() <= 0.5f && theTarget.GetY() <= 0.5f) {
            inUnit->StartAction(kActionFindUnexploredArea);
            theExplore->Step(inUnit);
            theExplore->GetTileCenter(theTarget);
            long theKind = inUnit->mUnitClass->mAirTrafficType;
            if (theKind == 2 || theKind == 4)
                inUnit->StartAction(kActionContinueToTaxi);
            else
                inUnit->StartAction(kActionTakeoff);
        }
    } else if (theGround != NULL) {
        theTarget.SetX(theGround->mTarget.GetX());
        theTarget.SetY(theGround->mTarget.GetY());
    } else if (theUnload != NULL)
        theUnload->GetGoalLocation(theTarget);
    else {
        theUnit = theAttack->mTarget;
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        if (inUnit->mUnitClass->mProjectileClass != NULL)
            EEFindDropLocation(inUnit, &theUnit, &theTarget, NULL);
        else if (theComplex != NULL) {
            theTarget.SetX(theComplex->mPhysicalModel.GetLocation().GetX());
            theTarget.SetY(theComplex->mPhysicalModel.GetLocation().GetY());
        } else {
            theTarget.SetX((float)theUnit->mUnitClass->GetTileSize() * 0.5f + (float)theUnit->GetTileX());
            theTarget.SetY((float)theUnit->mUnitClass->GetTileSize() * 0.5f + (float)theUnit->GetTileY());
        }
    }
    theTarget.SetZ(this->GetCruiseAltitude());
    EEFindLandingPoint(inUnit, &theTarget);
    this->PathToward(inUnit, inPoint, theTarget, true);
}

// FUNCTION: EXE 0x614612
// Gives the ground height under a point (the closest tile's off the map; 0 under water)
void EEUCLandable::GetGroundHeight(GE3DPoint& inPoint, float* outHeight)
{
    long theX;
    long theY;
    EEMapTile* theTile = ::gWorld.mMap->GetTile(UMFloor(inPoint.GetX()), UMFloor(inPoint.GetY()));
    if (theTile == NULL)
        theTile = ::gWorld.mMap->GetClosestTile(&inPoint, &theX, &theY);
    *outHeight = theTile->GetHeight();
    if (*outHeight < 0.0f)
        *outHeight = 0.0f;
}

// FUNCTION: EXE 0x6146a9
// Gets the unit's next waypoint and the climb to it; returns nonzero when there is none
long EEUCLandable::GetNextFlightPoint(EEComplexUnit* inUnit, GE3DPoint& outTarget, float* outClimb)
{
    GE3DPoint thePosition;
    inUnit->GetPosition(thePosition);
    long theResult = inUnit->GetNextWaypointPoint(outTarget);
    if (theResult == 0)
        *outClimb = outTarget.GetZ() - thePosition.GetZ();
    return theResult;
}

// FUNCTION: EXE 0x6146f0
// Handles a plane's new action: taxi, take-off and landing approaches (runway or carrier), landing requests,
// emergencies, secondary targets, bombing runs, initial contact, and death (falls, rolling at its turn rate)
void EEUCLandable::ActionChanged(EEComplexUnit* inUnit)
{
    GE3DPoint theTarget;
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          inUnit->mPhysicalModel.GetLocation().GetZ());
    GEOrientation theOrientation;
    EEAirTrafficControl* theAirTraffic = inUnit->mPlayer->mAirTraffic[inUnit->mUnitClass->mAirTrafficType - 1];
    EEUnitGoal* theGoal = inUnit->mGoalState;
    EEUnit* theEnemy;
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionBrake:
        if (inUnit->mUnitClass->mAirTrafficType == 2 || inUnit->mUnitClass->mAirTrafficType == 4)
            this->ApproachCarrier(inUnit, false);
        inUnit->SetCurrentPhysicsAction(0);
        inUnit->SetCurrentPhysicsOrder(5);
        inUnit->SetHasArrived(false);
        inUnit->SetF220(0.0f);
        break;
    case kActionBoardTransport:
        inUnit->SetCurrentPhysicsAction(6);
        if (inUnit->mHomeBase == NULL)
            return;
        theAirTraffic->RemovePlane(inUnit->mHomeBase, inUnit);
        inUnit->mHomeBase->LoadCargo(inUnit, 1, 0);   // the carrier takes the plane aboard
        inUnit->SetHomeBase(NULL);
        inUnit->SetIsCommandable(true);
        inUnit->SetHeight(0.0f);
        break;
    case kActionContinueToTaxi:
        if ((inUnit->mUnitClass->mAirTrafficType == 2 || inUnit->mUnitClass->mAirTrafficType == 4) && inUnit->mIsClearanceGranted == true) {
            this->TakeOffFromCarrier(inUnit);
            inUnit->SetCurrentPhysicsAction(0);
            inUnit->SetCurrentPhysicsOrder(5);
            inUnit->SetHasArrived(false);
            inUnit->SetF220(0.0f);
            break;
        }
    case kActionContinueToClimb:
    case kActionContinueToDescend:
    case kActionContinueToBrake:
    case kActionContinueToWaitForLandingClearance:
        inUnit->SetCurrentPhysicsOrder(5);
        inUnit->NextWaypoint();
        break;
    case kActionTaxi:
    case kActionClimb:
    case kActionDescend:
        inUnit->SetCurrentPhysicsAction(0);
        inUnit->SetCurrentPhysicsOrder(5);
        inUnit->SetHasArrived(false);
        inUnit->SetF220(0.0f);
        break;
    case kActionFly:
    case kActionContinueToFly: {
        if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionFly) {
            inUnit->SetCurrentPhysicsAction(0);
            inUnit->SetCurrentPhysicsOrder(5);
            inUnit->SetHasArrived(false);
            inUnit->SetF220(0.0f);
        } else {
            inUnit->SetCurrentPhysicsOrder(5);
            inUnit->NextWaypoint();
        }
        GE3DPoint theNext;
        float theIndex;
        if (inUnit->GetPathStart(theNext, theIndex) == 0 &&
            (!inUnit->mUnitClass->mIsParatrooperBomber || inUnit->GetNumCargoUnits() != 0) &&
            !gContinentManager.CanGetTo(inUnit, (long)theNext.GetX(), (long)theNext.GetY())) {
            inUnit->DropPath();
            EEFindLandingPoint(inUnit, &theNext);
            inUnit->AddWaypoint(theNext, false);
        }
        break;
    }
    case kActionEmergencyTakeoff: {
        inUnit->SetCurrentPhysicsAction(0);
        EEComplexUnit* theBase = theAirTraffic->FindNearestAirport(inUnit);
        if ((long)inUnit->mWaypoints.size() <= 3 || theBase == NULL) {
            inUnit->Kill(true, false, false);
            return;
        }
        inUnit->SetHomeBase(theBase);
        inUnit->StartAction(kActionRequestLanding);
        this->ActionChanged(inUnit);
        break;
    }
    case kActionLand:
    case kActionRecalculateLanding:
        if (inUnit->mUnitClass->mAirTrafficType == 1 || inUnit->mUnitClass->mAirTrafficType == 3) {
            switch (inUnit->mHomeBase->mFacing) {
            case 0:
                this->ApproachRunwayPosY(inUnit);
                break;
            case 1:
                this->ApproachRunwayNegY(inUnit);
                break;
            case 2:
                this->ApproachRunwayPosX(inUnit);
                break;
            default:
                this->ApproachRunwayNegX(inUnit);
                break;
            }
        } else
            this->ApproachCarrier(inUnit, (inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionLand);
        inUnit->SetCurrentPhysicsAction(0);
        inUnit->SetCurrentPhysicsOrder(5);
        inUnit->SetAnimation(0xd, -1, 0);
        if (inUnit->GetNextWaypointPoint(thePosition) == 0)
            inUnit->SetHeight(this->GetHeightAt(inUnit, thePosition));
        break;
    case kActionSearchForSecondaryTarget: {
        inUnit->SetCurrentPhysicsAction(6);
        EEUGAttackUnit* theAttack = dynamic_cast<EEUGAttackUnit*>(theGoal);
        if (theAttack == NULL)
            return;
        EEUnit* theTargetUnit = theAttack->mTarget;
        if (theTargetUnit != NULL) {
            long theX = theTargetUnit->GetTileX();
            long theY;
            if (theX == -10000) {
                theX = inUnit->GetTileX();
                theY = inUnit->GetTileY();
            } else
                theY = theTargetUnit->GetTileY();
            theEnemy = EEFindTargetNear(inUnit, theX, theY, 3.0f);
            if (theEnemy != NULL && inUnit->mBehavior->mCanTargetEnemies) {
                theAttack->SetTarget(theEnemy);
                return;
            }
        } else {
            inUnit->ClearGoals(0);
            break;
        }
        if (theAttack->mTarget->GetTileX() != -10000) {
            EEUGLocation* theMove = new EEUGLocation((float)theAttack->mTarget->GetTileX() + 0.5f,
                                                     (float)theAttack->mTarget->GetTileY() + 0.5f, this->GetCruiseAltitude(),
                                                     theAttack->GetB1C(), true, false, 0.0f, false, false);
            inUnit->SetGoal(theMove, 0, 0);
            inUnit->StartAction(theMove->GetStartAction());
            this->ActionChanged(inUnit);
            break;
        }
        inUnit->ClearGoals(0);
        break;
    }
    case kActionTakeoff: {
        inUnit->SetCurrentPhysicsOrder(5);
        if (inUnit->mHomeBase == NULL) {
            if (theAirTraffic->GetNumSlots() == 0)
                inUnit->Kill(true, false, false);
            else {
                if (inUnit->mContainer == NULL)
                    inUnit->SetHomeBase(theAirTraffic->mSlots[0].mAirport);
                else {
                    for (unsigned long i = 0; i < theAirTraffic->GetNumSlots(); i++) {
                        if (inUnit->mContainer == theAirTraffic->mSlots[i].mAirport)
                            inUnit->SetHomeBase(theAirTraffic->mSlots[i].mAirport);
                    }
                }
                this->ReturnToAirport(inUnit);
            }
        } else if (inUnit->mUnitClass->mAirTrafficType == 1 || inUnit->mUnitClass->mAirTrafficType == 3) {
            switch (inUnit->mHomeBase->mFacing) {
            case 0:
                this->TakeOffFromRunwayPosY(inUnit);
                break;
            case 1:
                this->TakeOffFromRunwayNegY(inUnit);
                break;
            case 2:
                this->TakeOffFromRunwayPosX(inUnit);
                break;
            default:
                this->TakeOffFromRunwayNegX(inUnit);
                break;
            }
        } else
            this->TakeOffFromCarrier(inUnit);
        if (inUnit->mHomeBase == NULL)
            return;
        inUnit->SetFlightTime(inUnit->GetStats()->mFlightTime);
        if (!this->mIsParatrooperBomber)
            inUnit->SetAmmunition(this->mAmmunition);
        inUnit->SetAnimation(0xd, -1, 0);
        if (inUnit->GetNextWaypointPoint(thePosition) != 0)
            return;
        inUnit->SetHeight(this->GetHeightAt(inUnit, thePosition));
        return;
    }
    case kActionRequestLanding: {
        long theResult;
        if (inUnit->mHomeBase != NULL)
            theResult = theAirTraffic->QueueRequest(inUnit->mHomeBase, inUnit, 1);
        else
            theResult = 9;
        if (theResult != 0) {
            EEComplexUnit* theBase = theAirTraffic->FindNearestAirport(inUnit);
            if (theBase == NULL)
                inUnit->Kill(true, false, false);
            else {
                inUnit->SetHomeBase(theBase);
                inUnit->StartAction(kActionRequestLanding);
                this->ActionChanged(inUnit);
            }
        }
        inUnit->mInitialContactHeading = kLandableNone;
        break;
    }
    case kActionWaitForLandingClearance:
        inUnit->SetCurrentPhysicsAction(0);
        inUnit->SetHasArrived(false);
        inUnit->SetF220(0.0f);
        break;
    case kActionAttackEnemyUnit:
        if (this->mProjectileClass != NULL && this->mProjectileClass->mCategory == kUnitCategoryBomb) {
            inUnit->SetIsCommandable(false);
            if (theGoal != NULL && theGoal->mKind != kGoalAttackGround) {
                EEFindDropLocation(inUnit, &theEnemy, &theTarget, NULL);
                inUnit->SetGoal(new EEUGAttackGround(theTarget, theGoal->GetB1C()), 0, 0);
            }
        }
    case kActionFaceEnemyUnit:
        inUnit->SetCurrentPhysicsOrder(5);
        this->HandleActionChanged(inUnit);
        break;
    case kActionRunFromAttacker:
        if (this->mProjectileClass != NULL && this->mProjectileClass->mCategory == kUnitCategoryBomb && !inUnit->mIsCommandable && inUnit->mFlightTime != 0)
            inUnit->mIsCommandable = true;
        this->HandleActionChanged(inUnit);
        break;
    case kActionInitialAttackState:
        if (inUnit->mHeight < this->GetCruiseAltitude() && this->GetCruiseAltitude() != inUnit->mHeight)
            inUnit->mHeight = this->GetCruiseAltitude();
        this->HandleActionChanged(inUnit);
        break;
    case kActionCheckRange:
        if (inUnit->mInitialContactHeading != kLandableNone)
            return;
        if (inUnit->mInitialContactLocation != thePosition)
            inUnit->mInitialContactLocation = thePosition;
        if (inUnit->mInitialContactHeading != 0.0f)
            inUnit->mInitialContactHeading = 0.0f;
        break;
    case kActionShouldIFollowEnemyUnit:
        if (inUnit->mInitialContactHeading != kLandableNone)
            return;
        inUnit->SetInitialContactLocation(thePosition);
        if (inUnit->mInitialContactHeading != 0.0f)
            inUnit->mInitialContactHeading = 0.0f;
        break;
    case kActionPrepareToMove:
        inUnit->SetCurrentPhysicsAction(0);
        inUnit->SetCurrentPhysicsOrder(5);
        inUnit->DropPath();
        if (inUnit->mInitialContactHeading == kLandableNone) {
            inUnit->SetInitialContactLocation(thePosition);
            if (inUnit->mInitialContactHeading != 0.0f)
                inUnit->mInitialContactHeading = 0.0f;
        }
        if (theGoal->mKind == kGoalAttackUnit) {
            EEFindDropLocation(inUnit, &theEnemy, &theTarget, NULL);
            EEComplexUnit* theComplex = theEnemy->mComplexUnit;
            if (theComplex != NULL && (theComplex->mVelocity < -0.0001f || theComplex->mVelocity > 0.0001f)) {
                float theRange = inUnit->GetStats()->mRange;
                float theRange2 = theRange;
                if (EEGetHitDistanceSquared(inUnit, theEnemy, -10000.0f, -10000.0f) > theRange * theRange2)
                    theGoal->GetDestination(inUnit, theTarget);
            }
        } else {
            theGoal->GetDestination(inUnit, theTarget);
            if (theGoal->mKind == kGoalExplore)
                theTarget.SetZ(this->GetCruiseAltitude());
        }
        this->PathToward(inUnit, thePosition, theTarget, true);
        inUnit->SetAnimation(0xd, -1, 0);
        if (inUnit->GetNextWaypointPoint(thePosition) == 0)
            inUnit->SetHeight(this->GetHeightAt(inUnit, thePosition));
        break;
    case kActionDying: {
        EESoundObject* theSound = inUnit->GetStats()->mDeathSound;
        if (theSound != NULL) {
            float theVolume;
            float thePan;
            float thePitch;
            this->GetSoundParameters(inUnit, theSound, &theVolume, &thePan, &thePitch);
            gSoundManager->Play(theSound->mSampleID, theVolume, thePan, thePitch, theSound->mLooping, NULL);
        }
        inUnit->SetCurrentPhysicsAction(2);
        inUnit->SetAnimation(4, -1, 0);
        inUnit->DropPath();
        EEPMAirplane* theModel = dynamic_cast<EEPMAirplane*>(this->mPhysicsModel);
        if (theModel != NULL)
            inUnit->SetF220(theModel->ComputeRoll(inUnit, inUnit->mAngularVelocity.GetYaw()));
        theOrientation = inUnit->mAngularVelocity;
        theOrientation.SetRoll(theOrientation.GetYaw());
        inUnit->mAngularVelocity = theOrientation;
        return;
    }
    default:
        this->HandleActionChanged(inUnit);
        return;
    }
}

// FUNCTION: EXE 0x615246
// Sets the unit's initial contact location
void EEComplexUnit::SetInitialContactLocation(const GE3DPoint& inPoint)
{
    if (this->mInitialContactLocation != inPoint)
        this->mInitialContactLocation = inPoint;
}

// FUNCTION: EXE 0x615290
// Sets up a new plane: adds its weight to its air traffic control and starts it on the ground
void EEUCLandable::UnitCreated(EEComplexUnit* inUnit)
{
    EEUnitClass::UnitCreated(inUnit);
    inUnit->GetPlayer()->mAirTraffic[inUnit->GetUnitClass()->mAirTrafficType - 1]->AddCargoWeight(inUnit);
    inUnit->SetHeight(0.0f);
}

// FUNCTION: EXE 0x6152d0
// Returns whether a bomb dropped now lands within reach of the attack target along its ballistic arc (reach:
// half the area effect for the last bomb on a moving target, else 0.25 for the last bomb or none)
bool EEUCLandable::IsBombOnTarget(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    EEUnitClassUpgrade* theStats = inUnit->GetStats();
    GE3DPoint theOffset;
    GE3DPoint theTarget;
    GE3DPoint theStart;
    EEUnit* theEnemy;
    GEVector theVelocity;
    GEVector theHeading;
    EETagPointList* theTags = inUnit->GetModel()->mTagPoints[inUnit->mAnimation];
    EEUGAttackUnit* theAttack = dynamic_cast<EEUGAttackUnit*>(inUnit->mGoalState);
    float theGravity = this->mProjectileClass->mAcceleration;
    float theSpeed = theStats->mLaunchSpeed;
    if (theAttack == NULL)
        return false;
    if (theTags == NULL || theTags->mPoints.size() == 0) {
        theOffset.SetZ(0.5f);
        theOffset.SetX(0.0f);
        theOffset.SetY(0.0f);
    } else
        theTags->mPoints[inUnit->mWeaponTagPoint]->GetOffset(inUnit->mPhysicalModel.GetFrame(), theOffset);
    theOffset.SetX(theStats->GetScale() * theOffset.GetX());
    theOffset.SetY(theStats->GetScale() * theOffset.GetY());
    theOffset.SetZ(theStats->GetScale() * theOffset.GetZ());
    theTransformation.Apply(theOffset, theStart);
    EEFindDropLocation(inUnit, &theEnemy, &theTarget, NULL);
    float theReach = inUnit->GetStats()->mAreaEffect;
    if (theEnemy != NULL && theEnemy->mComplexUnit != NULL &&
        (theEnemy->mComplexUnit->mVelocity < -0.0001f || theEnemy->mComplexUnit->mVelocity > 0.0001f) && theReach > 0.5f) {
        if (inUnit->mAmmunition == 1)
            theReach = theReach * 0.5f;
        else if (theReach == 0.0f)
            theReach = 0.25f;
    } else if (inUnit->mAmmunition == 1 || theReach == 0.0f)
        theReach = 0.25f;
    theVelocity.SetY(1.0f);
    theTransformation.Apply(theVelocity, theHeading);
    if (theHeading.GetZ() > 0.0f)
        theHeading.SetZ(0.0f);
    float theB = -(theHeading.GetZ() * theSpeed);
    float theHalf = theGravity * 0.5f;
    bool theResult = false;
    float theDiscriminant = theB * theB - (theTarget.GetZ() - theStart.GetZ()) * theHalf * 4.0f;
    if (theDiscriminant >= 0.0f) {
        float theRoot = (float)sqrt(theDiscriminant);
        float theOther = (-theB - theRoot) / (theHalf + theHalf);
        float theTime = (theRoot - theB) / (theHalf + theHalf);
        if (theOther > theTime)
            theTime = theOther;
        float theYaw = UMArcTan2(-theHeading.GetX(), theHeading.GetY());
        float theAngle = UMArcCos(-theHeading.GetZ()) - 1.5707964f;
        theVelocity.SetY(UMCos(theAngle) * theTime * theSpeed);
        theVelocity.SetZ(theHeading.GetZ() * theTime * theSpeed - theTime * theTime * theGravity * 0.5f);
        theTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
        theTransformation.Apply(theVelocity, theHeading);
        theStart += theHeading;
        if (theReach * theReach >= theStart.DistanceToSquared(theTarget))
            theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x61562f
// Plans a plane's approach to its home carrier's deck (tag point 0): turn, descent and touch-down (inSteep: the
// full approach whatever the height), or just the deck when braking; makes the deck the move goal
void EEUCLandable::ApproachCarrier(EEComplexUnit* inUnit, bool inSteep)
{
    GEOrientation theOrientation;
    EEComplexUnit* theCarrier = inUnit->mHomeBase;
    GE3DPoint thePosition;
    GE3DPoint theWaypoint;
    GE3DPoint theDeck;
    GE3DPoint theStep;
    EEUnitClassUpgrade* theStats = theCarrier->GetStats();
    EETagPointList* theTags = theCarrier->GetModel()->mTagPoints[theCarrier->mAnimation];
    float theScale = theStats->GetScale();
    inUnit->GetPosition(thePosition);
    float theHeight = thePosition.GetZ();
    inUnit->DropPath();
    if (theTags == NULL || theTags->mPoints.size() == 0)
        return;
    GETransformation theTransformation(false);
    theTransformation = theCarrier->mPhysicalModel.GetLocation();
    EETagPointList::Scale(theTransformation, theScale);
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) != kActionBrake) {
        theTags->mPoints[0]->GetPoint(inUnit->mPhysicalModel.GetFrame(), theTransformation, theDeck);
        theTransformation = theCarrier->mPhysicalModel.GetLocation();
        theTransformation.SetPosition(theDeck.GetX(), theDeck.GetY(), theDeck.GetZ());
        float theLength = inUnit->GetStats()->GetMaxVelocity() / inUnit->GetStats()->GetMaxAngularVelocity();
        long theCount = UMCeil(theLength);
        float theRun = (float)theCount * 1.5f;
        float theFar = theRun / UMTan(kLandableAngle) + theRun;
        float theDX = thePosition.GetX() - (theDeck.GetX() - theFar);
        if (theDX * theDX + (thePosition.GetY() - theDeck.GetY()) * (thePosition.GetY() - theDeck.GetY()) <=
            (float)theCount * 2.5f) {
            theWaypoint.SetX(theDeck.GetX());
            theWaypoint.SetY(theDeck.GetY());
            theWaypoint.SetZ(this->GetCruiseAltitude() * 0.7f);
            this->PathToward(inUnit, thePosition, theWaypoint, true);
            thePosition = theWaypoint;
        }
        if (inSteep || this->GetCruiseAltitude() <= theHeight) {
            theStep.SetY(-theFar);
            theTransformation.Apply(theStep, theWaypoint);
            theWaypoint.SetZ(this->GetCruiseAltitude());
            this->PathToward(inUnit, thePosition, theWaypoint, true);
        }
        if (inSteep || theDeck.GetZ() + (this->GetCruiseAltitude() - theDeck.GetZ()) * 0.3f <= theHeight) {
            theStep.SetY(-theRun);
            theTransformation.Apply(theStep, theWaypoint);
            theWaypoint.SetZ(theDeck.GetZ() + (this->GetCruiseAltitude() - theDeck.GetZ()) * 0.3f);
            this->AddWaypointAt(inUnit, theWaypoint.GetX(), theWaypoint.GetY(), theWaypoint.GetZ(), true);
        }
        this->AddWaypointAt(inUnit, theDeck.GetX(), theDeck.GetY(), theDeck.GetZ(), true);
        this->AddWaypointAt(inUnit, theCarrier->mPhysicalModel.GetLocation().GetX(),
                 theCarrier->mPhysicalModel.GetLocation().GetY(), theCarrier->mPhysicalModel.GetLocation().GetZ(),
                 true);
    } else {
        // braking: the deck's stop point is tag point 2, and the plane takes the carrier's own (unscaled) heading
        (theTags->mPoints[2])->GetPoint(inUnit->mPhysicalModel.GetFrame(), theTransformation, theDeck);
        this->AddWaypointAt(inUnit, theDeck.GetX(), theDeck.GetY(), theDeck.GetZ(), true);
        theCarrier->mPhysicalModel.GetLocation().ConvertToOrientation(theOrientation);
        inUnit->mOrientation = theOrientation;
    }
    EEUGLocation* theLocation = NULL;
    if (inUnit->mGoalState != NULL)
        theLocation = dynamic_cast<EEUGLocation*>(inUnit->GetGoal());
    if (theLocation == NULL) {
        EEUGLocation* theMove = new EEUGLocation(&theDeck, false, true, false, 0.0f, false, false);
        inUnit->ClearGoalQueue(0);
        inUnit->SetGoal(theMove, 0, 0);
    } else
        theLocation->mLocation = theDeck;
}

// FUNCTION: EXE 0x615a4d
// Plans a take-off from the home carrier: on take-off, sits at the deck start (tag point 0) and heads for the
// take-off point; otherwise flies off the deck end (tag point 2) towards the goal
void EEUCLandable::TakeOffFromCarrier(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theStart;
    EEComplexUnit* theCarrier = inUnit->mHomeBase;
    GE3DPoint theEnd;
    GE3DPoint theTarget;
    GETransformation theCarrierLocation(false);
    EEUnitClassUpgrade* theStats = theCarrier->GetStats();
    EETagPointList* theTags = theCarrier->GetModel()->mTagPoints[theCarrier->mAnimation];
    float theX = theCarrier->mPhysicalModel.GetLocation().GetX();
    float theY = theCarrier->mPhysicalModel.GetLocation().GetY();
    float theScale = theStats->GetScale();
    inUnit->DropPath();
    if (theTags != NULL && theTags->GetNumTagPoints() != 0) {
        theCarrierLocation = theCarrier->mPhysicalModel.GetLocation();
        EETagPointList::Scale(theCarrierLocation, theScale);
        theTags->GetTagPoint(0)->GetPoint(inUnit->mPhysicalModel.GetFrame(), theCarrierLocation, theStart);
        theTags->GetTagPoint(2)->GetPoint(inUnit->mPhysicalModel.GetFrame(), theCarrierLocation, theEnd);
    } else {
        theStart.SetX(theX);
        theStart.SetY(theY);
        theStart.SetZ(1.0f);
        theEnd.SetX(theX);
        theEnd.SetY(theY);
        theEnd.SetZ(1.0f);
    }
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionTakeoff) {
        theTransformation.SetPosition(theStart);
        inUnit->SetTransformation(theTransformation, true);
        if (theStart.GetZ() >= this->GetCruiseAltitude())
            inUnit->mPhysicalModel.SetScale(inUnit->GetStats()->GetScale());
        else {
            float theHalf = theStart.GetZ() / this->GetCruiseAltitude() * 0.5f;
            inUnit->mPhysicalModel.SetScale(inUnit->GetStats()->GetScale() * (theHalf + 0.5f));
        }
        long theIndex = UMFloatToLong(inUnit->GetStats()->GetMaxVelocity() / inUnit->GetStats()->GetMaxAngularVelocity());
        this->RouteDockWaypoint(inUnit);
    } else {
        theCarrierLocation = theCarrier->mPhysicalModel.GetLocation();
        theCarrierLocation.SetX(theEnd.GetX());
        theCarrierLocation.SetY(theEnd.GetY());
        theCarrierLocation.SetZ(theEnd.GetZ());
        theStart.SetX(0.0f);
        if (inUnit->mUnitClass->GetUnitType() == kUnitTypeSp15SpaceFighter)
            theStart.SetY(3.0f);
        else theStart.SetY(10.0f);
        theStart.SetZ(0.0f);
        theCarrierLocation.Apply(theStart, theTarget);
        this->AddWaypointAt(inUnit, theTarget.GetX(), theTarget.GetY(), this->GetCruiseAltitude() * 0.7f, true);
        this->FlyToGoal(theTarget, inUnit);
    }
}

// FUNCTION: EXE 0x615ccf
// Returns 0 (left) or 1 (right) when the next waypoint or goal lies inside the turning circle beside or behind
// the plane, else 5
unsigned long EEUCLandable::GetNextFlightState(EEComplexUnit* inUnit)
{
    GE3DPoint theLeft;
    EEUnitGoal* theGoal = inUnit->mGoalState;
    GE3DPoint theRight;
    unsigned long theResult = 5;
    GE3DPoint theAhead;
    GE3DPoint theOffset;
    GE3DPoint theTarget;
    float theRadius = inUnit->GetStats()->GetMaxVelocity() / inUnit->GetStats()->GetMaxAngularVelocity();
    float theRadius2 = theRadius * theRadius;
    if ((long)inUnit->mWaypoints.size() > 0)
        inUnit->GetNextWaypointPoint(theTarget);
    else {
        if (theGoal == NULL)
            return 5;
        switch (theGoal->mKind) {
        case kGoalLocation:
        case kGoalAttackGround:
            theTarget = static_cast<EEUGAttackGround*>(theGoal)->mTarget;
            break;
        case kGoalAttackUnit: {
            EEUnit* theUnit = static_cast<EEUGAttackUnit*>(theGoal)->mTarget;
            if (theUnit == NULL)
                return 5;
            EEComplexUnit* theComplex = theUnit->mComplexUnit;
            if (theComplex != NULL) {
                const GETransformation& theComplexLocation = theComplex->mPhysicalModel.GetLocation();
                theTarget.SetX(theComplexLocation.GetX());
                theTarget.SetY(theComplexLocation.GetY());
                theTarget.SetZ(theComplexLocation.GetZ());
            } else {
                theTarget.SetX((float)theUnit->GetTileX() + 0.5f);
                theTarget.SetY((float)theUnit->GetTileY() + 0.5f);
                theTarget.SetZ(theUnit->mTile->GetHeight());
            }
            break;
        }
        case kGoalPatrol: {
            EEComplexUnit* theOther = static_cast<EEUGPatrol*>(theGoal)->mTarget;
            const GETransformation& theOtherLocation = theOther->mPhysicalModel.GetLocation();
            theTarget.SetX(theOtherLocation.GetX());
            theTarget.SetY(theOtherLocation.GetY());
            theTarget.SetZ(theOtherLocation.GetZ());
            break;
        }
        case kGoalExplore: {
            static_cast<EEUGExplore*>(theGoal)->GetTileCenter(theTarget);
            break;
        }
        default:
            return 5;
        }
    }
    GETransformation theInverse(false);
    inUnit->mPhysicalModel.GetLocation().MakeInverse(theInverse);
    theInverse.Apply(theTarget, theOffset);
    theOffset.SetZ(0.0f);
    theLeft.SetX(-theRadius);
    theRight.SetX(theRadius);
    theAhead.SetY(-theRadius);
    bool theNearLeft = theLeft.DistanceToSquared(theOffset) <= theRadius2;
    bool theNearRight = theRight.DistanceToSquared(theOffset) <= theRadius2;
    bool theNearAhead = theAhead.DistanceToSquared(theOffset) <= theRadius2;
    if (theNearLeft || theNearRight || theNearAhead) {
        theResult = theOffset.GetX() < 0.0f ? 0 : 1;
    }
    return theResult;
}

// FUNCTION: EXE 0x615ed8
// Returns whether the plane reached its next waypoint (within 0.5); braking onto a runway's last one, whether
// inValue is at most 0.05
bool EEUCLandable::HasArrived(EEComplexUnit* inUnit, float inValue)
{
    GE3DPoint theTarget;
    unsigned long theCount = inUnit->mWaypoints.size();
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          0.0f);
    inUnit->GetNextWaypointPoint(theTarget);
    theTarget.SetZ(0.0f);
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionBrake && theCount <= 1 && this->mAirTrafficType != 2 && this->mAirTrafficType != 4) {
        return inValue <= 0.05f;
    }
    return theTarget.DistanceToSquared(thePosition) <= 0.25f;
}

// FUNCTION: EXE 0x615f7d
// Plans a land plane's runway approach in a direction: turn onto the line, descent and touch-down at the airport
// (a navy yard: its pier), then makes the airport the move goal (the queue kept behind an explore goal)
void EEUCLandable::ApproachRunway(EEComplexUnit* inUnit, long inDX, long inDY)
{
    GE3DPoint theWaypoint;
    EEComplexUnit* theBase = inUnit->mHomeBase;
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          inUnit->mPhysicalModel.GetLocation().GetZ());
    float theY = theBase->mPhysicalModel.GetLocation().GetY();
    float theX = theBase->mPhysicalModel.GetLocation().GetX();
    float theFar;
    float theNear;
    float theFinal;
    float theLow;
    float theFlare;
    float theGround;
    bool theCarrier;
    if (theBase->mUnitClass->GetUnitType() == kUnitTypeNavyYard) {
        theFar = 3.0f;
        theGround = 0.0f;
        theNear = 2.0f;
        theFinal = 0.8f;
        theLow = 0.3f;
        theFlare = 0.05f;
        theCarrier = true;
        theX = (float)inDX * 1.5f + theX;
        theY = (float)inDY * 1.5f + theY;
    } else {
        theFinal = 1.0f;
        theFar = 7.0f;
        theNear = 3.0f;
        theLow = 0.5f;
        theFlare = 0.15f;
        theGround = kLandableLow;
        theCarrier = false;
    }
    inUnit->DropPath();
    float theLength = inUnit->GetStats()->GetMaxVelocity() / inUnit->GetStats()->GetMaxAngularVelocity();
    long theCount = UMCeil(theLength);
    float theDX = (float)inDX;
    float theRadius = (float)theCount;
    float theDY = (float)inDY;
    float theStartX = theDX * theFar + theX;
    float theRun = theRadius * 1.5f;
    float theStartY = theDY * theFar + theY;
    float theTurn = theRun / UMTan(kLandableAngle) + theRun;
    float theOffsetY = theDY * theTurn;
    float theOffsetX = theDX * theTurn;
    float theDistY = thePosition.GetY() - (theStartY - theOffsetY);
    float theDistX = thePosition.GetX() - (theStartX - theOffsetX);
    if (theDistY * theDistY + theDistX * theDistX <= theRadius * 2.5f) {
        theWaypoint.SetZ(this->GetCruiseAltitude());
        theWaypoint.SetX(theStartX);
        theWaypoint.SetY(theStartY);
        this->PathToward(inUnit, thePosition, theWaypoint, true);
        thePosition = theWaypoint;
    }
    theWaypoint.SetX(theOffsetX + theStartX);
    theWaypoint.SetY(theOffsetY + theStartY);
    theWaypoint.SetZ(this->GetCruiseAltitude() * theFinal);
    this->PathToward(inUnit, thePosition, theWaypoint, true);
    this->AddWaypointAt(inUnit, theDX * theRun + theStartX, theDY * theRun + theStartY, this->GetCruiseAltitude() * theLow, theCarrier);
    this->AddWaypointAt(inUnit, theStartX, theStartY, this->GetCruiseAltitude() * theFlare, false);
    this->AddWaypointAt(inUnit, theDX * theNear + theX, theDY * theNear + theY, theGround, false);
    this->AddWaypointAt(inUnit, theDX + theX, theDY + theY, theGround, false);
    this->AddWaypointAt(inUnit, theX, theY, theGround, false);
    theWaypoint.SetX(theX);
    theWaypoint.SetY(theY);
    float theHeight = ::gWorld.mMap->GetTile(UMFloor(theX), UMFloor(theY))->GetHeight();
    if (theHeight < 0.0f)
        theHeight = 0.0f;
    theWaypoint.SetZ(theHeight);
    EEUGLocation* theMove = new EEUGLocation(&theWaypoint, false, true, false, 0.0f, false, false);
    bool theKeep;
    if (inUnit->mGoalState == NULL || inUnit->mGoalState->mKind != kGoalExplore) {
        inUnit->ClearGoalQueue(0);
        theKeep = false;
    } else
        theKeep = true;
    inUnit->SetGoal(theMove, theKeep, 0);
}

// FUNCTION: EXE 0x616308
// Plans the runway approach along +x
void EEUCLandable::ApproachRunwayPosX(EEComplexUnit* inUnit)
{
    this->ApproachRunway(inUnit, 1, 0);
}

// FUNCTION: EXE 0x616318
// Plans the runway approach along +y
void EEUCLandable::ApproachRunwayPosY(EEComplexUnit* inUnit)
{
    this->ApproachRunway(inUnit, 0, 1);
}

// FUNCTION: EXE 0x616328
// Plans the runway approach along -y
void EEUCLandable::ApproachRunwayNegY(EEComplexUnit* inUnit)
{
    this->ApproachRunway(inUnit, 0, -1);
}

// FUNCTION: EXE 0x616338
// Plans the runway approach along -x
void EEUCLandable::ApproachRunwayNegX(EEComplexUnit* inUnit)
{
    this->ApproachRunway(inUnit, -1, 0);
}

// FUNCTION: EXE 0x616348
// Plans a runway take-off in a direction: rolls 3 tiles low, climbs to 3/4 cruising height 7 tiles out, then
// flies to the goal
void EEUCLandable::TakeOffFromRunway(EEComplexUnit* inUnit, long inDX, long inDY)
{
    GE3DPoint theTarget;
    float theX = inUnit->mPhysicalModel.GetLocation().GetX();
    float theY = inUnit->mPhysicalModel.GetLocation().GetY();
    inUnit->DropPath();
    this->AddWaypointAt(inUnit, (float)inDX * 3.0f + theX, (float)inDY * 3.0f + theY, kLandableLow, false);
    float theEndY = (float)inDY * 7.0f + theY;
    float theEndX = (float)inDX * 7.0f + theX;
    this->AddWaypointAt(inUnit, theEndX, theEndY, this->GetCruiseAltitude() * 0.75f, false);
    theTarget.SetX(theEndX);
    theTarget.SetY(theEndY);
    theTarget.SetZ(this->GetCruiseAltitude() * 0.75f);
    this->FlyToGoal(theTarget, inUnit);
}

// FUNCTION: EXE 0x616430
// Plans the runway take-off along +x
void EEUCLandable::TakeOffFromRunwayPosX(EEComplexUnit* inUnit)
{
    this->TakeOffFromRunway(inUnit, 1, 0);
}

// FUNCTION: EXE 0x616440
// Plans the runway take-off along +y
void EEUCLandable::TakeOffFromRunwayPosY(EEComplexUnit* inUnit)
{
    this->TakeOffFromRunway(inUnit, 0, 1);
}

// FUNCTION: EXE 0x616450
// Plans the runway take-off along -y
void EEUCLandable::TakeOffFromRunwayNegY(EEComplexUnit* inUnit)
{
    this->TakeOffFromRunway(inUnit, 0, -1);
}

// FUNCTION: EXE 0x616460
// Plans the runway take-off along -x
void EEUCLandable::TakeOffFromRunwayNegX(EEComplexUnit* inUnit)
{
    this->TakeOffFromRunway(inUnit, -1, 0);
}

// FUNCTION: EXE 0x616470
// Returns whether the move goal is more than 1 from the home carrier's deck point (tag point 0): the carrier moved
bool EEUCLandable::IsAwayFromGoal(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mHomeBase->mPhysicalModel.GetLocation());
    EEComplexUnit* theCarrier = inUnit->mHomeBase;
    GE3DPoint theDeck;
    EEUnitClassUpgrade* theStats = theCarrier->GetStats();
    EETagPointList* theTags = theCarrier->GetModel()->mTagPoints[theCarrier->mAnimation];
    if (theTags == NULL || theTags->mPoints.size() == 0) {
        theDeck.SetX(theTransformation.GetX());
        theDeck.SetY(theTransformation.GetY());
        theDeck.SetZ(1.0f);
    } else {
        theTags->Scale(theTransformation, theStats->GetScale());
        theTags->mPoints[0]->GetPoint(inUnit->mPhysicalModel.GetFrame(), theTransformation, theDeck);
    }
    bool theResult;
    if (!(theDeck.DistanceToSquared(static_cast<EEUGLocation*>(inUnit->GetGoal())->mLocation) <= 1.0f))
        theResult = true;
    else
        theResult = false;
    return theResult;
}

// FUNCTION: EXE 0x61655a
// Answers the plane's flight triggers (waypoint on ground, in air, higher, same, lower; bomb in range; fuel alarm;
// clearance; airport destroyed; secondary target; controller moved; request queued); others the common way
long EEUCLandable::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    GE3DPoint theTarget;
    long theResult = 0;
    bool theFlag = false;
    float theClimb;
    float theGround;
    switch (inEvent) {
    case kTriggerDyingAnimationFinished:
        if (inUnit->GetUnitClass()->mDomains & kMovementSpace)
            theFlag = true;
        else
            theFlag = inUnit->mVelocity == 0.0f;
        break;
    case kTriggerMovePreparationComplete:
        theFlag = (long)inUnit->mWaypoints.size() > 0;
        break;
    case kTriggerWaypointOnGround: {
        if (this->GetNextFlightPoint(inUnit, theTarget, &theClimb) == 0) {
            this->GetGroundHeight(theTarget, &theGround);
            theFlag = inUnit->mHasArrived && inUnit->mWaypoints.size() > 0 && theTarget.GetZ() == theGround;
        }
        break;
    }
    case kTriggerWaypointInAir: {
        if (this->GetNextFlightPoint(inUnit, theTarget, &theClimb) == 0) {
            this->GetGroundHeight(theTarget, &theGround);
            theFlag = inUnit->mHasArrived && inUnit->mWaypoints.size() > 0 && theTarget.GetZ() > theGround;
        }
        break;
    }
    case kTriggerWaypointIsHigherElevation: {
        if (this->GetNextFlightPoint(inUnit, theTarget, &theClimb) == 0)
            theFlag = inUnit->mHasArrived && inUnit->mWaypoints.size() > 0 && theClimb > 0.0f;
        break;
    }
    case kTriggerWaypointIsSameElevation: {
        if (this->GetNextFlightPoint(inUnit, theTarget, &theClimb) == 0) {
            unsigned long theCount = inUnit->mWaypoints.size();
            if (inUnit->GetUnitClass()->mAirTrafficType == 2 || inUnit->GetUnitClass()->mAirTrafficType == 4)
                theFlag = theCount <= 1;
            else
                theFlag = inUnit->mHasArrived && theCount > 0 && theClimb == 0.0f;
        } else {
            theFlag = false;
        }
        break;
    }
    case kTriggerWaypointIsLowerElevation: {
        if (this->GetNextFlightPoint(inUnit, theTarget, &theClimb) == 0) {
            unsigned long theCount = inUnit->mWaypoints.size();
            if (inUnit->GetUnitClass()->mAirTrafficType == 2 || inUnit->GetUnitClass()->mAirTrafficType == 4)
                theFlag = theCount > 0;
            else
                theFlag = inUnit->mHasArrived && theCount > 0 && theClimb < 0.0f;
        } else {
            theFlag = false;
        }
        break;
    }
    default:
        theResult = EEUnitClass::EvaluateCondition(inUnit, inEvent);
        break;
    case kTriggerUnitInWeaponRange:
        if (this->mProjectileClass != NULL && this->mProjectileClass->mCategory == kUnitCategoryBomb)
            theFlag = inUnit->mAmmunition < this->mAmmunition || this->IsBombOnTarget(inUnit);
        else
            theResult = EEUnitClass::EvaluateCondition(inUnit, inEvent);
        break;
    case kTriggerFlightTimeAlarm:
        theFlag = inUnit->mPhysicalModel.GetLocation().GetZ() != inUnit->mTile->GetHeight() && inUnit->mFlightTime == 0;
        break;
    case kTriggerLandingClearanceGranted:
        theFlag = inUnit->mIsClearanceGranted;
        break;
    case kTriggerHoldingPatternComputed:
        theFlag = true;
        break;
    case kTriggerAirportDestroyed:
        theFlag = inUnit->mHomeBase == NULL || inUnit->mHomeBase->mHitPoints == 0;
        break;
    case kTriggerSecondaryTargetFound: {
        EEUGAttackUnit* theAttack = dynamic_cast<EEUGAttackUnit*>(inUnit->GetGoal());
        theFlag = theAttack != NULL && theAttack->mTarget != NULL && theAttack->mTarget->mHitPoints > 0;
        break;
    }
    case kTriggerAirtrafficControllerMoved: {
        EEUnitGoal* theGoal = inUnit->GetGoal();
        if (inUnit->mHomeBase != NULL) {
            long theCategory = inUnit->mHomeBase->GetUnitClass()->mCategory;
            if ((theCategory == kUnitCategoryNavalBuilding || theCategory == kUnitCategorySpaceBuilding) && inUnit->mHomeBase->GetUnitClass()->mAirTrafficType != 0
                && dynamic_cast<EEUGLocation*>(theGoal) != 0)
                theFlag = this->IsAwayFromGoal(inUnit);
        }
        break;
    }
    case kTriggerRequestStillValid:
        theFlag = inUnit->mPlayer->mAirTraffic[inUnit->GetUnitClass()->mAirTrafficType - 1]->GetQueuePosition(inUnit, 1) >= 0
                  || inUnit->mPlayer->mAirTraffic[inUnit->GetUnitClass()->mAirTrafficType - 1]->GetQueuePosition(inUnit, 0) >= 0;
        break;
    }
    if (theFlag)
        theResult = 1;
    return theResult;
}

// FUNCTION: EXE 0x61695d
// Updates the plane's circling side from GetNextFlightState (not while circling or waiting); a switch of side
// waits for the flight time's next whole second
void EEUCLandable::UpdateFlightState(EEComplexUnit* inUnit, long inTime, long inLeft)
{
    long theCommand = inUnit->mCommand ? inUnit->mCommand->mType : 0;
    if (theCommand == kActionCircle || theCommand == kActionWaitForLandingClearance || theCommand == kActionCircleAirtrafficController)
        return;
    unsigned long theState = inUnit->mCurrentPhysicsOrder;
    unsigned long theNew = this->GetNextFlightState(inUnit);
    if (theState == 5) {
        if (theNew != 5)
            inUnit->SetCurrentPhysicsOrder(theNew);
    } else if (theNew == 5)
        inUnit->SetCurrentPhysicsOrder(5);
    else if (theState != theNew) {
        long theNow = inTime / 1000;
        long theLast = inLeft / 1000;
        if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) != kActionFaceEnemyUnit && (theNow > theLast || theLast == 0))
            inUnit->SetCurrentPhysicsOrder(theNew);
    }
}

// FUNCTION: EXE 0x616a11
// Advances a plane each tick: flight time countdown (not in cinematics or for nature), circling side, goal queues,
// physics, timers, animation and sound, attack frames, rotting, and fading out below ground in space
void EEUCLandable::Update(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition;
    long theFrame = inUnit->mPhysicalModel.GetFrame();
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) != kActionIdle) {
        long theTime = inUnit->mFlightTime;
        long theLeft = theTime - gPhysics->mTickTimeMs;
        if (theLeft < 0)
            theLeft = 0;
        if (!::gWorld.mInCinematic && inUnit->mPlayer->mPlayerID != 0)
            inUnit->SetFlightTime(theLeft);
        this->UpdateFlightState(inUnit, theTime, theLeft);
    }
    unsigned long theCount = inUnit->GetNumGoalQueues();
    for (unsigned long i = 0; i < theCount; i++) {
        if (!inUnit->mGoalQueues[i].mStarted || inUnit->mGoalQueues[i].mGoals.size() <= 0)
            continue;
        inUnit->mGoalQueues[i].Update();
        inUnit->RefreshGoals();
        inUnit->RefreshActiveGoal();
    }
    if (this->mPhysicsModel != NULL && inUnit->GetTileX() != -10000)
        this->mPhysicsModel->Step(inUnit);
    inUnit->UpdateOrbitals();
    this->UpdateEffects(inUnit);
    inUnit->UpdateReloadTimer();
    inUnit->UpdateTargetSearchTimer();
    this->UpdateCamouflageTimer(inUnit);
    long theAttack = this->Animate(inUnit);
    this->RefreshSound(inUnit);
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionAttackEnemyUnit:
    case kActionAttackLocation:
    case kActionWaitForReload:
        this->FireOnAttackFrame(inUnit, theAttack, theFrame);
        break;
    case kActionUnitRotting: {
        long theLeft = inUnit->mRottingAwayTime - gPhysics->mTickTimeMs;
        if (theLeft < 0)
            theLeft = 0;
        inUnit->SetRottingAwayTime(theLeft);
        break;
    }
    case kActionFadingOutInSpace:
        inUnit->GetPosition(thePosition);
        if (thePosition.GetZ() < 0.0f)
            inUnit->SetOpacityChain(1.0f - thePosition.GetZ() * -0.16666667f);
        break;
    }
    this->UpdateDone(inUnit);
}

// FUNCTION: EXE 0x616c22
// Sets the opacity of a unit and the pieces linked after it (stops at the first already at the value)
void EEComplexUnit::SetOpacityChain(float inValue)
{
    EEComplexUnit* theUnit = this;
    do {
        if (theUnit->mOpacity == inValue)
            return;
        theUnit->mOpacity = inValue;
        theUnit = theUnit->mNextPart;
    } while (theUnit != NULL);
}

// ICF-FOLDED: EXE 0x61aaa9
// Adds the transport to its own tile only
// (no address of its own: the same body as EEUCSpaceTransport::AddToTile, which ICF kept)
void EEUCLandable::AddToTile(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = theMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->mUnits = theTile->AddUnit(theUnit, theTile->mUnits);
}

// ICF-FOLDED: EXE 0x60afe9
// Animates the animal like any unit
// (no address of its own: the same body as EEUCAirAnimal::Animate, which ICF kept)
long EEUCLandable::Animate(EEComplexUnit* inUnit)
{
    return EEUnitClass::Animate(inUnit);
}

// ICF-FOLDED: EXE 0x418aa9
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EEUCLandable::CanAttackMove()
{
    return true;
}

// ICF-FOLDED: EXE 0x61aaf5
// Removes the transport from its own tile only
// (no address of its own: the same body as EEUCSpaceTransport::RemoveFromTile, which ICF kept)
void EEUCLandable::RemoveFromTile(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = theMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->RemoveUnit(theUnit);
}
