//! The GameSpy Query & Reporting SDK and WON's `GameSpySupport` over it, whole in Rust (net round 28; the C++
//! units `GameSpySupport.cpp`, `GameSpySDK/gqueryreporting.c` and `GameSpySDK/nonport.c` and the SDK's headers are
//! deleted, `GameSpySupport` a shim over [`EeWonGameSpy`]).
//!
//! **The SDK** (`gqueryreporting.c`, the 2000 edition WON shipped): a UDP socket answering a game browser's queries
//! (`\basic\`, `\info\`, `\rules\`, `\players\`, `\status\`, `\packets\`, `\echo\`, `\secure\`) with key\value
//! replies cut into packets of under 1,400 bytes, each ending with its `\queryid\N.M`, the last with `\final\`, a
//! `\secure\` challenge answered by `\validate\` (the secret key's stream cipher, [`gs_encrypt`], in the SDK's
//! base64, [`gs_encode`]), and heartbeats to the master server (`master.gamespy.com:27900`): at the first pump,
//! every 30 s until the first query (ten times at most), else every 5 minutes, and a `\statechanged\` one when
//! asked. Its process-wide statics stay so: the master's host ([`set_master_host`], `qr_hostname`), its address as
//! the last `qr_init` resolved it (`hbaddr`), and the current record (`current_rec`, the last one started, or the
//! static record), which `NULL` record arguments use. A record is an [`Arc`]: one whose start-up failed (`*qrec`
//! left NULL) pumps the current record, as the original did.
//!
//! **nonport.c**: the millisecond clock (`GetTickCount`), [`msleep`], the socket system's start and end, a socket's
//! blocking mode and Nagle, the random id ([`goa_get_unique_id`]: the original's registry value under
//! `Software\GameSpy\GameSpy 3D\Registration` is the config.yaml key of the Rust `RegKey`, the round 27 rule). The
//! console and Mac builds' parts (`__KATANA__`, `UNDER_CE`, `_MACOS`, `__mips64`) were never compiled for a platform
//! the game runs on and are not ported; `SetReceiveBufferSize` (no caller, and no `SO_RCVBUF` in the platform layer)
//! is not either.
//!
//! **GameSpySupport** (WON SDK, never called by the game): the four key\value maps the callbacks dump, the
//! instance's start-up and status (the original's error switch had no `break`s: every failure is
//! `WS_GameSpySupport_UnhandledError`), the auto pump (a C++ `AsyncOp` timer through the library's core table), the
//! hourly `/GameSpy` service lookup on the directory servers (a Rust request op on a `RustServerOp`), the pump and
//! the shutdown's `exiting` state change.
//!
//! Behaviour on the original's undefined paths: a map dump never writes past its buffer (the original's `sprintf`
//! did once the pairs passed its end), an empty secret key is a key of one NUL byte (a division by zero), a pump
//! before `Startup` pumps nothing (an uninitialised record pointer), `mStateChanged` starts false (uninitialised),
//! long game names and keys are kept whole (they overflowed the record's fixed arrays), a record's socket is closed
//! when its last holder drops it (the original never closed it), and a callback whose instance is gone answers an
//! empty dump. `golden_gamespy.txt` (won_gamespy_test, recorded over the C units before the move) is the check.

use std::collections::BTreeMap;
use std::ffi::{c_char, c_void, CString};
use std::ptr::null_mut;
use std::sync::{Arc, LazyLock, Mutex, MutexGuard, Weak};

use ee_plat_sys::constants::PLAT_SOCKET_WAIT_READ;
use ee_plat_sys::constants::{PlatSockAddrIn, PlatSocket, PLAT_AF_INET, PLAT_SOCKET_DGRAM, PLAT_SOCKET_INVALID};
use ee_plat_sys::socket::imp as plat;

use crate::authobj::ReLock;
use crate::cppcore::{self, EeWonCoreHooks, Hold};
use crate::hostop::{EeWonHostOp, Obj, SetValue};
use crate::reqhost::{Plain, ReqPlan, EE_WON_REQHOST_CONTEXT};

fn lock<T>(m: &Mutex<T>) -> MutexGuard<'_, T> {
    m.lock().unwrap_or_else(|e| e.into_inner())
}

// ---- nonport.c

/// `current_time()`: the system's millisecond clock (`GetTickCount`, wrapping at 32 bits).
pub fn current_time() -> u32 {
    #[cfg(test)]
    if let Some(t) = tests::clock() {
        return t;
    }
    ee_plat_core::time::PlatTimeMilliseconds64() as u32
}

/// `msleep(msec)`.
pub fn msleep(msec: u32) {
    std::thread::sleep(std::time::Duration::from_millis(u64::from(msec)));
}

/// `SocketStartUp()`: the socket system started (counted, as `WSAStartup`).
pub fn socket_startup() {
    // SAFETY: a plain platform call
    unsafe { plat::PlatSocketStartup() };
}

/// `SocketShutDown()`.
pub fn socket_shutdown() {
    // SAFETY: a plain platform call
    unsafe { plat::PlatSocketCleanup() };
}

/// `SetSockBlocking(sock, isblocking)`: 1 when set, else 0.
pub fn set_sock_blocking(sock: PlatSocket, blocking: bool) -> i32 {
    // SAFETY: the caller's socket
    i32::from(unsafe { plat::PlatSocketSetNonBlocking(sock, i32::from(!blocking)) } != 0)
}

/// `DisableNagle(sock)`: 1 when set, else 0.
pub fn disable_nagle(sock: PlatSocket) -> i32 {
    // SAFETY: the caller's socket
    i32::from(unsafe { plat::PlatSocketSetNoDelay(sock, 1) } != 0)
}

/// The SDK's random numbers (`nextlongrand`, Park and Miller's minimal standard over 32-bit `unsigned long`s).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct LongRand(pub i32);

impl LongRand {
    /// `Util_RandSeed`: a zero seed is 1.
    pub fn seeded(seed: u32) -> LongRand {
        LongRand(if seed != 0 { (seed & 0x7fff_ffff) as i32 } else { 1 })
    }

