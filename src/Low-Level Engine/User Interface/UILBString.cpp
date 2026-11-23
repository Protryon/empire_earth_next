// C:\EEX\Low-Level Engine\User Interface\UILBString.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10078251..0x100785cd: the U2D key init chains, then DrawTextA and UpdateTranslatedKey (both Rust's: ee_ui
// listobj.rs); here the list boxes' shims. The class's inline members and every UIListBox<UWideString> member are
// emitted first by UIComboBox.cpp. UILineGraph.cpp's key chains start at 0x100785cd.
#include <windows.h>
#include <mbstring.h>

#include "rust/ui/draw_ffi.h"
#include "rust/ui/obj.h"
#include "rust/ui/rect.h"

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
#include "lle/GELine.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/TMPoll.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIObjHost.h"
#include "lle/UIShims.h"

// The Rust list box's ops that do not depend on T (ee_ui listobj.rs; UIListBoxRust<T>::Op answers the rest):
// UIControl's, the scroll bar's (a part by address), the polls', the selection rhombi's and the focus line's
#define lst static_cast<UIControl*>(inObj)
#define theBar reinterpret_cast<UIScrollBar*>(a)
#define thePoll reinterpret_cast<TMPoll*>(a)
#define theLine reinterpret_cast<GELine*>(a)
#define theRhombus reinterpret_cast<GERhombus*>(a)
EE_UI_CASE(list_poll_create) { return (intptr_t) new TMPoll((unsigned long)a, 0, false); }
EE_UI_VOID(list_poll_delete) { delete thePoll; }
EE_UI_CASE(list_poll_running) { return thePoll->GetIsRunning(); }
EE_UI_CASE(list_poll_poll) { return thePoll->Poll(); }
EE_UI_VOID(list_poll_start) { thePoll->Start(); }
EE_UI_VOID(list_poll_stop) { thePoll->Stop(); }
EE_UI_CASE(list_rhombus_create) { return (intptr_t) new GERhombus(); }
EE_UI_VOID(list_rhombus_delete) { delete theRhombus; }
EE_UI_CASE(list_line_create) { return (intptr_t) new GELine(GE2DPoint(0.0f, 0.0f), GE2DPoint(0.0f, 0.0f), GEColor(0.0f, 0.0f, 0.0f), GEColor(0.0f, 0.0f, 0.0f), 1.0f); }
EE_UI_VOID(list_line_delete) { delete theLine; }
EE_UI_VOID(list_draw_count_decrement) { lst->UIControl::DrawTextA(); }
EE_UI_VOID(list_set_bg_rectangle) { lst->SetBgRectangle(UIObjHost::Rect(f)); }
EE_UI_CASE(list_base_mouse_double_pressed) { return lst->UIControl::MouseLButtonDoublePressed(); }
EE_UI_CASE(list_create_scroll_bar) { return (intptr_t) new UIScrollBar(lst->GetId(), GE2DPoint(f[0], f[1]), f[2], 1, lst->GetForm(), lst->GetIndex(), lst->GetBorderThickness()); }
EE_UI_VOID(list_bar_set_range) { theBar->SetRange(0, (unsigned long)b); }
EE_UI_VOID(list_bar_set_page_size) { theBar->SetPageSize((unsigned long)b); }
EE_UI_CASE(list_bar_value) { return theBar->GetSlider()->GetCurrentRangeValue(); }
EE_UI_CASE(list_bar_max) { return theBar->GetSlider()->GetMaxRange(); }
EE_UI_VOID(list_bar_set_value) { theBar->SetCurrentRangeValue((unsigned long)b); }
EE_UI_VOID(list_bar_increment_by) { theBar->IncrementBy((long)b); }
#undef theRhombus
#undef theLine
#undef thePoll
#undef theBar
#undef lst

// The string list's draw (ee_ui listobj.rs string_draw): its drawn text placed, given a drawn line, drawn clipped
#define LBS static_cast<UILBString*>(inObj)
EE_UI_VOID(lbs_text_at) { LBS->mCopiedText->SetLocation(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(lbs_text_line) { LBS->mCopiedText->SetText(LBS->mCopyLineArray[(size_t)a]); }
EE_UI_CASE(lbs_draw_clipped) { return LBS->GetForm()->DrawClippedText(UIObjHost::Rect(f), LBS->mCopiedText, LBS); }
#undef LBS
