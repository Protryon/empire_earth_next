//! NEProtocol (`ffi_protocol.rs`) over a stand-in plug-in: each `ee_tcp_*` call is a line of the log with its
//! arguments, answered from a script. The whole class against the original units is tests/net/ne_protocol_test
//! (its golden, tests/net/golden_protocol.txt, made by the original NEProtocol.cpp and the TCP shim).

use std::ffi::c_void;
use std::sync::Mutex;

use crate::net::ffi_protocol::*;

/// The stand-in plug-in (the names `ffi_protocol` calls under `cfg(test)`)
pub mod stub {
    use super::EeNetPrSink;
    use std::ffi::c_void;
    use std::sync::Mutex;

    pub static LOG: Mutex<Vec<String>> = Mutex::new(Vec::new());
    /// the next answers (0 when none is left)
    pub static ANSWERS: Mutex<Vec<i32>> = Mutex::new(Vec::new());
    /// the handle a successful open writes
    pub const NEW: usize = 0x5000;

    fn log(s: String) {
        LOG.lock().unwrap().push(s);
    }
    fn answer() -> i32 {
        let mut a = ANSWERS.lock().unwrap();
        if a.is_empty() {
            0
        } else {
            a.remove(0)
        }
    }
    unsafe fn slot(s: *mut *mut c_void) -> usize {
        let v = *s as usize;
        *s = std::ptr::null_mut(); // the plug-in may clear it (a close): nothing reads it after
        v
    }
    unsafe fn open(out: *mut *mut c_void) -> i32 {
        let r = answer();
        *out = NEW as *mut c_void; // written even on failure: the protocol must not hand it on
        r
    }
    unsafe fn text(p: *const u8, n: usize) -> String {
        String::from_utf8_lossy(std::slice::from_raw_parts(p, n)).into_owned()
    }

