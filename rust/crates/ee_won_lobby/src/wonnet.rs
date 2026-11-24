//! The game's own WON front end, EEWonNet (`src/Empire Earth/Communications/EEWonNet.cpp`), whole in Rust (R6 WON
//! round 31): the routing server's rooms and games, the chat, the hosted and joined game's data object, the
//! account and directory requests. Unreachable in AoC (`gWonNet` is only ever set to NULL) and kept as the
//! owner's reference for the Internet code.
//!
//! The object's data (the strings, the game, room and client maps, the joined game's info, the ids and the flags)
//! are this object's, and so are the C++ objects the class held (R6 WON lobby and network round 35): the locks,
//! the routing connection, the server contexts, the lobby and its window, the user, the server list, the ops and
//! the two cancel flags the cancel boxes write through a pointer are [`EeWonNetObjects`], this object's slots (a
//! reference held on each counted object, released as the class's members were). The C++ class is the Rust
//! object and its forwards; every call on a C++ object is one op of the host given to `ee_won_wonnet_set_host`
//! (EEWonNet.cpp's `WonNetHost`, reading the slots through `ee_won_wonnet_objects`), the original's statements
//! with no decision between them. won_wonnet_test runs the deleted C++ against this over a stand-in world.
//!
//! Texts: a `UWideString`'s bytes (UTF-8, decision D6) are `Vec<u8>` without the terminator; a `std::wstring` is
//! `Vec<u32>` of `wchar_t` units (UTF-16 on Windows, UTF-32 elsewhere).
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_char, c_long, c_void};

use crate::wonaction::EeWonAction;
use std::cell::{Cell, RefCell, UnsafeCell};
use std::collections::BTreeMap;
use std::sync::atomic::{AtomicUsize, Ordering};

use ee_base::plat_core::string::{
    PlatMbsCmp, PlatMbsLen, PlatMbsNBCnt, PlatMbsNInc, PlatUtf8ToWide, PlatWideToUtf8, WChar,
};
use ee_base::ustring::{
    ee_uwstring_at, ee_uwstring_copy_text, ee_uwstring_free, ee_uwstring_init, ee_uwstring_init_text, EeUWideString,
};
use ee_won_net::routeop::{
    EeWonRouteRecord, EE_WON_RTF_CLIENT_ID, EE_WON_RTF_FLAGS16, EE_WON_RTF_GROUP_ID, EE_WON_RTF_LINK_ID,
    EE_WON_RTF_MAX_PLAYERS, EE_WON_RTF_RECONNECT_ID, EE_WON_RTR_CLIENT, EE_WON_RTR_MEMBER, EE_WON_RTR_OBJECT,
    EE_WON_RTT_TEXT,
};
use ee_won_net::smartroute::{EeWonSmartGet, EeWonSmartRecord};
use ee_won_net::status_codes::{
    WS_Killed, WS_RoutingServG2_ClientAlreadyExists, WS_RoutingServG2_ClientAlreadyInGroup,
    WS_RoutingServG2_GroupAlreadyExists, WS_RoutingServG2_InvalidPassword, WS_Success,
};

/// The host's ops: `(cpp, op, a, b, c, d)`, `cpp` the C++ EEWonNet. A text the Rust hands over is a NUL-terminated
/// byte pointer; a wide text a pointer to NUL-terminated `wchar_t` units and their count. A `sink` is a
/// [`WonNetSink`] the C++ fills through `ee_won_wonnet_sink`. `op` is an `AsyncOp*`, `form` a `UIForm*`.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonNetOp {
    /// `ESVERIFY_AT(TSScheduler::CreateCriticalSection(mLock) == keseOK, 195)`
    LockCreate,
    /// the same for `mLock8` (197)
    Lock8Create,
    /// `TSScheduler::DestroyCriticalSection(mLock)`
    LockDestroy,
    /// `TSScheduler::DestroyCriticalSection(mLock8)`
    Lock8Destroy,
    /// `mLock->Enter()`
    LockEnter,
    /// `mLock->Leave()`
    LockLeave,
    /// `mLock8->Enter()`
    Lock8Enter,
    /// `mLock8->Leave()`
    Lock8Leave,
    /// `mLobbyWindow->Close(true)`
    LobbyWindowClose,
    /// `mLobbyWindow->mClosed`
    LobbyWindowClosed,
    /// `delete mLobbyWindow`
    LobbyWindowDelete,
    /// `mLobbyWindow->Virtual18()`
    LobbyWindowVirtual18,
    /// `delete mLobby`
    LobbyDelete,
    /// `mLobby->Pump((unsigned long)a, 0)`
    LobbyPump,
    /// `gWonNet != NULL`
    WonNetExists,
    /// `gWonClientId = a`
    ClientIdSet,
    /// `new SmartRoutingConnection` (a `RefCount*`; the object's `routing` holds it)
    RoutingNew,
    /// `mRouting->Kill()`
    RoutingKill,
    /// `mRouting->SetAuth(mUser, AUTH_TYPE_PERSISTENT)`
    RoutingSetAuth,
    /// `mRouting->SetConnectCompletion(` a gWonNet completion of call `a` `)`
    RoutingSetConnectCompletion,
    /// `mRouting->SetCloseCompletion(` call `a` `)`
    RoutingSetCloseCompletion,
    /// `mRouting->SetOpCompletion(a, ` a routing completion of call `b` `)`
    RoutingSetOpCompletion,
    /// `mRouting->SetDefaultOpCompletion(` a routing completion of call `a` `)`
    RoutingSetDefaultOpCompletion,
    /// `mRouting->ClearProductName()`
    RoutingClearProductName,
    /// `mRouting->ConnectAsync(*(IPAddr*)a, 15000)`
    RoutingConnect,
    /// `mRouting->GetMyClientId()`
    RoutingMyClientId,
    /// `mRouting->IgnoreClient(a, b != 0)`
    RoutingIgnoreClient,
    /// `PlatMutexLock(mRouting->GetDataLock())` / `PlatMutexUnlock(...)`
    DataLock,
    DataUnlock,
    /// `mRouting->StartGroupItr()`
    StartGroupItr,
    /// `mRouting->StartMemberItr(a)`
    StartMemberItr,
    /// `ee_won_smart_get(mRouting->mModel, a, b, 0, (EeWonSmartRecord*)c)`
    SmartGet,
    /// `ee_won_smart_find(mRouting->mModel, (const wchar_t*)a, b, (EeWonSmartRecord*)c, (bool*)d)`
    SmartFind,
    /// `((AsyncOp*)a)->GetStatus()`
    OpStatus,
    /// `((RoutingOp*)a)->Num(b)`
    OpNum,
    /// `((RoutingOp*)a)->GetType()`
    OpType,
    /// `((RoutingOp*)a)->Text(b)` into the wide sink `c`
    OpText,
    /// `((RoutingOp*)a)->Recipients().empty()`
    OpRecipientsEmpty,
    /// `((RoutingOp*)a)->GetData()`'s bytes into the sink `b`; 0 when the op has no buffer
    OpData,
    /// `ee_won_routeop_records(((RoutingOp*)a)->mRust, b, ee_won_wonnet_record, (void*)c)`
    OpRecords,
    /// `((SocketOp*)a)->GetSocket() == mRouting.get()`
    OpSocketIsRouting,
    /// `((PeerAuthOp*)a)->GetSocket() == mRouting.get()`
    OpPeerSocketIsRouting,
    /// `((RoutingOp*)a)->mConnection.get() == mRouting.get()`
    OpOnRouting,
    /// `mUser->GetName()` into the wide sink `a`
    UserName,
    /// the resource string `a` (`gResource->GetString`) into the byte sink `b`
    TextLoad,
    /// `wcstombs((char*)a, (const wchar_t*)b, c)`
    Wcstombs,
    /// `gEEWonNetUI.mDisplayMessage(UWideString((const char*)a), 1, NULL, NULL, true)`
    ShowMessage,
    /// the same with `CloseMessageCallback` (the lost connection: back to the main menu)
    ShowMessageLost,
    /// the same with `(UIForm*)this, HostGameCallback` (the firewall warning)
    ShowMessageHost,
    /// `gEEWonNetUI.mDisplayCancel(UWideString((const char*)a), 1, (UIForm*)&mCancelled, CancelConnectCallback,
    /// gUIManager->GetFormByID(b))`
    ShowCancelConnect,
    /// the same with `&mJoinRoomCancelled`
    ShowCancelJoinRoom,
    /// the same with `(UIForm*)mAccountOp.get(), CancelOpCallback`
    ShowCancelAccount,
    /// `gEEWonNetUI.mSavePreferences()`
    SavePreferences,
    /// `gHostedGameName = UWideString((const char*)a)`
    SetHostedGameName,
    /// `gUIManager->GetFormByID(a)`
    FormById,
    /// `((std::wstring*)a)->assign((const wchar_t*)b, c)`
    WideOut,
    /// `gUIManager->GetCurrentForm()`
    CurrentForm,
    /// `((UIForm*)a)->GetControl(b)`
    FormControl,
    /// `gUserInterface->CloseForm((UIForm*)a)`
    CloseForm,
    /// `gUIManager->LockUIForCopyData()` / `UnlockUIForCopyData()`
    LockUi,
    UnlockUi,
    /// `gUIManager->PopTo((UIForm*)a)`
    PopTo,
    /// `gUIManager->Push((UIForm*)a)`
    Push,
    /// `gUserInterface->SwitchToForm((UIForm*)a)`
    SwitchToForm,
    /// `gEEWonNetUI.mNewRoomsOpenScreen()` / `mNewGamesOpenScreen()`
    NewRoomsOpenEvent,
    NewGamesOpenEvent,
    /// `((UIForm*)a)->SetFormOpenEvent((UIEvent*)b)`
    FormSetOpenEvent,
    /// `((UIEvent*)a)->Dereference()`
    EventDereference,
    /// `((UICheckBox*)a)->GetIsChecked()` / `SetChecked(b != 0)`
    CheckBoxChecked,
    CheckBoxSetChecked,
    /// `((UIEditBox*)a)->SetText(UWideString((const char*)b))`
    EditBoxSetText,
    /// `((UITextLabel*)a)->SetText(UWideString((const char*)b))`
    LabelSetText,
    /// `new EEAJoinOrHostAGame(a, UWideString((const char*)b), UWideString((const char*)c))`
    NewJoinOrHost,
    /// `new EEAWonNet((EeWonAction*)a)`: the action's C++ shell, which takes the action (round 37: wonaction.rs
    /// makes a game row, a player row or a chat line as the three constructors did)
    NewWonAction,
    /// `gUserInterface->PostAction((EEAction*)a)`
    PostAction,
    /// `((EEAction*)a)->Dereference()`
    ActionDereference,
    /// `gUserInterface->PostLoginDone()`
    PostLoginDone,
    /// the rooms box `a`: `GetCurrentSelection(*(unsigned long*)b)`, `DeleteAllEntries()`, `GetLineCount()`,
    /// `ChangeSelectionIndex(b, true, false)`, `AppendLine(sRoomLine)`
    RoomsBoxSelection,
    RoomsBoxClear,
    RoomsBoxLineCount,
    RoomsBoxSelect,
    RoomsBoxAppend,
    /// the room line's fields: `mDescription = UWideString((const char*)a)`, `mName = ...`, `mNumPlayers = a`,
    /// `mNumGames = a`
    RoomLineDescription,
    RoomLineName,
    RoomLinePlayers,
    RoomLineGames,
    /// `NEProtocol::GetProtocol(0, sProtocol)`
    ProtocolGet,
    /// `sProtocol->GetLocalAddress(gNetworkAdapter)`'s bytes into the sink `a`
    ProtocolLocalAddress,
    /// the requests (EEWonNet.cpp's `WonRequests` on `mRouting`); `call` makes a routing completion (-1: none)
    /// `Register(L"", L"", gWonClientId, 0, 5, a, call b)`
    ReqRegister,
    /// `SubscribeDataObject(0xffff, a ? kWonRoomObject : kWonGameObject, true, call b)`
    ReqSubscribe,
    /// `UnsubscribeDataObject(0xffff, kWonGameObject, true, NULL)`
    ReqUnsubscribeGames,
    /// `CreateGroup(0xffff, (wide)a, (wide)b, 8, 0, true, 0, call c)` (wide: `(const wchar_t*)` units, NUL-ended)
    ReqCreateGroup,
    /// `CreateDataObject(true, a, kWonGameObject, (wide)b, new ByteBuffer(` a copy of `c`, `d` bytes `), call
    /// JoinGameData)`
    ReqCreateGameObject,
    /// `ReplaceDataObject(a, kWonGameObject, (wide)b, NULL)`
    ReqReplaceGameObject,
    /// `ModifyDataObject(a, kWonGameObject, (wide)b, 0, 0, new ByteBuffer(` a copy of `c`, `d` bytes `), NULL)`
    ReqModifyGameObject,
    /// `ReadDataObject(a, kWonGameObject, true, call b)`
    ReqReadGameObject,
    /// `JoinGroup(a, (wide)b, L"", 0, 0, call c)`
    ReqJoinGroup,
    /// `CancelJoinGroup(a, true, NULL)`
    ReqCancelJoinGroup,
    /// `LeaveGroup(a, NULL)`
    ReqLeaveGroup,
    /// `GetGroupList(0xff, call a)`
    ReqGroupList,
    /// `GetClientList(call a)`
    ReqClientList,
    /// `GetMembersOfGroup(a, call b)`
    ReqMembers,
    /// `SendChat((wide)a, 0, b, call c)`
    ReqSendChat,
    /// `SendChatTo((wide)a, 4, ` the list `b`, `c` `, NULL)`
    ReqSendChatTo,
    /// `CDKey(kWonProductName)` loaded from the registry and valid (`LoadFromRegistry() && mValid`)
    KeyValid,
    /// `CDKey(kWonProductName).SaveToRegistry()`
    KeySave,
    /// `new GetCertOp(mUser)` (a `RefCount*`, held as `login_op`), its completion (call a), `SetCreateAccount(false)`, `RunAsync(15000)`
    LoginOpNew,
    LoginOpSetCompletion,
    LoginOpNoCreate,
    LoginOpRun,
    /// `a == mLoginOp.get()`
    LoginOpIs,
    /// `RustServerOp::NewGetDir(mDirServers.get(), kWonProductDirectory, 0x703003a)` (held as `dir_op`), its
    /// completion
    /// (call a), `RunAsync(15000)`
    DirOpNew,
    DirOpSetCompletion,
    DirOpRun,
    /// `a == mDirOp.get()`
    DirOpIs,
    /// `mServers.clear()`
    ServersClear,
    /// `sEntities = ((RustServerOp*)a)->GetEntities(EE_WON_REQOP_DIR_LIST)`, `sEntity = sEntities.begin()`,
    /// `sEntity == sEntities.end()`, `(*sEntity)->mName == kWonRoutingServerType`, `mServers.push_back(*sEntity)`,
    /// `sEntity++`
    EntitiesLoad,
    EntityFirst,
    EntityAtEnd,
    EntityIsRouting,
    EntityKeep,
    EntityNext,
    /// `sFile`: `SetVolume(gStartupVolume)`, `SetDirectory(gStartupDirectory)`, `SetFile(UString("SierraCom.cfg"))`,
    /// `FOpen("r") == 0`, `fgets((char*)a, 256, sFile) != NULL`, `Close()`
    FileSetVolume,
    FileSetDirectory,
    FileSetName,
    FileOpen,
    FileGets,
    FileClose,
    /// `new ServerContext` (held as `dir_servers`)
    DirServersNew,
    /// `mDirServers->AddAddress(IPAddr(std::string((const char*)a)))`
    DirServersAdd,
    /// `RustServerOp::NewUpdateAccount(mAuthServers, mUser, (wide)a, std::string((const char*)b))` into `sOp`,
    /// its completion (call c), `SetNum(EE_WON_REQOP_ACCOUNT_CLEAR_NEWSLETTERS, 0)`,
    /// `SetNum(EE_WON_REQOP_ACCOUNT_NEWSLETTER, a)`, `RunAsync(15000)`, `sOp = NULL`
    UpdateOpNew,
    UpdateOpSetCompletion,
    UpdateOpClearNewsletters,
    UpdateOpNewsletter,
    UpdateOpRun,
    UpdateOpRelease,
    /// `RustServerOp::NewGetAccount(mAuthServers, mUser)` (held as `account_op`), its completion (call a), `RunAsync(15000)`,
    /// `SetNum(EE_WON_REQOP_ACCOUNT_NEWSLETTER, 1)`
    AccountOpNew,
    AccountOpSetCompletion,
    AccountOpRun,
    AccountOpNewsletter,
    /// `((RustServerOp*)a)->GetNum(EE_WON_REQOP_ACCOUNT_NEWSLETTER, 1)`
    AccountNewsletter,
    /// `((RustServerOp*)a)->GetBytes(EE_WON_REQOP_ACCOUNT_EMAIL)` into the byte sink `b`
    AccountEmail,
    /// the C++ game info `a` (`EEWonNetGameInfo*`): its texts' bytes (`(const char*)mGameName` ..), its numbers,
    /// and the setters `mGameName = (const char*)b` .. / `mHumans = b` ..
    InfoName,
    InfoAddress,
    InfoPassword,
    InfoHumans,
    InfoPlayers,
    InfoHost,
    InfoFlags,
    InfoRoom,
    InfoSetName,
    InfoSetAddress,
    InfoSetPassword,
    InfoSetHumans,
    InfoSetPlayers,
    InfoSetHost,
    InfoSetFlags,
    InfoSetRoom,
    /// `((RefCount*)a)->AddRef()` / `((RefCount*)a)->Release()`: a slot's reference (the members' SmartPtr)
    RefAdd,
    RefRelease,
    /// `new DirList` (the server list, `servers`) / `delete (DirList*)a`
    ServersNew,
    ServersDelete,
}

