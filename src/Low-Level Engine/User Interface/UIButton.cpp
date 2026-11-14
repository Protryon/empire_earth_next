// C:\EEX\Low-Level Engine\User Interface\UIButton.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1005e47c..0x1005ef4b: the U2D key init chains (U2DRectangle.h, via the rasterizer header), the ctor, the dtor
// and the out-of-line members in alphabetical order. The header-inline members are emitted first by
// UIBlinkingButton.cpp (it sorts before UIButton.cpp). UICheckBox.cpp's inline block starts at 0x1005ef4b.
//
// UIButton is whole in Rust (rust/crates/ee_ui/src/buttonobj.rs, R6 ui round 11): the members below forward to the
// Rust object, which calls out through UIButtonHost::Op (UIControl's members, the virtuals ButtonDown, ButtonUp,
// MouseOver, MouseMove, SetDirty and the captures, the bitmaps, the texts, the sound manager). Its subclasses' records
// are the Rust button's too (R6 ui round 12: checkobj.rs, variantobj.rs); the host answers their ops (the label, the
// radio walk and Check/UnCheck, the polls, the slider) and the subclasses forward through UIButtonRust.h.
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
#include "lle/GEText.h"
#include "lle/SESoundManager.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIButton.h"
#include "lle/UITextLabel.h"
#include "lle/UIRadioButton.h"
#include "lle/TMPoll.h"
#include "lle/UISlider.h"
#include "UIButtonRust.h"
#include "UIObjHost.h"

struct UIButtonHost {
    static intptr_t Op(void* inButton, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f)
    {
        UIButton* s = static_cast<UIButton*>(inButton);
        intptr_t r = 0;
        if (UIObjHost::Base(s, inOp, a, f, &r) || UIObjHost::Media(s, inOp, a, b, f, &r) ||
            UIObjHost::Part(reinterpret_cast<UIControl*>(a), inOp, f, &r))
            return r;
        switch (inOp) {
        case EeUiOp::ButtonDown: s->ButtonDown(); break;
        case EeUiOp::ButtonUp: s->ButtonUp(); break;
        case EeUiOp::ButtonMouseOver: s->MouseOver(); break;
        case EeUiOp::ButtonMouseMove: s->MouseMove(GE2DPoint(f[0], f[1])); break;
        case EeUiOp::SoundLoad: return (intptr_t)s->GetSoundManager()->Load(*reinterpret_cast<const UString*>(a), 0, 1);
        case EeUiOp::SoundUnload: s->GetSoundManager()->Unload((unsigned long)a); break;
        case EeUiOp::SoundPlay: s->GetSoundManager()->Play((unsigned long)a, 1.0f, 0.0f, 1.0f, false, NULL); break;
        case EeUiOp::LabelCreate:
            return (intptr_t) new UITextLabel(*reinterpret_cast<const UWideString*>(a), reinterpret_cast<GEFont*>(b),
                                              s->GetId(), UIObjHost::Rect(f), s->GetForm(), 0, s->GetIndex(), 0);
        case EeUiOp::RadioCheck: static_cast<UIRadioButton*>(s)->Check(); break;
        case EeUiOp::RadioUnCheckAt: static_cast<UIRadioButton*>(s->GetForm()->GetControl((unsigned long)a))->UnCheck(); break;
        case EeUiOp::RadioOf: {
            UIControl* c = s->GetForm()->GetControl((unsigned long)a);
            return c && c->GetType() == kControlTypeRadioButton ? (intptr_t) static_cast<UIButton*>(c)->mRust : 0;
        }
        case EeUiOp::PollCreate: return (intptr_t) new TMPoll((unsigned long)a, 0, false);
        case EeUiOp::PollDelete: delete reinterpret_cast<TMPoll*>(a); break;
        case EeUiOp::PollRunning: return reinterpret_cast<TMPoll*>(a)->GetIsRunning();
        case EeUiOp::PollPoll: return (intptr_t)reinterpret_cast<TMPoll*>(a)->Poll();
        case EeUiOp::PollStart: reinterpret_cast<TMPoll*>(a)->Start(); break;
        case EeUiOp::PollStop: reinterpret_cast<TMPoll*>(a)->Stop(); break;
        case EeUiOp::PollTimeLeft: return (intptr_t)reinterpret_cast<TMPoll*>(a)->GetTimeLeft();
        case EeUiOp::PollSetDelay: reinterpret_cast<TMPoll*>(a)->SetTimeDelay((unsigned long)b); break;
        case EeUiOp::SliderValue: return reinterpret_cast<UISlider*>(a)->GetCurrentRangeValue();
        case EeUiOp::SliderSetValue: reinterpret_cast<UISlider*>(a)->SetCurrentRangeValue((long)b); break;
        case EeUiOp::BitmapStretch: {
            // the source rectangle's floats, two per integer (variantobj.rs)
            uint32_t theBits[4] = {(uint32_t)((uint64_t)c >> 32), (uint32_t)c, (uint32_t)((uint64_t)d >> 32), (uint32_t)d};
            float theSource[4];
            memcpy(theSource, theBits, sizeof(theSource));
            GERectangle theDestination = UIObjHost::Rect(f);
            reinterpret_cast<GEBitmap*>(a)->ComputeStretchedCoordsAndScale(theDestination, theDestination,
                                                                           UIObjHost::Rect(theSource), s->GetScreenIndex());
            break;
        }
        case EeUiOp::LabelSetText:
            reinterpret_cast<UITextLabel*>(a)->SetText(*reinterpret_cast<const UWideString*>(b));   // SetText(text, true)
            break;
        default: break;
        }
        return 0;
    }
    static intptr_t Call(const UIButton* s, EeUiButtonCall c, intptr_t a = 0, intptr_t b = 0, float* f = NULL)
    {
        return ee_ui_button_call(s->mRust, c, a, b, f);
    }
    static intptr_t CallF(const UIButton* s, EeUiButtonCall c, float f0, float f1, float f2 = 0.0f, float f3 = 0.0f)
    {
        float f[4] = {f0, f1, f2, f3};
        return ee_ui_button_call(s->mRust, c, 0, 0, f);
    }
};
typedef EeUiButtonCall C;
#define EE_UI_BUTTON(c, ...) UIButtonHost::Call(this, C::c, ##__VA_ARGS__)

