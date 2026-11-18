//! The Internet lobby's other controls as lobbyctl.rs kinds (R6 WONLobby round 13): the room tree (RoomCtrl), the
//! friends list (FriendsListCtrl), the room's user list (UserListCtrl), the quick play panel (QuickPlayCtrl), the
//! login dialog (LoginCtrl), the account pages (CreateAccountCtrl, UpdateAccountCtrl, LostPasswordCtrl,
//! LostUserNameCtrl), the login help and message of the day pages (LoginHelpCtrl, MOTDCtrl), the login status line
//! and the start-up failure panel (LoginStatusCtrl, InitFailedCtrl), the login screen's page switch (LoginScreen),
//! the direct-connect tab (DirectConnectCtrl) and the room label (RoomLabelCtrl, the LAN game room's too). Each was a
//! `Container` (two a `Dialog`) whose members were its controls and a few flags or texts; all of that and every
//! decision is here. The list rows (RoomListItem, FriendsListItem, UserListItem) stay C++ list rows (LobbyNetCtrlHost.cpp:
//! their paint, size and order), filled and read by ops.

use std::cell::{Cell, RefCell};

use crate::lobbyctl::{
    EeWonLobbyCtl, EeWonLobbyCtlEvField as EF, EeWonLobbyCtlEvent as Ev, EeWonLobbyCtlItemField as F,
    EeWonLobbyCtlKind as Kind, EeWonLobbyCtlOp as Op,
};
use crate::modelobj::{EeWonModelCall as MC, EeWonModelField as MF};
use crate::resource::EeWonLobbyRes as R;

/// The round 13 small kinds' members beyond their controls.
#[derive(Default)]
pub(crate) struct NetState {
    /// LoginCtrl's `mCanLogin`, LoginHelpCtrl's `mHaveLoadedHelp`, MOTDCtrl's `mNeedGenHTML`, UserListCtrl's
    /// `mUpdating`, RoomLabelCtrl's `mShowServer`
    pub flag: Cell<bool>,
    /// UserListCtrl's `mModeFlags` (5), RoomLabelCtrl's `mUserCt`
    pub number: Cell<i32>,
    /// UpdateAccountCtrl's `mActualOldPassword`; RoomLabelCtrl's `mPrefix`, `mServer`, `mRoom` (one reference
    /// each, 0 the empty string)
    pub texts: [Cell<isize>; 3],
    /// MOTDCtrl's `mSysMOTDPath`, `mGameMOTDPath`
    pub paths: RefCell<[Vec<u8>; 2]>,
}

/// MOTDCtrl's two pages (`BrowserShowFiles`).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonLobbyCtlFiles {
    pub sys: *const u8,
    pub sys_len: usize,
    pub game: *const u8,
    pub game_len: usize,
}

/// `LobbyIsValidEmail`: an `@` after the first character, a `.` after it with text after the dot.
pub fn is_valid_email(email: &[u8]) -> bool {
    if email.is_empty() {
        return false;
    }
    let Some(at) = email.iter().skip(1).position(|&c| c == b'@').map(|p| p + 1) else {
        return false;
    };
    let Some(dot) = email[at..].iter().position(|&c| c == b'.').map(|p| p + at) else {
        return false;
    };
    email.len() > dot + 1
}

/// `IsValidBirthDate`: month 1-12, day 1-31, year 1890-1999.
pub fn is_valid_birth_date(month: i32, day: i32, year: i32) -> bool {
    !(month <= 0 || day <= 0 || year <= 1889 || month > 12 || day > 31 || year > 1999)
}

fn res(r: R) -> isize {
    r as isize
}

impl EeWonLobbyCtl {
    fn net(&self) -> &NetState {
        &self.ext.net
    }

    fn evf(&self, event: isize, f: EF) -> isize {
        self.op(Op::EvGet, event, f as isize, 0)
    }

    fn mg(&self, obj: isize, f: MF) -> isize {
        self.op(Op::MGet, obj, f as isize, 0)
    }

    fn mc(&self, obj: isize, c: MC, a: isize) -> isize {
        self.op(Op::MCall, obj, c as isize, a)
    }

    fn ig(&self, item: isize, f: F) -> isize {
        self.op(Op::ItemGet, item, f as isize, 0)
    }

    fn is(&self, item: isize, f: F, v: isize) {
        self.op(Op::ItemSet, item, f as isize, v);
    }

    fn release(&self, p: isize) {
        if p != 0 {
            self.op(Op::Release, p, 0, 0);
        }
    }

    /// `(flags & e->x) != e->x`, the event's member read twice.
    fn spec_mismatch(&self, flags: i32, event: isize, f: EF) -> bool {
        let a = self.evf(event, f) as i32;
        flags & a != self.evf(event, f) as i32
    }

    /// The same over `ClientChangedEvent::mRoomSpec` (`EventRoomSpec`).
    fn cc_mismatch(&self, flags: i32, event: isize) -> bool {
        let a = self.op(Op::EventRoomSpec, event, 0, 0) as i32;
        flags & a != self.op(Op::EventRoomSpec, event, 0, 0) as i32
    }

    fn find_range(&self, config: isize, first: usize, count: usize, local: &Cell<isize>) {
        for slot in first..first + count {
            self.find(config, slot, local);
        }
    }

    /// The look-ups and the start of each kind's InitComponent.
    pub(crate) fn net_init(&self, config: isize, local: &Cell<isize>) {
        use crate::lobbyctl::slot_of;
        let first = slot_of(self.kind);
        let me = self.ctl as isize;
        match self.kind {
            Kind::Room => {
                self.find_range(config, first, 3, local);
                self.op(Op::SubscribeBroadcast, 1, 0, 0);
                self.room_enable_controls();
            }
            Kind::Friends => {
                self.find_range(config, first, 3, local);
                let separator = self.op(Op::FriendRowNew, 0, 0, 0);
                self.assign(&self.members[N_SEPARATOR], separator);
                self.is(self.m(N_SEPARATOR), F::FriendSortGroup, 1);
                self.is(self.m(N_SEPARATOR), F::Selectable, 0);
                self.op(Op::SubscribeBroadcast, 1, 0, 0);
                self.friends_update_buttons();
            }
            Kind::Users => {
                self.find_range(config, first, 3, local);
                self.op(Op::SubscribeBroadcast, 1, 0, 0);
                let first_sep = self.op(Op::UserRowNew, 0, 0, 0);
                self.assign(&self.members[U_FIRST], first_sep);
                self.is(self.m(U_FIRST), F::Selectable, 0);
                self.is(self.m(U_FIRST), F::UserSortKey, 3);
                let second = self.op(Op::UserRowNew, 0, 0, 0);
                self.assign(&self.members[U_SECOND], second);
                self.is(self.m(U_SECOND), F::Selectable, 0);
                self.is(self.m(U_SECOND), F::UserSortKey, 1);
                self.users_update_buttons();
            }
            Kind::QuickPlay | Kind::LoginStatus | Kind::DirectConnect => {
                let n = match self.kind {
                    Kind::QuickPlay => 6,
                    Kind::LoginStatus => 1,
                    _ => 3,
                };
                self.find_range(config, first, n, local);
                self.op(Op::SubscribeBroadcast, 1, 0, 0);
            }
            Kind::Login => {
                self.find_range(config, first, 4, local);
                self.login_update_button();
                self.op(Op::SubscribeBroadcast, 1, 0, 0);
            }
            Kind::CreateAccount => {
                self.find_range(config, first, 14, local);
                self.create_update_button();
            }
            Kind::UpdateAccount => {
                self.find_range(config, first, 13, local);
                self.set_enabled(self.m(UA_USER), false);
                self.update_check_input();
                self.op(Op::SubscribeBroadcast, 1, 0, 0);
            }
            Kind::LostPassword => {
                self.find_range(config, first, 2, local);
                self.lost_password_update();
                self.op(Op::SubscribeBroadcast, 1, 0, 0);
            }
            Kind::LostUserName => {
                self.find_range(config, first, 2, local);
                self.lost_user_name_update();
            }
            Kind::LoginHelp => self.find_range(config, first, 1, local),
            Kind::InitFailed => {
                self.find_range(config, first, 3, local);
                self.op(Op::SetVisible, me, 0, 0);
                self.op(Op::SubscribeBroadcast, 1, 0, 0);
            }
            Kind::Motd => {
                self.find_range(config, first, 1, local);
                self.op(Op::SubscribeBroadcast, 1, 0, 0);
            }
            Kind::LoginScreen => {
                self.find_range(config, first, 6, local);
                self.op(Op::ScreenShow, self.m(LS_PANEL), self.m(LS_MOTD), 0);
            }
            Kind::RoomLabel => {
                self.find_range(config, first, 1, local);
                self.room_label_calc();
                self.op(Op::SubscribeBroadcast, 1, 0, 0);
            }
            _ => {}
        }
    }

