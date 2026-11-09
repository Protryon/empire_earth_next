//! The plug-in over a scripted fake network: every entry point, its result codes and its error paths.

use std::collections::VecDeque;
use std::ffi::c_void;
use std::sync::{Arc, Mutex};

use crate::cipher::Cipher;
use crate::host::*;
use crate::plugin::*;

const WOULD_BLOCK_ERROR: u32 = E_WOULD_BLOCK;

#[derive(Default)]
struct State {
    next_socket: isize,
    open_fails: u32,
    closed: Vec<Sock>,
    close_fails: Vec<Sock>,
    bind_ok: Option<bool>,
    listen_ok: Option<bool>,
    connect_ok: Option<bool>,
    non_blocking_ok: Option<bool>,
    broadcast_ok: Option<bool>,
    startup_ok: Option<bool>,
    cleanup_ok: Option<bool>,
    last_error: u32,
    native_errors: Vec<u32>,
    accepts: VecDeque<(Sock, SockAddr)>,
    recvs: VecDeque<i32>,
    recv_from: VecDeque<(i32, Vec<u8>, SockAddr)>,
    peek_result: i32,
    send_limit: Option<i32>,
    send_to_result: Option<i32>,
    sent: Vec<Vec<u8>>,
    sent_to: Vec<(Vec<u8>, SockAddr)>,
    connects: Vec<SockAddr>,
    binds: Vec<SockAddr>,
    events: Vec<(Sock, u32)>,
    resolve: Vec<u32>,
    reverse_ok: bool,
    parse: u32,
    host_name_ok: Option<bool>,
    adapters: Vec<Adapter>,
    time: u32,
    sleeps: u32,
    get_name_port: u16,
}

#[derive(Default)]
struct Mock(Mutex<State>);

impl Host for Mock {
    fn startup(&self) -> bool {
        self.0.lock().unwrap().startup_ok.unwrap_or(true)
    }
    fn cleanup(&self) -> bool {
        self.0.lock().unwrap().cleanup_ok.unwrap_or(true)
    }
    fn last_error(&self) -> u32 {
        self.0.lock().unwrap().last_error
    }
    fn open(&self, _kind: i32) -> Sock {
        let mut s = self.0.lock().unwrap();
        if s.open_fails > 0 {
            s.open_fails -= 1;
            return SOCK_INVALID;
        }
        s.next_socket += 1;
        s.next_socket
    }
    fn close(&self, socket: Sock) -> bool {
        let mut s = self.0.lock().unwrap();
        s.closed.push(socket);
        !s.close_fails.contains(&socket)
    }
    fn bind(&self, _socket: Sock, address: &SockAddr) -> bool {
        let mut s = self.0.lock().unwrap();
        s.binds.push(*address);
        s.bind_ok.unwrap_or(true)
    }
    fn listen(&self, _socket: Sock, backlog: i32) -> bool {
        assert_eq!(backlog, MAX_BACKLOG);
        self.0.lock().unwrap().listen_ok.unwrap_or(true)
    }
    fn accept(&self, _socket: Sock, peer: Option<&mut SockAddr>) -> Sock {
        let mut s = self.0.lock().unwrap();
        match s.accepts.pop_front() {
            Some((socket, address)) => {
                if let Some(p) = peer {
                    *p = address;
                }
                socket
            }
            None => SOCK_INVALID,
        }
    }
    fn connect(&self, _socket: Sock, address: &SockAddr) -> bool {
        let mut s = self.0.lock().unwrap();
        s.connects.push(*address);
        s.connect_ok.unwrap_or(true)
    }
    fn get_name(&self, _socket: Sock, address: &mut SockAddr) -> bool {
        let mut s = self.0.lock().unwrap();
        s.get_name_port += 1;
        address.port = s.get_name_port;
        address.address = 0x0100_007f;
        true
    }
    fn set_non_blocking(&self, _socket: Sock, _non_blocking: bool) -> bool {
        self.0.lock().unwrap().non_blocking_ok.unwrap_or(true)
    }
    fn set_broadcast(&self, _socket: Sock, _broadcast: bool) -> bool {
        self.0.lock().unwrap().broadcast_ok.unwrap_or(true)
    }
    fn send(&self, _socket: Sock, data: &[u8]) -> i32 {
        let mut s = self.0.lock().unwrap();
        let limit = s.send_limit.unwrap_or(data.len() as i32);
        if limit < 0 {
            return limit;
        }
        let n = (limit as usize).min(data.len());
        s.sent.push(data[..n].to_vec());
        n as i32
    }
    fn recv(&self, _socket: Sock, data: &mut [u8]) -> i32 {
        let mut s = self.0.lock().unwrap();
        let result = s.recvs.pop_front().unwrap_or(-1);
        if result > 0 {
            data[..result as usize].fill(0xab);
        }
        result
    }
    fn peek(&self, _socket: Sock, _data: &mut [u8]) -> i32 {
        self.0.lock().unwrap().peek_result
    }
    fn send_to(&self, _socket: Sock, data: &[u8], to: &SockAddr) -> i32 {
        let mut s = self.0.lock().unwrap();
        s.sent_to.push((data.to_vec(), *to));
        s.send_to_result.unwrap_or(data.len() as i32)
    }
    fn recv_from(&self, _socket: Sock, data: &mut [u8], from: &mut SockAddr) -> i32 {
        let mut s = self.0.lock().unwrap();
        match s.recv_from.pop_front() {
            Some((result, bytes, address)) => {
                data[..bytes.len()].copy_from_slice(&bytes);
                *from = address;
                result
            }
            None => {
                s.last_error = WOULD_BLOCK_ERROR;
                -1
            }
        }
    }
    fn select_event(&self, socket: Sock, _event: *mut c_void, mask: u32) -> bool {
        self.0.lock().unwrap().events.push((socket, mask));
        true
    }
    fn get_host_name(&self, name: &mut [u8]) -> bool {
        name[..4].copy_from_slice(b"host");
        self.0.lock().unwrap().host_name_ok.unwrap_or(true)
    }
    fn resolve_ipv4(&self, _name: &[u8], addresses: &mut [u32]) -> u32 {
        let s = self.0.lock().unwrap();
        let n = s.resolve.len().min(addresses.len());
        addresses[..n].copy_from_slice(&s.resolve[..n]);
        n as u32
    }
    fn reverse_lookup(&self, _address: u32, _name: &mut [u8]) -> bool {
        self.0.lock().unwrap().reverse_ok
    }
    fn parse_ipv4(&self, _text: &[u8]) -> u32 {
        self.0.lock().unwrap().parse
    }
    fn format_ipv4(&self, address: u32) -> Vec<u8> {
        format!("ip{address:x}").into_bytes()
    }
    fn adapters(&self, max: u32) -> Vec<Adapter> {
        let s = self.0.lock().unwrap();
        s.adapters.iter().take(max as usize).cloned().collect()
    }
    fn sleep_ms(&self, _milliseconds: u32) {
        self.0.lock().unwrap().sleeps += 1;
    }
    fn time_ms(&self) -> u32 {
        let mut s = self.0.lock().unwrap();
        s.time += 10;
        s.time
    }
    fn record_native_error(&self, error: u32) {
        self.0.lock().unwrap().native_errors.push(error);
    }
    fn native_error(&self) -> u32 {
        0
    }
    fn port_offset(&self) -> i32 {
        0
    }
}

