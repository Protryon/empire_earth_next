//! TEST ONLY: the game messages' capture records (was EEMessageCapture.cpp; ee/EEMessageCapture.h;
//! tools/sim_capture.py; docs/RUST_PLAN.md R5b.4). Reads the messages through the cases of EEMessageCapture.cpp's
//! `EECore10OpC3`; writes nothing to the world.
//!
//! - NMIM: every game message the factory builds (EEMessage::BuildEEMessage): its type, ExportSize, the bytes it was
//!   built from, the bytes it exports again, then its members as Import filled them.
//! - NMEX: every game message the physics executes: its type, ExportSize, the bytes it exports, its members; then the
//!   message is built again from those bytes (an NMIM record).
//! - NMSW: the same as NMIM for the sweep's game messages (ee_sim netmsg capture.rs's edge payloads).
//! - NMCU: the box selection's packing (the sweep's items, written whole by the Rust).
//! - NMNE: the engine messages over the sweep's edge payloads, built by NEMessage::BuildNEMessage: the bytes in, the
//!   export, the members as Import filled them (the communications ticks' nested inputs in their order).
//!
//! A record committed while another is open (the NMIM of a message an NMEX or the sweep builds) is written first,
//! as the C++'s were: each record is committed when it is complete.

use std::ffi::{c_long, c_ulong, CStr};
use std::sync::atomic::{AtomicBool, Ordering};

use ee_base::plat_core::string::PlatMbsLen;
use ee_sim::netmsg::capture::{
    ee_netmsg_capture_sweep, EE_NETMSG_SWEEP_ITEM_ENGINE, EE_NETMSG_SWEEP_ITEM_GAME, EE_NETMSG_SWEEP_NMCU,
};
use ee_sim::netmsg::classes::EE_NETMSG_SELECT_UNITS;
use ee_sim::netmsg::ffi::{
    ee_netmsg_class_of_type, ee_netmsg_field_kind, ee_netmsg_import_fields, EeNetMsgSlot, EE_NETMSG_FIELD_RECORDS,
    EE_NETMSG_FIELD_STRING,
};
use ee_sim::netmsg::ne_records::{
    ee_netmsg_ne_field, ee_netmsg_ne_type, EE_NETMSG_NE_FIELD_INPUTS, EE_NETMSG_NE_FIELD_STRING,
};

use super::calamity_capture::Record;
use super::{h, hp, pa, ptr, Owned, O, P};

/// The capture tags (EECaptureTag order: the first letter in the low byte).
const MC_NMSW: u32 = 0x5753_4d4e;
const MC_NMIM: u32 = 0x4d49_4d4e;
const MC_NMEX: u32 = 0x5845_4d4e;
const MC_NMNE: u32 = 0x454e_4d4e;

/// While the sweep builds a game message (its NMIM record is an NMSW).
static SWEEPING: AtomicBool = AtomicBool::new(false);
/// While the engine sweep builds a message (the nested game inputs are in its NMNE record).
static ENGINE_SWEEP: AtomicBool = AtomicBool::new(false);

fn active() -> bool {
    ee_sim::app::capture::ee_app_capture_active()
}

/// `count` bytes at `data` (none when empty).
fn bytes(r: &mut Record, data: *const u8, count: usize) {
    if count > 0 {
        // SAFETY: the C++'s bytes, read before its next call
        r.bytes(unsafe { std::slice::from_raw_parts(data, count) });
    }
}

/// A UWideString: its bytes' length, the bytes, its length in characters.
fn wide(r: &mut Record, string: P) {
    let text = hp(O::LcWideText, string, &[]);
    // SAFETY: the string's C text
    let b = unsafe { CStr::from_ptr(text as *const std::ffi::c_char) }.to_bytes();
    r.u32(b.len() as u32);
    r.bytes(b);
    r.u32(h(O::LcWideLength, string, &[]) as u32);
}

/// The message's ExportMessage bytes as their length and the bytes (an empty export: the length only).
fn exported_bytes(r: &mut Record, m: P) -> Vec<u8> {
    let mut data: *mut u8 = std::ptr::null_mut();
    let mut size: c_ulong = 0;
    h(O::LcMsgExport, m, &[ptr(&mut data), ptr(&mut size)]);
    r.u32(size as u32);
    let mut copy = Vec::new();
    if !data.is_null() {
        // SAFETY: the export's new[] bytes, freed after the copy
        copy = unsafe { std::slice::from_raw_parts(data, size as usize) }.to_vec();
        r.bytes(&copy);
        h(O::LcBytesDelete, data as P, &[]);
    }
    copy
}

