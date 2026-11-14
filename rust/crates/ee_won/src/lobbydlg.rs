//! The lobby's small dialogs as one Rust object (R6 WONLobby round 1): LanNameDialog, PasswordDialog,
//! AddFriendDialog, DirectConnectDialog, LanPortDialog, CreateRoomDialog, CreateGameDialog and AdminActionDialog.
//! Each was a `Dialog` subclass whose code was its controls' look-up in the dialog's resource config, a Reset, an
//! input check and getters; their members were the controls (`SmartPtr`s) and a few scalars. One table here names
//! each kind's controls (in the original's member order, with the order `InitComponent` looked them up), and one
//! object per dialog holds the controls (by reference, as the `SmartPtr`s) and the scalars and runs every method of
//! the eight units.
//!
//! The C++ keeps one shell, `LobbyRustDialog : Dialog` (its InitComponent and HandleComponentEvent forward here),
//! and the eight class names as thin subclasses with one-line forwarders, so their holders (LanLogic, StagingLogic,
//! RoutingLogic) did not change. Every call on a control, the dialog's base, a lobby game or a global string is one
//! op of the host given to `ee_won_lobbydlg_new` (`LobbyDialogHost` in `WONLobby/LobbyDialogHost.cpp`); an op is
//! the original's statements with no decision between them. A control's `SetText` with a typed input fires an
//! event that re-enters this object (AddFriendDialog's `Clear(true)` reaches its CheckInput), so the state is in
//! `Cell`s behind a shared reference and no borrow lives across a call out.
//!
//! The pure reads the original made as one call's arguments (the create button's `create_enabled(name, shown,
//! password)`) are made left to right here; they change nothing, so their order is not behaviour.

use std::cell::Cell;
use std::ffi::{c_char, c_void, CStr};

use crate::dialogs;
use crate::eegame::{game_call, EeWonEeGameCall as GC};

/// The dialog kinds (each one of the original classes).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyDlgKind {
    LanName = 0,
    Password = 1,
    AddFriend = 2,
    DirectConnect = 3,
    LanPort = 4,
    CreateRoom = 5,
    CreateGame = 6,
    AdminAction = 7,
    /// StatusDialog (round 2): DialogLogic's status box.
    Status = 8,
    /// OkCancelDialog (round 2): DialogLogic's OK/Cancel and Yes/No box.
    OkCancel = 9,
    /// RoomDialog (round 3): no control (its RoomCtrl member was never set).
    Room = 10,
    /// TOUDialog (round 3): the terms of use.
    Tou = 11,
    /// BadUserDialog (round 3): the moderators' banned and muted users.
    BadUser = 12,
    /// ChooseNetworkAdapterDialog (round 3): InitLogic's adapter choice.
    ChooseAdapter = 13,
    /// CreditsDialog (round 3): AccountLogic's credits, with the scrolling text it holds.
    Credits = 14,
    /// EELobbyConnectDialog (round 5): the players' connection wait before a launch.
    Connect = 15,
}

/// A control's type: the `SmartPtr<T>` the original's `SafeGetComponent` filled (its `dynamic_cast`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyDlgType {
    Input = 0,
    Button = 1,
    Label = 2,
    Combo = 3,
    /// a plain `Component` (round 2)
    Component = 4,
    /// a `WONFrameComponent` (round 2)
    Frame = 5,
    /// a `BrowserComponent` (round 3)
    Browser = 6,
    /// a `MultiListArea` (round 3)
    MultiList = 7,
    /// a `ListArea` (round 3)
    List = 8,
}

/// One control of a kind: its resource name, the original's file and line (for the missing-control report) and type.
#[repr(C)]
pub struct EeWonLobbyDlgSlot {
    pub name: *const c_char,
    pub file: *const c_char,
    pub line: i32,
    pub ty: EeWonLobbyDlgType,
}

// SAFETY: the table's pointers are static C string literals.
unsafe impl Sync for EeWonLobbyDlgSlot {}

/// The texts AdminActionDialog sets (the lobby's resource strings).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyDlgString {
    /// an empty `GUIString` (an action the original's switch did not name)
    Empty = 0,
    Warn = 1,
    Mute = 2,
    UnMute = 3,
    Kick = 4,
    Ban = 5,
    ExplanationOpt = 6,
    ExplanationReq = 7,
    /// `LobbyGlobal_OK_String` (round 2)
    Ok = 8,
    /// `LobbyGlobal_Cancel_String` (round 2)
    Cancel = 9,
    /// `gEELobbyConnectDialog_AbortLaunch` (round 5)
    AbortLaunch = 10,
    /// `gEELobbyConnectDialog_LeaveGame` (round 5)
    LeaveGame = 11,
}

/// The event an inward `Event` carries (the C++ shell maps the `ComponentEvent`'s type).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyDlgEvent {
    Other = 0,
    InputTextChanged = 1,
    ButtonPressed = 2,
    ComboSelChanged = 3,
    /// `LobbyEvent_GotHTTPDocument` (round 3: a `GotHTTPDocumentEvent`, broadcast when a download finished)
    GotHttpDocument = 4,
}

