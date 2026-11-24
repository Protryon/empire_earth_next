//! The save's other chunks (game core lane, round 9): the players' (was EEPPlayerHelper.cpp, 0x14b22d..0x14b7a5:
//! the slot count, the local player, each player with its type and, for a computer player, its engine, AI id, the
//! next engine's state and the per-unit Lua state), the header's (EEPSaveGameHeader.cpp, 0x14b7a5..0x14bd64), the
//! multiplayer state's (EEPMultiplayerHelper.cpp: the physics and the network's tick counters), the calamities'
//! (EEPCalamityHelper.cpp: the active calamities and the contagions) and the checksum's (EEPChecksum.cpp). Each C++
//! call or member is a case of EEPPlayerHelper.cpp's `EEPHelpersOp` or EESaveLoad.cpp's `EECore9OpC`
//! ([`super::EeGameCpp`]'s lane C section); the methods forward here.

#![allow(non_snake_case)]
// the C++'s `inMajorVersion <= 0` on unsigned versions, kept as written
#![allow(clippy::unnecessary_cast, clippy::absurd_extreme_comparisons)]

use std::ffi::{c_long, c_ulong, c_void, CStr};

use ee_base::error::checksum::{
    ee_error_checksum_clear, ee_error_checksum_count, ee_error_checksum_entry, ee_error_checksum_push,
    ee_error_checksum_set_verbose, ee_error_checksum_verbose, EeErrorChecksum,
};
use ee_sim::tech::calamity_record::EeCalamityFields;
use ee_sim::world::player_fields::EeUnitsPlayerSaved;
use ee_sim::world::unit_fields::EeUnitsRtsUnitFields;

use super::persistent::{Pf, PF_FAILED, PF_OK};
use super::{h, hp, pa, rec, Owned, O, P};

/// `EEPPlayerHelper`'s members from `mLocalPlayerIndex` (+0x18) to `mLoaded` (+0x21).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EePPlayerHelperFields {
    pub mLocalPlayerIndex: c_ulong,
    pub mMaxPlayers: c_ulong,
    pub mIsForLoading: bool,
    pub mLoaded: bool,
}

/// `kEECPEngineClassic`, `kEECPEngineNext` (ee/EECPEngine.h).
const PP_ENGINE_CLASSIC: u8 = 0;
const PP_ENGINE_NEXT: u8 = 1;
/// The bytes a saved script state may hold (64 MB).
const PP_STATE_LIMIT: c_ulong = 64 << 20;
/// The error of a helper not loaded (GetLocalPlayerIndex, GetPlayers).
const PP_NOT_LOADED: i64 = 13;

fn fields<'a>(helper: P) -> &'a mut EePPlayerHelperFields {
    // SAFETY: the helper's plain members, used before the next C++ call that changes them
    unsafe { &mut *rec(O::PphFields, helper) }
}

fn ver(major: c_ulong, minor: c_ulong) -> [i64; 2] {
    [major as i64, minor as i64]
}

/// `EEPPlayerHelper()`: for loading, three slots, local player 0.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pph_init(helper: *mut c_void) {
    *fields(helper as P) =
        EePPlayerHelperFields { mLocalPlayerIndex: 0, mMaxPlayers: 3, mIsForLoading: true, mLoaded: false };
}

/// `EEPPlayerHelper(EEWorld&)`: for saving, the world's existing players, slot count and current player.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pph_from_world(helper: *mut c_void, world: *mut c_void) {
    let (helper, w) = (helper as P, world as P);
    for i in 0..17 {
        let pl = hp(O::WPlayer, w, &[i]);
        if pl != 0 {
            h(O::PphPush, helper, &[pa(pl)]);
        }
    }
    // SAFETY: the world's long members
    let (current, max) =
        unsafe { (*(hp(O::WCurrentPlayer, w, &[]) as *const c_long), *(hp(O::WMaxPlayers, w, &[]) as *const c_long)) };
    let f = fields(helper);
    f.mIsForLoading = false;
    f.mLocalPlayerIndex = current as c_ulong;
    f.mLoaded = false;
    f.mMaxPlayers = max as c_ulong;
}

