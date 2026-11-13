// C:\EEX\Empire Earth\Scenario Editor\EESETSETTrigger.h (RECONSTRUCTION; file name inferred).
// The effect screen's trigger effect type (EESETSEffect::mTypes[3]).
// Slot signatures: see ee/EESETSEffectType.h (ret 8 = (effect, controls), ret 4 = (controls); every caller
// ignores eax). Layouts from the `new` sizes and the ctor stores; member names by offset or by use. Slot names
// and argument types inferred; slot addresses from the vtables (tools/exe_mech_rtti.py).
#ifndef EE_EESETSETTRIGGER_H
#define EE_EESETSETTRIGGER_H

#include "ee/EESETSEffectType.h"

class EESETSEffectControls;
class EESMEffect;

// RTTI EESETSETTrigger > EESETSEffectType; vtable 0x443c24; sizeof 0x4; ctor inline in EESETSEffect's ctor 0x182629
class EESETSETTrigger : public EESETSEffectType {
public:
    virtual void ChangeAction(EESMEffect* inEffect, EESETSEffectControls* inControls);              // +0x04 0x18ea18
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18ea75
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x18eaa3
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18eadc
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x18eb78
    virtual void UpdateName(EESMEffect* inEffect, EESETSEffectControls* inControls);                // +0x18 0x18ee17
};

#endif
