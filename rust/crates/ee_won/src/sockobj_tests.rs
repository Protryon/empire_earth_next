//! `EeWonAsyncSocket` over 127.0.0.1 with recording hooks: the hooks' order, the transforms' chains, the steps'
//! statuses (the lines `golden_sockobj.txt` shows for the C++ class).

use super::*;
use std::sync::Mutex;

static LOG: Mutex<Vec<String>> = Mutex::new(Vec::new());

fn log(s: String) {
    LOG.lock().unwrap().push(s);
}

fn take() -> Vec<String> {
    std::mem::take(&mut *LOG.lock().unwrap())
}

// the "C++ socket" is a leaked name; a transform is a (tag, refuse) pair; a message is a Vec<u8>
unsafe extern "C" fn open_hook(s: *mut c_void) {
    log(format!("{} open", unsafe { &*(s as *const String) }));
}
unsafe extern "C" fn close_hook(s: *mut c_void) {
    log(format!("{} close", unsafe { &*(s as *const String) }));
}
unsafe extern "C" fn kill_hook(s: *mut c_void) {
    log(format!("{} kill", unsafe { &*(s as *const String) }));
}
struct Tag(u8, c_long);
unsafe extern "C" fn t_send(t: *mut c_void, m: *mut c_void) -> c_long {
    let t = unsafe { &*(t as *const Tag) };
    let m = unsafe { &mut *(m as *mut Vec<u8>) };
    log(format!("send {} {:?}", t.0 as char, String::from_utf8_lossy(m)));
    if t.1 != 0 {
        return t.1;
    }
    m.push(t.0);
    0
}
unsafe extern "C" fn t_recv(t: *mut c_void, m: *mut c_void) -> c_long {
    let t = unsafe { &*(t as *const Tag) };
    let m = unsafe { &mut *(m as *mut Vec<u8>) };
    log(format!("recv {} {:?}", t.0 as char, String::from_utf8_lossy(m)));
    if t.1 != 0 {
        return t.1;
    }
    if m.last() != Some(&t.0) {
        return WS_RECV_MSG_INVALID_MESSAGE_LENGTH as c_long;
    }
    m.pop();
    0
}
unsafe extern "C" fn t_free(t: *mut c_void) {
    let t = unsafe { Box::from_raw(t as *mut Tag) };
    log(format!("free {}", t.0 as char));
}

static HOOKS: EeWonAsyncSocketHooks = EeWonAsyncSocketHooks {
    open_hook: Some(open_hook),
    close_hook: Some(close_hook),
    kill_hook: Some(kill_hook),
    transform_send: Some(t_send),
    transform_recv: Some(t_recv),
    transform_free: Some(t_free),
};

fn new_socket(name: &str, kind: i32) -> EeWonAsyncSocket {
    let n: &'static String = Box::leak(Box::new(name.to_string()));
    // SAFETY: a plain platform call; the mutex is leaked with the test
    let lock = unsafe { plat::PlatMutexCreate() };
    EeWonAsyncSocket::new(n as *const String as *mut c_void, &HOOKS, lock, kind)
}

fn local(port: u16) -> Addr {
    Addr::from_host(0x7f00_0001, port)
}

