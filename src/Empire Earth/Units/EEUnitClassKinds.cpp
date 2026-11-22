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

void EEUnitClass::ActionChanged(EEComplexUnit* inUnit)
{
    ee_units_uck_call(EE_UNITS_UCK_DO_ACTION_CHANGED, this, inUnit, 0, 0, 0, 0.0f);   // uck_kinds.rs
}

void EEUnitClass::AddToRegion(EEUnit* inUnit)
{
    switch (EEClassBody(this, EE_UNITS_BODY_ADD_TO_REGION)) {
    case 1: ee_units_uu_footprint(2, (uint64_t)(uintptr_t)inUnit); return;   // uu_world.rs building_region
    case 2: return;
    }
    this->BaseAddToRegion(inUnit);
}

void EEUnitClass::AddToTile(EEUnit* inUnit)
{
    switch (EEClassBody(this, EE_UNITS_BODY_ADD_TO_TILE)) {
    case 1: ee_units_uu_footprint(0, (uint64_t)(uintptr_t)inUnit); return;   // uu_world.rs footprint_link
    case 2: this->SpaceTransportAddToTile(inUnit); return;
    case 3: this->AmbientAddToTile(inUnit); return;
    case 4: this->ResourceAddToTile(inUnit); return;
    case 5: this->SubmergedNavalAddToTile(inUnit); return;
    }
    this->BaseAddToTile(inUnit);
}

void EEUnitClass::BadlyHurt(EEUnit* inUnit)
{
    switch (EEClassBody(this, EE_UNITS_BODY_BADLY_HURT)) {
    case 1: this->LandTransportBadlyHurt(inUnit); return;
    }
}

void EEUnitClass::CallParatroopPlane(EEComplexUnit* inUnit)
{
    switch (EEClassBody(this, EE_UNITS_BODY_CALL_PARATROOP_PLANE)) {
    case 1: this->LandCallParatroopPlane(inUnit); return;
    }
}

bool EEUnitClass::CanUnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    EEUCNote theNote(2); theNote.C(this).U(inTransport).I(inX).I(inY);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_tr_can_unload_at(EEObj(this), EEObj(inTransport), (int32_t)inX, (int32_t)inY));
}

void EEUnitClass::CargoAdded(EEComplexUnit* inUnit, EEComplexUnit* inCargo, bool inArg)
{
    switch (EEClassBody(this, EE_UNITS_BODY_CARGO_ADDED)) {
    case 1: if (!inArg) inUnit->mPlayer->mPopulationCount -= inCargo->mUnitClass->GetPopulationCost(); return;
    }
}

void EEUnitClass::CargoRemoved(EEComplexUnit* inUnit, EEComplexUnit* inCargo)
{
    switch (EEClassBody(this, EE_UNITS_BODY_CARGO_REMOVED)) {
    case 1: inUnit->mPlayer->mPopulationCount += inCargo->mUnitClass->GetPopulationCost(); return;
    }
}

void EEUnitClass::CargoUnloaded(EEComplexUnit* inUnit, EEComplexUnit* inCargo)
{
    switch (EEClassBody(this, EE_UNITS_BODY_CARGO_UNLOADED)) {
    case 1: inUnit->mPlayer->mPopulationCount += inCargo->mUnitClass->GetPopulationCost(); return;
    }
}

bool EEUnitClass::CheckGroundImpact(EEComplexUnit* inUnit)
{
    switch (EEClassBody(this, EE_UNITS_BODY_CHECK_GROUND_IMPACT)) {
    case 1: return this->TorpedoCheckGroundImpact(inUnit);
    case 2: return this->MissileCheckGroundImpact(inUnit);
    case 3: return this->ProjectileCheckGroundImpact(inUnit);
    }
    return false;
}

bool EEUnitClass::CheckWaterImpact(EEComplexUnit* inUnit)
{
    switch (EEClassBody(this, EE_UNITS_BODY_CHECK_WATER_IMPACT)) {
    case 1: return false;
    case 2: return this->MissileCheckWaterImpact(inUnit);
    case 3: return this->ProjectileCheckWaterImpact(inUnit);
    }
    return false;
}

