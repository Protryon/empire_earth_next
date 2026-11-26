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
#include "lle/USingleton.h"
#include "lle/SESoundManager.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEGameVariant.h"
#include "ee/EECalamityManager.h"
#include "ee/EEAIArea.h"
#include "ee/EEHerd.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEE2DBattlefieldAmbient.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEUGRust.h"

namespace {

// EEUnitAI.cpp's: no initial contact heading
const float kUnitAINoHeading = -10000.0f;

// the host's context, EEUpdateContext: the class, the unit, an ActionChanged's healee found
// (EE_UNITS_ACT_FIND_HEALEE), the nearest airport found (EE_UNITS_ACT_FIND_NEAREST_AIRPORT); the Rust record
// EeUnitsUpdateContext (upd_entry.rs, a cbindgen rename)

// (the initial contact location and heading, EEUpdateContact: ee_sim units upd_split.rs update_contact)

template <class T> T* EEObjectOf(int64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }

EEUpdateContext* EEUpdateContextOf(void* inContext) { return static_cast<EEUpdateContext*>(inContext); }
EEComplexUnit* EEUpdateUnit(void* inContext) { return EEUpdateContextOf(inContext)->mUnit; }
EEUnitClass* EEUpdateClass(void* inContext) { return EEUpdateContextOf(inContext)->mClass; }

int64_t EEUpdateRead(void* inContext, int32_t inCode)
{
    switch (inCode) {
    case EE_UNITS_UPD_R_ANIMAL_ON_MAP: return EEHerd::IsAnimalOnMap(EEUpdateUnit(inContext)) ? 1 : 0;
    // (the plain member reads are Rust's: ee_units update_views.rs)
    case EE_UNITS_UPD_R_CALAMITY_CAMOUFLAGE: return USingleton<EECalamityManager>::GetInstance()->IsCalamityActive(0x17) ? 1 : 0;
    case EE_UNITS_UPD_R_IS_PRIEST: return EEUpdateClass(inContext)->IsPriest() ? 1 : 0;
    case EE_UNITS_UPD_R_WORK_TIME_FACTOR: return (int64_t)UFloatBits(::gWorld.GetGameVariant()->mWorkTimeFactor);
    // ActionChanged's
    case EE_UNITS_ACT_R_ATTACK_FRAME_SET: return (int64_t)(unsigned long)EEGetAttackFrameSet(EEUpdateUnit(inContext));
    case EE_UNITS_ACT_R_SWIMMING: return EEUpdateUnit(inContext)->IsSwimming() ? 1 : 0;
    case EE_UNITS_ACT_R_RANDOM: return (int64_t)UFloatBits(gPhysics->GetRandom());
    case EE_UNITS_ACT_R_WORLD_CLEARING: return gWorldClearing ? 1 : 0;
    case EE_UNITS_ACT_R_IS_HUMAN: return EEUpdateUnit(inContext)->mPlayer->IsHuman() ? 1 : 0;
    case EE_UNITS_ACT_R_BUILD_QUEUE_EMPTY: return EEUpdateUnit(inContext)->mCitizenBuildQueue.size() == 0 ? 1 : 0;
    default: return 0;
    }
}

int64_t EEUpdateAct(void* inContext, int32_t inCode, int64_t inA, int64_t inB)
{
    // (the one-member stores and the death effect are Rust's: ee_units update_views.rs view_act; the notes (USST
    // kinds 12, 13) and the compound cases' choices too: update_views.rs, upd_split.rs; each statement a case here)
    switch (inCode) {
    case EE_UNITS_UPD_PHYSICS_STEP: EEUpdateClass(inContext)->mPhysicsModel->Step(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_TURRETS: EEUpdateUnit(inContext)->UpdateTurrets(); break;
    case EE_UNITS_UPD_WHEELS: EEUpdateUnit(inContext)->UpdateWheels(); break;
    case EE_UNITS_UPD_ORBITALS: EEUpdateUnit(inContext)->UpdateOrbitals(); break;
    case EE_UNITS_UPD_EFFECTS: EEUpdateClass(inContext)->UpdateEffects(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_RELOAD_TIMER: EEUpdateUnit(inContext)->UpdateReloadTimer(); break;
    case EE_UNITS_UPD_TARGET_SEARCH_TIMER: EEUpdateUnit(inContext)->UpdateTargetSearchTimer(); break;
    case EE_UNITS_UPD_CAMOUFLAGE_TIMER: EEUpdateClass(inContext)->UpdateCamouflageTimer(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_ANIMATE: return EEUpdateClass(inContext)->Animate(EEUpdateUnit(inContext));
    case EE_UNITS_UPD_REFRESH_SOUND: EEUpdateClass(inContext)->RefreshSound(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_REGENERATE_POWER: EEUpdateClass(inContext)->RegeneratePower(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_FANATICISM_COLOR: EEUpdateUnit(inContext)->UpdateFanaticismColor(); break;
    case EE_UNITS_UPD_FIRE_ON_ATTACK_FRAME: EEUpdateClass(inContext)->FireOnAttackFrame(EEUpdateUnit(inContext), (long)inA, (long)inB); break;
    case EE_UNITS_UPD_ROTTING: return (int64_t)EEUpdateUnit(inContext)->UpdateRottingAwayTime();
    case EE_UNITS_UPD_HEAL_SEARCH_TIMER: EEUpdateUnit(inContext)->UpdateHealSearchTimer(); break;
    case EE_UNITS_UPD_GOAL_WORK: EEUpdateUnit(inContext)->mGoalState->Work(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_REGENERATE: EEUpdateClass(inContext)->Regenerate(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_DONE: EEUpdateClass(inContext)->UpdateDone(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_ANIMAL: EEUpdateClass(inContext)->UpdateAnimal(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_RESOURCE_UNIT: EEUpdateClass(inContext)->UpdateResourceUnit(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_CONTINUE_PRODUCTION: EEUpdateUnit(inContext)->ContinueProduction(); break;
    case EE_UNITS_UPD_UPDATE_CITIZEN: EEUpdateClass(inContext)->UpdateCitizen(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_UPDATE_CONVERSION: EEUpdateClass(inContext)->BasicPriestUpdateConversion(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_HIDE_CONVERSION_EFFECTS: EEUpdateClass(inContext)->BasicPriestHideConversionEffects(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_RESOURCES_ROTTING: EEUpdateUnit(inContext)->UpdateResourcesRotting(); break;
    case EE_UNITS_UPD_NEXT_FLIGHT_STATE: return (int64_t)EEUpdateClass(inContext)->GetNextFlightState(EEUpdateUnit(inContext));
    case EE_UNITS_UPD_UPDATE_BUILDING: EEUpdateClass(inContext)->UpdateBuilding(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_UPDATE_ARMED: EEUpdateClass(inContext)->UpdateArmed(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_ADD_RESOURCE: EEUpdateUnit(inContext)->mPlayer->AddResource((long)inA, (long)inB); break;
    case EE_UNITS_UPD_SAVED_GROUPS_RESET: EEUpdateUnit(inContext)->mPlayer->mSavedGroups.Reset(7, 1); break;
    case EE_UNITS_UPD_X_CARRIER_OPEN_FRACTION: EEUpdateClass(inContext)->GetLastLinkedCarrier(EEUpdateUnit(inContext))->SetOpenFraction(UBitsFloat(inA)); break;
    case EE_UNITS_UPD_X_LINKED_OPEN_FRACTION: EEUpdateClass(inContext)->GetLastLinked(EEUpdateUnit(inContext))->SetOpenFraction(UBitsFloat(inA)); break;
    case EE_UNITS_UPD_DEPLETE_MINE: EEUpdateUnit(inContext)->mMine->DepleteResource(1); break;
    // ActionChanged's
    case EE_UNITS_ACT_HERD_ATTACK_BACK: EEUpdateUnit(inContext)->mHerd->AttackBack(EEUpdateUnit(inContext), true); break;
    case EE_UNITS_ACT_HERD_UNIT_HIT: EEUpdateUnit(inContext)->mHerd->HandleUnitHit(EEUpdateUnit(inContext)); break;
    case EE_UNITS_ACT_ATTACKED: EEUpdateClass(inContext)->Attacked(EEUpdateUnit(inContext)); break;
    case EE_UNITS_ACT_FLEE_ATTACKER: EEUpdateClass(inContext)->FleeAttacker(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_X_RETURN_TO_CONTACT: EEUpdateUnit(inContext)->SetGoal(EEUGRust::NewLocation(&EEUpdateUnit(inContext)->mInitialContactLocation, false, false, true, EEUpdateUnit(inContext)->mInitialContactHeading, false, false), inA != 0, 0); break;
    case EE_UNITS_ACT_RANDOM_ANIMATION_TIME:
        EEUpdateUnit(inContext)->SetAnimationTime(gPhysics->GetRandom(0L, (long)EEUpdateUnit(inContext)->mStats->mModel->mHoldTimes[EEUpdateUnit(inContext)->GetAnimation()]));
        break;
    case EE_UNITS_ACT_UNIT_GOAL_DONE: EEUpdateUnit(inContext)->mPlayer->UnitGoalDone(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_X_ACTION_CHANGED_OF: EEObjectOf<EEComplexUnit>(inA)->mUnitClass->ActionChanged(EEObjectOf<EEComplexUnit>(inA)); break;
    case EE_UNITS_ACT_UNIT_DIED: EEUpdateClass(inContext)->UnitDied(EEUpdateUnit(inContext)); break;
    case EE_UNITS_ACT_GOAL_STEP: EEUpdateUnit(inContext)->mGoalState->Step(EEUpdateUnit(inContext)); break;
    case EE_UNITS_ACT_DESELECT: EEUpdateUnit(inContext)->mPlayer->DeselectUnit(EEUpdateUnit(inContext)); break;
    case EE_UNITS_ACT_CLOAK_CHAIN: EEUpdateUnit(inContext)->SetCloakedChain(true); break;
    case EE_UNITS_ACT_KILL: EEUpdateUnit(inContext)->Kill(inA != 0, false, false); break;
    case EE_UNITS_UPD_X_KILL_UNIT: EEUpdateUnit(inContext)->mPlayer->KillUnit(EEObjectOf<EEComplexUnit>(inA), false); break;
    case EE_UNITS_ACT_PATH_TO_GOAL: EEUpdateClass(inContext)->PathToGoal(EEUpdateUnit(inContext)); break;
    case EE_UNITS_ACT_MOVE_TO_TILE_CENTRE: EEUpdateClass(inContext)->MoveToTileCentre(EEUpdateUnit(inContext)); break;
    case EE_UNITS_ACT_FLEE: EEUpdateClass(inContext)->Flee(EEUpdateUnit(inContext)); break;
    case EE_UNITS_ACT_NEXT_WAYPOINT: EEUpdateClass(inContext)->NextWaypoint(EEUpdateUnit(inContext)); break;
    case EE_UNITS_ACT_FIND_SAFE_LOCATION: EEUpdateClass(inContext)->FindSafeLocation(EEUpdateUnit(inContext)); break;
    case EE_UNITS_ACT_FACE_YAW: return static_cast<EEUGRust*>(EEUpdateUnit(inContext)->mGoalState)->FaceYaw(EEUpdateUnit(inContext)) ? 1 : 0;
    case EE_UNITS_ACT_FIND_PATH: EEUpdateClass(inContext)->FindPath(EEUpdateUnit(inContext)); break;
    case EE_UNITS_ACT_HEAL: EEUpdateClass(inContext)->Heal(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_X_FIND_HEALEE: return EEUpdateClass(inContext)->FindTargetNear(EEUpdateUnit(inContext), EEUpdateUnit(inContext)->mStats->GetLineOfSight(), &EEUpdateContextOf(inContext)->mHealee);
    case EE_UNITS_UPD_X_SET_HEAL_GOAL: EEUpdateUnit(inContext)->SetGoal(EEUGRust::NewHealUnit(static_cast<EEComplexUnit*>(EEUpdateContextOf(inContext)->mHealee), false), inA != 0, 0); break;
    case EE_UNITS_ACT_GOAL_START_TIME: EEUpdateUnit(inContext)->mGoalState->SetStartTime(gPhysics->mGameTime); break;
    case EE_UNITS_ACT_CALL_PARATROOP_PLANE: EEUpdateClass(inContext)->CallParatroopPlane(EEUpdateUnit(inContext)); break;
    case EE_UNITS_ACT_PLACE_TRACK_DECAL: EEPlaceTrackDecal(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_X_SOUND_PARAMETERS: EEUpdateClass(inContext)->GetSoundParameters(EEUpdateUnit(inContext), EEObjectOf<EESoundObject>(inA), EEObjectOf<float>(inB), EEObjectOf<float>(inB) + 1, EEObjectOf<float>(inB) + 2); break;
    case EE_UNITS_UPD_X_PLAY_SAMPLE: gSoundManager->Play(EEObjectOf<EESoundObject>(inA)->mSampleID, EEObjectOf<float>(inB)[0], EEObjectOf<float>(inB)[1], EEObjectOf<float>(inB)[2], EEObjectOf<EESoundObject>(inA)->mLooping, NULL); break;
    case EE_UNITS_ACT_HIDE_CONVERSION_EFFECTS:
        // (the temple's too: its copy was the basic priest's body)
        EEUpdateClass(inContext)->BasicPriestHideConversionEffects(EEUpdateUnit(inContext));
        break;
    case EE_UNITS_UPD_X_FIND_STORAGE: return EEObj(EEUpdateClass(inContext)->FindStorage(EEUpdateUnit(inContext), inA != 0));
    case EE_UNITS_UPD_X_GATHER_AT: EEUpdateUnit(inContext)->SetGoal(EEUGRust::NewGather(NULL, EEObjectOf<EEUnit>(inA), false), 0, 0); break;
    case EE_UNITS_UPD_X_ATTACK_GOAL: return EEObj(EEAttackUnitCast(EEUpdateUnit(inContext)->GetGoal()));
    case EE_UNITS_UPD_X_ATTACK_TARGET: return EEObj(EEObjectOf<EEUGRust>(inA)->GetAttackTarget());
    case EE_UNITS_UPD_X_NEAREST_TARGET_BUILDING: return EEObj(EEUpdateClass(inContext)->FindNearestTargetBuilding(EEObjectOf<EEUnit>(inA)));
    case EE_UNITS_UPD_X_SET_ATTACK_TARGET: EEObjectOf<EEUGRust>(inA)->SetAttackTarget(EEObjectOf<EEUnit>(inB)); break;
    case EE_UNITS_UPD_X_EXITING_PASSENGER: return EEObj(EEUpdateUnit(inContext)->FindPassenger(EEUpdateUnit(inContext)->mExitingUnitID));
    case EE_UNITS_UPD_X_QUEUE_TAKEOFF: ee_units_atc_queue_request(EEObjectOf<EEComplexUnit>(inA)->mPlayer->mAirTraffic[EEObjectOf<EEComplexUnit>(inA)->mUnitClass->mAirTrafficType - 1], EEUpdateUnit(inContext), EEObjectOf<EEComplexUnit>(inA), 0); break;
    case EE_UNITS_ACT_RELEASE_UNIT: EEUpdateClass(inContext)->ReleaseUnit(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_X_GATE_SOUND: return EEObj(EESoundObject::Get(kSoundGate));
    case EE_UNITS_UPD_X_PLAY_SOUND: EEObjectOf<EESoundObject>(inA)->Play(EEObjectOf<float>(inB)[0], EEObjectOf<float>(inB)[1], EEObjectOf<float>(inB)[2]); break;
    case EE_UNITS_UPD_X_NEXT_WAYPOINT_POINT: return EEUpdateUnit(inContext)->GetNextWaypointPoint(*EEObjectOf<GE3DPoint>(inA));
    case EE_UNITS_UPD_X_SET_HEIGHT_AT: EEUpdateUnit(inContext)->SetHeight(EEUpdateClass(inContext)->GetHeightAt(EEUpdateUnit(inContext), *EEObjectOf<GE3DPoint>(inA))); break;
    case EE_UNITS_ACT_RETURN_TO_AIRPORT: EEUpdateClass(inContext)->AirAnimalReturnToAirport(EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_X_AIR_ANIMAL_DEATH_ROLL: EEAirAnimalDeathRoll(EEUpdateClass(inContext), EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_X_LANDABLE: return EELandableActionOperation((int32_t)inA, EEUpdateClass(inContext), EEUpdateUnit(inContext), inB, &EEUpdateContextOf(inContext)->mAirport);
    case EE_UNITS_UPD_X_HELICOPTER_PATH: EEHelicopterPathToGoal(EEUpdateClass(inContext), EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_X_HELICOPTER_TRANSPORT_PATH: EEHelicopterTransportPathToGoal(EEUpdateClass(inContext), EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_X_HELICOPTER_PRIEST_PATH: EEHelicopterPriestPathToGoal(EEUpdateClass(inContext), EEUpdateUnit(inContext)); break;
    case EE_UNITS_ACT_AIR_ANIMAL_PATH: EEAirAnimalPathToGoal(EEUpdateClass(inContext), EEUpdateUnit(inContext)); break;
    case EE_UNITS_ACT_CIRCLE_SIDE: EEAirplaneCircleSide(EEUpdateClass(inContext), EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_X_AIRPLANE_HOLDING: EEAirplaneHoldingPattern(EEUpdateClass(inContext), EEUpdateUnit(inContext)); break;
    case EE_UNITS_UPD_X_SPACESHIP_HOLDING: EEPlanetSpaceshipHoldingPattern(EEUpdateClass(inContext), EEUpdateUnit(inContext)); break;
    case EE_UNITS_ACT_FLY_OFF_MAP: EEUpdateClass(inContext)->FlyOffMap(EEUpdateUnit(inContext)); break;
    case EE_UNITS_ACT_CLASS_ACTION_CHANGED: EEUpdateClass(inContext)->ActionChanged(EEUpdateUnit(inContext)); break;
    default: break;
    }
    return 0;
}

}  // namespace

// the host table's reads and acts (ee_sim units upd_entry.rs makes the table over an EEUpdateContext)
extern "C" int64_t ee_units_upd_read(void* inContext, int32_t inCode) { return EEUpdateRead(inContext, inCode); }
extern "C" int64_t ee_units_upd_act(void* inContext, int32_t inCode, int64_t inA, int64_t inB) { return EEUpdateAct(inContext, inCode, inA, inB); }

// (EEUnitClassUpdate and EEUnitClassActionChanged: inline in ee/EEUnitUpdate.h)
