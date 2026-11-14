//! `AuthContext` whole in Rust (net round 20): its lock (`mDataCrit`, a Win32 critical section: recursive on its
//! owner thread), its sessions and certificate flow (authflow.rs), its login data (authlogin.rs), its server list (a
//! C++ `ServerContext`, one reference held) and the class's signing public key (`mVerifierKey` with its
//! file-checked flag and lock: [`verifier`]). The C++ class is a `RefCount` shim over one reference to an
//! [`EeWonAuthCtx`]; every method forwards here and takes the lock where the original took it. The Rust op base
//! answers the certificate ops' calls on it directly (hostauth.rs).
//!
//! The verifier key is made without a draw from the system seed: the original's static was constructed before
//! `main`, so no run (the drivers' fixed seeds included) ever saw its draw.
//!
//! Reached by the Internet login and by a direct-connect LAN game with the CD key check (its certificate refresh);
//! every LAN session constructs one (the lobby's auth context): its peer data, its login file name, its server list
//! and the verifier file check.

use std::ffi::{c_long, c_void};
use std::sync::{Arc, Condvar, Mutex, MutexGuard};
use std::thread::ThreadId;

use crate::authflow::{self, EeWonAuthFlow, EeWonAuthFlowHooks, EeWonLightCert};
use crate::authlogin::{self, EeWonAuthLogin};
use crate::elgamal::ElGamal;
use crate::ipobj::EeWonIpAddr;
use crate::peerobj::{self, unix_now, EeWonAuthObj, PeerData};
use crate::random::Random;

fn lock<T>(m: &Mutex<T>) -> MutexGuard<'_, T> {
    m.lock().unwrap_or_else(|e| e.into_inner())
}

/// A recursive lock (`CriticalSection`): its owner may take it again.
#[derive(Default)]
pub(crate) struct ReLock {
    state: Mutex<(Option<ThreadId>, u32)>,
    free: Condvar,
}

/// The lock held (`AutoCrit`).
pub(crate) struct ReGuard<'a>(&'a ReLock);

impl ReLock {
    pub(crate) fn lock(&self) -> ReGuard<'_> {
        let me = std::thread::current().id();
        let mut s = lock(&self.state);
        loop {
            match s.0 {
                None => {
                    *s = (Some(me), 1);
                    break;
                }
                Some(t) if t == me => {
                    s.1 += 1;
                    break;
                }
                Some(_) => s = self.free.wait(s).unwrap_or_else(|e| e.into_inner()),
            }
        }
        ReGuard(self)
    }
}

impl Drop for ReGuard<'_> {
    fn drop(&mut self) {
        let mut s = lock(&self.0.state);
        s.1 -= 1;
        if s.1 == 0 {
            s.0 = None;
            self.0.free.notify_one();
        }
    }
}

/// `mServerContext`: a C++ `ServerContext*` (one reference, 0: none), released last.
struct Servers {
    obj: Mutex<usize>,
    hooks: EeWonAuthFlowHooks,
}

impl Drop for Servers {
    fn drop(&mut self) {
        let p = std::mem::take(&mut *lock(&self.obj));
        if p != 0 {
            // SAFETY: the reference this slot held
            unsafe { (self.hooks.release)(p as *mut c_void) };
        }
    }
}

/// `AuthContext`: the members in the original's destruction order (the flow, the login data, the server list).
pub struct EeWonAuthCtx {
    lock: ReLock,
    flow: EeWonAuthFlow,
    login: Box<EeWonAuthLogin>,
    servers: Servers,
}

// SAFETY: the C++ objects it holds (sessions, ops, the server list) are reached only through their hooks, from
// whichever thread calls, under this object's lock where the original took its own, as the original's were
unsafe impl Send for EeWonAuthCtx {}
// SAFETY: as above
unsafe impl Sync for EeWonAuthCtx {}

