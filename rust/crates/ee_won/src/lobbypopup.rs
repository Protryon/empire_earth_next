//! The lobby's three options popups as one Rust object (R6 WONLobby round 3): ClientOptionsPopup (a player's menu:
//! whisper, invite, friends, ignore, block, AFK, the captain's and moderator's actions), GameOptionsPopup (a game's
//! menu: join, ping, details, remove) and FriendOptionsPopup (a friend's menu: locate, refresh, remove). Each was a
//! `Container` holding a `PopupMenu`, its subject (the client, game or friend) and the room's flags; `Init` filled the
//! menu by rules and `HandleComponentEvent` turned a left click on an entry into the subject's action event.
//!
//! The C++ keeps one shell, `LobbyRustPopup : Container` (HandleComponentEvent forwards here), and the three class
//! names with their `Init` forwarders, so their creators (LanLogic's and StagingLogic's hosts, RoutingLogic) did not
//! change. Every call on the menu, the popup, its subject, a font, the lobby's config or an event is one op of the host
//! given to `ee_won_lobbypopup_new` (`LobbyPopupHost` in `WONLobby/LobbyDialogHost.cpp`), the original's statements
//! with no decision between them. What the original held by `SmartPtr` (the menu, the subject, Init's two local fonts)
//! is held by reference through `AddRef`/`Release` ops in the original's order.

use std::cell::Cell;
use std::ffi::c_void;

/// The popup kinds.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyPopupKind {
    Client = 0,
    Game = 1,
    Friend = 2,
}

/// The menu entries' texts (the lobby's resource strings).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyPopupString {
    Whisper = 0,
    Invite,
    Uninvite,
    /// `FriendOptionsCtrl_Locate_String` (the client menu's and the friend menu's)
    Locate,
    AddToFriends,
    RemoveFromFriends,
    ClearAfk,
    SetAfk,
    Ignore,
    Unignore,
    Block,
    Unblock,
    Warn,
    UnMute,
    Mute,
    Kick,
    Ban,
    GameJoin,
    GamePing,
    GameQueryDetails,
    GameHideDetails,
    GameShowDetails,
    GameRemove,
    FriendRefreshList,
    /// `FriendOptionsCtrl_RemoveFromFriends_String`
    FriendRemove,
}

/// The event an inward `Event` carries (the C++ shell maps the `ComponentEvent`'s type).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyPopupEvent {
    Other = 0,
    /// `ComponentEvent_ListItemUpClicked` (a `ListItemClickedEvent`)
    ItemUpClicked = 1,
}