    pub unsafe fn ee_tcp_activate() -> i32 {
        log("activate".into());
        answer()
    }
    pub unsafe fn ee_tcp_deactivate() -> i32 {
        log("deactivate".into());
        answer()
    }
    pub unsafe fn ee_tcp_is_valid() -> bool {
        log("is_valid".into());
        answer() == 0
    }
    pub unsafe fn ee_tcp_get_status() -> i32 {
        log("status".into());
        answer()
    }
    pub unsafe fn ee_tcp_accept(server: *mut *mut c_void, connection: *mut *mut c_void) -> i32 {
        log(format!("accept {:x}", slot(server)));
        open(connection)
    }
    pub unsafe fn ee_tcp_accept_for_xfer(server: *mut *mut c_void, connection: *mut *mut c_void) -> i32 {
        log(format!("accept_for_xfer {:x}", slot(server)));
        open(connection)
    }
    pub unsafe fn ee_tcp_allow_new_connections(server: *mut *mut c_void, allow: bool) {
        log(format!("allow {:x} {allow}", slot(server)));
    }
    pub unsafe fn ee_tcp_close_connection(connection: *mut *mut c_void) -> i32 {
        log(format!("close_connection {:x}", slot(connection)));
        answer()
    }
    pub unsafe fn ee_tcp_close_server(server: *mut *mut c_void) -> i32 {
        log(format!("close_server {:x}", slot(server)));
        answer()
    }
    #[allow(clippy::too_many_arguments)]
    pub unsafe fn ee_tcp_connect(
        name: *const u8,
        name_length: usize,
        host: *const u8,
        host_length: usize,
        address: *const u8,
        address_length: usize,
        kind: i32,
        event: *mut c_void,
        connection: *mut *mut c_void,
        adapter: u32,
    ) -> i32 {
        log(format!(
            "connect {} {} {} {kind} {:x} {adapter}",
            text(name, name_length),
            text(host, host_length),
            text(address, address_length),
            event as usize
        ));
        open(connection)
    }
    pub unsafe fn ee_tcp_connect_for_xfer(
        host: *mut *mut c_void,
        mode: i32,
        event: *mut c_void,
        connection: *mut *mut c_void,
    ) -> i32 {
        log(format!("connect_for_xfer {:x} {mode} {:x}", slot(host), event as usize));
        open(connection)
    }
    pub unsafe fn ee_tcp_get_client_address(
        server: *mut *mut c_void,
        index: u32,
        sink: EeNetPrSink,
        context: *mut c_void,
    ) -> i32 {
        log(format!("client_address {:x} {index}", slot(server)));
        sink(context, b"addr".as_ptr(), 4);
        answer()
    }
    pub unsafe fn ee_tcp_get_detected_game(
        index: u32,
        sink_name: EeNetPrSink,
        context_name: *mut c_void,
        sink_host: EeNetPrSink,
        context_host: *mut c_void,
        sink_address: EeNetPrSink,
        context_address: *mut c_void,
    ) -> i32 {
        log(format!("detected_game {index}"));
        sink_name(context_name, b"n".as_ptr(), 1);
        sink_host(context_host, b"h".as_ptr(), 1);
        sink_address(context_address, b"a".as_ptr(), 1);
        answer()
    }
    pub unsafe fn ee_tcp_get_detected_game_count() -> u32 {
        log("detected_game_count".into());
        answer() as u32
    }
    pub unsafe fn ee_tcp_get_local_address(adapter: u32, sink: EeNetPrSink, context: *mut c_void) {
        log(format!("local_address {adapter}"));
        sink(context, b"local".as_ptr(), 5);
    }
    pub unsafe fn ee_tcp_enumerate_adapters(sink: EeNetPrSink, context: *mut c_void) -> u32 {
        log("adapters".into());
        sink(context, b"one".as_ptr(), 3);
        sink(context, b"two".as_ptr(), 3);
        2
    }
    pub unsafe fn ee_tcp_is_alive(connection: *mut *mut c_void) -> bool {
        log(format!("is_alive {:x}", slot(connection)));
        answer() != 0
    }
    #[allow(clippy::too_many_arguments)]
    pub unsafe fn ee_tcp_publish(
        name: *const u8,
        name_length: usize,
        address: *const u8,
        address_length: usize,
        flags: u32,
        data_event: *mut c_void,
        listen_event: *mut c_void,
        accept_event: *mut c_void,
        server: *mut *mut c_void,
        flag55: bool,
        announce: bool,
        adapter: u32,
    ) -> i32 {
        log(format!(
            "publish {} {} {flags} {:x} {:x} {:x} {flag55} {announce} {adapter}",
            text(name, name_length),
            text(address, address_length),
            data_event as usize,
            listen_event as usize,
            accept_event as usize
        ));
        open(server)
    }
    unsafe fn receive(size: u32, buffer: *mut u8, received: *mut u32) -> i32 {
        let r = answer();
        let n = (answer() as u32).min(size);
        for i in 0..n {
            *buffer.add(i as usize) = i as u8;
        }
        *received = n;
        r
    }
    pub unsafe fn ee_tcp_receive(connection: *mut *mut c_void, size: u32, buffer: *mut u8, received: *mut u32) -> i32 {
        log(format!("receive {:x} {size} {}", slot(connection), *received));
        receive(size, buffer, received)
    }
    pub unsafe fn ee_tcp_receive_from(
        server: *mut *mut c_void,
        size: u32,
        buffer: *mut u8,
        received: *mut u32,
        index: *mut u32,
    ) -> i32 {
        log(format!("receive_from {:x} {size} {} {}", slot(server), *received, *index));
        *index = 5;
        receive(size, buffer, received)
    }
    pub unsafe fn ee_tcp_reset_client_connection(server: *mut *mut c_void, index: u32) {
        log(format!("reset_client {:x} {index}", slot(server)));
    }
    pub unsafe fn ee_tcp_reset_xfer(server: *mut *mut c_void) {
        log(format!("reset_xfer {:x}", slot(server)));
    }
    pub unsafe fn ee_tcp_search_for_game(name: *const u8, length: usize, start: bool) -> i32 {
        log(format!("search {} {start}", text(name, length)));
        answer()
    }
    pub unsafe fn ee_tcp_send(connection: *mut *mut c_void, data: *mut u8, length: u32) -> i32 {
        log(format!("send {:x} {:?}", slot(connection), std::slice::from_raw_parts(data, length as usize)));
        answer()
    }
    pub unsafe fn ee_tcp_send_single(
        host: *const u8,
        host_length: usize,
        port: u32,
        data: *const u8,
        length: u32,
    ) -> i32 {
        let _ = data;
        log(format!("send_single {} {port} {length}", text(host, host_length)));
        answer()
    }
    pub unsafe fn ee_tcp_send_to(server: *mut *mut c_void, index: u32, data: *mut u8, length: u32) -> i32 {
        log(format!("send_to {:x} {index} {:?}", slot(server), std::slice::from_raw_parts(data, length as usize)));
        answer()
    }
    pub unsafe fn ee_tcp_xfer_receive(size: u32, buffer: *mut u8, received: *mut u32) -> i32 {
        log(format!("xfer_receive {size} {}", *received));
        receive(size, buffer, received)
    }
    pub unsafe fn ee_tcp_xfer_send_to(server: *mut *mut c_void, index: u32, data: *const u8, length: u32) -> i32 {
        let _ = data;
        log(format!("xfer_send_to {:x} {index} {length}", slot(server)));
        answer()
    }
    /// an event is its own handle here
    pub unsafe fn ee_ts_event_handle(event: *const c_void) -> *mut c_void {
        (event as usize + 1) as *mut c_void
    }
}

