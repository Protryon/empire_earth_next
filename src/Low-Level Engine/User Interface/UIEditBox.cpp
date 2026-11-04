// C:\EEX\Low-Level Engine\User Interface\UIEditBox.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10069d6b..0x1006de9d: the U2D key init chains (U2DRectangle.h, via the rasterizer header), the ctor, the dtor
// and the out-of-line members in alphabetical order (the multibyte character helper sits right after AddChar, its
// first user). The header-inline members, the implicit copy members and ??_E are emitted first by UIComboBox.cpp.
// UIEMouseWheelScroll.cpp starts at 0x1006de9d.
#include <windows.h>
#include <imm.h>
#include <mbstring.h>
#include <stdlib.h>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/GELine.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/TMPoll.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "lle/UIEditBox.h"
#include "LostDebug.h"  // NOT original source: EE_LOST_DEBUG filler (ProcessEnterKey, SetIndexFromUpDownOffset; docs/LOST_DEBUG.md)

// FUNCTION: LLE 0x10069e8b
// Creates an empty edit box (one blank line) with caret, key-repeat timers and text; multi-line boxes get a scroll bar
UIEditBox::UIEditBox(unsigned long inId, GEFont* inFont, unsigned long inMaxChars, bool inNumbersOnly,
                     const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                     unsigned long inBorderThickness, bool inMultiLine, bool inPasswordBox)
    : UIControl(kControlTypeEditBox, inId, inRectangle, inForm, inIndex, inBorderThickness)
{
    this->mCaretIndex = 0;
    this->mIsTextSelected = false;
    this->mCharScroll = 1;
    this->mDrawingIndex = 0;
    this->mCaretLine = 0;
    this->mMaxChars = inMaxChars;
    this->mCursorOn = false;
    this->mKeyRepeating = false;
    this->mSelecting = false;
    this->mScrollBarCaptured = false;
    this->mIsSingleLine = !inMultiLine;
    if (!inMultiLine)
        this->mIsPasswordBox = inPasswordBox;
    else
        this->mIsPasswordBox = false;
    this->mNumbersOnly = inNumbersOnly;
    this->mSavedBgOpacity = 0.0f;
    this->mSelectionStartLine = 0;
    this->mCopiedSelectionStartLine = 0;
    this->mCopiedSelectionEndLine = 0;
    this->mCopiedSelectionEndIndex = 0;
    this->mCopiedSelectionStartIndex = 0;
    this->mSelectionStartIndex = 0;
    this->mIMEStartIndex = 0;
    this->mIMEStartLine = 0;
    this->mIMECompositionLength = 0;
    this->mCopiedCursorOn = false;
    this->mCopiedIsTextSelected = false;
    this->mScrollBarVisible = false;
    this->mIMEActive = false;
    this->mCopiedIMEStartIndex = 0;
    this->mCopiedIMEStartLine = 0;
    this->mCopiedIMEActive = false;
    this->mCopiedIMEEndIndex = 0;
    this->mCopiedIMEEndLine = 0;
    this->mSavedAlwaysDirty = false;
    this->mIMETextColor = GEColor(0.0f, 1.0f, 0.0f);
    this->mCopiedIMETextColor = this->mIMETextColor;
    this->mCaretX = 0.0f;
    this->mBorderType = 0;
    this->mCursor = new GELine(GE2DPoint(0.0f, 0.0f), GE2DPoint(0.0f, 0.0f), GEColor(0.0f, 0.0f, 0.0f),
                         GEColor(0.0f, 0.0f, 0.0f), 1.0f);
    this->mLines.resize(1);
    this->mLines[this->mCaretLine] = "";
    this->mWorkString = "";
    this->mRepeatPoll = new TMPoll(50, 0, false);
    this->mRepeatDelayPoll = new TMPoll(500, 0, false);
    this->mCursorPoll = new TMPoll(500, 0, false);
    this->mScrollPoll = new TMPoll(25, 0, false);
    this->mText = new GEText(inFont);
    this->mCopiedText = new GEText(inFont);
    this->mText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
    this->mText->SetBgColor(GEColor(0.0f, 0.0f, 0.0f));
    this->mText->SetIsTransparent(true);
    this->mTextRectangle.SetTop(this->GetRectangle().GetTop() + inBorderThickness);
    this->mTextRectangle.SetLeft(this->GetRectangle().GetLeft() + inBorderThickness);
    this->mTextRectangle.SetRight(this->GetRectangle().GetRight() - inBorderThickness);
    this->mTextRectangle.SetBottom(this->GetRectangle().GetBottom() - inBorderThickness);
    if (this->mTextRectangle.GetTop() > this->mTextRectangle.GetBottom()) {
        this->mTextRectangle.SetTop(this->GetRectangle().GetHeight() * 0.5f + this->GetRectangle().GetTop());
        this->mTextRectangle.SetBottom(this->mTextRectangle.GetTop() + 1.0f);
    }
    if (this->mTextRectangle.GetLeft() > this->mTextRectangle.GetRight()) {
        this->mTextRectangle.SetLeft(this->GetRectangle().GetWidth() * 0.5f + this->GetRectangle().GetLeft());
        this->mTextRectangle.SetRight(this->mTextRectangle.GetLeft() + 1.0f);
    }
    this->mVisibleLineCount = UMFloor(this->mTextRectangle.GetHeight() / inFont->GetHeight());
    if (!this->mIsSingleLine) {
        this->mScrollBarVisible = true;
        this->mTextRectangle.SetRight(this->GetRectangle().GetRight() - kScrollBarWidth);
        this->mScrollBar = new UIScrollBar(this->GetId(), this->mTextRectangle.GetTopRight(),
                                     this->mTextRectangle.GetHeight(), 1, this->GetForm(), this->GetIndex(),
                                     this->GetBorderThickness());
        this->mScrollBar->SetRange(0, 0);
        this->mScrollBar->SetPageSize(this->mVisibleLineCount > 0 ? this->mVisibleLineCount - 1 : 0);
        this->mDisplayLines.resize(this->mVisibleLineCount);
    } else {
        this->mDisplayLines.resize(1);
        this->mScrollBar = NULL;
    }
}

// FUNCTION: LLE 0x1006a44d
// Destroys the edit box, deleting its timers, caret line, texts and scroll bar
UIEditBox::~UIEditBox()
{
    delete this->mRepeatPoll;
    delete this->mRepeatDelayPoll;
    delete this->mCursorPoll;
    delete this->mCursor;
    delete this->mText;
    delete this->mCopiedText;
    delete this->mScrollBar;
    delete this->mScrollPoll;
}

// FUNCTION: LLE 0x1006a53f
// Inserts text at the caret: single-line boxes clip to the character limit (numbers-only drops non-digits but still
// counts them); multi-line boxes re-wrap the following text at word breaks, adding lines as needed
const char* UIEditBox::AddChar(const char* inString)
{
    UWideString theLine;
    UWideString theChar;
    UWideString theEmpty;
    unsigned long theLength = _mbslen(reinterpret_cast<const unsigned char*>(inString));
    unsigned long theSpaces = 0;
    bool theNewLine = false;
    if (this->mIsTextSelected)
        this->DeleteSelectedText();
    theLine = this->mLines[this->mCaretLine];
    unsigned long theLineLength = _mbslen(reinterpret_cast<const unsigned char*>((const char*)theLine));
    this->mWorkString = "";
    if (theLength == 0)
        return inString;
    if (this->mIsSingleLine) {
        while (theLineLength < this->mMaxChars && *inString != 0) {
            if (!this->mNumbersOnly || _ismbcdigit(GetMultiByteChar(inString))) {
                theChar.CopyString(inString, 1);
                this->mWorkString += theChar;
            }
            theLineLength++;
            inString = (const char*)_mbsinc(reinterpret_cast<const unsigned char*>(inString));
        }
        theLength = _mbslen(reinterpret_cast<const unsigned char*>((const char*)this->mWorkString));
        char* theTail = (char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)theLine), this->mCaretIndex);
        this->mWorkString += theTail;
        strcpy(theTail, "");
        theLine += this->mWorkString;
        this->mLines[this->mCaretLine] = theLine;
    } else {
        unsigned long theLineIndex = this->mCaretLine;
        this->mWorkString.CopyString(theLine, this->mCaretIndex);
        const char* theTail = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)theLine), this->mCaretIndex);
        this->mWorkString += inString;
        this->mWorkString += theTail;
        const char* theCursor = this->mWorkString;
        while (_mbscmp(reinterpret_cast<const unsigned char*>(theCursor), (const unsigned char*)"") != 0) {
            theLine = "";
            float theWidth = 0.0f;
            unsigned long theCount = 0;
            unsigned long theLastSpace = 0;
            const char* theStart = theCursor;
            bool theWrap = false;
            do {
                if (theSpaces == 0) {
                    theChar.CopyString(theCursor, 1);
                    if (_mbsnbcmp(reinterpret_cast<const unsigned char*>(theCursor), (const unsigned char*)"\n", 1) == 0) {
                        theNewLine = true;
                        theCursor = (const char*)_mbsinc(reinterpret_cast<const unsigned char*>(theCursor));
                        theLine += "\n";
                    } else if (_ismbcspace(GetMultiByteChar(theCursor))) {
                        theLastSpace = theCount + 1;
                    } else if (_mbsnbcmp(reinterpret_cast<const unsigned char*>(theCursor), (const unsigned char*)"\r", 1) == 0) {
                        theChar = "";
                    } else if (_mbsnbcmp(reinterpret_cast<const unsigned char*>(theCursor), (const unsigned char*)"\t", 1) == 0) {
                        theSpaces = 4;
                        theChar = "";
                    }
                } else {
                    theChar = " ";
                    theSpaces--;
                }
                this->GetGEText()->SetText(theChar);
                theWidth += this->GetGEText()->GetTextExtent().GetWidth();
                if (theWidth > this->mTextRectangle.GetWidth() || theNewLine) {
                    theWrap = true;
                    break;
                }
                theLine += theChar;
                if (theSpaces == 0)
                    theCursor = (const char*)_mbsinc(reinterpret_cast<const unsigned char*>(theCursor));
                theCount++;
            } while (_mbscmp(reinterpret_cast<const unsigned char*>(theCursor), (const unsigned char*)"") != 0 &&
                     theCursor != theStart);
            if (!theWrap) {
                this->mLines[theLineIndex] = theLine;
                continue;
            }
            if (theLastSpace > 0 && !theNewLine && theLastSpace < theCount + 1) {
                theCursor = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)this->mWorkString), theLastSpace);
                this->mLines[theLineIndex].CopyString(theLine, theLastSpace);
                if (theLineIndex == this->mCaretLine && this->mCaretIndex > (long)this->mLines[theLineIndex].GetLength()) {
                    this->mCaretIndex -= theLastSpace;
                    this->mCaretLine++;
                }
            } else
                this->mLines[theLineIndex] = theLine;
            if (theLineIndex == this->mLines.size() - 1)
                this->mLines.push_back(theEmpty);
            theNewLine = false;
            theLineIndex++;
            theChar = theCursor;
            theChar += this->mLines[theLineIndex];
            this->mWorkString = theChar;
            theCursor = this->mWorkString;
        }
    }
    return inString;
}

