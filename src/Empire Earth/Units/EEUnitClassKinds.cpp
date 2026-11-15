// EEUnitClassKinds.cpp (new, R5b.4 round 32): EEUnitClass's virtuals as dispatchers by the class's kind.
// The 36 EEUC* subclasses (one per category the class factory built) are gone: a class is one EEUnitClass whose
// kind is its category's entry in ee_units' table (kinds.rs EE_UNIT_CLASS_KINDS: the class's bit and its
// ancestors'). Each dispatcher tests the kinds deepest first, as the vtables resolved an override: the first
// kind with a body of its own runs it; the others run the base body (Base*) or the default.
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitAction.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EECombatGoals.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEUnitKind.h"
#include "ee/EEUnitUpdate.h"
#include "rust/ee_units.h"

void EEUnitClass::ActionChanged(EEComplexUnit* inUnit)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_AIR_ANIMAL)
        EEUnitClassActionChanged(EE_UNITS_ACTION_AIR_ANIMAL, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_AIRPLANE)
        EEUnitClassActionChanged(EE_UNITS_ACTION_AIRPLANE, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_BUILDING_PRIEST)
        EEUnitClassActionChanged(EE_UNITS_ACTION_BUILDING_PRIEST, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_GATE)
        EEUnitClassActionChanged(EE_UNITS_ACTION_GATE, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_HELICOPTER_PRIEST)
        EEUnitClassActionChanged(EE_UNITS_ACTION_HELICOPTER_PRIEST, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_HELICOPTER_TRANSPORT)
        EEUnitClassActionChanged(EE_UNITS_ACTION_HELICOPTER_TRANSPORT, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_MINING_BUILDING)
        EEUnitClassActionChanged(EE_UNITS_ACTION_MINING_BUILDING, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_PLANET_SPACESHIP)
        EEUnitClassActionChanged(EE_UNITS_ACTION_PLANET_SPACESHIP, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_BASIC_CITIZEN)
        EEUnitClassActionChanged(EE_UNITS_ACTION_CITIZEN, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_BASIC_PRIEST)
        EEUnitClassActionChanged(EE_UNITS_ACTION_BASIC_PRIEST, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_BUILDING)
        EEUnitClassActionChanged(EE_UNITS_ACTION_BUILDING, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_HELICOPTER)
        EEUnitClassActionChanged(EE_UNITS_ACTION_HELICOPTER, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_LAND)
        EEUnitClassActionChanged(EE_UNITS_ACTION_LAND, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_LANDABLE)
        EEUnitClassActionChanged(EE_UNITS_ACTION_LANDABLE, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_MISSILE)
        this->MissileActionChanged(inUnit);
    else if (theKinds & EE_CLASS_KIND_NAVAL)
        EEUnitClassActionChanged(EE_UNITS_ACTION_NAVAL, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_PROJECTILE)
        this->ProjectileActionChanged(inUnit);
    else if (theKinds & EE_CLASS_KIND_SPACESHIP)
        EEUnitClassActionChanged(EE_UNITS_ACTION_SPACESHIP, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_SUBMERGED_NAVAL)
        EEUnitClassActionChanged(EE_UNITS_ACTION_SUBMERGED_NAVAL, this, inUnit);
    else
        EEUnitClassActionChanged(EE_UNITS_ACTION_BASE, this, inUnit);
}

void EEUnitClass::AddToRegion(EEUnit* inUnit)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & (EE_CLASS_KIND_FORTRESS | EE_CLASS_KIND_BUILDING))
        EEAddBuildingRegion(inUnit);
    else if (theKinds & (EE_CLASS_KIND_MISSILE | EE_CLASS_KIND_PROJECTILE)) {
    } else
        this->BaseAddToRegion(inUnit);
}

void EEUnitClass::AddToTile(EEUnit* inUnit)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & (EE_CLASS_KIND_FORTRESS | EE_CLASS_KIND_BUILDING))
        EEAddToFootprintTiles(inUnit);
    else if (theKinds & (EE_CLASS_KIND_HELICOPTER_PRIEST | EE_CLASS_KIND_HELICOPTER_TRANSPORT | EE_CLASS_KIND_NAVAL_TRANSPORT | EE_CLASS_KIND_SPACE_TRANSPORT | EE_CLASS_KIND_HELICOPTER | EE_CLASS_KIND_LANDABLE | EE_CLASS_KIND_NAVAL | EE_CLASS_KIND_SPACESHIP))
        this->SpaceTransportAddToTile(inUnit);
    else if (theKinds & EE_CLASS_KIND_AMBIENT)
        this->AmbientAddToTile(inUnit);
    else if (theKinds & EE_CLASS_KIND_RESOURCE)
        this->ResourceAddToTile(inUnit);
    else if (theKinds & EE_CLASS_KIND_SUBMERGED_NAVAL)
        this->SubmergedNavalAddToTile(inUnit);
    else
        this->BaseAddToTile(inUnit);
}