/// The ops (`a`, `b`, `c` as documented; "the popup" is the C++ popup this object belongs to).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyPopupOp {
    /// `new PopupMenu` (no reference added).
    MenuNew = 1,
    /// `a->AddRef()` (a `SmartPtr`'s reference: the menu, the subject, a font).
    AddRef,
    /// `a->Release()`.
    Release,
    /// `this->AddChild((Component*)a)`.
    AddChild,
    /// `((PopupMenu*)a)->Clear(false)`.
    MenuClear,
    /// `((PopupMenu*)a)->mFont.get()` with a reference added (the original's local `SmartPtr`), or NULL.
    MenuFont,
    /// `this->GetFontMod((Font*)a, 1, 0)` with a reference added.
    PopupFontMod,
    /// `((PopupMenu*)a)->GetFontMod((Font*)b, 1, 0)` with a reference added.
    MenuFontMod,
    /// `((PopupMenu*)a)->SetFont((Font*)b)`.
    MenuSetFont,
    /// `((PopupMenu*)a)->mFont = (Font*)b`.
    MenuAssignFont,
    /// `((PopupMenu*)a)->AddItem(((LobbyClient*)b)->GetName(), 0)->mSelectable = false`.
    MenuTitleClient,
    /// `((PopupMenu*)a)->AddItem(((LobbyGame*)b)->GetName(), 0)->mSelectable = false`.
    MenuTitleGame,
    /// `((PopupMenu*)a)->AddItem(((LobbyFriend*)b)->GetName(), 0)->mSelectable = false`.
    MenuTitleFriend,
    /// `sprintf(aBuffer, "%d", b); ((PopupMenu*)a)->AddItem(GUIString(aBuffer), 0)->mSelectable = false`.
    MenuTitleNumber,
    /// `((PopupMenu*)a)->AddItem(the string b, NULL, NULL, c)`.
    MenuAddItem,
    /// `((PopupMenu*)a)->AddSeperator()`.
    MenuAddSeparator,
    /// `((Component*)a)->Width()`.
    Width,
    /// `((Component*)a)->Height()`.
    Height,
    /// `this->SetSize(a, b)`.
    SetSize,
    /// `((LobbyClient*)a)->IsModerator()` (0 or 1).
    ClientIsModerator,
    /// `((LobbyClient*)a)->IsCaptain(b != 0)` (0 or 1).
    ClientIsCaptain,
    /// `((LobbyClient*)a)->GetClientId()`.
    ClientId,
    /// `((LobbyClient*)a)->GetWONUserId()`.
    ClientWonUserId,
    /// `((LobbyClient*)a)->GetGameGroupId()`.
    ClientGameGroupId,
    /// `((LobbyClient*)a)->IsInvited()` (0 or 1).
    ClientIsInvited,
    /// `((LobbyClient*)a)->IsFriend()` (0 or 1).
    ClientIsFriend,
    /// `((LobbyClient*)a)->IsAway()` (0 or 1).
    ClientIsAway,
    /// `((LobbyClient*)a)->IsIgnored()` (0 or 1).
    ClientIsIgnored,
    /// `((LobbyClient*)a)->IsBlocked()` (0 or 1).
    ClientIsBlocked,
    /// `((LobbyClient*)a)->IsMuted(b != 0)` (0 or 1).
    ClientIsMuted,
    /// `((LobbyGame*)a)->CanJoin()` (0 or 1).
    GameCanJoin,
    /// `((LobbyGame*)a)->IsInProgress()` (0 or 1).
    GameInProgress,
    /// `((LobbyGame*)a)->GetGameType()`.
    GameType,
    /// `((LobbyGame*)a)->GetShowDetails()` (0 or 1).
    GameShowDetails,
    /// `GetLobbyConfig()`.
    Config,
    /// `((LobbyConfig*)a)->GetNoPingInProgress()` (0 or 1).
    ConfigNoPingInProgress,
    /// `Container::HandleComponentEvent(a)` on the popup.
    BaseEvent,
    /// `((ListItemClickedEvent*)a)->mItem.get()`.
    EventItem,
    /// `((ListItemClickedEvent*)a)->mButton`.
    EventButton,
    /// `((MenuItem*)a)->mMenuId`.
    ItemMenuId,
    /// `this->DeliverEvent(new ClientActionEvent(a, c, (LobbyClient*)b), false)`.
    DeliverClient,
    /// `this->DeliverEvent(new GameActionEvent(a, (LobbyGame*)b), false)`.
    DeliverGame,
    /// `this->DeliverEvent(new FriendActionEvent(a, (LobbyFriend*)b, c), false)`.
    DeliverFriend,
    /// `this->GetRoot()->ClosePopup(this)`.
    ClosePopup,
}

/// The inward calls.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyPopupCall {
    /// The constructor's body: the menu made, held and added.
    Construct = 1,
    /// ClientOptionsPopup: `Init((LobbyClient*)a, (LobbyClient*)b, c)`; GameOptionsPopup: `Init((LobbyGame*)a)`;
    /// FriendOptionsPopup: `Init((LobbyFriend*)a, b)`.
    Init,
    /// `HandleComponentEvent(a)`: `b` the `EeWonLobbyPopupEvent`.
    Event,
    /// a member: 0 the menu, 1 the subject, 2 the room flags.
    Field,
}

