//! ChatCtrl as lobbyctl.rs kind `Chat` (R6 WONLobby round 13): a lobby chat panel (the room's, the game room's and
//! the team's: its room kind is the config's GROUPTYPE), its output, input line and Send button, and the links it
//! prints (a client, a friend, a friend's location, a room, a join request's Accept and Reject). The panel held its
//! controls, two fonts, eleven colours, the room kind, our client id and the room's name; every handler's choice
//! (which format, which links, which arguments, the duration's unit and count, the commands it runs itself) is here.
//!
//! The links stay C++ `ChatLink`s (a `TextLink` the output paints and hit-tests): made by one op from an
//! [`EeWonLobbyCtlLink`], read and changed by the link ops. A formatted line is one op over an
//! [`EeWonLobbyCtlFormat`] (`LobbyAppendFormattedText`'s arguments). Texts the original made on the stack are held
//! data the Rust releases where the original's local went out of scope.

use std::cell::Cell;

use crate::lobbyctl::{EeWonLobbyCtl, EeWonLobbyCtlEvField as EF, EeWonLobbyCtlEvent as Ev, EeWonLobbyCtlOp as Op};
use crate::modelobj::{EeWonModelCall as MC, EeWonModelField as MF};
use crate::resource::EeWonLobbyRes as R;

/// A link to make (`new ChatLink(...)`, its constructor by `kind`: 3 a client, 0 a room, 4 a friend, 5 a friend's
/// location, 1 Accept, 2 Reject).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonLobbyCtlLink {
    pub kind: i32,
    /// the client (3, 1, 2) or the friend (4, 5)
    pub obj: isize,
    /// the text (0, 5, 1, 2): a `GUIStringData*`
    pub text: isize,
    /// the room's id (0)
    pub id: i32,
    /// the server (0: a `LobbyServer*`, NULL none) or the address (5: a `const EeWonIpAddr*`)
    pub addr: isize,
    pub font: isize,
    pub color: i32,
}

/// `LobbyAppendFormattedText(output, format, strings, colors, fonts, links, line break)`'s arguments.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonLobbyCtlFormat {
    pub output: isize,
    /// the format (a `GUIStringData*`)
    pub format: isize,
    pub strings: [isize; 2],
    pub num_strings: i32,
    pub links: [isize; 3],
    pub num_links: i32,
    pub colors: [i32; 9],
    pub fonts: [isize; 3],
    pub line_break: i32,
}

/// A link's member (`LinkGet` / `LinkSet`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyCtlLinkField {
    Kind = 0,
    Client,
    Friend,
    /// `mColor` (set)
    Color,
    /// `mEnabled` (set)
    Enabled,
    /// `mNext.get()` (set: the `SmartPtr`'s assignment)
    Next,
    /// `mPrevious` (set: a plain pointer)
    Previous,
}

/// ChatCtrl's members beyond its controls.
#[derive(Default)]
pub(crate) struct ChatState {
    /// mDefaultColor, mUserColor, mWhisperColor, mEmoteColor, mWarningColor, mErrorColor, mHelpColor, mGroupColor,
    /// mMessageColor, mAcceptColor, mRejectColor
    pub colors: [Cell<i32>; 11],
    pub client_id: Cell<u16>,
    /// `mRoomName` (one reference; 0 the empty string)
    pub room_name: Cell<isize>,
}

impl ChatState {
    pub fn init(&self) {
        let c = [0, 0x800000, 0x8000, 0x800080, 0xff8000, 0xff0000, 0x404040, 0x80, 0xff, 0, 0xa0a0a0];
        for (cell, v) in self.colors.iter().zip(c) {
            cell.set(v);
        }
        self.client_id.set(0xffff);
    }
}

// the colours' indices
const DEFAULT: usize = 0;
const USER: usize = 1;
const ERROR: usize = 5;
const GROUP: usize = 7;
const MESSAGE: usize = 8;
const ACCEPT: usize = 9;
const REJECT: usize = 10;

// the members, in the original's member order
pub(crate) const C_BOLD: usize = 0;
pub(crate) const C_ITALIC: usize = 1;
pub(crate) const C_INPUT: usize = 2;
pub(crate) const C_SEND: usize = 3;
pub(crate) const C_OUTPUT: usize = 4;

/// The chat commands the panel runs itself (`LobbyChatCommand_*`).
const CMD_WHISPER: isize = 2;
const CMD_IGNORE: i32 = 5;
const CMD_REPLY: i32 = 18;
const CMD_CLEAR: i32 = 19;
const CMD_HELP: i32 = 20;
const CMD_SHOW_TEAM: i32 = 21;

