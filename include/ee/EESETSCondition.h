// C:\EEX\Empire Earth\Scenario Editor\EESETSCondition.h (RECONSTRUCTION; file name inferred). The trigger screen's
// condition sub-screen (RTTI EESETSCondition > EESETSBaseObject; vtable 0x443ac0, sizeof 0x3c, ctor 0x17e49e, ??_G
// 0x17e80b, dtor 0x17e827; EESETSCondition.cpp 0x17e49e..0x17ef10). It edits the
// scenario manager's condition mL04 + 1 (EEScenarioManager::GetCondition): the name/text edit boxes of the trigger
// screen, a type combo box (mTypeComboBox, id 0x8fd; types 2.. are mTypes[selection + 2]) and the current type's
// controls (mControls). Slot names by offset; provisional names. Requires ee/EESETSBaseObject.h.
// The seven condition types are one Rust object (ee_screens editor_conditions.rs). (EESETSConditionControls.h only
// for the slot bodies.)
#ifndef EE_EESETSCONDITION_H
#define EE_EESETSCONDITION_H

class UIComboBox;
class UITextLabel;
class EESETSConditionControls;
class EESMCondition;
struct EeScreensSeConditions;
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
    void TypeCall(unsigned long inSlot, EESMCondition* inCondition);   // the current type's slot (EE_SCREENS_SEET_*)
    EESETSConditionControls* GetControls() const { return this->mControls; }   // the editor gate
    UIComboBox* GetTypeComboBox() const { return this->mTypeComboBox; }       // the editor gate

    unsigned long mType;                    // +0x0c (the current mTypes index; 1 in the ctor)
    EeScreensSeConditions* mTypes;          // the seven condition types (ee_screens editor_conditions.rs; the
                                            // original's mTypes[8] at +0x10, [1] never set)
    EESETSConditionControls* mControls;     // +0x30
    UITextLabel* mTitleLabel;               // +0x34 (id 0x8fc)
    UIComboBox* mTypeComboBox;              // +0x38 (id 0x8fd)
};

#endif
