// New (port): the continent manager's regions in a saved game (save version 0.263, docs/RUST_PLAN.md R5a.2 "load
// equals continue"; ee/EESaveGameFiles.h EESaveHasContinentRegions).
//
// The original rebuilt the continent manager at every load (EEWorld::StartGame: Clear, then BuildRegions over the
// map as it is then). The running game built it once at its start and repaired it since (RepairPathArea as tiles
// change, forests losing trees): its path areas, sectors, sub sectors and choke points are not those of a fresh
// build. Its area serials were not either: a load numbered the herds and towns before the regions, the running game
// after them. The computer players' tasks name areas by serial and tile (EECPILocation), and a load re-found them
// (Restore calls Update): a choke point task became a continent task (the load had no choke point yet when it
// restored the tasks, and its own choke points are others), a town task of a town not its player's lost its area.
//
// From 0.263 a save holds the manager as it is (EEPWorldHelper::Save, after the dead attackers): every area with its
// serial, tiles and hull, the sectors and sub sectors with their links, the grids, counters and free ids, the map
// tiles' AI values (EETile::mAIValue: the path searches' tile costs) and the serials of the herds and towns. A load
// makes the regions from it where it ran BuildRegions (EEWorld::StartGame, EERestoreContinentRegions): the continents
// and hills Clear made take their saved contents, the path areas, forests, choke points, sectors and sub sectors are
// the saved ones, the herds and towns the load made again take their saved serials. The computer players' locations
// keep their saved areas from the start of the load to its first tick (gLoadKeepsLocations, ee/EECPILocation.h). An
// older save loads as before.
//
// The block's writer and reader are Rust (ee_sim::astar cm_io.rs, over the records of the manager and its objects);
// this file is their host (the objects the reader makes, an area's Contains, the towns and herds, the map tiles'
// AI values) and the save's FSFile part. The dump's digest (EEContinentRegionsDigest, TEST ONLY) is the same bytes
// a save writes.
#include <windows.h>
#include <string.h>
#include <vector>
#include <set>
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
#include "lle/USingleton.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEAIPathSector.h"
#include "ee/EEAIRectangleArea.h"
#include "ee/EEAIForest.h"
#include "ee/EEAIHill.h"
#include "ee/EEAIChokePoint.h"
#include "ee/EETown.h"
#include "ee/EEHerd.h"
#include "ee/EESaveGameFiles.h"

extern long& gEEAIRegionSize;

