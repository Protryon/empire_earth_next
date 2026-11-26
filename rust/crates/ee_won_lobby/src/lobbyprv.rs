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
    /// `p->mConfig->mLobbyPort` (the lobby port: the Rust's member, R6 WON lobby and network round 38).
    ConfigLobbyPort,
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
    /// `sPrvCrc.Get()` (a product id).
    CrcGet,
    /// `sPrvCrc.Reset()`.
    CrcReset,
    /// `sPrvCrc.Put(p->mConfig->mLanVersion.data(), p->mConfig->mLanVersion.length())`.
    CrcPutLanVersion,
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
    /// `p->mSuspended = false`.
    SuspendedClear,
    /// `p->mCurGameType = (LobbyGameType)0`.
    CurGameTypeClear,
    /// `p->mLobbyStatus = (LobbyStatus)0`.
    LobbyStatusClear,
    /// `p->mRunMode = (LobbyRunMode)1`.
    RunModeOne,
    /// `p->mSysTOUPath = WONAPICore::GetDefaultFileDirectory() + "_wonsystou_m.txt"`.
    SysTouPath,
    /// `p->mGameTOUPath = WONAPICore::GetDefaultFileDirectory() + "_wongametou_m.txt"`.
    GameTouPath,
    /// `p->mConfig = new LobbyConfig` (the game's: EELobbyConfig folded in, R6 WON lobby and network round 39).
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
    /// `sPrvTable->AddAllocator("ChatCtrl",NewConfigOf<EeWonCfgKind::LobbyChat>,NewComponentOf<ChatCtrl>)`.
    AllocChatCtrl,
    /// `sPrvTable->AddAllocator("CreateAccountCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::CreateAccount>)`.
    AllocCreateAccountCtrl,
    /// `sPrvTable->AddAllocator("DirectConnectCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::DirectConnect>)`.
    AllocDirectConnectCtrl,
    /// `sPrvTable->AddAllocator("FriendsListCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::Friends>)`.
    AllocFriendsListCtrl,
    /// `sPrvTable->AddAllocator("GameBrowserCtrl",NewConfigOf<EeWonCfgKind::LobbyGameBrowser>,NewComponentOf<GameBrowserCtrl>)`.
    AllocGameBrowserCtrl,
    /// `sPrvTable->AddAllocator("GameStagingCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::Staging>)`.
    AllocGameStagingCtrl,
    /// `sPrvTable->AddAllocator("GameUserListCtrl",NewConfigOf<EeWonCfgKind::LobbyGameUserList>,CtlFactory<Ck::UserList>)`.
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
    /// `sPrvTable->AddAllocator("RoomLabelCtrl",NewConfigOf<EeWonCfgKind::LobbyRoomLabel>,NewComponentOf<RoomLabelCtrl>)`.
    AllocRoomLabelCtrl,
    /// `sPrvTable->AddAllocator("ServerListCtrl",NewConfigOf<EeWonCfgKind::LobbyServerList>,NewComponentOf<ServerListCtrl>)`.
    AllocServerListCtrl,
    /// `sPrvTable->AddAllocator("UpdateAccountCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::UpdateAccount>)`.
    AllocUpdateAccountCtrl,
    /// `sPrvTable->AddAllocator("UserListCtrl",NewConfigOf<EeWonCfgKind::LobbyUserList>,CtlFactory<Ck::Users>)`.
    AllocUserListCtrl,
    /// `sPrvTable->AddAllocator("AddFriendDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::AddFriend>)`.
    AllocAddFriendDialog,
    /// `sPrvTable->AddAllocator("AdminActionDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::AdminAction>)`.
    AllocAdminActionDialog,
    /// `sPrvTable->AddAllocator("BadUserDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::BadUser>)`.
    AllocBadUserDialog,
    /// `sPrvTable->AddAllocator("ChooseNetworkAdapterDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::ChooseAdapter>)`.
    AllocChooseNetworkAdapterDialog,
    /// `sPrvTable->AddAllocator("CreditsDialog",NewConfigOf<EeWonCfgKind::LobbyCredits>,NewLobbyDialog<EeWonLobbyDlgKind::Credits>)`.
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
    /// `sPrvTable->AddAllocator("CrossPromotionButton",NewConfigOf<EeWonCfgKind::LobbyCrossPromotion>,NewComponentOf<CrossPromotionButton>)`.
    AllocCrossPromotionButton,
    /// `sPrvTable->AddAllocator("ImageTextButton",NewConfigOf<EeWonCfgKind::LobbyImageText>,NewComponentOf<ImageTextButton>)`.
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