// FUNCTION: LLE 0x1005e59c
// Creates a button with normal, focused and pressed bitmaps (pressed defaults to normal) and no text or sounds
UIButton::UIButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap,
                   GEBitmap* inPressedBitmap, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                   unsigned long inBorderThickness)
    : UIControl(kControlTypeButton, inId, inRectangle, inForm, inIndex, inBorderThickness)
{
    this->mBorderType = 1;
    this->mRust = ee_ui_button_new(this, UIButtonHost::Op, inNormalBitmap, inFocusedBitmap, inPressedBitmap);
}

// FUNCTION: LLE 0x1005e70b
UIButton::~UIButton() { ee_ui_button_free(this->mRust); }
// FUNCTION: LLE 0x1005e7bf
void UIButton::ButtonDown() { EE_UI_BUTTON(ButtonDown); }
// FUNCTION: LLE 0x1005e832
void UIButton::ButtonUp() { EE_UI_BUTTON(ButtonUp); }   // Fixed (kept): plays the button-up sound
// FUNCTION: LLE 0x1005e8b4
void UIButton::ChangeFont(GEFont* inFont) { EE_UI_BUTTON(ChangeFont, (intptr_t)inFont); }
// FUNCTION: LLE 0x1005e8dd
long UIButton::CopyData() { return (long)EE_UI_BUTTON(CopyData); }
// FUNCTION: LLE 0x1005eb30
long UIButton::DrawPolygons() { return (long)EE_UI_BUTTON(DrawPolygons); }
// FUNCTION: LLE 0x1005eb80
long UIButton::DrawTextA() { return (long)EE_UI_BUTTON(DrawText); }
// FUNCTION: LLE 0x1005ebcf
long UIButton::MouseLButtonDown() { return (long)EE_UI_BUTTON(MouseLButtonDown); }
// FUNCTION: LLE 0x1005ebf8
long UIButton::MouseLButtonUp() { return (long)EE_UI_BUTTON(MouseLButtonUp); }
// FUNCTION: LLE 0x1005ec54
long UIButton::MouseMove(const GE2DPoint& inLocation)
{
    return (long)UIButtonHost::CallF(this, C::MouseMove, inLocation.GetX(), inLocation.GetY());
}
// FUNCTION: LLE 0x1005ece9
void UIButton::MouseOver() { EE_UI_BUTTON(MouseOver); }
// FUNCTION: LLE 0x1005ed04
void UIButton::SetButtonDownSound(const UString& inName) { EE_UI_BUTTON(SetButtonDownSound, (intptr_t)&inName); }
// FUNCTION: LLE 0x1005ed3f
void UIButton::SetButtonUpSound(const UString& inName) { EE_UI_BUTTON(SetButtonUpSound, (intptr_t)&inName); }
// FUNCTION: LLE 0x1005ed7a
void UIButton::SetLocation(const GE2DPoint& inLocation)
{
    UIButtonHost::CallF(this, C::SetLocation, inLocation.GetX(), inLocation.GetY());
}
// FUNCTION: LLE 0x1005edf1
void UIButton::SetText(const UWideString& inText, GEFont* inFont)
{
    EE_UI_BUTTON(SetText, (intptr_t)&inText, (intptr_t)inFont);
}
// FUNCTION: LLE 0x1005ee67
void UIButton::TabbedTo() { EE_UI_BUTTON(TabbedTo); }
// FUNCTION: LLE 0x1005ee77
void UIButton::TabbedFrom() { EE_UI_BUTTON(TabbedFrom); }
// FUNCTION: LLE 0x1005eea6
long UIButton::UpdateKeyboard() { return (long)EE_UI_BUTTON(UpdateKeyboard); }   // Fixed (kept): a key press on it

