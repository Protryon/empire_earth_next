//! The C ABI of the TCP plug-in (called by the C++ shim, `TCP Protocol.cpp`), and the `Host` that reaches the platform
//! layer (`platform/PlatSocket.h`) and the engine (the shim's `ee_tcp_host_*`). Compiled out of `cargo test`.

use std::ffi::{c_char, c_int, c_void};
use std::slice;
use std::sync::{Arc, OnceLock};

use crate::host::*;
use crate::plugin::{Connection, Server, Tcp};

/// cbindgen:ignore
extern "C" {
    fn PlatSocketStartup() -> c_int;
    fn PlatSocketCleanup() -> c_int;
    fn PlatSocketLastError() -> u32;
    fn PlatSocketOpen(kind: c_int) -> Sock;
    fn PlatSocketClose(socket: Sock) -> c_int;
    fn PlatSocketBind(socket: Sock, address: *const SockAddr) -> c_int;
    fn PlatSocketListen(socket: Sock, backlog: c_int) -> c_int;
    fn PlatSocketAccept(socket: Sock, peer: *mut SockAddr) -> Sock;
    fn PlatSocketConnect(socket: Sock, address: *const SockAddr) -> c_int;
    fn PlatSocketGetName(socket: Sock, address: *mut SockAddr) -> c_int;
    fn PlatSocketSetNonBlocking(socket: Sock, non_blocking: c_int) -> c_int;
    fn PlatSocketSetBroadcast(socket: Sock, broadcast: c_int) -> c_int;
    fn PlatSocketSend(socket: Sock, data: *const c_void, length: u32) -> i32;
    fn PlatSocketRecv(socket: Sock, data: *mut c_void, length: u32) -> i32;
    fn PlatSocketPeek(socket: Sock, data: *mut c_void, length: u32) -> i32;
    fn PlatSocketSendTo(socket: Sock, data: *const c_void, length: u32, to: *const SockAddr) -> i32;
    fn PlatSocketRecvFrom(socket: Sock, data: *mut c_void, length: u32, from: *mut SockAddr) -> i32;
    fn PlatSocketSelectEvent(socket: Sock, event: *mut c_void, mask: u32) -> c_int;
    fn PlatSocketGetHostName(name: *mut c_char, size: u32) -> c_int;
    fn PlatSocketResolveIPv4(name: *const c_char, addresses: *mut u32, max: u32) -> u32;
    fn PlatSocketReverseLookup(address: u32, name: *mut c_char, size: u32) -> c_int;
    fn PlatSocketParseIPv4(text: *const c_char) -> u32;
    fn PlatSocketFormatIPv4(address: u32, text: *mut c_char);
    fn PlatSocketGetAdapters(adapters: *mut RawAdapter, max: u32) -> u32;
    fn PlatSleep(milliseconds: u32);
    // implemented by the C++ shim
    fn ee_tcp_host_record_native_error(error: u32);
    fn ee_tcp_host_native_error() -> u32;
    fn ee_tcp_host_time_ms() -> u32;
    fn ee_tcp_host_port_offset() -> i32;
}

/// cbindgen:ignore
#[repr(C)]
struct RawAdapter {
    description: [c_char; 132],
    address: u32,
    netmask: u32,
}

/// The real platform.
struct PlatHost;

fn with_c_string<R>(text: &[u8], f: impl FnOnce(*const c_char) -> R) -> R {
    let mut bytes = text.to_vec();
    bytes.push(0);
    f(bytes.as_ptr() as *const c_char)
}

