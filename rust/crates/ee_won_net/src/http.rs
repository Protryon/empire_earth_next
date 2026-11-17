//! WONMisc's HTTP (`src/Empire Earth/WONAPI/WONMisc/HTTPGetOp.cpp`, `URL.cpp`, `HTTPCache.cpp`): the dates
//! (`MakeInternetDateTime`, `ParseInternetDateTime`), the status line, the header lines, the redirect target,
//! the chunk length, the request's text, the URL forms, and the cache's entry names and ids.
//!
//! `HTTPGetOp` stays a C++ object (an `AsyncOp` over a C++ `BlockingSocket`, its steps C++ socket ops); it hands
//! its lines and members over and applies the results. Reached only by the Internet lobby (the WON servers are
//! gone): no LAN game, tour or EE-SIM run reaches it.

use std::ffi::c_void;

use crate::ffi::EeWonByteSink;

// ---- C's character rules (the "C" locale)

/// `isspace`: space, \t, \n, \v, \f, \r.
pub fn c_space(c: u8) -> bool {
    c == b' ' || (9..=13).contains(&c)
}

/// The bytes of a C string: up to the first NUL.
pub fn c_str(s: &[u8]) -> &[u8] {
    match s.iter().position(|&c| c == 0) {
        Some(n) => &s[..n],
        None => s,
    }
}

/// Digits in `radix` from `i`: the magnitude (`None`: past 64 bits) and the end; no digit gives the start back.
fn digits(s: &[u8], mut i: usize, radix: u32) -> (Option<u64>, usize) {
    let mut v: Option<u64> = Some(0);
    while let Some(d) = s.get(i).and_then(|&c| (c as char).to_digit(radix)) {
        v = v.and_then(|v| v.checked_mul(u64::from(radix))).and_then(|v| v.checked_add(u64::from(d)));
        i += 1;
    }
    (v, i)
}

/// White space and a sign from `i`: whether negative and where the digits start.
fn sign(s: &[u8], mut i: usize) -> (bool, usize) {
    while i < s.len() && c_space(s[i]) {
        i += 1;
    }
    let mut negative = false;
    if i < s.len() && (s[i] == b'+' || s[i] == b'-') {
        negative = s[i] == b'-';
        i += 1;
    }
    (negative, i)
}

/// `sscanf`'s `%d` at `i` (the UCRT's: `strtoll`, saturating to 64 bits, then the low 32 bits): white space, a
/// sign, at least one digit; the value and the end.
pub fn scan_int(s: &[u8], i: usize) -> Option<(i32, usize)> {
    let (negative, start) = sign(s, i);
    let (v, end) = digits(s, start, 10);
    if end == start {
        return None;
    }
    let v: i64 = match (v, negative) {
        (Some(m), false) if m <= i64::MAX as u64 => m as i64,
        (_, false) => i64::MAX,
        (Some(m), true) if m <= 1u64 << 63 => (m as i64).wrapping_neg(),
        (_, true) => i64::MIN,
    };
    Some((v as i32, end))
}

/// `sscanf`'s `%x` at the start (the UCRT's: `strtoull`, saturating to 64 bits, then the low 32 bits): white
/// space, a sign, an optional 0x that must be followed by a hex digit, at least one hex digit.
pub fn scan_hex(s: &[u8]) -> Option<u32> {
    let (negative, mut i) = sign(s, 0);
    if i + 1 < s.len() && s[i] == b'0' && (s[i + 1] == b'x' || s[i + 1] == b'X') {
        i += 2;
    }
    let (v, end) = digits(s, i, 16);
    if end == i {
        return None;
    }
    let v = match v {
        None => u64::MAX,
        Some(m) if negative => m.wrapping_neg(),
        Some(m) => m,
    };
    Some(v as u32)
}

/// `atoi` (the UCRT's: `strtol`, saturating to 32 bits): white space, a sign, digits.
pub fn c_atoi(s: &[u8]) -> i32 {
    let (negative, start) = sign(s, 0);
    let (v, end) = digits(s, start, 10);
    if end == start {
        return 0;
    }
    match (v, negative) {
        (Some(m), false) if m <= i32::MAX as u64 => m as i32,
        (_, false) => i32::MAX,
        (Some(m), true) if m <= 1u64 << 31 => (m as i64).wrapping_neg() as i32,
        (_, true) => i32::MIN,
    }
}

// ---- the dates

const WEEKDAYS: [&str; 7] = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
const MONTHS: [&str; 12] = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];

/// The civil date (year, month 1-12, day) of a day count since 1970-01-01 (proleptic Gregorian).
fn civil(days: i64) -> (i64, u32, u32) {
    let z = days + 719468;
    let era = if z >= 0 { z } else { z - 146096 } / 146097;
    let doe = z - era * 146097;
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = (doy - (153 * mp + 2) / 5 + 1) as u32;
    let m = if mp < 10 { mp + 3 } else { mp - 9 } as u32;
    (yoe + era * 400 + i64::from(m <= 2), m, d)
}

