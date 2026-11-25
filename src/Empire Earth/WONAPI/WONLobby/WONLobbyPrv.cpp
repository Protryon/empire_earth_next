// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// The lobby's private state (WONLobbyPrv: its host, the resource table's factories) and the free functions over it:
// start-up and shutdown, the server contexts, the address and firewall state, the auth context, the UDP port, the
// screens, the event dispatch and hooks, popups and the window-thread completions (R6 WON lobby and network round 38
// merged fifteen small units here; each part is headed by its old file name).
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
#include "rust/won_lobby/cfgval.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "rust/won_net/udpmgr.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/EELobbySound.h"
#include "platform/PlatThread.h"
#include "ee/wonapi/WONRouting.h"
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
    case Op::HashListRead: ee_won_cfgval_read_strings(reinterpret_cast<const uint8_t*>(a), &p->mHashFileList); return 0;
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
    case Op::AccountLogicNew: p->mAccountLogic = new RustLogic(EeWonLogicShellKind::Account, AccountLogicMake); return 0;
    case Op::DialogLogicNew: p->mDialogLogic = new RustLogic(EeWonLogicShellKind::Dialog, DialogLogicMake); return 0;
    case Op::InitLogicNew: p->mInitLogic = new RustLogic(EeWonLogicShellKind::Init, InitLogicMake); return 0;
    case Op::RoutingLogicNew: p->mRoutingLogic = new RustLogic(EeWonLogicShellKind::Routing, RoutingLogicMake); return 0;
    case Op::StagingLogicNew: p->mStagingLogic = new RustLogic(EeWonLogicShellKind::Staging, StagingLogicMake); return 0;
    case Op::LanLogicNew: p->mLanLogic = new RustLogic(EeWonLogicShellKind::Lan, LanLogicMake); return 0;
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

// ---- LobbyLifecycle.cpp (merged into this unit, R6 WON lobby and network round 38)
// Lobby start-up: creates the lobby singleton and records why its initialization failed
// The lobby singleton and its initialization failure state.
SmartPtr<WONLobbyPrv> gLobbyData;
GUIString gLobbyInitError;
SmartPtr<Dialog> gLobbyFailureDialog;
bool gLobbyInitFailed;
void LobbyInstallEventHooks(); // below

// Creates and initializes the lobby; on failure keeps its error text, drops it and returns false
bool LobbyAddComponentLoader(ResourceConfigTable* table,void (*initFunc)(ComponentConfig*))
{
    gLobbyInitError=GetEmptyGUIStringData();
    gLobbyInitFailed=false;
    gLobbyFailureDialog=NULL;
    gLobbyData=new WONLobbyPrv;
    if(!gLobbyData->Init(table,initFunc)) {
        if(WONAPI::WindowManager::GetDefaultWindowManager()!=NULL)
            WONAPI::WindowManager::GetDefaultWindowManager()->ClearSubscriptions();
        gLobbyInitError=gLobbyData->mInitError;
        gLobbyInitFailed=true;
        gLobbyData=NULL;
        return false;
    }
    LobbyInstallEventHooks();
    return true;
}
// The SDK's WONLobby::InitAuth (26 bytes): passes the login community and the hash file list to the lobby
// when it is running
void LobbySetProductName(const std::wstring& inLoginCommunity,const char* inHashFileListStr)
{
    if(gLobbyData.get()!=NULL) gLobbyData->InitAuth(inLoginCommunity,inHashFileListStr);
}

// ---- LobbyShutdown.cpp (merged into this unit, R6 WON lobby and network round 38)
// Lobby shutdown (WON SDK entry point)
// Clears the start-up failure state, then logs out, destroys the lobby, stops its music and unloads its resources
void LobbyShutdown()
{
    gLobbyFailureDialog=NULL;
    gLobbyInitFailed=false;
    gLobbyInitError=GetEmptyGUIStringData();
    if(gLobbyData.get()==NULL)
        return;
    LobbyLogout(false);
    gLobbyData->Destroy();
    LobbyRemoveEventHooks();
    LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Stop();
    LobbyResource_UnloadResources();
    gLobbyData=NULL;
}

