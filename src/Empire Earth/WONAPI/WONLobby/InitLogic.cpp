// InitLogic: Internet lobby start-up (DNS, directory, version and firewall checks, terms of use, advert)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include "platform/PlatString.h"
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyAccountLogic.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/EELobbySound.h"
#include "rust/won_lobby/flow.h"
#include "rust/won_lobby/logics.h"
#include "rust/won_net/hostop.h"
#include "rust/won_net/httpcache.h"
#include "rust/won_net/httpdoc.h"
#include "rust/won_net/httpget.h"
#include "rust/won_net/reqdir.h"
#include "rust/won_net/reqops.h"
// A Rust object since R6 WONLobby round 8 (rust/crates/ee_won_lobby/src/logics.rs: the errors, the order of the checks
// and every decision, over flow.rs's); this unit keeps the shell (the HTTP session, the firewall test's socket, op
// and address, the adapter dialog, the advert's link) and the host's ops, each one the original's calls on a C++
// object (the ops, the server lists, the files, the events).
using namespace WONAPI;
// Timeout for the firewall detect and accept operations, in milliseconds.
unsigned long WONAPI::gLobbyAcceptTimeout = 20000;

namespace WONAPI {
static void InitAddAll(ServerContext* inList, const std::list<std::string>& inNames)
{
    for (std::list<std::string>::const_iterator it = inNames.begin(); it != inNames.end(); ++it) {
        IPAddr anAddr(*it);
        inList->AddAddress(anAddr);
    }
}

// the terms-of-use document of an owner written to its file: true unless the writer threw (the original counts
// a file it could not open as written)
static bool InitSaveTOU(RustServerOp* inOp, int inOwner)
{
    FileWriter writer;
    try {
        if (writer.Open((inOwner == HTTPDocOwner_Sys ? LobbyGetSysTOUPath() : LobbyGetGameTOUPath()).c_str(), true)) {
            ByteBufferPtr document = inOp->GetHttpDocument(inOwner);
            size_t aBytes = document.get() != NULL ? ee_won_init_document_bytes(document->length()) : 0;
            if (aBytes != 0)
                writer.WriteBytes(document->data(), aBytes);
        }
        return true;
    } catch (FileWriterException&) {}
    return false;
}

static ServerContext* InitList(intptr_t inWhich)
{
    switch (inWhich) {
    case 1: return LobbyGetAuthServers();
    case 2: return LobbyGetAccountServers();
    case 3: return LobbyGetFirewallDetectServers();
    case 4: return LobbyGetPatchServers();
    }
    return NULL;
}

// the directory query's paths: the auth, database proxy and firewall services, then the product's patch servers
// when there is a config
static const std::wstring* InitDirPaths(LobbyConfig* inConfig)
{
    static std::wstring sPaths[4] = { L"/TitanServers/Auth", L"/TitanServers/DBProxy", L"/TitanServers/Firewall" };
    sPaths[3] = inConfig != NULL ? inConfig->mProductDirectory + L"/Patch" : std::wstring();
    return sPaths;
}

static RustServerOp* R(intptr_t a) { return static_cast<RustServerOp*>(reinterpret_cast<AsyncOp*>(a)); }
static GUIString* T(intptr_t a) { return reinterpret_cast<GUIString*>(a); }
static ServerContext* SC(intptr_t a) { return reinterpret_cast<ServerContext*>(a); }
static RustServerOp* FW(intptr_t a) { return static_cast<RustServerOp*>(reinterpret_cast<AsyncOp*>(a)); }
// a new object with one reference the Rust gives back (the original's SmartPtr local)
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
static const uint8_t* B(const std::string& inText) { return reinterpret_cast<const uint8_t*>(inText.data()); }

// The host: each case one statement; the decisions and the order are the Rust's (logics.rs, and netlogicops.rs
// for the cases R6 WON round 32 split).
static intptr_t InitHost(void* inObj, EeWonLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonLogicOp O;
    InitLogic* aLogic = static_cast<InitLogic*>(inObj);
    LobbyConfig* config = GetLobbyConfig();
    switch (inOp) {
    case O::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case O::OpStatus: return (int)reinterpret_cast<AsyncOp*>(a)->GetStatus();
    case O::BroadcastLobby: LobbyBroadcastLobbyEvent((int)a, true); return 0;
    case O::Exit: LobbyExit((int)a); return 0;
    case O::Root: return (intptr_t)GetLobbyRoot();
    case O::InitStatusText: LobbyBroadcastEvent(LoginInitStatusEvent(gLobbyResources[a].mString, *T(b), (d & 1) != 0, (int)(d >> 8)), true); return 0;
    case O::InitStatusRes: LobbyBroadcastEvent(LoginInitStatusEvent(gLobbyResources[a].mString, gLobbyResources[c].mString, (d & 1) != 0, (int)(d >> 8)), true); return 0;
    case O::InitStatusEmpty: LobbyBroadcastEvent(LoginInitStatusEvent(gLobbyResources[a].mString, gEmptyGUIString, (d & 1) != 0, (int)(d >> 8)), true); return 0;
    case O::SetVersionStatus: LobbySetVersionStatus((WONStatus)a); return 0;
    case O::CheckForInternet: return config != NULL && config->mCheckForInternet;
    case O::HasInternet: return AsyncSocket::HasInternetConnection();
    case O::HasConfig: return config != NULL;
    case O::InitListGet: return (intptr_t)InitList(a);
    case O::ServerClear: SC(a)->Clear(); return 0;
    case O::LoadPrefs: LobbyLoadGlobalPrefs(); IPAddr::SetLocalHost(LobbyGetNetworkAdapter(), true); return 0;
    case O::SysTouModifyTime: return (intptr_t)WONFile(LobbyGetSysTOUPath()).GetModifyTime();
    case O::GameTouModifyTime: return (intptr_t)WONFile(LobbyGetGameTOUPath()).GetModifyTime();
    case O::SetTOUTimes: LobbySetTOUTimes((long)a, (long)b); return 0;
    case O::BindUdp: LobbyBindUDPPort(); return 0;
    // the three controls the login screen disables while it initialises
    case O::EnableLogin: reinterpret_cast<Container*>(a)->EnableControlsById(ControlIdSet{200001, 200002, 200003}, b != 0); return 0;
    case O::LobbyMusic: return LobbyGetLobbyMusic();
    case O::MusicSetLooping: LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->SetLooping(true); return 0;
    case O::MusicPlay: LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Play(); return 0;
    case O::MusicStop: LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Stop(); return 0;
    case O::RoomDirServersGet: return (intptr_t)LobbyGetRoomDirServers();
    case O::RememberHostAddress: IPAddr().SetRememberHostString(true); return 0;
    case O::DirServersRemoveAll: LobbyGetDirServers()->RemoveAllAddresses(); return 0;
    case O::AddAllDir: InitAddAll(LobbyGetDirServers(), config->mDirServers); return 0;
    case O::AddAllRoomDir: InitAddAll(LobbyGetRoomDirServers(), config->mRoomDirServers); return 0;
    case O::AddAllAuth: InitAddAll(LobbyGetAuthServers(), config->mAuthServers); return 0;
    case O::RoomDirEmpty: return LobbyGetRoomDirServers()->GetNumAddresses() < 1;
    case O::ResetRoomDir: LobbyResetRoomDirServers(); return 0;
    case O::PromotionDecode: return Held(Window::GetDefaultWindow()->DecodeImage(R(a)->GetBytes(EE_WON_HTTPGET_LOCAL_PATH).c_str()));
    case O::PromotionBroadcast: LobbyBroadcastEvent(SetCrossPromotionEvent(reinterpret_cast<NativeImage*>(a), aLogic->mCrossPromotion), true); return 0;
    case O::SetPromotionLink: aLogic->mCrossPromotion.assign(reinterpret_cast<const char*>(a), (size_t)b); return 0;
    case O::HttpGetNew: return Held(RustServerOp::NewHttpGet(std::string(reinterpret_cast<const char*>(a), (size_t)b)));
    // HTTPCache::GetGlobalCache(): the Rust cache in the WON directory (ee_won httpcache.rs), the op's reference
    case O::HttpGetSetCache: ee_won_hostop_set(R(a)->GetRustOp(), EE_WON_HTTPGET_CACHE, 0, NULL, 0, NULL, ee_won_http_cache_global(B(WONAPICore::GetDefaultFileDirectory()), WONAPICore::GetDefaultFileDirectory().size())); return 0;
    case O::HttpGetSetPromotionCompletion: R(a)->SetCompletion(new LogicCompletion<InitLogic>(&InitLogic::CrossPromotionImageCompletion, aLogic)); return 0;
    case O::SessionAddOp: HttpSessionAddOp(aLogic->mHTTPSession.get(), R(a)); return 0;
    case O::TOUTimes: {
        long systemTime, gameTime;
        LobbyGetTOUTimes(systemTime, gameTime);
        *reinterpret_cast<int32_t*>(a) = (int32_t)systemTime;
        *reinterpret_cast<int32_t*>(b) = (int32_t)gameTime;
        return 0;
    }
    case O::OpNum: return (intptr_t)R(a)->GetNum((int)b, c);
    case O::SaveTOU: return InitSaveTOU(R(a), (int)b);
    case O::BroadcastDocument: LobbyBroadcastEvent(GotHTTPDocumentEvent(R(a)), true); return 0;
    case O::DocBytes: {
        ByteBufferPtr document = R(a)->GetHttpDocument((int)b);
        *reinterpret_cast<size_t*>(c) = document.get() != NULL ? (size_t)document->length() : (size_t)-1;
        return (intptr_t)(document.get() != NULL ? document->data() : NULL); // the op keeps the document
    }
    case O::BroadcastNoPromotion: LobbyBroadcastEvent(SetCrossPromotionEvent(NULL, std::string("")), true); return 0;
    case O::HasAuthConfig: return !config->mAuthServers.empty();
    case O::ServerAddFromDir: SC(a)->AddAddressesFromDir(R(b)->GetEntities(EE_WON_REQOP_DIR_LIST, c), NULL); return 0;
    case O::ServerCount: return InitList(a)->GetNumAddresses();
    case O::DirNotifyFailed: LobbyGetDirServers()->NotifyFailed(R(a)->GetCurAddr()); return 0;
    case O::ServerNotifyFailed: SC(a)->NotifyFailed(R(b)->GetCurAddr()); return 0;
    case O::TextNew: return (intptr_t)(a >= 0 ? new GUIString(gLobbyResources[a].mString) : new GUIString);
    case O::TextAppendRes: T(a)->append(gLobbyResources[b].mString); return 0;
    case O::TextAppendComma: T(a)->append(GUIString(", ")); return 0;
    case O::TextLength: return T(a)->length();
    case O::TextAppendText: T(a)->append(*T(b)); return 0;
    case O::TextFree: delete T(a); return 0;
    case O::TextStatus: return (intptr_t)new GUIString(LobbyFormatString(gLobbyResources[a].mString, GetWONStatusString((WONStatus)b)));
    case O::IsAcceptOp: return reinterpret_cast<AsyncOp*>(a) == aLogic->mAcceptOp.get();
    case O::ClearAcceptOp: aLogic->mAcceptOp = NULL; return 0;
    case O::AcceptedFromFirewall:
        return static_cast<SocketOp*>(reinterpret_cast<AsyncOp*>(a))->GetAcceptedSocket()->GetDestAddr().GetHost() ==
               aLogic->mFirewallAddress.GetHost();
    case O::SetBehindFirewall: LobbySetBehindFirewall(a != 0); return 0;
    case O::AcceptOpNewFrom: aLogic->mAcceptOp = new SocketOp(EeWonCoreSocketOp::Accept, static_cast<SocketOp*>(reinterpret_cast<AsyncOp*>(a))->GetSocket()); return 0;
    case O::AcceptOpNewFirewall: aLogic->mAcceptOp = new SocketOp(EeWonCoreSocketOp::Accept, aLogic->mFirewallSocket.get()); return 0;
    case O::AcceptOpRun: aLogic->RunOp(aLogic->mAcceptOp.get(), new LogicCompletion<InitLogic>(&InitLogic::AcceptCompletion, aLogic), gLobbyAcceptTimeout); return 0;
    case O::SetFirewallAddress: aLogic->mFirewallAddress = FW(a)->GetCurAddr(); return 0;
    case O::SetPublicAddress: LobbySetPublicAddress(FW(a)->AddrOf(EE_WON_FWOP_PUBLIC_ADDR)); return 0;
    case O::HasFirewallSocket: return aLogic->mFirewallSocket.get() != NULL;
    case O::DnsTestReady: return config != NULL && !config->mDirServers.empty();
    case O::DnsOpNew: return Held(NewDnsTestOp(config->mDirServers.front()));
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
    case O::VersionReady: return LobbyGetPatchServers() != NULL && config != NULL;
    // CheckValidVersionOp: a Rust op (ee_won reqops.rs) with the product, config and version
    case O::VersionCheckNew: return Held(RustServerOp::NewRequest(ee_won_reqop_version_check(B(config->mProductName), config->mProductName.size(), B(config->mLanguage), config->mLanguage.size(), B(config->mVersion), config->mVersion.size()), LobbyGetPatchServers()));
    case O::FirewallServers: return (intptr_t)LobbyGetFirewallDetectServers();
    case O::ResetFirewallTest: LobbyResetFirewallTest(); return 0;
    case O::FirewallOpNew: return Held(RustServerOp::NewFirewall(SC(a)));
    case O::FirewallSetListenPort: FW(a)->SetNum(EE_WON_FWOP_LISTEN_PORT, LobbyGetLobbyPort()); return 0;
    case O::FirewallSetDoListen: FW(a)->SetNum(EE_WON_FWOP_DO_LISTEN, b != 0); return 0;
    case O::FirewallSetWaitForConnect: FW(a)->SetNum(EE_WON_FWOP_WAIT_FOR_CONNECT, b != 0); return 0;
    case O::FirewallSocketNew: aLogic->mFirewallSocket = new AsyncSocket(0); return 0;
    case O::FirewallBind: return (int)aLogic->mFirewallSocket->Fn2B0D90(LobbyGetLobbyPort(), 0);
    case O::FirewallListen: return (int)aLogic->mFirewallSocket->Listen(1000);
    case O::FirewallSocketDrop: aLogic->mFirewallSocket = NULL; return 0;
    case O::HasAcceptOp: return aLogic->mAcceptOp.get() != NULL;
    case O::AcceptOpKill: aLogic->mAcceptOp->Kill(true); return 0;
    case O::AcceptOpHasSocket: return aLogic->mAcceptOp->GetSocket() != NULL;
    case O::AcceptOpCloseSocket: aLogic->mAcceptOp->GetSocket()->Close(); return 0;
    case O::KillHttp: HttpSessionKillAll(aLogic->mHTTPSession.get()); return 0;
    case O::ShowInternet: LobbyShowInternetScreen(); return 0;
    case O::AdapterReset: aLogic->mNetworkAdapterDialog->LCall(EeWonLobbyDlgCall::Reset); return 0;
    case O::AdapterDo: return LobbyDoDialog(aLogic->mNetworkAdapterDialog.get());
    case O::SetAdapter: LobbySetNetworkAdapter((unsigned long)(uint32_t)aLogic->mNetworkAdapterDialog->LCall(EeWonLobbyDlgCall::SelectedAddress)); return 0;
    default: return 0;
    }
}

// Creates the lobby start-up logic with its own HTTP session
InitLogic::InitLogic() : mRust(ee_won_logic_new(EeWonLogicKind::Init, this, InitHost))
{
    this->mHTTPSession = HttpSessionNew();
}

InitLogic::~InitLogic() { ee_won_logic_free(this->mRust); }

// Looks up the choose-network-adapter dialog from the lobby layout
void InitLogic::InitComponent(ComponentConfig* inConfig)
{
    SafeGetDialog(inConfig, this->mNetworkAdapterDialog, EeWonLobbyDlgKind::ChooseAdapter, "ChooseNetworkAdapterDialog", __FILE__, 49);
}

// Handles the login screen's Back, Internet and choose-network-adapter buttons
void InitLogic::HandleControlEvent(int inControlId) { this->Call(EeWonLogicCall::InitControlId, inControlId); }
}
