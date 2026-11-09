// Lost-password page: pick or type a user name and ask the server to e-mail its password
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <map>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyControls.h"
namespace WONAPI {
LostPasswordCtrl::LostPasswordCtrl() {}
// The destructor is implicit. Does not call the base InitComponent
// FUNCTION: WON 0x007c0400
// Looks up the user name combo and retrieve button from the layout and listens for broadcasts
void LostPasswordCtrl::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2)
        return;
    ComponentConfig* config=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(config,this->mUserNameCombo,"UserNameCombo",__FILE__,27);
    SafeGetComponent(config,this->mRetrieveButton,"RetrieveButton",__FILE__,28);
    this->UpdateRetrieveButton();
    this->SubscribeToBroadcast(1);
}
// FUNCTION: WON 0x007c0460
// Enables the retrieve button once the user name has at least two characters
void LostPasswordCtrl::UpdateRetrieveButton()
{
    this->mRetrieveButton->SetEnabled(this->mUserNameCombo->mEdit->GetText().length()>=2);
}
// FUNCTION: WON 0x007c0490
// Fills the user name combo with the saved accounts
void LostPasswordCtrl::FillUserNames()
{
    this->mUserNameCombo->Clear(true);
    const LobbyUserInfoMap& users=LobbyFn35CE60();
    for(LobbyUserInfoMap::const_iterator i=users.begin();i!=users.end();++i)
        this->mUserNameCombo->AddString(i->first,-1);
}
// FUNCTION: WON 0x007c0520
// Updates the button on typing, requests the lost password on retrieve and refills the names on event 100052
void LostPasswordCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    if(inEvent->mType==ComponentEvent_InputTextChanged)
        this->UpdateRetrieveButton();
    else if(inEvent->mType==ComponentEvent_ButtonPressed && inEvent->mComponent==this->mRetrieveButton.get())
        this->DeliverEvent(new GetLostPasswordEvent(this->mUserNameCombo->mEdit->GetText()),false);
    else if(inEvent->mType==100052)
        this->FillUserNames();
    else
        Component::HandleComponentEvent(inEvent);
}
}
// GetLostPasswordEvent's destructor is implicit: 0x7c06c0 (no store of its own vtable 0x853a38), scalar deleting destructor 0x7c06a0


// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007c06c0
// (public: virtual __thiscall WONAPI::GetLostPasswordEvent::~GetLostPasswordEvent(void)    [??1GetLostPasswordEvent@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007c0340
// (public: virtual void * __thiscall WONAPI::LostPasswordCtrl::`scalar deleting destructor'(unsigned int)    [??_GLostPasswordCtrl@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007c0360
// (public: virtual __thiscall WONAPI::LostPasswordCtrl::~LostPasswordCtrl(void)    [??1LostPasswordCtrl@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007c06a0
// (public: virtual void * __thiscall WONAPI::GetLostPasswordEvent::`scalar deleting destructor'(unsigned int)    [??_GGetLostPasswordEvent@WONAPI@@UAEPAX)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007c0320
// (public: __thiscall WONAPI::LostPasswordCtrl::LostPasswordCtrl(void)    [??0LostPasswordCtrl@WONAPI@@QAE@XZ])
