// Direct-connection panel: the list of direct-connect servers with Add and Remove buttons
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233

#include <windows.h>
#include <string>
#include <map>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyControlConfig.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyControls.h"

namespace WONAPI {
// The connection list is a ServerListCtrl (WONLobbyRoomServer.h); this file only calls its getter.
// DirectConnectCtrl itself is declared in WONLobbyControlConfig.h
}

namespace WONAPI {
DirectConnectCtrl::DirectConnectCtrl() {}
// The destructor (0x7badf0) stores its own vptr (0x7bae0f), then releases the remove button (+0x8c),
// the add button (+0x88) and the connection list (+0x84), then the Container
DirectConnectCtrl::~DirectConnectCtrl() {}
// Looks up the connection list and the add and remove buttons from the layout and listens for broadcasts
void DirectConnectCtrl::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2)
        return;
    ComponentConfig* config=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(config,this->mConnectionList,"ConnectionList",__FILE__,34);
    SafeGetComponent(config,this->mAddButton,"AddButton",__FILE__,35);
    SafeGetComponent(config,this->mRemoveButton,"RemoveButton",__FILE__,36);
    this->SubscribeToBroadcast(1);
}
// Asks to add a direct connection on Add, or to remove the selected one on Remove; the rest goes to Component
void DirectConnectCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch(inEvent->mType) {
    case ComponentEvent_ButtonPressed:
        if(inEvent->mComponent==this->mAddButton.get()) {
            this->DeliverEvent(new ChangeDirectConnectEvent(0,NULL),false);
        }
        else if(inEvent->mComponent==this->mRemoveButton.get()) {
            LobbyGame* game=this->mConnectionList->GetSelGame();
            if(game!=NULL)
                this->DeliverEvent(new ChangeDirectConnectEvent(1,game),false);
        }
        else break;
        return;
    }
    Component::HandleComponentEvent(inEvent);
}
}
