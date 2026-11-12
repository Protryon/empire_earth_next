//! The C ABI of WON's network layer for the C++ forwarders: WONStatusToString, NetStats, IPAddr, AsyncSocket's core
//! and the socket ops' algorithms. Addresses cross as (address, port), both in network byte order (a
//! `PlatSockAddrIn`'s `mAddress` and `mPort`). Statuses are `long` (the C++ `WONStatus`).

use crate::ipaddr::{self, Addr};
use crate::netstats;
use crate::ops::{self, LengthStep};
use crate::socket;
use crate::status;
use std::ffi::{c_char, c_long, c_void};

unsafe fn text<'a>(data: *const u8, length: usize) -> &'a [u8] {
    if data.is_null() || length == 0 {
        &[]
    } else {
        // SAFETY: the caller passes `length` bytes
        unsafe { std::slice::from_raw_parts(data, length) }
    }
}

unsafe fn put_addr(a: Option<Addr>, address: *mut u32, port: *mut u16) -> bool {
    match a {
        Some(a) => {
            // SAFETY: the caller's out-parameters
            unsafe {
                *address = a.address;
                *port = a.port;
            }
            true
        }
        None => false,
    }
}

/// Copies `src` with a NUL into `out` of `size` bytes (cut to fit); returns `src`'s length.
unsafe fn put_c(src: &[u8], out: *mut c_char, size: usize) -> usize {
    if !out.is_null() && size > 0 {
        let n = src.len().min(size - 1);
        // SAFETY: `out` has `size` bytes
        unsafe {
            std::ptr::copy_nonoverlapping(src.as_ptr(), out.cast(), n);
            *out.add(n) = 0;
        }
    }
    src.len()
}

// ---- WONStatus

thread_local! {
    /// The text of the last unnamed status this thread asked for (the C++ shared one 1024-byte buffer).
    static UNNAMED_STATUS: std::cell::RefCell<std::ffi::CString> = std::cell::RefCell::new(std::ffi::CString::default());
}

/// `WONStatusToString` (WONRouting.h's inline shim; WONStatus.cpp is deleted): the status's name. A `WS_` name is a
/// static string; another (a number, a DB proxy game-specific error) is valid until this thread's next unnamed one.
#[no_mangle]
pub extern "C" fn ee_won_status_text(s: c_long) -> *const c_char {
    if let Some(n) = status::named(s as status::Status) {
        return n.as_ptr();
    }
    let text = std::ffi::CString::new(status::unnamed(s as status::Status)).expect("digits and letters");
    UNNAMED_STATUS.with(|t| {
        *t.borrow_mut() = text;
        t.borrow().as_ptr()
    })
}

// ---- NetStats

#[no_mangle]
pub extern "C" fn ee_won_netstats_enable(on: bool) {
    netstats::enable(on)
}

/// Adds to counter `counter` (0 constructed sockets, 1 open sockets, 2 bytes sent, 3 bytes received, 4 messages
/// sent, 5 messages received) while counting is on.
#[no_mangle]
pub extern "C" fn ee_won_netstats_add(counter: u32, amount: i64) {
    if let Some(c) = netstats::Counter::from_u32(counter) {
        netstats::add(c, amount)
    }
}

#[no_mangle]
pub extern "C" fn ee_won_netstats_get(counter: u32) -> i64 {
    netstats::Counter::from_u32(counter).map(netstats::get).unwrap_or(0)
}

// ---- IPAddr

/// `Fn2B2660`: a dotted address or a host name (looked up) and a host-order port -> the address; false when it does
/// not resolve.
///
/// # Safety
/// `host` holds `length` bytes; the out-parameters are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_resolve(
    host: *const u8,
    length: usize,
    port: u16,
    address: *mut u32,
    port_out: *mut u16,
) -> bool {
    unsafe { put_addr(ipaddr::resolve(text(host, length), port), address, port_out) }
}

/// `Fn2B2890`'s port: the text after the colon as `atoi` reads it, cut to 16 bits (host order).
///
/// # Safety
/// `text_in` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_port_of(text_in: *const u8, length: usize) -> u16 {
    ipaddr::port_of(unsafe { text(text_in, length) })
}