/// The ops (`a`, `b`, `c` as documented; a control is a `Component*` this object holds).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyDlgOp {
    /// `SafeGetComponent(a, aPtr, b->name, b->file, b->line)` into a `SmartPtr` of `b->ty`; the control with a
    /// reference added (or NULL).
    Find = 1,
    /// `a->Release()` (a held control or game).
    Release,
    /// `Dialog::HandleComponentEvent(a)` on the dialog.
    BaseEvent,
    /// `Relayout()` on the dialog.
    Relayout,
    /// `a->RequestFocus()`.
    RequestFocus,
    /// `a->SetEnabled(b != 0)`.
    SetEnabled,
    /// `a->SetVisible(b != 0)`.
    SetVisible,
    /// `a->IsVisible()` (0 or 1).
    IsVisible,
    /// `((InputBox*)a)->Clear(b != 0)`.
    InputClear,
    /// `((InputBox*)a)->GetText().length()`.
    InputTextLength,
    /// `((InputBox*)a)->mText.length()` (the shown text: a password box's stars).
    InputShownLength,
    /// `((InputBox*)a)->GetText().mData->ToInt(0)`.
    InputToInt,
    /// `((InputBox*)a)->mInputFilter = InputBox::NumericFilter`.
    InputSetNumeric,
    /// `sprintf(aBuf, "%d", b); ((InputBox*)a)->SetText(GUIString(new GUIStringData(aBuf)), c != 0)`.
    InputSetNumber,
    /// `((InputBox*)a)->SetText(GUIString(""), b != 0)`.
    InputSetEmpty,
    /// `((InputBox*)a)->SelectRange(b, c)`.
    InputSelectRange,
    /// `sprintf(aBuf, "%d", b); ((Label*)a)->SetText(GUIString(new GUIStringData(aBuf)), true)`.
    LabelSetNumber,
    /// `((Label*)a)->SetText(the string b, true)` (`EeWonLobbyDlgString`).
    LabelSetString,
    /// `((Button*)a)->SetText(the string b, true)`.
    ButtonSetString,
    /// `aTitle = the string b; aTitle.append(" "); aTitle.append(((LobbyClient*)c)->GetName());
    /// ((Label*)a)->SetText(aTitle, true)`.
    AdminTitle,
    /// `((Button*)a)->IsDisabled()` (0 or 1).
    ButtonIsDisabled,
    /// `((Button*)a)->IsChecked()` (0 or 1).
    ButtonIsChecked,
    /// `((Button*)a)->SetCheck(b != 0, c != 0)`.
    ButtonSetCheck,
    /// `((ComboBox*)a)->GetSelItem()`.
    ComboGetSel,
    /// `((ComboBox*)a)->SetSelItem(b, c != 0)`.
    ComboSetSel,
    /// `LobbyGame::CreateGame((LobbyGameType)b)` with a reference added (the original's local `SmartPtr`).
    GameCreate,
    /// `((LobbyGame*)a)->SetName(((InputBox*)b)->GetText())`.
    GameSetName,
    /// `((LobbyGame*)a)->SetPassword(((InputBox*)b)->GetText())`.
    GameSetPassword,
    /// `((LobbyGame*)a)->SetAskToJoin(b != 0)`.
    GameSetAskToJoin,
    /// `((LobbyGame*)a)->SetInviteOnly(b != 0)`.
    GameSetInviteOnly,
    /// `((LobbyGame*)a)->SetSkill(b)`.
    GameSetSkill,
    // ---- round 2: the status and OK/Cancel boxes' sizing (`a` a control or the dialog itself)
    /// `a->Width()`.
    Width,
    /// `a->Height()`.
    Height,
    /// `a->Left()`.
    Left,
    /// `a->Top()`.
    Top,
    /// `a->SetSize(b, c)`.
    SetSize,
    /// `a->SetSizeAtLeast(b, c)`.
    SetSizeAtLeast,
    /// `a->SetDesiredSize(b, c)`.
    SetDesiredSize,
    /// `a->SetDesiredWidth(b)`.
    SetDesiredWidth,
    /// `a->SetPos(b, c)`.
    SetPos,
    /// `((Label*)a)->SetVertPad(b, c)`.
    SetVertPad,
    /// `((Label*)a)->SetText(*(const GUIString*)b, true)`, `gEmptyGUIString` for a NULL `b`.
    LabelSetText,
    /// `((Button*)a)->SetText(*(const GUIString*)b, true)`.
    ButtonSetText,
    /// `((WONFrameComponent*)a)->GetFrameLeft()`.
    FrameLeft,
    /// `((WONFrameComponent*)a)->GetFrameRight()`.
    FrameRight,
    /// `GetLobbyRoot()->Width()`.
    RootWidth,
    /// `GetLobbyRoot()->Height()`.
    RootHeight,
    /// `((Label*)a)->mWrappedText->mFont->GetStringWidth(*(const GUIString*)b)`.
    FontStringWidth,
    /// `aFont->GetStringWidth(s, 0, s.length())` of the label `a`'s font and the string `b`.
    FontStringWidthRange,
    /// `((Label*)a)->mWrappedText->mFont->GetCharacterWidth(b)`.
    FontCharWidth,
    /// `((Label*)a)->mWrappedText->mFont->GetHeight()`.
    FontHeight,
    /// `WONTickCount()`.
    TickCount,
    /// `a->Invalidate()` (the dialog).
    Invalidate,
    /// `((Dialog*)a)->IsDragging()` (0 or 1).
    IsDragging,
    /// `((Dialog*)a)->SetDragging(b != 0)`.
    SetDragging,
    /// `&((Label*)a)->mWrappedText->mText` (the title's own text, as SetStatus passed it on).
    TitleText,
    // ---- round 3: TOUDialog, BadUserDialog, ChooseNetworkAdapterDialog
    /// `((Button*)a)->mControlId = b`.
    ButtonSetControlId,
    /// `((Component*)a)->SubscribeToBroadcast(b != 0)` (the dialog).
    SubscribeBroadcast,
    /// `((BrowserComponent*)a)->ShowFiles(LobbyGetSysTOUPath(), LobbyGetGameTOUPath(), false)`.
    BrowserShowTou,
    /// `((BrowserComponent*)a)->GetHTMLView()->SetOffsets(0, 0)`.
    BrowserTop,
    /// `((GotHTTPDocumentEvent*)a)->mOp->GetNum(EE_WON_HTTPDOC_TYPE)`.
    HttpDocType,
    /// `((ListArea*)a)->BeginUpdate()`.
    ListBeginUpdate,
    /// `((ListArea*)a)->EndUpdate()`.
    ListEndUpdate,
    /// `((ListArea*)a)->Clear(b != 0)`.
    ListClear,
    /// `for (each user of *(const RoutingBadUserList*)b) ((ListArea*)a)->InsertItem(new BadUserItem(user, c != 0),
    /// -1)`.
    BadUsersInsert,
    /// `((BadUserDialog::IdList*)a)->clear()`.
    IdsClear,
    /// `((ListArea*)a)->RewindSelections()`.
    ListRewind,
    /// `((ListArea*)a)->HasMoreSelections()` (0 or 1).
    ListHasMore,
    /// `((ListArea*)a)->GetNextSelection()`.
    ListNextSel,
    /// `((BadUserItem*)a)->mValues[1]` (the user's WON id).
    BadUserId,
    /// `((BadUserDialog::IdList*)a)->push_back((unsigned long)b)`.
    IdsPush,
    /// `InsertItem(new NetworkAdapterListItem(0), -1); SetString(0, 0, ChooseNetworkAdapterDialog_Default_String,
    /// NULL)` on the list `a`.
    AdapterAddDefault,
    /// `IPAddr::GetLocalHost()`.
    LocalHost,
    /// `((ListArea*)a)->SetSelItem(b, c != 0)`.
    ListSetSel,
    /// `((ListArea*)a)->GetNumItems()`.
    ListNumItems,
    /// `LobbyGetNetAddresses(aList, true)` into a new list; the list (freed by `AdaptersFree`).
    AdaptersGet,
    /// the number of addresses in the list `a`.
    AdaptersCount,
    /// the `b`-th address's `mHandle` of the list `a`.
    AdapterHandle,
    /// the `b`-th address's `GUIString(mName).length()` of the list `a`.
    AdapterNameLength,
    /// `((MultiListArea*)a)->InsertItem(new NetworkAdapterListItem(b), -1)`.
    AdapterInsertRow,
    /// `SetString(GetNumItems() - 1, 0, GUIString(the c-th address of the list b's mName), NULL)` on the list `a`.
    AdapterSetName,
    /// `SetString(GetNumItems() - 1, 0, ChooseNetworkAdapterDialog_Unknown_String, NULL)` on the list `a`.
    AdapterSetUnknownName,
    /// `SetString(GetNumItems() - 1, 1, IPAddr(b, 0).GetHostString(true), NULL)` on the list `a`.
    AdapterSetAddress,
    /// frees the list `a` (the original's local list).
    AdaptersFree,
    /// `((ListArea*)a)->GetSelItem()`.
    ListGetSel,
    /// `((NetworkAdapterListItem*)a)->mHost`.
    AdapterHost,
    // ---- round 3: CreditsDialog
    /// `a->AddRef()` (a `SmartPtr` assignment's).
    AddRef,
    /// `Component* aParent = ((Component*)b)->mParent; this->Fn2F6140((Component*)a, 0x5003, (Component*)b,
    /// aParent->Left(), aParent->Top(), 0, 0, 0)` (the scrolling text `a` in place of the credits button `b`).
    CreditsPlace,
    /// `this->AddChild((Component*)a)`.
    AddChild,
    /// `((ScrollingTextComponent*)a)->Start()`.
    ScrollStart,
    /// `((ScrollingTextComponent*)a)->Stop()`.
    ScrollStop,
    /// `((ScrollingTextComponent*)a)->Reset()`.
    ScrollReset,
    /// `((ScrollingTextComponent*)a)->AddLine(*(const GUIString*)b, (unsigned long)c)`.
    ScrollAddLine,
    /// `this->GetDefaultFont()->GetStringWidth(t, 0, t.length())` of the string `a`.
    DefaultFontStringWidth,
    // ---- round 5: EELobbyConnectDialog
    /// `((EELobbyGame*)a)->GetIsHost()` (0 or 1).
    GameIsHost,
    /// `((EELobbyGame*)a)->mRust`: the game's Rust object, which this object calls directly (round 6: the slots'
    /// states were ops through the C++ slot views).
    GameRust,
    /// `((ListArea*)a)->InsertItem(new ConnectListItem(GUIString(b), image, c & 0xff), -1)` (`b` the slot's name, a
    /// `GUIStringData`),
    /// the image `GameUserListCtrl_Ready_Image` for `c >> 8` 0, else `LobbyGlobal_Searching_Image`; 1 when that
    /// image is not the ready one.
    ConnectInsert,
    /// `LobbyDoDialog(this)`.
    DoDialog,
    /// `this->GetRootWindow()`.
    RootWindow,
    /// `((Window*)a)->EndDialog(b, this)`.
    EndDialog,
    /// `((ListArea*)a)->GetItem(b)`.
    ListGetItem,
    /// `((ConnectListItem*)a)->mSlot`.
    ConnectItemSlot,
    /// `((ConnectListItem*)a)->mImage = ` `GameUserListCtrl_Ready_Image` (`b` 0) or `ServerListCtrl_PingFailed_Image`.
    ConnectItemSetImage,
    /// `((ConnectListItem*)a)->mImage.mPtr != GameUserListCtrl_Ready_Image.mPtr` (0 or 1).
    ConnectItemNotReady,
    /// `((ListArea*)a)->RefreshItem((ListItem*)b)`.
    ListRefresh,
}