fn fresh() -> Arc<Tcp<Mock>> {
    Arc::new(Tcp::new(Mock::default()))
}

fn activated() -> Arc<Tcp<Mock>> {
    let tcp = fresh();
    assert_eq!(tcp.activate(), OK);
    tcp
}

fn stream_connection(socket: Sock) -> Connection {
    Connection { socket, address: SockAddr::default(), state: 1 }
}

fn datagram_connection(socket: Sock, address: SockAddr) -> Connection {
    Connection { socket, address, state: 2 }
}

fn empty_server() -> Server {
    Server {
        socket: SOCK_INVALID,
        xfer_socket: SOCK_INVALID,
        clients: Vec::new(),
        connections: Vec::new(),
        listening: false,
        xfer: false,
        name: b"game".to_vec(),
        address: b"addr".to_vec(),
        port: 0,
        allow_new_connections: true,
        flag55: false,
        announce: true,
    }
}

fn pack_datagram(tcp: &Tcp<Mock>, key: u8, plain: &[u8]) -> Vec<u8> {
    let mut packet = plain.to_vec();
    packet[0] = key;
    let mut body = packet[1..].to_vec();
    tcp_cipher_apply(tcp, &mut body, key);
    packet[1..].copy_from_slice(&body);
    packet
}

fn tcp_cipher_apply(_tcp: &Tcp<Mock>, data: &mut [u8], key: u8) {
    // a fresh cipher with the same fixed-seed key buffer as the plug-in's
    let mut cipher = Cipher::new();
    cipher.fill_key_buffer();
    cipher.apply(data, key);
}

fn null_event() -> *mut c_void {
    std::ptr::null_mut()
}

// ---- activate / deactivate

#[test]
fn activate_starts_up_and_fills_the_cipher() {
    let tcp = fresh();
    assert_eq!(tcp.activate(), OK);
    assert!(tcp.is_valid());
    assert!(tcp.key_buffer_filled());
    assert_eq!(tcp.host.0.lock().unwrap().closed, vec![1], "the probe socket is closed");
}

#[test]
fn activate_fails_when_startup_or_the_probe_fails() {
    let tcp = fresh();
    tcp.host.0.lock().unwrap().startup_ok = Some(false);
    tcp.host.0.lock().unwrap().last_error = 77;
    assert_eq!(tcp.activate(), NATIVE_ERROR);
    assert_eq!(tcp.host.0.lock().unwrap().native_errors, vec![77]);
    assert!(!tcp.is_valid());

    let tcp = fresh();
    tcp.host.0.lock().unwrap().open_fails = 1;
    assert_eq!(tcp.activate(), UNAVAILABLE);
    assert!(!tcp.is_valid());
}

#[test]
fn deactivate_clears_the_cipher() {
    let tcp = activated();
    assert_eq!(tcp.deactivate(), OK);
    assert!(!tcp.is_valid());
    assert!(!tcp.key_buffer_filled());
}

#[test]
fn deactivate_failure_is_recorded() {
    let tcp = activated();
    tcp.host.0.lock().unwrap().cleanup_ok = Some(false);
    tcp.host.0.lock().unwrap().last_error = 5;
    assert_eq!(tcp.deactivate(), NATIVE_ERROR);
    assert_eq!(tcp.host.0.lock().unwrap().native_errors, vec![5]);
    assert!(tcp.is_valid());
}

// ---- connect

#[test]
fn connect_datagram_by_dns() {
    let tcp = activated();
    tcp.host.0.lock().unwrap().resolve = vec![0x0a00_0001];
    let mut out = std::ptr::null_mut();
    assert_eq!(tcp.connect(b"srv", b"h", b"a", 2, null_event(), 0, &mut out), OK);
    let c = unsafe { Box::from_raw(out) };
    assert_eq!(c.state, 2);
    assert_eq!(c.address.address, 0x0a00_0001);
    assert_eq!(c.address.port, net16(33334));
    assert_eq!(c.address.family, AF_INET);
    let s = tcp.host.0.lock().unwrap();
    assert_eq!(s.events, vec![(c.socket, EVENT_READ)]);
    assert_eq!(s.binds, vec![SockAddr::new(0, 0)]);
    assert!(s.connects.is_empty());
}

#[test]
fn connect_kind_zero_is_unavailable() {
    let tcp = activated();
    let mut out = std::ptr::null_mut();
    assert_eq!(tcp.connect(b"srv", b"h", b"a", 0, null_event(), 0, &mut out), UNAVAILABLE);
}

