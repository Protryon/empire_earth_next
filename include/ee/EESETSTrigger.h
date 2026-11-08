// C:\EEX\Empire Earth\Scenario Editor\EESETSTrigger.h (RECONSTRUCTION; file name inferred). The trigger screen's
// trigger sub-screen (RTTI EESETSTrigger > EESETSBaseObject; vtable 0x4447ac, sizeof 0xac, ctor 0x19393b, ??_G
// 0x195fc2, dtor 0x195fde; EESETSTrigger.cpp 0x19393b..0x196ebe). It edits the
// scenario manager's trigger mL04 (EEScenarioManager::GetTrigger): two condition rows (mLA4, mLA4 + 1) and two effect
// rows (mLA8, mLA8 + 1), each a not/value check box, an entry combo box and an operator combo box. Layout from the
// ctor (control ids 0x9c4..0x9e8) and the dtor; names by offset (provisional names).
// Requires ee/EESETSBaseObject.h.
#ifndef EE_EESETSTRIGGER_H
#define EE_EESETSTRIGGER_H

class EESEETriggerOperator;
class EESETSTriggerUIComboBox;
class GEFont;
class UIButton;
class UICheckBox;
class UIComboBox;
class UIEditBox;
class UIForm;
class UITextLabel;

class EESETSTrigger : public EESETSBaseObject {
public:
    EESETSTrigger(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex);   // 0x19393b
    virtual ~EESETSTrigger();               // +0x00 0x195fde
    virtual void Sort();                    // +0x04 0x1961d4 (SortTriggers)
    virtual void MoveDown();                // +0x0c 0x1961df (MoveTriggerDown)
    virtual void Delete();                  // +0x10 0x1961fd (DeleteTrigger)
    virtual void Deactivate();              // +0x14 (EESETSCondition's shared `ret` 0xe123d)
    virtual void Duplicate();               // +0x18 0x1962b0 (DuplicateTrigger)
    virtual void GetNames(std::vector<UWideString>& outNames);      // +0x1c 0x196725
    virtual void Activate();                // +0x20 0x1962cd (fills the entry combo boxes)
    virtual void Store();                   // +0x24 0x196415 (writes the controls into the trigger)
    virtual void Hide();                    // +0x28 0x196737 (hides the controls)
    virtual void Load();                    // +0x2c 0x196785 (shows the trigger)
    virtual void MoveUp();                  // +0x30 0x1961ee (MoveTriggerUp)
    virtual void New();                     // +0x34 0x196c50 (NewTrigger)
    virtual void Show();                    // +0x38 0x196d94 (shows the controls)

    void UpdateSecondConditionRow(bool inSingle);  // 0x19621a: the second condition row's controls hidden when inSingle
    void UpdateSecondEffectRow(bool inSingle);  // 0x196265: the second effect row's controls
    void ChangeOperator(unsigned long inControlID);  // 0x19666d (EESEETriggerOperator: the control's id)
    void RemoveEntry(unsigned long inControlID);    // 0x196c69 (EESEETriggerRemove: the control's id)
    void ShowOperator(UIComboBox* inComboBox, long inOperator);    // 0x196d15 (the operator combo box)
    // inline (COMDATs 0x16fdfc, 0x16fe13, 0x16fe2a, 0x16fe41: EESEETriggerChangeIndex calls them out of line;
    // declaration order = their emission order): step the indices at +0xa4/+0xa8 between slots +0x24 and +0x2c
    void DecrementA4() { this->Store(); this->mFirstCondition--; this->Load(); }
    void DecrementA8() { this->Store(); this->mFirstEffect--; this->Load(); }
    void IncrementA4() { this->Store(); this->mFirstCondition++; this->Load(); }
    void IncrementA8() { this->Store(); this->mFirstEffect++; this->Load(); }

    UITextLabel* mIfLabel;                  // +0x0c (id 0x9c4)
    UITextLabel* mThenLabel;                // +0x10 (0x9c5)
    UITextLabel* mTriggerStatementLabel;    // +0x14 (0x9c6)
    UITextLabel* mFirstDelayLabel;          // +0x18 (0x9c7)
    UITextLabel* mSecondDelayLabel;         // +0x1c (0x9c8)
    UITextLabel* mFirstNotLabel;            // +0x20 (0x9c9)
    UITextLabel* mSecondNotLabel;           // +0x24 (0x9ca)
    UITextLabel* mSecondsLabel;             // +0x28 (0x9cb: the timer's)
    UITextLabel* mFirstConditionLabel;      // +0x2c (0x9e4: "condition n")
    UITextLabel* mSecondConditionLabel;     // +0x30 (0x9e5: "condition n + 1")
    UITextLabel* mFirstEffectLabel;         // +0x34 (0x9e6: "effect n")
    UITextLabel* mSecondEffectLabel;        // +0x38 (0x9e7: "effect n + 1")
    UICheckBox* mFirstNotCheckBox;          // +0x3c (0x9cc: condition n's not flag)
    UICheckBox* mSecondNotCheckBox;         // +0x40 (0x9cd)
    EESETSTriggerUIComboBox* mFirstConditionComboBox;  // +0x44 (0x9ce: condition n)
    EESETSTriggerUIComboBox* mSecondConditionComboBox;  // +0x48 (0x9cf)
    UIComboBox* mFirstConditionOperatorComboBox;  // +0x4c (0x9d0: condition n's operator)
    UIComboBox* mSecondConditionOperatorComboBox;  // +0x50 (0x9d1)
    UIEditBox* mFirstDelayEditBox;          // +0x54 (0x9d2: effect n's value)
    UIEditBox* mSecondDelayEditBox;         // +0x58 (0x9d3)
    EESETSTriggerUIComboBox* mFirstEffectComboBox;  // +0x5c (0x9d4: effect n)
    EESETSTriggerUIComboBox* mSecondEffectComboBox;  // +0x60 (0x9d5)
    UIComboBox* mFirstEffectOperatorComboBox;  // +0x64 (0x9d6: effect n's operator)
    UIComboBox* mSecondEffectOperatorComboBox;  // +0x68 (0x9d7)
    UIButton* mRemoveFirstConditionButton;  // +0x6c (0x9d8..0x9db: remove; EESEETriggerRemove)
    UIButton* mRemoveSecondConditionButton;  // +0x70
    UIButton* mRemoveFirstEffectButton;     // +0x74
    UIButton* mRemoveSecondEffectButton;    // +0x78
    UIButton* mPrevConditionsButton;        // +0x7c (0x9dc..0x9df: index steps; EESEETriggerChangeIndex)
    UIButton* mNextConditionsButton;        // +0x80
    UIButton* mPrevEffectsButton;           // +0x84
    UIButton* mNextEffectsButton;           // +0x88
    UICheckBox* mIsOnCheckBox;              // +0x8c (0x9e0)
    UICheckBox* mConditionTimerCheckBox;    // +0x90 (0x9e1: the timer; EESEETriggerConditionTimer)
    UICheckBox* mInCinematicCheckBox;       // +0x94 (0x9e8)
    UIEditBox* mConditionDurationEditBox;   // +0x98 (0x9e2: the timer's value)
    UICheckBox* mLoopingCheckBox;           // +0x9c (0x9e3)
    EESEETriggerOperator* mOperatorEvent;   // +0xa0 (the operator combo boxes' trigger event)
    long GetFirstCondition() const { return this->mFirstCondition; }
    long mFirstCondition;                   // +0xa4 (an index: EESEETriggerChangeIndex 0x9dc/0x9dd step it)
    long mFirstEffect;                      // +0xa8 (an index: 0x9de/0x9df)
};

#endif
