// C:\EEX\Empire Earth\AI Thread\EEAIAStarStorage.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarStorage, vtable 0x43a0f0). The A* node storage: 0x1b991..0x1c666 (EEAIAStarPathSectorStorage's Virtual10
// ends at 0x1b991, EEAIAStarTerrainStorage's ctor starts at 0x1c666). The node free list (a static member: every user
// is here), ctor, ??_G (after its first user), dtor, then the members in address order.
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

// the free nodes (0x4e5c40) (provisional name)
static EEAIAStarNode* sFreeNodes;

// FUNCTION: EXE 0x41b991
// Deletes every node on the shared free node list and empties it
void EEAIAStarStorage::FreeNodePool()
{
    EEAIAStarNode* theNode = sFreeNodes;
    while (theNode) {
        EEAIAStarNode* theDelete = theNode;
        theNode = theNode->mNext;
        if (theDelete)
            delete theDelete;
    }
    sFreeNodes = NULL;
}

// FUNCTION: EXE 0x41b9b7
// Creates an empty A* node storage with no grid, no lists and 53 hash buckets
EEAIAStarStorage::EEAIAStarStorage()
{
    this->mWidth = 0;
    this->mHeight = 0;
    this->mHasBounds = false;
    this->mSortedOpen = NULL;
    this->mFastClosedList = NULL;
    this->mCurrentNode = NULL;
    this->mPath = NULL;
    this->mFastOpenList = NULL;
    this->mPathCursor = NULL;
    this->mTileFlags = NULL;
    this->mClosedTable = NULL;
    this->mOpenTable = NULL;
    this->mSortedOpenTail = NULL;
    this->mBucketCount = 0x35;
}

// SYNTHETIC: EXE 0x41b9fc
// (the scalar deleting destructor)

// FUNCTION: EXE 0x41ba18
// Destroys the storage, returning its nodes to the free list and freeing the tile flags and hash tables
EEAIAStarStorage::~EEAIAStarStorage()
{
    this->Clear();
    delete[] this->mTileFlags;
    delete[] this->mClosedTable;
    delete[] this->mOpenTable;
}

// FUNCTION: EXE 0x41ba43
// Pushes a node onto its closed hash bucket and flags its tile closed
void EEAIAStarStorage::AddClosed(EEAIAStarNode* inNode)
{
    long theBucket = this->GetBucket(inNode->mX, inNode->mY);
    if (this->mClosedTable[theBucket])
        this->mClosedTable[theBucket]->mPrev = inNode;
    inNode->mNext = this->mClosedTable[theBucket];
    this->mClosedTable[theBucket] = inNode;
    unsigned char* theFlags = this->mTileFlags + this->mWidth * inNode->mX + inNode->mY;
    *theFlags |= kAStarTileClosed;
}

// FUNCTION: EXE 0x41ba9b
// Returns a node to the shared free node list
void EEAIAStarStorage::ReleaseNode(EEAIAStarNode* inNode)
{
    inNode->mNext = sFreeNodes;
    sFreeNodes = inNode;
}