void EEUnitClass::AdjustPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inSmooth)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_NAVAL_CITIZEN)
        this->NavalCitizenAdjustPath(inUnit, ioPath, inSmooth);
    else if (theKinds & EE_CLASS_KIND_NAVAL_PRIEST)
        this->NavalPriestAdjustPath(inUnit, ioPath, inSmooth);
    else if (theKinds & (EE_CLASS_KIND_NAVAL_TRANSPORT | EE_CLASS_KIND_NAVAL))
        EEAdjustNavalPath(inUnit, ioPath, inSmooth);
    else if (theKinds & EE_CLASS_KIND_SEA_ANIMAL)
        this->SeaAnimalAdjustPath(inUnit, ioPath, inSmooth);
    else if (theKinds & EE_CLASS_KIND_SUBMERGED_NAVAL)
        this->NavalPriestAdjustPath(inUnit, ioPath, inSmooth);
}

void EEUnitClass::BadlyHurt(EEUnit* inUnit)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_LAND_TRANSPORT)
        this->LandTransportBadlyHurt(inUnit);
}

void EEUnitClass::CallParatroopPlane(EEComplexUnit* inUnit)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_LAND)
        this->LandCallParatroopPlane(inUnit);
}

bool EEUnitClass::CanAttackMove()
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_AIR_ANIMAL)
        return false;
    if (theKinds & (EE_CLASS_KIND_HELICOPTER_TRANSPORT | EE_CLASS_KIND_BASIC_PRIEST | EE_CLASS_KIND_HELICOPTER | EE_CLASS_KIND_LAND | EE_CLASS_KIND_LANDABLE | EE_CLASS_KIND_NAVAL | EE_CLASS_KIND_SPACESHIP | EE_CLASS_KIND_SUBMERGED_NAVAL))
        return true;
    return false;
}

bool EEUnitClass::CanCarry(EEComplexUnit* inUnit1, EEComplexUnit* inUnit2)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_LAND_TRANSPORT)
        return this->LandTransportCanCarry(inUnit1, inUnit2);
    if (theKinds & (EE_CLASS_KIND_NAVAL_TRANSPORT | EE_CLASS_KIND_SPACE_TRANSPORT))
        return this->SpaceTransportCanCarry(inUnit1, inUnit2);
    if (theKinds & EE_CLASS_KIND_TRANSPORT)
        return this->TransportCanCarry(inUnit1, inUnit2);
    return false;
}

bool EEUnitClass::CanUnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_FORTRESS)
        return this->FortressCanUnloadAt(inTransport, inX, inY);
    if (theKinds & EE_CLASS_KIND_HELICOPTER_TRANSPORT)
        return this->HelicopterTransportCanUnloadAt(inTransport, inX, inY);
    if (theKinds & EE_CLASS_KIND_LAND_TRANSPORT)
        return this->LandTransportCanUnloadAt(inTransport, inX, inY);
    if (theKinds & EE_CLASS_KIND_NAVAL_TRANSPORT)
        return this->NavalTransportCanUnloadAt(inTransport, inX, inY);
    if (theKinds & EE_CLASS_KIND_SPACE_TRANSPORT)
        return this->SpaceTransportCanUnloadAt(inTransport, inX, inY);
    if (theKinds & EE_CLASS_KIND_TRANSPORT)
        return this->TransportCanUnloadAt(inTransport, inX, inY);
    return false;
}

void EEUnitClass::CargoAdded(EEComplexUnit* inUnit, EEComplexUnit* inCargo, bool inArg)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_FORTRESS) {
        if (!inArg) inUnit->mPlayer->mPopulationCount -= inCargo->mUnitClass->GetPopulationCost();
    }
}

void EEUnitClass::CargoRemoved(EEComplexUnit* inUnit, EEComplexUnit* inCargo)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_FORTRESS)
        inUnit->mPlayer->mPopulationCount += inCargo->mUnitClass->GetPopulationCost();
}

void EEUnitClass::CargoUnloaded(EEComplexUnit* inUnit, EEComplexUnit* inCargo)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_FORTRESS)
        inUnit->mPlayer->mPopulationCount += inCargo->mUnitClass->GetPopulationCost();
}

bool EEUnitClass::CheckGroundImpact(EEComplexUnit* inUnit)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_TORPEDO)
        return this->TorpedoCheckGroundImpact(inUnit);
    if (theKinds & EE_CLASS_KIND_MISSILE)
        return this->MissileCheckGroundImpact(inUnit);
    if (theKinds & EE_CLASS_KIND_PROJECTILE)
        return this->ProjectileCheckGroundImpact(inUnit);
    return false;
}

bool EEUnitClass::CheckWaterImpact(EEComplexUnit* inUnit)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_TORPEDO)
        return false;
    if (theKinds & EE_CLASS_KIND_MISSILE)
        return this->MissileCheckWaterImpact(inUnit);
    if (theKinds & EE_CLASS_KIND_PROJECTILE)
        return this->ProjectileCheckWaterImpact(inUnit);
    return false;
}

