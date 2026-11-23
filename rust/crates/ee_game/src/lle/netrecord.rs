//! `NEMRecord`'s bodies (LLE round 8, from `src/Low-Level Engine/Network Engine/NEMessage.cpp`): the engine
//! messages' one data class. The record's layout, flags and rules were Rust already (ee_sim netmsg ne_records.rs),
//! the wire form too (the codec, netmsg ffi.rs); what moved here is the class's own work over them: the record's
//! allocation and filling, the members' reads, the send-to-all-and-wait id, the response rule, the players' inputs,
//! and the export and import through the codec's slots.
//!
//! The class stays C++ (`lle/NEMessage.h`: the game's servers and clients make and read it); its members (the
//! record's bytes, the two `UWideString`s, the inputs' vector of `NEMessage*`) are reached through one-statement shims
//! of `NEMessage.cpp` ([`RecordCpp`]), the inputs' virtuals by call. Only the LAN game sends engine messages
//! (single-player games hand objects straight to the physics).

use core::ffi::{c_char, c_long, c_ulong, c_void};

use ee_sim::netmsg::ffi::EeNetMsgSlot;
use ee_sim::netmsg::ne_records::{
    ee_netmsg_ne_field, ee_netmsg_ne_type, EeNetMsgNeField, EE_NETMSG_NE_FIELD_BOOL, EE_NETMSG_NE_FIELD_INPUTS,
    EE_NETMSG_NE_FIELD_INTEGER, EE_NETMSG_NE_FIELD_NONE, EE_NETMSG_NE_FIELD_STRING, EE_NETMSG_NE_NONE,
};

ee_base::cpp_shims! {
    trait RecordCpp;
    fn message_type(record: usize) -> u8 = ee_nemcpp_type;
    fn set_type(record: usize, message_type: u8) = ee_nemcpp_set_type;
    fn record_slot(record: usize) -> *mut *mut u8 = ee_nemcpp_record_slot;
    fn bytes_new(size: usize) -> *mut u8 = ee_nemcpp_bytes_new;
    fn bytes_delete(bytes: *mut u8) = ee_nemcpp_bytes_delete;
    fn header_slots(record: usize, slots: *mut EeNetMsgSlot) = ee_nemcpp_header_slots;
    fn string_set(record: usize, ordinal: u32, text: *const c_char) = ee_nemcpp_string_set;
    fn string_chars(record: usize, ordinal: u32) -> *const c_char = ee_nemcpp_string_chars;
    fn string_at(record: usize, ordinal: u32) -> usize = ee_nemcpp_string_at;
    fn string_import(record: usize, ordinal: u32, data: *const u8) = ee_nemcpp_string_import;
    fn inputs_size(record: usize) -> usize = ee_nemcpp_inputs_size;
    fn inputs_at(record: usize, index: usize) -> usize = ee_nemcpp_inputs_at;
    fn inputs_back(record: usize) -> usize = ee_nemcpp_inputs_back;
    fn inputs_pop(record: usize) = ee_nemcpp_inputs_pop;
    fn inputs_push(record: usize, input: usize) = ee_nemcpp_inputs_push;
    fn message_type_of(message: usize) -> u8 = ee_nemcpp_type_of;
    fn message_dereference(message: usize) = ee_nemcpp_dereference;
    fn message_export(message: usize, data: *mut *mut u8, size: *mut c_ulong) = ee_nemcpp_export;
    fn message_export_size(message: usize) -> c_ulong = ee_nemcpp_export_size;
    fn message_from_memory(data: *const u8) -> usize = ee_nemcpp_from_memory;
    fn record_new(data: *mut u8) -> usize = ee_nemcpp_record_new;
}

/// `kErrorOK`, `kErrorBadPath`, `kErrorBeyondEnd`.
const OK: c_long = 0;
/// (round 8 had 0x10 and 6, kErrorNotReadable and kErrorBusy: tests/net's ne_message_test golden has the original's)
const BAD_PATH: c_long = ee_base::lle_enums::ESError::kErrorBadPath as c_long;
const BEYOND_END: c_long = ee_base::lle_enums::ESError::kErrorBeyondEnd as c_long;
/// The codec's slots: the header's 4 and the members'.
pub const SLOTS: usize = 12;

