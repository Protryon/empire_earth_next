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
    /// `((EELobbyGame*)a)->GetOption(b)`.
    GameOption,
    /// `((EELobbyGame*)a)->SetOption(b, c)`.
    GameSetOption,
    /// `((EELobbyGame*)a)->ChangeOption(b, c, true)`.
    GameChangeOption,
    /// `((EELobbyGame*)a)->SendGameState()`.
    GameSendState,
    /// `((EELobbyGame*)a)->HasGameFile()` (0 or 1).
    GameHasFile,
    /// `((Label*)a)->SetText(((EELobbyGame*)b)->GetGameFileName())`.
    LabelSetGameFile,
    /// `((EELobbyGame*)a)->GetPlayerSlot(b)`.
    GameSlot,
    /// `((EELobbyPlayerSlot*)a)->mOptions[0]` (the slot's state).
    SlotState,
    /// `((EELobbyPlayerSlot*)a)->SetOpen()`.
    SlotSetOpen,
    /// `LobbyIAmCaptain()` (0 or 1).
    IAmCaptain,
    /// `((EELobbySyncOptionEvent*)a)->mParam`.
    EventParam,
    /// The saved game's choice: `EELobbyFn2988E9()`'s directory, `EELobbyGetFileDialog()`, its
    /// `ListFiles(dir, ".ees")` and `LobbyDoDialog`'s result (-1 without a dialog).
    ChooseSavedGame,
    /// `((EELobbyPlayerSlot*)a)->mOptions[b]` (round 5: EELobbyPlayerOptionsCtrl).
    SlotOption,
    /// `((ComboBox*)a)->SetText(((EELobbyPlayerSlot*)b)->GetName())`.
    ComboSetSlotName,
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
    /// `((EELobbyGame*)a)->ChangePlayerOption(b, c & 0xff, c >> 8)` (the slot, the option, the value).
    GameChangePlayerOption,
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
    /// EELobbyPlayerOptionsCtrl's own slot, 105 its host flag.
    Field,
}

/// The host: `(control, op, a, b, c) -> result`.
pub type EeWonLobbyCtlHostFn =
    Option<unsafe extern "C" fn(ctl: *mut c_void, op: EeWonLobbyCtlOp, a: isize, b: isize, c: isize) -> isize>;

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
pub static SLOTS: [EeWonLobbyCtlSlot; 103] = [
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
    slot!(c"UserNameCombo1", c"EELobbyPlayerOptionsCtrl.cpp", 50, Combo, 1, (P_NAME + 0) as i32),
    slot!(c"TeamCombo1", c"EELobbyPlayerOptionsCtrl.cpp", 54, Combo, 2, (P_TEAM + 0) as i32),
    slot!(c"ColorButton1", c"EELobbyPlayerOptionsCtrl.cpp", 58, ColorButton, 3, (P_COLOR + 0) as i32),
    slot!(c"CitizenCombo1", c"EELobbyPlayerOptionsCtrl.cpp", 62, Combo, 4, (P_CITIZEN + 0) as i32),
    slot!(c"ObserveCheck1", c"EELobbyPlayerOptionsCtrl.cpp", 66, Button, 5, (P_OBSERVE + 0) as i32),
    slot!(c"FileTransferLabel1", c"EELobbyPlayerOptionsCtrl.cpp", 70, Label, 6, (P_TRANSFER + 0) as i32),
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
];