// SYNTHETIC: LLE 0x1006a9a5
// GetMultiByteChar (lle/UIControl.h): its COMDAT follows AddChar, its first user

// FUNCTION: LLE 0x1006a9cb
// Types an IME result string into the box character by character, then restarts the composition at the caret
void UIEditBox::AddIMEResultString(const UWideString& inString)
{
    bool theIMEActive = this->mIMEActive;
    const char* theCursor = inString;
    this->RemoveIMECompositionString();
    this->mIMEActive = false;
    while (*theCursor != 0) {
        this->mForm->GetManager()->UpdateTranslatedKey(theCursor, 1);
        theCursor = (const char*)_mbsinc(reinterpret_cast<const unsigned char*>(theCursor));
    }
    this->mIMEStartIndex = this->mCaretIndex;
    this->mIMEStartLine = this->mCaretLine;
    this->mIMEActive = theIMEActive;
}

// FUNCTION: LLE 0x1006aa2f
// Deletes the character before the caret (joining onto the previous line at a line start); nothing at the very start
void UIEditBox::BackSpaceChar()
{
    if (!this->mIsSingleLine && this->mCaretLine != 0) {
        if (this->mCaretIndex == 0) {
            this->DecrementCaretPos();
            this->DeleteChar();
            this->ComputeCaretLocation(true);
        } else {
            this->DecrementCaretPos();
            this->DeleteChar();
        }
    } else if (this->mCaretIndex > 0) {
        this->DecrementCaretPos();
        this->DeleteChar();
    }
}

// FUNCTION: LLE 0x1006aa7b
// Converts a mouse location to the caret line and character index it points at, clamped to the visible text
void UIEditBox::CalculateCaretFromMouseLocation(unsigned long& outLine, long& outIndex,
                                                const GE2DPoint& inLocation) const
{
    UWideString theChar;
    long theX = UMFloor(this->mTextRectangle.GetLeft());
    long theIndex = this->mDrawingIndex;
    if (this->mIsSingleLine)
        outLine = 0;
    else {
        if (inLocation.GetY() < this->mTextRectangle.GetTop())
            outLine = this->mScrollBar->GetSlider()->GetCurrentRangeValue();
        else if (inLocation.GetY() > this->mTextRectangle.GetBottom())
            outLine = this->mScrollBar->GetSlider()->GetCurrentRangeValue() + this->mVisibleLineCount - 1;
        else
            outLine = this->mScrollBar->GetSlider()->GetCurrentRangeValue() +
                      UMFloor((inLocation.GetY() - this->mTextRectangle.GetTop()) / this->GetGEText()->GetFont()->GetHeight());
        if (outLine >= this->mLines.size())
            outLine = this->mLines.size() - 1;
    }
    if (inLocation.GetX() > this->mTextRectangle.GetLeft()) {
        const char* theCursor = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)this->mLines[outLine]),
                                                      this->mDrawingIndex);
        long theLength;
        if (!this->mIsSingleLine && _mbschr(reinterpret_cast<const unsigned char*>(theCursor), '\n'))
            theLength = this->mLines[outLine].GetLength() - 1;
        else
            theLength = this->mLines[outLine].GetLength();
        while (++theIndex <= theLength) {
            theChar.CopyString(theCursor, 1);
            char* theEnd = (char*)_mbsinc(reinterpret_cast<const unsigned char*>((const char*)theChar));
            strcpy(theEnd, "");
            this->GetGEText()->SetText(theChar);
            theX += UMFloor(this->GetGEText()->GetTextExtent().GetWidth());
            float theRight = (float)theX;
            if (theRight > this->mTextRectangle.GetRight() || theRight > inLocation.GetX())
                break;
            theCursor = (const char*)_mbsinc(reinterpret_cast<const unsigned char*>(theCursor));
        }
        theIndex--;
    }
    outIndex = theIndex;
}

// FUNCTION: LLE 0x1006ad00
// Replaces the active IME composition text with a new string and attributes and moves the caret inside it
long UIEditBox::ChangeIMECompositionString(const UWideString& inString, const UString& inAttributes,
                                           unsigned long inCursor)
{
    if (this->mIMEActive == true) {
        this->RemoveIMECompositionString();
        this->AddChar(inString);
        this->mIMEAttributes = inAttributes;
        this->mIMEStartIndex = this->mCaretIndex;
        this->mIMEStartLine = this->mCaretLine;
        this->mIMECompositionLength = inString.GetLength();
        this->IncrementCaretPos(_mbsnccnt(reinterpret_cast<const unsigned char*>((const char*)inString), inCursor));
    }
    return kErrorOK;
}

// FUNCTION: LLE 0x1006ad6f
// Changes the font of the edit box's live and draw-copy texts
void UIEditBox::ChangeFont(GEFont* inFont)
{
    if (this->mText) {
        this->mText->SetFont(inFont);
        this->mCopiedText->SetFont(inFont);
    }
}

// FUNCTION: LLE 0x1006ad98
// Resizes the edit box to a new width and recomputes the text area (leaving room for a visible scroll bar)
void UIEditBox::ChangeWidth(unsigned long inWidth)
{
    GERectangle theRectangle;
    theRectangle = this->mRectangle;
    theRectangle.SetRight(theRectangle.GetLeft() + inWidth - 1.0f);
    UIControl::SetRectangle(theRectangle);
    this->mTextRectangle.SetTop(this->GetRectangle().GetTop() + this->GetBorderThickness());
    this->mTextRectangle.SetLeft(this->GetRectangle().GetLeft() + this->GetBorderThickness());
    this->mTextRectangle.mBottom = this->mRectangle.GetBottom() - this->mBorderThickness;
    if (this->mScrollBarVisible)
        this->mTextRectangle.mRight = this->mRectangle.GetRight() - this->mBorderThickness - kScrollBarWidth;
    else
        this->mTextRectangle.mRight = this->mRectangle.GetRight() - this->mBorderThickness;
}

// FUNCTION: LLE 0x1006ae42
// Shows or hides the scroll bar and updates its range as the line count crosses the visible line count
void UIEditBox::CheckScrollBar()
{
    if (this->mScrollBar == NULL)
        return;
    if (this->mLines.size() <= this->mVisibleLineCount) {
        if (this->mScrollBar->GetSlider()->GetMaxRange() > 0) {
            this->mScrollBar->SetRange(0, 0);
            this->ComputeCaretLocation(true);
        }
        if (this->mScrollBarVisible == true) {
            this->mScrollBarVisible = false;
            this->SetRectangle(GERectangle(this->mRectangle.GetTopLeft(),
                                     GE2DPoint(this->mRectangle.GetRight() - kScrollBarWidth, this->mRectangle.GetBottom())));
            this->mTextRectangle.SetRight(this->GetRectangle().GetRight() - this->GetBorderThickness());
        }
    } else {
        if (this->mScrollBar->GetSlider()->GetMaxRange() != this->mLines.size() - this->mVisibleLineCount) {
            this->mScrollBar->SetRange(0, this->mLines.size() - this->mVisibleLineCount);
            this->ComputeCaretLocation(true);
        }
        if (this->mScrollBarVisible == false) {
            this->mScrollBarVisible = true;
            this->SetRectangle(GERectangle(this->mRectangle.GetTopLeft(),
                                     GE2DPoint(this->mRectangle.GetRight() + kScrollBarWidth, this->mRectangle.GetBottom())));
        }
    }
}

// FUNCTION: LLE 0x1006afca
// Ends an active IME composition, removing its uncommitted text
long UIEditBox::CloseIMECompositionString()
{
    if (this->mIMEActive == true) {
        this->RemoveIMECompositionString();
        this->mIMEActive = false;
    }
    return kErrorOK;
}

// FUNCTION: LLE 0x1006afe6
// Recomputes the caret's screen position (optionally its remembered x) and scrolls so the caret line is visible
void UIEditBox::ComputeCaretLocation(bool inUpdateCaretX)
{
    GE2DPoint theLocation;
    this->GetLocationFromIndex(this->mCaretIndex, this->mCaretLine, theLocation);
    if (inUpdateCaretX)
        this->mCaretX = theLocation.GetX() - this->mTextRectangle.GetLeft();
    if (this->mScrollBar == NULL)
        return;
    if (this->mCaretLine < (unsigned long)this->mScrollBar->GetSlider()->GetCurrentRangeValue())
        this->mScrollBar->SetCurrentRangeValue(this->mCaretLine);
    else if (this->mCaretLine >= this->mScrollBar->GetSlider()->GetCurrentRangeValue() + this->mVisibleLineCount)
        this->mScrollBar->SetCurrentRangeValue(this->mCaretLine - this->mVisibleLineCount + 1);
}

