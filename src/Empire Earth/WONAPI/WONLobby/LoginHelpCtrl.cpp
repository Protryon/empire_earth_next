// Login help page: the text of help/loginhelp.html
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyScreens.h"
namespace WONAPI {
// The constructor (0x7bfc20) only initializes members; the destructor (0x7bfc60) is implicit
LoginHelpCtrl::LoginHelpCtrl() : mHaveLoadedHelp(false) {}
// Looks up the help browser from the layout
void LoginHelpCtrl::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType==2) SafeGetComponent(static_cast<ComponentConfigInit&>(inInit).mConfig,this->mBrowser,"Browser",__FILE__,40);
}
// Both std::string temporaries are destroyed before the flag store and Container::Paint
// Loads the login help page on first paint, then paints the page
void LoginHelpCtrl::Paint(Graphics& inGraphics)
{
    if(!this->mHaveLoadedHelp) {
        this->mBrowser->Go(ComponentConfig::GetResourceFile("help/loginhelp.html"),false,0,-1);
        this->mHaveLoadedHelp=true;
    }
    Container::Paint(inGraphics);
}
}
