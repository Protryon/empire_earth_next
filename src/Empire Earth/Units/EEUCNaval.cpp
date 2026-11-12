// C:\EEX\Empire Earth\Units\EEUCNaval.cpp (file name inferred: no __FILE__; the class name). The warships' unit class:
// 0x217c6d..0x218158 (after EEUCMissile.cpp; EEUCNavalBuilding.cpp follows), slots +0x0c, +0x74, +0x78, +0x88, +0xa0
// and +0xb8 of ee/EEUCNaval.h plus the helper UpdateResourceUnit. The ctor is inline (EEUnitClass.cpp's factory). Unit flags:
// the exe's module set.
// Virtual0C 0x217c6d forwards (inUnit, ioPath, inArg) to the cdecl 0x23df77 (declared here).
#include <windows.h>
#include <stdio.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEBoundingBox.h"
#include "lle/GEModel.h"
#include "lle/SESoundManager.h"

#include "ee/EEUnitUpdateGolden.h"   // TEST ONLY (round 13's golden)
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEUnitAction.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEEffectManager.h"
#include "ee/EEUCNaval.h"
#include "ee/EEUnitUpdate.h"
#include "rust/ee_units.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEMap.h"
#include "ee/EmpireEarth.h"

// 0x23df77 (cdecl, void; in EEFOFindNewTargetNearOldTarget.cpp's range): the naval path smoothing the naval classes'
// Virtual0C forward to; no header declares it yet (provisional name)
void EEAdjustNavalPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inArg);

// FUNCTION: EXE 0x617c6d
// Smooths a ship's path with the naval path smoothing (0x23df77)
void EEUCNaval::AdjustPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inSmooth)
{
    EEAdjustNavalPath(inUnit, ioPath, inSmooth);
}

// FUNCTION: EXE 0x617c84
// Starts a ship sinking: the class's death effect and sound, then the sinking state; other actions go to the base
void EEUCNaval::ActionChanged(EEComplexUnit* inUnit)
{
    EEUnitActionScope theGoldenScope(inUnit->mUnitID);   // TEST ONLY (round 13's golden)
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) != kActionDying) {
        this->HandleActionChanged(inUnit);
        return;
    }
    if (this->mDeathEffect != NULL)
        (EEACN(DEATH_EFFECT), gEffectManager->CreateUnitEffect(inUnit, this->mDeathEffect, NULL, 0, 0, 0));
    EESoundObject* theSound = inUnit->mStats->mDeathSound;
    if (theSound != NULL) {
        float theVolume;
        float thePan;
        float thePitch;
        (EEACN(DEATH_SOUND), this->GetSoundParameters(inUnit, theSound, &theVolume, &thePan, &thePitch));
        gSoundManager->Play(theSound->mSampleID, theVolume, thePan, thePitch, theSound->mLooping, NULL);
    }
    (EEACNA(SET_PHYSICS_ACTION, 2, 0), inUnit->SetCurrentPhysicsAction(2));
    (EEACN(STOP), inUnit->mAngularVelocity = GEOrientation(0.0f, 0.0f, 0.0f));
    inUnit->SetVelocity(0.0f);
    (EEACNA(SET_ANIMATION, 4, 0), inUnit->SetAnimation(4, -1, 0));
    (EEACN(DROP_PATH), inUnit->DropPath());
}

// FUNCTION: EXE 0x617d6a
// Sets up a new ship at sea level (z 0), with the class's resource type and amount
void EEUCNaval::UnitCreated(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    EEUnitClass::UnitCreated(inUnit);
    theTransformation.SetX(theTransformation.GetX());
    theTransformation.SetZ(0.0f);
    inUnit->SetTransformation(theTransformation, true);
    inUnit->SetResourceType(this->mResourceType);
    if (inUnit->mResourceType != kResourceNone)
        inUnit->SetResourceAmount(this->mResourceAmount);
}

