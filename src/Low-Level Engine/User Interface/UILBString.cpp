// C:\EEX\Low-Level Engine\User Interface\UILBString.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10078251..0x100785cd: the U2D key init chains, then DrawTextA and UpdateTranslatedKey. The class's inline members
// and every UIListBox<UWideString> member are emitted first by UIComboBox.cpp. UILineGraph.cpp's key chains start at
// 0x100785cd.
#include <windows.h>
#include <mbstring.h>

#include "rust/ui/draw_ffi.h"
#include "rust/ui/obj.h"
#include "rust/ui/rect.h"
#include "rust/ui/string_list_ffi.h"

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
// UIControl's, the scroll bar's (a part by address), the polls', the selection rhombi's and the focus line's, the keys
#define lst static_cast<UIControl*>(inObj)
#define theBar reinterpret_cast<UIScrollBar*>(a)
#define thePoll reinterpret_cast<TMPoll*>(a)
#define theLine reinterpret_cast<GELine*>(a)
#define theRhombus reinterpret_cast<GERhombus*>(a)
EE_UI_CASE(list_edit_key)
{
    switch (a) {
    case 6: return UIControl::HomeKeyPressed();
    case 7: return UIControl::EndKeyPressed();
    case 8: return UIControl::UpKeyPressed();
    case 9: return UIControl::DownKeyPressed();
    default: return UIControl::EnterKeyPressed();
    }
}
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

// Draws the scroll bar text and each visible line of the string list box, clipped by the form
long UILBString::DrawTextA()
{
    GERectangle theRectangle;
    long theResult = kErrorOK;
    unsigned long theLineHeight = this->GetLineHeight();
    unsigned long i = 0;
    this->DrawScrollBarText();
    UIControl::DrawTextA();
    const GERectangle& theCtr = this->GetCopyTextRectangle();
    EeUiRect theCopy = {theCtr.mBottom, theCtr.mLeft, theCtr.mRight, theCtr.mTop};
    for (; i < this->GetDrawCount(); i++) {
        // the row, one pixel wider than the text rectangle (ee_ui draw.rs)
        EeUiRect theRow = ee_ui_string_list_row(theCopy, i, theLineHeight);
        theRectangle = GERectangle(theRow.left, theRow.top, theRow.right, theRow.bottom);
        this->mCopiedText->SetLocation(GE2DPoint(theRectangle.mLeft, theRectangle.mTop));
        this->mCopiedText->SetText(this->mCopyLineArray[i]);
        theResult = this->GetForm()->DrawClippedText(theRectangle, this->mCopiedText, this);
    }
    return theResult;
}

// The Rust type-to-find's questions (ee_ui find_key.rs): the virtual line count and a line's first byte
struct UILBStringRust {
    static unsigned long Lines(UILBString* inList) { return inList->GetNumLines(); }
};
extern "C" unsigned long ee_uicpp_string_list_lines(void* inList)
{
    return UILBStringRust::Lines(static_cast<UILBString*>(inList));
}
extern "C" int ee_uicpp_string_list_first(void* inList, unsigned long inIndex)
{
    UWideString theLine = static_cast<UILBString*>(inList)->GetData(inIndex);
    return theLine.GetLength() > 0 ? (unsigned char)theLine[0] : -1;
}

// Selects the next line starting with the typed letter (any case), wrapping to the top; kErrorNotFound when none does
// (the search is Rust: ee_ui find_key.rs)
long UILBString::UpdateTranslatedKey(const char* inKey, short inRepeat)
{
    long i = ee_ui_string_list_find(this, inKey, this->GetSelectionIndex());
    if (i < 0)
        return kErrorNotFound;
    this->SetScrollBarCurrentRange(i);
    this->ChangeSelectionIndex(i, true, true);
    this->SetDirty(true);
    return kErrorOK;
}