    // ---- RoomCtrl

    fn room_enable_controls(&self) {
        let sel = self.op(Op::ListGetSel, self.m(RM_LIST), 0, 0);
        self.set_enabled(self.m(RM_JOIN), sel != 0);
    }

    fn room_add_group(&self, parent: isize, server: isize, group: isize) {
        let item = self.op(Op::RoomItemGroup, group, server, 0);
        self.op(Op::TreeInsertChild, self.m(RM_LIST), parent, item);
    }

    fn room_add_server(&self, server: isize, open: bool, pos: i32) {
        let tree = self.m(RM_LIST);
        self.op(Op::ListBeginUpdate, tree, 0, 0);
        let item = self.op(Op::RoomItemServer, server, 0, 0);
        let server_item = self.op(Op::TreeInsertServer, tree, item, pos as isize);
        let mut i = 0;
        while i < self.mc(server, MC::ServerGroupCount, 0) {
            let group = self.mc(server, MC::ServerGroupAt, i);
            if self.mc(group, MC::GroupKind, 0) & 8 != 0 {
                self.room_add_group(server_item, server, group);
            }
            i += 1;
        }
        if self.mc(server, MC::ServerConnected, 0) == 0 && self.mc(server, MC::ServerGroupCount, 0) == 0 {
            self.op(Op::TreeRemoveAll, self.m(RM_LIST), server_item, 0);
            let loading = self.op(Op::RoomItemServer, server, 1, 0);
            let loading_tree = self.op(Op::TreeInsertLoading, self.m(RM_LIST), server_item, loading);
            self.is(loading_tree, F::Selectable, 0);
        }
        if open {
            self.op(Op::TreeOpen, self.m(RM_LIST), server_item, 1);
        }
        self.op(Op::ListEndUpdate, self.m(RM_LIST), 0, 0);
        self.room_enable_controls();
    }

    fn room_server_item_at(&self, address: isize) -> isize {
        let tree = self.m(RM_LIST);
        let mut i = 0;
        while i < self.op(Op::ListNumItems, tree, 0, 0) {
            let t = self.op(Op::ListItem, self.m(RM_LIST), i, 0);
            let item = self.op(Op::TreeWrapped, t, 0, 0);
            if self.ask_with(Op::AddrEquals, self.op(Op::RoomItemAddr, item, 0, 0), address) {
                return t;
            }
            i += 1;
        }
        0
    }

    fn room_group_item_at(&self, address: isize, id: i32) -> isize {
        let server = self.room_server_item_at(address);
        if server == 0 {
            return 0;
        }
        let mut i = 0;
        while i < self.op(Op::TreeChildCount, server, 0, 0) {
            let child = self.op(Op::TreeChild, server, i, 0);
            let wrapped = self.op(Op::TreeWrapped, child, 0, 0);
            if self.op(Op::RoomItemGroupId, wrapped, 0, 0) as u32 == id as u32 {
                return child;
            }
            i += 1;
        }
        0
    }

    fn room_server_item(&self, server: isize) -> isize {
        if server != 0 {
            return self.room_server_item_at(self.mg(server, MF::ServerAddress));
        }
        0
    }

    fn room_group_item(&self, server: isize, group: isize) -> isize {
        if server != 0 && group != 0 {
            // the arguments right to left: the room's id, then the server's address
            let id = self.mg(group, MF::GroupId) as u16 as i32;
            let address = self.mg(server, MF::ServerAddress);
            return self.room_group_item_at(address, id);
        }
        0
    }

    fn room_join(&self) {
        let sel = self.op(Op::ListGetSel, self.m(RM_LIST), 0, 0);
        if sel != 0 {
            let item = self.op(Op::TreeWrapped, sel, 0, 0);
            self.op(Op::DeliverJoinRoom, item, 0, 0);
        }
    }

    fn room_create(&self) {
        let mut address = 0;
        let sel = self.op(Op::ListGetSel, self.m(RM_LIST), 0, 0);
        if sel != 0 {
            let item = self.op(Op::TreeWrapped, sel, 0, 0);
            address = self.op(Op::AddrClone, self.op(Op::RoomItemAddr, item, 0, 0), 0, 0);
        }
        self.op(Op::DeliverCreateRoom, address, 0, 0);
        if address != 0 {
            self.op(Op::AddrFree, address, 0, 0);
        }
    }

    fn room_set_list(&self) {
        let servers = self.op(Op::ServerListGet, 0, 0, 0);
        if servers == 0 {
            return;
        }
        let tree = self.m(RM_LIST);
        self.op(Op::ListBeginUpdate, tree, 0, 0);
        self.op(Op::TreeClear, tree, 1, 0);
        let mut i = 0;
        while i < self.mc(servers, MC::ServerListCount, 0) {
            let server = self.mc(servers, MC::ServerListAt, i);
            self.room_add_server(server, false, -1);
            i += 1;
        }
        self.op(Op::ListEndUpdate, self.m(RM_LIST), 0, 0);
        let group = self.op(Op::ChatGroup, 0, 0, 0);
        let server = self.op(Op::CurrentServer, 0, 0, 0);
        if server != 0 {
            let mut item = self.room_server_item(server);
            if item != 0 {
                self.op(Op::TreeOpen, self.m(RM_LIST), item, 0);
                let found = group == 0 || {
                    item = self.room_group_item(server, group);
                    item != 0
                };
                if found {
                    let list = self.m(RM_LIST);
                    let pos = self.op(Op::ListItemPos, list, item, 0);
                    self.op(Op::ListSetSel, list, pos, 1);
                    self.op(Op::ListEnsureVisible, self.m(RM_LIST), item, 0);
                }
            }
        }
        self.room_enable_controls();
    }

    fn room_group_changed(&self, event: isize) {
        let group = self.evf(event, EF::GcGroup);
        let server = self.evf(event, EF::GcServer);
        if group == 0 || server == 0 || self.mc(group, MC::GroupKind, 0) & 8 == 0 {
            return;
        }
        if self.ask(Op::EventIsAdd, event) {
            let server_item = self.room_server_item(server);
            if server_item != 0 {
                self.room_add_group(server_item, server, group);
            }
            self.room_enable_controls();
            return;
        }
        if self.ask(Op::EventIsDelete, event) {
            let group_item = self.room_group_item(server, group);
            self.op(Op::TreeRemove, self.m(RM_LIST), group_item, 0);
            self.room_enable_controls();
            return;
        }
        let group_item = self.room_group_item(server, group);
        if group_item != 0 {
            self.op(Op::RoomItemUpdate, self.op(Op::TreeWrapped, group_item, 0, 0), server, group);
            let parent = self.op(Op::TreeParent, group_item, 0, 0);
            self.op(Op::RoomItemUpdate, self.op(Op::TreeWrapped, parent, 0, 0), server, 0);
            self.op(Op::ListRefresh, self.m(RM_LIST), self.op(Op::TreeParent, group_item, 0, 0), 0);
            self.op(Op::ListRefresh, self.m(RM_LIST), group_item, 0);
        }
        self.room_enable_controls();
    }

    fn room_item_opened(&self, event: isize) {
        let t = self.op(Op::EventItem, event, 0, 0);
        if self.ask(Op::TreeIsOpen, t) {
            let item = self.op(Op::TreeWrapped, t, 0, 0);
            self.op(Op::DeliverQueryGroups, self.op(Op::RoomItemAddr, item, 0, 0), 0, 0);
        }
    }

