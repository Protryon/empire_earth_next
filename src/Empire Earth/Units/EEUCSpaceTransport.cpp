// C:\EEX\Empire Earth\Units\EEUCSpaceTransport.cpp (file name inferred: no __FILE__; the class name). The space
// transport's unit class: 0x21a6e0..0x21ab66 (after EEUCSpaceship.cpp; EEUCSubmergedNaval.cpp's +0x04 0x21ab66
// follows). Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory). Slots +0xe4, +0x24,
// +0x2c, +0xf4, +0xf8, EEUCTransport::ContinentsConnect, +0x78, +0x84, +0x98, +0xa0 in address order; +0x24/+0x2c/+0x78/+0xa0
// call the spaceship's bodies (helpers ICF-folded into EEUCSpaceship.cpp's slots).
#include <windows.h>
#include <stdio.h>
#include <algorithm>

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
#include "lle/U2DPolygon.h"
#include "lle/U2DSparseArray.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUCTransport.h"
#include "lle/SESoundManager.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEEffectManager.h"
#include "ee/EEUCSpaceship.h"
#include "ee/EEUCSpaceTransport.h"
#include "ee/EEUnitUtilities.h"

// the spaceship unit's kSpaceshipHeight (0x447358): the bodies below are the class's own copies of EEUCSpaceship's
static const float kSpaceTransportHeight = 12.0f;

// FUNCTION: EXE 0x61a6e0
// Finds where a space transport meets its passengers: the base rule on one continent, else its shore tile and
// their landing tile, then the meeting tiles near them
void EEUCSpaceTransport::FindRendezvousTiles(std::vector<EEComplexUnit*>* inUnits, EEComplexUnit* inTransport,
                                   U2DPoint<long>* outTransportTile, U2DPoint<long>* outUnitTile)
{
    U2DPoint<long> theGather;
    U2DPoint<long> theFrom(inTransport->mTileX, inTransport->mTileY);
    unsigned short theUnitContinent = gContinentManager.GetContinentID((*inUnits->begin())->mTileX, (*inUnits->begin())->mTileY);
    unsigned short theTransportContinent = gContinentManager.GetContinentID(inTransport->mTileX, inTransport->mTileY);
    this->FindGatheringTile(inUnits, theFrom, &theGather);
    if (theUnitContinent == theTransportContinent) {
        EEUCTransport::FindRendezvousTiles(inUnits, inTransport, outTransportTile, outUnitTile);
    } else {
        U2DPoint<long> theTransportTile(inTransport->mTileX, inTransport->mTileY);
        U2DPoint<long> theUnitTile((*inUnits->begin())->mTileX, (*inUnits->begin())->mTileY);
        EETileCoordinate theStart(inTransport->mTileX, inTransport->mTileY);
        this->FindShoreTile(inTransport, theStart, &theUnitTile, &theTransportTile, true, outTransportTile);
        theFrom = *outTransportTile;
        theTransportTile = U2DPoint<long>((*inUnits->begin())->mTileX, (*inUnits->begin())->mTileY);
        this->FindLandingTile(inTransport, &theFrom, &theTransportTile, outUnitTile);
    }
    this->FindMeetingTiles(inTransport, (*inUnits->begin()), outTransportTile, outUnitTile);
}

// FUNCTION: EXE 0x61a80d
// Returns the flying height for a location (the spaceship rule)
float EEUCSpaceTransport::GetHeightAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{
    return this->SpaceshipVirtual24(inUnit, inLocation);
}

// FUNCTION: EXE 0x61a81d
// Returns a sound's volume, pan and pitch with a doppler shift (the spaceship rule)
void EEUCSpaceTransport::GetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
                                   float* outPitch)
{
    this->SpaceshipVirtual2C(inUnit, inSound, outVolume, outPan, outPitch);
}

