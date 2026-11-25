// The host of the skins' layouts (R6 WON round 37: rust/crates/ee_won_gui/src/skinlay.rs lays out MSComboBox,
// MSScrollContainer, MSListCtrl, WONComboBox, WONScrollContainer and WONListCtrl and sizes WONComboBox,
// MSEtchedFrameGroup and WONSimpleButton). R6 WONLobby round 23: the WON skin's widgets are shell kinds
// (shell_won.rs) that answer their own fields, header and images; this answers the combo boxes' and scroll
// containers' parts and the calls on other components.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONGUIDraw.h"
#include "ee/wonapi/WONGUIShell.h"
#include "ee/wonapi/WONSkinLayout.h"
// ---- the skins' layouts' host (skinlay.rs EeWonSkinLay): the receiver's parts and fields by the layout's kind, and the
// calls each layout makes
namespace WONAPI {
typedef EeWonSkinLay SL;
typedef EeWonSkinLayKind SK;
typedef EeWonSkinPart SP;
typedef EeWonSkinField SF;
template <class C> static intptr_t SkinScrollPart(C* t, intptr_t which)
{
    switch ((SP)which) {
    case SP::ScrollArea: return (intptr_t)static_cast<Component*>(t->GetScrollArea());
    case SP::VertBar: return (intptr_t)static_cast<Component*>(t->GetScrollbar(true));
    case SP::HorzBar: return (intptr_t)static_cast<Component*>(t->GetScrollbar(false));
    default: return 0;
    }
}
// the parts the kinds do not hold (a combo box's, a scroll container's: their base classes'); fields are the kinds'
static intptr_t SkinPart(Component* c, SK k, intptr_t which)
{
    switch (k) {
    case SK::MsCombo: case SK::WonCombo: case SK::WonComboSize: {
        ComboBox* t = static_cast<ComboBox*>(c);
        return (SP)which == SP::Button ? (intptr_t)static_cast<Component*>(t->mButton.get())
             : (SP)which == SP::Edit ? (intptr_t)static_cast<Component*>(t->mEdit.get()) : (intptr_t)static_cast<Component*>(t->mListArea.get());
    }
    case SK::MsScroll: case SK::MsList: case SK::WonScroll: case SK::WonList: case SK::WonListTextHeader: case SK::WonListHeader:
        return SkinScrollPart(static_cast<ScrollbarScroller*>(c), which);
    default: return 0;
    }
}
}

using namespace WONAPI;
extern "C" intptr_t ee_won_guicpp_skin_call(void* inMe, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    Component* m = (Component*)inMe;
    Component* x = (Component*)b; // (the bounds and visibility the layouts read are compmem.rs's block)
    if (op != EeWonGuiOp::SkinLay) return 0;
    switch ((SL)a) {
    case SL::Part: return SkinPart(m, (SK)b, c);
    case SL::SetPosSize: x->SetPosSize(((const int*)c)[0], ((const int*)c)[1], ((const int*)c)[2], ((const int*)c)[3]); return 0;
    case SL::DesiredSize: x->GetDesiredSize(((int*)c)[0], ((int*)c)[1]); return 0;
    case SL::SetLeftPad: static_cast<InputBox*>(x)->SetLeftPad((int)c); return 0;
    case SL::ListCount: return static_cast<ListArea*>(x)->GetNumItems();
    case SL::ListTotalWidth: return static_cast<ListArea*>(x)->GetTotalWidth();
    case SL::SetColumnWidth: static_cast<MultiListArea*>(static_cast<ScrollbarScroller*>(m)->GetScrollArea())->SetColumnWidth((int)b, (int)c, false); return 0;
    default: return 0;
    }
}
