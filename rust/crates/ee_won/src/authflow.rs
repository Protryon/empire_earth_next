//! `AuthContext`'s peer sessions and certificate flow (WONAuth/AuthContext.cpp) as a Rust object: the session cache
//! (a map from an address to its last use and its sessions, with the clean-up cursor that `PutSession` advances
//! five entries a call), the waiting `LightGetCertOp`s (a set in address order), the running `GetCertOp`, the
//! auto-refresh timer, the refresh status and the auto-refresh flag, and every step that touches them:
//! `PutSession`, `GetSession`, `SetPeerData`'s clear, `AutoRefreshCallback`, `StopAutoRefresh`,
//! `SetDoAutoRefresh`, `Kill`, `StartAutoRefresh`, `GetCertCallback`, `RefreshAsync` and `RefreshBlock`; and,
//! from round 15, the context's peer data (`mPeerData`: `GetPeerData`, `SetPeerData`, `IsExpired`, and the
//! auto-refresh's reads of its validity and expire delta) and `LightGetCertOp`'s own member, the peer data a
//! refresh hands it ([`EeWonLightCert`]; the op's run is a plain C++ `AsyncOp`).
//!
//! The C++ `AuthContext` keeps its lock (taken around each call where the original took it) and the login data;
//! the objects this flow holds (sessions, ops) are C++ `RefCount`s reached through [`EeWonAuthFlowHooks`], the
//! peer data is peerobj.rs's `PeerData`. No borrow of the state lives across a hook: an op's `Kill`, `Run` or `Finish` can
//! complete it inside the call, and its completion comes back into this object (the timer's into
//! `auto_refresh_callback`, the request's into `get_cert_callback`, a waiting op's into a new `refresh_async`).
//! References are dropped where the original's `SmartPtr` assignments dropped them, after the state's lock is let
//! go. The behaviour is `golden_authflow.txt`, recorded over the original's statements.
//!
//! Reached only by the internet login and peer authentication (the servers are gone): no game, tour or LAN game.

use std::collections::{BTreeMap, VecDeque};
use std::ffi::{c_long, c_ulong, c_void};
use std::sync::{Arc, Mutex, MutexGuard};

use crate::authctx;
use crate::ipobj::EeWonIpAddr;
use crate::peerobj::{self, EeWonAuthObj, PeerData};

/// The C++ side: `context` is the `AuthContext`; every object is a `RefCount*`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonAuthFlowHooks {
    pub context: *mut c_void,
    /// `RefCount::AddRef`
    pub add_ref: unsafe extern "C" fn(*mut c_void),
    /// `RefCount::Release`
    pub release: unsafe extern "C" fn(*mut c_void),
    /// `AuthSession::GetLastUseTime`
    pub session_last_use: unsafe extern "C" fn(*mut c_void) -> i64,
    /// `AsyncOp::Kill(true)`
    pub op_kill: unsafe extern "C" fn(*mut c_void),
    /// `AsyncOp::IsKilled`
    pub op_killed: unsafe extern "C" fn(*mut c_void) -> bool,
    /// `AsyncOp::GetStatus`
    pub op_status: unsafe extern "C" fn(*mut c_void) -> c_long,
    /// `AsyncOp::RunAsync`
    pub op_run_async: unsafe extern "C" fn(*mut c_void, c_long),
    /// `AsyncOp::Run(mode, timeout)`
    pub op_run: unsafe extern "C" fn(*mut c_void, c_long, c_ulong) -> c_long,
    /// `AsyncOp::Finish`
    pub op_finish: unsafe extern "C" fn(*mut c_void, c_long),
    /// a new `AsyncOp` (one reference) whose completion is `StaticAutoRefreshCallback` on the context
    pub new_timer: unsafe extern "C" fn(*mut c_void) -> *mut c_void,
    /// a new `GetCertOp` on the context (one reference), its completion `StaticGetCertCallback` when asked
    pub new_cert_op: unsafe extern "C" fn(*mut c_void, bool) -> *mut c_void,
    /// `GetCertOp::GetPeerData`: a new reference to its peer data, or NULL
    pub cert_peer_data: unsafe extern "C" fn(*mut c_void) -> *mut EeWonAuthObj,
}

