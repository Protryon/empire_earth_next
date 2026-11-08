// Original filename: C:\EEX\Empire Earth\WONAPI\WONLobby\AccountLogic.cpp.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// AccountLogic: login, account creation and update, lost password/user name, credits and terms of use.
// Known differences from the original: the constructor's cleanup action addresses the object at another frame
// offset, and the SafeGetComponent helpers differ in string growth and guards.
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyAccountLogic.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONLobbyEventTypes.h"
using namespace WONAPI;
namespace WONAPI {
// FUNCTION: WON 0x7db2a0
// Looks up the credits and terms-of-use dialogs from the lobby layout
void AccountLogic::InitComponent(ComponentConfig* inConfig)
{
    SafeGetComponent(inConfig, this->mCreditsDialog, "CreditsDialog", __FILE__, 43);
    SafeGetComponent(inConfig, this->mTOUDialog, "TOUDialog", __FILE__, 44);
}
// FUNCTION: WON 0x7db150
// Creates the account logic with no terms-of-use prompt pending
AccountLogic::AccountLogic() { this->mNeedTOU = false; }
// FUNCTION: WON 0x7db1d0
// Destroys the account logic (members only)
AccountLogic::~AccountLogic() {}
// FUNCTION: WON 0x7db2e0
// Shows the scrolling credits dialog
void AccountLogic::ShowCredits()
{
    this->mCreditsDialog->ResetScrolling();
    this->mCreditsDialog->StartScrolling();
    LobbyDoDialog(this->mCreditsDialog.get());
    this->mCreditsDialog->StopScrolling();
}
// FUNCTION: WON 0x7db310
// Shows the terms-of-use dialog; true if the player accepts
bool AccountLogic::ShowTOU()
{
    this->mTOUDialog->Reset();
    return LobbyDoDialog(this->mTOUDialog.get()) == 1;
}
// FUNCTION: WON 0x7db330
// Returns false when this game version is out of date, after offering to quit and update it
bool AccountLogic::CheckLoginStatus()
{
    if (LobbyGetVersionStatus() == WS_DBProxyServ_OutOfDate) {
        if (LobbyDoYesNoDialog(::LobbyContainer_VersionOutOfDataTitle_String, ::LobbyContainer_VersionOutOfDataDetails_String, true) == 100001)
            LobbyExit(3);
        return false;
    }
    return true;
}
// FUNCTION: WON 0x7dc2e0
// Opens the create-account (terms of use to follow), lost-password or lost-user-name screen if the version is usable
void AccountLogic::HandleControlEvent(int inControlId)
{
    switch (inControlId) {
    case ID_ShowCreateAccount:
        if (this->CheckLoginStatus()) {
            LobbyShowCreateAccount();
            this->mNeedTOU = true;
        }
        break;
    case ID_ShowLostPassword:
        if (this->CheckLoginStatus()) LobbyShowLostPassword();
        break;
    case ID_ShowLostUserName:
        if (this->CheckLoginStatus()) LobbyShowLostUsername();
        break;
    }
}
// FUNCTION: WON 0x7dc340
// Routes create-account, lost password/user name, login, account query and update events to the handlers
void AccountLogic::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch (inEvent->mType) {
    case LobbyEvent_CreateAccount: this->HandleCreateAccount(inEvent); break;
    case LobbyEvent_GetLostPassword: this->HandleLostPassword(inEvent); break;
    case LobbyEvent_GetLostUserName: this->HandleLostUsername(inEvent); break;
    case LobbyEvent_Login: this->HandleLogin(inEvent); break;
    case 100040: this->QueryAccount(); break;
    case LobbyEvent_UpdateAccount: this->HandleUpdateAccount(inEvent); break;
    }
}
}