// FUNCTION: LLE 0x1006b05c
// Picks the first visible character of a single-line box so the text before the caret fits the text area
void UIEditBox::ComputeDrawingIndex()
{
    this->mWorkString.CopyString(this->mLines[this->mCaretLine], this->mCaretIndex);
    const char* theCursor = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)this->mWorkString), this->mCaretIndex);
    theCursor = this->mWorkString;
    this->mDrawingIndex = 0;
    while (this->mDrawingIndex < this->mCaretIndex) {
        this->GetGEText()->SetText(theCursor);
        if (this->GetGEText()->GetTextExtent().GetWidth() < this->mTextRectangle.GetWidth())
            break;
        this->mDrawingIndex++;
        theCursor = (const char*)_mbsinc(reinterpret_cast<const unsigned char*>(theCursor));
    }
}

// FUNCTION: LLE 0x1006b122
// Copies the selected text to the Windows clipboard; the memory block leaks when the clipboard cannot be opened
void UIEditBox::Copy()
{
    if (!this->mIsTextSelected)
        return;
    UWideString theText("");
    this->GetSelectedText(theText);
    HGLOBAL theMemory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, theText.GetAllocatedLength() + 1);
    char* theBuffer = (char*)GlobalLock(theMemory);
    strcpy(theBuffer, theText);
    GlobalUnlock(theMemory);
    if (!OpenClipboard(NULL)) {
        GlobalFree(theMemory);   // Fixed: the block used to leak when the clipboard could not be opened
        return;
    }
    EmptyClipboard();
    SetClipboardData(CF_TEXT, theMemory);
    CloseClipboard();
}

// FUNCTION: LLE 0x1006b1a0
// Prepares the frame's draw copy: caret blink and position, visible lines (stars for a password box), IME and
// selection ranges clipped to the visible lines
long UIEditBox::CopyData()
{
    GE2DPoint theLocation;
    UWideString thePassword;
    this->CheckScrollBar();
    long theResult = UIControl::CopyData();
    if (this->mCursorPoll->GetIsRunning() == true && this->mCursorPoll->Poll() > 0)
        this->mCursorOn = !this->mCursorOn;
    if (this->mCursorOn) {
        if (this->mScrollBar != NULL &&
            (this->mCaretLine < (unsigned long)this->mScrollBar->GetSlider()->GetCurrentRangeValue() ||
             this->mCaretLine >= this->mScrollBar->GetSlider()->GetCurrentRangeValue() + this->mVisibleLineCount))
            this->mCopiedCursorOn = false;
        else
            this->mCopiedCursorOn = true;
        if (this->mCopiedCursorOn) {
            this->GetLocationFromIndex(this->mCaretIndex, this->mCaretLine, theLocation);
            this->mCursor->SetStartPoint(theLocation);
            this->mCursor->SetEndPoint(GE2DPoint(theLocation.GetX(), theLocation.GetY() + this->GetGEText()->GetFont()->GetHeight()));
            this->mCursor->SetColor(this->mCursorColor);
        }
    } else
        this->mCopiedCursorOn = false;
    unsigned long theFirstLine;
    unsigned long theLastLine;
    if (this->mIsSingleLine) {
        const char* theString;
        if (!this->mIsPasswordBox)
            theString = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)this->mLines[this->mCaretLine]), this->mDrawingIndex);
        else {
            thePassword.Clear();
            for (unsigned long i = 0; i < this->mLines[this->mCaretLine].GetLength(); i++)
                thePassword += UWideString("*");
            theString = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)thePassword), this->mDrawingIndex);
        }
        theLocation = GE2DPoint(this->mTextRectangle.GetLeft(), this->mTextRectangle.GetTop());
        this->GetGEText()->SetLocation(theLocation);
        this->mText->SetText(UWideString(theString));
        this->mDisplayLines[0] = theString;
        if (this->mIMEActive == true) {
            this->mCopiedIMEStartIndex = this->mIMEStartIndex;
            if (this->mCopiedIMEStartIndex < (unsigned long)this->mDrawingIndex)
                this->mCopiedIMEStartIndex = 0;
            else
                this->mCopiedIMEStartIndex -= this->mDrawingIndex;
            this->mCopiedIMEStartLine = 0;
            this->mCopiedIMEEndLine = 0;
            this->mCopiedIMEEndIndex = this->mIMECompositionLength + this->mCopiedIMEStartIndex;
            this->mCopiedIMEActive = true;
            this->mCopiedIMETextColor = this->mIMETextColor;
            this->mCopiedIMEAttributes = this->mIMEAttributes;
        } else
            this->mCopiedIMEActive = false;
        theFirstLine = 0;
        theLastLine = 1;
    } else {
        theFirstLine = this->mScrollBar->GetSlider()->GetCurrentRangeValue();
        theLastLine = theFirstLine + this->mVisibleLineCount;
        for (unsigned long i = theFirstLine; i < theLastLine; i++) {
            if (i < this->mLines.size()) {
                if (_mbschr(reinterpret_cast<const unsigned char*>((const char*)this->mLines[i]), '\n'))
                    this->mDisplayLines[i - theFirstLine].CopyString(this->mLines[i], this->mLines[i].GetLength() - 1);
                else
                    this->mDisplayLines[i - theFirstLine] = this->mLines[i];
            } else
                this->mDisplayLines[i - theFirstLine] = "";
        }
        theResult = this->mScrollBar->CopyData();
        if (this->mIMEActive == true) {
            this->mCopiedIMEActive = true;
            this->mCopiedIMETextColor = this->mIMETextColor;
            this->mCopiedIMEAttributes = this->mIMEAttributes;
            this->mCopiedIMEStartIndex = this->mIMEStartIndex;
            this->mCopiedIMEStartLine = this->mIMEStartLine;
            if (this->mIMEStartLine < theFirstLine) {
                this->mCopiedIMEStartLine = 0;
                this->mCopiedIMEStartIndex = 0;
            } else if (this->mIMEStartLine >= theLastLine)
                this->mCopiedIMEActive = false;
            else
                this->mCopiedIMEStartLine = this->mIMEStartLine - theFirstLine;
            long theLength = _mbslen(_mbsninc(reinterpret_cast<const unsigned char*>((const char*)this->mLines[this->mIMEStartLine]),
                                              this->mIMEStartIndex));
            if (theLength < (long)this->mIMECompositionLength) {
                long theCount = this->mIMECompositionLength - theLength;
                unsigned long theLine = this->mIMEStartLine + 1;
                while (theCount > 0) {
                    theCount -= this->mLines[theLine].GetLength();
                    theLine++;
                }
                this->mCopiedIMEEndLine = theLine - theFirstLine - 1;
                this->mCopiedIMEEndIndex = this->mLines[theLine - 1].GetLength() + theCount;
            } else {
                this->mCopiedIMEEndIndex = this->mIMECompositionLength + this->mIMEStartIndex;
                this->mCopiedIMEEndLine = this->mIMEStartLine - theFirstLine;
            }
        } else
            this->mCopiedIMEActive = false;
    }
    this->mCopiedTextRectangle = this->mTextRectangle;
    *this->mCopiedText = *this->GetGEText();
    if (this->mIsTextSelected) {
        this->mSelectionRhombus.SetColor(this->mSelectedBgColor);
        this->mCopiedIsTextSelected = true;
        if (this->mCaretLine < this->mSelectionStartLine) {
            this->mCopiedSelectionEndLine = this->mSelectionStartLine;
            this->mCopiedSelectionStartLine = this->mCaretLine;
            this->mCopiedSelectionEndIndex = this->mSelectionStartIndex;
            this->mCopiedSelectionStartIndex = this->mCaretIndex;
        } else {
            this->mCopiedSelectionStartLine = this->mSelectionStartLine;
            this->mCopiedSelectionEndLine = this->mCaretLine;
            this->mCopiedSelectionStartIndex = this->mSelectionStartIndex;
            this->mCopiedSelectionEndIndex = this->mCaretIndex;
        }
        if (this->mCopiedSelectionStartLine == this->mCopiedSelectionEndLine && this->mCaretIndex < this->mSelectionStartIndex) {
            this->mCopiedSelectionEndIndex = this->mSelectionStartIndex;
            this->mCopiedSelectionStartIndex = this->mCaretIndex;
        }
        if (this->mCopiedSelectionStartIndex <= this->mDrawingIndex)
            this->mCopiedSelectionStartIndex = 0;
        else
            this->mCopiedSelectionStartIndex -= this->mDrawingIndex;
        if (this->mCopiedSelectionEndIndex <= this->mDrawingIndex)
            this->mCopiedSelectionEndIndex = 0;
        else
            this->mCopiedSelectionEndIndex -= this->mDrawingIndex;
        if (this->mCopiedSelectionStartLine < theFirstLine) {
            this->mCopiedSelectionStartLine = 0;
            this->mCopiedSelectionStartIndex = 0;
        } else if (this->mCopiedSelectionStartLine >= theLastLine)
            this->mCopiedIsTextSelected = false;
        else
            this->mCopiedSelectionStartLine -= theFirstLine;
        if (this->mCopiedSelectionEndLine >= theLastLine) {
            this->mCopiedSelectionEndLine = theLastLine - theFirstLine - 1;
            this->mCopiedSelectionEndIndex = this->mDisplayLines[this->mCopiedSelectionEndLine].GetLength();
        } else if ((long)(this->mCopiedSelectionEndLine - theFirstLine) >= 0)
            this->mCopiedSelectionEndLine -= theFirstLine;
        else
            this->mCopiedIsTextSelected = false;
    } else
        this->mCopiedIsTextSelected = false;
    return theResult;
}

