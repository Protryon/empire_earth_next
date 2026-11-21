// New (port; docs/RUST_PLAN.md R5b.4 leaf 6, the twelfth and thirteenth rounds): the host of the unit classes' Update
// sequences and ActionChanged decisions (ee/EEUnitUpdate.h; rust/crates/ee_sim/src/units/update.rs, actions.rs; the
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
#include "ee/EEUnitVectorItem.h"
#include "ee/EEPathManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEEffectManager.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEE2DBattlefieldAmbient.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitKind.h"
#include "ee/EEUnitStore.h"
#include "ee/EEUnitUpdate.h"
#include "rust/sim/units.h"

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

template <class T> T* EEObjectOf(int64_t inHandle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle));
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
    // (the plain member reads are Rust's: ee_units update_views.rs)
    case EE_UNITS_UPD_R_POSITION_Z: {
        GE3DPoint thePosition;
        theUnit->GetPosition(thePosition);
        return EEUpdateBits(thePosition.GetZ());
    }
    case EE_UNITS_UPD_R_CALAMITY_CAMOUFLAGE:
        return USingleton<EECalamityManager>::GetInstance()->IsCalamityActive(0x17) ? 1 : 0;
    case EE_UNITS_UPD_R_IS_PRIEST:
        return theClass->IsPriest() ? 1 : 0;
    case EE_UNITS_UPD_R_WORK_TIME_FACTOR:
        return EEUpdateBits(::gWorld.GetGameVariant()->mWorkTimeFactor);
    case EE_UNITS_UPD_R_MINE:
        return theUnit->mMine != NULL ? 1 : 0;
    case EE_UNITS_UPD_R_MINE_RESOURCE_TYPE:
        return (int64_t)theUnit->mMine->mResourceType;
    // ActionChanged's
    case EE_UNITS_ACT_R_ATTACK_FRAME_SET:
        return (int64_t)(unsigned long)EEGetAttackFrameSet(theUnit);
    case EE_UNITS_ACT_R_SWIMMING:
        return theUnit->IsSwimming() ? 1 : 0;
    case EE_UNITS_ACT_R_RANDOM:
        return EEUpdateBits(gPhysics->GetRandom());
    case EE_UNITS_ACT_R_PATH_ATTEMPTS_EXCEEDED: {
        EEUnitGoal* theGoal = theUnit->mGoalState;
        return (unsigned long)theGoal->mPathAttempts > (unsigned long)theGoal->GetMaxPathAttempts() ? 1 : 0;
    }
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
    case EE_UNITS_ACT_R_IS_HUMAN:
        return theUnit->mPlayer->IsHuman() ? 1 : 0;
    case EE_UNITS_ACT_R_BUILD_QUEUE_EMPTY:
        return theUnit->mCitizenBuildQueue.size() == 0 ? 1 : 0;
    case EE_UNITS_ACT_R_HOME_BASE:
        return theUnit->mHomeBase != NULL ? 1 : 0;
    case EE_UNITS_ACT_R_BOMBER:
        return theClass->mProjectileClass != NULL && theClass->mProjectileClass->mCategory == kUnitCategoryBomb ? 1 : 0;
    default:
        return 0;
    }
}

