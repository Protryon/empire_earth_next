// Running the lobby: start, modal message loop, exit status, suspend and resume
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
namespace WONAPI {
extern SmartPtr<Dialog> gLobbyFailureDialog;
extern bool gLobbyInitFailed;
void LobbyNetSuspend(); // calls the network logic's virtual at +0x44
void LobbyNetResume(); // calls the network logic's virtual at +0x48
// FUNCTION: WON 0x00762760
// Runs the lobby until it exits and returns its exit status; after a failed start only shows the failure dialog
int LobbyRun()
{
    if(gLobbyInitFailed && gLobbyFailureDialog.get()!=NULL) {
        Window* window=gLobbyFailureDialog->GetRootWindow();
        if(window!=NULL && window->PlatformIsVisible())
            window->RunDialog(gLobbyFailureDialog.get());
        return 0;
    }
    if(gLobbyData.get()==NULL) return 0;
    gLobbyData->mLobbyStatus=static_cast<LobbyStatus>(0);
    gLobbyData->mRunMode=static_cast<LobbyRunMode>(1);
    gLobbyData->mSuspended=false;
    WindowManager* manager=GetLobbyWindow();
    if(manager==NULL) return 0;
    manager->MessageLoop();
    if(gLobbyData.get()==NULL) return 0;
    return gLobbyData->mLobbyStatus;
}
// FUNCTION: WON 0x007627f0
// Sets the lobby's exit status, tells all controls the lobby is closing and ends the loop if it is running
void LobbyExit(int status)
{
    if(gLobbyData.get()==NULL) return;
    gLobbyData->mLobbyStatus=static_cast<LobbyStatus>(status);
    LobbyRunMode mode=gLobbyData->mRunMode;
    LobbyBroadcastLobbyEvent(100013,true);
    if(mode!=0) {
        WindowManager* manager=GetLobbyWindow();
        if(manager!=NULL) manager->ExitMessageLoop();
    }
}
// FUNCTION: WON 0x00762830
// Suspends the lobby: stops its music and logon sound, cancels pending pings and suspends networking
void LobbySuspend()
{
    if(gLobbyData.get()==NULL) return;
    gLobbyData->mSuspended=true;
    ::LobbyGlobal_LobbyMusic_Sound->Stop();
    ::LobbyGlobal_SierraLogon_Sound->Stop();
    if(LobbyGetNumPingsToDo()>0) {
        LobbyKillPings(1);
        LobbyBroadcastLobbyEvent(100059,true);
    }
    LobbyNetSuspend();
}
// FUNCTION: WON 0x00762880
// Resumes the lobby: restarts the music if enabled and resumes networking
void LobbyResume()
{
    if(gLobbyData.get()==NULL) return;
    gLobbyData->mSuspended=false;
    if(LobbyGetLobbyMusic()) ::LobbyGlobal_LobbyMusic_Sound->Resume();
    LobbyNetResume();
}
}

#include "ee/wonapi/WONLobbyAccountLogic.h"

namespace WONAPI {
// WON SDK lobby entry point
// FUNCTION: WON 0x00762740
// Starts the lobby's start-up sequence (connect and log in)
void LobbyStart()
{
    if(gLobbyData.get()==NULL) return;
    gLobbyData->mSuspended=false;
    gLobbyData->mInitLogic->Init();
}
}
