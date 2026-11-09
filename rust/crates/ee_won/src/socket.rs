//! AsyncSocket's core (`src/Empire Earth/WONAPI/WONSocket/AsyncSocket.cpp`): WON's non-blocking TCP / UDP socket on
//! the platform layer's sockets (`ee_plat_sys`), with WON's statuses: a would-block is `WS_TimedOut`, a short stream
//! read or write is `WS_TimedOut`, a short datagram write `WS_AsyncSocket_PartialSendTo`, a closed peer
//! `WS_AsyncSocket_Shutdown`, any other failure the socket error in Winsock's numbering (kept as the socket's
//! status).
//!
//! The C++ `AsyncSocket` embeds a [`Socket`] (its handle, type, flags, status and times) and keeps what is C++'s: the
//! lock (`mDataCrit`: callers of the `&mut self` functions hold it, as the C++ did), the virtual open / close / kill
//! hooks, the destination `IPAddr` and the message transforms.

use crate::ipaddr::Addr;
use crate::netstats;
use crate::status::*;
use ee_plat_sys::constants::{
    PlatSockAddrIn, PlatSocket, PLAT_AF_INET, PLAT_SOCKET_DGRAM, PLAT_SOCKET_INVALID, PLAT_SOCKET_SHUT_SEND,
    PLAT_SOCKET_STREAM, PLAT_SOCKET_WAIT_EXCEPT, PLAT_SOCKET_WAIT_READ, PLAT_SOCKET_WAIT_WRITE, PLAT_SOCK_EINPROGRESS,
    PLAT_SOCK_EWOULDBLOCK,
};
use ee_plat_sys::socket::imp as plat;
use std::ffi::{c_long, c_void};
use std::sync::Mutex;

/// `AsyncSocket::TCP`.
/// cbindgen:ignore
pub const KIND_STREAM: i32 = 0;
/// `AsyncSocket::UDP`.
/// cbindgen:ignore
pub const KIND_DATAGRAM: i32 = 1;

/// Winsock's WSASYSNOTREADY: the socket system did not start.
const SYSNOTREADY: Status = 10091;

/// WON's socket state, embedded in the C++ `AsyncSocket` (`mCore`).
#[repr(C)]
#[derive(Debug)]
pub struct Socket {
    /// The platform socket (`PLAT_SOCKET_INVALID` when closed).
    pub handle: isize,
    /// [`KIND_STREAM`] or [`KIND_DATAGRAM`].
    pub kind: i32,
    /// Connected (a stream connect was started or accepted; a datagram socket's peer was set).
    pub connected: bool,
    /// Opened since the last close.
    pub open: bool,
    /// The last socket error recorded (`WS_None` after an open).
    pub status: c_long,
    /// When data last arrived (seconds since 1970; the original's 32-bit `time(NULL)`).
    pub last_recv_time: i64,
    /// When the first of the current run of blocked sends failed (0: the last send went out).
    pub first_blocked_send_time: i64,
}

fn now_seconds() -> i64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs() as i64).unwrap_or(0)
}

fn sock_addr(addr: Addr) -> PlatSockAddrIn {
    PlatSockAddrIn { mFamily: PLAT_AF_INET, mPort: addr.port, mAddress: addr.address, mZero: [0; 8] }
}

fn addr_of(a: &PlatSockAddrIn) -> Addr {
    Addr { address: a.mAddress, port: a.mPort }
}

static START_COUNT: Mutex<i32> = Mutex::new(0);

/// `AsyncSocket::StartWinsock`: starts the socket system on the first use and counts its users; the error
/// (WSASYSNOTREADY when it does not start) or 0.
pub fn start() -> Status {
    let mut n = START_COUNT.lock().unwrap_or_else(|e| e.into_inner());
    if *n != 0 {
        *n += 1;
        return WS_SUCCESS;
    }
    // SAFETY: no arguments
    if unsafe { plat::PlatSocketStartup() } == 0 {
        return SYSNOTREADY;
    }
    *n += 1;
    WS_SUCCESS
}

/// `AsyncSocket::StopWinsock`: releases one user; the last one cleans the socket system up (its error or 0).
pub fn stop() -> Status {
    let mut n = START_COUNT.lock().unwrap_or_else(|e| e.into_inner());
    if *n <= 0 {
        return WS_SUCCESS;
    }
    *n -= 1;
    if *n != 0 {
        return WS_SUCCESS;
    }
    // SAFETY: no arguments
    if unsafe { plat::PlatSocketCleanup() } != 0 {
        WS_SUCCESS
    } else {
        // SAFETY: no arguments
        unsafe { plat::PlatSocketLastError() as Status }
    }
}

