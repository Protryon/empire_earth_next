// C:\EEX\Low-Level Engine\Utilities\U2DSparseArray.h (RECONSTRUCTION). Requires lle/LowLevelEngine.h, lle/U2DPoint.h.
// A set of integer points stored as one sorted run list per x column (U2DSparseArray.cpp, 0x100549b6..0x10056369).
// Separate from the key headers (U2DRectangle/U2DPolygon): Default.dll's last unit (0x18ace..0x18af3) carries this
// header's sNodePool init pair but no key initializers.
//
// The node class and the list header's ctor/dtor/GetIsEmpty are in-class inline: U2DSparseArray.cpp emits them
// (in declaration order, the node's members alphabetical) right after the U2DPoint key chains, i.e. at the point the
// header is included, and the callers expand the small ones. Exported members whose kept copy sits elsewhere are
// ICF folds. The list header's inline dtor is also what makes every plug-in includer emit
// UList<U2DSparseArrayPointContainerNode>::sNodePool with its guarded initializer/atexit pair (mixer
// 0x10002205..0x1000239d; see lle/UList.h; probes/tstatic_member.cpp).
#ifndef LLE_U2DSPARSEARRAY_H
#define LLE_U2DSPARSEARRAY_H

#include "lle/UList.h"

class LLE_API U2DSparseArrayPointContainerNode {   // proven sizeof 8 (LLE ctor(long, long) 0x1000c158)
public:
    U2DSparseArrayPointContainerNode() : mStart(0), mEnd(0) {}
    U2DSparseArrayPointContainerNode(long inStart, long inEnd) : mStart(inStart), mEnd(inEnd) {}
    // user-declared: exported ??0U2DSparseArrayPointContainerNode@@QAE@ABV0@@Z (ICF-folded, 0x2fe8b)
    U2DSparseArrayPointContainerNode(const U2DSparseArrayPointContainerNode& inNode)
        : mStart(inNode.mStart), mEnd(inNode.mEnd) {}
    ~U2DSparseArrayPointContainerNode() {}  // proven inline + empty: delete[] of UListNode<...> has no dtor loop

    void Add(long inNumber)
    {
        if (inNumber + 1 == this->mStart)
            this->mStart = inNumber;
        else if (inNumber == this->mEnd + 1)
            this->mEnd = inNumber;
    }
    bool ConfirmPointsAreOk() const { return this->mStart <= this->mEnd; }
    bool Contains(long inNumber) const { return inNumber >= this->mStart && inNumber <= this->mEnd; }
    long GetEnd() const { return this->mEnd; }
    bool GetIsEmpty() const { return this->mStart > this->mEnd; }
    long GetStart() const { return this->mStart; }
    bool Intersects(const U2DSparseArrayPointContainerNode& inNode)
        { return this->mStart <= inNode.mEnd && this->mEnd >= inNode.mStart; }
    void Merge(const U2DSparseArrayPointContainerNode& inNode)
    {
        this->mStart = this->mStart < inNode.mStart ? this->mStart : inNode.mStart;
        this->mEnd = this->mEnd > inNode.mEnd ? this->mEnd : inNode.mEnd;
    }
    bool Remove(long inNumber)
    {
        bool theResult = false;
        if (this->mStart == inNumber && this->mEnd == inNumber) {
            this->mEnd--;
            theResult = true;
        } else if (this->mStart == inNumber) {
            this->mStart++;
            theResult = true;
        } else if (this->mEnd == inNumber) {
            this->mEnd--;
            theResult = true;
        }
        return theResult;
    }
    void SetEnd(long inEnd) { this->mEnd = inEnd; }
    void SetStart(long inStart) { this->mStart = inStart; }
    bool ShouldAddNumber(long inNumber) { return inNumber + 1 >= this->mStart && inNumber <= this->mEnd + 1; }
    bool ShouldMerge(const U2DSparseArrayPointContainerNode& inNode) const
        { return this->mStart <= inNode.mEnd + 1 && this->mEnd + 1 >= inNode.mStart; }
    long Size() const { return this->mEnd - this->mStart + 1; }
    bool operator<(const U2DSparseArrayPointContainerNode& inNode) const { return mStart < inNode.mStart; }
    bool operator>(const U2DSparseArrayPointContainerNode& inNode) const { return this->mEnd > inNode.mEnd; }
    bool operator==(const U2DSparseArrayPointContainerNode& inNode) const
        { return this->mStart == inNode.mStart && this->mEnd == inNode.mEnd; }
    bool operator<(long inNumber) const { return mEnd < inNumber; }
    bool operator<=(long inNumber) const { return this->mEnd <= inNumber; }
    bool operator>(long inNumber) const { return this->mStart > inNumber; }
    bool operator>=(long inNumber) const { return this->mStart >= inNumber; }

