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
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIAStarFastStorage.h"
#include "ee/EEAIAStarBoundedStorage.h"
#include "ee/EEAIAStarGraphStorage.h"
#include "ee/EEAIAStarGoalBase.h"
#include "ee/EEAIAStarGraphGoalBase.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIBoundedPathGoalModifierBase.h"
#include "ee/EEAIAStarPathfindingGoalBase.h"
#include "ee/EEAIAStarPathfindingGoal.h"
#include "ee/EEAIAStarBoundedPathGoal.h"
#include "ee/EECPASGFlags.h"
#include "ee/EEAIPathGoalModifiers.h"
#include "ee/EEStormModifiers.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EEAIAStarPathGraphGoal.h"
#include "ee/EEAIAStarMachine.h"
#include "ee/EEAIAStarMachineSteps.h"
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

// The path goal's sea and space modifiers (vtables 0x43c458/0x43c448; sizeof 0xc; the ctor is inline: refcount 1, the
// goal, the vptr). Their slots +0x04 0xaa63b / 0x10b8a and +0x08 0xaa65b forward to the base's shared tests
// (provisional member names)
class EEAIPathGoalModifierSea : public EEAIPathGoalModifierBase {
public:
    EEAIPathGoalModifierSea(EEAIAStarPathfindingGoalBase* inGoal) { this->mGoal = inGoal; }
    virtual long GetCost(long inFromX, long inFromY, long inX, long inY, long inParentX, long inParentY)
    {
        return this->GetStepCost(true, inFromX, inFromY, inX, inY, inParentX, inParentY);
    }
    virtual bool CanEnter(EETile* inTile, long inX, long inY, bool inGoal)
    {
        return this->IsPassable(this->mGoal, NULL, inX, inY, inGoal);
    }
};

class EEAIPathGoalModifierSpace : public EEAIPathGoalModifierBase {
public:
    EEAIPathGoalModifierSpace(EEAIAStarPathfindingGoalBase* inGoal) { this->mGoal = inGoal; }
    virtual long GetCost(long inFromX, long inFromY, long inX, long inY, long inParentX, long inParentY)
    {
        return this->GetStepCost(false, inFromX, inFromY, inX, inY, inParentX, inParentY);
    }
    virtual bool CanEnter(EETile* inTile, long inX, long inY, bool inGoal)
    {
        return this->IsPassable(this->mGoal, NULL, inX, inY, inGoal);
    }
};

