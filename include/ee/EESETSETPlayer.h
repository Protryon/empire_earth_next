// C:\EEX\Empire Earth\Scenario Editor\EESETSETPlayer.h (RECONSTRUCTION; file name inferred).
// The effect screen's player effect types (EESETSETPlayer.cpp 0x189904..0x18c72e).
// Slot signatures: see ee/EESETSEffectType.h (ret 8 = (effect, controls), ret 4 = (controls); every caller
// ignores eax). Layouts from the `new` sizes and the ctor stores; member names by offset or by use. Slot names
// and argument types inferred; slot addresses from the vtables (tools/exe_mech_rtti.py).
#ifndef EE_EESETSETPLAYER_H
#define EE_EESETSETPLAYER_H

#include "ee/EESETSEffectType.h"
#include <vector>

class EESETSEffectControls;
class EESMEffect;

// RTTI EESETSETPlayer > EESETSEffectType; vtable 0x444318; sizeof 0x3c; ctor 0x189904 and dtor 0x189a91
// out of line (the ctor is called at the `new`, the ??_G calls the dtor). EESETSEffect::mTypes[2]. The ctor's `new` order is [0], [2], [3], [4], [5], [1], [6]..[11].
class EESETSETPlayer : public EESETSEffectType {
public:
    EESETSETPlayer();                                                                               // 0x189904
    virtual ~EESETSETPlayer();                                                                      // 0x189a91 (deletes mTypes[] in order, Dereferences mAction)
    virtual void ChangeAction(EESMEffect* inEffect, EESETSEffectControls* inControls);              // +0x04 0x189b40
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x189b8f
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x189bf2
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x189c3c
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x189cae
    virtual void UpdateName(EESMEffect* inEffect, EESETSEffectControls* inControls);                // +0x18 0x189ebc

    EESETSEffectType* mTypes[12];                                                                   // +0x04 (the ctor's `new`s; indexed by mCurrentType)
    //   [0] EESETSETPlayerVictory
    //   [1] EESETSETPlayerView
    //   [2] EESETSETPlayerVictory (a second instance)
    //   [3] EESETSETPlayerDiplomacy
    //   [4] EESETSETPlayerResource
    //   [5] EESETSETPlayerTechnology
    //   [6] EESETSETPlayerCivPoint
    //   [7] EESETSETPCameraFadeIn
    //   [8] EESETSETPCameraFadeOut
    //   [9] EESETSETPVisibility
    //   [10] EESETSETPVisibility (a second instance)
    //   [11] EESETSETMiniMapFlare
    UIEvent* mAction;                                                                               // +0x34 (new 0xc: the action combo's trigger event)
    long mCurrentType;                                                                              // +0x38 (0 in the ctor)
};

// RTTI EESETSETPlayerVictory > EESETSEffectType; vtable 0x4442f8; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETPlayerVictory : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls) {}                   // +0x08 (the shared empty `ret 8` 0x163d6f)
    virtual void VirtualC(EESETSEffectControls* inControls) {}                                      // +0x0c (the shared empty `ret 4` 0x157269)
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18b156
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x1898eb; ICF-shared with EESETSETPVisibility, EESETSETPCameraFadeOut, EESETSETPCameraFadeIn
};

// RTTI EESETSETPlayerView > EESETSEffectType; vtable 0x444278; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETPlayerView : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18b362
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x18b456
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18b5cc
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x18b8f7
};

// RTTI EESETSETPlayerDiplomacy > EESETSEffectType; vtable 0x4442d8; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETPlayerDiplomacy : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18a38d
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x18a3b7
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18a3f1
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x18a4cc
};

// RTTI EESETSETPlayerResource > EESETSEffectType; vtable 0x4442b8; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETPlayerResource : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18a702
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x18a746
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18a780
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x18a85b
};

// RTTI EESETSETPlayerTechnology > EESETSEffectType; vtable 0x444298; sizeof 0x14; implicit ctor inline in 0x189904; ??_G 0x18a1d3
class EESETSETPlayerTechnology : public EESETSEffectType {
public:
    virtual ~EESETSETPlayerTechnology() {}                                                     // inline, user-declared (the ??_G stores the own vptr first)
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18ac29
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x18ac5b
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18ac92
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x18adb4
    std::vector<long> mTechnologies;        // +0x04 (the inline ctor's _Buy(0) 0xe7d2e; the ??_G's _Tidy 0x2002c7)
};

// RTTI EESETSETPlayerCivPoint > EESETSEffectType; vtable 0x444258; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETPlayerCivPoint : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18a1fe
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x18a222
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18a23f
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x18a2ac
};

// RTTI EESETSETPCameraFadeIn > EESETSEffectType; vtable 0x444238; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETPCameraFadeIn : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls) {}                   // +0x08 (the shared empty `ret 8` 0x163d6f)
    virtual void VirtualC(EESETSEffectControls* inControls) {}                                      // +0x0c (the shared empty `ret 4` 0x157269)
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x189891; ICF-shared with EESETSETPVisibility, EESETSETPCameraFadeOut
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x1898eb; ICF-shared with EESETSETPVisibility, EESETSETPCameraFadeOut, EESETSETPlayerVictory
};

// RTTI EESETSETPCameraFadeOut > EESETSEffectType; vtable 0x444218; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETPCameraFadeOut : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls) {}                   // +0x08 (the shared empty `ret 8` 0x163d6f)
    virtual void VirtualC(EESETSEffectControls* inControls) {}                                      // +0x0c (the shared empty `ret 4` 0x157269)
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x189891; ICF-shared with EESETSETPVisibility, EESETSETPCameraFadeIn
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x1898eb; ICF-shared with EESETSETPVisibility, EESETSETPCameraFadeIn, EESETSETPlayerVictory
};

// RTTI EESETSETPVisibility > EESETSEffectType; vtable 0x4441f8; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETPVisibility : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls) {}                   // +0x08 (the shared empty `ret 8` 0x163d6f)
    virtual void VirtualC(EESETSEffectControls* inControls) {}                                      // +0x0c (the shared empty `ret 4` 0x157269)
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x189891; ICF-shared with EESETSETPCameraFadeOut, EESETSETPCameraFadeIn
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x1898eb; ICF-shared with EESETSETPCameraFadeOut, EESETSETPCameraFadeIn, EESETSETPlayerVictory
};

// RTTI EESETSETMiniMapFlare > EESETSEffectType; vtable 0x4441d8; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETMiniMapFlare : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18780d
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x187871
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x1878e2
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x187a5f
};

#endif