use ee_won_net::ipobj::EeWonIpAddr;
use std::ffi::{c_long, c_void};
use std::sync::Mutex;
use EeWonLobbyPrvOp as Op;

// ---- WONLobbyPrv's plain members (R6 WON lobby and network round 38: the Rust's; the C++ object keeps its objects)

/// A plain member of the lobby's private state.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyPrvField {
    /// `mVersionStatus` (+0x70; `WS_None` 100004 from the constructor, and without a lobby).
    VersionStatus = 1,
    /// `mSysTOUTime` (+0xbc).
    SysTouTime,
    /// `mGameTOUTime` (+0xc0).
    GameTouTime,
    /// `mLobbyPort` (+0xb8; 8888 from the constructor, the config's from Init, the bound port after the UDP bind).
    LobbyPort,
    /// `mBehindFirewall` (+0xc4; a write also marks the firewall test done, as LobbySetBehindFirewall did).
    BehindFirewall,
    /// `mFirewallTestDone` (+0xc5).
    FirewallTestDone,
    /// `mProductId` (+0xb4).
    ProductId,
    /// `mLanProductId` (+0xb6; stamped on the game listings).
    LanProductId,
    /// `mHasSetHashFileList` (+0xa4; the login's file hashes were given, GetLobbyAuthContext's once: R6 WON round 39).
    HashFilesSet,
}

/// The members of the lobby `owner` (the WONLobbyPrv they belong to). None: no lobby, when every read answers what
/// the original's `gLobbyData == NULL` branch did and every write does nothing.
struct Vals {
    owner: usize,
    version_status: i32,
    sys_tou: i64,
    game_tou: i64,
    lobby_port: u16,
    behind_firewall: bool,
    firewall_test_done: bool,
    product_id: u16,
    lan_product_id: u16,
    /// `mPublicAddr` (+0xe8: the address seen from outside)
    public_addr: EeWonIpAddr,
    hash_files_set: bool,
    /// `mHashFileList` (+0xa8: the files the CRC check hashes; none: the running executable)
    hash_files: Vec<Vec<u8>>,
}

impl Vals {
    /// The constructor's values (the time, flag and id members cleared, port 8888, `WS_None`).
    fn new(owner: usize) -> Vals {
        Vals {
            owner,
            version_status: 100004,
            sys_tou: 0,
            game_tou: 0,
            lobby_port: 8888,
            behind_firewall: false,
            firewall_test_done: false,
            product_id: 0,
            lan_product_id: 0,
            public_addr: EeWonIpAddr::default(),
            hash_files_set: false,
            hash_files: Vec::new(),
        }
    }
}

static VALS: Mutex<Option<Vals>> = Mutex::new(None);

fn vals() -> std::sync::MutexGuard<'static, Option<Vals>> {
    VALS.lock().unwrap_or_else(|e| e.into_inner())
}

fn set(f: EeWonLobbyPrvField, v: i64) {
    ee_won_lobbyprv_set(f, v);
}

