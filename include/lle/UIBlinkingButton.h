// C:\EEX\Low-Level Engine\User Interface\UIBlinkingButton.h (RECONSTRUCTION). A UIButton that pulses its
// background, border and bitmap opacities while blinking. UIBlinkingButton.cpp (0x1005db0f..0x1005e47c) emits
// UIButton's and its own inline members first. Vtable 0x10098d44. sizeof 0x148 (??_E stride, 0x1005e061; the TMPoll
// member is 8-aligned at +0x108). Requires lle/UIButton.h, lle/TMPoll.h.
//
// The class is whole in Rust (rust/crates/ee_ui/src/variantobj.rs, R6 ui round 12): its record is the Rust button's
// and its methods forward to it.
#ifndef LLE_UIBLINKINGBUTTON_H
#define LLE_UIBLINKINGBUTTON_H

#include "lle/UIButton.h"

class LLE_API UIBlinkingButton : public UIButton {
public:
    UIBlinkingButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap,
                     GEBitmap* inPressedBitmap, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                     unsigned long inBorderThickness, unsigned long inBlinkRate);
    // implicit virtual dtor (0x1005e05c: a bare jmp to ~UIButton, no vptr store; exported)
    virtual long CopyData();
    virtual long DrawPolygons();            // ICF-folded with UIButton::DrawPolygons (0x1005eb30)
    virtual long DrawTextA();
    void SetIsBlinking(bool inBlinking);
    // the original's members (+0x108 TMPoll mPoll: one blink phase, +0x128 mBlinkPhase, +0x12c mBlinkRate, the full
    // opacities +0x130..+0x13c, +0x140 mSavedAlwaysDirty, +0x141 mNeedsOpacities, +0x142 mIsBlinking; sizeof 0x148)
    // are the Rust button's (variantobj.rs; the poll is now a heap object it makes and deletes)
};

#endif
