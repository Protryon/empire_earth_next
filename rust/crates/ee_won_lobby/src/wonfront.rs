//! The game's WON front end's screens (R6 WON round 32): the events of the server list, the room list, the game
//! room, the account and statistics forms (EEUIEWonNet*Events.cpp), the three list boxes' drawing, copies,
//! constructors and the statistics' placeholder refresh (EEUICWonNet*Box.cpp) and the game room form's manager
//! (EEWonNetAvailableGameFormManager::Get). Every decision, number, text and rectangle is Rust; each C++ statement on
//! a form, control, text, list line or `gWonNet` is one op of the host (`include/ee/EEWonFrontHost.inl`, included by
//! EEUIEWonNetArenaGameEvents.cpp and by won_wonnet_test, which runs the deleted C++ against this over one stand-in
//! world). The C++ classes keep their members (the boxes' texts, labels, widths and lines); the ProcessEvents and
//! the boxes' methods are one `ee_won_front_call` each.
//!
//! Unreachable in Age of Conquest (gWonNet is never made; the WON forms' boxes are built with the UI but only drawn
//! on those forms): only the golden and the unit tests below run the events.

use std::ffi::{c_long, c_ulong, c_void};
use std::sync::atomic::{AtomicU64, AtomicUsize, Ordering};

/// The host: `(op, a, b, c) -> result`.
pub type EeWonFrontHostFn = Option<unsafe extern "C" fn(op: EeWonFrontOp, a: isize, b: isize, c: isize) -> isize>;

static HOST: AtomicUsize = AtomicUsize::new(0);

/// Sets the host (EEUIEWonNetArenaGameEvents.cpp at static initialisation, the driver before its first call).
#[no_mangle]
pub extern "C" fn ee_won_front_set_host(host: EeWonFrontHostFn) {
    HOST.store(host.map_or(0, |f| f as usize), Ordering::Relaxed);
}

/// A rectangle as `GERectangle` lays it out (`mBottom`, `mLeft`, `mRight`, `mTop`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeWonFrontRect {
    pub bottom: f32,
    pub left: f32,
    pub right: f32,
    pub top: f32,
}

/// The C++ functions whose bodies are here: `ee_won_front_call(call, obj, a)`. `obj` is the form (the events,
/// `a` the entity), the box (`this`, its methods; the constructors' `a` the font) or nothing.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonFrontCall {
    /// EEUIEWNAGUpdateForm::ProcessEvent.
    AgUpdateForm = 0,
    /// EEUIEWNAGActivateForm::ProcessEvent.
    AgActivateForm,
    /// EEUIEWNAGBack::ProcessEvent.
    AgBack,
    /// EEUIEWNAGCheckPassword::ProcessEvent.
    AgCheckPassword,
    /// EEUIEWNAGClickGameFilter::ProcessEvent.
    AgClickGameFilter,
    /// EEUIEWNAGCloseForm::ProcessEvent.
    AgCloseForm,
    /// EEUIEWNAGDeactivateForm::ProcessEvent.
    AgDeactivateForm,
    /// EEUIEWNAGDoubleclickUser::ProcessEvent.
    AgDoubleclickUser,
    /// EEUIEWNAGHostOrJoinGame::ProcessEvent.
    AgHostOrJoinGame,
    /// The password prompt's callback (`a` the password, `const char*`).
    AgOnGamePassword,
    /// EEUIEWNAGSendChat::ProcessEvent.
    AgSendChat,
    /// EEUIEWNAGToggleHostScreen::ProcessEvent.
    AgToggleHostScreen,
    /// EEUIEWNAGToggleJoinScreen::ProcessEvent.
    AgToggleJoinScreen,
    /// EEUIEWNAGOpenScreen::ProcessEvent.
    AgOpenScreen,
    /// EEUIEWNARUpdateForm::ProcessEvent.
    ArUpdateForm,
    /// EEUIEWNARActivateForm::ProcessEvent.
    ArActivateForm,
    /// EEUIEWNARDeactivateForm::ProcessEvent.
    ArDeactivateForm,
    /// EEUIEWNARDoubleClickRoom::ProcessEvent (JoinSelectedRoom).
    ArDoubleClickRoom,
    /// EEUIEWNARClickUpdateAccount::ProcessEvent.
    ArClickUpdateAccount,
    /// EEUIEWNARJoinRoom::ProcessEvent.
    ArJoinRoom,
    /// EEUIEWNAROpenScreen::ProcessEvent.
    ArOpenScreen,
    /// EEUIEWNMUASubmit::ProcessEvent.
    MuaSubmit,
    /// EEUIEWNMUAOpenScreen::ProcessEvent.
    MuaOpenScreen,
    /// EEUIEWNSSOk::ProcessEvent.
    SsOk,
    /// EEUIEWNSSOpenScreen::ProcessEvent.
    SsOpenScreen,
    /// EEUIEWNSOpen::ProcessEvent.
    StatsOpen,
    /// EEUIEWNSCancel::ProcessEvent.
    StatsCancel,
    /// EEUICWonNetGamesBox's constructor body (`a` the font).
    GamesConstruct,
    /// EEUICWonNetGamesBox's destructor body.
    GamesDestruct,
    /// EEUICWonNetGamesBox::CopyData.
    GamesCopyData,
    /// EEUICWonNetGamesBox::DrawTextA.
    GamesDrawText,
    /// EEUICWonNetRoomsBox's constructor body (`a` the font).
    RoomsConstruct,
    /// EEUICWonNetRoomsBox's destructor body.
    RoomsDestruct,
    /// EEUICWonNetRoomsBox::CopyData.
    RoomsCopyData,
    /// EEUICWonNetRoomsBox::DrawTextA.
    RoomsDrawText,
    /// EEUICWonNetStatBox's constructor body (`a` the font).
    StatConstruct,
    /// EEUICWonNetStatBox::CopyData.
    StatCopyData,
    /// EEUICWonNetStatBox::DrawTextA.
    StatDrawText,
    /// EEUICWonNetStatBox::Refresh.
    StatRefresh,
    /// EEWonNetAvailableGameFormManager::Get.
    FormManagerGet,
    /// EEUIEWNARBack::ProcessEvent (R6 WON lobby and network round 37: the three Pop events).
    ArBack,
    /// EEUIEWNMUACancel::ProcessEvent.
    MuaCancel,
    /// EEUIEWNSSCancel::ProcessEvent.
    SsCancel,
}

/// A list line's number or flag (`GamesLineGet`, `RoomsLineGet`, `StatLineGet`, `StatLineSet`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonFrontField {
    /// EEUICWNGBLine::mHumans.
    Humans = 0,
    /// EEUICWNGBLine::mPlayers.
    Players,
    /// EEUICWNGBLine::mHasPassword.
    HasPassword,
    /// EEUICWNGBLine::mIsPrivate.
    IsPrivate,
    /// EEUICWNGBLine::mIsInProgress.
    IsInProgress,
    /// EEUICWNRBLine::mNumGames.
    NumGames,
    /// EEUICWNRBLine::mNumPlayers.
    NumPlayers,
    /// EEUICWNSBStat::mL14.
    L14,
    /// EEUICWNSBStat::mL18.
    L18,
    /// EEUICWNSBStat::mNumGames.
    StatNumGames,
    /// EEUICWNSBStat::mL20.
    L20,
    /// EEUICWNSBStat::mL24.
    L24,
    /// EEUICWNSBStat::mL28.
    L28,
    /// EEUICWNSBStat::mL2C.
    L2C,
    /// EEUICWNSBStat::mTotalTime.
    TotalTime,
}

