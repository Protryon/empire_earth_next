// Lost-user-name page: enter an e-mail address and ask the server to send the account's user name
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
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyControls.h"
namespace WONAPI {
LostUserNameCtrl::LostUserNameCtrl() {}
// The destructor is implicit. Unlike LostPasswordCtrl it does not subscribe to broadcasts
// Looks up the e-mail field and retrieve button from the layout and updates the button
void LostUserNameCtrl::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2)
        return;
    ComponentConfig* config=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(config,this->mEmailInput,"EmailInput",__FILE__,27);
    SafeGetComponent(config,this->mRetrieveButton,"RetrieveButton",__FILE__,28);
    this->UpdateRetrieveButton();
}
// The std::string temporary lives until after SetEnabled
// Enables the retrieve button only while the e-mail address looks valid
void LostUserNameCtrl::UpdateRetrieveButton()
{
    this->mRetrieveButton->SetEnabled(LobbyIsValidEmail(this->mEmailInput->GetText()));
}
// Updates the button on typing and requests the lost user name for the e-mail address on retrieve
void LostUserNameCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    if(inEvent->mType==ComponentEvent_InputTextChanged)
        this->UpdateRetrieveButton();
    else if(inEvent->mType==ComponentEvent_ButtonPressed && inEvent->mComponent==this->mRetrieveButton.get())
        this->DeliverEvent(new GetLostUserNameEvent(this->mEmailInput->GetText()),false);
    else
        Component::HandleComponentEvent(inEvent);
}
}
// GetLostUserNameEvent's destructor is implicit: 0x7c0ab0 (no store of its own vtable 0x853b14), scalar deleting destructor 0x7c0a90
