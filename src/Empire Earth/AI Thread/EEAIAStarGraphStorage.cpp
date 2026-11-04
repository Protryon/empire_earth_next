// C:\EEX\Empire Earth\AI Thread\EEAIAStarGraphStorage.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarGraphStorage, vtable 0x439ee8). The graph A* node storage: 0x16070..0x16c0e (the graph goal base's ??_G
// ends at 0x16070; EEAIAStarHillGoal's ctor starts at 0x16c0e; the name sorts between EEAIAStarGraphContinentalChainGoal
// and EEAIAStarHillGoal). The twin of EEAIAStarStorage.cpp: the node free list (file statics: every user is here),
// ctor, ??_G (after its first user), dtor, then the members in address order.
#include <windows.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"

#include "ee/EETileCoordinate.h"
#include "ee/EEAIAStarGraphStorage.h"
#include "ee/EEMathInline.h"

// the free nodes and their counters (0x4e5c28..0x4e5c3c) (provisional names)
static EEAIAStarGraphNode* sGSFreeNodes;   // 0x4e5c28
static long sGSNewNodeCalls;            // 0x4e5c2c
static long sGSReusedNodes;             // 0x4e5c30
static long sGSFreeCount;               // 0x4e5c34
static long sGSAllocatedNodes;          // 0x4e5c38
static long sGSPooledNodes;             // 0x4e5c3c

// FUNCTION: EXE 0x416070
// Deletes every node in the shared free node pool and resets the pool's counters
void EEAIAStarGraphStorage::FreeNodePool()
{
    EEAIAStarGraphNode* theNode = sGSFreeNodes;
    while (theNode) {
        EEAIAStarGraphNode* theDelete = theNode;
        theNode = theNode->mNext;
        if (theDelete)
            delete theDelete;
        sGSPooledNodes--;
        sGSAllocatedNodes--;
    }
    sGSFreeNodes = NULL;
    sGSFreeCount = 0;
    sGSNewNodeCalls = 0;
    sGSReusedNodes = 0;
}

// FUNCTION: EXE 0x4160b7
// Creates an empty graph node storage with no lists or tables and 27 hash buckets
EEAIAStarGraphStorage::EEAIAStarGraphStorage()
{
    this->mCount = 0;
    this->mSortedOpen = NULL;
    this->mClosedList = NULL;
    this->mCurrentNode = NULL;
    this->mPath = NULL;
    this->mOpenList = NULL;
    this->mPathCursor = NULL;
    this->mFlags = NULL;
    this->mClosedTable = NULL;
    this->mOpenTable = NULL;
    this->mSortedOpenTail = NULL;
    this->mNodes = NULL;
    this->mBucketCount = 0x1b;
    this->mOldCount = 0;
}

// SYNTHETIC: EXE 0x4160f0
// (the scalar deleting destructor)

// FUNCTION: EXE 0x41610c
// Destroys the storage, returning its nodes to the free pool and freeing its flag and node arrays
EEAIAStarGraphStorage::~EEAIAStarGraphStorage()
{
    this->Clear();
    delete[] this->mFlags;
    delete[] this->mClosedTable;
    delete[] this->mOpenTable;
    delete[] this->mNodes;
}

// FUNCTION: EXE 0x41613f
// Puts a node at the head of the closed list, flags its index closed and records it in the node array
void EEAIAStarGraphStorage::AddClosed(EEAIAStarGraphNode* inNode)
{
    if (this->mClosedList)
        this->mClosedList->mPrev = inNode;
    inNode->mNext = this->mClosedList;
    this->mClosedList = inNode;
    unsigned char* theFlags = this->mFlags + inNode->mIndex;
    *theFlags |= kAStarTileClosed;
    this->mNodes[inNode->mIndex] = inNode;
}

