// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// The lobby's private state (WONLobbyPrv: its host, the resource table's factories) and the free functions over it:
// start-up and shutdown, the server contexts, the address and firewall state, the UDP port, the screens, the event
// dispatch and hooks, popups, the window-thread completions, the error screen, the run loop and the logic manager's host,
// the lobby's config and the start-up logic's host (R6 WON lobby and network rounds 38 and 40 merged small units here;
// each part is headed by its old file name).
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
#include "ee/wonapi/WONLobbyLan.h"
#include "ee/wonapi/WONLobbyStaging.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/WONLobbyControls.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONLobbyImageButton.h"
#include "ee/wonapi/WONLobbyCrossPromotion.h"
#include "ee/wonapi/WONGUIShell.h"
#include "rust/won_net/status_codes.h"
#include "ee/wonapi/WONLobbyGuard.h"
#include "rust/won_lobby/lobbyprv.h"
#include "rust/won_lobby/cfgval.h"
#include "rust/won_lobby/lobbyconf.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "rust/won_net/udpmgr.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "rust/won_gui/soundobj.h" // the lobby's sounds
#include "platform/PlatThread.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "rust/won_lobby/errscreen.h"
#include "rust/won_lobby/lobbyrun.h"
#include "platform/PlatString.h"
#include "rust/won_lobby/flow.h"
#include "rust/won_lobby/logics.h"
#include "rust/won_net/hostop.h"
#include "rust/won_net/httpdoc.h"
#include "rust/won_net/httpget.h"
#include "rust/won_net/reqdir.h"
#include "rust/won_net/reqops.h"
using namespace WONAPI;


// The resource table's factories: a lobby control kind (the one-line subclasses LoginStatusCtrl, CreateAccountCtrl,
// ... were these shells made with their kinds), a root screen, a dialog class
template <EeWonLobbyCtlKind K> static Component* CtlFactory() { return new LobbyRustCtrl(K); }
template <EeWonLobbyCtlKind K> static Component* DlgCtlFactory() { return new LobbyRustDialogCtrl(K); }
template <bool Lan> static Component* ScreenFactory() { return new RootScreen(Lan); }
typedef EeWonLobbyCtlKind Ck;
typedef EeWonLobbyPrvField PF;