/// `LightGetCertOp`'s member: the peer data `GetCertCallback` hands the waiting op (`SetPeerData`), read by its
/// owner when the op completes (`GetPeerData`). Shared (an `Arc`) by the owner and the context's refresh set; the
/// op's run (its `AsyncOp` base) is a C++ object beside it.
#[derive(Default)]
pub struct EeWonLightCert {
    peer: Mutex<Option<Arc<PeerData>>>,
}

impl EeWonLightCert {
    pub(crate) fn new() -> Arc<EeWonLightCert> {
        Arc::new(EeWonLightCert::default())
    }

    /// `GetPeerData()`.
    pub(crate) fn peer(&self) -> Option<Arc<PeerData>> {
        self.peer.lock().unwrap_or_else(|e| e.into_inner()).clone()
    }

    /// `SetPeerData(data)`.
    pub(crate) fn set_peer(&self, peer: Option<Arc<PeerData>>) {
        let old = std::mem::replace(&mut *self.peer.lock().unwrap_or_else(|e| e.into_inner()), peer);
        drop(old);
    }
}

/// A `RefCount*` this object holds one reference to.
type Obj = *mut c_void;

/// `SessionStruct`.
#[derive(Default)]
struct Record {
    last_use: i64,
    sessions: VecDeque<Obj>,
}

#[derive(Default)]
struct State {
    /// `mRefreshSet`: by the op's address (`SmartPtr::Comp`), the op and its member
    refresh: BTreeMap<usize, (Obj, Arc<EeWonLightCert>)>,
    /// `mGetCertOp`
    get_cert: Option<Obj>,
    /// `mAutoRefreshOp`
    auto_op: Option<Obj>,
    /// `mRefreshStatus`
    refresh_status: c_long,
    /// `mDoAutoRefresh`
    do_auto: bool,
    /// `mSessionMap`: by the address's 6-byte wire form (`IPAddr::operator<`)
    sessions: BTreeMap<[u8; 6], Record>,
    /// `mSessionCleanItr`: the entry's key, `None` for `end()`
    clean: Option<[u8; 6]>,
    /// `mPeerData` (None: the original's NULL, which it never held after `Init`)
    peer: Option<Arc<PeerData>>,
}

/// `AuthContext`'s sessions and certificate flow.
pub struct EeWonAuthFlow {
    hooks: EeWonAuthFlowHooks,
    state: Mutex<State>,
}

/// `INFINITE` as `RefreshAsync`'s `(long)timeout`.
const INFINITE: u32 = 0xffff_ffff;

fn next_key<K: Ord + Copy, V>(map: &BTreeMap<K, V>, key: K) -> Option<K> {
    use std::ops::Bound::{Excluded, Unbounded};
    map.range((Excluded(key), Unbounded)).next().map(|(k, _)| *k)
}

impl EeWonAuthFlow {
    pub(crate) fn new(hooks: EeWonAuthFlowHooks) -> Self {
        EeWonAuthFlow { hooks, state: Mutex::new(State { refresh_status: 0x186a4, ..State::default() }) }
    }