#[test]
fn connect_stream_failure_closes_the_socket() {
    let tcp = activated();
    {
        let mut s = tcp.host.0.lock().unwrap();
        s.resolve = vec![1];
        s.connect_ok = Some(false);
        s.last_error = 9;
    }
    let mut out = std::ptr::null_mut();
    assert_eq!(tcp.connect(b"srv", b"h", b"a", 1, null_event(), 0, &mut out), NATIVE_ERROR);
    assert!(out.is_null());
    assert_eq!(tcp.host.0.lock().unwrap().native_errors, vec![9]);
}

#[test]
fn connect_falls_back_to_a_dotted_address_and_reverse_lookup() {
    let tcp = activated();
    {
        let mut s = tcp.host.0.lock().unwrap();
        s.parse = 0x0200_0002;
        s.reverse_ok = true;
    }
    let mut out = std::ptr::null_mut();
    assert_eq!(tcp.connect(b"1.2.3.4", b"h", b"a", 2, null_event(), 0, &mut out), OK);
    let c = unsafe { Box::from_raw(out) };
    assert_eq!(c.address.address, 0x0200_0002);
}

#[test]
fn connect_unknown_name_is_unavailable_and_closes_the_socket() {
    let tcp = activated();
    tcp.host.0.lock().unwrap().parse = INADDR_NONE;
    let mut out = std::ptr::null_mut();
    let before = tcp.host.0.lock().unwrap().closed.len();
    assert_eq!(tcp.connect(b"nowhere", b"h", b"a", 2, null_event(), 0, &mut out), UNAVAILABLE);
    assert_eq!(tcp.host.0.lock().unwrap().closed.len(), before + 1);
}

#[test]
fn connect_uses_a_detected_game_when_the_list_is_not_empty() {
    let tcp = activated();
    tcp.set_detected(vec![DetectedGame {
        name: b"n".to_vec(),
        host: b"h".to_vec(),
        address: b"a".to_vec(),
        datagram_address: SockAddr::new(net16(1), 0x07),
        stream_address: SockAddr::new(net16(2), 0x08),
    }]);
    let mut out = std::ptr::null_mut();
    assert_eq!(tcp.connect(b"n", b"h", b"a", 2, null_event(), 0, &mut out), OK);
    let c = unsafe { Box::from_raw(out) };
    assert_eq!(c.address.address, 0x07);
    assert_eq!(c.address.port, net16(33334), "the port is overwritten with the game's");
    let mut out = std::ptr::null_mut();
    assert_eq!(tcp.connect(b"n", b"h", b"a", 1, null_event(), 0, &mut out), OK);
    let c = unsafe { Box::from_raw(out) };
    assert_eq!(c.address.address, 0x08);
    let mut out = std::ptr::null_mut();
    assert_eq!(tcp.connect(b"x", b"h", b"a", 2, null_event(), 0, &mut out), BAD_INDEX);
    assert!(out.is_null());
}

#[test]
fn connect_bind_failure_closes() {
    let tcp = activated();
    {
        let mut s = tcp.host.0.lock().unwrap();
        s.resolve = vec![1];
        s.bind_ok = Some(false);
        s.last_error = 3;
    }
    let mut out = std::ptr::null_mut();
    assert_eq!(tcp.connect(b"srv", b"h", b"a", 2, null_event(), 0, &mut out), NATIVE_ERROR);
    assert!(out.is_null());
}

// ---- send/receive and the cipher

#[test]
fn a_datagram_round_trips_through_the_cipher() {
    let tcp = activated();
    let peer = SockAddr::new(net16(33334), 0x0100_007f);
    let c = datagram_connection(5, peer);
    let mut packet = vec![0u8; 50];
    packet[1..].copy_from_slice(&[7u8; 49]);
    let plain = packet.clone();
    assert_eq!(tcp.send(&c, &mut packet), OK);
    let sent = tcp.host.0.lock().unwrap().sent_to[0].clone();
    assert_eq!(sent.1, peer);
    assert_ne!(sent.0[1..], plain[1..], "the body is enciphered");
    assert_ne!(sent.0[0], 0, "the key byte is 1..=255");

    // the deliver side: decipher through receive
    tcp.host.0.lock().unwrap().recv_from.push_back((sent.0.len() as i32, sent.0.clone(), peer));
    let mut buffer = vec![0u8; 100];
    let mut received = 0u32;
    assert_eq!(tcp.receive(&c, &mut buffer, &mut received), OK);
    assert_eq!(received, 50);
    assert_eq!(buffer[1..50], plain[1..]);
}

#[test]
fn receive_skips_other_senders_and_drops_discovery_packets() {
    let tcp = activated();
    let peer = SockAddr::new(net16(1), 2);
    let other = SockAddr::new(net16(9), 9);
    let c = datagram_connection(5, peer);
    let discovery = pack_datagram(&tcp, 3, &{
        let mut p = vec![0u8; 0x88];
        p[1] = 0x16;
        p
    });
    let wrong = pack_datagram(&tcp, 3, &[0u8; 40]);
    let good = pack_datagram(&tcp, 4, &[0u8; 30]);
    {
        let mut s = tcp.host.0.lock().unwrap();
        s.recv_from.push_back((0x88, discovery, peer));
        s.recv_from.push_back((40, wrong, other));
        s.recv_from.push_back((30, good, peer));
    }
    let mut buffer = vec![0u8; 200];
    let mut received = 77u32;
    assert_eq!(tcp.receive(&c, &mut buffer, &mut received), OK);
    assert_eq!(received, 30);
}

