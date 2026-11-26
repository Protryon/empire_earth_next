// Low-Level Engine fixed-size object pool template, as seen by its includers (the layout and the original's
// addresses: git history before LLE round 8). LLE round 8: the pool is whole in Rust (ee_base util pool.rs: the
// chunks, the free-index stacks, the statistics, the order objects are handed out in); the template holds its
// pointer, every member forwards, and the host functions below are what only the C++ can do for a T.
#ifndef LLE_UMEMORYPOOL_H
#define LLE_UMEMORYPOOL_H

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

#endif
