// C:\EEX\Low-Level Engine\User Interface\UITLScrolling.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007fd7d..0x100805c4: the ctor, the dtor, then the out-of-line members in alphabetical order. No key init chains
// (the unit does not include the rasterizer header) and no COMDATs: the inline members, the implicit copy members
// and ??_E are emitted by UIControlsTable.cpp. UITriStateCheckBox.cpp starts at 0x100805c4.
//
// The scroll's timing and offsets are Rust (rust/crates/ee_ui/src/scrolling.rs, docs/RUST_PLAN.md R6): CopyData's
// scroll and snapshot, both ScrollTexts, Pause/Resume/StopScrolling and DrawTextA's line places. The timers, the
// lines, the trigger and the draws stay here (UITLScrollingRust answers the Rust's questions).
#include <windows.h>

#include "rust/ee_ui.h"
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

// The Rust scroll's view of the label (ee_ui scrolling.rs)
struct UITLScrollingRust {
    static UITLScrolling* L(void* inLabel) { return static_cast<UITLScrolling*>(inLabel); }
    static TMPoll* Poll(UITLScrolling* l, int inPoll)
    {
        return inPoll == 0 ? l->mScrollPoll : inPoll == 1 ? l->mStartPoll : l->mEndPoll;
    }
    static unsigned long Value(void* inLabel, int inWhich)
    {
        UITLScrolling* l = L(inLabel);
        switch (inWhich) {
        case 0:
            return l->GetGEText()->GetFont()->GetHeight();
        case 1:
            return l->GetBorderThickness();
        case 2:
            return l->GetLineCount();
        default:
            return l->GetNumberLinesDisplayed();
        }
    }
    static EeUiRect Rect(void* inLabel, int inWhich)
    {
        const GERectangle& r = inWhich == 0 ? L(inLabel)->mRectangle : L(inLabel)->GetTextRectangle();
        return EeUiRect{r.mBottom, r.mLeft, r.mRight, r.mTop};
    }
    static unsigned long Get(void* inLabel, int inField)
    {
        UITLScrolling* l = L(inLabel);
        switch (inField) {
        case 0:
            return l->mStartingLine;
        case 1:
            return l->mDrawStartingLine;
        case 2:
            return l->mEndOffset;
        case 3:
            return l->mDrawLineCount;
        case 4:
            return (unsigned long)l->mPixelStartingOffset;
        case 5:
            return l->mDrawPixelOffset;
        default:
            return l->mScrolling;
        }
    }
    static void Set(void* inLabel, int inField, unsigned long inValue)
    {
        UITLScrolling* l = L(inLabel);
        switch (inField) {
        case 0:
            l->mStartingLine = inValue;
            break;
        case 1:
            l->mDrawStartingLine = inValue;
            break;
        case 2:
            l->mEndOffset = inValue;
            break;
        case 3:
            l->mDrawLineCount = inValue;
            break;
        case 4:
            l->mPixelStartingOffset = (long)inValue;
            break;
        case 5:
            l->mDrawPixelOffset = inValue;
            break;
        default:
            l->mScrolling = inValue != 0;
            break;
        }
    }
    static unsigned long PollCall(void* inLabel, int inPoll, int inCall, unsigned long inArg)
    {
        TMPoll* p = Poll(L(inLabel), inPoll);
        switch (inCall) {
        case 0:
            return p->GetIsRunning();
        case 1:
            return p->Poll();
        case 2:
            p->Start();
            return 0;
        case 3:
            p->Stop();
            return 0;
        case 4:
            p->Pause();
            return 0;
        case 5:
            p->Resume();
            return 0;
        default:
            p->SetTimeDelay(inArg);
            return 0;
        }
    }
    static long Call(void* inLabel, int inCall, unsigned long inA, unsigned long inB)
    {
        UITLScrolling* l = L(inLabel);
        switch (inCall) {
        case 0:
            if (l->mDrawLines.size() != inA)
                l->mDrawLines.resize(inA);
            return 0;
        case 1:
            l->mDrawRectangle = l->GetTextRectangle();
            return 0;
        case 2:
            l->mDrawLines[inA] = l->GetText(inB);
            return 0;
        case 3:
            return l->UIControl::CopyData();
        case 4:
            l->FireTriggerEvent(kControlTriggerScrollingFinished);
            return 0;
        default:
            l->SetDirty(true);
            return 0;
        }
    }
};
extern "C" unsigned long ee_uicpp_scrolling_value(void* inLabel, int inWhich) { return UITLScrollingRust::Value(inLabel, inWhich); }
extern "C" EeUiRect ee_uicpp_scrolling_rect(void* inLabel, int inWhich) { return UITLScrollingRust::Rect(inLabel, inWhich); }
extern "C" unsigned long ee_uicpp_scrolling_get(void* inLabel, int inField) { return UITLScrollingRust::Get(inLabel, inField); }
extern "C" void ee_uicpp_scrolling_set(void* inLabel, int inField, unsigned long inValue)
{
    UITLScrollingRust::Set(inLabel, inField, inValue);
}
extern "C" unsigned long ee_uicpp_scrolling_poll(void* inLabel, int inPoll, int inCall, unsigned long inArg)
{
    return UITLScrollingRust::PollCall(inLabel, inPoll, inCall, inArg);
}
extern "C" long ee_uicpp_scrolling_call(void* inLabel, int inCall, unsigned long inA, unsigned long inB)
{
    return UITLScrollingRust::Call(inLabel, inCall, inA, inB);
}