#[test]
fn receive_datagram_would_block_and_errors() {
    let tcp = activated();
    let c = datagram_connection(5, SockAddr::default());
    let mut buffer = vec![0u8; 8];
    let mut received = 9u32;
    // the queue is empty: would block
    assert_eq!(tcp.receive(&c, &mut buffer, &mut received), WOULD_BLOCK);
    assert_eq!(received, 0);
    {
        let mut s = tcp.host.0.lock().unwrap();
        s.recv_from.push_back((-1, vec![], SockAddr::default()));
        s.last_error = 42;
    }
    received = 9;
    assert_eq!(tcp.receive(&c, &mut buffer, &mut received), NATIVE_ERROR);
    assert_eq!(received, 9, "a native error leaves the size alone");
    assert_eq!(tcp.host.0.lock().unwrap().native_errors, vec![42]);
    tcp.host.0.lock().unwrap().recv_from.push_back((0, vec![], SockAddr::default()));
    assert_eq!(tcp.receive(&c, &mut buffer, &mut received), UNAVAILABLE);
}

#[test]
fn stream_receive_codes() {
    let tcp = activated();
    let c = stream_connection(5);
    let mut buffer = vec![0u8; 8];
    let mut received = 0u32;
    tcp.host.0.lock().unwrap().recvs.push_back(4);
    assert_eq!(tcp.receive(&c, &mut buffer, &mut received), OK);
    assert_eq!(received, 4);
    tcp.host.0.lock().unwrap().recvs.push_back(0);
    assert_eq!(tcp.receive(&c, &mut buffer, &mut received), UNAVAILABLE);
    {
        let mut s = tcp.host.0.lock().unwrap();
        s.recvs.push_back(-1);
        s.last_error = WOULD_BLOCK_ERROR;
    }
    assert_eq!(tcp.receive(&c, &mut buffer, &mut received), WOULD_BLOCK);
    assert_eq!(received, 0);
    {
        let mut s = tcp.host.0.lock().unwrap();
        s.recvs.push_back(-1);
        s.last_error = 11;
    }
    assert_eq!(tcp.receive(&c, &mut buffer, &mut received), NATIVE_ERROR);
}

#[test]
fn stream_send_writes_everything_and_reports_errors() {
    let tcp = activated();
    let c = stream_connection(5);
    tcp.host.0.lock().unwrap().send_limit = Some(3);
    let mut data = [1u8; 7];
    assert_eq!(tcp.send(&c, &mut data), OK);
    assert_eq!(tcp.host.0.lock().unwrap().sent.iter().map(Vec::len).sum::<usize>(), 7);
    tcp.host.0.lock().unwrap().send_limit = Some(-1);
    tcp.host.0.lock().unwrap().last_error = 5;
    assert_eq!(tcp.send(&c, &mut [1u8; 3]), NATIVE_ERROR);
    assert_eq!(tcp.host.0.lock().unwrap().native_errors, vec![5]);
}

#[test]
fn datagram_send_short_and_failed() {
    let tcp = activated();
    let c = datagram_connection(5, SockAddr::default());
    tcp.host.0.lock().unwrap().send_to_result = Some(2);
    assert_eq!(tcp.send(&c, &mut [0u8; 8]), UNAVAILABLE);
    tcp.host.0.lock().unwrap().send_to_result = Some(-1);
    assert_eq!(tcp.send(&c, &mut [0u8; 8]), NATIVE_ERROR);
}

// ---- accept and xfer connections

#[test]
fn accept_codes() {
    let tcp = activated();
    let mut server = empty_server();
    let mut out = std::ptr::null_mut();
    assert_eq!(tcp.accept(&server, &mut out), UNAVAILABLE, "not listening");
    server.listening = true;
    tcp.host.0.lock().unwrap().last_error = WOULD_BLOCK_ERROR;
    assert_eq!(tcp.accept(&server, &mut out), WOULD_BLOCK);
    tcp.host.0.lock().unwrap().last_error = 7;
    assert_eq!(tcp.accept(&server, &mut out), NATIVE_ERROR);
    tcp.host.0.lock().unwrap().accepts.push_back((42, SockAddr::default()));
    assert_eq!(tcp.accept(&server, &mut out), OK);
    let c = unsafe { Box::from_raw(out) };
    assert_eq!((c.socket, c.state), (42, 1));
}

#[test]
fn accept_for_xfer_matches_the_client_and_binds_its_slot() {
    let tcp = activated();
    let mut server = empty_server();
    server.clients.push(SockAddr::new(net16(1000), 0x0a0b0c0d));
    server.connections.push(std::ptr::null_mut());

    // an unknown sender: closed, 13
    tcp.host.0.lock().unwrap().accepts.push_back((8, SockAddr::new(net16(1), 0x99)));
    let mut out = std::ptr::null_mut();
    assert_eq!(tcp.accept_for_xfer(&mut server, &mut out), 13);
    assert!(tcp.host.0.lock().unwrap().closed.contains(&8));

    // a known client (the port is not compared)
    tcp.host.0.lock().unwrap().accepts.push_back((9, SockAddr::new(net16(5), 0x0a0b0c0d)));
    assert_eq!(tcp.accept_for_xfer(&mut server, &mut out), OK);
    assert_eq!(server.connections[0], out);
    assert_eq!(unsafe { (*out).socket }, 9);
    drop(unsafe { Box::from_raw(out) });
    server.connections[0] = std::ptr::null_mut();

    tcp.host.0.lock().unwrap().last_error = WOULD_BLOCK_ERROR;
    assert_eq!(tcp.accept_for_xfer(&mut server, &mut out), WOULD_BLOCK);
}

#[test]
fn connect_for_xfer_validates_its_arguments() {
    let tcp = activated();
    let mut out = std::ptr::null_mut();
    let host = Connection { socket: 1, address: SockAddr::new(net16(5), 0x7f00_0001), state: 2 };
    assert_eq!(tcp.connect_for_xfer(Some(&host), 2, null_event(), &mut out), 13, "bad mode");
    assert_eq!(tcp.connect_for_xfer(None, 1, null_event(), &mut out), 13, "no host");
    assert_eq!(tcp.connect_for_xfer(Some(&host), 1, null_event(), &mut out), OK);
    let c = unsafe { Box::from_raw(out) };
    assert_eq!(c.state, 1);
    assert_eq!(c.address.address, 0x7f00_0001);
    assert_eq!(c.address.port, net16(33335));
    let s = tcp.host.0.lock().unwrap();
    assert_eq!(s.connects[0].port, net16(33335));
    assert_eq!(s.events[0], (c.socket, EVENT_READ));
    drop(s);

    tcp.host.0.lock().unwrap().connect_ok = Some(false);
    tcp.host.0.lock().unwrap().last_error = 77;
    assert_eq!(tcp.connect_for_xfer(Some(&host), 1, null_event(), &mut out), NATIVE_ERROR);
}