/// The C++ objects EEWonNet held as members (+0x004..+0x189), the Rust object's slots; the host reads them through
/// `ee_won_wonnet_objects`. Each pointer is the object as its own class (`routing` a `SmartRoutingConnection*`
/// as a `RefCount*`, the locks `TSCriticalSection*`); the counted ones (routing, dir_servers, the ops) hold a
/// reference, as the SmartPtr members did. The flags are written by the cancel boxes through their address.
#[repr(C)]
#[derive(Debug)]
pub struct EeWonNetObjects {
    pub lock: *mut c_void,
    pub lock8: *mut c_void,
    pub routing: *mut c_void,
    pub dir_servers: *mut c_void,
    pub auth_servers: *mut c_void,
    pub lobby: *mut c_void,
    pub lobby_window: *mut c_void,
    pub user: *mut c_void,
    pub servers: *mut c_void,
    pub motd_op: *mut c_void,
    pub login_op: *mut c_void,
    pub dir_op: *mut c_void,
    pub account_op: *mut c_void,
    pub cancelled: bool,
    pub join_room_cancelled: bool,
}

impl Default for EeWonNetObjects {
    fn default() -> Self {
        let n = core::ptr::null_mut();
        Self {
            lock: n,
            lock8: n,
            routing: n,
            dir_servers: n,
            auth_servers: n,
            lobby: n,
            lobby_window: n,
            user: n,
            servers: n,
            motd_op: n,
            login_op: n,
            dir_op: n,
            account_op: n,
            cancelled: false,
            join_room_cancelled: false,
        }
    }
}

/// A counted slot of [`EeWonNetObjects`].
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Slot {
    Routing,
    DirServers,
    MotdOp,
    LoginOp,
    DirOp,
    AccountOp,
}

/// The C++ forwards' calls: `(obj, call, a, b, c, d)`; the completions' too (an `AsyncOp*` in `a`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonNetCall {
    /// the constructor's body
    Construct,
    /// `ConnectToLobby()`
    ConnectToLobby,
    /// `FnCFFDD(a)`, `FnCFFF2()`
    FnCFFDD,
    FnCFFF2,
    /// `SelectServer(name units a, count b, (IPAddr*)c)`
    SelectServer,
    /// `HostGame((const char*)a, (const char*)b)`, `JoinGame(..)`, `JoinRoom(..)` (b may be NULL)
    HostGame,
    JoinGame,
    JoinRoom,
    LeaveRoom,
    /// `GetGameInfo((EEWonNetGameInfo*)a)`, `SetGameInfo(..)`
    GetGameInfo,
    SetGameInfo,
    ClearGameList,
    /// `RejoinRoom(a != 0)` -> bool
    RejoinRoom,
    /// `RefreshGameList(a)` (the games box, unused)
    RefreshGameList,
    UpdateGameList,
    /// `UpdateAccount(name bytes a, password bytes b, flag c)`
    UpdateAccount,
    OpenUpdateAccount,
    /// `SendChat(text bytes a)`
    SendChat,
    /// `UpdateRoomList(rooms box a, label 16 b, label 14 c)`
    UpdateRoomList,
    CreateGame,
    UpdateGameObject,
    Disconnect,
    LeaveGroups,
    ClearGames,
    /// `Initialize()` -> bool
    Initialize,
    GetTitanServerList,
    /// `Login((const char*)a, (const char*)b)`
    Login,
    /// the readers the game's screens use: `mGameListPort`, `mRoomListChanged`, `mIsBehindFirewall`, `mRoomName`
    /// (into the `std::wstring*` `a`)
    GameListPort,
    RoomListChanged,
    IsBehindFirewall,
    RoomName,
    // the completions (an `AsyncOp*` in `a`)
    ListGamesCallback,
    ListRoomCallback,
    CloseCallback,
    TempCallback,
    ConnectCallback,
    ChatRoomListCallback,
    JoinGameDataCallback,
    CreateGameCallback,
    JoinGroupCallback,
    JoinGameObjectsCallback,
    JoinRoomCallback,
    ClientListCallback,
    MembersCallback,
    UpdateAccountCallback,
    GetAccountCallback,
    OnChat,
    OnDataObjectChanged,
    OnDataObjectCreated,
    OnDataObjectDeleted,
    OnDataObjectModified,
    OnGroupMaxPlayersChanged,
    RegisterCallback,
    SendChatCallback,
    LoginCallback,
    GetTitanServerListCompletion,
}

/// The host: `(cpp, op, a, b, c, d) -> result`.
pub type EeWonNetHostFn =
    Option<unsafe extern "C" fn(cpp: *mut c_void, op: EeWonNetOp, a: isize, b: isize, c: isize, d: isize) -> isize>;

static HOST: AtomicUsize = AtomicUsize::new(0);

/// Sets the host (EEWonNet.cpp at static initialisation, a driver before its first call).
#[no_mangle]
pub extern "C" fn ee_won_wonnet_set_host(host: EeWonNetHostFn) {
    HOST.store(host.map_or(0, |f| f as usize), Ordering::Relaxed);
}

/// A sink the host fills through [`ee_won_wonnet_sink`]: bytes or `wchar_t` units, each kept as a `u32`.
#[derive(Default)]
pub struct WonNetSink {
    units: Vec<u32>,
    /// whether anything was written
    set: bool,
}

/// Appends `count` units of `unit` bytes each (1: bytes, 2 or 4: `wchar_t`) to the sink.
///
/// # Safety
/// `sink` is a live [`WonNetSink`], `data` holds `count` units.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wonnet_sink(sink: *mut c_void, data: *const c_void, count: usize, unit: u32) {
    let s = &mut *(sink as *mut WonNetSink);
    s.set = true;
    for i in 0..count {
        let v = match unit {
            1 => *(data as *const u8).add(i) as u32,
            2 => *(data as *const u16).add(i) as u32,
            _ => *(data as *const u32).add(i),
        };
        s.units.push(v);
    }
}

/// A finished routing op's record (`ee_won_routeop_records`' callback): a data object, a member's client id or a
/// client's name, into the [`Records`] given.
///
/// # Safety
/// `ctx` is a live [`Records`], `record` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wonnet_record(ctx: *mut c_void, record: *const EeWonRouteRecord) {
    let r = &*record;
    let out = &mut *(ctx as *mut Records);
    match r.kind {
        EE_WON_RTR_OBJECT => {
            let data_type = std::slice::from_raw_parts(r.data_type, r.data_type_len).to_vec();
            let data = if r.has_data { std::slice::from_raw_parts(r.data, r.data_len).to_vec() } else { Vec::new() };
            out.objects.push(RouteObject { link_id: r.id, data_type, data });
        }
        EE_WON_RTR_MEMBER => out.members.push(r.id),
        EE_WON_RTR_CLIENT => {
            // WonWideSink: the UTF-16 name as wchar_t units
            let mut wide = vec![0 as WChar; r.name_len];
            let n = if r.name_len > 0 {
                ee_won_net::ffi::ee_won_wide_from_utf16(
                    r.name,
                    r.name_len,
                    core::mem::size_of::<WChar>() as u32,
                    wide.as_mut_ptr() as *mut c_void,
                    r.name_len,
                )
            } else {
                0
            };
            out.clients.insert(r.key, wide[..n].iter().map(|&u| u as u32).collect());
        }
        _ => {}
    }
}

/// A data object of a finished op.
#[derive(Clone, Debug, Default)]
pub struct RouteObject {
    pub link_id: u16,
    pub data_type: Vec<u8>,
    pub data: Vec<u8>,
}

/// What `ee_won_wonnet_record` collects.
#[derive(Default)]
pub struct Records {
    pub objects: Vec<RouteObject>,
    pub members: Vec<u16>,
    pub clients: BTreeMap<u16, Vec<u32>>,
}

/// A published game (EEWonNetGameInfo): the texts' bytes, the counts, the host, the flags (bit 0 open, bit 1
/// firewalled) and the room.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct GameInfo {
    pub name: Vec<u8>,
    pub address: Vec<u8>,
    pub password: Vec<u8>,
    pub humans: u32,
    pub players: u32,
    pub host: u32,
    pub flags: u32,
    pub room: u16,
}