impl EeWonAuthCtx {
    /// `Init()`'s Rust part: the flow over the hooks, the login data with its secret file, an empty peer data
    /// (`new AuthPeerData`).
    pub(crate) fn new(hooks: EeWonAuthFlowHooks, secret_file: &[u8]) -> EeWonAuthCtx {
        // SAFETY: the bytes given; the box ee_won_auth_login_new made
        let login = unsafe { Box::from_raw(authlogin::ee_won_auth_login_new(secret_file.as_ptr(), secret_file.len())) };
        let c = EeWonAuthCtx {
            lock: ReLock::default(),
            flow: EeWonAuthFlow::new(hooks),
            login,
            servers: Servers { obj: Mutex::new(0), hooks },
        };
        c.flow.set_peer_data(Some(Arc::new(PeerData::empty(None))));
        c
    }

    /// The login data (its own lock).
    pub(crate) fn login(&self) -> *const EeWonAuthLogin {
        &*self.login
    }

    /// `GetPeerData()`.
    pub(crate) fn peer_data(&self) -> Option<Arc<PeerData>> {
        let _l = self.lock.lock();
        self.flow.peer_data()
    }

    /// `SetPeerData(data)`.
    pub(crate) fn set_peer_data(&self, peer: Option<Arc<PeerData>>) {
        let _l = self.lock.lock();
        self.flow.set_peer_data(peer)
    }

    /// `IsExpired(extra)`.
    pub(crate) fn is_expired(&self, extra: i64) -> bool {
        let _l = self.lock.lock();
        self.flow.is_expired(extra, unix_now())
    }

    /// `PutSession(address, session)`.
    pub(crate) fn put_session(&self, key: [u8; 6], session: *mut c_void) {
        let _l = self.lock.lock();
        if !session.is_null() {
            self.flow.put_session(key, session, unix_now())
        }
    }

    /// `GetSession(address)`: a reference the caller adopts, or NULL.
    pub(crate) fn get_session(&self, key: [u8; 6]) -> *mut c_void {
        let _l = self.lock.lock();
        self.flow.get_session(key, unix_now())
    }

    /// `AutoRefreshCallback(op)`.
    pub(crate) fn auto_refresh_callback(&self, op: *mut c_void) {
        let _l = self.lock.lock();
        self.flow.auto_refresh_callback(op)
    }

    /// `StopAutoRefresh()`.
    pub(crate) fn stop_auto_refresh(&self) {
        let _l = self.lock.lock();
        self.flow.stop_auto_refresh()
    }

    /// `SetDoAutoRefresh(enabled)`.
    pub(crate) fn set_do_auto_refresh(&self, enabled: bool) {
        let _l = self.lock.lock();
        self.flow.set_do_auto_refresh(enabled, unix_now())
    }

    /// `Kill()`.
    pub(crate) fn kill(&self) {
        let _l = self.lock.lock();
        self.flow.kill()
    }

    /// `StartAutoRefresh(force)`.
    pub(crate) fn start_auto_refresh(&self, force: bool) {
        let _l = self.lock.lock();
        self.flow.start_auto_refresh(force, unix_now())
    }

    /// `GetCertCallback(op)`.
    pub(crate) fn get_cert_callback(&self, op: *mut c_void) {
        let _l = self.lock.lock();
        self.flow.get_cert_callback(op, unix_now())
    }

    /// `RefreshAsync(op, cert, timeout)`.
    pub(crate) fn refresh_async(&self, light: *mut c_void, cert: Option<Arc<EeWonLightCert>>, timeout: u32) {
        let _l = self.lock.lock();
        self.flow.refresh_async(light, cert, timeout)
    }

    /// `RefreshBlock(timeout)` (no lock, as the original).
    pub(crate) fn refresh_block(&self, timeout: u32) -> c_long {
        self.flow.refresh_block(timeout, unix_now())
    }

    /// `SetServerContext(context)`: a reference of its own.
    pub(crate) fn set_server_context(&self, context: *mut c_void) {
        let _l = self.lock.lock();
        if !context.is_null() {
            // SAFETY: a live ServerContext (contract)
            unsafe { (self.servers.hooks.add_ref)(context) };
        }
        let old = std::mem::replace(&mut *lock(&self.servers.obj), context as usize);
        if old != 0 {
            // SAFETY: the reference the slot held
            unsafe { (self.servers.hooks.release)(old as *mut c_void) };
        }
    }

