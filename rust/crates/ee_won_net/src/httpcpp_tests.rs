//! The HTTP engine's native calls without C++ objects behind them (the defaults: no lock taken, an empty host, no
//! socket, no connection), the clock, and the tables' wiring.

use super::*;
use crate::cppcore::tests::LOCK;

unsafe extern "C" fn take_bytes(context: *mut c_void, bytes: *const u8, length: usize) {
    // SAFETY: the Vec handed in
    let v = unsafe { &mut *(context as *mut Vec<Vec<u8>>) };
    v.push(if length == 0 { Vec::new() } else { unsafe { std::slice::from_raw_parts(bytes, length) }.to_vec() });
}

#[test]
fn defaults_without_objects() {
    let _l = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let none = 0x7777_0010usize as *mut c_void;
    // SAFETY: no table call reaches a C++ object: every lookup finds none
    unsafe {
        n_lock(none);
        n_unlock(none);
        let mut got: Vec<Vec<u8>> = Vec::new();
        n_host(none, take_bytes, &mut got as *mut _ as *mut c_void);
        assert_eq!(got, [Vec::<u8>::new()]);
        assert!(n_op_socket(none).is_null());
        assert_eq!(n_conn_num_ops(none), 0);
        n_conn_kill(none);
        n_conn_queue(none, none);
        n_set_mode(none, 2);
        n_prepare(none, null_mut());
        n_run_async(none);
        n_socket_kill(none);
    }
}

#[test]
fn the_clock_is_unix_seconds() {
    // SAFETY: no object
    let t = unsafe { n_now(null_mut()) };
    let now = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_secs() as i64;
    assert!((now - t).abs() <= 1);
}

#[test]
fn the_tables_carry_their_object() {
    let c = conn_hooks(0x20usize as *mut c_void);
    assert_eq!(c.context as usize, 0x20);
    assert_eq!(c.now as usize, n_now as *const () as usize);
    let s = session_hooks(0x30usize as *mut c_void);
    assert_eq!(s.context as usize, 0x30);
    assert_eq!(s.new_conn as usize, n_new_conn as *const () as usize);
}
