// C:\EEX\Empire Earth\AI Thread\EECPPlacement.cpp (file name inferred: no __FILE__; RTTI class
// EECPPlacement, vtable 0x43c050; its key "EECPPlacement-Data" names the class). A computer player's build
// manager: 0x66af0..0x6e8d4 (the class-static key's guarded chain first, then three distance predicates built without
// /Og, the members in address order; the deque/vector/heap/sort COMDATs close the unit; EECPAttackSubGroupUnit.cpp ends
// at 0x66af0 and EECPCivilizationManager.cpp's key chain starts at 0x6e8d4).
//
// AI lane B, round 2: the class is Rust's (ee_sim::cp::placement, rust/sim/cp.h): every search, CanBuildAt, the "unit
// near" tests, the site tiles and the classes seen (the Rust object mRust), UnitAdded, Reset, Restore, the epoch's
// classes, EECPCountOwn and EECPNextPlaceSearch. What stays here: the members other files read (mTownCenterTile,
// mPlayer), the forwarders with their TEST ONLY PLCE scopes, the town tile's Load and Save (U2DPoint's), the two
// EETileDistanceLess comparators other units sort with, and ee_cp_place_host: the world the Rust bodies ask.
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <initializer_list>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"

#include "ee/EEAIFOUnitScreeners.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIContinent.h"
#include "ee/EETown.h"
#include "ee/EEAIChokePoint.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPPlacement.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EEUnitCollector.h"
#include "ee/EECPTrace.h"
#include "ee/EECPNext.h"
#include "ee/EESimCapture.h"
#include "ee/EECPPlaceCapture.h"
#include "rust/sim/cp.h"

// the class key (0x4e5d00, guard 0x4e5cf8 bit 1)
const UString EECPPlacement::sData("EECPPlacement-Data");

// Orders two (continent, town) pairs by the distance of the continent's centre from the reference tile
// Built without /Og (frame locals, `this` spilled to [ebp-0x3c]); the -4 -> -0x20 bool copy is the inline (tile, tile)
// operator's own result. NEAR: #pragma optimize("g", off) alone gives 45 (frame 0x20 vs 0x3c), not exact: removed
bool EETileDistanceLess::operator()(const std::pair<EEAIContinent*, EETown*>& inFirst,
    const std::pair<EEAIContinent*, EETown*>& inSecond) const
{
    const U2DPoint<long>& theFirstCenter = inFirst.first->GetHullCenter();
    EETileCoordinate theFirst;
    theFirst.mX = theFirstCenter.mX;
    theFirst.mY = theFirstCenter.mY;
    const U2DPoint<long>& theSecondCenter = inSecond.first->GetHullCenter();
    EETileCoordinate theSecond;
    theSecond.mX = theSecondCenter.mX;
    theSecond.mY = theSecondCenter.mY;
    return (*this)(theFirst, theSecond);
}

// Orders two choke points by the distance of their centre from the reference tile
// Built without /Og, as 0x466b6d
bool EETileDistanceLess::operator()(EEAIChokePoint* inFirst, EEAIChokePoint* inSecond) const
{
    const U2DPoint<long>& theFirstCenter = inFirst->GetHullCenter();
    EETileCoordinate theFirst;
    theFirst.mX = theFirstCenter.mX;
    theFirst.mY = theFirstCenter.mY;
    const U2DPoint<long>& theSecondCenter = inSecond->GetHullCenter();
    EETileCoordinate theSecond;
    theSecond.mX = theSecondCenter.mX;
    theSecond.mY = theSecondCenter.mY;
    return (*this)(theFirst, theSecond);
}

// ---------------------------------------------------------------------------------------------------------------
// The Rust bodies' context and world

static uint64_t EECPHandle(const void* inObject)
{
    return (uint64_t)(uintptr_t)inObject;
}

template <class T> static T* EECPObject(uint64_t inHandle)
{
    return reinterpret_cast<T*>((uintptr_t)inHandle);
}

