//! The C ABI of WON's network layer for the C++ forwarders: WONStatusToString, NetStats, IPAddr, AsyncSocket's core
//! and the socket ops' algorithms. Addresses cross as (address, port), both in network byte order (a
//! `PlatSockAddrIn`'s `mAddress` and `mPort`). Statuses are `long` (the C++ `WONStatus`).

use crate::ffi::EeWonByteSink;
use crate::ipaddr::{self, Addr};
use crate::netstats;
use crate::ops::{self, LengthStep, LockedSocket, OpTiming};
use crate::socket::{self, raw, raw_mut, Socket};
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

/// `WONStatusToString`: the status's name. A `WS_` name is a static string; another (a number, a DB proxy
/// game-specific error) is written into `buffer` (`size` bytes), which is returned.
///
/// # Safety
/// `buffer` has `size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_status_name(s: c_long, buffer: *mut c_char, size: usize) -> *const c_char {
    if let Some(n) = status::named(s as status::Status) {
        return n.as_ptr();
    }
    unsafe { put_c(status::unnamed(s as status::Status).as_bytes(), buffer, size) };
    buffer
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

// ---- AsyncSocket (the caller holds the socket's lock unless said otherwise)

/// The constructor: a closed socket of `kind` (0 stream, 1 datagram) into `core`.
///
/// # Safety
/// `core` points at an `EeWonSocket` (uninitialised is fine).
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_init(core: *mut Socket, kind: i32) {
    unsafe { core.write(Socket::new(kind)) }
}

/// The destructor's count (after the C++ closed the socket).
///
/// # Safety
/// `core` is a socket.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_destroyed(core: *mut Socket) {
    unsafe { (*core).destroyed() }
}

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

/// `GetLastSocketError`.
///
/// # Safety
/// `core` is a socket.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_last_error(core: *mut Socket) -> c_long {
    unsafe { (*core).last_error() as c_long }
}

/// `Open`'s handle (after the C++ closed a valid socket; the C++ runs the open hook on 0).
///
/// # Safety
/// `core` is a socket.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_open(core: *mut Socket) -> c_long {
    unsafe { (*core).open_handle() as c_long }
}

/// `Bind` on an open socket.
///
/// # Safety
/// `core` is a socket.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_bind(core: *mut Socket, address: u32, port: u16, reuse: bool) -> c_long {
    unsafe { (*core).bind(Addr { address, port }, reuse) as c_long }
}

/// The connect of `Fn2B0E60` on an open socket.
///
/// # Safety
/// `core` is a socket.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_connect(core: *mut Socket, address: u32, port: u16) -> c_long {
    unsafe { (*core).connect(Addr { address, port }) as c_long }
}

/// `Listen`.
///
/// # Safety
/// `core` is a socket.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_listen(core: *mut Socket, backlog: i32) -> c_long {
    unsafe { (*core).listen(backlog) as c_long }
}

/// `Accept`'s system part: the accepted handle and the peer, or the error.
///
/// # Safety
/// `core` is a socket; the out-parameters are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_accept(
    core: *mut Socket,
    handle: *mut isize,
    address: *mut u32,
    port: *mut u16,
) -> c_long {
    match unsafe { (*core).accept() } {
        Ok((h, a)) => {
            unsafe {
                *handle = h;
                *address = a.address;
                *port = a.port;
            }
            0
        }
        Err(s) => s as c_long,
    }
}

/// The accepted handle into the new socket's core (open, connected).
///
/// # Safety
/// `core` is a socket.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_adopt(core: *mut Socket, handle: isize) {
    unsafe { (*core).adopt(handle) }
}

/// A wait on a handle without the lock (`WaitForRead` 1, `WaitForWrite` 2).
#[no_mangle]
pub extern "C" fn ee_won_socket_wait(handle: isize, mask: u32, timeout: u32) {
    socket::wait(handle, mask, timeout)
}

/// `WaitForConnect`'s wait on a handle, without the lock.
#[no_mangle]
pub extern "C" fn ee_won_socket_wait_for_connect(handle: isize, timeout: u32) -> c_long {
    socket::wait_for_connect(handle, timeout) as c_long
}

/// `Send`; `*sent` (when not null) is set when the send did not fail.
///
/// # Safety
/// `core` is a socket; `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_send(
    core: *mut Socket,
    data: *const c_void,
    length: i32,
    sent: *mut i32,
) -> c_long {
    let mut n = -1;
    let s = unsafe { (*core).send(raw(data, length), &mut n) };
    if !sent.is_null() && n >= 0 {
        unsafe { *sent = n };
    }
    s as c_long
}

/// `Recv`; `*received` (when not null) is set when bytes came.
///
/// # Safety
/// `core` is a socket; `data` has room for `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_recv(
    core: *mut Socket,
    data: *mut c_void,
    length: i32,
    received: *mut i32,
) -> c_long {
    let mut n = -1;
    let s = unsafe { (*core).recv(raw_mut(data, length), &mut n) };
    if !received.is_null() && n >= 0 {
        unsafe { *received = n };
    }
    s as c_long
}

