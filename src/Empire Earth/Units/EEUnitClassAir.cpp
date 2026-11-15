// EEUnitClassAir.cpp (new, R5b.4 round 32): the aircraft and spaceships' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
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

#include "ee/EEUnitUpdate.h"
#include "rust/ee_units.h"
#include "ee/EEUnitKind.h"
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
#include "ee/EEUGRust.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEEffectManager.h"

// ---- EEUCLandable.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The unit class of the
// aircraft that land (airplanes, bombers, the animals' and planet ships' bases): 0x213c60..0x216c48 (after
// EEUCLand.cpp; EEUCLandCitizen.cpp follows). One unit per EEUC* class in alphabetical order. Unit flags: the exe's
// module set. Its ctor is inline (EEUnitClass.cpp's factory). Order: the landing (ReturnToAirport), the take-off point
// (RouteDockWaypoint), Virtual24, Virtual2C, the approach (FlyToGoal), the ground height (GetGroundHeight), GetNextFlightPoint, Virtual74,
// EEComplexUnit::SetInitialContactLocation, Virtual78, the bomb drop (IsBombOnTarget), the carrier approach (ApproachCarrier, TakeOffFromCarrier), the
// flight state (GetNextFlightState), Virtual88, the runway approaches (ApproachRunway and its four wrappers, TakeOffFromRunway and its four),
// IsAwayFromGoal, VirtualA0, VirtualE0, VirtualB8, EEComplexUnit::SetOpacityChain. EEUCAirAnimal.cpp's bodies are the twins of
// several of these (same sizes).

// 0x447038..0x447050: the unit's named float constants (one .rdata block per unit) (provisional names)
static const float kLandableNone = -10000.0f;   // 0x447038 (mInitialContactHeading: no landing height)

static const float kLandableCeiling = 12.0f;

static const float kLandableAngle = 0.5235988f; // 0x447040 (the approach's 30 degrees)

static const float kLandableAhead = 2.0f;

static const float kLandableLow = 0.1f;         // 0x447048 (the touch-down height)

// cdecl helpers (EEUCHelicopter.cpp declares the first the same way) (provisional names)
void EEFindLandingPoint(EEComplexUnit* inUnit, GE3DPoint* ioTarget);

// 0x24485b: the enemy unit near (inX, inY) within inRange that inUnit can attack (provisional name)
EEUnit* EEFindTargetNear(EEComplexUnit* inUnit, long inX, long inY, float inRange);

// Takes off from the ground: rolls 3 ahead, climbs 7 ahead to 3/4 cruising height (plus the ground height when the
// ground is higher), then flies to the goal; returns true, doing nothing without an air traffic control
bool EEUnitClass::LandableReturnToAirport(EEComplexUnit* inUnit)
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

// Returns the height to fly at towards a point: its height above the ground, over the highest tile corner ahead
// (the turning circle while circling, else the line to it), at most 12
float EEUnitClass::LandableGetHeightAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
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

// Gets the sound parameters, giving a moving plane's sound a Doppler pitch shift against the camera
void EEUnitClass::LandableGetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
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
    this->BaseGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
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

