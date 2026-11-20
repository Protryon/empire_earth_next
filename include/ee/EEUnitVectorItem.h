// EEUnitVectorItem (RECONSTRUCTION; file name inferred; provisional class name): the 0x28-byte polymorphic elements of
// EEComplexUnit +0x90 (`_Buy` 0x10918a multiplies by 40, the destroy loop 0x776bb steps 0x28 and calls the virtual
// dtor). Each holds a queue of goals (EEComplexUnit 0x3d416 pushes into +0x04) and a timer (0x205821). Names
// are provisional. Requires <deque>, ee/EEUnitGoal.h.
#ifndef EE_EEUNITVECTORITEM_H
#define EE_EEUNITVECTORITEM_H

#include <deque>
#include <stddef.h>
#include <stdint.h>

#include "rust/sim/units.h"

class EEUnitGoal;

class EEComplexUnit;
class FSFile;

// New (units behaviour, round 3): the goal queue is a Rust object (ee_units goals.rs EeUnitsGoalQueue: its unit, its
// goals, its timer); this is its deque's interface over it (size, the elements, read-only iterators by index, the
// edits). No writable reference or iterator leaves it; `Set` replaces one element.
class EEGoalDeque {
public:
    typedef size_t size_type;
    typedef EEUnitGoal* value_type;
    class const_iterator {
    public:
        const_iterator() : mQueue(NULL), mIndex(0) {}
        const_iterator(const EeUnitsGoalQueue* inQueue, size_t inIndex) : mQueue(inQueue), mIndex(inIndex) {}
        EEUnitGoal* operator*() const { return Goal(ee_units_gq_get(this->mQueue, this->mIndex)); }
        const_iterator& operator++() { ++this->mIndex; return *this; }
        const_iterator& operator--() { --this->mIndex; return *this; }
        const_iterator operator+(ptrdiff_t inBy) const { return const_iterator(this->mQueue, this->mIndex + inBy); }
        const_iterator operator-(ptrdiff_t inBy) const { return const_iterator(this->mQueue, this->mIndex - inBy); }
        ptrdiff_t operator-(const const_iterator& inOther) const { return (ptrdiff_t)(this->mIndex - inOther.mIndex); }
        bool operator==(const const_iterator& inOther) const { return this->mIndex == inOther.mIndex; }
        bool operator!=(const const_iterator& inOther) const { return this->mIndex != inOther.mIndex; }
        const EeUnitsGoalQueue* mQueue;
        size_t mIndex;
    };
    typedef const_iterator iterator;

    EEGoalDeque() : mQueue(NULL) {}
    EEGoalDeque(const EEGoalDeque&) = delete;
    EEGoalDeque& operator=(const EEGoalDeque&) = delete;

    static EEUnitGoal* Goal(uint64_t inGoal) { return reinterpret_cast<EEUnitGoal*>(static_cast<uintptr_t>(inGoal)); }
    static uint64_t H(const void* inObject) { return (uint64_t)(uintptr_t)inObject; }

    size_type size() const { return ee_units_gq_len(this->mQueue); }
    bool empty() const { return this->size() == 0; }
    EEUnitGoal* operator[](size_type inIndex) const { return Goal(ee_units_gq_get(this->mQueue, inIndex)); }
    EEUnitGoal* front() const { return (*this)[0]; }
    EEUnitGoal* back() const { return (*this)[this->size() - 1]; }
    const_iterator begin() const { return const_iterator(this->mQueue, 0); }
    const_iterator end() const { return const_iterator(this->mQueue, this->size()); }
    std::deque<EEUnitGoal*> Get() const
    {
        std::deque<EEUnitGoal*> theGoals;
        for (size_type i = 0; i < this->size(); i++)
            theGoals.push_back((*this)[i]);
        return theGoals;
    }

    void push_back(EEUnitGoal* inGoal) { this->Edit(0, 0, 0, inGoal); }
    void push_front(EEUnitGoal* inGoal) { this->Edit(1, 0, 0, inGoal); }
    void pop_back() { this->Edit(2, 0, 0, NULL); }
    void pop_front() { this->Edit(3, 0, 0, NULL); }
    const_iterator insert(const_iterator inAt, EEUnitGoal* inGoal) { this->Edit(4, inAt.mIndex, 0, inGoal); return inAt; }
    const_iterator erase(const_iterator inAt) { this->Edit(5, inAt.mIndex, 1, NULL); return inAt; }
    const_iterator erase(const_iterator inFirst, const_iterator inLast)
    {
        this->Edit(5, inFirst.mIndex, inLast.mIndex - inFirst.mIndex, NULL);
        return inFirst;
    }
    void clear() { this->Edit(6, 0, 0, NULL); }
    void resize(size_type inSize, EEUnitGoal* inGoal) { this->Edit(7, inSize, 0, inGoal); }
    void Set(size_type inIndex, EEUnitGoal* inGoal) { this->Edit(8, inIndex, 0, inGoal); }

