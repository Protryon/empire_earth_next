use super::*;
use std::cell::RefCell;
use std::ffi::c_ulong;

// a stand-in object: a counter per address (each test's objects are its own thread's)
thread_local! {
    static COUNTS: RefCell<std::collections::BTreeMap<usize, i64>> = RefCell::new(Default::default());
    static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
}

fn count(p: usize) -> i64 {
    COUNTS.with(|c| *c.borrow().get(&p).unwrap_or(&0))
}

unsafe extern "C" fn h_add_ref(p: *mut c_void) {
    COUNTS.with(|c| *c.borrow_mut().entry(p as usize).or_default() += 1);
}
unsafe extern "C" fn h_release(p: *mut c_void) {
    COUNTS.with(|c| *c.borrow_mut().entry(p as usize).or_default() -= 1);
    LOG.with(|l| l.borrow_mut().push(format!("release {}", p as usize)));
}
// a session handle (hostauth.rs) last used at `at`, and its handle count
fn session(at: i64) -> *mut c_void {
    let key = crate::blowfish::Blowfish::default();
    crate::hostauth::session_handle(crate::peerauth::EeWonAuthSession::new(1, 7, key, 4, at)).ptr()
}
fn handles(p: *mut c_void) -> usize {
    let a = crate::hostauth::item(crate::hostop::Obj(p as usize)).expect("a handle");
    std::sync::Arc::strong_count(&a) - 1
}
unsafe extern "C" fn h_kill(_: *mut c_void) {}
unsafe extern "C" fn h_killed(_: *mut c_void) -> bool {
    false
}
unsafe extern "C" fn h_status(_: *mut c_void) -> c_long {
    0
}
unsafe extern "C" fn h_run_async(_: *mut c_void, _: c_long) {}
unsafe extern "C" fn h_run(_: *mut c_void, _: c_long, _: c_ulong) -> c_long {
    100055
}
unsafe extern "C" fn h_finish(_: *mut c_void, _: c_long) {}
unsafe extern "C" fn h_new_timer(_: *mut c_void) -> *mut c_void {
    1000 as *mut c_void
}
unsafe extern "C" fn h_new_cert(_: *mut c_void, _: bool) -> *mut c_void {
    LOG.with(|l| l.borrow_mut().push("new cert op".into()));
    2000 as *mut c_void
}
unsafe extern "C" fn h_cert_peer(_: *mut c_void) -> *mut EeWonAuthObj {
    std::ptr::null_mut()
}

pub(crate) fn hooks() -> EeWonAuthFlowHooks {
    EeWonAuthFlowHooks {
        context: 77 as *mut c_void,
        add_ref: h_add_ref,
        release: h_release,
        op_kill: h_kill,
        op_killed: h_killed,
        op_status: h_status,
        op_run_async: h_run_async,
        op_run: h_run,
        op_finish: h_finish,
        new_timer: h_new_timer,
        new_cert_op: h_new_cert,
        cert_peer_data: h_cert_peer,
    }
}

#[test]
fn the_lock_is_recursive_on_its_thread_and_exclusive_across_threads() {
    let l = Arc::new(ReLock::default());
    let g1 = l.lock();
    let g2 = l.lock();
    let flag = Arc::new(Mutex::new(false));
    let (l2, f2) = (l.clone(), flag.clone());
    let t = std::thread::spawn(move || {
        let _g = l2.lock();
        *f2.lock().unwrap() = true;
    });
    std::thread::sleep(std::time::Duration::from_millis(50));
    assert!(!*flag.lock().unwrap(), "the other thread waits while the owner holds it");
    drop(g2);
    std::thread::sleep(std::time::Duration::from_millis(20));
    assert!(!*flag.lock().unwrap(), "still held once");
    drop(g1);
    t.join().unwrap();
    assert!(*flag.lock().unwrap());
}

