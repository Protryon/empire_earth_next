//! The lobby screens' four controls as one Rust object (R6 WONLobby round 4): GameBrowserCtrl (the help and news
//! browser tabs: navigation, find, status line), GameStagingCtrl (the game room's leave/dissolve and start/ready
//! buttons), LobbyOptionsCtrl (the LAN and Internet options tabs) and GameUserListCtrl (the game room's player list
//! with its Kick/Ban button). Each was a `Container` whose members were its controls (`SmartPtr`s looked up by name in
//! the resource config) and a few flags; its code was the look-ups, the event dispatch and the rules between the
//! lobby's state and the controls.
//!
//! The C++ keeps one shell, `LobbyRustCtrl : Container` (InitComponent and HandleComponentEvent forward here; the
//! event's type is mapped to `EeWonLobbyCtlEvent` in C++), and the four class names as subclasses, so the resource
//! factories, RootScreen (`ResetBrowser`) and the browser's config reader (`SetHomeLocation`) did not change.
//! GameBrowserCtrl keeps its home location (a `GUIString`) and its `Paint` override in C++; GameUserListItem (the
//! player row: paint, size, order) stays a C++ list row, its icon rule (`UpdateIcon`) is here. Every call on a
//! control, an event, the lobby's model, its settings or a global is one op of the host given to
//! `ee_won_lobbyctl_new` (`LobbyCtrlHost` in `WONLobby/LobbyCtrlHost.cpp`), the original's statements with no
//! decision between them. A check box set with its event (`SetCheck(x, true)`) or a list or combo cleared with its
//! event delivers that event back into this object, so the state is in `Cell`s behind a shared reference and nothing
//! is borrowed across an op.

use std::cell::Cell;
use std::ffi::{c_char, c_void};

/// The control kinds (the original classes; LobbyOptionsCtrl's LAN and Internet tabs are two kinds).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyCtlKind {
    Browser = 0,
    Staging = 1,
    /// `LobbyOptionsCtrl(true)`
    LanOptions = 2,
    /// `LobbyOptionsCtrl(false)`
    NetOptions = 3,
    UserList = 4,
    /// EELobbyGameOptionsCtrl (the EE game room's options panel)
    GameOptions = 5,
    /// EELobbyPlayerOptionsCtrl (the EE game room's eight player rows; round 5)
    PlayerOptions = 6,
    /// ServerListCtrl (the game lists; round 13, srvlistctl.rs)
    ServerList = 7,
    /// ChatCtrl (a chat panel; round 13, chatctl.rs)
    Chat = 8,
    /// RoomCtrl (the room tree; round 13, netctl.rs from here)
    Room = 9,
    /// FriendsListCtrl
    Friends = 10,
    /// UserListCtrl (the room's users)
    Users = 11,
    /// QuickPlayCtrl (a `Dialog`)
    QuickPlay = 12,
    /// LoginCtrl (a `Dialog`)
    Login = 13,
    CreateAccount = 14,
    UpdateAccount = 15,
    LostPassword = 16,
    LostUserName = 17,
    LoginHelp = 18,
    LoginStatus = 19,
    InitFailed = 20,
    /// MOTDCtrl
    Motd = 21,
    LoginScreen = 22,
    DirectConnect = 23,
    /// RoomLabelCtrl
    RoomLabel = 24,
}

/// A control's type: the `SmartPtr<T>` the original's `SafeGetComponent` filled (its `dynamic_cast`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyCtlType {
    Button = 0,
    Label = 1,
    Combo = 2,
    Input = 3,
    Browser = 4,
    MultiList = 5,
    /// `ImageComponent`
    Image = 6,
    /// `Container`
    Container = 7,
    /// `Component`
    Component = 8,
    /// `EELobbyColorButton` (round 5)
    ColorButton = 9,
    /// `TextArea` (round 13)
    TextArea = 10,
    /// `TreeArea`
    TreeArea = 11,
    /// `ListArea`
    ListArea = 12,
    /// `ScreenContainer`
    ScreenContainer = 13,
    /// `ServerListCtrl`
    ServerListCtrl = 14,
}

/// One control look-up: the resource name, the original's file and line (for the missing-control report), the type,
/// the control id `SafeGetComponent` set (-1: none) and the member it fills (-1: LobbyOptionsCtrl's local button).
#[repr(C)]
pub struct EeWonLobbyCtlSlot {
    pub name: *const c_char,
    pub file: *const c_char,
    pub line: i32,
    pub ty: EeWonLobbyCtlType,
    pub control_id: i32,
    pub member: i32,
}

// SAFETY: the table's pointers are static C string literals.
unsafe impl Sync for EeWonLobbyCtlSlot {}

/// The event an inward `Event` carries (the C++ shell maps the `ComponentEvent`'s type).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyCtlEvent {
    Other = 0,
    ButtonPressed,
    ComboSelChanged,
    ListSelChanged,
    /// `ComponentEvent_ListItemClicked` (a `ListItemClickedEvent`)
    ListItemClicked,
    BrowserButtonChange,
    BrowserStatusChange,
    /// 100029: the game room was joined
    JoinedGame,
    /// 100032: the game room was left
    LeftGame,
    /// `LobbyEvent_SetRoomSpecFlags` (a `SetRoomSpecFlagsEvent`)
    SetRoomSpecFlags,
    /// 100061: a ready state changed
    ReadyChanged,
    /// `LobbyEvent_ClientChanged` (a `ClientChangedEvent`)
    ClientChanged,
    /// `LobbyEvent_PlayerChanged` (a `PlayerChangedEvent`)
    PlayerChanged,
    /// 100014: the options to show again
    SyncOptions,
    /// 100050: the LAN options changed
    LanOptions,
    /// 100055: the public address changed
    PublicAddress,
    /// 100065: every game option to show again (EELobbyGameOptionsCtrl)
    AllOptions,
    /// `LobbyEvent_EELobbySyncOption` (an `EELobbySyncOptionEvent`)
    SyncOption,
    /// `LobbyEvent_EELobbySyncPlayer` (an `EELobbySyncPlayerEvent`; round 5)
    SyncPlayer,
    /// 100064: a colour button's colour changed (round 5)
    ColorChanged,
    /// `ComponentEvent_InputTextChanged` (round 13)
    InputTextChanged,
    /// `ComponentEvent_InputReturn`
    InputReturn,
    /// `LobbyEvent_GameChanged` (a `GameChangedEvent`)
    GameChanged,
    /// `LobbyEvent_SyncGameList` (a `SyncGameListEvent`)
    SyncGameList,
    /// 100034: the filter values changed
    FiltersChanged,
    /// 100059: every game to filter again
    RefilterAll,
    /// 100058: the shown games to filter again
    RefilterShown,
    /// `ComponentEvent_TextLinkActivated` (a `TextLinkActivatedEvent`)
    TextLinkActivated,
    /// `LobbyEvent_Alert`
    Alert,
    /// `LobbyEvent_AnnounceFriend`
    AnnounceFriend,
    /// `LobbyEvent_AnnounceClientLocation`
    AnnounceClientLocation,
    /// `LobbyEvent_ClientModerated`
    ClientModerated,
    /// `LobbyEvent_ChatCommandError`
    ChatCommandError,
    /// `LobbyEvent_GroupInvitation`
    GroupInvitation,
    /// `LobbyEvent_GroupJoinAttempt`
    GroupJoinAttempt,
    /// `LobbyEvent_JoinedRoom`
    JoinedRoom,
    /// `LobbyEvent_LeftRoom`
    LeftRoom,
    /// `LobbyEvent_PlayerTeamChanged`
    PlayerTeamChanged,
    /// `LobbyEvent_RecvChat`
    RecvChat,
    /// `LobbyEvent_SetChatInput`
    SetChatInput,
    /// `ComponentEvent_TreeItemOpened` (a `TreeItemOpenedEvent`)
    TreeItemOpened,
    /// `LobbyEvent_GroupChanged`
    GroupChanged,
    /// `LobbyEvent_ServerChanged`
    ServerChanged,
    /// 100053: the room list to fill again
    SetRoomList,
    /// `LobbyEvent_FriendChanged`
    FriendChanged,
    /// 100056: the friends to show again
    RefreshFriends,
    /// `LobbyEvent_LoginInitStatus`
    LoginInitStatus,
    /// `LobbyEvent_SetLoginFields`
    SetLoginFields,
    /// 100052: the saved user names to list again
    FillUserNames,
    /// `LobbyEvent_QueryAccountResult`
    QueryAccountResult,
    /// `LobbyEvent_GotHTTPDocument`
    GotHttpDocument,
}

/// An event's member (`EvGet`; round 13), by its class: AnnounceClientLocation, AnnounceFriend,
/// TextLinkActivated (the link as a `ChatLink`, NULL another), PlayerTeamChanged, RecvChat, JoinedRoom, LeftRoom,
/// ChatCommandError, Alert, SetChatInput, GroupInvitation, GroupJoinAttempt, ClientModerated. A text is its
/// `GUIStringData*`, a flag 0 or 1.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyCtlEvField {
    AclHandled = 0,
    AclRoomId,
    AclClient,
    AclRoom,
    AclServer,
    AfLocation,
    AfFriend,
    AfHandled,
    TlaLink,
    PtcClient,
    RcSender,
    RcTarget,
    RcRoomSpec,
    RcText,
    RcIsTeam,
    RcIsWhisper,
    RcIsWarning,
    RcIsEmote,
    JrRoomSpec,
    JrClientId,
    JrGroupName,
    JrServerName,
    JrClients,
    LrRoomSpec,
    LrFlag,
    LrForced,
    CceText,
    AlKind,
    AlValue,
    AlSeverity,
    AlText,
    SciHandled,
    SciRoomSpec,
    SciCommand,
    SciText,
    GiKind,
    GiClient,
    GiGameName,
    GiGameId,
    GiComment,
    GiFlag,
    GiInvite,
    GjaKind,
    GjaClient,
    GjaComment,
    CmRoomSpec,
    CmClient,
    CmFlag,
    CmKind,
    CmDuration,
    CmRoomName,
    CmComment,
    /// GroupChangedEvent
    GcGroup,
    GcServer,
    /// ServerChangedEvent
    ScServer,
    /// FriendChangedEvent
    FcFriend,
    /// LoginInitStatusEvent
    LisStatus,
    LisDescription,
    LisAllowIgnore,
    LisKind,
    /// SetLoginFieldsEvent
    SlfUserName,
    SlfPassword,
    /// QueryAccountResultEvent
    QarEmail,
    QarNewsletter,
    /// GotHTTPDocumentEvent's `mOp`
    GhdOp,
}

/// A list row's field (`ItemGet` / `ItemSet`; round 13).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyCtlItemField {
    /// `ListItem::mHeight`
    Height = 0,
    /// `ListItem::mTop`
    Top,
    /// `ListItem::mWidth`
    Width,
    /// `ListItem::mSelected` (0 or 1)
    Selected,
    /// `ListItem::mSelectable` (0 or 1)
    Selectable,
    /// `ServerDetailsItem::mIconId`
    IconId,
    /// `ServerDetailsItem::mSummaryHeight` (a byte)
    SummaryHeight,
    /// `TextListItem::mColor`
    Color,
    /// `FriendsListItem::mSeparator` (0 or 1)
    FriendSeparator,
    /// `FriendsListItem::mSortGroup` (a signed byte)
    FriendSortGroup,
    /// `UserListItem::mSortKey` (a signed byte)
    UserSortKey,
    /// `UserListItem::mShowSeparator` (0 or 1)
    UserShowSeparator,
}

/// Two `i32`s in one argument (`lo | hi << 32`).
pub fn pack(lo: i32, hi: i32) -> isize {
    (lo as u32 as u64 | ((hi as u32 as u64) << 32)) as isize
}

/// The texts the controls set (the lobby's resource strings).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyCtlString {
    Ready = 0,
    NotReady,
    StartGame,
    LeaveGame,
    DissolveGame,
    Ban,
    Kick,
    CheckingAddress,
    AddressUnavailable,
}

/// The pictures (the lobby's resource images).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyCtlImage {
    Ignored = 0,
    Blocked,
    Muted,
    Away,
    Admin,
    Moderator,
    Captain,
    Normal,
    /// `LobbyGlobal_Firewall_Image`
    Firewall,
    /// `LobbyGlobal_Searching_Image`
    Searching,
}

/// A player row's client tests (`item->mClient->...`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyCtlClientTest {
    IsIgnored = 0,
    IsBlocked,
    /// `IsMuted(true)`
    IsMuted,
    IsAway,
    IsAdmin,
    IsModerator,
    /// `IsCaptain(1)`
    IsCaptain,
    HasPlayer,
}

/// The saved settings the options tab reads and writes (`LobbyGet*` / `LobbySet*`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyCtlPref {
    CrossPromotion = 0,
    DoAutoAfk,
    AutoPingGames,
    ShowGamesInProgress,
    LobbyMusic,
    LobbySoundEffects,
    ChatSoundEffects,
    StagingSoundEffects,
    ShowConfirmations,
    FilterDirtyWords,
    AnonymousToFriends,
    ConnectionSpeed,
}