EEUnit* EEUnitClass::CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_MINING_BUILDING)
        return this->MiningBuildingCreateUnit(inX, inY, inPlayer, inIsFoundation, inCreator);
    if (theKinds & (EE_CLASS_KIND_NAVAL_BUILDING | EE_CLASS_KIND_NAVAL_CITIZEN | EE_CLASS_KIND_NAVAL_PRIEST | EE_CLASS_KIND_NAVAL_TRANSPORT | EE_CLASS_KIND_NAVAL))
        return this->NavalCitizenCreateUnit(inX, inY, inPlayer, inIsFoundation, inCreator);
    if (theKinds & EE_CLASS_KIND_SPACE_BUILDING)
        return this->SpaceBuildingCreateUnit(inX, inY, inPlayer, inIsFoundation, inCreator);
    if (theKinds & EE_CLASS_KIND_BUILDING)
        return this->BuildingCreateUnit(inX, inY, inPlayer, inIsFoundation, inCreator);
    if (theKinds & EE_CLASS_KIND_SUBMERGED_NAVAL)
        return this->SubmergedNavalCreateUnit(inX, inY, inPlayer, inIsFoundation, inCreator);
    return this->BaseCreateUnit(inX, inY, inPlayer, inIsFoundation, inCreator);
}

EEUnit* EEUnitClass::CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_MINING_BUILDING)
        return this->MiningBuildingCreateUnitAt(inTransformation, inPlayer, inIsFoundation, inCreator);
    if (theKinds & (EE_CLASS_KIND_NAVAL_BUILDING | EE_CLASS_KIND_NAVAL_CITIZEN | EE_CLASS_KIND_NAVAL_PRIEST | EE_CLASS_KIND_NAVAL_TRANSPORT | EE_CLASS_KIND_NAVAL))
        return this->NavalPriestCreateUnitAt(inTransformation, inPlayer, inIsFoundation, inCreator);
    if (theKinds & EE_CLASS_KIND_SPACE_BUILDING)
        return this->SpaceBuildingCreateUnitAt(inTransformation, inPlayer, inIsFoundation, inCreator);
    if (theKinds & EE_CLASS_KIND_BUILDING)
        return this->BuildingCreateUnitAt(inTransformation, inPlayer, inIsFoundation, inCreator);
    if (theKinds & EE_CLASS_KIND_SUBMERGED_NAVAL)
        return this->SubmergedNavalCreateUnitAt(inTransformation, inPlayer, inIsFoundation, inCreator);
    return this->BaseCreateUnitAt(inTransformation, inPlayer, inIsFoundation, inCreator);
}

long EEUnitClass::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_AIR_ANIMAL)
        return this->AirAnimalEvaluateCondition(inUnit, inEvent);
    if (theKinds & EE_CLASS_KIND_FORTRESS)
        return this->FortressEvaluateCondition(inUnit, inEvent);
    if (theKinds & EE_CLASS_KIND_BUILDING_PRIEST)
        return this->BuildingPriestEvaluateCondition(inUnit, inEvent);
    if (theKinds & EE_CLASS_KIND_GATE)
        return this->GateEvaluateCondition(inUnit, inEvent);
    if (theKinds & EE_CLASS_KIND_HELICOPTER_PRIEST)
        return this->HelicopterPriestEvaluateCondition(inUnit, inEvent);
    if (theKinds & (EE_CLASS_KIND_HELICOPTER_TRANSPORT | EE_CLASS_KIND_HELICOPTER))
        return this->HelicopterTransportEvaluateCondition(inUnit, inEvent);
    if (theKinds & EE_CLASS_KIND_MINING_BUILDING)
        return this->MiningBuildingEvaluateCondition(inUnit, inEvent);
    if (theKinds & (EE_CLASS_KIND_NAVAL_BUILDING | EE_CLASS_KIND_SPACE_BUILDING))
        return this->NavalBuildingEvaluateCondition(inUnit, inEvent);
    if (theKinds & EE_CLASS_KIND_NAVAL_CITIZEN)
        return this->NavalCitizenEvaluateCondition(inUnit, inEvent);
    if (theKinds & EE_CLASS_KIND_NAVAL_PRIEST)
        return this->NavalPriestEvaluateCondition(inUnit, inEvent);
    if (theKinds & (EE_CLASS_KIND_NAVAL_TRANSPORT | EE_CLASS_KIND_NAVAL))
        return this->NavalEvaluateCondition(inUnit, inEvent);
    if (theKinds & EE_CLASS_KIND_SPACE_TRANSPORT)
        return this->SpaceTransportEvaluateCondition(inUnit, inEvent);
    if (theKinds & EE_CLASS_KIND_TORPEDO)
        return EECombatCondition(EE_COMBAT_GOAL_TORPEDO, inUnit, inEvent, EECombatShotTarget(inUnit), NULL, this);
    if (theKinds & EE_CLASS_KIND_BASIC_CITIZEN)
        return this->BasicCitizenEvaluateCondition(inUnit, inEvent);
    if (theKinds & EE_CLASS_KIND_BASIC_PRIEST)
        return this->BasicPriestEvaluateCondition(inUnit, inEvent);
    if (theKinds & EE_CLASS_KIND_BUILDING)
        return this->BuildingEvaluateCondition(inUnit, inEvent);
    if (theKinds & EE_CLASS_KIND_LANDABLE)
        return this->LandableEvaluateCondition(inUnit, inEvent);
    if (theKinds & EE_CLASS_KIND_MISSILE)
        return this->MissileEvaluateCondition(inUnit, inEvent);
    if (theKinds & EE_CLASS_KIND_PROJECTILE)
        return EECombatCondition(EE_COMBAT_GOAL_PROJECTILE, inUnit, inEvent, EECombatShotTarget(inUnit), NULL, this);
    if (theKinds & EE_CLASS_KIND_SPACESHIP)
        return this->SpaceshipEvaluateCondition(inUnit, inEvent);
    if (theKinds & EE_CLASS_KIND_SUBMERGED_NAVAL)
        return this->SubmergedNavalEvaluateCondition(inUnit, inEvent);
    return this->BaseEvaluateCondition(inUnit, inEvent);
}