/// The message's members as Import filled them (0xffffffff for a type the factory does not build), and a box
/// selection's unit ids.
fn members(r: &mut Record, m: P) {
    let message_type = h(O::LcMsgType, m, &[]) as u8;
    r.u32(message_type as u32);
    r.u32(h(O::LcMsgForSlot, m, &[]) as u32);
    r.u32(h(O::LcMsgOrigSlot, m, &[]) as u32);
    if h(O::LcMsgIsGame, m, &[]) == 0 || ee_netmsg_class_of_type(message_type) == u32::MAX {
        r.u32(u32::MAX);
        return;
    }
    let mut slots = [EeNetMsgSlot { data: std::ptr::null_mut(), size: 0, count: 0, offset: 0 }; 32];
    let mut class: u32 = 0;
    h(O::LcMsgCodecSlots, m, &[slots.as_mut_ptr() as usize as i64, ptr(&mut class)]);
    for i in 0..ee_netmsg_import_fields(class) {
        let slot = slots[4 + i as usize];
        match ee_netmsg_field_kind(class, i) {
            EE_NETMSG_FIELD_STRING => {
                r.u32(slot.size);
                bytes(r, slot.data, slot.size as usize);
                // SAFETY: the string's bytes end in a NUL (UWideString::GetLength)
                r.u32(unsafe { PlatMbsLen(slot.data) } as u32);
            }
            EE_NETMSG_FIELD_RECORDS => {
                r.u32(slot.count);
                bytes(r, slot.data, (slot.count * slot.size) as usize);
            }
            _ => bytes(r, slot.data, slot.size as usize), // fixed, blob
        }
    }
    if class == EE_NETMSG_SELECT_UNITS {
        let ids = Owned::new(O::LcLongVecNew, O::LcLongVecDelete);
        h(O::LcMsgSelectIDs, m, &[pa(ids.0)]);
        let n = h(O::LcLongVecSize, ids.0, &[]);
        r.u32(n as u32);
        for i in 0..n {
            r.i32(h(O::LcLongVecAt, ids.0, &[i]) as c_long as i32);
        }
    }
}

/// NMIM (NMSW in the sweep): a game message the factory built from `data`.
#[no_mangle]
pub extern "C" fn ee_gamecpp_msgcap_import(message: *mut std::ffi::c_void, data: *const u8) {
    let m = message as P;
    if m == 0 || !active() || ENGINE_SWEEP.load(Ordering::Relaxed) {
        return;
    }
    let mut r = Record::default();
    let size = h(O::LcMsgExportSize, m, &[]) as c_ulong;
    r.u32(h(O::LcMsgType, m, &[]) as u32);
    r.u32(size as u32);
    bytes(&mut r, data, size as u32 as usize);
    exported_bytes(&mut r, m);
    members(&mut r, m);
    r.commit_tag(if SWEEPING.load(Ordering::Relaxed) { MC_NMSW } else { MC_NMIM });
}

/// Builds a game message from a copy of `data` (64 bytes of slack past it); NULL when none.
fn build_game(data: &[u8]) -> P {
    let mut copy = data.to_vec();
    copy.resize(copy.len() + 64, 0);
    hp(O::LcMsgBuildGame, 0, &[copy.as_mut_ptr() as usize as i64])
}

/// Lets a built message go.
fn release(m: P) {
    if m != 0 {
        h(O::LcMsgDereference, m, &[]);
    }
}

/// NMEX: a game message the physics executes (types 0x41..0xfd), then built again from its export (its NMIM).
#[no_mangle]
pub extern "C" fn ee_gamecpp_msgcap_executed(message: *mut std::ffi::c_void) {
    let m = message as P;
    if m == 0 || !active() {
        return;
    }
    let message_type = h(O::LcMsgType, m, &[]);
    if !(0x41..=0xfd).contains(&message_type) {
        return;
    }
    let mut r = Record::default();
    r.u32(message_type as u32);
    r.u32(h(O::LcMsgExportSize, m, &[]) as u32);
    let mut data: *mut u8 = std::ptr::null_mut();
    let mut size: c_ulong = 0;
    h(O::LcMsgExport, m, &[ptr(&mut data), ptr(&mut size)]);
    r.u32(size as u32);
    bytes(&mut r, data, size as usize);
    members(&mut r, m);
    r.commit_tag(MC_NMEX);
    let copy = if data.is_null() {
        Vec::new()
    } else {
        // SAFETY: the export's bytes
        unsafe { std::slice::from_raw_parts(data, size as usize) }.to_vec()
    };
    h(O::LcBytesDelete, data as P, &[]);
    release(build_game(&copy));
}