// ---- LobbySingletonAccess.cpp (merged into this unit, R6 WON lobby and network round 38)
// Returns the account servers the lobby uses, or NULL without a lobby
ServerContext* LobbyGetAccountServers() { return gLobbyData.get()!=NULL?gLobbyData->mAccountServers:SmartPtr<ServerContext>(); }
// Returns the authentication servers the lobby uses, or NULL without a lobby
ServerContext* LobbyGetAuthServers() { return gLobbyData.get()!=NULL?gLobbyData->mAuthServers:SmartPtr<ServerContext>(); }
// Returns the directory servers the lobby uses, or NULL without a lobby
ServerContext* LobbyGetDirServers() { return gLobbyData.get()!=NULL?gLobbyData->mDirServers:SmartPtr<ServerContext>(); }
// Returns the directory servers listing the lobby rooms, or NULL without a lobby
ServerContext* LobbyGetRoomDirServers() { return gLobbyData.get()!=NULL?gLobbyData->mRoomDirServers:SmartPtr<ServerContext>(); }
// Returns the servers used for the firewall test, or NULL without a lobby
ServerContext* LobbyGetFirewallDetectServers() { return gLobbyData.get()!=NULL?gLobbyData->mFirewallDetectServers:SmartPtr<ServerContext>(); }
// Returns the servers that offer game patches, or NULL without a lobby
ServerContext* LobbyGetPatchServers() { return gLobbyData.get()!=NULL?gLobbyData->mPatchServers:SmartPtr<ServerContext>(); }
// Makes the room listing use the main directory servers again
void LobbyResetRoomDirServers() { if(gLobbyData.get()!=NULL) gLobbyData->mRoomDirServers=gLobbyData->mDirServers; }
// Records the result of the game version check
void LobbySetVersionStatus(WONStatus status) { if(gLobbyData.get()!=NULL) gLobbyData->mVersionStatus=status; }
// Returns the result of the game version check; status 100004 without a lobby
WONStatus LobbyGetVersionStatus() { return gLobbyData.get()!=NULL?gLobbyData->mVersionStatus:100004; }
// Records the times of the system and game terms of use
void LobbySetTOUTimes(long systemTime,long gameTime)
{
    if(gLobbyData.get()!=NULL) {
        gLobbyData->mSysTOUTime=systemTime;
        gLobbyData->mGameTOUTime=gameTime;
    }
}
// Returns whether the recorded system or game terms of use are newer than the given times
bool LobbyIsTOUNewer(long systemTime,long gameTime)
{
    if(gLobbyData.get()!=NULL)
        return gLobbyData->mSysTOUTime>systemTime || gLobbyData->mGameTOUTime>gameTime;
    return false;
}
// Returns the lobby's configured network port, or 0 without a lobby
unsigned short LobbyGetLobbyPort() { return gLobbyData.get()!=NULL?gLobbyData->mLobbyPort:0; }

// ---- LobbyNetworkState.cpp (merged into this unit, R6 WON lobby and network round 38)
// Lobby address state: firewall test result, the public address seen from outside, and the address to give peers
// Forgets the firewall test result and the public address
void LobbyResetFirewallTest()
{
    if(gLobbyData.get()==NULL) return;
    gLobbyData->mFirewallTestDone=false;
    gLobbyData->mPublicAddr=IPAddr();
}
// Records the public address seen from outside
void LobbySetPublicAddress(const IPAddr& address)
{
    if(gLobbyData.get()==NULL) return;
    gLobbyData->mPublicAddr=address;
}
// Records whether the player is behind a firewall and marks the test done
void LobbySetBehindFirewall(bool behindFirewall)
{
    if(gLobbyData.get()==NULL) return;
    gLobbyData->mBehindFirewall=behindFirewall;
    gLobbyData->mFirewallTestDone=true;
}
// Returns the address to give peers on the lobby port: public unless room spec 1 or UseLocalIP, else local
IPAddr LobbyGetMyAddress(int roomSpec)
{
    IPAddr address;
    if(gLobbyData.get()!=NULL) {
        if(roomSpec!=1 && gLobbyData->mPublicAddr.IsValid() && !gLobbyData->mConfig->mUseLocalIP)
            address=gLobbyData->mPublicAddr;
        else
            address=IPAddr::GetLocalAddr();
        address.SetPort(gLobbyData->mLobbyPort);
    }
    return address;
}
// Returns the public address seen from outside, or an empty address
IPAddr LobbyGetPublicAddress()
{
    if(gLobbyData.get()!=NULL) return gLobbyData->mPublicAddr;
    return IPAddr();
}

