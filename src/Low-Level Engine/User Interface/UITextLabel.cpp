// C:\EEX\Low-Level Engine\User Interface\UITextLabel.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// The original unit: 0x1007e579..0x1007fb46 (the ctor, the out-of-line members in alphabetical order, the vector
// COMDATs); the header-inline members were emitted by UICheckBox.cpp (0x1005f538..0x1005f9da).
//
// UITextLabel is whole in Rust (rust/crates/ee_ui/src/labelobj.rs, R6 ui round 13): the members below forward to the
// Rust object, which calls out through UITextLabelRust::Op (UIControl's members, the label's own virtuals, the scroll
// bar, the texts and the strings it holds by address). The wrapping (wrap.rs), the geometry (label_fit.rs) and the
// draw places (draw.rs) are its algorithms.
#include <windows.h>
#include <stdlib.h>

#include "rust/ui/labelobj.h"
#include "rust/ui/obj.h"
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
#include "UIObjHost.h"

struct UITextLabelRust {
    static intptr_t Op(void* inLabel, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t, intptr_t, float* f)
    {
        UITextLabel* l = static_cast<UITextLabel*>(inLabel);
        UIScrollBar* theBar = reinterpret_cast<UIScrollBar*>(a);
        TMPoll* thePoll = reinterpret_cast<TMPoll*>(a);
        intptr_t r = 0;
        if (UIObjHost::Base(l, inOp, a, f, &r) || UIObjHost::Media(l, inOp, a, b, f, &r) ||
            UIObjHost::Part(reinterpret_cast<UIControl*>(a), inOp, f, &r))
            return r;
        switch (inOp) {
        case EeUiOp::StringNew: return (intptr_t) new UWideString(reinterpret_cast<const char*>(a));
        case EeUiOp::StringDelete: delete reinterpret_cast<UWideString*>(a); break;
        case EeUiOp::StringAssign: *reinterpret_cast<UWideString*>(a) = *reinterpret_cast<const UWideString*>(b); break;
        case EeUiOp::TextSetBytes: reinterpret_cast<GEText*>(a)->SetText(UWideString(reinterpret_cast<const char*>(b))); break;
        case EeUiOp::TextFontHeight: return reinterpret_cast<GEText*>(a)->GetFont()->GetHeight();
        case EeUiOp::DrawCountDecrement: l->UIControl::DrawTextA(); break;
        case EeUiOp::SetBgRectangle: l->SetBgRectangle(UIObjHost::Rect(f)); break;
        case EeUiOp::RedrawIntersect: l->GetForm()->RedrawIntersectControls(UIObjHost::Rect(f)); break;
        case EeUiOp::CreateScrollBar:
            return (intptr_t) new UIScrollBar(l->GetId(), GE2DPoint(f[0], f[1]), f[2], 1, l->GetForm(), l->GetIndex(),
                                              l->GetBorderThickness());
        case EeUiOp::BarSetRange: theBar->SetRange(0, (unsigned long)b); break;
        case EeUiOp::BarSetPageSize: theBar->SetPageSize((unsigned long)b); break;
        case EeUiOp::BarValue: return theBar->GetSlider()->GetCurrentRangeValue();
        case EeUiOp::BarIncrementBy: theBar->IncrementBy((long)b); break;
        case EeUiOp::BarSetValue: theBar->SetCurrentRangeValue((unsigned long)b); break;
        case EeUiOp::PartCopiedDrawRect: UIObjHost::PutRect(f, theBar->GetCopiedDrawRectangle()); break;
        case EeUiOp::PollCreate: return (intptr_t) new TMPoll((unsigned long)a, 0, false);
        case EeUiOp::PollDelete: delete thePoll; break;
        case EeUiOp::PollRunning: return thePoll->GetIsRunning();
        case EeUiOp::PollPoll: return thePoll->Poll();
        case EeUiOp::PollStart: thePoll->Start(); break;
        case EeUiOp::PollStop: thePoll->Stop(); break;
        case EeUiOp::PollPause: thePoll->Pause(); break;
        case EeUiOp::PollResume: thePoll->Resume(); break;
        case EeUiOp::PollSetDelay: thePoll->SetTimeDelay((unsigned long)b); break;
        case EeUiOp::VirtCheckScrollBar: l->CheckScrollBar(); break;
        case EeUiOp::VirtRecompute: l->RecomputeDisplayRange(); break;
        case EeUiOp::VirtSetLocation: l->SetLocation(GE2DPoint(f[0], f[1])); break;
        case EeUiOp::VirtSetRectangle: l->SetRectangle(UIObjHost::Rect(f)); break;
        default: break;
        }
        return 0;
    }
    static intptr_t Call(const UITextLabel* l, EeUiLabelCall c, intptr_t a = 0, intptr_t b = 0, float f0 = 0.0f,
                         float f1 = 0.0f, float f2 = 0.0f, float f3 = 0.0f)
    {
        float f[4] = {f0, f1, f2, f3};
        return ee_ui_label_call(l->mRust, c, a, b, f);
    }
    static intptr_t CallRect(const UITextLabel* l, EeUiLabelCall c, const GERectangle& r)
    {
        float f[4];
        UIObjHost::PutRect(f, r);
        return ee_ui_label_call(l->mRust, c, 0, 0, f);
    }
    static const GERectangle& RectAt(intptr_t inAddress) { return *reinterpret_cast<const GERectangle*>(inAddress); }
};
typedef EeUiLabelCall C;