#include "ee/wonapi/WONLobbyEvents.h"
namespace WONAPI {
// FUNCTION: WON 0x007db830
// Asks the account server to e-mail the password of the given user name, behind a status box
void AccountLogic::HandleLostPassword(ComponentEvent* inEvent)
{
    ServerContext* account = LobbyGetAccountServers();
    if (account == NULL || !this->CheckLoginStatus())
        return;
    SmartPtr<RequestPasswordEmailOp> op = new RequestPasswordEmailOp(account);
    op->SetUserName(static_cast<GetLostPasswordEvent*>(inEvent)->mUserName);
    LobbyRunOpWithStatus(op.get(), ::LobbyContainer_RetrievePasswordTitle_String, ::LobbyContainer_RetrievePasswordDetails_String, 0, 0);
}
// FUNCTION: WON 0x007db920
// Asks the account server to e-mail the user names registered to the given address, behind a status box
void AccountLogic::HandleLostUsername(ComponentEvent* inEvent)
{
    ServerContext* account = LobbyGetAccountServers();
    if (account == NULL || !this->CheckLoginStatus())
        return;
    SmartPtr<RequestAccountInfoEmailOp> op = new RequestAccountInfoEmailOp(account);
    op->SetEmail(static_cast<GetLostUserNameEvent*>(inEvent)->mEmail);
    LobbyRunOpWithStatus(op.get(), ::LobbyContainer_RetrieveUsernameTitle_String, ::LobbyContainer_RetrieveUsernameDetails_String, 0, 0);
}
// FUNCTION: WON 0x007dbee0
// Fetches our account's e-mail address and newsletter setting and broadcasts them
void AccountLogic::QueryAccount()
{
    ServerContext* account = LobbyGetAccountServers();
    AuthContext* auth = GetLobbyAuthContext();
    if (account == NULL || auth == NULL)
        return;
    SmartPtr<GetAccountOp> op = new GetAccountOp(account,auth);
    ProfileData* profile = op->mProfileData.get();
    profile->AddNewsletterSub(1,false);
    LobbyRunOpWithStatus(op.get(),::LobbyContainer_QueryAccountTitle_String,::LobbyContainer_QueryAccountDetails_String,1,0);
    if (op->GetStatus() == WS_Success)
        LobbyBroadcastEvent(new QueryAccountResultEvent(op->mEmail,
                                                  profile->GetNewsletterSub(1)),true);
}

// FUNCTION: WON 0x007dc160
// Sends our account's new e-mail address, password and newsletter setting, behind a status box
void AccountLogic::HandleUpdateAccount(ComponentEvent* inEvent)
{
    ServerContext* account = LobbyGetAccountServers();
    AuthContext* auth = GetLobbyAuthContext();
    if (account == NULL || auth == NULL)
        return;
    SmartPtr<UpdateAccountOp> op = new UpdateAccountOp(account,auth);
    UpdateAccountEvent* event = static_cast<UpdateAccountEvent*>(inEvent);
    op->SetEmail(event->mEmail);
    op->SetNewPassword(event->mNewPassword);
    op->mProfileData->AddNewsletterSub(1,event->mNewsletter);
    LobbyRunOpWithStatus(op.get(),::LobbyContainer_UpdateAccountTitle_String,::LobbyContainer_UpdateAccountDetails_String,0,0);
}

} //WONAPI

#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/EELobbyGameCode.h" // declares LobbyMessageBox
namespace WONAPI {
// FUNCTION: WON 0x007db370
// Creates an account (after the terms of use when needed); on success saves the user and fills in the login fields
void AccountLogic::HandleCreateAccount(ComponentEvent* inBaseEvent)
{
    if(!this->CheckLoginStatus()) return;
    if(this->mNeedTOU) {
        if(!this->ShowTOU()) return;
        this->mNeedTOU=false;
    }
    ServerContext* servers=LobbyGetAccountServers();
    if(!servers) return;
    SmartPtr<CreateAccountOp> op=new CreateAccountOp(servers);
    CreateAccountEvent* event=static_cast<CreateAccountEvent*>(inBaseEvent);
    op->SetUserName(event->mUserName);
    op->SetPassword(event->mPassword);
    op->SetEmail(event->mEmail);
    SmartPtr<ProfileData> profile=new ProfileData;
    profile->AddNewsletterSub(1,event->mNewsletter);
    profile->SetBirthDate(event->mMonth,event->mDay,event->mYear);
    op->SetProfileData(profile.get());
    LobbyRunOpWithStatus(op.get(),::LobbyContainer_CreateAccountTitle_String,::LobbyContainer_CreateAccountDetails_String,0,0);
    if(op->GetStatus()==WS_Success) {
        LobbyUserInfo* user=LobbyAddUser(event->mUserName,"",false);
        if(user) LobbyGetTOUTimes(user->mValue04,user->mValue08);
        LobbySaveGlobalPrefs(true);
        LobbyBroadcastLobbyEvent(100052,true);
        LobbyBroadcastEvent(new SetLoginFieldsEvent(GUIString(event->mUserName),
            GUIString(event->mPassword),false),true);
    }
}
}

