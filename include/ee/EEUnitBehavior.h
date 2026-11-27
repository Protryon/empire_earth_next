// C:\EEX\Empire Earth\Database\EEUnitBehavior.h (RECONSTRUCTION; header and provisional names).
// The unit behaviour flags of db\dbUnitBehavior.dat (RTTI EEUnitBehavior, no base; vtable 0x43f808, one slot: ??_G
// 0xdc950; sizeof 0xc from Load's new). Header-only since the game's leaves round 2 (EEUnitBehavior.cpp went: the
// list and the to-hit factors are ee_data's).
#ifndef EE_EEUNITBEHAVIOR_H
#define EE_EEUNITBEHAVIOR_H

#include "ee/EEDbList.h"
#include "rust/data/records.h"

// the flags are EeDataUnitBehaviorFields (rust/crates/ee_data tables.rs: the record, its copy and the list), the base
class EEUnitBehavior : public EeDataUnitBehaviorFields {
public:
    // 0xdc973: db\dbUnitBehavior.dat into the Rust list (records with a negative +0x68 stay NULL); the last status
    template <class F> static long Load(F* inFile) { return EEDbLoadList(inFile, EeDataList::UnitBehaviors); }
    static void Unload() { EEUnitBehavior::sTable.clear(); }
    // 0xdca7d: whether a unit may move freely: always, or by the melee or ranged permission matching its class
    // (provisional name; a template: the unit's class is complete only where it is called)
    template <class U> bool CanMoveFreely(U* inUnit)
    {
        return this->mCanMoveFreely || (inUnit->GetUnitClass()->mIsMelee && this->mMeleeCanMoveFreely)
               || (!inUnit->GetUnitClass()->mIsMelee && this->mRangedCanMoveFreely);
    }
    // cdecl COMDAT 0x1f5430, called out of line by EEComplexUnit::FinishLoad 0x1f353f (x2) and ResetEEComplexUnit
    // 0x1fac37
    static EEUnitBehavior* Get(unsigned long inIndex) { return EEUnitBehavior::sTable[inIndex]; }


    // 0x8e8740 (guard 0x8e8738 bit 1; only its unit sees it: other units read its _First as gUnitBehaviors)
    static constexpr EEDbList<EEUnitBehavior, EeDataList::UnitBehaviors> sTable{};   // Rust's list (ee/EEDbList.h)
};

// 0x4e8750: the weapon to-hit factors (ee_data tables.rs; provisional name; EEPhysicsModel::Push)
extern "C" float gWeaponToHitFactors[6][6];

#endif
