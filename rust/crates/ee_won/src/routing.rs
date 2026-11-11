//! RoutingLogic's decisions (RoutingLogic.cpp): the Internet lobby's logic over a routing server. The LAN path
//! meets it only through the lobby's shared events, which it answers with its guards (no current server: nothing);
//! everything else here runs only with a routing server, and the servers are gone. The C++ keeps the servers, the
//! rooms, the connection, the ops, the dialogs and the events; these answer:
//!
//! - the room flags of a group (`GetRoomSpec`) and the joined room a flag set names (`GetGroup`);
//! - Quick Play's pick (`SelectGame`): the ping ceiling, each game's rank, which unknown pings to start;
//! - whether a friend needs a server search (`ShouldLocateFriend`);
//! - the default server (`GetDefaultServer`);
//! - the client flags the anonymous and dirty-word settings send; the friends list's limit.

/// `GetRoomSpec(id)`: 5 the joined chat room, 6 the joined game room, 0 another.
pub fn room_spec(id: u16, chat_group: Option<u16>, game_group: Option<u16>) -> i32 {
    if chat_group == Some(id) {
        5
    } else if game_group == Some(id) {
        6
    } else {
        0
    }
}

/// `GetGroup(spec)`: 1 the chat room (bit 1), 2 the game room (bit 2), 0 none.
pub fn group_of_spec(spec: i32) -> i32 {
    if spec & 1 != 0 {
        1
    } else if spec & 2 != 0 {
        2
    } else {
        0
    }
}

/// `SelectGame`'s ping ceiling: the config's (0 or above 0xf000: 0xf000), 10000 without a config.
pub fn ping_ceiling(have_config: bool, config_ceiling: u16) -> u16 {
    if !have_config {
        10000
    } else if config_ceiling == 0 || config_ceiling > 0xf000 {
        0xf000
    } else {
        config_ceiling
    }
}

/// `SelectGame` over one game: skipped (password, ask to join, not joinable); ranked (`Some(rank)`: the skill
/// band, 0 the wanted skill, 10000 no skill, 20000 one below, 30000 one above, plus the ping, when the ping is
/// under the ceiling and the limit); or, with an unknown ping (0xfffe) at or over the ceiling, pinged.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonQuickPlay {
    /// 0 nothing, 1 ranked (`rank`), 2 ping it.
    pub action: i32,
    pub rank: i32,
}

#[allow(clippy::too_many_arguments)]
pub fn quick_play_rank(
    password: bool,
    ask_to_join: bool,
    can_join: bool,
    ping: u16,
    skill: i32,
    wanted: i32,
    ping_limit: u16,
    ceiling: u16,
) -> EeWonQuickPlay {
    let none = EeWonQuickPlay::default();
    if password || ask_to_join || !can_join {
        return none;
    }
    if ping < ceiling {
        let band = if skill == wanted {
            0
        } else if skill == 0 {
            10000
        } else if skill.wrapping_add(1) == wanted {
            20000
        } else if skill.wrapping_sub(1) == wanted {
            30000
        } else {
            return none;
        };
        if ping < ping_limit {
            EeWonQuickPlay { action: 1, rank: band + i32::from(ping) }
        } else {
            none
        }
    } else if ping == 0xfffe {
        EeWonQuickPlay { action: 2, rank: 0 }
    } else {
        none
    }
}

/// `ShouldLocateFriend`: not being located, not on this server, other servers to search, 30 seconds since the
/// last search (the difference of two `unsigned long`s as a `long`).
pub fn should_locate_friend(
    locating: bool,
    on_current_server: bool,
    server_count: usize,
    connected: bool,
    now: u32,
    last_locate: u32,
) -> bool {
    if locating || on_current_server || server_count == 0 {
        return false;
    }
    if server_count == 1 && connected {
        return false;
    }
    (now.wrapping_sub(last_locate) as i32) >= 30
}

/// A server as `GetDefaultServer` sees it.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonServerPick {
    /// Its name is the last server's.
    pub last_used: bool,
    pub clients: i32,
    /// It lists the lobby's language.
    pub language: bool,
}

/// `GetDefaultServer` over the servers in map order: the first last-used one, else the busiest listing the
/// language, else the busiest (the first of equals); `None` for none.
pub fn default_server(servers: &[EeWonServerPick]) -> Option<usize> {
    let mut best: Option<usize> = None;
    let mut preferred: Option<usize> = None;
    for (i, s) in servers.iter().enumerate() {
        if s.last_used {
            return Some(i);
        }
        if best.is_none_or(|b| s.clients > servers[b].clients) {
            best = Some(i);
        }
        if s.language && preferred.is_none_or(|p| s.clients > servers[p].clients) {
            preferred = Some(i);
        }
    }
    preferred.or(best)
}

/// The client flags `HandleAnonymousModeChange` (0x40, set when anonymous) and `HandleDirtyWordFilterChange` (4,
/// set when the filter is off) send: (mask, flags).
pub fn anonymous_flags(anonymous: bool) -> (u32, u32) {
    (0x40, if anonymous { 0x40 } else { 0 })
}

pub fn dirty_word_flags(filter: bool) -> (u32, u32) {
    (4, if filter { 0 } else { 4 })
}

/// `CanAddFriend`: the friends list holds at most 255.
pub fn can_add_friend(count: usize) -> bool {
    count < 255
}

// ---- the C ABI