/// `LobbySplitDuration`: seconds as a count and a unit (0/1 minute(s), 2/3 hour(s), 4/5 day(s), 6 none), rounded up
/// or down.
pub fn split_duration(seconds: u32, round_up: bool) -> (u8, u32) {
    if seconds == 0 {
        return (6, 0);
    }
    if round_up {
        if seconds <= 60 {
            return (0, 1);
        }
        if seconds <= 3540 {
            return (1, seconds / 60 + !seconds.is_multiple_of(60) as u32);
        }
        if seconds <= 3600 {
            return (2, 1);
        }
        if seconds <= 82800 {
            return (3, seconds / 3600 + !seconds.is_multiple_of(3600) as u32);
        }
        if seconds <= 86400 {
            return (4, 1);
        }
        return (5, seconds / 86400 + !seconds.is_multiple_of(86400) as u32);
    }
    if seconds < 120 {
        return (0, 1);
    }
    if seconds < 3600 {
        return (1, seconds / 60);
    }
    if seconds < 7200 {
        return (2, 1);
    }
    if seconds < 86400 {
        return (3, seconds / 3600);
    }
    if seconds < 172800 {
        return (4, 1);
    }
    (5, seconds / 86400)
}

fn res(r: R) -> isize {
    r as isize
}

impl EeWonLobbyCtl {
    fn chat(&self) -> &ChatState {
        &self.ext.chat
    }

    fn color(&self, i: usize) -> i32 {
        self.chat().colors[i].get()
    }

    fn ev(&self, event: isize, f: EF) -> isize {
        self.op(Op::EvGet, event, f as isize, 0)
    }

    fn rel(&self, p: isize) {
        if p != 0 {
            self.op(Op::Release, p, 0, 0);
        }
    }

    /// `(mRoomSpecFlags & e->x) != e->x` (the event's member read twice, as the original's).
    fn not_my_room(&self, event: isize, f: EF) -> bool {
        let a = self.ev(event, f) as i32;
        self.room_flags.get() & a != self.ev(event, f) as i32
    }

    fn is_team_room(&self) -> bool {
        self.room_flags.get() & 16 != 0
    }

    fn output(&self) -> isize {
        self.m(C_OUTPUT)
    }

    /// A resource text (the resource's own data: no reference).
    fn res_text(&self, r: isize) -> isize {
        self.op(Op::ResText, r, 0, 0)
    }

    /// `AddFormattedText(format, strings, links, line break)`.
    fn add_formatted(&self, format: isize, strings: &[isize], links: &[isize]) {
        let mut f = EeWonLobbyCtlFormat {
            output: self.output(),
            format,
            num_strings: strings.len() as i32,
            num_links: links.len() as i32,
            line_break: 1,
            ..Default::default()
        };
        f.strings[..strings.len()].copy_from_slice(strings);
        f.links[..links.len()].copy_from_slice(links);
        for (i, c) in [DEFAULT, USER, 2, 3, 4, ERROR, 6, GROUP, MESSAGE].into_iter().enumerate() {
            f.colors[i] = self.color(c);
        }
        f.fonts = [self.op(Op::TextAreaFont, self.output(), 0, 0), self.m(C_BOLD), self.m(C_ITALIC)];
        self.op(Op::ChatAddFormatted, &f as *const EeWonLobbyCtlFormat as isize, 0, 0);
    }

    fn new_link(&self, l: EeWonLobbyCtlLink) -> isize {
        self.op(Op::ChatLinkNew, &l as *const EeWonLobbyCtlLink as isize, 0, 0)
    }

    /// `MakeClientLink(client)`: held.
    fn client_link(&self, client: isize) -> isize {
        let color = self.color(USER);
        let font = self.op(Op::TextAreaFont, self.output(), 0, 0);
        self.new_link(EeWonLobbyCtlLink { kind: 3, obj: client, font, color, ..Default::default() })
    }

    fn room_link(&self, text: isize, id: i32, server: isize) -> isize {
        let color = self.color(GROUP);
        let font = self.op(Op::TextAreaLinkFont, self.output(), 0, 0);
        self.new_link(EeWonLobbyCtlLink { kind: 0, text, id, addr: server, font, color, ..Default::default() })
    }

    fn friend_link(&self, friend: isize) -> isize {
        let color = self.color(USER);
        let font = self.op(Op::TextAreaFont, self.output(), 0, 0);
        self.new_link(EeWonLobbyCtlLink { kind: 4, obj: friend, font, color, ..Default::default() })
    }

