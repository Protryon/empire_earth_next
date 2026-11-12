// C:\EEX\Empire Earth\AI Thread\EEAIAStarStorage.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarStorage, vtable 0x43a0f0). The A* node storage: 0x1b991..0x1c666 (EEAIAStarPathSectorStorage's Virtual10
// ends at 0x1b991, EEAIAStarTerrainStorage's ctor starts at 0x1c666).
//
// Port (docs/RUST_PLAN.md R5b.1): the storage is Rust (rust/crates/ee_astar/src/storage.rs, the members line by line
// there); the class forwards to it.
#include <windows.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"

#include "ee/EEUnit.h"

#include "ee/EEAreaUtilities.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEMathInline.h"
#include "ee/EEAIKindIDs.h"
#include "ee/EEAIAStarCapture.h"

// FUNCTION: EXE 0x41b9b7
// Creates an empty A* node storage with no grid, no lists and 53 hash buckets
EEAIAStarStorage::EEAIAStarStorage()
    : mHookHasCurrent(false), mHookCurrentX(0), mHookCurrentY(0), mHookHasParent(false), mHookParentX(0),
      mHookParentY(0), mHookHasOpen(false), mCore(ee_astar_new(EeAStarStorageKind::Hashed)), mWidth(0), mHeight(0)
{
}

// New (port): the storage of a derived kind (bounded, terrain)
EEAIAStarStorage::EEAIAStarStorage(EeAStarStorageKind inKind)
    : mHookHasCurrent(false), mHookCurrentX(0), mHookCurrentY(0), mHookHasParent(false), mHookParentX(0),
      mHookParentY(0), mHookHasOpen(false), mCore(ee_astar_new(inKind)), mWidth(0), mHeight(0)
{
}

// SYNTHETIC: EXE 0x41b9fc
// (the scalar deleting destructor)

// FUNCTION: EXE 0x41ba18
// Destroys the storage
EEAIAStarStorage::~EEAIAStarStorage()
{
    ee_astar_free(this->mCore);
    this->mCore = NULL;
}

// FUNCTION: EXE 0x41bb52
// Releases every node and forgets the found path
void EEAIAStarStorage::ResetNodes()
{
    ee_astar_reset_nodes(this->mCore);
}

// FUNCTION: EXE 0x41bc2a
// Starts walking the found path and returns its first tile; false when there is no path
bool EEAIAStarStorage::GetFirstPathTile(long& outX, long& outY)
{
    int32_t theX;
    int32_t theY;
    if (!ee_astar_first_path_tile(this->mCore, &theX, &theY))
        return false;
    outX = theX;
    outY = theY;
    return true;
}

// FUNCTION: EXE 0x41bcc3
// Returns the next tile along the found path (towards the start); false and (0, 0) at its end
bool EEAIAStarStorage::GetNextPathTile(long& outX, long& outY)
{
    int32_t theX;
    int32_t theY;
    bool theResult = ee_astar_next_path_tile(this->mCore, &theX, &theY);
    outX = theX;
    outY = theY;
    return theResult;
}

// FUNCTION: EXE 0x41be27
// Sets up the storage for an inWidth x inHeight search: releases old nodes, prepares the tile grid (the virtual
// slot: a derived storage's own arrays too) and allocates empty open and closed hash tables; always returns true
bool EEAIAStarStorage::Init(long inWidth, long inHeight)
{
    this->mWidth = inWidth;
    this->mHeight = inHeight;
    ee_astar_init_begin(this->mCore, inWidth, inHeight);
    this->PrepareGrid(inWidth, inHeight);
    ee_astar_alloc_tables(this->mCore);
    return true;
}

// FUNCTION: EXE 0x41c491
// Sizes the tile flag grid for the search and clears it, only the last search's bounds plus two when it can
void EEAIAStarStorage::PrepareGrid(long inWidth, long inHeight)
{
    this->mWidth = inWidth;
    this->mHeight = inHeight;
    ee_astar_prepare_grid(this->mCore, inWidth, inHeight, EeAStarGridKind::Hashed, false);
}

