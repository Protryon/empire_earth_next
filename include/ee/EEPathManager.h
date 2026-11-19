// EEPathManager.h (RECONSTRUCTION; file name inferred). RTTI EEPathManager (vtable 0x43c438, one slot: the deleting
// dtor); AI Thread/EEPathManager.cpp. The game's instance is gPathManager (0x52fcb8; Empire Earth.cpp's init chain
// constructs it; provisional name).
// The manager is Rust (ee_sim::astar pm.rs, the handle mRust): the searches in progress (the original's node pool,
// mPaths, mAutoPaths), the spare machines, the units waiting for the map (mPending), the counters and the request
// record, the requests' and the updates' decisions, the saved game's parts. This class is its C++ face: the entry
// points forward to it, and it keeps the bounded machine (EECPWallManager's FindPath) and its searches.
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
class EEAIPathGoalModifierBase;
class EEAIGraphPathGoalModifierBase;
class GE3DPoint;
class EEAIAStarGraphStorage;
class EEAIAStarPathGraphGoal;
class EEAIPathSubSector;
template <class S, class G, class M> class EEAIAStarGraphMachine;
struct EePathManager;                   // rust/sim/astar.h

typedef EEAIAStarGraphMachine<EEAIAStarGraphStorage, EEAIAStarPathGraphGoal, EEAIPathSubSector> EEGraphPathMachine;   // 0xd8

class EEPathManager {
public:
    EEPathManager();
    virtual ~EEPathManager();
    void CancelAutoPathRequest(UReferenceCounter* inRequest);  // 0xa896b (ret 4; EEHerd::Clear passes its path request; provisional name)
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
    // (EEAwaitAutoPathRequest, ee/EEAutoPathRequest.h)
    void SaveGraphSearches(FSFile& outFile);
    long LoadGraphSearches(FSFile& inFile);
    void RequestPendingPath(EEComplexUnit* inUnit, const EETileCoordinate& inTile);   // RequestPendingPaths' one unit
    void AddPending(unsigned long inUnitID, const EETileCoordinate& inTile);   // a loaded unit asks its path at StartGame
    void Update();                      // 0xa8db8 (EEPhysics::Tick, every tick) (provisional name)
    void Reset();                       // 0xa8778 (before a scenario starts; provisional name)
    void RequestPendingPaths(EEMap* inMap);  // 0xa8582 (EEWorld::StartGame; provisional name)
    void CompleteAutoPath(EEAutoPathRequest* inRequest);  // 0xa62ce (EEComplexUnit::RefreshPath; provisional name)
    void CompletePath(EEComplexUnit* inUnit);     // 0xa634b (EEComplexUnit::RefreshPath; provisional name)
    // 0xa6948 (EEGroup::Fn206581: the owner's player id, the target tile, the units' flags, 1, -1, 0, 0; returns a new
    // ee/EEAutoPathRequest.h) (provisional name)
    EEAutoPathRequest* RequestPath(unsigned long inPlayerID, const EETileCoordinate& inFrom, const EETileCoordinate& inTo,
                                   EECPASGFlags* inFlags, long inType, long inRevs, EEAIGraphPathGoalModifierBase* inModifier, bool inUnlimitedRevs);
    // 0xa6bfd (EECPWallManager.cpp: inUnit's path from inFrom to inTo inside inBounds, with a path modifier)
    // (provisional name)
    void FindPath(EEComplexUnit* inUnit, EEMap* inMap, const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo,
                  const EEAIAStarBounds& inBounds, std::vector<U2DPoint<long> >& outPath, long inType, long inRevs,
                  EEAIPathGoalModifierBase* inModifier, std::vector<U2DPoint<long> >* outVisited);   // 0x12e2c9 passes &local (the storm's tile list)
    bool HasAutoPath(EEComplexUnit* inUnit);
    bool IsPathDone(EEComplexUnit* inUnit);
    bool GetPathGoal(EEComplexUnit* inUnit, U2DPoint<long>& outTile);
    void RequestUnitPath(EEComplexUnit* inUnit, const EETileCoordinate& inFrom, const EETileCoordinate& inTo,
                         bool inUnlimitedRevs, long inRevs, bool inUseGraph, EEAIGraphPathGoalModifierBase* inModifier, bool inArg8);
    void CancelPaths(EEComplexUnit* inUnit, bool inAutoPaths);
    void SetRegionUpdates(bool inOn, bool inNow);
    void Clear();
    void FreePaths();                   // 0xa5f39 (both queues in progress)
    // 0xa71c9: a path through the bounded machine (inMargin tiles around the start); true when it reaches inTo
    bool FindBoundedPath(EEComplexUnit* inUnit, EEMap* inMap, long inType, const EETileCoordinate& inFrom,
                         const EETileCoordinate& inTo, std::vector<GE3DPoint>& outPath, long inMargin,
                         bool inArg8, bool* outReached);   // FindPath pushes 1; 0xa71c9 never reads [ebp+0x24] (provisional name)
    // 0xa7414 (cdecl; EEUnitClass 0x636b69, inArg3 0): inWaypoints after the last one on ioPath's tiles appended, last
    // first (provisional name)
    static void MergeWaypoints(std::vector<GE3DPoint>& ioPath, std::deque<EEComplexUnitWaypoint>& inWaypoints, long inArg3);

    // the Rust manager's host (EEPathManager.cpp): what stays C++ (provisional names; addresses are the bodies).
    // TryShortPath, TrimPath, AppendWaypoints, JoinPaths, RunBoundedPath and FinishPath with its found and failed
    // halves are Rust (pm_bounded.rs) over the bounded machine, a Rust search with the path machines (path_search.rs)
    int64_t Call(int32_t inWhat, uint64_t inH0 = 0, uint64_t inH1 = 0, std::initializer_list<int64_t> inValues = {},
                 int64_t* outValues = NULL);
    bool RequestUnitPaths(EEComplexUnit* inUnit, EEAutoPathRequest* inRequest, EEGraphPathMachine* inMachine);
    // 0xab3aa: a tile off the map is moved onto it (an inline member: its COMDAT is with the templates)
    void ClampTile(EEMap* inMap, EETileCoordinate& ioTile);

    EePathManager* mRust;                               // the manager (ee_sim::astar pm.rs)
    long mLC8;                                          // +0xc8 (= 5; gEEAIRegionSize)
};

extern EEPathManager gPathManager;

#endif
