// EEUnitClassImprovement.h (RECONSTRUCTION from RTTI/vtables/ctors; file name inferred). Classes:
// EEUnitClassImprovement (EEUnitClassImprovement.cpp 0x23a5bf..0x23b61a).
// Layout gap-free from the ctor 0x23a5bf (`rep stosd` of 16 ones from +0x04, +0x44 = 0) and the dtor 0x23a5f7 (the
// vptr store only: a user-declared empty dtor); sizeof 0x48 (EEUnitClassUpgrade's next member is at +0x4c of the
// object it is embedded in at +0x04). Names are provisional.
#ifndef EE_EEUNITCLASSIMPROVEMENT_H
#define EE_EEUNITCLASSIMPROVEMENT_H

#include <stdint.h>   // int32_t: the levels are saved as 16 raw 4-byte words
#include "rust/sim/units.h"

// vtable 0x449acc; bases EEUnitClassImprovement; sizeof 0x48; ctor 0x23a5bf; ??_G 0x23a5db; dtor 0x23a5f7
class EEUnitClass;
class EEUnitClassUpgrade;
class FSFile;
class UWideString;

// port (the groups lane, round 2): the constructor, CopyLevelsTo, Reapply, Load and Save are Rust (ee_units
// improvement.rs) over the levels and the points; the C++ they call is EEImprovementHost (ChangeLevel, the upgrade's
// name, the file's primitives) on an EEImprovementContext
struct EeUnitsPlayerSaveIo;
class EEUnitClassImprovement;
struct EEImprovementContext {
    EEUnitClassImprovement* mImprovement;   // the record ChangeLevel raises
    EEUnitClass* mClass;
    EEUnitClassUpgrade* mUpgrade;
    FSFile* mFile;
    operator void*() { return this; }
};
int32_t EEImprovementHost(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io);

class EEUnitClassImprovement {
public:
    EEUnitClassImprovement() { ee_units_improvement(EE_UNITS_IMPROVEMENT_DO_RESET, this->mLevels, &this->mTotalCost, NULL, &EEImprovementHost, 0, 0); }  // 0x23a5bf: every mLevels = 1, mL44 = 0
    virtual ~EEUnitClassImprovement() {}  // +0x00 dtor 0x23a5f7

    long GetLevel(long inIndex) { return this->mLevels[inIndex]; }  // 0x23a89d: mLevels[inIndex] (out of line)
    // 0x23a5fe: whether inImprovement can still be raised for inUnitClass (EEUIBUpgrade, the scenario editor's
    // selected unit; ret 8, tested `== 1`) (provisional name)
    bool CanUpgrade(EEUnitClass* inUnitClass, long inImprovement);
    long GetCost(long inImprovement) { return ee_units_improve_cost((int32_t)inImprovement); }  // 0x23aee3 (ret 4; EEPlayer 0x163cf2 adds mL44 to it; provisional name)
    // EEUnitClassImprovement.cpp's other members (provisional names; the arithmetic is ee_units improve.rs)
    void CopyLevelsTo(EEUnitClassUpgrade* inUpgrade, EEUnitClassImprovement* inTarget);
    void GetGainText(EEUnitClassUpgrade* inUpgrade, long inLevel, long inImprovement, UWideString& outText);
    void GetName(EEUnitClass* inUnitClass, long inImprovement, UWideString& outName);
    void ChangeLevel(EEUnitClass* inUnitClass, EEUnitClassUpgrade* inUpgrade, long inImprovement, long inDelta);
    void Reapply(EEUnitClass* inUnitClass, EEUnitClassUpgrade* inUpgrade);    // 0x23b35b (every level back to 1)
    long Load(EEUnitClass* inUnitClass, EEUnitClassUpgrade* inUpgrade, unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);

    // +0x04 (= 1). int32_t, not long: Load and Save move the array as 0x40 raw bytes (16 4-byte words, as on
    // 32-bit Windows); with an 8-byte long (LP64) only levels 0..7 reached the file and back.
    int32_t mLevels[16];
    long mTotalCost;                        // +0x44 (= 0)
};

#endif
