//! The C ABI the game messages' `ExportSize`/`ExportMessage`/`ImportMessage` call (`EEMessage.cpp`).
//!
//! A message hands its members as slots in wire order: the header's four (reserved, type, target slot, originating
//! slot), then one per member of its class (`classes::CLASSES`). A fixed slot is the member's address and `sizeof`
//! (checked against the layout: a wrong binding stops the game with the member's name). On export a string slot is
//! its bytes and their count, a record slot its records (`data`, `size` = one record's size, `count`), a blob slot
//! its bytes; on import the fixed members are written, and a string's, records' or blob's `offset`/`count` say
//! where the C++ finds it (the blob is also copied into `data` when `size` holds it). The players' inputs slot (the
//! engine ticks' last member) is `data` = an array of `count` slots, one per input of the list (its bytes and their
//! count; for a size alone the bytes may be NULL); on import its `offset` is the count byte's and `count` the count,
//! and the C++ builds the inputs with `ee_netmsg_inputs_import`. Class ids: the game classes (`classes`), then the
//! engine classes (`engine`). No Rust reference outlives a call; the C++ keeps the objects.

use crate::classes::{CLASSES, CLASS_COUNT, TYPES};
use crate::codec::{self, Error, Input, Source, Span, Value};
use crate::engine::{EE_NETMSG_ENGINE_BASE, ENGINE_CLASSES, ENGINE_TYPES};
use crate::schema::{Kind, MsgClass, HEADER_SIZE};
use crate::units;

/// One member of a message (see the module comment).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeNetMsgSlot {
    pub data: *mut u8,
    pub size: u32,
    pub count: u32,
    pub offset: u32,
}

fn class(id: u32) -> &'static MsgClass {
    let found = if id < EE_NETMSG_ENGINE_BASE {
        CLASSES.get(id as usize)
    } else {
        ENGINE_CLASSES.get((id - EE_NETMSG_ENGINE_BASE) as usize)
    };
    match found {
        Some(c) => c,
        None => panic!("ee_netmsg: no message class {id} (of {})", CLASS_COUNT + ENGINE_CLASSES.len()),
    }
}

/// A players' inputs slot's list. Safety: `data` points to `count` slots, each with `size` bytes at its `data`
/// (or a NULL `data` when `with_bytes` is false).
unsafe fn input_list<'a>(slot: &EeNetMsgSlot, with_bytes: bool) -> Vec<Input<'a>> {
    if slot.count == 0 {
        return Vec::new();
    }
    let parts = std::slice::from_raw_parts(slot.data as *const EeNetMsgSlot, slot.count as usize);
    parts
        .iter()
        .map(|p| Input {
            size: p.size as usize,
            bytes: if with_bytes && !p.data.is_null() {
                Some(std::slice::from_raw_parts(p.data as *const u8, p.size as usize))
            } else {
                None
            },
        })
        .collect()
}

/// The inputs of a class's inputs slot, if it has one.
unsafe fn class_inputs<'a>(c: &MsgClass, slots: &[EeNetMsgSlot], with_bytes: bool) -> Vec<Input<'a>> {
    match c.fields.iter().position(|f| f.kind == Kind::Inputs) {
        Some(i) => input_list(&slots[HEADER_SIZE + i], with_bytes),
        None => Vec::new(),
    }
}

/// The slots as values (`inputs`: the inputs slot's list, from `class_inputs`). Safety: every slot's `data` holds
/// what the slot says (see the module comment).
unsafe fn values<'a>(c: &MsgClass, slots: &'a [EeNetMsgSlot], inputs: &'a [Input<'a>]) -> Vec<Value<'a>> {
    let bytes = |s: &EeNetMsgSlot, n: usize| -> &'a [u8] {
        if n == 0 {
            &[]
        } else {
            std::slice::from_raw_parts(s.data as *const u8, n)
        }
    };
    c.fields
        .iter()
        .zip(&slots[HEADER_SIZE..])
        .map(|(f, s)| match f.kind {
            Kind::WStr => Value::Str(bytes(s, s.size as usize)),
            Kind::Records8(n) | Kind::Records32(n) => {
                if s.size != n {
                    panic!("ee_netmsg: {}::{} has {}-byte records, the C++ {}", c.name, f.name, n, s.size);
                }
                Value::Records { count: s.count as usize, bytes: bytes(s, s.count as usize * n as usize) }
            }
            Kind::Blob => Value::Blob(bytes(s, s.size as usize)),
            Kind::Inputs => Value::Inputs(inputs),
            k => {
                let n = k.fixed_size().unwrap();
                if s.size as usize != n {
                    panic!("ee_netmsg: {}::{} is {} bytes on the wire, the C++ member {}", c.name, f.name, n, s.size);
                }
                Value::Fixed(bytes(s, n))
            }
        })
        .collect()
}

