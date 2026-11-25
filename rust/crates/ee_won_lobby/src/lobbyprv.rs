//! The lobby's private state (WONLobbyPrv.cpp, R6 WON round 33): WONLobbyPrv's constructor, Init (WONLobby.cfg,
//! _LobbyMaster.cfg over the resource table, the logic objects' init, the product ids), Destroy and InitAuth, and
//! the resource table's 46 allocators (GetLobbyResourceConfigTable), in Rust over one-statement C++ ops (`PrvHost`
//! in WONLobbyPrv.cpp), in the original's order. The members stay the C++ object's; Init's locals (the parser, the
//! component config, the resource table, the defines' iterator, the CRC) are the host's statics, made and released
//! where the locals were made and went out of scope (in the original's reverse order).
//!
//! A C++ exception never unwinds through this code: the ops that could throw (the file open and parses, the logic
//! objects' init) run under WONLobbyGuard.h's `WonGuard` and answer `EE_WON_LOBBY_THROWN`; Init then releases its
//! locals as the unwinding did and answers it too, and the C++ entry rethrows the exception (`WonRethrow`).
//!
//! Reach: the LAN game constructs, inits and destroys the lobby and sets its auth; Init's failure branches (a
//! missing or broken WONLobby.cfg or _LobbyMaster.cfg, a logic's config error, an exception) run only in
//! won_lobby33_test's golden cases.

/// The host: `(prv, op, a) -> result`.
pub type EeWonLobbyPrvHostFn =
    Option<unsafe extern "C" fn(prv: *mut core::ffi::c_void, op: EeWonLobbyPrvOp, a: isize) -> isize>;

