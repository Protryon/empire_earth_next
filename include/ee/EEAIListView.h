// New (port): read-only views of the continent cluster's lists that Rust holds (ee_sim::astar continent.rs, cm.rs).
// EEAIListView is a view of plain values (valid until that list next changes; it converts to a std::vector copy);
// EEAIHandleView a view of object addresses held as 64-bit handles (each entry read as the pointer it holds).
#ifndef EE_EEAILISTVIEW_H
#define EE_EEAILISTVIEW_H

#include <stddef.h>
#include <stdint.h>
#include <vector>
#include "lle/U2DPoint.h"

template <class T> class EEAIListView {
public:
    EEAIListView(const T* inData, size_t inSize) : mData(inData), mSize(inSize) {}
    // the list a Rust accessor answers: inGet(inArgs..., &count) -> its data (any element type of T's layout)
    template <class F, class... A> EEAIListView(F inGet, A... inArgs) : mSize(0) { this->mData = reinterpret_cast<const T*>(inGet(inArgs..., &this->mSize)); }
    const T* begin() const { return this->mData; }
    size_t size() const { return this->mSize; }
    const T& operator[](size_t inIndex) const { return this->mData[inIndex]; }
    operator std::vector<T>() const { return std::vector<T>(this->begin(), this->end()); }

private:
    const T* mData;
    size_t mSize;
};

template <class T> class EEAIHandleView {
public:
    class Iterator {
    public:
        explicit Iterator(const uint64_t* inAt) : mAt(inAt) {}
        T* operator*() const { return (T*)(uintptr_t)*this->mAt; }
        Iterator& operator++() { ++this->mAt; return *this; }
        bool operator!=(const Iterator& inOther) const { return this->mAt != inOther.mAt; }
        bool operator==(const Iterator& inOther) const { return this->mAt == inOther.mAt; }

    private:
        const uint64_t* mAt;
    };
    EEAIHandleView(const uint64_t* inData, size_t inSize) : mData(inData), mSize(inSize) {}
    template <class F, class... A> EEAIHandleView(F inGet, A... inArgs) : mSize(0) { this->mData = inGet(inArgs..., &this->mSize); }
    size_t size() const { return this->mSize; }
    T* operator[](size_t inIndex) const { return (T*)(uintptr_t)this->mData[inIndex]; }

private:
    const uint64_t* mData;
    size_t mSize;
};

// the C++ lists the Rust fillers write (one call each)
template <class V> void EEAIClearList(void* inList) { static_cast<V*>(inList)->clear(); }
inline void EEAIPushPoint(void* inList, long inX, long inY) { static_cast<std::vector<U2DPoint<long> >*>(inList)->push_back(U2DPoint<long>(inX, inY)); }
inline void EEAIResizeIDs(void* inList, size_t inSize) { static_cast<std::vector<unsigned short>*>(inList)->resize(inSize); }
inline uint16_t* EEAIIDsData(void* inList) { return static_cast<std::vector<unsigned short>*>(inList)->data(); }

#endif
