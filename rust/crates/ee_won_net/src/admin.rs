//! WONInternal's admin ops (`src/Empire Earth/WONAPI/WONInternal`): the DB proxy requests of the master key,
//! key check, banned key, key subscription, CD key account, version, patch and module list ops, and their replies'
//! records, each op's message format a table here.
//!
//! The ops are Rust ops (adminop.rs) over these tables; `tests/won/won_admin_test.cpp` checks the tables against the
//! original's statements through `tests/won/won_admin_msg.h`. Reached by nothing in the game (the WON admin servers
//! are gone).

use std::ffi::c_void;

use crate::ffi::EeWonByteSink;
use crate::wire::{PastEnd, Reader, Writer};

/// A request field.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum F {
    /// a wide string with a length field of this size
    WStr(u8),
    /// a string with a length field of this size
    Str(u8),
    Short,
    Long,
    Byte,
    Bool,
    /// the previous field's value of bytes (the master key)
    Raw,
    /// a count, then that many strings with 1-byte lengths (the key lists): the count as a short
    Keys,
}

// the ops
pub const EE_WON_ADMIN_INSERT_MASTER_KEY: i32 = 1;
pub const EE_WON_ADMIN_UPDATE_MASTER_KEY: i32 = 2;
pub const EE_WON_ADMIN_DELETE_MASTER_KEY: i32 = 3;
pub const EE_WON_ADMIN_QUERY_MASTER_KEY: i32 = 4;
pub const EE_WON_ADMIN_AUTH_CHECK_KEY: i32 = 5;
pub const EE_WON_ADMIN_INSERT_BANNED_KEY: i32 = 6;
pub const EE_WON_ADMIN_QUERY_BANNED_KEY: i32 = 7;
pub const EE_WON_ADMIN_QUERY_LIST_BANNED_KEY: i32 = 8;
pub const EE_WON_ADMIN_REMOVE_BANNED_KEY: i32 = 9;
pub const EE_WON_ADMIN_INSERT_KEY_SUBSCRIBE: i32 = 10;
pub const EE_WON_ADMIN_UPDATE_KEY_SUBSCRIBE: i32 = 11;
pub const EE_WON_ADMIN_REMOVE_KEY_SUBSCRIBE: i32 = 12;
pub const EE_WON_ADMIN_QUERY_KEY_SUBSCRIBE: i32 = 13;
/// the CD key account's request before the profile (the key, the community)
pub const EE_WON_ADMIN_ACCOUNT_FROM_CDKEY: i32 = 14;
/// the same with the action flag
pub const EE_WON_ADMIN_ACCOUNT_FROM_CDKEY_EX: i32 = 15;
/// the version ops' requests: the product (before the version record)
pub const EE_WON_ADMIN_PRODUCT: i32 = 16;
/// `RemoveVersionOp`: product, config, version
pub const EE_WON_ADMIN_REMOVE_VERSION: i32 = 17;
/// `GetVersionListOp`: product, config, state filter
pub const EE_WON_ADMIN_GET_VERSION_LIST: i32 = 18;
/// `RemovePatchOp`: product, config, from, to, patch URL
pub const EE_WON_ADMIN_REMOVE_PATCH: i32 = 19;
/// `ActivatePatchOp`: the same and the active flag
pub const EE_WON_ADMIN_ACTIVATE_PATCH: i32 = 20;
/// `GetPatchServerModuleListOp`'s reply (its request has no data)
pub const EE_WON_ADMIN_MODULE_LIST: i32 = 21;

