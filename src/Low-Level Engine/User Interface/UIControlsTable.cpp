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

#include "rust/ui/obj.h"
#include "rust/ui/rect.h"
#include "rust/ui/tableobj.h"
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
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/GEFont.h"
#include "lle/GEFontBitmap.h"
#include "lle/GEText.h"
#include "lle/GELine.h"
#include "lle/FSFileSpec.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/UResourceFile.h"
#include "lle/UGraph.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIControlsTable.h"
#include "lle/UIBitmap.h"
#include "lle/UIButton.h"
#include "lle/UIBlinkingButton.h"
#include "lle/UISlider.h"
#include "lle/UIScrollButton.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UICheckBox.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIDualButton.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIFileListBox.h"
#include "lle/UIGraph.h"
#include "lle/UIRadioButton.h"
#include "lle/UIScalableButton.h"
#include "lle/UITLScrolling.h"
#include "lle/UIStatusBar.h"
#include "lle/UIFontTable.h"
#include "lle/UIManager.h"
#include "lle/UIShims.h"
#include "rust/ui/lle_table.h"

// the one-statement shims of the moved host cases, CreateBitmap and SetupStringArray (ee_ui lle_table.rs)
#define FORM(x) reinterpret_cast<UIForm*>(x)
#define MGR(x) reinterpret_cast<UIManager*>(x)
#define CTL(x) reinterpret_cast<UIControl*>(x)
#define TBL(x) reinterpret_cast<const UIControlsTable*>(x)
#define STRINGS(x) reinterpret_cast<std::vector<UWideString>*>(x)
#define RECT(x) reinterpret_cast<const GERectangle*>(x)
extern "C" void ee_uicpp_form_add_database_control(uintptr_t inForm, uint32_t inIndex)
{ FORM(inForm)->AddDatabaseControl(UICDatabase{inIndex, NULL}); }
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

// The Rust table's calls out (EeUiOp, obj.rs; ee_ui tableobj.rs); the cases of more than one call are Rust's
// (lle_table.rs, the host ee_ui_table_host): a is the form, font, file, string, bitmap,
// control or event the op names, d the manager where the op needs one; rectangles and colours travel in f
struct UIControlsTableHost {
    static GERectangle R(const float* f) { return GERectangle(f[1], f[3], f[2], f[0]); }
    static GERectangle R(const EeUiRect& r) { return GERectangle(r.left, r.top, r.right, r.bottom); }
    static GEColor C(const float* f) { return GEColor(f[0], f[1], f[2]); }

    // A control's constructor (the factories' new: CreateBitmap .. CreateTextLabel)
    static UIControl* Make(const EeUiTableMake& m)
    {
#define r R(m.rect)
#define theForm reinterpret_cast<UIForm*>(m.form)
#define theFont reinterpret_cast<GEFont*>(m.font)
#define theText reinterpret_cast<const UWideString*>(m.text)
#define theStrings reinterpret_cast<std::vector<UWideString>*>(m.strings)
#define n reinterpret_cast<GEBitmap*>(m.normal)
#define fo reinterpret_cast<GEBitmap*>(m.focused)
#define p reinterpret_cast<GEBitmap*>(m.pressed)
        switch (m.kind) {
        case EeUiTableKind::Bitmap: return new UIBitmap(m.id, n, r, theForm, m.index);
        case EeUiTableKind::Button: return new UIButton(m.id, n, fo, p, r, theForm, m.index, m.border);
        case EeUiTableKind::BlinkingButton: return new UIBlinkingButton(m.id, n, fo, p, r, theForm, m.index, m.border, m.blink_rate);
        case EeUiTableKind::ScalableButton: return new UIScalableButton(m.id, n, fo, p, R(m.normal_rect), R(m.focused_rect), R(m.pressed_rect), r, r, theForm, m.index);
        case EeUiTableKind::DualButton: return new UIDualButton(m.id, n, fo, p, r, theForm, m.index, m.border);
        case EeUiTableKind::CheckBox: return new UICheckBox(m.id, *theText, theFont, r, theForm, m.index, m.border, m.flag_a);
        case EeUiTableKind::ComboBox: return new UIComboBox(m.id, m.style, *theStrings, theFont, r, m.list_height, theForm, m.index, m.max_chars, m.border);
        case EeUiTableKind::FileComboBox: return new UIFileComboBox(m.id, 1, *theStrings, theFont, r.GetTopLeft(), m.length, m.list_height, theForm, m.index, m.max_chars, m.border);
        case EeUiTableKind::EditBox: return new UIEditBox(m.id, theFont, m.max_chars, m.flag_a, r, theForm, m.index, m.border, m.flag_b, m.flag_c);
        case EeUiTableKind::FileListBox: return new UIFileListBox(m.id, theFont, false, r, theForm, m.index, m.border);
        case EeUiTableKind::LineGraph: return new UIGraph(m.id, r, theForm, m.index, m.border);
        case EeUiTableKind::ListBox: return new UILBString(m.id, *theStrings, theFont, m.flag_a, r, theForm, m.index, m.border);
        case EeUiTableKind::RadioButton: return new UIRadioButton(m.id, *theText, theFont, r, theForm, m.index, m.border, m.flag_a, m.flag_b, m.flag_c);
        case EeUiTableKind::ScrollBar: return new UIScrollBar(m.id, r.GetTopLeft(), m.length, m.style, theForm, m.index, m.border);
        case EeUiTableKind::ScrollingLabel: return new UITLScrolling(*theText, theFont, m.id, r, theForm, m.index, m.border);
        case EeUiTableKind::Slider: return new UISlider(m.id, r, m.style, theForm, m.index, m.border);
        case EeUiTableKind::StatusBar: return new UIStatusBar(m.id, r, theForm, m.index, m.border);
        case EeUiTableKind::TextLabel: return new UITextLabel(*theText, theFont, m.id, r, theForm, m.style, m.index, m.border);
        }
        return NULL;
    }
#undef r
#undef theForm
#undef theFont
#undef theText
#undef theStrings
#undef n
#undef fo
#undef p

