// EEPathManager.h (RECONSTRUCTION; file name inferred). RTTI EEPathManager (vtable 0x43c438, one slot: the deleting
// dtor); AI Thread/EEPathManager.cpp. The game's instance is gPathManager (0x52fcb8; Empire Earth.cpp's init chain
// constructs it; provisional name).
// The manager is Rust (ee_sim::astar pm.rs, the handle mRust): the searches in progress (the original's node pool,
// mPaths, mAutoPaths), the spare machines, the units waiting for the map (mPending), the counters and the request
// record, the requests' and the updates' decisions, the saved game's parts, the PMRQ capture (pm_capture.rs). This
// class is its C++ face: the entry points are inline forwarders (Call's numbers: pm.rs ee_path_manager_call).
#ifndef EE_EEPATHMANAGER_H
#define EE_EEPATHMANAGER_H

#include <vector>
#include <deque>
#include <initializer_list>
#include <stdint.h>
#include "lle/U2DPoint.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEAIListView.h"      // EEAIClearList
#include "rust/sim/astar.h"         // the manager (pm.rs)
#include <stdio.h>                      // FILE

class UReferenceCounter;
class FSFile;
struct EECPASGFlags;
template <class T> class U2DPoint; typedef U2DPoint<long> EETileCoordinate;
class EEAutoPathRequest;                    // ee/EEAutoPathRequest.h
class EEComplexUnit;
struct EEComplexUnitWaypoint;                // ee/EEUnit.h
class EEMap;
template <class T> class U2DRectangle;
typedef U2DRectangle<long> EEAIAStarBounds;   // ee/EEAreaUtilities.h
struct EeAStarModifier;                  // rust/sim/astar.h (a goal's modifier or route planner: modifier_obj.rs)
class GE3DPoint;
class EEAIPathSubSector;
struct EePathManager;                   // rust/sim/astar.h

// a path list FindPath fills
inline void EEPathPushTile(void* inList, int32_t inX, int32_t inY)
    { static_cast<std::vector<U2DPoint<long> >*>(inList)->push_back(U2DPoint<long>(inX, inY)); }

class EEPathManager {
public:
    EEPathManager();
    virtual ~EEPathManager();
    void CancelAutoPathRequest(EEAutoPathRequest* inRequest) { this->Call(8, H(inRequest)); }
    // FILE*: 0xa639b (ret 4; EEPhysics's out-of-synch dump) (provisional name); the text is pm.rs's
    void DumpOutOfSynch(FILE* inFile) { ee_path_manager_dump_file(this->mRust, 1, inFile); }
    // TEST ONLY (EE-SIM's state dumps): the searches in progress, pending requests, timers
    void DumpSim(FILE* inFile) { ee_path_manager_dump_file(this->mRust, 0, inFile); }
    // port (save version 0.278, EESaveHasGraphSearches): the graph searches in progress as they are: node, machine,
    // goal, modifier, request and storage. The original saved none: their owners (attack sub groups, units) asked
    // again at the loaded game's first tick. LoadGraphSearches takes their units out of the pending ones; at the
    // first tick the searches go back to the queue's front and each request to the owner that waited for it
    // (EEAwaitAutoPathRequest: ee_sim::astar request.rs)
    void AddPending(unsigned long inUnitID, const EETileCoordinate& inTile)   // a loaded unit asks its path at StartGame
        { this->Call(17, 0, 0, {(int64_t)inUnitID, inTile.mX, inTile.mY}); }
    void Update() { this->Call(1); }    // 0xa8db8 (EEPhysics::Tick, every tick) (provisional name)
    void Reset() { this->Call(2); }     // 0xa8778 (before a scenario starts; provisional name)
    void RequestPendingPaths(EEMap* inMap) { this->Call(5); }  // 0xa8582 (EEWorld::StartGame; provisional name)
    // 0xa62ce CompleteAutoPath, 0xa634b CompletePath: Rust's (RefreshPath, waypoints.rs)
    // 0xa6948 (EEGroup::Fn206581: the owner's player id, the target tile, the units' flags, 1, -1, 0, 0; returns a new
    // ee/EEAutoPathRequest.h) (provisional name)
    EEAutoPathRequest* RequestPath(unsigned long inPlayerID, const EETileCoordinate& inFrom, const EETileCoordinate& inTo,
                                   EECPASGFlags* inFlags, long inType, long inRevs, EeAStarModifier* inModifier, bool inUnlimitedRevs)
        { return reinterpret_cast<EEAutoPathRequest*>(this->Call(14, H(inFlags), H(inModifier),
              {(int64_t)inPlayerID, inFrom.mX, inFrom.mY, inTo.mX, inTo.mY, inType, inRevs, inUnlimitedRevs})); }
    // 0xa6bfd (EECPWallManager.cpp: inUnit's path from inFrom to inTo inside inBounds, with a path modifier)
    // (provisional name)
    void FindPath(EEComplexUnit* inUnit, EEMap* inMap, const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo,
                  const EEAIAStarBounds& inBounds, std::vector<U2DPoint<long> >& outPath, long inType, long inRevs,
                  EeAStarModifier* inModifier, std::vector<U2DPoint<long> >* outVisited)   // 0x12e2c9 passes &local (the storm's tile list)
        { ee_path_find_path_into(this->mRust, H(inUnit), H(inMap), reinterpret_cast<const EeHullPoint*>(&inFrom), reinterpret_cast<const EeHullPoint*>(&inTo), reinterpret_cast<const EeHullRect*>(&inBounds), inType, inRevs, H(inModifier), &outPath, outVisited, EEPathPushTile, EEAIClearList<std::vector<U2DPoint<long> > >); }
    bool GetPathGoal(EEComplexUnit* inUnit, U2DPoint<long>& outTile)
        { return ee_path_manager_path_goal(this->mRust, H(inUnit), &outTile.mX); }
    void CancelPaths(EEComplexUnit* inUnit, bool inAutoPaths) { this->Call(7, H(inUnit), 0, {inAutoPaths}); }
    void Clear() { this->Call(3); }
    void FreePaths() { this->Call(4); }   // 0xa5f39 (both queues in progress)
    // 0xa71c9 FindBoundedPath: Rust (ee_sim::astar pm_entry.rs find_bounded_path; its callers are Rust)

    // the Rust manager's host (EEPathManager.cpp): what stays C++ (provisional names; addresses are the bodies).
    // TryShortPath, TrimPath, AppendWaypoints, JoinPaths, RunBoundedPath and FinishPath with its found and failed
    // halves are Rust (pm_bounded.rs) over the bounded machine, a Rust search with the path machines (path_search.rs)
    int64_t Call(int32_t inWhat, uint64_t inH0 = 0, uint64_t inH1 = 0, std::initializer_list<int64_t> inValues = {},
                 int64_t* outValues = NULL)
        { return ee_path_manager_call_values(this->mRust, inWhat, inH0, inH1, inValues.begin(), inValues.size(), outValues); }
    // RequestUnitPaths (a found graph path given to the unit or the request) and ClampTile (0xab3aa): Rust
    // (pm_apply.rs over EEPathManager.cpp's one-statement host cases)

    static uint64_t H(const void* inObject) { return (uint64_t)(uintptr_t)inObject; }   // a handle of the Rust calls

    EePathManager* mRust;                               // the manager (ee_sim::astar pm.rs)
    long mLC8;                                          // +0xc8 (= 5; gEEAIRegionSize)
};

extern EEPathManager gPathManager;

#endif