// Paths a plane from a point to its goal's location (move, patrol, explore, attack ground, unload, attack unit)
// at cruising height; an explore goal without a target picks a new area first
void EEUnitClass::FlyToGoal(GE3DPoint& inPoint, EEComplexUnit* inUnit)
{
    GE3DPoint theTarget;
    EEUnit* theUnit;
    EEUnitGoal* theGoal = inUnit->mGoalState;
    bool theHasLocation = EEGoalHasLocation(theGoal);   // a location goal or a heal goal
    EEUGRust* theAttack = EEAttackUnitCast(theGoal);
    EEUGRust* theExplore = EEExploreGoalCast(theGoal);
    EEUGRust* thePatrol = EEPatrolGoalCast(theGoal);
    EEUGRust* theGround = EEAttackGroundCast(theGoal);
    EEUGRust* theUnload = EEUnloadGoalCast(theGoal);
    if (theHasLocation) {
        GE3DPoint theLocation = EELocationGoalPoint(theGoal);
        theTarget.SetX(theLocation.GetX());
        theTarget.SetY(theLocation.GetY());
    } else if (thePatrol != NULL) {
        theTarget.SetX((float)thePatrol->GetPatrolTarget()->GetTileX() + 0.5f);
        theTarget.SetY((float)thePatrol->GetPatrolTarget()->GetTileY() + 0.5f);
    } else if (theExplore != NULL) {
        theExplore->GetExploreTileCenter(theTarget);
        if (theTarget.GetX() <= 0.5f && theTarget.GetY() <= 0.5f) {
            inUnit->StartAction(kActionFindUnexploredArea);
            theExplore->Step(inUnit);
            theExplore->GetExploreTileCenter(theTarget);
            long theKind = inUnit->mUnitClass->mAirTrafficType;
            if (theKind == 2 || theKind == 4)
                inUnit->StartAction(kActionContinueToTaxi);
            else
                inUnit->StartAction(kActionTakeoff);
        }
    } else if (theGround != NULL) {
        theTarget.SetX(theGround->GetGroundTarget().GetX());
        theTarget.SetY(theGround->GetGroundTarget().GetY());
    } else if (theUnload != NULL)
        theUnload->GetGoalLocation(theTarget);
    else {
        theUnit = theAttack->GetAttackTarget();
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

// Gives the ground height under a point (the closest tile's off the map; 0 under water)
void EEUnitClass::GetGroundHeight(GE3DPoint& inPoint, float* outHeight)
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

// Gets the unit's next waypoint and the climb to it; returns nonzero when there is none
long EEUnitClass::GetNextFlightPoint(EEComplexUnit* inUnit, GE3DPoint& outTarget, float* outClimb)
{
    GE3DPoint thePosition;
    inUnit->GetPosition(thePosition);
    long theResult = inUnit->GetNextWaypointPoint(outTarget);
    if (theResult == 0)
        *outClimb = outTarget.GetZ() - thePosition.GetZ();
    return theResult;
}

// New (R5b.4, the fourteenth round): EEUCLandable::ActionChanged's operations that are more than a call, each the
// original's statements of its branch (ee_units actions.rs decides which, in which order; EEUnitUpdate.cpp's host
// calls this with the operation code). inoutAirport: FIND_NEAREST_AIRPORT's result, for SET_HOME_BASE_FOUND.
int64_t EELandableActionOperation(int inOperation, EEUnitClass* inClass, EEComplexUnit* inUnit, int64_t inA,
                                  EEComplexUnit** inoutAirport)
{
    EEUnitClass* theClass = inClass;
    GE3DPoint theTarget;
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          inUnit->mPhysicalModel.GetLocation().GetZ());
    EEAirTrafficControl* theAirTraffic = inUnit->mPlayer->mAirTraffic[inUnit->mUnitClass->mAirTrafficType - 1];
    EEUnitGoal* theGoal = inUnit->mGoalState;
    EEUnit* theEnemy;
    switch (inOperation) {
    case EE_UNITS_ACT_APPROACH_CARRIER:
        theClass->ApproachCarrier(inUnit, inA != 0);
        break;
    case EE_UNITS_ACT_BOARD_CARRIER:
        theAirTraffic->RemovePlane(inUnit->mHomeBase, inUnit);
        inUnit->mHomeBase->LoadCargo(inUnit, 1, 0);   // the carrier takes the plane aboard
        inUnit->SetHomeBase(NULL);
        inUnit->SetIsCommandable(true);
        inUnit->SetHeight(0.0f);
        break;
    case EE_UNITS_ACT_TAKE_OFF_FROM_CARRIER:
        theClass->TakeOffFromCarrier(inUnit);
        break;
    case EE_UNITS_ACT_FLY_CHECK_PATH: {
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
    case EE_UNITS_ACT_FIND_NEAREST_AIRPORT:
        *inoutAirport = theAirTraffic->FindNearestAirport(inUnit);
        return *inoutAirport != NULL ? 1 : 0;
    case EE_UNITS_ACT_SET_HOME_BASE_FOUND:
        inUnit->SetHomeBase(*inoutAirport);
        break;
    case EE_UNITS_ACT_APPROACH_LANDING:
        if (inUnit->mUnitClass->mAirTrafficType == 1 || inUnit->mUnitClass->mAirTrafficType == 3) {
            switch (inUnit->mHomeBase->mFacing) {
            case 0:
                theClass->ApproachRunwayPosY(inUnit);
                break;
            case 1:
                theClass->ApproachRunwayNegY(inUnit);
                break;
            case 2:
                theClass->ApproachRunwayPosX(inUnit);
                break;
            default:
                theClass->ApproachRunwayNegX(inUnit);
                break;
            }
        } else
            theClass->ApproachCarrier(inUnit, inA != 0);
        break;
    case EE_UNITS_ACT_SECONDARY_TARGET: {
        EEUGRust* theAttack = EEAttackUnitCast(theGoal);
        if (theAttack == NULL)
            break;
        EEUnit* theTargetUnit = theAttack->GetAttackTarget();
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
                theAttack->SetAttackTarget(theEnemy);
                break;
            }
        } else {
            inUnit->ClearGoals(0);
            break;
        }
        if (theAttack->GetAttackTarget()->GetTileX() != -10000) {
            EEUGRust* theMove = EEUGRust::NewLocation((float)theAttack->GetAttackTarget()->GetTileX() + 0.5f,
                                                     (float)theAttack->GetAttackTarget()->GetTileY() + 0.5f, theClass->GetCruiseAltitude(),
                                                     theAttack->GetB1C(), true, false, 0.0f, false, false);
            inUnit->SetGoal(theMove, 0, 0);
            inUnit->StartAction(theMove->GetStartAction());
            theClass->ActionChanged(inUnit);
            break;
        }
        inUnit->ClearGoals(0);
        break;
    }
    case EE_UNITS_ACT_TAKEOFF:
        {
            if (inUnit->mHomeBase == NULL) {
                if (theAirTraffic->GetNumSlots() == 0)
                    inUnit->Kill(true, false, false);
                else {
                    if (inUnit->mContainer == NULL)
                        inUnit->SetHomeBase(theAirTraffic->GetSlotAirport(0));
                    else {
                        for (unsigned long i = 0; i < theAirTraffic->GetNumSlots(); i++) {
                            if (inUnit->mContainer == theAirTraffic->GetSlotAirport(i))
                                inUnit->SetHomeBase(theAirTraffic->GetSlotAirport(i));
                        }
                    }
                    theClass->LandableReturnToAirport(inUnit);
                }
            } else if (inUnit->mUnitClass->mAirTrafficType == 1 || inUnit->mUnitClass->mAirTrafficType == 3) {
                switch (inUnit->mHomeBase->mFacing) {
                case 0:
                    theClass->TakeOffFromRunwayPosY(inUnit);
                    break;
                case 1:
                    theClass->TakeOffFromRunwayNegY(inUnit);
                    break;
                case 2:
                    theClass->TakeOffFromRunwayPosX(inUnit);
                    break;
                default:
                    theClass->TakeOffFromRunwayNegX(inUnit);
                    break;
                }
            } else
                theClass->TakeOffFromCarrier(inUnit);
            if (inUnit->mHomeBase == NULL)
                return 0;
            inUnit->SetFlightTime(inUnit->GetStats()->mFlightTime);
            if (!theClass->mIsParatrooperBomber)
                inUnit->SetAmmunition(theClass->mAmmunition);
            inUnit->SetAnimation(0xd, -1, 0);
            if (inUnit->GetNextWaypointPoint(thePosition) != 0)
                return 0;
            inUnit->SetHeight(theClass->GetHeightAt(inUnit, thePosition));
            return 0;
        }
        break;
    case EE_UNITS_ACT_QUEUE_LANDING:
        return (int64_t)theAirTraffic->QueueRequest(inUnit->mHomeBase, inUnit, 1);
    case EE_UNITS_ACT_BOMBER_GROUND_GOAL:
        EEFindDropLocation(inUnit, &theEnemy, &theTarget, NULL);
        inUnit->SetGoal(EEUGRust::NewAttackGround(theTarget, theGoal->GetB1C()), 0, 0);
        break;
    case EE_UNITS_ACT_INITIAL_ATTACK_HEIGHT:
        if (inUnit->mHeight < theClass->GetCruiseAltitude() && theClass->GetCruiseAltitude() != inUnit->mHeight)
            inUnit->mHeight = theClass->GetCruiseAltitude();
        break;
    case EE_UNITS_ACT_CONTACT_HERE:
        if (inA != 0)
            inUnit->SetInitialContactLocation(thePosition);
        else if (inUnit->mInitialContactLocation != thePosition)
            inUnit->mInitialContactLocation = thePosition;
        if (inUnit->mInitialContactHeading != 0.0f)
            inUnit->mInitialContactHeading = 0.0f;
        break;
    case EE_UNITS_ACT_LANDABLE_PATH:
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
                theTarget.SetZ(theClass->GetCruiseAltitude());
        }
        theClass->PathToward(inUnit, thePosition, theTarget, true);
        break;
    case EE_UNITS_ACT_DEATH_ROLL: {
        if (theClass->mPhysicsModel != NULL && theClass->mPhysicsModel->IsAirplane())
            inUnit->SetF220(theClass->mPhysicsModel->ComputeRoll(inUnit, inUnit->mAngularVelocity.GetYaw()));
        GEOrientation theOrientation = inUnit->mAngularVelocity;
        theOrientation.SetRoll(theOrientation.GetYaw());
        inUnit->mAngularVelocity = theOrientation;
        break;
    }
    default:
        break;
    }
    return 0;
}

