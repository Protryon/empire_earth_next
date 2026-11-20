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
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"
#include "lle/UReferenceCounter.h"
#include "lle/UMemoryPool.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EECPASGFlags.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EEAIAStarView.h"   // the searches' view (ee_sim::astar path_search.rs)
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIPathSector.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEMathInline.h"

#include "ee/EEPathManager.h"
#include "ee/EEUnitHandle.h"
#include "ee/EESimCapture.h"
#include "rust/sim/astar.h"   // ee_path_*: the path splicing (ee_astar path_splice.rs)

static_assert(sizeof(GE3DPoint) == sizeof(EePathPoint) && offsetof(GE3DPoint, mZ) == offsetof(EePathPoint, z),
              "GE3DPoint");

static const EePathPoint* EEPathPoints(const std::vector<GE3DPoint>& inPoints)
{
    return inPoints.empty() ? NULL : reinterpret_cast<const EePathPoint*>(&inPoints[0]);
}

// TEST ONLY (docs/RUST_PLAN.md R5b.1, tools/sim_capture.py): the bounded searches' words of their PMRQ record (the
// record and its scope are Rust's: ee_sim::astar pm_capture.rs)
static void EEPathCaptureI32(long inValue) { ee_path_capture_word((uint32_t)(int32_t)inValue); }

static void EEPathCaptureF32(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    ee_path_capture_word(theBits);
}

static void EEPathCaptureTile(const U2DPoint<long>& inTile)
{
    EEPathCaptureI32(inTile.mX);
    EEPathCaptureI32(inTile.mY);
}

static void EEPathCaptureTiles(const std::vector<U2DPoint<long> >& inTiles)
{
    EEPathCaptureI32((long)inTiles.size());
    for (size_t i = 0; i < inTiles.size(); i++)
        EEPathCaptureTile(inTiles[i]);
}

static void EEPathCapturePoints(const std::vector<GE3DPoint>& inPoints)
{
    EEPathCaptureI32((long)inPoints.size());
    for (size_t i = 0; i < inPoints.size(); i++) {
        EEPathCaptureF32(inPoints[i].GetX());
        EEPathCaptureF32(inPoints[i].GetY());
        EEPathCaptureF32(inPoints[i].GetZ());
    }
}

static unsigned int EEPathCaptureUnitID(EEComplexUnit* inUnit)
{
    return inUnit != NULL ? (unsigned int)inUnit->mUnitID : 0xffffffffu;
}

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