    fn link_get(&self, link: isize, f: EeWonLobbyCtlLinkField) -> isize {
        self.op(Op::LinkGet, link, f as isize, 0)
    }

    fn link_set(&self, link: isize, f: EeWonLobbyCtlLinkField, v: isize) {
        self.op(Op::LinkSet, link, f as isize, v);
    }

    // ---- the handlers

    pub(crate) fn chat_init(&self, config: isize, local: &Cell<isize>) {
        for slot in crate::lobbyctl::SLOT_CHAT..crate::lobbyctl::SLOT_CHAT + 3 {
            self.find(config, slot, local);
        }
        let font = self.op(Op::TextAreaFont, self.output(), 0, 0);
        let bold = self.op(Op::FontMod, self.ctl as isize, font, 1);
        self.assign(&self.members[C_BOLD], bold);
        let font = self.op(Op::TextAreaFont, self.output(), 0, 0);
        let italic = self.op(Op::FontMod, self.ctl as isize, font, 2);
        self.assign(&self.members[C_ITALIC], italic);
        self.op(Op::SubscribeBroadcast, 1, 0, 0);
    }

    fn disable_link_chain(&self, link: isize, color: i32, linked: i32) {
        use EeWonLobbyCtlLinkField as L;
        self.link_set(link, L::Color, color as isize);
        self.link_set(link, L::Enabled, 0);
        let mut l = self.link_get(link, L::Next);
        while l != 0 {
            self.link_set(l, L::Enabled, 0);
            self.link_set(l, L::Color, linked as isize);
            l = self.link_get(l, L::Next);
        }
        let mut l = self.link_get(link, L::Previous);
        while l != 0 {
            self.link_set(l, L::Enabled, 0);
            self.link_set(l, L::Color, linked as isize);
            l = self.link_get(l, L::Previous);
        }
        self.op(Op::Invalidate, self.output(), 0, 0);
    }

    fn announce_client_location(&self, event: isize) {
        if self.ev(event, EF::AclHandled) != 0 || self.not_my_room(event, EF::AclRoomId) {
            return;
        }
        let client = self.ev(event, EF::AclClient);
        if client == 0 || !self.ask(Op::IsRootVisible, 0) {
            return;
        }
        self.op(Op::EvSetHandled, event, EF::AclHandled as isize, 0);
        let server = self.op(Op::CurrentServer, 0, 0, 0);
        let client_link = self.client_link(self.ev(event, EF::AclClient));
        let room = self.ev(event, EF::AclRoom);
        let room_link = self.room_link(room, self.mget_c(client, MF::ClientGameGroupId) as u16 as i32, server);
        let chat = self.ev(event, EF::AclServer);
        let server_link = self.room_link(chat, self.mget_c(client, MF::ClientChatGroupId) as u16 as i32, server);
        let has_room = self.op(Op::TextLen, self.ev(event, EF::AclRoom), 0, 0) != 0;
        let has_server = self.op(Op::TextLen, self.ev(event, EF::AclServer), 0, 0) != 0;
        let format = if self.op(Op::MCall, client, MC::ClientAnonymous as isize, 0) != 0 {
            R::ChatCtrl_ClientIsAnonymous_String
        } else if has_room && has_server {
            R::ChatCtrl_ClientInGameAndChat_String
        } else if has_server {
            R::ChatCtrl_ClientInChat_String
        } else if has_room {
            R::ChatCtrl_ClientInGame_String
        } else {
            R::ChatCtrl_ClientNotOnServer_String
        };
        self.add_formatted(self.res_text(res(format)), &[], &[client_link, room_link, server_link]);
        self.rel(server_link);
        self.rel(room_link);
        self.rel(client_link);
    }

    fn mget_c(&self, obj: isize, f: MF) -> isize {
        self.op(Op::MGet, obj, f as isize, 0)
    }

