// C:\EEX\Low-Level Engine\User Interface\UIScrollButton.h (RECONSTRUCTION). One arrow button of a scroll bar: while
// held it steps the slider once, then (after a 500 ms delay) every 100 ms. Only UIScrollBar creates it (private
// ctor, exported AAE). UIScrollButton.cpp (0x1007d013..0x1007d139) holds the ctor and ButtonDown; the inline
// members are emitted first by UICheckBox.cpp (0x1005f272..0x1005f34f). Vtable 0x10098e74 (UIButton's). Layout
// proven by the implicit copy ctor 0x1005f2bf (sizeof 0x114). Requires lle/UIButton.h, lle/UISlider.h and
// lle/TMPoll.h.
//
// The class is whole in Rust (rust/crates/ee_ui/src/variantobj.rs, R6 ui round 12): its record (the step direction,
// the slider, the two polls, which the Rust button makes and deletes) is the Rust button's, and its methods forward.
#ifndef LLE_UISCROLLBUTTON_H
#define LLE_UISCROLLBUTTON_H

class LLE_API UIScrollButton : public UIButton {
    friend class UIScrollBar;
    friend struct UIScrollBarRust;          // the Rust scroll bar logic's host calls (User Interface/UIScrollBar.cpp)
private:
    UIScrollButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inPressedBitmap,
                   const GE2DPoint& inLocation, UIForm* inForm, UISlider* inSlider, unsigned long inDecrement,
                   unsigned long inIndex);
    // the dtor (0x1005f272) deleted the two polls: the Rust button's release does
    virtual void ButtonDown();
    virtual void ButtonUp();                // 0x1005f2a6: the polls stopped, then UIButton's
    virtual bool GetTabable() const { return false; }   // ICF-folded (0x1005efa4)
    virtual long UpdateKeyboard() { return kErrorOK; }   // ICF-folded (0x1005d3d1)
    // the original's members (+0x104 mDecrement: nonzero steps the slider by -1, +0x108 mSlider, +0x10c mRepeatPoll
    // 100 ms, +0x110 mDelayPoll 500 ms before repeating; sizeof 0x114) are the Rust button's (variantobj.rs)
};

#endif