/// The verifies' file (the original's `__FILE__`).
#[cfg(not(test))]
const NEMESSAGE_CPP: &core::ffi::CStr = c"C:\\EEX\\Low-Level Engine\\Network Engine\\NEMessage.cpp";

/// `ESVERIFY_AT` of NEMessage.cpp (no line: the owner dropped line-number parity).
fn verify(ok: bool, expression: &core::ffi::CStr) {
    verify_at(ok, 0, expression)
}

/// `ESVERIFY_AT` of NEMessage.cpp at the original's line (tests/net's golden_message.txt traces the factory's).
fn verify_at(ok: bool, line: core::ffi::c_ulong, expression: &core::ffi::CStr) {
    if ok {
        return;
    }
    #[cfg(not(test))]
    // SAFETY: C strings
    unsafe {
        ee_base::error::ee_error_verify(NEMESSAGE_CPP.as_ptr(), line, expression.as_ptr())
    };
    #[cfg(test)]
    panic!("verify: {expression:?} {line}");
}

fn field(t: u8, i: u32) -> EeNetMsgNeField {
    ee_netmsg_ne_field(t, i)
}

/// `NEMRecord(inData)`'s body: the type at `data[1]` sizes the zeroed record, then the import.
///
/// # Safety
/// `data` holds a received message of an engine type.
pub unsafe fn construct_from<C: RecordCpp>(c: &mut C, record: usize, data: *mut u8) {
    let t = unsafe { *data.add(1) };
    c.set_type(record, t);
    let size = ee_netmsg_ne_type(t).record_size as usize + 1;
    let bytes = c.bytes_new(size);
    unsafe {
        core::ptr::write_bytes(bytes, 0, size);
        *c.record_slot(record) = bytes;
        import(c, record, data);
    }
}

/// `Fill`: the zeroed record, then the fixed members in record order (an integer truncated to its member's size, a
/// bool as != 0) and the strings in theirs; every value and string must be used.
///
/// # Safety
/// `record` is the C++ object under construction (its type set).
pub unsafe fn fill<C: RecordCpp>(c: &mut C, record: usize, values: &[i64], strings: &[*const c_char]) {
    let t = c.message_type(record);
    let ty = ee_netmsg_ne_type(t);
    verify(ty.class_id != u32::MAX, c"theType.class_id != 0xffffffffu");
    let size = ty.record_size as usize + 1;
    let bytes = c.bytes_new(size);
    unsafe {
        core::ptr::write_bytes(bytes, 0, size);
        *c.record_slot(record) = bytes;
    }
    let (mut v, mut s) = (values.iter(), strings.iter());
    let (mut v_left, mut s_left) = (values.len(), strings.len());
    for i in 0..ty.fields {
        let f = field(t, i);
        if f.kind == EE_NETMSG_NE_FIELD_STRING && s_left > 0 {
            s_left -= 1;
            c.string_set(record, f.offset, *s.next().expect("a string"));
        } else if (f.kind == EE_NETMSG_NE_FIELD_INTEGER || f.kind == EE_NETMSG_NE_FIELD_BOOL) && v_left > 0 {
            v_left -= 1;
            let mut value = *v.next().expect("a value");
            if f.kind == EE_NETMSG_NE_FIELD_BOOL {
                value = (value != 0) as i64;
            }
            let le = value.to_le_bytes();
            // SAFETY: the member lies inside the record (ne_records.rs' layout)
            unsafe { core::ptr::copy_nonoverlapping(le.as_ptr(), bytes.add(f.offset as usize), f.size as usize) };
        }
    }
    verify(v_left == 0 && s_left == 0, c"inValue == inValuesEnd && inString == inStringsEnd");
}

