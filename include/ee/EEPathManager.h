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


class EEPathManager {
public:
    EEPathManager();
    virtual ~EEPathManager();
    void CancelAutoPathRequest(EEAutoPathRequest* inRequest) { this->Call(8, H(inRequest)); }   // 0xa896b (ret 4) (provisional name)
    void DumpOutOfSynch(FILE* inFile);   // FILE*: 0xa639b (ret 4; EEPhysics's out-of-synch dump) (provisional name)
    void DumpSim(FILE* inFile);         // TEST ONLY (EE-SIM's state dumps): the searches in progress, pending requests, timers
    // port (save version 0.256, EEPWorldHelper): the request history, the region maps and the timers, which the
    // original did not save (a load started with none: other search budgets, no stranded units, other prune times)
    void SaveRunningState(FSFile& outFile);
    long LoadRunningState(FSFile& inFile);   // put back at the loaded game's first tick (EEAfterLoad)
    // port (save version 0.276, EESaveHasPathSearches): the units' tile searches in progress as they are, machine,
    // goal and storage. The original saved none: each unit asked its path again from the load
    // (RequestPendingPaths), so the path came later or another one. LoadSearches takes the units it holds out of
    // the pending ones and puts the searches back at the loaded game's first tick, before the queue's others
    void SaveSearches(FSFile& outFile);
    long LoadSearches(FSFile& inFile);
    // port (save version 0.278, EESaveHasGraphSearches): the graph searches in progress as they are: node, machine,
    // goal, modifier, request and storage. The original saved none: their owners (attack sub groups, units) asked
    // again at the loaded game's first tick. LoadGraphSearches takes their units out of the pending ones; at the
    // first tick the searches go back to the queue's front and each request to the owner that waited for it
    // (EEAwaitAutoPathRequest: ee_sim::astar request.rs)
    void SaveGraphSearches(FSFile& outFile);
    long LoadGraphSearches(FSFile& inFile);
    void RequestPendingPath(EEComplexUnit* inUnit, const EETileCoordinate& inTile)   // RequestPendingPaths' one unit
        { this->Call(16, H(inUnit), 0, {inTile.mX, inTile.mY}); }
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
                  EeAStarModifier* inModifier, std::vector<U2DPoint<long> >* outVisited);   // 0x12e2c9 passes &local (the storm's tile list)
    bool HasAutoPath(EEComplexUnit* inUnit) { return this->Call(11, H(inUnit)) != 0; }
    bool IsPathDone(EEComplexUnit* inUnit) { return this->Call(12, H(inUnit)) != 0; }
    bool GetPathGoal(EEComplexUnit* inUnit, U2DPoint<long>& outTile)
    {
        int64_t theTile[2] = {0, 0};
        bool theResult = this->Call(13, H(inUnit), 0, {}, theTile) != 0;
        outTile.mX = (long)theTile[0];
        outTile.mY = (long)theTile[1];
        return theResult;
    }
    void RequestUnitPath(EEComplexUnit* inUnit, const EETileCoordinate& inFrom, const EETileCoordinate& inTo,
                         bool inUnlimitedRevs, long inRevs, bool inUseGraph, EeAStarModifier* inModifier, bool inArg8)
        { this->Call(15, H(inUnit), H(inModifier), {inFrom.mX, inFrom.mY, inTo.mX, inTo.mY, inUnlimitedRevs, inRevs, inUseGraph, inArg8}); }
    void CancelPaths(EEComplexUnit* inUnit, bool inAutoPaths) { this->Call(7, H(inUnit), 0, {inAutoPaths}); }
    void SetRegionUpdates(bool inOn, bool inNow) { this->Call(6, 0, 0, {inOn, inNow}); }
    void Clear() { this->Call(3); }
    void FreePaths() { this->Call(4); }   // 0xa5f39 (both queues in progress)
    // 0xa71c9 FindBoundedPath: Rust (ee_sim::astar pm_entry.rs find_bounded_path; its callers are Rust)

    // the Rust manager's host (EEPathManager.cpp): what stays C++ (provisional names; addresses are the bodies).
    // TryShortPath, TrimPath, AppendWaypoints, JoinPaths, RunBoundedPath and FinishPath with its found and failed
    // halves are Rust (pm_bounded.rs) over the bounded machine, a Rust search with the path machines (path_search.rs)
    int64_t Call(int32_t inWhat, uint64_t inH0 = 0, uint64_t inH1 = 0, std::initializer_list<int64_t> inValues = {},
                 int64_t* outValues = NULL);
    // RequestUnitPaths (a found graph path given to the unit or the request) and ClampTile (0xab3aa): Rust
    // (pm_apply.rs over EEPathManager.cpp's one-statement host cases)

    static uint64_t H(const void* inObject) { return (uint64_t)(uintptr_t)inObject; }   // a handle of the Rust calls

    EePathManager* mRust;                               // the manager (ee_sim::astar pm.rs)
    long mLC8;                                          // +0xc8 (= 5; gEEAIRegionSize)
};

extern EEPathManager gPathManager;

#endif
