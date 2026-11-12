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
// The combo box's logic is Rust (rust/crates/ee_ui/src/combo.rs, docs/RUST_PLAN.md R6): the constructor's geometry,
// opening and closing the list (where it opens, the popup, the capture), the mouse and keyboard routing, the
// selection, the line edits' reopen, SetLocation, ChangeWidth and what CopyData shows. The members below forward to it
// with the object's members from mTextBox on as the pinned record EeUiComboFields; the parts stay here, reached
// through the ee_uicpp_combo_* host calls (UIComboBox::Rust). Drawing and the parts' lifetimes stay C++.
#include <windows.h>
#include <mbstring.h>
#include <stddef.h>
#include <stdlib.h>

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

// --- the Rust combo box's view of the object (ee_ui/src/combo_ffi.rs) ------------------------------------------------

// A nested struct: a member of UIComboBox, so it has the class's friendships (UIControl's, UITextLabel's, UIListBox's)
struct UIComboBox::Rust {
    struct Refs {
        EeUiComboRefs mRefs;
        Refs(const UIComboBox* inCombo, void* inContext = NULL)
        {
            UIComboBox* theCombo = const_cast<UIComboBox*>(inCombo);
            UIComboBox::Rust::CheckLayout();
            this->mRefs.combo = theCombo;
            this->mRefs.fields = reinterpret_cast<EeUiComboFields*>(&theCombo->mTextBox);
            this->mRefs.rect = reinterpret_cast<const EeUiRect*>(&theCombo->mRectangle);
            this->mRefs.index = &theCombo->mIndex;
            this->mRefs.ctx = inContext;
        }
        operator const EeUiComboRefs*() const { return &this->mRefs; }
    };
    // UpdateTranslatedKey's key
    struct Key {
        const char* mKey;
        short mRepeat;
    };
    static void CheckLayout()
    {
        // the record's offsets from mTextBox (ee_ui/src/combo.rs ComboFields)
#define EE_UI_COMBO_FIELD(member, field)                                                                              \
    static_assert(offsetof(UIComboBox, member) - offsetof(UIComboBox, mTextBox) == offsetof(EeUiComboFields, field),   \
                  "ee_ui/src/combo.rs")
        EE_UI_COMBO_FIELD(mTextBox, text_box);
        EE_UI_COMBO_FIELD(mTextBoxType, text_box_type);
        EE_UI_COMBO_FIELD(mCurrentSelection, current_selection);
        EE_UI_COMBO_FIELD(mMaxListBoxHeight, max_list_box_height);
        EE_UI_COMBO_FIELD(mIsListBoxOpen, is_list_box_open);
        EE_UI_COMBO_FIELD(mReleasedOnce, released_once);
        EE_UI_COMBO_FIELD(mCopiedListBoxOpen, copied_list_box_open);
        EE_UI_COMBO_FIELD(mListBoxAlwaysDirty, list_box_always_dirty);
        EE_UI_COMBO_FIELD(mCopiedHasFocus, copied_has_focus);
#undef EE_UI_COMBO_FIELD
        static_assert(sizeof(UIComboBox) - offsetof(UIComboBox, mTextBox) >= sizeof(EeUiComboFields), "ee_ui/src/combo.rs");
        static_assert(sizeof(GERectangle) == sizeof(EeUiRect), "ee_ui/src/rect.rs");
        static_assert(sizeof(((UIComboBox*)0)->mIndex) == sizeof(unsigned long), "ee_ui/src/combo_ffi.rs");
    }
    static UIComboBox* Combo(void* inCombo) { return static_cast<UIComboBox*>(inCombo); }
    static EeUiRect Rect(const GERectangle& inRectangle)
    {
        return EeUiRect{inRectangle.mBottom, inRectangle.mLeft, inRectangle.mRight, inRectangle.mTop};
    }
    static GERectangle Rect(const EeUiRect& inRectangle)
    {
        return GERectangle(inRectangle.left, inRectangle.top, inRectangle.right, inRectangle.bottom);
    }
    static EeUiPoint Point(const GE2DPoint& inPoint) { return EeUiPoint{inPoint.GetX(), inPoint.GetY()}; }
    // the parts as ee_ui/src/combo.rs ComboPart numbers them
    static UIControl* Part(void* inCombo, int inPart)
    {
        UIComboBox* theCombo = Combo(inCombo);
        switch (inPart) {
        case 0:
            return theCombo->mTextBox;
        case 1:
            return theCombo->mDropButton;
        case 2:
            return theCombo->mListBox;
        case 3:
            return theCombo->mEditBox;
        default:
            return theCombo->mTextLabel;
        }
    }
    static bool HasLabel(void* inCombo) { return Combo(inCombo)->mTextLabel != NULL; }
    static EeUiRect RectOf(void* inCombo, int inPart) { return Rect(Part(inCombo, inPart)->GetRectangle()); }
    static unsigned long TextBoxBorder(void* inCombo) { return Combo(inCombo)->mTextBox->GetBorderThickness(); }
    static UILBString* List(void* inCombo) { return Combo(inCombo)->mListBox; }
    static unsigned long LineHeight(void* inCombo) { return Combo(inCombo)->mListBox->mLineHeight; }
    static unsigned long ListBorder(void* inCombo) { return Combo(inCombo)->mListBox->GetBorderThickness(); }
    static unsigned long WindowHeight(void* inCombo)
    {
        UIComboBox* theCombo = Combo(inCombo);
        return theCombo->GetRasterizer()->GetWindow(theCombo->GetScreenIndex())->GetHeight();
    }
    static const char* EditText(void* inCombo) { return Combo(inCombo)->mEditBox->GetText(0); }
    static void ShowLine(void* inCombo, unsigned long inLine)
    {
        UIComboBox* theCombo = Combo(inCombo);
        if (theCombo->mTextLabel)
            theCombo->mTextLabel->SetText(theCombo->mListBox->GetData(inLine), true);
        else
            theCombo->mEditBox->SetText(theCombo->mListBox->GetData(inLine));
    }
    static void ClearText(void* inCombo)
    {
        UIComboBox* theCombo = Combo(inCombo);
        if (theCombo->mTextLabel)
            theCombo->mTextLabel->SetText("", true);
        else
            theCombo->mEditBox->SetText("");
    }
    static void DeleteText(void* inCombo)
    {
        UIComboBox* theCombo = Combo(inCombo);
        if (theCombo->mTextLabel)
            theCombo->mTextLabel->DeleteLine(0);
        else
            theCombo->mEditBox->DeleteText();
    }
    static long PartMouseDown(void* inCombo, int inPart) { return Part(inCombo, inPart)->MouseLButtonDown(); }
    static long PartMouseUp(void* inCombo, int inPart) { return Part(inCombo, inPart)->MouseLButtonUp(); }
    static long PartMouseMove(void* inCombo, int inPart, float inX, float inY)
    {
        return Part(inCombo, inPart)->MouseMove(GE2DPoint(inX, inY));
    }
    static long PartUpdateKeyboard(void* inCombo, int inPart) { return Part(inCombo, inPart)->UpdateKeyboard(); }
    static long PartTranslatedKey(void* inCombo, int inPart, void* inKey)
    {
        Key* theKey = static_cast<Key*>(inKey);
        return Part(inCombo, inPart)->UpdateTranslatedKey(theKey->mKey, theKey->mRepeat);
    }
    static void PartTabbed(void* inCombo, int inPart, bool inTo)
    {
        if (inTo)
            Part(inCombo, inPart)->TabbedTo();
        else
            Part(inCombo, inPart)->TabbedFrom();
    }
    static void CopyListColours(void* inCombo)
    {
        UIComboBox* theCombo = Combo(inCombo);
        GEText* theText;
        if (theCombo->mTextLabel)
            theText = theCombo->mTextLabel->GetGEText();
        else
            theText = theCombo->mEditBox->GetGEText();
        GEText* theListText = theCombo->mListBox->GetGEText();
        theListText->SetFgColor(theText->GetFgColor());
        theListText->SetBgColor(theText->GetBgColor());
        theListText->SetIsTransparent(theText->GetIsTransparent());
    }
    static void SetFocusRect(void* inCombo, const EeUiRect* inRectangle)
    {
        Combo(inCombo)->mSelectedBackground.SetRectangle(Rect(*inRectangle));
    }
    static void CaptureMouse(void* inCombo) { Combo(inCombo)->CaptureMouse(); }
    static void CaptureKeyboard(void* inCombo) { Combo(inCombo)->CaptureKeyboard(); }
    static bool HasMouseCapture(void* inCombo) { return Combo(inCombo)->GetHasMouseCapture(); }
    static EeUiPoint MouseLocation(void* inCombo) { return Point(Combo(inCombo)->GetMouseLocation()); }
    static bool KeyDown(int inKey)
    {
        switch (inKey) {
        case 0:
            return UIControl::DownKeyPressed();
        case 1:
            return UIControl::UpKeyPressed();
        case 2:
            return UIControl::HomeKeyPressed();
        case 3:
            return UIControl::EndKeyPressed();
        default:
            return UIControl::sEnterKey->GetValue() != 0;
        }
    }
    static void OpenList(void* inCombo) { Combo(inCombo)->OpenListBox(); }
    static void CloseList(void* inCombo) { Combo(inCombo)->CloseListBox(); }
    static long AddString(void* inCombo) { return Combo(inCombo)->AddString(); }
    static void ChangeSelection(void* inCombo) { Combo(inCombo)->ChangeSelection(); }
    static void ProcessArrow(void* inCombo, bool inDown)
    {
        if (inDown)
            Combo(inCombo)->ProcessDown();
        else
            Combo(inCombo)->ProcessUp();
    }
};

