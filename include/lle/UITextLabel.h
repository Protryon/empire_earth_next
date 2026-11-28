// C:\EEX\Low-Level Engine\User Interface\UITextLabel.h (RECONSTRUCTION). A text control: one line (label type 0)
// or word-wrapped lines with an optional vertical scroll bar (label type 1); the form's help text label is one.
// Vtable 0x10098f98 (UIControl's 34 slots, then AppendText, RecomputeDisplayRange, CheckScrollBar). The original's
// sizeof was 0x124 (the implicit copy ctor 0x1005f689; the object is never copied).
//
// The class is whole in Rust (rust/crates/ee_ui/src/labelobj.rs, R6 ui round 13): its state (the lines, the display
// copies, the rectangles, the scroll bar and its colours, the flags) and every method are the Rust object mRust;
// UITextLabel.cpp holds the forwarders, the ctor and dtor and the host (the op switch). The lines are still
// UWideStrings (GetText answers a reference), made and held by address by the Rust object. A UITLScrolling's own
// state is the same object's scrolling part. Requires lle/UIControl.h.
#ifndef LLE_UITEXTLABEL_H
#define LLE_UITEXTLABEL_H
#include "platform/PlatString.h"

#include <stdlib.h>
#include "lle/UIControl.h"
#include "lle/UIScrollBar.h"   // units including this header rely on it for the scroll bar
#include "rust/ui/labelobj.h"
#include "rust/ui/lle_hosts.h"


class LLE_API UITextLabel : public UIControl {
public:
    UITextLabel(const UWideString& inText, GEFont* inFont, unsigned long inId, const GERectangle& inRectangle, UIForm* inForm, unsigned long inLabelType, unsigned long inIndex, unsigned long inBorderThickness) : UIControl(kControlTypeTextLabel, inId, inRectangle, inForm, inIndex, inBorderThickness), mRust(ee_ui_label_new(this, ee_ui_label_host, UIF4(inRectangle), inFont, inLabelType, inBorderThickness)) { this->Call(EeUiLabelCall::Init, (intptr_t)(const char*)inText); }
    virtual ~UITextLabel() { ee_ui_label_free(this->mRust); }
    virtual void AppendText(const UWideString& inText) { this->SetText(inText, false); }   // slot 0x88
    virtual long CopyData() { return (long)this->Call(EeUiLabelCall::CopyData); }
    void DeleteLine(unsigned long inLine) { this->Call(EeUiLabelCall::DeleteLine, inLine); }
    virtual long DrawPolygons() { return (long)this->Call(EeUiLabelCall::DrawPolygons); }
    virtual long DrawTextA() { return (long)this->Call(EeUiLabelCall::DrawText); }
    GEText* GetGEText() const { return reinterpret_cast<GEText*>(this->Call(EeUiLabelCall::Text)); }
    unsigned long GetLineCount() const { return (unsigned long)this->Call(EeUiLabelCall::LineCount); }
    const UIScrollBar* GetScrollBar() const { return reinterpret_cast<const UIScrollBar*>(this->Call(EeUiLabelCall::ScrollBar)); } // NULL while it is marked removed
    virtual bool GetTabable() const { return false; }
    const UWideString& GetText(unsigned long inLine) const { return *reinterpret_cast<const UWideString*>(this->Call(EeUiLabelCall::Line, inLine)); }
    void SetAlignment(unsigned long inAlignment) { this->Call(EeUiLabelCall::SetAlignment, inAlignment); }
    void SetFitRectToText(bool inFit) { this->Call(EeUiLabelCall::SetFitRectToText, inFit); }
    virtual void SetLocation(const GE2DPoint& inLocation) { this->Call(EeUiLabelCall::SetLocation, 0, 0, inLocation.GetX(), inLocation.GetY()); }
    virtual void SetRectangle(const GERectangle& inRectangle) { this->CallRect(EeUiLabelCall::SetRectangle, inRectangle); }
    void SetStartingDisplayLine(unsigned long inLine) { this->Call(EeUiLabelCall::SetStartingDisplayLine, inLine); }
    void SetScrollBarBgOpacity(float inOpacity) { this->Call(EeUiLabelCall::SetScrollBarBgOpacity, 0, 0, inOpacity); }
    void SetText(const UWideString& inText) { this->SetText(inText, true); }
    void SetText(long inValue) { ee_ui_label_set_number(this->mRust, inValue); }   // _itoa's decimal
    virtual long UpdateKeyboard() { return kErrorFailed; }
    virtual long MouseLButtonDown() { return (long)this->Call(EeUiLabelCall::MouseLButtonDown); }
    virtual long MouseLButtonUp() { return (long)this->Call(EeUiLabelCall::MouseLButtonUp); }
    virtual long MouseMove(const GE2DPoint& inLocation) { return (long)this->Call(EeUiLabelCall::MouseMove, 0, 0, inLocation.GetX(), inLocation.GetY()); }
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void TabbedFrom() {}
    virtual void TabbedTo() {}
    void SetText(const UWideString& inText, bool inReplace) { this->Call(EeUiLabelCall::SetText, (intptr_t)(const char*)inText, inReplace); }
    intptr_t Call(EeUiLabelCall c, intptr_t a = 0, intptr_t b = 0, float f0 = 0.0f, float f1 = 0.0f, float f2 = 0.0f, float f3 = 0.0f) const { return ee_ui_label_call(this->mRust, c, a, b, UIF4(f0, f1, f2, f3)); }
    intptr_t CallRect(EeUiLabelCall c, const GERectangle& r) const { return ee_ui_label_call(this->mRust, c, 0, 0, UIF4(r)); }
    EeUiLabel* mRust;                       // the state (labelobj.rs)
};

#endif
