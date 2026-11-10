// EEAUpgradeImprovement (RECONSTRUCTION; file name inferred). The UI action that raises (or, from the right mouse
// button, lowers) one improvement of a unit (RTTI EEAUpgradeImprovement > EEAction; vtable 0x4390d4; sizeof 0x18:
// EEUIBUpgrade's `new`). Only the ctor is declared so far. Member names are provisional. Requires
// ee/EEAction.h.
#ifndef EE_EEAUPGRADEIMPROVEMENT_H
#define EE_EEAUPGRADEIMPROVEMENT_H

#include "ee/EEUnitHandle.h"

class EEUnit;

class EEAUpgradeImprovement : public EEAction {
public:
    // 0x505b: EEAction() (+0x08 = false), mUnit referenced, the rest from the arguments, then posts itself
    // (gUserInterface->PostAction(this))
    EEAUpgradeImprovement(EEUnit* inUnit, long inImprovement, bool inRaise, bool inIsInGame);
    virtual ~EEAUpgradeImprovement();       // 0x50bc (mUnit->Dereference(); ??_G 0x50a0)
    virtual void Virtual4();                // +0x04
    virtual void Virtual8();                // +0x08

    EEUnitHandle<EEUnit> mUnit;             // +0x0c (referenced; a handle since R5a.4: posted from the UI thread, run on the physics thread)
    long mImprovement;                      // +0x10
    bool mRaise;                            // +0x14 (EEUIBUpgrade: true from ButtonUp, false from MouseRButtonUp)
    bool mIsInGame;                         // +0x15 (EEUIBUpgrade: not on the scenario editor)
};

#endif