typedef UIComboBox::Rust UIComboBoxRust;

// The host calls of ee_ui/src/combo_ffi.rs
extern "C" bool ee_uicpp_combo_has_label(void* inCombo) { return UIComboBoxRust::HasLabel(inCombo); }
extern "C" EeUiRect ee_uicpp_combo_rect_of(void* inCombo, int inPart) { return UIComboBoxRust::RectOf(inCombo, inPart); }
extern "C" unsigned long ee_uicpp_combo_text_box_border(void* inCombo) { return UIComboBoxRust::TextBoxBorder(inCombo); }
extern "C" unsigned long ee_uicpp_combo_line_count(void* inCombo) { return UIComboBoxRust::List(inCombo)->GetLineCount(); }
extern "C" const char* ee_uicpp_combo_line(void* inCombo, unsigned long inLine) { return UIComboBoxRust::List(inCombo)->GetData(inLine); }
extern "C" unsigned long ee_uicpp_combo_line_height(void* inCombo) { return UIComboBoxRust::LineHeight(inCombo); }
extern "C" unsigned long ee_uicpp_combo_list_border(void* inCombo) { return UIComboBoxRust::ListBorder(inCombo); }
extern "C" bool ee_uicpp_combo_list_scroll_bar_rect(void* inCombo, EeUiRect* outRectangle)
{
    const UIScrollBar* theBar = UIComboBoxRust::List(inCombo)->GetScrollBar();
    if (!theBar)
        return false;
    *outRectangle = UIComboBoxRust::Rect(theBar->GetRectangle());
    return true;
}
extern "C" bool ee_uicpp_combo_list_scroll_bar_captured(void* inCombo) { return UIComboBoxRust::List(inCombo)->GetScrollBarCaptured(); }
extern "C" long ee_uicpp_combo_list_selection(void* inCombo, unsigned long* outSelection)
{
    return UIComboBoxRust::List(inCombo)->GetCurrentSelection(*outSelection);
}
extern "C" void ee_uicpp_combo_list_change_selection(void* inCombo, unsigned long inLine, bool inFire, bool inScroll)
{
    UIComboBoxRust::List(inCombo)->ChangeSelectionIndex(inLine, inFire, inScroll);
}
extern "C" void ee_uicpp_combo_list_insert_line(void* inCombo, const char* inText, unsigned long inLine)
{
    UIComboBoxRust::List(inCombo)->InsertLine(UWideString(inText), inLine);
}
extern "C" void ee_uicpp_combo_list_delete_line(void* inCombo, unsigned long inLine) { UIComboBoxRust::List(inCombo)->DeleteLine(inLine); }
extern "C" void ee_uicpp_combo_list_delete_all(void* inCombo) { UIComboBoxRust::List(inCombo)->DeleteAllEntries(); }
extern "C" void ee_uicpp_combo_list_change_width(void* inCombo, unsigned long inWidth) { UIComboBoxRust::List(inCombo)->ChangeWidth(inWidth); }
extern "C" unsigned long ee_uicpp_combo_window_height(void* inCombo) { return UIComboBoxRust::WindowHeight(inCombo); }
extern "C" const char* ee_uicpp_combo_edit_text(void* inCombo) { return UIComboBoxRust::EditText(inCombo); }
extern "C" void ee_uicpp_combo_show_line(void* inCombo, unsigned long inLine) { UIComboBoxRust::ShowLine(inCombo, inLine); }
extern "C" void ee_uicpp_combo_clear_text(void* inCombo) { UIComboBoxRust::ClearText(inCombo); }
extern "C" void ee_uicpp_combo_delete_text(void* inCombo) { UIComboBoxRust::DeleteText(inCombo); }
extern "C" long ee_uicpp_combo_part_mouse_down(void* inCombo, int inPart) { return UIComboBoxRust::PartMouseDown(inCombo, inPart); }
extern "C" long ee_uicpp_combo_part_mouse_up(void* inCombo, int inPart) { return UIComboBoxRust::PartMouseUp(inCombo, inPart); }
extern "C" long ee_uicpp_combo_part_mouse_move(void* inCombo, int inPart, float inX, float inY)
{
    return UIComboBoxRust::PartMouseMove(inCombo, inPart, inX, inY);
}
extern "C" long ee_uicpp_combo_list_wheel(void* inCombo, long inDelta) { return UIComboBoxRust::List(inCombo)->MouseWheelTurned(inDelta); }
extern "C" long ee_uicpp_combo_part_update_keyboard(void* inCombo, int inPart) { return UIComboBoxRust::PartUpdateKeyboard(inCombo, inPart); }
extern "C" long ee_uicpp_combo_part_translated_key(void* inCombo, int inPart, void* inKey)
{
    return UIComboBoxRust::PartTranslatedKey(inCombo, inPart, inKey);
}
extern "C" void ee_uicpp_combo_part_tabbed(void* inCombo, int inPart, bool inTo) { UIComboBoxRust::PartTabbed(inCombo, inPart, inTo); }
extern "C" void ee_uicpp_combo_part_set_rectangle(void* inCombo, int inPart, const EeUiRect* inRectangle)
{
    UIComboBoxRust::Part(inCombo, inPart)->SetRectangle(UIComboBoxRust::Rect(*inRectangle));
}
extern "C" void ee_uicpp_combo_part_set_location(void* inCombo, int inPart, float inX, float inY)
{
    UIComboBoxRust::Part(inCombo, inPart)->SetLocation(GE2DPoint(inX, inY));
}
extern "C" long ee_uicpp_combo_part_copy_data(void* inCombo, int inPart) { return UIComboBoxRust::Part(inCombo, inPart)->CopyData(); }
extern "C" void ee_uicpp_combo_copy_list_colours(void* inCombo) { UIComboBoxRust::CopyListColours(inCombo); }
extern "C" void ee_uicpp_combo_set_focus_rect(void* inCombo, const EeUiRect* inRectangle) { UIComboBoxRust::SetFocusRect(inCombo, inRectangle); }
extern "C" long ee_uicpp_combo_base_copy_data(void* inCombo) { return UIComboBoxRust::Combo(inCombo)->UIControl::CopyData(); }
extern "C" bool ee_uicpp_combo_tabbing_on(void* inCombo) { return UIComboBoxRust::Combo(inCombo)->GetForm()->GetTabbingOn(); }
extern "C" unsigned long ee_uicpp_combo_tab_stop(void* inCombo) { return UIComboBoxRust::Combo(inCombo)->GetForm()->GetTabStop(); }
extern "C" void ee_uicpp_combo_set_popup(void* inCombo, bool inThis)
{
    UIComboBox* theCombo = UIComboBoxRust::Combo(inCombo);
    theCombo->GetForm()->SetPopup(inThis ? theCombo : NULL);
}
extern "C" void ee_uicpp_combo_set_dirty(void* inCombo) { UIComboBoxRust::Combo(inCombo)->SetDirty(true); }
extern "C" void ee_uicpp_combo_set_rectangle(void* inCombo, const EeUiRect* inRectangle)
{
    UIComboBoxRust::Combo(inCombo)->SetRectangle(UIComboBoxRust::Rect(*inRectangle));
}
extern "C" void ee_uicpp_combo_base_set_rectangle(void* inCombo, const EeUiRect* inRectangle)
{
    UIComboBoxRust::Combo(inCombo)->UIControl::SetRectangle(UIComboBoxRust::Rect(*inRectangle));
}
extern "C" void ee_uicpp_combo_base_set_location(void* inCombo, float inX, float inY)
{
    UIComboBoxRust::Combo(inCombo)->UIControl::SetLocation(GE2DPoint(inX, inY));
}
extern "C" void ee_uicpp_combo_set_always_dirty(void* inCombo, bool inOn) { UIComboBoxRust::Combo(inCombo)->SetAlwaysDirty(inOn); }
extern "C" void ee_uicpp_combo_fire(void* inCombo, unsigned long inEvent) { UIComboBoxRust::Combo(inCombo)->FireTriggerEvent(inEvent); }
extern "C" void ee_uicpp_combo_capture_mouse(void* inCombo) { UIComboBoxRust::CaptureMouse(inCombo); }
extern "C" void ee_uicpp_combo_capture_keyboard(void* inCombo) { UIComboBoxRust::CaptureKeyboard(inCombo); }
extern "C" void ee_uicpp_combo_release_mouse(void* inCombo) { UIComboBoxRust::Combo(inCombo)->ReleaseCapturedMouse(); }
extern "C" void ee_uicpp_combo_release_keyboard(void* inCombo) { UIComboBoxRust::Combo(inCombo)->ReleaseCapturedKeyboard(); }
extern "C" bool ee_uicpp_combo_has_mouse_capture(void* inCombo) { return UIComboBoxRust::HasMouseCapture(inCombo); }
extern "C" EeUiPoint ee_uicpp_combo_mouse_location(void* inCombo) { return UIComboBoxRust::MouseLocation(inCombo); }
extern "C" bool ee_uicpp_combo_key(void* inCombo, int inKey) { return UIComboBoxRust::KeyDown(inKey); }
extern "C" void ee_uicpp_combo_open_list(void* inCombo) { UIComboBoxRust::OpenList(inCombo); }
extern "C" void ee_uicpp_combo_close_list(void* inCombo) { UIComboBoxRust::CloseList(inCombo); }
extern "C" long ee_uicpp_combo_add_string(void* inCombo) { return UIComboBoxRust::AddString(inCombo); }
extern "C" void ee_uicpp_combo_change_selection(void* inCombo) { UIComboBoxRust::ChangeSelection(inCombo); }
extern "C" void ee_uicpp_combo_process_arrow(void* inCombo, bool inDown) { UIComboBoxRust::ProcessArrow(inCombo, inDown); }

