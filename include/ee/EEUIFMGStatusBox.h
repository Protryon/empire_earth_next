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
#include "lle/LLEForward.h"
#include "lle/GETextureManager.h"   // GETextureDescription (the call record's)

// The host's record of one status box call (ee_screens status_box.rs): the box, the unit (Fill, Refresh), the
// complex unit (UpdateQueuedCounts), the held text (the original's `theText`), LoadIcons' description and textures
struct EEUIFMGStatusBoxCall : public EeScreensStatusBoxCallObjects {   // the pointers: the Rust record (hud_fields.rs)
    UWideString mText;
    GETextureDescription mDescription;
    // the record of a call (a temporary's, for the full expression)
    EEUIFMGStatusBoxCall* Ptr() { return this; }
};
static_assert(offsetof(EEUIFMGStatusBoxCall, mText) == sizeof(EeScreensStatusBoxCallObjects), "hud_direct.rs BoxCallHead");


// the members are the Rust record EeScreensStatusBoxFields' (ee_screens main_form_fields.rs), the class's only base:
// the box's pointer is the record's (status_box.rs reads it through the call record)
class EEUIFMGStatusBox : public EeScreensStatusBoxFields {
public:
    EEUIFMGStatusBox(UIForm* inForm) { ee_screens_status_box_init(this, inForm); }   // (main_form_fields.rs)
    ~EEUIFMGStatusBox() {}                    // (ICF-folded into the empty 0xe123d)
    // 0x26b593 (the main group's new selection): fills the status box for a unit: name (gate lock state),
    // civilization, hit point / power / flight / garrison bars, the improvable attribute buttons and the background
    // panels; clears it without a unit
    void Fill(EEUnit* inUnit) { ee_screens_status_box_fill(EEUIFMGStatusBoxCall{{this, inUnit, NULL}}.Ptr(), inUnit != NULL); }
    // 0x26c470 (before the main group fills the controls in): hides the background panels (all but panel 0x7c)
    void HideBackgrounds() { ee_screens_status_box_hide_backgrounds(EEUIFMGStatusBoxCall{{this, NULL, NULL}}.Ptr()); }
    // 0x26cbe1 (each update of the same selection): the texts for the unit: name, civilization, hit points,
    // attributes, cargo, flight time or power, garrison, and the resource it carries
    void Refresh(EEUnit* inUnit) { ee_screens_status_box_refresh(EEUIFMGStatusBoxCall{{this, inUnit, NULL}}.Ptr()); }
    // shows on each visible improvement button how many of its improvements the player has queued
    void UpdateQueuedCounts(EEComplexUnit* inUnit, long inImprovement)
    {
        ee_screens_status_box_update_queued(EEUIFMGStatusBoxCall{{this, NULL, inUnit}}.Ptr());
    }
    // the rest of EEUIFMGStatusBox.cpp (provisional names; FillArmorAttributes, UpdateArmorTexts and GetAttributeText
    // are Rust's: ee_screens status_box.rs)
    // 0x26c4fe: loads the three unit improvement textures and points the 50 attribute icons at their cells
    static void LoadIcons() { ee_screens_status_box_load_icons(EEUIFMGStatusBoxCall{{NULL, NULL, NULL}}.Ptr()); }
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
        ee_screens_status_box_set_controls(this, (int64_t)inIndex, inIcon, inLabel);   // (main_form_fields.rs)
    }
};

#endif
