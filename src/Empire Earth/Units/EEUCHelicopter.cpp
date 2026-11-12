// C:\EEX\Empire Earth\Units\EEUCHelicopter.cpp (file name inferred: no __FILE__; the class name).
// The helicopter's unit
// class: 0x212a6f..0x212edf (after EEUCGate.cpp; EEUCHelicopterPriest.cpp follows). One unit per EEUC* class in
// alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); slots +0x24,
// +0x74, +0xb8 in source order (the others are ICF-folded into twins).
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
#include "ee/EEUnitGoal.h"
#include "ee/EEUCTransport.h"
#include "ee/EEUCLandable.h"
#include "ee/EEUCHelicopter.h"
#include "ee/EEUCSpaceship.h"

// 0x447020: the highest a helicopter flies (one .rdata copy per unit: a named const) (provisional name)
static const float kHelicopterCeiling = 12.0f;

// 0x249560 (cdecl): the point to fly to for the attack on inTarget (EEMap.cpp's range) (provisional name)
void EEGetAimPoint(EEComplexUnit* inUnit, GE3DPoint* inTarget, GE3DPoint* outLocation);
// 0x23ca26 (cdecl): moves ioTarget off the tiles other aircraft hold (provisional name)
void EEFindLandingPoint(EEComplexUnit* inUnit, GE3DPoint* ioTarget);

// FUNCTION: EXE 0x612a6f
// Returns the height to fly at towards a point: cruising height above the highest tile corner on the way, at least
// the point's height, kept between cruising height and 12
float EEUCHelicopter::GetHeightAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
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

// FUNCTION: EXE 0x612c5a
// Handles a helicopter's new action: death sound and fall when dying; when preparing to move, paths to the goal
// (an attack's aim point; exploring at cruising height)
void EEUCHelicopter::ActionChanged(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          inUnit->mPhysicalModel.GetLocation().GetZ());
    GE3DPoint theTarget;
    GE3DPoint theEnemy;
    EEUnit* theUnit;
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionIdle:
        this->HandleActionChanged(inUnit);
        break;
    case kActionDying: {
        EESoundObject* theSound = inUnit->mStats->mDeathSound;
        if (theSound != NULL) {
            float theVolume;
            float thePan;
            float thePitch;
            this->GetSoundParameters(inUnit, theSound, &theVolume, &thePan, &thePitch);
            gSoundManager->Play(theSound->mSampleID, theVolume, thePan, thePitch, theSound->mLooping, NULL);
        }
        inUnit->SetCurrentPhysicsAction(2);
        inUnit->SetAnimation(0xd, -1, 0);
        inUnit->DropPath();
        return;
    }
    case kActionPrepareToMove: {
        EEUnitGoal* theGoal = inUnit->mGoalState;
        inUnit->SetCurrentPhysicsAction(6);
        inUnit->DropPath();
        if (theGoal->mKind == kGoalAttackUnit) {
            EEFindDropLocation(inUnit, &theUnit, &theEnemy, NULL);
            EEGetAimPoint(inUnit, &theEnemy, &theTarget);
        } else
            theGoal->GetDestination(inUnit, theTarget);
        if (theGoal->mKind == kGoalExplore)
            theTarget.SetZ(this->mCruiseAltitude);
        EEFindLandingPoint(inUnit, &theTarget);
        this->PathToward(inUnit, thePosition, theTarget, false);
        break;
    }
    case kActionResourcesRotting:
        this->HandleActionChanged(inUnit);
        return;
    default:
        this->HandleActionChanged(inUnit);
        return;
    }
    inUnit->SetAnimation(0xd, -1, 0);
}

// FUNCTION: EXE 0x612deb
// Advances a helicopter each tick: physics, orbitals, timers, animation and sound, attack frames, rotting, regeneration
void EEUCHelicopter::Update(EEComplexUnit* inUnit)
{
    long theFrame = inUnit->mPhysicalModel.GetFrame();
    inUnit->SetLastHitPoints(inUnit->mHitPoints);
    if (this->mPhysicsModel != NULL && inUnit->mTileX != -10000)
        this->mPhysicsModel->Step(inUnit);
    inUnit->UpdateOrbitals();
    inUnit->UpdateReloadTimer();
    inUnit->UpdateTargetSearchTimer();
    this->UpdateCamouflageTimer(inUnit);
    long theAttack = this->Animate(inUnit);
    this->RefreshSound(inUnit);
    unsigned long theCommand = inUnit->mCommand ? inUnit->mCommand->mType : 0;
    switch (theCommand) {
    case kActionAttackEnemyUnit:
    case kActionAttackLocation:
    case kActionWaitForReload:
        this->FireOnAttackFrame(inUnit, theAttack, theFrame);
        break;
    case kActionUnitRotting: {
        inUnit->UpdateRottingAwayTime();
        break;
    }
    }
    if (inUnit->mStats->mRegeneration > 0)
        this->Regenerate(inUnit);
    this->UpdateDone(inUnit);
}

// ICF-FOLDED: EXE 0x61aaa9
// Adds the transport to its own tile only
// (no address of its own: the same body as EEUCSpaceTransport::AddToTile, which ICF kept)
void EEUCHelicopter::AddToTile(EEUnit* inUnit)
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
long EEUCHelicopter::Animate(EEComplexUnit* inUnit)
{
    return EEUnitClass::Animate(inUnit);
}

// ICF-FOLDED: EXE 0x418aa9
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EEUCHelicopter::CanAttackMove()
{
    return true;
}

// ICF-FOLDED: EXE 0x6137f8
// Answers the dying-finished (below its tile's height) and move-prepared (has waypoints) triggers; others the
// common way
// (no address of its own: the same body as EEUCHelicopterTransport::EvaluateCondition, which ICF kept)
long EEUCHelicopter::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    bool theFlag = false;
    switch (inEvent) {
    case kTriggerDyingAnimationFinished:
        if (inUnit->mPhysicalModel.GetLocation().GetZ() < inUnit->mTile->GetHeight())
            theFlag = true;
        else
            theFlag = false;
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

// ICF-FOLDED: EXE 0x418aa9
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EEUCHelicopter::FliesOver()
{
    return true;
}

// ICF-FOLDED: EXE 0x61a404
// Returns a sound's volume, pan and pitch, with a doppler shift
// (no address of its own: the same body as EEUCSpaceship::GetSoundParameters, which ICF kept)
void EEUCHelicopter::GetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
                              float* outPitch)
{
    EEUnitClass::GetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    EEGetDopplerPitch(inUnit, inSound, outVolume, outPan, outPitch);
}

// ICF-FOLDED: EXE 0x61aaf5
// Removes the transport from its own tile only
// (no address of its own: the same body as EEUCSpaceTransport::RemoveFromTile, which ICF kept)
void EEUCHelicopter::RemoveFromTile(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = theMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->RemoveUnit(theUnit);
}

// ICF-FOLDED: EXE 0x61a51a
// Sets up a new spaceship at its flying height for its position
// (no address of its own: the same body as EEUCSpaceship::UnitCreated, which ICF kept)
void EEUCHelicopter::UnitCreated(EEComplexUnit* inUnit)
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