// FUNCTION: LLE 0x1006b7fc
// Copies the selected text to the clipboard, then deletes it
void UIEditBox::Cut()
{
    if (this->mIsTextSelected) {
        this->Copy();
        this->DeleteSelectedText();
    }
}

// FUNCTION: LLE 0x1006b816
// Moves the caret one character left, wrapping to the end of the previous line and scrolling left when needed
void UIEditBox::DecrementCaretPos()
{
    this->mCaretIndex--;
    if (this->mCaretIndex >= 0) {
        if (this->mDrawingIndex != 0 && this->mCaretIndex <= this->mDrawingIndex)
            this->ScrollLeft();
        else
            this->ComputeCaretLocation(true);
        this->SetDirty(true);
    } else if (this->mCaretLine > 0) {
        this->mCaretLine--;
        this->mCaretIndex = this->mLines[this->mCaretLine].GetLength() - 1;
        this->ComputeCaretLocation(true);
        this->SetDirty(true);
    } else
        this->mCaretIndex = 0;
}

// FUNCTION: LLE 0x1006b884
// Deletes the character after the caret
void UIEditBox::DeleteChar()
{
    long theIndex = this->mCaretIndex;
    unsigned long theLine = this->mCaretLine;
    this->IncrementCaretPos(1);
    long theEndIndex = this->mCaretIndex;
    unsigned long theEndLine = this->mCaretLine;
    this->mCaretIndex = theIndex;
    this->mCaretLine = theLine;
    this->DeleteText(theIndex, theLine, theEndIndex, theEndLine);
}

// FUNCTION: LLE 0x1006b8d7
// Removes one line, keeping the caret on a valid line and scrolling up by one when the box is scrolled down
void UIEditBox::DeleteLine(unsigned long inLine)
{
    std::vector<UWideString>::iterator theLine = this->mLines.begin() + inLine;
    if (this->mCaretLine > inLine)
        this->mCaretLine--;
    else if (this->mCaretLine == inLine) {
        if (this->mCaretLine == this->mLines.size() - 1)
            this->mCaretLine--;
        this->mCaretIndex = this->mLines[this->mCaretLine].GetLength();
    }
    this->mLines.erase(theLine);
    if (this->mScrollBar == NULL || !(this->mLines.size() >= this->mVisibleLineCount) ||
        !(this->mScrollBar->GetSlider()->GetCurrentRangeValue() > 0))
        return;
    this->mScrollBar->SetCurrentRangeValue(this->mScrollBar->GetSlider()->GetCurrentRangeValue() - 1);
    this->ComputeCaretLocation(true);
    this->SetDirty(true);
}

// FUNCTION: LLE 0x1006b9c1
// Deletes the selected text and clears the selection (not while an IME composition is open)
void UIEditBox::DeleteSelectedText()
{
    if (!this->mIsTextSelected || this->mIMEActive)
        return;
    this->mIsTextSelected = false;
    this->mSelecting = false;
    unsigned long theStartIndex;
    unsigned long theStartLine;
    unsigned long theEndIndex;
    unsigned long theEndLine;
    if (this->mCaretLine == this->mSelectionStartLine) {
        theStartLine = this->mCaretLine;
        theEndLine = this->mCaretLine;
        if (this->mCaretIndex < this->mSelectionStartIndex) {
            theStartIndex = this->mCaretIndex;
            theEndIndex = this->mSelectionStartIndex;
        } else {
            theStartIndex = this->mSelectionStartIndex;
            theEndIndex = this->mCaretIndex;
        }
    } else if (this->mCaretLine < this->mSelectionStartLine) {
        theStartIndex = this->mCaretIndex;
        theStartLine = this->mCaretLine;
        theEndIndex = this->mSelectionStartIndex;
        theEndLine = this->mSelectionStartLine;
    } else {
        theStartIndex = this->mSelectionStartIndex;
        theStartLine = this->mSelectionStartLine;
        theEndIndex = this->mCaretIndex;
        theEndLine = this->mCaretLine;
    }
    this->DeleteText(theStartIndex, theStartLine, theEndIndex, theEndLine);
}

// FUNCTION: LLE 0x1006ba3f
// Deletes the text between two caret positions, re-wraps the following lines and puts the caret at the start of the gap
void UIEditBox::DeleteText(unsigned long inStartIndex, unsigned long inStartLine, unsigned long inEndIndex,
                           unsigned long inEndLine)
{
    UWideString theString;
    if (inStartLine == inEndLine) {
        theString.CopyString(this->mLines[inStartLine], inStartIndex);
        theString += (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)this->mLines[inStartLine]),
                                           inEndIndex);
        this->mLines[inStartLine] = theString;
    } else {
        theString.CopyString(this->mLines[inStartLine], inStartIndex);
        *(this->mLines.begin() + inStartLine) = theString;
        if (inEndIndex == (this->mLines.begin() + inEndLine)->GetLength())
            this->DeleteLine(inEndLine);
        else {
            const char* theTail = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)this->mLines[inEndLine]),
                                                        inEndIndex);
            this->mLines[inEndLine] = theTail;
            if (this->mCaretIndex > (long)(this->mLines.begin() + inEndLine)->GetLength())
                this->mCaretIndex = this->mLines[inEndLine].GetLength();
        }
    }
    if (inEndLine > 0)
        for (unsigned long i = inStartLine + 1; i < inEndLine; i++)
            this->DeleteLine(inStartLine + 1);
    if (inStartLine > 0 && !_mbschr(reinterpret_cast<const unsigned char*>((const char*)this->mLines[inStartLine - 1]), '\n')) {
        inStartLine--;
        inStartIndex += this->mLines[inStartLine].GetLength();
    }
    unsigned long theLine = inStartLine;
    while (theLine + 1 < this->mLines.size()) {
        if (_mbschr(reinterpret_cast<const unsigned char*>((const char*)this->mLines[theLine]), '\n'))
            break;
        this->mCaretLine = theLine;
        this->mCaretIndex = this->mLines[theLine].GetLength();
        theString = this->mLines[theLine + 1];
        this->DeleteLine(theLine + 1);
        this->AddChar(theString);
        theLine++;
        if (theLine >= this->mLines.size() || theString == this->mLines[theLine])
            break;
    }
    this->mCaretIndex = 0;
    this->mDrawingIndex = 0;
    this->mCaretLine = inStartLine;
    this->IncrementCaretPos(inStartIndex);
    this->SetDirty(true);
    this->ComputeCaretLocation(true);
}

// FUNCTION: LLE 0x1006bdab
// Clears the edit box to one empty line, with the caret at the start and no selection
void UIEditBox::DeleteText()
{
    UWideString theEmpty;
    this->mIsTextSelected = false;
    this->mCaretIndex = 0;
    this->mDrawingIndex = 0;
    this->mCaretLine = 0;
    this->mLines.erase(this->mLines.begin() + 1, this->mLines.end());
    this->mLines[this->mCaretLine] = theEmpty;
    this->SetDirty(true);
}

// FUNCTION: LLE 0x1006be4b
// Draws the control frame, scroll bar, selection highlight, IME underlines and blinking caret from the draw copy
long UIEditBox::DrawPolygons()
{
    UWideString theString;
    GELine theLine;
    GERectangle theRectangle;
    GE2DPoint theStart;
    long theResult = UIControl::DrawPolygons();
    if (this->mScrollBarVisible)
        theResult = this->mScrollBar->DrawPolygons();
    if (this->mCopiedIsTextSelected) {
        for (unsigned long i = this->mCopiedSelectionStartLine; i <= this->mCopiedSelectionEndLine; i++) {
            if (i == this->mCopiedSelectionStartLine) {
                if (i == this->mCopiedSelectionEndLine)
                    this->GetSelectedTextRectangle(i, this->mCopiedSelectionStartIndex, this->mCopiedSelectionEndIndex, theRectangle);
                else
                    this->GetSelectedTextRectangle(i, this->mCopiedSelectionStartIndex, this->mDisplayLines[i].GetLength(), theRectangle);
            } else if (i == this->mCopiedSelectionEndLine)
                this->GetSelectedTextRectangle(i, 0, this->mCopiedSelectionEndIndex, theRectangle);
            else
                this->GetSelectedTextRectangle(i, 0, this->mDisplayLines[i].GetLength(), theRectangle);
            this->mSelectionRhombus.SetRectangle(theRectangle);
            theResult = this->GetRasterizer()->Draw(&this->mSelectionRhombus, this->GetScreenIndex());
        }
    }
    if (this->mCopiedIMEActive == true) {
        int theOffset = 0;
        for (unsigned long i = this->mCopiedIMEStartLine; i <= this->mCopiedIMEEndLine; i++) {
            unsigned long j;
            j = i == this->mCopiedIMEStartLine ? this->mCopiedIMEStartIndex : 0;
            for (; j <= (i == this->mCopiedIMEEndLine ? this->mCopiedIMEEndIndex : this->mDisplayLines[i].GetLength()); j++) {
                theString.CopyString(this->mDisplayLines[i], j);
                this->mCopiedText->SetText(theString);
                float theX = this->mCopiedText->GetTextExtent().GetWidth() + this->mCopiedTextRectangle.GetLeft();
                const char* theChar = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)this->mDisplayLines[i]), j);
                theString.CopyString(theChar, 1);
                if ((this->mCopiedIMEAttributes[theOffset] == 1 || this->mCopiedIMEAttributes[theOffset] == 3) &&
                    this->mCopiedTextRectangle.GetRight() > theX) {
                    theLine.SetStartPoint(GE2DPoint(theX, (float)((i + 1) * this->mCopiedText->GetFont()->GetHeight()) +
                                                              this->mCopiedTextRectangle.GetTop() - 1.0f));
                    this->mCopiedText->SetText(theString);
                    theLine.SetColor(this->mCopiedIMETextColor);
                    float theEnd;
                    if (this->mCopiedText->GetTextExtent().GetWidth() + theX < this->mCopiedTextRectangle.GetRight())
                        theEnd = this->mCopiedText->GetTextExtent().GetWidth() + theX;
                    else
                        theEnd = this->mCopiedTextRectangle.GetRight();
                    theLine.GetStartPoint(theStart);
                    theLine.SetEndPoint(GE2DPoint(theEnd, theStart.GetY()));
                    theResult = this->GetRasterizer()->Draw(&theLine, this->GetScreenIndex());
                }
                theOffset += _mbsnbcnt(reinterpret_cast<const unsigned char*>((const char*)theString), 1);
            }
        }
    }
    if (this->mCopiedCursorOn)
        theResult = this->GetRasterizer()->Draw(this->mCursor, this->GetScreenIndex());
    return theResult;
}

