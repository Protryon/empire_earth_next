// EEUnitClassKinds.cpp (new, R5b.4 round 32): EEUnitClass's virtuals as dispatchers by the class's kind.
// Round 34: the kind tests are one Rust table (ee_units cls_dispatch.rs, EEClassBody in ee/EEUnitKind.h): each
// dispatcher switches over its row's body for the class, in the order the tests below were written.
// The 36 EEUC* subclasses (one per category the class factory built) are gone: a class is one EEUnitClass whose
// kind is its category's entry in ee_units' table (kinds.rs EE_UNIT_CLASS_KINDS: the class's bit and its
// ancestors'). Each dispatcher tests the kinds deepest first, as the vtables resolved an override: the first
// kind with a body of its own runs it; the others run the base body (Base*) or the default.
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <mutex>

#include "lle/LowLevelEngine.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EECombatGoals.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEUnitUpdate.h"
#include "ee/EEPhysics.h"
#include "ee/EEUGRust.h"

// the kinds' trigger answers are Rust (ee_units cls_condition.rs, R5b.4 round 34); the shots' are ee_combat's


template <class T> static T* EEUckObject(uint64_t inHandle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle));
}

// The bodies the kind dispatchers of ee_units uck_kinds.rs choose (EE_UNITS_UCK_*): one statement each; the class
// host's statements (EE_UNITS_UC_*) pass through to EEUnitClass.cpp's
extern "C" int64_t ee_units_uck_host(int32_t inOp, void* inClass, void* inUnit, uint64_t inA, int64_t inB, int64_t inC,
                                     float (*ioF)[4])
{
    EEUnitClass* theClass = static_cast<EEUnitClass*>(inClass);
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    float* f = *ioF;
    switch (inOp) {
    case EE_UNITS_UCK_KINDS: return (int64_t)EEUnitClassKinds(theClass);
    case EE_UNITS_UCK_BODY: return EEClassBody(theClass, (size_t)inB);
    case EE_UNITS_UCK_MISSILE_ACTION_CHANGED: theClass->MissileActionChanged(theUnit); return 0;
    case EE_UNITS_UCK_PROJECTILE_ACTION_CHANGED: theClass->ProjectileActionChanged(theUnit); return 0;
    case EE_UNITS_UCK_ACTION_CHANGED: EEUnitClassActionChanged((int32_t)inB, theClass, theUnit); return 0;
    case EE_UNITS_UCK_UPDATE: EEUnitClassUpdate((int32_t)inB, theClass, theUnit); return 0;
    case EE_UNITS_UCK_BOMB_UPDATE: theClass->BombUpdate(theUnit); return 0;
    case EE_UNITS_UCK_PRIEST_UPDATES: return !theUnit->IsFoundation() || theUnit->mHitPoints <= 0 || theUnit->mCurrentPhysicsAction == 2;
    case EE_UNITS_UCK_PRIEST_UPDATE: theClass->PriestVirtualB8(theUnit); return 0;
    case EE_UNITS_UCK_TORPEDO_UPDATE: theClass->TorpedoUpdate(theUnit); return 0;
    case EE_UNITS_UCK_CITIZEN_UPDATE: theClass->UpdateCitizen(theUnit); return 0;
    case EE_UNITS_UCK_MISSILE_UPDATE: theClass->MissileUpdate(theUnit); return 0;
    case EE_UNITS_UCK_PROJECTILE_UPDATE: theClass->ProjectileUpdate(theUnit); return 0;
    case EE_UNITS_UCK_RIPPLES_DUE:
        return theClass->mRippleEffect != NULL && theUnit->mUnitClass->mUnitType == kUnitTypeCatalina && theUnit->mHitPoints != 0 && theClass->mRippleEffect->mDetailLevel <= gEffectDetail;
    case EE_UNITS_UCK_RIPPLES_DUE_3:
        return theClass->mRippleEffect != NULL && theUnit->mHitPoints != 0 && (unsigned long)theClass->mRippleEffect->mDetailLevel <= gEffectDetail;
    case EE_UNITS_UCK_RIPPLES_DUE_4:
        return (theClass->mUnitType == kUnitTypeDock || theClass->mUnitType == kUnitTypeNavyYard) && theClass->mRippleEffect != NULL && theUnit->mHitPoints != 0 && theClass->mRippleEffect->mDetailLevel <= gEffectDetail;
    case EE_UNITS_UCK_RIPPLES_DUE_5:
        return theClass->mRippleEffect != NULL && theUnit->mHitPoints > 0 && theClass->mRippleEffect->mDetailLevel <= gEffectDetail;
    case EE_UNITS_UCK_UPDATE_RIPPLES: theClass->UpdateRipples(theUnit); return 0;
    case EE_UNITS_UCK_TRAIL_DUE:
        return theUnit->mHitPoints > 0 && theClass->mTrailEffect != NULL && theUnit->mVelocity != 0.0f && theClass->mTrailEffect->mDetailLevel <= gEffectDetail;
    case EE_UNITS_UCK_UPDATE_TRAIL: theClass->UpdateTrail(theUnit); return 0;
    case EE_UNITS_UCK_PROJECTILE_EFFECTS: theClass->ProjectileUpdateEffects(theUnit); return 0;
    case EE_UNITS_UCK_CONDITION: return ee_units_cls_condition(EEObj(theClass), EEObj(theUnit), (uint32_t)inB);
    case EE_UNITS_UCK_TORPEDO_CONDITION: return EECombatCondition(EE_COMBAT_GOAL_TORPEDO, theUnit, (unsigned long)inB, EECombatShotTarget(theUnit), NULL, theClass);
    case EE_UNITS_UCK_MISSILE_CONDITION: return theClass->MissileEvaluateCondition(theUnit, (unsigned long)inB);
    case EE_UNITS_UCK_PROJECTILE_CONDITION: return EECombatCondition(EE_COMBAT_GOAL_PROJECTILE, theUnit, (unsigned long)inB, EECombatShotTarget(theUnit), NULL, theClass);
    case EE_UNITS_UCK_LANDING_ARRIVED: return ee_units_cls_landing_has_arrived(EEObj(theClass), EEObj(theUnit), f[0], inB != 0);
    case EE_UNITS_UCK_SPACE_BUILDING_ARRIVED: return theClass->SpaceBuildingHasArrived(theUnit, f[0]);
    case EE_UNITS_UCK_NAVAL_TRANSPORT_ARRIVED: return ee_units_tr_naval_has_arrived(EEObj(theUnit), f[0]);
    case EE_UNITS_UCK_NAVAL_ARRIVED: return theClass->NavalHasArrived(theUnit, f[0]);
    case EE_UNITS_UCK_SUBMERGED_ARRIVED: return theClass->SubmergedNavalHasArrived(theUnit, f[0]);
    case EE_UNITS_UCK_BASE_ARRIVED: return theClass->BaseHasArrived(theUnit, f[0]);
    case EE_UNITS_UCK_FORTRESS_HAS_ROOM: return theClass->FortressHasRoom(theUnit, EEUckObject<EEComplexUnit>(inA));
    case EE_UNITS_UCK_NAVAL_SELECT: return theClass->NavalBuildingSelectUnitToRelease(theUnit);
    case EE_UNITS_UCK_BUILDING_SELECT: return theClass->BuildingSelectUnitToRelease(theUnit);
    case EE_UNITS_UCK_BOARDING_LOCATION:
        return theClass->TransportFindBoardingLocation(theUnit, EEUckObject<EEComplexUnit>(inA), *EEUckObject<GE3DPoint>((uint64_t)inB), *EEUckObject<GE3DPoint>((uint64_t)inC));
    case EE_UNITS_UCK_GATE_SURROUNDING: theClass->GateGetSurroundingTiles(theUnit, *EEUckObject<std::vector<EETileCoordinate> >(inA), (long)inB, inC != 0); return 0;
    case EE_UNITS_UCK_BUILDING_SURROUNDING: theClass->BuildingGetSurroundingTiles(theUnit, *EEUckObject<std::vector<EETileCoordinate> >(inA), (long)inB, inC != 0); return 0;
    case EE_UNITS_UCK_BASE_SURROUNDING: theClass->BaseGetSurroundingTiles(theUnit, *EEUckObject<std::vector<EETileCoordinate> >(inA), (long)inB, inC != 0); return 0;
    // round 9's dispatcher bodies (uck_dispatch.rs: EE_UNITS_UCK_D_*)
    case EE_UNITS_UCK_D_BASE_ADD_TO_REGION: theClass->BaseAddToRegion(EEUckObject<EEUnit>(inA)); return 0;
    case EE_UNITS_UCK_D_SPACE_TRANSPORT_ADD_TO_TILE: theClass->SpaceTransportAddToTile(EEUckObject<EEUnit>(inA)); return 0;
    case EE_UNITS_UCK_D_AMBIENT_ADD_TO_TILE: theClass->AmbientAddToTile(EEUckObject<EEUnit>(inA)); return 0;
    case EE_UNITS_UCK_D_RESOURCE_ADD_TO_TILE: theClass->ResourceAddToTile(EEUckObject<EEUnit>(inA)); return 0;
    case EE_UNITS_UCK_D_SUBMERGED_ADD_TO_TILE: theClass->SubmergedNavalAddToTile(EEUckObject<EEUnit>(inA)); return 0;
    case EE_UNITS_UCK_D_LAND_TRANSPORT_BADLY_HURT: theClass->LandTransportBadlyHurt(EEUckObject<EEUnit>(inA)); return 0;
    case EE_UNITS_UCK_D_LAND_CALL_PARATROOP_PLANE: theClass->LandCallParatroopPlane(theUnit); return 0;
    case EE_UNITS_UCK_D_POPULATION_SUB: theUnit->mPlayer->mPopulationCount -= EEUckObject<EEComplexUnit>(inA)->mUnitClass->GetPopulationCost(); return 0;
    case EE_UNITS_UCK_D_POPULATION_ADD: theUnit->mPlayer->mPopulationCount += EEUckObject<EEComplexUnit>(inA)->mUnitClass->GetPopulationCost(); return 0;
    case EE_UNITS_UCK_D_TORPEDO_GROUND_IMPACT: return theClass->TorpedoCheckGroundImpact(theUnit);
    case EE_UNITS_UCK_D_MISSILE_GROUND_IMPACT: return theClass->MissileCheckGroundImpact(theUnit);
    case EE_UNITS_UCK_D_PROJECTILE_GROUND_IMPACT: return theClass->ProjectileCheckGroundImpact(theUnit);
    case EE_UNITS_UCK_D_MISSILE_WATER_IMPACT: return theClass->MissileCheckWaterImpact(theUnit);
    case EE_UNITS_UCK_D_PROJECTILE_WATER_IMPACT: return theClass->ProjectileCheckWaterImpact(theUnit);
    case EE_UNITS_UCK_D_MINING_CREATE_UNIT: return EEObj(theClass->MiningBuildingCreateUnit((long)(int32_t)inB, (long)(int32_t)(inB >> 32), (long)(inC >> 1), (inC & 1) != 0, EEUckObject<EEUnit>(inA)));
    case EE_UNITS_UCK_D_NAVAL_CITIZEN_CREATE_UNIT: return EEObj(theClass->NavalCitizenCreateUnit((long)(int32_t)inB, (long)(int32_t)(inB >> 32), (long)(inC >> 1), (inC & 1) != 0, EEUckObject<EEUnit>(inA)));
    case EE_UNITS_UCK_D_SPACE_BUILDING_CREATE_UNIT: return EEObj(theClass->SpaceBuildingCreateUnit((long)(int32_t)inB, (long)(int32_t)(inB >> 32), (long)(inC >> 1), (inC & 1) != 0, EEUckObject<EEUnit>(inA)));
    case EE_UNITS_UCK_D_BUILDING_CREATE_UNIT: return EEObj(theClass->BuildingCreateUnit((long)(int32_t)inB, (long)(int32_t)(inB >> 32), (long)(inC >> 1), (inC & 1) != 0, EEUckObject<EEUnit>(inA)));
    case EE_UNITS_UCK_D_SUBMERGED_CREATE_UNIT: return EEObj(theClass->SubmergedNavalCreateUnit((long)(int32_t)inB, (long)(int32_t)(inB >> 32), (long)(inC >> 1), (inC & 1) != 0, EEUckObject<EEUnit>(inA)));
    case EE_UNITS_UCK_D_BASE_CREATE_UNIT: return EEObj(theClass->BaseCreateUnit((long)(int32_t)inB, (long)(int32_t)(inB >> 32), (long)(inC >> 1), (inC & 1) != 0, EEUckObject<EEUnit>(inA)));
    case EE_UNITS_UCK_D_MINING_CREATE_UNIT_AT: return EEObj(theClass->MiningBuildingCreateUnitAt(*EEUckObject<GETransformation>((uint64_t)inB), (long)(inC >> 1), (inC & 1) != 0, EEUckObject<EEUnit>(inA)));
    case EE_UNITS_UCK_D_NAVAL_PRIEST_CREATE_UNIT_AT: return EEObj(theClass->NavalPriestCreateUnitAt(*EEUckObject<GETransformation>((uint64_t)inB), (long)(inC >> 1), (inC & 1) != 0, EEUckObject<EEUnit>(inA)));
    case EE_UNITS_UCK_D_SPACE_BUILDING_CREATE_UNIT_AT: return EEObj(theClass->SpaceBuildingCreateUnitAt(*EEUckObject<GETransformation>((uint64_t)inB), (long)(inC >> 1), (inC & 1) != 0, EEUckObject<EEUnit>(inA)));
    case EE_UNITS_UCK_D_BUILDING_CREATE_UNIT_AT: return EEObj(theClass->BuildingCreateUnitAt(*EEUckObject<GETransformation>((uint64_t)inB), (long)(inC >> 1), (inC & 1) != 0, EEUckObject<EEUnit>(inA)));
    case EE_UNITS_UCK_D_SUBMERGED_CREATE_UNIT_AT: return EEObj(theClass->SubmergedNavalCreateUnitAt(*EEUckObject<GETransformation>((uint64_t)inB), (long)(inC >> 1), (inC & 1) != 0, EEUckObject<EEUnit>(inA)));
    case EE_UNITS_UCK_D_BASE_CREATE_UNIT_AT: return EEObj(theClass->BaseCreateUnitAt(*EEUckObject<GETransformation>((uint64_t)inB), (long)(inC >> 1), (inC & 1) != 0, EEUckObject<EEUnit>(inA)));
    case EE_UNITS_UCK_D_GATE_GATE_REPLACED: theClass->GateGateReplaced(EEUckObject<EEUnit>(inA), EEUckObject<EEUnit>((uint64_t)inB)); return 0;
    case EE_UNITS_UCK_D_AIR_SOUND: theClass->AirGetSoundParameters(EEUckObject<EEUnit>(inA), EEUckObject<EESoundObject>((uint64_t)inB), EEUckObject<float*>((uint64_t)inC)[0], EEUckObject<float*>((uint64_t)inC)[1], EEUckObject<float*>((uint64_t)inC)[2]); return 0;
    case EE_UNITS_UCK_D_BUILDING_GROUND_LOCATION: theClass->BuildingGroundLocation(*reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UCK_D_SUBMERGED_GROUND_LOCATION: theClass->SubmergedNavalGroundLocation(*reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UCK_D_IS_SIDE_PART: return theClass->IsSidePart(theUnit);
    case EE_UNITS_UCK_D_FIRST_PART_COMMAND: return EEGetFirstPart(theUnit)->GetCommandType();
    case EE_UNITS_UCK_D_BOMB_LAUNCH: return theClass->BombLaunchProjectile(theUnit, *EEUckObject<GE3DPoint>(inA));
    case EE_UNITS_UCK_D_MISSILE_LAUNCH: return theClass->MissileLaunchProjectile(theUnit, *EEUckObject<GE3DPoint>(inA));
    case EE_UNITS_UCK_D_PROJECTILE_LAUNCH: return theClass->ProjectileLaunchProjectile(theUnit, *EEUckObject<GE3DPoint>(inA));
    case EE_UNITS_UCK_D_NAVAL_CITIZEN_TO_TILE: theClass->NavalCitizenLocationToTile(*EEUckObject<GE3DPoint>(inA), *EEUckObject<long>((uint64_t)inB), *EEUckObject<long>((uint64_t)inC)); return 0;
    case EE_UNITS_UCK_D_SPACE_BUILDING_TO_TILE: theClass->SpaceBuildingLocationToTile(*EEUckObject<GE3DPoint>(inA), *EEUckObject<long>((uint64_t)inB), *EEUckObject<long>((uint64_t)inC)); return 0;
    case EE_UNITS_UCK_D_AMBIENT_TO_TILE: theClass->AmbientLocationToTile(*EEUckObject<GE3DPoint>(inA), *EEUckObject<long>((uint64_t)inB), *EEUckObject<long>((uint64_t)inC)); return 0;
    case EE_UNITS_UCK_D_SUBMERGED_TO_TILE: theClass->SubmergedNavalLocationToTile(*EEUckObject<GE3DPoint>(inA), *EEUckObject<long>((uint64_t)inB), *EEUckObject<long>((uint64_t)inC)); return 0;
    case EE_UNITS_UCK_D_BASE_REMOVE_FROM_REGION: theClass->BaseRemoveFromRegion(EEUckObject<EEUnit>(inA)); return 0;
    case EE_UNITS_UCK_D_SPACE_TRANSPORT_REMOVE: theClass->SpaceTransportRemoveFromTile(EEUckObject<EEUnit>(inA)); return 0;
    case EE_UNITS_UCK_D_AMBIENT_REMOVE: theClass->AmbientRemoveFromTile(EEUckObject<EEUnit>(inA)); return 0;
    case EE_UNITS_UCK_D_RESOURCE_SIMPLE_CREATED: theClass->ResourceSimpleUnitCreated(EEUckObject<EEUnit>(inA)); return 0;
    case EE_UNITS_UCK_D_BASE_SIMPLE_CREATED: theClass->BaseSimpleUnitCreated(EEUckObject<EEUnit>(inA)); return 0;
    case EE_UNITS_UCK_D_AIR_ANIMAL_CREATED: theClass->AirAnimalUnitCreated(theUnit); return 0;
    case EE_UNITS_UCK_D_FORTRESS_CREATED: theClass->FortressUnitCreated(theUnit); return 0;
    case EE_UNITS_UCK_D_GATE_CREATED: theClass->GateUnitCreated(theUnit); return 0;
    case EE_UNITS_UCK_D_HELICOPTER_TRANSPORT_CREATED: theClass->HelicopterTransportUnitCreated(theUnit); return 0;
    case EE_UNITS_UCK_D_SPACE_BUILDING_CREATED: theClass->SpaceBuildingUnitCreated(theUnit); return 0;
    case EE_UNITS_UCK_D_SPACE_TRANSPORT_CREATED: theClass->SpaceTransportUnitCreated(theUnit); return 0;
    case EE_UNITS_UCK_D_BUILDING_CREATED: theClass->BuildingUnitCreated(theUnit); return 0;
    case EE_UNITS_UCK_D_LAND_CREATED: theClass->LandUnitCreated(theUnit); return 0;
    case EE_UNITS_UCK_D_LANDABLE_CREATED: theClass->LandableUnitCreated(theUnit); return 0;
    case EE_UNITS_UCK_D_MISSILE_CREATED: theClass->MissileUnitCreated(theUnit); return 0;
    case EE_UNITS_UCK_D_NAVAL_CREATED: theClass->NavalUnitCreated(theUnit); return 0;
    case EE_UNITS_UCK_D_PROJECTILE_CREATED: theClass->ProjectileUnitCreated(theUnit); return 0;
    case EE_UNITS_UCK_D_TRANSPORT_CREATED: theClass->TransportUnitCreated(theUnit); return 0;
    case EE_UNITS_UCK_D_FORTRESS_UNLOAD_AT: theClass->FortressUnloadAt(theUnit, (long)inB, (long)inC); return 0;
    case EE_UNITS_UCK_D_TRANSPORT_UNLOAD_AT: theClass->TransportUnloadAt(theUnit, (long)inB, (long)inC); return 0;
    default: return ee_units_uc_host(inOp, inClass, inUnit, inA, inB, inC, ioF);
    }
}

// (TEST ONLY, the UCBD record: its notes are Rust's, ee_sim units ucn_noted.rs over ucbd_note.rs; EEUCNote is gone)