/// A room (EEWonNetRoomInfo).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct RoomInfo {
    pub name: Vec<u8>,
    pub description: Vec<u8>,
    pub max_players: u32,
    pub id: u32,
}

/// A routing group of the smart connection's model.
#[derive(Clone, Debug)]
struct Group {
    name: Vec<u32>,
    id: u16,
    flags: u32,
    member_count: u16,
}

const NONE: u16 = 0xffff;
/// A request without a completion (the `call` argument of the request ops).
const NO_CALL: isize = -1;
const GAME_INFO_VERSION: u16 = 1;

// the forms and controls (EEUIManager.h)
const FORM_LOBBY: isize = 26;
const FORM_MULTIPLAYER: isize = 30;
const FORM_AVAILABLE_ROOMS: isize = 31;
const FORM_AVAILABLE_GAMES: isize = 32;
const FORM_UPDATE_SIERRA_ACCOUNT: isize = 37;
const FORM_SELECT_SERVER: isize = 38;
const AG_IN_PROGRESS_TOGGLE: isize = 32;
const AG_PASSWORD_TOGGLE: isize = 33;
const AG_PRIVATE_TOGGLE: isize = 34;
const UA_ENTER_EMAIL_EDIT_BOX: isize = 6;
const UA_SEND_ME_EMAIL_CHECKBOX: isize = 7;
const CHAT_LABEL: isize = 15;

// the strings (EEStringIDs.h)
const IDS_SETUP_PLAYERS: isize = 7206;
const IDS_YOU_HAVE_LOST_YOUR_CONNECTION_TO_SIERRA: isize = 32808;
const IDS_CONNECTING_TO_SERVER: isize = 31848;
const IDS_UNABLE_TO_ACCESS_SERVER_AT_THIS_TIME: isize = 30425;
const IDS_FAILED_TO_CREATE_GAME: isize = 30432;
const IDS_GAME_WITH_THAT_NAME_ALREADY_EXISTS: isize = 30430;
const IDS_SIERRA_COM_IS_UNABLE_TO_VERIFY: isize = 30474;
const IDS_COULDNT_FIND_GAME_REQUESTED: isize = 10066;
const IDS_PASSWORD_IS_INCORRECT: isize = 30413;
const IDS_COULDNT_JOIN_GAME: isize = 31853;
const IDS_SPECIFIED_GAME_NOT_FOUND: isize = 30440;
const IDS_JOINING_ROOM: isize = 31864;
const IDS_COULDNT_JOIN_ROOM: isize = 31854;
const IDS_SUCCESSFULLY_MODIFIED_YOUR_ACCOUNT: isize = 31857;
const IDS_COULD_NOT_MODIFY_YOUR_ACCOUNT: isize = 31858;
const IDS_GETTING_CURRENT_USER_INFO: isize = 31859;
const IDS_THERE_WAS_ERROR_GETTING_CURRENT_USER: isize = 31860;
const IDS_SIERRA_COM_USER_IS_ALREADY: isize = 30441;
const IDS_IS_NOW_IGNORING: isize = 30476;
const IDS_IS_NO_LONGER_IGNORING: isize = 30477;
const IDS_WHISPERED: isize = 30475;

/// The routing op completions that `SelectServer` installs, by op type.
const ROUTING_OP_CHAT: isize = 39;
const ROUTING_OP_DATA_CREATED: isize = 13;
const ROUTING_OP_DATA_DELETED: isize = 14;
const ROUTING_OP_DATA_MODIFIED: isize = 15;
const ROUTING_OP_CLIENT_JOINED_GROUP: isize = 7;
const ROUTING_OP_CLIENT_LEFT_GROUP: isize = 9;
const ROUTING_OP_MAX_PLAYERS_CHANGED: isize = 30;

/// The game's WON front end (the original's EEWonNet data; see the module's notes).
pub struct EeWonNet {
    cpp: *mut c_void,
    objects: UnsafeCell<EeWonNetObjects>,
    password: RefCell<Vec<u32>>,
    game_name: RefCell<Vec<u32>>,
    room_name: RefCell<Vec<u32>>,
    current_room: RefCell<Vec<u32>>,
    server_name: RefCell<Vec<u32>>,
    game_info: RefCell<GameInfo>,
    games: RefCell<BTreeMap<u16, GameInfo>>,
    rooms: RefCell<BTreeMap<u16, RoomInfo>>,
    clients: RefCell<BTreeMap<u16, Vec<u32>>>,
    directory_servers: RefCell<Vec<Vec<u8>>>,
    l168: Cell<i32>,
    l16c: Cell<i32>,
    join_group_id: Cell<u16>,
    game_group_id: Cell<u16>,
    reply_client_id: Cell<u16>,
    join_room_id: Cell<u16>,
    game_list_port: Cell<u16>,
    room_group_id: Cell<u16>,
    is_behind_firewall: Cell<bool>,
    b187: Cell<bool>,
    ignore_close: Cell<bool>,
    is_listing_players: Cell<bool>,
    room_list_changed: Cell<bool>,
    games_changed: Cell<bool>,
    b18e: Cell<bool>,
}

/// The names RoutingOpTypeToString (0x4cd4f9) gave the routing op types (ee_won_net routeop_types.rs's ids, in
/// order); "Unknown Routing Type" past them. The front end computed and dropped them (TempCallback,
/// JoinGameDataCallback).
pub const ROUTING_OP_TYPE_NAMES: [&str; 61] = [
    "RoutingOp_Undefined",
    "RoutingOp_AbortServerShutdown",
    "RoutingOp_AcceptClient",
    "RoutingOp_BanClient",
    "RoutingOp_BecomeModerator",
    "RoutingOp_CancelJoinGroup",
    "RoutingOp_ClientFlagsChanged",
    "RoutingOp_ClientJoinedGroup",
    "RoutingOp_ClientJoinedServer",
    "RoutingOp_ClientLeftGroup",
    "RoutingOp_ClientLeftServer",
    "RoutingOp_CreateDataObject",
    "RoutingOp_CreateGroup",
    "RoutingOp_DataObjectCreated",
    "RoutingOp_DataObjectDeleted",
    "RoutingOp_DataObjectModified",
    "RoutingOp_DeleteDataObject",
    "RoutingOp_DetectFirewall",
    "RoutingOp_DisconnectClient",
    "RoutingOp_GetBadUserList",
    "RoutingOp_GetClientList",
    "RoutingOp_GetGroupList",
    "RoutingOp_GetMembersOfGroup",
    "RoutingOp_GroupCaptainChanged",
    "RoutingOp_GroupCreated",
    "RoutingOp_GroupDeleted",
    "RoutingOp_GroupFlagsChanged",
    "RoutingOp_GroupInvitation",
    "RoutingOp_GroupJoinAttempt",
    "RoutingOp_GroupMaxPlayersChanged",
    "RoutingOp_GroupMemberCount",
    "RoutingOp_GroupNameChanged",
    "RoutingOp_GroupObserverCount",
    "RoutingOp_InviteClient",
    "RoutingOp_JoinGroup",
    "RoutingOp_LeaveGroup",
    "RoutingOp_MemberFlagsChanged",
    "RoutingOp_ModifyDataObject",
    "RoutingOp_MuteClient",
    "RoutingOp_PeerChat",
    "RoutingOp_PeerData",
    "RoutingOp_ReadDataObject",
    "RoutingOp_RegisterClient",
    "RoutingOp_RelinquishCaptaincy",
    "RoutingOp_SendChat",
    "RoutingOp_SendComplaint",
    "RoutingOp_SendData",
    "RoutingOp_SendServerAlert",
    "RoutingOp_ServerAlert",
    "RoutingOp_ServerShutdownAborted",
    "RoutingOp_ServerShutdownStarted",
    "RoutingOp_SetClientFlags",
    "RoutingOp_SetGroupFlags",
    "RoutingOp_SetGroupMaxPlayers",
    "RoutingOp_SetGroupName",
    "RoutingOp_SetGroupPassword",
    "RoutingOp_StartServerShutdown",
    "RoutingOp_UnsubscribeDataObject",
    "RoutingOp_YouWereBanned",
    "RoutingOp_YouWereMuted",
    "RoutingOp_DerivedServerOp",
];

/// RoutingOpTypeToString.
pub fn routing_op_type_name(kind: i64) -> &'static str {
    if (0..61).contains(&kind) {
        ROUTING_OP_TYPE_NAMES[kind as usize]
    } else {
        "Unknown Routing Type"
    }
}

/// A text as the C string it was passed as: up to its first NUL.
fn c_text(bytes: &[u8]) -> &[u8] {
    &bytes[..bytes.iter().position(|&b| b == 0).unwrap_or(bytes.len())]
}

/// A NUL-terminated copy of bytes, for a `(const char*)` argument.
fn cz(bytes: &[u8]) -> Vec<u8> {
    let mut v = bytes.to_vec();
    v.push(0);
    v
}

/// A NUL-terminated `wchar_t` copy of wide units, for a `(const wchar_t*)` argument.
fn wz(units: &[u32]) -> Vec<WChar> {
    let mut v: Vec<WChar> = units.iter().map(|&u| u as WChar).collect();
    v.push(0 as WChar);
    v
}

/// The bytes of a C string up to its terminator (empty for NULL).
unsafe fn c_bytes(p: *const c_char) -> Vec<u8> {
    if p.is_null() {
        return Vec::new();
    }
    std::ffi::CStr::from_ptr(p).to_bytes().to_vec()
}

/// The bytes up to the first NUL.
fn until_nul(bytes: &[u8]) -> &[u8] {
    &bytes[..bytes.iter().position(|&b| b == 0).unwrap_or(bytes.len())]
}

/// A `UWideString` over ee_base's record (the chat's character arithmetic, as the C++ did it).
struct Uw(EeUWideString);
impl Uw {
    fn new(text: &[u8]) -> Self {
        let z = cz(until_nul(text));
        let mut s = EeUWideString { string: core::ptr::null_mut(), byte_length: 0, length: 0, capacity: 0 };
        // SAFETY: a fresh record and a C string
        unsafe { ee_uwstring_init_text(&mut s, z.as_ptr() as *const c_char) };
        Uw(s)
    }
    fn empty() -> Self {
        let mut s = EeUWideString { string: core::ptr::null_mut(), byte_length: 0, length: 0, capacity: 0 };
        // SAFETY: a fresh record
        unsafe { ee_uwstring_init(&mut s) };
        Uw(s)
    }
    fn len(&self) -> i32 {
        self.0.length as i32
    }
    fn at(&self, i: i32) -> u8 {
        // SAFETY: a live record
        unsafe { ee_uwstring_at(&self.0, i) as u8 }
    }
    fn ptr(&self) -> *const u8 {
        self.0.string as *const u8
    }
    fn bytes(&self) -> Vec<u8> {
        // SAFETY: a live record's C string
        unsafe { c_bytes(self.0.string) }
    }
    /// `CopyString(source, count)`
    fn copy_from(&mut self, source: *const u8, count: i32) {
        // SAFETY: a live record; the source a C string of at least `count` characters (the original's contract)
        unsafe { ee_uwstring_copy_text(&mut self.0, source as *const c_char, count as _) };
    }
}
impl Drop for Uw {
    fn drop(&mut self) {
        // SAFETY: a live record
        unsafe { ee_uwstring_free(&mut self.0) };
    }
}

/// `EEWonNet::ToString`: a wide text to UTF-8 (at most four bytes a unit, cut at the first NUL).
pub fn wide_to_bytes(units: &[u32]) -> Vec<u8> {
    let size = units.len() * 4 + 1;
    let mut buffer = vec![0u8; size];
    let w = wz(units);
    // W2A: at most size - 1 bytes, terminated
    // SAFETY: w holds units.len() units, buffer size bytes
    let n = unsafe {
        PlatWideToUtf8(w.as_ptr(), units.len() as u32, buffer.as_mut_ptr() as *mut c_char, (size - 1) as u32)
    };
    buffer[n as usize] = 0;
    until_nul(&buffer).to_vec()
}

/// `EEWonNet::ToWString`: a UTF-8 text to wide (empty for NULL or ""); as many units as it has characters.
pub fn bytes_to_wide(text: &[u8]) -> Vec<u32> {
    let text = until_nul(text);
    if text.is_empty() {
        return Vec::new();
    }
    let z = cz(text);
    // SAFETY: a C string
    let length = unsafe { PlatMbsLen(z.as_ptr()) };
    let mut buffer = vec![0 as WChar; length + 1];
    // A2W: the bytes of the first `length` characters, into `length` units
    // SAFETY: as above
    unsafe {
        let bytes = PlatMbsNBCnt(z.as_ptr(), length);
        PlatUtf8ToWide(z.as_ptr() as *const c_char, bytes as u32, buffer.as_mut_ptr(), length as u32);
    }
    buffer[length] = 0 as WChar;
    let end = buffer.iter().position(|&u| u == 0 as WChar).unwrap_or(buffer.len());
    buffer[..end].iter().map(|&u| u as u32).collect()
}