/// The ops (`a`, `b`, `c` as documented; "the control" is the C++ control this object belongs to).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyCtlOp {
    /// `dynamic_cast<T*>(ComponentConfigGet(a, slot->name))` of the slot `b`'s type, its absence reported
    /// (`ComponentConfigMissing(name, file, line)`); no reference added (the `SmartPtr` assignment follows).
    Find = 1,
    /// `dynamic_cast<T*>(((ComponentConfig*)a)->Fn386860(slot->name))` of the slot `b` (no report).
    FindOptional,
    /// `a->AddRef()` (a `SmartPtr`'s reference).
    AddRef,
    /// `a->Release()`.
    Release,
    /// `a->mControlId = b`.
    SetControlId,
    /// `a->SetEnabled(b != 0)`.
    SetEnabled,
    /// `a->SetVisible(b != 0)`.
    SetVisible,
    /// `a->Invalidate()`.
    Invalidate,
    /// `this->SubscribeToBroadcast(a != 0)`.
    SubscribeBroadcast,
    /// `this->Component::HandleComponentEvent(a)`.
    BaseEvent,
    /// `this->Container::Paint(*(Graphics*)a)`.
    BasePaint,
    /// `a->GetControlId()` (the event's).
    EventControlId,
    /// `((SetRoomSpecFlagsEvent*)a)->mFlags`.
    EventFlags,
    /// `((ClientChangedEvent*)a)->mRoomSpec`.
    EventRoomSpec,
    /// `((LobbyChangeEvent*)a)->IsModify()` (0 or 1).
    EventIsModify,
    /// `((LobbyChangeEvent*)a)->IsAdd()` (0 or 1).
    EventIsAdd,
    /// `((LobbyChangeEvent*)a)->IsDelete()` (0 or 1).
    EventIsDelete,
    /// `((ClientChangedEvent*)a)->mClient.get()`.
    ClientChangedClient,
    /// `((PlayerChangedEvent*)a)->mClient.get()`.
    PlayerChangedClient,
    /// `((ListItemClickedEvent*)a)->IsSelectedItem()` (0 or 1).
    EventIsSelectedItem,
    /// `((ListItemClickedEvent*)a)->mButton`.
    EventButton,
    /// `((ListItemClickedEvent*)a)->mDoubleClick` (0 or 1).
    EventDoubleClick,
    /// `((BrowserComponent*)a)->BackButtonEnabled()` (0 or 1).
    BrowserBackEnabled,
    /// `((BrowserComponent*)a)->ForwardButtonEnabled()` (0 or 1).
    BrowserForwardEnabled,
    /// `((BrowserComponent*)a)->StopButtonEnabled()` (0 or 1).
    BrowserStopEnabled,
    /// `((BrowserComponent*)a)->FindStr(((InputBox*)b)->GetText(), c & 1, c & 2)`.
    BrowserFind,
    /// `((BrowserComponent*)a)->Reset()`.
    BrowserReset,
    /// `((BrowserComponent*)a)->Go(this->mHomeLocation, false, 0, -1)`.
    BrowserGoHome,
    /// `((BrowserComponent*)a)->Back()`.
    BrowserBack,
    /// `((BrowserComponent*)a)->Forward()`.
    BrowserForward,
    /// `((BrowserComponent*)a)->Stop()`.
    BrowserStop,
    /// `((BrowserComponent*)a)->Refresh()`.
    BrowserRefresh,
    /// `((Label*)a)->SetText(((BrowserComponent*)b)->GetCurStatus(), true)`.
    LabelSetBrowserStatus,
    /// `((Button*)a)->IsChecked()` (0 or 1).
    ButtonIsChecked,
    /// `((Button*)a)->SetCheck(b != 0, c != 0)`.
    ButtonSetCheck,
    /// `((Button*)a)->SetText(the string b, true)`.
    ButtonSetText,
    /// `((Label*)a)->SetText(the string b, true)`.
    LabelSetText,
    /// `GetMyLobbyGame()`.
    MyGame,
    /// `((LobbyGame*)a)->GetIsHost()` (0 or 1).
    GameIsHost,
    /// `((LobbyGame*)a)->CanStartGame()` (0 or 1).
    GameCanStart,
    /// `((LobbyGame*)a)->CanReady()` (0 or 1).
    GameCanReady,
    /// `((LobbyGame*)a)->MatchesRoomSpec(b)` (0 or 1).
    GameMatchesSpec,
    /// `((LobbyGame*)a)->GetCaptainId()`.
    GameCaptainId,
    /// `((LobbyGame*)a)->GetClients().get()`.
    GameClients,
    /// `LobbyIAmReady()` (0 or 1).
    IAmReady,
    /// `((LobbyClientList*)a)->GetCount()`.
    ClientsCount,
    /// `((LobbyClientList*)a)->ClientAt(b)`.
    ClientsAt,
    /// `((LobbyClient*)a)->GetClientId()`.
    ClientId,
    /// `((LobbyClient*)a)->HasPlayer()` (0 or 1).
    ClientHasPlayer,
    /// `LobbyKickClient((LobbyClient*)a, b != 0)`.
    KickClient,
    /// `this->DeliverEvent(new ClientActionEvent(a, b, (LobbyClient*)c), false)`.
    DeliverClientAction,
    /// `LobbyBroadcastEvent(new SetChatInputEvent(2, ((LobbyClient*)a)->GetName(), b, false), true)`.
    BroadcastWhisper,
    /// `((ListArea*)a)->GetSelItem()`.
    ListGetSel,
    /// `((ListArea*)a)->GetNumItems()`.
    ListNumItems,
    /// `((ListArea*)a)->GetItem(b)`.
    ListItem,
    /// `((ListArea*)a)->RefreshItem((ListItem*)b)`.
    ListRefresh,
    /// `((ListArea*)a)->ListArea::InsertItem((ListItem*)b, -1)`.
    ListInsert,
    /// `((ListArea*)a)->RemoveItem((ListItem*)b)`.
    ListRemove,
    /// `((ListArea*)a)->Clear(b != 0)`.
    ListClear,
    /// `((ListArea*)a)->BeginUpdate()`.
    ListBeginUpdate,
    /// `((ListArea*)a)->EndUpdate()`.
    ListEndUpdate,
    /// `new GameUserListItem((LobbyClient*)a)` with a reference added (the original's local `SmartPtr`).
    ItemNew,
    /// `((GameUserListItem*)a)->mClient.get()`.
    ItemClient,
    /// `((GameUserListItem*)a)->mClient->` the test `b` (0 or 1).
    ItemClientIs,
    /// `((GameUserListItem*)a)->mImage = ` the picture `b`.
    ItemSetImage,
    /// `dynamic_cast<Button*>((Component*)a)`.
    ButtonOf,
    /// `((Component*)a)->mControlId`.
    ControlId,
    /// `LobbyGet<a>()` (a bool as 0 or 1; the connection speed's value).
    PrefGet,
    /// `LobbySet<a>(b)`.
    PrefSet,
    /// `((ComboBox*)a)->GetSelItem()`.
    ComboGetSel,
    /// `((ComboBox*)a)->SetSelItem(b, c != 0)`.
    ComboSetSel,
    /// `((ComboBox*)a)->Clear(b != 0)`.
    ComboClear,
    /// `((ComboBox*)a)->SetTextAndSel(LobbyGetTabSelect(), false)`.
    ComboShowTabSelect,
    /// `LobbySetTabSelect(((ComboBox*)a)->mEdit->GetText())`.
    SaveTabSelect,
    /// `LobbySetLanName(((ComboBox*)a)->mEdit->GetText())`.
    SaveLanName,
    /// `LobbyGetLanNameCount()`.
    LanNameCount,
    /// `((ComboBox*)a)->AddString(LobbyGetLanNameAt(b), -1)`.
    ComboAddLanName,
    /// `((ComboBox*)a)->SetTextAndSel(LobbyGetLanName(), true)`.
    ComboShowLanName,
    /// `LobbyGetLanBroadcastPort()`.
    LanBroadcastPort,
    /// `sprintf(port, "%d", b); ((Label*)a)->SetText(port, true)` (a 20-byte buffer).
    LabelSetNumber,
    /// `((Label*)a)->SetText(IPAddr::GetLocalAddr().GetHostString(true), true)`.
    LabelSetLocalAddress,
    /// `LobbyBroadcastLobbyEvent(a, b != 0)`.
    BroadcastLobbyEvent,
    /// `LobbyRestoreDefaults()`.
    RestoreDefaults,
    /// `LobbySetNetworkAdapter(LobbyGetNetworkAdapter())`.
    ResetNetworkAdapter,
    /// `LobbyResetFilters()`.
    ResetFilters,
    /// `LobbyGetCurrentScreen()`.
    CurrentScreen,
    /// `((Component*)a)->BroadcastEventDown(new LobbyEvent(b))`.
    BroadcastDown,
    /// `LobbyGlobal_LobbyMusic_Sound->Resume()`.
    MusicResume,
    /// `LobbyGlobal_LobbyMusic_Sound->Stop()`.
    MusicStop,
    /// `LobbyGlobal_ButtonClick_Sound->SetMuted(a != 0)`.
    ClickSoundMute,
    /// `LobbyGlobal_SierraLogon_Sound->Stop()`.
    LogonSoundStop,
    /// `LobbyRemoveLanName()`.
    RemoveLanName,
    /// `LobbyOnLoginScreen()` (0 or 1).
    OnLoginScreen,
    /// `LobbyOnLanScreen()` (0 or 1).
    OnLanScreen,
    /// `LobbyIsFirewallTestDone()` (0 or 1).
    FirewallTestDone,
    /// `LobbyIsBehindFirewall()` (0 or 1).
    BehindFirewall,
    /// `((ImageComponent*)a)->SetImage(` the picture `b` `.get())`.
    IconSetImage,
    /// `LobbyGetPublicAddress().IsValid()` (0 or 1).
    PublicAddressValid,
    /// `((Label*)a)->SetText(LobbyGetPublicAddress().GetHostString(true), true)`.
    LabelSetPublicAddress,
    /// `ComponentConfigMissing(std::string("Invalid type for ") + slot a's name, its file, 89)`.
    ReportInvalidType,
    /// `dynamic_cast<Button*>((Component*)a) != NULL`.
    IsButton,
    /// `dynamic_cast<ComboBox*>((Component*)a) != NULL`.
    IsCombo,
    /// `sprintf(buffer, "%d", b); ((ComboBox*)a)->AddString(buffer)` (a 50-byte buffer).
    ComboAddNumber,
    /// `EELobbyFillMapTypeCombo((ComboBox*)a)`.
    FillMapTypes,
    /// `((ComboBox*)a)->mListArea->GetSelItem() != NULL`.
    ComboHasSel,
    /// `((EELobbyGame*)a)->mRust`: the game's Rust object, which this object calls directly (round 6: the game's
    /// options, slots and changes were ops that bounced through the C++ forwarders).
    GameRust,
    /// `((Label*)a)->SetText(GUIString(b))` (`b` a `GUIStringData`, 0 the empty string).
    LabelSetName,
    /// `LobbyIAmCaptain()` (0 or 1).
    IAmCaptain,
    /// `((EELobbySyncOptionEvent*)a)->mParam`.
    EventParam,
    /// The saved game's choice: `EELobbyFn2988E9()`'s directory, `EELobbyGetFileDialog()`, its
    /// `ListFiles(dir, ".ees")` and `LobbyDoDialog`'s result (-1 without a dialog).
    ChooseSavedGame,
    /// `((ComboBox*)a)->SetText(GUIString(b))` (`b` a `GUIStringData`, 0 the empty string; round 5:
    /// EELobbyPlayerOptionsCtrl).
    ComboSetName,
    /// `((ComboBox*)a)->mEdit->mTextColor = b`.
    ComboSetTextColor,
    /// `GetMyLobbyPlayer()->GetReady()` (0 or 1).
    MyPlayerReady,
    /// `GetMyLobbyPlayer()`.
    MyPlayer,
    /// `((EELobbyPlayer*)a)->GetSlot()`.
    PlayerSlot,
    /// `((LobbyClient*)a)->GetPlayer().mPtr`.
    ClientPlayer,
    /// `((EELobbyColorButton*)a)->SetColor(b)`.
    ColorSetColor,
    /// `((EELobbyColorButton*)a)->mColor`.
    ColorGetColor,
    /// `sprintf(buffer, "%d", b); ((Label*)a)->SetText(LobbyFormatString(TransferPercent, buffer))` (50 bytes).
    LabelSetPercent,
    // ---- round 13: the Internet controls (srvlistctl.rs, netctl.rs). Texts are `GUIStringData*` (borrowed; 0 the
    // empty string), a resource `res` an `EeWonLobbyRes` row, a point or size two `i32`s packed (`lo | hi << 32`).
    /// `ee_won_model_get(model of (LobbyModelObject*)a, b)` (answered by modelobj.rs in the game).
    MGet = 1000,
    /// `ee_won_model_set(model of a, b, c)`.
    MSet,
    /// `ee_won_model_call(model of a, b, c)` (`b` an `EeWonModelCall`).
    MCall,
    /// `ServerDetailsItem::CreateItem()` with a reference added (the original's local `SmartPtr`).
    RowNew = 1010,
    /// `((ServerDetailsItem*)a)->mGame.get()`.
    RowGame,
    /// `((ServerDetailsItem*)a)->mGame = (LobbyGame*)b`.
    RowSetGame,
    /// `a->mIPAddrStr = a->mGame->GetAddress().GetHostAndPortString(true)`.
    RowSetAddrText,
    /// `((ServerDetailsItem*)a)->mIPAddrStr` (a text).
    RowAddrText,
    /// A list row's field `b` (`EeWonLobbyCtlItemField`).
    ItemGet,
    /// A list row's field `b` set to `c`.
    ItemSet,
    /// `((ServerDetailsItem*)a)->GetDetailsHeightHook((MultiListArea*)b)`.
    RowDetailsHeight,
    /// `((ServerDetailsItem*)a)->PaintDetailsHook(*(Graphics*)b, (MultiListArea*)c)`.
    RowPaintDetails,
    /// `((Component*)a)->GetDefaultFont()`.
    DefaultFont = 1030,
    /// `((Component*)a)->GetDefaultFontMod(b, c)`.
    DefaultFontMod,
    /// `((Component*)a)->GetFontMod((Font*)b, c, 0)`.
    FontMod,
    /// `((ListArea*)a)->mFont.get()`.
    ListFont,
    /// `((Font*)a)->GetHeight()`.
    FontHeight,
    /// `gLobbyResources[a].mImage->mHeight` (`LOBBY_IMAGE(...)->mHeight`).
    ResImageHeight,
    /// `((MultiListArea*)a)->GetColumnWidth(b)`.
    ColumnWidth,
    /// `((ListArea*)a)->EnsureVisible((ListItem*)b)`.
    ListEnsureVisible,
    /// `((ListArea*)a)->GetItemPos((ListItem*)b)`.
    ListItemPos,
    /// `((ListArea*)a)->SetSelItem(b, c != 0)`.
    ListSetSel,
    /// `((ListArea*)a)->GetVertOffset()`.
    ListVertOffset,
    /// `((ListArea*)a)->SetVertOffset(b)`.
    ListSetVertOffset,
    /// `((ListArea*)a)->GetViewHeight()`.
    ListViewHeight,
    /// `((ListArea*)a)->GetItemPosAtYPos(b)`.
    ListItemAtY,
    /// `((ListArea*)a)->GetPaintWidth()`.
    ListPaintWidth,
    /// `((MultiListArea*)a)->GetColumnPaintContext()`.
    ListPaintColumn,
    /// `((MultiListArea*)a)->GetSortColumn()`.
    ListSortColumn,
    /// `((MultiListArea*)a)->GetSortColumnPrev()`.
    ListSortColumnPrev,
    /// `((ListArea*)a)->DeliverEvent(new PingGameEvent((LobbyGame*)b, true), true)`.
    ListDeliverPing,
    /// `((ListArea*)a)->GetTotalWidth()`.
    ListTotalWidth,
    /// `((ListArea*)a)->GetTextColor()`.
    ListTextColor,
    /// `((ListArea*)a)->ListFlagSet(b)` (0 or 1).
    ListFlagSet,
    /// `((Graphics*)a)->GetStandardColor(b)`.
    GfxStandardColor = 1060,
    /// `((ListArea*)b)->GetSelColor().GetForegroundColor(*(Graphics*)a, c)`.
    GfxSelFore,
    /// `((ListArea*)b)->GetSelColor().GetBackgroundColor(*(Graphics*)a)`.
    GfxSelBack,
    /// `((Graphics*)a)->SetColor(b)`.
    GfxSetColor,
    /// `((Graphics*)a)->SetFont((Font*)b)`.
    GfxSetFont,
    /// `((Graphics*)a)->SetSysColor(b)`.
    GfxSetSysColor,
    /// `((Graphics*)a)->FillRect(b's point, c's size)`.
    GfxFillRect,
    /// `((Graphics*)a)->Translate(b, c)`.
    GfxTranslate,
    /// `((Graphics*)a)->DrawImage(gLobbyResources[b].mImage.get(), c's point)`.
    GfxDrawResImage,
    /// `((Graphics*)a)->DrawString(GUIString(b), c's point)`.
    GfxDrawText,
    /// `((Graphics*)a)->DrawString(gLobbyResources[b].mString, c's point)`.
    GfxDrawResText,
    /// `((Graphics*)a)->mFont->GetStringWidth(GUIString(b))`.
    GfxTextWidth,
    /// `DrawLobbyPing((Graphics*)a, b, c, 0, 0)`.
    GfxDrawPing,
    /// `((Graphics*)a)->DrawLine(b's point, c's point)`.
    GfxDrawLine,
    /// `((Graphics*)a)->DrawImage((Image*)b, c's point)`.
    GfxDrawImage,
    /// `new GUIStringData(b bytes at (const char*)a)` with a reference (a temporary `GUIString`; `Release` ends it).
    TextNew = 1080,
    /// `GUIString(a).CompareNoCase(GUIString(b))`.
    TextCompareNoCase,
    /// `GUIString(a).findNoCase(GUIString(b), 0)`.
    TextFindNoCase,
    /// `GUIString(a).findNoCase(gLobbyResources[b].mString, 0)`.
    TextFindRes,
    /// `GUIString(a).length()`.
    TextLen,
    /// `GUIString(a)[b]` (a unit).
    TextAt,
    /// `std::string s = GUIString(a)`: its bytes copied to `(char*)b` (at most `c`), its length returned.
    TextBytes,
    /// `GUIString(a) == GUIString(b)` (0 or 1).
    TextEquals,
    /// `((LobbyGame*)a)->GetAddress().StringCompare(((LobbyGame*)b)->GetAddress())`.
    GameCompareAddress,
    /// `((ComboBox*)a)->mEdit->GetText()` (a text).
    ComboEditText = 1100,
    /// `LobbyGetNumPingsToDo()`.
    PingsToDo,
    /// `LobbyKillPings(1)`.
    KillPings,
    /// `this->DeliverEvent(new GameActionEvent(7, (LobbyGame*)a), false)`.
    DeliverGameAction,
    /// `this->DeliverEvent(new JoinGameEvent((LobbyGame*)a), false)`.
    DeliverJoinGame,
    /// `((GameChangedEvent*)a)->mGame.get()`.
    EventGame,
    /// `((SyncGameListEvent*)a)->mGames.get()`.
    EventGames,
    /// `((ListItemClickedEvent*)a)->mX`.
    EventX,
    /// `((ListItemClickedEvent*)a)->mY`.
    EventY,
    /// The event's `mItem.get()` (`ComboSelChangedEvent`, `ListItemClickedEvent`, `TreeItemOpenedEvent`).
    EventItem,
    /// `((ComboSelChangedEvent*)a)->mActiveSel` (0 or 1).
    EventActiveSel,
    /// `sprintf(n, "%d", b); ((Label*)a)->SetText(LobbyFormatString(ListPingSummary, n), true)`.
    LabelSetPingSummary,
    /// `sprintf` of `b` and `c`; `((Label*)a)->SetText(LobbyFormatString2(ListSummary, b's, c's), true)`.
    LabelSetListSummary,
    /// `((Button*)a)->SetText(gLobbyResources[b].mString, true)`.
    ButtonSetRes,
    /// `((Label*)a)->SetText(gLobbyResources[b].mString, true)`.
    LabelSetRes,
    /// `this->RequestTimer(a != 0)`.
    RequestTimer,
    /// `this->mTimerFlags &= ~2`.
    ClearTimerFlag,
    /// `this->Container::TimerEvent(a)` (0 or 1).
    BaseTimer,
    /// `((ComboBox*)a)->InsertItem(new TextListItem(NoFilter, (Font*)b), -1)`.
    ComboInsertNoFilter,
    /// `((ComboBox*)a)->InsertItem(new FilterListItem(GUIString(b), (Font*)c), -1)`.
    ComboInsertFilterRow,
    /// `LobbyGetFilterValues(GUIString(b))`'s values, each `((ComboBox*)a)->AddString(value, -1)`.
    ComboAddFilterValues,
    /// `((ComboBox*)a)->mListArea->GetItem(b)`.
    ComboItem,
    /// `((ComboBox*)a)->mListArea->GetNumItems()`.
    ComboCount,
    /// `((ComboBox*)a)->mListArea->GetItemPos((ListItem*)b)`.
    ComboItemPos,
    /// `((TextListItem*)a)->mText == GUIString(b)` (0 or 1).
    TextItemIs,
    /// `((ComboBox*)a)->UpdateItem((ListItem*)b)`.
    ComboUpdateItem,
    /// `((ComboBox*)a)->SetText(GUIString(b), true)`.
    ComboSetText,
    /// `((ComboBox*)a)->AddString(GUIString(b), -1)`.
    ComboAddString,
    /// `((ComboBox*)a)->DeleteItem(b)`.
    ComboDeleteItem,
    /// `((ComboBox*)a)->mEdit->SelectRange(-1, 0)`.
    ComboSelectEnd,
    /// `((FilterListItem*)a)->mIsAdd` (0 or 1).
    FilterIsAdd,
    /// `((FilterListItem*)a)->mSelPos`.
    FilterSelPos,
    /// `((FilterListItem*)a)->mFilterText` (a text).
    FilterText,
    /// `((FilterListItem*)a)->mColumnName` (a text).
    FilterColumn,
    /// The empty filter: `a->mText = AddRemove; a->mIsAdd = true; a->mFilterText = GUIString(""); a->mSelPos = 0`.
    FilterClear,
    /// `a->mSelPos = b & 0xffffffff; a->mIsAdd = b >> 32; a->mFilterText = GUIString(c)`, then the label (Add or
    /// Remove, the text, the closing quote and parenthesis).
    FilterSet,
    /// `LobbyAddFilterValue(GUIString(a), GUIString(b))`.
    AddFilterValue,
    /// `LobbyRemoveFilterValue(GUIString(a), GUIString(b))`.
    RemoveFilterValue,
    /// An EE game column's handler (`EeWonEeGlueCall` `a`; `b`, `c` its arguments; a filter's text a text).
    EeColumn,
    /// The event `a`'s member `b` (`EeWonLobbyCtlEvField`).
    EvGet = 1200,
    /// The event `a`'s `mHandled` (the member `b`) set.
    EvSetHandled,
    /// `new ChatLink(*(const EeWonLobbyCtlLink*)a)` (the constructor of its kind) with a reference added.
    ChatLinkNew,
    /// The link `a`'s member `b` (`EeWonLobbyCtlLinkField`).
    LinkGet,
    /// The link `a`'s member `b` set to `c`.
    LinkSet,
    /// `LobbyAppendFormattedText` over `*(const EeWonLobbyCtlFormat*)a`.
    ChatAddFormatted,
    /// `gLobbyResources[a].mString` (a text, the resource's own).
    ResText,
    /// `((TextArea*)a)->mFont.get()`.
    TextAreaFont,
    /// `((TextArea*)a)->mLinkFont.get()`.
    TextAreaLinkFont,
    /// `((TextArea*)a)->AddSegment(GUIString(b), c, true)`.
    TextAreaAddSegment,
    /// `((TextArea*)a)->LineBreak(b)`.
    TextAreaLineBreak,
    /// `((TextArea*)a)->Reset(b != 0)`.
    TextAreaReset,
    /// `((InputBox*)a)->GetText()` (a text).
    InputText,
    /// `((InputBox*)a)->AddInputHistory(GUIString(b), true)`.
    InputAddHistory,
    /// `((InputBox*)a)->Clear(b != 0)`.
    InputClear,
    /// `((InputBox*)a)->SetText(GUIString(b), c != 0)`.
    InputSetText,
    /// `((InputBox*)a)->SelectRange(b, c)`.
    InputSelectRange,
    /// `((Component*)a)->RequestFocus()`.
    RequestFocus,
    /// `((Component*)a)->IsDisabled()` (0 or 1).
    IsDisabled,
    /// `this->IsRootVisible()` (0 or 1).
    IsRootVisible,
    /// `LobbyGetCurrentServer()`.
    CurrentServer,
    /// `GetMyLobbyClient()`.
    MyClient,
    /// `LobbyGetChatCommandParser()`.
    ChatParser,
    /// `LobbyGetRoomChatParser(a)`.
    RoomChatParser,
    /// `((ChatCommandParser*)a)->` GetPrefix (`b` 0), GetError (1), GetCommand (2).
    ParserNumber,
    /// `((ChatCommandParser*)a)->` GetTargetName (`b` 0), GetText (1): held.
    ParserText,
    /// `((ChatCommandParser*)a)->HandleInput(GUIString(b))` (0 or 1).
    ParserHandleInput,
    /// `((ChatCommandParser*)a)->GetHelp(b)`: held.
    ParserHelp,
    /// `((ChatCommandParser*)a)->GetClients()`.
    ParserClients,
    /// The prefix, `GetCommandString(b)`, ` "`, the name `c` (0: `GetTargetName()`), `" `: held.
    ParserCommandText,
    /// `LobbyFormatString(gLobbyResources[a].mString, ((ChatCommandParser*)b)->GetTargetName())`: held.
    FormatTarget,
    /// The held text `a` with `GUIString(b)` appended: held (`a`'s reference given back).
    TextAppend,
    /// The held text `a` with `c` bytes at `b` appended: held.
    TextAppendBytes,
    /// `GUIString(a).find(GUIString(b), 0)`.
    TextFind,
    /// `LobbyGetIgnoredNameCount()`.
    IgnoredCount,
    /// `LobbyGetIgnoredNameAt(a)`: held.
    IgnoredAt,
    /// `this->Fn2F5380(a, b)` (FireEvent).
    FireEvent,
    /// `this->DeliverEvent(new FriendActionEvent(a, (LobbyFriend*)b, c), false)`.
    DeliverFriendAction,
    /// `this->DeliverEvent(new JoinFriendsRoomEvent((LobbyFriend*)a), false)`.
    DeliverJoinFriendsRoom,
    /// `this->DeliverEvent(new JumpToRoomEvent(link->mText, link->mGroupId, link->mAddress), false)` of the link `a`.
    DeliverJumpToRoom,
    /// `this->DeliverEvent(new AcceptClientEvent((LobbyClient*)a, b != 0), false)`.
    DeliverAcceptClient,
    /// `((const EeWonIpAddr*)a)` valid (0 or 1; answered by ipobj.rs in the game).
    AddrValid,
    /// `((ListArea*)a)->InsertItem((ListItem*)b, -1)` (the virtual).
    ListInsert2 = 1300,
    /// `((TreeArea*)a)->InsertTreeItem((TreeItem*)b, (ListItem*)c, -1)`.
    TreeInsertChild,
    /// `((TreeArea*)a)->InsertTreeItem(NULL, (ListItem*)b, c)`.
    TreeInsertServer,
    /// `((TreeArea*)a)->InsertTreeItem((TreeItem*)b, (ListItem*)c, 0)`.
    TreeInsertLoading,
    /// `((TreeItem*)a)->GetWrappedItem()`.
    TreeWrapped,
    /// `((TreeItem*)a)->GetNumChildren()`.
    TreeChildCount,
    /// `((TreeItem*)a)->GetChild(b)`.
    TreeChild,
    /// `((TreeItem*)a)->GetParentItem()`.
    TreeParent,
    /// `((TreeItem*)a)->GetIndex()`.
    TreeIndex,
    /// `((TreeItem*)a)->IsOpen()` (0 or 1).
    TreeIsOpen,
    /// `((TreeArea*)a)->OpenItem((TreeItem*)b, c != 0)`.
    TreeOpen,
    /// `((TreeArea*)a)->RemoveAllItems((TreeItem*)b)`.
    TreeRemoveAll,
    /// `((TreeArea*)a)->RemoveTreeItem((TreeItem*)b)`.
    TreeRemove,
    /// `((TreeArea*)a)->ClearTree(b != 0)`.
    TreeClear,
    /// `((TreeItem*)a)->ContainsPoint(b's point)` (0 or 1).
    TreeContainsPoint,
    /// `new RoomListItem((LobbyServer*)a, b != 0)` (a server's row, its loading placeholder).
    RoomItemServer,
    /// `new RoomListItem((LobbyGroup*)a, (LobbyServer*)b)` (a room's row).
    RoomItemGroup,
    /// `((RoomListItem*)a)->UpdateText((LobbyServer*)b, (LobbyGroup*)c)`.
    RoomItemUpdate,
    /// `((RoomListItem*)a)->mAddress.GetValue()` (a borrowed `const EeWonIpAddr*`).
    RoomItemAddr,
    /// `((RoomListItem*)a)->mGroupId`.
    RoomItemGroupId,
    /// `IPAddr` `a == b` (six bytes; 0 or 1; answered by ipobj.rs in the game).
    AddrEquals,
    /// A copy of the address `a` (an `EeWonIpAddr` the Rust frees with `AddrFree`; ipobj.rs in the game).
    AddrClone,
    /// The copy `a` freed.
    AddrFree,
    /// `this->DeliverEvent(new JoinRoomEvent(row->mAddress, row->mGroupId), false)` of the room row `a`.
    DeliverJoinRoom,
    /// `this->DeliverEvent(new CreateRoomEvent(the address a, NULL: IPAddr()), false)`.
    DeliverCreateRoom,
    /// `this->DeliverEvent(new QueryServerGroupsEvent(the address a), false)`.
    DeliverQueryGroups,
    /// `LobbyGetServerList()`.
    ServerListGet,
    /// `LobbyGetChatGroup()`.
    ChatGroup,
    /// `new FriendsListItem` (no reference).
    FriendRowNew,
    /// `new UserListItem` (no reference).
    UserRowNew,
    /// `((FriendsListItem*)a)->mFriend.get()`.
    ItemFriend,
    /// `((FriendsListItem*)a)->mFriend = (LobbyFriend*)b`.
    ItemSetFriend,
    /// `((UserListItem*)a)->mClient.get()`.
    ItemUserClient,
    /// `((UserListItem*)a)->mClient = (LobbyClient*)b`.
    ItemSetUserClient,
    /// `((TextListItem*)a)->mText = GUIString(b)`.
    ItemSetText,
    /// `((TextListItem*)a)->mImage = gLobbyResources[b].mImage` (-1: NULL).
    ItemSetImageRes,
    /// `((TextListItem*)a)->mFont = (Font*)b`.
    ItemSetFont,
    /// `LobbyGetFriendCount()`.
    FriendCount,
    /// `LobbyGetFriendAt(a)`.
    FriendAt,
    /// `LobbyGetCurrentUser()`.
    CurrentUser,
    /// `((Label*)a)->SetText(LobbyFormatString(UserWelcome, ((LobbyUserInfo*)b)->mUserName), true)`.
    LabelSetUserWelcome,
    /// `this->DeliverEvent(new QuickPlayEvent(a), false)`.
    DeliverQuickPlay,
    /// `this->Dialog::HandleComponentEvent(a)`.
    DialogBaseEvent,
    /// `LobbyGetUserCount()`.
    UserCount,
    /// `((ComboBox*)a)->AddString(LobbyGetUserNameAt(b), -1)`.
    ComboAddUserName,
    /// `LobbyGetUserLastUseAt(a)`.
    UserLastUseAt,
    /// `GetLobbyConfig() != NULL && GetLobbyConfig()->mPublic` (0 or 1).
    ConfigIsPublic,
    /// `LobbyGetUserPassword(GUIString(a))`: held.
    UserPassword,
    /// `LobbyAddUser(GUIString(a), GUIString(b), true)`.
    AddUser,
    /// `((ComboBox*)a)->SetText(GUIString(b), c != 0)`.
    ComboSetTextFlag,
    /// `this->DeliverEvent(new LoginEvent(((InputBox*)a)->GetText(), ((Button*)b)->IsChecked(),
    /// ((ComboBox*)c)->mEdit->GetText()), false)`.
    DeliverLogin,
    /// `GUIString(a).mData->ToInt(0)`.
    TextToInt,
    /// The create-account event of the controls `(const intptr_t*)a` (user, password, e-mail, day, month, year,
    /// newsletter) delivered.
    DeliverCreateAccount,
    /// `GetLobbyAuthContext()`.
    AuthContext,
    /// `((InputBox*)a)->SetText(GUIString(new GUIStringData(((AuthContext*)b)->GetName())), true)`.
    InputSetAuthName,
    /// `GUIString(new GUIStringData(((AuthContext*)a)->GetPassword()))`: held.
    AuthPassword,
    /// `this->DeliverEvent(new UpdateAccountEvent(((InputBox*)a)->GetText(), ((InputBox*)b)->GetText(),
    /// ((Button*)c)->IsChecked()), false)`.
    DeliverUpdateAccount,
    /// `this->DeliverEvent(new GetLostPasswordEvent(((ComboBox*)a)->mEdit->GetText()), false)`.
    DeliverLostPassword,
    /// `this->DeliverEvent(new GetLostUserNameEvent(((InputBox*)a)->GetText()), false)`.
    DeliverLostUserName,
    /// `((BrowserComponent*)a)->Go(ComponentConfig::GetResourceFile("help/loginhelp.html"), false, 0, -1)`.
    BrowserGoLoginHelp,
    /// `((Label*)a)->SetText(GUIString(b), true)`.
    LabelSetData,
    /// `((Container*)a)->EnableLayoutInstructions(b, c != 0)`.
    EnableLayout,
    /// `((Container*)a)->Relayout()`.
    Relayout,
    /// `((RustServerOp*)a)->GetNum(EE_WON_HTTPDOC_TYPE) == HTTPDocType_MOTD` (0 or 1).
    HttpDocIsMotd,
    /// `((RustServerOp*)a)->GetBytes(EE_WON_HTTPDOC_PATH, b ? Game : Sys)`: copied to `(char*)c` (1024 bytes at
    /// most), its length returned.
    HttpDocPath,
    /// `((BrowserComponent*)a)->ShowFiles(the paths at (const EeWonLobbyCtlFiles*)b, true)`.
    BrowserShowFiles,
    /// `((ScreenContainer*)a)->ShowScreen((Component*)b, c != 0)`.
    ScreenShow,
    /// `((ServerListCtrl*)a)->GetSelGame()`.
    ListSelGame,
    /// `this->DeliverEvent(new ChangeDirectConnectEvent(a, (LobbyGame*)b), false)`.
    DeliverDirectConnect,
    /// `((Label*)a)->SetText(LobbyFormatString3(GUIString(b), the texts at (const intptr_t*)c), true)`.
    LabelSetFormat3,
    // ---- round 16: the rows' own ops (rows.rs; with no control)
    /// `new RustTextRow((EeWonRow*)a)` when `b` is set, else `new RustRow((EeWonRow*)a)` (no reference).
    RowShellNew,
    /// `((RustTextRow*)a)->TextListItem::Paint(*(Graphics*)b, (ListArea*)c)`.
    RowTextPaint,
    /// `((RustTextRow*)a)->TextListItem::CalcDimensions((ListArea*)b)`.
    RowTextCalc,
    /// `((RustTextRow*)a)->TextListItem::Compare((ListItem*)b, (ListArea*)c)`.
    RowTextCompare,
    /// `((TextListItem*)a)->mText = gLobbyResources[b].mString`.
    ItemSetTextRes,
    /// `((TextListItem*)a)->mText = GUIString(b)`, then ` (%d)` of `c`'s low half appended when its high half is set.
    ItemSetTextCount,
    /// `((TextListItem*)a)->mText.erase()`.
    ItemTextErase,
    /// `((TextListItem*)a)->mImage.get()`.
    ItemImage,
    /// `((TextListItem*)a)->mFont.get()`.
    ItemFont,
    /// `((Font*)b)->GetStringWidth(t, 0, t.length())` of the item `a`'s text `t`.
    ItemTextWidth,
    /// `((Graphics*)b)->DrawString(((TextListItem*)a)->mText, c's point)`.
    ItemTextDraw,
    /// `((TextListItem*)a)->mText.CompareNoCase(((TextListItem*)b)->mText)`.
    ItemTextCompare,
    /// `((Image*)a)->mWidth`.
    ImageWidth,
    /// `((Image*)a)->mHeight`.
    ImageHeight,
    /// `gLobbyResources[a].mImage.get()`.
    ResImage,
    /// `((LobbyRustCtrl*)(ServerListCtrl*)((ListArea*)a)->mParent->mParent)->mLobbyCtl`: a game list row's control.
    ListOwnerCtl,
    /// `((LobbyGame*)a)->GetAddress().GetHostAndPortString(true)` (held).
    GameAddrText,
}

