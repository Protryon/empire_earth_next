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

// NEAR: the original copies the (uninitialised) allocator byte of its fifth argument
// Creates a player's graph path request between two tiles with the given sub-group flags, not yet done
EEAutoPathRequest::EEAutoPathRequest(long inRequestID, long inPlayerID, const EETileCoordinate& inFrom,
                                     const EETileCoordinate& inTo, const EECPASGFlags& inFlags)
    : mFlags(inFlags), mFrom(inFrom), mTo(inTo)
{
    this->mPlayerID = inPlayerID;
    this->mPathLength = 0;
    this->mIsDone = false;
    this->mRequestID = inRequestID;
}

// Destroys the path request (members only)
EEAutoPathRequest::~EEAutoPathRequest()
{
}

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

static EETileCoordinate EEPathTileArg(const int64_t* inValues)
{
    EETileCoordinate theTile;
    theTile.mX = (long)inValues[0];
    theTile.mY = (long)inValues[1];
    return theTile;
}

// the owners waiting for their running requests across a load (EEAwaitAutoPathRequest), by request id until the
// loaded game's first tick; the resolve takes them (EE_PATH_AWAIT 0) and gives each its request or none
namespace {
std::map<long, std::function<void(EEAutoPathRequest*)> > sAwaitedRequests, sResolvingRequests;
bool sGraphResolveQueued = false;

void EEResolveGraphSearches()
{
    sGraphResolveQueued = false;
    EePathIo theIO = {};
    ee_path_manager_call(gPathManager.mRust, 19, &theIO);
}

void EEQueueGraphResolve()
{
    if (!sGraphResolveQueued) {
        sGraphResolveQueued = true;
        EEAfterLoadLast(EEResolveGraphSearches);   // after every owner's own fix-up (the active groups')
    }
}

// a request from its saved record (one reference, the caller's)
EEAutoPathRequest* EENewLoadedRequest(const EePathRequestRecord& r, const std::vector<EETileCoordinate>& inPath)
{
    EECPASGFlags theFlags;
    theFlags.mPlayerID = r.flags_player;
    for (int f = 0; f < 17; f++)
        theFlags.mFlags[f] = r.flags[f];
    EETileCoordinate theFrom, theTo;
    theFrom.mX = r.from[0];
    theFrom.mY = r.from[1];
    theTo.mX = r.to[0];
    theTo.mY = r.to[1];
    EEAutoPathRequest* theRequest = new EEAutoPathRequest(r.id, r.player, theFrom, theTo, theFlags);
    theRequest->mFlags.mPlayerID = theFlags.mPlayerID;   // (the copy ctor zeroes it)
    theRequest->mPath = inPath;
    theRequest->mPathLength = r.length;
    theRequest->mIsDone = r.done;
    return theRequest;
}

void EERequestRecordOf(const EEAutoPathRequest& inRequest, EePathRequestRecord& outRecord, std::vector<int32_t>& outPath)
{
    outRecord.from[0] = (int32_t)inRequest.mFrom.mX;
    outRecord.from[1] = (int32_t)inRequest.mFrom.mY;
    outRecord.to[0] = (int32_t)inRequest.mTo.mX;
    outRecord.to[1] = (int32_t)inRequest.mTo.mY;
    outRecord.player = (int32_t)inRequest.mPlayerID;
    outRecord.id = (int32_t)inRequest.mRequestID;
    outRecord.length = (int32_t)inRequest.mPathLength;
    outRecord.done = inRequest.mIsDone;
    outRecord.flags_player = (int32_t)inRequest.mFlags.mPlayerID;
    for (int f = 0; f < 17; f++)
        outRecord.flags[f] = inRequest.mFlags.mFlags[f];
    for (size_t i = 0; i < inRequest.mPath.size(); i++) {
        outPath.push_back((int32_t)inRequest.mPath[i].mX);
        outPath.push_back((int32_t)inRequest.mPath[i].mY);
    }
}
// a found graph path's sub sectors' tiles, its summed distance, done (pm_apply.rs)
void EESetRequestPath(EEAutoPathRequest& ioRequest, const int32_t* inTiles, size_t inCount, int64_t inLength)
{
    ioRequest.mPath.clear();
    for (size_t k = 0; k < inCount; k++) {
        EETileCoordinate theTile;
        theTile.mX = inTiles[2 * k];
        theTile.mY = inTiles[2 * k + 1];
        ioRequest.mPath.push_back(theTile);
    }
    ioRequest.mPathLength = (long)inLength;
    ioRequest.mIsDone = true;
}
}   // namespace