/// The host's ops: one C++ statement each. Controls are `UIControl*` (a form's `GetControl`), texts given are
/// NUL-terminated bytes, texts read come through a sink (`ee_won_front_sink`) in the op's last argument.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonFrontOp {
    // ---- forms and the UI manager
    /// `((UIForm*)a)->GetControl(b)`.
    FormControl = 1,
    /// `gUIManager->GetFormByID(a)`.
    FormById,
    /// `((UIForm*)a)->GetEntityGroup(b)->Enable()`.
    FormGroupEnable,
    /// `((UIForm*)a)->GetEntityGroup(b)->Disable()`.
    FormGroupDisable,
    /// `((UIForm*)a)->GetEntityGroup(b)->GetIsEnabled()`.
    FormGroupIsEnabled,
    /// `((UIForm*)a)->SetTabbing(b != 0)`.
    FormSetTabbing,
    /// `((UIForm*)a)->SetTabStop(b)`.
    FormSetTabStop,
    /// `((UIForm*)a)->GetFormUpdateEvent()`.
    FormUpdateEvent,
    /// `((UIForm*)a)->SetFormUpdateEvent((UIEvent*)b)`.
    FormSetUpdateEvent,
    /// `((UIForm*)a)->SetFormActivateEvent((UIEvent*)b)`.
    FormSetActivateEvent,
    /// `((UIForm*)a)->SetFormDeactivateEvent((UIEvent*)b)`.
    FormSetDeactivateEvent,
    /// `((UIForm*)a)->SetFormCloseEvent((UIEvent*)b)`.
    FormSetCloseEvent,
    /// `((UIForm*)a)->SetFormOpenEvent((UIEvent*)b)`.
    FormSetOpenEvent,
    /// `((UIForm*)a)->GetManager()->Pop()`.
    FormPop,
    /// `((UIForm*)a)->GetFormManager()` as the game room's manager.
    FormManager,
    /// `((UIForm*)a)->SetFormManager((EEWonNetAvailableGameFormManager*)b)`.
    FormSetManager,
    /// `new EEWonNetAvailableGameFormManager`.
    FormManagerNew,
    /// `((EEWonNetAvailableGameFormManager*)a)->GetPoll()`.
    FormManagerPoll,
    /// `gUIManager->GetCurrentForm()->SetFormUpdateEvent(NULL)`.
    CurrentFormClearUpdateEvent,
    /// `gUIManager->LockUIForCopyData()`.
    UiLock,
    /// `gUIManager->UnlockUIForCopyData()`.
    UiUnlock,
    /// `gUIManager->Push((UIForm*)a)`.
    UiPush,
    /// `gUIManager->Pop()`.
    UiPop,
    /// `gUIManager->DisplayMessage(UWideString(a), true, NULL, 0, true)`.
    DisplayMessage,
    /// `gUIManager->DisplayPasswordPrompt(UWideString(a), true, OnGamePassword, true)`.
    DisplayPasswordPrompt,
    /// `gResource->GetString(a, text)`, the text into the sink `c`.
    ResString,
    /// `TMGetTime()`.
    Time,
    /// `UWideString(a).GetLength()`.
    TextLength,
    /// `EEIsValidName(UWideString(a))`.
    IsValidName,
    // ---- events
    /// `new EEUIEWonNetEvent((EeWonFrontCall)a)`: the event that runs the call `a` (round 37: the one event class).
    EventNew = 40,
    /// `((UIEvent*)a)->Dereference()`.
    EventDeref,
    /// `((UIControl*)a)->SetTriggerEvent(b, (UIEvent*)c)`.
    ControlSetTriggerEvent,
    // ---- controls
    /// `((UIControl*)a)->SetDisplayMode(b ? kDisplayModeShown : kDisplayModeHidden)`.
    ControlSetDisplayMode = 50,
    /// the edit box `a`'s `DeleteText()`.
    EditDeleteText,
    /// the edit box `a`'s `GetText(0)` into the sink `c`.
    EditText,
    /// the edit box `a`'s `SetText(UWideString(b))`.
    EditSetText,
    /// the edit box `a`'s `SetCaretLocation(b, 0)`.
    EditSetCaret,
    /// the edit box `a`'s `SetText(gHostedGameName)`.
    EditSetHostedName,
    /// the check box `a`'s `GetIsChecked()`.
    CheckIsChecked,
    /// the check box `a`'s `SetChecked(b != 0)`.
    CheckSetChecked,
    /// the radio button `a`'s `Check()`.
    RadioCheck,
    /// the button `a`'s `SetText(UWideString(b), its GEText's font)`.
    ButtonSetText,
    /// the label `a`'s `SetText(UWideString(b))`.
    LabelSetText,
    /// the list `a`'s `GetCurrentSelection(*(unsigned long*)b)`.
    ListSelection,
    /// the list `a`'s `GetData(b)` into the sink `c`.
    ListData,
    /// the list `a`'s `GetHasSelection()`.
    ListHasSelection,
    /// the list `a`'s `DeleteAllEntries()`.
    ListClear,
    /// the list `a`'s `AppendLine(UWideString(b))`.
    ListAppend,
    /// the list `a`'s `ChangeSelectionIndex(b, true, false)`.
    ListSelectIndex,
    /// the games box `a`'s `GetCurrentSelection(*(unsigned long*)b)`.
    GamesSelection,
    /// the games box `a`'s `GetIsInProgress(b)`.
    GamesInProgress,
    /// the games box `a`'s `GetHasPassword(b)`.
    GamesHasPassword,
    /// the games box `a`'s `GetGameName(b)` into the sink `c`.
    GamesName,
    /// the rooms box `a`'s `GetCurrentSelection(*(unsigned long*)b)`.
    RoomsSelection,
    /// the rooms box `a`'s `GetData(b).mName` into the sink `c`.
    RoomsName,
    /// the stat box `a`'s `Refresh()`.
    StatBoxRefresh,
    // ---- the poll
    /// `((TMPoll*)a)->Poll()`.
    PollPoll = 80,
    /// `((TMPoll*)a)->GetIsRunning()`.
    PollIsRunning,
    /// `((TMPoll*)a)->Start()`.
    PollStart,
    /// `((TMPoll*)a)->Stop()`.
    PollStop,
    // ---- gWonNet
    /// `gWonNet != NULL`.
    WonNetExists = 90,
    /// `gWonNet->GetGameListPort()`.
    GameListPort,
    /// `gWonNet->RefreshGameList((UIControl*)a)`.
    RefreshGameList,
    /// `gWonNet->UpdateGameList()`.
    UpdateGameList,
    /// `gWonNet->LeaveRoom()`.
    LeaveRoom,
    /// `gWonNet->HostGame(a, b)` (`b` NULL: no password).
    HostGame,
    /// `gWonNet->JoinGame(a, b)`.
    JoinGame,
    /// `gWonNet->SendChat(UWideString(a))`.
    SendChat,
    /// `WStringToString(gWonNet->mUser->GetName())` into the sink `c`.
    UserName,
    /// `WStringToString(gWonNet->GetRoomName())` into the sink `c`.
    RoomName,
    /// `gWonNet->GetRoomListChanged()`.
    RoomListChanged,
    /// `gWonNet->UpdateRoomList(rooms a, label b, label c)` (the original's dynamic_casts).
    UpdateRoomList,
    /// `gWonNet->JoinRoom(a, NULL)`.
    JoinRoom,
    /// `gWonNet->OpenUpdateAccount()`.
    OpenUpdateAccount,
    /// `gWonNet->ConnectToLobby()`.
    ConnectToLobby,
    /// `gWonNet->UpdateAccount(UWideString(a), UWideString(b), c != 0)`.
    UpdateAccount,
    /// `gWonNet->mServers.size()`.
    ServerCount,
    /// the server `a`'s `mDisplayName.size()`.
    ServerDisplayNameSize,
    /// the server `a`'s `mDisplayName` through `wcstombs` into the sink `c`.
    ServerDisplayName,
    /// the server `a`'s `mName` through `wcstombs` into the sink `c`.
    ServerName,
    /// `gWonNet->SelectServer(the server a's mDisplayName, its GetNetAddrAsIP())`.
    SelectServer,
    // ---- texts (`GEText*`)
    /// `((GEText*)a)->SetText(UWideString(b))`.
    TextSetText = 120,
    /// `((GEText*)a)->SetFgColor(GEColor())`.
    TextSetFgDefault,
    /// `((GEText*)a)->SetFgColor(GEColor(b[0], b[1], b[2]))` (`b` three floats).
    TextSetFg,
    /// `((GEText*)a)->SetBgColor(GEColor(b[0], b[1], b[2]))`.
    TextSetBg,
    /// `((GEText*)a)->SetLocation(GE2DPoint(b[0], b[1]))`.
    TextSetLocation,
    /// `((GEText*)a)->SetIsTransparent(b != 0)`.
    TextSetTransparent,
    /// `((GEText*)a)->SetAlignment(kTextAlignLeft)`.
    TextSetAlignLeft,
    /// its font made bold (`UIFontTable::FindOrCreateFont`, style `| kFontStyleBold`).
    TextFontBold,
    /// its font made not bold (style `& ~kFontStyleBold`).
    TextFontPlain,
    /// `*(float*)b = ((GEText*)a)->GetTextExtent().GetWidth()`.
    TextExtentWidth,
    /// `delete (GEText*)a`.
    TextDelete,
    // ---- the games box (`a` the box)
    /// `mCopiedText`.
    GamesCopiedOf = 140,
    /// `mText`.
    GamesTextOf,
    /// `mCopiedText = new GEText((GEFont*)b)`.
    GamesSetCopied,
    /// `mText = new GEText((GEFont*)b)`.
    GamesSetText,
    /// `GetSelectedBackground()->SetColor(GEColor(0, 0, 0))`.
    GamesSelectedBlack,
    /// `SetFocusedRectColor(GEColor(0, 0, 0))`.
    GamesFocusBlack,
    /// the label `b` (0 password, 1 private, 2 in progress) = `UWideString(c)`.
    GamesLabelSet,
    /// `mCopiedText->SetText(the label b)`.
    GamesTextSetLabel,
    /// `*(float*)c` = the width `b` (as the labels).
    GamesWidthGet,
    /// the width `b` = `*(const float*)c`.
    GamesWidthSet,
    /// `UIListBox<EEUICWNGBLine>::CopyData()`.
    GamesBaseCopyData,
    /// `*mCopiedText = *mText`.
    GamesCopyText,
    /// `DrawScrollBarText()`.
    GamesDrawScrollBar,
    /// `UIControl::DrawTextA()`.
    GamesBaseDrawText,
    /// `GetDrawCount()`.
    GamesDrawCount,
    /// `GetLineHeight()`.
    GamesLineHeight,
    /// `*(GERectangle*)b = GetCopyTextRectangle()`.
    GamesCopyRect,
    /// the copied line `b`'s field `c` (`EeWonFrontField`).
    GamesLineGet,
    /// `mCopiedText->SetText(mCopyLineArray[b].mName)`.
    GamesTextSetLineName,
    /// `GetForm()->DrawClippedText(*(GERectangle*)b, mCopiedText, this)`.
    GamesDrawClipped,
    // ---- the rooms box (`a` the box)
    /// `mCopiedText`.
    RoomsCopiedOf = 170,
    /// `mText`.
    RoomsTextOf,
    /// `mCopiedText = new GEText((GEFont*)b)`.
    RoomsSetCopied,
    /// `mText = new GEText((GEFont*)b)`.
    RoomsSetText,
    /// `GetSelectedBackground()->SetColor(GEColor(0, 0, 0))`.
    RoomsSelectedBlack,
    /// `SetFocusedRectColor(GEColor(0, 0, 0))`.
    RoomsFocusBlack,
    /// `UIListBox<EEUICWNRBLine>::CopyData()`.
    RoomsBaseCopyData,
    /// `*mCopiedText = *mText`.
    RoomsCopyText,
    /// `DrawScrollBarText()`.
    RoomsDrawScrollBar,
    /// `UIControl::DrawTextA()`.
    RoomsBaseDrawText,
    /// `GetDrawCount()`.
    RoomsDrawCount,
    /// `GetLineHeight()`.
    RoomsLineHeight,
    /// `*(GERectangle*)b = GetCopyTextRectangle()`.
    RoomsCopyRect,
    /// the copied line `b`'s field `c`.
    RoomsLineGet,
    /// `mCopiedText->SetText(mCopyLineArray[b].mName)` (`c` 0) or its `mDescription` (`c` 1).
    RoomsTextSetLine,
    /// `GetForm()->DrawClippedText(*(GERectangle*)b, mCopiedText, this)`.
    RoomsDrawClipped,
    // ---- the stat box (`a` the box)
    /// `mText`.
    StatTextOf = 200,
    /// `mRowHeight = ((GEFont*)b)->GetHeight()`.
    StatSetRowHeight,
    /// `mText = new GEText((GEFont*)b)`.
    StatSetText,
    /// `mRowHeight`.
    StatRowHeight,
    /// `UIListBox<EEUICWNSBStat>::CopyData()`.
    StatBaseCopyData,
    /// `GetNumberLinesDisplayed()`.
    StatLinesDisplayed,
    /// `SetCopiedHasSelection(false)`.
    StatClearCopiedSelection,
    /// `DrawScrollBarText()`.
    StatDrawScrollBar,
    /// `UIControl::DrawTextA()`.
    StatBaseDrawText,
    /// `mCopyLineArray.size()`.
    StatCopyCount,
    /// `*(GERectangle*)b = GetCopyTextRectangle()`.
    StatCopyRect,
    /// the copied line `b`'s field `c`.
    StatLineGet,
    /// `mText->SetText(mCopyLineArray[b].mText)`.
    StatTextSetLineText,
    /// `GetForm()->DrawClippedText(*(GERectangle*)b, mText, this)`.
    StatDrawClipped,
    /// `DeleteAllEntries()`.
    StatClear,
    /// the refresh's line's field `a` = `b`.
    StatLineSet,
    /// the refresh's line's `mText = UWideString(a)`.
    StatLineSetText,
    /// `AppendLine(the refresh's line)`.
    StatAppendLine,
}

