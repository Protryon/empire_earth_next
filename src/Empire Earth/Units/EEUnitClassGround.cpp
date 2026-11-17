// EEUnitClassGround.cpp (new, R5b.4 round 32): the ambient, resource, land and naval kinds' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
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
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/GEBoundingBox.h"
#include "lle/GEModel.h"
#include "lle/SESoundManager.h"
#include "lle/GE3DLine.h"

#include "ee/EEUnitUpdate.h"
#include "rust/sim/units.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIArea.h"
#include "ee/EEHerd.h"
#include "ee/EEWheel.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUnitAction.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEEffectManager.h"

// ---- EEUCAmbient.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The ambient object's
// unit class: 0x20b9fc..0x20bcf7 (after EEUCAirplane.cpp; EEUCBasicCitizen.cpp follows). Unit flags: the exe's module
// set. Its ctor is inline (EEUnitClass.cpp's factory); slots +0x04, +0x08, +0x64, +0x7c, +0x84, +0x98, +0xb8 in
// source order (unit type 0x234 sits on the water plane).

// For cattails, returns the tile where the view line through a point meets the water plane; otherwise sets nothing
void EEUnitClass::AmbientLocationToTile(const GE3DPoint& inLocation, long& outX, long& outY)
{
    GE3DPoint thePoint;
    GE3DLine theLine(::gWorld.mLine);
    if (this->mUnitType != kUnitTypeAmbCattails1)
        return;
    theLine.SetStart(inLocation);
    float theT;
    theLine.SolveForZ(0.0f, theT, thePoint);
    outX = UMFloor(thePoint.GetX());
    outY = UMFloor(thePoint.GetY());
}

// Adds the unit to every map tile of its footprint, its own tile first
void EEUnitClass::AmbientAddToTile(EEUnit* inUnit)
{
    long theSize = this->GetTileSize();
    EEMap* theMap = ::gWorld.mMap;
    long theX = inUnit->mTileX;
    long theY = inUnit->mTileY;
    long theEndX = theX + theSize;
    long theEndY = theY + theSize;
    EEMapTile* theTile = theMap->GetTile(theX, theY);
    if (theTile != NULL)
        theTile->mUnits = theTile->AddUnit(inUnit, theTile->mUnits);
    for (long i = theX; i < theEndX; i++) {
        for (long j = theY; j < theEndY; j++) {
            if (i == theX && j == theY)
                continue;
            theTile = theMap->GetTile(i, j);
            if (theTile != NULL)
                theTile->mUnits = theTile->AddUnit(inUnit, theTile->mUnits);
        }
    }
}

// Removes the unit from every map tile of its footprint, its own tile first
void EEUnitClass::AmbientRemoveFromTile(EEUnit* inUnit)
{
    long theSize = this->GetTileSize();
    EEMap* theMap = ::gWorld.mMap;
    long theX = inUnit->mTileX;
    long theY = inUnit->mTileY;
    long theEndX = theX + theSize;
    long theEndY = theY + theSize;
    EEMapTile* theTile = theMap->GetTile(theX, theY);
    if (theTile != NULL)
        theTile->RemoveUnit(inUnit);
    for (long i = theX; i < theEndX; i++) {
        for (long j = theY; j < theEndY; j++) {
            if (i == theX && j == theY)
                continue;
            EEMapTile* theOther = theMap->GetTile(i, j);
            if (theOther != NULL)
                theOther->RemoveUnit(inUnit);
        }
    }
}

// ---- EEUCResource.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The resource's unit
// class: 0x219c49..0x219d67 (after EEUCProjectile.cpp; EEUCSeaAnimal.cpp follows). Unit flags: the exe's module set.
// Its ctor is inline (no new site in the range); +0x70/+0x98 are ICF-folded elsewhere.

// Sets up a new resource unit with the class's resource type and amount
void EEUnitClass::ResourceSimpleUnitCreated(EEUnit* inUnit)
{
    this->BaseSimpleUnitCreated(inUnit);
    inUnit->SetResourceType(this->mResourceType);
    if (inUnit->mResourceType != kResourceNone)
        inUnit->SetResourceAmount(this->mResourceAmount);
}

// Adds the resource to every tile of its footprint (its own tile first), unless it is queued for deletion
void EEUnitClass::ResourceAddToTile(EEUnit* inUnit)
{
    long theSize = this->GetTileSize();
    EEMap* theMap = ::gWorld.mMap;
    long theX = inUnit->mTileX;
    long theY = inUnit->mTileY;
    long theEndX = theX + theSize;
    long theEndY = theY + theSize;
    if (inUnit->mIsQueuedForDelete)
        return;
    EEMapTile* theTile = theMap->GetTile(theX, theY);
    if (theTile != NULL)
        theTile->mUnits = theTile->AddUnit(inUnit, theTile->mUnits);
    for (long i = theX; i < theEndX; i++) {
        for (long j = theY; j < theEndY; j++) {
            if (i == theX && j == theY)
                continue;
            EEMapTile* theOther = theMap->GetTile(i, j);
            if (theOther != NULL)
                theOther->mUnits = theOther->AddUnit(inUnit, theOther->mUnits);
        }
    }
}

