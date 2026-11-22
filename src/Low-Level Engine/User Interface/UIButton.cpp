// C:\EEX\Low-Level Engine\User Interface\UIButton.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1005e47c..0x1005ef4b: the U2D key init chains (U2DRectangle.h, via the rasterizer header), the ctor, the dtor
// and the out-of-line members in alphabetical order. The header-inline members are emitted first by
// UIBlinkingButton.cpp (it sorts before UIButton.cpp). UICheckBox.cpp's inline block starts at 0x1005ef4b.
//
// UIButton is whole in Rust (rust/crates/ee_ui/src/buttonobj.rs, R6 ui round 11): the members below forward to the
// Rust object, which calls out through ee_ui_button_host (UIControl's members, the virtuals ButtonDown, ButtonUp,
// MouseOver, MouseMove, SetDirty and the captures, the bitmaps, the texts, the sound manager). Its subclasses' records
// are the Rust button's too (R6 ui round 12: checkobj.rs, variantobj.rs); the host answers their ops (the label, the
// radio walk and Check/UnCheck, the polls, the slider) and the subclasses forward through UIButtonRust.h.
#include <windows.h>
#include <stddef.h>

#include "rust/ui/buttonobj.h"
#include "rust/ui/obj.h"

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
#include "lle/UIButtonRust.h"
#include "lle/UIObjHost.h"
#include "lle/UIShims.h"
#include "rust/ui/lle_hosts.h"

#define s static_cast<UIButton*>(inObj)
EE_UI_VOID(button_down) { s->ButtonDown(); }
EE_UI_VOID(button_up) { s->ButtonUp(); }
EE_UI_VOID(button_mouse_over) { s->MouseOver(); }
EE_UI_VOID(button_mouse_move) { s->MouseMove(GE2DPoint(f[0], f[1])); }
EE_UI_CASE(button_sound_load) { return (intptr_t)s->GetSoundManager()->Load(*reinterpret_cast<const UString*>(a), 0, 1); }
EE_UI_VOID(button_sound_unload) { s->GetSoundManager()->Unload((unsigned long)a); }
EE_UI_VOID(button_sound_play) { s->GetSoundManager()->Play((unsigned long)a, 1.0f, 0.0f, 1.0f, false, NULL); }
EE_UI_CASE(button_label_create) { return (intptr_t) new UITextLabel(*reinterpret_cast<const UWideString*>(a), reinterpret_cast<GEFont*>(b), s->GetId(), UIObjHost::Rect(f), s->GetForm(), 0, s->GetIndex(), 0); }
EE_UI_VOID(button_radio_check) { static_cast<UIRadioButton*>(s)->Check(); }
EE_UI_VOID(button_radio_un_check_at) { static_cast<UIRadioButton*>(s->GetForm()->GetControl((unsigned long)a))->UnCheck(); }
EE_UI_CASE(button_radio_of)
{
    UIControl* theControl = s->GetForm()->GetControl((unsigned long)a);
    return theControl && theControl->GetType() == kControlTypeRadioButton ? (intptr_t) static_cast<UIButton*>(theControl)->mRust : 0;
}
EE_UI_CASE(button_poll_create) { return (intptr_t) new TMPoll((unsigned long)a, 0, false); }
EE_UI_VOID(button_poll_delete) { delete reinterpret_cast<TMPoll*>(a); }
EE_UI_CASE(button_poll_running) { return reinterpret_cast<TMPoll*>(a)->GetIsRunning(); }
EE_UI_CASE(button_poll_poll) { return (intptr_t)reinterpret_cast<TMPoll*>(a)->Poll(); }
EE_UI_VOID(button_poll_start) { reinterpret_cast<TMPoll*>(a)->Start(); }
EE_UI_VOID(button_poll_stop) { reinterpret_cast<TMPoll*>(a)->Stop(); }
EE_UI_CASE(button_poll_time_left) { return (intptr_t)reinterpret_cast<TMPoll*>(a)->GetTimeLeft(); }
EE_UI_VOID(button_poll_set_delay) { reinterpret_cast<TMPoll*>(a)->SetTimeDelay((unsigned long)b); }
EE_UI_CASE(button_slider_value) { return reinterpret_cast<UISlider*>(a)->GetCurrentRangeValue(); }
EE_UI_VOID(button_slider_set_value) { reinterpret_cast<UISlider*>(a)->SetCurrentRangeValue((long)b); }
EE_UI_VOID(button_bitmap_stretch) { reinterpret_cast<GEBitmap*>(a)->ComputeStretchedCoordsAndScale(UIObjHost::Rect(f), UIObjHost::Rect(f), UIObjHost::Rect(f + 4), s->GetScreenIndex()); }
EE_UI_VOID(button_label_set_text) { reinterpret_cast<UITextLabel*>(a)->SetText(*reinterpret_cast<const UWideString*>(b)); }
#undef s

// Creates a button with normal, focused and pressed bitmaps (pressed defaults to normal) and no text or sounds
UIButton::UIButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap,
                   GEBitmap* inPressedBitmap, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                   unsigned long inBorderThickness)
    : UIControl(kControlTypeButton, inId, inRectangle, inForm, inIndex, inBorderThickness)
{
    this->SetBorderType(1);
    this->mRust = ee_ui_button_new(this, ee_ui_button_host, inNormalBitmap, inFocusedBitmap, inPressedBitmap);
}

UIButton::~UIButton() { ee_ui_button_free(this->mRust); }

// The header's members (the original's inline ones, emitted by UIBlinkingButton.cpp)
GE2DPoint UIButton::GetBitmapLocation() const
{
    float f[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    this->Call(EeUiButtonCall::BitmapLocation, 0, 0, f);
    return GE2DPoint(f[0], f[1]);
}
