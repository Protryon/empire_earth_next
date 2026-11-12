//! WONSocket's `AsyncSocket` as a Rust object (round 14): the socket's core ([`Socket`]: its handle, type, flags,
//! status and times), the length prefix's size, the message transforms (the C++ `MsgTransform` objects, owned), the
//! socket thread's close callback, and every step of `AsyncSocket.cpp`: open, bind, connect, listen, accept, the
//! sends and receives, the datagram calls, disconnect, close, kill, the waits, the type change and the transform
//! chains. The C++ `AsyncSocket` is a shim holding an [`EeWonAsyncSocket`]; its virtual `OpenHook`, `CloseHook` and
//! `KillHook` and the transforms' `Send`, `Recv` and deletion come back to it as the [`EeWonAsyncSocketHooks`].
//!
//! Since round 15 the object also owns the lock (the original's `mDataCrit`, a recursive platform mutex), which the
//! subclasses (QueueSocket, the routing and server connections) take around their own state as well through
//! [`ee_won_asyncsocket_mutex`], and the destination address (`mDestAddr`, an [`EeWonIpAddr`] read and written under
//! the lock). This object takes the lock where `AsyncSocket.cpp` took it, and calls the hooks under it as the C++ did.
//! No borrow of the core lives across a hook. The behaviour is `golden_sockobj.txt`, recorded over the C++ class.

use std::cell::UnsafeCell;
use std::ffi::{c_long, c_void};
use std::sync::atomic::{AtomicU8, AtomicUsize, Ordering::Relaxed};

use ee_plat_sys::constants::PlatSocket;
use ee_plat_sys::thread::imp as plat;

use crate::ipaddr::Addr;
use crate::ipobj::EeWonIpAddr;
use crate::ops::LockedSocket;
use crate::socket::{self, Socket, KIND_STREAM};
use crate::status::*;

/// A C++ call on a socket (`AsyncSocket*`).
pub type EeWonAsyncSocketFn = Option<unsafe extern "C" fn(socket: *mut c_void)>;
/// A transform's `Send` / `Recv` on a message (`MsgTransform*`, `ByteBufferPtr*`): its status.
pub type EeWonTransformFn = Option<unsafe extern "C" fn(transform: *mut c_void, message: *mut c_void) -> c_long>;

/// What the socket object calls back in the C++. One static table serves every socket.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonAsyncSocketHooks {
    /// the virtual `OpenHook`, `CloseHook`, `KillHook`
    pub open_hook: EeWonAsyncSocketFn,
    pub close_hook: EeWonAsyncSocketFn,
    pub kill_hook: EeWonAsyncSocketFn,
    /// `transform->Send(message)`, `transform->Recv(message)`
    pub transform_send: EeWonTransformFn,
    pub transform_recv: EeWonTransformFn,
    /// `delete transform`
    pub transform_free: Option<unsafe extern "C" fn(transform: *mut c_void)>,
}

/// A recursive platform lock held for a scope.
struct Held(*mut plat::PlatMutex);

impl Held {
    fn new(m: *mut plat::PlatMutex) -> Held {
        // SAFETY: the socket's live mutex
        unsafe { plat::PlatMutexLock(m) };
        Held(m)
    }
}

impl Drop for Held {
    fn drop(&mut self) {
        // SAFETY: locked in `new`
        unsafe { plat::PlatMutexUnlock(self.0) };
    }
}

/// `AsyncSocket`'s state and steps.
pub struct EeWonAsyncSocket {
    /// the C++ `AsyncSocket*` the hooks get
    socket: *mut c_void,
    hooks: &'static EeWonAsyncSocketHooks,
    /// `mDataCrit`: the socket's recursive lock, owned
    lock: *mut plat::PlatMutex,
    /// `mDestAddr`: the peer of a connect or an accept (under the lock)
    dest: UnsafeCell<EeWonIpAddr>,
    core: UnsafeCell<Socket>,
    length_field_size: AtomicU8,
    /// the C++ `MsgTransform*`s, owned, in chain order
    transforms: UnsafeCell<Vec<*mut c_void>>,
    /// `mThreadCloseCallback` (a `void (*)(AsyncSocket*)`, 0 for none)
    close_callback: AtomicUsize,
}

