
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <string>
#include <list>
#include <map>
#include <vector>
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyControls.h"
namespace WONAPI {
// Login status line: shows the status text of the login progress events
LoginStatusCtrl::LoginStatusCtrl() {}
// The implicit destructor (0x7c0240) releases the label, then the Container base.
// Does not call the base InitComponent
// Looks up the status label from the layout and listens for broadcasts
void LoginStatusCtrl::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType != 2)
        return;
    ComponentConfig* config = static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(config, this->mStatusLabel, "StatusLabel", __FILE__, 26);
    this->SubscribeToBroadcast(1);
}
// Shows the status text of a login status event; the rest goes to Component
void LoginStatusCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    if (inEvent->mType == LobbyEvent_LoginInitStatus)
        this->mStatusLabel->SetText(static_cast<LoginInitStatusEvent*>(inEvent)->mStatus, true);
    else
        Component::HandleComponentEvent(inEvent);
}
}