int64_t EEUpdateAct(void* inContext, int32_t inCode, int64_t inA, int64_t inB)
{
    EEUpdateContext* theContext = static_cast<EEUpdateContext*>(inContext);
    EEComplexUnit* theUnit = theContext->mUnit;
    EEUnitClass* theClass = theContext->mClass;
    // (the one-member stores and the death effect are Rust's: ee_units update_views.rs view_act; the notes (USST
    // kinds 12, 13) and the compound cases' choices too: update_views.rs, upd_split.rs; each statement a case here)
    switch (inCode) {
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
        theClass->UpdateResourceUnit(theUnit);
        break;
    case EE_UNITS_UPD_CONTINUE_PRODUCTION:
        theUnit->ContinueProduction();
        break;
    case EE_UNITS_UPD_UPDATE_CITIZEN:
        theClass->UpdateCitizen(theUnit);
        break;
    case EE_UNITS_UPD_UPDATE_CONVERSION:
        theClass->BasicPriestUpdateConversion(theUnit);
        break;
    case EE_UNITS_UPD_RELEASE_CONVERSION_EFFECT:
        theUnit->ReleaseConversionEffect();
        break;
    case EE_UNITS_UPD_HIDE_CONVERSION_EFFECTS:
        theClass->BasicPriestHideConversionEffects(theUnit);
        break;
    case EE_UNITS_UPD_RESOURCES_ROTTING:
        theUnit->UpdateResourcesRotting();
        break;
    case EE_UNITS_UPD_NEXT_FLIGHT_STATE:
        return (int64_t)theClass->GetNextFlightState(theUnit);
    case EE_UNITS_UPD_GOAL_QUEUES: {
        // the started, non-empty queues' expiry and the goals' refresh (ee_units goals.rs)
        EEGoalQueueIO theIO = {NULL, 0, 0, theUnit};
        EeGoalQueueHost theHost;
        EEGoalQueueHostOf(theIO, theHost);
        ee_units_goal_queues_update(reinterpret_cast<uintptr_t>(theUnit), &theHost);
        break;
    }
    case EE_UNITS_UPD_OPACITY_CHAIN:
        theUnit->SetOpacityChain(EEUpdateFloat(inA));
        break;
    case EE_UNITS_UPD_UPDATE_BUILDING:
        theClass->UpdateBuilding(theUnit);
        break;
    case EE_UNITS_UPD_UPDATE_ARMED:
        theClass->UpdateArmed(theUnit);
        break;
    case EE_UNITS_UPD_ADD_RESOURCE:
        theUnit->mPlayer->AddResource((long)inA, (long)inB);
        break;
    case EE_UNITS_UPD_SAVED_GROUPS_RESET:
        theUnit->mPlayer->mSavedGroups.Reset(7, 1);
        break;
    case EE_UNITS_UPD_X_OPEN_FRACTION: theUnit->SetOpenFraction(EEUpdateFloat(inA)); break;
    case EE_UNITS_UPD_X_CARRIER_OPEN_FRACTION: theClass->GetLastLinkedCarrier(theUnit)->SetOpenFraction(EEUpdateFloat(inA)); break;
    case EE_UNITS_UPD_X_LINKED_OPEN_FRACTION: theClass->GetLastLinked(theUnit)->SetOpenFraction(EEUpdateFloat(inA)); break;
    case EE_UNITS_UPD_DEPLETE_MINE:
        theUnit->mMine->DepleteResource(1);
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
    case EE_UNITS_ACT_SET_ANIMATION:
        theUnit->SetAnimation((long)inA, -1, inB != 0);
        break;
    case EE_UNITS_UPD_X_RETURN_TO_CONTACT: theUnit->SetGoal(EEUGRust::NewLocation(&theUnit->mInitialContactLocation, false, false, true, theUnit->mInitialContactHeading, false, false), inA != 0, 0); break;
    case EE_UNITS_ACT_FLEE_AFTER_CALAMITY:
        theUnit->FleeAfterCalamity(0);
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
    case EE_UNITS_UPD_X_UNIT: return EEObj(theUnit);
    case EE_UNITS_UPD_X_NEXT_PART_OF: return EEObj(EEObjectOf<EEComplexUnit>(inA)->mNextPart);
    case EE_UNITS_UPD_X_START_ACTION_OF: EEObjectOf<EEComplexUnit>(inA)->StartAction((unsigned long)inB); break;
    case EE_UNITS_UPD_X_ACTION_CHANGED_OF: EEObjectOf<EEComplexUnit>(inA)->mUnitClass->ActionChanged(EEObjectOf<EEComplexUnit>(inA)); break;
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
    case EE_UNITS_UPD_X_KILL_UNIT: theUnit->mPlayer->KillUnit(EEObjectOf<EEComplexUnit>(inA), false); break;
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
    case EE_UNITS_ACT_FACE_YAW:
        return static_cast<EEUGRust*>(theUnit->mGoalState)->FaceYaw(theUnit) ? 1 : 0;
    case EE_UNITS_ACT_FIND_PATH:
        theClass->FindPath(theUnit);
        break;
    case EE_UNITS_ACT_HEAL:
        theClass->Heal(theUnit);
        break;
    case EE_UNITS_UPD_X_CLEAR_HEALEE: theContext->mHealee = NULL; break;
    case EE_UNITS_UPD_X_FIND_HEALEE: return theClass->FindTargetNear(theUnit, theUnit->mStats->GetLineOfSight(), &theContext->mHealee);
    case EE_UNITS_UPD_X_SET_HEAL_GOAL: theUnit->SetGoal(EEUGRust::NewHealUnit(static_cast<EEComplexUnit*>(theContext->mHealee), false), inA != 0, 0); break;
    case EE_UNITS_ACT_GOAL_START_TIME:
        theUnit->mGoalState->SetStartTime(gPhysics->mGameTime);
        break;
    case EE_UNITS_ACT_CALL_PARATROOP_PLANE:
        theClass->CallParatroopPlane(theUnit);
        break;
    case EE_UNITS_ACT_PLACE_TRACK_DECAL:
        EEPlaceTrackDecal(theUnit);
        break;
    case EE_UNITS_UPD_X_DEATH_SOUND: return EEObj(theUnit->mStats->mDeathSound);
    case EE_UNITS_UPD_X_SOUND_PARAMETERS: theClass->GetSoundParameters(theUnit, EEObjectOf<EESoundObject>(inA), EEObjectOf<float>(inB), EEObjectOf<float>(inB) + 1, EEObjectOf<float>(inB) + 2); break;
    case EE_UNITS_UPD_X_PLAY_SAMPLE: gSoundManager->Play(EEObjectOf<EESoundObject>(inA)->mSampleID, EEObjectOf<float>(inB)[0], EEObjectOf<float>(inB)[1], EEObjectOf<float>(inB)[2], EEObjectOf<EESoundObject>(inA)->mLooping, NULL); break;
    case EE_UNITS_ACT_RELEASE_CONVERSION_EFFECT:
        theUnit->ReleaseConversionEffect();
        break;
    case EE_UNITS_ACT_HIDE_CONVERSION_EFFECTS:
        // (the temple's too: its copy was the basic priest's body)
        theClass->BasicPriestHideConversionEffects(theUnit);
        break;
    case EE_UNITS_UPD_X_FIND_STORAGE: return EEObj(theClass->FindStorage(theUnit, inA != 0));
    case EE_UNITS_UPD_X_GATHER_AT: theUnit->SetGoal(EEUGRust::NewGather(NULL, EEObjectOf<EEUnit>(inA), false), 0, 0); break;
    case EE_UNITS_UPD_X_ATTACK_GOAL: return EEObj(EEAttackUnitCast(theUnit->GetGoal()));
    case EE_UNITS_UPD_X_ATTACK_TARGET: return EEObj(EEObjectOf<EEUGRust>(inA)->GetAttackTarget());
    case EE_UNITS_UPD_X_NEAREST_TARGET_BUILDING: return EEObj(theClass->FindNearestTargetBuilding(EEObjectOf<EEUnit>(inA)));
    case EE_UNITS_UPD_X_SET_ATTACK_TARGET: EEObjectOf<EEUGRust>(inA)->SetAttackTarget(EEObjectOf<EEUnit>(inB)); break;
    case EE_UNITS_UPD_X_EXITING_PASSENGER: return EEObj(theUnit->FindPassenger(theUnit->mExitingUnitID));
    case EE_UNITS_UPD_X_QUEUE_TAKEOFF: ee_units_atc_queue_request(EEObjectOf<EEComplexUnit>(inA)->mPlayer->mAirTraffic[EEObjectOf<EEComplexUnit>(inA)->mUnitClass->mAirTrafficType - 1], theUnit, EEObjectOf<EEComplexUnit>(inA), 0); break;
    case EE_UNITS_ACT_RELEASE_UNIT:
        theClass->ReleaseUnit(theUnit);
        break;
    case EE_UNITS_UPD_X_GATE_SOUND: return EEObj(EESoundObject::Get(kSoundGate));
    case EE_UNITS_UPD_X_PLAY_SOUND: EEObjectOf<EESoundObject>(inA)->Play(EEObjectOf<float>(inB)[0], EEObjectOf<float>(inB)[1], EEObjectOf<float>(inB)[2]); break;
    case EE_UNITS_UPD_X_LOCATION: *EEObjectOf<GE3DPoint>(inA) = GE3DPoint(theUnit->mPhysicalModel.GetLocation().GetX(), theUnit->mPhysicalModel.GetLocation().GetY(), theUnit->mPhysicalModel.GetLocation().GetZ()); break;
    case EE_UNITS_UPD_X_NEXT_WAYPOINT_POINT: return theUnit->GetNextWaypointPoint(*EEObjectOf<GE3DPoint>(inA));
    case EE_UNITS_UPD_X_SET_HEIGHT_AT: theUnit->SetHeight(theClass->GetHeightAt(theUnit, *EEObjectOf<GE3DPoint>(inA))); break;
    case EE_UNITS_ACT_RETURN_TO_AIRPORT:
        theClass->AirAnimalReturnToAirport(theUnit);
        break;
    case EE_UNITS_UPD_X_AIR_ANIMAL_DEATH_ROLL: EEAirAnimalDeathRoll(theClass, theUnit); break;
    case EE_UNITS_UPD_X_LANDABLE: return EELandableActionOperation((int32_t)inA, theClass, theUnit, inB, &theContext->mAirport);
    case EE_UNITS_UPD_X_HELICOPTER_PATH: EEHelicopterPathToGoal(theClass, theUnit); break;
    case EE_UNITS_UPD_X_HELICOPTER_TRANSPORT_PATH: EEHelicopterTransportPathToGoal(theClass, theUnit); break;
    case EE_UNITS_UPD_X_HELICOPTER_PRIEST_PATH: EEHelicopterPriestPathToGoal(theClass, theUnit); break;
    case EE_UNITS_ACT_AIR_ANIMAL_PATH:
        EEAirAnimalPathToGoal(theClass, theUnit);
        break;
    case EE_UNITS_ACT_CIRCLE_SIDE:
        EEAirplaneCircleSide(theClass, theUnit);
        break;
    case EE_UNITS_UPD_X_AIRPLANE_HOLDING: EEAirplaneHoldingPattern(theClass, theUnit); break;
    case EE_UNITS_UPD_X_SPACESHIP_HOLDING: EEPlanetSpaceshipHoldingPattern(theClass, theUnit); break;
    case EE_UNITS_ACT_FLY_OFF_MAP:
        theClass->FlyOffMap(theUnit);
        break;
    case EE_UNITS_ACT_START_ACTION:
        theUnit->StartAction((unsigned long)inA);
        break;
    case EE_UNITS_ACT_CLASS_ACTION_CHANGED:
        theClass->ActionChanged(theUnit);
        break;
    default:
        break;
    }
    return 0;
}

}  // namespace

void EEUnitClassUpdate(int inKind, EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    EEUpdateContext theContext = {inClass, inUnit, NULL, NULL};
    EeUnitsUpdateHost theHost = {&theContext, EEUpdateRead, EEUpdateAct, EEObj(inClass)};
    ee_units_update(inKind, static_cast<uint32_t>(inUnit->mUnitID), reinterpret_cast<uintptr_t>(inUnit), &theHost);
}

void EEUnitClassActionChanged(int inKind, EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    ee_units_note_action_begin((uint32_t)inUnit->mUnitID);   // TEST ONLY (USST kind 13)
    EEUpdateContext theContext = {inClass, inUnit, NULL, NULL};
    EeUnitsUpdateHost theHost = {&theContext, EEUpdateRead, EEUpdateAct, EEObj(inClass)};
    ee_units_action_changed(inKind, static_cast<uint32_t>(inUnit->mUnitID), reinterpret_cast<uintptr_t>(inUnit),
                            &theHost);
    ee_units_note_action_end();
}
