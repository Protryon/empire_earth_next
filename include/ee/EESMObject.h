// C:\EEX\Empire Earth\Scenario Manager\EESMObject.h (RECONSTRUCTION; file name inferred). One scenario object (RTTI
// EESMObject, a root class; vtable 0x4460a4, eight slots; ctor 0x1c5f73, dtor body 0x1c6035 behind the scalar
// deleting destructor 0x1c6019). Layout from the ctor: vptr, a
// UWideString at +0x04, a vector<long> at +0x18 (`_Buy` 0xe7d2e, `_Tidy` 0x2002c7), then longs to +0x7c and three
// bytes (sizeof 0x84). Slot signatures beyond Virtual0C are unproven. Member names by offset (provisional name).
// Requires lle/UWideString.h, <vector>. The fields +0x28..+0x7c are unsigned long (Load 0x1c8284 reads every one with
// FSFile::Read(unsigned long&), slot +0x5c; AcceptsClass compares mL2C/mL30 unsigned).
#ifndef EE_EESMOBJECT_H
#define EE_EESMOBJECT_H

#include <vector>
#include "lle/UWideString.h"
#include "ee/EESMIDMap.h"
class EEComplexUnit;
class EEUnit;

class EESMObject {
public:
    bool FindPlacement(unsigned long inUnitType, GE2DPoint& outLocation, bool inTryRandom);  // 0x1c6d4f, actual ret12/AL/float output
    EESMObject();                           // 0x1c5f73
    virtual ~EESMObject();                  // +0x00 (??_G 0x1c6019; the body 0x1c6035)
    // slot signatures from `ret N` and EEScenarioManager's broadcast hooks
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0x1c8284
    virtual void RemapAreas(EESMIDMap& inMap);  // +0x08 0x1c856e
    virtual void RenumberAreas(unsigned long inOldIndex, unsigned long inNewIndex);  // +0x0c 0x1c84db
    virtual void RemapObjects(EESMIDMap& inMap);  // +0x10 0x1c8580
    virtual void RenumberObjects(unsigned long inOldIndex, unsigned long inNewIndex);  // +0x14 0x1c8511
    virtual void ReplaceUnit(long inOldUnit, long inNewUnit);  // +0x18 0x1c85a4 (replaces a unit id in mUnits)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x1c 0x1c85de
    void Copy(const EESMObject& inObject);  // 0x1c6386 (provisional name)
    bool AcceptsClass(unsigned long inClass);   // 0x1c6051 (provisional name)
    bool IsNearUnit(EEUnit* inUnit, long inMode, bool inFromUnit);   // 0x1c6148 (`ret 0xc`; provisional name)
    bool IsNearTile(long inX, long inY, long inMode, bool inEvaluate);   // 0x1c629f (`ret 0x10`; provisional name)
    long GetUnitAttribute(unsigned long inMetric, bool inFlag, EEUnit* inUnit, long inArg4, long inArg5);  // 0x1c7749; last two slots unused/provisional
    long PickUnitType();                   // 0x1c6b14: chooses a matching unit class (provisional name)
    bool Evaluate(bool inIncludeDead);      // 0x1c6458 (the object condition: its units exist/are in the area; provisional name)
    void GetUnits(std::vector<unsigned long>& outUnits) const;
    // inline const accessor (Save 0x1c85de reads mUnits[i] through the const operator[]: lea; mov [eax]) (provisional
    // name)
    unsigned long GetUnitAt(unsigned long inIndex) const { return this->mUnits[inIndex]; }  // 0x1c8274 (`outUnits = mUnits`)
    unsigned long GetUnitCount() const { return this->mUnits.size(); }  // inline (EESMEffect 0x5bd5eb, EESMObject 0x5c6458)
    // inline value getters (EESMEffect 0x5c2b3e / 0x5bc942 read the object test through them)
    unsigned long GetOwner() const { return this->mOwner; }
    bool GetIsSelectOnMap() const { return this->mIsSelectOnMap; }
    unsigned long GetParent() const { return this->mParent; }
    unsigned long GetInAreaCheck() const { return this->mInAreaCheck; }
    unsigned long GetNearCheck() const { return this->mNearCheck; }
    unsigned long GetHasAttributeCheck() const { return this->mHasAttributeCheck; }
    unsigned long GetVisibleByCheck() const { return this->mVisibleByCheck; }
    unsigned long GetHasStateCheck() const { return this->mHasStateCheck; }
    unsigned long GetSelectedByCheck() const { return this->mSelectedByCheck; }
    unsigned long GetAttribute() const { return this->mAttribute; }
    bool GetIsAttributePercent() const { return this->mIsAttributePercent; }
    unsigned long GetFamily() const { return this->mFamily; }
    unsigned long GetClass() const { return this->mClass; }
    unsigned long GetMinCount() const { return this->mMinCount; }
    unsigned long GetMaxCount() const { return this->mMaxCount; }
    unsigned long GetArea() const { return this->mArea; }
    unsigned long GetNearObject() const { return this->mNearObject; }
    void SetUnits(const std::vector<unsigned long>& inUnits);  // 0x1c97a8
    void SetParent(long inParent);          // 0x1c96b7 (0 when it would close a cycle; provisional name)
    void SetName(const UWideString& inName);    // 0x1c9628 (also renames the conditions that use the object)

    UWideString mName;                      // +0x04 (ctor: gResource string 0x32c9)
    unsigned long GetUnitsAt(unsigned long inIndex) const { return this->mUnits[inIndex]; }
    std::vector<unsigned long> mUnits;      // +0x18 (unit ids; Load reads them as unsigned long)
    unsigned long mOwner;                   // +0x28 (ctor: 7)
    unsigned long mFamily;                  // +0x2c
    unsigned long mClass;                   // +0x30
    unsigned long mMinCount;                // +0x34 (ctor: 1)
    unsigned long mMaxCount;                // +0x38 (ctor: 1)
    unsigned long mArea;                    // +0x3c
    unsigned long mNearMode;                // +0x40
    unsigned long mNearObject;              // +0x44 (remapped/renumbered by Virtual10/Virtual14)
    unsigned long mParent;                  // +0x48 (the parent object index: SetParent)
    unsigned long mAttribute;               // +0x4c
    unsigned long mAttributeMin;            // +0x50
    unsigned long mAttributeMax;            // +0x54
    unsigned long mVisibleByPlayer;         // +0x58
    unsigned long mState;                   // +0x5c
    unsigned long mSelectedByPlayer;        // +0x60 (ctor: 7)
    unsigned long mL64;                     // +0x64
    unsigned long mInAreaCheck;             // +0x68
    unsigned long mNearCheck;               // +0x6c
    unsigned long mHasAttributeCheck;       // +0x70
    unsigned long mVisibleByCheck;          // +0x74
    unsigned long mHasStateCheck;           // +0x78
    unsigned long mSelectedByCheck;         // +0x7c
    bool mIsSelectOnMap;                    // +0x80 (ctor: 1)
    bool mIsDynamic;                        // +0x81 (ctor: 1)
    bool mIsAttributePercent;               // +0x82
};

#endif
