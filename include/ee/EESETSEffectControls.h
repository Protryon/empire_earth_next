// C:\EEX\Empire Earth\Scenario Editor\EESETSEffectControls.h (RECONSTRUCTION; class and header names inferred, by
// analogy with ee/EESETSConditionControls.h; was the placeholder EESETSETGameUI). The
// controls of the effect screen's types: not polymorphic (no RTTI); sizeof 0xb8 = EESETSEffect's `new` (its +0x2c);
// ctor 0x181792 (the first function of EESETSEComboBox.cpp), dtor 0x1824fc (EESETSEffect's dtor 0x1829a4 calls it
// before `delete`). Member types from the ctor's `new` + ctor (+ the vtable stored) per array, each filled by a loop
// whose bound gives the count; the ctor fills them in the order +0x00, +0xa8, +0x4c, +0x5c, +0x64, +0x34, +0x3c,
// +0x44, +0x48, +0x80, +0x98, +0x74, +0xb0. The effect types (ee_screens editor_effects.rs) address it by array and index.
// Member names are provisional.
#ifndef EE_EESETSEFFECTCONTROLS_H
#define EE_EESETSEFFECTCONTROLS_H

class EESETSEComboBox;
class EESETSEScenarioComboBox;
class EESETSUICBPlayer;
class GEFont;
class UICheckBox;
class UIComboBox;
class UIEditBox;
class UIForm;
class UILBString;
class UIRadioButton;
class UISlider;
class UITextLabel;

extern GEFont* gU531934;                    // 0x531934 (also ee/EESETSConditionControls.h; the check boxes' font)
extern GEFont* gU531938;                    // 0x531938 (a font of Empire Earth.cpp's globals; the ambient weather check box)

class EESETSEffectControls {
public:
    // the constructor (0x181792) and destructor (0x1824fc) are ee_screens editor_controls.rs's tables: the members
    // made, stored (EE_SCREENS_SEET_OP_SLOT) and deleted there; the class is made empty and deleted after them

    UITextLabel* mLabels[13];               // +0x00 (new 0x124, vtable 0x442d34)
    UIEditBox* mEditBoxes[2];               // +0x34 (new 0x1e0, vtable 0x442c98; all four edit-box members)
    UIEditBox* mEditBoxes2[2];              // +0x3c (the loop builds [0] and [1] with different rectangles)
    UIEditBox* mTextEditBox;                // +0x44
    UIEditBox* mEditBox48;                  // +0x48
    UIComboBox* mComboBoxes[4];             // +0x4c (new 0xe0, vtable 0x443424)
    EESETSEComboBox* mEffectComboBoxes[2];  // +0x5c (new 0xe0, ctor 0x182445)
    EESETSEScenarioComboBox* mScenarioComboBoxes[4];   // +0x64 (new 0xe8, ctor 0x17e291)
    EESETSUICBPlayer* mPlayers[3];          // +0x74 (new 0xe8, ctor 0x196ebe)
    UIRadioButton* mRadioButtons[6];        // +0x80 (new 0x114, vtable 0x443624)
    UISlider* mSliders[4];                  // +0x98 (new 0xf8, vtable 0x44336c)
    UICheckBox* mCheckBoxes[2];             // +0xa8 (new 0x10c, vtable 0x4431f8)
    UILBString* mListBoxes[2];              // +0xb0 (new 0x13c, vtable 0x4436dc)
};

#endif