/// The inward calls (the shells' methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyDlgCall {
    /// `InitComponent` of a config init (`a` the `ComponentConfig*`).
    Init = 1,
    /// `HandleComponentEvent(a)`: `b` the `EeWonLobbyDlgEvent`, `c` the event's component.
    Event,
    /// `Reset()`; CreateRoom and CreateGame: `Reset(a != 0)`; AdminAction: `Reset(a, (LobbyClient*)b)`.
    Reset,
    /// the control `a` (member order) as held, or NULL (the getters read its text in C++).
    Slot,
    /// `CheckInput()` (AdminAction: `UpdateDoActionButton()`).
    CheckInput,
    /// `UpdatePasswordVisibility()` (CreateRoom, CreateGame).
    UpdatePasswordVisibility,
    /// LanPort: `SetPort(a)`.
    SetPort,
    /// LanPort: `SetGamePort(a)`.
    SetGamePort,
    /// LanPort: `GetPort()`.
    GetPort,
    /// CreateGame: `SetGameType(a)`.
    SetGameType,
    /// CreateGame: `GetGame()`, the game with a reference added.
    GetGame,
    /// CreateRoom: `GetPassword()`'s input box, or NULL for the empty string.
    GetPassword,
    /// AdminAction: `GetApplyToServer()` (0 or 1).
    ApplyToServer,
    /// AdminAction: `GetNumSeconds()`.
    NumSeconds,
    /// a scalar member: 0 the game port, 1 the game type, 2 the action type, 3 whether the reason is optional,
    /// 4 StatusDialog's start tick, 5 TOUDialog's need-to-rebuild flag, 6 BadUserDialog's simple flag, 7
    /// EELobbyConnectDialog's running flag, 8 its ended flag.
    Field,
    /// StatusDialog: `SetText(*(GUIString*)a, *(GUIString*)b, c != 0)`; OkCancelDialog: `SetText(title a, body b,
    /// ((GUIString**)c)[0], ((GUIString**)c)[1])`.
    SetText,
    /// StatusDialog: `SetStatus(*(GUIString*)a, b != 0)`.
    SetStatus,
    /// StatusDialog: `mStartTick = a`.
    SetStartTick,
    /// BadUserDialog: `SetUserList(*(const RoutingBadUserList*)a, b != 0)`.
    SetUserList,
    /// BadUserDialog: `GetIds(*(IdList*)a)`.
    GetIds,
    /// ChooseNetworkAdapterDialog: `GetSelectedAddress()`.
    SelectedAddress,
    /// CreditsDialog's constructor: `mScrollingText = (ScrollingTextComponent*)a` (held).
    HoldScrollText,
    /// CreditsDialog: `StartScrolling()`.
    StartScrolling,
    /// CreditsDialog: `StopScrolling()`.
    StopScrolling,
    /// CreditsDialog: `ResetScrolling()`.
    ResetScrolling,
    /// CreditsDialog: `AddLine(*(const GUIString*)a, (Component*)b, (unsigned long)c)`.
    AddLine,
    /// EELobbyConnectDialog: `Run((EELobbyGame*)a)` (0 or 1).
    ConnectRun,
    /// EELobbyConnectDialog: `End(a != 0)`.
    ConnectEnd,
    /// EELobbyConnectDialog: `SetConnected(a, b != 0)`.
    ConnectSetConnected,
}

/// The host: `(dialog, op, a, b, c) -> result`. `dialog` is the C++ dialog the Rust object belongs to.
pub type EeWonLobbyDlgHostFn =
    Option<unsafe extern "C" fn(dialog: *mut c_void, op: EeWonLobbyDlgOp, a: isize, b: isize, c: isize) -> isize>;

const MAX_SLOTS: usize = 10;

macro_rules! slot {
    ($name:literal, $file:literal, $line:expr, $ty:ident) => {
        EeWonLobbyDlgSlot { name: $name.as_ptr(), file: $file.as_ptr(), line: $line, ty: EeWonLobbyDlgType::$ty }
    };
}

static LAN_NAME: [EeWonLobbyDlgSlot; 3] = [
    slot!(c"LanNameInput", c"LanNameDialog.cpp", 31, Input),
    slot!(c"OkButton", c"LanNameDialog.cpp", 32, Button),
    slot!(c"CancelButton", c"LanNameDialog.cpp", 33, Button),
];
static PASSWORD: [EeWonLobbyDlgSlot; 3] = [
    slot!(c"PasswordInput", c"PasswordDialog.cpp", 24, Input),
    slot!(c"OkButton", c"PasswordDialog.cpp", 25, Button),
    slot!(c"CancelButton", c"PasswordDialog.cpp", 26, Button),
];
static ADD_FRIEND: [EeWonLobbyDlgSlot; 3] = [
    slot!(c"NameInput", c"AddFriendDialog.cpp", 25, Input),
    slot!(c"OkButton", c"AddFriendDialog.cpp", 26, Button),
    slot!(c"CancelButton", c"AddFriendDialog.cpp", 27, Button),
];
static DIRECT_CONNECT: [EeWonLobbyDlgSlot; 4] = [
    slot!(c"NameInput", c"DirectConnectDialog.cpp", 25, Input),
    slot!(c"AddressInput", c"DirectConnectDialog.cpp", 26, Input),
    slot!(c"AddButton", c"DirectConnectDialog.cpp", 27, Button),
    slot!(c"CancelButton", c"DirectConnectDialog.cpp", 28, Button),
];
static LAN_PORT: [EeWonLobbyDlgSlot; 3] = [
    slot!(c"PortInput", c"LanPortDialog.cpp", 32, Input),
    slot!(c"GamePortLabel", c"LanPortDialog.cpp", 35, Label),
    slot!(c"OKButton", c"LanPortDialog.cpp", 36, Button),
];
static CREATE_ROOM: [EeWonLobbyDlgSlot; 5] = [
    slot!(c"RoomNameInput", c"CreateRoomDialog.cpp", 25, Input),
    slot!(c"PasswordInput", c"CreateRoomDialog.cpp", 26, Input),
    slot!(c"PasswordLabel", c"CreateRoomDialog.cpp", 27, Label),
    slot!(c"PasswordCheck", c"CreateRoomDialog.cpp", 28, Button),
    slot!(c"CreateButton", c"CreateRoomDialog.cpp", 29, Button),
];
static CREATE_GAME: [EeWonLobbyDlgSlot; 9] = [
    slot!(c"GameNameInput", c"CreateGameDialog.cpp", 34, Input),
    slot!(c"SkillCombo", c"CreateGameDialog.cpp", 35, Combo),
    slot!(c"PasswordInput", c"CreateGameDialog.cpp", 36, Input),
    slot!(c"PasswordLabel", c"CreateGameDialog.cpp", 37, Label),
    slot!(c"OpenToAllRadio", c"CreateGameDialog.cpp", 38, Button),
    slot!(c"PasswordRadio", c"CreateGameDialog.cpp", 39, Button),
    slot!(c"InviteOnlyRadio", c"CreateGameDialog.cpp", 40, Button),
    slot!(c"AskToJoinRadio", c"CreateGameDialog.cpp", 41, Button),
    slot!(c"CreateButton", c"CreateGameDialog.cpp", 42, Button),
];
static ADMIN_ACTION: [EeWonLobbyDlgSlot; 10] = [
    slot!(c"TitleLabel", c"AdminActionDialog.cpp", 114, Label),
    slot!(c"ExplanationHeader", c"AdminActionDialog.cpp", 105, Label),
    slot!(c"Explanation", c"AdminActionDialog.cpp", 106, Input),
    slot!(c"DurationHeader", c"AdminActionDialog.cpp", 107, Label),
    slot!(c"DurationCombo", c"AdminActionDialog.cpp", 108, Combo),
    slot!(c"TimeSegmentCombo", c"AdminActionDialog.cpp", 109, Combo),
    slot!(c"DoActionButton", c"AdminActionDialog.cpp", 110, Button),
    slot!(c"CancelButton", c"AdminActionDialog.cpp", 111, Button),
    slot!(c"ApplyToRoomRadio", c"AdminActionDialog.cpp", 112, Button),
    slot!(c"ApplyToServerRadio", c"AdminActionDialog.cpp", 113, Button),
];
static STATUS: [EeWonLobbyDlgSlot; 6] = [
    slot!(c"TitleLabel", c"StatusDialog.cpp", 28, Label),
    slot!(c"CloseButton", c"StatusDialog.cpp", 29, Button),
    slot!(c"StatusLabel", c"StatusDialog.cpp", 30, Label),
    slot!(c"CancelButton", c"StatusDialog.cpp", 31, Button),
    slot!(c"DialogComponent", c"StatusDialog.cpp", 34, Component),
    slot!(c"Back", c"StatusDialog.cpp", 33, Frame),
];
static OK_CANCEL: [EeWonLobbyDlgSlot; 7] = [
    slot!(c"TitleLabel", c"OkCancelDialog.cpp", 26, Label),
    slot!(c"BodyLabel", c"OkCancelDialog.cpp", 28, Label),
    slot!(c"OKButton", c"OkCancelDialog.cpp", 30, Button),
    slot!(c"CancelButton", c"OkCancelDialog.cpp", 31, Button),
    slot!(c"CloseButton", c"OkCancelDialog.cpp", 27, Button),
    slot!(c"DialogComponent", c"OkCancelDialog.cpp", 34, Component),
    slot!(c"Back", c"OkCancelDialog.cpp", 33, Frame),
];
static ROOM: [EeWonLobbyDlgSlot; 0] = [];
static CREDITS: [EeWonLobbyDlgSlot; 2] = [
    slot!(c"CreditsButton", c"CreditsDialog.cpp", 271, Button),
    slot!(c"CancelButton", c"CreditsDialog.cpp", 272, Button),
];
static TOU: [EeWonLobbyDlgSlot; 4] = [
    slot!(c"Browser", c"TOUDialog.cpp", 30, Browser),
    slot!(c"AgreeCheck", c"TOUDialog.cpp", 31, Button),
    slot!(c"AcceptButton", c"TOUDialog.cpp", 32, Button),
    slot!(c"RejectButton", c"TOUDialog.cpp", 33, Button),
];
static BAD_USER: [EeWonLobbyDlgSlot; 4] = [
    slot!(c"UserList", c"BadUserDialog.cpp", 157, MultiList),
    slot!(c"SimpleUserList", c"BadUserDialog.cpp", 158, List),
    slot!(c"UserListBox", c"BadUserDialog.cpp", 159, Component),
    slot!(c"SimpleUserListBox", c"BadUserDialog.cpp", 160, Component),
];
static CHOOSE_ADAPTER: [EeWonLobbyDlgSlot; 2] = [
    slot!(c"AdapterList", c"ChooseNetworkAdapterDialog.cpp", 60, MultiList),
    slot!(c"ActivateButton", c"ChooseNetworkAdapterDialog.cpp", 61, Button),
];
static CONNECT: [EeWonLobbyDlgSlot; 2] = [
    slot!(c"ConnectList", c"EELobbyConnectDialog.cpp", 61, List),
    slot!(c"CancelButton", c"EELobbyConnectDialog.cpp", 62, Button),
];
/// StatusDialog looked its frame up before its body.
const STATUS_INIT_ORDER: [usize; 6] = [0, 1, 2, 3, 5, 4];
/// OkCancelDialog looked up its title, close button, body, buttons, frame, then its body component.
const OK_CANCEL_INIT_ORDER: [usize; 7] = [0, 4, 1, 2, 3, 6, 5];
/// AdminActionDialog looked its title up last.
const ADMIN_INIT_ORDER: [usize; 10] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 0];