// ---- EEUCLand.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The land unit class:
// 0x213866..0x213c60 (after EEUCHelicopterTransport.cpp; EEUCLandable.cpp follows). One unit per EEUC* class in
// alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); slots +0x78,
// +0x74, +0xe0 (the paratroopers), +0xbc, +0xb8 in address order, then EEComplexUnit::UpdateWheels's COMDAT after its
// first user VirtualB8.

// 0x20b6ff (EEUCAirplane.cpp): the plane's flight for inPlayer (provisional name)
void EECreateCarrierAircraft(EEComplexUnit* inUnit, long inPlayer);

// 0xde4a9 (EEE2DBattlefieldAmbient.cpp's range): the unit's battlefield ambience (provisional name)
void EEPlaceTrackDecal(EEComplexUnit* inUnit);

// Sets up a new land unit: its class's resource (and amount when it has one) and, for the radio man, its power
void EEUnitClass::LandUnitCreated(EEComplexUnit* inUnit)
{
    this->BaseUnitCreated(inUnit);
    inUnit->SetResourceType(this->mResourceType);
    if (inUnit->mResourceType != kResourceNone)
        inUnit->SetResourceAmount(this->mResourceAmount);
    if (inUnit->mUnitClass->mUnitType == kUnitTypeInf10RadioMan)
        inUnit->SetPower(inUnit->mUnitClass->mPower);
}

// Calls a paratroop plane (class 0x2e1): it appears over the last map tile beyond the unit in +y and flies to the
// unit's goal location to unload its cargo
void EEUnitClass::LandCallParatroopPlane(EEComplexUnit* inUnit)
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
    thePlane->QueueGoal(EEUGRust::NewUnloadTransport(thePlane, (long)theTarget.GetX(), (long)theTarget.GetY(), false), 0);
    thePlane->SetFlightTime(1000000);
    GE3DPoint theStart(theTransformation);
    thePlane->mUnitClass->PathToward(thePlane, theStart, theTarget, true);
    thePlane->StartAction(kActionFly);
    thePlane->mUnitClass->ActionChanged(thePlane);
}

// Updates each of the unit's wheels
void EEComplexUnit::UpdateWheels()
{
    unsigned long theCount = this->mNumWheels;
    for (unsigned long i = 0; i < theCount; i++)
        this->mWheels[i]->Update(this);
}

// ---- EEUCLandMilitaryHero.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The military
// hero's unit class: 0x216d4b..0x216d57 (after EEUCLandCitizen.cpp; EEUCLandPriest.cpp follows). One unit per EEUC*
// class in alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); its
// only out-of-line slot is VirtualB8 (the others are EEUCLand's by ICF).

// ---- EEUCNaval.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The warships' unit class:
// 0x217c6d..0x218158 (after EEUCMissile.cpp; EEUCNavalBuilding.cpp follows), slots +0x0c, +0x74, +0x78, +0x88, +0xa0
// and +0xb8 of ee/EEUCNaval.h plus the helper UpdateResourceUnit. The ctor is inline (EEUnitClass.cpp's factory). Unit flags:
// the exe's module set.
// Virtual0C 0x217c6d forwards (inUnit, ioPath, inArg) to the cdecl 0x23df77 (declared here).


// Sets up a new ship at sea level (z 0), with the class's resource type and amount
void EEUnitClass::NavalUnitCreated(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    this->BaseUnitCreated(inUnit);
    theTransformation.SetX(theTransformation.GetX());
    theTransformation.SetZ(0.0f);
    inUnit->SetTransformation(theTransformation, true);
    inUnit->SetResourceType(this->mResourceType);
    if (inUnit->mResourceType != kResourceNone)
        inUnit->SetResourceAmount(this->mResourceAmount);
}

// Returns whether the ship may stop: already sinking, a slow request, or at the path's end tile with its speed reached
bool EEUnitClass::NavalHasArrived(EEComplexUnit* inUnit, float inValue)
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

// Updates a fish resource: while its resources rot it loses one every 4 s; a rotting unit counts its rotting time down
void EEUnitClass::UpdateResourceUnit(EEComplexUnit* inUnit)
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

// ---- EEUCSubmergedNaval.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The submarine's
// unit class (EEUCSeaAnimal shares most slots by ICF): 0x21ab66..0x21b163 (after EEUCSpaceTransport.cpp;
// EEUCTorpedo.cpp follows). Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); slots
// +0x04, +0x08, +0x34, +0x30, +0x74, +0x84, +0x88, +0xa0, +0xb8 in address order.

// 0x23f27e (cdecl): moves a location to the depth a submerged unit may take at its tile (provisional name)
void EEGetSeabedDepth(EEComplexUnit* inUnit, GE3DPoint& ioLocation);

