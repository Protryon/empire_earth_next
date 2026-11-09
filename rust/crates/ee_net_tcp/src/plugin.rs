//! The TCP/IP plug-in's logic, generic over `Host`: the result codes and every branch are the former C++
//! (`TCP Protocol.cpp`), including its error paths (a would-block is 7, a closed peer 9, ...).
//!
//! Changes from the C++, kept small: `strcpy` into the fixed packet fields is bounded; the 1-second query loop's
//! sockets are closed on its failure paths too; a datagram's cipher loop has no 16-bit counter limit; the searcher
//! thread is a Rust thread woken by a channel instead of TSEvents, and it no longer sets the FPU mode.

use std::ffi::c_void;
use std::sync::atomic::{AtomicBool, AtomicIsize, AtomicU32, Ordering};
use std::sync::mpsc::{self, RecvTimeoutError, Sender};
use std::sync::{Arc, Mutex};
use std::thread::JoinHandle;
use std::time::Duration;

use ee_random::URandom;

use crate::cipher::Cipher;
use crate::host::*;

/// Result codes of the NPIM entry points.
pub const OK: i32 = 0;
pub const NATIVE_ERROR: i32 = 1;
pub const BAD_INDEX: i32 = 3;
pub const WOULD_BLOCK: i32 = 7;
pub const OUT_OF_MEMORY: i32 = 8;
pub const UNAVAILABLE: i32 = 9;
const INDEX_RANGE: i32 = 10;
const BAD_XFER: i32 = 13;

const GAME_PORT: u16 = 33334;
const XFER_PORT: u16 = 33335;
const MAX_HOST_ADDRESSES: usize = 64;
const MAX_ADAPTERS: u32 = 64;
const QUERY_PACKET_SIZE: usize = 0x43;
const ANNOUNCEMENT_PACKET_SIZE: usize = 0x88;
const QUERY_PACKET_TYPE: u8 = 0x15;
const ANNOUNCEMENT_PACKET_TYPE: u8 = 0x16;
const NAME_FIELD: usize = 0x41;
const ADDRESS_FIELD: usize = 0x45;

/// A connection handle (`TCPConnection`): the engine's handle slot holds a pointer to one.
#[repr(C)]
pub struct Connection {
    pub socket: Sock,
    /// The peer address (read back by `connect_for_xfer`, whose "host" is the client's game connection).
    pub address: SockAddr,
    /// 1 = stream.
    pub state: i32,
}

/// A hosted server (`TCPServer`).
pub struct Server {
    pub socket: Sock,
    pub xfer_socket: Sock,
    pub clients: Vec<SockAddr>,
    pub connections: Vec<*mut Connection>,
    pub listening: bool,
    pub xfer: bool,
    pub name: Vec<u8>,
    pub address: Vec<u8>,
    pub port: u32,
    pub allow_new_connections: bool,
    pub flag55: bool,
    pub announce: bool,
}

/// A game found by the LAN search.
#[derive(Clone, Default)]
pub struct DetectedGame {
    pub stream_address: SockAddr,
    pub datagram_address: SockAddr,
    pub name: Vec<u8>,
    pub host: Vec<u8>,
    pub address: Vec<u8>,
}

struct Searcher {
    exit: Sender<()>,
    thread: JoinHandle<()>,
}

/// The plug-in. All state the engine sees is here or behind the raw handles.
pub struct Tcp<H: Host> {
    pub host: H,
    detected: Mutex<Vec<DetectedGame>>,
    socket_lock: Mutex<()>,
    xfer_lock: Mutex<()>,
    cipher: Mutex<Cipher>,
    random: Mutex<Option<URandom>>,
    valid: AtomicBool,
    xfer_socket: AtomicIsize,
    listen_socket: AtomicIsize,
    adapter_index: AtomicU32,
    searcher: Mutex<Option<Searcher>>,
}

fn c_field(field: &[u8]) -> &[u8] {
    match field.iter().position(|&b| b == 0) {
        Some(end) => &field[..end],
        None => field,
    }
}

/// `strcpy` into a fixed field: the text, cut to fit, and its terminator.
fn copy_c_string(field: &mut [u8], text: &[u8]) {
    let count = text.len().min(field.len() - 1);
    field[..count].copy_from_slice(&text[..count]);
    field[count] = 0;
}

