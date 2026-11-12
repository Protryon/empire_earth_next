// EEUnitVectorItem (RECONSTRUCTION; file name inferred; provisional class name): the 0x28-byte polymorphic elements of
// EEComplexUnit +0x90 (`_Buy` 0x10918a multiplies by 40, the destroy loop 0x776bb steps 0x28 and calls the virtual
// dtor). Each holds a queue of goals (EEComplexUnit 0x3d416 pushes into +0x04) and a timer (0x205821). Names
// are provisional. Requires <deque>, ee/EEUnitGoal.h.
#ifndef EE_EEUNITVECTORITEM_H
#define EE_EEUNITVECTORITEM_H

#include <deque>

class EEUnitGoal;

class EEComplexUnit;
class FSFile;

// New (R5b.4, the fifteenth round): the goal queue's deque as a write-through holder. It reads as the deque
// (size, the elements, const iterators) and every change reports the queue's goal ids to the Rust mirror
// (ee_units goals.rs) through EEGoalQueueChanged (EEUnitStore.cpp; the owner by offset). No writable reference or
// iterator leaves it, so the compiler finds every writer; `Set` replaces one element.
class EEGoalDeque {
public:
    typedef std::deque<EEUnitGoal*> Base;
    typedef Base::const_iterator const_iterator;
    typedef Base::const_iterator iterator;   // (read only: DeleteVectorContents' loop)
    typedef Base::size_type size_type;
    typedef EEUnitGoal* value_type;

    EEGoalDeque() {}
    EEGoalDeque(const EEGoalDeque&) = delete;
    EEGoalDeque& operator=(const EEGoalDeque&) = delete;

    size_type size() const { return this->mGoals.size(); }
    bool empty() const { return this->mGoals.empty(); }
    EEUnitGoal* operator[](size_type inIndex) const { return this->mGoals[inIndex]; }
    EEUnitGoal* front() const { return this->mGoals.front(); }
    EEUnitGoal* back() const { return this->mGoals.back(); }
    const_iterator begin() const { return this->mGoals.begin(); }
    const_iterator end() const { return this->mGoals.end(); }
    const Base& Get() const { return this->mGoals; }

    void push_back(EEUnitGoal* inGoal) { this->mGoals.push_back(inGoal); this->Changed(); }
    void push_front(EEUnitGoal* inGoal) { this->mGoals.push_front(inGoal); this->Changed(); }
    void pop_back() { this->mGoals.pop_back(); this->Changed(); }
    void pop_front() { this->mGoals.pop_front(); this->Changed(); }
    const_iterator insert(const_iterator inAt, EEUnitGoal* inGoal)
    {
        const_iterator theResult = this->mGoals.insert(inAt, inGoal);
        this->Changed();
        return theResult;
    }
    const_iterator erase(const_iterator inAt)
    {
        const_iterator theResult = this->mGoals.erase(inAt);
        this->Changed();
        return theResult;
    }
    const_iterator erase(const_iterator inFirst, const_iterator inLast)
    {
        const_iterator theResult = this->mGoals.erase(inFirst, inLast);
        this->Changed();
        return theResult;
    }
    void clear() { this->mGoals.clear(); this->Changed(); }
    void resize(size_type inSize, EEUnitGoal* inGoal) { this->mGoals.resize(inSize, inGoal); this->Changed(); }
    void Set(size_type inIndex, EEUnitGoal* inGoal) { this->mGoals[inIndex] = inGoal; this->Changed(); }

private:
    void Changed();   // EEUnitStore.cpp
    Base mGoals;
};

// RTTI EEGoalQueueContainer (vtable 0x446f0c; EEGoalQueueContainer.cpp 0x20560c..0x205eda, key
// "EEGoalQueueContainer-Data"): this is that class (the provisional name EEUnitVectorItem stays until its
// users are renamed).
class EEUnitVectorItem {
public:
    EEUnitVectorItem(EEComplexUnit* inUnit);   // 0x20564f
    EEUnitVectorItem(const EEUnitVectorItem& inItem);   // 0x20569d (copies every goal through its Virtual10)
    virtual ~EEUnitVectorItem();            // +0x00 0x205768 (??_G 0x205681)
    EEUnitVectorItem& operator=(const EEUnitVectorItem& inItem);   // 0x205ab9
    // 0x205780: drops the goals once the timer has run out (provisional name)
    void Update();
    void Clear();                           // 0x2057a7: every goal's Virtual68, then deletes them (provisional name)
    bool IsShiftMode();                     // 0x2057fa: the unit's player's mShiftMode (provisional name)
    unsigned long GetTimeLeft();            // 0x205807 (0 when not started) (provisional name)
    // 0x205821: starts the timer at gPhysics's +0xd0 the first time, then adds inTime (EEMIncrementRallyTimer 60000,
    // EEMScrambleAircraft 1000)
    void AddTime(long inTime);
    void Insert(unsigned long inIndex, EEUnitGoal* inGoal);   // 0x205843 (at the end past the size) (provisional name)
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x20588b
    void PopBack();                         // 0x205960 (provisional name)
    void PopFront();                        // 0x205999 (provisional name)
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // 0x2059ea
    void ResetTimer();                      // 0x205aad (provisional name)
    EEUnitGoal* GetFront();                 // 0x1f8a26: mGoals.front() (out of line; EEUICUnitIconBar 0x25ad15) (provisional name)
    EEUnitGoal* GetGoalAt(unsigned long inIndex);  // 0x10815e: mGoals[inIndex] (out of line; EEComplexUnit::ClearGoalQueue) (provisional name)
    // 0x1ed61b: mGoals.push_back(inGoal), then inGoal->Virtual3C(mUnit) (defined in EEActiveGroup.cpp right after its
    // user LayOutFormation) (provisional name)
    void Add(EEUnitGoal* inGoal);
    void AddFront(EEUnitGoal* inGoal);

    EEGoalDeque mGoals;                     // +0x04 (its size at +0x14: EEMIncrementRallyTimer tests it; a holder: R5b.4)
    EEComplexUnit* mUnit;                   // +0x18 (the goals' Virtual3C/Virtual68 argument)
    unsigned long mStartTime;               // +0x1c (FSFile Read/Write(unsigned long))
    unsigned long mTime;                    // +0x20
    bool mStarted;                          // +0x24

    static const UString sData;             // "EEGoalQueueContainer-Data" (init chain 0x20560c)
};

#endif