// SAFETY: the core and the transforms are touched under `lock` (or, where the C++ read them unlocked, read so); the
// hooks' contract makes the C++ calls thread-safe as the C++ was
unsafe impl Send for EeWonAsyncSocket {}
// SAFETY: as Send
unsafe impl Sync for EeWonAsyncSocket {}

impl EeWonAsyncSocket {
    /// `AsyncSocket::AsyncSocket(type)`: closed, a 4-byte length prefix, no transforms, no destination, a new
    /// lock; counts it.
    pub fn new(socket: *mut c_void, hooks: &'static EeWonAsyncSocketHooks, kind: i32) -> EeWonAsyncSocket {
        EeWonAsyncSocket {
            socket,
            hooks,
            // SAFETY: a plain platform call; destroyed in `drop`
            lock: unsafe { plat::PlatMutexCreate() },
            dest: UnsafeCell::new(EeWonIpAddr::default()),
            core: UnsafeCell::new(Socket::new(kind)),
            length_field_size: AtomicU8::new(4),
            transforms: UnsafeCell::new(Vec::new()),
            close_callback: AtomicUsize::new(0),
        }
    }

    #[allow(clippy::mut_from_ref)]
    fn core(&self) -> &mut Socket {
        // SAFETY: the callers hold the lock (or read a field the C++ read unlocked) and drop the borrow before any
        // hook
        unsafe { &mut *self.core.get() }
    }

    #[allow(clippy::mut_from_ref)]
    fn transforms(&self) -> &mut Vec<*mut c_void> {
        // SAFETY: the callers hold the lock and drop the borrow before any hook
        unsafe { &mut *self.transforms.get() }
    }

    fn call(&self, hook: EeWonAsyncSocketFn) {
        if let Some(h) = hook {
            // SAFETY: the hooks' contract: `socket` is the live C++ socket
            unsafe { h(self.socket) };
        }
    }

    /// The socket for the ops' loops (`ops::send_all`, `ops::recv_line`): the core and the lock.
    pub fn locked(&self) -> LockedSocket {
        LockedSocket { core: self.core.get(), lock: self.lock.cast() }
    }

    pub fn kind(&self) -> i32 {
        self.core().kind
    }
    pub fn is_valid(&self) -> bool {
        self.core().is_valid()
    }
    pub fn is_open(&self) -> bool {
        self.core().open
    }
    pub fn is_connected(&self) -> bool {
        self.core().connected
    }
    /// the last socket error recorded (`mCore.status`)
    pub fn status(&self) -> Status {
        self.core().status as Status
    }
    pub fn handle(&self) -> PlatSocket {
        let _held = Held::new(self.lock);
        self.core().handle
    }
    pub fn length_field_size(&self) -> u8 {
        self.length_field_size.load(Relaxed)
    }
    pub fn set_length_field_size(&self, size: u8) {
        self.length_field_size.store(size, Relaxed);
    }
    pub fn set_close_callback(&self, callback: usize) {
        self.close_callback.store(callback, Relaxed);
    }

    /// The lock (`mDataCrit`'s), for the subclasses and the connections.
    pub fn mutex(&self) -> *mut plat::PlatMutex {
        self.lock
    }

    #[allow(clippy::mut_from_ref)]
    fn dest(&self) -> &mut EeWonIpAddr {
        // SAFETY: the callers hold the lock and drop the borrow before any hook
        unsafe { &mut *self.dest.get() }
    }

    /// `GetDestAddr`: a copy, taken under the lock (the original returned a reference to the member).
    pub fn dest_addr(&self) -> EeWonIpAddr {
        let _held = Held::new(self.lock);
        self.dest().clone()
    }

    /// `SetDestAddr`.
    pub fn set_dest_addr(&self, addr: &EeWonIpAddr) {
        let _held = Held::new(self.lock);
        *self.dest() = addr.clone();
    }

    /// `Fn2B0E60`: `WS_InvalidAddress` for an invalid address; under the lock, [`connect_prepare`]
    /// (`local_host` is `IPAddr::GetLocalHost()`), the destination's store, then the connect.
    ///
    /// [`connect_prepare`]: Self::connect_prepare
    pub fn connect_to(&self, addr: &EeWonIpAddr, local_host: u32) -> Status {
        if !addr.valid {
            return WS_INVALID_ADDRESS;
        }
        let _held = Held::new(self.lock);
        let s = self.connect_prepare(local_host);
        if s != WS_SUCCESS {
            return s;
        }
        *self.dest() = addr.clone();
        self.connect(addr.addr())
    }