    // SetupGEText of a made control's text (EeUiTableSet::SetupText's classes)
    static GEText* Text(UIControl* c, intptr_t inClass)
    {
        switch (inClass) {
        case 0: return static_cast<UIButton*>(c)->GetGEText();
        case 1: return static_cast<UICheckBox*>(c)->GetTextLabel()->GetGEText();
        case 2: return static_cast<UIComboBox*>(c)->GetGEText();
        case 3: return static_cast<UIEditBox*>(c)->GetGEText();
        case 4: return static_cast<UILBString*>(c)->GetGEText();
        case 5: return static_cast<UIRadioButton*>(c)->GetTextLabel()->GetGEText();
        case 6: return static_cast<UITextLabel*>(c)->GetGEText();
        default: return static_cast<UIStatusBar*>(c)->GetGEText();
        }
    }

    // A made control's setter
    static void Set(const UIControlsTable* t, UIControl* c, EeUiTableSet inSet, intptr_t a, intptr_t b, const float* f)
    {
#define theButton static_cast<UIButton*>(c)
#define theCombo static_cast<UIComboBox*>(c)
#define theList static_cast<UILBString*>(c)
#define theRadio static_cast<UIRadioButton*>(c)
#define theBar static_cast<UIStatusBar*>(c)
#define theLabel static_cast<UITextLabel*>(c)
#define theBitmap reinterpret_cast<GEBitmap*>(a)
#define theText (*reinterpret_cast<const UWideString*>(a))
        switch (inSet) {
        case EeUiTableSet::ButtonText: theButton->SetText(theText, reinterpret_cast<GEFont*>(b)); break;
        case EeUiTableSet::ButtonFocusedColor: theButton->SetFocusedTextColor(C(f)); break;
        case EeUiTableSet::ButtonDownSound: theButton->SetButtonDownSound(UString(reinterpret_cast<const char*>(a))); break;
        case EeUiTableSet::ButtonUpSound: theButton->SetButtonUpSound(UString(reinterpret_cast<const char*>(a))); break;
        case EeUiTableSet::SetupText: t->SetupGEText(Text(c, a)); break;
        case EeUiTableSet::CheckSetChecked: static_cast<UICheckBox*>(c)->SetChecked(a != 0); break;
        case EeUiTableSet::ComboFocusedRect: theCombo->SetListBoxFocusedRectColor(C(f)); break;
        case EeUiTableSet::ComboSelectedBg: theCombo->SetListBoxSelectedBgColor(C(f)); break;
        case EeUiTableSet::ComboSelection: theCombo->SetCurrentSelection((long)a); break;
        case EeUiTableSet::ComboTextBoxOpacity: theCombo->SetTextBoxBgOpacity(f[0]); break;
        case EeUiTableSet::ComboTextBoxColor: theCombo->SetTextBoxBgColor(C(f)); break;
        case EeUiTableSet::ComboListAlwaysDirty: theCombo->SetListBoxAlwaysDirty(a != 0); break;
        case EeUiTableSet::EditNumber: static_cast<UIEditBox*>(c)->SetText((unsigned long)a); break;
        case EeUiTableSet::EditText: static_cast<UIEditBox*>(c)->SetText(theText); break;
        case EeUiTableSet::EditSelectedBg: static_cast<UIEditBox*>(c)->SetSelectedBgColor(C(f)); break;
        case EeUiTableSet::ListFocusedRect: theList->SetFocusedRectColor(C(f)); break;
        case EeUiTableSet::ListSelectedBg: theList->GetSelectedBackground()->SetColor(C(f)); break;
        case EeUiTableSet::ListScrollOpacity: theList->SetScrollBarBgOpacity(f[0]); break;
        case EeUiTableSet::ListSelection: theList->ChangeSelectionIndex((unsigned long)a, false, false); break;
        case EeUiTableSet::FileListExtensions: break;   // (Rust's: lle_table.rs)
        case EeUiTableSet::GraphFont: static_cast<UIGraph*>(c)->SetFont(reinterpret_cast<GEFont*>(a)); break;
        case EeUiTableSet::GraphFontColor: static_cast<UIGraph*>(c)->SetFontColor(C(f)); break;
        case EeUiTableSet::RadioUpUnchecked: theRadio->SetUpUncheckedBitmap(theBitmap); break;
        case EeUiTableSet::RadioPressed: break;   // (Rust's: lle_table.rs)
        case EeUiTableSet::ScrollRange: static_cast<UIScrollBar*>(c)->SetRange((long)a, (long)b); break;
        case EeUiTableSet::ScrollValue: static_cast<UIScrollBar*>(c)->SetCurrentRangeValue((long)a); break;
        case EeUiTableSet::SliderRange: static_cast<UISlider*>(c)->SetRange((long)a, (long)b); break;
        case EeUiTableSet::SliderValue: static_cast<UISlider*>(c)->SetCurrentRangeValue((long)a); break;
        case EeUiTableSet::SliderPage: static_cast<UISlider*>(c)->SetPageSize((long)a); break;
        case EeUiTableSet::StatusRange: theBar->SetRange((long)a, (long)b); break;
        case EeUiTableSet::StatusValue: theBar->SetCurrentRangeValue((long)a); break;
        case EeUiTableSet::StatusColor: theBar->SetStatusBarColor(C(f)); break;
        case EeUiTableSet::StatusOpacity: theBar->SetStatusBarOpacity(f[0]); break;
        case EeUiTableSet::StatusText: theBar->SetText(theText, reinterpret_cast<GEFont*>(b)); break;
        case EeUiTableSet::StatusFocusedColor: theBar->SetFocusedTextColor(C(f)); break;
        case EeUiTableSet::LabelNumber: theLabel->SetText((long)a); break;
        case EeUiTableSet::LabelAlign: theLabel->SetAlignment((long)a); break;
        case EeUiTableSet::LabelScrollOpacity: theLabel->SetScrollBarBgOpacity(f[0]); break;
        case EeUiTableSet::LabelFit: theLabel->SetFitRectToText(a != 0); break;
        case EeUiTableSet::BgColor: c->SetBgColor(C(f)); break;
        case EeUiTableSet::BgOpacity: c->SetBgOpacity(f[0]); break;
        case EeUiTableSet::AlwaysDirty: c->SetAlwaysDirty(a != 0); break;
        case EeUiTableSet::SetRectangle: c->SetRectangle(R(f)); break;
        case EeUiTableSet::ChangeFont: c->ChangeFont(reinterpret_cast<GEFont*>(a)); break;
        case EeUiTableSet::SetTrigger: c->SetTriggerEvent((unsigned long)a, reinterpret_cast<UIEvent*>(b)); break;
        }
    }
#undef theButton
#undef theCombo
#undef theList
#undef theRadio
#undef theBar
#undef theLabel
#undef theBitmap
#undef theText