EeCpPlaceCtx EECPPlaceContext(EEComputerPlayer* inPlayer)
{
    EeCpPlaceCtx theContext;
    theContext.player = EECPHandle(static_cast<EEPlayer*>(inPlayer));
    theContext.player_id = (uint32_t)inPlayer->mPlayerID;
    theContext.fields = static_cast<EeCpFields*>(inPlayer);
    EECPPlacement* thePlacement = inPlayer->mPlacement;
    theContext.town = thePlacement != NULL ? &thePlacement->mTownCenterTile.mX : NULL;
    theContext.placement = thePlacement != NULL ? thePlacement->mRust : NULL;
    theContext.capture = EESimCaptureRecord::Active() ? 1 : 0;
    return theContext;
}

static void EECPPush(EeCpPlaceIo* io, int64_t inA, int64_t inB)
{
    ee_cp_place_push(io->list, inA, inB);
}

static void EECPPushTiles(EeCpPlaceIo* io, const std::vector<U2DPoint<long> >& inTiles)
{
    for (size_t i = 0; i < inTiles.size(); i++)
        EECPPush(io, inTiles[i].mX, inTiles[i].mY);
}

static std::vector<U2DPoint<long> > EECPInputTiles(const EeCpPlaceIo* io)
{
    std::vector<U2DPoint<long> > theTiles;
    for (size_t i = 0; i < io->input_len; i++)
        theTiles.push_back(U2DPoint<long>((long)io->input[2 * i], (long)io->input[2 * i + 1]));
    return theTiles;
}

template <class T> static std::vector<T*> EECPInputObjects(const EeCpPlaceIo* io)
{
    std::vector<T*> theObjects;
    for (size_t i = 0; i < io->input_len; i++)
        theObjects.push_back(EECPObject<T>((uint64_t)io->input[2 * i]));
    return theObjects;
}

static EEAIArea* EECPArea(uint64_t inHandle, bool inChoke)
{
    if (inChoke)
        return EECPObject<EEAIChokePoint>(inHandle);
    return EECPObject<EEAIContinent>(inHandle);
}

extern "C" int64_t ee_cp_center_host(const EeCpPlaceCtx* inContext, int32_t inOp, uint64_t inA, uint64_t inB,
    EeCpPlaceIo* io);

