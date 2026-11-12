//! The decisions of three WONMisc and WONInternal ops: `FindPatchOp`'s patch choice (which patches lead from a
//! version, to a valid version, or to the highest version past a bound, and the directory entries' patch ids and
//! 4-byte data), `DetectFirewallOp`'s request, reply and datagram wait, and `RegAuth`'s password cipher and port
//! default. The ops stay C++ `AsyncOp`/`ServerRequestOp` objects (their sub-ops and sockets are the lobby lane's
//! moving machinery); `include/ee/wonapi/WONMiscOpsMsg.h` applies these in the original's order.
//!
//! Reached by nothing in the game (the patch and firewall servers are gone; RegAuth is the WON tools' login).

use std::ffi::c_void;

use crate::blowfish::Blowfish;
use crate::ffi::{give, EeWonByteSink};
use crate::wire::{PastEnd, Reader, Writer};

// ---- FindPatchOp

/// A patch's versions (`PatchInfo::mFromVersion`, `mToVersion`).
#[derive(Debug, Clone, Copy)]
pub struct Patch<'a> {
    pub from: &'a [u8],
    pub to: &'a [u8],
}

/// `ValidFromVersion`: the patch starts at `from`, or starts anywhere and leads past it (`cmp` is the op's
/// `mVersionCompare`, called as the original did).
pub fn valid_from(from: &[u8], p: &Patch, cmp: &mut dyn FnMut(&[u8], &[u8]) -> i32) -> bool {
    p.from == from || (p.from.is_empty() && cmp(p.to, from) > 0)
}

/// `GetPatchList`: the patches valid from `from`, by index.
pub fn patch_list(from: &[u8], patches: &[Patch], cmp: &mut dyn FnMut(&[u8], &[u8]) -> i32) -> Vec<usize> {
    (0..patches.len()).filter(|&i| valid_from(from, &patches[i], cmp)).collect()
}

/// `GetValidPatchList`: those also leading to a version of the valid set.
pub fn valid_patch_list(
    from: &[u8],
    patches: &[Patch],
    cmp: &mut dyn FnMut(&[u8], &[u8]) -> i32,
    valid: &mut dyn FnMut(&[u8]) -> bool,
) -> Vec<usize> {
    (0..patches.len()).filter(|&i| valid_from(from, &patches[i], cmp) && valid(patches[i].to)).collect()
}

/// `GetHighestPatchList(from, to)`: the patches valid from `from` to the highest version past `to` (the original
/// skips those whose target compares at or below it) that is not filtered; specific patches (with a from
/// version) displace general ones to the same version.
pub fn highest_patch_list(
    from: &[u8],
    to: &[u8],
    patches: &[Patch],
    cmp: &mut dyn FnMut(&[u8], &[u8]) -> i32,
    filtered: &mut dyn FnMut(&[u8]) -> bool,
) -> Vec<usize> {
    let mut out = Vec::new();
    let mut highest: Vec<u8> = Vec::new();
    let mut has_from = false;
    for (i, p) in patches.iter().enumerate() {
        if !valid_from(from, p, cmp) {
            continue;
        }
        if cmp(p.to, to) <= 0 {
            continue;
        }
        if filtered(p.to) {
            continue;
        }
        if highest.is_empty() {
            highest = p.to.to_vec();
            has_from = !p.from.is_empty();
        }
        let c = cmp(p.to, &highest);
        if c < 0 {
            continue;
        } else if c > 0 {
            highest = p.to.to_vec();
            has_from = !p.from.is_empty();
            out.clear();
            out.push(i);
        } else if !has_from {
            if !p.from.is_empty() {
                has_from = true;
                out.clear();
            }
            out.push(i);
        } else if !p.from.is_empty() {
            out.push(i);
        }
    }
    out
}

/// `DoFinish`'s patch id: the position of the `~` between the from and to versions, None for a to version only.
pub fn patch_id_split(id: &[u8]) -> Option<usize> {
    id.iter().position(|&c| c == b'~')
}

/// `DoFinish`'s `_cs` and `_ps` data: a little-endian 32-bit value when the data is 4 bytes long.
pub fn le32_field(data: &[u8]) -> Option<u32> {
    <[u8; 4]>::try_from(data).ok().map(u32::from_le_bytes)
}

// ---- DetectFirewallOp

/// `GetNextRequest`'s request (header 5, service 3, message 1 or 3, the port, and for TCP the connect wait).
pub fn firewall_request(lfs: u8, udp: bool, port: u16, wait_for_connect: bool, max_wait: u32) -> Vec<u8> {
    let mut w = Writer::new(lfs, true);
    w.append_byte(5);
    w.append_short(3);
    w.append_short(if udp { 3 } else { 1 });
    w.append_short(port as i16);
    if !udp {
        w.append_bool(wait_for_connect);
        w.append_long(max_wait as i32);
        w.append_bool(false);
    }
    w.to_message()
}

