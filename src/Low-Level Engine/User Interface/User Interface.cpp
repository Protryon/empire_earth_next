// C:\EEX\Low-Level Engine\User Interface\User Interface.cpp (file name inferred, by analogy with
// Thread System\Thread System.cpp: the subsystem's initialize/shutdown entry points)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10082871..0x10082c43: the init chain of an unused file-static UIBackgroundTable, the gDbUI* file name chains, then
// UICreateMultilineTextFromString, UIGetStringCutOffIndex (now ee_ui cutoff.rs, its one caller the chat line's
// Rust), UIInitialize and its shutdown hook. UString.cpp starts at 0x10082c43.
//
// port: the toolkit's one unit (LLE/UI round 2). The original's other User Interface units were one compiled file
// each; each is a section here, headed by its path and keeping its comment (its address range, what is whole in
// Rust). A reference elsewhere to one of them (UIForm.cpp's host, UIButton.cpp's shims, ...) names its section.
#include <windows.h>
#include <algorithm>
#include <mbstring.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <utility>
#include "lle/LowLevelEngine.h"
#include "lle/User Interface.h"
#include "EEDataText.h"   // EEDbFindFile: the db tables' text forms
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/UIBackgroundTable.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControlsTable.h"
#include "lle/UIBitmap.h"
#include "lle/UIEMouseWheelScroll.h"
#include "lle/UITiledBackground.h"
#include "lle/UIBFull.h"
#include "lle/SESoundManager.h"
#include "lle/UResourceFile.h"
#include "lle/Input System.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/UIBlinkingButton.h"
#include "lle/UICheckBox.h"
#include "lle/UIRadioButton.h"
#include "lle/UITLScrolling.h"
#include "lle/UIStatusBar.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIListBox.h"
#include "lle/UIEditBox.h"
#include "lle/UILBString.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIFileListBox.h"
#include "lle/UIGraph.h"
#include "lle/GEViewport.h"      // (unused here: the harness's layout dump, tools/harness/layout_gen.py, reads them)
#include "lle/UIFormManager.h"
#include "rust/ui/form_state.h"

// C:\EEX\Low-Level Engine\User Interface\UIBackground.cpp
// 0x10058d38..0x1005c34a: the first User Interface unit. It opens with the header-inline members of every UI class it
// is the first to include (UILayoutUtility, TMPoll's implicit operator=, UIBackground, UIEntity, UIEntityGroup,
// UIControl, UIFormManager, UIForm incl. its implicit copy members, UIEvent), the U2D key init chains (0x10059ebb),
// UIManager's inline and implicit copy members, then its own code (0x1005a76e) and the vector/deque/map COMDATs
// (0x1005aa1b..). UIBackgroundTable.cpp starts at 0x1005c34a.
//
// The backgrounds are whole in Rust (rust/crates/ee_ui/src/bgobj.rs, R6 ui round 20): this unit holds every
// background class's forwarders (UIBackground, UIBitmapBackground, UITiledBackground, UIBFull, UIFullBackground; the
// units UIBitmapBackground.cpp, UITiledBackground.cpp, UILRTiledBackground.cpp, UITBTiledBackground.cpp,
// UIStretchedTiledBackground.cpp, UIBFull.cpp and UIFullBackground.cpp are gone) and the Rust object's calls out
// (UIBackgroundHost::Op: the form's questions, locks and redraws, the virtuals, the bitmaps and textures).
static_assert(sizeof(GETextureDescription) == sizeof(EeUiTextureDescription), "ee_ui bgobj.rs");
static_assert(sizeof(GERectangle) == sizeof(EeUiRect), "ee_ui bgobj.rs");

// eight floats for the wide calls (two rectangles, or a rectangle and four floats; ee_ui bgobj.rs)
struct UIF8 {
    UIF8(const GERectangle& r, const GERectangle& s) : f{r.mBottom, r.mLeft, r.mRight, r.mTop, s.mBottom, s.mLeft, s.mRight, s.mTop} {}
    UIF8(const GERectangle& r, float a, float b, float c, float d) : f{r.mBottom, r.mLeft, r.mRight, r.mTop, a, b, c, d} {}
    operator float*() { return this->f; }
    float f[8];
};
#define r static_cast<EeUiBackgroundFields*>(inObj)
#define theForm (static_cast<UIForm*>(r->mForm))
#define m (theForm->GetManager())
#define s (m->GetScreen())
#define theBitmap reinterpret_cast<GEBitmap*>(a)
EE_UI_VOID(bg_add) { theForm->AddBackground(static_cast<UIBackground*>(r)); }
EE_UI_CASE(bg_window) { return a ? m->GetRasterizer()->GetWindow(s)->GetHeight() : m->GetRasterizer()->GetWindow(s)->GetWidth(); }
EE_UI_CASE(bg_exclusive) { return theForm->GetUseExclusiveCoords(); }
EE_UI_CASE(bg_form_current) { return theForm == m->GetCurrentForm(); }
EE_UI_VOID(bg_lock) { a ? theForm->LockDrawBackground() : theForm->UnlockDrawBackground(); }
EE_UI_VOID(bg_redraw) { theForm->RedrawIntersectControls(UIObjHost::Rect(f)); }
EE_UI_VOID(bg_dirty) { m->AddDirtyRectangle(UIObjHost::Rect(f), theForm); }
EE_UI_CASE(bg_virt_load) { return static_cast<UIBackground*>(r)->LoadBackground(); }
EE_UI_CASE(bg_virt_unload) { return static_cast<UIBackground*>(r)->UnloadBackground(); }
EE_UI_CASE(bg_virt_stretched) { return static_cast<UIBitmapBackground*>(static_cast<UIBackground*>(r)) ->DrawStrectchedBackground(UIObjHost::Rect(f), UIObjHost::Rect(f + 4)); }
EE_UI_CASE(bg_virt_tiled) { return static_cast<UITiledBackground*>(static_cast<UIBackground*>(r)) ->DrawTiledBackground(UIObjHost::Rect(f), a, b, c, d, f[4], f[5]); }
EE_UI_CASE(bg_bitmap_new) { return (intptr_t) new GEBitmap; }
EE_UI_VOID(bg_bitmap_release) { theBitmap->Dereference(); }
EE_UI_CASE(bg_bitmap_texture) { return (intptr_t)theBitmap->GetTexture(s); }
EE_UI_VOID(bg_bitmap_set_texture) { theBitmap->SetTexture(reinterpret_cast<GETexture*>(b), s); }
EE_UI_VOID(bg_form_rect) { UIObjHost::PutRect(f, theForm->GetRectangle()); }
EE_UI_VOID(bg_form_fade) { f[0] = theForm->GetFadeOpacity(); }
#undef theBitmap
#undef s
#undef m
#undef theForm
#undef r
typedef EeUiBgCall C;

// the texture's size and load: Rust's (ee_ui lle_bg_edit.rs, UI lane round 12) before the host above
#define UIBGM(r) static_cast<UIForm*>(static_cast<EeUiBackgroundFields*>((void*)(r))->mForm)->GetManager()
extern "C" uintptr_t ee_uicpp_bg_texture_load(uintptr_t inRecord, EeUiShimText inName, uintptr_t inDescription)
{ return (uintptr_t)UIBGM(inRecord)->GetRasterizer()->GetTextureManager(UIBGM(inRecord)->GetScreen())->LoadTexture(UIShimString(inName), *reinterpret_cast<const GETextureDescription*>(inDescription)); }

long UIBitmapBackground::DrawStrectchedBackground(const GERectangle& inSource, const GERectangle& inDestination)
{
    return (long)this->Call(C::BitmapStretched, 0, 0, UIF8(inSource, inDestination));
}

long UITiledBackground::DrawTiledBackground(const GERectangle& inClip, unsigned long inFirstRow, unsigned long inLastRow,
                                            unsigned long inFirstColumn, unsigned long inLastColumn, float inWidth,
                                            float inHeight)
{
    return (long)this->Call(C::DrawTiled, inFirstRow, inLastRow,
                            UIF8(inClip, UIF4::Bits((int32_t)inFirstColumn), UIF4::Bits((int32_t)inLastColumn), inWidth, inHeight));
}

// C:\EEX\Low-Level Engine\User Interface\UIBackgroundTable.cpp
// 0x1005c34a..0x1005c84e: UIBackgroundTable's inline members first (0x1005c34a..0x1005c575), then the
// sUIBackgroundArray init chain, the default ctor, Create and DeleteAll. UIBFull.cpp's inline block started at
// 0x1005c84e.
//
// UIBackgroundTable is whole in Rust with UIManager::CreateBackgrounds (rust/crates/ee_ui/src/bgtableobj.rs, R6 ui
// round 20): the members are the record EeUiBgTableFields, the array a Rust list; Create, DeleteAll and
// CreateBackgrounds call out through ee_ui_bg_table_host (the database file, an entry's new and delete, the
// manager's forms, a background's construction by type).
static_assert(sizeof(EeUiLayout) == sizeof(UILayoutUtility) && sizeof(EeUiColor) == sizeof(GEColor), "bgtableobj.rs");

// The Rust table's calls out (EeUiOp 590..600): the object is the file (Create) or the manager (CreateBackgrounds)
#define t (static_cast<UIBackgroundTable*>(reinterpret_cast<EeUiBgTableFields*>(a)))
#define m static_cast<UIManager*>(inObj)
EE_UI_VOID(bgt_file_open) { ee_uicpp_file_open(inObj); }
EE_UI_CASE(bgt_file_read) { return ee_uicpp_file_read(inObj, reinterpret_cast<unsigned char*>(b), (unsigned long)c); }
EE_UI_VOID(bgt_file_close) { ee_uicpp_file_close(inObj); }
EE_UI_CASE(bgt_new) { return (intptr_t) static_cast<EeUiBgTableFields*>(new UIBackgroundTable(*reinterpret_cast<const EeUiBgTableFields*>(a), reinterpret_cast<const char*>(b))); }
EE_UI_VOID(bgt_delete) { delete t; }
EE_UI_CASE(bgt_form) { return (intptr_t)m->GetFormByID((unsigned long)a); }
EE_UI_CASE(bgt_texture_depth) { return m->GetUITextureBitDepth(); }
EE_UI_CASE(bgt_entry_alpha) { return t->GetHasAlpha(); }
EE_UI_CASE(bgt_entry_form_id) { return t->GetFormID(); }
// a background of the entry's type on the form b with the texture description c (lle_hosts.rs bgt_make: the type's
// class; the tiled ones' tiling d and across and down percents f; others the manager's user-defined backgrounds)
#define BGT_ARGS t->GetBackgroundID(), t->GetTextureFile(), *reinterpret_cast<const GETextureDescription*>(c), t->GetTextureRect(), t->GetLayout(), reinterpret_cast<UIForm*>(b), t->GetDrawPriority()
EE_UI_CASE(bgt_make_bitmap) { return (intptr_t) static_cast<UIBackground*>(new UIBitmapBackground(BGT_ARGS, t->GetVisible())); }
// Fixed: the original passed the row ID (GetID) for a tiled background where every other type passes the background ID
EE_UI_CASE(bgt_make_tiled) { return (intptr_t) static_cast<UIBackground*>(new UITiledBackground(BGT_ARGS, t->GetVisible(), (UITiling)d, f[0], f[1])); }
EE_UI_CASE(bgt_make_full) { return (intptr_t) static_cast<UIBackground*>(new UIBFull(BGT_ARGS, t->GetNumCols(), t->GetNumRows(), t->GetVisible())); }
EE_UI_CASE(bgt_make_user) { return (intptr_t)m->CreateUserDefinedBackground(*t); }
#undef BGT_ARGS
#undef m
#undef t