/// `MakeInternetDateTime`: "Sun, 06 Nov 1994 08:49:37 GMT" (`gmtime`'s fields; the year's digits from its
/// 16-bit value, as the original).
pub fn format_date(t: i64) -> String {
    let days = t.div_euclid(86400);
    let secs = t.rem_euclid(86400);
    let (year, month, day) = civil(days);
    let wday = (days + 4).rem_euclid(7) as usize;
    let year = year as u16;
    let (h, m, s) = (secs / 3600, secs / 60 % 60, secs % 60);
    format!(
        "{}, {}{} {} {}{}{}{} {}{}:{}{}:{}{} GMT",
        WEEKDAYS[wday],
        day / 10 % 10,
        day % 10,
        MONTHS[(month - 1) as usize],
        year / 1000 % 10,
        year / 100 % 10,
        year / 10 % 10,
        year % 10,
        h / 10 % 10,
        h % 10,
        m / 10 % 10,
        m % 10,
        s / 10 % 10,
        s % 10
    )
}

/// `ParseInternetDateTime`: RFC 1123, RFC 850 or asctime; 0 when malformed. The original read past the string's
/// NUL in places (the month's three characters, the look-ahead for the time's colon): bytes past the end are 0
/// here.
pub fn parse_date(text: &[u8]) -> i64 {
    let text = c_str(text);
    // a signed char, 0 past the end
    let at = |i: usize| -> i32 { text.get(i).map_or(0, |&c| i32::from(c as i8)) };
    let sp = i32::from(b' ');
    let dash = i32::from(b'-');
    let colon = i32::from(b':');
    let digit = |c: i32| (48..=57).contains(&c);
    let mut p = 0;
    while at(p) == sp {
        p += 1;
    }
    while at(p) != sp {
        if at(p) == 0 {
            return 0;
        }
        p += 1;
    }
    while at(p) == sp {
        p += 1;
    }
    let mut got_day = false;
    let mut day: i32 = 0;
    // a day of the month: digits up to a space or dash
    let read_day = |p: &mut usize, day: &mut i32| -> bool {
        while at(*p) != sp && at(*p) != dash {
            if at(*p) == 0 {
                return false;
            }
            *day = day.wrapping_mul(10).wrapping_add(at(*p) - 48);
            *p += 1;
        }
        while at(*p) == sp || at(*p) == dash {
            *p += 1;
        }
        true
    };
    if digit(at(p)) {
        if !read_day(&mut p, &mut day) {
            return 0;
        }
        got_day = true;
    }
    let name = [at(p), at(p + 1), at(p + 2)];
    p += 3;
    let mut month = None;
    for (i, m) in MONTHS.iter().enumerate() {
        let m = m.as_bytes();
        let lower = [i32::from(m[0]), i32::from(m[1]), i32::from(m[2])];
        let upper = [i32::from(m[0]), i32::from(m[1].to_ascii_uppercase()), i32::from(m[2].to_ascii_uppercase())];
        if name == lower || name == upper {
            month = Some(i as i32);
        }
    }
    let Some(month) = month else { return 0 };
    while at(p) == sp || at(p) == dash {
        p += 1;
    }
    if !got_day && !read_day(&mut p, &mut day) {
        return 0;
    }
    let mut year: i32 = 0;
    // a year: digits up to a space; the end of the string ends it if a digit came
    let read_year = |p: &mut usize, year: &mut i32| -> bool {
        while at(*p) != sp {
            if at(*p) == 0 {
                if *year != 0 {
                    break;
                }
                return false;
            }
            *year = year.wrapping_mul(10).wrapping_add(at(*p) - 48);
            *p += 1;
        }
        true
    };
    let mut got_year = false;
    if at(p + 2) != colon {
        if !read_year(&mut p, &mut year) {
            return 0;
        }
        got_year = true;
    }
    while at(p) == sp {
        p += 1;
    }
    let two = |p: &mut usize| -> Option<i32> {
        if !digit(at(*p)) {
            return None;
        }
        let mut v = (at(*p) - 48) * 10;
        *p += 1;
        if !digit(at(*p)) {
            return None;
        }
        v += at(*p) - 48;
        *p += 1;
        Some(v)
    };
    let Some(hour) = two(&mut p) else { return 0 };
    if at(p) != colon {
        return 0;
    }
    p += 1;
    let Some(mins) = two(&mut p) else { return 0 };
    if at(p) != colon {
        return 0;
    }
    p += 1;
    let Some(sec) = two(&mut p) else { return 0 };
    while at(p) == sp {
        p += 1;
    }
    if !got_year && !read_year(&mut p, &mut year) {
        return 0;
    }
    if year < 100 {
        year = year.wrapping_add(1900);
    }
    // timegm over the struct tm fields (int arithmetic as the original, then 64 bits)
    let tm_year = year.wrapping_sub(1900);
    let y = i64::from(tm_year.wrapping_add(1900).wrapping_sub(i32::from(month < 2)));
    let era = if y >= 0 { y } else { y - 399 } / 400;
    let yoe = y - era * 400;
    let m = i64::from(month) + 1;
    let doy = (153 * (if m > 2 { m - 3 } else { m + 9 }) + 2) / 5 + i64::from(day) - 1;
    let doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    let days = era * 146097 + doe - 719468;
    days.wrapping_mul(86400) + i64::from(hour * 3600 + mins * 60 + sec)
}

