//! The lobby's saved settings' scalar state as a Rust object (round 16): `LobbyPersistentData`'s current user, its
//! flags (the chat filter, auto-away, cross promotion, the lobby's sound and music and their saved states, the chat
//! and game-room sounds, confirmations, auto-ping, games in progress, anonymity, the two dirty flags), the connection
//! speed, the LAN broadcast port, the network adapter and its four texts (the tab to open, the LAN name, the last
//! server and room), with the rules the lobby's `Lobby*` setters applied to them: a change marks the user's settings
//! changed, the lobby's sound and music mark the global ones changed unless back at their saved state, the restored
//! defaults, the adapter rule, the global save's saved states. The containers (users, friends, ignored names, options,
//! LAN names, direct connects, reconnect ids) and the password key stay with the C++ class, as do the files' layouts'
//! callers (persist.rs writes and reads the files). The texts are UTF-8 (D6; GUIString's narrow form), compared byte for
//! byte as GUIString's `Compare` compared its code units. The behaviour is `won_persist_test`'s object part, the
//! original's statements over a stand-in.

/// A flag of the saved settings (the C++ `LobbyPersistentData::Flag`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonPrefsFlag {
    FilterDirtyWords = 0,
    DoAutoAfk = 1,
    CrossPromotion = 2,
    LobbySoundEffects = 3,
    LobbyMusic = 4,
    ChatSoundEffects = 5,
    StagingSoundEffects = 6,
    ShowConfirmations = 7,
    AutoPingGames = 8,
    ShowGamesInProgress = 9,
    AnonymousToFriends = 10,
    SavedLobbySoundEffects = 11,
    SavedLobbyMusic = 12,
    GlobalDirty = 13,
    UserDirty = 14,
}
const FLAG_COUNT: usize = 15;

/// A text of the saved settings.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonPrefsText {
    TabSelect = 0,
    LanName = 1,
    LastServerName = 2,
    LastRoomName = 3,
}
const TEXT_COUNT: usize = 4;

/// A number of the saved settings.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonPrefsNumber {
    ConnectionSpeed = 0,
    LanBroadcastPort = 1,
}

/// `LobbyPersistentData`'s scalar state and its setters' rules.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct EeWonLobbyPrefs {
    flags: [bool; FLAG_COUNT],
    current_user: i64,
    connection_speed: u16,
    lan_port: u16,
    network_adapter: i64,
    texts: [Vec<u8>; TEXT_COUNT],
}

use EeWonPrefsFlag as F;

impl EeWonLobbyPrefs {
    /// The constructor's state before its `RestoreDefaults` (which the C++ runs next, with its texts): no user (the
    /// original left the id unset), the port 8585, clean, the sound and music saved on.
    pub fn new() -> EeWonLobbyPrefs {
        let mut p = EeWonLobbyPrefs {
            flags: [false; FLAG_COUNT],
            current_user: 0,
            connection_speed: 0,
            lan_port: 0,
            network_adapter: 0,
            texts: Default::default(),
        };
        p.restore_defaults(b"", b"");
        p.lan_port = 8585;
        p.set_flag(F::GlobalDirty, false);
        p.set_flag(F::UserDirty, false);
        p.set_flag(F::SavedLobbySoundEffects, true);
        p.set_flag(F::SavedLobbyMusic, true);
        p
    }

    pub fn flag(&self, f: EeWonPrefsFlag) -> bool {
        self.flags[f as usize]
    }
    pub fn set_flag(&mut self, f: EeWonPrefsFlag, on: bool) {
        self.flags[f as usize] = on;
    }
    /// The `LobbySet...` rule: a new value marks the user's settings changed; the lobby's sound and music also set
    /// the global dirty flag to whether they now differ from their saved state.
    pub fn change_flag(&mut self, f: EeWonPrefsFlag, on: bool) {
        if on == self.flag(f) {
            return;
        }
        self.set_flag(f, on);
        self.set_flag(F::UserDirty, true);
        let saved = match f {
            F::LobbySoundEffects => Some(F::SavedLobbySoundEffects),
            F::LobbyMusic => Some(F::SavedLobbyMusic),
            _ => None,
        };
        if let Some(s) = saved {
            self.set_flag(F::GlobalDirty, on != self.flag(s));
        }
    }

    pub fn current_user(&self) -> i64 {
        self.current_user
    }
    pub fn set_current_user(&mut self, id: i64) {
        self.current_user = id;
    }

