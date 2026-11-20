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

// TEST ONLY (docs/RUST_PLAN.md R5b.1, tools/sim_capture.py): the record writers of PMRQ below (points as three
// f32, lists with their u32 length first); the splicing's PMSP records are pm_bounded.rs's, in the same layout

static void EEPathCapturePoints(EESimCaptureRecord& ioRecord, const std::vector<GE3DPoint>& inPoints)
{
    ioRecord.U32((unsigned int)inPoints.size());
    for (size_t i = 0; i < inPoints.size(); i++) {
        ioRecord.F32(inPoints[i].GetX());
        ioRecord.F32(inPoints[i].GetY());
        ioRecord.F32(inPoints[i].GetZ());
    }
}

static void EEPathCaptureWaypoints(EESimCaptureRecord& ioRecord, const std::deque<EEComplexUnitWaypoint>& inWaypoints)
{
    ioRecord.U32((unsigned int)inWaypoints.size());
    for (size_t i = 0; i < inWaypoints.size(); i++) {
        ioRecord.F32(inWaypoints[i].mLocation.GetX());
        ioRecord.F32(inWaypoints[i].mLocation.GetY());
        ioRecord.F32(inWaypoints[i].mLocation.GetZ());
    }
}

static void EEPathCaptureBounds(EESimCaptureRecord& ioRecord, const EEAIAStarBounds& inBounds)
{
    ioRecord.I32((int)inBounds.mTop);
    ioRecord.I32((int)inBounds.mLeft);
    ioRecord.I32((int)inBounds.mBottom);
    ioRecord.I32((int)inBounds.mRight);
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

// (the path goal's sea and space modifiers, vtables 0x43c458/0x43c448, are Rust: ee_sim::astar modifier_obj.rs)

// TEST ONLY (docs/RUST_PLAN.md R5b.1, tools/sim_capture.py): the path manager's record PMRQ, one per outermost
// call of its entry points (the units', the AI's, the storm's, the physics tick's): a u32 kind, the inputs, the
// result, then the queued searches after the call (per search: the queue, unit, goal tile, passes, request id, the
// machine's type, revs, steps, found, step and rev limits) and every unit and request whose search left the queues
// during the call with what it got (a unit's waypoints, path-done flag, request count, bounded tiles, goals; a
// request's path, length and done flag). An Update that leaves nothing and finds nothing queued writes no record.
// The record reads only the manager's queues and the objects, so it is the same over any implementation of them.
namespace {
enum {
    kPathRequestCapture = 0x51524d50,   // "PMRQ"
    kPathRequestPath = 1,
    kPathRequestUnitPath = 2,
    kPathRequestFindPath = 3,
    kPathRequestFindBoundedPath = 4,
    kPathRequestCompletePath = 5,
    kPathRequestCompleteAutoPath = 6,
    kPathRequestCancelPaths = 7,
    kPathRequestCancelRequest = 8,
    kPathRequestUpdate = 9,
    kPathRequestPending = 10,
    kPathRequestRegionUpdates = 11,
    kPathRequestIsPathDone = 12,
    kPathRequestHasAutoPath = 13,
    kPathRequestGetPathGoal = 14,
    kPathRequestReset = 15,
};

struct EEPathQueueEntry {
    unsigned int mQueue, mUnitID;
    int mX, mY, mPasses, mRequestID, mType, mRevs, mSteps, mFound, mMaxRevs, mRevLimit;
    UReferenceCounter* mRequest;
    bool operator==(const EEPathQueueEntry& inOther) const
    {
        return mQueue == inOther.mQueue && mUnitID == inOther.mUnitID && mX == inOther.mX && mY == inOther.mY
            && mRequest == inOther.mRequest;
    }
};

thread_local int sPathRequestDepth = 0;
}   // namespace

class EEPathRequestCapture {
public:
    EEPathRequestCapture(unsigned int inKind, EEPathManager* inManager)
        : mRecord(kPathRequestCapture), mManager(inManager), mOn(sPathRequestDepth == 0 && EESimCaptureRecord::Active()),
          mKind(inKind)
    {
        sPathRequestDepth++;
        if (!this->mOn)
            return;
        this->mRecord.U32(inKind);
        this->Queues(this->mBefore);
        for (size_t i = 0; i < this->mBefore.size(); i++)
            if (this->mBefore[i].mRequest != NULL)
                this->mBefore[i].mRequest->Reference();   // read after the call, whoever else let go of it
    }
    ~EEPathRequestCapture()
    {
        sPathRequestDepth--;
        if (!this->mOn)
            return;
        std::vector<EEPathQueueEntry> theAfter;
        this->Queues(theAfter);
        std::vector<const EEPathQueueEntry*> theLeft;
        for (size_t i = 0; i < this->mBefore.size(); i++)
            if (std::find(theAfter.begin(), theAfter.end(), this->mBefore[i]) == theAfter.end())
                theLeft.push_back(&this->mBefore[i]);
        if (this->mKind != kPathRequestUpdate || !this->mBefore.empty() || !theAfter.empty()) {
            this->mRecord.U32(0x2d2d2d2d);   // the queues after
            this->mRecord.U32((unsigned int)theAfter.size());
            for (size_t i = 0; i < theAfter.size(); i++)
                this->Entry(theAfter[i]);
            this->mRecord.U32((unsigned int)theLeft.size());
            for (size_t i = 0; i < theLeft.size(); i++) {
                this->Entry(*theLeft[i]);
                this->Unit(theLeft[i]->mUnitID);
                if (theLeft[i]->mRequest != NULL)
                    this->Request(static_cast<EEAutoPathRequest*>(theLeft[i]->mRequest));
            }
            this->mRecord.Commit();
        }
        for (size_t i = 0; i < this->mBefore.size(); i++)
            if (this->mBefore[i].mRequest != NULL)
                this->mBefore[i].mRequest->Dereference();
    }
    bool On() const { return this->mOn; }
    void Tile(const EETileCoordinate& inTile)
    {
        this->mRecord.I32((int)inTile.mX);
        this->mRecord.I32((int)inTile.mY);
    }
    void Unit(unsigned int inUnitID)
    {
        EEComplexUnit* theUnit = NULL;
        EEUnitHandleResolve(inUnitID, theUnit);
        this->mRecord.U32(theUnit != NULL ? inUnitID : 0xffffffffu);
        if (theUnit == NULL)
            return;
        EEPathCaptureWaypoints(this->mRecord, theUnit->mWaypoints);
        this->mRecord.U32(theUnit->mIsPathDone ? 1 : 0);
        this->mRecord.I32((int)theUnit->mNumPathRequests);
        this->mRecord.U32((unsigned int)theUnit->mBoundedPathTiles.size());
        this->mRecord.U32((unsigned int)theUnit->mGoalQueues[0].mGoals.size());
        EEUnitGoal* theGoal = theUnit->GetGoal();
        this->mRecord.I32(theGoal != NULL ? (int)theGoal->GetKind() : -1);
        EEUGRust* theLocation = EELocationGoalCast(theGoal);
        this->mRecord.F32(theLocation != NULL ? theLocation->GetLocation().GetX() : 0.0f);
        this->mRecord.F32(theLocation != NULL ? theLocation->GetLocation().GetY() : 0.0f);
    }
    void Request(EEAutoPathRequest* inRequest)
    {
        this->mRecord.I32(inRequest != NULL ? (int)inRequest->mRequestID : -1);
        if (inRequest == NULL)
            return;
        this->mRecord.U32((unsigned int)inRequest->mPath.size());
        for (size_t i = 0; i < inRequest->mPath.size(); i++)
            this->Tile(inRequest->mPath[i]);
        this->mRecord.I32((int)inRequest->mPathLength);
        this->mRecord.U32(inRequest->mIsDone ? 1 : 0);
    }
    void Points(const std::vector<U2DPoint<long> >& inPoints)
    {
        this->mRecord.U32((unsigned int)inPoints.size());
        for (size_t i = 0; i < inPoints.size(); i++)
            this->Tile(inPoints[i]);
    }
    EESimCaptureRecord mRecord;

private:
    void Entry(const EEPathQueueEntry& inEntry)
    {
        const int theWords[] = {(int)inEntry.mQueue, (int)inEntry.mUnitID, inEntry.mX, inEntry.mY, inEntry.mPasses,
                                inEntry.mRequestID, inEntry.mType, inEntry.mRevs, inEntry.mSteps, inEntry.mFound,
                                inEntry.mMaxRevs, inEntry.mRevLimit};
        for (int w : theWords)
            this->mRecord.I32(w);
    }
    void Queues(std::vector<EEPathQueueEntry>& outEntries);
    EEPathManager* mManager;
    bool mOn;
    unsigned int mKind;
    std::vector<EEPathQueueEntry> mBefore;
};

void EEPathRequestCapture::Queues(std::vector<EEPathQueueEntry>& outEntries)
{
    std::vector<EePathQueued> theQueued(ee_path_manager_queued(this->mManager->mRust, NULL, 0));
    ee_path_manager_queued(this->mManager->mRust, theQueued.data(), (uint32_t)theQueued.size());
    for (size_t i = 0; i < theQueued.size(); i++) {
        const EePathQueued& q = theQueued[i];
        EEPathQueueEntry e;
        memset(&e, 0, sizeof(e));
        e.mQueue = q.queue;
        e.mUnitID = q.unit;
        e.mX = q.x;
        e.mY = q.y;
        e.mPasses = q.passes;
        e.mRequest = reinterpret_cast<EEAutoPathRequest*>(q.request);
        e.mRequestID = q.request != 0 ? (int)reinterpret_cast<EEAutoPathRequest*>(q.request)->mRequestID : -1;
        if (q.queue == 0 && q.machine != 0) {
            EeAStarPathSearchInfo m;
            ee_astar_path_search_info(reinterpret_cast<const void*>(q.machine), false, &m);
            e.mType = m.machine_type;
            e.mRevs = m.revs;
            e.mSteps = m.steps;
            e.mFound = m.found ? 1 : 0;
            e.mMaxRevs = m.max_revs;
            e.mRevLimit = m.rev_limit;
        } else if (q.queue == 1 && q.machine != 0) {
            EeAStarGraphSearchInfo m;
            ee_astar_graph_search_info(reinterpret_cast<const void*>(q.machine), &m);
            e.mType = m.machine_type;
            e.mRevs = m.revs;
            e.mSteps = m.steps;
            e.mFound = m.found ? 1 : 0;
            e.mMaxRevs = m.max_revs;
            e.mRevLimit = m.rev_limit;
        }
        outEntries.push_back(e);
    }
}

static unsigned int EEPathCaptureUnitID(EEComplexUnit* inUnit)
{
    return inUnit != NULL ? (unsigned int)inUnit->mUnitID : 0xffffffffu;
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

void EEPathManager::Update()
{
    EEPathRequestCapture theCapture(kPathRequestUpdate, this);   // TEST ONLY
    this->Call(1);
}

void EEPathManager::Reset()
{
    EEPathRequestCapture theCapture(kPathRequestReset, this);   // TEST ONLY
    this->Call(2);
}

void EEPathManager::Clear()
{
    this->Call(3);
}

void EEPathManager::FreePaths()
{
    this->Call(4);
}

void EEPathManager::RequestPendingPaths(EEMap* inMap)
{
    EEPathRequestCapture theCapture(kPathRequestPending, this);   // TEST ONLY
    if (theCapture.On()) {
        int32_t thePending[3];
        theCapture.mRecord.U32((unsigned int)this->Call(20));
        for (uint32_t k = 0; ee_path_manager_pending(this->mRust, k, thePending); k++) {
            theCapture.mRecord.U32((unsigned int)thePending[0]);
            theCapture.mRecord.I32(thePending[1]);
            theCapture.mRecord.I32(thePending[2]);
        }
    }
    this->Call(5);
}

void EEPathManager::SetRegionUpdates(bool inOn, bool inNow)
{
    EEPathRequestCapture theCapture(kPathRequestRegionUpdates, this);   // TEST ONLY
    if (theCapture.On()) {
        theCapture.mRecord.U32(inOn ? 1 : 0);
        theCapture.mRecord.U32(inNow ? 1 : 0);
        theCapture.mRecord.U32((unsigned int)this->Call(25));
        theCapture.mRecord.U32((unsigned int)this->Call(24));
    }
    this->Call(6, 0, 0, {inOn, inNow});
}

void EEPathManager::CancelPaths(EEComplexUnit* inUnit, bool inAutoPaths)
{
    EEPathRequestCapture theCapture(kPathRequestCancelPaths, this);   // TEST ONLY
    if (theCapture.On()) {
        theCapture.mRecord.U32(EEPathCaptureUnitID(inUnit));
        theCapture.mRecord.U32(inAutoPaths ? 1 : 0);
    }
    this->Call(7, EEPathHandle(inUnit), 0, {inAutoPaths});
}

void EEPathManager::CancelAutoPathRequest(UReferenceCounter* inRequest)
{
    EEPathRequestCapture theCapture(kPathRequestCancelRequest, this);   // TEST ONLY
    if (theCapture.On())
        theCapture.Request(static_cast<EEAutoPathRequest*>(inRequest));
    this->Call(8, EEPathHandle(static_cast<EEAutoPathRequest*>(inRequest)));
}

void EEPathManager::CompletePath(EEComplexUnit* inUnit)
{
    EEPathRequestCapture theCapture(kPathRequestCompletePath, this);   // TEST ONLY
    if (theCapture.On()) {
        theCapture.mRecord.U32(EEPathCaptureUnitID(inUnit));
        theCapture.Unit(EEPathCaptureUnitID(inUnit));
    }
    this->Call(9, EEPathHandle(inUnit));
}

void EEPathManager::CompleteAutoPath(EEAutoPathRequest* inRequest)
{
    EEPathRequestCapture theCapture(kPathRequestCompleteAutoPath, this);   // TEST ONLY
    if (theCapture.On())
        theCapture.Request(inRequest);
    this->Call(10, EEPathHandle(inRequest));
}

bool EEPathManager::HasAutoPath(EEComplexUnit* inUnit)
{
    EEPathRequestCapture theCapture(kPathRequestHasAutoPath, this);   // TEST ONLY
    if (theCapture.On())
        theCapture.mRecord.U32(EEPathCaptureUnitID(inUnit));
    bool theResult = this->Call(11, EEPathHandle(inUnit)) != 0;
    if (theCapture.On())
        theCapture.mRecord.U32(theResult ? 1 : 0);
    return theResult;
}

bool EEPathManager::IsPathDone(EEComplexUnit* inUnit)
{
    EEPathRequestCapture theCapture(kPathRequestIsPathDone, this);   // TEST ONLY
    if (theCapture.On())
        theCapture.mRecord.U32(EEPathCaptureUnitID(inUnit));
    bool theResult = this->Call(12, EEPathHandle(inUnit)) != 0;
    if (theCapture.On())
        theCapture.mRecord.U32(theResult ? 1 : 0);
    return theResult;
}

bool EEPathManager::GetPathGoal(EEComplexUnit* inUnit, U2DPoint<long>& outTile)
{
    EEPathRequestCapture theCapture(kPathRequestGetPathGoal, this);   // TEST ONLY
    if (theCapture.On())
        theCapture.mRecord.U32(EEPathCaptureUnitID(inUnit));
    int64_t theTile[2] = {0, 0};
    bool theResult = this->Call(13, EEPathHandle(inUnit), 0, {}, theTile) != 0;
    outTile.mX = (long)theTile[0];
    outTile.mY = (long)theTile[1];
    if (theCapture.On()) {
        theCapture.mRecord.U32(theResult ? 1 : 0);
        theCapture.Tile(outTile);
    }
    return theResult;
}

EEAutoPathRequest* EEPathManager::RequestPath(unsigned long inPlayerID, const EETileCoordinate& inFrom,
                                              const EETileCoordinate& inTo, EECPASGFlags* inFlags, long inType,
                                              long inRevs, EeAStarModifier* inModifier, bool inUnlimitedRevs)
{
    EEPathRequestCapture theCapture(kPathRequestPath, this);   // TEST ONLY
    if (theCapture.On()) {
        theCapture.mRecord.U32((unsigned int)inPlayerID);
        theCapture.Tile(inFrom);
        theCapture.Tile(inTo);
        theCapture.mRecord.I32((int)inFlags->mPlayerID);
        for (int f = 0; f < 17; f++)
            theCapture.mRecord.U32(inFlags->mFlags[f] ? 1 : 0);
        theCapture.mRecord.I32((int)inType);
        theCapture.mRecord.I32((int)inRevs);
        theCapture.mRecord.U32(inModifier != NULL ? 1 : 0);
        theCapture.mRecord.U32(inUnlimitedRevs ? 1 : 0);
    }
    EEAutoPathRequest* theRequest = reinterpret_cast<EEAutoPathRequest*>(
        this->Call(14, EEPathHandle(inFlags), EEPathHandle(inModifier),
                   {(int64_t)inPlayerID, inFrom.mX, inFrom.mY, inTo.mX, inTo.mY, inType, inRevs, inUnlimitedRevs}));
    if (theCapture.On())
        theCapture.Request(theRequest);
    return theRequest;
}

void EEPathManager::RequestUnitPath(EEComplexUnit* inUnit, const EETileCoordinate& inFrom, const EETileCoordinate& inTo,
                                    bool inUnlimitedRevs, long inRevs, bool inUseGraph, EeAStarModifier* inModifier, bool inArg8)
{
    EEPathRequestCapture theCapture(kPathRequestUnitPath, this);   // TEST ONLY
    if (theCapture.On()) {
        theCapture.mRecord.U32(EEPathCaptureUnitID(inUnit));
        theCapture.Tile(inFrom);
        theCapture.Tile(inTo);
        theCapture.mRecord.U32(inUnlimitedRevs ? 1 : 0);
        theCapture.mRecord.I32((int)inRevs);
        theCapture.mRecord.U32(inUseGraph ? 1 : 0);
        theCapture.mRecord.U32(inModifier != NULL ? 1 : 0);
        theCapture.mRecord.U32(inArg8 ? 1 : 0);
        theCapture.Unit(EEPathCaptureUnitID(inUnit));
    }
    this->Call(15, EEPathHandle(inUnit), EEPathHandle(inModifier),
               {inFrom.mX, inFrom.mY, inTo.mX, inTo.mY, inUnlimitedRevs, inRevs, inUseGraph, inArg8});
}

void EEPathManager::RequestPendingPath(EEComplexUnit* inUnit, const EETileCoordinate& inTile)
{
    this->Call(16, EEPathHandle(inUnit), 0, {inTile.mX, inTile.mY});
}

void EEPathManager::AddPending(unsigned long inUnitID, const EETileCoordinate& inTile)
{
    this->Call(17, 0, 0, {(int64_t)inUnitID, inTile.mX, inTile.mY});
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

// TEST ONLY (EE-SIM's state dumps): the searches in progress, the pending requests and the timers (pm.rs), then the
// bounded machine's storage (FindPath, FindBoundedPath: one machine kept from search to search) as a digest of its
// bytes (ee_astar storage_save.rs) and the C++ members around it
void EEPathManager::DumpSim(FILE* inFile)
{
    std::vector<char> theText(ee_path_manager_dump(this->mRust, NULL, 0));
    ee_path_manager_dump(this->mRust, reinterpret_cast<uint8_t*>(theText.data()), (uint32_t)theText.size());
    fwrite(theText.data(), 1, theText.size(), inFile);
    const void* theBounded = ee_path_manager_bounded(this->mRust);
    EeAStarPathSearchInfo theInfo = {};
    theInfo.storage_hash = 2166136261u;
    if (theBounded != NULL)
        ee_astar_path_search_info(theBounded, true, &theInfo);
    fprintf(inFile, "BOUNDED %d bytes=%lu hash=%08lx", theBounded != NULL ? 1 : 0, (unsigned long)theInfo.storage_len,
            (unsigned long)theInfo.storage_hash);
    if (theBounded != NULL)
        fprintf(inFile, " size=%ld,%ld map=%ld,%ld found=%d", (long)theInfo.width, (long)theInfo.height,
                (long)theInfo.map_width, (long)theInfo.map_height, theInfo.found ? 1 : 0);
    fprintf(inFile, "\n");
}

// Writes the path sub sectors and the queued tile and graph searches to an out-of-synch dump
void EEPathManager::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP: Pathfinding Information\nOUT OF SYNCH DUMP:----------------------------------\n");
    fprintf(inFile, "OUT OF SYNCH DUMP: Path SubSectors: %zu\n", gContinentManager.GetPathSubSectors().size());
    for (long i = gContinentManager.GetPathSubSectors().size() - 1; i > 0; i--) {
        EEAIPathSubSector* theSector = gContinentManager.GetPathSubSector((unsigned short)i);
        if (theSector)
            fprintf(inFile, "OUT OF SYNCH DUMP: PathSubSector[%d] with center point: %ld,%ld\n", theSector->GetID(),
                    theSector->GetLocation().GetX(), theSector->GetLocation().GetY());
    }
    std::vector<EePathQueued> theQueued(ee_path_manager_queued(this->mRust, NULL, 0));
    ee_path_manager_queued(this->mRust, theQueued.data(), (uint32_t)theQueued.size());
    for (unsigned int q = 0; q < 2; q++) {
        size_t theCount = 0;
        for (size_t j = 0; j < theQueued.size(); j++)
            theCount += theQueued[j].queue == q;
        fprintf(inFile, q == 0 ? "OUT OF SYNCH DUMP: Paths In progress: %zu\n" : "OUT OF SYNCH DUMP: Graphs In progress: %zu\n",
                theCount);
        long k = 0;
        for (size_t j = 0; j < theQueued.size(); j++) {
            const EePathQueued& e = theQueued[j];
            if (e.queue != q)
                continue;
            EEComplexUnit* theUnit = NULL;
            EEUnitHandleResolve(e.unit, theUnit);
            unsigned long theUnitID = theUnit != NULL ? theUnit->GetUnitID() : 0;
            fprintf(inFile, "OUT OF SYNCH DUMP:\n");
            if (q == 0) {
                // the node's machine (the node itself used to be cast, which always failed)
                fprintf(inFile, "OUT OF SYNCH DUMP: Paths Queue[%ld]: \n", k++);
                fprintf(inFile, "OUT OF SYNCH DUMP: Path for Unit: %ld to (%ld,%ld) has been queued %ld time.\n", theUnitID,
                        (long)e.x, (long)e.y, (long)e.passes);
                if (e.machine != 0) {
                    EeAStarPathSearchInfo theInfo;
                    ee_astar_path_search_info(reinterpret_cast<const void*>(e.machine), false, &theInfo);
                    fprintf(inFile, "OUT OF SYNCH DUMP: Revs completed: %ld. Tile Cap is: %ld Pathfinder Type is: %ld\n",
                            (long)theInfo.revs, (long)theInfo.max_revs, (long)theInfo.machine_type);
                }
            } else {
                fprintf(inFile, "OUT OF SYNCH DUMP: AutoPaths Queue[%ld]: \n", k++);
                fprintf(inFile, "OUT OF SYNCH DUMP: AutoPath for Unit: %ld to (%ld,%ld) has been queued %ld time.\n",
                        theUnitID, (long)e.x, (long)e.y, (long)e.passes);
                if (e.machine != 0) {
                    EeAStarGraphSearchInfo theInfo;
                    ee_astar_graph_search_info(reinterpret_cast<const void*>(e.machine), &theInfo);
                    fprintf(inFile, "OUT OF SYNCH DUMP: Revs completed: %ld. Tile Cap is: %ld\n", (long)theInfo.revs,
                            (long)theInfo.max_revs);
                }
            }
        }
    }
}

// Runs a bounded tile search (at most 200 steps; a window around inFrom when inBounds is empty) and returns its
// path (and the visited tiles when asked): ee_sim::astar pm_bounded.rs find_path
void EEPathManager::FindPath(EEComplexUnit* inUnit, EEMap* inMap, const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo,
                             const EEAIAStarBounds& inBounds, std::vector<U2DPoint<long> >& outPath, long inType,
                             long inRevs, EeAStarModifier* inModifier, std::vector<U2DPoint<long> >* outVisited)
{
    EEPathRequestCapture theCapture(kPathRequestFindPath, this);   // TEST ONLY
    if (theCapture.On()) {
        theCapture.mRecord.U32(EEPathCaptureUnitID(inUnit));
        theCapture.Tile(inFrom);
        theCapture.Tile(inTo);
        EEPathCaptureBounds(theCapture.mRecord, inBounds);
        theCapture.mRecord.I32((int)inType);
        theCapture.mRecord.I32((int)inRevs);
        theCapture.mRecord.U32(inModifier != NULL ? 1 : 0);
        theCapture.mRecord.U32(outVisited != NULL ? 1 : 0);
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
    if (theCapture.On()) {
        theCapture.Points(outPath);
        if (outVisited != NULL)
            theCapture.Points(*outVisited);
    }
}

// Runs a bounded search within inMargin tiles of inFrom and, when it reaches inTo, appends the path (start first);
// returns whether it reached inTo (*outReached the same; inArg8 is not read): pm_bounded.rs find_bounded_path
bool EEPathManager::FindBoundedPath(EEComplexUnit* inUnit, EEMap* inMap, long inType, const EETileCoordinate& inFrom,
                                    const EETileCoordinate& inTo, std::vector<GE3DPoint>& outPath, long inMargin,
                                    bool inArg8, bool* outReached)
{
    EEPathRequestCapture theCapture(kPathRequestFindBoundedPath, this);   // TEST ONLY
    if (theCapture.On()) {
        theCapture.mRecord.U32(EEPathCaptureUnitID(inUnit));
        theCapture.mRecord.I32((int)inType);
        theCapture.Tile(inFrom);
        theCapture.Tile(inTo);
        theCapture.mRecord.I32((int)inMargin);
        EEPathCapturePoints(theCapture.mRecord, outPath);
    }
    bool theReached = this->Call(28, EEPathHandle(inUnit), EEPathHandle(inMap),
                                 {inFrom.mX, inFrom.mY, inTo.mX, inTo.mY, inType, inMargin, (int64_t)EEPathHandle(&outPath)}) != 0;
    *outReached = theReached;
    if (theCapture.On()) {
        theCapture.mRecord.U32(theReached ? 1 : 0);
        EEPathCapturePoints(theCapture.mRecord, outPath);
    }
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
