// C:\EEX\Low-Level Engine\User Interface\UIComboBox.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100602e4..0x10064ed6. The first includer of UIListBox.h, UIEditBox.h, UILBString.h and UIComboBox.h, so it
// starts with their inline blocks in class-definition order (UWSNoCaseAlphaSort::operator(), UIEditBox's inline
// members + implicit copy members + ??_E, UIListBox<UWideString>'s copy members + ??_E, UILBString's, then
// UIComboBox's), then the U2D key init chains (0x100613ef), the ctor (0x1006150f) and the out-of-line members in
// alphabetical order, the vector<UWideString> copy members, every member of UIListBox<UWideString> in declaration
// order (0x100629a3..), the vector<UWideString> COMDATs, the InsertLine member template (0x1006452c) and the
// lower_bound/sort chains it and Sort() instantiate. UIControl.cpp starts at 0x10064ed6.
//
// UIComboBox is whole in Rust (rust/crates/ee_ui/src/comboobj.rs, R6 ui round 15; its logic is combo.rs): the
// members forward to the Rust object, which calls out through UIComboBox::Rust::Op (UIControl's members, the focus
// highlight, the parts by address, and the combo box's questions and actions, EeUiComboQuery / EeUiComboAct). The
// constructor makes the parts here.
#include <windows.h>
#include <mbstring.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "rust/ui/comboobj.h"
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
#include "lle/Input System.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/UITextLabel.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "lle/UIListBox.h"
#include "lle/UIEditBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIObjHost.h"
#include "lle/UIShims.h"
#include "rust/ui/lle_combo.h"