// FUNCTION: EXE 0x617dcf
// Returns whether the ship may stop: already sinking, a slow request, or at the path's end tile with its speed reached
bool EEUCNaval::HasArrived(EEComplexUnit* inUnit, float inValue)
{
    GE3DPoint thePosition;
    bool theResult = false;
    if (inUnit->mCollisionState == 8 || inValue <= 0.05f)
        theResult = true;
    else {
        unsigned long theCount = inUnit->mWaypoints.size();
        inUnit->GetNextWaypointPoint(thePosition);
        thePosition.SetZ(0.0f);
        if (inUnit->GetTileX() == UMFloor(thePosition.GetX()) && inUnit->GetTileY() == UMFloor(thePosition.GetY())
            && (theCount > 1 || inUnit->mCollisionRadius >= 2.0f
                || (inUnit->GetQueueGoals(0).size() > 1 && inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalLocation))) {
            if (inUnit->mCollisionRadius < inValue)
                theResult = false;
            else
                theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x617eb2
// Answers dying done: true once the ship's transformed bounding box lies wholly below the water line
long EEUCNaval::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    GE3DPoint thePosition;
    GEBoundingBox theBox;
    GEBoundingBox theTransformedBox;
    EEComplexUnit* theUnit = inUnit;
    const GEPhysicalModel& thePhysicalModel = theUnit->mPhysicalModel;
    GEModel* theModel = thePhysicalModel.GetModel();
    long theResult = 0;
    if (inEvent != kTriggerDyingAnimationFinished)
        theResult = EEUnitClass::EvaluateCondition(theUnit, inEvent);
    else {
        theUnit->GetPosition(thePosition);
        GETransformation theTransformation(false);
        thePhysicalModel.GetLocation().Apply(theModel->GetInternalTransform(thePhysicalModel.GetFrame()),
                                             theTransformation);
        float theScale = thePhysicalModel.GetScale();
        theTransformation.Scale(theScale, theScale, theScale);
        theTransformation.SetX(thePosition.GetX());
        theTransformation.SetY(thePosition.GetY());
        theTransformation.SetZ(thePosition.GetZ());
        theBox = theModel->GetBoundingBox(theUnit->mPhysicalModel.GetFrame());
        theBox.Transform(theTransformation, theTransformedBox);
        if (theTransformedBox.GetTopRightBack().GetZ() < 0.0f)
            theResult = 1;
    }
    return theResult;
}

// FUNCTION: EXE 0x617fb5
// Updates a fish resource: while its resources rot it loses one every 4 s; a rotting unit counts its rotting time down
void EEUCNaval::UpdateResourceUnit(EEComplexUnit* inUnit)
{
    inUnit->UpdateOrbitals();
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionResourcesRotting:
        inUnit->UpdateResourcesRotting();
        break;
    case kActionUnitRotting: {
        inUnit->UpdateRottingAwayTime();
        break;
    }
    }
}

// FUNCTION: EXE 0x618055
// Updates a ship each tick (fish as a resource): physics, turrets, effects, timers, animation, firing on attack frames
void EEUCNaval::Update(EEComplexUnit* inUnit)
{
    // New (R5b.4 leaf 6): the sequence is Rust (ee_units update.rs naval), the leaves EEUnitUpdate.cpp's host calls
    EEUnitClassUpdate(EE_UNITS_UPDATE_NAVAL, this, inUnit);
}

// ICF-FOLDED: EXE 0x61aaa9
// Adds the transport to its own tile only
// (no address of its own: the same body as EEUCSpaceTransport::AddToTile, which ICF kept)
void EEUCNaval::AddToTile(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = theMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->mUnits = theTile->AddUnit(theUnit, theTile->mUnits);
}

// ICF-FOLDED: EXE 0x418aa9
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EEUCNaval::CanAttackMove()
{
    return true;
}

// ICF-FOLDED: EXE 0x618605
// Creates a fishing boat on a tile at sea level
// (no address of its own: the same body as EEUCNavalCitizen::CreateUnit, which ICF kept)
EEUnit* EEUCNaval::CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    GETransformation theTransformation;
    theTransformation.SetX((float)inX);
    theTransformation.SetY((float)inY);
    theTransformation.SetZ(0.0f);
    return EEUnitClass::CreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// ICF-FOLDED: EXE 0x618909
// Creates a priest ship at a placement on the water plane (z 0)
// (no address of its own: the same body as EEUCNavalPriest::CreateUnitAt, which ICF kept)
EEUnit* EEUCNaval::CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation,
                                      EEUnit* inCreator)
{
    GETransformation theTransformation(inTransformation);
    theTransformation.SetX(theTransformation.GetX());
    theTransformation.SetZ(0.0f);
    return EEUnitClass::CreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// ICF-FOLDED: EXE 0x618a45
// Raises a location below the water plane to z 0
// (no address of its own: the same body as EEUCNavalTransport::GroundLocation, which ICF kept)
void EEUCNaval::GroundLocation(GE3DPoint& ioLocation)
{
    if (ioLocation.GetZ() < 0.0f)
        ioLocation.SetZ(0.0f);
}

// ICF-FOLDED: EXE 0x618490
// Returns the tile under a location along the camera line, at sea level
// (no address of its own: the same body as EEUCNavalCitizen::LocationToTile, which ICF kept)
void EEUCNaval::LocationToTile(const GE3DPoint& inLocation, long& outX, long& outY)
{
    GE3DPoint thePoint;
    GE3DLine theLine(::gWorld.mLine);
    theLine.SetStart(inLocation);
    float theT;
    theLine.SolveForZ(0.0f, theT, thePoint);
    outX = UMFloor(thePoint.GetX());
    outY = UMFloor(thePoint.GetY());
}

// ICF-FOLDED: EXE 0x61aaf5
// Removes the transport from its own tile only
// (no address of its own: the same body as EEUCSpaceTransport::RemoveFromTile, which ICF kept)
void EEUCNaval::RemoveFromTile(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = theMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->RemoveUnit(theUnit);
}

// ICF-FOLDED: EXE 0x60d336
// Updates a living citizen's water ripples when the effect detail allows them
// (no address of its own: the same body as EEUCBasicCitizen::UpdateEffects, which ICF kept)
void EEUCNaval::UpdateEffects(EEComplexUnit* inUnit)
{
    if (this->mRippleEffect != NULL && inUnit->mHitPoints != 0 && (unsigned long)this->mRippleEffect->mDetailLevel <= gEffectDetail)
        this->UpdateRipples(inUnit);
}
