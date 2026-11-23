// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGStatusBox.h (RECONSTRUCTION; file name inferred).
// The main group's status box (sizeof 0xb0 from the `new` in EEUIFMGMainGroup's ctor 0x267ec8; ctor 0x26ada8; the dtor
// is ICF-folded into the empty 0xe123d; no vtable seen). Only what EEUIFMGMainGroup.cpp uses; (provisional names)
#ifndef EE_EEUIFMGSTATUSBOX_H
#define EE_EEUIFMGSTATUSBOX_H

#include "rust/game/screens.h"   // EeScreensStatusBoxFields
#include <stdint.h>
#include "ee/EEImprovementIDs.h"

class EEComplexUnit;
class EEUnit;
class UIControl;
class UIForm;


// the members are the Rust record EeScreensStatusBoxFields' (ee_screens main_form_fields.rs), the class's only base:
// the box's pointer is the record's (status_box.rs reads it through the call record)
class EEUIFMGStatusBox : public EeScreensStatusBoxFields {
public:
    EEUIFMGStatusBox(UIForm* inForm);
    ~EEUIFMGStatusBox() {}                    // (ICF-folded into the empty 0xe123d)
    void Fill(EEUnit* inUnit);              // 0x26b593 (the main group's new selection)
    void HideBackgrounds();                 // 0x26c470 (before the main group fills the controls in)
    void Refresh(EEUnit* inUnit);           // 0x26cbe1 (each update of the same selection)
    void UpdateQueuedCounts(EEComplexUnit* inUnit, long inImprovement);
    // the rest of EEUIFMGStatusBox.cpp (provisional names; FillArmorAttributes, UpdateArmorTexts and GetAttributeText
    // are Rust's: ee_screens status_box.rs)
    static void LoadIcons();                // 0x26c4fe (the 50 icon bitmaps of the three unit improvement textures)
    // Releases the 50 attribute icon bitmaps
    // (status_box_calls.rs)
    static void UnloadIcons() { ee_screens_sb_unload_icons(); }
    // Reloads the three unit improvement textures (after a device loss); returns the first error
    // (status_box_calls.rs)
    static long ReloadIcons() { return (long)ee_screens_sb_reload_icons(); }              // 0x26dcd9 (ReloadTexture of the three textures)
    // (0x564ed8, the 50 loaded icons, a class static in the original: Rust's, ee_screens_icon_bitmap, hud_fields.rs)
    // (the host of ee_screens status_box.rs is Rust over EEUIFMGStatusBox.cpp's shims: status_box_calls.rs)
    // The control an EE_SCREENS_BOX_CTL_* names
    // (the record's slots)
    UIControl* GetBoxControl(int64_t inControl) const
    {
        return static_cast<UIControl*>(ee_screens_status_box_control(this, inControl));
    }  // an EE_SCREENS_BOX_CTL_*
    // inline (EEUIFMGMainGroup::Open loads the label first: the arguments are evaluated right to left)
    void SetControls(long inIndex, UIControl* inIcon, UIControl* inLabel)
    {
        this->mIcons[inIndex] = inIcon;
        this->mLabels[inIndex] = inLabel;
    }
};

#endif