// FUNCTION: EXE 0x41616b
// Returns a node to the shared free node pool
void EEAIAStarGraphStorage::ReleaseNode(EEAIAStarGraphNode* inNode)
{
    inNode->mNext = sGSFreeNodes;
    sGSPooledNodes++;
    sGSFreeCount++;
    sGSFreeNodes = inNode;
}

// FUNCTION: EXE 0x41618a
// Opens a node: inserts it into the cost-sorted open list when it is no dearer than the tail, else into its
// hash bucket; flags its index open and records it in the node array
void EEAIAStarGraphStorage::AddOpen(EEAIAStarGraphNode* inNode)
{
    bool theInserted = false;
    if (this->mSortedOpenTail && inNode->mCost <= this->mSortedOpenTail->mCost) {
        for (EEAIAStarGraphNode* theNode = this->mSortedOpen; theNode; theNode = theNode->mNext) {
            if (!(inNode->mCost <= theNode->mCost))
                continue;
            if (theNode->mPrev) {
                theNode->mPrev->mNext = inNode;
                inNode->mNext = theNode;
                inNode->mPrev = theNode->mPrev;
                theNode->mPrev = inNode;
            } else {
                inNode->mNext = theNode;
                theNode->mPrev = inNode;
                this->mSortedOpen = inNode;
            }
            if (theNode->mNext == NULL)
                this->mSortedOpenTail = theNode;
            theInserted = true;
            break;
        }
    }
    if (!theInserted) {
        long theBucket = this->GetBucket(inNode->mTile.mX, inNode->mTile.mY);
        if (this->mOpenTable[theBucket])
            this->mOpenTable[theBucket]->mPrev = inNode;
        inNode->mNext = this->mOpenTable[theBucket];
        this->mOpenTable[theBucket] = inNode;
    }
    unsigned char* theFlags = this->mFlags + inNode->mIndex;
    *theFlags |= kAStarTileOpen;
    this->mNodes[inNode->mIndex] = inNode;
}

// FUNCTION: EXE 0x416221
// Returns all nodes to the free pool and forgets the found path
void EEAIAStarGraphStorage::ResetNodes()
{
    this->Clear();
    this->mPathCursor = NULL;
    this->mPath = NULL;
}

// FUNCTION: EXE 0x416233
// Returns the hash bucket of a tile
long EEAIAStarGraphStorage::GetBucket(long inX, long inY)
{
    long theHash = (inX << 4) + inY;
    long theHigh = theHash & 0xf0000000;
    if (theHigh)
        theHash = (theHigh >> 24) ^ theHigh;
    return theHash % this->mBucketCount;
}

