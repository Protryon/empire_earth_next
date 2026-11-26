//! `EEAWonNet`, the WON front end's UI-thread action (R6 WON lobby and network round 37): the class whole.
//!
//! `EEWonNet` posts one action per network callback; the UI thread runs it (`Virtual4`, [`EeWonAction::run`]) and
//! it feeds the WON forms: the game room's games box (type 3) and player list (type 4), a chat line (types 6 and
//! 7), and, in the original only, the room list's box (type 2) and its two labels (type 8) -- `EEWonNet` fills
//! those itself now, but the cases are kept as they were. The C++ class is a shell holding this object; its
//! members (`mType`, `mList`, `mChat`, the five strings, the counts and the flags) are the fields here, set by the
//! three constructors as the original's set them (a member a constructor left alone is 0 here). `Virtual4`'s
//! five function statics (the selected game's name, the game count and selection, the two display lines) are
//! [`State`].
//!
//! Every statement on a C++ object is a host op ([`EeWonActionOp`]), one statement each in `EEAWonNet.cpp`. Texts
//! are the `UWideString`s' bytes (NUL-terminated multibyte text; `operator==` is `PlatMbsCmp`, a byte compare).
//!
//! Golden: tests/won/won_action37a_test.cpp runs the original `Virtual4` and this one over a stand-in UI.

use std::ffi::{c_char, c_long, c_ulong, CStr};
use std::sync::Mutex;

/// The host's ops: one C++ statement each. A control is a `UIControl*` (a form's `GetControl`), a text given is
/// NUL-terminated bytes.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonActionOp {
    /// `gUIManager->GetCurrentForm() == gUIManager->GetFormByID(a)`.
    CurrentFormIs = 1,
    /// `gUIManager->GetCurrentForm()->GetControl(a)`.
    CurrentControl,
    /// `gUIManager->GetFormByID(a)->GetControl(b)`.
    FormControl,
    /// `new UWideString`.
    TextNew,
    /// `(const char*)*(UWideString*)a`.
    TextBytes,
    /// `delete (UWideString*)a`.
    TextDelete,
    /// `gResource->GetString(a, *(UWideString*)b)`.
    ResString,
    /// `((UITextLabel*)a)->SetText(UWideString((const char*)b))`.
    LabelSetText,
    /// `((UITextLabel*)a)->GetScrollBar() != NULL`.
    LabelHasScrollBar,
    /// `((UITextLabel*)a)->GetScrollBar()->GetSlider()->GetMaxRange()`.
    LabelSliderMax,
    /// `((UITextLabel*)a)->GetScrollBar()->GetSlider()->GetCurrentRangeValue()`.
    LabelSliderValue,
    /// `((UITextLabel*)a)->AppendText(UWideString((const char*)b))`.
    LabelAppendText,
    /// `((UITextLabel*)a)->SetStartingDisplayLine(b)`.
    LabelSetLine,
    /// `((UIListBox<UWideString>*)a)->GetDisplayLine()`.
    ListDisplayLine,
    /// `((UIListBox<UWideString>*)a)->DeleteAllEntries()`.
    ListDeleteAll,
    /// `((UIListBox<UWideString>*)a)->GetLineCount()`.
    ListLineCount,
    /// `((UIListBox<UWideString>*)a)->SetStartingDisplayLine(b)`.
    ListSetLine,
    /// `((UIListBox<UWideString>*)a)->AppendLine(UWideString((const char*)b))`.
    ListAppend,
    /// `dynamic_cast<EEUICWonNetGamesBox*>(gUIManager->GetFormByID(kFormAvailableGames)->GetControl(
    /// kAvailableGamesAvalGamesListBox))`.
    GamesBox,
    /// `((EEUICWonNetGamesBox*)a)->GetLineCount()`.
    GamesLineCount,
    /// `((EEUICWonNetGamesBox*)a)->GetDisplayLine()`.
    GamesDisplayLine,
    /// `((EEUICWonNetGamesBox*)a)->GetCurrentSelection(*(unsigned long*)b)`.
    GamesSelection,
    /// `(const char*)((EEUICWonNetGamesBox*)a)->GetData(b).mName`.
    GamesName,
    /// `((EEUICWonNetGamesBox*)a)->DeleteAllEntries()`.
    GamesDeleteAll,
    /// `((EEUICWonNetGamesBox*)a)->SetStartingDisplayLine(b)`.
    GamesSetLine,
    /// `((EEUICWonNetGamesBox*)a)->ChangeSelectionIndex(b, true, false)`.
    GamesSelect,
    /// `((EEUICWonNetGamesBox*)a)->AppendLine(<the EEUICWNGBLine of the EeWonActionLine b>)`.
    GamesAppend,
    /// `dynamic_cast<EEUICWonNetRoomsBox*>(gUIManager->GetFormByID(kFormAvailableRooms)->GetControl(
    /// kAvailableRoomsAvailRoomsListBox))`.
    RoomsBox,
    /// `((EEUICWonNetRoomsBox*)a)->DeleteAllEntries()`.
    RoomsDeleteAll,
    /// `((EEUICWonNetRoomsBox*)a)->AppendLine(<the EEUICWNRBLine of the EeWonActionLine b>)`.
    RoomsAppend,
}

