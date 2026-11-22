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

class GEText;
struct EeUiLabel;

class LLE_API UITextLabel : public UIControl {
    friend class UIForm;                    // SetText(const UWideString&, bool) is private
    friend class UIESliderChange;           // ProcessEvent calls SetText(const UWideString&, bool) (0x1006e950)
    friend class UIComboBox;                // calls SetText(const UWideString&, bool) and MouseLButtonDown (ui-edit)
    friend class UICheckBox;                // SetText calls SetText(const UWideString&, bool) (0x1006013e)
    friend class UIRadioButton;             // likewise (0x1007c146)
    friend class UITriStateCheckBox;        // likewise (0x10080b1c)
    friend class UITLScrolling;             // its inline forwarders reach mRust
    friend struct UITextLabelRust;          // UITextLabel.cpp: the Rust object's calls out
public:
    UITextLabel(const UWideString& inText, GEFont* inFont, unsigned long inId, const GERectangle& inRectangle,
                UIForm* inForm, unsigned long inLabelType, unsigned long inIndex, unsigned long inBorderThickness);
    UITextLabel(const UITextLabel&) = delete;
    UITextLabel& operator=(const UITextLabel&) = delete;
    virtual ~UITextLabel();
    virtual void AppendText(const UWideString& inText) { this->SetText(inText, false); }   // slot 0x88
    virtual void ChangeFont(GEFont* inFont) { this->Call(EeUiLabelCall::ChangeFont, (intptr_t)inFont); }
    virtual long CopyData() { return (long)this->Call(EeUiLabelCall::CopyData); }
    void DeleteArray() { this->Call(EeUiLabelCall::DeleteArray); }
    void DeleteLine(unsigned long inLine) { this->Call(EeUiLabelCall::DeleteLine, inLine); }
    virtual long DrawPolygons() { return (long)this->Call(EeUiLabelCall::DrawPolygons); }
    virtual long DrawTextA() { return (long)this->Call(EeUiLabelCall::DrawText); }
    bool GetFitRectToText() const { return this->Call(EeUiLabelCall::FitRectToText) != 0; }
    GEText* GetGEText() const { return reinterpret_cast<GEText*>(this->Call(EeUiLabelCall::Text)); }
    unsigned long GetLineCount() const { return (unsigned long)this->Call(EeUiLabelCall::LineCount); }
    unsigned long GetNumberLinesDisplayed() const { return (unsigned long)this->Call(EeUiLabelCall::LinesDisplayed); }
    const UIScrollBar* GetScrollBar() const { return reinterpret_cast<const UIScrollBar*>(this->Call(EeUiLabelCall::ScrollBar)); } // NULL while it is marked removed
    virtual bool GetTabable() const { return false; }
    const UWideString& GetText(unsigned long inLine) const { return *reinterpret_cast<const UWideString*>(this->Call(EeUiLabelCall::Line, inLine)); }
    long GetTextAsInt(unsigned long inLine) const { return atoi(this->GetText(inLine)); }
    virtual long MouseWheelTurned(long inDelta) { return (long)this->Call(EeUiLabelCall::MouseWheelTurned, inDelta); }
    virtual void RecomputeDisplayRange() { this->Call(EeUiLabelCall::RecomputeDisplayRange); } // slot 0x8c
    bool ScrollBarExists() const { return this->Call(EeUiLabelCall::ScrollBarExists) != 0; }
    void SetAlignment(unsigned long inAlignment) { this->Call(EeUiLabelCall::SetAlignment, inAlignment); }
    void SetFitRectToText(bool inFit) { this->Call(EeUiLabelCall::SetFitRectToText, inFit); }
    virtual void SetLocation(const GE2DPoint& inLocation) { this->Call(EeUiLabelCall::SetLocation, 0, 0, inLocation.GetX(), inLocation.GetY()); }
    virtual void SetRectangle(const GERectangle& inRectangle) { this->CallRect(EeUiLabelCall::SetRectangle, inRectangle); }
    void SetStartingDisplayLine(unsigned long inLine) { this->Call(EeUiLabelCall::SetStartingDisplayLine, inLine); }
    void SetScrollBarBgOpacity(float inOpacity) { this->Call(EeUiLabelCall::SetScrollBarBgOpacity, 0, 0, inOpacity); }
    void SetScrollBarBgColor(const GEColor& inColor) { this->Call(EeUiLabelCall::SetScrollBarBgColor, 0, 0, inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue()); }
    void SetText(const UWideString& inText) { this->SetText(inText, true); }
    void SetText(long inValue)
    {
        char theBuffer[50];
        PlatIToA(inValue, theBuffer, 10);
        UWideString theText(theBuffer);
        this->SetText(theText, true);
    }
    virtual long UpdateKeyboard() { return kErrorFailed; }
protected:
    virtual void CheckScrollBar() { this->Call(EeUiLabelCall::CheckScrollBar); } // slot 0x90
    virtual long MouseLButtonDown() { return (long)this->Call(EeUiLabelCall::MouseLButtonDown); }
    virtual long MouseLButtonUp() { return (long)this->Call(EeUiLabelCall::MouseLButtonUp); }
    virtual long MouseMove(const GE2DPoint& inLocation) { return (long)this->Call(EeUiLabelCall::MouseMove, 0, 0, inLocation.GetX(), inLocation.GetY()); }
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    void SetTextRectangle(const GERectangle& inRectangle) { this->CallRect(EeUiLabelCall::SetTextRectangle, inRectangle); }
    virtual void TabbedFrom() {}
    virtual void TabbedTo() {}
public:   // (the combo box's shims: UIComboBox.cpp)
    void SetText(const UWideString& inText, bool inReplace) { this->Call(EeUiLabelCall::SetText, (intptr_t)(const char*)inText, inReplace); }
private:
private:
    intptr_t Call(EeUiLabelCall c, intptr_t a = 0, intptr_t b = 0, float f0 = 0.0f, float f1 = 0.0f, float f2 = 0.0f, float f3 = 0.0f) const { float f[4] = {f0, f1, f2, f3}; return ee_ui_label_call(this->mRust, c, a, b, f); }
    intptr_t CallRect(EeUiLabelCall c, const GERectangle& r) const { float f[4] = {r.GetBottom(), r.GetLeft(), r.GetRight(), r.GetTop()}; return ee_ui_label_call(this->mRust, c, 0, 0, f); }
    EeUiLabel* mRust;                       // the state (labelobj.rs)
};

#endif
