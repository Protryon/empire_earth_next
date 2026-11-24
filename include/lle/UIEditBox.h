// C:\EEX\Low-Level Engine\User Interface\UIEditBox.h (RECONSTRUCTION). A single- or multi-line text entry control
// (control type 4). UIEditBox.cpp (0x10069d6b..0x1006de9d) held the ctor, dtor and the out-of-line members; the
// inline members, the implicit copy ctor/operator= and ??_E were emitted first by UIComboBox.cpp (0x10060308..,
// the first includer in link order). Vtable 0x10099178 (UIControl's 34 slots, then ButtonDown 0x88 and
// ProcessEnterKey 0x8c). The original's sizeof was 0x1e0.
//
// The class is whole in Rust (rust/crates/ee_ui/src/editobj.rs, R6 ui round 14): its state (the lines, the display
// copies, the caret, selection and IME records, the rectangles, colours, texts, polls and scroll bar) and every
// method are the Rust object mRust; UIEditBox.cpp holds the forwarders, the ctor and dtor and the host (the op
// switch). GetText answers the address of the Rust object's line bytes (valid until the text changes).
// Requires lle/UIControl.h, lle/UIScrollBar.h.
#ifndef LLE_UIEDITBOX_H
#define LLE_UIEDITBOX_H
#include "platform/PlatString.h"

#include <stdlib.h>
#include <vector>   // units including this header rely on it
#include "rust/ui/editobj.h"