// ---- the reply lines

/// `ExtractStatus`'s `sscanf("%s%d")`: the status when both converted.
pub fn status_line(line: &[u8]) -> Option<i32> {
    let s = c_str(line);
    let mut i = 0;
    while i < s.len() && c_space(s[i]) {
        i += 1;
    }
    let start = i;
    while i < s.len() && !c_space(s[i]) {
        i += 1;
    }
    if i == start {
        return None;
    }
    scan_int(s, i).map(|(v, _)| v)
}

/// The header keys `ExtractHeaderLine` knows, in its order (a line's name matches the first key it is a
/// case-blind prefix of).
pub const HEADER_KEYS: [&str; 9] = [
    "Content-Length",
    "Content-Type",
    "Last-Modified",
    "Location",
    "Transfer-Encoding",
    "Connection",
    "Date",
    "Expires",
    "ETag",
];

/// One header line's reading.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonHttpHeader {
    /// false: no colon (the line is ignored)
    pub colon: bool,
    /// the key's index in `HEADER_KEYS`, 9 for none
    pub key: u8,
    /// the value's offset in the line (after the colon and its spaces)
    pub value: u32,
    /// Content-Length's `atoi`
    pub number: i32,
    /// a date's `ParseInternetDateTime`
    pub time: i64,
    /// Transfer-Encoding starts with CHUNKED, Connection with CLOSE (case-blind)
    pub flag: bool,
    /// ETag: the value's length kept (through the second quote), -1 to erase it
    pub tag_len: i32,
}

fn upper(c: u8) -> u8 {
    c.to_ascii_uppercase()
}

fn find(s: &[u8], what: &[u8]) -> Option<usize> {
    if what.is_empty() {
        return Some(0);
    }
    s.windows(what.len()).position(|w| w == what)
}

/// `ExtractHeaderLine`'s reading of a line (its C string).
pub fn header_line(line: &[u8]) -> EeWonHttpHeader {
    let s = c_str(line);
    let mut h = EeWonHttpHeader { key: 9, tag_len: -1, ..Default::default() };
    let Some(colon) = s.iter().position(|&c| c == b':') else { return h };
    h.colon = true;
    let name = &s[..colon];
    h.key = HEADER_KEYS
        .iter()
        .position(|k| {
            let k = k.as_bytes();
            name.len() <= k.len() && name.iter().zip(k).all(|(&a, &b)| upper(a) == upper(b))
        })
        .unwrap_or(9) as u8;
    let mut v = colon + 1;
    while v < s.len() && s[v] == b' ' {
        v += 1;
    }
    h.value = v as u32;
    let value = &s[v..];
    match h.key {
        0 => h.number = c_atoi(value),
        2 | 6 | 7 => h.time = parse_date(value),
        4 | 5 => {
            let want: &[u8] = if h.key == 4 { b"CHUNKED" } else { b"CLOSE" };
            h.flag =
                value.len() >= want.len() && value[..want.len()].iter().map(|&c| upper(c)).eq(want.iter().copied());
        }
        8 if value.len() > 1 => {
            if let Some(n) = value[1..].iter().position(|&c| c == b'"') {
                h.tag_len = (n + 2) as i32;
            }
        }
        _ => {}
    }
    h
}

/// `ExtractRedirect`: after the "://", the host up to the next '/' and the path from it (`None`: the path is
/// "/"); `None` without a "://".
pub fn redirect(location: &[u8]) -> Option<((usize, usize), Option<usize>)> {
    let s = c_str(location);
    let host = find(s, b"://")? + 3;
    Some(match s[host..].iter().position(|&c| c == b'/') {
        None => ((host, s.len()), None),
        Some(n) => ((host, host + n), Some(host + n)),
    })
}

/// `ExtractChunkLength`'s `sscanf("%x")`.
pub fn chunk_length(line: &[u8]) -> Option<u32> {
    scan_hex(c_str(line))
}

// ---- the request

/// What `SendRequest` writes.
#[derive(Clone, Debug, Default)]
pub struct Request<'a> {
    pub proxy: bool,
    pub host: &'a [u8],
    pub path: &'a [u8],
    pub close: bool,
    /// the local file's size and modify time (a local path that exists)
    pub file: Option<(u64, i64)>,
    pub range_resume: bool,
    pub entity_tag: &'a [u8],
    /// no cache entry, or a valid one
    pub cache_ok: bool,
}