EEUnit* EEUnitClass::CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    switch (EEClassBody(this, EE_UNITS_BODY_CREATE_UNIT)) {
    case 1: return this->MiningBuildingCreateUnit(inX, inY, inPlayer, inIsFoundation, inCreator);
    case 2: return this->NavalCitizenCreateUnit(inX, inY, inPlayer, inIsFoundation, inCreator);
    case 3: return this->SpaceBuildingCreateUnit(inX, inY, inPlayer, inIsFoundation, inCreator);
    case 4: return this->BuildingCreateUnit(inX, inY, inPlayer, inIsFoundation, inCreator);
    case 5: return this->SubmergedNavalCreateUnit(inX, inY, inPlayer, inIsFoundation, inCreator);
    }
    return this->BaseCreateUnit(inX, inY, inPlayer, inIsFoundation, inCreator);
}

EEUnit* EEUnitClass::CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    switch (EEClassBody(this, EE_UNITS_BODY_CREATE_UNIT_AT)) {
    case 1: return this->MiningBuildingCreateUnitAt(inTransformation, inPlayer, inIsFoundation, inCreator);
    case 2: return this->NavalPriestCreateUnitAt(inTransformation, inPlayer, inIsFoundation, inCreator);
    case 3: return this->SpaceBuildingCreateUnitAt(inTransformation, inPlayer, inIsFoundation, inCreator);
    case 4: return this->BuildingCreateUnitAt(inTransformation, inPlayer, inIsFoundation, inCreator);
    case 5: return this->SubmergedNavalCreateUnitAt(inTransformation, inPlayer, inIsFoundation, inCreator);
    }
    return this->BaseCreateUnitAt(inTransformation, inPlayer, inIsFoundation, inCreator);
}

// the kinds' trigger answers are Rust (ee_units cls_condition.rs, R5b.4 round 34); the shots' are ee_combat's
long EEUnitClass::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    return (long)ee_units_uck_call(EE_UNITS_UCK_DO_EVALUATE_CONDITION, this, inUnit, 0, (int64_t)inEvent, 0, 0.0f);   // uck_kinds.rs
}

void EEUnitClass::FilterLoadable(std::vector<EEComplexUnit*>* ioUnits, EEComplexUnit* inTransport, std::vector<EEComplexUnit*>* outRejected)
{
    EEUCNote theNote(6); theNote.C(this).Us(*ioUnits).U(inTransport).Us(*outRejected);   // TEST ONLY (UCBD)
    ee_units_tr_filter_loadable(EEObj(this), ioUnits, EEObj(inTransport), outRejected);
    theNote.Us(*ioUnits).Us(*outRejected);
}

bool EEUnitClass::FindBoardingLocation(EEComplexUnit* inUnit, EEComplexUnit* inTransport, GE3DPoint& inLocation, GE3DPoint& outLocation)
{
    EEUCNote theNote(32); theNote.C(this).U(inUnit).U(inTransport).P(inLocation);   // TEST ONLY (UCBD)
    bool theResult = theNote.R(ee_units_uck_call(EE_UNITS_UCK_DO_BOARDING_LOCATION, this, inUnit, EEObj(inTransport), (int64_t)EEObj(&inLocation), (int64_t)EEObj(&outLocation), 0.0f) != 0);   // uck_kinds.rs
    theNote.P(outLocation);
    return theResult;
}

void EEUnitClass::FindRendezvousTiles(std::vector<EEComplexUnit*>* inUnits, EEComplexUnit* inTransport, U2DPoint<long>* outTransportTile, U2DPoint<long>* outUnitTile)
{
    EEUCNote theNote(4); theNote.C(this).Us(*inUnits).U(inTransport);   // TEST ONLY (UCBD)
    ee_units_tr_rendezvous_tiles(EEObj(this), inUnits, EEObj(inTransport), (long(*)[2])outTransportTile, (long(*)[2])outUnitTile);
    theNote.T(*outTransportTile).T(*outUnitTile);
}

