// C:\EEX\Low-Level Engine\User Interface\UITextLabel.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007e579..0x1007fb46: the ctor, then the out-of-line members in alphabetical order, then the vector COMDATs it
// is the first to emit. No U2D key init chains (the unit does not include the rasterizer header). The
// header-inline members are emitted first by UICheckBox.cpp (0x1005f538..).
//
// A multi-line label's word wrapping (ReadWord 0x1007f5f8, ReadLine 0x1007f406 and SetText's line loop) is Rust
// (rust/crates/ee_ui/src/wrap.rs, docs/RUST_PLAN.md R6); SetText forwards to it, and ee_uicpp_label_measure/_add_line
// are its way back to the text object and the new lines. The label's geometry (ComputeTextLabelRect's fit,
// CheckScrollBar's decisions, SetLocation's rectangles) is Rust too (label_fit.rs); UITextLabelRust below answers its
// questions and makes its calls (the scroll bar's construction and calls, the virtual SetRectangle and SetDirty).
#include <windows.h>
#include <mbstring.h>
#include <stdlib.h>
#include <string.h>

#include "rust/ee_ui.h"
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

static EeUiRect UITextLabelRect(const GERectangle& inRectangle)
{
    EeUiRect theRectangle = {inRectangle.mBottom, inRectangle.mLeft, inRectangle.mRight, inRectangle.mTop};
    return theRectangle;
}

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

// The Rust geometry's view of a label (ee_ui label_fit.rs)
struct UITextLabelRust {
    static UITextLabel* L(void* inLabel) { return static_cast<UITextLabel*>(inLabel); }
    static unsigned long Value(void* inLabel, int inWhich)
    {
        UITextLabel* l = L(inLabel);
        switch (inWhich) {
        case 0:
            return l->mFitRectToText;
        case 1:
            return l->mLabelType;
        case 2:
            return l->mAlignment;
        case 3:
            return (unsigned long)l->mLines.size();
        case 4:
            return l->mNumberLinesDisplayed;
        case 5:
            return l->mScrollBar != NULL;
        case 6:
            return l->mScrollBarRemoved;
        default:
            return (unsigned long)l->mScrollBar->GetSlider()->GetCurrentRangeValue();
        }
    }
    static EeUiRect Rect(void* inLabel, int inWhich)
    {
        UITextLabel* l = L(inLabel);
        switch (inWhich) {
        case 0:
            return UITextLabelRect(l->mOriginalRectangle);
        case 1:
            return UITextLabelRect(l->mRectangle);
        case 2:
            return UITextLabelRect(l->mScrollBarRectangle);
        default:
            return UITextLabelRect(l->mScrollBar->GetRectangle());
        }
    }
    static void Measure(void* inLabel, unsigned long inLine, float* outWidth, float* outHeight)
    {
        UITextLabel* l = L(inLabel);
        l->mText->SetText(*l->mLines[inLine]);
        *outWidth = l->mText->GetTextExtent().GetWidth();
        *outHeight = l->mText->GetTextExtent().GetHeight();
    }
    static void Call(void* inLabel, int inCall, unsigned long inValue, const EeUiRect& inRectangle, float inLength)
    {
        UITextLabel* l = L(inLabel);
        GERectangle theRectangle(inRectangle.left, inRectangle.top, inRectangle.right, inRectangle.bottom);
        switch (inCall) {
        case 0:
            l->mScrollBarRemoved = inValue != 0;
            break;
        case 1:
            l->SetRectangle(theRectangle);
            break;
        case 2:
            l->SetDirty(true);
            break;
        case 3:
            l->SetBgRectangle(theRectangle);
            break;
        case 4:
            l->mScrollBar->SetRange(0, inValue);
            break;
        case 5:
            l->mScrollBar = new UIScrollBar(l->mId, l->mScrollBarRectangle.GetTopLeft(), inLength, 1, l->mForm, l->mIndex,
                                            l->GetBorderThickness());
            break;
        case 6:
            l->mScrollBar->SetPageSize(inValue);
            break;
        default:
            l->mScrollBar->SetBgColor(l->mScrollBarBgColor);
            l->mScrollBar->SetBgOpacity(l->mScrollBarBgOpacity);
            break;
        }
    }
};
extern "C" unsigned long ee_uicpp_label_fit_value(void* inLabel, int inWhich) { return UITextLabelRust::Value(inLabel, inWhich); }
extern "C" EeUiRect ee_uicpp_label_fit_rect(void* inLabel, int inWhich) { return UITextLabelRust::Rect(inLabel, inWhich); }
extern "C" void ee_uicpp_label_fit_measure(void* inLabel, unsigned long inLine, float* outWidth, float* outHeight)
{
    UITextLabelRust::Measure(inLabel, inLine, outWidth, outHeight);
}
extern "C" void ee_uicpp_label_fit_call(void* inLabel, int inCall, unsigned long inValue, EeUiRect inRectangle, float inLength)
{
    UITextLabelRust::Call(inLabel, inCall, inValue, inRectangle, inLength);
}