/// The host: `(popup, op, a, b, c) -> result`.
pub type EeWonLobbyPopupHostFn =
    Option<unsafe extern "C" fn(popup: *mut c_void, op: EeWonLobbyPopupOp, a: isize, b: isize, c: isize) -> isize>;

use EeWonLobbyPopupKind as Kind;
use EeWonLobbyPopupOp as Op;
use EeWonLobbyPopupString as S;

/// One options popup's Rust half.
pub struct EeWonLobbyPopup {
    popup: *mut c_void,
    host: EeWonLobbyPopupHostFn,
    /// the model's ops answered by its Rust objects (modelobj.rs; the game's), not the host
    model: Cell<bool>,
    kind: Kind,
    /// `mPopupMenu`.
    menu: Cell<isize>,
    /// `mClient`, `mGame` or `mFriend`.
    subject: Cell<isize>,
    /// `mRoomSpecFlags` (Client and Friend; the constructor left it unset: 0 here).
    room_flags: Cell<i32>,
}

impl EeWonLobbyPopup {
    pub fn new(popup: *mut c_void, kind: Kind, host: EeWonLobbyPopupHostFn) -> Self {
        Self {
            popup,
            host,
            model: Cell::new(false),
            kind,
            menu: Cell::new(0),
            subject: Cell::new(0),
            room_flags: Cell::new(0),
        }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize) -> isize {
        if self.model.get() {
            if let Some(r) = crate::modelobj::popup_op(op, a, b) {
                return r;
            }
        }
        match self.host {
            // SAFETY: the host is the C++ popup's (or a test's), called with the popup it was given for
            Some(f) => unsafe { f(self.popup, op, a, b, c) },
            None => 0,
        }
    }

    fn ask(&self, op: Op, a: isize) -> bool {
        self.op(op, a, 0, 0) != 0
    }

    /// A `SmartPtr` member's assignment: nothing for the same pointer, else the old one released, the new one held.
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

    fn release(&self, p: isize) {
        if p != 0 {
            self.op(Op::Release, p, 0, 0);
        }
    }

    fn add(&self, s: S, action: i32) {
        self.op(Op::MenuAddItem, self.menu.get(), s as isize, action as isize);
    }

    fn separator(&self) {
        self.op(Op::MenuAddSeparator, self.menu.get(), 0, 0);
    }

    fn size_to_menu(&self) {
        let w = self.op(Op::Width, self.menu.get(), 0, 0);
        let h = self.op(Op::Height, self.menu.get(), 0, 0);
        self.op(Op::SetSize, w, h, 0);
    }

    fn construct(&self) {
        let menu = self.op(Op::MenuNew, 0, 0, 0);
        self.assign(&self.menu, menu);
        self.op(Op::AddChild, self.menu.get(), 0, 0);
    }

