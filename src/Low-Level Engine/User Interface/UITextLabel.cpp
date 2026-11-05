// C:\EEX\Low-Level Engine\User Interface\UITextLabel.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007e579..0x1007fb46: the ctor, then the out-of-line members in alphabetical order, then the vector COMDATs it
// is the first to emit. No U2D key init chains (the unit does not include the rasterizer header). The
// header-inline members are emitted first by UICheckBox.cpp (0x1005f538..).
#include <windows.h>
#include <mbstring.h>
#include <stdlib.h>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/TSCriticalSection.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UITextLabel.h"

// The margin a fitted label keeps around its text (0x10099b54, 6.0f; a named const: CopyData multiplies it by 0.5f
// at run time) (provisional name).
const float kTextLabelMargin = 6.0f;

// FUNCTION: LLE 0x1007e579
// Creates a text label (type 0 one line, type 1 multi-line with a scroll bar area) and sets its initial text
UITextLabel::UITextLabel(const UWideString& inText, GEFont* inFont, unsigned long inId, const GERectangle& inRectangle,
                         UIForm* inForm, unsigned long inLabelType, unsigned long inIndex,
                         unsigned long inBorderThickness)
    : UIControl(kControlTypeTextLabel, inId, inRectangle, inForm, inIndex, inBorderThickness)
{
    this->mRecomputePending = false;
    this->mLabelType = inLabelType;
    this->mScrollBar = NULL;
    this->mScrollBarBgColor = GEColor(0.0f, 0.0f, 0.0f);
    this->mAlignment = kTextAlignLeft;
    this->mScrollBarBgOpacity = 0.0f;
    this->mLastDisplayLine = 0;
    this->mBorderType = 0;
    this->mFitRectToText = false;
    this->mScrollBarRemoved = false;
    this->mDrawScrollBar = false;
    this->mOriginalRectangle = this->mRectangle;
    this->mTextRectangle.SetLeft(this->mRectangle.GetLeft() + inBorderThickness + 1.0f);
    this->mTextRectangle.SetTop(this->mRectangle.GetTop() + inBorderThickness + 1.0f);
    this->mTextRectangle.SetBottom(this->mRectangle.GetBottom() - inBorderThickness - 1.0f);
    this->mTextRectangle.SetRight(this->mRectangle.GetRight() - inBorderThickness - 1.0f);
    this->mText = new GEText(inFont);
    this->mCopiedText = new GEText(inFont);
    this->mNumberLinesDisplayed = UMFloor((inRectangle.GetHeight() - 2 * this->mBorderThickness) / this->mText->GetFont()->GetHeight());
    if (this->mNumberLinesDisplayed == 0)
        this->mNumberLinesDisplayed = 1;
    if (this->mLabelType == 1) {
        this->mTextRectangle.SetRight(this->mRectangle.GetRight() - kScrollBarWidth - 1.0f);
        this->mScrollBarRectangle = GERectangle(this->mRectangle.GetRight() - kScrollBarWidth + 1.0f, this->mRectangle.GetTop(),
                                          this->mRectangle.GetRight(), this->mRectangle.GetBottom());
        this->mDisplayLines = new UWideString[this->mNumberLinesDisplayed];
    } else
        this->mDisplayLines = new UWideString[1];
    this->SetText(inText, true);
}

// FUNCTION: LLE 0x1007e8c6
// Destroys the label, deleting every stored line, the scroll bar and both text objects
UITextLabel::~UITextLabel()
{
    for (unsigned long i = 0; i < this->mLines.size(); i++)
        delete this->mLines[i];
    this->mLines.clear();
    delete this->mScrollBar;
    delete this->mText;
    delete this->mCopiedText;
    delete[] this->mDisplayLines;
}

// FUNCTION: LLE 0x1007e984
// Appends text to the label's lines without clearing the old ones
void UITextLabel::AppendText(const UWideString& inText)
{
    this->SetText(inText, false);
}