// ---- LobbyAuthContextAccess.cpp (merged into this unit, R6 WON lobby and network round 38)
// Access to the lobby's login (auth) context
// Returns the login context; with CRC checks on, first gives it the files to hash for the community (once)
AuthContext* GetLobbyAuthContext()
{
    if(gLobbyData.get()==NULL)
        return NULL;
    if(gLobbyData->mConfig->mCRCCheck && !gLobbyData->mHasSetHashFileList) {
        gLobbyData->mHasSetHashFileList=true;
        if(gLobbyData->mHashFileList.empty())
            gLobbyData->mAuthContext->SetHashFile(gLobbyData->mLoginCommunity);
        else
            gLobbyData->mAuthContext->SetHashFileList(gLobbyData->mLoginCommunity,gLobbyData->mHashFileList);
    }
    return gLobbyData->mAuthContext.get();
}

// ---- LobbyUDPManager.cpp (merged into this unit, R6 WON lobby and network round 38)
// Binds the lobby's UDP port (pings and friend searches)
// Binds the lobby UDP port from the config (8888 by default; + the harness slot's offset, TEST ONLY), else a random
// port 10000-19999, and starts receiving (ee_won udpmgr.rs ee_won_udpmanager_bind_lobby)
void LobbyBindUDPPort()
{
    LobbyConfig* aConfig=GetLobbyConfig();
    unsigned short aPort=(unsigned short)((aConfig?aConfig->mLobbyPort:8888)+LobbyTestPortOffset());
    UDPManager* aMgr=LobbyGetUDPManager();
    LobbySetLobbyPort(ee_won_udpmanager_bind_lobby(aMgr!=NULL?aMgr->GetManagerObject():NULL,aPort));
}

// ---- LobbyNetworkAdapter.cpp (merged into this unit, R6 WON lobby and network round 38)
// Network adapter selection for the lobby; the saved adapter changes only after a successful switch
// Switches the local address to an adapter, saves the choice if new and rebinds the UDP port after a switch
void LobbySetNetworkAdapter(long address)
{
    bool changed=false;
    if(WONAPI::IPAddr::GetLocalHost()!=address) {
        changed=true;
        if(!IPAddr::SetLocalHost(address,true)) return;
    }
    if(WONAPI::IPAddr::GetLocalHost()!=LobbyGetNetworkAdapter()) LobbySaveNetworkAdapter();
    if(changed) LobbyBindUDPPort();
}

// ---- LobbyClientFlags.cpp (merged into this unit, R6 WON lobby and network round 38)
// Blocks or ignores players in the lobby lists
// Saves a player name's ignored state in the persistent settings (0x75d100)
void LobbySetNameIgnored(const GUIString&,bool);
// Blocks or unblocks a player and refreshes the player lists; does nothing unless the config's mCDKeyCheck is set
void LobbySetClientBlocked(LobbyClient* client,bool inBlocked,unsigned int roomSpec)
{
    WONLobbyPrv* data=gLobbyData.get();
    if(data==NULL || !data->mConfig->mCDKeyCheck) return;
    if(client->IsBlocked()==inBlocked) return;
    client->SetBlocked(inBlocked);
    LobbyBroadcastEvent(ClientChangedEvent(client,roomSpec,2),true);
}
// Ignores or unignores a player, remembers the name in the settings and refreshes the player lists
void LobbySetClientIgnored(LobbyClient* client,bool inIgnored,unsigned int roomSpec)
{
    if(gLobbyData.get()==NULL) return;
    if(client->IsIgnored()==inIgnored) return;
    client->SetIgnored(inIgnored);
    LobbySetNameIgnored(client->GetName(),inIgnored);
    LobbyBroadcastEvent(ClientChangedEvent(client,roomSpec,2),true);
}