use EeWonFrontCall as Ev;
use EeWonFrontField as Fd;
use EeWonFrontOp as Op;

// the forms and controls the screens name (ee/EEUIManager.h; checked against it by the tests)
const WF_FORM_AVAILABLE_GAMES: isize = 32;
const WF_FORM_UPDATE_SIERRA_ACCOUNT: isize = 37;
const WF_AG_PLAYERS_NAME_LABEL: isize = 2;
const WF_AG_ROOMS_NAME_LABEL: isize = 4;
const WF_AG_JOIN_GAME_TAB: isize = 5;
const WF_AG_HOST_GAME_TAB: isize = 6;
const WF_AG_PLAYERS_IN_ROOM_LIST_BOX: isize = 8;
const WF_AG_AVAL_GAMES_LIST_BOX: isize = 13;
const WF_AG_CHAT_EDIT_BOX: isize = 16;
const WF_AG_JOIN_HOST_GAME_BUTTON: isize = 17;
const WF_AG_BACK_BUTTON: isize = 18;
const WF_AG_HOST_GAME_NAME_EDIT_BOX: isize = 24;
const WF_AG_PW_EDIT_BOX: isize = 27;
const WF_AG_PW_CHECKBOX: isize = 28;
const WF_AG_PW_RETYPE_EDIT_BOX: isize = 29;
const WF_AG_PW_LABEL: isize = 30;
const WF_AG_PW_RETYPE_LABEL: isize = 31;
const WF_AG_IN_PROGRESS_TOGGLE: isize = 32;
const WF_AG_PASSWORD_TOGGLE: isize = 33;
const WF_AG_PRIVATE_TOGGLE: isize = 34;
const WF_AR_PLAYERS_NAME_LABEL: isize = 2;
const WF_AR_AVAIL_ROOMS_LIST_BOX: isize = 6;
const WF_AR_JOIN_ROOM_BUTTON: isize = 7;
const WF_AR_BACK_BUTTON: isize = 8;
const WF_AR_UPDATE_MY_ACCOUNT: isize = 9;
const WF_AR_TOTAL_GAMES_NUMBER: isize = 14;
const WF_AR_SERVERS_NAME_LABEL: isize = 16;
const WF_UA_NEW_PASSWORD_EDIT_BOX: isize = 2;
const WF_UA_CONFIRM_PASSWORD_EDIT_BOX: isize = 4;
const WF_UA_ENTER_EMAIL_EDIT_BOX: isize = 6;
const WF_UA_SEND_ME_EMAIL_CHECKBOX: isize = 7;
const WF_UA_UPDATE_BUTTON: isize = 8;
const WF_UA_DONT_UPDATE_BUTTON: isize = 9;
const WF_SS_LIST_BOX: isize = 1;
const WF_SS_OK_BUTTON: isize = 2;
const WF_SS_CANCEL_BUTTON: isize = 3;
const WF_WS_STAT_BOX: isize = 2;
const WF_WS_MY_STATS_LINE: isize = 22;
// the control triggers (lle/UIControl.h)
const WF_TRIGGER_CHECKED: isize = 0x3e9;
const WF_TRIGGER_UNCHECKED: isize = 0x3ea;
const WF_TRIGGER_DOUBLE_CLICKED: isize = 0x3ec;
const WF_TRIGGER_BUTTON_UP: isize = 0x3f0;
const WF_TRIGGER_ENTER_PRESSED: isize = 0x3f1;
// the strings (ee/EEStringIDs.h)
const WF_IDS_GAME_NAME_IS_INVALID: isize = 10005;
const WF_IDS_LOBBY_PASSWORD: isize = 30205;
const WF_IDS_HOST: isize = 30226;
const WF_IDS_JOIN: isize = 30227;
const WF_IDS_YOUR_TWO_PASSWORDS_DONT_MATCH: isize = 30420;
const WF_IDS_CIV_POWER_PASSWORD: isize = 32800;
const WF_IDS_PRIVATE: isize = 32801;
const WF_IDS_IN_PROGRESS: isize = 32802;
const WF_IDS_YOU_CANT_JOIN_THAT_GAME: isize = 32803;

