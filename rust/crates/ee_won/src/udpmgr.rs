//! WONSocket's `UDPManager` as a Rust object (round 15): the shared datagram socket (a `QueueSocket`), the default
//! completion, the auto-receive op, the receive ops waiting without an address (a list, in order) and by sender
//! address (a multimap in the address's wire order, an equal address after the ones already there), the clean-up
//! cursors of both, the receive size, and every step of UDPManager.cpp's class: `Bind`, `Close`, `SetAutoRecv`,
//! `QueueOp`, `Send`, `CleanOps`, the iterator-keeping erases and `RecvCallback`'s dispatch. The C++ class is a
//! shim; the ops (`UDPManagerRecvOp`, an `AsyncOp` the lobby subclasses), their message checks and completions,
//! the socket and the ops it queues come back through the [`EeWonUdpHooks`]. Every object held is a `RefCount*` with
//! a reference.
//!
//! The list's and the map's iterators are the entries' ids (an erased entry's id is gone, the end is none), so a
//! cursor survives the inserts and erases the original's node-based containers allowed. No borrow of the state lives
//! across a hook (an op's check, finish or completion may call back into the manager). Like the original, the
//! manager takes no lock: it runs on the thread that pumps the ops' completions. The behaviour is
//! `golden_sockobj.txt`'s UDPManager lines, recorded over the C++ class.

use std::cell::UnsafeCell;
use std::cmp::Ordering;
use std::ffi::{c_long, c_void};
use std::ptr::null;

use crate::ipobj::EeWonIpAddr;
use crate::status::*;

/// `WS_ServerReq_Recv`: a check asking for a new op for the sender.
const WS_SERVER_REQ_RECV: Status = 100059;

/// A C++ call on one object.
pub type EeWonUdpFn = Option<unsafe extern "C" fn(object: *mut c_void)>;

/// What the manager calls back in the C++. One static table serves every manager.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonUdpHooks {
    /// any object's `AddRef()` / `Release()` (`RefCount*`)
    pub ref_add: EeWonUdpFn,
    pub ref_release: EeWonUdpFn,
    /// an op's `GetStatus()`, `GetCompletion()`, `SetCompletion(c)`, `RunAsync(t)`, `Kill(true)`, `Finish(s)`,
    /// `ForceFinish(s)`, `TimeLeft()`
    pub op_status: Option<unsafe extern "C" fn(op: *mut c_void) -> c_long>,
    pub op_completion: Option<unsafe extern "C" fn(op: *mut c_void) -> *mut c_void>,
    pub op_set_completion: Option<unsafe extern "C" fn(op: *mut c_void, completion: *mut c_void)>,
    pub op_run_async: Option<unsafe extern "C" fn(op: *mut c_void, timeout: c_long)>,
    pub op_kill: EeWonUdpFn,
    pub op_finish: Option<unsafe extern "C" fn(op: *mut c_void, status: c_long)>,
    pub op_force_finish: Option<unsafe extern "C" fn(op: *mut c_void, status: c_long)>,
    pub op_time_left: Option<unsafe extern "C" fn(op: *mut c_void) -> u32>,
    /// a receive op's `mAddress` (a new value) and its store, its `SetMsg(bytes)`, and its virtual
    /// `CheckMessage` over the datagram (`*threw` for a `ReadBufferException`)
    pub recv_address: Option<unsafe extern "C" fn(op: *mut c_void) -> *mut EeWonIpAddr>,
    pub recv_set_address: Option<unsafe extern "C" fn(op: *mut c_void, addr: *const EeWonIpAddr)>,
    pub recv_set_msg: Option<unsafe extern "C" fn(op: *mut c_void, bytes: *mut c_void)>,
    pub recv_check:
        Option<unsafe extern "C" fn(op: *mut c_void, data: *const u8, length: usize, threw: *mut bool) -> c_long>,
    /// `new UDPManagerRecvOp` (from `addr`, none for null), a reference taken
    pub new_recv_op: Option<unsafe extern "C" fn(addr: *const EeWonIpAddr) -> *mut c_void>,
    /// `new QueueSocket(UDP)`, a reference taken; its `Kill()`, `SetBroadcast(on)`, `Fn2B0D90(port, false)`
    pub socket_new: Option<unsafe extern "C" fn() -> *mut c_void>,
    pub socket_kill: EeWonUdpFn,
    pub socket_set_broadcast: Option<unsafe extern "C" fn(socket: *mut c_void, on: bool)>,
    pub socket_bind: Option<unsafe extern "C" fn(socket: *mut c_void, port: i32) -> c_long>,
    /// the repeat receive: `SetRepeatCompletion(ParamCompletion(StaticRecvCallback, manager))` and
    /// `SetRepeatOp(new RecvBytesFromOp(size))`
    pub socket_repeat: Option<unsafe extern "C" fn(socket: *mut c_void, manager: *mut c_void, size: i32)>,
    /// `QueueOp(new SendBytesToOp(bytes, addr), -1)`
    pub socket_send: Option<unsafe extern "C" fn(socket: *mut c_void, bytes: *mut c_void, addr: *const EeWonIpAddr)>,
}

