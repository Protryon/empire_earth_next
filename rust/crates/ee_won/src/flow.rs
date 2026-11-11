//! The lobby's game room flow and its start-up checks (StagingLogic.cpp's hosting, joining, leaving, ready and kick
//! flow; InitLogic.cpp's start-up checks): the decisions, over the state the C++ objects hold. The room messages
//! themselves are `staging`; the C++ keeps the game, the clients, the dialogs and their objects, the ops, the events,
//! the sounds and the drawing, and runs what these answer.

/// Where StagingLogic sends a room message: nowhere, handled by itself (the host), or handed to the network logic.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonStageRoute {
    Drop = 0,
    Local = 1,
    Net = 2,
    /// Handled locally and handed to the network logic too (the host's broadcast).
    LocalAndNet = 3,
}

/// `SendToCaptain`: the host handles its own, a joiner sends to the host.
pub fn send_to_captain(have_game: bool, i_am_captain: bool) -> EeWonStageRoute {
    match (have_game, i_am_captain) {
        (false, _) => EeWonStageRoute::Drop,
        (true, true) => EeWonStageRoute::Local,
        (true, false) => EeWonStageRoute::Net,
    }
}

/// `BroadcastToGame`: only the host, which handles it itself too.
pub fn broadcast(have_game: bool, i_am_captain: bool) -> EeWonStageRoute {
    if have_game && i_am_captain {
        EeWonStageRoute::LocalAndNet
    } else {
        EeWonStageRoute::Drop
    }
}

/// `SendToClient`: only the host.
pub fn send_to_client(have_game: bool, i_am_captain: bool) -> EeWonStageRoute {
    if have_game && i_am_captain {
        EeWonStageRoute::Net
    } else {
        EeWonStageRoute::Drop
    }
}

/// `HandlePlayerJoined` on a found client: a client without a player reads the announced one (0); one with a
/// player is announced again only by the host (1), and ignored elsewhere (2).
pub fn player_joined(has_player: bool, i_am_captain: bool) -> i32 {
    if !has_player {
        0
    } else if i_am_captain {
        1
    } else {
        2
    }
}

/// `HandlePlayerJoined`: a player who is not ready ends the remembered all-ready state.
pub fn joined_keeps_all_ready(last_all_ready: bool, ready: bool) -> bool {
    last_all_ready && ready
}

/// `SetGameInProgress`: only the host changes it, and only to another value.
pub fn sets_in_progress(have_game: bool, in_progress: bool, want: bool, i_am_captain: bool) -> bool {
    have_game && in_progress != want && i_am_captain
}

/// `ToggleReady`: the host tries to start the game (1); another player asks for the other ready flag (2, ready
/// in `want_ready`, when the game code allows readying: `CanReady` is asked only for ready); nothing without a
/// client or game (0).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonToggleReady {
    pub action: i32,
    pub want_ready: bool,
    pub ask_can_ready: bool,
}

pub fn toggle_ready(have_client: bool, have_game: bool, i_am_captain: bool, i_am_ready: bool) -> EeWonToggleReady {
    if !have_client || !have_game {
        return EeWonToggleReady::default();
    }
    if i_am_captain {
        return EeWonToggleReady { action: 1, want_ready: false, ask_can_ready: false };
    }
    let want = !i_am_ready;
    EeWonToggleReady { action: 2, want_ready: want, ask_can_ready: want }
}

/// `HostGame`'s warning before the create-game dialog: -1 no game screen (stop), 0 none, 1 the firewall test is
/// not done, 2 behind a firewall. LAN games and players who turned confirmations off get none.
pub fn host_warning(game_type: i32, show_confirmations: bool, firewall_test_done: bool, behind_firewall: bool) -> i32 {
    if game_type == 0 {
        return -1;
    }
    if game_type == crate::staging::GAME_LAN || !show_confirmations {
        return 0;
    }
    if !firewall_test_done {
        1
    } else if behind_firewall {
        2
    } else {
        0
    }
}

