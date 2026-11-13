// C:\EEX\Empire Earth\Scenario Editor\EESETSETObject.h (RECONSTRUCTION; file name inferred).
// The effect screen's object effect types (EESETSETObject.cpp 0x18849c..0x189904; their slots also lie in later units).
// Slot signatures: see ee/EESETSEffectType.h (ret 8 = (effect, controls), ret 4 = (controls); every caller
// ignores eax). Layouts from the `new` sizes and the ctor stores; member names by offset or by use. Slot names
// and argument types inferred; slot addresses from the vtables (tools/exe_mech_rtti.py).
#ifndef EE_EESETSETOBJECT_H
#define EE_EESETSETOBJECT_H

#include "ee/EESETSEffectType.h"
#include <vector>

class EESETSEffectControls;
class EESMEffect;

// RTTI EESETSETObject > EESETSEffectType; vtable 0x4441b4; sizeof 0x40; ctor 0x18849c and dtor 0x188660
// out of line (the ctor is called at the `new`, the ??_G calls the dtor). EESETSEffect::mTypes[1]. The ctor's `new` order is [0], [11], [1]..[6], [9], [8], [7], [10], [12].
class EESETSETObject : public EESETSEffectType {
public:
    EESETSETObject();                                                                               // 0x18849c
    virtual ~EESETSETObject();                                                                      // 0x188660 (deletes mTypes[] in order, Dereferences mAction)
    virtual void ChangeAction(EESMEffect* inEffect, EESETSEffectControls* inControls);              // +0x04 0x18871b
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18876a
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x1887d3
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18881d
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x188890
    virtual void UpdateName(EESMEffect* inEffect, EESETSEffectControls* inControls);                // +0x18 0x188aef

    EESETSEffectType* mTypes[13];                                                                   // +0x04 (the ctor's `new`s; indexed by mCurrentType)
    //   [0] EESETSETObjectCreate
    //   [1] EESETSETFreezeObject
    //   [2] EESETSETFreezeObject (a second instance)
    //   [3] EESETSETKillObject
    //   [4] EESETSETRemoveObject
    //   [5] EESETSETObjectToObject
    //   [6] EESETSETObjectToArea
    //   [7] EESETSETSetObjectStance (ee/EESETSETSetObjectStance.h; out-of-line ctor 0x18e4a4)
    //   [8] EESETSETSetObjectAttribute
    //   [9] EESETSETSetObjectOwner
    //   [10] EESETSETSetObjectName
    //   [11] EESETSETLogObject
    //   [12] EESETSETSetObjectClassAttribute
    UIEvent* mAction;                                                                               // +0x38 (new 0xc: the action combo's trigger event)
    unsigned long mCurrentType;                                                                     // +0x3c (0 in the ctor; unsigned: Virtual18's switch compares with jbe)
};

// RTTI EESETSETObjectCreate > EESETSEffectType; vtable 0x444194; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETObjectCreate : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls) {}                   // +0x08 (the shared empty `ret 8` 0x163d6f)
    virtual void VirtualC(EESETSEffectControls* inControls) {}                                      // +0x0c (the shared empty `ret 4` 0x157269)
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls) {}                    // +0x10 (the shared empty `ret 8` 0x163d6f)
    virtual void Show(EESETSEffectControls* inControls) {}                                          // +0x14 (the shared empty `ret 4` 0x157269)
};

// RTTI EESETSETFreezeObject > EESETSEffectType; vtable 0x444154; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETFreezeObject : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls) {}                   // +0x08 (the shared empty `ret 8` 0x163d6f)
    virtual void VirtualC(EESETSEffectControls* inControls) {}                                      // +0x0c (the shared empty `ret 4` 0x157269)
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls) {}                    // +0x10 (the shared empty `ret 8` 0x163d6f)
    virtual void Show(EESETSEffectControls* inControls) {}                                          // +0x14 (the shared empty `ret 4` 0x157269)
};

// RTTI EESETSETKillObject > EESETSEffectType; vtable 0x444134; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETKillObject : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls) {}                   // +0x08 (the shared empty `ret 8` 0x163d6f)
    virtual void VirtualC(EESETSEffectControls* inControls) {}                                      // +0x0c (the shared empty `ret 4` 0x157269)
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls) {}                    // +0x10 (the shared empty `ret 8` 0x163d6f)
    virtual void Show(EESETSEffectControls* inControls) {}                                          // +0x14 (the shared empty `ret 4` 0x157269)
};

// RTTI EESETSETRemoveObject > EESETSEffectType; vtable 0x444114; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETRemoveObject : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls) {}                   // +0x08 (the shared empty `ret 8` 0x163d6f)
    virtual void VirtualC(EESETSEffectControls* inControls) {}                                      // +0x0c (the shared empty `ret 4` 0x157269)
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls) {}                    // +0x10 (the shared empty `ret 8` 0x163d6f)
    virtual void Show(EESETSEffectControls* inControls) {}                                          // +0x14 (the shared empty `ret 4` 0x157269)
};

// RTTI EESETSETObjectToObject > EESETSEffectType; vtable 0x4440f4; sizeof 0x14; implicit ctor inline in 0x18849c; ??_G 0x188f0a
class EESETSETObjectToObject : public EESETSEffectType {
public:
    virtual ~EESETSETObjectToObject() {}                                                        // inline, user-declared (the ??_G stores the own vptr first)
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x1893fc
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x189432
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x189475
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x189552
    std::vector<long> mActions;             // +0x04 (the inline ctor's _Buy(0) 0xe7d2e; the ??_G's _Tidy 0x2002c7)
};

// RTTI EESETSETObjectToArea > EESETSEffectType; vtable 0x4440d4; sizeof 0x14; implicit ctor inline in 0x18849c; ??_G 0x188f35
class EESETSETObjectToArea : public EESETSEffectType {
public:
    virtual ~EESETSETObjectToArea() {}                                                        // inline, user-declared (the ??_G stores the own vptr first)
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x188f60
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x188f96
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x188fd9
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x1890bd
    std::vector<long> mActions;             // +0x04 (the inline ctor's _Buy(0) 0xe7d2e; the ??_G's _Tidy 0x2002c7)
};

// RTTI EESETSETSetObjectAttribute > EESETSEffectType; vtable 0x444094; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETSetObjectAttribute : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18da54
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x18daaa; ICF-shared with EESETSETSetObjectClassAttribute
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18dae7
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x18dbb5
};

// RTTI EESETSETSetObjectOwner > EESETSEffectType; vtable 0x4440b4; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETSetObjectOwner : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18e2a9
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x18e2c2
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18e2f3
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x18e34d
};

// RTTI EESETSETSetObjectName > EESETSEffectType; vtable 0x444074; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETSetObjectName : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18e12a
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x18e168
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18e185
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x18e19f
};

// RTTI EESETSETLogObject > EESETSEffectType; vtable 0x444174; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETLogObject : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x185d31
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x185d69
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x185d9d
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x185dd8
};

// RTTI EESETSETSetObjectClassAttribute > EESETSEffectType; vtable 0x444054; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETSetObjectClassAttribute : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18ddeb
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x18daaa; ICF-shared with EESETSETSetObjectAttribute
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18de44
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x18def4
};

#endif