/// A kind's controls in member order (the order the `SmartPtr` members were destroyed in reverse).
pub fn slots(kind: EeWonLobbyDlgKind) -> &'static [EeWonLobbyDlgSlot] {
    match kind {
        EeWonLobbyDlgKind::LanName => &LAN_NAME,
        EeWonLobbyDlgKind::Password => &PASSWORD,
        EeWonLobbyDlgKind::AddFriend => &ADD_FRIEND,
        EeWonLobbyDlgKind::DirectConnect => &DIRECT_CONNECT,
        EeWonLobbyDlgKind::LanPort => &LAN_PORT,
        EeWonLobbyDlgKind::CreateRoom => &CREATE_ROOM,
        EeWonLobbyDlgKind::CreateGame => &CREATE_GAME,
        EeWonLobbyDlgKind::AdminAction => &ADMIN_ACTION,
        EeWonLobbyDlgKind::Status => &STATUS,
        EeWonLobbyDlgKind::OkCancel => &OK_CANCEL,
        EeWonLobbyDlgKind::Room => &ROOM,
        EeWonLobbyDlgKind::Tou => &TOU,
        EeWonLobbyDlgKind::BadUser => &BAD_USER,
        EeWonLobbyDlgKind::ChooseAdapter => &CHOOSE_ADAPTER,
        EeWonLobbyDlgKind::Credits => &CREDITS,
        EeWonLobbyDlgKind::Connect => &CONNECT,
    }
}

/// A slot's name (for the tests' traces).
pub fn slot_name(slot: &EeWonLobbyDlgSlot) -> &'static str {
    // SAFETY: the table's names are static C string literals
    unsafe { CStr::from_ptr(slot.name) }.to_str().unwrap_or("")
}

use EeWonLobbyDlgOp as Op;

// the controls of each kind, by member index
const S_INPUT: usize = 0; // the first input of every kind
const LP_LABEL: usize = 1;
const LP_OK: usize = 2;
const AF_OK: usize = 1;
const DC_ADDRESS: usize = 1;
const CR_PASSWORD: usize = 1;
const CR_LABEL: usize = 2;
const CR_CHECK: usize = 3;
const CR_CREATE: usize = 4;
const CG_SKILL: usize = 1;
const CG_PASSWORD: usize = 2;
const CG_LABEL: usize = 3;
const CG_OPEN: usize = 4;
const CG_PASSWORD_RADIO: usize = 5;
const CG_INVITE: usize = 6;
const CG_ASK: usize = 7;
const CG_CREATE: usize = 8;
const AA_TITLE: usize = 0;
const AA_HEADER: usize = 1;
const AA_EXPLANATION: usize = 2;
const AA_DURATION_HEADER: usize = 3;
const AA_DURATION: usize = 4;
const AA_SEGMENT: usize = 5;
const AA_DO: usize = 6;
const AA_ROOM: usize = 8;
const AA_SERVER: usize = 9;
const ST_TITLE: usize = 0;
const ST_CLOSE: usize = 1;
const ST_STATUS: usize = 2;
const ST_CANCEL: usize = 3;
const ST_BODY: usize = 4;
const ST_BACK: usize = 5;
const OC_TITLE: usize = 0;
const OC_BODY: usize = 1;
const OC_OK: usize = 2;
const OC_CANCEL: usize = 3;
const OC_CLOSE: usize = 4;
const OC_COMPONENT: usize = 5;
const OC_BACK: usize = 6;
const TOU_BROWSER: usize = 0;
const TOU_AGREE: usize = 1;
const TOU_ACCEPT: usize = 2;
const TOU_REJECT: usize = 3;
const BU_LIST: usize = 0;
const BU_SIMPLE_LIST: usize = 1;
const BU_BOX: usize = 2;
const BU_SIMPLE_BOX: usize = 3;
const AD_LIST: usize = 0;
const CD_CREDITS: usize = 0;
const CN_LIST: usize = 0;
const CN_CANCEL: usize = 1;
/// The dialogs' OK (100001) and Cancel (100002) results.
const DIALOG_OK: isize = 100001;
const DIALOG_CANCEL: isize = 100002;
/// `HTTPDocType_TOU` (WONRouting.h): the terms of use document.
const HTTPDOC_TYPE_TOU: isize = 1;

/// One lobby dialog's Rust half.
pub struct EeWonLobbyDlg {
    dialog: *mut c_void,
    host: EeWonLobbyDlgHostFn,
    kind: EeWonLobbyDlgKind,
    controls: [Cell<isize>; MAX_SLOTS],
    /// LanPortDialog's `mGamePort`.
    game_port: Cell<u16>,
    /// CreateGameDialog's `mGameType` (the constructor set 0).
    game_type: Cell<i32>,
    /// AdminActionDialog's `mActionType` (the constructor left it unset: 0 here).
    action_type: Cell<i32>,
    /// AdminActionDialog's `mIsReasonOptional` (unset by the constructor: false here).
    reason_optional: Cell<bool>,
    /// StatusDialog's `mStartTick` (its constructor's tick, set through `SetStartTick`).
    start_tick: Cell<u32>,
    /// TOUDialog's `mNeedGenHTML` (false from its constructor).
    need_gen_html: Cell<bool>,
    /// BadUserDialog's `mIsSimple` (false from its constructor).
    is_simple: Cell<bool>,
    /// CreditsDialog's `mScrollingText` (its first member: released after the controls).
    scroll_text: Cell<isize>,
    /// EELobbyConnectDialog's `mIsRunning` (false from its constructor).
    running: Cell<bool>,
    /// EELobbyConnectDialog's `mIsEnded` (unset by its constructor: false here).
    ended: Cell<bool>,
}

impl EeWonLobbyDlg {
    pub fn new(dialog: *mut c_void, kind: EeWonLobbyDlgKind, host: EeWonLobbyDlgHostFn) -> Self {
        Self {
            dialog,
            host,
            kind,
            controls: Default::default(),
            game_port: Cell::new(0),
            game_type: Cell::new(0),
            action_type: Cell::new(0),
            reason_optional: Cell::new(false),
            start_tick: Cell::new(0),
            need_gen_html: Cell::new(false),
            is_simple: Cell::new(false),
            scroll_text: Cell::new(0),
            running: Cell::new(false),
            ended: Cell::new(false),
        }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize) -> isize {
        match self.host {
            // SAFETY: the host is the C++ dialog's (or a test's), called with the dialog it was given for
            Some(f) => unsafe { f(self.dialog, op, a, b, c) },
            None => 0,
        }
    }

    fn ctl(&self, i: usize) -> isize {
        self.controls[i].get()
    }

    /// An op on control `i`.
    fn on(&self, i: usize, op: Op, b: isize, c: isize) -> isize {
        self.op(op, self.ctl(i), b, c)
    }

    fn flag(&self, i: usize, op: Op) -> bool {
        self.on(i, op, 0, 0) != 0
    }

    /// `InitComponent` for a config init: every control looked up in the original's order (`SmartPtr`
    /// assignment: the new one held before the old one is released).
    fn init(&self, config: isize) {
        let table = slots(self.kind);
        let order: Vec<usize> = match self.kind {
            EeWonLobbyDlgKind::AdminAction => ADMIN_INIT_ORDER.to_vec(),
            EeWonLobbyDlgKind::Status => STATUS_INIT_ORDER.to_vec(),
            EeWonLobbyDlgKind::OkCancel => OK_CANCEL_INIT_ORDER.to_vec(),
            _ => (0..table.len()).collect(),
        };
        for i in order {
            let found = self.op(Op::Find, config, &table[i] as *const EeWonLobbyDlgSlot as isize, 0);
            let old = self.controls[i].replace(found);
            if old != 0 {
                self.op(Op::Release, old, 0, 0);
            }
            if self.kind == EeWonLobbyDlgKind::LanPort && i == S_INPUT {
                self.on(S_INPUT, Op::InputSetNumeric, 0, 0);
            }
        }
        if self.kind == EeWonLobbyDlgKind::CreateRoom {
            self.check_input();
        }
        if self.kind == EeWonLobbyDlgKind::Credits {
            // the scrolling text in place of the hidden credits button (UpdateCredits was empty)
            self.op(Op::CreditsPlace, self.scroll_text.get(), self.ctl(CD_CREDITS), 0);
            self.op(Op::AddChild, self.scroll_text.get(), 0, 0);
            self.on(CD_CREDITS, Op::SetVisible, 0, 0);
        }
        if self.kind == EeWonLobbyDlgKind::Tou {
            self.on(TOU_ACCEPT, Op::ButtonSetControlId, 1, 0);
            self.on(TOU_REJECT, Op::ButtonSetControlId, -1, 0);
            self.tou_reset();
            self.me(Op::SubscribeBroadcast, 1, 0);
        }
    }