impl Host for PlatHost {
    fn startup(&self) -> bool {
        unsafe { PlatSocketStartup() != 0 }
    }
    fn cleanup(&self) -> bool {
        unsafe { PlatSocketCleanup() != 0 }
    }
    fn last_error(&self) -> u32 {
        unsafe { PlatSocketLastError() }
    }
    fn open(&self, kind: i32) -> Sock {
        unsafe { PlatSocketOpen(kind) }
    }
    fn close(&self, socket: Sock) -> bool {
        unsafe { PlatSocketClose(socket) != 0 }
    }
    fn bind(&self, socket: Sock, address: &SockAddr) -> bool {
        unsafe { PlatSocketBind(socket, address) != 0 }
    }
    fn listen(&self, socket: Sock, backlog: i32) -> bool {
        unsafe { PlatSocketListen(socket, backlog) != 0 }
    }
    fn accept(&self, socket: Sock, peer: Option<&mut SockAddr>) -> Sock {
        let peer = peer.map_or(std::ptr::null_mut(), |p| p as *mut SockAddr);
        unsafe { PlatSocketAccept(socket, peer) }
    }
    fn connect(&self, socket: Sock, address: &SockAddr) -> bool {
        unsafe { PlatSocketConnect(socket, address) != 0 }
    }
    fn get_name(&self, socket: Sock, address: &mut SockAddr) -> bool {
        unsafe { PlatSocketGetName(socket, address) != 0 }
    }
    fn set_non_blocking(&self, socket: Sock, non_blocking: bool) -> bool {
        unsafe { PlatSocketSetNonBlocking(socket, c_int::from(non_blocking)) != 0 }
    }
    fn set_broadcast(&self, socket: Sock, broadcast: bool) -> bool {
        unsafe { PlatSocketSetBroadcast(socket, c_int::from(broadcast)) != 0 }
    }
    fn send(&self, socket: Sock, data: &[u8]) -> i32 {
        unsafe { PlatSocketSend(socket, data.as_ptr() as *const c_void, data.len() as u32) }
    }
    fn recv(&self, socket: Sock, data: &mut [u8]) -> i32 {
        unsafe { PlatSocketRecv(socket, data.as_mut_ptr() as *mut c_void, data.len() as u32) }
    }
    fn peek(&self, socket: Sock, data: &mut [u8]) -> i32 {
        unsafe { PlatSocketPeek(socket, data.as_mut_ptr() as *mut c_void, data.len() as u32) }
    }
    fn send_to(&self, socket: Sock, data: &[u8], to: &SockAddr) -> i32 {
        unsafe { PlatSocketSendTo(socket, data.as_ptr() as *const c_void, data.len() as u32, to) }
    }
    fn recv_from(&self, socket: Sock, data: &mut [u8], from: &mut SockAddr) -> i32 {
        unsafe { PlatSocketRecvFrom(socket, data.as_mut_ptr() as *mut c_void, data.len() as u32, from) }
    }
    fn select_event(&self, socket: Sock, event: *mut c_void, mask: u32) -> bool {
        unsafe { PlatSocketSelectEvent(socket, event, mask) != 0 }
    }
    fn get_host_name(&self, name: &mut [u8]) -> bool {
        unsafe { PlatSocketGetHostName(name.as_mut_ptr() as *mut c_char, name.len() as u32) != 0 }
    }
    fn resolve_ipv4(&self, name: &[u8], addresses: &mut [u32]) -> u32 {
        with_c_string(name, |p| unsafe { PlatSocketResolveIPv4(p, addresses.as_mut_ptr(), addresses.len() as u32) })
    }
    fn reverse_lookup(&self, address: u32, name: &mut [u8]) -> bool {
        unsafe { PlatSocketReverseLookup(address, name.as_mut_ptr() as *mut c_char, name.len() as u32) != 0 }
    }
    fn parse_ipv4(&self, text: &[u8]) -> u32 {
        with_c_string(text, |p| unsafe { PlatSocketParseIPv4(p) })
    }
    fn format_ipv4(&self, address: u32) -> Vec<u8> {
        let mut text = [0u8; 16];
        unsafe { PlatSocketFormatIPv4(address, text.as_mut_ptr() as *mut c_char) };
        let end = text.iter().position(|&b| b == 0).unwrap_or(16);
        text[..end].to_vec()
    }
    fn adapters(&self, max: u32) -> Vec<Adapter> {
        let mut raw: Vec<RawAdapter> = Vec::with_capacity(max as usize);
        let count = unsafe { PlatSocketGetAdapters(raw.as_mut_ptr(), max) }.min(max);
        // SAFETY: the platform initialised `count` elements
        unsafe { raw.set_len(count as usize) };
        raw.iter()
            .map(|a| {
                // SAFETY: `description` is 132 NUL-terminated bytes
                let end = a.description.iter().position(|&c| c == 0).unwrap_or(132);
                let description = a.description[..end].iter().map(|&c| c as u8).collect();
                Adapter { description, address: a.address, netmask: a.netmask }
            })
            .collect()
    }
    fn sleep_ms(&self, milliseconds: u32) {
        unsafe { PlatSleep(milliseconds) }
    }
    fn time_ms(&self) -> u32 {
        unsafe { ee_tcp_host_time_ms() }
    }
    fn record_native_error(&self, error: u32) {
        unsafe { ee_tcp_host_record_native_error(error) }
    }
    fn native_error(&self) -> u32 {
        unsafe { ee_tcp_host_native_error() }
    }
    fn port_offset(&self) -> i32 {
        unsafe { ee_tcp_host_port_offset() }
    }
}

