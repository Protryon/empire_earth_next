//! `NEProtocol` whole (the original NEProtocol.cpp, LLE 0x1004440f..0x10044c71; PORT_LEARNINGS "LLE Network Engine,
//! round 7"): the game's one network protocol, TCP/IP, over its plug-in (`ee_net_tcp`, called through its C ABI),
//! with the registry of protocols and the traffic counters. Each member calls the plug-in as the original's entry
//! point through its table did, with what the plug-in's C++ shim did on the way: an event as its platform handle, a
//! connection's or server's handle in a slot the call makes (a close clears it, which nothing reads after), a new
//! connection or server the handle the plug-in wrote or NULL when it failed, the sent and received byte counts added
//! to the protocol's totals. Strings are bytes; the answers go to the caller's sinks (`EeTcpSink`).
//!
//! The Rust server and client call these directly (their `protocol_*` host calls, unless the host answers them: the
//! goldens' worlds); the C++ class `NEProtocol` keeps the members the game calls as forwarders.

use std::ffi::c_void;
use std::sync::atomic::{AtomicU32, Ordering::SeqCst};
use std::sync::Mutex;

/// The plug-in's sink for a text answer (`ee_net_tcp`'s `EeTcpSink`)
pub type EeNetPrSink = unsafe extern "C" fn(context: *mut c_void, bytes: *const u8, length: usize);

/// The plug-in's C ABI and `TSEvent::GetHandle` (ee_ts), imported
/// cbindgen:ignore
#[cfg(not(test))]
mod sys {
    use super::EeNetPrSink;
    use std::ffi::c_void;
    extern "C" {
        pub fn ee_tcp_activate() -> i32;
        pub fn ee_tcp_deactivate() -> i32;
        pub fn ee_tcp_is_valid() -> bool;
        pub fn ee_tcp_get_status() -> i32;
        pub fn ee_tcp_accept(server: *mut *mut c_void, connection: *mut *mut c_void) -> i32;
        pub fn ee_tcp_accept_for_xfer(server: *mut *mut c_void, connection: *mut *mut c_void) -> i32;
        pub fn ee_tcp_allow_new_connections(server: *mut *mut c_void, allow: bool);
        pub fn ee_tcp_close_connection(connection: *mut *mut c_void) -> i32;
        pub fn ee_tcp_close_server(server: *mut *mut c_void) -> i32;
        #[allow(clippy::too_many_arguments)]
        pub fn ee_tcp_connect(
            name: *const u8,
            name_length: usize,
            host: *const u8,
            host_length: usize,
            address: *const u8,
            address_length: usize,
            kind: i32,
            event: *mut c_void,
            connection: *mut *mut c_void,
            adapter: u32,
        ) -> i32;
        pub fn ee_tcp_connect_for_xfer(
            host: *mut *mut c_void,
            mode: i32,
            event: *mut c_void,
            connection: *mut *mut c_void,
        ) -> i32;
        pub fn ee_tcp_get_client_address(
            server: *mut *mut c_void,
            index: u32,
            sink: EeNetPrSink,
            context: *mut c_void,
        ) -> i32;
        pub fn ee_tcp_get_detected_game(
            index: u32,
            sink_name: EeNetPrSink,
            context_name: *mut c_void,
            sink_host: EeNetPrSink,
            context_host: *mut c_void,
            sink_address: EeNetPrSink,
            context_address: *mut c_void,
        ) -> i32;
        pub fn ee_tcp_get_detected_game_count() -> u32;
        pub fn ee_tcp_get_local_address(adapter: u32, sink: EeNetPrSink, context: *mut c_void);
        pub fn ee_tcp_enumerate_adapters(sink: EeNetPrSink, context: *mut c_void) -> u32;
        pub fn ee_tcp_is_alive(connection: *mut *mut c_void) -> bool;
        #[allow(clippy::too_many_arguments)]
        pub fn ee_tcp_publish(
            name: *const u8,
            name_length: usize,
            address: *const u8,
            address_length: usize,
            flags: u32,
            data_event: *mut c_void,
            listen_event: *mut c_void,
            accept_event: *mut c_void,
            server: *mut *mut c_void,
            flag55: bool,
            announce: bool,
            adapter: u32,
        ) -> i32;
        pub fn ee_tcp_receive(connection: *mut *mut c_void, size: u32, buffer: *mut u8, received: *mut u32) -> i32;
        pub fn ee_tcp_receive_from(
            server: *mut *mut c_void,
            size: u32,
            buffer: *mut u8,
            received: *mut u32,
            index: *mut u32,
        ) -> i32;
        pub fn ee_tcp_reset_client_connection(server: *mut *mut c_void, index: u32);
        pub fn ee_tcp_reset_xfer(server: *mut *mut c_void);
        pub fn ee_tcp_search_for_game(name: *const u8, length: usize, start: bool) -> i32;
        pub fn ee_tcp_send(connection: *mut *mut c_void, data: *mut u8, length: u32) -> i32;
        pub fn ee_tcp_send_single(host: *const u8, host_length: usize, port: u32, data: *const u8, length: u32) -> i32;
        pub fn ee_tcp_send_to(server: *mut *mut c_void, index: u32, data: *mut u8, length: u32) -> i32;
        pub fn ee_tcp_xfer_receive(size: u32, buffer: *mut u8, received: *mut u32) -> i32;
        pub fn ee_tcp_xfer_send_to(server: *mut *mut c_void, index: u32, data: *const u8, length: u32) -> i32;
        /// `TSEvent::GetHandle` (a `TSEvent*` is the Rust event, ee_ts)
        pub fn ee_ts_event_handle(event: *const c_void) -> *mut c_void;
    }
}
#[cfg(test)]
use crate::net::tests_protocol::stub::*;
#[cfg(not(test))]
use sys::*;

