// C:\EEX\Empire Earth\Units\EEUCTransport.cpp (file name inferred: no __FILE__; the class name). The transports' base
// unit class: 0x21b27a..0x21c9e1 (after EEUCTorpedo.cpp). Unit flags: the exe's module set. Its ctor is out of line in
// EEUnitClass.cpp (0x23629e); slots +0xe0, +0xe4, +0xe8, the loading helpers (two EEComplexUnit distance inlines among
// them, first emitted here), +0xf4, +0xfc, +0xf8, +0x78, +0x100, +0x104, +0xb8 in address order. The untagged
// functions 0x21c9e1..0x21ce6c before EEUGAttackGround.cpp's key chain are not placed yet (another unit's?).
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

#include "ee/EEUnitUpdate.h"
#include "rust/ee_units.h"
#include "ee/EEUnitKind.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EETileLocation.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUCTransport.h"
#include "ee/EEUGRust.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EmpireEarth.h"

// unit-local constants (provisional names)
static const float kTransportNoPath = 10000.0f;   // 0x44736c (EEGetPathLengthTo: no path)
static const float kTransportNearSquared = 25.0f; // 0x447370

// FUNCTION: EXE 0x61b27a
// Returns the index of the unit nearest a tile and puts that unit's tile in outTile (0, nothing set, for no units)
long EEUCTransport::FindGatheringTile(std::vector<EEComplexUnit*>* inUnits, U2DPoint<long> inFrom, U2DPoint<long>* outTile)
{
    U2DPoint<long> theTile;
    unsigned long theCount = inUnits->size();
    long theResult = 0;
    if (theCount > 0) {
        EEComplexUnit* theFirst = inUnits->front();
        theTile.SetX(theFirst->GetTileX());
        theTile.SetY(theFirst->GetTileY());
        float theBest = inFrom.DistanceToSquaredF(theTile);
        *outTile = theTile;
        theResult = 0;
        for (unsigned long i = 1; i < theCount; i++) {
            EEComplexUnit* theUnit = (*inUnits)[i];
            theTile.SetX(theUnit->GetTileX());
            theTile.SetY(theUnit->GetTileY());
            float theDistance = inFrom.DistanceToSquaredF(theTile);
            if (!(theDistance < theBest))
                continue;
            theBest = theDistance;
            *outTile = theTile;
            theResult = i;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x61b33f
// Uses the transport's own tile as both the transport's and the units' meeting tile
void EEUCTransport::FindRendezvousTiles(std::vector<EEComplexUnit*>* inUnits, EEComplexUnit* inTransport,
                              U2DPoint<long>* outTransportTile, U2DPoint<long>* outUnitTile)
{
    *outTransportTile = U2DPoint<long>(inTransport->GetTileX(), inTransport->GetTileY());
    *outUnitTile = *outTransportTile;
}

// FUNCTION: EXE 0x61b361
// Uses the destination as the unload tile, the first cargo tile and the final tile
void EEUCTransport::FindUnloadTiles(EEComplexUnit* inTransport, U2DPoint<long>* inDestination, U2DPoint<long>* outUnloadTile,
                              U2DPoint<long>* outFirstCargoTile, U2DPoint<long>* outTile)
{
    *outTile = *outFirstCargoTile = *outUnloadTile = *inDestination;
}

// FUNCTION: EXE 0x61b38e
// Picks the unit's boarding tile: its own when the transport can unload there, else the nearest free one within
// two tiles, else the transport's tile (naval and space transports) or the unit's own
// Returns nothing (no value in eax at the ret); compare EEUCSpaceTransport::FindRendezvousTiles
void EEUCTransport::FindMeetingTiles(EEComplexUnit* inTransport, EEComplexUnit* inUnit, U2DPoint<long>* ioTransportTile,
                             U2DPoint<long>* ioUnitTile)
{
    std::vector<EEUnit*> theUnits;
    std::vector<EETileCoordinate> theTiles;
    if (this->CanUnloadAt(inTransport, ioUnitTile->GetX(), ioUnitTile->GetY()))
        return;
    EEGetTilesInRadius(ioUnitTile->GetX(), ioUnitTile->GetY(), 2, true, theTiles);
    std::sort(theTiles.begin(), theTiles.end(), EETileDistanceLess(*ioUnitTile));
    std::vector<EETileCoordinate>::iterator theTile = theTiles.begin();
    for (; theTile != theTiles.end(); theTile++) {
        EEMapTile* theMapTile = ::gWorld.mMap->GetTile(theTile->GetX(), theTile->GetY());
        if (theMapTile != NULL && this->CanUnloadAt(inTransport, theTile->GetX(), theTile->GetY())
            && !EEGetBlockingUnits(inTransport->GetPlayer(), this->GetUnitType(), theMapTile, 0, theUnits)) {
            *ioUnitTile = *theTile;
            break;
        }
    }
    if (theTile != theTiles.end())
        return;
    long theKind = inTransport->mUnitClass->mCategory;
    if (theKind == kUnitCategoryNavalTransport || theKind == kUnitCategorySpaceTransport)
        *ioUnitTile = *ioTransportTile;
    else
        *ioUnitTile = U2DPoint<long>(inUnit->GetTileX(), inUnit->GetTileY());
}

// FUNCTION: EXE 0x61b4de
// Lists the 8 tiles around a tile (the four sides first, then the corners)
void EEUCTransport::GetRingTiles(long inX, long inY, std::vector<U2DPoint<long> >& outTiles)
{
    outTiles.clear();
    outTiles.reserve(8);
    outTiles.push_back(U2DPoint<long>(inX, inY + 1));
    outTiles.push_back(U2DPoint<long>(inX, inY - 1));
    outTiles.push_back(U2DPoint<long>(inX + 1, inY));
    outTiles.push_back(U2DPoint<long>(inX - 1, inY));
    outTiles.push_back(U2DPoint<long>(inX - 1, inY + 1));
    outTiles.push_back(U2DPoint<long>(inX + 1, inY + 1));
    outTiles.push_back(U2DPoint<long>(inX + 1, inY - 1));
    outTiles.push_back(U2DPoint<long>(inX - 1, inY - 1));
}

// FUNCTION: EXE 0x61b5ab
// Returns whether a unit may enter this kind of transport (land, helicopter, naval or space transport, fortress)
bool EEUCTransport::CanCarry(EEComplexUnit* inUnit1, EEComplexUnit* inUnit2)
{
    long theCategory = this->mCategory;
    return (theCategory == kUnitCategoryLandTransport && inUnit2->mUnitClass->mCanEnterLandTransports) || (theCategory == kUnitCategoryHelicopterTransport && inUnit2->mUnitClass->mCanEnterAirTransports)
        || (theCategory == kUnitCategoryNavalTransport && inUnit2->mUnitClass->mCanEnterSeaTransports) || (theCategory == kUnitCategorySpaceTransport && inUnit2->mUnitClass->mCanEnterSpaceTransports)
        || (theCategory == kUnitCategoryFortress && inUnit2->mUnitClass->mCanEnterFortresses);
}

// FUNCTION: EXE 0x61b613
// Returns whether the transport can pick a unit up now: on one continent when the squared path length is in load
// range or the unit is next to it; across water when the unit and transport are near its shore tile
bool EEUCTransport::CanLoad(EEComplexUnit* inTransport, EEComplexUnit* inUnit)
{
    long theY;
    long theX;
    EEUnit* theArg;
    std::vector<EETileCoordinate> theTiles;
    U2DPoint<long> theUnitTile;
    U2DPoint<long> theCargoTile;
    U2DPoint<long> theTransportTile;
    EEUGRust* theGoal = EELoadTransportGoalCast(inTransport->GetGoal());
    float theRange = this->mLoadRange;
    unsigned short theFrom = gContinentManager.GetContinentID(inTransport->GetTileX(), inTransport->GetTileY());
    unsigned short theTo = gContinentManager.GetContinentID(inUnit->GetTileX(), inUnit->GetTileY());
    theRange *= theRange;
    bool theResult = false;
    theTransportTile = U2DPoint<long>(inTransport->GetTileX(), inTransport->GetTileY());
    if (theFrom == theTo) {
        float theDistance = EEGetPathLengthTo(inUnit, theTransportTile, 0x10, false);
        float theDistance2 = EEGetPathLengthTo(inUnit, theTransportTile, 0x10, false);
        if (theDistance2 * theDistance <= theRange) {
            theResult = true;
        } else {
            theResult = false;
            inTransport->mUnitClass->GetSurroundingTiles(inTransport, theTiles, 0, false);
            theCargoTile = U2DPoint<long>(inUnit->GetTileX(), inUnit->GetTileY());
            for (std::vector<EETileCoordinate>::iterator it = theTiles.begin(); !theResult && it != theTiles.end(); it++)
                theResult = *it == theCargoTile;
        }
    } else {
        theCargoTile = U2DPoint<long>(inUnit->GetTileX(), inUnit->GetTileY());
        if (theGoal == NULL)
            this->FindBestShoreTile(inTransport, theTransportTile, theCargoTile, theTransportTile, false, &theUnitTile);
        else
            theUnitTile = theGoal->GetLoadUnitTile();
        if (theUnitTile.GetX() != -10000) {
            float theDistance = inUnit->DistanceSquaredToTile(theUnitTile);
            if (theDistance < theRange) {
                if (!(inTransport->DistanceSquaredToTile(theUnitTile) + theDistance < theRange)) {
                    theResult = false;
                } else {
                    if (EEGetPathLengthTo(inUnit, theUnitTile, 0x10, false) != kTransportNoPath) {
                        theResult = true;
                    } else if (inTransport->DistanceSquaredTo(inUnit) < theRange) {
                        theResult = true;
                        if (EEIsLineBlocked(inUnit->GetTileX(), inUnit->GetTileY(), inTransport->GetTileX(),
                                       inTransport->GetTileY(), &theArg, &theX, &theY))
                            theResult = false;
                    } else {
                        theResult = false;
                    }
                }
            }
        }
    }
    return theResult;
}

// TEMPLATE: EXE 0x61b819
// EEComplexUnit::DistanceSquaredToTile (inline in ee/EEUnit.h; COMDAT after its first user CanLoad)

// FUNCTION: EXE 0x61b83f
// Returns whether cargo unloaded here reaches the destination within unload range (squared path length, plus
// the transport's distance to the first cargo tile across water); siege towers path as towers
bool EEUCTransport::CanUnloadNear(EEComplexUnit* inTransport, EEComplexUnit* inCargo,
                             U2DPoint<long>* ioUnloadTile, U2DPoint<long>* ioFirstCargoTile,
                             U2DPoint<long>* inDestination)
{
    GE3DPoint thePosition;
    EETileCoordinate theDestination(inDestination->GetX(), inDestination->GetY());
    GE3DPoint theInvalid;
    GE3DPoint theFromPosition;
    float theRange = this->mUnloadRange * this->mUnloadRange;
    unsigned short theFrom = gContinentManager.GetContinentID(ioUnloadTile->GetX(), ioUnloadTile->GetY());
    unsigned short theTo = gContinentManager.GetContinentID(inDestination->GetX(), inDestination->GetY());
    bool theResult = false;
    theInvalid.SetXYZ(-10000.0f, -10000.0f, 0.0f);
    if (theFrom == theTo) {
        bool theFlag = false;
        if (this->mCategory == kUnitCategoryLandTransport || this->mCategory == kUnitCategoryFortress)
            ::gWorld.mMap->GetTileLocation(inTransport->GetTileX(), inTransport->GetTileY(), thePosition);
        else
            ::gWorld.mMap->GetTileLocation(ioFirstCargoTile->GetX(), ioFirstCargoTile->GetY(), thePosition);
        inCargo->SetLocation(thePosition, false);
        if (this->GetUnitType() == kUnitTypeSiege04Tower || this->GetUnitType() == kUnitTypeSiege06HeavyTower)
            theFlag = true;
        float theFirst = EEGetPathLengthTo(inCargo, theDestination, 0x10, this->GetUnitType() == kUnitTypeSiege04Tower || this->GetUnitType() == kUnitTypeSiege06HeavyTower);
        float theSecond = EEGetPathLengthTo(inCargo, theDestination, 0x10, theFlag);
        float theDistance = theSecond * theFirst;
        inCargo->SetLocation(theInvalid, false);
        theResult = theDistance < theRange;
    } else {
        bool theFlag = false;
        float theDistance;
        ::gWorld.mMap->GetTileLocation(ioFirstCargoTile->GetX(), ioFirstCargoTile->GetY(), thePosition);
        if (inTransport != NULL) {
            theDistance = inTransport->DistanceSquared3D(thePosition);
        } else {
            ::gWorld.mMap->GetTileLocation(ioUnloadTile->GetX(), ioUnloadTile->GetY(), theFromPosition);
            theDistance = theFromPosition.DistanceToSquared(thePosition);
        }
        if (theDistance < theRange) {
            inCargo->SetLocation(thePosition, false);
            if (this->GetUnitType() == kUnitTypeSiege04Tower || this->GetUnitType() == kUnitTypeSiege06HeavyTower)
                theFlag = true;
            float theFirst = EEGetPathLengthTo(inCargo, theDestination, 0x10, this->GetUnitType() == kUnitTypeSiege04Tower || this->GetUnitType() == kUnitTypeSiege06HeavyTower);
            float theSecond = EEGetPathLengthTo(inCargo, theDestination, 0x10, theFlag);
            float theCargoDistance = theSecond * theFirst;
            inCargo->SetLocation(theInvalid, false);
            theResult = theDistance + theCargoDistance < theRange;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x61ba6f
// Returns the squared 3D distance from the unit to a point
float EEComplexUnit::DistanceSquared3D(const GE3DPoint& inPoint)
{
    float theDZ = this->mPhysicalModel.GetLocation().GetZ() - inPoint.GetZ();
    float theDY = this->mPhysicalModel.GetLocation().GetY() - inPoint.GetY();
    float theDX = this->mPhysicalModel.GetLocation().GetX() - inPoint.GetX();
    return theDX * theDX + theDY * theDY + theDZ * theDZ;
}

// FUNCTION: EXE 0x61baac
// Picks where cargo lands: from deep water or space the free ring tile on the continent nearest the target, from
// a cliff the transport's tile, otherwise (or when none fits) the unload tile itself
void EEUCTransport::FindLandingTile(EEComplexUnit* inTransport, U2DPoint<long>* inUnloadTile,
                             U2DPoint<long>* inTarget, U2DPoint<long>* outTile)
{
    std::vector<EEUnit*> theBlockers;
    std::vector<U2DPoint<long> > theTiles;
    float theBest = 0.0f;
    bool theFound = false;
    EEMapTile* theTile = ::gWorld.mMap->GetTile(inUnloadTile->GetX(), inUnloadTile->GetY());
    if (theTile == NULL)
        return;
    long theTerrain = theTile->mTerrainType;
    if (theTerrain != kTerrainShallowWater) {
        if (theTerrain == kTerrainDeepWater || theTerrain == kTerrainSpace) {
            EEAIContinent* theContinent = gContinentManager.GetContinentAt(*inUnloadTile);
            this->GetRingTiles(inUnloadTile->mX, inUnloadTile->mY, theTiles);
            for (std::vector<U2DPoint<long> >::iterator it = theTiles.begin(); it != theTiles.end(); it++) {
                theTile = ::gWorld.mMap->GetTile(it->mX, it->mY);
                unsigned short theID = theContinent->mID;
                EEUnit* theArg5;
                long theX, theY;
                if (theTile == NULL || theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainCliff ||
                    theTile->mTerrainType == kTerrainSpace ||
                    !gContinentManager.IsNextToArea(it->GetX(), it->GetY(), theID) ||
                    EEGetBlockingUnits(inTransport->GetPlayer(), this->GetUnitType(), theTile, 0, theBlockers) ||
                    EEIsLineBlocked(inUnloadTile->GetX(), inUnloadTile->GetY(), it->GetX(), it->GetY(),
                                &theArg5, &theX, &theY))
                    continue;
                float theDistance = it->DistanceToSquaredF(*inTarget);
                if (!(theDistance <= theBest) && theBest != 0.0f)
                    continue;
                theBest = theDistance;
                *outTile = *it;
                theFound = true;
            }
            if (theFound)
                return;
        } else if (theTerrain == kTerrainCliff) {
            *outTile = U2DPoint<long>(inTransport->GetTileX(), inTransport->GetTileY());
            return;
        }
    }
    *outTile = *inUnloadTile;
}

// FUNCTION: EXE 0x61bc79
// Takes a tile as the unload tile when it is free of blocking units, within 5 tiles of the target and (when
// restricted) water for a naval transport or space for a space transport
// cdecl (provisional names). Reads only the transport's player (+0x18) and class (+0x2c); whether the original
// took an EEUnit* or an EEComplexUnit* is not recoverable from its callers
void EETestUnloadTile(EEUnit* inTransport, const U2DPoint<long>& inTile,
                 const U2DPoint<long>& inTarget, bool inRestrictTerrain, U2DPoint<long>* outTile)
{
    std::vector<EEUnit*> theBlockers;
    EETile* theTile = ::gWorld.mMap->GetTile(inTile.GetX(), inTile.GetY());
    if (theTile == NULL || (inRestrictTerrain
        && (inTransport->mUnitClass->mCategory != kUnitCategoryNavalTransport
            || (theTile->mTerrainType != kTerrainDeepWater && theTile->mTerrainType != kTerrainShallowWater))
        && (inTransport->mUnitClass->mCategory != kUnitCategorySpaceTransport || theTile->mTerrainType != kTerrainSpace)))
        return;
    if (!EEGetBlockingUnits(inTransport->GetPlayer(), inTransport->mUnitClass->GetUnitType(), theTile, 0, theBlockers)
        && inTile.DistanceToSquaredF(inTarget) < kTransportNearSquared)
        *outTile = inTile;
}

// FUNCTION: EXE 0x61bd41
// Keeps a tile as the best unload tile when it passes the same tests and is no further from the target than the
// best so far (0: none yet)
// cdecl (provisional names). The first argument needs only the base unit; its original type is not
// recoverable
void EEUpdateBestUnloadTile(EEUnit* inTransport, const U2DPoint<long>& inTile,
                 const U2DPoint<long>& inTarget, bool inRestrictTerrain, float* ioBest,
                 U2DPoint<long>* outTile)
{
    std::vector<EEUnit*> theBlockers;
    EETile* theTile = ::gWorld.mMap->GetTile(inTile.GetX(), inTile.GetY());
    if (theTile == NULL || (inRestrictTerrain
        && (inTransport->mUnitClass->mCategory != kUnitCategoryNavalTransport
            || (theTile->mTerrainType != kTerrainDeepWater && theTile->mTerrainType != kTerrainShallowWater))
        && (inTransport->mUnitClass->mCategory != kUnitCategorySpaceTransport || theTile->mTerrainType != kTerrainSpace)))
        return;
    if (EEGetBlockingUnits(inTransport->GetPlayer(), inTransport->mUnitClass->GetUnitType(), theTile, 0, theBlockers))
        return;
    float theDistance = inTile.DistanceToSquaredF(inTarget);
    if (theDistance <= *ioBest || *ioBest == 0.0f) {
        *outTile = inTile;
        *ioBest = theDistance;
    }
}

// FUNCTION: EXE 0x61be21
// Picks a shore tile of the transport's region next to the destination's continent, nearest the target (the start
// tile when in 5 tiles); the destination when there is no region, (0,0) when no tile fits
void EEUCTransport::FindShoreTile(EEComplexUnit* inTransport, const EETileCoordinate& inFrom,
                             U2DPoint<long>* inDestination, U2DPoint<long>* inTarget,
                             bool inRestrictTerrain, U2DPoint<long>* outTile)
{
    std::vector<EEUnit*> theUnits;
    std::vector<U2DPoint<long> > theTiles;
    EEAIContinent* theTo = gContinentManager.GetContinentAt(inDestination->GetX(), inDestination->GetY());
    long theKind = ((inTransport->mUnitClass->mDomains & kMovementSpace) | 4) >> 2;
    unsigned short theFromID = gContinentManager.GetContinent(inFrom, theKind);
    float theBest = 0.0f;
    bool theContainsFrom;
    outTile->SetXY(0, 0);
    EEAIContinent* theFrom = gContinentManager.GetContinentAt(inFrom.GetX(), inFrom.GetY());
    if (theFrom == theTo) {
        if (theFromID != 0) {
            theFrom->GetShoreInRegion(theFromID, theTiles, theKind);
            if (theTiles.size() != 0) {
                theContainsFrom = false;
                theBest = 0.0f;
                for (long i = theTiles.size() - 1; i >= 0; i--) {
                    unsigned short theID = theTo->mID;
                    if (!gContinentManager.IsNextToArea(theTiles[i].GetX(), theTiles[i].GetY(), theID))
                        continue;
                    EEUpdateBestUnloadTile(inTransport, theTiles[i], *inTarget, inRestrictTerrain, &theBest, outTile);
                    if (theTiles[i] == inFrom)
                        theContainsFrom = true;
                }
                if (theContainsFrom)
                    EETestUnloadTile(inTransport, inFrom, *inTarget, inRestrictTerrain, outTile);
            }
        } else if (gContinentManager.IsNextToArea(inDestination->GetX(), inDestination->GetY(), theTo->mID)) {
            *outTile = *inDestination;
        }
    } else if (theFromID != 0) {
        theTo->GetShoreInRegion(theFromID, theTiles, theKind);
        if (theTiles.size() != 0) {
            this->FindBestShoreTile(inTransport, inFrom, *inDestination, *inTarget, inRestrictTerrain, outTile);
        } else if (gContinentManager.mTileFlags[theTo->mID + theFrom->mID * gContinentManager.mFlagsWidth]) {
            theFrom->GetShoreInRegion(theFromID, theTiles, theKind);
            if (theTiles.size() != 0) {
                theContainsFrom = false;
                theBest = 0.0f;
                for (long i = theTiles.size() - 1; i >= 0; i--) {
                    unsigned short theID = theTo->mID;
                    if (!gContinentManager.IsNextToArea(theTiles[i].GetX(), theTiles[i].GetY(), theID))
                        continue;
                    EEUpdateBestUnloadTile(inTransport, theTiles[i], *inTarget, inRestrictTerrain, &theBest, outTile);
                    if (theTiles[i] == inFrom)
                        theContainsFrom = true;
                }
                if (theContainsFrom)
                    EETestUnloadTile(inTransport, inFrom, *inTarget, inRestrictTerrain, outTile);
            }
        } else {
            theFrom->GetShoreInRegion(theFromID, theTiles, theKind);
            if (theTiles.size() != 0) {
                theContainsFrom = false;
                theBest = 0.0f;
                for (long i = theTiles.size() - 1; i >= 0; i--) {
                    unsigned short theID = theFrom->mID;
                    if (!gContinentManager.IsNextToArea(theTiles[i].GetX(), theTiles[i].GetY(), theID))
                        continue;
                    EEUpdateBestUnloadTile(inTransport, theTiles[i], *inTarget, inRestrictTerrain, &theBest, outTile);
                    if (theTiles[i] == inFrom)
                        theContainsFrom = true;
                }
                if (theContainsFrom)
                    EETestUnloadTile(inTransport, inFrom, *inTarget, inRestrictTerrain, outTile);
            }
        }
    } else {
        *outTile = *inDestination;
    }
}

// FUNCTION: EXE 0x61c255
// Picks the shore tile between the start and destination continents nearest the target (else the first shore
// tile); returns its distance from the start, or 0 with outTile -10000 when there is none
float EEUCTransport::FindBestShoreTile(EEComplexUnit* inTransport, const U2DPoint<long>& inFrom,
                             const U2DPoint<long>& inDestination, const U2DPoint<long>& inTarget,
                             bool inRestrictTerrain, U2DPoint<long>* outTile)
{
    std::vector<U2DPoint<long> > theTiles;
    EEAIContinent* theFrom = gContinentManager.GetContinentAt(inFrom.GetX(), inFrom.GetY());
    EEAIContinent* theTo = gContinentManager.GetContinentAt(inDestination.GetX(), inDestination.GetY());
    long theKind = ((inTransport->mUnitClass->mDomains & kMovementSpace) | 4) >> 2;
    unsigned short theFromID = gContinentManager.GetContinent(inFrom, theKind);
    unsigned short theToID = gContinentManager.GetContinent(inDestination, theKind);
    float theResult = 0.0f;
    unsigned short theID = 0;
    float theBest = 0.0f;
    outTile->SetXY(-10000, -10000);
    if (theFrom != NULL) {
        if (theFromID != 0 && theTo->mCategory == kContinentCategoryLand) {
            theTo->GetShoreInRegion(theFromID, theTiles, theKind);
            theID = theTo->mID;
            if (theTiles.size() == 0 &&
                gContinentManager.mTileFlags[theID + theFrom->mID * gContinentManager.mFlagsWidth]) {
                theFrom->GetShoreInRegion(theFromID, theTiles, theKind);
                if (theTiles.size() != 0) {
                    bool theContainsFrom = false;
                    theBest = 0.0f;
                    for (long i = theTiles.size() - 1; i >= 0; i--) {
                        unsigned short theToAreaID = theTo->mID;
                        if (!gContinentManager.IsNextToArea(theTiles[i].GetX(), theTiles[i].GetY(), theToAreaID))
                            continue;
                        EEUpdateBestUnloadTile(inTransport, theTiles[i], inTarget, inRestrictTerrain, &theBest, outTile);
                        if (theTiles[i] == inFrom)
                            theContainsFrom = true;
                    }
                    if (theContainsFrom)
                        EETestUnloadTile(inTransport, inFrom, inTarget, inRestrictTerrain, outTile);
                }
            }
        } else if (theToID != 0 && theFrom->mCategory == kContinentCategoryLand) {
            theTo->GetShoreInRegion(theToID, theTiles, theKind);
            theID = theTo->mID;
        } else {
            EEAIContinent* theSource = NULL;
            if (theToID == 0 && theFrom->mCategory == kContinentCategoryLand) {
                theSource = theFrom;
                theID = theTo == NULL ? 0 : theTo->mID;
            } else if (theTo != NULL) {
                theID = theFrom->mID;
                theSource = theTo;
            }
            if (theSource != NULL)
                theSource->GetShoreNextTo(theID, theTiles);
        }
        if (theTiles.size() != 0) {
            bool theContainsFrom = false;
            theBest = 0.0f;
            for (long i = theTiles.size() - 1; i >= 0; i--) {
                if (!gContinentManager.IsNextToArea(theTiles[i].GetX(), theTiles[i].GetY(), theID))
                    continue;
                EEUpdateBestUnloadTile(inTransport, theTiles[i], inTarget, inRestrictTerrain, &theBest, outTile);
                if (theTiles[i] == inFrom)
                    theContainsFrom = true;
            }
            if (theContainsFrom)
                EETestUnloadTile(inTransport, inFrom, inTarget, inRestrictTerrain, outTile);
            if (outTile->GetX() == -10000)
                *outTile = theTiles[0];
        }
    }
    if (outTile->GetX() != -10000 && outTile->GetY() != -10000)
        theResult = outTile->DistanceToF(inFrom);
    return theResult;
}

// FUNCTION: EXE 0x61c610
// Finds where cargo leaves the transport: a tile around it for a unit that cannot move, else a free placement
// near the location (water allowed for non-land units); returns whether one was found
bool EEUCTransport::FindBoardingLocation(EEComplexUnit* inTransport, EEComplexUnit* inCargo,
                              GE3DPoint& inLocation, GE3DPoint& outLocation)
{
    std::vector<EETileCoordinate> theTiles;
    GETransformation theTransformation;
    GE3DPoint thePosition;
    bool theAllowWater = (inCargo->mUnitClass->mDomains & kMovementLand) == 0;
    bool theResult;
    if (inCargo->mStats->mMaxVelocity <= 0.0f) {
        EEGetTilesAroundUnit(inTransport, inCargo, 1, 0, 0, theTiles);
        theResult = EEPickTileNear(theTiles, inTransport, inCargo, 0, outLocation);
    } else {
        inTransport->GetPosition(thePosition);
        long theSize = inCargo->mUnitClass->mTileSize;
        unsigned long theY = UMFloorUnsigned(inLocation.GetY());
        unsigned long theX = UMFloorUnsigned(inLocation.GetX());
        if (EEFindPlacement(&thePosition, inTransport, NULL, theX, theY, theSize, 1, false,
                            theAllowWater, false, true, &theTransformation, NULL)) {
            theResult = true;
            outLocation = theTransformation.GetPosition();
        } else {
            theResult = false;
            outLocation = inLocation;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x61c75c
// Returns whether cargo can be unloaded on a tile (on the map, not a cliff, space or a map edge tile)
bool EEUCTransport::CanUnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    EEMapTile* theTile = ::gWorld.mMap->GetTile(inX, inY);
    return theTile != NULL && theTile->mTerrainType != kTerrainCliff && theTile->mTerrainType != kTerrainSpace
        && !::gWorld.mMap->IsEdgeTile(inX, inY);
}

// FUNCTION: EXE 0x61c7d1
// Sets up a new transport as any unit, then gives it its class's resource type and amount
void EEUCTransport::UnitCreated(EEComplexUnit* inUnit)
{
    EEUnitClass::UnitCreated(inUnit);
    inUnit->SetResourceType(this->mResourceType);
    if (inUnit->mResourceType != kResourceNone)
        inUnit->SetResourceAmount(this->mResourceAmount);
}

// FUNCTION: EXE 0x61c806
// Moves the units that cannot board (not commandable, calamity flags 0x10 or 1, already inside something, off
// the map or on deep water, or not carried by this class) from the list to outRejected
void EEUCTransport::FilterLoadable(std::vector<EEComplexUnit*>* ioUnits, EEComplexUnit* inTransport,
                               std::vector<EEComplexUnit*>* outRejected)
{
    for (unsigned long i = 0; i < ioUnits->size(); i++) {
        EEComplexUnit* theUnit = (*ioUnits)[i];
        EETile* theTile = ::gWorld.mMap->GetTileAtLocation(theUnit->GetTile());
        if (theUnit->GetIsCommandable() && !theUnit->HasCalamityFlag(0x10) && !theUnit->HasCalamityFlag(1)
            && theUnit->GetContainer() == NULL && theTile != NULL && theTile->GetTerrainType() != kTerrainDeepWater
            && this->CanCarry(inTransport, theUnit))
            continue;
        ioUnits->erase(ioUnits->begin() + i);
        i--;
        outRejected->push_back(theUnit);
    }
}

// FUNCTION: EXE 0x61c8f7
// Unloads the transport's cargo at a tile (siege towers with the tower flag set)
void EEUCTransport::UnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    bool theFlag;
    if (this->GetUnitType() == kUnitTypeSiege04Tower || this->GetUnitType() == kUnitTypeSiege06HeavyTower)
        theFlag = true;
    else
        theFlag = false;
    inTransport->UnloadCargoAtTile(inX, inY, theFlag);
}

// FUNCTION: EXE 0x61c92c
// Runs a transport's tick (physics, effects, camouflage, animation, sound), counts down its rotting time and
// regenerates when its stats allow
void EEUCTransport::Update(EEComplexUnit* inUnit)
{
    // New (R5b.4 leaf 6, the thirteenth round): the sequence is Rust (ee_units update.rs), the leaves
    // EEUnitUpdate.cpp's host calls
    EEUnitClassUpdate(EE_UNITS_UPDATE_TRANSPORT, this, inUnit);
}

// FUNCTION: EXE 0x61c9e1
// std::sort<vector<EETileCoordinate>::iterator, EETileDistanceLess> (template COMDAT; FindMeetingTiles). Its helpers
// 0x61cac7..0x61ce6c (the sort/heap family of this instantiation) are emitted too but not yet paired by csname.

// TEMPLATE: EXE 0x61cac7
// std::_Sort<_Ptrit<U2DPoint<long> >, EETileDistanceLess> (called by the sort and itself)

// TEMPLATE: EXE 0x61cbd4
// std::_Unguarded_insertion_sort

// TEMPLATE: EXE 0x61cc1e
// std::_Insertion_sort_1

// TEMPLATE: EXE 0x61cce0
// std::make_heap (calls _Make_heap 0x61cd71)

// TEMPLATE: EXE 0x61cd24
// std::sort_heap (calls pop_heap 0x61cdd6)

// TEMPLATE: EXE 0x61cd71
// std::_Make_heap

// TEMPLATE: EXE 0x61cdd6
// std::pop_heap (calls _Pop_heap_0)

// TEMPLATE: EXE 0x61ce09
// std::_Push_heap (operator() only)

// ICF-FOLDED: EXE 0x557269
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEUCTransport::BadlyHurt(class EEUnit *)
{
}

// ICF-FOLDED: EXE 0x60d336
// Updates a living citizen's water ripples when the effect detail allows them
// (no address of its own: the same body as EEUCBasicCitizen::UpdateEffects, which ICF kept)
void EEUCTransport::UpdateEffects(EEComplexUnit* inUnit)
{
    if (this->mRippleEffect != NULL && inUnit->mHitPoints != 0 && (unsigned long)this->mRippleEffect->mDetailLevel <= gEffectDetail)
        this->UpdateRipples(inUnit);
}