// ---- publish

#[test]
fn publish_creates_its_sockets_and_the_shared_listen_socket() {
    let tcp = activated();
    let mut out = std::ptr::null_mut();
    let result =
        tcp.publish(b"name", b"address", true, true, null_event(), null_event(), null_event(), &mut out, true, true, 0);
    assert_eq!(result, OK);
    let server = unsafe { Box::from_raw(out) };
    assert!(server.listening && server.xfer);
    assert_ne!(server.socket, SOCK_INVALID);
    assert_ne!(server.xfer_socket, SOCK_INVALID);
    assert_eq!(server.name, b"name");
    assert_eq!(server.address, b"address");
    assert!(server.allow_new_connections, "the datagram socket allows new connections");
    assert_ne!(tcp.listen_socket(), SOCK_INVALID);
    let s = tcp.host.0.lock().unwrap();
    // the game port goes to the first socket, the bound port to the second
    assert_eq!(s.binds[0].port, net16(33334));
    assert_eq!(s.binds[1].port, 1, "the port get_name reported");
    assert_eq!(s.binds[2].port, net16(33335));
    // the listen socket has both events
    assert!(s.events.contains(&(tcp.listen_socket(), EVENT_READ)));
    assert!(s.events.contains(&(tcp.listen_socket(), EVENT_ACCEPT)));
}

#[test]
fn publish_refuses_a_game_already_announced_at_that_address() {
    let tcp = activated();
    let ann = {
        let mut p = vec![0u8; 0x88];
        p[1] = 0x16;
        p[0x43..0x43 + 4].copy_from_slice(b"addr");
        pack_datagram(&tcp, 9, &p)
    };
    tcp.host.0.lock().unwrap().recv_from.push_back((0x88, ann, SockAddr::new(net16(33334), 0x11)));
    let mut out = std::ptr::null_mut();
    let result =
        tcp.publish(b"name", b"addr", true, false, null_event(), null_event(), null_event(), &mut out, false, true, 0);
    assert_eq!(result, NATIVE_ERROR);
    assert!(out.is_null());
}

#[test]
fn publish_flag55_skips_the_search() {
    let tcp = activated();
    let mut out = std::ptr::null_mut();
    assert_eq!(
        tcp.publish(b"name", b"addr", true, false, null_event(), null_event(), null_event(), &mut out, true, false, 0),
        OK
    );
    drop(unsafe { Box::from_raw(out) });
    assert!(tcp.host.0.lock().unwrap().recv_from.is_empty());
}

#[test]
fn publish_socket_failure_cleans_up() {
    let tcp = activated();
    tcp.host.0.lock().unwrap().non_blocking_ok = Some(false);
    tcp.host.0.lock().unwrap().last_error = 3;
    let mut out = std::ptr::null_mut();
    assert_eq!(
        tcp.publish(b"n", b"a", true, false, null_event(), null_event(), null_event(), &mut out, true, false, 0),
        NATIVE_ERROR
    );
    assert!(out.is_null());
}

#[test]
fn publish_listen_socket_failure_still_returns_the_server() {
    let tcp = activated();
    let mut out = std::ptr::null_mut();
    tcp.host.0.lock().unwrap().listen_ok = Some(false);
    assert_eq!(
        tcp.publish(b"n", b"a", false, true, null_event(), null_event(), null_event(), &mut out, true, false, 0),
        NATIVE_ERROR
    );
    assert!(!out.is_null(), "the shared listen socket failing leaves the handle set, as the C++");
    drop(unsafe { Box::from_raw(out) });
}

// ---- server datagrams

#[test]
fn receive_from_registers_a_joining_client() {
    let tcp = activated();
    let mut server = empty_server();
    server.xfer = true;
    server.xfer_socket = 3;
    let from = SockAddr::new(net16(4444), 0x0102_0304);
    let packet = {
        let mut p = vec![0u8; 20];
        p[1] = 0x22;
        p
    };
    let ciphered = pack_datagram(&tcp, 5, &packet);
    tcp.host.0.lock().unwrap().recv_from.push_back((20, ciphered.clone(), from));
    let mut buffer = vec![0u8; 50];
    let (mut received, mut index) = (0, 99);
    assert_eq!(tcp.receive_from(&mut server, &mut buffer, &mut received, &mut index), OK);
    assert_eq!((received, index), (20, 0));
    assert_eq!(server.clients, vec![from]);
    assert_eq!(server.connections.len(), 1);

    // the known client's next packet is routed to it
    tcp.host.0.lock().unwrap().recv_from.push_back((20, ciphered, from));
    assert_eq!(tcp.receive_from(&mut server, &mut buffer, &mut received, &mut index), OK);
    assert_eq!(index, 0);
}

#[test]
fn receive_from_refused_join_zeroes_its_outputs() {
    let tcp = activated();
    let mut server = empty_server();
    server.xfer = true;
    server.allow_new_connections = false;
    let packet = {
        let mut p = vec![0u8; 20];
        p[1] = 0x37;
        p
    };
    let from = SockAddr::new(net16(1), 2);
    tcp.host.0.lock().unwrap().recv_from.push_back((20, pack_datagram(&tcp, 2, &packet), from));
    let mut buffer = vec![0u8; 50];
    let (mut received, mut index) = (7, 7);
    assert_eq!(tcp.receive_from(&mut server, &mut buffer, &mut received, &mut index), OK);
    assert_eq!((received, index), (0, 0));
    assert!(server.clients.is_empty());
}

