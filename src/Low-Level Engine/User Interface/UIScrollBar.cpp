// C:\EEX\Low-Level Engine\User Interface\UIScrollBar.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// The original unit: 0x1007c901..0x1007d013 (the U2D key init chains, the ctor, the out-of-line members); the
// header-inline members were emitted by UICheckBox.cpp (0x1005f3b0..0x1005f538).
//
// UIScrollBar is whole in Rust (rust/crates/ee_ui/src/scrollbarobj.rs, R6 ui round 10): the members below forward to
// the Rust object, which calls out through UIScrollBarRust::Op (UIControl's members, the parts' virtuals, their new
// and delete, the manager's arrow bitmaps).
#include <windows.h>

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
#include "UIObjHost.h"

struct UIScrollBarRust {
    static intptr_t Op(void* inBar, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f)
    {
        UIScrollBar* s = static_cast<UIScrollBar*>(inBar);
        UISlider* theSlider = reinterpret_cast<UISlider*>(a);
        intptr_t r = 0;
        if (UIObjHost::Base(s, inOp, a, f, &r) || UIObjHost::Part(reinterpret_cast<UIControl*>(a), inOp, f, &r))
            return r;
        switch (inOp) {
        case EeUiOp::CreateSlider:
            return (intptr_t) new UISlider(s->GetId(), UIObjHost::Rect(f), (unsigned long)a, s->GetForm(), s->GetIndex(),
                                           (unsigned long)b);
        case EeUiOp::CreateButton:
            return (intptr_t) new UIScrollButton(s->GetId(), reinterpret_cast<GEBitmap*>(a), reinterpret_cast<GEBitmap*>(b),
                                                 GE2DPoint(f[0], f[1]), s->GetForm(), reinterpret_cast<UISlider*>(c),
                                                 (unsigned long)d, s->GetIndex());
        case EeUiOp::PartButtonDown: reinterpret_cast<UIScrollButton*>(a)->ButtonDown(); break;
        case EeUiOp::SliderStyle: return theSlider->GetSliderStyle();
        case EeUiOp::SliderValue: return theSlider->GetCurrentRangeValue();
        case EeUiOp::SliderSetValue: theSlider->SetCurrentRangeValue((long)b); break;
        case EeUiOp::SliderDecrementPage: theSlider->DecrementByPageSize(); break;
        case EeUiOp::SliderIncrementPage: theSlider->IncrementByPageSize(); break;
        case EeUiOp::SliderSetPageSize: theSlider->SetPageSize((unsigned long)b); break;
        case EeUiOp::SliderSetRange: theSlider->SetRange((unsigned long)b, (unsigned long)c); break;
        default: break;
        }
        return 0;
    }
    static intptr_t Call(const UIScrollBar* s, EeUiScrollBarCall c, intptr_t a = 0, intptr_t b = 0, float* f = NULL)
    {
        return ee_ui_scroll_bar_call(s->mRust, c, a, b, f);
    }
    static intptr_t CallF(const UIScrollBar* s, EeUiScrollBarCall c, float f0, float f1 = 0.0f, float f2 = 0.0f)
    {
        float f[4] = {f0, f1, f2, 0.0f};
        return ee_ui_scroll_bar_call(s->mRust, c, 0, 0, f);
    }
    // the constructor's rectangle, before the Rust object exists
    static GERectangle Layout(const GE2DPoint& inLocation, float inLength, unsigned long inStyle)
    {
        EeUiRect r = ee_ui_scroll_bar_layout(EeUiPoint{inLocation.GetX(), inLocation.GetY()}, inLength, inStyle).rect;
        return GERectangle(r.left, r.top, r.right, r.bottom);
    }
};
typedef EeUiScrollBarCall C;

// FUNCTION: LLE 0x1007ca21
// Creates a vertical (style 1) or horizontal scroll bar: a slider between two arrow buttons
// Fixed: the rectangle's size was `x + (style == 1) ? 19 : length - 1` (a precedence slip), which made it
// 19 x (length - 1) for both styles; a horizontal bar is now (length - 1) x 19
UIScrollBar::UIScrollBar(unsigned long inId, const GE2DPoint& inLocation, float inLength, unsigned long inStyle,
                         UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
    : UIControl(kControlTypeScrollBar, inId, UIScrollBarRust::Layout(inLocation, inLength, inStyle), inForm, inIndex, 0)
{
    float f[4] = {inLocation.GetX(), inLocation.GetY(), inLength, 0.0f};
    this->mRust = ee_ui_scroll_bar_new(this, UIScrollBarRust::Op, f, inStyle, inBorderThickness);
}

UIScrollBar::~UIScrollBar() { ee_ui_scroll_bar_free(this->mRust); }
long UIScrollBar::CopyData() { return (long)UIScrollBarRust::Call(this, C::CopyData); }
long UIScrollBar::DrawPolygons() { return (long)UIScrollBarRust::Call(this, C::DrawPolygons); }
long UIScrollBar::DrawTextA() { return (long)UIScrollBarRust::Call(this, C::DrawText); }
const UISlider* UIScrollBar::GetSlider() const { return reinterpret_cast<const UISlider*>(UIScrollBarRust::Call(this, C::Slider)); }
void UIScrollBar::IncrementBy(long inAmount) { UIScrollBarRust::Call(this, C::IncrementBy, inAmount); }
long UIScrollBar::MouseLButtonDown() { return (long)UIScrollBarRust::Call(this, C::MouseLButtonDown); }
long UIScrollBar::MouseLButtonUp() { return (long)UIScrollBarRust::Call(this, C::MouseLButtonUp); }
long UIScrollBar::MouseMove(const GE2DPoint& inLocation)
{
    return (long)UIScrollBarRust::CallF(this, C::MouseMove, inLocation.GetX(), inLocation.GetY());
}
void UIScrollBar::SetCurrentRangeValue(unsigned long inValue) { UIScrollBarRust::Call(this, C::SetCurrentRangeValue, inValue); }
void UIScrollBar::SetBgColor(const GEColor& inColor)
{
    UIScrollBarRust::CallF(this, C::SetBgColor, inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue());
}
void UIScrollBar::SetBgOpacity(float inOpacity) { UIScrollBarRust::CallF(this, C::SetBgOpacity, inOpacity); }
void UIScrollBar::SetLocation(const GE2DPoint& inLocation)
{
    UIScrollBarRust::CallF(this, C::SetLocation, inLocation.GetX(), inLocation.GetY());
}
void UIScrollBar::SetPageSize(unsigned long inPageSize) { UIScrollBarRust::Call(this, C::SetPageSize, inPageSize); }
void UIScrollBar::SetRange(unsigned long inMin, unsigned long inMax) { UIScrollBarRust::Call(this, C::SetRange, inMin, inMax); }