// The placement's world (ee_sim::cp::place_game's EE_CP_PLACE_* operations): the original's calls, one each
extern "C" int64_t ee_cp_place_host(const EeCpPlaceCtx* inContext, int32_t inOp, uint64_t inA, uint64_t inB,
    EeCpPlaceIo* io)
{
    EEComputerPlayer* thePlayer = static_cast<EEComputerPlayer*>(EECPObject<EEPlayer>(inContext->player));
    const int64_t* v = io->v;
    switch (inOp) {
    case EE_CP_PLACE_EPOCH:
        return thePlayer->mTechnologies->mCurrentEpoch;
    case EE_CP_PLACE_PASSABILITY:
        return thePlayer->GetCitizenPassability();
    case EE_CP_PLACE_RANGE:
        io->f[0] = thePlayer->mUnitClassUpgrades[(long)v[0]]->mRange;
        return 0;
    case EE_CP_PLACE_TECH_KIND: {
        EETechTreeNode* theNode = thePlayer->GetTechTree()->Get((long)v[0]);
        if (theNode == NULL)
            return 0;
        io->v[0] = theNode->mData->mKind;
        return 1;
    }
    case EE_CP_PLACE_HAS_PATHFINDING:
        return gCivPowersManager.HasPower(thePlayer, kCivPowerPathfinding) ? 1 : 0;
    case EE_CP_PLACE_CLAIM_RANGE_ON:
        return EECPClaimRangeOn(thePlayer) ? 1 : 0;
    case EE_CP_PLACE_CLAIM_RANGE:
        return EECPClaimRange(thePlayer);
    case EE_CP_PLACE_OWN_UNITS:
        for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
            EECPPush(io, (int64_t)EECPHandle(theUnit), 0);
        return 0;
    case EE_CP_PLACE_NATURE_UNITS:
        if (gWorld.mPlayers[0] == NULL)
            return 0;
        for (EEUnit* theUnit = gWorld.mPlayers[0]->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
            EECPPush(io, (int64_t)EECPHandle(theUnit), 0);
        return 1;
    case EE_CP_PLACE_TOWNS:
        for (size_t i = 0; i < thePlayer->mTowns.size(); i++) {
            EETown* theTown = thePlayer->mTowns[i];
            if (theTown == NULL) {
                EECPPush(io, -1, 0);
                continue;
            }
            const U2DPoint<long>& theCenter = theTown->GetHullCenter();
            EECPPush(io, theTown->mContinent, (int64_t)((uint64_t)(uint32_t)theCenter.mX << 32 | (uint32_t)theCenter.mY));
        }
        return 0;
    case EE_CP_PLACE_CENTERS:
        for (size_t i = 0; i < thePlayer->mResourceCenters.size(); i++)
            EECPPush(io, (int64_t)EECPHandle(thePlayer->mResourceCenters[i]), 0);
        return 0;
    case EE_CP_PLACE_HAS_PLAYER0:
        return gWorld.mPlayers[0] != NULL ? 1 : 0;
    case EE_CP_PLACE_WORLD_UNIT:
        return (int64_t)EECPHandle(gWorld.GetUnit((unsigned long)v[0]));
    case EE_CP_PLACE_UNIT_INFO: {
        EEUnit* theUnit = EECPObject<EEUnit>(inA);
        EeCpUnitInfo& i = io->unit;
        i.tile[0] = (int32_t)theUnit->mTileX;
        i.tile[1] = (int32_t)theUnit->mTileY;
        i.id = (uint32_t)theUnit->mUnitID;
        EEUnitClass* theClass = theUnit->mUnitClass;
        i.has_class = theClass != NULL;
        if (theClass != NULL) {
            i.unit_type = (int32_t)theClass->mUnitType;
            i.tile_size = (int32_t)theClass->GetTileSize();
            i.category = (int32_t)theClass->mCategory;
            i.family = (int32_t)theClass->mFamily;
            i.technology = (int32_t)theClass->mTechnology;
            i.land_building = theClass->IsLandBuilding() ? 1 : 0;
            i.wall = theClass->mIsWall ? 1 : 0;
            i.drop_site = theClass->IsDropSite() ? 1 : 0;
        }
        i.hit_points = (int32_t)theUnit->mHitPoints;
        i.resource_amount = (int32_t)theUnit->mResourceAmount;
        i.resource_type = (int32_t)theUnit->mResourceType;
        i.valid = theUnit->mIsValid ? 1 : 0;
        i.has_tile = theUnit->mTile != NULL ? 1 : 0;
        i.has_tile_node = theUnit->GetTileNode() != NULL ? 1 : 0;
        i.player = EECPHandle(theUnit->mPlayer);
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        i.complex = EECPHandle(theComplex);
        if (theComplex != NULL) {
            i.complex_tile[0] = (int32_t)theComplex->mTileX;
            i.complex_tile[1] = (int32_t)theComplex->mTileY;
            i.complex_hit_points = (int32_t)theComplex->mHitPoints;
            i.complex_built = theComplex->mIsBuilt ? 1 : 0;
            i.facing = (uint32_t)theComplex->mFacing;
            i.previous_part = EECPHandle(theComplex->GetPreviousPart());
            i.next_part = EECPHandle(theComplex->GetNextPart());
        }
        return 0;
    }
    case EE_CP_PLACE_CLASS_COUNT:
        return (int64_t)EEUnitClassCount();
    case EE_CP_PLACE_CLASS_EXISTS:
        return gUnitClassTable[(long)v[0]] != NULL ? 1 : 0;
    case EE_CP_PLACE_CLASS_TILE_SIZE:
        return gUnitClassTable[(long)v[0]]->GetTileSize();
    case EE_CP_PLACE_CLASS_TERRAIN:
        return gUnitClassTable[(long)v[0]]->mTerrainTypes[(long)v[1]] ? 1 : 0;
    case EE_CP_PLACE_CLASS_TECHNOLOGY:
        return gUnitClassTable[(long)v[0]]->GetTechnology();
    case EE_CP_PLACE_HAS_MAP:
        return gWorld.mMap != NULL ? 1 : 0;
    case EE_CP_PLACE_MAP_FLAGS:
        return (int64_t)gWorld.mMap->mMapCharacteristics;
    case EE_CP_PLACE_TILE_TERRAIN: {
        EETile* theTile = gWorld.mMap->GetTileAtLocation(U2DPoint<long>((long)v[0], (long)v[1]));
        return theTile != NULL ? (int64_t)theTile->mTerrainType : -1;
    }
    case EE_CP_PLACE_MAP_TILE_TERRAIN: {
        EETile* theTile = gWorld.mMap->GetTile((long)v[0], (long)v[1]);
        return theTile != NULL ? (int64_t)theTile->GetTerrainType() : -1;
    }
    case EE_CP_PLACE_TILE_FREE: {
        EETile* theTile = gWorld.GetMap()->GetTileAtLocation(U2DPoint<long>((long)v[0], (long)v[1]));
        return theTile == NULL ? -1 : theTile->mUnits == NULL ? 1 : 0;
    }
    case EE_CP_PLACE_IS_EDGE:
        return gWorld.mMap->IsEdgeTile((long)v[0], (long)v[1]) ? 1 : 0;
    case EE_CP_PLACE_LARGEST_CONTINENT:
        return gContinentManager.GetLargestContinent((long)v[0]);
    case EE_CP_PLACE_CONTINENT_FROM_ID:
        return (int64_t)EECPHandle(gContinentManager.GetContinentFromID((unsigned short)v[0]));
    case EE_CP_PLACE_CONTINENT_BY_ID:
        return (int64_t)EECPHandle(gContinentManager.GetContinentByID((unsigned short)v[0]));
    case EE_CP_PLACE_CONTINENT_AT:
        return (int64_t)EECPHandle(gContinentManager.GetContinentAt((long)v[0], (long)v[1]));
    case EE_CP_PLACE_CONTINENT_ID:
        return EECPObject<EEAIContinent>(inA)->GetID();
    case EE_CP_PLACE_CONTINENT_FLAG:
        return gContinentManager.AreAdjacent((long)v[0], (long)v[1]) ? 1 : 0;
    case EE_CP_PLACE_SHORE_NEXT_TO: {
        std::vector<U2DPoint<long> > theTiles = EECPInputTiles(io);
        EECPObject<EEAIContinent>(inA)->GetShoreNextTo((unsigned short)v[0], theTiles);
        EECPPushTiles(io, theTiles);
        return 0;
    }
    case EE_CP_PLACE_SHORE_TILES: {
        std::vector<U2DPoint<long> > theTiles = EECPObject<EEAIContinent>(inA)->GetShoreTiles();
        EECPPushTiles(io, theTiles);
        return 0;
    }
    case EE_CP_PLACE_HULL_CENTER: {
        const U2DPoint<long>& theCenter = EECPArea(inA, v[0] != 0)->GetHullCenter();
        io->v[0] = theCenter.mX;
        io->v[1] = theCenter.mY;
        return 0;
    }
    case EE_CP_PLACE_ARE_CONNECTED:
        return gContinentManager.AreConnected(EECPObject<EEAIContinent>(inA), EECPObject<EEAIContinent>(inB), (long)v[0]) ? 1 : 0;
    case EE_CP_PLACE_TILE_CONTINENT:
        return gContinentManager.GetContinent(EETileCoordinate((long)v[0], (long)v[1]), kAIPathLandMass);
    case EE_CP_PLACE_UNIT_CONTINENT:
        return gContinentManager.GetContinent(EECPObject<EEUnit>(inA), kAIPathLandMass);
    case EE_CP_PLACE_CONTINENT_ID_AT:
        return gContinentManager.GetContinentID((long)v[0], (long)v[1]);
    case EE_CP_PLACE_CHOKE_AT:
        return gContinentManager.GetChokePointAt(EETileCoordinate((long)v[0], (long)v[1])) != NULL ? 1 : 0;
    case EE_CP_PLACE_HILL_AT:
        return gContinentManager.GetHillAt(EETileCoordinate((long)v[0], (long)v[1])) != NULL ? 1 : 0;
    case EE_CP_PLACE_PATH_AREA_AT:
        return (int64_t)EECPHandle(gContinentManager.GetPathAreaAt(EETileCoordinate((long)v[0], (long)v[1])));
    case EE_CP_PLACE_UNIT_PATH_AREA:
        return (int64_t)EECPHandle(gContinentManager.GetPathAreaAt(EECPObject<EEUnit>(inA)));
    case EE_CP_PLACE_CHOKE_POINTS: {
        std::vector<EEAIChokePoint*> theChokePoints;
        gContinentManager.GetChokePoints(theChokePoints, (unsigned short)v[0], true);
        for (size_t i = 0; i < theChokePoints.size(); i++)
            EECPPush(io, (int64_t)EECPHandle(theChokePoints[i]), 0);
        return 0;
    }
    case EE_CP_PLACE_HULL_BOUNDS: {
        const U2DRectangle<long>& theBounds = EECPObject<EEAIChokePoint>(inA)->GetHullBounds();
        io->v[0] = theBounds.mLeft;
        io->v[1] = theBounds.mTop;
        io->v[2] = theBounds.mRight;
        io->v[3] = theBounds.mBottom;
        return 0;
    }
    case EE_CP_PLACE_CHOKE_TILES: {
        std::vector<U2DPoint<long> > theTiles = EECPInputTiles(io);
        EECPObject<EEAIChokePoint>(inA)->GetTileSet().BuildPointList(theTiles, 1);
        EECPPushTiles(io, theTiles);
        return 0;
    }
    case EE_CP_PLACE_FACING_QUARTER:
        return EEGetFacingQuarter((long)v[0], (long)v[1]);
    case EE_CP_PLACE_SITE_THREAT:
        return EECPNextSiteThreat(thePlayer, (long)v[0], (long)v[1], false, NULL) != 0 ? 1 : 0;
    case EE_CP_PLACE_UNITS_IN_RECT:   // the houses' filters (the centres'; any unit: ee_units uu_ffi.rs collect_units)
        return ee_cp_center_host(inContext, inOp, inA, inB, io);
    case EE_CP_PLACE_RANDOM:
        return gPhysics->GetRandom(0L, (long)v[0]);
    case EE_CP_PLACE_SPACE_DOCK_OPEN:
        return EEIsSpaceDockSiteOpen((long)v[0], (long)v[1], thePlayer) ? 1 : 0;
    case EE_CP_PLACE_SORT: {
        // the original's std::sort with the comparator's key: the same comparisons, so the same order of ties
        std::vector<std::pair<int64_t, uint32_t> > theKeys;
        for (size_t i = 0; i < io->input_len; i++)
            theKeys.push_back(std::pair<int64_t, uint32_t>(io->input[i], (uint32_t)i));
        std::sort(theKeys.begin(), theKeys.end(),
            [](const std::pair<int64_t, uint32_t>& inFirst, const std::pair<int64_t, uint32_t>& inSecond) {
                return inFirst.first < inSecond.first;
            });
        for (size_t i = 0; i < theKeys.size(); i++)
            io->order[i] = theKeys[i].second;
        return 0;
    }
    }
    return 0;
}

// ---------------------------------------------------------------------------------------------------------------
// EECPPlacement: the members other files read, the Rust object, the forwarders

// Creates the build manager for a civilization manager's player, with empty queues and no town centre tile
EECPPlacement::EECPPlacement(EEComputerPlayer* inPlayer)
    : mPlayer(inPlayer)
{
    this->mTownCenterTile = U2DPoint<long>(-10000, -10000);
    this->mRust = ee_cp_placement_new();
}

EECPPlacement::~EECPPlacement()
{
    ee_cp_placement_free(this->mRust);
}

#define EECP_CONTEXT EeCpPlaceCtx theContext = EECPPlaceContext(this->mPlayer)

// the forwarders' PLCE records (TEST ONLY): a float argument's bits, a unit's id
static long EECPBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return (long)theBits;
}