/// A list line the action appends: a games box line (`EEUICWNGBLine`: the name, the counts, the three flags) or a
/// rooms box line (`EEUICWNRBLine`: the name, the description, the players in `humans`, the games in `players`).
#[repr(C)]
pub struct EeWonActionLine {
    /// `mName`.
    pub name: *const c_char,
    /// `EEUICWNRBLine::mDescription`.
    pub description: *const c_char,
    /// `EEUICWNGBLine::mHumans`, `EEUICWNRBLine::mNumPlayers`.
    pub humans: c_long,
    /// `EEUICWNGBLine::mPlayers`, `EEUICWNRBLine::mNumGames`.
    pub players: c_long,
    /// `mHasPassword`.
    pub has_password: bool,
    /// `mIsPrivate`.
    pub is_private: bool,
    /// `mIsInProgress`.
    pub is_in_progress: bool,
}

/// The host: `EEAWonNet.cpp`'s `WonActionHost`.
pub type EeWonActionHost = unsafe extern "C" fn(op: EeWonActionOp, a: isize, b: isize) -> isize;

const FORM_LOBBY: isize = 26;
const FORM_AVAILABLE_ROOMS: isize = 31;
const FORM_AVAILABLE_GAMES: isize = 32;
/// Control 8 of the lobby form (the chat) and of the game room form (the player list).
const CONTROL_8: isize = 8;
const AR_TOTAL_GAMES_NUMBER: isize = 14;
const AR_SERVERS_NAME_LABEL: isize = 16;
const IDS_SETUP_PLAYERS: isize = 7206;
const ERROR_OK: isize = 0;

/// `Virtual4`'s function statics.
struct State {
    /// `sSelectedGame`: the games box's selected name when a refill began.
    selected_game: Vec<u8>,
    /// `sGameCount`: the lines of the refill so far.
    game_count: c_ulong,
    /// `sGameSelection`: the line of the refill with the selected name.
    game_selection: c_ulong,
    /// `sGameDisplayLine`: the games box's first line shown when a refill began.
    game_display_line: c_ulong,
    /// `sListDisplayLine`: the player list's.
    list_display_line: c_ulong,
}

static STATE: Mutex<State> = Mutex::new(State {
    selected_game: Vec::new(),
    game_count: 0,
    game_selection: 0,
    game_display_line: 0,
    list_display_line: 0,
});

/// The action: `EEAWonNet`'s members.
pub struct EeWonAction {
    /// `mList` (a `UIListBox<UWideString>*`; type 4 finds its own).
    list: isize,
    /// `mChat` (a `UITextLabel*`; type 7 finds its own).
    chat: isize,
    /// `mType`.
    ty: c_long,
    /// `mPrefix` (a chat line's speaker).
    prefix: Vec<u8>,
    /// `mRoomName` (type 2; no constructor sets it).
    room_name: Vec<u8>,
    /// `mServerName` (type 8; no constructor sets it).
    server_name: Vec<u8>,
    /// `mText` ("@@@@" and "@@@" bracket a refill).
    text: Vec<u8>,
    /// `mHumans`.
    humans: c_long,
    /// `mPlayers`.
    players: c_long,
    /// `mL8C` (types 2 and 8: the games).
    l8c: c_long,
    /// `mHasPassword`.
    has_password: bool,
    /// `mIsFromMe`.
    is_from_me: bool,
    /// `mIsInProgress`.
    is_in_progress: bool,
    /// `mIsPrivate`.
    is_private: bool,
}

