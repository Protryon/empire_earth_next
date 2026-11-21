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
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEPlayer.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
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

extern long& gEEAIRegionSize;

namespace {

std::vector<unsigned char> sLoadedRegions;   // the block of the save being loaded (EELoadContinentRegions)

template <class T> size_t OffsetOf(const T* inObject, const void* inMember)
{
    return (size_t)((const char*)inMember - (const char*)inObject);
}


int64_t EEAIContinentHost(void*, int32_t inOp, EeAStarCmIo* io)
{
    EEMap* theMap = gWorld.mMap;
    switch (inOp) {
    case EE_ASTAR_CM_TOWN_ID:
        return ((EETown*)(uintptr_t)io->h[0])->mTownID;
    case EE_ASTAR_CM_CLEAR_MAP: gContinentManager.Clear(gWorld.mMap);
        return 0;
    case EE_ASTAR_CM_BUILD_MAP: gContinentManager.BuildRegions(gWorld.mMap);
        return 0;
    case EE_ASTAR_CM_REGISTER: io->i[1] == 0 ? gWorld.mPlayers[(size_t)io->i[0]]->RegisterHerds() : gWorld.mPlayers[(size_t)io->i[0]]->RegisterTowns();
        return 0;
    case EE_ASTAR_CM_DEBUG_OUT: ESDebugOut("%s", (const char*)(uintptr_t)io->h[0]);
        return 0;
    case EE_ASTAR_CM_REGION_SIZE: return gEEAIRegionSize;
    case EE_ASTAR_CM_SET_REGION_SIZE: gEEAIRegionSize = (long)io->i[0]; return 0;
    case EE_ASTAR_CM_SET_AI_VALUE: {
        EETile* theTile = theMap != NULL ? theMap->GetTile((long)io->i[0], (long)io->i[1]) : NULL;
        if (theTile != NULL)
            theTile->mAIValue = (signed char)io->i[2];
        return 0;
    }
    case EE_ASTAR_CM_RANDOM: return EEGetRandomInt(0, (long)io->i[0], *(URandom*)(uintptr_t)io->h[0]);
    case EE_ASTAR_CM_AREA_CREATE: {   // a herd's (the other kinds' are Rust's: cm_host.rs)
        const U2DPoint<long>* thePoints = (const U2DPoint<long>*)(uintptr_t)io->h[1];
        ((EEAIArea*)(uintptr_t)io->h[0])->Create(std::vector<U2DPoint<long> >(thePoints, thePoints + io->i[0]));
        return 0;
    }
    }
    return 0;
}

}   // namespace

// Checks that the C++ classes are views of the Rust objects (ee_sim::astar objects.rs: sizes and the records'
// places), registers the host (EEAIContinentManager's ctor)
template <class T> static void EEAICheckObject(EeAStarCmTable inTable, size_t inRecord)
{
    T* theObject = (T*)(uintptr_t)ee_astar_object_new(inTable);
    bool theOK = sizeof(T) == ee_astar_object_size(inTable) && OffsetOf(theObject, theObject->Record()) == inRecord;
    ee_astar_object_delete(inTable, (uint64_t)(uintptr_t)theObject);
    if (!theOK)
        abort();
}

void EEAIRegisterContinentState()
{
    EeAStarCmLayout theLayout = ee_astar_cm_layout();
    EEAIContinent* theArea = EEAIContinent::New();
    bool theOK = OffsetOf(static_cast<EEAIArea*>(theArea), theArea->AreaRecord()) == theLayout.area_record
        && OffsetOf(static_cast<EEAIArea*>(theArea), theArea->GetTileSet().GetHandleSlot()) == theLayout.area_tiles
        && (void*)static_cast<EEAIArea*>(theArea) == (void*)theArea;
    EEAIContinent::Delete(theArea);
    if (!theOK || sizeof(EEAIArea) != theLayout.area_tiles + sizeof(void*))
        abort();
    EEAICheckObject<EEAIContinent>(EeAStarCmTable::Continents, theLayout.continent);
    EEAICheckObject<EEAIPathArea>(EeAStarCmTable::PathAreas, theLayout.path_area);
    EEAICheckObject<EEAIForest>(EeAStarCmTable::Forests, theLayout.forest);
    EEAICheckObject<EEAIHill>(EeAStarCmTable::Hills, theLayout.hill);
    EEAICheckObject<EEAIChokePoint>(EeAStarCmTable::ChokePoints, theLayout.choke);
    EEAICheckObject<EEAIRectangleArea>(EeAStarCmTable::RectangleAreas, theLayout.rect);
    EEAICheckObject<EEAIPathSector>(EeAStarCmTable::PathSectors, theLayout.sector);
    EEAICheckObject<EEAIPathSubSector>(EeAStarCmTable::PathSubSectors, theLayout.sub_sector);
    ee_astar_cm_set_host(EEAIContinentHost, NULL);
    ee_astar_world_view_set_manager(&gContinentManager.mRecord);   // the A* view's (world_view.rs)
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
    std::vector<unsigned char> theBytes;
    theBytes.swap(sLoadedRegions);
    return !theBytes.empty() && ee_astar_cm_restore_regions(&gContinentManager.mRecord, &theBytes[0], theBytes.size());
}

unsigned long EEContinentRegionsDigest()
{
    return ee_astar_cm_regions_digest(&gContinentManager.mRecord);
}
