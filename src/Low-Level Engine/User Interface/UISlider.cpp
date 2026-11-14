// C:\EEX\Low-Level Engine\User Interface\UISlider.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// The original unit: 0x1007d139..0x1007db5b (the U2D key init chains, the ctor, the out-of-line members); the
// header-inline members were emitted by UICheckBox.cpp (0x1005ef4b..0x1005f272).
//
// UISlider is whole in Rust (rust/crates/ee_ui/src/sliderobj.rs, R6 ui round 10): the members below forward to the
// Rust object, which calls out through UISliderHost::Op (UIControl's members, the virtuals ChangeRange, ButtonDown,
// ButtonUp and SetDirty, the thumb bitmap, the manager's drag bar).
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
#include "lle/TSCriticalSection.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UISlider.h"
#include "UIObjHost.h"

struct UISliderHost {
    static intptr_t Op(void* inSlider, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t, intptr_t, float* f)
    {
        UISlider* s = static_cast<UISlider*>(inSlider);
        UIManager* theManager = s->mForm->GetManager();
        intptr_t r = 0;
        if (UIObjHost::Base(s, inOp, a, f, &r) || UIObjHost::Media(s, inOp, a, b, f, &r))
            return r;
        switch (inOp) {
        case EeUiOp::SliderChangeRange: s->ChangeRange((unsigned long)a); break;
        case EeUiOp::ButtonDown: s->ButtonDown(); break;
        case EeUiOp::ButtonUp: s->ButtonUp(); break;
        case EeUiOp::ThumbCreate:
            return (intptr_t)theManager->CreateBitmapForScaledControl(theManager->GetDragBarBitmap(), UIObjHost::Rect(f));
        case EeUiOp::DragBarScale:
            f[0] = theManager->GetDragBarBitmap()->GetXScale();
            f[1] = theManager->GetDragBarBitmap()->GetYScale();
            break;
        default: break;
        }
        return 0;
    }
    static intptr_t Call(const UISlider* s, EeUiSliderCall c, intptr_t a = 0, intptr_t b = 0, float* f = NULL)
    {
        return ee_ui_slider_call(s->mRust, c, a, b, f);
    }
    static intptr_t CallAt(const UISlider* s, EeUiSliderCall c, const GE2DPoint& p)
    {
        float f[4] = {p.GetX(), p.GetY(), 0.0f, 0.0f};
        return ee_ui_slider_call(s->mRust, c, 0, 0, f);
    }
};
typedef EeUiSliderCall C;

// FUNCTION: LLE 0x1007d259
// Creates a horizontal (style 0) or vertical slider with a square thumb, an empty range and no page size
UISlider::UISlider(unsigned long inId, const GERectangle& inRectangle, unsigned long inSliderStyle, UIForm* inForm,
                   unsigned long inIndex, unsigned long inBorderThickness)
    : UIControl(kControlTypeSlider, inId, inRectangle, inForm, inIndex, inBorderThickness)
{
    this->mBorderType = 0;
    this->mRust = ee_ui_slider_new(this, UISliderHost::Op, inSliderStyle);
}

UISlider::~UISlider() { ee_ui_slider_free(this->mRust); }
void UISlider::ButtonUp() { UISliderHost::Call(this, C::ButtonUp); }
void UISlider::ChangeRange(unsigned long inValue) { UISliderHost::Call(this, C::ChangeRange, inValue); }
long UISlider::CopyData() { return (long)UISliderHost::Call(this, C::CopyData); }
void UISlider::DecrementByPageSize() { UISliderHost::Call(this, C::DecrementByPageSize); }
long UISlider::DrawPolygons() { return (long)UISliderHost::Call(this, C::DrawPolygons); }
long UISlider::GetCurrentRangeValue() const { return (long)UISliderHost::Call(this, C::CurrentRangeValue); }
long UISlider::GetMaxRange() const { return (long)UISliderHost::Call(this, C::MaxRange); }
unsigned long UISlider::GetSliderStyle() const { return (unsigned long)UISliderHost::Call(this, C::Style); }
void UISlider::IncrementByPageSize() { UISliderHost::Call(this, C::IncrementByPageSize); }
long UISlider::MouseLButtonDown() { return (long)UISliderHost::Call(this, C::MouseLButtonDown); }
long UISlider::MouseLButtonUp() { return (long)UISliderHost::Call(this, C::MouseLButtonUp); }
long UISlider::MouseMove(const GE2DPoint& inLocation) { return (long)UISliderHost::CallAt(this, C::MouseMove, inLocation); }
void UISlider::SetCurrentRangeValue(long inValue) { UISliderHost::Call(this, C::SetCurrentRangeValue, inValue); }
void UISlider::SetLocation(const GE2DPoint& inLocation) { UISliderHost::CallAt(this, C::SetLocation, inLocation); }
void UISlider::SetPageSize(unsigned long inPageSize) { UISliderHost::Call(this, C::SetPageSize, inPageSize); }
void UISlider::SetRange(unsigned long inMin, unsigned long inMax) { UISliderHost::Call(this, C::SetRange, inMin, inMax); }