/// A text with its NUL.
fn cz(bytes: &[u8]) -> Vec<u8> {
    let mut v = bytes.to_vec();
    v.push(0);
    v
}

impl EeWonAction {
    fn blank(ty: c_long, text: &[u8]) -> EeWonAction {
        EeWonAction {
            list: 0,
            chat: 0,
            ty,
            prefix: Vec::new(),
            room_name: Vec::new(),
            server_name: Vec::new(),
            text: text.to_vec(),
            humans: 0,
            players: 0,
            l8c: 0,
            has_password: false,
            is_from_me: false,
            is_in_progress: false,
            is_private: false,
        }
    }

    /// `EEAWonNet(inType, inList, inText, inHasPassword)`: a list box line (the game room's player list).
    pub fn new_list(ty: c_long, list: isize, text: &[u8], has_password: bool) -> EeWonAction {
        EeWonAction { list, has_password, ..Self::blank(ty, text) }
    }

    /// `EEAWonNet(inType, inText, inHumans, inPlayers, inHasPassword, inIsPrivate, inIsInProgress)`: a game line.
    pub fn new_game(
        ty: c_long,
        text: &[u8],
        humans: c_long,
        players: c_long,
        has_password: bool,
        is_private: bool,
        is_in_progress: bool,
    ) -> EeWonAction {
        EeWonAction { humans, players, has_password, is_private, is_in_progress, ..Self::blank(ty, text) }
    }

    /// `EEAWonNet(inType, inChat, inPrefix, inText, inIsFromMe)`: a chat line.
    pub fn new_chat(ty: c_long, chat: isize, prefix: &[u8], text: &[u8], is_from_me: bool) -> EeWonAction {
        EeWonAction { chat, prefix: prefix.to_vec(), is_from_me, ..Self::blank(ty, text) }
    }

    /// The action as a line of text (the drivers compare what `EEWonNet` posts).
    pub fn describe(&self) -> String {
        format!(
            "{} list={} chat={} prefix={} text={} humans={} players={} flags={}{}{}{}",
            self.ty,
            (self.list != 0) as u8,
            (self.chat != 0) as u8,
            String::from_utf8_lossy(&self.prefix),
            String::from_utf8_lossy(&self.text),
            self.humans,
            self.players,
            self.has_password as u8,
            self.is_from_me as u8,
            self.is_in_progress as u8,
            self.is_private as u8
        )
    }