#[test]
fn receive_from_answers_a_query_with_an_announcement() {
    let tcp = activated();
    let mut server = empty_server();
    server.xfer = true;
    let mut query = vec![0u8; 0x43];
    query[1] = 0x15;
    query[2..6].copy_from_slice(b"game");
    let from = SockAddr::new(net16(2222), 0x0505_0505);
    tcp.host.0.lock().unwrap().recv_from.push_back((0x43, pack_datagram(&tcp, 6, &query), from));
    let mut buffer = vec![0u8; 200];
    let (mut received, mut index) = (3, 3);
    assert_eq!(tcp.receive_from(&mut server, &mut buffer, &mut received, &mut index), WOULD_BLOCK);
    assert_eq!(received, 0);
    let (packet, to) = tcp.host.0.lock().unwrap().sent_to[0].clone();
    assert_eq!(packet.len(), 0x88);
    assert_eq!(to.address, from.address);
    assert_eq!(to.port, net16(33334));
    let key = packet[0];
    let mut body = packet[1..].to_vec();
    tcp_cipher_apply(&tcp, &mut body, key);
    assert_eq!(body[0], 0x16);
    assert_eq!(&body[1..5], b"game");
    assert_eq!(&body[0x42..0x46], b"addr");
}

#[test]
fn receive_from_query_of_another_name_or_flags_is_not_answered() {
    let tcp = activated();
    let mut server = empty_server();
    server.xfer = true;
    server.announce = false;
    let mut query = vec![0u8; 0x43];
    query[1] = 0x15;
    query[2..6].copy_from_slice(b"game");
    let from = SockAddr::new(net16(1), 2);
    tcp.host.0.lock().unwrap().recv_from.push_back((0x43, pack_datagram(&tcp, 6, &query), from));
    let mut buffer = vec![0u8; 200];
    let (mut received, mut index) = (3, 3);
    assert_eq!(tcp.receive_from(&mut server, &mut buffer, &mut received, &mut index), WOULD_BLOCK);
    assert!(tcp.host.0.lock().unwrap().sent_to.is_empty());
}

#[test]
fn receive_from_unknown_sender_without_a_join_is_ignored() {
    let tcp = activated();
    let mut server = empty_server();
    server.xfer = true;
    let from = SockAddr::new(net16(1), 2);
    tcp.host.0.lock().unwrap().recv_from.push_back((3, pack_datagram(&tcp, 1, &[0u8, 0x22, 9]), from));
    let mut buffer = vec![0u8; 50];
    let (mut received, mut index) = (3, 3);
    // too short for a join packet
    assert_eq!(tcp.receive_from(&mut server, &mut buffer, &mut received, &mut index), WOULD_BLOCK);
    assert_eq!(received, 0);
    assert!(server.clients.is_empty());
}

#[test]
fn receive_from_codes() {
    let tcp = activated();
    let mut server = empty_server();
    let mut buffer = vec![0u8; 8];
    let (mut received, mut index) = (0, 0);
    assert_eq!(tcp.receive_from(&mut server, &mut buffer, &mut received, &mut index), UNAVAILABLE);
    server.xfer = true;
    tcp.host.0.lock().unwrap().recv_from.push_back((0, vec![], SockAddr::default()));
    assert_eq!(tcp.receive_from(&mut server, &mut buffer, &mut received, &mut index), UNAVAILABLE);
    received = 9;
    assert_eq!(tcp.receive_from(&mut server, &mut buffer, &mut received, &mut index), WOULD_BLOCK);
    assert_eq!(received, 0);
}

#[test]
fn send_to_codes() {
    let tcp = activated();
    let mut server = empty_server();
    assert_eq!(tcp.send_to(&server, 0, &mut [0u8; 4]), UNAVAILABLE);
    server.xfer = true;
    assert_eq!(tcp.send_to(&server, 0, &mut [0u8; 4]), 10);
    server.clients.push(SockAddr::new(net16(5), 6));
    assert_eq!(tcp.send_to(&server, 0, &mut [0u8; 4]), OK);
    tcp.host.0.lock().unwrap().send_to_result = Some(1);
    assert_eq!(tcp.send_to(&server, 0, &mut [0u8; 4]), UNAVAILABLE);
    tcp.host.0.lock().unwrap().send_to_result = Some(-1);
    assert_eq!(tcp.send_to(&server, 0, &mut [0u8; 4]), NATIVE_ERROR);
}

#[test]
fn xfer_send_to_retries_on_would_block() {
    let tcp = activated();
    let mut server = empty_server();
    let c = Box::into_raw(Box::new(stream_connection(11)));
    server.connections.push(c);
    assert_eq!(tcp.xfer_send_to(&server, 5, b"x"), 10);
    let data = vec![1u8; 9];
    tcp.host.0.lock().unwrap().send_limit = Some(4);
    assert_eq!(tcp.xfer_send_to(&server, 0, &data), OK);
    assert_eq!(tcp.host.0.lock().unwrap().sent.iter().map(Vec::len).sum::<usize>(), 9);
    server.connections[0] = std::ptr::null_mut();
    assert_eq!(tcp.xfer_send_to(&server, 0, &data), 10, "a NULL slot");
    drop(unsafe { Box::from_raw(c) });
}

#[test]
fn xfer_send_to_fails_on_an_error() {
    let tcp = activated();
    let mut server = empty_server();
    let c = Box::into_raw(Box::new(stream_connection(11)));
    server.connections.push(c);
    {
        let mut s = tcp.host.0.lock().unwrap();
        s.send_limit = Some(-1);
        s.last_error = 33;
    }
    assert_eq!(tcp.xfer_send_to(&server, 0, b"abc"), NATIVE_ERROR);
    drop(unsafe { Box::from_raw(c) });
}