// a test's forwarder: the PLCE record of its arguments and result around the Rust call
template <class F> static bool EECPAsk(unsigned int inKind, EEComputerPlayer* inPlayer, std::initializer_list<long> inArgs,
    F inCall)
{
    EECPPlaceScope theScope(inKind, inPlayer);   // PLCE (TEST ONLY)
    for (long theArg : inArgs)
        theScope.I(theArg);
    EeCpPlaceCtx theContext = EECPPlaceContext(inPlayer);
    bool theResult = inCall(&theContext);
    theScope.I(theResult);
    return theResult;
}

// a search's forwarder: the tile in and, found, out (else what the caller left there, as the original's searches did);
// the record's found tile and, for the searches over the centres, the player's centres
template <class F> static bool EECPSearch(unsigned int inKind, EEComputerPlayer* inPlayer, std::initializer_list<long> inArgs,
    EETileCoordinate& ioTile, bool inCenters, F inCall)
{
    EECPPlaceScope theScope(inKind, inPlayer);   // PLCE (TEST ONLY)
    for (long theArg : inArgs)
        theScope.I(theArg);
    EeCpPlaceCtx theContext = EECPPlaceContext(inPlayer);
    int32_t theTile[2] = {(int32_t)ioTile.mX, (int32_t)ioTile.mY};
    bool theResult = inCall(&theContext, theTile);
    if (theResult) {
        ioTile.mX = theTile[0];
        ioTile.mY = theTile[1];
    }
    theScope.I(theResult).I(theResult ? ioTile.mX : 0).I(theResult ? ioTile.mY : 0);
    if (inCenters)
        theScope.Centers(inPlayer);
    return theResult;
}