/// ExportGameInfo: version 1, the three texts (UWideString::Export: their size + 2, the bytes, two zeros), the
/// counts, the host, the flags and the room.
pub fn export_game_info(info: &GameInfo) -> Vec<u8> {
    let mut out = Vec::new();
    out.extend_from_slice(&GAME_INFO_VERSION.to_le_bytes());
    for text in [&info.name, &info.address, &info.password] {
        let text = until_nul(text);
        out.extend_from_slice(&(text.len() as u32).wrapping_add(2).to_le_bytes());
        out.extend_from_slice(text);
        out.extend_from_slice(&[0, 0]);
    }
    for v in [info.humans, info.players, info.host, info.flags] {
        out.extend_from_slice(&v.to_le_bytes());
    }
    out.extend_from_slice(&info.room.to_le_bytes());
    out
}

/// ImportGameInfo into `info` (only the fields read change: the flags first cleared, as the original): version
/// 1's texts, an older record's C strings (each advancing by its characters + 1, as the original did), then the
/// counts. A short buffer reads as zeros past its end (the original read past it).
pub fn import_game_info(data: &[u8], info: &mut GameInfo) {
    let byte = |i: usize| data.get(i).copied().unwrap_or(0);
    let u32_at = |i: usize| u32::from_le_bytes([byte(i), byte(i + 1), byte(i + 2), byte(i + 3)]);
    info.flags = 0;
    let version = u16::from_le_bytes([byte(0), byte(1)]);
    let mut at;
    // a C string at i (bytes up to the terminator, zeros past the end)
    let c_string = |i: usize| -> Vec<u8> {
        let mut v = Vec::new();
        let mut k = i;
        while byte(k) != 0 {
            v.push(byte(k));
            k += 1;
        }
        v
    };
    if version == 1 {
        // UWideString::Import: a size, then that many bytes (the text up to its terminator); size + 4 read
        let import = |i: usize| -> (Vec<u8>, usize) {
            let size = u32_at(i) as usize;
            let text: Vec<u8> = (0..size).map(|k| byte(i + 4 + k)).collect();
            (until_nul(&text).to_vec(), size + 4)
        };
        let (name, n) = import(2);
        at = n + 2;
        let (address, n) = import(at);
        at += n;
        let (password, n) = import(at);
        at += n;
        info.name = name;
        info.address = address;
        info.password = password;
    } else {
        let chars = |v: &[u8]| -> usize {
            let z = cz(v);
            // SAFETY: a C string
            unsafe { PlatMbsLen(z.as_ptr()) }
        };
        info.name = c_string(0);
        at = chars(&info.name) + 1;
        info.address = c_string(at);
        at += chars(&info.address) + 1;
        info.password = c_string(at);
        at += chars(&info.password) + 1;
    }
    info.humans = u32_at(at);
    at += 4;
    info.players = u32_at(at);
    at += 4;
    info.host = u32_at(at);
    at += 4;
    info.flags = u32_at(at);
    at += 4;
    info.room = u16::from_le_bytes([byte(at), byte(at + 1)]);
}

impl EeWonNet {
    fn new(cpp: *mut c_void) -> Self {
        Self {
            cpp,
            objects: UnsafeCell::new(EeWonNetObjects::default()),
            password: RefCell::default(),
            game_name: RefCell::default(),
            room_name: RefCell::default(),
            current_room: RefCell::default(),
            server_name: RefCell::default(),
            game_info: RefCell::default(),
            games: RefCell::default(),
            rooms: RefCell::default(),
            clients: RefCell::default(),
            directory_servers: RefCell::default(),
            l168: Cell::new(0),
            l16c: Cell::new(0),
            join_group_id: Cell::new(NONE),
            game_group_id: Cell::new(NONE),
            reply_client_id: Cell::new(NONE),
            join_room_id: Cell::new(NONE),
            game_list_port: Cell::new(NONE),
            room_group_id: Cell::new(NONE),
            is_behind_firewall: Cell::new(false),
            b187: Cell::new(false),
            ignore_close: Cell::new(false),
            is_listing_players: Cell::new(false),
            room_list_changed: Cell::new(false),
            games_changed: Cell::new(false),
            b18e: Cell::new(false),
        }
    }

    fn op(&self, op: EeWonNetOp, a: isize, b: isize, c: isize, d: isize) -> isize {
        let h = HOST.load(Ordering::Relaxed);
        if h == 0 {
            return 0;
        }
        // SAFETY: the host set by `ee_won_wonnet_set_host` (a function of that type)
        let h: unsafe extern "C" fn(*mut c_void, EeWonNetOp, isize, isize, isize, isize) -> isize =
            unsafe { std::mem::transmute(h) };
        // SAFETY: the C++ host's ops
        unsafe { h(self.cpp, op, a, b, c, d) }
    }
    fn op0(&self, op: EeWonNetOp) -> isize {
        self.op(op, 0, 0, 0, 0)
    }
    fn op1(&self, op: EeWonNetOp, a: isize) -> isize {
        self.op(op, a, 0, 0, 0)
    }
    fn op2(&self, op: EeWonNetOp, a: isize, b: isize) -> isize {
        self.op(op, a, b, 0, 0)
    }

    // ---- the C++ objects' slots

    /// The slots (the host reads and the cancel boxes write them between this object's calls).
    fn objects(&self) -> *mut EeWonNetObjects {
        self.objects.get()
    }
    fn slot(&self, s: Slot) -> *mut *mut c_void {
        let o = self.objects();
        // SAFETY: the object's own slots; no reference into them is kept
        unsafe {
            match s {
                Slot::Routing => &raw mut (*o).routing,
                Slot::DirServers => &raw mut (*o).dir_servers,
                Slot::MotdOp => &raw mut (*o).motd_op,
                Slot::LoginOp => &raw mut (*o).login_op,
                Slot::DirOp => &raw mut (*o).dir_op,
                Slot::AccountOp => &raw mut (*o).account_op,
            }
        }
    }
    /// `member = new` as the SmartPtr member's assignment did: nothing for the same object; else the old one
    /// released (the slot still naming it), the new one referenced, then stored.
    fn hold(&self, s: Slot, new: isize) {
        let slot = self.slot(s);
        // SAFETY: the object's slot
        let old = unsafe { slot.read() } as isize;
        if old == new {
            return;
        }
        if old != 0 {
            self.op1(Op::RefRelease, old);
        }
        if new != 0 {
            self.op1(Op::RefAdd, new);
        }
        // SAFETY: as above
        unsafe { slot.write(new as *mut c_void) };
    }
    fn holds(&self, s: Slot) -> bool {
        // SAFETY: the object's slot
        !unsafe { self.slot(s).read() }.is_null()
    }
    fn lobby_window(&self) -> bool {
        // SAFETY: the object's slot
        !unsafe { (*self.objects()).lobby_window }.is_null()
    }
    fn lobby(&self) -> bool {
        // SAFETY: the object's slot
        !unsafe { (*self.objects()).lobby }.is_null()
    }
    /// `mCancelled` (`join`: `mJoinRoomCancelled`): read as memory a cancel box may have written.
    fn cancelled(&self, join: bool) -> bool {
        let o = self.objects();
        // SAFETY: the object's flag (a C++ callback writes it through its address)
        unsafe {
            if join {
                (&raw const (*o).join_room_cancelled).read_volatile()
            } else {
                (&raw const (*o).cancelled).read_volatile()
            }
        }
    }
    fn set_cancelled(&self, join: bool, v: bool) {
        let o = self.objects();
        // SAFETY: as above
        unsafe {
            if join {
                (&raw mut (*o).join_room_cancelled).write_volatile(v)
            } else {
                (&raw mut (*o).cancelled).write_volatile(v)
            }
        }
    }
    /// The members' destruction after the destructor's body, in reverse order of declaration: the ops, the
    /// server list, the directory servers, the routing connection.
    fn release_objects(&self) {
        for s in [Slot::AccountOp, Slot::DirOp, Slot::LoginOp, Slot::MotdOp] {
            self.hold(s, 0);
        }
        // SAFETY: the object's slot
        let servers = unsafe { (*self.objects()).servers } as isize;
        if servers != 0 {
            self.op1(Op::ServersDelete, servers);
        }
        // SAFETY: as above
        unsafe { (*self.objects()).servers = core::ptr::null_mut() };
        self.hold(Slot::DirServers, 0);
        self.hold(Slot::Routing, 0);
    }
    /// An op that fills a sink; the sink's units.
    fn sunk(&self, op: EeWonNetOp, a: isize, sink_arg: usize) -> (isize, WonNetSink) {
        let mut sink = WonNetSink::default();
        let p = &mut sink as *mut WonNetSink as isize;
        let r = if sink_arg == 0 { self.op(op, p, 0, 0, 0) } else { self.op(op, a, p, 0, 0) };
        (r, sink)
    }
    fn bytes_of(sink: &WonNetSink) -> Vec<u8> {
        sink.units.iter().map(|&u| u as u8).collect()
    }

    // ---- the C++ objects' services

    fn routing(&self) -> bool {
        self.holds(Slot::Routing)
    }
    fn status(&self, op: isize) -> i32 {
        self.op1(Op::OpStatus, op) as i32
    }
    fn num(&self, op: isize, field: i32) -> i64 {
        self.op2(Op::OpNum, op, field as isize) as i64
    }
    fn group_id(&self, op: isize) -> u16 {
        self.num(op, EE_WON_RTF_GROUP_ID) as u16
    }
    fn link_id(&self, op: isize) -> u16 {
        self.num(op, EE_WON_RTF_LINK_ID) as u16
    }
    fn client_id(&self, op: isize) -> u16 {
        self.num(op, EE_WON_RTF_CLIENT_ID) as u16
    }
    fn my_client_id(&self) -> u16 {
        self.op0(Op::RoutingMyClientId) as u16
    }
    fn text(&self, ids: isize) -> Vec<u8> {
        Self::bytes_of(&self.sunk(Op::TextLoad, ids, 1).1)
    }
    fn message(&self, ids: isize) {
        let t = cz(&self.text(ids));
        self.op1(Op::ShowMessage, t.as_ptr() as isize);
    }
    fn message_lost(&self) {
        let t = cz(&self.text(IDS_YOU_HAVE_LOST_YOUR_CONNECTION_TO_SIERRA));
        self.op1(Op::ShowMessageLost, t.as_ptr() as isize);
    }
    fn form(&self, id: isize) -> isize {
        self.op1(Op::FormById, id)
    }
    fn close_form(&self, id: Option<isize>) {
        let f = id.map_or(0, |id| self.form(id));
        self.op1(Op::CloseForm, f);
    }
    fn post(&self, action: isize) {
        self.op1(Op::PostAction, action);
        self.op1(Op::ActionDereference, action);
    }
    /// Posts a WON action: its C++ shell made (it takes the action), posted and let go.
    fn post_won(&self, action: EeWonAction) {
        let shell = self.op1(Op::NewWonAction, Box::into_raw(Box::new(action)) as isize);
        self.post(shell);
    }
    fn user_name(&self) -> Vec<u8> {
        wide_to_bytes(&self.sunk(Op::UserName, 0, 0).1.units)
    }
    fn records(&self, op: isize, kind: i32) -> Records {
        let mut r = Records::default();
        self.op(Op::OpRecords, op, kind as isize, &mut r as *mut Records as isize, 0);
        r
    }
    fn op_data(&self, op: isize) -> Vec<u8> {
        let (_, sink) = self.sunk(Op::OpData, op, 1);
        Self::bytes_of(&sink)
    }
    /// SmartGet: a record of the model under the data lock; its name with a client (always for the groups and
    /// clients).
    fn smart_get(&self, which: EeWonSmartGet, id: u16) -> Option<(EeWonSmartRecord, Option<Vec<u32>>)> {
        let mut rec = blank_record();
        self.op0(Op::DataLock);
        let found =
            self.op(Op::SmartGet, which as isize, id as isize, &mut rec as *mut EeWonSmartRecord as isize, 0) != 0;
        let name = if found && (which != EeWonSmartGet::NextMember || rec.has_client) {
            Some(record_name(&rec))
        } else {
            None
        };
        self.op0(Op::DataUnlock);
        found.then_some((rec, name))
    }
    /// SmartFind: the client a typed name refers to, when the search found one.
    fn smart_find(&self, name: &[u32]) -> Option<EeWonSmartRecord> {
        let mut rec = blank_record();
        let mut found = false;
        let w = wz(name);
        self.op0(Op::DataLock);
        self.op(
            Op::SmartFind,
            w.as_ptr() as isize,
            name.len() as isize,
            &mut rec as *mut EeWonSmartRecord as isize,
            &mut found as *mut bool as isize,
        );
        self.op0(Op::DataUnlock);
        found.then_some(rec)
    }
    /// The routing groups in id order.
    fn routing_groups(&self) -> Vec<Group> {
        let mut groups = Vec::new();
        self.op0(Op::DataLock);
        self.op0(Op::StartGroupItr);
        while let Some((rec, name)) = self.smart_get(EeWonSmartGet::NextGroup, 0) {
            groups.push(Group {
                name: name.unwrap_or_default(),
                id: rec.id,
                flags: rec.flags,
                member_count: rec.member_count,
            });
        }
        self.op0(Op::DataUnlock);
        groups
    }
    /// `wcstombs(buffer, text, max)` into a zeroed 1024-byte buffer: the bytes up to the terminator.
    fn wcstombs(&self, units: &[u32], max: usize) -> Vec<u8> {
        let mut buffer = [0u8; 1024];
        let w = wz(units);
        self.op(Op::Wcstombs, buffer.as_mut_ptr() as isize, w.as_ptr() as isize, max as isize, 0);
        until_nul(&buffer).to_vec()
    }

