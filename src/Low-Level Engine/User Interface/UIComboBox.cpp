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
extern "C" bool ee_uicpp_key_enter_value() { return UIControl::sEnterKey->GetValue() != 0; }
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
extern "C" uintptr_t ee_uicpp_combo_new_label(uintptr_t inCombo, uintptr_t inFont, float inLeft, float inTop, float inRight, float inBottom, uint32_t inBorder)
{ return (uintptr_t) new UITextLabel(UWideString(""), reinterpret_cast<GEFont*>(inFont), COMBO(inCombo)->GetId(), GERectangle(inLeft, inTop, inRight, inBottom), COMBO(inCombo)->GetForm(), 0, COMBO(inCombo)->GetIndex(), inBorder); }
extern "C" uintptr_t ee_uicpp_combo_new_edit(uintptr_t inCombo, uintptr_t inFont, uint32_t inMaxChars, float inLeft, float inTop, float inRight, float inBottom, uint32_t inBorder)
{ return (uintptr_t) new UIEditBox(COMBO(inCombo)->GetId(), reinterpret_cast<GEFont*>(inFont), inMaxChars, false, GERectangle(inLeft, inTop, inRight, inBottom), COMBO(inCombo)->GetForm(), COMBO(inCombo)->GetIndex(), inBorder, false, false); }
extern "C" uintptr_t ee_uicpp_combo_new_list(uintptr_t inCombo, uintptr_t inData, uintptr_t inFont, float inLeft, float inTop, float inRight, float inBottom, uint32_t inBorder)
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

// A nested struct: a member of UIComboBox, so it has the class's friendships (UIControl's, UITextLabel's, UIListBox's)
struct UIComboBox::Rust {
    static UIControl* Part(UIComboBox* c, intptr_t inPart) { return c->PartOf((int)inPart); }
    static UILBString* List(UIComboBox* c) { return c->GetList(); }
    static UIEditBox* Edit(UIComboBox* c) { return reinterpret_cast<UIEditBox*>(c->PartOf(3)); }
    static intptr_t Query(UIComboBox* c, intptr_t q, intptr_t arg, float* f)
    {
        switch ((EeUiComboQuery)q) {
        case EeUiComboQuery::RectOf: UIObjHost::PutRect(f, Part(c, arg)->GetRectangle()); return 0;
        case EeUiComboQuery::TextBoxBorder: return Part(c, 0)->GetBorderThickness();
        case EeUiComboQuery::LineCount: return List(c)->GetLineCount();
        case EeUiComboQuery::Line: return (intptr_t)(const char*)List(c)->GetData((unsigned long)arg);
        case EeUiComboQuery::LineHeight: return List(c)->GetLineHeight();
        case EeUiComboQuery::ListBorder: return List(c)->GetBorderThickness();
        case EeUiComboQuery::ListScrollBarCaptured: return List(c)->GetScrollBarCaptured();
        case EeUiComboQuery::WindowHeight: return c->GetRasterizer()->GetWindow(c->GetScreenIndex())->GetHeight();
        case EeUiComboQuery::EditText: return (intptr_t)Edit(c)->GetText(0);
        case EeUiComboQuery::TabbingOn: return c->GetForm()->GetTabbingOn();
        case EeUiComboQuery::TabStop: return c->GetForm()->GetTabStop();
        case EeUiComboQuery::HasMouseCapture: return c->GetHasMouseCapture();
        }
        return 0;
    }
    static intptr_t Act(UIComboBox* c, intptr_t inAct, intptr_t b, intptr_t cc, intptr_t d, float* f)
    {
        switch ((EeUiComboAct)inAct) {
        case EeUiComboAct::ListChangeSelection: List(c)->ChangeSelectionIndex((unsigned long)b, (cc & 1) != 0, (cc & 2) != 0); break;
        case EeUiComboAct::ListInsertLine: List(c)->InsertLine(UWideString(reinterpret_cast<const char*>(b)), (unsigned long)cc); break;
        case EeUiComboAct::ListDeleteLine: List(c)->DeleteLine((unsigned long)b); break;
        case EeUiComboAct::ListDeleteAll: List(c)->DeleteAllEntries(); break;
        case EeUiComboAct::ListChangeWidth: List(c)->ChangeWidth((unsigned long)b); break;
        case EeUiComboAct::PartMouseDown: return Part(c, b)->MouseLButtonDown();
        case EeUiComboAct::PartMouseUp: return Part(c, b)->MouseLButtonUp();
        case EeUiComboAct::PartMouseMove: return Part(c, b)->MouseMove(GE2DPoint(f[0], f[1]));
        case EeUiComboAct::ListWheel: return List(c)->MouseWheelTurned((long)b);
        case EeUiComboAct::PartUpdateKeyboard: return Part(c, b)->UpdateKeyboard();
        case EeUiComboAct::PartTranslatedKey: return Part(c, b)->UpdateTranslatedKey(reinterpret_cast<const char*>(cc), (short)d);
        case EeUiComboAct::PartSetRectangle: Part(c, b)->SetRectangle(UIObjHost::Rect(f)); break;
        case EeUiComboAct::PartSetLocation: Part(c, b)->SetLocation(GE2DPoint(f[0], f[1])); break;
        case EeUiComboAct::PartCopyData: return Part(c, b)->CopyData();
        case EeUiComboAct::BaseCopyData: return c->UIControl::CopyData();
        case EeUiComboAct::SetPopup: c->GetForm()->SetPopup(b ? c : NULL); break;
        case EeUiComboAct::SetDirty: c->SetDirty(true); break;
        case EeUiComboAct::SetRectangle: c->SetRectangle(UIObjHost::Rect(f)); break;
        case EeUiComboAct::BaseSetRectangle: c->UIControl::SetRectangle(UIObjHost::Rect(f)); break;
        case EeUiComboAct::BaseSetLocation: c->UIControl::SetLocation(GE2DPoint(f[0], f[1])); break;
        case EeUiComboAct::SetAlwaysDirty: c->SetAlwaysDirty(b != 0); break;
        case EeUiComboAct::Fire: c->FireTriggerEvent((unsigned long)b); break;
        case EeUiComboAct::CaptureMouse: c->CaptureMouse(); break;
        case EeUiComboAct::CaptureKeyboard: c->CaptureKeyboard(); break;
        case EeUiComboAct::ReleaseMouse: c->ReleaseCapturedMouse(); break;
        case EeUiComboAct::ReleaseKeyboard: c->ReleaseCapturedKeyboard(); break;
        case EeUiComboAct::OpenList: c->OpenListBox(); break;
        case EeUiComboAct::CloseList: c->CloseListBox(); break;
        case EeUiComboAct::AddString: return c->AddString();
        case EeUiComboAct::ChangeSelection: c->ChangeSelection(); break;
        }
        return 0;
    }
    static intptr_t Op(void* inCombo, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t cc, intptr_t d, float* f)
    {
        UIComboBox* c = static_cast<UIComboBox*>(inCombo);
        intptr_t r = 0;
        if (UIObjHost::Base(c, inOp, a, f, &r) || UIObjHost::Part(reinterpret_cast<UIControl*>(a), inOp, f, &r))
            return r;
        switch (inOp) {
        case EeUiOp::ComboQuery: return Query(c, a >> 24, a & 0xffffff, f);
        case EeUiOp::ComboAct: return Act(c, a, b, cc, d, f);
        case EeUiOp::RhombusCreate: return (intptr_t) new GERhombus();
        case EeUiOp::RhombusDelete: delete reinterpret_cast<GERhombus*>(a); break;
        case EeUiOp::DrawCountDecrement: c->mDrawCount--; break;
        default: break;
        }
        return 0;
    }
};

