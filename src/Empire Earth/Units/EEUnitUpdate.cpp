// New (port; docs/RUST_PLAN.md R5b.4 leaf 6, the twelfth and thirteenth rounds): the host of the unit classes' Update
// sequences and ActionChanged decisions (ee/EEUnitUpdate.h; rust/crates/ee_units/src/update.rs, actions.rs; the
// fourteenth round's overrides with their classes' operations). Rust
// decides which of a unit's leaves run and in which order; each read and operation here is one expression or call of
// the original bodies, through the class (its virtuals dispatch as before) and the unit. TEST ONLY: every call is noted
// (USST kinds 12 and 13) while the capture runs.
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/USingleton.h"
#include "lle/SESoundManager.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEGameVariant.h"
#include "ee/EECalamityManager.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIArea.h"
#include "ee/EEHerd.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGHealUnit.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEPathManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEEffectManager.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEE2DBattlefieldAmbient.h"
#include "ee/EEUCLand.h"
#include "ee/EEUCNaval.h"
#include "ee/EEUCBasicCitizen.h"
#include "ee/EEUCBasicPriest.h"
#include "ee/EEUCLandable.h"
#include "ee/EEUCAirplane.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCBuildingPriest.h"
#include "ee/EEUCAirAnimal.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EEUGGather.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUnitKind.h"
#include "ee/EEUnitStore.h"
#include "ee/EEUnitUpdate.h"
#include "rust/ee_units.h"

namespace {

// EEUnitAI.cpp's: no initial contact heading
const float kUnitAINoHeading = -10000.0f;

// the host's context: the class, the unit, an ActionChanged's healee found (EE_UNITS_ACT_FIND_HEALEE), the nearest
// airport found (EE_UNITS_ACT_FIND_NEAREST_AIRPORT)
struct EEUpdateContext {
    EEUnitClass* mClass;
    EEComplexUnit* mUnit;
    EEUnit* mHealee;
    EEComplexUnit* mAirport;
};

// the initial contact location and heading from the unit's location (EEUnitClass::ActionChanged's two copies)
void EEUpdateContact(EEComplexUnit* inUnit, bool inThroughSetter)
{
    GEVector theUp(0.0f, 1.0f, 0.0f);
    GEVector theDirection(0.0f, 0.0f, 0.0f);
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    GE3DPoint thePoint(theLocation.GetX(), theLocation.GetY(), 0.0f);
    theLocation.Apply(theUp, theDirection);
    float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
    if (inThroughSetter)
        inUnit->SetInitialContactLocation(thePoint);
    else if (inUnit->mInitialContactLocation != thePoint)
        inUnit->mInitialContactLocation = thePoint;
    if (inUnit->mInitialContactHeading != theYaw)
        inUnit->mInitialContactHeading = theYaw;
}

int64_t EEUpdateBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, sizeof(theBits));
    return (int64_t)theBits;
}

float EEUpdateFloat(int64_t inBits)
{
    uint32_t theBits = (uint32_t)inBits;
    float theValue;
    memcpy(&theValue, &theBits, sizeof(theValue));
    return theValue;
}