/// `room_spec`; a negative group is none.
#[no_mangle]
pub extern "C" fn ee_won_routing_room_spec(id: u16, chat_group: i32, game_group: i32) -> i32 {
    let g = |v: i32| u16::try_from(v).ok();
    room_spec(id, g(chat_group), g(game_group))
}

#[no_mangle]
pub extern "C" fn ee_won_routing_group_of_spec(spec: i32) -> i32 {
    group_of_spec(spec)
}

#[no_mangle]
pub extern "C" fn ee_won_routing_ping_ceiling(have_config: bool, config_ceiling: u16) -> u16 {
    ping_ceiling(have_config, config_ceiling)
}

#[allow(clippy::too_many_arguments)]
#[no_mangle]
pub extern "C" fn ee_won_routing_quick_play_rank(
    password: bool,
    ask_to_join: bool,
    can_join: bool,
    ping: u16,
    skill: i32,
    wanted: i32,
    ping_limit: u16,
    ceiling: u16,
) -> EeWonQuickPlay {
    quick_play_rank(password, ask_to_join, can_join, ping, skill, wanted, ping_limit, ceiling)
}

#[no_mangle]
pub extern "C" fn ee_won_routing_should_locate_friend(
    locating: bool,
    on_current_server: bool,
    server_count: usize,
    connected: bool,
    now: u32,
    last_locate: u32,
) -> bool {
    should_locate_friend(locating, on_current_server, server_count, connected, now, last_locate)
}

/// `default_server` over `count` servers: the index, or -1.
///
/// # Safety
/// `servers` holds `count` entries (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_routing_default_server(servers: *const EeWonServerPick, count: usize) -> i64 {
    let s = if servers.is_null() || count == 0 {
        &[][..]
    } else {
        // SAFETY: `count` entries (the contract)
        unsafe { std::slice::from_raw_parts(servers, count) }
    };
    default_server(s).map_or(-1, |i| i as i64)
}

/// The (mask, flags) of `anonymous_flags` (`which` 0) or `dirty_word_flags` (1) into the outs.
///
/// # Safety
/// Both outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routing_client_flags(which: i32, on: bool, out_mask: *mut u32, out_flags: *mut u32) {
    let (m, f) = if which == 0 { anonymous_flags(on) } else { dirty_word_flags(on) };
    // SAFETY: writable (the contract)
    unsafe {
        *out_mask = m;
        *out_flags = f;
    }
}

#[no_mangle]
pub extern "C" fn ee_won_routing_can_add_friend(count: usize) -> bool {
    can_add_friend(count)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rooms() {
        assert_eq!(room_spec(3, Some(3), Some(3)), 5);
        assert_eq!(room_spec(4, Some(3), Some(4)), 6);
        assert_eq!(room_spec(4, None, None), 0);
        assert_eq!((group_of_spec(5), group_of_spec(6), group_of_spec(8)), (1, 2, 0));
        assert_eq!(ee_won_routing_room_spec(0xffff, -1, 0xffff), 6);
    }

    #[test]
    fn quick_play() {
        assert_eq!(
            (ping_ceiling(false, 7), ping_ceiling(true, 0), ping_ceiling(true, 0xf001), ping_ceiling(true, 9)),
            (10000, 0xf000, 0xf000, 9)
        );
        let q = |skill, ping| quick_play_rank(false, false, true, ping, skill, 2, 1000, 0xf000);
        assert_eq!(q(2, 40), EeWonQuickPlay { action: 1, rank: 40 });
        assert_eq!(q(0, 40).rank, 10040);
        assert_eq!(q(1, 40).rank, 20040);
        assert_eq!(q(3, 40).rank, 30040);
        assert_eq!(q(5, 40), EeWonQuickPlay::default());
        assert_eq!(q(2, 1000), EeWonQuickPlay::default());
        assert_eq!(q(2, 0xfffe), EeWonQuickPlay { action: 2, rank: 0 });
        assert_eq!(q(2, 0xfffd), EeWonQuickPlay::default());
        assert_eq!(quick_play_rank(true, false, true, 1, 2, 2, 1000, 0xf000), EeWonQuickPlay::default());
    }

    #[test]
    fn friends_and_servers() {
        assert!(should_locate_friend(false, false, 2, true, 130, 100));
        assert!(!should_locate_friend(false, false, 2, true, 129, 100));
        assert!(!should_locate_friend(true, false, 2, true, 130, 100));
        assert!(!should_locate_friend(false, true, 2, true, 130, 100));
        assert!(!should_locate_friend(false, false, 1, true, 130, 100));
        assert!(should_locate_friend(false, false, 1, false, 130, 100));
        assert!(!should_locate_friend(false, false, 0, false, 130, 100));
        assert!(!should_locate_friend(false, false, 2, false, 10, 100));
        let s = |last_used, clients, language| EeWonServerPick { last_used, clients, language };
        assert_eq!(default_server(&[]), None);
        assert_eq!(default_server(&[s(false, 3, false), s(false, 5, false)]), Some(1));
        assert_eq!(default_server(&[s(false, 3, true), s(false, 5, false)]), Some(0));
        assert_eq!(default_server(&[s(false, 3, false), s(true, 0, false)]), Some(1));
        assert_eq!(default_server(&[s(false, 5, false), s(false, 5, false)]), Some(0));
        assert_eq!(
            (anonymous_flags(true), dirty_word_flags(true), dirty_word_flags(false)),
            ((0x40, 0x40), (4, 0), (4, 4))
        );
        assert!(can_add_friend(254) && !can_add_friend(255));
    }
}