/// The inward calls.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyCtlCall {
    /// `InitComponent` of a config init: `a` the `ComponentConfig*`.
    Init = 1,
    /// `HandleComponentEvent(a)`: `b` the `EeWonLobbyCtlEvent`, `c` the event's `mComponent`.
    Event,
    /// GameBrowserCtrl::Paint(`*(Graphics*)a`).
    Paint,
    /// GameBrowserCtrl::ResetBrowser.
    ResetBrowser,
    /// a member: `a` < 48 a control member, 100 the room flags, 101 in game (EELobbyGameOptionsCtrl and
    /// EELobbyPlayerOptionsCtrl: active), 102 the last ready state, 103 the browser's reset flag, 104
    /// EELobbyPlayerOptionsCtrl's own slot, 105 its host flag; 110.. ServerListCtrl's (the game type, the filter
    /// flag, the filter time, the column count, the rows, the columns).
    Field,
    /// ServerListCtrl::TimerEvent(`a`) (0 or 1; round 13).
    Timer,
    /// A row's virtual (`EeWonLobbyCtlRowCall` `a`, the row `b`, `c` its three arguments `const intptr_t[3]`).
    Row,
    /// The game list's config reader (`EeWonLobbyCtlCfg` `a`, its arguments `b`, `c`).
    Cfg,
    /// A config reader's store: the member `a` set to `b` (ChatCtrl: 0..=10 the colours, 20 the room kind;
    /// RoomLabelCtrl: 0 the prefix, 1 the room kind, 2 the user count shown, 3 the server shown).
    SetField,
    /// LoginScreen's page `a` shown (0 the message of the day, 1 create account, 2 lost password, 3 lost user name).
    Page,
}

