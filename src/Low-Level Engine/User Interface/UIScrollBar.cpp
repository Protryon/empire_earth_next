// C:\EEX\Low-Level Engine\User Interface\UIScrollBar.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007c901..0x1007d013: the U2D key init chains, the ctor, then the out-of-line members in alphabetical order. The
// header-inline members are emitted first by UICheckBox.cpp (0x1005f3b0..).
//
// The bar's logic is Rust (rust/crates/ee_ui/src/scrollbar.rs, docs/RUST_PLAN.md R6): the constructor's layout, the
// mouse routing to the part under it, SetLocation's layout and IncrementBy. The members below forward to it with the
// object's members from mIncrementButton on as the pinned record EeUiScrollBarFields; the parts, the bitmaps and
// drawing stay here, reached through the ee_uicpp_scroll_bar_* host calls.
#include <windows.h>
#include <stddef.h>

#include "rust/ee_ui.h"

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
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIButton.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UISlider.h"
#include "lle/UIScrollButton.h"
#include "lle/UIScrollBar.h"


// --- the Rust scroll bar's view of the bar (ee_ui/src/scrollbar_ffi.rs) ---------------------------------------------

struct UIScrollBarRust {
    static UIScrollBar* Bar(void* inBar) { return static_cast<UIScrollBar*>(inBar); }
    struct LayoutResult {
        GERectangle mRectangle;
        GERectangle mSlider;
        GE2DPoint mIncrement;
    };
    static LayoutResult Layout(const GE2DPoint& inLocation, float inLength, unsigned long inStyle)
    {
        EeUiScrollBarLayout theLayout = ee_ui_scroll_bar_layout(EeUiPoint{inLocation.GetX(), inLocation.GetY()}, inLength, inStyle);
        LayoutResult theResult;
        theResult.mRectangle = GERectangle(theLayout.rect.left, theLayout.rect.top, theLayout.rect.right, theLayout.rect.bottom);
        theResult.mSlider = GERectangle(theLayout.slider.left, theLayout.slider.top, theLayout.slider.right, theLayout.slider.bottom);
        theResult.mIncrement = GE2DPoint(theLayout.increment.x, theLayout.increment.y);
        return theResult;
    }
    static EeUiScrollBarFields* Fields(UIScrollBar* inBar)
    {
#define EE_UI_SCROLL_BAR_FIELD(member, field)                                                                         \
    static_assert(offsetof(UIScrollBar, member) - offsetof(UIScrollBar, mIncrementButton) ==                          \
                      offsetof(EeUiScrollBarFields, field),                                                           \
                  "ee_ui/src/scrollbar.rs")
        EE_UI_SCROLL_BAR_FIELD(mIncrementButton, increment_button);
        EE_UI_SCROLL_BAR_FIELD(mDecrementButton, decrement_button);
        EE_UI_SCROLL_BAR_FIELD(mSlider, slider);
        EE_UI_SCROLL_BAR_FIELD(mIncrementPressed, increment_pressed);
        EE_UI_SCROLL_BAR_FIELD(mDecrementPressed, decrement_pressed);
        EE_UI_SCROLL_BAR_FIELD(mSliderPressed, slider_pressed);
#undef EE_UI_SCROLL_BAR_FIELD
        static_assert(sizeof(UIScrollBar) - offsetof(UIScrollBar, mIncrementButton) >= sizeof(EeUiScrollBarFields),
                      "ee_ui/src/scrollbar.rs");
        static_assert(sizeof(GERectangle) == sizeof(EeUiRect), "ee_ui/src/rect.rs");
        return reinterpret_cast<EeUiScrollBarFields*>(&inBar->mIncrementButton);
    }
    static const EeUiRect* Rectangle(UIScrollBar* inBar) { return reinterpret_cast<const EeUiRect*>(&inBar->mRectangle); }
    // the part (0 increment, 1 decrement, 2 slider) as the control the bar drives
    static UIControl* Part(void* inBar, int inPart)
    {
        if (inPart == 0)
            return Bar(inBar)->mIncrementButton;
        if (inPart == 1)
            return Bar(inBar)->mDecrementButton;
        return Bar(inBar)->mSlider;
    }
    static long PartMouseDown(void* inBar, int inPart)
    {
        if (inPart == 2)
            return Bar(inBar)->mSlider->MouseLButtonDown();
        return (inPart == 0 ? Bar(inBar)->mIncrementButton : Bar(inBar)->mDecrementButton)->MouseLButtonDown();
    }
    static long PartMouseUp(void* inBar, int inPart)
    {
        if (inPart == 2)
            return Bar(inBar)->mSlider->MouseLButtonUp();
        return (inPart == 0 ? Bar(inBar)->mIncrementButton : Bar(inBar)->mDecrementButton)->MouseLButtonUp();
    }
    static UIScrollButton* Button(void* inBar, int inPart)
    {
        return inPart == 0 ? Bar(inBar)->mIncrementButton : Bar(inBar)->mDecrementButton;
    }
    static void CaptureMouse(void* inBar) { Bar(inBar)->CaptureMouse(); }
    static UISlider* Slider(void* inBar) { return Bar(inBar)->mSlider; }
    static long SliderMouseMove(void* inBar, float inX, float inY) { return Bar(inBar)->mSlider->MouseMove(GE2DPoint(inX, inY)); }
    static void ButtonDown(void* inBar, int inPart) { Button(inBar, inPart)->ButtonDown(); }
    static const GE2DPoint& MouseLocation(void* inBar) { return Bar(inBar)->GetMouseLocation(); }
};

