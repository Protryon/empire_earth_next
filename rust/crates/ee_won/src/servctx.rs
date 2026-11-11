//! `ServerContext` (WONServer/ServerContext.cpp), wholly here: a lock-protected list of server addresses in the
//! order they are tried. An address joins once (by its six network bytes, `IPAddr::operator==`); a server that
//! failed moves to the end; the first copy shuffles the list once (the original's `random_shuffle` over the CRT's
//! `rand()`, the draws handed in). The C++ `ServerContext` is a shim over a [`ServerList`]: it turns `IPAddr` values
//! into [`Address`] (the sockaddr's 16 bytes, the validity, the remembered host string) and back, and walks the
//! directory entities itself.
//!
//! Reached by nothing in the game today: every server list is for the WON servers, which are gone.

use std::ffi::c_void;
use std::sync::Mutex;

/// An `IPAddr`'s value.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Address {
    /// `mAddr`: family, port and address in network order, zero padding
    pub sockaddr: [u8; 16],
    pub valid: bool,
    pub remember_host: bool,
    pub host: Vec<u8>,
}

impl Address {
    /// The six bytes `IPAddr` compares (port, then address).
    pub fn key(&self) -> [u8; 6] {
        let mut k = [0u8; 6];
        k.copy_from_slice(&self.sockaddr[2..8]);
        k
    }
}

#[derive(Debug)]
struct Inner {
    shuffle: bool,
    shuffled: bool,
    list: Vec<Address>,
}

/// The server list.
#[derive(Debug)]
pub struct ServerList {
    inner: Mutex<Inner>,
}

impl Default for ServerList {
    fn default() -> Self {
        ServerList::new()
    }
}

impl ServerList {
    pub fn new() -> ServerList {
        ServerList { inner: Mutex::new(Inner { shuffle: true, shuffled: false, list: Vec::new() }) }
    }

    fn lock(&self) -> std::sync::MutexGuard<'_, Inner> {
        self.inner.lock().unwrap_or_else(|e| e.into_inner())
    }

    /// `Clear`: no address, the one-time shuffle armed again.
    pub fn clear(&self) {
        let mut g = self.lock();
        g.list.clear();
        g.shuffle = true;
        g.shuffled = false;
    }

    /// `AddAddress`: a valid address not yet listed joins at the end; whether it joined.
    pub fn add(&self, a: &Address) -> bool {
        if !a.valid {
            return false;
        }
        let mut g = self.lock();
        let k = a.key();
        if g.list.iter().any(|e| e.key() == k) {
            return false;
        }
        g.list.push(a.clone());
        true
    }

    /// `AtomicClearAndAddAddressesFromDir`: cleared and the addresses added under one lock; whether any joined.
    pub fn replace(&self, addrs: &[Address]) -> bool {
        let mut g = self.lock();
        g.list.clear();
        g.shuffle = true;
        g.shuffled = false;
        let mut added = false;
        for a in addrs {
            if a.valid && !g.list.iter().any(|e| e.key() == a.key()) {
                g.list.push(a.clone());
                added = true;
            }
        }
        added
    }

    /// `NotifyFailed`: a listed server moves to the end (the list keeps the value it was added with).
    pub fn notify_failed(&self, a: &Address) {
        let mut g = self.lock();
        let k = a.key();
        if let Some(i) = g.list.iter().position(|e| e.key() == k) {
            let e = g.list.remove(i);
            g.list.push(e);
        }
    }

    /// `RemoveAddress`: whether it was listed.
    pub fn remove(&self, a: &Address) -> bool {
        let mut g = self.lock();
        let k = a.key();
        match g.list.iter().position(|e| e.key() == k) {
            Some(i) => {
                g.list.remove(i);
                true
            }
            None => false,
        }
    }

    /// `GetNumAddresses`.
    pub fn len(&self) -> usize {
        self.lock().list.len()
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    pub fn set_shuffle(&self, shuffle: bool) {
        self.lock().shuffle = shuffle;
    }

    pub fn shuffle(&self) -> bool {
        self.lock().shuffle
    }

    /// `CopyAddresses(AddrList&)`: the list in order, shuffled first the first time (when shuffling is on);
    /// `srand_and_draw(true)` seeds the generator (the original's `srand(time(NULL))`), then each call is a draw.
    pub fn copy(&self, mut srand_and_draw: impl FnMut(bool) -> i32) -> Vec<Address> {
        let mut g = self.lock();
        if g.shuffle && !g.shuffled {
            srand_and_draw(true);
            let order = crate::server::shuffle_order(g.list.len(), || srand_and_draw(false));
            let old = std::mem::take(&mut g.list);
            g.list = order.into_iter().map(|i| old[i].clone()).collect();
            g.shuffled = true;
        }
        g.list.clone()
    }

    /// `CopyAddresses(AddrSet&)`: the addresses in the six bytes' order.
    pub fn copy_sorted(&self) -> Vec<Address> {
        let mut v = self.lock().list.clone();
        v.sort_by_key(|a| a.key());
        v
    }
}