// Moves a tile that is off the map onto the map tile closest to its center
inline void EEPathManager::ClampTile(EEMap* inMap, EETileCoordinate& ioTile)
{
    if (inMap->GetTileAtLocation(ioTile) != NULL)
        return;
    float theHalf = 0.5f;
    GE3DPoint thePoint((float)ioTile.GetX() + theHalf, (float)ioTile.GetY() + theHalf, 0.0f);
    long theX = 0;
    long theY = 0;
    inMap->GetClosestTile(&thePoint, &theX, &theY);
    ioTile.mX = theX;
    ioTile.mY = theY;
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

static void EEPathPutTile(int64_t* outValues, const EETileCoordinate& inTile)
{
    outValues[0] = inTile.mX;
    outValues[1] = inTile.mY;
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
}   // namespace

// a C++ point list into a Rust one (pm_bounded.rs's lists)
static void EEPathPointsOut(uint64_t inList, const std::vector<GE3DPoint>& inPoints)
{
    if (!inPoints.empty())
        ee_path_points_append(reinterpret_cast<void*>(inList), reinterpret_cast<const EePathPoint*>(&inPoints[0]),
                              (uint32_t)inPoints.size());
}

static std::vector<GE3DPoint> EEPathPointsIn(uint64_t inPoints, int64_t inCount)
{
    const GE3DPoint* thePoints = reinterpret_cast<const GE3DPoint*>(inPoints);
    return std::vector<GE3DPoint>(thePoints, thePoints + inCount);
}

// The searches' and the units' operations (path_search.rs EE_PATH_VIEW_NEW and below, pm_bounded.rs EE_PATH_UNIT_*,
// the capture)
static int64_t EEPathBoundedHost(EEPathManager* inManager, int32_t inOp, EePathIo* io)
{
    int64_t* i = io->i;
    EEComplexUnit* theUnit = EEPathUnitOf(io->h[0]);
    switch (inOp) {
    // the graph searches' world (graph_search.rs EE_PATH_SS_INFO and below)
    case EE_PATH_GRAPH_TERRAIN_FLAGS: {   // SetFlags: the unit class's GetTerrainFlags into the goal's flags
        EeAStarGraphGoalRec* theGoal = reinterpret_cast<EeAStarGraphGoalRec*>(io->h[1]);
        EECPASGFlags theFlags;
        theFlags.mPlayerID = theGoal->flags_player;
        memcpy(theFlags.mFlags, theGoal->path_flags, sizeof(theFlags.mFlags));
        theUnit->mUnitClass->GetTerrainFlags(theUnit, theFlags);
        theGoal->flags_player = (int32_t)theFlags.mPlayerID;
        memcpy(theGoal->path_flags, theFlags.mFlags, sizeof(theFlags.mFlags));
        return 0;
    }
    case EE_PATH_APPLY_GRAPH_PATH:   // RequestUnitPaths' world
        inManager->RequestUnitPaths(EEPathUnitOf(io->h[1]), reinterpret_cast<EEAutoPathRequest*>(io->h[2]),
                                    reinterpret_cast<const int32_t*>(io->h[0]), (size_t)i[0],
                                    reinterpret_cast<EEAIPathSubSector*>(io->h[3]));
        return 0;
    case EE_PATH_REQUEST_FLAGS: {   // the goal's flags = the request's (EECPASGFlags::operator=)
        EeAStarGraphGoalRec* theGoal = reinterpret_cast<EeAStarGraphGoalRec*>(io->h[1]);
        const EECPASGFlags& theFlags = reinterpret_cast<EEAutoPathRequest*>(io->h[0])->mFlags;
        theGoal->flags_player = (int32_t)theFlags.mPlayerID;
        memcpy(theGoal->path_flags, theFlags.mFlags, sizeof(theFlags.mFlags));
        return 0;
    }
    case EE_PATH_TILES_OUT: {   // into the C++ vector h1
        const EePoint* theTiles = reinterpret_cast<const EePoint*>(io->h[2]);
        std::vector<U2DPoint<long> >& theOut = *reinterpret_cast<std::vector<U2DPoint<long> >*>(io->h[1]);
        for (int64_t k = 0; k < i[0]; k++)
            theOut.push_back(U2DPoint<long>(theTiles[k].x, theTiles[k].y));
        return 0;
    }
    case EE_PATH_POINTS_OUT: {   // into the C++ vector h1
        std::vector<GE3DPoint> thePoints = EEPathPointsIn(io->h[2], i[0]);
        std::vector<GE3DPoint>& theOut = *reinterpret_cast<std::vector<GE3DPoint>*>(io->h[1]);
        theOut.insert(theOut.end(), thePoints.begin(), thePoints.end());
        return 0;
    }
    case EE_PATH_UNIT_WAYPOINT_POINTS: {
        std::vector<GE3DPoint> thePoints;
        for (size_t k = 0; k < theUnit->mWaypoints.size(); k++)
            thePoints.push_back(theUnit->mWaypoints[k].mLocation);
        EEPathPointsOut(io->h[1], thePoints);
        return 0;
    }
    case EE_PATH_UNIT_DROP_PATH:
        theUnit->DropPath();
        return 0;
    case EE_PATH_UNIT_TAKE_PATH: {
        std::vector<GE3DPoint> thePath = EEPathPointsIn(io->h[1], i[0]);
        theUnit->TakePath(thePath, i[1] != 0);
        return 0;
    }
    case EE_PATH_UNIT_PATH_DONE:
        if (theUnit->mIsPathDone != true)
            theUnit->mIsPathDone = true;
        theUnit->mBoundedPathTiles.clear();
        return 0;
    case EE_PATH_UNIT_CAPTURE: {   // TEST ONLY (pm_capture.rs): the first goal queue's length, a location goal's place
        EEUGRust* theLocation = EELocationGoalCast(theUnit->GetGoal());
        i[0] = theLocation != NULL;
        if (theLocation != NULL) {
            float theX = theLocation->GetLocation().GetX(), theY = theLocation->GetLocation().GetY();
            uint32_t theBits[2];
            memcpy(&theBits[0], &theX, 4);
            memcpy(&theBits[1], &theY, 4);
            i[1] = theBits[0];
            i[2] = theBits[1];
        }
        return (int64_t)theUnit->mGoalQueues[0].mGoals.size();
    }
    case EE_PATH_UNIT_SNAP:
        theUnit->SnapLastWaypoint();
        if (i[0] && theUnit->GetGoal())
            theUnit->GetGoal()->PathTaken(theUnit);
        return 0;
    }
    return 0;
}

// The Rust manager's host: one call per operation (pm.rs EE_PATH_*)
static int64_t EEPathManagerHost(void* inContext, int32_t inOp, EePathIo* io)
{
    EEPathManager* theManager = static_cast<EEPathManager*>(inContext);
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
    case EE_PATH_CLAMP: {   // (on the map h1 when i2)
        EETileCoordinate theTile = EEPathTileArg(i);
        theManager->ClampTile(i[2] ? reinterpret_cast<EEMap*>(io->h[1]) : gWorld.mMap, theTile);
        EEPathPutTile(i, theTile);
        return 0;
    }
    // the tile machines: Rust (path_search.rs)
    // the graph machines: Rust (graph_search.rs)
    // the units
    case EE_PATH_AUTO_WAYPOINT: {   // FinishGraphPath's unit part
        EEComplexUnit* theUnit = EEPathUnitOf(h);
        EETileCoordinate theFrom = theUnit->GetTile();
        EEUGRust* theGoal = EELocationGoalCast(theUnit->GetGoal());
        if (!(theGoal && theGoal->GetIsAutoWaypoint() && theUnit->mTile))
            return 0;
        EEPathPutTile(i, theFrom);
        i[3] = UMFloor(theGoal->GetLocation().GetY());
        i[2] = UMFloor(theGoal->GetLocation().GetX());
        theUnit->DropPath();
        return 1;
    }
    case EE_PATH_UNIT_WAYPOINTS:
        return (int64_t)EEPathUnitOf(h)->mWaypoints.size();
    case EE_PATH_UNIT_GOAL_TILE: {
        EEUGRust* theGoal = EELocationGoalCast(EEPathUnitOf(h)->GetGoal());
        if (!(theGoal && theGoal->GetIsAutoWaypoint()))
            return 0;
        i[0] = (long)theGoal->GetLocation().GetX();
        i[1] = (long)theGoal->GetLocation().GetY();
        return 1;
    }
    case EE_PATH_UNIT_COUNT_REQUEST: {   // the request counted; whether it goes into the history
        EEComplexUnit* theUnit = EEPathUnitOf(h);
        if (theUnit->mNumPathRequests != theUnit->mNumPathRequests + 1)
            theUnit->mNumPathRequests = theUnit->mNumPathRequests + 1;
        return (!theUnit->GetPlayer()->IsHuman() || theUnit->GetPlayer()->GetPlayerID() == 0) && !theUnit->mIsDetachedFromAI
            && theUnit->mPreviousPart == NULL;
    }
    case EE_PATH_PENDING_UNIT: {   // RequestPendingPath's unit part
        EEComplexUnit* theUnit = EEPathUnitOf(h);
        theUnit->DropPath();
        EEUGRust* theGoal = EELocationGoalCast(theUnit->GetGoal());
        bool theGraph = theUnit->GetGoal() != NULL && (theGoal == NULL || !theGoal->GetIsAutoWaypoint());
        i[0] = theUnit->mTileX;
        i[1] = theUnit->mTileY;
        return theGraph;
    }
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
    default:
        return EEPathBoundedHost(theManager, inOp, io);
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
// path (and the visited tiles when asked): ee_sim::astar pm_bounded.rs find_path
void EEPathManager::FindPath(EEComplexUnit* inUnit, EEMap* inMap, const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo,
                             const EEAIAStarBounds& inBounds, std::vector<U2DPoint<long> >& outPath, long inType,
                             long inRevs, EeAStarModifier* inModifier, std::vector<U2DPoint<long> >* outVisited)
{
    bool theCapture = ee_path_capture_open(this->mRust, 3);   // TEST ONLY (PMRQ)
    if (theCapture) {
        EEPathCaptureI32((long)EEPathCaptureUnitID(inUnit));
        EEPathCaptureTile(inFrom);
        EEPathCaptureTile(inTo);
        const long theBounds[4] = {inBounds.mTop, inBounds.mLeft, inBounds.mBottom, inBounds.mRight};
        for (long b : theBounds)
            EEPathCaptureI32(b);
        EEPathCaptureI32(inType);
        EEPathCaptureI32(inRevs);
        EEPathCaptureI32(inModifier != NULL ? 1 : 0);
        EEPathCaptureI32(outVisited != NULL ? 1 : 0);
    }
    outPath.clear();
    EePathIo theIO = {};
    theIO.h[0] = EEPathHandle(inUnit);
    theIO.h[1] = EEPathHandle(inMap);
    theIO.h[2] = EEPathHandle(&outPath);
    theIO.h[3] = EEPathHandle(outVisited);
    const int64_t theValues[11] = {inFrom.mX, inFrom.mY, inTo.mX, inTo.mY, inBounds.mBottom, inBounds.mLeft, inBounds.mRight,
                                   inBounds.mTop, inType, inRevs, (int64_t)EEPathHandle(inModifier)};
    memcpy(theIO.i, theValues, sizeof(theValues));
    ee_path_manager_call(this->mRust, 27, &theIO);
    if (theCapture) {
        EEPathCaptureTiles(outPath);
        if (outVisited != NULL)
            EEPathCaptureTiles(*outVisited);
    }
    ee_path_capture_close(this->mRust);
}

// Runs a bounded search within inMargin tiles of inFrom and, when it reaches inTo, appends the path (start first);
// returns whether it reached inTo (*outReached the same; inArg8 is not read): pm_bounded.rs find_bounded_path
bool EEPathManager::FindBoundedPath(EEComplexUnit* inUnit, EEMap* inMap, long inType, const EETileCoordinate& inFrom,
                                    const EETileCoordinate& inTo, std::vector<GE3DPoint>& outPath, long inMargin,
                                    bool inArg8, bool* outReached)
{
    bool theCapture = ee_path_capture_open(this->mRust, 4);   // TEST ONLY (PMRQ)
    if (theCapture) {
        EEPathCaptureI32((long)EEPathCaptureUnitID(inUnit));
        EEPathCaptureI32(inType);
        EEPathCaptureTile(inFrom);
        EEPathCaptureTile(inTo);
        EEPathCaptureI32(inMargin);
        EEPathCapturePoints(outPath);
    }
    bool theReached = this->Call(28, EEPathHandle(inUnit), EEPathHandle(inMap),
                                 {inFrom.mX, inFrom.mY, inTo.mX, inTo.mY, inType, inMargin, (int64_t)EEPathHandle(&outPath)}) != 0;
    *outReached = theReached;
    if (theCapture) {
        EEPathCaptureI32(theReached ? 1 : 0);
        EEPathCapturePoints(outPath);
    }
    ee_path_capture_close(this->mRust);
    return theReached;
}

// Called by EEUnitClass 0x636b69 (cdecl, the third argument 0)
// When one of the unit's waypoints, last first, lies on a tile of ioPath, appends the waypoints after it to ioPath
// (inArg3 unused)
void EEPathManager::MergeWaypoints(std::vector<GE3DPoint>& ioPath, std::deque<EEComplexUnitWaypoint>& inWaypoints,
                                   long inArg3)
{
    // (Rust: ee_astar path_splice.rs merge_waypoints: the waypoint found)
    std::vector<GE3DPoint> theLocations;
    theLocations.reserve(inWaypoints.size());
    for (size_t i = 0; i < inWaypoints.size(); i++)
        theLocations.push_back(inWaypoints[i].mLocation);
    long theFound = (long)ee_path_merge_waypoints(EEPathPoints(ioPath), ioPath.size(), EEPathPoints(theLocations),
                                                  theLocations.size());
    if (theFound >= 0)
        ioPath.insert(ioPath.end(), theLocations.begin() + theFound + 1, theLocations.end());
}

// Applies a graph path: a unit gets a location goal per sub sector on the way (or inserted before a pending attack),
// a request the sub sectors' tiles and summed distance; returns whether it reached the goal's sub sector
void EEPathManager::RequestUnitPaths(EEComplexUnit* inUnit, EEAutoPathRequest* inRequest, const int32_t* inPath,
                                     size_t inCount, EEAIPathSubSector* inFrom)
{
    std::vector<long> thePath(inPath, inPath + inCount);
    EETileCoordinate theTile;
    theTile.mX = 0;
    theTile.mY = 0;
    EETileCoordinate theLast;
    theLast.mX = 0;
    theLast.mY = 0;
    long theDistance = 0;
    bool theAttack = false;
    bool theSet = false;
    if (inUnit) {
        EEUGRust* theLocation = EELocationGoalCast(inUnit->GetGoal());
        const EEGoalDeque& theGoals = inUnit->mGoalQueues[0].mGoals;
        if (inUnit->GetGoal())
            inUnit->GetGoal()->mB1D = true;
        bool theForced = theLocation == NULL || theLocation->GetIsForcedMove();
        if (theLocation == NULL) {
            EEUGRust* theAttackGoal = EEAttackUnitCast(inUnit->GetGoal());
            if (theAttackGoal && !theAttackGoal->GetB1C()) {
                theAttack = true;
                if (theGoals.size() <= 1)
                    goto done;
                goto build;
            }
        }
        if (theGoals.size() == 0)
            goto done;
        if (theGoals.back()->GetKind() == 1 && static_cast<EEUGRust*>(theGoals.back())->GetIsAutoWaypoint())
            goto done;
    build:
        if (thePath.size() > 1 && inUnit->mTile) {
            for (long i = 0; i < (long)thePath.size() - 2; i++) {
                EEAIPathSubSector* theSector = gContinentManager.GetPathSubSector((unsigned short)thePath[i]);
                theTile = theSector->GetLocation();
                EEUGRust* theGoal = EEUGRust::NewLocation(theTile.GetX() + 0.5f, theTile.GetY() + 0.5f, 0.0f,
                                                         inUnit->GetGoal() && inUnit->GetGoal()->GetB1C(), theForced,
                                                         false, 0.0f, true, 0);
                if (!theAttack) {
                    inUnit->SetGoal(theGoal, 1, 0);
                    theSet = true;
                } else {
                    inUnit->mGoalQueues[0].Insert(1, theGoal);
                    inUnit->RefreshGoals();
                    inUnit->RefreshActiveGoal();
                }
            }
            inUnit->RefreshGoals();
            inUnit->RefreshActiveGoal();
            if (theSet) {
                inUnit->StartAction(kActionPrepareToMove);
                inUnit->GetUnitClass()->ActionChanged(inUnit);
            }
        }
    } else if (inRequest) {
        inRequest->mPath.clear();
        if (thePath.size() != 0) {
            EEAIPathSubSector* theSector = gContinentManager.GetPathSubSector((unsigned short)thePath.back());
            theLast = theSector->GetLocation();
            inRequest->mPath.push_back(theLast);
            if (inFrom)
                theDistance = UMAbs(inFrom->GetLocation().GetX() - theLast.GetX()) +
                              UMAbs(inFrom->GetLocation().GetY() - theLast.GetY());
        }
        for (long i = thePath.size() - 2; i >= 0; i--) {
            EEAIPathSubSector* theSector = gContinentManager.GetPathSubSector((unsigned short)thePath[i]);
            theTile = theSector->GetLocation();
            inRequest->mPath.push_back(theTile);
            theDistance += UMAbs(theTile.GetX() - theLast.GetX()) + UMAbs(theTile.GetY() - theLast.GetY());
        }
        inRequest->mPathLength = theDistance;
        inRequest->mIsDone = true;
    }
done:
    return;
}