    /// `GetLastSocketError`.
    pub fn last_error(&self) -> Status {
        self.core().last_error()
    }

    fn notify_thread(&self) {
        let c = self.close_callback.load(Relaxed);
        if c != 0 {
            // SAFETY: a `void (*)(AsyncSocket*)` the C++ set
            let f: unsafe extern "C" fn(*mut c_void) = unsafe { std::mem::transmute(c) };
            // SAFETY: the C++ socket
            unsafe { f(self.socket) };
        }
    }

    /// `Close`: the socket thread's callback, then under the lock the close hook and the handle's close.
    pub fn close(&self) {
        self.notify_thread();
        let _held = Held::new(self.lock);
        self.call(self.hooks.close_hook);
        self.core().close_handle();
    }

    /// `ClosePrv`: the handle's close alone.
    pub fn close_handle(&self) {
        let _held = Held::new(self.lock);
        self.core().close_handle();
    }

    /// `Kill`: as `Close`, with the kill hook after the close hook.
    pub fn kill(&self) {
        self.notify_thread();
        let _held = Held::new(self.lock);
        self.call(self.hooks.close_hook);
        self.call(self.hooks.kill_hook);
        self.core().close_handle();
    }

    /// `Open`: an open socket is closed first; a fresh handle, then the open hook. 0 or the error.
    pub fn open(&self) -> Status {
        let _held = Held::new(self.lock);
        if self.is_valid() {
            self.close();
        }
        let s = self.core().open_handle();
        if s != WS_SUCCESS {
            return s;
        }
        self.call(self.hooks.open_hook);
        WS_SUCCESS
    }

    /// `Bind`: opened first when closed; address reuse when asked.
    pub fn bind(&self, addr: Addr, reuse: bool) -> Status {
        let _held = Held::new(self.lock);
        if !self.is_valid() {
            let s = self.open();
            if s != WS_SUCCESS {
                return s;
            }
        }
        self.core().bind(addr, reuse)
    }

    /// `Fn2B0E60`'s first part (the caller holds the lock and has checked the address): a connected stream socket
    /// is disconnected; a closed socket is opened and, when the local host is set, bound to it (`Fn2B0D90(0, 0)`).
    pub fn connect_prepare(&self, local_host: u32) -> Status {
        let _held = Held::new(self.lock);
        if self.kind() == KIND_STREAM && self.is_connected() {
            self.disconnect();
        }
        if !self.is_valid() {
            let s = self.open();
            if s != WS_SUCCESS {
                return s;
            }
            if local_host != 0 {
                let s = self.bind(Addr::from_host(local_host, 0), false);
                if s != WS_SUCCESS {
                    return s;
                }
            }
        }
        WS_SUCCESS
    }

    /// `Fn2B0E60`'s connect (after `connect_prepare` and the destination's store).
    pub fn connect(&self, addr: Addr) -> Status {
        let _held = Held::new(self.lock);
        self.core().connect(addr)
    }

    /// `Listen`.
    pub fn listen(&self, backlog: i32) -> Status {
        let _held = Held::new(self.lock);
        self.core().listen(backlog)
    }

    /// `Accept`'s system part (the caller duplicates the socket and [`adopt`](Self::adopt)s the handle into it).
    pub fn accept(&self) -> Result<(PlatSocket, Addr), Status> {
        let _held = Held::new(self.lock);
        self.core().accept()
    }

    /// The accepted handle into this new socket, and its peer as the destination (an address made from the
    /// sockaddr, as `Accept`'s `IPAddr(sockaddr)`).
    pub fn adopt(&self, handle: PlatSocket, peer: Addr) {
        let _held = Held::new(self.lock);
        self.core().adopt(handle);
        let mut a = EeWonIpAddr::default();
        a.set_sockaddr(peer_sockaddr(peer));
        *self.dest() = a;
    }

    /// `WaitForConnect`: the handle read under the lock, the wait without it.
    pub fn wait_for_connect(&self, timeout: u32) -> Status {
        socket::wait_for_connect(self.handle(), timeout)
    }

