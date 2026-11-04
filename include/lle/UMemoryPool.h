// Low-Level Engine fixed-size object pool template, as seen by its includers. Member names are provisional; layout
// proven by the mixer's UMemoryPool<UListNode<U2DSparseArrayPointContainerNode> > (ctor 0x10002363, Release
// 0x1000227d, CountUsed 0x1000232e, chunk dtor 0x100022e1) and by LLE's twins (SetSize 0x10055c0a, DumpStats
// 0x10055baa: "UMemoryPool< %s >: Stats are: Requests / Deletions / Peak usage / NormalNews / NormalDeletes /
// Leaks", New 0x10055efb, Delete 0x10055ea1, chunk ctor 0x10055dee).
//
// Definition placement is proven by codegen and COMDAT order: ctor, Release and CountUsed are out-of-class
// (called, not expanded); the virtual dtor and the chunk's CountUsed are in-class and expanded. The chunk dtor is
// in-class too, yet called out of line: VC6 never inlines a body that deletes an object of a class with a dtor
// (probes/inline_delete_array.cpp). Defined out of class instead, it is emitted before ~Chunk/??_E rather than
// after them (mixer order: Release 0x227d, ~Chunk 0x22e1, UListNode ??_E 0x22fc, CountUsed 0x232e, ctor 0x2363).
#ifndef LLE_UMEMORYPOOL_H
#define LLE_UMEMORYPOOL_H

// One block of mSize preallocated objects plus a stack of free indices. (provisional name)
template <class T> class UMemoryPoolChunk {     // proven sizeof 0x10 (LLE: operator new(0x10))
public:
    UMemoryPoolChunk(unsigned long inSize);
    ~UMemoryPoolChunk()
    {
        delete[] this->mObjects;
        delete[] this->mFreeList;
    }

    long CountUsed() const { return this->mSize - (this->mFreeTop + 1 - this->mFreeList); }
    long CountFree() const { return this->mFreeTop + 1 - this->mFreeList; }   // (provisional name)
    T* New() { this->mFreeTop--; return &this->mObjects[this->mFreeTop[1]]; }  // (provisional name)
    void Delete(T* inObject) { *++this->mFreeTop = inObject - this->mObjects; }  // (provisional name)
    void GetUsed(std::vector<T*>& outObjects);                              // exe 0x6011b3 (provisional name)

    T* mObjects;                // proven +0x00: new T[mSize] (array cookie; deleted with delete[])
    long* mFreeList;            // proven +0x04: new long[mSize], filled with the indices
    long* mFreeTop;             // proven +0x08: top of the free-index stack (&mFreeList[mSize - 1] when empty)
    unsigned long mSize;        // proven +0x0c
};


template <class T> class UMemoryPool {          // proven sizeof 0x38 (mixer .data 0x5340..0x5378)
public:
    UMemoryPool();
    virtual ~UMemoryPool() { this->Release(); }  // proven: vptr store + call Release (mixer 0x10002236)

    bool SetSize(unsigned long inChunkSize, unsigned long inMaxChunks, unsigned long inChunkCount);
    T* New();
    void Delete(T* inObject);
    void DumpStats(bool inToDebugger);
    bool Release();
    long CountUsed();
    bool AddChunk();                    // LLE 0x1005616f (provisional name)
    void GetUsed(std::vector<T*>& outObjects);   // exe 0x600a3b: EEComplexUnit's pool (provisional name)

    UMemoryPoolChunk<T>** mChunks;      // proven +0x04: new[mMaxChunks]
    UMemoryPoolChunk<T>* mCurrentChunk; // proven +0x08
    UMemoryPoolChunk<T>** mLastChunk;   // proven +0x0c: &mChunks[mChunkCount - 1]
    T** mChunkObjects;                  // proven +0x10: new[mMaxChunks], mChunkObjects[i] = mChunks[i]->mObjects
    long mNormalNews;                   // proven +0x14 ("NormalNews")
    long mNormalDeletes;                // proven +0x18 ("NormalDeletes")
    unsigned long mChunkSize;           // proven +0x1c (default 250)
    unsigned long mMaxChunks;           // proven +0x20 (default 1)
    unsigned long mChunkCount;          // proven +0x24
    long mRequests;                     // proven +0x28 ("Requests")
    long mDeletions;                    // proven +0x2c ("Deletions")
    long mPeakUsage;                    // proven +0x30 ("Peak usage")
    bool mIsSetUp;                      // proven +0x34 (set by SetSize)
};