// Sets the unit's initial contact location
void EEComplexUnit::SetInitialContactLocation(const GE3DPoint& inPoint)
{
    if (this->mInitialContactLocation != inPoint)
        this->mInitialContactLocation = inPoint;
}

// Sets up a new plane: adds its weight to its air traffic control and starts it on the ground
void EEUnitClass::LandableUnitCreated(EEComplexUnit* inUnit)
{
    this->BaseUnitCreated(inUnit);
    inUnit->GetPlayer()->mAirTraffic[inUnit->GetUnitClass()->mAirTrafficType - 1]->AddCargoWeight(inUnit);
    inUnit->SetHeight(0.0f);
}

// Returns whether a bomb dropped now lands within reach of the attack target along its ballistic arc (reach:
// half the area effect for the last bomb on a moving target, else 0.25 for the last bomb or none)
bool EEUnitClass::IsBombOnTarget(EEComplexUnit* inUnit)
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
    EEUGRust* theAttack = EEAttackUnitCast(inUnit->mGoalState);
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

// Plans a plane's approach to its home carrier's deck (tag point 0): turn, descent and touch-down (inSteep: the
// full approach whatever the height), or just the deck when braking; makes the deck the move goal
void EEUnitClass::ApproachCarrier(EEComplexUnit* inUnit, bool inSteep)
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
    EEUnitGoal* theLocation = NULL;   // a location goal or a heal goal
    if (inUnit->mGoalState != NULL && EEGoalHasLocation(inUnit->GetGoal()))
        theLocation = inUnit->GetGoal();
    if (theLocation == NULL) {
        EEUGRust* theMove = EEUGRust::NewLocation(&theDeck, false, true, false, 0.0f, false, false);
        inUnit->ClearGoalQueue(0);
        inUnit->SetGoal(theMove, 0, 0);
    } else
        EESetLocationGoalPoint(theLocation, theDeck);
}

// Plans a take-off from the home carrier: on take-off, sits at the deck start (tag point 0) and heads for the
// take-off point; otherwise flies off the deck end (tag point 2) towards the goal
void EEUnitClass::TakeOffFromCarrier(EEComplexUnit* inUnit)
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

// Returns 0 (left) or 1 (right) when the next waypoint or goal lies inside the turning circle beside or behind
// the plane, else 5
unsigned long EEUnitClass::GetNextFlightState(EEComplexUnit* inUnit)
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
            theTarget = static_cast<EEUGRust*>(theGoal)->GetGroundTarget();
            break;
        case kGoalAttackUnit: {
            EEUnit* theUnit = static_cast<EEUGRust*>(theGoal)->GetAttackTarget();
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
            EEComplexUnit* theOther = static_cast<EEUGRust*>(theGoal)->GetPatrolTarget();
            const GETransformation& theOtherLocation = theOther->mPhysicalModel.GetLocation();
            theTarget.SetX(theOtherLocation.GetX());
            theTarget.SetY(theOtherLocation.GetY());
            theTarget.SetZ(theOtherLocation.GetZ());
            break;
        }
        case kGoalExplore: {
            static_cast<EEUGRust*>(theGoal)->GetExploreTileCenter(theTarget);
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

// Returns whether the plane reached its next waypoint (within 0.5); braking onto a runway's last one, whether
// inValue is at most 0.05
bool EEUnitClass::LandableHasArrived(EEComplexUnit* inUnit, float inValue)
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

// Plans a land plane's runway approach in a direction: turn onto the line, descent and touch-down at the airport
// (a navy yard: its pier), then makes the airport the move goal (the queue kept behind an explore goal)
void EEUnitClass::ApproachRunway(EEComplexUnit* inUnit, long inDX, long inDY)
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
    EEUGRust* theMove = EEUGRust::NewLocation(&theWaypoint, false, true, false, 0.0f, false, false);
    bool theKeep;
    if (inUnit->mGoalState == NULL || inUnit->mGoalState->mKind != kGoalExplore) {
        inUnit->ClearGoalQueue(0);
        theKeep = false;
    } else
        theKeep = true;
    inUnit->SetGoal(theMove, theKeep, 0);
}