    /// `GetServerContext()`: a new reference the caller adopts, or NULL.
    pub(crate) fn server_context(&self) -> *mut c_void {
        let _l = self.lock.lock();
        let p = *lock(&self.servers.obj) as *mut c_void;
        if !p.is_null() {
            // SAFETY: the slot's live object
            unsafe { (self.servers.hooks.add_ref)(p) };
        }
        p
    }
}

// ---- the verifier key (`AuthContext::mVerifierKey`, `mCheckedVerifierFile`, `GetVerifierCrit()`)

/// The built-in signing public key (0x566018): `VerifierKeyBuf`, with the string literal's NUL (`sizeof`).
const VERIFIER_KEY: [u8; 817] = [
    0x30, 0x82, 0x03, 0x2c, 0x02, 0x82, 0x01, 0x01, 0x00, 0x86, 0x18, 0x0e, 0x3d, 0xd4, 0x9e, 0xfa, 0x42, 0xb9, 0x4c,
    0xb3, 0x3e, 0xf7, 0x2b, 0x76, 0xce, 0xe6, 0xb0, 0x27, 0x8c, 0x64, 0x07, 0x93, 0x3b, 0x24, 0x64, 0x6c, 0x47, 0xb6,
    0x9e, 0x51, 0x7b, 0xbf, 0xc9, 0xd5, 0xa1, 0xfb, 0xcf, 0x04, 0xfa, 0x7b, 0x8e, 0xc3, 0x6e, 0xd1, 0x88, 0xf6, 0x2e,
    0xbd, 0x2b, 0x29, 0xda, 0xb5, 0x76, 0x66, 0x12, 0xb9, 0x7a, 0x64, 0x73, 0x19, 0x53, 0xa9, 0x78, 0xd8, 0x1e, 0xe5,
    0x11, 0x0a, 0x23, 0x1c, 0x08, 0x21, 0x33, 0xad, 0x65, 0x76, 0x48, 0xff, 0xc5, 0x0b, 0xa2, 0xaa, 0x10, 0x22, 0xcb,
    0x63, 0xbb, 0x68, 0xfc, 0xf0, 0x15, 0xf3, 0xe9, 0x90, 0x5f, 0x9d, 0x17, 0x8e, 0x77, 0x67, 0xec, 0xde, 0xdf, 0x8b,
    0xcc, 0xbb, 0xa4, 0xd6, 0x06, 0xfa, 0x0e, 0xd8, 0xc0, 0x67, 0xd4, 0x36, 0xc9, 0x3a, 0xeb, 0x3f, 0xa4, 0x93, 0x57,
    0xad, 0x62, 0x29, 0x24, 0x08, 0x67, 0x91, 0x6a, 0x0b, 0xd4, 0xdc, 0xad, 0xa4, 0x86, 0x85, 0x9f, 0xed, 0xf8, 0xb8,
    0xda, 0xe1, 0xac, 0xf3, 0xf4, 0x44, 0xaf, 0x85, 0x8d, 0x21, 0x95, 0x6a, 0xf4, 0x6e, 0xac, 0x36, 0x83, 0x8f, 0x70,
    0x0a, 0xc6, 0x17, 0xc1, 0x90, 0xc4, 0x7f, 0xbb, 0xe8, 0x18, 0xed, 0x4d, 0x0b, 0x8c, 0xdc, 0x76, 0x7f, 0xa9, 0xb2,
    0x45, 0x0f, 0xc6, 0xa8, 0xca, 0x0a, 0x9f, 0xec, 0x9f, 0xe5, 0xe0, 0xbd, 0x71, 0x63, 0x06, 0xef, 0x32, 0xba, 0xe0,
    0x40, 0x47, 0xf1, 0x91, 0xa8, 0xbf, 0x5e, 0xad, 0xb8, 0x2d, 0x28, 0x3a, 0x94, 0x63, 0xfa, 0x85, 0x1d, 0x24, 0xe3,
    0xd2, 0x78, 0x7f, 0x0e, 0x85, 0xb1, 0xbb, 0x3f, 0x2d, 0x27, 0x16, 0x34, 0xcf, 0x94, 0x7d, 0x71, 0xdd, 0xf3, 0xec,
    0x41, 0xc1, 0x27, 0xe5, 0x6f, 0xe6, 0x88, 0x78, 0xc8, 0x65, 0x15, 0x45, 0x3c, 0x0c, 0x5f, 0x30, 0x60, 0xc7, 0x02,
    0x1d, 0x02, 0xe1, 0x4f, 0x25, 0x13, 0x97, 0xee, 0xdb, 0x46, 0x94, 0xb2, 0x54, 0xfc, 0x54, 0x26, 0x20, 0xa2, 0x6d,
    0x57, 0x72, 0xdd, 0x47, 0xd2, 0xf0, 0x92, 0x00, 0x38, 0x51, 0xad, 0x02, 0x82, 0x01, 0x00, 0x72, 0x44, 0x9d, 0x49,
    0x40, 0xff, 0x84, 0x75, 0xd8, 0x73, 0x57, 0x1f, 0xa7, 0xff, 0x52, 0x08, 0x82, 0x63, 0xfa, 0xd1, 0xa7, 0x30, 0xb8,
    0x63, 0xc5, 0x10, 0x45, 0x8e, 0x12, 0xca, 0x9c, 0x36, 0x4f, 0xc8, 0x3e, 0xe8, 0x9a, 0x95, 0xfc, 0x48, 0xae, 0xcf,
    0x80, 0xea, 0x75, 0xf6, 0x11, 0x3b, 0x2a, 0xf0, 0xd9, 0x52, 0xfb, 0x66, 0xee, 0x4b, 0xdb, 0x6c, 0xc4, 0x06, 0x28,
    0x12, 0x28, 0xd2, 0x2c, 0x53, 0x3f, 0x08, 0x86, 0x89, 0x69, 0x6a, 0x89, 0x57, 0x7e, 0xb0, 0xd7, 0x81, 0xac, 0x79,
    0xae, 0xfb, 0x56, 0xc0, 0x24, 0x0d, 0x84, 0x5a, 0xe1, 0x22, 0xc8, 0x3f, 0xba, 0x3c, 0x77, 0xbc, 0x22, 0x56, 0xdf,
    0xf1, 0x78, 0x55, 0xcc, 0xd5, 0x97, 0x88, 0x00, 0x71, 0x1a, 0xe8, 0x8e, 0xa7, 0xb6, 0x87, 0x62, 0x55, 0x4d, 0x53,
    0x87, 0xa2, 0x49, 0x3e, 0xa2, 0x79, 0x99, 0x51, 0x94, 0xd7, 0xe2, 0xf5, 0xce, 0x37, 0x1e, 0xc7, 0x79, 0xbd, 0xea,
    0x0a, 0x31, 0x56, 0x1d, 0x56, 0x1a, 0x8c, 0xb7, 0xf3, 0xa5, 0xdc, 0x2a, 0xcd, 0xdc, 0xb1, 0x5c, 0x3e, 0x47, 0x20,
    0xc7, 0xc2, 0x30, 0x32, 0xdf, 0x64, 0x91, 0xd5, 0xc6, 0xab, 0x85, 0x21, 0x67, 0x59, 0xc6, 0x09, 0x7c, 0x61, 0xd5,
    0x90, 0x9b, 0xff, 0x5c, 0x6b, 0x48, 0x5e, 0x41, 0x85, 0xdb, 0x12, 0xed, 0x63, 0x04, 0xd3, 0x35, 0xca, 0x86, 0xce,
    0x64, 0xa6, 0x54, 0x5b, 0x0f, 0x77, 0x90, 0x7e, 0xad, 0xd9, 0x56, 0x5d, 0x2f, 0x8a, 0x52, 0xd9, 0xcc, 0x79, 0x41,
    0x69, 0x73, 0xca, 0x15, 0x0a, 0xbc, 0x68, 0x93, 0xbe, 0x2f, 0x03, 0x7c, 0x60, 0x7b, 0xf2, 0xa3, 0x03, 0x98, 0x34,
    0x36, 0x9f, 0x88, 0x8e, 0xfb, 0x2b, 0x8a, 0x40, 0xdd, 0x35, 0xa9, 0x21, 0x2d, 0x92, 0xba, 0x1b, 0xe8, 0x58, 0xe1,
    0x1c, 0xc2, 0x47, 0x34, 0x9c, 0x02, 0x82, 0x01, 0x00, 0x7f, 0xb4, 0x68, 0x1d, 0xde, 0x12, 0xd3, 0x00, 0x3a, 0x26,
    0xb6, 0x60, 0x7e, 0xb9, 0x62, 0x9f, 0x31, 0xcb, 0x6b, 0x3a, 0x4f, 0x0e, 0x18, 0x64, 0x4f, 0x65, 0xad, 0x79, 0x1d,
    0xd7, 0xe8, 0xbf, 0x83, 0x68, 0x78, 0xc4, 0x9e, 0x2b, 0x90, 0xcd, 0xd8, 0xad, 0x36, 0x1a, 0x1d, 0x4f, 0x56, 0x4b,
    0x8a, 0x6f, 0xc3, 0x77, 0x93, 0x5e, 0xf4, 0x7d, 0x61, 0xfd, 0x48, 0xa5, 0xa5, 0xbe, 0x27, 0xa3, 0x5d, 0xa9, 0x0a,
    0xb0, 0x77, 0xc9, 0x6d, 0x8d, 0xae, 0x94, 0x5a, 0x90, 0x07, 0xb4, 0x96, 0x1b, 0xda, 0x93, 0x56, 0xc5, 0x50, 0x19,
    0xdd, 0xa4, 0xb3, 0x40, 0x66, 0x0b, 0xb0, 0x55, 0xdd, 0xd7, 0x9a, 0x50, 0x7b, 0x9a, 0xe7, 0xbc, 0x63, 0x65, 0x9b,
    0x45, 0x2d, 0x82, 0xf3, 0xe7, 0x49, 0x19, 0x71, 0x10, 0x9d, 0x46, 0x2e, 0x82, 0x8e, 0x15, 0x35, 0xf8, 0x53, 0x99,
    0x3f, 0x4f, 0xec, 0x83, 0x48, 0xef, 0x0f, 0x27, 0x28, 0x75, 0x1f, 0x73, 0x97, 0x1e, 0x14, 0x16, 0x24, 0x16, 0xe6,
    0x3f, 0x67, 0xdc, 0x50, 0x4a, 0xdc, 0x83, 0x89, 0x47, 0xf9, 0x41, 0xa5, 0xec, 0xe1, 0x86, 0x08, 0x46, 0xf4, 0xa9,
    0x97, 0x14, 0x73, 0xaf, 0xe6, 0x01, 0x30, 0xa1, 0xad, 0x2f, 0xb0, 0x2a, 0x35, 0xdd, 0xad, 0x1d, 0x70, 0x25, 0x95,
    0xbf, 0x07, 0xd2, 0x8c, 0x14, 0x46, 0xc4, 0xd2, 0x79, 0x21, 0xdc, 0xa9, 0x21, 0x6e, 0xff, 0x99, 0x69, 0x47, 0x8b,
    0x67, 0x95, 0x98, 0x4d, 0x09, 0x75, 0xee, 0x97, 0x0b, 0xd9, 0x3d, 0x75, 0xd1, 0xa8, 0x82, 0x1f, 0xc8, 0x15, 0x2e,
    0x39, 0x29, 0x6f, 0x95, 0x4b, 0xf4, 0x94, 0x74, 0xeb, 0xd8, 0xe3, 0x4a, 0xfb, 0x41, 0xa8, 0x4c, 0x5c, 0x3d, 0xb6,
    0x42, 0xc0, 0x15, 0x35, 0x9e, 0x1a, 0xe2, 0x54, 0x2c, 0x0c, 0x0b, 0x34, 0x5f, 0xe2, 0x35, 0x8d, 0x30, 0x73, 0x00,
];