    // ---- the methods

    /// The constructor's body: the two locks (the other members are this object's defaults).
    fn construct(&self) {
        // the server list member (constructed before the body)
        let servers = self.op0(Op::ServersNew) as *mut c_void;
        // SAFETY: the object's slot
        unsafe { (*self.objects()).servers = servers };
        self.op0(Op::LockCreate);
        self.op0(Op::Lock8Create);
    }

    /// The destructor's body.
    fn destruct(&self) {
        self.ignore_close.set(true);
        if self.lobby_window() {
            self.op0(Op::LobbyWindowClose);
            while self.op0(Op::LobbyWindowClosed) == 0 {}
            self.op0(Op::LobbyWindowDelete);
        }
        self.op0(Op::LobbyDelete);
        self.games.borrow_mut().clear();
        self.rooms.borrow_mut().clear();
        self.directory_servers.borrow_mut().clear();
        self.op0(Op::LockDestroy);
        self.op0(Op::Lock8Destroy);
    }

    /// Resets two counters and walks every routing group and member (the names converted and dropped).
    fn connect_to_lobby(&self) {
        if !self.routing() {
            return;
        }
        self.l16c.set(0);
        self.l168.set(0);
        for g in self.routing_groups() {
            if self.op1(Op::StartMemberItr, g.id as isize) == 0 {
                continue;
            }
            while self.smart_get(EeWonSmartGet::NextMember, 0).is_some() {}
        }
    }

    /// Replaces the game list with the game objects read and marks the game and room lists changed.
    fn list_games_callback(&self, op: isize) {
        let objects = self.records(op, EE_WON_RTR_OBJECT).objects;
        if self.status(op) == WS_Success {
            self.op0(Op::LockEnter);
            let mut info = GameInfo::default();
            self.games.borrow_mut().clear();
            for o in &objects {
                import_game_info(&o.data, &mut info);
                self.games.borrow_mut().insert(o.link_id, info.clone());
                self.games_changed.set(true);
            }
            self.room_list_changed.set(true);
            self.b18e.set(true);
            self.games_changed.set(true);
            self.op0(Op::LockLeave);
        }
    }

    /// Rebuilds the room list from the chat-room groups and the rooms' descriptions from their data objects.
    fn list_room_callback(&self, op: isize) {
        let objects = self.records(op, EE_WON_RTR_OBJECT).objects;
        let status = self.status(op);
        if status == WS_Success && self.routing() {
            self.op0(Op::LockEnter);
            self.op0(Op::DataLock);
            self.rooms.borrow_mut().clear();
            for g in self.routing_groups() {
                if g.flags & 0x800 == 0 {
                    continue;
                }
                let name = self.wcstombs(&g.name, 1024);
                let mut rooms = self.rooms.borrow_mut();
                let room = rooms.entry(g.id).or_default();
                room.id = g.id as u32;
                room.name = name;
                room.max_players = g.member_count as u32;
            }
            for o in &objects {
                // the data's wchar_t units, length / 2 of them, up to the first NUL (std::wstring's c_str)
                let unit = core::mem::size_of::<WChar>();
                let count = o.data.len() / 2;
                let byte = |i: usize| o.data.get(i).copied().unwrap_or(0) as u32;
                let wide: Vec<u32> = (0..count)
                    .map(|k| (0..unit).fold(0u32, |v, j| v | byte(k * unit + j) << (8 * j)))
                    .take_while(|&u| u != 0)
                    .collect();
                let description = self.wcstombs(&wide, 1023);
                self.rooms.borrow_mut().entry(o.link_id).or_default().description = description;
            }
            self.room_list_changed.set(true);
            self.op0(Op::DataUnlock);
            self.op0(Op::LockLeave);
        }
    }

    /// The routing connection closed: the lost connection reported (unless expected), then disconnected.
    fn close_callback(&self, op: isize) {
        if !self.ignore_close.get() {
            self.message_lost();
        }
        self.ignore_close.set(false);
        if self.op1(Op::OpSocketIsRouting, op) != 0 {
            self.disconnect();
        }
    }

    /// The catch-all handler: the op's type name, computed and dropped.
    fn temp_callback(&self, op: isize) {
        let _ = routing_op_type_name(self.op1(Op::OpType, op) as i64);
    }

    /// Connects to a routing server: the connect, close, chat and data-object handlers, a cancel box.
    fn select_server(&self, name: Vec<u32>, address: isize) {
        self.disconnect();
        self.hold(Slot::Routing, self.op0(Op::RoutingNew));
        self.op0(Op::RoutingSetAuth);
        self.op1(Op::RoutingSetConnectCompletion, Call::ConnectCallback as isize);
        self.op1(Op::RoutingSetCloseCompletion, Call::CloseCallback as isize);
        for (kind, call) in [
            (ROUTING_OP_CHAT, Call::OnChat),
            (ROUTING_OP_DATA_CREATED, Call::OnDataObjectCreated),
            (ROUTING_OP_DATA_DELETED, Call::OnDataObjectDeleted),
            (ROUTING_OP_DATA_MODIFIED, Call::OnDataObjectModified),
            (ROUTING_OP_CLIENT_JOINED_GROUP, Call::OnDataObjectChanged),
            (ROUTING_OP_CLIENT_LEFT_GROUP, Call::OnDataObjectChanged),
            (ROUTING_OP_MAX_PLAYERS_CHANGED, Call::OnGroupMaxPlayersChanged),
        ] {
            self.op2(Op::RoutingSetOpCompletion, kind, call as isize);
        }
        self.op1(Op::RoutingSetDefaultOpCompletion, Call::TempCallback as isize);
        self.set_cancelled(false, false);
        *self.server_name.borrow_mut() = name;
        self.op0(Op::RoutingClearProductName);
        self.op1(Op::RoutingConnect, address);
        let t = cz(&self.text(IDS_CONNECTING_TO_SERVER));
        self.op2(Op::ShowCancelConnect, t.as_ptr() as isize, FORM_SELECT_SERVER);
    }

    /// The connection's result: the client registered, or disconnected with the error; when cancelled or stale
    /// only the server selection form closed.
    fn connect_callback(&self, op: isize) {
        let status = self.status(op);
        if self.op1(Op::OpPeerSocketIsRouting, op) != 0 && status != WS_Killed && !self.cancelled(false) {
            if status != WS_Success {
                self.disconnect();
                self.close_form(Some(FORM_SELECT_SERVER));
                self.message(IDS_UNABLE_TO_ACCESS_SERVER_AT_THIS_TIME);
            } else {
                self.ignore_close.set(false);
                let flags = if self.b187.get() { 0 } else { 4 };
                self.op2(Op::ReqRegister, flags, Call::RegisterCallback as isize);
            }
        } else {
            self.close_form(Some(FORM_SELECT_SERVER));
        }
    }

    /// The group list arrived: the room list screen opened and the room and game objects read.
    fn chat_room_list_callback(&self, op: isize) {
        if self.status(op) != WS_Success {
            return;
        }
        self.op0(Op::LockUi);
        if self.op0(Op::WonNetExists) != 0 && !self.cancelled(false) {
            self.op1(Op::PopTo, self.form(FORM_MULTIPLAYER));
            let event = self.op0(Op::NewRoomsOpenEvent);
            self.op2(Op::FormSetOpenEvent, self.form(FORM_AVAILABLE_ROOMS), event);
            self.op1(Op::EventDereference, event);
            self.op1(Op::Push, self.form(FORM_AVAILABLE_ROOMS));
            self.op2(Op::ReqSubscribe, 1, Call::ListRoomCallback as isize);
            self.op2(Op::ReqSubscribe, 0, Call::ListGamesCallback as isize);
        }
        self.op0(Op::UnlockUi);
    }

    /// The created game's info stored and its hosting started; on failure the game left and reported.
    fn join_game_data_callback(&self, op: isize) {
        let status = self.status(op);
        self.op0(Op::LockEnter);
        let _ = routing_op_type_name(self.op1(Op::OpType, op) as i64);
        if status != WS_Success {
            self.clear_games();
            self.rejoin_room(false);
            self.message(IDS_FAILED_TO_CREATE_GAME);
        } else {
            let mut info = GameInfo::default();
            import_game_info(&self.op_data(op), &mut info);
            let link = self.link_id(op);
            self.games.borrow_mut().insert(link, info);
            let copy = self.games.borrow_mut().entry(link).or_default().clone();
            *self.game_info.borrow_mut() = copy;
            self.games_changed.set(true);
            let user = cz(&self.user_name());
            let game = cz(&wide_to_bytes(&self.game_name.borrow()));
            let action = self.op(Op::NewJoinOrHost, 4, user.as_ptr() as isize, game.as_ptr() as isize, 0);
            self.post(action);
        }
        self.op0(Op::LockLeave);
    }

    /// The hosted game's group exists: its data object published (name, address, password, flags); or the
    /// failure or a duplicate name reported.
    fn create_game_callback(&self, op: isize) {
        let status = self.status(op);
        if status == WS_RoutingServG2_GroupAlreadyExists {
            self.message(IDS_GAME_WITH_THAT_NAME_ALREADY_EXISTS);
            return;
        }
        if status != WS_Success {
            self.message(IDS_FAILED_TO_CREATE_GAME);
            return;
        }
        self.room_group_id.set(self.game_list_port.get());
        let room = self.room_name.borrow().clone();
        *self.current_room.borrow_mut() = room;
        self.ignore_close.set(true);
        self.op0(Op::ReqUnsubscribeGames);
        self.leave_groups();
        self.game_group_id.set(self.group_id(op));
        let mut info = GameInfo::default();
        if !self.password.borrow().is_empty() {
            info.password = wide_to_bytes(&self.password.borrow());
        }
        self.op0(Op::ProtocolGet);
        info.name = wide_to_bytes(&self.game_name.borrow());
        info.address = Self::bytes_of(&self.sunk(Op::ProtocolLocalAddress, 0, 0).1);
        info.humans = 0;
        info.players = 8;
        info.host = self.my_client_id() as u32;
        info.flags = if self.is_behind_firewall.get() { 3 } else { 1 };
        info.room = self.room_group_id.get();
        let data = export_game_info(&info);
        let name = wz(&self.game_name.borrow());
        self.op(
            Op::ReqCreateGameObject,
            self.game_group_id.get() as isize,
            name.as_ptr() as isize,
            data.as_ptr() as isize,
            data.len() as isize,
        );
    }

    /// Asks the routing server to create the game's group (8 players, with the password).
    fn create_game(&self) {
        let name = wz(&self.game_name.borrow());
        let password = wz(&self.password.borrow());
        self.op(
            Op::ReqCreateGroup,
            name.as_ptr() as isize,
            password.as_ptr() as isize,
            Call::CreateGameCallback as isize,
            0,
        );
    }

    /// Hosts a game: its name and password stored, the group created (after a warning when firewalled).
    fn host_game(&self, name: Vec<u8>, password: Option<Vec<u8>>) {
        let n = cz(&name);
        self.op1(Op::SetHostedGameName, n.as_ptr() as isize);
        self.op0(Op::SavePreferences);
        self.clear_games();
        *self.game_name.borrow_mut() = bytes_to_wide(&name);
        *self.password.borrow_mut() = password.map_or_else(Vec::new, |p| bytes_to_wide(&p));
        if !self.routing() {
            return;
        }
        if self.is_behind_firewall.get() {
            let t = cz(&self.text(IDS_SIERRA_COM_IS_UNABLE_TO_VERIFY));
            self.op1(Op::ShowMessageHost, t.as_ptr() as isize);
        } else {
            self.create_game();
        }
    }

    /// Rewrites the joined game's data object when this client is its host.
    fn update_game_object(&self) {
        let id = self.game_group_id.get();
        if id == NONE {
            return;
        }
        let host = self.games.borrow().get(&id).map(|g| g.host);
        if let Some(host) = host {
            if host == self.my_client_id() as u32 {
                let name = wz(&self.game_name.borrow());
                self.op2(Op::ReqReplaceGameObject, id as isize, name.as_ptr() as isize);
            }
        }
    }