// The bounded goal's sea/space modifiers EEAIBPGModifierSea/Space: ee/EEAIBoundedPathGoalModifierBase.h

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
            EEAIAStarMachineBase* m = reinterpret_cast<EEAIAStarMachineBase*>(q.machine);
            e.mType = (int)m->mType;
            e.mRevs = (int)m->mRevs;
            e.mSteps = (int)m->mSteps;
            e.mFound = m->mFound ? 1 : 0;
            e.mMaxRevs = (int)m->GetMaxRevs();
            e.mRevLimit = (int)m->GetRevLimit();
        } else if (q.queue == 1 && q.machine != 0) {
            EEGraphPathMachine* m = reinterpret_cast<EEGraphPathMachine*>(q.machine);
            e.mType = (int)m->mType;
            e.mRevs = (int)m->mRevs;
            e.mSteps = (int)m->mSteps;
            e.mFound = m->mFound ? 1 : 0;
            e.mMaxRevs = (int)m->GetMaxRevs();
            e.mRevLimit = (int)m->GetRevLimit();
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

static EEAIAStarMachineBase* EEPathTile(uint64_t inHandle)
{
    return reinterpret_cast<EEAIAStarMachineBase*>(inHandle);
}

static EEPathMachine* EEPathOf(uint64_t inHandle)
{
    return static_cast<EEPathMachine*>(EEPathTile(inHandle));
}

static EEGraphPathMachine* EEGraphOf(uint64_t inHandle)
{
    return reinterpret_cast<EEGraphPathMachine*>(inHandle);
}

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

// SetPathRevs' inputs: the distance, whether the unit is amphibious, whether both tiles are on one continent
static void EEPathRevsInputs(EEComplexUnit* inUnit, const EETileCoordinate& inFrom, const EETileCoordinate& inTo,
                             int64_t* outValues)
{
    EEAIContinent* theFromContinent = gContinentManager.GetContinentAt(inFrom.GetX(), inFrom.GetY());
    EEAIContinent* theToContinent = gContinentManager.GetContinentAt(inTo);
    outValues[0] = inFrom.DistanceTo(inTo);
    outValues[1] = inUnit->mUnitClass->CanMoveOnTerrain(inUnit, 0) && inUnit->mUnitClass->CanMoveOnTerrain(inUnit, 1);
    outValues[2] = theFromContinent == theToContinent;
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

// the graph search's modifier kinds in a saved game (pm.rs EE_PATH_MODIFIER_*; 3..8 were the attack sub groups'
// route planners, deleted with the C++ combat tree in main's stage 5)
enum { kGraphModifierNone = 0, kGraphModifierEnemyAvoidance, kGraphModifierWallPlanner, kGraphModifierStormRain = 9,
       kGraphModifierStormSnow };

long EEGraphModifierKindOf(EEAIGraphPathGoalModifierBase* inModifier)
{
    if (inModifier == NULL)
        return kGraphModifierNone;
    const std::type_info& theType = typeid(*inModifier);
    if (theType == typeid(EEAIGPModifierEnemyAvoidance))
        return kGraphModifierEnemyAvoidance;
    if (theType == typeid(EEAIGPGModifierComputerPlayerWallPlanner))
        return kGraphModifierWallPlanner;
    if (theType == typeid(EEAIGPGModifierStormRoutePlannerRain))
        return kGraphModifierStormRain;
    if (theType == typeid(EEAIGPGModifierStormRoutePlannerSnow))
        return kGraphModifierStormSnow;
    return -1;
}

void* EEGraphObjectOf(void* inContext, int32_t inIndex)
{
    (void)inContext;
    EEAIHandleView<EEAIPathSubSector> theSectors = gContinentManager.GetPathSubSectors();
    if (inIndex < 0 || (size_t)inIndex >= theSectors.size() || theSectors[inIndex] == NULL)
        return NULL;
    return static_cast<AStarGraphableObjectBase*>(theSectors[inIndex]);
}

EEAIPathSubSector* EESubSectorOf(long inID)
{
    if (inID < 0 || (size_t)inID >= gContinentManager.GetPathSubSectors().size())
        return NULL;
    return gContinentManager.GetPathSubSectors()[inID];
}

void EEPathBounds(int32_t* outValues, const EEAIAStarBounds& inBounds)
{
    outValues[0] = (int32_t)inBounds.mBottom;
    outValues[1] = (int32_t)inBounds.mLeft;
    outValues[2] = (int32_t)inBounds.mRight;
    outValues[3] = (int32_t)inBounds.mTop;
}

void EEPathSetBounds(EEAIAStarBounds& outBounds, const int32_t* inValues)
{
    outBounds.mBottom = inValues[0];
    outBounds.mLeft = inValues[1];
    outBounds.mRight = inValues[2];
    outBounds.mTop = inValues[3];
}

// a tile search's machine and goal into the saved record (SaveSearches)
bool EESavePathMachine(EEAIAStarMachineBase* inMachine, EePathSavedMachine& outSaved, EePathSavedLists* outLists)
{
    if (inMachine->mType == kAIMachineBoundedPath)
        return false;
    EEPathMachine* theMachine = dynamic_cast<EEPathMachine*>(inMachine);
    if (theMachine == NULL)
        return false;
    const EEAIAStarPathfindingGoal& theGoal = theMachine->mGoal;
    outSaved.tile[0] = (int32_t)theMachine->mTile.mX;
    outSaved.tile[1] = (int32_t)theMachine->mTile.mY;
    const long theFields[8] = {theMachine->mType, theMachine->mRevs, theMachine->mNumRuns, theMachine->mSteps,
                               theMachine->mMapWidth, theMachine->mMapHeight, theMachine->mWidth, theMachine->mHeight};
    for (int f = 0; f < 8; f++)
        outSaved.fields[f] = (int32_t)theFields[f];
    outSaved.check_corners = theMachine->mCheckCorners;
    outSaved.found = theMachine->mFound;
    outSaved.start[0] = (int32_t)theGoal.mStartTile.mX;
    outSaved.start[1] = (int32_t)theGoal.mStartTile.mY;
    outSaved.goal[0] = (int32_t)theGoal.mGoalTile.mX;
    outSaved.goal[1] = (int32_t)theGoal.mGoalTile.mY;
    outSaved.rev_limit = (int32_t)theGoal.mRevLimit;
    outSaved.max_revs = (int32_t)theGoal.mMaxRevs;
    const bool theFlags[6] = {theGoal.mSearchFailed, theGoal.mB29, theGoal.mAcceptNearest, theGoal.mB2B, theGoal.mB2C,
                              theGoal.mTookNearest};
    for (int f = 0; f < 6; f++)
        outSaved.flags[f] = theFlags[f];
    for (size_t i = 0; i < theGoal.mBlockedTiles.size(); i++)
        ee_path_saved_tile_push(outLists, 0, (int32_t)theGoal.mBlockedTiles[i].mX, (int32_t)theGoal.mBlockedTiles[i].mY);
    for (size_t i = 0; i < theGoal.mUnitTiles.size(); i++)
        ee_path_saved_tile_push(outLists, 1, (int32_t)theGoal.mUnitTiles[i].mX, (int32_t)theGoal.mUnitTiles[i].mY);
    EEPathBounds(outSaved.bounds[0], theGoal.mBounds24);
    EEPathBounds(outSaved.bounds[1], theGoal.mGoalBounds);
    EEPathBounds(outSaved.bounds[2], theGoal.mStartBounds);
    outSaved.destination[0] = (int32_t)theGoal.mDestination.mX;
    outSaved.destination[1] = (int32_t)theGoal.mDestination.mY;
    outSaved.l64 = (int32_t)theGoal.mL64;
    outSaved.offset[0] = theGoal.mOffsetX;
    outSaved.offset[1] = theGoal.mOffsetY;
    outSaved.continent = theGoal.mContinentID;
    outSaved.bounded = theGoal.mIsBounded;
    outSaved.avoid_units = theGoal.mAvoidUnits;
    std::vector<unsigned char> theBytes(ee_astar_save(theMachine->mCore, NULL, 0));
    ee_astar_save(theMachine->mCore, theBytes.data(), (uint32_t)theBytes.size());
    uint32_t theLength = 0;
    ee_path_saved_bytes(ee_path_saved_storage(outLists), theBytes.data(), (uint32_t)theBytes.size(), &theLength);
    return true;
}

// the saved record back into a new machine for inUnit (the loaded game's first tick)
bool EELoadPathMachine(EEPathMachine* inMachine, const EePathSavedMachine& inSaved, EePathSavedLists* inLists,
                       EEComplexUnit* inUnit)
{
    uint32_t theLength = 0;
    const uint8_t* theBytes = ee_path_saved_bytes(ee_path_saved_storage(inLists), NULL, 0, &theLength);
    if (!ee_astar_load(inMachine->mCore, theBytes, theLength))
        return false;
    inMachine->mWidth = inSaved.fields[6];
    inMachine->mHeight = inSaved.fields[7];
    inMachine->mTile.mX = inSaved.tile[0];
    inMachine->mTile.mY = inSaved.tile[1];
    inMachine->mRevs = inSaved.fields[1];
    inMachine->mNumRuns = inSaved.fields[2];
    inMachine->mSteps = inSaved.fields[3];
    inMachine->mMapWidth = inSaved.fields[4];
    inMachine->mMapHeight = inSaved.fields[5];
    inMachine->mCheckCorners = inSaved.check_corners;
    inMachine->mFound = inSaved.found;
    EEAIAStarPathfindingGoal& theGoal = inMachine->mGoal;
    theGoal.Attach(gWorld.mMap, inMachine, inMachine);   // as Run does at a search's start
    theGoal.SetUnit(inUnit);
    theGoal.mStartTile = U2DPoint<long>(inSaved.start[0], inSaved.start[1]);
    theGoal.mGoalTile = U2DPoint<long>(inSaved.goal[0], inSaved.goal[1]);
    theGoal.mRevLimit = inSaved.rev_limit;
    theGoal.mMaxRevs = inSaved.max_revs;
    theGoal.mSearchFailed = inSaved.flags[0];
    theGoal.mB29 = inSaved.flags[1];
    theGoal.mAcceptNearest = inSaved.flags[2];
    theGoal.mB2B = inSaved.flags[3];
    theGoal.mB2C = inSaved.flags[4];
    theGoal.mTookNearest = inSaved.flags[5];
    for (int which = 0; which < 2; which++) {
        std::vector<U2DPoint<long> >& theTiles = which == 0 ? theGoal.mBlockedTiles : theGoal.mUnitTiles;
        theTiles.clear();
        int32_t theTile[2];
        for (uint32_t k = 0; ee_path_saved_tile(inLists, which, k, theTile); k++)
            theTiles.push_back(U2DPoint<long>(theTile[0], theTile[1]));
    }
    EEPathSetBounds(theGoal.mBounds24, inSaved.bounds[0]);
    EEPathSetBounds(theGoal.mGoalBounds, inSaved.bounds[1]);
    EEPathSetBounds(theGoal.mStartBounds, inSaved.bounds[2]);
    theGoal.mDestination = U2DPoint<long>(inSaved.destination[0], inSaved.destination[1]);
    theGoal.mL64 = inSaved.l64;
    theGoal.mOffsetX = inSaved.offset[0];
    theGoal.mOffsetY = inSaved.offset[1];
    theGoal.mContinentID = inSaved.continent;
    theGoal.mIsBounded = inSaved.bounded;
    theGoal.mAvoidUnits = inSaved.avoid_units;
    return true;
}

// a graph search's machine, goal and modifier into the saved record (SaveGraphSearches), when the load can rebuild
// every part of it
bool EESaveGraphMachine(EEGraphPathMachine* inMachine, EePathSavedGraph& outSaved, void* outStorage)
{
    EEAIAStarPathGraphGoal& theGoal = inMachine->mGoal;
    long theKind = EEGraphModifierKindOf(theGoal.mModifier);
    if (theKind < 0 || inMachine->mNodeCount != (long)gContinentManager.GetPathSubSectors().size() + 1
        || !ee_astar_graph_objects_match(inMachine->mCore, EEGraphObjectOf, NULL))
        return false;
    const long theMachine[9] = {inMachine->mTile.mX, inMachine->mTile.mY, inMachine->mType, inMachine->mRevs,
                                inMachine->mNumRuns, inMachine->mSteps, inMachine->mSector, inMachine->mNodeCount,
                                inMachine->mCount};
    for (int f = 0; f < 9; f++)
        outSaved.machine[f] = (int32_t)theMachine[f];
    outSaved.found = inMachine->mFound;
    const long theFields[17] = {theGoal.mStartTile.mX, theGoal.mStartTile.mY, theGoal.mGoalTile.mX, theGoal.mGoalTile.mY,
                                theGoal.mPoint38.mX, theGoal.mPoint38.mY, theGoal.mPoint40.mX, theGoal.mPoint40.mY,
                                theGoal.mPlayerID, theGoal.mRevLimit, theGoal.mMaxRevs, theGoal.mGoalSector,
                                theGoal.mStartSector, theGoal.mL70,
                                theGoal.mUnit != NULL ? (long)theGoal.mUnit->mUnitID : -1L,
                                theGoal.mFrom == NULL ? -1L : (long)theGoal.mFrom->mID,
                                theGoal.mTo == NULL ? -1L : (long)theGoal.mTo->mID};
    for (int f = 0; f < 17; f++)
        outSaved.goal[f] = (int32_t)theFields[f];
    const bool theFlags[6] = {theGoal.mSearchFailed, theGoal.mB31, theGoal.mAcceptNearest, theGoal.mB33, theGoal.mB34,
                              theGoal.mTookNearest};
    for (int f = 0; f < 6; f++)
        outSaved.flags[f] = theFlags[f];
    outSaved.path_flags_player = (int32_t)theGoal.mPathFlags.mPlayerID;
    for (int f = 0; f < 17; f++)
        outSaved.path_flags[f] = theGoal.mPathFlags.mFlags[f];
    outSaved.modifier = (int32_t)theKind;
    if (theKind != kGraphModifierNone && theKind != kGraphModifierStormRain && theKind != kGraphModifierStormSnow) {
        EEAIGPGModiferUseInfluenceCostsBase* theInfluence = static_cast<EEAIGPGModiferUseInfluenceCostsBase*>(theGoal.mModifier);
        for (int w = 0; w < 15; w++)
            outSaved.weights[w] = theInfluence->mWeights.mValues[w];
        outSaved.modifier_player = (uint32_t)theInfluence->mPlayerID;
        outSaved.modifier_enemy = (uint32_t)theInfluence->mEnemyID;
        // (the land route planners' continents and flags and the sub group owner: none since main's stage 5)
    }
    std::vector<unsigned char> theBytes(ee_astar_graph_save(inMachine->mCore, NULL, 0));
    ee_astar_graph_save(inMachine->mCore, theBytes.data(), (uint32_t)theBytes.size());
    uint32_t theLength = 0;
    ee_path_saved_bytes(outStorage, theBytes.data(), (uint32_t)theBytes.size(), &theLength);
    return true;
}

// the saved modifier, NULL for a kind that cannot be rebuilt (the caller's reference)
EEAIGraphPathGoalModifierBase* EENewGraphModifier(const EePathSavedGraph& s)
{
    EEAIGPGModiferUseInfluenceCostsBase* theInfluence = NULL;
    switch (s.modifier) {
    case kGraphModifierEnemyAvoidance:
        theInfluence = new EEAIGPModifierEnemyAvoidance(s.modifier_player);
        break;
    case kGraphModifierWallPlanner:
        theInfluence = new EEAIGPGModifierComputerPlayerWallPlanner(s.modifier_player, s.modifier_enemy);
        break;
    case kGraphModifierStormRain:
        return new EEAIGPGModifierStormRoutePlannerRain;
    case kGraphModifierStormSnow:
        return new EEAIGPGModifierStormRoutePlannerSnow;
    default:
        return NULL;   // (the deleted sub groups' route planners)
    }
    // the members as saved (the ctors take some of them from the sub group's now)
    for (int w = 0; w < 15; w++)
        theInfluence->mWeights.mValues[w] = s.weights[w];
    theInfluence->mPlayerID = s.modifier_player;
    theInfluence->mEnemyID = s.modifier_enemy;
    return theInfluence;
}

// the saved record back into a new graph machine (the loaded game's first tick)
bool EELoadGraphMachine(EEGraphPathMachine* inMachine, const EePathSavedGraph& s, EEAIGraphPathGoalModifierBase* inModifier,
                        EEComplexUnit* inGoalUnit, const uint8_t* inBytes, uint32_t inLength)
{
    inMachine->Reset();
    if (!ee_astar_graph_load(inMachine->mCore, inBytes, inLength, EEGraphObjectOf, NULL))
        return false;
    inMachine->mCount = s.machine[8];
    inMachine->mTile.mX = s.machine[0];
    inMachine->mTile.mY = s.machine[1];
    inMachine->mRevs = s.machine[3];
    inMachine->mNumRuns = s.machine[4];
    inMachine->mSteps = s.machine[5];
    inMachine->mSector = s.machine[6];
    inMachine->mFound = s.found;
    EEAIAStarPathGraphGoal& theGoal = inMachine->mGoal;
    theGoal.Attach(inMachine, inMachine);   // as Run does at a search's start
    theGoal.mStartTile = U2DPoint<long>(s.goal[0], s.goal[1]);
    theGoal.mGoalTile = U2DPoint<long>(s.goal[2], s.goal[3]);
    theGoal.mPlayerID = s.goal[8];
    theGoal.mRevLimit = s.goal[9];
    theGoal.mMaxRevs = s.goal[10];
    theGoal.mGoalSector = s.goal[11];
    theGoal.mStartSector = s.goal[12];
    theGoal.mSearchFailed = s.flags[0];
    theGoal.mB31 = s.flags[1];
    theGoal.mAcceptNearest = s.flags[2];
    theGoal.mB33 = s.flags[3];
    theGoal.mB34 = s.flags[4];
    theGoal.mTookNearest = s.flags[5];
    theGoal.mPoint38 = U2DPoint<long>(s.goal[4], s.goal[5]);
    theGoal.mPoint40 = U2DPoint<long>(s.goal[6], s.goal[7]);
    theGoal.SetUnit(inGoalUnit);
    if (inModifier != NULL)
        theGoal.SetModifier(inModifier);
    theGoal.mFrom = EESubSectorOf(s.goal[15]);
    theGoal.mTo = EESubSectorOf(s.goal[16]);
    theGoal.mPathFlags.mPlayerID = s.path_flags_player;
    for (int f = 0; f < 17; f++)
        theGoal.mPathFlags.mFlags[f] = s.path_flags[f];
    theGoal.mL70 = s.goal[13];
    return true;
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

// The bounded machine's and the units' operations (pm_bounded.rs EE_PATH_BM_*, EE_PATH_UNIT_*, the capture)
static int64_t EEPathBoundedHost(EEPathManager* inManager, int32_t inOp, EePathIo* io)
{
    int64_t* i = io->i;
    EEBoundedPathMachine* theMachine = inManager->mBoundedMachine;
    EEComplexUnit* theUnit = EEPathUnitOf(io->h[0]);
    switch (inOp) {
    case EE_PATH_BM_PREPARE:
        inManager->PrepareBoundedMachine(reinterpret_cast<EEMap*>(io->h[1]));
        return (int64_t)EEPathHandle(static_cast<EEAIAStarMachineBase*>(inManager->mBoundedMachine));
    case EE_PATH_BM_MODIFIER: {
        EEAIPathGoalModifierBase* theModifier = NULL;
        if (i[0] == kAIMachineWaterPath)
            theModifier = new EEAIBPGModifierSea(&theMachine->mGoal);
        else if (i[0] == kAIMachineSpacePath)
            theModifier = new EEAIBPGModifierSpace(&theMachine->mGoal);
        if (theModifier != NULL) {
            theMachine->mGoal.SetModifier(theModifier);
            theModifier->Dereference();
        }
        return 0;
    }
    case EE_PATH_BM_SET_MODIFIER:
        theMachine->mGoal.SetModifier(reinterpret_cast<EEAIPathGoalModifierBase*>(io->h[1]));
        return 0;
    case EE_PATH_BM_SET_UNIT:
        theMachine->mGoal.SetUnit(EEPathUnitOf(io->h[1]));
        return 0;
    case EE_PATH_BM_ENDS:
        theMachine->SetStart(EEPathTileArg(i));
        theMachine->SetGoal(EEPathTileArg(i + 2));
        if (i[4])
            theMachine->mTile = EEPathTileArg(i + 2);
        return 0;
    case EE_PATH_BM_SET_MAX_REVS:
        theMachine->SetMaxRevs((long)i[0]);
        return 0;
    case EE_PATH_BM_MAX_REVS:
        return theMachine->GetMaxRevs();
    case EE_PATH_BM_BOUNDS: {
        EEAIAStarBounds theBounds;
        theBounds.mTop = (long)i[0];
        theBounds.mLeft = (long)i[1];
        theBounds.mBottom = (long)i[2];
        theBounds.mRight = (long)i[3];
        theMachine->mGoal.mBounds = theBounds;
        theMachine->mSearchBounds = theBounds;
        return 0;
    }
    case EE_PATH_BM_RUN:
        theMachine->Run();
        return theMachine->mFound;
    case EE_PATH_BM_TILE:
        EEPathPutTile(i, theMachine->mTile);
        return 0;
    case EE_PATH_BM_RESET:
        theMachine->Reset();
        return 0;
    case EE_PATH_BM_PATH:
        if (io->h[2] == 0)
            theMachine->GetPath(*reinterpret_cast<std::vector<U2DPoint<long> >*>(io->h[1]));
        else
            theMachine->GetPath(*reinterpret_cast<std::vector<U2DPoint<long> >*>(io->h[1]),
                                *reinterpret_cast<std::vector<U2DPoint<long> >*>(io->h[2]));
        return 0;
    case EE_PATH_BM_BOUNDED_START: {   // FindBoundedPath's setup
        EEAIAStarBoundedPathGoal& theGoal = theMachine->mGoal;
        theMachine->CancelSearch();
        theGoal.mMaxRevs = (long)i[0];
        theMachine->SetStart(EEPathTileArg(i + 1));
        theMachine->SetGoal(EEPathTileArg(i + 3));
        theGoal.mDestination = EEPathTileArg(i + 3);
        theGoal.mRevLimit = -1;
        theGoal.SetUnit(EEPathUnitOf(io->h[1]));
        theGoal.mIsBounded = true;
        return 0;
    }
    case EE_PATH_BM_RELEASE_MODIFIER:
        theMachine->mGoal.ReleaseModifier();
        return 0;
    case EE_PATH_BM_RUN_BOUNDED: {   // RunBoundedPath
        EEAIAStarBoundedPathGoal& theGoal = theMachine->mGoal;
        theGoal.mMaxRevs = (long)i[0];
        theGoal.SetUnit(EEPathUnitOf(io->h[1]));
        theGoal.mAcceptNearest = true;
        theMachine->SetStart(EEPathTileArg(i + 1));
        theMachine->SetGoal(EEPathTileArg(i + 3));
        theMachine->Run();
        return !theGoal.mSearchFailed;
    }
    case EE_PATH_BM_GOAL_POINTS: {
        std::vector<GE3DPoint> thePoints;
        theMachine->mGoal.GetPoints(thePoints);
        EEPathPointsOut(io->h[1], thePoints);
        return 0;
    }
    case EE_PATH_TM_POINTS: {
        std::vector<GE3DPoint> thePoints;
        EEAIAStarMachineBase* theTile = EEPathTile(io->h[0]);
        if (theTile->mType == kAIMachineBoundedPath)
            ((EEBoundedPathMachine*)theTile)->GetPoints(thePoints);
        else
            ((EEPathMachine*)theTile)->GetPoints(thePoints);
        EEPathPointsOut(io->h[1], thePoints);
        return 0;
    }
    case EE_PATH_TM_FLAGS: {
        EEAIAStarMachineBase* theTile = EEPathTile(io->h[0]);
        if (theTile->mType == kAIMachineBoundedPath)
            return (((EEBoundedPathMachine*)theTile)->mGoal.mAcceptNearest ? 1 : 0)
                | (((EEBoundedPathMachine*)theTile)->mGoal.mTookNearest ? 2 : 0);
        return (((EEPathMachine*)theTile)->mGoal.mAcceptNearest ? 1 : 0) | (((EEPathMachine*)theTile)->mGoal.mTookNearest ? 2 : 0);
    }
    case EE_PATH_TM_TYPE:
        return EEPathTile(io->h[0])->GetType();
    case EE_PATH_MAP_SIZE:
        return reinterpret_cast<EEMap*>(io->h[1])->mSize;
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
    case EE_PATH_UNIT_LOCATION: {
        float theX = theUnit->mPhysicalModel.GetLocation().GetX(), theY = theUnit->mPhysicalModel.GetLocation().GetY();
        uint32_t theBits[2];
        memcpy(&theBits[0], &theX, 4);
        memcpy(&theBits[1], &theY, 4);
        i[0] = theBits[0];
        i[1] = theBits[1];
        return 0;
    }
    case EE_PATH_UNIT_TILE:
        EEPathPutTile(i, theUnit->GetTile());
        return 0;
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
    case EE_PATH_CAPTURE_ACTIVE:   // TEST ONLY
        return EESimCaptureRecord::Active();
    case EE_PATH_CAPTURE_COMMIT: {
        EESimCaptureRecord theRecord((unsigned int)i[0]);
        theRecord.Bytes(reinterpret_cast<const void*>(io->h[1]), (unsigned int)i[1]);
        theRecord.Commit();
        return 0;
    }
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
    case EE_PATH_UNIT_ID:
        return EEUnitHandleID(EEPathUnitOf(h));
    case EE_PATH_UNIT_RESOLVE: {
        EEComplexUnit* theUnit = NULL;
        EEUnitHandleResolve((uint32_t)i[0], theUnit);
        return (int64_t)EEPathHandle(theUnit);
    }
    case EE_PATH_WORLD_UNIT:
        return (int64_t)EEPathHandle(EEPathWorldUnit(i[0]));
    case EE_PATH_UNIT_REFERENCE:
        EEPathUnitOf(h)->Reference();
        return 0;
    case EE_PATH_UNIT_DEREFERENCE:
        EEPathUnitOf(h)->Dereference();
        return 0;
    case EE_PATH_REQUEST_REFERENCE:
        reinterpret_cast<EEAutoPathRequest*>(h)->Reference();
        return 0;
    case EE_PATH_REQUEST_DEREFERENCE:
        reinterpret_cast<EEAutoPathRequest*>(h)->Dereference();
        return 0;
    case EE_PATH_MODIFIER_DEREFERENCE:
        reinterpret_cast<EEAIGraphPathGoalModifierBase*>(h)->Dereference();
        return 0;
    case EE_PATH_GAME_TIME:
        return gPhysics ? (int64_t)(uint32_t)gPhysics->GetGameTime() : 0;
    case EE_PATH_SUB_SECTORS:
        return (int64_t)gContinentManager.GetPathSubSectors().size();
    case EE_PATH_SUB_SECTOR:
        return EESubSectorOf((long)i[0]) != NULL;
    case EE_PATH_MAP:
        return (int64_t)EEPathHandle(gWorld.mMap);
    case EE_PATH_HAS_MAP:
        return gWorld.mMap != NULL;
    case EE_PATH_LOADING:
        return gLoadingSavedGame;
    case EE_PATH_CLAMP: {   // (on the map h1 when i2)
        EETileCoordinate theTile = EEPathTileArg(i);
        theManager->ClampTile(i[2] ? reinterpret_cast<EEMap*>(io->h[1]) : gWorld.mMap, theTile);
        EEPathPutTile(i, theTile);
        return 0;
    }
    // the tile machines
    case EE_PATH_PM_NEW:
        return (int64_t)EEPathHandle(static_cast<EEAIAStarMachineBase*>(new EEPathMachine(1, reinterpret_cast<EEMap*>(io->h[1]))));
    case EE_PATH_PM_DELETE:
        delete EEPathOf(h);
        return 0;
    case EE_PATH_PM_MAP:
        return (int64_t)EEPathHandle(EEPathOf(h)->mGoal.mMap);
    case EE_PATH_PM_TYPE: {
        EEPathMachine* theMachine = EEPathOf(h);
        EEAIPathGoalModifierBase* theModifier = NULL;
        if (i[0] == kAIMachineWaterPath)
            theModifier = new EEAIPathGoalModifierSea(&theMachine->mGoal);
        else if (i[0] == kAIMachineSpacePath)
            theModifier = new EEAIPathGoalModifierSpace(&theMachine->mGoal);
        if (theModifier != NULL) {
            theMachine->mGoal.SetModifier(theModifier);
            theModifier->Dereference();
        }
        theMachine->mType = (long)i[0];
        return 0;
    }
    case EE_PATH_TM_RUN:
        EEPathTile(h)->Run();
        return EEPathTile(h)->mFound;
    case EE_PATH_TM_FOUND:
        return EEPathTile(h)->mFound;
    case EE_PATH_TM_SET_REV_LIMIT:
        EEPathTile(h)->SetRevLimit((long)i[0]);
        return 0;
    case EE_PATH_TM_REV_LIMIT:
        return EEPathTile(h)->GetRevLimit();
    case EE_PATH_TM_STEPS:
        return EEPathTile(h)->mSteps;
    case EE_PATH_PM_RESET:
        EEPathOf(h)->Reset();
        return 0;
    case EE_PATH_PM_RELEASE: {
        EEPathMachine* theMachine = EEPathOf(h);
        theMachine->Reset();
        theMachine->mFound = false;
        if (i[0]) {
            theMachine->mGoal.ReleaseModifier();
            theMachine->mGoal.SetUnit(0);
        }
        return 0;
    }
    case EE_PATH_PM_START: {
        EEPathMachine* theMachine = EEPathOf(h);
        EETileCoordinate theFrom = EEPathTileArg(i), theTo = EEPathTileArg(i + 2);
        theMachine->CancelSearch();
        theMachine->SetRevLimit((long)i[4]);
        theMachine->SetMaxRevs((long)i[5]);
        theMachine->SetStart(theFrom);
        theMachine->SetGoal(theTo.GetX(), theTo.GetY());
        theMachine->mTile = theTo;
        theMachine->mGoal.mDestination = theTo;
        theMachine->mGoal.SetUnit(EEPathUnitOf(io->h[1]));
        theMachine->mCheckCorners = true;
        return 0;
    }
    case EE_PATH_PATH_REVS:
        EEPathRevsInputs(EEPathUnitOf(h), EEPathTileArg(i), EEPathTileArg(i + 2), i);
        return 0;
    case EE_PATH_SET_MAX_REVS:
        if (i[0] == 0)
            EEPathTile(h)->SetMaxRevs((long)i[1]);
        else
            EEGraphOf(h)->SetMaxRevs((long)i[1]);
        return 0;
    // the graph machines
    case EE_PATH_GM_NEW:
        return (int64_t)EEPathHandle(new EEGraphPathMachine((long)i[0], (long)i[1]));
    case EE_PATH_GM_DELETE:
        delete EEGraphOf(h);
        return 0;
    case EE_PATH_GM_COUNT:
        return EEGraphOf(h)->GetCount();
    case EE_PATH_GM_TYPE:
        EEGraphOf(h)->mType = (long)i[0];
        return 0;
    case EE_PATH_GM_RUN:
        EEGraphOf(h)->Run();
        return EEGraphOf(h)->mFound;
    case EE_PATH_GM_FOUND:
        return EEGraphOf(h)->mFound;
    case EE_PATH_GM_SET_REV_LIMIT:
        EEGraphOf(h)->SetRevLimit((long)i[0]);
        return 0;
    case EE_PATH_GM_REV_LIMIT:
        return EEGraphOf(h)->GetRevLimit();
    case EE_PATH_GM_STEPS:
        return EEGraphOf(h)->mSteps;
    case EE_PATH_GM_RESET:
        EEGraphOf(h)->Reset();
        return 0;
    case EE_PATH_GM_RELEASE: {
        EEGraphPathMachine* theMachine = EEGraphOf(h);
        theMachine->Reset();
        theMachine->mFound = false;
        if (i[0]) {
            theMachine->mGoal.ReleaseModifier();
            theMachine->mGoal.SetUnit(0);
        }
        return 0;
    }
    case EE_PATH_GM_START: {   // StartGraphPath's setup: the tiles moved onto the map, the sub sectors' centers
        EEGraphPathMachine* theMachine = EEGraphOf(h);
        EEAutoPathRequest* theRequest = reinterpret_cast<EEAutoPathRequest*>(io->h[2]);
        EEAIGraphPathGoalModifierBase* theModifier = reinterpret_cast<EEAIGraphPathGoalModifierBase*>(io->h[3]);
        EETileCoordinate theFromCenter = EEPathTileArg(i), theToCenter = EEPathTileArg(i + 2);
        EEAIAStarPathGraphGoal& theGoal = theMachine->mGoal;
        theMachine->Reset();
        theGoal.Reset();
        if (!i[5])
            theGoal.mRevLimit = 0xf;
        if (theModifier)
            theGoal.SetModifier(theModifier);
        theManager->ClampTile(gWorld.mMap, theFromCenter);
        EEPathPutTile(i, theFromCenter);
        EEAIPathSubSector* theFromSector = gContinentManager.GetPathSubSectorAt(theFromCenter.GetX(), theFromCenter.GetY());
        theFromCenter = theFromSector->mSector->mCenter;
        theManager->ClampTile(gWorld.mMap, theToCenter);
        EEPathPutTile(i + 2, theToCenter);
        EEAIPathSubSector* theToSector = gContinentManager.GetPathSubSectorAt(theToCenter.GetX(), theToCenter.GetY());
        theToCenter = theToSector->mSector->mCenter;
        theGoal.SetUnit(EEPathUnitOf(io->h[1]));
        if (theRequest)
            theGoal.mPathFlags = theRequest->mFlags;
        theMachine->SetGoal(theFromCenter, theFromSector->mID);
        theMachine->SetStart(theToCenter, theToSector->mID);
        theMachine->mTile = theToCenter;
        theMachine->mSector = theToSector->mID;
        theGoal.mPoint38 = theToCenter;
        theGoal.mFrom = theFromSector;
        theGoal.mTo = theToSector;
        theGoal.mPlayerID = (long)i[4];
        return 0;
    }
    case EE_PATH_GRAPH_REVS: {   // SetGraphRevs' inputs
        EEGraphPathMachine* theMachine = EEGraphOf(h);
        EETileCoordinate theFrom = EEPathTileArg(i), theTo = EEPathTileArg(i + 2);
        EEAIPathSubSector* theSector = gContinentManager.GetPathSubSectorAt(theTo.GetX(), theTo.GetY());
        i[0] = theFrom.DistanceTo(theTo);
        i[1] = theMachine->mGoal.mPathFlags.mFlags[theSector->GetKind()] != 0;
        EEComplexUnit* theUnit = theMachine->mGoal.mUnit;
        i[2] = theUnit && theUnit->GetPlayer()->mComputerPlayer && theUnit->mIsCitizen ? (int64_t)(uint32_t)theUnit->GetUnitID()
                                                                                      : (int64_t)0xffffffffu;
        return 0;
    }
    case EE_PATH_REQUEST_UNIT_PATHS:
        return theManager->RequestUnitPaths(EEPathUnitOf(io->h[1]), reinterpret_cast<EEAutoPathRequest*>(io->h[2]), EEGraphOf(h));
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
    case EE_PATH_UNIT_PLAYER:
        return EEPathUnitOf(h)->GetPlayer()->GetPlayerID();
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
    case EE_PATH_UNIT_PATH_TYPE: {
        EEComplexUnit* theUnit = EEPathUnitOf(h);
        if (!(theUnit->mUnitClass->CanMoveOnTerrain(theUnit, 1)))
            return theUnit->mUnitClass->CanMoveOnTerrain(theUnit, 0xc) ? 0x10 : 1;
        return 2;
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
    case EE_PATH_REGION_DELAY:
        return (int64_t)(uint32_t)UMFloatToLong(gWorld.mGameSpeed * 10000.0f);
    case EE_PATH_BOUNDED_FREE:
        if (theManager->mBoundedMachine)
            theManager->mBoundedMachine->Reset();
        if (theManager->mBoundedMachine)
            delete theManager->mBoundedMachine;
        theManager->mBoundedMachine = NULL;
        return 0;
    case EE_PATH_FREE_POOLS:
        EEAIAStarGraphStorage::FreeNodePool();
        EEAIAStarStorage::FreeNodePool();
        return 0;
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
    // the saved game
    case EE_PATH_PM_SAVE:
        return EESavePathMachine(EEPathTile(h), *reinterpret_cast<EePathSavedMachine*>(io->h[1]),
                                 reinterpret_cast<EePathSavedLists*>(io->h[2]));
    case EE_PATH_PM_LOAD:
        return EELoadPathMachine(EEPathOf(h), *reinterpret_cast<const EePathSavedMachine*>(io->h[1]),
                                 reinterpret_cast<EePathSavedLists*>(io->h[2]), EEPathUnitOf(io->h[3]));
    case EE_PATH_GM_SAVE:
        return EESaveGraphMachine(EEGraphOf(h), *reinterpret_cast<EePathSavedGraph*>(io->h[1]), (void*)(uintptr_t)io->h[2]);
    case EE_PATH_GM_LOAD:
        return EELoadGraphMachine(EEGraphOf(h), *reinterpret_cast<const EePathSavedGraph*>(io->h[1]),
                                  reinterpret_cast<EEAIGraphPathGoalModifierBase*>(io->h[2]), EEPathUnitOf(io->h[3]),
                                  reinterpret_cast<const uint8_t*>(i[0]), (uint32_t)i[1]);
    case EE_PATH_GM_MODIFIER:
        return (int64_t)EEPathHandle(EENewGraphModifier(*reinterpret_cast<const EePathSavedGraph*>(io->h[1])));
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
EEPathManager::EEPathManager() : mRust(ee_path_manager_new(EEPathManagerHost, this)), mBoundedMachine(NULL), mLC8(5)
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
                                              long inRevs, EEAIGraphPathGoalModifierBase* inModifier, bool inUnlimitedRevs)
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
                                    bool inUnlimitedRevs, long inRevs, bool inUseGraph, EEAIGraphPathGoalModifierBase* inModifier, bool inArg8)
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
    unsigned long theHash = 2166136261u, theSize = 0;
    if (this->mBoundedMachine != NULL) {
        std::vector<unsigned char> theBytes(ee_astar_save(this->mBoundedMachine->mCore, NULL, 0));
        ee_astar_save(this->mBoundedMachine->mCore, theBytes.data(), (uint32_t)theBytes.size());
        for (size_t i = 0; i < theBytes.size(); i++)
            theHash = (uint32_t)((theHash ^ theBytes[i]) * 16777619u);
        theSize = (unsigned long)theBytes.size();
    }
    fprintf(inFile, "BOUNDED %d bytes=%lu hash=%08lx", this->mBoundedMachine != NULL ? 1 : 0, theSize, theHash);
    if (this->mBoundedMachine != NULL)
        fprintf(inFile, " size=%ld,%ld map=%ld,%ld found=%d", this->mBoundedMachine->mWidth, this->mBoundedMachine->mHeight,
                this->mBoundedMachine->mMapWidth, this->mBoundedMachine->mMapHeight, this->mBoundedMachine->mFound ? 1 : 0);
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
                    theSector->mLocation.GetX(), theSector->mLocation.GetY());
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
                EEPathMachine* theMachine = dynamic_cast<EEPathMachine*>(EEPathTile(e.machine));
                fprintf(inFile, "OUT OF SYNCH DUMP: Paths Queue[%ld]: \n", k++);
                fprintf(inFile, "OUT OF SYNCH DUMP: Path for Unit: %ld to (%ld,%ld) has been queued %ld time.\n", theUnitID,
                        (long)e.x, (long)e.y, (long)e.passes);
                if (theMachine)
                    fprintf(inFile, "OUT OF SYNCH DUMP: Revs completed: %ld. Tile Cap is: %ld Pathfinder Type is: %ld\n",
                            theMachine->GetRevs(), theMachine->GetMaxRevs(), theMachine->GetType());
            } else {
                EEGraphPathMachine* theMachine = EEGraphOf(e.machine);
                fprintf(inFile, "OUT OF SYNCH DUMP: AutoPaths Queue[%ld]: \n", k++);
                fprintf(inFile, "OUT OF SYNCH DUMP: AutoPath for Unit: %ld to (%ld,%ld) has been queued %ld time.\n",
                        theUnitID, (long)e.x, (long)e.y, (long)e.passes);
                if (theMachine)
                    fprintf(inFile, "OUT OF SYNCH DUMP: Revs completed: %ld. Tile Cap is: %ld\n", theMachine->GetRevs(),
                            theMachine->GetMaxRevs());
            }
        }
    }
}

