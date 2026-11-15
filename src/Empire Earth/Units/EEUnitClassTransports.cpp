// EEUnitClassTransports.cpp (new, R5b.4 round 32): the transports' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <set>

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
#include "lle/GE3DLine.h"
#include "lle/SESoundManager.h"
#include "lle/GEBoundingBox.h"
#include "lle/GEModel.h"

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
#include "ee/EEUnitClassKinds.h"
#include "ee/EEUGRust.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EmpireEarth.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEEffectManager.h"

// ---- EEUCTransport.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The transports' base
// unit class: 0x21b27a..0x21c9e1 (after EEUCTorpedo.cpp). Unit flags: the exe's module set. Its ctor is out of line in
// EEUnitClass.cpp (0x23629e); slots +0xe0, +0xe4, +0xe8, the loading helpers (two EEComplexUnit distance inlines among
// them, first emitted here), +0xf4, +0xfc, +0xf8, +0x78, +0x100, +0x104, +0xb8 in address order. The untagged
// functions 0x21c9e1..0x21ce6c before EEUGAttackGround.cpp's key chain are not placed yet (another unit's?).

// unit-local constants (provisional names)
static const float kTransportNoPath = 10000.0f;   // 0x44736c (EEGetPathLengthTo: no path)

static const float kTransportNearSquared = 25.0f; // 0x447370

// FUNCTION: EXE 0x61b27a
// Returns the index of the unit nearest a tile and puts that unit's tile in outTile (0, nothing set, for no units)
long EEUnitClass::TransportFindGatheringTile(std::vector<EEComplexUnit*>* inUnits, U2DPoint<long> inFrom, U2DPoint<long>* outTile)
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
void EEUnitClass::TransportFindRendezvousTiles(std::vector<EEComplexUnit*>* inUnits, EEComplexUnit* inTransport,
                              U2DPoint<long>* outTransportTile, U2DPoint<long>* outUnitTile)
{
    *outTransportTile = U2DPoint<long>(inTransport->GetTileX(), inTransport->GetTileY());
    *outUnitTile = *outTransportTile;
}