impl<H: Host> Tcp<H> {
    pub fn new(host: H) -> Self {
        Tcp {
            host,
            detected: Mutex::new(Vec::new()),
            socket_lock: Mutex::new(()),
            xfer_lock: Mutex::new(()),
            cipher: Mutex::new(Cipher::new()),
            random: Mutex::new(None),
            valid: AtomicBool::new(false),
            xfer_socket: AtomicIsize::new(SOCK_INVALID),
            listen_socket: AtomicIsize::new(SOCK_INVALID),
            adapter_index: AtomicU32::new(0),
            searcher: Mutex::new(None),
        }
    }

    pub fn is_valid(&self) -> bool {
        self.valid.load(Ordering::Relaxed)
    }

    fn port(&self, port: u16) -> u16 {
        net16((i32::from(port) + self.host.port_offset()) as u16)
    }

    fn record_error(&self) {
        let error = self.host.last_error();
        self.host.record_native_error(error);
        self.host.native_error();
    }

    fn native_error_code(&self) -> i32 {
        self.record_error();
        NATIVE_ERROR
    }

    fn would_block_or_error(&self, error: u32) -> i32 {
        self.host.record_native_error(error);
        if error == E_WOULD_BLOCK {
            WOULD_BLOCK
        } else {
            NATIVE_ERROR
        }
    }

    fn new_key(&self) -> u8 {
        let mut random = self.random.lock().expect("no panics while locked");
        let Some(random) = random.as_mut() else {
            return 1;
        };
        (random.generate() * 254.0 + 1.0) as i32 as u8
    }

    fn encipher(&self, data: &mut [u8], key: u8) {
        self.cipher.lock().expect("no panics while locked").apply(data, key);
    }

    /// `NPIMActivate`.
    pub fn activate(&self) -> i32 {
        if !self.host.startup() {
            self.host.record_native_error(self.host.last_error());
            return NATIVE_ERROR;
        }
        let probe = self.host.open(SOCK_STREAM);
        if probe == SOCK_INVALID {
            return UNAVAILABLE;
        }
        self.host.close(probe);
        self.valid.store(true, Ordering::Relaxed);
        *self.random.lock().expect("no panics while locked") = Some(URandom::new(self.host.time_ms()));
        self.get_status();
        OK
    }

    /// `NPIMDeactivate`.
    pub fn deactivate(&self) -> i32 {
        if !self.host.cleanup() {
            self.host.record_native_error(self.host.last_error());
            return NATIVE_ERROR;
        }
        self.cipher.lock().expect("no panics while locked").clear();
        *self.random.lock().expect("no panics while locked") = None;
        self.valid.store(false, Ordering::Relaxed);
        OK
    }

    /// `NPIMGetStatus`: (re)fills the cipher's key buffer.
    pub fn get_status(&self) -> i32 {
        self.cipher.lock().expect("no panics while locked").fill_key_buffer();
        OK
    }

    pub fn close_connection(&self, slot: &mut *mut Connection) -> i32 {
        let connection = *slot;
        let mut result;
        // SAFETY: the slot holds a live connection from this plug-in, or NULL
        let closed = !connection.is_null() && self.host.close(unsafe { (*connection).socket });
        if closed {
            // SAFETY: as above; the slot is cleared, the engine no longer uses it
            drop(unsafe { Box::from_raw(connection) });
            *slot = std::ptr::null_mut();
            result = OK;
        } else {
            result = self.native_error_code();
        }
        if !self.host.close(self.listen_socket.load(Ordering::Relaxed)) {
            result = self.native_error_code();
        }
        result
    }

    pub fn close_server(&self, slot: &mut *mut Server) -> i32 {
        // SAFETY: the slot holds a live server from this plug-in
        let server = unsafe { &mut **slot };
        let mut result = OK;
        if server.listening && !self.host.close(server.socket) {
            result = self.native_error_code();
        }
        if server.xfer && !self.host.close(server.xfer_socket) {
            result = self.native_error_code();
        }
        if !self.host.close(self.listen_socket.load(Ordering::Relaxed)) {
            result = self.native_error_code();
        }
        if result == OK {
            // SAFETY: a server is freed exactly once, here or through `publish`'s failure path
            let server = unsafe { Box::from_raw(*slot) };
            // ~TCPServer: freeing the client addresses (owned) and the xfer connections
            for &connection in &server.connections {
                if !connection.is_null() {
                    // SAFETY: slots with an accepted connection the engine has not closed through this path
                    drop(unsafe { Box::from_raw(connection) });
                }
            }
            *slot = std::ptr::null_mut();
        }
        result
    }