/// `CheckResponse`'s reading of the reply.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FirewallReply {
    /// Not header 5, service 3, message 2: `InvalidReplyHeader()`.
    BadHeader,
    /// The status and the public address's host when the reply carried a non-zero one.
    Status(i16, Option<i32>),
}

/// `CheckResponse`'s reads; Err where the original's `ReadBufferException` came.
pub fn firewall_reply(reply: &[u8]) -> Result<FirewallReply, PastEnd> {
    let mut r = Reader::new(reply);
    let header = r.read_byte()?;
    let service = r.read_short()? as u16;
    let message = r.read_short()? as u16;
    if header != 5 || service != 3 || message != 2 {
        return Ok(FirewallReply::BadHeader);
    }
    let status = r.read_short()?;
    let mut host = None;
    if r.available() >= 4 {
        let h = r.read_long()?;
        if h != 0 {
            host = Some(h);
        }
    }
    Ok(FirewallReply::Status(status, host))
}

/// `RecvServerDatagram`'s wait: the time left, shortened to the connect wait when one is set; an asynchronous
/// wait of 0 is infinite.
pub fn firewall_wait(time_left: u32, max_wait: u32, asynchronous: bool) -> u32 {
    let mut wait = time_left;
    if max_wait > 0 && max_wait < wait {
        wait = max_wait;
    }
    if asynchronous && wait == 0 {
        wait = u32::MAX;
    }
    wait
}

// ---- RegAuth

/// `gPasswordKey`'s 8 bytes.
const PASSWORD_KEY: [u8; 8] = [0x23, 0x77, 0xCF, 0x67, 0x99, 0xAB, 0xBA, 0xE5];

/// The stored password's cipher (`gPasswordKey.Encrypt` / `Decrypt`).
pub fn password(decrypt: bool, data: &[u8]) -> Option<Vec<u8>> {
    let mut key = Blowfish::new();
    key.set_key(&PASSWORD_KEY);
    if decrypt {
        key.decrypt(data)
    } else {
        key.encrypt(data)
    }
}

/// `ReadAuthSettings`'s port: `QueryDWORD`'s result 1 (the value missing) gives 15200, else the value's low 16
/// bits.
pub fn auth_port(query_result: i32, value: u32) -> u16 {
    if query_result == 1 {
        15200
    } else {
        value as u16
    }
}

// ---- FFI

/// One patch as the C++ hands it over.
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct EeWonPatchRef {
    pub from: *const u8,
    pub from_len: usize,
    pub to: *const u8,
    pub to_len: usize,
}

/// The op's version compare and set lookups, called back with byte strings.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonPatchHooks {
    pub context: *mut c_void,
    pub compare: unsafe extern "C" fn(*mut c_void, *const u8, usize, *const u8, usize) -> i32,
    /// in the valid set (`GetValidPatchList`) or the filtered set (`GetHighestPatchList`)
    pub in_set: unsafe extern "C" fn(*mut c_void, *const u8, usize) -> bool,
}

unsafe fn sl<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// Which list: 0 `GetPatchList`, 1 `GetValidPatchList`, 2 `GetHighestPatchList(from, to)`. The chosen patches'
/// indices go to `put` in the list's order.
///
/// # Safety
/// The pointers hold their lengths; the hooks and `put` are callable with their contexts.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_findpatch_list(
    which: i32,
    from: *const u8,
    from_len: usize,
    to: *const u8,
    to_len: usize,
    patches: *const EeWonPatchRef,
    count: usize,
    hooks: *const EeWonPatchHooks,
    put: unsafe extern "C" fn(*mut c_void, usize),
    put_context: *mut c_void,
) {
    let refs =
        if patches.is_null() || count == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(patches, count) } };
    let list: Vec<Patch> =
        refs.iter().map(|p| unsafe { Patch { from: sl(p.from, p.from_len), to: sl(p.to, p.to_len) } }).collect();
    let h = unsafe { *hooks };
    let mut cmp = |a: &[u8], b: &[u8]| unsafe { (h.compare)(h.context, a.as_ptr(), a.len(), b.as_ptr(), b.len()) };
    let mut in_set = |a: &[u8]| unsafe { (h.in_set)(h.context, a.as_ptr(), a.len()) };
    let from = unsafe { sl(from, from_len) };
    let out = match which {
        0 => patch_list(from, &list, &mut cmp),
        1 => valid_patch_list(from, &list, &mut cmp, &mut in_set),
        _ => highest_patch_list(from, unsafe { sl(to, to_len) }, &list, &mut cmp, &mut in_set),
    };
    for i in out {
        unsafe { put(put_context, i) };
    }
}

