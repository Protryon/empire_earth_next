//! The GameSpy SDK's pieces against the original's: the cipher and encoding against the answers the C SDK gave in
//! `golden_gamespy.txt`, the random numbers against the minimal standard's sequence, the reply rules and the
//! heartbeat schedule over real sockets on 127.0.0.1 with a test clock. The instance over the C++ ops is
//! won_gamespy_test's (the golden).

use super::*;
use std::cell::Cell;

thread_local! {
    static CLOCK: Cell<Option<u32>> = const { Cell::new(None) };
}
pub(super) fn clock() -> Option<u32> {
    CLOCK.with(Cell::get)
}
fn set_clock(t: u32) {
    CLOCK.with(|c| c.set(Some(t)));
}

/// The SDK's statics (the master, its address, the current record) are the process's: one test at a time.
static SERIAL: Mutex<()> = Mutex::new(());

fn encode_text(key: &[u8], challenge: &[u8]) -> String {
    let mut b = challenge.to_vec();
    gs_encrypt(key, &mut b);
    String::from_utf8(gs_encode(&b)).unwrap()
}

#[test]
fn the_validation_is_the_c_sdk_s() {
    // golden_gamespy.txt: "\secure\abcdef\" and "\secure\0123456789\" with the key s3cr3tKEY
    assert_eq!(encode_text(b"s3cr3tKEY", b"abcdef"), "5/5YpGJM");
    assert_eq!(encode_text(b"s3cr3tKEY", b"0123456789"), "PVNKPgsgEboz6QAA");
    let long = encode_text(b"s3cr3tKEY", &[b'v'; 128]);
    assert_eq!(long.len(), 172);
    assert!(long.starts_with("+UcX4v1+s/BYOFWOcIqK/yir9N6T1LBAUXWKBm8FK6Te+wGP8w"), "{long}");
    assert!(long.ends_with("owC45T4brxPc/0rjOU8A"), "{long}");
    // the encoding alone: zero bytes pad a short group, no '='
    assert_eq!(gs_encode(b"Man"), b"TWFu");
    assert_eq!(gs_encode(b"Ma"), b"TWEA");
    assert_eq!(gs_encode(b""), b"");
    // an empty key is a key of one NUL byte (the original divided by zero)
    let mut a = b"xy".to_vec();
    let mut z = b"xy".to_vec();
    gs_encrypt(b"", &mut a);
    gs_encrypt(&[0], &mut z);
    assert_eq!(a, z);
}

#[test]
fn the_random_numbers_are_the_minimal_standard() {
    let mut r = LongRand::seeded(1);
    let got: Vec<i32> = (0..5).map(|_| r.next_value()).collect();
    assert_eq!(got, [16807, 282_475_249, 1_622_650_073, 984_943_658, 1_144_108_930]);
    // the 10,000th value from 1 (Park and Miller's check)
    let mut r = LongRand::seeded(1);
    let mut v = 0;
    for _ in 0..10_000 {
        v = r.next_value();
    }
    assert_eq!(v, 1_043_618_065);
    assert_eq!(LongRand::seeded(0), LongRand(1));
    assert_eq!(LongRand::seeded(0x8000_0005), LongRand(5));
    let id = generate_id(1234);
    assert_eq!(id, generate_id(1234));
    for (i, c) in id.iter().enumerate() {
        if i == 4 || i == 9 || i == 14 {
            assert_eq!(*c, b'-');
        } else {
            assert!(c.is_ascii_alphanumeric());
        }
    }
    assert_ne!(generate_id(1), generate_id(2));
}

#[test]
fn values_are_found_after_their_key() {
    assert_eq!(value_for_key(b"\\basic\\", b"basic"), Some(&b""[..]));
    assert_eq!(value_for_key(b"\\echo\\hi there\\x\\", b"echo"), Some(&b"hi there"[..]));
    assert_eq!(value_for_key(b"\\echo\\tail", b"echo"), Some(&b"tail"[..]));
    assert_eq!(value_for_key(b"\\basic", b"basic"), None);
    assert_eq!(value_for_key(b"\\x\\basic\\", b"basic"), Some(&b""[..]));
    assert_eq!(value_for_key(b"", b"info"), None);
    // the first occurrence
    assert_eq!(value_for_key(b"\\echo\\1\\echo\\2\\", b"echo"), Some(&b"1"[..]));
}