    pub fn number(&self, n: EeWonPrefsNumber) -> u16 {
        match n {
            EeWonPrefsNumber::ConnectionSpeed => self.connection_speed,
            EeWonPrefsNumber::LanBroadcastPort => self.lan_port,
        }
    }
    pub fn set_number(&mut self, n: EeWonPrefsNumber, v: u16) {
        match n {
            EeWonPrefsNumber::ConnectionSpeed => self.connection_speed = v,
            EeWonPrefsNumber::LanBroadcastPort => self.lan_port = v,
        }
    }
    /// `LobbySetConnectionSpeed` / `LobbySetLanBroadcastPort`: a new value marks the user's settings changed.
    pub fn change_number(&mut self, n: EeWonPrefsNumber, v: u16) {
        if v != self.number(n) {
            self.set_number(n, v);
            self.set_flag(F::UserDirty, true);
        }
    }

    pub fn network_adapter(&self) -> i64 {
        self.network_adapter
    }
    pub fn set_network_adapter(&mut self, a: i64) {
        self.network_adapter = a;
    }
    /// `LobbySaveNetworkAdapter`: the adapter in use (the local host), marking the global settings changed.
    pub fn save_network_adapter(&mut self, local_host: i64) {
        if self.network_adapter != local_host {
            self.network_adapter = local_host;
            self.set_flag(F::GlobalDirty, true);
        }
    }

    pub fn text(&self, t: EeWonPrefsText) -> &[u8] {
        &self.texts[t as usize]
    }
    pub fn set_text(&mut self, t: EeWonPrefsText, s: &[u8]) {
        self.texts[t as usize] = s.to_vec();
    }
    /// `LobbySetTabSelect`: a new tab marks the user's settings changed.
    pub fn change_tab_select(&mut self, s: &[u8]) {
        if s != self.text(EeWonPrefsText::TabSelect) {
            self.set_text(EeWonPrefsText::TabSelect, s);
            self.set_flag(F::UserDirty, true);
        }
    }
    /// `LobbySetLastRoom`: a new server or room marks the user's settings changed.
    pub fn set_last_room(&mut self, server: &[u8], room: &[u8]) {
        if server == self.text(EeWonPrefsText::LastServerName) && room == self.text(EeWonPrefsText::LastRoomName) {
            return;
        }
        self.set_text(EeWonPrefsText::LastServerName, server);
        self.set_text(EeWonPrefsText::LastRoomName, room);
        self.set_flag(F::UserDirty, true);
    }

    /// `RestoreDefaults` (0x75c720): no last server or room, the Quick Play tab for a logged-in user or the LAN tab
    /// for the LAN user, every option on but anonymity, the port 8585, the speed 25, the default adapter. Returns
    /// whether the user is the LAN user (the C++ then sets the default LAN name, as the original did there).
    pub fn restore_defaults(&mut self, quick_play: &[u8], lan: &[u8]) -> bool {
        self.set_text(EeWonPrefsText::LastServerName, b"");
        self.set_text(EeWonPrefsText::LastRoomName, b"");
        let lan_user = self.current_user == 0;
        self.set_text(EeWonPrefsText::TabSelect, if lan_user { lan } else { quick_play });
        for f in [
            F::FilterDirtyWords,
            F::DoAutoAfk,
            F::CrossPromotion,
            F::LobbySoundEffects,
            F::LobbyMusic,
            F::ShowConfirmations,
            F::ChatSoundEffects,
            F::StagingSoundEffects,
            F::AutoPingGames,
            F::ShowGamesInProgress,
        ] {
            self.set_flag(f, true);
        }
        self.set_flag(F::AnonymousToFriends, false);
        self.lan_port = 8585;
        self.connection_speed = 25;
        self.network_adapter = 0;
        lan_user
    }
    /// `LobbyRestoreDefaults`' rule after the restore: the user's settings changed, the global ones when the sound,
    /// the music or the adapter (compared as the original's `int` copy) changed.
    pub fn after_restore(&mut self, old_adapter: i64) {
        self.set_flag(F::UserDirty, true);
        let old = old_adapter as i32 as i64;
        let dirty = self.flag(F::SavedLobbySoundEffects) != self.flag(F::LobbySoundEffects)
            || self.flag(F::SavedLobbyMusic) != self.flag(F::LobbyMusic)
            || self.network_adapter != old;
        self.set_flag(F::GlobalDirty, dirty);
    }
    /// The end of `SaveGlobalPrefs` (written or not): the saved sound and music are the current ones, clean.
    pub fn after_global_save(&mut self) {
        self.set_flag(F::SavedLobbySoundEffects, self.flag(F::LobbySoundEffects));
        self.set_flag(F::SavedLobbyMusic, self.flag(F::LobbyMusic));
        self.set_flag(F::GlobalDirty, false);
    }
}