/// The host: `(control, op, a, b, c) -> result`.
pub type EeWonLobbyCtlHostFn =
    Option<unsafe extern "C" fn(ctl: *mut c_void, op: EeWonLobbyCtlOp, a: isize, b: isize, c: isize) -> isize>;

use crate::eegame::{game_call, EeWonEeGameCall as GC};
use EeWonLobbyCtlEvent as Ev;
use EeWonLobbyCtlKind as Kind;
use EeWonLobbyCtlOp as Op;
use EeWonLobbyCtlString as S;

const MEMBERS: usize = 48;

macro_rules! slot {
    ($name:literal, $file:literal, $line:expr, $ty:ident, $id:expr, $member:expr) => {
        EeWonLobbyCtlSlot {
            name: $name.as_ptr(),
            file: $file.as_ptr(),
            line: $line,
            ty: EeWonLobbyCtlType::$ty,
            control_id: $id,
            member: $member,
        }
    };
}

// the browser's members: Browser, Back, Forward, Stop, Home, Refresh, FindInput, FindNext, FindPrev, CaseSensitive,
// StatusLabel
const B_BROWSER: usize = 0;
const B_BACK: usize = 1;
const B_FORWARD: usize = 2;
const B_STOP: usize = 3;
const B_HOME: usize = 4;
const B_REFRESH: usize = 5;
const B_FIND_INPUT: usize = 6;
const B_FIND_NEXT: usize = 7;
const B_FIND_PREV: usize = 8;
const B_CASE: usize = 9;
const B_STATUS: usize = 10;
// the staging buttons
const S_LEAVE: usize = 0;
const S_START: usize = 1;
// the options' members (the original's member order)
const O_TAB: usize = 0;
const O_SPEED: usize = 1;
const O_CROSS: usize = 2;
const O_LOBBY_SFX: usize = 3;
const O_CHAT_SFX: usize = 4;
const O_STAGING_SFX: usize = 5;
const O_MUSIC: usize = 6;
const O_CONFIRM: usize = 7;
const O_IN_PROGRESS: usize = 8;
const O_ANONYMOUS: usize = 9;
const O_AUTO_PING: usize = 10;
const O_LOCAL_ADDR: usize = 11;
const O_PUBLIC_ADDR: usize = 12;
const O_FIREWALL: usize = 13;
const O_USER_NAME: usize = 14;
const O_BROADCAST: usize = 15;
const O_DIRTY_WORDS: usize = 16;
const O_AUTO_AFK: usize = 17;
// the player list
const U_LIST: usize = 0;
const U_KICK: usize = 1;
// the game options: the campaign label and boxes, then `mOptions[16]`
const G_LABEL: usize = 0;
const G_BOX1: usize = 1;
const G_BOX2: usize = 2;
const G_OPTION: usize = 3;
// the player rows: `mUserNameCombo[8]`, `mTeamCombo[8]`, `mColorButton[8]`, `mCitizenCombo[8]`, `mObserveCheck[8]`,
// `mFileTransferLabel[8]`
const P_NAME: usize = 0;
const P_TEAM: usize = 8;
const P_COLOR: usize = 16;
const P_CITIZEN: usize = 24;
const P_OBSERVE: usize = 32;
const P_TRANSFER: usize = 40;
/// The game options' table (`sOptionTable`): the eleven combo boxes, then the five check boxes.
const OPTION_IS_CHECK: [bool; 16] =
    [false, false, false, false, false, false, false, false, false, false, false, true, true, true, true, true];

/// Every look-up, kind by kind in the originals' order (the slot index is what `Find` passes).
/// ServerListCtrl's first look-up in `SLOTS` (round 13).
pub(crate) const SLOT_SERVER_LIST: usize = 103;

/// ChatCtrl's first look-up in `SLOTS` (round 13).
pub(crate) const SLOT_CHAT: usize = 108;

/// The first look-up in `SLOTS` of each of round 13's small kinds (netctl.rs).
pub(crate) fn slot_of(kind: Kind) -> usize {
    match kind {
        Kind::Room => 111,
        Kind::Friends => 114,
        Kind::Users => 117,
        Kind::QuickPlay => 120,
        Kind::Login => 126,
        Kind::CreateAccount => 130,
        Kind::UpdateAccount => 144,
        Kind::LostPassword => 157,
        Kind::LostUserName => 159,
        Kind::LoginHelp => 161,
        Kind::LoginStatus => 162,
        Kind::InitFailed => 163,
        Kind::Motd => 166,
        Kind::LoginScreen => 167,
        Kind::DirectConnect => 173,
        Kind::RoomLabel => 176,
        _ => 0,
    }
}