/// `SendRequest`'s text.
pub fn request(q: &Request) -> Vec<u8> {
    let mut r = Vec::new();
    r.extend_from_slice(b"GET ");
    if q.proxy {
        r.extend_from_slice(b"http://");
        r.extend_from_slice(q.host);
    }
    r.extend_from_slice(q.path);
    r.extend_from_slice(b" HTTP/1.1\r\nHost: ");
    r.extend_from_slice(q.host);
    r.extend_from_slice(b"\r\n");
    r.extend_from_slice(if q.close { b"Connection: close\r\n" as &[u8] } else { b"Connection: Keep-Alive\r\n" });
    if let Some((size, time)) = q.file {
        if size > 0 {
            if q.range_resume {
                r.extend_from_slice(b"If-Range: ");
                if !q.entity_tag.is_empty() {
                    r.extend_from_slice(q.entity_tag);
                } else {
                    r.extend_from_slice(format_date(time).as_bytes());
                }
                r.extend_from_slice(b"\r\nRange: bytes=");
                // snprintf(buffer, 10, "%d", (int)size): nine characters at most
                let n = (size as i32).to_string();
                r.extend_from_slice(&n.as_bytes()[..n.len().min(9)]);
                r.extend_from_slice(b"-\r\n");
            } else if q.cache_ok {
                if !q.entity_tag.is_empty() {
                    r.extend_from_slice(b"If-None-Match: ");
                    r.extend_from_slice(q.entity_tag);
                    r.extend_from_slice(b"\r\n");
                }
                r.extend_from_slice(b"If-Modified-Since: ");
                r.extend_from_slice(format_date(time).as_bytes());
                r.extend_from_slice(b"\r\n");
            }
        }
    }
    r.extend_from_slice(b"\r\n");
    r
}

/// `HTTPGetOp(url)`'s split: the host's and the path's ranges after the scheme (`None` path: "/").
pub fn split_url(url: &[u8]) -> ((usize, usize), Option<(usize, usize)>) {
    let start = find(url, b"://").map_or(0, |n| n + 3);
    let rest = &url[start..];
    match rest.iter().position(|&c| c == b'/') {
        None => ((start, url.len()), None),
        Some(n) => ((start, start + n), Some((start + n, url.len()))),
    }
}

/// `RecvContent`'s test that the local file is already the content (not appended, same size, modify times at
/// most a second apart, the connection closing).
pub fn up_to_date(
    append: bool,
    local_size: u64,
    content_length: u32,
    local_time: i64,
    last_modified: i64,
    close: bool,
) -> bool {
    // abs() of the 64-bit time_t difference
    let delta = local_time.wrapping_sub(last_modified).wrapping_abs();
    !append && local_size > 0 && local_size == u64::from(content_length) && delta <= 1 && close
}

// ---- URL

/// `URL`'s types (`URLType`).
pub const EE_WON_URL_NONE: i32 = 0;
pub const EE_WON_URL_FILE: i32 = 1;
pub const EE_WON_URL_HTTP: i32 = 2;
pub const EE_WON_URL_FTP: i32 = 3;

/// A parsed URL: `URL::URL`.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Url {
    pub kind: i32,
    pub absolute: bool,
    pub address: Vec<u8>,
    pub path: Vec<u8>,
}

fn ieq(a: &[u8], b: &[u8]) -> bool {
    a.eq_ignore_ascii_case(b)
}

impl Url {
    /// `URL::URL`: an unknown scheme leaves the type `None` and nothing else set.
    pub fn parse(s: &[u8], kinds: [i32; 4]) -> Url {
        let mut u = Url { kind: kinds[0], ..Default::default() };
        // a protocol name with a NUL compares only up to it (PlatStrICmp on its c_str)
        if let Some(end) = find(s, b"://") {
            let protocol = c_str(&s[..end]);
            u.kind = if ieq(protocol, b"HTTP") {
                kinds[1]
            } else if ieq(protocol, b"FTP") {
                kinds[2]
            } else if ieq(protocol, b"FILE") {
                kinds[3]
            } else {
                return Url { kind: kinds[0], ..Default::default() };
            };
            u.absolute = true;
            let a = end + 3;
            match s[a..].iter().position(|&c| c == b'/') {
                None => {
                    u.address = s[a..].to_vec();
                    u.path = b"/".to_vec();
                }
                Some(n) => {
                    u.address = s[a..a + n].to_vec();
                    u.path = s[a + n..].to_vec();
                }
            }
        } else {
            u.kind = kinds[3];
            u.path = s.to_vec();
        }
        if u.kind == kinds[3] {
            for c in &mut u.path {
                if *c == b'\\' {
                    *c = b'/';
                }
            }
        }
        u
    }

    /// `CalcRelativeTo`. (The original read the path's first character even when it was empty: its NUL.)
    pub fn relative_to(&mut self, base: &Url) {
        if self.absolute {
            return;
        }
        self.absolute = base.absolute;
        self.kind = base.kind;
        self.address = base.address.clone();
        if self.path.first() == Some(&b'#') {
            let mut p = base.path.clone();
            p.extend_from_slice(&self.path);
            self.path = p;
            return;
        }
        if !self.path.is_empty() && !base.path.is_empty() && self.path[0] != b'/' {
            let relative = std::mem::take(&mut self.path);
            let mut p = base.path.clone();
            match p.iter().rposition(|&c| c == b'/') {
                None => p.push(b'/'),
                Some(n) => p.truncate(n + 1),
            }
            p.extend_from_slice(&relative);
            self.path = p;
        }
    }

