// Original filename: C:\EEX\Empire Earth\WONAPI\WONLobby\AccountLogic.cpp.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// AccountLogic: login, account creation and update, lost password/user name, credits and terms of use.
// A Rust object since R6 WONLobby round 8 (rust/crates/ee_won_lobby/src/logics.rs: the last user name, the terms-of-use
// prompt, every decision); this unit keeps the shell (the dialogs' SmartPtrs) and the host's ops, each one the
// original's calls on a C++ object (the ops, the dialogs, the users, the events, the authentication context).
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
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/EELobbyGameCode.h" // declares LobbyMessageBox
#include "rust/won_lobby/logics.h"
#include "rust/won_net/adminop.h"
#include "rust/won_net/reqdir.h"
using namespace WONAPI;
namespace WONAPI {
static intptr_t AccountHost(void* inObj, EeWonLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t)
{
    typedef EeWonLogicOp O;
    typedef LobbyEvtField EF;
    AccountLogic* aLogic = static_cast<AccountLogic*>(inObj);
    switch (inOp) {
    case O::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case O::OpStatus: return (int)reinterpret_cast<AsyncOp*>(a)->GetStatus();
    case O::BroadcastLobby: LobbyBroadcastLobbyEvent((int)a, true); return 0;
    case O::Exit: LobbyExit((int)a); return 0;
    case O::AuthContext: return (intptr_t)GetLobbyAuthContext();
    case O::VersionStatus: return (int)LobbyGetVersionStatus();
    case O::OutOfDateDialog:
        return LobbyDoYesNoDialog(LOBBY_STRING(LobbyContainer_VersionOutOfDataTitle_String),
                                  LOBBY_STRING(LobbyContainer_VersionOutOfDataDetails_String), true);
    case O::ShowCredits:
        aLogic->mCreditsDialog->ResetScrolling();
        aLogic->mCreditsDialog->StartScrolling();
        LobbyDoDialog(aLogic->mCreditsDialog.get());
        aLogic->mCreditsDialog->StopScrolling();
        return 0;
    case O::ShowTOU:
        aLogic->mTOUDialog->Reset();
        return LobbyDoDialog(aLogic->mTOUDialog.get());
    case O::ShowScreen:
        if (a == 1) LobbyShowCreateAccount();
        else if (a == 2) LobbyShowLostPassword();
        else LobbyShowLostUsername();
        return 0;
    case O::AccountServers: return (intptr_t)LobbyGetAccountServers();
    case O::RequestPasswordEmail: {
        // RequestPasswordEmailOp: a Rust op (ee_won adminop.rs)
        std::string aName;
        WideAppendUtf16LE(aName, LobbyEvtStr<std::wstring>(reinterpret_cast<ComponentEvent*>(a), EF::UserName));
        SmartPtr<RustServerOp> op = RustServerOp::NewRequest(
            ee_won_reqop_password_email(reinterpret_cast<const uint8_t*>(aName.data()), aName.size()),
            reinterpret_cast<ServerContext*>(b));
        LobbyRunOpWithStatus(op.get(), LOBBY_STRING(LobbyContainer_RetrievePasswordTitle_String),
                             LOBBY_STRING(LobbyContainer_RetrievePasswordDetails_String), 0, 0);
        return 0;
    }
    case O::RequestInfoEmail: {
        // RequestAccountInfoEmailOp: a Rust op (ee_won adminop.rs)
        const std::string aEmail = LobbyEvtStr<std::string>(reinterpret_cast<ComponentEvent*>(a), EF::Email);
        SmartPtr<RustServerOp> op = RustServerOp::NewRequest(
            ee_won_reqop_info_email(reinterpret_cast<const uint8_t*>(aEmail.data()), aEmail.size()),
            reinterpret_cast<ServerContext*>(b));
        LobbyRunOpWithStatus(op.get(), LOBBY_STRING(LobbyContainer_RetrieveUsernameTitle_String),
                             LOBBY_STRING(LobbyContainer_RetrieveUsernameDetails_String), 0, 0);
        return 0;
    }
    case O::QueryAccount: {
        SmartPtr<RustServerOp> op = RustServerOp::NewGetAccount(reinterpret_cast<ServerContext*>(a),
                                                                reinterpret_cast<AuthContext*>(b));
        op->SetNum(EE_WON_REQOP_ACCOUNT_NEWSLETTER, 1);
        LobbyRunOpWithStatus(op.get(), LOBBY_STRING(LobbyContainer_QueryAccountTitle_String),
                             LOBBY_STRING(LobbyContainer_QueryAccountDetails_String), 1, 0);
        op->AddRef();
        return (intptr_t)static_cast<AsyncOp*>(op.get());
    }
    case O::BroadcastAccount: {
        RustServerOp* op = static_cast<RustServerOp*>(reinterpret_cast<AsyncOp*>(a));
        LobbyBroadcastEvent(QueryAccountResultEvent(op->GetBytes(EE_WON_REQOP_ACCOUNT_EMAIL),
                                                        op->GetNum(EE_WON_REQOP_ACCOUNT_NEWSLETTER, 1) != 0), true);
        return 0;
    }
    case O::UpdateAccount: {
        ComponentEvent* event = reinterpret_cast<ComponentEvent*>(c);
        SmartPtr<RustServerOp> op = RustServerOp::NewUpdateAccount(reinterpret_cast<ServerContext*>(a),
            reinterpret_cast<AuthContext*>(b), LobbyEvtText(event, EF::NewPassword), LobbyEvtText(event, EF::Email));
        op->SetNum(EE_WON_REQOP_ACCOUNT_NEWSLETTER, 1 | (LobbyEvtGet(event, EF::Newsletter) ? 0x10000 : 0));
        LobbyRunOpWithStatus(op.get(), LOBBY_STRING(LobbyContainer_UpdateAccountTitle_String),
                             LOBBY_STRING(LobbyContainer_UpdateAccountDetails_String), 0, 0);
        return 0;
    }
    case O::CreateAccount: {
        ComponentEvent* event = reinterpret_cast<ComponentEvent*>(b);
        SmartPtr<RustServerOp> op = RustServerOp::NewCreateAccount(reinterpret_cast<ServerContext*>(a),
            LobbyEvtStr<std::wstring>(event, EF::UserName), LobbyEvtStr<std::wstring>(event, EF::Password),
            LobbyEvtStr<std::string>(event, EF::Email), LobbyEvtGet(event, EF::Newsletter) != 0,
            (unsigned char)LobbyEvtGet(event, EF::Month), (unsigned char)LobbyEvtGet(event, EF::Day),
            (unsigned short)LobbyEvtGet(event, EF::Year));
        LobbyRunOpWithStatus(op.get(), LOBBY_STRING(LobbyContainer_CreateAccountTitle_String),
                             LOBBY_STRING(LobbyContainer_CreateAccountDetails_String), 0, 0);
        op->AddRef();
        return (intptr_t)static_cast<AsyncOp*>(op.get());
    }
    case O::AccountCreated: {
        ComponentEvent* event = reinterpret_cast<ComponentEvent*>(a);
        LobbyUserInfo* user = LobbyAddUser(LobbyEvtStr<std::wstring>(event, EF::UserName), "", false);
        if (user) {
            long aSys, aGame;
            LobbyGetTOUTimes(aSys, aGame);
            LobbyUserSetTOU(user, aSys, aGame);
        }
        LobbySaveGlobalPrefs(true);
        LobbyBroadcastLobbyEvent(100052, true);
        LobbyBroadcastEvent(SetLoginFieldsEvent(GUIString(LobbyEvtStr<std::wstring>(event, EF::UserName)),
                                                GUIString(LobbyEvtStr<std::wstring>(event, EF::Password)), false), true);
        return 0;
    }
    case O::RootWindow: return (intptr_t)LobbyGetRootWindow();
    case O::KeyMod: return reinterpret_cast<Window*>(a)->GetKeyMod();
    case O::LoginName: {
        // the event's own copy (lobbyevt.rs), live while the event is
        ComponentEvent* event = reinterpret_cast<ComponentEvent*>(a);
        *reinterpret_cast<size_t*>(b) = LobbyEvtStr<std::wstring>(event, EF::UserName).length();
        return LobbyEvtGet(event, EF::UserName);
    }
    case O::LoginStripAt: {
        ComponentEvent* event = reinterpret_cast<ComponentEvent*>(a);
        LobbyEvtSetStr(event, EF::UserName, LobbyEvtStr<std::wstring>(event, EF::UserName).substr(1));
        return 0;
    }
    case O::UserNeedsTOU: {
        LobbyUserInfo* user;
        {
            GUIString name(new GUIStringData(LobbyEvtStr<std::wstring>(reinterpret_cast<ComponentEvent*>(a), EF::UserName)));
            user = LobbyFindUser(name);
        }
        return !user || LobbyIsTOUNewer(LobbyUserTOU(user, 0), LobbyUserTOU(user, 1));
    }
    case O::SetCredentials: {
        AuthContext* auth = reinterpret_cast<AuthContext*>(a);
        auth->SetUserName(LobbyEvtStr<std::wstring>(reinterpret_cast<ComponentEvent*>(b), EF::UserName));
        auth->SetPassword(LobbyEvtStr<std::wstring>(reinterpret_cast<ComponentEvent*>(b), EF::Password));
        return 0;
    }
    case O::DoCDKeyCheck: return LobbyDoCDKeyCheck();
    case O::CDKeyValid: return LobbyIsCDKeyValid();
    case O::InvalidCDKeyBox:
        LobbyMessageBox(LOBBY_STRING(AccountLogic_InvalidCDKeyTitle_String), LOBBY_STRING(AccountLogic_InvalidCDKeyDetails_String));
        return 0;
    case O::SetAdmin:
        if (b) reinterpret_cast<AuthContext*>(a)->AddCommunity(std::wstring(L"Admin"));
        else reinterpret_cast<AuthContext*>(a)->RemoveCommunity(std::wstring(L"Admin"));
        return 0;
    case O::GetCert: {
        SmartPtr<GetCertOp> op = new GetCertOp(reinterpret_cast<AuthContext*>(a));
        LobbyRunOpWithStatus(op.get(), LOBBY_STRING(LobbyContainer_LoginTitle_String), LOBBY_STRING(LobbyContainer_LoginDetails_String), 1, 0);
        op->AddRef();
        return (intptr_t)static_cast<AsyncOp*>(op.get());
    }
    case O::AutoRefresh: reinterpret_cast<AuthContext*>(a)->SetDoAutoRefresh(true); return 0;
    case O::LoginSaveUser: {
        ComponentEvent* event = reinterpret_cast<ComponentEvent*>(a);
        GUIString password(GetEmptyGUIStringData());
        if (LobbyEvtGet(event, EF::Remember)) password = LobbyEvtStr<std::wstring>(event, EF::Password);
        LobbyUserInfo* user = LobbyAddUser(LobbyEvtStr<std::wstring>(event, EF::UserName), password, false);
        if (user) {
            long aSys, aGame;
            LobbyGetTOUTimes(aSys, aGame);
            LobbyUserSetTOU(user, aSys, aGame);
            LobbySaveGlobalPrefs(true);
            LobbySetCurrentUser(user);
        }
        return 0;
    }
    case O::EnterInternet: LobbyEnterInternetScreen(a != 0); return 0;
    default: return 0;
    }
}

AccountLogic::AccountLogic() : mRust(ee_won_logic_new(EeWonLogicKind::Account, this, AccountHost)) {}
AccountLogic::~AccountLogic() { ee_won_logic_free(this->mRust); }
// Looks up the credits and terms-of-use dialogs from the lobby layout
void AccountLogic::InitComponent(ComponentConfig* inConfig)
{
    SafeGetComponent(inConfig, this->mCreditsDialog, "CreditsDialog", __FILE__, 43);
    SafeGetComponent(inConfig, this->mTOUDialog, "TOUDialog", __FILE__, 44);
}
// Opens the create-account (terms of use to follow), lost-password or lost-user-name screen if the version is usable
void AccountLogic::HandleControlEvent(int inControlId)
{
    int aWhich = inControlId == ID_ShowCreateAccount ? 1 : inControlId == ID_ShowLostPassword ? 2
               : inControlId == ID_ShowLostUserName ? 3 : 0;
    if (aWhich != 0) ee_won_logic_call(this->mRust, EeWonLogicCall::AcctControl, aWhich, 0, 0);
}
// Routes create-account, lost password/user name, login, account query and update events to the handlers
void AccountLogic::HandleComponentEvent(ComponentEvent* inEvent)
{
    ee_won_logic_call(this->mRust, EeWonLogicCall::AcctEvent, (intptr_t)inEvent, inEvent->mType, 0);
}
}