namespace {

std::vector<unsigned char> sLoadedRegions;   // the block of the save being loaded (EELoadContinentRegions)

template <class T> size_t OffsetOf(const T* inObject, const void* inMember)
{
    return (size_t)((const char*)inMember - (const char*)inObject);
}

void* NewObject(long inTable)
{
    switch ((EeAStarCmTable)inTable) {
    case EeAStarCmTable::Continents: return new EEAIContinent;
    case EeAStarCmTable::PathSectors: return new EEAIPathSector;
    case EeAStarCmTable::PathSubSectors: return new EEAIPathSubSector;
    case EeAStarCmTable::Forests: return new EEAIForest;
    case EeAStarCmTable::PathAreas: return new EEAIPathArea;
    case EeAStarCmTable::Hills: return new EEAIHill;
    case EeAStarCmTable::ChokePoints: return new EEAIChokePoint;
    case EeAStarCmTable::RectangleAreas: return new EEAIRectangleArea;
    }
    return NULL;
}

void DeleteObject(long inTable, void* inObject)
{
    switch ((EeAStarCmTable)inTable) {
    case EeAStarCmTable::Continents: delete (EEAIContinent*)inObject; break;
    case EeAStarCmTable::PathSectors: delete (EEAIPathSector*)inObject; break;
    case EeAStarCmTable::PathSubSectors: delete (EEAIPathSubSector*)inObject; break;
    case EeAStarCmTable::Forests: delete (EEAIForest*)inObject; break;
    case EeAStarCmTable::PathAreas: delete (EEAIPathArea*)inObject; break;
    case EeAStarCmTable::Hills: delete (EEAIHill*)inObject; break;
    case EeAStarCmTable::ChokePoints: delete (EEAIChokePoint*)inObject; break;
    case EeAStarCmTable::RectangleAreas: delete (EEAIRectangleArea*)inObject; break;
    }
}

// the town or herd an area of the serial table is (its player and id), else none
void TownOrHerd(const EEAIArea* inArea, EeAStarCmIo* io)
{
    io->i[0] = 0;
    io->i[1] = -1;
    io->i[2] = -1;
    for (unsigned long p = 0; p < gWorld.mNumPlayers; p++) {
        EEPlayer* thePlayer = gWorld.mPlayers[p];
        if (thePlayer == NULL)
            continue;
        for (size_t i = 0; i < thePlayer->mTowns.size(); i++)
            if (thePlayer->mTowns[i] == inArea) {
                io->i[0] = 7; io->i[1] = thePlayer->mTowns[i]->mTownID; io->i[2] = (long)p; return;
            }
        for (size_t i = 0; i < thePlayer->mHerds.size(); i++)
            if (thePlayer->mHerds[i] == inArea) {
                io->i[0] = 8; io->i[1] = thePlayer->mHerds[i]->mID; io->i[2] = (long)p; return;
            }
    }
}

int64_t EEAIContinentHost(void*, int32_t inOp, EeAStarCmIo* io)
{
    EEMap* theMap = gWorld.mMap;
    switch (inOp) {
    case EE_ASTAR_CM_CONTAINS:
        return ((EEAIArea*)(uintptr_t)io->h[0])->Contains(U2DPoint<long>((long)io->i[0], (long)io->i[1]));
    case EE_ASTAR_CM_OWNER:
        TownOrHerd((const EEAIArea*)(uintptr_t)io->h[0], io);
        return 0;
    case EE_ASTAR_CM_REGION_SIZE: return gEEAIRegionSize;
    case EE_ASTAR_CM_SET_REGION_SIZE: gEEAIRegionSize = (long)io->i[0]; return 0;
    case EE_ASTAR_CM_MAP_SIZE: return theMap != NULL ? (int64_t)theMap->mSize : -1;
    case EE_ASTAR_CM_AI_VALUE: {
        const EETile* theTile = theMap != NULL ? theMap->GetTile((long)io->i[0], (long)io->i[1]) : NULL;
        return theTile != NULL ? theTile->mAIValue : 0;
    }
    case EE_ASTAR_CM_SET_AI_VALUE: {
        EETile* theTile = theMap != NULL ? theMap->GetTile((long)io->i[0], (long)io->i[1]) : NULL;
        if (theTile != NULL)
            theTile->mAIValue = (signed char)io->i[2];
        return 0;
    }
    case EE_ASTAR_CM_HAS_TILE: return theMap != NULL && theMap->GetTileAtLocation(U2DPoint<long>((long)io->i[0], (long)io->i[1])) != NULL;
    case EE_ASTAR_CM_RANDOM: return EEGetRandomInt(0, (long)io->i[0], *(URandom*)(uintptr_t)io->h[0]);
    case EE_ASTAR_CM_FOREST_REMOVE_TILE:
        return ((EEAIForest*)(uintptr_t)io->h[0])->RemoveTile(U2DPoint<long>((long)io->i[0], (long)io->i[1]));
    case EE_ASTAR_CM_TILE_KIND: return EEAIGetTileKind(EETileLocation((long)io->i[0], (long)io->i[1]));
    case EE_ASTAR_CM_WALL_ID: return EEAIGetTileWallID(EETileLocation((long)io->i[0], (long)io->i[1]));
    case EE_ASTAR_CM_TERRAIN: {
        const EETile* theTile = theMap != NULL ? theMap->GetTile((long)io->i[0], (long)io->i[1]) : NULL;
        return theTile != NULL ? (int64_t)theTile->mTerrainType : -1;
    }
    case EE_ASTAR_CM_AREA_CREATE: {
        const U2DPoint<long>* thePoints = (const U2DPoint<long>*)(uintptr_t)io->h[1];
        ((EEAIArea*)(uintptr_t)io->h[0])->Create(std::vector<U2DPoint<long> >(thePoints, thePoints + io->i[0]));
        return 0;
    }
    case EE_ASTAR_CM_AREA_REMOVE_TILE:
        return ((EEAIArea*)(uintptr_t)io->h[0])->RemoveTile(U2DPoint<long>((long)io->i[0], (long)io->i[1]));
    case EE_ASTAR_CM_RESOURCE_WEIGHT: return gContinentManager.GetResourceWeight((long)io->i[0], (long)io->i[1]);
    case EE_ASTAR_CM_NEW: return (int64_t)(uintptr_t)NewObject((long)io->i[0]);
    case EE_ASTAR_CM_DELETE: DeleteObject((long)io->i[0], (void*)(uintptr_t)io->h[0]); return 0;
    }
    return 0;
}

}   // namespace