// FUNCTION: LLE 0x1006150f
// Creates a combo box: a text label (type 1) or edit box, a drop button and a hidden list box showing the first entry
UIComboBox::UIComboBox(unsigned long inId, unsigned long inTextBoxType, std::vector<UWideString> inData,
                       GEFont* inFont, const GERectangle& inRectangle, unsigned long inMaxListBoxHeight,
                       UIForm* inForm, unsigned long inIndex, unsigned long inMaxChars,
                       unsigned long inBorderThickness)
    : UIControl(kControlTypeComboBox, inId, inRectangle, inForm, inIndex, 0)
{
    GERectangle theRectangle;
    UWideString theEmpty("");
    GEColor theColor;
    this->mTextBoxType = inTextBoxType;
    this->mMaxListBoxHeight = inMaxListBoxHeight;
    this->mReleasedOnce = false;
    theRectangle = UIComboBoxRust::Rect(ee_ui_combo_drop_rect(UIComboBoxRust::Rect(this->mRectangle), UIComboBoxRust::Rect(inRectangle)));
    GEBitmap* theNormal = inForm->GetManager()->CreateBitmapForScaledControl(
        inForm->GetManager()->GetSBBitmapDownNormal(), theRectangle);
    GEBitmap* thePressed = inForm->GetManager()->CreateBitmapForScaledControl(
        inForm->GetManager()->GetSBBitmapDownPressed(), theRectangle);
    this->mDropButton = new UIButton(this->GetId(), theNormal, NULL, thePressed, theRectangle, this->GetForm(), this->GetIndex(), 0);
    theNormal->Dereference();
    thePressed->Dereference();
    GERectangle theTextRectangle = UIComboBoxRust::Rect(ee_ui_combo_text_rect(
        UIComboBoxRust::Rect(this->mRectangle), UIComboBoxRust::Rect(inRectangle), this->mDropButton->GetWidth()));
    if (inTextBoxType == 1) {
        this->mTextLabel = new UITextLabel(theEmpty, inFont, this->GetId(), theTextRectangle, this->GetForm(), 0,
                                     this->GetIndex(), inBorderThickness);
        this->mEditBox = NULL;
        this->mTextBox = this->mTextLabel;
    } else {
        this->mEditBox = new UIEditBox(this->GetId(), inFont, inMaxChars, false, theTextRectangle, this->GetForm(),
                                 this->GetIndex(), inBorderThickness, false, false);
        this->mTextLabel = NULL;
        this->mTextBox = this->mEditBox;
    }
    this->mListBox = new UILBString(this->GetId(), inData, inFont, true,
                              UIComboBoxRust::Rect(ee_ui_combo_list_rect(UIComboBoxRust::Rect(this->mRectangle),
                                                                         this->mTextBox->GetRectangle().GetBottom(),
                                                                         inMaxListBoxHeight)),
                              this->GetForm(), this->GetIndex(), inBorderThickness);
    this->mListBox->ChangeSelectionIndex(0, true, false);
    this->mIsListBoxOpen = false;
    this->mListBox->SetBgOpacity(1.0f);
    this->mListBox->SetBgColor(GEColor(0.0f, 0.0f, 0.0f));
    this->mListBox->SetScrollBarBgOpacity(1.0f);
    this->mListBox->SetScrollBarBgColor(GEColor(0.0f, 0.0f, 0.0f));
    this->mCurrentSelection = -1;
    this->mCopiedListBoxOpen = false;
    this->mListBoxAlwaysDirty = false;
    if (this->mListBox->GetLineCount() > 0) {
        if (this->mTextLabel)
            this->mTextLabel->SetText(this->mListBox->GetData(0), true);
        else
            this->mEditBox->SetText(this->mListBox->GetData(0));
        this->mCurrentSelection = 0;
    }
    UIControl::SetRectangle(GERectangle(this->mTextBox->GetRectangle().GetTopLeft(),
                                        this->mDropButton->GetRectangle().GetBottomRight()));
    this->mCopiedHasFocus = false;
    this->mListBox->GetSelectedBackground()->GetColorPerVertex(theColor, theColor, theColor, theColor);
    this->mSelectedBackground.SetColor(theColor);
}