#[test]
fn a_new_context_has_empty_peer_data_and_its_secret_file() {
    let c = EeWonAuthCtx::new(hooks(), b"dir/_wonlogin.ks");
    let p = c.peer_data().expect("Init's peer data");
    assert!(!p.is_valid());
    assert!(p.block.is_some() && p.cert.is_some() && p.cert2.is_some());
    assert!(c.is_expired(0));
    let mut v = Vec::new();
    // SAFETY: the login, a Vec sink
    unsafe {
        authlogin::ee_won_auth_login_text(
            c.login(),
            authlogin::EE_WON_AUTH_LOGIN_SECRET_FILE,
            crate::hostauth::tests::sink,
            &mut v as *mut Vec<u8> as *mut c_void,
        )
    };
    assert_eq!(v, b"dir/_wonlogin.ks");
    c.set_peer_data(None);
    assert!(c.peer_data().is_none());
    assert!(c.is_expired(0));
}

#[test]
fn the_server_list_is_held_and_released_last() {
    let c = EeWonAuthCtx::new(hooks(), b"x");
    assert!(c.server_context().is_null());
    c.set_server_context(500 as *mut c_void);
    assert_eq!(count(500), 1);
    let got = c.server_context();
    assert_eq!(got as usize, 500);
    assert_eq!(count(500), 2, "a reference for the caller");
    // SAFETY: the stand-in's counter
    unsafe { h_release(got) };
    c.set_server_context(600 as *mut c_void);
    assert_eq!((count(500), count(600)), (0, 1));
    let s = session(unix_now());
    c.put_session([1, 2, 3, 4, 5, 6], s);
    assert_eq!(handles(s), 2, "the cache's reference to the session handle");
    LOG.with(|l| l.borrow_mut().clear());
    drop(c);
    let log = LOG.with(|l| l.borrow().clone());
    assert_eq!(log, vec!["release 600".to_string()], "the servers (the session is a handle)");
    assert_eq!(handles(s), 1, "the cache's reference given back");
    crate::hostauth::release(crate::hostop::Obj(s as usize));
}

#[test]
fn sessions_go_through_the_flow_under_the_lock() {
    let c = EeWonAuthCtx::new(hooks(), b"x");
    let a = [9, 9, 1, 2, 3, 4];
    c.put_session(a, std::ptr::null_mut());
    assert!(c.get_session(a).is_null());
    let s = session(unix_now());
    c.put_session(a, s);
    assert_eq!(c.get_session(a), s);
    assert!(c.get_session(a).is_null());
    assert_eq!(handles(s), 2, "the caller's reference from get_session");
    crate::hostauth::release(crate::hostop::Obj(s as usize));
    // a session last used over 300 seconds ago is dropped, not answered
    let old = session(unix_now() - 301);
    c.put_session(a, old);
    assert!(c.get_session(a).is_null());
    assert_eq!(handles(old), 1, "the cache's reference given back");
    crate::hostauth::release(crate::hostop::Obj(old as usize));
    crate::hostauth::release(crate::hostop::Obj(s as usize));
    assert_eq!(c.refresh_block(10), 100055);
}

#[test]
fn the_verifier_key_loads_from_a_file() {
    // the built-in key is public, made with no draw
    assert!(verifier(|k, _| k.is_public()));
    let dir = std::env::temp_dir().join(format!("ee_won_authobj_{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let missing = dir.join("missing.pub");
    assert!(!load_verifier(missing.to_str().unwrap().as_bytes()));
    assert!(verifier(|k, checked| k.is_public() && *checked), "a missing file leaves the key");
    let bad = dir.join("bad.pub");
    std::fs::write(&bad, b"not a key").unwrap();
    assert!(!load_verifier(bad.to_str().unwrap().as_bytes()));
    assert!(!verifier(|k, _| k.is_public()), "a bad file invalidates it");
    let good = dir.join("good.pub");
    std::fs::write(&good, &VERIFIER_KEY[..816]).unwrap();
    assert!(load_verifier(good.to_str().unwrap().as_bytes()));
    assert!(verifier(|k, _| k.is_public()));
    std::fs::remove_dir_all(&dir).ok();
}