    long mStart;                            // proven +0x00 (GetStart)
    long mEnd;                              // proven +0x04 (GetEnd)
};

class LLE_API U2DSparseArrayPointContainerListHeader {   // proven sizeof 0xc (LLE ctor 0x10054b8f)
public:
#ifdef LLE_EXE_HEADERS   // EEUGExplore.cpp (0x2213dd) has no UList sNodePool init pair, so the exe's view has no
                         // inline bodies here (the engine exports both)
    U2DSparseArrayPointContainerListHeader();
    ~U2DSparseArrayPointContainerListHeader();
#else
    U2DSparseArrayPointContainerListHeader() : mSize(0) {}
    ~U2DSparseArrayPointContainerListHeader() {}   // proven: LLE's exported body is `jmp UList::Clear`
#endif
    bool GetIsEmpty() const { return this->mList.mHead == 0; }

    void BuildPointList(long inX, std::vector<U2DPoint<long> >& outList, long inStep);
    bool Contains(const U2DPoint<long>& inPoint) const;
    long CountContainerNodes() const { return this->mList.mCount; }
    U2DPoint<long> GetArrayElement(long inX, long inIndex) const;
    void GetOnlyExtremeEdgePoints(long inX, std::vector<U2DPoint<long> >& outList);
    U2DPoint<long> GetRandomPoint(long inX, URandom& inRandom);
    long GetSize() const { return this->mSize; }
    void Insert(const U2DPoint<long>& inPoint);
    bool Remove(const U2DPoint<long>& inPoint);

protected:
    // the three GetRandom*Between are ICF-folded into one body (0x54d9c)
    long GetRandomIntegerBetween(URandom& inRandom, long inMinimum, long inMaximum) const;
    long GetRandomNumberBetween(URandom& inRandom, long inMinimum, long inMaximum) const;

public:
    UList<U2DSparseArrayPointContainerNode> mList;   // proven +0x00 (head), +0x04 (count)
    long mSize;                                     // proven +0x08 (point count)
};

class LLE_API U2DSparseArrayPointContainer {   // proven sizeof 0x1c (??_E stride)
public:
    U2DSparseArrayPointContainer();
    U2DSparseArrayPointContainer(const U2DSparseArrayPointContainer& inContainer);
#if defined(LLE_EXE_HEADERS) || defined(LOWLEVELENGINE_EXPORTS)
    // inline in the real header: the exe expands it (EEUGExplore's dtor 0x22147e, EEAIArea's 0x11123: the imported
    // vptr, Clear(), mArray's _Tidy), and so does the engine's ??_E 0x10054c15; the exported COMDAT is 0x10054bd0.
    // The renderers' view keeps the declaration (not re-proven there).
    virtual ~U2DSparseArrayPointContainer() { this->Clear(); }
#else
    virtual ~U2DSparseArrayPointContainer();
#endif
    // exported 0xd5a1 (ICF-folded `mov eax, ecx; ret 4`): copies nothing
    U2DSparseArrayPointContainer& operator=(const U2DSparseArrayPointContainer& inContainer) { return *this; }