// FUNCTION: EXE 0x41c586
// Flags an on-grid tile as accepted or rejected in advance and grows the search bounds to include it
void EEAIAStarStorage::MarkTile(unsigned long inX, unsigned long inY, bool inFlag)
{
    EEAIAStarNote(kAStarEventMarkTile, inX, inY, inFlag, 0, 0, 0);
    ee_astar_mark_tile(this->mCore, (uint32_t)inX, (uint32_t)inY, inFlag);
}

// New (port): a node as the capture sees it
static void EEAIAStarViewOf(const EeAStarNodeView& inNode, EEAIAStarNodeView& outNode)
{
    outNode.mX = inNode.x;
    outNode.mY = inNode.y;
    outNode.mCost = inNode.cost;
    outNode.mPathCost = inNode.path_cost;
    outNode.mEstimate = inNode.estimate;
    outNode.mHasParent = inNode.has_parent;
    outNode.mParentX = inNode.parent_x;
    outNode.mParentY = inNode.parent_y;
}

// New (port): the current node (inWhich 0) or the found path's (1); false when there is none
bool EEAIAStarStorage::GetNodeView(long inWhich, EEAIAStarNodeView& outNode)
{
    EeAStarNodeView theNode;
    if (!ee_astar_node(this->mCore, (uint32_t)inWhich, &theNode))
        return false;
    EEAIAStarViewOf(theNode, outNode);
    return true;
}

// New (port): a list's nodes in its order (EEAIAStarList; the tables bucket by bucket, the last bucket first)
void EEAIAStarStorage::ListNodes(long inList, std::vector<EEAIAStarNodeView>& outNodes)
{
    EeAStarList theList = (EeAStarList)inList;
    uint32_t theCount = ee_astar_list(this->mCore, theList, NULL, 0);
    if (theCount == 0)
        return;
    std::vector<EeAStarNodeView> theNodes(theCount);
    ee_astar_list(this->mCore, theList, &theNodes[0], theCount);
    EEAIAStarNodeView theView;
    for (uint32_t i = 0; i < theCount; i++) {
        EEAIAStarViewOf(theNodes[i], theView);
        outNodes.push_back(theView);
    }
}

// New (port): a list's tiles in its order (the path goals' visited tiles)
void EEAIAStarStorage::AppendTiles(long inList, std::vector<U2DPoint<long> >& ioTiles)
{
    std::vector<EEAIAStarNodeView> theNodes;
    this->ListNodes(inList, theNodes);
    for (size_t i = 0; i < theNodes.size(); i++)
        ioTiles.push_back(U2DPoint<long>(theNodes[i].mX, theNodes[i].mY));
}

static EEAIAStarBounds EEAIAStarBoundsOf(const EeAStarRect& inRect)
{
    EEAIAStarBounds theBounds;
    theBounds.mBottom = inRect.bottom;
    theBounds.mLeft = inRect.left;
    theBounds.mRight = inRect.right;
    theBounds.mTop = inRect.top;
    return theBounds;
}

EEAIAStarBounds EEAIAStarStorage::GetSearchBounds()
{
    EeAStarFields theFields;
    ee_astar_get_fields(this->mCore, &theFields);
    return EEAIAStarBoundsOf(theFields.bounds);
}

bool EEAIAStarStorage::GetHasBounds()
{
    EeAStarFields theFields;
    ee_astar_get_fields(this->mCore, &theFields);
    return theFields.has_bounds;
}

void EEAIAStarStorage::SetHasBounds(bool inHasBounds)
{
    EeAStarFields theFields;
    ee_astar_get_fields(this->mCore, &theFields);
    theFields.has_bounds = inHasBounds;
    ee_astar_set_fields(this->mCore, &theFields);
}

void EEAIAStarStorage::SetDirtyBounds(const EEAIAStarBounds& inBounds)
{
    EeAStarFields theFields;
    ee_astar_get_fields(this->mCore, &theFields);
    theFields.dirty.bottom = inBounds.mBottom;
    theFields.dirty.left = inBounds.mLeft;
    theFields.dirty.right = inBounds.mRight;
    theFields.dirty.top = inBounds.mTop;
    ee_astar_set_fields(this->mCore, &theFields);
}
