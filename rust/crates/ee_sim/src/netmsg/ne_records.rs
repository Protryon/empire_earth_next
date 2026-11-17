//! The engine messages as one data class (LLE Network Engine round 6): `NEMRecord` (lle/NEMessage.h) carries any of
//! the 28 engine message types (0x20..0x3c but 0x2c). Its fixed members sit packed in a byte record in wire order
//! (the transfer's 1 KB block among them), then the members that never travel (the physics tick's rate); its strings
//! are `UWideString`s by ordinal, the communications ticks' players' inputs a list. The C++ asks here for the record's
//! layout, the type's flags (counts towards the input throttle, can be sent while paused), its send-to-all-and-wait
//! member, its response rule and its debug label: the 28 classes' constructors, getters and one-line overrides.
//!
//! The factory (`NEMessage::ConvertMemoryToMessage`, the original's `sConstructors[256]`) is here too: the engine and
//! the game register their builders by type range, and a build calls the one registered for the type in the bytes.

use crate::netmsg::engine::{EE_NETMSG_ENGINE_BASE, ENGINE_CLASSES, ENGINE_TYPES};
use crate::netmsg::schema::{Field, Kind, MsgClass};
use std::ffi::{c_char, c_void};
use std::sync::atomic::{AtomicUsize, Ordering};

/// `GetCountsTowardsPlayerInputThrottle` answers true (the base's answer; 20 types say no).
pub const EE_NETMSG_NE_THROTTLE: u8 = 1;
/// `GetCanSendWhilePaused` answers true.
pub const EE_NETMSG_NE_PAUSED: u8 = 2;

/// No response rule, no send-to-all-and-wait member.
pub const EE_NETMSG_NE_NONE: u8 = 0xff;

/// One engine type: everything the C++ class answered beyond its members.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeNetMsgNeType {
    /// the codec class (`EE_NETMSG_NE_*`), `u32::MAX` for a type the engine does not build
    pub class_id: u32,
    /// the members on the wire (the codec's slots after the header)
    pub wire_fields: u32,
    /// the record's members: the wire's, then the ones that never travel
    pub fields: u32,
    /// the fixed members' bytes (the record's size)
    pub record_size: u32,
    /// `EE_NETMSG_NE_THROTTLE`, `EE_NETMSG_NE_PAUSED`
    pub flags: u8,
    /// the send-to-all-and-wait id's member (`Get/SetSendToAllAndWaitMessageID`), or `EE_NETMSG_NE_NONE`
    pub s2aw_field: u8,
    /// `IsResponse`: the response's type (0: none), this message's member and the response's that must be equal
    pub response_type: u8,
    pub response_mine: u8,
    pub response_theirs: u8,
    /// `GetMessageLabel`: a static C string
    pub label: *const c_char,
}

struct TypeInfo {
    message_type: u8,
    flags: u8,
    s2aw_field: u8,
    response: (u8, u8, u8),
    /// the members after the wire's (never exported or imported)
    local: &'static [Field],
    label: &'static str,
}

const T: u8 = EE_NETMSG_NE_THROTTLE;
const N: u8 = EE_NETMSG_NE_NONE;
const NO_RESPONSE: (u8, u8, u8) = (0, 0, 0);

const fn info(message_type: u8, flags: u8, label: &'static str) -> TypeInfo {
    TypeInfo { message_type, flags, s2aw_field: N, response: NO_RESPONSE, local: &[], label }
}

