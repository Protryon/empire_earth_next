// The lobby's logic manager: one list of logic objects that it initializes, kills and passes events to (the list and
// every method are ee_won_lobby logicmgr.rs's since R6 WON lobby and network round 35; this is its host)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyLogic.h"
namespace WONAPI {
// the manager's host (logicmgr.rs): one statement each on a logic or an event; a logic's virtual runs under WonGuard
static intptr_t LogicMgrHost(EeWonLogicMgrOp inOp, intptr_t a, intptr_t b)
{
    typedef EeWonLogicMgrOp Op;
    LobbyLogic* aLogic = reinterpret_cast<LobbyLogic*>(a);
    ComponentEvent* aClick = reinterpret_cast<ComponentEvent*>(a);
    switch (inOp) {
    case Op::AddRef: aLogic->AddRef(); return 0;
    case Op::Release: aLogic->Release(); return 0;
    case Op::InitComponent: return WON_GUARDED_V(aLogic->InitComponent(reinterpret_cast<ComponentConfig*>(b)));
    case Op::Kill: return WON_GUARDED_V(aLogic->Kill());
    case Op::HandleControlEvent: return WON_GUARDED_V(aLogic->HandleControlEvent((int)b));
    case Op::HandleComponentEvent: return WON_GUARDED_V(aLogic->HandleComponentEvent(reinterpret_cast<ComponentEvent*>(b)));
    case Op::Reset: return WON_GUARDED_V(aLogic->Reset());
    case Op::ClickItem: return aClick->mEv.item;
    case Op::ClickButton: return aClick->mEv.button;
    case Op::IsMenuItem: return dynamic_cast<MenuItem*>(reinterpret_cast<ListItem*>(a)) != NULL ? 1 : 0;
    case Op::MenuId: return dynamic_cast<MenuItem*>(reinterpret_cast<ListItem*>(a))->mMenuId;
    case Op::ControlId: return reinterpret_cast<ComponentEvent*>(a)->GetControlId();
    }
    return 0;
}

// The constructor (0x766b30) only builds the empty logic list; the class has no vtable (0xc bytes)
LobbyLogicMgr::LobbyLogicMgr() : mRust(ee_won_logicmgr_new(LogicMgrHost)) {}
LobbyLogicMgr::~LobbyLogicMgr() { ee_won_logicmgr_free(this->mRust); }
// Turns left clicks on menu items and button presses into control events; passes other events to the logics
void LobbyLogicMgr::HandleComponentEvent(ComponentEvent* e) { this->Call(EeWonLogicMgrCall::HandleComponentEvent, (intptr_t)e, e->mEv.kind); }
}