// the one-statement shims of the host's moved queries and actions and of the constructor (ee_ui lle_combo.rs)
#define COMBO(x) reinterpret_cast<UIComboBox*>(x)
#define CTL(x) reinterpret_cast<UIControl*>(x)
#define LIST(x) reinterpret_cast<UILBString*>(x)
#define LABEL(x) reinterpret_cast<UITextLabel*>(x)
#define EDIT(x) reinterpret_cast<UIEditBox*>(x)
#define TEXT(x) reinterpret_cast<GEText*>(x)
#define FORM(x) reinterpret_cast<UIForm*>(x)
#define BMP(x) reinterpret_cast<GEBitmap*>(x)
extern "C" uintptr_t ee_uicpp_combo_part(uintptr_t inCombo, uint32_t inPart)
{ return (uintptr_t)COMBO(inCombo)->PartOf((int)inPart); }
extern "C" uintptr_t ee_uicpp_list_scroll_bar(uintptr_t inList) { return (uintptr_t)LIST(inList)->GetScrollBar(); }
extern "C" void ee_uicpp_control_rect(uintptr_t inControl, float* inRect)
{ UIObjHost::PutRect(inRect, CTL(inControl)->GetRectangle()); }
extern "C" long ee_uicpp_list_selection(uintptr_t inList, unsigned long* inSelection)
{ return LIST(inList)->GetCurrentSelection(*inSelection); }
extern "C" float ee_uicpp_control_mouse_x(uintptr_t inControl) { return CTL(inControl)->GetMouseLocation().GetX(); }
extern "C" float ee_uicpp_control_mouse_y(uintptr_t inControl) { return CTL(inControl)->GetMouseLocation().GetY(); }
extern "C" bool ee_uicpp_key_down_pressed() { return UIControl::DownKeyPressed(); }
extern "C" bool ee_uicpp_key_up_pressed() { return UIControl::UpKeyPressed(); }
extern "C" bool ee_uicpp_key_home_pressed() { return UIControl::HomeKeyPressed(); }
extern "C" bool ee_uicpp_key_end_pressed() { return UIControl::EndKeyPressed(); }
extern "C" bool ee_uicpp_key_enter_value() { return UIControl::Key(EeUiKey::Enter)->GetValue() != 0; }
extern "C" void ee_uicpp_label_set_text_line(uintptr_t inLabel, uintptr_t inList, uint32_t inLine)
{ LABEL(inLabel)->SetText(LIST(inList)->GetData(inLine), true); }
extern "C" void ee_uicpp_edit_set_text_line(uintptr_t inEdit, uintptr_t inList, uint32_t inLine)
{ EDIT(inEdit)->SetText(LIST(inList)->GetData(inLine)); }
extern "C" void ee_uicpp_label_clear_text(uintptr_t inLabel) { LABEL(inLabel)->SetText("", true); }
extern "C" void ee_uicpp_edit_clear_text(uintptr_t inEdit) { EDIT(inEdit)->SetText(""); }
extern "C" void ee_uicpp_label_delete_first_line(uintptr_t inLabel) { LABEL(inLabel)->DeleteLine(0); }
extern "C" void ee_uicpp_edit_delete_text(uintptr_t inEdit) { EDIT(inEdit)->DeleteText(); }
extern "C" void ee_uicpp_control_tabbed_to(uintptr_t inControl) { CTL(inControl)->TabbedTo(); }
extern "C" void ee_uicpp_control_tabbed_from(uintptr_t inControl) { CTL(inControl)->TabbedFrom(); }
extern "C" uintptr_t ee_uicpp_label_text(uintptr_t inLabel) { return (uintptr_t)LABEL(inLabel)->GetGEText(); }
extern "C" uintptr_t ee_uicpp_edit_text(uintptr_t inEdit) { return (uintptr_t)EDIT(inEdit)->GetGEText(); }
extern "C" uintptr_t ee_uicpp_list_text(uintptr_t inList) { return (uintptr_t)LIST(inList)->GetGEText(); }
extern "C" void ee_uicpp_text_copy_fg(uintptr_t inTo, uintptr_t inFrom)
{ TEXT(inTo)->SetFgColor(TEXT(inFrom)->GetFgColor()); }
extern "C" void ee_uicpp_text_copy_bg(uintptr_t inTo, uintptr_t inFrom)
{ TEXT(inTo)->SetBgColor(TEXT(inFrom)->GetBgColor()); }
extern "C" void ee_uicpp_text_copy_transparent(uintptr_t inTo, uintptr_t inFrom)
{ TEXT(inTo)->SetIsTransparent(TEXT(inFrom)->GetIsTransparent()); }
extern "C" void ee_uicpp_combo_process_down(uintptr_t inCombo) { COMBO(inCombo)->ProcessDown(); }
extern "C" void ee_uicpp_combo_process_up(uintptr_t inCombo) { COMBO(inCombo)->ProcessUp(); }
extern "C" uintptr_t ee_uicpp_form_scaled_manager_bitmap(uintptr_t inForm, uint32_t inIndex, float inLeft, float inTop, float inRight, float inBottom)
{ return (uintptr_t)FORM(inForm)->GetManager()->CreateBitmapForScaledControl(FORM(inForm)->GetManager()->GetBitmap(inIndex), GERectangle(inLeft, inTop, inRight, inBottom)); }
extern "C" uintptr_t ee_uicpp_combo_new_drop(uintptr_t inCombo, uintptr_t inNormal, uintptr_t inPressed, float inLeft, float inTop, float inRight, float inBottom)
{ return (uintptr_t) new UIButton(COMBO(inCombo)->GetId(), BMP(inNormal), NULL, BMP(inPressed), GERectangle(inLeft, inTop, inRight, inBottom), COMBO(inCombo)->GetForm(), COMBO(inCombo)->GetIndex(), 0); }
extern "C" uint32_t ee_uicpp_control_width(uintptr_t inControl) { return (uint32_t)CTL(inControl)->GetWidth(); }
extern "C" uintptr_t ee_uicpp_combo_new_label(uintptr_t inCombo, uintptr_t inFont, float inLeft, float inTop, float inRight, float inBottom, unsigned long inBorder)
{ return (uintptr_t) new UITextLabel(UWideString(""), reinterpret_cast<GEFont*>(inFont), COMBO(inCombo)->GetId(), GERectangle(inLeft, inTop, inRight, inBottom), COMBO(inCombo)->GetForm(), 0, COMBO(inCombo)->GetIndex(), inBorder); }
extern "C" uintptr_t ee_uicpp_combo_new_edit(uintptr_t inCombo, uintptr_t inFont, unsigned long inMaxChars, float inLeft, float inTop, float inRight, float inBottom, unsigned long inBorder)
{ return (uintptr_t) new UIEditBox(COMBO(inCombo)->GetId(), reinterpret_cast<GEFont*>(inFont), inMaxChars, false, GERectangle(inLeft, inTop, inRight, inBottom), COMBO(inCombo)->GetForm(), COMBO(inCombo)->GetIndex(), inBorder, false, false); }
extern "C" uintptr_t ee_uicpp_combo_new_list(uintptr_t inCombo, uintptr_t inData, uintptr_t inFont, float inLeft, float inTop, float inRight, float inBottom, unsigned long inBorder)
{ return (uintptr_t) new UILBString(COMBO(inCombo)->GetId(), *reinterpret_cast<const std::vector<UWideString>*>(inData), reinterpret_cast<GEFont*>(inFont), true, GERectangle(inLeft, inTop, inRight, inBottom), COMBO(inCombo)->GetForm(), COMBO(inCombo)->GetIndex(), inBorder); }
extern "C" void ee_uicpp_list_selected_color(uintptr_t inList, float* inRgb)
{ LIST(inList)->GetSelectedBackground()->GetColorPerVertex(*reinterpret_cast<GEColor*>(inRgb), *reinterpret_cast<GEColor*>(inRgb), *reinterpret_cast<GEColor*>(inRgb), *reinterpret_cast<GEColor*>(inRgb)); }
extern "C" uint32_t ee_uicpp_list_line_count(uintptr_t inList) { return (uint32_t)LIST(inList)->GetLineCount(); }
extern "C" void ee_uicpp_combo_set_rust(uintptr_t inCombo, uintptr_t inRust)
{ COMBO(inCombo)->mRust = reinterpret_cast<EeUiCombo*>(inRust); }
extern "C" void ee_uicpp_list_change_selection(uintptr_t inList, uint32_t inIndex, bool inA, bool inB)
{ LIST(inList)->ChangeSelectionIndex(inIndex, inA, inB); }
extern "C" void ee_uicpp_control_set_bg_opacity(uintptr_t inControl, float inOpacity)
{ CTL(inControl)->SetBgOpacity(inOpacity); }
extern "C" void ee_uicpp_control_set_bg_color(uintptr_t inControl, float inRed, float inGreen, float inBlue)
{ CTL(inControl)->SetBgColor(GEColor(inRed, inGreen, inBlue)); }
extern "C" void ee_uicpp_list_set_scroll_bg_opacity(uintptr_t inList, float inOpacity)
{ LIST(inList)->SetScrollBarBgOpacity(inOpacity); }
extern "C" void ee_uicpp_list_set_scroll_bg_color(uintptr_t inList, float inRed, float inGreen, float inBlue)
{ LIST(inList)->SetScrollBarBgColor(GEColor(inRed, inGreen, inBlue)); }
extern "C" void ee_uicpp_combo_base_set_rect(uintptr_t inCombo, uintptr_t inTextBox, uintptr_t inDrop)
{ COMBO(inCombo)->UIControl::SetRectangle(GERectangle(CTL(inTextBox)->GetRectangle().GetTopLeft(), CTL(inDrop)->GetRectangle().GetBottomRight())); }