/// The engine types: the original classes' overrides (`NEMessage.h`), in `ENGINE_TYPES`' order.
static TYPE_INFO: [TypeInfo; 28] = [
    info(0x20, T, "Add Player\0"),
    info(0x21, 0, "Add Player Response\0"),
    info(0x22, T, "Client Connection Request\0"),
    info(0x23, 0, "Client Connection Request Response\0"),
    info(0x24, T, "Client Disconnection Notification\0"),
    info(0x25, 0, "Client Disconnection Notification Response\0"),
    // the communications tick: its response is the tick response whose servers' tick is this tick
    TypeInfo { response: (0x27, 0, 1), ..info(0x26, 0, "Communications Tick\0") },
    info(0x27, 0, "Communications Tick Response\0"),
    // the physics tick's rate is never on the wire: only the local physics thread reads it
    TypeInfo { local: &[Field::new("mPhysicsTicksPerSecond", Kind::U8)], ..info(0x28, 0, "Physics Tick\0") },
    info(0x29, 0, "Player Connection Check\0"),
    info(0x2a, 0, "Player Connection Check Response\0"),
    info(0x2b, EE_NETMSG_NE_PAUSED, "Remove Player\0"),
    info(0x2d, 0, "Start Game\0"),
    info(0x2e, 0, "Start Game Response\0"),
    info(0x2f, T, "Stop Game\0"),
    info(0x30, 0, "Stop Game Response\0"),
    info(0x31, 0, "World Database Checksum\0"),
    info(0x32, T, "World Database Checksum Response\0"),
    // pause and unpause: their unique id first, the send-to-all-and-wait id second; answered by the response with
    // the same unique id
    TypeInfo { s2aw_field: 1, response: (0x34, 0, 0), ..info(0x33, T, "Pause Game\0") },
    info(0x34, 0, "Pause Game Response\0"),
    TypeInfo { s2aw_field: 1, response: (0x36, 0, 0), ..info(0x35, T, "Un-Pause Game\0") },
    info(0x36, 0, "Un-Pause Game Response\0"),
    info(0x37, 0, "Client Reconnection Request\0"),
    info(0x38, 0, "Client Reconnection Request Response\0"),
    info(0x39, T, "Client Request Pause\0"),
    info(0x3a, 0, "Server Is Alive\0"),
    info(0x3b, 0, "Xfer Message\0"),
    info(0x3c, 0, "Xfer Status\0"),
];

fn find(message_type: u8) -> Option<(&'static MsgClass, u32, &'static TypeInfo)> {
    let class = ENGINE_TYPES.iter().find(|t| t.0 == message_type)?.1;
    let info = TYPE_INFO.iter().find(|i| i.message_type == message_type)?;
    Some((&ENGINE_CLASSES[(class - EE_NETMSG_ENGINE_BASE) as usize], class, info))
}

/// The record's members of a type: the wire's, then the local ones.
fn members(c: &'static MsgClass, info: &'static TypeInfo) -> impl Iterator<Item = &'static Field> {
    c.fields.iter().chain(info.local.iter())
}

/// A record member's bytes: the fixed kinds' (a string or the inputs take none).
fn record_bytes(kind: Kind) -> usize {
    match kind {
        Kind::WStr | Kind::Inputs => 0,
        k => k.fixed_size().expect("an engine member of a fixed kind"),
    }
}

/// What a type is (see `EeNetMsgNeType`).
pub fn type_info(message_type: u8) -> EeNetMsgNeType {
    let Some((c, class, info)) = find(message_type) else {
        return EeNetMsgNeType {
            class_id: u32::MAX,
            wire_fields: 0,
            fields: 0,
            record_size: 0,
            flags: EE_NETMSG_NE_THROTTLE,
            s2aw_field: N,
            response_type: 0,
            response_mine: 0,
            response_theirs: 0,
            label: c"".as_ptr(),
        };
    };
    EeNetMsgNeType {
        class_id: class,
        wire_fields: c.fields.len() as u32,
        fields: (c.fields.len() + info.local.len()) as u32,
        record_size: members(c, info).map(|f| record_bytes(f.kind)).sum::<usize>() as u32,
        flags: info.flags,
        s2aw_field: info.s2aw_field,
        response_type: info.response.0,
        response_mine: info.response.1,
        response_theirs: info.response.2,
        label: info.label.as_ptr() as *const c_char,
    }
}

/// No such member.
pub const EE_NETMSG_NE_FIELD_NONE: u8 = 0;
/// An integer (U8, U16, I32, U32): a sender's value is truncated to its size.
pub const EE_NETMSG_NE_FIELD_INTEGER: u8 = 1;
/// A `bool`: a sender's value is stored as value != 0.
pub const EE_NETMSG_NE_FIELD_BOOL: u8 = 2;
/// A string: `offset` is its ordinal among the type's strings.
pub const EE_NETMSG_NE_FIELD_STRING: u8 = 3;
/// The players' inputs (the list).
pub const EE_NETMSG_NE_FIELD_INPUTS: u8 = 4;
/// A fixed block of bytes (the transfer's data): a sender copies it in.
pub const EE_NETMSG_NE_FIELD_BLOCK: u8 = 5;

/// A record member: its kind (`EE_NETMSG_NE_FIELD_*`), its offset in the record (a string's ordinal) and its size.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EeNetMsgNeField {
    pub kind: u8,
    pub offset: u32,
    pub size: u32,
}