    void Edit(int32_t inEdit, size_t inIndex, size_t inCount, EEUnitGoal* inGoal)
    {
        ee_units_gq_edit(this->mQueue, inEdit, inIndex, inCount, H(inGoal));
    }
    EeUnitsGoalQueue* mQueue;
};

// RTTI EEGoalQueueContainer (vtable 0x446f0c; EEGoalQueueContainer.cpp 0x20560c..0x205eda, key
// "EEGoalQueueContainer-Data"): this is that class (the provisional name EEUnitVectorItem stays until its
// users are renamed). Its members are the Rust queue's (ee_units goals.rs); the element of a unit's mGoalQueues is
// the queue's handle (the layout's complex_goal_queues reads them).
class EEUnitVectorItem {
public:
    explicit EEUnitVectorItem(EEComplexUnit* inUnit) { this->mGoals.mQueue = ee_units_gq_new(EEGoalDeque::H(inUnit)); }
    // 0x20569d: a copy of another queue: its goals cloned (last to first) for the same unit, its timer
    EEUnitVectorItem(const EEUnitVectorItem& inItem)
    {
        this->mGoals.mQueue = ee_units_gq_new(0);
        ee_units_gq_assign(this->mGoals.mQueue, inItem.mGoals.mQueue);
    }
    ~EEUnitVectorItem() { ee_units_gq_free(this->mGoals.mQueue); }   // +0x00 0x205768 (its goals deleted)
    EEUnitVectorItem& operator=(const EEUnitVectorItem& inItem)
    {
        ee_units_gq_assign(this->mGoals.mQueue, inItem.mGoals.mQueue);
        return *this;
    }
    void Clear() { ee_units_gq_clear(this->mGoals.mQueue); }   // 0x2057a7: every goal deleted, the timer stopped
    EEComplexUnit* GetUnit() const   // mUnit (+0x18: the goals' owner)
    {
        return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(ee_units_gq_unit(this->mGoals.mQueue)));
    }
    // 0x205807: the game time left on the timer (0 when it is not running)
    unsigned long GetTimeLeft() { return ee_units_gq_timer(this->mGoals.mQueue, 0, 0); }
    // 0x205821: starts the timer at the game time the first time, then adds inTime (EEMIncrementRallyTimer 60000,
    // EEMScrambleAircraft 1000)
    void AddTime(long inTime) { ee_units_gq_timer(this->mGoals.mQueue, 1, inTime); }
    void ResetTimer() { ee_units_gq_timer(this->mGoals.mQueue, 2, 0); }
    // 0x205843: inserts a goal at a position (appending past the end), the unit its owner
    void Insert(unsigned long inIndex, EEUnitGoal* inGoal) { this->GoalOp(2, inIndex, inGoal); }
    void Add(EEUnitGoal* inGoal) { this->GoalOp(0, 0, inGoal); }       // 0x1ed61b: appended, the unit its owner
    void AddFront(EEUnitGoal* inGoal) { this->GoalOp(1, 0, inGoal); }
    void PopBack() { this->GoalOp(3, 0, NULL); }                         // the last goal removed and deleted
    void PopFront() { this->GoalOp(4, 0, NULL); }                        // the first goal removed and deleted
    EEUnitGoal* GetFront() { return this->mGoals.front(); }             // 0x1f8a26 (EEUICUnitIconBar 0x25ad15)
    EEUnitGoal* GetGoalAt(unsigned long inIndex) { return this->mGoals[inIndex]; }   // 0x10815e
    // the goals (kind, then data) and the timer from and to the saved game; the first error (EEGoalQueueContainer.cpp)
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);

    void GoalOp(int32_t inOp, unsigned long inIndex, EEUnitGoal* inGoal)
    {
        ee_units_gq_goal_op(this->mGoals.mQueue, inOp, inIndex, EEGoalDeque::H(inGoal));
    }

    EEGoalDeque mGoals;                     // the Rust queue (+0x04 of the original: its goals' deque)

    static const UString sData;             // "EEGoalQueueContainer-Data" (init chain 0x20560c)
};

// the context of the queues' Rust host (EEGoalQueueContainer.cpp): the file and its version for the record, the unit
// for the per-tick update's refresh (ee_units goals.rs)
struct EEGoalQueueIO {
    FSFile* mFile;
    unsigned long mMajor;
    unsigned long mMinor;
    EEComplexUnit* mUnit;
};
void EEGoalQueueHostOf(EEGoalQueueIO& inIO, EeGoalQueueHost& outHost);

#endif
