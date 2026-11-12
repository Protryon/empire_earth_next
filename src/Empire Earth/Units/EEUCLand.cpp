// C:\EEX\Empire Earth\Units\EEUCLand.cpp (file name inferred: no __FILE__; the class name).
// The land unit class:
// 0x213866..0x213c60 (after EEUCHelicopterTransport.cpp; EEUCLandable.cpp follows). One unit per EEUC* class in
// alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); slots +0x78,
// +0x74, +0xe0 (the paratroopers), +0xbc, +0xb8 in address order, then EEComplexUnit::UpdateWheels's COMDAT after its
// first user VirtualB8.
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"

#include "ee/EEUnitUpdateGolden.h"   // TEST ONLY (round 13's golden)
#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIArea.h"
#include "ee/EEHerd.h"
#include "ee/EEWheel.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGUnloadTransport.h"
#include "ee/EEUCLand.h"
#include "ee/EEUnitUpdate.h"
#include "rust/ee_units.h"

// 0x20b6ff (EEUCAirplane.cpp): the plane's flight for inPlayer (provisional name)
void EECreateCarrierAircraft(EEComplexUnit* inUnit, long inPlayer);
// 0xde4a9 (EEE2DBattlefieldAmbient.cpp's range): the unit's battlefield ambience (provisional name)
void EEPlaceTrackDecal(EEComplexUnit* inUnit);

// FUNCTION: EXE 0x613866
// Sets up a new land unit: its class's resource (and amount when it has one) and, for the radio man, its power
void EEUCLand::UnitCreated(EEComplexUnit* inUnit)
{
    EEUnitClass::UnitCreated(inUnit);
    inUnit->SetResourceType(this->mResourceType);
    if (inUnit->mResourceType != kResourceNone)
        inUnit->SetResourceAmount(this->mResourceAmount);
    if (inUnit->mUnitClass->mUnitType == kUnitTypeInf10RadioMan)
        inUnit->SetPower(inUnit->mUnitClass->mPower);
}

// FUNCTION: EXE 0x6138c0
// Handles a land unit's new action: a radio man's special order calls the paratroop plane, a rotting body leaves
// its blood stain; then the common handling
void EEUCLand::ActionChanged(EEComplexUnit* inUnit)
{
    EEUnitActionScope theGoldenScope(inUnit->mUnitID);   // TEST ONLY (round 13's golden)
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionSpecialLocation:
        if (this->mUnitType == kUnitTypeInf10RadioMan) {
            (EEACNA(SET_PHYSICS_ACTION, 1, 0), inUnit->SetCurrentPhysicsAction(1));
            (EEACNA(SET_ANIMATION, 0xb, 0), inUnit->SetAnimation(0xb, -1, 0));
            (EEACN(CALL_PARATROOP_PLANE), this->CallParatroopPlane(inUnit));
        }
        break;
    case kActionResourcesRotting:
        if (inUnit->mUnitClass->mBloodStainEffect != NULL)
            (EEACN(PLACE_TRACK_DECAL), EEPlaceTrackDecal(inUnit));
        break;
    }
    this->HandleActionChanged(inUnit);
}

// FUNCTION: EXE 0x61393c
// Calls a paratroop plane (class 0x2e1): it appears over the last map tile beyond the unit in +y and flies to the
// unit's goal location to unload its cargo
void EEUCLand::CallParatroopPlane(EEComplexUnit* inUnit)
{
    EEUnitClass* thePlaneClass = gUnitClassTable[kUnitTypeParatrooperPlane];
    GETransformation theTransformation;
    long theX = inUnit->mTileX;
    long theY = inUnit->mTileY;
    while (::gWorld.GetMap()->GetTile(theX, theY) != NULL)
        theY++;
    theTransformation.SetX((float)theX);
    theTransformation.SetY((float)(theY - 1));
    theTransformation.SetZ(thePlaneClass->mCruiseAltitude);
    theTransformation.SetOrientationYPR(3.1415927f, 0.0f, 0.0f);
    EEComplexUnit* thePlane = thePlaneClass->CreateUnitAt(theTransformation, 0, 0, inUnit)->mComplexUnit;
    EECreateCarrierAircraft(thePlane, inUnit->mPlayer->mPlayerID);
    thePlane->mOrientation = GEOrientation(3.1415927f, 0.0f, 0.0f);
    GE3DPoint theTarget;
    inUnit->mGoalState->GetGoalLocation(theTarget);
    thePlane->QueueGoal(new EEUGUnloadTransport(thePlane, (long)theTarget.GetX(), (long)theTarget.GetY(), false), 0);
    thePlane->SetFlightTime(1000000);
    GE3DPoint theStart(theTransformation);
    thePlane->mUnitClass->PathToward(thePlane, theStart, theTarget, true);
    thePlane->StartAction(kActionFly);
    thePlane->mUnitClass->ActionChanged(thePlane);
}

// FUNCTION: EXE 0x613a94
// Updates the unit's ripples while it is alive and the effect's detail level is shown
void EEUCLand::UpdateEffects(EEComplexUnit* inUnit)
{
    if (this->mRippleEffect != NULL && inUnit->mHitPoints > 0 && this->mRippleEffect->mDetailLevel <= gEffectDetail)
        this->UpdateRipples(inUnit);
}

// FUNCTION: EXE 0x613abf
// Advances a land unit each tick (physics, turrets, wheels, timers, animation, power, attack frames, rotting, medic
// healing, regeneration); herd members, nature's units and map animals update as animals
void EEUCLand::Update(EEComplexUnit* inUnit)
{
    // New (R5b.4 leaf 6): the sequence is Rust (ee_units update.rs land), the leaves this host's calls
    EEUnitClassUpdate(EE_UNITS_UPDATE_LAND, this, inUnit);
}

// FUNCTION: EXE 0x613c37
// Updates each of the unit's wheels
void EEComplexUnit::UpdateWheels()
{
    unsigned long theCount = this->mNumWheels;
    for (unsigned long i = 0; i < theCount; i++)
        this->mWheels[i]->Update(this);
}

// ICF-FOLDED: EXE 0x418aa9
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EEUCLand::CanAttackMove()
{
    return true;
}
