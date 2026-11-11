// C:\EEX\Low-Level Engine\User Interface\UIEditBox.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10069d6b..0x1006de9d: the U2D key init chains (U2DRectangle.h, via the rasterizer header), the ctor, the dtor
// and the out-of-line members in alphabetical order (the multibyte character helper sits right after AddChar, its
// first user). The header-inline members, the implicit copy members and ??_E are emitted first by UIComboBox.cpp.
// UIEMouseWheelScroll.cpp starts at 0x1006de9d.
//
// The text model is Rust (rust/crates/ee_ui/src/edit.rs, docs/RUST_PLAN.md R6): insertion and word wrap, deletion,
// caret moves, selection, scrolling, the caret from a mouse location and the IME composition's bookkeeping. Their
// members below forward to it with the box's members from mCaretX on as the pinned record EeUiEditFields; the lines
// stay here (mLines), read and written through the ee_uicpp_edit_* host calls. The drawing's ranges, rows and IME
// underlines are Rust too (edit_draw.rs), and so is the key dispatch (edit_keys.rs): UpdateKeyboard's repeat, which
// keys move, select, delete, cut, copy, paste, scroll or fire Enter, the clipboard's Cut/Copy/Paste, an IME result's
// typing and a drag's selection and auto-scroll. Rust asks every key through ee_uicpp_edit_key (UIControl's
// predicates stay here) and every timer through ee_uicpp_edit_poll. What stays C++: construction, the draw calls,
// the platform clipboard, mouse capture, focus.
#include <windows.h>
#include <mbstring.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "rust/ee_rust.h"
#include "lle/LowLevelEngine.h"
#include "platform/PlatWindow.h"
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
#include "lle/ISKeyboard.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "lle/UIEditBox.h"

// --- the Rust text model's view of the box (ee_ui/src/edit_ffi.rs) --------------------------------------------------

struct UIEditBoxRust {
    // The members Rust reads and writes: the record from mCaretX on, mTextRectangle, mRectangle, mBorderThickness
    struct Refs {
        EeUiEditRefs mRefs;
        Refs(const UIEditBox* inBox)
        {
            UIEditBox* theBox = const_cast<UIEditBox*>(inBox);
            this->mRefs.edit = theBox;
            this->mRefs.fields = reinterpret_cast<EeUiEditFields*>(&theBox->mCaretX);
            this->mRefs.text = reinterpret_cast<EeUiRect*>(&theBox->mTextRectangle);
            this->mRefs.rect = reinterpret_cast<const EeUiRect*>(&theBox->mRectangle);
            this->mRefs.border = &theBox->mBorderThickness;
        }
        operator const EeUiEditRefs*() const { return &this->mRefs; }
    };
    static EeUiPoint Point(const GE2DPoint& inPoint) { return EeUiPoint{inPoint.GetX(), inPoint.GetY()}; }
    static UIEditBox* Box(void* inBox) { return static_cast<UIEditBox*>(inBox); }
    static void CheckLayout()
    {
        // the record's offsets from mCaretX (ee_ui/src/edit.rs EditFields)
#define EE_UI_EDIT_FIELD(member, field)                                                                               \
    static_assert(offsetof(UIEditBox, member) - offsetof(UIEditBox, mCaretX) == offsetof(EeUiEditFields, field),      \
                  "ee_ui/src/edit.rs")
        EE_UI_EDIT_FIELD(mCaretX, caret_x);
        EE_UI_EDIT_FIELD(mSavedBgOpacity, saved_bg_opacity);
        EE_UI_EDIT_FIELD(mSelectionStartLine, selection_start_line);
        EE_UI_EDIT_FIELD(mCopiedSelectionStartLine, copied_selection_start_line);
        EE_UI_EDIT_FIELD(mCopiedSelectionEndLine, copied_selection_end_line);
        EE_UI_EDIT_FIELD(mMaxChars, max_chars);
        EE_UI_EDIT_FIELD(mCharScroll, char_scroll);
        EE_UI_EDIT_FIELD(mCaretLine, caret_line);
        EE_UI_EDIT_FIELD(mVisibleLineCount, visible_line_count);
        EE_UI_EDIT_FIELD(mIMECompositionLength, ime_composition_length);
        EE_UI_EDIT_FIELD(mIMEStartIndex, ime_start_index);
        EE_UI_EDIT_FIELD(mIMEStartLine, ime_start_line);
        EE_UI_EDIT_FIELD(mCopiedIMEStartIndex, copied_ime_start_index);
        EE_UI_EDIT_FIELD(mCopiedIMEStartLine, copied_ime_start_line);
        EE_UI_EDIT_FIELD(mCopiedIMEEndIndex, copied_ime_end_index);
        EE_UI_EDIT_FIELD(mCopiedIMEEndLine, copied_ime_end_line);
        EE_UI_EDIT_FIELD(mCopiedSelectionEndIndex, copied_selection_end_index);
        EE_UI_EDIT_FIELD(mCopiedSelectionStartIndex, copied_selection_start_index);
        EE_UI_EDIT_FIELD(mSelectionStartIndex, selection_start_index);
        EE_UI_EDIT_FIELD(mCaretIndex, caret_index);
        EE_UI_EDIT_FIELD(mDrawingIndex, drawing_index);
        EE_UI_EDIT_FIELD(mIsTextSelected, is_text_selected);
        EE_UI_EDIT_FIELD(mKeyRepeating, key_repeating);
        EE_UI_EDIT_FIELD(mCursorOn, cursor_on);
        EE_UI_EDIT_FIELD(mSelecting, selecting);
        EE_UI_EDIT_FIELD(mCopiedCursorOn, copied_cursor_on);
        EE_UI_EDIT_FIELD(mCopiedIsTextSelected, copied_is_text_selected);
        EE_UI_EDIT_FIELD(mNumbersOnly, numbers_only);
        EE_UI_EDIT_FIELD(mIsSingleLine, is_single_line);
        EE_UI_EDIT_FIELD(mScrollBarCaptured, scroll_bar_captured);
        EE_UI_EDIT_FIELD(mScrollBarVisible, scroll_bar_visible);
        EE_UI_EDIT_FIELD(mIsPasswordBox, is_password_box);
        EE_UI_EDIT_FIELD(mIMEActive, ime_active);
        EE_UI_EDIT_FIELD(mCopiedIMEActive, copied_ime_active);
        EE_UI_EDIT_FIELD(mSavedAlwaysDirty, saved_always_dirty);
#undef EE_UI_EDIT_FIELD
        static_assert(sizeof(UIEditBox) - offsetof(UIEditBox, mCaretX) >= sizeof(EeUiEditFields), "ee_ui/src/edit.rs");
        static_assert(sizeof(GERectangle) == sizeof(EeUiRect), "ee_ui/src/rect.rs");
        static_assert(sizeof(((UIEditBox*)0)->mBorderThickness) == sizeof(unsigned long), "ee_ui/src/edit_ffi.rs");
    }