/// `WaitForRead` / `WaitForWrite`'s wait on a handle (the lock is not held): up to `timeout_ms` for `mask`; nothing
/// for a zero time or a closed socket.
pub fn wait(handle: PlatSocket, mask: u32, timeout_ms: u32) {
    if timeout_ms == 0 || handle == PLAT_SOCKET_INVALID {
        return;
    }
    // SAFETY: a socket handle (a stale one fails the wait, as in the C++)
    unsafe { plat::PlatSocketWait(handle, mask, timeout_ms) };
}

/// `WaitForConnect`'s wait on a handle (the lock is not held): `WS_Success` when the connect finished, the connect's
/// error (or `WS_AsyncSocket_ConnectFailed` when the system gives none) when it failed, else `WS_TimedOut`.
pub fn wait_for_connect(handle: PlatSocket, timeout_ms: u32) -> Status {
    if handle == PLAT_SOCKET_INVALID {
        return WS_ASYNC_SOCKET_INVALID_SOCKET;
    }
    // SAFETY: a socket handle
    let ready = unsafe { plat::PlatSocketWait(handle, PLAT_SOCKET_WAIT_WRITE | PLAT_SOCKET_WAIT_EXCEPT, timeout_ms) };
    if ready > 0 && (ready as u32 & PLAT_SOCKET_WAIT_EXCEPT) != 0 {
        // SAFETY: a socket handle
        let error = unsafe { plat::PlatSocketGetError(handle) } as Status;
        return if error == 0 { WS_ASYNC_SOCKET_CONNECT_FAILED } else { error };
    }
    if ready > 0 {
        WS_SUCCESS
    } else {
        WS_TIMED_OUT
    }
}

/// The read mask for [`wait`].
/// cbindgen:ignore
pub const WAIT_READ: u32 = PLAT_SOCKET_WAIT_READ;
/// The write mask for [`wait`].
/// cbindgen:ignore
pub const WAIT_WRITE: u32 = PLAT_SOCKET_WAIT_WRITE;

impl Socket {
    /// A closed socket of `kind` (the constructor); counts it.
    pub fn new(kind: i32) -> Socket {
        netstats::add_constructed_sockets(1);
        Socket {
            handle: PLAT_SOCKET_INVALID,
            kind,
            connected: false,
            open: false,
            status: WS_NONE as c_long,
            last_recv_time: now_seconds(),
            first_blocked_send_time: 0,
        }
    }

    /// The destructor's count.
    pub fn destroyed(&mut self) {
        netstats::add_constructed_sockets(-1);
    }

    pub fn is_valid(&self) -> bool {
        self.handle != PLAT_SOCKET_INVALID
    }

    /// `GetLastSocketError`: the calling thread's socket error, `WS_TimedOut` for would-block / in progress; any
    /// other error is recorded as the socket's status.
    pub fn last_error(&mut self) -> Status {
        // SAFETY: no arguments
        let error = unsafe { plat::PlatSocketLastError() };
        if error == PLAT_SOCK_EWOULDBLOCK || error == PLAT_SOCK_EINPROGRESS {
            return WS_TIMED_OUT;
        }
        self.status = error as c_long;
        error as Status
    }

    /// `Open` after its close of a valid socket and before its open hook: a fresh non-blocking handle of the
    /// socket's kind (no linger for a stream, broadcast for a datagram). 0 or the error.
    pub fn open_handle(&mut self) -> Status {
        self.open = true;
        match self.kind {
            // SAFETY: plain calls
            KIND_STREAM => self.handle = unsafe { plat::PlatSocketOpen(PLAT_SOCKET_STREAM) },
            KIND_DATAGRAM => self.handle = unsafe { plat::PlatSocketOpen(PLAT_SOCKET_DGRAM) },
            _ => {}
        }
        if !self.is_valid() {
            return self.last_error();
        }
        self.status = WS_NONE as c_long;
        netstats::add_open_sockets(1);
        // SAFETY: an open handle
        unsafe {
            if self.kind == KIND_STREAM && plat::PlatSocketSetLinger(self.handle, 0, 0) == 0 {
                return self.last_error();
            }
            if self.kind == KIND_DATAGRAM && plat::PlatSocketSetBroadcast(self.handle, 1) == 0 {
                return self.last_error();
            }
            if plat::PlatSocketSetNonBlocking(self.handle, 1) == 0 {
                return self.last_error();
            }
        }
        WS_SUCCESS
    }