void EEUnitClass::FilterLoadable(std::vector<EEComplexUnit*>* ioUnits, EEComplexUnit* inTransport, std::vector<EEComplexUnit*>* outRejected)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_TRANSPORT)
        this->TransportFilterLoadable(ioUnits, inTransport, outRejected);
}

bool EEUnitClass::FindBoardingLocation(EEComplexUnit* inUnit, EEComplexUnit* inTransport, GE3DPoint& inLocation, GE3DPoint& outLocation)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_TRANSPORT)
        return this->TransportFindBoardingLocation(inUnit, inTransport, inLocation, outLocation);
    return false;
}

long EEUnitClass::FindGatheringTile(std::vector<EEComplexUnit*>* inUnits, U2DPoint<long> inFrom, U2DPoint<long>* outTile)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_TRANSPORT)
        return this->TransportFindGatheringTile(inUnits, inFrom, outTile);
    return 0;
}

void EEUnitClass::FindRendezvousTiles(std::vector<EEComplexUnit*>* inUnits, EEComplexUnit* inTransport, U2DPoint<long>* outTransportTile, U2DPoint<long>* outUnitTile)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_FORTRESS)
        this->TransportFindRendezvousTiles(inUnits, inTransport, outTransportTile, outUnitTile);
    else if (theKinds & EE_CLASS_KIND_HELICOPTER_TRANSPORT)
        this->HelicopterTransportFindRendezvousTiles(inUnits, inTransport, outTransportTile, outUnitTile);
    else if (theKinds & EE_CLASS_KIND_LAND_TRANSPORT)
        this->LandTransportFindRendezvousTiles(inUnits, inTransport, outTransportTile, outUnitTile);
    else if (theKinds & (EE_CLASS_KIND_NAVAL_TRANSPORT | EE_CLASS_KIND_SPACE_TRANSPORT))
        this->SpaceTransportFindRendezvousTiles(inUnits, inTransport, outTransportTile, outUnitTile);
    else if (theKinds & EE_CLASS_KIND_TRANSPORT)
        this->TransportFindRendezvousTiles(inUnits, inTransport, outTransportTile, outUnitTile);
}

void EEUnitClass::FindUnloadTiles(EEComplexUnit* inTransport, U2DPoint<long>* inDestination, U2DPoint<long>* outUnloadTile, U2DPoint<long>* outFirstCargoTile, U2DPoint<long>* outTile)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_FORTRESS)
        this->FortressFindUnloadTiles(inTransport, inDestination, outUnloadTile, outFirstCargoTile, outTile);
    else if (theKinds & EE_CLASS_KIND_AIRPLANE)
        *outTile = *outFirstCargoTile = *outUnloadTile = *inDestination;
    else if (theKinds & EE_CLASS_KIND_NAVAL_TRANSPORT)
        this->NavalTransportFindUnloadTiles(inTransport, inDestination, outUnloadTile, outFirstCargoTile, outTile);
    else if (theKinds & EE_CLASS_KIND_SPACE_TRANSPORT)
        this->SpaceTransportFindUnloadTiles(inTransport, inDestination, outUnloadTile, outFirstCargoTile, outTile);
    else if (theKinds & EE_CLASS_KIND_TRANSPORT)
        *outTile = *outFirstCargoTile = *outUnloadTile = *inDestination;
}

bool EEUnitClass::FliesOver()
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & (EE_CLASS_KIND_HELICOPTER_PRIEST | EE_CLASS_KIND_HELICOPTER))
        return true;
    if (theKinds & EE_CLASS_KIND_TRANSPORT)
        return this->mDomains & kMovementAir;
    return false;
}

void EEUnitClass::GateReplaced(EEUnit* inGate, EEUnit* inUnit)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_GATE)
        this->GateGateReplaced(inGate, inUnit);
}

