// C:\EEX\RTS Engine\Units\RTSUnit.h (RECONSTRUCTION; file name inferred). The engine-side unit (vtable 0x449b10;
// RTSUnit.cpp 0x23d7c0..0x23dd54, its asserts name mIsValid). Slot signatures from RTSUnit.cpp's bodies
// (names provisional unless stated).
// Moved out of ee/EEUnit.h (R5a.6): the RTS layer's unit without the game's unit header (EEUnit.h includes it).
// Requires lle/LowLevelEngine.h (UReferenceCounter, UString, UWideString).
#ifndef EE_RTSUNIT_H
#define EE_RTSUNIT_H

#include "ee/EEUnitStore.h"   // the members the unit store mirrors (EEUnitStoreField)
#include "rust/sim/records.h"
#include "rust/game/rts.h"

class EEPlayer;
class EEMap;
class FSFile;
class RTSTile;
class RTSUnit;
// a unit's Load (inLoad) or Save over its parts' records (inDepth 0: RTSUnit's, 1: with EEUnit's, 2: with
// EEComplexUnit's; EEComplexDoppelganger.cpp)
long EEUnitFile(bool inLoad, RTSUnit* inUnit, int inDepth, unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& ioFile);

// What RTSUnit+0x08 points to: the unit's links in a second list whose head can be an RTSDummyUnit
// (0x23dae5/0x23dbda dynamic_cast the previous element to it). new'd by 0x23da54 (8 bytes, both zeroed inline).
// Name is provisional.
struct RTSUnitLink {
    RTSUnitLink() : mNext(NULL), mPrev(NULL) {}
    RTSUnit* mNext;                         // +0x00
    RTSUnit* mPrev;                         // +0x04 (an RTSUnit, or the list's RTSDummyUnit head)
};

// New (port): a unit object's C++ class as data (ee/EEUnitKind.h, docs/RUST_PLAN.md R5b.4; the values are
// ee_units' EE_OBJECT_*): each constructor sets its class's value first thing and each destructor its own on entry,
// as the vptr is. Not copyable: a copy would carry the other object's class.
struct RTSUnitObjectKind {
    RTSUnitObjectKind() : mValue(0) {}
    unsigned char mValue;

private:
    RTSUnitObjectKind(const RTSUnitObjectKind&);
    RTSUnitObjectKind& operator=(const RTSUnitObjectKind&);
};

// New (R5b.4, round 26): the plain members (mTileX, mTileY, mUnitID, mIsQueuedForDelete, mIsEditorSelected,
// mIsNodeFreed, mIsValid: their names, types and order) are the Rust record EeUnitsRtsUnitFields (ee_units
// unit_fields.rs), the second base; UReferenceCounter stays the first, so RTSUnit is at offset 0 of every unit. The
// unit store reads the record directly.
class RTSUnit : public UReferenceCounter, public EeUnitsRtsUnitFields {
public:
    // inline (COMDAT 0x1db4 in EEACalamity.cpp's range, after its first user: count above 1 ->
    // UReferenceCounter::Dereference, else the virtual at +0x34); every caller still calls it out of line
    void Dereference() { if (this->GetReferenceCount() > 1) UReferenceCounter::Dereference(); else this->LastDereference(); }

