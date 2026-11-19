// EEUnitClassAir.cpp (new, R5b.4 round 32): the aircraft and spaceships' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <typeinfo>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitClassKinds.h"

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

static const float kLandableAngle = 0.5235988f; // 0x447040 (the approach's 30 degrees)

// cdecl helpers (EEUCHelicopter.cpp declares the first the same way) (provisional names)
void EEFindLandingPoint(EEComplexUnit* inUnit, GE3DPoint* ioTarget);

// 0x24485b: the enemy unit near (inX, inY) within inRange that inUnit can attack (provisional name)
EEUnit* EEFindTargetNear(EEComplexUnit* inUnit, long inX, long inY, float inRange);

// Takes off from the ground: rolls 3 ahead, climbs 7 ahead to 3/4 cruising height (plus the ground height when the
// ground is higher), then flies to the goal; returns true, doing nothing without an air traffic control
bool EEUnitClass::LandableReturnToAirport(EEComplexUnit* inUnit)
{
    EEUCNote theNote(19); theNote.C(this).S(inUnit);   // TEST ONLY (UCBD)
    bool theResult = theNote.R([&]() -> bool {
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
    }());
    theNote.S(inUnit);
    return theResult;
}

// Drops the path and adds a waypoint at the home carrier's take-off point (its tag point 2; its centre without tags)
void EEUnitClass::RouteDockWaypoint(EEComplexUnit* inUnit)
{
    EEUCNote theNote(20); theNote.C(this).S(inUnit).U(inUnit->mHomeBase);   // TEST ONLY (UCBD)
    [&]() {
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
    }();
    theNote.S(inUnit);
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
            long theStep[2];
            ee_units_cls_runway_direction((uint32_t)inUnit->mHomeBase->mFacing, &theStep);
            theClass->ApproachRunway(inUnit, theStep[0], theStep[1]);
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
                long theStep[2];
                ee_units_cls_runway_direction((uint32_t)inUnit->mHomeBase->mFacing, &theStep);
                theClass->TakeOffFromRunway(inUnit, theStep[0], theStep[1]);
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
    EEUCNote theNote(17); theNote.C(this).S(inUnit);   // TEST ONLY (UCBD)
    return theNote.R([&]() -> bool {
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
    }());
}

// Plans a plane's approach to its home carrier's deck (tag point 0): turn, descent and touch-down (inSteep: the
// full approach whatever the height), or just the deck when braking; makes the deck the move goal
void EEUnitClass::ApproachCarrier(EEComplexUnit* inUnit, bool inSteep)
{
    EEUCNote theNote(13); theNote.C(this).S(inUnit).U(inUnit->mHomeBase).I(inSteep);   // TEST ONLY (UCBD)
    [&]() {
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
    }();
    theNote.S(inUnit);
}

// Plans a take-off from the home carrier: on take-off, sits at the deck start (tag point 0) and heads for the
// take-off point; otherwise flies off the deck end (tag point 2) towards the goal
void EEUnitClass::TakeOffFromCarrier(EEComplexUnit* inUnit)
{
    EEUCNote theNote(14); theNote.C(this).S(inUnit).U(inUnit->mHomeBase);   // TEST ONLY (UCBD)
    [&]() {
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
    }();
    theNote.S(inUnit).F(inUnit->mPhysicalModel.GetScale());
}

// Returns 0 (left) or 1 (right) when the next waypoint or goal lies inside the turning circle beside or behind
// the plane, else 5 (ee_units cls_air.rs, R5b.4 round 33)
unsigned long EEUnitClass::GetNextFlightState(EEComplexUnit* inUnit)
{
    EEUCNote theNote(12); theNote.C(this).S(inUnit);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_cls_next_flight_state(EEObj(inUnit)));
}

// Plans a land plane's runway approach in a direction: turn onto the line, descent and touch-down at the airport
// (a navy yard: its pier), then makes the airport the move goal (ee_units cls_air.rs, R5b.4 round 33). The four
// direction wrappers (0x616308..0x616338) and the take-off's (0x616430..0x616460) are the runway direction's table.
void EEUnitClass::ApproachRunway(EEComplexUnit* inUnit, long inDX, long inDY)
{
    EEUCNote theNote(15); theNote.C(this).S(inUnit).U(inUnit->mHomeBase).I(inDX).I(inDY);   // TEST ONLY (UCBD)
    ee_units_cls_approach_runway(EEObj(this), EEObj(inUnit), (int32_t)inDX, (int32_t)inDY);
    theNote.S(inUnit);   // TEST ONLY (UCBD)
}

// Plans a runway take-off in a direction: rolls 3 tiles low, climbs to 3/4 cruising height 7 tiles out, then
// flies to the goal (ee_units cls_air.rs)
void EEUnitClass::TakeOffFromRunway(EEComplexUnit* inUnit, long inDX, long inDY)
{
    EEUCNote theNote(16); theNote.C(this).S(inUnit).I(inDX).I(inDY);   // TEST ONLY (UCBD)
    ee_units_cls_take_off_from_runway(EEObj(this), EEObj(inUnit), (int32_t)inDX, (int32_t)inDY);
    theNote.S(inUnit);   // TEST ONLY (UCBD)
}

// Returns whether the move goal is more than 1 from the home carrier's deck point (tag point 0): the carrier moved
bool EEUnitClass::IsAwayFromGoal(EEComplexUnit* inUnit)
{
    EEUCNote theNote(18); theNote.C(this).S(inUnit).U(inUnit->mHomeBase);   // TEST ONLY (UCBD)
    return theNote.R([&]() -> bool {
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
    }());
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

// 0x249560 (cdecl): the point to fly to for the attack on inTarget (EEMap.cpp's range) (provisional name)
void EEGetAimPoint(EEComplexUnit* inUnit, GE3DPoint* inTarget, GE3DPoint* outLocation);

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

// Returns a sound's volume, pan and pitch, with a doppler shift
void EEUnitClass::SpaceshipGetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
                              float* outPitch)
{
    this->BaseGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    EEGetDopplerPitch(inUnit, inSound, outVolume, outPan, outPitch);
}

