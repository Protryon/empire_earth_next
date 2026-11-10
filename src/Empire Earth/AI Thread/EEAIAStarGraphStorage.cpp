// C:\EEX\Empire Earth\AI Thread\EEAIAStarGraphStorage.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarGraphStorage, vtable 0x439ee8). The graph A* node storage: 0x16070..0x16c0e (the graph goal base's ??_G
// ends at 0x16070; EEAIAStarHillGoal's ctor starts at 0x16c0e; the name sorts between EEAIAStarGraphContinentalChainGoal
// and EEAIAStarHillGoal). The twin of EEAIAStarStorage.cpp.
//
// Port (docs/RUST_PLAN.md R5b.1): the nodes, the lists, the tables, the per-index flag and node arrays and the
// machine's search steps are Rust (rust/crates/ee_astar/src/graph.rs, the members line by line there: the node free
// list 0x16070/0x1616b/0x162b0, AddClosed 0x1613f, AddOpen 0x1618a, GetBucket 0x16233, Clear 0x16394, UpdateOpen
// 0x1656d, RemoveNearestClosed 0x1665f, RemoveNearestOpen 0x16771, RemoveBestOpen 0x16905, RemoveClosed 0x16aa5,
// InsertSorted 0x16b5b, Push 0x16bb5, Pop 0x16be1); the class forwards to it.
#include <windows.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"

#include "ee/EETileCoordinate.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarGraphStorage.h"
#include "ee/EEAIAStarCapture.h"

// New (port): the goals' view of a Rust node (the links stay NULL: the lists are Rust's)
EEAIAStarGraphNode::EEAIAStarGraphNode(const EeAStarGraphNodeView& inView)
    : mObject(reinterpret_cast<AStarGraphableObjectBase*>(inView.object)), mParent(NULL), mNext(NULL), mPrev(NULL),
      mIndex(inView.index), mCost(inView.cost), mPathCost(inView.path_cost), mEstimate(inView.estimate),
      mTile(inView.x, inView.y)
{
}

// FUNCTION: EXE 0x416070
// The shared free node pool is gone: each Rust storage frees its nodes with itself (EEPathManager::Shutdown calls it)
void EEAIAStarGraphStorage::FreeNodePool()
{
}

// FUNCTION: EXE 0x4160b7
// Creates an empty graph node storage with no lists or tables and 27 hash buckets
EEAIAStarGraphStorage::EEAIAStarGraphStorage()
    : mHookHasCurrent(false), mHookHasOpen(false), mCore(ee_astar_graph_new(EeAStarGraphKind::Hashed)), mCount(0)
{
}

// New (port): the storage of a derived kind (the terrain storage's fast open list)
EEAIAStarGraphStorage::EEAIAStarGraphStorage(EeAStarGraphKind inKind)
    : mHookHasCurrent(false), mHookHasOpen(false), mCore(ee_astar_graph_new(inKind)), mCount(0)
{
}

// SYNTHETIC: EXE 0x4160f0
// (the scalar deleting destructor)

// FUNCTION: EXE 0x41610c
// Destroys the storage and its nodes
EEAIAStarGraphStorage::~EEAIAStarGraphStorage()
{
    ee_astar_graph_free(this->mCore);
    this->mCore = NULL;
}

// FUNCTION: EXE 0x416221
// Returns all nodes to the free pool and forgets the found path
void EEAIAStarGraphStorage::ResetNodes()
{
    ee_astar_graph_reset_nodes(this->mCore);
}

// FUNCTION: EXE 0x416258
// Starts walking the found path and returns its first node's tile and index; false (zeros) without a path
bool EEAIAStarGraphStorage::GetFirstPathNode(EETileCoordinate& outTile, long& outIndex)
{
    int32_t theX;
    int32_t theY;
    int32_t theIndex;
    bool theResult = ee_astar_graph_first_path_node(this->mCore, &theX, &theY, &theIndex);
    outTile.mX = theX;
    outTile.mY = theY;
    outIndex = theIndex;
    return theResult;
}