    fn room_server_changed(&self, event: isize) {
        if self.ask(Op::EventIsAdd, event) {
            self.room_add_server(self.evf(event, EF::ScServer), false, -1);
            return;
        }
        if self.ask(Op::EventIsDelete, event) {
            let server_item = self.room_server_item(self.evf(event, EF::ScServer));
            if server_item != 0 {
                self.op(Op::TreeRemove, self.m(RM_LIST), server_item, 0);
            }
            self.room_enable_controls();
            return;
        }
        let server_item = self.room_server_item(self.evf(event, EF::ScServer));
        if server_item == 0 {
            return;
        }
        let open = self.ask(Op::TreeIsOpen, server_item);
        let selected = self.op(Op::ListGetSel, self.m(RM_LIST), 0, 0);
        let mut group_id: u32 = 0xffff;
        let mut address = 0;
        if selected != 0 && self.op(Op::TreeParent, selected, 0, 0) == server_item {
            let item = self.op(Op::TreeWrapped, selected, 0, 0);
            group_id = self.op(Op::RoomItemGroupId, item, 0, 0) as u32;
            address = self.op(Op::AddrClone, self.op(Op::RoomItemAddr, item, 0, 0), 0, 0);
        }
        let position = self.op(Op::TreeIndex, server_item, 0, 0) as i32;
        let offset = self.op(Op::ListVertOffset, self.m(RM_LIST), 0, 0);
        self.op(Op::ListBeginUpdate, self.m(RM_LIST), 0, 0);
        self.op(Op::TreeRemove, self.m(RM_LIST), server_item, 0);
        self.room_add_server(self.evf(event, EF::ScServer), open, position);
        if group_id != 0xffff && self.ask(Op::AddrValid, address) {
            let item = self.room_group_item_at(address, group_id as i32);
            if item != 0 {
                let list = self.m(RM_LIST);
                let pos = self.op(Op::ListItemPos, list, item, 0);
                self.op(Op::ListSetSel, list, pos, 1);
            }
        }
        self.op(Op::ListEndUpdate, self.m(RM_LIST), 0, 0);
        self.op(Op::ListSetVertOffset, self.m(RM_LIST), offset, 0);
        self.room_enable_controls();
        if address != 0 {
            self.op(Op::AddrFree, address, 0, 0);
        }
    }

    fn room_event(&self, event: isize, kind: Ev, component: isize) {
        match kind {
            Ev::TreeItemOpened => return self.room_item_opened(event),
            Ev::ListItemClicked => {
                self.room_enable_controls();
                if !self.ask(Op::EventDoubleClick, event)
                    || component != self.m(RM_LIST)
                    || !self.ask(Op::EventIsSelectedItem, event)
                {
                    return;
                }
                let item = self.op(Op::EventItem, event, 0, 0);
                // ContainsPoint's arguments right to left
                let y = self.op(Op::EventY, event, 0, 0) as i32;
                let x = self.op(Op::EventX, event, 0, 0) as i32;
                if self.ask_with(Op::TreeContainsPoint, item, crate::lobbyctl::pack(x, y)) {
                    self.room_join();
                }
                return;
            }
            Ev::ButtonPressed => {
                if component == self.m(RM_JOIN) {
                    return self.room_join();
                }
                if component == self.m(RM_CREATE) {
                    return self.room_create();
                }
            }
            Ev::GroupChanged => return self.room_group_changed(event),
            Ev::ServerChanged => return self.room_server_changed(event),
            Ev::SetRoomList => return self.room_set_list(),
            _ => {}
        }
        self.op(Op::BaseEvent, event, 0, 0);
    }

    // ---- FriendsListCtrl

    fn friends_update_separator(&self) {
        let sep = self.m(N_SEPARATOR);
        self.op(Op::ListRefresh, self.m(N_LIST), sep, 0);
        let pos = self.op(Op::ListItemPos, self.m(N_LIST), self.m(N_SEPARATOR), 0) as i32;
        let show = pos > 0 && pos < self.op(Op::ListNumItems, self.m(N_LIST), 0, 0) as i32 - 1;
        if (self.ig(self.m(N_SEPARATOR), F::FriendSeparator) != 0) != show {
            self.is(self.m(N_SEPARATOR), F::FriendSeparator, show as isize);
            self.op(Op::ListRefresh, self.m(N_LIST), self.m(N_SEPARATOR), 0);
        }
    }

    fn friends_update(&self, friend: isize, item: isize) {
        self.op(Op::ItemSetFriend, item, friend, 0);
        let flags = || self.mg(friend, MF::FriendFlags) as u8;
        let anonymous = || self.mg(friend, MF::FriendAnonymous) != 0;
        if flags() & 3 != 0 && !anonymous() {
            self.is(item, F::FriendSortGroup, 2);
        } else {
            self.is(item, F::FriendSortGroup, 0);
        }
        let image = if flags() & 4 != 0 {
            R::LobbyGlobal_Searching_Image
        } else if !anonymous() {
            if flags() & 2 != 0 {
                R::FriendsListCtrl_InGame_Image
            } else if flags() & 1 != 0 {
                R::FriendsListCtrl_InChat_Image
            } else {
                R::FriendsListCtrl_Unknown_Image
            }
        } else {
            R::FriendsListCtrl_Unknown_Image
        };
        self.op(Op::ItemSetImageRes, item, res(image), 0);
        self.op(Op::ItemSetText, item, self.mg(friend, MF::FriendName), 0);
        let me = self.ctl as isize;
        if flags() & 3 != 0 && !anonymous() {
            self.is(item, F::Color, 0x8000000e);
            self.op(Op::ItemSetFont, item, self.op(Op::DefaultFont, me, 0, 0), 0);
        } else {
            self.is(item, F::Color, 0x80000006);
            self.op(Op::ItemSetFont, item, self.op(Op::DefaultFontMod, me, 2, 0), 0);
        }
    }

    fn friends_find(&self, friend: isize) -> isize {
        let mut i = 0;
        while i < self.op(Op::ListNumItems, self.m(N_LIST), 0, 0) {
            let item = self.op(Op::ListItem, self.m(N_LIST), i, 0);
            let f = self.op(Op::ItemFriend, item, 0, 0);
            if f != 0
                && self.mg(self.op(Op::ItemFriend, item, 0, 0), MF::FriendUserId) as u32
                    == self.mg(friend, MF::FriendUserId) as u32
            {
                return item;
            }
            i += 1;
        }
        0
    }

    fn friends_selected(&self) -> isize {
        let item = self.op(Op::ListGetSel, self.m(N_LIST), 0, 0);
        if item != 0 {
            return self.op(Op::ItemFriend, item, 0, 0);
        }
        0
    }

    fn friends_changed(&self, event: isize) {
        if self.ask(Op::EventIsAdd, event) {
            let item = self.op(Op::FriendRowNew, 0, 0, 0);
            self.op(Op::AddRef, item, 0, 0);
            self.friends_update(self.evf(event, EF::FcFriend), item);
            self.op(Op::ListInsert2, self.m(N_LIST), item, 0);
            self.op(Op::Release, item, 0, 0);
        } else if self.ask(Op::EventIsDelete, event) {
            let item = self.friends_find(self.evf(event, EF::FcFriend));
            if item == 0 {
                return;
            }
            self.op(Op::ListRemove, self.m(N_LIST), item, 0);
        } else {
            let item = self.friends_find(self.evf(event, EF::FcFriend));
            if item == 0 {
                return;
            }
            self.friends_update(self.evf(event, EF::FcFriend), item);
            self.op(Op::ListRefresh, self.m(N_LIST), item, 0);
        }
        self.friends_update_buttons();
        self.friends_update_separator();
    }

    fn friends_refresh(&self) {
        self.op(Op::ListBeginUpdate, self.m(N_LIST), 0, 0);
        let mut i = 0;
        while i < self.op(Op::ListNumItems, self.m(N_LIST), 0, 0) {
            let item = self.op(Op::ListItem, self.m(N_LIST), i, 0);
            if item != 0 && self.op(Op::ItemFriend, item, 0, 0) != 0 {
                self.friends_update(self.op(Op::ItemFriend, item, 0, 0), item);
            }
            i += 1;
        }
        self.op(Op::ListEndUpdate, self.m(N_LIST), 0, 0);
        self.friends_update_buttons();
        self.friends_update_separator();
    }