fn check_arity(c: &MsgClass, n: usize) {
    if n != HEADER_SIZE + c.fields.len() {
        panic!("ee_netmsg: {} takes {} slots, the C++ passed {}", c.name, HEADER_SIZE + c.fields.len(), n);
    }
}

/// `ExportSize` of a class whose members are all fixed (its constant); 0 for a class with variable members.
#[no_mangle]
pub extern "C" fn ee_netmsg_fixed_size(class_id: u32) -> u32 {
    class(class_id).fixed_size().map_or(0, |n| n as u32)
}

/// `ExportSize`: the wire size of the message these slots describe.
///
/// # Safety
/// `slots` points to `count` slots filled as the module comment says.
#[no_mangle]
pub unsafe extern "C" fn ee_netmsg_export_size(class_id: u32, slots: *const EeNetMsgSlot, count: usize) -> u32 {
    let c = class(class_id);
    check_arity(c, count);
    let slots = std::slice::from_raw_parts(slots, count);
    let inputs = class_inputs(c, slots, false);
    match codec::encoded_size(c, &values(c, slots, &inputs)) {
        Ok(n) => n as u32,
        Err(e) => panic!("ee_netmsg: {}: {:?}", c.name, e),
    }
}

/// `ExportMessage`: writes the message into `out` (`out_size` bytes, from `ee_netmsg_export_size`); returns the bytes
/// written.
///
/// # Safety
/// `slots` as for `ee_netmsg_export_size`; `out` holds `out_size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_netmsg_export(
    class_id: u32,
    slots: *const EeNetMsgSlot,
    count: usize,
    out: *mut u8,
    out_size: usize,
) -> u32 {
    let c = class(class_id);
    check_arity(c, count);
    let slots = std::slice::from_raw_parts(slots, count);
    let mut header = [0u8; HEADER_SIZE];
    for (h, s) in header.iter_mut().zip(slots) {
        if s.size != 1 {
            panic!("ee_netmsg: {}: a header slot of {} bytes", c.name, s.size);
        }
        *h = *s.data;
    }
    let out = std::slice::from_raw_parts_mut(out, out_size);
    let inputs = class_inputs(c, slots, true);
    match codec::encode(c, header, &values(c, slots, &inputs), out) {
        Ok(n) => n as u32,
        Err(e) => panic!("ee_netmsg: {}: {:?}", c.name, e),
    }
}

/// The received buffer, trusted as the original's imports trusted it (they take no length).
struct Received(*const u8);

impl Source for Received {
    fn bytes(&self, offset: usize, len: usize, _member: &'static str) -> Result<&[u8], Error> {
        // Safety: the caller of ee_netmsg_import vouches for the message's bytes (the network layer received them)
        Ok(unsafe { std::slice::from_raw_parts(self.0.add(offset), len) })
    }
}

/// `ImportMessage`: reads the header and the members the class's import reads from `data`.
///
/// # Safety
/// `data` holds a whole message of this class; `slots` points to `count` slots whose fixed members are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_netmsg_import(class_id: u32, data: *const u8, slots: *mut EeNetMsgSlot, count: usize) {
    let c = class(class_id);
    check_arity(c, count);
    let slots = std::slice::from_raw_parts_mut(slots, count);
    let mut spans: Vec<Span> = Vec::with_capacity(c.fields.len());
    let header = match codec::decode(c, &Received(data), &mut spans) {
        Ok(h) => h,
        Err(e) => panic!("ee_netmsg: {}: {:?}", c.name, e),
    };
    for (s, h) in slots.iter_mut().zip(header) {
        *s.data = h;
    }
    for ((f, s), span) in c.fields.iter().zip(&mut slots[HEADER_SIZE..]).zip(&spans) {
        s.offset = span.offset as u32;
        match f.kind {
            Kind::WStr => s.count = span.len as u32,
            Kind::Records8(_) | Kind::Records32(_) | Kind::Inputs => s.count = span.count as u32,
            Kind::Blob => {
                s.count = span.len as u32;
                if span.len <= s.size as usize && span.len > 0 {
                    std::ptr::copy_nonoverlapping(data.add(span.offset), s.data, span.len);
                }
            }
            k => {
                let n = k.fixed_size().unwrap();
                if s.size as usize != n {
                    panic!("ee_netmsg: {}::{} is {} bytes on the wire, the C++ member {}", c.name, f.name, n, s.size);
                }
                std::ptr::copy_nonoverlapping(data.add(span.offset), s.data, n);
            }
        }
    }
}