// Creates a text label (type 0 one line, type 1 multi-line with a scroll bar area) and sets its initial text
UITextLabel::UITextLabel(const UWideString& inText, GEFont* inFont, unsigned long inId, const GERectangle& inRectangle,
                         UIForm* inForm, unsigned long inLabelType, unsigned long inIndex,
                         unsigned long inBorderThickness)
    : UIControl(kControlTypeTextLabel, inId, inRectangle, inForm, inIndex, inBorderThickness)
{
    this->SetBorderType(0);
    float f[4];
    UIObjHost::PutRect(f, inRectangle);
    this->mRust = ee_ui_label_new(this, UITextLabelRust::Op, f, inFont, inLabelType, inBorderThickness);
    UITextLabelRust::Call(this, C::Init, (intptr_t)(const char*)inText);
}

UITextLabel::~UITextLabel() { ee_ui_label_free(this->mRust); }
void UITextLabel::AppendText(const UWideString& inText) { this->SetText(inText, false); }
void UITextLabel::ChangeFont(GEFont* inFont) { UITextLabelRust::Call(this, C::ChangeFont, (intptr_t)inFont); }
void UITextLabel::CheckScrollBar() { UITextLabelRust::Call(this, C::CheckScrollBar); }
long UITextLabel::CopyData() { return (long)UITextLabelRust::Call(this, C::CopyData); }
void UITextLabel::DeleteArray() { UITextLabelRust::Call(this, C::DeleteArray); }
void UITextLabel::DeleteLine(unsigned long inLine) { UITextLabelRust::Call(this, C::DeleteLine, inLine); }
long UITextLabel::DrawPolygons() { return (long)UITextLabelRust::Call(this, C::DrawPolygons); }
long UITextLabel::DrawTextA() { return (long)UITextLabelRust::Call(this, C::DrawText); }
bool UITextLabel::GetFitRectToText() const { return UITextLabelRust::Call(this, C::FitRectToText) != 0; }
GEText* UITextLabel::GetGEText() const { return reinterpret_cast<GEText*>(UITextLabelRust::Call(this, C::Text)); }
unsigned long UITextLabel::GetLineCount() const { return (unsigned long)UITextLabelRust::Call(this, C::LineCount); }
unsigned long UITextLabel::GetNumberLinesDisplayed() const
{
    return (unsigned long)UITextLabelRust::Call(this, C::LinesDisplayed);
}
const UIScrollBar* UITextLabel::GetScrollBar() const
{
    return reinterpret_cast<const UIScrollBar*>(UITextLabelRust::Call(this, C::ScrollBar));
}
const UWideString& UITextLabel::GetText(unsigned long inLine) const
{
    return *reinterpret_cast<const UWideString*>(UITextLabelRust::Call(this, C::Line, inLine));
}
long UITextLabel::MouseLButtonDown() { return (long)UITextLabelRust::Call(this, C::MouseLButtonDown); }
long UITextLabel::MouseLButtonUp() { return (long)UITextLabelRust::Call(this, C::MouseLButtonUp); }
long UITextLabel::MouseMove(const GE2DPoint& inLocation)
{
    return (long)UITextLabelRust::Call(this, C::MouseMove, 0, 0, inLocation.GetX(), inLocation.GetY());
}
long UITextLabel::MouseWheelTurned(long inDelta) { return (long)UITextLabelRust::Call(this, C::MouseWheelTurned, inDelta); }
void UITextLabel::RecomputeDisplayRange() { UITextLabelRust::Call(this, C::RecomputeDisplayRange); }
bool UITextLabel::ScrollBarExists() const { return UITextLabelRust::Call(this, C::ScrollBarExists) != 0; }
void UITextLabel::SetAlignment(unsigned long inAlignment) { UITextLabelRust::Call(this, C::SetAlignment, inAlignment); }
void UITextLabel::SetFitRectToText(bool inFit) { UITextLabelRust::Call(this, C::SetFitRectToText, inFit); }
void UITextLabel::SetLocation(const GE2DPoint& inLocation)
{
    UITextLabelRust::Call(this, C::SetLocation, 0, 0, inLocation.GetX(), inLocation.GetY());
}
void UITextLabel::SetRectangle(const GERectangle& inRectangle) { UITextLabelRust::CallRect(this, C::SetRectangle, inRectangle); }
void UITextLabel::SetStartingDisplayLine(unsigned long inLine) { UITextLabelRust::Call(this, C::SetStartingDisplayLine, inLine); }
void UITextLabel::SetScrollBarBgOpacity(float inOpacity) { UITextLabelRust::Call(this, C::SetScrollBarBgOpacity, 0, 0, inOpacity); }
void UITextLabel::SetScrollBarBgColor(const GEColor& inColor)
{
    UITextLabelRust::Call(this, C::SetScrollBarBgColor, 0, 0, inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue());
}
void UITextLabel::SetText(const UWideString& inText, bool inReplace)
{
    UITextLabelRust::Call(this, C::SetText, (intptr_t)(const char*)inText, inReplace);
}
void UITextLabel::SetTextRectangle(const GERectangle& inRectangle) { UITextLabelRust::CallRect(this, C::SetTextRectangle, inRectangle); }
