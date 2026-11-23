//! The game messages' C++ class over the Rust codec (game core lane, round 10, lane C; was EEMRecord.cpp's bodies):
//! EEMessage's export, export size and import over its codec slots, and EEMRecord's constructors (a sender's values
//! into the record's members: ee_netmsg records.rs's layout), member access, rules, codec slots and import. Each C++
//! call or member is a one-statement case of EEMessageCapture.cpp's `EECore10OpC3` (p the message as an
//! NEMessage*).
//!
//! The wire is ee_sim netmsg's; this is the C++ object's side of it (its record bytes, its string, its list).

use std::ffi::{c_ulong, c_void, CStr};

use ee_sim::netmsg::ffi::{
    ee_netmsg_export, ee_netmsg_export_size, ee_netmsg_fixed_size, ee_netmsg_import, EeNetMsgSlot,
};
use ee_sim::netmsg::records::{
    ee_netmsg_record_class, ee_netmsg_record_field, ee_netmsg_record_member, ee_netmsg_record_rules,
    EeNetMsgRecordField, EE_NETMSG_RECORD_FIELD_BLOB, EE_NETMSG_RECORD_FIELD_BOOL, EE_NETMSG_RECORD_FIELD_FLOAT,
    EE_NETMSG_RECORD_FIELD_INTEGER, EE_NETMSG_RECORD_FIELD_LIST, EE_NETMSG_RECORD_FIELD_NONE,
    EE_NETMSG_RECORD_FIELD_RAW, EE_NETMSG_RECORD_FIELD_STRING,
};

use super::{h, hp, ptr, O, P};

/// The codec's slots a message hands over (kNetMsgMaxSlots).
const MR_MAX_SLOTS: usize = 32;
/// EEMRecord::mRecord's size (EE_NETMSG_RECORD_BYTES).
const MR_RECORD_BYTES: usize = 64;

/// A sender's value for one member (EEMRecordValue, ee/EEMessage.h: its layout).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGamecppMrValue {
    pub integer: i64,
    pub floats: [f32; 3],
    pub is_float: bool,
    pub string: *const c_void,
    pub list: *const u8,
    pub list_count: usize,
    pub list_element: usize,
}

fn empty_slots() -> [EeNetMsgSlot; MR_MAX_SLOTS] {
    [EeNetMsgSlot { data: std::ptr::null_mut(), size: 0, count: 0, offset: 0 }; MR_MAX_SLOTS]
}

fn verify(ok: bool, line: i64) {
    if !ok {
        h(O::Verify, 0, &[0, line]);
    }
}

fn message_type(m: P) -> u8 {
    h(O::LcMsgType, m, &[]) as u8
}

fn record(m: P) -> *mut u8 {
    hp(O::LcMrRecord, m, &[]) as *mut u8
}

/// The C text's pointer and length.
fn c_text(text: P) -> (*mut u8, u32) {
    // SAFETY: a C string the C++ answered
    let n = unsafe { CStr::from_ptr(text as *const std::ffi::c_char) }.to_bytes().len();
    (text as *mut u8, n as u32)
}

// ---- EEMessage's codec (p the message: its virtual GetCodecSlots)

fn codec_slots(m: P, slots: &mut [EeNetMsgSlot; MR_MAX_SLOTS]) -> (u32, usize) {
    let mut class: u32 = 0;
    let n = h(O::LcEmCodecSlots, m, &[slots.as_mut_ptr() as usize as i64, ptr(&mut class)]) as usize;
    (class, n)
}

/// EEMessage::ExportMessage: the codec's bytes in a new[] block (the caller deletes it), and their size.
///
/// # Safety
/// `out` and `out_size` are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_msg_export(message: *mut c_void, out: *mut *mut u8, out_size: *mut c_ulong) {
    let m = message as P;
    let mut slots = empty_slots();
    let (class, n) = codec_slots(m, &mut slots);
    // SAFETY: the message's slots
    let size = unsafe { ee_netmsg_export_size(class, slots.as_ptr(), n) };
    let data = hp(O::LcBytesNew, 0, &[size as i64]) as *mut u8;
    // SAFETY: the caller's out values; the block of `size` bytes
    unsafe {
        *out = data;
        *out_size = ee_netmsg_export(class, slots.as_ptr(), n, data, size as usize) as c_ulong;
    }
}

