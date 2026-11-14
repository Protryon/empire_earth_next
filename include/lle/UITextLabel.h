// C:\EEX\Low-Level Engine\User Interface\UITextLabel.h (RECONSTRUCTION). A text control: one line (label type 0)
// or word-wrapped lines with an optional vertical scroll bar (label type 1); the form's help text label is one.
// Vtable 0x10098f98 (UIControl's 34 slots, then AppendText, RecomputeDisplayRange, CheckScrollBar). The original's
// sizeof was 0x124 (the implicit copy ctor 0x1005f689; the object is never copied).
//
// The class is whole in Rust (rust/crates/ee_ui/src/labelobj.rs, R6 ui round 13): its state (the lines, the display
// copies, the rectangles, the scroll bar and its colours, the flags) and every method are the Rust object mRust;
// UITextLabel.cpp holds the forwarders, the ctor and dtor and the host (the op switch). The lines are still
// UWideStrings (GetText answers a reference), made and held by address by the Rust object. Requires lle/UIControl.h.
#ifndef LLE_UITEXTLABEL_H
#define LLE_UITEXTLABEL_H
#include "platform/PlatString.h"

#include <stdlib.h>
#include "lle/UIControl.h"
#include "lle/UIScrollBar.h"   // units including this header rely on it for the scroll bar

class GEText;
struct EeUiLabel;

class LLE_API UITextLabel : public UIControl {
    friend class UIForm;                    // SetText(const UWideString&, bool) is private
    friend class UIESliderChange;           // ProcessEvent calls SetText(const UWideString&, bool) (0x1006e950)
    friend class UIComboBox;                // calls SetText(const UWideString&, bool) and MouseLButtonDown (ui-edit)
    friend class UICheckBox;                // SetText calls SetText(const UWideString&, bool) (0x1006013e)
    friend class UIRadioButton;             // likewise (0x1007c146)
    friend class UITriStateCheckBox;        // likewise (0x10080b1c)
    friend struct UITLScrollingRust;        // UITLScrolling.cpp: its forwarders reach mRust
    friend struct UITextLabelRust;          // UITextLabel.cpp: the Rust object's calls out
public:
    UITextLabel(const UWideString& inText, GEFont* inFont, unsigned long inId, const GERectangle& inRectangle,
                UIForm* inForm, unsigned long inLabelType, unsigned long inIndex, unsigned long inBorderThickness);
    UITextLabel(const UITextLabel&) = delete;
    UITextLabel& operator=(const UITextLabel&) = delete;
    virtual ~UITextLabel();
    virtual void AppendText(const UWideString& inText);   // slot 0x88
    virtual void ChangeFont(GEFont* inFont);
    virtual long CopyData();
    void DeleteArray();
    void DeleteLine(unsigned long inLine);
    virtual long DrawPolygons();
    virtual long DrawTextA();
    const GERectangle& GetCopiedTextRectangle() const;
    bool GetFitRectToText() const;
    GEText* GetGEText() const;
    unsigned long GetLineCount() const;
    unsigned long GetNumberLinesDisplayed() const;
    const UIScrollBar* GetScrollBar() const;   // NULL while it is marked removed
    const GERectangle& GetScrollBarRectangle() const;
    virtual bool GetTabable() const { return false; }                // 0x1005efa4 (ICF)
    const UWideString& GetText(unsigned long inLine) const;
    long GetTextAsInt(unsigned long inLine) const { return atoi(this->GetText(inLine)); }  // 0x1005f584
    const GERectangle& GetTextRectangle() const;
    virtual long MouseWheelTurned(long inDelta);
    virtual void RecomputeDisplayRange();   // slot 0x8c
    bool ScrollBarExists() const;
    void SetAlignment(unsigned long inAlignment);
    void SetFitRectToText(bool inFit);
    virtual void SetLocation(const GE2DPoint& inLocation);
    virtual void SetRectangle(const GERectangle& inRectangle);
    void SetStartingDisplayLine(unsigned long inLine);
    void SetScrollBarBgOpacity(float inOpacity);
    void SetScrollBarBgColor(const GEColor& inColor);
    void SetText(const UWideString& inText) { this->SetText(inText, true); }  // 0x1005f617
    // 0x1005f625
    void SetText(long inValue)
    {
        char theBuffer[50];
        PlatIToA(inValue, theBuffer, 10);
        UWideString theText(theBuffer);
        this->SetText(theText, true);
    }
    virtual long UpdateKeyboard() { return kErrorFailed; }   // 0x10004652 (ICF)
protected:
    virtual void CheckScrollBar();          // slot 0x90
    virtual long MouseLButtonDown();
    virtual long MouseLButtonUp();
    virtual long MouseMove(const GE2DPoint& inLocation);
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    void SetTextRectangle(const GERectangle& inRectangle);
    virtual void TabbedFrom() {}
    virtual void TabbedTo() {}
private:
    void SetText(const UWideString& inText, bool inReplace);

    EeUiLabel* mRust;                       // the state (labelobj.rs)
};

#endif