pub static SLOTS: [EeWonLobbyCtlSlot; 177] = [
    // GameBrowserCtrl::InitComponent (0..=10)
    slot!(c"Browser", c"GameBrowserCtrl.cpp", 27, Browser, -1, B_BROWSER as i32),
    slot!(c"BackButton", c"GameBrowserCtrl.cpp", 28, Button, -1, B_BACK as i32),
    slot!(c"ForwardButton", c"GameBrowserCtrl.cpp", 29, Button, -1, B_FORWARD as i32),
    slot!(c"StopButton", c"GameBrowserCtrl.cpp", 30, Button, -1, B_STOP as i32),
    slot!(c"HomeButton", c"GameBrowserCtrl.cpp", 31, Button, -1, B_HOME as i32),
    slot!(c"RefreshButton", c"GameBrowserCtrl.cpp", 32, Button, -1, B_REFRESH as i32),
    slot!(c"FindInput", c"GameBrowserCtrl.cpp", 33, Input, -1, B_FIND_INPUT as i32),
    slot!(c"FindNextButton", c"GameBrowserCtrl.cpp", 34, Button, -1, B_FIND_NEXT as i32),
    slot!(c"FindPrevButton", c"GameBrowserCtrl.cpp", 35, Button, -1, B_FIND_PREV as i32),
    slot!(c"CaseSensitiveCheck", c"GameBrowserCtrl.cpp", 36, Button, -1, B_CASE as i32),
    slot!(c"StatusLabel", c"GameBrowserCtrl.cpp", 37, Label, -1, B_STATUS as i32),
    // GameStagingCtrl::InitComponent (11..=12)
    slot!(c"LeaveGameButton", c"GameStagingCtrl.cpp", 33, Button, -1, S_LEAVE as i32),
    slot!(c"StartGameButton", c"GameStagingCtrl.cpp", 34, Button, -1, S_START as i32),
    // LobbyOptionsCtrl::InitComponent, both tabs (13..=26)
    slot!(c"ChatSoundEffectsCheck", c"LobbyOptionsCtrl.cpp", 95, Button, 2, O_CHAT_SFX as i32),
    slot!(c"ConnectionSpeedCombo", c"LobbyOptionsCtrl.cpp", 96, Combo, -1, O_SPEED as i32),
    slot!(c"CrossPromotionCheck", c"LobbyOptionsCtrl.cpp", 97, Button, 3, O_CROSS as i32),
    slot!(c"FirewalledIcon", c"LobbyOptionsCtrl.cpp", 98, Image, -1, O_FIREWALL as i32),
    slot!(c"LobbyMusicCheck", c"LobbyOptionsCtrl.cpp", 99, Button, 5, O_MUSIC as i32),
    slot!(c"LobbySoundEffectsCheck", c"LobbyOptionsCtrl.cpp", 100, Button, 6, O_LOBBY_SFX as i32),
    slot!(c"LocalAddressDetailLabel", c"LobbyOptionsCtrl.cpp", 101, Label, -1, O_LOCAL_ADDR as i32),
    slot!(c"PublicAddressDetailLabel", c"LobbyOptionsCtrl.cpp", 102, Label, -1, O_PUBLIC_ADDR as i32),
    slot!(c"ResetFiltersButton", c"LobbyOptionsCtrl.cpp", 103, Button, 8, -1),
    slot!(c"RestoreDefaultsButton", c"LobbyOptionsCtrl.cpp", 104, Button, 9, -1),
    slot!(c"ShowConfirmationsCheck", c"LobbyOptionsCtrl.cpp", 105, Button, 10, O_CONFIRM as i32),
    slot!(c"ShowGamesInProgressCheck", c"LobbyOptionsCtrl.cpp", 106, Button, 11, O_IN_PROGRESS as i32),
    slot!(c"StagingSoundEffectsCheck", c"LobbyOptionsCtrl.cpp", 107, Button, 12, O_STAGING_SFX as i32),
    slot!(c"TabSelectCombo", c"LobbyOptionsCtrl.cpp", 108, Combo, -1, O_TAB as i32),
    // the LAN tab's (27..=29)
    slot!(c"LANBroadcastDetailLabel", c"LobbyOptionsCtrl.cpp", 112, Label, -1, O_BROADCAST as i32),
    slot!(c"RemoveLanNameButton", c"LobbyOptionsCtrl.cpp", 113, Button, 7, -1),
    slot!(c"UserNameCombo", c"LobbyOptionsCtrl.cpp", 114, Combo, -1, O_USER_NAME as i32),
    // the Internet tab's (30..=33)
    slot!(c"AnonymousToFriendsCheck", c"LobbyOptionsCtrl.cpp", 118, Button, 0x30d56, O_ANONYMOUS as i32),
    slot!(c"AutoPingGamesCheck", c"LobbyOptionsCtrl.cpp", 119, Button, 1, O_AUTO_PING as i32),
    slot!(c"DoAutoAFKCheck", c"LobbyOptionsCtrl.cpp", 120, Button, 4, O_AUTO_AFK as i32),
    slot!(c"FilterDirtyWordsCheck", c"LobbyOptionsCtrl.cpp", 121, Button, 0x30d55, O_DIRTY_WORDS as i32),
    // GameUserListCtrl::InitComponent (34..=35; the Kick button is optional: no report)
    slot!(c"UserList", c"GameUserListCtrl.cpp", 159, MultiList, -1, U_LIST as i32),
    slot!(c"KickButton", c"GameUserListCtrl.cpp", 182, Button, -1, U_KICK as i32),
    // EELobbyGameOptionsCtrl::InitComponent (36..=54; each option's type checked and its id set after its look-up)
    slot!(c"CampaignInfoLabel", c"EELobbyGameOptionsCtrl.cpp", 74, Label, -1, G_LABEL as i32),
    slot!(c"CampaignInfoBox1", c"EELobbyGameOptionsCtrl.cpp", 75, Container, -1, G_BOX1 as i32),
    slot!(c"CampaignInfoBox2", c"EELobbyGameOptionsCtrl.cpp", 76, Container, -1, G_BOX2 as i32),
    slot!(c"GameTypeCombo", c"EELobbyGameOptionsCtrl.cpp", 80, Component, -1, G_OPTION as i32),
    slot!(c"MapSizeCombo", c"EELobbyGameOptionsCtrl.cpp", 80, Component, -1, G_OPTION as i32 + 1),
    slot!(c"MapTypeCombo", c"EELobbyGameOptionsCtrl.cpp", 80, Component, -1, G_OPTION as i32 + 2),
    slot!(c"ResourcesCombo", c"EELobbyGameOptionsCtrl.cpp", 80, Component, -1, G_OPTION as i32 + 3),
    slot!(c"StartingEpochCombo", c"EELobbyGameOptionsCtrl.cpp", 80, Component, -1, G_OPTION as i32 + 4),
    slot!(c"EndingEpochCombo", c"EELobbyGameOptionsCtrl.cpp", 80, Component, -1, G_OPTION as i32 + 5),
    slot!(c"GameVariantCombo", c"EELobbyGameOptionsCtrl.cpp", 80, Component, -1, G_OPTION as i32 + 6),
    slot!(c"DifficultyLevelCombo", c"EELobbyGameOptionsCtrl.cpp", 80, Component, -1, G_OPTION as i32 + 7),
    slot!(c"GameSpeedCombo", c"EELobbyGameOptionsCtrl.cpp", 80, Component, -1, G_OPTION as i32 + 8),
    slot!(c"GameUnitLimitCombo", c"EELobbyGameOptionsCtrl.cpp", 80, Component, -1, G_OPTION as i32 + 9),
    slot!(c"WondersCombo", c"EELobbyGameOptionsCtrl.cpp", 80, Component, -1, G_OPTION as i32 + 10),
    slot!(c"RevealMapCheck", c"EELobbyGameOptionsCtrl.cpp", 80, Component, -1, G_OPTION as i32 + 11),
    slot!(c"UseCustomCivsCheck", c"EELobbyGameOptionsCtrl.cpp", 80, Component, -1, G_OPTION as i32 + 12),
    slot!(c"LockTeamsCheck", c"EELobbyGameOptionsCtrl.cpp", 80, Component, -1, G_OPTION as i32 + 13),
    slot!(c"LockSpeedCheck", c"EELobbyGameOptionsCtrl.cpp", 80, Component, -1, G_OPTION as i32 + 14),
    slot!(c"VictoryCheck", c"EELobbyGameOptionsCtrl.cpp", 80, Component, -1, G_OPTION as i32 + 15),
    // EELobbyPlayerOptionsCtrl::InitComponent (55..=102): each slot's name, team, colour, citizen, observe and
    // transfer controls, ids slot * 6 + column + 1
    slot!(c"UserNameCombo1", c"EELobbyPlayerOptionsCtrl.cpp", 50, Combo, 1, P_NAME as i32),
    slot!(c"TeamCombo1", c"EELobbyPlayerOptionsCtrl.cpp", 54, Combo, 2, P_TEAM as i32),
    slot!(c"ColorButton1", c"EELobbyPlayerOptionsCtrl.cpp", 58, ColorButton, 3, P_COLOR as i32),
    slot!(c"CitizenCombo1", c"EELobbyPlayerOptionsCtrl.cpp", 62, Combo, 4, P_CITIZEN as i32),
    slot!(c"ObserveCheck1", c"EELobbyPlayerOptionsCtrl.cpp", 66, Button, 5, P_OBSERVE as i32),
    slot!(c"FileTransferLabel1", c"EELobbyPlayerOptionsCtrl.cpp", 70, Label, 6, P_TRANSFER as i32),
    slot!(c"UserNameCombo2", c"EELobbyPlayerOptionsCtrl.cpp", 50, Combo, 7, (P_NAME + 1) as i32),
    slot!(c"TeamCombo2", c"EELobbyPlayerOptionsCtrl.cpp", 54, Combo, 8, (P_TEAM + 1) as i32),
    slot!(c"ColorButton2", c"EELobbyPlayerOptionsCtrl.cpp", 58, ColorButton, 9, (P_COLOR + 1) as i32),
    slot!(c"CitizenCombo2", c"EELobbyPlayerOptionsCtrl.cpp", 62, Combo, 10, (P_CITIZEN + 1) as i32),
    slot!(c"ObserveCheck2", c"EELobbyPlayerOptionsCtrl.cpp", 66, Button, 11, (P_OBSERVE + 1) as i32),
    slot!(c"FileTransferLabel2", c"EELobbyPlayerOptionsCtrl.cpp", 70, Label, 12, (P_TRANSFER + 1) as i32),
    slot!(c"UserNameCombo3", c"EELobbyPlayerOptionsCtrl.cpp", 50, Combo, 13, (P_NAME + 2) as i32),
    slot!(c"TeamCombo3", c"EELobbyPlayerOptionsCtrl.cpp", 54, Combo, 14, (P_TEAM + 2) as i32),
    slot!(c"ColorButton3", c"EELobbyPlayerOptionsCtrl.cpp", 58, ColorButton, 15, (P_COLOR + 2) as i32),
    slot!(c"CitizenCombo3", c"EELobbyPlayerOptionsCtrl.cpp", 62, Combo, 16, (P_CITIZEN + 2) as i32),
    slot!(c"ObserveCheck3", c"EELobbyPlayerOptionsCtrl.cpp", 66, Button, 17, (P_OBSERVE + 2) as i32),
    slot!(c"FileTransferLabel3", c"EELobbyPlayerOptionsCtrl.cpp", 70, Label, 18, (P_TRANSFER + 2) as i32),
    slot!(c"UserNameCombo4", c"EELobbyPlayerOptionsCtrl.cpp", 50, Combo, 19, (P_NAME + 3) as i32),
    slot!(c"TeamCombo4", c"EELobbyPlayerOptionsCtrl.cpp", 54, Combo, 20, (P_TEAM + 3) as i32),
    slot!(c"ColorButton4", c"EELobbyPlayerOptionsCtrl.cpp", 58, ColorButton, 21, (P_COLOR + 3) as i32),
    slot!(c"CitizenCombo4", c"EELobbyPlayerOptionsCtrl.cpp", 62, Combo, 22, (P_CITIZEN + 3) as i32),
    slot!(c"ObserveCheck4", c"EELobbyPlayerOptionsCtrl.cpp", 66, Button, 23, (P_OBSERVE + 3) as i32),
    slot!(c"FileTransferLabel4", c"EELobbyPlayerOptionsCtrl.cpp", 70, Label, 24, (P_TRANSFER + 3) as i32),
    slot!(c"UserNameCombo5", c"EELobbyPlayerOptionsCtrl.cpp", 50, Combo, 25, (P_NAME + 4) as i32),
    slot!(c"TeamCombo5", c"EELobbyPlayerOptionsCtrl.cpp", 54, Combo, 26, (P_TEAM + 4) as i32),
    slot!(c"ColorButton5", c"EELobbyPlayerOptionsCtrl.cpp", 58, ColorButton, 27, (P_COLOR + 4) as i32),
    slot!(c"CitizenCombo5", c"EELobbyPlayerOptionsCtrl.cpp", 62, Combo, 28, (P_CITIZEN + 4) as i32),
    slot!(c"ObserveCheck5", c"EELobbyPlayerOptionsCtrl.cpp", 66, Button, 29, (P_OBSERVE + 4) as i32),
    slot!(c"FileTransferLabel5", c"EELobbyPlayerOptionsCtrl.cpp", 70, Label, 30, (P_TRANSFER + 4) as i32),
    slot!(c"UserNameCombo6", c"EELobbyPlayerOptionsCtrl.cpp", 50, Combo, 31, (P_NAME + 5) as i32),
    slot!(c"TeamCombo6", c"EELobbyPlayerOptionsCtrl.cpp", 54, Combo, 32, (P_TEAM + 5) as i32),
    slot!(c"ColorButton6", c"EELobbyPlayerOptionsCtrl.cpp", 58, ColorButton, 33, (P_COLOR + 5) as i32),
    slot!(c"CitizenCombo6", c"EELobbyPlayerOptionsCtrl.cpp", 62, Combo, 34, (P_CITIZEN + 5) as i32),
    slot!(c"ObserveCheck6", c"EELobbyPlayerOptionsCtrl.cpp", 66, Button, 35, (P_OBSERVE + 5) as i32),
    slot!(c"FileTransferLabel6", c"EELobbyPlayerOptionsCtrl.cpp", 70, Label, 36, (P_TRANSFER + 5) as i32),
    slot!(c"UserNameCombo7", c"EELobbyPlayerOptionsCtrl.cpp", 50, Combo, 37, (P_NAME + 6) as i32),
    slot!(c"TeamCombo7", c"EELobbyPlayerOptionsCtrl.cpp", 54, Combo, 38, (P_TEAM + 6) as i32),
    slot!(c"ColorButton7", c"EELobbyPlayerOptionsCtrl.cpp", 58, ColorButton, 39, (P_COLOR + 6) as i32),
    slot!(c"CitizenCombo7", c"EELobbyPlayerOptionsCtrl.cpp", 62, Combo, 40, (P_CITIZEN + 6) as i32),
    slot!(c"ObserveCheck7", c"EELobbyPlayerOptionsCtrl.cpp", 66, Button, 41, (P_OBSERVE + 6) as i32),
    slot!(c"FileTransferLabel7", c"EELobbyPlayerOptionsCtrl.cpp", 70, Label, 42, (P_TRANSFER + 6) as i32),
    slot!(c"UserNameCombo8", c"EELobbyPlayerOptionsCtrl.cpp", 50, Combo, 43, (P_NAME + 7) as i32),
    slot!(c"TeamCombo8", c"EELobbyPlayerOptionsCtrl.cpp", 54, Combo, 44, (P_TEAM + 7) as i32),
    slot!(c"ColorButton8", c"EELobbyPlayerOptionsCtrl.cpp", 58, ColorButton, 45, (P_COLOR + 7) as i32),
    slot!(c"CitizenCombo8", c"EELobbyPlayerOptionsCtrl.cpp", 62, Combo, 46, (P_CITIZEN + 7) as i32),
    slot!(c"ObserveCheck8", c"EELobbyPlayerOptionsCtrl.cpp", 66, Button, 47, (P_OBSERVE + 7) as i32),
    slot!(c"FileTransferLabel8", c"EELobbyPlayerOptionsCtrl.cpp", 70, Label, 48, (P_TRANSFER + 7) as i32),
    // ServerListCtrl::InitComponent (103..=107; the bold font is made after the list, the Remove button optional)
    slot!(c"ServerList", c"ServerListCtrl.cpp", 647, MultiList, -1, crate::srvlistctl::L_LIST as i32),
    slot!(c"JoinGameButton", c"ServerListCtrl.cpp", 650, Button, -1, crate::srvlistctl::L_JOIN as i32),
    slot!(c"RefreshListButton", c"ServerListCtrl.cpp", 651, Button, -1, crate::srvlistctl::L_REFRESH as i32),
    slot!(c"ServerListSummary", c"ServerListCtrl.cpp", 652, Label, -1, crate::srvlistctl::L_SUMMARY as i32),
    slot!(c"RemoveButton", c"ServerListCtrl.cpp", 653, Component, -1, crate::srvlistctl::L_REMOVE as i32),
    // ChatCtrl::InitComponent (108..=110)
    slot!(c"ChatInput", c"ChatCtrl.cpp", 160, Input, -1, crate::chatctl::C_INPUT as i32),
    slot!(c"SendButton", c"ChatCtrl.cpp", 161, Button, -1, crate::chatctl::C_SEND as i32),
    slot!(c"ChatOutput", c"ChatCtrl.cpp", 162, TextArea, -1, crate::chatctl::C_OUTPUT as i32),
    // RoomCtrl (111..=113)
    slot!(c"RoomList", c"RoomCtrl.cpp", 168, TreeArea, -1, 0),
    slot!(c"JoinButton", c"RoomCtrl.cpp", 169, Button, -1, 1),
    slot!(c"CreateButton", c"RoomCtrl.cpp", 170, Button, -1, 2),
    // FriendsListCtrl (114..=116)
    slot!(c"FriendsList", c"FriendsListCtrl.cpp", 109, ListArea, -1, 0),
    slot!(c"RemoveButton", c"FriendsListCtrl.cpp", 110, Button, -1, 1),
    slot!(c"LocateButton", c"FriendsListCtrl.cpp", 111, Button, -1, 2),
    // UserListCtrl (117..=119)
    slot!(c"UserList", c"UserListCtrl.cpp", 194, ListArea, -1, 0),
    slot!(c"WhisperButton", c"UserListCtrl.cpp", 195, Button, -1, 3),
    slot!(c"IgnoreButton", c"UserListCtrl.cpp", 196, Button, -1, 4),
    // QuickPlayCtrl (120..=125)
    slot!(c"WelcomeLabel", c"QuickPlayCtrl.cpp", 29, Label, -1, 0),
    slot!(c"NoviceRadio", c"QuickPlayCtrl.cpp", 30, Button, -1, 1),
    slot!(c"IntermediateRadio", c"QuickPlayCtrl.cpp", 31, Button, -1, 2),
    slot!(c"AdvancedRadio", c"QuickPlayCtrl.cpp", 32, Button, -1, 3),
    slot!(c"ExpertRadio", c"QuickPlayCtrl.cpp", 33, Button, -1, 4),
    slot!(c"PlayButton", c"QuickPlayCtrl.cpp", 34, Button, -1, 5),
    // LoginCtrl (126..=129)
    slot!(c"UserNameCombo", c"LoginCtrl.cpp", 31, Combo, -1, 0),
    slot!(c"PasswordInput", c"LoginCtrl.cpp", 32, Input, -1, 1),
    slot!(c"RememberPasswordCheck", c"LoginCtrl.cpp", 33, Button, -1, 2),
    slot!(c"LoginButton", c"LoginCtrl.cpp", 34, Button, -1, 3),
    // CreateAccountCtrl (130..=143)
    slot!(c"UserNameInput", c"CreateAccountCtrl.cpp", 30, Input, -1, 0),
    slot!(c"UserNameDesc", c"CreateAccountCtrl.cpp", 31, Label, -1, 8),
    slot!(c"PasswordInput", c"CreateAccountCtrl.cpp", 32, Input, -1, 1),
    slot!(c"PasswordDesc", c"CreateAccountCtrl.cpp", 33, Label, -1, 9),
    slot!(c"ConfirmPasswordInput", c"CreateAccountCtrl.cpp", 34, Input, -1, 2),
    slot!(c"ConfirmPasswordDesc", c"CreateAccountCtrl.cpp", 35, Label, -1, 10),
    slot!(c"EmailInput", c"CreateAccountCtrl.cpp", 36, Input, -1, 3),
    slot!(c"EmailDesc", c"CreateAccountCtrl.cpp", 37, Label, -1, 11),
    slot!(c"MonthInput", c"CreateAccountCtrl.cpp", 38, Input, -1, 4),
    slot!(c"DayInput", c"CreateAccountCtrl.cpp", 39, Input, -1, 5),
    slot!(c"YearInput", c"CreateAccountCtrl.cpp", 40, Input, -1, 6),
    slot!(c"BirthdateDesc", c"CreateAccountCtrl.cpp", 41, Label, -1, 12),
    slot!(c"NewsletterCheck", c"CreateAccountCtrl.cpp", 42, Button, -1, 7),
    slot!(c"CreateButton", c"CreateAccountCtrl.cpp", 43, Button, -1, 13),
    // UpdateAccountCtrl (144..=156)
    slot!(c"UserNameInput", c"UpdateAccountCtrl.cpp", 39, Input, -1, 0),
    slot!(c"OldPasswordInput", c"UpdateAccountCtrl.cpp", 40, Input, -1, 1),
    slot!(c"PasswordInput", c"UpdateAccountCtrl.cpp", 41, Input, -1, 2),
    slot!(c"ConfirmPasswordInput", c"UpdateAccountCtrl.cpp", 42, Input, -1, 3),
    slot!(c"EmailInput", c"UpdateAccountCtrl.cpp", 43, Input, -1, 4),
    slot!(c"NewsletterCheck", c"UpdateAccountCtrl.cpp", 44, Button, -1, 5),
    slot!(c"UserNameDesc", c"UpdateAccountCtrl.cpp", 45, Label, -1, 6),
    slot!(c"OldPasswordDesc", c"UpdateAccountCtrl.cpp", 46, Label, -1, 7),
    slot!(c"PasswordDesc", c"UpdateAccountCtrl.cpp", 47, Label, -1, 8),
    slot!(c"ConfirmPasswordDesc", c"UpdateAccountCtrl.cpp", 48, Label, -1, 9),
    slot!(c"EmailDesc", c"UpdateAccountCtrl.cpp", 49, Label, -1, 10),
    slot!(c"QueryButton", c"UpdateAccountCtrl.cpp", 50, Button, -1, 11),
    slot!(c"UpdateButton", c"UpdateAccountCtrl.cpp", 51, Button, -1, 12),
    // LostPasswordCtrl (157..=158), LostUserNameCtrl (159..=160)
    slot!(c"UserNameCombo", c"LostPasswordCtrl.cpp", 27, Combo, -1, 0),
    slot!(c"RetrieveButton", c"LostPasswordCtrl.cpp", 28, Button, -1, 1),
    slot!(c"EmailInput", c"LostUserNameCtrl.cpp", 27, Input, -1, 0),
    slot!(c"RetrieveButton", c"LostUserNameCtrl.cpp", 28, Button, -1, 1),
    // LoginHelpCtrl (161), LoginStatusCtrl (162), InitFailedCtrl (163..=165), MOTDCtrl (166)
    slot!(c"Browser", c"LoginHelpCtrl.cpp", 40, Browser, -1, 0),
    slot!(c"StatusLabel", c"LoginStatusCtrl.cpp", 26, Label, -1, 0),
    slot!(c"RetryButton", c"InitFailedCtrl.cpp", 26, Button, -1, 1),
    slot!(c"IgnoreButton", c"InitFailedCtrl.cpp", 27, Button, -1, 2),
    slot!(c"DescLabel", c"InitFailedCtrl.cpp", 28, Label, -1, 0),
    slot!(c"Browser", c"MOTDCtrl.cpp", 36, Browser, -1, 0),
    // LoginScreen (167..=172)
    slot!(c"MOTDCtrl", c"LoginScreen.cpp", 26, Component, -1, 1),
    slot!(c"CreateAccountCtrl", c"LoginScreen.cpp", 27, Component, -1, 2),
    slot!(c"LostUserNameCtrl", c"LoginScreen.cpp", 28, Component, -1, 3),
    slot!(c"LostPasswordCtrl", c"LoginScreen.cpp", 29, Component, -1, 4),
    slot!(c"LoginHelpCtrl", c"LoginScreen.cpp", 30, Component, -1, 5),
    slot!(c"RightPanel", c"LoginScreen.cpp", 31, ScreenContainer, -1, 0),
    // DirectConnectCtrl (173..=175), RoomLabelCtrl (176)
    slot!(c"ConnectionList", c"DirectConnectCtrl.cpp", 34, ServerListCtrl, -1, 0),
    slot!(c"AddButton", c"DirectConnectCtrl.cpp", 35, Button, -1, 1),
    slot!(c"RemoveButton", c"DirectConnectCtrl.cpp", 36, Button, -1, 2),
    slot!(c"Label", c"RoomLabelCtrl.cpp", 29, Label, -1, 0),
];

/// The number of members each kind holds (released in reverse by the destructor).
fn member_count(kind: Kind) -> usize {
    match kind {
        Kind::Browser => 11,
        Kind::Staging | Kind::UserList => 2,
        Kind::LanOptions | Kind::NetOptions => 18,
        Kind::GameOptions => 19,
        Kind::PlayerOptions => MEMBERS,
        Kind::ServerList => 6,
        Kind::Chat => 5,
        k => crate::netctl::net_member_count(k),
    }
}