#[derive(Default)]
struct State {
    socket: usize,
    default_completion: usize,
    auto_op: usize,
    /// (id, op)
    list: Vec<(u64, usize)>,
    /// (id, address, op), in the address's order
    map: Vec<(u64, EeWonIpAddr, usize)>,
    list_cursor: Option<u64>,
    map_cursor: Option<u64>,
    next_id: u64,
    max_recv: i32,
}

/// `UDPManager`'s state and steps.
pub struct EeWonUdpManager {
    /// the C++ `UDPManager*` (the repeat completion's parameter)
    manager: *mut c_void,
    hooks: &'static EeWonUdpHooks,
    state: UnsafeCell<State>,
}

// SAFETY: the state is touched by the thread running the manager's calls (the C++ contract); no borrow lives across
// a hook
unsafe impl Send for EeWonUdpManager {}
// SAFETY: as Send
unsafe impl Sync for EeWonUdpManager {}

impl EeWonUdpManager {
    /// `UDPManager()`: unbound, no ops, 1024-byte receives.
    pub fn new(manager: *mut c_void, hooks: &'static EeWonUdpHooks) -> EeWonUdpManager {
        EeWonUdpManager { manager, hooks, state: UnsafeCell::new(State { max_recv: 1024, ..State::default() }) }
    }

    #[allow(clippy::mut_from_ref)]
    fn st(&self) -> &mut State {
        // SAFETY: the one caller at a time; no borrow lives across a hook
        unsafe { &mut *self.state.get() }
    }

    fn call(&self, h: EeWonUdpFn, p: usize) {
        if p != 0 {
            if let Some(h) = h {
                // SAFETY: the hooks' contract: a live object
                unsafe { h(p as *mut c_void) };
            }
        }
    }
    fn hold(&self, p: usize) -> usize {
        self.call(self.hooks.ref_add, p);
        p
    }
    fn release(&self, p: usize) {
        self.call(self.hooks.ref_release, p);
    }
    fn status(&self, op: usize) -> Status {
        // SAFETY: the hooks' contract: a live op
        self.hooks.op_status.map_or(WS_SUCCESS, |h| unsafe { h(op as *mut c_void) } as Status)
    }
    fn address(&self, op: usize) -> EeWonIpAddr {
        match self.hooks.recv_address {
            Some(h) => {
                // SAFETY: the hooks' contract: a new value, owned from here
                let a = unsafe { h(op as *mut c_void) };
                if a.is_null() {
                    EeWonIpAddr::default()
                } else {
                    // SAFETY: as above
                    *unsafe { Box::from_raw(a) }
                }
            }
            None => EeWonIpAddr::default(),
        }
    }
    fn new_recv_op(&self, addr: Option<&EeWonIpAddr>) -> usize {
        let a = addr.map_or(null(), |a| a as *const EeWonIpAddr);
        // SAFETY: the hooks' contract: a new op with a reference
        self.hooks.new_recv_op.map_or(0, |h| unsafe { h(a) } as usize)
    }