/// `SendTo` / `RecvFrom`'s test of the socket's kind.
///
/// # Safety
/// `core` is a socket.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_datagram_only(core: *const Socket) -> c_long {
    unsafe { (*core).datagram_only() as c_long }
}

/// `SendTo` on an open datagram socket.
///
/// # Safety
/// `core` is a socket; `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_send_to(
    core: *mut Socket,
    data: *const c_void,
    length: i32,
    address: u32,
    port: u16,
) -> c_long {
    unsafe { (*core).send_to(raw(data, length), Addr { address, port }) as c_long }
}

/// `RecvFrom`: on success the sender in `*address` / `*port` and `*from` true, `*received` (when not null) set.
///
/// # Safety
/// `core` is a socket; `data` has room for `length` bytes; the out-parameters are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_recv_from(
    core: *mut Socket,
    data: *mut c_void,
    length: i32,
    address: *mut u32,
    port: *mut u16,
    from: *mut bool,
    received: *mut i32,
) -> c_long {
    let mut peer = None;
    let mut n = 0;
    let s = unsafe { (*core).recv_from(raw_mut(data, length), &mut peer, &mut n) };
    unsafe {
        *from = put_addr(peer, address, port);
        if !received.is_null() && peer.is_some() {
            *received = n;
        }
    }
    s as c_long
}

/// `ClosePrv`.
///
/// # Safety
/// `core` is a socket.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_close(core: *mut Socket) {
    unsafe { (*core).close_handle() }
}

/// `Disconnect` of a datagram socket: false when dissolving the peer failed.
///
/// # Safety
/// `core` is a socket.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_dissolve_peer(core: *mut Socket) -> bool {
    unsafe { (*core).dissolve_peer() }
}

/// `GetLocalPort`.
///
/// # Safety
/// `core` is a socket.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_local_port(core: *const Socket) -> u16 {
    unsafe { (*core).local_port() }
}

/// `SetBroadcast`.
///
/// # Safety
/// `core` is a socket.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_set_broadcast(core: *mut Socket, on: bool) -> c_long {
    unsafe { (*core).set_broadcast(on) as c_long }
}

/// `SetNagling`.
///
/// # Safety
/// `core` is a socket.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_set_nagling(core: *mut Socket, on: bool) -> c_long {
    unsafe { (*core).set_nagling(on) as c_long }
}

// ---- the ops

/// WON's millisecond tick (`WONTickCount`).
#[no_mangle]
pub extern "C" fn ee_won_tick() -> u32 {
    ops::tick()
}

/// `AsyncOp::TimeLeft` now.
#[no_mangle]
pub extern "C" fn ee_won_op_time_left(timing: OpTiming) -> u32 {
    timing.time_left(ops::tick())
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

/// `SendBytesOp::Continue`: sends `data[*pos..length]`; `*pos` follows.
///
/// # Safety
/// `core` is the socket and `lock` its `PlatMutex`, both alive during the call; `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_send_all(
    core: *mut Socket,
    lock: *mut c_void,
    data: *const u8,
    length: usize,
    pos: *mut u32,
    timing: OpTiming,
) -> c_long {
    let s = LockedSocket { core, lock };
    unsafe { ops::send_all(&s, text(data, length), &mut *pos, timing) as c_long }
}

/// `RecvCRMsgOp::Continue`: each byte to `sink` until a CR-LF; `last` is the line so far's last byte (-1 for none).
///
/// # Safety
/// As `ee_won_op_send_all`; the sink is called during the call only.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_recv_line(
    core: *mut Socket,
    lock: *mut c_void,
    last: i32,
    timing: OpTiming,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> c_long {
    let s = LockedSocket { core, lock };
    let mut last = last;
    // SAFETY: the sink's contract
    ops::recv_line(&s, &mut last, timing, |b| unsafe { sink(context, &b, 1) }) as c_long
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn status_names_through_the_abi() {
        let mut buffer = [0 as c_char; 64];
        // SAFETY: 64 bytes
        let n = unsafe { std::ffi::CStr::from_ptr(ee_won_status_name(100001, buffer.as_mut_ptr(), 64)) };
        assert_eq!(n.to_str().unwrap(), "WS_TimedOut");
        let p = unsafe { ee_won_status_name(-2950, buffer.as_mut_ptr(), 64) };
        assert_eq!(p, buffer.as_ptr());
        assert_eq!(unsafe { std::ffi::CStr::from_ptr(p) }.to_str().unwrap(), "DBProxyServ_GameSpecificError-2950");
        // a short buffer is cut
        let p = unsafe { ee_won_status_name(123456, buffer.as_mut_ptr(), 4) };
        assert_eq!(unsafe { std::ffi::CStr::from_ptr(p) }.to_str().unwrap(), "123");
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