// ---- the C ABI

/// An address crossing to and from the C++ (`host` points into the C++'s or Rust's string for the call).
#[repr(C)]
pub struct EeWonAddr {
    pub sockaddr: [u8; 16],
    pub valid: bool,
    pub remember_host: bool,
    pub host: *const u8,
    pub host_len: usize,
}

unsafe fn from_c(a: *const EeWonAddr) -> Address {
    let a = unsafe { &*a };
    let host = if a.host.is_null() || a.host_len == 0 {
        Vec::new()
    } else {
        unsafe { std::slice::from_raw_parts(a.host, a.host_len) }.to_vec()
    };
    Address { sockaddr: a.sockaddr, valid: a.valid, remember_host: a.remember_host, host }
}

/// Receives each address of a copy, in order.
pub type EeWonAddrSink = unsafe extern "C" fn(context: *mut c_void, addr: *const EeWonAddr);
/// `srand(time(NULL))` (`seed` true, the result unused) or `rand()`.
pub type EeWonRand = unsafe extern "C" fn(seed: bool) -> i32;

unsafe fn give(list: &[Address], sink: EeWonAddrSink, context: *mut c_void) {
    for a in list {
        let c = EeWonAddr {
            sockaddr: a.sockaddr,
            valid: a.valid,
            remember_host: a.remember_host,
            host: a.host.as_ptr(),
            host_len: a.host.len(),
        };
        unsafe { sink(context, &c) }
    }
}

/// A new, empty list (shuffling on).
#[no_mangle]
pub extern "C" fn ee_won_ctx_new() -> *mut ServerList {
    Box::into_raw(Box::new(ServerList::new()))
}

/// # Safety
/// `c` came from [`ee_won_ctx_new`] (or is NULL) and is not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ctx_free(c: *mut ServerList) {
    if !c.is_null() {
        drop(unsafe { Box::from_raw(c) });
    }
}

/// # Safety
/// `c` is a live list.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ctx_clear(c: *const ServerList) {
    unsafe { (*c).clear() }
}

/// # Safety
/// `c` is a live list, `a` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ctx_add(c: *const ServerList, a: *const EeWonAddr) -> bool {
    unsafe { (*c).add(&from_c(a)) }
}

/// # Safety
/// `c` is a live list, `addrs` holds `count` addresses.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ctx_replace(c: *const ServerList, addrs: *const EeWonAddr, count: usize) -> bool {
    let list: Vec<Address> = (0..count).map(|i| unsafe { from_c(addrs.add(i)) }).collect();
    unsafe { (*c).replace(&list) }
}

/// # Safety
/// `c` is a live list, `a` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ctx_notify_failed(c: *const ServerList, a: *const EeWonAddr) {
    unsafe { (*c).notify_failed(&from_c(a)) }
}

