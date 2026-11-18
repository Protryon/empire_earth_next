// New (port): read-only views of the continent cluster's lists that Rust holds (ee_sim::astar continent.rs, cm.rs).
// EEAIListView is a view of plain values (valid until that list next changes; it converts to a std::vector copy);
// EEAIHandleView a view of object addresses held as 64-bit handles (each entry read as the pointer it holds).
#ifndef EE_EEAILISTVIEW_H
#define EE_EEAILISTVIEW_H

#include <stddef.h>
#include <stdint.h>
#include <vector>

template <class T> class EEAIListView {
public:
    EEAIListView(const T* inData, size_t inSize) : mData(inData), mSize(inSize) {}
    const T* begin() const { return this->mData; }
    const T* end() const { return this->mData + this->mSize; }
    size_t size() const { return this->mSize; }
    bool empty() const { return this->mSize == 0; }
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
    Iterator begin() const { return Iterator(this->mData); }
    Iterator end() const { return Iterator(this->mData + this->mSize); }
    size_t size() const { return this->mSize; }
    bool empty() const { return this->mSize == 0; }
    T* operator[](size_t inIndex) const { return (T*)(uintptr_t)this->mData[inIndex]; }

private:
    const uint64_t* mData;
    size_t mSize;
};

#endif