// FUNCTION: LLE 0x1006c1fd
// Draws the visible lines, then redraws the IME composition text in its own colour, then the scroll bar
long UIEditBox::DrawTextA()
{
    UWideString theString;
    unsigned long i = 0;
    this->mDrawCount--;
    long theResult = kErrorOK;
    for (std::vector<UWideString>::iterator theLine = this->mDisplayLines.begin(); theLine < this->mDisplayLines.end();
         ++theLine, i++) {
        this->mCopiedText->SetText(*theLine);
        float theY = (float)(i * this->mCopiedText->GetFont()->GetHeight()) + this->mCopiedTextRectangle.GetTop();
        this->mCopiedText->SetLocation(this->mCopiedTextRectangle.GetLeft(), theY);
        theResult = this->GetForm()->DrawClippedText(this->mCopiedTextRectangle, this->mCopiedText, this);
        if (this->mCopiedIMEActive != true || !(i >= this->mCopiedIMEStartLine) || !(i <= this->mCopiedIMEEndLine))
            continue;
        if (this->mCopiedIMEStartLine == i) {
            theString.CopyString(this->mDisplayLines[i], this->mCopiedIMEStartIndex);
            this->mCopiedText->SetText(theString);
            this->mCopiedText->SetLocation(this->mCopiedText->GetTextExtent().GetRight() + this->mCopiedTextRectangle.GetLeft(),
                                     theY);
        }
        theString = this->mDisplayLines[i];
        if (this->mCopiedIMEEndLine == i) {
            char* theEnd = (char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)theString), this->mCopiedIMEEndIndex);
            strcpy(theEnd, "");
        }
        const char* theText;
        if (this->mCopiedIMEStartLine == i)
            theText = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)theString), this->mCopiedIMEStartIndex);
        else
            theText = theString;
        GEColor theColor;
        theColor = this->mCopiedText->GetFgColor();
        this->mCopiedText->SetFgColor(this->mCopiedIMETextColor);
        this->mCopiedText->SetText(UWideString(theText));
        theResult = this->GetForm()->DrawClippedText(this->mCopiedTextRectangle, this->mCopiedText, this);
        this->mCopiedText->SetFgColor(theColor);
    }
    if (this->mScrollBarVisible)
        theResult = this->mScrollBar->DrawTextA();
    return theResult;
}

// FUNCTION: LLE 0x1006c43d
// Returns the screen position of a character index on a line (x from the caret line's text, y from the line)
void UIEditBox::GetLocationFromIndex(unsigned long inIndex, unsigned long inLine, GE2DPoint& outLocation) const
{
    UWideString theString;
    UWideString thePassword;
    const char* theText;
    if (!this->mIsPasswordBox)
        theText = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)this->mLines[this->mCaretLine]),
                                        this->mDrawingIndex);
    else {
        thePassword.Clear();
        for (unsigned long i = 0; i < this->mLines[this->mCaretLine].GetLength(); i++)
            thePassword += UWideString("*");
        theText = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)thePassword), this->mDrawingIndex);
    }
    theString.CopyString(theText, inIndex - this->mDrawingIndex);
    char* theEnd = (char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)theString), inIndex - this->mDrawingIndex);
    strcpy(theEnd, "");
    this->GetGEText()->SetText(theString);
    outLocation = GE2DPoint(this->GetGEText()->GetTextExtent().GetWidth() + this->mTextRectangle.GetLeft(), this->mTextRectangle.GetTop());
    if (this->mScrollBar != NULL)
        outLocation.SetY((float)((inLine - this->mScrollBar->GetSlider()->GetCurrentRangeValue()) *
                                 this->GetGEText()->GetFont()->GetHeight()) + this->mTextRectangle.GetTop());
}

// FUNCTION: LLE 0x1006c5d9
// Returns the selected text, ordering the caret and selection anchor into start and end
void UIEditBox::GetSelectedText(UWideString& outString) const
{
    if (!this->mIsTextSelected)
        return;
    unsigned long theStartIndex;
    unsigned long theStartLine;
    unsigned long theEndIndex;
    unsigned long theEndLine;
    if (this->mCaretLine == this->mSelectionStartLine) {
        theStartLine = this->mCaretLine;
        theEndLine = this->mCaretLine;
        if (this->mCaretIndex < this->mSelectionStartIndex) {
            theStartIndex = this->mCaretIndex;
            theEndIndex = this->mSelectionStartIndex;
        } else {
            theStartIndex = this->mSelectionStartIndex;
            theEndIndex = this->mCaretIndex;
        }
    } else if (this->mCaretLine < this->mSelectionStartLine) {
        theStartIndex = this->mCaretIndex;
        theStartLine = this->mCaretLine;
        theEndIndex = this->mSelectionStartIndex;
        theEndLine = this->mSelectionStartLine;
    } else {
        theStartIndex = this->mSelectionStartIndex;
        theStartLine = this->mSelectionStartLine;
        theEndIndex = this->mCaretIndex;
        theEndLine = this->mCaretLine;
    }
    this->GetText(theStartLine, theEndLine, theStartIndex, theEndIndex, outString);
}

// FUNCTION: LLE 0x1006c64a
// Returns the screen rectangle covering a character range on a visible line, clipped to the text area
void UIEditBox::GetSelectedTextRectangle(unsigned long inLine, unsigned long inStartIndex, unsigned long inEndIndex,
                                         GERectangle& outRectangle) const
{
    UWideString theString;
    long theHeight = this->mCopiedText->GetFont()->GetHeight();
    outRectangle.SetTop((float)(inLine * theHeight) + this->mTextRectangle.GetTop());
    outRectangle.SetBottom(theHeight + outRectangle.GetTop());
    theString.CopyString(this->mDisplayLines[inLine], inStartIndex);
    this->mCopiedText->SetText(theString);
    outRectangle.SetLeft(this->mCopiedText->GetTextExtent().GetWidth() + this->mTextRectangle.GetLeft());
    const char* theText = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)this->mDisplayLines[inLine]),
                                                inStartIndex);
    theString.CopyString(theText, inEndIndex - inStartIndex);
    this->mCopiedText->SetText(theString);
    outRectangle.SetRight(this->mCopiedText->GetTextExtent().GetWidth() + outRectangle.GetLeft());
    outRectangle.And(this->mTextRectangle);
}

// FUNCTION: LLE 0x1006c768
// Returns the text between two line/index positions, joining the full lines in between
void UIEditBox::GetText(unsigned long inStartLine, unsigned long inEndLine, unsigned long inStartIndex,
                        unsigned long inEndIndex, UWideString& outString) const
{
    UWideString theString;
    const char* theText;
    if (inStartLine == inEndLine) {
        theText = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)this->mLines[inStartLine]), inStartIndex);
        outString.CopyString(theText, inEndIndex - inStartIndex);
    } else {
        if (inStartIndex == this->mLines[inStartLine].GetLength())
            outString = "";
        else {
            theText = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)this->mLines[inStartLine]), inStartIndex);
            outString.CopyString(theText, this->mLines[inStartLine].GetLength() - inStartIndex);
        }
        for (unsigned long i = inStartLine + 1; i < inEndLine; i++)
            outString += this->mLines[i];
        if (inEndIndex == this->mLines[inEndLine].GetLength())
            outString += this->mLines[inEndLine];
        else {
            theString.CopyString(this->mLines[inEndLine], inEndIndex);
            outString += theString;
        }
    }
}

// FUNCTION: LLE 0x1006c89a
// Moves the caret a number of characters right, wrapping across lines and scrolling right when it passes the edge
void UIEditBox::IncrementCaretPos(long inCount)
{
    GE2DPoint theLocation;
    while (inCount > 0) {
        long theIndex = this->mCaretIndex + inCount;
        if ((unsigned long)theIndex < this->mLines[this->mCaretLine].GetLength()) {
            this->mCaretIndex = theIndex;
            break;
        }
        if ((unsigned long)theIndex == this->mLines[this->mCaretLine].GetLength()) {
            if (this->mCaretLine < this->mLines.size() - 1) {
                this->mCaretLine++;
                this->mCaretIndex = 0;
            } else
                this->mCaretIndex = theIndex;
            break;
        }
        if (this->mCaretLine < this->mLines.size() - 1) {
            inCount += this->mCaretIndex - this->mLines[this->mCaretLine].GetLength();
            this->mCaretIndex = 0;
            this->mCaretLine++;
        } else {
            this->mCaretIndex = this->mLines[this->mCaretLine].GetLength();
            break;
        }
    }
    this->GetLocationFromIndex(this->mCaretIndex, this->mCaretLine, theLocation);
    if (theLocation.mX > this->mTextRectangle.GetRight())
        this->ScrollRight(inCount);
    this->ComputeCaretLocation(true);
    this->SetDirty(true);
}