/// Member `index` (0 = the first after the header) of a type's record.
pub fn field(message_type: u8, index: u32) -> EeNetMsgNeField {
    let none = EeNetMsgNeField { kind: EE_NETMSG_NE_FIELD_NONE, offset: 0, size: 0 };
    let Some((c, _, info)) = find(message_type) else { return none };
    let Some(f) = members(c, info).nth(index as usize) else { return none };
    let before = || members(c, info).take(index as usize);
    match f.kind {
        Kind::WStr => EeNetMsgNeField {
            kind: EE_NETMSG_NE_FIELD_STRING,
            offset: before().filter(|g| g.kind == Kind::WStr).count() as u32,
            size: 0,
        },
        Kind::Inputs => EeNetMsgNeField { kind: EE_NETMSG_NE_FIELD_INPUTS, offset: 0, size: 0 },
        k => EeNetMsgNeField {
            kind: match k {
                Kind::Bool => EE_NETMSG_NE_FIELD_BOOL,
                Kind::Raw(_) => EE_NETMSG_NE_FIELD_BLOCK,
                _ => EE_NETMSG_NE_FIELD_INTEGER,
            },
            offset: before().map(|g| record_bytes(g.kind)).sum::<usize>() as u32,
            size: record_bytes(k) as u32,
        },
    }
}

/// What an engine message type is: its codec class, member counts, record size, flags, send-to-all-and-wait member,
/// response rule, label (`class_id` = `u32::MAX` for a type the engine does not build).
#[no_mangle]
pub extern "C" fn ee_netmsg_ne_type(message_type: u8) -> EeNetMsgNeType {
    type_info(message_type)
}

/// Member `index` (0 = the first after the header) of an engine type's record; NONE past the last.
#[no_mangle]
pub extern "C" fn ee_netmsg_ne_field(message_type: u8, index: u32) -> EeNetMsgNeField {
    field(message_type, index)
}

// ---- the factory: the builders the engine and the game register by type (the original's sConstructors[256])

/// Builds a message from the received bytes at `data` (its type at `data[1]`); NULL when it cannot.
pub type EeNetMsgBuilder = unsafe extern "C" fn(data: *mut u8) -> *mut c_void;

static BUILDERS: [AtomicUsize; 256] = [const { AtomicUsize::new(0) }; 256];

/// Registers `builder` (NULL: none) for the types `first..=last` (`NEMessage::RegisterKnownMessages` 0x20..0x3c,
/// `EEMessage::RegisterKnownMessages` 0x41..0xfd).
#[no_mangle]
pub extern "C" fn ee_netmsg_register_builder(
    first: u8,
    last: u8,
    builder: Option<unsafe extern "C" fn(data: *mut u8) -> *mut c_void>,
) {
    let value = builder.map_or(0, |b| b as usize);
    for t in first..=last {
        BUILDERS[t as usize].store(value, Ordering::Release);
    }
}

