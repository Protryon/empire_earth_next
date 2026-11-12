// New (port): the unit store, stage one (docs/RUST_PLAN.md R5b.4 leaf 5). Each unit's plain state is a Rust record
// (rust/crates/ee_units/src/store.rs), readable by id without a call into C++, while the unit is in a player's list:
// RTSPlayer::InsertUnit inserts it (EEUnitStoreInsert: seeded whole from the object), RTSPlayer::RemoveKilledUnits
// removes it, ~RTSPlayer drops the player's records.
//
// The C++ members it mirrors are write-through holders: EEUnitStoreField<T, F> has T's size and layout (the offsets,
// a memcpy and the save code's bytes are unchanged), reads as T, and every assignment form stores the value and then
// reports it (EEUnitStoreWrote: the record of a unit in the store, the object checked). A holder has no conversion to
// T& or T*, so the compiler finds every writer; a memcpy into one bypasses it (the hit points' load: a hooked write
// follows, and the load runs before the insert). Derived fields are reported where they change: the class's category,
// type, domains and land-building flag at mUnitClass, the goal's kind at mGoalState, the complex flag at mComplexUnit.
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
    kUnitFieldGoal = 9,         // EEComplexUnit::mGoalState (the record's goal kind)
    kUnitFieldCommandable = 12, // EEComplexUnit::mIsCommandable
    kUnitFieldFormationSlot = 13,  // EEComplexUnit::mFormationSlot
    kUnitFieldResourceAmount = 15, // EEUnit::mResourceAmount
    kUnitFieldLocation = 17,    // EEComplexUnit::mPhysicalModel's location (EEUnitStoreModel; the record's x, y, z)
    kUnitFieldHeight = 20,      // EEComplexUnit::mHeight
    kUnitFieldTimesUpdated = 21,   // EEComplexUnit::mTimesUpdated
    kUnitFieldActiveGroup = 22, // EEComplexUnit::mActiveGroup (the record's: set or not)
    kUnitFieldCommand = 23,     // EEComplexUnit::mCommand (the record's: its mType, 0 for none)
    // the step's timers and power (R5b.4 leaf 6; the step's leaves read them from the record: ee_units step.rs)
    kUnitFieldReloadTimer = 24,         // EEComplexUnit::mReloadTimer
    kUnitFieldTargetSearchTimer = 25,   // EEComplexUnit::mTargetSearchTimer
    kUnitFieldHealSearchTimer = 26,     // EEComplexUnit::mHealSearchTimer (float bits)
    kUnitFieldNextTriggerTime = 27,     // EEComplexUnit::mNextTriggerTime
    kUnitFieldRottingAwayTime = 28,     // EEComplexUnit::mRottingAwayTime
    kUnitFieldPower = 29,               // EEComplexUnit::mPower
    kUnitFieldPowerRegenTimer = 30,     // EEComplexUnit::mPowerRegenTimer
    kUnitFieldCamouflageTimer = 31      // EEComplexUnit::mCamouflageTimer
};
// reported with kUnitFieldUnitClass (store.rs EE_UNIT_FIELD_DOMAINS, EE_UNIT_FIELD_LAND_BUILDING,
// EE_UNIT_FIELD_CAN_ATTACK_GROUND): the class's mDomains, IsLandBuilding() and mCanAttackGround, fixed by its constructor

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
    EEUnitStoreField& operator|=(T inValue) { return *this = (T)(this->mValue | inValue); }
    EEUnitStoreField& operator&=(T inValue) { return *this = (T)(this->mValue & inValue); }
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
// the unit was linked into or unlinked from the world's unit chain (RTSUnit::Link, RTSUnit::Unlink): the record's
// on-the-map flag (store.rs EE_UNIT_FIELD_ON_MAP), the domain of the out-of-sync checksum's unit walk
void EEUnitStoreLinked(RTSUnit* inUnit);