// Plans the runway approach along +x
void EEUnitClass::ApproachRunwayPosX(EEComplexUnit* inUnit)
{
    this->ApproachRunway(inUnit, 1, 0);
}

// Plans the runway approach along +y
void EEUnitClass::ApproachRunwayPosY(EEComplexUnit* inUnit)
{
    this->ApproachRunway(inUnit, 0, 1);
}

// Plans the runway approach along -y
void EEUnitClass::ApproachRunwayNegY(EEComplexUnit* inUnit)
{
    this->ApproachRunway(inUnit, 0, -1);
}

// Plans the runway approach along -x
void EEUnitClass::ApproachRunwayNegX(EEComplexUnit* inUnit)
{
    this->ApproachRunway(inUnit, -1, 0);
}

// Plans a runway take-off in a direction: rolls 3 tiles low, climbs to 3/4 cruising height 7 tiles out, then
// flies to the goal
void EEUnitClass::TakeOffFromRunway(EEComplexUnit* inUnit, long inDX, long inDY)
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

// Plans the runway take-off along +x
void EEUnitClass::TakeOffFromRunwayPosX(EEComplexUnit* inUnit)
{
    this->TakeOffFromRunway(inUnit, 1, 0);
}

// Plans the runway take-off along +y
void EEUnitClass::TakeOffFromRunwayPosY(EEComplexUnit* inUnit)
{
    this->TakeOffFromRunway(inUnit, 0, 1);
}

// Plans the runway take-off along -y
void EEUnitClass::TakeOffFromRunwayNegY(EEComplexUnit* inUnit)
{
    this->TakeOffFromRunway(inUnit, 0, -1);
}

// Plans the runway take-off along -x
void EEUnitClass::TakeOffFromRunwayNegX(EEComplexUnit* inUnit)
{
    this->TakeOffFromRunway(inUnit, -1, 0);
}

// Returns whether the move goal is more than 1 from the home carrier's deck point (tag point 0): the carrier moved
bool EEUnitClass::IsAwayFromGoal(EEComplexUnit* inUnit)
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
    if (!(theDeck.DistanceToSquared(EELocationGoalPoint(inUnit->GetGoal())) <= 1.0f))
        theResult = true;
    else
        theResult = false;
    return theResult;
}