    fn lock(&self) -> MutexGuard<'_, State> {
        self.state.lock().unwrap_or_else(|e| e.into_inner())
    }

    fn ctx(&self) -> *mut c_void {
        self.hooks.context
    }

    fn release(&self, o: Option<Obj>) {
        if let Some(p) = o {
            // SAFETY: a reference this object held
            unsafe { (self.hooks.release)(p) }
        }
    }

    fn release_all(&self, objs: impl IntoIterator<Item = Obj>) {
        for p in objs {
            self.release(Some(p));
        }
    }

    #[cfg(test)]
    pub(crate) fn refresh_status(&self) -> c_long {
        self.lock().refresh_status
    }

    /// `PutSession`: the session (a new reference) appended to the address's list and its last use set; then up to
    /// five steps of the clean-up cursor, each dropping an entry unused for over 300 seconds (or wrapping to the
    /// first entry at the end, which ends the walk).
    pub(crate) fn put_session(&self, key: [u8; 6], session: Obj, now: i64) {
        if session.is_null() {
            return;
        }
        // SAFETY: a live session (the list's reference)
        unsafe { (self.hooks.add_ref)(session) };
        let mut dropped = Vec::new();
        {
            let mut s = self.lock();
            let r = s.sessions.entry(key).or_default();
            r.last_use = now;
            r.sessions.push_back(session);
            for _ in 0..5 {
                let Some(at) = s.clean else {
                    s.clean = s.sessions.keys().next().copied();
                    break;
                };
                let next = next_key(&s.sessions, at);
                if now.wrapping_sub(s.sessions[&at].last_use) > 300 {
                    let r = s.sessions.remove(&at).unwrap_or_default();
                    dropped.push(r.sessions);
                }
                s.clean = next.or(next);
            }
        }
        for list in dropped {
            self.release_all(list);
        }
    }

    /// `GetSession`: the address's entry gets its last use; its sessions are taken from the front until one was
    /// used within 300 seconds (returned, a reference the caller adopts); NULL when none.
    pub(crate) fn get_session(&self, key: [u8; 6], now: i64) -> Obj {
        {
            let mut s = self.lock();
            let Some(r) = s.sessions.get_mut(&key) else {
                return std::ptr::null_mut();
            };
            r.last_use = now;
        }
        loop {
            let next = {
                let mut s = self.lock();
                match s.sessions.get_mut(&key) {
                    Some(r) => r.sessions.pop_front(),
                    None => None,
                }
            };
            let Some(session) = next else {
                return std::ptr::null_mut();
            };
            // SAFETY: a live session this object held
            let used = unsafe { (self.hooks.session_last_use)(session) };
            if now.wrapping_sub(used) < 300 {
                return session;
            }
            self.release(Some(session));
        }
    }

    /// `GetPeerData`.
    pub(crate) fn peer_data(&self) -> Option<Arc<PeerData>> {
        self.lock().peer.clone()
    }

    /// `IsExpired(extra)` at `now`: the second certificate's (no peer data: expired).
    pub(crate) fn is_expired(&self, extra: i64, now: i64) -> bool {
        self.lock().peer.as_ref().is_none_or(|p| p.cert2_expired(now, extra))
    }

    /// `SetPeerData`: the peer data replaced, then every cached session dropped, the cursor at the end.
    pub(crate) fn set_peer_data(&self, peer: Option<Arc<PeerData>>) {
        let (old, map) = {
            let mut s = self.lock();
            let old = std::mem::replace(&mut s.peer, peer);
            s.clean = None;
            (old, std::mem::take(&mut s.sessions))
        };
        drop(old);
        for (_, r) in map {
            self.release_all(r.sessions);
        }
    }

    /// `AutoRefreshCallback`: only the current timer counts; it is dropped, and unless it was killed a refresh
    /// starts.
    pub(crate) fn auto_refresh_callback(&self, op: Obj) {
        let old = {
            let mut s = self.lock();
            if s.auto_op != Some(op) {
                return;
            }
            s.auto_op.take()
        };
        self.release(old);
        // SAFETY: the completing op, alive for its completion
        if unsafe { (self.hooks.op_killed)(op) } {
            return;
        }
        self.refresh_async(std::ptr::null_mut(), None, INFINITE);
    }

    /// `StopAutoRefresh`: the timer killed (its completion may come back inside), then dropped.
    pub(crate) fn stop_auto_refresh(&self) {
        let Some(op) = self.lock().auto_op else {
            return;
        };
        // SAFETY: the held timer
        unsafe { (self.hooks.op_kill)(op) };
        let old = self.lock().auto_op.take();
        self.release(old);
    }

    /// `SetDoAutoRefresh` (`now` the time the peer data's expire delta is read at).
    pub(crate) fn set_do_auto_refresh(&self, enabled: bool, now: i64) {
        self.lock().do_auto = enabled;
        self.start_auto_refresh(false, now);
    }

    /// `Kill`: the running request killed, then the timer stopped.
    pub(crate) fn kill(&self) {
        let op = self.lock().get_cert;
        if let Some(op) = op {
            // SAFETY: the held request
            unsafe { (self.hooks.op_kill)(op) };
        }
        self.stop_auto_refresh();
    }

    /// `StartAutoRefresh`: the timer stopped, then a new one started for `auto_refresh_wait`'s seconds (the
    /// original's `(long)((unsigned long)wait*1000)`); the peer data's validity and expire delta (at `now`) are
    /// read only when auto-refresh is on, the delta only when valid, as the original's.
    pub(crate) fn start_auto_refresh(&self, force: bool, now: i64) {
        self.stop_auto_refresh();
        let (enabled, peer) = {
            let s = self.lock();
            (s.do_auto, s.peer.clone())
        };
        let valid = enabled && peer.as_ref().is_some_and(|p| p.is_valid());
        let delta = if valid { peer.as_ref().map_or(0, |p| p.expire_delta(now)) } else { 0 };
        drop(peer);
        let Some(wait) = authctx::auto_refresh_wait(enabled, force, valid, delta) else {
            return;
        };
        // SAFETY: the context
        let op = unsafe { (self.hooks.new_timer)(self.ctx()) };
        let old = self.lock().auto_op.replace(op);
        self.release(old);
        let ms = (wait as c_ulong).wrapping_mul(1000) as c_long;
        // SAFETY: the timer just made, held
        unsafe { (self.hooks.op_run_async)(op, ms) };
    }

    /// `GetCertCallback`: only the current request counts; each waiting op (in address order, including those a
    /// completion adds during the walk past the current one) gets the request's peer data and is finished with its
    /// status; then the set is cleared, the request dropped, its status kept and the timer restarted (forced).
    pub(crate) fn get_cert_callback(&self, op: Obj, now: i64) {
        if self.lock().get_cert != Some(op) {
            return;
        }
        // SAFETY: the completing request; its answer a new reference (or NULL) taken here
        let peer = unsafe { peerobj::take_peer((self.hooks.cert_peer_data)(op)) };
        let entry = |s: &State, k: usize| (k, s.refresh[&k].0, s.refresh[&k].1.clone());
        let mut at = {
            let s = self.lock();
            s.refresh.keys().next().map(|&k| entry(&s, k))
        };
        while let Some((key, waiting, cert)) = at {
            cert.set_peer(peer.clone());
            // SAFETY: a waiting op the set holds
            unsafe {
                let status = (self.hooks.op_status)(op);
                (self.hooks.op_finish)(waiting, status);
            }
            let s = self.lock();
            at = next_key(&s.refresh, key).map(|k| entry(&s, k));
        }
        let set = std::mem::take(&mut self.lock().refresh);
        self.release_all(set.into_values().map(|(o, _)| o));
        let old = self.lock().get_cert.take();
        self.release(old);
        // SAFETY: the completing request
        let status = unsafe { (self.hooks.op_status)(op) };
        self.lock().refresh_status = status;
        self.start_auto_refresh(true, now);
        drop(peer);
    }

    /// `RefreshAsync`: the timer stopped; the waiting op (if any: its run `light`, its member `cert`) added to the
    /// set and run; a request started when none runs (`Run(1, 60000)`: it may complete inside), its status the
    /// refresh status.
    pub(crate) fn refresh_async(&self, light: Obj, cert: Option<Arc<EeWonLightCert>>, timeout: u32) {
        self.stop_auto_refresh();
        if !light.is_null() {
            let new = {
                let mut s = self.lock();
                let fresh = !s.refresh.contains_key(&(light as usize));
                if fresh {
                    s.refresh.insert(light as usize, (light, cert.unwrap_or_default()));
                }
                fresh
            };
            // SAFETY: a live waiting op
            unsafe {
                if new {
                    (self.hooks.add_ref)(light);
                }
                (self.hooks.op_run_async)(light, timeout as c_long);
            }
        }
        if self.lock().get_cert.is_some() {
            return;
        }
        // SAFETY: the context
        let op = unsafe { (self.hooks.new_cert_op)(self.ctx(), true) };
        let old = self.lock().get_cert.replace(op);
        self.release(old);
        // SAFETY: the request just made (held, unless its completion drops it inside the call, as the original's)
        let status = unsafe { (self.hooks.op_run)(op, 1, 60000) };
        self.lock().refresh_status = status;
    }

    /// `RefreshBlock`: a request of its own run blocking; on success the timer restarts. Returns the refresh
    /// status, read again after the restart as the original did.
    pub(crate) fn refresh_block(&self, timeout: u32, now: i64) -> c_long {
        // SAFETY: the context
        let op = unsafe { (self.hooks.new_cert_op)(self.ctx(), false) };
        // SAFETY: the request just made, held here
        let status = unsafe { (self.hooks.op_run)(op, 2, c_ulong::from(timeout)) };
        self.lock().refresh_status = status;
        if self.lock().refresh_status == 0 {
            self.start_auto_refresh(false, now);
        }
        let status = self.lock().refresh_status;
        self.release(Some(op));
        status
    }
}