static TCP: OnceLock<Arc<Tcp<PlatHost>>> = OnceLock::new();

fn tcp() -> &'static Arc<Tcp<PlatHost>> {
    TCP.get_or_init(|| Arc::new(Tcp::new(PlatHost)))
}

/// Receives text output (the shim copies it into a `UString`/`UWideString`). Valid only during the call.
pub type EeTcpSink = unsafe extern "C" fn(context: *mut c_void, bytes: *const u8, length: usize);

unsafe fn text<'a>(data: *const u8, length: usize) -> &'a [u8] {
    if length == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(data, length) }
    }
}

unsafe fn give(sink: EeTcpSink, context: *mut c_void, data: &[u8]) {
    unsafe { sink(context, data.as_ptr(), data.len()) }
}

/// `NPIMActivate`.
///
/// # Safety
/// The platform layer is initialised.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_activate() -> i32 {
    tcp().activate()
}

/// `NPIMDeactivate`.
///
/// # Safety
/// As `ee_tcp_activate`.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_deactivate() -> i32 {
    tcp().deactivate()
}

/// Whether the plug-in started (`PIMIsValid`).
#[no_mangle]
pub extern "C" fn ee_tcp_is_valid() -> bool {
    tcp().is_valid()
}

/// `NPIMGetStatus`.
#[no_mangle]
pub extern "C" fn ee_tcp_get_status() -> i32 {
    tcp().get_status()
}

/// `NPIMAccept`.
///
/// # Safety
/// `server` and `connection` are valid handle slots.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_accept(server: *mut *mut c_void, connection: *mut *mut c_void) -> i32 {
    let server = unsafe { &*(*server as *const Server) };
    let mut out: *mut Connection = std::ptr::null_mut();
    let code = tcp().accept(server, &mut out);
    if !out.is_null() {
        unsafe { *connection = out as *mut c_void };
    }
    code
}

/// `NPIMAcceptForXfer`.
///
/// # Safety
/// As `ee_tcp_accept`.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_accept_for_xfer(server: *mut *mut c_void, connection: *mut *mut c_void) -> i32 {
    let server = unsafe { &mut *(*server as *mut Server) };
    let mut out: *mut Connection = std::ptr::null_mut();
    let code = tcp().accept_for_xfer(server, &mut out);
    if !out.is_null() {
        unsafe { *connection = out as *mut c_void };
    }
    code
}

/// `NPIMAllowNewConnections`.
///
/// # Safety
/// `server` is a valid handle slot.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_allow_new_connections(server: *mut *mut c_void, allow: bool) {
    tcp().allow_new_connections(unsafe { &mut *(*server as *mut Server) }, allow);
}

/// `NPIMCloseConnection`.
///
/// # Safety
/// `connection` is a valid handle slot.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_close_connection(connection: *mut *mut c_void) -> i32 {
    let mut raw = unsafe { *connection } as *mut Connection;
    let code = tcp().close_connection(&mut raw);
    unsafe { *connection = raw as *mut c_void };
    code
}

/// `NPIMCloseServer`.
///
/// # Safety
/// `server` is a valid handle slot.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_close_server(server: *mut *mut c_void) -> i32 {
    let mut raw = unsafe { *server } as *mut Server;
    let code = tcp().close_server(&mut raw);
    unsafe { *server = raw as *mut c_void };
    code
}

