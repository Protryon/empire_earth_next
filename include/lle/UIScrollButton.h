// C:\EEX\Low-Level Engine\User Interface\UIScrollButton.h (RECONSTRUCTION). One arrow button of a scroll bar: while
// held it steps the slider once, then (after a 500 ms delay) every 100 ms. Only UIScrollBar creates it (private
// ctor, exported AAE). UIScrollButton.cpp (0x1007d013..0x1007d139) holds the ctor and ButtonDown; the inline
// members are emitted first by UICheckBox.cpp (0x1005f272..0x1005f34f). Vtable 0x10098e74 (UIButton's). Layout
// proven by the implicit copy ctor 0x1005f2bf (sizeof 0x114). Requires lle/UIButton.h, lle/UISlider.h and
// lle/TMPoll.h.
#ifndef LLE_UISCROLLBUTTON_H
#define LLE_UISCROLLBUTTON_H

class LLE_API UIScrollButton : public UIButton {
    friend struct UIButtonRust;             // the Rust button logic's host calls (User Interface/UIButtonRust.h)
    friend class UIScrollBar;
    friend struct UIScrollBarRust;          // the Rust scroll bar logic's host calls (User Interface/UIScrollBar.cpp)
private:
    UIScrollButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inPressedBitmap,
                   const GE2DPoint& inLocation, UIForm* inForm, UISlider* inSlider, unsigned long inDecrement,
                   unsigned long inIndex);
    // 0x1005f272
    virtual ~UIScrollButton()
    {
        delete this->mRepeatPoll;
        delete this->mDelayPoll;
    }
    virtual void ButtonDown();
    // 0x1005f2a6
    virtual void ButtonUp()
    {
        this->mRepeatPoll->Stop();
        this->mDelayPoll->Stop();
        UIButton::ButtonUp();
    }
    virtual bool GetTabable() const { return false; }   // ICF-folded (0x1005efa4)
    virtual long UpdateKeyboard() { return kErrorOK; }   // ICF-folded (0x1005d3d1)

    unsigned long mDecrement;               // +0x104 nonzero: steps the slider by -1
    UISlider* mSlider;                      // +0x108
    TMPoll* mRepeatPoll;                    // +0x10c 100 ms
    TMPoll* mDelayPoll;                     // +0x110 500 ms before repeating
};

#endif