/// `EEPPlayerHelper::GetChecksum`: every player's checksum and the local player index.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pph_checksum(helper: *const c_void, checksum: *mut c_void) {
    let helper = helper as P;
    let n = h(O::PsPphPlayerCount, helper, &[]);
    for i in 0..n {
        h(O::PphPlayerChecksum, hp(O::PsPphPlayerAt, helper, &[i]), &[checksum as i64]);
    }
    h(O::PphChecksumLocal, checksum as P, &[fields(helper).mLocalPlayerIndex as i64]);
}

/// `EEPPlayerHelper::GetLocalPlayerIndex`: the loaded local player index; 13 unless the chunk was loaded.
///
/// # Safety
/// `out` is the caller's live value.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_pph_local_index(helper: *mut c_void, out: *mut c_long) -> c_long {
    let f = *fields(helper as P);
    if f.mIsForLoading && f.mLoaded {
        // SAFETY: the caller's out value
        unsafe { *out = f.mLocalPlayerIndex as c_long };
        return PF_OK as c_long;
    }
    PP_NOT_LOADED as c_long
}

/// `EEPPlayerHelper::GetPlayers`: the loaded players; 13 unless the chunk was loaded.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pph_players(helper: *mut c_void, out: *mut c_void) -> c_long {
    let helper = helper as P;
    let f = *fields(helper);
    if f.mIsForLoading && f.mLoaded {
        h(O::PphCopyPlayers, helper, &[out as i64]);
        return PF_OK as c_long;
    }
    PP_NOT_LOADED as c_long
}

/// A length-prefixed byte block (a uint32 length, then the bytes; `limit` the most it may hold, `zero_fails` when 0 is
/// an error): the result and the bytes.
fn read_block(file: Pf, limit: c_ulong, zero_fails: bool, fill: u8) -> (i64, Vec<u8>) {
    let mut len: c_ulong = 0;
    let mut r = file.read_ulong(&mut len);
    if r != PF_OK {
        return (r, Vec::new());
    }
    if zero_fails && (len == 0 || len > limit) {
        return (PF_FAILED, Vec::new());
    }
    if len == 0 {
        return (r, Vec::new());
    }
    if len > limit {
        return (PF_FAILED, Vec::new());
    }
    let mut bytes = vec![fill; len as usize];
    let (rr, read) = file.read_chars(bytes.as_mut_ptr(), len);
    r = rr;
    if r == PF_OK && read != len {
        r = PF_FAILED;
    }
    (r, bytes)
}

fn bytes_arg(b: &[u8]) -> [i64; 2] {
    [b.as_ptr() as usize as i64, b.len() as i64]
}

/// `EEPPlayerHelper::Load`: the slot count, the local player index and each player (human or computer), then each
/// player's ally list filled with the allies that share line of sight; the first read error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pph_load(
    helper: *mut c_void,
    major: c_ulong,
    minor: c_ulong,
    file: *mut c_void,
) -> c_long {
    let (helper, file) = (helper as P, Pf(file as P));
    let v = ver(major, minor);
    let mut r = PF_OK;
    if fields(helper).mIsForLoading {
        let mut count: c_ulong = 0;
        r = file.read_ulong(&mut count);
        if r == PF_OK {
            let f = fields(helper) as *mut EePPlayerHelperFields;
            // SAFETY: the helper's members, one address per read
            r = file.read_ulong(unsafe { std::ptr::addr_of_mut!((*f).mMaxPlayers) });
            if r == PF_OK {
                // SAFETY: as above
                r = file.read_ulong(unsafe { std::ptr::addr_of_mut!((*f).mLocalPlayerIndex) });
                if r == PF_OK {
                    let mut i = 0;
                    while i < count && r == PF_OK {
                        r = load_player(helper, file, major, minor, v);
                        fields(helper).mLoaded = true;
                        i += 1;
                    }
                }
            }
        }
    }
    let n = h(O::PsPphPlayerCount, helper, &[]);
    for i in 0..n {
        for j in 0..n {
            let pi = hp(O::PsPphPlayerAt, helper, &[i]);
            let pj = hp(O::PsPphPlayerAt, helper, &[j]);
            // SAFETY: the player's record
            if j != i
                && h(O::PphIsAllied, pj, &[pa(pi)]) != 0
                && unsafe { (*rec::<EeUnitsPlayerSaved>(O::PlSaved, pj)).mSharedLOS }
            {
                h(O::PphAddAlly, pj, &[pa(pi)]);
            }
        }
    }
    r as c_long
}