#[test]
fn reset_and_close_server_free_its_connections_once() {
    let tcp = activated();
    let mut server = Box::into_raw(Box::new(empty_server()));
    unsafe {
        (*server).listening = true;
        (*server).socket = 21;
        (*server).xfer = true;
        (*server).xfer_socket = 22;
        (*server).connections.push(Box::into_raw(Box::new(stream_connection(23))));
    }
    tcp.reset_xfer(unsafe { &mut *server });
    assert!(unsafe { (*server).connections.is_empty() });
    assert_eq!(tcp.close_server(&mut server), OK);
    assert!(server.is_null());
    let s = tcp.host.0.lock().unwrap();
    assert!(s.closed.contains(&21) && s.closed.contains(&22));
}

#[test]
fn close_server_keeps_the_server_when_a_close_fails() {
    let tcp = activated();
    let mut server = Box::into_raw(Box::new(empty_server()));
    unsafe {
        (*server).listening = true;
        (*server).socket = 21;
    }
    {
        let mut s = tcp.host.0.lock().unwrap();
        s.close_fails.push(21);
        s.last_error = 1;
    }
    assert_eq!(tcp.close_server(&mut server), NATIVE_ERROR);
    assert!(!server.is_null());
    drop(unsafe { Box::from_raw(server) });
}

#[test]
fn close_connection_codes() {
    let tcp = activated();
    let mut c = Box::into_raw(Box::new(stream_connection(31)));
    assert_eq!(tcp.close_connection(&mut c), OK);
    assert!(c.is_null());
    let closed_before = tcp.host.0.lock().unwrap().closed.len();
    tcp.host.0.lock().unwrap().last_error = 12;
    assert_eq!(tcp.close_connection(&mut c), NATIVE_ERROR, "a NULL slot");
    assert_eq!(tcp.host.0.lock().unwrap().closed.len(), closed_before + 1, "the listen socket closes anyway");
}

// ---- search

#[test]
fn query_games_collects_the_announcements() {
    let tcp = activated();
    let announcement = |ip: u32, host: &[u8]| {
        let mut p = vec![0u8; 0x88];
        p[1] = 0x16;
        p[2..2 + host.len()].copy_from_slice(host);
        p[0x43..0x47].copy_from_slice(b"addr");
        (0x88, pack_datagram(&tcp, 2, &p), SockAddr::new(net16(33334), ip))
    };
    {
        let mut s = tcp.host.0.lock().unwrap();
        s.recv_from.push_back(announcement(0x11, b"h1"));
        s.recv_from.push_back(announcement(0x11, b"dup"));
        s.recv_from.push_back(announcement(0x22, b"h2"));
    }
    assert_eq!(tcp.query_games(b"name"), OK);
    assert_eq!(tcp.detected_game_count(), 2);
    let g = tcp.detected_game(0).unwrap();
    assert_eq!(g.name, b"ip11");
    assert_eq!(g.host, b"h1");
    assert_eq!(g.address, b"addr");
    assert_eq!(g.datagram_address.address, 0x11);
    assert!(tcp.detected_game(5).is_none());
    // the query packet went out enciphered with its name
    let (packet, to) = tcp.host.0.lock().unwrap().sent_to[0].clone();
    assert_eq!(packet.len(), 0x43);
    assert_eq!(to.address, 0xffff_ffff);
    let mut body = packet[1..].to_vec();
    tcp_cipher_apply(&tcp, &mut body, packet[0]);
    assert_eq!(body[0], 0x15);
    assert_eq!(&body[1..5], b"name");
    assert!(tcp.host.0.lock().unwrap().sleeps > 10);
}

#[test]
fn query_games_with_an_empty_name_does_nothing() {
    let tcp = activated();
    assert_eq!(tcp.query_games(b""), OK);
    assert!(tcp.host.0.lock().unwrap().sent_to.is_empty());
}

#[test]
fn query_games_failures_record_and_clean_up() {
    let tcp = activated();
    tcp.host.0.lock().unwrap().open_fails = 1;
    assert_eq!(tcp.query_games(b"name"), NATIVE_ERROR);

    let tcp = activated();
    tcp.host.0.lock().unwrap().broadcast_ok = Some(false);
    assert_eq!(tcp.query_games(b"name"), NATIVE_ERROR);
    assert!(tcp.host.0.lock().unwrap().closed.len() >= 2, "the send socket is closed too");

    let tcp = activated();
    tcp.host.0.lock().unwrap().send_to_result = Some(-1);
    assert_eq!(tcp.query_games(b"name"), NATIVE_ERROR);
}

#[test]
fn search_thread_starts_and_stops() {
    let tcp = activated();
    assert_eq!(tcp.search_for_game(b"", true), OK, "an empty name is a no-op");
    assert_eq!(tcp.search_for_game(b"name", true), OK);
    // starting twice keeps the one thread
    assert_eq!(tcp.search_for_game(b"name", true), OK);
    std::thread::sleep(std::time::Duration::from_millis(5));
    assert_eq!(tcp.search_for_game(b"name", false), OK);
    assert_eq!(tcp.detected_game_count(), 0);
}

// ---- misc

#[test]
fn is_alive_cases() {
    let tcp = activated();
    assert!(tcp.is_alive(&datagram_connection(1, SockAddr::default())), "datagrams are always alive");
    let c = stream_connection(1);
    tcp.host.0.lock().unwrap().peek_result = 1;
    assert!(tcp.is_alive(&c));
    tcp.host.0.lock().unwrap().peek_result = 0;
    assert!(!tcp.is_alive(&c));
    {
        let mut s = tcp.host.0.lock().unwrap();
        s.peek_result = -1;
        s.last_error = WOULD_BLOCK_ERROR;
    }
    assert!(tcp.is_alive(&c));
    tcp.host.0.lock().unwrap().last_error = 7;
    assert!(!tcp.is_alive(&c));
}

#[test]
fn client_address_and_detected_games_bounds() {
    let tcp = activated();
    let mut server = empty_server();
    server.clients.push(SockAddr::new(net16(1), 0x77));
    assert_eq!(tcp.get_client_address(&server, 0), (OK, b"ip77".to_vec()));
    assert_eq!(tcp.get_client_address(&server, 1), (BAD_INDEX, Vec::new()));
}