/// `VerifyPingBeforeJoin`: join (0); ping the Internet game and wait for its reply (1); ask whether to join
/// without one (2).
pub fn verify_ping(
    game_type: i32,
    ping_replied: bool,
    have_config: bool,
    allow_join_without_ping: bool,
    show_confirmations: bool,
) -> i32 {
    if game_type != crate::staging::GAME_INTERNET || ping_replied {
        0
    } else if have_config && !allow_join_without_ping {
        1
    } else if show_confirmations {
        2
    } else {
        0
    }
}

/// `JoinGame`: a join status shown to the player (not 0, joinable, nor 1, still joining or cancelled).
pub fn join_status_shown(status: i16) -> bool {
    status != 0 && status != 1
}

/// `LeaveGame`'s confirmation: none (0), dissolve the hosted game (1), leave the game (2).
pub fn leave_confirm(confirm: bool, show_confirmations: bool, i_am_captain: bool) -> i32 {
    if !confirm || !show_confirmations {
        0
    } else if i_am_captain {
        1
    } else {
        2
    }
}

/// `HandleKicked`: the moderation event's kind (0 a ban, 1 a kick) and room flags (6 an Internet game, 10 the
/// others), and the message when it is us (0 none, 1 banned, 2 kicked).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonKicked {
    pub kind: i32,
    pub flags: i32,
    pub message: i32,
}

pub fn kicked(ban: bool, game_type: i32, is_me: bool) -> EeWonKicked {
    EeWonKicked {
        kind: i32::from(!ban),
        flags: if game_type == crate::staging::GAME_INTERNET { 6 } else { 10 },
        message: match (is_me, ban) {
            (false, _) => 0,
            (true, true) => 1,
            (true, false) => 2,
        },
    }
}

/// `HandleGameAction`: the action run on a listed game: 7 options, 1 join, 2 ping, 3 details (3 and 4), 5 hide
/// the details, 6 edit a direct-connect entry (direct-connect games only); 0 nothing.
pub fn game_action(action: i32, game_type: i32) -> i32 {
    match action {
        7 | 1 | 2 | 5 => action,
        3 | 4 => 3,
        6 if game_type == crate::staging::GAME_DIRECT => 6,
        _ => 0,
    }
}

/// `NotifyPingChange`: a joiner reports its ping to the host of the game it is in.
pub fn reports_ping(have_game: bool, i_am_captain: bool, same_game: bool) -> bool {
    have_game && !i_am_captain && same_game
}

/// `KickClient`: only the host kicks.
pub fn kicks(have_client: bool, have_game: bool, i_am_captain: bool) -> bool {
    have_client && have_game && i_am_captain
}

/// `HandleClientLeft`: a departed player of the current game is removed (and the host advertises again).
pub fn client_left(has_player: bool, have_game: bool) -> bool {
    has_player && have_game
}

// ---- InitLogic

/// `SetError`: the errors not ignored, and whether the player may ignore them (only no-Internet, 1, and
/// account-server, 4, errors).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonInitErrors {
    pub errors: i32,
    pub allow_ignore: bool,
}

pub fn init_errors(error: i32, ignored: i32) -> EeWonInitErrors {
    let errors = error & !ignored;
    EeWonInitErrors { errors, allow_ignore: errors & 5 == errors }
}

/// `Start`: the no-Internet error, when the config asks to check.
pub fn no_internet(check_for_internet: bool, has_connection: bool) -> bool {
    check_for_internet && !has_connection
}

/// `DirectoryCompletion`: which server list a directory request's answer fills: 1 the auth servers (only when the
/// config names none), 2 the account servers, 3 the firewall servers, 4 the version servers, 0 none.
pub fn dir_request_target(index: i32, config_has_auth_servers: bool) -> i32 {
    match index {
        0 if !config_has_auth_servers => 1,
        1 => 2,
        2 => 3,
        3 => 4,
        _ => 0,
    }
}

/// `DirectoryCompletion`: the directory answered (fully or partly).
pub fn dir_answered(status: i32) -> bool {
    status == 0 || status == WS_DIR_MULTI_GET_PARTIAL
}

