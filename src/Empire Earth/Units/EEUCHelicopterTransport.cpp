// C:\EEX\Empire Earth\Units\EEUCHelicopterTransport.cpp (file name inferred: no __FILE__; the class name).
// The transport
// helicopter's unit class: 0x21339d..0x213866 (after EEUCHelicopterPriest.cpp; EEUCLand.cpp follows). One unit per
// EEUC* class in alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's
// factory); slots +0xe4, +0x24, +0xf0, +0xf8, +0x74, +0x78, +0xa0, +0xb8 in address order.
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
#include "ee/EEUCHelicopterTransport.h"
#include "ee/EEUCSpaceship.h"

// 0x447030: the highest a transport helicopter flies (one .rdata copy per unit: a named const) (provisional name)
static const float kHelicopterTransportCeiling = 12.0f;

// 0x23ca26 (cdecl; EEUCHelicopter.cpp declares it the same way) (provisional name)
void EEFindLandingPoint(EEComplexUnit* inUnit, GE3DPoint* ioTarget);

// FUNCTION: EXE 0x61339d
// Picks the gathering tile near the transport's own tile; the units board there too
void EEUCHelicopterTransport::FindRendezvousTiles(std::vector<EEComplexUnit*>* inUnits, EEComplexUnit* inTransport,
                                        U2DPoint<long>* outTransportTile, U2DPoint<long>* outUnitTile)
{
    U2DPoint<long> theFrom(inTransport->mTileX, inTransport->mTileY);
    this->FindGatheringTile(inUnits, theFrom, outTransportTile);
    *outUnitTile = *outTransportTile;
}

// FUNCTION: EXE 0x6133e8
// Returns the height to fly at towards a point: cruising height above the highest tile corner on the way, at least
// the point's height, at most 12
float EEUCHelicopterTransport::GetHeightAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
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
    if (theHeight > kHelicopterTransportCeiling)
        theHeight = kHelicopterTransportCeiling;
    return theHeight;
}

// FUNCTION: EXE 0x6135bc  (ICF-folded with EECCCloaking::Virtual4C; this unit owns the address)
// Returns true: a transport helicopter always has room
bool EEUCHelicopterTransport::HasRoom(EEComplexUnit* inUnit, EEComplexUnit* inCargo)
{
    return true;
}

// FUNCTION: EXE 0x6135c1
// Returns whether the transport can unload on a tile: not deep water (1) or cliff (9), and the common test allows it
bool EEUCHelicopterTransport::CanUnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    EEMapTile* theTile = ::gWorld.mMap->GetTile(inX, inY);
    bool theResult = false;
    if (theTile != NULL) {
        if (theTile->mTerrainType != kTerrainDeepWater && theTile->mTerrainType != kTerrainCliff &&
            EEUCTransport::CanUnloadAt(inTransport, inX, inY))
            theResult = true;
        else
            theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x61361f
// Handles a transport helicopter's new action: death sound and fall when dying; when preparing to move, paths
// to the goal (exploring at cruising height)
void EEUCHelicopterTransport::ActionChanged(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          inUnit->mPhysicalModel.GetLocation().GetZ());
    EEUnitGoal* theGoal = inUnit->mGoalState;
    GE3DPoint theTarget;
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
        theGoal->GetDestination(inUnit, theTarget);
        if (theGoal->mKind == kGoalExplore)
            theTarget.SetZ(this->mCruiseAltitude);
        EEFindLandingPoint(inUnit, &theTarget);
        this->PathToward(inUnit, thePosition, theTarget, false);
        break;
    case kActionResourcesRotting:
        this->HandleActionChanged(inUnit);
        return;
    default:
        this->HandleActionChanged(inUnit);
        return;
    }
    inUnit->SetAnimation(0xd, -1, 0);
}

// FUNCTION: EXE 0x613774
// Sets up a new transport helicopter as a transport, then lifts it to its flying height
void EEUCHelicopterTransport::UnitCreated(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    EEUCTransport::UnitCreated(inUnit);
    thePosition.SetX(theTransformation.GetX());
    thePosition.SetY(theTransformation.GetY());
    thePosition.SetZ(theTransformation.GetZ());
    float theZ = this->GetHeightAt(inUnit, thePosition);
    theTransformation.SetZ(theZ);
    inUnit->SetTransformation(theTransformation, true);
    inUnit->SetHeight(theZ);
}

// FUNCTION: EXE 0x6137f8
// Answers the dying-finished (below its tile's height) and move-prepared (has waypoints) triggers; others the
// common way
long EEUCHelicopterTransport::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
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

// FUNCTION: EXE 0x613851
// Updates the transport the common way, then its orbitals
void EEUCHelicopterTransport::Update(EEComplexUnit* inUnit)
{
    EEUCTransport::Update(inUnit);
    inUnit->UpdateOrbitals();
}

// ICF-FOLDED: EXE 0x61aaa9
// Adds the transport to its own tile only
// (no address of its own: the same body as EEUCSpaceTransport::AddToTile, which ICF kept)
void EEUCHelicopterTransport::AddToTile(EEUnit* inUnit)
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
long EEUCHelicopterTransport::Animate(EEComplexUnit* inUnit)
{
    return EEUnitClass::Animate(inUnit);
}

// ICF-FOLDED: EXE 0x418aa9
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EEUCHelicopterTransport::CanAttackMove()
{
    return true;
}

// ICF-FOLDED: EXE 0x618d33
// Finds where a unit boards the transport (the base transport's rule)
// (no address of its own: the same body as EEUCNavalTransport::FindBoardingLocation, which ICF kept)
bool EEUCHelicopterTransport::FindBoardingLocation(EEComplexUnit* inUnit, EEComplexUnit* inTransport, GE3DPoint& inLocation,
                                   GE3DPoint& outLocation)
{
    return EEUCTransport::FindBoardingLocation(inUnit, inTransport, inLocation, outLocation);
}

// ICF-FOLDED: EXE 0x61a404
// Returns a sound's volume, pan and pitch, with a doppler shift
// (no address of its own: the same body as EEUCSpaceship::GetSoundParameters, which ICF kept)
void EEUCHelicopterTransport::GetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
                              float* outPitch)
{
    EEUnitClass::GetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    EEGetDopplerPitch(inUnit, inSound, outVolume, outPan, outPitch);
}

// ICF-FOLDED: EXE 0x61aaf5
// Removes the transport from its own tile only
// (no address of its own: the same body as EEUCSpaceTransport::RemoveFromTile, which ICF kept)
void EEUCHelicopterTransport::RemoveFromTile(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = theMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->RemoveUnit(theUnit);
}

// ICF-FOLDED: EXE 0x557269
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEUCHelicopterTransport::UpdateEffects(class EEComplexUnit *)
{
}
