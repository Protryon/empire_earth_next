// C:\EEX\Empire Earth\AI Thread\EEAIAStarStorage.h (RECONSTRUCTION; file name inferred). The
// node storage of the AI's A* searches (RTTI EEAIAStarStorage, no base; vtable 0x43a0f0, 5 slots; EEAIAStarStorage.cpp
// 0x1b991..0x1c666): two hash tables of nodes (closed +0x14, flag 8; open +0x18, flag 4) with mL44 buckets, a sorted
// open list (+0x24/+0x28), the found path (+0x2c, walked through mParent) and a per-tile flag byte (+0x38) over
// mL3C x mL40 tiles; nodes come from a static free list. Derived: EEAIAStarFastStorage > EEAIAStarTerrainStorage > the
// per-goal storages. Slots named by vtable offset; fields by offset (provisional names).
//
// Port (docs/RUST_PLAN.md R5b.1): the nodes, the lists, the tile flags, the node map, the bounds and the machine's
// search steps are Rust (rust/crates/ee_astar, one EeAStarStorage per storage). The C++ classes keep their vtables,
// their own per-tile arrays (the flood storages' ids) and forward the rest; what the goals read of the storage while a
// search runs comes from the machine's hooks (mHook*), since a hook must not call back into the running storage.
#ifndef EE_EEAIASTARSTORAGE_H
#define EE_EEAIASTARSTORAGE_H

#include "ee/EETileCoordinate.h"     // EETileCoordinate (the inline Set needs the whole type; was a forward declaration)
#include "lle/U2DRectangle.h"     // EEAIAStarBounds (exe view: no statics)
#include "rust/ee_astar.h"         // EeAStarStorage, the storage kinds and slots

// A search node as the goals see one (the machine's hooks hand IsGoalNode / CheckNode a node of the tile; the links
// and costs are Rust's and zero here). Names are provisional.
struct EEAIAStarNode {
    EEAIAStarNode* mParent;                 // +0x00
    EEAIAStarNode* mNext;                   // +0x04
    EEAIAStarNode* mPrev;                   // +0x08
    unsigned long mCost;                    // +0x0c
    long mPathCost;                         // +0x10
    long mEstimate;                         // +0x14
    long mX;                                // +0x18
    long mY;                                // +0x1c
};

// The bounds of the tiles a search touched: a U2DRectangle<long> (+0 mBottom the last y, +4 mLeft the first x, +8
// mRight the last x, +0xc mTop the first y; the same ctor/Clear store order, the same Contains test, Extend 0x123d1 is
// U2DRectangle<long>::Extend; RTSRegionTable::CollectValues takes both spellings in its callers).
// The storages' Virtual10 clears only these tiles, grown by a margin, when they are valid.
typedef U2DRectangle<long> EEAIAStarBounds;

struct EEAIAStarNodeView;               // ee/EEAIAStarCapture.h

class EEAIAStarStorage {
public:
    EEAIAStarStorage();                     // 0x1b9b7 (a hashed storage)
    virtual ~EEAIAStarStorage();            // +0x00 (dtor 0x1ba18)
    virtual void ResetNodes();              // +0x04 0x1bb52 (Clear, then no path)
    virtual void Virtual08(long inArg) {}   // +0x08 (empty: ICF-folded `ret 4` 0x157269)
    virtual bool Init(long inWidth, long inHeight);        // +0x0c 0x1be27 (sizes the storage, new hash tables)
    virtual void PrepareGrid(long inWidth, long inHeight);  // +0x10 0x1c491 (each storage overrides it)

    static void FreeNodePool() {}           // 0x1b991 (the Rust storages free their nodes with themselves)
    bool GetFirstPathTile(long& outX, long& outY);   // 0x1bc2a
    bool GetNextPathTile(long& outX, long& outY);    // 0x1bcc3
    void MarkTile(unsigned long inX, unsigned long inY, bool inFlag);   // 0x1c586 (flag 1 or 2, extends the bounds)

    // New (port): the storage as the capture and the goals read it outside a search: the current node (0) and the
    // found path's (1), a list's nodes in its order (EEAIAStarList), the bounds
    bool GetNodeView(long inWhich, EEAIAStarNodeView& outNode);
    void ListNodes(long inList, std::vector<EEAIAStarNodeView>& outNodes);
    void AppendTiles(long inList, std::vector<U2DPoint<long> >& ioTiles);   // a list's tiles in its order
    EEAIAStarBounds GetSearchBounds();
    bool GetHasBounds();
    void SetHasBounds(bool inHasBounds);
    EEAIAStarBounds GetDirtyBounds();
    void SetDirtyBounds(const EEAIAStarBounds& inBounds);  // EEAIAStarTerrainStorage::mDirtyBounds (the flood goals)

    long GetWidth() const { return this->mWidth; }
    long GetHeight() const { return this->mHeight; }

    // New (port): what the goals read during a search, set by the machine's hooks before it asks the goal: the
    // current node, its parent (GetCost), whether an open node is left (SearchDone)
    bool mHookHasCurrent;
    long mHookCurrentX;
    long mHookCurrentY;
    bool mHookHasParent;
    long mHookParentX;
    long mHookParentY;
    bool mHookHasOpen;

    EeAStarStorage* mCore;                  // the Rust storage
    long mWidth;                            // +0x3c the width (Init's; the per-tile arrays' stride)
    long mHeight;                           // +0x40 the height

protected:
    explicit EEAIAStarStorage(EeAStarStorageKind inKind);
};

#endif