fn layout(op: i32) -> &'static [F] {
    use F::*;
    match op {
        EE_WON_ADMIN_INSERT_MASTER_KEY => &[WStr(2), Str(2), Short, Bool, Long, Long, Byte, Raw, Long],
        EE_WON_ADMIN_UPDATE_MASTER_KEY => &[WStr(2), Str(2), Short, Bool, Long, Long, Long],
        EE_WON_ADMIN_DELETE_MASTER_KEY => &[WStr(2), Str(2), Short],
        EE_WON_ADMIN_QUERY_MASTER_KEY | EE_WON_ADMIN_QUERY_BANNED_KEY => &[WStr(2), Str(2)],
        EE_WON_ADMIN_AUTH_CHECK_KEY => &[WStr(1), Str(1), Keys],
        EE_WON_ADMIN_INSERT_BANNED_KEY => &[WStr(2), Str(2), Long, Keys],
        EE_WON_ADMIN_QUERY_LIST_BANNED_KEY | EE_WON_ADMIN_REMOVE_BANNED_KEY => &[WStr(2), Str(2), Keys],
        EE_WON_ADMIN_INSERT_KEY_SUBSCRIBE => &[Str(2), WStr(2), Keys, Long, Long, Long, Long],
        EE_WON_ADMIN_UPDATE_KEY_SUBSCRIBE => &[Str(2), WStr(2), Keys, Short, Long, Long, Long, Long, Long],
        EE_WON_ADMIN_REMOVE_KEY_SUBSCRIBE | EE_WON_ADMIN_QUERY_KEY_SUBSCRIBE => &[Str(2), WStr(2), Keys],
        EE_WON_ADMIN_ACCOUNT_FROM_CDKEY => &[Str(2), Long],
        EE_WON_ADMIN_ACCOUNT_FROM_CDKEY_EX => &[Str(2), Long, Byte],
        EE_WON_ADMIN_PRODUCT => &[Str(2)],
        EE_WON_ADMIN_REMOVE_VERSION => &[Str(2), Str(2), Str(2)],
        EE_WON_ADMIN_GET_VERSION_LIST => &[Str(2), Str(2), Byte],
        EE_WON_ADMIN_REMOVE_PATCH => &[Str(2), Str(2), Str(2), Str(2), Str(2)],
        EE_WON_ADMIN_ACTIVATE_PATCH => &[Str(2), Str(2), Str(2), Str(2), Str(2), Byte],
        _ => &[],
    }
}

/// A value handed over: a number, or bytes (a wide string's as UTF-16 LE).
#[derive(Clone, Copy, Debug, Default)]
pub struct Value<'a> {
    pub num: i64,
    pub bytes: &'a [u8],
}

/// An op's request data from its members in its table's order (a key list: its count, then its strings).
pub fn request(op: i32, values: &[Value]) -> Vec<u8> {
    let mut w = Writer::new(0, true);
    let mut v = values.iter();
    let mut next = || v.next().copied().unwrap_or_default();
    let mut last = 0i64;
    for f in layout(op) {
        match *f {
            F::WStr(size) => {
                let b = next().bytes;
                let units: Vec<u16> = b.as_chunks::<2>().0.iter().map(|c| u16::from_le_bytes(*c)).collect();
                w.append_wstring(&units, size);
            }
            F::Str(size) => w.append_string(next().bytes, size),
            F::Short => w.append_short(next().num as i16),
            F::Long => w.append_long(next().num as i32),
            F::Byte => {
                last = next().num;
                w.append_byte(last as u8);
            }
            F::Bool => w.append_bool(next().num != 0),
            F::Raw => {
                let b = next().bytes;
                w.append_bytes(&b[..(last as u8 as usize).min(b.len())]);
            }
            F::Keys => {
                let n = next().num;
                w.append_short(n as i16);
                for _ in 0..n {
                    w.append_string(next().bytes, 1);
                }
            }
        }
    }
    w.data().to_vec()
}

/// A reply record's field.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum R {
    Short,
    Long,
    Byte,
    /// a string with a 1-byte length
    Str1,
    /// a record of a 2-byte size (read by its own reader)
    Rec,
}

/// The replies' record lists: the count signed or not, the record's fields.
fn reply_layout(op: i32) -> Option<(bool, &'static [R])> {
    use R::*;
    Some(match op {
        EE_WON_ADMIN_QUERY_MASTER_KEY => (true, &[Short, Long, Long, Byte, Long]),
        EE_WON_ADMIN_AUTH_CHECK_KEY => (true, &[Short]),
        EE_WON_ADMIN_INSERT_BANNED_KEY
        | EE_WON_ADMIN_REMOVE_BANNED_KEY
        | EE_WON_ADMIN_INSERT_KEY_SUBSCRIBE
        | EE_WON_ADMIN_UPDATE_KEY_SUBSCRIBE
        | EE_WON_ADMIN_REMOVE_KEY_SUBSCRIBE => (false, &[Short]),
        EE_WON_ADMIN_QUERY_BANNED_KEY => (false, &[Str1, Long]),
        EE_WON_ADMIN_QUERY_LIST_BANNED_KEY => (false, &[Long, Short]),
        EE_WON_ADMIN_QUERY_KEY_SUBSCRIBE => (true, &[Short, Long, Long, Long, Long, Long]),
        EE_WON_ADMIN_MODULE_LIST => (false, &[Long]),
        EE_WON_ADMIN_GET_VERSION_LIST => (false, &[Rec]),
        _ => return None,
    })
}