void EEUnitClass::FindUnloadTiles(EEComplexUnit* inTransport, U2DPoint<long>* inDestination, U2DPoint<long>* outUnloadTile, U2DPoint<long>* outFirstCargoTile, U2DPoint<long>* outTile)
{
    EEUCNote theNote(5); theNote.C(this).U(inTransport).T(*inDestination);   // TEST ONLY (UCBD)
    ee_units_tr_unload_tiles(EEObj(this), EEObj(inTransport), (const long(*)[2])inDestination, (long(*)[2])outUnloadTile,
                             (long(*)[2])outFirstCargoTile, (long(*)[2])outTile);
    theNote.T(*outUnloadTile).T(*outFirstCargoTile).T(*outTile);
}

bool EEUnitClass::FliesOver()
{
    switch (EEClassBody(this, EE_UNITS_BODY_FLIES_OVER)) {
    case 1: return true;
    case 2: return this->mDomains & kMovementAir;
    }
    return false;
}

void EEUnitClass::GateReplaced(EEUnit* inGate, EEUnit* inUnit)
{
    switch (EEClassBody(this, EE_UNITS_BODY_GATE_REPLACED)) {
    case 1: this->GateGateReplaced(inGate, inUnit); return;
    }
}

float EEUnitClass::GetHeightAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{
    EEUCNote theNote(11); theNote.C(this).U(inUnit).P(inLocation);   // TEST ONLY (UCBD)
    return theNote.RF(ee_units_cls_height_at(EEObj(this), EEObj(inUnit), inLocation.mX, inLocation.mY, inLocation.mZ));
}

long EEUnitClass::GetPopulationCost()
{
    // every kind's body is this one (the land and helicopter priests' overrides were the same)
    return this->mWeight;
}

void EEUnitClass::GetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan, float* outPitch)
{
    switch (EEClassBody(this, EE_UNITS_BODY_GET_SOUND_PARAMETERS)) {
    case 1: this->AirGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch); return;   // the air animal's: the same Doppler pitch, unnoted
    case 2: this->HelicopterPriestGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch); return;
    case 3: this->SpaceshipGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch); return;
    case 4: this->LandPriestGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch); return;
    case 5: this->SpaceshipVirtual2C(inUnit, inSound, outVolume, outPan, outPitch); return;
    case 6: this->AirGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch); return;   // the landable's: likewise
    }
    this->BaseGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
}

void EEUnitClass::GetSurroundingTiles(EEComplexUnit* inUnit, std::vector<EETileCoordinate>& outTiles, long inCorners, bool inAppend)
{
    EEUCNote theNote(31); theNote.C(this).U(inUnit).I(inCorners).I(inAppend).Ts(outTiles);   // TEST ONLY (UCBD)
    ee_units_uck_call(EE_UNITS_UCK_DO_SURROUNDING_TILES, this, inUnit, EEObj(&outTiles), inCorners, inAppend, 0.0f);   // uck_kinds.rs
    theNote.Ts(outTiles);
}

long EEUnitClass::GetUnloadFailedTextID()
{
    switch (EEClassBody(this, EE_UNITS_BODY_GET_UNLOAD_FAILED_TEXT_ID)) {
    case 1: return 0x7c39;
    case 2: return 0x7c38;
    }
    return 0;
}

void EEUnitClass::GroundLocation(GE3DPoint& ioLocation)
{
    switch (EEClassBody(this, EE_UNITS_BODY_GROUND_LOCATION)) {
    case 1: ioLocation.SetZ(0.0f); return;
    case 2: if (ioLocation.GetZ() < 0.0f) ioLocation.SetZ(0.0f); return;
    case 3: ioLocation.SetZ(this->mCruiseAltitude); return;
    case 4: if (this->mUnitType == kUnitTypeAmbCattails1) ioLocation.SetZ(0.0f); return;
    case 5: this->BuildingGroundLocation(ioLocation); return;
    case 6: this->SubmergedNavalGroundLocation(ioLocation); return;
    }
}