    /// `WaitForRead` / `WaitForWrite` (`socket::WAIT_READ` / `WAIT_WRITE`): nothing for 0.
    pub fn wait(&self, mask: u32, timeout: u32) {
        if timeout == 0 {
            return;
        }
        socket::wait(self.handle(), mask, timeout);
    }

    /// `Send`.
    pub fn send(&self, data: &[u8], sent: &mut i32) -> Status {
        let _held = Held::new(self.lock);
        self.core().send(data, sent)
    }

    /// `Recv`.
    pub fn recv(&self, buffer: &mut [u8], received: &mut i32) -> Status {
        let _held = Held::new(self.lock);
        self.core().recv(buffer, received)
    }

    /// `SendTo`: a datagram socket only; opened first when closed.
    pub fn send_to(&self, data: &[u8], to: Addr) -> Status {
        let _held = Held::new(self.lock);
        let s = self.core().datagram_only();
        if s != WS_SUCCESS {
            return s;
        }
        if !self.is_valid() {
            let s = self.open();
            if s != WS_SUCCESS {
                return s;
            }
        }
        self.core().send_to(data, to)
    }

    /// `RecvFrom`.
    pub fn recv_from(&self, buffer: &mut [u8], from: &mut Option<Addr>, received: &mut i32) -> Status {
        let _held = Held::new(self.lock);
        self.core().recv_from(buffer, from, received)
    }

    /// `Disconnect`: a stream socket closes; a datagram socket dissolves its peer (and closes when that fails).
    pub fn disconnect(&self) {
        let _held = Held::new(self.lock);
        if self.kind() != KIND_STREAM {
            if !self.core().dissolve_peer() {
                self.close();
            }
        } else {
            self.close();
        }
    }

    /// `SetType`: a change of type closes the socket.
    pub fn set_type(&self, kind: i32) {
        let _held = Held::new(self.lock);
        if self.kind() != kind {
            self.close();
        }
        self.core().kind = kind;
    }

    /// `GetLocalPort` (unlocked, as the C++).
    pub fn local_port(&self) -> u16 {
        self.core().local_port()
    }

    /// `SetBroadcast` (unlocked, as the C++).
    pub fn set_broadcast(&self, on: bool) -> Status {
        self.core().set_broadcast(on)
    }

    /// `SetNagling` (unlocked, as the C++).
    pub fn set_nagling(&self, on: bool) -> Status {
        self.core().set_nagling(on)
    }

    /// `AddMsgTransform`: appended; the socket owns it.
    pub fn add_transform(&self, transform: *mut c_void) {
        let _held = Held::new(self.lock);
        self.transforms().push(transform);
    }

    /// `RemoveAllMsgTransforms`: each deleted, in order.
    pub fn remove_all_transforms(&self) {
        let _held = Held::new(self.lock);
        let all = std::mem::take(self.transforms());
        self.free_transforms(&all);
    }

    fn free_transforms(&self, all: &[*mut c_void]) {
        if let Some(h) = self.hooks.transform_free {
            for &t in all {
                // SAFETY: an owned transform, deleted once
                unsafe { h(t) };
            }
        }
    }

    /// `TransformSendMsg`: each transform's `Send` in order; the first failure ends the chain.
    ///
    /// # Safety
    /// `message` is the caller's live `ByteBufferPtr*`.
    pub unsafe fn transform_send(&self, message: *mut c_void) -> Status {
        let _held = Held::new(self.lock);
        let chain = self.transforms().clone();
        for t in chain {
            // SAFETY: the hooks' contract; an owned transform and the caller's message
            let s = self.hooks.transform_send.map_or(WS_SUCCESS, |h| unsafe { h(t, message) } as Status);
            if s != WS_SUCCESS {
                return s;
            }
        }
        WS_SUCCESS
    }

    /// `TransformRecvMsg`: each transform's `Recv` in reverse order; the first failure ends the chain.
    ///
    /// # Safety
    /// `message` is the caller's live `ByteBufferPtr*`.
    pub unsafe fn transform_recv(&self, message: *mut c_void) -> Status {
        let _held = Held::new(self.lock);
        let chain = self.transforms().clone();
        for &t in chain.iter().rev() {
            // SAFETY: as transform_send
            let s = self.hooks.transform_recv.map_or(WS_SUCCESS, |h| unsafe { h(t, message) } as Status);
            if s != WS_SUCCESS {
                return s;
            }
        }
        WS_SUCCESS
    }
}