    pub fn accept(&self, server: &Server, out: &mut *mut Connection) -> i32 {
        if !server.listening {
            return UNAVAILABLE;
        }
        let socket = self.host.accept(server.socket, None);
        if socket != SOCK_INVALID {
            let connection = Connection { socket, address: SockAddr::default(), state: 1 };
            *out = Box::into_raw(Box::new(connection));
            return OK;
        }
        self.would_block_or_error(self.host.last_error())
    }

    pub fn accept_for_xfer(&self, server: &mut Server, out: &mut *mut Connection) -> i32 {
        let _lock = self.xfer_lock.lock().expect("no panics while locked");
        let mut peer = SockAddr::default();
        let socket = self.host.accept(self.listen_socket.load(Ordering::Relaxed), Some(&mut peer));
        if socket == SOCK_INVALID {
            return self.would_block_or_error(self.host.last_error());
        }
        let slot =
            server.clients.iter().position(|c| peer.family == c.family && c.port != 0 && peer.address == c.address);
        let Some(i) = slot else {
            self.host.close(socket);
            return BAD_XFER;
        };
        let connection = Box::into_raw(Box::new(Connection { socket, address: SockAddr::default(), state: 1 }));
        *out = connection;
        server.connections[i] = connection;
        OK
    }

    pub fn allow_new_connections(&self, server: &mut Server, allow: bool) {
        server.allow_new_connections = allow;
    }

    /// `NPIMConnect`. `kind` 1 = stream, otherwise datagram; 0 is `UNAVAILABLE`.
    #[allow(clippy::too_many_arguments)]
    pub fn connect(
        &self,
        name: &[u8],
        host_name: &[u8],
        address: &[u8],
        kind: i32,
        event: *mut c_void,
        adapter: u32,
        out: &mut *mut Connection,
    ) -> i32 {
        if kind == 0 {
            return UNAVAILABLE;
        }
        self.adapter_index.store(self.find_adapter_address_index(adapter), Ordering::Relaxed);
        let mut socket = SOCK_INVALID;
        let mut target = SockAddr::default();
        let result = self.resolve_address(name, host_name, address, kind, &mut socket, &mut target);
        if result != OK {
            return result;
        }
        if kind == 1 && !self.host.connect(socket, &target) {
            let code = self.native_error_code();
            self.host.close(socket);
            return code;
        }
        let local = SockAddr::new(0, 0);
        let _lock = self.socket_lock.lock().expect("no panics while locked");
        if self.host.bind(socket, &local) {
            self.host.select_event(socket, event, EVENT_READ);
            *out = Box::into_raw(Box::new(Connection { socket, address: target, state: kind }));
            OK
        } else {
            self.host.record_native_error(self.host.last_error());
            self.host.close(socket);
            NATIVE_ERROR
        }
    }

    /// `NPIMConnectForXfer`: `host` is the client's game connection.
    pub fn connect_for_xfer(
        &self,
        host: Option<&Connection>,
        mode: i32,
        event: *mut c_void,
        out: &mut *mut Connection,
    ) -> i32 {
        let Some(host) = host.filter(|_| mode == 1) else {
            return BAD_XFER;
        };
        let socket = self.host.open(SOCK_STREAM);
        self.xfer_socket.store(socket, Ordering::Relaxed);
        if socket != SOCK_INVALID {
            let address = SockAddr::new(self.port(XFER_PORT), host.address.address);
            if self.host.connect(socket, &address) {
                *out = Box::into_raw(Box::new(Connection { socket, address, state: 1 }));
                self.host.select_event(socket, event, EVENT_READ);
                return OK;
            }
        }
        self.native_error_code()
    }

    fn resolve_address(
        &self,
        name: &[u8],
        host_name: &[u8],
        address: &[u8],
        kind: i32,
        out_socket: &mut Sock,
        out_address: &mut SockAddr,
    ) -> i32 {
        let detected = self.detected.lock().expect("no panics while locked");
        let socket = self.host.open(if kind == 1 { SOCK_STREAM } else { SOCK_DGRAM });
        if socket == SOCK_INVALID {
            return self.native_error_code();
        }
        if !detected.is_empty() {
            let found = detected.iter().position(|g| g.name == name && g.host == host_name && g.address == address);
            match found {
                Some(i) => {
                    *out_address = if kind == 1 { detected[i].stream_address } else { detected[i].datagram_address };
                    out_address.port = self.port(GAME_PORT);
                    *out_socket = socket;
                    OK
                }
                None => {
                    self.host.close(socket);
                    BAD_INDEX
                }
            }
        } else {
            // gethostbyname, else (a dotted address) inet_addr + gethostbyaddr
            let mut addresses = [0u32; 1];
            let mut found = self.host.resolve_ipv4(name, &mut addresses) != 0;
            if !found {
                addresses[0] = self.host.parse_ipv4(name);
                let mut scratch = [0u8; 0x400];
                found = addresses[0] != INADDR_NONE && self.host.reverse_lookup(addresses[0], &mut scratch);
            }
            if found {
                *out_socket = socket;
                *out_address = SockAddr::new(self.port(GAME_PORT), addresses[0]);
                OK
            } else {
                self.host.close(socket); // the C++ leaked it here
                UNAVAILABLE
            }
        }
    }