    fn friends_fill(&self) {
        self.op(Op::ListBeginUpdate, self.m(N_LIST), 0, 0);
        self.op(Op::ListClear, self.m(N_LIST), 1, 0);
        self.op(Op::ListInsert2, self.m(N_LIST), self.m(N_SEPARATOR), 0);
        let mut i = 0;
        while i < self.op(Op::FriendCount, 0, 0, 0) {
            let item = self.op(Op::FriendRowNew, 0, 0, 0);
            self.op(Op::AddRef, item, 0, 0);
            self.friends_update(self.op(Op::FriendAt, i, 0, 0), item);
            self.op(Op::ListInsert2, self.m(N_LIST), item, 0);
            self.op(Op::Release, item, 0, 0);
            i += 1;
        }
        self.op(Op::ListEndUpdate, self.m(N_LIST), 0, 0);
        self.friends_update_buttons();
        self.friends_update_separator();
    }

    fn friends_update_buttons(&self) {
        let on = self.op(Op::ListGetSel, self.m(N_LIST), 0, 0) != 0;
        self.set_enabled(self.m(N_LOCATE), on);
        self.set_enabled(self.m(N_REMOVE), on);
    }

    fn friends_event(&self, event: isize, kind: Ev, component: isize) {
        match kind {
            Ev::ButtonPressed => {
                if component == self.m(N_REMOVE) {
                    let friend = self.friends_selected();
                    if friend != 0 {
                        self.op(Op::DeliverFriendAction, 3, friend, 0);
                    }
                    return;
                } else if component == self.m(N_LOCATE) {
                    let friend = self.friends_selected();
                    if friend != 0 {
                        self.op(Op::DeliverFriendAction, 1, friend, 0);
                    }
                    return;
                }
            }
            Ev::ListSelChanged => return self.friends_update_buttons(),
            Ev::ListItemClicked => {
                let item = self.op(Op::ListGetSel, self.m(N_LIST), 0, 0);
                if item != 0 && item == self.op(Op::EventItem, event, 0, 0) && self.op(Op::ItemFriend, item, 0, 0) != 0
                {
                    if self.op(Op::EventButton, event, 0, 0) as i32 == 2 {
                        self.op(Op::DeliverFriendAction, 5, self.op(Op::ItemFriend, item, 0, 0), 0);
                        return;
                    }
                    if self.op(Op::EventButton, event, 0, 0) as i32 == 0 && self.ask(Op::EventDoubleClick, event) {
                        self.op(Op::DeliverFriendAction, 1, self.op(Op::ItemFriend, item, 0, 0), 0);
                    }
                }
                return;
            }
            Ev::SyncOptions => return self.friends_fill(),
            Ev::FriendChanged => return self.friends_changed(event),
            Ev::RefreshFriends => return self.friends_refresh(),
            _ => {}
        }
        self.op(Op::BaseEvent, event, 0, 0);
    }

    // ---- UserListCtrl

    /// `GetLobbyClientIcon(client, mode)`: the first of ignored, blocked, muted, away, admin, moderator, captain.
    fn client_icon(&self, client: isize, mode: bool) -> R {
        if self.mg(client, MF::ClientIgnored) != 0 {
            return R::UserListCtrl_Ignored_Image;
        }
        if self.mg(client, MF::ClientBlocked) != 0 {
            return R::UserListCtrl_Blocked_Image;
        }
        if self.mc(client, MC::ClientMuted, mode as isize) != 0 {
            return R::UserListCtrl_Muted_Image;
        }
        if self.mc(client, MC::ClientAway, 0) != 0 {
            return R::UserListCtrl_Away_Image;
        }
        if self.mc(client, MC::ClientAdmin, 0) != 0 {
            return R::UserListCtrl_Admin_Image;
        }
        if self.mc(client, MC::ClientModerator, 0) != 0 {
            return R::UserListCtrl_Moderator_Image;
        }
        if self.mc(client, MC::ClientIsCaptain, mode as isize) != 0 {
            R::UserListCtrl_Captain_Image
        } else {
            R::UserListCtrl_Normal_Image
        }
    }

    fn users_update_item(&self, client: isize, item: isize) {
        self.op(Op::ItemSetUserClient, item, client, 0);
        let key = if self.mc(client, MC::ClientAdmin, 0) != 0 {
            5
        } else if self.mc(client, MC::ClientModerator, 0) != 0 {
            4
        } else if self.mg(client, MF::ClientIsFriend) != 0 && self.mc(client, MC::ClientAnonymous, 0) == 0 {
            2
        } else {
            0
        };
        self.is(item, F::UserSortKey, key);
        self.op(Op::ItemSetText, item, self.mg(client, MF::ClientName), 0);
        let icon = if client == 0 { -1 } else { res(self.client_icon(client, self.net().number.get() & 2 != 0)) };
        self.op(Op::ItemSetImageRes, item, icon, 0);
        let me = self.ctl as isize;
        if self.mg(client, MF::ClientIsFriend) != 0 && self.mc(client, MC::ClientAnonymous, 0) == 0 {
            self.op(Op::ItemSetFont, item, self.op(Op::DefaultFontMod, me, 1, 0), 0);
        } else {
            self.op(Op::ItemSetFont, item, self.op(Op::DefaultFont, me, 0, 0), 0);
        }
    }

    fn users_refresh_separators(&self) {
        let list = || self.m(U_LIST);
        self.op(Op::ListRefresh, list(), self.m(U_FIRST), 0);
        self.op(Op::ListRefresh, list(), self.m(U_SECOND), 0);
        let first = self.op(Op::ListItemPos, list(), self.m(U_FIRST), 0) as i32;
        let second = self.op(Op::ListItemPos, list(), self.m(U_SECOND), 0) as i32;
        let before = first > 0;
        let middle = second > first + 1;
        let after = second < self.op(Op::ListNumItems, list(), 0, 0) as i32 - 1;
        let show_first = before && (middle || after);
        let show_second = middle && after;
        if (self.ig(self.m(U_FIRST), F::UserShowSeparator) != 0) != show_first {
            self.is(self.m(U_FIRST), F::UserShowSeparator, show_first as isize);
            self.op(Op::ListRefresh, list(), self.m(U_FIRST), 0);
        }
        if (self.ig(self.m(U_SECOND), F::UserShowSeparator) != 0) != show_second {
            self.is(self.m(U_SECOND), F::UserShowSeparator, show_second as isize);
            self.op(Op::ListRefresh, list(), self.m(U_SECOND), 0);
        }
    }

    fn users_find(&self, client: isize) -> isize {
        let mut i = 0;
        while i < self.op(Op::ListNumItems, self.m(U_LIST), 0, 0) {
            let item = self.op(Op::ListItem, self.m(U_LIST), i, 0);
            if self.op(Op::ItemUserClient, item, 0, 0) != 0
                && self.mg(self.op(Op::ItemUserClient, item, 0, 0), MF::ClientId) as u16
                    == self.mg(client, MF::ClientId) as u16
            {
                return item;
            }
            i += 1;
        }
        0
    }

    fn users_selected_client(&self) -> isize {
        let item = self.op(Op::ListGetSel, self.m(U_LIST), 0, 0);
        if item == 0 {
            0
        } else {
            self.op(Op::ItemUserClient, item, 0, 0)
        }
    }

    fn users_update_buttons(&self) {
        let client = self.users_selected_client();
        if client == 0 {
            self.op(Op::ButtonSetRes, self.m(U_IGNORE), res(R::UserListCtrl_Ignore_String), 0);
            self.set_enabled(self.m(U_IGNORE), false);
            self.set_enabled(self.m(U_WHISPER), false);
        } else {
            self.set_enabled(self.m(U_WHISPER), true);
            self.set_enabled(self.m(U_IGNORE), true);
            if self.mg(client, MF::ClientIgnored) != 0 {
                self.op(Op::ButtonSetRes, self.m(U_IGNORE), res(R::UserListCtrl_Unignore_String), 0);
            } else {
                self.op(Op::ButtonSetRes, self.m(U_IGNORE), res(R::UserListCtrl_Ignore_String), 0);
            }
        }
    }