    /// `Bind` on an open socket: address reuse when asked, then the bind. 0 or the error.
    pub fn bind(&mut self, addr: Addr, reuse: bool) -> Status {
        // SAFETY: the socket's handle and a local address
        unsafe {
            if reuse && plat::PlatSocketSetReuseAddress(self.handle, 1) == 0 {
                return self.last_error();
            }
            if plat::PlatSocketBind(self.handle, &sock_addr(addr)) == 0 {
                return self.last_error();
            }
        }
        WS_SUCCESS
    }

    /// `Fn2B0E60` (connect) on an open socket: marks it connected and starts the connect. `WS_Success`,
    /// `WS_TimedOut` while a non-blocking connect is in progress, or the error.
    pub fn connect(&mut self, addr: Addr) -> Status {
        self.connected = true;
        // SAFETY: the socket's handle and an address
        if unsafe { plat::PlatSocketConnect(self.handle, &sock_addr(addr)) } == 0 {
            return self.last_error();
        }
        WS_SUCCESS
    }

    /// `Listen`.
    pub fn listen(&mut self, backlog: i32) -> Status {
        if !self.is_valid() {
            return WS_ASYNC_SOCKET_INVALID_SOCKET;
        }
        if self.kind == KIND_DATAGRAM {
            return WS_ASYNC_SOCKET_DATAGRAM_NOT_ALLOWED;
        }
        // SAFETY: the socket's handle
        if unsafe { plat::PlatSocketListen(self.handle, backlog) } == 0 {
            return self.last_error();
        }
        WS_SUCCESS
    }

    /// `Accept`'s system part: a pending connection's handle (no linger, non-blocking) and the peer.
    pub fn accept(&mut self) -> Result<(PlatSocket, Addr), Status> {
        let mut from = PlatSockAddrIn::default();
        // SAFETY: the socket's handle and room for the peer
        let handle = unsafe { plat::PlatSocketAccept(self.handle, &mut from) };
        if handle == PLAT_SOCKET_INVALID {
            return Err(self.last_error());
        }
        // SAFETY: a new handle
        unsafe {
            plat::PlatSocketSetLinger(handle, 0, 0);
            plat::PlatSocketSetNonBlocking(handle, 1);
        }
        Ok((handle, addr_of(&from)))
    }

    /// The new socket of an accept takes the accepted handle: open and connected; counts it.
    pub fn adopt(&mut self, handle: PlatSocket) {
        self.handle = handle;
        self.open = true;
        self.connected = true;
        netstats::add_open_sockets(1);
    }

    /// `Send`: the bytes the system takes (`*sent`); `WS_TimedOut` on a short stream send or a would-block (the
    /// first blocked send's time is kept), `WS_AsyncSocket_PartialSendTo` on a short datagram.
    pub fn send(&mut self, data: &[u8], sent: &mut i32) -> Status {
        if !self.is_valid() {
            return WS_ASYNC_SOCKET_INVALID_SOCKET;
        }
        // SAFETY: the socket's handle and the caller's bytes
        let n = unsafe { plat::PlatSocketSend(self.handle, data.as_ptr().cast(), data.len() as u32) };
        if n < 0 {
            if self.first_blocked_send_time == 0 {
                self.first_blocked_send_time = now_seconds();
            }
            return self.last_error();
        }
        self.first_blocked_send_time = 0;
        netstats::add_bytes_sent(n as i64);
        *sent = n;
        if (n as usize) < data.len() {
            if self.kind == KIND_DATAGRAM {
                return WS_ASYNC_SOCKET_PARTIAL_SEND_TO;
            }
            return WS_TIMED_OUT;
        }
        WS_SUCCESS
    }

