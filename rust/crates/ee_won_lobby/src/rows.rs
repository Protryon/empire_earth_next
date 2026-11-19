//! The lobby's list rows as Rust rows (R6 WONLobby round 16): RoomListItem (the room tree's servers and rooms),
//! FriendsListItem (the friends list), UserListItem (a room's users), GameUserListItem (the game room's players) and
//! ServerDetailsItem with the EE game's EELobbyServerDetailsItem (the game lists' rows, whose virtuals were already
//! forwards to their control, srvlistctl.rs).
//! Each was a C++ `ListItem` or `TextListItem` subclass with a few members and its own Paint, CalcDimensions and
//! Compare; each is now ee_won_gui's generic shell (`RustRow` or `RustTextRow`, rowobj.rs) over a state here, made by
//! the controls' row ops (`RoomItemServer`, `FriendRowNew`, `ItemNew`, ...), which this module answers in the game
//! (`ctl_op`, reached from lobbyctl.rs after the model's answers).
//!
//! What stays C++: the shells, the `TextListItem` base (its text, colour, font and image, which the room, friend and
//! user rows keep there: the controls' generic text-row ops write them, and the lists read them) and every call on a
//! graphics context, a list, a font, an image or a text: one op each of the controls' host (`EeWonLobbyCtlOp`,
//! given with `ee_won_lobby_rows_host_register`: LobbyCtrlHost.cpp's, called with no control), the original's
//! statement with no decision. The model's reads and the addresses are modelobj.rs's and ipobj.rs's in the game; a driver's world
//! (`ee_won_lobby_rows_host_world`) answers them as host ops too.
//!
//! The states are `Cell`s: a host op may re-enter the row (a paint never does, but nothing is borrowed across one).

use std::cell::Cell;
use std::ffi::c_void;
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};

use ee_won_gui::rowobj::{self, EeWonRow, EeWonRowCall, EeWonRowTable};

use crate::lobbyctl::{
    EeWonLobbyCtlClientTest as Test, EeWonLobbyCtlImage as Icon, EeWonLobbyCtlItemField as F, EeWonLobbyCtlOp as LOp,
};
use crate::modelobj::{EeWonModelCall as MC, EeWonModelField as MF};
use crate::resource::EeWonLobbyRes as R;

use crate::lobbyctl::EeWonLobbyCtlHostFn;
use LOp as Op;

static GAME_HOST: AtomicUsize = AtomicUsize::new(0);

thread_local! {
    static WORLD_HOST: Cell<EeWonLobbyCtlHostFn> = const { Cell::new(None) };
}

/// Registers the game's rows host: the controls' host (LobbyCtrlHost.cpp, at start), called with no control.
#[no_mangle]
pub extern "C" fn ee_won_lobby_rows_host_register(host: EeWonLobbyCtlHostFn) {
    GAME_HOST.store(host.map_or(0, |f| f as usize), Ordering::Release);
}

/// A driver's world on this thread (NULL removes it): every op, the model's and the addresses' too, goes to it.
#[no_mangle]
pub extern "C" fn ee_won_lobby_rows_host_world(host: EeWonLobbyCtlHostFn) {
    WORLD_HOST.with(|h| h.set(host));
}

fn world() -> bool {
    WORLD_HOST.with(|h| h.get()).is_some()
}

fn h(op: Op, a: isize, b: isize, c: isize) -> isize {
    let f = match WORLD_HOST.with(|h| h.get()) {
        Some(f) => f,
        None => {
            let p = GAME_HOST.load(Ordering::Acquire);
            if p == 0 {
                return 0;
            }
            // SAFETY: a value `ee_won_lobby_rows_host_register` stored from a host function pointer
            unsafe {
                std::mem::transmute::<usize, unsafe extern "C" fn(*mut c_void, Op, isize, isize, isize) -> isize>(p)
            }
        }
    };
    // SAFETY: the host answers its ops; no state is borrowed across the call
    unsafe { f(std::ptr::null_mut(), op, a, b, c) }
}

fn pt(x: i32, y: i32) -> isize {
    crate::lobbyctl::pack(x, y)
}

fn mget(obj: isize, f: MF) -> isize {
    if world() {
        return h(Op::MGet, obj, f as isize, 0);
    }
    crate::modelobj::ctl_op(LOp::MGet, obj, f as isize, 0).unwrap_or(0)
}

