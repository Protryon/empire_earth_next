// New (port): the unit store, stage one (docs/RUST_PLAN.md R5b.4 leaf 5). Each unit's plain state is a Rust record
// (rust/crates/ee_units/src/store.rs), readable by id without a call into C++, while the unit is in a player's list:
// RTSPlayer::InsertUnit inserts it (EEUnitStoreInsert: seeded whole from the object), RTSPlayer::RemoveKilledUnits
// removes it, ~RTSPlayer drops the player's records.
//
// The C++ members it mirrors are write-through holders: EEUnitStoreField<T, F> has T's size and layout (the offsets,
// a memcpy and the save code's bytes are unchanged), reads as T, and every assignment form stores the value and then
// reports it (EEUnitStoreWrote: the record of a unit in the store, the object checked). A holder has no conversion to
// T& or T*, so the compiler finds every writer; a memcpy into one bypasses it (the hit points' load: a hooked write
// follows, and the load runs before the insert). Derived fields are reported where they change: the class's category
// and type at mUnitClass, the goal's kind at mGoalState, the complex flag at mComplexUnit.
//
// TEST ONLY: EEUnitStoreCheck compares every unit of every player's list with its record (the simulation capture's
// USCK record; a mismatch stops EE-SIM, exit code 3).
#ifndef EE_EEUNITSTORE_H
#define EE_EEUNITSTORE_H

#include <stddef.h>
#include <stdint.h>

class RTSUnit;
class RTSPlayer;

// the fields a holder reports (store.rs EE_UNIT_FIELD_*: the same values)
enum EEUnitStoreFieldID {
    kUnitFieldTileX = 1,        // RTSUnit::mTileX
    kUnitFieldTileY = 2,        // RTSUnit::mTileY
    kUnitFieldValid = 3,        // RTSUnit::mIsValid
    kUnitFieldHitPoints = 4,    // EEUnit::mHitPoints
    kUnitFieldUnitClass = 5,    // EEUnit::mUnitClass (the record's category and unit type)
    kUnitFieldComplex = 7,      // EEUnit::mComplexUnit
    kUnitFieldBuilt = 8,        // EEComplexUnit::mIsBuilt
    kUnitFieldGoal = 9          // EEComplexUnit::mGoalState (the record's goal kind)
};

// a holder's member was written (EEUnitStore.cpp: the owner from the member's offset)
void EEUnitStoreWrote(unsigned int inField, const void* inMember);

// a member the unit store mirrors: T's size and layout, read as T, every write reported
template <class T, unsigned int F>
class EEUnitStoreField {
public:
    typedef T ValueType;
    EEUnitStoreField() {}   // as the plain member: the constructors write it
    // not copied: a holder lives only inside its unit (a copy's write would report from outside one)
    EEUnitStoreField(const EEUnitStoreField& inOther) = delete;

    operator T() const { return this->mValue; }
    T Get() const { return this->mValue; }
    T operator->() const { return this->mValue; }   // a pointer member's
    // a pointer member's static_cast or C-style cast to a derived class (dynamic_cast takes Get())
    template <class U> explicit operator U*() const { return static_cast<U*>(this->mValue); }

    EEUnitStoreField& operator=(T inValue)
    {
        this->mValue = inValue;
        EEUnitStoreWrote(F, this);
        return *this;
    }
    EEUnitStoreField& operator=(const EEUnitStoreField& inOther) { return *this = inOther.mValue; }
    EEUnitStoreField& operator+=(T inValue) { return *this = (T)(this->mValue + inValue); }
    EEUnitStoreField& operator-=(T inValue) { return *this = (T)(this->mValue - inValue); }
    EEUnitStoreField& operator++() { return *this = (T)(this->mValue + 1); }
    EEUnitStoreField& operator--() { return *this = (T)(this->mValue - 1); }
    T operator++(int) { T theOld = this->mValue; *this = (T)(theOld + 1); return theOld; }
    T operator--(int) { T theOld = this->mValue; *this = (T)(theOld - 1); return theOld; }

private:
    T mValue;
};

// a load's read into a holder: read as the plain value, then written through the holder (reported)
template <class File, class Field>
long EEUnitStoreRead(File& inFile, Field& ioField)
{
    typename Field::ValueType theValue = ioField.Get();
    long theResult = inFile.Read(theValue);
    ioField = theValue;
    return theResult;
}

// the unit enters inPlayer's list (its id inOwner; after the unit's mPlayer and id are final) / leaves it (also at
// RTSUnit::FreeNode, a no-op unless a teardown dropped the list's reference first)
void EEUnitStoreInsert(RTSUnit* inUnit, unsigned long inOwner, const RTSPlayer* inPlayer);
void EEUnitStoreRemove(RTSUnit* inUnit);
// a player object goes (~RTSPlayer): the records of its list go, by the object (temporary players may share an id)
void EEUnitStoreClearPlayer(const RTSPlayer* inPlayer);

class EEPlayer;
// the box selection's pick (EEPlayer::SelectUnits): the ids of inIDs (inCount) the selection takes, in order (an
// invalid unit at each of its ids: the group's Add skips it); outIDs has room for inCount; the count
size_t EESelectPick(EEPlayer* inPlayer, const uint32_t* inIDs, size_t inCount, uint32_t* outIDs);
// TEST ONLY: a USPK record of one pick (inCase 100: the game's EEPlayer::SelectUnits; 1..4: the sweep's)
void EESelectPickCapture(unsigned long inCase, EEPlayer* inPlayer, const uint32_t* inIDs, size_t inCount,
                         const uint32_t* inPicked, size_t inPickedCount);
// TEST ONLY: the pick over every player's units (USPK records; EE-SIM, every five game minutes)
void EESelectPickSweep();

// TEST ONLY: every unit of every player's list against its record; the count compared. inWhere names the point
// (0: after a physics tick, else the executed message's type). A mismatch prints the unit and field and exits 3.
unsigned long EEUnitStoreCheck(unsigned long inWhere);

#endif
