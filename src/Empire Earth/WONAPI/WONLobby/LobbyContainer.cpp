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
#include "ee/wonapi/WONLobbyHTMLDisplayGen.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
namespace WONAPI {

LobbyContainer::LobbyContainer() {}
// The original stores its own vtable (0x7bdd7f) before releasing the three screens
// FUNCTION: WON 0x007bdd60
// Destroys the container, releasing its three screens
LobbyContainer::~LobbyContainer() {}
// FUNCTION: WON 0x007bde20
// Sets the HTML default font, finds and adds the login, Internet and LAN screens and shows the login one
void LobbyContainer::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2) return;
    ComponentConfig* config=static_cast<ComponentConfigInit&>(inInit).mConfig;
    HTMLDisplayGen::SetGlobalDefaultFontFace(this->GetDefaultFont()->mDescriptor.mFace);
    SafeGetComponent(config,this->mLoginScreen,"LoginScreen",__FILE__,37);
    SafeGetComponent(config,this->mInternetScreen,"InternetScreen",__FILE__,38);
    SafeGetComponent(config,this->mLanScreen,"LanScreen",__FILE__,39);
    this->AddScreen(this->mLoginScreen.get());
    this->AddScreen(this->mInternetScreen.get());
    this->AddScreen(this->mLanScreen.get());
    this->ShowScreen(this->mLoginScreen.get(),true);
}
// FUNCTION: WON 0x007bded0
// Shows the Internet lobby screen
void LobbyContainer::ShowInternetScreen() { this->ShowScreen(this->mInternetScreen.get(),true); }
// FUNCTION: WON 0x007bdee0
// Shows the LAN lobby screen
void LobbyContainer::ShowLanScreen() { this->ShowScreen(this->mLanScreen.get(),true); }
// mLoginScreen is read again after ShowScreen
// FUNCTION: WON 0x007bdef0
// Shows the login screen with the message of the day
void LobbyContainer::ShowLoginScreen()
{
    this->ShowScreen(this->mLoginScreen.get(),true);
    this->mLoginScreen->ShowMOTD();
}
// FUNCTION: WON 0x007bdf10
// Returns whether the Internet lobby screen is showing
bool LobbyContainer::OnInternetScreen() { return this->mCurScreen==this->mInternetScreen.get(); }
// FUNCTION: WON 0x007bdf30
// Returns whether the LAN lobby screen is showing
bool LobbyContainer::OnLanScreen() { return this->mCurScreen==this->mLanScreen.get(); }
// FUNCTION: WON 0x007bdf50
// Returns whether the login screen is showing
bool LobbyContainer::OnLoginScreen() { return this->mCurScreen==this->mLoginScreen.get(); }
// FUNCTION: WON 0x007bdf70
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

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007bdd10
// (public: __thiscall WONAPI::LobbyContainer::LobbyContainer(void)    [??0LobbyContainer@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x007bdd40
// (public: virtual void * __thiscall WONAPI::LobbyContainer::`scalar deleting destructor'(unsigned int)    [??_GLobbyContainer@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007cdea0
// TEMPLATE: WON 0x007ce020
