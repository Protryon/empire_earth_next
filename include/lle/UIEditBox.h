// C:\EEX\Low-Level Engine\User Interface\UIEditBox.h (RECONSTRUCTION). A single- or multi-line text entry control
// (control type 4). UIEditBox.cpp (0x10069d6b..0x1006de9d) holds the ctor, dtor and the out-of-line members; the
// inline members, the implicit copy ctor/operator= and ??_E are emitted first by UIComboBox.cpp (0x10060308..,
// the first includer in link order). Vtable 0x10099178 (UIControl's 34 slots, then ButtonDown 0x88 and
// ProcessEnterKey 0x8c). sizeof 0x1e0. Field names are provisional (roles from the code).
// Requires lle/UIControl.h, lle/UIScrollBar.h, <vector>.
#ifndef LLE_UIEDITBOX_H
#define LLE_UIEDITBOX_H
#include "platform/PlatString.h"

#include <vector>

class GEFont;
class GELine;
class GEText;
class TMPoll;
class UIScrollBar;

class LLE_API UIEditBox : public UIControl {
    friend class UIComboBox;                // calls the protected mouse/tab handlers
    friend struct UIEditBoxRust;            // the Rust text model's host calls (UIEditBox.cpp)
public:
    UIEditBox(unsigned long inId, GEFont* inFont, unsigned long inMaxChars, bool inNumbersOnly,
              const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness,
              bool inMultiLine, bool inPasswordBox);
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
    void GetAllText(UWideString& outString) const
    {
        this->GetText(0, this->mLines.size() - 1, 0, this->mLines[this->mLines.size() - 1].GetLength(), outString);
    }
    void GetCaretLocation(GE2DPoint& outLocation) const { this->GetLocationFromIndex(this->mCaretIndex, this->mCaretLine, outLocation); }
    bool GetCursorOn() const { return this->mCursorOn; }
    GEText* GetGEText() const { return this->mText; }
    virtual bool GetIMEEnabled() const { return this->mIMEActive; }
    bool GetIsPasswordBox() const { return this->mIsPasswordBox; }
    bool GetIsTextSelected() const { return this->mIsTextSelected; }
    unsigned long GetLineCount() const { return this->mLines.size(); }
    void GetSelectedText(UWideString& outString) const;
    void GetSelectedTextIndices(unsigned long& outStart, unsigned long& outEnd) const
    {
        outStart = this->mSelectionStartIndex;
        outEnd = this->mCaretIndex;
    }
    long GetStartingDisplayIndex() const { return this->mDrawingIndex; }
    const char* GetText(unsigned long inLine) const { return this->mLines[inLine]; }
    long GetTextAsInt(unsigned long inLine) const { return atoi(this->mLines[inLine]); }
    void Paste();
    virtual long OpenIMECompositionString();
    virtual void ReleaseCapturedKeyboard();
    void SelectText();
    void SelectText(unsigned long inCaretIndex, unsigned long inStartIndex, unsigned long inCaretLine,
                    unsigned long inStartLine);
    void SetCaretLocation(unsigned long inIndex, unsigned long inLine);
    void SetCharScroll(unsigned long inCharScroll) { this->mCharScroll = inCharScroll; }
    void SetCursorColor(const GEColor& inColor) { this->mCursorColor = inColor; }
    void SetIMETextColor(const GEColor& inColor) { this->mIMETextColor = inColor; }
    void SetIsPasswordBox(bool inPasswordBox)
    {
        if (this->mIsSingleLine)
            this->mIsPasswordBox = inPasswordBox;
        else
            this->mIsPasswordBox = false;
    }
    virtual void SetLocation(const GE2DPoint& inLocation);
    void SetSelectedBgColor(const GEColor& inColor) { this->mSelectedBgColor = inColor; }
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
    const char* AddChar(const char* inString);
    void BackSpaceChar();
    void CheckScrollBar();
    void ComputeCaretLocation(bool inUpdateCaretX);
    void ComputeDrawingIndex();
    void DecrementCaretPos();
    void DeleteChar();
    void DeleteLine(unsigned long inLine);
    void GetLocationFromIndex(unsigned long inIndex, unsigned long inLine, GE2DPoint& outLocation) const;
    void GetSelectedTextRectangle(unsigned long inLine, unsigned long inStartIndex, unsigned long inEndIndex,
                                  GERectangle& outRectangle) const;
    void GetText(unsigned long inStartLine, unsigned long inEndLine, unsigned long inStartIndex,
                 unsigned long inEndIndex, UWideString& outString) const;
    void IncrementCaretPos(long inCount);
    void ProcessDownKey();
    void ProcessEndKey();
    void ProcessHomeKey();
    void ProcessUpKey();
    void RemoveIMECompositionString();
    void ScrollLeft();
    void ScrollRight(long inCount);
    void SetIndexFromUpDownOffset();
    void SetupKeyboardForCapture();