/// `Field`: member `index`'s bytes (its kind and size verified).
pub fn field_at<C: RecordCpp>(c: &mut C, record: usize, index: u32, size: usize) -> *mut u8 {
    let f = field(c.message_type(record), index);
    verify(
        f.kind != EE_NETMSG_NE_FIELD_NONE && f.size as usize == size && size != 0,
        c"theField.kind != EE_NETMSG_NE_FIELD_NONE && theField.size == inSize && inSize != 0",
    );
    // SAFETY: the record's member
    unsafe { (*c.record_slot(record)).add(f.offset as usize) }
}

fn get_u32<C: RecordCpp>(c: &mut C, record: usize, index: u32) -> u32 {
    let p = field_at(c, record, index, 4);
    // SAFETY: a 4-byte member
    unsafe { core::ptr::read_unaligned(p as *const u32) }
}

/// `Value`: member `index` zero-extended; the inputs' count; a block's address.
pub fn value<C: RecordCpp>(c: &mut C, record: usize, index: u32) -> usize {
    let f = field(c.message_type(record), index);
    if f.kind == EE_NETMSG_NE_FIELD_INPUTS {
        return c.inputs_size(record);
    }
    if f.size as usize > 8 {
        return field_at(c, record, index, f.size as usize) as usize;
    }
    let p = field_at(c, record, index, f.size as usize);
    let mut v = [0u8; 8];
    // SAFETY: the member's bytes
    unsafe { core::ptr::copy_nonoverlapping(p, v.as_mut_ptr(), f.size as usize) };
    u64::from_le_bytes(v) as usize
}

/// `GetString`: member `index`'s string (its kind verified), as the C++ object's address.
pub fn string<C: RecordCpp>(c: &mut C, record: usize, index: u32) -> usize {
    let f = field(c.message_type(record), index);
    verify(f.kind == EE_NETMSG_NE_FIELD_STRING, c"theField.kind == EE_NETMSG_NE_FIELD_STRING");
    c.string_at(record, f.offset)
}

/// `~NEMRecord`: every player input released (from the last), the record freed.
pub fn destruct<C: RecordCpp>(c: &mut C, record: usize) {
    while c.inputs_size(record) > 0 {
        let input = c.inputs_back(record);
        c.message_dereference(input);
        c.inputs_pop(record);
    }
    // SAFETY: the record's member
    let bytes = unsafe { *c.record_slot(record) };
    c.bytes_delete(bytes);
}

/// `GetSendToAllAndWaitMessageID`: the type's id member, if it has one.
pub fn s2aw_id<C: RecordCpp>(c: &mut C, record: usize, out: &mut c_ulong) -> bool {
    let ty = ee_netmsg_ne_type(c.message_type(record));
    if ty.s2aw_field == EE_NETMSG_NE_NONE {
        return false;
    }
    *out = get_u32(c, record, ty.s2aw_field as u32) as c_ulong;
    true
}

/// `SetSendToAllAndWaitMessageID`.
pub fn set_s2aw_id<C: RecordCpp>(c: &mut C, record: usize, id: c_ulong) {
    let ty = ee_netmsg_ne_type(c.message_type(record));
    if ty.s2aw_field != EE_NETMSG_NE_NONE {
        let p = field_at(c, record, ty.s2aw_field as u32, 4);
        // SAFETY: a 4-byte member
        unsafe { core::ptr::write_unaligned(p as *mut u32, id as u32) };
    }
}

/// `IsResponse`: the type's rule (a tick and the response for its tick, a pause or an unpause and the response with
/// its unique id).
pub fn is_response<C: RecordCpp>(c: &mut C, record: usize, other: usize) -> bool {
    let ty = ee_netmsg_ne_type(c.message_type(record));
    ty.response_type != 0
        && c.message_type_of(other) == ty.response_type
        && get_u32(c, record, ty.response_mine as u32) == get_u32(c, other, ty.response_theirs as u32)
}

/// `AddPlayerInput`: at most 255.
pub fn add_player_input<C: RecordCpp>(c: &mut C, record: usize, input: usize) -> c_long {
    if c.inputs_size(record) < 255 {
        c.inputs_push(record, input);
        return OK;
    }
    BAD_PATH
}

