// Low-Level Engine pooled doubly linked list template, as seen by its includers. Member names are provisional.
// Evidence: RTTI .?AU?$UListNode@...@@ (a struct), LLE's UList<U2DSparseArrayPointContainerNode> code (Remove
// 0x10055b70, Clear 0x10055fe3 = the jmp target of the exported ~U2DSparseArrayPointContainerListHeader) and the
// per-T pool statics in every includer (mixer 0x10005340).
//
// Why an includer that never uses a list still carries the pool: VC6 instantiates the template member bodies
// that an *inline member of a non-template class* references, even when that inline member is never called, and
// so marks the static data member referenced (probes/tstatic_member.cpp). U2DSparseArrayPointContainerListHeader's
// inline dtor -> ~UList -> Clear -> Remove -> sNodePool is that chain.
#ifndef LLE_ULIST_H
#define LLE_ULIST_H

#include "lle/UMemoryPool.h"

template <class T> struct UListNode {           // sizeof(T) + 8 (LLE: 0x10 for an 8-byte T)
    UListNode() : mPrev(NULL), mNext(NULL) {}

    T mData;                                    // proven +0x00
    UListNode* mPrev;                           // proven +sizeof(T)
    UListNode* mNext;                           // proven +sizeof(T)+4
};

template <class T> class UList {               // proven sizeof 8 (inside the 0xc-byte list header)
public:
    UList() : mHead(NULL), mCount(0) {}
    ~UList() { this->Clear(); }

    void InsertAfter(UListNode<T>* inPrevious, const T& inData);   // LLE 0x10055aa3 (provisional names)
    void Remove(UListNode<T>* inNode);
    void Clear();
    void AddHead(const T& inData);              // LLE 0x1001e135 (GELOD lists; provisional name, gemodel)
    UListNode<T>* Find(T inData)             // inline (GELOD's Add*/Remove* expand it; provisional name, gemodel)
    {
        UListNode<T>* theResult = NULL;
        for (UListNode<T>* theNode = this->mHead; theNode; theNode = theNode->mNext) {
            if (theNode->mData == inData) {
                theResult = theNode;
                break;
            }
        }
        return theResult;
    }

    UListNode<T>* mHead;                        // proven +0x00
    unsigned long mCount;                       // proven +0x04 (unsigned: GELOD code compares it with ja/jbe)

    static UMemoryPool<UListNode<T> > sNodePool;   // one per T (mixer 0x10005340) (provisional name)
};

template <class T> UMemoryPool<UListNode<T> > UList<T>::sNodePool;

// inPrevious == 0: insert at the head
template <class T> void UList<T>::InsertAfter(UListNode<T>* inPrevious, const T& inData)
{
    UListNode<T>* theNode = UList::sNodePool.New();
    theNode->mData = inData;
    theNode->mPrev = NULL;
    theNode->mNext = NULL;
    if (inPrevious) {
        if (inPrevious->mNext) {
            theNode->mNext = inPrevious->mNext;
            inPrevious->mNext->mPrev = theNode;
        }
        theNode->mPrev = inPrevious;
        inPrevious->mNext = theNode;
    } else {
        theNode->mNext = this->mHead;
        if (this->mHead)
            this->mHead->mPrev = theNode;
        this->mHead = theNode;
    }
    this->mCount++;
}

template <class T> void UList<T>::Remove(UListNode<T>* inNode)
{
    if (inNode->mPrev)
        inNode->mPrev->mNext = inNode->mNext;
    else
        this->mHead = inNode->mNext;
    if (inNode->mNext)
        inNode->mNext->mPrev = inNode->mPrev;
    UList::sNodePool.Delete(inNode);
    this->mCount--;
}

template <class T> void UList<T>::Clear()
{
    UListNode<T>* theNode = this->mHead;
    while (theNode) {
        UListNode<T>* theNext = theNode->mNext;
        this->Remove(theNode);
        theNode = theNext;
    }
    this->mHead = NULL;
    this->mCount = 0;
}

// LLE 0x1001e135/0x1001e169/0x1001e19d (UList<GELODVertex*>/<GELODPolygon*>/<GELODMetaVertex*>; gemodel)
template <class T> void UList<T>::AddHead(const T& inData)
{
    UListNode<T>* theNode = UList::sNodePool.New();
    theNode->mData = inData;
    theNode->mNext = NULL;
    theNode->mPrev = NULL;
    theNode->mNext = this->mHead;
    if (this->mHead)
        this->mHead->mPrev = theNode;
    this->mCount++;
    this->mHead = theNode;
}

#endif