// Answers the plane's flight triggers (waypoint on ground, in air, higher, same, lower; bomb in range; fuel alarm;
// clearance; airport destroyed; secondary target; controller moved; request queued); others the common way
long EEUnitClass::LandableEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
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
        theResult = this->BaseEvaluateCondition(inUnit, inEvent);
        break;
    case kTriggerUnitInWeaponRange:
        if (this->mProjectileClass != NULL && this->mProjectileClass->mCategory == kUnitCategoryBomb)
            theFlag = inUnit->mAmmunition < this->mAmmunition || this->IsBombOnTarget(inUnit);
        else
            theResult = this->BaseEvaluateCondition(inUnit, inEvent);
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
        EEUGRust* theAttack = EEAttackUnitCast(inUnit->GetGoal());
        theFlag = theAttack != NULL && theAttack->GetAttackTarget() != NULL && theAttack->GetAttackTarget()->mHitPoints > 0;
        break;
    }
    case kTriggerAirtrafficControllerMoved: {
        EEUnitGoal* theGoal = inUnit->GetGoal();
        if (inUnit->mHomeBase != NULL) {
            long theCategory = inUnit->mHomeBase->GetUnitClass()->mCategory;
            if ((theCategory == kUnitCategoryNavalBuilding || theCategory == kUnitCategorySpaceBuilding) && inUnit->mHomeBase->GetUnitClass()->mAirTrafficType != 0
                && EEGoalHasLocation(theGoal))
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

// ---- EEUCAirplane.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The airplane's unit
// class: 0x20b192..0x20b9fc (after EEUCAirAnimal.cpp; EEUCAmbient.cpp follows). Unit flags: the exe's module set. Its
// ctor is inline (EEUnitClass.cpp's factory). Order: Virtual74 with EEAirTrafficControl::GetNumSlots's COMDAT after
// it, EEIsTileFree, EECreateCarrierAircraft (the carrier's aircraft), FlyOffMap (off the map edge), VirtualBC, VirtualB8.
// Virtual74 is EEUCPlanetSpaceship's twin (0x218e34).

// 0x446fcc: "no height" (one .rdata copy per unit: a named const; provisional name)
static const float kAirplaneNoHeight = -10000.0f;

// New (R5b.4, the fourteenth round): EEUCAirplane::ActionChanged's operations that are more than a call (the original's
// statements; ee_units actions.rs decides when, EEUnitUpdate.cpp's host calls them): circling's side (on space maps
// toward the side it can reach; EE_UNITS_ACT_CIRCLE_SIDE), the holding pattern over its home airport (killed when
// there is none; EE_UNITS_ACT_HOLDING_PATTERN 0)
void EEAirplaneCircleSide(EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    (void)inClass;
    if (::gWorld.mMap->mTerrainMesh.GetIsSpaceMap()) {
        float theDistance = inUnit->mStats->GetMaxVelocity() / inUnit->mStats->GetMaxAngularVelocity();
        theDistance = (float)UMCeil(theDistance);
        GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
        float theAngle = UMArcTan2(theTransformation.GetElement(1, 0), theTransformation.GetElement(0, 0));
        float theLeft = theAngle - 1.5707964f;
        float theRight = theAngle + 1.5707964f;
        bool theLeftNear = gContinentManager.CanGetTo(inUnit,
            (long)(theTransformation.GetX() - UMSin(theLeft) * theDistance),
            (long)(UMCos(theLeft) * theDistance + theTransformation.GetY()));
        bool theRightNear = gContinentManager.CanGetTo(inUnit,
            (long)(theTransformation.GetX() - UMSin(theRight) * theDistance),
            (long)(UMCos(theRight) * theDistance + theTransformation.GetY()));
        if (theLeftNear && !theRightNear)
            inUnit->SetCurrentPhysicsOrder(0);
        else if (!theLeftNear && theRightNear)
            inUnit->SetCurrentPhysicsOrder(1);
        else {
            theLeftNear = gContinentManager.CanGetTo(inUnit,
                (long)(theTransformation.GetX() - UMSin(theLeft) * theDistance * 2.0f),
                (long)(UMCos(theLeft) * theDistance * 2.0f + theTransformation.GetY()));
            theRightNear = gContinentManager.CanGetTo(inUnit,
                (long)(theTransformation.GetX() - UMSin(theRight) * theDistance * 2.0f),
                (long)(UMCos(theRight) * theDistance * 2.0f + theTransformation.GetY()));
            if (theLeftNear && !theRightNear)
                inUnit->SetCurrentPhysicsOrder(0);
            else if (!theLeftNear && theRightNear)
                inUnit->SetCurrentPhysicsOrder(1);
            else if (gPhysics->mRandom.Generate() < 0.5f)
                inUnit->SetCurrentPhysicsOrder(0);
            else
                inUnit->SetCurrentPhysicsOrder(1);
        }
    } else if (gPhysics->mRandom.Generate() < 0.5f)
        inUnit->SetCurrentPhysicsOrder(0);
    else
        inUnit->SetCurrentPhysicsOrder(1);
}

void EEAirplaneHoldingPattern(EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    EEUnitClass* theClass = inClass;
    GE3DPoint theTarget(0.0f, 0.0f, 0.0f);
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          inUnit->mPhysicalModel.GetLocation().GetZ());
    EEAirTrafficControl* theControl = inUnit->mPlayer->mAirTraffic[inUnit->mUnitClass->mAirTrafficType - 1];
    if (inUnit->mHomeBase == NULL) {
        if (theControl->GetNumSlots() > 0)
            inUnit->SetHomeBase(theControl->GetSlotAirport(0));
        else
            inUnit->Kill(true, false, false);
        if (inUnit->mHomeBase == NULL)
            return;
    }
    inUnit->DropPath();
    theTarget.SetX(inUnit->mHomeBase->GetTileX() + 1.5f);
    theTarget.SetY(inUnit->mHomeBase->GetTileY() + 1.5f);
    theTarget.SetZ(theClass->GetCruiseAltitude());
    theClass->PathToward(inUnit, thePosition, theTarget, true);
    if (gPhysics->mRandom.Generate() < 0.5f)
        inUnit->SetCurrentPhysicsOrder(0);
    else
        inUnit->SetCurrentPhysicsOrder(1);
    inUnit->SetIsCommandable(false);
    if (inUnit->GetNextWaypointPoint(thePosition) == 0)
        inUnit->SetHeight(theClass->GetHeightAt(inUnit, thePosition));
}

// Returns whether a tile is on the map, not cliff, deep water or space, and not on the map edge
bool EEIsTileFree(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY)
{
    EEMapTile* theTile = ::gWorld.mMap->GetTile(inX, inY);
    // The terrain and edge checks keep the original short-circuit order.
    return theTile != NULL && theTile->mTerrainType != kTerrainCliff && theTile->mTerrainType != kTerrainDeepWater && theTile->mTerrainType != kTerrainSpace &&
           !::gWorld.mMap->IsEdgeTile(inX, inY);
}

// Fills a new carrier with aircraft of the best of three fighter techs its player has (else unit class 0x1f)
void EECreateCarrierAircraft(EEComplexUnit* inUnit, long inPlayer)
{
    GETransformation theTransformation;
    long thePlayerID = inPlayer;
    if (thePlayerID == -1)
        thePlayerID = inUnit->mPlayer->mPlayerID;
    EEPlayer* thePlayer = ::gWorld.mPlayers[thePlayerID];
    long theType;
    if (thePlayer->mTechnologies->Get(kTechInf15Watchman)->GetIsAvailable() && thePlayer->mTechnologies->Get(kTechInf15Watchman)->mIsAllowed)
        theType = thePlayer->mTechnologies->Get(kTechInf15Watchman)->mData->mUnitClass;
    else if (thePlayer->mTechnologies->Get(kTechInf14Guardian)->GetIsAvailable() && thePlayer->mTechnologies->Get(kTechInf14Guardian)->mIsAllowed)
        theType = thePlayer->mTechnologies->Get(kTechInf14Guardian)->mData->mUnitClass;
    else if (thePlayer->mTechnologies->Get(kTechInf13Sentinel)->GetIsAvailable() && thePlayer->mTechnologies->Get(kTechInf13Sentinel)->mIsAllowed)
        theType = thePlayer->mTechnologies->Get(kTechInf13Sentinel)->mData->mUnitClass;
    else
        theType = 0x1f;
    EEUnitClass* theClass = gUnitClassTable[theType];
    theTransformation.SetX(-10000.0f);
    theTransformation.SetY(-10000.0f);
    for (long i = inUnit->GetNumCargoUnits(); i < inUnit->mUnitClass->mTransportCapacity; i++) {
        EEUnit* theAircraft = theClass->CreateUnitAt(theTransformation, thePlayerID, 0, inUnit);
        inUnit->LoadCargo(theAircraft->mComplexUnit, 1, 0);
    }
}

// Sends a leaving plane off the map: hands it to player 0, fuels it for 1000 s and flies it out 1000 tiles along
// its direction from the map origin
void EEUnitClass::FlyOffMap(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition;
    GEVector theDirection;
    inUnit->GetPosition(thePosition);
    theDirection.SetX(thePosition.GetX());
    theDirection.SetY(thePosition.GetY());
    theDirection.Normalize();
    long theX = UMFloor(theDirection.GetX() * 1000.0f);
    long theY = UMFloor(theDirection.GetY() * 1000.0f);
    if (::gWorld.mMap->GetTile(theX, theY) != NULL) {
        theX = -10000;
        theY = -10000;
    }
    EEComplexUnit* theUnit = EEConvertUnit(inUnit, 0, 0);
    theUnit->SetFlightTime(1000000);
    EEUGRust* theGoal = EEUGRust::NewLocation(theX + 0.5f, theY + 0.5f, 0.0f, true, true, false, 0.0f, false, false);
    theUnit->DropPath();
    this->AddWaypointAt(theUnit, theX + 0.5f, theY + 0.5f, this->GetCruiseAltitude(), true);
    theUnit->SetGoal(theGoal, 0, 0);
    theUnit->StartAction(kActionFly);
    theUnit->mUnitClass->ActionChanged(theUnit);
}

// ---- EEUCAirAnimal.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The flying animal's
// unit class: 0x20a279..0x20b192 (after EESelectedGroup.cpp's sort COMDATs; EEUCAirplane.cpp follows). One unit per
// EEUC* class in alphabetical order (AirAnimal, Airplane, Ambient, BasicCitizen, BasicPriest, Bomb, Building, ...).
// Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); ReturnToAirport (the landing on an
// airport, used by Virtual74) comes first, then the slots +0x24, +0x2c, +0x74, +0x78, +0x88, +0xa0, (+0xc0 folded)
// +0xb8 in source order. Bodies are the EEUCLandable twins' (0x213ed2.., same sizes) with the animal's states.

// 0x446fc0..0x446fc8: the unit's named float constants (one .rdata block per unit)
static const float kAirAnimalHeight = 12.0f;     // 0x446fc0 (the highest an animal flies)

static const float kAirAnimalAhead = 2.0f;       // 0x446fc4 (the look-ahead beyond the step)

static const float kAirAnimalDoppler = 0.0025f;  // 0x446fc8 (0.05 squared)

// Lands the animal like a plane: two waypoints ahead (on the ground, then at 3/4 of cruise altitude) and a flight there
bool EEUnitClass::AirAnimalReturnToAirport(EEComplexUnit* inUnit)
{
    GE3DPoint thePoint;
    GEOrientation theOrientation;
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    if (inUnit == NULL || inUnit->mPlayer->mAirTraffic[inUnit->mUnitClass->mAirTrafficType - 1] == 0)
        return true;
    inUnit->DropPath();
    theTransformation.Apply(GE3DPoint(0.0f, 3.0f, 0.0f), thePoint);
    this->AddWaypointAt(inUnit, thePoint.GetX(), thePoint.GetY(), inUnit->mTile->GetHeight(), true);
    theTransformation.Apply(GE3DPoint(0.0f, 7.0f, 0.0f), thePoint);
    float theHeight = this->GetCruiseAltitude() * 0.75f;
    if (inUnit->mTile->GetHeight() > theHeight)
        theHeight = inUnit->mTile->GetHeight() + theHeight;
    this->AddWaypointAt(inUnit, thePoint.GetX(), thePoint.GetY(), theHeight, true);
    thePoint.SetZ(theHeight);
    theTransformation.ConvertToOrientation(theOrientation);
    inUnit->mOrientation = theOrientation;
    this->FlyToGoal(thePoint, inUnit);
    return true;
}

// Returns the height to fly at toward a point: its height above ground kept over the highest terrain on the way
// (the circle ahead when circling, else the tile line), capped at 12
float EEUnitClass::AirAnimalGetHeightAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{
    GE3DPoint thePosition;
    GEVector theStep;
    GEVector theOffset;
    std::vector<EETileCoordinate> theTiles;
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    thePosition.SetX(theLocation.GetX());
    EEMap* theMap = ::gWorld.mMap;
    EEUnitClassUpgrade* theStats = inUnit->mStats;
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
        EEGetTilesNearPoint(thePosition, kAirAnimalAhead + theDistance, theTiles);
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
    if (theHeight > kAirAnimalHeight)
        theHeight = kAirAnimalHeight;
    return theHeight;
}

// Gets the base sound parameters, then Doppler-shifts the pitch of a moving animal by its speed toward the camera
void EEUnitClass::AirAnimalGetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
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
    this->BaseGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
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
    float theGamma = (float)sqrt(1.0f - theVelocity.GetY() * theVelocity.GetY() * kAirAnimalDoppler);
    *outPitch = theGamma / (1.0f - theRatio * ((theRatio + theCosine) / (1.0f - theRatio * theCosine)));
}