class LLE_API UIEditBox : public UIControl {
public:
    UIEditBox(unsigned long inId, GEFont* inFont, unsigned long inMaxChars, bool inNumbersOnly,
              const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness,
              bool inMultiLine, bool inPasswordBox)
        : UIControl(kControlTypeEditBox, inId, inRectangle, inForm, inIndex, inBorderThickness),
      mRust(ee_ui_edit_new(this, ee_ui_edit_host, inFont, inMaxChars, ee_ui_edit_flags(inNumbersOnly, inMultiLine, inPasswordBox),
                           inBorderThickness)) {}
    virtual ~UIEditBox() { ee_ui_edit_free(this->mRust); }
    virtual void AddIMEResultString(const UWideString& inString) { this->Call(EeUiEditCall::AddImeResult, (intptr_t)(const char*)inString); }
    virtual long ChangeIMECompositionString(const UWideString& inString, const UString& inAttributes,
                                            unsigned long inCursor) { return (long)this->Call(EeUiEditCall::ImeChange, (intptr_t)(const char*)inString, inCursor, (intptr_t)(const char*)inAttributes); }
    virtual void ChangeFont(GEFont* inFont) { this->Call(EeUiEditCall::ChangeFont, (intptr_t)inFont); }
    void ChangeWidth(unsigned long inWidth) { this->Call(EeUiEditCall::ChangeWidth, inWidth); }
    virtual long CloseIMECompositionString() { return (long)this->Call(EeUiEditCall::ImeClose); }
    void Copy() { this->Call(EeUiEditCall::Copy); }
    virtual long CopyData() { return (long)this->Call(EeUiEditCall::CopyData); }
    void Cut() { this->Call(EeUiEditCall::Cut); }
    void DeleteText(unsigned long inStartIndex, unsigned long inStartLine, unsigned long inEndIndex,
                    unsigned long inEndLine) { this->Call(EeUiEditCall::DeleteRange, inStartIndex, inStartLine, inEndIndex, inEndLine); }
    void DeleteText() { this->Call(EeUiEditCall::Clear); }
    virtual long DrawPolygons() { return (long)this->Call(EeUiEditCall::DrawPolygons); }
    virtual long DrawTextA() { return (long)this->Call(EeUiEditCall::DrawText); }
    void GetAllText(UWideString& outString) const { outString = (const char*)this->Call(EeUiEditCall::AllText); }
    GEText* GetGEText() const { return reinterpret_cast<GEText*>(this->Call(EeUiEditCall::Text)); }
    virtual bool GetIMEEnabled() const { return this->Call(EeUiEditCall::ImeEnabled) != 0; }
    unsigned long GetLineCount() const { return (unsigned long)this->Call(EeUiEditCall::LineCount); }
    const char* GetText(unsigned long inLine) const { return (const char*)this->Call(EeUiEditCall::Line, inLine); }
    long GetTextAsInt(unsigned long inLine) const { return atoi(this->GetText(inLine)); }
    virtual long OpenIMECompositionString() { return (long)this->Call(EeUiEditCall::ImeOpen); }
    virtual void ReleaseCapturedKeyboard() { this->Call(EeUiEditCall::ReleaseKeyboard); }
    void SelectText() { this->Call(EeUiEditCall::SelectAll); }
    void SelectText(unsigned long inCaretIndex, unsigned long inStartIndex, unsigned long inCaretLine,
                    unsigned long inStartLine) { this->Call(EeUiEditCall::Select, inCaretIndex, inStartIndex, inCaretLine, inStartLine); }
    void SetCaretLocation(unsigned long inIndex, unsigned long inLine) { this->Call(EeUiEditCall::SetCaretLocation, inIndex, inLine); }
    void SetCursorColor(const GEColor& inColor) { this->CallColor(EeUiEditCall::SetCursorColor, inColor); }
    void SetIsPasswordBox(bool inPasswordBox) { this->Call(EeUiEditCall::SetIsPasswordBox, inPasswordBox); }
    virtual void SetLocation(const GE2DPoint& inLocation) { this->CallPoint(EeUiEditCall::SetLocation, inLocation); }
    void SetSelectedBgColor(const GEColor& inColor) { this->CallColor(EeUiEditCall::SetSelectedBgColor, inColor); }
    void SetText(const UWideString& inString) { this->Call(EeUiEditCall::SetText, (intptr_t)(const char*)inString); }
    void SetText(unsigned long inValue) { ee_ui_edit_set_number(this->mRust, inValue); }   // _itoa's decimal
    virtual long UpdateKeyboard() { return (long)this->Call(EeUiEditCall::UpdateKeyboard); }
    virtual long UpdateTranslatedKey(const char* inKey, short inRepeat) { return (long)this->Call(EeUiEditCall::UpdateTranslatedKey, (intptr_t)inKey, inRepeat); }
    virtual void ButtonDown() { this->SetDirty(true); }  // slot 0x88
    virtual long MouseLButtonDoublePressed() { return (long)this->Call(EeUiEditCall::MouseDoublePressed); }
    virtual long MouseLButtonDown() { return (long)this->Call(EeUiEditCall::MouseLButtonDown); }
    virtual long MouseLButtonUp() { return (long)this->Call(EeUiEditCall::MouseLButtonUp); }
    virtual long MouseMove(const GE2DPoint& inLocation) { return (long)this->CallPoint(EeUiEditCall::MouseMove, inLocation); }
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void ProcessEnterKey() { this->Call(EeUiEditCall::ProcessEnterKey); } // slot 0x8c
    virtual void TabbedFrom() { this->Call(EeUiEditCall::TabbedFrom); }
    virtual void TabbedTo() { this->Call(EeUiEditCall::TabbedTo); }
    intptr_t CallColor(EeUiEditCall c, const GEColor& k) const { return this->Call(c, 0, 0, 0, 0, k.GetRed(), k.GetGreen(), k.GetBlue()); }
    intptr_t CallPoint(EeUiEditCall c, const GE2DPoint& p, intptr_t a = 0) const { return this->Call(c, a, 0, 0, 0, p.GetX(), p.GetY()); }
    intptr_t Call(EeUiEditCall c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0, intptr_t d = 0, float f0 = 0.0f, float f1 = 0.0f, float f2 = 0.0f) const { return ee_ui_edit_call(this->mRust, c, a, b, cc, d, UIF4(f0, f1, f2)); }
    EeUiEdit* mRust;                        // the state (editobj.rs)
};

#endif