/// `NPIMConnect`. Strings are bytes (the shim strips `UString`s); `event` a `PlatEvent*`.
///
/// # Safety
/// The strings are valid for their lengths; `connection` is a valid slot.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_tcp_connect(
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
) -> i32 {
    let mut out: *mut Connection = std::ptr::null_mut();
    let code = tcp().connect(
        unsafe { text(name, name_length) },
        unsafe { text(host, host_length) },
        unsafe { text(address, address_length) },
        kind,
        event,
        adapter,
        &mut out,
    );
    if !out.is_null() {
        unsafe { *connection = out as *mut c_void };
    }
    code
}

/// `NPIMConnectForXfer`. `host` is the client's game connection slot.
///
/// # Safety
/// `host` points to a valid slot (its contents may be NULL); `connection` is a valid slot.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_connect_for_xfer(
    host: *mut *mut c_void,
    mode: i32,
    event: *mut c_void,
    connection: *mut *mut c_void,
) -> i32 {
    let host = unsafe { (*host as *const Connection).as_ref() };
    let mut out: *mut Connection = std::ptr::null_mut();
    let code = tcp().connect_for_xfer(host, mode, event, &mut out);
    if !out.is_null() {
        unsafe { *connection = out as *mut c_void };
    }
    code
}

/// `NPIMGetClientAddress`.
///
/// # Safety
/// `server` is a valid slot; `sink` is called once with the text.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_get_client_address(
    server: *mut *mut c_void,
    index: u32,
    sink: EeTcpSink,
    context: *mut c_void,
) -> i32 {
    let (code, text) = tcp().get_client_address(unsafe { &*(*server as *const Server) }, index);
    unsafe { give(sink, context, &text) };
    code
}

/// `NPIMGetDetectedGame`. Returns 10 for an out-of-range index.
///
/// # Safety
/// Each sink is called once.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_get_detected_game(
    index: u32,
    sink_name: EeTcpSink,
    context_name: *mut c_void,
    sink_host: EeTcpSink,
    context_host: *mut c_void,
    sink_address: EeTcpSink,
    context_address: *mut c_void,
) -> i32 {
    match tcp().detected_game(index) {
        Some(game) => {
            unsafe {
                give(sink_name, context_name, &game.name);
                give(sink_host, context_host, &game.host);
                give(sink_address, context_address, &game.address);
            }
            0
        }
        None => 10,
    }
}

/// `NPIMGetDetectedGameCount`.
#[no_mangle]
pub extern "C" fn ee_tcp_get_detected_game_count() -> u32 {
    tcp().detected_game_count()
}

/// `NPIMGetLocalAddress`: gives the address text.
///
/// # Safety
/// `sink` is called once.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_get_local_address(adapter: u32, sink: EeTcpSink, context: *mut c_void) {
    let text = tcp().get_local_address(adapter);
    unsafe { give(sink, context, &text) };
}

/// `NPIMEnumerateNetworkAdapters`: `sink` receives each description in turn; returns how many.
///
/// # Safety
/// `sink` is called once per adapter.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_enumerate_adapters(sink: EeTcpSink, context: *mut c_void) -> u32 {
    let adapters = tcp().enumerate_network_adapters();
    for description in &adapters {
        unsafe { give(sink, context, description) };
    }
    adapters.len() as u32
}

/// `NPIMIsAlive`.
///
/// # Safety
/// `connection` is a valid slot holding a live connection.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_is_alive(connection: *mut *mut c_void) -> bool {
    tcp().is_alive(unsafe { &*(*connection as *const Connection) })
}

/// `NPIMPublish`. `flags`: bit 0 listening (stream), bit 1 datagram; events are `PlatEvent*`.
///
/// # Safety
/// The strings are valid for their lengths; `server` is a valid slot.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_tcp_publish(
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
) -> i32 {
    let mut out: *mut Server = std::ptr::null_mut();
    let code = tcp().publish(
        unsafe { text(name, name_length) },
        unsafe { text(address, address_length) },
        flags & 1 != 0,
        flags & 2 != 0,
        data_event,
        listen_event,
        accept_event,
        &mut out,
        flag55,
        announce,
        adapter,
    );
    if !out.is_null() {
        unsafe { *server = out as *mut c_void };
    }
    code
}

