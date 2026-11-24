//! A routing request's values whole in Rust (R6 WON round 34): WONRouting.h's `RoutingRequestValues`, the values a
//! request helper hands [`crate::routecpp::request`] in its parameter order: numbers, texts (`wchar_t` text as UTF-16
//! little-endian units), data types, a buffer (its number says whether there is one) and the recipients (16-bit
//! little-endian ids). The C++ class owns one builder and forwards each value; `Run` makes the request and spends it.
//!
//! Reached by nothing in the game today: the routing servers are gone.

use std::ffi::c_void;

use crate::routecpp::{request, Value};

/// The values in order.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct EeWonRouteReq {
    pub values: Vec<Value>,
}

impl EeWonRouteReq {
    /// `Add(num, bytes)`.
    pub fn add(&mut self, num: i64, bytes: &[u8]) {
        self.values.push(Value { num, bytes: bytes.to_vec() });
    }
    /// `W(text)`: the UTF-16 units little-endian.
    pub fn wide(&mut self, units: &[u16]) {
        self.values.push(Value::units(units));
    }
    /// `L(ids)`: each id's low byte, then its high byte.
    pub fn ids(&mut self, ids: &[u16]) {
        self.values.push(Value::units(ids));
    }
}

unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: the callers' contract: `n` elements
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// A builder with no value (the C++ frees it with `ee_won_routereq_free`).
#[no_mangle]
pub extern "C" fn ee_won_routereq_new() -> *mut EeWonRouteReq {
    Box::into_raw(Box::default())
}
/// # Safety
/// `r` is NULL or a builder from `ee_won_routereq_new`, not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routereq_free(r: *mut EeWonRouteReq) {
    if !r.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(r) });
    }
}
/// A number with bytes: `N(num)` (no bytes), `S(type)` (0 and the bytes), `D(buffer)` (1 and the buffer's bytes, or
/// 0 and none for no buffer).
///
/// # Safety
/// `r` is a live builder; `data` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routereq_add(r: *mut EeWonRouteReq, num: i64, data: *const u8, len: usize) {
    unsafe { (*r).add(num, slice(data, len)) }
}
/// `W(text)`: `count` `wchar_t`s of `wchar_size` bytes.
///
/// # Safety
/// `r` is a live builder; `wide` holds `count` characters.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routereq_wide(
    r: *mut EeWonRouteReq,
    wide: *const c_void,
    wchar_size: u32,
    count: usize,
) {
    let u = unsafe { crate::ffi::wide_units(wide, wchar_size, count) };
    unsafe { (*r).wide(&u) }
}
/// `L(ids)`: the recipients.
///
/// # Safety
/// `r` is a live builder; `ids` holds `count` ids.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routereq_ids(r: *mut EeWonRouteReq, ids: *const u16, count: usize) {
    unsafe { (*r).ids(slice(ids, count)) }
}
/// The values to `sink` in order, each its number and bytes (the drivers' view).
///
/// # Safety
/// `r` is a live builder; `sink` is valid with `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routereq_each(
    r: *const EeWonRouteReq,
    sink: unsafe extern "C" fn(context: *mut c_void, num: i64, data: *const u8, len: usize),
    context: *mut c_void,
) {
    for v in unsafe { &(*r).values } {
        unsafe { sink(context, v.num, v.bytes.as_ptr(), v.bytes.len()) }
    }
}
/// `Run(conn, kind, completion)`: the request of the kind with the values on the connection (`ee_won_routeconn_request`).
///
/// # Safety
/// `r` is a live builder; `conn` a live connection.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routereq_run(
    r: *const EeWonRouteReq,
    conn: *mut c_void,
    kind: i32,
    completion: *mut c_void,
) {
    request(conn, kind, unsafe { &(*r).values }, completion)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn values_in_order() {
        let r = ee_won_routereq_new();
        unsafe {
            ee_won_routereq_add(r, 7, std::ptr::null(), 0);
            let w: [u16; 2] = [0x41, 0x263a];
            ee_won_routereq_wide(r, w.as_ptr() as *const c_void, 2, 2);
            let ids = [0x0102u16, 3];
            ee_won_routereq_ids(r, ids.as_ptr(), 2);
            ee_won_routereq_add(r, 1, b"xy".as_ptr(), 2);
            let v = &(*r).values;
            assert_eq!(v[0], Value { num: 7, bytes: vec![] });
            assert_eq!(v[1].bytes, vec![0x41, 0, 0x3a, 0x26]);
            assert_eq!(v[2].bytes, vec![2, 1, 3, 0]);
            assert_eq!(v[3], Value { num: 1, bytes: b"xy".to_vec() });
            ee_won_routereq_free(r);
        }
    }

    #[test]
    fn utf32_text_becomes_pairs() {
        let r = ee_won_routereq_new();
        unsafe {
            let w: [u32; 1] = [0x1f600];
            ee_won_routereq_wide(r, w.as_ptr() as *const c_void, 4, 1);
            assert_eq!((&(*r).values)[0].bytes, vec![0x3d, 0xd8, 0x00, 0xde]);
            ee_won_routereq_free(r);
        }
    }
}