/// `kErrorOK`, `kErrorNotFound`, `kErrorBeyondEnd`
const ERROR_OK: i32 = 0;
const ERROR_NOT_FOUND: i32 = 3;
const ERROR_BEYOND_END: i32 = 0xa;

/// The protocol's name (`GetName`) and type (`GetProtocolType`)
const NAME: &[u8] = b"TCP/IP Protocol";
const PROTOCOL_TYPE: u32 = 1;

/// A protocol: the traffic counters (`mBytesSent`, `mBytesReceived`)
pub struct EeNetProtocol {
    sent: AtomicU32,
    received: AtomicU32,
}

/// The registered protocols (`NEProtocol::sProtocols`), as addresses
static PROTOCOLS: Mutex<Vec<usize>> = Mutex::new(Vec::new());

fn registry() -> std::sync::MutexGuard<'static, Vec<usize>> {
    PROTOCOLS.lock().unwrap_or_else(|e| e.into_inner())
}

unsafe fn me<'a>(p: *const EeNetProtocol) -> &'a EeNetProtocol {
    &*p
}

/// a new connection or server: the handle the plug-in wrote, or NULL when it failed
unsafe fn opened(result: i32, handle: *mut c_void, out: *mut *mut c_void) -> i32 {
    *out = if result == ERROR_OK { handle } else { std::ptr::null_mut() };
    result
}

/// `TSEvent::GetHandle`
unsafe fn handle(event: *const c_void) -> *mut c_void {
    ee_ts_event_handle(event)
}

/// The constructor: registers the protocol and activates its plug-in.
#[no_mangle]
pub extern "C" fn ee_net_pr_new() -> *mut EeNetProtocol {
    let p = Box::into_raw(Box::new(EeNetProtocol { sent: AtomicU32::new(0), received: AtomicU32::new(0) }));
    registry().push(p as usize);
    unsafe { ee_tcp_activate() };
    p
}

/// The destructor: deactivates the plug-in while it is valid and unregisters the protocol.
///
/// # Safety
/// `p` from `ee_net_pr_new`, used by nothing after.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_free(p: *mut EeNetProtocol) {
    if ee_tcp_is_valid() {
        ee_tcp_deactivate();
    }
    registry().retain(|&q| q != p as usize);
    drop(Box::from_raw(p));
}