static EeUiPoint ScrollBarPoint(const GE2DPoint& inPoint) { return EeUiPoint{inPoint.GetX(), inPoint.GetY()}; }
#define EE_UI_SCROLL_BAR_ARGS this, UIScrollBarRust::Fields(this), UIScrollBarRust::Rectangle(this)

// The host calls of ee_ui/src/scrollbar_ffi.rs
extern "C" EeUiPoint ee_uicpp_scroll_bar_mouse_location(void* inBar)
{
    return ScrollBarPoint(UIScrollBarRust::MouseLocation(inBar));
}
extern "C" EeUiRect ee_uicpp_scroll_bar_part_rect(void* inBar, int inPart)
{
    const GERectangle& theRectangle = UIScrollBarRust::Part(inBar, inPart)->GetRectangle();
    return EeUiRect{theRectangle.mBottom, theRectangle.mLeft, theRectangle.mRight, theRectangle.mTop};
}
extern "C" long ee_uicpp_scroll_bar_part_mouse_down(void* inBar, int inPart) { return UIScrollBarRust::PartMouseDown(inBar, inPart); }
extern "C" long ee_uicpp_scroll_bar_part_mouse_up(void* inBar, int inPart) { return UIScrollBarRust::PartMouseUp(inBar, inPart); }
extern "C" long ee_uicpp_scroll_bar_slider_mouse_move(void* inBar, float inX, float inY)
{
    return UIScrollBarRust::SliderMouseMove(inBar, inX, inY);
}
extern "C" void ee_uicpp_scroll_bar_button_down(void* inBar, int inPart) { UIScrollBarRust::ButtonDown(inBar, inPart); }
extern "C" void ee_uicpp_scroll_bar_capture_mouse(void* inBar) { UIScrollBarRust::CaptureMouse(inBar); }
extern "C" void ee_uicpp_scroll_bar_release_mouse(void* inBar) { UIScrollBarRust::Bar(inBar)->ReleaseCapturedMouse(); }
extern "C" void ee_uicpp_scroll_bar_base_set_location(void* inBar, float inX, float inY)
{
    UIScrollBarRust::Bar(inBar)->UIControl::SetLocation(GE2DPoint(inX, inY));
}
extern "C" unsigned long ee_uicpp_scroll_bar_slider_style(void* inBar) { return UIScrollBarRust::Slider(inBar)->GetSliderStyle(); }
extern "C" void ee_uicpp_scroll_bar_part_set_location(void* inBar, int inPart, float inX, float inY)
{
    UIScrollBarRust::Part(inBar, inPart)->SetLocation(GE2DPoint(inX, inY));
}
extern "C" void ee_uicpp_scroll_bar_slider_set_rectangle(void* inBar, const EeUiRect* inRectangle)
{
    UIScrollBarRust::Slider(inBar)->SetRectangle(
        GERectangle(inRectangle->left, inRectangle->top, inRectangle->right, inRectangle->bottom));
}
extern "C" long ee_uicpp_scroll_bar_slider_value(void* inBar) { return UIScrollBarRust::Slider(inBar)->GetCurrentRangeValue(); }
extern "C" void ee_uicpp_scroll_bar_set_slider_value(void* inBar, long inValue)
{
    UIScrollBarRust::Slider(inBar)->SetCurrentRangeValue(inValue);
}

