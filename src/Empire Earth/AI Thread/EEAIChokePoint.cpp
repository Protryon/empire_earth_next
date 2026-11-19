// C:\EEX\Empire Earth\AI Thread\EEAIChokePoint.cpp (file name inferred: no __FILE__; RTTI class EEAIChokePoint,
// vtable 0x439d3c). 0x1c740..0x1d846 (EEAIAStarTerrainStorage.cpp ends at 0x1c740; EEAIContinent's ctor starts at
// 0x1d846). The dtor, AddAreaSerials, then the choke point pass EEAIBuildChokePoints (EEAIContinentManager's terrain pass):
// a per-tile area serial grid (continents and big forests are 0x36 "kinds"), smoothed inPasses - 1 times so that the
// tiles between two areas get a flag (both in Rust: ee_astar choke.rs), then a flood fill (ee_sim::astar flood.rs, the choke point goal's) that turns each run of flagged tiles into an EEAIChokePoint of gContinentManager.
// The machine's COMDATs follow (ctor, dtor, the inline slots, Run and its steps, EEAIAStarMachineBase's inline ctor).
// File globals in .bss 0x4e5c44..0x4e5c54 (later definitions get lower addresses).
#include <windows.h>
#include <string.h>
#include <vector>
#include <map>
#include <math.h>
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
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIForest.h"
#include "ee/EEAIChokePoint.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIAStarView.h"   // the fill's view (ee_sim::astar flood.rs)

// EEAIContinentManager.cpp's inline (out of line here: 0x29ced)
long EEAIHasResourceUnit(EETile* inTile);

long gEEAIChokeHeight;
long gEEAIChokeWidth;
unsigned char* gEEAIChokeTileFlags;
uint32_t* gEEAIChokeTileSerials;   // (the pass's grids are fixed-width: the Rust smoothing reads them)
int32_t* gEEAIChokeTileKinds;

static void EEAIChokeGetTileInfo(EETile* inTile, long inX, long inY, EeChokeTileInfo* outInfo);
static void EEAIChokeCreate(EEMap* inMap);
static void EEAIChokeDone();

// Destroys the choke point (members only)
// Builds the continent manager's choke points: classifies every tile by area serial, smooths inPasses - 1
// times to flag the tiles between areas, flood-fills the flagged runs into choke points and frees the grids
// (Rust: ee_astar choke.rs build_grids does the classification (EEAIChokeGetTileSerial 0x1ccd0 over what
// EEAIChokeGetTileInfo reads of a tile) and the smoothing passes (EEAIChokeGetKinds 0x1ca7c, EEAIChokeSmooth
// 0x1c78c) into the pass's grids)
void EEAIBuildChokePoints(EEMap* inMap, long inPasses)
{
    long theSize = inMap->mSize;
    long theCount = theSize * theSize;
    delete[] gEEAIChokeTileKinds;
    gEEAIChokeTileKinds = new int32_t[theCount];
    delete[] gEEAIChokeTileSerials;
    gEEAIChokeTileSerials = new uint32_t[theCount];
    delete[] gEEAIChokeTileFlags;
    gEEAIChokeTileFlags = new unsigned char[theCount];
    std::vector<EeChokeTileInfo> theInfos(theCount);
    for (long x = theSize - 1; x >= 0; x--) {
        for (long y = theSize - 1; y >= 0; y--)
            EEAIChokeGetTileInfo(inMap->GetTile(x, y), x, y, &theInfos[x * theSize + y]);
    }
    ee_astar_choke_build_grids((int32_t)theSize, (int32_t)inPasses, theInfos.data(), gEEAIChokeTileKinds,
                               gEEAIChokeTileSerials, gEEAIChokeTileFlags);
    gEEAIChokeWidth = theSize;
    gEEAIChokeHeight = theSize;
    EEAIChokeCreate(inMap);
    delete[] gEEAIChokeTileKinds;
    gEEAIChokeTileKinds = NULL;
    delete[] gEEAIChokeTileSerials;
    gEEAIChokeTileSerials = NULL;
    delete[] gEEAIChokeTileFlags;
    gEEAIChokeTileFlags = NULL;
    EEAIChokeDone();
}