    GERectangle mCopiedTextRectangle;       // +0x0a0 mTextRectangle as of the last CopyData
    GERhombus mSelectionRhombus;            // +0x0b0
    GERectangle mTextRectangle;             // +0x0c8 mRectangle inset by the border (and the scroll bar)
    GEColor mSelectedBgColor;               // +0x0d8
    GEColor mCursorColor;                   // +0x0e4
    GEColor mIMETextColor;                  // +0x0f0
    GEColor mCopiedIMETextColor;            // +0x0fc
    UIScrollBar* mScrollBar;                // +0x108 multi-line boxes only
    GELine* mCursor;                        // +0x10c
    GEText* mText;                          // +0x110 measuring
    GEText* mCopiedText;                    // +0x114 drawing
    TMPoll* mRepeatPoll;                    // +0x118 key repeat rate (50 ms)
    TMPoll* mRepeatDelayPoll;               // +0x11c key repeat delay (500 ms)
    TMPoll* mCursorPoll;                    // +0x120 cursor blink (500 ms)
    TMPoll* mScrollPoll;                    // +0x124 drag scrolling (25 ms)
    std::vector<UWideString> mLines;        // +0x128
    std::vector<UWideString> mDisplayLines; // +0x138 the visible lines as of the last CopyData
    UWideString mWorkString;                // +0x148
    UString mIMEAttributes;                 // +0x15c
    UString mCopiedIMEAttributes;           // +0x16c
    float mCaretX;                          // +0x17c kept across up/down moves
    float mSavedBgOpacity;                  // +0x180
    unsigned long mSelectionStartLine;      // +0x184
    unsigned long mCopiedSelectionStartLine;   // +0x188
    unsigned long mCopiedSelectionEndLine;  // +0x18c
    unsigned long mMaxChars;                // +0x190
    unsigned long mCharScroll;              // +0x194
    unsigned long mCaretLine;               // +0x198
    unsigned long mVisibleLineCount;        // +0x19c
    unsigned long mIMECompositionLength;    // +0x1a0
    unsigned long mIMEStartIndex;           // +0x1a4
    unsigned long mIMEStartLine;            // +0x1a8
    unsigned long mCopiedIMEStartIndex;     // +0x1ac
    unsigned long mCopiedIMEStartLine;      // +0x1b0
    unsigned long mCopiedIMEEndIndex;       // +0x1b4
    unsigned long mCopiedIMEEndLine;        // +0x1b8
    long mCopiedSelectionEndIndex;          // +0x1bc
    long mCopiedSelectionStartIndex;        // +0x1c0
    long mSelectionStartIndex;              // +0x1c4
    long mCaretIndex;                       // +0x1c8
    long mDrawingIndex;                     // +0x1cc first character shown (single-line boxes)
    bool mIsTextSelected;                   // +0x1d0
    bool mKeyRepeating;                     // +0x1d1
    bool mCursorOn;                         // +0x1d2
    bool mSelecting;                        // +0x1d3
    bool mCopiedCursorOn;                   // +0x1d4
    bool mCopiedIsTextSelected;             // +0x1d5
    bool mNumbersOnly;                      // +0x1d6
    bool mIsSingleLine;                     // +0x1d7
    bool mScrollBarCaptured;                // +0x1d8
    bool mScrollBarVisible;                 // +0x1d9
    bool mIsPasswordBox;                    // +0x1da
    bool mIMEActive;                        // +0x1db
    bool mCopiedIMEActive;                  // +0x1dc
    bool mSavedAlwaysDirty;                 // +0x1dd
};

#endif
