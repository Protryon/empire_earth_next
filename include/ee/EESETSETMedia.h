// C:\EEX\Empire Earth\Scenario Editor\EESETSETMedia.h (RECONSTRUCTION; file name inferred).
// The effect screen's media effect types (EESETSETMedia.cpp 0x185fed..0x186e1f).
// Slot signatures: see ee/EESETSEffectType.h (ret 8 = (effect, controls), ret 4 = (controls); every caller
// ignores eax). Layouts from the `new` sizes and the ctor stores; member names by offset or by use. Slot names
// and argument types inferred; slot addresses from the vtables (tools/exe_mech_rtti.py).
#ifndef EE_EESETSETMEDIA_H
#define EE_EESETSETMEDIA_H

#include "ee/EESETSEffectType.h"

class EESETSEffectControls;
class EESMEffect;

// RTTI EESETSETMedia > EESETSEffectType; vtable 0x443fe8; sizeof 0x1c; ctor 0x185fed and dtor 0x1860a6
// out of line (the ctor is called at the `new`, the ??_G calls the dtor). EESETSEffect::mTypes[4].
class EESETSETMedia : public EESETSEffectType {
public:
    EESETSETMedia();                                                                                // 0x185fed
    virtual ~EESETSETMedia();                                                                       // 0x1860a6 (deletes mTypes[] in order, Dereferences mAction)
    virtual void ChangeAction(EESMEffect* inEffect, EESETSEffectControls* inControls);              // +0x04 0x1860f4
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x186143
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x186198
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x1861cf
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x186233
    virtual void UpdateName(EESMEffect* inEffect, EESETSEffectControls* inControls);                // +0x18 0x186394

    EESETSEffectType* mTypes[4];                                                                    // +0x04 (the ctor's `new`s; indexed by mCurrentType)
    //   [0] EESETSETMediaPlayGraphic
    //   [1] EESETSETMediaChangeText
    //   [2] EESETSETMediaSendDialog
    //   [3] EESETSETMediaSendMessage
    UIEvent* mAction;                                                                               // +0x14 (new 0xc: the action combo's trigger event)
    unsigned long mCurrentType;                                                                     // +0x18 (0 in the ctor; Virtual04 passes it to GetCurrentSelection: unsigned)
};

// RTTI EESETSETMediaPlayGraphic > EESETSEffectType; vtable 0x443fc8; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETMediaPlayGraphic : public EESETSEffectType {
public:
    EESETSETMediaPlayGraphic() : mUsesFileName(true) {}  // inline (the ctor 0x185fed news 8 bytes and stores +0x04 = 1 before the vptr)
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x1869fc
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x186a4d
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x186a6a
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x186a8a
    bool mUsesFileName;                     // +0x04 (true in the ctor; sizeof 8)
};

// RTTI EESETSETMediaChangeText > EESETSEffectType; vtable 0x443fa8; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETMediaChangeText : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x186531
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x18659f
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18660b
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x1866ab
};

// RTTI EESETSETMediaSendDialog > EESETSEffectType; vtable 0x443f88; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETMediaSendDialog : public EESETSEffectType {
public:
    void ShowCameraScrollSpeed(EESETSEffectControls* inControls);                                   // 0x187441 (ret 4; EESETSETMSDCameraScrollChange::ProcessEvent 0x186bb0)
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x186bc0
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x186c7e
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x186d1a
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x186e1f
};

// RTTI EESETSETMediaSendMessage > EESETSEffectType; vtable 0x443f68; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETMediaSendMessage : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x1874b3
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x18751a
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18755d
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x1875d7
};

#endif