// FUNCTION: EXE 0x61a838
// Returns whether the transport can carry a unit: both on the map, the base rule agrees and their continents connect
bool EEUCSpaceTransport::CanCarry(EEComplexUnit* inUnit1, EEComplexUnit* inUnit2)
{
    bool theResult = false;
    if (inUnit1->mTileX != -10000 && inUnit2->mTileX != -10000) {
        unsigned short theFirst = gContinentManager.GetContinentID(inUnit1->mTileX, inUnit1->GetTileY());
        unsigned short theSecond = gContinentManager.GetContinentID(inUnit2->GetTileX(), inUnit2->GetTileY());
        if (EEUCTransport::CanCarry(inUnit1, inUnit2) && this->ContinentsConnect(theFirst, theSecond))
            theResult = true;
        else
            theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x61a8aa
// Returns whether the transport can unload at a tile: not deep water, cliff or space, both continents known, the base
// class agrees, and its land continent connects to the tile's (or its space region is flagged for it)
bool EEUCSpaceTransport::CanUnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    EEMapTile* theTile = ::gWorld.mMap->GetTile(inX, inY);
    bool theResult = false;
    if (theTile != NULL) {
        long theType = theTile->mTerrainType;
        if (theType == kTerrainDeepWater || theType == kTerrainCliff || theType == kTerrainSpace) {
            theResult = false;
        } else {
            EEAIContinent* theFrom = gContinentManager.GetContinentAt(inTransport->GetTileX(), inTransport->GetTileY());
            EEAIContinent* theTo = gContinentManager.GetContinentAt(inX, inY);
            if (theFrom == NULL || theTo == NULL) {
                theResult = false;
            } else {
                // the base class decides; a continent of another kind keeps its answer
                theResult = EEUCTransport::CanUnloadAt(inTransport, inX, inY);
                if (theResult) {
                    if (theFrom->mCategory == kContinentCategoryLand) {
                        unsigned short theToID = theTo->mID;
                        unsigned short theFromID = theFrom->mID;
                        theResult = this->ContinentsConnect(theFromID, theToID);
                    } else if (theFrom->mCategory == kContinentCategorySpace)
                        theResult = gContinentManager.mTileFlags[theFrom->mID * gContinentManager.mFlagsWidth + theTo->mID];
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x61a995
// Returns whether two continents connect: the same one, or a neighbour of the first that is the second or (not a
// cliff) is flagged as linked to it
bool EEUCTransport::ContinentsConnect(unsigned short inFrom, unsigned short inTo)
{
    std::vector<unsigned short> theIDs;
    bool theResult = inFrom == inTo;
    if (!theResult && inFrom != 0 && inTo != 0) {
        gContinentManager.GetAdjacentIDs(theIDs, inFrom);
        for (long i = theIDs.size() - 1; i >= 0 && !theResult; i--) {
            EEAIContinent* theContinent = gContinentManager.mContinents[theIDs[i]];
            if (theContinent != NULL
                && (theIDs[i] == inTo
                    || (theContinent->mCategory != kContinentCategoryCliff
                        && gContinentManager.mTileFlags[inTo * gContinentManager.mFlagsWidth + theIDs[i]] != 0)))
                theResult = true;
            else
                theResult = false;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x61aa74
// Sets up a new space transport at its flying height, with the class's resource type and amount
void EEUCSpaceTransport::UnitCreated(EEComplexUnit* inUnit)
{
    this->SpaceshipVirtual78(inUnit);
    inUnit->SetResourceType(this->mResourceType);
    if (inUnit->mResourceType != kResourceNone)
        inUnit->SetResourceAmount(this->mResourceAmount);
}

// FUNCTION: EXE 0x61aaa9
// Adds the transport to its own tile only
void EEUCSpaceTransport::AddToTile(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = theMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->mUnits = theTile->AddUnit(theUnit, theTile->mUnits);
}

// FUNCTION: EXE 0x61aaf5
// Removes the transport from its own tile only
void EEUCSpaceTransport::RemoveFromTile(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = theMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->RemoveUnit(theUnit);
}

// FUNCTION: EXE 0x61ab3b
// Answers dying done (always) and move preparation complete (has a path) as a spaceship; others to the base class
long EEUCSpaceTransport::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    if (inEvent == kTriggerDyingAnimationFinished || inEvent == kTriggerMovePreparationComplete)
        return this->SpaceshipVirtualA0(inUnit, inEvent);
    return EEUnitClass::EvaluateCondition(inUnit, inEvent);
}

// The spaceship half: the class's own copies of EEUCSpaceship's slot bodies (identical source, so LINK's ICF folds
// them into 0x21a219/0x21a404/0x21a51a/0x21a59e; names.txt ICF aliases): the flying height, the sound with a
// doppler shift, the new unit at its flying height, and the dying / move-preparation triggers. Owner decision:
// the helpers were declared only.

float EEUCSpaceTransport::SpaceshipVirtual24(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
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
    theHeight = min(theHeight, kSpaceTransportHeight);
    theHeight = max(theHeight, theOffset);   // never below the cruise altitude (as EEUCSpaceship::GetHeightAt 0x61a219)
    return theHeight;
}

void EEUCSpaceTransport::SpaceshipVirtual2C(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
                              float* outPitch)
{
    EEUnitClass::GetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    EEGetDopplerPitch(inUnit, inSound, outVolume, outPan, outPitch);
}

void EEUCSpaceTransport::SpaceshipVirtual78(EEComplexUnit* inUnit)
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

long EEUCSpaceTransport::SpaceshipVirtualA0(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    bool theDone;
    if (inEvent == kTriggerDyingAnimationFinished)
        theDone = true;
    else if (inEvent == kTriggerMovePreparationComplete)
        theDone = (long)inUnit->mWaypoints.size() > 0;
    else
        return EEUnitClass::EvaluateCondition(inUnit, inEvent);
    if (theDone)
        theResult = 1;
    return theResult;
}

// ICF-FOLDED: EXE 0x618d33
// Finds where a unit boards the transport (the base transport's rule)
// (no address of its own: the same body as EEUCNavalTransport::FindBoardingLocation, which ICF kept)
bool EEUCSpaceTransport::FindBoardingLocation(EEComplexUnit* inUnit, EEComplexUnit* inTransport, GE3DPoint& inLocation,
                                   GE3DPoint& outLocation)
{
    return EEUCTransport::FindBoardingLocation(inUnit, inTransport, inLocation, outLocation);
}

// ICF-FOLDED: EXE 0x618a5f
// Finds where the transport unloads for a destination: shore and landing tiles, then the destination when in range
// and reachable, else the last good tile on the line from the landing tile (the first one if none is good)
// (no address of its own: the same body as EEUCNavalTransport::FindUnloadTiles, which ICF kept)
void EEUCSpaceTransport::FindUnloadTiles(EEComplexUnit* inTransport, U2DPoint<long>* inDestination,
                                   U2DPoint<long>* outUnloadTile, U2DPoint<long>* outFirstCargoTile,
                                   U2DPoint<long>* outTile)
{
    std::vector<EEUnit*> theUnits;
    std::vector<U2DPoint<long> > theTiles;
    float theRadius = this->mUnloadRange;
    float theRange = theRadius * theRadius;
    EETileCoordinate theFrom = inTransport->GetTile();
    this->FindShoreTile(inTransport, theFrom, inDestination, inDestination, true, outUnloadTile);
    this->FindLandingTile(inTransport, outUnloadTile, inDestination, outFirstCargoTile);
    if (outUnloadTile->DistanceToSquaredF(*inDestination) < theRange
        && this->CanUnloadNear(NULL, inTransport->GetCargo(0), outUnloadTile, outFirstCargoTile, inDestination)) {
        if (this->CanUnloadNear(NULL, *(inTransport->mCargo.begin() + 0), outUnloadTile, outFirstCargoTile, inDestination))
            *outTile = *inDestination;
        else
            *outTile = *outFirstCargoTile;
    } else {
        EEGetTileLine(outFirstCargoTile->GetX(), outFirstCargoTile->GetY(), inDestination->GetX(),
                   inDestination->GetY(), &theTiles, 1, 0);
        std::vector<U2DPoint<long> >::iterator theTile = theTiles.begin();
        while (theTile != theTiles.end()) {
            U2DPoint<long>* thePoint = &(*theTile);
            float theDistance = outUnloadTile->DistanceToSquaredF(*thePoint);
            EEMapTile* theMapTile = ::gWorld.mMap->GetTile(thePoint->GetX(), thePoint->GetY());
            // port: a tile off the map ends the line (GetTile returns NULL there; EEGetBlockingUnits read through it:
            // an AI transport's unload line ran off the map in an EE-SIM game, docs/AI_RESEARCH.md §8.27)
            if (thePoint == NULL || theMapTile == NULL || theDistance > theRange
                || EEGetBlockingUnits(inTransport->GetPlayer(), this->GetUnitType(), theMapTile, 0, theUnits)
                || !this->CanUnloadNear(NULL, inTransport->mCargo[0], outUnloadTile, outFirstCargoTile, thePoint))
                break;
            theTile++;
        }
        if (theTile != theTiles.begin() && theTiles.size() > 0)
            theTile--;
        if (theTile != theTiles.end())
            *outTile = *theTile;
        else
            *outTile = *outFirstCargoTile;
    }
}

// ICF-FOLDED: EXE 0x6135bc
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EEUCSpaceTransport::HasRoom(class EEComplexUnit *,class EEComplexUnit *)
{
    return true;
}