/// A member's value (without a lobby: `WS_None` for the version status, else 0).
#[no_mangle]
pub extern "C" fn ee_won_lobbyprv_get(f: EeWonLobbyPrvField) -> i64 {
    use EeWonLobbyPrvField as F;
    let g = vals();
    let Some(v) = g.as_ref() else {
        return if f == F::VersionStatus { 100004 } else { 0 };
    };
    match f {
        F::VersionStatus => v.version_status as i64,
        F::SysTouTime => v.sys_tou,
        F::GameTouTime => v.game_tou,
        F::LobbyPort => v.lobby_port as i64,
        F::BehindFirewall => v.behind_firewall as i64,
        F::FirewallTestDone => v.firewall_test_done as i64,
        F::ProductId => v.product_id as i64,
        F::LanProductId => v.lan_product_id as i64,
        F::HashFilesSet => v.hash_files_set as i64,
    }
}

/// A member set (nothing without a lobby).
#[no_mangle]
pub extern "C" fn ee_won_lobbyprv_set(f: EeWonLobbyPrvField, x: i64) {
    use EeWonLobbyPrvField as F;
    let mut g = vals();
    let Some(v) = g.as_mut() else { return };
    match f {
        F::VersionStatus => v.version_status = x as i32,
        F::SysTouTime => v.sys_tou = x,
        F::GameTouTime => v.game_tou = x,
        F::LobbyPort => v.lobby_port = x as u16,
        F::BehindFirewall => {
            v.behind_firewall = x != 0;
            v.firewall_test_done = true;
        }
        F::FirewallTestDone => v.firewall_test_done = x != 0,
        F::ProductId => v.product_id = x as u16,
        F::LanProductId => v.lan_product_id = x as u16,
        F::HashFilesSet => v.hash_files_set = x != 0,
    }
}

/// `mHashFileList`, each file NUL-ended, to the sink (nothing without a lobby).
///
/// # Safety
/// `sink` takes `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprv_hash_files(sink: ee_won_net::ffi::EeWonByteSink, context: *mut c_void) {
    let joined: Vec<u8> = vals()
        .as_ref()
        .map_or(Vec::new(), |v| v.hash_files.iter().flat_map(|f| f.iter().copied().chain([0])).collect());
    // SAFETY: the caller's sink
    unsafe { ee_won_net::ffi::give(sink, context, &joined) };
}

/// `mHashFileList` from `len` bytes, each file NUL-ended (nothing without a lobby).
///
/// # Safety
/// `bytes` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprv_set_hash_files(bytes: *const u8, len: usize) {
    let b = if len == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(bytes, len) } };
    if let Some(v) = vals().as_mut() {
        v.hash_files = b.split(|&c| c == 0).map(<[u8]>::to_vec).collect();
        v.hash_files.pop();
    }
}

/// GetLobbyAuthContext's CRC check (its caller tests the config's flag): once per InitAuth, the community's hash
/// files to the login, the running executable when the list is empty (the original's SetHashFile(community) or
/// SetHashFileList(community, mHashFileList)); nothing without a lobby.
///
/// # Safety
/// `login` is a live login; `community` holds `count` characters of `wchar_size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprv_hash_login(
    login: *const ee_won_net::authlogin::EeWonAuthLogin,
    community: *const c_void,
    wchar_size: u32,
    count: usize,
) {
    let files = {
        let mut g = vals();
        let Some(v) = g.as_mut() else { return };
        if v.hash_files_set {
            return;
        }
        v.hash_files_set = true;
        v.hash_files.clone()
    };
    // SAFETY: the caller's login and text
    unsafe { ee_won_net::authlogin::hash_files_or_exe(login, community, wchar_size, count, &files) };
}

/// LobbyIsTOUNewer: whether the recorded system or game terms of use are newer than the times given (false without
/// a lobby).
#[no_mangle]
pub extern "C" fn ee_won_lobbyprv_tou_newer(sys: c_long, game: c_long) -> bool {
    vals().as_ref().is_some_and(|v| v.sys_tou > sys as i64 || v.game_tou > game as i64)
}