bool EEUnitClass::HasArrived(EEComplexUnit* inUnit, float inValue)
{
    EEUCNote theNote(7); theNote.C(this).U(inUnit).F(inValue);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_uck_call(EE_UNITS_UCK_DO_HAS_ARRIVED, this, inUnit, 0, 0, 0, inValue) != 0);   // uck_kinds.rs
}

bool EEUnitClass::HasRoom(EEComplexUnit* inUnit, EEComplexUnit* inCargo)
{
    EEUCNote theNote(10); theNote.C(this).U(inUnit).U(inCargo);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_uck_call(EE_UNITS_UCK_DO_HAS_ROOM, this, inUnit, EEObj(inCargo), 0, 0, 0.0f) != 0);   // uck_kinds.rs
}

bool EEUnitClass::HasSunk(EEComplexUnit* inUnit)
{
    switch (EEClassBody(this, EE_UNITS_BODY_HAS_SUNK)) {
    case 1: return this->NavalBuildingHasSunk(inUnit);
    case 2: return true;
    }
    return false;
}

bool EEUnitClass::IsBuilding()
{
    return EEClassBody(this, EE_UNITS_BODY_IS_BUILDING) == 1;
}

bool EEUnitClass::IsLandBuilding()
{
    unsigned theBody = EEClassBody(this, EE_UNITS_BODY_IS_LAND_BUILDING);
    return theBody == 1 || theBody == 3;
}

bool EEUnitClass::IsPartLeftOut(EEComplexUnit* inUnit)
{
    switch (EEClassBody(this, EE_UNITS_BODY_IS_PART_LEFT_OUT)) {
    case 1: return !this->IsSidePart(inUnit) && EEGetFirstPart(inUnit)->GetCommandType() != kActionClosed;
    }
    return false;
}

bool EEUnitClass::IsPickable()
{
    switch (EEClassBody(this, EE_UNITS_BODY_IS_PICKABLE)) {
    case 1: return false;
    case 2: return !this->mIsFlare && this->mUnitType != kUnitTypeVolcano;
    }
    return true;
}

bool EEUnitClass::IsSelectable(EEUnit* inUnit)
{
    switch (EEClassBody(this, EE_UNITS_BODY_IS_SELECTABLE)) {
    case 1: return inUnit->mUnitClass->mUnitType == kUnitTypePathPoint;
    case 2: return !this->mIsFlare && this->mUnitType != kUnitTypeVolcano;
    }
    return true;
}

bool EEUnitClass::LaunchProjectile(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{
    switch (EEClassBody(this, EE_UNITS_BODY_LAUNCH_PROJECTILE)) {
    case 1: return this->BombLaunchProjectile(inUnit, inLocation);
    case 2: return this->MissileLaunchProjectile(inUnit, inLocation);
    case 3: return this->ProjectileLaunchProjectile(inUnit, inLocation);
    }
    return false;
}

void EEUnitClass::LoadMedia(EEPlayer* inPlayer)
{
    switch (EEClassBody(this, EE_UNITS_BODY_LOAD_MEDIA)) {
    case 1: this->BasicPriestLoadMedia(inPlayer); return;
    case 2: this->BasicPriestLoadMedia(inPlayer); return;
    }
    this->BaseLoadMedia(inPlayer);
}

void EEUnitClass::LocationToTile(const GE3DPoint& inLocation, long& outX, long& outY)
{
    switch (EEClassBody(this, EE_UNITS_BODY_LOCATION_TO_TILE)) {
    case 1: this->NavalCitizenLocationToTile(inLocation, outX, outY); return;
    case 2: this->SpaceBuildingLocationToTile(inLocation, outX, outY); return;
    case 3: this->AmbientLocationToTile(inLocation, outX, outY); return;
    case 4: this->SubmergedNavalLocationToTile(inLocation, outX, outY); return;
    }
}

void EEUnitClass::RemoveFromRegion(EEUnit* inUnit)
{
    switch (EEClassBody(this, EE_UNITS_BODY_REMOVE_FROM_REGION)) {
    case 1: ee_units_uu_footprint(3, (uint64_t)(uintptr_t)inUnit); return;
    case 2: return;
    }
    this->BaseRemoveFromRegion(inUnit);
}

void EEUnitClass::RemoveFromTile(EEUnit* inUnit)
{
    switch (EEClassBody(this, EE_UNITS_BODY_REMOVE_FROM_TILE)) {
    case 1: ee_units_uu_footprint(1, (uint64_t)(uintptr_t)inUnit); return;
    case 2: this->SpaceTransportRemoveFromTile(inUnit); return;
    case 3: this->AmbientRemoveFromTile(inUnit); return;
    }
    this->BaseRemoveFromTile(inUnit);
}

bool EEUnitClass::SelectUnitToRelease(EEComplexUnit* inUnit)
{
    EEUCNote theNote(30); theNote.C(this).U(inUnit);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_uck_call(EE_UNITS_UCK_DO_SELECT_TO_RELEASE, this, inUnit, 0, 0, 0, 0.0f) != 0);   // uck_kinds.rs
}

