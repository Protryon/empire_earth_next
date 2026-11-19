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

// The three controls the login screen disables while it initialises.
struct LoginControlIds : public ControlIdSet {
    void Add(value_type inA,value_type inB,value_type inC) { insert(inA); insert(inB); insert(inC); }
};

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

static RustServerOp* R(intptr_t a) { return static_cast<RustServerOp*>(reinterpret_cast<AsyncOp*>(a)); }
static GUIString* T(intptr_t a) { return reinterpret_cast<GUIString*>(a); }

static intptr_t InitHost(void* inObj, EeWonLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonLogicOp O;
    InitLogic* aLogic = static_cast<InitLogic*>(inObj);
    LobbyConfig* config = GetLobbyConfig();
    switch (inOp) {
    case O::OpStatus: return (int)reinterpret_cast<AsyncOp*>(a)->GetStatus();
    case O::BroadcastLobby: LobbyBroadcastLobbyEvent((int)a, true); return 0;
    case O::Exit: LobbyExit((int)a); return 0;
    case O::Root: return (intptr_t)GetLobbyRoot();
    case O::InitStatus: {
        const GUIString& aDescription = b != 0 ? *T(b) : c >= 0 ? gLobbyResources[c].mString : gEmptyGUIString;
        LobbyBroadcastEvent(LoginInitStatusEvent(gLobbyResources[a].mString, aDescription, (d & 1) != 0, (int)(d >> 8)), true);
        return 0;
    }
    case O::SetVersionStatus: LobbySetVersionStatus((WONStatus)a); return 0;
    case O::CheckForInternet: return config != NULL && config->mCheckForInternet;
    case O::HasInternet: return AsyncSocket::HasInternetConnection();
    case O::ClearServerLists: {
        ServerContext* account = LobbyGetAccountServers();
        ServerContext* auth = LobbyGetAuthServers();
        ServerContext* firewall = LobbyGetFirewallDetectServers();
        ServerContext* routing = LobbyGetPatchServers();
        if (account != NULL) account->Clear();
        if (auth != NULL) auth->Clear();
        if (firewall != NULL) firewall->Clear();
        if (routing != NULL) routing->Clear();
        return 0;
    }
    case O::LoadPrefs: LobbyLoadGlobalPrefs(); IPAddr::SetLocalHost(LobbyGetNetworkAdapter(), true); return 0;
    case O::LoadTOUTimes: {
        WONFile file(LobbyGetSysTOUPath());
        long systemTime = file.GetModifyTime();
        file.SetFilePath(LobbyGetGameTOUPath());
        long gameTime = file.GetModifyTime();
        LobbySetTOUTimes(systemTime, gameTime);
        return 0;
    }
    case O::BindUdp: LobbyBindUDPPort(); return 0;
    case O::EnableLogin: {
        LoginControlIds ids;
        ids.Add(200001, 200002, 200003);
        reinterpret_cast<Container*>(a)->EnableControlsById(ids, b != 0);
        return 0;
    }
    case O::LobbyMusic: return LobbyGetLobbyMusic();
    case O::Music:
        if (a == 0) LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->SetLooping(true);
        else if (a == 1) LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Play();
        else LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Stop();
        return 0;
    case O::DnsReady:
        return config != NULL && LobbyGetDirServers() != NULL && LobbyGetRoomDirServers() != NULL && LobbyGetAuthServers() != NULL;
    case O::FillDnsServers: {
        // the original's address with mRememberHostString set (separate from the loops' addresses)
        IPAddr address;
        address.SetRememberHostString(true);
        LobbyGetDirServers()->RemoveAllAddresses();
        InitAddAll(LobbyGetDirServers(), config->mDirServers);
        InitAddAll(LobbyGetRoomDirServers(), config->mRoomDirServers);
        InitAddAll(LobbyGetAuthServers(), config->mAuthServers);
        return 0;
    }
    case O::RoomDirEmpty: return LobbyGetRoomDirServers()->GetNumAddresses() < 1;
    case O::ResetRoomDir: LobbyResetRoomDirServers(); return 0;
    case O::PromotionImage: {
        SmartPtr<NativeImage> image = Window::GetDefaultWindow()->DecodeImage(R(a)->GetBytes(EE_WON_HTTPGET_LOCAL_PATH).c_str());
        LobbyBroadcastEvent(SetCrossPromotionEvent(image.get(), aLogic->mCrossPromotion), true);
        return 0;
    }
    case O::SetPromotionLink: aLogic->mCrossPromotion.assign(reinterpret_cast<const char*>(a), (size_t)b); return 0;
    case O::FetchPromotionImage: {
        SmartPtr<RustServerOp> op = RustServerOp::NewHttpGet(std::string(reinterpret_cast<const char*>(a), (size_t)b));
        // HTTPCache::GetGlobalCache(): the Rust cache in the WON directory (ee_won httpcache.rs), the op's reference
        const std::string& aDir = WONAPICore::GetDefaultFileDirectory();
        ee_won_hostop_set(op->GetRustOp(), EE_WON_HTTPGET_CACHE, 0, NULL, 0, NULL,
                          ee_won_http_cache_global(reinterpret_cast<const uint8_t*>(aDir.data()), aDir.size()));
        op->SetCompletion(new LogicCompletion<InitLogic>(&InitLogic::CrossPromotionImageCompletion, aLogic));
        aLogic->mHTTPSession->AddOp(op.get());
        return 0;
    }
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
    case O::DirReady:
        return config != NULL && LobbyGetAccountServers() != NULL && LobbyGetAuthServers() != NULL &&
               LobbyGetFirewallDetectServers() != NULL && LobbyGetDirServers() != NULL;
    case O::HasAuthConfig: return !config->mAuthServers.empty();
    case O::AddDirAddresses: {
        ServerContext* aList = InitList(a);
        if (aList != NULL)
            aList->AddAddressesFromDir(R(b)->GetEntities(EE_WON_REQOP_DIR_LIST, c), NULL);
        return 0;
    }
    case O::ServerCount: return InitList(a)->GetNumAddresses();
    case O::DirNotifyFailed: LobbyGetDirServers()->NotifyFailed(R(a)->GetCurAddr()); return 0;
    case O::TextNew: return (intptr_t)(a >= 0 ? new GUIString(gLobbyResources[a].mString) : new GUIString);
    case O::TextAppendRes: T(a)->append(gLobbyResources[b].mString); return 0;
    case O::TextAppendComma: T(a)->append(GUIString(", ")); return 0;
    case O::TextLength: return T(a)->length();
    case O::TextAppendText: T(a)->append(*T(b)); return 0;
    case O::TextFree: delete T(a); return 0;
    case O::TextStatus: return (intptr_t)new GUIString(LobbyFormatString(gLobbyResources[a].mString, GetWONStatusString((WONStatus)b)));
    case O::VersionNotifyFailed: {
        ServerContext* versions = LobbyGetPatchServers();
        if (versions != NULL)
            versions->NotifyFailed(R(a)->GetCurAddr());
        return 0;
    }
    case O::IsAcceptOp: return reinterpret_cast<AsyncOp*>(a) == aLogic->mAcceptOp.get();
    case O::ClearAcceptOp: aLogic->mAcceptOp = NULL; return 0;
    case O::AcceptedFromFirewall:
        return static_cast<AcceptOp*>(reinterpret_cast<AsyncOp*>(a))->GetAcceptedSocket()->GetDestAddr().GetHost() ==
               aLogic->mFirewallAddress.GetHost();
    case O::SetBehindFirewall: LobbySetBehindFirewall(a != 0); return 0;
    case O::RestartAccept:
        aLogic->mAcceptOp = new AcceptOp(static_cast<AcceptOp*>(reinterpret_cast<AsyncOp*>(a))->GetSocket());
        aLogic->RunOp(aLogic->mAcceptOp.get(), new LogicCompletion<InitLogic>(&InitLogic::AcceptCompletion, aLogic),
                      gLobbyAcceptTimeout);
        return 0;
    case O::SetPublicAddress:
        LobbySetPublicAddress(static_cast<DetectFirewallOp*>(reinterpret_cast<AsyncOp*>(a))->GetPublicAddr());
        return 0;
    case O::HasFirewallSocket: return aLogic->mFirewallSocket.get() != NULL;
    case O::StartAccept:
        aLogic->mFirewallAddress = static_cast<DetectFirewallOp*>(reinterpret_cast<AsyncOp*>(a))->GetCurAddr();
        aLogic->mAcceptOp = new AcceptOp(aLogic->mFirewallSocket.get());
        aLogic->RunOp(aLogic->mAcceptOp.get(), new LogicCompletion<InitLogic>(&InitLogic::AcceptCompletion, aLogic),
                      gLobbyAcceptTimeout);
        aLogic->mFirewallSocket = NULL;
        return 0;
    case O::DnsTestReady: return config != NULL && !config->mDirServers.empty();
    case O::RunDnsTest: {
        SmartPtr<DNSTestOp> op = new DNSTestOp(config->mDirServers.front());
        aLogic->RunTrackedOp(op.get(), new LogicCompletion<InitLogic>(&InitLogic::DNSCompletion, aLogic), 0);
        return 0;
    }
    case O::StartDocuments:
        if (config != NULL) {
            static const int kTypes[3] = {HTTPDocType_MOTD, HTTPDocType_TOU, HTTPDocType_ADV};
            SmartPtr<RustServerOp> ops[3];
            for (int i = 0; i < 3; i++) {
                ops[i] = RustServerOp::NewHttpDocument(config->mProductName, kTypes[i]);
                ops[i]->SetBytes(EE_WON_HTTPDOC_CONFIG, config->mLanguage);
                ops[i]->Set(EE_WON_HTTPDOC_SESSION, aLogic->mHTTPSession.get());
                aLogic->RunTrackedOp(ops[i].get(), new LogicCompletion<InitLogic>(&InitLogic::HTTPDocumentCompletion, aLogic), 0);
            }
        }
        return 0;
    case O::DirServers: return (intptr_t)LobbyGetDirServers();
    case O::RunDirQuery: {
        std::wstring paths[4] = { L"/TitanServers/Auth", L"/TitanServers/DBProxy", L"/TitanServers/Firewall" };
        if (config != NULL)
            paths[3] = config->mProductDirectory + L"/Patch";
        SmartPtr<RustServerOp> op = RustServerOp::NewGetMultiDir(reinterpret_cast<ServerContext*>(a), 0x04000002, paths,
                                                                 config != NULL ? 4 : 3);
        aLogic->RunTrackedOp(op.get(), new LogicCompletion<InitLogic>(&InitLogic::DirectoryCompletion, aLogic), 0);
        return 0;
    }
    case O::VersionReady: return LobbyGetPatchServers() != NULL && config != NULL;
    case O::RunVersionCheck: {
        // CheckValidVersionOp: a Rust op (ee_won reqops.rs) with the product, config and version
        const std::string& aProduct = config->mProductName;
        const std::string& aConfig = config->mLanguage;
        const std::string& aVersion = config->mVersion;
        SmartPtr<RustServerOp> op = RustServerOp::NewRequest(ee_won_reqop_version_check(
            reinterpret_cast<const uint8_t*>(aProduct.data()), aProduct.size(), reinterpret_cast<const uint8_t*>(aConfig.data()),
            aConfig.size(), reinterpret_cast<const uint8_t*>(aVersion.data()), aVersion.size()), LobbyGetPatchServers());
        aLogic->RunTrackedOp(op.get(), new LogicCompletion<InitLogic>(&InitLogic::VersionCompletion, aLogic), 0);
        return 0;
    }
    case O::FirewallServers: return (intptr_t)LobbyGetFirewallDetectServers();
    case O::ResetFirewallTest: LobbyResetFirewallTest(); return 0;
    case O::RunFirewallDetect: {
        SmartPtr<DetectFirewallOp> op = new DetectFirewallOp(reinterpret_cast<ServerContext*>(a));
        op->SetListenPort(LobbyGetLobbyPort());
        op->SetDoListen(false);
        op->SetWaitForConnect(false);
        aLogic->RunTrackedOp(op.get(), new LogicCompletion<InitLogic>(&InitLogic::FirewallCompletion, aLogic), 0);
        return 0;
    }
    case O::FirewallSocketNew: aLogic->mFirewallSocket = new AsyncSocket(0); return 0;
    case O::FirewallBind: return (int)aLogic->mFirewallSocket->Fn2B0D90(LobbyGetLobbyPort(), 0);
    case O::FirewallListen: return (int)aLogic->mFirewallSocket->Listen(1000);
    case O::FirewallSocketDrop: aLogic->mFirewallSocket = NULL; return 0;
    case O::KillAcceptOp:
        if (aLogic->mAcceptOp.get() != NULL) {
            aLogic->mAcceptOp->Kill(true);
            if (aLogic->mAcceptOp->GetSocket() != NULL)
                aLogic->mAcceptOp->GetSocket()->Close();
            aLogic->mAcceptOp = NULL;
        }
        return 0;
    case O::KillHttp: aLogic->mHTTPSession->KillAll(); return 0;
    case O::ShowInternet: LobbyShowInternetScreen(); return 0;
    case O::AdapterDialog:
        aLogic->mNetworkAdapterDialog->Reset();
        return LobbyDoDialog(aLogic->mNetworkAdapterDialog.get());
    case O::SetAdapter: LobbySetNetworkAdapter(aLogic->mNetworkAdapterDialog->GetSelectedAddress()); return 0;
    default: return 0;
    }
}

// Creates the lobby start-up logic with its own HTTP session
InitLogic::InitLogic() : mRust(ee_won_logic_new(EeWonLogicKind::Init, this, InitHost))
{
    this->mHTTPSession = new HTTPSession;
}

InitLogic::~InitLogic() { ee_won_logic_free(this->mRust); }

// Looks up the choose-network-adapter dialog from the lobby layout
void InitLogic::InitComponent(ComponentConfig* inConfig)
{
    SafeGetComponent(inConfig, this->mNetworkAdapterDialog, "ChooseNetworkAdapterDialog", __FILE__, 49);
}

// Handles the login screen's Back, Internet and choose-network-adapter buttons
void InitLogic::HandleControlEvent(int inControlId)
{
    int aWhich = inControlId == ID_LoginScreenBack ? 1 : inControlId == ID_ShowInternetScreen ? 2
               : inControlId == ID_ChooseNetworkAdapter ? 3 : 0;
    if (aWhich != 0) this->Call(EeWonLogicCall::InitControl, aWhich);
}
}