// ---- LobbyScreenQueries.cpp (merged into this unit, R6 WON lobby and network round 38)
// Free lobby functions that show login and Internet screen panels and report which lobby screen is shown
// Switches the Internet screen to its lobby (rooms) tab
void LobbyShowLobbyTab() { if(gLobbyData.get()!=NULL) gLobbyData->mLobbyContainer->mInternetScreen->ShowLobbyTab(); }
// Shows the create-account panel of the login screen
void LobbyShowCreateAccount() { if(gLobbyData.get()!=NULL) gLobbyData->mLobbyContainer->mLoginScreen->ShowCreateAccountCtrl(); }
// Shows the lost-password panel of the login screen
void LobbyShowLostPassword() { if(gLobbyData.get()!=NULL) gLobbyData->mLobbyContainer->mLoginScreen->ShowLostPasswordCtrl(); }
// Shows the lost-username panel of the login screen
void LobbyShowLostUsername() { if(gLobbyData.get()!=NULL) gLobbyData->mLobbyContainer->mLoginScreen->ShowLostUsernameCtrl(); }
// Returns whether the lobby is showing the Internet screen
bool LobbyOnInternetScreen() { return gLobbyData.get()!=NULL?gLobbyData->mLobbyContainer->OnInternetScreen():false; }
// Returns whether the lobby is showing the LAN screen
bool LobbyOnLanScreen() { return gLobbyData.get()!=NULL?gLobbyData->mLobbyContainer->OnLanScreen():false; }
// Returns whether the lobby is showing the login screen
bool LobbyOnLoginScreen() { return gLobbyData.get()!=NULL?gLobbyData->mLobbyContainer->OnLoginScreen():false; }
// Returns the game type of the screen shown: Internet 2, LAN 1, direct connect 3, else 0
int LobbyGetGameScreenType()
{
    if(gLobbyData.get()==NULL) return 0;
    return gLobbyData->mLobbyContainer->GetGameScreenType();
}
// Returns the lobby's color scheme, or NULL without a lobby
ColorScheme* LobbyGetColorScheme() { return gLobbyData.get()!=NULL?gLobbyData->mLobbyContainer->GetColorScheme():NULL; }
// Returns the lobby's root window, or NULL without a lobby
Window* LobbyGetRootWindow() { return gLobbyData.get()!=NULL?gLobbyData->mLobbyContainer->GetRoot():NULL; }
// Returns the default window manager
WindowManager* GetLobbyWindow() { return WindowManager::GetDefaultWindowManager(); }
// Returns whether the lobby is suspended; false without a lobby
bool LobbyIsSuspended()
{
    if(gLobbyData.get()==NULL) return false;
    return gLobbyData->mSuspended;
}

