
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
// Lobby start-up failure panel: shows why connecting failed, with Retry and (when allowed) Ignore buttons
InitFailedCtrl::InitFailedCtrl() {}
// The implicit destructor (0x7bdaf0) releases the three controls in reverse order, then the Container
// Looks up the retry and ignore buttons and the description label, starts hidden and listens for broadcasts
void InitFailedCtrl::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType != 2)
        return;
    ComponentConfig* config = static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(config, this->mRetryButton, "RetryButton", __FILE__, 26);
    SafeGetComponent(config, this->mIgnoreButton, "IgnoreButton", __FILE__, 27);
    SafeGetComponent(config, this->mDescLabel, "DescLabel", __FILE__, 28);
    this->SetVisible(false);
    this->SubscribeToBroadcast(1);
}
// Hides the panel when start-up begins (kind 1); on failure (kind 3) shows the reason and Ignore if allowed
void InitFailedCtrl::HandleLoginInitStatus(LoginInitStatusEvent* inEvent)
{
    if (inEvent->mKind == 1)
        this->SetVisible(false);
    else if (inEvent->mKind == 3) {
        this->mDescLabel->SetText(inEvent->mDescription, true);
        this->mIgnoreButton->SetVisible(inEvent->mAllowIgnore);
        this->EnableLayoutInstructions(1, inEvent->mAllowIgnore != false);
        this->SetVisible(true);
        this->Relayout();
    }
}
// Handles start-up status events; Retry restarts start-up and Ignore restarts it ignoring the current errors
void InitFailedCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    if (inEvent->mType == LobbyEvent_LoginInitStatus) {
        this->HandleLoginInitStatus(static_cast<LoginInitStatusEvent*>(inEvent));
        return;
    }
    if (inEvent->mType == ComponentEvent_ButtonPressed) {
        Component* component = inEvent->mComponent;
        if (component == this->mRetryButton.get()) {
            this->Fn2F5380(100045, 0);
            return;
        }
        if (component == this->mIgnoreButton.get()) {
            this->Fn2F5380(100025, 0);
            return;
        }
    }
    Component::HandleComponentEvent(inEvent);
}
}