// FUNCTION: LLE 0x10061978
// Destroys the combo box and its text box, list box and drop button
UIComboBox::~UIComboBox()
{
    delete this->mTextBox;
    delete this->mListBox;
    delete this->mDropButton;
}

// FUNCTION: LLE 0x100619c5
// Selects the edit box's text in the list (OK), or adds it at the top and selects it (not found); fails on empty text
long UIComboBox::AddString()
{
    return ee_ui_combo_add_string(UIComboBoxRust::Refs(this));
}

// FUNCTION: LLE 0x10061a86
// Appends a line to the list, reopening an open list so it resizes
void UIComboBox::AppendLine(UWideString& inLine)
{
    this->SetDirty(true);
    this->mListBox->AppendLine(inLine);
    ee_ui_combo_reopen(UIComboBoxRust::Refs(this));
}

// FUNCTION: LLE 0x10061ac3
// Appends several lines to the list, reopening an open list so it resizes
void UIComboBox::AppendStringArray(std::vector<UWideString>& inLines)
{
    this->SetDirty(true);
    this->mListBox->AppendStringArray(inLines);
    ee_ui_combo_reopen(UIComboBoxRust::Refs(this));
}

// FUNCTION: LLE 0x10061b00
// Shows the selected entry in the text box and fires the selection-changed trigger
void UIComboBox::ChangeSelection()
{
    ee_ui_combo_change_selection(UIComboBoxRust::Refs(this));
}

