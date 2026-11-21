// C:\EEX\Empire Earth\AI Thread\EEPathManager.cpp (file name inferred: no __FILE__; RTTI classes
// EEAutoPathRequest (vtable 0x43c430), EEPathManager (vtable 0x43c438), EEAIPathGoalModifierSpace/Sea
// (0x43c448/0x43c458), the three A* machines (ee/EEAIAStarMachine.h)). 0xa5c31..0xabbaf.
// The manager is Rust (ee_sim::astar pm.rs: the searches in progress and their nodes, the spare machines, the
// pending units, the counters, the request record, the requests' and the updates' decisions, the saved game's
// parts); this file is its host over the machines, units and requests, and keeps the bounded machine's searches.
#include <windows.h>
#include <stdio.h>
#include <typeinfo>
#include <math.h>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <algorithm>
#include <string.h>
#include <memory>
#include <functional>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEAIContinentManager.h"

#include "ee/EEPathManager.h"
#include "rust/sim/astar.h"   // ee_path_*: the path splicing (ee_astar path_splice.rs)
#include "rust/sim/units.h"

static_assert(sizeof(GE3DPoint) == sizeof(EePathPoint) && offsetof(GE3DPoint, mZ) == offsetof(EePathPoint, z),
              "GE3DPoint");

// The requests (EEAutoPathRequest) are Rust records (ee_sim::astar request.rs), with the owners waiting for a
// loaded one and their saved record

// ---- The manager in Rust (ee_sim::astar pm.rs): the searches in progress, the spare machines, the pending units,
// the counters and the request record, and every decision over them. The C++ answers its host's operations over
// the machines, the units and the requests, and keeps the bounded machine's searches (below).

static EEComplexUnit* EEPathUnitOf(uint64_t inHandle)
{
    return reinterpret_cast<EEComplexUnit*>(inHandle);
}

static uint64_t EEPathHandle(const void* inObject)
{
    return (uint64_t)(uintptr_t)inObject;
}

static EEComplexUnit* EEPathWorldUnit(int64_t inID)
{
    return dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)inID)));
}

// the loaded game's graph searches resolved after every owner's own fix-up (the active groups'; request.rs)
extern "C" void ee_path_after_load_last(void (*inFixup)())
{
    EEAfterLoadLast(inFixup);
}

// The Rust manager's host: one call per operation (pm.rs EE_PATH_*)
static int64_t EEPathManagerHost(void* inContext, int32_t inOp, EePathIo* io)
{
    int64_t* i = io->i;
    const uint64_t h = io->h[0];
    switch (inOp) {
    case EE_PATH_WORLD_UNIT:
        return (int64_t)EEPathHandle(EEPathWorldUnit(i[0]));
    case EE_PATH_CLOSEST_TILE: {   // ClampTile's (pm_apply.rs): the tile of map h1 closest to the point of float bits i0, i1
        const uint32_t theBits[2] = {(uint32_t)i[0], (uint32_t)i[1]};
        float theXY[2];
        memcpy(theXY, theBits, sizeof(theXY));
        GE3DPoint thePoint(theXY[0], theXY[1], 0.0f);
        long theX = 0, theY = 0;
        reinterpret_cast<EEMap*>(io->h[1])->GetClosestTile(&thePoint, &theX, &theY);
        i[0] = theX;
        i[1] = theY;
        return 0;
    }
    // the tile machines: Rust (path_search.rs)
    // the graph machines: Rust (graph_search.rs)
    // the units
    case EE_PATH_UNIT_STRANDED:
        EEPathUnitOf(h)->mPlayer->UnitStranded(EEPathUnitOf(h));
        return 0;
    // the regions' rebuild and the requests: Rust (cm_passes.rs, request.rs; answered in pm_host.rs)
    }
    return 0;
}

// Creates the path manager with no bounded machine, nothing queued and region updates off
EEPathManager::EEPathManager() : mRust(ee_path_manager_new(EEPathManagerHost, this)), mLC8(5)
{
}

// Destroys the path manager, freeing every machine, queued search, region map and the node pools
EEPathManager::~EEPathManager()
{
    this->Clear();
    ee_path_manager_free(this->mRust);
    this->mRust = NULL;
}

// one call of the Rust manager (pm.rs ee_path_manager_call)
int64_t EEPathManager::Call(int32_t inWhat, uint64_t inH0, uint64_t inH1, std::initializer_list<int64_t> inValues,
                            int64_t* outValues)
{
    EePathIo theIO = {};
    theIO.h[0] = inH0;
    theIO.h[1] = inH1;
    int k = 0;
    for (int64_t v : inValues)
        theIO.i[k++] = v;
    int64_t theResult = ee_path_manager_call(this->mRust, inWhat, &theIO);
    if (outValues != NULL) {
        outValues[0] = theIO.i[0];
        outValues[1] = theIO.i[1];
    }
    return theResult;
}

// the save file through FSFile's typed calls (pm.rs EePathFileFn)
static int32_t EEPathFileWrite(void* inFile, uint32_t inKind, uint32_t* ioValue, uint8_t* inData, uint32_t inLength)
{
    FSFile& theFile = *static_cast<FSFile*>(inFile);
    switch (inKind) {
    case EE_PATH_FILE_U32: return (int32_t)theFile.Write(*ioValue);
    case EE_PATH_FILE_I32: return (int32_t)theFile.Write((int32_t)*ioValue);
    case EE_PATH_FILE_U16: return (int32_t)theFile.Write((unsigned short)*ioValue);
    case EE_PATH_FILE_BOOL: return (int32_t)theFile.Write(*ioValue != 0);
    case EE_PATH_FILE_F32: { float theValue; memcpy(&theValue, ioValue, 4); return (int32_t)theFile.Write(theValue); }
    default: { unsigned long theWritten = 0; return (int32_t)theFile.Write(inData, inLength, theWritten); }
    }
}