/// `GetPlayerInput`.
pub fn player_input<C: RecordCpp>(c: &mut C, record: usize, index: c_ulong, out: &mut usize) -> c_long {
    if (index as usize) < c.inputs_size(record) {
        *out = c.inputs_at(record, index as usize);
        return OK;
    }
    BEYOND_END
}

/// The codec's slots (`CodecSlots`): the header, then the wire members (a fixed one in the record, a string's
/// bytes, the inputs: each exported, `buffers` keeping their exports, or only sized); the slot count.
fn codec_slots<C: RecordCpp>(
    c: &mut C,
    record: usize,
    slots: &mut [EeNetMsgSlot; SLOTS],
    class: &mut u32,
    parts: &mut Vec<EeNetMsgSlot>,
    mut buffers: Option<&mut Vec<*mut u8>>,
) -> usize {
    let t = c.message_type(record);
    let ty = ee_netmsg_ne_type(t);
    *class = ty.class_id;
    c.header_slots(record, slots.as_mut_ptr());
    let bytes = unsafe { *c.record_slot(record) };
    for i in 0..ty.wire_fields {
        let f = field(t, i);
        let mut slot = EeNetMsgSlot { data: bytes.wrapping_add(f.offset as usize), size: f.size, count: 0, offset: 0 };
        if f.kind == EE_NETMSG_NE_FIELD_STRING {
            let text = c.string_chars(record, f.offset);
            // SAFETY: the string's characters
            let len = unsafe { core::ffi::CStr::from_ptr(text) }.to_bytes().len();
            slot = EeNetMsgSlot { data: text as *mut u8, size: len as u32, count: 0, offset: 0 };
        } else if f.kind == EE_NETMSG_NE_FIELD_INPUTS {
            parts.clear();
            for k in 0..c.inputs_size(record) {
                let input = c.inputs_at(record, k);
                let mut part = EeNetMsgSlot { data: core::ptr::null_mut(), size: 0, count: 0, offset: 0 };
                if let Some(b) = buffers.as_deref_mut() {
                    let (mut data, mut size) = (core::ptr::null_mut::<u8>(), 0 as c_ulong);
                    c.message_export(input, &mut data, &mut size);
                    b.push(data);
                    part.data = data;
                    part.size = size as u32;
                } else {
                    part.size = c.message_export_size(input) as u32;
                }
                parts.push(part);
            }
            let data = if parts.is_empty() { core::ptr::null_mut() } else { parts.as_mut_ptr() as *mut u8 };
            slot = EeNetMsgSlot { data, size: 0, count: parts.len() as u32, offset: 0 };
        }
        slots[4 + i as usize] = slot;
    }
    4 + ty.wire_fields as usize
}

fn empty_slots() -> [EeNetMsgSlot; SLOTS] {
    core::array::from_fn(|_| EeNetMsgSlot { data: core::ptr::null_mut(), size: 0, count: 0, offset: 0 })
}

/// `ExportSize`.
pub fn export_size<C: RecordCpp>(c: &mut C, record: usize) -> c_ulong {
    let mut slots = empty_slots();
    let (mut parts, mut class) = (Vec::new(), 0u32);
    let n = codec_slots(c, record, &mut slots, &mut class, &mut parts, None);
    // SAFETY: the slots point at the live members
    unsafe { ee_sim::netmsg::ffi::ee_netmsg_export_size(class, slots.as_ptr(), n) as c_ulong }
}

/// `ExportMessage`: a new buffer of the message's size, the message exported into it (the inputs' exports freed).
///
/// # Safety
/// The outs are writable.
pub unsafe fn export<C: RecordCpp>(c: &mut C, record: usize, out_data: *mut *mut u8, out_size: *mut c_ulong) {
    let mut slots = empty_slots();
    let (mut parts, mut buffers, mut class) = (Vec::new(), Vec::new(), 0u32);
    let n = codec_slots(c, record, &mut slots, &mut class, &mut parts, Some(&mut buffers));
    unsafe {
        let size = ee_sim::netmsg::ffi::ee_netmsg_export_size(class, slots.as_ptr(), n);
        let data = c.bytes_new(size as usize);
        *out_data = data;
        *out_size = ee_sim::netmsg::ffi::ee_netmsg_export(class, slots.as_ptr(), n, data, size as usize) as c_ulong;
    }
    for b in buffers {
        c.bytes_delete(b);
    }
}