// New (R5b.4, the fourteenth round): EEUCAirAnimal::ActionChanged's operations that are more than a call (the
// original's statements; ee_units actions.rs decides when, EEUnitUpdate.cpp's host calls them): the path to the goal
// when preparing to move (EE_UNITS_ACT_AIR_ANIMAL_PATH), the dying spin (EE_UNITS_ACT_DEATH_ROLL 0)
void EEAirAnimalPathToGoal(EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    EEUnitClass* theClass = inClass;
    GE3DPoint theTarget;
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          inUnit->mPhysicalModel.GetLocation().GetZ());
    EEUnitGoal* theGoal = inUnit->mGoalState;
    if (theGoal->mKind == kGoalAttackUnit) {
        EEUnit* theUnit;
        EEFindDropLocation(inUnit, &theUnit, &theTarget, NULL);
        float theRange = inUnit->mStats->mRange;
        float theRange2 = theRange;
        if (EEGetHitDistanceSquared(inUnit, theUnit, -10000.0f, -10000.0f) > theRange * theRange2)
            theGoal->GetDestination(inUnit, theTarget);
    } else
        theGoal->GetDestination(inUnit, theTarget);
    if (theGoal->mKind == kGoalExplore)
        theTarget.SetZ(theClass->GetCruiseAltitude());
    theClass->PathToward(inUnit, thePosition, theTarget, true);
}