int64_t EEUpdateRead(void* inContext, int32_t inCode)
{
    EEUpdateContext* theContext = static_cast<EEUpdateContext*>(inContext);
    EEComplexUnit* theUnit = theContext->mUnit;
    EEUnitClass* theClass = theContext->mClass;
    switch (inCode) {
    case EE_UNITS_UPD_R_FRAME:
        return theUnit->mPhysicalModel.GetFrame();
    case EE_UNITS_UPD_R_ANIMAL_ON_MAP:
        return EEHerd::IsAnimalOnMap(theUnit) ? 1 : 0;
    case EE_UNITS_UPD_R_PHYSICS_MODEL:
        return theClass->mPhysicsModel != NULL ? 1 : 0;
    case EE_UNITS_UPD_R_MEDIC:
        return theClass->mIsMedic ? 1 : 0;
    case EE_UNITS_UPD_R_REGENERATES:
        return theUnit->mStats != NULL && theUnit->mStats->mRegeneration > 0 ? 1 : 0;
    // the store's fields, for a unit with no record
    case EE_UNITS_UPD_R_HERD:
        return theUnit->mHerd != NULL ? 1 : 0;
    case EE_UNITS_UPD_R_OWNER:
        return (int64_t)theUnit->mPlayer->mPlayerID;
    case EE_UNITS_UPD_R_TILE_X:
        return (int64_t)theUnit->mTileX.Get();
    case EE_UNITS_UPD_R_COMMAND:
        return theUnit->mCommand ? (int64_t)theUnit->mCommand->mType : 0;
    case EE_UNITS_UPD_R_GOAL_KIND:
        return theUnit->mGoalState != NULL ? (int64_t)theUnit->mGoalState->mKind : -1;
    case EE_UNITS_UPD_R_UNIT_TYPE:
        return (int64_t)theUnit->mUnitClass->mUnitType;
    case EE_UNITS_UPD_R_HIT_POINTS:
        return (int64_t)theUnit->mHitPoints.Get();
    case EE_UNITS_UPD_R_FAMILY:
        return (int64_t)theUnit->mUnitClass->mFamily;
    case EE_UNITS_UPD_R_BUILT:
        return theUnit->mIsBuilt ? 1 : 0;
    case EE_UNITS_UPD_R_FLIGHT_TIME:
        return (int64_t)theUnit->mFlightTime.Get();
    // the others
    case EE_UNITS_UPD_R_CONVERSION_START:
        return (int64_t)theUnit->mConversionStartTime;
    case EE_UNITS_UPD_R_CINEMATIC:
        return ::gWorld.mInCinematic ? 1 : 0;
    case EE_UNITS_UPD_R_TICK_MS:
        return (int64_t)gPhysics->mTickTimeMs;
    case EE_UNITS_UPD_R_PHYSICS_ORDER:
        return (int64_t)theUnit->mCurrentPhysicsOrder;
    case EE_UNITS_UPD_R_PHYSICS_ACTION:
        return (int64_t)theUnit->mCurrentPhysicsAction;
    case EE_UNITS_UPD_R_POSITION_Z: {
        GE3DPoint thePosition;
        theUnit->GetPosition(thePosition);
        return EEUpdateBits(thePosition.GetZ());
    }
    case EE_UNITS_UPD_R_CALAMITY_CAMOUFLAGE:
        return USingleton<EECalamityManager>::GetInstance()->IsCalamityActive(0x17) ? 1 : 0;
    case EE_UNITS_UPD_R_CLASS_UNARMED:
        return theClass->mAttack == 0.0f ? 1 : 0;
    case EE_UNITS_UPD_R_IS_PRIEST:
        return theClass->IsPriest() ? 1 : 0;
    case EE_UNITS_UPD_R_STATS_POWER:
        return (int64_t)theUnit->mStats->mPower;
    case EE_UNITS_UPD_R_GAME_TIME:
        return (int64_t)gPhysics->mGameTime;
    case EE_UNITS_UPD_R_GRAZE_TIME:
        return (int64_t)theUnit->mGrazeTime;
    case EE_UNITS_UPD_R_GATHER_TIME_2:
        return (int64_t)theUnit->mStats->mGatherTimes[2];
    case EE_UNITS_UPD_R_GATHER_TIME_6:
        return (int64_t)theUnit->mStats->mGatherTimes[6];
    case EE_UNITS_UPD_R_WORK_TIME_FACTOR:
        return EEUpdateBits(::gWorld.GetGameVariant()->mWorkTimeFactor);
    case EE_UNITS_UPD_R_WORK_TIMER:
        return (int64_t)theUnit->mWorkTimer;
    case EE_UNITS_UPD_R_MINE:
        return theUnit->mMine != NULL ? 1 : 0;
    case EE_UNITS_UPD_R_MINE_RESOURCE_TYPE:
        return (int64_t)theUnit->mMine->mResourceType;
    case EE_UNITS_UPD_R_FIRST_PART:
        return theUnit->mPreviousPart == NULL ? 1 : 0;
    case EE_UNITS_UPD_R_HAS_NEXT_PART:
        return theUnit->mNextPart != NULL ? 1 : 0;
    case EE_UNITS_UPD_R_OPEN_FRACTION:
        return EEUpdateBits(theUnit->mOpenFraction);
    // ActionChanged's
    case EE_UNITS_ACT_R_ANIMATION:
        return (int64_t)theUnit->GetAnimation();
    case EE_UNITS_ACT_R_ATTACK_FRAME_SET:
        return (int64_t)(unsigned long)EEGetAttackFrameSet(theUnit);
    case EE_UNITS_ACT_R_SWIMMING:
        return theUnit->IsSwimming() ? 1 : 0;
    case EE_UNITS_ACT_R_LAND_BUILDING:
        return theClass->IsLandBuilding() ? 1 : 0;
    case EE_UNITS_ACT_R_CLASS_ARMED:
        return theClass->mAttack > 0 ? 1 : 0;
    case EE_UNITS_ACT_R_FIRE_OVER_WALLS:
        return theUnit->mStats->mCanFireOverWalls ? 1 : 0;
    case EE_UNITS_ACT_R_RANDOM:
        return EEUpdateBits(gPhysics->GetRandom());
    case EE_UNITS_ACT_R_IS_WALL:
        return theUnit->mUnitClass->mIsWall ? 1 : 0;
    case EE_UNITS_ACT_R_ROTTING_TIME:
        return (int64_t)theClass->mRottingTime;
    case EE_UNITS_ACT_R_PATH_ATTEMPTS_EXCEEDED: {
        EEUnitGoal* theGoal = theUnit->mGoalState;
        return (unsigned long)theGoal->mPathAttempts > (unsigned long)theGoal->GetMaxPathAttempts() ? 1 : 0;
    }
    case EE_UNITS_ACT_R_CLASS_RESOURCE_AMOUNT:
        return (int64_t)theUnit->mUnitClass->mResourceAmount;
    case EE_UNITS_ACT_R_NO_HEADING:
        return theUnit->mInitialContactHeading == kUnitAINoHeading ? 1 : 0;
    case EE_UNITS_ACT_R_WORLD_CLEARING:
        return gWorldClearing ? 1 : 0;
    case EE_UNITS_ACT_R_QUEUED_GOALS:
        return theUnit->GetQueueGoals(0).size() > 1 ? 1 : 0;
    case EE_UNITS_ACT_R_DEATH_EFFECT:
        return theClass->mDeathEffect != NULL ? 1 : 0;
    case EE_UNITS_ACT_R_DEATH_SOUND:
        return theUnit->mStats->mDeathSound != NULL ? 1 : 0;
    case EE_UNITS_ACT_R_BLOOD_STAIN:
        return theUnit->mUnitClass->mBloodStainEffect != NULL ? 1 : 0;
    case EE_UNITS_ACT_R_DOMAINS:
        return (int64_t)theUnit->mUnitClass->mDomains;
    case EE_UNITS_ACT_R_CATEGORY:
        return (int64_t)theUnit->mUnitClass->mCategory;
    case EE_UNITS_ACT_R_CONTAINER:
        return theUnit->mContainer != NULL ? 1 : 0;
    case EE_UNITS_ACT_R_RESOURCE_AMOUNT:
        return (int64_t)theUnit->mResourceAmount.Get();
    // the fourteenth round's
    case EE_UNITS_ACT_R_IS_HUMAN:
        return theUnit->mPlayer->IsHuman() ? 1 : 0;
    case EE_UNITS_ACT_R_BUILD_QUEUE_EMPTY:
        return theUnit->mCitizenBuildQueue.size() == 0 ? 1 : 0;
    case EE_UNITS_ACT_R_AIRCRAFT_CARRIER:
        return theUnit->mUnitClass->mAIKind == kAIKindAircraftCarrier ? 1 : 0;
    case EE_UNITS_ACT_R_EXITING_UNIT:
        return (int64_t)theUnit->mExitingUnitID;
    case EE_UNITS_ACT_R_AIR_TRAFFIC_TYPE:
        return (int64_t)theUnit->mUnitClass->mAirTrafficType;
    case EE_UNITS_ACT_R_WAYPOINTS:
        return (int64_t)(long)theUnit->mWaypoints.size();
    case EE_UNITS_ACT_R_HOME_BASE:
        return theUnit->mHomeBase != NULL ? 1 : 0;
    case EE_UNITS_ACT_R_CLEARANCE_GRANTED:
        return theUnit->mIsClearanceGranted == true ? 1 : 0;
    case EE_UNITS_ACT_R_BOMBER:
        return theClass->mProjectileClass != NULL && theClass->mProjectileClass->mCategory == kUnitCategoryBomb ? 1 : 0;
    case EE_UNITS_ACT_R_COMMANDABLE:
        return theUnit->mIsCommandable ? 1 : 0;
    default:
        return 0;
    }
}