impl Drop for EeWonAsyncSocket {
    fn drop(&mut self) {
        // `~AsyncSocket`: closed, every transform deleted, uncounted
        self.close();
        let all = std::mem::take(self.transforms());
        self.free_transforms(&all);
        self.core().destroyed();
        // SAFETY: created in `new`, no longer held
        unsafe { plat::PlatMutexDestroy(self.lock) };
    }
}

/// An accepted peer's sockaddr (family, network-order port and address, zero padding).
fn peer_sockaddr(a: Addr) -> ee_plat_sys::constants::PlatSockAddrIn {
    ee_plat_sys::constants::PlatSockAddrIn {
        mFamily: ee_plat_sys::constants::PLAT_AF_INET,
        mPort: a.port,
        mAddress: a.address,
        mZero: [0; 8],
    }
}

// ---- the C ABI

/// A new socket object for the C++ `socket` (the hooks get it), with its own lock.
///
/// # Safety
/// `hooks` is a static table; `socket` outlives the object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_new(
    socket: *mut c_void,
    hooks: *const EeWonAsyncSocketHooks,
    kind: i32,
) -> *mut EeWonAsyncSocket {
    // SAFETY: the caller's static table
    let hooks = unsafe { &*hooks };
    Box::into_raw(Box::new(EeWonAsyncSocket::new(socket, hooks, kind)))
}

/// `~AsyncSocket`: closes the socket, deletes the transforms, frees the object.
///
/// # Safety
/// `s` came from `ee_won_asyncsocket_new` and is freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_free(s: *mut EeWonAsyncSocket) {
    if !s.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(s) });
    }
}

macro_rules! sock {
    ($s:expr) => {
        // SAFETY: the callers pass a live socket object
        unsafe { &*$s }
    };
}

