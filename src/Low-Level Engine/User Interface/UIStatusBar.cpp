// C:\EEX\Low-Level Engine\User Interface\UIStatusBar.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// The original unit: 0x1007db5b..0x1007e312 (the U2D key init chains, the ctor, the dtor, the out-of-line members);
// the header-inline members were emitted by UIControlsTable.cpp.
//
// UIStatusBar is whole in Rust (rust/crates/ee_ui/src/statusobj.rs, R6 ui round 10): the members below forward to
// the Rust object, which calls out through UIStatusBarHost::Op (UIControl's members, the two rhombi, the texts).
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
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/TSCriticalSection.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIStatusBar.h"
#include "UIObjHost.h"

struct UIStatusBarHost {
    static intptr_t Op(void* inBar, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t, intptr_t, float* f)
    {
        UIStatusBar* s = static_cast<UIStatusBar*>(inBar);
        GERhombus& theRhombus = a ? s->mCopiedStatusRhombus : s->mStatusRhombus;
        intptr_t r = 0;
        if (UIObjHost::Base(s, inOp, a, f, &r) || UIObjHost::Media(s, inOp, a, b, f, &r))
            return r;
        switch (inOp) {
        case EeUiOp::RhombusRect: UIObjHost::PutRect(f, theRhombus.GetRectangle()); break;
        case EeUiOp::RhombusSetRect: theRhombus.SetRectangle(UIObjHost::Rect(f)); break;
        case EeUiOp::RhombusCopy: s->mCopiedStatusRhombus = s->mStatusRhombus; break;
        case EeUiOp::RhombusSetOpacity: theRhombus.SetOpacity(f[0]); break;
        case EeUiOp::RhombusSetColor: theRhombus.SetColor(GEColor(f[0], f[1], f[2])); break;
        case EeUiOp::RhombusDraw: return s->GetRasterizer()->Draw(&theRhombus, s->GetScreenIndex());
        default: break;
        }
        return 0;
    }
    static intptr_t Call(const UIStatusBar* s, EeUiStatusBarCall c, intptr_t a = 0, intptr_t b = 0, float* f = NULL)
    {
        return ee_ui_status_bar_call(s->mRust, c, a, b, f);
    }
    static intptr_t CallF(const UIStatusBar* s, EeUiStatusBarCall c, float f0, float f1 = 0.0f, float f2 = 0.0f)
    {
        float f[4] = {f0, f1, f2, 0.0f};
        return ee_ui_status_bar_call(s->mRust, c, 0, 0, f);
    }
};
typedef EeUiStatusBarCall C;

// FUNCTION: LLE 0x1007dc7b
// Creates a progress bar control: empty range, black fill, white text and a text area inset by the border
UIStatusBar::UIStatusBar(unsigned long inId, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                         unsigned long inBorderThickness)
    : UIControl(kControlTypeStatusBar, inId, inRectangle, inForm, inIndex, inBorderThickness)
{
    this->mRust = ee_ui_status_bar_new(this, UIStatusBarHost::Op, inBorderThickness);
}

UIStatusBar::~UIStatusBar() { ee_ui_status_bar_free(this->mRust); }
void UIStatusBar::ChangeFont(GEFont* inFont) { UIStatusBarHost::Call(this, C::ChangeFont, (intptr_t)inFont); }
long UIStatusBar::CopyData() { return (long)UIStatusBarHost::Call(this, C::CopyData); }
long UIStatusBar::DrawPolygons() { return (long)UIStatusBarHost::Call(this, C::DrawPolygons); }
long UIStatusBar::DrawTextA() { return (long)UIStatusBarHost::Call(this, C::DrawText); }
long UIStatusBar::GetCurrentRangeValue() const { return (long)UIStatusBarHost::Call(this, C::CurrentRangeValue); }
GEText* UIStatusBar::GetGEText() { return reinterpret_cast<GEText*>(UIStatusBarHost::Call(this, C::Text)); }
long UIStatusBar::GetMaxRange() const { return (long)UIStatusBarHost::Call(this, C::MaxRange); }
unsigned long UIStatusBar::GetRange() const { return (unsigned long)UIStatusBarHost::Call(this, C::Range); }
void UIStatusBar::SetCurrentRangeValue(long inValue) { UIStatusBarHost::Call(this, C::SetCurrentRangeValue, inValue); }
void UIStatusBar::SetFocusedTextColor(const GEColor& inColor)
{
    UIStatusBarHost::CallF(this, C::SetFocusedTextColor, inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue());
}
void UIStatusBar::SetLocation(const GE2DPoint& inLocation)
{
    UIStatusBarHost::CallF(this, C::SetLocation, inLocation.GetX(), inLocation.GetY());
}
void UIStatusBar::SetRange(unsigned long inMin, unsigned long inMax) { UIStatusBarHost::Call(this, C::SetRange, inMin, inMax); }
void UIStatusBar::SetStatusBarOpacity(float inOpacity) { UIStatusBarHost::CallF(this, C::SetStatusBarOpacity, inOpacity); }
void UIStatusBar::SetStatusBarColor(const GEColor& inColor)
{
    UIStatusBarHost::CallF(this, C::SetStatusBarColor, inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue());
}
void UIStatusBar::SetText(const UWideString& inText, GEFont* inFont)
{
    UIStatusBarHost::Call(this, C::SetText, (intptr_t)&inText, (intptr_t)inFont);
}