/// one test at a time: the stand-in's log and answers and the registry are global
static SERIAL: Mutex<()> = Mutex::new(());

fn script(answers: &[i32]) -> std::sync::MutexGuard<'static, ()> {
    let g = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
    stub::LOG.lock().unwrap().clear();
    *stub::ANSWERS.lock().unwrap() = answers.to_vec();
    g
}

fn log() -> Vec<String> {
    std::mem::take(&mut *stub::LOG.lock().unwrap())
}

unsafe extern "C" fn sink(context: *mut c_void, bytes: *const u8, length: usize) {
    let s = &mut *(context as *mut Vec<String>);
    s.push(String::from_utf8_lossy(std::slice::from_raw_parts(bytes, length)).into_owned());
}

#[test]
fn the_registry_and_initialize() {
    let g = script(&[0, 0, 0, 0]);
    assert_eq!(ee_net_pr_count(), 0);
    // valid: kept
    assert_eq!(ee_net_pr_initialize(), 0);
    assert_eq!(ee_net_pr_count(), 1);
    let mut p = std::ptr::null_mut();
    assert_eq!(unsafe { ee_net_pr_get(0, &mut p) }, 0);
    assert!(!p.is_null());
    let mut q = 7usize as *mut EeNetProtocol;
    assert_eq!(unsafe { ee_net_pr_get(1, &mut q) }, 0xa);
    assert_eq!(q as usize, 7, "past the end leaves the out argument");
    ee_net_pr_shutdown();
    assert_eq!(ee_net_pr_count(), 0);
    assert_eq!(log(), ["activate", "is_valid", "is_valid", "deactivate"]);
    // not valid: freed again (deactivate skipped while not valid), no protocol
    drop(g);
    let _g = script(&[0, 1, 1]);
    assert_eq!(ee_net_pr_initialize(), 3);
    assert_eq!(ee_net_pr_count(), 0);
    assert_eq!(log(), ["activate", "is_valid", "is_valid"]);
    let mut n = 0;
    let name = unsafe { std::slice::from_raw_parts(ee_net_pr_name(&mut n), n) };
    assert_eq!(name, b"TCP/IP Protocol");
    assert_eq!(ee_net_pr_type(), 1);
}

#[test]
fn opens_hand_on_the_handle_only_on_success() {
    let _g = script(&[0]);
    let p = ee_net_pr_new();
    log();
    unsafe {
        let ev = 0x40 as *const c_void;
        let mut out = 7usize as *mut c_void;
        assert_eq!(ee_net_pr_connect(p, b"n".as_ptr(), 1, b"h".as_ptr(), 1, b"a".as_ptr(), 1, 2, ev, &mut out, 900), 0);
        assert_eq!(out as usize, stub::NEW);
        *stub::ANSWERS.lock().unwrap() = vec![6, 0, 7, 9];
        assert_eq!(ee_net_pr_connect(p, b"n".as_ptr(), 1, b"h".as_ptr(), 1, b"a".as_ptr(), 1, 2, ev, &mut out, 900), 6);
        assert!(out.is_null());
        let mut c = std::ptr::null_mut();
        assert_eq!(ee_net_pr_accept(p, 0x77 as *mut c_void, false, &mut c), 0);
        assert_eq!(c as usize, stub::NEW);
        assert_eq!(ee_net_pr_accept(p, 0x77 as *mut c_void, true, &mut c), 7);
        assert!(c.is_null());
        assert_eq!(ee_net_pr_connect_for_xfer(p, 0x88 as *mut c_void, 1, ev, &mut c), 9);
        assert!(c.is_null());
        let events = [0x10 as *const c_void, 0x20 as *const c_void, 0x30 as *const c_void];
        assert_eq!(
            ee_net_pr_publish(
                p,
                b"EEService".as_ptr(),
                9,
                b"me".as_ptr(),
                2,
                2,
                events.as_ptr(),
                &mut c,
                true,
                false,
                3
            ),
            0
        );
        assert_eq!(c as usize, stub::NEW);
        assert_eq!(
            log(),
            [
                "connect n h a 2 41 900",
                "connect n h a 2 41 900",
                "accept 77",
                "accept_for_xfer 77",
                "connect_for_xfer 88 1 41",
                "publish EEService me 2 11 21 31 true false 3"
            ]
        );
        ee_net_pr_free(p);
    }
}