    /// `GetEntire`.
    pub fn entire(&self, kinds: [i32; 4]) -> Vec<u8> {
        let mut s = Vec::new();
        if self.kind == kinds[1] {
            s.extend_from_slice(b"http://");
        } else if self.kind == kinds[2] {
            s.extend_from_slice(b"ftp://");
        }
        s.extend_from_slice(&self.address);
        s.extend_from_slice(&self.path);
        s
    }
}

// ---- HTTPCache

/// `GetFreeId`: the lowest id from `next` on that the sorted `ids` lack.
pub fn free_id(ids: &[i32], next: i32) -> i32 {
    let mut n = next;
    let mut i = ids.partition_point(|&x| x < n);
    if i == ids.len() || ids[i] != n {
        return n;
    }
    while i < ids.len() && ids[i] == n {
        i += 1;
        n = n.wrapping_add(1);
    }
    n
}

/// `GetEntry`'s file name for an entry: "%06d.<type>", the type the location's extension when its last dot is
/// after its last slash (or, without one, its last backslash) and the extension is at most 9 characters, else
/// "dat".
pub fn cache_file_name(id: i32, location: &[u8]) -> Vec<u8> {
    let slash = location.iter().rposition(|&c| c == b'/').or_else(|| location.iter().rposition(|&c| c == b'\\'));
    let mut kind: &[u8] = b"dat";
    if let Some(slash) = slash {
        // int positions as the original's (npos is -1: never after a slash)
        if let Some(dot) = location.iter().rposition(|&c| c == b'.') {
            if dot > slash && dot + 10 >= location.len() {
                kind = &location[dot + 1..];
            }
        }
    }
    // sprintf's %s stops at a NUL
    let kind = c_str(kind);
    let mut s = format!("{id:06}.").into_bytes();
    s.extend_from_slice(kind);
    s
}

/// `WriteCacheMap`'s keep rule: valid and used in the last 14 days.
pub fn cache_keeps(valid: bool, now: i64, last_access: i64) -> bool {
    valid && now.wrapping_sub(last_access) < 1209600
}

// ---- the C ABI

unsafe fn slice<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if n == 0 || p.is_null() {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

unsafe fn cstr<'a>(p: *const u8) -> &'a [u8] {
    if p.is_null() {
        return &[];
    }
    unsafe { std::ffi::CStr::from_ptr(p.cast()) }.to_bytes()
}

/// `MakeInternetDateTime` into `sink`.
///
/// # Safety
/// `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_format_date(t: i64, sink: EeWonByteSink, context: *mut c_void) {
    let s = format_date(t);
    unsafe { sink(context, s.as_ptr(), s.len()) }
}

/// `ParseInternetDateTime` of a C string.
///
/// # Safety
/// `text` is a NUL-terminated string.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_parse_date(text: *const u8) -> i64 {
    parse_date(unsafe { cstr(text) })
}

/// `ExtractStatus`'s scan of a C string: false when it fails, else the status in `*out`.
///
/// # Safety
/// `line` is a NUL-terminated string; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_status_line(line: *const u8, out: *mut i32) -> bool {
    match status_line(unsafe { cstr(line) }) {
        Some(v) => {
            unsafe { *out = v };
            true
        }
        None => false,
    }
}

/// `ExtractHeaderLine`'s reading of a C string.
///
/// # Safety
/// `line` is a NUL-terminated string; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_header_line(line: *const u8, out: *mut EeWonHttpHeader) {
    unsafe { *out = header_line(cstr(line)) }
}

/// `ExtractRedirect` of a C string: false without a "://", else the host's range and the path's start
/// (`*path_at` -1: the path is "/").
///
/// # Safety
/// `location` is a NUL-terminated string; the outputs are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_redirect(
    location: *const u8,
    host_at: *mut usize,
    host_end: *mut usize,
    path_at: *mut isize,
) -> bool {
    match redirect(unsafe { cstr(location) }) {
        None => false,
        Some(((a, b), p)) => {
            unsafe {
                *host_at = a;
                *host_end = b;
                *path_at = p.map_or(-1, |x| x as isize);
            }
            true
        }
    }
}

/// `ExtractChunkLength`'s scan of a C string: false when it fails, else the length in `*out`.
///
/// # Safety
/// `line` is a NUL-terminated string; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_chunk_length(line: *const u8, out: *mut u32) -> bool {
    match chunk_length(unsafe { cstr(line) }) {
        Some(v) => {
            unsafe { *out = v };
            true
        }
        None => false,
    }
}

/// `SendRequest`'s inputs.
#[repr(C)]
pub struct EeWonHttpRequest {
    pub proxy: bool,
    pub host: *const u8,
    pub host_len: usize,
    pub path: *const u8,
    pub path_len: usize,
    pub close: bool,
    /// a local path that exists: `size` and `time` are its
    pub file: bool,
    pub size: u64,
    pub time: i64,
    pub range_resume: bool,
    pub tag: *const u8,
    pub tag_len: usize,
    pub cache_ok: bool,
}