    fn announce_friend(&self, event: isize) {
        if self.not_my_room(event, EF::AfLocation) || self.ev(event, EF::AfFriend) == 0 {
            return;
        }
        let friend = self.ev(event, EF::AfFriend);
        if self.ask(Op::EventIsAdd, event) {
            let link = self.friend_link(friend);
            self.add_formatted(self.res_text(res(R::ChatCtrl_FriendJoined_String)), &[], &[link]);
            self.rel(link);
            return;
        }
        if self.ask(Op::EventIsDelete, event) {
            let link = self.friend_link(friend);
            self.add_formatted(self.res_text(res(R::ChatCtrl_FriendLeft_String)), &[], &[link]);
            self.rel(link);
            return;
        }
        if self.ev(event, EF::AfHandled) != 0 || !self.ask(Op::IsRootVisible, 0) {
            return;
        }
        self.op(Op::EvSetHandled, event, EF::AfHandled as isize, 0);
        // the IPAddr copy and the location text (a GUIString copy: held)
        let address = self.mget_c(friend, MF::FriendAddress);
        let location = self.mget_c(friend, MF::FriendLocation);
        self.op(Op::AddRef, location, 0, 0);
        let flags = || self.mget_c(friend, MF::FriendFlags) as u8;
        let has_location =
            flags() & 3 != 0 && self.ask(Op::AddrValid, address) && self.op(Op::TextLen, location, 0, 0) != 0;
        let friend_link = self.friend_link(friend);
        let location_link = if has_location {
            let color = self.color(USER);
            let font = self.op(Op::TextAreaFont, self.output(), 0, 0);
            self.new_link(EeWonLobbyCtlLink {
                kind: 5,
                obj: friend,
                text: location,
                addr: address,
                font,
                color,
                ..Default::default()
            })
        } else {
            0
        };
        let format = if !has_location {
            R::ChatCtrl_FriendIsNotOnline_String
        } else if flags() & 1 != 0 {
            if flags() & 2 != 0 {
                R::ChatCtrl_FriendIsInGameStaging_String
            } else {
                R::ChatCtrl_FriendIsChatting_String
            }
        } else if flags() & 2 != 0 {
            R::ChatCtrl_FriendIsPlaying_String
        } else {
            R::ChatCtrl_FriendIsOn_String
        };
        self.add_formatted(self.res_text(res(format)), &[], &[friend_link, location_link]);
        self.rel(location_link);
        self.rel(friend_link);
        self.op(Op::Release, location, 0, 0);
    }

    fn text_link_activated(&self, event: isize) {
        use EeWonLobbyCtlLinkField as L;
        let link = self.ev(event, EF::TlaLink);
        if link == 0 {
            return;
        }
        let flags = self.room_flags.get() as isize;
        match self.link_get(link, L::Kind) {
            3 => {
                self.op(Op::DeliverClientAction, 18, flags, self.link_get(link, L::Client));
            }
            4 => {
                self.op(Op::DeliverFriendAction, 5, self.link_get(link, L::Friend), flags);
            }
            5 => {
                self.op(Op::DeliverJoinFriendsRoom, self.link_get(link, L::Friend), 0, 0);
            }
            0 => {
                self.op(Op::DeliverJumpToRoom, link, 0, 0);
            }
            k @ (1 | 2) => {
                self.disable_link_chain(link, self.color(ACCEPT), self.color(REJECT));
                self.op(Op::DeliverAcceptClient, self.link_get(link, L::Client), (k == 1) as isize, 0);
            }
            _ => {}
        }
    }

    fn player_team_changed(&self, event: isize) {
        if !self.is_team_room() {
            return;
        }
        let format = if self.ask(Op::EventIsAdd, event) {
            R::ChatCtrl_TeamMemberJoined_String
        } else if self.ask(Op::EventIsDelete, event) {
            R::ChatCtrl_TeamMemberLeft_String
        } else {
            return;
        };
        let link = self.client_link(self.ev(event, EF::PtcClient));
        self.add_formatted(self.res_text(res(format)), &[], &[link]);
        self.rel(link);
    }

