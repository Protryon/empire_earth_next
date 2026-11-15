// C:\EEX\Empire Earth\Scenario Editor\EESETSConditionControls.h (RECONSTRUCTION; class and header names
// inferred; was EESETSCTPlayerResearchData). The controls of the condition screen's types
// (no RTTI: not polymorphic; sizeof 0x5c: EESETSCondition's `new`; ctor 0x17daa8, dtor 0x17e3d7: EESETSCComboBox.cpp).
// Member types from the ctor's `new` + ctor per array (filled by loops); the dtor deletes the arrays in the order
// +0x00, +0x20, +0x2c, +0x14, +0x1c, +0x3c, +0x50, +0x48, +0x58. Requires lle/UIEditBox.h.
#ifndef EE_EESETSCONDITIONCONTROLS_H
#define EE_EESETSCONDITIONCONTROLS_H

class EESETSCComboBox;
class EESETSEScenarioComboBox;
class EESETSUICBPlayer;
class UICheckBox;
class UIRadioButton;
class UISlider;
class UITextLabel;
class GEFont;
class UIForm;

extern GEFont* gU531934;                    // 0x531934 (the check boxes' font; a global of Empire Earth.cpp)
extern GEFont* gU531930;                    // 0x531930 (the chat text edit box's font; Empire Earth.cpp's globals)

class EESETSConditionControls {
public:
    EESETSConditionControls(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex);   // 0x17daa8
    ~EESETSConditionControls();             // 0x17e3d7

    UITextLabel* mLabels[5];                // +0x00
    UIEditBox* mEditBoxes[2];               // +0x14
    UIEditBox* mTextEditBox;                // +0x1c (EESETSCTChatMessage: the chat text)
    EESETSCComboBox* mComboBoxes[3];        // +0x20 (ctor 0x17e33d)
    EESETSEScenarioComboBox* mScenarioComboBoxes[4];   // +0x2c (ctor 0x17e291)
    UICheckBox* mCheckBoxes[3];             // +0x3c
    EESETSUICBPlayer* mPlayers[2];          // +0x48 (ctor 0x196ebe; EESETSCondition::Virtual20 sets their +0xe4)
    UIRadioButton* mRadioButtons[2];        // +0x50
    UISlider* mSlider;                      // +0x58
};

#endif
