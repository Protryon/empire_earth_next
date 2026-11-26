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

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/GEText.h"
#include "lle/TMPoll.h"
#include "lle/UITextLabel.h"

struct UITextLabelRust {
    static const GERectangle& RectAt(intptr_t inAddress) { return *reinterpret_cast<const GERectangle*>(inAddress); }
};
#define l static_cast<UITextLabel*>(inObj)
#define theBar reinterpret_cast<UIScrollBar*>(a)
#define thePoll reinterpret_cast<TMPoll*>(a)
EE_UI_CASE(label_string_new) { return (intptr_t) new UWideString(reinterpret_cast<const char*>(a)); }
EE_UI_VOID(label_string_delete) { delete reinterpret_cast<UWideString*>(a); }
EE_UI_VOID(label_string_assign) { *reinterpret_cast<UWideString*>(a) = *reinterpret_cast<const UWideString*>(b); }
EE_UI_VOID(label_text_set_bytes) { reinterpret_cast<GEText*>(a)->SetText(UWideString(reinterpret_cast<const char*>(b))); }
EE_UI_CASE(label_text_font_height) { return reinterpret_cast<GEText*>(a)->GetFont()->GetHeight(); }
EE_UI_VOID(label_draw_count_decrement) { l->UIControl::DrawTextA(); }
EE_UI_VOID(label_set_bg_rectangle) { l->SetBgRectangle(UIObjHost::Rect(f)); }
EE_UI_VOID(label_redraw_intersect) { l->GetForm()->RedrawIntersectControls(UIObjHost::Rect(f)); }
EE_UI_CASE(label_create_scroll_bar) { return (intptr_t) new UIScrollBar(l->GetId(), GE2DPoint(f[0], f[1]), f[2], 1, l->GetForm(), l->GetIndex(), l->GetBorderThickness()); }
EE_UI_VOID(label_bar_set_range) { theBar->SetRange(0, (unsigned long)b); }
EE_UI_VOID(label_bar_set_page_size) { theBar->SetPageSize((unsigned long)b); }
EE_UI_CASE(label_bar_value) { return theBar->GetSlider()->GetCurrentRangeValue(); }
EE_UI_VOID(label_bar_increment_by) { theBar->IncrementBy((long)b); }
EE_UI_VOID(label_bar_set_value) { theBar->SetCurrentRangeValue((unsigned long)b); }
EE_UI_VOID(label_part_copied_draw_rect) { UIObjHost::PutRect(f, theBar->GetCopiedDrawRectangle()); }
EE_UI_CASE(label_poll_create) { return (intptr_t) new TMPoll((unsigned long)a, 0, false); }
EE_UI_VOID(label_poll_delete) { delete thePoll; }
EE_UI_CASE(label_poll_running) { return thePoll->GetIsRunning(); }
EE_UI_CASE(label_poll_poll) { return thePoll->Poll(); }
EE_UI_VOID(label_poll_start) { thePoll->Start(); }
EE_UI_VOID(label_poll_stop) { thePoll->Stop(); }
EE_UI_VOID(label_poll_pause) { thePoll->Pause(); }
EE_UI_VOID(label_poll_resume) { thePoll->Resume(); }
EE_UI_VOID(label_poll_set_delay) { thePoll->SetTimeDelay((unsigned long)b); }
EE_UI_VOID(label_virt_check_scroll_bar) { l->CheckScrollBar(); }
EE_UI_VOID(label_virt_recompute) { l->RecomputeDisplayRange(); }
EE_UI_VOID(label_virt_set_location) { l->SetLocation(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(label_virt_set_rectangle) { l->SetRectangle(UIObjHost::Rect(f)); }
#undef thePoll
#undef theBar
#undef l