    fn recv_chat(&self, event: isize) {
        let client_id = self.chat().client_id.get() as isize;
        if self.ev(event, EF::RcSender) == 0 || client_id == 0xffff {
            return;
        }
        let is = |f: EF| self.ev(event, f) != 0;
        if is(EF::RcIsTeam) {
            let spec = self.ev(event, EF::RcRoomSpec) as i32;
            if (self.room_flags.get() | 16) & spec != self.ev(event, EF::RcRoomSpec) as i32 {
                return;
            }
        } else {
            if self.not_my_room(event, EF::RcRoomSpec) {
                return;
            }
            if self.is_team_room() && !is(EF::RcIsWhisper) && !is(EF::RcIsWarning) {
                return;
            }
        }
        let cross_room = is(EF::RcIsTeam) && !self.is_team_room();
        let sender = self.client_link(self.ev(event, EF::RcSender));
        let mut target = 0;
        let mut links = vec![sender];
        if self.ev(event, EF::RcTarget) != 0 {
            target = self.client_link(self.ev(event, EF::RcTarget));
            links.push(target);
        }
        let text = self.ev(event, EF::RcText);
        let id_of = |f: EF| self.mget_c(self.ev(event, f), MF::ClientId);
        let format = if is(EF::RcIsEmote) {
            Some(if cross_room { R::ChatCtrl_TeamEmote_String } else { R::ChatCtrl_Emote_String })
        } else if is(EF::RcIsWhisper) && self.ev(event, EF::RcTarget) != 0 {
            Some(if id_of(EF::RcSender) == client_id {
                if id_of(EF::RcTarget) == client_id {
                    R::ChatCtrl_YouWhisperToYourself_String
                } else {
                    R::ChatCtrl_YouWhisper_String
                }
            } else {
                R::ChatCtrl_SomeoneWhispersToYou_String
            })
        } else if is(EF::RcIsWarning) && self.ev(event, EF::RcTarget) != 0 {
            Some(if id_of(EF::RcSender) == client_id {
                if id_of(EF::RcTarget) == client_id {
                    R::ChatCtrl_YouWarnYourself_String
                } else {
                    R::ChatCtrl_YouWarnSomeone_String
                }
            } else if id_of(EF::RcTarget) == client_id {
                R::ChatCtrl_SomeoneWarnsYou_String
            } else {
                R::ChatCtrl_SomeoneWarnsSomeone_String
            })
        } else {
            Some(if cross_room { R::ChatCtrl_TeamChat_String } else { R::ChatCtrl_NormalChat_String })
        };
        if let Some(f) = format {
            let f = self.res_text(res(f));
            if self.op(Op::TextLen, f, 0, 0) != 0 {
                self.add_formatted(f, &[text], &links);
            }
        }
        self.rel(target);
        self.rel(sender);
    }

    fn joined_room(&self, event: isize) {
        if self.not_my_room(event, EF::JrRoomSpec) {
            return;
        }
        self.chat().client_id.set(self.ev(event, EF::JrClientId) as u16);
        let name = self.ev(event, EF::JrGroupName);
        self.assign(&self.chat().room_name, name);
        self.set_enabled(self.m(C_SEND), true);
        self.add_formatted(
            self.res_text(res(R::ChatCtrl_EnteringRoom_String)),
            &[self.ev(event, EF::JrGroupName)],
            &[],
        );
        self.op(Op::TextAreaLineBreak, self.output(), 10, 0);
    }

    fn left_room(&self, event: isize) {
        if self.not_my_room(event, EF::LrRoomSpec) {
            return;
        }
        self.chat().client_id.set(0xffff);
        self.set_enabled(self.m(C_SEND), false);
        if self.ev(event, EF::LrFlag) == 0 {
            return;
        }
        let format = if self.ev(event, EF::LrForced) != 0 {
            R::ChatCtrl_Disconnected_String
        } else {
            R::ChatCtrl_LeavingRoom_String
        };
        self.add_formatted(self.res_text(res(format)), &[self.chat().room_name.get()], &[]);
        self.op(Op::TextAreaLineBreak, self.output(), 10, 0);
    }

    /// A number as a temporary text (`sprintf(text, "%lu", value); GUIString number(text)`): held.
    fn number_text(&self, v: u32) -> isize {
        let s = v.to_string();
        self.op(Op::TextNew, s.as_ptr() as isize, s.len() as isize, 0)
    }

    fn alert(&self, event: isize) {
        if self.not_my_room(event, EF::AlKind) {
            return;
        }
        let text = self.ev(event, EF::AlText);
        let format = match self.ev(event, EF::AlValue) as i32 {
            0 => self.res_text(res(R::ChatCtrl_ServerAlert_String)),
            1 => {
                let (unit, value) = split_duration(self.ev(event, EF::AlSeverity) as u32, false);
                let mut format = self.res_text(unit as isize + 0x86);
                if value != 0 {
                    let number = self.number_text(value);
                    self.add_formatted(format, &[number], &[]);
                    self.op(Op::Release, number, 0, 0);
                    format = self.res_text(res(R::ChatCtrl_AlertComment_String));
                }
                format
            }
            _ => {
                self.add_formatted(self.res_text(res(R::ChatCtrl_ServerShutdownAborted_String)), &[], &[]);
                self.res_text(res(R::ChatCtrl_AlertComment_String))
            }
        };
        if self.op(Op::TextLen, text, 0, 0) != 0 {
            self.add_formatted(format, &[text], &[]);
        }
    }