    static size_t LineCount(void* inBox) { return Box(inBox)->mLines.size(); }
    static const char* Line(void* inBox, size_t inLine) { return Box(inBox)->mLines[inLine]; }
    static void SetLine(void* inBox, size_t inLine, const char* inText) { Box(inBox)->mLines[inLine] = inText; }
    static void InsertLine(void* inBox, size_t inLine, const char* inText)
    {
        Box(inBox)->mLines.insert(Box(inBox)->mLines.begin() + inLine, UWideString(inText));
    }
    static void EraseLine(void* inBox, size_t inLine) { Box(inBox)->mLines.erase(Box(inBox)->mLines.begin() + inLine); }
    static void KeepFirstLine(void* inBox) { Box(inBox)->mLines.erase(Box(inBox)->mLines.begin() + 1, Box(inBox)->mLines.end()); }
    static UIScrollBar* ScrollBar(void* inBox) { return Box(inBox)->mScrollBar; }
    static void ProcessEnterKey(void* inBox) { Box(inBox)->ProcessEnterKey(); }   // virtual
    static void TranslatedKey(void* inBox, const char* inText) { Box(inBox)->mForm->GetManager()->UpdateTranslatedKey(inText, 1); }
    // EditKey: UIControl's key predicates and the keyboard's A, L and Return entities, asked as the C++ asked them
    static bool Key(int inKey)
    {
        ISEntity* theKey;
        switch (inKey) {
        case 0: return UIControl::ShiftKeyPressed();
        case 1: return UIEditBox::sControlKey->GetValue() != 0;
        case 2: return UIEditBox::sBackSpaceKey->GetValue() != 0;
        case 3: return UIControl::DeleteKeyPressed();
        case 4: return UIControl::RightKeyPressed();
        case 5: return UIControl::LeftKeyPressed();
        case 6: return UIControl::HomeKeyPressed();
        case 7: return UIControl::EndKeyPressed();
        case 8: return UIControl::UpKeyPressed();
        case 9: return UIControl::DownKeyPressed();
        case 10: return UIControl::EnterKeyPressed();
        case 11: return UIControl::CutKeyPressed();
        case 12: return UIControl::CopyKeyPressed();
        case 13: return UIControl::PasteKeyPressed();
        case 14: gISKeyboard->GetEntity(kKeyA, theKey); return theKey->JustPressed();
        case 15: gISKeyboard->GetEntity(kKeyL, theKey); return theKey->JustPressed();
        case 16: gISKeyboard->GetEntity(kKeyReturn, theKey); return theKey->JustPressed();
        case 17: return UIEditBox::sEnterKey->JustPressed();
        case 18: return UIEditBox::sNumEnterKey->JustPressed();
        }
        return false;
    }
    // EditPoll
    static TMPoll* Poll(void* inBox, int inPoll)
    {
        UIEditBox* theBox = Box(inBox);
        switch (inPoll) {
        case 0: return theBox->mRepeatPoll;
        case 1: return theBox->mRepeatDelayPoll;
        case 2: return theBox->mCursorPoll;
        default: return theBox->mScrollPoll;
        }
    }
    // PollOp: GetIsRunning, Poll, Start, Stop
    static unsigned long PollOp(TMPoll* inPoll, int inOp)
    {
        switch (inOp) {
        case 0: return inPoll->GetIsRunning() ? 1 : 0;
        case 1: return inPoll->Poll();
        case 2: inPoll->Start(); return 0;
        default: inPoll->Stop(); return 0;
        }
    }
    static void SetCopiedIME(UIEditBox* inBox, const EeUiEditIme& inIME)
    {
        inBox->mCopiedIMEActive = inIME.active;
        inBox->mCopiedIMEStartLine = inIME.start_line;
        inBox->mCopiedIMEStartIndex = inIME.start_index;
        inBox->mCopiedIMEEndLine = inIME.end_line;
        inBox->mCopiedIMEEndIndex = inIME.end_index;
    }
    // DrawPolygons' IME underlines (ee_ui edit_draw.rs): the copied text measures, the line draws
    struct Underlines {
        UIEditBox* mBox;
        UWideString mString;
        GELine* mLine;
    };
    static unsigned long DisplayLength(void* inContext, unsigned long inLine)
    {
        return static_cast<Underlines*>(inContext)->mBox->mDisplayLines[inLine].GetLength();
    }
    static float PrefixWidth(void* inContext, unsigned long inLine, unsigned long inIndex)
    {
        Underlines* u = static_cast<Underlines*>(inContext);
        u->mString.CopyString(u->mBox->mDisplayLines[inLine], inIndex);
        u->mBox->mCopiedText->SetText(u->mString);
        return u->mBox->mCopiedText->GetTextExtent().GetWidth();
    }
    static size_t CharBytes(void* inContext, unsigned long inLine, unsigned long inIndex)
    {
        Underlines* u = static_cast<Underlines*>(inContext);
        const char* theChar = (const char*)PlatMbsNInc(
            reinterpret_cast<const unsigned char*>((const char*)u->mBox->mDisplayLines[inLine]), inIndex);
        u->mString.CopyString(theChar, 1);
        return PlatMbsNBCnt(reinterpret_cast<const unsigned char*>((const char*)u->mString), 1);
    }
    static float CharWidth(void* inContext, unsigned long, unsigned long)
    {
        Underlines* u = static_cast<Underlines*>(inContext);   // the character CharBytes copied
        u->mBox->mCopiedText->SetText(u->mString);
        return u->mBox->mCopiedText->GetTextExtent().GetWidth();
    }
    static uint8_t Attribute(void* inContext, int32_t inOffset)
    {
        return (uint8_t)static_cast<Underlines*>(inContext)->mBox->mCopiedIMEAttributes[inOffset];
    }
    static long DrawLine(void* inContext, EeUiPoint inStart, EeUiPoint inEnd)
    {
        Underlines* u = static_cast<Underlines*>(inContext);
        u->mLine->SetStartPoint(GE2DPoint(inStart.x, inStart.y));
        u->mLine->SetColor(u->mBox->mCopiedIMETextColor);
        u->mLine->SetEndPoint(GE2DPoint(inEnd.x, inEnd.y));
        return u->mBox->GetRasterizer()->Draw(u->mLine, u->mBox->GetScreenIndex());
    }
};