/// `DirectoryCompletion`: the missing server lists' error bits (8 auth, 4 account, 0x10 version), whose names
/// the C++ joins with ", " in that order.
pub fn missing_servers(auth: usize, account: usize, versions: usize) -> i32 {
    let mut errors = 0;
    if auth == 0 {
        errors = 8;
    }
    if account == 0 {
        errors |= 4;
    }
    if versions == 0 {
        errors |= 0x10;
    }
    errors
}

pub(crate) const WS_DIR_MULTI_GET_PARTIAL: i32 = -1223;
pub(crate) const WS_DB_VALID_NOT_LATEST: i32 = -2816;
pub(crate) const WS_DB_OUT_OF_DATE_NO_UPDATE: i32 = -2815;
pub(crate) const WS_DB_OUT_OF_DATE: i32 = -2814;
pub(crate) const WS_DB_ERROR: i32 = -2800;

/// `VersionCompletion`: the version is usable (0); not, with the version server marked failed (1); not (2).
pub fn version_outcome(status: i32) -> i32 {
    match status {
        0 | WS_DB_VALID_NOT_LATEST | WS_DB_OUT_OF_DATE => 0,
        WS_DB_OUT_OF_DATE_NO_UPDATE | WS_DB_ERROR => 1,
        _ => 2,
    }
}

/// `AcceptCompletion`: no connection means behind a firewall (0); one from the firewall server means not (1);
/// one from elsewhere waits for another (2).
pub fn accept_outcome(accepted: bool, from_firewall_server: bool) -> i32 {
    if !accepted {
        0
    } else if from_firewall_server {
        1
    } else {
        2
    }
}

/// `HandleTOU`: a downloaded terms-of-use document is saved when it came (status 0) and is newer than the saved
/// one; its bytes are written without the last (the terminator) when there are more than one.
pub fn tou_saved(doc_status: i32, modified: i32, saved: i32) -> bool {
    doc_status == 0 && modified > saved
}

/// (one byte, or none, writes nothing: the original's `length() > 1`)
pub fn document_bytes(length: usize) -> usize {
    length.saturating_sub(1)
}

/// `HandleComponentEvent`: 100045 retries, 100025 ignores the current errors and retries; 0 for anything else.
pub fn init_event(kind: i32) -> i32 {
    match kind {
        100045 => 1,
        100025 => 2,
        _ => 0,
    }
}

/// `ParseCrossPromotion`: the advert document's `link` and `image` lines, read as the original's `strtok` read
/// them: words split at blanks, tabs, newlines and '=', a `link` or `image` word (any case) takes the rest of its
/// line (the next run up to a newline, so "link = x" gives "= x"); the document ends at its first NUL. A later line
/// replaces an earlier one; a keyword with nothing after it ends the parse.
pub fn cross_promotion(doc: &[u8]) -> (Vec<u8>, Vec<u8>) {
    let end = doc.iter().position(|&b| b == 0).unwrap_or(doc.len());
    let mut t = Strtok { s: &doc[..end], pos: 0 };
    let mut link = Vec::new();
    let mut image = Vec::new();
    const WORDS: &[u8] = b" \t\n=";
    let mut token = t.next(WORDS);
    while let Some(word) = token {
        let target = if word.eq_ignore_ascii_case(b"link") {
            Some(&mut link)
        } else if word.eq_ignore_ascii_case(b"image") {
            Some(&mut image)
        } else {
            None
        };
        if let Some(target) = target {
            match t.next(b"\n") {
                None => break,
                Some(line) => *target = line.to_vec(),
            }
        }
        token = t.next(WORDS);
    }
    (link, image)
}

/// The CRT's `strtok` over a buffer: skip the delimiters, take up to the next one, step past it.
struct Strtok<'a> {
    s: &'a [u8],
    pos: usize,
}

impl<'a> Strtok<'a> {
    fn next(&mut self, delims: &[u8]) -> Option<&'a [u8]> {
        let s = self.s;
        let mut i = self.pos;
        while i < s.len() && delims.contains(&s[i]) {
            i += 1;
        }
        if i >= s.len() {
            self.pos = s.len();
            return None;
        }
        let start = i;
        while i < s.len() && !delims.contains(&s[i]) {
            i += 1;
        }
        self.pos = if i < s.len() { i + 1 } else { i };
        Some(&s[start..i])
    }
}