    /// The index of an adapter's IP in the local host's address list (0 when not found).
    fn find_adapter_address_index(&self, adapter: u32) -> u32 {
        if adapter == 0 {
            return 0;
        }
        let adapters = self.host.adapters(MAX_ADAPTERS);
        if adapters.is_empty() {
            return 0;
        }
        let Some(chosen) = adapters.get(adapter as usize - 1) else {
            return 0;
        };
        if chosen.address == 0 {
            return 0;
        }
        let mut name = [0u8; 0x400];
        if !self.host.get_host_name(&mut name) {
            return 0;
        }
        let mut addresses = [0u32; MAX_HOST_ADDRESSES];
        let count = self.host.resolve_ipv4(c_field(&name), &mut addresses) as usize;
        addresses[..count.min(MAX_HOST_ADDRESSES)].iter().position(|&a| a == chosen.address).map_or(0, |i| i as u32)
    }

    pub fn get_client_address(&self, server: &Server, index: u32) -> (i32, Vec<u8>) {
        match server.clients.get(index as usize) {
            Some(client) => (OK, self.host.format_ipv4(client.address)),
            None => (BAD_INDEX, Vec::new()),
        }
    }

    pub fn detected_game(&self, index: u32) -> Option<DetectedGame> {
        self.detected.lock().expect("no panics while locked").get(index as usize).cloned()
    }

    pub fn detected_game_count(&self) -> u32 {
        self.detected.lock().expect("no panics while locked").len() as u32
    }

    /// Up to 10 adapter descriptions, in the system's order.
    pub fn enumerate_network_adapters(&self) -> Vec<Vec<u8>> {
        self.host.adapters(10).into_iter().take(10).map(|a| a.description).collect()
    }

    /// `NPIMGetLocalAddress`: the chosen adapter's address as text, else the address a UDP socket gets.
    pub fn get_local_address(&self, adapter: u32) -> Vec<u8> {
        let index = self.find_adapter_address_index(adapter);
        self.adapter_index.store(index, Ordering::Relaxed);
        let mut local = SockAddr::default();
        let mut name = [0u8; 0x400];
        if self.host.get_host_name(&mut name) {
            let mut addresses = [0u32; MAX_HOST_ADDRESSES];
            let count = self.host.resolve_ipv4(c_field(&name), &mut addresses);
            if count != 0 && index < count {
                local.address = addresses[index as usize];
            }
        }
        if local.address == 0 {
            let socket = self.host.open(SOCK_DGRAM);
            if socket != SOCK_INVALID {
                let remote = SockAddr::new(net16(7), self.host.parse_ipv4(b"128.127.126.125"));
                if self.host.connect(socket, &remote) {
                    self.host.get_name(socket, &mut local);
                }
                self.host.close(socket);
            }
        }
        self.host.format_ipv4(local.address)
    }

    pub fn is_alive(&self, connection: &Connection) -> bool {
        if connection.state != 1 {
            return true;
        }
        let mut byte = [0u8; 1];
        match self.host.peek(connection.socket, &mut byte) {
            1 => true,
            0 => false,
            _ => self.host.last_error() == E_WOULD_BLOCK,
        }
    }

