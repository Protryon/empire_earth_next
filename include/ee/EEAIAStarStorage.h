// C:\EEX\Empire Earth\AI Thread\EEAIAStarStorage.h (RECONSTRUCTION; file name inferred). The
// node storage of the AI's A* searches (RTTI EEAIAStarStorage, no base; vtable 0x43a0f0, 5 slots; EEAIAStarStorage.cpp
// 0x1b991..0x1c666): two hash tables of nodes (closed +0x14, flag 8; open +0x18, flag 4) with mL44 buckets, a sorted
// open list (+0x24/+0x28), the found path (+0x2c, walked through mParent) and a per-tile flag byte (+0x38) over
// mL3C x mL40 tiles; nodes come from a static free list. Derived: EEAIAStarFastStorage > EEAIAStarTerrainStorage > the
// per-goal storages. Slots named by vtable offset; fields by offset (provisional names).
#ifndef EE_EEAIASTARSTORAGE_H
#define EE_EEAIASTARSTORAGE_H

#include "ee/EETileCoordinate.h"     // EETileCoordinate (the inline Set needs the whole type; was a forward declaration)
#include "lle/U2DRectangle.h"     // EEAIAStarBounds (exe view: no statics)

// A search node (0x20 bytes; EEAIAStarStorage keeps a free list of them, 0x4e5c40: NewNode 0x1bc68 pops or allocates
// one, ReleaseNode 0x1ba9b pushes it back). Names are provisional.
struct EEAIAStarNode {
    EEAIAStarNode* mParent;                 // +0x00 (the path is walked through it)
    EEAIAStarNode* mNext;                   // +0x04 (a hash bucket, a list; the free list)
    EEAIAStarNode* mPrev;                   // +0x08
    unsigned long mCost;                    // +0x0c (the sorted open list's key)
    long mPathCost;                         // +0x10
    long mEstimate;                         // +0x14
    long mX;                                // +0x18 (the tile: index mX * mL3C + mY; hash (mX << 4) + mY)
    long mY;                                // +0x1c
};

// The bounds of the tiles a search touched: a U2DRectangle<long> (+0 mBottom the last y, +4 mLeft the first x, +8
// mRight the last x, +0xc mTop the first y; the same ctor/Clear store order, the same Contains test, Extend 0x123d1 is
// U2DRectangle<long>::Extend; RTSRegionTable::CollectValues takes both spellings in its callers).
// The storages' Virtual10 clears only these tiles, grown by a margin, when they are valid.
typedef U2DRectangle<long> EEAIAStarBounds;

class EEAIAStarStorage {
public:
    EEAIAStarStorage();                     // 0x1b9b7
    virtual ~EEAIAStarStorage();            // +0x00 (dtor 0x1ba18)
    virtual void ResetNodes();              // +0x04 0x1bb52 (Clear, then no path)
    virtual void Virtual08(long inArg) {}   // +0x08 (empty: ICF-folded `ret 4` 0x157269)
    virtual bool Init(long inWidth, long inHeight);        // +0x0c 0x1be27 (sizes the storage, new hash tables)
    virtual void PrepareGrid(long inWidth, long inHeight);  // +0x10 0x1c491 (each storage overrides it)

    static void FreeNodePool();             // 0x1b991 (deletes the free list)
    static void ReleaseNode(EEAIAStarNode* inNode);        // 0x1ba9b
    EEAIAStarNode* NewNode(long inX, long inY);            // 0x1bc68 (thiscall: `ret 8`; this unused)
    // the inline hash of a tile (PJW-like; 0x1ba43 and every table user expand it)
    long GetBucket(long inX, long inY) const
    {
        long theHash = (inX << 4) + inY;
        long theHigh = theHash & 0xf0000000;
        if (theHigh)
            theHash = (theHigh >> 24) ^ theHigh;
        return theHash % this->mBucketCount;
    }
    void AddClosed(EEAIAStarNode* inNode);  // 0x1ba43
    void AddOpen(EEAIAStarNode* inNode);    // 0x1baae
    EEAIAStarNode* FindClosed(long inX, long inY);   // 0x1bb64
    EEAIAStarNode* FindOpen(long inX, long inY);     // 0x1bbb9
    bool GetFirstPathTile(long& outX, long& outY);   // 0x1bc2a
    bool GetNextPathTile(long& outX, long& outY);    // 0x1bcc3
    void Clear();                           // 0x1bcfe (every node back to the free list)
    void UpdateOpen(EEAIAStarNode* inNode); // 0x1be86 (its cost changed: re-sort it)
    EEAIAStarNode* RemoveNearestClosed(const EETileCoordinate& inTile);   // 0x1bf8b
    EEAIAStarNode* RemoveNearestOpen(const EETileCoordinate& inTile);     // 0x1c0b6
    EEAIAStarNode* RemoveBestOpen();        // 0x1c268
    void RemoveClosed(EEAIAStarNode* inNode);   // 0x1c420
    void MarkTile(unsigned long inX, unsigned long inY, bool inFlag);   // 0x1c586 (flag 1 or 2, extends the bounds)
    void InsertSorted(EEAIAStarNode* inNode);   // 0x1c5bd
    void ExtendBounds(long inX, long inY);  // 0x1c617
    unsigned char* GetTileFlags(unsigned long inX, unsigned long inY) { return this->mTileFlags + this->mWidth * inX + inY; }

    EEAIAStarBounds mBounds;                // +0x04
    EEAIAStarNode** mClosedTable;           // +0x14 (mL44 buckets; delete[] in the dtor)
    EEAIAStarNode** mOpenTable;             // +0x18 (mL44 buckets; delete[] in the dtor)
    EEAIAStarNode* mFastClosedList;         // +0x1c (EEAIAStarFastStorage::Add pushes in front)
    EEAIAStarNode* mFastOpenList;           // +0x20
    EEAIAStarNode* mSortedOpen;             // +0x24 (ascending mCost)
    EEAIAStarNode* mSortedOpenTail;         // +0x28
    EEAIAStarNode* mPath;                   // +0x2c
    EEAIAStarNode* mPathCursor;             // +0x30
    EEAIAStarNode* mCurrentNode;            // +0x34
    unsigned char* mTileFlags;              // +0x38 (delete[] in the dtor; one byte per tile: 8 closed, 4 open)
    long GetWidth() const { return this->mWidth; }
    long mWidth;                            // +0x3c the width (the terrain storages: +0x58 = this - 1)
    long GetHeight() const { return this->mHeight; }
    long mHeight;                           // +0x40 the height (the terrain storages: +0x50 = this - 1)
    long mBucketCount;                      // +0x44 the bucket count (= 0x35)
    bool mHasBounds;                        // +0x48 (set once the bounds hold a tile)
};

#endif