/// HostOrJoinGame's and JoinRoom's last accepted times and the room list's last refresh (the originals' statics).
/// (`unsigned long`s: `TMGetTime()`'s type, 32 bits on Windows, 64 on Linux and macOS.)
static HOST_OR_JOIN_LAST: AtomicU64 = AtomicU64::new(0);
static JOIN_ROOM_LAST: AtomicU64 = AtomicU64::new(0);
static ROOMS_LAST_UPDATE: AtomicU64 = AtomicU64::new(0);

fn last(t: &AtomicU64) -> c_ulong {
    t.load(Ordering::Relaxed) as c_ulong
}

fn set_last(t: &AtomicU64, v: c_ulong) {
    t.store(v as u64, Ordering::Relaxed);
}

fn h(op: Op, a: isize, b: isize, c: isize) -> isize {
    let f = HOST.load(Ordering::Relaxed);
    if f == 0 {
        return 0;
    }
    // SAFETY: set by ee_won_front_set_host from a host of this signature
    let f: unsafe extern "C" fn(Op, isize, isize, isize) -> isize = unsafe { std::mem::transmute(f) };
    // SAFETY: the host's contract (EeWonFrontOp)
    unsafe { f(op, a, b, c) }
}

/// A text the host writes (`ee_won_front_sink`): the op's sink is its argument `c`.
fn text(op: Op, a: isize, b: isize) -> Vec<u8> {
    let mut s: Vec<u8> = Vec::new();
    h(op, a, b, &mut s as *mut Vec<u8> as isize);
    s
}

/// A text handed to the host: NUL-terminated (a NUL inside ends it there, as the C string did).
fn cz(s: &[u8]) -> Vec<u8> {
    let mut v: Vec<u8> = s.iter().copied().take_while(|&b| b != 0).collect();
    v.push(0);
    v
}

/// Appends `len` bytes to the sink (a `Vec<u8>` the op was handed).
///
/// # Safety
/// `sink` is the op's live sink, `data` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_front_sink(sink: *mut c_void, data: *const u8, len: usize) {
    let s = &mut *(sink as *mut Vec<u8>);
    if len > 0 {
        s.extend_from_slice(std::slice::from_raw_parts(data, len));
    }
}

fn ctl(form: isize, id: isize) -> isize {
    h(Op::FormControl, form, id, 0)
}

fn res(id: isize) -> Vec<u8> {
    text(Op::ResString, id, 0)
}

fn message(id: isize) {
    let t = cz(&res(id));
    h(Op::DisplayMessage, t.as_ptr() as isize, 0, 0);
}

fn set_trigger(ev: Ev, control: isize, trigger: isize) {
    let e = h(Op::EventNew, ev as isize, 0, 0);
    h(Op::ControlSetTriggerEvent, control, trigger, e);
    h(Op::EventDeref, e, 0, 0);
}

/// `form->SetForm<X>Event(new ev); ev->Dereference()`.
fn form_event(form: isize, set: Op, ev: Ev) {
    let e = h(Op::EventNew, ev as isize, 0, 0);
    h(set, form, e, 0);
    h(Op::EventDeref, e, 0, 0);
}

fn button_text(button: isize, s: &[u8]) {
    let t = cz(s);
    h(Op::ButtonSetText, button, t.as_ptr() as isize, 0);
}

fn label_text(label: isize, s: &[u8]) {
    let t = cz(s);
    h(Op::LabelSetText, label, t.as_ptr() as isize, 0);
}

fn selection(op: Op, list: isize) -> Option<isize> {
    let mut sel: c_ulong = 0;
    (h(op, list, &mut sel as *mut c_ulong as isize, 0) == 0).then_some(sel as isize)
}

fn valid_name(s: &[u8]) -> bool {
    let t = cz(s);
    h(Op::IsValidName, t.as_ptr() as isize, 0, 0) != 0
}

fn length(s: &[u8]) -> isize {
    let t = cz(s);
    h(Op::TextLength, t.as_ptr() as isize, 0, 0)
}

/// `EEWonNetAvailableGameFormManager::Get()`: the game room form's manager, made and attached on first use.
fn manager() -> isize {
    let form = h(Op::FormById, WF_FORM_AVAILABLE_GAMES, 0, 0);
    let m = h(Op::FormManager, form, 0, 0);
    if m != 0 {
        return m;
    }
    let m = h(Op::FormManagerNew, 0, 0, 0);
    h(Op::FormSetManager, form, m, 0);
    m
}

fn poll() -> isize {
    h(Op::FormManagerPoll, manager(), 0, 0)
}

fn wonnet() -> bool {
    h(Op::WonNetExists, 0, 0, 0) != 0
}

fn now() -> c_ulong {
    h(Op::Time, 0, 0, 0) as c_ulong
}

// ---- the game room (form 0x20)

fn ag_update_form() -> isize {
    if h(Op::PollPoll, poll(), 0, 0) == 0 {
        return 0;
    }
    if h(Op::GameListPort, 0, 0, 0) as u16 != 0xffff {
        h(Op::RefreshGameList, 0, 0, 0);
        h(Op::UpdateGameList, 0, 0, 0);
    }
    0
}

fn ag_activate_form(form: isize) -> isize {
    let p = poll();
    if h(Op::PollIsRunning, p, 0, 0) == 0 {
        h(Op::PollStart, p, 0, 0);
    }
    if h(Op::FormUpdateEvent, form, 0, 0) == 0 {
        form_event(form, Op::FormSetUpdateEvent, Ev::AgUpdateForm);
        h(Op::RefreshGameList, ctl(form, WF_AG_PLAYERS_IN_ROOM_LIST_BOX), 0, 0);
        h(Op::UpdateGameList, 0, 0, 0);
    }
    0
}

fn ag_check_password(form: isize) -> isize {
    let password = ctl(form, WF_AG_PW_EDIT_BOX);
    let check = ctl(form, WF_AG_PW_CHECKBOX);
    let confirmation = ctl(form, WF_AG_PW_RETYPE_EDIT_BOX);
    let password_label = ctl(form, WF_AG_PW_LABEL);
    let confirmation_label = ctl(form, WF_AG_PW_RETYPE_LABEL);
    let shown = h(Op::CheckIsChecked, check, 0, 0) != 0;
    if shown {
        h(Op::EditDeleteText, password, 0, 0);
        h(Op::EditDeleteText, confirmation, 0, 0);
    }
    for c in [password_label, confirmation_label, password, confirmation] {
        h(Op::ControlSetDisplayMode, c, shown as isize, 0);
    }
    if shown {
        h(Op::FormSetTabbing, form, 1, 0);
        h(Op::FormSetTabStop, form, WF_AG_PW_EDIT_BOX, 0);
    }
    0
}

fn ag_close_poll() {
    let p = poll();
    if h(Op::PollIsRunning, p, 0, 0) != 0 {
        h(Op::PollStop, p, 0, 0);
    }
}

fn ag_doubleclick_user(form: isize) -> isize {
    let users = ctl(form, WF_AG_PLAYERS_IN_ROOM_LIST_BOX);
    let Some(sel) = selection(Op::ListSelection, users) else {
        return 0;
    };
    let name = text(Op::ListData, users, sel);
    if valid_name(&name) {
        let chat = ctl(form, WF_AG_CHAT_EDIT_BOX);
        h(Op::EditDeleteText, chat, 0, 0);
        let mut line = b"/t \"".to_vec();
        line.extend_from_slice(&name);
        line.extend_from_slice(b"\" ");
        let t = cz(&line);
        h(Op::EditSetText, chat, t.as_ptr() as isize, 0);
        h(Op::FormSetTabbing, form, 1, 0);
        h(Op::FormSetTabStop, form, WF_AG_CHAT_EDIT_BOX, 0);
        h(Op::EditSetCaret, chat, length(&line), 0);
    }
    0
}