// FUNCTION: EXE 0x41634f
// Returns the next path node's tile and index and steps to its parent; false (zeros) at the end
bool EEAIAStarGraphStorage::GetNextPathNode(EETileCoordinate& outTile, long& outIndex)
{
    int32_t theX;
    int32_t theY;
    int32_t theIndex;
    bool theResult = ee_astar_graph_next_path_node(this->mCore, &theX, &theY, &theIndex);
    outTile.mX = theX;
    outTile.mY = theY;
    outIndex = theIndex;
    return theResult;
}

// FUNCTION: EXE 0x416514
// Prepares the storage for a graph of inCount nodes: clears the arrays (the virtual slot) and nodes and allocates
// empty hash tables; returns true
bool EEAIAStarGraphStorage::Init(long inCount)
{
    this->mCount = inCount;
    ee_astar_graph_init_begin(this->mCore, inCount);
    this->PrepareArrays(inCount);
    ee_astar_graph_init_end(this->mCore);
    return true;
}

// FUNCTION: EXE 0x416af6
// Sizes the flag and node arrays to inCount (reallocating when the count changed) and clears both
void EEAIAStarGraphStorage::PrepareArrays(long inCount)
{
    ee_astar_graph_prepare_arrays(this->mCore, inCount);
}

// New (port): the objects of a list's nodes in its order (the chain goal's NumberChain walked mClosedList and
// mOpenList)
void EEAIAStarGraphStorage::ListObjects(EeAStarGraphList inList, std::vector<AStarGraphableObjectBase*>& outObjects)
{
    uint32_t theCount = ee_astar_graph_list_objects(this->mCore, inList, NULL, 0);
    if (theCount == 0)
        return;
    std::vector<void*> theHandles(theCount);
    ee_astar_graph_list_objects(this->mCore, inList, &theHandles[0], theCount);
    for (uint32_t i = 0; i < theCount; i++)
        outObjects.push_back(static_cast<AStarGraphableObjectBase*>(theHandles[i]));
}

// New (port), TEST ONLY: a node as the capture sees it (EEAIAStarNodeView's fields: the tile, the costs, the parent's
// index in mParentX and the node's own index in mParentY)
static void EEAIAStarGraphViewOf(const EeAStarNodeView& inNode, EEAIAStarNodeView& outNode)
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

static void EEAIAStarGraphListNodes(EeAStarGraphStorage* inCore, EeAStarGraphList inList,
                                    std::vector<EEAIAStarNodeView>& ioNodes)
{
    uint32_t theCount = ee_astar_graph_list(inCore, inList, NULL, 0);
    if (theCount == 0)
        return;
    std::vector<EeAStarNodeView> theNodes(theCount);
    ee_astar_graph_list(inCore, inList, &theNodes[0], theCount);
    EEAIAStarNodeView theView;
    for (uint32_t i = 0; i < theCount; i++) {
        EEAIAStarGraphViewOf(theNodes[i], theView);
        ioNodes.push_back(theView);
    }
}

// New (port), TEST ONLY: the current and path nodes, then the closed list, the closed table (the last bucket first),
// the sorted open list, the open table (the last bucket first) and the fast open list, each digested in order
void EEAIAStarGraphStorage::CaptureNodes(EEAIAStarCaptureRun& ioRun)
{
    for (uint32_t i = 0; i < 2; i++) {
        EeAStarNodeView theNode;
        bool theHas = ee_astar_graph_node(this->mCore, i, &theNode);
        ioRun.U8(theHas);
        if (theHas) {
            EEAIAStarNodeView theView;
            EEAIAStarGraphViewOf(theNode, theView);
            ioRun.Node(theView);
        }
    }
    static const EeAStarGraphList kLists[5] = {EeAStarGraphList::Closed, EeAStarGraphList::ClosedTable,
                                               EeAStarGraphList::SortedOpen, EeAStarGraphList::OpenTable,
                                               EeAStarGraphList::FastOpen};
    std::vector<EEAIAStarNodeView> theList;
    for (long i = 0; i < 5; i++) {
        theList.clear();
        EEAIAStarGraphListNodes(this->mCore, kLists[i], theList);
        ioRun.Nodes(theList);
    }
}