    fn users_joined(&self, event: isize) {
        let flags = self.net().number.get();
        if self.spec_mismatch(flags, event, EF::JrRoomSpec) {
            return;
        }
        self.net().flag.set(true);
        self.op(Op::ListBeginUpdate, self.m(U_LIST), 0, 0);
        self.op(Op::ListClear, self.m(U_LIST), 1, 0);
        self.op(Op::ListInsert2, self.m(U_LIST), self.m(U_FIRST), 0);
        self.op(Op::ListInsert2, self.m(U_LIST), self.m(U_SECOND), 0);
        let clients = self.evf(event, EF::JrClients);
        let mut i = 0;
        while i < self.mc(clients, MC::ListCount, 0) {
            let client = self.mc(clients, MC::ListAt, i);
            let item = self.op(Op::UserRowNew, 0, 0, 0);
            self.op(Op::AddRef, item, 0, 0);
            self.users_update_item(client, item);
            self.op(Op::ListInsert2, self.m(U_LIST), item, 0);
            self.op(Op::Release, item, 0, 0);
            i += 1;
        }
        self.op(Op::ListEndUpdate, self.m(U_LIST), 0, 0);
        self.users_update_buttons();
        self.users_refresh_separators();
    }

    fn users_left(&self, event: isize) {
        let flags = self.net().number.get();
        if self.spec_mismatch(flags, event, EF::LrRoomSpec) {
            return;
        }
        self.net().flag.set(false);
        self.op(Op::ListClear, self.m(U_LIST), 1, 0);
        self.users_update_buttons();
    }

    fn users_client_changed(&self, event: isize) {
        let flags = self.net().number.get();
        if self.cc_mismatch(flags, event) {
            return;
        }
        if self.ask(Op::EventIsAdd, event) {
            let item = self.op(Op::UserRowNew, 0, 0, 0);
            self.op(Op::AddRef, item, 0, 0);
            let client = self.op(Op::ClientChangedClient, event, 0, 0);
            self.users_update_item(client, item);
            self.op(Op::ListInsert2, self.m(U_LIST), item, 0);
            self.op(Op::Release, item, 0, 0);
        } else if self.ask(Op::EventIsDelete, event) {
            let item = self.users_find(self.op(Op::ClientChangedClient, event, 0, 0));
            if item == 0 {
                return;
            }
            self.op(Op::ListRemove, self.m(U_LIST), item, 0);
        } else {
            let item = self.users_find(self.op(Op::ClientChangedClient, event, 0, 0));
            if item == 0 {
                return;
            }
            self.users_update_item(self.op(Op::ClientChangedClient, event, 0, 0), item);
            self.op(Op::ListRefresh, self.m(U_LIST), item, 0);
        }
        self.users_update_buttons();
        self.users_refresh_separators();
    }

    fn users_event(&self, event: isize, kind: Ev, component: isize) {
        let flags = self.net().number.get() as isize;
        match kind {
            Ev::ClientChanged => self.users_client_changed(event),
            Ev::JoinedRoom => self.users_joined(event),
            Ev::LeftRoom => self.users_left(event),
            Ev::ButtonPressed => {
                if component == self.m(U_IGNORE) {
                    let client = self.users_selected_client();
                    if client != 0 {
                        let action = if self.mg(client, MF::ClientIgnored) != 0 { 12 } else { 5 };
                        self.op(Op::DeliverClientAction, action, flags, client);
                    }
                } else if component == self.m(U_WHISPER) {
                    let client = self.users_selected_client();
                    if client != 0 {
                        self.op(Op::BroadcastWhisper, client, flags, 0);
                    }
                }
            }
            Ev::ListSelChanged => self.users_update_buttons(),
            Ev::ListItemClicked => {
                let client = self.users_selected_client();
                if client == 0 {
                    return;
                }
                let item = self.op(Op::EventItem, event, 0, 0);
                if self.op(Op::ListGetSel, self.m(U_LIST), 0, 0) != item {
                    return;
                }
                if self.op(Op::EventButton, event, 0, 0) as i32 == 2 {
                    self.op(Op::DeliverClientAction, 18, flags, client);
                } else if self.op(Op::EventButton, event, 0, 0) as i32 == 0 && self.ask(Op::EventDoubleClick, event) {
                    self.op(Op::BroadcastWhisper, client, flags, 0);
                }
            }
            _ => {
                self.op(Op::BaseEvent, event, 0, 0);
            }
        }
    }

    // ---- QuickPlayCtrl

    fn quick_play_event(&self, event: isize, kind: Ev, component: isize) {
        match kind {
            Ev::SyncOptions => {
                let user = self.op(Op::CurrentUser, 0, 0, 0);
                if user != 0 {
                    self.op(Op::LabelSetUserWelcome, self.m(0), user, 0);
                } else {
                    self.op(Op::LabelSetRes, self.m(0), res(R::InternetQuickPlay_Welcome_String), 0);
                }
                return;
            }
            Ev::ButtonPressed if component == self.m(5) => {
                let mut game_type = 0;
                for (i, radio) in (1..=4).enumerate() {
                    if self.ask(Op::ButtonIsChecked, self.m(radio)) {
                        game_type = i + 1;
                        break;
                    }
                }
                self.op(Op::DeliverQuickPlay, game_type as isize, 0, 0);
            }
            _ => {}
        }
        self.op(Op::DialogBaseEvent, event, 0, 0);
    }

    // ---- LoginCtrl

    fn login_update_button(&self) {
        let on = self.net().flag.get()
            && self.op(Op::TextLen, self.op(Op::ComboEditText, self.m(LG_USER), 0, 0), 0, 0) != 0
            && self.op(Op::TextLen, self.op(Op::InputText, self.m(LG_PASSWORD), 0, 0), 0, 0) != 0;
        self.set_enabled(self.m(LG_LOGIN), on);
    }

    fn login_fill_user_names(&self) {
        self.op(Op::ComboClear, self.m(LG_USER), 1, 0);
        let mut newest: i32 = 0;
        let mut latest = 0;
        let mut i = 0;
        while i < self.op(Op::UserCount, 0, 0, 0) {
            let item = self.op(Op::ComboAddUserName, self.m(LG_USER), i, 0);
            let last_use = self.op(Op::UserLastUseAt, i, 0, 0) as i32;
            if last_use > newest {
                newest = last_use;
                latest = item;
            }
            i += 1;
        }
        if latest != 0 {
            let combo = self.m(LG_USER);
            let pos = self.op(Op::ComboItemPos, combo, latest, 0);
            self.op(Op::ComboSetSel, combo, pos, 0);
            self.login_load_password();
        }
    }

    fn login_load_password(&self) {
        if self.ask(Op::ConfigIsPublic, 0) {
            self.op(Op::InputClear, self.m(LG_PASSWORD), 1, 0);
            self.op(Op::SetVisible, self.m(LG_REMEMBER), 0, 0);
            self.op(Op::ButtonSetCheck, self.m(LG_REMEMBER), 0, 0);
            return;
        }
        let password = self.op(Op::UserPassword, self.op(Op::ComboEditText, self.m(LG_USER), 0, 0), 0, 0);
        self.op(Op::InputSetText, self.m(LG_PASSWORD), password, 1);
        self.op(Op::InputSelectRange, self.m(LG_PASSWORD), 0, -1);
        let has = self.op(Op::TextLen, password, 0, 0) != 0;
        self.op(Op::ButtonSetCheck, self.m(LG_REMEMBER), has as isize, 0);
        self.op(Op::SetVisible, self.m(LG_REMEMBER), 1, 0);
        self.op(Op::Release, password, 0, 0);
    }

    fn login_save_password(&self) {
        let user = self.op(Op::ComboEditText, self.m(LG_USER), 0, 0);
        self.op(Op::AddRef, user, 0, 0);
        let mut password = 0;
        if self.ask(Op::ButtonIsChecked, self.m(LG_REMEMBER)) {
            password = self.op(Op::InputText, self.m(LG_PASSWORD), 0, 0);
            self.op(Op::AddRef, password, 0, 0);
        }
        if self.op(Op::TextLen, user, 0, 0) != 0 {
            self.op(Op::AddUser, user, password, 0);
        }
        self.release(password);
        self.op(Op::Release, user, 0, 0);
    }