// ---- the C ABI

#[no_mangle]
pub extern "C" fn ee_won_stage_send_to_captain(have_game: bool, i_am_captain: bool) -> EeWonStageRoute {
    send_to_captain(have_game, i_am_captain)
}

#[no_mangle]
pub extern "C" fn ee_won_stage_broadcast(have_game: bool, i_am_captain: bool) -> EeWonStageRoute {
    broadcast(have_game, i_am_captain)
}

#[no_mangle]
pub extern "C" fn ee_won_stage_send_to_client(have_game: bool, i_am_captain: bool) -> EeWonStageRoute {
    send_to_client(have_game, i_am_captain)
}

#[no_mangle]
pub extern "C" fn ee_won_stage_player_joined(has_player: bool, i_am_captain: bool) -> i32 {
    player_joined(has_player, i_am_captain)
}

#[no_mangle]
pub extern "C" fn ee_won_stage_joined_keeps_all_ready(last_all_ready: bool, ready: bool) -> bool {
    joined_keeps_all_ready(last_all_ready, ready)
}

#[no_mangle]
pub extern "C" fn ee_won_stage_sets_in_progress(
    have_game: bool,
    in_progress: bool,
    want: bool,
    i_am_captain: bool,
) -> bool {
    sets_in_progress(have_game, in_progress, want, i_am_captain)
}

#[no_mangle]
pub extern "C" fn ee_won_stage_toggle_ready(
    have_client: bool,
    have_game: bool,
    i_am_captain: bool,
    i_am_ready: bool,
) -> EeWonToggleReady {
    toggle_ready(have_client, have_game, i_am_captain, i_am_ready)
}

#[no_mangle]
pub extern "C" fn ee_won_stage_host_warning(
    game_type: i32,
    show_confirmations: bool,
    firewall_test_done: bool,
    behind_firewall: bool,
) -> i32 {
    host_warning(game_type, show_confirmations, firewall_test_done, behind_firewall)
}

#[no_mangle]
pub extern "C" fn ee_won_stage_verify_ping(
    game_type: i32,
    ping_replied: bool,
    have_config: bool,
    allow_join_without_ping: bool,
    show_confirmations: bool,
) -> i32 {
    verify_ping(game_type, ping_replied, have_config, allow_join_without_ping, show_confirmations)
}

#[no_mangle]
pub extern "C" fn ee_won_stage_join_status_shown(status: i16) -> bool {
    join_status_shown(status)
}

#[no_mangle]
pub extern "C" fn ee_won_stage_leave_confirm(confirm: bool, show_confirmations: bool, i_am_captain: bool) -> i32 {
    leave_confirm(confirm, show_confirmations, i_am_captain)
}

#[no_mangle]
pub extern "C" fn ee_won_stage_kicked(ban: bool, game_type: i32, is_me: bool) -> EeWonKicked {
    kicked(ban, game_type, is_me)
}

#[no_mangle]
pub extern "C" fn ee_won_stage_game_action(action: i32, game_type: i32) -> i32 {
    game_action(action, game_type)
}

#[no_mangle]
pub extern "C" fn ee_won_stage_reports_ping(have_game: bool, i_am_captain: bool, same_game: bool) -> bool {
    reports_ping(have_game, i_am_captain, same_game)
}

#[no_mangle]
pub extern "C" fn ee_won_stage_kicks(have_client: bool, have_game: bool, i_am_captain: bool) -> bool {
    kicks(have_client, have_game, i_am_captain)
}

#[no_mangle]
pub extern "C" fn ee_won_stage_client_left(has_player: bool, have_game: bool) -> bool {
    client_left(has_player, have_game)
}

#[no_mangle]
pub extern "C" fn ee_won_init_errors(error: i32, ignored: i32) -> EeWonInitErrors {
    init_errors(error, ignored)
}

#[no_mangle]
pub extern "C" fn ee_won_init_no_internet(check_for_internet: bool, has_connection: bool) -> bool {
    no_internet(check_for_internet, has_connection)
}