/// One player of Load: its type, the player made and loaded, a computer player's engine and AI (0.251, 0.253), the
/// next engine's state (0.252) and the per-unit Lua state (0.280).
fn load_player(helper: P, file: Pf, major: c_ulong, minor: c_ulong, v: [i64; 2]) -> i64 {
    let mut kind: c_long = 0;
    let mut r = file.read_long(&mut kind);
    if r != PF_OK {
        return r;
    }
    let pl = match kind {
        0 => hp(O::PphNewHuman, 0, &[]),
        1 => hp(O::PphNewComputer, 0, &[]),
        3 => {
            h(O::PphVerifyType3, 0, &[]);
            0
        }
        _ => 0,
    };
    // an unknown player type (a corrupt save) makes no player: the load fails instead of calling through NULL
    r = if pl == 0 { PF_FAILED } else { h(O::PphPlayerLoad, pl, &[v[0], v[1], pa(file.0)]) };
    let cp = if pl != 0 { hp(O::PphComputerOf, pl, &[]) } else { 0 };
    // port: a computer player's engine from 0.251, from 0.253 the next engine's AI id after it; an AI the manifest
    // no longer lists fails the load; a seat saved with the deleted C++ classic plays the manifest's "classic"
    if r == PF_OK && kind == 1 && cp != 0 && (major > 0 || minor >= 251) {
        let mut engine = PP_ENGINE_CLASSIC;
        r = file.read_u8(&mut engine);
        h(O::PphCpSetEngineNext, cp, &[]);
        if r == PF_OK && engine == PP_ENGINE_NEXT {
            let mut ai: Vec<u8> = b"boom".to_vec();
            if major > 0 || minor >= 253 {
                let (rr, bytes) = read_block(file, 64, true, 0);
                r = rr;
                if r == PF_OK {
                    ai = bytes;
                }
            }
            // SAFETY: the id's bytes
            if r == PF_OK
                && ai != b"none"
                && unsafe { crate::cp_engine::ee_cp_engine_by_id(ai.as_ptr(), ai.len()) }.is_null()
            {
                let a = bytes_arg(&ai);
                h(O::PphLogUnknownAi, pl, &[a[0], a[1]]);
                r = PF_FAILED;
            }
            if r == PF_OK {
                let a = bytes_arg(&ai);
                h(O::PphCpSetAi, cp, &[a[0], a[1]]);
            }
        }
    }
    // port: from 0.252 the next engine's state and its script's memory (0 for none)
    if r == PF_OK && kind == 1 && cp != 0 && (major > 0 || minor >= 252) {
        let (rr, bytes) = read_block(file, PP_STATE_LIMIT, false, 0);
        r = rr;
        // SAFETY: the loaded computer player and the block's bytes
        if r == PF_OK
            && !bytes.is_empty()
            && !unsafe { ee_sim::ai::next_game::ee_ai_next_load(cp as *mut c_void, bytes.as_ptr(), bytes.len()) }
        {
            r = PF_FAILED;
        }
    }
    // port: from 0.280 the per-unit Lua interface's state (0 for none; EESaveHasUnitsLuaState)
    if r == PF_OK && kind == 1 && cp != 0 && ee_sim::save_versions::EESaveHasUnitsLuaState(major, minor) {
        let (rr, bytes) = read_block(file, PP_STATE_LIMIT, false, b' ');
        r = rr;
        // SAFETY: as above
        if r == PF_OK
            && !bytes.is_empty()
            && !unsafe { ee_sim::ai::units_bind::ee_ai_units_load(cp as *mut c_void, bytes.as_ptr(), bytes.len()) }
        {
            r = PF_FAILED;
        }
    }
    if r == PF_OK {
        h(O::PphPush, helper, &[pa(pl)]);
    }
    r
}

/// A C++ `std::string`'s bytes.
fn cpp_string(s: P) -> Vec<u8> {
    let n = h(O::PphStrSize, s, &[]) as usize;
    let d = hp(O::PphStrData, s, &[]) as *const u8;
    if n == 0 {
        return Vec::new();
    }
    // SAFETY: the string's n bytes, copied before the next C++ call
    unsafe { std::slice::from_raw_parts(d, n) }.to_vec()
}