/// # Safety
/// `c` is a live list, `a` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ctx_remove(c: *const ServerList, a: *const EeWonAddr) -> bool {
    unsafe { (*c).remove(&from_c(a)) }
}

/// # Safety
/// `c` is a live list.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ctx_len(c: *const ServerList) -> usize {
    unsafe { (*c).len() }
}

/// # Safety
/// `c` is a live list.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ctx_set_shuffle(c: *const ServerList, shuffle: bool) {
    unsafe { (*c).set_shuffle(shuffle) }
}

/// # Safety
/// `c` is a live list.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ctx_shuffle(c: *const ServerList) -> bool {
    unsafe { (*c).shuffle() }
}

/// `CopyAddresses(AddrList&)`: each address into `sink`, in order (outside the lock).
///
/// # Safety
/// `c` is a live list; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ctx_copy(
    c: *const ServerList,
    rand: EeWonRand,
    sink: EeWonAddrSink,
    context: *mut c_void,
) {
    let list = unsafe { (*c).copy(|seed| rand(seed)) };
    unsafe { give(&list, sink, context) }
}

/// `CopyAddresses(AddrSet&)`: each address into `sink`, in the six bytes' order.
///
/// # Safety
/// As [`ee_won_ctx_copy`].
#[no_mangle]
pub unsafe extern "C" fn ee_won_ctx_copy_sorted(c: *const ServerList, sink: EeWonAddrSink, context: *mut c_void) {
    let list = unsafe { (*c).copy_sorted() };
    unsafe { give(&list, sink, context) }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn a(port: u16, ip: [u8; 4], host: &str) -> Address {
        let mut s = [0u8; 16];
        s[0] = 2;
        s[2..4].copy_from_slice(&port.to_be_bytes());
        s[4..8].copy_from_slice(&ip);
        Address { sockaddr: s, valid: true, remember_host: !host.is_empty(), host: host.as_bytes().to_vec() }
    }

    #[test]
    fn list_rules() {
        let c = ServerList::new();
        assert!(c.add(&a(1, [1, 2, 3, 4], "x")));
        assert!(!c.add(&a(1, [1, 2, 3, 4], "other name, same address")));
        assert!(!c.add(&Address { valid: false, ..a(9, [9, 9, 9, 9], "") }));
        assert!(c.add(&a(2, [1, 2, 3, 4], "")));
        assert!(c.add(&a(1, [0, 0, 0, 1], "")));
        c.notify_failed(&a(1, [1, 2, 3, 4], ""));
        c.set_shuffle(false);
        let got = c.copy(|_| panic!("no shuffle"));
        assert_eq!(got.iter().map(|e| e.key()[1]).collect::<Vec<_>>(), vec![2, 1, 1]);
        assert_eq!(got[2].host, b"x");
        let sorted = c.copy_sorted();
        assert_eq!(sorted[0].key(), [0, 1, 0, 0, 0, 1]);
        assert!(c.remove(&a(2, [1, 2, 3, 4], "")) && !c.remove(&a(2, [1, 2, 3, 4], "")));
        assert_eq!(c.len(), 2);
        c.clear();
        assert!(c.is_empty() && c.shuffle());
    }

    #[test]
    fn shuffles_once() {
        let c = ServerList::new();
        for p in 0..4 {
            c.add(&a(p, [1, 1, 1, 1], ""));
        }
        let mut draws = vec![7, 0, 5].into_iter();
        let mut seeded = 0;
        let got = c.copy(|seed| {
            if seed {
                seeded += 1;
                0
            } else {
                draws.next().unwrap()
            }
        });
        assert_eq!(seeded, 1);
        assert_eq!(got.iter().map(|e| e.key()[1]).collect::<Vec<_>>(), vec![2, 3, 0, 1]);
        let again = c.copy(|_| panic!("shuffled once"));
        assert_eq!(again, got);
    }
}