    /// Joins the named game group with the password, or reports that it was not found.
    fn join_game(&self, name: Vec<u8>, password: Option<Vec<u8>>) {
        if !self.routing() {
            return;
        }
        self.op0(Op::DataLock);
        let groups = self.routing_groups();
        if groups.is_empty() {
            self.op0(Op::DataUnlock);
            return;
        }
        let wanted = cz(&name);
        let found = groups.iter().find(|g| {
            let n = cz(&wide_to_bytes(&g.name));
            // SAFETY: C strings
            unsafe { PlatMbsCmp(n.as_ptr(), wanted.as_ptr()) == 0 && g.flags & 0x800 == 0 }
        });
        match found {
            None => self.message(IDS_COULDNT_FIND_GAME_REQUESTED),
            Some(g) => {
                self.clear_games();
                self.join_group_id.set(g.id);
                let pw = wz(&password.map_or_else(Vec::new, |p| bytes_to_wide(&p)));
                self.op(Op::ReqJoinGroup, g.id as isize, pw.as_ptr() as isize, Call::JoinGroupCallback as isize, 0);
            }
        }
        self.op0(Op::DataUnlock);
    }

    /// A game join's answer: the room left and the game's object read, or a bad password or failure reported.
    fn join_group_callback(&self, op: isize) {
        let group = self.group_id(op);
        if group != self.join_group_id.get() {
            if self.status(op) == WS_Success {
                self.op1(Op::ReqLeaveGroup, group as isize);
            }
            return;
        }
        self.join_group_id.set(NONE);
        let status = self.status(op);
        if status != WS_Success {
            self.message(if status == WS_RoutingServG2_InvalidPassword {
                IDS_PASSWORD_IS_INCORRECT
            } else {
                IDS_COULDNT_JOIN_GAME
            });
        } else {
            self.room_group_id.set(self.game_list_port.get());
            let room = self.room_name.borrow().clone();
            *self.current_room.borrow_mut() = room;
            self.ignore_close.set(true);
            self.leave_groups();
            self.game_group_id.set(group);
            self.op2(Op::ReqReadGameObject, group as isize, Call::JoinGameObjectsCallback as isize);
            self.op0(Op::ReqUnsubscribeGames);
        }
    }

    /// Joins the host of each game object read, or rejoins the room and reports that the game was not found.
    fn join_game_objects_callback(&self, op: isize) {
        let objects = self.records(op, EE_WON_RTR_OBJECT).objects;
        let mut found = false;
        if self.status(op) == WS_Success {
            let mut info = GameInfo::default();
            for o in &objects {
                // compare(0, length, kWonGameObject, strlen): the whole type equals "EE Game Object"
                if o.data_type != b"EE Game Object" {
                    continue;
                }
                import_game_info(&o.data, &mut info);
                let user = cz(&self.user_name());
                let address = cz(&info.address);
                let action = self.op(Op::NewJoinOrHost, 3, user.as_ptr() as isize, address.as_ptr() as isize, 0);
                self.post(action);
                found = true;
            }
        }
        if !found {
            self.clear_games();
            self.rejoin_room(false);
            self.message(IDS_SPECIFIED_GAME_NOT_FOUND);
        }
    }

    /// Rejoins the last room and rereads its games; false (the lost connection reported) when there is none.
    fn rejoin_room(&self, _flag: bool) -> bool {
        if self.routing() && self.room_group_id.get() != NONE {
            let empty = wz(&[]);
            self.op(Op::ReqJoinGroup, self.room_group_id.get() as isize, empty.as_ptr() as isize, NO_CALL, 0);
            self.game_list_port.set(self.room_group_id.get());
            let room = self.current_room.borrow().clone();
            *self.room_name.borrow_mut() = room;
            self.ignore_close.set(false);
            self.op2(Op::ReqSubscribe, 0, Call::ListGamesCallback as isize);
            true
        } else {
            self.message_lost();
            false
        }
    }

    /// Leaves the current groups and joins the named chat room with the password, showing a cancel box.
    fn join_room(&self, room: Vec<u8>, password: Option<Vec<u8>>) {
        if !self.routing() {
            return;
        }
        let wanted = cz(&room);
        for g in self.routing_groups() {
            let n = cz(&wide_to_bytes(&g.name));
            // SAFETY: C strings
            let found = unsafe { PlatMbsCmp(n.as_ptr(), wanted.as_ptr()) == 0 } && g.flags & 0x800 != 0;
            if !found {
                continue;
            }
            let current = self.room_name.borrow().clone();
            *self.current_room.borrow_mut() = current;
            self.leave_groups();
            self.join_room_id.set(g.id);
            *self.room_name.borrow_mut() = g.name.clone();
            let pw = wz(&password.map_or_else(Vec::new, |p| bytes_to_wide(&p)));
            self.op(Op::ReqJoinGroup, g.id as isize, pw.as_ptr() as isize, Call::JoinRoomCallback as isize, 0);
            self.set_cancelled(true, false);
            let t = cz(&self.text(IDS_JOINING_ROOM));
            self.op2(Op::ShowCancelJoinRoom, t.as_ptr() as isize, FORM_AVAILABLE_ROOMS);
            break;
        }
    }

    /// A room join's answer: the game list screen opened, or a bad password or failure reported; stale joins left.
    fn join_room_callback(&self, op: isize) {
        let group = self.group_id(op);
        if group != self.join_room_id.get() {
            if self.status(op) == WS_Success {
                self.op1(Op::ReqLeaveGroup, group as isize);
            }
            return;
        }
        self.join_room_id.set(NONE);
        if self.cancelled(true) {
            return;
        }
        self.close_form(None);
        let status = self.status(op);
        if status != WS_Success && status != WS_RoutingServG2_ClientAlreadyInGroup {
            self.message(if status == WS_RoutingServG2_InvalidPassword {
                IDS_PASSWORD_IS_INCORRECT
            } else {
                IDS_COULDNT_JOIN_ROOM
            });
            self.room_name.borrow_mut().clear();
        } else {
            self.game_list_port.set(group);
            let event = self.op0(Op::NewGamesOpenEvent);
            self.op2(Op::FormSetOpenEvent, self.form(FORM_AVAILABLE_GAMES), event);
            self.op1(Op::EventDereference, event);
            self.op1(Op::SwitchToForm, self.form(FORM_AVAILABLE_GAMES));
        }
    }

    /// Closes the routing connection, leaving the room and the game.
    fn disconnect(&self) {
        if !self.routing() {
            return;
        }
        self.ignore_close.set(true);
        self.op0(Op::RoutingKill);
        self.hold(Slot::Routing, 0);
        self.leave_groups();
        self.clear_games();
        self.server_name.borrow_mut().clear();
    }

    /// Cancels a pending room join and leaves the current room.
    fn leave_groups(&self) {
        if self.routing() {
            if self.join_room_id.get() != NONE {
                self.op1(Op::ReqCancelJoinGroup, self.join_room_id.get() as isize);
            }
            if self.game_list_port.get() != NONE {
                self.op1(Op::ReqLeaveGroup, self.game_list_port.get() as isize);
            }
        }
        self.join_room_id.set(NONE);
        self.game_list_port.set(NONE);
        self.room_name.borrow_mut().clear();
    }

    /// Updates the hosted game object, cancels a pending game join and leaves the current game group.
    fn clear_games(&self) {
        if self.routing() {
            self.update_game_object();
            if self.join_group_id.get() != NONE {
                self.op1(Op::ReqCancelJoinGroup, self.join_group_id.get() as isize);
            }
            if self.game_group_id.get() != NONE {
                self.op1(Op::ReqLeaveGroup, self.game_group_id.get() as isize);
            }
        }
        self.join_group_id.set(NONE);
        self.game_group_id.set(NONE);
    }

    /// Refills the games box with the room's games that pass the in-progress, password and private filters.
    fn update_game_list(&self) {
        if self.op0(Op::CurrentForm) != self.form(FORM_AVAILABLE_GAMES) {
            return;
        }
        let checked = |ctl: isize| {
            self.op1(Op::CheckBoxChecked, self.op2(Op::FormControl, self.form(FORM_AVAILABLE_GAMES), ctl)) != 0
        };
        let show_closed = checked(AG_IN_PROGRESS_TOGGLE);
        let show_passworded = checked(AG_PASSWORD_TOGGLE);
        let show_firewalled = checked(AG_PRIVATE_TOGGLE);
        // `new EEAWonNet(3, text, humans, players, bits & 1, bits & 2, bits & 4)` (a game row)
        let row = |text: &[u8], humans: u32, players: u32, bits: isize| {
            let (humans, players) = (humans as i32 as c_long, players as i32 as c_long);
            self.post_won(EeWonAction::new_game(
                3,
                c_text(text),
                humans,
                players,
                bits & 1 != 0,
                bits & 2 != 0,
                bits & 4 != 0,
            ));
        };
        row(b"@@@@", 0, 0, 0);
        let games: Vec<GameInfo> = self.games.borrow().values().cloned().collect();
        for g in &games {
            if g.room != self.game_list_port.get() {
                continue;
            }
            let passworded = !g.password.is_empty();
            if (show_closed || g.flags & 1 != 0)
                && (show_passworded || !passworded)
                && (show_firewalled || g.flags & 2 == 0)
            {
                let bits =
                    passworded as isize | ((g.flags & 2 != 0) as isize) << 1 | ((g.flags & 1 == 0) as isize) << 2;
                row(&g.name, g.humans, g.players, bits);
            }
        }
        row(b"@@@", 0, 0, 0);
        self.games_changed.set(false);
    }

    /// Starts listing the room's players (the client list requested) unless a listing is running.
    fn refresh_game_list(&self) {
        if self.is_listing_players.get() || !self.routing() {
            return;
        }
        self.is_listing_players.set(true);
        self.clients.borrow_mut().clear();
        self.op1(Op::ReqClientList, Call::ClientListCallback as isize);
    }

    /// Stores the client list and requests the room's members.
    fn client_list_callback(&self, op: isize) {
        if self.status(op) == WS_Success {
            self.clients.borrow_mut().clear();
            let clients = self.records(op, EE_WON_RTR_CLIENT).clients;
            self.clients.borrow_mut().extend(clients);
            self.op2(Op::ReqMembers, self.game_list_port.get() as isize, Call::MembersCallback as isize);
        } else {
            self.is_listing_players.set(false);
        }
    }

    /// Refills the player list box with the names of the room's members.
    fn members_callback(&self, op: isize) {
        let status = self.status(op);
        let members = self.records(op, EE_WON_RTR_MEMBER).members;
        if status != WS_Success {
            self.is_listing_players.set(false);
            return;
        }
        // `new EEAWonNet(4, NULL, text, false)` (a player row)
        let row = |text: &[u8]| self.post_won(EeWonAction::new_list(4, 0, c_text(text), false));
        row(b"@@@@");
        for m in members {
            let name = self.clients.borrow().get(&m).cloned();
            if let Some(name) = name {
                row(&wide_to_bytes(&name));
            }
        }
        row(b"@@@");
        self.is_listing_players.set(false);
    }

    /// (Dead in AoC) the CD key checked, the certificate request started.
    fn login(&self) {
        if self.op0(Op::KeyValid) != 0 {
            self.hold(Slot::LoginOp, self.op0(Op::LoginOpNew));
            self.op1(Op::LoginOpSetCompletion, Call::LoginCallback as isize);
            self.op0(Op::LoginOpNoCreate);
            self.op0(Op::LoginOpRun);
        }
    }

    /// (Dead in AoC) the login's answer.
    fn login_callback(&self, op: isize) {
        if self.op1(Op::LoginOpIs, op) == 0 {
            return;
        }
        if self.status(op) == WS_Success {
            self.op0(Op::PostLoginDone);
        } else {
            self.op0(Op::KeySave);
        }
    }

    /// (Dead in AoC) asks the directory servers for the Titan servers.
    fn get_titan_server_list(&self) {
        self.hold(Slot::DirOp, self.op0(Op::DirOpNew));
        self.op1(Op::DirOpSetCompletion, Call::GetTitanServerListCompletion as isize);
        self.op0(Op::DirOpRun);
    }

    /// (Dead in AoC) the routing servers of the directory's answer kept as the server list.
    fn get_titan_server_list_completion(&self, op: isize) {
        self.op0(Op::ServersClear);
        if self.op1(Op::DirOpIs, op) == 0 || self.status(op) != WS_Success {
            return;
        }
        self.op1(Op::EntitiesLoad, op);
        self.op0(Op::EntityFirst);
        while self.op0(Op::EntityAtEnd) == 0 {
            if self.op0(Op::EntityIsRouting) != 0 {
                self.op0(Op::EntityKeep);
            }
            self.op0(Op::EntityNext);
        }
    }

