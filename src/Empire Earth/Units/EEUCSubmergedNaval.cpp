// C:\EEX\Empire Earth\Units\EEUCSubmergedNaval.cpp (file name inferred: no __FILE__; the class name). The submarine's
// unit class (EEUCSeaAnimal shares most slots by ICF): 0x21ab66..0x21b163 (after EEUCSpaceTransport.cpp;
// EEUCTorpedo.cpp follows). Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); slots
// +0x04, +0x08, +0x34, +0x30, +0x74, +0x84, +0x88, +0xa0, +0xb8 in address order.
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GETerrainMesh.h"
#include "lle/SESoundManager.h"

#include "ee/EEUnitVectorItem.h"
#include "ee/EEUnitUpdate.h"
#include "rust/ee_units.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEEffectManager.h"
#include "ee/EEUCSubmergedNaval.h"

// 0x23f27e (cdecl): moves a location to the depth a submerged unit may take at its tile (provisional name)
void EEGetSeabedDepth(EEComplexUnit* inUnit, GE3DPoint& ioLocation);

// FUNCTION: EXE 0x61ab66
// Lifts a location that lies below the terrain mesh (the sea floor) up onto it
void EEUCSubmergedNaval::GroundLocation(GE3DPoint& ioLocation)
{
    GETerrainMesh& theMesh = ::gWorld.GetMap()->mTerrainMesh;
    float theGround = theMesh.GetMeshZ(ioLocation.GetX(), ioLocation.GetY());
    if (theGround >= ioLocation.GetZ())
        ioLocation.SetZ(theGround);
}

// FUNCTION: EXE 0x61aba6
// Returns the tile where the view line through a point meets the submarine's cruise depth (or the sea
// floor where that is higher)
void EEUCSubmergedNaval::LocationToTile(const GE3DPoint& inLocation, long& outX, long& outY)
{
    GE3DPoint thePoint;
    GE3DPoint theGround;
    GE3DLine theLine(::gWorld.mLine);
    float theT;
    float theZ = -this->GetCruiseAltitude();
    theLine.SetStart(inLocation);
    EEMap* theMap = ::gWorld.mMap;
    theLine.SolveForZ(theZ, theT, thePoint);
    theMap->GetGroundPoint(thePoint, theGround);
    if (theGround.GetZ() > theZ)
        theLine.SolveForZ(theGround.GetZ(), theT, thePoint);
    outX = UMFloor(thePoint.GetX());
    outY = UMFloor(thePoint.GetY());
}

// FUNCTION: EXE 0x61aca0
// Creates a submarine at a placement kept above the sea floor, then moves it to the depth it may take there
EEUnit* EEUCSubmergedNaval::CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation,
                                         EEUnit* inCreator)
{
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GETransformation theTransformation(inTransformation);
    thePosition.SetX(theTransformation.GetX());
    thePosition.SetY(theTransformation.GetY());
    thePosition.SetZ(theTransformation.GetZ());
    this->GroundLocation(thePosition);
    theTransformation.SetX(thePosition.GetX());
    theTransformation.SetY(thePosition.GetY());
    theTransformation.SetZ(thePosition.GetZ());
    EEUnit* theUnit = EEUnitClass::CreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
    EEGetSeabedDepth(theUnit->mComplexUnit, thePosition);
    theUnit->mComplexUnit->SetLocation(thePosition, true);
    return theUnit;
}

// FUNCTION: EXE 0x61ad2c
// Creates a submarine on a tile (yaw pi) kept above the sea floor, then moves it to the depth it may take there
EEUnit* EEUCSubmergedNaval::CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GETransformation theTransformation(false);
    float theZ = ::gWorld.mMap->mTerrainMesh.GetZ(inX, inY);
    thePosition.SetX((float)inX);
    thePosition.SetY((float)inY);
    thePosition.SetZ(theZ);
    this->GroundLocation(thePosition);
    theTransformation.SetX(thePosition.GetX());
    theTransformation.SetY(thePosition.GetY());
    theTransformation.SetZ(thePosition.GetZ());
    theTransformation.SetOrientationYPR(2.0f * 1.5707964f, 0.0f, 0.0f);
    theTransformation.SetElement(3, 0, 0.0f);
    theTransformation.SetElement(3, 1, 0.0f);
    theTransformation.SetElement(3, 2, 0.0f);
    theTransformation.SetElement(3, 3, 1.0f);
    EEUnit* theUnit = EEUnitClass::CreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
    EEGetSeabedDepth(theUnit->mComplexUnit, thePosition);
    theUnit->mComplexUnit->SetLocation(thePosition, true);
    return theUnit;
}

