// C:\EEX\Empire Earth\Database\EEUnitBehavior.h (RECONSTRUCTION; header and provisional names).
// The unit behaviour flags of db\dbUnitBehavior.dat (RTTI EEUnitBehavior, no base; vtable 0x43f808, one slot: ??_G
// 0xdc950; sizeof 0xc from Load's new). EEUnitBehavior.cpp 0xdc8e7..0xdd160 (with EEWorldTable's and EEToHitTable's
// loaders).
#ifndef EE_EEUNITBEHAVIOR_H
#define EE_EEUNITBEHAVIOR_H

#include "ee/EEDbList.h"

class EEUnit;
class FSFileSpec;

// the flags are EeDataUnitBehaviorFields (rust/crates/ee_data tables.rs: the record, its copy and the list), the base
class EEUnitBehavior : public EeDataUnitBehaviorFields {
public:
    static long Load(FSFileSpec* inFile);   // 0xdc973 (records with a negative +0x68 stay NULL)
    static void Unload();
    bool CanMoveFreely(EEUnit* inUnit);     // 0xdca7d (by the unit class's +0x318 flag; provisional name)
    // cdecl COMDAT 0x1f5430, called out of line by EEComplexUnit::FinishLoad 0x1f353f (x2) and ResetEEComplexUnit
    // 0x1fac37
    static EEUnitBehavior* Get(unsigned long inIndex) { return EEUnitBehavior::sTable[inIndex]; }


    // 0x8e8740 (guard 0x8e8738 bit 1; only its unit sees it: other units read its _First as gUnitBehaviors)
    static EEDbList<EEUnitBehavior, EeDataList::UnitBehaviors> sTable;   // Rust's list (ee/EEDbList.h)
};

// 0x4e8750: the weapon to-hit factors, defined in EEUnitBehavior.cpp (provisional name; EEPhysicsModel::Push)
extern float gWeaponToHitFactors[6][6];

#endif
