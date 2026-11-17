//! A civilization file read and applied, and a player's upgrade request granted, in Rust (the civilization builder
//! lane, round 2): `EELoadCivilization`'s bytes (0x280840: the legacy binary form, its (category << 6) | level ids
//! converted, or the keyed text the builder writes), `EEApplyCivilizationFile` (0x2806e7: the AI's premade
//! civilizations, the campaign's, the player resets') and `EECBRequestUpgrades` (0x280a05: the server's grant of a
//! player's request, or the chat line refusing it).
//!
//! The file system, the player's `EECivilization`, the strings and the messages stay C++: `EELoadCivilization` finds,
//! opens and reads the file and hands its bytes to [`read_file`]; the two others ask their host ([`CivFileHost`],
//! EECBCivilizationFiles.cpp) for each step in the original's order.

use crate::tech::civ::{self, EeTechCivUpgrade, CIV_OK};
use std::ffi::c_void;
use std::os::raw::c_char;

/// `kErrorFailed`: a keyed text the game cannot read
pub(crate) const CIV_FAILED: i32 = 9;

/// What a civilization file's bytes give (`EELoadCivilization` after the file opened).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct CivFile {
    pub result: i32,
    /// the name, when the file sets it (the caller's name is kept otherwise)
    pub name: Option<Vec<u8>>,
    /// the upgrades read, appended to the caller's
    pub upgrades: Vec<EeTechCivUpgrade>,
    /// a keyed text's problem, shown to the player
    pub error: Option<Vec<u8>>,
}

/// Whether bytes are a keyed text (`eedata` or a comment first) rather than the legacy binary form.
pub fn is_keyed(bytes: &[u8]) -> bool {
    bytes.len() >= 6 && (bytes.starts_with(b"eedata") || bytes[0] == b'#')
}

/// A civilization file's bytes: `file` its name (`<name>.civ`, the keyed text's messages' place).
pub fn read_file(tables: &civ::CivTables, bytes: &[u8], file: &[u8]) -> CivFile {
    if is_keyed(bytes) {
        return read_keyed(tables, bytes, file);
    }
    let mut out = CivFile::default();
    let mut r = civ::Reader::new(bytes);
    let length = match r.u32() {
        Ok(n) => n,
        Err(e) => {
            out.result = e;
            return out;
        }
    };
    if length == 0 {
        out.name = Some(Vec::new());
    } else {
        // the original's 0x200-byte buffer, zeroed, then the name's bytes (a name read short is not set)
        let mut name = vec![0u8; length as usize];
        if r.bytes(&mut name) == CIV_OK {
            let end = name.iter().position(|&b| b == 0).unwrap_or(name.len());
            name.truncate(end);
            out.name = Some(name);
        }
    }
    let count = match r.u32() {
        Ok(n) => n,
        Err(e) => {
            out.result = e;
            return out;
        }
    };
    for _ in 0..count {
        match r.u32() {
            Ok(legacy) => out.upgrades.push(civ::from_legacy(legacy)),
            Err(e) => {
                out.result = e;
                break;
            }
        }
    }
    out
}

/// A keyed text (`EEReadKeyedCivilization`): the name, then each key as an upgrade until one is not.
fn read_keyed(tables: &civ::CivTables, bytes: &[u8], file: &[u8]) -> CivFile {
    let mut out = CivFile::default();
    match ee_data::ffi::civ_read(bytes, file) {
        Ok((name, keys)) => {
            out.name = Some(name.split(|&b| b == 0).next().unwrap_or(&[]).to_vec());
            for (key, line) in keys {
                match tables.find_upgrade_key(civ::rules(), &key) {
                    Some(u) => out.upgrades.push(u),
                    None => {
                        let mut e = file.to_vec();
                        e.extend_from_slice(b" line ");
                        e.extend_from_slice(line.to_string().as_bytes());
                        e.extend_from_slice(b": '");
                        e.extend_from_slice(&key);
                        e.extend_from_slice(b"' is not an upgrade (<dbcivilization id>.<civbuilder attribute id>)");
                        out.error = Some(e);
                        break;
                    }
                }
            }
        }
        Err(e) => out.error = Some(e),
    }
    if out.error.is_some() {
        out.result = CIV_FAILED;
    }
    out
}

// ------------------------------------------------------------------------------------------------ the host