#[test]
fn the_counters_add_what_moved() {
    let _g = script(&[0]);
    let p = ee_net_pr_new();
    log();
    unsafe {
        let mut data = [1u8, 2, 3];
        *stub::ANSWERS.lock().unwrap() = vec![4, 0, 0, 5, 3, 0, 9, 2];
        assert_eq!(ee_net_pr_send(p, 0x11 as *mut c_void, data.as_mut_ptr(), 3), 4, "counted even when it failed");
        assert_eq!(ee_net_pr_send_to(p, 0x2 as *mut c_void, false, 3, data.as_mut_ptr(), 2), 0);
        assert_eq!(ee_net_pr_send_to(p, 0x2 as *mut c_void, true, 3, data.as_mut_ptr(), 1), 0);
        assert_eq!(ee_net_pr_send_single(p, b"host".as_ptr(), 4, 0x1234, data.as_ptr(), 3), 5);
        assert_eq!(ee_net_pr_bytes(p, true), 9);
        let mut buf = [0u8; 8];
        let mut got = 1u32;
        assert_eq!(ee_net_pr_receive(p, 0x3 as *mut c_void, 8, buf.as_mut_ptr(), &mut got), 3);
        assert_eq!(got, 0);
        let mut index = 0;
        assert_eq!(ee_net_pr_receive_from(p, 0x4 as *mut c_void, 8, buf.as_mut_ptr(), &mut got, &mut index), 9);
        assert_eq!((got, index), (2, 5));
        *stub::ANSWERS.lock().unwrap() = vec![0, 6];
        assert_eq!(ee_net_pr_xfer_receive(p, 4, buf.as_mut_ptr(), &mut got), 0);
        assert_eq!(got, 4);
        assert_eq!(ee_net_pr_bytes(p, false), 6);
        assert_eq!(
            log(),
            [
                "send 11 [1, 2, 3]",
                "send_to 2 3 [1, 2]",
                "xfer_send_to 2 3 1",
                "send_single host 4660 3",
                "receive 3 8 1",
                "receive_from 4 8 0 0",
                "xfer_receive 4 2"
            ]
        );
        ee_net_pr_free(p);
    }
}

#[test]
fn the_rest_forward() {
    let _g = script(&[0]);
    let p = ee_net_pr_new();
    log();
    unsafe {
        *stub::ANSWERS.lock().unwrap() = vec![2, 0, 3, 4, 1, 5, 6];
        let mut texts: Vec<String> = Vec::new();
        let t = &mut texts as *mut Vec<String> as *mut c_void;
        assert_eq!(ee_net_pr_client_address(p, 0x9 as *mut c_void, 2, sink, t), 2);
        assert_eq!(ee_net_pr_detected_game(p, 1, sink, t, sink, t, sink, t), 0);
        assert_eq!(ee_net_pr_detected_game_count(p), 3);
        ee_net_pr_local_address(p, 1, sink, t);
        assert_eq!(ee_net_pr_enumerate_adapters(p, sink, t), 2);
        assert_eq!(ee_net_pr_status(p), 4);
        assert!(ee_net_pr_is_alive(p, 0xa as *mut c_void));
        assert_eq!(ee_net_pr_search_for_game(p, b"EEService".as_ptr(), 9, true), 5);
        ee_net_pr_allow_new_connections(p, 0xb as *mut c_void, false);
        ee_net_pr_reset(p, 0xb as *mut c_void, false, 4);
        ee_net_pr_reset(p, 0xb as *mut c_void, true, 0);
        assert_eq!(ee_net_pr_close(p, 0xc as *mut c_void, false), 6);
        assert_eq!(ee_net_pr_close(p, 0xd as *mut c_void, true), 0);
        assert_eq!(texts, ["addr", "n", "h", "a", "local", "one", "two"]);
        assert_eq!(
            log(),
            [
                "client_address 9 2",
                "detected_game 1",
                "detected_game_count",
                "local_address 1",
                "adapters",
                "status",
                "is_alive a",
                "search EEService true",
                "allow b false",
                "reset_client b 4",
                "reset_xfer b",
                "close_connection c",
                "close_server d"
            ]
        );
        assert_eq!(ee_net_pr_bytes(p, true) + ee_net_pr_bytes(p, false), 0);
        ee_net_pr_free(p);
    }
}
