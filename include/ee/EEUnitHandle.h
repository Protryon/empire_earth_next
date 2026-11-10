// New (R5a.4, docs/RUST_PLAN.md): a unit held by its unit id instead of its address.
//
// The registry maps a unit id to its unit object from the id's assignment (RTSPlayer::InsertUnit: a new id, a loaded
// one, a kept one) until the object is freed (RTSUnit::FreeNode: the last reference, the complex units' pool, the
// destructor). Ids are never reused within a game: RTSPlayer::AddUnit counts them up (24 bits per player) and a
// converted unit is a new unit with a new id. So a handle to a freed unit resolves to NULL, never to the unit reborn
// in its pool slot (PORT_LEARNINGS "R5a.4"). Resolution does not depend on player membership: a killed unit that
// is still referenced resolves to itself, as the pointer it replaces did.
//
// The registry is only looked up, never iterated: it orders nothing and draws no random number. It is Rust
// (rust/crates/ee_units, a lock-free radix tree of atomic pointers), so a thread may resolve a handle while another
// inserts a unit or frees one: the physics thread under the world's write lock, the UI and graphics threads under
// its read lock (where a unit's last Dereference can run). A resolve gives the registered object or NULL, never a
// torn value; whether the object is still alive is the holder's business, as it was with the pointer.
//
// A holder that referenced the unit (Reference/Dereference) still does, through the handle: then the handle
// always resolves to the same object as the pointer did. A unit without an id (pending: created this tick, before
// RTSPlayer::AddPendingUnits) has no handle; storing one is reported once on stderr ("EE-HANDLE").
//
// A container of handles keeps a vector's or deque's order (the elements are what they were, resolved on each
// read); a container keyed by the unit (std::map<EEUnit*, ...>) keyed by handles orders by id instead of by
// address (EEUnitHandleLess), which is only the same for a container nothing iterates.
#ifndef EE_EEUNITHANDLE_H
#define EE_EEUNITHANDLE_H

#include <stdint.h>
#include <stddef.h>

class RTSUnit;
class EEUnit;
class EEComplexUnit;

// The registry (RTSUnit.cpp over ee_units)
RTSUnit* RTSResolveUnitID(uint32_t inUnitID);  // NULL for none (0xffffffff) or a freed unit
void RTSRegisterUnit(RTSUnit* inUnit);         // at RTSPlayer::InsertUnit (its id is final there)
void RTSUnregisterUnit(RTSUnit* inUnit);       // at RTSUnit::FreeNode (only if the id is still this unit's)

// Typed access for the handle (RTSUnit.cpp: the casts need the complete classes)
uint32_t EEUnitHandleID(const RTSUnit* inUnit);
uint32_t EEUnitHandleID(const EEUnit* inUnit);
uint32_t EEUnitHandleID(const EEComplexUnit* inUnit);
void EEUnitHandleResolve(uint32_t inUnitID, RTSUnit*& outUnit);
void EEUnitHandleResolve(uint32_t inUnitID, EEUnit*& outUnit);
void EEUnitHandleResolve(uint32_t inUnitID, EEComplexUnit*& outUnit);

// A unit of class T (RTSUnit, EEUnit or EEComplexUnit) by id. It converts to and from T*, so code written for the
// pointer reads the same; each use resolves through the registry. Saved games store the id: FromSavedID/GetSavedID
// carry it through a Load/Restore pass without resolving (the pointer fields held the raw id there).
template <class T>
class EEUnitHandle {
public:
    enum { kNone = 0xffffffff };
    EEUnitHandle() : mUnitID(kNone) {}
    EEUnitHandle(T* inUnit) : mUnitID(inUnit != NULL ? EEUnitHandleID(inUnit) : (uint32_t)kNone) {}
    EEUnitHandle& operator=(T* inUnit)
    {
        this->mUnitID = inUnit != NULL ? EEUnitHandleID(inUnit) : (uint32_t)kNone;
        return *this;
    }
    T* Get() const
    {
        T* theUnit;
        EEUnitHandleResolve(this->mUnitID, theUnit);
        return theUnit;
    }
    operator T*() const { return this->Get(); }
    T* operator->() const { return this->Get(); }

    // The saved-game swizzle: the id read from a file, kept unresolved until the Restore pass resolves it.
    static EEUnitHandle FromSavedID(uint32_t inUnitID)
    {
        EEUnitHandle theHandle;
        theHandle.mUnitID = inUnitID;
        return theHandle;
    }
    uint32_t GetSavedID() const { return this->mUnitID; }

private:
    uint32_t mUnitID;
};

// The order of a map keyed by handles: by id (a map keyed by pointers ordered by address). Only for a map nothing
// iterates, where the order is not observable.
template <class T>
struct EEUnitHandleLess {
    bool operator()(const EEUnitHandle<T>& inFirst, const EEUnitHandle<T>& inSecond) const
    {
        return inFirst.GetSavedID() < inSecond.GetSavedID();
    }
};

#endif