#[no_mangle]
pub extern "C" fn ee_won_init_dir_request_target(index: i32, config_has_auth_servers: bool) -> i32 {
    dir_request_target(index, config_has_auth_servers)
}

#[no_mangle]
pub extern "C" fn ee_won_init_dir_answered(status: i32) -> bool {
    dir_answered(status)
}

#[no_mangle]
pub extern "C" fn ee_won_init_missing_servers(auth: usize, account: usize, versions: usize) -> i32 {
    missing_servers(auth, account, versions)
}

#[no_mangle]
pub extern "C" fn ee_won_init_version_outcome(status: i32) -> i32 {
    version_outcome(status)
}

#[no_mangle]
pub extern "C" fn ee_won_init_accept_outcome(accepted: bool, from_firewall_server: bool) -> i32 {
    accept_outcome(accepted, from_firewall_server)
}

#[no_mangle]
pub extern "C" fn ee_won_init_tou_saved(doc_status: i32, modified: i32, saved: i32) -> bool {
    tou_saved(doc_status, modified, saved)
}

#[no_mangle]
pub extern "C" fn ee_won_init_document_bytes(length: usize) -> usize {
    document_bytes(length)
}

#[no_mangle]
pub extern "C" fn ee_won_init_event(kind: i32) -> i32 {
    init_event(kind)
}