/// `NEInitialize`'s protocols (after its shutdown hook): TCP/IP, freed again when its plug-in is not usable ->
/// kErrorOK, or kErrorNotFound for no protocol.
#[no_mangle]
pub extern "C" fn ee_net_pr_initialize() -> i32 {
    let p = ee_net_pr_new();
    if !unsafe { ee_tcp_is_valid() } {
        unsafe { ee_net_pr_free(p) };
    }
    if ee_net_pr_count() > 0 {
        ERROR_OK
    } else {
        ERROR_NOT_FOUND
    }
}

/// `NEShutdown`: every registered protocol freed.
#[no_mangle]
pub extern "C" fn ee_net_pr_shutdown() {
    // (the registry's lock released before each free, which takes it)
    loop {
        let first = registry().first().copied();
        match first {
            Some(p) => unsafe { ee_net_pr_free(p as *mut EeNetProtocol) },
            None => break,
        }
    }
}

/// `NEProtocol::GetCount()`
#[no_mangle]
pub extern "C" fn ee_net_pr_count() -> u32 {
    registry().len() as u32
}

/// `NEProtocol::GetProtocol(index, out)`: kErrorBeyondEnd (out unchanged) past the last.
///
/// # Safety
/// `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_get(index: u32, out: *mut *mut EeNetProtocol) -> i32 {
    match registry().get(index as usize) {
        Some(&p) => {
            *out = p as *mut EeNetProtocol;
            ERROR_OK
        }
        None => ERROR_BEYOND_END,
    }
}

/// `GetName()` (its bytes, `length` out) and `GetProtocolType()`
///
/// # Safety
/// `length` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_name(length: *mut usize) -> *const u8 {
    *length = NAME.len();
    NAME.as_ptr()
}
#[no_mangle]
pub extern "C" fn ee_net_pr_type() -> u32 {
    PROTOCOL_TYPE
}

/// `GetBytesSent()` (`sent`) or `GetBytesReceived()`
///
/// # Safety
/// `p` is a live protocol.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_bytes(p: *const EeNetProtocol, sent: bool) -> u32 {
    if sent {
        me(p).sent.load(SeqCst)
    } else {
        me(p).received.load(SeqCst)
    }
}

unsafe fn add(p: *const EeNetProtocol, sent: bool, n: u32) {
    let c = if sent { &me(p).sent } else { &me(p).received };
    c.fetch_add(n, SeqCst);
}

/// `Accept(server, out)` / `AcceptForXfer(server, out)` (`xfer`): the new connection's handle, NULL on failure.
///
/// # Safety
/// `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_accept(
    _p: *const EeNetProtocol,
    server: *mut c_void,
    xfer: bool,
    out: *mut *mut c_void,
) -> i32 {
    let mut slot = server;
    let mut new = std::ptr::null_mut();
    let r = if xfer { ee_tcp_accept_for_xfer(&mut slot, &mut new) } else { ee_tcp_accept(&mut slot, &mut new) };
    opened(r, new, out)
}

/// `AllowNewConnections(server, allow)`
///
/// # Safety
/// `server` is the plug-in's.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_allow_new_connections(_p: *const EeNetProtocol, server: *mut c_void, allow: bool) {
    let mut slot = server;
    ee_tcp_allow_new_connections(&mut slot, allow);
}

/// `Close(connection)` (`server` false) or `Close(server)`: the plug-in frees it when the close succeeds.
///
/// # Safety
/// `handle` is the plug-in's.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_close(_p: *const EeNetProtocol, handle: *mut c_void, server: bool) -> i32 {
    let mut slot = handle;
    if server {
        ee_tcp_close_server(&mut slot)
    } else {
        ee_tcp_close_connection(&mut slot)
    }
}

