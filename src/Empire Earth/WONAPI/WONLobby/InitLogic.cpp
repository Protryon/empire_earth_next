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
#include "rust/won_net/status_codes.h"
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "rust/won_lobby/flow.h"
#include "rust/won_lobby/logics.h"
#include "rust/won_net/hostop.h"
#include "rust/won_net/httpdoc.h"
#include "rust/won_net/httpget.h"
#include "rust/won_net/reqdir.h"
#include "rust/won_net/reqmake.h"
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
// the terms-of-use document of an owner written to its file (ee_won flow.rs: true unless the write failed)
static bool InitSaveTOU(const std::string& inPath, const std::string& inDocument)
{ return ee_won_init_save_tou(inPath.c_str(), B(inDocument), inDocument.size()); }

// The host: each case one statement; the decisions and the order are the Rust's (logics.rs, and netlogicops.rs
// for the cases R6 WON round 32 split; the private state's setters are its own since round 39: prv_op).
static intptr_t InitHost(void* inObj, EeWonLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonLogicOp O;
    RustLogic* aLogic = static_cast<RustLogic*>(inObj);
    LobbyConfig* config = GetLobbyConfig();
    switch (inOp) {
    case O::BroadcastLobby: LobbyBroadcastLobbyEvent((int)a, true); return 0;
    case O::Exit: LobbyExit((int)a); return 0;
    case O::Root: return (intptr_t)GetLobbyRoot();
    case O::InitStatusText: LobbyBroadcastEvent(LoginInitStatusEvent(gLobbyResources[a].mString, *T(b), (d & 1) != 0, (int)(d >> 8)), true); return 0;
    case O::InitStatusRes: LobbyBroadcastEvent(LoginInitStatusEvent(gLobbyResources[a].mString, gLobbyResources[c].mString, (d & 1) != 0, (int)(d >> 8)), true); return 0;
    case O::InitStatusEmpty: LobbyBroadcastEvent(LoginInitStatusEvent(gLobbyResources[a].mString, gEmptyGUIString, (d & 1) != 0, (int)(d >> 8)), true); return 0;
    case O::CheckForInternet: return config != NULL && config->mCheckForInternet;
    case O::HasConfig: return config != NULL;
    case O::InitListGet: return (intptr_t)InitList(a);
    case O::ServerClear: SC(a)->Clear(); return 0;
    case O::LoadPrefs: LobbyLoadGlobalPrefs(); IPAddr::SetLocalHost(LobbyGetNetworkAdapter(), true); return 0;
    case O::SysTouModifyTime: return (intptr_t)WONFile(LobbyGetSysTOUPath()).GetModifyTime();
    case O::GameTouModifyTime: return (intptr_t)WONFile(LobbyGetGameTOUPath()).GetModifyTime();
    case O::BindUdp: LobbyBindUDPPort(); return 0;
    // the three controls the login screen disables while it initialises
    case O::EnableLogin: reinterpret_cast<Container*>(a)->EnableControlsById(ControlIdSet{200001, 200002, 200003}, b != 0); return 0;
    case O::LobbyMusic: return LobbyGetLobbyMusic();
    case O::RoomDirServersGet: return (intptr_t)LobbyGetRoomDirServers();
    case O::RememberHostAddress: IPAddr().SetRememberHostString(true); return 0;
    case O::DirServersRemoveAll: LobbyGetDirServers()->RemoveAllAddresses(); return 0;
    case O::AddAllDir: InitAddAll(LobbyGetDirServers(), config->mDirServers); return 0;
    case O::AddAllRoomDir: InitAddAll(LobbyGetRoomDirServers(), config->mRoomDirServers); return 0;
    case O::AddAllAuth: InitAddAll(LobbyGetAuthServers(), config->mAuthServers); return 0;
    case O::RoomDirEmpty: return LobbyGetRoomDirServers()->GetNumAddresses() < 1;
    case O::ResetRoomDir: LobbyResetRoomDirServers(); return 0;
    case O::PromotionDecode: return Held(Window::GetDefaultWindow()->DecodeImage(R(a)->GetBytes(EE_WON_HTTPGET_LOCAL_PATH).c_str()));
    case O::PromotionBroadcast: LobbyBroadcastEvent(SetCrossPromotionEvent(reinterpret_cast<NativeImage*>(a), aLogic->mText), true); return 0;
    case O::SetPromotionLink: aLogic->mText.assign(reinterpret_cast<const char*>(a), (size_t)b); return 0;
    case O::HttpGetNew: return Held(RustServerOp::NewHttpGet(std::string(reinterpret_cast<const char*>(a), (size_t)b)));
    case O::HttpGetSetPromotionCompletion: R(a)->SetCompletion(new ShellCompletion(aLogic, EeWonLogicShellDone::InitPromotionImage)); return 0;
    case O::SessionAddOp: HttpSessionAddOp(aLogic->Slot<HTTPSession>(kInitSession), R(a)); return 0;
    case O::SaveTOU: return InitSaveTOU(b == HTTPDocOwner_Sys ? LobbyGetSysTOUPath() : LobbyGetGameTOUPath(), R(a)->GetBytes(EE_WON_HTTPDOC_DOCUMENT, b));
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
    case O::IsAcceptOp: return reinterpret_cast<AsyncOp*>(a) == aLogic->Slot<AsyncOp>(kInitAcceptOp);
    case O::ClearAcceptOp: aLogic->mSlot[kInitAcceptOp] = NULL; return 0;
    case O::AcceptedFromFirewall:
        return static_cast<SocketOp*>(reinterpret_cast<AsyncOp*>(a))->GetAcceptedSocket()->GetDestAddr().GetHost() ==
               aLogic->mAddr.GetHost();
    case O::AcceptOpNewFrom: aLogic->mSlot[kInitAcceptOp] = new SocketOp(EeWonCoreSocketOp::Accept, static_cast<SocketOp*>(reinterpret_cast<AsyncOp*>(a))->GetSocket()); return 0;
    case O::AcceptOpNewFirewall: aLogic->mSlot[kInitAcceptOp] = new SocketOp(EeWonCoreSocketOp::Accept, aLogic->Slot<AsyncSocket>(kInitFirewallSocket)); return 0;
    case O::AcceptOpRun: aLogic->RunOp(aLogic->Slot<AsyncOp>(kInitAcceptOp), new ShellCompletion(aLogic, EeWonLogicShellDone::InitAccept), gLobbyAcceptTimeout); return 0;
    case O::SetFirewallAddress: aLogic->mAddr = FW(a)->GetCurAddr(); return 0;
    case O::SetPublicAddress: LobbySetPublicAddress(FW(a)->AddrOf(EE_WON_FWOP_PUBLIC_ADDR)); return 0;
    case O::HasFirewallSocket: return aLogic->mSlot[kInitFirewallSocket].get() != NULL;
    case O::DnsTestReady: return config != NULL && !config->mDirServers.empty();
    case O::DnsOpNew: return Held(NewDnsTestOp(config->mDirServers.front()));
    case O::RunTrackedDns: aLogic->RunTrackedOp(reinterpret_cast<AsyncOp*>(a), new ShellCompletion(aLogic, EeWonLogicShellDone::InitDns), 0); return 0;
    case O::RunTrackedDocument: aLogic->RunTrackedOp(reinterpret_cast<AsyncOp*>(a), new ShellCompletion(aLogic, EeWonLogicShellDone::InitDocument), 0); return 0;
    case O::RunTrackedDirectory: aLogic->RunTrackedOp(reinterpret_cast<AsyncOp*>(a), new ShellCompletion(aLogic, EeWonLogicShellDone::InitDirectory), 0); return 0;
    case O::RunTrackedVersion: aLogic->RunTrackedOp(reinterpret_cast<AsyncOp*>(a), new ShellCompletion(aLogic, EeWonLogicShellDone::InitVersion), 0); return 0;
    case O::RunTrackedFirewall: aLogic->RunTrackedOp(reinterpret_cast<AsyncOp*>(a), new ShellCompletion(aLogic, EeWonLogicShellDone::InitFirewall), 0); return 0;
    case O::HttpDocNew: return Held(RustServerOp::NewHttpDocument(config->mProductName, (int)a));
    case O::HttpDocSetConfig: R(a)->SetBytes(EE_WON_HTTPDOC_CONFIG, config->mLanguage); return 0;
    case O::HttpDocSetSession: R(a)->Set(EE_WON_HTTPDOC_SESSION, aLogic->Slot<HTTPSession>(kInitSession)); return 0;
    case O::DirServers: return (intptr_t)LobbyGetDirServers();
    case O::DirQueryNew: return Held(RustServerOp::NewRequest(ee_won_reqmake_init_dirs(0x04000002, config != NULL ? config->mProductDirectory.data() : NULL, config != NULL ? config->mProductDirectory.size() : 0, sizeof(wchar_t), (int)b), SC(a)));
    case O::VersionReady: return LobbyGetPatchServers() != NULL && config != NULL;
    // CheckValidVersionOp: a Rust op (ee_won reqops.rs) with the product, config and version
    case O::VersionCheckNew: return Held(RustServerOp::NewRequest(ee_won_reqop_version_check(B(config->mProductName), config->mProductName.size(), B(config->mLanguage), config->mLanguage.size(), B(config->mVersion), config->mVersion.size()), LobbyGetPatchServers()));
    case O::FirewallServers: return (intptr_t)LobbyGetFirewallDetectServers();
    case O::FirewallOpNew: return Held(RustServerOp::NewFirewall(SC(a)));
    case O::FirewallSetListenPort: FW(a)->SetNum(EE_WON_FWOP_LISTEN_PORT, LobbyGetLobbyPort()); return 0;
    case O::FirewallSetDoListen: FW(a)->SetNum(EE_WON_FWOP_DO_LISTEN, b != 0); return 0;
    case O::FirewallSetWaitForConnect: FW(a)->SetNum(EE_WON_FWOP_WAIT_FOR_CONNECT, b != 0); return 0;
    case O::FirewallSocketNew: aLogic->mSlot[kInitFirewallSocket] = new AsyncSocket(0); return 0;
    case O::FirewallBind: return (int)aLogic->Slot<AsyncSocket>(kInitFirewallSocket)->Fn2B0D90(LobbyGetLobbyPort(), 0);
    case O::FirewallListen: return (int)aLogic->Slot<AsyncSocket>(kInitFirewallSocket)->Listen(1000);
    case O::FirewallSocketDrop: aLogic->mSlot[kInitFirewallSocket] = NULL; return 0;
    case O::HasAcceptOp: return aLogic->mSlot[kInitAcceptOp].get() != NULL;
    case O::AcceptOpKill: aLogic->Slot<AsyncOp>(kInitAcceptOp)->Kill(true); return 0;
    case O::AcceptOpHasSocket: return aLogic->Slot<SocketOp>(kInitAcceptOp)->GetSocket() != NULL;
    case O::AcceptOpCloseSocket: aLogic->Slot<SocketOp>(kInitAcceptOp)->GetSocket()->Close(); return 0;
    case O::KillHttp: HttpSessionKillAll(aLogic->Slot<HTTPSession>(kInitSession)); return 0;
    case O::ShowInternet: LobbyShowInternetScreen(); return 0;
    case O::AdapterReset: aLogic->mDialog[kInitAdapter]->LCall(EeWonLobbyDlgCall::Reset); return 0;
    case O::AdapterDo: return LobbyDoDialog(aLogic->mDialog[kInitAdapter].get());
    case O::SetAdapter: LobbySetNetworkAdapter((unsigned long)(uint32_t)aLogic->mDialog[kInitAdapter]->LCall(EeWonLobbyDlgCall::SelectedAddress)); return 0;
    default: return 0;
    }
}

// The start-up logic's Rust object over the shell, with its own HTTP session (the shell's virtuals and its
// dialog's lookup: logicshell.rs)
void* InitLogicMake(RustLogic* inSelf)
{
    return (inSelf->mSlot[kInitSession] = HttpSessionNew(), ee_won_logic_new(EeWonLogicKind::Init, inSelf, InitHost));
}
}
