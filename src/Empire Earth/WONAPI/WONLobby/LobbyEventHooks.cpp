// Installs and removes the lobby's logic event listener and its timer
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
void LobbyLogicEventHook(ComponentEvent*);

// FUNCTION: WON 0x00759020
// Passes the lobby timer tick to the lobby logic
void LobbyTimerCallback(int)
{
    if(gLobbyData.get()!=NULL)
        gLobbyData->mLogicMgr.DispatchTimerEvent();
}

// FUNCTION: WON 0x00758f70
// Routes the lobby container's events to the lobby logic and starts the lobby timer
void LobbyInstallEventHooks()
{
    if(gLobbyData.get()==NULL)
        return;
    LobbyAddLogicListener(gLobbyData->mLobbyContainer.get());
    gLobbyData->mTimerEvent=new UserEvent(LobbyTimerCallback,0);
    WONAPI::WindowManager::GetDefaultWindowManager()->AddTimerEvent(gLobbyData->mTimerEvent.get());
}

// FUNCTION: WON 0x00759040
// Detaches the lobby logic from the lobby container, stops the timer and clears window subscriptions
void LobbyRemoveEventHooks()
{
    if(gLobbyData.get()==NULL)
        return;
    gLobbyData->mLobbyContainer->AddListener(NULL);
    WindowManager* manager=WONAPI::WindowManager::GetDefaultWindowManager();
    if(manager!=NULL) {
        manager->RemoveTimerEvent(gLobbyData->mTimerEvent.get());
        manager->ClearSubscriptions();
    }
}

// FUNCTION: WON 0x00759080
// Makes a component send its events to the lobby logic
void LobbyAddLogicListener(Component* component)
{
    if(component!=NULL)
        component->AddListener(new ComponentListenerFunc(LobbyLogicEventHook));
}

// FUNCTION: WON 0x007590e0
// Hands a component event to the lobby logic
void LobbyLogicEventHook(ComponentEvent* event)
{
    if(gLobbyData.get()!=NULL)
        gLobbyData->mLogicMgr.HandleComponentEvent(event);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007590d0
// (public: virtual void __thiscall WONAPI::ComponentListenerFunc::Virtual04(class WONAPI::ComponentEvent *)    [?Virtual04@ComponentListenerFunc@WONAPI@@)
