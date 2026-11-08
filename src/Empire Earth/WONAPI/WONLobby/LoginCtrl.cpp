// Login dialog: user name combo box, password field, remember-password check box and login button
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// Method names are inferred from behaviour (no strings or exports name them)
#include <string>
#include <list>
#include <map>
#include <vector>
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyControls.h"
namespace WONAPI {
// FUNCTION: WON 0x007bf190
// Creates the login dialog with login disabled and no controls looked up yet
LoginCtrl::LoginCtrl() : mCanLogin(false) {}
// The implicit destructor (0x7bf1f0) releases the four control pointers in reverse order, then the
// Dialog base (its member at +0x94, then the Container)
// FUNCTION: WON 0x007bf300
// Looks up the user name, password, remember-password and login controls from the layout and listens for broadcasts
void LoginCtrl::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType != 2)
        return;
    ComponentConfig* config = static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(config, this->mUserNameCombo, "UserNameCombo", __FILE__, 31);
    SafeGetComponent(config, this->mPasswordInput, "PasswordInput", __FILE__, 32);
    SafeGetComponent(config, this->mRememberPasswordCheck, "RememberPasswordCheck", __FILE__, 33);
    SafeGetComponent(config, this->mLoginButton, "LoginButton", __FILE__, 34);
    this->UpdateLoginButton();
    this->SubscribeToBroadcast(1);
}
// FUNCTION: WON 0x007bf390
// Enables the login button only while login is allowed and both user name and password are filled in
void LoginCtrl::UpdateLoginButton()
{
    this->mLoginButton->SetEnabled(this->mCanLogin && this->mUserNameCombo->mEdit->GetText().length() != 0 &&
        this->mPasswordInput->GetText().length() != 0);
}
// FUNCTION: WON 0x007bf400
// Fills the user name combo with the saved accounts and selects the most recently used one with its password
void LoginCtrl::FillUserNames()
{
    this->mUserNameCombo->Clear(true);
    const LobbyUserInfoMap& users=LobbyFn35CE60();
    long newest=0;
    ListItem* latest=NULL;
    LobbyUserInfoMap::const_iterator i=users.begin();
    while(i!=users.end()) {
        ListItem* item=this->mUserNameCombo->AddString(i->first,-1);
        if(i->second.mLastUseTime>newest) { newest=i->second.mLastUseTime;latest=item; }
        ++i;
    }
    if(latest!=NULL) {
        this->mUserNameCombo->SetSelItem(this->mUserNameCombo->mListArea->GetItemPos(latest),false);
        this->LoadPassword();
    }
}
// FUNCTION: WON 0x007bf4d0
// Fills in the saved password for the chosen user name; on a public machine clears it and hides the remember box
void LoginCtrl::LoadPassword()
{
    LobbyConfig* config = GetLobbyConfig();
    if (config != NULL && config->mPublic) {
        this->mPasswordInput->Clear(true);
        this->mRememberPasswordCheck->SetVisible(false);
        this->mRememberPasswordCheck->SetCheck(false,false);
        return;
    }
    GUIString password = LobbyGetUserPassword(this->mUserNameCombo->mEdit->GetText());
    this->mPasswordInput->SetText(password,true);
    this->mPasswordInput->SelectRange(0,-1);
    this->mRememberPasswordCheck->SetCheck(password.mData->mLength!=0,false);
    this->mRememberPasswordCheck->SetVisible(true);
}
// FUNCTION: WON 0x007bf5f0
// Stores the user name in the saved accounts, with its password only if remember-password is checked
void LoginCtrl::SavePassword()
{
    GUIString userName = this->mUserNameCombo->mEdit->GetText();
    GUIString password(GetEmptyGUIStringData());
    if (this->mRememberPasswordCheck->IsChecked())
        password=this->mPasswordInput->GetText();
    if (userName.empty())
        return;
    LobbyAddUser(userName,password,true);
}
// FUNCTION: WON 0x007bf760
// Fills in user name and password from the event, unchecks remember-password and refreshes the login button
void LoginCtrl::SetCredentials(SetLoginFieldsEvent* inEvent)
{
    this->mUserNameCombo->SetText(inEvent->mUserName,false);
    this->mPasswordInput->SetText(inEvent->mPassword,false);
    this->mRememberPasswordCheck->SetCheck(false,false);
    this->UpdateLoginButton();
}
// FUNCTION: WON 0x007bf7b0
// Shows the dialog with login disabled (kind 1), allows login (kind 2) or hides the dialog (kind 3)
void LoginCtrl::HandleLoginInitStatus(LoginInitStatusEvent* inEvent)
{
    if (inEvent->mKind==1) {
        this->mCanLogin=false;
        this->UpdateLoginButton();
        this->SetVisible(true);
    } else if (inEvent->mKind==2) {
        this->mCanLogin=true;
        this->UpdateLoginButton();
    } else if (inEvent->mKind==3)
        this->SetVisible(false);
}
// FUNCTION: WON 0x007bf810
// Handles typing, Enter, login and remember clicks, user picks and login status events; the rest goes to Dialog
void LoginCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch (inEvent->mType) {
    case ComponentEvent_InputTextChanged:
        if (inEvent->mComponent==this->mUserNameCombo.get() &&
            this->mUserNameCombo->mEdit->GetText()[0]=='@' &&
            this->mPasswordInput->GetText().length()!=0)
            this->mPasswordInput->Clear(false);
        this->UpdateLoginButton();
        return;
    case ComponentEvent_InputReturn:
        if (this->mLoginButton->IsDisabled()) {
            if (inEvent->mComponent==this->mUserNameCombo.get())
                this->mPasswordInput->RequestFocus();
            else if (inEvent->mComponent==this->mPasswordInput.get())
                this->mUserNameCombo->RequestFocus();
            return;
        }
        // Enter with the login button enabled falls through to the button-press checks
    case ComponentEvent_ButtonPressed:
        if (inEvent->mComponent==this->mLoginButton.get()) {
            this->DeliverEvent(new LoginEvent(this->mPasswordInput->GetText(),
                                  this->mRememberPasswordCheck->mChecked!=0,
                                  this->mUserNameCombo->mEdit.mPtr->GetText()),false);
            return;
        } else if (inEvent->mComponent==this->mRememberPasswordCheck.get()) {
            this->SavePassword();
            return;
        }
        break;
    case ComponentEvent_ComboSelChanged:
        if (inEvent->mComponent==this->mUserNameCombo.get()) this->LoadPassword();
        return;
    case LobbyEvent_LoginInitStatus:
        this->HandleLoginInitStatus(static_cast<LoginInitStatusEvent*>(inEvent));
        return;
    case LobbyEvent_SetLoginFields:
        this->SetCredentials(static_cast<SetLoginFieldsEvent*>(inEvent));
        return;
    case 100052:
        this->FillUserNames();
        return;
    }
    Dialog::HandleComponentEvent(inEvent);
}
}



// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007bfb50
// (public: virtual __thiscall WONAPI::LoginEvent::~LoginEvent(void)    [??1LoginEvent@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007bf1d0
// (public: virtual void * __thiscall WONAPI::LoginCtrl::`scalar deleting destructor'(unsigned int)    [??_GLoginCtrl@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007bf1f0
// (public: virtual __thiscall WONAPI::LoginCtrl::~LoginCtrl(void)    [??1LoginCtrl@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007bfb30
// (public: virtual void * __thiscall WONAPI::LoginEvent::`scalar deleting destructor'(unsigned int)    [??_GLoginEvent@WONAPI@@UAEPAXI@Z])