/// `ImportMessage`: the fixed members into the record, the strings imported, the inputs built (each by the
/// factory) and added.
///
/// # Safety
/// `data` holds a received message of the record's type.
pub unsafe fn import<C: RecordCpp>(c: &mut C, record: usize, data: *mut u8) {
    verify(!data.is_null(), c"inPayloadPointer != NULL");
    let mut slots = empty_slots();
    let (mut parts, mut class) = (Vec::new(), 0u32);
    let n = codec_slots(c, record, &mut slots, &mut class, &mut parts, None);
    unsafe { ee_sim::netmsg::ffi::ee_netmsg_import(class, data, slots.as_mut_ptr(), n) };
    let t = c.message_type(record);
    for (i, slot) in slots.iter().enumerate().take(n).skip(4) {
        let f = field(t, i as u32 - 4);
        let at = data.wrapping_add(slot.offset as usize);
        if f.kind == EE_NETMSG_NE_FIELD_STRING {
            c.string_import(record, f.offset, at);
        } else if f.kind == EE_NETMSG_NE_FIELD_INPUTS {
            // the codec walks the inputs; each is built by the factory and added (`NetMsgBuildInput`)
            let mut each = |input_data: *const u8| -> u32 {
                let m = c.message_from_memory(input_data);
                if m == 0 {
                    return 0;
                }
                c.inputs_push(record, m);
                c.message_export_size(m) as u32
            };
            unsafe { walk_inputs(at, &mut each) };
        }
    }
}

/// The codec's inputs walk with a Rust closure.
unsafe fn walk_inputs(data: *const u8, each: &mut dyn FnMut(*const u8) -> u32) {
    unsafe extern "C" fn trampoline(context: *mut c_void, data: *const u8) -> u32 {
        // SAFETY: the closure handed below
        let f = unsafe { &mut *(context as *mut &mut dyn FnMut(*const u8) -> u32) };
        f(data)
    }
    let mut each = each;
    // SAFETY: the received list; the trampoline takes the closure
    unsafe { ee_sim::netmsg::ffi::ee_netmsg_inputs_import(data, trampoline, &mut each as *mut _ as *mut c_void) };
}

/// `BuildNEMessage`: a record for an engine type, else the verify (NULL).
///
/// # Safety
/// `data` holds a received message.
pub unsafe fn build<C: RecordCpp>(c: &mut C, data: *mut u8) -> usize {
    if ee_netmsg_ne_type(unsafe { *data.add(1) }).class_id != u32::MAX {
        return c.record_new(data);
    }
    verify_at(false, 227, c"\"Undefined message type\" == NULL");
    0
}

#[cfg(not(test))]
mod ffi {
    use super::*;
    use ee_base::shim::ShimFfi as Ffi;

    /// `NEMRecord(inData)`'s body.
    ///
    /// # Safety
    /// `data` holds a received message of an engine type.
    #[no_mangle]
    pub unsafe extern "C" fn ee_ne_record_construct_from(record: *mut c_void, data: *mut u8) {
        unsafe { construct_from(&mut Ffi, record as usize, data) }
    }

    /// `Fill` from the senders' values (`NEMRecordValue`s: one `long long` each) and strings.
    ///
    /// # Safety
    /// The arrays hold their counts.
    #[no_mangle]
    pub unsafe extern "C" fn ee_ne_record_fill(
        record: *mut c_void,
        values: *const i64,
        value_count: usize,
        strings: *const *const c_char,
        string_count: usize,
    ) {
        let v = if value_count == 0 { &[][..] } else { unsafe { core::slice::from_raw_parts(values, value_count) } };
        let s = if string_count == 0 { &[][..] } else { unsafe { core::slice::from_raw_parts(strings, string_count) } };
        unsafe { fill(&mut Ffi, record as usize, v, s) }
    }

