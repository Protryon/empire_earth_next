// C:\EEX\Low-Level Engine\User Interface\UITLScrolling.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// The original unit: 0x1007fd7d..0x100805c4 (the ctor, the dtor, the out-of-line members in alphabetical order); the
// inline members, the implicit copy members and ??_E were emitted by UIControlsTable.cpp.
//
// UITLScrolling is whole in Rust (rust/crates/ee_ui/src/labelobj.rs, R6 ui round 13): the Rust label's scrolling
// part (its init call below) holds the state; the scroll's timing and offsets are scrolling.rs. The label's
// destructor frees the part (the polls, the draw text, the draw lines) before the label's own.
#include <windows.h>
#include <string.h>

#include "rust/ui/labelobj.h"
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/TSCriticalSection.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UITextLabel.h"
#include "lle/UITLScrolling.h"

struct UITLScrollingRust {
    static intptr_t Call(const UITLScrolling* inLabel, EeUiLabelCall inCall, intptr_t a = 0, intptr_t b = 0,
                         float* f = NULL)
    {
        return ee_ui_label_call(inLabel->mRust, inCall, a, b, f);
    }
};

// Creates a scrolling text label with scroll, start and end timers and a text area inset by the border; not scrolling
UITLScrolling::UITLScrolling(const UWideString& inText, GEFont* inFont, unsigned long inId,
                             const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                             unsigned long inBorderThickness)
    : UITextLabel(inText, inFont, inId, inRectangle, inForm, 1, inIndex, inBorderThickness)
{
    UITLScrollingRust::Call(this, EeUiLabelCall::ScrollInit, (intptr_t)inFont);
}

void UITLScrolling::PauseScrolling() { UITLScrollingRust::Call(this, EeUiLabelCall::PauseScrolling); }
void UITLScrolling::ResumeScrolling() { UITLScrollingRust::Call(this, EeUiLabelCall::ResumeScrolling); }
// the offsets travel as their 32 bits in the floats
void UITLScrolling::ScrollText(float inSpeed, long inStartDelay, long inEndDelay, long inStartOffset, long inEndOffset)
{
    int theStart = (int)inStartOffset, theEnd = (int)inEndOffset;
    float f[4] = {inSpeed, 0.0f, 0.0f, 0.0f};
    memcpy(&f[1], &theStart, 4);
    memcpy(&f[2], &theEnd, 4);
    UITLScrollingRust::Call(this, EeUiLabelCall::ScrollText, inStartDelay, inEndDelay, f);
}
void UITLScrolling::ScrollText(unsigned long inTime, unsigned long inEndDelay)
{
    UITLScrollingRust::Call(this, EeUiLabelCall::ScrollTextFor, inTime, inEndDelay);
}
void UITLScrolling::SetPixelStartingOffset(unsigned long inOffset)
{
    UITLScrollingRust::Call(this, EeUiLabelCall::SetPixelStartingOffset, inOffset);
}
void UITLScrolling::SetStartingLine(unsigned long inLine) { UITLScrollingRust::Call(this, EeUiLabelCall::SetStartingLine, inLine); }
void UITLScrolling::StopScrolling() { UITLScrollingRust::Call(this, EeUiLabelCall::StopScrolling); }