/// `Connect(name, host, address, port, event, out, timeout)`: the plug-in takes the timeout in its adapter argument,
/// as the original's entry point did.
///
/// # Safety
/// The strings are valid for their lengths; `event` is a `TSEvent*`; `out` is valid.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_net_pr_connect(
    _p: *const EeNetProtocol,
    name: *const u8,
    name_length: usize,
    host: *const u8,
    host_length: usize,
    address: *const u8,
    address_length: usize,
    port: u32,
    event: *const c_void,
    out: *mut *mut c_void,
    timeout: u32,
) -> i32 {
    let mut new = std::ptr::null_mut();
    let r = ee_tcp_connect(
        name,
        name_length,
        host,
        host_length,
        address,
        address_length,
        port as i32,
        handle(event),
        &mut new,
        timeout,
    );
    opened(r, new, out)
}

/// `ConnectForXfer(connection, port, event, out)`
///
/// # Safety
/// `event` is a `TSEvent*`; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_connect_for_xfer(
    _p: *const EeNetProtocol,
    connection: *mut c_void,
    port: u32,
    event: *const c_void,
    out: *mut *mut c_void,
) -> i32 {
    let mut slot = connection;
    let mut new = std::ptr::null_mut();
    let r = ee_tcp_connect_for_xfer(&mut slot, port as i32, handle(event), &mut new);
    opened(r, new, out)
}

/// `EnumerateNetworkAdapters(out)`: each adapter's description to the sink (the caller cleared its list) -> how many
///
/// # Safety
/// `sink` takes `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_enumerate_adapters(
    _p: *const EeNetProtocol,
    sink: EeNetPrSink,
    context: *mut c_void,
) -> u32 {
    ee_tcp_enumerate_adapters(sink, context)
}

/// `GetClientAddress(server, index, out)`
///
/// # Safety
/// `server` is the plug-in's; `sink` takes `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_client_address(
    _p: *const EeNetProtocol,
    server: *mut c_void,
    index: u32,
    sink: EeNetPrSink,
    context: *mut c_void,
) -> i32 {
    let mut slot = server;
    ee_tcp_get_client_address(&mut slot, index, sink, context)
}

/// `GetDetectedGame(index, name, host, address)`: the three answers to their sinks
///
/// # Safety
/// Each sink takes its context.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_net_pr_detected_game(
    _p: *const EeNetProtocol,
    index: u32,
    sink_name: EeNetPrSink,
    name: *mut c_void,
    sink_host: EeNetPrSink,
    host: *mut c_void,
    sink_address: EeNetPrSink,
    address: *mut c_void,
) -> i32 {
    ee_tcp_get_detected_game(index, sink_name, name, sink_host, host, sink_address, address)
}

/// `GetDetectedGameCount()`
///
/// # Safety
/// None beyond the plug-in's.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_detected_game_count(_p: *const EeNetProtocol) -> u32 {
    ee_tcp_get_detected_game_count()
}

/// `GetLocalAddress(adapter)` to the sink
///
/// # Safety
/// `sink` takes `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_local_address(
    _p: *const EeNetProtocol,
    adapter: u32,
    sink: EeNetPrSink,
    context: *mut c_void,
) {
    ee_tcp_get_local_address(adapter, sink, context)
}

/// `GetStatus(name, port)` (the plug-in reads neither)
///
/// # Safety
/// None beyond the plug-in's.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_status(_p: *const EeNetProtocol) -> i32 {
    ee_tcp_get_status()
}

/// `IsAlive(connection)`
///
/// # Safety
/// `connection` is the plug-in's.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_is_alive(_p: *const EeNetProtocol, connection: *mut c_void) -> bool {
    let mut slot = connection;
    ee_tcp_is_alive(&mut slot)
}

/// `Publish(name, address, port, accept event, receive event, xfer event, out, allow, announce, max connections)`
///
/// # Safety
/// The strings are valid for their lengths; the events are `TSEvent*`s; `out` is valid.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_net_pr_publish(
    _p: *const EeNetProtocol,
    name: *const u8,
    name_length: usize,
    address: *const u8,
    address_length: usize,
    port: u32,
    events: *const *const c_void,
    out: *mut *mut c_void,
    allow: bool,
    announce: bool,
    max_connections: u32,
) -> i32 {
    let e = std::slice::from_raw_parts(events, 3);
    let mut new = std::ptr::null_mut();
    let r = ee_tcp_publish(
        name,
        name_length,
        address,
        address_length,
        port,
        handle(e[0]),
        handle(e[1]),
        handle(e[2]),
        &mut new,
        allow,
        announce,
        max_connections,
    );
    opened(r, new, out)
}