    fn set_chat_input(&self, event: isize) {
        if self.ev(event, EF::SciHandled) != 0
            || self.not_my_room(event, EF::SciRoomSpec)
            || !self.ask(Op::IsRootVisible, 0)
        {
            return;
        }
        self.op(Op::EvSetHandled, event, EF::SciHandled as isize, 0);
        let parser = self.op(Op::ChatParser, 0, 0, 0);
        if parser == 0 {
            return;
        }
        let input_box = self.m(C_INPUT);
        self.op(Op::RequestFocus, input_box, 0, 0);
        let mut input =
            self.op(Op::ParserCommandText, parser, self.ev(event, EF::SciCommand), self.ev(event, EF::SciText));
        self.op(Op::InputSetText, input_box, input, 0);
        self.op(Op::InputSelectRange, input_box, -1, -1);
        let current = self.op(Op::InputText, input_box, 0, 0);
        if self.op(Op::TextFind, current, input, 0) as i32 != -1 {
            self.op(Op::Release, input, 0, 0);
            return;
        }
        let first = self.op(Op::TextAt, self.op(Op::InputText, input_box, 0, 0), 0, 0);
        if first != self.op(Op::ParserNumber, parser, 0, 0) {
            input = self.op(Op::TextAppend, input, self.op(Op::InputText, input_box, 0, 0), 0);
        }
        self.op(Op::InputSetText, input_box, input, 0);
        self.op(Op::InputSelectRange, input_box, -1, -1);
        self.op(Op::Release, input, 0, 0);
    }

    fn group_invitation(&self, event: isize) {
        if self.not_my_room(event, EF::GiKind) {
            return;
        }
        let client = self.client_link(self.ev(event, EF::GiClient));
        let server = self.op(Op::CurrentServer, 0, 0, 0);
        let room = self.room_link(self.ev(event, EF::GiGameName), self.ev(event, EF::GiGameId) as u16 as i32, server);
        let texts = [self.ev(event, EF::GiGameName), self.ev(event, EF::GiComment)];
        let flag = self.ev(event, EF::GiFlag) != 0;
        let invite = self.ev(event, EF::GiInvite) != 0;
        let mut id = if !flag {
            if invite {
                0x8d
            } else {
                0x8e
            }
        } else if invite {
            0x8f
        } else {
            0x90
        };
        if self.op(Op::TextLen, self.ev(event, EF::GiComment), 0, 0) != 0 {
            id += 4;
        }
        self.add_formatted(self.res_text(id), &texts, &[client, room]);
        self.rel(room);
        self.rel(client);
    }

    fn group_join_attempt(&self, event: isize) {
        use EeWonLobbyCtlLinkField as L;
        if self.not_my_room(event, EF::GjaKind) || self.is_team_room() {
            return;
        }
        let client = self.client_link(self.ev(event, EF::GjaClient));
        let link = |kind: i32, text: R| {
            let color = self.color(MESSAGE);
            let font = self.op(Op::TextAreaLinkFont, self.output(), 0, 0);
            let text = self.res_text(res(text));
            let obj = self.ev(event, EF::GjaClient);
            self.new_link(EeWonLobbyCtlLink { kind, obj, text, font, color, ..Default::default() })
        };
        let accept = link(1, R::ChatCtrl_Accept_String);
        let decline = link(2, R::ChatCtrl_Reject_String);
        self.link_set(accept, L::Previous, decline);
        self.link_set(decline, L::Next, accept);
        let comment = self.ev(event, EF::GjaComment);
        let format = if self.op(Op::TextLen, self.ev(event, EF::GjaComment), 0, 0) == 0 {
            R::ChatCtrl_SomeoneAsksToJoin_String
        } else {
            R::ChatCtrl_SomeoneAsksToJoinExpl_String
        };
        self.add_formatted(self.res_text(res(format)), &[comment], &[client, accept, decline]);
        self.rel(decline);
        self.rel(accept);
        self.rel(client);
    }

    fn show_parser_error(&self, parser: isize) {
        let with_target = |r: R| self.op(Op::FormatTarget, res(r), parser, 0);
        let (mut error, held) = match self.op(Op::ParserNumber, parser, 1, 0) as i32 {
            3 => (with_target(R::RoutingLogic_ClientNotFound_String), true),
            4 => (with_target(R::RoutingLogic_ClientAmbiguous_String), true),
            1 => (self.res_text(res(R::RoutingLogic_InvalidCommand_String)), false),
            2 => (self.res_text(res(R::RoutingLogic_RequireName_String)), false),
            7 => (self.res_text(res(R::RoutingLogic_BadTimeSpec_String)), false),
            _ => (0, false),
        };
        let mut held = if held { error } else { 0 };
        if self.op(Op::TextLen, error, 0, 0) == 0 {
            self.rel(held);
            held = 0;
            error = self.res_text(res(R::RoutingLogic_Error_String));
        }
        self.op(Op::TextAreaAddSegment, self.output(), error, self.color(ERROR) as isize);
        self.rel(held);
    }

