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
}

/// A control's type: the `SmartPtr<T>` the original's `SafeGetComponent` filled (its `dynamic_cast`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyDlgType {
    Input = 0,
    Button = 1,
    Label = 2,
    Combo = 3,
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
}

/// The event an inward `Event` carries (the C++ shell maps the `ComponentEvent`'s type).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyDlgEvent {
    Other = 0,
    InputTextChanged = 1,
    ButtonPressed = 2,
    ComboSelChanged = 3,
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
    /// a scalar member: 0 the game port, 1 the game type, 2 the action type, 3 whether the reason is optional.
    Field,
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
        let order: Vec<usize> = if self.kind == EeWonLobbyDlgKind::AdminAction {
            ADMIN_INIT_ORDER.to_vec()
        } else {
            (0..table.len()).collect()
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
    }

    fn event(&self, event: isize, kind: u32, component: isize) {
        let text_changed = kind == EeWonLobbyDlgEvent::InputTextChanged as u32;
        match self.kind {
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
                    _ => 0,
                }
            }
        }
        0
    }

    /// The destructor: the controls released in reverse member order (the `SmartPtr` members' destruction).
    fn release_all(&self) {
        for i in (0..slots(self.kind).len()).rev() {
            let c = self.controls[i].replace(0);
            if c != 0 {
                self.op(Op::Release, c, 0, 0);
            }
        }
    }
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
