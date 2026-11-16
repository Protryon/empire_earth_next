// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGStatusBox.h (RECONSTRUCTION; file name inferred).
// The main group's status box (sizeof 0xb0 from the `new` in EEUIFMGMainGroup's ctor 0x267ec8; ctor 0x26ada8; the dtor
// is ICF-folded into the empty 0xe123d; no vtable seen). Only what EEUIFMGMainGroup.cpp uses; (provisional names)
#ifndef EE_EEUIFMGSTATUSBOX_H
#define EE_EEUIFMGSTATUSBOX_H
#include <stdint.h>
#include "ee/EEImprovementIDs.h"

class EEComplexUnit;
class EEUIFMainGame;
class EEUnit;
class UIControl;
class UIForm;
class UWideString;

struct EEUIFMGIconBitmap;   // EEUIFMGStatusBox.cpp (one loaded icon, 0x14 bytes)

class EEUIFMGStatusBox {
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

    long mAttributes[14];                   // +0x00 (= -1) the icon index of each attribute shown
    UIControl* mIcons[9];                   // +0x38 the main game form's controls 0x45..0x4d
    UIControl* mLabels[9];                  // +0x5c controls 0x20..0x28
    UIForm* mForm;                   // +0x80 (the ctor argument)
    UIControl* mC84;                        // +0x84 control 0x1e
    UIControl* mC88;                        // +0x88 control 0x1f
    UIControl* mC8C;                        // +0x8c control 0x2a
    UIControl* mC90;                        // +0x90 control 0x2b
    UIControl* mC94;                        // +0x94 control 0x64
    UIControl* mC98;                        // +0x98 control 0x29
    long mAttributeCount;                   // +0x9c
    long mArmorAttributeCount;              // +0xa0 the next free icon
    bool mIsResourceShown;                  // +0xa4
    UIControl* mCA8;                        // +0xa8 control 0x33
    UIControl* mCAC;                        // +0xac control 0x32
};

#endif