// FUNCTION: LLE 0x1007ca21
// Creates a vertical (style 1) or horizontal scroll bar: a slider between two arrow buttons
// Fixed: the rectangle's size was `x + (style == 1) ? 19 : length - 1` (a precedence slip), which made it
// 19 x (length - 1) for both styles; a horizontal bar is now (length - 1) x 19
UIScrollBar::UIScrollBar(unsigned long inId, const GE2DPoint& inLocation, float inLength, unsigned long inStyle,
                         UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
    : UIControl(kControlTypeScrollBar, inId,
                UIScrollBarRust::Layout(inLocation, inLength, inStyle).mRectangle, inForm, inIndex, 0)
{
    UIScrollBarRust::LayoutResult theLayout = UIScrollBarRust::Layout(inLocation, inLength, inStyle);
    GEBitmap* theDecrementNormal;
    GEBitmap* theDecrementPressed;
    GEBitmap* theIncrementNormal;
    GEBitmap* theIncrementPressed;
    this->mSliderPressed = false;
    this->mDecrementPressed = false;
    this->mIncrementPressed = false;
    if (inStyle == 1) {
        theIncrementNormal = this->mForm->GetManager()->GetSBBitmapDownNormal();
        theDecrementNormal = this->mForm->GetManager()->GetSBBitmapUpNormal();
        theIncrementPressed = this->mForm->GetManager()->GetSBBitmapDownPressed();
        theDecrementPressed = this->mForm->GetManager()->GetSBBitmapUpPressed();
    } else {
        theIncrementNormal = this->mForm->GetManager()->GetSBBitmapRightNormal();
        theDecrementNormal = this->mForm->GetManager()->GetSBBitmapLeftNormal();
        theIncrementPressed = this->mForm->GetManager()->GetSBBitmapRightPressed();
        theDecrementPressed = this->mForm->GetManager()->GetSBBitmapLeftPressed();
    }
    this->mSlider = new UISlider(this->GetId(), theLayout.mSlider, inStyle, inForm, this->GetIndex(), inBorderThickness);
    this->mDecrementButton = new UIScrollButton(this->GetId(), theDecrementNormal, theDecrementPressed, inLocation, inForm, this->mSlider,
                                          1, this->GetIndex());
    this->mIncrementButton = new UIScrollButton(this->GetId(), theIncrementNormal, theIncrementPressed, theLayout.mIncrement,
                                          inForm, this->mSlider, 0, this->GetIndex());
}

// FUNCTION: LLE 0x1007cc8b
// Destroys the scroll bar with its slider and arrow buttons
UIScrollBar::~UIScrollBar()
{
    delete this->mSlider;
    delete this->mIncrementButton;
    delete this->mDecrementButton;
}

// FUNCTION: LLE 0x1007cccd
// Snapshots the bar, its arrow buttons and its slider; returns the slider's result
long UIScrollBar::CopyData()
{
    UIControl::CopyData();
    this->mIncrementButton->CopyData();
    this->mDecrementButton->CopyData();
    return this->mSlider->CopyData();
}

// FUNCTION: LLE 0x1007ccf8
// Draws the bar, the slider and the arrow buttons; returns the last result
long UIScrollBar::DrawPolygons()
{
    UIControl::DrawPolygons();
    this->mSlider->DrawPolygons();
    this->mIncrementButton->DrawPolygons();
    return this->mDecrementButton->DrawPolygons();
}

// FUNCTION: LLE 0x1007cd23
// Draws the text of the arrow buttons, the slider and the bar
long UIScrollBar::DrawTextA()
{
    this->mDecrementButton->DrawTextA();
    this->mIncrementButton->DrawTextA();
    this->mSlider->DrawTextA();
    return UIControl::DrawTextA();
}

// FUNCTION: LLE 0x1007cd51
// Moves the slider value by an amount
void UIScrollBar::IncrementBy(long inAmount)
{
    ee_ui_scroll_bar_increment_by(EE_UI_SCROLL_BAR_ARGS, inAmount);
}

// FUNCTION: LLE 0x1007cd6e
// Presses the part under the mouse (an arrow button, else the slider) and captures the mouse
long UIScrollBar::MouseLButtonDown()
{
    return ee_ui_scroll_bar_mouse_l_button_down(EE_UI_SCROLL_BAR_ARGS);
}

// FUNCTION: LLE 0x1007cdf4
// Releases the pressed part and the mouse capture
long UIScrollBar::MouseLButtonUp()
{
    return ee_ui_scroll_bar_mouse_l_button_up(EE_UI_SCROLL_BAR_ARGS);
}

// FUNCTION: LLE 0x1007ce5f
// Drags the slider when held or under the mouse, and repeats a held arrow button while the mouse stays on it
long UIScrollBar::MouseMove(const GE2DPoint& inLocation)
{
    return ee_ui_scroll_bar_mouse_move(EE_UI_SCROLL_BAR_ARGS, ScrollBarPoint(inLocation));
}

// FUNCTION: LLE 0x1007cf07
// Moves the scroll bar, laying out its arrow buttons and slider for its style
void UIScrollBar::SetLocation(const GE2DPoint& inLocation)
{
    ee_ui_scroll_bar_set_location(EE_UI_SCROLL_BAR_ARGS, ScrollBarPoint(inLocation));
}

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1007c901
// SYNTHETIC: LLE 0x1007c90b
// SYNTHETIC: LLE 0x1007c91b
// SYNTHETIC: LLE 0x1007c927
// SYNTHETIC: LLE 0x1007c931
// SYNTHETIC: LLE 0x1007c93b
// SYNTHETIC: LLE 0x1007c94b
// SYNTHETIC: LLE 0x1007c957
// SYNTHETIC: LLE 0x1007c961
// SYNTHETIC: LLE 0x1007c96b
// SYNTHETIC: LLE 0x1007c97b
// SYNTHETIC: LLE 0x1007c987
// SYNTHETIC: LLE 0x1007c991
// SYNTHETIC: LLE 0x1007c99b
// SYNTHETIC: LLE 0x1007c9ab
// SYNTHETIC: LLE 0x1007c9b7
// SYNTHETIC: LLE 0x1007c9c1
// SYNTHETIC: LLE 0x1007c9cb
// SYNTHETIC: LLE 0x1007c9db
// SYNTHETIC: LLE 0x1007c9e7
// SYNTHETIC: LLE 0x1007c9f1
// SYNTHETIC: LLE 0x1007c9fb
// SYNTHETIC: LLE 0x1007ca0b
// SYNTHETIC: LLE 0x1007ca17