impl EeWonLobbyPrefs {
    /// The end of a read user file (`LoadUserPrefs`): the global settings changed when the sound or music now
    /// differ from their saved state; the user's are clean.
    pub fn after_user_load(&mut self) {
        let dirty = self.flag(F::LobbySoundEffects) != self.flag(F::SavedLobbySoundEffects)
            || self.flag(F::SavedLobbyMusic) != self.flag(F::LobbyMusic);
        self.set_flag(F::GlobalDirty, dirty);
        self.set_flag(F::UserDirty, false);
    }
}

impl Default for EeWonLobbyPrefs {
    fn default() -> EeWonLobbyPrefs {
        EeWonLobbyPrefs::new()
    }
}

// ---- the C ABI

fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: the callers' contract: `n` readable bytes
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}
macro_rules! obj {
    ($p:expr) => {
        // SAFETY: the caller's live object
        unsafe { &*$p }
    };
}
macro_rules! obj_mut {
    ($p:expr) => {
        // SAFETY: the caller's live object, one caller at a time
        unsafe { &mut *$p }
    };
}

/// The constructor's state (see [`EeWonLobbyPrefs::new`]).
#[no_mangle]
pub extern "C" fn ee_won_lobbyprefs_new() -> *mut EeWonLobbyPrefs {
    Box::into_raw(Box::new(EeWonLobbyPrefs::new()))
}
/// Frees the object.
///
/// # Safety
/// `p` from `ee_won_lobbyprefs_new` (or null), not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_free(p: *mut EeWonLobbyPrefs) {
    if !p.is_null() {
        // SAFETY: the caller's object
        drop(unsafe { Box::from_raw(p) });
    }
}
/// A flag.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_flag(p: *const EeWonLobbyPrefs, f: EeWonPrefsFlag) -> bool {
    obj!(p).flag(f)
}
/// Sets a flag.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_set_flag(p: *mut EeWonLobbyPrefs, f: EeWonPrefsFlag, on: bool) {
    obj_mut!(p).set_flag(f, on)
}
/// The setters' rule (see [`EeWonLobbyPrefs::change_flag`]).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_change_flag(p: *mut EeWonLobbyPrefs, f: EeWonPrefsFlag, on: bool) {
    obj_mut!(p).change_flag(f, on)
}
/// The current user's id (0: the LAN user).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_current_user(p: *const EeWonLobbyPrefs) -> i64 {
    obj!(p).current_user()
}
/// Sets the current user's id.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_set_current_user(p: *mut EeWonLobbyPrefs, id: i64) {
    obj_mut!(p).set_current_user(id)
}
/// A number.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_number(p: *const EeWonLobbyPrefs, n: EeWonPrefsNumber) -> u16 {
    obj!(p).number(n)
}
/// Sets a number.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_set_number(p: *mut EeWonLobbyPrefs, n: EeWonPrefsNumber, v: u16) {
    obj_mut!(p).set_number(n, v)
}
/// The setters' rule for a number.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_change_number(p: *mut EeWonLobbyPrefs, n: EeWonPrefsNumber, v: u16) {
    obj_mut!(p).change_number(n, v)
}
/// The network adapter (a numeric IPv4 address, 0 for the default).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_network_adapter(p: *const EeWonLobbyPrefs) -> i64 {
    obj!(p).network_adapter()
}
/// Sets the network adapter.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_set_network_adapter(p: *mut EeWonLobbyPrefs, a: i64) {
    obj_mut!(p).set_network_adapter(a)
}
/// `LobbySaveNetworkAdapter`'s rule.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_save_network_adapter(p: *mut EeWonLobbyPrefs, local_host: i64) {
    obj_mut!(p).save_network_adapter(local_host)
}
/// A text's UTF-8 bytes (valid until the text changes), its length in `*length`.
///
/// # Safety
/// `p` is live, `length` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_text(
    p: *const EeWonLobbyPrefs,
    t: EeWonPrefsText,
    length: *mut usize,
) -> *const u8 {
    let s = obj!(p).text(t);
    // SAFETY: the caller's slot
    unsafe { *length = s.len() };
    s.as_ptr()
}
/// Sets a text.
///
/// # Safety
/// `p` is live; `s` null or `n` readable bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_set_text(
    p: *mut EeWonLobbyPrefs,
    t: EeWonPrefsText,
    s: *const u8,
    n: usize,
) {
    obj_mut!(p).set_text(t, bytes(s, n))
}
/// `LobbySetTabSelect`'s rule.
///
/// # Safety
/// As `ee_won_lobbyprefs_set_text`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_change_tab_select(p: *mut EeWonLobbyPrefs, s: *const u8, n: usize) {
    obj_mut!(p).change_tab_select(bytes(s, n))
}
/// `LobbySetLastRoom`'s rule.
///
/// # Safety
/// `p` is live; each text null or its length's readable bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_set_last_room(
    p: *mut EeWonLobbyPrefs,
    server: *const u8,
    server_length: usize,
    room: *const u8,
    room_length: usize,
) {
    obj_mut!(p).set_last_room(bytes(server, server_length), bytes(room, room_length))
}
/// `RestoreDefaults`; true for the LAN user (the caller then sets the default LAN name).
///
/// # Safety
/// `p` is live; each text null or its length's readable bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_restore_defaults(
    p: *mut EeWonLobbyPrefs,
    quick_play: *const u8,
    quick_play_length: usize,
    lan: *const u8,
    lan_length: usize,
) -> bool {
    obj_mut!(p).restore_defaults(bytes(quick_play, quick_play_length), bytes(lan, lan_length))
}
/// `LobbyRestoreDefaults`' rule after the restore.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_after_restore(p: *mut EeWonLobbyPrefs, old_adapter: i64) {
    obj_mut!(p).after_restore(old_adapter)
}
/// The end of `SaveGlobalPrefs`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_after_global_save(p: *mut EeWonLobbyPrefs) {
    obj_mut!(p).after_global_save()
}

