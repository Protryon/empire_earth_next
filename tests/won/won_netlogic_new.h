// The Internet logics' hosts' one-statement cases the moved cases became (R6 WON round 32), as they are (copied
// by script from AccountLogic.cpp and InitLogic.cpp, with their helpers), for won_lobbyops_test: the Rust's calls
// out. InitHost's config is the world's, as in won_netlogic_orig.h.
#ifndef WON_NETLOGIC_NEW_H
#define WON_NETLOGIC_NEW_H
#include "rust/won_lobby/lobbydlg.h"
#include "rust/won_lobby/logics.h"
#include "rust/won_lobby/routelogic.h"
#include "rust/won_net/adminop.h"
#include "rust/won_net/hostop.h"
#include "rust/won_net/httpcache.h"
#include "rust/won_net/httpdoc.h"
#include "rust/won_net/httpget.h"
#include "rust/won_net/reqdir.h"
#include "rust/won_net/reqops.h"
#include "rust/won_net/routecpp.h"
#include "rust/won_net/routeop.h"
namespace NlNew {
using namespace LoWorld;
using namespace NlWorld;
static void UserSetTOUNow(LobbyUserInfo* inUser)
{
    long aSys, aGame;
    LobbyGetTOUTimes(aSys, aGame);
    LobbyUserSetTOU(inUser, aSys, aGame);
}
static ComponentEvent* E(intptr_t a) { return reinterpret_cast<ComponentEvent*>(a); }
static AuthContext* A(intptr_t a) { return reinterpret_cast<AuthContext*>(a); }
static const std::wstring* InitDirPaths(LobbyConfig* inConfig)
{
    static std::wstring sPaths[4] = { L"/TitanServers/Auth", L"/TitanServers/DBProxy", L"/TitanServers/Firewall" };
    sPaths[3] = inConfig != NULL ? inConfig->mProductDirectory + L"/Patch" : std::wstring();
    return sPaths;
}
static RustServerOp* R(intptr_t a) { return static_cast<RustServerOp*>(reinterpret_cast<AsyncOp*>(a)); }
static GUIString* T(intptr_t a) { return reinterpret_cast<GUIString*>(a); }
static ServerContext* SC(intptr_t a) { return reinterpret_cast<ServerContext*>(a); }
static DetectFirewallOp* FW(intptr_t a) { return static_cast<DetectFirewallOp*>(reinterpret_cast<AsyncOp*>(a)); }
static const uint8_t* B(const std::string& inText) { return reinterpret_cast<const uint8_t*>(inText.data()); }
static intptr_t Held(AsyncOp* inOp)
{
    inOp->AddRef();
    return (intptr_t)inOp;
}
template <class X> static intptr_t Held(const SmartPtr<X>& inPtr)
{
    if (inPtr.get() != NULL) inPtr->AddRef();
    return (intptr_t)inPtr.get();
}
static intptr_t Account(void* inObj, EeWonLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonLogicOp O;
    typedef LobbyEvtField EF;
    AccountLogic* aLogic = static_cast<AccountLogic*>(inObj);
    (void)aLogic;
    switch (inOp) {
    case O::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case O::BroadcastLobby: LobbyBroadcastLobbyEvent((int)a, true); return 0;
    case O::YesNoRes: return LobbyDoYesNoDialog(gLobbyResources[a].mString, gLobbyResources[b].mString, true);
    case O::MessageBoxRes: LobbyMessageBox(gLobbyResources[a].mString, gLobbyResources[b].mString); return 0;
    case O::CreditsLCall: aLogic->mCreditsDialog->LCall((EeWonLobbyDlgCall)a); return 0;
    case O::CreditsDo: return LobbyDoDialog(aLogic->mCreditsDialog.get());
    case O::TouLCall: aLogic->mTOUDialog->LCall((EeWonLobbyDlgCall)a); return 0;
    case O::TouDo: return LobbyDoDialog(aLogic->mTOUDialog.get());
    case O::ShowCreateAccount: LobbyShowCreateAccount(); return 0;
    case O::ShowLostPassword: LobbyShowLostPassword(); return 0;
    case O::ShowLostUsername: LobbyShowLostUsername(); return 0;
    case O::RunWithStatusRes: LobbyRunOpWithStatus(reinterpret_cast<AsyncOp*>(a), gLobbyResources[b].mString, gLobbyResources[c].mString, (int)(d & 0xff), (unsigned long)(d >> 8)); return 0;
    case O::OpSetNum: R(a)->SetNum((int)b, (long long)c); return 0;
    case O::GetCertNew: return Held(new GetCertOp(A(a)));
    case O::GetAccountNew: return Held(RustServerOp::NewGetAccount(SC(a), A(b)));
    case O::PasswordEmailNew: return Held(RustServerOp::NewRequest(ee_won_reqop_password_email(reinterpret_cast<const uint8_t*>(a), (size_t)b), SC(c)));
    case O::InfoEmailNew: return Held(RustServerOp::NewRequest(ee_won_reqop_info_email(reinterpret_cast<const uint8_t*>(a), (size_t)b), SC(c)));
    case O::UpdateAccountNew: return Held(RustServerOp::NewUpdateAccount(SC(a), A(b), LobbyEvtText(E(c), EF::NewPassword), LobbyEvtText(E(c), EF::Email)));
    case O::CreateAccountNew: return Held(RustServerOp::NewCreateAccount(SC(a), LobbyEvtStr<std::wstring>(E(b), EF::UserName), LobbyEvtStr<std::wstring>(E(b), EF::Password), LobbyEvtStr<std::string>(E(b), EF::Email), LobbyEvtGet(E(b), EF::Newsletter) != 0, (unsigned char)LobbyEvtGet(E(b), EF::Month), (unsigned char)LobbyEvtGet(E(b), EF::Day), (unsigned short)LobbyEvtGet(E(b), EF::Year)));
    case O::AddUserNamed: return (intptr_t)LobbyAddUser(LobbyEvtStr<std::wstring>(E(a), EF::UserName), "", false);
    case O::AddUserNoPassword: return (intptr_t)LobbyAddUser(LobbyEvtStr<std::wstring>(E(a), EF::UserName), GUIString(GetEmptyGUIStringData()), false);
    case O::AddUserWithPassword: return (intptr_t)LobbyAddUser(LobbyEvtStr<std::wstring>(E(a), EF::UserName), GUIString(LobbyEvtStr<std::wstring>(E(a), EF::Password)), false);
    case O::UserSetTOUNow: UserSetTOUNow(reinterpret_cast<LobbyUserInfo*>(a)); return 0;
    case O::SaveGlobalPrefsNow: LobbySaveGlobalPrefs(true); return 0;
    case O::SetCurrentUser: LobbySetCurrentUser(reinterpret_cast<LobbyUserInfo*>(a)); return 0;
    case O::BroadcastLoginFields: LobbyBroadcastEvent(SetLoginFieldsEvent(GUIString(LobbyEvtStr<std::wstring>(E(a), EF::UserName)), GUIString(LobbyEvtStr<std::wstring>(E(a), EF::Password)), false), true); return 0;
    case O::FindUserOfEvent: return (intptr_t)LobbyFindUser(GUIString(new GUIStringData(LobbyEvtStr<std::wstring>(E(a), EF::UserName))));
    case O::UserTOUNewer: return LobbyIsTOUNewer(LobbyUserTOU(reinterpret_cast<LobbyUserInfo*>(a), 0), LobbyUserTOU(reinterpret_cast<LobbyUserInfo*>(a), 1));
    case O::AuthSetUserNameOf: A(a)->SetUserName(LobbyEvtStr<std::wstring>(E(b), EF::UserName)); return 0;
    case O::AuthSetPasswordOf: A(a)->SetPassword(LobbyEvtStr<std::wstring>(E(b), EF::Password)); return 0;
    case O::AuthAddAdmin: A(a)->AddCommunity(std::wstring(L"Admin")); return 0;
    case O::AuthRemoveAdmin: A(a)->RemoveCommunity(std::wstring(L"Admin")); return 0;
    default: Log("unexpected op " + N((long long)inOp)); return 0;
    }
}
static intptr_t Init(void* inObj, EeWonLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonLogicOp O;
    InitLogic* aLogic = static_cast<InitLogic*>(inObj);
    LobbyConfig* config = sConfig;
    (void)aLogic; (void)config;
    switch (inOp) {
    case O::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case O::InitStatusText: LobbyBroadcastEvent(LoginInitStatusEvent(gLobbyResources[a].mString, *T(b), (d & 1) != 0, (int)(d >> 8)), true); return 0;
    case O::InitStatusRes: LobbyBroadcastEvent(LoginInitStatusEvent(gLobbyResources[a].mString, gLobbyResources[c].mString, (d & 1) != 0, (int)(d >> 8)), true); return 0;
    case O::InitStatusEmpty: LobbyBroadcastEvent(LoginInitStatusEvent(gLobbyResources[a].mString, gEmptyGUIString, (d & 1) != 0, (int)(d >> 8)), true); return 0;
    case O::HasConfig: return config != NULL;
    case O::InitListGet: return (intptr_t)InitList(a);
    case O::ServerClear: SC(a)->Clear(); return 0;
    case O::SysTouModifyTime: return (intptr_t)WONFile(LobbyGetSysTOUPath()).GetModifyTime();
    case O::GameTouModifyTime: return (intptr_t)WONFile(LobbyGetGameTOUPath()).GetModifyTime();
    case O::SetTOUTimes: LobbySetTOUTimes((long)a, (long)b); return 0;
    case O::MusicSetLooping: LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->SetLooping(true); return 0;
    case O::MusicPlay: LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Play(); return 0;
    case O::MusicStop: LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Stop(); return 0;
    case O::RoomDirServersGet: return (intptr_t)LobbyGetRoomDirServers();
    case O::RememberHostAddress: IPAddr().SetRememberHostString(true); return 0;
    case O::DirServersRemoveAll: LobbyGetDirServers()->RemoveAllAddresses(); return 0;
    case O::AddAllDir: InitAddAll(LobbyGetDirServers(), config->mDirServers); return 0;
    case O::AddAllRoomDir: InitAddAll(LobbyGetRoomDirServers(), config->mRoomDirServers); return 0;
    case O::AddAllAuth: InitAddAll(LobbyGetAuthServers(), config->mAuthServers); return 0;
    case O::PromotionDecode: return Held(Window::GetDefaultWindow()->DecodeImage(R(a)->GetBytes(EE_WON_HTTPGET_LOCAL_PATH).c_str()));
    case O::PromotionBroadcast: LobbyBroadcastEvent(SetCrossPromotionEvent(reinterpret_cast<NativeImage*>(a), aLogic->mCrossPromotion), true); return 0;
    case O::HttpGetNew: return Held(RustServerOp::NewHttpGet(std::string(reinterpret_cast<const char*>(a), (size_t)b)));
    case O::HttpGetSetCache: ee_won_hostop_set(R(a)->GetRustOp(), EE_WON_HTTPGET_CACHE, 0, NULL, 0, NULL, ee_won_http_cache_global(B(WONAPICore::GetDefaultFileDirectory()), WONAPICore::GetDefaultFileDirectory().size())); return 0;
    case O::HttpGetSetPromotionCompletion: R(a)->SetCompletion(new LogicCompletion<InitLogic>(&InitLogic::CrossPromotionImageCompletion, aLogic)); return 0;
    case O::SessionAddOp: aLogic->mHTTPSession->AddOp(R(a)); return 0;
    case O::ServerAddFromDir: SC(a)->AddAddressesFromDir(R(b)->GetEntities(EE_WON_REQOP_DIR_LIST, c), NULL); return 0;
    case O::ServerNotifyFailed: SC(a)->NotifyFailed(R(b)->GetCurAddr()); return 0;
    case O::ClearAcceptOp: aLogic->mAcceptOp = NULL; return 0;
    case O::AcceptOpNewFrom: aLogic->mAcceptOp = new SocketOp(EeWonCoreSocketOp::Accept, static_cast<SocketOp*>(reinterpret_cast<AsyncOp*>(a))->GetSocket()); return 0;
    case O::AcceptOpNewFirewall: aLogic->mAcceptOp = new SocketOp(EeWonCoreSocketOp::Accept, aLogic->mFirewallSocket.get()); return 0;
    case O::AcceptOpRun: aLogic->RunOp(aLogic->mAcceptOp.get(), new LogicCompletion<InitLogic>(&InitLogic::AcceptCompletion, aLogic), gLobbyAcceptTimeout); return 0;
    case O::SetFirewallAddress: aLogic->mFirewallAddress = FW(a)->GetCurAddr(); return 0;
    case O::DnsOpNew: return Held(new DNSTestOp(config->mDirServers.front()));
    case O::RunTrackedDns: aLogic->RunTrackedOp(reinterpret_cast<AsyncOp*>(a), new LogicCompletion<InitLogic>(&InitLogic::DNSCompletion, aLogic), 0); return 0;
    case O::RunTrackedDocument: aLogic->RunTrackedOp(reinterpret_cast<AsyncOp*>(a), new LogicCompletion<InitLogic>(&InitLogic::HTTPDocumentCompletion, aLogic), 0); return 0;
    case O::RunTrackedDirectory: aLogic->RunTrackedOp(reinterpret_cast<AsyncOp*>(a), new LogicCompletion<InitLogic>(&InitLogic::DirectoryCompletion, aLogic), 0); return 0;
    case O::RunTrackedVersion: aLogic->RunTrackedOp(reinterpret_cast<AsyncOp*>(a), new LogicCompletion<InitLogic>(&InitLogic::VersionCompletion, aLogic), 0); return 0;
    case O::RunTrackedFirewall: aLogic->RunTrackedOp(reinterpret_cast<AsyncOp*>(a), new LogicCompletion<InitLogic>(&InitLogic::FirewallCompletion, aLogic), 0); return 0;
    case O::HttpDocNew: return Held(RustServerOp::NewHttpDocument(config->mProductName, (int)a));
    case O::HttpDocSetConfig: R(a)->SetBytes(EE_WON_HTTPDOC_CONFIG, config->mLanguage); return 0;
    case O::HttpDocSetSession: R(a)->Set(EE_WON_HTTPDOC_SESSION, aLogic->mHTTPSession.get()); return 0;
    case O::DirServers: return (intptr_t)LobbyGetDirServers();
    case O::DirQueryNew: return Held(RustServerOp::NewGetMultiDir(SC(a), 0x04000002, InitDirPaths(config), (int)b));
    case O::VersionCheckNew: return Held(RustServerOp::NewRequest(ee_won_reqop_version_check(B(config->mProductName), config->mProductName.size(), B(config->mLanguage), config->mLanguage.size(), B(config->mVersion), config->mVersion.size()), LobbyGetPatchServers()));
    case O::FirewallOpNew: return Held(new DetectFirewallOp(SC(a)));
    case O::FirewallSetListenPort: FW(a)->SetListenPort(LobbyGetLobbyPort()); return 0;
    case O::FirewallSetDoListen: FW(a)->SetDoListen(b != 0); return 0;
    case O::FirewallSetWaitForConnect: FW(a)->SetWaitForConnect(b != 0); return 0;
    case O::FirewallSocketDrop: aLogic->mFirewallSocket = NULL; return 0;
    case O::HasAcceptOp: return aLogic->mAcceptOp.get() != NULL;
    case O::AcceptOpKill: aLogic->mAcceptOp->Kill(true); return 0;
    case O::AcceptOpHasSocket: return aLogic->mAcceptOp->GetSocket() != NULL;
    case O::AcceptOpCloseSocket: aLogic->mAcceptOp->GetSocket()->Close(); return 0;
    case O::AdapterReset: aLogic->mNetworkAdapterDialog->LCall(EeWonLobbyDlgCall::Reset); return 0;
    case O::AdapterDo: return LobbyDoDialog(aLogic->mNetworkAdapterDialog.get());
    default: Log("unexpected op " + N((long long)inOp)); return 0;
    }
}
namespace Route {
static const GUIString& TX(intptr_t a) { return a != 0 ? *T(a) : gEmptyGUIString; }
static LobbyServer* S(intptr_t a) { return reinterpret_cast<LobbyServer*>(a); }
static LobbyGroup* G(intptr_t a) { return reinterpret_cast<LobbyGroup*>(a); }
static LobbyClient* C(intptr_t a) { return reinterpret_cast<LobbyClient*>(a); }
static LobbyGame* GM(intptr_t a) { return reinterpret_cast<LobbyGame*>(a); }
static RoutingOp* RO(intptr_t a) { return reinterpret_cast<RoutingOp*>(a); }
static RoutingConnection* RC(intptr_t a) { return reinterpret_cast<RoutingConnection*>(a); }
static AsyncOp* AO(intptr_t a) { return reinterpret_cast<AsyncOp*>(a); }
static RustServerOp* RS(intptr_t a) { return static_cast<RustServerOp*>(AO(a)); }
static RoutingGetUserStatusOp* US(intptr_t a) { return static_cast<RoutingGetUserStatusOp*>(AO(a)); }
static ComponentEvent* E(intptr_t a) { return reinterpret_cast<ComponentEvent*>(a); }
static const char* const kDirTypes[] = {"_C", "_L"}; // the room directory's entity types
static intptr_t HeldOp(AsyncOp* inOp)
{
    inOp->AddRef();
    return (intptr_t)inOp;
}
static intptr_t Route(void* inObj, EeWonRouteLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e)
{
    typedef EeWonRouteLogicOp O;
    RoutingLogic* L = static_cast<RoutingLogic*>(inObj);
    switch (inOp) {
    // the model's answers in the game (netops.rs: the references and statuses; the text's units), the world's
    case O::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case O::OpStatus: return (int)AO(a)->GetStatus();
    case O::TextUnits:
        if (b != 0)
            for (size_t i = 0; i < TX(a).s.size(); i++) reinterpret_cast<uint32_t*>(b)[i] = (uint32_t)(unsigned char)TX(a).s[i];
        return (intptr_t)TX(a).s.size();
    // the host's, as they are
    case O::RunWithStatus:
        LobbyRunOpWithStatus(AO(a), gLobbyResources[b].mString, gLobbyResources[c].mString, (int)(d & 0xff),
                             (unsigned long)(d >> 8));
        return 0;
    case O::RNew: return HeldOp(new RoutingOp(RC(a), (RoutingOpType)b));
    case O::ConnSetAuth: RC(a)->SetAuth(reinterpret_cast<AuthContext*>(b), AUTH_TYPE_PERSISTENT); return 0;
    case O::ConnSetNotice: RC(a)->SetOpCompletion((int)b, new ::RoutingOpCompletion((int)c, L)); return 0;
    case O::ConnSetClose: RC(a)->SetCloseCompletion(new LogicCompletion<RoutingLogic>(&RoutingLogic::ConnectionClosed, L)); return 0;
    case O::ConnSubscribeGames: RoutingRequestValues().N(1).N(0xffff).S(L->mGameDataType).N(false).Run(RC(a), EE_WON_RTQ_READ_DATA, new ::RoutingOpCompletion(21, L)); return 0;
    case O::ConnIsConnected: return RC(a)->IsConnected();
    case O::UserIdOpNew: return HeldOp(RustServerOp::NewRequest(ee_won_reqop_user_id(reinterpret_cast<const uint8_t*>(a), (size_t)b), reinterpret_cast<ServerContext*>(c)));
    case O::OpUserId: return (intptr_t)(unsigned long)ee_won_hostop_num(RS(a)->GetRustOp(), EE_WON_REQOP_USER_ID, 0);
    case O::DirOpNew: return HeldOp(RustServerOp::NewGetDir(reinterpret_cast<ServerContext*>(a), GetLobbyConfig()->mProductDirectory, 0x04000622, kDirTypes, 2));
    case O::BufNew: return (intptr_t) new WriteBuffer(0, true);
    case O::GameWriteSummary: GM(a)->WriteSummary(*reinterpret_cast<WriteBuffer*>(b)); return 0;
    case O::ConnCreateGameObject: RoutingRequestValues().N(true).N((unsigned short)c).S(L->mGameDataType).W(gEmptyWideString).D(reinterpret_cast<WriteBuffer*>(b)->ToByteBuffer(true).get()).Run(RC(a), EE_WON_RTQ_CREATE_DATA, NULL); return 0;
    case O::BufFree: delete reinterpret_cast<WriteBuffer*>(a); return 0;
    case O::EventType: return E(a)->mType;
    case O::EventFindServer: return (intptr_t)L->mServerList->FindServer(LobbyEvtAddr(E(a), LobbyEvtField::Address));
    case O::StatusOpRust: return (intptr_t)US(a)->GetRust();
    case O::EvModeratedGroup: Broadcast(ClientModeratedEvent(C(a), (int)b, G(c)->GetName(), (int)d, (unsigned long)RO(e)->Num(EE_WON_RTF_TIME), true, GUIString(RO(e)->Text(EE_WON_RTT_COMMENT)))); return 0;
    case O::EvModeratedServer: Broadcast(ClientModeratedEvent(C(a), (int)b, S(c)->GetName(), (int)d, (unsigned long)RO(e)->Num(EE_WON_RTF_TIME), true, GUIString(RO(e)->Text(EE_WON_RTT_COMMENT)))); return 0;
    case O::DirListFree: delete reinterpret_cast<DirList*>(a); return 0;
    case O::IterFree: delete reinterpret_cast<LobbyClientIterator*>(a); return 0;
    case O::EvAlertTimed: Broadcast(AlertEvent(4, (int)a, (unsigned long)RO(b)->Num(EE_WON_RTF_TIME), GUIString(new GUIStringData(RO(b)->Text(EE_WON_RTT_TEXT))))); return 0;
    case O::EvAlertPlain: Broadcast(AlertEvent(4, (int)a, 0, GUIString(new GUIStringData(RO(b)->Text(EE_WON_RTT_TEXT))))); return 0;
    case O::DirEntitiesNew: return (intptr_t) new DirList(RS(a)->GetEntities(EE_WON_REQOP_DIR_LIST));
    case O::DirListEmpty: return reinterpret_cast<DirList*>(a)->empty();
    case O::GroupQueryNew: return HeldOp(RustServerOp::NewGroupListReq(S(a)->GetAddress(), 0x29));
    case O::RunTrackedGroups: L->RunTrackedOp(AO(a), new LogicCompletion<RoutingLogic>(&RoutingLogic::QueryServerGroupsDone, L), 0); return 0;
    case O::EventComponent: return (intptr_t)E(a)->mComponent;
    case O::OnInternetScreen: return LobbyOnInternetScreen();
    case O::UserCanceledLast: return LobbyUserCanceledLastStatusOp();
    case O::StatusOpNew: return HeldOp(new RoutingGetUserStatusOp(L->mServerList->AddressAt((size_t)a), reinterpret_cast<UDPManager*>(b)));
    case O::ConnectOpNew: return HeldOp(new ServerConnectionConnectOp(RC(a), S(b)->GetAddress()));
    default: Log("unexpected op " + N((long long)inOp)); (void)d; (void)e; return 0;
    }
}
} // namespace Route
} // namespace NlNew
#endif