// FUNCTION: LLE 0x1007e992
// Switches both text objects to a new font and refits the label rectangle (nothing if there is no text yet)
void UITextLabel::ChangeFont(GEFont* inFont)
{
    if (!this->mText)
        return;
    this->mCopiedText->SetFont(inFont);
    this->mText->SetFont(inFont);
    this->ComputeTextLabelRect();
}

// FUNCTION: LLE 0x1007e9cb
// Creates, updates or marks for removal the scroll bar depending on whether the lines overflow the visible rows
void UITextLabel::CheckScrollBar()
{
    if (this->mScrollBar != NULL && !this->mScrollBarRemoved) {
        if (this->mLines.size() <= this->mNumberLinesDisplayed) {
            this->mScrollBarRemoved = true;
            this->SetDirty(true);
            this->SetBgRectangle(this->mRectangle);
        } else
            this->mScrollBar->SetRange(0, this->mLines.size() - this->mNumberLinesDisplayed);
    } else if (this->mLines.size() > this->mNumberLinesDisplayed) {
        if (this->mScrollBar == NULL) {
            this->mScrollBar = new UIScrollBar(this->mId, this->mScrollBarRectangle.GetTopLeft(),
                                         this->mScrollBarRectangle.GetBottom() + 1.0f - this->mScrollBarRectangle.GetTop(), 1,
                                         this->mForm, this->mIndex, this->GetBorderThickness());
            this->mScrollBar->SetRange(0, this->mLines.size() - this->mNumberLinesDisplayed);
            this->mScrollBar->SetPageSize(this->mNumberLinesDisplayed > 1 ? this->mNumberLinesDisplayed - 1 : this->mNumberLinesDisplayed);
            this->mScrollBar->SetBgColor(this->mScrollBarBgColor);
            this->mScrollBar->SetBgOpacity(this->mScrollBarBgOpacity);
            this->SetBgRectangle(GERectangle(this->mRectangle.GetLeft(), this->mRectangle.GetTop(),
                                       this->mScrollBar->GetRectangle().GetLeft() - 1.0f, this->mRectangle.GetBottom()));
        } else
            this->mScrollBar->SetRange(0, this->mLines.size() - this->mNumberLinesDisplayed);
        this->mScrollBarRemoved = false;
    }
    this->ComputeTextLabelRect();
}

// FUNCTION: LLE 0x1007ebf2
// With fit-to-text on, shrinks the label rectangle around its (visible) lines and marks it dirty when it changed
void UITextLabel::ComputeTextLabelRect()
{
    GERectangle theRectangle = this->mOriginalRectangle;
    if (!this->mFitRectToText)
        return;
    float theWidth;
    float theHeight;
    if (this->mLabelType == 0 && this->mLines.size() > 0) {
        this->mText->SetText(*this->mLines[0]);
        theWidth = this->mText->GetTextExtent().GetWidth();
        theHeight = this->mText->GetTextExtent().GetHeight();
        if (this->mAlignment == kTextAlignCenter)
            theRectangle.SetLeft(theRectangle.GetWidth() * 0.5f + theRectangle.GetLeft() - theWidth * 0.5f);
        else if (this->mAlignment == kTextAlignRight)
            theRectangle.SetLeft(theRectangle.GetRight() - theWidth);
        theRectangle.SetTop(theRectangle.GetHeight() * 0.5f + theRectangle.GetTop() - theHeight * 0.5f);
    } else {
        theHeight = 0.0f;
        theWidth = 0.0f;
        unsigned long i;
        unsigned long theEnd;
        if (this->mScrollBar != NULL && !this->mScrollBarRemoved) {
            i = this->mScrollBar->GetSlider()->GetCurrentRangeValue();
            theEnd = this->mNumberLinesDisplayed + i;
        } else {
            i = 0;
            theEnd = this->mLines.size();
        }
        for (; i < theEnd; i++) {
            this->mText->SetText(*this->mLines[i]);
            theHeight += this->mText->GetTextExtent().GetHeight();
            if (this->mText->GetTextExtent().GetWidth() > theWidth)
                theWidth = this->mText->GetTextExtent().GetWidth();
        }
    }
    theRectangle.SetRight(theRectangle.GetLeft() + kTextLabelMargin + theWidth);
    theRectangle.SetBottom(theRectangle.GetTop() + theHeight);
    theRectangle.RoundToNearest();
    if (theRectangle != this->mRectangle) {
        this->SetRectangle(theRectangle);
        this->SetDirty(true);
    }
}