// FUNCTION: LLE 0x1006c9f3
// Inserts the clipboard text at the caret; the copy buffer is never freed
void UIEditBox::Paste()
{
    if (!IsClipboardFormatAvailable(CF_TEXT) || !OpenClipboard(NULL))
        return;
    HANDLE theData = GetClipboardData(CF_TEXT);
    char* theText = new char[GlobalSize(theData)];
    char* theLocked = (char*)GlobalLock(theData);
    if (theLocked)
        strcpy(theText, theLocked);
    GlobalUnlock(theData);
    CloseClipboard();
    // Fixed: the copy used to leak, and an uninitialised buffer was added when the clipboard could not be locked
    if (theLocked)
        this->AddChar(theText);
    delete[] theText;
}

// FUNCTION: LLE 0x1006ca57
// Handles a left click: forwards it to the scroll bar, or places the caret, starts a selection and captures input
long UIEditBox::MouseLButtonDown()
{
    long theResult = kErrorOK;
    if (this->mScrollBar != NULL && this->mScrollBar->GetRectangle().Contains(this->GetMouseLocation())) {
        theResult = this->mScrollBar->MouseLButtonDown();
        this->mScrollBarCaptured = true;
        this->CaptureMouse();
    } else if (!this->mIMEActive) {
        if (this->GetTabable() && this->mForm->GetTabStop() != this->mIndex)
            this->mForm->SetTabStop(this->mIndex);
        this->CalculateCaretFromMouseLocation(this->mCaretLine, this->mCaretIndex, this->GetMouseLocation());
        this->ComputeCaretLocation(true);
        this->mSelectionStartIndex = this->mCaretIndex;
        this->mSelecting = true;
        this->mSelectionStartLine = this->mCaretLine;
        this->SetupKeyboardForCapture();
        this->ButtonDown();
        this->CaptureMouse();
    }
    this->mCursorPoll->Stop();
    return theResult;
}

// FUNCTION: LLE 0x1006cb28
// Selects all text on a double click inside the text area; otherwise handles it as a normal control
long UIEditBox::MouseLButtonDoublePressed()
{
    long theResult = kErrorOK;
    if (this->mTextRectangle.Contains(this->mForm->GetMouseLocation()))
        this->SelectText();
    else
        theResult = UIControl::MouseLButtonDoublePressed();
    return theResult;
}

// FUNCTION: LLE 0x1006cb64
// Ends a scroll-bar drag or a selection drag and releases the mouse
long UIEditBox::MouseLButtonUp()
{
    long theResult = kErrorOK;
    if (this->mScrollBarCaptured) {
        theResult = this->mScrollBar->MouseLButtonUp();
        this->mScrollBarCaptured = false;
    } else
        this->mSelecting = false;
    this->mCursorPoll->Start();
    this->ReleaseCapturedMouse();
    return theResult;
}

// FUNCTION: LLE 0x1006cbaa
// Forwards a drag to the scroll bar, or extends the selection to the mouse, auto-scrolling while it is outside the text
long UIEditBox::MouseMove(const GE2DPoint& inLocation)
{
    long theResult = kErrorOK;
    if (this->mScrollBarCaptured)
        theResult = this->mScrollBar->MouseMove(inLocation);
    else if (this->mSelecting) {
        this->CalculateCaretFromMouseLocation(this->mCaretLine, this->mCaretIndex, inLocation);
        this->mIsTextSelected = this->mSelectionStartLine != this->mCaretLine || this->mSelectionStartIndex != this->mCaretIndex;
        if (!this->mTextRectangle.Contains(inLocation)) {
            unsigned long theCount;
            if (this->mScrollPoll->GetIsRunning())
                theCount = this->mScrollPoll->Poll();
            else {
                this->mScrollPoll->Start();
                theCount = 1;
            }
            if (this->mIsSingleLine) {
                if (theCount > 0) {
                    if (inLocation.GetX() < this->mTextRectangle.GetLeft()) {
                        for (unsigned long i = theCount; i > 0; i--)
                            this->DecrementCaretPos();
                    } else if (inLocation.GetX() > this->mTextRectangle.GetRight())
                        this->IncrementCaretPos(theCount);
                    else
                        this->mScrollPoll->Stop();
                }
            } else if (theCount > 0) {
                if (inLocation.GetY() < this->mTextRectangle.GetTop() && this->mCaretLine > 0) {
                    if ((long)(this->mCaretLine - theCount) < 0)
                        this->mCaretLine = 0;
                    else
                        this->mCaretLine -= theCount;
                    this->SetIndexFromUpDownOffset();
                    this->ComputeCaretLocation(false);
                } else if (inLocation.GetY() > this->mTextRectangle.GetBottom() && this->mCaretLine < this->mLines.size() - 1) {
                    if (this->mCaretLine + theCount >= this->mLines.size())
                        this->mCaretLine = this->mLines.size() - 1;
                    else
                        this->mCaretLine += theCount;
                    this->SetIndexFromUpDownOffset();
                    this->ComputeCaretLocation(false);
                } else
                    this->mScrollPoll->Stop();
            }
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1006cd80
// Starts an IME composition at the caret, deleting any selection; fails for a numbers-only box
long UIEditBox::OpenIMECompositionString()
{
    if (!this->mNumbersOnly) {
        if (!this->mIMEActive) {
            this->DeleteSelectedText();
            this->mIMEStartIndex = this->mCaretIndex;
            this->mIMEActive = true;
            this->mIMEStartLine = this->mCaretLine;
        }
        return kErrorOK;
    }
    return kErrorFailed;
}

// FUNCTION: LLE 0x1006cdc6
// Moves the caret down one line, keeping its x position
void UIEditBox::ProcessDownKey()
{
    if (this->mCaretLine < this->mLines.size() - 1) {
        this->mCaretLine++;
        this->SetIndexFromUpDownOffset();
    }
    this->ComputeCaretLocation(false);
}

// FUNCTION: LLE 0x1006ce0b
// Moves the caret to the end of the line (before its line break)
void UIEditBox::ProcessEndKey()
{
    this->mCaretIndex = this->mLines[this->mCaretLine].GetLength();
    if (_mbschr(reinterpret_cast<const unsigned char*>((const char*)this->mLines[this->mCaretLine]), '\n'))
        this->mCaretIndex--;
    if (this->mIsSingleLine)
        this->ComputeDrawingIndex();
    this->ComputeCaretLocation(true);
    this->SetDirty(true);
}

// FUNCTION: LLE 0x1006ce6f
// Breaks the line at the caret in a multi-line box (re-wrapping the rest); a single-line box fires its Enter trigger
void UIEditBox::ProcessEnterKey()
{
    EE_LOST_DEBUG(4);  // LOST-DEBUG (PROVISIONAL, owner experiment 2026-10-24): no independent evidence; base-game LLE identical (exact only at n = 4)
    UWideString theLine;
    UWideString theEmpty;
    if (!this->mIsSingleLine) {
        if (this->mIsTextSelected)
            this->DeleteSelectedText();
        if (this->mCaretLine == this->mLines.size() - 1)
            this->mLines.insert(this->mLines.begin() + (this->mCaretLine + 1), theEmpty);
        const char* theTail;
        if (this->mLines[this->mCaretLine].GetLength() > 0) {
            theLine = this->mLines[this->mCaretLine];
            this->mLines[this->mCaretLine].InsertString("\n", this->mCaretIndex, true);
            theTail = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)theLine), this->mCaretIndex);
        } else {
            this->mLines[this->mCaretLine] = "\n";
            theTail = (const char*)_mbsinc(reinterpret_cast<const unsigned char*>((const char*)this->mLines[this->mCaretLine]));
        }
        this->mCaretLine++;
        this->mCaretIndex = 0;
        this->AddChar(theTail);
        this->ComputeCaretLocation(true);
    } else
        this->FireTriggerEvent(kControlTriggerEnterPressed);
}

// FUNCTION: LLE 0x1006cfed
// Moves the caret to the start of the line and scrolls back to the first character
void UIEditBox::ProcessHomeKey()
{
    this->mDrawingIndex = 0;
    this->mCaretIndex = 0;
    this->ComputeCaretLocation(true);
    this->SetDirty(true);
}

// FUNCTION: LLE 0x1006d010
// Handles Ctrl keys: cut, copy, paste, Home/End of text, Ctrl+A select all, Ctrl+L clear line, Ctrl+Up/Down scroll,
// Ctrl+Enter trigger; returns whether a key was handled
bool UIEditBox::ProcessControlInput(bool inRepeat)
{
    bool theHandled = false;
    if (UIControl::CutKeyPressed()) {
        theHandled = true;
        if (inRepeat)
            this->Cut();
    }
    if (UIControl::CopyKeyPressed()) {
        theHandled = true;
        if (inRepeat)
            this->Copy();
    }
    if (UIControl::PasteKeyPressed()) {
        theHandled = true;
        if (inRepeat)
            this->Paste();
    }
    if (UIControl::HomeKeyPressed()) {
        this->mIsTextSelected = false;
        theHandled = true;
        this->mCaretLine = 0;
        this->ProcessHomeKey();
    }
    if (UIControl::EndKeyPressed()) {
        this->mIsTextSelected = false;
        theHandled = true;
        this->mCaretLine = this->mLines.size() - 1;
        this->ProcessEndKey();
    }
    ISEntity* theKey;
    gISKeyboard->GetEntity(kKeyA, theKey);
    if (theKey->JustPressed())
        this->SelectText();
    gISKeyboard->GetEntity(kKeyL, theKey);
    if (theKey->JustPressed()) {
        this->mIsTextSelected = false;
        this->mCaretIndex = 0;
        this->mDrawingIndex = 0;
        if (this->mCaretLine == this->mLines.size() - 1) {
            this->mLines[this->mCaretLine] = "";
            this->ComputeCaretLocation(true);
        } else
            this->DeleteLine(this->mCaretLine);
    }
    if (this->mIsSingleLine)
        return theHandled;
    if (UIControl::DownKeyPressed()) {
        this->mIsTextSelected = false;
        theHandled = true;
        if (inRepeat &&
            this->mScrollBar->GetSlider()->GetCurrentRangeValue() < this->mScrollBar->GetSlider()->GetMaxRange())
            this->mScrollBar->SetCurrentRangeValue(this->mScrollBar->GetSlider()->GetCurrentRangeValue() + 1);
    }
    if (UIControl::UpKeyPressed()) {
        this->mIsTextSelected = false;
        theHandled = true;
        if (inRepeat && this->mScrollBar->GetSlider()->GetCurrentRangeValue() > 0)
            this->mScrollBar->SetCurrentRangeValue(this->mScrollBar->GetSlider()->GetCurrentRangeValue() - 1);
    }
    gISKeyboard->GetEntity(kKeyReturn, theKey);
    if (theKey->JustPressed())
        this->FireTriggerEvent(kControlTriggerEnterPressed);
    return theHandled;
}