float EEUnitClass::GetHeightAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_AIR_ANIMAL)
        return this->AirAnimalGetHeightAt(inUnit, inLocation);
    if (theKinds & EE_CLASS_KIND_HELICOPTER_PRIEST)
        return this->HelicopterPriestGetHeightAt(inUnit, inLocation);
    if (theKinds & EE_CLASS_KIND_HELICOPTER_TRANSPORT)
        return this->HelicopterTransportGetHeightAt(inUnit, inLocation);
    if (theKinds & EE_CLASS_KIND_SPACE_TRANSPORT)
        return this->SpaceshipVirtual24(inUnit, inLocation);
    if (theKinds & EE_CLASS_KIND_HELICOPTER)
        return this->HelicopterGetHeightAt(inUnit, inLocation);
    if (theKinds & EE_CLASS_KIND_LANDABLE)
        return this->LandableGetHeightAt(inUnit, inLocation);
    if (theKinds & EE_CLASS_KIND_SPACESHIP)
        return this->SpaceshipGetHeightAt(inUnit, inLocation);
    return inLocation.GetZ();
}

long EEUnitClass::GetPopulationCost()
{
    // every kind's body is this one (the land and helicopter priests' overrides were the same)
    return this->mWeight;
}

void EEUnitClass::GetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan, float* outPitch)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_AIR_ANIMAL)
        this->AirAnimalGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    else if (theKinds & EE_CLASS_KIND_HELICOPTER_PRIEST)
        this->HelicopterPriestGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    else if (theKinds & (EE_CLASS_KIND_HELICOPTER_TRANSPORT | EE_CLASS_KIND_HELICOPTER | EE_CLASS_KIND_SPACESHIP))
        this->SpaceshipGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    else if (theKinds & EE_CLASS_KIND_LAND_PRIEST)
        this->LandPriestGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    else if (theKinds & EE_CLASS_KIND_SPACE_TRANSPORT)
        this->SpaceshipVirtual2C(inUnit, inSound, outVolume, outPan, outPitch);
    else if (theKinds & EE_CLASS_KIND_LANDABLE)
        this->LandableGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    else
        this->BaseGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
}

void EEUnitClass::GetSurroundingTiles(EEComplexUnit* inUnit, std::vector<EETileCoordinate>& outTiles, long inCorners, bool inAppend)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_GATE)
        this->GateGetSurroundingTiles(inUnit, outTiles, inCorners, inAppend);
    else if (theKinds & EE_CLASS_KIND_BUILDING)
        this->BuildingGetSurroundingTiles(inUnit, outTiles, inCorners, inAppend);
    else
        this->BaseGetSurroundingTiles(inUnit, outTiles, inCorners, inAppend);
}

long EEUnitClass::GetUnloadFailedTextID()
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_FORTRESS)
        return 0x7c39;
    if (theKinds & EE_CLASS_KIND_TRANSPORT)
        return 0x7c38;
    return 0;
}

void EEUnitClass::GroundLocation(GE3DPoint& ioLocation)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_NAVAL_BUILDING)
        ioLocation.SetZ(0.0f);
    else if (theKinds & (EE_CLASS_KIND_NAVAL_CITIZEN | EE_CLASS_KIND_NAVAL_TRANSPORT | EE_CLASS_KIND_NAVAL)) {
        if (ioLocation.GetZ() < 0.0f) ioLocation.SetZ(0.0f);
    } else if (theKinds & EE_CLASS_KIND_SPACE_BUILDING)
        ioLocation.SetZ(this->mCruiseAltitude);
    else if (theKinds & EE_CLASS_KIND_AMBIENT) {
        if (this->mUnitType == kUnitTypeAmbCattails1) ioLocation.SetZ(0.0f);
    } else if (theKinds & EE_CLASS_KIND_BUILDING)
        this->BuildingGroundLocation(ioLocation);
    else if (theKinds & EE_CLASS_KIND_SUBMERGED_NAVAL)
        this->SubmergedNavalGroundLocation(ioLocation);
}

bool EEUnitClass::HasArrived(EEComplexUnit* inUnit, float inValue)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_AIR_ANIMAL)
        return this->AirAnimalHasArrived(inUnit, inValue);
    if (theKinds & (EE_CLASS_KIND_NAVAL_BUILDING | EE_CLASS_KIND_SPACE_BUILDING))
        return this->SpaceBuildingHasArrived(inUnit, inValue);
    if (theKinds & EE_CLASS_KIND_NAVAL_TRANSPORT)
        return this->NavalTransportHasArrived(inUnit, inValue);
    if (theKinds & EE_CLASS_KIND_LANDABLE)
        return this->LandableHasArrived(inUnit, inValue);
    if (theKinds & EE_CLASS_KIND_NAVAL)
        return this->NavalHasArrived(inUnit, inValue);
    if (theKinds & EE_CLASS_KIND_SUBMERGED_NAVAL)
        return this->SubmergedNavalHasArrived(inUnit, inValue);
    return this->BaseHasArrived(inUnit, inValue);
}