    /// (Dead in AoC) SierraCom.cfg's directory servers (up to 12 lines), the directory request, a new routing
    /// connection.
    fn initialize(&self) -> bool {
        self.op0(Op::FileSetVolume);
        self.op0(Op::FileSetDirectory);
        self.op0(Op::FileSetName);
        self.directory_servers.borrow_mut().clear();
        if self.op0(Op::FileOpen) != 0 {
            let mut count = 0;
            let mut line = [0u8; 256];
            // fgets first, then the count (the thirteenth line is read and dropped)
            while self.op1(Op::FileGets, line.as_mut_ptr() as isize) != 0 && count < 12 {
                let mut text = until_nul(&line).to_vec();
                // theLine[strlen - 1] == '\n' (an empty line reads the byte before the buffer: never, fgets
                // returns at least one byte)
                if text.last() == Some(&b'\n') {
                    text.pop();
                }
                self.directory_servers.borrow_mut().push(text);
                count += 1;
            }
            self.op0(Op::FileClose);
        }
        self.hold(Slot::DirServers, self.op0(Op::DirServersNew));
        let servers = self.directory_servers.borrow().clone();
        for s in &servers {
            let z = cz(s);
            self.op1(Op::DirServersAdd, z.as_ptr() as isize);
        }
        self.is_behind_firewall.set(false);
        self.get_titan_server_list();
        self.hold(Slot::Routing, self.op0(Op::RoutingNew));
        self.join_group_id.set(NONE);
        self.game_group_id.set(NONE);
        self.b187.set(false);
        true
    }

    /// Copies the joined game's info, when there is one.
    fn get_game_info(&self, info: isize) {
        if self.game_group_id.get() == NONE {
            return;
        }
        let g = self.game_info.borrow().clone();
        for (op, text) in
            [(Op::InfoSetName, &g.name), (Op::InfoSetAddress, &g.address), (Op::InfoSetPassword, &g.password)]
        {
            let t = cz(text);
            self.op2(op, info, t.as_ptr() as isize);
        }
        self.op2(Op::InfoSetHumans, info, g.humans as isize);
        self.op2(Op::InfoSetPlayers, info, g.players as isize);
        self.op2(Op::InfoSetHost, info, g.host as isize);
        self.op2(Op::InfoSetFlags, info, g.flags as isize);
        self.op2(Op::InfoSetRoom, info, g.room as isize);
    }

    /// Publishes the joined game's info to its data object and keeps a copy.
    fn set_game_info(&self, info: isize) {
        if self.game_group_id.get() == NONE {
            return;
        }
        // SAFETY: the host hands the C++ info's texts
        let text = |op: Op| unsafe { c_bytes(self.op1(op, info) as *const c_char) };
        let g = GameInfo {
            name: text(Op::InfoName),
            address: text(Op::InfoAddress),
            password: text(Op::InfoPassword),
            humans: self.op1(Op::InfoHumans, info) as u32,
            players: self.op1(Op::InfoPlayers, info) as u32,
            host: self.op1(Op::InfoHost, info) as u32,
            flags: self.op1(Op::InfoFlags, info) as u32,
            room: self.op1(Op::InfoRoom, info) as u16,
        };
        let data = export_game_info(&g);
        let name = wz(&self.game_name.borrow());
        self.op(
            Op::ReqModifyGameObject,
            self.game_group_id.get() as isize,
            name.as_ptr() as isize,
            data.as_ptr() as isize,
            data.len() as isize,
        );
        *self.game_info.borrow_mut() = g;
    }

    /// Starts a WON account update with the new name and password and the e-mail flag.
    fn update_account(&self, name: Vec<u8>, password: Vec<u8>, flag: bool) {
        let n = wz(&bytes_to_wide(&name));
        let p = cz(&password);
        self.op2(Op::UpdateOpNew, n.as_ptr() as isize, p.as_ptr() as isize);
        self.op1(Op::UpdateOpSetCompletion, Call::UpdateAccountCallback as isize);
        self.op0(Op::UpdateOpClearNewsletters);
        self.op1(Op::UpdateOpNewsletter, 1 | if flag { 0x10000 } else { 0 });
        self.op0(Op::UpdateOpRun);
        self.op0(Op::UpdateOpRelease);
    }

    /// The account update's result in a message box (nothing when cancelled).
    fn update_account_callback(&self, op: isize) {
        let status = self.status(op);
        self.close_form(None);
        if status == WS_Killed {
            return;
        }
        self.message(if status == WS_Success {
            IDS_SUCCESSFULLY_MODIFIED_YOUR_ACCOUNT
        } else {
            IDS_COULD_NOT_MODIFY_YOUR_ACCOUNT
        });
    }

    /// Requests the current account info and shows a cancel box.
    fn open_update_account(&self) {
        self.hold(Slot::AccountOp, self.op0(Op::AccountOpNew));
        self.op1(Op::AccountOpSetCompletion, Call::GetAccountCallback as isize);
        self.op0(Op::AccountOpRun);
        // after RunAsync, as the original (the setter waits for the Rust op's lock)
        self.op0(Op::AccountOpNewsletter);
        let t = cz(&self.text(IDS_GETTING_CURRENT_USER_INFO));
        self.op2(Op::ShowCancelAccount, t.as_ptr() as isize, FORM_UPDATE_SIERRA_ACCOUNT);
    }

    /// The account form's e-mail box and checkbox from the account info, or the error.
    fn get_account_callback(&self, op: isize) {
        let status = self.status(op);
        if status == WS_Killed {
            return;
        }
        self.close_form(None);
        if status == WS_Success {
            let checked = self.op1(Op::AccountNewsletter, op) != 0;
            let email = cz(&Self::bytes_of(&self.sunk(Op::AccountEmail, op, 1).1));
            let form = self.form(FORM_UPDATE_SIERRA_ACCOUNT);
            self.op2(
                Op::CheckBoxSetChecked,
                self.op2(Op::FormControl, form, UA_SEND_ME_EMAIL_CHECKBOX),
                checked as isize,
            );
            let form = self.form(FORM_UPDATE_SIERRA_ACCOUNT);
            self.op2(
                Op::EditBoxSetText,
                self.op2(Op::FormControl, form, UA_ENTER_EMAIL_EDIT_BOX),
                email.as_ptr() as isize,
            );
        } else {
            let t = cz(&self.text(IDS_THERE_WAS_ERROR_GETTING_CURRENT_USER));
            self.op2(Op::ShowCancelAccount, t.as_ptr() as isize, FORM_UPDATE_SIERRA_ACCOUNT);
        }
    }

    /// An incoming chat line shown in the games screen or the lobby; a whisper's sender kept for /r.
    fn on_chat(&self, op: isize) {
        let sender = self.client_id(op);
        let client = self.smart_get(EeWonSmartGet::Client, sender);
        let Some((client, name)) = client else { return };
        if self.op1(Op::OpRecipientsEmpty, op) != 0 {
            return;
        }
        let from = wide_to_bytes(&name.unwrap_or_default());
        if self.num(op, EE_WON_RTF_FLAGS16) & 4 != 0 && client.id != self.my_client_id() {
            self.reply_client_id.set(sender);
        }
        let mut sink = WonNetSink::default();
        self.op(Op::OpText, op, EE_WON_RTT_TEXT as isize, &mut sink as *mut WonNetSink as isize, 0);
        let text = wide_to_bytes(&sink.units);
        let current = self.op0(Op::CurrentForm);
        let mine = client.id == self.my_client_id();
        // `new EEAWonNet(6 or 7, the label, from, text, mine)` (a chat line)
        let action = if current == self.form(FORM_AVAILABLE_GAMES) {
            let label = self.op2(Op::FormControl, self.op0(Op::CurrentForm), CHAT_LABEL);
            EeWonAction::new_chat(6, label, c_text(&from), c_text(&text), mine)
        } else if current == self.form(FORM_LOBBY) {
            EeWonAction::new_chat(7, 0, c_text(&from), c_text(&text), mine)
        } else {
            return;
        };
        self.post_won(action);
    }

    /// Does nothing (enters and leaves a lock).
    fn on_data_object_changed(&self) {
        self.op0(Op::Lock8Enter);
        self.op0(Op::Lock8Leave);
    }

    /// A game object created or modified: into the game list (and the joined game's info), the list changed.
    fn on_data_object_stored(&self, op: isize) {
        self.op0(Op::LockEnter);
        let data = self.op_data(op);
        let link = self.link_id(op);
        {
            let mut games = self.games.borrow_mut();
            import_game_info(&data, games.entry(link).or_default());
        }
        if self.game_group_id.get() == link {
            let copy = self.games.borrow_mut().entry(link).or_default().clone();
            *self.game_info.borrow_mut() = copy;
        }
        self.games_changed.set(true);
        self.op0(Op::LockLeave);
    }

    /// A deleted game removed from the list; left (and the room rejoined) when it was the joined game.
    fn on_data_object_deleted(&self, op: isize) {
        self.op0(Op::LockEnter);
        let link = self.link_id(op);
        // the original's `mGames[id]` statements made the entry
        self.games.borrow_mut().entry(link).or_default();
        if self.game_group_id.get() == link {
            self.clear_games();
            self.rejoin_room(false);
        }
        self.games.borrow_mut().remove(&link);
        self.games_changed.set(true);
        self.op0(Op::LockLeave);
    }

    /// A room's maximum players updated; the room list flagged when it is the current room.
    fn on_group_max_players_changed(&self, op: isize) {
        self.op0(Op::LockEnter);
        let group = self.group_id(op);
        let max = self.num(op, EE_WON_RTF_MAX_PLAYERS) as u16;
        if let Some(room) = self.rooms.borrow_mut().get_mut(&group) {
            room.max_players = max as u32;
        }
        if self.game_list_port.get() == group {
            self.b18e.set(true);
        }
        self.op0(Op::LockLeave);
    }

    /// The client registration: the group list requested, or the failure (name in use) reported and disconnected.
    fn register_callback(&self, op: isize) {
        let status = self.status(op);
        if status == WS_Killed {
            return;
        }
        if status != WS_Success {
            self.message(if status != WS_RoutingServG2_ClientAlreadyExists {
                IDS_UNABLE_TO_ACCESS_SERVER_AT_THIS_TIME
            } else {
                IDS_SIERRA_COM_USER_IS_ALREADY
            });
            self.disconnect();
        } else {
            self.op1(Op::ClientIdSet, self.num(op, EE_WON_RTF_RECONNECT_ID) as u32 as isize);
            self.op1(Op::ReqGroupList, Call::ChatRoomListCallback as isize);
        }
    }

    /// The name of a chat command's target: quoted (from character 6 to the closing quote) or a word (from 5 to
    /// the space); the character after it.
    fn command_name(text: &Uw) -> (Uw, i32) {
        let mut name = Uw::empty();
        let (start, stop) = if text.at(5) == b'"' { (6, b'"') } else { (5, b' ') };
        let mut end = start;
        while end < text.len() && text.at(end) != stop {
            end += 1;
        }
        // SAFETY: a live record's C string
        let p = unsafe { PlatMbsNInc(text.ptr(), start as usize) };
        name.copy_from(p, end - start);
        (name, end)
    }

    /// Sends a chat line to the room; /i and /u ignore or unignore a user, /t whispers, /r replies to the last
    /// whisper.
    fn send_chat(&self, text: Vec<u8>) {
        let t = Uw::new(&text);
        let send = |message: &[u8], call: isize| {
            let w = wz(&bytes_to_wide(message));
            self.op(Op::ReqSendChat, w.as_ptr() as isize, self.game_list_port.get() as isize, call, 0);
        };
        let whisper = |message: Vec<u8>, to: u16| {
            let w = wz(&bytes_to_wide(&message));
            let recipients = [self.my_client_id(), to];
            self.op(Op::ReqSendChatTo, w.as_ptr() as isize, recipients.as_ptr() as isize, 2, 0);
        };
        if !(t.len() >= 4 && t.at(2) == b'/') {
            let _ = self.smart_find(&[]);
            send(&t.bytes(), Call::SendChatCallback as isize);
            return;
        }
        match t.at(3) {
            b'I' | b'i' | b'U' | b'u' => {
                if t.len() < 6 {
                    return;
                }
                let (name, _) = Self::command_name(&t);
                let mut message = Vec::new();
                if let Some(client) = self.smart_find(&bytes_to_wide(&name.bytes())) {
                    let ignore = t.at(3) == b'i' || t.at(3) == b'I';
                    self.op2(Op::RoutingIgnoreClient, client.id as isize, ignore as isize);
                    message = self.text(if ignore { IDS_IS_NOW_IGNORING } else { IDS_IS_NO_LONGER_IGNORING });
                }
                message.extend_from_slice(&name.bytes());
                let label = self.op2(Op::FormControl, self.op0(Op::CurrentForm), CHAT_LABEL);
                self.post_won(EeWonAction::new_chat(6, label, c_text(&self.user_name()), c_text(&message), true));
            }
            b'T' | b't' => {
                if t.len() < 8 {
                    return;
                }
                let (name, end) = Self::command_name(&t);
                let end = if t.at(5) == b'"' { end + 2 } else { end + 1 };
                // SAFETY: a live record's C string (the original's _mbsninc past the end stops at its terminator)
                let rest = unsafe { c_bytes(PlatMbsNInc(t.ptr(), end as usize) as *const c_char) };
                if let Some(client) = self.smart_find(&bytes_to_wide(&name.bytes())) {
                    let mut message = self.text(IDS_WHISPERED);
                    message.extend_from_slice(&rest);
                    whisper(message, client.id);
                }
            }
            b'R' | b'r' => {
                // Fix (kept): "no reply target yet" is 0xffff (the constructor's value), not 0.
                if t.len() >= 6 && self.reply_client_id.get() != NONE {
                    // SAFETY: as above
                    let rest = unsafe { c_bytes(PlatMbsNInc(t.ptr(), 5) as *const c_char) };
                    let mut message = self.text(IDS_WHISPERED);
                    message.extend_from_slice(&rest);
                    whisper(message, self.reply_client_id.get());
                }
            }
            b'W' | b'w' => {}
            _ => send(&t.bytes(), NO_CALL),
        }
    }

