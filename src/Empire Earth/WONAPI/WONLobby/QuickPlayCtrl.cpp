
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
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyScreens.h"
namespace WONAPI {

QuickPlayCtrl::QuickPlayCtrl() {}
// FUNCTION: WON 0x007c1690
// Finds the welcome label, the four skill radios and the play button in the layout
void QuickPlayCtrl::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2)
        return;
    ComponentConfig* config=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(config,this->mWelcomeLabel,"WelcomeLabel",__FILE__,29);
    SafeGetComponent(config,this->mNoviceRadio,"NoviceRadio",__FILE__,30);
    SafeGetComponent(config,this->mIntermediateRadio,"IntermediateRadio",__FILE__,31);
    SafeGetComponent(config,this->mAdvancedRadio,"AdvancedRadio",__FILE__,32);
    SafeGetComponent(config,this->mExpertRadio,"ExpertRadio",__FILE__,33);
    SafeGetComponent(config,this->mPlayButton,"PlayButton",__FILE__,34);
    this->SubscribeToBroadcast(1);
}
// FUNCTION: WON 0x007c1750
// Welcomes the logged-in user by name, or shows the plain welcome text
void QuickPlayCtrl::RefreshUserName()
{
    LobbyUserInfo* user=LobbyGetCurrentUser();
    if(user!=NULL)
        this->mWelcomeLabel->SetText(LobbyFormatString(::InternetQuickPlay_UserWelcome_String,user->mUserName),true);
    else
        this->mWelcomeLabel->SetText(::InternetQuickPlay_Welcome_String,true);
}
// FUNCTION: WON 0x007c1800
// Refreshes the welcome on entering the screen; Play sends a quick-play request for the chosen skill (1-4)
void QuickPlayCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch(inEvent->mType) {
    case 100014:
        this->RefreshUserName();
        return;
    case ComponentEvent_ButtonPressed:
      if(inEvent->mComponent==this->mPlayButton.get()) {
        int gameType=0;
        if(this->mNoviceRadio->IsChecked()) gameType=1;
        else if(this->mIntermediateRadio->IsChecked()) gameType=2;
        else if(this->mAdvancedRadio->IsChecked()) gameType=3;
        else if(this->mExpertRadio->IsChecked()) gameType=4;
        this->DeliverEvent(new QuickPlayEvent(gameType),false);
      }
      break;
    }
    Dialog::HandleComponentEvent(inEvent);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007c1520
// (public: virtual void * __thiscall WONAPI::QuickPlayCtrl::`scalar deleting destructor'(unsigned int)    [??_GQuickPlayCtrl@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007c1540
// (public: virtual __thiscall WONAPI::QuickPlayCtrl::~QuickPlayCtrl(void)    [??1QuickPlayCtrl@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007c1920
// (public: virtual void * __thiscall WONAPI::QuickPlayEvent::`scalar deleting destructor'(unsigned int)    [??_GQuickPlayEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007c1940
// (public: virtual __thiscall WONAPI::QuickPlayEvent::~QuickPlayEvent(void)    [??1QuickPlayEvent@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007c14e0
// (public: __thiscall WONAPI::QuickPlayCtrl::QuickPlayCtrl(void)    [??0QuickPlayCtrl@WONAPI@@QAE@XZ])