fn write_block(file: Pf, bytes: &[u8]) -> i64 {
    let mut r = file.write_ulong(bytes.len() as c_ulong);
    if r == PF_OK && !bytes.is_empty() {
        r = file.write_chars(bytes);
    }
    r
}

/// `EEPPlayerHelper::Save`: the player count, slot count, local player index and each player with its type (and a
/// computer player's engine, AI id, next engine's state and per-unit Lua state); fails for a loading chunk or no
/// players.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pph_save(
    helper: *mut c_void,
    major: c_ulong,
    minor: c_ulong,
    file: *mut c_void,
) -> c_long {
    let (helper, file) = (helper as P, Pf(file as P));
    let count = h(O::PsPphPlayerCount, helper, &[]) as c_ulong;
    let f = *fields(helper);
    if f.mIsForLoading || count == 0 {
        return PF_FAILED as c_long;
    }
    let mut r = file.write_ulong(count);
    if r != PF_OK {
        return r as c_long;
    }
    r = file.write_ulong(f.mMaxPlayers);
    if r != PF_OK {
        return r as c_long;
    }
    r = file.write_ulong(f.mLocalPlayerIndex);
    if r != PF_OK {
        return r as c_long;
    }
    let mut i = 0;
    while i < count && r == PF_OK {
        let pl = hp(O::PsPphPlayerAt, helper, &[i as i64]);
        let kind = player_type(pl);
        r = file.write_long(kind);
        if r == PF_OK {
            r = h(O::PphPlayerSave, pl, &[major as i64, minor as i64, pa(file.0)]);
        }
        let cp = hp(O::PphComputerOf, pl, &[]);
        if r == PF_OK && kind == 1 {
            // port: the computer player's engine (Load: from 0.251)
            r = file.write_u8(h(O::PphCpEngine, cp, &[]) as u8);
        }
        if r == PF_OK && kind == 1 && h(O::PphCpEngine, cp, &[]) as u8 == PP_ENGINE_NEXT {
            // port: the next engine's AI id (Load: from 0.253)
            let s = Owned::new(O::PphStrNew, O::PphStrDelete);
            h(O::PphCpEngineAi, cp, &[pa(s.0)]);
            let mut ai = cpp_string(s.0);
            if ai.is_empty() {
                if !crate::cp_engine::ee_cp_engine_classic().is_null() {
                    h(O::PphClassicAiId, 0, &[pa(s.0)]);
                    ai = cpp_string(s.0);
                } else {
                    ai = b"none".to_vec();
                }
            }
            r = write_block(file, &ai);
        }
        if r == PF_OK && kind == 1 {
            // port: the next engine's state (Load: from 0.252); a script's failure is logged, its state goes
            let s = Owned::new(O::PphStrNew, O::PphStrDelete);
            h(O::PphCpSaveState, cp, &[pa(s.0)]);
            r = write_block(file, &cpp_string(s.0));
        }
        if r == PF_OK && kind == 1 {
            // port: the per-unit Lua interface's state (Load: from 0.280)
            let s = Owned::new(O::PphStrNew, O::PphStrDelete);
            h(O::PphCpUnitsSave, cp, &[pa(s.0)]);
            r = write_block(file, &cpp_string(s.0));
        }
        i += 1;
    }
    r as c_long
}

/// GetPlayerType: 0 human, 1 computer, 3 both (asserts when neither).
fn player_type(pl: P) -> c_long {
    let computer = hp(O::PphComputerOf, pl, &[]) != 0;
    let human = h(O::PphHasHuman, pl, &[]) != 0;
    h(O::PphVerifyPlayerType, 0, &[(computer || human) as i64]);
    if computer && human {
        return 3;
    }
    if human {
        return 0;
    }
    1
}

// ---------------------------------------------------------------------------------------------- the header

fn hdr_versions(header: P) -> *mut c_ulong {
    hp(O::PsHdrVersions, header, &[]) as *mut c_ulong
}

fn hdr_numbers(header: P) -> *mut c_ulong {
    hp(O::PsHdrNumbers, header, &[]) as *mut c_ulong
}