// ---- LobbyScreenAccess.cpp (merged into this unit, R6 WON lobby and network round 38)
// Returns the chat command parser, pointed at the user list of the Internet or LAN screen showing
ChatCommandParser* LobbyGetRoomChatParser(int flags)
{
    if(gLobbyData.get()==NULL) return NULL;
    ChatCommandParser* parser=gLobbyData->mChatCommandParser.get();
    LobbyClientList* clients=NULL;
    if(gLobbyData->mLobbyContainer->OnInternetScreen())
        clients=reinterpret_cast<LobbyClientList*>(gLobbyData->mRoutingLogic->Call(EeWonRouteLogicCall::ClientList));
    else if(gLobbyData->mLobbyContainer->OnLanScreen())
        clients=reinterpret_cast<LobbyClientList*>(gLobbyData->mLanLogic->Call(EeWonLanCall::ClientList));
    parser->SetClientList(clients,flags);
    return parser;
}
// Returns the Internet, LAN or login screen, whichever is showing, or NULL
Component* LobbyGetCurrentScreen()
{
    if(gLobbyData.get()==NULL) return NULL;
    if(LobbyOnInternetScreen()) return gLobbyData->mLobbyContainer->mInternetScreen.get();
    if(LobbyOnLanScreen()) return gLobbyData->mLobbyContainer->mLanScreen.get();
    if(LobbyOnLoginScreen()) return gLobbyData->mLobbyContainer->mLoginScreen.get();
    return NULL;
}
// Switches the lobby to the Internet screen and tells its controls they were entered
void LobbyShowInternetScreen()
{
    if(gLobbyData.get()==NULL)
        return;
    gLobbyData->mLobbyContainer->ShowInternetScreen();
    LobbyEvent* event=new LobbyEvent(100014);
    gLobbyData->mLobbyContainer->mInternetScreen->BroadcastEventDown(event);
}
// Switches the lobby to the LAN screen and tells its controls they were entered
void LobbyShowLanScreen()
{
    if(gLobbyData.get()==NULL)
        return;
    gLobbyData->mLobbyContainer->ShowLanScreen();
    LobbyEvent* event=new LobbyEvent(100014);
    gLobbyData->mLobbyContainer->mLanScreen->BroadcastEventDown(event);
}
// Returns the lobby's root container, or NULL before the lobby exists
Container* GetLobbyRoot()
{
    return gLobbyData.get()!=NULL?gLobbyData->mLobbyContainer:SmartPtr<LobbyContainer>();
}

// ---- LobbyDispatch.cpp (merged into this unit, R6 WON lobby and network round 38)
// Lobby event dispatch: broadcast to the lobby screens, send to one component, or hand to the lobby logic
// LobbyLogicEventHook (0x7590e0, cdecl) receives the event while the caller still holds it
void LobbyLogicEventHook(ComponentEvent*);
// Broadcasts a plain lobby event of the given type to the lobby screens
void LobbyBroadcastLobbyEvent(int type,bool broadcast)
{
    if(gLobbyData.get()==NULL) return;
    SmartPtr<LobbyEvent> event = new LobbyEvent(type);
    gLobbyData->mLobbyContainer->FireBroadcastEvent(event.get(),broadcast);
}
// Sends an event straight to one component, from the lobby container by default
void LobbySendEventTo(ComponentEvent* event,Component* target)
{
    SmartPtr<ComponentEvent> eventOwner=event;
    if(gLobbyData.get()!=NULL && target!=NULL) {
        if(event->mComponent==NULL)
            event->SetComponent(gLobbyData->mLobbyContainer.get());
        event->mBroadcast=true;
        target->HandleComponentEvent(event);
    }
}
// Hands an event to the lobby logic, from the lobby container by default
void LobbyDispatchEvent(ComponentEvent* event)
{
    SmartPtr<ComponentEvent> eventOwner=event;
    if(gLobbyData.get()!=NULL) {
        if(event->mComponent==NULL)
            event->SetComponent(gLobbyData->mLobbyContainer.get());
        LobbyLogicEventHook(event);
    }
}
// Shows the login screen and tells it that it was opened
void LobbyShowLoginScreen()
{
    if(gLobbyData.get()==NULL) return;
    gLobbyData->mLobbyContainer->ShowLoginScreen();
    LobbyEvent* event=new LobbyEvent(100014);
    gLobbyData->mLobbyContainer->mLoginScreen->BroadcastEventDown(event);
}
// Broadcasts an event from the lobby container to the lobby screens (emitted at 0x759100)

void LobbyBroadcastEvent(ComponentEvent* event,bool sendNow)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mLobbyContainer->FireBroadcastEvent(event,sendNow);
}

