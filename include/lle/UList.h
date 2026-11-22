// Low-Level Engine pooled doubly linked list template, as seen by its includers (the original's evidence: git
// history before LLE round 8). LLE round 8: the links are Rust's (ee_base util pool.rs, `ee_util_list_*`) over the
// node pool, itself Rust's; the one list the game makes holds unit pointers (EEComplexUnit::mContacts), so the data
// is a pointer.
#ifndef LLE_ULIST_H
#define LLE_ULIST_H

#include "lle/UMemoryPool.h"

template <class T> struct UListNode {
    UListNode() : mPrev(NULL), mNext(NULL) {}

    T mData;
    UListNode* mPrev;
    UListNode* mNext;
};

template <class T> class UList {
public:
    UList() : mHead(NULL), mCount(0) {}
    ~UList() { this->Clear(); }

    void InsertAfter(UListNode<T>* inPrevious, const T& inData) { ee_util_list_insert_after(this->Rust(), sNodePool.Rust(), reinterpret_cast<EeUtilListNode*>(inPrevious), (void*)inData); }
    void Remove(UListNode<T>* inNode) { ee_util_list_remove(this->Rust(), sNodePool.Rust(), reinterpret_cast<EeUtilListNode*>(inNode)); }
    void Clear() { ee_util_list_clear(this->Rust(), sNodePool.Rust()); }
    void AddHead(const T& inData) { ee_util_list_add_head(this->Rust(), sNodePool.Rust(), (void*)inData); }
    UListNode<T>* Find(T inData) { return reinterpret_cast<UListNode<T>*>(ee_util_list_find(this->Rust(), (void*)inData)); }
    EeUtilList* Rust() { return reinterpret_cast<EeUtilList*>(this); }

    UListNode<T>* mHead;
    unsigned long mCount;

    static UMemoryPool<UListNode<T> > sNodePool;   // one per T: a pointer list (UListNode<T> is EeUtilListNode)
};

template <class T> UMemoryPool<UListNode<T> > UList<T>::sNodePool;

#endif