// one test: the hook log is shared
#[test]
fn a_socket_object_over_loopback() {
    socket::start();
    take();
    let a = new_socket("a", KIND_STREAM);
    assert!(!a.is_valid() && !a.is_open() && a.length_field_size() == 4 && a.local_port() == 0);
    assert_eq!(a.open(), WS_SUCCESS);
    assert_eq!(take(), ["a open"]);
    // an open socket is closed before it opens again
    assert_eq!(a.open(), WS_SUCCESS);
    assert_eq!(take(), ["a close", "a open"]);
    assert_eq!(a.bind(local(0), true), WS_SUCCESS);
    assert_eq!(a.listen(5), WS_SUCCESS);
    let port = a.local_port();
    assert_ne!(port, 0);

    let b = new_socket("b", KIND_STREAM);
    assert_eq!(b.connect_prepare(0), WS_SUCCESS);
    assert_eq!(take(), ["b open"]);
    let s = b.connect(local(port));
    assert!(s == WS_SUCCESS || s == WS_TIMED_OUT, "{s}");
    assert_eq!(b.wait_for_connect(5000), WS_SUCCESS);
    assert!(b.is_connected());
    a.wait(socket::WAIT_READ, 5000);
    let (h, peer) = a.accept().expect("accept");
    assert_eq!(peer.host(), 0x7f00_0001);
    assert_eq!(peer.host_port(), b.local_port());
    let c = new_socket("c", KIND_STREAM);
    c.adopt(h);
    assert!(c.is_valid() && c.is_open() && c.is_connected());
    assert_eq!(a.accept().err(), Some(WS_TIMED_OUT));

    // bytes
    let mut n = -1;
    assert_eq!(b.send(b"hello", &mut n), WS_SUCCESS);
    assert_eq!(n, 5);
    c.wait(socket::WAIT_READ, 5000);
    let mut buf = [0u8; 3];
    assert_eq!(c.recv(&mut buf, &mut n), WS_SUCCESS);
    assert_eq!(&buf, b"hel");
    let mut buf = [0u8; 10];
    assert_eq!(c.recv(&mut buf, &mut n), WS_TIMED_OUT);
    assert_eq!(&buf[..n as usize], b"lo");
    assert_eq!(c.send_to(b"x", local(9)), WS_ASYNC_SOCKET_STREAM_NOT_ALLOWED);

    // the transforms: in order on send, reversed on receive, the first refusal ends the chain
    b.add_transform(Box::into_raw(Box::new(Tag(b'x', 0))).cast());
    b.add_transform(Box::into_raw(Box::new(Tag(b'y', 0))).cast());
    let mut m = b"m".to_vec();
    assert_eq!(unsafe { b.transform_send((&mut m as *mut Vec<u8>).cast()) }, WS_SUCCESS);
    assert_eq!(m, b"mxy");
    assert_eq!(unsafe { b.transform_recv((&mut m as *mut Vec<u8>).cast()) }, WS_SUCCESS);
    assert_eq!(m, b"m");
    assert_eq!(take(), ["send x \"m\"", "send y \"mx\"", "recv y \"mxy\"", "recv x \"mx\""]);
    let mut m = b"nx".to_vec();
    assert_eq!(unsafe { b.transform_recv((&mut m as *mut Vec<u8>).cast()) }, WS_RECV_MSG_INVALID_MESSAGE_LENGTH);
    assert_eq!(take(), ["recv y \"nx\""]);
    b.remove_all_transforms();
    assert_eq!(take(), ["free x", "free y"]);

    // a type change closes; the same type does not
    b.set_type(KIND_STREAM);
    assert!(take().is_empty());
    b.set_type(socket::KIND_DATAGRAM);
    assert_eq!(take(), ["b close"]);
    assert!(!b.is_valid() && b.kind() == socket::KIND_DATAGRAM);
    // and back: closed again (the hook runs on a closed socket too)
    b.set_type(KIND_STREAM);
    assert_eq!(take(), ["b close"]);

    c.close();
    a.kill();
    assert_eq!(take(), ["c close", "a close", "a kill"]);
    drop(c);
    assert_eq!(take(), ["c close"]);

    // datagrams
    let u = new_socket("u", socket::KIND_DATAGRAM);
    let v = new_socket("v", socket::KIND_DATAGRAM);
    assert_eq!(u.send_to(b"ping", local(9)), WS_SUCCESS);
    assert_eq!(take(), ["u open"]);
    assert_eq!(v.bind(local(0), false), WS_SUCCESS);
    assert_eq!(u.send_to(b"ping", local(v.local_port())), WS_SUCCESS);
    v.wait(socket::WAIT_READ, 5000);
    let mut from = None;
    let mut buf = [0u8; 64];
    assert_eq!(v.recv_from(&mut buf, &mut from, &mut n), WS_SUCCESS);
    assert_eq!((&buf[..n as usize], from.map(|a| a.host_port())), (&b"ping"[..], Some(u.local_port())));
    assert_eq!(v.recv_from(&mut buf, &mut from, &mut n), WS_TIMED_OUT);
    assert_eq!(u.listen(1), WS_ASYNC_SOCKET_DATAGRAM_NOT_ALLOWED);
    // a stream socket's disconnect closes it
    take();
    b.disconnect();
    assert_eq!(take(), ["b close"]);
    drop((u, v, a, b));
    socket::stop();
}

#[test]
fn the_close_callback_runs_before_the_hooks() {
    static CALLS: Mutex<Vec<usize>> = Mutex::new(Vec::new());
    unsafe extern "C" fn cb(s: *mut c_void) {
        CALLS.lock().unwrap().push(s as usize);
    }
    static QUIET: EeWonAsyncSocketHooks = EeWonAsyncSocketHooks {
        open_hook: None,
        close_hook: None,
        kill_hook: None,
        transform_send: None,
        transform_recv: None,
        transform_free: None,
    };
    // SAFETY: a plain platform call
    let lock = unsafe { plat::PlatMutexCreate() };
    let s = EeWonAsyncSocket::new(42 as *mut c_void, &QUIET, lock, KIND_STREAM);
    s.set_close_callback(cb as *const () as usize);
    s.close();
    s.kill();
    assert_eq!(*CALLS.lock().unwrap(), [42, 42]);
    s.set_close_callback(0);
    drop(s);
    assert_eq!(CALLS.lock().unwrap().len(), 2);
    // no transforms set: the chains pass
    let t = EeWonAsyncSocket::new(std::ptr::null_mut(), &QUIET, lock, KIND_STREAM);
    assert_eq!(unsafe { t.transform_send(std::ptr::null_mut()) }, WS_SUCCESS);
    assert_eq!(unsafe { t.transform_recv(std::ptr::null_mut()) }, WS_SUCCESS);
}