static int32_t EEPathFileRead(void* inFile, uint32_t inKind, uint32_t* outValue, uint8_t* outData, uint32_t inLength)
{
    FSFile& theFile = *static_cast<FSFile*>(inFile);
    long theResult = 0;
    switch (inKind) {
    case EE_PATH_FILE_U32: theResult = theFile.Read(*outValue); break;
    case EE_PATH_FILE_I32: { int32_t v = 0; theResult = theFile.Read(v); *outValue = (uint32_t)v; break; }
    case EE_PATH_FILE_U16: { unsigned short v = 0; theResult = theFile.Read(v); *outValue = v; break; }
    case EE_PATH_FILE_BOOL: { bool v = false; theResult = theFile.Read(v); *outValue = v; break; }
    case EE_PATH_FILE_F32: { float v = 0; theResult = theFile.Read(v); memcpy(outValue, &v, 4); break; }
    default: { unsigned long theRead = 0; theResult = theFile.Read(outData, inLength, theRead); *outValue = (uint32_t)theRead; break; }
    }
    return (int32_t)theResult;
}

// port (save version 0.256): the request record, the region maps, the timers and the counters (pm.rs RunningState)
void EEPathManager::SaveRunningState(FSFile& outFile)
{
    ee_path_manager_file(this->mRust, 0, false, EEPathFileWrite, &outFile);
}

long EEPathManager::LoadRunningState(FSFile& inFile)
{
    long theResult = ee_path_manager_file(this->mRust, 0, true, EEPathFileRead, &inFile);
    if (theResult == 0)
        EEAfterLoad([]() { gPathManager.Call(18); });
    return theResult;
}

// port (save version 0.276): the units' tile searches in progress (pm.rs SavedSearch)
void EEPathManager::SaveSearches(FSFile& outFile)
{
    ee_path_manager_file(this->mRust, 1, false, EEPathFileWrite, &outFile);
}

long EEPathManager::LoadSearches(FSFile& inFile)
{
    long theResult = ee_path_manager_file(this->mRust, 1, true, EEPathFileRead, &inFile);
    if (theResult == 0)
        EEAfterLoad([]() { gPathManager.Call(26); });
    return theResult;
}

// port (save version 0.278): the graph searches in progress (pm.rs SavedGraph), resolved at the first tick
void EEPathManager::SaveGraphSearches(FSFile& outFile)
{
    ee_path_manager_file(this->mRust, 2, false, EEPathFileWrite, &outFile);
}

long EEPathManager::LoadGraphSearches(FSFile& inFile)
{
    return ee_path_manager_file(this->mRust, 2, true, EEPathFileRead, &inFile);
}

// The dumps' text (pm.rs ee_path_manager_dump): EE-SIM's state (TEST ONLY), the out-of-synch dump
static void EEPathDump(EePathManager* inManager, int32_t inWhat, FILE* inFile)
{
    std::vector<char> theText(ee_path_manager_dump(inManager, inWhat, NULL, 0));
    ee_path_manager_dump(inManager, inWhat, reinterpret_cast<uint8_t*>(theText.data()), (uint32_t)theText.size());
    fwrite(theText.data(), 1, theText.size(), inFile);
}

void EEPathManager::DumpSim(FILE* inFile)
{
    EEPathDump(this->mRust, 0, inFile);
}

void EEPathManager::DumpOutOfSynch(FILE* inFile)
{
    EEPathDump(this->mRust, 1, inFile);
}

// Runs a bounded tile search (at most 200 steps; a window around inFrom when inBounds is empty) and returns its
// path (and the visited tiles when asked; outVisited is appended to): ee_sim::astar pm_entry.rs with its PMRQ record
void EEPathManager::FindPath(EEComplexUnit* inUnit, EEMap* inMap, const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo,
                             const EEAIAStarBounds& inBounds, std::vector<U2DPoint<long> >& outPath, long inType,
                             long inRevs, EeAStarModifier* inModifier, std::vector<U2DPoint<long> >* outVisited)
{
    void* theLists[2] = {ee_path_tiles_new(), outVisited != NULL ? ee_path_tiles_new() : NULL};
    const int32_t theEnds[4] = {(int32_t)inFrom.mX, (int32_t)inFrom.mY, (int32_t)inTo.mX, (int32_t)inTo.mY};
    const EePathWindow theWindow = {(int32_t)inBounds.mTop, (int32_t)inBounds.mLeft, (int32_t)inBounds.mBottom, (int32_t)inBounds.mRight};
    ee_path_find_path(this->mRust, EEPathHandle(inUnit), EEPathHandle(inMap), theEnds, theWindow, (int32_t)inType, (int32_t)inRevs,
                      EEPathHandle(inModifier), theLists[0], theLists[1]);
    outPath.clear();
    for (int k = 0; k < 2; k++) {
        std::vector<U2DPoint<long> >* theOut = k == 0 ? &outPath : outVisited;
        int32_t theTile[2];
        for (uint32_t n = 0; theLists[k] != NULL && ee_path_tiles_get(theLists[k], n, theTile); n++)
            theOut->push_back(U2DPoint<long>(theTile[0], theTile[1]));
        ee_path_tiles_free(theLists[k]);
    }
}