/// `GetType`.
///
/// # Safety
/// `s` is a live socket object (so for every `ee_won_asyncsocket_` call).
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_kind(s: *const EeWonAsyncSocket) -> i32 {
    sock!(s).kind()
}
/// `IsValid`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_is_valid(s: *const EeWonAsyncSocket) -> bool {
    sock!(s).is_valid()
}
/// `IsOpen`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_is_open(s: *const EeWonAsyncSocket) -> bool {
    sock!(s).is_open()
}
/// `IsConnected`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_is_connected(s: *const EeWonAsyncSocket) -> bool {
    sock!(s).is_connected()
}
/// The last socket error recorded (the original's `mStatus`, +0x54).
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_status(s: *const EeWonAsyncSocket) -> c_long {
    sock!(s).status() as c_long
}
/// `GetLengthFieldSize`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_length_field_size(s: *const EeWonAsyncSocket) -> u8 {
    sock!(s).length_field_size()
}
/// `SetLengthFieldSize`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_set_length_field_size(s: *const EeWonAsyncSocket, size: u8) {
    sock!(s).set_length_field_size(size)
}
/// `mThreadCloseCallback = callback` (a `void (*)(AsyncSocket*)`, null for none).
///
/// # Safety
/// As `ee_won_asyncsocket_kind`; `callback` is null or such a function.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_set_close_callback(s: *const EeWonAsyncSocket, callback: *mut c_void) {
    sock!(s).set_close_callback(callback as usize)
}
/// `GetLastSocketError`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_last_error(s: *const EeWonAsyncSocket) -> c_long {
    sock!(s).last_error() as c_long
}
/// `Close`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_close(s: *const EeWonAsyncSocket) {
    sock!(s).close()
}
/// `Kill`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_kill(s: *const EeWonAsyncSocket) {
    sock!(s).kill()
}
/// `Open`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_open(s: *const EeWonAsyncSocket) -> c_long {
    sock!(s).open() as c_long
}
/// `Bind` (network-order address and port).
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_bind(
    s: *const EeWonAsyncSocket,
    address: u32,
    port: u16,
    reuse: bool,
) -> c_long {
    sock!(s).bind(Addr { address, port }, reuse) as c_long
}
/// `Fn2B0E60` (`local_host` is `IPAddr::GetLocalHost()`, host order).
///
/// # Safety
/// As `ee_won_asyncsocket_kind`; `addr` is a live value.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_connect_to(
    s: *const EeWonAsyncSocket,
    addr: *const EeWonIpAddr,
    local_host: u32,
) -> c_long {
    // SAFETY: the caller's value
    sock!(s).connect_to(unsafe { &*addr }, local_host) as c_long
}
/// The lock (a `PlatMutex*`, recursive), alive as long as the object.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_mutex(s: *const EeWonAsyncSocket) -> *mut c_void {
    sock!(s).mutex().cast()
}
/// `GetDestAddr`: a new value (the C++ `IPAddr` adopts it).
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_dest_addr(s: *const EeWonAsyncSocket) -> *mut EeWonIpAddr {
    Box::into_raw(Box::new(sock!(s).dest_addr()))
}
/// `SetDestAddr`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`; `addr` is a live value.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_set_dest_addr(s: *const EeWonAsyncSocket, addr: *const EeWonIpAddr) {
    // SAFETY: the caller's value
    sock!(s).set_dest_addr(unsafe { &*addr })
}
/// `Listen`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_listen(s: *const EeWonAsyncSocket, backlog: i32) -> c_long {
    sock!(s).listen(backlog) as c_long
}
/// `Accept`'s system part: the accepted handle and the peer, or the error.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`; the out-parameters are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_accept(
    s: *const EeWonAsyncSocket,
    handle: *mut isize,
    address: *mut u32,
    port: *mut u16,
) -> c_long {
    match sock!(s).accept() {
        Ok((h, a)) => {
            // SAFETY: the caller's out-parameters
            unsafe {
                *handle = h;
                *address = a.address;
                *port = a.port;
            }
            0
        }
        Err(e) => e as c_long,
    }
}
/// The accepted handle into the new socket `s`, the peer (network order) its destination.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_adopt(s: *const EeWonAsyncSocket, handle: isize, address: u32, port: u16) {
    sock!(s).adopt(handle, Addr { address, port })
}
/// `WaitForConnect`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_wait_for_connect(s: *const EeWonAsyncSocket, timeout: u32) -> c_long {
    sock!(s).wait_for_connect(timeout) as c_long
}
/// `WaitForRead` (mask 1) / `WaitForWrite` (mask 2).
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_wait(s: *const EeWonAsyncSocket, mask: u32, timeout: u32) {
    sock!(s).wait(mask, timeout)
}
/// `Send` (`sent` may be null).
///
/// # Safety
/// As `ee_won_asyncsocket_kind`; `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_send(
    s: *const EeWonAsyncSocket,
    data: *const c_void,
    length: i32,
    sent: *mut i32,
) -> c_long {
    let mut n = -1;
    // SAFETY: the caller's bytes
    let r = sock!(s).send(unsafe { socket::raw(data, length) }, &mut n);
    if !sent.is_null() && n >= 0 {
        // SAFETY: the caller's out-parameter
        unsafe { *sent = n };
    }
    r as c_long
}