bool EEUnitClass::HasRoom(EEComplexUnit* inUnit, EEComplexUnit* inCargo)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_FORTRESS)
        return this->FortressHasRoom(inUnit, inCargo);
    if (theKinds & (EE_CLASS_KIND_HELICOPTER_TRANSPORT | EE_CLASS_KIND_LAND_TRANSPORT | EE_CLASS_KIND_NAVAL_TRANSPORT | EE_CLASS_KIND_SPACE_TRANSPORT))
        return true;
    return false;
}

bool EEUnitClass::HasSunk(EEComplexUnit* inUnit)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_NAVAL_BUILDING)
        return this->NavalBuildingHasSunk(inUnit);
    if (theKinds & EE_CLASS_KIND_SPACE_BUILDING)
        return true;
    return false;
}

bool EEUnitClass::IsBuilding()
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_BUILDING)
        return true;
    return false;
}

bool EEUnitClass::IsLandBuilding()
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_FORTRESS)
        return true;
    if (theKinds & (EE_CLASS_KIND_NAVAL_BUILDING | EE_CLASS_KIND_SPACE_BUILDING))
        return false;
    if (theKinds & EE_CLASS_KIND_BUILDING)
        return true;
    return false;
}

bool EEUnitClass::IsPartLeftOut(EEComplexUnit* inUnit)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_GATE)
        return !this->IsSidePart(inUnit) && EEGetFirstPart(inUnit)->GetCommandType() != kActionClosed;
    return false;
}

bool EEUnitClass::IsPickable()
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_AMBIENT)
        return false;
    if (theKinds & EE_CLASS_KIND_BUILDING)
        return !this->mIsFlare && this->mUnitType != kUnitTypeVolcano;
    return true;
}

bool EEUnitClass::IsSelectable(EEUnit* inUnit)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_AMBIENT)
        return inUnit->mUnitClass->mUnitType == kUnitTypePathPoint;
    if (theKinds & EE_CLASS_KIND_BUILDING)
        return !this->mIsFlare && this->mUnitType != kUnitTypeVolcano;
    return true;
}

bool EEUnitClass::LaunchProjectile(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_BOMB)
        return this->BombLaunchProjectile(inUnit, inLocation);
    if (theKinds & EE_CLASS_KIND_MISSILE)
        return this->MissileLaunchProjectile(inUnit, inLocation);
    if (theKinds & EE_CLASS_KIND_PROJECTILE)
        return this->ProjectileLaunchProjectile(inUnit, inLocation);
    return false;
}

void EEUnitClass::LoadMedia(EEPlayer* inPlayer)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_BUILDING_PRIEST)
        this->BasicPriestLoadMedia(inPlayer);
    else if (theKinds & EE_CLASS_KIND_BASIC_PRIEST)
        this->BasicPriestLoadMedia(inPlayer);
    else
        this->BaseLoadMedia(inPlayer);
}

void EEUnitClass::LocationToTile(const GE3DPoint& inLocation, long& outX, long& outY)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & (EE_CLASS_KIND_NAVAL_BUILDING | EE_CLASS_KIND_NAVAL_CITIZEN | EE_CLASS_KIND_NAVAL_TRANSPORT | EE_CLASS_KIND_NAVAL))
        this->NavalCitizenLocationToTile(inLocation, outX, outY);
    else if (theKinds & EE_CLASS_KIND_SPACE_BUILDING)
        this->SpaceBuildingLocationToTile(inLocation, outX, outY);
    else if (theKinds & EE_CLASS_KIND_AMBIENT)
        this->AmbientLocationToTile(inLocation, outX, outY);
    else if (theKinds & EE_CLASS_KIND_SUBMERGED_NAVAL)
        this->SubmergedNavalLocationToTile(inLocation, outX, outY);
}

void EEUnitClass::RemoveFromRegion(EEUnit* inUnit)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & (EE_CLASS_KIND_FORTRESS | EE_CLASS_KIND_BUILDING))
        EERemoveBuildingRegion(inUnit);
    else if (theKinds & (EE_CLASS_KIND_MISSILE | EE_CLASS_KIND_PROJECTILE)) {
    } else
        this->BaseRemoveFromRegion(inUnit);
}

void EEUnitClass::RemoveFromTile(EEUnit* inUnit)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & (EE_CLASS_KIND_FORTRESS | EE_CLASS_KIND_BUILDING))
        EERemoveFromFootprintTiles(inUnit);
    else if (theKinds & (EE_CLASS_KIND_HELICOPTER_PRIEST | EE_CLASS_KIND_HELICOPTER_TRANSPORT | EE_CLASS_KIND_NAVAL_TRANSPORT | EE_CLASS_KIND_SPACE_TRANSPORT | EE_CLASS_KIND_HELICOPTER | EE_CLASS_KIND_LANDABLE | EE_CLASS_KIND_NAVAL | EE_CLASS_KIND_SPACESHIP | EE_CLASS_KIND_SUBMERGED_NAVAL))
        this->SpaceTransportRemoveFromTile(inUnit);
    else if (theKinds & (EE_CLASS_KIND_AMBIENT | EE_CLASS_KIND_RESOURCE))
        this->AmbientRemoveFromTile(inUnit);
    else
        this->BaseRemoveFromTile(inUnit);
}