// The host calls of ee_ui/src/edit_ffi.rs
extern "C" size_t ee_uicpp_edit_line_count(void* inBox) { return UIEditBoxRust::LineCount(inBox); }
extern "C" const char* ee_uicpp_edit_line(void* inBox, size_t inLine) { return UIEditBoxRust::Line(inBox, inLine); }
extern "C" void ee_uicpp_edit_set_line(void* inBox, size_t inLine, const char* inText)
{
    UIEditBoxRust::SetLine(inBox, inLine, inText);
}
extern "C" void ee_uicpp_edit_insert_line(void* inBox, size_t inLine, const char* inText)
{
    UIEditBoxRust::InsertLine(inBox, inLine, inText);
}
extern "C" void ee_uicpp_edit_erase_line(void* inBox, size_t inLine) { UIEditBoxRust::EraseLine(inBox, inLine); }
extern "C" void ee_uicpp_edit_keep_first_line(void* inBox) { UIEditBoxRust::KeepFirstLine(inBox); }
extern "C" float ee_uicpp_edit_text_width(void* inBox, const char* inText)
{
    GEText* theText = UIEditBoxRust::Box(inBox)->GetGEText();
    theText->SetText(UWideString(inText));
    return theText->GetTextExtent().GetWidth();
}
extern "C" long ee_uicpp_edit_font_height(void* inBox)
{
    return UIEditBoxRust::Box(inBox)->GetGEText()->GetFont()->GetHeight();
}
extern "C" bool ee_uicpp_edit_has_scroll_bar(void* inBox) { return UIEditBoxRust::ScrollBar(inBox) != NULL; }
extern "C" long ee_uicpp_edit_scroll_value(void* inBox)
{
    return UIEditBoxRust::ScrollBar(inBox)->GetSlider()->GetCurrentRangeValue();
}
extern "C" long ee_uicpp_edit_scroll_max(void* inBox) { return UIEditBoxRust::ScrollBar(inBox)->GetSlider()->GetMaxRange(); }
extern "C" void ee_uicpp_edit_set_scroll_value(void* inBox, unsigned long inValue)
{
    UIEditBoxRust::ScrollBar(inBox)->SetCurrentRangeValue(inValue);
}
extern "C" void ee_uicpp_edit_set_scroll_range(void* inBox, unsigned long inMin, unsigned long inMax)
{
    UIEditBoxRust::ScrollBar(inBox)->SetRange(inMin, inMax);
}
extern "C" void ee_uicpp_edit_scroll_set_location(void* inBox, float inX, float inY)
{
    UIEditBoxRust::ScrollBar(inBox)->SetLocation(GE2DPoint(inX, inY));
}
extern "C" void ee_uicpp_edit_set_dirty(void* inBox) { UIEditBoxRust::Box(inBox)->SetDirty(true); }
extern "C" void ee_uicpp_edit_set_rectangle(void* inBox, const EeUiRect* inRectangle)
{
    UIEditBoxRust::Box(inBox)->SetRectangle(*reinterpret_cast<const GERectangle*>(inRectangle));
}
extern "C" void ee_uicpp_edit_base_set_rectangle(void* inBox, const EeUiRect* inRectangle)
{
    UIEditBoxRust::Box(inBox)->UIControl::SetRectangle(*reinterpret_cast<const GERectangle*>(inRectangle));
}
extern "C" void ee_uicpp_edit_base_set_location(void* inBox, float inX, float inY)
{
    UIEditBoxRust::Box(inBox)->UIControl::SetLocation(GE2DPoint(inX, inY));
}
extern "C" void ee_uicpp_edit_fire(void* inBox, unsigned long inEvent) { UIEditBoxRust::Box(inBox)->FireTriggerEvent(inEvent); }
extern "C" void ee_uicpp_wide_assign(void* outString, const char* inText) { *static_cast<UWideString*>(outString) = inText; }

// The key dispatch's host calls (ee_ui/src/edit_keys.rs EditKey, EditPoll, PollOp)
extern "C" bool ee_uicpp_edit_key(void* inBox, int inKey) { return UIEditBoxRust::Key(inKey); }
extern "C" unsigned long ee_uicpp_edit_poll(void* inBox, int inPoll, int inOp)
{
    return UIEditBoxRust::PollOp(UIEditBoxRust::Poll(inBox, inPoll), inOp);
}
extern "C" void ee_uicpp_edit_process_enter_key(void* inBox) { UIEditBoxRust::ProcessEnterKey(inBox); }
extern "C" void ee_uicpp_edit_clipboard_set(const char* inText) { PlatClipboardSetText(inText); }
extern "C" char* ee_uicpp_edit_clipboard_get() { return PlatClipboardGetText(); }
extern "C" void ee_uicpp_edit_clipboard_free(char* inText) { free(inText); }
extern "C" void ee_uicpp_edit_translated_key(void* inBox, const char* inText)
{
    UIEditBoxRust::TranslatedKey(inBox, inText);
}