/// `Receive(connection, size, buffer, received)`: the received count added to the total
///
/// # Safety
/// `buffer` holds `size` bytes; `received` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_receive(
    p: *const EeNetProtocol,
    connection: *mut c_void,
    size: u32,
    buffer: *mut u8,
    received: *mut u32,
) -> i32 {
    let mut slot = connection;
    let r = ee_tcp_receive(&mut slot, size, buffer, received);
    add(p, false, *received);
    r
}

/// `ReceiveFrom(server, size, buffer, received, index)`: the received count added to the total
///
/// # Safety
/// `buffer` holds `size` bytes; `received` and `index` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_receive_from(
    p: *const EeNetProtocol,
    server: *mut c_void,
    size: u32,
    buffer: *mut u8,
    received: *mut u32,
    index: *mut u32,
) -> i32 {
    let mut slot = server;
    let r = ee_tcp_receive_from(&mut slot, size, buffer, received, index);
    add(p, false, *received);
    r
}

/// `ResetClientConnection(server, index)` (`index` Some) or `ResetXfer(server)`
///
/// # Safety
/// `server` is the plug-in's.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_reset(_p: *const EeNetProtocol, server: *mut c_void, xfer: bool, index: u32) {
    let mut slot = server;
    if xfer {
        ee_tcp_reset_xfer(&mut slot)
    } else {
        ee_tcp_reset_client_connection(&mut slot, index)
    }
}

/// `SearchForGame(host, name, start)`: the plug-in takes the service name; the host is not used
///
/// # Safety
/// `name` is valid for its length.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_search_for_game(
    _p: *const EeNetProtocol,
    name: *const u8,
    name_length: usize,
    start: bool,
) -> i32 {
    ee_tcp_search_for_game(name, name_length, start)
}

/// `Send(connection, data, length)`: the plug-in enciphers the bytes in place; the length added to the sent total
///
/// # Safety
/// `data` holds `length` writable bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_send(
    p: *const EeNetProtocol,
    connection: *mut c_void,
    data: *mut u8,
    length: u32,
) -> i32 {
    let mut slot = connection;
    let r = ee_tcp_send(&mut slot, data, length);
    add(p, true, length);
    r
}

/// `SendSingle(host, port, data, length)`: one datagram; the length added to the sent total
///
/// # Safety
/// `host` and `data` are valid for their lengths.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_send_single(
    p: *const EeNetProtocol,
    host: *const u8,
    host_length: usize,
    port: u16,
    data: *const u8,
    length: u32,
) -> i32 {
    let r = ee_tcp_send_single(host, host_length, port as u32, data, length);
    add(p, true, length);
    r
}

/// `SendTo(server, index, data, length)` (`xfer` false, enciphered in place) or `XferSendTo`: the length added to
/// the sent total
///
/// # Safety
/// `data` holds `length` writable bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_send_to(
    p: *const EeNetProtocol,
    server: *mut c_void,
    xfer: bool,
    index: u32,
    data: *mut u8,
    length: u32,
) -> i32 {
    let mut slot = server;
    let r = if xfer {
        ee_tcp_xfer_send_to(&mut slot, index, data, length)
    } else {
        ee_tcp_send_to(&mut slot, index, data, length)
    };
    add(p, true, length);
    r
}

/// `XferReceive(size, buffer, received)`: the received count added to the total
///
/// # Safety
/// `buffer` holds `size` bytes; `received` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_net_pr_xfer_receive(
    p: *const EeNetProtocol,
    size: u32,
    buffer: *mut u8,
    received: *mut u32,
) -> i32 {
    let r = ee_tcp_xfer_receive(size, buffer, received);
    add(p, false, *received);
    r
}