/// The end of `LoadUserPrefs`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyprefs_after_user_load(p: *mut EeWonLobbyPrefs) {
    obj_mut!(p).after_user_load()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_constructor_and_the_defaults() {
        let p = EeWonLobbyPrefs::new();
        assert!(p.flag(F::LobbySoundEffects) && p.flag(F::SavedLobbyMusic) && !p.flag(F::AnonymousToFriends));
        assert!(!p.flag(F::GlobalDirty) && !p.flag(F::UserDirty));
        assert_eq!(
            (p.number(EeWonPrefsNumber::LanBroadcastPort), p.number(EeWonPrefsNumber::ConnectionSpeed)),
            (8585, 25)
        );
        let mut q = p.clone();
        q.set_current_user(7);
        assert!(!q.restore_defaults(b"QuickPlay", b"Lan"));
        assert_eq!(q.text(EeWonPrefsText::TabSelect), b"QuickPlay");
        q.set_current_user(0);
        assert!(q.restore_defaults(b"QuickPlay", b"Lan"));
        assert_eq!(q.text(EeWonPrefsText::TabSelect), b"Lan");
    }

    #[test]
    fn the_setters_rules() {
        let mut p = EeWonLobbyPrefs::new();
        p.change_flag(F::ChatSoundEffects, true);
        assert!(!p.flag(F::UserDirty));
        p.change_flag(F::ChatSoundEffects, false);
        assert!(p.flag(F::UserDirty) && !p.flag(F::GlobalDirty));
        // the sound: global dirty while it differs from the saved state
        p.change_flag(F::LobbySoundEffects, false);
        assert!(p.flag(F::GlobalDirty));
        p.change_flag(F::LobbySoundEffects, true);
        assert!(!p.flag(F::GlobalDirty));
        p.change_flag(F::LobbyMusic, false);
        p.after_global_save();
        assert!(!p.flag(F::SavedLobbyMusic) && !p.flag(F::GlobalDirty));
        p.change_flag(F::LobbyMusic, true);
        assert!(p.flag(F::GlobalDirty));
        // the adapter
        p.after_global_save();
        p.save_network_adapter(0);
        assert!(!p.flag(F::GlobalDirty));
        p.save_network_adapter(0x0a00_0001);
        assert!(p.flag(F::GlobalDirty));
        // the restore: the adapter compared as an int
        p.after_global_save();
        let old = p.network_adapter();
        p.restore_defaults(b"Q", b"L");
        p.after_restore(old);
        assert!(p.flag(F::GlobalDirty) && p.flag(F::UserDirty));
        // the last room and the tab
        p.set_flag(F::UserDirty, false);
        p.set_last_room(b"", b"");
        assert!(!p.flag(F::UserDirty));
        p.set_last_room(b"s", b"");
        assert!(p.flag(F::UserDirty));
        p.set_flag(F::UserDirty, false);
        p.change_tab_select(b"L");
        assert!(!p.flag(F::UserDirty));
        p.change_tab_select(b"l");
        assert!(p.flag(F::UserDirty));
        p.set_flag(F::UserDirty, false);
        p.change_number(EeWonPrefsNumber::ConnectionSpeed, 25);
        assert!(!p.flag(F::UserDirty));
        p.change_number(EeWonPrefsNumber::ConnectionSpeed, 50);
        assert!(p.flag(F::UserDirty));
    }
}
