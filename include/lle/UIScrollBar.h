// C:\EEX\Low-Level Engine\User Interface\UIScrollBar.h (RECONSTRUCTION). A UISlider between two UIScrollButtons
// (decrement at the top/left, increment at the bottom/right). UIScrollBar.cpp (0x1007c901..0x1007d013) holds the
// ctor and the out-of-line members; the inline members are emitted first by UICheckBox.cpp (0x1005f3b0..0x1005f538,
// in declaration order: SetCurrentRangeValue sits before SetBgOpacity). Vtable 0x10098f0c (UIControl's 34 slots).
// Layout proven by the implicit copy ctor 0x1005f417 (sizeof 0xb0). Requires lle/UIControl.h (includes lle/UISlider.h).
#ifndef LLE_UISCROLLBAR_H
#define LLE_UISCROLLBAR_H

#include "lle/UISlider.h"

class UIScrollButton;

// The width of a scroll bar (0x1009916c, one .rdata constant: the edit and list boxes, the scroll bar and the check
// box load it). Inferred home.
#define kScrollBarWidth 20.0f   // a #define: every user reads the one pooled literal (lle 0x1009916c, exe 0x43bff4 __real 20.0f; exe UIListBox::SetLocation 0x5ad1f2)

class LLE_API UIScrollBar : public UIControl {
    friend struct UIScrollBarRust;          // the Rust scroll bar logic's host calls (User Interface/UIScrollBar.cpp)
public:
    UIScrollBar(unsigned long inId, const GE2DPoint& inLocation, float inLength, unsigned long inStyle, UIForm* inForm,
                unsigned long inIndex, unsigned long inBorderThickness);
    virtual ~UIScrollBar();
    virtual long CopyData();
    void DecrementByPageSize() { this->mSlider->DecrementByPageSize(); }  // 0x1005f3b0
    virtual long DrawPolygons();
    virtual long DrawTextA();
    const UIScrollButton* GetDecrementButton() { return this->mDecrementButton; }  // 0x1005a10b (ICF)
    const UIScrollButton* GetIncrementButton() { return this->mIncrementButton; }  // 0x1006114b (ICF)
    const UISlider* GetSlider() const { return this->mSlider; }  // 0x10061130 (ICF; exported ?GetSlider@UIScrollBar@@QBEPBVUISlider@@XZ)
    virtual bool GetTabable() const { return false; }                // 0x1005efa4 (ICF)
    void IncrementBy(long inAmount);
    void IncrementByPageSize() { this->mSlider->IncrementByPageSize(); }  // 0x1005f3bb
    virtual long MouseLButtonDown();
    virtual long MouseLButtonUp();
    virtual long MouseMove(const GE2DPoint& inLocation);
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    void SetCurrentRangeValue(unsigned long inValue) { this->mSlider->SetCurrentRangeValue(inValue); }  // 0x1005f3c6
    virtual void SetBgColor(const GEColor& inColor) { this->mSlider->SetBgColor(inColor); }  // 0x10061166 (ICF)
    virtual void SetBgOpacity(float inOpacity) { this->mSlider->SetBgOpacity(inOpacity); }  // 0x1005f3d8
    virtual void SetLocation(const GE2DPoint& inLocation);
    void SetPageSize(unsigned long inPageSize) { this->mSlider->SetPageSize(inPageSize); }  // 0x1005f3ee
    void SetRange(unsigned long inMin, unsigned long inMax) { this->mSlider->SetRange(inMin, inMax); }  // 0x1005f401
    virtual void TabbedFrom() {}
    virtual void TabbedTo() {}
    virtual long UpdateKeyboard() { return kErrorOK; }
private:
    UIScrollButton* mIncrementButton;       // +0xa0 bottom/right
    UIScrollButton* mDecrementButton;       // +0xa4 top/left (at the bar's location)
    UISlider* mSlider;                      // +0xa8
    bool mIncrementPressed;                 // +0xac (the mouse went down on it)
    bool mDecrementPressed;                 // +0xad
    bool mSliderPressed;                    // +0xae
};

#endif
