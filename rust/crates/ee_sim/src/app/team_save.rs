//! A team's players in a saved game (`EETeam::Load` and `Save`; was EETeam.cpp, the game's leaves round 2): the count,
//! then each player's id, through the saved game's primitives (cp file.rs's [`SaveFile`] over
//! `EESaveFilePrimitive`). The victory condition's own load stays the C++ object's (a virtual call, ee/EETeam.h).

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::ffi::{c_ulong, c_void};

use crate::cp::file::{SaveFile, OK};
use crate::units::player_save::EeUnitsPlayerSaveFn;

/// The player count's read: (the result, the count; 0 when the read failed).
pub fn load_count(f: &mut SaveFile) -> (i32, u32) {
    f.read_ulong(0)
}

/// The players' reads into `players` (each read into its old value), stopping at the first failure; that result.
pub fn load_players(f: &mut SaveFile, players: &mut [c_ulong]) -> i32 {
    let mut result = OK;
    for p in players.iter_mut() {
        if result == OK {
            let (r, v) = f.read_ulong(*p as u32);
            result = r;
            *p = v as c_ulong;
        }
    }
    result
}

/// The count and the players written; the first write error.
pub fn save(f: &mut SaveFile, players: &[c_ulong]) -> i32 {
    let mut result = f.write_ulong(players.len() as u32);
    for &p in players {
        if result == OK {
            result = f.write_ulong(p as u32);
        }
    }
    result
}

/// # Safety
/// `players` holds `count` values (or is null with 0).
unsafe fn slice<'a>(players: *mut c_ulong, count: usize) -> &'a mut [c_ulong] {
    if players.is_null() || count == 0 {
        &mut []
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts_mut(players, count) }
    }
}

/// `Load`'s count: the result; `out` the count (0 when the read failed).
///
/// # Safety
/// `file` and `host` are the saved game's (`EESaveFilePrimitive`); `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_app_team_load_count(file: *mut c_void, host: EeUnitsPlayerSaveFn, out: *mut u32) -> i32 {
    // SAFETY: the caller's contract
    let mut f = unsafe { SaveFile::new(file, host) };
    let (r, n) = load_count(&mut f);
    // SAFETY: as above
    unsafe { *out = n };
    r
}

/// `Load`'s players, into the team's `count` players (sized by the caller).
///
/// # Safety
/// As [`ee_app_team_load_count`]; `players` holds `count` values.
#[no_mangle]
pub unsafe extern "C" fn ee_app_team_load_players(
    file: *mut c_void,
    host: EeUnitsPlayerSaveFn,
    players: *mut c_ulong,
    count: usize,
) -> i32 {
    // SAFETY: the caller's contract
    let mut f = unsafe { SaveFile::new(file, host) };
    // SAFETY: as above
    load_players(&mut f, unsafe { slice(players, count) })
}

/// `Save`: the count and the players.
///
/// # Safety
/// As [`ee_app_team_load_players`].
#[no_mangle]
pub unsafe extern "C" fn ee_app_team_save(
    file: *mut c_void,
    host: EeUnitsPlayerSaveFn,
    players: *const c_ulong,
    count: usize,
) -> i32 {
    // SAFETY: the caller's contract
    let mut f = unsafe { SaveFile::new(file, host) };
    // SAFETY: as above (only read)
    save(&mut f, unsafe { slice(players as *mut c_ulong, count) })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::cp::file::test_file::{file, Bytes};

    #[test]
    fn players_round_trip_and_a_short_file_keeps_the_rest() {
        let mut b = Bytes::default();
        assert_eq!(save(&mut file(&mut b), &[3, 1, 7]), OK);
        assert_eq!(b.data.len(), 16);
        b.at = 0;
        let mut f = file(&mut b);
        assert_eq!(load_count(&mut f), (OK, 3));
        let mut p = [0 as c_ulong; 3];
        assert_eq!(load_players(&mut f, &mut p), OK);
        assert_eq!(p, [3, 1, 7]);
        // a file that ends after the first player: the others keep their old values, the error is the read's
        let mut b = Bytes::default();
        b.u32(2).u32(9);
        let mut f = file(&mut b);
        assert_eq!(load_count(&mut f), (OK, 2));
        let mut p = [5 as c_ulong, 6];
        assert_eq!(load_players(&mut f, &mut p), 0xa);
        assert_eq!(p, [9, 6]);
        let mut empty = Bytes::default();
        assert_eq!(load_count(&mut file(&mut empty)).0, 0xa);
    }
}
