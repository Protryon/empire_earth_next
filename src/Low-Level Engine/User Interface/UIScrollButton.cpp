// C:\EEX\Low-Level Engine\User Interface\UIScrollButton.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007d013..0x1007d139: the ctor and ButtonDown only. No U2D key init chains: the unit does not include the
// rasterizer header. The header-inline members are emitted first by UICheckBox.cpp (0x1005f272..).
//
// ButtonDown's stepping is Rust (rust/crates/ee_ui/src/button.rs, docs/RUST_PLAN.md R6); the polls and the slider
// stay here, reached through the ee_uicpp_scroll_button_* host calls.
#include <windows.h>
#include <stddef.h>

#include "rust/ee_ui.h"

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/TMPoll.h"
#include "lle/UIControl.h"
#include "lle/UIButton.h"
#include "lle/UISlider.h"
#include "lle/UIScrollButton.h"
#include "lle/UITextLabel.h"
#include "UIButtonRust.h"

void* UIButtonRust::ScrollFields(UIScrollButton* inButton)
{
    static_assert(sizeof(inButton->mDecrement) == sizeof(unsigned long), "ee_ui/src/button_ffi.rs");
    return &inButton->mDecrement;
}
TMPoll* UIButtonRust::ScrollDelay(void* inButton) { return static_cast<UIScrollButton*>(Button(inButton))->mDelayPoll; }
TMPoll* UIButtonRust::ScrollRepeat(void* inButton) { return static_cast<UIScrollButton*>(Button(inButton))->mRepeatPoll; }
UISlider* UIButtonRust::ScrollSlider(void* inButton) { return static_cast<UIScrollButton*>(Button(inButton))->mSlider; }

// The scroll button's own host calls (ee_ui/src/button_ffi.rs)
extern "C" bool ee_uicpp_scroll_button_delay_running(void* inButton) { return UIButtonRust::ScrollDelay(inButton)->GetIsRunning(); }
extern "C" bool ee_uicpp_scroll_button_delay_poll(void* inButton) { return UIButtonRust::ScrollDelay(inButton)->Poll() != 0; }
extern "C" void ee_uicpp_scroll_button_delay_start(void* inButton) { UIButtonRust::ScrollDelay(inButton)->Start(); }
extern "C" void ee_uicpp_scroll_button_delay_stop(void* inButton) { UIButtonRust::ScrollDelay(inButton)->Stop(); }
extern "C" bool ee_uicpp_scroll_button_repeat_running(void* inButton) { return UIButtonRust::ScrollRepeat(inButton)->GetIsRunning(); }
extern "C" bool ee_uicpp_scroll_button_repeat_poll(void* inButton) { return UIButtonRust::ScrollRepeat(inButton)->Poll() != 0; }
extern "C" void ee_uicpp_scroll_button_repeat_start(void* inButton) { UIButtonRust::ScrollRepeat(inButton)->Start(); }
extern "C" long ee_uicpp_scroll_button_slider_value(void* inButton)
{
    return UIButtonRust::ScrollSlider(inButton)->GetCurrentRangeValue();
}
extern "C" void ee_uicpp_scroll_button_set_slider_value(void* inButton, long inValue)
{
    UIButtonRust::ScrollSlider(inButton)->SetCurrentRangeValue(inValue);
}

// FUNCTION: LLE 0x1007d013
// Creates a 20x20 scroll arrow button that steps a slider down or up, with a 500 ms delay and 100 ms repeat
UIScrollButton::UIScrollButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inPressedBitmap,
                               const GE2DPoint& inLocation, UIForm* inForm, UISlider* inSlider,
                               unsigned long inDecrement, unsigned long inIndex)
    : UIButton(inId, inNormalBitmap, NULL, inPressedBitmap,
               GERectangle(inLocation, GE2DPoint(inLocation.GetX() + 19.0f,
                                                 inLocation.GetY() + 19.0f)),
               inForm, inIndex, 0)
{
    this->mDecrement = inDecrement;
    this->mSlider = inSlider;
    this->mDelayPoll = new TMPoll(500, 0, false);
    this->mRepeatPoll = new TMPoll(100, 0, false);
}

// FUNCTION: LLE 0x1007d0c3
// Steps the slider while held: once on press, again after the 500 ms delay, then every 100 ms
void UIScrollButton::ButtonDown()
{
    ee_ui_scroll_button_button_down(UIButtonRust::Refs(this, UIButtonRust::ScrollFields(this)));
}
