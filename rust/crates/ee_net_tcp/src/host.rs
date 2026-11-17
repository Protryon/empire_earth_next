//! What the plug-in needs from outside: the platform layer's sockets (`platform/PlatSocket.h`) and a little of the
//! engine (the error system, the clock). A trait so the plug-in's logic is tested with a fake network, no sockets.

use std::ffi::c_void;

/// `PlatSocket` (an `intptr_t`).
pub type Sock = isize;
pub const SOCK_INVALID: Sock = -1;

/// `PLAT_SOCK_EWOULDBLOCK`.
pub const E_WOULD_BLOCK: u32 = 10035;
pub const AF_INET: u16 = 2;
pub const INADDR_NONE: u32 = 0xffff_ffff;
pub const SOCK_STREAM: i32 = 1;
pub const SOCK_DGRAM: i32 = 2;
pub const EVENT_READ: u32 = 0x01;
pub const EVENT_ACCEPT: u32 = 0x08;

/// The platform's maximum backlog (`PLAT_SOCKET_MAX_BACKLOG`).
pub const MAX_BACKLOG: i32 = 0x7fff_ffff;

/// `PlatSockAddrIn`: `sockaddr_in`'s 16 bytes (port and address in network order).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct SockAddr {
    pub family: u16,
    pub port: u16,
    pub address: u32,
    pub zero: [u8; 8],
}

impl SockAddr {
    pub fn new(port_be: u16, address: u32) -> Self {
        SockAddr { family: AF_INET, port: port_be, address, zero: [0; 8] }
    }
}

/// An IPv4 network adapter (`PlatNetAdapter`, without its fixed-size text).
#[derive(Clone, Debug, PartialEq, Eq, Default)]
pub struct Adapter {
    pub description: Vec<u8>,
    pub address: u32,
    pub netmask: u32,
}

/// The network byte order of a port (`PlatHostToNet16`).
pub fn net16(host: u16) -> u16 {
    host.to_be()
}

/// The platform and engine calls of the plug-in. Argument order and results are those of `PlatSocket.h`.
pub trait Host: Send + Sync + 'static {
    fn startup(&self) -> bool;
    fn cleanup(&self) -> bool;
    fn last_error(&self) -> u32;
    fn open(&self, kind: i32) -> Sock;
    fn close(&self, socket: Sock) -> bool;
    fn bind(&self, socket: Sock, address: &SockAddr) -> bool;
    fn listen(&self, socket: Sock, backlog: i32) -> bool;
    fn accept(&self, socket: Sock, peer: Option<&mut SockAddr>) -> Sock;
    fn connect(&self, socket: Sock, address: &SockAddr) -> bool;
    fn get_name(&self, socket: Sock, address: &mut SockAddr) -> bool;
    fn set_non_blocking(&self, socket: Sock, non_blocking: bool) -> bool;
    fn set_broadcast(&self, socket: Sock, broadcast: bool) -> bool;
    fn send(&self, socket: Sock, data: &[u8]) -> i32;
    fn recv(&self, socket: Sock, data: &mut [u8]) -> i32;
    fn peek(&self, socket: Sock, data: &mut [u8]) -> i32;
    fn send_to(&self, socket: Sock, data: &[u8], to: &SockAddr) -> i32;
    fn recv_from(&self, socket: Sock, data: &mut [u8], from: &mut SockAddr) -> i32;
    fn select_event(&self, socket: Sock, event: *mut c_void, mask: u32) -> bool;
    fn get_host_name(&self, name: &mut [u8]) -> bool;
    fn resolve_ipv4(&self, name: &[u8], addresses: &mut [u32]) -> u32;
    fn reverse_lookup(&self, address: u32, name: &mut [u8]) -> bool;
    fn parse_ipv4(&self, text: &[u8]) -> u32;
    fn format_ipv4(&self, address: u32) -> Vec<u8>;
    fn adapters(&self, max: u32) -> Vec<Adapter>;
    fn sleep_ms(&self, milliseconds: u32);
    fn time_ms(&self) -> u32;
    fn record_native_error(&self, error: u32);
    fn native_error(&self) -> u32;
    fn port_offset(&self) -> i32;
}