impl Drop for EeWonAuthFlow {
    /// The members in the original's reverse order: the sessions, the timer, the request, the waiting ops.
    fn drop(&mut self) {
        let s = std::mem::take(&mut *self.lock());
        for (_, r) in s.sessions {
            self.release_all(r.sessions);
        }
        self.release(s.auto_op);
        self.release(s.get_cert);
        self.release_all(s.refresh.into_values().map(|(o, _)| o));
    }
}

// ---- the C ABI (AuthContext.cpp; `f` is from `ee_won_auth_flow_new`)

fn addr_key(a: *const EeWonIpAddr) -> [u8; 6] {
    // SAFETY: a live IPAddr value (contract)
    unsafe { &*a }.addr().six_bytes()
}

macro_rules! flow {
    ($f:expr) => {
        // SAFETY: a live flow (contract)
        unsafe { &*$f }
    };
}

/// # Safety
/// `hooks` points to a hook table; its context outlives the flow.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_flow_new(hooks: *const EeWonAuthFlowHooks) -> *mut EeWonAuthFlow {
    // SAFETY: the caller's table
    Box::into_raw(Box::new(EeWonAuthFlow::new(unsafe { *hooks })))
}

/// The members' references dropped.
///
/// # Safety
/// `f` is from `ee_won_auth_flow_new` (or NULL), not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_flow_free(f: *mut EeWonAuthFlow) {
    if !f.is_null() {
        // SAFETY: the caller's flow
        drop(unsafe { Box::from_raw(f) });
    }
}

