// C:\EEX\Empire Earth\Scenario Editor\EESETSETSetObjectStance.h (RECONSTRUCTION; file name inferred; layout proven:
// EESETSETSetObjectStance sizeof 0x8 = its new-site size, ctor 0x18e4a4 exact in EESETSETSetObjectStance.cpp; member
// names are provisional).
// RTTI EESETSETSetObjectStance > EESETSEffectType (vtable 0x44439c); EESETSETObject::mTypes[7] (new 8 in 0x18849c).
// The user dtor 0x18e4f1 (out of line: the ??_G 0x18e4d5 calls it) Dereferences mAction. Slot signatures: see
// ee/EESETSEffectType.h.
#ifndef EE_EESETSETSETOBJECTSTANCE_H
#define EE_EESETSETSETOBJECTSTANCE_H

#include "ee/EESETSEffectType.h"


class EESETSETSetObjectStance : public EESETSEffectType
{
public:
    EESETSETSetObjectStance();                                                                      // 0x18e4a4
    virtual ~EESETSETSetObjectStance();                                                             // 0x18e4f1
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18e50b
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x18e525
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18e54c
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x18e5a7

    UIEvent* mAction;                                                                               // +0x04 (the ctor's new 0xc)
};

#endif
