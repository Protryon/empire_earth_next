// C:\EEX\Empire Earth\Units\EEUCSpaceship.cpp (file name inferred: no __FILE__; the class name). The spaceship's unit
// class: 0x21a219..0x21a6e0 (after EEUCSpaceBuilding.cpp; EEUCSpaceTransport.cpp's +0xe4 0x21a6e0 follows). Unit
// flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); slots +0x24, +0x2c, +0x74, +0x78,
// +0xa0, +0xb8 in source order.
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
#include "lle/SESoundManager.h"
#include "lle/U2DPoint.h"

#include "ee/EEUnitUpdateGolden.h"   // TEST ONLY (round 13's golden)
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEEffectManager.h"
#include "ee/EEUCTransport.h"
#include "ee/EEUCSpaceship.h"

// 0x447358: the highest a spaceship flies over the terrain (one .rdata copy per unit: a named const)
static const float kSpaceshipHeight = 12.0f;

// FUNCTION: EXE 0x61a219
// Returns the flying height for a location: its z raised to clear every tile corner on the way by the cruise
// altitude, kept between the cruise altitude and 12
// The floor clamp is the class's mCruiseAltitude offset (the original keeps it on the FPU after the loop)
float EEUCSpaceship::GetHeightAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
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

// FUNCTION: EXE 0x61a404
// Returns a sound's volume, pan and pitch, with a doppler shift
void EEUCSpaceship::GetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
                              float* outPitch)
{
    EEUnitClass::GetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    EEGetDopplerPitch(inUnit, inSound, outVolume, outPan, outPitch);
}

// FUNCTION: EXE 0x61a436
// Handles a spaceship's new action: idle restarts its flight animation; dying plays its sound and death effect
void EEUCSpaceship::ActionChanged(EEComplexUnit* inUnit)
{
    EEUnitActionScope theGoldenScope(inUnit->mUnitID);   // TEST ONLY (round 13's golden)
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionIdle:
        this->HandleActionChanged(inUnit);
        (EEACNA(SET_ANIMATION, 0xd, 0), inUnit->SetAnimation(0xd, -1, 0));
        break;
    case kActionDying: {
        EESoundObject* theSound = inUnit->mStats->mDeathSound;
        if (theSound != NULL) {
            float theVolume;
            float thePan;
            float thePitch;
            (EEACN(DEATH_SOUND), this->GetSoundParameters(inUnit, theSound, &theVolume, &thePan, &thePitch));
            gSoundManager->Play(theSound->mSampleID, theVolume, thePan, thePitch, theSound->mLooping, NULL);
        }
        if (this->mDeathEffect != NULL)
            (EEACN(DEATH_EFFECT), gEffectManager->CreateUnitEffect(inUnit, this->mDeathEffect, NULL, 0, 0, 0));
        (EEACNA(SET_PHYSICS_ACTION, 2, 0), inUnit->SetCurrentPhysicsAction(2));
        (EEACNA(SET_ANIMATION, 0xd, 0), inUnit->SetAnimation(0xd, -1, 0));
        (EEACN(DROP_PATH), inUnit->DropPath());
        break;
    }
    case kActionResourcesRotting:
        this->HandleActionChanged(inUnit);
        break;
    default:
        this->HandleActionChanged(inUnit);
        break;
    }
}

// FUNCTION: EXE 0x61a51a
// Sets up a new spaceship at its flying height for its position
void EEUCSpaceship::UnitCreated(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    EEUnitClass::UnitCreated(inUnit);
    thePosition.SetX(theTransformation.GetX());
    thePosition.SetY(theTransformation.GetY());
    thePosition.SetZ(theTransformation.GetZ());
    float theZ = this->GetHeightAt(inUnit, thePosition);
    theTransformation.SetZ(theZ);
    inUnit->SetTransformation(theTransformation, true);
    inUnit->SetHeight(theZ);
}

// FUNCTION: EXE 0x61a59e
// Answers dying done (always) and move preparation complete (has a path); others to the base class
long EEUCSpaceship::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
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
        theResult = EEUnitClass::EvaluateCondition(inUnit, inEvent);
        break;
    }
    if (theFlag)
        theResult = 1;
    return theResult;
}

// FUNCTION: EXE 0x61a5dd
// Updates a spaceship each tick: physics, turrets, timers, animation, sound, power, firing, rotting, regeneration
void EEUCSpaceship::Update(EEComplexUnit* inUnit)
{
    long theFrame = inUnit->mPhysicalModel.GetFrame();
    (EEUN(LAST_HIT_POINTS), inUnit->SetLastHitPoints(inUnit->mHitPoints));
    if (this->mPhysicsModel != NULL && inUnit->mTileX != -10000) {
        (EEUN(PHYSICS_STEP), this->mPhysicsModel->Step(inUnit));
        (EEUN(TURRETS), inUnit->UpdateTurrets());
    }
    (EEUN(ORBITALS), inUnit->UpdateOrbitals());
    (EEUN(RELOAD_TIMER), inUnit->UpdateReloadTimer());
    (EEUN(TARGET_SEARCH_TIMER), inUnit->UpdateTargetSearchTimer());
    (EEUN(CAMOUFLAGE_TIMER), this->UpdateCamouflageTimer(inUnit));
    long theAttack = (EEUN(ANIMATE), this->Animate(inUnit));
    (EEUN(REFRESH_SOUND), this->RefreshSound(inUnit));
    (EEUN(REGENERATE_POWER), this->RegeneratePower(inUnit));
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionAttackEnemyUnit:
    case kActionAttackLocation:
    case kActionWaitForReload:
        (EEUNA(FIRE_ON_ATTACK_FRAME, theAttack, theFrame), this->FireOnAttackFrame(inUnit, theAttack, theFrame));
        break;
    case kActionUnitRotting: {
        (EEUN(ROTTING), inUnit->UpdateRottingAwayTime());
        break;
    }
    }
    if (inUnit->mStats->mRegeneration > 0)
        (EEUN(REGENERATE), this->Regenerate(inUnit));
    (EEUN(DONE), this->UpdateDone(inUnit));
}

// ICF-FOLDED: EXE 0x61aaa9
// Adds the transport to its own tile only
// (no address of its own: the same body as EEUCSpaceTransport::AddToTile, which ICF kept)
void EEUCSpaceship::AddToTile(EEUnit* inUnit)
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
long EEUCSpaceship::Animate(EEComplexUnit* inUnit)
{
    return EEUnitClass::Animate(inUnit);
}

// ICF-FOLDED: EXE 0x418aa9
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EEUCSpaceship::CanAttackMove()
{
    return true;
}

// ICF-FOLDED: EXE 0x61aaf5
// Removes the transport from its own tile only
// (no address of its own: the same body as EEUCSpaceTransport::RemoveFromTile, which ICF kept)
void EEUCSpaceship::RemoveFromTile(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = theMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->RemoveUnit(theUnit);
}