    fn client_init(&self, client: isize, me: isize, spec: i32) {
        if client == 0 || me == 0 {
            return;
        }
        self.assign(&self.subject, client);
        self.room_flags.set(spec);
        let menu = self.menu.get();
        self.op(Op::MenuClear, menu, 0, 0);
        let lan = spec & 2 != 0;
        let game = spec & 4 != 0;
        let moderator = self.ask(Op::ClientIsModerator, me);
        let captain = self.op(Op::ClientIsCaptain, me, lan as isize, 0) != 0;
        let my_id = self.op(Op::ClientId, me, 0, 0);
        let is_self = my_id == self.op(Op::ClientId, client, 0, 0);
        let old_font = self.op(Op::MenuFont, menu, 0, 0);
        let bold = self.op(Op::PopupFontMod, old_font, 0, 0);
        self.op(Op::MenuSetFont, menu, bold, 0);
        self.op(Op::MenuTitleClient, menu, client, 0);
        if moderator {
            let id = self.op(Op::ClientWonUserId, client, 0, 0);
            self.op(Op::MenuTitleNumber, menu, id, 0);
        }
        self.op(Op::MenuAssignFont, menu, old_font, 0);
        self.add(S::Whisper, 16);
        if !is_self && self.op(Op::ClientIsCaptain, me, 1, 0) != 0 && {
            let theirs = self.op(Op::ClientGameGroupId, client, 0, 0);
            theirs != self.op(Op::ClientGameGroupId, me, 0, 0)
        } {
            if !self.ask(Op::ClientIsInvited, self.subject.get()) {
                self.add(S::Invite, 6);
            } else {
                self.add(S::Uninvite, 13);
            }
        }
        if game {
            if !is_self {
                self.add(S::Locate, 17);
            }
            if !self.ask(Op::ClientIsFriend, client) {
                self.add(S::AddToFriends, 1);
            } else {
                self.add(S::RemoveFromFriends, 9);
            }
            self.separator();
        }
        if is_self && game {
            if self.ask(Op::ClientIsAway, client) {
                self.add(S::ClearAfk, 4);
            } else {
                self.add(S::SetAfk, 10);
            }
        } else {
            if !self.ask(Op::ClientIsIgnored, client) {
                self.add(S::Ignore, 5);
            } else {
                self.add(S::Unignore, 12);
            }
            if game {
                if !self.ask(Op::ClientIsBlocked, client) {
                    self.add(S::Block, 3);
                } else {
                    self.add(S::Unblock, 11);
                }
            }
        }
        if game {
            if moderator || captain {
                self.separator();
                if moderator {
                    self.add(S::Warn, 15);
                }
                if self.op(Op::ClientIsMuted, client, lan as isize, 0) != 0 {
                    self.add(S::UnMute, 14);
                } else {
                    self.add(S::Mute, 8);
                }
                if !is_self {
                    self.add(S::Kick, 7);
                    self.add(S::Ban, 2);
                }
            }
        } else if captain && !is_self {
            self.separator();
            self.add(S::Kick, 7);
        }
        self.size_to_menu();
        // the locals' SmartPtrs, in reverse order
        self.release(bold);
        self.release(old_font);
    }

    fn game_init(&self, game: isize) {
        let menu = self.menu.get();
        self.op(Op::MenuClear, menu, 0, 0);
        self.assign(&self.subject, game);
        let old_font = self.op(Op::MenuFont, menu, 0, 0);
        let bold = self.op(Op::MenuFontMod, menu, old_font, 0);
        self.op(Op::MenuSetFont, menu, bold, 0);
        self.op(Op::MenuTitleGame, menu, game, 0);
        self.op(Op::MenuSetFont, menu, old_font, 0);
        self.separator();
        if self.ask(Op::GameCanJoin, game) {
            self.add(S::GameJoin, 1);
        }
        let mut allow_ping = true;
        if self.ask(Op::GameInProgress, game) {
            let config = self.op(Op::Config, 0, 0, 0);
            if config != 0 {
                allow_ping = !self.ask(Op::ConfigNoPingInProgress, config);
            }
        }
        if allow_ping {
            if self.op(Op::GameType, game, 0, 0) != 1 {
                self.add(S::GamePing, 2);
            }
            self.add(S::GameQueryDetails, 3);
        }
        if self.ask(Op::GameShowDetails, game) {
            self.add(S::GameHideDetails, 5);
        } else {
            self.add(S::GameShowDetails, 4);
        }
        if self.op(Op::GameType, game, 0, 0) == 3 {
            self.separator();
            self.add(S::GameRemove, 6);
        }
        self.size_to_menu();
        self.release(bold);
        self.release(old_font);
    }

    fn friend_init(&self, friend: isize, spec: i32) {
        if friend == 0 {
            return;
        }
        self.assign(&self.subject, friend);
        self.room_flags.set(spec);
        let menu = self.menu.get();
        self.op(Op::MenuClear, menu, 0, 0);
        let old_font = self.op(Op::MenuFont, menu, 0, 0);
        let bold = self.op(Op::PopupFontMod, old_font, 0, 0);
        self.op(Op::MenuSetFont, menu, bold, 0);
        self.op(Op::MenuTitleFriend, menu, friend, 0);
        self.op(Op::MenuSetFont, menu, old_font, 0);
        self.separator();
        self.add(S::Locate, 1);
        self.add(S::FriendRefreshList, 2);
        self.separator();
        self.add(S::FriendRemove, 3);
        self.size_to_menu();
        self.release(bold);
        self.release(old_font);
    }