    pub fn socket(&self) -> usize {
        self.st().socket
    }
    /// `mDefaultCompletion = completion`: the new one's reference first, then the old one's goes.
    pub fn set_default_completion(&self, completion: usize) {
        let new = self.hold(completion);
        let old = std::mem::replace(&mut self.st().default_completion, new);
        self.release(old);
    }

    /// `Close`: the socket killed and dropped, the auto-receive op and every waiting op killed, the containers
    /// emptied, the cursors at their ends.
    pub fn close(&self) {
        let s = self.st().socket;
        if s == 0 {
            return;
        }
        self.call(self.hooks.socket_kill, s);
        self.st().socket = 0;
        self.release(s);
        let auto = self.st().auto_op;
        if auto != 0 {
            self.call(self.hooks.op_kill, auto);
            self.st().auto_op = 0;
            self.release(auto);
        }
        let list: Vec<usize> = self.st().list.iter().map(|e| e.1).collect();
        let map: Vec<usize> = self.st().map.iter().map(|e| e.2).collect();
        for &op in list.iter().chain(map.iter()) {
            self.call(self.hooks.op_kill, op);
        }
        let st = self.st();
        let list = std::mem::take(&mut st.list);
        let map = std::mem::take(&mut st.map);
        st.list_cursor = None;
        st.map_cursor = None;
        for (_, op) in list {
            self.release(op);
        }
        for (_, _, op) in map {
            self.release(op);
        }
    }

    /// `SetAutoRecv`: on makes an auto-receive op when there is none; off drops it.
    pub fn set_auto_recv(&self, on: bool) {
        if on {
            if self.st().auto_op == 0 {
                let op = self.new_recv_op(None);
                self.st().auto_op = op;
            }
        } else {
            let old = std::mem::replace(&mut self.st().auto_op, 0);
            self.release(old);
        }
    }

    /// `QueueOp`: the default completion for an op without one; unbound, the op is run (when not pending) and
    /// killed; else the containers cleaned, the op filed by its address (or in the list), and run when not pending.
    pub fn queue_op(&self, op: usize, timeout: u32) {
        let h = self.hooks;
        // SAFETY (each hook call below): the hooks' contract: a live op
        if h.op_completion.map_or(null(), |f| unsafe { f(op as *mut c_void) } as *const c_void).is_null() {
            let c = self.st().default_completion;
            if let Some(f) = h.op_set_completion {
                unsafe { f(op as *mut c_void, c as *mut c_void) };
            }
        }
        let run = |m: &Self| {
            if m.status(op) != WS_PENDING {
                if let Some(f) = h.op_run_async {
                    unsafe { f(op as *mut c_void, timeout as i32 as c_long) };
                }
            }
        };
        if self.st().socket == 0 {
            run(self);
            self.call(h.op_kill, op);
            return;
        }
        self.clean_ops();
        let addr = self.address(op);
        let op = self.hold(op);
        let st = self.st();
        let id = st.next_id;
        st.next_id += 1;
        if addr.valid {
            // a multimap's insert: after the equal keys
            let at = st.map.partition_point(|e| e.1.compare(&addr) != Ordering::Greater);
            st.map.insert(at, (id, addr, op));
        } else {
            st.list.push((id, op));
        }
        run(self);
    }

    /// `Send`: a datagram queued on the socket (nothing when unbound: the original used the null socket).
    pub fn send(&self, bytes: usize, addr: &EeWonIpAddr) {
        let s = self.st().socket;
        if s != 0 {
            if let Some(h) = self.hooks.socket_send {
                // SAFETY: the hooks' contract: the live socket, the caller's bytes and value
                unsafe { h(s as *mut c_void, bytes as *mut c_void, addr) };
            }
        }
    }

