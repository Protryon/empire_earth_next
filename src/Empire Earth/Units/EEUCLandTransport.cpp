// C:\EEX\Empire Earth\Units\EEUCLandTransport.cpp (file name inferred: no __FILE__; the class name).
// The land
// transport's unit class: 0x216de3..0x217085 (after EEUCLandPriest.cpp; EEUCMiningBuilding.cpp follows). Unit flags:
// the exe's module set. Its ctor is inline (vtable 0x448930); the slots +0xe4, +0xf4, +0xf8, +0x8c, +0xb8 in address
// order. +0x8c and +0xf4 are also EEUCFortress's (ICF).
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <set>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DPoint.h"

#include "ee/EEUnitUpdateGolden.h"   // TEST ONLY (round 13's golden)
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUCTransport.h"
#include "ee/EEUCLandTransport.h"

// FUNCTION: EXE 0x616de3
// Picks where a land transport meets the unit FindGatheringTile picks: the tile on the line between them split by
// their top speeds, refined by FindMeetingTiles; the units board there too
void EEUCLandTransport::FindRendezvousTiles(std::vector<EEComplexUnit*>* inUnits, EEComplexUnit* inTransport,
                                  U2DPoint<long>* outTransportTile, U2DPoint<long>* outUnitTile)
{
    std::vector<U2DPoint<long> > theLine;
    U2DPoint<long> theFrom(inTransport->GetTileX(), inTransport->GetTileY());
    U2DPoint<long> theNearest;
    U2DPoint<long> theTile;
    EEComplexUnit* theUnit = (*inUnits)[this->FindGatheringTile(inUnits, theFrom, &theNearest)];
    theFrom.GetLineTiles(theNearest, 1.0f, theLine);
    float theShare = inTransport->mUnitClass->GetMaxVelocity();
    *outTransportTile = theLine[UMFloorUnsigned(theLine.size() * theShare /
                                                (theShare + theUnit->mUnitClass->GetMaxVelocity()))];
    theTile = *outTransportTile;
    this->FindMeetingTiles(inTransport, theUnit, &theTile, outTransportTile);
    *outUnitTile = *outTransportTile;
}

// FUNCTION: EXE 0x616f17
// Returns whether the transport can carry a unit: both on the map, on the same continent, and the common test allows it
bool EEUCLandTransport::CanCarry(EEComplexUnit* inUnit1, EEComplexUnit* inUnit2)
{
    bool theResult = false;
    long theX1 = inUnit1->mTileX;
    if (theX1 != -10000) {
        long theX2 = inUnit2->mTileX;
        if (theX2 != -10000) {
            long theY1 = inUnit1->mTileY;
            if (gContinentManager.GetContinentID(theX1, theY1) == gContinentManager.GetContinentID(theX2, inUnit2->mTileY)
                && EEUCTransport::CanCarry(inUnit1, inUnit2))
                theResult = true;
            else
                theResult = false;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x616f8e
// Returns whether the transport can unload on a tile: on its own continent, and the common test allows it
bool EEUCLandTransport::CanUnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    return gContinentManager.GetContinentID(inTransport->GetTileX(), inTransport->GetTileY()) == gContinentManager.GetContinentID(inX, inY)
        && EEUCTransport::CanUnloadAt(inTransport, inX, inY);
}

// FUNCTION: EXE 0x616fea
// Unloads all cargo at once when the transport is badly hurt
void EEUCLandTransport::BadlyHurt(EEUnit* inUnit)
{
    std::vector<EEComplexUnit*> theCargo;
    EEComplexUnit* theTransport = inUnit->mComplexUnit;
    EEComplexUnit* theUnit;
    unsigned long theCount = theTransport->GetNumCargoUnits();
    for (unsigned long i = 0; i < theCount; i++) {
        theUnit = theTransport->GetCargo(i);
        if (theUnit != NULL)
            theCargo.push_back(theUnit);
    }
    unsigned long theSize = theCargo.size();
    for (unsigned long j = 0; j < theSize; j++)
        theTransport->UnloadCargoNow(theCargo[j]);
}

// FUNCTION: EXE 0x617070
// Updates the transport the common way, then its wheels
void EEUCLandTransport::Update(EEComplexUnit* inUnit)
{
    EEUCTransport::Update(inUnit);
    (EEUN(WHEELS), inUnit->UpdateWheels());
}

// UnloadAt: unloads at a tile the common transport way
// (no marker: ICF-folded into EEUCNavalTransport::UnloadAt 0x218e14, the same forwarding body; the vtable 0x448930's
// +0x104 slot points there)
void EEUCLandTransport::UnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    EEUCTransport::UnloadAt(inTransport, inX, inY);
}

// ICF-FOLDED: EXE 0x618d33
// Finds where a unit boards the transport (the base transport's rule)
// (no address of its own: the same body as EEUCNavalTransport::FindBoardingLocation, which ICF kept)
bool EEUCLandTransport::FindBoardingLocation(EEComplexUnit* inUnit, EEComplexUnit* inTransport, GE3DPoint& inLocation,
                                   GE3DPoint& outLocation)
{
    return EEUCTransport::FindBoardingLocation(inUnit, inTransport, inLocation, outLocation);
}

// ICF-FOLDED: EXE 0x6135bc
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EEUCLandTransport::HasRoom(class EEComplexUnit *,class EEComplexUnit *)
{
    return true;
}