    fn client_moderated(&self, event: isize) {
        if self.not_my_room(event, EF::CmRoomSpec) {
            return;
        }
        let client = self.client_link(self.ev(event, EF::CmClient));
        let duration = || self.ev(event, EF::CmDuration) as u32;
        let mut value = 0;
        let mut split = |base: isize| {
            let (unit, v) = split_duration(duration(), true);
            value = v;
            self.res_text(unit as isize + base)
        };
        let format = if self.ev(event, EF::CmFlag) != 0 {
            match self.ev(event, EF::CmKind) as i32 {
                0 => split(0x64),
                2 => split(0x74),
                3 => self.res_text(res(R::ChatCtrl_YouWereUnmuted_String)),
                _ => self.res_text(res(R::ChatCtrl_YouWereKicked_String)),
            }
        } else {
            match self.ev(event, EF::CmKind) as i32 {
                0 => split(0x6c),
                2 => split(0x7c),
                3 => self.res_text(res(R::ChatCtrl_SomeoneWasUnmuted_String)),
                _ => self.res_text(res(R::ChatCtrl_SomeoneWasKicked_String)),
            }
        };
        let room = self.ev(event, EF::CmRoomName);
        if value == 0 {
            self.add_formatted(format, &[room], &[client]);
        } else {
            let number = self.number_text(value);
            self.add_formatted(format, &[room, number], &[client]);
            self.op(Op::Release, number, 0, 0);
        }
        let comment = self.ev(event, EF::CmComment);
        if self.op(Op::TextLen, comment, 0, 0) != 0 {
            self.add_formatted(self.res_text(res(R::ChatCtrl_ModeratorComment_String)), &[comment], &[]);
        }
        self.rel(client);
    }

    /// `if (names.length() != 0) names.append(", ")`: the held line, a new one.
    fn append_comma(&self, line: isize) -> isize {
        if self.op(Op::TextLen, line, 0, 0) != 0 {
            let comma = ", ";
            return self.op(Op::TextAppendBytes, line, comma.as_ptr() as isize, comma.len() as isize);
        }
        line
    }

    fn send_input(&self) {
        let input_box = self.m(C_INPUT);
        if self.op(Op::TextLen, self.op(Op::InputText, input_box, 0, 0), 0, 0) == 0
            || self.ask(Op::IsDisabled, self.m(C_SEND))
        {
            return;
        }
        let parser = self.op(Op::RoomChatParser, self.room_flags.get() as isize, 0, 0);
        if parser == 0 {
            return;
        }
        let input = self.op(Op::InputText, input_box, 0, 0);
        self.op(Op::AddRef, input, 0, 0);
        self.op(Op::InputAddHistory, input_box, input, 0);
        self.op(Op::InputClear, input_box, 1, 0);
        if !self.ask_with(Op::ParserHandleInput, parser, input) {
            self.show_parser_error(parser);
            self.op(Op::Release, input, 0, 0);
            return;
        }
        let command = self.op(Op::ParserNumber, parser, 2, 0) as i32;
        let done = match command {
            CMD_HELP => {
                let help = self.op(Op::ParserHelp, parser, self.chat().client_id.get() as isize, 0);
                self.add_formatted(help, &[], &[]);
                self.op(Op::Release, help, 0, 0);
                true
            }
            CMD_CLEAR => {
                self.op(Op::TextAreaReset, self.output(), 0, 0);
                true
            }
            CMD_REPLY => {
                let text = self.op(Op::ParserText, parser, 1, 0);
                let has_text = self.op(Op::TextLen, text, 0, 0) != 0;
                self.op(Op::Release, text, 0, 0);
                if has_text {
                    false
                } else {
                    let command = self.op(Op::ParserCommandText, parser, CMD_WHISPER, 0);
                    self.op(Op::InputSetText, input_box, command, 0);
                    self.op(Op::InputSelectRange, input_box, -1, -1);
                    self.op(Op::Release, command, 0, 0);
                    true
                }
            }
            CMD_SHOW_TEAM => {
                let clients = self.op(Op::ParserClients, parser, 0, 0);
                let me = self.op(Op::MyClient, 0, 0, 0);
                if me != 0 {
                    let mut names = self.op(Op::TextNew, "".as_ptr() as isize, 0, 0);
                    // the iterator's HasMore and Next: the list's count read again, the client at the cursor
                    let mut i = 0;
                    while clients != 0 && i < self.op(Op::MCall, clients, MC::ListCount as isize, 0) {
                        let client = self.op(Op::MCall, clients, MC::ListAt as isize, i);
                        i += 1;
                        if self.op(Op::MCall, me, MC::ClientIsTeammate as isize, client) != 0 {
                            names = self.append_comma(names);
                            let name = self.mget_c(client, MF::ClientName);
                            names = self.op(Op::TextAppend, names, name, 0);
                        }
                    }
                    self.add_formatted(self.res_text(res(R::ChatCtrl_TeamMembers_String)), &[names], &[]);
                    self.op(Op::Release, names, 0, 0);
                }
                true
            }
            CMD_IGNORE => {
                let target = self.op(Op::ParserText, parser, 0, 0);
                let has_target = self.op(Op::TextLen, target, 0, 0) != 0;
                self.op(Op::Release, target, 0, 0);
                if has_target {
                    false
                } else {
                    let mut names = self.op(Op::TextNew, "".as_ptr() as isize, 0, 0);
                    let mut i = 0;
                    while i < self.op(Op::IgnoredCount, 0, 0, 0) {
                        names = self.append_comma(names);
                        let name = self.op(Op::IgnoredAt, i, 0, 0);
                        names = self.op(Op::TextAppend, names, name, 0);
                        self.op(Op::Release, name, 0, 0);
                        i += 1;
                    }
                    self.add_formatted(self.res_text(res(R::ChatCtrl_IgnoreList_String)), &[names], &[]);
                    self.op(Op::Release, names, 0, 0);
                    true
                }
            }
            _ => false,
        };
        if !done {
            self.op(Op::FireEvent, 100012, 1, 0);
        }
        self.op(Op::Release, input, 0, 0);
    }

