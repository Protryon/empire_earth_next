// C:\EEX\Empire Earth\Scenario Editor\EESETSETGame.h (RECONSTRUCTION; file name inferred).
// The effect screen's game effect types (EESETSETGame.cpp 0x1843c7..0x185fed).
// Slot signatures: see ee/EESETSEffectType.h (ret 8 = (effect, controls), ret 4 = (controls); every caller
// ignores eax). Layouts from the `new` sizes and the ctor stores; member names by offset or by use. Slot names
// and argument types inferred; slot addresses from the vtables (tools/exe_mech_rtti.py).
#ifndef EE_EESETSETGAME_H
#define EE_EESETSETGAME_H

#include "ee/EESETSEffectType.h"
#include <vector>

class EESETSEffectControls;
class EESMEffect;

// RTTI EESETSETGame > EESETSEffectType; vtable 0x443f14; sizeof 0x28; ctor 0x1843c7 and dtor 0x1844fb
// out of line (the ctor is called at the `new`, the ??_G calls the dtor). EESETSEffect::mTypes[5].
class EESETSETGame : public EESETSEffectType {
public:
    EESETSETGame();                                                                                 // 0x1843c7
    virtual ~EESETSETGame();                                                                        // 0x1844fb (deletes mTypes[] in order, Dereferences mAction)
    virtual void ChangeAction(EESMEffect* inEffect, EESETSEffectControls* inControls);              // +0x04 0x182f6a; ICF-shared with EESETSETAI
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18456e
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x1845c3
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x1845fa
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x18465e
    virtual void UpdateName(EESMEffect* inEffect, EESETSEffectControls* inControls);                // +0x18 0x1847bf

    EESETSEffectType* mTypes[7];                                                                    // +0x04 (the ctor's `new`s; indexed by mCurrentType)
    //   [0] EESETSETGameDayNight
    //   [1] EESETSETGameCycleTime
    //   [2] EESETSETGameCalamity
    //   [3] EESETSETGameGraphicEffect
    //   [4] EESETSETGameStartCinematic
    //   [5] EESETSETGameEndCinematic
    //   [6] EESETSETGameAmbientWeather
    UIEvent* mAction;                                                                               // +0x20 (new 0xc: the action combo's trigger event)
    long mCurrentType;                                                                              // +0x24 (0 in the ctor)
};

// RTTI EESETSETGameDayNight > EESETSEffectType; vtable 0x443ef4; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETGameDayNight : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18526f; ICF-shared with EESETSETGameStartCinematic
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x185a89; ICF-shared with EESETSETGameStartCinematic
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18528c
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x1852b2
};

// RTTI EESETSETGameCycleTime > EESETSEffectType; vtable 0x443ed4; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETGameCycleTime : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18509d
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x1850c1
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x1850f2
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x18510c
};

// RTTI EESETSETGameCalamity > EESETSEffectType; vtable 0x443eb4; sizeof 0x14; implicit ctor inline in 0x1843c7; ??_G 0x1848de
class EESETSETGameCalamity : public EESETSEffectType {
public:
    virtual ~EESETSETGameCalamity() {}                                                                    // (the ??_G stores the own vptr first: user-declared)
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x184b58; ICF-shared with EESETSETGameGraphicEffect
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x1854cb; ICF-shared with EESETSETGameGraphicEffect
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x184bd6
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x184db5
    std::vector<long> mCalamities;          // +0x04 (the inline ctor's _Buy(0) 0xe7d2e; the ??_G's _Tidy 0x2002c7)
};

// RTTI EESETSETGameGraphicEffect > EESETSEffectType; vtable 0x443e94; sizeof 0x14; implicit ctor inline in 0x1843c7; ??_G 0x184909
class EESETSETGameGraphicEffect : public EESETSEffectType {
public:
    virtual ~EESETSETGameGraphicEffect() {}                                                                    // (the ??_G stores the own vptr first: user-declared)
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x184b58; ICF-shared with EESETSETGameCalamity
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x1854cb; ICF-shared with EESETSETGameCalamity
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x185537
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x185790
    std::vector<long> mGraphicEffects;      // +0x04 (the inline ctor's _Buy(0) 0xe7d2e; the ??_G's _Tidy 0x2002c7)
};

// RTTI EESETSETGameStartCinematic > EESETSEffectType; vtable 0x443e74; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETGameStartCinematic : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18526f; ICF-shared with EESETSETGameDayNight
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x185a89; ICF-shared with EESETSETGameDayNight
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x185adc
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x185b5f
};

// RTTI EESETSETGameEndCinematic > EESETSEffectType; vtable 0x443e54; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETGameEndCinematic : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls) {}                   // +0x08 (the shared empty `ret 8` 0xac9c8)
    virtual void VirtualC(EESETSEffectControls* inControls) {}                                      // +0x0c (the shared empty `ret 4` 0x156dd8)
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls) {}                    // +0x10 (the shared empty `ret 8` 0xac9c8)
    virtual void Show(EESETSEffectControls* inControls) {}                                          // +0x14 (the shared empty `ret 4` 0x156dd8)
};

// RTTI EESETSETGameAmbientWeather > EESETSEffectType; vtable 0x443e34; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETGameAmbientWeather : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x184934
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x184951
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x184965
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x184985
};

#endif