    /// Creates a non-blocking stream or datagram socket bound to `*port` (restarting the plug-in first, as the C++
    /// `CreateSocket` does); `(result, socket, bound port)`.
    fn create_socket(&self, kind: i32, port: u32) -> (i32, Sock, u32) {
        self.deactivate();
        self.activate();
        let socket = self.host.open(if kind == 1 { SOCK_STREAM } else { SOCK_DGRAM });
        if socket == SOCK_INVALID {
            return (self.native_error_code(), SOCK_INVALID, port);
        }
        if !self.host.set_non_blocking(socket, true) {
            let code = self.native_error_code();
            self.host.close(socket);
            return (code, SOCK_INVALID, port);
        }
        let mut address = SockAddr::new(port as u16, 0);
        if !self.host.bind(socket, &address) || !self.host.get_name(socket, &mut address) {
            let code = self.native_error_code();
            self.host.close(socket);
            return (code, SOCK_INVALID, port);
        }
        let bound = u32::from(address.port);
        if kind != 1 || self.host.listen(socket, MAX_BACKLOG) {
            (OK, socket, bound)
        } else {
            let code = self.native_error_code();
            self.host.close(socket);
            (code, SOCK_INVALID, bound)
        }
    }

    /// `NPIMPublish`. `listening`/`xfer` are the flag bits; `flag55` = the engine's "do not check for duplicates".
    #[allow(clippy::too_many_arguments)]
    pub fn publish(
        self: &Arc<Self>,
        name: &[u8],
        address: &[u8],
        listening: bool,
        xfer: bool,
        data_event: *mut c_void,
        listen_event: *mut c_void,
        accept_event: *mut c_void,
        out: &mut *mut Server,
        flag55: bool,
        announce: bool,
        adapter: u32,
    ) -> i32 {
        let mut port = u32::from(self.port(GAME_PORT));
        let found = if !flag55 && self.query_games(name) == OK {
            let games = self.detected.lock().expect("no panics while locked");
            games.iter().any(|g| g.address == address)
        } else {
            false
        };
        if found {
            let error = self.host.native_error();
            self.host.record_native_error(error);
            return NATIVE_ERROR;
        }
        self.adapter_index.store(self.find_adapter_address_index(adapter), Ordering::Relaxed);
        let mut server = Box::new(Server {
            socket: SOCK_INVALID,
            xfer_socket: SOCK_INVALID,
            clients: Vec::new(),
            connections: Vec::new(),
            listening,
            xfer,
            name: Vec::new(),
            address: Vec::new(),
            port: 0,
            allow_new_connections: false,
            flag55,
            announce,
        });
        let mut result = OK;
        if listening {
            let (code, socket, bound) = self.create_socket(1, port);
            port = bound;
            result = code;
            if code == OK {
                server.port = bound;
                server.socket = socket;
                server.allow_new_connections = false;
                self.host.select_event(socket, data_event, EVENT_READ);
            }
        }
        if result == OK && xfer {
            let (code, socket, bound) = self.create_socket(2, port);
            result = code;
            if code == OK {
                server.port = bound;
                server.xfer_socket = socket;
                server.allow_new_connections = true;
                self.host.select_event(socket, data_event, EVENT_READ);
            }
        }
        if result != OK {
            if server.socket != SOCK_INVALID {
                self.host.close(server.socket);
            }
            if server.xfer_socket != SOCK_INVALID {
                self.host.close(server.xfer_socket);
            }
            return result;
        }
        server.name = name.to_vec();
        server.address = address.to_vec();
        *out = Box::into_raw(server);
        let listen = self.host.open(SOCK_STREAM);
        self.listen_socket.store(listen, Ordering::Relaxed);
        if listen == SOCK_INVALID || !self.host.set_non_blocking(listen, true) {
            self.host.record_native_error(self.host.last_error());
            self.host.close(listen);
            return NATIVE_ERROR;
        }
        let local = SockAddr::new(self.port(XFER_PORT), 0);
        if !self.host.bind(listen, &local) || !self.host.listen(listen, MAX_BACKLOG) {
            self.host.record_native_error(self.host.last_error());
            self.host.close(listen);
            return NATIVE_ERROR;
        }
        self.host.select_event(listen, listen_event, EVENT_READ);
        self.host.select_event(listen, accept_event, EVENT_ACCEPT);
        OK
    }

    fn matches(from: &SockAddr, peer: &SockAddr) -> bool {
        from.family == peer.family && from.port == peer.port && from.address == peer.address
    }

