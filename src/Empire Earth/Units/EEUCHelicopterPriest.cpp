// C:\EEX\Empire Earth\Units\EEUCHelicopterPriest.cpp (file name inferred: no __FILE__; the class name).
// The priest
// helicopter's unit class: 0x212edf..0x21339d (after EEUCHelicopter.cpp; EEUCHelicopterTransport.cpp follows). One unit
// per EEUC* class in alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's
// factory); slots +0x24, +0x2c, +0x6c (EEUCLandPriest's inline twin is ICF-folded into it), +0x74, +0x78, +0xa0 in
// source order.
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
#include "ee/EEUCSpaceship.h"
#include "ee/EEUCBasicPriest.h"
#include "ee/EEUCHelicopterPriest.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUnitUtilities.h"

// 0x447028: the highest a priest helicopter flies (one .rdata copy per unit: a named const) (provisional name)
static const float kHelicopterPriestCeiling = 12.0f;

// 0x23ca26 (cdecl; EEUCHelicopter.cpp declares it the same way; 0x249560 / 0x2495f5: ee/EEUnitUtilities.h)
// (provisional name)
void EEFindLandingPoint(EEComplexUnit* inUnit, GE3DPoint* ioTarget);

// FUNCTION: EXE 0x612edf
// Returns the height to fly at towards a point: cruising height above the highest tile corner on the way, at least
// the point's height, at most 12
float EEUCHelicopterPriest::GetHeightAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
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
    if (theHeight > kHelicopterPriestCeiling)
        theHeight = kHelicopterPriestCeiling;
    return theHeight;
}

// FUNCTION: EXE 0x6130b3
// Gets the sound parameters with a Doppler pitch; silent while the helicopter is on the map but not seen
void EEUCHelicopterPriest::GetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
                                     float* outPitch)
{
    EEUnitClass::GetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    EEGetDopplerPitch(inUnit, inSound, outVolume, outPan, outPitch);
    if (inUnit->mTileNode != NULL && !EEIsSeen(inUnit->mComplexUnit))
        *outVolume = 0.0f;
}

// FUNCTION: EXE 0x6130fe
// Returns the class's weight as its population cost
long EEUCHelicopterPriest::GetPopulationCost()
{
    return this->mWeight;
}

// FUNCTION: EXE 0x613105
// Handles a priest helicopter's new action: death sound and fall when dying; when preparing to move, paths to the
// goal (an attack's aim point, a calamity's cast point, a heal target); others the priest's way
void EEUCHelicopterPriest::ActionChanged(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          inUnit->mPhysicalModel.GetLocation().GetZ());
    EEUnitGoal* theGoal = inUnit->mGoalState;
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
    case kActionPrepareToMove:
        inUnit->SetCurrentPhysicsAction(6);
        inUnit->DropPath();
        if (theGoal->mKind == kGoalAttackUnit) {
            EEFindDropLocation(inUnit, &theUnit, &theEnemy, NULL);
            EEGetAimPoint(inUnit, &theEnemy, &theTarget);
        } else if (theGoal->mKind == kGoalCastCalamity) {
            theGoal->GetDestination(inUnit, theTarget);
            EEGetHelicopterPriestDestination(inUnit, theTarget);
        } else if (theGoal->mKind == kGoalHealUnit)
            theGoal->GetGoalLocation(theTarget);
        else
            theGoal->GetDestination(inUnit, theTarget);
        if (theGoal->mKind == kGoalExplore)
            theTarget.SetZ(this->mCruiseAltitude);
        EEFindLandingPoint(inUnit, &theTarget);
        this->PathToward(inUnit, thePosition, theTarget, false);
        break;
    case kActionResourcesRotting:
        EEUCBasicPriest::ActionChanged(inUnit);
        return;
    default:
        EEUCBasicPriest::ActionChanged(inUnit);
        return;
    }
    inUnit->SetAnimation(0xd, -1, 0);
}

// FUNCTION: EXE 0x6132c0
// Sets up a new priest helicopter as a priest, then lifts it to its flying height
void EEUCHelicopterPriest::UnitCreated(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    EEUCBasicPriest::UnitCreated(inUnit);
    thePosition.SetX(theTransformation.GetX());
    thePosition.SetY(theTransformation.GetY());
    thePosition.SetZ(theTransformation.GetZ());
    float theZ = this->GetHeightAt(inUnit, thePosition);
    theTransformation.SetZ(theZ);
    inUnit->SetTransformation(theTransformation, true);
    inUnit->SetHeight(theZ);
}

// FUNCTION: EXE 0x613344
// Answers the dying-finished (below its tile's height) and move-prepared (has waypoints) triggers; others the
// priest's way
long EEUCHelicopterPriest::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
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
        theResult = EEUCBasicPriest::EvaluateCondition(inUnit, inEvent);
        break;
    }
    if (theFlag)
        theResult = 1;
    return theResult;
}

// ICF-FOLDED: EXE 0x61aaa9
// Adds the transport to its own tile only
// (no address of its own: the same body as EEUCSpaceTransport::AddToTile, which ICF kept)
void EEUCHelicopterPriest::AddToTile(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = theMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->mUnits = theTile->AddUnit(theUnit, theTile->mUnits);
}

// ICF-FOLDED: EXE 0x418aa9
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EEUCHelicopterPriest::FliesOver()
{
    return true;
}

// ICF-FOLDED: EXE 0x61aaf5
// Removes the transport from its own tile only
// (no address of its own: the same body as EEUCSpaceTransport::RemoveFromTile, which ICF kept)
void EEUCHelicopterPriest::RemoveFromTile(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = theMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->RemoveUnit(theUnit);
}

// ICF-FOLDED: EXE 0x616dd7
// Updates the land priest as a priest
// (no address of its own: the same body as EEUCLandPriest::Update, which ICF kept)
void EEUCHelicopterPriest::Update(EEComplexUnit* inUnit)
{
    EEUCBasicPriest::Update(inUnit);
}

// ICF-FOLDED: EXE 0x557269
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEUCHelicopterPriest::UpdateEffects(class EEComplexUnit *)
{
}