    pub(crate) fn chat_event(&self, event: isize, kind: Ev, component: isize) {
        match kind {
            Ev::TextLinkActivated => return self.text_link_activated(event),
            Ev::ButtonPressed if component == self.m(C_SEND) => return self.send_input(),
            Ev::InputReturn if component == self.m(C_INPUT) => return self.send_input(),
            Ev::Alert => return self.alert(event),
            Ev::AnnounceFriend => return self.announce_friend(event),
            Ev::AnnounceClientLocation => return self.announce_client_location(event),
            Ev::ClientModerated => return self.client_moderated(event),
            Ev::ChatCommandError => {
                let text = self.ev(event, EF::CceText);
                self.op(Op::TextAreaAddSegment, self.output(), text, self.color(ERROR) as isize);
                return;
            }
            Ev::GroupInvitation => return self.group_invitation(event),
            Ev::GroupJoinAttempt => return self.group_join_attempt(event),
            Ev::JoinedRoom => return self.joined_room(event),
            Ev::LeftRoom => return self.left_room(event),
            Ev::PlayerTeamChanged => return self.player_team_changed(event),
            Ev::RecvChat => return self.recv_chat(event),
            Ev::SetChatInput => return self.set_chat_input(event),
            Ev::SetRoomSpecFlags => {
                let flags = self.op(Op::EventFlags, event, 0, 0) as i32;
                self.room_flags.set(self.room_flags.get() | flags);
                return;
            }
            _ => {}
        }
        self.op(Op::BaseEvent, event, 0, 0);
    }

    /// The destructor's part beyond the controls: the room's name first (the last member).
    pub(crate) fn chat_release(&self) {
        let name = self.chat().room_name.replace(0);
        self.rel(name);
    }
}

impl EeWonLobbyCtl {
    /// A config reader's store (`SetField`).
    pub(crate) fn set_field(&self, f: isize, v: isize) {
        if self.kind == crate::lobbyctl::EeWonLobbyCtlKind::Chat {
            match f {
                0..=10 => self.chat().colors[f as usize].set(v as i32),
                20 => self.room_flags.set(v as i32),
                _ => {}
            }
        }
    }
}

/// `LobbySplitDuration(seconds, round_up, &value)`: the unit, the count at `value`.
///
/// # Safety
/// `value` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyctl_split_duration(seconds: u32, round_up: bool, value: *mut u32) -> u8 {
    let (unit, v) = split_duration(seconds, round_up);
    // SAFETY: the contract
    unsafe { *value = v };
    unit
}

#[cfg(test)]
#[path = "chatctl_tests.rs"]
mod tests;