// FUNCTION: LLE 0x10069e8b
// Creates an empty edit box (one blank line) with caret, key-repeat timers and text; multi-line boxes get a scroll bar
UIEditBox::UIEditBox(unsigned long inId, GEFont* inFont, unsigned long inMaxChars, bool inNumbersOnly,
                     const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                     unsigned long inBorderThickness, bool inMultiLine, bool inPasswordBox)
    : UIControl(kControlTypeEditBox, inId, inRectangle, inForm, inIndex, inBorderThickness)
{
    this->mCaretIndex = 0;
    this->mIsTextSelected = false;
    this->mCharScroll = 1;
    this->mDrawingIndex = 0;
    this->mCaretLine = 0;
    this->mMaxChars = inMaxChars;
    this->mCursorOn = false;
    this->mKeyRepeating = false;
    this->mSelecting = false;
    this->mScrollBarCaptured = false;
    this->mIsSingleLine = !inMultiLine;
    if (!inMultiLine)
        this->mIsPasswordBox = inPasswordBox;
    else
        this->mIsPasswordBox = false;
    this->mNumbersOnly = inNumbersOnly;
    this->mSavedBgOpacity = 0.0f;
    this->mSelectionStartLine = 0;
    this->mCopiedSelectionStartLine = 0;
    this->mCopiedSelectionEndLine = 0;
    this->mCopiedSelectionEndIndex = 0;
    this->mCopiedSelectionStartIndex = 0;
    this->mSelectionStartIndex = 0;
    this->mIMEStartIndex = 0;
    this->mIMEStartLine = 0;
    this->mIMECompositionLength = 0;
    this->mCopiedCursorOn = false;
    this->mCopiedIsTextSelected = false;
    this->mScrollBarVisible = false;
    this->mIMEActive = false;
    this->mCopiedIMEStartIndex = 0;
    this->mCopiedIMEStartLine = 0;
    this->mCopiedIMEActive = false;
    this->mCopiedIMEEndIndex = 0;
    this->mCopiedIMEEndLine = 0;
    this->mSavedAlwaysDirty = false;
    this->mIMETextColor = GEColor(0.0f, 1.0f, 0.0f);
    this->mCopiedIMETextColor = this->mIMETextColor;
    this->mCaretX = 0.0f;
    this->mBorderType = 0;
    this->mCursor = new GELine(GE2DPoint(0.0f, 0.0f), GE2DPoint(0.0f, 0.0f), GEColor(0.0f, 0.0f, 0.0f),
                         GEColor(0.0f, 0.0f, 0.0f), 1.0f);
    this->mLines.resize(1);
    this->mLines[this->mCaretLine] = "";
    this->mWorkString = "";
    this->mRepeatPoll = new TMPoll(50, 0, false);
    this->mRepeatDelayPoll = new TMPoll(500, 0, false);
    this->mCursorPoll = new TMPoll(500, 0, false);
    this->mScrollPoll = new TMPoll(25, 0, false);
    this->mText = new GEText(inFont);
    this->mCopiedText = new GEText(inFont);
    this->mText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
    this->mText->SetBgColor(GEColor(0.0f, 0.0f, 0.0f));
    this->mText->SetIsTransparent(true);
    this->mTextRectangle.SetTop(this->GetRectangle().GetTop() + inBorderThickness);
    this->mTextRectangle.SetLeft(this->GetRectangle().GetLeft() + inBorderThickness);
    this->mTextRectangle.SetRight(this->GetRectangle().GetRight() - inBorderThickness);
    this->mTextRectangle.SetBottom(this->GetRectangle().GetBottom() - inBorderThickness);
    if (this->mTextRectangle.GetTop() > this->mTextRectangle.GetBottom()) {
        this->mTextRectangle.SetTop(this->GetRectangle().GetHeight() * 0.5f + this->GetRectangle().GetTop());
        this->mTextRectangle.SetBottom(this->mTextRectangle.GetTop() + 1.0f);
    }
    if (this->mTextRectangle.GetLeft() > this->mTextRectangle.GetRight()) {
        this->mTextRectangle.SetLeft(this->GetRectangle().GetWidth() * 0.5f + this->GetRectangle().GetLeft());
        this->mTextRectangle.SetRight(this->mTextRectangle.GetLeft() + 1.0f);
    }
    this->mVisibleLineCount = UMFloor(this->mTextRectangle.GetHeight() / inFont->GetHeight());
    if (!this->mIsSingleLine) {
        this->mScrollBarVisible = true;
        this->mTextRectangle.SetRight(this->GetRectangle().GetRight() - kScrollBarWidth);
        this->mScrollBar = new UIScrollBar(this->GetId(), this->mTextRectangle.GetTopRight(),
                                     this->mTextRectangle.GetHeight(), 1, this->GetForm(), this->GetIndex(),
                                     this->GetBorderThickness());
        this->mScrollBar->SetRange(0, 0);
        this->mScrollBar->SetPageSize(this->mVisibleLineCount > 0 ? this->mVisibleLineCount - 1 : 0);
        this->mDisplayLines.resize(this->mVisibleLineCount);
    } else {
        this->mDisplayLines.resize(1);
        this->mScrollBar = NULL;
    }
}

// FUNCTION: LLE 0x1006a44d
// Destroys the edit box, deleting its timers, caret line, texts and scroll bar
UIEditBox::~UIEditBox()
{
    delete this->mRepeatPoll;
    delete this->mRepeatDelayPoll;
    delete this->mCursorPoll;
    delete this->mCursor;
    delete this->mText;
    delete this->mCopiedText;
    delete this->mScrollBar;
    delete this->mScrollPoll;
}

// FUNCTION: LLE 0x1006a53f
// Inserts text at the caret: single-line boxes clip to the character limit (numbers-only drops non-digits but still
// counts them); multi-line boxes re-wrap the following text at word breaks, adding lines as needed
const char* UIEditBox::AddChar(const char* inString)
{
    return inString + ee_ui_edit_add_char(UIEditBoxRust::Refs(this), inString);
}

// SYNTHETIC: LLE 0x1006a9a5
// GetMultiByteChar (lle/UIControl.h): its COMDAT follows AddChar, its first user

// FUNCTION: LLE 0x1006a9cb
// Types an IME result string into the box character by character, then restarts the composition at the caret
void UIEditBox::AddIMEResultString(const UWideString& inString)
{
    ee_ui_edit_add_ime_result(UIEditBoxRust::Refs(this), inString);
}

