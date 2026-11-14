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

#include "rust/ee_ui.h"
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
#include "UIObjHost.h"

struct UIEditBoxRust {
    // EditKey (ee_ui edit_keys.rs): UIControl's key predicates and the keyboard's A, L and Return entities
    static bool Key(intptr_t inKey)
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
    static intptr_t Op(void* inBox, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t, intptr_t, float* f)
    {
        static char* sClipboard = NULL;
        UIEditBox* e = static_cast<UIEditBox*>(inBox);
        UIScrollBar* theBar = reinterpret_cast<UIScrollBar*>(a);
        TMPoll* thePoll = reinterpret_cast<TMPoll*>(a);
        GELine* theLine = reinterpret_cast<GELine*>(a);
        GERhombus* theRhombus = reinterpret_cast<GERhombus*>(a);
        GEText* theText = reinterpret_cast<GEText*>(a);
        intptr_t r = 0;
        if (UIObjHost::Base(e, inOp, a, f, &r) || UIObjHost::Media(e, inOp, a, b, f, &r) ||
            UIObjHost::Part(reinterpret_cast<UIControl*>(a), inOp, f, &r))
            return r;
        switch (inOp) {
        case EeUiOp::ButtonDown: e->ButtonDown(); break;
        case EeUiOp::RhombusSetRect: theRhombus->SetRectangle(UIObjHost::Rect(f)); break;
        case EeUiOp::RhombusSetColor: theRhombus->SetColor(GEColor(f[0], f[1], f[2])); break;
        case EeUiOp::RhombusDraw: return e->GetRasterizer()->Draw(theRhombus, e->GetScreenIndex());
        case EeUiOp::PollCreate: return (intptr_t) new TMPoll((unsigned long)a, 0, false);
        case EeUiOp::PollDelete: delete thePoll; break;
        case EeUiOp::PollRunning: return thePoll->GetIsRunning();
        case EeUiOp::PollPoll: return thePoll->Poll();
        case EeUiOp::PollStart: thePoll->Start(); break;
        case EeUiOp::PollStop: thePoll->Stop(); break;
        case EeUiOp::TextSetBytes: theText->SetText(UWideString(reinterpret_cast<const char*>(b))); break;
        case EeUiOp::TextFontHeight: return theText->GetFont()->GetHeight();
        case EeUiOp::DrawCountDecrement: e->mDrawCount--; break;
        case EeUiOp::CreateScrollBar:
            return (intptr_t) new UIScrollBar(e->mId, GE2DPoint(f[0], f[1]), f[2], 1, e->mForm, e->mIndex,
                                              e->GetBorderThickness());
        case EeUiOp::BarSetRange: theBar->SetRange(0, (unsigned long)b); break;
        case EeUiOp::BarSetPageSize: theBar->SetPageSize((unsigned long)b); break;
        case EeUiOp::BarValue: return theBar->GetSlider()->GetCurrentRangeValue();
        case EeUiOp::BarSetValue: theBar->SetCurrentRangeValue((unsigned long)b); break;
        case EeUiOp::BarMax: return theBar->GetSlider()->GetMaxRange();
        case EeUiOp::VirtSetRectangle: e->SetRectangle(UIObjHost::Rect(f)); break;
        case EeUiOp::EditKey: return Key(a);
        case EeUiOp::VirtProcessEnterKey: e->ProcessEnterKey(); break;
        case EeUiOp::ClipboardSet: PlatClipboardSetText(reinterpret_cast<const char*>(a)); break;
        case EeUiOp::ClipboardGet:
            free(sClipboard);
            sClipboard = PlatClipboardGetText();
            return (intptr_t)sClipboard;
        case EeUiOp::TranslatedKey: e->mForm->GetManager()->UpdateTranslatedKey(reinterpret_cast<const char*>(a), 1); break;
        case EeUiOp::TextInputCancel:
            // ImmNotifyIME(NI_COMPOSITIONSTR, CPS_CANCEL)
            PlatTextInputCancel(e->GetRasterizer()->GetWindow(e->GetScreenIndex())->GetOSRepresentation());
            break;
        case EeUiOp::BaseReleaseKeyboard: e->UIControl::ReleaseCapturedKeyboard(); break;
        case EeUiOp::BaseMouseDoublePressed: return e->UIControl::MouseLButtonDoublePressed();
        case EeUiOp::FormMouseLocation:
            f[0] = e->mForm->GetMouseLocation().GetX();
            f[1] = e->mForm->GetMouseLocation().GetY();
            break;
        case EeUiOp::LineCreate:
            if (a == 1)
                return (intptr_t) new GELine(GE2DPoint(0.0f, 0.0f), GE2DPoint(0.0f, 0.0f), GEColor(0.0f, 0.0f, 0.0f),
                                             GEColor(0.0f, 0.0f, 0.0f), 1.0f);
            return (intptr_t) new GELine();
        case EeUiOp::LineDelete: delete theLine; break;
        case EeUiOp::LineSetPoints:
            theLine->SetStartPoint(GE2DPoint(f[0], f[1]));
            theLine->SetEndPoint(GE2DPoint(f[2], f[3]));
            break;
        case EeUiOp::LineSetColor: theLine->SetColor(GEColor(f[0], f[1], f[2])); break;
        case EeUiOp::LineDraw: return e->GetRasterizer()->Draw(theLine, e->GetScreenIndex());
        case EeUiOp::RhombusCreate: return (intptr_t) new GERhombus();
        case EeUiOp::RhombusDelete: delete theRhombus; break;
        case EeUiOp::TextFgColor:
            f[0] = theText->GetFgColor().GetRed();
            f[1] = theText->GetFgColor().GetGreen();
            f[2] = theText->GetFgColor().GetBlue();
            break;
        case EeUiOp::TextSetBgColor: theText->SetBgColor(GEColor(f[0], f[1], f[2])); break;
        case EeUiOp::TextSetTransparent: theText->SetIsTransparent(b != 0); break;
        default: break;
        }
        return 0;
    }
    static intptr_t Call(const UIEditBox* e, EeUiEditCall c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0,
                         intptr_t d = 0, float f0 = 0.0f, float f1 = 0.0f, float f2 = 0.0f)
    {
        float f[4] = {f0, f1, f2, 0.0f};
        return ee_ui_edit_call(e->mRust, c, a, b, cc, d, f);
    }
    static intptr_t Color(const UIEditBox* e, EeUiEditCall c, const GEColor& inColor)
    {
        return Call(e, c, 0, 0, 0, 0, inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue());
    }
    static intptr_t Point(const UIEditBox* e, EeUiEditCall c, const GE2DPoint& inPoint, intptr_t a = 0)
    {
        return Call(e, c, a, 0, 0, 0, inPoint.GetX(), inPoint.GetY());
    }
};
typedef EeUiEditCall C;
typedef UIEditBoxRust R;