/// `EEApplyCivilizationFile`'s steps: `EELoadCivilization` from the game's (`a` 0) or the user's (`a` 1)
/// directory into the host's name and upgrades (`value` its result)
pub const EE_TECH_CIVF_OP_LOAD: u32 = 1;
/// the host's name (`text`, `characters`) and upgrades (`upgrades`, `count`) as they are now
pub const EE_TECH_CIVF_OP_STATE: u32 = 2;
/// the host's name from string `a` (`gResource->GetString`)
pub const EE_TECH_CIVF_OP_NAME_STRING: u32 = 3;
/// the player's civilization emptied: no upgrades, every point, then none, the host's name
pub const EE_TECH_CIVF_OP_CIV_RESET: u32 = 4;
/// upgrade `a` bought for free: its cost added to the points (`CanAfford`, `SetPointsAndMax`), then `Buy`
pub const EE_TECH_CIVF_OP_CIV_BUY_FREE: u32 = 5;
/// `SetPoints(0)`
pub const EE_TECH_CIVF_OP_CIV_DONE: u32 = 6;
/// `EECBRequestUpgrades`' steps: `gWorld.mGameOptions.CanBuyCivilization()`
pub const EE_TECH_CIVF_OP_CAN_BUY: u32 = 7;
/// the player's `mCivUpgradesPurchased`
pub const EE_TECH_CIVF_OP_PURCHASED: u32 = 8;
/// any civilization is valid: `gAllowCustomCivs`, a scenario game, the scenario editor
pub const EE_TECH_CIVF_OP_ANY_CIV: u32 = 9;
/// `mCivilization.CanAffordAll(the request's upgrades)`
pub const EE_TECH_CIVF_OP_CAN_AFFORD_ALL: u32 = 10;
/// the request granted (`EEMCivUpgradesPurchased` with name id `a`: sent in multiplayer, else executed)
pub const EE_TECH_CIVF_OP_GRANT: u32 = 11;
/// the request refused: the chat line naming the civilization, then the random draw
pub const EE_TECH_CIVF_OP_REFUSE: u32 = 12;

/// A host call.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeTechCivFileCall {
    pub op: u32,
    pub a: i64,
}

/// The host's answer: a value, and for [`EE_TECH_CIVF_OP_STATE`] the name and the upgrades (the host's, live until
/// its next step).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeTechCivFileReply {
    pub value: i64,
    pub text: *const c_char,
    pub characters: u32,
    pub upgrades: *const EeTechCivUpgrade,
    pub count: usize,
}

impl Default for EeTechCivFileReply {
    fn default() -> Self {
        EeTechCivFileReply { value: 0, text: std::ptr::null(), characters: 0, upgrades: std::ptr::null(), count: 0 }
    }
}

/// The C++ side of a file's application or a request (a test host in the tests).
pub trait CivFileHost {
    fn call(&mut self, op: u32, a: i64) -> EeTechCivFileReply;
}

/// The C++ host: a callback.
#[repr(C)]
pub struct EeTechCivFileHost {
    pub ctx: *mut c_void,
    pub call: unsafe extern "C" fn(ctx: *mut c_void, call: *const EeTechCivFileCall, reply: *mut EeTechCivFileReply),
}

impl CivFileHost for EeTechCivFileHost {
    fn call(&mut self, op: u32, a: i64) -> EeTechCivFileReply {
        let mut reply = EeTechCivFileReply::default();
        // SAFETY: the C++ host answers every op on its own objects
        unsafe { (self.call)(self.ctx, &EeTechCivFileCall { op, a }, &mut reply) };
        reply
    }
}

/// The host's name's characters' first bytes and its upgrades, copied.
fn state<H: CivFileHost>(h: &mut H) -> (Vec<c_char>, Vec<EeTechCivUpgrade>) {
    let r = h.call(EE_TECH_CIVF_OP_STATE, 0);
    // SAFETY: the host's name and upgrades, live until its next step
    unsafe {
        let name = civ::character_bytes(r.text, r.characters);
        let upgrades =
            if r.upgrades.is_null() { Vec::new() } else { std::slice::from_raw_parts(r.upgrades, r.count).to_vec() };
        (name, upgrades)
    }
}

/// `EEApplyCivilizationFile`: the civilization of a file (`file`: its name, as the premade civilizations name their
/// files), from the game's directory, else the user's, given to the player with its upgrades bought at no cost; a
/// game file's premade civilization keeps its localized name. The load's result.
pub fn apply_file<H: CivFileHost>(h: &mut H, file: &[u8]) -> i32 {
    let mut result = h.call(EE_TECH_CIVF_OP_LOAD, 0).value as i32;
    if result != 0 {
        result = h.call(EE_TECH_CIVF_OP_LOAD, 1).value as i32;
    } else {
        let (name, upgrades) = state(h);
        let (_, name_id) = civ::tables().matches_premade(file, &name, &upgrades);
        if name_id != 0 {
            h.call(EE_TECH_CIVF_OP_NAME_STRING, name_id as i64);
        }
    }
    if result == 0 {
        h.call(EE_TECH_CIVF_OP_CIV_RESET, 0);
        let (_, upgrades) = state(h);
        for u in upgrades {
            h.call(EE_TECH_CIVF_OP_CIV_BUY_FREE, u as i64);
        }
        h.call(EE_TECH_CIVF_OP_CIV_DONE, 0);
    }
    result
}

