// C:\EEX\Empire Earth\Scenario Editor\EESETSETAI.h (RECONSTRUCTION; file name inferred).
// The effect screen's AI effect types (EESETSETAI.cpp 0x182dda..0x183d81; their slots also lie in later units).
// Slot signatures: see ee/EESETSEffectType.h (ret 8 = (effect, controls), ret 4 = (controls); every caller
// ignores eax). Layouts from the `new` sizes and the ctor stores; member names by offset or by use. Slot names
// and argument types inferred; slot addresses from the vtables (tools/exe_mech_rtti.py).
#ifndef EE_EESETSETAI_H
#define EE_EESETSETAI_H

#include "ee/EESETSEffectType.h"
#include <vector>

class EESETSEffectControls;
class EESMEffect;
class UILBString;

// RTTI EESETSETAI > EESETSEffectType; vtable 0x443d98; sizeof 0x28; ctor 0x182dda and dtor 0x182ef7
// out of line (the ctor is called at the `new`, the ??_G calls the dtor). EESETSEffect::mTypes[6].
class EESETSETAI : public EESETSEffectType {
public:
    EESETSETAI();                                                                                   // 0x182dda
    virtual ~EESETSETAI();                                                                          // 0x182ef7 (deletes mTypes[] in order, Dereferences mAction)
    virtual void ChangeAction(EESMEffect* inEffect, EESETSEffectControls* inControls);              // +0x04 0x182f6a; ICF-shared with EESETSETGame
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x182fb9
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x18301c
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x183066
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x183114
    virtual void UpdateName(EESMEffect* inEffect, EESETSEffectControls* inControls);                // +0x18 0x18330a

    EESETSEffectType* mTypes[7];                                                                    // +0x04 (the ctor's `new`s; indexed by mCurrentType)
    //   [0] EESETSETModifyAIMilitaryTarget
    //   [1] EESETSETCreateAIMilitaryGroup
    //   [2] EESETSETBuildObject
    //   [3] EESETSETMaintainObject
    //   [4] EESETSETToggleAI
    //   [5] EESETSETSetAIAttribute
    //   [6] EESETSETSetFavoriteUnit
    UIEvent* mAction;                                                                               // +0x20 (new 0xc: the action combo's trigger event)
    long mCurrentType;                                                                              // +0x24 (0 in the ctor)
};

// RTTI EESETSETModifyAIMilitaryTarget > EESETSEffectType; vtable 0x443d78; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETModifyAIMilitaryTarget : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x187ce2
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x187d6c
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x187df6
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x187f71
};

// RTTI EESETSETCreateAIMilitaryGroup > EESETSEffectType; vtable 0x443dbc; sizeof 0x24; implicit ctor = the refused-inline COMDAT 0x18347f; ??_G 0x1834b5 calls the implicit dtor 0x1834d1
class EESETSETCreateAIMilitaryGroup : public EESETSEffectType {
public:
    virtual ~EESETSETCreateAIMilitaryGroup() {}                                                  // inline, user-declared (the dtor stores the vptr)
    // the list-box handlers of EESETSETCAMGModifyObject::ProcessEvent 0x18370d (ret 0xc; the clicked list's selection,
    // then the controls' mListBoxes[0] and [1])
    void AddObject(unsigned long inSelection, UILBString* inFirst, UILBString* inSecond);                 // 0x183753
    void RemoveObject(unsigned long inSelection, UILBString* inFirst, UILBString* inSecond);              // 0x183c66
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18386e
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x183904
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x1839da
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x183d81
    std::vector<unsigned long> mObjectsToAdd;  // +0x04
    std::vector<unsigned long> mObjectsAdded;  // +0x14
};

// RTTI EESETSETBuildObject > EESETSEffectType; vtable 0x443d58; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETBuildObject : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18357c; ICF-shared with EESETSETMaintainObject
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x185f75; ICF-shared with EESETSETMaintainObject
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x183596
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x1835f1; ICF-shared with EESETSETMaintainObject
};

// RTTI EESETSETMaintainObject > EESETSEffectType; vtable 0x443d38; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETMaintainObject : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18357c; ICF-shared with EESETSETBuildObject
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x185f75; ICF-shared with EESETSETBuildObject
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x185f92
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x1835f1; ICF-shared with EESETSETBuildObject
};

// RTTI EESETSETToggleAI > EESETSEffectType; vtable 0x443d18; sizeof 0x4; ctor inline (vptr store at the `new`)
class EESETSETToggleAI : public EESETSEffectType {
public:
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18e72b
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x18e757
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18e78e
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x18e80c
};

// RTTI EESETSETSetAIAttribute > EESETSEffectType; vtable 0x443cf8; sizeof 0x14; implicit ctor inline in 0x182dda; ??_G 0x183454
class EESETSETSetAIAttribute : public EESETSEffectType {
public:
    virtual ~EESETSETSetAIAttribute() {}                                                         // inline, user-declared (the dtor stores the vptr)
    void ShowAttributeRanges(unsigned long inSelection, EESETSEffectControls* inControls);          // 0x18d020 (ret 8; EESETSETSAAAttributeChange::ProcessEvent 0x18c4e3 passes the combo's +0xd0)
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18c4fd
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x18c593
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18c72e
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x18c96c
    std::vector<long> mAttributes;          // +0x04 (the inline ctor's _Buy(0) 0xe7d2e; the ??_G's _Tidy 0x2002c7)
};

// RTTI EESETSETSetFavoriteUnit > EESETSEffectType; vtable 0x443ddc; sizeof 0x34; implicit ctor = the refused-inline COMDAT 0x1834f2; ??_G 0x183537 calls the implicit dtor 0x183553
class EESETSETSetFavoriteUnit : public EESETSEffectType {
public:
    virtual ~EESETSETSetFavoriteUnit() {}                                                        // inline, user-declared (the dtor stores the vptr)
    virtual void Store(EESMEffect* inEffect, EESETSEffectControls* inControls);                     // +0x08 0x18d435
    virtual void VirtualC(EESETSEffectControls* inControls);                                        // +0x0c 0x18d492
    virtual void Load(EESMEffect* inEffect, EESETSEffectControls* inControls);                      // +0x10 0x18d4d5
    virtual void Show(EESETSEffectControls* inControls);                                            // +0x14 0x18d608
    std::vector<long> mAirUnits;            // +0x04
    std::vector<long> mLandUnits;           // +0x14
    std::vector<long> mSeaUnits;            // +0x24
};

#endif