// FUNCTION: LLE 0x1006aa2f
// Deletes the character before the caret (joining onto the previous line at a line start); nothing at the very start
void UIEditBox::BackSpaceChar()
{
    ee_ui_edit_back_space_char(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006aa7b
// Converts a mouse location to the caret line and character index it points at, clamped to the visible text
void UIEditBox::CalculateCaretFromMouseLocation(unsigned long& outLine, long& outIndex,
                                                const GE2DPoint& inLocation) const
{
    ee_ui_edit_caret_from_mouse(UIEditBoxRust::Refs(this), UIEditBoxRust::Point(inLocation), &outLine, &outIndex);
}

// FUNCTION: LLE 0x1006ad00
// Replaces the active IME composition text with a new string and attributes and moves the caret inside it
long UIEditBox::ChangeIMECompositionString(const UWideString& inString, const UString& inAttributes,
                                           unsigned long inCursor)
{
    bool theActive = this->mIMEActive == true;
    long theResult = ee_ui_edit_ime_change(UIEditBoxRust::Refs(this), inString, inCursor);
    if (theActive)
        this->mIMEAttributes = inAttributes;
    return theResult;
}

// FUNCTION: LLE 0x1006ad6f
// Changes the font of the edit box's live and draw-copy texts
void UIEditBox::ChangeFont(GEFont* inFont)
{
    if (this->mText) {
        this->mText->SetFont(inFont);
        this->mCopiedText->SetFont(inFont);
    }
}

// FUNCTION: LLE 0x1006ad98
// Resizes the edit box to a new width and recomputes the text area (leaving room for a visible scroll bar)
void UIEditBox::ChangeWidth(unsigned long inWidth)
{
    ee_ui_edit_change_width(UIEditBoxRust::Refs(this), inWidth);
}

// FUNCTION: LLE 0x1006ae42
// Shows or hides the scroll bar and updates its range as the line count crosses the visible line count
void UIEditBox::CheckScrollBar()
{
    ee_ui_edit_check_scroll_bar(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006afca
// Ends an active IME composition, removing its uncommitted text
long UIEditBox::CloseIMECompositionString()
{
    return ee_ui_edit_ime_close(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006afe6
// Recomputes the caret's screen position (optionally its remembered x) and scrolls so the caret line is visible
void UIEditBox::ComputeCaretLocation(bool inUpdateCaretX)
{
    ee_ui_edit_compute_caret_location(UIEditBoxRust::Refs(this), inUpdateCaretX);
}

// FUNCTION: LLE 0x1006b05c
// Picks the first visible character of a single-line box so the text before the caret fits the text area
void UIEditBox::ComputeDrawingIndex()
{
    ee_ui_edit_compute_drawing_index(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006b122
// Copies the selected text to the clipboard (platform/PlatWindow.h; Win32's CF_TEXT before)
void UIEditBox::Copy()
{
    ee_ui_edit_copy(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006b1a0
// Prepares the frame's draw copy: caret blink and position, visible lines (stars for a password box), IME and
// selection ranges clipped to the visible lines
long UIEditBox::CopyData()
{
    GE2DPoint theLocation;
    UWideString thePassword;
    this->CheckScrollBar();
    long theResult = UIControl::CopyData();
    if (this->mCursorPoll->GetIsRunning() == true && this->mCursorPoll->Poll() > 0)
        this->mCursorOn = !this->mCursorOn;
    if (this->mCursorOn) {
        if (this->mScrollBar != NULL &&
            (this->mCaretLine < (unsigned long)this->mScrollBar->GetSlider()->GetCurrentRangeValue() ||
             this->mCaretLine >= this->mScrollBar->GetSlider()->GetCurrentRangeValue() + this->mVisibleLineCount))
            this->mCopiedCursorOn = false;
        else
            this->mCopiedCursorOn = true;
        if (this->mCopiedCursorOn) {
            this->GetLocationFromIndex(this->mCaretIndex, this->mCaretLine, theLocation);
            this->mCursor->SetStartPoint(theLocation);
            this->mCursor->SetEndPoint(GE2DPoint(theLocation.GetX(), theLocation.GetY() + this->GetGEText()->GetFont()->GetHeight()));
            this->mCursor->SetColor(this->mCursorColor);
        }
    } else
        this->mCopiedCursorOn = false;
    unsigned long theFirstLine;
    unsigned long theLastLine;
    if (this->mIsSingleLine) {
        const char* theString;
        if (!this->mIsPasswordBox)
            theString = (const char*)PlatMbsNInc(reinterpret_cast<const unsigned char*>((const char*)this->mLines[this->mCaretLine]), this->mDrawingIndex);
        else {
            thePassword.Clear();
            for (unsigned long i = 0; i < this->mLines[this->mCaretLine].GetLength(); i++)
                thePassword += UWideString("*");
            theString = (const char*)PlatMbsNInc(reinterpret_cast<const unsigned char*>((const char*)thePassword), this->mDrawingIndex);
        }
        theLocation = GE2DPoint(this->mTextRectangle.GetLeft(), this->mTextRectangle.GetTop());
        this->GetGEText()->SetLocation(theLocation);
        this->mText->SetText(UWideString(theString));
        this->mDisplayLines[0] = theString;
        if (this->mIMEActive == true) {
            // from the drawing index on (ee_ui edit_draw.rs)
            UIEditBoxRust::SetCopiedIME(this, ee_ui_edit_copied_ime_single(this->mIMEStartIndex, this->mIMECompositionLength,
                                                                          this->mDrawingIndex));
            this->mCopiedIMETextColor = this->mIMETextColor;
            this->mCopiedIMEAttributes = this->mIMEAttributes;
        } else
            this->mCopiedIMEActive = false;
        theFirstLine = 0;
        theLastLine = 1;
    } else {
        theFirstLine = this->mScrollBar->GetSlider()->GetCurrentRangeValue();
        theLastLine = theFirstLine + this->mVisibleLineCount;
        for (unsigned long i = theFirstLine; i < theLastLine; i++) {
            if (i < this->mLines.size()) {
                if (PlatMbsChr(reinterpret_cast<const unsigned char*>((const char*)this->mLines[i]), '\n'))
                    this->mDisplayLines[i - theFirstLine].CopyString(this->mLines[i], this->mLines[i].GetLength() - 1);
                else
                    this->mDisplayLines[i - theFirstLine] = this->mLines[i];
            } else
                this->mDisplayLines[i - theFirstLine] = "";
        }
        theResult = this->mScrollBar->CopyData();
        if (this->mIMEActive == true) {
            this->mCopiedIMETextColor = this->mIMETextColor;
            this->mCopiedIMEAttributes = this->mIMEAttributes;
            // clipped to the copied lines, its end walked over the lines after its start (ee_ui edit_draw.rs)
            long theLength = PlatMbsLen(PlatMbsNInc(reinterpret_cast<const unsigned char*>((const char*)this->mLines[this->mIMEStartLine]),
                                              this->mIMEStartIndex));
            std::vector<unsigned long> theLengths(this->mLines.size());
            for (size_t i = 0; i < this->mLines.size(); i++)
                theLengths[i] = this->mLines[i].GetLength();
            UIEditBoxRust::SetCopiedIME(this, ee_ui_edit_copied_ime_lines(this->mIMEStartLine, this->mIMEStartIndex,
                                                                         this->mIMECompositionLength, theFirstLine, theLastLine,
                                                                         theLength, theLengths.data(), theLengths.size()));
        } else
            this->mCopiedIMEActive = false;
    }
    this->mCopiedTextRectangle = this->mTextRectangle;
    *this->mCopiedText = *this->GetGEText();
    if (this->mIsTextSelected) {
        this->mSelectionRhombus.SetColor(this->mSelectedBgColor);
        // the caret and the anchor ordered, clipped to the copied lines (ee_ui edit_draw.rs)
        std::vector<unsigned long> theLengths(this->mDisplayLines.size());
        for (size_t i = 0; i < this->mDisplayLines.size(); i++)
            theLengths[i] = this->mDisplayLines[i].GetLength();
        EeUiEditSelection theSelection = ee_ui_edit_copied_selection(this->mCaretLine, this->mCaretIndex, this->mSelectionStartLine,
                                                                     this->mSelectionStartIndex, this->mDrawingIndex, theFirstLine,
                                                                     theLastLine, theLengths.data(), theLengths.size());
        this->mCopiedIsTextSelected = theSelection.selected;
        this->mCopiedSelectionStartLine = theSelection.start_line;
        this->mCopiedSelectionEndLine = theSelection.end_line;
        this->mCopiedSelectionStartIndex = theSelection.start_index;
        this->mCopiedSelectionEndIndex = theSelection.end_index;
    } else
        this->mCopiedIsTextSelected = false;
    return theResult;
}

// FUNCTION: LLE 0x1006b7fc
// Copies the selected text to the clipboard, then deletes it
void UIEditBox::Cut()
{
    ee_ui_edit_cut(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006b816
// Moves the caret one character left, wrapping to the end of the previous line and scrolling left when needed
void UIEditBox::DecrementCaretPos()
{
    ee_ui_edit_decrement_caret_pos(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006b884
// Deletes the character after the caret
void UIEditBox::DeleteChar()
{
    ee_ui_edit_delete_char(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006b8d7
// Removes one line, keeping the caret on a valid line and scrolling up by one when the box is scrolled down
void UIEditBox::DeleteLine(unsigned long inLine)
{
    ee_ui_edit_delete_line(UIEditBoxRust::Refs(this), inLine);
}

// FUNCTION: LLE 0x1006b9c1
// Deletes the selected text and clears the selection (not while an IME composition is open)
void UIEditBox::DeleteSelectedText()
{
    ee_ui_edit_delete_selected_text(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006ba3f
// Deletes the text between two caret positions, re-wraps the following lines and puts the caret at the start of the gap
void UIEditBox::DeleteText(unsigned long inStartIndex, unsigned long inStartLine, unsigned long inEndIndex,
                           unsigned long inEndLine)
{
    ee_ui_edit_delete_range(UIEditBoxRust::Refs(this), inStartIndex, inStartLine, inEndIndex, inEndLine);
}

// FUNCTION: LLE 0x1006bdab
// Clears the edit box to one empty line, with the caret at the start and no selection
void UIEditBox::DeleteText()
{
    ee_ui_edit_clear(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006be4b
// Draws the control frame, scroll bar, selection highlight, IME underlines and blinking caret from the draw copy
long UIEditBox::DrawPolygons()
{
    GELine theLine;
    GERectangle theRectangle;
    long theResult = UIControl::DrawPolygons();
    if (this->mScrollBarVisible)
        theResult = this->mScrollBar->DrawPolygons();
    if (this->mCopiedIsTextSelected) {
        // per copied line, its highlighted characters (ee_ui edit_draw.rs)
        EeUiEditSelection theSelection = {this->mCopiedSelectionStartLine, this->mCopiedSelectionEndLine,
                                          this->mCopiedSelectionStartIndex, this->mCopiedSelectionEndIndex, true};
        for (unsigned long i = this->mCopiedSelectionStartLine; i <= this->mCopiedSelectionEndLine; i++) {
            unsigned long theLength = i < this->mDisplayLines.size() ? this->mDisplayLines[i].GetLength() : 0;
            EeUiEditSpan theSpan = ee_ui_edit_selection_span(&theSelection, i, theLength);
            this->GetSelectedTextRectangle(i, theSpan.from, theSpan.to, theRectangle);
            this->mSelectionRhombus.SetRectangle(theRectangle);
            theResult = this->GetRasterizer()->Draw(&this->mSelectionRhombus, this->GetScreenIndex());
        }
    }
    if (this->mCopiedIMEActive == true) {
        // the target clauses underlined (ee_ui edit_draw.rs)
        UIEditBoxRust::Underlines theUnderlines = {this, UWideString(), &theLine};
        EeUiImeHost theHost = {&theUnderlines,         UIEditBoxRust::DisplayLength, UIEditBoxRust::PrefixWidth,
                               UIEditBoxRust::CharBytes, UIEditBoxRust::CharWidth,     UIEditBoxRust::Attribute,
                               UIEditBoxRust::DrawLine};
        EeUiEditIme theIME = {this->mCopiedIMEStartLine, this->mCopiedIMEStartIndex, this->mCopiedIMEEndLine,
                              this->mCopiedIMEEndIndex, true};
        GERectangle& r = this->mCopiedTextRectangle;
        EeUiRect theText = {r.mBottom, r.mLeft, r.mRight, r.mTop};
        theResult = ee_ui_edit_ime_underlines(&theIME, theText, this->mCopiedText->GetFont()->GetHeight(), theResult, &theHost);
    }
    if (this->mCopiedCursorOn)
        theResult = this->GetRasterizer()->Draw(this->mCursor, this->GetScreenIndex());
    return theResult;
}

// FUNCTION: LLE 0x1006c1fd
// Draws the visible lines, then redraws the IME composition text in its own colour, then the scroll bar
long UIEditBox::DrawTextA()
{
    UWideString theString;
    unsigned long i = 0;
    this->mDrawCount--;
    long theResult = kErrorOK;
    for (std::vector<UWideString>::iterator theLine = this->mDisplayLines.begin(); theLine < this->mDisplayLines.end();
         ++theLine, i++) {
        this->mCopiedText->SetText(*theLine);
        float theY = ee_ui_edit_text_row_y(i, this->mCopiedText->GetFont()->GetHeight(), this->mCopiedTextRectangle.GetTop());
        this->mCopiedText->SetLocation(this->mCopiedTextRectangle.GetLeft(), theY);
        theResult = this->GetForm()->DrawClippedText(this->mCopiedTextRectangle, this->mCopiedText, this);
        if (this->mCopiedIMEActive != true || !(i >= this->mCopiedIMEStartLine) || !(i <= this->mCopiedIMEEndLine))
            continue;
        if (this->mCopiedIMEStartLine == i) {
            theString.CopyString(this->mDisplayLines[i], this->mCopiedIMEStartIndex);
            this->mCopiedText->SetText(theString);
            this->mCopiedText->SetLocation(this->mCopiedText->GetTextExtent().GetRight() + this->mCopiedTextRectangle.GetLeft(),
                                     theY);
        }
        theString = this->mDisplayLines[i];
        if (this->mCopiedIMEEndLine == i) {
            char* theEnd = (char*)PlatMbsNInc(reinterpret_cast<const unsigned char*>((const char*)theString), this->mCopiedIMEEndIndex);
            strcpy(theEnd, "");
        }
        const char* theText;
        if (this->mCopiedIMEStartLine == i)
            theText = (const char*)PlatMbsNInc(reinterpret_cast<const unsigned char*>((const char*)theString), this->mCopiedIMEStartIndex);
        else
            theText = theString;
        GEColor theColor;
        theColor = this->mCopiedText->GetFgColor();
        this->mCopiedText->SetFgColor(this->mCopiedIMETextColor);
        this->mCopiedText->SetText(UWideString(theText));
        theResult = this->GetForm()->DrawClippedText(this->mCopiedTextRectangle, this->mCopiedText, this);
        this->mCopiedText->SetFgColor(theColor);
    }
    if (this->mScrollBarVisible)
        theResult = this->mScrollBar->DrawTextA();
    return theResult;
}

// FUNCTION: LLE 0x1006c43d
// Returns the screen position of a character index on a line (x from the caret line's text, y from the line)
void UIEditBox::GetLocationFromIndex(unsigned long inIndex, unsigned long inLine, GE2DPoint& outLocation) const
{
    EeUiPoint theLocation;
    ee_ui_edit_location_from_index(UIEditBoxRust::Refs(this), inIndex, inLine, &theLocation);
    outLocation = GE2DPoint(theLocation.x, theLocation.y);
}

// FUNCTION: LLE 0x1006c5d9
// Returns the selected text, ordering the caret and selection anchor into start and end
void UIEditBox::GetSelectedText(UWideString& outString) const
{
    ee_ui_edit_selected_text(UIEditBoxRust::Refs(this), &outString);
}

// FUNCTION: LLE 0x1006c64a
// Returns the screen rectangle covering a character range on a visible line, clipped to the text area
void UIEditBox::GetSelectedTextRectangle(unsigned long inLine, unsigned long inStartIndex, unsigned long inEndIndex,
                                         GERectangle& outRectangle) const
{
    UWideString theString;
    long theHeight = this->mCopiedText->GetFont()->GetHeight();
    theString.CopyString(this->mDisplayLines[inLine], inStartIndex);
    this->mCopiedText->SetText(theString);
    float theBefore = this->mCopiedText->GetTextExtent().GetWidth();
    const char* theText = (const char*)PlatMbsNInc(reinterpret_cast<const unsigned char*>((const char*)this->mDisplayLines[inLine]),
                                                inStartIndex);
    theString.CopyString(theText, inEndIndex - inStartIndex);
    this->mCopiedText->SetText(theString);
    // the row, from the text before to the end of the span, clipped to the text rectangle (ee_ui edit_draw.rs)
    const GERectangle& r = this->mTextRectangle;
    EeUiRect theTextRectangle = {r.mBottom, r.mLeft, r.mRight, r.mTop};
    EeUiRect theRow = ee_ui_edit_selection_rect(inLine, theHeight, theTextRectangle, theBefore,
                                                this->mCopiedText->GetTextExtent().GetWidth());
    outRectangle = GERectangle(theRow.left, theRow.top, theRow.right, theRow.bottom);
}

// FUNCTION: LLE 0x1006c768
// Returns the text between two line/index positions, joining the full lines in between
void UIEditBox::GetText(unsigned long inStartLine, unsigned long inEndLine, unsigned long inStartIndex,
                        unsigned long inEndIndex, UWideString& outString) const
{
    ee_ui_edit_text(UIEditBoxRust::Refs(this), inStartLine, inEndLine, inStartIndex, inEndIndex, &outString);
}

// FUNCTION: LLE 0x1006c89a
// Moves the caret a number of characters right, wrapping across lines and scrolling right when it passes the edge
void UIEditBox::IncrementCaretPos(long inCount)
{
    ee_ui_edit_increment_caret_pos(UIEditBoxRust::Refs(this), inCount);
}

// FUNCTION: LLE 0x1006c9f3
// Inserts the clipboard's text at the caret (platform/PlatWindow.h; Win32's CF_TEXT before)
void UIEditBox::Paste()
{
    ee_ui_edit_paste(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006ca57
// Handles a left click: forwards it to the scroll bar, or places the caret, starts a selection and captures input
long UIEditBox::MouseLButtonDown()
{
    long theResult = kErrorOK;
    if (this->mScrollBar != NULL && this->mScrollBar->GetRectangle().Contains(this->GetMouseLocation())) {
        theResult = this->mScrollBar->MouseLButtonDown();
        this->mScrollBarCaptured = true;
        this->CaptureMouse();
    } else if (!this->mIMEActive) {
        if (this->GetTabable() && this->mForm->GetTabStop() != this->mIndex)
            this->mForm->SetTabStop(this->mIndex);
        this->CalculateCaretFromMouseLocation(this->mCaretLine, this->mCaretIndex, this->GetMouseLocation());
        this->ComputeCaretLocation(true);
        this->mSelectionStartIndex = this->mCaretIndex;
        this->mSelecting = true;
        this->mSelectionStartLine = this->mCaretLine;
        this->SetupKeyboardForCapture();
        this->ButtonDown();
        this->CaptureMouse();
    }
    this->mCursorPoll->Stop();
    return theResult;
}

// FUNCTION: LLE 0x1006cb28
// Selects all text on a double click inside the text area; otherwise handles it as a normal control
long UIEditBox::MouseLButtonDoublePressed()
{
    long theResult = kErrorOK;
    if (this->mTextRectangle.Contains(this->mForm->GetMouseLocation()))
        this->SelectText();
    else
        theResult = UIControl::MouseLButtonDoublePressed();
    return theResult;
}

// FUNCTION: LLE 0x1006cb64
// Ends a scroll-bar drag or a selection drag and releases the mouse
long UIEditBox::MouseLButtonUp()
{
    long theResult = kErrorOK;
    if (this->mScrollBarCaptured) {
        theResult = this->mScrollBar->MouseLButtonUp();
        this->mScrollBarCaptured = false;
    } else
        this->mSelecting = false;
    this->mCursorPoll->Start();
    this->ReleaseCapturedMouse();
    return theResult;
}

// FUNCTION: LLE 0x1006cbaa
// Forwards a drag to the scroll bar, or extends the selection to the mouse, auto-scrolling while it is outside the text
long UIEditBox::MouseMove(const GE2DPoint& inLocation)
{
    long theResult = kErrorOK;
    if (this->mScrollBarCaptured)
        theResult = this->mScrollBar->MouseMove(inLocation);
    else if (this->mSelecting)
        ee_ui_edit_drag(UIEditBoxRust::Refs(this), UIEditBoxRust::Point(inLocation));
    return theResult;
}

// FUNCTION: LLE 0x1006cd80
// Starts an IME composition at the caret, deleting any selection; fails for a numbers-only box
long UIEditBox::OpenIMECompositionString()
{
    return ee_ui_edit_ime_open(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006cdc6
// Moves the caret down one line, keeping its x position
void UIEditBox::ProcessDownKey()
{
    ee_ui_edit_down_key(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006ce0b
// Moves the caret to the end of the line (before its line break)
void UIEditBox::ProcessEndKey()
{
    ee_ui_edit_end_key(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006ce6f
// Breaks the line at the caret in a multi-line box (re-wrapping the rest); a single-line box fires its Enter trigger
void UIEditBox::ProcessEnterKey()
{
    ee_ui_edit_enter_key(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006cfed
// Moves the caret to the start of the line and scrolls back to the first character
void UIEditBox::ProcessHomeKey()
{
    ee_ui_edit_home_key(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006d6c6
// Moves the caret up one line, keeping its x position
void UIEditBox::ProcessUpKey()
{
    ee_ui_edit_up_key(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006d6ee
// Scrolls a single-line box left so the caret stays visible
void UIEditBox::ScrollLeft()
{
    ee_ui_edit_scroll_left(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006d75d
// Scrolls right to show the given number of extra characters (moves the caret to line 0 while doing so)
void UIEditBox::ScrollRight(long inCount)
{
    ee_ui_edit_scroll_right(UIEditBoxRust::Refs(this), inCount);
}

// FUNCTION: LLE 0x1006d7be
// Releases the keyboard, cancelling any open IME composition and restoring the saved always-dirty flag
void UIEditBox::ReleaseCapturedKeyboard()
{
    GEWindow* theWindow = this->GetRasterizer()->GetWindow(this->GetScreenIndex());
    if (this->mIMEActive == true) {
        this->RemoveIMECompositionString();
        PlatTextInputCancel(theWindow->GetOSRepresentation());   // ImmNotifyIME(NI_COMPOSITIONSTR, CPS_CANCEL)
        this->mIMEActive = false;
    }
    this->SetAlwaysDirty(this->mSavedAlwaysDirty);
    UIControl::ReleaseCapturedKeyboard();
}

// FUNCTION: LLE 0x1006d82e
// Deletes the uncommitted IME composition text, leaving the caret where it started
void UIEditBox::RemoveIMECompositionString()
{
    ee_ui_edit_ime_remove(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006d860
// Selects all the text (not while an IME composition is open)
void UIEditBox::SelectText()
{
    ee_ui_edit_select_all(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006d8dc
// Sets the caret and the selection anchor, selecting the text between them (not while an IME composition is open)
void UIEditBox::SelectText(unsigned long inCaretIndex, unsigned long inStartIndex, unsigned long inCaretLine,
                           unsigned long inStartLine)
{
    ee_ui_edit_select(UIEditBoxRust::Refs(this), inCaretIndex, inStartIndex, inCaretLine, inStartLine);
}

// FUNCTION: LLE 0x1006d95d
// Moves the caret to a character index on a line, scrolling to show it
void UIEditBox::SetCaretLocation(unsigned long inIndex, unsigned long inLine)
{
    ee_ui_edit_set_caret_location(UIEditBoxRust::Refs(this), inIndex, inLine);
}

// FUNCTION: LLE 0x1006d98f
// Places the caret on its new line at the character nearest its remembered x position (after an up/down move)
void UIEditBox::SetIndexFromUpDownOffset()
{
    ee_ui_edit_index_from_up_down_offset(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006da92
// Moves the edit box, keeping the text area's size and moving the scroll bar with it
void UIEditBox::SetLocation(const GE2DPoint& inLocation)
{
    ee_ui_edit_set_location(UIEditBoxRust::Refs(this), UIEditBoxRust::Point(inLocation));
}

// FUNCTION: LLE 0x1006db7b
// Replaces the whole text with a string and puts the caret at the start
void UIEditBox::SetText(const UWideString& inString)
{
    ee_ui_edit_set_text(UIEditBoxRust::Refs(this), inString);
}

// FUNCTION: LLE 0x1006dbaf
// Captures the keyboard with the caret shown and the box redrawn every frame
void UIEditBox::SetupKeyboardForCapture()
{
    this->mCursorOn = true;
    this->CaptureKeyboard();
    this->mSavedAlwaysDirty = this->GetAlwaysDirty();
    this->SetAlwaysDirty(true);
}

// FUNCTION: LLE 0x1006dbd5
// Takes focus away: stops the caret, restores the background, clears the selection and cancels any IME composition
void UIEditBox::TabbedFrom()
{
    GEWindow* theWindow = this->GetRasterizer()->GetWindow(this->GetScreenIndex());
    this->mCursorPoll->Stop();
    this->mCursorOn = false;
    this->SetBgOpacity(this->mSavedBgOpacity);
    this->mIsTextSelected = false;
    this->mSelecting = false;
    this->mScrollBarCaptured = false;
    this->ReleaseCapturedKeyboard();
    this->ReleaseCapturedMouse();
    PlatTextInputCancel(theWindow->GetOSRepresentation());   // ImmNotifyIME(NI_COMPOSITIONSTR, CPS_CANCEL)
    this->SetDirty(true);
}

// FUNCTION: LLE 0x1006dc67
// Gives the box focus: starts the caret blink, makes the background opaque, captures the keyboard, cancels IME input
void UIEditBox::TabbedTo()
{
    GEWindow* theWindow = this->GetRasterizer()->GetWindow(this->GetScreenIndex());
    this->mCursorPoll->Start();
    this->mSavedBgOpacity = this->GetBgOpacity();
    this->SetBgOpacity(1.0f);
    this->SetupKeyboardForCapture();
    PlatTextInputCancel(theWindow->GetOSRepresentation());   // ImmNotifyIME(NI_COMPOSITIONSTR, CPS_CANCEL)
    this->SetDirty(true);
}

// FUNCTION: LLE 0x1006dcfb
// Processes held keys with a 500 ms repeat delay and 50 ms repeat; Enter in a single-line box fires its trigger
long UIEditBox::UpdateKeyboard()
{
    return ee_ui_edit_update_keyboard(UIEditBoxRust::Refs(this));
}

// FUNCTION: LLE 0x1006ddff
// Types a translated key the given number of times at the caret (or into the IME composition) and shows the caret
long UIEditBox::UpdateTranslatedKey(const char* inKey, short inRepeat)
{
    ee_ui_edit_type_key(UIEditBoxRust::Refs(this), inKey, inRepeat);
    this->mCursorPoll->Stop();
    this->mCursorPoll->Start();
    this->mCursorOn = true;
    return kErrorOK;
}

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x10069d6b
// SYNTHETIC: LLE 0x10069d75
// SYNTHETIC: LLE 0x10069d85
// SYNTHETIC: LLE 0x10069d91
// SYNTHETIC: LLE 0x10069d9b
// SYNTHETIC: LLE 0x10069da5
// SYNTHETIC: LLE 0x10069db5
// SYNTHETIC: LLE 0x10069dc1
// SYNTHETIC: LLE 0x10069dcb
// SYNTHETIC: LLE 0x10069dd5
// SYNTHETIC: LLE 0x10069de5
// SYNTHETIC: LLE 0x10069df1
// SYNTHETIC: LLE 0x10069dfb
// SYNTHETIC: LLE 0x10069e05
// SYNTHETIC: LLE 0x10069e15
// SYNTHETIC: LLE 0x10069e21
// SYNTHETIC: LLE 0x10069e2b
// SYNTHETIC: LLE 0x10069e35
// SYNTHETIC: LLE 0x10069e45
// SYNTHETIC: LLE 0x10069e51
// SYNTHETIC: LLE 0x10069e5b
// SYNTHETIC: LLE 0x10069e65
// SYNTHETIC: LLE 0x10069e75
// SYNTHETIC: LLE 0x10069e81