    fn next_long_rand(seed: i32) -> i32 {
        const RANA: u32 = 16807;
        const LONGRAND_MAX: u32 = 0x7fff_ffff;
        let mut lo = RANA.wrapping_mul(seed as u32 & 0xffff);
        let hi = RANA.wrapping_mul(seed as u32 >> 16);
        lo = lo.wrapping_add((hi & 0x7fff) << 16);
        if lo > LONGRAND_MAX {
            lo &= LONGRAND_MAX;
            lo += 1;
        }
        lo = lo.wrapping_add(hi >> 15);
        if lo > LONGRAND_MAX {
            lo &= LONGRAND_MAX;
            lo += 1;
        }
        lo as i32
    }

    /// `longrand()`.
    pub fn next_value(&mut self) -> i32 {
        self.0 = Self::next_long_rand(self.0);
        self.0
    }

    /// `Util_RandInt(low, high)`: `low + longrand() % (high - low)`.
    pub fn int(&mut self, low: i32, high: i32) -> i32 {
        let range = high - low;
        self.next_value() % range + low
    }
}

/// `GenerateID`: 19 characters of the id table, with dashes at 4, 9 and 14, from the seed (the original's was the
/// performance counter's halves, the tick count, the time and `clock()` xored).
pub fn generate_id(seed: u32) -> [u8; 19] {
    const CRYPTTAB: &[u8; 62] = b"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    let mut r = LongRand::seeded(seed);
    let mut id = [0u8; 19];
    for (i, c) in id.iter_mut().enumerate() {
        *c = if i == 4 || i == 9 || i == 14 { b'-' } else { CRYPTTAB[r.int(0, 62) as usize] };
    }
    id
}

/// `GOAGetUniqueID()`: this machine's id, 16 characters: the stored one's (19 with its dashes), or a new one made
/// and stored (the config.yaml key of `HKEY_CURRENT_USER\Software\GameSpy\GameSpy 3D\Registration`, value `Crypt`).
pub fn goa_get_unique_id() -> Vec<u8> {
    use crate::regkey::{EeWonRegKey, RegValue};
    let mut key = EeWonRegKey::default();
    key.open_new_key(1, b"Software\\GameSpy\\GameSpy 3D\\Registration");
    let mut id = key.get_value(b"Crypt").unwrap_or_default();
    if id.len() != 19 {
        let seed = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map_or(0, |d| d.subsec_nanos() ^ d.as_secs() as u32)
            ^ current_time();
        id = generate_id(seed).to_vec();
        key.set_value(b"Crypt", RegValue::Text(&id));
    }
    // the dashes taken out
    let mut out = Vec::with_capacity(16);
    out.extend_from_slice(&id[0..4]);
    out.extend_from_slice(&id[5..9]);
    out.extend_from_slice(&id[10..14]);
    out.extend_from_slice(&id[15..19]);
    out
}

// ---- the SDK's encryption and encoding

/// `encode_ct`: the SDK's base64 digit.
fn encode_ct(c: u8) -> u8 {
    match c {
        0..=25 => b'A' + c,
        26..=51 => b'a' + c - 26,
        52..=61 => b'0' + c - 52,
        62 => b'+',
        63 => b'/',
        _ => 0,
    }
}

/// `gs_encode(ins, size, result)`: four digits per three bytes, a short last group padded with zero bytes (no `=`).
pub fn gs_encode(ins: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(ins.len().div_ceil(3) * 4);
    for chunk in ins.chunks(3) {
        let t = [chunk[0], chunk.get(1).copied().unwrap_or(0), chunk.get(2).copied().unwrap_or(0)];
        for k in [t[0] >> 2, ((t[0] & 3) << 4) + (t[1] >> 4), ((t[1] & 15) << 2) + (t[2] >> 6), t[2] & 63] {
            out.push(encode_ct(k));
        }
    }
    out
}

/// `gs_encrypt(key, key_len, buffer, buffer_len)`: the key's RC4-like schedule, then a stream xored over the buffer
/// whose index steps by each plain byte. An empty key is one NUL byte (the original divided by zero).
pub fn gs_encrypt(key: &[u8], buffer: &mut [u8]) {
    let key: &[u8] = if key.is_empty() { &[0] } else { key };
    let key_len = key.len() as i32;
    let mut state = [0u8; 256];
    for (i, s) in state.iter_mut().enumerate() {
        *s = i as u8;
    }
    let (mut x, mut y) = (0u8, 0u8);
    for counter in 0..256 {
        y = key[x as usize].wrapping_add(state[counter]).wrapping_add(y);
        x = ((i32::from(x) + 1) % key_len) as u8;
        state.swap(counter, y as usize);
    }
    let (mut x, mut y) = (0u8, 0u8);
    for b in buffer.iter_mut() {
        x = x.wrapping_add(*b).wrapping_add(1);
        y = state[x as usize].wrapping_add(y);
        state.swap(x as usize, y as usize);
        let xor_index = state[x as usize].wrapping_add(state[y as usize]);
        *b ^= state[xor_index as usize];
    }
}

// ---- the SDK (gqueryreporting.c)

/// `qr_init`'s errors (the SDK's `E_GOA_*`).
pub const E_GOA_WSOCKERROR: i32 = 1;
pub const E_GOA_BINDERROR: i32 = 2;
pub const E_GOA_DNSERROR: i32 = 3;
pub const E_GOA_CONNERROR: i32 = 4;
/// `NUM_PORTS_TO_TRY`: the ports `qr_init` scans from its base port.
pub const NUM_PORTS_TO_TRY: i32 = 100;
const MASTER_PORT: u16 = 27900;
const FIRST_HB_TIME: u32 = 30000;
const HB_TIME: u32 = 300_000;
const MAX_FIRST_COUNT: i8 = 10;
const MAX_DATA_SIZE: usize = 1400;
/// `INBUF_LEN - 1`: the most of a query read
const QUERY_MAX: u32 = 255;
/// `BUF_SIZE`: a callback's buffer
pub const BUF_SIZE: usize = 1400;
const QUERIES: [&[u8]; 9] = [b"", b"basic", b"info", b"rules", b"players", b"status", b"packets", b"echo", b"secure"];