    /// TOUDialog::Reset: the terms page rebuilt if new terms arrived, scrolled to the top, agree unticked, accept
    /// disabled.
    fn tou_reset(&self) {
        if self.need_gen_html.get() {
            self.need_gen_html.set(false);
            self.on(TOU_BROWSER, Op::BrowserShowTou, 0, 0);
        }
        self.on(TOU_BROWSER, Op::BrowserTop, 0, 0);
        self.on(TOU_AGREE, Op::ButtonSetCheck, 0, 0);
        self.on(TOU_ACCEPT, Op::SetEnabled, 0, 0);
    }

    /// BadUserDialog's list in use (`mIsSimple`).
    fn bad_user_list(&self) -> isize {
        self.ctl(if self.is_simple.get() { BU_SIMPLE_LIST } else { BU_LIST })
    }

    /// BadUserDialog::SetUserList: the full or the simple list filled and shown alone.
    fn set_user_list(&self, users: isize, simple: bool) {
        self.is_simple.set(simple);
        self.on(BU_BOX, Op::SetVisible, !simple as isize, 0);
        self.on(BU_SIMPLE_BOX, Op::SetVisible, simple as isize, 0);
        let list = self.bad_user_list();
        self.op(Op::SetVisible, list, 1, 0);
        self.op(Op::ListBeginUpdate, list, 0, 0);
        self.op(Op::ListClear, list, 1, 0);
        self.op(Op::BadUsersInsert, list, users, simple as isize);
        self.op(Op::ListEndUpdate, list, 0, 0);
    }

    /// BadUserDialog::GetIds: the selected users' WON ids.
    fn get_ids(&self, ids: isize) {
        let list = self.bad_user_list();
        self.op(Op::IdsClear, ids, 0, 0);
        self.op(Op::ListRewind, list, 0, 0);
        while self.op(Op::ListHasMore, list, 0, 0) != 0 {
            let item = self.op(Op::ListNextSel, list, 0, 0);
            let id = self.op(Op::BadUserId, item, 0, 0);
            self.op(Op::IdsPush, ids, id, 0);
        }
    }

    /// ChooseNetworkAdapterDialog::Reset: the default row and one row per adapter (name, address), the one in use
    /// selected.
    fn adapter_reset(&self) {
        let list = self.ctl(AD_LIST);
        self.op(Op::ListBeginUpdate, list, 0, 0);
        self.op(Op::ListClear, list, 0, 0);
        self.op(Op::AdapterAddDefault, list, 0, 0);
        if self.op(Op::LocalHost, 0, 0, 0) == 0 {
            self.op(Op::ListSetSel, list, 0, 0);
        }
        let addresses = self.op(Op::AdaptersGet, 0, 0, 0);
        let count = self.op(Op::AdaptersCount, addresses, 0, 0);
        for i in 0..count {
            // the original's `long` handle, compared with the local host's `long`
            let handle = self.op(Op::AdapterHandle, addresses, i, 0);
            let unnamed = self.op(Op::AdapterNameLength, addresses, i, 0) == 0;
            self.op(Op::AdapterInsertRow, list, handle, 0);
            if unnamed {
                self.op(Op::AdapterSetUnknownName, list, 0, 0);
            } else {
                self.op(Op::AdapterSetName, list, addresses, i);
            }
            self.op(Op::AdapterSetAddress, list, handle, 0);
            if self.op(Op::LocalHost, 0, 0, 0) == handle {
                let last = self.op(Op::ListNumItems, list, 0, 0).wrapping_sub(1);
                self.op(Op::ListSetSel, list, last, 1);
            }
        }
        self.op(Op::AdaptersFree, addresses, 0, 0);
        self.op(Op::ListEndUpdate, list, 0, 0);
    }

    /// ChooseNetworkAdapterDialog::GetSelectedAddress: the selected row's address, else the local host.
    fn selected_address(&self) -> u32 {
        let item = self.op(Op::ListGetSel, self.ctl(AD_LIST), 0, 0);
        let host = if item != 0 { Some(self.op(Op::AdapterHost, item, 0, 0) as u32) } else { None };
        let local = self.op(Op::LocalHost, 0, 0, 0) as u32;
        dialogs::selected_address(host, local)
    }

    fn event(&self, event: isize, kind: u32, component: isize) {
        let text_changed = kind == EeWonLobbyDlgEvent::InputTextChanged as u32;
        match self.kind {
            EeWonLobbyDlgKind::Tou if kind == EeWonLobbyDlgEvent::GotHttpDocument as u32 => {
                // HandleHTTPDocEvent: a terms of use download marks the page for rebuilding
                if self.op(Op::HttpDocType, event, 0, 0) == HTTPDOC_TYPE_TOU {
                    self.need_gen_html.set(true);
                }
            }
            EeWonLobbyDlgKind::Tou
                if kind == EeWonLobbyDlgEvent::ButtonPressed as u32 && component == self.ctl(TOU_AGREE) =>
            {
                let checked = self.flag(TOU_AGREE, Op::ButtonIsChecked);
                self.on(TOU_ACCEPT, Op::SetEnabled, checked as isize, 0);
            }
            EeWonLobbyDlgKind::AddFriend if text_changed => self.check_input(),
            EeWonLobbyDlgKind::LanPort if text_changed && component == self.ctl(S_INPUT) => self.check_input(),
            EeWonLobbyDlgKind::CreateRoom
                if kind == EeWonLobbyDlgEvent::ButtonPressed as u32 && component == self.ctl(CR_CHECK) =>
            {
                self.update_password_visibility()
            }
            EeWonLobbyDlgKind::CreateRoom if text_changed => self.check_input(),
            EeWonLobbyDlgKind::CreateGame if kind == EeWonLobbyDlgEvent::ButtonPressed as u32 => {
                // the original fell through to the dialog's handler after the update
                self.update_password_visibility();
                self.op(Op::BaseEvent, event, 0, 0);
            }
            EeWonLobbyDlgKind::CreateGame if text_changed => self.check_input(),
            EeWonLobbyDlgKind::CreateGame
                if kind == EeWonLobbyDlgEvent::ComboSelChanged as u32 && component == self.ctl(CG_SKILL) =>
            {
                self.check_input()
            }
            EeWonLobbyDlgKind::AdminAction if text_changed && component == self.ctl(AA_EXPLANATION) => {
                self.check_input()
            }
            _ => {
                self.op(Op::BaseEvent, event, 0, 0);
            }
        }
    }

    fn reset(&self, a: isize, b: isize) {
        match self.kind {
            EeWonLobbyDlgKind::LanName | EeWonLobbyDlgKind::Password => {
                self.on(S_INPUT, Op::InputClear, 0, 0);
                self.on(S_INPUT, Op::RequestFocus, 0, 0);
            }
            EeWonLobbyDlgKind::AddFriend => {
                self.on(S_INPUT, Op::InputClear, 1, 0);
                self.on(S_INPUT, Op::RequestFocus, 0, 0);
            }
            EeWonLobbyDlgKind::DirectConnect => {
                self.on(S_INPUT, Op::RequestFocus, 0, 0);
                self.on(S_INPUT, Op::InputClear, 0, 0);
                self.on(DC_ADDRESS, Op::InputClear, 0, 0);
            }
            EeWonLobbyDlgKind::LanPort => {
                self.on(S_INPUT, Op::RequestFocus, 0, 0);
                self.check_input();
            }
            EeWonLobbyDlgKind::CreateRoom => {
                self.on(S_INPUT, Op::RequestFocus, 0, 0);
                if a != 0 {
                    self.on(S_INPUT, Op::InputClear, 0, 0);
                    self.on(CR_PASSWORD, Op::InputClear, 0, 0);
                    self.on(CR_CHECK, Op::ButtonSetCheck, 0, 0);
                    self.update_password_visibility();
                }
                self.check_input();
            }
            EeWonLobbyDlgKind::CreateGame => {
                self.on(S_INPUT, Op::RequestFocus, 0, 0);
                if a != 0 {
                    self.on(CG_OPEN, Op::ButtonSetCheck, 1, 0);
                    self.on(CG_SKILL, Op::ComboSetSel, 0, 0);
                    self.on(S_INPUT, Op::InputClear, 0, 0);
                    self.on(CG_PASSWORD, Op::InputClear, 0, 0);
                    self.update_password_visibility();
                }
                self.check_input();
            }
            EeWonLobbyDlgKind::AdminAction => self.admin_reset(a as i32, b),
            EeWonLobbyDlgKind::Tou => self.tou_reset(),
            EeWonLobbyDlgKind::ChooseAdapter => self.adapter_reset(),
            EeWonLobbyDlgKind::Status
            | EeWonLobbyDlgKind::OkCancel
            | EeWonLobbyDlgKind::Room
            | EeWonLobbyDlgKind::BadUser
            | EeWonLobbyDlgKind::Credits
            | EeWonLobbyDlgKind::Connect => {}
        }
    }