    /// Refills the rooms box (games and players per room), keeps the selection and shows the server and totals.
    fn update_room_list(&self, rooms_box: isize, label16: isize, label14: isize) {
        let server = cz(&wide_to_bytes(&self.server_name.borrow()));
        let mut game_counts: BTreeMap<u16, u32> = BTreeMap::new();
        let mut player_counts: BTreeMap<u16, u32> = BTreeMap::new();
        let mut games = 0u32;
        let mut players = 0u32;
        self.op0(Op::LockEnter);
        let mut sel_out: core::ffi::c_ulong = 0;
        let no_selection =
            self.op2(Op::RoomsBoxSelection, rooms_box, &mut sel_out as *mut core::ffi::c_ulong as isize) as i32;
        let selection = sel_out as u32;
        for g in self.games.borrow().values() {
            if g.flags & 1 != 0 {
                let c = game_counts.entry(g.room).or_insert(0);
                *c = c.wrapping_add(1);
                let p = player_counts.entry(g.room).or_insert(0);
                *p = p.wrapping_add(g.humans);
            } else {
                games = games.wrapping_add(1);
                players = players.wrapping_add(g.humans);
            }
        }
        self.op1(Op::RoomsBoxClear, rooms_box);
        let rooms: Vec<RoomInfo> = self.rooms.borrow().values().cloned().collect();
        for r in &rooms {
            let d = cz(&r.description);
            let n = cz(&r.name);
            self.op1(Op::RoomLineDescription, d.as_ptr() as isize);
            self.op1(Op::RoomLineName, n.as_ptr() as isize);
            let key = r.id as u16;
            let mut players_line = r.max_players;
            let games_line = game_counts.get(&key).copied().unwrap_or(0);
            if let Some(&p) = player_counts.get(&key) {
                players_line = players_line.wrapping_add(p);
            }
            self.op1(Op::RoomLinePlayers, players_line as isize);
            self.op1(Op::RoomLineGames, games_line as isize);
            self.op1(Op::RoomsBoxAppend, rooms_box);
        }
        if no_selection == 0 && (selection as isize) < self.op1(Op::RoomsBoxLineCount, rooms_box) {
            self.op2(Op::RoomsBoxSelect, rooms_box, selection as isize);
        }
        let format = self.text(IDS_SETUP_PLAYERS);
        // sprintf(theText, "%ld  (%ld %s)", ...): the counts as the 32-bit longs they were
        let mut label = format!("{}  ({} ", games as i32, players as i32).into_bytes();
        label.extend_from_slice(&format);
        label.push(b')');
        let l = cz(&label);
        self.op2(Op::LabelSetText, label14, l.as_ptr() as isize);
        self.op2(Op::LabelSetText, label16, server.as_ptr() as isize);
        self.room_list_changed.set(false);
        self.op0(Op::LockLeave);
    }

    /// A text dump of the data (the golden compares it with the deleted C++'s members).
    pub fn dump(&self) -> String {
        let w = |v: &Vec<u32>| String::from_utf8_lossy(&wide_to_bytes(v)).into_owned();
        let b = |v: &Vec<u8>| String::from_utf8_lossy(v).into_owned();
        let info = |g: &GameInfo| {
            format!(
                "{}|{}|{}|{} {} {} {} {}",
                b(&g.name),
                b(&g.address),
                b(&g.password),
                g.humans,
                g.players,
                g.host,
                g.flags,
                g.room
            )
        };
        let mut s = format!(
            "pw={} game={} room={} cur={} server={} ids {} {} {} {} {} {} flags {}{}{}{}{}{}{} l {} {}\ninfo {}\n",
            w(&self.password.borrow()),
            w(&self.game_name.borrow()),
            w(&self.room_name.borrow()),
            w(&self.current_room.borrow()),
            w(&self.server_name.borrow()),
            self.join_group_id.get(),
            self.game_group_id.get(),
            self.reply_client_id.get(),
            self.join_room_id.get(),
            self.game_list_port.get(),
            self.room_group_id.get(),
            self.is_behind_firewall.get() as u8,
            self.b187.get() as u8,
            self.ignore_close.get() as u8,
            self.is_listing_players.get() as u8,
            self.room_list_changed.get() as u8,
            self.games_changed.get() as u8,
            self.b18e.get() as u8,
            self.l168.get(),
            self.l16c.get(),
            info(&self.game_info.borrow()),
        );
        for (k, g) in self.games.borrow().iter() {
            s += &format!("game {} {}\n", k, info(g));
        }
        for (k, r) in self.rooms.borrow().iter() {
            s += &format!("room {} {}|{}|{} {}\n", k, b(&r.name), b(&r.description), r.max_players, r.id);
        }
        for (k, c) in self.clients.borrow().iter() {
            s += &format!("client {} {}\n", k, w(c));
        }
        for d in self.directory_servers.borrow().iter() {
            s += &format!("dir {}\n", b(d));
        }
        s
    }

    /// One call (the forwards' and the completions').
    ///
    /// # Safety
    /// The call's pointer arguments as documented on [`EeWonNetCall`].
    pub unsafe fn call(&self, call: EeWonNetCall, a: isize, b: isize, c: isize) -> isize {
        let text = |p: isize| c_bytes(p as *const c_char);
        let opt = |p: isize| (p != 0).then(|| c_bytes(p as *const c_char));
        match call {
            Call::Construct => self.construct(),
            Call::ConnectToLobby => self.connect_to_lobby(),
            Call::FnCFFDD => {
                if self.lobby() {
                    self.op1(Op::LobbyPump, a);
                }
            }
            Call::FnCFFF2 => {
                // Fix (kept): test the window that is called (this used to test mLobby).
                if self.lobby_window() {
                    self.op0(Op::LobbyWindowVirtual18);
                }
            }
            Call::SelectServer => {
                let units: Vec<u32> = (0..b as usize).map(|i| *(a as *const WChar).add(i) as u32).collect();
                self.select_server(units, c);
            }
            Call::HostGame => self.host_game(text(a), opt(b)),
            Call::JoinGame => self.join_game(text(a), opt(b)),
            Call::JoinRoom => self.join_room(text(a), opt(b)),
            Call::LeaveRoom => self.leave_groups(),
            Call::GetGameInfo => self.get_game_info(a),
            Call::SetGameInfo => self.set_game_info(a),
            Call::ClearGameList => self.clear_games(),
            Call::RejoinRoom => return self.rejoin_room(a != 0) as isize,
            Call::RefreshGameList => self.refresh_game_list(),
            Call::UpdateGameList => self.update_game_list(),
            Call::UpdateAccount => self.update_account(text(a), text(b), c != 0),
            Call::OpenUpdateAccount => self.open_update_account(),
            Call::SendChat => self.send_chat(text(a)),
            Call::UpdateRoomList => self.update_room_list(a, b, c),
            Call::CreateGame => self.create_game(),
            Call::UpdateGameObject => self.update_game_object(),
            Call::Disconnect => self.disconnect(),
            Call::LeaveGroups => self.leave_groups(),
            Call::ClearGames => self.clear_games(),
            Call::Initialize => return self.initialize() as isize,
            Call::GetTitanServerList => self.get_titan_server_list(),
            Call::Login => self.login(),
            Call::GameListPort => return self.game_list_port.get() as isize,
            Call::RoomListChanged => return self.room_list_changed.get() as isize,
            Call::IsBehindFirewall => return self.is_behind_firewall.get() as isize,
            Call::RoomName => {
                let w = wz(&self.room_name.borrow());
                self.op(Op::WideOut, a, w.as_ptr() as isize, (w.len() - 1) as isize, 0);
            }
            Call::ListGamesCallback => self.list_games_callback(a),
            Call::ListRoomCallback => self.list_room_callback(a),
            Call::CloseCallback => self.close_callback(a),
            Call::TempCallback => self.temp_callback(a),
            Call::ConnectCallback => self.connect_callback(a),
            Call::ChatRoomListCallback => self.chat_room_list_callback(a),
            Call::JoinGameDataCallback => self.join_game_data_callback(a),
            Call::CreateGameCallback => self.create_game_callback(a),
            Call::JoinGroupCallback => self.join_group_callback(a),
            Call::JoinGameObjectsCallback => self.join_game_objects_callback(a),
            Call::JoinRoomCallback => self.join_room_callback(a),
            Call::ClientListCallback => self.client_list_callback(a),
            Call::MembersCallback => self.members_callback(a),
            Call::UpdateAccountCallback => self.update_account_callback(a),
            Call::GetAccountCallback => self.get_account_callback(a),
            Call::OnChat => self.on_chat(a),
            Call::OnDataObjectChanged => self.on_data_object_changed(),
            Call::OnDataObjectCreated | Call::OnDataObjectModified => self.on_data_object_stored(a),
            Call::OnDataObjectDeleted => self.on_data_object_deleted(a),
            Call::OnGroupMaxPlayersChanged => self.on_group_max_players_changed(a),
            Call::RegisterCallback => self.register_callback(a),
            Call::SendChatCallback => {}
            Call::LoginCallback => self.login_callback(a),
            Call::GetTitanServerListCompletion => self.get_titan_server_list_completion(a),
        }
        0
    }
}

fn blank_record() -> EeWonSmartRecord {
    EeWonSmartRecord {
        id: 0,
        captain: 0,
        max_clients: 0,
        observers: 0,
        member_count: 0,
        flags: 0,
        async_flags: 0,
        cdkey: 0,
        client_flags: 0,
        member_flags: 0,
        has_client: false,
        name: core::ptr::null(),
        name_len: 0,
    }
}

/// SmartName: a record's name units.
fn record_name(rec: &EeWonSmartRecord) -> Vec<u32> {
    if rec.name.is_null() {
        return Vec::new();
    }
    // SAFETY: the model's copy, valid until its next call
    unsafe { std::slice::from_raw_parts(rec.name, rec.name_len).to_vec() }
}

use EeWonNetCall as Call;
use EeWonNetOp as Op;

/// A new front end for the C++ object `cpp` (its constructor; the body runs as the `Construct` call).
#[no_mangle]
pub extern "C" fn ee_won_wonnet_new(cpp: *mut c_void) -> *mut EeWonNet {
    Box::into_raw(Box::new(EeWonNet::new(cpp)))
}

/// The destructor's body, then the object freed.
///
/// # Safety
/// `obj` from `ee_won_wonnet_new`, not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wonnet_free(obj: *mut EeWonNet) {
    if obj.is_null() {
        return;
    }
    (*obj).destruct();
    (*obj).release_objects();
    drop(Box::from_raw(obj));
}

/// The object's C++ objects (the host's and the cancel boxes' view of the slots; valid while the object lives).
///
/// # Safety
/// `obj` from `ee_won_wonnet_new` (or NULL: NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_wonnet_objects(obj: *const EeWonNet) -> *mut EeWonNetObjects {
    if obj.is_null() {
        return core::ptr::null_mut();
    }
    (*obj).objects.get()
}

/// One call on the object.
///
/// # Safety
/// `obj` from `ee_won_wonnet_new`; the arguments as [`EeWonNetCall`] documents.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wonnet_call(
    obj: *const EeWonNet,
    call: EeWonNetCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    (*obj).call(call, a, b, c)
}

/// A completion's Complete: nothing when `gWonNet` (`obj`) is gone; a routing completion only for an op of the
/// front end's own routing connection; then the call with the op.
///
/// # Safety
/// `obj` NULL or from `ee_won_wonnet_new`; `op` the finished `AsyncOp*`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wonnet_complete(obj: *const EeWonNet, call: EeWonNetCall, routing: bool, op: isize) {
    if obj.is_null() {
        return;
    }
    let o = &*obj;
    if routing && o.op1(Op::OpOnRouting, op) == 0 {
        return;
    }
    o.call(call, op, 0, 0);
}

/// The object's data as text (won_wonnet_test's comparison), valid until the next call on this thread.
///
/// # Safety
/// `obj` from `ee_won_wonnet_new`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wonnet_dump_text(obj: *const EeWonNet) -> *const c_char {
    thread_local! {
        static TEXT: RefCell<std::ffi::CString> = RefCell::new(std::ffi::CString::default());
    }
    let text = std::ffi::CString::new((*obj).dump().replace('\0', "")).unwrap_or_default();
    TEXT.with(|t| {
        *t.borrow_mut() = text;
        t.borrow().as_ptr()
    })
}

#[cfg(test)]
#[path = "wonnet_tests.rs"]
mod tests;