    /// `Recv`: the bytes received (`*received`); `WS_AsyncSocket_Shutdown` when the peer closed, `WS_TimedOut` on a
    /// short stream read or a would-block.
    pub fn recv(&mut self, buffer: &mut [u8], received: &mut i32) -> Status {
        if !self.is_valid() {
            return WS_ASYNC_SOCKET_INVALID_SOCKET;
        }
        // SAFETY: the socket's handle and the caller's buffer
        let n = unsafe { plat::PlatSocketRecv(self.handle, buffer.as_mut_ptr().cast(), buffer.len() as u32) };
        if n < 0 {
            return self.last_error();
        }
        if n == 0 {
            return WS_ASYNC_SOCKET_SHUTDOWN;
        }
        netstats::add_bytes_received(n as i64);
        self.last_recv_time = now_seconds();
        *received = n;
        if n as usize != buffer.len() && self.kind == KIND_STREAM {
            return WS_TIMED_OUT;
        }
        WS_SUCCESS
    }

    /// `SendTo`'s and `RecvFrom`'s first test: a stream socket may not.
    pub fn datagram_only(&self) -> Status {
        if self.kind == KIND_STREAM {
            WS_ASYNC_SOCKET_STREAM_NOT_ALLOWED
        } else {
            WS_SUCCESS
        }
    }

    /// `SendTo` on an open datagram socket: `WS_AsyncSocket_PartialSendTo` when not all went.
    pub fn send_to(&mut self, data: &[u8], to: Addr) -> Status {
        // SAFETY: the socket's handle, the caller's bytes and an address
        let n = unsafe { plat::PlatSocketSendTo(self.handle, data.as_ptr().cast(), data.len() as u32, &sock_addr(to)) };
        if n < 0 {
            return self.last_error();
        }
        netstats::add_bytes_sent(n as i64);
        if n as usize == data.len() {
            WS_SUCCESS
        } else {
            WS_ASYNC_SOCKET_PARTIAL_SEND_TO
        }
    }

    /// `RecvFrom`: one datagram and its sender (`*from` set on success).
    pub fn recv_from(&mut self, buffer: &mut [u8], from: &mut Option<Addr>, received: &mut i32) -> Status {
        let s = self.datagram_only();
        if s != WS_SUCCESS {
            return s;
        }
        if !self.is_valid() {
            return WS_ASYNC_SOCKET_INVALID_SOCKET;
        }
        let mut peer = PlatSockAddrIn::default();
        // SAFETY: the socket's handle, the caller's buffer and room for the sender
        let n = unsafe {
            plat::PlatSocketRecvFrom(self.handle, buffer.as_mut_ptr().cast(), buffer.len() as u32, &mut peer)
        };
        if n >= 0 {
            netstats::add_bytes_received(n as i64);
            *from = Some(addr_of(&peer));
            *received = n;
            return WS_SUCCESS;
        }
        self.last_error()
    }

    /// `ClosePrv`: shuts the sending side down and closes the handle (on Windows a close that would block is made
    /// abortive and repeated); the socket is closed and not connected.
    pub fn close_handle(&mut self) {
        if self.is_valid() {
            // SAFETY: the socket's handle, closed once
            unsafe {
                plat::PlatSocketShutdown(self.handle, PLAT_SOCKET_SHUT_SEND);
                // a failed closesocket keeps the Winsock handle; a POSIX close always frees the descriptor
                if plat::PlatSocketClose(self.handle) == 0 && cfg!(windows) && self.last_error() == WS_TIMED_OUT {
                    plat::PlatSocketSetLinger(self.handle, 1, 0);
                    plat::PlatSocketClose(self.handle);
                }
            }
            self.handle = PLAT_SOCKET_INVALID;
            netstats::add_open_sockets(-1);
        }
        self.connected = false;
        self.open = false;
    }

    /// `Disconnect` of a datagram socket: connecting to the null address dissolves its peer; false when that
    /// failed (the C++ then closes the socket).
    pub fn dissolve_peer(&mut self) -> bool {
        let none = PlatSockAddrIn::default();
        // SAFETY: the socket's handle (a closed one fails)
        unsafe { plat::PlatSocketConnect(self.handle, &none) != 0 }
    }

    /// `GetLocalPort`: the bound port in host order, 0 for a closed or unbound socket.
    pub fn local_port(&self) -> u16 {
        if !self.is_valid() {
            return 0;
        }
        let mut a = PlatSockAddrIn::default();
        // SAFETY: the socket's handle and room for the address
        if unsafe { plat::PlatSocketGetName(self.handle, &mut a) } == 0 {
            return 0;
        }
        u16::from_be(a.mPort)
    }