    fn admin_reset(&self, action: i32, target: isize) {
        self.action_type.set(action);
        self.on(AA_SERVER, Op::ButtonSetCheck, 0, 0);
        self.on(AA_ROOM, Op::ButtonSetCheck, 1, 0);
        self.on(AA_EXPLANATION, Op::InputSetEmpty, 0, 0);
        self.on(AA_DURATION, Op::ComboSetSel, 1, 0);
        self.on(AA_SEGMENT, Op::ComboSetSel, 0, 0);
        // (text, show duration, enable reason, reason optional, show scope); another action left the original's
        // locals unset and its reason flag unchanged: hidden and disabled here
        use EeWonLobbyDlgString as S;
        let (text, duration, reason, scope) = match action {
            15 => (S::Warn, false, true, Some(false)),
            8 => (S::Mute, true, true, Some(true)),
            14 => (S::UnMute, false, false, Some(true)),
            7 => (S::Kick, false, true, Some(true)),
            2 => (S::Ban, true, true, Some(false)),
            _ => (S::Empty, false, false, None),
        };
        let show_scope = matches!(action, 8 | 14 | 2);
        if let Some(optional) = scope {
            self.reason_optional.set(optional);
        }
        self.on(AA_DURATION_HEADER, Op::SetVisible, duration as isize, 0);
        self.on(AA_DURATION, Op::SetVisible, duration as isize, 0);
        self.on(AA_SEGMENT, Op::SetVisible, duration as isize, 0);
        self.on(AA_SERVER, Op::SetVisible, show_scope as isize, 0);
        self.on(AA_ROOM, Op::SetVisible, show_scope as isize, 0);
        self.on(AA_EXPLANATION, Op::SetEnabled, reason as isize, 0);
        if reason {
            self.on(AA_EXPLANATION, Op::RequestFocus, 0, 0);
        }
        self.check_input();
        self.on(AA_TITLE, Op::AdminTitle, text as isize, target);
        let header = if self.reason_optional.get() { S::ExplanationOpt } else { S::ExplanationReq };
        self.on(AA_HEADER, Op::LabelSetString, header as isize, 0);
        self.on(AA_DO, Op::ButtonSetString, text as isize, 0);
        self.op(Op::Relayout, 0, 0, 0);
    }

    fn check_input(&self) {
        match self.kind {
            EeWonLobbyDlgKind::AddFriend => {
                let len = self.on(S_INPUT, Op::InputShownLength, 0, 0) as u32;
                self.on(AF_OK, Op::SetEnabled, dialogs::name_enabled(len) as isize, 0);
            }
            EeWonLobbyDlgKind::LanPort => {
                let port = self.on(S_INPUT, Op::InputToInt, 0, 0) as i32;
                if dialogs::lan_port_ok(port, self.game_port.get()) {
                    if self.flag(LP_OK, Op::ButtonIsDisabled) {
                        self.on(LP_OK, Op::SetEnabled, 1, 0);
                    }
                } else if !self.flag(LP_OK, Op::ButtonIsDisabled) {
                    self.on(LP_OK, Op::SetEnabled, 0, 0);
                }
            }
            EeWonLobbyDlgKind::CreateRoom => {
                let name = self.on(S_INPUT, Op::InputShownLength, 0, 0) as u32;
                let shown = self.flag(CR_PASSWORD, Op::IsVisible);
                let password = self.on(CR_PASSWORD, Op::InputShownLength, 0, 0) as u32;
                self.on(CR_CREATE, Op::SetEnabled, dialogs::create_enabled(name, shown, password) as isize, 0);
            }
            EeWonLobbyDlgKind::CreateGame => {
                let name = self.on(S_INPUT, Op::InputTextLength, 0, 0) as u32;
                let shown = self.flag(CG_PASSWORD, Op::IsVisible);
                let password = self.on(CG_PASSWORD, Op::InputShownLength, 0, 0) as u32;
                self.on(CG_CREATE, Op::SetEnabled, dialogs::create_enabled(name, shown, password) as isize, 0);
            }
            EeWonLobbyDlgKind::AdminAction => {
                // UpdateDoActionButton: the text is read first, the flag only when it is empty
                let enable = self.on(AA_EXPLANATION, Op::InputTextLength, 0, 0) != 0 || self.reason_optional.get();
                self.on(AA_DO, Op::SetEnabled, enable as isize, 0);
            }
            _ => {}
        }
    }

    fn update_password_visibility(&self) {
        let (input, label, check) = match self.kind {
            EeWonLobbyDlgKind::CreateRoom => (CR_PASSWORD, CR_LABEL, CR_CHECK),
            EeWonLobbyDlgKind::CreateGame => (CG_PASSWORD, CG_LABEL, CG_PASSWORD_RADIO),
            _ => return,
        };
        let checked = self.flag(check, Op::ButtonIsChecked);
        self.on(input, Op::SetVisible, checked as isize, 0);
        let checked = self.flag(check, Op::ButtonIsChecked);
        self.on(label, Op::SetVisible, checked as isize, 0);
        self.check_input();
    }

    fn set_game_type(&self, game_type: i32) {
        self.game_type.set(game_type);
        let internet = dialogs::internet_access(game_type);
        if self.flag(CG_INVITE, Op::ButtonIsChecked) || self.flag(CG_ASK, Op::ButtonIsChecked) {
            self.on(CG_OPEN, Op::ButtonSetCheck, 1, 0);
        }
        self.on(CG_INVITE, Op::SetVisible, internet as isize, 0);
        self.on(CG_ASK, Op::SetVisible, internet as isize, 0);
    }

    fn get_game(&self) -> isize {
        let game = self.op(Op::GameCreate, 0, self.game_type.get() as isize, 0);
        self.op(Op::GameSetName, game, self.ctl(S_INPUT), 0);
        if self.flag(CG_PASSWORD_RADIO, Op::ButtonIsChecked) {
            self.op(Op::GameSetPassword, game, self.ctl(CG_PASSWORD), 0);
        }
        let ask = self.on(CG_ASK, Op::ButtonIsChecked, 0, 0);
        self.op(Op::GameSetAskToJoin, game, ask, 0);
        let invite = self.on(CG_INVITE, Op::ButtonIsChecked, 0, 0);
        self.op(Op::GameSetInviteOnly, game, invite, 0);
        let sel = self.on(CG_SKILL, Op::ComboGetSel, 0, 0) as i32;
        self.op(Op::GameSetSkill, game, dialogs::skill_level(sel) as isize, 0);
        game
    }

    fn num_seconds(&self) -> u32 {
        let mut seconds: u32 = match self.on(AA_DURATION, Op::ComboGetSel, 0, 0) as i32 {
            0 => 1,
            1 => 5,
            2 => 10,
            3 => 30,
            4 => 0,
            _ => 3600,
        };
        // the original's switch fell through: days are 24 hours of 60 minutes of 60 seconds
        let segment = self.on(AA_SEGMENT, Op::ComboGetSel, 0, 0) as i32;
        if segment == 2 {
            seconds = seconds.wrapping_mul(24);
        }
        if (1..=2).contains(&segment) {
            seconds = seconds.wrapping_mul(60);
        }
        if (0..=2).contains(&segment) {
            seconds = seconds.wrapping_mul(60);
        }
        seconds
    }