/// `EEPSaveGameHeader::Load`: the name and version part, then the body (a save older than 0.255 holds Windows-1252
/// text, converted as it loads); the first read error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_hdr_load(header: *mut c_void, file: *mut c_void) -> c_long {
    let (header, file) = (header as P, Pf(file as P));
    let mut r = load_header(header, file);
    if r == PF_OK {
        // SAFETY: the header's versions
        let (maj, min) = unsafe { (*hdr_versions(header), *hdr_versions(header).add(1)) };
        ee_base::ustring::ee_ustring_set_legacy_text_load(!ee_sim::save_versions::EESaveTextIsUtf8(maj, min));
        r = load_body(header, file);
        ee_base::ustring::ee_ustring_set_legacy_text_load(false);
    }
    r as c_long
}

/// LoadHeader: the name, description and version; fails for saves older than 0.197.
fn load_header(header: P, file: Pf) -> i64 {
    let mut r = h(O::HdrNameLoad, header, &[pa(file.0)]);
    if r != PF_OK {
        return r;
    }
    r = h(O::HdrDescriptionLoad, header, &[pa(file.0)]);
    if r != PF_OK {
        return r;
    }
    let v = hdr_versions(header);
    r = file.read_ulong(v);
    if r != PF_OK {
        return r;
    }
    // SAFETY: the header's minor version
    r = file.read_ulong(unsafe { v.add(1) });
    // SAFETY: as above
    if r == PF_OK && unsafe { *v <= 0 && *v.add(1) < 0xc5 } {
        r = PF_FAILED;
    }
    r
}

/// LoadBody: the player names, date, game options, player count, chunk tags, media strings and checksum.
fn load_body(header: P, file: Pf) -> i64 {
    let fp = pa(file.0);
    let n = hdr_numbers(header);
    let mut r = h(O::HdrPlayerNamesLoad, header, &[fp]);
    if r == PF_OK {
        r = h(O::HdrDateLoad, header, &[fp]);
    }
    if r == PF_OK {
        r = file.read_ulong(n);
    }
    if r == PF_OK {
        r = h(O::HdrOptionsLoad, header, &[fp]);
    }
    if r == PF_OK {
        // SAFETY: the header's mNumPlayers
        r = file.read_ulong(unsafe { n.add(1) });
    }
    if r == PF_OK {
        h(O::PsHdrTagsClear, header, &[]);
        let mut count: c_long = 0;
        r = file.read_long(&mut count);
        if r == PF_OK {
            let mut i = 0;
            while i < count && r == PF_OK {
                let mut value: c_ulong = 0;
                r = file.read_ulong(&mut value);
                if r == PF_OK {
                    h(O::PsHdrTagsPush, header, &[value as c_long as i64]);
                }
                i += 1;
            }
            if r == PF_OK {
                r = h(O::HdrMediaLoad, header, &[fp]);
                if r == PF_OK {
                    // SAFETY: the header's mChecksum
                    r = file.read_ulong(unsafe { n.add(2) });
                }
            }
        }
    }
    r
}

/// `EEPSaveGameHeader::LoadFromFile`: a save file opened, copied into memory and its header read from there.
#[no_mangle]
pub extern "C" fn ee_gamecpp_hdr_load_from_file(header: *mut c_void, file: *mut c_void) -> c_long {
    let (header, file) = (header as P, file as P);
    let memory = Owned::new(O::PsMemFileNew1000, O::PsMemFileDelete);
    let mut r = h(O::PsSpecOpenAny, file, &[1]);
    if r != PF_OK {
        return r as c_long;
    }
    r = h(O::PsMemFileLoad, memory.0, &[pa(file)]);
    if r == PF_OK {
        h(O::PsFileCloseAny, file, &[]);
        r = ee_gamecpp_hdr_load(header as *mut c_void, memory.0 as *mut c_void) as i64;
        h(O::PsFileCloseAny, memory.0, &[]);
    }
    r as c_long
}