// FUNCTION: LLE 0x1006d221
// Handles the edit keys (backspace, delete, arrows, Home, End, Enter) and dispatches Shift and Ctrl combinations
// Returns whether a key was handled
bool UIEditBox::ProcessInput(bool inRepeat)
{
    bool theHandled = false;
    if (UIControl::ShiftKeyPressed()) {
        if (!this->mIMEActive)
            theHandled = this->ProcessShiftInput(inRepeat);
    } else if (UIControl::sControlKey->GetValue()) {
        if (!this->mIMEActive)
            theHandled = this->ProcessControlInput(inRepeat);
    } else {
        if (UIControl::sBackSpaceKey->GetValue()) {
            theHandled = true;
            if (!this->mIMEActive && inRepeat) {
                if (this->mIsTextSelected)
                    this->DeleteSelectedText();
                else
                    this->BackSpaceChar();
            }
        }
        if (UIControl::DeleteKeyPressed()) {
            theHandled = true;
            if (!this->mIMEActive && inRepeat) {
                if (this->mIsTextSelected)
                    this->DeleteSelectedText();
                else
                    this->DeleteChar();
            }
        }
        if (UIControl::RightKeyPressed()) {
            theHandled = true;
            if (!this->mIMEActive) {
                this->mIsTextSelected = false;
                if (inRepeat)
                    this->IncrementCaretPos(1);
            }
        }
        if (UIControl::LeftKeyPressed()) {
            theHandled = true;
            if (!this->mIMEActive) {
                this->mIsTextSelected = false;
                if (inRepeat)
                    this->DecrementCaretPos();
            }
        }
        if (UIControl::HomeKeyPressed()) {
            theHandled = true;
            if (!this->mIMEActive) {
                this->mIsTextSelected = false;
                this->ProcessHomeKey();
            }
        }
        if (UIControl::EndKeyPressed()) {
            theHandled = true;
            if (!this->mIMEActive) {
                this->mIsTextSelected = false;
                this->ProcessEndKey();
            }
        }
        if (!this->mIsSingleLine) {
            if (UIControl::UpKeyPressed()) {
                theHandled = true;
                if (!this->mIMEActive) {
                    this->mIsTextSelected = false;
                    if (inRepeat)
                        this->ProcessUpKey();
                }
            } else if (UIControl::DownKeyPressed()) {
                theHandled = true;
                if (!this->mIMEActive) {
                    this->mIsTextSelected = false;
                    if (inRepeat)
                        this->ProcessDownKey();
                }
            }
            if (UIControl::EnterKeyPressed()) {
                theHandled = true;
                if (!this->mIMEActive && inRepeat)
                    this->ProcessEnterKey();
            }
        }
    }
    return theHandled;
}

// FUNCTION: LLE 0x1006d3fb
// Handles Shift keys: cut, copy, paste and caret moves that extend the selection; returns whether a key was handled
bool UIEditBox::ProcessShiftInput(bool inRepeat)
{
    bool theHandled = false;
    if (!this->mIsTextSelected) {
        this->mSelectionStartIndex = this->mCaretIndex;
        this->mSelectionStartLine = this->mCaretLine;
    }
    if (UIControl::CutKeyPressed()) {
        theHandled = true;
        if (inRepeat)
            this->Cut();
    }
    if (UIControl::CopyKeyPressed()) {
        theHandled = true;
        if (inRepeat)
            this->Copy();
    }
    if (UIControl::PasteKeyPressed()) {
        theHandled = true;
        if (inRepeat)
            this->Paste();
    }
    if (theHandled)
        return theHandled;
    if (UIControl::sBackSpaceKey->GetValue()) {
        theHandled = true;
        if (inRepeat) {
            if (this->mIsTextSelected)
                this->DeleteSelectedText();
            else
                this->BackSpaceChar();
        }
    }
    if (UIControl::DeleteKeyPressed()) {
        theHandled = true;
        if (inRepeat) {
            if (this->mIsTextSelected)
                this->DeleteSelectedText();
            else
                this->BackSpaceChar();
        }
    }
    if (UIControl::RightKeyPressed()) {
        theHandled = true;
        if (inRepeat)
            this->IncrementCaretPos(this->mCharScroll);
        this->mIsTextSelected = this->mSelectionStartLine != this->mCaretLine || this->mSelectionStartIndex != this->mCaretIndex;
    }
    if (UIControl::LeftKeyPressed()) {
        theHandled = true;
        if (inRepeat)
            this->DecrementCaretPos();
        this->mIsTextSelected = this->mSelectionStartLine != this->mCaretLine || this->mSelectionStartIndex != this->mCaretIndex;
    }
    if (UIControl::HomeKeyPressed()) {
        theHandled = true;
        this->ProcessHomeKey();
        this->mIsTextSelected = this->mSelectionStartLine != this->mCaretLine || this->mSelectionStartIndex != this->mCaretIndex;
    }
    if (UIControl::EndKeyPressed()) {
        theHandled = true;
        this->ProcessEndKey();
        this->mIsTextSelected = this->mSelectionStartLine != this->mCaretLine || this->mSelectionStartIndex != this->mCaretIndex;
    }
    if (!this->mIsSingleLine) {
        if (UIControl::DownKeyPressed()) {
            theHandled = true;
            if (inRepeat && this->mCaretLine < this->mLines.size() - 1) {
                this->mCaretLine++;
                this->SetIndexFromUpDownOffset();
                this->ComputeCaretLocation(false);
            }
            this->mIsTextSelected = this->mSelectionStartLine != this->mCaretLine || this->mSelectionStartIndex != this->mCaretIndex;
        }
        if (UIControl::UpKeyPressed()) {
            theHandled = true;
            if (inRepeat && this->mCaretLine > 0) {
                this->mCaretLine--;
                this->SetIndexFromUpDownOffset();
                this->ComputeCaretLocation(false);
            }
            this->mIsTextSelected = this->mSelectionStartLine != this->mCaretLine || this->mSelectionStartIndex != this->mCaretIndex;
        }
    }
    return theHandled;
}

// FUNCTION: LLE 0x1006d6c6
// Moves the caret up one line, keeping its x position
void UIEditBox::ProcessUpKey()
{
    if (this->mCaretLine > 0) {
        this->mCaretLine--;
        this->SetIndexFromUpDownOffset();
    }
    this->ComputeCaretLocation(false);
}

// FUNCTION: LLE 0x1006d6ee
// Scrolls a single-line box left so the caret stays visible
void UIEditBox::ScrollLeft()
{
    GE2DPoint theLocation;
    this->mDrawingIndex = this->mCaretIndex;
    for (unsigned long i = this->mCharScroll - 1; i > 0; i--) {
        this->mDrawingIndex--;
        this->GetLocationFromIndex(this->mCaretIndex, this->mCaretLine, theLocation);
        if (this->mDrawingIndex < 0 || this->mTextRectangle.GetRight() == theLocation.GetX()) {
            this->mDrawingIndex++;
            break;
        }
    }
}

// FUNCTION: LLE 0x1006d75d
// Scrolls right to show the given number of extra characters (moves the caret to line 0 while doing so)
void UIEditBox::ScrollRight(long inCount)
{
    long theIndex = this->mCaretIndex;
    this->mCaretIndex += inCount;
    if (this->mCaretIndex > (long)this->mLines[this->mCaretLine].GetLength())
        this->mCaretIndex = this->mLines[this->mCaretLine].GetLength();
    this->SetCaretLocation(this->mCaretIndex, 0);
    this->mCaretIndex = theIndex;
    if (this->mCaretIndex < this->mDrawingIndex)
        this->mCaretIndex = this->mDrawingIndex;
}

