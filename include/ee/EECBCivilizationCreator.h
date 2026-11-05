// C:\EEX\Empire Earth\Civilization Builder\EECBCivilizationCreator.h (RECONSTRUCTION; file name inferred;
// provisional member names). RTTI EECBCivilizationCreator : EECBEditableCivilization (vtable 0x44be88, sizeof
// 0x35, ctor 0x27c8b9). The creator works with points to spend instead of points left: when mB34 is set the
// upgrades add to mL14. Slot +0x00 and +0x0c are the shared `mov al, 1; ret` (0x60b1). Requires
// ee/EECBEditableCivilization.h.
#ifndef EE_EECBCIVILIZATIONCREATOR_H
#define EE_EECBCIVILIZATIONCREATOR_H
#include "ee/EECivUpgrade.h"

#include "ee/EECBEditableCivilization.h"

class EECBCivilizationCreator : public EECBEditableCivilization {
public:
    EECBCivilizationCreator(EECivilization* inCivilization, bool inIsUnlimited);  // 0x27c8b9
    virtual bool Apply() { return true; }           // +0x00 (ICF-folded 0x60b1)
    virtual bool CanChange() { return true; }       // +0x0c (ICF-folded 0x60b1)
    virtual unsigned long GetPointsLeft();                 // +0x10 0x27c8e2
    virtual long GetMaxPoints();                    // +0x14 0x27c8f0
    virtual bool AddUpgrade(EECivUpgrade inCount);  // +0x18 0x27c8ff
    virtual bool RemoveUpgrade(EECivUpgrade inUpgrade);  // +0x1c 0x27c954

    bool mIsUnlimited;                      // +0x34 (ctor argument 2)
};

#endif