/// The four query callbacks in one (`qr_basic_callback` .. `qr_players_callback` with their `udata`): the kind
/// (0 basic, 1 info, 2 rules, 3 players) to the text the callback wrote into its zeroed [`BUF_SIZE`] buffer (up to
/// its first NUL).
pub type QueryCallback = Arc<dyn Fn(usize) -> Vec<u8> + Send + Sync>;

/// `struct qr_implementation_s`.
#[derive(Clone)]
pub struct QrRec {
    pub querysock: PlatSocket,
    pub hbsock: PlatSocket,
    pub gamename: Vec<u8>,
    pub secret_key: Vec<u8>,
    pub callback: Option<QueryCallback>,
    pub lastheartbeat: u32,
    pub queryid: i32,
    pub packetnumber: i32,
    pub qport: i32,
    pub no_query: i8,
}

impl Default for QrRec {
    /// `static_rec`: no sockets, the rest zero.
    fn default() -> QrRec {
        QrRec {
            querysock: PLAT_SOCKET_INVALID,
            hbsock: PLAT_SOCKET_INVALID,
            gamename: Vec::new(),
            secret_key: Vec::new(),
            callback: None,
            lastheartbeat: 0,
            queryid: 0,
            packetnumber: 0,
            qport: 0,
            no_query: 0,
        }
    }
}

/// A record (`qr_t`): the SDK's functions take one, or none for the current record.
pub struct Qr(Mutex<QrRec>);
pub type QrHandle = Arc<Qr>;

impl Drop for Qr {
    fn drop(&mut self) {
        let r = lock(&self.0);
        if r.querysock != PLAT_SOCKET_INVALID {
            // SAFETY: the record's own socket, closed once
            unsafe { plat::PlatSocketClose(r.querysock) };
        }
    }
}

impl Qr {
    pub fn rec(&self) -> QrRec {
        lock(&self.0).clone()
    }
    fn with<R>(&self, f: impl FnOnce(&mut QrRec) -> R) -> R {
        f(&mut lock(&self.0))
    }
}

/// `qr_hostname` ("master.gamespy.com"), and the master's port (27900; a test's stand-in may move it).
static MASTER: Mutex<(Vec<u8>, u16)> = Mutex::new((Vec::new(), MASTER_PORT));
/// `hbaddr`: the master's address, as the last `qr_init` resolved it.
static HB_ADDR: Mutex<PlatSockAddrIn> = Mutex::new(PlatSockAddrIn { mFamily: 0, mPort: 0, mAddress: 0, mZero: [0; 8] });
/// `static_rec` and `current_rec`.
static STATIC_REC: LazyLock<QrHandle> = LazyLock::new(|| Arc::new(Qr(Mutex::new(QrRec::default()))));
static CURRENT: LazyLock<Mutex<QrHandle>> = LazyLock::new(|| Mutex::new(STATIC_REC.clone()));

/// Sets `qr_hostname` (read by the next `qr_init`).
pub fn set_master_host(host: &[u8]) {
    lock(&MASTER).0 = host.to_vec();
}
/// The master's port (27900): a test stand-in's.
pub fn set_master_port(port: u16) {
    lock(&MASTER).1 = port;
}
fn master() -> (Vec<u8>, u16) {
    let m = lock(&MASTER);
    (if m.0.is_empty() { b"master.gamespy.com".to_vec() } else { m.0.clone() }, m.1)
}

/// The record a function acts on: the given one, else the current one.
fn rec_or_current(qrec: Option<&QrHandle>) -> QrHandle {
    qrec.cloned().unwrap_or_else(|| lock(&CURRENT).clone())
}

/// The bytes up to a NUL (a C string's).
fn c_bytes(s: &[u8]) -> &[u8] {
    s.split(|&c| c == 0).next().unwrap_or(&[])
}

fn c_string(s: &[u8]) -> CString {
    CString::new(c_bytes(s)).unwrap_or_default()
}

/// `get_sockaddrin(host, port, saddr, NULL)`: the address (a dotted IP, else the host's first address); false when
/// the host does not resolve (the address is then `INADDR_NONE`).
fn get_sockaddrin(host: Option<&[u8]>, port: i32) -> (bool, PlatSockAddrIn) {
    let mut a = PlatSockAddrIn { mFamily: PLAT_AF_INET, mPort: (port as u16).to_be(), ..Default::default() };
    let Some(host) = host else {
        return (true, a);
    };
    let text = c_string(host);
    // SAFETY: a C string
    a.mAddress = unsafe { plat::PlatSocketParseIPv4(text.as_ptr()) };
    if a.mAddress == 0xffff_ffff && text.as_bytes() != b"255.255.255.255" {
        let mut found = [0u32; 1];
        // SAFETY: a C string and room for one address
        if unsafe { plat::PlatSocketResolveIPv4(text.as_ptr(), found.as_mut_ptr(), 1) } == 0 {
            return (false, a);
        }
        a.mAddress = found[0];
    }
    (true, a)
}