// The Rust manager's host: one call per operation (pm.rs EE_PATH_*)
static int64_t EEPathManagerHost(void* inContext, int32_t inOp, EePathIo* io)
{
    int64_t* i = io->i;
    const uint64_t h = io->h[0];
    switch (inOp) {
    case EE_PATH_WORLD_UNIT:
        return (int64_t)EEPathHandle(EEPathWorldUnit(i[0]));
    case EE_PATH_REQUEST_REFERENCE:
        reinterpret_cast<EEAutoPathRequest*>(h)->Reference();
        return 0;
    case EE_PATH_REQUEST_DEREFERENCE:
        reinterpret_cast<EEAutoPathRequest*>(h)->Dereference();
        return 0;
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
    // the regions, the pools
    case EE_PATH_REBUILD:
        return gContinentManager.Rebuild();
    // the requests
    case EE_PATH_REQUEST_LOAD: {   // a loaded one (the record in h0)
        {
            EePathRequestRecord theRecord;
            std::vector<int32_t> theTiles(2 * ee_path_request_record_get((const void*)(uintptr_t)h, &theRecord, NULL, 0));
            ee_path_request_record_get((const void*)(uintptr_t)h, &theRecord, theTiles.data(), (uint32_t)(theTiles.size() / 2));
            std::vector<EETileCoordinate> thePath;
            for (size_t k = 0; k + 1 < theTiles.size(); k += 2) {
                EETileCoordinate theTile;
                theTile.mX = theTiles[k];
                theTile.mY = theTiles[k + 1];
                thePath.push_back(theTile);
            }
            return (int64_t)EEPathHandle(EENewLoadedRequest(theRecord, thePath));
        }
    }
    case EE_PATH_REQUEST_NEW: {
        EETileCoordinate theFrom = EEPathTileArg(i + 2), theTo = EEPathTileArg(i + 4);
        return (int64_t)EEPathHandle(new EEAutoPathRequest((long)i[0], (long)i[1], theFrom, theTo,
                                                           *reinterpret_cast<EECPASGFlags*>(io->h[1])));
    }
    case EE_PATH_REQUEST_SAVE: {
        EePathRequestRecord theRecord = {};
        std::vector<int32_t> thePath;
        EERequestRecordOf(*reinterpret_cast<EEAutoPathRequest*>(h), theRecord, thePath);
        ee_path_request_record_set((void*)(uintptr_t)io->h[1], &theRecord, thePath.data(), (uint32_t)(thePath.size() / 2));
        return 0;
    }
    case EE_PATH_AWAIT:
        switch (i[0]) {
        case 0:   // the resolve takes the owners
            sResolvingRequests.clear();
            sResolvingRequests.swap(sAwaitedRequests);
            return 0;
        case 1:
            return sResolvingRequests.find((long)i[1]) != sResolvingRequests.end();
        case 2: {   // the owner takes the load's reference
            std::map<long, std::function<void(EEAutoPathRequest*)> >::iterator it = sResolvingRequests.find((long)i[1]);
            std::function<void(EEAutoPathRequest*)> theAttach = it->second;
            sResolvingRequests.erase(it);
            theAttach(reinterpret_cast<EEAutoPathRequest*>(h));
            return 0;
        }
        default: {   // the owners' fallback: they ask again
            std::map<long, std::function<void(EEAutoPathRequest*)> > theRest;
            theRest.swap(sResolvingRequests);
            for (std::map<long, std::function<void(EEAutoPathRequest*)> >::iterator it = theRest.begin(); it != theRest.end(); it++)
                it->second(NULL);
            return 0;
        }
        }
    case EE_PATH_QUEUE_RESOLVE:
        EEQueueGraphResolve();
        return 0;
    case EE_PATH_REQUEST_SET_PATH:   // RequestUnitPaths' (pm_apply.rs)
        EESetRequestPath(*reinterpret_cast<EEAutoPathRequest*>(io->h[1]), reinterpret_cast<const int32_t*>(io->h[2]), (size_t)io->i[0], io->i[1]);
        return 0;
    }
    return 0;
}

void EEAwaitAutoPathRequest(long inRequestID, std::function<void(EEAutoPathRequest*)> inAttach)
{
    std::map<long, std::function<void(EEAutoPathRequest*)> >::iterator it = sAwaitedRequests.find(inRequestID);
    if (it != sAwaitedRequests.end()) {   // (ids are unique: a second owner of one asks again)
        it->second(NULL);
        sAwaitedRequests.erase(it);
    }
    sAwaitedRequests[inRequestID] = inAttach;
    EEQueueGraphResolve();
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

void EEPathManager::CancelAutoPathRequest(UReferenceCounter* inRequest)
{
    this->Call(8, EEPathHandle(static_cast<EEAutoPathRequest*>(inRequest)));
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

// port (save version 0.277): a path request's every field (ee/EEAutoPathRequest.h; pm.rs RequestRecord)
void EESaveAutoPathRequest(FSFile& outFile, const EEAutoPathRequest& inRequest)
{
    EePathRequestRecord theRecord = {};
    std::vector<int32_t> thePath;
    EERequestRecordOf(inRequest, theRecord, thePath);
    ee_path_request_write(&theRecord, thePath.data(), (uint32_t)(thePath.size() / 2), EEPathFileWrite, &outFile);
}

long EELoadAutoPathRequest(FSFile& inFile, EEAutoPathRequest*& outRequest)
{
    outRequest = NULL;
    EePathRequestRecord theRecord = {};
    void* theTiles = ee_path_tiles_new();
    long theResult = ee_path_request_read(&theRecord, theTiles, EEPathFileRead, &inFile);
    if (theResult == 0) {
        std::vector<EETileCoordinate> thePath;
        int32_t theTile[2];
        for (uint32_t k = 0; ee_path_tiles_get(theTiles, k, theTile); k++) {
            EETileCoordinate t;
            t.mX = theTile[0];
            t.mY = theTile[1];
            thePath.push_back(t);
        }
        outRequest = EENewLoadedRequest(theRecord, thePath);
    }
    ee_path_tiles_free(theTiles);
    return theResult;
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