fn host_game(name: &[u8], password: Option<&[u8]>) {
    let n = cz(name);
    let p = password.map(cz);
    h(Op::HostGame, n.as_ptr() as isize, p.as_ref().map_or(0, |p| p.as_ptr() as isize), 0);
}

fn ag_host_or_join_game(form: isize) -> isize {
    let time = now();
    if time.wrapping_sub(last(&HOST_OR_JOIN_LAST)) <= 5000 {
        return 0;
    }
    set_last(&HOST_OR_JOIN_LAST, time);
    if h(Op::FormGroupIsEnabled, form, 2, 0) != 0 {
        let name = ctl(form, WF_AG_HOST_GAME_NAME_EDIT_BOX);
        let password = ctl(form, WF_AG_PW_EDIT_BOX);
        let check = ctl(form, WF_AG_PW_CHECKBOX);
        let confirmation = ctl(form, WF_AG_PW_RETYPE_EDIT_BOX);
        let pw = text(Op::EditText, password, 0);
        if h(Op::CheckIsChecked, check, 0, 0) != 0 && !pw.is_empty() {
            if pw == text(Op::EditText, confirmation, 0) {
                let n = text(Op::EditText, name, 0);
                if valid_name(&n) {
                    host_game(&n, Some(&pw));
                } else {
                    message(WF_IDS_GAME_NAME_IS_INVALID);
                }
            } else {
                message(WF_IDS_YOUR_TWO_PASSWORDS_DONT_MATCH);
            }
        } else {
            let n = text(Op::EditText, name, 0);
            if valid_name(&n) {
                host_game(&n, None);
            } else {
                message(WF_IDS_GAME_NAME_IS_INVALID);
            }
        }
    } else {
        let games = ctl(form, WF_AG_AVAL_GAMES_LIST_BOX);
        if let Some(sel) = selection(Op::GamesSelection, games) {
            if h(Op::GamesInProgress, games, sel, 0) != 0 {
                message(WF_IDS_YOU_CANT_JOIN_THAT_GAME);
                set_last(&HOST_OR_JOIN_LAST, 0);
            } else if h(Op::GamesHasPassword, games, sel, 0) != 0 {
                let t = cz(&res(WF_IDS_LOBBY_PASSWORD));
                h(Op::DisplayPasswordPrompt, t.as_ptr() as isize, 0, 0);
            } else {
                let n = cz(&text(Op::GamesName, games, sel));
                h(Op::JoinGame, n.as_ptr() as isize, 0, 0);
            }
        }
    }
    0
}

fn ag_on_game_password(password: isize) -> isize {
    let form = h(Op::FormById, WF_FORM_AVAILABLE_GAMES, 0, 0);
    let games = ctl(form, WF_AG_AVAL_GAMES_LIST_BOX);
    if let Some(sel) = selection(Op::GamesSelection, games) {
        let n = cz(&text(Op::GamesName, games, sel));
        h(Op::JoinGame, n.as_ptr() as isize, password, 0);
    }
    0
}

fn ag_send_chat(form: isize) -> isize {
    let edit = ctl(form, WF_AG_CHAT_EDIT_BOX);
    let typed = text(Op::EditText, edit, 0);
    if !wonnet() {
        return 0;
    }
    if !typed.is_empty() {
        let mut line = b": ".to_vec();
        line.extend_from_slice(&typed);
        let t = cz(&line);
        h(Op::SendChat, t.as_ptr() as isize, 0, 0);
    }
    h(Op::EditDeleteText, edit, 0, 0);
    0
}

fn ag_toggle_host_screen(form: isize) -> isize {
    h(Op::FormGroupEnable, form, 2, 0);
    h(Op::FormGroupDisable, form, 1, 0);
    let t = res(WF_IDS_HOST);
    button_text(ctl(form, WF_AG_JOIN_HOST_GAME_BUTTON), &t);
    h(Op::CheckSetChecked, ctl(form, WF_AG_PW_CHECKBOX), 0, 0);
    h(Op::FormSetTabStop, form, WF_AG_HOST_GAME_NAME_EDIT_BOX, 0);
    let name = ctl(form, WF_AG_HOST_GAME_NAME_EDIT_BOX);
    let n = text(Op::EditText, name, 0);
    h(Op::EditSetCaret, name, length(&n), 0);
    0
}

fn ag_toggle_join_screen(form: isize) -> isize {
    h(Op::FormGroupEnable, form, 1, 0);
    h(Op::FormGroupDisable, form, 2, 0);
    let t = res(WF_IDS_JOIN);
    button_text(ctl(form, WF_AG_JOIN_HOST_GAME_BUTTON), &t);
    h(Op::FormSetTabStop, form, WF_AG_CHAT_EDIT_BOX, 0);
    0
}

fn ag_open_screen(form: isize) -> isize {
    let triggers = [
        (Ev::AgBack, WF_AG_BACK_BUTTON, WF_TRIGGER_BUTTON_UP),
        (Ev::AgToggleHostScreen, WF_AG_HOST_GAME_TAB, WF_TRIGGER_BUTTON_UP),
        (Ev::AgToggleJoinScreen, WF_AG_JOIN_GAME_TAB, WF_TRIGGER_BUTTON_UP),
        (Ev::AgHostOrJoinGame, WF_AG_JOIN_HOST_GAME_BUTTON, WF_TRIGGER_BUTTON_UP),
        (Ev::AgHostOrJoinGame, WF_AG_HOST_GAME_NAME_EDIT_BOX, WF_TRIGGER_ENTER_PRESSED),
        (Ev::AgHostOrJoinGame, WF_AG_AVAL_GAMES_LIST_BOX, WF_TRIGGER_DOUBLE_CLICKED),
        (Ev::AgDoubleclickUser, WF_AG_PLAYERS_IN_ROOM_LIST_BOX, WF_TRIGGER_DOUBLE_CLICKED),
        (Ev::AgCheckPassword, WF_AG_PW_CHECKBOX, WF_TRIGGER_CHECKED),
        (Ev::AgCheckPassword, WF_AG_PW_CHECKBOX, WF_TRIGGER_UNCHECKED),
        (Ev::AgClickGameFilter, WF_AG_IN_PROGRESS_TOGGLE, WF_TRIGGER_CHECKED),
        (Ev::AgClickGameFilter, WF_AG_IN_PROGRESS_TOGGLE, WF_TRIGGER_UNCHECKED),
        (Ev::AgClickGameFilter, WF_AG_PASSWORD_TOGGLE, WF_TRIGGER_CHECKED),
        (Ev::AgClickGameFilter, WF_AG_PASSWORD_TOGGLE, WF_TRIGGER_UNCHECKED),
        (Ev::AgClickGameFilter, WF_AG_PRIVATE_TOGGLE, WF_TRIGGER_CHECKED),
        (Ev::AgClickGameFilter, WF_AG_PRIVATE_TOGGLE, WF_TRIGGER_UNCHECKED),
    ];
    for (ev, id, trigger) in triggers {
        set_trigger(ev, ctl(form, id), trigger);
    }
    form_event(form, Op::FormSetActivateEvent, Ev::AgActivateForm);
    form_event(form, Op::FormSetDeactivateEvent, Ev::AgDeactivateForm);
    form_event(form, Op::FormSetCloseEvent, Ev::AgCloseForm);
    let e = h(Op::EventNew, Ev::AgSendChat as isize, 0, 0);
    h(Op::ControlSetTriggerEvent, ctl(form, WF_AG_CHAT_EDIT_BOX), WF_TRIGGER_ENTER_PRESSED, e);
    h(Op::EventDeref, e, 0, 0);
    h(Op::FormGroupEnable, form, 1, 0);
    h(Op::FormGroupDisable, form, 2, 0);
    h(Op::RadioCheck, ctl(form, WF_AG_JOIN_GAME_TAB), 0, 0);
    let t = res(WF_IDS_JOIN);
    button_text(ctl(form, WF_AG_JOIN_HOST_GAME_BUTTON), &t);
    h(Op::FormSetTabbing, form, 1, 0);
    h(Op::FormSetTabStop, form, WF_AG_CHAT_EDIT_BOX, 0);
    let user = ctl(form, WF_AG_PLAYERS_NAME_LABEL);
    label_text(user, &text(Op::UserName, 0, 0));
    let room = ctl(form, WF_AG_ROOMS_NAME_LABEL);
    label_text(room, &text(Op::RoomName, 0, 0));
    h(Op::EditSetHostedName, ctl(form, WF_AG_HOST_GAME_NAME_EDIT_BOX), 0, 0);
    h(Op::CheckSetChecked, ctl(form, WF_AG_PW_CHECKBOX), 0, 0);
    0
}