/// `Recv` (`received` may be null).
///
/// # Safety
/// As `ee_won_asyncsocket_kind`; `data` has room for `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_recv(
    s: *const EeWonAsyncSocket,
    data: *mut c_void,
    length: i32,
    received: *mut i32,
) -> c_long {
    let mut n = -1;
    // SAFETY: the caller's buffer
    let r = sock!(s).recv(unsafe { socket::raw_mut(data, length) }, &mut n);
    if !received.is_null() && n >= 0 {
        // SAFETY: the caller's out-parameter
        unsafe { *received = n };
    }
    r as c_long
}
/// `SendTo`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`; `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_send_to(
    s: *const EeWonAsyncSocket,
    data: *const c_void,
    length: i32,
    address: u32,
    port: u16,
) -> c_long {
    // SAFETY: the caller's bytes
    sock!(s).send_to(unsafe { socket::raw(data, length) }, Addr { address, port }) as c_long
}
/// `RecvFrom`: the sender in `*address` / `*port` and true in `*from` on success.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`; `data` has room for `length` bytes; the out-parameters are valid (`received` may
/// be null).
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_recv_from(
    s: *const EeWonAsyncSocket,
    data: *mut c_void,
    length: i32,
    address: *mut u32,
    port: *mut u16,
    from: *mut bool,
    received: *mut i32,
) -> c_long {
    let mut peer = None;
    let mut n = 0;
    // SAFETY: the caller's buffer
    let r = sock!(s).recv_from(unsafe { socket::raw_mut(data, length) }, &mut peer, &mut n);
    // SAFETY: the caller's out-parameters
    unsafe {
        *from = false;
        if let Some(a) = peer {
            *address = a.address;
            *port = a.port;
            *from = true;
            if !received.is_null() {
                *received = n;
            }
        }
    }
    r as c_long
}
/// `Disconnect`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_disconnect(s: *const EeWonAsyncSocket) {
    sock!(s).disconnect()
}
/// `SetType`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_set_type(s: *const EeWonAsyncSocket, kind: i32) {
    sock!(s).set_type(kind)
}
/// `GetLocalPort`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_local_port(s: *const EeWonAsyncSocket) -> u16 {
    sock!(s).local_port()
}
/// `SetBroadcast`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_set_broadcast(s: *const EeWonAsyncSocket, on: bool) -> c_long {
    sock!(s).set_broadcast(on) as c_long
}
/// `SetNagling`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_set_nagling(s: *const EeWonAsyncSocket, on: bool) -> c_long {
    sock!(s).set_nagling(on) as c_long
}
/// `AddMsgTransform` (the socket owns `transform`).
///
/// # Safety
/// As `ee_won_asyncsocket_kind`; `transform` is a `MsgTransform*` the hooks delete.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_add_transform(s: *const EeWonAsyncSocket, transform: *mut c_void) {
    sock!(s).add_transform(transform)
}
/// `RemoveAllMsgTransforms`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_remove_all_transforms(s: *const EeWonAsyncSocket) {
    sock!(s).remove_all_transforms()
}
/// `TransformSendMsg` (`message` is the caller's `ByteBufferPtr*`).
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_transform_send(s: *const EeWonAsyncSocket, message: *mut c_void) -> c_long {
    let s = sock!(s);
    // SAFETY: the caller's message
    unsafe { s.transform_send(message) as c_long }
}
/// `TransformRecvMsg`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_transform_recv(s: *const EeWonAsyncSocket, message: *mut c_void) -> c_long {
    let s = sock!(s);
    // SAFETY: the caller's message
    unsafe { s.transform_recv(message) as c_long }
}
/// `SendBytesOp::Continue`'s loop on the socket: sends `data[*pos..length]`; `*pos` follows.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`; `data` holds `length` bytes; `pos` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_send_all(
    s: *const EeWonAsyncSocket,
    data: *const u8,
    length: usize,
    pos: *mut u32,
    timing: crate::ops::OpTiming,
) -> c_long {
    let bytes = if data.is_null() || length == 0 {
        &[][..]
    } else {
        // SAFETY: the caller's bytes
        unsafe { std::slice::from_raw_parts(data, length) }
    };
    // SAFETY: the caller's out-parameter
    crate::ops::send_all(&sock!(s).locked(), bytes, unsafe { &mut *pos }, timing) as c_long
}

/// `ClosePrv`.
///
/// # Safety
/// As `ee_won_asyncsocket_kind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_close_handle(s: *const EeWonAsyncSocket) {
    sock!(s).close_handle()
}

/// `RecvCRMsgOp::Continue`'s loop on the socket: each byte to `sink` until a CR-LF; `last` is the line so far's
/// last byte (-1 for none).
///
/// # Safety
/// As `ee_won_asyncsocket_kind`; the sink is called during the call only.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncsocket_recv_line(
    s: *const EeWonAsyncSocket,
    last: i32,
    timing: crate::ops::OpTiming,
    sink: crate::ffi::EeWonByteSink,
    context: *mut c_void,
) -> c_long {
    let mut last = last;
    // SAFETY: the sink's contract
    crate::ops::recv_line(&sock!(s).locked(), &mut last, timing, |b| unsafe { sink(context, &b, 1) }) as c_long
}

#[cfg(test)]
#[path = "sockobj_tests.rs"]
mod tests;
