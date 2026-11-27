//! The C ABI the C++ classes call (include/rust/ee_app.h).

use crate::app::game_options::{self, EeAppOptionsFields, EeAppOptionsLimits, EeAppOptionsPlayerFields};
use crate::app::save_io::{EeAppSaveFn, Host};
use std::ffi::c_void;

/// The slots' records behind the C++ objects' pointers, copied in (the host calls touch only the C++ members).
///
/// # Safety
/// `players` holds `count` live record pointers.
unsafe fn players_in(players: *const *mut EeAppOptionsPlayerFields, count: u32) -> Vec<EeAppOptionsPlayerFields> {
    (0..count as usize).map(|i| unsafe { **players.add(i) }).collect()
}

/// # Safety
/// as [`players_in`]
unsafe fn players_out(players: *const *mut EeAppOptionsPlayerFields, values: &[EeAppOptionsPlayerFields]) {
    for (i, v) in values.iter().enumerate() {
        unsafe { **players.add(i) = *v };
    }
}

/// `EEGameOptionsPlayer()`'s plain members.
///
/// # Safety
/// `out` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_app_options_player_default(out: *mut EeAppOptionsPlayerFields) {
    unsafe { *out = game_options::player_default() };
}

/// `EEGameOptions::Load` over the host (`EESavePrimitive` and the strings); the first read error.
///
/// # Safety
/// live records; `players` holds `count` live record pointers; `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_options_load(
    options: *mut EeAppOptionsFields,
    players: *const *mut EeAppOptionsPlayerFields,
    count: u32,
    major: u32,
    minor: u32,
    limits: *const EeAppOptionsLimits,
    ctx: *mut c_void,
    call: EeAppSaveFn,
) -> i32 {
    let mut host = Host { ctx, call };
    let mut o = unsafe { *options };
    let mut p = unsafe { players_in(players, count) };
    let r = game_options::load(&mut host, &mut o, &mut p, major, minor, unsafe { &*limits });
    unsafe {
        *options = o;
        players_out(players, &p);
    }
    r
}
