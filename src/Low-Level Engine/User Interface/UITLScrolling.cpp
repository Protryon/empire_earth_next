// C:\EEX\Low-Level Engine\User Interface\UITLScrolling.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007fd7d..0x100805c4: the ctor, the dtor, then the out-of-line members in alphabetical order. No key init chains
// (the unit does not include the rasterizer header) and no COMDATs: the inline members, the implicit copy members
// and ??_E are emitted by UIControlsTable.cpp. UITriStateCheckBox.cpp starts at 0x100805c4.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UITextLabel.h"
#include "lle/UITLScrolling.h"

// FUNCTION: LLE 0x1007fd7d
// Creates a scrolling text label with scroll, start and end timers and a text area inset by the border; not scrolling
UITLScrolling::UITLScrolling(const UWideString& inText, GEFont* inFont, unsigned long inId,
                             const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                             unsigned long inBorderThickness)
    : UITextLabel(inText, inFont, inId, inRectangle, inForm, 1, inIndex, inBorderThickness)
{
    this->mScrollPoll = new TMPoll(1000, 0, false);
    this->mStartPoll = new TMPoll(1000, 0, false);
    this->mEndPoll = new TMPoll(1000, 0, false);
    this->mDrawText = new GEText(inFont);
    this->mStartingLine = 0;
    this->mDrawStartingLine = 0;
    this->mEndOffset = 0;
    this->mDrawLineCount = 0;
    this->mScrolling = false;
    this->mPixelStartingOffset = UMFloor(this->mRectangle.GetTop() + this->GetBorderThickness() + 1.0f);
    this->mDrawPixelOffset = 0;
    this->SetTextRectangle(GERectangle(this->mRectangle.GetLeft() + this->GetBorderThickness() + 1.0f,
                                 this->mRectangle.GetTop() + this->GetBorderThickness() + 1.0f,
                                 this->mRectangle.GetRight() - this->GetBorderThickness() - 1.0f,
                                 this->mRectangle.GetBottom() - this->GetBorderThickness() - 1.0f));
}

// FUNCTION: LLE 0x1007ff1b
// Destroys the scrolling label with its timers and draw text
UITLScrolling::~UITLScrolling()
{
    delete this->mScrollPoll;
    delete this->mStartPoll;
    delete this->mEndPoll;
    if (this->mDrawText)
        delete this->mDrawText;
}

// FUNCTION: LLE 0x1007ff7c
// Advances the scroll by elapsed time and snapshots the visible lines; fires scrolling-finished after the end delay
long UITLScrolling::CopyData()
{
    *this->mDrawText = *this->GetGEText();
    unsigned long theHeight = this->mDrawText->GetFont()->GetHeight();
    float theLineHeight = theHeight;
    long theLines = UMFloor((this->mRectangle.GetHeight() - 2 * this->GetBorderThickness()) / theLineHeight) + 2;
    if (this->mDrawLines.size() != theLines)
        this->mDrawLines.resize(theLines);
    this->mDrawRectangle = this->GetTextRectangle();
    if (this->mScrollPoll->GetIsRunning())
        this->mPixelStartingOffset -= this->mScrollPoll->Poll();
    if (this->mStartPoll->GetIsRunning() && this->mStartPoll->Poll() > 0) {
        this->mStartPoll->Stop();
        this->mScrollPoll->Start();
    }
    while (this->mPixelStartingOffset < this->mRectangle.GetTop() + this->GetBorderThickness() + 1.0f - theLineHeight) {
        this->mPixelStartingOffset += theHeight;
        this->mStartingLine++;
    }
    if (this->mStartingLine < this->GetLineCount()) {
        this->mDrawPixelOffset = this->mPixelStartingOffset;
        this->mDrawStartingLine = this->mStartingLine;
        unsigned long i = 0;
        long theBottom = UMFloor(this->mRectangle.GetBottom() - this->GetBorderThickness());
        for (unsigned long y = this->mPixelStartingOffset; y < theBottom; y += theHeight) {
            if (this->mStartingLine + i >= this->GetLineCount())
                break;
            this->mDrawLines[i] = this->GetText(this->mStartingLine + i);
            i++;
        }
        this->mDrawLineCount = i;
        if (this->mScrollPoll->GetIsRunning() == true && this->mStartingLine + i >= this->GetLineCount() &&
            i * theHeight + (this->mPixelStartingOffset - this->mRectangle.GetTop() + (this->GetBorderThickness() + 1.0f)) < this->mEndOffset) {
            this->mScrollPoll->Stop();
            this->mEndPoll->Start();
        }
    } else if (this->mScrolling == true) {
        this->mScrollPoll->Stop();
        this->mEndPoll->Start();
    }
    long theResult = UIControl::CopyData();
    if (this->mEndPoll->GetIsRunning() && this->mEndPoll->Poll() > 0) {
        this->mScrolling = false;
        this->mEndPoll->Stop();
        this->FireTriggerEvent(kControlTriggerScrollingFinished);
    } else if (this->mScrolling == true)
        this->SetDirty(true);
    return theResult;
}

