// C:\EEX\Empire Earth\Scenario Editor\EESETSEffect.h (RECONSTRUCTION; file name inferred). The trigger screen's
// effect sub-screen (RTTI EESETSEffect > EESETSBaseObject; vtable 0x443c78, sizeof 0x38, ctor 0x182629, ??_G
// 0x182988, dtor 0x1829a4; EESETSEffect.cpp 0x182629..0x182dda), the twin
// of EESETSCondition: it edits the scenario manager's effect mL04 + 1 (EEScenarioManager::GetEffect) through the
// current effect type (mType) and the shared controls (mControls). The seven effect types (the original's
// mTypes[7] of EESETSEffectType) are one Rust object (ee_screens editor_effects.rs). Layout from the ctor's stores
// and `new`s; slot addresses from the vtable; provisional names. Requires ee/EESETSBaseObject.h and
// rust/ee_screens.h.
#ifndef EE_EESETSEFFECT_H
#define EE_EESETSEFFECT_H

struct EeScreensSeEffects;
class EESETSEffectControls;
class EESMEffect;
class GEFont;
class UIComboBox;
class UIForm;
class UITextLabel;

class EESETSEffect : public EESETSBaseObject {
public:
    EESETSEffect(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex);   // 0x182629
    virtual ~EESETSEffect();                // +0x00 0x1829a4 (deletes mControls, the label, the combo, mTypes[])
    virtual void Sort();                    // +0x04 0x182ac0 (the scenario manager's 0x1b02f6)
    virtual void MoveDown();                // +0x0c 0x182acb (the manager's 0x1b0cde(mL04 + 1))
    // +0x10 0x182aed: the manager's 0x1b0fb5(mL04 + 1), then mTypes[mType]->VirtualC(mControls); bool only because the
    // base slot is (EESETSCondition's returns the manager's bool); this body leaves eax unset (OPEN)
    virtual void Delete();
    virtual void Deactivate();              // +0x14 0xe123d (shared `ret`)
    virtual void Duplicate();               // +0x18 0x182b11 (the manager's 0x1b15a7(mL04 + 1))
    virtual void GetNames(std::vector<UWideString>& outNames);      // +0x1c 0x182c43 (the manager's 0x1b3726(inArg))
    virtual void Activate();                // +0x20 0x182b22
    virtual void Store();                   // +0x24 0x182b96
    virtual void Hide();                    // +0x28 0x17c5d9 (shared with EESETriggerScreen +0x2c)
    virtual void Load();                    // +0x2c 0x182c55 (shows the effect: Virtual14/Virtual10 of its type)
    virtual void MoveUp();                  // +0x30 0x182adc (the manager's 0x1b3edd(mL04 + 1))
    virtual void New();                     // +0x34 0x182d35 (the manager's 0x1b099c, a tail jump)
    virtual void Show();                    // +0x38 0x182d69 (shared with EESETriggerScreen +0x34)
    virtual void RefreshControls();         // +0x44 0x182d40 (mTypes[mType]->Virtual18(effect, mControls))

    void ChangeEffectAction();              // 0x182a3b (EESEEChangeEffectAction)
    void ChangeEffectType();                // 0x182a69 (EESEEChangeEffectType)
    void TypeCall(unsigned long inSlot, EESMEffect* inEffect);   // the current type's slot (EE_SCREENS_SEET_*)

    unsigned long mType;                    // +0x0c (the current type, kSMEffect*; 1 in the ctor)
    EeScreensSeEffects* mTypes;             // the seven effect types (0 NoEffect, 1 Object, 2 Player, 3 Trigger, 4 Media, 5 Game, 6 AI)
    EESETSEffectControls* mControls;        // +0x2c (new 0xb8)
    UITextLabel* mTitleLabel;               // +0x30 (id 0x960, text 0x7690)
    UIComboBox* mTypeComboBox;              // +0x34 (id 0x961; six strings 0x7289..; trigger event 0x3eb)
};

#endif