    /// `NPIMReceive`: `(result, received)`. A datagram's discovery packets are dropped and other senders skipped.
    pub fn receive(&self, connection: &Connection, buffer: &mut [u8], received: &mut u32) -> i32 {
        if connection.state == 1 {
            let count = self.host.recv(connection.socket, buffer);
            return match count {
                -1 => {
                    let error = self.host.last_error();
                    if error == E_WOULD_BLOCK {
                        *received = 0;
                        WOULD_BLOCK
                    } else {
                        self.host.record_native_error(error);
                        NATIVE_ERROR
                    }
                }
                0 => UNAVAILABLE,
                n => {
                    *received = n as u32;
                    OK
                }
            };
        }
        loop {
            let mut from = SockAddr::default();
            let count = self.host.recv_from(connection.socket, buffer, &mut from);
            if count == 0 {
                return UNAVAILABLE;
            }
            if count == -1 {
                let error = self.host.last_error();
                if error == E_WOULD_BLOCK {
                    *received = 0;
                    return WOULD_BLOCK;
                }
                self.host.record_native_error(error);
                return NATIVE_ERROR;
            }
            let key = buffer[0];
            self.encipher(&mut buffer[1..count as usize], key);
            let n = count as usize;
            if (n == ANNOUNCEMENT_PACKET_SIZE && buffer[1] == ANNOUNCEMENT_PACKET_TYPE)
                || (n == QUERY_PACKET_SIZE && buffer[1] == QUERY_PACKET_TYPE)
            {
                *received = 0;
                continue;
            }
            if Self::matches(&from, &connection.address) {
                *received = n as u32;
                return OK;
            }
        }
    }

    /// `NPIMReceiveFrom`: returns the result; `received`/`index` as the C++.
    pub fn receive_from(&self, server: &mut Server, buffer: &mut [u8], received: &mut u32, index: &mut u32) -> i32 {
        if !server.xfer {
            return UNAVAILABLE;
        }
        let mut from = SockAddr::default();
        let count = self.host.recv_from(server.xfer_socket, buffer, &mut from);
        if count == 0 {
            return UNAVAILABLE;
        }
        if count == -1 {
            let error = self.host.last_error();
            if error == E_WOULD_BLOCK {
                *received = 0;
                return WOULD_BLOCK;
            }
            self.host.record_native_error(error);
            return NATIVE_ERROR;
        }
        let key = buffer[0];
        self.encipher(&mut buffer[1..count as usize], key);
        let n = count as usize;
        if n == QUERY_PACKET_SIZE && buffer[1] == QUERY_PACKET_TYPE {
            *received = 0;
            if c_field(&buffer[2..2 + NAME_FIELD]) == server.name.as_slice()
                && server.allow_new_connections
                && !server.flag55
                && server.announce
            {
                self.send_announcement(server, from);
            }
            return WOULD_BLOCK;
        }
        let existing = server
            .clients
            .iter()
            .position(|c| from.family == c.family && from.address == c.address && from.port == c.port);
        if let Some(i) = existing {
            *index = i as u32;
            *received = n as u32;
            return OK;
        }
        if n >= 4 && (buffer[1] == 0x22 || buffer[1] == 0x37) {
            if server.allow_new_connections && from.port != 0 {
                server.clients.push(from);
                *index = (server.clients.len() - 1) as u32;
                *received = n as u32;
                server.connections.push(std::ptr::null_mut());
            } else {
                *index = 0;
                *received = 0;
            }
            return OK;
        }
        *received = 0;
        WOULD_BLOCK
    }

    fn send_announcement(&self, server: &Server, mut from: SockAddr) {
        let socket = self.host.open(SOCK_DGRAM);
        if socket == SOCK_INVALID {
            self.record_error();
            return;
        }
        let mut packet = [0u8; ANNOUNCEMENT_PACKET_SIZE];
        packet[1] = ANNOUNCEMENT_PACKET_TYPE;
        copy_c_string(&mut packet[2..2 + NAME_FIELD], &server.name);
        copy_c_string(&mut packet[0x43..0x43 + ADDRESS_FIELD], &server.address);
        let key = self.new_key();
        packet[0] = key;
        self.encipher(&mut packet[1..], key);
        from.port = self.port(GAME_PORT);
        if self.host.send_to(socket, &packet, &from) == -1 {
            self.record_error();
        }
        self.host.close(socket);
    }

    pub fn reset_client_connection(&self, server: &mut Server, index: u32) {
        if let Some(client) = server.clients.get_mut(index as usize) {
            client.port = 0;
        }
    }

    pub fn reset_xfer(&self, server: &mut Server) {
        for &connection in &server.connections {
            if !connection.is_null() {
                // SAFETY: entries are connections accepted for this server, freed here or by `close_server`
                drop(unsafe { Box::from_raw(connection) });
            }
        }
        server.connections.clear();
    }