    /// `Fill` from the Rust server's and client's fixed members.
    ///
    /// # Safety
    /// `values` holds `count`.
    #[no_mangle]
    pub unsafe extern "C" fn ee_ne_record_fill_u32(record: *mut c_void, values: *const u32, count: usize) {
        let v: Vec<i64> = if count == 0 {
            Vec::new()
        } else {
            unsafe { core::slice::from_raw_parts(values, count) }.iter().map(|&x| x as i64).collect()
        };
        unsafe { fill(&mut Ffi, record as usize, &v, &[]) }
    }

    /// `Field`.
    #[no_mangle]
    pub extern "C" fn ee_ne_record_field(record: *const c_void, index: u32, size: usize) -> *mut u8 {
        field_at(&mut Ffi, record as usize, index, size)
    }

    /// `Value`.
    #[no_mangle]
    pub extern "C" fn ee_ne_record_value(record: *const c_void, index: u32) -> usize {
        value(&mut Ffi, record as usize, index)
    }

    /// `GetString` (the `UWideString`'s address).
    #[no_mangle]
    pub extern "C" fn ee_ne_record_string(record: *const c_void, index: u32) -> *const c_void {
        string(&mut Ffi, record as usize, index) as *const c_void
    }

    /// `~NEMRecord`.
    #[no_mangle]
    pub extern "C" fn ee_ne_record_destruct(record: *mut c_void) {
        destruct(&mut Ffi, record as usize)
    }

    /// `GetSendToAllAndWaitMessageID`.
    ///
    /// # Safety
    /// `out` is writable.
    #[no_mangle]
    pub unsafe extern "C" fn ee_ne_record_s2aw_id(record: *const c_void, out: *mut c_ulong) -> bool {
        s2aw_id(&mut Ffi, record as usize, unsafe { &mut *out })
    }

    /// `SetSendToAllAndWaitMessageID`.
    #[no_mangle]
    pub extern "C" fn ee_ne_record_set_s2aw_id(record: *mut c_void, id: c_ulong) {
        set_s2aw_id(&mut Ffi, record as usize, id)
    }

    /// `IsResponse`.
    #[no_mangle]
    pub extern "C" fn ee_ne_record_is_response(record: *mut c_void, other: *mut c_void) -> bool {
        is_response(&mut Ffi, record as usize, other as usize)
    }

    /// `AddPlayerInput`.
    #[no_mangle]
    pub extern "C" fn ee_ne_record_add_input(record: *mut c_void, input: *mut c_void) -> c_long {
        add_player_input(&mut Ffi, record as usize, input as usize)
    }

    /// `GetPlayerInput`.
    ///
    /// # Safety
    /// `out` is writable.
    #[no_mangle]
    pub unsafe extern "C" fn ee_ne_record_input(record: *mut c_void, index: c_ulong, out: *mut *mut c_void) -> c_long {
        let mut found = 0usize;
        let r = player_input(&mut Ffi, record as usize, index, &mut found);
        if r == OK {
            unsafe { *out = found as *mut c_void };
        }
        r
    }

    /// `ExportSize`.
    #[no_mangle]
    pub extern "C" fn ee_ne_record_export_size(record: *mut c_void) -> c_ulong {
        export_size(&mut Ffi, record as usize)
    }

    /// `ExportMessage`.
    ///
    /// # Safety
    /// The outs are writable.
    #[no_mangle]
    pub unsafe extern "C" fn ee_ne_record_export(record: *mut c_void, out_data: *mut *mut u8, out_size: *mut c_ulong) {
        unsafe { export(&mut Ffi, record as usize, out_data, out_size) }
    }

    /// `ImportMessage`.
    ///
    /// # Safety
    /// `data` holds a received message of the record's type.
    #[no_mangle]
    pub unsafe extern "C" fn ee_ne_record_import(record: *mut c_void, data: *mut u8) {
        unsafe { import(&mut Ffi, record as usize, data) }
    }