/// `SendRequest`'s text into `sink`.
///
/// # Safety
/// `q` and its byte ranges are valid; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_request(q: *const EeWonHttpRequest, sink: EeWonByteSink, context: *mut c_void) {
    let q = unsafe { &*q };
    let r = unsafe {
        request(&Request {
            proxy: q.proxy,
            host: slice(q.host, q.host_len),
            path: slice(q.path, q.path_len),
            close: q.close,
            file: if q.file { Some((q.size, q.time)) } else { None },
            range_resume: q.range_resume,
            entity_tag: slice(q.tag, q.tag_len),
            cache_ok: q.cache_ok,
        })
    };
    unsafe { sink(context, r.as_ptr(), r.len()) }
}

/// `HTTPGetOp(url)`'s split: the host's range and the path's (`*path_at` -1: none, the path is "/").
///
/// # Safety
/// `url` holds `len` bytes; the outputs are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_split_url(
    url: *const u8,
    len: usize,
    host_at: *mut usize,
    host_end: *mut usize,
    path_at: *mut isize,
) {
    let ((a, b), p) = split_url(unsafe { slice(url, len) });
    unsafe {
        *host_at = a;
        *host_end = b;
        *path_at = p.map_or(-1, |(x, _)| x as isize);
    }
}

/// `RecvContent`'s up-to-date test.
#[no_mangle]
pub extern "C" fn ee_won_http_up_to_date(
    append: bool,
    local_size: u64,
    content_length: u32,
    local_time: i64,
    last_modified: i64,
    close: bool,
) -> bool {
    up_to_date(append, local_size, content_length, local_time, last_modified, close)
}

/// A URL handed over: its type, absolute flag, address and path.
#[repr(C)]
pub struct EeWonUrl {
    pub kind: i32,
    pub absolute: bool,
    pub address: *const u8,
    pub address_len: usize,
    pub path: *const u8,
    pub path_len: usize,
}

/// A URL's results: the type, the absolute flag, the address and the path into `sink` (in that order).
pub type EeWonUrlSink = unsafe extern "C" fn(
    context: *mut c_void,
    kind: i32,
    absolute: bool,
    address: *const u8,
    address_len: usize,
    path: *const u8,
    path_len: usize,
);

const KINDS: [i32; 4] = [EE_WON_URL_NONE, EE_WON_URL_HTTP, EE_WON_URL_FTP, EE_WON_URL_FILE];

unsafe fn url_from(u: &EeWonUrl) -> Url {
    unsafe {
        Url {
            kind: u.kind,
            absolute: u.absolute,
            address: slice(u.address, u.address_len).to_vec(),
            path: slice(u.path, u.path_len).to_vec(),
        }
    }
}

unsafe fn url_out(u: &Url, sink: EeWonUrlSink, context: *mut c_void) {
    unsafe { sink(context, u.kind, u.absolute, u.address.as_ptr(), u.address.len(), u.path.as_ptr(), u.path.len()) }
}

/// `URL::URL` into `sink`; an unknown scheme gives the type `EE_WON_URL_NONE` (and the C++ assigns nothing else).
///
/// # Safety
/// `text` holds `len` bytes; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_url_parse(text: *const u8, len: usize, sink: EeWonUrlSink, context: *mut c_void) {
    let u = Url::parse(unsafe { slice(text, len) }, KINDS);
    unsafe { url_out(&u, sink, context) }
}

/// `URL::CalcRelativeTo` into `sink`.
///
/// # Safety
/// `url` and `base` are valid; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_url_relative(
    url: *const EeWonUrl,
    base: *const EeWonUrl,
    sink: EeWonUrlSink,
    context: *mut c_void,
) {
    let mut u = unsafe { url_from(&*url) };
    u.relative_to(&unsafe { url_from(&*base) });
    unsafe { url_out(&u, sink, context) }
}

/// `URL::GetEntire` into `sink`.
///
/// # Safety
/// `url` is valid; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_url_entire(url: *const EeWonUrl, sink: EeWonByteSink, context: *mut c_void) {
    let s = unsafe { url_from(&*url) }.entire(KINDS);
    unsafe { sink(context, s.as_ptr(), s.len()) }
}

/// `HTTPCache::GetFreeId` over the sorted ids.
///
/// # Safety
/// `ids` holds `count` ids.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_cache_free_id(ids: *const i32, count: usize, next: i32) -> i32 {
    let ids = if count == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(ids, count) } };
    free_id(ids, next)
}

/// `HTTPCache::GetEntry`'s file name into `sink`.
///
/// # Safety
/// `location` holds `len` bytes; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_cache_file_name(
    id: i32,
    location: *const u8,
    len: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let s = cache_file_name(id, unsafe { slice(location, len) });
    unsafe { sink(context, s.as_ptr(), s.len()) }
}