/// EEMessage::ExportSize: the class's fixed size, else the codec's size of the members.
#[no_mangle]
pub extern "C" fn ee_gamecpp_msg_export_size(message: *mut c_void) -> c_ulong {
    let mut slots = empty_slots();
    let (class, n) = codec_slots(message as P, &mut slots);
    let fixed = ee_netmsg_fixed_size(class);
    // SAFETY: the message's slots
    (if fixed != 0 { fixed } else { unsafe { ee_netmsg_export_size(class, slots.as_ptr(), n) } }) as c_ulong
}

/// EEMessage::ImportMessage: the codec's import into the members.
///
/// # Safety
/// `data` holds the received message (NULL fails the check).
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_msg_import(message: *mut c_void, data: *mut u8) {
    verify(!data.is_null(), 64);
    let mut slots = empty_slots();
    let (class, n) = codec_slots(message as P, &mut slots);
    // SAFETY: the received bytes and the message's slots
    unsafe { ee_netmsg_import(class, data, slots.as_mut_ptr(), n) };
}

// ---- EEMRecord (p the record as an NEMessage*)

/// Member `index`'s address in the record, its kind and size verified (EEMRecord::Field).
fn field_at(m: P, index: u32, size: usize) -> *mut u8 {
    let f = ee_netmsg_record_member(message_type(m), index);
    verify(f.kind != EE_NETMSG_RECORD_FIELD_NONE && f.size as usize == size, 143);
    // SAFETY: an offset inside the record's bytes
    unsafe { record(m).add(f.offset as usize) }
}

/// EEMRecord::Field.
#[no_mangle]
pub extern "C" fn ee_gamecpp_mrecord_field(message: *const c_void, index: u32, size: usize) -> *mut u8 {
    field_at(message as P, index, size)
}

fn get_u32(m: P, index: u32) -> u32 {
    // SAFETY: a 4-byte member of the record
    unsafe { std::ptr::read_unaligned(field_at(m, index, 4) as *const u32) }
}

/// A sender's value as the member's integer, float and bool (an integer truncated, a float's integer part).
pub fn value_forms(v: &EeGamecppMrValue) -> (i64, f32, bool) {
    let float = if v.is_float { v.floats[0] } else { v.integer as f32 };
    let integer = if v.is_float { v.floats[0] as i64 } else { v.integer };
    let boolean = integer != 0 || (v.is_float && v.floats[0] != 0.0);
    (integer, float, boolean)
}

/// The bytes a fixed member of `kind` and `size` takes from a value (None: the member is not in the record).
pub fn member_bytes(kind: u8, size: usize, v: &EeGamecppMrValue) -> Option<Vec<u8>> {
    let (integer, float, boolean) = value_forms(v);
    match kind {
        EE_NETMSG_RECORD_FIELD_STRING | EE_NETMSG_RECORD_FIELD_LIST | EE_NETMSG_RECORD_FIELD_BLOB => None,
        EE_NETMSG_RECORD_FIELD_RAW => None,
        EE_NETMSG_RECORD_FIELD_INTEGER => Some(integer.to_le_bytes()[..size].to_vec()),
        EE_NETMSG_RECORD_FIELD_BOOL => Some(vec![boolean as u8]),
        EE_NETMSG_RECORD_FIELD_FLOAT => Some(float.to_le_bytes().to_vec()),
        _ => Some(v.floats.iter().flat_map(|x| x.to_le_bytes()).take(size).collect()), // a point or a vector
    }
}