bool EEUnitClass::SelectUnitToRelease(EEComplexUnit* inUnit)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & (EE_CLASS_KIND_NAVAL_BUILDING | EE_CLASS_KIND_SPACE_BUILDING))
        return this->NavalBuildingSelectUnitToRelease(inUnit);
    if (theKinds & EE_CLASS_KIND_BUILDING)
        return this->BuildingSelectUnitToRelease(inUnit);
    return false;
}

void EEUnitClass::SimpleUnitCreated(EEUnit* inUnit)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_RESOURCE)
        this->ResourceSimpleUnitCreated(inUnit);
    else
        this->BaseSimpleUnitCreated(inUnit);
}

void EEUnitClass::UnitCreated(EEComplexUnit* inUnit)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_AIR_ANIMAL)
        this->AirAnimalUnitCreated(inUnit);
    else if (theKinds & EE_CLASS_KIND_FORTRESS)
        this->FortressUnitCreated(inUnit);
    else if (theKinds & EE_CLASS_KIND_GATE)
        this->GateUnitCreated(inUnit);
    else if (theKinds & (EE_CLASS_KIND_HELICOPTER_PRIEST | EE_CLASS_KIND_HELICOPTER | EE_CLASS_KIND_SPACESHIP))
        this->HelicopterPriestUnitCreated(inUnit);
    else if (theKinds & EE_CLASS_KIND_HELICOPTER_TRANSPORT)
        this->HelicopterTransportUnitCreated(inUnit);
    else if (theKinds & (EE_CLASS_KIND_NAVAL_BUILDING | EE_CLASS_KIND_SPACE_BUILDING))
        this->SpaceBuildingUnitCreated(inUnit);
    else if (theKinds & EE_CLASS_KIND_SPACE_TRANSPORT)
        this->SpaceTransportUnitCreated(inUnit);
    else if (theKinds & EE_CLASS_KIND_BUILDING)
        this->BuildingUnitCreated(inUnit);
    else if (theKinds & EE_CLASS_KIND_LAND)
        this->LandUnitCreated(inUnit);
    else if (theKinds & EE_CLASS_KIND_LANDABLE)
        this->LandableUnitCreated(inUnit);
    else if (theKinds & EE_CLASS_KIND_MISSILE)
        this->MissileUnitCreated(inUnit);
    else if (theKinds & EE_CLASS_KIND_NAVAL)
        this->NavalUnitCreated(inUnit);
    else if (theKinds & EE_CLASS_KIND_PROJECTILE)
        this->ProjectileUnitCreated(inUnit);
    else if (theKinds & EE_CLASS_KIND_TRANSPORT)
        this->TransportUnitCreated(inUnit);
    else
        this->BaseUnitCreated(inUnit);
}

void EEUnitClass::UnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_FORTRESS)
        this->FortressUnloadAt(inTransport, inX, inY);
    else if (theKinds & EE_CLASS_KIND_TRANSPORT)
        this->TransportUnloadAt(inTransport, inX, inY);
}

void EEUnitClass::UnloadMedia(EEPlayer* inPlayer)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_BUILDING_PRIEST)
        this->BasicPriestUnloadMedia(inPlayer);
    else if (theKinds & EE_CLASS_KIND_BASIC_PRIEST)
        this->BasicPriestUnloadMedia(inPlayer);
    else
        this->BaseUnloadMedia(inPlayer);
}