// Creates a combo box: a text label (type 1) or edit box, a drop button and a hidden list box showing the first entry
UIComboBox::UIComboBox(unsigned long inId, unsigned long inTextBoxType, std::vector<UWideString> inData,
                       GEFont* inFont, const GERectangle& inRectangle, unsigned long inMaxListBoxHeight,
                       UIForm* inForm, unsigned long inIndex, unsigned long inMaxChars,
                       unsigned long inBorderThickness)
    : UIControl(kControlTypeComboBox, inId, inRectangle, inForm, inIndex, 0)
{
    ee_ui_combo_construct(this, inTextBoxType, &inData, inFont, reinterpret_cast<const EeUiRect*>(&inRectangle), inMaxListBoxHeight, inForm, inMaxChars, inBorderThickness);
}

extern "C" intptr_t ee_uicpp_combo_host(EE_UI_HOST_ARGS) { return UIComboBox::Rust::Op(inObj, inOp, a, b, c, d, f); }

// Destroys the combo box and its text box, list box and drop button
UIComboBox::~UIComboBox() { ee_ui_combo_free(this->mRust); }

// Appends a line to the list, reopening an open list so it resizes
void UIComboBox::AppendLine(UWideString& inLine)
{
    this->SetDirty(true);
    this->GetList()->AppendLine(inLine);
    this->Call(EeUiComboCall::Reopen);
}

// Appends several lines to the list, reopening an open list so it resizes
void UIComboBox::AppendStringArray(std::vector<UWideString>& inLines)
{
    this->SetDirty(true);
    this->GetList()->AppendStringArray(inLines);
    this->Call(EeUiComboCall::Reopen);
}