/// `EEPSaveGameHeader::Save`: the name and version part, then the body; the first write error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_hdr_save(header: *mut c_void, file: *mut c_void) -> c_long {
    let (header, file) = (header as P, Pf(file as P));
    let fp = pa(file.0);
    let v = hdr_versions(header);
    let n = hdr_numbers(header);
    // the header part
    let mut r = h(O::HdrNameSave, header, &[fp]);
    if r != PF_OK {
        return r as c_long;
    }
    r = h(O::HdrDescriptionSave, header, &[fp]);
    if r != PF_OK {
        return r as c_long;
    }
    // SAFETY: the header's versions and numbers, read right before each write
    r = file.write_ulong(unsafe { *v });
    if r == PF_OK {
        r = file.write_ulong(unsafe { *v.add(1) });
    }
    if r != PF_OK {
        return r as c_long;
    }
    // the body
    r = h(O::HdrPlayerNamesSave, header, &[fp]);
    if r != PF_OK {
        return r as c_long;
    }
    r = h(O::HdrDateSave, header, &[fp]);
    if r != PF_OK {
        return r as c_long;
    }
    r = file.write_ulong(unsafe { *n });
    if r != PF_OK {
        return r as c_long;
    }
    r = h(O::HdrOptionsSave, header, &[fp]);
    if r != PF_OK {
        return r as c_long;
    }
    r = file.write_ulong(unsafe { *n.add(1) });
    if r != PF_OK {
        return r as c_long;
    }
    let count = h(O::PsHdrTagsCount, header, &[]);
    r = file.write_long(count as c_long);
    if r != PF_OK {
        return r as c_long;
    }
    let mut i = 0;
    while i < count && r == PF_OK {
        r = file.write_ulong(h(O::PsHdrTagAt, header, &[i]) as c_long as c_ulong);
        i += 1;
    }
    if r != PF_OK {
        return r as c_long;
    }
    r = h(O::HdrMediaSave, header, &[fp]);
    if r == PF_OK {
        r = file.write_ulong(unsafe { *n.add(2) });
    }
    r as c_long
}

// ---------------------------------------------------------------------------------------------- the multiplayer state

/// `EEPMultiplayerHelper::Load`: the physics state and the eight network tick counters; only the server's or client's
/// physics tick restored from them; the first read error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pmh_load(major: c_ulong, minor: c_ulong, file: *mut c_void) -> c_long {
    let file = Pf(file as P);
    // the message number, the client's CT and PT, the communications' CT and last tick, the server's last CT, CT, PT
    let mut v: [c_ulong; 8] = [0; 8];
    let mut r = h(O::PmPhysicsLoad, file.0, &ver(major, minor));
    for x in v.iter_mut() {
        if r != PF_OK {
            break;
        }
        r = file.read_ulong(x);
    }
    if r == PF_OK && h(O::PsCommPresent, 0, &[]) != 0 {
        if h(O::CommIsServer, 0, &[]) != 0 {
            h(O::PmServerSetPhysicsTick, 0, &[&mut v[7] as *mut c_ulong as usize as i64]);
        } else {
            h(O::PmClientSetPhysicsTick, 0, &[&mut v[2] as *mut c_ulong as usize as i64]);
        }
    }
    r as c_long
}

/// `EEPMultiplayerHelper::Save`: the physics state and the server's or client's network tick counters (nothing for
/// versions up to 0.12); the first write error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pmh_save(major: c_ulong, minor: c_ulong, file: *mut c_void) -> c_long {
    let file = Pf(file as P);
    if major <= 0 && minor <= 12 {
        return PF_OK as c_long;
    }
    let mut v: [c_ulong; 8] = [0; 8];
    v[3] = h(O::PmCommCtsProcessed, 0, &[]) as c_ulong;
    v[4] = h(O::PmCommLastTick, 0, &[]) as c_ulong;
    if h(O::CommIsServer, 0, &[]) != 0 {
        v[5] = h(O::PmServerLastCt, 0, &[]) as c_ulong;
        v[6] = h(O::PmServerCt, 0, &[]) as c_ulong;
        v[7] = h(O::PmServerPt, 0, &[]) as c_ulong;
        v[0] = h(O::PmServerMessageNumber, 0, &[]) as c_ulong;
    } else {
        v[1] = h(O::PmClientCt, 0, &[]) as c_ulong;
        v[2] = h(O::PmClientPt, 0, &[]) as c_ulong;
    }
    let mut r = h(O::PmPhysicsSave, file.0, &ver(major, minor));
    for &x in v.iter() {
        if r != PF_OK {
            return r as c_long;
        }
        r = file.write_ulong(x);
    }
    r as c_long
}