/// `SetWithDefaultPortNoDNS`.
///
/// # Safety
/// As `ee_won_ipaddr_resolve`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_parse_no_dns(
    text_in: *const u8,
    length: usize,
    default_port: u16,
    address: *mut u32,
    port: *mut u16,
) -> bool {
    unsafe { put_addr(ipaddr::parse_no_dns(text(text_in, length), default_port), address, port) }
}

/// The dotted form of a network-order address into `out` (16 bytes).
///
/// # Safety
/// `out` has 16 bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_dotted(address: u32, out: *mut c_char) {
    unsafe { put_c(ipaddr::dotted(address).as_bytes(), out, 16) };
}

/// `GetHostAndPortString`: "host:port" (at most 49 bytes) into `out` (50 bytes); returns its length.
///
/// # Safety
/// `host` holds `length` bytes; `out` has 50 bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_host_and_port(
    host: *const u8,
    length: usize,
    port: u16,
    out: *mut c_char,
) -> usize {
    let s = ipaddr::host_and_port(unsafe { text(host, length) }, port);
    unsafe { put_c(&s, out, 50) }
}

/// The order of two addresses' 6-byte wire forms (`operator<`, `==`): negative, 0 or positive.
///
/// # Safety
/// `a` and `b` point at 6 bytes each (a `PlatSockAddrIn`'s port and address).
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_compare_six(a: *const u8, b: *const u8) -> i32 {
    // SAFETY: 6 bytes each
    let (a, b) = unsafe { (&*(a as *const [u8; 6]), &*(b as *const [u8; 6])) };
    ipaddr::compare_six(&Addr::from_six_bytes(a), &Addr::from_six_bytes(b)) as i32
}

/// `StringCompare` (network-order addresses and ports).
#[no_mangle]
pub extern "C" fn ee_won_ipaddr_string_compare(a_address: u32, a_port: u16, b_address: u32, b_port: u16) -> i32 {
    ipaddr::string_compare(&Addr { address: a_address, port: a_port }, &Addr { address: b_address, port: b_port })
}

/// Receives one local address: its name (`length` bytes) and the host-order address.
pub type EeWonAddressSink = unsafe extern "C" fn(context: *mut c_void, name: *const u8, length: usize, host: u32);

/// `LobbyGetNetAddresses`: each of this machine's addresses to `sink`, in order.
///
/// # Safety
/// The sink is called during the call only.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_local_addresses(details: bool, sink: EeWonAddressSink, context: *mut c_void) {
    for (name, host) in ipaddr::local_addresses(details) {
        // SAFETY: the sink's contract
        unsafe { sink(context, name.as_ptr(), name.len(), host) };
    }
}

/// Whether a host-order address is one of this machine's (`SetLocalHost`'s test: the host name's addresses).
#[no_mangle]
pub extern "C" fn ee_won_ipaddr_is_local(host: u32) -> bool {
    ipaddr::local_addresses(false).iter().any(|&(_, a)| a == host)
}

/// This machine's name into `out` (`size` bytes); false when the system gives none.
///
/// # Safety
/// `out` has `size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_host_name(out: *mut c_char, size: usize) -> bool {
    match ipaddr::host_name() {
        Some(n) => {
            unsafe { put_c(&n, out, size) };
            true
        }
        None => false,
    }
}

/// `AsyncSocket::HasInternetConnection`.
#[no_mangle]
pub extern "C" fn ee_won_has_internet_connection() -> bool {
    ipaddr::has_internet_connection()
}

// ---- AsyncSocket's socket system (the socket itself is sockobj.rs's object)

/// `StartWinsock`.
#[no_mangle]
pub extern "C" fn ee_won_socket_start() -> c_long {
    socket::start() as c_long
}

/// `StopWinsock`.
#[no_mangle]
pub extern "C" fn ee_won_socket_stop() -> c_long {
    socket::stop() as c_long
}

// ---- the ops

/// WON's millisecond tick (`WONTickCount`).
#[no_mangle]
pub extern "C" fn ee_won_tick() -> u32 {
    ops::tick()
}

/// `AsyncOp::RealTimeLeft` / `AsyncSocket::TimeLeft` now.
#[no_mangle]
pub extern "C" fn ee_won_real_time_left(start: u32, timeout: u32) -> u32 {
    ops::real_time_left(start, timeout, ops::tick())
}