/// `cross_promotion`: the link, then the image, each through the sink (each given once, maybe empty).
///
/// # Safety
/// `doc` holds `length` bytes (or is NULL); the sink's contract.
#[no_mangle]
pub unsafe extern "C" fn ee_won_init_cross_promotion(
    doc: *const u8,
    length: usize,
    link: crate::ffi::EeWonByteSink,
    image: crate::ffi::EeWonByteSink,
    context: *mut std::ffi::c_void,
) {
    let d = if doc.is_null() || length == 0 {
        &[][..]
    } else {
        // SAFETY: `length` bytes (the contract)
        unsafe { std::slice::from_raw_parts(doc, length) }
    };
    let (l, i) = cross_promotion(d);
    unsafe {
        crate::ffi::give(link, context, &l);
        crate::ffi::give(image, context, &i);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn routes() {
        assert_eq!(send_to_captain(false, true), EeWonStageRoute::Drop);
        assert_eq!(send_to_captain(true, true), EeWonStageRoute::Local);
        assert_eq!(send_to_captain(true, false), EeWonStageRoute::Net);
        assert_eq!(broadcast(true, true), EeWonStageRoute::LocalAndNet);
        assert_eq!(broadcast(true, false), EeWonStageRoute::Drop);
        assert_eq!(send_to_client(true, true), EeWonStageRoute::Net);
        assert_eq!(send_to_client(false, true), EeWonStageRoute::Drop);
    }

    #[test]
    fn the_room() {
        assert_eq!((player_joined(false, true), player_joined(true, true), player_joined(true, false)), (0, 1, 2));
        assert!(joined_keeps_all_ready(true, true) && !joined_keeps_all_ready(true, false));
        assert!(!joined_keeps_all_ready(false, true));
        assert!(sets_in_progress(true, false, true, true) && !sets_in_progress(true, true, true, true));
        assert!(!sets_in_progress(true, false, true, false) && !sets_in_progress(false, false, true, true));
        assert_eq!(toggle_ready(false, true, false, false), EeWonToggleReady::default());
        assert_eq!(toggle_ready(true, true, true, false).action, 1);
        assert_eq!(
            toggle_ready(true, true, false, false),
            EeWonToggleReady { action: 2, want_ready: true, ask_can_ready: true }
        );
        assert_eq!(
            toggle_ready(true, true, false, true),
            EeWonToggleReady { action: 2, want_ready: false, ask_can_ready: false }
        );
        assert_eq!(host_warning(0, true, false, false), -1);
        assert_eq!(host_warning(1, true, false, true), 0);
        assert_eq!(host_warning(3, false, false, true), 0);
        assert_eq!(host_warning(2, true, false, true), 1);
        assert_eq!(host_warning(3, true, true, true), 2);
        assert_eq!(host_warning(3, true, true, false), 0);
        assert_eq!(verify_ping(1, false, true, false, true), 0);
        assert_eq!(verify_ping(2, true, true, false, true), 0);
        assert_eq!(verify_ping(2, false, true, false, true), 1);
        assert_eq!(verify_ping(2, false, true, true, true), 2);
        assert_eq!(verify_ping(2, false, false, false, true), 2);
        assert_eq!(verify_ping(2, false, false, false, false), 0);
        assert!(!join_status_shown(0) && !join_status_shown(1) && join_status_shown(2) && join_status_shown(-1));
        assert_eq!(
            (leave_confirm(false, true, true), leave_confirm(true, true, true), leave_confirm(true, true, false)),
            (0, 1, 2)
        );
        assert_eq!(leave_confirm(true, false, true), 0);
        assert_eq!(kicked(true, 2, true), EeWonKicked { kind: 0, flags: 6, message: 1 });
        assert_eq!(kicked(false, 1, true), EeWonKicked { kind: 1, flags: 10, message: 2 });
        assert_eq!(kicked(false, 3, false).message, 0);
        assert_eq!((game_action(4, 1), game_action(6, 1), game_action(6, 3), game_action(8, 3)), (3, 0, 6, 0));
        assert!(
            reports_ping(true, false, true) && !reports_ping(true, true, true) && !reports_ping(true, false, false)
        );
        assert!(kicks(true, true, true) && !kicks(true, true, false));
        assert!(client_left(true, true) && !client_left(false, true));
    }

    #[test]
    fn start_up() {
        assert_eq!(init_errors(5, 0), EeWonInitErrors { errors: 5, allow_ignore: true });
        assert_eq!(init_errors(0x22, 0), EeWonInitErrors { errors: 0x22, allow_ignore: false });
        assert_eq!(init_errors(0x22, 0x20), EeWonInitErrors { errors: 2, allow_ignore: false });
        assert_eq!(init_errors(4, 4).errors, 0);
        assert!(no_internet(true, false) && !no_internet(false, false) && !no_internet(true, true));
        assert_eq!((dir_request_target(0, false), dir_request_target(0, true), dir_request_target(3, true)), (1, 0, 4));
        assert_eq!(dir_request_target(4, false), 0);
        assert!(dir_answered(0) && dir_answered(-1223) && !dir_answered(-1));
        assert_eq!(missing_servers(0, 0, 0), 0x1c);
        assert_eq!(missing_servers(1, 0, 1), 4);
        assert_eq!(version_outcome(0), 0);
        assert_eq!(version_outcome(-2814), 0);
        assert_eq!(version_outcome(-2800), 1);
        assert_eq!(version_outcome(-5), 2);
        assert_eq!((accept_outcome(false, true), accept_outcome(true, true), accept_outcome(true, false)), (0, 1, 2));
        assert!(tou_saved(0, 5, 4) && !tou_saved(0, 4, 4) && !tou_saved(1, 5, 4));
        assert_eq!((document_bytes(0), document_bytes(1), document_bytes(5)), (0, 0, 4));
        assert_eq!((init_event(100045), init_event(100025), init_event(1)), (1, 2, 0));
    }

    #[test]
    fn the_advert() {
        let (l, i) = cross_promotion(b"link http://a/b\nimage=pic.png\n");
        assert_eq!((l.as_slice(), i.as_slice()), (&b"http://a/b"[..], &b"pic.png"[..]));
        let (l, _) = cross_promotion(b"LINK = x y\n");
        assert_eq!(l, b"= x y");
        let (l, i) = cross_promotion(b"image a\nlink");
        assert_eq!((l.as_slice(), i.as_slice()), (&b""[..], &b"a"[..]));
        let (l, _) = cross_promotion(b"link a\0link b\n");
        assert_eq!(l, b"a");
        let (l, i) = cross_promotion(b"foo link\nimage\n\nz\n");
        assert_eq!((l.as_slice(), i.as_slice()), (&b"image"[..], &b""[..]));
        assert_eq!(cross_promotion(b""), (vec![], vec![]));
    }
}