/// LobbySetTOUTimes (nothing without a lobby).
#[no_mangle]
pub extern "C" fn ee_won_lobbyprv_set_tou(sys: c_long, game: c_long) {
    if let Some(v) = vals().as_mut() {
        v.sys_tou = sys as i64;
        v.game_tou = game as i64;
    }
}

/// LobbyGetTOUTimes: the two times into `sys` and `game` (left as they are without a lobby).
///
/// # Safety
/// `sys` and `game` are live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprv_tou_times(sys: *mut c_long, game: *mut c_long) {
    if let Some(v) = vals().as_ref() {
        // SAFETY: the contract
        unsafe {
            *sys = v.sys_tou as c_long;
            *game = v.game_tou as c_long;
        }
    }
}

/// LobbyResetFirewallTest: the test not done, the public address forgotten (nothing without a lobby).
#[no_mangle]
pub extern "C" fn ee_won_lobbyprv_reset_firewall() {
    if let Some(v) = vals().as_mut() {
        v.firewall_test_done = false;
        v.public_addr = EeWonIpAddr::default();
    }
}

/// LobbySetPublicAddress (nothing without a lobby).
///
/// # Safety
/// `a` is a live address.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprv_set_public_addr(a: *const EeWonIpAddr) {
    if let Some(v) = vals().as_mut() {
        // SAFETY: the contract
        v.public_addr = unsafe { (*a).clone() };
    }
}

/// LobbyGetPublicAddress: a new copy (an empty address without a lobby).
#[no_mangle]
pub extern "C" fn ee_won_lobbyprv_public_addr() -> *mut EeWonIpAddr {
    Box::into_raw(Box::new(vals().as_ref().map_or_else(EeWonIpAddr::default, |v| v.public_addr.clone())))
}

/// LobbyGetMyAddress: the address to give peers, a new value: the public address unless room spec 1, no valid
/// public address or the config's UseLocalIP (`use_local_ip`), else the local one; on the lobby port. An empty
/// address without a lobby.
#[no_mangle]
pub extern "C" fn ee_won_lobbyprv_my_address(room_spec: i32, use_local_ip: bool) -> *mut EeWonIpAddr {
    // (the lock is not held over the local address's look-up)
    let Some((public, port)) = vals().as_ref().map(|v| (v.public_addr.clone(), v.lobby_port)) else {
        return Box::into_raw(Box::default());
    };
    let mut a = if room_spec != 1 && public.valid && !use_local_ip { public } else { ee_won_net::ipobj::local_addr() };
    a.set_port(port);
    Box::into_raw(Box::new(a))
}

/// `~WONLobbyPrv`: the lobby `prv` gone, its members with it (a newer lobby's stay).
#[no_mangle]
pub extern "C" fn ee_won_lobbyprv_drop(prv: *mut core::ffi::c_void) {
    let mut g = vals();
    if g.as_ref().is_some_and(|v| v.owner == prv as usize) {
        *g = None;
    }
}

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
    set(EeWonLobbyPrvField::LobbyPort, on(Op::ConfigLobbyPort) as i64);
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
    set(EeWonLobbyPrvField::ProductId, on(Op::CrcGet) as u16 as i64);
    on(Op::CrcReset);
    on(Op::CrcPutProductName);
    on(Op::CrcPutLanVersion);
    set(EeWonLobbyPrvField::LanProductId, on(Op::CrcGet) as u16 as i64);
    on(Op::ChatParserInitCommands);
    init_exit(op, true, 1)
}