// Lifts a location that lies below the terrain mesh (the sea floor) up onto it
void EEUnitClass::SubmergedNavalGroundLocation(GE3DPoint& ioLocation)
{
    GETerrainMesh& theMesh = ::gWorld.GetMap()->mTerrainMesh;
    float theGround = theMesh.GetMeshZ(ioLocation.GetX(), ioLocation.GetY());
    if (theGround >= ioLocation.GetZ())
        ioLocation.SetZ(theGround);
}

// Returns the tile where the view line through a point meets the submarine's cruise depth (or the sea
// floor where that is higher)
void EEUnitClass::SubmergedNavalLocationToTile(const GE3DPoint& inLocation, long& outX, long& outY)
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

// Creates a submarine at a placement kept above the sea floor, then moves it to the depth it may take there
EEUnit* EEUnitClass::SubmergedNavalCreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation,
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
    EEUnit* theUnit = this->BaseCreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
    EEGetSeabedDepth(theUnit->mComplexUnit, thePosition);
    theUnit->mComplexUnit->SetLocation(thePosition, true);
    return theUnit;
}

// Creates a submarine on a tile (yaw pi) kept above the sea floor, then moves it to the depth it may take there
EEUnit* EEUnitClass::SubmergedNavalCreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
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
    EEUnit* theUnit = this->BaseCreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
    EEGetSeabedDepth(theUnit->mComplexUnit, thePosition);
    theUnit->mComplexUnit->SetLocation(thePosition, true);
    return theUnit;
}

// Adds the submarine to its tile's unit list
void EEUnitClass::SubmergedNavalAddToTile(EEUnit* inUnit)
{
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = ::gWorld.mMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->mUnits = theTile->AddUnit(theUnit, theTile->mUnits);
}

// Returns whether the submarine reached its waypoint: on the waypoint's tile with more to go, when its collision
// radius covers the distance left; otherwise when that distance is at most 0.05 (always in collision state 8)
bool EEUnitClass::SubmergedNavalHasArrived(EEComplexUnit* inUnit, float inValue)
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

// ---- EEUCSeaAnimal.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The sea animal's unit
// class: 0x219d67..0x219fe3 (after EEUCResource.cpp; EEUCSpaceBuilding.cpp follows). Unit flags: the exe's module
// set. Its ctor is inline. Data (.rdata, in order): the four named depth constants 0x447340..0x44734c.

// the depths a sea animal swims at (provisional names)
const float kSeaAnimalBottomClearance = 0.09f;

const float kSeaAnimalMaximumStep = 0.48f;

const float kSeaAnimalMinimumDepth = -0.6f;

const float kSeaAnimalMaximumDepthFactor = 0.81f;

// Gives each path point (from the end back) a depth one random step toward a random depth: dolphins step 0.2..1.2
// and may dive deeper, others step up to 0.48; kept under -0.1 (dolphins) or -0.6 and 0.09 above the sea floor
void EEUnitClass::SeaAnimalAdjustPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inSmooth)
{
    const GETerrainMesh& theMesh = ::gWorld.mMap->mTerrainMesh;
    float theDepth = inUnit->mPhysicalModel.GetLocation().GetZ();
    bool theSurfaced = true;
    for (long i = ioPath.size() - 1; i >= 0; i--) {
        float theGround = theMesh.GetMeshZ(ioPath[i].GetX(), ioPath[i].GetY());
        float theTarget;
        bool theDive = this->GetUnitType() == kUnitTypeResDolphin;
        if (theDive) {
            theDive = i != 0;
            if (theDive) {
                theDive = !theSurfaced;
                if (theDive)
                    theDive = gPhysics->mRandom.Generate() < 0.35f;
            }
        }
        if (!theDive) {
            theTarget = gPhysics->GetRandom(0.1f, kSeaAnimalMaximumDepthFactor) * theGround;
            theTarget = min(kSeaAnimalMinimumDepth, theTarget);
        } else {
            theTarget = gPhysics->GetRandom(0.45f, 0.85f) * theGround;
            theSurfaced = true;
        }
        if (theSurfaced)
            theSurfaced = false;
        float theDistance = UMAbs(theDepth - theTarget);
        float theLimit;
        if (this->GetUnitType() == kUnitTypeResDolphin) {
            float theStep = gPhysics->GetRandom(0.2f, 1.2f);
            if (theDepth < theTarget)
                theDepth += theStep;
            else
                theDepth -= theStep;
            theDistance = UMAbs(theDistance - theStep);
            theLimit = -0.1f;
        } else {
            float theStep = gPhysics->GetRandom(0.2f, kSeaAnimalMaximumStep);
            if (theDepth < theTarget)
                theDepth += theStep;
            else
                theDepth -= theStep;
            theDistance = UMAbs(theDistance - theStep);
            theLimit = kSeaAnimalMinimumDepth;
        }
        theDepth = min(theLimit, theDepth);
        if (theDepth <= theGround + kSeaAnimalBottomClearance)
            theDepth = theGround + kSeaAnimalBottomClearance;
        ioPath[i].SetZ(theDepth);
    }
}