    /// One inward call.
    pub fn call(&self, call: EeWonLobbyDlgCall, a: isize, b: isize, c: isize) -> isize {
        use EeWonLobbyDlgCall as C;
        match call {
            C::Init => self.init(a),
            C::Event => self.event(a, b as u32, c),
            C::Reset => self.reset(a, b),
            C::Slot => return if (a as usize) < MAX_SLOTS { self.ctl(a as usize) } else { 0 },
            C::CheckInput => self.check_input(),
            C::UpdatePasswordVisibility => self.update_password_visibility(),
            C::SetPort => {
                self.on(S_INPUT, Op::InputSetNumber, a as u16 as isize, 1);
                self.on(S_INPUT, Op::InputSelectRange, 0, -1);
            }
            C::SetGamePort => {
                self.game_port.set(a as u16);
                self.on(LP_LABEL, Op::LabelSetNumber, a as u16 as isize, 0);
            }
            C::GetPort => return self.on(S_INPUT, Op::InputToInt, 0, 0) as u16 as isize,
            C::SetGameType => self.set_game_type(a as i32),
            C::GetGame => return self.get_game(),
            C::GetPassword => {
                return if self.flag(CR_CHECK, Op::ButtonIsChecked) { self.ctl(CR_PASSWORD) } else { 0 };
            }
            C::ApplyToServer => return self.on(AA_SERVER, Op::ButtonIsChecked, 0, 0),
            C::NumSeconds => return self.num_seconds() as isize,
            C::Field => {
                return match a {
                    0 => self.game_port.get() as isize,
                    1 => self.game_type.get() as isize,
                    2 => self.action_type.get() as isize,
                    3 => self.reason_optional.get() as isize,
                    4 => self.start_tick.get() as isize,
                    5 => self.need_gen_html.get() as isize,
                    6 => self.is_simple.get() as isize,
                    7 => self.running.get() as isize,
                    8 => self.ended.get() as isize,
                    _ => 0,
                }
            }
            C::SetText => match self.kind {
                EeWonLobbyDlgKind::Status => self.status_set_text(a, b, c != 0),
                EeWonLobbyDlgKind::OkCancel => {
                    let (ok, cancel) = if c != 0 {
                        let p = c as *const isize;
                        // SAFETY: the caller's two string pointers (the contract)
                        unsafe { (*p, *p.add(1)) }
                    } else {
                        (0, 0)
                    };
                    self.ok_cancel_set_text(a, b, ok, cancel)
                }
                _ => {}
            },
            C::SetStatus => {
                if self.kind == EeWonLobbyDlgKind::Status {
                    let title = self.on(ST_TITLE, Op::TitleText, 0, 0);
                    self.status_set_text(title, a, b != 0);
                }
            }
            C::SetStartTick => self.start_tick.set(a as u32),
            C::SetUserList => {
                if self.kind == EeWonLobbyDlgKind::BadUser {
                    self.set_user_list(a, b != 0)
                }
            }
            C::GetIds => {
                if self.kind == EeWonLobbyDlgKind::BadUser {
                    self.get_ids(a)
                }
            }
            C::SelectedAddress => {
                if self.kind == EeWonLobbyDlgKind::ChooseAdapter {
                    return self.selected_address() as isize;
                }
            }
            C::HoldScrollText => {
                // the SmartPtr assignment: nothing for the same pointer, else the old one released, the new one held
                let old = self.scroll_text.get();
                if old != a {
                    if old != 0 {
                        self.op(Op::Release, old, 0, 0);
                    }
                    self.scroll_text.set(a);
                    if a != 0 {
                        self.op(Op::AddRef, a, 0, 0);
                    }
                }
            }
            C::StartScrolling => {
                self.op(Op::ScrollStart, self.scroll_text.get(), 0, 0);
            }
            C::StopScrolling => {
                self.op(Op::ScrollStop, self.scroll_text.get(), 0, 0);
            }
            C::ResetScrolling => {
                self.op(Op::ScrollReset, self.scroll_text.get(), 0, 0);
            }
            C::AddLine => self.credits_add_line(a, b, c as u32),
            C::ConnectRun => {
                if self.kind == EeWonLobbyDlgKind::Connect {
                    return self.connect_run(a) as isize;
                }
            }
            C::ConnectEnd => {
                if self.kind == EeWonLobbyDlgKind::Connect {
                    self.connect_end(a != 0)
                }
            }
            C::ConnectSetConnected => {
                if self.kind == EeWonLobbyDlgKind::Connect {
                    self.connect_set_connected(a as u8, b != 0)
                }
            }
        }
        0
    }

    /// EELobbyConnectDialog::Run: the game's seated players listed (the host ready, the others searching); unless
    /// all are ready, the dialog runs until every player connected (OK) or it is cancelled.
    fn connect_run(&self, game: isize) -> bool {
        if self.running.get() {
            return false;
        }
        let mut all = true;
        let host = self.op(Op::GameIsHost, game, 0, 0) != 0;
        let text = if host { EeWonLobbyDlgString::AbortLaunch } else { EeWonLobbyDlgString::LeaveGame };
        self.on(CN_CANCEL, Op::ButtonSetString, text as isize, 0);
        self.on(CN_LIST, Op::ListBeginUpdate, 0, 0);
        self.on(CN_LIST, Op::ListClear, 1, 0);
        // GetPlayerSlot(i) of 0..7 is never NULL
        for i in 0..8 {
            let handle = self.op(Op::GameRust, game, 0, 0);
            if game_call(handle, GC::SlotRawOption, i, 0, 0) == 3 {
                let searching = (i != 0) as isize;
                let name = game_call(handle, GC::SlotGetName, i, 0, 0);
                if self.op(Op::ConnectInsert, self.ctl(CN_LIST), name, i | searching << 8) != 0 {
                    all = false;
                }
            }
        }
        self.on(CN_LIST, Op::ListEndUpdate, 0, 0);
        if all {
            return true;
        }
        self.on(CN_LIST, Op::RequestFocus, 0, 0);
        self.ended.set(false);
        self.running.set(true);
        let result = self.me(Op::DoDialog, 0, 0);
        self.running.set(false);
        result == DIALOG_OK
    }

    /// EELobbyConnectDialog::End: the dialog closed as launched or cancelled.
    fn connect_end(&self, ok: bool) {
        let root = self.me(Op::RootWindow, 0, 0);
        if root != 0 {
            self.ended.set(true);
            self.op(Op::EndDialog, root, if ok { DIALOG_OK } else { DIALOG_CANCEL }, 0);
        }
    }

    /// EELobbyConnectDialog::SetConnected: the slot's line marked connected or failed; the launch goes ahead once
    /// every listed player is connected.
    fn connect_set_connected(&self, slot: u8, connected: bool) {
        if !self.running.get() {
            return;
        }
        let mut all = true;
        // the original's SmartPtr<ListItem>
        let mut changed = 0;
        let mut i = 0;
        while i < self.on(CN_LIST, Op::ListNumItems, 0, 0) as i32 {
            let item = self.on(CN_LIST, Op::ListGetItem, i as isize, 0);
            if self.op(Op::ConnectItemSlot, item, 0, 0) as u8 == slot {
                self.op(Op::ConnectItemSetImage, item, (!connected) as isize, 0);
                if changed != item {
                    if changed != 0 {
                        self.op(Op::Release, changed, 0, 0);
                    }
                    changed = item;
                    if item != 0 {
                        self.op(Op::AddRef, item, 0, 0);
                    }
                }
            }
            if self.op(Op::ConnectItemNotReady, item, 0, 0) != 0 {
                all = false;
            }
            i += 1;
        }
        if changed != 0 {
            self.on(CN_LIST, Op::ListRefresh, changed, 0);
        }
        if all {
            self.connect_end(true);
        }
        if changed != 0 {
            self.op(Op::Release, changed, 0, 0);
        }
    }

    /// An op on the dialog itself.
    fn me(&self, op: Op, b: isize, c: isize) -> isize {
        self.op(op, self.dialog as isize, b, c)
    }

    fn int(&self, i: usize, op: Op) -> i32 {
        self.on(i, op, 0, 0) as i32
    }

    /// `StatusDialog::SetText`: title, message and Cancel/OK button; the dialog sized to the message (within 50 ms
    /// of its start tick it never shrinks).
    fn status_set_text(&self, title: isize, status: isize, cancel: bool) {
        use EeWonLobbyDlgString as S;
        let dragging = self.me(Op::IsDragging, 0, 0);
        let initial_width = self.me(Op::Width, 0, 0) as i32;
        let initial_height = self.me(Op::Height, 0, 0) as i32;
        let initial_status_width = self.int(ST_STATUS, Op::Width);
        let initial_status_height = self.int(ST_STATUS, Op::Height);
        let initial_body_width = self.int(ST_BODY, Op::Width);
        let initial_body_height = self.int(ST_BODY, Op::Height);
        let initial_button_y = self.int(ST_CANCEL, Op::Top);
        self.on(ST_TITLE, Op::LabelSetText, title, 0);
        self.on(ST_TITLE, Op::SetDesiredWidth, 0, 0);
        self.on(ST_CANCEL, Op::ButtonSetString, if cancel { S::Cancel } else { S::Ok } as isize, 0);
        self.on(ST_CANCEL, Op::SetDesiredSize, 0, 0);
        self.on(ST_CANCEL, Op::SetSizeAtLeast, 100, 0);
        self.on(ST_STATUS, Op::SetVertPad, 0, 0);
        self.on(ST_STATUS, Op::LabelSetText, 0, 0);
        self.on(ST_STATUS, Op::SetSize, 0, 0);
        self.me(Op::SetSize, 0, 0);
        let least = self
            .int(ST_TITLE, Op::Width)
            .wrapping_add(self.int(ST_CLOSE, Op::Width))
            .wrapping_add(self.int(ST_BACK, Op::FrameRight))
            .wrapping_add(self.int(ST_BACK, Op::FrameLeft))
            .wrapping_add(5);
        self.me(Op::SetSizeAtLeast, least as isize, 0);
        self.on(ST_BODY, Op::SetSize, 0, 0);
        self.on(ST_BODY, Op::SetDesiredSize, 0, 0);
        self.me(Op::SetDesiredSize, 20, 20);
        let old_width = self.me(Op::Width, 0, 0) as i32;
        let old_height = self.me(Op::Height, 0, 0) as i32;
        let old_status_height = self.int(ST_STATUS, Op::Height);
        let root_width = self.op(Op::RootWidth, 0, 0, 0) as i32;
        let root_height = self.op(Op::RootHeight, 0, 0, 0) as i32;
        let text_width = self.on(ST_STATUS, Op::FontStringWidth, status, 0) as i32;
        let m_width = self.on(ST_STATUS, Op::FontCharWidth, b'M' as isize, 0) as i32;
        let body_width = dialogs::status_body_width(text_width, m_width, root_width);
        let mut width_change = dialogs::growth(body_width, self.int(ST_STATUS, Op::Width));
        let h = self.int(ST_STATUS, Op::Height);
        self.on(ST_STATUS, Op::SetSize, body_width as isize, h as isize);
        self.on(ST_STATUS, Op::LabelSetText, status, 0);
        let label_height = self.int(ST_STATUS, Op::Height);
        let line_height = self.int(ST_STATUS, Op::FontHeight);
        if let Some(pad) = dialogs::status_pad(label_height, line_height) {
            self.on(ST_STATUS, Op::SetVertPad, pad as isize, pad as isize);
            self.on(ST_STATUS, Op::SetDesiredSize, 0, 0);
        }
        let mut height_change = dialogs::growth(self.int(ST_STATUS, Op::Height), old_status_height);
        let new_width = old_width.wrapping_add(width_change);
        let new_height = height_change.wrapping_add(old_height);
        if let Some(extra) = dialogs::status_extra_width(new_width, new_height, root_height, width_change, old_width) {
            width_change = width_change.wrapping_add(extra);
            let h = self.int(ST_STATUS, Op::Height);
            self.on(ST_STATUS, Op::SetSize, body_width.wrapping_add(extra) as isize, h as isize);
            height_change = self.int(ST_STATUS, Op::Height).wrapping_sub(old_status_height);
        }
        self.me(
            Op::SetSize,
            width_change.wrapping_add(old_width) as isize,
            height_change.wrapping_add(old_height) as isize,
        );
        let now = self.op(Op::TickCount, 0, 0, 0) as u32;
        if dialogs::status_young(now, self.start_tick.get()) {
            self.on(ST_STATUS, Op::SetSizeAtLeast, initial_status_width as isize, initial_status_height as isize);
            self.on(ST_BODY, Op::SetSizeAtLeast, initial_body_width as isize, initial_body_height as isize);
            self.me(Op::SetSizeAtLeast, initial_width as isize, initial_height as isize);
            if initial_button_y > self.int(ST_CANCEL, Op::Top) {
                let left = self.int(ST_CANCEL, Op::Left);
                self.on(ST_CANCEL, Op::SetPos, left as isize, initial_button_y as isize);
            }
        }
        self.me(Op::Invalidate, 0, 0);
        self.me(Op::SetDragging, dragging, 0);
    }