// FUNCTION: LLE 0x1007ff7c
// Advances the scroll by elapsed time and snapshots the visible lines; fires scrolling-finished after the end delay
// (ee_ui scrolling.rs)
long UITLScrolling::CopyData()
{
    *this->mDrawText = *this->GetGEText();
    return ee_ui_scrolling_copy_data(this);
}

// FUNCTION: LLE 0x10080261
// Draws the snapshotted lines one font height apart, clipped to the text area
long UITLScrolling::DrawTextA()
{
    long theResult = UIControl::DrawTextA();
    unsigned long theOffset = this->mDrawPixelOffset;
    for (unsigned long i = 0; i < this->mDrawLineCount; i++) {
        this->mDrawText->SetText(this->mDrawLines[i]);
        EeUiPoint theAt = ee_ui_scrolling_line_at(this->mDrawRectangle.GetLeft(), this->mDrawText->GetFont()->GetHeight(),
                                                  i, theOffset);
        this->mDrawText->SetLocation(GE2DPoint(theAt.x, theAt.y));
        theResult = this->GetForm()->DrawClippedText(this->mDrawRectangle, this->mDrawText, this);
    }
    return theResult;
}

// FUNCTION: LLE 0x10080306
// Pauses the running scroll timer
void UITLScrolling::PauseScrolling()
{
    ee_ui_scrolling_pause(this, false);
}

// FUNCTION: LLE 0x10080345
// Resumes the paused scroll timer and redraws
void UITLScrolling::ResumeScrolling()
{
    ee_ui_scrolling_pause(this, true);
}

// FUNCTION: LLE 0x10080383
// Starts scrolling at a speed in lines per second after a start delay; stops an end delay after the end offset
void UITLScrolling::ScrollText(float inSpeed, long inStartDelay, long inEndDelay, long inStartOffset, long inEndOffset)
{
    ee_ui_scrolling_scroll_text(this, inSpeed, inStartDelay, inEndDelay, inStartOffset, inEndOffset);
}

// FUNCTION: LLE 0x1008048d
// Starts scrolling to fill a total time at half a line per second, with start and end delays from the line count
void UITLScrolling::ScrollText(unsigned long inTime, unsigned long inEndDelay)
{
    ee_ui_scrolling_scroll_text_for(this, inTime, inEndDelay);
}

// FUNCTION: LLE 0x10080590
// Stops scrolling and fires the scrolling-finished trigger
void UITLScrolling::StopScrolling()
{
    ee_ui_scrolling_stop(this);
}