// Creates an empty background table entry (IDs -1, bitmap type, hidden, full opacity, draw priority 1)

// C:\EEX\Low-Level Engine\User Interface\UIButton.cpp
// 0x1005e47c..0x1005ef4b: the U2D key init chains (U2DRectangle.h, via the rasterizer header), the ctor, the dtor
// and the out-of-line members in alphabetical order. The header-inline members are emitted first by
// UIBlinkingButton.cpp (it sorts before UIButton.cpp). UICheckBox.cpp's inline block starts at 0x1005ef4b.
//
// UIButton is whole in Rust (rust/crates/ee_ui/src/buttonobj.rs, R6 ui round 11): the members below forward to the
// Rust object, which calls out through ee_ui_button_host (UIControl's members, the virtuals ButtonDown, ButtonUp,
// MouseOver, MouseMove, SetDirty and the captures, the bitmaps, the texts, the sound manager). Its subclasses' records
// are the Rust button's too (R6 ui round 12: checkobj.rs, variantobj.rs); the host answers their ops (the label, the
// radio walk and Check/UnCheck, the polls, the slider) and the subclasses forward through UIButtonRust.h.
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
EE_UI_CASE(button_slider_value) { return reinterpret_cast<UISlider*>(a)->GetCurrentRangeValue(); }
EE_UI_VOID(button_slider_set_value) { reinterpret_cast<UISlider*>(a)->SetCurrentRangeValue((long)b); }
EE_UI_VOID(button_bitmap_stretch) { reinterpret_cast<GEBitmap*>(a)->ComputeStretchedCoordsAndScale(UIObjHost::Rect(f), UIObjHost::Rect(f), UIObjHost::Rect(f + 4), s->GetScreenIndex()); }
EE_UI_VOID(button_label_set_text) { reinterpret_cast<UITextLabel*>(a)->SetText(*reinterpret_cast<const UWideString*>(b)); }
#undef s

// C:\EEX\Low-Level Engine\User Interface\UIComboBox.cpp
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

// C:\EEX\Low-Level Engine\User Interface\UIControl.cpp
// 0x10064ed6..0x10065f69: the U2D key init chains, the sDarkBorder/sLightBorder init chains, the ctor first, then the
// out-of-line members in alphabetical order, then the map<unsigned long, UIEvent*> COMDATs (~_Tree, clear, _Dec).
// UIControl's header-inline members are emitted first by UIBackground.cpp. UIControlsTable.cpp's inline block
// starts at 0x10065f69.
// the record's rectangles are read as GERectangle (bottom, left, right, top)
static_assert(sizeof(EeUiRect) == sizeof(GERectangle), "EeUiRect is GERectangle's layout");