/// The host's ops: each one C++ statement.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyPrvOp {
    /// `p->mInitError = GetEmptyGUIStringData()`.
    InitErrorClear = 1,
    /// `sPrvParser = new ConfigParser`.
    ParserNew,
    /// `sPrvParser->SetWarningsAsErrors(true)`.
    ParserWarningsAsErrors,
    /// `return WonGuard([&] { return sPrvParser->OpenFile(WONAPICore::GetDefaultFileDirectory() + LobbyConfig::mDefaultFileName); });`.
    ParserOpenDefault,
    /// `p->mInitError = GUIString("Couldn't find " + LobbyConfig::mDefaultFileName)`.
    ErrorNotFound,
    /// `return WonGuard([&] { return p->mConfig->ParseTopLevel(*sPrvParser, NULL); });`.
    ParseTopLevel,
    /// `p->mInitError = GUIString(LobbyConfig::mDefaultFileName + " error: ")`.
    ErrorParseHead,
    /// `p->mInitError.append(GUIString(sPrvParser->GetAbortReason()))`.
    ErrorParseReason,
    /// `p->mLobbyPort = p->mConfig->mLobbyPort`.
    LobbyPortFromConfig,
    /// `sPrvComponentConfig = new ContainerConfig`.
    ComponentConfigNew,
    /// `p->mLobbyContainer = new LobbyContainer`.
    LobbyContainerNew,
    /// `ComponentConfig::SetResourceFolder(WONAPICore::GetDefaultFileDirectory() + p->mConfig->mResourceDirectory)`.
    SetResourceFolder,
    /// `sPrvDefine = p->mConfig->mDefines.begin()`.
    DefinesBegin,
    /// `sPrvDefine == p->mConfig->mDefines.end()`.
    DefinesAtEnd,
    /// `sPrvParser->AddDefine(sPrvDefine->first, sPrvDefine->second, false)`.
    DefineAdd,
    /// `++sPrvDefine`.
    DefineNext,
    /// `return WonGuard([&] { return sPrvComponentConfig->ReadConfig_Unsafe(p->mLobbyContainer.get(), "_LobbyMaster.cfg", sPrvTable.get(), sPrvParser); });`.
    ReadMaster,
    /// `p->mInitError = sPrvComponentConfig->mConfigError`.
    ErrorFromConfig,
    /// `return WonGuard([&] { return PrvConfigTry(p, [&] { p->mLogicMgr.Init(sPrvComponentConfig.get()); }); });`.
    LogicMgrInit,
    /// `return WonGuard([&] { return PrvConfigTry(p, [&] { reinterpret_cast<void (*)(ComponentConfig*)>(a)(sPrvComponentConfig.get()); }); });`.
    InitFuncRun,
    /// `sPrvCrc = CRC16()`.
    CrcNew,
    /// `sPrvCrc.Put(p->mConfig->mProductName.data(), p->mConfig->mProductName.length())`.
    CrcPutProductName,
    /// `sPrvCrc.Put(p->mConfig->mVersion.data(), p->mConfig->mVersion.length())`.
    CrcPutVersion,
    /// `p->mProductId = sPrvCrc.Get()`.
    ProductIdFromCrc,
    /// `sPrvCrc.Reset()`.
    CrcReset,
    /// `sPrvCrc.Put(p->mConfig->mLanVersion.data(), p->mConfig->mLanVersion.length())`.
    CrcPutLanVersion,
    /// `p->mLanProductId = sPrvCrc.Get()`.
    LanProductIdFromCrc,
    /// `p->mChatCommandParser->InitCommands()`.
    ChatParserInitCommands,
    /// `sPrvTable = NULL`.
    ResDone,
    /// `sPrvComponentConfig = NULL`.
    ComponentConfigDone,
    /// `delete sPrvParser`.
    ParserFree,
    /// `p->mLogicMgr.Kill()`.
    LogicMgrKill,
    /// `p->mUDPManager.get() != NULL`.
    UdpIs,
    /// `p->mUDPManager->Close()`.
    UdpClose,
    /// `p->mUDPManager = NULL`.
    UdpClear,
    /// `p->mAuthContext.get() != NULL`.
    AuthIs,
    /// `p->mAuthContext->Kill()`.
    AuthKill,
    /// `p->mAuthContext = NULL`.
    AuthClear,
    /// `p->mHasSetHashFileList = false`.
    HashSetClear,
    /// `p->mHashFileList.clear()`.
    HashListClear,
    /// `ee_won_cfgval_read_strings(a, &p->mHashFileList)` (the original's `StringParser(a).ReadValue(list)`).
    HashListRead,
    /// `p->mLoginCommunity = *reinterpret_cast<const std::wstring*>(a)`.
    CommunitySet,
    /// `p->mAuthContext->SetCommunity(p->mLoginCommunity)`.
    AuthSetCommunity,
    /// `p->mConfig->mCDKeyCheck`.
    CdKeyCheck,
    /// `p->mCDKey.SetProductString(p->mConfig->mProductName)`.
    CdKeySetProduct,
    /// `p->mCDKey.LoadFromRegistry()`.
    CdKeyLoad,
    /// `p->mAuthContext->SetCDKey(p->mLoginCommunity, p->mCDKey)`.
    AuthSetCdKey,
    /// `p->mLobbyPort = 8888`.
    LobbyPort8888,
    /// `p->mSysTOUTime = p->mGameTOUTime = 0`.
    TouTimesClear,
    /// `p->mBehindFirewall = false`.
    BehindFirewallClear,
    /// `p->mFirewallTestDone = false`.
    FirewallTestDoneClear,
    /// `p->mSuspended = false`.
    SuspendedClear,
    /// `p->mCurGameType = (LobbyGameType)0`.
    CurGameTypeClear,
    /// `p->mLobbyStatus = (LobbyStatus)0`.
    LobbyStatusClear,
    /// `p->mRunMode = (LobbyRunMode)1`.
    RunModeOne,
    /// `p->mProductId = 0`.
    ProductIdClear,
    /// `p->mLanProductId = 0`.
    LanProductIdClear,
    /// `p->mSysTOUPath = WONAPICore::GetDefaultFileDirectory() + "_wonsystou_m.txt"`.
    SysTouPath,
    /// `p->mGameTOUPath = WONAPICore::GetDefaultFileDirectory() + "_wongametou_m.txt"`.
    GameTouPath,
    /// `p->mConfig = LobbyConfig::mAllocator()`.
    ConfigNew,
    /// `p->mPersistentData = new LobbyPersistentData`.
    PersistentDataNew,
    /// `p->mChatCommandParser = new ChatCommandParser`.
    ChatParserNew,
    /// `p->mUDPManager = new UDPManager`.
    UdpNew,
    /// `p->mAccountServers = new ServerContext`.
    AccountServersNew,
    /// `p->mAuthServers = new ServerContext`.
    AuthServersNew,
    /// `p->mDirServers = new ServerContext`.
    DirServersNew,
    /// `p->mRoomDirServers = new ServerContext`.
    RoomDirServersNew,
    /// `p->mFirewallDetectServers = new ServerContext`.
    FirewallServersNew,
    /// `p->mPatchServers = new ServerContext`.
    PatchServersNew,
    /// `p->mVersionStatus = WS_None`.
    VersionStatusNone,
    /// `p->mAuthContext = new AuthContext`.
    AuthContextNew,
    /// `p->mAuthContext->SetServerContext(p->mAuthServers.get())`.
    AuthSetServers,
    /// `p->mAccountLogic = new AccountLogic`.
    AccountLogicNew,
    /// `p->mDialogLogic = new DialogLogic`.
    DialogLogicNew,
    /// `p->mInitLogic = new InitLogic`.
    InitLogicNew,
    /// `p->mRoutingLogic = new RoutingLogic`.
    RoutingLogicNew,
    /// `p->mStagingLogic = new StagingLogic`.
    StagingLogicNew,
    /// `p->mLanLogic = new LanLogic`.
    LanLogicNew,
    /// `p->mPingLogic = new PingLogic`.
    PingLogicNew,
    /// `p->mLogicMgr.AddLogic(p->mAccountLogic.get())`.
    AddAccountLogic,
    /// `p->mLogicMgr.AddLogic(p->mDialogLogic.get())`.
    AddDialogLogic,
    /// `p->mLogicMgr.AddLogic(p->mStagingLogic.get())`.
    AddStagingLogic,
    /// `p->mLogicMgr.AddLogic(p->mInitLogic.get())`.
    AddInitLogic,
    /// `p->mLogicMgr.AddLogic(p->mLanLogic.get())`.
    AddLanLogic,
    /// `p->mLogicMgr.AddLogic(p->mRoutingLogic.get())`.
    AddRoutingLogic,
    /// `p->mLogicMgr.AddLogic(p->mPingLogic.get())`.
    AddPingLogic,
    /// `sPrvTable = LobbyResource_Init(reinterpret_cast<ResourceConfigTable*>(a))`.
    ResInit,
    /// `WONGUIConfig_Init(sPrvTable.get())`.
    ResGuiInit,
    /// `sPrvTable->AddAllocator("BrowserComponent",NewConfigOf<EeWonCfgKind::Browser>,NewComponentOf<BrowserComponent>)`.
    AllocBrowserComponent,
    /// `sPrvTable->AddAllocator("ChatCtrl",NewConfigOf<ChatCtrlConfig>,NewComponentOf<ChatCtrl>)`.
    AllocChatCtrl,
    /// `sPrvTable->AddAllocator("CreateAccountCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::CreateAccount>)`.
    AllocCreateAccountCtrl,
    /// `sPrvTable->AddAllocator("DirectConnectCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::DirectConnect>)`.
    AllocDirectConnectCtrl,
    /// `sPrvTable->AddAllocator("FriendsListCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::Friends>)`.
    AllocFriendsListCtrl,
    /// `sPrvTable->AddAllocator("GameBrowserCtrl",NewConfigOf<GameBrowserCtrlConfig>,NewComponentOf<GameBrowserCtrl>)`.
    AllocGameBrowserCtrl,
    /// `sPrvTable->AddAllocator("GameStagingCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::Staging>)`.
    AllocGameStagingCtrl,
    /// `sPrvTable->AddAllocator("GameUserListCtrl",NewConfigOf<GameUserListCtrlConfig>,GameUserListCtrlConfig::CompFactory)`.
    AllocGameUserListCtrl,
    /// `sPrvTable->AddAllocator("InitFailedCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::InitFailed>)`.
    AllocInitFailedCtrl,
    /// `sPrvTable->AddAllocator("InternetOptionsCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::NetOptions>)`.
    AllocInternetOptionsCtrl,
    /// `sPrvTable->AddAllocator("InternetScreen",NewConfigOf<ContainerConfig>,ScreenFactory<false>)`.
    AllocInternetScreen,
    /// `sPrvTable->AddAllocator("LanOptionsCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LanOptions>)`.
    AllocLanOptionsCtrl,
    /// `sPrvTable->AddAllocator("LanScreen",NewConfigOf<ContainerConfig>,ScreenFactory<true>)`.
    AllocLanScreen,
    /// `sPrvTable->AddAllocator("LoginCtrl",NewConfigOf<EeWonCfgKind::Dialog>,DlgCtlFactory<Ck::Login>)`.
    AllocLoginCtrl,
    /// `sPrvTable->AddAllocator("LoginHelpCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<LoginHelpCtrl>)`.
    AllocLoginHelpCtrl,
    /// `sPrvTable->AddAllocator("LoginScreen",NewConfigOf<ContainerConfig>,NewComponentOf<LoginScreen>)`.
    AllocLoginScreen,
    /// `sPrvTable->AddAllocator("LoginStatusCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LoginStatus>)`.
    AllocLoginStatusCtrl,
    /// `sPrvTable->AddAllocator("LostPasswordCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LostPassword>)`.
    AllocLostPasswordCtrl,
    /// `sPrvTable->AddAllocator("LostUserNameCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LostUserName>)`.
    AllocLostUserNameCtrl,
    /// `sPrvTable->AddAllocator("MOTDCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<MOTDCtrl>)`.
    AllocMOTDCtrl,
    /// `sPrvTable->AddAllocator("QuickPlayCtrl",NewConfigOf<EeWonCfgKind::Dialog>,DlgCtlFactory<Ck::QuickPlay>)`.
    AllocQuickPlayCtrl,
    /// `sPrvTable->AddAllocator("RoomCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::Room>)`.
    AllocRoomCtrl,
    /// `sPrvTable->AddAllocator("RoomLabelCtrl",NewConfigOf<RoomLabelCtrlConfig>,NewComponentOf<RoomLabelCtrl>)`.
    AllocRoomLabelCtrl,
    /// `sPrvTable->AddAllocator("ServerListCtrl",NewConfigOf<ServerListCtrlConfig>,NewComponentOf<ServerListCtrl>)`.
    AllocServerListCtrl,
    /// `sPrvTable->AddAllocator("UpdateAccountCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::UpdateAccount>)`.
    AllocUpdateAccountCtrl,
    /// `sPrvTable->AddAllocator("UserListCtrl",NewConfigOf<UserListCtrlConfig>,UserListCtrlConfig::CompFactory)`.
    AllocUserListCtrl,
    /// `sPrvTable->AddAllocator("AddFriendDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::AddFriend>)`.
    AllocAddFriendDialog,
    /// `sPrvTable->AddAllocator("AdminActionDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::AdminAction>)`.
    AllocAdminActionDialog,
    /// `sPrvTable->AddAllocator("BadUserDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::BadUser>)`.
    AllocBadUserDialog,
    /// `sPrvTable->AddAllocator("ChooseNetworkAdapterDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::ChooseAdapter>)`.
    AllocChooseNetworkAdapterDialog,
    /// `sPrvTable->AddAllocator("CreditsDialog",NewConfigOf<CreditsDialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::Credits>)`.
    AllocCreditsDialog,
    /// `sPrvTable->AddAllocator("CreateGameDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::CreateGame>)`.
    AllocCreateGameDialog,
    /// `sPrvTable->AddAllocator("CreateRoomDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::CreateRoom>)`.
    AllocCreateRoomDialog,
    /// `sPrvTable->AddAllocator("DirectConnectDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::DirectConnect>)`.
    AllocDirectConnectDialog,
    /// `sPrvTable->AddAllocator("LanNameDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::LanName>)`.
    AllocLanNameDialog,
    /// `sPrvTable->AddAllocator("LanPortDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::LanPort>)`.
    AllocLanPortDialog,
    /// `sPrvTable->AddAllocator("OkCancelDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::OkCancel>)`.
    AllocOkCancelDialog,
    /// `sPrvTable->AddAllocator("PasswordDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::Password>)`.
    AllocPasswordDialog,
    /// `sPrvTable->AddAllocator("RoomDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::Room>)`.
    AllocRoomDialog,
    /// `sPrvTable->AddAllocator("StatusDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::Status>)`.
    AllocStatusDialog,
    /// `sPrvTable->AddAllocator("TOUDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::Tou>)`.
    AllocTOUDialog,
    /// `sPrvTable->AddAllocator("CrossPromotionButton",NewConfigOf<CrossPromotionButtonConfig>,NewComponentOf<CrossPromotionButton>)`.
    AllocCrossPromotionButton,
    /// `sPrvTable->AddAllocator("ImageTextButton",NewConfigOf<ImageTextButtonConfig>,NewComponentOf<ImageTextButton>)`.
    AllocImageTextButton,
    /// `sPrvTable->AddAllocator("MaxSplitterButton",NewConfigOf<EeWonCfgKind::MsArrowButton>,NewShellOf<EeWonShellKind::MaxSplitterButton,Direction_Up>)`.
    AllocMaxSplitterButton,
    /// `sPrvTable->AddAllocator("GameOptionsCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<Container>)`.
    AllocGameOptionsCtrl,
    /// `sPrvTable->AddAllocator("PlayerOptionsCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<Container>)`.
    AllocPlayerOptionsCtrl,
}