/// # Safety
/// `f` a flow, `a` a live IPAddr value, `session` an `AuthSession`'s `RefCount*` or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_flow_put_session(
    f: *const EeWonAuthFlow,
    a: *const EeWonIpAddr,
    session: *mut c_void,
    now: i64,
) {
    flow!(f).put_session(addr_key(a), session, now)
}

/// A reference the caller adopts, or NULL.
///
/// # Safety
/// `f` a flow, `a` a live IPAddr value.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_flow_get_session(
    f: *const EeWonAuthFlow,
    a: *const EeWonIpAddr,
    now: i64,
) -> *mut c_void {
    flow!(f).get_session(addr_key(a), now)
}

/// `SetPeerData(data)`: a reference of its own to `peer` (NULL: none), then the sessions cleared.
///
/// # Safety
/// `f` a flow, `peer` a live peer data object or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_flow_set_peer_data(f: *const EeWonAuthFlow, peer: *const EeWonAuthObj) {
    // SAFETY: the caller's object
    flow!(f).set_peer_data(unsafe { peerobj::peer_of(peer) })
}

/// `GetPeerData()`: a new reference the caller frees (`ee_won_authobj_free`), NULL for none.
///
/// # Safety
/// `f` a flow.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_flow_peer_data(f: *const EeWonAuthFlow) -> *mut EeWonAuthObj {
    flow!(f).peer_data().map_or(std::ptr::null_mut(), peerobj::peer_ref)
}

/// `IsExpired(extra)` at `now`.
///
/// # Safety
/// `f` a flow.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_flow_is_expired(f: *const EeWonAuthFlow, extra: i64, now: i64) -> bool {
    flow!(f).is_expired(extra, now)
}