// ---- the room list

fn update_room_list(form: isize) {
    let games = ctl(form, WF_AR_TOTAL_GAMES_NUMBER);
    let server = ctl(form, WF_AR_SERVERS_NAME_LABEL);
    let rooms = ctl(form, WF_AR_AVAIL_ROOMS_LIST_BOX);
    h(Op::UpdateRoomList, rooms, server, games);
}

fn ar_update_form(form: isize) -> isize {
    if wonnet() && (now().wrapping_sub(last(&ROOMS_LAST_UPDATE)) > 10000 || h(Op::RoomListChanged, 0, 0, 0) != 0) {
        set_last(&ROOMS_LAST_UPDATE, now());
        update_room_list(form);
    }
    0
}

fn ar_activate_form(form: isize) -> isize {
    if h(Op::FormUpdateEvent, form, 0, 0) == 0 {
        form_event(form, Op::FormSetUpdateEvent, Ev::ArUpdateForm);
    }
    if wonnet() {
        update_room_list(form);
    }
    0
}

fn join_selected_room(form: isize) -> isize {
    let rooms = ctl(form, WF_AR_AVAIL_ROOMS_LIST_BOX);
    if let Some(sel) = selection(Op::RoomsSelection, rooms) {
        let room = cz(&text(Op::RoomsName, rooms, sel));
        if wonnet() {
            h(Op::JoinRoom, room.as_ptr() as isize, 0, 0);
        }
    }
    0
}

fn ar_click_update_account() -> isize {
    let e = h(Op::EventNew, Ev::MuaOpenScreen as isize, 0, 0);
    h(Op::FormSetOpenEvent, h(Op::FormById, WF_FORM_UPDATE_SIERRA_ACCOUNT, 0, 0), e, 0);
    h(Op::EventDeref, e, 0, 0);
    h(Op::UiLock, 0, 0, 0);
    h(Op::UiPush, h(Op::FormById, WF_FORM_UPDATE_SIERRA_ACCOUNT, 0, 0), 0, 0);
    h(Op::UiUnlock, 0, 0, 0);
    h(Op::OpenUpdateAccount, 0, 0, 0);
    0
}

fn ar_join_room(form: isize) -> isize {
    let time = now();
    if time.wrapping_sub(last(&JOIN_ROOM_LAST)) > 5000 {
        set_last(&JOIN_ROOM_LAST, time);
        join_selected_room(form);
    }
    0
}

fn ar_open_screen(form: isize) -> isize {
    set_trigger(Ev::ArBack, ctl(form, WF_AR_BACK_BUTTON), WF_TRIGGER_BUTTON_UP);
    set_trigger(Ev::ArJoinRoom, ctl(form, WF_AR_JOIN_ROOM_BUTTON), WF_TRIGGER_BUTTON_UP);
    set_trigger(Ev::ArDoubleClickRoom, ctl(form, WF_AR_AVAIL_ROOMS_LIST_BOX), WF_TRIGGER_DOUBLE_CLICKED);
    set_trigger(Ev::ArDoubleClickRoom, ctl(form, WF_AR_AVAIL_ROOMS_LIST_BOX), WF_TRIGGER_ENTER_PRESSED);
    form_event(form, Op::FormSetActivateEvent, Ev::ArActivateForm);
    form_event(form, Op::FormSetDeactivateEvent, Ev::ArDeactivateForm);
    set_trigger(Ev::ArClickUpdateAccount, ctl(form, WF_AR_UPDATE_MY_ACCOUNT), WF_TRIGGER_BUTTON_UP);
    let user = ctl(form, WF_AR_PLAYERS_NAME_LABEL);
    label_text(user, &text(Op::UserName, 0, 0));
    if wonnet() {
        h(Op::ConnectToLobby, 0, 0, 0);
    }
    set_last(&ROOMS_LAST_UPDATE, now());
    0
}

// ---- the account, server and statistics forms

fn mua_submit(form: isize) -> isize {
    let password = text(Op::EditText, ctl(form, WF_UA_NEW_PASSWORD_EDIT_BOX), 0);
    let confirmation = text(Op::EditText, ctl(form, WF_UA_CONFIRM_PASSWORD_EDIT_BOX), 0);
    let email = text(Op::EditText, ctl(form, WF_UA_ENTER_EMAIL_EDIT_BOX), 0);
    let flag = h(Op::CheckIsChecked, ctl(form, WF_UA_SEND_ME_EMAIL_CHECKBOX), 0, 0);
    if password == confirmation {
        let (p, e) = (cz(&password), cz(&email));
        h(Op::UpdateAccount, p.as_ptr() as isize, e.as_ptr() as isize, (flag != 0) as isize);
    } else {
        message(WF_IDS_YOUR_TWO_PASSWORDS_DONT_MATCH);
    }
    0
}

fn mua_open_screen(form: isize) -> isize {
    set_trigger(Ev::MuaCancel, ctl(form, WF_UA_DONT_UPDATE_BUTTON), WF_TRIGGER_BUTTON_UP);
    set_trigger(Ev::MuaSubmit, ctl(form, WF_UA_UPDATE_BUTTON), WF_TRIGGER_BUTTON_UP);
    0
}

fn ss_ok(form: isize) -> isize {
    let count = h(Op::ServerCount, 0, 0, 0) as usize;
    let list = ctl(form, WF_SS_LIST_BOX);
    if h(Op::ListHasSelection, list, 0, 0) == 0 {
        return 0;
    }
    let mut sel: c_ulong = 0;
    h(Op::ListSelection, list, &mut sel as *mut c_ulong as isize, 0);
    if sel as usize >= count {
        return 0;
    }
    h(Op::SelectServer, sel as isize, 0, 0);
    0
}

fn ss_open_screen(form: isize) -> isize {
    let list = ctl(form, WF_SS_LIST_BOX);
    set_trigger(Ev::SsCancel, ctl(form, WF_SS_CANCEL_BUTTON), WF_TRIGGER_BUTTON_UP);
    set_trigger(Ev::SsOk, ctl(form, WF_SS_OK_BUTTON), WF_TRIGGER_BUTTON_UP);
    set_trigger(Ev::SsOk, ctl(form, WF_SS_LIST_BOX), WF_TRIGGER_DOUBLE_CLICKED);
    set_trigger(Ev::SsOk, ctl(form, WF_SS_LIST_BOX), WF_TRIGGER_ENTER_PRESSED);
    h(Op::ListClear, list, 0, 0);
    let servers = h(Op::ServerCount, 0, 0, 0);
    let mut count = 0;
    for i in 0..servers {
        let name = if h(Op::ServerDisplayNameSize, i, 0, 0) > 0 {
            text(Op::ServerDisplayName, i, 0)
        } else {
            text(Op::ServerName, i, 0)
        };
        let t = cz(&name);
        h(Op::ListAppend, list, t.as_ptr() as isize, 0);
        count += 1;
    }
    if count == 1 {
        h(Op::ListSelectIndex, list, 0, 0);
    }
    0
}

fn stats_open(form: isize) -> isize {
    if wonnet() {
        h(Op::StatBoxRefresh, ctl(form, WF_WS_STAT_BOX), 0, 0);
        h(Op::StatBoxRefresh, ctl(form, WF_WS_MY_STATS_LINE), 0, 0);
    }
    0
}

// ---- the boxes

/// `(long)f` as x86's cvttss2si: truncation, and the integer indefinite (the type's minimum) for NaN or out of range.
fn to_long(f: f32) -> c_long {
    let (lo, hi) = (c_long::MIN as f32, -(c_long::MIN as f32));
    if f.is_nan() || f < lo || f >= hi {
        c_long::MIN
    } else {
        f as c_long
    }
}

/// `SetLeft((long)x)`: the long overload stores the long as a float.
fn long_f(f: f32) -> f32 {
    to_long(f) as f32
}

fn rect_of(op: Op, b: isize) -> EeWonFrontRect {
    let mut r = EeWonFrontRect::default();
    h(op, b, &mut r as *mut EeWonFrontRect as isize, 0);
    r
}

fn text_bytes(t: isize, s: &[u8]) {
    let z = cz(s);
    h(Op::TextSetText, t, z.as_ptr() as isize, 0);
}