int64_t EEUpdateAct(void* inContext, int32_t inCode, int64_t inA, int64_t inB)
{
    EEUpdateContext* theContext = static_cast<EEUpdateContext*>(inContext);
    EEComplexUnit* theUnit = theContext->mUnit;
    EEUnitClass* theClass = theContext->mClass;
    if (inCode >= EE_UNITS_ACT_RELEASE_HEAL_EFFECT)
        EEUnitActionNote(inCode, inA, inB);   // TEST ONLY (USST kind 13)
    else
        EEUnitUpdateNote(inCode, inA, inB);   // TEST ONLY (USST kind 12)
    switch (inCode) {
    case EE_UNITS_UPD_LAST_HIT_POINTS:
        theUnit->SetLastHitPoints(theUnit->mHitPoints);
        break;
    case EE_UNITS_UPD_PHYSICS_STEP:
        theClass->mPhysicsModel->Step(theUnit);
        break;
    case EE_UNITS_UPD_TURRETS:
        theUnit->UpdateTurrets();
        break;
    case EE_UNITS_UPD_WHEELS:
        theUnit->UpdateWheels();
        break;
    case EE_UNITS_UPD_ORBITALS:
        theUnit->UpdateOrbitals();
        break;
    case EE_UNITS_UPD_EFFECTS:
        theClass->UpdateEffects(theUnit);
        break;
    case EE_UNITS_UPD_RELOAD_TIMER:
        theUnit->UpdateReloadTimer();
        break;
    case EE_UNITS_UPD_TARGET_SEARCH_TIMER:
        theUnit->UpdateTargetSearchTimer();
        break;
    case EE_UNITS_UPD_CAMOUFLAGE_TIMER:
        theClass->UpdateCamouflageTimer(theUnit);
        break;
    case EE_UNITS_UPD_ANIMATE:
        return theClass->Animate(theUnit);
    case EE_UNITS_UPD_REFRESH_SOUND:
        theClass->RefreshSound(theUnit);
        break;
    case EE_UNITS_UPD_REGENERATE_POWER:
        theClass->RegeneratePower(theUnit);
        break;
    case EE_UNITS_UPD_FANATICISM_COLOR:
        theUnit->UpdateFanaticismColor();
        break;
    case EE_UNITS_UPD_FIRE_ON_ATTACK_FRAME:
        theClass->FireOnAttackFrame(theUnit, (long)inA, (long)inB);
        break;
    case EE_UNITS_UPD_ROTTING:
        return (int64_t)theUnit->UpdateRottingAwayTime();
    case EE_UNITS_UPD_HEAL_SEARCH_TIMER:
        theUnit->UpdateHealSearchTimer();
        break;
    case EE_UNITS_UPD_GOAL_WORK:
        theUnit->mGoalState->Work(theUnit);
        break;
    case EE_UNITS_UPD_REGENERATE:
        theClass->Regenerate(theUnit);
        break;
    case EE_UNITS_UPD_DONE:
        theClass->UpdateDone(theUnit);
        break;
    case EE_UNITS_UPD_ANIMAL:
        theClass->UpdateAnimal(theUnit);
        break;
    case EE_UNITS_UPD_RESOURCE_UNIT:
        static_cast<EEUCNaval*>(theClass)->UpdateResourceUnit(theUnit);
        break;
    case EE_UNITS_UPD_CONTINUE_PRODUCTION:
        theUnit->ContinueProduction();
        break;
    case EE_UNITS_UPD_UPDATE_CITIZEN:
        static_cast<EEUCBasicCitizen*>(theClass)->UpdateCitizen(theUnit);
        break;
    case EE_UNITS_UPD_UPDATE_CONVERSION:
        static_cast<EEUCBasicPriest*>(theClass)->UpdateConversion(theUnit);
        break;
    case EE_UNITS_UPD_CLEAR_CONVERSION_START:
        theUnit->mConversionStartTime = 0;
        break;
    case EE_UNITS_UPD_RELEASE_CONVERSION_EFFECT:
        theUnit->ReleaseConversionEffect();
        break;
    case EE_UNITS_UPD_HIDE_CONVERSION_EFFECTS:
        static_cast<EEUCBasicPriest*>(theClass)->HideConversionEffects(theUnit);
        break;
    case EE_UNITS_UPD_RESOURCES_ROTTING:
        theUnit->UpdateResourcesRotting();
        break;
    case EE_UNITS_UPD_SET_FLIGHT_TIME:
        theUnit->SetFlightTime((long)inA);
        break;
    case EE_UNITS_UPD_NEXT_FLIGHT_STATE:
        return (int64_t)static_cast<EEUCLandable*>(theClass)->GetNextFlightState(theUnit);
    case EE_UNITS_UPD_SET_PHYSICS_ORDER:
        theUnit->SetCurrentPhysicsOrder((unsigned long)inA);
        break;
    case EE_UNITS_UPD_GOAL_QUEUES: {
        unsigned long theCount = theUnit->GetNumGoalQueues();
        for (unsigned long i = 0; i < theCount; i++) {
            if (!theUnit->mGoalQueues[i].mStarted || theUnit->mGoalQueues[i].mGoals.size() <= 0)
                continue;
            theUnit->mGoalQueues[i].Update();
            theUnit->RefreshGoals();
            theUnit->RefreshActiveGoal();
        }
        break;
    }
    case EE_UNITS_UPD_OPACITY_CHAIN:
        theUnit->SetOpacityChain(EEUpdateFloat(inA));
        break;
    case EE_UNITS_UPD_UPDATE_BUILDING:
        theClass->UpdateBuilding(theUnit);
        break;
    case EE_UNITS_UPD_UPDATE_ARMED:
        static_cast<EEUCBuilding*>(theClass)->UpdateArmed(theUnit);
        break;
    case EE_UNITS_UPD_ADD_RESOURCE:
        theUnit->mPlayer->AddResource((long)inA, (long)inB);
        break;
    case EE_UNITS_UPD_SAVED_GROUPS_RESET:
        theUnit->mPlayer->mSavedGroups.Reset(7, 1);
        break;
    case EE_UNITS_UPD_SET_GRAZE_TIME:
        theUnit->SetGrazeTime((unsigned long)(uint32_t)inA);
        break;
    case EE_UNITS_UPD_SET_OPEN_FRACTION: {
        float theOpen = EEUpdateFloat(inA);
        theUnit->SetOpenFraction(theOpen);
        theClass->GetLastLinkedCarrier(theUnit)->SetOpenFraction(theOpen);
        theClass->GetLastLinked(theUnit)->SetOpenFraction(theOpen);
        break;
    }
    case EE_UNITS_UPD_SET_WORK_TIMER:
        theUnit->SetWorkTimer((long)inA);
        break;
    case EE_UNITS_UPD_DEPLETE_MINE:
        theUnit->mMine->DepleteResource(1);
        break;
    case EE_UNITS_UPD_SET_PARTS_ROTTING:
        for (EEComplexUnit* thePart = theUnit->mNextPart; thePart != NULL; thePart = thePart->mNextPart)
            thePart->SetRottingAwayTime((long)inA);
        break;
    // ActionChanged's
    case EE_UNITS_ACT_RELEASE_HEAL_EFFECT:
        theUnit->ReleaseHealEffect();
        break;
    case EE_UNITS_ACT_HERD_ATTACK_BACK:
        theUnit->mHerd->AttackBack(theUnit, true);
        break;
    case EE_UNITS_ACT_HERD_UNIT_HIT:
        theUnit->mHerd->HandleUnitHit(theUnit);
        break;
    case EE_UNITS_ACT_CONTACT_CHECK_RANGE:
        EEUpdateContact(theUnit, false);
        break;
    case EE_UNITS_ACT_CONTACT_FOLLOW:
        EEUpdateContact(theUnit, true);
        break;
    case EE_UNITS_ACT_ATTACKED:
        theClass->Attacked(theUnit);
        break;
    case EE_UNITS_ACT_FLEE_ATTACKER:
        theClass->FleeAttacker(theUnit);
        break;
    case EE_UNITS_ACT_SET_PHYSICS_ACTION:
        theUnit->SetCurrentPhysicsAction((unsigned long)inA);
        break;
    case EE_UNITS_ACT_SET_ANIMATION:
        theUnit->SetAnimation((long)inA, -1, inB != 0);
        break;
    case EE_UNITS_ACT_RETURN_TO_CONTACT: {
        EEUnitGoal* theLocation = new EEUGLocation(&theUnit->mInitialContactLocation, false, false, true,
                                                   theUnit->mInitialContactHeading, false, false);
        theUnit->SetGoal(theLocation, inA != 0, 0);
        break;
    }
    case EE_UNITS_ACT_SET_TARGET_SEARCH_TIMER:
        theUnit->SetTargetSearchTimer((long)inA);
        break;
    case EE_UNITS_ACT_FLEE_AFTER_CALAMITY:
        theUnit->FleeAfterCalamity(0);
        break;
    case EE_UNITS_ACT_SET_NEXT_TRIGGER_TIME:
        theUnit->mNextTriggerTime = (unsigned long)(long)inA;
        break;
    case EE_UNITS_ACT_PAUSE_ANIMATION:
        theUnit->SetIsAnimationPaused(true);
        break;
    case EE_UNITS_ACT_RANDOM_ANIMATION_TIME:
        theUnit->SetAnimationTime(gPhysics->GetRandom(0L, (long)theUnit->mStats->mModel->mHoldTimes[theUnit->GetAnimation()]));
        break;
    case EE_UNITS_ACT_DROP_PATH:
        theUnit->DropPath();
        break;
    case EE_UNITS_ACT_UPDATE_WATER_STATE:
        theUnit->UpdateWaterState(false);
        break;
    case EE_UNITS_ACT_UNIT_GOAL_DONE:
        theUnit->mPlayer->UnitGoalDone(theUnit);
        break;
    case EE_UNITS_ACT_CLEAR_GOALS:
        theUnit->ClearGoals((long)inA);
        break;
    case EE_UNITS_ACT_CLEAR_HEADING:
        theUnit->mInitialContactHeading = kUnitAINoHeading;
        break;
    case EE_UNITS_ACT_CANCEL_PATHS:
        gPathManager.CancelPaths(theUnit, true);
        break;
    case EE_UNITS_ACT_SET_ROTTING:
        theUnit->SetRottingAwayTime((long)inA);
        break;
    case EE_UNITS_ACT_NEXT_PART_ACTION:
        theUnit->mNextPart->StartAction((unsigned long)inA);
        theUnit->mNextPart->mUnitClass->ActionChanged(theUnit->mNextPart);
        break;
    case EE_UNITS_ACT_UNIT_DIED:
        theClass->UnitDied(theUnit);
        break;
    case EE_UNITS_ACT_GOAL_STEP:
        theUnit->mGoalState->Step(theUnit);
        break;
    case EE_UNITS_ACT_DESELECT:
        theUnit->mPlayer->DeselectUnit(theUnit);
        break;
    case EE_UNITS_ACT_CLOAK_CHAIN:
        theUnit->SetCloakedChain(true);
        break;
    case EE_UNITS_ACT_OPAQUE_CHAIN:
        theUnit->SetOpacityChain(1.0f);
        break;
    case EE_UNITS_ACT_KILL:
        theUnit->Kill(inA != 0, false, false);
        break;
    case EE_UNITS_ACT_KILL_UNIT_AND_PARTS:
        theUnit->mPlayer->KillUnit(theUnit, false);
        for (EEComplexUnit* thePart = theUnit->mNextPart; thePart != NULL; thePart = thePart->mNextPart)
            theUnit->mPlayer->KillUnit(thePart, false);
        break;
    case EE_UNITS_ACT_START_IDLE:
        theUnit->StartAction(kActionIdle);
        theUnit->mUnitClass->ActionChanged(theUnit);
        break;
    case EE_UNITS_ACT_PATH_TO_GOAL:
        theClass->PathToGoal(theUnit);
        break;
    case EE_UNITS_ACT_MOVE_TO_TILE_CENTRE:
        theClass->MoveToTileCentre(theUnit);
        break;
    case EE_UNITS_ACT_FLEE:
        theClass->Flee(theUnit);
        break;
    case EE_UNITS_ACT_NEXT_WAYPOINT:
        theClass->NextWaypoint(theUnit);
        break;
    case EE_UNITS_ACT_FIND_SAFE_LOCATION:
        theClass->FindSafeLocation(theUnit);
        break;
    case EE_UNITS_ACT_CLEAR_ARRIVED:
        theUnit->SetHasArrived(false);
        break;
    case EE_UNITS_ACT_FACE_YAW:
        return static_cast<EEUGLocation*>(theUnit->mGoalState)->FaceYaw(theUnit) ? 1 : 0;
    case EE_UNITS_ACT_FIND_PATH:
        theClass->FindPath(theUnit);
        break;
    case EE_UNITS_ACT_HEAL:
        theClass->Heal(theUnit);
        break;
    case EE_UNITS_ACT_FIND_HEALEE: {
        EEUnit* theTarget = NULL;
        bool theFound = theClass->FindTargetNear(theUnit, theUnit->mStats->GetLineOfSight(), &theTarget);
        theContext->mHealee = theTarget;
        return theFound ? 1 : 0;
    }
    case EE_UNITS_ACT_SET_HEAL_GOAL: {
        EEUnitGoal* theHeal = new EEUGHealUnit(static_cast<EEComplexUnit*>(theContext->mHealee), false);
        theUnit->SetGoal(theHeal, inA != 0, 0);
        break;
    }
    case EE_UNITS_ACT_RESET_HEAL_SEARCH:
        theUnit->SetHealSearchTimer(theClass->mHealDelay);
        break;
    case EE_UNITS_ACT_GOAL_START_TIME:
        theUnit->mGoalState->SetStartTime(gPhysics->mGameTime);
        break;
    case EE_UNITS_ACT_CALL_PARATROOP_PLANE:
        static_cast<EEUCLand*>(theClass)->CallParatroopPlane(theUnit);
        break;
    case EE_UNITS_ACT_PLACE_TRACK_DECAL:
        EEPlaceTrackDecal(theUnit);
        break;
    case EE_UNITS_ACT_DEATH_EFFECT:
        gEffectManager->CreateUnitEffect(theUnit, theClass->mDeathEffect, NULL, 0, 0, 0);
        break;
    case EE_UNITS_ACT_DEATH_SOUND: {
        EESoundObject* theSound = theUnit->mStats->mDeathSound;
        float theVolume;
        float thePan;
        float thePitch;
        theClass->GetSoundParameters(theUnit, theSound, &theVolume, &thePan, &thePitch);
        gSoundManager->Play(theSound->mSampleID, theVolume, thePan, thePitch, theSound->mLooping, NULL);
        break;
    }
    case EE_UNITS_ACT_STOP:
        theUnit->mAngularVelocity = GEOrientation(0.0f, 0.0f, 0.0f);
        theUnit->SetVelocity(0.0f);
        break;
    case EE_UNITS_ACT_RELEASE_CONVERSION_EFFECT:
        theUnit->ReleaseConversionEffect();
        break;
    case EE_UNITS_ACT_HIDE_CONVERSION_EFFECTS:
        // (the temple is no basic priest: its own copy)
        if (EEUnitClassCast<EEUCBuildingPriest>(theClass) != NULL)
            static_cast<EEUCBuildingPriest*>(theClass)->HideConversionEffects(theUnit);
        else
            static_cast<EEUCBasicPriest*>(theClass)->HideConversionEffects(theUnit);
        break;
    case EE_UNITS_ACT_SET_CONVERSION_START:
        theUnit->mConversionStartTime = gPhysics->mGameTime;
        break;
    case EE_UNITS_ACT_SET_WORK_TIMER:
        theUnit->SetWorkTimer((long)inA);
        break;
    // the fourteenth round's
    case EE_UNITS_ACT_FIND_STORAGE_GOAL: {
        EEUnit* theStorage = static_cast<EEUCBasicCitizen*>(theClass)->FindStorage(theUnit, inA != 0);
        if (theStorage != NULL)
            theUnit->SetGoal(new EEUGGather(NULL, theStorage, false), 0, 0);
        break;
    }
    case EE_UNITS_ACT_SET_RESOURCE_AMOUNT:
        theUnit->SetResourceAmount((long)inA);
        break;
    case EE_UNITS_ACT_SET_PRODUCTION_TIME:
        theUnit->SetProductionTime((long)inA);
        break;
    case EE_UNITS_ACT_CLEAR_SHIFT_QUEUED:
        theUnit->mHasShiftQueuedBuild = false;
        break;
    case EE_UNITS_ACT_RETARGET_NEAREST_BUILDING: {
        EEUGAttackUnit* theGoal = EEGoalCast<EEUGAttackUnit>(theUnit->GetGoal());
        if (theGoal != NULL && theGoal->mTarget != NULL) {
            EEUnit* theTarget = static_cast<EEUCBuilding*>(theClass)->FindNearestTargetBuilding(theGoal->mTarget);
            if (theTarget != NULL)
                theGoal->SetTarget(theTarget);
        }
        break;
    }
    case EE_UNITS_ACT_SET_EXITING_UNIT:
        theUnit->SetExitingUnitID((long)inA);
        break;
    case EE_UNITS_ACT_QUEUE_TAKEOFF: {
        EEComplexUnit* theCargo = theUnit->FindPassenger(theUnit->mExitingUnitID);
        if (theCargo != NULL)
            theCargo->mPlayer->mAirTraffic[theCargo->mUnitClass->mAirTrafficType - 1]->QueueRequest(theUnit, theCargo, 0);
        break;
    }
    case EE_UNITS_ACT_RELEASE_UNIT:
        static_cast<EEUCBuilding*>(theClass)->ReleaseUnit(theUnit);
        break;
    case EE_UNITS_ACT_GATE_SOUND: {
        EESoundObject* theSound = EESoundObject::Get(kSoundGate);
        if (theSound != NULL) {
            float theVolume;
            float thePan;
            float thePitch;
            theClass->GetSoundParameters(theUnit, theSound, &theVolume, &thePan, &thePitch);
            theSound->Play(theVolume, thePan, thePitch);
        }
        break;
    }
    case EE_UNITS_ACT_GATE_PARTS_ACTION:
        theUnit->mNextPart->StartAction((unsigned long)inA);
        theUnit->mNextPart->mNextPart->StartAction((unsigned long)inA);
        break;
    case EE_UNITS_ACT_SET_PHYSICS_ORDER:
        theUnit->SetCurrentPhysicsOrder((unsigned long)inA);
        break;
    case EE_UNITS_ACT_CLEAR_F220:
        theUnit->SetF220(0.0f);
        break;
    case EE_UNITS_ACT_HEIGHT_HERE: {
        GE3DPoint thePosition(theUnit->mPhysicalModel.GetLocation().GetX(), theUnit->mPhysicalModel.GetLocation().GetY(),
                              theUnit->mPhysicalModel.GetLocation().GetZ());
        theUnit->SetHeight(theClass->GetHeightAt(theUnit, thePosition));
        break;
    }
    case EE_UNITS_ACT_HEIGHT_AT_NEXT_WAYPOINT: {
        GE3DPoint thePosition(theUnit->mPhysicalModel.GetLocation().GetX(), theUnit->mPhysicalModel.GetLocation().GetY(),
                              theUnit->mPhysicalModel.GetLocation().GetZ());
        if (theUnit->GetNextWaypointPoint(thePosition) == 0)
            theUnit->SetHeight(theClass->GetHeightAt(theUnit, thePosition));
        break;
    }
    case EE_UNITS_ACT_UNIT_NEXT_WAYPOINT:
        theUnit->NextWaypoint();
        break;
    case EE_UNITS_ACT_RETURN_TO_AIRPORT:
        static_cast<EEUCAirAnimal*>(theClass)->ReturnToAirport(theUnit);
        break;
    case EE_UNITS_ACT_SET_FLIGHT_TIME_STATS:
        theUnit->SetFlightTime(theUnit->mStats->mFlightTime);
        break;
    case EE_UNITS_ACT_SET_AMMUNITION:
        theUnit->SetAmmunition(theClass->mAmmunition);
        break;
    case EE_UNITS_ACT_DEATH_ROLL:
        if (inA == 0)
            EEAirAnimalDeathRoll(theClass, theUnit);
        else
            EELandableActionOperation(inCode, theClass, theUnit, inA, &theContext->mAirport);
        break;
    case EE_UNITS_ACT_HELICOPTER_PATH:
        if (inA == 0)
            EEHelicopterPathToGoal(theClass, theUnit);
        else if (inA == 1)
            EEHelicopterTransportPathToGoal(theClass, theUnit);
        else
            EEHelicopterPriestPathToGoal(theClass, theUnit);
        break;
    case EE_UNITS_ACT_AIR_ANIMAL_PATH:
        EEAirAnimalPathToGoal(theClass, theUnit);
        break;
    case EE_UNITS_ACT_CIRCLE_SIDE:
        EEAirplaneCircleSide(theClass, theUnit);
        break;
    case EE_UNITS_ACT_HOLDING_PATTERN:
        if (inA == 0)
            EEAirplaneHoldingPattern(theClass, theUnit);
        else
            EEPlanetSpaceshipHoldingPattern(theClass, theUnit);
        break;
    case EE_UNITS_ACT_FLY_OFF_MAP:
        static_cast<EEUCAirplane*>(theClass)->FlyOffMap(theUnit);
        break;
    case EE_UNITS_ACT_START_ACTION:
        theUnit->StartAction((unsigned long)inA);
        break;
    case EE_UNITS_ACT_CLASS_ACTION_CHANGED:
        theClass->ActionChanged(theUnit);
        break;
    case EE_UNITS_ACT_SET_COMMANDABLE:
        theUnit->SetIsCommandable(inA != 0);
        break;
    case EE_UNITS_ACT_MAKE_COMMANDABLE:
        theUnit->mIsCommandable = true;
        break;
    case EE_UNITS_ACT_APPROACH_CARRIER:
    case EE_UNITS_ACT_BOARD_CARRIER:
    case EE_UNITS_ACT_TAKE_OFF_FROM_CARRIER:
    case EE_UNITS_ACT_FLY_CHECK_PATH:
    case EE_UNITS_ACT_FIND_NEAREST_AIRPORT:
    case EE_UNITS_ACT_SET_HOME_BASE_FOUND:
    case EE_UNITS_ACT_APPROACH_LANDING:
    case EE_UNITS_ACT_SECONDARY_TARGET:
    case EE_UNITS_ACT_TAKEOFF:
    case EE_UNITS_ACT_QUEUE_LANDING:
    case EE_UNITS_ACT_BOMBER_GROUND_GOAL:
    case EE_UNITS_ACT_INITIAL_ATTACK_HEIGHT:
    case EE_UNITS_ACT_CONTACT_HERE:
    case EE_UNITS_ACT_LANDABLE_PATH:
        return EELandableActionOperation(inCode, theClass, theUnit, inA, &theContext->mAirport);
    default:
        break;
    }
    return 0;
}

}  // namespace

void EEUnitClassUpdate(int inKind, EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    EEUpdateContext theContext = {inClass, inUnit, NULL, NULL};
    EeUnitsUpdateHost theHost = {&theContext, EEUpdateRead, EEUpdateAct};
    ee_units_update(inKind, static_cast<uint32_t>(inUnit->mUnitID), reinterpret_cast<uintptr_t>(inUnit), &theHost);
}

void EEUnitClassActionChanged(int inKind, EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    EEUnitActionScope theScope(inUnit->mUnitID);   // TEST ONLY (USST kind 13)
    EEUpdateContext theContext = {inClass, inUnit, NULL, NULL};
    EeUnitsUpdateHost theHost = {&theContext, EEUpdateRead, EEUpdateAct};
    ee_units_action_changed(inKind, static_cast<uint32_t>(inUnit->mUnitID), reinterpret_cast<uintptr_t>(inUnit),
                            &theHost);
}
