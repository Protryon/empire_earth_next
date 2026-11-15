// EEUnitClassImprovement.h (RECONSTRUCTION from RTTI/vtables/ctors; file name inferred). Classes:
// EEUnitClassImprovement (EEUnitClassImprovement.cpp 0x23a5bf..0x23b61a).
// Layout gap-free from the ctor 0x23a5bf (`rep stosd` of 16 ones from +0x04, +0x44 = 0) and the dtor 0x23a5f7 (the
// vptr store only: a user-declared empty dtor); sizeof 0x48 (EEUnitClassUpgrade's next member is at +0x4c of the
// object it is embedded in at +0x04). Names are provisional.
#ifndef EE_EEUNITCLASSIMPROVEMENT_H
#define EE_EEUNITCLASSIMPROVEMENT_H

#include <stdint.h>   // int32_t: the levels are saved as 16 raw 4-byte words

// vtable 0x449acc; bases EEUnitClassImprovement; sizeof 0x48; ctor 0x23a5bf; ??_G 0x23a5db; dtor 0x23a5f7
class EEUnitClass;
class EEUnitClassUpgrade;
class FSFile;
class UWideString;

class EEUnitClassImprovement {
public:
    EEUnitClassImprovement();               // 0x23a5bf: every mLevels = 1, mL44 = 0
    virtual ~EEUnitClassImprovement();      // +0x00 dtor 0x23a5f7

    long GetLevel(long inIndex);            // 0x23a89d: mLevels[inIndex] (out of line)
    // 0x23a5fe: whether inImprovement can still be raised for inUnitClass (EEUIBUpgrade, the scenario editor's
    // selected unit; ret 8, tested `== 1`) (provisional name)
    bool CanUpgrade(EEUnitClass* inUnitClass, long inImprovement);
    long GetCost(long inImprovement);       // 0x23aee3 (ret 4; EEPlayer 0x163cf2 adds mL44 to it; provisional name)
    // EEUnitClassImprovement.cpp's other members (provisional names; the arithmetic is ee_units improve.rs)
    void CopyLevelsTo(EEUnitClassUpgrade* inUpgrade, EEUnitClassImprovement* inTarget);  // 0x23a737
    void GetGainText(EEUnitClassUpgrade* inUpgrade, long inLevel, long inImprovement, UWideString& outText);  // 0x23a8a8
    void GetName(EEUnitClass* inUnitClass, long inImprovement, UWideString& outName);    // 0x23adbd
    void ChangeLevel(EEUnitClass* inUnitClass, EEUnitClassUpgrade* inUpgrade, long inImprovement, long inDelta);  // 0x23afc1
    void Reapply(EEUnitClass* inUnitClass, EEUnitClassUpgrade* inUpgrade);    // 0x23b35b (every level back to 1)
    long Load(EEUnitClass* inUnitClass, EEUnitClassUpgrade* inUpgrade, unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x23b39b
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // 0x23b5e4

    // +0x04 (= 1). int32_t, not long: Load and Save move the array as 0x40 raw bytes (16 4-byte words, as on
    // 32-bit Windows); with an 8-byte long (LP64) only levels 0..7 reached the file and back.
    int32_t mLevels[16];
    long mTotalCost;                        // +0x44 (= 0)
};

#endif