    /// `OkCancelDialog::SetText`: title, message and the two buttons' texts; the dialog sized to the message (up to
    /// 250 wide, 400 for long ones).
    fn ok_cancel_set_text(&self, title: isize, body: isize, ok: isize, cancel: isize) {
        self.on(OC_TITLE, Op::LabelSetText, title, 0);
        self.on(OC_TITLE, Op::SetDesiredWidth, 0, 0);
        self.on(OC_OK, Op::ButtonSetText, ok, 0);
        self.on(OC_OK, Op::SetDesiredSize, 0, 0);
        self.on(OC_OK, Op::SetSizeAtLeast, 100, 0);
        self.on(OC_CANCEL, Op::ButtonSetText, cancel, 0);
        self.on(OC_CANCEL, Op::SetDesiredSize, 0, 0);
        self.on(OC_CANCEL, Op::SetSizeAtLeast, 100, 0);
        self.on(OC_BODY, Op::SetVertPad, 0, 0);
        self.on(OC_BODY, Op::LabelSetText, 0, 0);
        self.on(OC_BODY, Op::SetSize, 0, 0);
        self.me(Op::SetSize, 0, 0);
        let least = self
            .int(OC_TITLE, Op::Width)
            .wrapping_add(self.int(OC_CLOSE, Op::Width))
            .wrapping_add(self.int(OC_BACK, Op::FrameLeft))
            .wrapping_add(self.int(OC_BACK, Op::FrameRight))
            .wrapping_add(5);
        self.me(Op::SetSizeAtLeast, least as isize, 0);
        self.on(OC_COMPONENT, Op::SetSize, 0, 0);
        self.on(OC_COMPONENT, Op::SetDesiredSize, 0, 0);
        self.me(Op::SetDesiredSize, 20, 20);
        let old_height = self.me(Op::Height, 0, 0) as i32;
        let old_width = self.me(Op::Width, 0, 0) as i32;
        let old_body_height = self.int(OC_BODY, Op::Height);
        let text_width = self.on(OC_BODY, Op::FontStringWidthRange, body, 0) as i32;
        let body_width = ok_cancel_body_width(text_width);
        let mut width_change = dialogs::growth(body_width, self.int(OC_BODY, Op::Width));
        let h = self.int(OC_BODY, Op::Height);
        self.on(OC_BODY, Op::SetSize, body_width as isize, h as isize);
        self.on(OC_BODY, Op::LabelSetText, body, 0);
        let label_height = self.int(OC_BODY, Op::Height);
        let lines = label_height / self.int(OC_BODY, Op::FontHeight);
        if lines < 5 {
            // the original read the font's height a second time here
            let pad = (5 - lines).wrapping_mul(self.int(OC_BODY, Op::FontHeight)) / 2;
            self.on(OC_BODY, Op::SetVertPad, pad as isize, pad as isize);
            self.on(OC_BODY, Op::SetDesiredSize, 0, 0);
        }
        let mut height_change = dialogs::growth(self.int(OC_BODY, Op::Height), old_body_height);
        // the original read the root's width and did not use it
        self.op(Op::RootWidth, 0, 0, 0);
        let root_height = self.op(Op::RootHeight, 0, 0, 0) as i32;
        let new_width = old_width.wrapping_add(width_change);
        let new_height = old_height.wrapping_add(height_change);
        if let Some(extra) = dialogs::status_extra_width(new_width, new_height, root_height, width_change, old_width) {
            width_change = width_change.wrapping_add(extra);
            let h = self.int(OC_BODY, Op::Height);
            self.on(OC_BODY, Op::SetSize, body_width.wrapping_add(extra) as isize, h as isize);
            height_change = self.int(OC_BODY, Op::Height).wrapping_sub(old_body_height);
        }
        self.me(
            Op::SetSize,
            old_width.wrapping_add(width_change) as isize,
            old_height.wrapping_add(height_change) as isize,
        );
    }

    /// CreditsDialog::AddLine: the component `comp` widened to the line (its default font's width and 20), the line
    /// added to the scrolling text.
    fn credits_add_line(&self, text: isize, comp: isize, color: u32) {
        let width = (self.op(Op::DefaultFontStringWidth, text, 0, 0) as i32).wrapping_add(20);
        if width > self.op(Op::Width, comp, 0, 0) as i32 {
            let height = self.op(Op::Height, comp, 0, 0);
            self.op(Op::SetSize, comp, width as isize, height);
        }
        self.op(Op::ScrollAddLine, self.scroll_text.get(), text, color as isize);
    }

    /// The destructor: the controls released in reverse member order (the `SmartPtr` members' destruction), then
    /// CreditsDialog's scrolling text (its first member).
    fn release_all(&self) {
        for i in (0..slots(self.kind).len()).rev() {
            let c = self.controls[i].replace(0);
            if c != 0 {
                self.op(Op::Release, c, 0, 0);
            }
        }
        let s = self.scroll_text.replace(0);
        if s != 0 {
            self.op(Op::Release, s, 0, 0);
        }
    }
}

/// `OkCancelDialog::SetText`: the message's width: its text's width up to 250, 250 up to 800, else 400.
pub fn ok_cancel_body_width(text_width: i32) -> i32 {
    if text_width < 250 {
        text_width
    } else if text_width < 800 {
        250
    } else {
        400
    }
}

/// The control `index` of a kind's table (member order; its name, file, line and type), NULL past its end.
#[no_mangle]
pub extern "C" fn ee_won_lobbydlg_slot(kind: EeWonLobbyDlgKind, index: u32) -> *const EeWonLobbyDlgSlot {
    slots(kind).get(index as usize).map_or(std::ptr::null(), |s| s as *const _)
}

/// A new dialog object for the C++ `dialog` of `kind`.
#[no_mangle]
pub extern "C" fn ee_won_lobbydlg_new(
    dialog: *mut c_void,
    kind: EeWonLobbyDlgKind,
    host: EeWonLobbyDlgHostFn,
) -> *mut EeWonLobbyDlg {
    Box::into_raw(Box::new(EeWonLobbyDlg::new(dialog, kind, host)))
}

/// Releases the controls (reverse member order) and frees the object.
///
/// # Safety
/// `dlg` came from `ee_won_lobbydlg_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbydlg_free(dlg: *mut EeWonLobbyDlg) {
    if !dlg.is_null() {
        // SAFETY: the contract; the releases run before the box is dropped, through a shared reference
        unsafe { (*dlg).release_all() };
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(dlg) });
    }
}

/// One inward call (`EeWonLobbyDlgCall`).
///
/// # Safety
/// `dlg` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbydlg_call(
    dlg: *const EeWonLobbyDlg,
    call: EeWonLobbyDlgCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*dlg).call(call, a, b, c) }
}

#[cfg(test)]
#[path = "lobbydlg_tests.rs"]
mod tests;
