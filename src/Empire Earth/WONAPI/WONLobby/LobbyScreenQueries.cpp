// Free lobby functions that show login and Internet screen panels and report which lobby screen is shown
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
#include "ee/wonapi/WONLobbyAccountLogic.h"
#include "ee/wonapi/WONLobbyDialogLogic.h"
#include "ee/wonapi/WONLobbyLan.h"
#include "ee/wonapi/WONLobbyRoutingLogic.h"
#include "ee/wonapi/WONLobbyStaging.h"
#include "ee/wonapi/WONLobbyPing.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/WONCRC16.h"
#include "ee/wonapi/EELobbySound.h"
namespace WONAPI {
// FUNCTION: WON 0x007610b0
// Switches the Internet screen to its lobby (rooms) tab
void LobbyShowLobbyTab() { if(gLobbyData.get()!=NULL) gLobbyData->mLobbyContainer->mInternetScreen->ShowLobbyTab(); }
// FUNCTION: WON 0x007610d0
// Shows the create-account panel of the login screen
void LobbyShowCreateAccount() { if(gLobbyData.get()!=NULL) gLobbyData->mLobbyContainer->mLoginScreen->ShowCreateAccountCtrl(); }
// FUNCTION: WON 0x007610f0
// Shows the lost-password panel of the login screen
void LobbyShowLostPassword() { if(gLobbyData.get()!=NULL) gLobbyData->mLobbyContainer->mLoginScreen->ShowLostPasswordCtrl(); }
// FUNCTION: WON 0x00761110
// Shows the lost-username panel of the login screen
void LobbyShowLostUsername() { if(gLobbyData.get()!=NULL) gLobbyData->mLobbyContainer->mLoginScreen->ShowLostUsernameCtrl(); }
// FUNCTION: WON 0x00761130
// Returns whether the lobby is showing the Internet screen
bool LobbyOnInternetScreen() { return gLobbyData.get()!=NULL?gLobbyData->mLobbyContainer->OnInternetScreen():false; }
// FUNCTION: WON 0x00761150
// Returns whether the lobby is showing the LAN screen
bool LobbyOnLanScreen() { return gLobbyData.get()!=NULL?gLobbyData->mLobbyContainer->OnLanScreen():false; }
// FUNCTION: WON 0x00761170
// Returns whether the lobby is showing the login screen
bool LobbyOnLoginScreen() { return gLobbyData.get()!=NULL?gLobbyData->mLobbyContainer->OnLoginScreen():false; }
// FUNCTION: WON 0x00761190
// Returns the game type of the screen shown: Internet 2, LAN 1, direct connect 3, else 0
int LobbyGetGameScreenType()
{
    if(gLobbyData.get()==NULL) return 0;
    return gLobbyData->mLobbyContainer->GetGameScreenType();
}
// FUNCTION: WON 0x00761210
// Returns the lobby's color scheme, or NULL without a lobby
ColorScheme* LobbyGetColorScheme() { return gLobbyData.get()!=NULL?gLobbyData->mLobbyContainer->GetColorScheme():NULL; }
// FUNCTION: WON 0x00761230
// Returns the lobby's root window, or NULL without a lobby
Window* LobbyGetRootWindow() { return gLobbyData.get()!=NULL?gLobbyData->mLobbyContainer->GetRoot():NULL; }
// FUNCTION: WON 0x00761250
// Returns the default window manager
WindowManager* GetLobbyWindow() { return WindowManager::GetDefaultWindowManager(); }
// FUNCTION: WON 0x007628b0
// Returns whether the lobby is suspended; false without a lobby
bool LobbyIsSuspended()
{
    if(gLobbyData.get()==NULL) return false;
    return gLobbyData->mSuspended;
}
}