    fn login_event(&self, event: isize, kind: Ev, component: isize) {
        match kind {
            Ev::InputTextChanged => {
                if component == self.m(LG_USER)
                    && self.op(Op::TextAt, self.op(Op::ComboEditText, self.m(LG_USER), 0, 0), 0, 0) == b'@' as isize
                    && self.op(Op::TextLen, self.op(Op::InputText, self.m(LG_PASSWORD), 0, 0), 0, 0) != 0
                {
                    self.op(Op::InputClear, self.m(LG_PASSWORD), 0, 0);
                }
                return self.login_update_button();
            }
            Ev::InputReturn | Ev::ButtonPressed => {
                if kind == Ev::InputReturn && self.ask(Op::IsDisabled, self.m(LG_LOGIN)) {
                    if component == self.m(LG_USER) {
                        self.op(Op::RequestFocus, self.m(LG_PASSWORD), 0, 0);
                    } else if component == self.m(LG_PASSWORD) {
                        self.op(Op::RequestFocus, self.m(LG_USER), 0, 0);
                    }
                    return;
                }
                if component == self.m(LG_LOGIN) {
                    self.op(Op::DeliverLogin, self.m(LG_PASSWORD), self.m(LG_REMEMBER), self.m(LG_USER));
                    return;
                } else if component == self.m(LG_REMEMBER) {
                    return self.login_save_password();
                }
            }
            Ev::ComboSelChanged => {
                if component == self.m(LG_USER) {
                    self.login_load_password();
                }
                return;
            }
            Ev::LoginInitStatus => {
                let me = self.ctl as isize;
                if self.evf(event, EF::LisKind) == 1 {
                    self.net().flag.set(false);
                    self.login_update_button();
                    self.op(Op::SetVisible, me, 1, 0);
                } else if self.evf(event, EF::LisKind) == 2 {
                    self.net().flag.set(true);
                    self.login_update_button();
                } else if self.evf(event, EF::LisKind) == 3 {
                    self.op(Op::SetVisible, me, 0, 0);
                }
                return;
            }
            Ev::SetLoginFields => {
                self.op(Op::ComboSetTextFlag, self.m(LG_USER), self.evf(event, EF::SlfUserName), 0);
                self.op(Op::InputSetText, self.m(LG_PASSWORD), self.evf(event, EF::SlfPassword), 0);
                self.op(Op::ButtonSetCheck, self.m(LG_REMEMBER), 0, 0);
                return self.login_update_button();
            }
            Ev::FillUserNames => return self.login_fill_user_names(),
            _ => {}
        }
        self.op(Op::DialogBaseEvent, event, 0, 0);
    }

    // ---- CreateAccountCtrl

    fn show_error(&self, show: bool, label: isize) -> bool {
        self.op(Op::SetVisible, label, show as isize, 0);
        show
    }

    fn input_len(&self, input: isize) -> isize {
        self.op(Op::TextLen, self.op(Op::InputText, input, 0, 0), 0, 0)
    }

    fn input_int(&self, input: isize) -> i32 {
        self.op(Op::TextToInt, self.op(Op::InputText, input, 0, 0), 0, 0) as i32
    }

    fn create_update_button(&self) {
        let mut invalid = false;
        if self.show_error(self.input_len(self.m(CA_USER)) < 2, self.m(CA_USER_DESC)) {
            invalid = true;
        }
        if self.show_error(self.input_len(self.m(CA_PASSWORD)) < 4, self.m(CA_PASSWORD_DESC)) {
            invalid = true;
        }
        let password = self.op(Op::InputText, self.m(CA_PASSWORD), 0, 0);
        let confirm = self.op(Op::InputText, self.m(CA_CONFIRM), 0, 0);
        if self.show_error(!self.ask_with(Op::TextEquals, password, confirm), self.m(CA_CONFIRM_DESC)) {
            invalid = true;
        }
        let email = self.text_bytes(self.op(Op::InputText, self.m(CA_EMAIL), 0, 0));
        if self.show_error(!is_valid_email(&email), self.m(CA_EMAIL_DESC)) {
            invalid = true;
        }
        let month = self.input_int(self.m(CA_MONTH));
        let day = self.input_int(self.m(CA_DAY));
        let year = self.input_int(self.m(CA_YEAR));
        if self.show_error(!is_valid_birth_date(month, day, year), self.m(CA_BIRTH_DESC)) {
            invalid = true;
        }
        self.set_enabled(self.m(CA_CREATE), !invalid);
    }

    fn create_event(&self, event: isize, kind: Ev, component: isize) {
        if kind == Ev::InputTextChanged {
            return self.create_update_button();
        }
        if kind == Ev::ButtonPressed && component == self.m(CA_CREATE) {
            let fields: [isize; 7] = [
                self.m(CA_USER),
                self.m(CA_PASSWORD),
                self.m(CA_EMAIL),
                self.m(CA_DAY),
                self.m(CA_MONTH),
                self.m(CA_YEAR),
                self.m(CA_NEWSLETTER),
            ];
            self.op(Op::DeliverCreateAccount, fields.as_ptr() as isize, 0, 0);
        } else {
            self.op(Op::BaseEvent, event, 0, 0);
        }
    }

    // ---- UpdateAccountCtrl

    fn update_check_input(&self) {
        let mut error = false;
        let mut query_allowed = true;
        let old = self.op(Op::InputText, self.m(UA_OLD), 0, 0);
        if self.show_error(!self.ask_with(Op::TextEquals, old, self.net().texts[0].get()), self.m(UA_OLD_DESC)) {
            error = true;
            query_allowed = false;
        }
        let short = self.input_len(self.m(UA_PASSWORD)) != 0 && self.input_len(self.m(UA_PASSWORD)) < 4;
        if self.show_error(short, self.m(UA_PASSWORD_DESC)) {
            error = true;
        }
        let password = self.op(Op::InputText, self.m(UA_PASSWORD), 0, 0);
        let confirm = self.op(Op::InputText, self.m(UA_CONFIRM), 0, 0);
        if self.show_error(!self.ask_with(Op::TextEquals, password, confirm), self.m(UA_CONFIRM_DESC)) {
            error = true;
        }
        let bad_email = self.input_len(self.m(UA_EMAIL)) != 0
            && !is_valid_email(&self.text_bytes(self.op(Op::InputText, self.m(UA_EMAIL), 0, 0)));
        if self.show_error(bad_email, self.m(UA_EMAIL_DESC)) {
            error = true;
        }
        self.set_enabled(self.m(UA_UPDATE), !error);
        self.set_enabled(self.m(UA_QUERY), query_allowed);
    }

    fn update_entered_screen(&self) {
        let context = self.op(Op::AuthContext, 0, 0, 0);
        if context == 0 {
            return;
        }
        self.op(Op::InputSetAuthName, self.m(UA_USER), context, 0);
        for i in [UA_OLD, UA_PASSWORD, UA_CONFIRM, UA_EMAIL] {
            self.op(Op::InputClear, self.m(i), 1, 0);
        }
        self.op(Op::ButtonSetCheck, self.m(UA_NEWSLETTER), 0, 0);
        let password = self.op(Op::AuthPassword, context, 0, 0);
        let old = self.net().texts[0].replace(password);
        self.release(old);
    }

    fn update_event(&self, event: isize, kind: Ev, component: isize) {
        match kind {
            Ev::InputTextChanged => self.update_check_input(),
            Ev::ButtonPressed => {
                if component == self.m(UA_QUERY) {
                    self.op(Op::FireEvent, 100040, 0, 0);
                } else if component == self.m(UA_UPDATE) {
                    self.op(Op::DeliverUpdateAccount, self.m(UA_EMAIL), self.m(UA_PASSWORD), self.m(UA_NEWSLETTER));
                }
            }
            Ev::SyncOptions => self.update_entered_screen(),
            Ev::QueryAccountResult => {
                self.op(Op::InputSetText, self.m(UA_EMAIL), self.evf(event, EF::QarEmail), 1);
                self.op(Op::ButtonSetCheck, self.m(UA_NEWSLETTER), self.evf(event, EF::QarNewsletter), 0);
            }
            _ => {
                self.op(Op::BaseEvent, event, 0, 0);
            }
        }
    }