struct Verifier {
    key: ElGamal,
    checked: bool,
}

static VERIFIER: Mutex<Option<Verifier>> = Mutex::new(None);

/// The verifier key and its checked flag under their lock: made from the built-in key on first use (no random
/// draw).
pub(crate) fn verifier<R>(f: impl FnOnce(&mut ElGamal, &mut bool) -> R) -> R {
    let mut g = lock(&VERIFIER);
    let v = g.get_or_insert_with(|| {
        let mut key = ElGamal::with_random(Random::default());
        key.set_public_key(&VERIFIER_KEY);
        Verifier { key, checked: false }
    });
    f(&mut v.key, &mut v.checked)
}

/// `LoadVerifierKey(filename)`: the file checked, then its bytes read whole (`PlatFOpen(.., "rb")`); false when it
/// does not open, else the key's `SetPublicKey`.
pub(crate) fn load_verifier(path: &[u8]) -> bool {
    verifier(|_, checked| *checked = true);
    let end = path.iter().position(|&b| b == 0).unwrap_or(path.len());
    let Some(bytes) = authlogin::read_file(&path[..end], c"rb") else {
        return false;
    };
    verifier(|key, _| key.set_public_key(&bytes))
}

// ---- the C ABI (AuthContext.cpp; `c` is from `ee_won_authctx_new`)

