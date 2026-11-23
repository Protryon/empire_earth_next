// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyAccountLogic.h"
#include "ee/wonapi/WONLobbyDialogLogic.h"
#include "ee/wonapi/WONLobbyLan.h"
#include "ee/wonapi/WONLobbyRoutingLogic.h"
#include "ee/wonapi/WONLobbyStaging.h"
#include "ee/wonapi/WONLobbyPing.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/WONCRC16.h"
#include "ee/wonapi/WONLobbyControls.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONLobbyControlConfig.h"
#include "ee/wonapi/WONGUIShell.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONLobbyGuard.h"
#include "rust/won_lobby/lobbyprv.h"
using namespace WONAPI;


// The resource table's factories: a lobby control kind (the one-line subclasses LoginStatusCtrl, CreateAccountCtrl,
// ... were these shells made with their kinds), a root screen, a dialog class
template <EeWonLobbyCtlKind K> static Component* CtlFactory() { return new LobbyRustCtrl(K); }
template <EeWonLobbyCtlKind K> static Component* DlgCtlFactory() { return new LobbyRustDialogCtrl(K); }
template <bool Lan> static Component* ScreenFactory() { return new RootScreen(Lan); }
typedef EeWonLobbyCtlKind Ck;

namespace WONAPI {

// the methods' statements (lobbyprv.rs composes them, R6 WON round 33); Init's locals are these statics
static ConfigParser* sPrvParser;
static SmartPtr<ContainerConfig> sPrvComponentConfig;
static ResourceConfigTablePtr sPrvTable;
static std::map<std::string, std::string, StringLessNoCase>::iterator sPrvDefine;
static CRC16 sPrvCrc;
// Init's try: a config error is the init error (0); 1 when f returned
template <class F> static intptr_t PrvConfigTry(WONLobbyPrv* p, F f)
{
    try {
        f();
        return 1;
    } catch (ConfigObjectException& error) {
        p->mInitError = GUIString(error.mText);
        return 0;
    }
}
static intptr_t PrvHost(void* inPrv, EeWonLobbyPrvOp inOp, intptr_t a)
{
    typedef EeWonLobbyPrvOp Op;
    WONLobbyPrv* p = static_cast<WONLobbyPrv*>(inPrv);
    switch (inOp) {
    case Op::InitErrorClear: p->mInitError = GetEmptyGUIStringData(); return 0;
    case Op::ParserNew: sPrvParser = new ConfigParser; return 0;
    case Op::ParserWarningsAsErrors: sPrvParser->SetWarningsAsErrors(true); return 0;
    case Op::ParserOpenDefault: return WON_GUARDED(sPrvParser->OpenFile(WONAPICore::GetDefaultFileDirectory() + LobbyConfig::mDefaultFileName));
    case Op::ErrorNotFound: p->mInitError = GUIString("Couldn't find " + LobbyConfig::mDefaultFileName); return 0;
    case Op::ParseTopLevel: return WON_GUARDED(p->mConfig->ParseTopLevel(*sPrvParser, NULL));
    case Op::ErrorParseHead: p->mInitError = GUIString(LobbyConfig::mDefaultFileName + " error: "); return 0;
    case Op::ErrorParseReason: p->mInitError.append(GUIString(sPrvParser->GetAbortReason())); return 0;
    case Op::LobbyPortFromConfig: p->mLobbyPort = p->mConfig->mLobbyPort; return 0;
    case Op::ComponentConfigNew: sPrvComponentConfig = new ContainerConfig; return 0;
    case Op::LobbyContainerNew: p->mLobbyContainer = new LobbyContainer; return 0;
    case Op::SetResourceFolder: ComponentConfig::SetResourceFolder(WONAPICore::GetDefaultFileDirectory() + p->mConfig->mResourceDirectory); return 0;
    case Op::DefinesBegin: sPrvDefine = p->mConfig->mDefines.begin(); return 0;
    case Op::DefinesAtEnd: return (sPrvDefine == p->mConfig->mDefines.end()) ? 1 : 0;
    case Op::DefineAdd: sPrvParser->AddDefine(sPrvDefine->first, sPrvDefine->second, false); return 0;
    case Op::DefineNext: ++sPrvDefine; return 0;
    case Op::ReadMaster: return WON_GUARDED(sPrvComponentConfig->ReadConfig_Unsafe(p->mLobbyContainer.get(), "_LobbyMaster.cfg", sPrvTable.get(), sPrvParser));
    case Op::ErrorFromConfig: p->mInitError = sPrvComponentConfig->mConfigError; return 0;
    case Op::LogicMgrInit: return WON_GUARDED(PrvConfigTry(p, [&] { p->mLogicMgr.Init(sPrvComponentConfig.get()); }));
    case Op::InitFuncRun: return WON_GUARDED(PrvConfigTry(p, [&] { reinterpret_cast<void (*)(ComponentConfig*)>(a)(sPrvComponentConfig.get()); }));
    case Op::CrcNew: sPrvCrc = CRC16(); return 0;
    case Op::CrcPutProductName: sPrvCrc.Put(p->mConfig->mProductName.data(), p->mConfig->mProductName.length()); return 0;
    case Op::CrcPutVersion: sPrvCrc.Put(p->mConfig->mVersion.data(), p->mConfig->mVersion.length()); return 0;
    case Op::ProductIdFromCrc: p->mProductId = sPrvCrc.Get(); return 0;
    case Op::CrcReset: sPrvCrc.Reset(); return 0;
    case Op::CrcPutLanVersion: sPrvCrc.Put(p->mConfig->mLanVersion.data(), p->mConfig->mLanVersion.length()); return 0;
    case Op::LanProductIdFromCrc: p->mLanProductId = sPrvCrc.Get(); return 0;
    case Op::ChatParserInitCommands: p->mChatCommandParser->InitCommands(); return 0;
    case Op::ResDone: sPrvTable = NULL; return 0;
    case Op::ComponentConfigDone: sPrvComponentConfig = NULL; return 0;
    case Op::ParserFree: delete sPrvParser; return 0;
    case Op::LogicMgrKill: p->mLogicMgr.Kill(); return 0;
    case Op::UdpIs: return (p->mUDPManager.get() != NULL) ? 1 : 0;
    case Op::UdpClose: p->mUDPManager->Close(); return 0;
    case Op::UdpClear: p->mUDPManager = NULL; return 0;
    case Op::AuthIs: return (p->mAuthContext.get() != NULL) ? 1 : 0;
    case Op::AuthKill: p->mAuthContext->Kill(); return 0;
    case Op::AuthClear: p->mAuthContext = NULL; return 0;
    case Op::HashSetClear: p->mHasSetHashFileList = false; return 0;
    case Op::HashListClear: p->mHashFileList.clear(); return 0;
    case Op::HashListRead: StringParser(reinterpret_cast<const char*>(a)).ReadValue(p->mHashFileList); return 0;
    case Op::CommunitySet: p->mLoginCommunity = *reinterpret_cast<const std::wstring*>(a); return 0;
    case Op::AuthSetCommunity: p->mAuthContext->SetCommunity(p->mLoginCommunity); return 0;
    case Op::CdKeyCheck: return (p->mConfig->mCDKeyCheck) ? 1 : 0;
    case Op::CdKeySetProduct: p->mCDKey.SetProductString(p->mConfig->mProductName); return 0;
    case Op::CdKeyLoad: p->mCDKey.LoadFromRegistry(); return 0;
    case Op::AuthSetCdKey: p->mAuthContext->SetCDKey(p->mLoginCommunity, p->mCDKey); return 0;
    case Op::LobbyPort8888: p->mLobbyPort = 8888; return 0;
    case Op::TouTimesClear: p->mSysTOUTime = p->mGameTOUTime = 0; return 0;
    case Op::BehindFirewallClear: p->mBehindFirewall = false; return 0;
    case Op::FirewallTestDoneClear: p->mFirewallTestDone = false; return 0;
    case Op::SuspendedClear: p->mSuspended = false; return 0;
    case Op::CurGameTypeClear: p->mCurGameType = (LobbyGameType)0; return 0;
    case Op::LobbyStatusClear: p->mLobbyStatus = (LobbyStatus)0; return 0;
    case Op::RunModeOne: p->mRunMode = (LobbyRunMode)1; return 0;
    case Op::ProductIdClear: p->mProductId = 0; return 0;
    case Op::LanProductIdClear: p->mLanProductId = 0; return 0;
    case Op::SysTouPath: p->mSysTOUPath = WONAPICore::GetDefaultFileDirectory() + "_wonsystou_m.txt"; return 0;
    case Op::GameTouPath: p->mGameTOUPath = WONAPICore::GetDefaultFileDirectory() + "_wongametou_m.txt"; return 0;
    case Op::ConfigNew: p->mConfig = LobbyConfig::mAllocator(); return 0;
    case Op::PersistentDataNew: p->mPersistentData = new LobbyPersistentData; return 0;
    case Op::ChatParserNew: p->mChatCommandParser = new ChatCommandParser; return 0;
    case Op::UdpNew: p->mUDPManager = new UDPManager; return 0;
    case Op::AccountServersNew: p->mAccountServers = new ServerContext; return 0;
    case Op::AuthServersNew: p->mAuthServers = new ServerContext; return 0;
    case Op::DirServersNew: p->mDirServers = new ServerContext; return 0;
    case Op::RoomDirServersNew: p->mRoomDirServers = new ServerContext; return 0;
    case Op::FirewallServersNew: p->mFirewallDetectServers = new ServerContext; return 0;
    case Op::PatchServersNew: p->mPatchServers = new ServerContext; return 0;
    case Op::VersionStatusNone: p->mVersionStatus = WS_None; return 0;
    case Op::AuthContextNew: p->mAuthContext = new AuthContext; return 0;
    case Op::AuthSetServers: p->mAuthContext->SetServerContext(p->mAuthServers.get()); return 0;
    case Op::AccountLogicNew: p->mAccountLogic = new AccountLogic; return 0;
    case Op::DialogLogicNew: p->mDialogLogic = new DialogLogic; return 0;
    case Op::InitLogicNew: p->mInitLogic = new InitLogic; return 0;
    case Op::RoutingLogicNew: p->mRoutingLogic = new RoutingLogic; return 0;
    case Op::StagingLogicNew: p->mStagingLogic = new StagingLogic; return 0;
    case Op::LanLogicNew: p->mLanLogic = new LanLogic; return 0;
    case Op::PingLogicNew: p->mPingLogic = new PingLogic; return 0;
    case Op::AddAccountLogic: p->mLogicMgr.AddLogic(p->mAccountLogic.get()); return 0;
    case Op::AddDialogLogic: p->mLogicMgr.AddLogic(p->mDialogLogic.get()); return 0;
    case Op::AddStagingLogic: p->mLogicMgr.AddLogic(p->mStagingLogic.get()); return 0;
    case Op::AddInitLogic: p->mLogicMgr.AddLogic(p->mInitLogic.get()); return 0;
    case Op::AddLanLogic: p->mLogicMgr.AddLogic(p->mLanLogic.get()); return 0;
    case Op::AddRoutingLogic: p->mLogicMgr.AddLogic(p->mRoutingLogic.get()); return 0;
    case Op::AddPingLogic: p->mLogicMgr.AddLogic(p->mPingLogic.get()); return 0;
    case Op::ResInit: sPrvTable = LobbyResource_Init(reinterpret_cast<ResourceConfigTable*>(a)); return 0;
    case Op::ResGuiInit: WONGUIConfig_Init(sPrvTable.get()); return 0;
    case Op::AllocBrowserComponent: sPrvTable->AddAllocator("BrowserComponent",NewConfigOf<EeWonCfgKind::Browser>,NewComponentOf<BrowserComponent>); return 0;
    case Op::AllocChatCtrl: sPrvTable->AddAllocator("ChatCtrl",NewConfigOf<ChatCtrlConfig>,NewComponentOf<ChatCtrl>); return 0;
    case Op::AllocCreateAccountCtrl: sPrvTable->AddAllocator("CreateAccountCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::CreateAccount>); return 0;
    case Op::AllocDirectConnectCtrl: sPrvTable->AddAllocator("DirectConnectCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::DirectConnect>); return 0;
    case Op::AllocFriendsListCtrl: sPrvTable->AddAllocator("FriendsListCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::Friends>); return 0;
    case Op::AllocGameBrowserCtrl: sPrvTable->AddAllocator("GameBrowserCtrl",NewConfigOf<GameBrowserCtrlConfig>,NewComponentOf<GameBrowserCtrl>); return 0;
    case Op::AllocGameStagingCtrl: sPrvTable->AddAllocator("GameStagingCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::Staging>); return 0;
    case Op::AllocGameUserListCtrl: sPrvTable->AddAllocator("GameUserListCtrl",NewConfigOf<GameUserListCtrlConfig>,GameUserListCtrlConfig::CompFactory); return 0;
    case Op::AllocInitFailedCtrl: sPrvTable->AddAllocator("InitFailedCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::InitFailed>); return 0;
    case Op::AllocInternetOptionsCtrl: sPrvTable->AddAllocator("InternetOptionsCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::NetOptions>); return 0;
    case Op::AllocInternetScreen: sPrvTable->AddAllocator("InternetScreen",NewConfigOf<ContainerConfig>,ScreenFactory<false>); return 0;
    case Op::AllocLanOptionsCtrl: sPrvTable->AddAllocator("LanOptionsCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LanOptions>); return 0;
    case Op::AllocLanScreen: sPrvTable->AddAllocator("LanScreen",NewConfigOf<ContainerConfig>,ScreenFactory<true>); return 0;
    case Op::AllocLoginCtrl: sPrvTable->AddAllocator("LoginCtrl",NewConfigOf<EeWonCfgKind::Dialog>,DlgCtlFactory<Ck::Login>); return 0;
    case Op::AllocLoginHelpCtrl: sPrvTable->AddAllocator("LoginHelpCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<LoginHelpCtrl>); return 0;
    case Op::AllocLoginScreen: sPrvTable->AddAllocator("LoginScreen",NewConfigOf<ContainerConfig>,NewComponentOf<LoginScreen>); return 0;
    case Op::AllocLoginStatusCtrl: sPrvTable->AddAllocator("LoginStatusCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LoginStatus>); return 0;
    case Op::AllocLostPasswordCtrl: sPrvTable->AddAllocator("LostPasswordCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LostPassword>); return 0;
    case Op::AllocLostUserNameCtrl: sPrvTable->AddAllocator("LostUserNameCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LostUserName>); return 0;
    case Op::AllocMOTDCtrl: sPrvTable->AddAllocator("MOTDCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<MOTDCtrl>); return 0;
    case Op::AllocQuickPlayCtrl: sPrvTable->AddAllocator("QuickPlayCtrl",NewConfigOf<EeWonCfgKind::Dialog>,DlgCtlFactory<Ck::QuickPlay>); return 0;
    case Op::AllocRoomCtrl: sPrvTable->AddAllocator("RoomCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::Room>); return 0;
    case Op::AllocRoomLabelCtrl: sPrvTable->AddAllocator("RoomLabelCtrl",NewConfigOf<RoomLabelCtrlConfig>,NewComponentOf<RoomLabelCtrl>); return 0;
    case Op::AllocServerListCtrl: sPrvTable->AddAllocator("ServerListCtrl",NewConfigOf<ServerListCtrlConfig>,NewComponentOf<ServerListCtrl>); return 0;
    case Op::AllocUpdateAccountCtrl: sPrvTable->AddAllocator("UpdateAccountCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::UpdateAccount>); return 0;
    case Op::AllocUserListCtrl: sPrvTable->AddAllocator("UserListCtrl",NewConfigOf<UserListCtrlConfig>,UserListCtrlConfig::CompFactory); return 0;
    case Op::AllocAddFriendDialog: sPrvTable->AddAllocator("AddFriendDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::AddFriend>); return 0;
    case Op::AllocAdminActionDialog: sPrvTable->AddAllocator("AdminActionDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::AdminAction>); return 0;
    case Op::AllocBadUserDialog: sPrvTable->AddAllocator("BadUserDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::BadUser>); return 0;
    case Op::AllocChooseNetworkAdapterDialog: sPrvTable->AddAllocator("ChooseNetworkAdapterDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::ChooseAdapter>); return 0;
    case Op::AllocCreditsDialog: sPrvTable->AddAllocator("CreditsDialog",NewConfigOf<CreditsDialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::Credits>); return 0;
    case Op::AllocCreateGameDialog: sPrvTable->AddAllocator("CreateGameDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::CreateGame>); return 0;
    case Op::AllocCreateRoomDialog: sPrvTable->AddAllocator("CreateRoomDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::CreateRoom>); return 0;
    case Op::AllocDirectConnectDialog: sPrvTable->AddAllocator("DirectConnectDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::DirectConnect>); return 0;
    case Op::AllocLanNameDialog: sPrvTable->AddAllocator("LanNameDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::LanName>); return 0;
    case Op::AllocLanPortDialog: sPrvTable->AddAllocator("LanPortDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::LanPort>); return 0;
    case Op::AllocOkCancelDialog: sPrvTable->AddAllocator("OkCancelDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::OkCancel>); return 0;
    case Op::AllocPasswordDialog: sPrvTable->AddAllocator("PasswordDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::Password>); return 0;
    case Op::AllocRoomDialog: sPrvTable->AddAllocator("RoomDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::Room>); return 0;
    case Op::AllocStatusDialog: sPrvTable->AddAllocator("StatusDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::Status>); return 0;
    case Op::AllocTOUDialog: sPrvTable->AddAllocator("TOUDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::Tou>); return 0;
    case Op::AllocCrossPromotionButton: sPrvTable->AddAllocator("CrossPromotionButton",NewConfigOf<CrossPromotionButtonConfig>,NewComponentOf<CrossPromotionButton>); return 0;
    case Op::AllocImageTextButton: sPrvTable->AddAllocator("ImageTextButton",NewConfigOf<ImageTextButtonConfig>,NewComponentOf<ImageTextButton>); return 0;
    case Op::AllocMaxSplitterButton: sPrvTable->AddAllocator("MaxSplitterButton",NewConfigOf<EeWonCfgKind::MsArrowButton>,NewShellOf<EeWonShellKind::MaxSplitterButton,Direction_Up>); return 0;
    case Op::AllocGameOptionsCtrl: sPrvTable->AddAllocator("GameOptionsCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<Container>); return 0;
    case Op::AllocPlayerOptionsCtrl: sPrvTable->AddAllocator("PlayerOptionsCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<Container>); return 0;
    }
    return 0;
}

// Starts the lobby: parses WONLobby.cfg, builds the lobby screens from _LobbyMaster.cfg (over the resource table of
// the base GUI and browser types and every lobby screen, control, dialog and button type), inits the logic objects
// and derives the internet and LAN product ids; on failure stores the reason in mInitError and returns false
bool WONLobbyPrv::Init(ResourceConfigTable* input, void (*initFunc)(ComponentConfig*)) { return WonRethrow(ee_won_lobbyprv_call(this, PrvHost, EeWonLobbyPrvCall::Init, (intptr_t)input, (intptr_t)initFunc)) != 0; }
// Shuts the lobby down: stops the logic objects and releases the UDP manager and the auth context
void WONLobbyPrv::Destroy() { ee_won_lobbyprv_call(this, PrvHost, EeWonLobbyPrvCall::Destroy, 0, 0); }
// Sets the login community and the list of files to hash for authentication; with CD key checks on, loads the
// product's CD key from the registry into the auth context
void WONLobbyPrv::InitAuth(const std::wstring& loginCommunity, const char* hashFileListStr) { ee_won_lobbyprv_call(this, PrvHost, EeWonLobbyPrvCall::InitAuth, (intptr_t)&loginCommunity, (intptr_t)hashFileListStr); }
// Creates the lobby's private state: configuration, saved settings, server contexts, auth context and the account,
// dialog, staging, init, LAN, routing and ping logic objects (port 8888); members not set are left to their own
// constructors, as in the original
WONLobbyPrv::WONLobbyPrv() { ee_won_lobbyprv_call(this, PrvHost, EeWonLobbyPrvCall::Construct, 0, 0); }

}