    // ---- LostPasswordCtrl, LostUserNameCtrl

    fn lost_password_update(&self) {
        let len = self.op(Op::TextLen, self.op(Op::ComboEditText, self.m(0), 0, 0), 0, 0);
        self.set_enabled(self.m(1), len >= 2);
    }

    fn lost_user_name_update(&self) {
        let email = self.text_bytes(self.op(Op::InputText, self.m(0), 0, 0));
        self.set_enabled(self.m(1), is_valid_email(&email));
    }

    // ---- MOTDCtrl, LoginHelpCtrl

    /// A page's paint: the help's first load, the MOTD's pages once both arrived; then the container's.
    pub(crate) fn net_paint(&self, graphics: isize) {
        if self.kind == Kind::LoginHelp && !self.net().flag.get() {
            self.op(Op::BrowserGoLoginHelp, self.m(0), 0, 0);
            self.net().flag.set(true);
        }
        if self.kind == Kind::Motd && self.net().flag.get() {
            self.net().flag.set(false);
            let ready = {
                let p = self.net().paths.borrow();
                !p[0].is_empty() && !p[1].is_empty()
            };
            if ready {
                let paths = self.net().paths.borrow().clone();
                let files = EeWonLobbyCtlFiles {
                    sys: paths[0].as_ptr(),
                    sys_len: paths[0].len(),
                    game: paths[1].as_ptr(),
                    game_len: paths[1].len(),
                };
                self.op(Op::BrowserShowFiles, self.m(0), &files as *const EeWonLobbyCtlFiles as isize, 0);
                let mut p = self.net().paths.borrow_mut();
                p[0].clear();
                p[1].clear();
            }
        }
        self.op(Op::BasePaint, graphics, 0, 0);
    }

    fn doc_path(&self, op: isize, owner: isize) -> Vec<u8> {
        let mut buffer = vec![0u8; 1024];
        let n = self.op(Op::HttpDocPath, op, owner, buffer.as_mut_ptr() as isize) as usize;
        if n > buffer.len() {
            buffer = vec![0u8; n];
            self.op(Op::HttpDocPath, op, owner, buffer.as_mut_ptr() as isize);
        }
        buffer.truncate(n);
        buffer
    }

    fn motd_show_message(&self, op: isize) {
        if !self.ask(Op::HttpDocIsMotd, op) {
            return;
        }
        self.net().flag.set(true);
        let sys = self.doc_path(op, 0);
        let game = self.doc_path(op, 1);
        *self.net().paths.borrow_mut() = [sys, game];
        self.op(Op::Invalidate, self.ctl as isize, 0, 0);
    }

    // ---- RoomLabelCtrl

    fn room_label_calc(&self) {
        let t = &self.net().texts;
        let mut count = String::new();
        let (format, held) = if self.op(Op::TextLen, t[2].get(), 0, 0) == 0 {
            (self.op(Op::ResText, res(R::RoomLabelCtrl_LabelDisconnected_String), 0, 0), false)
        } else {
            let mut f = self.op(Op::TextNew, "".as_ptr() as isize, 0, 0);
            if self.op(Op::TextLen, t[0].get(), 0, 0) != 0 {
                f = self.op(Op::TextAppend, f, self.op(Op::ResText, res(R::RoomLabelCtrl_LabelPrefix_String), 0, 0), 0);
            }
            if self.net().flag.get() && self.op(Op::TextLen, t[1].get(), 0, 0) != 0 {
                f = self.op(Op::TextAppend, f, self.op(Op::ResText, res(R::RoomLabelCtrl_LabelServer_String), 0, 0), 0);
            }
            f = self.op(Op::TextAppend, f, t[2].get(), 0);
            let users = self.net().number.get();
            if users > 0 {
                count = users.to_string();
                let suffix =
                    if users == 1 { R::RoomLabelCtrl_LabelUser_String } else { R::RoomLabelCtrl_LabelUsers_String };
                f = self.op(Op::TextAppend, f, self.op(Op::ResText, res(suffix), 0, 0), 0);
            }
            (f, true)
        };
        let count = self.op(Op::TextNew, count.as_ptr() as isize, count.len() as isize, 0);
        let args = [t[0].get(), t[1].get(), count];
        self.op(Op::LabelSetFormat3, self.m(0), format, args.as_ptr() as isize);
        self.op(Op::Release, count, 0, 0);
        if held {
            self.op(Op::Release, format, 0, 0);
        }
    }

    fn room_label_event(&self, event: isize, kind: Ev) {
        let flags = self.room_flags.get();
        let t = &self.net().texts;
        match kind {
            Ev::JoinedRoom => {
                if self.spec_mismatch(flags, event, EF::JrRoomSpec) {
                    return;
                }
                self.assign(&t[1], self.evf(event, EF::JrServerName));
                self.assign(&t[2], self.evf(event, EF::JrGroupName));
                if self.net().number.get() >= 0 {
                    let clients = self.evf(event, EF::JrClients);
                    self.net().number.set(self.mc(clients, MC::ListCount, 0) as i32);
                }
                self.room_label_calc();
            }
            Ev::LeftRoom => {
                if self.spec_mismatch(flags, event, EF::LrRoomSpec) {
                    return;
                }
                self.assign(&t[1], 0);
                self.assign(&t[2], 0);
                if self.net().number.get() > 0 {
                    self.net().number.set(0);
                }
                self.room_label_calc();
            }
            Ev::ClientChanged => {
                if self.net().number.get() < 0 {
                    return;
                }
                if self.cc_mismatch(flags, event) {
                    return;
                }
                if self.ask(Op::EventIsAdd, event) {
                    self.net().number.set(self.net().number.get().wrapping_add(1));
                    self.room_label_calc();
                    return;
                }
                if self.net().number.get() > 0 && self.ask(Op::EventIsDelete, event) {
                    self.net().number.set(self.net().number.get() - 1);
                }
                self.room_label_calc();
            }
            Ev::SetRoomSpecFlags => {
                let f = self.op(Op::EventFlags, event, 0, 0) as i32;
                self.room_flags.set(self.room_flags.get() | f);
            }
            _ => {
                self.op(Op::BaseEvent, event, 0, 0);
            }
        }
    }

    // ---- the dispatch

