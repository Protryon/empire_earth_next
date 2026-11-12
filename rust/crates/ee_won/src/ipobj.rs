//! WONSocket's `IPAddr` as a Rust value (round 14): the `PlatSockAddrIn`, the validity, the remember-host flag and
//! the remembered host text, and every step of `IPAddr.cpp` (the constructors' and setters' parses and lookups, the
//! text forms, the order, the local host). The C++ `IPAddr` is a handle with copy semantics: its copy constructor
//! clones the value, its assignment copies it, its destructor frees it, so every copy site (maps, messages, the
//! sockets' and ops' members) copies the whole value as the original's implicit copy did. The parsing and the
//! lookups themselves are `ipaddr.rs`'s since round 2. ServerContext's conversions go through
//! [`ee_won_ipaddr_to_raw`] / [`ee_won_ipaddr_from_raw`] (servctx.rs's `EeWonAddr`).

use std::ffi::{c_char, c_long, c_void};
use std::sync::atomic::{AtomicI32, Ordering::Relaxed};

use ee_plat_sys::constants::{PlatSockAddrIn, PLAT_AF_INET};

use crate::ffi::EeWonByteSink;
use crate::ipaddr::{self, Addr};
use crate::servctx::EeWonAddr;

/// `IPAddr`'s value.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct EeWonIpAddr {
    /// `mAddr` (+0x14): family, network-order port and address, zero padding
    pub sa: PlatSockAddrIn,
    /// `mIsValid` (+0x24)
    pub valid: bool,
    /// `mRememberHostString` (+0x00)
    pub remember_host: bool,
    /// `mHost` (+0x04)
    pub host: Vec<u8>,
}

/// `IPAddr::mLocalHost`: the chosen local interface, host order (0: none).
static LOCAL_HOST: AtomicI32 = AtomicI32::new(0);

fn sockaddr(a: Addr) -> PlatSockAddrIn {
    PlatSockAddrIn { mFamily: PLAT_AF_INET, mPort: a.port, mAddress: a.address, mZero: [0; 8] }
}

impl EeWonIpAddr {
    pub fn addr(&self) -> Addr {
        Addr { address: self.sa.mAddress, port: self.sa.mPort }
    }

    /// `Fn2B2660`: a dotted address or a host name (looked up) and a host-order port; on failure the address is
    /// zero and invalid (the host text kept); on success the host is remembered when asked, else forgotten.
    pub fn set_resolve(&mut self, host: &[u8], port: u16) -> bool {
        self.sa = PlatSockAddrIn::default();
        self.valid = false;
        let Some(a) = ipaddr::resolve(host, port) else {
            return false;
        };
        if self.remember_host {
            self.host = host.to_vec();
        } else {
            self.host.clear();
        }
        self.sa = sockaddr(a);
        self.valid = true;
        true
    }

    /// `Fn2B2890`: "host:port"; invalid (the rest kept) and false without a colon.
    pub fn set_host_port(&mut self, text: &[u8]) -> bool {
        match ipaddr::split_host_port(text) {
            None => {
                self.valid = false;
                false
            }
            Some((host, port)) => {
                let host = host.to_vec();
                self.set_resolve(&host, port)
            }
        }
    }

    /// `SetSixByte`: the 6-byte wire form (port, then address); the host forgotten.
    pub fn set_six(&mut self, six: &[u8; 6]) {
        self.sa = sockaddr(Addr::from_six_bytes(six));
        self.host.clear();
        self.valid = true;
    }

    /// `Fn2B2B00`: a `PlatSockAddrIn` as it is; the host forgotten.
    pub fn set_sockaddr(&mut self, sa: PlatSockAddrIn) {
        self.sa = sa;
        self.host.clear();
        self.valid = true;
    }

    /// `Fn2B2C20`: a host-order address and port; the host forgotten.
    pub fn set_host_order(&mut self, host: u32, port: u16) {
        self.sa = sockaddr(Addr::from_host(host, port));
        self.host.clear();
        self.valid = true;
    }