void EEAirAnimalDeathRoll(EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    inUnit->SetF220(inClass->mPhysicsModel->ComputeRoll(inUnit, inUnit->mAngularVelocity.GetYaw()));
    inUnit->mAngularVelocity = GEOrientation(inUnit->mAngularVelocity.GetYaw(), inUnit->mAngularVelocity.GetPitch(), inUnit->mAngularVelocity.GetYaw());
}

// Sets up a new animal on the ground (height 0)
void EEUnitClass::AirAnimalUnitCreated(EEComplexUnit* inUnit)
{
    this->BaseUnitCreated(inUnit);
    inUnit->SetHeight(0.0f);
}

// Returns whether the animal reached its next waypoint (within 0.5 tiles), or when braking on its last one, has stopped
bool EEUnitClass::AirAnimalHasArrived(EEComplexUnit* inUnit, float inValue)
{
    GE3DPoint theTarget;
    unsigned long theCount = inUnit->mWaypoints.size();
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          0.0f);
    inUnit->GetNextWaypointPoint(theTarget);
    theTarget.SetZ(0.0f);
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionBrake && theCount <= 1 && this->mAirTrafficType != 2) {
        return inValue == 0.0f;
    }
    return theTarget.DistanceToSquared(thePosition) <= 0.25f;
}

// Answers the flight triggers (next waypoint lower, level, higher, in the air or on the ground; move prepared; fall
// finished); other triggers go to the common unit class
long EEUnitClass::AirAnimalEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    float theZ = inUnit->mPhysicalModel.GetLocation().GetZ();
    unsigned long theCount = inUnit->mWaypoints.size();
    GE3DPoint theTarget;
    long theResult = 0;
    bool theFlag = false;
    float theDelta = 0.0f;
    float theGround = 0.0f;
    if (inUnit->GetNextWaypointPoint(theTarget) == 0) {
        theDelta = theTarget.GetZ() - theZ;
        long theX;
        long theY;
        EEMapTile* theTile = ::gWorld.mMap->GetTile(UMFloor(theTarget.GetX()), UMFloor(theTarget.GetY()));
        if (theTile == NULL)
            theTile = ::gWorld.mMap->GetClosestTile(&theTarget, &theX, &theY);
        theGround = theTile->GetHeight();
        if (theGround < 0.0f)
            theGround = 0.0f;
    }
    switch (inEvent) {
    case kTriggerWaypointIsLowerElevation:
        if (inUnit->mUnitClass->mAirTrafficType != 2)
            theFlag = inUnit->mHasArrived && theCount > 0 && theDelta < 0.0f;
        else
            theFlag = theCount > 0;
        break;
    case kTriggerWaypointIsSameElevation:
        if (inUnit->mUnitClass->mAirTrafficType != 2)
            theFlag = inUnit->mHasArrived && theCount > 0 && theDelta == 0.0f;
        else
            theFlag = theCount <= 1;
        break;
    case kTriggerWaypointIsHigherElevation:
        theFlag = inUnit->mHasArrived && theCount > 0 && theDelta > 0.0f;
        break;
    case kTriggerWaypointInAir:
        theFlag = inUnit->mHasArrived && theCount > 0 && theTarget.GetZ() > theGround;
        break;
    case kTriggerWaypointOnGround:
        theFlag = inUnit->mHasArrived && theCount > 0 && theTarget.GetZ() == theGround;
        break;
    case kTriggerMovePreparationComplete:
        theFlag = (long)inUnit->mWaypoints.size() > 0;
        break;
    case kTriggerDyingAnimationFinished:
        theFlag = inUnit->mVelocity == 0.0f;
        break;
    default:
        theResult = this->BaseEvaluateCondition(inUnit, inEvent);
    }
    if (theFlag)
        theResult = 1;
    return theResult;
}

// ---- EEUCPlanetSpaceship.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The planet
// spaceship's unit class: 0x218e34..0x219143 (after EEUCNavalTransport.cpp; EEUCProjectile.cpp's +0xe0 0x219143
// follows). Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); one own slot, +0x74.

// 0x447330: the height a landed spaceship's mInitialContactHeading is reset to (one .rdata copy per unit: a named const)
static const float kPlanetSpaceshipNoHeight = -10000.0f;

// New (R5b.4, the fourteenth round): EEUCPlanetSpaceship::ActionChanged's holding pattern over its home airport (dies
// without one; the original's statements; EEUnitUpdate.cpp's EE_UNITS_ACT_HOLDING_PATTERN 1 calls it)
void EEPlanetSpaceshipHoldingPattern(EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    EEUnitClass* theClass = inClass;
    GE3DPoint theTarget(0.0f, 0.0f, 0.0f);
    EEAirTrafficControl* theControl = inUnit->mPlayer->mAirTraffic[inUnit->mUnitClass->mAirTrafficType - 1];
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          inUnit->mPhysicalModel.GetLocation().GetZ());
    if (inUnit->mHomeBase == NULL) {
        if (theControl->GetNumSlots() > 0)
            inUnit->SetHomeBase(theControl->GetSlotAirport(0));
        else
            inUnit->Kill(true, false, false);
    }
    if (inUnit->mHomeBase != NULL) {
        inUnit->DropPath();
        theTarget.SetX(inUnit->mHomeBase->mTileX + 1.5f);
        theTarget.SetY(inUnit->mHomeBase->GetTileY() + 1.5f);
        theTarget.SetZ(theClass->GetCruiseAltitude());
        theClass->PathToward(inUnit, thePosition, theTarget, true);
        inUnit->SetIsCommandable(false);
        if (inUnit->GetNextWaypointPoint(thePosition) == 0)
            inUnit->SetHeight(theClass->GetHeightAt(inUnit, thePosition));
    }
}

// ---- EEUCHelicopter.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The helicopter's unit
// class: 0x212a6f..0x212edf (after EEUCGate.cpp; EEUCHelicopterPriest.cpp follows). One unit per EEUC* class in
// alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); slots +0x24,
// +0x74, +0xb8 in source order (the others are ICF-folded into twins).