    static intptr_t Op(void* inTable, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f)
    {
#define t static_cast<UIControlsTable*>(inTable)
#define m reinterpret_cast<UIManager*>(d)
#define theForm reinterpret_cast<UIForm*>(a)
#define theFont reinterpret_cast<GEFont*>(a)
#define theFile reinterpret_cast<FSFileSpec*>(a)
        switch (inOp) {
        case EeUiOp::TblWindowWidth: return m->GetRasterizer()->GetWindow(m->GetScreen())->GetWidth();
        case EeUiOp::TblWindowHeight: return m->GetRasterizer()->GetWindow(m->GetScreen())->GetHeight();
        case EeUiOp::TblFormById: return (intptr_t)m->GetFormByID((unsigned long)a);
        case EeUiOp::TblFormExclusive: return theForm->GetUseExclusiveCoords();
        case EeUiOp::TblFormDesignedHeight: return theForm->GetDesignedHeight();
        case EeUiOp::TblFormGdi: return theForm->GetUseGDIForText();
        case EeUiOp::TblFontHeight: return theFont->GetHeight();
        case EeUiOp::TblFontHasBitmap: return theFont->GetFontBitmap() != NULL;
        case EeUiOp::TblStringLength: return reinterpret_cast<const UWideString*>(a)->GetLength();
        case EeUiOp::TblFileOpen: theFile->Open(kFileAccessRead); return 0;
        case EeUiOp::TblFileClose: theFile->Close(); return 0;
        case EeUiOp::TblWrap: return (intptr_t) new UIControlsTable(reinterpret_cast<EeUiControlsTable*>(a));
        case EeUiOp::TblDelete: delete reinterpret_cast<UIControlsTable*>(a); return 0;
        case EeUiOp::TblReserveIndex: theForm->ReserveControlIndex((unsigned long)b); return 0;
        case EeUiOp::TblStringDelete: delete reinterpret_cast<UWideString*>(a); return 0;
        case EeUiOp::TblStringsDelete: delete reinterpret_cast<std::vector<UWideString>*>(a); return 0;
        case EeUiOp::TblFontGet: return (intptr_t)UIFontTable::GetFont((unsigned long)a);
        case EeUiOp::TblFontClosest: return (intptr_t)UIFontTable::CreateClosestFont((unsigned long)a, (unsigned long)b);
        case EeUiOp::TblFontRelease: theFont->Dereference(); return 0;
        case EeUiOp::TblFontBitmapNew: new GEFontBitmap(m->GetRasterizer(), theFont, m->GetScreen()); return 0;
        case EeUiOp::TblMake: return (intptr_t)Make(*reinterpret_cast<const EeUiTableMake*>(a));
        case EeUiOp::TblSet: Set(t, reinterpret_cast<UIControl*>(a), (EeUiTableSet)b, c, d, f); return 0;
        case EeUiOp::TblUserDefined: return (intptr_t)m->CreateUserDefinedControl(*t);
        case EeUiOp::TblEventNew: return (intptr_t)theForm->GetManager()->CreateControlEvent(theForm, (unsigned long)b);
        case EeUiOp::TblEventRelease: reinterpret_cast<UIEvent*>(a)->Dereference(); return 0;
        case EeUiOp::TblFormControl: return (intptr_t)theForm->GetControl((unsigned long)b);
        default: return 0;
        }
    }
#undef t
#undef m
#undef theForm
#undef theFont
#undef theFile
};

// Creates an empty control description (a bitmap with no font, files or layout)
extern "C" intptr_t ee_uicpp_table_host(EE_UI_HOST_ARGS) { return UIControlsTableHost::Op(inObj, inOp, a, b, c, d, f); }
UIControlsTable::UIControlsTable() : mRust(ee_ui_controls_table_new(this, ee_ui_table_host)) {}

// Destroys the control description, releasing its font
UIControlsTable::~UIControlsTable() { ee_ui_controls_table_free(this->mRust); }

// port: see the header
long UIControlsTable::MoveControlsHorizontally(long inFormID, long inFirstIndex, long inLastIndex, float inLeftDelta,
                                               float inRightDelta)
{
    float f[4] = {inLeftDelta, inRightDelta, 0.0f, 0.0f};
    return (long)ee_ui_controls_tables(EeUiTablesCall::MoveControlsHorizontally, inFormID,
                                       (intptr_t)((uint64_t)(uint32_t)inFirstIndex | (uint64_t)(uint32_t)inLastIndex << 32),
                                       NULL, f);
}

// Sets text to transparent in the description's text colour
void UIControlsTable::SetupGEText(GEText* inText) const
{
    inText->SetIsTransparent(true);
    inText->SetFgColor(this->GetTextForeground());
}