fn text_location(t: isize, x: f32, y: f32) {
    let p = [x, y];
    h(Op::TextSetLocation, t, p.as_ptr() as isize, 0);
}

fn games_construct(b: isize, font: isize) {
    h(Op::GamesSetCopied, b, font, 0);
    h(Op::GamesSetText, b, font, 0);
    let t = h(Op::GamesTextOf, b, 0, 0);
    h(Op::TextSetFgDefault, t, 0, 0);
    h(Op::TextSetTransparent, t, 1, 0);
    h(Op::GamesSelectedBlack, b, 0, 0);
    h(Op::GamesFocusBlack, b, 0, 0);
    for (which, id) in [(0, WF_IDS_CIV_POWER_PASSWORD), (1, WF_IDS_PRIVATE), (2, WF_IDS_IN_PROGRESS)] {
        let mut label = b"[".to_vec();
        label.extend_from_slice(&res(id));
        label.push(b']');
        let z = cz(&label);
        h(Op::GamesLabelSet, b, which, z.as_ptr() as isize);
        h(Op::GamesTextSetLabel, b, which, 0);
        let mut w = 0f32;
        h(Op::TextExtentWidth, h(Op::GamesCopiedOf, b, 0, 0), &mut w as *mut f32 as isize, 0);
        let w = w + 8.0;
        h(Op::GamesWidthSet, b, which, &w as *const f32 as isize);
    }
}

fn games_width(b: isize, which: isize) -> f32 {
    let mut w = 0f32;
    h(Op::GamesWidthGet, b, which, &mut w as *mut f32 as isize);
    w
}

fn games_draw_text(b: isize) -> isize {
    h(Op::GamesDrawScrollBar, b, 0, 0);
    h(Op::GamesBaseDrawText, b, 0, 0);
    let mut result = 0;
    let copied = h(Op::GamesCopiedOf, b, 0, 0);
    let mut rect = EeWonFrontRect::default();
    let count = h(Op::GamesDrawCount, b, 0, 0) as c_ulong;
    for i in 0..count {
        let height = h(Op::GamesLineHeight, b, 0, 0) as c_ulong;
        rect.top = rect_of(Op::GamesCopyRect, b).top + height.wrapping_mul(i) as f32;
        rect.bottom = rect.top + h(Op::GamesLineHeight, b, 0, 0) as c_ulong as f32;
        let ii = i as isize;
        let field = |f: Fd| h(Op::GamesLineGet, b, ii, f as isize);
        if field(Fd::IsInProgress) == 0 && field(Fd::Humans) as c_long != field(Fd::Players) as c_long {
            h(Op::TextSetFgDefault, copied, 0, 0);
            h(Op::TextFontBold, copied, 0, 0);
        } else {
            let grey = [0.8f32, 0.8, 0.8];
            h(Op::TextSetFg, copied, grey.as_ptr() as isize, 0);
            h(Op::TextFontPlain, copied, 0, 0);
        }
        for j in 0..6 {
            let left = rect_of(Op::GamesCopyRect, b).left;
            match j {
                0 => {
                    rect.left = left + 57.0;
                    rect.right = rect.left + 350.0;
                    h(Op::GamesTextSetLineName, b, ii, 0);
                }
                1 => {
                    rect.left = left + 2.0;
                    rect.right = rect.left + 55.0;
                    let s = format!("[{:02}/{:02}]", field(Fd::Humans) as c_long, field(Fd::Players) as c_long);
                    text_bytes(copied, s.as_bytes());
                }
                3 => {
                    rect.left = left + games_width(b, 2) + 415.0;
                    rect.right = rect.left + games_width(b, 0);
                    games_label_or_empty(b, copied, field(Fd::HasPassword) != 0, 0);
                }
                5 => {
                    rect.left = left + 415.0;
                    rect.right = rect.left + games_width(b, 2);
                    games_label_or_empty(b, copied, field(Fd::IsInProgress) != 0, 2);
                }
                4 => {
                    rect.left = left + games_width(b, 2) + games_width(b, 0) + 415.0;
                    rect.right = rect.left + games_width(b, 1);
                    games_label_or_empty(b, copied, field(Fd::IsPrivate) != 0, 1);
                }
                _ => {}
            }
            text_location(copied, rect.left, rect.top);
            result = h(Op::GamesDrawClipped, b, &rect as *const EeWonFrontRect as isize, 0);
        }
    }
    result
}

fn games_label_or_empty(b: isize, copied: isize, on: bool, which: isize) {
    if on {
        h(Op::GamesTextSetLabel, b, which, 0);
    } else {
        text_bytes(copied, b"");
    }
}

fn rooms_construct(b: isize, font: isize) {
    h(Op::RoomsSetCopied, b, font, 0);
    h(Op::RoomsSetText, b, font, 0);
    let t = h(Op::RoomsTextOf, b, 0, 0);
    h(Op::TextSetFgDefault, t, 0, 0);
    h(Op::TextSetTransparent, t, 1, 0);
    h(Op::RoomsSelectedBlack, b, 0, 0);
    h(Op::RoomsFocusBlack, b, 0, 0);
}

fn rooms_draw_text(b: isize) -> isize {
    h(Op::RoomsDrawScrollBar, b, 0, 0);
    h(Op::RoomsBaseDrawText, b, 0, 0);
    let mut result = 0;
    let copied = h(Op::RoomsCopiedOf, b, 0, 0);
    let mut rect = EeWonFrontRect::default();
    let count = h(Op::RoomsDrawCount, b, 0, 0) as c_ulong;
    for i in 0..count {
        let height = h(Op::RoomsLineHeight, b, 0, 0) as c_ulong;
        rect.top = rect_of(Op::RoomsCopyRect, b).top + height.wrapping_mul(i) as f32;
        rect.bottom = rect.top + h(Op::RoomsLineHeight, b, 0, 0) as c_ulong as f32;
        let ii = i as isize;
        for j in 0..4 {
            let left = rect_of(Op::RoomsCopyRect, b).left;
            match j {
                0 => {
                    rect.left = left + 2.0;
                    rect.right = rect.left + 198.0;
                    h(Op::RoomsTextSetLine, b, ii, 0);
                }
                1 => {
                    rect.left = long_f(left + 200.0);
                    rect.right = long_f(rect.left + 445.0);
                    h(Op::RoomsTextSetLine, b, ii, 1);
                }
                2 => {
                    rect.left = long_f(left + 645.0);
                    rect.right = long_f(rect.left + 185.0);
                    let n = h(Op::RoomsLineGet, b, ii, Fd::NumGames as isize) as c_long;
                    text_bytes(copied, n.to_string().as_bytes());
                }
                _ => {
                    rect.left = long_f(left + 830.0);
                    rect.right = long_f(rect.left + 100.0);
                    let n = h(Op::RoomsLineGet, b, ii, Fd::NumPlayers as isize) as c_long;
                    text_bytes(copied, n.to_string().as_bytes());
                }
            }
            text_location(copied, rect.left, rect.top);
            result = h(Op::RoomsDrawClipped, b, &rect as *const EeWonFrontRect as isize, 0);
        }
    }
    result
}

/// `"%ld"` of an `unsigned long` field (printed signed, as the original's format did).
fn ld(v: isize) -> Vec<u8> {
    (v as c_ulong as c_long).to_string().into_bytes()
}

/// A percentage `count / games * 100`, clamped to 0..100, as `"%ld"` of its `(long)`.
fn percent(count: isize, games: isize) -> Vec<u8> {
    let p = count as c_ulong as f32 / games as c_ulong as f32 * 100.0;
    // the original's `p < 0 ? 0 : p > 100 ? 100 : p` (NaN passes through, as there)
    let p = p.clamp(0.0, 100.0);
    to_long(p).to_string().into_bytes()
}

/// The stat box's columns' left offsets (the first column's is its own, unrounded).
const WF_STAT_OFFSETS: [f32; 12] = [0.0, 151.0, 221.0, 291.0, 361.0, 431.0, 501.0, 571.0, 641.0, 711.0, 781.0, 851.0];

