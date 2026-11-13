// C:\EEX\Empire Earth\Scenario Editor\EESETSETNoEffect.h (RECONSTRUCTION; file name inferred).
// The effect screen's no-effect type (EESETSEffect::mTypes[0]).
// Slot signatures: see ee/EESETSEffectType.h (ret 8 = (effect, controls), ret 4 = (controls); every caller
// ignores eax). Layouts from the `new` sizes and the ctor stores; member names by offset or by use. Slot names
// and argument types inferred; slot addresses from the vtables (tools/exe_mech_rtti.py).
#ifndef EE_EESETSETNOEFFECT_H
#define EE_EESETSETNOEFFECT_H

#include "ee/EESETSEffectType.h"

class EESETSEffectControls;
class EESMEffect;

// RTTI EESETSETNoEffect > EESETSEffectType; vtable 0x443c44; sizeof 0x4; ctor inline in EESETSEffect's ctor 0x182629
class EESETSETNoEffect : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls) {}                   // +0x08 (the shared empty `ret 8` 0x163d6f)
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x188399
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls) {}                    // +0x10 (the shared empty `ret 8` 0x163d6f)
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x18841b
};

#endif
