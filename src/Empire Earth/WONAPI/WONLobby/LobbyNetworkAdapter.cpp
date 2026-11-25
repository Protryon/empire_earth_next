// The lobby's network glue: the adapter selection (the saved adapter changes only after a successful switch), the UDP
// port's bind (LobbyUDPManager.cpp's until R6 WON round 38) and the completions run on the window thread
// (LobbyCompletion.cpp's, 0x757d70..0x757f24, file name inferred)
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
#include "ee/wonapi/WONLobbyPersistence.h"
#include "platform/PlatThread.h"
#include "rust/won_net/udpmgr.h"
namespace WONAPI {
// Switches the local address to an adapter, saves the choice if new and rebinds the UDP port after a switch
void LobbySetNetworkAdapter(long address)
{
    bool changed=false;
    if(WONAPI::IPAddr::GetLocalHost()!=address) {
        changed=true;
        if(!IPAddr::SetLocalHost(address,true)) return;
    }
    if(WONAPI::IPAddr::GetLocalHost()!=LobbyGetNetworkAdapter()) LobbySaveNetworkAdapter();
    if(changed) LobbyBindUDPPort();
}
// Binds the lobby UDP port from the config (8888 by default; + the harness slot's offset, TEST ONLY), else a random
// port 10000-19999, and starts receiving (ee_won udpmgr.rs ee_won_udpmanager_bind_lobby)
void LobbyBindUDPPort()
{
    LobbyConfig* aConfig=GetLobbyConfig();
    unsigned short aPort=(unsigned short)((aConfig?aConfig->mLobbyPort:8888)+LobbyTestPortOffset());
    UDPManager* aMgr=LobbyGetUDPManager();
    LobbySetLobbyPort(ee_won_udpmanager_bind_lobby(aMgr!=NULL?aMgr->GetManagerObject():NULL,aPort));
}
class LobbyCompletionEvent : public WindowEventBase {
public:
    LobbyCompletionEvent(LobbyCompletionBase* inCompletion, AsyncOp* inOp)
        : mCompletion(inCompletion), mOp(inOp) {}
    virtual void Deliver();
    SmartPtr<LobbyCompletionBase> mCompletion; //10
    AsyncOpPtr mOp; //14
};
// Runs the completion at once on the window thread, else posts it there as a window event
void LobbyCompletionBase::Complete(AsyncOpPtr theParam)
{
    DWORD aThreadId = WONAPI::WindowManager::GetThreadId();
    if (PlatCurrentThreadId() == aThreadId)
        this->Run(theParam.get());
    else {
        WindowManager* aManager = WONAPI::WindowManager::GetDefaultWindowManager();
        if (aManager != NULL)
            aManager->PostEvent(new LobbyCompletionEvent(this, theParam.get()), true);
    }
}
// Runs the posted completion for its operation on the window thread
void LobbyCompletionEvent::Deliver()
{
    this->mCompletion->Run(this->mOp.get());
}
// The implicit destructor (0x757ea0) releases mOp, then mCompletion, then the WindowEventBase part
}