/// [`patch_id_split`]: the `~`'s position, -1 for none.
///
/// # Safety
/// `id` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_findpatch_id_split(id: *const u8, len: usize) -> isize {
    patch_id_split(unsafe { sl(id, len) }).map_or(-1, |p| p as isize)
}

/// [`le32_field`]: false unless the data is 4 bytes long.
///
/// # Safety
/// `data` holds `len` bytes; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_findpatch_le32(data: *const u8, len: usize, out: *mut u32) -> bool {
    match le32_field(unsafe { sl(data, len) }) {
        Some(v) => {
            unsafe { *out = v };
            true
        }
        None => false,
    }
}

/// [`password`] handed to `sink`; false where the cipher gave nothing (the original's NULL).
///
/// # Safety
/// `data` holds `len` bytes; `sink` is valid with `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_regauth_password(
    decrypt: bool,
    data: *const u8,
    len: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> bool {
    match password(decrypt, unsafe { sl(data, len) }) {
        Some(v) => {
            unsafe { give(sink, context, &v) };
            true
        }
        None => false,
    }
}

/// [`auth_port`].
#[no_mangle]
pub extern "C" fn ee_won_regauth_port(query_result: i32, value: u32) -> u16 {
    auth_port(query_result, value)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn p<'a>(from: &'a str, to: &'a str) -> Patch<'a> {
        Patch { from: from.as_bytes(), to: to.as_bytes() }
    }

    fn cmp(a: &[u8], b: &[u8]) -> i32 {
        a.cmp(b) as i32
    }

    #[test]
    fn patch_lists() {
        let ps = [p("1.0", "1.1"), p("", "1.2"), p("0.9", "1.0"), p("", "0.5"), p("1.0", "1.2"), p("", "1.2")];
        assert_eq!(patch_list(b"1.0", &ps, &mut cmp), vec![0, 1, 4, 5]);
        let mut valid = |v: &[u8]| v == b"1.2";
        assert_eq!(valid_patch_list(b"1.0", &ps, &mut cmp, &mut valid), vec![1, 4, 5]);
        // the highest past 1.0: the specific 1.0 -> 1.2 displaces the general ones before it and keeps out the one
        // after
        let mut none = |_: &[u8]| false;
        assert_eq!(highest_patch_list(b"1.0", b"1.0", &ps, &mut cmp, &mut none), vec![4]);
        // general ones only: all of them
        let gs = [p("", "1.2"), p("", "1.1"), p("", "1.2")];
        assert_eq!(highest_patch_list(b"1.0", b"1.0", &gs, &mut cmp, &mut none), vec![0, 2]);
        // a filtered target is skipped; a bound at the top leaves nothing
        let mut f = |v: &[u8]| v == b"1.2";
        assert_eq!(highest_patch_list(b"1.0", b"1.0", &ps, &mut cmp, &mut f), vec![0]);
        assert_eq!(highest_patch_list(b"1.0", b"9", &ps, &mut cmp, &mut none), Vec::<usize>::new());
    }

    #[test]
    fn compare_calls_follow_the_original() {
        let ps = [p("", "2"), p("x", "3")];
        let mut calls = Vec::new();
        let mut c = |a: &[u8], b: &[u8]| {
            calls.push((a.to_vec(), b.to_vec()));
            cmp(a, b)
        };
        let mut none = |_: &[u8]| false;
        highest_patch_list(b"1", b"0", &ps, &mut c, &mut none);
        // valid_from, the bound, the first highest (itself); the second fails valid_from without a compare
        assert_eq!(
            calls,
            vec![(b"2".to_vec(), b"1".to_vec()), (b"2".to_vec(), b"0".to_vec()), (b"2".to_vec(), b"2".to_vec())]
        );
    }

    #[test]
    fn ids_and_fields() {
        assert_eq!(patch_id_split(b"1.0~1.1"), Some(3));
        assert_eq!(patch_id_split(b"1.1"), None);
        assert_eq!(le32_field(&[1, 2, 3, 4]), Some(0x0403_0201));
        assert_eq!(le32_field(&[1, 2, 3]), None);
    }

    #[test]
    fn firewall() {
        let r = firewall_request(2, false, 0x1234, true, 7);
        assert_eq!(r, vec![15, 0, 5, 3, 0, 1, 0, 0x34, 0x12, 1, 7, 0, 0, 0, 0]);
        assert_eq!(firewall_request(0, true, 1, true, 7), vec![5, 3, 0, 3, 0, 1, 0]);
        assert_eq!(firewall_reply(&[5, 3, 0, 2, 0, 0xfe, 0xff]), Ok(FirewallReply::Status(-2, None)));
        assert_eq!(firewall_reply(&[5, 3, 0, 2, 0, 0, 0, 1, 2, 3, 4]), Ok(FirewallReply::Status(0, Some(0x0403_0201))));
        assert_eq!(firewall_reply(&[5, 3, 0, 2, 0, 0, 0, 0, 0, 0, 0]), Ok(FirewallReply::Status(0, None)));
        assert_eq!(firewall_reply(&[5, 3, 0, 2, 0, 0, 0, 1, 2]), Ok(FirewallReply::Status(0, None)));
        assert_eq!(firewall_reply(&[6, 3, 0, 2, 0]), Ok(FirewallReply::BadHeader));
        assert_eq!(firewall_reply(&[5, 3, 0, 2, 0, 1]), Err(PastEnd));
        assert_eq!(firewall_reply(&[5, 3]), Err(PastEnd));
        assert_eq!(firewall_wait(100, 0, false), 100);
        assert_eq!(firewall_wait(100, 30, false), 30);
        assert_eq!(firewall_wait(0, 30, true), u32::MAX);
        assert_eq!(firewall_wait(0, 0, false), 0);
    }

    fn unhex(s: &str) -> Vec<u8> {
        if s == "-" {
            return Vec::new();
        }
        (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
    }

    fn hex(b: &[u8]) -> String {
        if b.is_empty() {
            return "-".into();
        }
        b.iter().map(|x| format!("{x:02x}")).collect()
    }

    /// The UCRT's `atoi` on the short strings the driver makes (no overflow): blanks, a sign, digits, up to a NUL.
    fn atoi(s: &[u8]) -> i32 {
        let s = &s[..s.iter().position(|&c| c == 0).unwrap_or(s.len())];
        let mut i = 0;
        while i < s.len() && matches!(s[i], b' ' | 0x09..=0x0d) {
            i += 1;
        }
        let neg = i < s.len() && s[i] == b'-';
        if i < s.len() && (s[i] == b'-' || s[i] == b'+') {
            i += 1;
        }
        let mut v = 0i32;
        while i < s.len() && s[i].is_ascii_digit() {
            v = v * 10 + i32::from(s[i] - b'0');
            i += 1;
        }
        if neg {
            -v
        } else {
            v
        }
    }

    /// The original's choices recorded by tests/won/won_miscops_test.cpp (`--print 400`): the list, the compare,
    /// the versions, the patches and the set, then the chosen patches and the compare's calls.
    #[test]
    fn golden() {
        let mut n = 0;
        for line in include_str!("golden_miscops.txt").lines() {
            let f: Vec<&str> = line.split(' ').collect();
            assert_eq!(f[0], "find");
            let (which, kind): (i32, i32) = (f[1].parse().unwrap(), f[2].parse().unwrap());
            let (from, to) = (unhex(f[3]), unhex(f[4]));
            let owned: Vec<(Vec<u8>, Vec<u8>)> = if f[5] == "*" {
                Vec::new()
            } else {
                f[5].split(',')
                    .map(|p| {
                        let (a, b) = p.split_once('~').unwrap();
                        (unhex(a), unhex(b))
                    })
                    .collect()
            };
            let patches: Vec<Patch> = owned.iter().map(|(a, b)| Patch { from: a, to: b }).collect();
            let set: Vec<Vec<u8>> = if f[6] == "*" { Vec::new() } else { f[6].split(',').map(unhex).collect() };
            let mut log = String::new();
            let mut cmp = |a: &[u8], b: &[u8]| {
                log += &format!("{}/{};", hex(a), hex(b));
                match kind {
                    0 => a.cmp(b) as i32,
                    1 => atoi(a) - atoi(b),
                    2 => b.cmp(a) as i32,
                    _ => a.len() as i32 - b.len() as i32,
                }
            };
            let mut in_set = |v: &[u8]| set.iter().any(|s| s == v);
            let out = match which {
                0 => patch_list(&from, &patches, &mut cmp),
                1 => valid_patch_list(&from, &patches, &mut cmp, &mut in_set),
                _ => highest_patch_list(&from, &to, &patches, &mut cmp, &mut in_set),
            };
            let tags = if out.is_empty() {
                "-".to_string()
            } else {
                out.iter().map(|i| i.to_string()).collect::<Vec<_>>().join(",")
            };
            assert_eq!(tags, f[7], "{line}");
            assert_eq!(hex(log.as_bytes()), f[8], "{line}");
            n += 1;
        }
        assert_eq!(n, 400);
    }

    #[test]
    fn passwords() {
        let e = password(false, b"hunter2").unwrap();
        assert_eq!(e.len(), 16);
        assert_eq!(password(true, &e).unwrap(), b"hunter2");
        assert_eq!(password(true, &[1, 2, 3]), None);
        assert_eq!(auth_port(1, 77), 15200);
        assert_eq!(auth_port(0, 0x1_0050), 0x50);
    }
}