/// The calls in.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyPrvCall {
    /// `WONLobbyPrv::WONLobbyPrv()`'s body.
    Construct = 1,
    /// `Init(a, b)`: true, false, or `EE_WON_LOBBY_THROWN`.
    Init,
    /// `Destroy()`.
    Destroy,
    /// `InitAuth(*(std::wstring*)a, (const char*)b)`.
    InitAuth,
}

use EeWonLobbyPrvOp as Op;

/// The resource table's allocators (GetLobbyResourceConfigTable's lines), in order.
const ALLOCATORS: &[Op] = &[
    Op::AllocBrowserComponent,
    Op::AllocChatCtrl,
    Op::AllocCreateAccountCtrl,
    Op::AllocDirectConnectCtrl,
    Op::AllocFriendsListCtrl,
    Op::AllocGameBrowserCtrl,
    Op::AllocGameStagingCtrl,
    Op::AllocGameUserListCtrl,
    Op::AllocInitFailedCtrl,
    Op::AllocInternetOptionsCtrl,
    Op::AllocInternetScreen,
    Op::AllocLanOptionsCtrl,
    Op::AllocLanScreen,
    Op::AllocLoginCtrl,
    Op::AllocLoginHelpCtrl,
    Op::AllocLoginScreen,
    Op::AllocLoginStatusCtrl,
    Op::AllocLostPasswordCtrl,
    Op::AllocLostUserNameCtrl,
    Op::AllocMOTDCtrl,
    Op::AllocQuickPlayCtrl,
    Op::AllocRoomCtrl,
    Op::AllocRoomLabelCtrl,
    Op::AllocServerListCtrl,
    Op::AllocUpdateAccountCtrl,
    Op::AllocUserListCtrl,
    Op::AllocAddFriendDialog,
    Op::AllocAdminActionDialog,
    Op::AllocBadUserDialog,
    Op::AllocChooseNetworkAdapterDialog,
    Op::AllocCreditsDialog,
    Op::AllocCreateGameDialog,
    Op::AllocCreateRoomDialog,
    Op::AllocDirectConnectDialog,
    Op::AllocLanNameDialog,
    Op::AllocLanPortDialog,
    Op::AllocOkCancelDialog,
    Op::AllocPasswordDialog,
    Op::AllocRoomDialog,
    Op::AllocStatusDialog,
    Op::AllocTOUDialog,
    Op::AllocCrossPromotionButton,
    Op::AllocImageTextButton,
    Op::AllocMaxSplitterButton,
    Op::AllocGameOptionsCtrl,
    Op::AllocPlayerOptionsCtrl,
];