    virtual ~RTSUnit();                     // +0x00 (??_G 0x23d8ed; dtor 0x23d909: asserts !mIsValid)
    // +0x04 0x23d93f (asserts mIsValid; unlinks; mIsValid = false)
    virtual void Invalidate() { ee_rts_unit_invalidate(this); }
    // +0x08 (the shared empty `ret 4` 0x157269): copies from inUnit (EEUnit::CopyFrom 0x228ef5 calls it last with
    // its argument; provisional name)
    virtual void CopyFrom(RTSUnit*) {}
    virtual void SightOnHere() = 0;         // +0x0c
    virtual void Describe(UWideString& ioString) { ioString += ee_rts_unit_describe(this, false); }  // +0x10 0x23d9a8 ("uid %d, at %d,%d:")
    virtual void DescribeLong(UWideString& ioString) { ioString += ee_rts_unit_describe(this, true); }  // +0x14 0x23d9f6 ("UnitID:%d\nTile:(%d,%d)\n")
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return EEUnitFile(true, this, 0, inMajorVersion, inMinorVersion, inFile); }  // +0x1c 0x23db4f
    // +0x20 0x23dd46 (Reset, mIsValid = false)
    virtual void Clear() { ee_rts_unit_clear(this); }
    virtual void SightOffHere() = 0;        // +0x24
    // +0x28
    virtual long RepairPointers(unsigned long, unsigned long) { return 0; }
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return EEUnitFile(false, this, 0, inMajorVersion, inMinorVersion, outFile); }  // +0x2c 0x23dc3a
    // +0x30 0x23d977 (asserts !mIsValid; deletes the link)
    virtual void FreeNode() { ee_rts_unit_free_node(this); }
    // +0x34 0x23db49 (RTSUnit::Dereference calls it at the last reference)
    virtual void LastDereference() { UReferenceCounter::Dereference(); }

    // 0x23d8cc (out of line): the count = 1 (UReferenceCounter's inline ctor), the vptr, Reset(), then +0x2b = 0,
    // +0x2a = 1
    RTSUnit() { ee_rts_unit_construct(this); }
    // 0x23da54: Reset, mIsValid, a new RTSUnitLink unless inLinked == 0
    void Initialize(long inLinked) { ee_rts_unit_initialize(this, inLinked); }
    // 0x23da8e: +0x24 = -1, +0x1c/+0x20 = -10000, the rest 0 (provisional name)
    void Reset() { ee_rts_unit_reset(this); }
    // 0x23dae5 (the mL08 list, after inUnit or at an RTSDummyUnit head)
    void Link(RTSUnit* inUnit) { ee_rts_unit_link(this, inUnit); }
    unsigned long GetUnitID() const { return this->mUnitID; }  // (EEWUnitsOffMapInfo::Save 0x161d22 reads an RTSUnit's id through it)

    // Members (moved here from EEUnit, RTSUnit's Reset sets them; provisional names)
    RTSUnitLink* mTileNode;                 // +0x08 (0: not a valid target; the unit calamities' Virtual4C)
    RTSUnit* mNext;                         // +0x0c the owner's next unit (EEPlayer::mFirstUnit)
    RTSUnit* mPrevious;                     // +0x10 the previous unit of that list (0x23dabb)
    RTSTile* mTile;                         // +0x14 its tile (0x23dcf3; 0: not selectable by the hot keys; provisional name)
    EEPlayer* mPlayer;                      // +0x18 (its owner)
    // +0x1c mTileX, +0x20 mTileY (= -10000), +0x24 mUnitID (-1 for none), +0x28 mIsQueuedForDelete (saved), +0x29
    // mIsEditorSelected (saved as false), +0x2a mIsNodeFreed, +0x2b mIsValid: EeUnitsRtsUnitFields
    RTSUnitObjectKind mObjectKind;          // New (port): RTSUnit 0, EEUnit 1, EEComplexUnit 2 (EEUnitCast<T>)
    bool mInUnitStore = false;              // New (port): its record is in the unit store (ee/EEUnitStore.h)
};

// New (R5b.4, round 26): the units' Save and Load are Rust (ee_units unit_save.rs, unit_load.rs) over the records and
// this host (EEComplexDoppelganger.cpp): the file's primitives with the overload the original called and the unit's C++
// parts by operation. mGoals: EEComplexUnit::Load's stand-in goal queue (the queues past the unit's own)
class EEUnitVectorItem;
struct EEUnitSaveContext {
    RTSUnit* mUnit;
    FSFile* mFile;
    unsigned long mMajorVersion;
    unsigned long mMinorVersion;
    EEUnitVectorItem* mGoals;
};
int32_t EEUnitSaveOperation(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsUnitSaveIo* io);

#endif
