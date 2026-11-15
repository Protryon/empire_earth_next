// Internet login screen: its right panel switches between the message of the day, create-account,
// lost user name, lost password and login help pages
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
namespace WONAPI {

LoginScreen::LoginScreen() {}
// Five pages are looked up first, the ScreenContainer panel last
// FUNCTION: WON 0x007c0060
// Looks up the right-panel pages from the layout and shows the message of the day
void LoginScreen::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2) return;
    ComponentConfig* config=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(config,this->mMOTDCtrl,"MOTDCtrl",__FILE__,26);
    SafeGetComponent(config,this->mCreateAccountCtrl,"CreateAccountCtrl",__FILE__,27);
    SafeGetComponent(config,this->mLostUserNameCtrl,"LostUserNameCtrl",__FILE__,28);
    SafeGetComponent(config,this->mLostPasswordCtrl,"LostPasswordCtrl",__FILE__,29);
    SafeGetComponent(config,this->mLoginHelpCtrl,"LoginHelpCtrl",__FILE__,30);
    SafeGetComponent(config,this->mRightPanel,"RightPanel",__FILE__,31);
    this->mRightPanel->ShowScreen(this->mMOTDCtrl.get(),false);
}
// Switches on the control id of the pressed button, not of this screen
// FUNCTION: WON 0x007c0130
// Shows login help or the message of the day for their buttons; the rest goes to Component
void LoginScreen::HandleComponentEvent(ComponentEvent* inEvent)
{
    if(inEvent->mType==ComponentEvent_ButtonPressed) {
        switch(inEvent->mComponent->mControlId) {
        case 200007: this->mRightPanel->ShowScreen(this->mLoginHelpCtrl.get(),true); return;
        case 200004: this->ShowMOTD(); return;
        }
    }
    Component::HandleComponentEvent(inEvent);
}
// FUNCTION: WON 0x007c0180
// Shows the message of the day in the right panel
void LoginScreen::ShowMOTD() { this->mRightPanel->ShowScreen(this->mMOTDCtrl.get(),true); }
// FUNCTION: WON 0x007c01a0
// Shows the create-account page in the right panel
void LoginScreen::ShowCreateAccountCtrl() { this->mRightPanel->ShowScreen(this->mCreateAccountCtrl.get(),true); }
// FUNCTION: WON 0x007c01c0
// Shows the lost-password page in the right panel
void LoginScreen::ShowLostPasswordCtrl() { this->mRightPanel->ShowScreen(this->mLostPasswordCtrl.get(),true); }
// FUNCTION: WON 0x007c01e0
// Shows the lost-user-name page in the right panel
void LoginScreen::ShowLostUsernameCtrl() { this->mRightPanel->ShowScreen(this->mLostUserNameCtrl.get(),true); }
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007bff30
// (public: virtual __thiscall WONAPI::LoginScreen::~LoginScreen(void)    [??1LoginScreen@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007ce1d0

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007bff10
// (public: virtual void * __thiscall WONAPI::LoginScreen::`scalar deleting destructor'(unsigned int)    [??_GLoginScreen@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007bfed0
// (public: __thiscall WONAPI::LoginScreen::LoginScreen(void)    [??0LoginScreen@WONAPI@@QAE@XZ])