/// An op whose statement threw (WONLobbyGuard.h's `kWonThrown`).
pub const EE_WON_LOBBY_THROWN: isize = isize::MIN;
const THROWN: isize = EE_WON_LOBBY_THROWN;

/// Init's locals released, in the original's reverse order of construction (the table, the component config, then
/// the parser), and `r` answered.
fn init_exit(op: &dyn Fn(Op, isize) -> isize, made: bool, r: isize) -> isize {
    if made {
        op(Op::ResDone, 0);
        op(Op::ComponentConfigDone, 0);
    }
    op(Op::ParserFree, 0);
    r
}

fn init(input: isize, init_func: isize, op: &dyn Fn(Op, isize) -> isize) -> isize {
    let on = |o: Op| op(o, 0);
    on(Op::InitErrorClear);
    on(Op::ParserNew);
    on(Op::ParserWarningsAsErrors);
    match on(Op::ParserOpenDefault) {
        THROWN => return init_exit(op, false, THROWN),
        0 => {
            on(Op::ErrorNotFound);
            return init_exit(op, false, 0);
        }
        _ => {}
    }
    match on(Op::ParseTopLevel) {
        THROWN => return init_exit(op, false, THROWN),
        0 => {
            on(Op::ErrorParseHead);
            on(Op::ErrorParseReason);
            return init_exit(op, false, 0);
        }
        _ => {}
    }
    on(Op::LobbyPortFromConfig);
    on(Op::ComponentConfigNew);
    on(Op::LobbyContainerNew);
    on(Op::SetResourceFolder);
    // GetLobbyResourceConfigTable(input)
    op(Op::ResInit, input);
    on(Op::ResGuiInit);
    for &a in ALLOCATORS {
        on(a);
    }
    crate::srvlistctl::ee_won_lobbyctl_init_columns();
    on(Op::DefinesBegin);
    while on(Op::DefinesAtEnd) == 0 {
        on(Op::DefineAdd);
        on(Op::DefineNext);
    }
    match on(Op::ReadMaster) {
        THROWN => return init_exit(op, true, THROWN),
        0 => {
            on(Op::ErrorFromConfig);
            return init_exit(op, true, 0);
        }
        _ => {}
    }
    // the try: the logic objects' init, then the caller's (a config error is the init error: the op answers 0)
    let r = on(Op::LogicMgrInit);
    if r != 1 {
        return init_exit(op, true, r);
    }
    if init_func != 0 {
        let r = op(Op::InitFuncRun, init_func);
        if r != 1 {
            return init_exit(op, true, r);
        }
    }
    on(Op::CrcNew);
    on(Op::CrcPutProductName);
    on(Op::CrcPutVersion);
    on(Op::ProductIdFromCrc);
    on(Op::CrcReset);
    on(Op::CrcPutProductName);
    on(Op::CrcPutLanVersion);
    on(Op::LanProductIdFromCrc);
    on(Op::ChatParserInitCommands);
    init_exit(op, true, 1)
}