    /// `SetWithDefaultPortNoDNS`: "a.b.c.d[:port]" without a lookup; invalid when it is not one (the rest kept).
    pub fn set_no_dns(&mut self, text: &[u8], default_port: u16) -> bool {
        self.valid = false;
        if let Some(a) = ipaddr::parse_no_dns(text, default_port) {
            self.sa = sockaddr(a);
            self.host.clear();
            self.valid = true;
        }
        self.valid
    }

    /// `SetWithDefaultPort`: "host[:port]" (looked up), the default port without a colon.
    pub fn set_with_default_port(&mut self, text: &[u8], default_port: u16) -> bool {
        if text.contains(&b':') {
            self.set_host_port(text)
        } else {
            self.set_resolve(text, default_port)
        }
    }

    /// `GetHostString`: the remembered host when asked and known, else the dotted address.
    pub fn host_string(&self, use_original: bool) -> Vec<u8> {
        if use_original && !self.host.is_empty() {
            return self.host.clone();
        }
        ipaddr::dotted(self.sa.mAddress).into_bytes()
    }

    /// `GetHostAndPortString`: "host:port" as the original's 50-byte buffer held it.
    pub fn host_and_port_string(&self, use_original: bool) -> Vec<u8> {
        ipaddr::host_and_port(&self.host_string(use_original), self.port())
    }

    /// `GetPort`, host order.
    pub fn port(&self) -> u16 {
        u16::from_be(self.sa.mPort)
    }

    /// `SetPort`, host order.
    pub fn set_port(&mut self, port: u16) {
        self.sa.mPort = port.to_be();
    }

    /// `GetHost`, host order (a `long` of the unsigned value).
    pub fn host(&self) -> u32 {
        u32::from_be(self.sa.mAddress)
    }

    /// `operator<`, `==`: the 6-byte wire forms.
    pub fn compare(&self, other: &EeWonIpAddr) -> std::cmp::Ordering {
        ipaddr::compare_six(&self.addr(), &other.addr())
    }

    /// `StringCompare`: the host-order address, then the port.
    pub fn string_compare(&self, other: &EeWonIpAddr) -> i32 {
        ipaddr::string_compare(&self.addr(), &other.addr())
    }
}

/// `GetLocalHost`.
pub fn local_host() -> i32 {
    LOCAL_HOST.load(Relaxed)
}

/// `SetLocalHost`: with validation, an address this machine does not have is refused.
pub fn set_local_host(host: i32, validate: bool) -> bool {
    if validate && host != 0 && !ipaddr::local_addresses(false).iter().any(|&(_, a)| a == host as u32) {
        return false;
    }
    LOCAL_HOST.store(host, Relaxed);
    true
}

/// `GetLocalAddr`: the chosen local interface (port 0), else this machine's name looked up, else invalid.
pub fn local_addr() -> EeWonIpAddr {
    let mut a = EeWonIpAddr::default();
    let h = local_host();
    if h != 0 {
        a.set_host_order(h as u32, 0);
    } else if let Some(name) = ipaddr::host_name() {
        a.set_resolve(&name, 0);
    }
    a
}

// ---- the C ABI (`a` is a live value from `ee_won_ipaddr_new` or `_clone`, freed once)

unsafe fn bytes<'a>(data: *const u8, length: usize) -> &'a [u8] {
    if data.is_null() || length == 0 {
        &[]
    } else {
        // SAFETY: the caller passes `length` bytes
        unsafe { std::slice::from_raw_parts(data, length) }
    }
}

macro_rules! ip {
    ($a:expr) => {
        // SAFETY: the callers pass a live value
        unsafe { &*$a }
    };
}
macro_rules! ip_mut {
    ($a:expr) => {
        // SAFETY: the callers pass a live value, not shared during the call
        unsafe { &mut *$a }
    };
}