// FUNCTION: LLE 0x1007edf9
// Prepares the draw copy: places a one-line label's text by alignment, or copies the visible lines of a multi-line
long UITextLabel::CopyData()
{
    *this->mCopiedText = *this->mText;
    this->mCopiedTextRectangle = this->mTextRectangle;
    if (this->mLabelType == 0) {
        if (this->mLines.size() > 0)
            this->mCopiedText->SetText(*this->mLines[0]);
        else
            this->mCopiedText->SetText("");
        float theX;
        if (this->mAlignment == kTextAlignLeft) {
            theX = this->mTextRectangle.GetLeft();
            if (this->mFitRectToText)
                theX += kTextLabelMargin * 0.5f;
        } else if (this->mAlignment == kTextAlignCenter)
            theX = this->mTextRectangle.GetWidth() * 0.5f + this->mTextRectangle.GetLeft() -
                   this->mCopiedText->GetTextExtent().GetWidth() * 0.5f;
        else {
            theX = this->mTextRectangle.GetRight() - this->mCopiedText->GetTextExtent().GetWidth();
            if (this->mFitRectToText)
                theX -= kTextLabelMargin * 0.5f;
        }
        this->mCopiedText->SetLocation(GE2DPoint(theX, this->mTextRectangle.GetHeight() * 0.5f + this->mTextRectangle.GetTop() -
                                                     this->mCopiedText->GetTextExtent().GetHeight() * 0.5f));
    } else {
        if (this->mScrollBarRemoved) {
            delete this->mScrollBar;
            this->mScrollBarRemoved = false;
            this->mScrollBar = NULL;
        }
        if (this->mRecomputePending)
            this->RecomputeDisplayRange();
        unsigned long i;
        unsigned long theEnd;
        if (this->mScrollBar != NULL) {
            this->mDrawScrollBar = true;
            i = this->mScrollBar->GetSlider()->GetCurrentRangeValue();
            theEnd = this->mNumberLinesDisplayed + i;
            this->mLastDisplayLine = this->mNumberLinesDisplayed - 1;
            this->mScrollBar->CopyData();
        } else {
            i = 0;
            theEnd = this->mLines.size();
            this->mLastDisplayLine = this->mLines.size() - 1;
            this->mDrawScrollBar = false;
        }
        for (unsigned long j = 0; i < theEnd; i++, j++)
            this->mDisplayLines[j] = *this->mLines[i];
    }
    return UIControl::CopyData();
}

// FUNCTION: LLE 0x1007f064
// Deletes every stored line, marks the label dirty and updates the scroll bar
void UITextLabel::DeleteArray()
{
    for (unsigned long i = 0; i < this->mLines.size(); i++)
        delete this->mLines[i];
    this->mLines.clear();
    this->SetDirty(true);
    this->CheckScrollBar();
}

// FUNCTION: LLE 0x1007f0b8
// Deletes one stored line and updates the scroll bar
void UITextLabel::DeleteLine(unsigned long inLine)
{
    delete this->mLines[inLine];
    this->mLines.erase(this->mLines.begin() + inLine);
    this->CheckScrollBar();
}

// FUNCTION: LLE 0x1007f118
// Draws the label's polygons, then the scroll bar's when it is shown; returns the last result
long UITextLabel::DrawPolygons()
{
    long theResult = UIControl::DrawPolygons();
    if (this->mDrawScrollBar)
        theResult = this->mScrollBar->DrawPolygons();
    return theResult;
}

