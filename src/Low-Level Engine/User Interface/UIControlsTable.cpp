// C:\EEX\Low-Level Engine\User Interface\UIControlsTable.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10065f69..0x10069cc7: UIControlsTable's header-inline block (0x10065f69, declaration order; the getters ICF
// folded into earlier units are missing), its implicit copy members and ??_E, the U2D key init chains (0x1006672c),
// then the header-inline/implicit copy members and ??_E of every control class this unit includes first
// (UIDualButton, UIFileComboBox, UIFileListBox, UIGraph, UILineGraph, UIMovie, UITLScrolling, UIStatusBar,
// UIFontTable), the sUIControlsArray init chain (0x10067792), the ctors, the dtor and the members in alphabetical
// order, then the vector<UString> COMDATs (0x10069ad4..). UIDualButton.cpp starts at 0x10069cc7.
//
// UIControlsTable is whole in Rust (rust/crates/ee_ui/src/tableobj.rs, R6 ui round 18): the record, the static array,
// Create, the factories and the font are the Rust object; this unit keeps the members that only call GE objects and
// the host of its calls out (UIControlsTableHost: the controls' constructors and setters, the files, fonts, strings).
#include <windows.h>
#include <stddef.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureManager.h"
#include "lle/GEScreen.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/UResourceFile.h"
#include "lle/UIControlsTable.h"
#include "lle/UIBitmap.h"
#include "lle/UIBlinkingButton.h"
#include "lle/UICheckBox.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIFileListBox.h"
#include "lle/UIGraph.h"
#include "lle/UIRadioButton.h"
#include "lle/UITLScrolling.h"
#include "lle/UIStatusBar.h"
#include "lle/UIFontTable.h"

