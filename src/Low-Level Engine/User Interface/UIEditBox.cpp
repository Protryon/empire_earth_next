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

#include "rust/ui/editobj.h"
#include "rust/ui/obj.h"
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
#include "lle/UIObjHost.h"
#include "lle/UIShims.h"

struct UIEditBoxRust {
    // EditKey (ee_ui edit_keys.rs): UIControl's key predicates and the keyboard's A, L and Return entities
    static bool Key(intptr_t inKey)
    {
        switch (inKey) {
        case 0: return UIControl::ShiftKeyPressed();
        case 1: return UIControl::Key(EeUiKey::Control)->GetValue() != 0;
        case 2: return UIControl::Key(EeUiKey::BackSpace)->GetValue() != 0;
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
        case 17: return UIControl::Key(EeUiKey::Enter)->JustPressed();
        case 18: return UIControl::Key(EeUiKey::NumEnter)->JustPressed();
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
        case EeUiOp::PollCreate: return (intptr_t) new TMPoll((unsigned long)a, 0, false);
        case EeUiOp::PollDelete: delete thePoll; break;
        case EeUiOp::PollRunning: return thePoll->GetIsRunning();
        case EeUiOp::PollPoll: return thePoll->Poll();
        case EeUiOp::PollStart: thePoll->Start(); break;
        case EeUiOp::PollStop: thePoll->Stop(); break;
        case EeUiOp::TextSetBytes: theText->SetText(UWideString(reinterpret_cast<const char*>(b))); break;
        case EeUiOp::TextFontHeight: return theText->GetFont()->GetHeight();
        case EeUiOp::DrawCountDecrement: e->UIControl::DrawTextA(); break;
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
        case EeUiOp::TranslatedKey: e->GetForm()->GetManager()->UpdateTranslatedKey(reinterpret_cast<const char*>(a), 1); break;
        case EeUiOp::TextInputCancel:
            // ImmNotifyIME(NI_COMPOSITIONSTR, CPS_CANCEL)
            PlatTextInputCancel(e->GetRasterizer()->GetWindow(e->GetScreenIndex())->GetOSRepresentation());
            break;
        case EeUiOp::BaseReleaseKeyboard: e->UIControl::ReleaseCapturedKeyboard(); break;
        case EeUiOp::BaseMouseDoublePressed: return e->UIControl::MouseLButtonDoublePressed();
        case EeUiOp::LineDelete: delete theLine; break;
        case EeUiOp::RhombusCreate: return (intptr_t) new GERhombus();
        case EeUiOp::RhombusDelete: delete theRhombus; break;
        case EeUiOp::TextSetBgColor: theText->SetBgColor(GEColor(f[0], f[1], f[2])); break;
        case EeUiOp::TextSetTransparent: theText->SetIsTransparent(b != 0); break;
        default: break;
        }
        return 0;
    }
};

// the keyboard's A, L and Return keys, the scroll bar, the form's mouse, the lines and the text colour: Rust's (ee_ui
// lle_bg_edit.rs, UI lane round 12) before the host above
#define UIEB(e) static_cast<UIEditBox*>(reinterpret_cast<UIControl*>(e))
static const unsigned long kUIEditKeys[] = {kKeyA, kKeyL, kKeyReturn};
extern "C" void ee_uicpp_edit_key_fetch(uint32_t inWhich, uintptr_t* inOut)
{ gISKeyboard->GetEntity(kUIEditKeys[inWhich], *reinterpret_cast<ISEntity**>(inOut)); }
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
extern "C" intptr_t ee_uicpp_edit_host(void* inObj, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f)
{ return UIEditBoxRust::Op(inObj, inOp, a, b, c, d, f); }

// Creates an empty edit box (one blank line) with caret, key-repeat timers and text; multi-line boxes get a scroll bar
UIEditBox::UIEditBox(unsigned long inId, GEFont* inFont, unsigned long inMaxChars, bool inNumbersOnly,
                     const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                     unsigned long inBorderThickness, bool inMultiLine, bool inPasswordBox)
    : UIControl(kControlTypeEditBox, inId, inRectangle, inForm, inIndex, inBorderThickness)
{
    uint32_t theFlags = (inNumbersOnly ? 1u : 0u) | (inMultiLine ? 2u : 0u) | (inPasswordBox ? 4u : 0u);
    this->mRust = ee_ui_edit_new(this, ee_ui_edit_host, inFont, inMaxChars, theFlags, inBorderThickness);
}

UIEditBox::~UIEditBox() { ee_ui_edit_free(this->mRust); }