// the units stepped every tick (EEWorld::mUpdateUnits; ee_units order.rs, the same ids in the same order): each change
// of the deque reported where it is made (UpdateActiveUnits, RebuildActiveUnits, Clear); EEUnitOrderReset copies the
// deque whole (after a load's reorder, EEPhysics::Tick's after-load fixups). USCK compares the two.
void EEUnitOrderPush(unsigned long inID);
void EEUnitOrderErase(unsigned long inID);
void EEUnitOrderClear();
void EEUnitOrderReset();

class EEPlayer;
// the box selection's pick (EEPlayer::SelectUnits): the ids of inIDs (inCount) the selection takes, in order (an
// invalid unit at each of its ids: the group's Add skips it); outIDs has room for inCount; the count
size_t EESelectPick(EEPlayer* inPlayer, const uint32_t* inIDs, size_t inCount, uint32_t* outIDs);
// TEST ONLY: a USPK record of one pick (inCase 100: the game's EEPlayer::SelectUnits; 1..4: the sweep's)
void EESelectPickCapture(unsigned long inCase, EEPlayer* inPlayer, const uint32_t* inIDs, size_t inCount,
                         const uint32_t* inPicked, size_t inPickedCount);
// TEST ONLY: the pick over every player's units (USPK records; EE-SIM, every five game minutes)
void EESelectPickSweep();
class EEComplexUnit;
// EEActiveGroup::Fill's test: the indices (in order) of the units of inUnits (a selection's, inCount) that an active
// group takes for formation slot inSlot: valid, with hit points, in the slot; outIndices has room for inCount; the count
size_t EEFillMembers(EEComplexUnit* const* inUnits, size_t inCount, long inSlot, uint32_t* outIndices);
// TEST ONLY: a USFL record of one Fill test (inCase 100: EEActiveGroup::Fill; 1, 2: the sweep's), inPlayer the
// selection's player
void EEFillCapture(unsigned long inCase, EEPlayer* inPlayer, EEComplexUnit* const* inUnits, size_t inCount, long inSlot,
                   const uint32_t* inIndices, size_t inTaken);
// TEST ONLY: the Fill test over every player's units for every slot (USFL records; EE-SIM, every five game minutes)
void EEFillSweep();

// TEST ONLY: the step leaves' record (USTM, R5b.4 leaf 6; ee_units step.rs): EEUnitStepNote adds one call of a leaf
// (its kind, the unit's id, its inputs and outputs as 32-bit words) to the tick's digest; EEUnitStepCommit (the end of
// EEPhysics::UpdateUnits) writes the tick's record: the tick, the calls, their FNV-1a. Words: reload / target search
// [timer, tick ms, written, timer after]; heal search the same with float bits (the tick time, not ms); power
// [regeneration, maximum, cinematic, land building, built, power, timer, tick ms, interval, written, power set, power
// after, timer after]. Nothing is noted unless the capture runs.
enum EEUnitStepKind {
    kUnitStepReload = 1,        // EEComplexUnit::UpdateReloadTimer
    kUnitStepTargetSearch = 2,  // EEComplexUnit::UpdateTargetSearchTimer
    kUnitStepHealSearch = 3,    // EEComplexUnit::UpdateHealSearchTimer
    kUnitStepPower = 4          // EEUnitClass::RegeneratePower
};
void EEUnitStepNote(unsigned int inKind, unsigned long inID, const uint32_t* inWords, unsigned int inCount);
void EEUnitStepCommit();

// TEST ONLY: every unit of every player's list against its record, the world's unit chain against the records'
// on-the-map flags, the out-of-sync checksum's flags 1 and 2 over the chain against the store's, and the stepped units'
// deque against the Rust list (EEUnitOrder*); the count compared.
// inWhere names the point (0: after a physics tick, else the executed message's type). A mismatch prints the unit and
// field and exits 3.
unsigned long EEUnitStoreCheck(unsigned long inWhere);

#endif
