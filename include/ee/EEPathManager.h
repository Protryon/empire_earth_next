// EEPathManager.h (RECONSTRUCTION; file name inferred). RTTI EEPathManager (vtable 0x43c438, one slot: the deleting
// dtor); AI Thread/EEPathManager.cpp. The game's instance is gPathManager (0x52fcb8; Empire Earth.cpp's init chain
// constructs it; provisional name). Only what the decompiled units use.
// Layout (from the ctor 0xa5d10, the dtor 0xa5e1c and the members): a pool of path nodes, a map of
// per-sector region maps, the recent-request history and four queues (paths and auto paths in progress, spare path and
// graph machines), a scratch path, the bounded machine (EECPWallManager's FindPath) and counters. Includes what the
// layout needs so its includers do not have to.
#ifndef EE_EEPATHMANAGER_H
#define EE_EEPATHMANAGER_H

#include <vector>
#include <deque>
#include <map>
#include "lle/U2DPoint.h"
#include "lle/UMemoryPool.h"
#include "ee/EETileCoordinate.h"

class UReferenceCounter;
struct EECPASGFlags;
template <class T> class U2DPoint; typedef U2DPoint<long> EETileCoordinate;
class EEAutoPathRequest;                    // ee/EEAutoPathRequest.h
class EEComplexUnit;
struct EEComplexUnitWaypoint;                // ee/EEUnit.h
class EEMap;
template <class T> class U2DRectangle;
typedef U2DRectangle<long> EEAIAStarBounds;   // ee/EEAIAStarStorage.h
class EEAIPathGoalModifierBase;
class EEAIGraphPathGoalModifierBase;
#include <stdio.h>                      // FILE
class GE3DPoint;
class EEPathManagerNode;                // EEPathManager.cpp (vtable 0x43c570, sizeof 0x38; pooled)
class EEAIAStarMachineBase;             // ee/EEAIAStarMachine.h
class EEAIAStarGraphMachineBase;
class EEAIAStarStorage;
class EEAIAStarBoundedStorage;
class EEAIAStarGraphStorage;
class EEAIAStarPathfindingGoal;
class EEAIAStarBoundedPathGoal;
class EEAIAStarPathGraphGoal;
class EEAIPathSubSector;
template <class S, class G, class M> class EEAIAStarMachine;
template <class S, class G, class M> class EEAIAStarGraphMachine;

typedef EEAIAStarMachine<EEAIAStarStorage, EEAIAStarPathfindingGoal, EEMap> EEPathMachine;          // sizeof 0x11c
typedef EEAIAStarMachine<EEAIAStarBoundedStorage, EEAIAStarBoundedPathGoal, EEMap> EEBoundedPathMachine;   // 0x148
typedef EEAIAStarGraphMachine<EEAIAStarGraphStorage, EEAIAStarPathGraphGoal, EEAIPathSubSector> EEGraphPathMachine;   // 0xd8

/// A unit path request waiting for the map (mPending +0xac; RequestPendingPaths 0xa8582 re-requests each, then clear() 0x4c41f;
// 0xc bytes) (provisional names)
struct EEPathManagerPending {
    unsigned long mUnitID;                  // +0x00
    EETileCoordinate mTile;                 // +0x04 the goal tile
};
// A finished request remembered for a while (the deque at +0x48; push_front 0xa95ae, newest first; 0x10 bytes)
// The goal tile is its U2DPoint<long> base: PruneHistory 0xa8ea1 passes an entry to the mRegions inner map's find
// 0xa992c (U2DPoint<long>::operator<) and pair ctor 0xaa7fb as the key
struct EEPathHistoryEntry : public U2DPoint<long> {
    long mUnitID;                           // +0x08
    unsigned long mTime;                    // +0x0c (gPhysics's game time)
};

