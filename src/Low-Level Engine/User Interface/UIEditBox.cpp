// C:\EEX\Low-Level Engine\User Interface\UIEditBox.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10069d6b..0x1006de9d: the U2D key init chains (U2DRectangle.h, via the rasterizer header), the ctor, the dtor
// and the out-of-line members in alphabetical order. The header-inline members, the implicit copy members and ??_E
// were emitted first by UIComboBox.cpp. UIEMouseWheelScroll.cpp starts at 0x1006de9d.
//
// UIEditBox is whole in Rust (rust/crates/ee_ui/src/editobj.rs, R6 ui round 14): the members below forward to the
// Rust object, which calls out through UIEditBoxRust::Op (UIControl's members, the box's virtuals, the keys, the
// clipboard, the scroll bar, the texts, lines, rhombus and polls it holds by address). The text model (edit.rs), the
// key dispatch (edit_keys.rs) and the drawing's ranges (edit_draw.rs) are its algorithms.
#include <windows.h>
#include <stdlib.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEScreen.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/TMPoll.h"
#include "lle/Input System.h"
#include "lle/UIControl.h"
#include "lle/UIScrollBar.h"
#include "lle/UIEditBox.h"

#define e static_cast<UIEditBox*>(inObj)
#define theBar reinterpret_cast<UIScrollBar*>(a)
#define thePoll reinterpret_cast<TMPoll*>(a)
#define theLine reinterpret_cast<GELine*>(a)
#define theRhombus reinterpret_cast<GERhombus*>(a)
#define theText reinterpret_cast<GEText*>(a)
EE_UI_VOID(edit_button_down) { e->ButtonDown(); }
EE_UI_CASE(edit_poll_create) { return (intptr_t) new TMPoll((unsigned long)a, 0, false); }
EE_UI_VOID(edit_poll_delete) { delete thePoll; }
EE_UI_CASE(edit_poll_running) { return thePoll->GetIsRunning(); }
EE_UI_CASE(edit_poll_poll) { return thePoll->Poll(); }
EE_UI_VOID(edit_poll_start) { thePoll->Start(); }
EE_UI_VOID(edit_poll_stop) { thePoll->Stop(); }
EE_UI_VOID(edit_text_set_bytes) { theText->SetText(UWideString(reinterpret_cast<const char*>(b))); }
EE_UI_CASE(edit_text_font_height) { return theText->GetFont()->GetHeight(); }
EE_UI_VOID(edit_draw_count_decrement) { e->UIControl::DrawTextA(); }
EE_UI_VOID(edit_bar_set_range) { theBar->SetRange(0, (unsigned long)b); }
EE_UI_VOID(edit_bar_set_page_size) { theBar->SetPageSize((unsigned long)b); }
EE_UI_CASE(edit_bar_value) { return theBar->GetSlider()->GetCurrentRangeValue(); }
EE_UI_VOID(edit_bar_set_value) { theBar->SetCurrentRangeValue((unsigned long)b); }
EE_UI_CASE(edit_bar_max) { return theBar->GetSlider()->GetMaxRange(); }
EE_UI_VOID(edit_virt_set_rectangle) { e->SetRectangle(UIObjHost::Rect(f)); }
EE_UI_VOID(edit_virt_process_enter_key) { e->ProcessEnterKey(); }
EE_UI_VOID(edit_clipboard_set) { PlatClipboardSetText(reinterpret_cast<const char*>(a)); }
EE_UI_VOID(edit_translated_key) { e->GetForm()->GetManager()->UpdateTranslatedKey(reinterpret_cast<const char*>(a), 1); }
EE_UI_VOID(edit_text_input_cancel) { PlatTextInputCancel(e->GetRasterizer()->GetWindow(e->GetScreenIndex())->GetOSRepresentation()); }
EE_UI_VOID(edit_base_release_keyboard) { e->UIControl::ReleaseCapturedKeyboard(); }
EE_UI_CASE(edit_base_mouse_double_pressed) { return e->UIControl::MouseLButtonDoublePressed(); }
EE_UI_VOID(edit_line_delete) { delete theLine; }
EE_UI_CASE(edit_rhombus_create) { return (intptr_t) new GERhombus(); }
EE_UI_VOID(edit_rhombus_delete) { delete theRhombus; }
EE_UI_VOID(edit_text_set_bg_color) { theText->SetBgColor(GEColor(f[0], f[1], f[2])); }
EE_UI_VOID(edit_text_set_transparent) { theText->SetIsTransparent(b != 0); }
#undef theText
#undef theRhombus
#undef theLine
#undef thePoll
#undef theBar
#undef e

// the keyboard's A, L and Return keys, the scroll bar, the form's mouse, the lines and the text colour: Rust's (ee_ui
// lle_bg_edit.rs, UI lane round 12) before the host above
#define UIEB(e) static_cast<UIEditBox*>(reinterpret_cast<UIControl*>(e))
extern "C" void ee_uicpp_keyboard_entity(uint32_t inKey, uintptr_t* inOut)
{ gISKeyboard->GetEntity(inKey, *reinterpret_cast<ISEntity**>(inOut)); }
extern "C" uintptr_t ee_uicpp_edit_scroll_bar_new(uintptr_t inEdit, float inX, float inY, float inLength)
{ return (uintptr_t) new UIScrollBar(UIEB(inEdit)->GetId(), GE2DPoint(inX, inY), inLength, 1, UIEB(inEdit)->GetForm(), UIEB(inEdit)->GetIndex(), UIEB(inEdit)->GetBorderThickness()); }
extern "C" float ee_uicpp_edit_form_mouse_x(uintptr_t inEdit) { return UIEB(inEdit)->GetForm()->GetMouseLocation().GetX(); }
extern "C" float ee_uicpp_edit_form_mouse_y(uintptr_t inEdit) { return UIEB(inEdit)->GetForm()->GetMouseLocation().GetY(); }
extern "C" uintptr_t ee_uicpp_line_new_black()
{ return (uintptr_t) new GELine(GE2DPoint(0.0f, 0.0f), GE2DPoint(0.0f, 0.0f), GEColor(0.0f, 0.0f, 0.0f), GEColor(0.0f, 0.0f, 0.0f), 1.0f); }
extern "C" uintptr_t ee_uicpp_line_new() { return (uintptr_t) new GELine(); }
extern "C" float ee_uicpp_text_fg_red(uintptr_t inText) { return ((GEText*)inText)->GetFgColor().GetRed(); }
extern "C" float ee_uicpp_text_fg_green(uintptr_t inText) { return ((GEText*)inText)->GetFgColor().GetGreen(); }
extern "C" float ee_uicpp_text_fg_blue(uintptr_t inText) { return ((GEText*)inText)->GetFgColor().GetBlue(); }