/// # Safety
/// `c` is a live context (a pointer from `ee_won_authctx_new`).
unsafe fn ctx<'a>(c: *const EeWonAuthCtx) -> &'a EeWonAuthCtx {
    // SAFETY: the caller's live context
    unsafe { &*c }
}

/// Another reference to a borrowed context (NULL: none).
///
/// # Safety
/// `c` is NULL or a live context.
pub(crate) unsafe fn ctx_ref(c: *const EeWonAuthCtx) -> Option<Arc<EeWonAuthCtx>> {
    if c.is_null() {
        return None;
    }
    // SAFETY: a live Arc's pointer (contract): one more count, then an Arc for it
    unsafe {
        Arc::increment_strong_count(c);
        Some(Arc::from_raw(c))
    }
}

/// A new auth context (one reference, `ee_won_authctx_free`): `Init()`'s flow over `hooks` (whose context is the C++
/// `AuthContext`), its login data with the secret file `secret_file` and its empty peer data.
///
/// # Safety
/// `hooks` points to a hook table whose context outlives the context; `secret_file` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_new(
    hooks: *const EeWonAuthFlowHooks,
    secret_file: *const u8,
    len: usize,
) -> *const EeWonAuthCtx {
    // SAFETY: the caller's table and bytes
    let c = EeWonAuthCtx::new(unsafe { *hooks }, unsafe { crate::ffi::bytes(secret_file, len) });
    Arc::into_raw(Arc::new(c))
}

