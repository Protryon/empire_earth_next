// Low-Level Engine pooled doubly linked list template, as seen by its includers (the original's evidence: git
// history before LLE round 8). LLE round 8: the links are Rust's (ee_base util pool.rs, `ee_util_list_*`) over the
// node pool, itself Rust's; the one list the game makes holds unit pointers (EEComplexUnit::mContacts), so the data
// is a pointer.
#ifndef LLE_ULIST_H
#define LLE_ULIST_H

// Low-Level Engine fixed-size object pool template (UMemoryPool.h in the original: folded here in LLE/UI round 2),
// as seen by its includers (the layout and the original's addresses: git history before LLE round 8). LLE round 8:
// the pool is whole in Rust (ee_base util pool.rs: the chunks, the free-index stacks, the statistics, the order
// objects are handed out in); the template holds its pointer, every member forwards, and the host functions below
// are what only the C++ can do for a T.
#include <typeinfo>
#include <vector>
#include "rust/base/util.h"

template <class T> class UMemoryPool {
public:
    UMemoryPool() : mRust(ee_util_pool_new(sizeof(T), EeUtilPoolHost{&NewArray, &DeleteArray, &NewOne, &DeleteOne})) {}
    virtual ~UMemoryPool() { ee_util_pool_free(this->mRust); }

    bool SetSize(unsigned long inChunkSize, unsigned long inMaxChunks, unsigned long inChunkCount) { return ee_util_pool_set_size(this->mRust, inChunkSize, inMaxChunks, inChunkCount); }
    T* New() { return static_cast<T*>(ee_util_pool_new_object(this->mRust)); }
    void Delete(T* inObject) { ee_util_pool_delete(this->mRust, inObject); }
    void DumpStats(bool inToDebugger) { ee_util_pool_dump_stats(this->mRust, typeid(T).name(), inToDebugger); }
    bool Release() { return ee_util_pool_release(this->mRust); }
    void GetUsed(std::vector<T*>& outObjects) { ee_util_pool_get_used(this->mRust, &outObjects, &Push); }
    EeUtilPool* Rust() const { return this->mRust; }

    static void* NewArray(size_t inCount) { return new T[inCount]; }
    static void DeleteArray(void* inObjects) { delete[] static_cast<T*>(inObjects); }
    static void* NewOne() { return new T; }
    static void DeleteOne(void* inObject) { delete static_cast<T*>(inObject); }
    static void Push(void* ioList, void* inObject) { static_cast<std::vector<T*>*>(ioList)->push_back(static_cast<T*>(inObject)); }

    EeUtilPool* mRust;
};

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