#define EECP_ASK(kind, args, call) return EECPAsk(kind, this->mPlayer, args, [&](const EeCpPlaceCtx* c) { return call; })
#define EECP_SEARCH(kind, args, tile, centers, call)                                                                    \
    return EECPSearch(kind, this->mPlayer, args, tile, centers, [&](const EeCpPlaceCtx* c, int32_t* t) { return call; })
#define EECP_ARGS(...) std::initializer_list<long>({__VA_ARGS__})

long EECPPlacement::Restore()
{
    EECPPlaceScope theScope(kPlceRestore, this->mPlayer);   // PLCE (TEST ONLY)
    EECP_CONTEXT;
    long theResult = ee_cp_placement_restore(&theContext);
    theScope.I(theResult).State(this);
    return theResult;
}

// the town centre tile (U2DPoint's own Load and Save)
long EECPPlacement::Load(FSFile& inFile)
{
    EECPPlaceScope theScope(kPlceLoad, this->mPlayer);   // PLCE (TEST ONLY)
    long theResult = this->mTownCenterTile.Load(inFile);
    theScope.I(theResult).State(this);
    return theResult;
}

long EECPPlacement::Save(FSFile& outFile)
{
    EECPPlaceScope theScope(kPlceSave, this->mPlayer);   // PLCE (TEST ONLY)
    theScope.State(this);
    long theResult = this->mTownCenterTile.Save(outFile);
    theScope.I(theResult);
    return theResult;
}