/// `qr_init(qrec, ip, baseport, gamename, secret_key, callbacks, userdata)`: 0 and the new record (made current),
/// or an `E_GOA_*` error. `want_rec` false is a NULL `qrec`: the static record is filled and made current.
pub fn qr_init(
    want_rec: bool,
    ip: Option<&[u8]>,
    mut baseport: i32,
    gamename: &[u8],
    secret_key: &[u8],
    callback: QueryCallback,
) -> (i32, Option<QrHandle>) {
    socket_startup();
    // SAFETY: a plain platform call
    let querysock = unsafe { plat::PlatSocketOpen(PLAT_SOCKET_DGRAM) };
    if querysock == PLAT_SOCKET_INVALID {
        return (E_GOA_WSOCKERROR, None);
    }
    let maxport = baseport + NUM_PORTS_TO_TRY;
    let mut bound = false;
    while baseport < maxport {
        let (_, mut saddr) = get_sockaddrin(ip, baseport);
        if saddr.mAddress == 0x7f00_0001u32.to_be() {
            saddr.mAddress = 0; // localhost: INADDR_ANY instead
        }
        // SAFETY: the new socket and an address
        if unsafe { plat::PlatSocketBind(querysock, &saddr) } != 0 {
            bound = true;
            break;
        }
        baseport += 1;
    }
    let close = |code: i32| {
        // SAFETY: the new socket, closed once (the original left it open)
        unsafe { plat::PlatSocketClose(querysock) };
        (code, None)
    };
    if !bound {
        return close(E_GOA_BINDERROR);
    }
    if baseport == 0 {
        let mut saddr = PlatSockAddrIn::default();
        // SAFETY: the bound socket and a writable address
        if unsafe { plat::PlatSocketGetName(querysock, &mut saddr) } == 0 {
            return close(E_GOA_BINDERROR);
        }
        baseport = i32::from(u16::from_be(saddr.mPort));
    }
    let rec = QrRec {
        querysock,
        hbsock: querysock, // one socket for both (no TCP_HEARTBEATS)
        gamename: c_bytes(gamename).to_vec(),
        secret_key: c_bytes(secret_key).to_vec(),
        callback: Some(callback),
        lastheartbeat: 0,
        queryid: 1,
        packetnumber: 0,
        qport: baseport,
        no_query: 1,
    };
    let handle = if want_rec {
        Arc::new(Qr(Mutex::new(rec)))
    } else {
        STATIC_REC.with(|r| *r = rec);
        STATIC_REC.clone()
    };
    *lock(&CURRENT) = handle.clone();
    // do_connect: the master's address resolved (INADDR_NONE when it does not)
    let (host, port) = master();
    *lock(&HB_ADDR) = get_sockaddrin(Some(&host), i32::from(port)).1;
    (0, if want_rec { Some(handle) } else { None })
}

/// `qr_process_queries(qrec)`: a heartbeat when due, then the waiting queries.
pub fn qr_process_queries(qrec: Option<&QrHandle>) {
    let r = rec_or_current(qrec);
    qr_check_send_heartbeat(&r);
    qr_check_queries(&r);
}

/// `qr_process_queries_no_heartbeat(qrec)`: the waiting queries only (a LAN-only server).
pub fn qr_process_queries_no_heartbeat(qrec: Option<&QrHandle>) {
    qr_check_queries(&rec_or_current(qrec));
}

/// `qr_send_statechanged(qrec)`: a `\statechanged\` heartbeat now.
pub fn qr_send_statechanged(qrec: Option<&QrHandle>) {
    send_heartbeat(&rec_or_current(qrec), true);
}

/// `qr_shutdown(qrec)`: the socket closed, the record cleared, the socket system's start undone.
pub fn qr_shutdown(qrec: Option<&QrHandle>) {
    let r = rec_or_current(qrec);
    r.with(|r| {
        if r.querysock != PLAT_SOCKET_INVALID {
            // SAFETY: the record's socket, closed once (it is cleared below)
            unsafe { plat::PlatSocketClose(r.querysock) };
        }
        if r.hbsock != PLAT_SOCKET_INVALID && r.hbsock != r.querysock {
            // SAFETY: as above
            unsafe { plat::PlatSocketClose(r.hbsock) };
        }
        r.hbsock = PLAT_SOCKET_INVALID;
        r.querysock = PLAT_SOCKET_INVALID;
        r.lastheartbeat = 0;
    });
    socket_shutdown();
}

/// `qr_check_queries(qrec)`: every query waiting on the socket read (255 bytes at most, as the platform's
/// `recvfrom` gives them: Winsock drops a longer datagram) and answered.
pub fn qr_check_queries(qrec: &QrHandle) {
    let sock = qrec.rec().querysock;
    if sock == PLAT_SOCKET_INVALID {
        return;
    }
    loop {
        // SAFETY: the record's socket
        if unsafe { plat::PlatSocketWait(sock, PLAT_SOCKET_WAIT_READ, 0) } <= 0 {
            return;
        }
        let mut data = [0u8; QUERY_MAX as usize + 1];
        let mut from = PlatSockAddrIn::default();
        // SAFETY: the socket, a buffer of the length given and a writable address
        let n = unsafe { plat::PlatSocketRecvFrom(sock, data.as_mut_ptr().cast(), QUERY_MAX, &mut from) };
        if n >= 0 {
            parse_query(qrec, c_bytes(&data[..n as usize]), &from);
        }
    }
}

/// `qr_check_send_heartbeat(qrec)`: the 5-minute heartbeat (or the first, or after the clock wrapped), else while
/// no query came the 30-second one, ten times at most.
pub fn qr_check_send_heartbeat(qrec: &QrHandle) {
    let tc = current_time();
    let r = qrec.rec();
    if r.hbsock == PLAT_SOCKET_INVALID {
        return;
    }
    if tc.wrapping_sub(r.lastheartbeat) > HB_TIME || r.lastheartbeat == 0 || tc < r.lastheartbeat {
        send_heartbeat(qrec, false);
    } else if r.no_query > 0 && tc.wrapping_sub(r.lastheartbeat) > FIRST_HB_TIME {
        send_heartbeat(qrec, false);
        qrec.with(|r| {
            r.no_query += 1;
            if r.no_query > MAX_FIRST_COUNT {
                r.no_query = 0;
            }
        });
    }
}

/// `send_heartbeat(qrec, statechanged)`: `\heartbeat\<port>\gamename\<name>` (and `\statechanged\`) to the master.
fn send_heartbeat(qrec: &QrHandle, statechanged: bool) {
    let r = qrec.rec();
    let mut buf = format!("\\heartbeat\\{}\\gamename\\", r.qport).into_bytes();
    buf.extend_from_slice(&r.gamename);
    if statechanged {
        buf.extend_from_slice(b"\\statechanged\\");
    }
    let to = *lock(&HB_ADDR);
    // SAFETY: the record's socket (an invalid one fails), the bytes and an address
    unsafe { plat::PlatSocketSendTo(r.hbsock, buf.as_ptr().cast(), buf.len() as u32, &to) };
    qrec.with(|r| r.lastheartbeat = current_time());
}