    /// HandleComponentEvent: a left click on an entry sends its action for the subject and closes the popup.
    fn event(&self, event: isize, kind: u32) {
        if kind != EeWonLobbyPopupEvent::ItemUpClicked as u32 {
            self.op(Op::BaseEvent, event, 0, 0);
            return;
        }
        let item = self.op(Op::EventItem, event, 0, 0);
        if item == 0 || self.op(Op::EventButton, event, 0, 0) != 0 {
            return;
        }
        let action = self.op(Op::ItemMenuId, item, 0, 0);
        if action != 0 {
            let subject = self.subject.get();
            let flags = self.room_flags.get() as isize;
            match self.kind {
                Kind::Client => self.op(Op::DeliverClient, action, subject, flags),
                Kind::Game => self.op(Op::DeliverGame, action, subject, 0),
                Kind::Friend => self.op(Op::DeliverFriend, action, subject, flags),
            };
        }
        self.op(Op::ClosePopup, 0, 0, 0);
    }

    /// One inward call.
    pub fn call(&self, call: EeWonLobbyPopupCall, a: isize, b: isize, c: isize) -> isize {
        use EeWonLobbyPopupCall as C;
        match call {
            C::Construct => self.construct(),
            C::Init => match self.kind {
                Kind::Client => self.client_init(a, b, c as i32),
                Kind::Game => self.game_init(a),
                Kind::Friend => self.friend_init(a, b as i32),
            },
            C::Event => self.event(a, b as u32),
            C::Field => {
                return match a {
                    0 => self.menu.get(),
                    1 => self.subject.get(),
                    2 => self.room_flags.get() as isize,
                    _ => 0,
                }
            }
        }
        0
    }

    /// The destructor: the `SmartPtr` members in reverse order (GameOptionsPopup's menu was its second member).
    fn release_all(&self) {
        let (first, second) = match self.kind {
            Kind::Game => (&self.subject, &self.menu),
            Kind::Client | Kind::Friend => (&self.menu, &self.subject),
        };
        self.release(second.replace(0));
        self.release(first.replace(0));
    }
}

/// A new popup object for the C++ `popup` of `kind`.
/// As `ee_won_lobbypopup_new`, with the model's ops answered by its Rust objects (the game's).
#[no_mangle]
pub extern "C" fn ee_won_lobbypopup_new_model(
    popup: *mut c_void,
    kind: EeWonLobbyPopupKind,
    host: EeWonLobbyPopupHostFn,
) -> *mut EeWonLobbyPopup {
    let p = EeWonLobbyPopup::new(popup, kind, host);
    p.model.set(true);
    Box::into_raw(Box::new(p))
}

#[no_mangle]
pub extern "C" fn ee_won_lobbypopup_new(
    popup: *mut c_void,
    kind: EeWonLobbyPopupKind,
    host: EeWonLobbyPopupHostFn,
) -> *mut EeWonLobbyPopup {
    Box::into_raw(Box::new(EeWonLobbyPopup::new(popup, kind, host)))
}

/// Releases the members (reverse member order) and frees the object.
///
/// # Safety
/// `p` came from `ee_won_lobbypopup_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbypopup_free(p: *mut EeWonLobbyPopup) {
    if !p.is_null() {
        // SAFETY: the contract; the releases run through a shared reference before the box is dropped
        unsafe { (*p).release_all() };
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(p) });
    }
}

/// One inward call (`EeWonLobbyPopupCall`).
///
/// # Safety
/// `p` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbypopup_call(
    p: *const EeWonLobbyPopup,
    call: EeWonLobbyPopupCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out may re-enter
    unsafe { (*p).call(call, a, b, c) }
}

#[cfg(test)]
#[path = "lobbypopup_tests.rs"]
mod tests;