// FUNCTION: LLE 0x1007f136
// Draws the label text clipped to the form: one line, or each visible line placed by alignment; then the scroll bar
long UITextLabel::DrawTextA()
{
    GERectangle theRectangle;
    long theResult = kErrorOK;
    long i = 0;
    unsigned long theLineHeight = this->mCopiedText->GetFont()->GetHeight();
    this->mDrawCount--;
    if (this->mLabelType == 0)
        theResult = this->mForm->DrawClippedText(this->mCopiedDrawRectangle, this->mCopiedText, this);
    else {
        float theHeight = theLineHeight;
        unsigned long theOffset = 0;
        for (; i <= this->mLastDisplayLine; i++, theOffset += theLineHeight) {
            this->mCopiedText->SetText(this->mDisplayLines[i]);
            float theX;
            if (this->mAlignment == kTextAlignLeft) {
                theX = this->mCopiedTextRectangle.GetLeft();
                if (this->mFitRectToText)
                    theX += kTextLabelMargin * 0.5f;
            } else if (this->mAlignment == kTextAlignCenter) {
                theX = this->mRectangle.GetWidth() * 0.5f + this->mCopiedTextRectangle.GetLeft() -
                       this->mCopiedText->GetTextExtent().GetWidth() * 0.5f;
                if (this->mScrollBar)
                    theX -= this->mScrollBar->GetCopiedDrawRectangle().GetWidth() * 0.5f;
            } else {
                theX = this->mRectangle.GetRight() - this->mCopiedText->GetTextExtent().GetWidth();
                if (this->mFitRectToText)
                    theX -= kTextLabelMargin * 0.5f;
            }
            this->mCopiedText->SetLocation(theX, theOffset + this->mCopiedTextRectangle.GetTop());
            const GE2DPoint& theLocation = this->mCopiedText->GetLocation();
            theRectangle = GERectangle(theLocation, GE2DPoint(this->mCopiedTextRectangle.GetRight(), theLocation.GetY() + theHeight));
            theResult = this->mForm->DrawClippedText(theRectangle, this->mCopiedText, this);
        }
    }
    if (this->mDrawScrollBar)
        theResult = this->mScrollBar->DrawTextA();
    return theResult;
}

// FUNCTION: LLE 0x1007f2ef
// Forwards a left click inside the scroll bar area to the scroll bar, captures the mouse, refits on a scroll
long UITextLabel::MouseLButtonDown()
{
    long theResult = kErrorOK;
    if (this->mScrollBar != NULL && this->mScrollBarRectangle.Contains(this->GetMouseLocation())) {
        long theOldValue = this->mScrollBar->GetSlider()->GetCurrentRangeValue();
        theResult = this->mScrollBar->MouseLButtonDown();
        this->CaptureMouse();
        if (theOldValue != this->mScrollBar->GetSlider()->GetCurrentRangeValue())
            this->ComputeTextLabelRect();
    }
    return theResult;
}

// FUNCTION: LLE 0x1007f354
// Forwards a left button release to the scroll bar, releases the mouse capture and refits on a scroll
long UITextLabel::MouseLButtonUp()
{
    long theResult = kErrorOK;
    if (this->mScrollBar != NULL) {
        long theOldValue = this->mScrollBar->GetSlider()->GetCurrentRangeValue();
        theResult = this->mScrollBar->MouseLButtonUp();
        this->ReleaseCapturedMouse();
        if (theOldValue != this->mScrollBar->GetSlider()->GetCurrentRangeValue())
            this->ComputeTextLabelRect();
    }
    return theResult;
}

// FUNCTION: LLE 0x1007f3a0
// Forwards a mouse move to the scroll bar and refits the label when the scroll position changed
long UITextLabel::MouseMove(const GE2DPoint& inLocation)
{
    long theResult = kErrorOK;
    if (this->mScrollBar != NULL) {
        long theOldValue = this->mScrollBar->GetSlider()->GetCurrentRangeValue();
        theResult = this->mScrollBar->MouseMove(inLocation);
        if (theOldValue != this->mScrollBar->GetSlider()->GetCurrentRangeValue())
            this->ComputeTextLabelRect();
    }
    return theResult;
}

// FUNCTION: LLE 0x1007f3eb
// Scrolls by the wheel delta; fails when the label has no scroll bar
long UITextLabel::MouseWheelTurned(long inDelta)
{
    long theResult = kErrorFailed;
    if (this->mScrollBar) {
        this->mScrollBar->IncrementBy(inDelta);
        theResult = kErrorOK;
    }
    return theResult;
}