// FUNCTION: LLE 0x10080261
// Draws the snapshotted lines one font height apart, clipped to the text area
long UITLScrolling::DrawTextA()
{
    long theResult = UIControl::DrawTextA();
    unsigned long theOffset = this->mDrawPixelOffset;
    for (unsigned long i = 0; i < this->mDrawLineCount; i++) {
        this->mDrawText->SetText(this->mDrawLines[i]);
        this->mDrawText->SetLocation(GE2DPoint(this->mDrawRectangle.GetLeft(), this->mDrawText->GetFont()->GetHeight() * i + theOffset));
        theResult = this->GetForm()->DrawClippedText(this->mDrawRectangle, this->mDrawText, this);
    }
    return theResult;
}

// FUNCTION: LLE 0x10080306
// Pauses the running scroll timer
void UITLScrolling::PauseScrolling()
{
    if (!this->mScrolling)
        return;
    if (this->mEndPoll->GetIsRunning())
        this->mEndPoll->Pause();
    else if (this->mStartPoll->GetIsRunning())
        this->mStartPoll->Pause();
    else if (this->mScrollPoll->GetIsRunning())
        this->mScrollPoll->Pause();
}

// FUNCTION: LLE 0x10080345
// Resumes the paused scroll timer and redraws
void UITLScrolling::ResumeScrolling()
{
    if (!this->mScrolling)
        return;
    if (this->mEndPoll->GetIsRunning())
        this->mEndPoll->Resume();
    else if (this->mStartPoll->GetIsRunning())
        this->mStartPoll->Resume();
    else if (this->mScrollPoll->GetIsRunning())
        this->mScrollPoll->Resume();
    this->SetDirty(true);
}

// FUNCTION: LLE 0x10080383
// Starts scrolling at a speed in lines per second after a start delay; stops an end delay after the end offset
void UITLScrolling::ScrollText(float inSpeed, long inStartDelay, long inEndDelay, long inStartOffset, long inEndOffset)
{
    float theDelay = 1000.0f / (this->GetGEText()->GetFont()->GetHeight() * inSpeed);
    if (theDelay < 1.0f)
        theDelay = 1.0f;
    this->mScrollPoll->Stop();
    this->mScrollPoll->SetTimeDelay(UMFloor(theDelay));
    this->mEndPoll->Stop();
    this->mEndPoll->SetTimeDelay(inEndDelay);
    this->mStartPoll->Stop();
    this->mStartPoll->SetTimeDelay(inStartDelay);
    this->mStartPoll->Start();
    this->mPixelStartingOffset = UMFloor(this->GetBorderThickness() + this->mRectangle.GetTop() + inStartOffset + 1.0f);
    this->mEndOffset = inEndOffset;
    this->mStartingLine = 0;
    this->mScrolling = true;
    this->SetDirty(true);
}

// FUNCTION: LLE 0x1008048d
// Starts scrolling to fill a total time at half a line per second, with start and end delays from the line count
void UITLScrolling::ScrollText(unsigned long inTime, unsigned long inEndDelay)
{
    unsigned long theStartDelay;
    unsigned long theEndDelay = inEndDelay;
    if (this->GetLineCount() > this->GetNumberLinesDisplayed()) {
        theStartDelay = UMFloatToLong(inTime / this->GetLineCount() * (this->GetNumberLinesDisplayed() * 0.5f));
        theEndDelay = inTime -
                      UMFloatToLong(((this->GetLineCount() - this->GetTextRectangle().GetHeight() /
                                                           this->GetGEText()->GetFont()->GetHeight()) * 2) * 1000.0f) -
                      theStartDelay;
    } else
        theStartDelay = inTime;
    this->ScrollText(0.5f, theStartDelay, theEndDelay, 0, UMFloor(this->mRectangle.GetHeight()));
}

// FUNCTION: LLE 0x10080590
// Stops scrolling and fires the scrolling-finished trigger
void UITLScrolling::StopScrolling()
{
    if (!this->mScrolling)
        return;
    this->mScrolling = false;
    this->mEndPoll->Stop();
    this->mScrollPoll->Stop();
    this->mStartPoll->Stop();
    this->FireTriggerEvent(kControlTriggerScrollingFinished);
}
