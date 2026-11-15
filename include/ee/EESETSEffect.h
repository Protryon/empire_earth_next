// C:\EEX\Empire Earth\Scenario Editor\EESETSEffect.h (RECONSTRUCTION; file name inferred). The trigger screen's
// effect sub-screen (RTTI EESETSEffect > EESETSBaseObject; vtable 0x443c78, sizeof 0x38, ctor 0x182629, ??_G
// 0x182988, dtor 0x1829a4; EESETSEffect.cpp 0x182629..0x182dda), the twin
// of EESETSCondition: it edits the scenario manager's effect mL04 + 1 (EEScenarioManager::GetEffect) through the
// current effect type and the shared controls. Whole in Rust (ee_screens editor_subscreens.rs: its controls, title,
// type combo box, store, load, names; its seven effect types editor_effects.rs): the class holds the object and its
// members forward. Slot addresses from the vtable; provisional names. Requires ee/EESETSBaseObject.h.
#ifndef EE_EESETSEFFECT_H
#define EE_EESETSEFFECT_H

struct EeScreensSeEffectScreen;
class EESETSEffectControls;
class GEFont;
class UIComboBox;
class UIForm;

class EESETSEffect : public EESETSBaseObject {
public:
    EESETSEffect(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex);   // 0x182629
    virtual ~EESETSEffect();                // +0x00 0x1829a4 (deletes the controls, the label, the combo, the types)
    virtual void Sort();                    // +0x04 0x182ac0 (the scenario manager's 0x1b02f6)
    virtual void MoveDown();                // +0x0c 0x182acb (the manager's 0x1b0cde(mL04 + 1))
    // +0x10 0x182aed: the manager's 0x1b0fb5(mL04 + 1), then the type's hide; bool only because the base slot is
    // (EESETSCondition's returns the manager's bool); this body leaves eax unset (OPEN)
    virtual void Delete();
    virtual void Deactivate();              // +0x14 0xe123d (shared `ret`)
    virtual void Duplicate();               // +0x18 0x182b11 (the manager's 0x1b15a7(mL04 + 1))
    virtual void GetNames(std::vector<UWideString>& outNames);      // +0x1c 0x182c43 (the manager's 0x1b3726(inArg))
    virtual void Activate();                // +0x20 0x182b22
    virtual void Store();                   // +0x24 0x182b96
    virtual void Hide();                    // +0x28 0x17c5d9 (shared with EESETriggerScreen +0x2c)
    virtual void Load();                    // +0x2c 0x182c55 (shows the effect: its type's hide and show)
    virtual void MoveUp();                  // +0x30 0x182adc (the manager's 0x1b3edd(mL04 + 1))
    virtual void New();                     // +0x34 0x182d35 (the manager's 0x1b099c, a tail jump)
    virtual void Show();                    // +0x38 0x182d69 (shared with EESETriggerScreen +0x34)
    virtual void RefreshControls();         // +0x44 0x182d40 (the type's UpdateName)

    void ChangeEffectAction();              // 0x182a3b (EESEEChangeEffectAction)
    void ChangeEffectType();                // 0x182a69 (EESEEChangeEffectType)
    EESETSEffectControls* GetControls() const;   // the controls the types share (the editor gate)
    UIComboBox* GetTypeComboBox() const;         // the type combo box (the editor gate)
    void Call(unsigned long inWhat);        // the screen's slot (EE_SCREENS_SESC_*) with the selection

    EeScreensSeEffectScreen* mRust;         // the screen (editor_subscreens.rs)
};

#endif