// ---------------------------------------------------------------------------------------------- the calamities

fn cal<'a>(calamity: P) -> &'a mut EeCalamityFields {
    // SAFETY: a live calamity's record, used before the next C++ call that changes it
    unsafe { &mut *rec(O::CalFields, calamity) }
}

/// `EEPCalamityHelper::Load`: the active calamities, then the contagion manager; the contagion manager's error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pch_load(
    helper: *mut c_void,
    major: c_ulong,
    minor: c_ulong,
    file: *mut c_void,
) -> c_long {
    let (helper, file) = (helper as P, Pf(file as P));
    h(O::PchClearCalamities, helper, &[]);
    if h(O::PchCalamities, helper, &[]) == 0 {
        h(O::PchNewCalamities, helper, &[]);
    }
    let mut count: c_long = 0;
    file.read_long(&mut count);
    for _ in 0..count {
        let calamity = hp(O::PchNewCalamity, 0, &[]);
        h(O::PchCalamityLoad, calamity, &[major as i64, minor as i64, pa(file.0)]);
        h(O::PchPush, helper, &[pa(calamity)]);
    }
    h(O::PchContagionLoad, helper, &[major as i64, minor as i64, pa(file.0)]) as c_long
}

/// `EEPCalamityHelper::RepairPointers`: the loaded calamities reconnected to their caster and target units and their
/// media loaded (those whose units are gone deleted); then the contagions reconnected.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pch_repair(helper: *mut c_void) -> c_long {
    let helper = helper as P;
    if h(O::PchCalamities, helper, &[]) == 0 {
        return 0;
    }
    let n = h(O::PchCount, helper, &[]);
    for i in 0..n {
        let calamity = hp(O::PchAt, helper, &[i]);
        let mut error = 0;
        let class = hp(O::CalClass, calamity, &[]);
        if class == 0 {
            error = 3;
        }
        let caster_id = cal(calamity).mOriginatingUnitID;
        let caster = hp(O::WorldUnit, 0, &[caster_id as i64]);
        h(O::PchSetCaster, calamity, &[pa(caster)]);
        if caster == 0 {
            if caster_id as u32 == u32::MAX {
                h(O::KlLoadMedia, class, &[pa(hp(O::WorldPlayer, 0, &[0])), 0]);
            } else {
                error = 9;
            }
        } else {
            h(O::KlLoadMedia, class, &[pa(hp(O::UnPlayer, caster, &[])), 0]);
        }
        if cal(calamity).mHasTargetUnit {
            let target = hp(O::WorldUnit, 0, &[cal(calamity).mTargetUnitID as i64]);
            super::calamity_objects::ee_gamecpp_calamity_set_unit(
                calamity as *mut std::ffi::c_void,
                target as *mut std::ffi::c_void,
                true,
            );
            if target == 0 {
                error = 3;
            }
        }
        if error == 0 {
            h(O::PchPlaySound, calamity, &[]);
        } else {
            h(O::PchDeleteAt, helper, &[i]);
        }
    }
    h(O::PchRestoreContagions, helper, &[]) as c_long
}

/// `EEPCalamityHelper::Save`: the calamity manager's active calamities (with their current target ids) and the world's
/// contagions.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pch_save(major: c_ulong, minor: c_ulong, file: *mut c_void) -> c_long {
    let file = Pf(file as P);
    let active = hp(O::PchActive, 0, &[]);
    h(O::PchVerifyActive, 0, &[(active != 0) as i64]);
    if active == 0 {
        return 0;
    }
    let n = h(O::VecCalamitiesCount, active, &[]);
    file.write_long(n as c_long);
    for i in 0..n {
        let calamity = hp(O::VecCalamitiesAt, active, &[i]);
        let mut offset: c_long = 0;
        if h(O::PchClassIndex, calamity, &[&mut offset as *mut c_long as usize as i64]) == 0 {
            h(O::PchVerifyClassIndex, 0, &[]);
        }
        let mut target: P = 0;
        let id = if h(O::PchGetTarget, calamity, &[&mut target as *mut P as usize as i64]) != 0 {
            // SAFETY: the target's record
            unsafe { (*rec::<EeUnitsRtsUnitFields>(O::UnRts, target)).mUnitID }
        } else {
            c_ulong::MAX
        };
        cal(calamity).mTargetUnitID = id;
        h(O::PchCalamitySave, calamity, &[major as i64, minor as i64, pa(file.0)]);
    }
    h(O::PchContagionSave, 0, &[major as i64, minor as i64, pa(file.0)]) as c_long
}