/// EEMRecord(type, values): the record zeroed, then each value into its member in wire order (the string and the
/// list beside the record; a blob or a raw member left to its sender).
///
/// # Safety
/// `values` points at `count` values (their strings and lists alive for the call).
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_mrecord_init(message: *mut c_void, values: *const EeGamecppMrValue, count: usize) {
    let m = message as P;
    let t = message_type(m);
    // SAFETY: the record's bytes
    unsafe { std::ptr::write_bytes(record(m), 0, MR_RECORD_BYTES) };
    verify(ee_netmsg_record_class(t) != u32::MAX, 98);
    let values = if count == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(values, count) } };
    for (i, v) in values.iter().enumerate() {
        let index = i as u32 + 1;
        let f: EeNetMsgRecordField = ee_netmsg_record_member(t, index);
        let beside =
            matches!(f.kind, EE_NETMSG_RECORD_FIELD_STRING | EE_NETMSG_RECORD_FIELD_LIST | EE_NETMSG_RECORD_FIELD_BLOB);
        let to = if beside { std::ptr::null_mut() } else { field_at(m, index, f.size as usize) };
        match f.kind {
            EE_NETMSG_RECORD_FIELD_STRING => {
                verify(!v.string.is_null(), 111);
                h(O::LcMrSetString, m, &[v.string as usize as i64]);
            }
            EE_NETMSG_RECORD_FIELD_LIST => {
                verify(!v.list.is_null() && v.list_element >= f.size as usize, 115);
                for k in 0..v.list_count {
                    // SAFETY: the sender's list element
                    let element = unsafe { v.list.add(k * v.list_element) };
                    h(O::LcMrListAppend, m, &[element as usize as i64, f.size as i64]);
                }
            }
            _ => {
                if let Some(b) = member_bytes(f.kind, f.size as usize, v) {
                    // SAFETY: the member's bytes in the record
                    unsafe { std::ptr::copy_nonoverlapping(b.as_ptr(), to, b.len()) };
                }
            }
        }
    }
}

/// EEMRecord::GetCodecSlots: the header's four slots, the fast selection id, then each wire member (the string's
/// text, the list's records, the blob's bytes, the record's members).
///
/// # Safety
/// `slots` has room for 32 (`MR_MAX_SLOTS`); `class` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_mrecord_slots(
    message: *mut c_void,
    slots: *mut EeNetMsgSlot,
    class: *mut u32,
) -> usize {
    let m = message as P;
    let c = ee_netmsg_record_class(message_type(m));
    // SAFETY: the caller's out values
    let slots = unsafe {
        *class = c;
        std::slice::from_raw_parts_mut(slots, MR_MAX_SLOTS)
    };
    h(O::LcMrHeaderSlots, m, &[slots.as_mut_ptr() as usize as i64]);
    slots[4] = EeNetMsgSlot { data: hp(O::LcMrFastId, m, &[]) as *mut u8, size: 1, count: 0, offset: 0 };
    let rec = record(m);
    let mut n = 5;
    loop {
        let f = ee_netmsg_record_field(c, n as u32 - 4);
        if f.kind == EE_NETMSG_RECORD_FIELD_NONE {
            break;
        }
        // SAFETY: an offset inside the record
        let mut slot = EeNetMsgSlot { data: unsafe { rec.add(f.offset as usize) }, size: f.size, count: 0, offset: 0 };
        if f.kind == EE_NETMSG_RECORD_FIELD_STRING {
            let (data, size) = c_text(hp(O::LcMrStringText, m, &[]));
            slot = EeNetMsgSlot { data, size, count: 0, offset: 0 };
        } else if f.kind == EE_NETMSG_RECORD_FIELD_LIST || f.kind == EE_NETMSG_RECORD_FIELD_BLOB {
            let bytes = h(O::LcMrListSize, m, &[]) as u32;
            slot.data = hp(O::LcMrListData, m, &[]) as *mut u8;
            slot.size = if f.kind == EE_NETMSG_RECORD_FIELD_LIST { f.size } else { bytes };
            slot.count = if f.kind == EE_NETMSG_RECORD_FIELD_LIST { bytes / f.size } else { 0 };
        }
        slots[n] = slot;
        n += 1;
    }
    n
}

/// EEMRecord::ImportMessage (and the factory's constructor, `fresh`: the record zeroed first): the codec, then the
/// string's text and the list's or blob's bytes from where the codec found them.
///
/// # Safety
/// `data` holds the received message (NULL fails the check).
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_mrecord_import(message: *mut c_void, data: *mut u8, fresh: bool) {
    let m = message as P;
    if fresh {
        // SAFETY: the record's bytes
        unsafe { std::ptr::write_bytes(record(m), 0, MR_RECORD_BYTES) };
    }
    verify(!data.is_null(), 205);
    h(O::LcMrListClear, m, &[]);
    let mut slots = empty_slots();
    let mut class: u32 = 0;
    // SAFETY: room for every slot
    let n = unsafe { ee_gamecpp_mrecord_slots(message, slots.as_mut_ptr(), &mut class) };
    // SAFETY: the received bytes and the slots
    unsafe { ee_netmsg_import(class, data, slots.as_mut_ptr(), n) };
    for i in 5..n {
        let f = ee_netmsg_record_field(class, i as u32 - 4);
        // SAFETY: the member's place in the received bytes
        let from = unsafe { data.add(slots[i].offset as usize) };
        let from_arg = from as usize as i64;
        match f.kind {
            EE_NETMSG_RECORD_FIELD_STRING => {
                h(O::LcMrStringImport, m, &[from_arg]);
            }
            EE_NETMSG_RECORD_FIELD_LIST => {
                h(O::LcMrListAssign, m, &[from_arg, (slots[i].count * f.size) as i64]);
            }
            EE_NETMSG_RECORD_FIELD_BLOB => {
                h(O::LcMrListAssign, m, &[from_arg, slots[i].count as i64]);
            }
            _ => {}
        }
    }
}