// FUNCTION: EXE 0x416258
// Starts walking the found path and returns its first node's tile and index; false (zeros) without a path
bool EEAIAStarGraphStorage::GetFirstPathNode(EETileCoordinate& outTile, long& outIndex)
{
    bool theResult = false;
    if (this->mPath) {
        this->mPathCursor = this->mPath;
        if (this->mPath->mParent) {
            theResult = this->GetNextPathNode(outTile, outIndex);
        } else {
            outTile = this->mPath->mTile;
            outIndex = this->mPath->mIndex;
            theResult = true;
            this->mPathCursor = NULL;
        }
    } else {
        outIndex = 0;
        outTile.mX = 0;
        outTile.mY = 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x4162b0
// Returns a node for a tile, object and index, reusing one from the free pool (reset) or allocating a new one
EEAIAStarGraphNode* EEAIAStarGraphStorage::NewNode(EETileCoordinate inTile, AStarGraphableObjectBase* inObject,
                                                   long inIndex)
{
    EEAIAStarGraphNode* theNode;
    sGSNewNodeCalls++;
    if (sGSFreeNodes) {
        theNode = sGSFreeNodes;
        sGSFreeNodes = sGSFreeNodes->mNext;
        theNode->mTile = inTile;
        theNode->mIndex = inIndex;
        theNode->mCost = 0;
        theNode->mPathCost = 0;
        theNode->mEstimate = 0;
        theNode->mParent = NULL;
        theNode->mPrev = NULL;
        theNode->mNext = NULL;
        theNode->mObject = inObject;
        sGSPooledNodes--;
        sGSReusedNodes++;
        sGSFreeCount--;
    } else {
        theNode = new EEAIAStarGraphNode(inTile, inIndex);
        theNode->mObject = inObject;
        sGSAllocatedNodes++;
    }
    return theNode;
}

// FUNCTION: EXE 0x41634f
// Returns the next path node's tile and index and steps to its parent; false (zeros) at the end
bool EEAIAStarGraphStorage::GetNextPathNode(EETileCoordinate& outTile, long& outIndex)
{
    bool theResult = false;
    if (this->mPathCursor) {
        outTile = this->mPathCursor->mTile;
        outIndex = this->mPathCursor->mIndex;
        this->mPathCursor = this->mPathCursor->mParent;
        theResult = true;
    } else {
        outIndex = 0;
        outTile.mX = 0;
        outTile.mY = 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x416394
// Returns the current node and every closed, sorted open, bucketed and pushed node to the free pool and frees
// the hash tables
void EEAIAStarGraphStorage::Clear()
{
    EEAIAStarGraphNode* theLast;
    long theCount;
    if (this->mCurrentNode) {
        EEAIAStarGraphStorage::ReleaseNode(this->mCurrentNode);
        this->mCurrentNode = NULL;
    }
    if (this->mClosedTable) {
        for (long i = this->mBucketCount - 1; i >= 0; i--) {
            if (!this->mClosedTable[i])
                continue;
            theCount = 0;
            for (theLast = this->mClosedTable[i]; theLast; theLast = theLast->mNext, theCount++)
                if (theLast->mNext == NULL)
                    break;
            theCount++;   // the last node counts too (the loop stops before counting it)
            theLast->mNext = sGSFreeNodes;
            sGSFreeNodes = this->mClosedTable[i];
            sGSFreeCount += theCount;
            sGSPooledNodes += theCount;
            this->mClosedTable[i] = NULL;
        }
        delete[] this->mClosedTable;
        this->mClosedTable = NULL;
    }
    if (this->mClosedList) {
        theCount = 0;
        for (theLast = this->mClosedList; theLast; theLast = theLast->mNext, theCount++)
            if (theLast->mNext == NULL)
                break;
        theCount++;   // the last node counts too (the loop stops before counting it)
        theLast->mNext = sGSFreeNodes;
        sGSFreeNodes = this->mClosedList;
        sGSFreeCount += theCount;
        sGSPooledNodes += theCount;
        this->mClosedList = NULL;
    }
    if (this->mSortedOpen) {
        theCount = 0;
        for (theLast = this->mSortedOpen; theLast; theLast = theLast->mNext, theCount++)
            if (theLast->mNext == NULL)
                break;
        theCount++;   // the last node counts too (the loop stops before counting it)
        theLast->mNext = sGSFreeNodes;
        sGSFreeNodes = this->mSortedOpen;
        sGSFreeCount += theCount;
        sGSPooledNodes += theCount;
        this->mSortedOpen = NULL;
        this->mSortedOpenTail = NULL;
    }
    if (this->mOpenTable) {
        for (long i = this->mBucketCount - 1; i >= 0; i--) {
            if (!this->mOpenTable[i])
                continue;
            theCount = 0;
            for (theLast = this->mOpenTable[i]; theLast; theLast = theLast->mNext, theCount++)
                if (theLast->mNext == NULL)
                    break;
            theCount++;   // the last node counts too (the loop stops before counting it)
            theLast->mNext = sGSFreeNodes;
            sGSFreeNodes = this->mOpenTable[i];
            sGSFreeCount += theCount;
            sGSPooledNodes += theCount;
            this->mOpenTable[i] = NULL;
        }
        delete[] this->mOpenTable;
        this->mOpenTable = NULL;
    }
    if (!this->mOpenList)
        return;
    theCount = 0;
    for (theLast = this->mOpenList; theLast; theLast = theLast->mNext, theCount++)
        if (theLast->mNext == NULL)
            break;
    theCount++;   // the last node counts too (the loop stops before counting it)
    theLast->mNext = sGSFreeNodes;
    sGSFreeNodes = this->mOpenList;
    sGSFreeCount += theCount;
    sGSPooledNodes += theCount;
    this->mOpenList = NULL;
}

// FUNCTION: EXE 0x416514
// Prepares the storage for a graph of inCount nodes: clears the arrays and nodes and allocates empty hash
// tables; returns true
bool EEAIAStarGraphStorage::Init(long inCount)
{
    this->mOldCount = this->mCount;
    this->mCount = inCount;
    this->PrepareArrays(inCount);
    this->Clear();
    this->mClosedTable = new EEAIAStarGraphNode*[this->mBucketCount];
    this->mOpenTable = new EEAIAStarGraphNode*[this->mBucketCount];
    for (long i = this->mBucketCount - 1; i >= 0; i--) {
        this->mClosedTable[i] = NULL;
        this->mOpenTable[i] = NULL;
    }
    return true;
}

// FUNCTION: EXE 0x41656d
// Re-files an open node after its cost changed: unlinks it, then inserts it into the sorted open list when it
// is no dearer than the tail, else at the head of its hash bucket
void EEAIAStarGraphStorage::UpdateOpen(EEAIAStarGraphNode* inNode)
{
    long theBucket = this->GetBucket(inNode->mTile.mX, inNode->mTile.mY);
    unsigned long theCost = inNode->mCost;
    if (inNode->mPrev) {
        inNode->mPrev->mNext = inNode->mNext;
        if (inNode->mNext)
            inNode->mNext->mPrev = inNode->mPrev;
    } else if (this->mOpenTable[theBucket] == inNode) {
        this->mOpenTable[theBucket] = this->mOpenTable[theBucket]->mNext;
        if (this->mOpenTable[theBucket])
            this->mOpenTable[theBucket]->mPrev = NULL;
    } else if (this->mSortedOpen == inNode) {
        this->mSortedOpen = this->mSortedOpen->mNext;
        if (this->mSortedOpen)
            this->mSortedOpen->mPrev = NULL;
    }
    if (this->mSortedOpenTail == inNode)
        this->mSortedOpenTail = inNode->mPrev;
    inNode->mPrev = NULL;
    inNode->mNext = NULL;
    if (this->mSortedOpen && this->mSortedOpenTail && theCost <= this->mSortedOpenTail->mCost) {
        EEAIAStarGraphNode* theNode;
        for (theNode = this->mSortedOpen; theNode; theNode = theNode->mNext)
            if (theCost <= theNode->mCost)
                break;
        if (theNode) {
            if (theNode->mPrev) {
                inNode->mPrev = theNode->mPrev;
                inNode->mNext = theNode;
                theNode->mPrev->mNext = inNode;
                theNode->mPrev = inNode;
            } else {
                this->mSortedOpen->mPrev = inNode;
                inNode->mNext = this->mSortedOpen;
                this->mSortedOpen = inNode;
            }
            if (inNode->mNext == NULL)
                this->mSortedOpenTail = inNode;
            return;
        }
    }
    if (this->mOpenTable[theBucket])
        this->mOpenTable[theBucket]->mPrev = inNode;
    inNode->mNext = this->mOpenTable[theBucket];
    this->mOpenTable[theBucket] = inNode;
}

// FUNCTION: EXE 0x41665f
// Removes and returns the closed node nearest to a tile (by Manhattan distance), clearing its closed flag;
// NULL when none
EEAIAStarGraphNode* EEAIAStarGraphStorage::RemoveNearestClosed(const EETileCoordinate& inTile)
{
    long theBestDistance = -1;
    EEAIAStarGraphNode* theResult = NULL;
    EEAIAStarGraphNode* theNode;
    long theDistance;
    for (theNode = this->mClosedList; theNode; theNode = theNode->mNext) {
        theDistance = UMAbs(theNode->mTile.mX - inTile.mX) + UMAbs(theNode->mTile.mY - inTile.mY);
        if (theBestDistance == -1 || theDistance < theBestDistance) {
            theResult = theNode;
            theBestDistance = theDistance;
        }
    }
    for (long i = this->mBucketCount - 1; i >= 0; i--) {
        for (theNode = this->mClosedTable[i]; theNode; theNode = theNode->mNext) {
            theDistance = UMAbs(theNode->mTile.mX - inTile.mX) + UMAbs(theNode->mTile.mY - inTile.mY);
            if (theBestDistance == -1 || theDistance < theBestDistance) {
                theResult = theNode;
                theBestDistance = theDistance;
            }
        }
    }
    if (theResult) {
        if (theResult->mPrev) {
            theResult->mPrev->mNext = theResult->mNext;
            if (theResult->mNext)
                theResult->mNext->mPrev = theResult->mPrev;
        } else {
            long theBucket = this->GetBucket(theResult->mTile.mX, theResult->mTile.mY);
            if (theResult == this->mClosedTable[theBucket])
                this->mClosedTable[theBucket] = theResult->mNext;
            else
                this->mClosedList = theResult->mNext;
            if (theResult->mNext)
                theResult->mNext->mPrev = NULL;
        }
        theResult->mNext = NULL;
        theResult->mPrev = NULL;
        unsigned char* theFlags = this->mFlags + theResult->mIndex;
        *theFlags &= ~kAStarTileClosed;
    }
    return theResult;
}

// FUNCTION: EXE 0x416771
// Removes and returns the open node nearest to a tile (by Manhattan distance) from whichever open list holds it,
// clearing its open flag; NULL when none
EEAIAStarGraphNode* EEAIAStarGraphStorage::RemoveNearestOpen(const EETileCoordinate& inTile)
{
    long theBestDistance = -1;
    EEAIAStarGraphNode* theResult = NULL;
    EEAIAStarGraphNode* theNode;
    long theDistance;
    for (theNode = this->mSortedOpen; theNode; theNode = theNode->mNext) {
        theDistance = UMAbs(theNode->mTile.mX - inTile.mX) + UMAbs(theNode->mTile.mY - inTile.mY);
        if (theBestDistance == -1 || theDistance < theBestDistance) {
            theResult = theNode;
            theBestDistance = theDistance;
        }
    }
    for (theNode = this->mOpenList; theNode; theNode = theNode->mNext) {
        theDistance = UMAbs(theNode->mTile.mX - inTile.mX) + UMAbs(theNode->mTile.mY - inTile.mY);
        if (theBestDistance == -1 || theDistance < theBestDistance) {
            theResult = theNode;
            theBestDistance = theDistance;
        }
    }
    for (long i = this->mBucketCount - 1; i >= 0; i--) {
        for (theNode = this->mOpenTable[i]; theNode; theNode = theNode->mNext) {
            theDistance = UMAbs(theNode->mTile.mX - inTile.mX) + UMAbs(theNode->mTile.mY - inTile.mY);
            if (theBestDistance == -1 || theDistance < theBestDistance) {
                theResult = theNode;
                theBestDistance = theDistance;
            }
        }
    }
    if (theResult) {
        if (theResult->mPrev) {
            theResult->mPrev->mNext = theResult->mNext;
            if (theResult->mNext)
                theResult->mNext->mPrev = theResult->mPrev;
        } else {
            long theBucket = this->GetBucket(theResult->mTile.mX, theResult->mTile.mY);
            if (this->mOpenTable[theBucket] == theResult) {
                if (theResult->mNext)
                    theResult->mNext->mPrev = NULL;
                this->mOpenTable[theBucket] = theResult->mNext;
            } else if (this->mOpenList == theResult) {
                if (theResult->mNext)
                    theResult->mNext->mPrev = NULL;
                this->mOpenList = theResult->mNext;
            } else if (this->mSortedOpen == theResult) {
                if (theResult->mNext)
                    theResult->mNext->mPrev = NULL;
                this->mSortedOpen = theResult->mNext;
                if (this->mSortedOpenTail == theResult)
                    this->mSortedOpen = theResult->mPrev;
            }
        }
        if (this->mSortedOpenTail == theResult)
            this->mSortedOpenTail = theResult->mPrev;
        theResult->mNext = NULL;
        theResult->mPrev = NULL;
        unsigned char* theFlags = this->mFlags + theResult->mIndex;
        *theFlags &= ~kAStarTileOpen;
    }
    return theResult;
}

// FUNCTION: EXE 0x416905
// Removes and returns the cheapest open node; when the sorted list is empty, first moves up to 15 of the
// cheapest bucketed nodes into it; NULL when nothing is open
EEAIAStarGraphNode* EEAIAStarGraphStorage::RemoveBestOpen()
{
    EEAIAStarGraphNode* theResult = NULL;
    if (this->mSortedOpen == NULL) {
        EEAIAStarGraphNode* theBest[15];
        long theWorst = -1;
        unsigned long theWorstCost = 0;
        long theSeen = 0;
        long theFilled = 0;
        long i;
        for (i = this->mBucketCount - 1; i >= 0; i--) {
            for (EEAIAStarGraphNode* theNode = this->mOpenTable[i]; theNode; theNode = theNode->mNext) {
                if (theSeen >= 15) {
                    if (theNode->mCost < theWorstCost) {
                        theBest[theWorst] = theNode;
                        theWorst = 0;
                        for (long j = 1; j < 15; j++)
                            if (theBest[j]->mCost > theBest[theWorst]->mCost)
                                theWorst = j;
                        theWorstCost = theBest[theWorst]->mCost;
                        theSeen++;
                    }
                } else {
                    if (theWorst != -1) {
                        if (theNode->mCost >= theBest[theWorst]->mCost) {
                            theWorst = theFilled;
                            theWorstCost = theNode->mCost;
                        }
                        theBest[theFilled++] = theNode;
                    } else {
                        theWorstCost = theNode->mCost;
                        theWorst = 0;
                        theBest[0] = theNode;
                        theFilled = 1;
                    }
                    theSeen++;
                }
            }
        }
        if (theWorst != -1) {
            for (i = min(15, theFilled) - 1; i >= 0; i--) {
                EEAIAStarGraphNode* theNode = theBest[i];
                if (theNode->mPrev) {
                    theNode->mPrev->mNext = theNode->mNext;
                    if (theNode->mNext)
                        theNode->mNext->mPrev = theNode->mPrev;
                    theNode->mPrev = NULL;
                    theNode->mNext = NULL;
                } else {
                    long theBucket = this->GetBucket(theNode->mTile.mX, theNode->mTile.mY);
                    this->mOpenTable[theBucket] = theNode->mNext;
                    if (this->mOpenTable[theBucket])
                        this->mOpenTable[theBucket]->mPrev = NULL;
                    theNode->mNext = NULL;
                }
                this->InsertSorted(theNode);
            }
        }
    }
    if (this->mSortedOpen) {
        theResult = this->mSortedOpen;
        this->mSortedOpen = theResult->mNext;
        if (this->mSortedOpen)
            this->mSortedOpen->mPrev = NULL;
        if (this->mSortedOpen == NULL)
            this->mSortedOpenTail = NULL;
        if (theResult) {
            theResult->mNext = NULL;
            theResult->mPrev = NULL;
            unsigned char* theFlags = this->mFlags + theResult->mIndex;
            *theFlags &= ~kAStarTileOpen;
            this->mNodes[theResult->mIndex] = NULL;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x416aa5
// Unlinks a node from the closed list, clears its closed flag and removes it from the node array
void EEAIAStarGraphStorage::RemoveClosed(EEAIAStarGraphNode* inNode)
{
    if (inNode->mPrev) {
        inNode->mPrev->mNext = inNode->mNext;
        if (inNode->mNext)
            inNode->mNext->mPrev = inNode->mPrev;
    } else {
        this->mClosedList = inNode->mNext;
        if (inNode->mNext)
            inNode->mNext->mPrev = NULL;
    }
    inNode->mNext = NULL;
    inNode->mPrev = NULL;
    unsigned char* theFlags = this->mFlags + inNode->mIndex;
    *theFlags &= ~kAStarTileClosed;
    this->mNodes[inNode->mIndex] = NULL;
}

// FUNCTION: EXE 0x416af6
// Sizes the flag and node arrays to inCount (reallocating when the count changed) and clears both
void EEAIAStarGraphStorage::PrepareArrays(long inCount)
{
    if (this->mFlags == NULL || this->mNodes == NULL || this->mOldCount != inCount) {
        delete[] this->mFlags;
        this->mFlags = new unsigned char[inCount];
        delete[] this->mNodes;
        this->mNodes = new EEAIAStarGraphNode*[inCount];
    }
    memset(this->mNodes, 0, inCount * sizeof(EEAIAStarGraphNode*));
    memset(this->mFlags, 0, inCount);
}

// FUNCTION: EXE 0x416b5b
// Inserts a node into the cost-sorted open list before the first node that costs at least as much
void EEAIAStarGraphStorage::InsertSorted(EEAIAStarGraphNode* inNode)
{
    EEAIAStarGraphNode* thePrev = NULL;
    EEAIAStarGraphNode* theNode = this->mSortedOpen;
    unsigned long theCost = inNode->mCost;
    if (theNode) {
        for (; theNode; theNode = theNode->mNext) {
            if (theCost <= theNode->mCost)
                break;
            thePrev = theNode;
        }
        if (theNode) {
            if (theNode->mPrev) {
                theNode->mPrev->mNext = inNode;
                inNode->mNext = theNode;
                inNode->mPrev = theNode->mPrev;
                theNode->mPrev = inNode;
            } else {
                inNode->mNext = theNode;
                theNode->mPrev = inNode;
                this->mSortedOpen = inNode;
            }
        } else {
            inNode->mPrev = thePrev;
            thePrev->mNext = inNode;
            this->mSortedOpenTail = inNode;
        }
    } else {
        this->mSortedOpen = inNode;
        this->mSortedOpenTail = inNode;
    }
}

// FUNCTION: EXE 0x416bb5
// Pushes a node onto the unsorted open list, flags its index open and records it in the node array
void EEAIAStarGraphStorage::Push(EEAIAStarGraphNode* inNode)
{
    unsigned char* theFlags = this->mFlags + inNode->mIndex;
    *theFlags |= kAStarTileOpen;
    if (this->mOpenList)
        this->mOpenList->mPrev = inNode;
    inNode->mNext = this->mOpenList;
    this->mOpenList = inNode;
    this->mNodes[inNode->mIndex] = inNode;
}

// FUNCTION: EXE 0x416be1
// Pops the head of the unsorted open list (NULL when empty), clearing its open flag and node array entry; the
// new head keeps its stale back link
EEAIAStarGraphNode* EEAIAStarGraphStorage::Pop()
{
    EEAIAStarGraphNode* theNode = NULL;
    if (this->mOpenList) {
        theNode = this->mOpenList;
        this->mOpenList = theNode->mNext;
        theNode->mNext = NULL;
        theNode->mPrev = NULL;
        unsigned char* theFlags = this->mFlags + theNode->mIndex;
        *theFlags &= ~kAStarTileOpen;
        this->mNodes[theNode->mIndex] = NULL;
    }
    return theNode;
}