// FUNCTION: LLE 0x1007e9cb
// Creates, updates or marks for removal the scroll bar depending on whether the lines overflow the visible rows
// (ee_ui label_fit.rs)
void UITextLabel::CheckScrollBar()
{
    ee_ui_label_check_scroll_bar(this);
}

// FUNCTION: LLE 0x1007ebf2
// With fit-to-text on, shrinks the label rectangle around its (visible) lines and marks it dirty when it changed
// (ee_ui label_fit.rs)
void UITextLabel::ComputeTextLabelRect()
{
    ee_ui_label_compute_rect(this);
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
        // placed by its alignment in the text rectangle (ee_ui draw.rs)
        const GERectangle& theExtent = this->mCopiedText->GetTextExtent();
        EeUiPoint theLocation = ee_ui_label_one_line_location(this->mAlignment, this->mFitRectToText,
                                                              UITextLabelRect(this->mTextRectangle), theExtent.GetWidth(),
                                                              theExtent.GetHeight());
        this->mCopiedText->SetLocation(GE2DPoint(theLocation.x, theLocation.y));
    } else {
        if (this->mScrollBarRemoved) {
            delete this->mScrollBar;
            this->mScrollBarRemoved = false;
            this->mScrollBar = NULL;
        }
        if (this->mRecomputePending)
            this->RecomputeDisplayRange();
        // the scroll bar's window of lines, or every line (ee_ui draw.rs)
        bool theScroll = this->mScrollBar != NULL;
        EeUiLineRange theRange = ee_ui_label_line_range(theScroll, theScroll ? this->mScrollBar->GetSlider()->GetCurrentRangeValue() : 0,
                                                        this->mNumberLinesDisplayed, this->mLines.size());
        unsigned long i = theRange.first;
        unsigned long theEnd = theRange.end;
        this->mLastDisplayLine = theRange.last;
        this->mDrawScrollBar = theScroll;
        if (theScroll)
            this->mScrollBar->CopyData();
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
        unsigned long theOffset = 0;
        for (; i <= this->mLastDisplayLine; i++, theOffset += theLineHeight) {
            this->mCopiedText->SetText(this->mDisplayLines[i]);
            // placed by its alignment, clipped to the rest of its row (ee_ui draw.rs)
            EeUiLabelLine thePlan = ee_ui_label_line(
                this->mAlignment, this->mFitRectToText, UITextLabelRect(this->mRectangle),
                UITextLabelRect(this->mCopiedTextRectangle), this->mCopiedText->GetTextExtent().GetWidth(),
                this->mScrollBar != NULL, this->mScrollBar ? this->mScrollBar->GetCopiedDrawRectangle().GetWidth() : 0.0f,
                theOffset, theLineHeight);
            this->mCopiedText->SetLocation(thePlan.at.x, thePlan.at.y);
            theRectangle = GERectangle(thePlan.rect.left, thePlan.rect.top, thePlan.rect.right, thePlan.rect.bottom);
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

// SetText's wrapping state for ee_ui_label_wrap: the label's text object (it measures each word, keeping the last
// one as ReadLine left it) and the lines made so far (appended to the label's in order at the end)
struct UITextLabelWrap {
    GEText* mText;
    std::vector<UWideString*> mLines;
};

// A word's width, as ReadLine measured it (UMFloor is Rust's)
extern "C" float ee_uicpp_label_measure(void* inContext, const char* inWord)
{
    GEText* theText = static_cast<UITextLabelWrap*>(inContext)->mText;
    theText->SetText(UWideString(inWord));
    return theText->GetTextExtent().GetWidth();
}

// One wrapped line (no final newline)
extern "C" void ee_uicpp_label_add_line(void* inContext, const char* inLine)
{
    static_cast<UITextLabelWrap*>(inContext)->mLines.push_back(new UWideString(inLine));
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
// Moves the label, recomputing the text and scroll bar rectangles (ee_ui label_fit.rs) and moving the scroll bar
void UITextLabel::SetLocation(const GE2DPoint& inLocation)
{
    UIControl::SetLocation(inLocation);
    EeUiRect theScrollBar;
    EeUiRect theText;
    ee_ui_label_location_rects(UITextLabelRect(this->mRectangle), this->GetBorderThickness(), this->mScrollBar != NULL,
                               &theScrollBar, &theText);
    this->mScrollBarRectangle = GERectangle(theScrollBar.left, theScrollBar.top, theScrollBar.right, theScrollBar.bottom);
    this->mTextRectangle = GERectangle(theText.left, theText.top, theText.right, theText.bottom);
    if (this->mScrollBar != NULL)
        this->mScrollBar->SetLocation(this->mScrollBarRectangle.GetTopLeft());
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
        UITextLabelWrap theWrap;
        theWrap.mText = this->mText;
        ee_ui_label_wrap(&theWrap, theText, this->mTextRectangle.GetWidth());
        this->mLines.insert(this->mLines.end(), theWrap.mLines.begin(), theWrap.mLines.end());
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