/// Drops a reference (NULL: nothing); the last one releases the members.
///
/// # Safety
/// `c` is NULL or a reference from `ee_won_authctx_new`, not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_free(c: *const EeWonAuthCtx) {
    if !c.is_null() {
        // SAFETY: the caller's reference
        drop(unsafe { Arc::from_raw(c) });
    }
}

/// The login data (borrowed, for `ee_won_auth_login_*`).
///
/// # Safety
/// `c` is a live context.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_login(c: *const EeWonAuthCtx) -> *const EeWonAuthLogin {
    unsafe { ctx(c) }.login()
}

/// `PutSession(address, session)` (a session's `RefCount*`; NULL: nothing).
///
/// # Safety
/// `c` a live context, `a` a live IPAddr value, `session` NULL or a live session.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_put_session(
    c: *const EeWonAuthCtx,
    a: *const EeWonIpAddr,
    session: *mut c_void,
) {
    unsafe { ctx(c) }.put_session(authflow::addr_key(a), session)
}

/// `GetSession(address)`: a reference the caller adopts, or NULL.
///
/// # Safety
/// `c` a live context, `a` a live IPAddr value.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_get_session(c: *const EeWonAuthCtx, a: *const EeWonIpAddr) -> *mut c_void {
    unsafe { ctx(c) }.get_session(authflow::addr_key(a))
}

/// `GetPeerData()`: a new reference (`ee_won_authobj_free`), NULL for none.
///
/// # Safety
/// `c` a live context.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_peer_data(c: *const EeWonAuthCtx) -> *mut EeWonAuthObj {
    unsafe { ctx(c) }.peer_data().map_or(std::ptr::null_mut(), peerobj::peer_ref)
}

/// `SetPeerData(data)` (NULL, or anything but peer data: none).
///
/// # Safety
/// `c` a live context, `peer` NULL or a live object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_set_peer_data(c: *const EeWonAuthCtx, peer: *const EeWonAuthObj) {
    // SAFETY: the caller's object
    unsafe { ctx(c) }.set_peer_data(unsafe { peerobj::peer_of(peer) })
}