void EECPPlacement::Reset()
{
    EECPPlaceScope theScope(kPlceReset, this->mPlayer);   // PLCE (TEST ONLY)
    EECP_CONTEXT;
    ee_cp_placement_reset(&theContext);
    theScope.State(this);
}

void EECPPlacement::UnitAdded(EEComplexUnit* inUnit)
{
    EECPPlaceScope theScope(kPlceUnitAdded, this->mPlayer);   // PLCE (TEST ONLY)
    theScope.CU(inUnit);
    EECP_CONTEXT;
    ee_cp_unit_added(&theContext, EECPHandle(static_cast<EEUnit*>(inUnit)));
    theScope.State(this);
}

// ---------------------------------------------------------------------------------------------------------------
// The free functions

long EECPCountOwn(EEComputerPlayer* inPlayer, long inType)
{
    EECPPlaceScope theScope(kPlceCountOwn, inPlayer);   // PLCE (TEST ONLY)
    theScope.I(inType);
    EeCpPlaceCtx theContext = EECPPlaceContext(inPlayer);
    long theResult = ee_cp_count_own(&theContext, (int32_t)inType);
    theScope.I(theResult);
    return theResult;
}

// the epoch's classes (the original civilization manager's GetAntiAirClass .. GetWallClass)
#define EECP_EPOCH_CLASS(name, kind, call)                                                                              \
    long name(EEComputerPlayer* inPlayer)                                                                              \
    {                                                                                                                  \
        EECPPlaceScope theScope(kind, inPlayer);   /* PLCE (TEST ONLY) */                                              \
        long theResult = call((int32_t)inPlayer->mTechnologies->mCurrentEpoch);                                        \
        theScope.I(theResult);                                                                                         \
        return theResult;                                                                                              \
    }