/// `EECBRequestUpgrades`: a player's request granted when allowed (one purchase when the options limit it), valid
/// (a premade civilization unless custom ones are allowed, a scenario or the editor) and affordable; otherwise
/// refused. The host's name is the requested civilization's (the premade test reads it as the file's too).
pub fn request_upgrades<H: CivFileHost>(h: &mut H) -> bool {
    let allowed = if h.call(EE_TECH_CIVF_OP_CAN_BUY, 0).value != 0 {
        h.call(EE_TECH_CIVF_OP_PURCHASED, 0).value == 0
    } else {
        true
    };
    let mut name_id = 0;
    let valid = if h.call(EE_TECH_CIVF_OP_ANY_CIV, 0).value == 0 {
        let r = h.call(EE_TECH_CIVF_OP_STATE, 0);
        // SAFETY: the host's name, live until its next step
        let file =
            if r.text.is_null() { Vec::new() } else { unsafe { std::ffi::CStr::from_ptr(r.text) }.to_bytes().to_vec() };
        let (name, upgrades) = state(h);
        let (matches, id) = civ::tables().matches_premade(&file, &name, &upgrades);
        name_id = id;
        matches
    } else {
        true
    };
    if allowed && valid && h.call(EE_TECH_CIVF_OP_CAN_AFFORD_ALL, 0).value != 0 {
        h.call(EE_TECH_CIVF_OP_GRANT, name_id as i64);
        true
    } else {
        h.call(EE_TECH_CIVF_OP_REFUSE, 0);
        false
    }
}

// ------------------------------------------------------------------------------------------------ the C ABI

/// What [`read_file`] gave, for the C++: the result, the name (when set) and the error (when any) as owned bytes
/// the C++ frees with [`ee_tech_civ_file_free`], the upgrades.
#[repr(C)]
pub struct EeTechCivFileRead {
    pub result: i32,
    pub has_name: bool,
    pub name: *mut u8,
    pub name_length: usize,
    pub error: *mut u8,
    pub error_length: usize,
    pub upgrades: *mut EeTechCivUpgrade,
    pub count: usize,
}

fn leak<T: Copy>(v: Vec<T>) -> (*mut T, usize) {
    let b = v.into_boxed_slice();
    let n = b.len();
    (Box::into_raw(b) as *mut T, n)
}

/// `EELoadCivilization`'s bytes ([`read_file`]): `file` the file's name (`<name>.civ`, NUL-terminated).
///
/// # Safety
/// `data` names `size` readable bytes or is null; `file` is NUL-terminated; `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_read_file(
    data: *const u8,
    size: usize,
    file: *const c_char,
    out: *mut EeTechCivFileRead,
) {
    // SAFETY: the caller's contract
    let (bytes, file) = unsafe {
        let bytes = if data.is_null() || size == 0 { &[][..] } else { std::slice::from_raw_parts(data, size) };
        (bytes, std::ffi::CStr::from_ptr(file).to_bytes())
    };
    let read = read_file(&civ::tables(), bytes, file);
    let (name, name_length) = leak(read.name.clone().unwrap_or_default());
    let (error, error_length) = leak(read.error.unwrap_or_default());
    let (upgrades, count) = leak(read.upgrades);
    // SAFETY: the caller's contract
    unsafe {
        *out = EeTechCivFileRead {
            result: read.result,
            has_name: read.name.is_some(),
            name,
            name_length,
            error,
            error_length,
            upgrades,
            count,
        };
    }
}

/// Frees what [`ee_tech_civ_read_file`] gave.
///
/// # Safety
/// `read` came from [`ee_tech_civ_read_file`], once.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_file_free(read: *mut EeTechCivFileRead) {
    // SAFETY: the caller's contract: the boxed slices leak() made
    unsafe {
        let r = &mut *read;
        drop(Box::from_raw(std::ptr::slice_from_raw_parts_mut(r.name, r.name_length)));
        drop(Box::from_raw(std::ptr::slice_from_raw_parts_mut(r.error, r.error_length)));
        drop(Box::from_raw(std::ptr::slice_from_raw_parts_mut(r.upgrades, r.count)));
        r.name_length = 0;
        r.error_length = 0;
        r.count = 0;
    }
}

/// `EEApplyCivilizationFile` over the C++ host ([`apply_file`]).
///
/// # Safety
/// `host` names a live host; `file` is NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_apply_file(host: *mut EeTechCivFileHost, file: *const c_char) -> i32 {
    // SAFETY: the caller's contract
    let file = unsafe { std::ffi::CStr::from_ptr(file) }.to_bytes().to_vec();
    apply_file(unsafe { &mut *host }, &file)
}

/// `EECBRequestUpgrades` over the C++ host ([`request_upgrades`]).
///
/// # Safety
/// `host` names a live host.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_request_upgrades(host: *mut EeTechCivFileHost) -> bool {
    // SAFETY: the caller's contract
    request_upgrades(unsafe { &mut *host })
}