/// `IsExpired(extra)`.
///
/// # Safety
/// `c` a live context.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_is_expired(c: *const EeWonAuthCtx, extra: i64) -> bool {
    unsafe { ctx(c) }.is_expired(extra)
}

/// `AutoRefreshCallback(op)` (the completing timer's `RefCount*`).
///
/// # Safety
/// `c` a live context, `op` a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_auto_refresh_callback(c: *const EeWonAuthCtx, op: *mut c_void) {
    unsafe { ctx(c) }.auto_refresh_callback(op)
}

/// `StopAutoRefresh()`.
///
/// # Safety
/// `c` a live context.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_stop_auto_refresh(c: *const EeWonAuthCtx) {
    unsafe { ctx(c) }.stop_auto_refresh()
}

/// `SetDoAutoRefresh(enabled)`.
///
/// # Safety
/// `c` a live context.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_set_do_auto_refresh(c: *const EeWonAuthCtx, enabled: bool) {
    unsafe { ctx(c) }.set_do_auto_refresh(enabled)
}

/// `Kill()`.
///
/// # Safety
/// `c` a live context.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_kill(c: *const EeWonAuthCtx) {
    unsafe { ctx(c) }.kill()
}

/// `StartAutoRefresh(force)`.
///
/// # Safety
/// `c` a live context.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_start_auto_refresh(c: *const EeWonAuthCtx, force: bool) {
    unsafe { ctx(c) }.start_auto_refresh(force)
}

/// `GetCertCallback(op)` (the completing `GetCertOp`'s `RefCount*`).
///
/// # Safety
/// `c` a live context, `op` a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_get_cert_callback(c: *const EeWonAuthCtx, op: *mut c_void) {
    unsafe { ctx(c) }.get_cert_callback(op)
}

/// `RefreshAsync(op, cert, timeout)`: `light` NULL or a waiting op's run, `cert` its member (borrowed).
///
/// # Safety
/// `c` a live context; `light` NULL or a live op; `cert` NULL or from `ee_won_light_cert_new`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_refresh_async(
    c: *const EeWonAuthCtx,
    light: *mut c_void,
    cert: *const EeWonLightCert,
    timeout: u32,
) {
    // SAFETY: the caller's object
    unsafe { ctx(c) }.refresh_async(light, unsafe { authflow::light_ref(cert) }, timeout)
}

/// `RefreshBlock(timeout)`: the status.
///
/// # Safety
/// `c` a live context.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_refresh_block(c: *const EeWonAuthCtx, timeout: u32) -> c_long {
    unsafe { ctx(c) }.refresh_block(timeout)
}

/// `SetServerContext(context)` (a `ServerContext`'s `RefCount*`, NULL: none; a reference of its own).
///
/// # Safety
/// `c` a live context, `servers` NULL or a live object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_set_server_context(c: *const EeWonAuthCtx, servers: *mut c_void) {
    unsafe { ctx(c) }.set_server_context(servers)
}

/// `GetServerContext()`: a new reference the caller adopts, or NULL.
///
/// # Safety
/// `c` a live context.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_server_context(c: *const EeWonAuthCtx) -> *mut c_void {
    unsafe { ctx(c) }.server_context()
}

/// `Init()`'s verifier file: unless a file was checked already (in this process), `LoadVerifierKey(dir +
/// "_wonkver.pub")`.
///
/// # Safety
/// `dir` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_verifier_init(dir: *const u8, len: usize) {
    if verifier(|_, checked| *checked) {
        return;
    }
    // SAFETY: the caller's bytes
    let mut path = unsafe { crate::ffi::bytes(dir, len) }.to_vec();
    path.extend_from_slice(b"_wonkver.pub");
    load_verifier(&path);
}

/// `AuthContext::LoadVerifierKey(filename)`.
///
/// # Safety
/// `path` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_verifier_load(path: *const u8, len: usize) -> bool {
    // SAFETY: the caller's bytes
    load_verifier(unsafe { crate::ffi::bytes(path, len) })
}

#[cfg(test)]
#[path = "authobj_tests.rs"]
pub(crate) mod tests;