// The records' places in their objects and the host (EEAIContinentManager's ctor)
void EEAIRegisterContinentState()
{
    EEAIContinent theContinent;
    EEAIPathArea thePathArea;
    EEAIForest theForest;
    EEAIHill theHill;
    EEAIChokePoint theChokePoint;
    EEAIRectangleArea theRectangle;
    EEAIPathSector theSector;
    EEAIPathSubSector theSubSector;
    EEAIArea* theArea = &theContinent;
    EeAStarCmLayout theLayout;
    theLayout.area_record = OffsetOf(theArea, theArea->AreaRecord());
    theLayout.area_tiles = OffsetOf(theArea, theArea->GetTileSet().GetHandleSlot());
    theLayout.continent = OffsetOf(&theContinent, theContinent.Record());
    theLayout.path_area = OffsetOf(&thePathArea, thePathArea.Record());
    theLayout.forest = OffsetOf(&theForest, theForest.Record());
    theLayout.hill = OffsetOf(&theHill, theHill.Record());
    theLayout.choke = OffsetOf(&theChokePoint, theChokePoint.Record());
    theLayout.rect = OffsetOf(&theRectangle, theRectangle.Record());
    theLayout.sector = OffsetOf(&theSector, theSector.Record());
    theLayout.sub_sector = OffsetOf(&theSubSector, theSubSector.Record());
    theLayout.sub_tiles = OffsetOf(&theSubSector, theSubSector.mTiles.GetHandleSlot());
    // every kind's EEAIArea is its first base: an object's address is its area's
    if ((void*)static_cast<EEAIArea*>(&thePathArea) != (void*)&thePathArea
        || (void*)static_cast<EEAIArea*>(&theForest) != (void*)&theForest
        || (void*)static_cast<EEAIArea*>(&theHill) != (void*)&theHill
        || (void*)static_cast<EEAIArea*>(&theChokePoint) != (void*)&theChokePoint
        || (void*)static_cast<EEAIArea*>(&theRectangle) != (void*)&theRectangle
        || OffsetOf(&thePathArea, static_cast<EEAIArea*>(&thePathArea)->AreaRecord()) != theLayout.area_record)
        abort();
    ee_astar_cm_set_layout(&theLayout);
    ee_astar_cm_set_host(EEAIContinentHost, NULL);
}

void EEWriteAreaBytes(const EEAIArea& inArea, std::vector<unsigned char>& outBytes)
{
    EeAStarCmBytes* theBytes = ee_astar_cm_area_bytes((uint64_t)(uintptr_t)&inArea);
    size_t theCount;
    const uint8_t* theData = ee_astar_cm_bytes_data(theBytes, &theCount);
    outBytes.assign(theData, theData + theCount);
    ee_astar_cm_bytes_free(theBytes);
}

bool EEReadAreaBytes(const std::vector<unsigned char>& inBytes, EEAIArea& outArea)
{
    return ee_astar_cm_read_area((uint64_t)(uintptr_t)&outArea, inBytes.empty() ? NULL : &inBytes[0], inBytes.size());
}

long EESaveContinentRegions(FSFile& outFile)
{
    EeAStarCmBytes* theBytes = ee_astar_cm_write_regions(&gContinentManager.mRecord);
    size_t theCount;
    const uint8_t* theData = ee_astar_cm_bytes_data(theBytes, &theCount);
    unsigned long theWritten = 0;
    long theResult = outFile.Write((unsigned long)theCount);
    if (theResult == kErrorOK && theCount != 0)
        theResult = outFile.Write((const unsigned char*)theData, (unsigned long)theCount, theWritten);
    ee_astar_cm_bytes_free(theBytes);
    return theResult;
}

long EELoadContinentRegions(FSFile& inFile)
{
    unsigned long theSize = 0;
    unsigned long theRead = 0;
    sLoadedRegions.clear();
    long theResult = inFile.Read(theSize);
    if (theResult == kErrorOK && theSize > 0) {
        sLoadedRegions.resize(theSize);
        theResult = inFile.Read(&sLoadedRegions[0], theSize, theRead);
        if (theResult == kErrorOK && theRead != theSize)
            theResult = kErrorBeyondEnd;
    }
    if (theResult != kErrorOK)
        sLoadedRegions.clear();
    return theResult;
}

bool EEContinentRegionsPending()
{
    return !sLoadedRegions.empty();
}