#[test]
fn the_dump_stops_at_the_buffer() {
    let s = Shared::default();
    {
        let mut m = lock(&s.maps);
        m[2].insert(b"k1".to_vec(), CString::new(vec![b'a'; 6]).unwrap());
        m[2].insert(b"k2".to_vec(), CString::new(vec![b'b'; 4]).unwrap());
        m[2].insert(b"k3".to_vec(), CString::new("c").unwrap());
    }
    // all fit in 40 bytes
    assert_eq!(s.dump(2, 40), b"\\k1\\aaaaaa\\k2\\bbbb\\k3\\c");
    // k1 takes 10, k2 8 bytes: in 18 bytes k2 leaves no room for the NUL (strictly less than the space left), k3 is
    // past the end
    assert_eq!(s.dump(2, 18), b"\\k1\\aaaaaa");
    // a pair skipped leaves a gap: the text ends before it even when a later pair fits
    {
        let mut m = lock(&s.maps);
        m[2].insert(b"k2".to_vec(), CString::new(vec![b'b'; 30]).unwrap());
    }
    assert_eq!(s.dump(2, 40), b"\\k1\\aaaaaa");
    assert_eq!(s.dump(0, 40), b"");
}

/// A query socket on 127.0.0.1, its callback answering each kind with a text of the kind.
fn start(texts: [Vec<u8>; 4], base: i32) -> (i32, Option<QrHandle>) {
    let cb: QueryCallback = Arc::new(move |k| texts[k].clone());
    qr_init(true, Some(b"127.0.0.1"), base, b"tg", b"key", cb)
}
fn udp(port: u16) -> PlatSocket {
    socket_startup();
    // SAFETY: plain platform calls on a new socket
    unsafe {
        let s = plat::PlatSocketOpen(PLAT_SOCKET_DGRAM);
        let a = PlatSockAddrIn {
            mFamily: PLAT_AF_INET,
            mPort: port.to_be(),
            mAddress: 0x7f00_0001u32.to_be(),
            mZero: [0; 8],
        };
        assert_ne!(plat::PlatSocketBind(s, &a), 0);
        s
    }
}
fn port_of(s: PlatSocket) -> u16 {
    let mut a = PlatSockAddrIn::default();
    // SAFETY: a bound socket and a writable address
    unsafe { plat::PlatSocketGetName(s, &mut a) };
    u16::from_be(a.mPort)
}
fn send(s: PlatSocket, port: i32, data: &[u8]) {
    let a = PlatSockAddrIn {
        mFamily: PLAT_AF_INET,
        mPort: (port as u16).to_be(),
        mAddress: 0x7f00_0001u32.to_be(),
        mZero: [0; 8],
    };
    // SAFETY: the socket, the bytes and an address
    unsafe { plat::PlatSocketSendTo(s, data.as_ptr().cast(), data.len() as u32, &a) };
    std::thread::sleep(std::time::Duration::from_millis(20));
}
fn drain(s: PlatSocket) -> Vec<String> {
    let mut got = Vec::new();
    let mut wait = 300;
    // SAFETY: the socket and a buffer of its length
    while unsafe { plat::PlatSocketWait(s, PLAT_SOCKET_WAIT_READ, wait) } > 0 {
        let mut b = [0u8; 2048];
        let n = unsafe { plat::PlatSocketRecvFrom(s, b.as_mut_ptr().cast(), 2048, std::ptr::null_mut()) };
        if n < 0 {
            break;
        }
        got.push(String::from_utf8_lossy(&b[..n as usize]).into_owned());
        wait = 50;
    }
    got
}
fn close(s: PlatSocket) {
    // SAFETY: the test's socket
    unsafe { plat::PlatSocketClose(s) };
}

#[test]
fn replies_are_cut_into_packets() {
    let _serial = lock(&SERIAL);
    let big: Vec<u8> = (0..29).flat_map(|i| format!("\\key{i:02}\\{}", "v".repeat(40)).into_bytes()).collect();
    let texts = [b"\\gamename\\tg".to_vec(), big.clone(), b"\\r\\1".to_vec(), b"\\p\\".to_vec()];
    let (code, qr) = start(texts, 0);
    assert_eq!(code, 0);
    let qr = qr.unwrap();
    let port = qr.rec().qport;
    let client = udp(0);
    send(client, port, b"\\basic\\\\rules\\");
    qr_process_queries_no_heartbeat(Some(&qr));
    assert_eq!(drain(client), ["\\gamename\\tg\\r\\1\\final\\\\queryid\\2.1"]);
    // the info text (1,363 bytes, under the callback buffer) cut before the last key starting within 1,350 bytes
    send(client, port, b"\\info\\");
    qr_process_queries_no_heartbeat(Some(&qr));
    let got = drain(client);
    assert_eq!(got.len(), 2);
    // (each pair 47 bytes: key 28 starts at 1,316, its value's backslash at 1,322 is not a key's)
    let cut = 28 * 47;
    let text = |b: &[u8]| String::from_utf8_lossy(b).into_owned();
    assert_eq!(got[0], format!("{}\\queryid\\3.1", text(&big[..cut])));
    assert_eq!(got[1], format!("{}\\final\\\\queryid\\3.2", text(&big[cut..])));
    // a challenge over 128 bytes: nothing at all
    send(client, port, format!("\\basic\\\\secure\\{}\\", "w".repeat(129)).as_bytes());
    qr_process_queries_no_heartbeat(Some(&qr));
    assert!(drain(client).is_empty());
    // the echo, an unknown query
    send(client, port, b"\\echo\\hi\\");
    send(client, port, b"\\what\\");
    qr_process_queries_no_heartbeat(Some(&qr));
    assert_eq!(drain(client), ["\\echo\\hi\\final\\\\queryid\\5.1", "\\final\\\\queryid\\6.1"]);
    // shut down: nothing answered
    qr_shutdown(Some(&qr));
    send(client, port, b"\\basic\\");
    qr_process_queries_no_heartbeat(Some(&qr));
    assert!(drain(client).is_empty());
    close(client);
}

