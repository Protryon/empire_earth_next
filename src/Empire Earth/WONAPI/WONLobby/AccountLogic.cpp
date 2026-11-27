// Original filename: C:\EEX\Empire Earth\WONAPI\WONLobby\AccountLogic.cpp.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// AccountLogic: login, account creation and update, lost password/user name, credits and terms of use.
// A Rust object since R6 WONLobby round 8 (rust/crates/ee_won_lobby/src/logics.rs: the last user name, the terms-of-use
// prompt, every decision); the class is the RustLogic of kind Account (R6 WON round 36: its two dialogs in the
// shell's slots); this unit keeps the host's ops, each one the original's calls on a C++ object (the ops, the
// dialogs, the users, the events, the authentication context).
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "rust/won_net/status_codes.h"
#include "ee/wonapi/WONLobbyEventTypes.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/EELobbyGameCode.h" // declares LobbyMessageBox
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "rust/won_lobby/logics.h"
#include "rust/won_net/adminop.h"
#include "rust/won_net/reqdir.h"
using namespace WONAPI;
namespace WONAPI {
// the user's terms-of-use times set to the lobby's
static void UserSetTOUNow(LobbyUserInfo* inUser)
{
    long aSys, aGame;
    LobbyGetTOUTimes(aSys, aGame);
    LobbyUserSetTOU(inUser, aSys, aGame);
}

static ComponentEvent* E(intptr_t a) { return reinterpret_cast<ComponentEvent*>(a); }
static RustServerOp* R(intptr_t a) { return static_cast<RustServerOp*>(reinterpret_cast<AsyncOp*>(a)); }
static AuthContext* A(intptr_t a) { return reinterpret_cast<AuthContext*>(a); }
static ServerContext* SC(intptr_t a) { return reinterpret_cast<ServerContext*>(a); }
// a new op with one reference the Rust gives back (the original's SmartPtr local)
static intptr_t Held(AsyncOp* inOp)
{
    inOp->AddRef();
    return (intptr_t)inOp;
}

// The host: each case one statement; the decisions and the order are the Rust's (logics.rs, and netlogicops.rs
// for the cases R6 WON round 32 split).
static intptr_t AccountHost(void* inObj, EeWonLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonLogicOp O;
    typedef LobbyEvtField EF;
    RustLogic* aLogic = static_cast<RustLogic*>(inObj);
    switch (inOp) {
    case O::BroadcastLobby: LobbyBroadcastLobbyEvent((int)a, true); return 0;
    case O::Exit: LobbyExit((int)a); return 0;
    case O::AuthContext: return (intptr_t)GetLobbyAuthContext();
    case O::YesNoRes: return LobbyDoYesNoDialog(gLobbyResources[a].mString, gLobbyResources[b].mString, true);
    case O::MessageBoxRes: LobbyMessageBox(gLobbyResources[a].mString, gLobbyResources[b].mString); return 0;
    case O::CreditsLCall: aLogic->mDialog[kAccountCredits]->LCall((EeWonLobbyDlgCall)a); return 0;
    case O::CreditsDo: return LobbyDoDialog(aLogic->mDialog[kAccountCredits].get());
    case O::TouLCall: aLogic->mDialog[kAccountTOU]->LCall((EeWonLobbyDlgCall)a); return 0;
    case O::TouDo: return LobbyDoDialog(aLogic->mDialog[kAccountTOU].get());
    case O::ShowCreateAccount: LobbyShowCreateAccount(); return 0;
    case O::ShowLostPassword: LobbyShowLostPassword(); return 0;
    case O::ShowLostUsername: LobbyShowLostUsername(); return 0;
    case O::AccountServers: return (intptr_t)LobbyGetAccountServers();
    case O::RunWithStatusRes: LobbyRunOpWithStatus(reinterpret_cast<AsyncOp*>(a), gLobbyResources[b].mString, gLobbyResources[c].mString, (int)(d & 0xff), (unsigned long)(d >> 8)); return 0;
    case O::GetCertNew: return Held(RustServerOp::NewGetCert(A(a)));
    case O::GetAccountNew: return Held(RustServerOp::NewGetAccount(SC(a), A(b)));
    case O::PasswordEmailNew: return Held(RustServerOp::NewRequest(ee_won_reqop_password_email(reinterpret_cast<const uint8_t*>(a), (size_t)b), SC(c)));
    case O::InfoEmailNew: return Held(RustServerOp::NewRequest(ee_won_reqop_info_email(reinterpret_cast<const uint8_t*>(a), (size_t)b), SC(c)));
    case O::UpdateAccountNew: return Held(RustServerOp::NewUpdateAccount(SC(a), A(b), LobbyEvtText(E(c), EF::NewPassword), LobbyEvtText(E(c), EF::Email)));
    case O::CreateAccountNew: return Held(RustServerOp::NewCreateAccount(SC(a), LobbyEvtStr<std::wstring>(E(b), EF::UserName), LobbyEvtStr<std::wstring>(E(b), EF::Password), LobbyEvtStr<std::string>(E(b), EF::Email), LobbyEvtGet(E(b), EF::Newsletter) != 0, (unsigned char)LobbyEvtGet(E(b), EF::Month), (unsigned char)LobbyEvtGet(E(b), EF::Day), (unsigned short)LobbyEvtGet(E(b), EF::Year)));
    case O::BroadcastAccount:
        LobbyBroadcastEvent(QueryAccountResultEvent(R(a)->GetBytes(EE_WON_REQOP_ACCOUNT_EMAIL), R(a)->GetNum(EE_WON_REQOP_ACCOUNT_NEWSLETTER, 1) != 0), true);
        return 0;
    case O::AddUserNamed: return (intptr_t)LobbyAddUser(LobbyEvtStr<std::wstring>(E(a), EF::UserName), "", false);
    case O::AddUserNoPassword: return (intptr_t)LobbyAddUser(LobbyEvtStr<std::wstring>(E(a), EF::UserName), GUIString(GetEmptyGUIStringData()), false);
    case O::AddUserWithPassword: return (intptr_t)LobbyAddUser(LobbyEvtStr<std::wstring>(E(a), EF::UserName), GUIString(LobbyEvtStr<std::wstring>(E(a), EF::Password)), false);
    case O::UserSetTOUNow: UserSetTOUNow(reinterpret_cast<LobbyUserInfo*>(a)); return 0;
    case O::SaveGlobalPrefsNow: LobbySaveGlobalPrefs(true); return 0;
    case O::SetCurrentUser: LobbySetCurrentUser(reinterpret_cast<LobbyUserInfo*>(a)); return 0;
    case O::BroadcastLoginFields: LobbyBroadcastEvent(SetLoginFieldsEvent(GUIString(LobbyEvtStr<std::wstring>(E(a), EF::UserName)), GUIString(LobbyEvtStr<std::wstring>(E(a), EF::Password)), false), true); return 0;
    case O::RootWindow: return (intptr_t)LobbyGetRootWindow();
    case O::KeyMod: return reinterpret_cast<Window*>(a)->GetKeyMod();
    case O::FindUserOfEvent: return (intptr_t)LobbyFindUser(GUIString(LobbyEvtStr<std::wstring>(E(a), EF::UserName)));
    case O::UserTOUNewer: return LobbyIsTOUNewer(LobbyUserTOU(reinterpret_cast<LobbyUserInfo*>(a), 0), LobbyUserTOU(reinterpret_cast<LobbyUserInfo*>(a), 1));
    case O::AuthSetUserNameOf: A(a)->SetUserName(LobbyEvtStr<std::wstring>(E(b), EF::UserName)); return 0;
    case O::AuthSetPasswordOf: A(a)->SetPassword(LobbyEvtStr<std::wstring>(E(b), EF::Password)); return 0;
    case O::DoCDKeyCheck: return LobbyDoCDKeyCheck();
    case O::CDKeyValid: return LobbyIsCDKeyValid();
    case O::EnterInternet: LobbyEnterInternetScreen(a != 0); return 0;
    default: return 0;
    }
}

// The account logic's Rust object over the shell (its virtuals, its two dialogs' lookup: logicshell.rs)
void* AccountLogicMake(RustLogic* inSelf) { return ee_won_logic_new(EeWonLogicKind::Account, inSelf, AccountHost); }
}