template <class T> UMemoryPool<T>::UMemoryPool()
    : mChunks(NULL), mCurrentChunk(NULL), mLastChunk(NULL), mChunkObjects(NULL), mNormalNews(0), mNormalDeletes(0),
      mChunkSize(250), mMaxChunks(1), mChunkCount(0), mRequests(0), mDeletions(0), mPeakUsage(0), mIsSetUp(false)
{
}

// LLE 0x10055dee / 0x100561e2
template <class T> UMemoryPoolChunk<T>::UMemoryPoolChunk(unsigned long inSize)
{
    this->mSize = inSize;
    this->mObjects = new T[inSize];
    this->mFreeList = new long[inSize];
    for (long i = inSize - 1; i >= 0; i--)
        this->mFreeList[i] = i;
    this->mFreeTop = &this->mFreeList[inSize - 1];
}

// LLE 0x10055c0a / 0x1005625c. Fails (and keeps the pool) while an object is in use.
template <class T> bool UMemoryPool<T>::SetSize(unsigned long inChunkSize, unsigned long inMaxChunks,
                                                unsigned long inChunkCount)
{
    bool theResult = this->mChunks == NULL;
    if (!theResult) {
        theResult = true;
        for (unsigned long i = 0; i < this->mChunkCount && theResult == true; i++)
            theResult = this->mChunks[i]->CountUsed() == 0;
        if (theResult)
            this->Release();
    }
    if (theResult) {
        this->mChunkSize = inChunkSize;
        this->mMaxChunks = inMaxChunks;
        this->mChunkCount = inChunkCount;
        this->mChunks = new UMemoryPoolChunk<T>*[this->mMaxChunks];
        this->mChunkObjects = new T*[this->mMaxChunks];
        memset(this->mChunks, 0, this->mMaxChunks * sizeof(UMemoryPoolChunk<T>*));
        memset(this->mChunkObjects, 0, this->mMaxChunks * sizeof(T*));
        for (unsigned long i = 0; i < this->mChunkCount; i++) {
            this->mChunks[i] = new UMemoryPoolChunk<T>(this->mChunkSize);
            this->mChunkObjects[i] = this->mChunks[i]->mObjects;
        }
        this->mCurrentChunk = this->mChunks[0];
        this->mLastChunk = &this->mChunks[this->mChunkCount - 1];
    }
    this->mIsSetUp = true;
    return theResult;
}

// LLE 0x10055efb. Falls back to operator new when every chunk is full and no chunk can be added.
template <class T> T* UMemoryPool<T>::New()
{
    T* theObject = NULL;
    bool theAddChunk = this->mChunks == NULL;
    if (!theAddChunk && this->mCurrentChunk == NULL) {
        theAddChunk = true;
        for (UMemoryPoolChunk<T>** theChunk = this->mChunks; theChunk != this->mLastChunk; theChunk++) {
            if ((*theChunk)->CountFree() != 0) {
                this->mCurrentChunk = *theChunk;
                theAddChunk = false;
                break;
            }
        }
    }
    if (theAddChunk)
        this->AddChunk();
    if (this->mCurrentChunk) {
        theObject = this->mCurrentChunk->New();
        if (this->mCurrentChunk->CountFree() == 0)
            this->mCurrentChunk = NULL;
    } else {
        for (UMemoryPoolChunk<T>** theChunk = this->mChunks; theChunk != this->mLastChunk; theChunk++) {
            if ((*theChunk)->CountFree() > 0) {
                theObject = (*theChunk)->New();
                this->mCurrentChunk = (*theChunk)->CountFree() > 0 ? *theChunk : 0;
                break;
            }
        }
    }
    if (theObject == 0) {
        theObject = new T;
        this->mNormalNews++;
    }
    this->mRequests++;
    long theUsage = this->mRequests - this->mDeletions;
    if (this->mPeakUsage < theUsage)
        this->mPeakUsage = theUsage;
    return theObject;
}

// LLE 0x10055ea1. Objects that did not come from a chunk were made by operator new.
template <class T> void UMemoryPool<T>::Delete(T* inObject)
{
    for (unsigned long i = 0; i < this->mChunkCount; i++) {
        if ((unsigned long)(inObject - this->mChunkObjects[i]) < this->mChunkSize) {
            this->mCurrentChunk = this->mChunks[i];
            this->mCurrentChunk->Delete(inObject);
            this->mDeletions++;
            return;
        }
    }
    this->mNormalDeletes++;
    delete inObject;
    this->mDeletions++;
}