/// LobbyOptionsCtrl's `GetConnectionSpeed`: the speed value (5, 15, .., 55) of a combo box entry.
pub fn connection_speed(selection: i32) -> i32 {
    match selection {
        i32::MIN..=0 => 5,
        1 => 15,
        2 => 25,
        3 => 35,
        4 => 45,
        _ => 55,
    }
}

/// LobbyOptionsCtrl's `GetConnectionSpeedIndex`: the combo box entry of a saved speed.
pub fn connection_speed_index(speed: i32) -> i32 {
    let mut i = 1;
    while i < 6 {
        if connection_speed(i) > speed {
            break;
        }
        i += 1;
    }
    i - 1
}

/// One control's Rust half.
pub struct EeWonLobbyCtl {
    pub(crate) ctl: *mut c_void,
    host: EeWonLobbyCtlHostFn,
    /// the model's ops answered by its Rust objects (modelobj.rs; the game's), not the host
    pub(crate) model: Cell<bool>,
    pub(crate) kind: Kind,
    /// the `SmartPtr` members, in the original's member order (see the `B_`, `S_`, `O_` and `U_` indices)
    pub(crate) members: [Cell<isize>; MEMBERS],
    /// the round 13 kinds' members beyond their controls
    pub(crate) ext: Ext,
    /// `mRoomSpecFlags` (GameStagingCtrl, GameUserListCtrl: 2 from the constructor)
    pub(crate) room_flags: Cell<i32>,
    /// `mInGame`
    in_game: Cell<bool>,
    /// GameStagingCtrl's `mLastReady`
    last_ready: Cell<bool>,
    /// GameBrowserCtrl's `mIsReset` (true from the constructor)
    is_reset: Cell<bool>,
    /// EELobbyPlayerOptionsCtrl's `mMySlot` (-1 from the constructor)
    my_slot: Cell<i32>,
    /// EELobbyPlayerOptionsCtrl's `mIsHost`
    is_host: Cell<bool>,
}

impl EeWonLobbyCtl {
    pub fn new(ctl: *mut c_void, kind: Kind, host: EeWonLobbyCtlHostFn) -> Self {
        Self {
            ctl,
            model: Cell::new(false),
            host,
            kind,
            members: std::array::from_fn(|_| Cell::new(0)),
            room_flags: Cell::new(
                if matches!(kind, Kind::Staging | Kind::UserList | Kind::GameOptions | Kind::PlayerOptions) {
                    2
                } else {
                    0
                },
            ),
            in_game: Cell::new(false),
            last_ready: Cell::new(false),
            is_reset: Cell::new(kind == Kind::Browser),
            my_slot: Cell::new(-1),
            is_host: Cell::new(false),
            ext: Ext::new(kind),
        }
    }

    pub(crate) fn op(&self, op: Op, a: isize, b: isize, c: isize) -> isize {
        if self.model.get() {
            if let Some(r) = crate::modelobj::ctl_op(op, a, b, c) {
                return r;
            }
            if let Some(r) = crate::persistobj::game_ctl_op(op, a, b) {
                return r;
            }
            if let Some(r) = crate::rows::ctl_op(op, a, b, c) {
                return r;
            }
        }
        match self.host {
            // SAFETY: the host is the C++ control's (or a test's), called with the control it was given for
            Some(f) => unsafe { f(self.ctl, op, a, b, c) },
            None => 0,
        }
    }

    pub(crate) fn ask(&self, op: Op, a: isize) -> bool {
        self.op(op, a, 0, 0) != 0
    }

    pub(crate) fn ask_with(&self, op: Op, a: isize, b: isize) -> bool {
        self.op(op, a, b, 0) != 0
    }

    /// A call on the C++ game `game`'s Rust object (its `mRust`), as the C++ forwarder made it.
    fn game(&self, game: isize, call: GC, a: isize, b: isize) -> isize {
        self.game3(game, call, a, b, 0)
    }

    fn game3(&self, game: isize, call: GC, a: isize, b: isize, c: isize) -> isize {
        let handle = self.op(Op::GameRust, game, 0, 0);
        game_call(handle, call, a, b, c)
    }

    pub(crate) fn m(&self, i: usize) -> isize {
        self.members[i].get()
    }

    /// A `SmartPtr`'s assignment (`ConstSmartPtr::operator=`): nothing for the same pointer, else the old one
    /// released, the new one referenced.
    pub(crate) fn assign(&self, member: &Cell<isize>, new: isize) {
        let old = member.get();
        if old == new {
            return;
        }
        if old != 0 {
            self.op(Op::Release, old, 0, 0);
        }
        member.set(new);
        if new != 0 {
            self.op(Op::AddRef, new, 0, 0);
        }
    }

    /// `SafeGetComponent(config, member, name, [id,] file, line)`: the look-up, the assignment, the control id.
    pub(crate) fn find(&self, config: isize, slot: usize, local: &Cell<isize>) {
        let s = &SLOTS[slot];
        let found = self.op(Op::Find, config, slot as isize, 0);
        let member = if s.member < 0 { local } else { &self.members[s.member as usize] };
        self.assign(member, found);
        if s.control_id >= 0 {
            self.op(Op::SetControlId, member.get(), s.control_id as isize, 0);
        }
    }

    pub(crate) fn set_enabled(&self, control: isize, enabled: bool) {
        self.op(Op::SetEnabled, control, enabled as isize, 0);
    }

    fn text(&self, button: isize, s: S) {
        self.op(Op::ButtonSetText, button, s as isize, 0);
    }

    fn init(&self, config: isize) {
        let local = Cell::new(0);
        match self.kind {
            Kind::Browser => {
                for slot in 0..=10 {
                    self.find(config, slot, &local);
                }
                for (member, id) in [
                    (B_BACK, 2),
                    (B_FORWARD, 3),
                    (B_STOP, 5),
                    (B_REFRESH, 1),
                    (B_HOME, 4),
                    (B_FIND_NEXT, 7),
                    (B_FIND_PREV, 8),
                ] {
                    self.op(Op::SetControlId, self.m(member), id, 0);
                }
                self.enable_buttons();
            }
            Kind::Staging => {
                self.find(config, 11, &local);
                self.find(config, 12, &local);
                self.set_enabled(self.m(S_START), false);
                self.op(Op::SubscribeBroadcast, 1, 0, 0);
            }
            Kind::LanOptions | Kind::NetOptions => {
                for slot in 13..=26 {
                    self.find(config, slot, &local);
                }
                let extra = if self.kind == Kind::LanOptions { 27..=29 } else { 30..=33 };
                for slot in extra {
                    self.find(config, slot, &local);
                }
                self.op(Op::LabelSetLocalAddress, self.m(O_LOCAL_ADDR), 0, 0);
                self.op(Op::SubscribeBroadcast, 1, 0, 0);
            }
            Kind::UserList => {
                self.find(config, 34, &local);
                let kick = self.op(Op::FindOptional, config, 35, 0);
                self.assign(&self.members[U_KICK], kick);
                if self.m(U_KICK) != 0 {
                    self.set_enabled(self.m(U_KICK), false);
                }
                self.op(Op::SubscribeBroadcast, 1, 0, 0);
            }
            Kind::GameOptions => self.game_options_init(config, &local),
            Kind::PlayerOptions => self.player_options_init(config, &local),
            Kind::ServerList => self.srv_init(config, &local),
            Kind::Chat => self.chat_init(config, &local),
            _ => self.net_init(config, &local),
        }
        // the local SmartPtr<Button> (LobbyOptionsCtrl's reset, restore and remove buttons in turn)
        let l = local.replace(0);
        if l != 0 {
            self.op(Op::Release, l, 0, 0);
        }
    }

    // ---- GameBrowserCtrl

    fn enable_buttons(&self) {
        let browser = self.m(B_BROWSER);
        let v = self.op(Op::BrowserBackEnabled, browser, 0, 0);
        self.op(Op::SetEnabled, self.m(B_BACK), v, 0);
        let v = self.op(Op::BrowserForwardEnabled, browser, 0, 0);
        self.op(Op::SetEnabled, self.m(B_FORWARD), v, 0);
        let v = self.op(Op::BrowserStopEnabled, browser, 0, 0);
        self.op(Op::SetEnabled, self.m(B_STOP), v, 0);
    }

    fn do_find(&self, next: bool) {
        // the original's arguments, right to left: the check box first
        let case = self.ask(Op::ButtonIsChecked, self.m(B_CASE));
        let flags = (next as isize) | ((case as isize) << 1);
        self.op(Op::BrowserFind, self.m(B_BROWSER), self.m(B_FIND_INPUT), flags);
    }

    fn reset_browser(&self) {
        self.op(Op::BrowserReset, self.m(B_BROWSER), 0, 0);
        self.is_reset.set(true);
    }

    fn go_home(&self) {
        self.op(Op::BrowserGoHome, self.m(B_BROWSER), 0, 0);
    }

    fn browser_event(&self, event: isize, kind: Ev) {
        match kind {
            Ev::BrowserButtonChange => self.enable_buttons(),
            Ev::BrowserStatusChange => {
                self.op(Op::LabelSetBrowserStatus, self.m(B_STATUS), self.m(B_BROWSER), 0);
            }
            Ev::ButtonPressed => {
                let browser = self.m(B_BROWSER);
                match self.op(Op::EventControlId, event, 0, 0) {
                    2 => self.op(Op::BrowserBack, browser, 0, 0),
                    3 => self.op(Op::BrowserForward, browser, 0, 0),
                    5 => self.op(Op::BrowserStop, browser, 0, 0),
                    1 => self.op(Op::BrowserRefresh, browser, 0, 0),
                    4 => {
                        self.go_home();
                        0
                    }
                    7 => {
                        self.do_find(true);
                        0
                    }
                    8 => {
                        self.do_find(false);
                        0
                    }
                    _ => 0,
                };
            }
            // every other event is dropped (no base call)
            _ => {}
        }
    }

    fn paint(&self, graphics: isize) {
        if self.is_reset.get() {
            self.is_reset.set(false);
            self.go_home();
        }
        self.op(Op::BasePaint, graphics, 0, 0);
    }

    // ---- GameStagingCtrl

    fn ready_text(ready: bool) -> S {
        if ready {
            S::NotReady
        } else {
            S::Ready
        }
    }

    fn update_start_button(&self) {
        if !self.in_game.get() {
            return;
        }
        let game = self.op(Op::MyGame, 0, 0, 0);
        if game == 0 {
            return;
        }
        let enabled = if self.ask(Op::GameIsHost, game) {
            self.ask(Op::GameCanStart, game)
        } else {
            if self.last_ready.get() != self.ask(Op::IAmReady, 0) {
                self.last_ready.set(self.ask(Op::IAmReady, 0));
                self.text(self.m(S_START), Self::ready_text(self.last_ready.get()));
            }
            if !self.last_ready.get() {
                self.ask(Op::GameCanReady, game)
            } else {
                true
            }
        };
        self.set_enabled(self.m(S_START), enabled);
    }

    fn staging_joined(&self) {
        let game = self.op(Op::MyGame, 0, 0, 0);
        if game == 0 || self.op(Op::GameMatchesSpec, game, self.room_flags.get() as isize, 0) == 0 {
            return;
        }
        self.in_game.set(true);
        if self.ask(Op::GameIsHost, game) {
            self.text(self.m(S_LEAVE), S::DissolveGame);
            self.text(self.m(S_START), S::StartGame);
        } else {
            self.text(self.m(S_LEAVE), S::LeaveGame);
            self.last_ready.set(self.ask(Op::IAmReady, 0));
            self.text(self.m(S_START), Self::ready_text(self.last_ready.get()));
        }
        self.update_start_button();
    }

    fn staging_event(&self, event: isize, kind: Ev) {
        match kind {
            Ev::JoinedGame => self.staging_joined(),
            Ev::LeftGame => self.in_game.set(false),
            Ev::SetRoomSpecFlags => self.add_room_flags(event),
            Ev::ReadyChanged => self.update_start_button(),
            _ => {}
        }
        self.op(Op::BaseEvent, event, 0, 0);
    }

    fn add_room_flags(&self, event: isize) {
        let flags = self.op(Op::EventFlags, event, 0, 0) as i32;
        self.room_flags.set(self.room_flags.get() | flags);
    }

    // ---- LobbyOptionsCtrl

    fn is_lan(&self) -> bool {
        self.kind == Kind::LanOptions
    }

    pub(crate) fn pref(&self, p: EeWonLobbyCtlPref) -> isize {
        self.op(Op::PrefGet, p as isize, 0, 0)
    }

    fn set_pref(&self, p: EeWonLobbyCtlPref, v: isize) {
        self.op(Op::PrefSet, p as isize, v, 0);
    }

    fn options_event(&self, event: isize, kind: Ev, component: isize) {
        match kind {
            Ev::ComboSelChanged => {
                if component == self.m(O_TAB) {
                    self.op(Op::SaveTabSelect, self.m(O_TAB), 0, 0);
                    return;
                } else if component == self.m(O_SPEED) {
                    let sel = self.op(Op::ComboGetSel, self.m(O_SPEED), 0, 0) as i32;
                    self.set_pref(EeWonLobbyCtlPref::ConnectionSpeed, connection_speed(sel) as isize);
                    return;
                } else if component == self.m(O_USER_NAME) {
                    self.op(Op::SaveLanName, self.m(O_USER_NAME), 0, 0);
                    return;
                }
            }
            Ev::ButtonPressed => {
                if self.handle_button(component) {
                    return;
                }
            }
            Ev::SyncOptions => {
                if !self.ask(Op::OnLoginScreen, 0) && self.ask(Op::OnLanScreen, 0) == self.is_lan() {
                    self.refresh_options();
                }
                return;
            }
            Ev::LanOptions => {
                self.refresh_lan_options();
                return;
            }
            Ev::PublicAddress => {
                self.refresh_public_address();
                return;
            }
            _ => {}
        }
        self.op(Op::BaseEvent, event, 0, 0);
    }

    /// LobbyOptionsCtrl::HandleButton: a toggled option saved and applied; false for the cross promotion, the
    /// dirty-word filter, the anonymity and an unknown button, so the base sees them too.
    fn handle_button(&self, component: isize) -> bool {
        use EeWonLobbyCtlPref as P;
        if component == 0 {
            return false;
        }
        let button = self.op(Op::ButtonOf, component, 0, 0);
        if button == 0 {
            return false;
        }
        let checked = self.op(Op::ButtonIsChecked, button, 0, 0);
        match self.op(Op::ControlId, component, 0, 0) {
            3 => self.set_pref(P::CrossPromotion, checked),
            4 => {
                self.set_pref(P::DoAutoAfk, checked);
                return true;
            }
            1 => {
                self.set_pref(P::AutoPingGames, checked);
                return true;
            }
            11 => {
                self.set_pref(P::ShowGamesInProgress, checked);
                self.op(Op::BroadcastLobbyEvent, 100059, 1, 0);
                return true;
            }
            9 => {
                self.op(Op::RestoreDefaults, 0, 0, 0);
                self.op(Op::ResetNetworkAdapter, 0, 0, 0);
                self.refresh_options();
                return true;
            }
            8 => {
                self.op(Op::ResetFilters, 0, 0, 0);
                let screen = self.op(Op::CurrentScreen, 0, 0, 0);
                if screen != 0 {
                    self.op(Op::BroadcastDown, screen, 100034, 0);
                }
                return true;
            }
            5 => {
                self.set_pref(P::LobbyMusic, checked);
                if checked != 0 {
                    self.op(Op::MusicResume, 0, 0, 0);
                } else {
                    self.op(Op::MusicStop, 0, 0, 0);
                }
                return true;
            }
            6 => {
                self.set_pref(P::LobbySoundEffects, checked);
                self.op(Op::ClickSoundMute, (checked == 0) as isize, 0, 0);
                if checked == 0 {
                    self.op(Op::LogonSoundStop, 0, 0, 0);
                }
                return true;
            }
            2 => {
                self.set_pref(P::ChatSoundEffects, checked);
                return true;
            }
            12 => {
                self.set_pref(P::StagingSoundEffects, checked);
                return true;
            }
            10 => {
                self.set_pref(P::ShowConfirmations, checked);
                return true;
            }
            7 => {
                self.op(Op::RemoveLanName, 0, 0, 0);
                self.refresh_lan_options();
                return true;
            }
            0x30d55 => self.set_pref(P::FilterDirtyWords, checked),
            0x30d56 => self.set_pref(P::AnonymousToFriends, checked),
            _ => {}
        }
        false
    }