    /// `BuildNEMessage`.
    ///
    /// # Safety
    /// `data` holds a received message.
    #[no_mangle]
    pub unsafe extern "C" fn ee_ne_record_build(data: *mut u8) -> *mut c_void {
        unsafe { build(&mut Ffi, data) as *mut c_void }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ee_base::shim::ShimFake;

    #[test]
    fn inputs_are_capped_at_255_and_read_by_index() {
        let mut c = ShimFake::default();
        c.answer("inputs_size", &[3, 255, 3]);
        assert_eq!(add_player_input(&mut c, 1, 9), OK);
        assert_eq!(add_player_input(&mut c, 1, 9), BAD_PATH);
        c.answer("inputs_at", &[0x44]);
        let mut out = 0;
        assert_eq!(player_input(&mut c, 1, 2, &mut out), OK);
        assert_eq!(out, 0x44);
        c.answer("inputs_size", &[3]);
        assert_eq!(player_input(&mut c, 1, 3, &mut out), BEYOND_END);
        assert_eq!(c.count("inputs_push"), 1);
    }

    #[test]
    fn the_destructor_releases_the_inputs_from_the_last() {
        let mut c = ShimFake::default();
        c.answer("inputs_size", &[2, 1, 0]);
        c.answer("inputs_back", &[7, 6]);
        let mut slot: *mut u8 = 0x99 as *mut u8;
        c.answer("record_slot", &[&mut slot as *mut _ as i64]);
        destruct(&mut c, 1);
        let calls: Vec<&str> = c.log.iter().map(|l| l.as_str()).filter(|l| !l.starts_with("inputs_size")).collect();
        assert_eq!(
            calls,
            [
                "inputs_back(1)",
                "message_dereference(7)",
                "inputs_pop(1)",
                "inputs_back(1)",
                "message_dereference(6)",
                "inputs_pop(1)",
                "record_slot(1)",
                "bytes_delete(0x99)"
            ]
        );
    }

    #[test]
    fn filling_truncates_integers_and_normalises_bools() {
        // the client connection request (0x22): its fixed members and strings per ne_records.rs
        let t = 0x22u8;
        let ty = ee_netmsg_ne_type(t);
        assert_ne!(ty.class_id, u32::MAX);
        let mut record = vec![0xaau8; ty.record_size as usize + 1];
        let mut slot: *mut u8 = core::ptr::null_mut();
        let mut c = ShimFake::default();
        c.answer("message_type", &[t as i64]);
        c.answer("bytes_new", &[record.as_mut_ptr() as i64]);
        c.answer("record_slot", &[&mut slot as *mut _ as i64]);
        let ints: Vec<i64> = (0..ty.fields)
            .filter(|&i| matches!(field(t, i).kind, EE_NETMSG_NE_FIELD_INTEGER | EE_NETMSG_NE_FIELD_BOOL))
            .map(|i| 0x1_0000_0000 + i as i64 + 2)
            .collect();
        let names = [c"a".as_ptr(), c"b".as_ptr()];
        let strings: Vec<*const c_char> = (0..ty.fields)
            .filter(|&i| field(t, i).kind == EE_NETMSG_NE_FIELD_STRING)
            .enumerate()
            .map(|(k, _)| names[k])
            .collect();
        unsafe { fill(&mut c, 1, &ints, &strings) };
        assert_eq!(slot, record.as_mut_ptr());
        let mut k = 0;
        for i in 0..ty.fields {
            let f = field(t, i);
            match f.kind {
                EE_NETMSG_NE_FIELD_BOOL => {
                    assert_eq!(record[f.offset as usize], 1);
                    k += 1;
                }
                EE_NETMSG_NE_FIELD_INTEGER => {
                    let mut v = [0u8; 8];
                    v[..f.size as usize].copy_from_slice(&record[f.offset as usize..(f.offset + f.size) as usize]);
                    let mask = if f.size >= 8 { u64::MAX } else { (1u64 << (8 * f.size)) - 1 };
                    assert_eq!(u64::from_le_bytes(v), (ints[k] as u64) & mask);
                    k += 1;
                }
                _ => {}
            }
        }
        assert_eq!(c.count("string_set"), strings.len());
    }
}