// ---------------------------------------------------------------------------------------------- the checksum

fn checksum_set(chunk: P) -> *mut EeErrorChecksum {
    hp(O::PcRust, chunk, &[]) as *mut EeErrorChecksum
}

/// `EEPChecksum::Load`: the verbose flag and the described checksum entries; the first read error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pc_load(chunk: *mut c_void, file: *mut c_void) -> c_long {
    let (chunk, file) = (chunk as P, Pf(file as P));
    let set = checksum_set(chunk);
    // SAFETY: the chunk's live set
    unsafe { ee_error_checksum_clear(set) };
    let mut verbose = false;
    let mut r = file.read_bool(&mut verbose);
    if r != PF_OK {
        return r as c_long;
    }
    // SAFETY: as above
    unsafe { ee_error_checksum_set_verbose(set, verbose) };
    let mut count: c_long = 0;
    r = file.read_long(&mut count);
    if r != PF_OK {
        return r as c_long;
    }
    let description = Owned::new(O::PsUStrNew, O::PsUStrDelete);
    let mut i = 0;
    while i < count && r == PF_OK {
        r = h(O::PcUStrLoad, description.0, &[pa(file.0)]);
        if r != PF_OK {
            i += 1;
            continue;
        }
        let mut value: c_ulong = 0;
        r = file.read_ulong(&mut value);
        if r == PF_OK {
            // SAFETY: the description's text, live until the next change of the string
            unsafe { ee_error_checksum_push(set, hp(O::PcUStrText, description.0, &[]) as *const _, value as u32) };
        }
        i += 1;
    }
    r as c_long
}

/// `EEPChecksum::Save`: the verbose flag and the described checksum entries; the first write error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pc_save(chunk: *mut c_void, file: *mut c_void) -> c_long {
    let (chunk, file) = (chunk as P, Pf(file as P));
    let set = checksum_set(chunk);
    // SAFETY: the chunk's live set
    let count = unsafe { ee_error_checksum_count(set) };
    let mut r = file.write_bool(unsafe { ee_error_checksum_verbose(set) });
    if r != PF_OK {
        return r as c_long;
    }
    r = file.write_long(count as c_long);
    if r != PF_OK {
        return r as c_long;
    }
    let mut i = 0;
    while i < count && r == PF_OK {
        let mut value: u32 = 0;
        // SAFETY: the entry's length, then its text into a buffer one longer
        let len = unsafe { ee_error_checksum_entry(set, i, std::ptr::null_mut(), 0, &mut value) };
        let mut text = vec![0u8; len + 1];
        unsafe { ee_error_checksum_entry(set, i, text.as_mut_ptr() as *mut _, text.len(), &mut value) };
        let text = CStr::from_bytes_until_nul(&text).map(|c| c.as_ptr()).unwrap_or(std::ptr::null());
        r = h(O::PcSaveText, file.0, &[text as usize as i64]);
        if r == PF_OK {
            r = file.write_ulong(value as c_ulong);
        }
        i += 1;
    }
    r as c_long
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_player_helper_fields_mirror_the_class() {
        let l = std::mem::size_of::<c_ulong>();
        let f = EePPlayerHelperFields::default();
        let base = &f as *const _ as usize;
        assert_eq!(&f.mLoaded as *const _ as usize - base, 2 * l + 1);
    }

    #[test]
    fn the_engines_and_limits() {
        assert_eq!((PP_ENGINE_CLASSIC, PP_ENGINE_NEXT), (0, 1));
        assert_eq!(PP_STATE_LIMIT, 64 * 1024 * 1024);
    }

    #[test]
    fn a_block_argument_is_its_bytes() {
        let b = b"boom";
        assert_eq!(bytes_arg(b)[1], 4);
    }
}