// The record's calls out (ee_ui controlobj.rs): the form's and manager's, the virtuals, the rhombi; the event's is
// Rust's (lle_control.rs, the host ee_ui_control_host)
#define ctl static_cast<UIControl*>(inObj)
#define theForm ctl->GetForm()
EE_UI_CASE(ctl_form_control) { return (intptr_t)theForm->GetControl((unsigned long)a); }
EE_UI_CASE(ctl_slot_left_down) { return theForm->GetControl((unsigned long)a)->mLeftButtonDown; }
EE_UI_CASE(ctl_slot_right_down) { return theForm->GetControl((unsigned long)a)->mRightButtonDown; }
EE_UI_CASE(ctl_group_enabled) { return theForm->GetEntityGroupById((unsigned long)a)->GetIsEnabled(); }
EE_UI_CASE(ctl_current_form) { return (intptr_t)theForm->GetManager()->GetCurrentForm(); }
EE_UI_CASE(ctl_keyboard_captured) { return (intptr_t)theForm->GetKeyboardCaptured(); }
EE_UI_CASE(ctl_mouse_captured) { return (intptr_t)theForm->GetMouseCaptured(); }
EE_UI_VOID(ctl_set_keyboard_captured) { theForm->SetKeyboardCaptured(reinterpret_cast<UIControl*>(a)); }
EE_UI_VOID(ctl_set_mouse_captured) { theForm->SetMouseCaptured(reinterpret_cast<UIControl*>(a)); }
EE_UI_VOID(ctl_hide) { ctl->HideControl(); }
EE_UI_VOID(ctl_add_dirty) { theForm->GetManager()->AddDirtyRectangle(UIObjHost::Rect(f), theForm); }
EE_UI_VOID(ctl_tab) { theForm->Tab(); }
EE_UI_VOID(ctl_add_event) { theForm->GetManager()->AddEventToTrigger(reinterpret_cast<UIEvent*>(a), theForm, ctl); }
EE_UI_VOID(ctl_help_string) { theForm->GetManager()->GetStringTable()->GetString((unsigned long)b, *reinterpret_cast<UWideString*>(a)); }
EE_UI_CASE(ctl_tab_stop) { return theForm->GetTabStop(); }
EE_UI_CASE(ctl_tabbing_on) { return theForm->GetTabbingOn(); }
EE_UI_CASE(ctl_form_surfaces) { return theForm->GetManager()->GetNumberOfSurfaces(); }
EE_UI_VOID(ctl_set_dirty) { ctl->SetDirty(a != 0); }
EE_UI_VOID(ctl_form_ctl_set_dirty) { reinterpret_cast<UIControl*>(a)->SetDirty(b != 0); }
EE_UI_VOID(ctl_virt_set_location) { ctl->SetLocation(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(ctl_set_display_mode) { ctl->SetDisplayMode((unsigned long)a); }
EE_UI_VOID(ctl_release_mouse) { ctl->ReleaseCapturedMouse(); }
EE_UI_VOID(ctl_set_bg_opacity) { ctl->SetBgOpacity(f[0]); }
EE_UI_VOID(ctl_part_set_bg_color) { ctl->SetBgColor(GEColor(f[0], f[1], f[2])); }
EE_UI_VOID(ctl_bg_opacity) { f[0] = ctl->GetBgOpacity(); }
EE_UI_VOID(ctl_tabbed_to) { ctl->TabbedTo(); }
EE_UI_VOID(ctl_draw_copied_bg) { ctl->GetRasterizer()->Draw(ctl->CopiedBgRhombus(), ctl->GetScreenIndex()); }
EE_UI_CASE(ctl_virt_draw_border) { return ctl->DrawBorder(UIObjHost::Rect(f), (unsigned long)a); }
EE_UI_CASE(ctl_virt_mouse_down) { return ctl->MouseLButtonDown(); }
EE_UI_VOID(ctl_bg_rhombus_opacity) { ctl->BgRhombus()->GetOpacityPerVertex(f[0], f[0], f[0], f[0]); }
#undef theForm
#undef ctl

// The first control's key lookup (lle_control.rs): the statics in its order, the one-statement shims
#define CTL(x) reinterpret_cast<UIControl*>(x)
extern "C" bool ee_uicpp_control_keys_ready() { return UIControl::Key(EeUiKey::Space) != NULL; }
extern "C" void ee_uicpp_control_key_fetch(uint32_t inSlot, uint32_t inCode)
{ gISKeyboard->GetEntity(inCode, UIControl::Key((EeUiKey)inSlot)); }
extern "C" uintptr_t ee_uicpp_mouse_l_button() { return (uintptr_t)UIManager::GetLMouseButton(); }
extern "C" uintptr_t ee_uicpp_mouse_r_button() { return (uintptr_t)UIManager::GetRMouseButton(); }
extern "C" bool ee_uicpp_control_left_down(uintptr_t inControl) { return CTL(inControl)->mLeftButtonDown; }
extern "C" bool ee_uicpp_control_right_down(uintptr_t inControl) { return CTL(inControl)->mRightButtonDown; }
extern "C" void ee_uicpp_control_set_left_down(uintptr_t inControl, bool inDown)
{ CTL(inControl)->mLeftButtonDown = inDown; }
extern "C" void ee_uicpp_control_set_right_down(uintptr_t inControl, bool inDown)
{ CTL(inControl)->mRightButtonDown = inDown; }
extern "C" long ee_uicpp_control_mouse_left_double(uintptr_t inControl)
{ return CTL(inControl)->MouseLButtonDoublePressed(); }
extern "C" long ee_uicpp_control_mouse_left_up(uintptr_t inControl) { return CTL(inControl)->MouseLButtonUp(); }
extern "C" long ee_uicpp_control_mouse_left_down(uintptr_t inControl) { return CTL(inControl)->MouseLButtonDown(); }
extern "C" long ee_uicpp_control_mouse_right_up(uintptr_t inControl) { return CTL(inControl)->MouseRButtonUp(); }
extern "C" long ee_uicpp_control_mouse_right_down(uintptr_t inControl) { return CTL(inControl)->MouseRButtonDown(); }
extern "C" long ee_uicpp_control_mouse_move(uintptr_t inControl, uintptr_t inLocation)
{ return CTL(inControl)->MouseMove(*reinterpret_cast<const GE2DPoint*>(inLocation)); }

// the background rhombi's new and delete (ee_ui lle_control.rs: the constructor and destructor)
extern "C" void* ee_uicpp_rhombus_new() { return new GERhombus; }
extern "C" void ee_uicpp_rhombus_delete(void* inRhombus) { delete static_cast<GERhombus*>(inRhombus); }

// The ops every whole object's host shares (ee_ui obj.rs): UIControl's members on the object
#define ctl static_cast<UIControl*>(inObj)
EE_UI_CASE(obj_base_copy_data) { return ctl->UIControl::CopyData(); }
EE_UI_CASE(obj_base_draw_polygons) { return ctl->UIControl::DrawPolygons(); }
EE_UI_CASE(obj_base_draw_text) { return ctl->UIControl::DrawTextA(); }
EE_UI_VOID(obj_base_set_location) { ctl->UIControl::SetLocation(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(obj_set_dirty) { ctl->SetDirty(a != 0); }
EE_UI_VOID(obj_rect) { UIObjHost::PutRect(f, ctl->GetRectangle()); }
EE_UI_VOID(obj_mouse_location) { *reinterpret_cast<GE2DPoint*>(f) = ctl->GetMouseLocation(); }
EE_UI_VOID(obj_capture_mouse) { ctl->CaptureMouse(); }
EE_UI_VOID(obj_release_mouse) { ctl->ReleaseCapturedMouse(); }
EE_UI_VOID(obj_fire_trigger) { ctl->FireTriggerEvent((unsigned long)a); }
EE_UI_CASE(obj_border_thickness) { return ctl->mBorderThickness; }
EE_UI_CASE(obj_width) { return ctl->GetWidth(); }
EE_UI_CASE(obj_index) { return ctl->mIndex; }
EE_UI_CASE(obj_border_type) { return ctl->mBorderType; }
EE_UI_VOID(obj_set_border_type) { ctl->mBorderType = (unsigned long)a; }
EE_UI_CASE(obj_tab_stop) { return ctl->GetForm()->GetTabStop(); }
EE_UI_CASE(obj_tabbing_on) { return ctl->GetForm()->GetTabbingOn(); }
EE_UI_VOID(obj_set_tab_stop) { ctl->GetForm()->SetTabStop((unsigned long)a); }
EE_UI_CASE(obj_tabable) { return ctl->GetTabable(); }
EE_UI_CASE(obj_has_mouse_capture) { return ctl->GetHasMouseCapture(); }
EE_UI_CASE(obj_right_button_down) { return ctl->GetRightMouseButtonDown(); }
EE_UI_VOID(obj_capture_keyboard) { ctl->CaptureKeyboard(); }
EE_UI_VOID(obj_release_keyboard) { ctl->ReleaseCapturedKeyboard(); }
EE_UI_CASE(obj_manager_bitmap) { return (intptr_t)ctl->GetForm()->GetManager()->GetBitmap(ee_ui_obj_manager_bitmap(a)); }
EE_UI_VOID(obj_copied_draw_rect) { UIObjHost::PutRect(f, ctl->GetCopiedDrawRectangle()); }
EE_UI_VOID(obj_base_set_rectangle) { ctl->UIControl::SetRectangle(UIObjHost::Rect(f)); }
EE_UI_CASE(obj_control_count) { return ctl->GetForm()->GetControlCount(); }
EE_UI_CASE(obj_always_dirty) { return ctl->GetAlwaysDirty(); }
EE_UI_VOID(obj_set_always_dirty) { ctl->SetAlwaysDirty(a != 0); }
EE_UI_VOID(obj_set_display_mode) { ctl->SetDisplayMode((unsigned long)a); }
EE_UI_VOID(obj_bg_opacity) { f[0] = ctl->GetBgOpacity(); }
EE_UI_VOID(obj_border_opacity) { f[0] = ctl->GetBorderOpacity(); }
EE_UI_VOID(obj_set_bg_opacity) { ctl->SetBgOpacity(f[0]); }
EE_UI_VOID(obj_set_border_opacity) { ctl->SetBorderOpacity(f[0]); }
#undef ctl
// The ops on the bitmaps and texts a whole object holds by address (ee_ui obj.rs): a the object, b an argument
#define ctl static_cast<UIControl*>(inObj)
#define theBitmap reinterpret_cast<GEBitmap*>(a)
#define theText reinterpret_cast<GEText*>(a)
EE_UI_VOID(obj_bitmap_reference) { theBitmap->Reference(); }
EE_UI_VOID(obj_bitmap_release) { theBitmap->Dereference(); }
EE_UI_CASE(obj_bitmap_texture_width) { return theBitmap->GetTexture(ctl->GetScreenIndex())->GetWidth(); }
EE_UI_CASE(obj_string_length) { return reinterpret_cast<const UWideString*>(a)->GetLength(); }
EE_UI_CASE(obj_text_create) { return (intptr_t) new GEText(reinterpret_cast<GEFont*>(a)); }
EE_UI_VOID(obj_text_delete) { delete theText; }
EE_UI_VOID(obj_text_set_font) { theText->SetFont(reinterpret_cast<GEFont*>(b)); }
EE_UI_CASE(obj_text_length) { return theText->GetText().GetLength(); }
EE_UI_VOID(obj_text_extent) { *reinterpret_cast<GERectangle*>(f) = theText->GetTextExtent(); }
EE_UI_VOID(obj_text_copy) { *theText = *reinterpret_cast<GEText*>(b); }
EE_UI_VOID(obj_text_set_text) { theText->SetText(*reinterpret_cast<const UWideString*>(b)); }
EE_UI_VOID(obj_text_set_location) { theText->SetLocation(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(obj_text_set_fg_color) { theText->SetFgColor(GEColor(f[0], f[1], f[2])); }
EE_UI_CASE(obj_draw_clipped_text) { return ctl->GetForm()->DrawClippedText(UIObjHost::Rect(f), theText, ctl); }
#undef theText
#undef theBitmap
#undef ctl
// The ops on a part a whole object owns (ee_ui obj.rs): another control's virtuals, by address
#define part reinterpret_cast<UIControl*>(a)
EE_UI_VOID(obj_delete_part) { delete part; }
EE_UI_CASE(obj_part_copy_data) { return part->CopyData(); }
EE_UI_CASE(obj_part_draw_polygons) { return part->DrawPolygons(); }
EE_UI_CASE(obj_part_draw_text) { return part->DrawTextA(); }
EE_UI_VOID(obj_part_rect) { UIObjHost::PutRect(f, part->GetRectangle()); }
EE_UI_CASE(obj_part_mouse_down) { return part->MouseLButtonDown(); }
EE_UI_CASE(obj_part_mouse_up) { return part->MouseLButtonUp(); }
EE_UI_CASE(obj_part_mouse_move) { return part->MouseMove(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(obj_part_set_location) { part->SetLocation(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(obj_part_set_rectangle) { part->SetRectangle(UIObjHost::Rect(f)); }
EE_UI_VOID(obj_part_set_bg_color) { part->SetBgColor(GEColor(f[0], f[1], f[2])); }
EE_UI_VOID(obj_part_set_bg_opacity) { part->SetBgOpacity(f[0]); }
#undef part

// the key predicates' calls (lle_control.rs key_pressed)
extern "C" long ee_uicpp_entity_long_value(uintptr_t inEntity) { return reinterpret_cast<ISEntity*>(inEntity)->GetValue(); }
extern "C" bool ee_uicpp_num_lock_off() { return UIForm::GetNumLockOff(); }

// C:\EEX\Low-Level Engine\User Interface\UIControlsTable.cpp
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

// C:\EEX\Low-Level Engine\User Interface\UIEditBox.cpp
// 0x10069d6b..0x1006de9d: the U2D key init chains (U2DRectangle.h, via the rasterizer header), the ctor, the dtor
// and the out-of-line members in alphabetical order. The header-inline members, the implicit copy members and ??_E
// were emitted first by UIComboBox.cpp. UIEMouseWheelScroll.cpp starts at 0x1006de9d.
//
// UIEditBox is whole in Rust (rust/crates/ee_ui/src/editobj.rs, R6 ui round 14): the members below forward to the
// Rust object, which calls out through UIEditBoxRust::Op (UIControl's members, the box's virtuals, the keys, the
// clipboard, the scroll bar, the texts, lines, rhombus and polls it holds by address). The text model (edit.rs), the
// key dispatch (edit_keys.rs) and the drawing's ranges (edit_draw.rs) are its algorithms.
#define e static_cast<UIEditBox*>(inObj)
#define theBar reinterpret_cast<UIScrollBar*>(a)
#define theLine reinterpret_cast<GELine*>(a)
#define theRhombus reinterpret_cast<GERhombus*>(a)
#define theText reinterpret_cast<GEText*>(a)
EE_UI_VOID(edit_button_down) { e->ButtonDown(); }
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

// C:\EEX\Low-Level Engine\User Interface\UIFileListBox.cpp
// The original units: UIFileListBox.cpp 0x1006fae7..0x1006ff58 and UIFileComboBox.cpp 0x1006f4ef..0x1006fae7.
//
// UIFileListBox and UIFileComboBox are whole in Rust (rust/crates/ee_ui/src/filelistobj.rs, R6 ui round 21): the
// members below forward to the Rust object, which calls out through UIFileListHost (the list's own methods, a
// directory search, the path's lookup and the file's delete, the path's references). UIFileComboBox.cpp's
// forwarders live here too.
// The Rust file lists' calls out (EeUiOp 650..667; ee_ui lle_hosts.rs): a directory search (a), the path, the list's
// own methods (the list box's or the combo box's: FILE_LIST, FILE_COMBO), the lines' bytes into the Rust buffer b of
// c bytes
struct UIFileSearch {                       // the original's locals
    FSFileSpec mFile;
    FSFileSpec mDirectory;
};
#define search reinterpret_cast<UIFileSearch*>(a)
#define path reinterpret_cast<FSPath*>(a)
EE_UI_CASE(fl_search_new) { return (intptr_t) new UIFileSearch; }
EE_UI_VOID(fl_search_delete) { delete search; }
EE_UI_CASE(fl_dir_count) { return path->GetDirectoryCount(); }
EE_UI_VOID(fl_dir_get) { reinterpret_cast<FSPath*>(b)->GetDirectory((unsigned long)c, search->mDirectory); }
EE_UI_CASE(fl_first) { return search->mDirectory.GetFirstFile(search->mFile); }
EE_UI_CASE(fl_next) { return search->mDirectory.GetNextFile(search->mFile); }
EE_UI_VOID(fl_rewind) { search->mDirectory.GetFirstFile(search->mFile); }
EE_UI_CASE(fl_name_length) { return strlen(search->mFile.GetFile()); }
EE_UI_VOID(fl_name_copy) { snprintf(reinterpret_cast<char*>(b), (size_t)c, "%s", (const char*)search->mFile.GetFile()); }
EE_UI_VOID(fl_path_reference) { path->Reference(); }
EE_UI_VOID(fl_path_release) { path->Dereference(); }
#undef path
#undef search
extern "C" intptr_t ee_uicpp_fl_find_file(intptr_t inName, intptr_t inPath, FSFileSpec** outFile)
{ return reinterpret_cast<FSPath*>(inPath)->FindFile(UString(reinterpret_cast<const char*>(inName)), *outFile); }
extern "C" intptr_t ee_uicpp_fl_file_delete(FSFileSpec* inFile) { return inFile->Delete(); }
extern "C" void ee_uicpp_fl_file_free(FSFileSpec* inFile) { delete inFile; }
#define FILE_LIST static_cast<UIFileListBox*>(inObj)
#define FILE_COMBO static_cast<UIFileComboBox*>(inObj)
static UWideString sFileLine;   // the combo box's appended line (its AppendLine takes a reference)
EE_UI_VOID(fl_list_delete_all) { FILE_LIST->DeleteAllEntries(); }
EE_UI_VOID(fl_list_append) { FILE_LIST->AppendLine(UWideString(reinterpret_cast<const char*>(a))); }
// the file lists' case-insensitive alphabetical order (the original's UWSNoCaseAlphaSort)
static bool UINoCaseLess(const UWideString& inLeft, const UWideString& inRight) { return ee_ui_text_less_no_case(inLeft, inRight); }
EE_UI_VOID(fl_list_insert_sorted) { FILE_LIST->InsertLine(UWideString(reinterpret_cast<const char*>(a)), 0, &UINoCaseLess); }
EE_UI_CASE(fl_list_selection) { return FILE_LIST->GetCurrentSelection(*reinterpret_cast<unsigned long*>(f)); }
EE_UI_CASE(fl_list_line_length) { return strlen(FILE_LIST->GetData((unsigned long)a)); }
EE_UI_VOID(fl_list_line_copy) { snprintf(reinterpret_cast<char*>(b), (size_t)c, "%s", (const char*)FILE_LIST->GetData((unsigned long)a)); }
EE_UI_VOID(fl_list_delete_line) { FILE_LIST->DeleteLine((unsigned long)a); }
EE_UI_VOID(fl_combo_delete_all) { FILE_COMBO->DeleteAllEntries(); }
EE_UI_VOID(fl_combo_append) { FILE_COMBO->AppendLine(sFileLine = reinterpret_cast<const char*>(a)); }
EE_UI_VOID(fl_combo_insert_sorted) { FILE_COMBO->GetList()->InsertLine(UWideString(reinterpret_cast<const char*>(a)), 0, &UINoCaseLess); }
EE_UI_CASE(fl_combo_selection) { return FILE_COMBO->GetCurrentSelection(*reinterpret_cast<unsigned long*>(f)); }
EE_UI_CASE(fl_combo_line_length) { return strlen(FILE_COMBO->GetList()->GetData((unsigned long)a)); }
EE_UI_VOID(fl_combo_line_copy) { snprintf(reinterpret_cast<char*>(b), (size_t)c, "%s", (const char*)FILE_COMBO->GetList()->GetData((unsigned long)a)); }
EE_UI_VOID(fl_combo_delete_line) { FILE_COMBO->DeleteLine((unsigned long)a); }
#undef FILE_COMBO
#undef FILE_LIST

// C:\EEX\Low-Level Engine\User Interface\UIFontTable.cpp
// 0x1006ff58..0x1007042f: the U2D key init chains, the sFontTable/sFont init chains (guarded atexit dtors), the ctor,
// the static members, vector<GEFont*>::push_back last. The dtor and the implicit copy members are emitted by
// UIControlsTable.cpp. UIForm.cpp's key chains start at 0x1007042f.
//
// UIFontTable is whole in Rust (rust/crates/ee_ui/src/fonttableobj.rs, R6 ui round 18): the records, the shared
// font list, Create, CreateClosestFont, GetFont, FindOrCreateFont and DeleteAll; this unit keeps the forwarders and
// the host of their calls out (UIFontTableHost: the file, the string table's face names, the GEFonts).
// The Rust font table's calls out (EeUiOp, obj.rs; ee_ui fonttableobj.rs): a the file, manager, name or font
static UWideString sFace;
#define theFile reinterpret_cast<FSFileSpec*>(a)
#define theFont reinterpret_cast<GEFont*>(a)
#define theName (*reinterpret_cast<const UString*>(b))
EE_UI_VOID(font_tbl_file_open) { theFile->Open(kFileAccessRead); }
EE_UI_VOID(font_tbl_file_close) { theFile->Close(); }
EE_UI_CASE(font_name)
{
    reinterpret_cast<UIManager*>(a)->GetStringTable()->GetString((unsigned long)b, sFace);
    UString* theCopy = new UString;
    theCopy->Copy(sFace, 0, sFace.GetLength(), 0, true);
    return (intptr_t)theCopy;
}
EE_UI_VOID(font_name_delete) { delete reinterpret_cast<UString*>(a); }
EE_UI_CASE(font_compare) { return theFont->CompareFont(theName, (unsigned long)c, (unsigned long)(uint32_t)d, (unsigned long)((uint64_t)d >> 32)); }
EE_UI_VOID(font_reference) { theFont->Reference(); }
EE_UI_CASE(font_create) { return (intptr_t) new GEFont(*reinterpret_cast<const UString*>(a), (unsigned long)d, (unsigned long)b, (unsigned long)c); }
EE_UI_VOID(font_release) { theFont->Dereference(); }
#undef theName
#undef theFont
#undef theFile
// Creates an empty font table entry

// C:\EEX\Low-Level Engine\User Interface\UIForm.cpp
// 0x1007042f..0x10075330: the U2D key init chains, UIIMECandidate's implicit copy members and ??_E (UIForm.cpp is the
// first includer of UIIMECandidate.h), the init chain of an unreferenced file-scope UString, then the ctors, the dtor
// and the members in the original's order (roughly alphabetical), then the template COMDATs (hot key vectors, sort,
// the dirty-rectangle deque). UIFullBackground.cpp starts at 0x10075330.
//
// The class is whole in Rust (rust/crates/ee_ui/src/formobj.rs, R6 ui round 16): the methods are forwarders to the
// Rust form (UIForm::Call), the ctors and dtor make and free it, and UIForm::HostOp answers its calls out (the
// controls', groups', backgrounds', hot keys' and events' members and virtuals, the manager, the input keys).
// (the original's unreferenced key string, kUIFormKey, whose init chain 0x10070706 was its only user, is gone)

static GERectangle UIFormRect(const float* inF) { return GERectangle(inF[1], inF[3], inF[2], inF[0]); }

// The form's calls out of more than one call are Rust's (ee_ui lle_form.rs, the host ee_ui_form_host), over the
// one-statement shims below (and UIManager.cpp's)
#define FORM(x) reinterpret_cast<UIForm*>(x)
#define MGR(x) reinterpret_cast<UIManager*>(x)
#define CTL(x) reinterpret_cast<UIControl*>(x)
#define HOTKEY(x) reinterpret_cast<UIHotKey*>(x)
#define EVENT(x) reinterpret_cast<UIEvent*>(x)
static UWideString sUIFormHelpString;   // FormHelpGet's string, which FormHelpSetText sets (the UI thread's)
extern "C" void ee_uicpp_form_help_clear() { sUIFormHelpString = ""; }
extern "C" bool ee_uicpp_control_help_string(uintptr_t inControl)
{ return CTL(inControl)->GetHelpString(sUIFormHelpString); }
extern "C" void ee_uicpp_hot_key_set_just_pressed(uintptr_t inKey, uintptr_t inEvent)
{ HOTKEY(inKey)->SetJustPressedEvent(EVENT(inEvent)); }
extern "C" void ee_uicpp_hot_key_set_pressed(uintptr_t inKey, uintptr_t inEvent)
{ HOTKEY(inKey)->SetPressedEvent(EVENT(inEvent)); }
extern "C" void ee_uicpp_hot_key_set_just_unpressed(uintptr_t inKey, uintptr_t inEvent)
{ HOTKEY(inKey)->SetJustUnpressedEvent(EVENT(inEvent)); }
extern "C" void ee_uicpp_hot_key_set_double_pressed(uintptr_t inKey, uintptr_t inEvent)
{ HOTKEY(inKey)->SetDoublePressedEvent(EVENT(inEvent)); }
extern "C" void ee_uicpp_form_fetch_tab_key() { gISKeyboard->GetEntity(kKeyTab, UIControl::Key(EeUiKey::Tab)); }
extern "C" void ee_uicpp_form_fetch_shift_key() { gISKeyboard->GetEntity(kKeyShift, UIControl::Key(EeUiKey::Shift)); }
extern "C" void ee_uicpp_event_uninitialize(uintptr_t inEvent) { EVENT(inEvent)->Uninitialize(); }
extern "C" void ee_uicpp_event_initialize(uintptr_t inEvent) { EVENT(inEvent)->Initialize(); }
extern "C" uint32_t ee_uicpp_entity_value(uintptr_t inEntity)
{ return (uint32_t)reinterpret_cast<ISEntity*>(inEntity)->GetValue(); }
extern "C" bool ee_uicpp_entity_just_pressed(uintptr_t inEntity)
{ return reinterpret_cast<ISEntity*>(inEntity)->JustPressed(); }
extern "C" bool ee_uicpp_entity_just_unpressed(uintptr_t inEntity)
{ return reinterpret_cast<ISEntity*>(inEntity)->JustUnpressed(); }
extern "C" bool ee_uicpp_entity_just_double_pressed(uintptr_t inEntity)
{ return reinterpret_cast<ISEntity*>(inEntity)->JustDoublePressed(); }
extern "C" uint32_t ee_uicpp_form_screen(uintptr_t inForm) { return FORM(inForm)->GetManager()->GetScreen(); }
extern "C" uintptr_t ee_uicpp_form_keyboard_captured(uintptr_t inForm)
{ return (uintptr_t)FORM(inForm)->GetKeyboardCaptured(); }
extern "C" void ee_uicpp_control_ime_result(uintptr_t inControl, uintptr_t inText)
{ CTL(inControl)->AddIMEResultString(*reinterpret_cast<const UWideString*>(inText)); }
extern "C" long ee_uicpp_control_ime_change(uintptr_t inControl, uintptr_t inText, uintptr_t inAttributes, uint32_t inCursor)
{ return CTL(inControl)->ChangeIMECompositionString(*reinterpret_cast<const UWideString*>(inText), *reinterpret_cast<const UString*>(inAttributes), inCursor); }
extern "C" long ee_uicpp_control_ime_close(uintptr_t inControl) { return CTL(inControl)->CloseIMECompositionString(); }
extern "C" long ee_uicpp_control_ime_open(uintptr_t inControl) { return CTL(inControl)->OpenIMECompositionString(); }
extern "C" long ee_uicpp_control_translated_key(uintptr_t inControl, uintptr_t inKey, int32_t inRepeat)
{ return CTL(inControl)->UpdateTranslatedKey(reinterpret_cast<const char*>(inKey), (short)inRepeat); }
extern "C" uintptr_t ee_uicpp_mgr_create_form(uintptr_t inM, EeUiShimText inData)
{ return (uintptr_t)MGR(inM)->CreateForm(*reinterpret_cast<const UIDbDataForms*>(inData.ptr)); }
extern "C" void ee_uicpp_mgr_add_form(uintptr_t inM, uintptr_t inForm) { MGR(inM)->AddForm(FORM(inForm)); }
extern "C" void ee_uicpp_form_enable_group0(uintptr_t inForm) { FORM(inForm)->GetEntityGroup(0)->Enable(); }

// UIEntityGroup is whole in Rust (rust/crates/ee_ui/src/groupobj.rs, R6 ui round 21; the original unit
// UIEntityGroup.cpp, 0x1006e304..0x1006e5a8): the calls out of Disable/Enable on entity a, a button or a hot key
#define ENTITY_CONTROL static_cast<UIControl*>(reinterpret_cast<UIEntity*>(a))
EE_UI_CASE(group_is_button) { return reinterpret_cast<UIEntity*>(a)->GetType() == kControlTypeButton; }
EE_UI_CASE(group_shown) { return ENTITY_CONTROL->GetDisplayMode() == kDisplayModeShown; }
EE_UI_CASE(group_on_current_form) { return ENTITY_CONTROL->GetForm() == ENTITY_CONTROL->GetForm()->GetManager()->GetCurrentForm(); }
EE_UI_VOID(group_add_dirty) { ENTITY_CONTROL->GetForm()->GetManager()->AddDirtyRectangle(ENTITY_CONTROL->GetRectangle(), ENTITY_CONTROL->GetForm()); }
EE_UI_VOID(group_hide) { ENTITY_CONTROL->HideControl(); }
EE_UI_VOID(group_set_dirty) { ENTITY_CONTROL->SetDirty(true); }
EE_UI_CASE(group_tabbing) { return ENTITY_CONTROL->GetForm()->GetTabbingOn(); }
EE_UI_CASE(group_is_tab_stop) { return ENTITY_CONTROL->GetForm()->GetTabStop() == ENTITY_CONTROL->GetIndex(); }
EE_UI_VOID(group_tab) { ENTITY_CONTROL->GetForm()->Tab(); }
EE_UI_VOID(group_set_enabled) { static_cast<UIHotKey*>(reinterpret_cast<UIEntity*>(a))->SetEnabled(b != 0); }
#undef ENTITY_CONTROL

// The Rust form's calls out (EeUiOp, obj.rs): a is the control, group, background, hot key, event, table or other
// object the op names; rectangles and points travel in f
#define theForm static_cast<UIForm*>(inObj)
#define theControl reinterpret_cast<UIControl*>(a)
#define theGroup reinterpret_cast<UIEntityGroup*>(a)
#define theBackground reinterpret_cast<UIBackground*>(a)
#define theKey reinterpret_cast<UIHotKey*>(a)
#define theEvent reinterpret_cast<UIEvent*>(a)
#define theTable reinterpret_cast<UIControlsTable*>(a)
#define theRhombus reinterpret_cast<GERhombus*>(a)
EE_UI_CASE(form_ctl_type) { return theControl->GetType(); }
EE_UI_CASE(form_ctl_entity_type) { return theControl->UIEntity::GetType(); }
EE_UI_CASE(form_ctl_display_mode) { return theControl->GetDisplayMode(); }
EE_UI_CASE(form_ctl_dirty) { return theControl->GetIsDirty(); }
EE_UI_CASE(form_ctl_draw_count) { return theControl->GetDrawCount(); }
EE_UI_CASE(form_ctl_index) { return theControl->GetIndex(); }
EE_UI_CASE(form_ctl_id) { return theControl->GetId(); }
EE_UI_CASE(form_ctl_tabable) { return theControl->GetTabable(); }
EE_UI_CASE(form_ctl_checked) { return static_cast<UIRadioButton*>(theControl)->GetIsChecked(); }
EE_UI_CASE(form_ctl_ime_enabled) { return theControl->GetIMEEnabled(); }
EE_UI_VOID(form_part_rect) { UIObjHost::PutRect(f, theControl->GetRectangle()); }
EE_UI_VOID(form_part_copied_draw_rect) { UIObjHost::PutRect(f, theControl->GetCopiedDrawRectangle()); }
EE_UI_CASE(form_part_copy_data) { return theControl->CopyData(); }
EE_UI_CASE(form_part_draw_polygons) { return theControl->DrawPolygons(); }
EE_UI_CASE(form_part_draw_text) { return theControl->DrawTextA(); }
EE_UI_VOID(form_part_set_location) { theControl->SetLocation(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(form_delete_part) { delete theControl; }
EE_UI_VOID(form_ctl_set_dirty) { theControl->SetDirty(b != 0); }
EE_UI_VOID(form_ctl_set_display_mode) { theControl->SetDisplayMode((unsigned long)b); }
EE_UI_VOID(form_ctl_hide) { theControl->HideControl(); }
EE_UI_VOID(form_ctl_set_string_id) { theControl->SetStringTableID((unsigned long)b); }
EE_UI_VOID(form_ctl_tabbed_from) { theControl->TabbedFrom(); }
EE_UI_VOID(form_ctl_tabbed_to) { theControl->TabbedTo(); }
EE_UI_CASE(form_ctl_update_keyboard) { return theControl->UpdateKeyboard(); }
EE_UI_CASE(form_ctl_update_mouse) { return theControl->UpdateMouse(theForm->GetMouseLocation()); }
EE_UI_CASE(form_ctl_wheel_turned) { return theControl->MouseWheelTurned((long)b); }
EE_UI_VOID(form_ctl_fire_trigger) { theControl->FireTriggerEvent((unsigned long)b); }
EE_UI_VOID(form_ctl_release_mouse) { theControl->ReleaseCapturedMouse(); }
EE_UI_VOID(form_ctl_release_keyboard) { theControl->ReleaseCapturedKeyboard(); }
EE_UI_VOID(form_help_set_text) { static_cast<UITextLabel*>(theControl)->SetText(sUIFormHelpString, true); }
EE_UI_CASE(form_label_fit) { return static_cast<UITextLabel*>(theControl)->GetFitRectToText(); }
EE_UI_CASE(form_group_enabled) { return theGroup->GetIsEnabled(); }
EE_UI_CASE(form_group_count) { return theGroup->GetEntityCount(); }
EE_UI_CASE(form_group_entity) { return (intptr_t) static_cast<UIControl*>(theGroup->GetEntity((unsigned long)b)); }
EE_UI_CASE(form_group_new) { return (intptr_t) new UIEntityGroup; }
EE_UI_VOID(form_group_delete) { delete theGroup; }
EE_UI_VOID(form_group_add) { theGroup->AddEntity(reinterpret_cast<UIControl*>(b)); }
EE_UI_VOID(form_group_remove) { theGroup->RemoveEntity(reinterpret_cast<UIControl*>(b)); }
EE_UI_CASE(form_bg_priority) { return theBackground->GetDrawPriority(); }
EE_UI_CASE(form_bg_id) { return theBackground->GetBackgroundID(); }
EE_UI_CASE(form_bg_visible) { return theBackground->GetIsVisible(); }
EE_UI_VOID(form_bg_rect) { UIObjHost::PutRect(f, theBackground->GetRectangle()); }
EE_UI_VOID(form_bg_open) { theBackground->Open(); }
EE_UI_VOID(form_bg_close) { theBackground->Close(); }
EE_UI_VOID(form_bg_delete) { delete theBackground; }
EE_UI_CASE(form_bg_draw) { return theBackground->DrawBackground(UIFormRect(f)); }
EE_UI_VOID(form_bg_load) { theBackground->LoadBackground(); }
EE_UI_VOID(form_bg_unload) { theBackground->UnloadBackground(); }
EE_UI_VOID(form_bg_relayout) { theBackground->RelayoutBackground(); }
EE_UI_CASE(form_hot_key_always) { return theKey->GetAlwaysFire(); }
EE_UI_CASE(form_hot_key_keys) { return theKey->GetKeyCount(); }
EE_UI_CASE(form_hot_key_key) { return (intptr_t)theKey->mKey; }
EE_UI_CASE(form_hot_key_mod0) { return (intptr_t)theKey->mModifiers[0]; }
EE_UI_CASE(form_hot_key_mod1) { return (intptr_t)theKey->mModifiers[1]; }
EE_UI_CASE(form_hot_key_mod2) { return (intptr_t)theKey->mModifiers[2]; }
EE_UI_CASE(form_hot_key_copy) { return (intptr_t) new UIHotKey(theForm, *theKey); }
EE_UI_VOID(form_hot_key_delete) { delete theKey; }
EE_UI_VOID(form_hot_key_set_enabled) { theKey->mEnabled = b != 0; }
EE_UI_CASE(form_hot_key_fire) { return theKey->FireEvents(); }
// the temporary hot key AddHotKey builds (lle_hosts.rs form_hot_key_temp): made, on this form, its key b
EE_UI_CASE(form_hot_key_new) { return (intptr_t) new UIHotKey; }
EE_UI_VOID(form_hot_key_set_form) { theKey->mForm = theForm; }
EE_UI_VOID(form_hot_key_set_key) { theKey->SetKey((unsigned long)b); }
EE_UI_VOID(form_hot_key_set_modifier) { theKey->SetModifier((unsigned long)b, (unsigned char)c); }
EE_UI_VOID(form_hot_key_set_always) { theKey->mAlwaysFire = b != 0; }
// an input entity by key or button a into the slot b (lle_hosts.rs form_entity)
EE_UI_VOID(form_key_entity_get) { gISKeyboard->GetEntity((unsigned long)a, *reinterpret_cast<ISEntity**>(b)); }
EE_UI_CASE(form_keys_ready) { return UIControl::Key(EeUiKey::Tab) != NULL; }
EE_UI_CASE(form_tab_just_pressed) { return UIControl::Key(EeUiKey::Tab)->JustPressed(); }
EE_UI_CASE(form_shift_value) { return UIControl::Key(EeUiKey::Shift)->GetValue(); }
EE_UI_CASE(form_event_process) { return theEvent->ProcessEvent(theForm, NULL); }
EE_UI_CASE(form_wheel_event_new) { return (intptr_t) new UIEMouseWheelScroll; }
EE_UI_CASE(form_mouse_wheel) { return UIManager::GetMouseWheel() != NULL; }
// the mouse wheel's scroll step: -1 when rolled forward, 1 when rolled back, 0 when still
EE_UI_CASE(form_wheel_amount) { return ee_ui_manager_wheel_scroll_amount(UIControl::Key(EeUiKey::MouseWheel)->GetNormalizedValue()); }
EE_UI_VOID(form_mouse_entity_get) { gISMouse->GetEntity((unsigned long)a, *reinterpret_cast<ISEntity**>(b)); }
EE_UI_CASE(form_table) { return (intptr_t)UIControlsTable::GetUIControlsTable((unsigned long)a); }
EE_UI_CASE(form_table_form_index) { return theTable->GetFormIndex(); }
EE_UI_CASE(form_table_group) { return theTable->GetGroup(); }
EE_UI_CASE(form_table_visible) { return theTable->GetIsVisible() == true; }
EE_UI_CASE(form_table_type) { return theTable->GetControlType(); }
EE_UI_CASE(form_table_string_id) { return theTable->GetStringTableID(); }
EE_UI_CASE(form_table_create_control) { return (intptr_t)theTable->CreateControl(theForm->GetManager()); }
EE_UI_VOID(form_table_create_event) { theTable->CreateControlEvent(theForm); }
EE_UI_VOID(form_table_relayout) { theTable->RelayoutControl(reinterpret_cast<UIControl*>(b), theForm->GetManager()); }
EE_UI_CASE(form_surfaces) { return theForm->GetManager()->GetNumberOfSurfaces(); }
EE_UI_CASE(form_window_width) { return theForm->GetManager()->GetRasterizer()->GetWindow(theForm->GetManager()->GetScreen())->GetWidth(); }
EE_UI_CASE(form_window_height) { return theForm->GetManager()->GetRasterizer()->GetWindow(theForm->GetManager()->GetScreen())->GetHeight(); }
EE_UI_VOID(form_manager_process_events) { theForm->GetManager()->ProcessAllEventsForForm(theForm); }
EE_UI_VOID(form_manager_add_dirty) { theForm->GetManager()->AddDirtyRectangle(UIFormRect(f), theForm); }
EE_UI_VOID(form_music_stop) { theForm->GetManager()->StopMusic(); }
EE_UI_VOID(form_music_start) { theForm->GetManager()->StartMusic(UWideString((const char*)a)); }
EE_UI_CASE(form_create_form_event) { return (intptr_t)theForm->GetManager()->CreateFormEvent((unsigned long)a); }
EE_UI_VOID(form_manager_delete) { delete reinterpret_cast<UIFormManager*>(a); }
EE_UI_CASE(form_now) { return TMGetTime(); }
EE_UI_VOID(form_lock_delete) { TSScheduler::DestroyCriticalSection(reinterpret_cast<TSCriticalSection*>(a)); }
EE_UI_VOID(form_lock_enter) { reinterpret_cast<TSCriticalSection*>(a)->Enter(); }
EE_UI_VOID(form_lock_leave) { reinterpret_cast<TSCriticalSection*>(a)->Leave(); }
EE_UI_VOID(form_virt_help_location) { theForm->SetHelpTextPopupLocation(); }
EE_UI_CASE(form_virt_draw_background) { return theForm->DrawBackground(UIFormRect(f)); }
EE_UI_CASE(form_rhombus_create) { return (intptr_t) new GERhombus; }
EE_UI_VOID(form_rhombus_delete) { delete theRhombus; }
EE_UI_VOID(form_rhombus_label_color) { theRhombus->SetColor(static_cast<UITextLabel*>(reinterpret_cast<UIControl*>(b))->GetGEText()->GetFgColor()); }
#undef theForm
#undef theControl
#undef theGroup
#undef theBackground
#undef theKey
#undef theEvent
#undef theTable
#undef theRhombus

// Records the mouse location, finds the control under it, hides a tool tip the mouse enters, updates the mouse-over

// C:\EEX\Low-Level Engine\User Interface\UIGraph.cpp
// UIGraph (with UILineGraph and the post-game summary's EEUIPGSLineGraph, R6 game screens B round 3) is whole in
// Rust (rust/crates/ee_ui/src/graphobj.rs): the members below forward to the Rust object, which calls out through
// ee_ui_graph_host (UIControl's members, the texts, a line).
#define g static_cast<UIGraph*>(inObj)
#define theText reinterpret_cast<GEText*>(a)
#define theLine reinterpret_cast<GELine*>(a)
EE_UI_VOID(graph_text_set_bytes) { theText->SetText(UWideString(reinterpret_cast<const char*>(b))); }
EE_UI_VOID(graph_text_fg_color) { *reinterpret_cast<GEColor*>(f) = theText->GetFgColor(); }
EE_UI_VOID(graph_text_set_transparent) { theText->SetIsTransparent(b != 0); }
EE_UI_VOID(graph_set_bg_rectangle) { g->SetBgRectangle(UIObjHost::Rect(f)); }
EE_UI_CASE(graph_draw_border) { return g->DrawBorder(UIObjHost::Rect(f), (unsigned long)a); }
EE_UI_CASE(graph_line_create) { return (intptr_t) new GELine; }
EE_UI_VOID(graph_line_delete) { delete theLine; }
#undef theLine
#undef theText
#undef g

// C:\EEX\Low-Level Engine\User Interface\UIHotKey.cpp
// 0x10076f27..0x100774b9: the sPressedPoll/sHotKey init chains, then the out-of-line members (CreateHotKeys,
// DeleteHotKeys, the private CalculateKeyCount, GetHotKeyName, the Set*Event setters, SetModifier, FireEvents) and the
// vector-delete helper COMDAT last. The inline members are emitted by UIEMouseWheelScroll.cpp (0x1006e075..).
// UIIMECandidate.cpp's key chains start at 0x100774b9.
//
// UIHotKey is whole in Rust (rust/crates/ee_ui/src/hotkeyobj.rs, R6 ui round 20): the members forward to the Rust
// record, which calls out through ee_ui_hotkey_host (the keyboard's entities, the events, new and delete); the
// database records are read in Rust (hotkey.rs DbHotKey, 124 bytes).
// The UI database files' calls (FSFileSpec&): the hot keys' records (hotkeyobj.rs), the events' and the background
// table's (UIManager.cpp, UIBackgroundTable.cpp)
extern "C" void ee_uicpp_file_open(void* inFile) { static_cast<FSFileSpec*>(inFile)->Open(kFileAccessRead); }
extern "C" long ee_uicpp_file_read_count(void* inFile, unsigned long* outCount)
{
    return static_cast<FSFileSpec*>(inFile)->Read(*outCount);
}
static unsigned long sUIFileRead;   // the reads' byte counts (unread)
extern "C" long ee_uicpp_file_read(void* inFile, unsigned char* outData, unsigned long inLength)
{ return static_cast<FSFileSpec*>(inFile)->Read(outData, inLength, sUIFileRead); }
extern "C" void ee_uicpp_file_close(void* inFile) { static_cast<FSFileSpec*>(inFile)->Close(); }

// The Rust hot keys' calls out (EeUiOp 570..581): the object is the record (NULL for the table's own calls)
#define r static_cast<EeUiHotKeyFields*>(inObj)
#define theEntity reinterpret_cast<ISEntity*>(a)
#define theEvent reinterpret_cast<UIEvent*>(a)
EE_UI_CASE(hk_value) { return theEntity->GetValue(); }
EE_UI_CASE(hk_double_pressed) { return theEntity->JustDoublePressed(); }
EE_UI_CASE(hk_just_pressed) { return theEntity->JustPressed(); }
EE_UI_CASE(hk_just_unpressed) { return theEntity->JustUnpressed(); }
EE_UI_CASE(hk_keyboard_state) { return gISKeyboard->GetModifierState(); }
EE_UI_CASE(hk_modifier_state) { return ISKeyboard::ComputeModifierState((unsigned long)a); }
EE_UI_CASE(hk_key_index) { return theEntity->GetIndex(); }
EE_UI_VOID(hk_process) { theEvent->ProcessEvent(static_cast<UIForm*>(r->mForm), static_cast<UIHotKey*>(r)); }
EE_UI_CASE(hk_new) { return (intptr_t) static_cast<EeUiHotKeyFields*>(new UIHotKey); }
EE_UI_VOID(hk_delete) { delete static_cast<UIHotKey*>(reinterpret_cast<EeUiHotKeyFields*>(a)); }
extern "C" intptr_t ee_uicpp_hk_get_entity(intptr_t inKey, ISEntity** ioEntity) { return gISKeyboard->GetEntity((unsigned long)inKey, *ioEntity); }
#undef theEvent
#undef theEntity
#undef r

// the entities' names (ee_ui hotkey_ffi.rs: the hot key's display name)
extern "C" const char* ee_uicpp_entity_name(void* inEntity) { return static_cast<ISEntity*>(inEntity)->GetName(); }

// C:\EEX\Low-Level Engine\User Interface\UILBString.cpp
// 0x10078251..0x100785cd: the U2D key init chains, then DrawTextA and UpdateTranslatedKey (both Rust's: ee_ui
// listobj.rs); here the list boxes' shims. The class's inline members and every UIListBox<UWideString> member are
// emitted first by UIComboBox.cpp. UILineGraph.cpp's key chains start at 0x100785cd.
// The Rust list box's ops that do not depend on T (ee_ui listobj.rs; UIListBoxRust<T>::Op answers the rest):
// UIControl's, the scroll bar's (a part by address), the polls', the selection rhombi's and the focus line's
#define lst static_cast<UIControl*>(inObj)
#define theBar reinterpret_cast<UIScrollBar*>(a)
#define theLine reinterpret_cast<GELine*>(a)
#define theRhombus reinterpret_cast<GERhombus*>(a)
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
#undef theBar
#undef lst

// The string list's draw (ee_ui listobj.rs string_draw): its drawn text placed, given a drawn line, drawn clipped
#define LBS static_cast<UILBString*>(inObj)
EE_UI_VOID(lbs_text_at) { LBS->mCopiedText->SetLocation(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(lbs_text_line) { LBS->mCopiedText->SetText(LBS->mCopyLineArray[(size_t)a]); }
EE_UI_CASE(lbs_draw_clipped) { return LBS->GetForm()->DrawClippedText(UIObjHost::Rect(f), LBS->mCopiedText, LBS); }
#undef LBS

// C:\EEX\Low-Level Engine\User Interface\UIManager.cpp
// 0x10078c81..0x1007ba37: the U2D key init chains, the implicit copy members and ??_E of UIStretchedTiledBackground
// and UITBTiledBackground (UIManager.cpp was their first includer; both are UITiledBackground's tilings now), the
// ctor, the dtor and the out-of-line members in
// the original's order, then the vector/deque COMDATs. UIMovie.cpp starts at 0x1007ba37.
// The Rust manager's calls out (EeUiOp, obj.rs): a is the form, event, bitmap, texture, lock, sound instance or
// other object the op names; rectangles and points travel in f. The cases of more than one call are Rust's (ee_ui
// lle_manager.rs, the host ee_ui_manager_host), over the one-statement shims below.
static GERectangle UIManagerRect(const float* inF) { return GERectangle(inF[1], inF[3], inF[2], inF[0]); }
#define MGR(x) reinterpret_cast<UIManager*>(x)
#define TEX(x) reinterpret_cast<GETexture*>(x)
#define BMP(x) reinterpret_cast<GEBitmap*>(x)
#define UVS(x) reinterpret_cast<GETextureCoordinate*>(x)
#define FILE_SPEC(x) reinterpret_cast<FSFileSpec*>(x)
static UWideString sUIManagerKey;   // UpdateTranslatedKey's character (the UI thread's)
extern "C" uint32_t ee_uicpp_mgr_screen(uintptr_t inM) { return MGR(inM)->GetScreen(); }
extern "C" void ee_uicpp_mgr_rasterizer_screen(uintptr_t inM)
{ MGR(inM)->GetRasterizer()->GetScreen(MGR(inM)->GetScreen()); }
extern "C" uintptr_t ee_uicpp_mgr_window(uintptr_t inM)
{ return (uintptr_t)MGR(inM)->GetRasterizer()->GetWindow(MGR(inM)->GetScreen()); }
extern "C" float ee_uicpp_window_origin_x(uintptr_t inW)
{ return reinterpret_cast<GEWindow*>(inW)->GetClientOrigin().GetX(); }
extern "C" float ee_uicpp_window_origin_y(uintptr_t inW)
{ return reinterpret_cast<GEWindow*>(inW)->GetClientOrigin().GetY(); }
extern "C" uint32_t ee_uicpp_window_width(uintptr_t inW)
{ return (uint32_t)reinterpret_cast<GEWindow*>(inW)->GetWidth(); }
extern "C" uint32_t ee_uicpp_window_height(uintptr_t inW)
{ return (uint32_t)reinterpret_cast<GEWindow*>(inW)->GetHeight(); }
extern "C" void ee_uicpp_mgr_blending(uintptr_t inM, unsigned long* inSource, unsigned long* inDestination)
{ MGR(inM)->GetRasterizer()->GetBlending(*inSource, *inDestination, MGR(inM)->GetScreen()); }
extern "C" void ee_uicpp_key_copy(uintptr_t inKey)
{ sUIManagerKey.CopyString(reinterpret_cast<const char*>(inKey), 1); }
extern "C" long ee_uicpp_form_translated_key(uintptr_t inForm, int32_t inRepeat)
{ return reinterpret_cast<UIForm*>(inForm)->UpdateTranslatedKey(sUIManagerKey, (short)inRepeat); }
extern "C" void ee_uicpp_event_reference(uintptr_t inEvent) { reinterpret_cast<UIEvent*>(inEvent)->Reference(); }
extern "C" void ee_uicpp_event_dereference(uintptr_t inEvent) { reinterpret_cast<UIEvent*>(inEvent)->Dereference(); }
extern "C" uintptr_t ee_uicpp_callback_manager(uintptr_t inCallback, uintptr_t inM, uint32_t inId)
{ return (uintptr_t)reinterpret_cast<UIEvent* (*)(UIManager*, unsigned long)>(inCallback)(MGR(inM), inId); }
extern "C" uintptr_t ee_uicpp_callback_form(uintptr_t inCallback, uintptr_t inForm, uint32_t inId)
{ return (uintptr_t)reinterpret_cast<UIEvent* (*)(UIForm*, unsigned long)>(inCallback)(reinterpret_cast<UIForm*>(inForm), inId); }
extern "C" uintptr_t ee_uicpp_mgr_texture_manager(uintptr_t inM)
{ return (uintptr_t)MGR(inM)->GetRasterizer()->GetScreen(MGR(inM)->GetScreen())->GetTextureManager(); }
extern "C" uint32_t ee_uicpp_mgr_ui_depth(uintptr_t inM) { return (uint32_t)MGR(inM)->GetUITextureBitDepth(); }
extern "C" uintptr_t ee_uicpp_texture_load_ui(uintptr_t inManager, EeUiShimText inName, uint32_t inDepth, bool inAlpha)
{ return (uintptr_t)reinterpret_cast<GETextureManager*>(inManager)->LoadTexture(UIShimString(inName), GETextureDescription{1, 0, inDepth, 0, false, false, inAlpha, true}); }
extern "C" void ee_uicpp_texture_reference(uintptr_t inT) { TEX(inT)->Reference(); }
extern "C" void ee_uicpp_texture_stage_alpha(uintptr_t inT, uint32_t inMode)
{ TEX(inT)->SetStageAlphaMode(inMode, -1); }
extern "C" void ee_uicpp_texture_stage_color(uintptr_t inT, uint32_t inMode)
{ TEX(inT)->SetStageColorMode(inMode, -1); }
extern "C" void ee_uicpp_texture_stage_min(uintptr_t inT, uint32_t inMode)
{ TEX(inT)->SetStageMinFilterMode(inMode, -1); }
extern "C" void ee_uicpp_texture_stage_mag(uintptr_t inT, uint32_t inMode)
{ TEX(inT)->SetStageMagFilterMode(inMode, -1); }
extern "C" void ee_uicpp_texture_stage_mip(uintptr_t inT, uint32_t inMode)
{ TEX(inT)->SetStageMipFilterMode(inMode, -1); }
extern "C" void ee_uicpp_texture_stage_bias(uintptr_t inT, float inBias) { TEX(inT)->SetStageMipMapBias(inBias, -1); }
extern "C" void ee_uicpp_texture_stage_address(uintptr_t inT, uint32_t inMode)
{ TEX(inT)->SetStageTextureAddressingMode(inMode, -1); }
extern "C" uint32_t ee_uicpp_texture_width(uintptr_t inT) { return (uint32_t)TEX(inT)->GetWidth(); }
extern "C" uint32_t ee_uicpp_texture_height(uintptr_t inT) { return (uint32_t)TEX(inT)->GetHeight(); }
extern "C" uintptr_t ee_uicpp_bitmap_new() { return (uintptr_t) new GEBitmap; }
extern "C" void ee_uicpp_bitmap_set_texture(uintptr_t inB, uintptr_t inT, uint32_t inScreen)
{ BMP(inB)->SetTexture(TEX(inT), inScreen); }
extern "C" uintptr_t ee_uicpp_bitmap_texture(uintptr_t inB, uint32_t inScreen)
{ return (uintptr_t)BMP(inB)->GetTexture(inScreen); }
extern "C" void ee_uicpp_bitmap_set_opacity(uintptr_t inB, float inOpacity) { BMP(inB)->SetOpacity(inOpacity); }
extern "C" void ee_uicpp_bitmap_uvs(uintptr_t inB, float* inUvs)
{ BMP(inB)->GetTextureCoordinatesPerVertex(UVS(inUvs)[0], UVS(inUvs)[1], UVS(inUvs)[2], UVS(inUvs)[3]); }
extern "C" void ee_uicpp_bitmap_set_uvs(uintptr_t inB, float inU0, float inV0, float inU1, float inV1, float inU2, float inV2, float inU3, float inV3)
{ BMP(inB)->SetTextureCoordinates(GETextureCoordinate(inU0, inV0), GETextureCoordinate(inU1, inV1), GETextureCoordinate(inU2, inV2), GETextureCoordinate(inU3, inV3)); }
extern "C" bool ee_uicpp_lock_create(uintptr_t* inLock)
{ return TSScheduler::CreateCriticalSection(*reinterpret_cast<TSCriticalSection**>(inLock)) == keseOK; }
extern "C" void ee_uicpp_es_verify(uint32_t inLine, EeUiShimText inExpression)
{ ESVerify(__FILE__, inLine, UIShimStd(inExpression).c_str()); }
extern "C" void ee_uicpp_instance_reference(uintptr_t inInstance)
{ reinterpret_cast<SESMInstanceNode*>(inInstance)->Reference(); }
extern "C" void ee_uicpp_instance_dereference(uintptr_t inInstance)
{ reinterpret_cast<SESMInstanceNode*>(inInstance)->Dereference(); }
extern "C" uintptr_t ee_uicpp_table_get(uint32_t inIndex)
{ return (uintptr_t)UIControlsTable::GetUIControlsTable(inIndex); }
extern "C" uintptr_t ee_uicpp_mgr_form_by_id(uintptr_t inM, uint32_t inId)
{ return (uintptr_t)MGR(inM)->GetFormByID(inId); }
extern "C" void ee_uicpp_table_add_event(uintptr_t inTable, uint32_t inId, uint32_t inKind)
{ reinterpret_cast<UIControlsTable*>(inTable)->AddEvent(inId, inKind); }
extern "C" void ee_uicpp_form_add_event(uintptr_t inForm, uint32_t inId, uint32_t inKind)
{ reinterpret_cast<UIForm*>(inForm)->AddEvent(inId, inKind); }
extern "C" long ee_uicpp_db_find(uintptr_t inPath, uintptr_t inDirectory, EeUiShimText inName, uintptr_t* inFile)
{ return EEDbFindFile(reinterpret_cast<FSPath*>(inPath), *reinterpret_cast<const UString*>(inDirectory) + UIShimString(inName), *reinterpret_cast<FSFileSpec**>(inFile)); }
extern "C" long ee_uicpp_db_fonts(uintptr_t inFile, uintptr_t inM)
{ return UIFontTable::Create(*FILE_SPEC(inFile), MGR(inM)); }
extern "C" long ee_uicpp_db_forms(uintptr_t inFile, uintptr_t inM)
{ return UIForm::Create(*FILE_SPEC(inFile), MGR(inM)); }
extern "C" long ee_uicpp_db_backgrounds(uintptr_t inFile) { return UIBackgroundTable::Create(*FILE_SPEC(inFile)); }
extern "C" void ee_uicpp_db_create_backgrounds(uintptr_t inM) { ee_ui_bgtable_create_backgrounds(MGR(inM), ee_ui_bg_table_host); }
extern "C" void ee_uicpp_db_controls(uintptr_t inFile, uintptr_t inM)
{ UIControlsTable::Create(*FILE_SPEC(inFile), MGR(inM)); }
extern "C" void ee_uicpp_db_control_events(uintptr_t inFile) { UIEvent::CreateControlEvents(*FILE_SPEC(inFile)); }
extern "C" void ee_uicpp_db_form_events(uintptr_t inFile, uintptr_t inM)
{ UIEvent::CreateFormEvents(*FILE_SPEC(inFile), MGR(inM)); }
extern "C" void ee_uicpp_db_hot_keys(uintptr_t inFile) { UIHotKey::CreateHotKeys(*FILE_SPEC(inFile)); }
extern "C" void ee_uicpp_file_delete(uintptr_t inFile) { delete FILE_SPEC(inFile); }
extern "C" void ee_uicpp_db_remove_controls() { UIControlsTable::DeleteAll(); }
extern "C" void ee_uicpp_db_remove_backgrounds() { UIBackgroundTable::DeleteAll(); }
extern "C" void ee_uicpp_db_remove_fonts() { UIFontTable::DeleteAll(); }
extern "C" bool ee_uicpp_mouse_buttons_ready() { return UIControl::Key(EeUiKey::LeftMouse) != NULL; }
// the shared mouse button inButton into the key slot inWhich after the left button's (lle_manager.rs)
extern "C" void ee_uicpp_mouse_button_fetch(uint32_t inWhich, uint32_t inButton)
{ gISMouse->GetEntity(inButton, UIControl::Key((EeUiKey)((uint32_t)EeUiKey::LeftMouse + inWhich))); }
extern "C" void ee_uicpp_mouse_swap_buttons() { std::swap(UIControl::Key(EeUiKey::LeftMouse), UIControl::Key(EeUiKey::RightMouse)); }

// a call with a rectangle (GERectangle is the buffer's order: bottom, left, right, top) or two floats
#undef FORM   // (the earlier sections' FORM(x))
#define FORM reinterpret_cast<UIForm*>(a)
#define INSTANCE reinterpret_cast<SESMInstanceNode*>(a)
#define SOUNDS static_cast<UIManager*>(inObj)->GetSoundManager()
EE_UI_VOID(mgr_form_rect) { UIObjHost::PutRect(f, FORM->GetRectangle()); }
EE_UI_CASE(mgr_form_id) { return FORM->GetFormID(); }
EE_UI_CASE(mgr_form_use_gdi) { return FORM->GetUseGDIForText(); }
EE_UI_CASE(mgr_form_keyboard) { return (intptr_t)FORM->GetKeyboardCaptured(); }
EE_UI_CASE(mgr_form_mouse) { return (intptr_t)FORM->GetMouseCaptured(); }
// the form calls (lle_hosts.rs mgr_form_call, mgr_form_rect_call, mgr_form_ime: b the call)
EE_UI_VOID(mgr_form_open) { FORM->Open(); }
EE_UI_VOID(mgr_form_close) { FORM->Close(); }
EE_UI_VOID(mgr_form_activate) { FORM->Activate(); }
EE_UI_VOID(mgr_form_deactivate) { FORM->Deactivate(); }
EE_UI_VOID(mgr_form_clear_clips) { FORM->ClearAllClipRectangles(); }
EE_UI_VOID(mgr_form_all_dirty) { FORM->SetAllControlsDirty(); }
EE_UI_CASE(mgr_form_copy) { return FORM->CopyData(); }
EE_UI_CASE(mgr_form_draw_polygons) { return FORM->DrawPolygons(); }
EE_UI_CASE(mgr_form_draw_text) { return FORM->DrawTextA(); }
EE_UI_CASE(mgr_form_update_keyboard) { return FORM->UpdateKeyboard(); }
EE_UI_CASE(mgr_form_update_mouse) { return FORM->UpdateMouse(); }
EE_UI_VOID(mgr_form_delete) { delete FORM; }
EE_UI_VOID(mgr_form_add_dirty) { FORM->AddDirtyRectangle(UIManagerRect(f)); }
EE_UI_VOID(mgr_form_redraw) { FORM->RedrawIntersectControls(UIManagerRect(f)); }
EE_UI_VOID(mgr_form_add_clip) { FORM->AddClipRectangle(UIManagerRect(f)); }
EE_UI_VOID(mgr_form_relayout) { FORM->RelayoutForm(UIManagerRect(f)); }
EE_UI_VOID(mgr_form_mouse_location) { FORM->UpdateMouseLocation(GE2DPoint(f[0], f[1])); }
#define COMPOSITION (*reinterpret_cast<const EeUiImeComposition*>(c))
EE_UI_VOID(mgr_form_ime_result) { FORM->AddIMEResultString(*reinterpret_cast<const UWideString*>(c)); }
EE_UI_CASE(mgr_form_ime_change) { return FORM->ChangeIMECompositionString(*static_cast<const UWideString*>(COMPOSITION.string), *static_cast<const UString*>(COMPOSITION.attributes), COMPOSITION.cursor); }
EE_UI_CASE(mgr_form_ime_close) { return FORM->CloseIMECompositionString(); }
EE_UI_CASE(mgr_form_ime_open) { return FORM->OpenIMECompositionString(); }
#undef COMPOSITION
EE_UI_CASE(mgr_ctl_ime) { return reinterpret_cast<UIControl*>(a)->GetIMEEnabled(); }
EE_UI_VOID(mgr_part_rect) { UIObjHost::PutRect(f, reinterpret_cast<UIControl*>(a)->GetRectangle()); }
EE_UI_VOID(mgr_event_process) { reinterpret_cast<UIEvent*>(a)->ProcessEvent(reinterpret_cast<UIForm*>(b), reinterpret_cast<UIEntity*>(c)); }
EE_UI_CASE(mgr_keyboard_should_update) { return gISKeyboard->GetShouldUpdate(); }
EE_UI_CASE(mgr_mouse_should_update) { return gISMouse->GetShouldUpdate(); }
EE_UI_VOID(mgr_hot_keys_delete) { UIHotKey::DeleteHotKeys(); }
EE_UI_CASE(mgr_virt_update_keyboard) { return static_cast<UIManager*>(inObj)->UpdateKeyboard(); }
EE_UI_CASE(mgr_virt_update_mouse) { return static_cast<UIManager*>(inObj)->UpdateMouse(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(mgr_set_blending) { static_cast<UIManager*>(inObj)->GetRasterizer()->SetBlending((unsigned long)a, (unsigned long)b, static_cast<UIManager*>(inObj)->GetScreen()); }
EE_UI_VOID(mgr_texture_release) { reinterpret_cast<GETexture*>(a)->Dereference(); }
EE_UI_VOID(mgr_bitmap_release) { reinterpret_cast<GEBitmap*>(a)->Dereference(); }
EE_UI_VOID(mgr_lock_delete) { TSScheduler::DestroyCriticalSection(reinterpret_cast<TSCriticalSection*>(a)); }
EE_UI_VOID(mgr_form_lock_enter) { reinterpret_cast<TSCriticalSection*>(a)->Enter(); }
EE_UI_VOID(mgr_form_lock_leave) { reinterpret_cast<TSCriticalSection*>(a)->Leave(); }
EE_UI_VOID(mgr_sound_stop) { SOUNDS->Stop(INSTANCE, false); }
EE_UI_VOID(mgr_sound_unload) { SOUNDS->Unload((long)a); }
EE_UI_CASE(mgr_sound_load) { return SOUNDS->Load(reinterpret_cast<const char*>(a), 0, 1); }
EE_UI_CASE(mgr_sound_play) { return (intptr_t)SOUNDS->PlayAndReturnReferencedNode((long)a, f[0], 0.0f, 1.0f, true, NULL); }
EE_UI_VOID(mgr_sound_parameters) { SOUNDS->SetSoundParameters(INSTANCE, f[0], 0.0f, 1.0f); }
#undef FORM
#undef INSTANCE
#undef SOUNDS

// UIEvent is whole in Rust (rust/crates/ee_ui/src/eventobj.rs, R6 ui round 21; the original unit UIEvent.cpp,
// 0x1006f278..0x1006f4ef): Initialize/Uninitialize over the record, the two loaders over the events file. The calls
// out: the event's virtuals on the record, the file's reads (UIHotKey.cpp's ee_uicpp_file_*) and the owners'
EE_UI_CASE(event_ev_initialize) { return static_cast<UIEvent*>(static_cast<EeUiEventFields*>(inObj))->InitializeEvent(); }
EE_UI_CASE(event_ev_uninitialize) { return static_cast<UIEvent*>(static_cast<EeUiEventFields*>(inObj))->UninitializeEvent(); }
EE_UI_VOID(event_bgt_file_open) { ee_uicpp_file_open(inObj); }
EE_UI_CASE(event_bgt_file_read) { return ee_uicpp_file_read(inObj, reinterpret_cast<unsigned char*>(b), (unsigned long)c); }
EE_UI_VOID(event_bgt_file_close) { ee_uicpp_file_close(inObj); }
// TEST ONLY: the harness's presentation bridge (tools/harness/presentation_bridge.c) finds these by name in the map
// (the manager's and forms' state is Rust's: ee_ui managerobj.rs, formobj.rs): the UI lock it try-enters, the form
// stack, and a form's id and controls. Kept by the linker for it.
extern "C" void* EETestUILock(UIManager* inManager) { return inManager->GetUILock(); }
extern "C" unsigned long EETestUIFormCount(UIManager* inManager) { return inManager->GetFormCount(); }
extern "C" void* EETestUIForm(UIManager* inManager, unsigned long inIndex) { return inManager->GetStackedForm(inIndex); }
extern "C" unsigned long EETestUIFormID(UIForm* inForm) { return inForm->GetFormID(); }
extern "C" unsigned long EETestUIFormControlCount(UIForm* inForm) { return inForm->GetControlCount(); }
extern "C" void* EETestUIFormControl(UIForm* inForm, unsigned long inIndex) { return inForm->GetControl(inIndex); }
#if defined(_MSC_VER)
#pragma comment(linker, "/include:EETestUILock")
#pragma comment(linker, "/include:EETestUIFormCount")
#pragma comment(linker, "/include:EETestUIForm")
#pragma comment(linker, "/include:EETestUIFormID")
#pragma comment(linker, "/include:EETestUIFormControlCount")
#pragma comment(linker, "/include:EETestUIFormControl")
#endif

// C:\EEX\Low-Level Engine\User Interface\UIScrollBar.cpp
// The original unit: 0x1007c901..0x1007d013 (the U2D key init chains, the ctor, the out-of-line members); the
// header-inline members were emitted by UICheckBox.cpp (0x1005f3b0..0x1005f538).
//
// UIScrollBar is whole in Rust (rust/crates/ee_ui/src/scrollbarobj.rs, R6 ui round 10): the members below forward to
// the Rust object, which calls out through UIScrollBarRust::Op (UIControl's members, the parts' virtuals, their new
// and delete, the manager's arrow bitmaps).
#define s static_cast<UIScrollBar*>(inObj)
#define theSlider reinterpret_cast<UISlider*>(a)
EE_UI_CASE(bar_create_slider) { return (intptr_t) new UISlider(s->GetId(), UIObjHost::Rect(f), (unsigned long)a, s->GetForm(), s->GetIndex(), (unsigned long)b); }
// a 19 x 19 arrow button at f (its slider and direction set by Rust: scrollbarobj.rs scroll_init)
EE_UI_CASE(bar_create_button) { return (intptr_t) new UIButton(s->GetId(), reinterpret_cast<GEBitmap*>(a), NULL, reinterpret_cast<GEBitmap*>(b), GERectangle(GE2DPoint(f[0], f[1]), GE2DPoint(f[0] + 19.0f, f[1] + 19.0f)), s->GetForm(), s->GetIndex(), 0); }
EE_UI_VOID(bar_part_button_down) { reinterpret_cast<UIButton*>(a)->ButtonDown(); }
EE_UI_CASE(bar_slider_style) { return theSlider->GetSliderStyle(); }
EE_UI_CASE(bar_slider_value) { return theSlider->GetCurrentRangeValue(); }
EE_UI_VOID(bar_slider_set_value) { theSlider->SetCurrentRangeValue((long)b); }
EE_UI_VOID(bar_slider_decrement_page) { theSlider->DecrementByPageSize(); }
EE_UI_VOID(bar_slider_increment_page) { theSlider->IncrementByPageSize(); }
EE_UI_VOID(bar_slider_set_page_size) { theSlider->SetPageSize((unsigned long)b); }
EE_UI_VOID(bar_slider_set_range) { theSlider->SetRange((unsigned long)b, (unsigned long)c); }
#undef theSlider
#undef s

// C:\EEX\Low-Level Engine\User Interface\UISlider.cpp
// The original unit: 0x1007d139..0x1007db5b (the U2D key init chains, the ctor, the out-of-line members); the
// header-inline members were emitted by UICheckBox.cpp (0x1005ef4b..0x1005f272).
//
// UISlider is whole in Rust (rust/crates/ee_ui/src/sliderobj.rs, R6 ui round 10): the members below forward to the
// Rust object, which calls out through ee_ui_slider_host (UIControl's members, the virtuals ChangeRange, ButtonDown,
// ButtonUp and SetDirty, the thumb bitmap, the manager's drag bar).
#define s static_cast<UISlider*>(inObj)
#define theManager (s->GetForm()->GetManager())
EE_UI_VOID(slider_change_range) { s->ChangeRange((unsigned long)a); }
EE_UI_VOID(slider_button_down) { s->ButtonDown(); }
EE_UI_VOID(slider_button_up) { s->ButtonUp(); }
EE_UI_CASE(slider_thumb_create) { return (intptr_t)theManager->CreateBitmapForScaledControl(theManager->GetDragBarBitmap(), UIObjHost::Rect(f)); }
EE_UI_VOID(slider_drag_bar_scale) { *reinterpret_cast<GE2DPoint*>(f) = GE2DPoint(theManager->GetDragBarBitmap()->GetXScale(), theManager->GetDragBarBitmap()->GetYScale()); }
#undef theManager
#undef s

// C:\EEX\Low-Level Engine\User Interface\UIStatusBar.cpp
// The original unit: 0x1007db5b..0x1007e312 (the U2D key init chains, the ctor, the dtor, the out-of-line members);
// the header-inline members were emitted by UIControlsTable.cpp.
//
// UIStatusBar is whole in Rust (rust/crates/ee_ui/src/statusobj.rs, R6 ui round 10): the members below forward to
// the Rust object, which calls out through ee_ui_status_bar_host (UIControl's members, the two rhombi, the texts).
#define s static_cast<UIStatusBar*>(inObj)
#define theRhombus (a ? s->mCopiedStatusRhombus : s->mStatusRhombus)
EE_UI_VOID(status_rhombus_rect) { UIObjHost::PutRect(f, theRhombus.GetRectangle()); }
EE_UI_VOID(status_rhombus_set_rect) { theRhombus.SetRectangle(UIObjHost::Rect(f)); }
EE_UI_VOID(status_rhombus_copy) { s->mCopiedStatusRhombus = s->mStatusRhombus; }
EE_UI_VOID(status_rhombus_set_opacity) { theRhombus.SetOpacity(f[0]); }
EE_UI_VOID(status_rhombus_set_color) { theRhombus.SetColor(GEColor(f[0], f[1], f[2])); }
EE_UI_CASE(status_rhombus_draw) { return s->GetRasterizer()->Draw(&theRhombus, s->GetScreenIndex()); }
#undef theRhombus
#undef s

// C:\EEX\Low-Level Engine\User Interface\UITextLabel.cpp
// The original unit: 0x1007e579..0x1007fb46 (the ctor, the out-of-line members in alphabetical order, the vector
// COMDATs); the header-inline members were emitted by UICheckBox.cpp (0x1005f538..0x1005f9da).
//
// UITextLabel is whole in Rust (rust/crates/ee_ui/src/labelobj.rs, R6 ui round 13): the members below forward to the
// Rust object, which calls out through UITextLabelRust::Op (UIControl's members, the label's own virtuals, the scroll
// bar, the texts and the strings it holds by address). The wrapping (wrap.rs), the geometry (label_fit.rs) and the
// draw places (draw.rs) are its algorithms.
struct UITextLabelRust {
    static const GERectangle& RectAt(intptr_t inAddress) { return *reinterpret_cast<const GERectangle*>(inAddress); }
};
#define l static_cast<UITextLabel*>(inObj)
#define theBar reinterpret_cast<UIScrollBar*>(a)
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
EE_UI_VOID(label_virt_check_scroll_bar) { l->CheckScrollBar(); }
EE_UI_VOID(label_virt_recompute) { l->RecomputeDisplayRange(); }
EE_UI_VOID(label_virt_set_location) { l->SetLocation(GE2DPoint(f[0], f[1])); }
EE_UI_VOID(label_virt_set_rectangle) { l->SetRectangle(UIObjHost::Rect(f)); }
#undef theBar
#undef l

// ---- the unit's own code
// Registers the user interface's shutdown hook (every background table deleted); always succeeds (ee_ui
// bgtableobj.rs). The original's unreferenced file-static UIBackgroundTable and its gUIInitialized flag (set here,
// read by nothing) are gone.
long UIInitialize() { return ee_ui_initialize(); }