// ---- LobbyEventHooks.cpp (merged into this unit, R6 WON lobby and network round 38)
// Installs and removes the lobby's logic event listener and its timer

// Passes the lobby timer tick to the lobby logic
void LobbyTimerCallback(int)
{
    if(gLobbyData.get()!=NULL)
        gLobbyData->mLogicMgr.DispatchTimerEvent();
}

// Routes the lobby container's events to the lobby logic and starts the lobby timer
void LobbyInstallEventHooks()
{
    if(gLobbyData.get()==NULL)
        return;
    LobbyAddLogicListener(gLobbyData->mLobbyContainer.get());
    gLobbyData->mTimerEvent=new UserEvent(LobbyTimerCallback,0);
    WONAPI::WindowManager::GetDefaultWindowManager()->AddTimerEvent(gLobbyData->mTimerEvent.get());
}

// Detaches the lobby logic from the lobby container, stops the timer and clears window subscriptions
void LobbyRemoveEventHooks()
{
    if(gLobbyData.get()==NULL)
        return;
    gLobbyData->mLobbyContainer->AddListener(NULL);
    WindowManager* manager=WONAPI::WindowManager::GetDefaultWindowManager();
    if(manager!=NULL) {
        manager->RemoveTimerEvent(gLobbyData->mTimerEvent.get());
        manager->ClearSubscriptions();
    }
}

// Makes a component send its events to the lobby logic
void LobbyAddLogicListener(Component* component)
{
    if(component!=NULL)
        component->AddListener(new ComponentListenerFunc(LobbyLogicEventHook));
}

// Hands a component event to the lobby logic
void LobbyLogicEventHook(ComponentEvent* event)
{
    if(gLobbyData.get()!=NULL)
        gLobbyData->mLogicMgr.HandleComponentEvent(event);
}

// ---- LobbyPopupSupport.cpp (merged into this unit, R6 WON lobby and network round 38)
// Opens a popup over the lobby at the given position and lets it receive lobby logic events
void LobbyOpenPopup(Component* popup,int flags,int x,int y)
{
    if(gLobbyData.get()==NULL)
        return;
    Window* window=LobbyGetRootWindow();
    if(window!=NULL) {
        LobbyAddLogicListener(popup);
        PopupParams params(popup,flags,gLobbyData->mLobbyContainer.get(),x,y);
        window->OpenPopup(params);
    }
}

// ---- LobbyCompletion.cpp (merged into this unit, R6 WON lobby and network round 38)
// Runs lobby operation completions on the window thread (file name inferred: no __FILE__)
// The original object of 0x757d70..0x757f24 is unknown: it may be part of the neighbouring chat or config unit
class LobbyCompletionEvent : public WindowEventBase {
public:
    LobbyCompletionEvent(LobbyCompletionBase* inCompletion, AsyncOp* inOp)
        : mCompletion(inCompletion), mOp(inOp) {}
    virtual void Deliver();
    SmartPtr<LobbyCompletionBase> mCompletion; //10
    AsyncOpPtr mOp; //14
};
// Runs the completion at once on the window thread, else posts it there as a window event
void LobbyCompletionBase::Complete(AsyncOpPtr theParam)
{
    DWORD aThreadId = WONAPI::WindowManager::GetThreadId();
    if (PlatCurrentThreadId() == aThreadId)
        this->Run(theParam.get());
    else {
        WindowManager* aManager = WONAPI::WindowManager::GetDefaultWindowManager();
        if (aManager != NULL)
            aManager->PostEvent(new LobbyCompletionEvent(this, theParam.get()), true);
    }
}
// Runs the posted completion for its operation on the window thread
void LobbyCompletionEvent::Deliver()
{
    this->mCompletion->Run(this->mOp.get());
}
// The implicit destructor (0x757ea0) releases mOp, then mCompletion, then the WindowEventBase part

// ---- WONLobbyPrvLifetime.cpp (merged into this unit, R6 WON lobby and network round 38)
// Destroys the lobby's private state (members only)
WONLobbyPrv::~WONLobbyPrv() {}
}