class EEPathManager {
public:
    EEPathManager();                    // 0xa5d10
    virtual ~EEPathManager();           // 0xa5e1c
    void CancelAutoPathRequest(UReferenceCounter* inRequest);  // 0xa896b (ret 4; EEHerd::Clear passes its path request; provisional name)
    void DumpOutOfSynch(FILE* inFile);   // FILE*: 0xa639b (ret 4; EEPhysics's out-of-synch dump) (provisional name)
    void DumpSim(FILE* inFile);         // TEST ONLY (EE-SIM's state dumps): the searches in progress, pending requests, timers
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

    // the members (provisional names; addresses are the bodies)
    void AddHistory(EEComplexUnit* inUnit, const EETileCoordinate& inTile);    // 0xa5e7b
    void FreeMachines();                // 0xa5eba (the spare machine queues)
    void FreePaths();                   // 0xa5f39 (both queues in progress)
    void FreeRegions();                 // 0xa5fc4
    void SetGraphRevs(EEGraphPathMachine* inMachine, const EETileCoordinate& inFrom, const EETileCoordinate& inTo);  // 0xa6010
    void SetPathRevs(EEPathMachine* inMachine, EEComplexUnit* inUnit, const EETileCoordinate& inFrom,
                     const EETileCoordinate& inTo);     // 0xa6157
    void PrepareBoundedMachine(EEMap* inMap);           // 0xa61f2
    void CancelAutoPath(EEComplexUnit* inUnit);         // 0xa6251
    EEGraphPathMachine* NewGraphMachine(long inType);   // 0xa6687
    EEPathMachine* NewPathMachine(long inType, EEMap* inMap);   // 0xa6724
    EEPathManagerNode* NewNode();       // 0xa6819
    bool HasAutoPath(EEComplexUnit* inUnit);            // 0xa682f
    bool IsPathDone(EEComplexUnit* inUnit);             // 0xa687d
    bool GetPathGoal(EEComplexUnit* inUnit, U2DPoint<long>& outTile);   // 0xa68cf
    bool RequestGroupPath(EEComplexUnit* inUnit, const EETileCoordinate& inFrom, const EETileCoordinate& inTo,
                          long inPlayerID, long inType, long inRevs, EEAIGraphPathGoalModifierBase* inModifier, bool inUnlimitedRevs);  // 0xa69b0 (inArg8 pushed raw: bool)
    bool StartGraphPath(EEComplexUnit* inUnit, const EETileCoordinate& inFrom, const EETileCoordinate& inTo,
                        bool* outStarted, long inPlayerID, EEAutoPathRequest* inRequest, long inType, long inRevs,
                        EEAIGraphPathGoalModifierBase* inModifier, bool inUnlimitedRevs);  // 0xa69e2 (the graph goal's modifier)
    // 0xa78c1: a short bounded path first (true: done; ioTo gets the reached tile) (provisional name)
    bool TryShortPath(EEComplexUnit* inUnit, EEMap* inMap, long inType, const EETileCoordinate& inFrom,
                      EETileCoordinate& ioTo);
    void RequestUnitPath(EEComplexUnit* inUnit, const EETileCoordinate& inFrom, const EETileCoordinate& inTo,
                         bool inUnlimitedRevs, long inRevs, bool inUseGraph, EEAIGraphPathGoalModifierBase* inModifier, bool inArg8);  // 0xa6e88
    EEPathManagerNode* FindPath(EEComplexUnit* inUnit);  // 0xa7170
    // 0xa71c9: a path through the bounded machine (inMargin tiles around the start); true when it reaches inTo
    bool FindBoundedPath(EEComplexUnit* inUnit, EEMap* inMap, long inType, const EETileCoordinate& inFrom,
                         const EETileCoordinate& inTo, std::vector<GE3DPoint>& outPath, long inMargin,
                         bool inArg8, bool* outReached);   // FindPath pushes 1; 0xa71c9 never reads [ebp+0x24] (provisional name)
    // 0xa75f4: ioPath loses its points from the last one on inUnit's tile on (true when cut) (provisional name)
    bool TrimPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath);
    // 0xa7414 (cdecl; EEUnitClass 0x636b69, inArg3 0): inWaypoints after the last one on ioPath's tiles appended, last
    // first (provisional name)
    static void MergeWaypoints(std::vector<GE3DPoint>& ioPath, std::deque<EEComplexUnitWaypoint>& inWaypoints, long inArg3);
    // 0xa777f: unless TrimPath cut it, inUnit's remaining waypoints appended to ioPath (provisional name)
    void AppendWaypoints(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath);
    // 0xa8bf4: inPath up to its point inCount from the end, then inDetour, when inDetour starts there (provisional
    // name)
    bool JoinPaths(std::vector<GE3DPoint>& inPath, std::vector<GE3DPoint>& inDetour, std::vector<GE3DPoint>* outPath,
                   long inCount);
    bool RequestUnitPaths(EEComplexUnit* inUnit, EEAutoPathRequest* inRequest, EEGraphPathMachine* inMachine);   // 0xa7b03
    void SetRegionUpdates(bool inOn, bool inNow);       // 0xa8b40
    bool RunBoundedPath(EEBoundedPathMachine* inMachine, EEComplexUnit* inUnit, const EETileCoordinate& inFrom,
                        const EETileCoordinate& inTo, long inRevs);   // 0xa8ba4
    void FinishPath(EEPathManagerNode* inNode);         // 0xa86cb
    bool FinishGraphPath(EEPathManagerNode* inNode);    // 0xa7e6c
    void FinishFailedPath(EEPathManagerNode* inNode);   // 0xa7f4a
    void FinishFoundPath(EEPathManagerNode* inNode);    // 0xa870d
    void PruneHistory();                // 0xa8ea1 (every 10 s)
    void PruneRegions();                // 0xa9266 (every 30 s)
    void ReleasePathMachine(EEPathMachine* inMachine);  // 0xa8861
    void ReleaseGraphMachine(EEGraphPathMachine* inMachine);  // 0xa8808
    void ReleaseNode(EEPathManagerNode* inNode);        // 0xa88c3
    void CancelAutoPaths(EEComplexUnit* inUnit);        // 0xa88df
    void CancelPaths(EEComplexUnit* inUnit, bool inAutoPaths);   // 0xa89f7
    void UpdateRegions();               // 0xa8a8e
    void Clear();                       // 0xa8adb
    void UpdateAutoPaths();             // 0xa8de8
    void UpdatePaths();                 // 0xa9367
    // 0xab3aa: a tile off the map is moved onto it (an inline member: its COMDAT is with the templates)
    void ClampTile(EEMap* inMap, EETileCoordinate& ioTile);

    UMemoryPool<EEPathManagerNode> mNodePool;           // +0x04
    // +0x3c per unit id: its recent goal tiles and their request counts (PruneHistory 0xa8ea1: find 0xa992c calls
    // U2DPoint<long>::operator<; insert is the ICF-folded map<U2DPoint<long>, long> one 0x75d7d; its own dtor 0xa9901:
    // an unsigned mapped count, PruneRegions `jb`)
    std::map<long, std::map<U2DPoint<long>, unsigned long>*> mRegions;
    std::deque<EEPathHistoryEntry> mHistory;            // +0x48
    std::deque<EEPathManagerNode*> mPaths;              // +0x5c (path machines in progress)
    std::deque<EEPathManagerNode*> mAutoPaths;          // +0x70 (graph machines in progress)
    std::deque<EEPathMachine*> mSparePathMachines;      // +0x84 (at most 0x23)
    std::deque<EEGraphPathMachine*> mSpareGraphMachines;    // +0x98 (at most 0x19)
    std::vector<EEPathManagerPending> mPending;         // +0xac (12-byte elements: RequestPendingPaths; was mScratchPath)
    EEBoundedPathMachine* mBoundedMachine;              // +0xbc
    long mNextHistoryPruneTime;                         // +0xc0
    long mNextRegionPruneTime;                          // +0xc4
    long mLC8;                                          // +0xc8 (= 5)
    long mLCC;                                          // +0xcc
    long mNextRequestID;                                // +0xd0
    unsigned long mNextRegionTime;                      // +0xd4
    bool mIsUpdatingRegions;                            // +0xd8
};

extern EEPathManager gPathManager;      // 0x52fcb8

#endif