/// `value_for_key(s, key)`: the value after the first `\key\` in the query, up to the next `\` (None: no key).
pub fn value_for_key<'a>(s: &'a [u8], key: &[u8]) -> Option<&'a [u8]> {
    let mut spec = vec![b'\\'];
    spec.extend_from_slice(key);
    spec.push(b'\\');
    let at = s.windows(spec.len()).position(|w| w == spec.as_slice())? + spec.len();
    let rest = &s[at..];
    Some(&rest[..rest.iter().position(|&c| c == b'\\').unwrap_or(rest.len())])
}

/// One reply's packets: where they go and the record whose numbers they take.
struct Reply<'a> {
    qrec: &'a QrHandle,
    to: &'a PlatSockAddrIn,
    buffer: Vec<u8>,
}

impl Reply<'_> {
    /// `packet_send`: the buffer with its `\queryid\<query>.<packet>` sent and cleared (an empty one is not sent).
    fn packet_send(&mut self) {
        if self.buffer.is_empty() {
            return;
        }
        let (sock, id, packet) = self.qrec.with(|r| {
            r.packetnumber += 1;
            (r.querysock, r.queryid, r.packetnumber)
        });
        self.buffer.extend_from_slice(format!("\\queryid\\{id}.{packet}").as_bytes());
        // SAFETY: the record's socket, the bytes and the sender's address
        unsafe { plat::PlatSocketSendTo(sock, self.buffer.as_ptr().cast(), self.buffer.len() as u32, self.to) };
        self.buffer.clear();
    }

    /// `buffer_send`: appended while under 1,350 bytes, else the buffer sent first; data over 1,350 bytes cut
    /// before the last key that starts within them (a single key that long is dropped).
    fn buffer_send(&mut self, newdata: &[u8]) {
        const LIMIT: usize = MAX_DATA_SIZE - 50;
        if self.buffer.len() + newdata.len() < LIMIT {
            self.buffer.extend_from_slice(newdata);
        } else if newdata.len() > LIMIT {
            let (mut lastkey, mut bcount) = (0usize, 0u32);
            for (pos, &c) in newdata[..LIMIT].iter().enumerate() {
                if c == b'\\' {
                    if bcount % 2 == 0 {
                        lastkey = pos;
                    }
                    bcount += 1;
                }
            }
            if lastkey == 0 {
                return; // a single key too big
            }
            self.buffer_send(&newdata[..lastkey]);
            self.buffer_send(&newdata[lastkey..]);
        } else {
            self.packet_send();
            self.buffer = newdata.to_vec();
        }
    }

    /// `send_basic` .. `send_players`: the callback's text.
    fn send_kind(&mut self, kind: usize) {
        let callback = self.qrec.rec().callback;
        let text = callback.map(|c| c(kind)).unwrap_or_default();
        self.buffer_send(c_bytes(&text[..text.len().min(BUF_SIZE - 1)]));
    }

    /// `send_echo`: `\echo\<value>` (a value over 1,350 bytes is not echoed).
    fn send_echo(&mut self, echo: &[u8]) {
        if echo.len() > MAX_DATA_SIZE - 50 {
            return;
        }
        let mut kv = b"\\echo\\".to_vec();
        kv.extend_from_slice(echo);
        self.buffer_send(&kv);
    }

    /// `send_final`: the `\validate\` answer to a challenge (none over 128 bytes: then nothing more is sent at
    /// all), `\final\` and the last packet.
    fn send_final(&mut self, validation: &[u8]) {
        if !validation.is_empty() {
            if validation.len() > 128 {
                return;
            }
            let mut encrypted = validation.to_vec();
            gs_encrypt(&self.qrec.rec().secret_key, &mut encrypted);
            let mut kv = b"\\validate\\".to_vec();
            kv.extend_from_slice(&gs_encode(&encrypted));
            self.buffer_send(&kv);
        }
        self.buffer_send(b"\\final\\");
        self.packet_send();
    }
}

/// `parse_query(qrec, query, sender)`: a new query id, then each query type the text names, in the types' order.
fn parse_query(qrec: &QrHandle, query: &[u8], to: &PlatSockAddrIn) {
    qrec.with(|r| {
        r.queryid += 1;
        r.packetnumber = 0;
        if r.no_query > 0 {
            r.no_query = 0;
        }
    });
    let mut reply = Reply { qrec, to, buffer: Vec::new() };
    let mut validation = Vec::new();
    for (kind, key) in QUERIES.iter().enumerate().skip(1) {
        let Some(value) = value_for_key(query, key) else {
            continue;
        };
        match kind {
            1..=4 => reply.send_kind(kind - 1),
            5 => (0..4).for_each(|k| reply.send_kind(k)),
            6 => {
                // "packets": each map in its own packet (the SDK's illustration of a large reply)
                for k in 0..4 {
                    reply.send_kind(k);
                    if k < 3 {
                        reply.packet_send();
                    }
                }
            }
            7 => reply.send_echo(value),
            _ => validation = value.to_vec(),
        }
    }
    reply.send_final(&validation);
}

// ---- GameSpySupport

/// `GameSpyQueryType`'s maps: 0 the basic keys, 1 info, 2 rules, 3 players.
pub const EE_WON_GAMESPY_MAPS: usize = 4;
/// `WS_GameSpySupport_UnhandledError`
const WS_GAMESPY_UNHANDLED: i32 = 100106;
/// `WS_DirServ_ServiceNotFound`
const WS_DIRSERV_SERVICE_NOT_FOUND: i64 = -1201;
/// `AsyncOp::RunAsync`'s mode
const MODE_ASYNC: i32 = 1;

type KeyValueMap = BTreeMap<Vec<u8>, CString>;

/// What the SDK's callbacks reach: the lock (`mDataCrit`) and the maps (`mQueryMap[4]`).
#[derive(Default)]
struct Shared {
    lock: ReLock,
    maps: Mutex<[KeyValueMap; EE_WON_GAMESPY_MAPS]>,
}