/// # Safety
/// `f` a flow, `op` the completing timer's `RefCount*`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_flow_auto_refresh_callback(f: *const EeWonAuthFlow, op: *mut c_void) {
    flow!(f).auto_refresh_callback(op)
}

/// # Safety
/// `f` a flow.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_flow_stop_auto_refresh(f: *const EeWonAuthFlow) {
    flow!(f).stop_auto_refresh()
}

/// # Safety
/// `f` a flow.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_flow_set_do_auto_refresh(f: *const EeWonAuthFlow, enabled: bool, now: i64) {
    flow!(f).set_do_auto_refresh(enabled, now)
}

/// # Safety
/// `f` a flow.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_flow_kill(f: *const EeWonAuthFlow) {
    flow!(f).kill()
}

/// # Safety
/// `f` a flow.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_flow_start_auto_refresh(f: *const EeWonAuthFlow, force: bool, now: i64) {
    flow!(f).start_auto_refresh(force, now)
}

/// # Safety
/// `f` a flow, `op` the completing request's `RefCount*`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_flow_get_cert_callback(f: *const EeWonAuthFlow, op: *mut c_void, now: i64) {
    flow!(f).get_cert_callback(op, now)
}

/// `RefreshAsync(op, timeout)`: `light` the waiting op's run (an `AsyncOp`'s `RefCount*`, or NULL for none),
/// `cert` its member (borrowed: the set takes a reference of its own).
///
/// # Safety
/// `f` a flow; `light` NULL or a live op; `cert` NULL or from `ee_won_light_cert_new`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_flow_refresh_async(
    f: *const EeWonAuthFlow,
    light: *mut c_void,
    cert: *const EeWonLightCert,
    timeout: u32,
) {
    // SAFETY: the caller's object
    flow!(f).refresh_async(light, unsafe { light_ref(cert) }, timeout)
}

/// # Safety
/// `f` a flow.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_flow_refresh_block(f: *const EeWonAuthFlow, timeout: u32, now: i64) -> c_long {
    flow!(f).refresh_block(timeout, now)
}

/// A new reference of the caller's to a borrowed light op member.
///
/// # Safety
/// `cert` NULL or from `ee_won_light_cert_new` (alive).
pub(crate) unsafe fn light_ref(cert: *const EeWonLightCert) -> Option<Arc<EeWonLightCert>> {
    if cert.is_null() {
        return None;
    }
    // SAFETY: a live Arc's pointer (contract): one more count, then an Arc for it
    unsafe {
        Arc::increment_strong_count(cert);
        Some(Arc::from_raw(cert))
    }
}

/// `new LightGetCertOp`'s member (no peer data): a reference, freed with `ee_won_light_cert_free`.
#[no_mangle]
pub extern "C" fn ee_won_light_cert_new() -> *mut EeWonLightCert {
    Arc::into_raw(EeWonLightCert::new()).cast_mut()
}

/// Drops a reference (NULL: nothing).
///
/// # Safety
/// `cert` from `ee_won_light_cert_new`, not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_won_light_cert_free(cert: *mut EeWonLightCert) {
    if !cert.is_null() {
        // SAFETY: the caller's reference
        drop(unsafe { Arc::from_raw(cert.cast_const()) });
    }
}

/// `GetPeerData()`: a new reference (`ee_won_authobj_free`), NULL for none.
///
/// # Safety
/// `cert` a live light op member.
#[no_mangle]
pub unsafe extern "C" fn ee_won_light_cert_peer(cert: *const EeWonLightCert) -> *mut EeWonAuthObj {
    // SAFETY: the caller's object
    unsafe { &*cert }.peer().map_or(std::ptr::null_mut(), peerobj::peer_ref)
}

/// `SetPeerData(data)` (NULL: none; a reference of its own).
///
/// # Safety
/// `cert` a live light op member, `peer` NULL or a live peer data object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_light_cert_set_peer(cert: *const EeWonLightCert, peer: *const EeWonAuthObj) {
    // SAFETY: the caller's objects
    unsafe { &*cert }.set_peer(unsafe { peerobj::peer_of(peer) })
}

#[cfg(test)]
#[path = "authflow_tests.rs"]
mod tests;