// the one-statement shims of the moved host cases, CreateBitmap and SetupStringArray (ee_ui lle_table.rs)
#define FORM(x) reinterpret_cast<UIForm*>(x)
#define MGR(x) reinterpret_cast<UIManager*>(x)
#define CTL(x) reinterpret_cast<UIControl*>(x)
#define TBL(x) reinterpret_cast<const UIControlsTable*>(x)
#define STRINGS(x) reinterpret_cast<std::vector<UWideString>*>(x)
#define RECT(x) reinterpret_cast<const GERectangle*>(x)
extern "C" void ee_uicpp_form_add_database_control(uintptr_t inForm, uint32_t inIndex)
{ FORM(inForm)->AddDatabaseControl(inIndex, NULL); }
extern "C" uintptr_t ee_uicpp_string_new() { return (uintptr_t) new UWideString; }
extern "C" void ee_uicpp_mgr_string(uintptr_t inM, uint32_t inId, uintptr_t inString)
{ MGR(inM)->GetStringTable()->GetString(inId, *reinterpret_cast<UWideString*>(inString)); }
extern "C" uintptr_t ee_uicpp_strings_new() { return (uintptr_t) new std::vector<UWideString>; }
extern "C" void ee_uicpp_strings_clear(uintptr_t inStrings) { STRINGS(inStrings)->clear(); }
extern "C" void ee_uicpp_strings_resize(uintptr_t inStrings, uint32_t inCount) { STRINGS(inStrings)->resize(inCount); }
extern "C" uint32_t ee_uicpp_strings_count(uintptr_t inStrings) { return (uint32_t)STRINGS(inStrings)->size(); }
extern "C" void ee_uicpp_mgr_string_at(uintptr_t inM, int32_t inId, uintptr_t inStrings, uint32_t inIndex)
{ MGR(inM)->GetStringTable()->GetString((long)inId, (*STRINGS(inStrings))[inIndex]); }
extern "C" int32_t ee_uicpp_table_string_begin(uintptr_t inT) { return (int32_t)TBL(inT)->GetStringTableBegin(); }
extern "C" int32_t ee_uicpp_table_string_end(uintptr_t inT) { return (int32_t)TBL(inT)->GetStringTableEnd(); }
extern "C" uintptr_t ee_uicpp_table_bitmap_normal(uintptr_t inT, uintptr_t inM, float inLeft, float inTop, float inRight, float inBottom)
{ return (uintptr_t)TBL(inT)->CreateNormalBitmap(MGR(inM), GERectangle(inLeft, inTop, inRight, inBottom)); }
extern "C" uintptr_t ee_uicpp_table_bitmap_pressed(uintptr_t inT, uintptr_t inM, float inLeft, float inTop, float inRight, float inBottom)
{ return (uintptr_t)TBL(inT)->CreatePressedBitmap(MGR(inM), GERectangle(inLeft, inTop, inRight, inBottom)); }
extern "C" uintptr_t ee_uicpp_table_bitmap_focused(uintptr_t inT, uintptr_t inM, float inLeft, float inTop, float inRight, float inBottom)
{ return (uintptr_t)TBL(inT)->CreateFocusedBitmap(MGR(inM), GERectangle(inLeft, inTop, inRight, inBottom)); }
extern "C" void ee_uicpp_bitmap_dereference(uintptr_t inB) { reinterpret_cast<GEBitmap*>(inB)->Dereference(); }
extern "C" void ee_uicpp_file_list_add_extension(uintptr_t inControl, uintptr_t inStrings, uint32_t inIndex)
{ static_cast<UIFileListBox*>(CTL(inControl))->AddValidExtension(UString((const char*)(*STRINGS(inStrings))[inIndex])); }
extern "C" void ee_uicpp_radio_set_down_unchecked(uintptr_t inControl, uintptr_t inBitmap)
{ static_cast<UIRadioButton*>(CTL(inControl))->SetDownUncheckedBitmap(reinterpret_cast<GEBitmap*>(inBitmap)); }
extern "C" void ee_uicpp_radio_set_down_checked(uintptr_t inControl, uintptr_t inBitmap)
{ static_cast<UIRadioButton*>(CTL(inControl))->SetDownCheckedBitmap(reinterpret_cast<GEBitmap*>(inBitmap)); }
extern "C" void ee_uicpp_radio_set_up_checked(uintptr_t inControl, uintptr_t inBitmap)
{ static_cast<UIRadioButton*>(CTL(inControl))->SetUpCheckedBitmap(reinterpret_cast<GEBitmap*>(inBitmap)); }
extern "C" uintptr_t ee_uicpp_mgr_texture_manager_by_screen(uintptr_t inM)
{ return (uintptr_t)MGR(inM)->GetRasterizer()->GetTextureManager(MGR(inM)->GetScreen()); }
extern "C" uintptr_t ee_uicpp_texture_load_ui_string(uintptr_t inManager, uintptr_t inName, uint32_t inDepth, bool inAlpha)
{ return (uintptr_t)reinterpret_cast<GETextureManager*>(inManager)->LoadTexture(*reinterpret_cast<const UString*>(inName), GETextureDescription{1, 0, inDepth, 0, false, false, inAlpha, true}); }
extern "C" void ee_uicpp_bitmap_stretch(uintptr_t inB, uintptr_t inRect, uintptr_t inTextureRect, uint32_t inScreen)
{ reinterpret_cast<GEBitmap*>(inB)->ComputeStretchedCoordsAndScale(*RECT(inRect), *RECT(inRect), *RECT(inTextureRect), inScreen); }