/// GetSendToAllAndWaitMessageID: the type's wait member (false when it has none).
///
/// # Safety
/// `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_mrecord_wait_id(message: *const c_void, out: *mut c_ulong) -> bool {
    let m = message as P;
    let rules = ee_netmsg_record_rules(message_type(m));
    if rules.wait_field != 0 {
        // SAFETY: the caller's out value
        unsafe { *out = get_u32(m, rules.wait_field as u32) as c_ulong };
    }
    rules.wait_field != 0
}

/// SetSendToAllAndWaitMessageID: the type's wait member, if it has one.
#[no_mangle]
pub extern "C" fn ee_gamecpp_mrecord_set_wait_id(message: *mut c_void, id: c_ulong) {
    let m = message as P;
    let rules = ee_netmsg_record_rules(message_type(m));
    if rules.wait_field != 0 {
        let to = field_at(m, rules.wait_field as u32, 4);
        // SAFETY: a 4-byte member of the record
        unsafe { std::ptr::write_unaligned(to as *mut u32, id as u32) };
    }
}

/// IsResponse: the type's rule (the response's type, this message's and the response's members equal as int32).
#[no_mangle]
pub extern "C" fn ee_gamecpp_mrecord_is_response(message: *mut c_void, other: *mut c_void) -> bool {
    let (m, o) = (message as P, other as P);
    let rules = ee_netmsg_record_rules(message_type(m));
    rules.response_type != 0
        && message_type(o) == rules.response_type
        && get_u32(m, rules.response_mine as u32) == get_u32(o, rules.response_theirs as u32)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn value(integer: i64, floats: [f32; 3], is_float: bool) -> EeGamecppMrValue {
        EeGamecppMrValue {
            integer,
            floats,
            is_float,
            string: std::ptr::null(),
            list: std::ptr::null(),
            list_count: 0,
            list_element: 0,
        }
    }

    #[test]
    fn a_value_converts_as_the_old_constructors_did() {
        assert_eq!(value_forms(&value(-3, [0.0; 3], false)), (-3, -3.0, true));
        assert_eq!(value_forms(&value(0, [2.75, 0.0, 0.0], true)), (2, 2.75, true));
        assert_eq!(value_forms(&value(0, [0.5, 0.0, 0.0], true)), (0, 0.5, true));
        assert_eq!(value_forms(&value(0, [0.0; 3], false)), (0, 0.0, false));
    }

    #[test]
    fn the_members_take_their_low_bytes() {
        let v = value(0x1234_5678, [0.0; 3], false);
        assert_eq!(member_bytes(EE_NETMSG_RECORD_FIELD_INTEGER, 2, &v), Some(vec![0x78, 0x56]));
        assert_eq!(member_bytes(EE_NETMSG_RECORD_FIELD_BOOL, 1, &v), Some(vec![1]));
        assert_eq!(member_bytes(EE_NETMSG_RECORD_FIELD_STRING, 0, &v), None);
        let p = value(0, [1.0, 2.0, 3.0], true);
        assert_eq!(member_bytes(5, 12, &p).map(|b| b.len()), Some(12));
        assert_eq!(member_bytes(EE_NETMSG_RECORD_FIELD_FLOAT, 4, &p), Some(1.0f32.to_le_bytes().to_vec()));
    }

    #[test]
    fn the_value_layout_is_the_cpp_struct() {
        assert_eq!(std::mem::offset_of!(EeGamecppMrValue, floats), 8);
        assert_eq!(std::mem::offset_of!(EeGamecppMrValue, is_float), 20);
        assert_eq!(std::mem::offset_of!(EeGamecppMrValue, string), 24);
        assert_eq!(std::mem::size_of::<EeGamecppMrValue>(), 24 + 4 * std::mem::size_of::<usize>());
    }
}