    void BuildPointList(std::vector<U2DPoint<long> >& outList, long inStep);
    void Clear();
    U2DPoint<long> ComputeCenterPoint(bool inMustBeInList);
    bool Contains(const U2DPoint<long>& inPoint) const;
    long CountContainerNodes();
    void Create(const std::vector<U2DPoint<long> >& inPoints);
    static void DumpPoolStats();
    U2DPoint<long> GetClosestPoint(const U2DPoint<long>& inPoint);
    void GetOnlyExtremeEdgePoints(std::vector<U2DPoint<long> >& outList);
    U2DPoint<long> GetRandomPoint(URandom& inRandom);
    void Insert(const U2DPoint<long>& inPoint);
    void Insert(const std::vector<U2DPoint<long> >& inPoints);
    bool Remove(const U2DPoint<long>& inPoint);
    static bool SetPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks, unsigned long inChunkCount);
    long Size() const { return this->mCount; }  // exported 0x40127 (ICF)
    U2DPoint<long> operator[](long inIndex) const;

    // port (save version 0.263, the continent manager's regions in a saved game): the set as it is, its columns
    // (empty ones too: GetRandomPoint draws a column) and each column's runs in their order. W and R are the
    // save's byte writer and reader (EEAIContinentSave.cpp: U32, I32, Flag)
    template <class W> void PortWrite(W& ioWriter) const
    {
        ioWriter.I32(this->mOffset);
        ioWriter.I32(this->mCount);
        ioWriter.U32((unsigned long)this->mArray.size());
        for (size_t i = 0; i < this->mArray.size(); i++) {
            const U2DSparseArrayPointContainerListHeader* theList = this->mArray[i];
            ioWriter.Flag(theList != NULL);
            if (theList == NULL)
                continue;
            ioWriter.I32(theList->mSize);
            ioWriter.U32(theList->mList.mCount);
            for (UListNode<U2DSparseArrayPointContainerNode>* theNode = theList->mList.mHead; theNode; theNode = theNode->mNext) {
                ioWriter.I32(theNode->mData.mStart);
                ioWriter.I32(theNode->mData.mEnd);
            }
        }
    }
    template <class R> bool PortRead(R& ioReader)
    {
        this->Clear();
        unsigned long theColumns = 0;
        if (!ioReader.I32(this->mOffset) || !ioReader.I32(this->mCount) || !ioReader.U32(theColumns))
            return false;
        this->mArray.resize(theColumns, NULL);
        for (unsigned long i = 0; i < theColumns; i++) {
            bool thePresent = false;
            if (!ioReader.Flag(thePresent))
                return false;
            if (!thePresent)
                continue;
            U2DSparseArrayPointContainerListHeader* theList = new U2DSparseArrayPointContainerListHeader;
            this->mArray[i] = theList;
            unsigned long theRuns = 0;
            if (!ioReader.I32(theList->mSize) || !ioReader.U32(theRuns))
                return false;
            UListNode<U2DSparseArrayPointContainerNode>* theLast = NULL;
            for (unsigned long r = 0; r < theRuns; r++) {
                U2DSparseArrayPointContainerNode theRun;
                if (!ioReader.I32(theRun.mStart) || !ioReader.I32(theRun.mEnd))
                    return false;
                theList->mList.InsertAfter(theLast, theRun);
                theLast = theLast != NULL ? theLast->mNext : theList->mList.mHead;
            }
        }
        return true;
    }

protected:
    void ExpandArrayForPoint(const U2DPoint<long>& inPoint);
    bool GetArrayBoundaryPoints(const std::vector<U2DPoint<long> >& inPoints, U2DPoint<long>& outMinimum,
                                U2DPoint<long>& outMaximum);
    long GetRandomIntegerBetween(URandom& inRandom, long inMinimum, long inMaximum) const;
    void InsertIntoBucket(long inIndex, const U2DPoint<long>& inPoint);

    std::vector<U2DSparseArrayPointContainerListHeader*> mArray;   // +0x04 one list per column
    long mOffset;                           // +0x14 x of mArray[0]
    long mCount;                            // +0x18 point count
#ifdef LOWLEVELENGINE_EXPORTS
    static bool sPoolSizeSet;               // exported 0x1012df30
#endif
};

#endif