    /// `EEAWonNet::Virtual4`: applies the update on the UI thread.
    ///
    /// # Safety
    /// `host` is the game's host (or a driver's) and the controls the action holds are live.
    pub unsafe fn run(&mut self, host: EeWonActionHost) {
        let h = |op: EeWonActionOp, a: isize, b: isize| host(op, a, b);
        use EeWonActionOp as Op;
        let mut st = STATE.lock().unwrap_or_else(|e| e.into_inner());
        match self.ty {
            8 => {
                if h(Op::CurrentFormIs, FORM_AVAILABLE_ROOMS, 0) != 0 {
                    let t = h(Op::TextNew, 0, 0);
                    h(Op::ResString, IDS_SETUP_PLAYERS, t);
                    // sprintf(theLabel, "%ld  (%ld ", mL8C, mPlayers), then the format and ")"
                    let mut label = format!("{}  ({} ", self.l8c, self.players).into_bytes();
                    label.extend_from_slice(CStr::from_ptr(h(Op::TextBytes, t, 0) as *const c_char).to_bytes());
                    h(Op::TextDelete, t, 0);
                    label.push(b')');
                    let label = cz(&label);
                    let server = cz(&self.server_name);
                    h(
                        Op::LabelSetText,
                        h(Op::FormControl, FORM_AVAILABLE_ROOMS, AR_TOTAL_GAMES_NUMBER),
                        label.as_ptr() as isize,
                    );
                    h(
                        Op::LabelSetText,
                        h(Op::FormControl, FORM_AVAILABLE_ROOMS, AR_SERVERS_NAME_LABEL),
                        server.as_ptr() as isize,
                    );
                }
            }
            6 | 7 => {
                if self.ty == 7 {
                    if h(Op::CurrentFormIs, FORM_LOBBY, 0) == 0 {
                        return;
                    }
                    self.chat = h(Op::CurrentControl, CONTROL_8, 0);
                }
                let chat = self.chat;
                // a reader scrolled back stays where they are, unless the line is their own
                let at_end = !(!self.is_from_me
                    && h(Op::LabelHasScrollBar, chat, 0) != 0
                    && h(Op::LabelSliderMax, chat, 0) != h(Op::LabelSliderValue, chat, 0));
                let mut label = self.prefix.clone();
                label.extend_from_slice(&self.text);
                let label = cz(&label);
                h(Op::LabelAppendText, chat, label.as_ptr() as isize);
                if at_end && h(Op::LabelHasScrollBar, chat, 0) != 0 {
                    h(Op::LabelSetLine, chat, h(Op::LabelSliderMax, chat, 0));
                }
            }
            4 => {
                self.list = if h(Op::CurrentFormIs, FORM_AVAILABLE_GAMES, 0) != 0 {
                    h(Op::CurrentControl, CONTROL_8, 0)
                } else {
                    0
                };
                let list = self.list;
                if list != 0 {
                    if self.text == b"@@@@" {
                        st.list_display_line = h(Op::ListDisplayLine, list, 0) as c_ulong;
                        h(Op::ListDeleteAll, list, 0);
                    } else if self.text == b"@@@" {
                        if h(Op::ListLineCount, list, 0) as c_ulong > st.list_display_line {
                            h(Op::ListSetLine, list, st.list_display_line as isize);
                        }
                    } else {
                        let text = cz(&self.text);
                        h(Op::ListAppend, list, text.as_ptr() as isize);
                    }
                }
            }
            3 => {
                if h(Op::CurrentFormIs, FORM_AVAILABLE_GAMES, 0) != 0 {
                    let games = h(Op::GamesBox, 0, 0);
                    if self.text == b"@@@@" {
                        if h(Op::GamesLineCount, games, 0) as c_ulong > 0 {
                            st.game_display_line = h(Op::GamesDisplayLine, games, 0) as c_ulong;
                            let mut selection: c_ulong = 0;
                            if h(Op::GamesSelection, games, &mut selection as *mut c_ulong as isize) == ERROR_OK {
                                let name = h(Op::GamesName, games, selection as isize) as *const c_char;
                                st.selected_game = CStr::from_ptr(name).to_bytes().to_vec();
                            } else {
                                st.selected_game.clear();
                            }
                            h(Op::GamesDeleteAll, games, 0);
                        } else {
                            st.game_display_line = 0;
                            st.selected_game.clear();
                        }
                        st.game_count = 0;
                    } else if self.text == b"@@@" {
                        if h(Op::GamesLineCount, games, 0) as c_ulong > st.game_display_line {
                            h(Op::GamesSetLine, games, st.game_display_line as isize);
                            h(Op::GamesSelect, games, st.game_selection as isize);
                        }
                        st.game_count = 0;
                        st.game_selection = 0;
                    } else {
                        let name = cz(&self.text);
                        let line = EeWonActionLine {
                            name: name.as_ptr().cast(),
                            description: c"".as_ptr(),
                            humans: self.humans,
                            players: self.players,
                            has_password: self.has_password,
                            is_private: self.is_private,
                            is_in_progress: self.is_in_progress,
                        };
                        h(Op::GamesAppend, games, &line as *const EeWonActionLine as isize);
                        if !st.selected_game.is_empty() {
                            if st.selected_game == self.text {
                                st.game_selection = st.game_count;
                            }
                        } else {
                            h(Op::GamesSelect, games, 0);
                        }
                        st.game_count = st.game_count.wrapping_add(1);
                    }
                }
            }
            2 if h(Op::CurrentFormIs, FORM_AVAILABLE_ROOMS, 0) != 0 => {
                let rooms = h(Op::RoomsBox, 0, 0);
                if self.room_name.is_empty() {
                    h(Op::RoomsDeleteAll, rooms, 0);
                } else {
                    let name = cz(&self.room_name);
                    let description = cz(&self.text);
                    let line = EeWonActionLine {
                        name: name.as_ptr().cast(),
                        description: description.as_ptr().cast(),
                        humans: self.humans,
                        players: self.l8c,
                        has_password: false,
                        is_private: false,
                        is_in_progress: false,
                    };
                    h(Op::RoomsAppend, rooms, &line as *const EeWonActionLine as isize);
                }
            }
            _ => {}
        }
    }
}

unsafe fn bytes<'a>(text: *const c_char) -> &'a [u8] {
    if text.is_null() {
        &[]
    } else {
        CStr::from_ptr(text).to_bytes()
    }
}