namespace WONAPI {

// the methods' statements (lobbyprv.rs composes them, R6 WON round 33); Init's locals are these statics
static ConfigParser* sPrvParser;
static SmartPtr<ContainerConfig> sPrvComponentConfig;
static ResourceConfigTablePtr sPrvTable;
// a text the Rust gives (its bytes and length)
static std::string PrvText(intptr_t a) { const uintptr_t* t = reinterpret_cast<const uintptr_t*>(a); return std::string((const char*)t[0], (size_t)t[1]); }
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
    case Op::ComponentConfigNew: sPrvComponentConfig = new ContainerConfig; return 0;
    case Op::LobbyContainerNew: p->mLobbyContainer = new LobbyContainer; return 0;
    case Op::SetResourceFolder: ComponentConfig::SetResourceFolder(WONAPICore::GetDefaultFileDirectory() + PrvText(a)); return 0;
    case Op::DefineAdd: sPrvParser->AddDefine(PrvText(a), PrvText(a + 2 * sizeof(uintptr_t)), false); return 0;
    case Op::ReadMaster: return WON_GUARDED(sPrvComponentConfig->ReadConfig_Unsafe(p->mLobbyContainer.get(), "_LobbyMaster.cfg", sPrvTable.get(), sPrvParser));
    case Op::ErrorFromConfig: p->mInitError = sPrvComponentConfig->mConfigError; return 0;
    case Op::LogicMgrInit: return WON_GUARDED(PrvConfigTry(p, [&] { p->mLogicMgr.Init(sPrvComponentConfig.get()); }));
    case Op::InitFuncRun: return WON_GUARDED(PrvConfigTry(p, [&] { reinterpret_cast<void (*)(ComponentConfig*)>(a)(sPrvComponentConfig.get()); }));
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
    case Op::CommunitySet: p->mLoginCommunity = *reinterpret_cast<const std::wstring*>(a); return 0;
    case Op::AuthSetCommunity: p->mAuthContext->SetCommunity(p->mLoginCommunity); return 0;
    case Op::CdKeySetProduct: p->mCDKey.SetProductString(PrvText(a)); return 0;
    case Op::CdKeyLoad: p->mCDKey.LoadFromRegistry(); return 0;
    case Op::AuthSetCdKey: p->mAuthContext->SetCDKey(p->mLoginCommunity, p->mCDKey); return 0;
    case Op::CurGameTypeClear: p->mCurGameType = (LobbyGameType)0; return 0;
    case Op::SysTouPath: p->mSysTOUPath = WONAPICore::GetDefaultFileDirectory() + "_wonsystou_m.txt"; return 0;
    case Op::GameTouPath: p->mGameTOUPath = WONAPICore::GetDefaultFileDirectory() + "_wongametou_m.txt"; return 0;
    case Op::ConfigNew: p->mConfig = new LobbyConfig; return (intptr_t)p->mConfig->mConf;
    case Op::PersistentDataNew: p->mPersistentData = new LobbyPersistentData; return 0;
    case Op::ChatParserNew: p->mChatCommandParser = new ChatCommandParser; return 0;
    case Op::UdpNew: p->mUDPManager = new UDPManager; return 0;
    case Op::AccountServersNew: p->mAccountServers = new ServerContext; return 0;
    case Op::AuthServersNew: p->mAuthServers = new ServerContext; return 0;
    case Op::DirServersNew: p->mDirServers = new ServerContext; return 0;
    case Op::RoomDirServersNew: p->mRoomDirServers = new ServerContext; return 0;
    case Op::FirewallServersNew: p->mFirewallDetectServers = new ServerContext; return 0;
    case Op::PatchServersNew: p->mPatchServers = new ServerContext; return 0;
    case Op::AuthContextNew: p->mAuthContext = new AuthContext; return 0;
    case Op::AuthSetServers: p->mAuthContext->SetServerContext(p->mAuthServers.get()); return 0;
    case Op::AccountLogicNew: p->mAccountLogic = new RustLogic(EeWonLogicShellKind::Account, AccountLogicMake); return 0;
    case Op::DialogLogicNew: p->mDialogLogic = new RustLogic(EeWonLogicShellKind::Dialog, DialogLogicMake); return 0;
    case Op::InitLogicNew: p->mInitLogic = new RustLogic(EeWonLogicShellKind::Init, InitLogicMake); return 0;
    case Op::RoutingLogicNew: p->mRoutingLogic = new RustLogic(EeWonLogicShellKind::Routing, RoutingLogicMake); return 0;
    case Op::StagingLogicNew: p->mStagingLogic = new RustLogic(EeWonLogicShellKind::Staging, StagingLogicMake); return 0;
    case Op::LanLogicNew: p->mLanLogic = new RustLogic(EeWonLogicShellKind::Lan, LanLogicMake); return 0;
    case Op::PingLogicNew: p->mPingLogic = new RustLogic(EeWonLogicShellKind::Ping, PingLogicMake); return 0;
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
    case Op::AllocChatCtrl: sPrvTable->AddAllocator("ChatCtrl",NewConfigOf<EeWonCfgKind::LobbyChat>,NewComponentOf<ChatCtrl>); return 0;
    case Op::AllocCreateAccountCtrl: sPrvTable->AddAllocator("CreateAccountCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::CreateAccount>); return 0;
    case Op::AllocDirectConnectCtrl: sPrvTable->AddAllocator("DirectConnectCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::DirectConnect>); return 0;
    case Op::AllocFriendsListCtrl: sPrvTable->AddAllocator("FriendsListCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::Friends>); return 0;
    case Op::AllocGameBrowserCtrl: sPrvTable->AddAllocator("GameBrowserCtrl",NewConfigOf<EeWonCfgKind::LobbyGameBrowser>,NewComponentOf<GameBrowserCtrl>); return 0;
    case Op::AllocGameStagingCtrl: sPrvTable->AddAllocator("GameStagingCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::Staging>); return 0;
    case Op::AllocGameUserListCtrl: sPrvTable->AddAllocator("GameUserListCtrl",NewConfigOf<EeWonCfgKind::LobbyGameUserList>,CtlFactory<Ck::UserList>); return 0;
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
    case Op::AllocRoomLabelCtrl: sPrvTable->AddAllocator("RoomLabelCtrl",NewConfigOf<EeWonCfgKind::LobbyRoomLabel>,NewComponentOf<RoomLabelCtrl>); return 0;
    case Op::AllocServerListCtrl: sPrvTable->AddAllocator("ServerListCtrl",NewConfigOf<EeWonCfgKind::LobbyServerList>,NewComponentOf<ServerListCtrl>); return 0;
    case Op::AllocUpdateAccountCtrl: sPrvTable->AddAllocator("UpdateAccountCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::UpdateAccount>); return 0;
    case Op::AllocUserListCtrl: sPrvTable->AddAllocator("UserListCtrl",NewConfigOf<EeWonCfgKind::LobbyUserList>,CtlFactory<Ck::Users>); return 0;
    case Op::AllocAddFriendDialog: sPrvTable->AddAllocator("AddFriendDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::AddFriend>); return 0;
    case Op::AllocAdminActionDialog: sPrvTable->AddAllocator("AdminActionDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::AdminAction>); return 0;
    case Op::AllocBadUserDialog: sPrvTable->AddAllocator("BadUserDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::BadUser>); return 0;
    case Op::AllocChooseNetworkAdapterDialog: sPrvTable->AddAllocator("ChooseNetworkAdapterDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::ChooseAdapter>); return 0;
    case Op::AllocCreditsDialog: sPrvTable->AddAllocator("CreditsDialog",NewConfigOf<EeWonCfgKind::LobbyCredits>,NewLobbyDialog<EeWonLobbyDlgKind::Credits>); return 0;
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
    case Op::AllocCrossPromotionButton: sPrvTable->AddAllocator("CrossPromotionButton",NewConfigOf<EeWonCfgKind::LobbyCrossPromotion>,NewComponentOf<CrossPromotionButton>); return 0;
    case Op::AllocImageTextButton: sPrvTable->AddAllocator("ImageTextButton",NewConfigOf<EeWonCfgKind::LobbyImageText>,NewComponentOf<ImageTextButton>); return 0;
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
    ee_won_sound_call((EeWonSound*)LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound).get(), EeWonSoundCall::Stop, 0);
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
// the plain members are lobbyprv.rs's since R6 WON lobby and network round 38 (each answers as without a lobby when
// there is none: the version status 100004, the rest 0 and false; a set then does nothing). InitLogic's setters of
// them (the version status, the terms' times, the firewall test's) are its Rust's since round 39 (netlogicops.rs prv_op)
// Returns whether the recorded system or game terms of use are newer than the given times
bool LobbyIsTOUNewer(long systemTime,long gameTime) { return ee_won_lobbyprv_tou_newer(systemTime, gameTime); }
// Returns the lobby's network port
unsigned short LobbyGetLobbyPort() { return (unsigned short)ee_won_lobbyprv_get(PF::LobbyPort); }

// ---- LobbyNetworkState.cpp (merged into this unit, R6 WON lobby and network round 38)
// Lobby address state: firewall test result, the public address seen from outside, and the address to give peers
// Records the public address seen from outside
void LobbySetPublicAddress(const IPAddr& address) { ee_won_lobbyprv_set_public_addr(address.GetValue()); }
// Returns the address to give peers on the lobby port: public unless room spec 1 or UseLocalIP, else local
IPAddr LobbyGetMyAddress(int roomSpec)
{ return IPAddr::FromValue(ee_won_lobbyprv_my_address(roomSpec)); }
// Returns the public address seen from outside, or an empty address
IPAddr LobbyGetPublicAddress() { return IPAddr::FromValue(ee_won_lobbyprv_public_addr()); }

// ---- LobbyUDPManager.cpp (merged into this unit, R6 WON lobby and network round 38)
// Binds the lobby's UDP port (pings and friend searches)
// Binds the lobby UDP port from the config (8888 by default; + the harness slot's offset, TEST ONLY), else a random
// port 10000-19999, and starts receiving (ee_won udpmgr.rs ee_won_udpmanager_bind_lobby)
void LobbyBindUDPPort()
{
    EeWonLobbyConf* aConfig=ee_won_lobbyconf_current();
    unsigned short aPort=(unsigned short)((aConfig?ee_won_lobbyconf_num(aConfig,EeWonLobbyConfNum::LobbyPort):8888)+LobbyTestPortOffset());
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
    if(gLobbyData.get()==NULL || !ee_won_lobbyconf_num(ee_won_lobbyconf_current(),EeWonLobbyConfNum::CdKeyCheck)) return;
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
void LobbyShowLobbyTab() { if(gLobbyData.get()!=NULL) gLobbyData->mLobbyContainer->Internet()->ShowLobbyTab(); }
// Shows the create-account panel of the login screen
void LobbyShowCreateAccount() { if(gLobbyData.get()!=NULL) gLobbyData->mLobbyContainer->Login()->ShowCreateAccountCtrl(); }
// Shows the lost-password panel of the login screen
void LobbyShowLostPassword() { if(gLobbyData.get()!=NULL) gLobbyData->mLobbyContainer->Login()->ShowLostPasswordCtrl(); }
// Shows the lost-username panel of the login screen
void LobbyShowLostUsername() { if(gLobbyData.get()!=NULL) gLobbyData->mLobbyContainer->Login()->ShowLostUsernameCtrl(); }
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
bool LobbyIsSuspended() { return ee_won_lobbyprv_get(EeWonLobbyPrvField::Suspended) != 0; } // (lobbyprv.rs's)

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
    if(LobbyOnInternetScreen()) return gLobbyData->mLobbyContainer->Internet();
    if(LobbyOnLanScreen()) return gLobbyData->mLobbyContainer->Lan();
    if(LobbyOnLoginScreen()) return gLobbyData->mLobbyContainer->Login();
    return NULL;
}
// Switches the lobby to the Internet screen and tells its controls they were entered
void LobbyShowInternetScreen()
{
    if(gLobbyData.get()==NULL)
        return;
    gLobbyData->mLobbyContainer->ShowInternetScreen();
    LobbyEvent* event=new LobbyEvent(100014);
    gLobbyData->mLobbyContainer->Internet()->BroadcastEventDown(event);
}
// Switches the lobby to the LAN screen and tells its controls they were entered
void LobbyShowLanScreen()
{
    if(gLobbyData.get()==NULL)
        return;
    gLobbyData->mLobbyContainer->ShowLanScreen();
    LobbyEvent* event=new LobbyEvent(100014);
    gLobbyData->mLobbyContainer->Lan()->BroadcastEventDown(event);
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
        if(event->mEv.comp==0)
            event->SetComponent(gLobbyData->mLobbyContainer.get());
        event->mEv.broadcast=1;
        target->HandleComponentEvent(event);
    }
}
// Hands an event to the lobby logic, from the lobby container by default
void LobbyDispatchEvent(ComponentEvent* event)
{
    SmartPtr<ComponentEvent> eventOwner=event;
    if(gLobbyData.get()!=NULL) {
        if(event->mEv.comp==0)
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
    gLobbyData->mLobbyContainer->Login()->BroadcastEventDown(event);
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
    gLobbyData->mTimerEvent=NewUserEvent(LobbyTimerCallback,0);
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
// Destroys the lobby's private state: its members, the plain ones the Rust's
WONLobbyPrv::~WONLobbyPrv() { ee_won_lobbyprv_drop(this); }

// ---- LobbyErrorScreen.cpp (merged into this unit, R6 WON lobby and network round 38)
// The lobby's top container, or the "Lobby Error" dialog shown when the lobby failed to start
// the error dialog's locals (errscreen.rs builds it over these statements)
static SmartPtr<Dialog> sErrDialog;
static SmartPtr<Component> sErrBackground;
static SmartPtr<Label> sErrLabel;
static GUIString sErrText;
static GUIString sErrOkText;
static SmartPtr<Button> sErrButton;
static CenterLayout* sErrVertical;
static intptr_t ErrorScreenHost(EeWonErrScreenOp inOp)
{
    typedef EeWonErrScreenOp Op;
    switch (inOp) {
    case Op::PrvExists: return gLobbyData.get() != NULL ? 1 : 0;
    case Op::PrvContainer: return (intptr_t)static_cast<Container*>(gLobbyData->mLobbyContainer.get());
    case Op::InitFailed: return gLobbyInitFailed ? 1 : 0;
    case Op::FailureExists: return gLobbyFailureDialog.get() != NULL ? 1 : 0;
    case Op::FailureGet: return (intptr_t)static_cast<Container*>(gLobbyFailureDialog.get());
    case Op::DialogNew: sErrDialog = new Dialog; return 0;
    case Op::DialogNoEscape: sErrDialog->SetEndOnEscape(0); return 0;
    case Op::BackgroundNew: sErrBackground = WONShellNew(EeWonShellKind::MsDlgBack); return 0;
    case Op::LabelNew: sErrLabel = new Label; return 0;
    case Op::LabelWrap: sErrLabel->SetWrap(true); return 0;
    case Op::LabelCenter: sErrLabel->SetHorzAlign(HorzAlign_Center); return 0;
    case Op::LabelFit: sErrLabel->SetAdjustHeightToFitText(true); return 0;
    case Op::TextNew: sErrText = GUIString("Lobby Error\n\n"); return 0;
    case Op::TextAppendError: sErrText.append(gLobbyInitError); return 0;
    case Op::LabelSetText: sErrLabel->SetText(sErrText, true); return 0;
    case Op::OkTextNew: sErrOkText = GUIString("Ok"); return 0;
    case Op::ButtonNew: sErrButton = static_cast<Button*>(WONShellNew(EeWonShellKind::MsButton, (intptr_t)&sErrOkText)); return 0;
    case Op::ButtonFlags: sErrButton->SetButtonFlags(0x100, true); return 0;
    case Op::ButtonSize: sErrButton->SetSizeAtLeast(100, 0); return 0;
    case Op::ButtonFocus: sErrButton->RequestFocus(); return 0;
    case Op::LayoutBackground: sErrDialog->Fn2F6140(sErrBackground.get(), 3, sErrDialog.get(), 0, 0, 0, 0, 0); return 0;
    case Op::LayoutLabel: sErrDialog->Fn2F6140(sErrLabel.get(), 0x1001, sErrDialog.get(), 10, 0, -20, 0, 0); return 0;
    case Op::CenterButton: sErrDialog->AddChildLayout(new CenterLayout(sErrDialog.get(), sErrButton.get(), true, false), 0); return 0;
    case Op::LayoutButton: sErrDialog->Fn2F6140(sErrButton.get(), 0x200, sErrLabel.get(), 0, 20, 0, 0, 0); return 0;
    case Op::VerticalNew: sErrVertical = new CenterLayout(sErrDialog.get(), sErrLabel.get(), false, true); return 0;
    case Op::VerticalAdd: sErrVertical->Add(sErrButton.get()); return 0;
    case Op::VerticalLayout: sErrDialog->AddChildLayout(sErrVertical, 0); return 0;
    case Op::AddBackground: sErrDialog->AddChild(sErrBackground.get()); return 0;
    case Op::AddLabel: sErrDialog->AddChild(sErrLabel.get()); return 0;
    case Op::AddButton: sErrDialog->AddChild(sErrButton.get()); return 0;
    case Op::FailureSet: gLobbyFailureDialog = sErrDialog; return 0;
    case Op::ButtonDrop: sErrButton = NULL; return 0;
    case Op::OkTextDrop: sErrOkText = GUIString(); return 0;
    case Op::TextDrop: sErrText = GUIString(); return 0;
    case Op::LabelDrop: sErrLabel = NULL; return 0;
    case Op::BackgroundDrop: sErrBackground = NULL; return 0;
    case Op::DialogDrop: sErrDialog = NULL; return 0;
    }
    return 0;
}
// Returns the lobby container; after a failed start builds once and returns a dialog showing the error and Ok (the
// choices and the order are ee_won_lobby errscreen.rs's, R6 WON lobby and network round 35)
Container* LobbyGetContainer() { return reinterpret_cast<Container*>(ee_won_lobby_error_container(ErrorScreenHost)); }

// ---- LobbyRunState.cpp (merged into this unit, R6 WON lobby and network round 38)
// Running the lobby: start, modal message loop, exit status, suspend and resume (their bodies are Rust:
// ee_won_lobby lobbyrun.rs, R6 WON round 32)
void LobbyNetSuspend(); // calls the network logic's virtual at +0x44
void LobbyNetResume(); // calls the network logic's virtual at +0x48

// the functions' statements (lobbyrun.rs composes them, R6 WON round 32)
static intptr_t RunHost(EeWonRunOp inOp, intptr_t a)
{
    typedef EeWonRunOp Op;
    switch (inOp) {
    case Op::InitFailedWithDialog: return gLobbyInitFailed && gLobbyFailureDialog.get() != NULL;
    case Op::FailureWindow: return (intptr_t)gLobbyFailureDialog->GetRootWindow();
    case Op::WindowVisible: return reinterpret_cast<Window*>(a)->PlatformIsVisible() ? 1 : 0;
    case Op::WindowRunFailureDialog: reinterpret_cast<Window*>(a)->RunDialog(gLobbyFailureDialog.get()); return 0;
    case Op::HasData: return gLobbyData.get() != NULL;
    case Op::LobbyWindow: return (intptr_t)GetLobbyWindow();
    case Op::MessageLoop: reinterpret_cast<WindowManager*>(a)->MessageLoop(); return 0;
    case Op::ExitMessageLoop: reinterpret_cast<WindowManager*>(a)->ExitMessageLoop(); return 0;
    case Op::BroadcastLobbyEvent: LobbyBroadcastLobbyEvent((int)a, true); return 0;
    case Op::PingsToDo: return LobbyGetNumPingsToDo();
    case Op::KillPings: LobbyKillPings(1); return 0;
    case Op::NetSuspend: LobbyNetSuspend(); return 0;
    case Op::MusicOn: return LobbyGetLobbyMusic() ? 1 : 0;
    case Op::NetResume: LobbyNetResume(); return 0;
    case Op::InitLogicInit: gLobbyData->mInitLogic->Call(EeWonLogicCall::InitInit); return 0;
    }
    return 0;
}

int LobbyRun() { return (int)ee_won_lobbyrun_call(RunHost, EeWonRunCall::Run, 0); }
void LobbyExit(int status) { ee_won_lobbyrun_call(RunHost, EeWonRunCall::Exit, status); }
void LobbySuspend() { ee_won_lobbyrun_call(RunHost, EeWonRunCall::Suspend, 0); }
void LobbyResume() { ee_won_lobbyrun_call(RunHost, EeWonRunCall::Resume, 0); }
// WON SDK lobby entry point: starts the lobby's start-up sequence (connect and log in)
void LobbyStart() { ee_won_lobbyrun_call(RunHost, EeWonRunCall::Start, 0); }

// ---- LobbyLogicMgr.cpp (merged into this unit, R6 WON lobby and network round 38)
// The lobby's logic manager: one list of logic objects that it initializes, kills and passes events to (the list and
// every method are ee_won_lobby logicmgr.rs's since R6 WON lobby and network round 35; this is its host)
// the manager's host (logicmgr.rs): one statement each on a logic or an event; a logic's virtual runs under WonGuard
static intptr_t LogicMgrHost(EeWonLogicMgrOp inOp, intptr_t a, intptr_t b)
{
    typedef EeWonLogicMgrOp Op;
    LobbyLogic* aLogic = reinterpret_cast<LobbyLogic*>(a);
    ComponentEvent* aClick = reinterpret_cast<ComponentEvent*>(a);
    switch (inOp) {
    case Op::AddRef: aLogic->AddRef(); return 0;
    case Op::Release: aLogic->Release(); return 0;
    case Op::InitComponent: return WON_GUARDED_V(aLogic->InitComponent(reinterpret_cast<ComponentConfig*>(b)));
    case Op::Kill: return WON_GUARDED_V(aLogic->Kill());
    case Op::HandleControlEvent: return WON_GUARDED_V(aLogic->HandleControlEvent((int)b));
    case Op::HandleComponentEvent: return WON_GUARDED_V(aLogic->HandleComponentEvent(reinterpret_cast<ComponentEvent*>(b)));
    case Op::Reset: return WON_GUARDED_V(aLogic->Reset());
    case Op::ClickItem: return aClick->mEv.item;
    case Op::ClickButton: return aClick->mEv.button;
    case Op::IsMenuItem: return dynamic_cast<MenuItem*>(reinterpret_cast<ListItem*>(a)) != NULL ? 1 : 0;
    case Op::MenuId: return dynamic_cast<MenuItem*>(reinterpret_cast<ListItem*>(a))->mMenuId;
    case Op::ControlId: return reinterpret_cast<ComponentEvent*>(a)->GetControlId();
    }
    return 0;
}

// The constructor (0x766b30) only builds the empty logic list; the class has no vtable (0xc bytes)
LobbyLogicMgr::LobbyLogicMgr() : mRust(ee_won_logicmgr_new(LogicMgrHost)) {}
LobbyLogicMgr::~LobbyLogicMgr() { ee_won_logicmgr_free(this->mRust); }
// Turns left clicks on menu items and button presses into control events; passes other events to the logics
void LobbyLogicMgr::HandleComponentEvent(ComponentEvent* e) { this->Call(EeWonLogicMgrCall::HandleComponentEvent, (intptr_t)e, e->mEv.kind); }

// ---- LobbyContainer.cpp (merged into this unit, R6 WON lobby/net round 40)
// The lobby's top container: switches between the login, Internet and LAN screens (lobbyscreens.rs: the screens and
// the methods; these are its statements)
static const char kLobbyContainerFile[] = "C:\\EEX\\Empire Earth\\WONAPI\\WONLobby\\LobbyContainer.cpp"; // its __FILE__
template<class T> static intptr_t HeldScreen(ComponentConfig* inConfig, const char* inName, int inLine)
{
    SmartPtr<T> aScreen;
    SafeGetComponent(inConfig, aScreen, inName, kLobbyContainerFile, inLine);
    if (aScreen.get() != NULL) aScreen->AddRef();
    return (intptr_t)static_cast<Component*>(aScreen.get());
}
// a screen lobbyscreens.rs holds (as its Component*)
static Component* Scr(intptr_t a) { return reinterpret_cast<Component*>(a); }
static intptr_t ScreensHost(void* inContainer, EeWonLobbyScreensOp inOp, intptr_t a, intptr_t b)
{
    typedef EeWonLobbyScreensOp Op;
    LobbyContainer* c = static_cast<LobbyContainer*>(inContainer);
    ComponentConfig* aConfig = reinterpret_cast<ComponentConfig*>(a);
    switch (inOp) {
    case Op::GetLogin: return WON_GUARDED(HeldScreen<LoginScreen>(aConfig, "LoginScreen", 37));
    case Op::GetRoot: return WON_GUARDED(HeldScreen<RootScreen>(aConfig, b ? "LanScreen" : "InternetScreen", b ? 39 : 38));
    case Op::AddScreen: c->AddScreen(Scr(a)); return 0;
    case Op::ShowScreen: c->ShowScreen(Scr(a), true); return 0;
    case Op::CurScreen: return (intptr_t)c->mCurScreen;
    case Op::ShowMotd: static_cast<LoginScreen*>(Scr(a))->ShowMOTD(); return 0;
    case Op::OnLanTab: return static_cast<RootScreen*>(Scr(a))->OnLanTab() ? 1 : 0;
    case Op::OnDirectConnectTab: return static_cast<RootScreen*>(Scr(a))->OnDirectConnectTab() ? 1 : 0;
    case Op::Release: Scr(a)->Release(); return 0;
    }
    return 0;
}
intptr_t LobbyContainer::SC(EeWonLobbyScreensCall inCall, intptr_t a) { return WonRethrow(ee_won_lobbyscreens_call(this, ScreensHost, inCall, a)); }
LoginScreen* LobbyContainer::Login() { return static_cast<LoginScreen*>(Scr(this->SC(EeWonLobbyScreensCall::Screen, 0))); }
RootScreen* LobbyContainer::Internet() { return static_cast<RootScreen*>(Scr(this->SC(EeWonLobbyScreensCall::Screen, 1))); }
RootScreen* LobbyContainer::Lan() { return static_cast<RootScreen*>(Scr(this->SC(EeWonLobbyScreensCall::Screen, 2))); }
// Finds and adds the login, Internet and LAN screens and shows the login one
void LobbyContainer::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType==2) this->SC(EeWonLobbyScreensCall::Init, (intptr_t)static_cast<ComponentConfigInit&>(inInit).mConfig);
}

// ---- LobbyConfig.cpp (merged into this unit, R6 WON lobby/net round 40)
// The lobby's configuration: its values, its LOBBYDEFINE lines and its checks are the Rust record's (lobbyconf.rs);
// these are ConfigObject's hooks over it
std::string LobbyConfig::mDefaultFileName("WONLobby.cfg");
LobbyConfig::LobbyConfig() : mConf(ee_won_lobbyconf_new(this->mObj)) {}
LobbyConfig::~LobbyConfig() { ee_won_lobbyconf_free(this->mConf); }
bool LobbyConfig::HandleValueInstruction(ConfigParser* p, const std::string& k, StringParser& v)
{ CfgExc x; return Ret(ee_won_lobbyconf_handle_value(this->mConf, this->mObj, p, B(k), k.length(), (EeWonCfgStringParser*)&v, &x), x); }
bool LobbyConfig::PostParse(ConfigParser* p) { return ee_won_lobbyconf_post_parse(this->mConf, p->Obj()); }

// ---- LobbyAuthContextAccess.cpp (merged into this unit, R6 WON lobby/net round 40)
// Returns the login context; with CRC checks on, first gives it the files to hash for the community (once)
AuthContext* GetLobbyAuthContext()
{
    if(gLobbyData.get()==NULL)
        return NULL;
    if(ee_won_lobbyconf_num(ee_won_lobbyconf_current(),EeWonLobbyConfNum::CrcCheck)) // the files once (lobbyprv.rs: the list, or the running executable)
        ee_won_lobbyprv_hash_login(ee_won_authctx_login(gLobbyData->mAuthContext->Core()), gLobbyData->mLoginCommunity.data(),
                                   sizeof(wchar_t), gLobbyData->mLoginCommunity.size());
    return gLobbyData->mAuthContext.get();
}
}

// ---- InitLogic.cpp (merged into this unit, R6 WON lobby/net round 40)
// InitLogic: Internet lobby start-up (DNS, directory, version and firewall checks, terms of use, advert).
// A Rust object since R6 WONLobby round 8 (rust/crates/ee_won_lobby/src/logics.rs: the errors, the order of the checks
// and every decision, over flow.rs's); this unit keeps the shell (the HTTP session, the firewall test's socket, op
// and address, the adapter dialog, the advert's link) and the host's ops, each one the original's calls on a C++
// object (the ops, the server lists, the files, the events).
// Timeout for the firewall detect and accept operations, in milliseconds.
unsigned long WONAPI::gLobbyAcceptTimeout = 20000;

namespace WONAPI {
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
    switch (inOp) {
    case O::BroadcastLobby: LobbyBroadcastLobbyEvent((int)a, true); return 0;
    case O::Exit: LobbyExit((int)a); return 0;
    case O::Root: return (intptr_t)GetLobbyRoot();
    case O::InitStatusText: LobbyBroadcastEvent(LoginInitStatusEvent(gLobbyResources[a].mString, *T(b), (d & 1) != 0, (int)(d >> 8)), true); return 0;
    case O::InitStatusRes: LobbyBroadcastEvent(LoginInitStatusEvent(gLobbyResources[a].mString, gLobbyResources[c].mString, (d & 1) != 0, (int)(d >> 8)), true); return 0;
    case O::InitStatusEmpty: LobbyBroadcastEvent(LoginInitStatusEvent(gLobbyResources[a].mString, gEmptyGUIString, (d & 1) != 0, (int)(d >> 8)), true); return 0;
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
    case O::ServerAddAddress: SC(a)->AddAddress(IPAddr(std::string(reinterpret_cast<const char*>(b), (size_t)c))); return 0;
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
    case O::BroadcastNoPromotion: LobbyBroadcastEvent(SetCrossPromotionEvent(NULL, std::string("")), true); return 0;
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
    case O::DnsOpNew: return Held(NewDnsTestOp(std::string(reinterpret_cast<const char*>(a), (size_t)b)));
    case O::HttpDocNew: return Held(RustServerOp::NewHttpDocument(std::string(reinterpret_cast<const char*>(b), (size_t)c), (int)a));
    case O::HttpDocSetConfig: R(a)->SetBytes(EE_WON_HTTPDOC_CONFIG, std::string(reinterpret_cast<const char*>(b), (size_t)c)); return 0;
    case O::HttpDocSetSession: R(a)->Set(EE_WON_HTTPDOC_SESSION, aLogic->Slot<HTTPSession>(kInitSession)); return 0;
    case O::DirServers: return (intptr_t)LobbyGetDirServers();
    case O::RequestNew: return Held(RustServerOp::NewRequest(reinterpret_cast<EeWonReqOp*>(b), SC(a)));
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
