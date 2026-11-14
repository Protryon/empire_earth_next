//! The C ABI of NESynchronousServer's Rust part (`server.rs`): the C++ server holds an `EeNetServer*`; its calls out
//! go to the server's host function by `EeNetSvOp`.

use std::ffi::c_void;

use crate::server::{Server, ServerHost, SvField};

/// The server's fields the Rust reads and writes (still the C++ class's)
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeNetSvField {
    /// mNumHumanPlayers (read, written)
    NumHumanPlayers = 0,
    /// mMaxPlayers
    MaxPlayers = 1,
}

/// The server's calls out; a, b, c, d as each op says, the answer as isize.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeNetSvOp {
    /// a field (EeNetSvField) -> its value
    Get = 0,
    /// a field, a value
    Set = 1,
    /// a player -> IsPlayerPresent
    IsPlayerPresent = 2,
    /// a player -> GetPlayerSendToSlot
    PlayerSendToSlot = 3,
    /// a slot: UnregisterSlot
    UnregisterSlot = 4,
    /// BeforeRemovingPlayers()
    BeforeRemovingPlayers = 5,
    /// player, booted, out of sync, exiting: ProcessLocalUserInput(new NEMRemovePlayer(...), kSlotNone)
    RemovePlayerInput = 6,
}

pub type EeNetSvHostFn =
    unsafe extern "C" fn(ctx: *mut c_void, op: EeNetSvOp, a: usize, b: usize, c: usize, d: usize) -> isize;

pub struct SvFfiHost {
    ctx: *mut c_void,
    f: EeNetSvHostFn,
}

// The context is the C++ server, used from its threads as the original's members were.
unsafe impl Send for SvFfiHost {}
unsafe impl Sync for SvFfiHost {}

impl SvFfiHost {
    fn call(&self, op: EeNetSvOp, a: usize, b: usize, c: usize, d: usize) -> isize {
        unsafe { (self.f)(self.ctx, op, a, b, c, d) }
    }
}

fn field(f: SvField) -> EeNetSvField {
    match f {
        SvField::NumHumanPlayers => EeNetSvField::NumHumanPlayers,
        SvField::MaxPlayers => EeNetSvField::MaxPlayers,
    }
}

impl ServerHost for SvFfiHost {
    fn get(&self, f: SvField) -> u32 {
        self.call(EeNetSvOp::Get, field(f) as usize, 0, 0, 0) as u32
    }
    fn set(&self, f: SvField, value: u32) {
        self.call(EeNetSvOp::Set, field(f) as usize, value as usize, 0, 0);
    }
    fn is_player_present(&self, player: u8) -> bool {
        self.call(EeNetSvOp::IsPlayerPresent, player as usize, 0, 0, 0) != 0
    }
    fn player_send_to_slot(&self, player: u8) -> u8 {
        self.call(EeNetSvOp::PlayerSendToSlot, player as usize, 0, 0, 0) as u8
    }
    fn unregister_slot(&self, slot: u8) {
        self.call(EeNetSvOp::UnregisterSlot, slot as usize, 0, 0, 0);
    }
    fn before_removing_players(&self) {
        self.call(EeNetSvOp::BeforeRemovingPlayers, 0, 0, 0, 0);
    }
    fn remove_player_input(&self, player: u8, was_booted: bool, is_oos: bool, game_is_exiting: bool) {
        self.call(
            EeNetSvOp::RemovePlayerInput,
            player as usize,
            was_booted as usize,
            is_oos as usize,
            game_is_exiting as usize,
        );
    }
}

/// NESynchronousServer's Rust part.
pub struct EeNetServer(Server<SvFfiHost>);

fn sv<'a>(s: *const EeNetServer) -> &'a Server<SvFfiHost> {
    unsafe { &(*s).0 }
}

/// # Safety
/// `host` answers every op for `ctx` until `ee_net_sv_free`.
#[no_mangle]
pub unsafe extern "C" fn ee_net_sv_new(ctx: *mut c_void, host: EeNetSvHostFn) -> *mut EeNetServer {
    Box::into_raw(Box::new(EeNetServer(Server::new(SvFfiHost { ctx, f: host }))))
}

/// # Safety
/// `s` from `ee_net_sv_new` (or NULL), used by nothing after.
#[no_mangle]
pub unsafe extern "C" fn ee_net_sv_free(s: *mut EeNetServer) {
    if !s.is_null() {
        drop(Box::from_raw(s));
    }
}

#[no_mangle]
pub extern "C" fn ee_net_sv_is_oos(s: *const EeNetServer) -> bool {
    sv(s).is_oos()
}

#[no_mangle]
pub extern "C" fn ee_net_sv_log_checksum_response(s: *const EeNetServer, tick: u32, checksum: u32, slot: u8) {
    sv(s).log_checksum_response(tick, checksum, slot)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_remove_player_from_game_in_progress(
    s: *const EeNetServer,
    slot: u8,
    was_booted: bool,
    is_oos: bool,
    game_is_exiting: bool,
) {
    sv(s).remove_player_from_game_in_progress(slot, was_booted, is_oos, game_is_exiting)
}

#[no_mangle]
pub extern "C" fn ee_net_sv_remove_oos_players(s: *const EeNetServer) {
    sv(s).remove_oos_players()
}

/// The checksums' state (the tests): writes at most `size` bytes of `Server::checks_state` (NUL-terminated) and
/// answers its length.
///
/// # Safety
/// `out` holds `size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_net_sv_checks_state(s: *const EeNetServer, out: *mut u8, size: usize) -> usize {
    let text = sv(s).checks_state();
    if size > 0 {
        let n = text.len().min(size - 1);
        std::ptr::copy_nonoverlapping(text.as_ptr(), out, n);
        *out.add(n) = 0;
    }
    text.len()
}