bool EERestoreContinentRegions()
{
    if (sLoadedRegions.empty())
        return false;
    std::vector<unsigned char> theBytes;
    theBytes.swap(sLoadedRegions);
    EEAIContinentManager& theManager = gContinentManager;
    EeAStarCmLoad* theLoad = ee_astar_cm_read_regions(&theManager.mRecord, &theBytes[0], theBytes.size());
    if (theLoad == NULL) {
        // a damaged block: the regions are built as an older save's load builds them
        ESDebugOut("EEAIContinentSave: the saved regions could not be read: the load builds its own\n");
        theManager.Clear(gWorld.mMap);
        theManager.BuildRegions(gWorld.mMap);
        for (unsigned long p = 0; p < gWorld.mNumPlayers; p++)
            if (gWorld.mPlayers[p] != NULL) {
                gWorld.mPlayers[p]->RegisterHerds();
                gWorld.mPlayers[p]->RegisterTowns();
            }
        return false;
    }
    EeAStarCmLoadInfo theInfo = ee_astar_cm_load_info(theLoad);
    if (theInfo.unknown_owners != 0)
        ESDebugOut("EEAIContinentSave: %u serials named areas of no known owner in the saved game\n", theInfo.unknown_owners);
    if (theInfo.unfiled_sub_sectors != 0)   // no sector held them in the saved game (not owned there either): kept
        ESDebugOut("EEAIContinentSave: %u saved sub sectors are in no sector\n", theInfo.unfiled_sub_sectors);
    // the herds and towns the load made again: their saved serials (by player and id), else the load's
    std::map<std::pair<long, long>, unsigned long> theSaved[2];
    for (int k = 0; k < 2; k++) {
        size_t theCount;
        const EeAStarCmOwned* theOwned = ee_astar_cm_load_owned(theLoad, k == 1, &theCount);
        for (size_t i = 0; i < theCount; i++)
            theSaved[k][std::make_pair((long)theOwned[i].player, (long)theOwned[i].index)] = (unsigned long)theOwned[i].serial;
    }
    ee_astar_cm_load_free(theLoad);
    unsigned long theUnmatched = 0;
    for (unsigned long p = 0; p < gWorld.mNumPlayers; p++) {
        EEPlayer* thePlayer = gWorld.mPlayers[p];
        if (thePlayer == NULL)
            continue;
        for (int k = 0; k < 2; k++) {
            size_t theCount = k == 0 ? thePlayer->mTowns.size() : thePlayer->mHerds.size();
            for (size_t t = 0; t < theCount; t++) {
                EEAIArea* theArea = k == 0 ? (EEAIArea*)thePlayer->mTowns[t] : (EEAIArea*)thePlayer->mHerds[t];
                if (theArea == NULL)
                    continue;
                long theID = k == 0 ? thePlayer->mTowns[t]->mTownID : (long)thePlayer->mHerds[t]->mID;
                std::map<std::pair<long, long>, unsigned long>::iterator theIt = theSaved[k].find(std::make_pair((long)p, theID));
                if (theIt != theSaved[k].end()) {
                    theArea->SetSerial(theIt->second);
                    theSaved[k].erase(theIt);
                } else
                    theUnmatched++;
                theManager.AddArea(theArea);
            }
        }
    }
    if (theUnmatched != 0 || !theSaved[0].empty() || !theSaved[1].empty())
        ESDebugOut("EEAIContinentSave: %lu herds and towns of the load are not the saved ones (%lu saved towns, %lu saved herds left)\n",
                   theUnmatched, (unsigned long)theSaved[0].size(), (unsigned long)theSaved[1].size());
    theManager.mRecord.clearing = theInfo.clearing;
    // an area of the load past the saved counter (a herd or town the saved game did not have) keeps the counter
    EEAIListView<EeAStarCmArea> theAreas = theManager.GetAreas();
    if (!theAreas.empty() && theAreas[theAreas.size() - 1].serial > theInfo.last_serial) {
        ESDebugOut("EEAIContinentSave: an area of the load has serial %lu past the saved %lu: the counter stays\n",
                   (unsigned long)theAreas[theAreas.size() - 1].serial, (unsigned long)theInfo.last_serial);
        theManager.mRecord.last_serial = theAreas[theAreas.size() - 1].serial;
    } else
        theManager.mRecord.last_serial = theInfo.last_serial;
    return true;
}

unsigned long EEContinentRegionsDigest()
{
    return ee_astar_cm_regions_digest(&gContinentManager.mRecord);
}