namespace WONAPI {
// FUNCTION: WON 0x007dba10
// Logs in: three modifier keys show the credits, an '@' prefix asks for admin, new users get the terms of use;
// checks the CD key, then saves the user (password if asked) and enters the Internet screen
void AccountLogic::HandleLogin(ComponentEvent* inBaseEvent)
{
    AuthContext* auth=GetLobbyAuthContext();
    Window* window=LobbyGetRootWindow();
    if(!auth || !window) return;
    int keyMod=window->GetKeyMod();
    if((keyMod&7)==7) { this->ShowCredits();return; }
    LoginEvent* event=static_cast<LoginEvent*>(inBaseEvent);
    if(event->mUserName.empty() || !this->CheckLoginStatus()) return;
    bool admin=false;
    if(event->mUserName.at(0)==L'@') {
        admin=true;
        event->mUserName=event->mUserName.substr(1);
    }
    if(WStringCompareNoCase(event->mUserName,this->mLastUserName)!=0) {
        LobbyUserInfo* user;
        {
            GUIString name(new GUIStringData(event->mUserName));
            user=LobbyFindUser(name);
        }
        if((!user || LobbyIsTOUNewer(user->mValue04,user->mValue08)) && !this->ShowTOU()) return;
        this->mLastUserName=event->mUserName;
    }
    auth->SetUserName(event->mUserName);
    auth->SetPassword(event->mPassword);
    if(LobbyDoCDKeyCheck() && !LobbyIsCDKeyValid()) {
        LobbyMessageBox(::AccountLogic_InvalidCDKeyTitle_String,::AccountLogic_InvalidCDKeyDetails_String);
        return;
    }
    if(admin) auth->AddCommunity(std::wstring(L"Admin"));
    else auth->RemoveCommunity(std::wstring(L"Admin"));
    SmartPtr<GetCertOp> op=new GetCertOp(auth);
    LobbyRunOpWithStatus(op.get(),::LobbyContainer_LoginTitle_String,::LobbyContainer_LoginDetails_String,1,0);
    if(op->GetStatus()!=WS_Success) return;
    auth->SetDoAutoRefresh(true);
    GUIString password(GetEmptyGUIStringData());
    if(event->mRemember) password=event->mPassword;
    LobbyUserInfo* user=LobbyAddUser(event->mUserName,password,false);
    if(user) {
        LobbyGetTOUTimes(user->mValue04,user->mValue08);
        LobbySaveGlobalPrefs(true);
        LobbySetCurrentUser(user);
    }
    LobbyBroadcastLobbyEvent(100052,true);
    LobbyEnterInternetScreen((keyMod&4)==0);
}
}

// SafeGetComponent COMDATs this unit references first (placed after the last logic unit)
// TEMPLATE: WON 0x007f30d0
// TEMPLATE: WON 0x007f3250

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007db1b0
// (public: virtual void * __thiscall WONAPI::AccountLogic::`scalar deleting destructor'(unsigned int)    [??_GAccountLogic@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007db760
// (public: virtual void * __thiscall WONAPI::SetLoginFieldsEvent::`scalar deleting destructor'(unsigned int)    [??_GSetLoginFieldsEvent@WONAPI@@UAEPAXI@)
// TEMPLATE: WON 0x007db780
// (public: virtual __thiscall WONAPI::SetLoginFieldsEvent::~SetLoginFieldsEvent(void)    [??1SetLoginFieldsEvent@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007dc0b0
// (public: virtual void * __thiscall WONAPI::QueryAccountResultEvent::`scalar deleting destructor'(unsigned int)    [??_GQueryAccountResultEvent@WONAPI@@)
// TEMPLATE: WON 0x007dc0d0
// (public: virtual __thiscall WONAPI::QueryAccountResultEvent::~QueryAccountResultEvent(void)    [??1QueryAccountResultEvent@WONAPI@@UAE@XZ])
