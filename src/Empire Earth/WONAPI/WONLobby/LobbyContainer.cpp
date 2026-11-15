// The lobby's top container: switches between the login, Internet and LAN screens
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
namespace WONAPI {

LobbyContainer::LobbyContainer() {}
// The original stores its own vtable (0x7bdd7f) before releasing the three screens
// Destroys the container, releasing its three screens
LobbyContainer::~LobbyContainer() {}
// Sets the HTML default font, finds and adds the login, Internet and LAN screens and shows the login one
void LobbyContainer::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2) return;
    ComponentConfig* config=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(config,this->mLoginScreen,"LoginScreen",__FILE__,37);
    SafeGetComponent(config,this->mInternetScreen,"InternetScreen",__FILE__,38);
    SafeGetComponent(config,this->mLanScreen,"LanScreen",__FILE__,39);
    this->AddScreen(this->mLoginScreen.get());
    this->AddScreen(this->mInternetScreen.get());
    this->AddScreen(this->mLanScreen.get());
    this->ShowScreen(this->mLoginScreen.get(),true);
}
// Shows the Internet lobby screen
void LobbyContainer::ShowInternetScreen() { this->ShowScreen(this->mInternetScreen.get(),true); }
// Shows the LAN lobby screen
void LobbyContainer::ShowLanScreen() { this->ShowScreen(this->mLanScreen.get(),true); }
// mLoginScreen is read again after ShowScreen
// Shows the login screen with the message of the day
void LobbyContainer::ShowLoginScreen()
{
    this->ShowScreen(this->mLoginScreen.get(),true);
    this->mLoginScreen->ShowMOTD();
}
// Returns whether the Internet lobby screen is showing
bool LobbyContainer::OnInternetScreen() { return this->mCurScreen==this->mInternetScreen.get(); }
// Returns whether the LAN lobby screen is showing
bool LobbyContainer::OnLanScreen() { return this->mCurScreen==this->mLanScreen.get(); }
// Returns whether the login screen is showing
bool LobbyContainer::OnLoginScreen() { return this->mCurScreen==this->mLoginScreen.get(); }
// Returns where games are set up: 2 Internet, 1 LAN games tab, 3 direct connect tab, else 0
int LobbyContainer::GetGameScreenType()
{
    if(this->OnInternetScreen()) return 2;
    if(this->OnLanScreen()) {
        if(this->mLanScreen->OnLanTab()) return 1;
        if(this->mLanScreen->OnDirectConnectTab()) return 3;
    }
    return 0;
}
}
