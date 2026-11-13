// C:\EEX\Low-Level Engine\User Interface\UISlider.h (RECONSTRUCTION). A thumb (the UI drag-bar bitmap) moving along
// a horizontal (style 0) or vertical (style 1) track; the scroll bar's middle part. Vtable 0x10098ddc (UIControl's
// 34 slots, then ButtonDown, ButtonUp, ChangeRange). The original's sizeof was 0xf8 (the implicit copy ctor
// 0x1005efdc; the object is never copied).
//
// The class is whole in Rust (rust/crates/ee_ui/src/sliderobj.rs, R6 ui round 10): its state and every method are
// the Rust object mRust; UISlider.cpp holds the forwarders, the ctor and dtor and the host (the op switch). The
// virtuals stay virtual: EEUIFSEESElevation overrides ChangeRange. Requires lle/UIControl.h.
#ifndef LLE_UISLIDER_H
#define LLE_UISLIDER_H

#include "lle/GETextureCoordinate.h"
#include "lle/GEBitmap.h"      // the units including this header rely on it

struct EeUiSlider;

class LLE_API UISlider : public UIControl {
    friend class UIScrollBar;               // its host calls the protected page steps and mouse handlers
    friend struct UIScrollBarRust;          // UIScrollBar.cpp: the scroll bar's host
    friend struct UISliderHost;             // UISlider.cpp: the Rust object's calls out
public:
    UISlider(unsigned long inId, const GERectangle& inRectangle, unsigned long inSliderStyle, UIForm* inForm,
             unsigned long inIndex, unsigned long inBorderThickness);
    UISlider(const UISlider&) = delete;
    UISlider& operator=(const UISlider&) = delete;
    virtual ~UISlider();
    virtual long CopyData();
    virtual long DrawPolygons();
    long GetCurrentRangeValue() const;
    long GetMaxRange() const;
    unsigned long GetSliderStyle() const;
    virtual bool GetTabable() const { return false; }
    void SetCurrentRangeValue(long inValue);
    virtual void SetLocation(const GE2DPoint& inLocation);
    void SetPageSize(unsigned long inPageSize);
    void SetRange(unsigned long inMin, unsigned long inMax);
    virtual long UpdateKeyboard() { return kErrorOK; }
protected:
    virtual void ButtonDown() {}            // slot 0x88
    virtual void ButtonUp();                // slot 0x8c
    virtual void ChangeRange(unsigned long inValue);   // slot 0x90
    void DecrementByPageSize();
    void IncrementByPageSize();
    virtual long MouseLButtonDown();
    virtual long MouseLButtonUp();
    virtual long MouseMove(const GE2DPoint& inLocation);
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void TabbedFrom() {}
    virtual void TabbedTo() {}

    EeUiSlider* mRust;                      // the state (sliderobj.rs)
};

#endif