    pub(crate) fn net_event(&self, event: isize, kind: Ev, component: isize) {
        let me = self.ctl as isize;
        match self.kind {
            Kind::Room => self.room_event(event, kind, component),
            Kind::Friends => self.friends_event(event, kind, component),
            Kind::Users => self.users_event(event, kind, component),
            Kind::QuickPlay => self.quick_play_event(event, kind, component),
            Kind::Login => self.login_event(event, kind, component),
            Kind::CreateAccount => self.create_event(event, kind, component),
            Kind::UpdateAccount => self.update_event(event, kind, component),
            Kind::LostPassword => {
                if kind == Ev::InputTextChanged {
                    self.lost_password_update();
                } else if kind == Ev::ButtonPressed && component == self.m(1) {
                    self.op(Op::DeliverLostPassword, self.m(0), 0, 0);
                } else if kind == Ev::FillUserNames {
                    self.op(Op::ComboClear, self.m(0), 1, 0);
                    let mut i = 0;
                    while i < self.op(Op::UserCount, 0, 0, 0) {
                        self.op(Op::ComboAddUserName, self.m(0), i, 0);
                        i += 1;
                    }
                } else {
                    self.op(Op::BaseEvent, event, 0, 0);
                }
            }
            Kind::LostUserName => {
                if kind == Ev::InputTextChanged {
                    self.lost_user_name_update();
                } else if kind == Ev::ButtonPressed && component == self.m(1) {
                    self.op(Op::DeliverLostUserName, self.m(0), 0, 0);
                } else {
                    self.op(Op::BaseEvent, event, 0, 0);
                }
            }
            Kind::LoginStatus => {
                if kind == Ev::LoginInitStatus {
                    self.op(Op::LabelSetData, self.m(0), self.evf(event, EF::LisStatus), 0);
                } else {
                    self.op(Op::BaseEvent, event, 0, 0);
                }
            }
            Kind::InitFailed => {
                if kind == Ev::LoginInitStatus {
                    if self.evf(event, EF::LisKind) == 1 {
                        self.op(Op::SetVisible, me, 0, 0);
                    } else if self.evf(event, EF::LisKind) == 3 {
                        self.op(Op::LabelSetData, self.m(0), self.evf(event, EF::LisDescription), 0);
                        self.op(Op::SetVisible, self.m(2), self.evf(event, EF::LisAllowIgnore), 0);
                        self.op(Op::EnableLayout, me, 1, self.evf(event, EF::LisAllowIgnore));
                        self.op(Op::SetVisible, me, 1, 0);
                        self.op(Op::Relayout, me, 0, 0);
                    }
                    return;
                }
                if kind == Ev::ButtonPressed {
                    if component == self.m(1) {
                        self.op(Op::FireEvent, 100045, 0, 0);
                        return;
                    }
                    if component == self.m(2) {
                        self.op(Op::FireEvent, 100025, 0, 0);
                        return;
                    }
                }
                self.op(Op::BaseEvent, event, 0, 0);
            }
            Kind::Motd => {
                if kind == Ev::GotHttpDocument {
                    self.motd_show_message(self.evf(event, EF::GhdOp));
                } else {
                    self.op(Op::BaseEvent, event, 0, 0);
                }
            }
            Kind::LoginScreen => {
                if kind == Ev::ButtonPressed {
                    match self.op(Op::ControlId, component, 0, 0) as i32 {
                        200007 => {
                            self.op(Op::ScreenShow, self.m(LS_PANEL), self.m(LS_HELP), 1);
                            return;
                        }
                        200004 => {
                            self.login_screen_page(0);
                            return;
                        }
                        _ => {}
                    }
                }
                self.op(Op::BaseEvent, event, 0, 0);
            }
            Kind::DirectConnect => {
                if kind == Ev::ButtonPressed {
                    if component == self.m(1) {
                        self.op(Op::DeliverDirectConnect, 0, 0, 0);
                        return;
                    } else if component == self.m(2) {
                        let game = self.op(Op::ListSelGame, self.m(0), 0, 0);
                        if game != 0 {
                            self.op(Op::DeliverDirectConnect, 1, game, 0);
                        }
                        return;
                    }
                }
                self.op(Op::BaseEvent, event, 0, 0);
            }
            Kind::RoomLabel => self.room_label_event(event, kind),
            // LoginHelpCtrl has no handler of its own
            _ => {
                self.op(Op::BaseEvent, event, 0, 0);
            }
        }
    }

    /// LoginScreen's ShowMOTD (0), ShowCreateAccountCtrl (1), ShowLostPasswordCtrl (2), ShowLostUsernameCtrl (3).
    pub(crate) fn login_screen_page(&self, page: isize) {
        let shown = match page {
            0 => LS_MOTD,
            1 => LS_CREATE,
            2 => LS_LOST_PASSWORD,
            _ => LS_LOST_USER,
        };
        self.op(Op::ScreenShow, self.m(LS_PANEL), self.m(shown), 1);
    }

    /// A config reader's store for these kinds (RoomLabelCtrlConfig: 0 the prefix, 1 the room kind, 2 the user
    /// count shown, 3 the server shown).
    pub(crate) fn net_set_field(&self, f: isize, v: isize) {
        if self.kind != Kind::RoomLabel {
            return;
        }
        match f {
            0 => self.assign(&self.net().texts[0], v),
            1 => self.room_flags.set(v as i32),
            2 => {
                let n = self.net().number.get();
                if v == 0 {
                    self.net().number.set(-1);
                } else if n < 0 {
                    self.net().number.set(0);
                }
            }
            3 => self.net().flag.set(v != 0),
            _ => {}
        }
    }

    /// The destructor's part beyond the controls (released first: the members declared after the controls).
    pub(crate) fn net_release(&self) {
        let t = &self.net().texts;
        for i in (0..3).rev() {
            let p = t[i].replace(0);
            self.release(p);
        }
    }
}

// the members, in each original's member order
pub(crate) const RM_LIST: usize = 0;
pub(crate) const RM_JOIN: usize = 1;
pub(crate) const RM_CREATE: usize = 2;
pub(crate) const N_LIST: usize = 0;
pub(crate) const N_REMOVE: usize = 1;
pub(crate) const N_LOCATE: usize = 2;
pub(crate) const N_SEPARATOR: usize = 3;
pub(crate) const U_LIST: usize = 0;
pub(crate) const U_FIRST: usize = 1;
pub(crate) const U_SECOND: usize = 2;
pub(crate) const U_WHISPER: usize = 3;
pub(crate) const U_IGNORE: usize = 4;
pub(crate) const LG_USER: usize = 0;
pub(crate) const LG_PASSWORD: usize = 1;
pub(crate) const LG_REMEMBER: usize = 2;
pub(crate) const LG_LOGIN: usize = 3;
pub(crate) const CA_USER: usize = 0;
pub(crate) const CA_PASSWORD: usize = 1;
pub(crate) const CA_CONFIRM: usize = 2;
pub(crate) const CA_EMAIL: usize = 3;
pub(crate) const CA_MONTH: usize = 4;
pub(crate) const CA_DAY: usize = 5;
pub(crate) const CA_YEAR: usize = 6;
pub(crate) const CA_NEWSLETTER: usize = 7;
pub(crate) const CA_USER_DESC: usize = 8;
pub(crate) const CA_PASSWORD_DESC: usize = 9;
pub(crate) const CA_CONFIRM_DESC: usize = 10;
pub(crate) const CA_EMAIL_DESC: usize = 11;
pub(crate) const CA_BIRTH_DESC: usize = 12;
pub(crate) const CA_CREATE: usize = 13;
pub(crate) const UA_USER: usize = 0;
pub(crate) const UA_OLD: usize = 1;
pub(crate) const UA_PASSWORD: usize = 2;
pub(crate) const UA_CONFIRM: usize = 3;
pub(crate) const UA_EMAIL: usize = 4;
pub(crate) const UA_NEWSLETTER: usize = 5;
// (6: the user name's label, never read)
pub(crate) const UA_OLD_DESC: usize = 7;
pub(crate) const UA_PASSWORD_DESC: usize = 8;
pub(crate) const UA_CONFIRM_DESC: usize = 9;
pub(crate) const UA_EMAIL_DESC: usize = 10;
pub(crate) const UA_QUERY: usize = 11;
pub(crate) const UA_UPDATE: usize = 12;
pub(crate) const LS_PANEL: usize = 0;
pub(crate) const LS_MOTD: usize = 1;
pub(crate) const LS_CREATE: usize = 2;
pub(crate) const LS_LOST_USER: usize = 3;
pub(crate) const LS_LOST_PASSWORD: usize = 4;
pub(crate) const LS_HELP: usize = 5;

/// The number of members each small kind holds.
pub(crate) fn net_member_count(kind: Kind) -> usize {
    match kind {
        Kind::Room | Kind::DirectConnect | Kind::InitFailed => 3,
        Kind::Friends | Kind::Login => 4,
        Kind::Users => 5,
        Kind::QuickPlay | Kind::LoginScreen => 6,
        Kind::CreateAccount => 14,
        Kind::UpdateAccount => 13,
        Kind::LostPassword | Kind::LostUserName => 2,
        Kind::LoginHelp | Kind::LoginStatus | Kind::Motd | Kind::RoomLabel => 1,
        _ => 0,
    }
}

/// `LobbyIsValidEmail` over `len` bytes at `email`.
///
/// # Safety
/// `email` points at `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyctl_valid_email(email: *const u8, len: usize) -> bool {
    // SAFETY: the contract
    is_valid_email(unsafe { std::slice::from_raw_parts(email, len) })
}

#[cfg(test)]
#[path = "netctl_tests.rs"]
mod tests;