// Makes sure the bounded path machine exists for the map, recreating it (with corner checks) for another map
void EEPathManager::PrepareBoundedMachine(EEMap* inMap)
{
    if (this->mBoundedMachine != NULL && this->mBoundedMachine->mMap == inMap)
        return;
    if (this->mBoundedMachine) {
        this->mBoundedMachine->Reset();
        delete this->mBoundedMachine;
    }
    this->mBoundedMachine = new EEBoundedPathMachine(4, inMap);
    this->mBoundedMachine->mCheckCorners = true;
}

// Runs a bounded tile search (at most 200 steps; a window around inFrom when inBounds is empty) and returns its
// path (and the visited tiles when asked): ee_sim::astar pm_bounded.rs find_path
void EEPathManager::FindPath(EEComplexUnit* inUnit, EEMap* inMap, const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo,
                             const EEAIAStarBounds& inBounds, std::vector<U2DPoint<long> >& outPath, long inType,
                             long inRevs, EEAIPathGoalModifierBase* inModifier, std::vector<U2DPoint<long> >* outVisited)
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
bool EEPathManager::RequestUnitPaths(EEComplexUnit* inUnit, EEAutoPathRequest* inRequest, EEGraphPathMachine* inMachine)
{
    std::vector<long> thePath;
    EETileCoordinate theTile;
    theTile.mX = 0;
    theTile.mY = 0;
    EETileCoordinate theLast;
    theLast.mX = 0;
    theLast.mY = 0;
    long theDistance = 0;
    bool theAttack = false;
    bool theSet = false;
    inMachine->GetPath(thePath);
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
                theTile = theSector->mLocation;
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
            theLast = theSector->mLocation;
            inRequest->mPath.push_back(theLast);
            if (inMachine->mGoal.GetFrom())
                theDistance = UMAbs(inMachine->mGoal.mFrom->mLocation.GetX() - theLast.GetX()) +
                              UMAbs(inMachine->mGoal.mFrom->mLocation.GetY() - theLast.GetY());
        }
        for (long i = thePath.size() - 2; i >= 0; i--) {
            EEAIPathSubSector* theSector = gContinentManager.GetPathSubSector((unsigned short)thePath[i]);
            theTile = theSector->mLocation;
            inRequest->mPath.push_back(theTile);
            theDistance += UMAbs(theTile.GetX() - theLast.GetX()) + UMAbs(theTile.GetY() - theLast.GetY());
        }
        inRequest->mPathLength = theDistance;
        inRequest->mIsDone = true;
    }