/// The number of members each kind holds (released in reverse by the destructor).
fn member_count(kind: Kind) -> usize {
    match kind {
        Kind::Browser => 11,
        Kind::Staging | Kind::UserList => 2,
        Kind::LanOptions | Kind::NetOptions => 18,
        Kind::GameOptions => 19,
        Kind::PlayerOptions => MEMBERS,
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
    ctl: *mut c_void,
    host: EeWonLobbyCtlHostFn,
    kind: Kind,
    /// the `SmartPtr` members, in the original's member order (see the `B_`, `S_`, `O_` and `U_` indices)
    members: [Cell<isize>; MEMBERS],
    /// `mRoomSpecFlags` (GameStagingCtrl, GameUserListCtrl: 2 from the constructor)
    room_flags: Cell<i32>,
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
        }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize) -> isize {
        match self.host {
            // SAFETY: the host is the C++ control's (or a test's), called with the control it was given for
            Some(f) => unsafe { f(self.ctl, op, a, b, c) },
            None => 0,
        }
    }

    fn ask(&self, op: Op, a: isize) -> bool {
        self.op(op, a, 0, 0) != 0
    }

    fn m(&self, i: usize) -> isize {
        self.members[i].get()
    }

    /// A `SmartPtr`'s assignment (`ConstSmartPtr::operator=`): nothing for the same pointer, else the old one
    /// released, the new one referenced.
    fn assign(&self, member: &Cell<isize>, new: isize) {
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
    fn find(&self, config: isize, slot: usize, local: &Cell<isize>) {
        let s = &SLOTS[slot];
        let found = self.op(Op::Find, config, slot as isize, 0);
        let member = if s.member < 0 { local } else { &self.members[s.member as usize] };
        self.assign(member, found);
        if s.control_id >= 0 {
            self.op(Op::SetControlId, member.get(), s.control_id as isize, 0);
        }
    }

    fn set_enabled(&self, control: isize, enabled: bool) {
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

    fn pref(&self, p: EeWonLobbyCtlPref) -> isize {
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
                    let v = self.op(Op::GameOption, game, option as isize, 0);
                    self.op(Op::ButtonSetCheck, check, (v != 0) as isize, 0);
                }
            } else {
                let combo = self.option_combo(option);
                if combo != 0 {
                    let v = self.op(Op::GameOption, game, option as isize, 0) as i32;
                    self.op(Op::ComboSetSel, combo, v as isize, 0);
                }
            }
        } else {
            let combo = self.option_combo(9);
            if combo != 0 {
                let v = self.op(Op::GameOption, game, 9, 0) as u32;
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
        let not_random_map = self.op(Op::GameOption, game, 0, 0) != 0;
        let host = self.ask(Op::GameIsHost, game);
        for j in 0..16 {
            let control = self.option_control(j);
            let enabled = if j == 0 { host } else { host && !not_random_map };
            if control != 0 {
                self.set_enabled(control, enabled);
            }
        }
        if self.ask(Op::GameHasFile, game) {
            self.op(Op::SetVisible, self.m(G_BOX1), 0, 0);
            self.op(Op::SetVisible, self.m(G_BOX2), 1, 0);
            self.op(Op::LabelSetGameFile, self.m(G_LABEL), game, 0);
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
        let old = self.op(Op::GameOption, game, 0, 0) as i32;
        if self.op(Op::ComboGetSel, combo, 0, 0) == 1 {
            let result = self.op(Op::ChooseSavedGame, 0, 0, 0);
            let game = self.op(Op::MyGame, 0, 0, 0);
            if game != 0 {
                if result == 100001 {
                    self.op(Op::GameSetOption, game, 0, 1);
                    self.op(Op::GameSendState, game, 0, 0);
                } else {
                    self.op(Op::ComboSetSel, combo, old as isize, 0);
                }
            }
        } else {
            self.op(Op::ComboSetSel, combo, 0, 0);
            self.op(Op::GameSetOption, game, 0, 0);
            for i in 0..8 {
                let slot = self.op(Op::GameSlot, game, i, 0);
                if slot != 0 && self.op(Op::SlotState, slot, 0, 0) == 4 {
                    self.op(Op::SlotSetOpen, slot, 0, 0);
                }
            }
            self.op(Op::GameSendState, game, 0, 0);
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
                    self.op(Op::GameChangeOption, game, option as isize, v);
                }
            } else {
                let combo = self.option_combo(option);
                if combo != 0 && self.ask(Op::ComboHasSel, combo) {
                    let v = self.op(Op::ComboGetSel, combo, 0, 0) as i32;
                    self.op(Op::GameChangeOption, game, option as isize, v as isize);
                }
            }
        } else {
            let combo = self.option_combo(9);
            if combo != 0 && self.ask(Op::ComboHasSel, combo) {
                let sel = self.op(Op::ComboGetSel, combo, 0, 0) as i32;
                self.op(Op::GameChangeOption, game, 9, sel.wrapping_add(1).wrapping_mul(50) as isize);
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

    fn slot_option(&self, slot: isize, option: isize) -> isize {
        self.op(Op::SlotOption, slot, option, 0)
    }

    /// UpdatePlayer(slot, game): one slot's row (name or state, team, colour, citizen, observer, transfer percent),
    /// enabled by role.
    fn update_player(&self, slot: i32, game: isize) {
        if game == 0 || !(0..8).contains(&slot) {
            return;
        }
        let s = self.op(Op::GameSlot, game, slot as isize, 0);
        if s == 0 {
            return;
        }
        let i = slot as usize;
        let team = self.m(P_TEAM + i);
        self.op(Op::ComboSetSel, team, self.slot_option(s, 1), 0);
        let citizen = self.m(P_CITIZEN + i);
        self.op(Op::ComboSetSel, citizen, self.slot_option(s, 3) - 1, 0);
        let mut taken = false;
        let mut show_team = false;
        let not_random_map = self.op(Op::GameOption, game, 0, 0) != 0;
        let is_me = slot == self.my_slot.get() && !self.ask(Op::MyPlayerReady, 0);
        if self.slot_option(s, 0) == 1 {
            show_team = true;
        }
        let name = self.m(P_NAME + i);
        if self.slot_option(s, 0) != 3 && self.slot_option(s, 0) != 4 {
            self.op(Op::ComboSetSel, name, self.slot_option(s, 0), 0);
            self.op(Op::ComboSetTextColor, name, 0x8000000e, 0);
            self.op(Op::SetVisible, self.m(P_OBSERVE + i), 0, 0);
            self.op(Op::SetVisible, self.m(P_COLOR + i), 0, 0);
            self.op(Op::SetVisible, self.m(P_TRANSFER + i), 0, 0);
        } else {
            show_team = self.slot_option(s, 4) == 0;
            taken = true;
            self.op(Op::ComboSetSlotName, name, s, 0);
            let color = if self.slot_option(s, 0) == 4 { 0xff0000 } else { 0x8000000e };
            self.op(Op::ComboSetTextColor, name, color, 0);
            let observe = self.m(P_OBSERVE + i);
            self.op(Op::SetVisible, observe, 1, 0);
            self.set_enabled(observe, is_me && !not_random_map);
            let checked = self.slot_option(s, 4) != 0;
            self.op(Op::ButtonSetCheck, observe, checked as isize, 0);
            let button = self.m(P_COLOR + i);
            let visible = self.slot_option(s, 4) == 0;
            self.op(Op::SetVisible, button, visible as isize, 0);
            self.set_enabled(button, is_me && !not_random_map);
            self.op(Op::ColorSetColor, button, self.slot_option(s, 2), 0);
            let transfer = self.m(P_TRANSFER + i);
            let percent = self.slot_option(s, 5);
            if percent >= 100 {
                self.op(Op::SetVisible, transfer, 0, 0);
            } else {
                self.op(Op::SetVisible, transfer, 1, 0);
                self.op(Op::LabelSetPercent, transfer, self.slot_option(s, 5), 0);
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
        self.op(Op::GameChangePlayerOption, game, slot as isize, what as isize | (value as isize) << 8);
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
        }
    }

    /// One inward call.
    pub fn call(&self, call: EeWonLobbyCtlCall, a: isize, b: isize, c: isize) -> isize {
        use EeWonLobbyCtlCall as C;
        match call {
            C::Init => self.init(a),
            C::Event => self.event(a, b as u32, c),
            C::Paint => self.paint(a),
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
                    _ => 0,
                }
            }
        }
        0
    }

    /// The destructor: the `SmartPtr` members in reverse order.
    fn release_all(&self) {
        for i in (0..member_count(self.kind)).rev() {
            let p = self.members[i].replace(0);
            if p != 0 {
                self.op(Op::Release, p, 0, 0);
            }
        }
    }
}

const EVENTS: [Ev; 20] = [
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
];

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
