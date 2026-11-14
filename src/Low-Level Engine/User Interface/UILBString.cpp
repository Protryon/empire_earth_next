// C:\EEX\Low-Level Engine\User Interface\UILBString.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10078251..0x100785cd: the U2D key init chains, then DrawTextA and UpdateTranslatedKey. The class's inline members
// and every UIListBox<UWideString> member are emitted first by UIComboBox.cpp. UILineGraph.cpp's key chains start at
// 0x100785cd.
#include <windows.h>
#include <mbstring.h>

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
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "UIObjHost.h"

// The Rust list box's ops that do not depend on T (ee_ui listobj.rs; UIListBoxRust<T>::Op answers the rest):
// UIControl's, the scroll bar's (a part by address), the polls', the selection rhombi's and the focus line's, the keys
intptr_t UIObjHost::List(UIControl* l, EeUiOp inOp, intptr_t a, intptr_t b, float* f)
{
    UIScrollBar* theBar = reinterpret_cast<UIScrollBar*>(a);
    TMPoll* thePoll = reinterpret_cast<TMPoll*>(a);
    GELine* theLine = reinterpret_cast<GELine*>(a);
    GERhombus* theRhombus = reinterpret_cast<GERhombus*>(a);
    intptr_t r = 0;
    if (UIObjHost::Base(l, inOp, a, f, &r) || UIObjHost::Part(reinterpret_cast<UIControl*>(a), inOp, f, &r))
        return r;
    switch (inOp) {
    case EeUiOp::EditKey:
        switch (a) {
        case 6: return UIControl::HomeKeyPressed();
        case 7: return UIControl::EndKeyPressed();
        case 8: return UIControl::UpKeyPressed();
        case 9: return UIControl::DownKeyPressed();
        default: return UIControl::EnterKeyPressed();
        }
    case EeUiOp::PollCreate: return (intptr_t) new TMPoll((unsigned long)a, 0, false);
    case EeUiOp::PollDelete: delete thePoll; break;
    case EeUiOp::PollRunning: return thePoll->GetIsRunning();
    case EeUiOp::PollPoll: return thePoll->Poll();
    case EeUiOp::PollStart: thePoll->Start(); break;
    case EeUiOp::PollStop: thePoll->Stop(); break;
    case EeUiOp::RhombusCreate: return (intptr_t) new GERhombus();
    case EeUiOp::RhombusDelete: delete theRhombus; break;
    case EeUiOp::RhombusRect: UIObjHost::PutRect(f, theRhombus->GetRectangle()); break;
    case EeUiOp::RhombusSetRect: theRhombus->SetRectangle(UIObjHost::Rect(f)); break;
    case EeUiOp::RhombusAssign: *theRhombus = *reinterpret_cast<GERhombus*>(b); break;
    case EeUiOp::RhombusDraw: return l->GetRasterizer()->Draw(theRhombus, l->GetScreenIndex());
    case EeUiOp::LineCreate:
        return (intptr_t) new GELine(GE2DPoint(0.0f, 0.0f), GE2DPoint(0.0f, 0.0f), GEColor(0.0f, 0.0f, 0.0f),
                                     GEColor(0.0f, 0.0f, 0.0f), 1.0f);
    case EeUiOp::LineDelete: delete theLine; break;
    case EeUiOp::LineSetPoints:
        theLine->SetStartPoint(GE2DPoint(f[0], f[1]));
        theLine->SetEndPoint(GE2DPoint(f[2], f[3]));
        break;
    case EeUiOp::LineSetColor: theLine->SetColor(GEColor(f[0], f[1], f[2])); break;
    case EeUiOp::LineDraw: return l->GetRasterizer()->Draw(theLine, l->GetScreenIndex());
    case EeUiOp::DrawCountDecrement: l->mDrawCount--; break;
    case EeUiOp::SetBgRectangle: l->SetBgRectangle(UIObjHost::Rect(f)); break;
    case EeUiOp::BaseMouseDoublePressed: return l->UIControl::MouseLButtonDoublePressed();
    case EeUiOp::CreateScrollBar:
        return (intptr_t) new UIScrollBar(l->mId, GE2DPoint(f[0], f[1]), f[2], 1, l->mForm, l->mIndex,
                                          l->GetBorderThickness());
    case EeUiOp::BarSetRange: theBar->SetRange(0, (unsigned long)b); break;
    case EeUiOp::BarSetPageSize: theBar->SetPageSize((unsigned long)b); break;
    case EeUiOp::BarValue: return theBar->GetSlider()->GetCurrentRangeValue();
    case EeUiOp::BarMax: return theBar->GetSlider()->GetMaxRange();
    case EeUiOp::BarSetValue: theBar->SetCurrentRangeValue((unsigned long)b); break;
    case EeUiOp::BarIncrementBy: theBar->IncrementBy((long)b); break;
    default: break;
    }
    return 0;
}
intptr_t UIListBoxHostOp(UIControl* inList, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t, intptr_t, float* f)
{
    return UIObjHost::List(inList, inOp, a, b, f);
}

// FUNCTION: LLE 0x10078371
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

// FUNCTION: LLE 0x10078470
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

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x10078251
// SYNTHETIC: LLE 0x1007825b
// SYNTHETIC: LLE 0x1007826b
// SYNTHETIC: LLE 0x10078277
// SYNTHETIC: LLE 0x10078281
// SYNTHETIC: LLE 0x1007828b
// SYNTHETIC: LLE 0x1007829b
// SYNTHETIC: LLE 0x100782a7
// SYNTHETIC: LLE 0x100782b1
// SYNTHETIC: LLE 0x100782bb
// SYNTHETIC: LLE 0x100782cb
// SYNTHETIC: LLE 0x100782d7
// SYNTHETIC: LLE 0x100782e1
// SYNTHETIC: LLE 0x100782eb
// SYNTHETIC: LLE 0x100782fb
// SYNTHETIC: LLE 0x10078307
// SYNTHETIC: LLE 0x10078311
// SYNTHETIC: LLE 0x1007831b
// SYNTHETIC: LLE 0x1007832b
// SYNTHETIC: LLE 0x10078337
// SYNTHETIC: LLE 0x10078341
// SYNTHETIC: LLE 0x1007834b
// SYNTHETIC: LLE 0x1007835b
// SYNTHETIC: LLE 0x10078367
