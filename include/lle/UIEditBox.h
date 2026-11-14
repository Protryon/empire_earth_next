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

class GEFont;
class GEText;
struct EeUiEdit;

class LLE_API UIEditBox : public UIControl {
    friend class UIComboBox;                // calls the protected mouse/tab handlers
    friend struct UIEditBoxRust;            // UIEditBox.cpp: the Rust object's calls out
public:
    UIEditBox(unsigned long inId, GEFont* inFont, unsigned long inMaxChars, bool inNumbersOnly,
              const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness,
              bool inMultiLine, bool inPasswordBox);
    UIEditBox(const UIEditBox&) = delete;
    UIEditBox& operator=(const UIEditBox&) = delete;
    virtual ~UIEditBox();
    virtual void AddIMEResultString(const UWideString& inString);
    void CalculateCaretFromMouseLocation(unsigned long& outLine, long& outIndex, const GE2DPoint& inLocation) const;
    virtual long ChangeIMECompositionString(const UWideString& inString, const UString& inAttributes,
                                            unsigned long inCursor);
    virtual void ChangeFont(GEFont* inFont);
    void ChangeWidth(unsigned long inWidth);
    virtual long CloseIMECompositionString();
    void Copy();
    virtual long CopyData();
    void Cut();
    void DeleteSelectedText();
    void DeleteText(unsigned long inStartIndex, unsigned long inStartLine, unsigned long inEndIndex,
                    unsigned long inEndLine);
    void DeleteText();
    virtual long DrawPolygons();
    virtual long DrawTextA();
    void GetAllText(UWideString& outString) const;
    void GetCaretLocation(GE2DPoint& outLocation) const;
    bool GetCursorOn() const;
    GEText* GetGEText() const;
    virtual bool GetIMEEnabled() const;
    bool GetIsPasswordBox() const;
    bool GetIsTextSelected() const;
    unsigned long GetLineCount() const;
    void GetSelectedText(UWideString& outString) const;
    void GetSelectedTextIndices(unsigned long& outStart, unsigned long& outEnd) const;
    long GetStartingDisplayIndex() const;
    const char* GetText(unsigned long inLine) const;
    long GetTextAsInt(unsigned long inLine) const { return atoi(this->GetText(inLine)); }
    void Paste();
    virtual long OpenIMECompositionString();
    virtual void ReleaseCapturedKeyboard();
    void SelectText();
    void SelectText(unsigned long inCaretIndex, unsigned long inStartIndex, unsigned long inCaretLine,
                    unsigned long inStartLine);
    void SetCaretLocation(unsigned long inIndex, unsigned long inLine);
    void SetCharScroll(unsigned long inCharScroll);
    void SetCursorColor(const GEColor& inColor);
    void SetIMETextColor(const GEColor& inColor);
    void SetIsPasswordBox(bool inPasswordBox);
    virtual void SetLocation(const GE2DPoint& inLocation);
    void SetSelectedBgColor(const GEColor& inColor);
    void SetText(const UWideString& inString);
    void SetText(unsigned long inValue)
    {
        char theBuffer[50];
        PlatIToA(inValue, theBuffer, 10);
        this->SetText(theBuffer);
    }
    virtual long UpdateKeyboard();
    virtual long UpdateTranslatedKey(const char* inKey, short inRepeat);
protected:
    virtual void ButtonDown() { this->SetDirty(true); }  // slot 0x88
    virtual long MouseLButtonDoublePressed();
    virtual long MouseLButtonDown();
    virtual long MouseLButtonUp();
    virtual long MouseMove(const GE2DPoint& inLocation);
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void ProcessEnterKey();         // slot 0x8c
    virtual void TabbedFrom();
    virtual void TabbedTo();
private:
    EeUiEdit* mRust;                        // the state (editobj.rs)
};

#endif