// The Rust table's calls out (EeUiOp, obj.rs; ee_ui tableobj.rs), one statement each (ee_ui lle_hosts_tables.rs
// table; lle_table.rs: the cases of more than one call, the constructors by kind, the setters and texts by
// selector): a is the form, font, file, string, bitmap, control or event the op names, d the manager where the op
// needs one; rectangles and colours travel in f
#define TM reinterpret_cast<UIManager*>(d)
#define TF reinterpret_cast<UIForm*>(a)
EE_UI_CASE(tbl_window_width) { return TM->GetRasterizer()->GetWindow(TM->GetScreen())->GetWidth(); }
EE_UI_CASE(tbl_window_height) { return TM->GetRasterizer()->GetWindow(TM->GetScreen())->GetHeight(); }
EE_UI_CASE(tbl_form_by_id) { return (intptr_t)TM->GetFormByID((unsigned long)a); }
EE_UI_CASE(tbl_form_exclusive) { return TF->GetUseExclusiveCoords(); }
EE_UI_CASE(tbl_form_designed_height) { return TF->GetDesignedHeight(); }
EE_UI_CASE(tbl_form_gdi) { return TF->GetUseGDIForText(); }
EE_UI_CASE(tbl_font_height) { return reinterpret_cast<GEFont*>(a)->GetHeight(); }
EE_UI_CASE(tbl_font_has_bitmap) { return reinterpret_cast<GEFont*>(a)->GetFontBitmap() != NULL; }
EE_UI_CASE(tbl_string_length) { return reinterpret_cast<const UWideString*>(a)->GetLength(); }
EE_UI_VOID(tbl_file_open) { reinterpret_cast<FSFileSpec*>(a)->Open(kFileAccessRead); }
EE_UI_VOID(tbl_file_close) { reinterpret_cast<FSFileSpec*>(a)->Close(); }
EE_UI_CASE(tbl_wrap) { return (intptr_t) new UIControlsTable(reinterpret_cast<EeUiControlsTable*>(a)); }
EE_UI_VOID(tbl_delete) { delete reinterpret_cast<UIControlsTable*>(a); }
EE_UI_VOID(tbl_reserve_index) { TF->ReserveControlIndex((unsigned long)b); }
EE_UI_VOID(tbl_string_delete) { delete reinterpret_cast<UWideString*>(a); }
EE_UI_VOID(tbl_strings_delete) { delete reinterpret_cast<std::vector<UWideString>*>(a); }
EE_UI_CASE(tbl_font_get) { return (intptr_t)UIFontTable::GetFont((unsigned long)a); }
EE_UI_CASE(tbl_font_closest) { return (intptr_t)UIFontTable::CreateClosestFont((unsigned long)a, (unsigned long)b); }
EE_UI_VOID(tbl_font_release) { reinterpret_cast<GEFont*>(a)->Dereference(); }
EE_UI_VOID(tbl_font_bitmap_new) { new GEFontBitmap(TM->GetRasterizer(), reinterpret_cast<GEFont*>(a), TM->GetScreen()); }
EE_UI_CASE(tbl_user_defined) { return (intptr_t)TM->CreateUserDefinedControl(*static_cast<UIControlsTable*>(inObj)); }
EE_UI_CASE(tbl_event_new) { return (intptr_t)TF->GetManager()->CreateControlEvent(TF, (unsigned long)b); }
EE_UI_VOID(tbl_event_release) { reinterpret_cast<UIEvent*>(a)->Dereference(); }
EE_UI_CASE(tbl_form_control) { return (intptr_t)TF->GetControl((unsigned long)b); }
#undef TF
#undef TM
// A control's constructor (the factories' new: CreateBitmap .. CreateTextLabel), its arguments at a
#define MK (*reinterpret_cast<const EeUiTableMake*>(a))
#define MK_R GERectangle(MK.rect.left, MK.rect.top, MK.rect.right, MK.rect.bottom)
#define MK_FORM reinterpret_cast<UIForm*>(MK.form)
#define MK_FONT reinterpret_cast<GEFont*>(MK.font)
#define MK_TEXT reinterpret_cast<const UWideString*>(MK.text)
#define MK_STRINGS reinterpret_cast<std::vector<UWideString>*>(MK.strings)
#define MK_N reinterpret_cast<GEBitmap*>(MK.normal)
#define MK_FO reinterpret_cast<GEBitmap*>(MK.focused)
#define MK_P reinterpret_cast<GEBitmap*>(MK.pressed)
EE_UI_CASE(tbl_make_bitmap) { return (intptr_t)static_cast<UIControl*>(new UIBitmap(MK.id, MK_N, MK_R, MK_FORM, MK.index)); }
EE_UI_CASE(tbl_make_button) { return (intptr_t)static_cast<UIControl*>(new UIButton(MK.id, MK_N, MK_FO, MK_P, MK_R, MK_FORM, MK.index, MK.border)); }
EE_UI_CASE(tbl_make_blinking_button) { return (intptr_t)static_cast<UIControl*>(new UIBlinkingButton(MK.id, MK_N, MK_FO, MK_P, MK_R, MK_FORM, MK.index, MK.border, MK.blink_rate)); }
// (the scalable and dual buttons are buttons whose Rust record the factory then sets: lle_hosts.rs table_make)
EE_UI_CASE(tbl_make_scalable_button) { return (intptr_t)static_cast<UIControl*>(new UIButton(MK.id, MK_N, MK_FO, MK_P, MK_R, MK_FORM, MK.index, 0)); }
EE_UI_CASE(tbl_make_check_box) { return (intptr_t)static_cast<UIControl*>(new UICheckBox(MK.id, *MK_TEXT, MK_FONT, MK_R, MK_FORM, MK.index, MK.border, MK.flag_a)); }
EE_UI_CASE(tbl_make_combo_box) { return (intptr_t)static_cast<UIControl*>(new UIComboBox(MK.id, MK.style, *MK_STRINGS, MK_FONT, MK_R, MK.list_height, MK_FORM, MK.index, MK.max_chars, MK.border)); }
EE_UI_CASE(tbl_make_file_combo_box) { return (intptr_t)static_cast<UIControl*>(new UIFileComboBox(MK.id, 1, *MK_STRINGS, MK_FONT, MK_R.GetTopLeft(), MK.length, MK.list_height, MK_FORM, MK.index, MK.max_chars, MK.border)); }
EE_UI_CASE(tbl_make_edit_box) { return (intptr_t)static_cast<UIControl*>(new UIEditBox(MK.id, MK_FONT, MK.max_chars, MK.flag_a, MK_R, MK_FORM, MK.index, MK.border, MK.flag_b, MK.flag_c)); }
EE_UI_CASE(tbl_make_file_list_box) { return (intptr_t)static_cast<UIControl*>(new UIFileListBox(MK.id, MK_FONT, false, MK_R, MK_FORM, MK.index, MK.border)); }
EE_UI_CASE(tbl_make_line_graph) { return (intptr_t)static_cast<UIControl*>(new UIGraph(MK.id, MK_R, MK_FORM, MK.index, MK.border)); }
EE_UI_CASE(tbl_make_list_box) { return (intptr_t)static_cast<UIControl*>(new UILBString(MK.id, *MK_STRINGS, MK_FONT, MK.flag_a, MK_R, MK_FORM, MK.index, MK.border)); }
EE_UI_CASE(tbl_make_radio_button) { return (intptr_t)static_cast<UIControl*>(new UIRadioButton(MK.id, *MK_TEXT, MK_FONT, MK_R, MK_FORM, MK.index, MK.border, MK.flag_a, MK.flag_b, MK.flag_c)); }
EE_UI_CASE(tbl_make_scroll_bar) { return (intptr_t)static_cast<UIControl*>(new UIScrollBar(MK.id, MK_R.GetTopLeft(), MK.length, MK.style, MK_FORM, MK.index, MK.border)); }
EE_UI_CASE(tbl_make_scrolling_label) { return (intptr_t)static_cast<UIControl*>(new UITLScrolling(*MK_TEXT, MK_FONT, MK.id, MK_R, MK_FORM, MK.index, MK.border)); }
EE_UI_CASE(tbl_make_slider) { return (intptr_t)static_cast<UIControl*>(new UISlider(MK.id, MK_R, MK.style, MK_FORM, MK.index, MK.border)); }
EE_UI_CASE(tbl_make_status_bar) { return (intptr_t)static_cast<UIControl*>(new UIStatusBar(MK.id, MK_R, MK_FORM, MK.index, MK.border)); }
EE_UI_CASE(tbl_make_text_label) { return (intptr_t)static_cast<UIControl*>(new UITextLabel(*MK_TEXT, MK_FONT, MK.id, MK_R, MK_FORM, MK.style, MK.index, MK.border)); }
#undef MK_P
#undef MK_FO
#undef MK_N
#undef MK_STRINGS
#undef MK_TEXT
#undef MK_FONT
#undef MK_FORM
#undef MK_R
#undef MK
// A made control's (a) setter by selector, its values c, d and f; the texts SetupText sets up
#define SC reinterpret_cast<UIControl*>(a)
EE_UI_VOID(tbl_set_button_text) { static_cast<UIButton*>(SC)->SetText((*reinterpret_cast<const UWideString*>(c)), reinterpret_cast<GEFont*>(d)); }
EE_UI_VOID(tbl_set_button_focused_color) { static_cast<UIButton*>(SC)->SetFocusedTextColor(GEColor(f[0], f[1], f[2])); }
EE_UI_VOID(tbl_set_button_down_sound) { static_cast<UIButton*>(SC)->SetButtonDownSound(UString(reinterpret_cast<const char*>(c))); }
EE_UI_VOID(tbl_set_button_up_sound) { static_cast<UIButton*>(SC)->SetButtonUpSound(UString(reinterpret_cast<const char*>(c))); }
EE_UI_VOID(tbl_set_check_set_checked) { static_cast<UICheckBox*>(SC)->SetChecked(c != 0); }
EE_UI_VOID(tbl_set_combo_focused_rect) { static_cast<UIComboBox*>(SC)->SetListBoxFocusedRectColor(GEColor(f[0], f[1], f[2])); }
EE_UI_VOID(tbl_set_combo_selected_bg) { static_cast<UIComboBox*>(SC)->SetListBoxSelectedBgColor(GEColor(f[0], f[1], f[2])); }
EE_UI_VOID(tbl_set_combo_selection) { static_cast<UIComboBox*>(SC)->SetCurrentSelection((long)c); }
EE_UI_VOID(tbl_set_combo_text_box_opacity) { static_cast<UIComboBox*>(SC)->SetTextBoxBgOpacity(f[0]); }
EE_UI_VOID(tbl_set_combo_text_box_color) { static_cast<UIComboBox*>(SC)->SetTextBoxBgColor(GEColor(f[0], f[1], f[2])); }
EE_UI_VOID(tbl_set_combo_list_always_dirty) { static_cast<UIComboBox*>(SC)->SetListBoxAlwaysDirty(c != 0); }
EE_UI_VOID(tbl_set_edit_number) { static_cast<UIEditBox*>(SC)->SetText((unsigned long)c); }
EE_UI_VOID(tbl_set_edit_text) { static_cast<UIEditBox*>(SC)->SetText((*reinterpret_cast<const UWideString*>(c))); }
EE_UI_VOID(tbl_set_edit_selected_bg) { static_cast<UIEditBox*>(SC)->SetSelectedBgColor(GEColor(f[0], f[1], f[2])); }
EE_UI_VOID(tbl_set_list_focused_rect) { static_cast<UILBString*>(SC)->SetFocusedRectColor(GEColor(f[0], f[1], f[2])); }
EE_UI_VOID(tbl_set_list_selected_bg) { static_cast<UILBString*>(SC)->GetSelectedBackground()->SetColor(GEColor(f[0], f[1], f[2])); }
EE_UI_VOID(tbl_set_list_scroll_opacity) { static_cast<UILBString*>(SC)->SetScrollBarBgOpacity(f[0]); }
EE_UI_VOID(tbl_set_list_selection) { static_cast<UILBString*>(SC)->ChangeSelectionIndex((unsigned long)c, false, false); }
EE_UI_VOID(tbl_set_graph_font) { static_cast<UIGraph*>(SC)->SetFont(reinterpret_cast<GEFont*>(c)); }
EE_UI_VOID(tbl_set_graph_font_color) { static_cast<UIGraph*>(SC)->SetFontColor(GEColor(f[0], f[1], f[2])); }
EE_UI_VOID(tbl_set_radio_up_unchecked) { static_cast<UIRadioButton*>(SC)->SetUpUncheckedBitmap(reinterpret_cast<GEBitmap*>(c)); }
EE_UI_VOID(tbl_set_scroll_range) { static_cast<UIScrollBar*>(SC)->SetRange((long)c, (long)d); }
EE_UI_VOID(tbl_set_scroll_value) { static_cast<UIScrollBar*>(SC)->SetCurrentRangeValue((long)c); }
EE_UI_VOID(tbl_set_slider_range) { static_cast<UISlider*>(SC)->SetRange((long)c, (long)d); }
EE_UI_VOID(tbl_set_slider_value) { static_cast<UISlider*>(SC)->SetCurrentRangeValue((long)c); }
EE_UI_VOID(tbl_set_slider_page) { static_cast<UISlider*>(SC)->SetPageSize((long)c); }
EE_UI_VOID(tbl_set_status_range) { static_cast<UIStatusBar*>(SC)->SetRange((long)c, (long)d); }
EE_UI_VOID(tbl_set_status_value) { static_cast<UIStatusBar*>(SC)->SetCurrentRangeValue((long)c); }
EE_UI_VOID(tbl_set_status_color) { static_cast<UIStatusBar*>(SC)->SetStatusBarColor(GEColor(f[0], f[1], f[2])); }
EE_UI_VOID(tbl_set_status_opacity) { static_cast<UIStatusBar*>(SC)->SetStatusBarOpacity(f[0]); }
EE_UI_VOID(tbl_set_status_text) { static_cast<UIStatusBar*>(SC)->SetText((*reinterpret_cast<const UWideString*>(c)), reinterpret_cast<GEFont*>(d)); }
EE_UI_VOID(tbl_set_status_focused_color) { static_cast<UIStatusBar*>(SC)->SetFocusedTextColor(GEColor(f[0], f[1], f[2])); }
EE_UI_VOID(tbl_set_label_number) { static_cast<UITextLabel*>(SC)->SetText((long)c); }
EE_UI_VOID(tbl_set_label_align) { static_cast<UITextLabel*>(SC)->SetAlignment((long)c); }
EE_UI_VOID(tbl_set_label_scroll_opacity) { static_cast<UITextLabel*>(SC)->SetScrollBarBgOpacity(f[0]); }
EE_UI_VOID(tbl_set_label_fit) { static_cast<UITextLabel*>(SC)->SetFitRectToText(c != 0); }
EE_UI_VOID(tbl_set_bg_color) { SC->SetBgColor(GEColor(f[0], f[1], f[2])); }
EE_UI_VOID(tbl_set_bg_opacity) { SC->SetBgOpacity(f[0]); }
EE_UI_VOID(tbl_set_always_dirty) { SC->SetAlwaysDirty(c != 0); }
EE_UI_VOID(tbl_set_set_rectangle) { SC->SetRectangle(UIObjHost::Rect(f)); }
EE_UI_VOID(tbl_set_change_font) { SC->ChangeFont(reinterpret_cast<GEFont*>(c)); }
EE_UI_VOID(tbl_set_set_trigger) { SC->SetTriggerEvent((unsigned long)c, reinterpret_cast<UIEvent*>(d)); }
EE_UI_CASE(tbl_text_button) { return (intptr_t)static_cast<UIButton*>(SC)->GetGEText(); }
EE_UI_CASE(tbl_text_check) { return (intptr_t)static_cast<UICheckBox*>(SC)->GetTextLabel()->GetGEText(); }
EE_UI_CASE(tbl_text_combo) { return (intptr_t)static_cast<UIComboBox*>(SC)->GetGEText(); }
EE_UI_CASE(tbl_text_edit) { return (intptr_t)static_cast<UIEditBox*>(SC)->GetGEText(); }
EE_UI_CASE(tbl_text_list) { return (intptr_t)static_cast<UILBString*>(SC)->GetGEText(); }
EE_UI_CASE(tbl_text_radio) { return (intptr_t)static_cast<UIRadioButton*>(SC)->GetTextLabel()->GetGEText(); }
EE_UI_CASE(tbl_text_label) { return (intptr_t)static_cast<UITextLabel*>(SC)->GetGEText(); }
EE_UI_CASE(tbl_text_status) { return (intptr_t)static_cast<UIStatusBar*>(SC)->GetGEText(); }
#undef SC
// SetupGEText's two calls (lle_table.rs ee_ui_table_setup_text) on the text a
EE_UI_VOID(tbl_text_transparent) { reinterpret_cast<GEText*>(a)->SetIsTransparent(true); }
EE_UI_VOID(tbl_text_fg) { reinterpret_cast<GEText*>(a)->SetFgColor(static_cast<UIControlsTable*>(inObj)->GetTextForeground()); }