// FUNCTION: LLE 0x1007f406
// Returns how many characters of the text fit on one line by whole words (cuts an over-wide first word by width)
unsigned long UITextLabel::ReadLine(const char* inText)
{
    unsigned long theWidth = 0;
    unsigned long theLength = 0;
    char theWord[512];
    while (PlatMbsCmp(reinterpret_cast<const unsigned char*>(inText), (const unsigned char*)"") != 0) {
        long theWordLength = this->ReadWord(inText, theWord);
        this->mText->SetText(UWideString(theWord));
        theWidth += UMFloor(this->mText->GetTextExtent().GetWidth());
        if (theWidth > this->mTextRectangle.GetWidth()) {
            if (theLength == 0) {
                while (theWordLength > 0) {
                    theWordLength--;
                    strcpy((char*)PlatMbsNInc(reinterpret_cast<unsigned char*>(theWord), theWordLength), "");
                    this->mText->SetText(UWideString(theWord));
                    if ((unsigned long)UMFloor(this->mText->GetTextExtent().GetWidth()) <
                        UMFloorUnsigned(this->mTextRectangle.GetWidth())) {
                        theLength = theWordLength;
                        break;
                    }
                }
            }
            break;
        }
        theLength += theWordLength;
        inText = (const char*)PlatMbsNInc(reinterpret_cast<const unsigned char*>(inText), theWordLength);
        if (PlatMbsNBCmp(PlatMbsNInc(reinterpret_cast<unsigned char*>(theWord), theWordLength - 1), (const unsigned char*)"\n", 1) == 0)
            break;
    }
    return theLength;
}

// FUNCTION: LLE 0x1007f5f8
// Copies the next word (up to and including a space, tab or newline; at most 256 characters); returns its length
unsigned long UITextLabel::ReadWord(const char* inText, char* outWord)
{
    unsigned long theCount = 0;
    while (PlatMbsCmp(reinterpret_cast<const unsigned char*>(inText), (const unsigned char*)"") != 0 && theCount++ < 256) {
        PlatMbsNCpy(reinterpret_cast<unsigned char*>(outWord), reinterpret_cast<const unsigned char*>(inText), 1);
        if (PlatMbsNBCmp(reinterpret_cast<const unsigned char*>(outWord), (const unsigned char*)"\n", 1) == 0 ||
            PlatIsMbcSpace(GetMultiByteChar(outWord)) ||
            PlatMbsNBCmp(reinterpret_cast<const unsigned char*>(outWord), (const unsigned char*)"\t", 1) == 0) {
            outWord = (char*)PlatMbsInc(reinterpret_cast<unsigned char*>(outWord));
            break;
        }
        outWord = (char*)PlatMbsInc(reinterpret_cast<unsigned char*>(outWord));
        inText = (const char*)PlatMbsInc(reinterpret_cast<const unsigned char*>(inText));
    }
    strcpy(outWord, "");
    return theCount;
}

// FUNCTION: LLE 0x1007f6b9
// Recomputes the visible row count from the rectangle, re-wraps all lines and rebuilds the scroll bar
void UITextLabel::RecomputeDisplayRange()
{
    UWideString theText;
    this->mNumberLinesDisplayed = UMFloor((this->mRectangle.GetHeight() - 2 * this->mBorderThickness) / this->mText->GetFont()->GetHeight());
    if (this->mNumberLinesDisplayed == 0)
        this->mNumberLinesDisplayed = 1;
    delete[] this->mDisplayLines;
    this->mDisplayLines = new UWideString[this->mNumberLinesDisplayed];
    theText = "";
    for (unsigned long i = 0; i < this->GetLineCount(); i++)
        theText += this->GetText(i) + "\n";
    this->SetText(theText, true);
    this->mScrollBarRemoved = false;
    this->mRecomputePending = false;
    this->SetLocation(this->mRectangle.GetTopLeft());
    this->CheckScrollBar();
    *this->mCopiedText = *this->mText;
    this->mCopiedTextRectangle = this->mTextRectangle;
}