    fn list_next(&self, id: u64) -> Option<u64> {
        let l = &self.st().list;
        let i = l.iter().position(|e| e.0 == id)?;
        l.get(i + 1).map(|e| e.0)
    }
    fn map_next(&self, id: u64) -> Option<u64> {
        let m = &self.st().map;
        let i = m.iter().position(|e| e.0 == id)?;
        m.get(i + 1).map(|e| e.0)
    }

    /// `Erase(list iterator)`: the cursor moved past it first.
    fn erase_list(&self, id: u64) {
        if self.st().list_cursor == Some(id) {
            self.st().list_cursor = self.list_next(id);
        }
        let st = self.st();
        if let Some(i) = st.list.iter().position(|e| e.0 == id) {
            let (_, op) = st.list.remove(i);
            self.release(op);
        }
    }
    /// `Erase(map iterator)`.
    fn erase_map(&self, id: u64) {
        if self.st().map_cursor == Some(id) {
            self.st().map_cursor = self.map_next(id);
        }
        let st = self.st();
        if let Some(i) = st.map.iter().position(|e| e.0 == id) {
            let (_, _, op) = st.map.remove(i);
            self.release(op);
        }
    }

    /// `CleanOps`: up to 10 entries of each container from its cursor, the finished ones dropped; at the end the
    /// cursor goes back to the start for the next time.
    pub fn clean_ops(&self) {
        for _ in 0..10 {
            let Some(id) = self.st().list_cursor else {
                self.st().list_cursor = self.st().list.first().map(|e| e.0);
                break;
            };
            let op = self.st().list.iter().find(|e| e.0 == id).map_or(0, |e| e.1);
            let next = self.list_next(id);
            if self.status(op) != WS_PENDING {
                let st = self.st();
                if let Some(i) = st.list.iter().position(|e| e.0 == id) {
                    let (_, op) = st.list.remove(i);
                    self.release(op);
                }
            }
            self.st().list_cursor = next;
        }
        for _ in 0..10 {
            let Some(id) = self.st().map_cursor else {
                self.st().map_cursor = self.st().map.first().map(|e| e.0);
                break;
            };
            let op = self.st().map.iter().find(|e| e.0 == id).map_or(0, |e| e.2);
            let next = self.map_next(id);
            if self.status(op) != WS_PENDING {
                let st = self.st();
                if let Some(i) = st.map.iter().position(|e| e.0 == id) {
                    let (_, _, op) = st.map.remove(i);
                    self.release(op);
                }
            }
            self.st().map_cursor = next;
        }
    }

    /// An op's message check: `None` for a `ReadBufferException`.
    fn check(&self, op: usize, data: &[u8]) -> Option<Status> {
        let mut threw = false;
        let s = self.hooks.recv_check.map_or(WS_SUCCESS, |h| {
            // SAFETY: the hooks' contract: a live op and the datagram's bytes
            unsafe { h(op as *mut c_void, data.as_ptr(), data.len(), &mut threw) as Status }
        });
        (!threw).then_some(s)
    }

    fn deliver(&self, op: usize, bytes: usize, status: Status) {
        if let Some(h) = self.hooks.recv_set_msg {
            // SAFETY: the hooks' contract: a live op and the datagram
            unsafe { h(op as *mut c_void, bytes as *mut c_void) };
        }
        if let Some(h) = self.hooks.op_finish {
            // SAFETY: as above
            unsafe { h(op as *mut c_void, status as c_long) };
        }
    }