// STAND-IN (owner 2026-11-01, Q1): the original's else arm stores the string's (const char*) conversion into the
// argument slot of a compiled-away trace call (exe DumpStats rows, no-/Og end-of-file state of Empire Earth.cpp);
// an empty inline function taking the argument keeps that store; under /Og it emits nothing. docs/LOST_DEBUG.md.
inline void UMemoryPoolTraceStub(const char*) {}

// LLE 0x10055baa
template <class T> void UMemoryPool<T>::DumpStats(bool inToDebugger)
{
    UWideString theString;
    sprintf(theString, "!UMemoryPool< %s >: Stats are:\n\tRequests: %d\n\tDeletions: %d\n\tPeak usage: %d\n\t"
            "NormalNews: %d\n\tNormalDeletes: %d\n\tLeaks: %d\n", typeid(T).name(), this->mRequests, this->mDeletions,
            this->mPeakUsage, this->mNormalNews, this->mNormalDeletes, this->mRequests - this->mDeletions);
    if (inToDebugger)
        OutputDebugString(theString);
    else
        UMemoryPoolTraceStub(theString);
}

// LLE 0x1005616f
template <class T> bool UMemoryPool<T>::AddChunk()
{
    bool theResult = false;
    if (this->mChunkCount < this->mMaxChunks) {
        if (this->mChunks == NULL)
            this->SetSize(this->mChunkSize, this->mMaxChunks, 1);
        else {
            this->mChunks[this->mChunkCount] = new UMemoryPoolChunk<T>(this->mChunkSize);
            this->mChunkObjects[this->mChunkCount] = this->mChunks[this->mChunkCount]->mObjects;
            this->mCurrentChunk = this->mChunks[this->mChunkCount];
            this->mLastChunk = &this->mChunks[this->mChunkCount];
            this->mChunkCount++;
        }
        theResult = true;
    }
    return theResult;
}

// Frees every chunk, but only when no object is in use. (provisional name)
template <class T> bool UMemoryPool<T>::Release()
{
    bool theResult = this->CountUsed() == 0;
    if (theResult) {
        for (unsigned long i = 0; i < this->mChunkCount; i++)
            delete this->mChunks[i];
        delete[] this->mChunks;
        delete[] this->mChunkObjects;
        this->mChunks = NULL;
        this->mChunkObjects = NULL;
        this->mLastChunk = NULL;
        this->mChunkCount = 0;
        this->mCurrentChunk = NULL;
    }
    return theResult;
}

template <class T> long UMemoryPool<T>::CountUsed()
{
    long theCount = 0;
    if (this->mChunks) {
        for (long i = this->mChunkCount - 1; i >= 0; i--)
            theCount += this->mChunks[i]->CountUsed();
    }
    return theCount;
}

// Appends every object in use, last chunk first (exe 0x600a3b, EEComplexUnit's dumps). (provisional name)
// Neither the engine nor the plug-ins instantiate it, so no engine object changes.
template <class T> void UMemoryPool<T>::GetUsed(std::vector<T*>& outObjects)
{
    if (this->mChunks) {
        for (long i = this->mChunkCount - 1; i >= 0; i--)
            this->mChunks[i]->GetUsed(outObjects);
    }
}

// Appends the chunk's objects in use: marks the free indices, collects the unmarked ones from the back, then pushes
// them in ascending order (exe 0x6011b3). (provisional name)
template <class T> void UMemoryPoolChunk<T>::GetUsed(std::vector<T*>& outObjects)
{
    std::vector<long> theMarks;
    std::vector<long> theUsed;
    if (this->CountUsed() > 0) {
        theMarks.resize(this->mSize, -1);
        for (long* theFree = this->mFreeList; theFree <= this->mFreeTop; theFree++)
            theMarks[*theFree] = *theFree;
        long i; for (i = theMarks.size() - 1; i >= 0; i--) {
            if (theMarks[i] == -1)
                theUsed.push_back(i);
        }
        for (i = theUsed.size() - 1; i >= 0; i--)
            outObjects.push_back(&this->mObjects[theUsed[i]]);
    }
}

#endif