impl Shared {
    /// `DumpKeyValueMapToBuffer(map, outbuf, maxlen)`: `\key\value` for each pair that fits before the buffer's
    /// end; a pair that does not leaves a gap (the dump's text ends there) and moves on by its length.
    fn dump(&self, kind: usize, maxlen: usize) -> Vec<u8> {
        let _held = self.lock.lock();
        let maps = lock(&self.maps);
        let mut out = vec![0u8; maxlen];
        let mut pos = 0u32;
        for (k, v) in &maps[kind] {
            let pair = (k.len() + v.as_bytes().len() + 2) as u32;
            if pair < (maxlen as u32).wrapping_sub(pos) && (pos as usize + pair as usize) < maxlen {
                let at = pos as usize;
                out[at] = b'\\';
                out[at + 1..at + 1 + k.len()].copy_from_slice(k);
                out[at + 1 + k.len()] = b'\\';
                out[at + 2 + k.len()..at + pair as usize].copy_from_slice(v.as_bytes());
                out[at + pair as usize] = 0;
            }
            pos = pos.wrapping_add(pair);
        }
        c_bytes(&out).to_vec()
    }
}

/// `GameSpySupport`'s other members.
struct State {
    qr: Option<QrHandle>,
    secret_key: Vec<u8>,
    state_changed: bool,
    initialized: bool,
    status: i32,
    gamespy_status: i32,
    auto_pump_time: u32,
    /// `mPumpTimerOp`, `mGetServiceOp`: C++ ops with a reference each
    pump_timer: Hold,
    get_service: Hold,
    disabled: bool,
    last_get_service: i64,
    /// `Startup` called (a pump before it pumps nothing)
    started: bool,
}

/// `GameSpySupport` (the C++ class a `RefCount` shim holding this).
pub struct EeWonGameSpy {
    shared: Arc<Shared>,
    state: Mutex<State>,
    /// the C++ `GameSpySupport*` (its completions hold references to it, as the `ParamCompletion`s did)
    shim: usize,
    /// `mDirServers` (a reference held)
    dir_servers: Hold,
}

impl EeWonGameSpy {
    /// `GameSpySupport(name, version, secret key, directory servers)`: the basic keys `gamename` and `gamever`.
    pub fn new(name: &[u8], version: &[u8], secret_key: &[u8], shim: *mut c_void, dirs: *mut c_void) -> EeWonGameSpy {
        let shared = Arc::new(Shared::default());
        {
            let mut maps = lock(&shared.maps);
            maps[0].insert(b"gamename".to_vec(), c_string(name));
            maps[0].insert(b"gamever".to_vec(), c_string(version));
        }
        EeWonGameSpy {
            shared,
            state: Mutex::new(State {
                qr: None,
                secret_key: c_bytes(secret_key).to_vec(),
                state_changed: false,
                initialized: false,
                status: crate::status::WS_NONE as i32,
                gamespy_status: 0,
                auto_pump_time: 0,
                pump_timer: cppcore::hold(null_mut()),
                get_service: cppcore::hold(null_mut()),
                disabled: false,
                last_get_service: 0,
                started: false,
            }),
            shim: shim as usize,
            dir_servers: cppcore::hold(dirs),
        }
    }