#[test]
fn reset_client_clears_only_that_port() {
    let tcp = activated();
    let mut server = empty_server();
    server.clients.push(SockAddr::new(net16(1), 1));
    server.clients.push(SockAddr::new(net16(2), 2));
    tcp.reset_client_connection(&mut server, 1);
    assert_eq!(server.clients[0].port, net16(1));
    assert_eq!(server.clients[1].port, 0);
    tcp.reset_client_connection(&mut server, 5); // out of range: nothing
}

fn adapter(name: &[u8], address: u32) -> Adapter {
    Adapter { description: name.to_vec(), address, netmask: 0 }
}

#[test]
fn local_address_prefers_the_adapter_list_and_falls_back_to_a_udp_probe() {
    let tcp = activated();
    {
        let mut s = tcp.host.0.lock().unwrap();
        s.adapters = vec![adapter(b"first", 0x0a), adapter(b"second", 0x0b)];
        s.resolve = vec![0x0c, 0x0b];
    }
    // adapter 2 is the second address of the host's list
    assert_eq!(tcp.get_local_address(2), b"ipb".to_vec());
    // adapter 0: the first host address
    assert_eq!(tcp.get_local_address(0), b"ipc".to_vec());
    assert_eq!(tcp.enumerate_network_adapters(), vec![b"first".to_vec(), b"second".to_vec()]);
    // no host addresses: the UDP probe (get_name's address)
    tcp.host.0.lock().unwrap().resolve = Vec::new();
    assert_eq!(tcp.get_local_address(0), b"ip100007f".to_vec());
}

#[test]
fn enumerating_adapters_stops_at_ten() {
    let tcp = activated();
    tcp.host.0.lock().unwrap().adapters = (0..15).map(|i| adapter(format!("a{i}").as_bytes(), i + 1)).collect();
    assert_eq!(tcp.enumerate_network_adapters().len(), 10);
}

#[test]
fn send_single_uses_the_local_host_index_and_codes() {
    let tcp = activated();
    tcp.host.0.lock().unwrap().resolve = vec![0x21, 0x22];
    tcp.set_adapter_index(1);
    assert_eq!(tcp.send_single(b"remote", 1234, b"payload"), OK);
    let (data, to) = tcp.host.0.lock().unwrap().sent_to[0].clone();
    assert_eq!(data, b"payload");
    assert_eq!((to.address, to.port), (0x22, net16(1234)));

    // an index past the remote list's end: address 0
    tcp.set_adapter_index(9);
    assert_eq!(tcp.send_single(b"remote", 1, b"x"), OK);
    assert_eq!(tcp.host.0.lock().unwrap().sent_to[1].1.address, 0);

    // short send, error, no socket, unresolved, bind failure
    tcp.set_adapter_index(0);
    tcp.host.0.lock().unwrap().send_to_result = Some(1);
    assert_eq!(tcp.send_single(b"remote", 1, b"xxxx"), UNAVAILABLE);
    tcp.host.0.lock().unwrap().send_to_result = Some(-1);
    assert_eq!(tcp.send_single(b"remote", 1, b"xxxx"), NATIVE_ERROR);
    tcp.host.0.lock().unwrap().open_fails = 1;
    assert_eq!(tcp.send_single(b"remote", 1, b"xxxx"), OK, "as the C++: an open failure is OK");
    tcp.host.0.lock().unwrap().resolve = Vec::new();
    tcp.host.0.lock().unwrap().parse = INADDR_NONE;
    assert_eq!(tcp.send_single(b"remote", 1, b"xxxx"), UNAVAILABLE);
    let tcp2 = activated();
    {
        let mut s = tcp2.host.0.lock().unwrap();
        s.resolve = vec![1];
        s.bind_ok = Some(false);
    }
    assert_eq!(tcp2.send_single(b"remote", 1, b"xxxx"), NATIVE_ERROR);
}

#[test]
fn adapter_index_of_connect_comes_from_the_adapter_list() {
    let tcp = activated();
    {
        let mut s = tcp.host.0.lock().unwrap();
        s.adapters = vec![adapter(b"a", 0x0a)];
        s.resolve = vec![0x0b, 0x0a];
    }
    let mut out = std::ptr::null_mut();
    assert_eq!(tcp.connect(b"x", b"h", b"a", 2, null_event(), 1, &mut out), OK);
    let c = unsafe { Box::from_raw(out) };
    assert_eq!(c.state, 2);
    // the adapter index was 1 (the host's second address)
    tcp.host.0.lock().unwrap().resolve = vec![0x31, 0x32];
    assert_eq!(tcp.send_single(b"remote", 1, b"p"), OK);
    assert_eq!(tcp.host.0.lock().unwrap().sent_to.last().unwrap().1.address, 0x32);
}

#[test]
fn xfer_receive_codes() {
    let tcp = activated();
    let mut buffer = vec![0u8; 8];
    let mut received = 0u32;
    tcp.host.0.lock().unwrap().recvs.push_back(3);
    assert_eq!(tcp.xfer_receive(&mut buffer, &mut received), OK);
    assert_eq!(received, 3);
    tcp.host.0.lock().unwrap().recvs.push_back(0);
    assert_eq!(tcp.xfer_receive(&mut buffer, &mut received), UNAVAILABLE);
    {
        let mut s = tcp.host.0.lock().unwrap();
        s.recvs.push_back(-1);
        s.last_error = WOULD_BLOCK_ERROR;
    }
    assert_eq!(tcp.xfer_receive(&mut buffer, &mut received), WOULD_BLOCK);
    assert_eq!(received, 0);
    {
        let mut s = tcp.host.0.lock().unwrap();
        s.recvs.push_back(-1);
        s.last_error = 4;
    }
    assert_eq!(tcp.xfer_receive(&mut buffer, &mut received), NATIVE_ERROR);
}
