// C:\EEX\Empire Earth\Scenario Editor\EESETSCondition.h (RECONSTRUCTION; file name inferred). The trigger screen's
// condition sub-screen (RTTI EESETSCondition > EESETSBaseObject; vtable 0x443ac0, sizeof 0x3c, ctor 0x17e49e, ??_G
// 0x17e80b, dtor 0x17e827; EESETSCondition.cpp 0x17e49e..0x17ef10). It edits the
// scenario manager's condition mL04 + 1 (EEScenarioManager::GetCondition): the name/text edit boxes of the trigger
// screen, a type combo box (id 0x8fd; types 2.. are the selection + 2) and the current type's controls. Whole in Rust
// (ee_screens editor_subscreens.rs: its controls, title, type combo box, store, load, names; its seven types
// editor_conditions.rs): the class holds the object and its members forward. Slot names by offset; provisional
// names. Requires ee/EESETSBaseObject.h.
#ifndef EE_EESETSCONDITION_H
#define EE_EESETSCONDITION_H

class UIComboBox;
class EESETSConditionControls;
struct EeScreensSeConditionScreen;
class GEFont;

class EESETSCondition : public EESETSBaseObject {
public:
    EESETSCondition(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex);   // 0x17e49e
    virtual ~EESETSCondition();             // +0x00 0x17e827
    virtual void Sort();                    // +0x04 0x17e91d
    virtual void MoveDown();                // +0x0c 0x17e928
    virtual void Delete();                  // +0x10 0x17e94a
    virtual void Deactivate();              // +0x14 0xe123d (shared `ret`)
    virtual void Duplicate();               // +0x18 0x17e95b
    virtual void GetNames(std::vector<UWideString>& outNames);      // +0x1c 0x17eab8
    virtual void Activate();                // +0x20 0x17e96c
    virtual void Store();                   // +0x24 0x17e9fb (writes the controls into the condition)
    virtual void Hide();                    // +0x28 0x17eaca (hides the screen's controls)
    virtual void Load();                    // +0x2c 0x17eae3 (shows the condition)
    virtual void MoveUp();                  // +0x30 0x17e939
    virtual void New();                     // +0x34 0x17ebb7
    virtual void Show();                    // +0x38 0x17edfc (shows the screen's controls)
    virtual void RefreshControls();         // +0x44 0x17ebc2

    void ChangeConditionType();             // 0x17e8be (EESEEChangeConditionType)
    EESETSConditionControls* GetControls() const;   // the controls the types share (the editor gate)
    UIComboBox* GetTypeComboBox() const;            // the type combo box (the editor gate)
    void Call(unsigned long inWhat);        // the screen's slot (EE_SCREENS_SESC_*) with the selection

    EeScreensSeConditionScreen* mRust;      // the screen (editor_subscreens.rs)
};

#endif