// Flood-fills every flagged non-area tile not yet in a choke point into a new choke point (the fill: ee_sim::astar
// flood.rs), then gives each one a serial and registers it with the continent manager
static void EEAIChokeCreate(EEMap* inMap)
{
    long theSize = inMap->mSize;
    EeAStarFlood* theFill = ee_astar_flood_new(EeAStarFloodKind::Choke, 9, theSize);
    EeAStarFloodState* theState = ee_astar_flood_state(theFill);
    EEAIAStarView theView;
    theView.SetMap(inMap);
    EeAStarViewCallbacks theCallbacks = theView.Callbacks();
    std::map<long, EEAIChokePoint*> theChokePoints;
    ee_astar_flood_init(theFill, theSize, theSize);
    ee_astar_flood_set_check_corners(theFill, false);
    for (long x = theSize - 1; x >= 0; x--) {
        for (long y = theSize - 1; y >= 0; y--) {
            long theIndex = x * theSize + y;
            if (!gEEAIChokeTileFlags[theIndex] || gEEAIChokeTileKinds[theIndex] == 0x36 || ee_astar_flood_id(theFill, x, y) != 0)
                continue;
            ee_astar_flood_run(theFill, x, y, inMap->mSize, inMap->mSize, &theCallbacks);
            // CreateChokePoint: a choke point of the fill's tiles (hull, bounds, unique area serials, continent) under
            // the next number
            long theNumber = theState->next_id;
            size_t theCount = 0;
            const EePoint* thePoints = ee_astar_flood_collect(theFill, (uint32_t)theNumber, &theCount);
            std::vector<U2DPoint<long> > theTiles;
            for (size_t i = 0; i < theCount; i++)
                theTiles.push_back(U2DPoint<long>(thePoints[i].x, thePoints[i].y));
            EEAIChokePoint* theChokePoint = new EEAIChokePoint;
            theChokePoint->SetIndex(theNumber);
            theChokePoint->SetAreaType(kAIAreaChokePoint);
            theChokePoint->Create(theTiles);
            theChokePoint->UpdateBounds(gWorld.mInfluenceTable);
            size_t theSerialCount = 0;
            const uint32_t* theSerials = ee_astar_flood_serials(theFill, &theSerialCount);
            theChokePoint->AddAreaSerials(std::vector<unsigned long>(theSerials, theSerials + theSerialCount));
            if (theTiles.size() > 0)
                theChokePoint->SetContinentID(gContinentManager.GetContinentID(theTiles.front().GetX(), theTiles.front().GetY()));
            EeAStarRect theBounds = EEAIAStarRectOf(theChokePoint->GetHullBounds());
            ee_astar_flood_finish(theFill, &theBounds);
            theChokePoints.insert(std::map<long, EEAIChokePoint*>::value_type(theNumber, theChokePoint));
            theState->next_id++;
        }
    }
    gContinentManager.ResizeTable(EeAStarCmTable::ChokePoints, theState->next_id);
    for (std::map<long, EEAIChokePoint*>::iterator theIt = theChokePoints.begin(); theIt != theChokePoints.end(); theIt++) {
        EEAIChokePoint* theChokePoint = theIt->second;
        theChokePoint->SetSerial(gContinentManager.NewSerial());
        gContinentManager.AddChokePoint(theChokePoint);
    }
    ee_astar_flood_free(theFill);
}

// What EEAIChokeGetTileSerial 0x1ccd0 read of a tile, in its order: nothing of a missing tile; the continent's
// category and serial; on land or cliff whether the tile has a wood resource, and then the forest the manager lists
// at it (GetForestAt's id, then the manager's entry under that id) with its serial and tile count. Rust decides.
static void EEAIChokeGetTileInfo(EETile* inTile, long inX, long inY, EeChokeTileInfo* outInfo)
{
    memset(outInfo, 0, sizeof(*outInfo));
    if (!inTile)
        return;
    outInfo->tile = true;
    EEAIContinent* theContinent = gContinentManager.GetContinentFromID(gContinentManager.GetContinentID(inX, inY));
    outInfo->category = theContinent->GetCategory();
    outInfo->continent_serial = (uint32_t)theContinent->GetSerial();
    if (outInfo->category != kContinentCategoryLand && outInfo->category != kContinentCategoryCliff)
        return;
    outInfo->resource = EEAIHasResourceUnit(inTile) != 0;
    if (!outInfo->resource)
        return;
    EETileLocation theTile(inX, inY);
    EEAIForest* theForest = gContinentManager.GetForestAt(theTile);
    unsigned short theID = theForest ? theForest->GetID() : 0;
    EEAIHandleView<EEAIForest> theForests = gContinentManager.GetForests();
    theForest = theForests[theID];
    if (theForest) {
        outInfo->forest = true;
        outInfo->forest_serial = (uint32_t)theForest->GetSerial();
        outInfo->forest_tiles = (int32_t)theForest->GetNumTiles();
    }
}

// an empty function (ICF-folded into 0xe123d; called last by EEAIBuildChokePoints) (provisional name)
static void EEAIChokeDone()
{
}