    fn refresh_lan_options(&self) {
        if !self.is_lan() {
            return;
        }
        self.op(Op::ComboClear, self.m(O_USER_NAME), 1, 0);
        let mut i = 0;
        while i < self.op(Op::LanNameCount, 0, 0, 0) {
            self.op(Op::ComboAddLanName, self.m(O_USER_NAME), i, 0);
            i += 1;
        }
        self.op(Op::ComboShowLanName, self.m(O_USER_NAME), 0, 0);
        let port = self.op(Op::LanBroadcastPort, 0, 0, 0);
        self.op(Op::LabelSetNumber, self.m(O_BROADCAST), port, 0);
    }

    fn check(&self, member: usize, p: EeWonLobbyCtlPref, notify: bool) {
        let v = self.pref(p);
        self.op(Op::ButtonSetCheck, self.m(member), v, notify as isize);
    }

    fn refresh_options(&self) {
        use EeWonLobbyCtlPref as P;
        if self.is_lan() {
            self.refresh_lan_options();
        } else {
            // the dirty-word and auto-AFK checks do not notify
            self.check(O_DIRTY_WORDS, P::FilterDirtyWords, false);
            self.check(O_AUTO_AFK, P::DoAutoAfk, false);
            self.check(O_ANONYMOUS, P::AnonymousToFriends, true);
            self.check(O_AUTO_PING, P::AutoPingGames, true);
        }
        self.check(O_CHAT_SFX, P::ChatSoundEffects, true);
        let speed = self.pref(P::ConnectionSpeed) as i32;
        self.op(Op::ComboSetSel, self.m(O_SPEED), connection_speed_index(speed) as isize, 0);
        self.op(Op::ComboShowTabSelect, self.m(O_TAB), 0, 0);
        self.check(O_MUSIC, P::LobbyMusic, true);
        self.check(O_LOBBY_SFX, P::LobbySoundEffects, true);
        self.check(O_CONFIRM, P::ShowConfirmations, true);
        self.check(O_CROSS, P::CrossPromotion, true);
        self.check(O_IN_PROGRESS, P::ShowGamesInProgress, true);
        self.check(O_STAGING_SFX, P::StagingSoundEffects, true);
    }

    fn show_icon(&self, image: EeWonLobbyCtlImage) {
        let icon = self.m(O_FIREWALL);
        self.op(Op::IconSetImage, icon, image as isize, 0);
        self.op(Op::Invalidate, icon, 0, 0);
        self.op(Op::SetVisible, icon, 1, 0);
    }

    fn refresh_public_address(&self) {
        if self.ask(Op::FirewallTestDone, 0) {
            if self.ask(Op::BehindFirewall, 0) {
                self.show_icon(EeWonLobbyCtlImage::Firewall);
            } else {
                self.op(Op::SetVisible, self.m(O_FIREWALL), 0, 0);
            }
        } else {
            self.show_icon(EeWonLobbyCtlImage::Searching);
        }
        let label = self.m(O_PUBLIC_ADDR);
        if !self.ask(Op::PublicAddressValid, 0) {
            let s = if !self.ask(Op::FirewallTestDone, 0) { S::CheckingAddress } else { S::AddressUnavailable };
            self.op(Op::LabelSetText, label, s as isize, 0);
        } else {
            self.op(Op::LabelSetPublicAddress, label, 0, 0);
        }
    }

    // ---- GameUserListCtrl

    /// GameUserListItem::UpdateIcon: the first match of ignored, blocked, muted, away, admin, moderator, captain.
    fn update_icon(&self, item: isize) {
        use EeWonLobbyCtlClientTest as T;
        use EeWonLobbyCtlImage as I;
        let image = [
            (T::IsIgnored, I::Ignored),
            (T::IsBlocked, I::Blocked),
            (T::IsMuted, I::Muted),
            (T::IsAway, I::Away),
            (T::IsAdmin, I::Admin),
            (T::IsModerator, I::Moderator),
            (T::IsCaptain, I::Captain),
        ]
        .into_iter()
        .find(|(t, _)| self.op(Op::ItemClientIs, item, *t as isize, 0) != 0)
        .map_or(I::Normal, |(_, i)| i);
        self.op(Op::ItemSetImage, item, image as isize, 0);
    }

    fn selected_client(&self) -> isize {
        let item = self.op(Op::ListGetSel, self.m(U_LIST), 0, 0);
        if item == 0 {
            0
        } else {
            self.op(Op::ItemClient, item, 0, 0)
        }
    }

    fn list_item(&self, client: isize) -> isize {
        let list = self.m(U_LIST);
        let mut i = 0;
        while i < self.op(Op::ListNumItems, list, 0, 0) as i32 {
            let item = self.op(Op::ListItem, list, i as isize, 0);
            let c = self.op(Op::ItemClient, item, 0, 0);
            if c != 0 && self.op(Op::ClientId, c, 0, 0) == self.op(Op::ClientId, client, 0, 0) {
                return item;
            }
            i += 1;
        }
        0
    }

    fn update_kick_button(&self) {
        let kick = self.m(U_KICK);
        if !self.in_game.get() || kick == 0 {
            return;
        }
        let game = self.op(Op::MyGame, 0, 0, 0);
        if game == 0 {
            return;
        }
        if self.ask(Op::GameIsHost, game) {
            let client = self.selected_client();
            let enabled = client != 0 && self.op(Op::ClientId, client, 0, 0) != self.op(Op::GameCaptainId, game, 0, 0);
            self.set_enabled(self.m(U_KICK), enabled);
        } else {
            self.set_enabled(kick, false);
        }
    }

    fn add_player(&self, client: isize) {
        // SmartPtr<GameUserListItem> item = new GameUserListItem(client)
        let item = self.op(Op::ItemNew, client, 0, 0);
        self.update_icon(item);
        self.op(Op::ListInsert, self.m(U_LIST), item, 0);
        self.op(Op::Release, item, 0, 0);
    }

    fn refresh_row(&self, item: isize) {
        self.update_icon(item);
        self.op(Op::ListRefresh, self.m(U_LIST), item, 0);
    }

    fn client_changed(&self, event: isize) {
        if !self.in_game.get() {
            return;
        }
        let spec = self.op(Op::EventRoomSpec, event, 0, 0) as i32;
        if self.room_flags.get() & spec != spec || !self.ask(Op::EventIsModify, event) {
            return;
        }
        let item = self.list_item(self.op(Op::ClientChangedClient, event, 0, 0));
        if item == 0 {
            return;
        }
        self.refresh_row(item);
        self.update_kick_button();
    }

    fn player_changed(&self, event: isize) {
        if !self.in_game.get() {
            return;
        }
        if self.ask(Op::EventIsAdd, event) {
            self.add_player(self.op(Op::PlayerChangedClient, event, 0, 0));
        } else if self.ask(Op::EventIsDelete, event) {
            let item = self.list_item(self.op(Op::PlayerChangedClient, event, 0, 0));
            if item != 0 {
                self.op(Op::ListRemove, self.m(U_LIST), item, 0);
            }
        } else {
            let item = self.list_item(self.op(Op::PlayerChangedClient, event, 0, 0));
            if item != 0 {
                self.refresh_row(item);
            }
        }
    }

    fn user_list_joined(&self) {
        let game = self.op(Op::MyGame, 0, 0, 0);
        if game == 0 || self.op(Op::GameMatchesSpec, game, self.room_flags.get() as isize, 0) == 0 {
            return;
        }
        let clients = self.op(Op::GameClients, game, 0, 0);
        if clients == 0 {
            return;
        }
        let kick = self.m(U_KICK);
        if kick != 0 {
            self.text(kick, if self.room_flags.get() & 4 != 0 { S::Ban } else { S::Kick });
        }
        let list = self.m(U_LIST);
        self.op(Op::ListBeginUpdate, list, 0, 0);
        self.op(Op::ListClear, list, 1, 0);
        let mut i = 0;
        while i < self.op(Op::ClientsCount, clients, 0, 0) as usize {
            let client = self.op(Op::ClientsAt, clients, i as isize, 0);
            i += 1;
            if !self.ask(Op::ClientHasPlayer, client) {
                continue;
            }
            self.add_player(client);
        }
        self.op(Op::ListEndUpdate, self.m(U_LIST), 0, 0);
        self.in_game.set(true);
        self.update_kick_button();
    }

    fn user_list_event(&self, event: isize, kind: Ev, component: isize) {
        match kind {
            Ev::ListSelChanged => return self.update_kick_button(),
            Ev::ButtonPressed => {
                if component == self.m(U_KICK) {
                    let client = self.selected_client();
                    if client != 0 {
                        self.op(Op::KickClient, client, (self.room_flags.get() & 4 != 0) as isize, 0);
                    }
                    return;
                }
            }
            Ev::ListItemClicked => {
                let client = self.selected_client();
                if client == 0 || !self.ask(Op::EventIsSelectedItem, event) {
                    return;
                }
                let button = self.op(Op::EventButton, event, 0, 0);
                let flags = self.room_flags.get() as isize;
                if button == 2 {
                    self.op(Op::DeliverClientAction, 18, flags, client);
                } else if button == 0 && self.ask(Op::EventDoubleClick, event) {
                    self.op(Op::BroadcastWhisper, client, flags, 0);
                }
                return;
            }
            Ev::ClientChanged => return self.client_changed(event),
            Ev::JoinedGame => return self.user_list_joined(),
            Ev::LeftGame => {
                if self.in_game.get() {
                    self.in_game.set(false);
                    self.op(Op::ListClear, self.m(U_LIST), 1, 0);
                }
                return;
            }
            Ev::PlayerChanged => return self.player_changed(event),
            Ev::SetRoomSpecFlags => return self.add_room_flags(event),
            _ => {}
        }
        self.op(Op::BaseEvent, event, 0, 0);
    }

    // ---- EELobbyGameOptionsCtrl (`in_game` is its `mIsActive`)

    fn option_control(&self, option: i32) -> isize {
        if (0..16).contains(&option) {
            self.m(G_OPTION + option as usize)
        } else {
            0
        }
    }

    fn option_combo(&self, option: i32) -> isize {
        if (0..16).contains(&option) && !OPTION_IS_CHECK[option as usize] {
            self.m(G_OPTION + option as usize)
        } else {
            0
        }
    }

    fn option_check(&self, option: i32) -> isize {
        if (0..16).contains(&option) && OPTION_IS_CHECK[option as usize] {
            self.m(G_OPTION + option as usize)
        } else {
            0
        }
    }

    fn game_options_init(&self, config: isize, local: &Cell<isize>) {
        for slot in 36..=38 {
            self.find(config, slot, local);
        }
        for (i, &is_check) in OPTION_IS_CHECK.iter().enumerate() {
            let slot = 39 + i;
            self.find(config, slot, local);
            let control = self.m(G_OPTION + i);
            let ok = if is_check { self.ask(Op::IsButton, control) } else { self.ask(Op::IsCombo, control) };
            if !ok {
                self.op(Op::ReportInvalidType, slot as isize, 0, 0);
            }
            self.op(Op::SetControlId, self.m(G_OPTION + i), i as isize + 1, 0);
        }
        let limits = self.option_combo(9);
        if limits != 0 {
            for limit in (50..=1200).step_by(50) {
                self.op(Op::ComboAddNumber, limits, limit, 0);
            }
        }
        self.op(Op::FillMapTypes, self.option_combo(2), 0, 0);
        self.op(Op::SubscribeBroadcast, 1, 0, 0);
        self.op(Op::SetVisible, self.option_check(15), 0, 0);
    }

    /// UpdateOption(option, game): the game's option shown in its control; the unit limit in steps of 50.
    fn update_option(&self, option: i32, game: isize) {
        // the original's range test let 16 through and then read past its table (undefined): nothing here
        if game == 0 || !(0..16).contains(&option) {
            return;
        }
        if option != 9 {
            if OPTION_IS_CHECK[option as usize] {
                let check = self.option_check(option);
                if check != 0 {
                    let v = self.game(game, GC::GetOption, option as isize, 0);
                    self.op(Op::ButtonSetCheck, check, (v != 0) as isize, 0);
                }
            } else {
                let combo = self.option_combo(option);
                if combo != 0 {
                    let v = self.game(game, GC::GetOption, option as isize, 0) as i32;
                    self.op(Op::ComboSetSel, combo, v as isize, 0);
                }
            }
        } else {
            let combo = self.option_combo(9);
            if combo != 0 {
                let v = self.game(game, GC::GetOption, 9, 0) as u32;
                self.op(Op::ComboSetSel, combo, (v / 50).wrapping_sub(1) as i32 as isize, 0);
            }
        }
    }

    fn update_all(&self) {
        let game = self.op(Op::MyGame, 0, 0, 0);
        if game == 0 {
            return;
        }
        for i in 0..16 {
            self.update_option(i, game);
        }
        let not_random_map = self.game(game, GC::GetOption, 0, 0) != 0;
        let host = self.ask(Op::GameIsHost, game);
        for j in 0..16 {
            let control = self.option_control(j);
            let enabled = if j == 0 { host } else { host && !not_random_map };
            if control != 0 {
                self.set_enabled(control, enabled);
            }
        }
        if self.game(game, GC::HasGameFile, 0, 0) != 0 {
            self.op(Op::SetVisible, self.m(G_BOX1), 0, 0);
            self.op(Op::SetVisible, self.m(G_BOX2), 1, 0);
            let name = self.game(game, GC::GetGameFileName, 0, 0);
            self.op(Op::LabelSetName, self.m(G_LABEL), name, 0);
        } else {
            self.op(Op::SetVisible, self.m(G_BOX1), 1, 0);
            self.op(Op::SetVisible, self.m(G_BOX2), 0, 0);
        }
    }

    fn activate(&self) {
        let game = self.op(Op::MyGame, 0, 0, 0);
        if game == 0 || self.op(Op::GameMatchesSpec, game, self.room_flags.get() as isize, 0) == 0 {
            return;
        }
        self.in_game.set(true);
        self.update_all();
        let host = self.ask(Op::GameIsHost, game);
        for i in 0..16 {
            self.set_enabled(self.m(G_OPTION + i), host);
        }
    }

    /// ChangeGameType: random map or a saved game (the file dialog; the named slots opened for a random map).
    fn change_game_type(&self) {
        let combo = self.option_combo(0);
        if combo == 0 || !self.ask(Op::ComboHasSel, combo) {
            return;
        }
        let game = self.op(Op::MyGame, 0, 0, 0);
        if game == 0 {
            return;
        }
        let old = self.game(game, GC::GetOption, 0, 0) as i32;
        if self.op(Op::ComboGetSel, combo, 0, 0) == 1 {
            let result = self.op(Op::ChooseSavedGame, 0, 0, 0);
            let game = self.op(Op::MyGame, 0, 0, 0);
            if game != 0 {
                if result == 100001 {
                    self.game(game, GC::SetOption, 0, 1);
                    self.game(game, GC::SendGameState, 0, 0);
                } else {
                    self.op(Op::ComboSetSel, combo, old as isize, 0);
                }
            }
        } else {
            self.op(Op::ComboSetSel, combo, 0, 0);
            self.game(game, GC::SetOption, 0, 0);
            // GetPlayerSlot(i) of 0..7 is never NULL
            for i in 0..8 {
                if self.game(game, GC::SlotRawOption, i, 0) == 4 {
                    self.game(game, GC::SlotSetOpen, i, 0);
                }
            }
            self.game(game, GC::SendGameState, 0, 0);
        }
    }

    /// OptionChanged: the captain's change sent to the game; anyone else's control restored from the game.
    fn option_changed(&self, option: i32) {
        if !(0..16).contains(&option) {
            return;
        }
        if !self.ask(Op::IAmCaptain, 0) {
            let game = self.op(Op::MyGame, 0, 0, 0);
            self.update_option(option, game);
            return;
        }
        let game = self.op(Op::MyGame, 0, 0, 0);
        if game == 0 {
            return;
        }
        if option == 0 {
            self.change_game_type();
        } else if option != 9 {
            if OPTION_IS_CHECK[option as usize] {
                let check = self.option_check(option);
                if check != 0 {
                    let v = self.op(Op::ButtonIsChecked, check, 0, 0);
                    self.game3(game, GC::ChangeOption, option as isize, v, 1);
                }
            } else {
                let combo = self.option_combo(option);
                if combo != 0 && self.ask(Op::ComboHasSel, combo) {
                    let v = self.op(Op::ComboGetSel, combo, 0, 0) as i32;
                    self.game3(game, GC::ChangeOption, option as isize, v as isize, 1);
                }
            }
        } else {
            let combo = self.option_combo(9);
            if combo != 0 && self.ask(Op::ComboHasSel, combo) {
                let sel = self.op(Op::ComboGetSel, combo, 0, 0) as i32;
                self.game3(game, GC::ChangeOption, 9, sel.wrapping_add(1).wrapping_mul(50) as isize, 1);
            }
        }
    }