EECP_EPOCH_CLASS(EECPAntiAirClass, kPlceAntiAirClass, ee_cp_anti_air_class)
EECP_EPOCH_CLASS(EECPFishingBoatClass, kPlceFishingBoatClass, ee_cp_fishing_boat_class)
EECP_EPOCH_CLASS(EECPGateClass, kPlceGateClass, ee_cp_gate_class)
EECP_EPOCH_CLASS(EECPTowerClass, kPlceTowerClass, ee_cp_tower_class)
EECP_EPOCH_CLASS(EECPWallClass, kPlceWallClass, ee_cp_wall_class)

// The placement searches of the Lua verbs (ai.place; ee_sim::cp::placement next_place_search)
bool EECPNextPlaceSearch(EEComputerPlayer* inPlayer, long inType, long inMode, long inX, long inY, long inFirst,
    long inLast, const std::vector<long>& inUnits, long inSite, long& outX, long& outY, long& outGap)
{
    EECPPlaceScope theScope(kPlceNextPlaceSearch, inPlayer);   // PLCE (TEST ONLY)
    theScope.I(inType).I(inMode).I(inX).I(inY).I(inFirst).I(inLast).I((long)inUnits.size());
    for (size_t theScopeK = 0; theScopeK < inUnits.size(); theScopeK++)
        theScope.I(inUnits[theScopeK]);
    theScope.I(inSite);
    EeCpPlaceCtx theContext = EECPPlaceContext(inPlayer);
    std::vector<int64_t> theUnits(inUnits.begin(), inUnits.end());
    EeCpPlaceResult theFound = ee_cp_next_place_search(&theContext, (int32_t)inType, (int32_t)inMode, (int32_t)inX,
        (int32_t)inY, (int32_t)inFirst, (int32_t)inLast, theUnits.empty() ? NULL : &theUnits[0], theUnits.size(),
        (int32_t)inSite);
    outX = theFound.x;
    outY = theFound.y;
    outGap = theFound.gap;
    bool theResult = theFound.found != 0;
    theScope.I(theResult).I(outX).I(outY).I(outGap).Centers(inPlayer);
    return theResult;
}
