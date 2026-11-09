// Update-account dialog: change password, e-mail address and newsletter choice of the logged-in account
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
#include "ee/wonapi/WONLobbyScreens.h"
namespace WONAPI {

UpdateAccountCtrl::UpdateAccountCtrl() {}
// The original stores its own vptr (0x7c9d3f), so the destructor is written out
// FUNCTION: WON 0x007c9d20
// Destroys the dialog, releasing its controls and the remembered old password
UpdateAccountCtrl::~UpdateAccountCtrl() {}
// FUNCTION: WON 0x007c9ef0
// Looks up the account fields, error labels and buttons from the layout; the user name stays read-only
void UpdateAccountCtrl::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2) return;
    ComponentConfig* config=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(config,this->mUserNameInput,"UserNameInput",__FILE__,39);
    SafeGetComponent(config,this->mOldPasswordInput,"OldPasswordInput",__FILE__,40);
    SafeGetComponent(config,this->mPasswordInput,"PasswordInput",__FILE__,41);
    SafeGetComponent(config,this->mConfirmPasswordInput,"ConfirmPasswordInput",__FILE__,42);
    SafeGetComponent(config,this->mEmailInput,"EmailInput",__FILE__,43);
    SafeGetComponent(config,this->mNewsletterCheck,"NewsletterCheck",__FILE__,44);
    SafeGetComponent(config,this->mUserNameDesc,"UserNameDesc",__FILE__,45);
    SafeGetComponent(config,this->mOldPasswordDesc,"OldPasswordDesc",__FILE__,46);
    SafeGetComponent(config,this->mPasswordDesc,"PasswordDesc",__FILE__,47);
    SafeGetComponent(config,this->mConfirmPasswordDesc,"ConfirmPasswordDesc",__FILE__,48);
    SafeGetComponent(config,this->mEmailDesc,"EmailDesc",__FILE__,49);
    SafeGetComponent(config,this->mQueryButton,"QueryButton",__FILE__,50);
    SafeGetComponent(config,this->mUpdateButton,"UpdateButton",__FILE__,51);
    this->mUserNameInput->SetEnabled(false);
    this->CheckInput();
    this->SubscribeToBroadcast(1);
}
// Static (cdecl) in the original
// FUNCTION: WON 0x007ca080
// Shows or hides an error label and returns whether it is shown
bool UpdateAccountCtrl::SetDesc(bool inCondition,Label* inLabel)
{
    inLabel->SetVisible(inCondition);
    return inCondition;
}
// FUNCTION: WON 0x007ca0a0
// Flags a wrong old password, short or unconfirmed new password and bad e-mail; enables update and query
void UpdateAccountCtrl::CheckInput()
{
    bool error=false;
    bool queryAllowed=true;
    if(UpdateAccountCtrl::SetDesc(this->mOldPasswordInput->GetText()!=this->mActualOldPassword,this->mOldPasswordDesc.get())) {
        error=true;
        queryAllowed=false;
    }
    if(UpdateAccountCtrl::SetDesc(this->mPasswordInput->GetText().length()!=0 && this->mPasswordInput->GetText().length()<4,this->mPasswordDesc.get())) error=true;
    if(UpdateAccountCtrl::SetDesc(this->mPasswordInput->GetText()!=this->mConfirmPasswordInput->GetText(),this->mConfirmPasswordDesc.get())) error=true;
    if(UpdateAccountCtrl::SetDesc(this->mEmailInput->GetText().length()!=0 && !LobbyIsValidEmail(this->mEmailInput->GetText()),this->mEmailDesc.get())) error=true;
    this->mUpdateButton->SetEnabled(!error);
    this->mQueryButton->SetEnabled(queryAllowed);
}
// Reads the user name and password of the auth context at +0x50 and +0x60 (inferred)
// FUNCTION: WON 0x007ca250
// Fills in the logged-in user name, clears the other fields and remembers the login password to check against
void UpdateAccountCtrl::HandleEnteredScreen()
{
    AuthContext* context=GetLobbyAuthContext();
    if(context==NULL) return;
    this->mUserNameInput->SetText(GUIString(new GUIStringData(context->mName)),true);
    this->mOldPasswordInput->Clear(true);
    this->mPasswordInput->Clear(true);
    this->mConfirmPasswordInput->Clear(true);
    this->mEmailInput->Clear(true);
    this->mNewsletterCheck->SetCheck(false,false);
    this->mActualOldPassword=GUIString(new GUIStringData(context->mPassword));
}
// FUNCTION: WON 0x007ca3e0
// Shows the e-mail address and newsletter choice returned by the account query
void UpdateAccountCtrl::HandleQueryAccountResult(QueryAccountResultEvent* inEvent)
{
    this->mEmailInput->SetText(inEvent->mEmail,true);
    this->mNewsletterCheck->SetCheck(inEvent->mNewsletter,false);
}
// FUNCTION: WON 0x007ca410
// Revalidates on typing, queries the account or sends the update request, and refreshes on entering the screen
void UpdateAccountCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch(inEvent->mType) {
    case ComponentEvent_InputTextChanged: this->CheckInput(); return;
    case ComponentEvent_ButtonPressed:
        if(inEvent->mComponent==this->mQueryButton.get()) {
            this->Fn2F5380(100040,false);
            return;
        }
        if(inEvent->mComponent==this->mUpdateButton.get()) {
            this->DeliverEvent(new UpdateAccountEvent(this->mEmailInput->GetText(),this->mPasswordInput->GetText(),this->mNewsletterCheck->mChecked!=0),false);
        }
        return;
    case 100014: this->HandleEnteredScreen(); return;
    case LobbyEvent_QueryAccountResult: this->HandleQueryAccountResult(static_cast<QueryAccountResultEvent*>(inEvent)); return;
    default: Component::HandleComponentEvent(inEvent); return;
    }
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007c9d00
// (public: virtual void * __thiscall WONAPI::UpdateAccountCtrl::`scalar deleting destructor'(unsigned int)    [??_GUpdateAccountCtrl@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007ca5d0
// (public: virtual void * __thiscall WONAPI::UpdateAccountEvent::`scalar deleting destructor'(unsigned int)    [??_GUpdateAccountEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007ca5f0
// (public: virtual __thiscall WONAPI::UpdateAccountEvent::~UpdateAccountEvent(void)    [??1UpdateAccountEvent@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007c9c40
// (public: __thiscall WONAPI::UpdateAccountCtrl::UpdateAccountCtrl(void)    [??0UpdateAccountCtrl@WONAPI@@QAE@XZ])