/// The class the game's or the engine's factory builds for message type `message_type`, or `u32::MAX`.
#[no_mangle]
pub extern "C" fn ee_netmsg_class_of_type(message_type: u8) -> u32 {
    TYPES.iter().chain(&ENGINE_TYPES).find(|t| t.0 == message_type).map_or(u32::MAX, |t| t.1)
}

/// No message has this type.
pub const EE_NETMSG_KIND_NONE: u8 = 0;
/// An engine message (`NEMessage::BuildNEMessage` builds it).
pub const EE_NETMSG_KIND_ENGINE: u8 = 1;
/// A game message: the types the game registers its factory for (`EEMessage::RegisterKnownMessages`, 0x41..0xfd),
/// the types every `EEMessage` class takes (`EEMEndCinematic`'s 0x9d too, which the factory does not build).
pub const EE_NETMSG_KIND_GAME: u8 = 2;

/// The message kind of a type: engine, game, or none. A message object is an `EEMessage` exactly when its type is a
/// game type: every class's constructor passes its type, and the import constructors take the type the factory
/// dispatched on.
#[no_mangle]
pub extern "C" fn ee_netmsg_message_kind(message_type: u8) -> u8 {
    if ENGINE_TYPES.iter().any(|t| t.0 == message_type) {
        EE_NETMSG_KIND_ENGINE
    } else if (0x41..=0xfd).contains(&message_type) {
        EE_NETMSG_KIND_GAME
    } else {
        EE_NETMSG_KIND_NONE
    }
}

/// `NEMPlayerInput::ExportSize`: the count byte and the travelling inputs' sizes (`inputs`: one slot per input of
/// the list, its `size` its `ExportSize`).
///
/// # Safety
/// `inputs` points to `count` slots.
#[no_mangle]
pub unsafe extern "C" fn ee_netmsg_inputs_size(inputs: *const EeNetMsgSlot, count: usize) -> u32 {
    let slot = EeNetMsgSlot { data: inputs as *mut u8, size: 0, count: count as u32, offset: 0 };
    codec::inputs_size(&input_list(&slot, false)) as u32
}

/// `NEMPlayerInput::ExportMessage`: the count byte and the travelling inputs' bytes into `out` (`out_size` bytes);
/// returns the bytes written.
///
/// # Safety
/// `inputs` points to `count` slots, each with its exported bytes; `out` holds `out_size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_netmsg_inputs_export(
    inputs: *const EeNetMsgSlot,
    count: usize,
    out: *mut u8,
    out_size: usize,
) -> u32 {
    let slot = EeNetMsgSlot { data: inputs as *mut u8, size: 0, count: count as u32, offset: 0 };
    let out = std::slice::from_raw_parts_mut(out, out_size);
    match codec::encode_inputs(&input_list(&slot, true), out) {
        Ok(n) => n as u32,
        Err(e) => panic!("ee_netmsg: NEMPlayerInput: {:?}", e),
    }
}

/// Builds one nested input from the bytes at `data`: its `ExportSize`, or 0 when no message has its type.
pub type EeNetMsgBuildInput = unsafe extern "C" fn(context: *mut std::ffi::c_void, data: *const u8) -> u32;

/// `NEMPlayerInput::ImportMessage`: reads the count byte at `data` and has `build` make each input in turn
/// (`codec::walk_inputs`); returns the offset after the last input built.
///
/// # Safety
/// `data` holds a received inputs list; `build` is safe to call with `context` and a pointer into it.
#[no_mangle]
pub unsafe extern "C" fn ee_netmsg_inputs_import(
    data: *const u8,
    build: EeNetMsgBuildInput,
    context: *mut std::ffi::c_void,
) -> u32 {
    codec::walk_inputs(*data, |at| match build(context, data.add(at)) {
        0 => None,
        n => Some(n as usize),
    }) as u32
}