/// `HTTPCache::WriteCacheMap`'s keep rule.
#[no_mangle]
pub extern "C" fn ee_won_http_cache_keeps(valid: bool, now: i64, last_access: i64) -> bool {
    cache_keeps(valid, now, last_access)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn dates() {
        assert_eq!(format_date(784111777), "Sun, 06 Nov 1994 08:49:37 GMT");
        assert_eq!(format_date(0), "Thu, 01 Jan 1970 00:00:00 GMT");
        assert_eq!(parse_date(b"Sun, 06 Nov 1994 08:49:37 GMT"), 784111777);
        assert_eq!(parse_date(b"Sunday, 06-Nov-94 08:49:37 GMT"), 784111777);
        assert_eq!(parse_date(b"Sun Nov  6 08:49:37 1994"), 784111777);
        assert_eq!(parse_date(b"Sun, 06 Xyz 1994 08:49:37 GMT"), 0);
        assert_eq!(parse_date(b"Sun"), 0);
        assert_eq!(parse_date(b"  Mon, 01 JAN 2001 00:00:00"), 978307200);
        assert_eq!(parse_date(b"Sun Nov  6 08:49:3"), 0);
    }

    #[test]
    fn scans() {
        assert_eq!(status_line(b"HTTP/1.1 200 OK"), Some(200));
        assert_eq!(status_line(b"HTTP/1.1"), None);
        assert_eq!(status_line(b"  X -7"), Some(-7));
        assert_eq!(status_line(b""), None);
        assert_eq!(chunk_length(b"1a\r"), Some(26));
        assert_eq!(chunk_length(b" 0x10"), Some(16));
        assert_eq!(chunk_length(b"zz"), None);
        assert_eq!(chunk_length(b"-1"), Some(u32::MAX));
        assert_eq!(chunk_length(b"0x"), None);
        assert_eq!(chunk_length(b"0xg"), None);
        assert_eq!(chunk_length(b"0"), Some(0));
        assert_eq!(chunk_length(b"0z"), Some(0));
        assert_eq!(chunk_length(b"8000000000000000"), Some(0));
        assert_eq!(chunk_length(b"10000000000000000"), Some(u32::MAX));
        assert_eq!(chunk_length(b"-ffffffffffffffff"), Some(1));
        assert_eq!(chunk_length(b"-10000000000000000"), Some(u32::MAX));
        assert_eq!(c_atoi(b" \x0b12x"), 12);
        assert_eq!(c_atoi(b"2147483648"), i32::MAX);
        assert_eq!(c_atoi(b"-2147483649"), i32::MIN);
        assert_eq!(scan_int(b"4294967295", 0), Some((-1, 10)));
        assert_eq!(scan_int(b"9223372036854775808", 0), Some((-1, 19)));
        assert_eq!(scan_int(b"-2147483649", 0), Some((i32::MAX, 11)));
        assert_eq!(scan_int(b"-99999999999999999999", 0), Some((0, 21)));
        assert_eq!(scan_int(b"+", 0), None);
    }

    #[test]
    fn headers() {
        let h = header_line(b"content-length:  42");
        assert_eq!((h.colon, h.key, h.value, h.number), (true, 0, 17, 42));
        assert!(!header_line(b"no colon").colon);
        assert_eq!(header_line(b"Content:x").key, 0);
        assert_eq!(header_line(b":x").key, 0);
        assert_eq!(header_line(b"X-Thing: y").key, 9);
        assert!(header_line(b"Transfer-Encoding: chunked").flag);
        assert!(!header_line(b"Connection: keep").flag);
        assert_eq!(header_line(b"ETag: \"abc\"x").tag_len, 5);
        assert_eq!(header_line(b"ETag: \"abc").tag_len, -1);
        assert_eq!(header_line(b"Location: http://a.b/c/d").key, 3);
        assert_eq!(redirect(b"http://a.b/c/d"), Some(((7, 10), Some(10))));
        assert_eq!(redirect(b"ftp://host"), Some(((6, 10), None)));
        assert_eq!(redirect(b"/x"), None);
    }

    #[test]
    fn requests() {
        let q = Request { host: b"h", path: b"/p", close: true, ..Default::default() };
        assert_eq!(request(&q), b"GET /p HTTP/1.1\r\nHost: h\r\nConnection: close\r\n\r\n");
        let q = Request {
            proxy: true,
            host: b"h",
            path: b"/p",
            file: Some((2147483647, 0)),
            range_resume: true,
            ..Default::default()
        };
        let r = String::from_utf8(request(&q)).unwrap();
        assert!(r.starts_with("GET http://h/p HTTP/1.1\r\nHost: h\r\nConnection: Keep-Alive\r\n"));
        assert!(r.contains("If-Range: Thu, 01 Jan 1970 00:00:00 GMT\r\nRange: bytes=214748364-\r\n"));
        let q = Request {
            host: b"h",
            path: b"/",
            file: Some((5, 0)),
            entity_tag: b"\"t\"",
            cache_ok: true,
            ..Default::default()
        };
        assert!(String::from_utf8(request(&q)).unwrap().contains("If-None-Match: \"t\"\r\nIf-Modified-Since: "));
        assert_eq!(split_url(b"http://a/b"), ((7, 8), Some((8, 10))));
        assert_eq!(split_url(b"a"), ((0, 1), None));
        assert!(up_to_date(false, 3, 3, 10, 11, true));
        assert!(!up_to_date(false, 3, 3, 10, 12, true));
    }

    #[test]
    fn urls() {
        let u = Url::parse(b"HTTP://host/a/b", KINDS);
        assert_eq!(
            (u.kind, u.absolute, &u.address[..], &u.path[..]),
            (EE_WON_URL_HTTP, true, &b"host"[..], &b"/a/b"[..])
        );
        let u = Url::parse(b"gopher://x", KINDS);
        assert_eq!(u, Url::default());
        let mut r = Url::parse(b"c\\d", KINDS);
        assert_eq!(r.path, b"c/d");
        r.relative_to(&Url::parse(b"http://h/a/b", KINDS));
        assert_eq!(r.entire(KINDS), b"http://h/a/c/d");
        let mut f = Url::parse(b"#frag", KINDS);
        f.relative_to(&Url::parse(b"ftp://h", KINDS));
        assert_eq!(f.entire(KINDS), b"ftp://h/#frag");
    }

    #[test]
    fn the_cache() {
        assert_eq!(free_id(&[1, 2, 3, 5], 1), 4);
        assert_eq!(free_id(&[1, 2, 3, 5], 5), 6);
        assert_eq!(free_id(&[2], 1), 1);
        assert_eq!(cache_file_name(7, b"http://h/a.png"), b"000007.png");
        assert_eq!(cache_file_name(7, b"h/a.verylongextension"), b"000007.dat");
        assert_eq!(cache_file_name(7, b"a.png"), b"000007.dat");
        assert_eq!(cache_file_name(-3, b"x\\y.gif"), b"-00003.gif");
        assert!(cache_keeps(true, 1209599, 0));
        assert!(!cache_keeps(true, 1209600, 0));
    }

    fn unhex(h: &str) -> Vec<u8> {
        if h == "-" {
            return Vec::new();
        }
        (0..h.len()).step_by(2).map(|i| u8::from_str_radix(&h[i..i + 2], 16).unwrap()).collect()
    }

    fn hex(b: &[u8]) -> String {
        if b.is_empty() {
            return "-".into();
        }
        b.iter().map(|c| format!("{c:02x}")).collect()
    }

    /// The driver's stand-in op after `ExtractHeaderLine` (WONHttpMsg.h's assignments), as the driver prints it.
    fn header_dump(line: &[u8]) -> String {
        let status = -5;
        let (mut chunked, mut first, mut close) = (false, false, false);
        let (mut length, mut last, mut server, mut expire, mut finished) = (7u32, 1i64, 2i64, 3i64, 0);
        let (mut kind, mut rhost, mut rpath, mut tag) = (b"t".to_vec(), b"rh".to_vec(), b"rp".to_vec(), b"e".to_vec());
        let h = header_line(line);
        let mut ret = 1;
        if h.colon {
            let value = c_str(&line[h.value as usize..]);
            match h.key {
                0 => length = h.number as u32,
                1 => kind = value.to_vec(),
                2 => last = h.time,
                3 => match redirect(value) {
                    None => {
                        finished = 100094;
                        ret = 0;
                    }
                    Some(((a, b), p)) => match p {
                        None => {
                            rhost = value[a..].to_vec();
                            rpath = b"/".to_vec();
                        }
                        Some(p) => {
                            rhost = value[a..b].to_vec();
                            rpath = value[p..].to_vec();
                        }
                    },
                },
                4 if h.flag => {
                    chunked = true;
                    first = true;
                }
                5 if h.flag => close = true,
                6 => server = h.time,
                7 => expire = h.time,
                8 => {
                    tag = value.to_vec();
                    if h.tag_len < 0 {
                        tag.clear();
                    } else {
                        tag.truncate(h.tag_len as usize);
                    }
                }
                _ => {}
            }
        }
        format!(
            "{ret} {status} {} {} {} {length} {last} {server} {expire} {finished} {} {} {} {}",
            u8::from(chunked),
            u8::from(first),
            u8::from(close),
            hex(&kind),
            hex(&rhost),
            hex(&rpath),
            hex(&tag)
        )
    }

    #[test]
    fn the_golden() {
        let mut n = 0;
        for line in include_str!("golden_http.txt").lines() {
            let (kind, rest) = line.split_once(' ').unwrap();
            match kind {
                "format" => {
                    let (t, d) = rest.split_once(' ').unwrap();
                    assert_eq!(format_date(t.parse().unwrap()), d, "{line}");
                }
                "parse" => {
                    let (h, t) = rest.split_once(' ').unwrap();
                    assert_eq!(parse_date(&unhex(h)).to_string(), t, "{line}");
                }
                "status" => {
                    let (h, r) = rest.split_once(' ').unwrap();
                    let got = match status_line(&unhex(h)) {
                        Some(v) => format!("1 {v}"),
                        None => "0 -5".into(),
                    };
                    assert_eq!(got, r, "{line}");
                }
                "header" => {
                    let (h, r) = rest.split_once(' ').unwrap();
                    assert_eq!(header_dump(&unhex(h)), r, "{line}");
                }
                _ => {
                    let (h, r) = rest.split_once(' ').unwrap();
                    let got = match chunk_length(&unhex(h)) {
                        Some(v) => format!("1 {v}"),
                        None => "0 23130".into(),
                    };
                    assert_eq!(got, r, "{line}");
                }
            }
            n += 1;
        }
        assert_eq!(n, 5000);
    }
}