// FUNCTION: EXE 0x61adff
// On dying, plays the death effect and sound and stops the submarine (physics action 2, death animation,
// path dropped); other actions use the common handling
void EEUCSubmergedNaval::ActionChanged(EEComplexUnit* inUnit)
{
    // New (R5b.4, the thirteenth round): the decision is Rust (ee_units actions.rs), its operations
    // EEUnitUpdate.cpp's host calls
    EEUnitClassActionChanged(EE_UNITS_ACTION_SUBMERGED_NAVAL, this, inUnit);
}

// FUNCTION: EXE 0x61aef9
// Adds the submarine to its tile's unit list
void EEUCSubmergedNaval::AddToTile(EEUnit* inUnit)
{
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = ::gWorld.mMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->mUnits = theTile->AddUnit(theUnit, theTile->mUnits);
}

// FUNCTION: EXE 0x61af45
// Returns whether the submarine reached its waypoint: on the waypoint's tile with more to go, when its collision
// radius covers the distance left; otherwise when that distance is at most 0.05 (always in collision state 8)
bool EEUCSubmergedNaval::HasArrived(EEComplexUnit* inUnit, float inValue)
{
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    if (inUnit->mCollisionState == 8)
        return true;
    unsigned long theCount = inUnit->mWaypoints.size();
    inUnit->GetNextWaypointPoint(thePosition);
    if (inUnit->GetTileX() == UMFloor(thePosition.GetX()) && inUnit->GetTileY() == UMFloor(thePosition.GetY())) {
        if (theCount > 1
            || (inUnit->GetQueueGoals(0).size() > 1 && inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalLocation)) {
            return !(inUnit->mCollisionRadius < inValue);
        }
    }
    return inValue <= 0.05f;
}

// FUNCTION: EXE 0x61b010
// Answers dying-animation-finished once the submarine has sunk below the sea floor; other triggers go to the
// common unit class
long EEUCSubmergedNaval::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GE3DPoint theGround(0.0f, 0.0f, 0.0f);
    if (inEvent != kTriggerDyingAnimationFinished) {
        theResult = EEUnitClass::EvaluateCondition(inUnit, inEvent);
    } else {
        inUnit->GetPosition(thePosition);
        ::gWorld.mMap->GetGroundPoint(thePosition, theGround);
        if (thePosition.GetZ() < theGround.GetZ())
            theResult = 1;
    }
    return theResult;
}

// FUNCTION: EXE 0x61b085
// Runs the submarine's tick (physics, effects, timers, camouflage, animation, sound), fires on its attack frame
// and counts down its rotting time
void EEUCSubmergedNaval::Update(EEComplexUnit* inUnit)
{
    // New (R5b.4 leaf 6, the thirteenth round): the sequence is Rust (ee_units update.rs), the leaves
    // EEUnitUpdate.cpp's host calls
    EEUnitClassUpdate(EE_UNITS_UPDATE_SUBMERGED_NAVAL, this, inUnit);
}

// ICF-FOLDED: EXE 0x61884e
// Thins the priest ship's path to every n-th point and both ends when inSmooth is set (n: its inner collision sphere)
// (no address of its own: the same body as EEUCNavalPriest::AdjustPath, which ICF kept)
void EEUCSubmergedNaval::AdjustPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inSmooth)
{
    std::vector<GE3DPoint> thePath;
    long theCount = ioPath.size();
    long theStep = UMFloatToLong(this->mInnerCollisionSphere);
    if (inSmooth != true)
        return;
    thePath.reserve(ioPath.size());
    // a collision sphere that rounds to 0 keeps every point (it used to divide by zero); as in
    // EEUCNavalCitizen::AdjustPath
    if (theStep == 0)
        theStep = 1;
    for (long i = 0; i < theCount; i++) {
        if (i == theCount - 1 || i == 0 || i % theStep == 0)
            thePath.push_back(ioPath[i]);
    }
    ioPath = thePath;
}

// ICF-FOLDED: EXE 0x60afe9
// Animates the animal like any unit
// (no address of its own: the same body as EEUCAirAnimal::Animate, which ICF kept)
long EEUCSubmergedNaval::Animate(EEComplexUnit* inUnit)
{
    return EEUnitClass::Animate(inUnit);
}

// ICF-FOLDED: EXE 0x418aa9
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EEUCSubmergedNaval::CanAttackMove()
{
    return true;
}

// ICF-FOLDED: EXE 0x61aaf5
// Removes the transport from its own tile only
// (no address of its own: the same body as EEUCSpaceTransport::RemoveFromTile, which ICF kept)
void EEUCSubmergedNaval::RemoveFromTile(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = theMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->RemoveUnit(theUnit);
}

// ICF-FOLDED: EXE 0x60da7a
// Sets up a new priest like any unit
// (no address of its own: the same body as EEUCBasicPriest::UnitCreated, which ICF kept)
void EEUCSubmergedNaval::UnitCreated(EEComplexUnit* inUnit)
{
    EEUnitClass::UnitCreated(inUnit);
}