/// `IPAddr()`: zero, invalid, no remembered host.
#[no_mangle]
pub extern "C" fn ee_won_ipaddr_new() -> *mut EeWonIpAddr {
    Box::into_raw(Box::default())
}
/// The copy constructor: a copy of `a`'s whole value.
///
/// # Safety
/// `a` is a live value (so for every `ee_won_ipaddr_` call taking one).
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_clone(a: *const EeWonIpAddr) -> *mut EeWonIpAddr {
    Box::into_raw(Box::new(ip!(a).clone()))
}
/// `operator=`: `to` becomes a copy of `from` (the same value is fine).
///
/// # Safety
/// As `ee_won_ipaddr_clone`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_assign(to: *mut EeWonIpAddr, from: *const EeWonIpAddr) {
    if std::ptr::eq(to, from) {
        return;
    }
    let v = ip!(from).clone();
    *ip_mut!(to) = v;
}
/// The destructor.
///
/// # Safety
/// `a` came from `ee_won_ipaddr_new` / `_clone` and is freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_free(a: *mut EeWonIpAddr) {
    if !a.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(a) });
    }
}
/// `SetRememberHostString`.
///
/// # Safety
/// As `ee_won_ipaddr_clone`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_set_remember_host(a: *mut EeWonIpAddr, on: bool) {
    ip_mut!(a).remember_host = on;
}
/// `GetSockAddrIn`: the value's `PlatSockAddrIn` (valid while the value lives and is not set again).
///
/// # Safety
/// As `ee_won_ipaddr_clone`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_sockaddr(a: *const EeWonIpAddr) -> *const c_void {
    (&ip!(a).sa as *const PlatSockAddrIn).cast()
}
/// `GetSixByte`: the 6-byte wire form inside the value (the sockaddr's port and address).
///
/// # Safety
/// As `ee_won_ipaddr_sockaddr`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_six(a: *const EeWonIpAddr) -> *const c_char {
    (&ip!(a).sa.mPort as *const u16).cast()
}
/// `Fn2B2660` (a host-order port).
///
/// # Safety
/// As `ee_won_ipaddr_clone`; `host` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_set_resolve(
    a: *mut EeWonIpAddr,
    host: *const u8,
    length: usize,
    port: u16,
) -> bool {
    // SAFETY: the caller's bytes
    ip_mut!(a).set_resolve(unsafe { bytes(host, length) }, port)
}
/// `Fn2B2890`.
///
/// # Safety
/// As `ee_won_ipaddr_set_resolve`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_set_host_port(a: *mut EeWonIpAddr, text: *const u8, length: usize) -> bool {
    // SAFETY: the caller's bytes
    ip_mut!(a).set_host_port(unsafe { bytes(text, length) })
}
/// `SetSixByte`.
///
/// # Safety
/// As `ee_won_ipaddr_clone`; `six` points at 6 bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_set_six(a: *mut EeWonIpAddr, six: *const u8) {
    // SAFETY: 6 bytes
    ip_mut!(a).set_six(unsafe { &*(six as *const [u8; 6]) })
}
/// `Fn2B2B00`.
///
/// # Safety
/// As `ee_won_ipaddr_clone`; `sa` points at a `PlatSockAddrIn`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_set_sockaddr(a: *mut EeWonIpAddr, sa: *const c_void) {
    // SAFETY: the caller's sockaddr (unaligned reads are fine)
    let sa = unsafe { std::ptr::read_unaligned(sa as *const PlatSockAddrIn) };
    ip_mut!(a).set_sockaddr(sa)
}
/// `Fn2B2C20` (host order).
///
/// # Safety
/// As `ee_won_ipaddr_clone`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_set_host_order(a: *mut EeWonIpAddr, host: u32, port: u16) {
    ip_mut!(a).set_host_order(host, port)
}
/// `SetWithDefaultPortNoDNS`.
///
/// # Safety
/// As `ee_won_ipaddr_set_resolve`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_set_no_dns(
    a: *mut EeWonIpAddr,
    text: *const u8,
    length: usize,
    default_port: u16,
) -> bool {
    // SAFETY: the caller's bytes
    ip_mut!(a).set_no_dns(unsafe { bytes(text, length) }, default_port)
}
/// `SetWithDefaultPort`.
///
/// # Safety
/// As `ee_won_ipaddr_set_resolve`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_set_with_default_port(
    a: *mut EeWonIpAddr,
    text: *const u8,
    length: usize,
    default_port: u16,
) -> bool {
    // SAFETY: the caller's bytes
    ip_mut!(a).set_with_default_port(unsafe { bytes(text, length) }, default_port)
}
/// `GetHostString` to `sink`.
///
/// # Safety
/// As `ee_won_ipaddr_clone`; the sink is called once during the call.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_host_string(
    a: *const EeWonIpAddr,
    use_original: bool,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let s = ip!(a).host_string(use_original);
    // SAFETY: the sink's contract
    unsafe { sink(context, s.as_ptr(), s.len()) };
}
/// `GetHostAndPortString` to `sink`.
///
/// # Safety
/// As `ee_won_ipaddr_host_string`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_host_and_port_string(
    a: *const EeWonIpAddr,
    use_original: bool,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let s = ip!(a).host_and_port_string(use_original);
    // SAFETY: the sink's contract
    unsafe { sink(context, s.as_ptr(), s.len()) };
}
/// `GetPort`.
///
/// # Safety
/// As `ee_won_ipaddr_clone`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_port(a: *const EeWonIpAddr) -> u16 {
    ip!(a).port()
}
/// `SetPort`.
///
/// # Safety
/// As `ee_won_ipaddr_clone`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_set_port(a: *mut EeWonIpAddr, port: u16) {
    ip_mut!(a).set_port(port)
}
/// `GetHost`.
///
/// # Safety
/// As `ee_won_ipaddr_clone`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_host(a: *const EeWonIpAddr) -> c_long {
    ip!(a).host() as c_long
}
/// `IsValid`.
///
/// # Safety
/// As `ee_won_ipaddr_clone`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_valid(a: *const EeWonIpAddr) -> bool {
    ip!(a).valid
}
/// `operator<` / `==`: negative, 0 or positive.
///
/// # Safety
/// As `ee_won_ipaddr_clone`, for both.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_compare(a: *const EeWonIpAddr, b: *const EeWonIpAddr) -> i32 {
    ip!(a).compare(ip!(b)) as i32
}
/// `StringCompare`.
///
/// # Safety
/// As `ee_won_ipaddr_compare`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_string_compare_values(a: *const EeWonIpAddr, b: *const EeWonIpAddr) -> i32 {
    ip!(a).string_compare(ip!(b))
}
/// `GetLocalHost`.
#[no_mangle]
#[allow(clippy::unnecessary_cast)] // as ee_won_ipaddr_set_local_host
pub extern "C" fn ee_won_ipaddr_local_host() -> c_long {
    local_host() as c_long
}
/// `SetLocalHost`.
#[no_mangle]
#[allow(clippy::unnecessary_cast)] // `long` is 64-bit on POSIX
pub extern "C" fn ee_won_ipaddr_set_local_host(host: c_long, validate: bool) -> bool {
    set_local_host(host as i32, validate)
}
/// `GetLocalAddr`: a new value.
#[no_mangle]
pub extern "C" fn ee_won_ipaddr_local_addr() -> *mut EeWonIpAddr {
    Box::into_raw(Box::new(local_addr()))
}
/// ServerContext's `ToRust`: the value as servctx.rs's `EeWonAddr` (the host borrowed while the value lives).
///
/// # Safety
/// As `ee_won_ipaddr_clone`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_to_raw(a: *const EeWonIpAddr) -> EeWonAddr {
    let v = ip!(a);
    let mut sockaddr = [0u8; 16];
    // SAFETY: a 16-byte repr(C) sockaddr
    sockaddr.copy_from_slice(unsafe { std::slice::from_raw_parts((&v.sa as *const PlatSockAddrIn).cast::<u8>(), 16) });
    EeWonAddr {
        sockaddr,
        valid: v.valid,
        remember_host: v.remember_host,
        host: v.host.as_ptr(),
        host_len: v.host.len(),
    }
}
/// ServerContext's `FromRust`: the value set from an `EeWonAddr`.
///
/// # Safety
/// As `ee_won_ipaddr_clone`; `raw` is valid, its host `host_len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ipaddr_from_raw(a: *mut EeWonIpAddr, raw: *const EeWonAddr) {
    // SAFETY: the caller's value
    let r = unsafe { &*raw };
    let v = ip_mut!(a);
    // SAFETY: 16 bytes into a repr(C) sockaddr
    v.sa = unsafe { std::ptr::read_unaligned(r.sockaddr.as_ptr() as *const PlatSockAddrIn) };
    v.valid = r.valid;
    v.remember_host = r.remember_host;
    // SAFETY: the caller's host bytes
    v.host = unsafe { bytes(r.host, r.host_len) }.to_vec();
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn values_parse_print_order_and_copy() {
        crate::socket::start();
        let mut a = EeWonIpAddr::default();
        assert!(!a.valid && a.host().eq(&0) && a.port() == 0);
        assert!(a.set_host_port(b"127.0.0.1:1234"));
        assert_eq!((a.host(), a.port(), a.valid), (0x7f00_0001, 1234, true));
        assert_eq!(a.host_string(true), b"127.0.0.1");
        assert_eq!(a.host_and_port_string(true), b"127.0.0.1:1234");
        // a remembered host
        let mut r = EeWonIpAddr { remember_host: true, ..Default::default() };
        assert!(r.set_resolve(b"127.0.0.1", 80));
        assert_eq!(r.host_string(true), b"127.0.0.1");
        assert_eq!(r.host, b"127.0.0.1");
        r.set_host_order(0x0a00_0001, 9);
        assert!(r.host.is_empty(), "a numeric set forgets the host");
        assert_eq!(r.host_string(true), b"10.0.0.1");
        // no colon: invalid, the address kept
        assert!(!a.set_host_port(b"127.0.0.1"));
        assert!(!a.valid && a.port() == 1234);
        // no DNS
        assert!(a.set_no_dns(b"10.1.2.3", 77));
        assert_eq!((a.host(), a.port()), (0x0a01_0203, 77));
        assert!(!a.set_no_dns(b"name", 77) && !a.valid && a.host() == 0x0a01_0203);
        // the wire form and the order (port first)
        let mut s = EeWonIpAddr::default();
        s.set_six(&[0x04, 0xd2, 127, 0, 0, 1]);
        assert_eq!((s.host(), s.port()), (0x7f00_0001, 1234));
        let mut t = EeWonIpAddr::default();
        t.set_host_order(0x0100_0000, 1235);
        assert_eq!(s.compare(&t), std::cmp::Ordering::Less);
        assert!(s.string_compare(&t) > 0, "by address first");
        // copies are values
        let c = r.clone();
        r.set_port(1);
        assert_ne!(c, r);
        // the C ABI's copy, assignment and raw form
        let p = ee_won_ipaddr_new();
        unsafe {
            ee_won_ipaddr_set_host_order(p, 0x7f00_0001, 99);
            let q = ee_won_ipaddr_clone(p);
            ee_won_ipaddr_set_port(p, 100);
            assert_eq!((ee_won_ipaddr_port(q), ee_won_ipaddr_port(p)), (99, 100));
            ee_won_ipaddr_assign(q, p);
            ee_won_ipaddr_assign(q, q);
            assert_eq!(ee_won_ipaddr_compare(p, q), 0);
            let raw = ee_won_ipaddr_to_raw(p);
            let z = ee_won_ipaddr_new();
            ee_won_ipaddr_from_raw(z, &raw);
            assert_eq!(*z, *p);
            for x in [p, q, z] {
                ee_won_ipaddr_free(x);
            }
        }
        // the local host
        assert!(set_local_host(0, true));
        assert_eq!(local_host(), 0);
        assert!(!set_local_host(0x0102_0304, true), "not this machine's");
        assert!(set_local_host(0x7f00_0002, false));
        assert_eq!(local_addr().host(), 0x7f00_0002);
        assert!(set_local_host(0, false));
    }
}