/// `NPIMReceive`.
///
/// # Safety
/// `connection` is a valid slot, or NULL-holding; `buffer` is writable for `size`; `received` for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_receive(
    connection: *mut *mut c_void,
    size: u32,
    buffer: *mut u8,
    received: *mut u32,
) -> i32 {
    let raw = unsafe { *connection } as *const Connection;
    if raw.is_null() {
        return crate::plugin::WOULD_BLOCK;
    }
    let buffer = unsafe { slice::from_raw_parts_mut(buffer, size as usize) };
    tcp().receive(unsafe { &*raw }, buffer, unsafe { &mut *received })
}

/// `NPIMReceiveFrom`.
///
/// # Safety
/// `server` is a valid slot; `buffer` is writable for `size`; the out pointers for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_receive_from(
    server: *mut *mut c_void,
    size: u32,
    buffer: *mut u8,
    received: *mut u32,
    index: *mut u32,
) -> i32 {
    let buffer = unsafe { slice::from_raw_parts_mut(buffer, size as usize) };
    tcp().receive_from(unsafe { &mut *(*server as *mut Server) }, buffer, unsafe { &mut *received }, unsafe {
        &mut *index
    })
}

/// `NPIMResetClientConnection`.
///
/// # Safety
/// `server` is a valid slot.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_reset_client_connection(server: *mut *mut c_void, index: u32) {
    tcp().reset_client_connection(unsafe { &mut *(*server as *mut Server) }, index);
}

/// `NPIMResetXfer`.
///
/// # Safety
/// `server` is a valid slot.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_reset_xfer(server: *mut *mut c_void) {
    tcp().reset_xfer(unsafe { &mut *(*server as *mut Server) });
}

/// `NPIMSearchForGame`.
///
/// # Safety
/// `name` is valid for `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_search_for_game(name: *const u8, length: usize, start: bool) -> i32 {
    tcp().search_for_game(unsafe { text(name, length) }, start)
}

/// `NPIMSend`.
///
/// # Safety
/// `connection` is a valid slot; `data` is writable for `length`.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_send(connection: *mut *mut c_void, data: *mut u8, length: u32) -> i32 {
    let connection = unsafe { &*(*connection as *const Connection) };
    let data = unsafe { slice::from_raw_parts_mut(data, length as usize) };
    tcp().send(connection, data)
}

/// `NPIMSendSingle`.
///
/// # Safety
/// `host` is valid for `host_length`; `data` for `length`.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_send_single(
    host: *const u8,
    host_length: usize,
    port: u32,
    data: *const u8,
    length: u32,
) -> i32 {
    tcp().send_single(unsafe { text(host, host_length) }, port as u16, unsafe { text(data, length as usize) })
}

/// `NPIMSendTo`.
///
/// # Safety
/// `server` is a valid slot; `data` is writable for `length`.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_send_to(server: *mut *mut c_void, index: u32, data: *mut u8, length: u32) -> i32 {
    let server = unsafe { &*(*server as *const Server) };
    let data = unsafe { slice::from_raw_parts_mut(data, length as usize) };
    tcp().send_to(server, index, data)
}

/// `NPIMXferReceive`.
///
/// # Safety
/// `buffer` is writable for `size`; `received` for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_xfer_receive(size: u32, buffer: *mut u8, received: *mut u32) -> i32 {
    let buffer = unsafe { slice::from_raw_parts_mut(buffer, size as usize) };
    tcp().xfer_receive(buffer, unsafe { &mut *received })
}

/// `NPIMXferSendTo`.
///
/// # Safety
/// `server` is a valid slot; `data` for `length`.
#[no_mangle]
pub unsafe extern "C" fn ee_tcp_xfer_send_to(
    server: *mut *mut c_void,
    index: u32,
    data: *const u8,
    length: u32,
) -> i32 {
    let server = unsafe { &*(*server as *const Server) };
    tcp().xfer_send_to(server, index, unsafe { text(data, length as usize) })
}