void EEUnitClass::SimpleUnitCreated(EEUnit* inUnit)
{
    switch (EEClassBody(this, EE_UNITS_BODY_SIMPLE_UNIT_CREATED)) {
    case 1: this->ResourceSimpleUnitCreated(inUnit); return;
    }
    this->BaseSimpleUnitCreated(inUnit);
}

void EEUnitClass::UnitCreated(EEComplexUnit* inUnit)
{
    switch (EEClassBody(this, EE_UNITS_BODY_UNIT_CREATED)) {
    case 1: this->AirAnimalUnitCreated(inUnit); return;
    case 2: this->FortressUnitCreated(inUnit); return;
    case 3: this->GateUnitCreated(inUnit); return;
    case 4: this->HelicopterPriestUnitCreated(inUnit); return;
    case 5: this->HelicopterTransportUnitCreated(inUnit); return;
    case 6: this->SpaceBuildingUnitCreated(inUnit); return;
    case 7: this->SpaceTransportUnitCreated(inUnit); return;
    case 8: this->BuildingUnitCreated(inUnit); return;
    case 9: this->LandUnitCreated(inUnit); return;
    case 10: this->LandableUnitCreated(inUnit); return;
    case 11: this->MissileUnitCreated(inUnit); return;
    case 12: this->NavalUnitCreated(inUnit); return;
    case 13: this->ProjectileUnitCreated(inUnit); return;
    case 14: this->TransportUnitCreated(inUnit); return;
    }
    this->BaseUnitCreated(inUnit);
}

void EEUnitClass::UnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    switch (EEClassBody(this, EE_UNITS_BODY_UNLOAD_AT)) {
    case 1: this->FortressUnloadAt(inTransport, inX, inY); return;
    case 2: this->TransportUnloadAt(inTransport, inX, inY); return;
    }
}

void EEUnitClass::UnloadMedia(EEPlayer* inPlayer)
{
    switch (EEClassBody(this, EE_UNITS_BODY_UNLOAD_MEDIA)) {
    case 1: this->BasicPriestUnloadMedia(inPlayer); return;
    case 2: this->BasicPriestUnloadMedia(inPlayer); return;
    }
    this->BaseUnloadMedia(inPlayer);
}

void EEUnitClass::Update(EEComplexUnit* inUnit)
{
    ee_units_uck_call(EE_UNITS_UCK_DO_UPDATE, this, inUnit, 0, 0, 0, 0.0f);   // uck_kinds.rs
}

void EEUnitClass::UpdateEffects(EEComplexUnit* inUnit)
{
    ee_units_uck_call(EE_UNITS_UCK_DO_UPDATE_EFFECTS, this, inUnit, 0, 0, 0, 0.0f);   // uck_kinds.rs
}

long EEUnitClass::Virtual70()
{
    return EEClassBody(this, EE_UNITS_BODY_VIRTUAL70);   // 1 a building, 2 a resource
}

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
    default: return ee_units_uc_host(inOp, inClass, inUnit, inA, inB, inC, ioF);
    }
}