// FUNCTION: LLE 0x10069e8b
// Creates an empty edit box (one blank line) with caret, key-repeat timers and text; multi-line boxes get a scroll bar
UIEditBox::UIEditBox(unsigned long inId, GEFont* inFont, unsigned long inMaxChars, bool inNumbersOnly,
                     const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                     unsigned long inBorderThickness, bool inMultiLine, bool inPasswordBox)
    : UIControl(kControlTypeEditBox, inId, inRectangle, inForm, inIndex, inBorderThickness)
{
    uint32_t theFlags = (inNumbersOnly ? 1u : 0u) | (inMultiLine ? 2u : 0u) | (inPasswordBox ? 4u : 0u);
    this->mRust = ee_ui_edit_new(this, R::Op, inFont, inMaxChars, theFlags, inBorderThickness);
}

UIEditBox::~UIEditBox() { ee_ui_edit_free(this->mRust); }
void UIEditBox::AddIMEResultString(const UWideString& inString) { R::Call(this, C::AddImeResult, (intptr_t)(const char*)inString); }
long UIEditBox::ChangeIMECompositionString(const UWideString& inString, const UString& inAttributes, unsigned long inCursor)
{
    return (long)R::Call(this, C::ImeChange, (intptr_t)(const char*)inString, inCursor, (intptr_t)(const char*)inAttributes);
}
void UIEditBox::ChangeFont(GEFont* inFont) { R::Call(this, C::ChangeFont, (intptr_t)inFont); }
void UIEditBox::ChangeWidth(unsigned long inWidth) { R::Call(this, C::ChangeWidth, inWidth); }
long UIEditBox::CloseIMECompositionString() { return (long)R::Call(this, C::ImeClose); }
void UIEditBox::Copy() { R::Call(this, C::Copy); }
long UIEditBox::CopyData() { return (long)R::Call(this, C::CopyData); }
void UIEditBox::Cut() { R::Call(this, C::Cut); }
void UIEditBox::DeleteSelectedText() { R::Call(this, C::DeleteSelectedText); }
void UIEditBox::DeleteText(unsigned long inStartIndex, unsigned long inStartLine, unsigned long inEndIndex, unsigned long inEndLine)
{
    R::Call(this, C::DeleteRange, inStartIndex, inStartLine, inEndIndex, inEndLine);
}
void UIEditBox::DeleteText() { R::Call(this, C::Clear); }
long UIEditBox::DrawPolygons() { return (long)R::Call(this, C::DrawPolygons); }
long UIEditBox::DrawTextA() { return (long)R::Call(this, C::DrawText); }
void UIEditBox::GetAllText(UWideString& outString) const { outString = (const char*)R::Call(this, C::AllText); }
bool UIEditBox::GetCursorOn() const { return R::Call(this, C::CursorOn) != 0; }
GEText* UIEditBox::GetGEText() const { return reinterpret_cast<GEText*>(R::Call(this, C::Text)); }
bool UIEditBox::GetIMEEnabled() const { return R::Call(this, C::ImeEnabled) != 0; }
bool UIEditBox::GetIsPasswordBox() const { return R::Call(this, C::IsPasswordBox) != 0; }
bool UIEditBox::GetIsTextSelected() const { return R::Call(this, C::IsTextSelected) != 0; }
unsigned long UIEditBox::GetLineCount() const { return (unsigned long)R::Call(this, C::LineCount); }
const char* UIEditBox::GetText(unsigned long inLine) const { return (const char*)R::Call(this, C::Line, inLine); }
void UIEditBox::Paste() { R::Call(this, C::Paste); }
long UIEditBox::OpenIMECompositionString() { return (long)R::Call(this, C::ImeOpen); }
void UIEditBox::ReleaseCapturedKeyboard() { R::Call(this, C::ReleaseKeyboard); }
void UIEditBox::SelectText() { R::Call(this, C::SelectAll); }
void UIEditBox::SelectText(unsigned long inCaretIndex, unsigned long inStartIndex, unsigned long inCaretLine, unsigned long inStartLine)
{
    R::Call(this, C::Select, inCaretIndex, inStartIndex, inCaretLine, inStartLine);
}
void UIEditBox::SetCaretLocation(unsigned long inIndex, unsigned long inLine) { R::Call(this, C::SetCaretLocation, inIndex, inLine); }
void UIEditBox::SetCharScroll(unsigned long inCharScroll) { R::Call(this, C::SetCharScroll, inCharScroll); }
void UIEditBox::SetCursorColor(const GEColor& inColor) { R::Color(this, C::SetCursorColor, inColor); }
void UIEditBox::SetIsPasswordBox(bool inPasswordBox) { R::Call(this, C::SetIsPasswordBox, inPasswordBox); }
void UIEditBox::SetLocation(const GE2DPoint& inLocation) { R::Point(this, C::SetLocation, inLocation); }
void UIEditBox::SetSelectedBgColor(const GEColor& inColor) { R::Color(this, C::SetSelectedBgColor, inColor); }
void UIEditBox::SetText(const UWideString& inString) { R::Call(this, C::SetText, (intptr_t)(const char*)inString); }
long UIEditBox::UpdateKeyboard() { return (long)R::Call(this, C::UpdateKeyboard); }
long UIEditBox::UpdateTranslatedKey(const char* inKey, short inRepeat)
{
    return (long)R::Call(this, C::UpdateTranslatedKey, (intptr_t)inKey, inRepeat);
}
long UIEditBox::MouseLButtonDoublePressed() { return (long)R::Call(this, C::MouseDoublePressed); }
long UIEditBox::MouseLButtonDown() { return (long)R::Call(this, C::MouseLButtonDown); }
long UIEditBox::MouseLButtonUp() { return (long)R::Call(this, C::MouseLButtonUp); }
long UIEditBox::MouseMove(const GE2DPoint& inLocation) { return (long)R::Point(this, C::MouseMove, inLocation); }
void UIEditBox::ProcessEnterKey() { R::Call(this, C::ProcessEnterKey); }
void UIEditBox::TabbedFrom() { R::Call(this, C::TabbedFrom); }
void UIEditBox::TabbedTo() { R::Call(this, C::TabbedTo); }

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
