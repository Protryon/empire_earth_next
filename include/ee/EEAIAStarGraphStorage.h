// C:\EEX\Empire Earth\AI Thread\EEAIAStarGraphStorage.h (RECONSTRUCTION; file name inferred). The node storage of the
// AI's graph A* searches (RTTI EEAIAStarGraphStorage, no base; vtable 0x439ee8, 5 slots; EEAIAStarGraphStorage.cpp
// 0x16070..0x16c0e): the twin of EEAIAStarStorage over graph nodes (an index per node instead of a tile): a closed list
// (+0x0c), two hash tables of mBucketCount buckets (closed +0x04, open +0x08), a sorted open
// list (+0x14/+0x18), a fast open list (+0x10), the found path (+0x1c, walked through mParent), and per node index a
// flag byte (+0x28: 8 closed, 4 open) and the node (+0x2c). Nodes come from a static free list with counters. Derived:
// EEAIAStarGraphTerrainStorage. Slots named by vtable offset; fields by offset (provisional names).
// Requires ee/EETileCoordinate.h.
#ifndef EE_EEAIASTARGRAPHSTORAGE_H
#define EE_EEAIASTARGRAPHSTORAGE_H
#include "ee/EEAIKindIDs.h"

class AStarGraphableObjectBase;         // ee/AStarGraphableObjectBase.h

// A graph search node (0x28 bytes; NewNode 0x162b0 pops one off the free list or allocates one). Names are provisional.
struct EEAIAStarGraphNode {
    EEAIAStarGraphNode(const EETileCoordinate& inTile, long inIndex)
        : mParent(NULL), mNext(NULL), mPrev(NULL), mIndex(inIndex), mCost(0), mPathCost(0), mEstimate(0), mTile(inTile)
    {
    }

    AStarGraphableObjectBase* mObject;      // +0x00 (the graph object: EEAIContinent's base at +0x68)
    EEAIAStarGraphNode* mParent;            // +0x04 (the path is walked through it)
    EEAIAStarGraphNode* mNext;              // +0x08 (a hash bucket, a list; the free list)
    EEAIAStarGraphNode* mPrev;              // +0x0c
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

    static void FreeNodePool();             // 0x16070 (deletes the free list)
    static void ReleaseNode(EEAIAStarGraphNode* inNode);   // 0x1616b
    void AddClosed(EEAIAStarGraphNode* inNode);             // 0x1613f
    void AddOpen(EEAIAStarGraphNode* inNode);               // 0x1618a
    long GetBucket(long inX, long inY);     // 0x16233 (out of line, unlike EEAIAStarStorage's)
    bool GetFirstPathNode(EETileCoordinate& outTile, long& outIndex);   // 0x16258
    // 0x162b0 (thiscall: `ret 0x10`; this unused)
    EEAIAStarGraphNode* NewNode(EETileCoordinate inTile, AStarGraphableObjectBase* inObject, long inIndex);
    bool GetNextPathNode(EETileCoordinate& outTile, long& outIndex);    // 0x1634f
    void Clear();                           // 0x16394 (every node back to the free list)
    void UpdateOpen(EEAIAStarGraphNode* inNode);            // 0x1656d
    EEAIAStarGraphNode* RemoveNearestClosed(const EETileCoordinate& inTile);   // 0x1665f
    EEAIAStarGraphNode* RemoveNearestOpen(const EETileCoordinate& inTile);     // 0x16771
    EEAIAStarGraphNode* RemoveBestOpen();   // 0x16905
    void RemoveClosed(EEAIAStarGraphNode* inNode);          // 0x16aa5
    void InsertSorted(EEAIAStarGraphNode* inNode);          // 0x16b5b
    void Push(EEAIAStarGraphNode* inNode);  // 0x16bb5 (in front of the fast open list)
    EEAIAStarGraphNode* Pop();              // 0x16be1
    // inline (COMDATs 0x28b6d / 0x28b87 after their first user, the chain machine's AddNode 0x28aa7; also the path
    // machine's 0xab6c1): the index's node when it is open (flag 4) / closed (flag 8), else 0 (provisional names)
    EEAIAStarGraphNode* GetOpenNode(long inIndex) { return this->mFlags[inIndex] & kAStarTileOpen ? this->mNodes[inIndex] : 0; }
    EEAIAStarGraphNode* GetClosedNode(long inIndex) { return this->mFlags[inIndex] & kAStarTileClosed ? this->mNodes[inIndex] : 0; }

    EEAIAStarGraphNode** mClosedTable;      // +0x04 (mBucketCount buckets; delete[] in the dtor)
    EEAIAStarGraphNode** mOpenTable;        // +0x08 (mBucketCount buckets; delete[] in the dtor)
    EEAIAStarGraphNode* mClosedList;        // +0x0c
    EEAIAStarGraphNode* mOpenList;          // +0x10 (Push/Pop)
    EEAIAStarGraphNode* mSortedOpen;        // +0x14 (ascending mCost)
    EEAIAStarGraphNode* mSortedOpenTail;    // +0x18
    EEAIAStarGraphNode* mPath;              // +0x1c
    EEAIAStarGraphNode* mPathCursor;        // +0x20
    EEAIAStarGraphNode* mCurrentNode;       // +0x24
    unsigned char* mFlags;                  // +0x28 (one byte per index: 8 closed, 4 open)
    EEAIAStarGraphNode** mNodes;            // +0x2c (one per index)
    long GetCount() const { return this->mCount; }
    long mCount;                            // +0x30 the index count
    long mBucketCount;                      // +0x34 (= 0x1b)
    long mOldCount;                         // +0x38 (the count the arrays were sized for)
};

#endif