    fn st(&self) -> MutexGuard<'_, State> {
        lock(&self.state)
    }

    /// `mQueryMap[type]["gamename"]` (operator[]: made empty when missing).
    fn gamename(&self) -> Vec<u8> {
        let mut maps = lock(&self.shared.maps);
        maps[0].entry(b"gamename".to_vec()).or_default().as_bytes().to_vec()
    }

    /// `Startup(addr)`: the SDK started on the address's host string and port, disabled until the first service
    /// lookup answers; the status (`WS_Success`, or for any SDK error `WS_GameSpySupport_UnhandledError`).
    pub fn startup(&self, host: &[u8], port: u16) -> i32 {
        {
            let mut st = self.st();
            st.last_get_service = 0;
            st.disabled = true;
            st.started = true;
        }
        let weak: Weak<Shared> = Arc::downgrade(&self.shared);
        let callback: QueryCallback =
            Arc::new(move |kind| weak.upgrade().map(|s| s.dump(kind, BUF_SIZE)).unwrap_or_default());
        let secret = self.st().secret_key.clone();
        let (code, qr) = qr_init(true, Some(host), i32::from(port), &self.gamename(), &secret, callback);
        let start_pump = {
            let mut st = self.st();
            st.qr = qr;
            st.gamespy_status = code;
            st.status = if code == 0 { 0 } else { WS_GAMESPY_UNHANDLED };
            st.initialized |= code == 0;
            code == 0
        };
        if start_pump {
            self.begin_auto_pump();
        }
        self.st().status
    }

    /// The pump timer's or the service lookup's completion: a reference to the shim while it lives, this object
    /// reached through it.
    fn completion(&self, call: fn(&EeWonGameSpy, *mut c_void)) -> *mut c_void {
        let held = cppcore::hold(self.shim as *mut c_void);
        let me = self as *const EeWonGameSpy as usize;
        cppcore::completion(move |op| {
            let _ = &held;
            // SAFETY: `held` keeps the C++ shim, which owns this object, alive while the completion lives
            call(unsafe { &*(me as *const EeWonGameSpy) }, op);
        })
    }

    /// `BeginAutoPump`: the timer killed and dropped, and a new one armed for a non-zero time.
    fn begin_auto_pump(&self) {
        let _held = self.shared.lock.lock();
        let old = std::mem::replace(&mut self.st().pump_timer, cppcore::hold(null_mut()));
        if let Some(op) = cppcore::op(old.get()) {
            op.kill(true);
        }
        drop(old);
        let time = self.st().auto_pump_time;
        if time != 0 {
            // SAFETY: the table's contract
            let t = cppcore::core().and_then(|c| c.new_op).map_or(null_mut(), |h| unsafe { h() });
            let timer = cppcore::hold(t);
            if let Some(op) = cppcore::op(t) {
                let c = self.completion(|me, op| {
                    // PumpTimerCompletion: nothing for a killed timer
                    if cppcore::op(op).is_some_and(|o| !o.killed()) {
                        me.pump();
                    }
                });
                // SAFETY: a new completion
                unsafe { op.set_completion(c) };
                self.st().pump_timer = timer;
                op.run_as_timer(time);
            }
        }
    }

    /// `SetAutoPumpTime(ms)`.
    pub fn set_auto_pump_time(&self, ms: u32) {
        let _held = self.shared.lock.lock();
        self.st().auto_pump_time = ms;
        self.begin_auto_pump();
    }

    /// `IsAutoPumping()`: started, with a pump time.
    pub fn is_auto_pumping(&self) -> bool {
        let st = self.st();
        st.initialized && st.auto_pump_time != 0
    }

    /// `LaunchGetServiceOp`: the `/GameSpy` service of the game's name looked up on the directory servers (a
    /// running lookup killed first), its completion enabling the instance unless the service is not found.
    fn launch_get_service(&self) {
        let running = self.st().get_service.get();
        if let Some(op) = cppcore::op(running) {
            op.kill(true);
        }
        let name = self.gamename();
        let wide =
            |t: &[u8]| -> Vec<u8> { String::from_utf8_lossy(t).encode_utf16().flat_map(u16::to_le_bytes).collect() };
        use crate::adminop::Member;
        let n = |num: i64| Member { num, bytes: Vec::new() };
        let b = |bytes: Vec<u8>| Member { num: 0, bytes };
        let members = [n(0x0600_0822), b(wide(b"/GameSpy")), b(wide(&name)), n(1), b(name.clone()), n(0)];
        let Some(request) = crate::reqops::new_op(crate::reqdir::EE_WON_REQOPS_DIR_SERVICE, &members) else {
            return;
        };
        let host = EeWonHostOp::boxed(ReqPlan::new(Box::new(Plain(request))));
        let cpp = cppcore::new_host_op(host);
        if cpp.is_null() {
            return;
        }
        let held = cppcore::hold(cpp);
        // SAFETY: the C++ op owns `host` and lives while `held` does
        unsafe {
            (*host).set(
                EE_WON_REQHOST_CONTEXT,
                &SetValue { num: 0, data: &[], addr: [0; 6], obj: Obj::of(self.dir_servers.get()) },
            )
        };
        let Some(op) = cppcore::op(cpp) else {
            return;
        };
        let c = self.completion(|me, op| me.get_service_done(op));
        // SAFETY: a new completion
        unsafe { op.set_completion(c) };
        let old = std::mem::replace(&mut self.st().get_service, cppcore::hold(cpp));
        drop(old);
        op.run(MODE_ASYNC, 60000);
        drop(held);
    }

    /// `GetServiceOpCompletion(op)`: the current lookup's answer (a killed or an older one is ignored).
    fn get_service_done(&self, op: *mut c_void) {
        let _held = self.shared.lock.lock();
        let Some(o) = cppcore::op(op) else {
            return;
        };
        if o.killed() || op != self.st().get_service.get() {
            return;
        }
        let old = {
            let mut st = self.st();
            st.disabled = o.status() == WS_DIRSERV_SERVICE_NOT_FOUND;
            std::mem::replace(&mut st.get_service, cppcore::hold(null_mut()))
        };
        drop(old);
    }

    /// `Pump()`: the queries and a due heartbeat (unless disabled) and a state change's heartbeat, the auto pump's
    /// timer again, and the service lookup once an hour.
    pub fn pump(&self) {
        let _held = self.shared.lock.lock();
        let (disabled, started, qr) = {
            let st = self.st();
            (st.disabled, st.started, st.qr.clone())
        };
        // (a pump before Startup has no record: the original's was uninitialised)
        if !disabled && started {
            qr_process_queries(qr.as_ref());
            let changed = std::mem::take(&mut self.st().state_changed);
            if changed {
                qr_send_statechanged(qr.as_ref());
            }
        }
        if self.is_auto_pumping() {
            let (t, time) = {
                let st = self.st();
                (st.pump_timer.get(), st.auto_pump_time)
            };
            if let Some(op) = cppcore::op(t) {
                op.run_as_timer(time);
            }
        }
        let now = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map_or(0, |d| d.as_secs() as i64);
        let due = {
            let mut st = self.st();
            let due = now - st.last_get_service >= 3600;
            if due {
                st.last_get_service = now;
            }
            due
        };
        if due {
            self.launch_get_service();
        }
    }

    /// `Shutdown()`: no more auto pumps, the lookup killed, `gamemode` `exiting` and a last pump to send it.
    pub fn shutdown(&self) {
        self.set_auto_pump_time(0);
        {
            let _held = self.shared.lock.lock();
            let old = std::mem::replace(&mut self.st().get_service, cppcore::hold(null_mut()));
            if let Some(op) = cppcore::op(old.get()) {
                op.kill(true);
            }
            drop(old);
        }
        self.set_game_mode(b"exiting");
        self.pump();
    }

    /// `SetGameMode(mode)`: the info key `gamemode`, and a state change for the next pump.
    pub fn set_game_mode(&self, mode: &[u8]) {
        self.set_key_value(1, b"gamemode", mode);
        self.st().state_changed = true;
    }

    /// `SetKeyValue(type, key, value)`.
    pub fn set_key_value(&self, kind: usize, key: &[u8], value: &[u8]) {
        let _held = self.shared.lock.lock();
        if let Some(m) = lock(&self.shared.maps).get_mut(kind) {
            m.insert(c_bytes(key).to_vec(), c_string(value));
        }
    }

    /// `GetKeyValue(type, key)`: the value's C string (valid until the key changes), or NULL.
    pub fn key_value(&self, kind: usize, key: &[u8]) -> *const c_char {
        lock(&self.shared.maps).get(kind).and_then(|m| m.get(c_bytes(key))).map_or(std::ptr::null(), |v| v.as_ptr())
    }

    /// `EraseKeyValue(type, key)`: whether it was there.
    pub fn erase_key_value(&self, kind: usize, key: &[u8]) -> bool {
        let _held = self.shared.lock.lock();
        lock(&self.shared.maps).get_mut(kind).is_some_and(|m| m.remove(key).is_some())
    }

    /// `EraseAllKeyValues(type)`.
    pub fn erase_all(&self, kind: usize) {
        let _held = self.shared.lock.lock();
        if let Some(m) = lock(&self.shared.maps).get_mut(kind) {
            m.clear();
        }
    }

    /// `LockKeyValues()` / `UnlockKeyValues()`: the lock held across calls.
    pub fn lock_key_values(&self) {
        self.shared.lock.enter();
    }
    pub fn unlock_key_values(&self) {
        self.shared.lock.leave();
    }

    pub fn status(&self) -> i32 {
        self.st().status
    }
    pub fn gamespy_status(&self) -> i32 {
        self.st().gamespy_status
    }
}