// FUNCTION: LLE 0x1007f84a
// Turns fit-to-text on or off and refits the label rectangle
void UITextLabel::SetFitRectToText(bool inFit)
{
    this->mFitRectToText = inFit;
    this->ComputeTextLabelRect();
}

// FUNCTION: LLE 0x1007f85c
// Moves the label, recomputing the text and scroll bar rectangles and moving the scroll bar
void UITextLabel::SetLocation(const GE2DPoint& inLocation)
{
    UIControl::SetLocation(inLocation);
    this->mScrollBarRectangle = GERectangle(this->mRectangle.GetRight() - kScrollBarWidth + 1.0f, this->mRectangle.GetTop(),
                                      this->mRectangle.GetRight(), this->mRectangle.GetBottom());
    this->mTextRectangle.SetLeft(this->mRectangle.GetLeft() + this->GetBorderThickness() + 1.0f);
    this->mTextRectangle.SetTop(this->mRectangle.GetTop() + this->GetBorderThickness() + 1.0f);
    this->mTextRectangle.SetBottom(this->mRectangle.GetBottom() - this->GetBorderThickness() - 1.0f);
    this->mTextRectangle.SetRight(this->mRectangle.GetRight() - this->GetBorderThickness() - 1.0f);
    if (this->mScrollBar != NULL) {
        this->mTextRectangle.SetRight(this->mTextRectangle.GetRight() - this->mScrollBarRectangle.GetWidth() - 1.0f);
        this->mScrollBar->SetLocation(this->mScrollBarRectangle.GetTopLeft());
    }
}

// FUNCTION: LLE 0x1007f960
// Resizes the label; without fit-to-text schedules a re-wrap and scroll bar rebuild for the next draw copy
void UITextLabel::SetRectangle(const GERectangle& inRectangle)
{
    UIControl::SetRectangle(inRectangle);
    if (!this->mFitRectToText) {
        this->mScrollBarRemoved = true;
        this->mRecomputePending = true;
    } else
        this->mRecomputePending = false;
}

// FUNCTION: LLE 0x1007f990
// Sets or appends text: one line as is, or word-wrapped into lines for a multi-line label; marks it dirty
void UITextLabel::SetText(const UWideString& inText, bool inReplace)
{
    const char* theText = inText;
    if (inReplace)
        this->DeleteArray();
    if (this->mFitRectToText) {
        this->SetRectangle(this->mOriginalRectangle);
        this->mForm->RedrawIntersectControls(this->mOriginalRectangle);
    }
    if (this->mLabelType == 0)
        this->mLines.push_back(new UWideString(inText));
    else {
        while (PlatMbsCmp(reinterpret_cast<const unsigned char*>(theText), (const unsigned char*)"") != 0) {
            unsigned long theLength = this->ReadLine(theText);
            if (theLength == 0)
                break;
            char* theLine = new char[theLength * 2 + 1];
            PlatMbsNCpy(reinterpret_cast<unsigned char*>(theLine), reinterpret_cast<const unsigned char*>(theText), theLength);
            char* theEnd = (char*)PlatMbsNInc(reinterpret_cast<unsigned char*>(theLine), theLength - 1);
            if (PlatMbsNBCmp(reinterpret_cast<const unsigned char*>(theEnd), (const unsigned char*)"\n", 1) != 0)
                theEnd = (char*)PlatMbsInc(reinterpret_cast<unsigned char*>(theEnd));
            strcpy(theEnd, "");
            this->mLines.push_back(new UWideString(theLine));
            delete theLine;
            theText = (const char*)PlatMbsNInc(reinterpret_cast<const unsigned char*>(theText), theLength);
        }
    }
    this->SetDirty(true);
    this->CheckScrollBar();
}

// vector<UWideString*> COMDATs first emitted here (push_back, _Ptrit::operator-, copy)
// TEMPLATE: LLE 0x1007fae6
// TEMPLATE: LLE 0x1007fb12
// TEMPLATE: LLE 0x1007fb26

// Compiler-generated: UWideString's vector deleting dtor, first emitted after the ctor (new[]/delete[] of UWideString)
// SYNTHETIC: LLE 0x1007e86c