/// `RecvMsgOp`'s length prefix: 0 and the body's length in `*body`, 1 for the 2-byte escape (a 4-byte prefix
/// follows), 2 for an invalid length.
///
/// # Safety
/// `prefix` holds `length` bytes; `body` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_length_step(prefix: *const u8, length: usize, body: *mut u32) -> i32 {
    match ops::length_step(unsafe { text(prefix, length) }) {
        LengthStep::Body(n) => {
            unsafe { *body = n };
            0
        }
        LengthStep::Widen => 1,
        LengthStep::Invalid => 2,
    }
}

/// `RecvMsgOp::Start`'s length field test.
#[no_mangle]
pub extern "C" fn ee_won_op_valid_length_field(size: u32) -> bool {
    ops::valid_length_field(size)
}

/// `RecvBytesOp`'s chunk read size.
#[no_mangle]
pub extern "C" fn ee_won_op_chunk_length(length: u32, bytes_read: u32) -> i32 {
    ops::chunk_length(length, bytes_read)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn status_names_through_the_abi() {
        let text = |s| unsafe { std::ffi::CStr::from_ptr(ee_won_status_text(s)) }.to_str().unwrap().to_string();
        assert_eq!(text(100001), "WS_TimedOut");
        assert_eq!(text(-2950), "DBProxyServ_GameSpecificError-2950");
        assert_eq!(text(123456), "123456");
        // a named one does not disturb the last unnamed text
        let p = ee_won_status_text(99_999);
        let _ = text(0);
        assert_eq!(unsafe { std::ffi::CStr::from_ptr(p) }.to_str().unwrap(), "99999");
    }

    #[test]
    fn the_cpp_status_constants_are_the_rust_table() {
        // include/ee/wonapi/WONStatus.h's enum, the names the C++ callers use, against status.rs's table
        let header = include_str!("../../../../include/ee/wonapi/WONStatus.h");
        let mut seen = 0;
        for line in header.lines() {
            let line = line.trim();
            let Some(rest) = line.strip_prefix("WS_") else { continue };
            let Some((name, value)) = rest.split_once('=') else { continue };
            let value: status::Status =
                value.split(',').next().unwrap().trim().parse().unwrap_or_else(|_| panic!("{line}"));
            let want = format!("WS_{}", name.trim());
            assert_eq!(status::named(value).map(|n| n.to_str().unwrap().to_string()), Some(want), "{line}");
            seen += 1;
        }
        assert_eq!(seen, status::NAMES.len());
    }

    #[test]
    fn length_steps_through_the_abi() {
        let mut body = 0;
        assert_eq!(unsafe { ee_won_op_length_step([9u8, 0, 0, 0].as_ptr(), 4, &mut body) }, 0);
        assert_eq!(body, 5);
        assert_eq!(unsafe { ee_won_op_length_step([0xffu8, 0xff].as_ptr(), 2, &mut body) }, 1);
        assert_eq!(unsafe { ee_won_op_length_step([1u8, 2, 3].as_ptr(), 3, &mut body) }, 2);
    }

    #[test]
    fn addresses_through_the_abi() {
        socket::start();
        let (mut a, mut p) = (0u32, 0u16);
        let t = b"127.0.0.1:1234";
        assert!(unsafe { ee_won_ipaddr_parse_no_dns(t.as_ptr(), t.len(), 9, &mut a, &mut p) });
        assert_eq!((u32::from_be(a), u16::from_be(p)), (0x7f00_0001, 1234));
        let mut out = [0 as c_char; 50];
        let n = unsafe { ee_won_ipaddr_host_and_port(b"h".as_ptr(), 1, 80, out.as_mut_ptr()) };
        assert_eq!(n, 4);
        let six_a = [0x04u8, 0xd2, 127, 0, 0, 1];
        let six_b = [0x04u8, 0xd3, 0, 0, 0, 0];
        assert!(unsafe { ee_won_ipaddr_compare_six(six_a.as_ptr(), six_b.as_ptr()) } < 0);
        assert_eq!(unsafe { ee_won_ipaddr_compare_six(six_a.as_ptr(), six_a.as_ptr()) }, 0);
    }
}