fn stat_draw_text(b: isize) -> isize {
    h(Op::StatDrawScrollBar, b, 0, 0);
    h(Op::StatBaseDrawText, b, 0, 0);
    let mut result = 0;
    let t = h(Op::StatTextOf, b, 0, 0);
    let mut rect = EeWonFrontRect::default();
    let mut i: c_ulong = 0;
    while (i as usize) < h(Op::StatCopyCount, b, 0, 0) as usize {
        let row = h(Op::StatRowHeight, b, 0, 0) as c_ulong;
        rect.top = rect_of(Op::StatCopyRect, b).top + row.wrapping_mul(i) as f32;
        rect.bottom = rect.top + h(Op::StatRowHeight, b, 0, 0) as c_ulong as f32 - 1.0;
        let ii = i as isize;
        let field = |f: Fd| h(Op::StatLineGet, b, ii, f as isize);
        for (j, offset) in WF_STAT_OFFSETS.iter().enumerate() {
            let left = rect_of(Op::StatCopyRect, b).left;
            if j == 0 {
                rect.left = left;
                rect.right = rect.left + 150.0;
                h(Op::StatTextSetLineText, b, ii, 0);
            } else {
                rect.left = long_f(left + offset);
                rect.right = long_f(rect.left + 69.0);
                let s = match j {
                    1 => ld(field(Fd::L14)),
                    2 => ld(field(Fd::L18)),
                    3 => ld(field(Fd::StatNumGames)),
                    4 => ld(field(Fd::L20)),
                    5 => ld(field(Fd::L24)),
                    6 => ld(field(Fd::L28)),
                    7 => percent(field(Fd::L20), field(Fd::StatNumGames)),
                    8 => percent(field(Fd::L24), field(Fd::StatNumGames)),
                    9 => percent(field(Fd::L28), field(Fd::StatNumGames)),
                    10 => (field(Fd::L2C) as c_long).to_string().into_bytes(),
                    _ => {
                        let total = field(Fd::TotalTime) as c_ulong;
                        let games = field(Fd::StatNumGames) as c_ulong;
                        // the original divides by zero (a crash) for a line without games
                        let seconds = total.checked_div(games).unwrap_or(0);
                        if seconds >= 3600 {
                            let hours = seconds / 3600;
                            let minutes = (seconds - hours * 3600) / 60;
                            format!("{:02}h:{:02}m", hours as c_long, minutes as c_long).into_bytes()
                        } else {
                            format!("00h:{:02}m", (seconds / 60) as c_long).into_bytes()
                        }
                    }
                };
                text_bytes(t, &s);
            }
            h(Op::TextSetBg, t, [0f32, 0.0, 0.0].as_ptr() as isize, 0);
            h(Op::TextSetFg, t, [0f32, 0.0, 1.0].as_ptr() as isize, 0);
            text_location(t, rect.left, rect.top);
            h(Op::TextSetTransparent, t, 1, 0);
            h(Op::TextSetAlignLeft, t, 0, 0);
            result = h(Op::StatDrawClipped, b, &rect as *const EeWonFrontRect as isize, 0);
        }
        i += 1;
    }
    result
}

fn stat_refresh(b: isize) {
    h(Op::StatClear, b, 0, 0);
    let fields: [(Fd, isize); 8] = [
        (Fd::TotalTime, 2720000),
        (Fd::L20, 324),
        (Fd::L24, 128),
        (Fd::L28, 12),
        (Fd::StatNumGames, 464),
        (Fd::L14, 8888888),
        (Fd::L18, 492),
        (Fd::L2C, -6),
    ];
    for (f, v) in fields {
        h(Op::StatLineSet, f as isize, v, 0);
    }
    let name = cz(b"Joe Mamma II");
    h(Op::StatLineSetText, name.as_ptr() as isize, 0, 0);
    let mut i: c_ulong = 0;
    while (i as usize) < h(Op::StatLinesDisplayed, b, 0, 0) as c_ulong as usize {
        h(Op::StatAppendLine, b, 0, 0);
        i += 1;
    }
}

fn copy_data(base: Op, copy: Op, b: isize) -> isize {
    let r = h(base, b, 0, 0);
    h(copy, b, 0, 0);
    r
}

fn stat_copy_data(b: isize) -> isize {
    let r = h(Op::StatBaseCopyData, b, 0, 0);
    if (h(Op::StatLinesDisplayed, b, 0, 0) as c_ulong) < 2 {
        h(Op::StatClearCopiedSelection, b, 0, 0);
    }
    r
}

fn call(c: EeWonFrontCall, obj: isize, a: isize) -> isize {
    use EeWonFrontCall as C;
    match c {
        C::AgUpdateForm => ag_update_form(),
        C::AgActivateForm => ag_activate_form(obj),
        C::AgBack => {
            h(Op::LeaveRoom, 0, 0, 0);
            h(Op::FormPop, obj, 0, 0)
        }
        C::AgCheckPassword => ag_check_password(obj),
        C::AgClickGameFilter => {
            if wonnet() {
                h(Op::UpdateGameList, 0, 0, 0);
            }
            0
        }
        C::AgCloseForm => {
            ag_close_poll();
            0
        }
        C::AgDeactivateForm => {
            ag_close_poll();
            h(Op::CurrentFormClearUpdateEvent, 0, 0, 0);
            0
        }
        C::AgDoubleclickUser => ag_doubleclick_user(obj),
        C::AgHostOrJoinGame => ag_host_or_join_game(obj),
        C::AgOnGamePassword => ag_on_game_password(a),
        C::AgSendChat => ag_send_chat(obj),
        C::AgToggleHostScreen => ag_toggle_host_screen(obj),
        C::AgToggleJoinScreen => ag_toggle_join_screen(obj),
        C::AgOpenScreen => ag_open_screen(obj),
        C::ArUpdateForm => ar_update_form(obj),
        C::ArActivateForm => ar_activate_form(obj),
        C::ArDeactivateForm => {
            h(Op::CurrentFormClearUpdateEvent, 0, 0, 0);
            0
        }
        C::ArDoubleClickRoom => join_selected_room(obj),
        C::ArClickUpdateAccount => ar_click_update_account(),
        C::ArJoinRoom => ar_join_room(obj),
        C::ArOpenScreen => ar_open_screen(obj),
        C::MuaSubmit => mua_submit(obj),
        C::MuaOpenScreen => mua_open_screen(obj),
        C::SsOk => ss_ok(obj),
        C::SsOpenScreen => ss_open_screen(obj),
        C::StatsOpen => stats_open(obj),
        C::StatsCancel => {
            h(Op::UiPop, 0, 0, 0);
            0
        }
        C::GamesConstruct => {
            games_construct(obj, a);
            0
        }
        C::GamesDestruct => {
            h(Op::TextDelete, h(Op::GamesCopiedOf, obj, 0, 0), 0, 0);
            h(Op::TextDelete, h(Op::GamesTextOf, obj, 0, 0), 0, 0);
            0
        }
        C::GamesCopyData => copy_data(Op::GamesBaseCopyData, Op::GamesCopyText, obj),
        C::GamesDrawText => games_draw_text(obj),
        C::RoomsConstruct => {
            rooms_construct(obj, a);
            0
        }
        C::RoomsDestruct => {
            h(Op::TextDelete, h(Op::RoomsCopiedOf, obj, 0, 0), 0, 0);
            h(Op::TextDelete, h(Op::RoomsTextOf, obj, 0, 0), 0, 0);
            0
        }
        C::RoomsCopyData => copy_data(Op::RoomsBaseCopyData, Op::RoomsCopyText, obj),
        C::RoomsDrawText => rooms_draw_text(obj),
        C::StatConstruct => {
            h(Op::StatSetRowHeight, obj, a, 0);
            h(Op::StatSetText, obj, a, 0);
            0
        }
        C::StatCopyData => stat_copy_data(obj),
        C::StatDrawText => stat_draw_text(obj),
        C::StatRefresh => {
            stat_refresh(obj);
            0
        }
        C::FormManagerGet => manager(),
        C::ArBack | C::MuaCancel | C::SsCancel => h(Op::FormPop, obj, 0, 0),
    }
}

/// One of the C++ functions [`EeWonFrontCall`] names.
#[no_mangle]
pub extern "C" fn ee_won_front_call(call_: EeWonFrontCall, obj: *mut c_void, a: isize) -> isize {
    call(call_, obj as isize, a)
}

/// Resets the originals' statics (the driver, between its two sides' runs).
#[no_mangle]
pub extern "C" fn ee_won_front_reset() {
    set_last(&HOST_OR_JOIN_LAST, 0);
    set_last(&JOIN_ROOM_LAST, 0);
    set_last(&ROOMS_LAST_UPDATE, 0);
}

#[cfg(test)]
#[path = "wonfront_tests.rs"]
mod tests;