/// `EEAWonNet(inType, inList, inText, inHasPassword)`.
///
/// # Safety
/// `text` is NULL or NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_won_action_new_list(
    ty: c_long,
    list: isize,
    text: *const c_char,
    has_password: bool,
) -> *mut EeWonAction {
    Box::into_raw(Box::new(EeWonAction::new_list(ty, list, bytes(text), has_password)))
}

/// `EEAWonNet(inType, inText, inHumans, inPlayers, inHasPassword, inIsPrivate, inIsInProgress)`.
///
/// # Safety
/// `text` is NULL or NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_won_action_new_game(
    ty: c_long,
    text: *const c_char,
    humans: c_long,
    players: c_long,
    has_password: bool,
    is_private: bool,
    is_in_progress: bool,
) -> *mut EeWonAction {
    Box::into_raw(Box::new(EeWonAction::new_game(
        ty,
        bytes(text),
        humans,
        players,
        has_password,
        is_private,
        is_in_progress,
    )))
}

/// `EEAWonNet(inType, inChat, inPrefix, inText, inIsFromMe)`.
///
/// # Safety
/// The texts are NULL or NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_won_action_new_chat(
    ty: c_long,
    chat: isize,
    prefix: *const c_char,
    text: *const c_char,
    is_from_me: bool,
) -> *mut EeWonAction {
    Box::into_raw(Box::new(EeWonAction::new_chat(ty, chat, bytes(prefix), bytes(text), is_from_me)))
}

/// The members no constructor sets (the drivers' types 2 and 8): `mRoomName`, `mServerName`, `mL8C`, `mHumans`,
/// `mPlayers`.
///
/// # Safety
/// `action` is live; the texts are NULL or NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_won_action_set_room(
    action: *mut EeWonAction,
    room_name: *const c_char,
    server_name: *const c_char,
    l8c: c_long,
    humans: c_long,
    players: c_long,
) {
    let a = &mut *action;
    a.room_name = bytes(room_name).to_vec();
    a.server_name = bytes(server_name).to_vec();
    a.l8c = l8c;
    a.humans = humans;
    a.players = players;
}

/// `~EEAWonNet`.
///
/// # Safety
/// `action` came from a constructor above (or `Box::into_raw`) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_action_free(action: *mut EeWonAction) {
    if !action.is_null() {
        drop(Box::from_raw(action));
    }
}

/// `EEAWonNet::Virtual4`.
///
/// # Safety
/// As [`EeWonAction::run`].
#[no_mangle]
pub unsafe extern "C" fn ee_won_action_run(action: *mut EeWonAction, host: EeWonActionHost) {
    (*action).run(host);
}

/// `mChat` (the drivers name the label a chat line was given).
///
/// # Safety
/// `action` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_action_chat(action: *const EeWonAction) -> isize {
    (*action).chat
}

/// The action as text into `out` (at most `size` bytes with the NUL; the drivers' stand-in for the class logs it).
///
/// # Safety
/// `action` is live and `out` has `size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_action_describe(action: *const EeWonAction, out: *mut c_char, size: usize) {
    let text = (*action).describe();
    let n = text.len().min(size.saturating_sub(1));
    std::ptr::copy_nonoverlapping(text.as_ptr(), out.cast::<u8>(), n);
    *out.add(n) = 0;
}

/// `Virtual4`'s statics as text into `out`, then (with `reset`) as they are at program start: the driver compares
/// them with the original's and starts every sequence alike.
///
/// # Safety
/// `out` has `size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_action_statics(out: *mut c_char, size: usize, reset: bool) {
    let mut st = STATE.lock().unwrap_or_else(|e| e.into_inner());
    let text = format!(
        "selected={} count={} selection={} games_line={} list_line={}",
        String::from_utf8_lossy(&st.selected_game),
        st.game_count,
        st.game_selection,
        st.game_display_line,
        st.list_display_line
    );
    let n = text.len().min(size.saturating_sub(1));
    std::ptr::copy_nonoverlapping(text.as_ptr(), out.cast::<u8>(), n);
    *out.add(n) = 0;
    if reset {
        st.selected_game.clear();
        st.game_count = 0;
        st.game_selection = 0;
        st.game_display_line = 0;
        st.list_display_line = 0;
    }
}

#[cfg(test)]
#[path = "wonaction_tests.rs"]
mod tests;