// FUNCTION: EXE 0x61b38e
// Picks the unit's boarding tile: its own when the transport can unload there, else the nearest free one within
// two tiles, else the transport's tile (naval and space transports) or the unit's own
// Returns nothing (no value in eax at the ret); compare EEUCSpaceTransport::FindRendezvousTiles
void EEUnitClass::FindMeetingTiles(EEComplexUnit* inTransport, EEComplexUnit* inUnit, U2DPoint<long>* ioTransportTile,
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
void EEUnitClass::GetRingTiles(long inX, long inY, std::vector<U2DPoint<long> >& outTiles)
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
bool EEUnitClass::TransportCanCarry(EEComplexUnit* inUnit1, EEComplexUnit* inUnit2)
{
    long theCategory = this->mCategory;
    return (theCategory == kUnitCategoryLandTransport && inUnit2->mUnitClass->mCanEnterLandTransports) || (theCategory == kUnitCategoryHelicopterTransport && inUnit2->mUnitClass->mCanEnterAirTransports)
        || (theCategory == kUnitCategoryNavalTransport && inUnit2->mUnitClass->mCanEnterSeaTransports) || (theCategory == kUnitCategorySpaceTransport && inUnit2->mUnitClass->mCanEnterSpaceTransports)
        || (theCategory == kUnitCategoryFortress && inUnit2->mUnitClass->mCanEnterFortresses);
}

// FUNCTION: EXE 0x61b613
// Returns whether the transport can pick a unit up now: on one continent when the squared path length is in load
// range or the unit is next to it; across water when the unit and transport are near its shore tile
bool EEUnitClass::CanLoad(EEComplexUnit* inTransport, EEComplexUnit* inUnit)
{
    EEUCNote theNote(8); theNote.C(this).U(inTransport).U(inUnit);   // TEST ONLY (UCBD)
    return theNote.R([&]() -> bool {
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
    }());
}

// TEMPLATE: EXE 0x61b819
// EEComplexUnit::DistanceSquaredToTile (inline in ee/EEUnit.h; COMDAT after its first user CanLoad)

// FUNCTION: EXE 0x61b83f
// Returns whether cargo unloaded here reaches the destination within unload range (squared path length, plus
// the transport's distance to the first cargo tile across water); siege towers path as towers
bool EEUnitClass::CanUnloadNear(EEComplexUnit* inTransport, EEComplexUnit* inCargo,
                             U2DPoint<long>* ioUnloadTile, U2DPoint<long>* ioFirstCargoTile,
                             U2DPoint<long>* inDestination)
{
    EEUCNote theNote(9); theNote.C(this).U(inTransport).U(inCargo).T(*ioUnloadTile).T(*ioFirstCargoTile).T(*inDestination);   // TEST ONLY (UCBD)
    bool theResult = theNote.R([&]() -> bool {
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
    }());
    theNote.S(inCargo);
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
void EEUnitClass::FindLandingTile(EEComplexUnit* inTransport, U2DPoint<long>* inUnloadTile,
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
                unsigned short theID = theContinent->GetID();
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
void EEUnitClass::FindShoreTile(EEComplexUnit* inTransport, const EETileCoordinate& inFrom,
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
                    unsigned short theID = theTo->GetID();
                    if (!gContinentManager.IsNextToArea(theTiles[i].GetX(), theTiles[i].GetY(), theID))
                        continue;
                    EEUpdateBestUnloadTile(inTransport, theTiles[i], *inTarget, inRestrictTerrain, &theBest, outTile);
                    if (theTiles[i] == inFrom)
                        theContainsFrom = true;
                }
                if (theContainsFrom)
                    EETestUnloadTile(inTransport, inFrom, *inTarget, inRestrictTerrain, outTile);
            }
        } else if (gContinentManager.IsNextToArea(inDestination->GetX(), inDestination->GetY(), theTo->GetID())) {
            *outTile = *inDestination;
        }
    } else if (theFromID != 0) {
        theTo->GetShoreInRegion(theFromID, theTiles, theKind);
        if (theTiles.size() != 0) {
            this->FindBestShoreTile(inTransport, inFrom, *inDestination, *inTarget, inRestrictTerrain, outTile);
        } else if (gContinentManager.mTileFlags[theTo->GetID() + theFrom->GetID() * gContinentManager.mFlagsWidth]) {
            theFrom->GetShoreInRegion(theFromID, theTiles, theKind);
            if (theTiles.size() != 0) {
                theContainsFrom = false;
                theBest = 0.0f;
                for (long i = theTiles.size() - 1; i >= 0; i--) {
                    unsigned short theID = theTo->GetID();
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
                    unsigned short theID = theFrom->GetID();
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
float EEUnitClass::FindBestShoreTile(EEComplexUnit* inTransport, const U2DPoint<long>& inFrom,
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
        if (theFromID != 0 && theTo->GetCategory() == kContinentCategoryLand) {
            theTo->GetShoreInRegion(theFromID, theTiles, theKind);
            theID = theTo->GetID();
            if (theTiles.size() == 0 &&
                gContinentManager.mTileFlags[theID + theFrom->GetID() * gContinentManager.mFlagsWidth]) {
                theFrom->GetShoreInRegion(theFromID, theTiles, theKind);
                if (theTiles.size() != 0) {
                    bool theContainsFrom = false;
                    theBest = 0.0f;
                    for (long i = theTiles.size() - 1; i >= 0; i--) {
                        unsigned short theToAreaID = theTo->GetID();
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
        } else if (theToID != 0 && theFrom->GetCategory() == kContinentCategoryLand) {
            theTo->GetShoreInRegion(theToID, theTiles, theKind);
            theID = theTo->GetID();
        } else {
            EEAIContinent* theSource = NULL;
            if (theToID == 0 && theFrom->GetCategory() == kContinentCategoryLand) {
                theSource = theFrom;
                theID = theTo == NULL ? 0 : theTo->GetID();
            } else if (theTo != NULL) {
                theID = theFrom->GetID();
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
bool EEUnitClass::TransportFindBoardingLocation(EEComplexUnit* inTransport, EEComplexUnit* inCargo,
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
bool EEUnitClass::TransportCanUnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    EEMapTile* theTile = ::gWorld.mMap->GetTile(inX, inY);
    return theTile != NULL && theTile->mTerrainType != kTerrainCliff && theTile->mTerrainType != kTerrainSpace
        && !::gWorld.mMap->IsEdgeTile(inX, inY);
}

// FUNCTION: EXE 0x61c7d1
// Sets up a new transport as any unit, then gives it its class's resource type and amount
void EEUnitClass::TransportUnitCreated(EEComplexUnit* inUnit)
{
    this->BaseUnitCreated(inUnit);
    inUnit->SetResourceType(this->mResourceType);
    if (inUnit->mResourceType != kResourceNone)
        inUnit->SetResourceAmount(this->mResourceAmount);
}

// FUNCTION: EXE 0x61c806
// Moves the units that cannot board (not commandable, calamity flags 0x10 or 1, already inside something, off
// the map or on deep water, or not carried by this class) from the list to outRejected
void EEUnitClass::TransportFilterLoadable(std::vector<EEComplexUnit*>* ioUnits, EEComplexUnit* inTransport,
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
void EEUnitClass::TransportUnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    bool theFlag;
    if (this->GetUnitType() == kUnitTypeSiege04Tower || this->GetUnitType() == kUnitTypeSiege06HeavyTower)
        theFlag = true;
    else
        theFlag = false;
    inTransport->UnloadCargoAtTile(inX, inY, theFlag);
}

// ---- EEUCLandTransport.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The land
// transport's unit class: 0x216de3..0x217085 (after EEUCLandPriest.cpp; EEUCMiningBuilding.cpp follows). Unit flags:
// the exe's module set. Its ctor is inline (vtable 0x448930); the slots +0xe4, +0xf4, +0xf8, +0x8c, +0xb8 in address
// order. +0x8c and +0xf4 are also EEUCFortress's (ICF).

// FUNCTION: EXE 0x616de3
// Picks where a land transport meets the unit FindGatheringTile picks: the tile on the line between them split by
// their top speeds, refined by FindMeetingTiles; the units board there too
void EEUnitClass::LandTransportFindRendezvousTiles(std::vector<EEComplexUnit*>* inUnits, EEComplexUnit* inTransport,
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
bool EEUnitClass::LandTransportCanCarry(EEComplexUnit* inUnit1, EEComplexUnit* inUnit2)
{
    bool theResult = false;
    long theX1 = inUnit1->mTileX;
    if (theX1 != -10000) {
        long theX2 = inUnit2->mTileX;
        if (theX2 != -10000) {
            long theY1 = inUnit1->mTileY;
            if (gContinentManager.GetContinentID(theX1, theY1) == gContinentManager.GetContinentID(theX2, inUnit2->mTileY)
                && this->TransportCanCarry(inUnit1, inUnit2))
                theResult = true;
            else
                theResult = false;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x616f8e
// Returns whether the transport can unload on a tile: on its own continent, and the common test allows it
bool EEUnitClass::LandTransportCanUnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    return gContinentManager.GetContinentID(inTransport->GetTileX(), inTransport->GetTileY()) == gContinentManager.GetContinentID(inX, inY)
        && this->TransportCanUnloadAt(inTransport, inX, inY);
}

// FUNCTION: EXE 0x616fea
// Unloads all cargo at once when the transport is badly hurt
void EEUnitClass::LandTransportBadlyHurt(EEUnit* inUnit)
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

// ---- EEUCHelicopterTransport.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The transport
// helicopter's unit class: 0x21339d..0x213866 (after EEUCHelicopterPriest.cpp; EEUCLand.cpp follows). One unit per
// EEUC* class in alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's
// factory); slots +0xe4, +0x24, +0xf0, +0xf8, +0x74, +0x78, +0xa0, +0xb8 in address order.

// 0x447030: the highest a transport helicopter flies (one .rdata copy per unit: a named const) (provisional name)
static const float kHelicopterTransportCeiling = 12.0f;

// 0x23ca26 (cdecl; EEUCHelicopter.cpp declares it the same way) (provisional name)
void EEFindLandingPoint(EEComplexUnit* inUnit, GE3DPoint* ioTarget);

// FUNCTION: EXE 0x61339d
// Picks the gathering tile near the transport's own tile; the units board there too
void EEUnitClass::HelicopterTransportFindRendezvousTiles(std::vector<EEComplexUnit*>* inUnits, EEComplexUnit* inTransport,
                                        U2DPoint<long>* outTransportTile, U2DPoint<long>* outUnitTile)
{
    U2DPoint<long> theFrom(inTransport->mTileX, inTransport->mTileY);
    this->FindGatheringTile(inUnits, theFrom, outTransportTile);
    *outUnitTile = *outTransportTile;
}

// FUNCTION: EXE 0x6133e8
// Returns the height to fly at towards a point: cruising height above the highest tile corner on the way, at least
// the point's height, at most 12
float EEUnitClass::HelicopterTransportGetHeightAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
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

// FUNCTION: EXE 0x6135c1
// Returns whether the transport can unload on a tile: not deep water (1) or cliff (9), and the common test allows it
bool EEUnitClass::HelicopterTransportCanUnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    EEMapTile* theTile = ::gWorld.mMap->GetTile(inX, inY);
    bool theResult = false;
    if (theTile != NULL) {
        if (theTile->mTerrainType != kTerrainDeepWater && theTile->mTerrainType != kTerrainCliff &&
            this->TransportCanUnloadAt(inTransport, inX, inY))
            theResult = true;
        else
            theResult = false;
    }
    return theResult;
}

// New (R5b.4, the fourteenth round): EEUCHelicopterTransport::ActionChanged's path to the goal when preparing to move
// (the original's statements; EEUnitUpdate.cpp's EE_UNITS_ACT_HELICOPTER_PATH calls it)
void EEHelicopterTransportPathToGoal(EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    EEUnitClass* theClass = inClass;
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          inUnit->mPhysicalModel.GetLocation().GetZ());
    EEUnitGoal* theGoal = inUnit->mGoalState;
    GE3DPoint theTarget;
    theGoal->GetDestination(inUnit, theTarget);
    if (theGoal->mKind == kGoalExplore)
        theTarget.SetZ(theClass->mCruiseAltitude);
    EEFindLandingPoint(inUnit, &theTarget);
    theClass->PathToward(inUnit, thePosition, theTarget, false);
}

// FUNCTION: EXE 0x613774
// Sets up a new transport helicopter as a transport, then lifts it to its flying height
void EEUnitClass::HelicopterTransportUnitCreated(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    this->TransportUnitCreated(inUnit);
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
long EEUnitClass::HelicopterTransportEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
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
        theResult = this->BaseEvaluateCondition(inUnit, inEvent);
        break;
    }
    if (theFlag)
        theResult = 1;
    return theResult;
}

// ---- EEUCNavalTransport.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The naval
// transport's unit class: 0x218a45..0x218e34 (after EEUCNavalPriest.cpp; EEUCPlanetSpaceship.cpp's +0x74 0x218e34
// follows). Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory; vtable 0x448718); its
// slots +0x04, +0xe8, +0xf8, +0xfc, +0x88, +0x104, +0xb8 in source order (Virtual08/0C/30/34/A0/E4/F4 are other
// units' copies by ICF).

// FUNCTION: EXE 0x618a5f
// Finds where the transport unloads for a destination: shore and landing tiles, then the destination when in range
// and reachable, else the last good tile on the line from the landing tile (the first one if none is good)
void EEUnitClass::NavalTransportFindUnloadTiles(EEComplexUnit* inTransport, U2DPoint<long>* inDestination,
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
        while (true) {
            if (theTile == theTiles.end())
                break;
            U2DPoint<long>* thePoint = &(*theTile);
            float theDistance = outUnloadTile->DistanceToSquaredF(*thePoint);
            EEMapTile* theMapTile = ::gWorld.mMap->GetTile(thePoint->GetX(), thePoint->GetY());
            // port: a tile off the map ends the line (as EEUnitClass::FindUnloadTiles)
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

// FUNCTION: EXE 0x618c48
// Returns whether the transport can unload at a tile: not deep water, cliff or space, both continents known, the base
// class agrees, and its land continent connects to the tile's (or its water body is flagged for it)
bool EEUnitClass::NavalTransportCanUnloadAt(EEComplexUnit* inTransport, long inX, long inY)
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
                theResult = this->TransportCanUnloadAt(inTransport, inX, inY);
                if (theResult) {
                    if (theFrom->GetCategory() == kContinentCategoryLand) {
                        unsigned short theToID = theTo->GetID();
                        unsigned short theFromID = theFrom->GetID();
                        theResult = this->ContinentsConnect(theFromID, theToID);
                    } else if (theFrom->GetCategory() == kContinentCategoryWater)
                        theResult = gContinentManager.mTileFlags[theFrom->GetID() * gContinentManager.mFlagsWidth + theTo->GetID()];
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x618d4b
// Returns whether the transport may stop: already sinking, at the path's end tile with its speed reached, or slow
bool EEUnitClass::NavalTransportHasArrived(EEComplexUnit* inUnit, float inValue)
{
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    bool theResult = false;
    if (inUnit->mCollisionState != 8) {
        unsigned long theCount = inUnit->mWaypoints.size();
        inUnit->GetNextWaypointPoint(thePosition);
        if (inUnit->GetTileX() == UMFloor(thePosition.GetX()) && inUnit->GetTileY() == UMFloor(thePosition.GetY())
            && (theCount > 1
                || (inUnit->GetQueueGoals(0).size() > 1 && inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalLocation))) {
            if (inUnit->mCollisionRadius < inValue)
                theResult = false;
            else
                theResult = true;
        } else if (inValue <= 0.05f) {
            theResult = true;
        }
    } else {
        theResult = true;
    }
    return theResult;
}


// ---- EEUCSpaceTransport.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The space
// transport's unit class: 0x21a6e0..0x21ab66 (after EEUCSpaceship.cpp; EEUCSubmergedNaval.cpp's +0x04 0x21ab66
// follows). Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory). Slots +0xe4, +0x24,
// +0x2c, +0xf4, +0xf8, EEUCTransport::ContinentsConnect, +0x78, +0x84, +0x98, +0xa0 in address order; +0x24/+0x2c/+0x78/+0xa0
// call the spaceship's bodies (helpers ICF-folded into EEUCSpaceship.cpp's slots).

// the spaceship unit's kSpaceshipHeight (0x447358): the bodies below are the class's own copies of EEUCSpaceship's
static const float kSpaceTransportHeight = 12.0f;

// FUNCTION: EXE 0x61a6e0
// Finds where a space transport meets its passengers: the base rule on one continent, else its shore tile and
// their landing tile, then the meeting tiles near them
void EEUnitClass::SpaceTransportFindRendezvousTiles(std::vector<EEComplexUnit*>* inUnits, EEComplexUnit* inTransport,
                                   U2DPoint<long>* outTransportTile, U2DPoint<long>* outUnitTile)
{
    U2DPoint<long> theGather;
    U2DPoint<long> theFrom(inTransport->mTileX, inTransport->mTileY);
    unsigned short theUnitContinent = gContinentManager.GetContinentID((*inUnits->begin())->mTileX, (*inUnits->begin())->mTileY);
    unsigned short theTransportContinent = gContinentManager.GetContinentID(inTransport->mTileX, inTransport->mTileY);
    this->FindGatheringTile(inUnits, theFrom, &theGather);
    if (theUnitContinent == theTransportContinent) {
        this->TransportFindRendezvousTiles(inUnits, inTransport, outTransportTile, outUnitTile);
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

// FUNCTION: EXE 0x61a838
// Returns whether the transport can carry a unit: both on the map, the base rule agrees and their continents connect
bool EEUnitClass::SpaceTransportCanCarry(EEComplexUnit* inUnit1, EEComplexUnit* inUnit2)
{
    bool theResult = false;
    if (inUnit1->mTileX != -10000 && inUnit2->mTileX != -10000) {
        unsigned short theFirst = gContinentManager.GetContinentID(inUnit1->mTileX, inUnit1->GetTileY());
        unsigned short theSecond = gContinentManager.GetContinentID(inUnit2->GetTileX(), inUnit2->GetTileY());
        if (this->TransportCanCarry(inUnit1, inUnit2) && this->ContinentsConnect(theFirst, theSecond))
            theResult = true;
        else
            theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x61a8aa
// Returns whether the transport can unload at a tile: not deep water, cliff or space, both continents known, the base
// class agrees, and its land continent connects to the tile's (or its space region is flagged for it)
bool EEUnitClass::SpaceTransportCanUnloadAt(EEComplexUnit* inTransport, long inX, long inY)
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
                theResult = this->TransportCanUnloadAt(inTransport, inX, inY);
                if (theResult) {
                    if (theFrom->GetCategory() == kContinentCategoryLand) {
                        unsigned short theToID = theTo->GetID();
                        unsigned short theFromID = theFrom->GetID();
                        theResult = this->ContinentsConnect(theFromID, theToID);
                    } else if (theFrom->GetCategory() == kContinentCategorySpace)
                        theResult = gContinentManager.mTileFlags[theFrom->GetID() * gContinentManager.mFlagsWidth + theTo->GetID()];
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x61a995
// Returns whether two continents connect: the same one, or a neighbour of the first that is the second or (not a
// cliff) is flagged as linked to it
bool EEUnitClass::ContinentsConnect(unsigned short inFrom, unsigned short inTo)
{
    std::vector<unsigned short> theIDs;
    bool theResult = inFrom == inTo;
    if (!theResult && inFrom != 0 && inTo != 0) {
        gContinentManager.GetAdjacentIDs(theIDs, inFrom);
        for (long i = theIDs.size() - 1; i >= 0 && !theResult; i--) {
            EEAIContinent* theContinent = gContinentManager.mContinents[theIDs[i]];
            if (theContinent != NULL
                && (theIDs[i] == inTo
                    || (theContinent->GetCategory() != kContinentCategoryCliff
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
void EEUnitClass::SpaceTransportUnitCreated(EEComplexUnit* inUnit)
{
    this->SpaceshipVirtual78(inUnit);
    inUnit->SetResourceType(this->mResourceType);
    if (inUnit->mResourceType != kResourceNone)
        inUnit->SetResourceAmount(this->mResourceAmount);
}

// FUNCTION: EXE 0x61aaa9
// Adds the transport to its own tile only
void EEUnitClass::SpaceTransportAddToTile(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = theMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->mUnits = theTile->AddUnit(theUnit, theTile->mUnits);
}

// FUNCTION: EXE 0x61aaf5
// Removes the transport from its own tile only
void EEUnitClass::SpaceTransportRemoveFromTile(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = theMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->RemoveUnit(theUnit);
}

// FUNCTION: EXE 0x61ab3b
// Answers dying done (always) and move preparation complete (has a path) as a spaceship; others to the base class
long EEUnitClass::SpaceTransportEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    if (inEvent == kTriggerDyingAnimationFinished || inEvent == kTriggerMovePreparationComplete)
        return this->SpaceshipVirtualA0(inUnit, inEvent);
    return this->BaseEvaluateCondition(inUnit, inEvent);
}

// The spaceship half: the class's own copies of EEUCSpaceship's slot bodies (identical source, so LINK's ICF folds
// them into 0x21a219/0x21a404/0x21a51a/0x21a59e; names.txt ICF aliases): the flying height, the sound with a
// doppler shift, the new unit at its flying height, and the dying / move-preparation triggers. Owner decision:
// the helpers were declared only.
float EEUnitClass::SpaceshipVirtual24(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
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
    theHeight = max(theHeight, theOffset);   // never below the cruise altitude (as EEUnitClass::GetHeightAt 0x61a219)
    return theHeight;
}

void EEUnitClass::SpaceshipVirtual2C(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
                              float* outPitch)
{
    this->BaseGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    EEGetDopplerPitch(inUnit, inSound, outVolume, outPan, outPitch);
}

void EEUnitClass::SpaceshipVirtual78(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    this->BaseUnitCreated(inUnit);
    thePosition.SetX(theTransformation.GetX());
    thePosition.SetY(theTransformation.GetY());
    thePosition.SetZ(theTransformation.GetZ());
    float theZ = this->GetHeightAt(inUnit, thePosition);
    theTransformation.SetZ(theZ);
    inUnit->SetTransformation(theTransformation, true);
    inUnit->SetHeight(theZ);
}

long EEUnitClass::SpaceshipVirtualA0(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    bool theDone;
    if (inEvent == kTriggerDyingAnimationFinished)
        theDone = true;
    else if (inEvent == kTriggerMovePreparationComplete)
        theDone = (long)inUnit->mWaypoints.size() > 0;
    else
        return this->BaseEvaluateCondition(inUnit, inEvent);
    if (theDone)
        theResult = 1;
    return theResult;
}

// ICF-FOLDED: EXE 0x618a5f
// Finds where the transport unloads for a destination: shore and landing tiles, then the destination when in range
// and reachable, else the last good tile on the line from the landing tile (the first one if none is good)
// (no address of its own: the same body as EEUCNavalTransport::FindUnloadTiles, which ICF kept)
void EEUnitClass::SpaceTransportFindUnloadTiles(EEComplexUnit* inTransport, U2DPoint<long>* inDestination,
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

// ---- EEUCFortress.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The fortress (a land
// transport that garrisons; unit type 0x313 is the teleporter): 0x21013b..0x21048f (after
// EEUCBuildingPriest.cpp; a globals chain opens the next unit at 0x21048f). Unit flags: the exe's module set. Its ctor
// is inline (EEUnitClass.cpp's factory).

// 0x4c8fa4 (.data, a word): the faith a teleporter spends per full unload (provisional name)
unsigned short gFortressUnloadCost = 100;

// FUNCTION: EXE 0x610153
// Unloads where the fortress stands; a teleporter unloads at the destination
void EEUnitClass::FortressFindUnloadTiles(EEComplexUnit* inTransport, U2DPoint<long>* inDestination,
                             U2DPoint<long>* outUnloadTile, U2DPoint<long>* outFirstCargoTile, U2DPoint<long>* outTile)
{
    if (this->mUnitType != kUnitTypeTeleporter) {
        *outTile = EETileCoordinate(inTransport->mTileX, inTransport->mTileY);
        *outFirstCargoTile = *outTile;
        *outUnloadTile = *outFirstCargoTile;
    } else {
        *outUnloadTile = *inDestination;
        *outFirstCargoTile = *outUnloadTile;
        *outTile = *outFirstCargoTile;
    }
}

// FUNCTION: EXE 0x6101f8
// Returns whether the player's population has room for the unit's cost
bool EEUnitClass::FortressHasRoom(EEComplexUnit* inUnit, EEComplexUnit* inCargo)
{
    EEPlayer* thePlayer = inUnit->mPlayer;
    EEUnitClass* theClass = inCargo->mUnitClass;
    bool theResult = thePlayer->GetTotalPopulation(false, false) + theClass->GetPopulationCost() <= thePlayer->GetPopulationLimit(0);
    return theResult;
}

// FUNCTION: EXE 0x610235
// Returns whether a tile can take the cargo: within unload range for a fortress, visible for a teleporter, and
// allowed by the transport rules
bool EEUnitClass::FortressCanUnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    if (this->mUnitType != kUnitTypeTeleporter) {
        GE2DPoint theCenter(inX + 0.5f, inY + 0.5f);
        float theRange = inTransport->mUnitClass->mUnloadRange;
        return inTransport->DistanceSquared2D(theCenter) <= theRange * theRange && this->LandTransportCanUnloadAt(inTransport, inX, inY);
    }
    EEMapTile* theTile = ::gWorld.mMap->GetTile(inX, inY);
    return theTile != NULL && theTile->IsVisibleTo(inTransport->GetPlayer()) && this->TransportCanUnloadAt(inTransport, inX, inY);
}

// FUNCTION: EXE 0x61030a
// Sets up a new fortress as built, with a 1 s regeneration timer
void EEUnitClass::FortressUnitCreated(EEComplexUnit* inUnit)
{
    inUnit->SetBuilt(0, 1, 1);
    this->BaseUnitCreated(inUnit);
    inUnit->SetRegenerationTimer(1000);
}

// FUNCTION: EXE 0x610357
// Answers the fall-finished trigger once it has sunk below -2; other triggers go to the common unit class
long EEUnitClass::FortressEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    switch (inEvent) {
    default:
        theResult = this->BaseEvaluateCondition(inUnit, inEvent);
        break;
    case kTriggerDyingAnimationFinished:
        if (inUnit->mPhysicalModel.GetLocation().GetZ() < -2.0f)
            theResult = 1;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x610391
// Unloads at a tile; a teleporter needs 100 faith and spends it in proportion to the cargo unloaded, else warns
// its owner (event 0x274e)
void EEUnitClass::FortressUnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    if (inTransport->mUnitClass->mUnitType == kUnitTypeTeleporter) {
        if (inTransport->GetPower() >= gFortressUnloadCost) {
            unsigned long theBefore = inTransport->mUnitClass->mTransportCapacity - inTransport->mCargoRoom;
            this->TransportUnloadAt(inTransport, inX, inY);
            long theCapacity = inTransport->mUnitClass->mTransportCapacity;
            unsigned long theAfter = theCapacity - inTransport->mCargoRoom;
            if (theBefore != theAfter)
            {
                float theFraction = (float)(theBefore - theAfter) / theCapacity;
                inTransport->SetPower(inTransport->GetPower() - UMFloatToLong(theFraction * gFortressUnloadCost));
            }
        } else {
            U2DPoint<long> theTile(inX, inY);
            inTransport->mPlayer->mEvents.AddEvent(theTile, 2, inTransport->GetPlayer(), 0x274e, false);
        }
    } else
        this->TransportUnloadAt(inTransport, inX, inY);
}

