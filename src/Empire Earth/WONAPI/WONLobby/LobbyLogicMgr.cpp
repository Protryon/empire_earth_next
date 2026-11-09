// The lobby's logic manager: one list of logic objects that it initializes, kills and passes events to
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
// FUNCTION: WON 0x007e5c10
// Adds a logic object to the end of the list
void LobbyLogicMgr::AddLogic(LobbyLogic* logic)
{
    this->mLogicList.push_back(logic);
}
// FUNCTION: WON 0x007e5d40
// Lets every logic object find its controls in the lobby layout
void LobbyLogicMgr::Init(ComponentConfig* config)
{
    LogicList::iterator itr=this->mLogicList.begin();
    while(itr!=this->mLogicList.end()) {
        (*itr)->InitComponent(config);
        ++itr;
    }
}
// FUNCTION: WON 0x007e5d80
// Kills every logic object, cancelling their tracked operations
void LobbyLogicMgr::Kill()
{
    LogicList::iterator itr=this->mLogicList.begin();
    while(itr!=this->mLogicList.end()) {
        (*itr)->Kill();
        ++itr;
    }
}
// FUNCTION: WON 0x007e5db0
// Passes a control or menu id to every logic object
void LobbyLogicMgr::DispatchControlEvent(int control)
{
    LogicList::iterator itr=this->mLogicList.begin();
    while(itr!=this->mLogicList.end()) {
        (*itr)->HandleControlEvent(control);
        ++itr;
    }
}
// FUNCTION: WON 0x007e5df0
// Passes a component event to every logic object
void LobbyLogicMgr::DispatchComponentEvent(ComponentEvent* event)
{
    LogicList::iterator itr=this->mLogicList.begin();
    while(itr!=this->mLogicList.end()) {
        (*itr)->HandleComponentEvent(event);
        ++itr;
    }
}
// FUNCTION: WON 0x007e5e30
// Gives every logic object its timer tick (vtable slot 0xc, declared as Reset)
void LobbyLogicMgr::DispatchTimerEvent()
{
    LogicList::iterator itr=this->mLogicList.begin();
    while(itr!=this->mLogicList.end()) {
        (*itr)->Reset();
        ++itr;
    }
}
// FUNCTION: WON 0x007e5e60
// Turns left clicks on menu items and button presses into control events; passes other events to the logics
void LobbyLogicMgr::HandleComponentEvent(ComponentEvent* event)
{
    switch(event->mType) {
    default:
        this->DispatchComponentEvent(event);
        break;
    case ComponentEvent_ListItemUpClicked: {
        ListItemClickedEvent* click=static_cast<ListItemClickedEvent*>(event);
        if(click->mItem.get()==NULL || click->mButton!=0) return;
        MenuItem* menu=dynamic_cast<MenuItem*>(click->mItem.get());
        if(menu!=NULL) this->DispatchControlEvent(menu->mMenuId);
        break;
    }
    case ComponentEvent_ButtonPressed:
        this->DispatchControlEvent(event->GetControlId());
        break;
    }
}
}

namespace WONAPI {
// The constructor (0x766b30) only builds the empty logic list; the class has no vtable (0xc bytes)
LobbyLogicMgr::LobbyLogicMgr() {}
}