    /// `SetBroadcast`.
    pub fn set_broadcast(&mut self, on: bool) -> Status {
        // SAFETY: the socket's handle
        if unsafe { plat::PlatSocketSetBroadcast(self.handle, on as i32) } == 0 {
            return self.last_error();
        }
        WS_SUCCESS
    }

    /// `SetNagling`: Nagle's algorithm on or off (TCP_NODELAY off or on).
    pub fn set_nagling(&mut self, on: bool) -> Status {
        // SAFETY: the socket's handle
        if unsafe { plat::PlatSocketSetNoDelay(self.handle, (!on) as i32) } == 0 {
            return self.last_error();
        }
        WS_SUCCESS
    }
}

/// The bytes of a raw (pointer, length) pair; empty for a null pointer or a length of 0 or less.
///
/// # Safety
/// `data` holds `length` bytes when `length > 0`.
pub unsafe fn raw<'a>(data: *const c_void, length: i32) -> &'a [u8] {
    if data.is_null() || length <= 0 {
        &[]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(data.cast(), length as usize) }
    }
}

/// As [`raw`], writable.
///
/// # Safety
/// `data` has room for `length` bytes when `length > 0`.
pub unsafe fn raw_mut<'a>(data: *mut c_void, length: i32) -> &'a mut [u8] {
    if data.is_null() || length <= 0 {
        &mut []
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts_mut(data.cast(), length as usize) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{Duration, Instant};

    fn loopback(port: u16) -> Addr {
        Addr::from_host(0x7f00_0001, port)
    }

    fn retry(mut f: impl FnMut() -> Status, handle: PlatSocket, mask: u32) -> Status {
        let end = Instant::now() + Duration::from_secs(2);
        loop {
            let s = f();
            if s != WS_TIMED_OUT || Instant::now() > end {
                return s;
            }
            wait(handle, mask, 50);
        }
    }

    fn stream_pair() -> (Socket, Socket, Socket) {
        let mut listener = Socket::new(KIND_STREAM);
        assert_eq!(listener.open_handle(), WS_SUCCESS);
        assert_eq!(listener.bind(loopback(0), true), WS_SUCCESS);
        let port = listener.local_port();
        assert_ne!(port, 0);
        assert_eq!(listener.listen(5), WS_SUCCESS);
        let mut client = Socket::new(KIND_STREAM);
        assert_eq!(client.open_handle(), WS_SUCCESS);
        let s = client.connect(loopback(port));
        assert!(s == WS_SUCCESS || s == WS_TIMED_OUT, "{s}");
        assert!(client.connected);
        assert_eq!(wait_for_connect(client.handle, 2000), WS_SUCCESS);
        assert_eq!(client.set_nagling(false), WS_SUCCESS);
        wait(listener.handle, WAIT_READ, 2000);
        let (handle, peer) = listener.accept().unwrap();
        assert_eq!(peer.host(), 0x7f00_0001);
        assert_eq!(peer.host_port(), client.local_port());
        let mut server = Socket::new(KIND_STREAM);
        server.adopt(handle);
        assert!(server.open && server.connected);
        (listener, client, server)
    }

    #[test]
    fn start_and_stop_count_users() {
        assert_eq!(start(), WS_SUCCESS);
        assert_eq!(start(), WS_SUCCESS);
        assert_eq!(stop(), WS_SUCCESS);
        assert_eq!(stop(), WS_SUCCESS);
        // one more stop than starts is harmless; leave the system started for the other tests
        assert_eq!(stop(), WS_SUCCESS);
        assert_eq!(start(), WS_SUCCESS);
    }

    #[test]
    fn stream_exchange_shutdown_and_refusal() {
        start();
        let (mut listener, mut client, mut server) = stream_pair();
        let mut buffer = [0u8; 64];
        let mut n = -1;
        // nothing queued: would block
        assert_eq!(server.recv(&mut buffer, &mut n), WS_TIMED_OUT);
        assert_eq!(n, -1);
        let message = b"hello, WON\0";
        let mut sent = 0;
        let ch = client.handle;
        assert_eq!(retry(|| client.send(message, &mut sent), ch, WAIT_WRITE), WS_SUCCESS);
        assert_eq!(sent, message.len() as i32);
        assert_eq!(client.first_blocked_send_time, 0);
        let h = server.handle;
        let mut got = 0;
        assert_eq!(retry(|| server.recv(&mut buffer[..message.len()], &mut got), h, WAIT_READ), WS_SUCCESS);
        assert_eq!(&buffer[..message.len()], message);
        // a read asking for more than is there is short: WS_TimedOut with the count
        assert_eq!(retry(|| client.send(b"ab", &mut sent), ch, WAIT_WRITE), WS_SUCCESS);
        wait(h, WAIT_READ, 1000);
        std::thread::sleep(Duration::from_millis(20));
        assert_eq!(server.recv(&mut buffer, &mut got), WS_TIMED_OUT);
        assert_eq!(got, 2);
        // datagram calls on a stream socket
        let mut from = None;
        assert_eq!(server.datagram_only(), WS_ASYNC_SOCKET_STREAM_NOT_ALLOWED);
        assert_eq!(server.recv_from(&mut buffer, &mut from, &mut got), WS_ASYNC_SOCKET_STREAM_NOT_ALLOWED);
        // the peer closes
        client.close_handle();
        assert!(!client.is_valid() && !client.connected && !client.open);
        assert_eq!(client.send(b"x", &mut sent), WS_ASYNC_SOCKET_INVALID_SOCKET);
        assert_eq!(client.local_port(), 0);
        assert_eq!(retry(|| server.recv(&mut buffer, &mut got), h, WAIT_READ), WS_ASYNC_SOCKET_SHUTDOWN);
        // a port nobody listens on
        let port = listener.local_port();
        listener.close_handle();
        assert_eq!(listener.listen(1), WS_ASYNC_SOCKET_INVALID_SOCKET);
        let mut refused = Socket::new(KIND_STREAM);
        assert_eq!(refused.open_handle(), WS_SUCCESS);
        let mut s = refused.connect(loopback(port));
        if s == WS_TIMED_OUT || s == WS_SUCCESS {
            s = wait_for_connect(refused.handle, 3000);
        }
        assert_eq!(s, 10061);
        assert_eq!(wait_for_connect(PLAT_SOCKET_INVALID, 10), WS_ASYNC_SOCKET_INVALID_SOCKET);
        for x in [&mut refused, &mut server] {
            x.close_handle();
            x.destroyed();
        }
        client.destroyed();
        listener.destroyed();
    }

    #[test]
    fn datagrams() {
        start();
        let mut receiver = Socket::new(KIND_DATAGRAM);
        assert_eq!(receiver.open_handle(), WS_SUCCESS);
        assert_eq!(receiver.bind(loopback(0), false), WS_SUCCESS);
        let port = receiver.local_port();
        let mut sender = Socket::new(KIND_DATAGRAM);
        assert_eq!(sender.open_handle(), WS_SUCCESS);
        assert_eq!(sender.bind(loopback(0), false), WS_SUCCESS);
        assert_eq!(sender.datagram_only(), WS_SUCCESS);
        assert_eq!(sender.send_to(b"datagram\0", loopback(port)), WS_SUCCESS);
        let mut buffer = [0u8; 64];
        let mut from = None;
        let mut n = 0;
        let h = receiver.handle;
        assert_eq!(retry(|| receiver.recv_from(&mut buffer, &mut from, &mut n), h, WAIT_READ), WS_SUCCESS);
        assert_eq!(&buffer[..n as usize], b"datagram\0");
        assert_eq!(from.unwrap(), loopback(sender.local_port()));
        // a datagram socket with no peer cannot send
        let mut sent = 0;
        assert_ne!(receiver.send(b"d", &mut sent), WS_SUCCESS);
        assert_eq!(receiver.listen(1), WS_ASYNC_SOCKET_DATAGRAM_NOT_ALLOWED);
        assert_eq!(receiver.set_broadcast(false), WS_SUCCESS);
        // a peer set and dissolved
        assert_eq!(sender.connect(loopback(port)), WS_SUCCESS);
        assert_eq!(sender.send(b"peer", &mut sent), WS_SUCCESS);
        assert!(sender.dissolve_peer() || cfg!(not(windows)));
        sender.close_handle();
        assert!(!sender.dissolve_peer());
        receiver.close_handle();
        let mut closed = Socket::new(KIND_DATAGRAM);
        assert_eq!(closed.recv_from(&mut buffer, &mut from, &mut n), WS_ASYNC_SOCKET_INVALID_SOCKET);
    }

    #[test]
    fn a_kind_without_a_socket_fails_to_open() {
        start();
        let mut odd = Socket::new(7);
        let s = odd.open_handle();
        assert!(odd.open && !odd.is_valid());
        let _ = s;
    }
}