/// `CompressUnits`: packs `count` ids for `owner` into `out` (256 bytes); returns the byte count.
///
/// # Safety
/// `ids` holds `count` values; `out` 256 bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_netmsg_compress_units(ids: *const u32, count: usize, owner: u32, out: *mut u8) -> u8 {
    let ids = if count == 0 { &[][..] } else { std::slice::from_raw_parts(ids, count) };
    units::compress(ids, owner, &mut *(out as *mut [u8; 256]))
}

/// `DecompressUnits`: the ids of a buffer (256 bytes, `size` of them used) into `out` (room for `capacity`, at least
/// `units::MAX_BUFFER`); returns how many.
///
/// # Safety
/// `buffer` holds 256 bytes; `out` `capacity` values.
#[no_mangle]
pub unsafe extern "C" fn ee_netmsg_decompress_units(
    buffer: *const u8,
    size: u8,
    out: *mut i32,
    capacity: usize,
) -> usize {
    let mut ids = Vec::new();
    units::decompress(&*(buffer as *const [u8; 256]), size, &mut ids);
    let n = ids.len().min(capacity);
    std::ptr::copy_nonoverlapping(ids.as_ptr(), out, n);
    ids.len()
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::classes;

    fn slot<T>(v: &mut T) -> EeNetMsgSlot {
        EeNetMsgSlot { data: v as *mut T as *mut u8, size: std::mem::size_of::<T>() as u32, count: 0, offset: 0 }
    }

    #[test]
    fn attack_unit_round_trips_through_the_slots() {
        // EEMAttackUnit: selection id, target, queued, convert: 11 bytes
        let c = classes::EE_NETMSG_ATTACK_UNIT;
        assert_eq!(ee_netmsg_fixed_size(c), 11);
        let (mut r, mut t, mut f, mut o) = (0u8, 0x4cu8, 0xffu8, 1u8);
        let (mut sel, mut target, mut queued, mut convert) = (9u8, -7i32, 1u8, 0u8);
        let slots = [
            slot(&mut r),
            slot(&mut t),
            slot(&mut f),
            slot(&mut o),
            slot(&mut sel),
            slot(&mut target),
            slot(&mut queued),
            slot(&mut convert),
        ];
        let n = unsafe { ee_netmsg_export_size(c, slots.as_ptr(), slots.len()) };
        assert_eq!(n, 11);
        let mut out = [0u8; 11];
        assert_eq!(unsafe { ee_netmsg_export(c, slots.as_ptr(), slots.len(), out.as_mut_ptr(), 11) }, 11);
        assert_eq!(out, [0, 0x4c, 0xff, 1, 9, 0xf9, 0xff, 0xff, 0xff, 1, 0]);
        let (mut r2, mut t2, mut f2, mut o2) = (5u8, 5u8, 5u8, 5u8);
        let (mut sel2, mut target2, mut queued2, mut convert2) = (0u8, 0i32, 0u8, 9u8);
        let mut slots2 = [
            slot(&mut r2),
            slot(&mut t2),
            slot(&mut f2),
            slot(&mut o2),
            slot(&mut sel2),
            slot(&mut target2),
            slot(&mut queued2),
            slot(&mut convert2),
        ];
        unsafe { ee_netmsg_import(c, out.as_ptr(), slots2.as_mut_ptr(), slots2.len()) };
        assert_eq!((r2, t2, f2, o2, sel2, target2, queued2, convert2), (0, 0x4c, 0xff, 1, 9, -7, 1, 0));
    }

    #[test]
    fn a_string_import_reports_where_the_string_is() {
        let c = classes::EE_NETMSG_CHAT;
        assert_eq!(ee_netmsg_fixed_size(c), 0);
        let (mut r, mut t, mut f, mut o, mut sel, mut player) = (0u8, 0x93u8, 0u8, 0u8, 1u8, 2u8);
        let text = b"hi".to_vec();
        let slots = [
            slot(&mut r),
            slot(&mut t),
            slot(&mut f),
            slot(&mut o),
            slot(&mut sel),
            slot(&mut player),
            EeNetMsgSlot { data: text.as_ptr() as *mut u8, size: 2, count: 0, offset: 0 },
        ];
        let n = unsafe { ee_netmsg_export_size(c, slots.as_ptr(), slots.len()) } as usize;
        assert_eq!(n, 6 + 2 + 6);
        let mut out = vec![0u8; n];
        unsafe { ee_netmsg_export(c, slots.as_ptr(), slots.len(), out.as_mut_ptr(), n) };
        let mut back = slots;
        let (mut sel2, mut player2) = (0u8, 0u8);
        back[4] = slot(&mut sel2);
        back[5] = slot(&mut player2);
        unsafe { ee_netmsg_import(c, out.as_ptr(), back.as_mut_ptr(), back.len()) };
        assert_eq!((sel2, player2, back[6].offset, back[6].count), (1, 2, 6, 4));
        assert_eq!(&out[10..12], b"hi");
    }

    #[test]
    fn every_type_has_a_class() {
        assert_eq!(TYPES.len(), 107);
        for &(t, c) in &TYPES {
            assert_eq!(ee_netmsg_class_of_type(t), c);
            assert!((c as usize) < CLASS_COUNT);
        }
        assert_eq!(ee_netmsg_class_of_type(0x20), crate::engine::EE_NETMSG_NE_ADD_PLAYER);
        assert_eq!(ee_netmsg_class_of_type(0x2c), u32::MAX);
        assert_eq!(ee_netmsg_class_of_type(0x4c), classes::EE_NETMSG_ATTACK_UNIT);
    }

    #[test]
    fn a_tick_with_inputs_round_trips_through_the_slots() {
        use crate::engine::EE_NETMSG_NE_COMMUNICATIONS_TICK as TICK;
        let mut h = [0u8, 0x26, 0xff, 3];
        let hs = h.as_mut_ptr();
        let header = |i: usize| EeNetMsgSlot { data: unsafe { hs.add(i) }, size: 1, count: 0, offset: 0 };
        let (mut ct, mut perform, mut rate, mut sum, mut dump) = (0x1234u32, 2u8, 8u8, 1u8, 0u8);
        let a = [0u8, 0x3a, 1, 2];
        let b = [0u8, 0x41, 1, 2, 5];
        let parts = [
            EeNetMsgSlot { data: a.as_ptr() as *mut u8, size: 4, count: 0, offset: 0 },
            EeNetMsgSlot { data: b.as_ptr() as *mut u8, size: 5, count: 0, offset: 0 },
        ];
        let slots = [
            header(0),
            header(1),
            header(2),
            header(3),
            slot(&mut ct),
            slot(&mut perform),
            slot(&mut rate),
            slot(&mut sum),
            slot(&mut dump),
            EeNetMsgSlot { data: parts.as_ptr() as *mut u8, size: 0, count: 2, offset: 0 },
        ];
        let n = unsafe { ee_netmsg_export_size(TICK, slots.as_ptr(), slots.len()) } as usize;
        assert_eq!(n, 12 + 1 + 9);
        // the size needs no bytes
        let sized = [
            EeNetMsgSlot { data: std::ptr::null_mut(), size: 4, count: 0, offset: 0 },
            EeNetMsgSlot { data: std::ptr::null_mut(), size: 5, count: 0, offset: 0 },
        ];
        assert_eq!(unsafe { ee_netmsg_inputs_size(sized.as_ptr(), 2) }, 10);
        let mut out = vec![0u8; n];
        assert_eq!(unsafe { ee_netmsg_export(TICK, slots.as_ptr(), slots.len(), out.as_mut_ptr(), n) } as usize, n);
        assert_eq!(&out[..13], &[0, 0x26, 0xff, 3, 0x34, 0x12, 0, 0, 2, 8, 1, 0, 2]);
        assert_eq!(&out[13..], &[0, 0x3a, 1, 2, 0, 0x41, 1, 2, 5]);
        let mut alone = [0u8; 10];
        assert_eq!(unsafe { ee_netmsg_inputs_export(parts.as_ptr(), 2, alone.as_mut_ptr(), 10) }, 10);
        assert_eq!(&alone[..], &out[12..]);

        // back: the fixed members, then where the inputs are
        let mut back = slots;
        let (mut ct2, mut perform2, mut rate2, mut sum2, mut dump2) = (0u32, 0u8, 0u8, 0u8, 9u8);
        back[4] = slot(&mut ct2);
        back[5] = slot(&mut perform2);
        back[6] = slot(&mut rate2);
        back[7] = slot(&mut sum2);
        back[8] = slot(&mut dump2);
        unsafe { ee_netmsg_import(TICK, out.as_ptr(), back.as_mut_ptr(), back.len()) };
        assert_eq!((ct2, perform2, rate2, sum2, dump2), (0x1234, 2, 8, 1, 0));
        assert_eq!((back[9].offset, back[9].count), (12, 2));

        // the walk over a C callback: sizes by type; an unknown type builds nothing
        unsafe extern "C" fn build(context: *mut std::ffi::c_void, data: *const u8) -> u32 {
            let seen = &mut *(context as *mut Vec<u8>);
            seen.push(*data.add(1));
            match *data.add(1) {
                0x3a => 4,
                0x41 => 5,
                _ => 0,
            }
        }
        let mut seen: Vec<u8> = Vec::new();
        let end = unsafe {
            ee_netmsg_inputs_import(out.as_ptr().add(12), build, &mut seen as *mut Vec<u8> as *mut std::ffi::c_void)
        };
        assert_eq!((end, seen.as_slice()), (10, &[0x3a, 0x41][..]));
    }

    #[test]
    fn message_kinds() {
        assert_eq!(ee_netmsg_message_kind(0x20), EE_NETMSG_KIND_ENGINE);
        assert_eq!(ee_netmsg_message_kind(0x3c), EE_NETMSG_KIND_ENGINE);
        assert_eq!(ee_netmsg_message_kind(0x2c), EE_NETMSG_KIND_NONE);
        assert_eq!(ee_netmsg_message_kind(0x41), EE_NETMSG_KIND_GAME);
        assert_eq!(ee_netmsg_message_kind(0xae), EE_NETMSG_KIND_GAME);
        // EEMEndCinematic: a game message the factory does not build
        assert_eq!(ee_netmsg_message_kind(0x9d), EE_NETMSG_KIND_GAME);
        assert_eq!(ee_netmsg_class_of_type(0x9d), u32::MAX);
        assert_eq!(ee_netmsg_message_kind(0xfd), EE_NETMSG_KIND_GAME);
        for t in [0x00u8, 0x1f, 0x3d, 0x40, 0xfe, 0xff] {
            assert_eq!(ee_netmsg_message_kind(t), EE_NETMSG_KIND_NONE, "{t:#x}");
        }
        assert_eq!(ee_netmsg_class_of_type(0x26), crate::engine::EE_NETMSG_NE_COMMUNICATIONS_TICK);
        assert_eq!(ee_netmsg_fixed_size(crate::engine::EE_NETMSG_NE_XFER_MESSAGE), 1033);
        assert_eq!(ee_netmsg_fixed_size(crate::engine::EE_NETMSG_NE_COMMUNICATIONS_TICK), 0);
    }

    #[test]
    fn units_through_the_abi() {
        let ids = [1u32, 0x205];
        let mut buf = [0u8; 256];
        let n = unsafe { ee_netmsg_compress_units(ids.as_ptr(), 2, 4, buf.as_mut_ptr()) };
        assert_eq!(&buf[..n as usize], &[4, 1, 1, 0x41, 2, 5]);
        let mut out = [0i32; units::MAX_BUFFER];
        let k = unsafe { ee_netmsg_decompress_units(buf.as_ptr(), n, out.as_mut_ptr(), out.len()) };
        assert_eq!(&out[..k], &[0x0400_0001, 0x0400_0205]);
        assert_eq!(unsafe { ee_netmsg_compress_units(std::ptr::null(), 0, 0, buf.as_mut_ptr()) }, 2);
    }

    #[test]
    #[should_panic(expected = "is 4 bytes on the wire")]
    fn a_wrong_member_size_stops() {
        // (through `values`: a panic cannot unwind out of the extern "C" functions)
        let c = &CLASSES[classes::EE_NETMSG_ATTACK_UNIT as usize];
        let (mut h, mut sel, mut target, mut q, mut v) = ([0u8; 4], 0u8, 0u16, 0u8, 0u8);
        let hs = h.as_mut_ptr();
        let header = |i: usize| EeNetMsgSlot { data: unsafe { hs.add(i) }, size: 1, count: 0, offset: 0 };
        let slots =
            [header(0), header(1), header(2), header(3), slot(&mut sel), slot(&mut target), slot(&mut q), slot(&mut v)];
        unsafe { values(c, &slots, &[]) };
    }
}