impl Drop for EeWonGameSpy {
    /// The members in the original's destruction order: the directory servers, the lookup, the timer.
    fn drop(&mut self) {
        let st = self.state.get_mut().unwrap_or_else(|e| e.into_inner());
        let lookup = std::mem::replace(&mut st.get_service, cppcore::hold(null_mut()));
        let timer = std::mem::replace(&mut st.pump_timer, cppcore::hold(null_mut()));
        drop(std::mem::replace(&mut self.dir_servers, cppcore::hold(null_mut())));
        drop(lookup);
        drop(timer);
    }
}

// ---- the C ABI (WONGameSpySupport.h)

/// `EE_WON_GAMESPY_*`: [`ee_won_gamespy_op`]'s calls.
pub const EE_WON_GAMESPY_SHUTDOWN: i32 = 0;
pub const EE_WON_GAMESPY_PUMP: i32 = 1;
/// the number: the time in ms
pub const EE_WON_GAMESPY_SET_AUTO_PUMP: i32 = 2;
pub const EE_WON_GAMESPY_IS_AUTO_PUMPING: i32 = 3;
pub const EE_WON_GAMESPY_STATUS: i32 = 4;
pub const EE_WON_GAMESPY_GAMESPY_STATUS: i32 = 5;
pub const EE_WON_GAMESPY_LOCK: i32 = 6;
pub const EE_WON_GAMESPY_UNLOCK: i32 = 7;
/// the number: the map
pub const EE_WON_GAMESPY_ERASE_ALL: i32 = 8;

/// A new instance: C strings for the name, version and key; the shim (`GameSpySupport*`), its directory servers
/// (`ServerContext*`, a reference taken) and the library's core table (bound).
///
/// # Safety
/// The strings are NUL-ended; the objects live C++ objects (the shim owns the result).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gamespy_new(
    name: *const c_char,
    version: *const c_char,
    secret_key: *const c_char,
    shim: *mut c_void,
    dirs: *mut c_void,
    core: *const EeWonCoreHooks,
) -> *mut EeWonGameSpy {
    cppcore::bind(core);
    // SAFETY: the contract's C strings
    let text = |s: *const c_char| if s.is_null() { &[][..] } else { unsafe { std::ffi::CStr::from_ptr(s) }.to_bytes() };
    Box::into_raw(Box::new(EeWonGameSpy::new(text(name), text(version), text(secret_key), shim, dirs)))
}

/// # Safety
/// `g` from [`ee_won_gamespy_new`], freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gamespy_free(g: *mut EeWonGameSpy) {
    if !g.is_null() {
        // SAFETY: the constructor's box
        drop(unsafe { Box::from_raw(g) });
    }
}

/// `Startup(addr)`: the address's `GetHostString(true)` and port.
///
/// # Safety
/// `g` live; `host` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gamespy_startup(
    g: *const EeWonGameSpy,
    host: *const u8,
    length: usize,
    port: u16,
) -> i32 {
    // SAFETY: the contract
    unsafe { (*g).startup(crate::ffi::bytes(host, length), port) }
}

/// The calls without arguments but a number ([`EE_WON_GAMESPY_SHUTDOWN`] ..): their result (0 for none).
///
/// # Safety
/// `g` live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gamespy_op(g: *const EeWonGameSpy, which: i32, num: i64) -> i64 {
    // SAFETY: the contract
    let g = unsafe { &*g };
    match which {
        EE_WON_GAMESPY_SHUTDOWN => g.shutdown(),
        EE_WON_GAMESPY_PUMP => g.pump(),
        EE_WON_GAMESPY_SET_AUTO_PUMP => g.set_auto_pump_time(num as u32),
        EE_WON_GAMESPY_IS_AUTO_PUMPING => return i64::from(g.is_auto_pumping()),
        EE_WON_GAMESPY_STATUS => return i64::from(g.status()),
        EE_WON_GAMESPY_GAMESPY_STATUS => return i64::from(g.gamespy_status()),
        EE_WON_GAMESPY_LOCK => g.lock_key_values(),
        EE_WON_GAMESPY_UNLOCK => g.unlock_key_values(),
        EE_WON_GAMESPY_ERASE_ALL => g.erase_all(num as usize),
        _ => {}
    }
    0
}

/// `SetKeyValue(type, key, value)` (C strings); a NULL value erases the key and returns whether it was there
/// (`EraseKeyValue`); `SetGameMode(value)` for a NULL key.
///
/// # Safety
/// `g` live; the strings NUL-ended or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gamespy_set(
    g: *const EeWonGameSpy,
    kind: i32,
    key: *const c_char,
    value: *const c_char,
) -> bool {
    // SAFETY: the contract
    let (g, text) = (unsafe { &*g }, |s: *const c_char| unsafe { std::ffi::CStr::from_ptr(s) }.to_bytes());
    match (key.is_null(), value.is_null()) {
        (true, false) => g.set_game_mode(text(value)),
        (false, true) => return g.erase_key_value(kind as usize, text(key)),
        (false, false) => g.set_key_value(kind as usize, text(key), text(value)),
        _ => {}
    }
    false
}

/// `GetKeyValue(type, key)`: the value's C string, valid until the key changes, or NULL.
///
/// # Safety
/// `g` live; `key` NUL-ended.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gamespy_get(g: *const EeWonGameSpy, kind: i32, key: *const c_char) -> *const c_char {
    // SAFETY: the contract
    unsafe { (*g).key_value(kind as usize, std::ffi::CStr::from_ptr(key).to_bytes()) }
}

/// `qr_hostname`: the master server's host the next start-up resolves (a test's stand-in).
///
/// # Safety
/// `host` NUL-ended.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gamespy_set_master_host(host: *const c_char) {
    // SAFETY: the contract
    set_master_host(unsafe { std::ffi::CStr::from_ptr(host) }.to_bytes());
}

#[cfg(test)]
#[path = "gamespy_tests.rs"]
mod tests;