// 0x447020: the highest a helicopter flies (one .rdata copy per unit: a named const) (provisional name)
static const float kHelicopterCeiling = 12.0f;

// 0x249560 (cdecl): the point to fly to for the attack on inTarget (EEMap.cpp's range) (provisional name)
void EEGetAimPoint(EEComplexUnit* inUnit, GE3DPoint* inTarget, GE3DPoint* outLocation);

// Returns the height to fly at towards a point: cruising height above the highest tile corner on the way, at least
// the point's height, kept between cruising height and 12
float EEUnitClass::HelicopterGetHeightAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{
    std::vector<U2DPoint<long> > theTiles;
    EEMap* theMap = ::gWorld.mMap;
    float theHeight = inLocation.GetZ();
    float theBase = this->mCruiseAltitude;
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    GE3DPoint thePosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    EEGetTileLine(UMFloor(thePosition.GetX()), UMFloor(thePosition.GetY()), UMFloor(inLocation.GetX()),
               UMFloor(inLocation.GetY()), &theTiles, 1, 0);
    for (std::vector<U2DPoint<long> >::iterator it = theTiles.begin(); it != theTiles.end(); it++) {
        unsigned long theX = it->mX;
        unsigned long theY = it->mY;
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
    if (theHeight >= kHelicopterCeiling)
        theHeight = kHelicopterCeiling;
    if (theHeight <= theBase)
        theHeight = theBase;
    return theHeight;
}

// New (R5b.4, the fourteenth round): EEUCHelicopter::ActionChanged's path to the goal when preparing to move (the
// original's statements; ee_units actions.rs decides when, EEUnitUpdate.cpp's EE_UNITS_ACT_HELICOPTER_PATH calls it)
void EEHelicopterPathToGoal(EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    EEUnitClass* theClass = inClass;
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          inUnit->mPhysicalModel.GetLocation().GetZ());
    GE3DPoint theTarget;
    GE3DPoint theEnemy;
    EEUnit* theUnit;
    EEUnitGoal* theGoal = inUnit->mGoalState;
    if (theGoal->mKind == kGoalAttackUnit) {
        EEFindDropLocation(inUnit, &theUnit, &theEnemy, NULL);
        EEGetAimPoint(inUnit, &theEnemy, &theTarget);
    } else
        theGoal->GetDestination(inUnit, theTarget);
    if (theGoal->mKind == kGoalExplore)
        theTarget.SetZ(theClass->mCruiseAltitude);
    EEFindLandingPoint(inUnit, &theTarget);
    theClass->PathToward(inUnit, thePosition, theTarget, false);
}

// ---- EEUCSpaceship.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The spaceship's unit
// class: 0x21a219..0x21a6e0 (after EEUCSpaceBuilding.cpp; EEUCSpaceTransport.cpp's +0xe4 0x21a6e0 follows). Unit
// flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); slots +0x24, +0x2c, +0x74, +0x78,
// +0xa0, +0xb8 in source order.

// 0x447358: the highest a spaceship flies over the terrain (one .rdata copy per unit: a named const)
static const float kSpaceshipHeight = 12.0f;

// Returns the flying height for a location: its z raised to clear every tile corner on the way by the cruise
// altitude, kept between the cruise altitude and 12
// The floor clamp is the class's mCruiseAltitude offset (the original keeps it on the FPU after the loop)
float EEUnitClass::SpaceshipGetHeightAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{
    std::vector<U2DPoint<long> > theTiles;
    float theHeight = inLocation.GetZ();
    EEMap* theMap = ::gWorld.mMap;
    float theOffset = this->mCruiseAltitude;
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    GE3DPoint thePosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    long theToY = UMFloor(inLocation.GetY());
    long theToX = UMFloor(inLocation.GetX());
    long theFromY = UMFloor(thePosition.GetY());
    long theFromX = UMFloor(thePosition.GetX());
    EEGetTileLine(theFromX, theFromY, theToX, theToY, &theTiles, 1, 0);
    for (std::vector<U2DPoint<long> >::iterator theTile = theTiles.begin(); theTile != theTiles.end(); theTile++) {
        long theX = theTile->GetX();
        long theY = theTile->GetY();
        if (theMap->GetTile(theX, theY) == NULL)
            continue;
        float theZ = theMap->mTerrainMesh.GetZ(theX, theY) + theOffset;
        if (theZ > theHeight)
            theHeight = theZ;
        theZ = theMap->mTerrainMesh.GetZ(theX + 1, theY) + theOffset;
        if (theZ > theHeight)
            theHeight = theZ;
        theZ = theMap->mTerrainMesh.GetZ(theX, theY + 1) + theOffset;
        if (theZ > theHeight)
            theHeight = theZ;
        theZ = theMap->mTerrainMesh.GetZ(theX + 1, theY + 1) + theOffset;
        if (theZ > theHeight)
            theHeight = theZ;
    }
    theHeight = min(theHeight, kSpaceshipHeight);
    theHeight = max(theHeight, theOffset);
    return theHeight;
}

// Returns a sound's volume, pan and pitch, with a doppler shift
void EEUnitClass::SpaceshipGetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
                              float* outPitch)
{
    this->BaseGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    EEGetDopplerPitch(inUnit, inSound, outVolume, outPan, outPitch);
}

// Answers dying done (always) and move preparation complete (has a path); others to the base class
long EEUnitClass::SpaceshipEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    bool theFlag = false;
    switch (inEvent) {
    case kTriggerDyingAnimationFinished:
        theFlag = true;
        break;
    case kTriggerMovePreparationComplete:
        theFlag = (long)inUnit->mWaypoints.size() > 0;
        break;
    default:
        theResult = this->BaseEvaluateCondition(inUnit, inEvent);
        break;
    }
    if (theFlag)
        theResult = 1;
    return theResult;
}