// The Rust combo box's calls out (ee_ui lle_hosts.rs combo: ComboQuery's and ComboAct's one shim each, by its
// number; a the query's argument, or b, c, d, f the action's)
#define CB static_cast<UIComboBox*>(inObj)
#define CB_PART(x) CB->PartOf((int)(x))
#define CB_LIST CB->GetList()
#define CB_EDIT reinterpret_cast<UIEditBox*>(CB->PartOf(3))
EE_UI_VOID(combo_q_rect_of) { UIObjHost::PutRect(f, CB_PART(a)->GetRectangle()); }
EE_UI_CASE(combo_q_text_box_border) { return CB_PART(0)->GetBorderThickness(); }
EE_UI_CASE(combo_q_line_count) { return CB_LIST->GetLineCount(); }
EE_UI_CASE(combo_q_line) { return (intptr_t)(const char*)CB_LIST->GetData((unsigned long)a); }
EE_UI_CASE(combo_q_line_height) { return CB_LIST->GetLineHeight(); }
EE_UI_CASE(combo_q_list_border) { return CB_LIST->GetBorderThickness(); }
EE_UI_CASE(combo_q_list_scroll_bar_captured) { return CB_LIST->GetScrollBarCaptured(); }
EE_UI_CASE(combo_q_window_height) { return CB->GetRasterizer()->GetWindow(CB->GetScreenIndex())->GetHeight(); }
EE_UI_CASE(combo_q_edit_text) { return (intptr_t)CB_EDIT->GetText(0); }
EE_UI_CASE(combo_q_tabbing_on) { return CB->GetForm()->GetTabbingOn(); }
EE_UI_CASE(combo_q_tab_stop) { return CB->GetForm()->GetTabStop(); }
EE_UI_CASE(combo_q_has_mouse_capture) { return CB->GetHasMouseCapture(); }
EE_UI_VOID(combo_a_list_change_selection) { CB_LIST->ChangeSelectionIndex((unsigned long)b, (c & 1) != 0, (c & 2) != 0); }
EE_UI_VOID(combo_a_list_insert_line) { CB_LIST->InsertLine(UWideString(reinterpret_cast<const char*>(b)), (unsigned long)c); }
EE_UI_VOID(combo_a_list_delete_line) { CB_LIST->DeleteLine((unsigned long)b); }
EE_UI_VOID(combo_a_list_delete_all) { CB_LIST->DeleteAllEntries(); }
EE_UI_VOID(combo_a_list_change_width) { CB_LIST->ChangeWidth((unsigned long)b); }
EE_UI_CASE(combo_a_part_mouse_down) { return CB_PART(b)->MouseLButtonDown(); }
EE_UI_CASE(combo_a_part_mouse_up) { return CB_PART(b)->MouseLButtonUp(); }
EE_UI_CASE(combo_a_part_mouse_move) { return CB_PART(b)->MouseMove(GE2DPoint(f[0], f[1])); }
EE_UI_CASE(combo_a_list_wheel) { return CB_LIST->MouseWheelTurned((long)b); }
EE_UI_CASE(combo_a_part_update_keyboard) { return CB_PART(b)->UpdateKeyboard(); }
EE_UI_CASE(combo_a_part_translated_key) { return CB_PART(b)->UpdateTranslatedKey(reinterpret_cast<const char*>(c), (short)d); }
EE_UI_VOID(combo_a_part_set_rectangle) { CB_PART(b)->SetRectangle(UIObjHost::Rect(f)); }
EE_UI_VOID(combo_a_part_set_location) { CB_PART(b)->SetLocation(GE2DPoint(f[0], f[1])); }
EE_UI_CASE(combo_a_part_copy_data) { return CB_PART(b)->CopyData(); }
EE_UI_CASE(combo_a_base_copy_data) { return CB->UIControl::CopyData(); }
EE_UI_VOID(combo_a_set_popup) { CB->GetForm()->SetPopup(b ? CB : NULL); }
EE_UI_VOID(combo_a_set_dirty) { CB->SetDirty(true); }
EE_UI_VOID(combo_a_set_rectangle) { CB->SetRectangle(UIObjHost::Rect(f)); }
EE_UI_VOID(combo_a_base_set_rectangle) { CB->UIControl::SetRectangle(UIObjHost::Rect(f)); }
EE_UI_VOID(combo_a_base_set_location) { CB->UIControl::SetLocation(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(combo_a_set_always_dirty) { CB->SetAlwaysDirty(b != 0); }
EE_UI_VOID(combo_a_fire) { CB->FireTriggerEvent((unsigned long)b); }
EE_UI_VOID(combo_a_capture_mouse) { CB->CaptureMouse(); }
EE_UI_VOID(combo_a_capture_keyboard) { CB->CaptureKeyboard(); }
EE_UI_VOID(combo_a_release_mouse) { CB->ReleaseCapturedMouse(); }
EE_UI_VOID(combo_a_release_keyboard) { CB->ReleaseCapturedKeyboard(); }
EE_UI_VOID(combo_a_open_list) { CB->OpenListBox(); }
EE_UI_VOID(combo_a_close_list) { CB->CloseListBox(); }
EE_UI_CASE(combo_a_add_string) { return CB->AddString(); }
EE_UI_VOID(combo_a_change_selection) { CB->ChangeSelection(); }
EE_UI_CASE(combo_rhombus_create) { return (intptr_t) new GERhombus(); }
EE_UI_VOID(combo_rhombus_delete) { delete reinterpret_cast<GERhombus*>(a); }
EE_UI_VOID(combo_draw_count_decrement) { CB->mDrawCount--; }
#undef CB_EDIT
#undef CB_LIST
#undef CB_PART
#undef CB