    fn game_options_event(&self, event: isize, kind: Ev) {
        match kind {
            Ev::AllOptions => {
                if self.in_game.get() {
                    self.update_all();
                }
            }
            Ev::SyncOption => {
                if self.in_game.get() {
                    let option = self.op(Op::EventParam, event, 0, 0) as i32;
                    let game = self.op(Op::MyGame, 0, 0, 0);
                    self.update_option(option, game);
                }
            }
            Ev::SetRoomSpecFlags => self.add_room_flags(event),
            Ev::LeftGame => self.in_game.set(false),
            Ev::JoinedGame => self.activate(),
            Ev::ComboSelChanged | Ev::ButtonPressed if self.in_game.get() => {
                let id = self.op(Op::EventControlId, event, 0, 0) as i32;
                self.option_changed(id.wrapping_sub(1));
            }
            // no base call
            _ => {}
        }
    }

    // ---- EELobbyPlayerOptionsCtrl

    fn player_options_init(&self, config: isize, local: &Cell<isize>) {
        for slot in 55..=102 {
            self.find(config, slot, local);
        }
        self.op(Op::SubscribeBroadcast, 1, 0, 0);
    }

    /// The slot's `mOptions[option]` (the view's `Option`).
    fn slot_option(&self, game: isize, slot: isize, option: isize) -> isize {
        self.game(game, GC::SlotRawOption, slot, option)
    }

    /// UpdatePlayer(slot, game): one slot's row (name or state, team, colour, citizen, observer, transfer percent),
    /// enabled by role.
    fn update_player(&self, slot: i32, game: isize) {
        if game == 0 || !(0..8).contains(&slot) {
            return;
        }
        // GetPlayerSlot of 0..7 is never NULL
        let s = slot as isize;
        let i = slot as usize;
        let team = self.m(P_TEAM + i);
        self.op(Op::ComboSetSel, team, self.slot_option(game, s, 1), 0);
        let citizen = self.m(P_CITIZEN + i);
        self.op(Op::ComboSetSel, citizen, self.slot_option(game, s, 3) - 1, 0);
        let mut taken = false;
        let mut show_team = false;
        let not_random_map = self.game(game, GC::GetOption, 0, 0) != 0;
        let is_me = slot == self.my_slot.get() && !self.ask(Op::MyPlayerReady, 0);
        if self.slot_option(game, s, 0) == 1 {
            show_team = true;
        }
        let name = self.m(P_NAME + i);
        if self.slot_option(game, s, 0) != 3 && self.slot_option(game, s, 0) != 4 {
            self.op(Op::ComboSetSel, name, self.slot_option(game, s, 0), 0);
            self.op(Op::ComboSetTextColor, name, 0x8000000e, 0);
            self.op(Op::SetVisible, self.m(P_OBSERVE + i), 0, 0);
            self.op(Op::SetVisible, self.m(P_COLOR + i), 0, 0);
            self.op(Op::SetVisible, self.m(P_TRANSFER + i), 0, 0);
        } else {
            show_team = self.slot_option(game, s, 4) == 0;
            taken = true;
            let text = self.game(game, GC::SlotGetName, s, 0);
            self.op(Op::ComboSetName, name, text, 0);
            let color = if self.slot_option(game, s, 0) == 4 { 0xff0000 } else { 0x8000000e };
            self.op(Op::ComboSetTextColor, name, color, 0);
            let observe = self.m(P_OBSERVE + i);
            self.op(Op::SetVisible, observe, 1, 0);
            self.set_enabled(observe, is_me && !not_random_map);
            let checked = self.slot_option(game, s, 4) != 0;
            self.op(Op::ButtonSetCheck, observe, checked as isize, 0);
            let button = self.m(P_COLOR + i);
            let visible = self.slot_option(game, s, 4) == 0;
            self.op(Op::SetVisible, button, visible as isize, 0);
            self.set_enabled(button, is_me && !not_random_map);
            self.op(Op::ColorSetColor, button, self.slot_option(game, s, 2), 0);
            let transfer = self.m(P_TRANSFER + i);
            let percent = self.slot_option(game, s, 5);
            if percent >= 100 {
                self.op(Op::SetVisible, transfer, 0, 0);
            } else {
                self.op(Op::SetVisible, transfer, 1, 0);
                self.op(Op::LabelSetPercent, transfer, self.slot_option(game, s, 5), 0);
            }
        }
        let host = self.is_host.get();
        self.set_enabled(name, host && slot != self.my_slot.get() && !not_random_map);
        self.set_enabled(citizen, host && !not_random_map);
        self.set_enabled(team, (is_me || (host && !taken)) && !not_random_map);
        self.op(Op::SetVisible, citizen, show_team as isize, 0);
        self.op(Op::SetVisible, team, show_team as isize, 0);
    }

    fn update_my_player(&self, slot: i32) {
        let game = self.op(Op::MyGame, 0, 0, 0);
        self.update_player(slot, game);
    }

    fn update_players(&self) {
        let game = self.op(Op::MyGame, 0, 0, 0);
        if game != 0 {
            for i in 0..8 {
                self.update_player(i, game);
            }
        }
    }

    fn activate_players(&self) {
        let game = self.op(Op::MyGame, 0, 0, 0);
        if game == 0 || self.op(Op::GameMatchesSpec, game, self.room_flags.get() as isize, 0) == 0 {
            return;
        }
        self.in_game.set(true);
        self.is_host.set(self.ask(Op::IAmCaptain, 0));
        let player = self.op(Op::MyPlayer, 0, 0, 0);
        if player != 0 {
            self.my_slot.set(self.op(Op::PlayerSlot, player, 0, 0) as u8 as i32);
        }
        self.update_players();
    }

    /// PlayerOptionChanged(slot, what): the changed row control's value sent to the game as the slot's option.
    fn player_option_changed(&self, slot: i32, what: i32) {
        let game = self.op(Op::MyGame, 0, 0, 0);
        if game == 0 || !(0..8).contains(&slot) || !(0..6).contains(&what) {
            return;
        }
        let i = slot as usize;
        let value = match what {
            4 => self.op(Op::ButtonIsChecked, self.m(P_OBSERVE + i), 0, 0) as u8,
            3 => (self.op(Op::ComboGetSel, self.m(P_CITIZEN + i), 0, 0) as i32).wrapping_add(1) as u8,
            2 => self.op(Op::ColorGetColor, self.m(P_COLOR + i), 0, 0) as u8,
            1 => self.op(Op::ComboGetSel, self.m(P_TEAM + i), 0, 0) as u8,
            0 => self.op(Op::ComboGetSel, self.m(P_NAME + i), 0, 0) as u8,
            // the transfer label (5) has no value of its own: 0
            _ => 0,
        };
        self.game3(game, GC::ChangePlayerOption, slot as isize, what as isize, value as isize);
    }

    fn players_event(&self, event: isize, kind: Ev) {
        match kind {
            Ev::LeftGame => {
                if self.in_game.get() {
                    self.my_slot.set(-1);
                    self.in_game.set(false);
                    self.is_host.set(false);
                }
            }
            Ev::JoinedGame => self.activate_players(),
            Ev::PlayerChanged | Ev::SyncPlayer => {
                if kind == Ev::PlayerChanged && self.ask(Op::EventIsModify, event) {
                    // HandlePlayerChanged: our own row
                    let client = self.op(Op::PlayerChangedClient, event, 0, 0);
                    if self.op(Op::ClientPlayer, client, 0, 0) != 0 {
                        let player = self.op(Op::ClientPlayer, client, 0, 0);
                        if self.op(Op::PlayerSlot, player, 0, 0) as u8 as i32 == self.my_slot.get() {
                            self.update_my_player(self.my_slot.get());
                        }
                    }
                }
                // a player change falls through: its change kind read as the slot
                if self.in_game.get() {
                    let slot = self.op(Op::EventParam, event, 0, 0) as i32;
                    self.update_my_player(slot);
                }
            }
            Ev::AllOptions => {
                if self.in_game.get() {
                    self.update_players();
                }
            }
            Ev::ButtonPressed | Ev::ComboSelChanged | Ev::ColorChanged => {
                if self.in_game.get() {
                    let id = (self.op(Op::EventControlId, event, 0, 0) as i32).wrapping_sub(1);
                    self.player_option_changed(id / 6, id % 6);
                }
            }
            Ev::SetRoomSpecFlags => self.add_room_flags(event),
            // no base call
            _ => {}
        }
    }

    // ---- the calls

    fn event(&self, event: isize, kind: u32, component: isize) {
        let kind = EVENTS.get(kind as usize).copied().unwrap_or(Ev::Other);
        match self.kind {
            Kind::Browser => self.browser_event(event, kind),
            Kind::Staging => self.staging_event(event, kind),
            Kind::LanOptions | Kind::NetOptions => self.options_event(event, kind, component),
            Kind::UserList => self.user_list_event(event, kind, component),
            Kind::GameOptions => self.game_options_event(event, kind),
            Kind::PlayerOptions => self.players_event(event, kind),
            Kind::ServerList => self.srv_event(event, kind, component),
            Kind::Chat => self.chat_event(event, kind, component),
            _ => self.net_event(event, kind, component),
        }
    }

    /// One inward call.
    pub fn call(&self, call: EeWonLobbyCtlCall, a: isize, b: isize, c: isize) -> isize {
        use EeWonLobbyCtlCall as C;
        match call {
            C::Init => self.init(a),
            C::Event => self.event(a, b as u32, c),
            C::Paint => {
                if self.kind == Kind::Browser {
                    self.paint(a)
                } else {
                    self.net_paint(a)
                }
            }
            C::ResetBrowser => self.reset_browser(),
            C::Field => {
                return match a {
                    0..=47 => self.members[a as usize].get(),
                    100 => self.room_flags.get() as isize,
                    101 => self.in_game.get() as isize,
                    102 => self.last_ready.get() as isize,
                    103 => self.is_reset.get() as isize,
                    104 => self.my_slot.get() as isize,
                    105 => self.is_host.get() as isize,
                    110 => self.ext.srv.game_type.get() as isize,
                    111 => self.ext.srv.need_filter.get() as isize,
                    112 => self.ext.srv.filter_time.get() as isize,
                    113 => self.ext.srv.num_columns.get() as isize,
                    114 => self.ext.srv.map.borrow().len() as isize,
                    115 => self.ext.srv.columns.borrow().len() as isize,
                    120 => self.ext.chat.client_id.get() as isize,
                    121 => self.ext.chat.room_name.get(),
                    130..=140 => self.ext.chat.colors[a as usize - 130].get() as isize,
                    150 => self.ext.net.flag.get() as isize,
                    151 => self.ext.net.number.get() as isize,
                    152..=154 => self.ext.net.texts[a as usize - 152].get(),
                    _ => 0,
                }
            }
            C::Timer => return self.srv_timer_call(a),
            C::SetField => {
                if self.kind == Kind::Chat {
                    self.set_field(a, b)
                } else {
                    self.net_set_field(a, b)
                }
            }
            C::Page => self.login_screen_page(a),
            C::Row => {
                let Some(call) = crate::srvlistctl::row_call_of(a) else {
                    return 0;
                };
                // SAFETY: the shell's three arguments
                let args = unsafe { *(c as *const [isize; 3]) };
                return self.row_call(call, b, args[0], args[1], args[2]);
            }
            C::Cfg => {
                let Some(call) = crate::srvlistctl::cfg_call_of(a) else {
                    return 0;
                };
                return self.srv_config(call, b, c);
            }
        }
        0
    }

    /// The destructor: the `SmartPtr` members in reverse order (ServerListCtrl: its rows first, its columns last).
    fn release_all(&self) {
        if self.kind == Kind::ServerList {
            self.srv_release_rows();
        }
        if self.kind == Kind::Chat {
            self.chat_release();
        }
        if self.kind as u32 >= Kind::Room as u32 {
            self.net_release();
        }
        for i in (0..member_count(self.kind)).rev() {
            let p = self.members[i].replace(0);
            if p != 0 {
                self.op(Op::Release, p, 0, 0);
            }
        }
        if self.kind == Kind::ServerList {
            self.srv_release_columns();
        }
    }
}

const EVENTS: [Ev; 51] = [
    Ev::Other,
    Ev::ButtonPressed,
    Ev::ComboSelChanged,
    Ev::ListSelChanged,
    Ev::ListItemClicked,
    Ev::BrowserButtonChange,
    Ev::BrowserStatusChange,
    Ev::JoinedGame,
    Ev::LeftGame,
    Ev::SetRoomSpecFlags,
    Ev::ReadyChanged,
    Ev::ClientChanged,
    Ev::PlayerChanged,
    Ev::SyncOptions,
    Ev::LanOptions,
    Ev::PublicAddress,
    Ev::AllOptions,
    Ev::SyncOption,
    Ev::SyncPlayer,
    Ev::ColorChanged,
    Ev::InputTextChanged,
    Ev::InputReturn,
    Ev::GameChanged,
    Ev::SyncGameList,
    Ev::FiltersChanged,
    Ev::RefilterAll,
    Ev::RefilterShown,
    Ev::TextLinkActivated,
    Ev::Alert,
    Ev::AnnounceFriend,
    Ev::AnnounceClientLocation,
    Ev::ClientModerated,
    Ev::ChatCommandError,
    Ev::GroupInvitation,
    Ev::GroupJoinAttempt,
    Ev::JoinedRoom,
    Ev::LeftRoom,
    Ev::PlayerTeamChanged,
    Ev::RecvChat,
    Ev::SetChatInput,
    Ev::TreeItemOpened,
    Ev::GroupChanged,
    Ev::ServerChanged,
    Ev::SetRoomList,
    Ev::FriendChanged,
    Ev::RefreshFriends,
    Ev::LoginInitStatus,
    Ev::SetLoginFields,
    Ev::FillUserNames,
    Ev::QueryAccountResult,
    Ev::GotHttpDocument,
];

/// The round 13 kinds' members beyond their controls.
#[derive(Default)]
pub(crate) struct Ext {
    pub srv: crate::srvlistctl::SrvState,
    pub chat: crate::chatctl::ChatState,
    pub net: crate::netctl::NetState,
}

impl Ext {
    fn new(kind: Kind) -> Self {
        let e = Ext::default();
        if kind == Kind::ServerList {
            e.srv.num_columns.set(4);
        }
        if kind == Kind::Chat {
            e.chat.init();
        }
        if kind == Kind::Users {
            e.net.number.set(5);
        }
        if kind == Kind::RoomLabel {
            e.net.flag.set(true);
        }
        e
    }
}

/// The look-up `index` of `SLOTS` (NULL past the end).
#[no_mangle]
pub extern "C" fn ee_won_lobbyctl_slot(index: u32) -> *const EeWonLobbyCtlSlot {
    SLOTS.get(index as usize).map_or(std::ptr::null(), |s| s as *const _)
}

/// The number of look-ups in `SLOTS`.
#[no_mangle]
pub extern "C" fn ee_won_lobbyctl_slot_count() -> u32 {
    SLOTS.len() as u32
}

/// A new control object for the C++ `ctl` of `kind`.
/// As `ee_won_lobbyctl_new`, with the model's ops answered by its Rust objects (the game's).
#[no_mangle]
pub extern "C" fn ee_won_lobbyctl_new_model(
    ctl: *mut c_void,
    kind: EeWonLobbyCtlKind,
    host: EeWonLobbyCtlHostFn,
) -> *mut EeWonLobbyCtl {
    let c = EeWonLobbyCtl::new(ctl, kind, host);
    c.model.set(true);
    Box::into_raw(Box::new(c))
}

#[no_mangle]
pub extern "C" fn ee_won_lobbyctl_new(
    ctl: *mut c_void,
    kind: EeWonLobbyCtlKind,
    host: EeWonLobbyCtlHostFn,
) -> *mut EeWonLobbyCtl {
    Box::into_raw(Box::new(EeWonLobbyCtl::new(ctl, kind, host)))
}

/// Releases the members (reverse member order) and frees the object.
///
/// # Safety
/// `p` came from `ee_won_lobbyctl_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyctl_free(p: *mut EeWonLobbyCtl) {
    if !p.is_null() {
        // SAFETY: the contract; the releases run through a shared reference before the box is dropped
        unsafe { (*p).release_all() };
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(p) });
    }
}

/// One inward call (`EeWonLobbyCtlCall`).
///
/// # Safety
/// `p` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyctl_call(
    p: *const EeWonLobbyCtl,
    call: EeWonLobbyCtlCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out may re-enter
    unsafe { (*p).call(call, a, b, c) }
}

#[cfg(test)]
#[path = "lobbyctl_tests.rs"]
mod tests;