    /// `RecvCallback`: the datagram (`bytes`, a `ByteBuffer`, and its `data`) from `from` to the first waiting op
    /// for that sender whose check accepts it (`WS_ServerReq_Recv` queues a new op for the sender instead), then to
    /// the first address-less op that accepts it (the sender stored in it), else to the auto-receive op with the
    /// default completion (finished at once; a new one armed). A finished op met on the way is dropped.
    pub fn recv(&self, bytes: usize, data: &[u8], from: &EeWonIpAddr) {
        let h = self.hooks;
        let first = self.st().map.iter().find(|e| e.1.compare(from) != Ordering::Less).map(|e| e.0);
        let mut cur = first;
        while let Some(id) = cur {
            let Some(op) =
                self.st().map.iter().find(|e| e.0 == id && e.1.compare(from) == Ordering::Equal).map(|e| e.2)
            else {
                break;
            };
            if self.status(op) != WS_PENDING {
                cur = self.map_next(id);
                self.erase_map(id);
                continue;
            }
            let op = self.hold(op);
            match self.check(op, data) {
                Some(WS_SERVER_REQ_RECV) => {
                    let a = self.address(op);
                    // SAFETY: the hooks' contract: a live op
                    let left = h.op_time_left.map_or(0, |f| unsafe { f(op as *mut c_void) });
                    let new = self.new_recv_op(Some(&a));
                    self.queue_op(new, left);
                    self.release(new);
                }
                Some(s) if s != WS_NONE => {
                    self.deliver(op, bytes, s);
                    self.erase_map(id);
                    self.release(op);
                    return;
                }
                _ => {}
            }
            self.release(op);
            cur = self.map_next(id);
        }
        let mut cur = self.st().list.first().map(|e| e.0);
        while let Some(id) = cur {
            let Some(op) = self.st().list.iter().find(|e| e.0 == id).map(|e| e.1) else { break };
            if self.status(op) != WS_PENDING {
                cur = self.list_next(id);
                self.erase_list(id);
                continue;
            }
            let op = self.hold(op);
            if let Some(s) = self.check(op, data) {
                if s != WS_NONE {
                    if let Some(f) = h.recv_set_address {
                        // SAFETY: the hooks' contract: a live op and the caller's value
                        unsafe { f(op as *mut c_void, from) };
                    }
                    self.deliver(op, bytes, s);
                    self.erase_list(id);
                    self.release(op);
                    return;
                }
            }
            self.release(op);
            cur = self.list_next(id);
        }
        let auto = self.st().auto_op;
        if auto != 0 {
            // SAFETY (each hook call): the hooks' contract: the live auto-receive op
            unsafe {
                if let Some(f) = h.recv_set_msg {
                    f(auto as *mut c_void, bytes as *mut c_void);
                }
                if let Some(f) = h.recv_set_address {
                    f(auto as *mut c_void, from);
                }
                if let Some(f) = h.op_set_completion {
                    f(auto as *mut c_void, self.st().default_completion as *mut c_void);
                }
                if let Some(f) = h.op_force_finish {
                    f(auto as *mut c_void, WS_SUCCESS as c_long);
                }
            }
            let new = self.new_recv_op(None);
            let old = std::mem::replace(&mut self.st().auto_op, new);
            self.release(old);
        }
    }

    /// `Bind`: closed first; a new datagram socket (broadcast when asked) bound to the local host's `port`; on
    /// success the repeat receive of `mMaxRecvBytes`.
    pub fn bind(&self, port: i32, broadcast: bool) -> Status {
        self.close();
        // SAFETY: the hooks' contract: a new socket with a reference
        let s = self.hooks.socket_new.map_or(0, |f| unsafe { f() } as usize);
        let old = std::mem::replace(&mut self.st().socket, s);
        self.release(old);
        if s == 0 {
            return WS_ASYNC_SOCKET_INVALID_SOCKET;
        }
        if broadcast {
            if let Some(f) = self.hooks.socket_set_broadcast {
                // SAFETY: the live socket
                unsafe { f(s as *mut c_void, true) };
            }
        }
        // SAFETY: the live socket
        let status = self.hooks.socket_bind.map_or(WS_SUCCESS, |f| unsafe { f(s as *mut c_void, port) } as Status);
        if status == WS_SUCCESS {
            if let Some(f) = self.hooks.socket_repeat {
                // SAFETY: the live socket and the C++ manager
                unsafe { f(s as *mut c_void, self.manager, self.st().max_recv) };
            }
        }
        status
    }
}

impl Drop for EeWonUdpManager {
    fn drop(&mut self) {
        // the members' destructors: references only (the original's implicit destructor closed nothing)
        let st = std::mem::take(self.st());
        for p in [st.socket, st.default_completion, st.auto_op] {
            self.release(p);
        }
        for (_, op) in st.list {
            self.release(op);
        }
        for (_, _, op) in st.map {
            self.release(op);
        }
    }
}

