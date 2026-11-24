// ListAreaToolTip (R6 WON round 34): the tooltip a list box opens over an item it cuts off is the Rust tip's
// (rust/crates/ee_won_gui/src/tipobj.rs: the paint, DoTip and EndTip); this is its host, each op one of the original's
// statements (the class keeps the list and the item).
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <deque>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONGUIDraw.h"
using namespace WONAPI;
typedef EeWonTipOp To;
static_assert(StandardColor_ToolTipBack == EE_WON_TIP_COLOR_BACK && StandardColor_ToolTipText == EE_WON_TIP_COLOR_TEXT, "tipobj.rs colours");

// OpenPopup takes its parameters by reference
static void OpenWith(Window* inWindow, PopupParams inParams) { inWindow->OpenPopup(inParams); }

extern "C" intptr_t ee_won_guicpp_tip_call(void* inMe, EeWonTipOp op, intptr_t a, intptr_t b)
{
    ListAreaToolTip* t = (ListAreaToolTip*)inMe;
    Graphics* g = (Graphics*)a;
    switch (op) {
    case To::BasePaint: t->Component::Paint(*g); return 0;
    case To::ItemIs: return t->mItem.get() == (ListItem*)a ? 1 : 0;
    case To::HasItem: return t->mItem.get() != NULL ? 1 : 0;
    case To::HasParent: return t->mParent != NULL ? 1 : 0;
    case To::SetItem: t->mItem = (ListItem*)a; return 0;
    case To::ItemTop: return t->mItem->mTop;
    case To::ItemWidth: return t->mItem->mWidth;
    case To::ItemHeight: return t->mItem->mHeight;
    case To::Scheme: return (intptr_t)g->mColorScheme;
    case To::SetScheme: g->mColorScheme = (ColorScheme*)b; return 0;
    case To::HasOwnScheme: return t->GetOwnColorScheme() != NULL ? 1 : 0;
    case To::ListScheme: return (intptr_t)t->mListArea->GetColorScheme();
    case To::ListLeftPad: return t->mListArea->GetLeftPad();
    case To::ListRightPad: return t->mListArea->GetRightPad();
    case To::ListTopPad: return t->mListArea->GetTopPad();
    case To::ListVertOffset: return t->mListArea->GetVertOffset();
    case To::ListWidth: return t->mListArea->Width();
    case To::Width: return t->Width();
    case To::Height: return t->Height();
    case To::Draw: WONDrawPlain(*g, *(const EeWonDrawOp*)b); return 0;
    case To::ScrollPaint: t->mListArea->ScrollPaint(*g); return 0;
    case To::ListLocalToRoot: t->mListArea->LocalToRoot(((int*)a)[0], ((int*)a)[1]); return 0;
    case To::SetSize: t->SetSize((int)a, (int)b); return 0;
    case To::OpenPopup: OpenWith(t->mListArea->GetRoot(), PopupParams(t, EE_WON_TIP_POPUP_FLAGS, t->mListArea, (int)a, (int)b)); return 0;
    case To::ClosePopup: t->mListArea->GetRoot()->ClosePopup(t); return 0;
    }
    return 0;
}