done:
    return inMachine->mGoal.GetTo()->GetID() == inMachine->GetSector();
}

// The path graph machine's goal hooks (ee/EEAIAStarPathGraphGoal.h). GetGoalID and the node tile are read inline
// (0xaacf0, 0xab6c1).
inline unsigned short EEAIAStarPathGraphGoal::GetGoalID() const
{
    return this->mTo->mID;
}

inline U2DPoint<long> EEAIPathSubSector::GetGraphLocation() const
{
    return U2DPoint<long>(this->mSector->mCenter.mX, this->mSector->mCenter.mY);
}

// Returns the Manhattan distance between two tiles times 10 (ICF-folded with EEAIAStarPathfindingGoalBase's)
long EEAIAStarPathGraphGoal::Estimate(const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo) const
{
    return (UMAbs(inFrom.GetX() - inTo.GetX()) + UMAbs(inFrom.GetY() - inTo.GetY())) * 10;
}

// Returns whether the search may enter a sub sector from another, asking the modifier when there is one
bool EEAIAStarPathGraphGoal::CanEnter(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom)
{
    if (!this->mModifier)
        return this->AcceptNeighbor(inSector, inFrom);
    return this->mModifier->CanEnter(inSector, inFrom);
}

// Returns the cost of a step into a sub sector: the modifier's when there is one, else the default cost
long EEAIAStarPathGraphGoal::GetStepCost(EEAIAStarGraphNode* inParent, EEAIPathSubSector* inTo)
{
    if (!this->mModifier)
        return this->GetDefaultStepCost(inParent, inTo);
    return this->mModifier->GetCost(inParent, inTo);
}

// (ICF-folded into 0x410799, the wall planner's +0x04): 1 inside one sector, 10 straight, 14 diagonal
long EEAIAStarPathGraphGoal::GetDefaultStepCost(EEAIAStarGraphNode* inParent, EEAIPathSubSector* inTo)
{
    EEAIPathSector* theFrom = static_cast<EEAIPathSubSector*>(inParent->mObject)->mSector;
    EEAIPathSector* theTo = inTo->mSector;
    if (theFrom != theTo)
        return theFrom->mCenter.mX == theTo->mCenter.mX || theFrom->mCenter.mY == theTo->mCenter.mY ? 10 : 14;
    return 1;
}