// FUNCTION: LLE 0x10061b5b
// Resizes the text box, the list and the control to a new width
void UIComboBox::ChangeWidth(unsigned long inWidth)
{
    ee_ui_combo_change_width(UIComboBoxRust::Refs(this), inWidth);
}

// FUNCTION: LLE 0x10061bdf
// Closes the drop-down list, shrinks the control back to the text box and fires the list-closed trigger
void UIComboBox::CloseListBox()
{
    ee_ui_combo_close_list_box(UIComboBoxRust::Refs(this));
}

// FUNCTION: LLE 0x10061c92
// Snapshots the parts for drawing: the open list in the text box's colours, or the focus highlight when tabbed to
long UIComboBox::CopyData()
{
    return ee_ui_combo_copy_data(UIComboBoxRust::Refs(this));
}

// FUNCTION: LLE 0x10061dd0
// Empties the list and the text box and clears the selection
void UIComboBox::DeleteAllEntries()
{
    ee_ui_combo_delete_all_entries(UIComboBoxRust::Refs(this));
}

// FUNCTION: LLE 0x10061e62
// Removes a list line and shows the entry now selected, or clears the text and the selection when none is
void UIComboBox::DeleteLine(unsigned long inLine)
{
    ee_ui_combo_delete_line(UIComboBoxRust::Refs(this), inLine);
}

