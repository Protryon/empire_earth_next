// C:\EEX\Low-Level Engine\User Interface\UIScrollBar.h (RECONSTRUCTION). A UISlider between two UIScrollButtons
// (decrement at the top/left, increment at the bottom/right). Vtable 0x10098f0c (UIControl's 34 slots). The original's
// sizeof was 0xb0 (the implicit copy ctor 0x1005f417; the object is never copied).
//
// The class is whole in Rust (rust/crates/ee_ui/src/scrollbarobj.rs, R6 ui round 10): its state (the parts and
// which was pressed) and every method are the Rust object mRust; UIScrollBar.cpp holds the forwarders, the ctor and
// dtor and the host (the op switch, the parts' new and delete). Requires lle/UIControl.h (includes lle/UISlider.h).
#ifndef LLE_UISCROLLBAR_H
#define LLE_UISCROLLBAR_H

#include "lle/UISlider.h"

struct EeUiScrollBar;

// The width of a scroll bar (0x1009916c, one .rdata constant: the edit and list boxes, the scroll bar and the check
// box load it). Inferred home.
#define kScrollBarWidth 20.0f   // a #define: every user reads the one pooled literal (lle 0x1009916c, exe 0x43bff4 __real 20.0f; exe UIListBox::SetLocation 0x5ad1f2)

class LLE_API UIScrollBar : public UIControl {
    friend struct UIScrollBarRust;          // UIScrollBar.cpp: the Rust object's calls out
public:
    UIScrollBar(unsigned long inId, const GE2DPoint& inLocation, float inLength, unsigned long inStyle, UIForm* inForm,
                unsigned long inIndex, unsigned long inBorderThickness);
    UIScrollBar(const UIScrollBar&) = delete;
    UIScrollBar& operator=(const UIScrollBar&) = delete;
    virtual ~UIScrollBar();
    virtual long CopyData();
    virtual long DrawPolygons();
    virtual long DrawTextA();
    const UISlider* GetSlider() const;
    virtual bool GetTabable() const { return false; }
    void IncrementBy(long inAmount);
    virtual long MouseLButtonDown();
    virtual long MouseLButtonUp();
    virtual long MouseMove(const GE2DPoint& inLocation);
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    void SetCurrentRangeValue(unsigned long inValue);
    virtual void SetBgColor(const GEColor& inColor);
    virtual void SetBgOpacity(float inOpacity);
    virtual void SetLocation(const GE2DPoint& inLocation);
    void SetPageSize(unsigned long inPageSize);
    void SetRange(unsigned long inMin, unsigned long inMax);
    virtual void TabbedFrom() {}
    virtual void TabbedTo() {}
    virtual long UpdateKeyboard() { return kErrorOK; }
private:
    EeUiScrollBar* mRust;                   // the state (scrollbarobj.rs)
};

#endif