void EEUnitClass::Update(EEComplexUnit* inUnit)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_AIR_ANIMAL)
        EEUnitClassUpdate(EE_UNITS_UPDATE_AIR_ANIMAL, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_FORTRESS)
        EEUnitClassUpdate(EE_UNITS_UPDATE_FORTRESS, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_BOMB)
        this->BombUpdate(inUnit);
    else if (theKinds & EE_CLASS_KIND_BUILDING_PRIEST) {
        if (!inUnit->IsFoundation() || inUnit->mHitPoints <= 0 || inUnit->mCurrentPhysicsAction == 2) this->PriestVirtualB8(inUnit);
    } else if (theKinds & EE_CLASS_KIND_GATE)
        EEUnitClassUpdate(EE_UNITS_UPDATE_GATE, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_HELICOPTER_TRANSPORT)
        EEUnitClassUpdate(EE_UNITS_UPDATE_HELICOPTER_TRANSPORT, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_LAND_CITIZEN)
        EEUnitClassUpdate(EE_UNITS_UPDATE_LAND_CITIZEN, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_LAND_TRANSPORT)
        EEUnitClassUpdate(EE_UNITS_UPDATE_LAND_TRANSPORT, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_MINING_BUILDING)
        EEUnitClassUpdate(EE_UNITS_UPDATE_MINING_BUILDING, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_NAVAL_BUILDING)
        EEUnitClassUpdate(EE_UNITS_UPDATE_NAVAL_BUILDING, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_NAVAL_CITIZEN)
        EEUnitClassUpdate(EE_UNITS_UPDATE_NAVAL_CITIZEN, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_SPACE_BUILDING)
        EEUnitClassUpdate(EE_UNITS_UPDATE_SPACE_BUILDING, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_TORPEDO)
        this->TorpedoUpdate(inUnit);
    else if (theKinds & EE_CLASS_KIND_AMBIENT)
        EEUnitClassUpdate(EE_UNITS_UPDATE_AMBIENT, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_BASIC_CITIZEN)
        this->UpdateCitizen(inUnit);
    else if (theKinds & EE_CLASS_KIND_BASIC_PRIEST)
        EEUnitClassUpdate(EE_UNITS_UPDATE_BASIC_PRIEST, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_BUILDING)
        EEUnitClassUpdate(EE_UNITS_UPDATE_BUILDING, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_HELICOPTER)
        EEUnitClassUpdate(EE_UNITS_UPDATE_HELICOPTER, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_LAND)
        EEUnitClassUpdate(EE_UNITS_UPDATE_LAND, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_LANDABLE)
        EEUnitClassUpdate(EE_UNITS_UPDATE_LANDABLE, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_MISSILE)
        this->MissileUpdate(inUnit);
    else if (theKinds & EE_CLASS_KIND_NAVAL)
        EEUnitClassUpdate(EE_UNITS_UPDATE_NAVAL, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_PROJECTILE)
        this->ProjectileUpdate(inUnit);
    else if (theKinds & EE_CLASS_KIND_SPACESHIP)
        EEUnitClassUpdate(EE_UNITS_UPDATE_SPACESHIP, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_SUBMERGED_NAVAL)
        EEUnitClassUpdate(EE_UNITS_UPDATE_SUBMERGED_NAVAL, this, inUnit);
    else if (theKinds & EE_CLASS_KIND_TRANSPORT)
        EEUnitClassUpdate(EE_UNITS_UPDATE_TRANSPORT, this, inUnit);
}

void EEUnitClass::UpdateEffects(EEComplexUnit* inUnit)
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & (EE_CLASS_KIND_AIR_ANIMAL | EE_CLASS_KIND_FORTRESS | EE_CLASS_KIND_HELICOPTER_PRIEST | EE_CLASS_KIND_HELICOPTER_TRANSPORT | EE_CLASS_KIND_SPACE_BUILDING)) {
        // their overrides were empty
    } else if (theKinds & EE_CLASS_KIND_AIRPLANE) {
        if (this->mRippleEffect != NULL && inUnit->mUnitClass->mUnitType == kUnitTypeCatalina && inUnit->mHitPoints != 0 && this->mRippleEffect->mDetailLevel <= gEffectDetail) this->UpdateRipples(inUnit);
    } else if (theKinds & (EE_CLASS_KIND_NAVAL_BUILDING | EE_CLASS_KIND_SEA_ANIMAL | EE_CLASS_KIND_TORPEDO | EE_CLASS_KIND_BASIC_CITIZEN | EE_CLASS_KIND_BASIC_PRIEST | EE_CLASS_KIND_NAVAL | EE_CLASS_KIND_TRANSPORT)) {
        if (this->mRippleEffect != NULL && inUnit->mHitPoints != 0 && (unsigned long)this->mRippleEffect->mDetailLevel <= gEffectDetail) this->UpdateRipples(inUnit);
    } else if (theKinds & EE_CLASS_KIND_BUILDING) {
        if ((this->mUnitType == kUnitTypeDock || this->mUnitType == kUnitTypeNavyYard) && this->mRippleEffect != NULL && inUnit->mHitPoints != 0 && this->mRippleEffect->mDetailLevel <= gEffectDetail) this->UpdateRipples(inUnit);
    } else if (theKinds & EE_CLASS_KIND_LAND) {
        if (this->mRippleEffect != NULL && inUnit->mHitPoints > 0 && this->mRippleEffect->mDetailLevel <= gEffectDetail) this->UpdateRipples(inUnit);
    } else if (theKinds & EE_CLASS_KIND_MISSILE) {
        if (inUnit->mHitPoints > 0 && this->mTrailEffect != NULL && inUnit->mVelocity != 0.0f && this->mTrailEffect->mDetailLevel <= gEffectDetail) this->UpdateTrail(inUnit);
    } else if (theKinds & EE_CLASS_KIND_PROJECTILE)
        this->ProjectileUpdateEffects(inUnit);
}

long EEUnitClass::Virtual70()
{
    uint64_t theKinds = EEUnitClassKinds(this);
    if (theKinds & EE_CLASS_KIND_BUILDING)
        return 1;
    if (theKinds & EE_CLASS_KIND_RESOURCE)
        return 2;
    return 0;
}