/// One value read: a number, or a string's range in the reply (`whole` false: its length was read but not its
/// bytes, so the original erased the member).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonAdminField {
    /// `EE_WON_ADMIN_VALUE`, `EE_WON_ADMIN_END`, `EE_WON_ADMIN_CUT`
    pub tag: u8,
    pub whole: bool,
    pub num: i32,
    pub at: u32,
    pub len: u32,
}

pub const EE_WON_ADMIN_VALUE: u8 = 0;
/// the read ended whole (`at`: where the account reply's profile starts)
pub const EE_WON_ADMIN_END: u8 = 1;
/// the read failed here
pub const EE_WON_ADMIN_CUT: u8 = 2;

fn value(num: i32) -> EeWonAdminField {
    EeWonAdminField { tag: EE_WON_ADMIN_VALUE, whole: true, num, ..Default::default() }
}

/// A reply's values: a record list's whole records' values in order (a cut record gives none of its values), or
/// the CD key account reply's five strings (each assigned as read); then `EE_WON_ADMIN_END` or `EE_WON_ADMIN_CUT`.
pub fn reply(op: i32, data: &[u8]) -> Vec<EeWonAdminField> {
    let mut out = Vec::new();
    let mut r = Reader::new(data);
    let end = if op == EE_WON_ADMIN_ACCOUNT_FROM_CDKEY {
        account(&mut r, &mut out)
    } else if let Some((signed, fields)) = reply_layout(op) {
        list(&mut r, signed, fields, &mut out)
    } else {
        Ok(())
    };
    let tag = if end.is_ok() { EE_WON_ADMIN_END } else { EE_WON_ADMIN_CUT };
    out.push(EeWonAdminField { tag, at: r.pos() as u32, ..Default::default() });
    out
}

fn list(r: &mut Reader, signed: bool, fields: &[R], out: &mut Vec<EeWonAdminField>) -> Result<(), PastEnd> {
    let n = r.read_short()?;
    let n = if signed { i32::from(n).max(0) } else { i32::from(n as u16) };
    for _ in 0..n {
        let mut rec = Vec::with_capacity(fields.len());
        for f in fields {
            rec.push(match f {
                R::Short => value(i32::from(r.read_short()?)),
                R::Long => value(r.read_long()?),
                R::Byte => value(i32::from(r.read_byte()?)),
                R::Str1 => {
                    let s = r.read_string(1)?;
                    EeWonAdminField {
                        tag: EE_WON_ADMIN_VALUE,
                        whole: true,
                        num: 0,
                        at: (r.pos() - s.len()) as u32,
                        len: s.len() as u32,
                    }
                }
                R::Rec => {
                    let n = r.read_short()? as u16 as usize;
                    let at = r.pos();
                    r.read_bytes(n)?;
                    EeWonAdminField { tag: EE_WON_ADMIN_VALUE, whole: true, num: 0, at: at as u32, len: n as u32 }
                }
            });
        }
        out.extend(rec);
    }
    Ok(())
}

fn account(r: &mut Reader, out: &mut Vec<EeWonAdminField>) -> Result<(), PastEnd> {
    // user (wide), email, password (wide), last login, activated
    for wide in [true, false, true, false, false] {
        let n = r.read_length(2)?;
        let bytes = if wide { (n as usize).checked_mul(2).ok_or(PastEnd)? } else { n as usize };
        let at = r.pos();
        if r.read_bytes(bytes).is_err() {
            out.push(EeWonAdminField { tag: EE_WON_ADMIN_VALUE, whole: false, ..Default::default() });
            return Err(PastEnd);
        }
        out.push(EeWonAdminField { tag: EE_WON_ADMIN_VALUE, whole: true, num: 0, at: at as u32, len: n });
    }
    Ok(())
}

