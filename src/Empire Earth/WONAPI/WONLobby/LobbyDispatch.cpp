// Lobby event dispatch: broadcast to the lobby screens, send to one component, or hand to the lobby logic
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
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyScreens.h"

namespace WONAPI {
// LobbyLogicEventHook (0x7590e0, cdecl) receives the event while the caller still holds it
void LobbyLogicEventHook(ComponentEvent*);
// Broadcasts a plain lobby event of the given type to the lobby screens
void LobbyBroadcastLobbyEvent(int type,bool broadcast)
{
    if(gLobbyData.get()==NULL) return;
    SmartPtr<LobbyEvent> event = new LobbyEvent(type);
    gLobbyData->mLobbyContainer->FireBroadcastEvent(event.get(),broadcast);
}
// Sends an event straight to one component, from the lobby container by default
void LobbySendEventTo(ComponentEvent* event,Component* target)
{
    SmartPtr<ComponentEvent> eventOwner=event;
    if(gLobbyData.get()!=NULL && target!=NULL) {
        if(event->mComponent==NULL)
            event->SetComponent(gLobbyData->mLobbyContainer.get());
        event->mBroadcast=true;
        target->HandleComponentEvent(event);
    }
}
// Hands an event to the lobby logic, from the lobby container by default
void LobbyDispatchEvent(ComponentEvent* event)
{
    SmartPtr<ComponentEvent> eventOwner=event;
    if(gLobbyData.get()!=NULL) {
        if(event->mComponent==NULL)
            event->SetComponent(gLobbyData->mLobbyContainer.get());
        LobbyLogicEventHook(event);
    }
}
// Shows the login screen and tells it that it was opened
void LobbyShowLoginScreen()
{
    if(gLobbyData.get()==NULL) return;
    gLobbyData->mLobbyContainer->ShowLoginScreen();
    LobbyEvent* event=new LobbyEvent(100014);
    gLobbyData->mLobbyContainer->mLoginScreen->BroadcastEventDown(event);
}
}

namespace WONAPI {
// Broadcasts an event from the lobby container to the lobby screens (emitted at 0x759100)

void LobbyBroadcastEvent(ComponentEvent* event,bool sendNow)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mLobbyContainer->FireBroadcastEvent(event,sendNow);
}
}
