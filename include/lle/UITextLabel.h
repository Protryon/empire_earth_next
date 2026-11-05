// C:\EEX\Low-Level Engine\User Interface\UITextLabel.h (RECONSTRUCTION). A text control: one line (label type 0)
// or word-wrapped lines with an optional vertical scroll bar (label type 1); the form's help text label is one.
// UITextLabel.cpp (0x1007e579..0x1007fb46) holds the ctor and the out-of-line members; the inline members are
// emitted first by UICheckBox.cpp (0x1005f538..0x1005f9da, in declaration order). Vtable 0x10098f98 (UIControl's
// 34 slots, then AppendText, RecomputeDisplayRange, CheckScrollBar). Layout proven by the implicit copy ctor
// 0x1005f689 (sizeof 0x124). Access levels from the exports. Requires lle/UIControl.h (includes lle/UIScrollBar.h).
#ifndef LLE_UITEXTLABEL_H
#define LLE_UITEXTLABEL_H
#include "platform/PlatString.h"

#include <stdlib.h>
#include <vector>
#include "lle/UIControl.h"
#include "lle/UIScrollBar.h"   // the inline scroll bar forwarders need it complete

class GEText;

class LLE_API UITextLabel : public UIControl {
    friend class UIForm;                    // SetText(const UWideString&, bool) is private
    friend class UIESliderChange;           // ProcessEvent calls SetText(const UWideString&, bool) (0x1006e950)
    friend class UIComboBox;                // calls SetText(const UWideString&, bool) and MouseLButtonDown (ui-edit)
    friend class UICheckBox;                // SetText calls SetText(const UWideString&, bool) (0x1006013e)
    friend class UIRadioButton;             // likewise (0x1007c146)
    friend class UITriStateCheckBox;        // likewise (0x10080b1c)
public:
    UITextLabel(const UWideString& inText, GEFont* inFont, unsigned long inId, const GERectangle& inRectangle,
                UIForm* inForm, unsigned long inLabelType, unsigned long inIndex, unsigned long inBorderThickness);
    virtual ~UITextLabel();
    virtual void AppendText(const UWideString& inText);   // slot 0x88
    virtual void ChangeFont(GEFont* inFont);
    virtual long CopyData();
    void DeleteArray();
    void DeleteLine(unsigned long inLine);
    virtual long DrawPolygons();
    virtual long DrawTextA();
    const GERectangle& GetCopiedTextRectangle() const { return this->mCopiedTextRectangle; }  // 0x10059663 (ICF)
    bool GetFitRectToText() const { return this->mFitRectToText; }  // 0x1005f538
    GEText* GetGEText() const { return this->mText; }  // ICF-folded (0x100660be)
    unsigned long GetLineCount() const { return this->mLines.size(); }  // 0x1005f53f
    unsigned long GetNumberLinesDisplayed() const { return this->mNumberLinesDisplayed; }  // 0x10060381 (ICF)
    // 0x1005f558
    const UIScrollBar* GetScrollBar() const
    {
        if (!this->mScrollBarRemoved)
            return this->mScrollBar;
        return NULL;
    }
    const GERectangle& GetScrollBarRectangle() const { return this->mScrollBarRectangle; }  // 0x1005f56b
    virtual bool GetTabable() const { return false; }                // 0x1005efa4 (ICF)
    const UWideString& GetText(unsigned long inLine) const { return *this->mLines[inLine]; }  // 0x1005f572
    long GetTextAsInt(unsigned long inLine) const { return atoi(*this->mLines[inLine]); }  // 0x1005f584
    const GERectangle& GetTextRectangle() const { return this->mTextRectangle; }  // 0x10011394 (ICF)
    virtual long MouseWheelTurned(long inDelta);
    virtual void RecomputeDisplayRange();   // slot 0x8c
    bool ScrollBarExists() const { return this->mScrollBar ? true : false; }  // 0x100634de (ICF)
    void SetAlignment(unsigned long inAlignment) { this->mAlignment = inAlignment; }  // 0x10059702 (ICF)
    void SetFitRectToText(bool inFit);
    virtual void SetLocation(const GE2DPoint& inLocation);
    virtual void SetRectangle(const GERectangle& inRectangle);
    // 0x1005f5a0
    void SetStartingDisplayLine(unsigned long inLine)
    {
        if (this->mScrollBar)
            this->mScrollBar->SetCurrentRangeValue(inLine);
    }
    // 0x1005f5bc
    void SetScrollBarBgOpacity(float inOpacity)
    {
        if (this->mScrollBar)
            this->mScrollBar->SetBgOpacity(inOpacity);
        this->mScrollBarBgOpacity = inOpacity;
    }
    // 0x1005f5e4
    void SetScrollBarBgColor(const GEColor& inColor)
    {
        if (this->mScrollBar)
            this->mScrollBar->SetBgColor(inColor);
        this->mScrollBarBgColor = inColor;
    }
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
    void SetTextRectangle(const GERectangle& inRectangle) { this->mTextRectangle = inRectangle; }  // 0x1005f666
    virtual void TabbedFrom() {}
    virtual void TabbedTo() {}
private:
    void ComputeTextLabelRect();
    unsigned long ReadLine(const char* inText);
    unsigned long ReadWord(const char* inText, char* outWord);
    void SetText(const UWideString& inText, bool inReplace);

    std::vector<UWideString*> mLines;       // +0x0a0 one string per line (owned)
    UWideString* mDisplayLines;             // +0x0b0 new[mNumberLinesDisplayed]: the lines CopyData hands to DrawTextA
    GERectangle mOriginalRectangle;         // +0x0b4 the rectangle given at construction
    GERectangle mScrollBarRectangle;        // +0x0c4
    GERectangle mTextRectangle;             // +0x0d4
    GERectangle mCopiedTextRectangle;       // +0x0e4 (CopyData)
    GEColor mScrollBarBgColor;              // +0x0f4
    float mScrollBarBgOpacity;              // +0x100
    GEText* mCopiedText;                    // +0x104 (CopyData)
    GEText* mText;                          // +0x108
    UIScrollBar* mScrollBar;                // +0x10c type 1 only, created by CheckScrollBar
    unsigned long mNumberLinesDisplayed;    // +0x110 (at least 1)
    long mLastDisplayLine;                  // +0x114 (CopyData; DrawTextA draws 0..it)
    unsigned long mLabelType;               // +0x118 0 single line, 1 word-wrapped
    unsigned long mAlignment;               // +0x11c 0 left, 1 centred, 2 right
    bool mFitRectToText;                    // +0x120
    bool mScrollBarRemoved;                 // +0x121
    bool mDrawScrollBar;                    // +0x122 (CopyData)
    bool mRecomputePending;                 // +0x123 SetRectangle: CopyData recomputes the display range
};

#endif