// FUNCTION: EXE 0x41baae
// Adds a node to the open set: into the cost-sorted list when it costs no more than its tail, else onto its open
// hash bucket; flags its tile open
void EEAIAStarStorage::AddOpen(EEAIAStarNode* inNode)
{
    bool theInserted = false;
    if (this->mSortedOpenTail && inNode->mCost <= this->mSortedOpenTail->mCost) {
        for (EEAIAStarNode* theNode = this->mSortedOpen; theNode; theNode = theNode->mNext) {
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
        long theBucket = this->GetBucket(inNode->mX, inNode->mY);
        if (this->mOpenTable[theBucket])
            this->mOpenTable[theBucket]->mPrev = inNode;
        inNode->mNext = this->mOpenTable[theBucket];
        this->mOpenTable[theBucket] = inNode;
    }
    unsigned char* theFlags = this->mTileFlags + this->mWidth * inNode->mX + inNode->mY;
    *theFlags |= kAStarTileOpen;
}

// FUNCTION: EXE 0x41bb52
// Releases every node and forgets the found path
void EEAIAStarStorage::ResetNodes()
{
    this->Clear();
    this->mPathCursor = NULL;
    this->mPath = NULL;
}

// FUNCTION: EXE 0x41bb64
// Returns the closed node at tile (inX, inY), or NULL when the tile is not flagged closed
EEAIAStarNode* EEAIAStarStorage::FindClosed(long inX, long inY)
{
    EEAIAStarNode* theNode = NULL;
    if (*(this->mTileFlags + this->mWidth * inX + inY) & kAStarTileClosed) {
        for (theNode = this->mClosedTable[this->GetBucket(inX, inY)]; theNode; theNode = theNode->mNext)
            if (theNode->mX == inX && theNode->mY == inY)
                break;
    }
    return theNode;
}

// FUNCTION: EXE 0x41bbb9
// Returns the open node at tile (inX, inY) from the sorted list or its hash bucket, or NULL when not flagged open
EEAIAStarNode* EEAIAStarStorage::FindOpen(long inX, long inY)
{
    EEAIAStarNode* theNode = NULL;
    if (*(this->mTileFlags + this->mWidth * inX + inY) & kAStarTileOpen) {
        for (theNode = this->mSortedOpen; theNode; theNode = theNode->mNext)
            if (theNode->mX == inX && theNode->mY == inY)
                break;
        if (!theNode) {
            for (theNode = this->mOpenTable[this->GetBucket(inX, inY)]; theNode; theNode = theNode->mNext)
                if (theNode->mX == inX && theNode->mY == inY)
                    break;
        }
    }
    return theNode;
}

// FUNCTION: EXE 0x41bc2a
// Starts walking the found path and returns its first tile; false when there is no path
bool EEAIAStarStorage::GetFirstPathTile(long& outX, long& outY)
{
    bool theResult = false;
    if (this->mPath) {
        this->mPathCursor = this->mPath;
        if (this->mPath->mParent) {
            theResult = this->GetNextPathTile(outX, outY);
        } else {
            outX = this->mPath->mX;
            outY = this->mPath->mY;
            theResult = true;
            this->mPathCursor = NULL;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x41bc68
// Returns a cleared node for tile (inX, inY), reused from the free list or newly allocated (NULL if that fails)
EEAIAStarNode* EEAIAStarStorage::NewNode(long inX, long inY)
{
    EEAIAStarNode* theNode = NULL;
    if (sFreeNodes) {
        theNode = sFreeNodes;
        sFreeNodes = sFreeNodes->mNext;
        theNode->mCost = 0;
        theNode->mPathCost = 0;
        theNode->mEstimate = 0;
        theNode->mParent = NULL;
        theNode->mPrev = NULL;
        theNode->mNext = NULL;
    } else {
        theNode = new EEAIAStarNode;
        if (!theNode)
            return NULL;
        theNode->mParent = NULL;
        theNode->mNext = NULL;
        theNode->mPrev = NULL;
        theNode->mCost = 0;
        theNode->mPathCost = 0;
        theNode->mEstimate = 0;
    }
    theNode->mX = inX;
    theNode->mY = inY;
    return theNode;
}

// FUNCTION: EXE 0x41bcc3
// Returns the next tile along the found path (towards the start); false and (0, 0) at its end
bool EEAIAStarStorage::GetNextPathTile(long& outX, long& outY)
{
    if (this->mPathCursor) {
        outX = this->mPathCursor->mX;
        outY = this->mPathCursor->mY;
        this->mPathCursor = this->mPathCursor->mParent;
        return true;
    }
    outX = 0;
    outY = 0;
    return false;
}

// FUNCTION: EXE 0x41bcfe
// Moves the current node and every open and closed node to the free list and frees both hash tables
void EEAIAStarStorage::Clear()
{
    EEAIAStarNode* theLast;
    if (this->mCurrentNode) {
        EEAIAStarStorage::ReleaseNode(this->mCurrentNode);
        this->mCurrentNode = NULL;
    }
    if (this->mClosedTable) {
        for (long i = this->mBucketCount - 1; i >= 0; i--) {
            if (!this->mClosedTable[i])
                continue;
            for (theLast = this->mClosedTable[i]; theLast; theLast = theLast->mNext)
                if (theLast->mNext == NULL)
                    break;
            theLast->mNext = sFreeNodes;
            sFreeNodes = this->mClosedTable[i];
            this->mClosedTable[i] = NULL;
        }
        delete[] this->mClosedTable;
        this->mClosedTable = NULL;
    }
    if (this->mFastClosedList) {
        for (theLast = this->mFastClosedList; theLast; theLast = theLast->mNext)
            if (theLast->mNext == NULL)
                break;
        theLast->mNext = sFreeNodes;
        sFreeNodes = this->mFastClosedList;
        this->mFastClosedList = NULL;
    }
    if (this->mSortedOpen) {
        for (theLast = this->mSortedOpen; theLast; theLast = theLast->mNext)
            if (theLast->mNext == NULL)
                break;
        theLast->mNext = sFreeNodes;
        sFreeNodes = this->mSortedOpen;
        this->mSortedOpen = NULL;
        this->mSortedOpenTail = NULL;
    }
    if (this->mOpenTable) {
        for (long i = this->mBucketCount - 1; i >= 0; i--) {
            if (!this->mOpenTable[i])
                continue;
            for (theLast = this->mOpenTable[i]; theLast; theLast = theLast->mNext)
                if (theLast->mNext == NULL)
                    break;
            theLast->mNext = sFreeNodes;
            sFreeNodes = this->mOpenTable[i];
            this->mOpenTable[i] = NULL;
        }
        delete[] this->mOpenTable;
        this->mOpenTable = NULL;
    }
    if (!this->mFastOpenList)
        return;
    for (theLast = this->mFastOpenList; theLast; theLast = theLast->mNext)
        if (theLast->mNext == NULL)
            break;
    theLast->mNext = sFreeNodes;
    sFreeNodes = this->mFastOpenList;
    this->mFastOpenList = NULL;
}

// FUNCTION: EXE 0x41be27
// Sets up the storage for an inWidth x inHeight search: releases old nodes, prepares the tile grid and allocates
// empty open and closed hash tables; always returns true
bool EEAIAStarStorage::Init(long inWidth, long inHeight)
{
    this->mWidth = inWidth;
    this->mHeight = inHeight;
    this->Clear();
    this->PrepareGrid(inWidth, inHeight);
    this->mClosedTable = new EEAIAStarNode*[this->mBucketCount];
    this->mOpenTable = new EEAIAStarNode*[this->mBucketCount];
    for (long i = this->mBucketCount - 1; i >= 0; i--) {
        this->mClosedTable[i] = NULL;
        this->mOpenTable[i] = NULL;
    }
    return true;
}

// FUNCTION: EXE 0x41be86
// Re-files an open node after its cost changed: unlinks it, then reinserts it into the sorted list or its bucket
void EEAIAStarStorage::UpdateOpen(EEAIAStarNode* inNode)
{
    long theBucket = this->GetBucket(inNode->mX, inNode->mY);
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
        EEAIAStarNode* theNode;
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

// FUNCTION: EXE 0x41bf8b
// Removes and returns the closed node nearest to a tile (Manhattan distance), clearing its closed flag
EEAIAStarNode* EEAIAStarStorage::RemoveNearestClosed(const EETileCoordinate& inTile)
{
    long theBestDistance = -1;
    EEAIAStarNode* theResult = NULL;
    EEAIAStarNode* theNode;
    long theDistance;
    for (theNode = this->mFastClosedList; theNode; theNode = theNode->mNext) {
        theDistance = UMAbs(theNode->mX - inTile.mX) + UMAbs(theNode->mY - inTile.mY);
        if (theBestDistance == -1 || theDistance < theBestDistance) {
            theResult = theNode;
            theBestDistance = theDistance;
        }
    }
    for (long i = this->mBucketCount - 1; i >= 0; i--) {
        for (theNode = this->mClosedTable[i]; theNode; theNode = theNode->mNext) {
            theDistance = UMAbs(theNode->mX - inTile.mX) + UMAbs(theNode->mY - inTile.mY);
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
            long theBucket = this->GetBucket(theResult->mX, theResult->mY);
            if (theResult == this->mClosedTable[theBucket])
                this->mClosedTable[theBucket] = theResult->mNext;
            else
                this->mFastClosedList = theResult->mNext;
            if (theResult->mNext)
                theResult->mNext->mPrev = NULL;
        }
        theResult->mNext = NULL;
        theResult->mPrev = NULL;
        unsigned char* theFlags = this->mTileFlags + this->mWidth * theResult->mX + theResult->mY;
        *theFlags &= ~kAStarTileClosed;
    }
    return theResult;
}

// FUNCTION: EXE 0x41c0b6
// Removes and returns the open node nearest to a tile (Manhattan distance), clearing its open flag
EEAIAStarNode* EEAIAStarStorage::RemoveNearestOpen(const EETileCoordinate& inTile)
{
    long theBestDistance = -1;
    EEAIAStarNode* theResult = NULL;
    EEAIAStarNode* theNode;
    long theDistance;
    for (theNode = this->mSortedOpen; theNode; theNode = theNode->mNext) {
        theDistance = UMAbs(theNode->mX - inTile.mX) + UMAbs(theNode->mY - inTile.mY);
        if (theBestDistance == -1 || theDistance < theBestDistance) {
            theResult = theNode;
            theBestDistance = theDistance;
        }
    }
    for (theNode = this->mFastOpenList; theNode; theNode = theNode->mNext) {
        theDistance = UMAbs(theNode->mX - inTile.mX) + UMAbs(theNode->mY - inTile.mY);
        if (theBestDistance == -1 || theDistance < theBestDistance) {
            theResult = theNode;
            theBestDistance = theDistance;
        }
    }
    for (long i = this->mBucketCount - 1; i >= 0; i--) {
        for (theNode = this->mOpenTable[i]; theNode; theNode = theNode->mNext) {
            theDistance = UMAbs(theNode->mX - inTile.mX) + UMAbs(theNode->mY - inTile.mY);
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
            long theBucket = this->GetBucket(theResult->mX, theResult->mY);
            if (this->mOpenTable[theBucket] == theResult) {
                if (theResult->mNext)
                    theResult->mNext->mPrev = NULL;
                this->mOpenTable[theBucket] = theResult->mNext;
            } else if (this->mFastOpenList == theResult) {
                if (theResult->mNext)
                    theResult->mNext->mPrev = NULL;
                this->mFastOpenList = theResult->mNext;
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
        unsigned char* theFlags = this->mTileFlags + this->mWidth * theResult->mX + theResult->mY;
        *theFlags &= ~kAStarTileOpen;
    }
    return theResult;
}

// FUNCTION: EXE 0x41c268
// Removes and returns the cheapest open node; when the sorted list is empty it first moves the 15 cheapest
// bucket nodes into it. Returns NULL when nothing is open
EEAIAStarNode* EEAIAStarStorage::RemoveBestOpen()
{
    EEAIAStarNode* theResult = NULL;
    if (this->mSortedOpen == NULL) {
        EEAIAStarNode* theBest[15];
        long theWorst = -1;
        unsigned long theWorstCost = 0;
        long theSeen = 0;
        long theFilled = 0;
        long i;
        for (i = this->mBucketCount - 1; i >= 0; i--) {
            for (EEAIAStarNode* theNode = this->mOpenTable[i]; theNode; theNode = theNode->mNext) {
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
            for (i = (theFilled > 15 ? 15 : theFilled) - 1; i >= 0; i--) {
                EEAIAStarNode* theNode = theBest[i];
                if (theNode->mPrev) {
                    theNode->mPrev->mNext = theNode->mNext;
                    if (theNode->mNext)
                        theNode->mNext->mPrev = theNode->mPrev;
                    theNode->mPrev = NULL;
                    theNode->mNext = NULL;
                } else {
                    long theBucket = this->GetBucket(theNode->mX, theNode->mY);
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
            unsigned char* theFlags = this->mTileFlags + this->mWidth * theResult->mX + theResult->mY;
            *theFlags &= ~kAStarTileOpen;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x41c420
// Unlinks a node from its closed hash bucket and clears its tile's closed flag
void EEAIAStarStorage::RemoveClosed(EEAIAStarNode* inNode)
{
    long theBucket = this->GetBucket(inNode->mX, inNode->mY);
    if (inNode->mPrev) {
        inNode->mPrev->mNext = inNode->mNext;
        if (inNode->mNext)
            inNode->mNext->mPrev = inNode->mPrev;
    } else {
        this->mClosedTable[theBucket] = inNode->mNext;
        if (inNode->mNext)
            inNode->mNext->mPrev = NULL;
    }
    inNode->mNext = NULL;
    inNode->mPrev = NULL;
    unsigned char* theFlags = this->mTileFlags + this->mWidth * inNode->mX + inNode->mY;
    *theFlags &= ~kAStarTileClosed;
}

// FUNCTION: EXE 0x41c491
// Sizes the tile flag grid for the search (reallocating on a size change) and clears it, only the last search's
// bounds plus two when it can
void EEAIAStarStorage::PrepareGrid(long inWidth, long inHeight)
{
    EEAIAStarBounds theBounds = this->mBounds;
    long theOldWidth = this->mWidth;
    long theOldHeight = this->mHeight;
    this->mWidth = inWidth;
    this->mHeight = inHeight;
    if (this->mTileFlags == NULL || theOldWidth != inWidth || theOldHeight != inHeight) {
        delete[] this->mTileFlags;
        this->mTileFlags = new unsigned char[this->mWidth * this->mHeight];
    }
    if (this->mHasBounds && theBounds.mLeft <= theBounds.mRight && theBounds.mTop <= theBounds.mBottom) {
        long theFirstY = 0;
        if (theBounds.mTop - 2 >= 0)
            theFirstY = theBounds.mTop - 2;
        long theFirstX = 0;
        if (theBounds.mLeft - 2 >= 0)
            theFirstX = theBounds.mLeft - 2;
        long theLastX = min(this->mWidth - 1, theBounds.mRight + 2);
        long theLastY = min(this->mHeight - 1, theBounds.mBottom + 2);
        long theStart = this->mWidth * theFirstX + theFirstY;
        memset(this->mTileFlags + theStart, 0, this->mWidth * theLastX - theStart + theLastY);
    } else {
        memset(this->mTileFlags, 0, this->mWidth * this->mHeight);
    }
    this->mHasBounds = false;
    this->mBounds.Clear();
}

// FUNCTION: EXE 0x41c586
// Flags an on-grid tile as accepted or rejected in advance and grows the search bounds to include it
void EEAIAStarStorage::MarkTile(unsigned long inX, unsigned long inY, bool inFlag)
{
    if (!(inX < this->mWidth) || !(inY < this->mHeight))
        return;
    unsigned char* theFlags = this->mTileFlags + this->mWidth * inX + inY;
    if (!inFlag)
        *theFlags |= kAStarTileRejected;
    else
        *theFlags |= kAStarTileAccepted;
    this->ExtendBounds(inX, inY);
}

// FUNCTION: EXE 0x41c5bd
// Inserts a node into the cost-sorted open list before the first node costing at least as much
void EEAIAStarStorage::InsertSorted(EEAIAStarNode* inNode)
{
    EEAIAStarNode* thePrev = NULL;
    EEAIAStarNode* theNode = this->mSortedOpen;
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

// FUNCTION: EXE 0x41c617
// Grows the search bounds to include tile (inX, inY), starting them there while they are all zero
void EEAIAStarStorage::ExtendBounds(long inX, long inY)
{
    if (this->mBounds.mTop == 0 && this->mBounds.mLeft == 0 && this->mBounds.mRight == 0 && this->mBounds.mBottom == 0) {
        this->mBounds.mTop = inY;
        this->mBounds.mLeft = inX;
        this->mBounds.mBottom = inY;
        this->mBounds.mRight = inX;
        this->mHasBounds = true;
    } else {
        EETileCoordinate theTile;
        theTile.mX = inX;
        theTile.mY = inY;
        this->mBounds.Extend(theTile);
    }
}
