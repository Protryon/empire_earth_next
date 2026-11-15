// C:\EEX\Empire Earth\AI Thread\EEAIAStarGraphStorage.h (RECONSTRUCTION; file name inferred). The node storage of the
// AI's graph A* searches (RTTI EEAIAStarGraphStorage, no base; vtable 0x439ee8, 5 slots; EEAIAStarGraphStorage.cpp
// 0x16070..0x16c0e): the twin of EEAIAStarStorage over graph nodes (an index per node instead of a tile): a closed list
// (+0x0c), two hash tables of mBucketCount buckets (closed +0x04, open +0x08), a sorted open
// list (+0x14/+0x18), a fast open list (+0x10), the found path (+0x1c, walked through mParent), and per node index a
// flag byte (+0x28: 8 closed, 4 open) and the node (+0x2c). Nodes came from a static free list with counters. Derived:
// EEAIAStarGraphTerrainStorage. Slots named by vtable offset (provisional names).
//
// Port (docs/RUST_PLAN.md R5b.1): the nodes, the lists, the tables, the per-index arrays and the machine's search
// steps are Rust (rust/crates/ee_astar/src/graph.rs, one EeAStarGraphStorage per storage). The class keeps its vtable
// and forwards; the goals get the nodes the machine hands them (IsGoalNode, GetStepCost) and the current node
// (SearchDone) as EEAIAStarGraphNode views built from the Rust node, and the lists' objects through ListObjects.
// Requires ee/EETileCoordinate.h.
#ifndef EE_EEAIASTARGRAPHSTORAGE_H
#define EE_EEAIASTARGRAPHSTORAGE_H
#include <vector>
#include "ee/EEAIKindIDs.h"
#include "rust/ee_astar.h"         // EeAStarGraphStorage, EeAStarGraphKind, EeAStarGraphList, EeAStarGraphNodeView

class AStarGraphableObjectBase;         // ee/AStarGraphableObjectBase.h
class EEAIAStarCaptureRun;              // ee/EEAIAStarCapture.h

// A graph search node as the goals see one (0x28 bytes in the original; the links are Rust's now and stay NULL here:
// the goals and the route planners read mObject, mIndex and mTile). Names are provisional.
struct EEAIAStarGraphNode {
    EEAIAStarGraphNode()
        : mObject(NULL), mParent(NULL), mNext(NULL), mPrev(NULL), mIndex(0), mCost(0), mPathCost(0), mEstimate(0), mTile(0, 0)
    {
    }
    EEAIAStarGraphNode(const EETileCoordinate& inTile, long inIndex)
        : mObject(NULL), mParent(NULL), mNext(NULL), mPrev(NULL), mIndex(inIndex), mCost(0), mPathCost(0), mEstimate(0), mTile(inTile)
    {
    }
    // New (port): the view of a Rust node (EEAIAStarGraphStorage.cpp)
    explicit EEAIAStarGraphNode(const EeAStarGraphNodeView& inView);

    AStarGraphableObjectBase* mObject;      // +0x00 (the graph object: EEAIContinent's base at +0x68)
    EEAIAStarGraphNode* mParent;            // +0x04 (the path is walked through it: Rust's now, NULL here)
    EEAIAStarGraphNode* mNext;              // +0x08 (a hash bucket, a list; the free list: Rust's now, NULL here)
    EEAIAStarGraphNode* mPrev;              // +0x0c (NULL here)
    long mIndex;                            // +0x10 (the object's index: mFlags, mNodes)
    unsigned long mCost;                    // +0x14 (the sorted open list's key)
    long mPathCost;                         // +0x18
    long mEstimate;                         // +0x1c
    EETileCoordinate mTile;                 // +0x20 (copied as one: GetFirstPathNode; the hash (mX << 4) + mY)
};

class EEAIAStarGraphStorage {
public:
    EEAIAStarGraphStorage();                // 0x160b7
    virtual ~EEAIAStarGraphStorage();       // +0x00 (dtor 0x1610c)
    virtual void ResetNodes();              // +0x04 0x16221 (Clear, then no path)
    virtual void Virtual08(long inArg) {}   // +0x08 (empty: ICF-folded `ret 4` 0x157269)
    virtual bool Init(long inCount);        // +0x0c 0x16514 (sizes the storage, new hash tables)
    virtual void PrepareArrays(long inCount);  // +0x10 0x16af6 (the per-index arrays)

    static void FreeNodePool();             // 0x16070 (nothing to free: the Rust storages free their nodes with themselves)
    bool GetFirstPathNode(EETileCoordinate& outTile, long& outIndex);   // 0x16258
    bool GetNextPathNode(EETileCoordinate& outTile, long& outIndex);    // 0x1634f
    // New (port): the objects of a list's nodes in its order (the chain goal's NumberChain)
    void ListObjects(EeAStarGraphList inList, std::vector<AStarGraphableObjectBase*>& outObjects);
    // New (port), TEST ONLY: the current and path nodes and every list's nodes in order into the A* capture
    // (ee/EEAIAStarCapture.h)
    void CaptureNodes(EEAIAStarCaptureRun& ioRun);

    long GetCount() const { return this->mCount; }

    // New (port): what the goal reads of the storage during a search, set by the machine's SearchDone hook before it
    // asks the goal: the current node (a view) and whether an open node is left
    bool mHookHasCurrent;
    EEAIAStarGraphNode mHookCurrent;
    bool mHookHasOpen;

    EeAStarGraphStorage* mCore;             // the Rust storage
    long mCount;                            // +0x30 the index count

protected:
    explicit EEAIAStarGraphStorage(EeAStarGraphKind inKind);
};

#endif