/// The constructor's statements, in order.
const CONSTRUCT: &[Op] = &[
    Op::LobbyPort8888,
    Op::TouTimesClear,
    Op::BehindFirewallClear,
    Op::FirewallTestDoneClear,
    Op::SuspendedClear,
    Op::CurGameTypeClear,
    Op::LobbyStatusClear,
    Op::RunModeOne,
    Op::ProductIdClear,
    Op::LanProductIdClear,
    Op::SysTouPath,
    Op::GameTouPath,
    Op::ConfigNew,
    Op::PersistentDataNew,
    Op::ChatParserNew,
    Op::UdpNew,
    Op::AccountServersNew,
    Op::AuthServersNew,
    Op::DirServersNew,
    Op::RoomDirServersNew,
    Op::FirewallServersNew,
    Op::PatchServersNew,
    Op::VersionStatusNone,
    Op::AuthContextNew,
    Op::AuthSetServers,
    Op::AccountLogicNew,
    Op::DialogLogicNew,
    Op::InitLogicNew,
    Op::RoutingLogicNew,
    Op::StagingLogicNew,
    Op::LanLogicNew,
    Op::PingLogicNew,
    Op::AddAccountLogic,
    Op::AddDialogLogic,
    Op::AddStagingLogic,
    Op::AddInitLogic,
    Op::AddLanLogic,
    Op::AddRoutingLogic,
    Op::AddPingLogic,
];