/// `NEMessage::ConvertMemoryToMessage`: the message the builder registered for the type at `data[1]` makes, or NULL
/// when no builder is registered for it.
///
/// # Safety
/// `data` holds a received message; the registered builder is safe to call with it.
#[no_mangle]
pub unsafe extern "C" fn ee_netmsg_build(data: *mut u8) -> *mut c_void {
    let builder = BUILDERS[*data.add(1) as usize].load(Ordering::Acquire);
    if builder == 0 {
        return std::ptr::null_mut();
    }
    let builder: EeNetMsgBuilder = std::mem::transmute::<usize, EeNetMsgBuilder>(builder);
    builder(data)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CStr;

    fn label(t: u8) -> String {
        unsafe { CStr::from_ptr(type_info(t).label) }.to_str().unwrap().to_string()
    }

    #[test]
    fn every_engine_type_has_its_info() {
        for &(t, class) in &ENGINE_TYPES {
            let i = type_info(t);
            assert_eq!(i.class_id, class);
            assert!(!label(t).is_empty());
            assert!(i.fields >= i.wire_fields);
            for k in 0..i.fields {
                assert_ne!(field(t, k).kind, EE_NETMSG_NE_FIELD_NONE, "{t:02x} {k}");
            }
            assert_eq!(field(t, i.fields).kind, EE_NETMSG_NE_FIELD_NONE);
        }
        assert_eq!(TYPE_INFO.len(), ENGINE_TYPES.len());
        // 0x2c and the game's types are not the engine's
        for t in [0x1f, 0x2c, 0x3d, 0x41, 0xff] {
            assert_eq!(type_info(t).class_id, u32::MAX);
            assert_eq!(field(t, 0).kind, EE_NETMSG_NE_FIELD_NONE);
        }
    }

    #[test]
    fn layouts() {
        // NEMClientConnectionRequestResponse: error, UI slot, send-to slot, tick, version
        let f = |i| field(0x23, i);
        assert_eq!(f(0), EeNetMsgNeField { kind: EE_NETMSG_NE_FIELD_INTEGER, offset: 0, size: 4 });
        assert_eq!(f(1), EeNetMsgNeField { kind: EE_NETMSG_NE_FIELD_INTEGER, offset: 4, size: 1 });
        assert_eq!(f(2), EeNetMsgNeField { kind: EE_NETMSG_NE_FIELD_INTEGER, offset: 5, size: 1 });
        assert_eq!(f(3), EeNetMsgNeField { kind: EE_NETMSG_NE_FIELD_INTEGER, offset: 6, size: 4 });
        assert_eq!(f(4), EeNetMsgNeField { kind: EE_NETMSG_NE_FIELD_STRING, offset: 0, size: 0 });
        assert_eq!(type_info(0x23).record_size, 10);
        // NEMClientConnectionRequest: firewall, version (string 0), name (string 1), checksum
        assert_eq!(field(0x22, 0).kind, EE_NETMSG_NE_FIELD_BOOL);
        assert_eq!(field(0x22, 2), EeNetMsgNeField { kind: EE_NETMSG_NE_FIELD_STRING, offset: 1, size: 0 });
        assert_eq!(field(0x22, 3), EeNetMsgNeField { kind: EE_NETMSG_NE_FIELD_INTEGER, offset: 1, size: 4 });
        // the physics tick: four on the wire, the rate after them
        let p = type_info(0x28);
        assert_eq!((p.wire_fields, p.fields, p.record_size), (4, 5, 8));
        assert_eq!(field(0x28, 4), EeNetMsgNeField { kind: EE_NETMSG_NE_FIELD_INTEGER, offset: 7, size: 1 });
        // the transfer: id, size, block
        assert_eq!(field(0x3b, 2), EeNetMsgNeField { kind: EE_NETMSG_NE_FIELD_BLOCK, offset: 5, size: 0x400 });
        assert_eq!(type_info(0x3b).record_size, 0x405);
        // the ticks' inputs
        assert_eq!(field(0x26, 5).kind, EE_NETMSG_NE_FIELD_INPUTS);
        assert_eq!(field(0x27, 3).kind, EE_NETMSG_NE_FIELD_INPUTS);
        assert_eq!(type_info(0x27).record_size, 10);
        // a header-only type
        assert_eq!((type_info(0x3a).fields, type_info(0x3a).record_size), (0, 0));
    }

    #[test]
    fn flags_responses_labels() {
        let throttling: Vec<u8> =
            ENGINE_TYPES.iter().map(|t| t.0).filter(|&t| type_info(t).flags & EE_NETMSG_NE_THROTTLE != 0).collect();
        assert_eq!(throttling, [0x20, 0x22, 0x24, 0x2f, 0x32, 0x33, 0x35, 0x39]);
        let paused: Vec<u8> =
            ENGINE_TYPES.iter().map(|t| t.0).filter(|&t| type_info(t).flags & EE_NETMSG_NE_PAUSED != 0).collect();
        assert_eq!(paused, [0x2b]);
        assert_eq!(type_info(0x33).s2aw_field, 1);
        assert_eq!(type_info(0x35).s2aw_field, 1);
        assert_eq!(type_info(0x26).s2aw_field, N);
        let r = type_info(0x26);
        assert_eq!((r.response_type, r.response_mine, r.response_theirs), (0x27, 0, 1));
        assert_eq!(type_info(0x35).response_type, 0x36);
        assert_eq!(type_info(0x27).response_type, 0);
        assert_eq!(label(0x35), "Un-Pause Game");
        assert_eq!(label(0x3b), "Xfer Message");
        assert_eq!(label(0x2c), "");
    }

    unsafe extern "C" fn build_one(data: *mut u8) -> *mut c_void {
        (*data.add(2) as usize + 1) as *mut c_void
    }

    #[test]
    fn the_factory_calls_the_registered_builder() {
        // (types no other test of this crate registers)
        let mut data = [0u8, 0xf0, 7, 0];
        assert!(unsafe { ee_netmsg_build(data.as_mut_ptr()) }.is_null());
        ee_netmsg_register_builder(0xf0, 0xf1, Some(build_one));
        assert_eq!(unsafe { ee_netmsg_build(data.as_mut_ptr()) } as usize, 8);
        data[1] = 0xf1;
        assert_eq!(unsafe { ee_netmsg_build(data.as_mut_ptr()) } as usize, 8);
        data[1] = 0xf2;
        assert!(unsafe { ee_netmsg_build(data.as_mut_ptr()) }.is_null());
        ee_netmsg_register_builder(0xf1, 0xf1, None);
        data[1] = 0xf1;
        assert!(unsafe { ee_netmsg_build(data.as_mut_ptr()) }.is_null());
    }
}