// FUNCTION: LLE 0x10061f56
// Draws the text box, the focus highlight, the drop button and the open list
long UIComboBox::DrawPolygons()
{
    long theResult;
    UIControl::DrawPolygons();
    this->mTextBox->DrawPolygons();
    if (this->mCopiedHasFocus)
        this->GetRasterizer()->Draw(&this->mSelectedBackground, this->GetScreenIndex());
    theResult = this->mDropButton->DrawPolygons();
    if (this->mCopiedListBoxOpen)
        theResult = this->mListBox->DrawPolygons();
    return theResult;
}

// FUNCTION: LLE 0x10061fb6
// Draws the text of the text box, the drop button and the open list
long UIComboBox::DrawTextA()
{
    long theResult;
    this->mDrawCount--;
    this->mTextBox->DrawTextA();
    theResult = this->mDropButton->DrawTextA();
    if (this->mCopiedListBoxOpen)
        theResult = this->mListBox->DrawTextA();
    return theResult;
}

// FUNCTION: LLE 0x10061feb
// Closes an open list and releases mouse and keyboard capture when the control is hidden
void UIComboBox::HideControl()
{
    ee_ui_combo_hide_control(UIComboBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006200f
// Inserts a line into the list at an index, reopening an open list so it resizes
void UIComboBox::InsertLine(const UWideString& inLine, unsigned long inIndex)
{
    ee_ui_combo_insert_line(UIComboBoxRust::Refs(this), inLine, inIndex);
}

// FUNCTION: LLE 0x10062053
// Handles a click: opens the list (or starts editing the edit box), or picks in / closes the open list
long UIComboBox::MouseLButtonDown()
{
    return ee_ui_combo_mouse_l_button_down(UIComboBoxRust::Refs(this));
}

// FUNCTION: LLE 0x10062144
// Handles a release: commits the entry under the mouse and closes the list, else keeps it open for a second click
long UIComboBox::MouseLButtonUp()
{
    return ee_ui_combo_mouse_l_button_up(UIComboBoxRust::Refs(this));
}

// FUNCTION: LLE 0x10062252
// Passes mouse movement to the text box, the drop button and the open list
long UIComboBox::MouseMove(const GE2DPoint& inLocation)
{
    return ee_ui_combo_mouse_move(UIComboBoxRust::Refs(this), UIComboBoxRust::Point(inLocation));
}

// FUNCTION: LLE 0x1006228c
// Scrolls the open list with the mouse wheel; fails while the list is closed
long UIComboBox::MouseWheelTurned(long inDelta)
{
    return ee_ui_combo_mouse_wheel_turned(UIComboBoxRust::Refs(this), inDelta);
}

// FUNCTION: LLE 0x100622aa
// Opens the drop-down list below the text box, or above it when there is more room, and captures input
void UIComboBox::OpenListBox()
{
    ee_ui_combo_open_list_box(UIComboBoxRust::Refs(this));
}

// FUNCTION: LLE 0x10062462
// Shows the list's current selection in the text box after a Down key, or clears the text
void UIComboBox::ProcessDown()
{
    ee_ui_combo_process_arrow(UIComboBoxRust::Refs(this));
}

// Shows the list's current selection in the text box after an Up key, or clears the text
// (ICF-folded with ProcessDown 0x10062462)
void UIComboBox::ProcessUp()
{
    ee_ui_combo_process_arrow(UIComboBoxRust::Refs(this));
}

// FUNCTION: LLE 0x100624db
// Selects a list entry by index and shows it; out-of-range indices are ignored
void UIComboBox::SetCurrentSelection(unsigned long inIndex)
{
    ee_ui_combo_set_current_selection(UIComboBoxRust::Refs(this), inIndex);
}

// FUNCTION: LLE 0x1006252f
// Moves the combo box, keeping a list that opened upwards above the text box
void UIComboBox::SetLocation(const GE2DPoint& inLocation)
{
    ee_ui_combo_set_location(UIComboBoxRust::Refs(this), UIComboBoxRust::Point(inLocation));
}

// FUNCTION: LLE 0x10062619
// Takes keyboard focus when tabbed to (the edit box too, if any)
void UIComboBox::TabbedTo()
{
    ee_ui_combo_tabbed(UIComboBoxRust::Refs(this), true);
}

// FUNCTION: LLE 0x10062649
// Gives up focus when tabbed away: closes the list and releases mouse and keyboard
void UIComboBox::TabbedFrom()
{
    ee_ui_combo_tabbed(UIComboBoxRust::Refs(this), false);
}

// FUNCTION: LLE 0x1006268a
// Handles keys: Down opens the list; in the open list Enter commits and Up/Down move; else the edit box gets them
long UIComboBox::UpdateKeyboard()
{
    return ee_ui_combo_update_keyboard(UIComboBoxRust::Refs(this));
}

// FUNCTION: LLE 0x100627a7
// Passes a typed character to the open list, else to the text box
long UIComboBox::UpdateTranslatedKey(const char* inKey, short inRepeat)
{
    UIComboBoxRust::Key theKey = {inKey, inRepeat};
    return ee_ui_combo_update_translated_key(UIComboBoxRust::Refs(this, &theKey));
}

// Header-inline members first emitted here, in class-definition order (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x100602e4
// SYNTHETIC: LLE 0x10060308
// SYNTHETIC: LLE 0x10060362
// SYNTHETIC: LLE 0x1006037a
// SYNTHETIC: LLE 0x10060381
// SYNTHETIC: LLE 0x10060388
// SYNTHETIC: LLE 0x1006038f
// SYNTHETIC: LLE 0x10060396
// SYNTHETIC: LLE 0x1006039d
// SYNTHETIC: LLE 0x100603b9
// SYNTHETIC: LLE 0x100603d4
// SYNTHETIC: LLE 0x100603db
// SYNTHETIC: LLE 0x100603f1
// SYNTHETIC: LLE 0x1006040f
// SYNTHETIC: LLE 0x1006042c
// SYNTHETIC: LLE 0x1006044b
// SYNTHETIC: LLE 0x10060468
// SYNTHETIC: LLE 0x100604a7
// SYNTHETIC: LLE 0x100604af
// SYNTHETIC: LLE 0x10060799
// SYNTHETIC: LLE 0x10060b30
// SYNTHETIC: LLE 0x10060b91
// SYNTHETIC: LLE 0x10060cf9
// SYNTHETIC: LLE 0x10060f03
// SYNTHETIC: LLE 0x10060f64
// SYNTHETIC: LLE 0x10060fd7
// SYNTHETIC: LLE 0x1006100b
// SYNTHETIC: LLE 0x10061028
// SYNTHETIC: LLE 0x1006105b
// SYNTHETIC: LLE 0x10061088
// SYNTHETIC: LLE 0x100610e9
// SYNTHETIC: LLE 0x1006110b
// SYNTHETIC: LLE 0x10061112
// SYNTHETIC: LLE 0x10061130
// SYNTHETIC: LLE 0x10061137
// SYNTHETIC: LLE 0x1006113e
// SYNTHETIC: LLE 0x1006114b
// SYNTHETIC: LLE 0x10061152
// SYNTHETIC: LLE 0x10061159
// SYNTHETIC: LLE 0x10061166
// SYNTHETIC: LLE 0x10061178
// SYNTHETIC: LLE 0x1006119a
// SYNTHETIC: LLE 0x100611c5
// SYNTHETIC: LLE 0x100611d7
// SYNTHETIC: LLE 0x100611e9
// SYNTHETIC: LLE 0x100611ff
// SYNTHETIC: LLE 0x100612cb
// SYNTHETIC: LLE 0x1006138e

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x100613ef
// SYNTHETIC: LLE 0x100613f9
// SYNTHETIC: LLE 0x10061409
// SYNTHETIC: LLE 0x10061415
// SYNTHETIC: LLE 0x1006141f
// SYNTHETIC: LLE 0x10061429
// SYNTHETIC: LLE 0x10061439
// SYNTHETIC: LLE 0x10061445
// SYNTHETIC: LLE 0x1006144f
// SYNTHETIC: LLE 0x10061459
// SYNTHETIC: LLE 0x10061469
// SYNTHETIC: LLE 0x10061475
// SYNTHETIC: LLE 0x1006147f
// SYNTHETIC: LLE 0x10061489
// SYNTHETIC: LLE 0x10061499
// SYNTHETIC: LLE 0x100614a5
// SYNTHETIC: LLE 0x100614af
// SYNTHETIC: LLE 0x100614b9
// SYNTHETIC: LLE 0x100614c9
// SYNTHETIC: LLE 0x100614d5
// SYNTHETIC: LLE 0x100614df
// SYNTHETIC: LLE 0x100614e9
// SYNTHETIC: LLE 0x100614f9
// SYNTHETIC: LLE 0x10061505

// The vector<UWideString> copy members, UIListBox<UWideString>'s members in declaration order, the vector
// COMDATs, the InsertLine member template and the lower_bound/sort chains
// TEMPLATE: LLE 0x100627d1
// TEMPLATE: LLE 0x1006281b
// TEMPLATE: LLE 0x1006298d
// TEMPLATE: LLE 0x100629a3
// TEMPLATE: LLE 0x10062c18
// TEMPLATE: LLE 0x10062ca9
// TEMPLATE: LLE 0x10062ccf
// TEMPLATE: LLE 0x10062d24
// TEMPLATE: LLE 0x10062dda
// TEMPLATE: LLE 0x10062e18
// TEMPLATE: LLE 0x10062e20
// TEMPLATE: LLE 0x10063079
// TEMPLATE: LLE 0x100630aa
// TEMPLATE: LLE 0x10063138
// TEMPLATE: LLE 0x10063348
// TEMPLATE: LLE 0x1006335a
// TEMPLATE: LLE 0x10063397
// TEMPLATE: LLE 0x100633b1
// TEMPLATE: LLE 0x100633b8
// TEMPLATE: LLE 0x100633d4
// TEMPLATE: LLE 0x100633db
// TEMPLATE: LLE 0x100633e2
// TEMPLATE: LLE 0x100633e9
// TEMPLATE: LLE 0x100633fc
// TEMPLATE: LLE 0x10063403
// TEMPLATE: LLE 0x1006340a
// TEMPLATE: LLE 0x10063464
// TEMPLATE: LLE 0x1006347a
// TEMPLATE: LLE 0x10063489
// TEMPLATE: LLE 0x10063498
// TEMPLATE: LLE 0x100634b5
// TEMPLATE: LLE 0x100634de
// TEMPLATE: LLE 0x100634e9
// TEMPLATE: LLE 0x100634f6
// TEMPLATE: LLE 0x10063513
// TEMPLATE: LLE 0x10063645
// TEMPLATE: LLE 0x1006366d
// TEMPLATE: LLE 0x100636a0
// TEMPLATE: LLE 0x100636e2
// TEMPLATE: LLE 0x100637d9
// TEMPLATE: LLE 0x1006388e
// TEMPLATE: LLE 0x10063899
// TEMPLATE: LLE 0x100638af
// TEMPLATE: LLE 0x100638b6
// TEMPLATE: LLE 0x100638bd
// TEMPLATE: LLE 0x10063951
// TEMPLATE: LLE 0x10063a48
// TEMPLATE: LLE 0x10063b0b
// TEMPLATE: LLE 0x10063bdf
// TEMPLATE: LLE 0x10063c48
// TEMPLATE: LLE 0x10063c9d
// TEMPLATE: LLE 0x10063cdb
// TEMPLATE: LLE 0x10063d38
// TEMPLATE: LLE 0x10063d60
// TEMPLATE: LLE 0x10063f1e
// TEMPLATE: LLE 0x10063f3a
// TEMPLATE: LLE 0x10063f66
// TEMPLATE: LLE 0x10063fda
// TEMPLATE: LLE 0x10064015
// TEMPLATE: LLE 0x1006404e
// TEMPLATE: LLE 0x10064063
// TEMPLATE: LLE 0x1006411f
// TEMPLATE: LLE 0x10064179
// TEMPLATE: LLE 0x100641e4
// TEMPLATE: LLE 0x100644be
// TEMPLATE: LLE 0x100644e9
// TEMPLATE: LLE 0x10064500
// TEMPLATE: LLE 0x1006452c
// TEMPLATE: LLE 0x100645fb
// TEMPLATE: LLE 0x100646b6
// TEMPLATE: LLE 0x100646df
// TEMPLATE: LLE 0x10064703
// TEMPLATE: LLE 0x10064720
// TEMPLATE: LLE 0x1006474c
// TEMPLATE: LLE 0x1006475e
// TEMPLATE: LLE 0x10064796
// TEMPLATE: LLE 0x100647bf
// TEMPLATE: LLE 0x10064898
// TEMPLATE: LLE 0x100648d7
// TEMPLATE: LLE 0x1006491b
// TEMPLATE: LLE 0x100649e2
// TEMPLATE: LLE 0x100649e8
// TEMPLATE: LLE 0x10064a25
// TEMPLATE: LLE 0x10064a64
// TEMPLATE: LLE 0x10064b93
// TEMPLATE: LLE 0x10064be0
// TEMPLATE: LLE 0x10064c0b
// TEMPLATE: LLE 0x10064c68
// TEMPLATE: LLE 0x10064c91
// TEMPLATE: LLE 0x10064cb6
// TEMPLATE: LLE 0x10064ccb
// TEMPLATE: LLE 0x10064d69
// TEMPLATE: LLE 0x10064dd7
// TEMPLATE: LLE 0x10064e09
// TEMPLATE: LLE 0x10064e79
// TEMPLATE: LLE 0x10064ec5