#[test]
fn heartbeats_follow_the_schedule() {
    let _serial = lock(&SERIAL);
    let master = udp(0);
    set_master_host(b"127.0.0.1");
    set_master_port(port_of(master));
    let (code, qr) = start([Vec::new(), Vec::new(), Vec::new(), Vec::new()], 0);
    assert_eq!(code, 0);
    let qr = qr.unwrap();
    let hb = format!("\\heartbeat\\{}\\gamename\\tg", qr.rec().qport);
    set_clock(1000);
    qr_process_queries(Some(&qr)); // the first
    assert_eq!(drain(master), std::slice::from_ref(&hb));
    set_clock(1000 + 30000);
    qr_process_queries(Some(&qr)); // not yet 30 s
    assert!(drain(master).is_empty());
    set_clock(1000 + 30001);
    qr_process_queries(Some(&qr)); // no query yet: every 30 s
    assert_eq!(drain(master), std::slice::from_ref(&hb));
    assert_eq!(qr.rec().no_query, 2);
    for i in 2..=10u32 {
        set_clock(1000 + 30001 * i);
        qr_process_queries(Some(&qr));
        assert_eq!(drain(master).len(), 1);
    }
    assert_eq!(qr.rec().no_query, 0); // ten tries: now only every 5 minutes
    let last = qr.rec().lastheartbeat;
    set_clock(last + 300_000);
    qr_process_queries(Some(&qr));
    assert!(drain(master).is_empty());
    set_clock(last + 300_001);
    qr_process_queries(Some(&qr));
    assert_eq!(drain(master), std::slice::from_ref(&hb));
    // the clock went back: at once
    set_clock(5);
    qr_process_queries(Some(&qr));
    assert_eq!(drain(master), std::slice::from_ref(&hb));
    qr_send_statechanged(Some(&qr));
    assert_eq!(drain(master), [format!("{hb}\\statechanged\\")]);
    // a query stops the 30-second heartbeats
    let fresh = start([Vec::new(), Vec::new(), Vec::new(), Vec::new()], 0).1.unwrap();
    set_clock(7);
    qr_process_queries(Some(&fresh));
    drain(master);
    let client = udp(0);
    send(client, fresh.rec().qport, b"\\basic\\");
    qr_process_queries(Some(&fresh));
    assert_eq!(fresh.rec().no_query, 0);
    set_clock(7 + 30001);
    qr_process_queries(Some(&fresh));
    assert!(drain(master).is_empty());
    close(client);
    close(master);
    set_master_port(MASTER_PORT);
    CLOCK.with(|c| c.set(None));
}

#[test]
fn a_failed_start_leaves_the_current_record() {
    let _serial = lock(&SERIAL);
    let (code, a) = start([b"\\who\\a".to_vec(), Vec::new(), Vec::new(), Vec::new()], 0);
    assert_eq!(code, 0);
    let a = a.unwrap();
    // an address of no interface here: no port binds
    let (code, b) = qr_init(true, Some(b"192.0.2.1"), 0, b"x", b"k", Arc::new(|_| b"\\who\\b".to_vec()));
    assert_eq!((code, b.is_none()), (E_GOA_BINDERROR, true));
    let client = udp(0);
    send(client, a.rec().qport, b"\\basic\\");
    qr_process_queries_no_heartbeat(None); // the current record: a's
    assert_eq!(drain(client), ["\\who\\a\\final\\\\queryid\\2.1"]);
    // the base port taken: the next one
    let (code, c) = start([Vec::new(), Vec::new(), Vec::new(), Vec::new()], a.rec().qport);
    assert_eq!(code, 0);
    assert_eq!(c.unwrap().rec().qport, a.rec().qport + 1);
    close(client);
}