fn call(call: EeWonLobbyPrvCall, a: isize, b: isize, op: &dyn Fn(Op, isize) -> isize) -> isize {
    let on = |o: Op| op(o, 0);
    match call {
        EeWonLobbyPrvCall::Construct => {
            for &o in CONSTRUCT {
                on(o);
            }
            0
        }
        EeWonLobbyPrvCall::Init => init(a, b, op),
        EeWonLobbyPrvCall::Destroy => {
            on(Op::LogicMgrKill);
            if on(Op::UdpIs) != 0 {
                on(Op::UdpClose);
                on(Op::UdpClear);
            }
            if on(Op::AuthIs) != 0 {
                on(Op::AuthKill);
                on(Op::AuthClear);
            }
            0
        }
        EeWonLobbyPrvCall::InitAuth => {
            on(Op::HashSetClear);
            on(Op::HashListClear);
            if b != 0 {
                op(Op::HashListRead, b);
            }
            op(Op::CommunitySet, a);
            on(Op::AuthSetCommunity);
            if on(Op::CdKeyCheck) == 0 {
                return 0;
            }
            on(Op::CdKeySetProduct);
            on(Op::CdKeyLoad);
            on(Op::AuthSetCdKey);
            0
        }
    }
}

/// One of WONLobbyPrv's methods on `prv` over the host `host`.
///
/// # Safety
/// `host` answers the ops for `prv` (PrvHost).
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprv_call(
    prv: *mut core::ffi::c_void,
    host: EeWonLobbyPrvHostFn,
    which: EeWonLobbyPrvCall,
    a: isize,
    b: isize,
) -> isize {
    let op = |o: Op, a: isize| match host {
        // SAFETY: the contract
        Some(f) => unsafe { f(prv, o, a) },
        None => 0,
    };
    call(which, a, b, &op)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    fn run(c: EeWonLobbyPrvCall, a: isize, b: isize, answer: &dyn Fn(Op) -> isize) -> (isize, Vec<String>) {
        let log = RefCell::new(Vec::new());
        let op = |o: Op, a: isize| {
            log.borrow_mut().push(format!("{o:?} {a}"));
            answer(o)
        };
        let r = call(c, a, b, &op);
        (r, log.into_inner())
    }

    fn ok(o: Op) -> isize {
        matches!(
            o,
            Op::ParserOpenDefault
                | Op::ParseTopLevel
                | Op::ReadMaster
                | Op::LogicMgrInit
                | Op::InitFuncRun
                | Op::DefinesAtEnd
        ) as isize
    }

    #[test]
    fn init_releases_its_locals_on_every_exit() {
        let (r, log) = run(EeWonLobbyPrvCall::Init, 5, 9, &ok);
        assert_eq!(r, 1);
        assert_eq!(log[log.len() - 3..], ["ResDone 0", "ComponentConfigDone 0", "ParserFree 0"]);
        assert!(log.contains(&"ResInit 5".to_string()) && log.contains(&"InitFuncRun 9".to_string()));
        assert_eq!(log.iter().filter(|l| l.starts_with("Alloc")).count(), 46);
        let (r, log) = run(EeWonLobbyPrvCall::Init, 5, 0, &|o| if o == Op::ParserOpenDefault { THROWN } else { 0 });
        assert_eq!((r, log.last().unwrap().as_str()), (THROWN, "ParserFree 0"));
        let (r, log) = run(EeWonLobbyPrvCall::Init, 5, 0, &|o| if o == Op::LogicMgrInit { 0 } else { ok(o) });
        assert_eq!((r, log[log.len() - 4].as_str()), (0, "LogicMgrInit 0"));
        let (r, log) = run(EeWonLobbyPrvCall::Init, 5, 0, &|o| (o == Op::ParserOpenDefault) as isize);
        assert_eq!(
            (r, log[log.len() - 3].as_str(), log.last().unwrap().as_str()),
            (0, "ErrorParseHead 0", "ParserFree 0")
        );
    }

    #[test]
    fn destroy_auth_and_the_constructor() {
        let (_, log) = run(EeWonLobbyPrvCall::Destroy, 0, 0, &|o| (o == Op::AuthIs) as isize);
        assert_eq!(log, ["LogicMgrKill 0", "UdpIs 0", "AuthIs 0", "AuthKill 0", "AuthClear 0"]);
        let (_, log) = run(EeWonLobbyPrvCall::InitAuth, 7, 0, &|_| 0);
        assert_eq!(log, ["HashSetClear 0", "HashListClear 0", "CommunitySet 7", "AuthSetCommunity 0", "CdKeyCheck 0"]);
        let (_, log) = run(EeWonLobbyPrvCall::Construct, 0, 0, &|_| 0);
        assert_eq!((log.len(), log[0].as_str(), log[38].as_str()), (39, "LobbyPort8888 0", "AddPingLogic 0"));
    }
}
