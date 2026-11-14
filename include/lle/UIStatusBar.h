// C:\EEX\Low-Level Engine\User Interface\UIStatusBar.h (RECONSTRUCTION). A progress bar: a coloured rhombus whose
// width follows a value in [min, max], with an optional centred text (control type 9). Vtable 0x100998d4 (UIControl's
// 34 slots, no new virtuals). The original's sizeof was 0x114 (the copy ctor; the object is never copied).
//
// The class is whole in Rust (rust/crates/ee_ui/src/statusobj.rs, R6 ui round 10): its state (the text areas and
// colour, the texts, the value and range) and every method are the Rust object mRust; the two rhombi stay members
// (GERhombus objects the rasterizer draws). UIStatusBar.cpp holds the forwarders, the ctor and dtor and the host.
// Requires lle/UIControl.h, lle/GERhombus.h, lle/GEText.h.
#ifndef LLE_UISTATUSBAR_H
#define LLE_UISTATUSBAR_H

#include "lle/UIControl.h"
#include "lle/GEText.h"

struct EeUiStatusBar;

class LLE_API UIStatusBar : public UIControl {
    friend struct UIStatusBarHost;          // UIStatusBar.cpp: the Rust object's calls out
public:
    UIStatusBar(unsigned long inId, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                unsigned long inBorderThickness);
    UIStatusBar(const UIStatusBar&) = delete;
    UIStatusBar& operator=(const UIStatusBar&) = delete;
    virtual ~UIStatusBar();
    virtual void ChangeFont(GEFont* inFont);
    virtual long CopyData();
    virtual long DrawPolygons();
    virtual long DrawTextA();
    long GetCurrentRangeValue() const;
    GEText* GetGEText();
    long GetMaxRange() const;
    unsigned long GetRange() const;
    virtual bool GetTabable() const { return false; }
    void SetCurrentRangeValue(long inValue);
    void SetFocusedTextColor(const GEColor& inColor);
    virtual void SetLocation(const GE2DPoint& inLocation);
    void SetRange(unsigned long inMin, unsigned long inMax);
    void SetStatusBarOpacity(float inOpacity);
    void SetStatusBarColor(const GEColor& inColor);
    void SetText(const UWideString& inText, GEFont* inFont);
    virtual long UpdateKeyboard() { return kErrorOK; }
protected:
    virtual long MouseLButtonDown() { return kErrorOK; }
    virtual long MouseLButtonUp() { return kErrorOK; }
    virtual long MouseMove(const GE2DPoint& inLocation) { return kErrorOK; }
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void TabbedTo() {}
    virtual void TabbedFrom() {}

    GERhombus mStatusRhombus;               // the bar
    GERhombus mCopiedStatusRhombus;         // the bar as of the last CopyData (inset by the border)
    EeUiStatusBar* mRust;                   // the state (statusobj.rs)
};

#endif
