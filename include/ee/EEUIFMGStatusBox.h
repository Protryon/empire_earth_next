// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGStatusBox.h (RECONSTRUCTION; file name inferred).
// The main group's status box (sizeof 0xb0 from the `new` in EEUIFMGMainGroup's ctor 0x267ec8; ctor 0x26ada8; the dtor
// is ICF-folded into the empty 0xe123d; no vtable seen). Only what EEUIFMGMainGroup.cpp uses; (provisional names)
#ifndef EE_EEUIFMGSTATUSBOX_H
#define EE_EEUIFMGSTATUSBOX_H

#include "rust/game/screens.h"   // EeScreensStatusBoxFields
#include <stdint.h>
#include "ee/EEImprovementIDs.h"

class EEComplexUnit;
class EEUIFMainGame;
class EEUnit;
class UIControl;
class UIForm;
class UWideString;

struct EEUIFMGIconBitmap;   // EEUIFMGStatusBox.cpp (one loaded icon, 0x14 bytes)

// the members are the Rust record EeScreensStatusBoxFields' (ee_screens main_form_fields.rs), the class's only base:
// the box's pointer is the record's (status_box.rs reads it through the call record)
class EEUIFMGStatusBox : public EeScreensStatusBoxFields {
public:
    EEUIFMGStatusBox(UIForm* inForm);
    ~EEUIFMGStatusBox();                    // (ICF-folded into the empty 0xe123d)
    void Fill(EEUnit* inUnit);              // 0x26b593 (the main group's new selection)
    void HideBackgrounds();                 // 0x26c470 (before the main group fills the controls in)
    void Refresh(EEUnit* inUnit);           // 0x26cbe1 (each update of the same selection)
    void UpdateQueuedCounts(EEComplexUnit* inUnit, long inImprovement);
    // the rest of EEUIFMGStatusBox.cpp (provisional names; FillArmorAttributes, UpdateArmorTexts and GetAttributeText
    // are Rust's: ee_screens status_box.rs)
    static void LoadIcons();                // 0x26c4fe (the 50 icon bitmaps of the three unit improvement textures)
    static void UnloadIcons();
    static long ReloadIcons();              // 0x26dcd9 (ReloadTexture of the three textures)
    // 0x564ed8: the 50 loaded icons, a class static (its atexit dtor is guarded: bit of 0x564ed0)
    // (provisional name)
    static EEUIFMGIconBitmap sIconBitmaps[50];
    // ee_screens status_box.rs's host (a member: the status bar's SetIconBitmap is protected, the box its friend)
    static int64_t HostOp(void* inCall, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC);
    UIControl* GetBoxControl(int64_t inControl) const;  // an EE_SCREENS_BOX_CTL_*
    // inline (EEUIFMGMainGroup::Open loads the label first: the arguments are evaluated right to left)
    void SetControls(long inIndex, UIControl* inIcon, UIControl* inLabel)
    {
        this->mIcons[inIndex] = inIcon;
        this->mLabels[inIndex] = inLabel;
    }
};

#endif