    /// `NPIMSend`.
    pub fn send(&self, connection: &Connection, data: &mut [u8]) -> i32 {
        if connection.state == 1 {
            let mut remaining = data;
            while !remaining.is_empty() {
                let count = self.host.send(connection.socket, remaining);
                if count == -1 {
                    return self.native_error_code();
                }
                remaining = &mut remaining[count as usize..];
            }
            return OK;
        }
        if !data.is_empty() {
            let key = self.new_key();
            data[0] = key;
            self.encipher(&mut data[1..], key);
        }
        let count = self.host.send_to(connection.socket, data, &connection.address);
        if count == data.len() as i32 {
            OK
        } else if count != -1 {
            UNAVAILABLE
        } else {
            self.native_error_code()
        }
    }

    /// `NPIMSendTo`.
    pub fn send_to(&self, server: &Server, index: u32, data: &mut [u8]) -> i32 {
        if !server.xfer {
            return UNAVAILABLE;
        }
        let Some(client) = server.clients.get(index as usize) else {
            return INDEX_RANGE;
        };
        if !data.is_empty() {
            let key = self.new_key();
            data[0] = key;
            self.encipher(&mut data[1..], key);
        }
        let count = self.host.send_to(server.xfer_socket, data, client);
        if count == data.len() as i32 {
            OK
        } else if count == -1 {
            self.native_error_code()
        } else {
            UNAVAILABLE
        }
    }

    /// `NPIMSendSingle`: one datagram from a temporary socket. `OK` when the socket could not even be opened, as the
    /// C++ does.
    pub fn send_single(&self, host_name: &[u8], port: u16, data: &[u8]) -> i32 {
        let socket = self.host.open(SOCK_DGRAM);
        if socket == SOCK_INVALID {
            return OK;
        }
        let mut result = OK;
        let mut addresses = [0u32; MAX_HOST_ADDRESSES];
        let mut count = self.host.resolve_ipv4(host_name, &mut addresses) as usize;
        if count == 0 {
            let parsed = self.host.parse_ipv4(host_name);
            let mut scratch = [0u8; 0x400];
            if parsed != INADDR_NONE && self.host.reverse_lookup(parsed, &mut scratch) {
                addresses[0] = parsed;
                count = 1;
            }
        }
        let mut to = SockAddr::default();
        if count != 0 {
            to = SockAddr::new(net16(port), 0);
            // the index comes from the local host's list; an address the remote list lacks is 0
            let index = self.adapter_index.load(Ordering::Relaxed) as usize;
            if index < count {
                to.address = addresses[index];
            }
        } else {
            result = UNAVAILABLE;
        }
        if result == OK {
            let local = SockAddr::new(0, 0);
            if self.host.bind(socket, &local) {
                let sent = self.host.send_to(socket, data, &to);
                result = if sent == data.len() as i32 {
                    OK
                } else if sent != -1 {
                    UNAVAILABLE
                } else {
                    self.native_error_code()
                };
            } else {
                result = self.native_error_code();
            }
        }
        self.host.close(socket);
        result
    }

    pub fn xfer_receive(&self, buffer: &mut [u8], received: &mut u32) -> i32 {
        let count = self.host.recv(self.xfer_socket.load(Ordering::Relaxed), buffer);
        match count {
            0 => UNAVAILABLE,
            -1 => {
                let error = self.host.last_error();
                if error == E_WOULD_BLOCK {
                    *received = 0;
                    WOULD_BLOCK
                } else {
                    self.host.record_native_error(error);
                    NATIVE_ERROR
                }
            }
            n => {
                *received = n as u32;
                OK
            }
        }
    }

    pub fn xfer_send_to(&self, server: &Server, index: u32, data: &[u8]) -> i32 {
        let _lock = self.xfer_lock.lock().expect("no panics while locked");
        let Some(&connection) = server.connections.get(index as usize) else {
            return INDEX_RANGE;
        };
        if connection.is_null() {
            return INDEX_RANGE;
        }
        // SAFETY: a live connection accepted for this server (the C++'s lifetime contract)
        let socket = unsafe { (*connection).socket };
        let mut remaining = data;
        while !remaining.is_empty() {
            let count = self.host.send(socket, remaining);
            if count != -1 {
                remaining = &remaining[count as usize..];
            } else if self.host.last_error() == E_WOULD_BLOCK {
                self.host.sleep_ms(1);
            } else {
                return NATIVE_ERROR;
            }
        }
        OK
    }