// The header's members (the original's inline ones, emitted by UIBlinkingButton.cpp)
unsigned long UIButton::GetButtonDownSound() const { return (unsigned long)EE_UI_BUTTON(ButtonDownSound); }
unsigned long UIButton::GetButtonUpSound() const { return (unsigned long)EE_UI_BUTTON(ButtonUpSound); }
GEBitmap* UIButton::GetCurrentBitmap() const { return (GEBitmap*)EE_UI_BUTTON(CurrentBitmap); }
GEBitmap* UIButton::GetCopiedBitmap() const { return (GEBitmap*)EE_UI_BUTTON(CopiedBitmap); }
GE2DPoint UIButton::GetBitmapLocation() const
{
    float f[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    EE_UI_BUTTON(BitmapLocation, 0, 0, f);
    return GE2DPoint(f[0], f[1]);
}
unsigned long UIButton::GetState() const { return (unsigned long)EE_UI_BUTTON(State); }
GEBitmap* UIButton::GetFocusedBitmap() const { return (GEBitmap*)EE_UI_BUTTON(FocusedBitmap); }
GEText* UIButton::GetGEText() { return (GEText*)EE_UI_BUTTON(Text); }
bool UIButton::GetIsButtonDown() const { return EE_UI_BUTTON(IsButtonDown) != 0; }
GEBitmap* UIButton::GetNormalBitmap() const { return (GEBitmap*)EE_UI_BUTTON(NormalBitmap); }
GEBitmap* UIButton::GetPressedBitmap() const { return (GEBitmap*)EE_UI_BUTTON(PressedBitmap); }
void UIButton::SetBitmapDrawOffset(const GE2DPoint& inOffset)
{
    UIButtonHost::CallF(this, C::SetBitmapDrawOffset, inOffset.GetX(), inOffset.GetY());
}
void UIButton::SetFocusedBitmap(GEBitmap* inBitmap) { EE_UI_BUTTON(SetFocusedBitmap, (intptr_t)inBitmap); }
void UIButton::SetFocusedTextColor(const GEColor& inColor)
{
    UIButtonHost::CallF(this, C::SetFocusedTextColor, inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue());
}
void UIButton::SetNormalBitmap(GEBitmap* inBitmap) { EE_UI_BUTTON(SetNormalBitmap, (intptr_t)inBitmap); }
void UIButton::SetPressedBitmap(GEBitmap* inBitmap) { EE_UI_BUTTON(SetPressedBitmap, (intptr_t)inBitmap); }
void UIButton::SetTextRectangle(const GERectangle& inRectangle)
{
    UIButtonHost::CallF(this, C::SetTextRectangle, inRectangle.mBottom, inRectangle.mLeft, inRectangle.mRight,
                        inRectangle.mTop);
}
void UIButton::ReplaceNormalBitmap(GEBitmap* inBitmap) { EE_UI_BUTTON(ReplaceNormalBitmap, (intptr_t)inBitmap); }
void UIButton::ReplacePressedBitmap(GEBitmap* inBitmap) { EE_UI_BUTTON(ReplacePressedBitmap, (intptr_t)inBitmap); }

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1005e47c
// SYNTHETIC: LLE 0x1005e486
// SYNTHETIC: LLE 0x1005e496
// SYNTHETIC: LLE 0x1005e4a2
// SYNTHETIC: LLE 0x1005e4ac
// SYNTHETIC: LLE 0x1005e4b6
// SYNTHETIC: LLE 0x1005e4c6
// SYNTHETIC: LLE 0x1005e4d2
// SYNTHETIC: LLE 0x1005e4dc
// SYNTHETIC: LLE 0x1005e4e6
// SYNTHETIC: LLE 0x1005e4f6
// SYNTHETIC: LLE 0x1005e502
// SYNTHETIC: LLE 0x1005e50c
// SYNTHETIC: LLE 0x1005e516
// SYNTHETIC: LLE 0x1005e526
// SYNTHETIC: LLE 0x1005e532
// SYNTHETIC: LLE 0x1005e53c
// SYNTHETIC: LLE 0x1005e546
// SYNTHETIC: LLE 0x1005e556
// SYNTHETIC: LLE 0x1005e562
// SYNTHETIC: LLE 0x1005e56c
// SYNTHETIC: LLE 0x1005e576
// SYNTHETIC: LLE 0x1005e586
// SYNTHETIC: LLE 0x1005e592