/// The constructor's statements, in order.
/// (the plain members' are [`Vals::new`]'s)
const CONSTRUCT: &[Op] = &[
    Op::SuspendedClear,
    Op::CurGameTypeClear,
    Op::LobbyStatusClear,
    Op::RunModeOne,
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

fn call(prv: usize, call: EeWonLobbyPrvCall, a: isize, b: isize, op: &dyn Fn(Op, isize) -> isize) -> isize {
    let on = |o: Op| op(o, 0);
    match call {
        EeWonLobbyPrvCall::Construct => {
            // the plain members first (the original's constructor set them first)
            *vals() = Some(Vals::new(prv));
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
            if let Some(v) = vals().as_mut() {
                v.hash_files_set = false;
                // `StringParser(b).ReadValue(mHashFileList)`
                v.hash_files = if b != 0 { crate::cfgval::read_string_list(b as *const u8) } else { Vec::new() };
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
    call(prv as usize, which, a, b, &op)
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
        let r = call(1, c, a, b, &op);
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
        let _turn = LOBBY.lock().unwrap_or_else(|e| e.into_inner());
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

    /// the plain members are one process-wide state: the tests that make a lobby take turns
    static LOBBY: std::sync::Mutex<()> = std::sync::Mutex::new(());

    #[test]
    fn destroy_auth_and_the_constructor() {
        let _turn = LOBBY.lock().unwrap_or_else(|e| e.into_inner());
        let (_, log) = run(EeWonLobbyPrvCall::Destroy, 0, 0, &|o| (o == Op::AuthIs) as isize);
        assert_eq!(log, ["LogicMgrKill 0", "UdpIs 0", "AuthIs 0", "AuthKill 0", "AuthClear 0"]);
        let (_, log) = run(EeWonLobbyPrvCall::InitAuth, 7, 0, &|_| 0);
        assert_eq!(log, ["CommunitySet 7", "AuthSetCommunity 0", "CdKeyCheck 0"]);
        let (_, log) = run(EeWonLobbyPrvCall::Construct, 0, 0, &|_| 0);
        assert_eq!((log.len(), log[0].as_str(), log[31].as_str()), (32, "SuspendedClear 0", "AddPingLogic 0"));
    }

    #[test]
    fn the_plain_members_answer_as_without_a_lobby_until_one_is_made() {
        use EeWonLobbyPrvField as F;
        let _turn = LOBBY.lock().unwrap_or_else(|e| e.into_inner());
        let (mut x, mut y) = (0u8, 0u8);
        let (me, other) = (&mut x as *mut u8 as *mut core::ffi::c_void, &mut y as *mut u8 as *mut core::ffi::c_void);
        // the other test's lobby (prv 1), if it is still there
        ee_won_lobbyprv_drop(std::ptr::without_provenance_mut(1));
        ee_won_lobbyprv_drop(me);
        assert_eq!((ee_won_lobbyprv_get(F::VersionStatus), ee_won_lobbyprv_get(F::LobbyPort)), (100004, 0));
        ee_won_lobbyprv_set(F::LobbyPort, 9);
        assert!(!ee_won_lobbyprv_tou_newer(-5, -5));
        call(me as usize, EeWonLobbyPrvCall::Construct, 0, 0, &|_, _| 0);
        assert_eq!((ee_won_lobbyprv_get(F::VersionStatus), ee_won_lobbyprv_get(F::LobbyPort)), (100004, 8888));
        ee_won_lobbyprv_set_tou(5, 7);
        assert!(ee_won_lobbyprv_tou_newer(4, 7) && ee_won_lobbyprv_tou_newer(5, 6) && !ee_won_lobbyprv_tou_newer(5, 7));
        let (mut s, mut g) = (0, 0);
        // SAFETY: live locals
        unsafe { ee_won_lobbyprv_tou_times(&mut s, &mut g) };
        assert_eq!((s, g), (5, 7));
        ee_won_lobbyprv_set(F::BehindFirewall, 1);
        assert_eq!((ee_won_lobbyprv_get(F::BehindFirewall), ee_won_lobbyprv_get(F::FirewallTestDone)), (1, 1));
        ee_won_lobbyprv_reset_firewall();
        assert_eq!((ee_won_lobbyprv_get(F::BehindFirewall), ee_won_lobbyprv_get(F::FirewallTestDone)), (1, 0));
        // the address: the local one on the lobby port without a valid public address
        // SAFETY: a value made here
        let a = unsafe { Box::from_raw(ee_won_lobbyprv_my_address(0, false)) };
        assert_eq!(a.port(), 8888);
        // another lobby's end leaves this one's members; this one's end drops them
        ee_won_lobbyprv_drop(other);
        assert_eq!(ee_won_lobbyprv_get(F::LobbyPort), 8888);
        ee_won_lobbyprv_drop(me);
        assert_eq!((ee_won_lobbyprv_get(F::VersionStatus), ee_won_lobbyprv_get(F::LobbyPort)), (100004, 0));
    }

    /// R6 WON round 39: InitLogic's private state ops (netlogicops.rs prv_op, the game's answer for the deleted
    /// LobbySet* forwards) and the hash file list (InitAuth's read, GetLobbyAuthContext's once)
    #[test]
    fn init_logic_ops_and_the_hash_list_are_the_state() {
        use crate::logics::EeWonLogicOp as LOp;
        use crate::netlogicops::prv_op;
        use EeWonLobbyPrvField as F;
        let _turn = LOBBY.lock().unwrap_or_else(|e| e.into_inner());
        let mut x = 0u8;
        let me = &mut x as *mut u8 as *mut core::ffi::c_void;
        call(me as usize, EeWonLobbyPrvCall::Construct, 0, 0, &|_, _| 0);
        assert_eq!(prv_op(LOp::SetVersionStatus, 100002, 0, 0), Some(0));
        assert_eq!(ee_won_lobbyprv_get(F::VersionStatus), 100002);
        prv_op(LOp::SetBehindFirewall, 2, 0, 0);
        assert_eq!((ee_won_lobbyprv_get(F::BehindFirewall), ee_won_lobbyprv_get(F::FirewallTestDone)), (1, 1));
        prv_op(LOp::ResetFirewallTest, 0, 0, 0);
        assert_eq!(ee_won_lobbyprv_get(F::FirewallTestDone), 0);
        prv_op(LOp::SetTOUTimes, 5, 9, 0);
        let (mut s, mut g) = (0i32, 0i32);
        prv_op(LOp::TOUTimes, &mut s as *mut i32 as isize, &mut g as *mut i32 as isize, 0);
        assert_eq!((s, g), (5, 9));
        assert_eq!(prv_op(LOp::Exit, 0, 0, 0), None);
        // InitAuth's list: cleared, read; the flag cleared
        ee_won_lobbyprv_set(F::HashFilesSet, 1);
        let (_, log) = run(EeWonLobbyPrvCall::InitAuth, 7, c"a.dat, b.dat".as_ptr() as isize, &|_| 0);
        assert_eq!(log[0], "CommunitySet 7");
        assert_eq!(ee_won_lobbyprv_get(F::HashFilesSet), 0);
        unsafe extern "C" fn sink(c: *mut core::ffi::c_void, b: *const u8, n: usize) {
            // SAFETY: the test's vector and the bytes given
            unsafe { (*(c as *mut Vec<u8>)).extend_from_slice(std::slice::from_raw_parts(b, n)) };
        }
        let mut files: Vec<u8> = Vec::new();
        // SAFETY: the sink and its vector
        unsafe { ee_won_lobbyprv_hash_files(sink, &mut files as *mut Vec<u8> as *mut core::ffi::c_void) };
        assert_eq!(files, b"a.dat\0b.dat\0");
        // SAFETY: the bytes
        unsafe { ee_won_lobbyprv_set_hash_files(c"x".as_ptr().cast(), 2) };
        files.clear();
        // SAFETY: as above
        unsafe { ee_won_lobbyprv_hash_files(sink, &mut files as *mut Vec<u8> as *mut core::ffi::c_void) };
        assert_eq!(files, b"x\0");
        ee_won_lobbyprv_drop(me);
    }
}