    /// Broadcasts a game query and gathers the announcements answering within a second into the detected list.
    pub fn query_games(&self, name: &[u8]) -> i32 {
        if name.is_empty() {
            return OK;
        }
        let _lock = self.socket_lock.lock().expect("no panics while locked");
        let send_socket = self.host.open(SOCK_DGRAM);
        if send_socket == SOCK_INVALID {
            return self.native_error_code();
        }
        let result = self.query_games_with(send_socket, name);
        self.host.close(send_socket);
        result
    }

    fn query_games_with(&self, send_socket: Sock, name: &[u8]) -> i32 {
        let to = SockAddr::new(self.port(GAME_PORT), 0xffff_ffff);
        if !self.host.set_broadcast(send_socket, true) {
            return self.native_error_code();
        }
        let local = SockAddr::new(self.port(GAME_PORT), 0);
        let receive_socket = self.host.open(SOCK_DGRAM);
        if !self.host.bind(receive_socket, &local) || !self.host.set_non_blocking(receive_socket, true) {
            let code = self.native_error_code();
            self.host.close(receive_socket);
            return code;
        }
        let mut query = [0u8; QUERY_PACKET_SIZE];
        query[1] = QUERY_PACKET_TYPE;
        copy_c_string(&mut query[2..2 + NAME_FIELD], name);
        let key = self.new_key();
        query[0] = key;
        self.encipher(&mut query[1..], key);
        if self.host.send_to(send_socket, &query, &to) == -1 {
            let code = self.native_error_code();
            self.host.close(receive_socket);
            return code;
        }
        let mut found: Vec<DetectedGame> = Vec::new();
        let deadline = self.host.time_ms().wrapping_add(1000);
        while deadline > self.host.time_ms() {
            let mut buffer = [0u8; 0x200];
            let mut from = SockAddr::default();
            let count = self.host.recv_from(receive_socket, &mut buffer, &mut from);
            if count > 0 {
                let key = buffer[0];
                self.encipher(&mut buffer[1..count as usize], key);
            }
            if count == ANNOUNCEMENT_PACKET_SIZE as i32 && buffer[1] == ANNOUNCEMENT_PACKET_TYPE {
                let ip = self.host.format_ipv4(from.address);
                if found.iter().all(|g| g.name != ip) {
                    found.push(DetectedGame {
                        name: ip,
                        host: c_field(&buffer[2..2 + NAME_FIELD]).to_vec(),
                        address: c_field(&buffer[0x43..0x43 + ADDRESS_FIELD]).to_vec(),
                        stream_address: SockAddr::default(),
                        datagram_address: from,
                    });
                }
            }
            self.host.sleep_ms(1);
        }
        *self.detected.lock().expect("no panics while locked") = found;
        self.host.close(receive_socket);
        OK
    }

    /// `NPIMSearchForGame`: starts or stops the background search.
    pub fn search_for_game(self: &Arc<Self>, name: &[u8], start: bool) -> i32 {
        if name.is_empty() {
            return OK;
        }
        let mut guard = self.searcher.lock().expect("no panics while locked");
        if start {
            if guard.is_none() {
                self.detected.lock().expect("no panics while locked").clear();
                let (exit, inbox) = mpsc::channel();
                let tcp = Arc::clone(self);
                let name = name.to_vec();
                let thread = std::thread::spawn(move || loop {
                    match inbox.recv_timeout(Duration::from_millis(50)) {
                        Ok(()) | Err(RecvTimeoutError::Disconnected) => break,
                        Err(RecvTimeoutError::Timeout) => {
                            tcp.query_games(&name);
                        }
                    }
                });
                *guard = Some(Searcher { exit, thread });
            }
        } else {
            if let Some(searcher) = guard.take() {
                let _ = searcher.exit.send(());
                let _ = searcher.thread.join();
            }
            self.detected.lock().expect("no panics while locked").clear();
        }
        OK
    }

    #[cfg(test)]
    pub(crate) fn set_adapter_index(&self, index: u32) {
        self.adapter_index.store(index, Ordering::Relaxed);
    }

    #[cfg(test)]
    pub(crate) fn listen_socket(&self) -> Sock {
        self.listen_socket.load(Ordering::Relaxed)
    }

    #[cfg(test)]
    pub(crate) fn set_detected(&self, games: Vec<DetectedGame>) {
        *self.detected.lock().expect("no panics while locked") = games;
    }

    #[cfg(test)]
    pub(crate) fn key_buffer_filled(&self) -> bool {
        self.cipher.lock().expect("no panics while locked").key_buffer().is_some()
    }
}