// ---- the C ABI

unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if n == 0 || p.is_null() {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// A member handed over.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonAdminValue {
    pub num: i64,
    pub data: *const u8,
    pub len: usize,
}

/// An op's request data into `sink` (`values`: its members in its table's order).
///
/// # Safety
/// `values` holds `count` values whose byte ranges are valid; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_admin_request(
    op: i32,
    values: *const EeWonAdminValue,
    count: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let v: Vec<Value> = unsafe { slice(values, count) }
        .iter()
        .map(|x| Value { num: x.num, bytes: unsafe { slice(x.data, x.len) } })
        .collect();
    let m = request(op, &v);
    unsafe { sink(context, m.as_ptr(), m.len()) }
}

/// The values of a reply, handed over once.
pub type EeWonAdminSink = unsafe extern "C" fn(context: *mut c_void, fields: *const EeWonAdminField, count: usize);

/// A reply's values into `sink`.
///
/// # Safety
/// `data` holds `len` bytes; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_admin_reply(
    op: i32,
    data: *const u8,
    len: usize,
    sink: EeWonAdminSink,
    context: *mut c_void,
) {
    let f = reply(op, unsafe { slice(data, len) });
    unsafe { sink(context, f.as_ptr(), f.len()) }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn n(num: i64) -> Value<'static> {
        Value { num, bytes: &[] }
    }

    fn b(bytes: &'static [u8]) -> Value<'static> {
        Value { num: 0, bytes }
    }

    #[test]
    fn requests() {
        let v = [b(&[0x41, 0]), b(b"p"), n(3), n(1), n(-1), n(2), n(2), b(&[9, 8, 7]), n(1)];
        assert_eq!(
            request(EE_WON_ADMIN_INSERT_MASTER_KEY, &v),
            [1, 0, 0x41, 0, 1, 0, b'p', 3, 0, 1, 255, 255, 255, 255, 2, 0, 0, 0, 2, 9, 8, 1, 0, 0, 0]
        );
        let v = [b(&[0x41, 0]), b(b"p"), n(2), b(b"ab"), b(b"c")];
        assert_eq!(request(EE_WON_ADMIN_AUTH_CHECK_KEY, &v), [1, 0x41, 0, 1, b'p', 2, 0, 2, b'a', b'b', 1, b'c']);
        assert_eq!(request(EE_WON_ADMIN_GET_VERSION_LIST, &[b(b"x"), b(b""), n(3)]), [1, 0, b'x', 0, 0, 3]);
        assert!(request(99, &[]).is_empty());
    }

    #[test]
    fn replies() {
        let r = reply(EE_WON_ADMIN_INSERT_BANNED_KEY, &[2, 0, 5, 0, 6, 0]);
        assert_eq!(r.iter().map(|f| f.num).collect::<Vec<_>>()[..2], [5, 6]);
        assert_eq!(r[2].tag, EE_WON_ADMIN_END);
        // a record cut: none of its values
        let r = reply(EE_WON_ADMIN_QUERY_BANNED_KEY, &[2, 0, 1, b'a', 7, 0, 0, 0, 1, b'b', 9]);
        assert_eq!(r.len(), 3);
        assert_eq!((r[0].at, r[0].len, r[1].num, r[2].tag), (3, 1, 7, EE_WON_ADMIN_CUT));
        // a negative signed count reads nothing more
        assert_eq!(reply(EE_WON_ADMIN_AUTH_CHECK_KEY, &[0xff, 0xff])[0].tag, EE_WON_ADMIN_END);
        assert_eq!(reply(EE_WON_ADMIN_MODULE_LIST, &[0xff, 0xff]).last().unwrap().tag, EE_WON_ADMIN_CUT);
        // the account reply: cut inside the email
        let r = reply(EE_WON_ADMIN_ACCOUNT_FROM_CDKEY, &[1, 0, 0x41, 0, 3, 0, b'a']);
        assert_eq!((r[0].whole, r[0].len, r[1].whole, r[2].tag), (true, 1, false, EE_WON_ADMIN_CUT));
    }
}