// TEST ONLY: the UCBD record (ee/EEUnitClassKinds.h EEUCNote; the unit utilities' UUTL record's pattern)
static thread_local int sUCDepth = 0;
static std::mutex sUCMutex;
static uint64_t sUCDigest = 0xcbf29ce484222325ull;
static uint32_t sUCCalls = 0;
static unsigned long sUCTick = 0;
static uint32_t sUCKinds[64];

EEUCNote::EEUCNote(unsigned int inKind) : mOn(++sUCDepth == 1 && EESimCaptureRecord::Active())
{
    this->I((long)inKind);
}

EEUCNote& EEUCNote::F(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return this->I((long)theBits);
}

EEUCNote& EEUCNote::U(const EEUnit* inUnit)
{
    return this->I(inUnit != NULL ? (long)((const RTSUnit*)inUnit)->mUnitID : -2);
}

EEUCNote& EEUCNote::C(const EEUnitClass* inClass)
{
    return this->I(inClass != NULL ? (long)inClass->mUnitType : -2);
}

EEUCNote& EEUCNote::Ts(const std::vector<U2DPoint<long> >& inTiles)
{
    this->I((long)inTiles.size());
    for (size_t i = 0; this->mOn && i < inTiles.size(); i++)
        this->T(inTiles[i]);
    return *this;
}

EEUCNote& EEUCNote::Us(const std::vector<EEComplexUnit*>& inUnits)
{
    this->I((long)inUnits.size());
    for (size_t i = 0; this->mOn && i < inUnits.size(); i++)
        this->U(inUnits[i]);
    return *this;
}

EEUCNote& EEUCNote::Us(const std::vector<EEUnit*>& inUnits)
{
    this->I((long)inUnits.size());
    for (size_t i = 0; this->mOn && i < inUnits.size(); i++)
        this->U(inUnits[i]);
    return *this;
}

EEUCNote& EEUCNote::S(EEComplexUnit* inUnit)
{
    if (!this->mOn || inUnit == NULL)
        return this->I(-2);
    this->I((long)inUnit->mWaypoints.size());
    for (size_t i = 0; i < inUnit->mWaypoints.size(); i++)
        this->P(inUnit->mWaypoints[i].mLocation).F(inUnit->mWaypoints[i].mDistance).F(inUnit->mWaypoints[i].mStartDistance);
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    this->F(theLocation.GetX()).F(theLocation.GetY()).F(theLocation.GetZ());
    EEUnitGoal* theGoal = inUnit->mGoalState;
    this->I(theGoal != NULL ? (long)theGoal->mKind : -1);
    if (theGoal != NULL && EEGoalHasLocation(theGoal))
        this->P(EELocationGoalPoint(theGoal));
    return *this;
}

EEUCNote::~EEUCNote()
{
    sUCDepth--;
    if (!this->mOn)
        return;
    std::lock_guard<std::mutex> theLock(sUCMutex);
    unsigned long theTick = gPhysics != NULL ? gPhysics->mNumTicksDone : 0;
    if (theTick != sUCTick) {
        if (sUCCalls != 0) {
            EESimCaptureRecord theRecord(0x44424355u);   // "UCBD"
            theRecord.U32((unsigned int)sUCTick);
            theRecord.U32(sUCCalls);
            theRecord.U32((unsigned int)(sUCDigest & 0xffffffffu));
            theRecord.U32((unsigned int)(sUCDigest >> 32));
            theRecord.Commit();
        }
        if (theTick / 20000 != sUCTick / 20000) {
            fprintf(stderr, "EE-UCBD-COUNT %lu", theTick);
            for (int i = 1; i < 50; i++)
                fprintf(stderr, " %d:%u", i, sUCKinds[i]);
            fprintf(stderr, "\n");
        }
        sUCDigest = 0xcbf29ce484222325ull;
        sUCCalls = 0;
        sUCTick = theTick;
    }
    sUCKinds[this->mWords[0] & 63]++;
    for (size_t i = 0; i < this->mWords.size(); i++)
        for (int b = 0; b < 4; b++) {
            sUCDigest ^= (this->mWords[i] >> (8 * b)) & 0xff;
            sUCDigest *= 0x100000001b3ull;
        }
    sUCCalls++;
}