fn mcall(obj: isize, c: MC, a: isize) -> isize {
    if world() {
        return h(Op::MCall, obj, c as isize, a);
    }
    crate::modelobj::ctl_op(LOp::MCall, obj, c as isize, a).unwrap_or(0)
}

fn addr_clone(a: isize) -> isize {
    if world() {
        return h(Op::AddrClone, a, 0, 0);
    }
    crate::modelobj::ctl_op(LOp::AddrClone, a, 0, 0).unwrap_or(0)
}

fn addr_free(a: isize) {
    if world() {
        h(Op::AddrFree, a, 0, 0);
    } else {
        crate::modelobj::ctl_op(LOp::AddrFree, a, 0, 0);
    }
}

fn get(item: isize, f: F) -> isize {
    h(Op::ItemGet, item, f as isize, 0)
}

fn set(item: isize, f: F, v: isize) {
    h(Op::ItemSet, item, f as isize, v);
}

/// A `SmartPtr` member assigned: the old object released, the new one referenced (when they differ).
fn assign(slot: &Cell<isize>, p: isize) {
    let old = slot.get();
    if old != p {
        if old != 0 {
            h(Op::Release, old, 0, 0);
        }
        if p != 0 {
            h(Op::AddRef, p, 0, 0);
        }
        slot.set(p);
    }
}

/// A `SmartPtr` member's end.
fn drop_ref(slot: &Cell<isize>) {
    let p = slot.replace(0);
    if p != 0 {
        h(Op::Release, p, 0, 0);
    }
}

/// `(a - b) / 2` in `int`s.
fn half(a: isize, b: isize) -> i32 {
    (a as i32).wrapping_sub(b as i32) / 2
}

// ---- the kinds

/// RoomListItem's own members (the text, colour, font and image are its `TextListItem` base's).
struct Room {
    loading: bool,
    /// `mAddress` (an `EeWonIpAddr` copy, owned)
    address: Cell<isize>,
    /// `mGroupId` (0xffff on a server row)
    group_id: Cell<u32>,
}

/// FriendsListItem's members.
struct Friend {
    friend: Cell<isize>,
    separator: Cell<bool>,
    sort_group: Cell<i8>,
}

/// UserListItem's members.
struct User {
    client: Cell<isize>,
    sort_key: Cell<i8>,
    show_separator: Cell<bool>,
}

/// GameUserListItem's members.
struct GameUser {
    client: Cell<isize>,
    image: Cell<isize>,
    font: Cell<isize>,
}

macro_rules! table {
    ($name:ident, $ty:ty, $call:ident) => {
        static $name: EeWonRowTable = EeWonRowTable {
            call: |s, c, item, list, a, b, cc| {
                // SAFETY: the state this table was made with
                let s = unsafe { &*(s as *const $ty) };
                $call(s, c, item as isize, list as isize, a, b, cc)
            },
            free: |s| {
                // SAFETY: the state this table was made with, freed once
                let s = unsafe { Box::from_raw(s as *mut $ty) };
                s.end();
            },
        };
    };
}

table!(ROOM, Room, room_call);
table!(FRIEND, Friend, friend_call);
table!(USER, User, user_call);
table!(GAME_USER, GameUser, game_user_call);
table!(SERVER, Server, server_call);

/// A row of a kind over its state, its shell made (a text row or not) and bound.
fn make<T>(table: &'static EeWonRowTable, state: T, text: bool) -> (isize, *const T) {
    let s = Box::into_raw(Box::new(state));
    let row: *mut EeWonRow = rowobj::new(table, s as *mut c_void);
    let item = h(Op::RowShellNew, row as isize, text as isize, 0);
    // SAFETY: a new row
    unsafe { rowobj::bind(row, item as *mut c_void) };
    (item, s)
}

