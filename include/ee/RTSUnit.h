// C:\EEX\RTS Engine\Units\RTSUnit.h (RECONSTRUCTION; file name inferred). The engine-side unit (vtable 0x449b10;
// RTSUnit.cpp 0x23d7c0..0x23dd54, its asserts name mIsValid). Slot signatures from RTSUnit.cpp's bodies
// (names provisional unless stated).
// Moved out of ee/EEUnit.h (R5a.6): the RTS layer's unit without the game's unit header (EEUnit.h includes it).
// Requires lle/LowLevelEngine.h (UReferenceCounter, UString, UWideString).
#ifndef EE_RTSUNIT_H
#define EE_RTSUNIT_H

class EEPlayer;
class EEMap;
class FSFile;
class RTSTile;
class RTSUnit;

// What RTSUnit+0x08 points to: the unit's links in a second list whose head can be an RTSDummyUnit
// (0x23dae5/0x23dbda dynamic_cast the previous element to it). new'd by 0x23da54 (8 bytes, both zeroed inline).
// Name is provisional.
struct RTSUnitLink {
    RTSUnitLink() : mNext(NULL), mPrev(NULL) {}
    RTSUnit* mNext;                         // +0x00
    RTSUnit* mPrev;                         // +0x04 (an RTSUnit, or the list's RTSDummyUnit head)
};

class RTSUnit : public UReferenceCounter {
public:
    // inline (COMDAT 0x1db4 in EEACalamity.cpp's range, after its first user: count above 1 ->
    // UReferenceCounter::Dereference, else the virtual at +0x34); every caller still calls it out of line
    void Dereference() { if (this->GetReferenceCount() > 1) UReferenceCounter::Dereference(); else this->LastDereference(); }

    virtual ~RTSUnit();                     // +0x00 (??_G 0x23d8ed; dtor 0x23d909: asserts !mIsValid)
    virtual void Invalidate();              // +0x04 0x23d93f (asserts mIsValid; unlinks; mIsValid = false)
    // +0x08 (the shared empty `ret 4` 0x157269): copies from inUnit (EEUnit::CopyFrom 0x228ef5 calls it last with
    // its argument; provisional name)
    virtual void CopyFrom(RTSUnit* inUnit);
    virtual void SightOnHere() = 0;         // +0x0c
    virtual void Describe(UWideString& ioString);   // +0x10 0x23d9a8 ("uid %d, at %d,%d:")
    virtual void DescribeLong(UWideString& ioString);  // +0x14 0x23d9f6 ("UnitID:%d\nTile:(%d,%d)\n")
    virtual unsigned long HashValue();      // +0x18 0x23da44 ((id << 4 + y) << 4 + x)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x1c 0x23db4f
    virtual void Clear();                   // +0x20 0x23dd46 (Reset, mIsValid = false)
    virtual void SightOffHere() = 0;        // +0x24
    virtual long RepairPointers(unsigned long, unsigned long);  // +0x28
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x2c 0x23dc3a
    virtual void FreeNode();                // +0x30 0x23d977 (asserts !mIsValid; deletes the link)
    virtual void LastDereference();         // +0x34 0x23db49 (RTSUnit::Dereference calls it at the last reference)

    // 0x23d8cc (out of line): the count = 1 (UReferenceCounter's inline ctor), the vptr, Reset(), then +0x2b = 0,
    // +0x2a = 1
    RTSUnit();
    void Initialize(long inLinked);         // 0x23da54: Reset, mIsValid, a new RTSUnitLink unless inLinked == 0
    void Reset();                           // 0x23da8e: +0x24 = -1, +0x1c/+0x20 = -10000, the rest 0 (provisional name)
    void InsertAfter(RTSUnit* inUnit);      // 0x23dabb (the mNext/mL10 list)
    void Link(RTSUnit* inUnit);             // 0x23dae5 (the mL08 list, after inUnit or at an RTSDummyUnit head)
    void RemoveFromList();                  // 0x23dbb7 (the mNext/mL10 list)
    void Unlink();                          // 0x23dbda (the mL08 list)
    void UpdateTile(EEMap* inMap);          // 0x23dcf3: mL14 = the map's tile at (mTileX, mTileY)
    unsigned long GetUnitID() const { return this->mUnitID; }  // (EEWUnitsOffMapInfo::Save 0x161d22 reads an RTSUnit's id through it)

    // the save keys (RTSUnit.cpp's four `$E` chains; one guard byte; unguarded: a class static only
    // renumbers an includer's `$E` symbols, never its bytes)
    static const UString sUnitID;
    static const UString sTileX;
    static const UString sTileY;
    static const UString sSpecificData;

    // Members (moved here from EEUnit, RTSUnit's Reset sets them; provisional names)
    RTSUnitLink* mTileNode;                 // +0x08 (0: not a valid target; the unit calamities' Virtual4C)
    RTSUnit* GetNext() const { return this->mNext; }
    RTSUnit* mNext;                         // +0x0c the owner's next unit (EEPlayer::mFirstUnit)
    RTSUnit* mPrevious;                     // +0x10 the previous unit of that list (0x23dabb)
    RTSTile* mTile;                         // +0x14 its tile (0x23dcf3; 0: not selectable by the hot keys; provisional name)
    EEPlayer* mPlayer;                      // +0x18 (its owner)
    long mTileX;                            // +0x1c provisional name (the cast-calamity goal's x; = -10000)
    long mTileY;                            // +0x20 provisional name (= -10000)
    unsigned long mUnitID;                  // +0x24 (EECalamity keeps its target's and caster's; -1 for none)
    bool mIsQueuedForDelete;                // +0x28 (saved)
    bool mIsEditorSelected;                 // +0x29 (saved as false; Load clears it)
    bool mIsNodeFreed;                      // +0x2a (= true after the ctor)
    bool GetIsValid() const { return this->mIsValid; }
    bool mIsValid;                          // +0x2b (named by the asserts "this->mIsValid == true")
};

#endif