// ---- the C ABI (`m` is a live manager object)

macro_rules! mgr {
    ($m:expr) => {
        // SAFETY: the callers pass a live manager object
        unsafe { &*$m }
    };
}

/// A new manager object for the C++ `manager`.
///
/// # Safety
/// `hooks` is a static table; `manager` outlives the object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpmanager_new(
    manager: *mut c_void,
    hooks: *const EeWonUdpHooks,
) -> *mut EeWonUdpManager {
    // SAFETY: the caller's static table
    Box::into_raw(Box::new(EeWonUdpManager::new(manager, unsafe { &*hooks })))
}
/// Frees the object (its references go).
///
/// # Safety
/// `m` came from `ee_won_udpmanager_new` and is freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpmanager_free(m: *mut EeWonUdpManager) {
    if !m.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(m) });
    }
}
/// `Bind`.
///
/// # Safety
/// `m` is a live manager object (so for every call below).
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpmanager_bind(m: *const EeWonUdpManager, port: i32, broadcast: bool) -> c_long {
    mgr!(m).bind(port, broadcast) as c_long
}
/// `Close`.
///
/// # Safety
/// As `ee_won_udpmanager_bind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpmanager_close(m: *const EeWonUdpManager) {
    mgr!(m).close()
}
/// `SetAutoRecv`.
///
/// # Safety
/// As `ee_won_udpmanager_bind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpmanager_set_auto_recv(m: *const EeWonUdpManager, on: bool) {
    mgr!(m).set_auto_recv(on)
}
/// `QueueOp` (`op` a `UDPManagerRecvOp` as `RefCount*`).
///
/// # Safety
/// As `ee_won_udpmanager_bind`; `op` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpmanager_queue_op(m: *const EeWonUdpManager, op: *mut c_void, timeout: u32) {
    mgr!(m).queue_op(op as usize, timeout)
}
/// `Send`.
///
/// # Safety
/// As `ee_won_udpmanager_bind`; `bytes` is a live `ByteBuffer` (or null), `addr` a live value.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpmanager_send(
    m: *const EeWonUdpManager,
    bytes: *mut c_void,
    addr: *const EeWonIpAddr,
) {
    // SAFETY: the caller's value
    mgr!(m).send(bytes as usize, unsafe { &*addr })
}
/// `CleanOps`.
///
/// # Safety
/// As `ee_won_udpmanager_bind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpmanager_clean_ops(m: *const EeWonUdpManager) {
    mgr!(m).clean_ops()
}
/// `SetDefaultCompletion`.
///
/// # Safety
/// As `ee_won_udpmanager_bind`; `completion` is null or live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpmanager_set_default_completion(m: *const EeWonUdpManager, completion: *mut c_void) {
    mgr!(m).set_default_completion(completion as usize)
}
/// The socket (a borrowed `QueueSocket*` as `RefCount*`, null when unbound).
///
/// # Safety
/// As `ee_won_udpmanager_bind`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpmanager_socket(m: *const EeWonUdpManager) -> *mut c_void {
    mgr!(m).socket() as *mut c_void
}
/// `RecvCallback`: the datagram (`bytes`, its `data`) and its sender.
///
/// # Safety
/// As `ee_won_udpmanager_bind`; `data` holds `length` bytes; `from` is a live value.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpmanager_recv(
    m: *const EeWonUdpManager,
    bytes: *mut c_void,
    data: *const u8,
    length: usize,
    from: *const EeWonIpAddr,
) {
    let d = if data.is_null() || length == 0 {
        &[][..]
    } else {
        // SAFETY: the caller's bytes
        unsafe { std::slice::from_raw_parts(data, length) }
    };
    // SAFETY: the caller's value
    mgr!(m).recv(bytes as usize, d, unsafe { &*from })
}

#[cfg(test)]
#[path = "udpmgr_tests.rs"]
mod tests;