// FUNCTION: LLE 0x1006d7be
// Releases the keyboard, cancelling any open IME composition and restoring the saved always-dirty flag
void UIEditBox::ReleaseCapturedKeyboard()
{
    GEWindow* theWindow = this->GetRasterizer()->GetWindow(this->GetScreenIndex());
    if (this->mIMEActive == true) {
        this->RemoveIMECompositionString();
        HIMC theContext = ImmGetContext(theWindow->GetOSRepresentation());
        if (theContext) {
            ImmNotifyIME(theContext, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
            ImmReleaseContext(theWindow->GetOSRepresentation(), theContext);
        }
        this->mIMEActive = false;
    }
    this->SetAlwaysDirty(this->mSavedAlwaysDirty);
    UIControl::ReleaseCapturedKeyboard();
}

// FUNCTION: LLE 0x1006d82e
// Deletes the uncommitted IME composition text, leaving the caret where it started
void UIEditBox::RemoveIMECompositionString()
{
    this->SetCaretLocation(this->mIMEStartIndex, this->mIMEStartLine);
    while (this->mIMECompositionLength > 0) {
        this->DeleteChar();
        this->mIMECompositionLength--;
    }
}

// FUNCTION: LLE 0x1006d860
// Selects all the text (not while an IME composition is open)
void UIEditBox::SelectText()
{
    if (!this->mIMEActive && this->mLines.size() > 0)
        this->SelectText(0, this->mLines[this->mLines.size() - 1].GetLength(), 0, this->mLines.size() - 1);
}

// FUNCTION: LLE 0x1006d8dc
// Sets the caret and the selection anchor, selecting the text between them (not while an IME composition is open)
void UIEditBox::SelectText(unsigned long inCaretIndex, unsigned long inStartIndex, unsigned long inCaretLine,
                           unsigned long inStartLine)
{
    if (this->mIMEActive)
        return;
    this->mCaretIndex = inCaretIndex;
    this->mCaretLine = inCaretLine;
    this->mSelectionStartIndex = inStartIndex;
    this->mSelectionStartLine = inStartLine;
    if (this->mIsSingleLine)
        this->ComputeDrawingIndex();
    this->ComputeCaretLocation(true);
    this->mIsTextSelected = this->mSelectionStartLine != this->mCaretLine || this->mSelectionStartIndex != this->mCaretIndex;
    this->SetDirty(true);
}

// FUNCTION: LLE 0x1006d95d
// Moves the caret to a character index on a line, scrolling to show it
void UIEditBox::SetCaretLocation(unsigned long inIndex, unsigned long inLine)
{
    this->mCaretLine = inLine;
    this->mCaretIndex = inIndex;
    this->ComputeDrawingIndex();
    this->ComputeCaretLocation(true);
    this->SetDirty(true);
}

// FUNCTION: LLE 0x1006d98f
// Places the caret on its new line at the character nearest its remembered x position (after an up/down move)
void UIEditBox::SetIndexFromUpDownOffset()
{
    EE_LOST_DEBUG(3);  // LOST-DEBUG (PROVISIONAL, owner experiment 2026-10-24): no independent evidence; base-game LLE identical (exact window 3..4)
    UWideString theChar;
    const char* theCursor = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)this->mLines[this->mCaretLine]),
                                                  this->mDrawingIndex);
    float theWidth = 0.0f;
    long theLength = this->mLines[this->mCaretLine].GetLength();
    if (_mbschr(reinterpret_cast<const unsigned char*>(theCursor), '\n'))
        theLength--;
    for (this->mCaretIndex = this->mDrawingIndex; this->mCaretIndex < theLength; this->mCaretIndex++) {
        theChar = "";
        theChar.CopyString(theCursor, 1);
        this->GetGEText()->SetText(theChar);
        theWidth += this->GetGEText()->GetTextExtent().GetWidth();
        if (theWidth > this->mCaretX)
            break;
        theCursor = (const char*)_mbsinc(reinterpret_cast<const unsigned char*>(theCursor));
    }
    this->SetDirty(true);
}

// FUNCTION: LLE 0x1006da92
// Moves the edit box, keeping the text area's size and moving the scroll bar with it
void UIEditBox::SetLocation(const GE2DPoint& inLocation)
{
    unsigned long theWidth = UMFloor(this->mTextRectangle.GetWidth());
    unsigned long theHeight = UMFloor(this->mTextRectangle.GetHeight());
    UIControl::SetLocation(inLocation);
    this->mTextRectangle.SetTop(this->GetRectangle().GetTop() + this->GetBorderThickness());
    this->mTextRectangle.SetLeft(this->GetRectangle().GetLeft() + this->GetBorderThickness());
    this->mTextRectangle.SetRight(this->mTextRectangle.GetLeft() + theWidth);
    this->mTextRectangle.SetBottom(this->mTextRectangle.GetTop() + theHeight);
    if (this->mScrollBar != NULL)
        this->mScrollBar->SetLocation(this->mTextRectangle.GetTopRight());
}

// FUNCTION: LLE 0x1006db7b
// Replaces the whole text with a string and puts the caret at the start
void UIEditBox::SetText(const UWideString& inString)
{
    this->DeleteText();
    this->AddChar(inString);
    this->mCaretLine = 0;
    this->SetCaretLocation(0, 0);
    this->SetDirty(true);
}

// FUNCTION: LLE 0x1006dbaf
// Captures the keyboard with the caret shown and the box redrawn every frame
void UIEditBox::SetupKeyboardForCapture()
{
    this->mCursorOn = true;
    this->CaptureKeyboard();
    this->mSavedAlwaysDirty = this->GetAlwaysDirty();
    this->SetAlwaysDirty(true);
}

// FUNCTION: LLE 0x1006dbd5
// Takes focus away: stops the caret, restores the background, clears the selection and cancels any IME composition
void UIEditBox::TabbedFrom()
{
    GEWindow* theWindow = this->GetRasterizer()->GetWindow(this->GetScreenIndex());
    this->mCursorPoll->Stop();
    this->mCursorOn = false;
    this->SetBgOpacity(this->mSavedBgOpacity);
    this->mIsTextSelected = false;
    this->mSelecting = false;
    this->mScrollBarCaptured = false;
    this->ReleaseCapturedKeyboard();
    this->ReleaseCapturedMouse();
    HIMC theContext = ImmGetContext(theWindow->GetOSRepresentation());
    if (theContext) {
        ImmNotifyIME(theContext, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
        ImmReleaseContext(theWindow->GetOSRepresentation(), theContext);
    }
    this->SetDirty(true);
}

// FUNCTION: LLE 0x1006dc67
// Gives the box focus: starts the caret blink, makes the background opaque, captures the keyboard, cancels IME input
void UIEditBox::TabbedTo()
{
    GEWindow* theWindow = this->GetRasterizer()->GetWindow(this->GetScreenIndex());
    this->mCursorPoll->Start();
    this->mSavedBgOpacity = this->GetBgOpacity();
    this->SetBgOpacity(1.0f);
    this->SetupKeyboardForCapture();
    HIMC theContext = ImmGetContext(theWindow->GetOSRepresentation());
    if (theContext) {
        ImmNotifyIME(theContext, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
        ImmReleaseContext(theWindow->GetOSRepresentation(), theContext);
    }
    this->SetDirty(true);
}

// FUNCTION: LLE 0x1006dcfb
// Processes held keys with a 500 ms repeat delay and 50 ms repeat; Enter in a single-line box fires its trigger
long UIEditBox::UpdateKeyboard()
{
    bool theRepeat = false;
    if (!this->mIMEActive && this->mIsSingleLine && (UIControl::sEnterKey->JustPressed() || UIControl::sNumEnterKey->JustPressed())) {
        this->ProcessEnterKey();
        this->mKeyRepeating = false;
        this->mRepeatDelayPoll->Stop();
        this->mRepeatPoll->Stop();
        this->mCursorPoll->Start();
    } else {
        if (this->mRepeatDelayPoll->GetIsRunning()) {
            if (this->mRepeatDelayPoll->Poll()) {
                this->mRepeatDelayPoll->Stop();
                this->mRepeatPoll->Start();
                theRepeat = true;
            }
        } else if (!this->mRepeatPoll->GetIsRunning() || this->mRepeatPoll->Poll())
            theRepeat = true;
        if (this->ProcessInput(theRepeat)) {
            if (!this->mKeyRepeating)
                this->mRepeatDelayPoll->Start();
            this->mKeyRepeating = true;
            if (!this->mCursorOn)
                this->SetDirty(true);
            this->mCursorPoll->Stop();
            this->mCursorOn = true;
        } else {
            this->mKeyRepeating = false;
            this->mRepeatDelayPoll->Stop();
            this->mRepeatPoll->Stop();
            this->mCursorPoll->Start();
        }
    }
    return kErrorOK;
}

// FUNCTION: LLE 0x1006ddff
// Types a translated key the given number of times at the caret (or into the IME composition) and shows the caret
long UIEditBox::UpdateTranslatedKey(const char* inKey, short inRepeat)
{
    for (short i = inRepeat; i-- > 0;) {
        if (!this->mIMEActive) {
            this->AddChar(inKey);
            this->IncrementCaretPos(1);
        } else {
            this->SetCaretLocation(this->mIMEStartIndex, this->mIMEStartLine);
            this->AddChar(inKey);
            this->IncrementCaretPos(1);
            this->mIMEStartIndex = this->mCaretIndex;
            this->mIMEStartLine = this->mCaretLine;
        }
    }
    this->mCursorPoll->Stop();
    this->mCursorPoll->Start();
    this->mCursorOn = true;
    return kErrorOK;
}

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x10069d6b
// SYNTHETIC: LLE 0x10069d75
// SYNTHETIC: LLE 0x10069d85
// SYNTHETIC: LLE 0x10069d91
// SYNTHETIC: LLE 0x10069d9b
// SYNTHETIC: LLE 0x10069da5
// SYNTHETIC: LLE 0x10069db5
// SYNTHETIC: LLE 0x10069dc1
// SYNTHETIC: LLE 0x10069dcb
// SYNTHETIC: LLE 0x10069dd5
// SYNTHETIC: LLE 0x10069de5
// SYNTHETIC: LLE 0x10069df1
// SYNTHETIC: LLE 0x10069dfb
// SYNTHETIC: LLE 0x10069e05
// SYNTHETIC: LLE 0x10069e15
// SYNTHETIC: LLE 0x10069e21
// SYNTHETIC: LLE 0x10069e2b
// SYNTHETIC: LLE 0x10069e35
// SYNTHETIC: LLE 0x10069e45
// SYNTHETIC: LLE 0x10069e51
// SYNTHETIC: LLE 0x10069e5b
// SYNTHETIC: LLE 0x10069e65
// SYNTHETIC: LLE 0x10069e75
// SYNTHETIC: LLE 0x10069e81