/// An engine message's export: its type, ExportSize, the export's bytes.
fn exported(r: &mut Record, m: P) {
    r.u32(h(O::LcMsgType, m, &[]) as u32);
    r.u32(h(O::LcMsgExportSize, m, &[]) as u32);
    exported_bytes(r, m);
}

/// A communications tick's player inputs: their count, then each (an engine message whole, a game message's export
/// and members).
fn inputs(r: &mut Record, m: P) {
    let n = h(O::LcNeInputs, m, &[]);
    r.u32(n as u32);
    for i in 0..n {
        let input = hp(O::LcNeInputAt, m, &[i]);
        let input_type = h(O::LcMsgType, input, &[]);
        if (0x20..=0x3c).contains(&input_type) {
            engine_message(r, input);
        } else {
            exported(r, input);
            members(r, input);
        }
    }
}

/// An engine message: its export, its slots, its wire members (strings, inputs, raw bytes).
fn engine_message(r: &mut Record, m: P) {
    exported(r, m);
    r.u32(h(O::LcMsgForSlot, m, &[]) as u32);
    r.u32(h(O::LcMsgOrigSlot, m, &[]) as u32);
    let message_type = h(O::LcMsgType, m, &[]) as u8;
    for i in 0..ee_netmsg_ne_type(message_type).wire_fields {
        let field = ee_netmsg_ne_field(message_type, i);
        if field.kind == EE_NETMSG_NE_FIELD_STRING {
            wide(r, hp(O::LcNeString, m, &[i as i64]));
        } else if field.kind == EE_NETMSG_NE_FIELD_INPUTS {
            inputs(r, m);
        } else {
            let record = hp(O::LcNeRecord, m, &[]) as *const u8;
            // SAFETY: the record's member, inside its new[] block
            bytes(r, unsafe { record.add(field.offset as usize) }, field.size as usize);
        }
    }
}

/// NMNE: an engine message built from the sweep's bytes (0xffffffff when the engine builds none).
fn build_engine(data: &[u8]) {
    let mut copy = data.to_vec();
    copy.resize(copy.len() + 64, 0);
    let mut r = Record::default();
    r.u32(data.len() as u32);
    r.bytes(data);
    ENGINE_SWEEP.store(true, Ordering::Relaxed);
    let m = hp(O::LcMsgBuildEngine, 0, &[copy.as_mut_ptr() as usize as i64]);
    ENGINE_SWEEP.store(false, Ordering::Relaxed);
    if m != 0 {
        engine_message(&mut r, m);
        release(m);
    } else {
        r.u32(u32::MAX);
    }
    r.commit_tag(MC_NMNE);
}

/// One item of the sweep: a game message (NMSW), an engine message (NMNE) or a packing record (NMCU) of its bytes.
unsafe extern "C" fn sweep_item(kind: u8, data: *const u8, size: usize) {
    let data = if size > 0 {
        // SAFETY: the sweep's bytes for the call
        unsafe { std::slice::from_raw_parts(data, size) }
    } else {
        &[]
    };
    if kind == EE_NETMSG_SWEEP_ITEM_GAME {
        SWEEPING.store(true, Ordering::Relaxed);
        let m = build_game(data);
        SWEEPING.store(false, Ordering::Relaxed);
        release(m);
    } else if kind == EE_NETMSG_SWEEP_ITEM_ENGINE {
        build_engine(data);
    } else {
        let mut r = Record::default();
        r.bytes(data);
        r.commit_tag(EE_NETMSG_SWEEP_NMCU);
    }
}

/// The sweep (EEMessageCaptureSweep): ee_sim's edge payloads, each item built.
#[no_mangle]
pub extern "C" fn ee_gamecpp_msgcap_sweep() {
    if !active() {
        return;
    }
    // SAFETY: our item sink
    unsafe { ee_netmsg_capture_sweep(sweep_item) };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_tags_are_the_capture_names() {
        let tag = |s: &[u8; 4]| u32::from_le_bytes(*s);
        assert_eq!(MC_NMSW, tag(b"NMSW"));
        assert_eq!(MC_NMIM, tag(b"NMIM"));
        assert_eq!(MC_NMEX, tag(b"NMEX"));
        assert_eq!(MC_NMNE, tag(b"NMNE"));
    }

    #[test]
    fn empty_bytes_append_nothing() {
        let mut r = Record::default();
        bytes(&mut r, std::ptr::null(), 0);
        assert!(r.payload().is_empty());
    }
}