fn state_of<T>(item: isize, table: &'static EeWonRowTable) -> Option<&'static T> {
    // SAFETY: a row of this table holds a `T` until its shell's destructor frees it
    rowobj::of(item as *mut c_void, table).map(|s| unsafe { &*(s as *const T) })
}

/// The friend, user and game list rows' own fields (`EeWonLobbyCtlItemField`).
fn row_field(f: isize) -> Option<F> {
    [F::FriendSeparator, F::FriendSortGroup, F::UserSortKey, F::UserShowSeparator, F::IconId, F::SummaryHeight]
        .into_iter()
        .find(|x| *x as isize == f)
}

/// The other row's state in a Compare (its `EeWonRow*`).
fn other_of<T>(row: isize, table: &'static EeWonRowTable) -> Option<&'static T> {
    // SAFETY: the shell passes its own row
    unsafe { rowobj::state(row as *const EeWonRow, table) }.map(|s| unsafe { &*(s as *const T) })
}

// ---- RoomListItem

impl Room {
    fn end(&self) {
        let a = self.address.replace(0);
        if a != 0 {
            addr_free(a);
        }
    }
}

/// `new RoomListItem(server, loading)`.
fn room_server(server: isize, loading: bool) -> isize {
    let (item, s) = make(&ROOM, Room { loading, address: Cell::new(0), group_id: Cell::new(0xffff) }, true);
    // SAFETY: the new row's state
    let s = unsafe { &*s };
    s.address.set(addr_clone(mget(server, MF::ServerAddress)));
    room_update(item, s, server, 0);
    item
}

/// `new RoomListItem(group, server)`.
fn room_group(group: isize, server: isize) -> isize {
    let (item, s) = make(&ROOM, Room { loading: false, address: Cell::new(0), group_id: Cell::new(0) }, true);
    // SAFETY: the new row's state
    let s = unsafe { &*s };
    s.address.set(addr_clone(mget(server, MF::ServerAddress)));
    s.group_id.set(mget(group, MF::GroupId) as u16 as u32);
    room_update(item, s, server, group);
    item
}

/// `UpdateText(server, group)`: 'querying server', or the room or server name with its player count; the lock icon.
fn room_update(item: isize, s: &Room, server: isize, group: isize) {
    if s.loading {
        h(Op::ItemSetTextRes, item, R::RoomCtrl_QueryingServer_String as isize, 0);
    } else if group != 0 {
        let count = mget(group, MF::GroupNumClients) as u32;
        let name = mget(group, MF::GroupName);
        let shown = mget(group, MF::GroupNumClients) as u32 as i32 > 0;
        h(Op::ItemSetTextCount, item, name, crate::lobbyctl::pack(count as i32, shown as i32));
    } else if server != 0 {
        let count = mcall(server, MC::ServerNumClientsNow, 0) as i32;
        let name = mget(server, MF::ServerName);
        let shown = mcall(server, MC::ServerNumClientsNow, 0) as i32 != 0;
        h(Op::ItemSetTextCount, item, name, crate::lobbyctl::pack(count, shown as i32));
    } else {
        h(Op::ItemTextErase, item, 0, 0);
    }
    if group != 0 && mcall(group, MC::GroupKind, 0) & 1 != 0 {
        h(Op::ItemSetImageRes, item, R::RoomCtrl_Password_Image as isize, 0);
    } else {
        h(Op::ItemSetImageRes, item, -1, 0);
    }
}

fn room_call(s: &Room, c: EeWonRowCall, item: isize, list: isize, a: isize, b: isize, _: isize) -> isize {
    match c {
        // Orders room tree entries by their text, ignoring case
        EeWonRowCall::Compare => h(Op::ItemTextCompare, item, b, 0) as i32 as isize,
        // Measures the entry: server entries use a bold font, and the lock icon adds width and height
        EeWonRowCall::CalcDimensions => {
            if h(Op::ItemFont, item, 0, 0) == 0 {
                if s.group_id.get() == 0xffff && !s.loading {
                    let f = h(Op::FontMod, list, h(Op::ListFont, list, 0, 0), 1);
                    h(Op::ItemSetFont, item, f, 0);
                } else {
                    h(Op::ItemSetFont, item, h(Op::ListFont, list, 0, 0), 0);
                }
            }
            if (get(item, F::Color) as i32) < 0 {
                set(item, F::Color, h(Op::ListTextColor, list, 0, 0) as i32 as isize);
            }
            let font = h(Op::ItemFont, item, 0, 0);
            set(item, F::Width, (h(Op::ItemTextWidth, item, font, 0) as i32).wrapping_add(2) as isize);
            set(item, F::Height, h(Op::FontHeight, font, 0, 0) as i32 as isize);
            let image = h(Op::ItemImage, item, 0, 0);
            if image != 0 {
                let w = (get(item, F::Width) as i32).wrapping_add(h(Op::ImageWidth, image, 0, 0) as i32 + 2);
                set(item, F::Width, w as isize);
                let ih = h(Op::ImageHeight, image, 0, 0) as i32;
                if ih > get(item, F::Height) as i32 {
                    set(item, F::Height, ih as isize);
                }
            }
            0
        }
        // Draws the entry with its selection highlight, lock icon and text
        EeWonRowCall::Paint => {
            let g = a;
            let mut color = get(item, F::Color) as i32;
            let height = get(item, F::Height);
            if get(item, F::Selected) != 0 {
                h(Op::GfxSetColor, g, h(Op::GfxSelBack, g, list, 0), 0);
                let mut width = h(Op::ListPaintWidth, list, 0, 0) as i32;
                if h(Op::ListFlagSet, list, 0x10, 0) == 0 {
                    width = get(item, F::Width) as i32;
                }
                h(Op::GfxFillRect, g, pt(0, 0), pt(width, height as i32));
                color = h(Op::GfxSelFore, g, list, get(item, F::Color) as i32 as isize) as i32;
            }
            let mut x = 2;
            let image = h(Op::ItemImage, item, 0, 0);
            if image != 0 {
                let ih = h(Op::ImageHeight, image, 0, 0);
                h(Op::GfxDrawImage, g, image, pt(x, half(height, ih)));
                x = (h(Op::ImageWidth, image, 0, 0) as i32) + 4;
            }
            h(Op::GfxSetColor, g, color as isize, 0);
            let font = h(Op::ItemFont, item, 0, 0);
            h(Op::GfxSetFont, g, font, 0);
            let fh = h(Op::FontHeight, font, 0, 0);
            h(Op::ItemTextDraw, item, g, pt(x, half(height, fh)));
            0
        }
        // the shells keep ListItem's CanSelect and MouseDown
        EeWonRowCall::CanSelect | EeWonRowCall::MouseDown => 0,
    }
}

// ---- FriendsListItem and UserListItem

impl Friend {
    fn end(&self) {
        drop_ref(&self.friend);
    }
}

impl User {
    fn end(&self) {
        drop_ref(&self.client);
    }
}

/// A separator's size (8 pixels high when shown, else 1).
fn separator_size(item: isize, shown: bool) {
    set(item, F::Width, 1);
    set(item, F::Height, if shown { 8 } else { 1 });
}

/// A shown separator: a line across the list.
fn separator_paint(item: isize, g: isize, list: isize) {
    let mut width = (h(Op::ListTotalWidth, list, 0, 0) as i32).wrapping_add(10);
    let pw = h(Op::ListPaintWidth, list, 0, 0) as i32;
    if pw > width {
        width = pw;
    }
    let y = (get(item, F::Height) as i32) >> 2;
    h(Op::GfxSetSysColor, g, 14, 0); // StandardColor_Text
    h(Op::GfxDrawLine, g, pt(0, y), pt(width, y));
}

fn friend_call(s: &Friend, c: EeWonRowCall, item: isize, list: isize, a: isize, b: isize, _: isize) -> isize {
    match c {
        // Sizes a separator (8 pixels high when shown, else 1) or a friend's text entry
        EeWonRowCall::CalcDimensions => {
            if s.friend.get() == 0 {
                separator_size(item, s.separator.get());
            } else {
                h(Op::RowTextCalc, item, list, 0);
            }
            0
        }
        // Orders entries by sort group, highest first (online friends, separator, others), then by text
        EeWonRowCall::Compare => {
            let other = other_of::<Friend>(a, &FRIEND).map_or(0, |o| o.sort_group.get() as i32);
            let r = other - s.sort_group.get() as i32;
            if r != 0 {
                return r as isize;
            }
            h(Op::RowTextCompare, item, b, list) as i32 as isize
        }
        // Draws a shown separator as a line across the list, or a friend's text entry
        EeWonRowCall::Paint => {
            if s.friend.get() == 0 {
                if s.separator.get() {
                    separator_paint(item, a, list);
                }
            } else {
                h(Op::RowTextPaint, item, a, list);
            }
            0
        }
        EeWonRowCall::CanSelect | EeWonRowCall::MouseDown => 0,
    }
}

fn user_call(s: &User, c: EeWonRowCall, item: isize, list: isize, a: isize, b: isize, _: isize) -> isize {
    match c {
        // Orders entries by sort key, highest first (admins, moderators, separator, friends, separator, others), then
        // text
        EeWonRowCall::Compare => {
            let other = other_of::<User>(a, &USER).map_or(0, |o| o.sort_key.get() as i32);
            let r = other - s.sort_key.get() as i32;
            if r != 0 {
                return r as isize;
            }
            h(Op::RowTextCompare, item, b, list) as i32 as isize
        }
        // Sizes a separator (8 pixels high when shown, else 1) or a client's text entry
        EeWonRowCall::CalcDimensions => {
            if s.client.get() == 0 {
                separator_size(item, s.show_separator.get());
            } else {
                h(Op::RowTextCalc, item, list, 0);
            }
            0
        }
        // Draws a shown separator as a line across the list, or a client's text entry
        EeWonRowCall::Paint => {
            if s.client.get() == 0 {
                if s.show_separator.get() {
                    separator_paint(item, a, list);
                }
            } else {
                h(Op::RowTextPaint, item, a, list);
            }
            0
        }
        EeWonRowCall::CanSelect | EeWonRowCall::MouseDown => 0,
    }
}

// ---- GameUserListItem

impl GameUser {
    fn end(&self) {
        // the members in reverse order: the font, the image, the client
        drop_ref(&self.font);
        drop_ref(&self.image);
        drop_ref(&self.client);
    }
}

/// `new GameUserListItem(client)` with a reference added (the original's local `SmartPtr`).
fn game_user_new(client: isize) -> isize {
    let (item, s) = make(&GAME_USER, GameUser { client: Cell::new(0), image: Cell::new(0), font: Cell::new(0) }, false);
    // SAFETY: the new row's state
    assign(unsafe { &(*s).client }, client);
    h(Op::AddRef, item, 0, 0);
    item
}

/// The client test numbered `t`.
fn test_of(t: isize) -> Option<Test> {
    [
        Test::IsIgnored,
        Test::IsBlocked,
        Test::IsMuted,
        Test::IsAway,
        Test::IsAdmin,
        Test::IsModerator,
        Test::IsCaptain,
        Test::HasPlayer,
    ]
    .into_iter()
    .find(|x| *x as isize == t)
}

/// LobbyCtrlHost.cpp's `ClientIs`.
fn client_is(client: isize, test: isize) -> bool {
    let call = |c: MC, a: isize| mcall(client, c, a) != 0;
    let Some(t) = test_of(test) else {
        return false;
    };
    match t {
        Test::IsIgnored => mget(client, MF::ClientIgnored) != 0,
        Test::IsBlocked => mget(client, MF::ClientBlocked) != 0,
        Test::IsMuted => call(MC::ClientMuted, 1),
        Test::IsAway => call(MC::ClientAway, 0),
        Test::IsAdmin => call(MC::ClientAdmin, 0),
        Test::IsModerator => call(MC::ClientModerator, 0),
        Test::IsCaptain => call(MC::ClientIsCaptain, 1),
        Test::HasPlayer => call(MC::ClientHasPlayer, 0),
    }
}

/// The player list's icon images (LobbyCtrlHost.cpp's `CtlImage`).
pub(crate) fn icon_res(icon: isize) -> R {
    match icon {
        i if i == Icon::Ignored as isize => R::UserListCtrl_Ignored_Image,
        i if i == Icon::Blocked as isize => R::UserListCtrl_Blocked_Image,
        i if i == Icon::Muted as isize => R::UserListCtrl_Muted_Image,
        i if i == Icon::Away as isize => R::UserListCtrl_Away_Image,
        i if i == Icon::Admin as isize => R::UserListCtrl_Admin_Image,
        i if i == Icon::Moderator as isize => R::UserListCtrl_Moderator_Image,
        i if i == Icon::Captain as isize => R::UserListCtrl_Captain_Image,
        i if i == Icon::Firewall as isize => R::LobbyGlobal_Firewall_Image,
        i if i == Icon::Searching as isize => R::LobbyGlobal_Searching_Image,
        // Icon::Normal and any other value
        _ => R::UserListCtrl_Normal_Image,
    }
}

fn game_user_call(s: &GameUser, c: EeWonRowCall, item: isize, list: isize, a: isize, _: isize, _: isize) -> isize {
    match c {
        // Sizes the entry to the list font's height, or the icon's if taller
        EeWonRowCall::CalcDimensions => {
            assign(&s.font, h(Op::ListFont, list, 0, 0));
            set(item, F::Height, h(Op::FontHeight, s.font.get(), 0, 0) as i32 as isize);
            let image = s.image.get();
            if image != 0 {
                let ih = h(Op::ImageHeight, image, 0, 0) as i32;
                if ih > get(item, F::Height) as i32 {
                    set(item, F::Height, ih as isize);
                }
            }
            0
        }
        // Orders the game captain first, then players by name ignoring case
        EeWonRowCall::Compare => {
            let other = other_of::<GameUser>(a, &GAME_USER).map_or(0, |o| o.client.get());
            let mine = (mcall(s.client.get(), MC::ClientIsCaptain, 1) != 0) as i32;
            let theirs = (mcall(other, MC::ClientIsCaptain, 1) != 0) as i32;
            if mine != theirs {
                return (theirs - mine) as isize;
            }
            let n1 = mget(s.client.get(), MF::ClientName);
            let n2 = mget(other, MF::ClientName);
            h(Op::TextCompareNoCase, n1, n2, 0) as i32 as isize
        }
        // Draws one column of the entry: selection background, status icon, ready mark, name or ping bars (the ready
        // mark is centred by the status icon's height, not its own, as in the original)
        EeWonRowCall::Paint => {
            let g = a;
            let column = h(Op::ListPaintColumn, list, 0, 0) as i32;
            let mut color = h(Op::GfxStandardColor, g, 14, 0); // StandardColor_Text
            let selected = get(item, F::Selected) != 0;
            if selected {
                color = h(Op::GfxSelFore, g, list, color as u32 as isize);
            }
            h(Op::GfxSetColor, g, color as u32 as isize, 0);
            h(Op::GfxSetFont, g, s.font.get(), 0);
            let height = get(item, F::Height);
            match column {
                -1 => {
                    if selected {
                        h(Op::GfxSetColor, g, h(Op::GfxSelBack, g, list, 0), 0);
                        let w = h(Op::ListPaintWidth, list, 0, 0) as i32;
                        h(Op::GfxFillRect, g, pt(0, 0), pt(w, height as i32));
                    }
                }
                0 => {
                    let image = s.image.get();
                    let ih = h(Op::ImageHeight, image, 0, 0);
                    h(Op::GfxDrawImage, g, image, pt(0, half(height, ih)));
                }
                1 => {
                    if mcall(s.client.get(), MC::ClientIsReady, 0) != 0 {
                        let ih = h(Op::ImageHeight, s.image.get(), 0, 0);
                        h(Op::GfxDrawResImage, g, R::GameUserListCtrl_Ready_Image as isize, pt(0, half(height, ih)));
                    }
                }
                2 => {
                    h(Op::GfxSetFont, g, s.font.get(), 0);
                    let name = mget(s.client.get(), MF::ClientName);
                    let fh = h(Op::FontHeight, s.font.get(), 0, 0);
                    h(Op::GfxDrawText, g, name, pt(0, half(height, fh)));
                }
                3 => {
                    let ping = mcall(s.client.get(), MC::ClientPing, 0) as u16;
                    h(Op::GfxDrawPing, g, ping as isize, (height as i32 - 2) as isize);
                }
                _ => {}
            }
            0
        }
        EeWonRowCall::CanSelect => (get(item, F::Selectable) != 0) as isize,
        EeWonRowCall::MouseDown => 0,
    }
}

// ---- ServerDetailsItem (the game list's rows: their virtuals are their control's, srvlistctl.rs, as the
// original's forwards were)

/// ServerDetailsItem's members.
struct Server {
    game: Cell<isize>,
    /// `mIPAddrStr` (a held `GUIStringData*`; 0 until it is set)
    addr_text: Cell<isize>,
    icon_id: Cell<i32>,
    summary_height: Cell<u8>,
}

impl Server {
    fn end(&self) {
        // the members in reverse order: the address text, the game
        drop_ref(&self.addr_text);
        drop_ref(&self.game);
    }
}

/// Whether the game list's rows are the EE game's (`EELobbyServerDetailsItem`, whose details eeglue.rs measures and
/// paints): set with the glue's factories.
static EE_DETAILS: AtomicBool = AtomicBool::new(false);

/// The game list's rows are the EE game's from now on (the glue's `SetFactories`).
#[no_mangle]
pub extern "C" fn ee_won_lobby_rows_ee_details() {
    EE_DETAILS.store(true, Ordering::Relaxed);
}

/// A row's virtual, forwarded to the list's control (the list's grandparent), as the original's forwards did.
fn server_call(_: &Server, c: EeWonRowCall, item: isize, list: isize, a: isize, b: isize, cc: isize) -> isize {
    use crate::srvlistctl::EeWonLobbyCtlRowCall as RC;
    let (call, args) = match c {
        EeWonRowCall::Paint => (RC::Paint, [a, list, 0]),
        EeWonRowCall::CalcDimensions => (RC::CalcDimensions, [list, 0, 0]),
        EeWonRowCall::Compare => (RC::Compare, [b, list, cc]),
        EeWonRowCall::CanSelect => (RC::CanSelect, [list, pt(a as i32, b as i32), 0]),
        EeWonRowCall::MouseDown => (RC::MouseDown, [list, pt(a as i32, b as i32), cc]),
    };
    let ctl = h(Op::ListOwnerCtl, list, 0, 0) as *mut crate::lobbyctl::EeWonLobbyCtl;
    // SAFETY: the list's grandparent is its live game list control; the arguments outlive the call
    let r = unsafe {
        crate::lobbyctl::ee_won_lobbyctl_call(
            ctl,
            crate::lobbyctl::EeWonLobbyCtlCall::Row,
            call as isize,
            item,
            args.as_ptr() as isize,
        )
    };
    match c {
        EeWonRowCall::Compare => r as i32 as isize,
        EeWonRowCall::CanSelect => (r != 0) as isize,
        _ => 0,
    }
}

// ---- the controls' row ops

/// The controls' row ops answered here in the game (lobbyctl.rs, after the model's answers); `None` for the rest.
pub(crate) fn ctl_op(op: LOp, a: isize, b: isize, c: isize) -> Option<isize> {
    Some(match op {
        LOp::RoomItemServer => room_server(a, b != 0),
        LOp::RoomItemGroup => room_group(a, b),
        LOp::RoomItemUpdate => {
            let s = state_of::<Room>(a, &ROOM)?;
            room_update(a, s, b, c);
            0
        }
        LOp::RoomItemAddr => state_of::<Room>(a, &ROOM)?.address.get(),
        LOp::RoomItemGroupId => state_of::<Room>(a, &ROOM)?.group_id.get() as isize,
        LOp::FriendRowNew => {
            let s = Friend { friend: Cell::new(0), separator: Cell::new(true), sort_group: Cell::new(0) };
            make(&FRIEND, s, true).0
        }
        LOp::UserRowNew => {
            let s = User { client: Cell::new(0), sort_key: Cell::new(0), show_separator: Cell::new(true) };
            make(&USER, s, true).0
        }
        LOp::ItemFriend => state_of::<Friend>(a, &FRIEND)?.friend.get(),
        LOp::ItemSetFriend => {
            assign(&state_of::<Friend>(a, &FRIEND)?.friend, b);
            0
        }
        LOp::ItemUserClient => state_of::<User>(a, &USER)?.client.get(),
        LOp::ItemSetUserClient => {
            assign(&state_of::<User>(a, &USER)?.client, b);
            0
        }
        LOp::ItemGet | LOp::ItemSet => {
            let set = op == LOp::ItemSet;
            match row_field(b)? {
                F::FriendSeparator => {
                    let s = state_of::<Friend>(a, &FRIEND)?;
                    if set {
                        s.separator.set(c != 0);
                    }
                    s.separator.get() as isize
                }
                F::FriendSortGroup => {
                    let s = state_of::<Friend>(a, &FRIEND)?;
                    if set {
                        s.sort_group.set(c as i8);
                    }
                    s.sort_group.get() as isize
                }
                F::UserSortKey => {
                    let s = state_of::<User>(a, &USER)?;
                    if set {
                        s.sort_key.set(c as i8);
                    }
                    s.sort_key.get() as isize
                }
                F::UserShowSeparator => {
                    let s = state_of::<User>(a, &USER)?;
                    if set {
                        s.show_separator.set(c != 0);
                    }
                    s.show_separator.get() as isize
                }
                F::IconId => {
                    let s = state_of::<Server>(a, &SERVER)?;
                    if set {
                        s.icon_id.set(c as i32);
                    }
                    s.icon_id.get() as isize
                }
                F::SummaryHeight => {
                    let s = state_of::<Server>(a, &SERVER)?;
                    if set {
                        s.summary_height.set(c as u8);
                    }
                    s.summary_height.get() as isize
                }
                _ => return None,
            }
        }
        LOp::RowNew => {
            let s = Server {
                game: Cell::new(0),
                addr_text: Cell::new(0),
                icon_id: Cell::new(0),
                summary_height: Cell::new(0),
            };
            let (item, _) = make(&SERVER, s, false);
            h(Op::AddRef, item, 0, 0);
            item
        }
        LOp::RowGame => state_of::<Server>(a, &SERVER)?.game.get(),
        LOp::RowSetGame => {
            assign(&state_of::<Server>(a, &SERVER)?.game, b);
            0
        }
        LOp::RowSetAddrText => {
            let s = state_of::<Server>(a, &SERVER)?;
            // `mIPAddrStr = mGame->GetAddress().GetHostAndPortString(true)`: the new text held, the old given back
            let text = h(Op::GameAddrText, s.game.get(), 0, 0);
            let old = s.addr_text.replace(text);
            if old != 0 {
                h(Op::Release, old, 0, 0);
            }
            0
        }
        LOp::RowAddrText => state_of::<Server>(a, &SERVER)?.addr_text.get(),
        // the EE game's rows' hooks (EELobbyServerDetailsItem: eeglue.rs), else ServerDetailsItem's empty ones
        LOp::RowDetailsHeight => {
            state_of::<Server>(a, &SERVER)?;
            if !EE_DETAILS.load(Ordering::Relaxed) {
                return Some(0);
            }
            // SAFETY: the list is the row's live list
            let height =
                unsafe { crate::eeglue::ee_won_eeglue_call(crate::eeglue::EeWonEeGlueCall::DetailsHeight, b, 0, 0) };
            height as i32 as isize
        }
        LOp::RowPaintDetails => {
            state_of::<Server>(a, &SERVER)?;
            if EE_DETAILS.load(Ordering::Relaxed) {
                // SAFETY: the row and the graphics context are live
                unsafe { crate::eeglue::ee_won_eeglue_call(crate::eeglue::EeWonEeGlueCall::PaintDetails, a, b, 0) };
            }
            0
        }
        LOp::ItemNew => game_user_new(a),
        LOp::ItemClient => state_of::<GameUser>(a, &GAME_USER)?.client.get(),
        LOp::ItemClientIs => client_is(state_of::<GameUser>(a, &GAME_USER)?.client.get(), b) as isize,
        LOp::ItemSetImage => {
            let s = state_of::<GameUser>(a, &GAME_USER)?;
            let res = icon_res(b);
            // `mImage = LOBBY_IMAGE(...)`: the resource's image (a ResImage read through the host)
            assign(&s.image, h(Op::ResImage, res as isize, 0, 0));
            0
        }
        _ => return None,
    })
}

/// `icon_res` for the drivers: the picture's resource row.
#[no_mangle]
pub extern "C" fn ee_won_lobby_rows_icon_row(icon: isize) -> i32 {
    icon_res(icon) as i32
}

/// The controls' row ops from C++ and the drivers (`ctl_op`; `INTPTR_MIN` when the op is not a row op or its row
/// is not of the op's kind).
#[no_mangle]
pub extern "C" fn ee_won_lobby_rows_ctl_op(
    op: crate::lobbyctl::EeWonLobbyCtlOp,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    ctl_op(op, a, b, c).unwrap_or(isize::MIN)
}

#[cfg(test)]
#[path = "rows_tests.rs"]
mod tests;
