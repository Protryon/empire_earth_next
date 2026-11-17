//! WONMisc's version and patch records and ops (`PatchTypes.cpp`, `GetVersionOp.cpp`, `CheckValidVersionOp.cpp`,
//! `GetPatchListOp.cpp`, `ReportPatchStatusOp.cpp`): `VersionData`'s and `PatchData`'s wire forms (their reads as
//! the original's ordered assignments, so that a record cut short keeps what was read), the version type list's
//! split, the version filter (`IsFiltered`, `BuildVersionFilterSet`), and the patch ops' requests and replies.
//!
//! The ops stay C++ objects (`DBProxyOp`s and `AsyncOp`s); the records stay C++ value classes whose members the
//! callers read. Reached by nothing in the game (the WON patch servers are gone).

use std::collections::BTreeSet;
use std::ffi::c_void;

use crate::ffi::EeWonByteSink;
use crate::wire::{PastEnd, Reader, Writer};

/// A string field's or a number's assignment.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonPatchField {
    /// the field (the record's order), or `EE_WON_PATCH_CUT`
    pub slot: u8,
    /// false: a string whose length was read but not its bytes (`ReadRawString` erased it)
    pub whole: bool,
    pub num: i32,
    pub at: u32,
    pub len: u32,
}

/// The read failed here: the original threw `ReadBufferException` (`at`: the reader's position).
pub const EE_WON_PATCH_CUT: u8 = 255;
/// The read ended whole (`at`: the reader's position).
pub const EE_WON_PATCH_END: u8 = 254;

// VersionData's fields
pub const EE_WON_VERSION_CONFIG: u8 = 0;
pub const EE_WON_VERSION_VERSION: u8 = 1;
/// one version type pushed (a piece of a comma-separated type string)
pub const EE_WON_VERSION_TYPE: u8 = 2;
pub const EE_WON_VERSION_DESCRIPTION: u8 = 3;
pub const EE_WON_VERSION_STATE: u8 = 4;

// PatchData's fields
pub const EE_WON_PATCH_CONFIG: u8 = 0;
pub const EE_WON_PATCH_FROM: u8 = 1;
pub const EE_WON_PATCH_TO: u8 = 2;
pub const EE_WON_PATCH_ACTIVE: u8 = 3;
pub const EE_WON_PATCH_DISPLAY: u8 = 4;
pub const EE_WON_PATCH_NET_ADDRESS: u8 = 5;
pub const EE_WON_PATCH_CHECKSUM: u8 = 6;
pub const EE_WON_PATCH_SIZE: u8 = 7;
pub const EE_WON_PATCH_HOST_NAME: u8 = 8;
pub const EE_WON_PATCH_HOST_URL: u8 = 9;
pub const EE_WON_PATCH_HOST_TEXT: u8 = 10;
pub const EE_WON_PATCH_HOST_IMAGE: u8 = 11;
pub const EE_WON_PATCH_MUST_VISIT: u8 = 12;

/// The fields read so far.
struct Fields<'a> {
    r: Reader<'a>,
    base: usize,
    out: Vec<EeWonPatchField>,
}

impl<'a> Fields<'a> {
    fn new(data: &'a [u8], base: usize) -> Fields<'a> {
        Fields { r: Reader::new(data), base, out: Vec::new() }
    }

    /// A `ReadString` (2-byte length) into a member: erased when its bytes are missing.
    fn string(&mut self, slot: u8) -> Result<(), PastEnd> {
        let n = self.r.read_length(2)?;
        let at = self.r.pos();
        if self.r.read_bytes(n as usize).is_err() {
            self.out.push(EeWonPatchField { slot, whole: false, ..Default::default() });
            return Err(PastEnd);
        }
        self.out.push(EeWonPatchField { slot, whole: true, num: 0, at: (self.base + at) as u32, len: n });
        Ok(())
    }

    fn num(&mut self, slot: u8, v: i32) {
        self.out.push(EeWonPatchField { slot, whole: true, num: v, ..Default::default() });
    }

    fn end(mut self, r: Result<(), PastEnd>) -> Vec<EeWonPatchField> {
        let slot = if r.is_err() { EE_WON_PATCH_CUT } else { EE_WON_PATCH_END };
        self.out.push(EeWonPatchField { slot, at: self.r.pos() as u32, ..Default::default() });
        self.out
    }
}

/// `AddVersionType`: the pieces between commas (offsets in `s`).
pub fn split_types(s: &[u8]) -> Vec<(usize, usize)> {
    let mut v = Vec::new();
    let mut last = 0;
    for (i, &c) in s.iter().enumerate() {
        if c == b',' {
            v.push((last, i));
            last = i + 1;
        }
    }
    v.push((last, s.len()));
    v
}

/// `VersionData::ReadFromBuffer`'s assignments.
pub fn read_version(data: &[u8]) -> Vec<EeWonPatchField> {
    let mut f = Fields::new(data, 0);
    let r = (|| {
        f.string(EE_WON_VERSION_CONFIG)?;
        f.string(EE_WON_VERSION_VERSION)?;
        let n = f.r.read_byte()?;
        for _ in 0..n {
            // a local string: a cut one is not seen
            let s = f.r.read_string(2)?;
            let at = f.r.pos() - s.len();
            for (a, b) in split_types(s) {
                f.out.push(EeWonPatchField {
                    slot: EE_WON_VERSION_TYPE,
                    whole: true,
                    num: 0,
                    at: (at + a) as u32,
                    len: (b - a) as u32,
                });
            }
        }
        f.string(EE_WON_VERSION_DESCRIPTION)?;
        let s = f.r.read_byte()?;
        f.num(EE_WON_VERSION_STATE, i32::from(s));
        Ok(())
    })();
    f.end(r)
}

/// `PatchData::ReadFromBuffer`'s assignments (`list`: from a patch list, with the config and versions).
pub fn read_patch(data: &[u8], base: usize, list: bool) -> Vec<EeWonPatchField> {
    let mut f = Fields::new(data, base);
    let r = (|| {
        if list {
            f.string(EE_WON_PATCH_CONFIG)?;
            f.string(EE_WON_PATCH_FROM)?;
            f.string(EE_WON_PATCH_TO)?;
            let b = f.r.read_bool()?;
            f.num(EE_WON_PATCH_ACTIVE, i32::from(b));
        }
        f.string(EE_WON_PATCH_DISPLAY)?;
        f.string(EE_WON_PATCH_NET_ADDRESS)?;
        let v = f.r.read_long()?;
        f.num(EE_WON_PATCH_CHECKSUM, v);
        let v = f.r.read_long()?;
        f.num(EE_WON_PATCH_SIZE, v);
        f.string(EE_WON_PATCH_HOST_NAME)?;
        f.string(EE_WON_PATCH_HOST_URL)?;
        f.string(EE_WON_PATCH_HOST_TEXT)?;
        f.string(EE_WON_PATCH_HOST_IMAGE)?;
        let b = f.r.read_bool()?;
        f.num(EE_WON_PATCH_MUST_VISIT, i32::from(b));
        Ok(())
    })();
    f.end(r)
}

/// `VersionData::WriteToBuffer`: the config, version and types, the description URL (flag 2), the state (flag 1).
pub fn write_version(
    w: &mut Writer,
    config: &[u8],
    version: &[u8],
    types: &[&[u8]],
    description: &[u8],
    state: u8,
    flags: u32,
) {
    w.append_string(config, 2);
    w.append_string(version, 2);
    w.append_byte(types.len() as u8);
    for t in types {
        w.append_string(t, 2);
    }
    if flags & 2 != 0 {
        w.append_string(description, 2);
    }
    if flags & 1 != 0 {
        w.append_byte(state);
    }
}

/// `PatchData::WriteToBuffer`'s fields.
pub struct Patch<'a> {
    pub strings: [&'a [u8]; 9],
    pub checksum: i32,
    pub size: i32,
    pub must_visit: bool,
    pub active: bool,
}

/// `PatchData::WriteToBuffer` (strings: config, from, to, display, net address, host name, URL, text, image).
pub fn write_patch(w: &mut Writer, p: &Patch, update_only: bool) {
    let s = &p.strings;
    w.append_string(s[0], 2);
    w.append_string(s[1], 2);
    w.append_string(s[2], 2);
    w.append_byte(u8::from(update_only));
    w.append_string(s[3], 2);
    w.append_string(s[4], 2);
    w.append_long(p.checksum);
    w.append_long(p.size);
    w.append_string(s[5], 2);
    w.append_string(s[6], 2);
    w.append_string(s[7], 2);
    w.append_string(s[8], 2);
    w.append_byte(u8::from(p.must_visit));
    w.append_byte(u8::from(p.active));
}

/// `CheckValidVersionOp::CheckResponse`'s and `GetPatchListOp::CheckResponse`'s reply data: the target version and
/// its description URL (the version check only), then each patch's record (`EE_WON_PATCH_RECORD`: its range,
/// read whole by `PatchData::ReadFromBuffer`).
pub const EE_WON_PATCH_VERSION_TO: u8 = 20;
pub const EE_WON_PATCH_VERSION_URL: u8 = 21;
pub const EE_WON_PATCH_RECORD: u8 = 22;

pub fn read_patch_reply(data: &[u8], version_check: bool) -> Vec<EeWonPatchField> {
    let mut f = Fields::new(data, 0);
    let r = (|| {
        if version_check {
            f.string(EE_WON_PATCH_VERSION_TO)?;
            f.string(EE_WON_PATCH_VERSION_URL)?;
        }
        let n = f.r.read_short()? as u16;
        for _ in 0..n {
            let len = f.r.read_short()? as u16 as usize;
            let at = f.r.pos();
            let record = f.r.read_bytes(len)?;
            // the record is pushed only when its read ends
            if read_patch(record, at, !version_check).last().map(|x| x.slot) != Some(EE_WON_PATCH_END) {
                return Err(PastEnd);
            }
            f.out.push(EeWonPatchField {
                slot: EE_WON_PATCH_RECORD,
                whole: true,
                num: 0,
                at: at as u32,
                len: len as u32,
            });
        }
        Ok(())
    })();
    f.end(r)
}

/// `CheckValidVersionOp::RunHook`'s request data: the product, the version record (flags 0), the patch list flag.
pub fn version_check_request(
    product: &[u8],
    config: &[u8],
    version: &[u8],
    types: &[&[u8]],
    get_patch_list: bool,
) -> Vec<u8> {
    let mut w = Writer::new(0, true);
    w.append_string(product, 2);
    write_version(&mut w, config, version, types, &[], 0, 0);
    w.append_bool(get_patch_list);
    w.data().to_vec()
}

/// `GetPatchListOp::RunHook`'s request data (product, config, from, to, the state filter) and
/// `ReportPatchStatusOp::RunHook`'s (product, config, from, to, the patch URL, the status).
pub fn strings_and_byte(strings: &[&[u8]], byte: u8) -> Vec<u8> {
    let mut w = Writer::new(0, true);
    for s in strings {
        w.append_string(s, 2);
    }
    w.append_byte(byte);
    w.data().to_vec()
}

// ---- the version filter

/// `BuildVersionFilterSet`: the pieces between commas (none for an empty string).
pub fn filter_set(s: &[u8]) -> BTreeSet<Vec<u8>> {
    if s.is_empty() {
        return BTreeSet::new();
    }
    split_types(s).into_iter().map(|(a, b)| s[a..b].to_vec()).collect()
}

fn find_from(s: &[u8], c: u8, from: usize) -> Option<usize> {
    s.get(from..)?.iter().position(|&x| x == c).map(|n| n + from)
}

/// `IsFiltered`: the version's data (lowercased, ASCII) holds a "pt" line whose comma-separated values are not
/// all in `filters` (the filter set, already lowercased).
pub fn is_filtered(data: &[u8], filters: &BTreeSet<Vec<u8>>) -> bool {
    if data.is_empty() {
        return false;
    }
    let d: Vec<u8> = data.iter().map(|c| c.to_ascii_lowercase()).collect();
    // unsigned int positions (npos as 0xffffffff, its +1 as 0), as the original's
    const NPOS: u32 = u32::MAX;
    let pos = |p: Option<usize>| p.map_or(NPOS, |n| n as u32);
    let sub = |a: u32, n: u32| -> Vec<u8> {
        let a = a as usize;
        if a > d.len() {
            return Vec::new(); // substr past the end throws in the original; never reached for a found line
        }
        let n = (n as usize).min(d.len() - a);
        d[a..a + n].to_vec()
    };
    let mut line_start: u32 = 0;
    let mut space: u32 = 0;
    let mut key: Vec<u8> = Vec::new();
    while line_start != NPOS && key != b"pt" {
        space = pos(find_from(&d, b' ', line_start as usize));
        key = sub(line_start, space.wrapping_sub(line_start));
        line_start = pos(if space == NPOS { None } else { find_from(&d, b'\n', space as usize) });
        if line_start != NPOS {
            line_start += 1;
        } else {
            line_start = pos(if space == NPOS { None } else { find_from(&d, b'\r', space as usize) });
            if line_start != NPOS {
                line_start += 1;
            }
        }
    }
    if key != b"pt" {
        return false;
    }
    let mut value = if line_start != NPOS {
        sub(space.wrapping_add(1), line_start.wrapping_sub(space.wrapping_add(1)))
    } else {
        sub(space.wrapping_add(1), (d.len() as u32).wrapping_sub(space.wrapping_add(1)))
    };
    if line_start != NPOS {
        if let Some(n) = value.iter().position(|&c| c == b'\n') {
            value.remove(n);
        }
        if let Some(n) = value.iter().position(|&c| c == b'\r') {
            value.remove(n);
        }
    }
    filter_set(&value).iter().any(|t| !t.is_empty() && !filters.contains(t))
}

// ---- the C ABI

unsafe fn slice<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if n == 0 || p.is_null() {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// The fields of a read, handed over once.
pub type EeWonPatchSink = unsafe extern "C" fn(context: *mut c_void, fields: *const EeWonPatchField, count: usize);

/// A record's or reply's read into `sink`: `kind` 0 a `VersionData`, 1 a `PatchData`, 2 a `PatchData` of a patch
/// list, 3 the version check's reply data, 4 the patch list's.
///
/// # Safety
/// `data` holds `len` bytes; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_patch_read(
    kind: i32,
    data: *const u8,
    len: usize,
    sink: EeWonPatchSink,
    context: *mut c_void,
) {
    let d = unsafe { slice(data, len) };
    let f = match kind {
        0 => read_version(d),
        1 => read_patch(d, 0, false),
        2 => read_patch(d, 0, true),
        3 => read_patch_reply(d, true),
        _ => read_patch_reply(d, false),
    };
    unsafe { sink(context, f.as_ptr(), f.len()) }
}

/// Byte strings handed over.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonPatchBytes {
    pub data: *const u8,
    pub len: usize,
}

unsafe fn bytes<'a>(b: &EeWonPatchBytes) -> &'a [u8] {
    unsafe { slice(b.data, b.len) }
}

unsafe fn list<'a>(p: *const EeWonPatchBytes, n: usize) -> Vec<&'a [u8]> {
    if n == 0 {
        return Vec::new();
    }
    unsafe { std::slice::from_raw_parts(p, n) }.iter().map(|b| unsafe { bytes(b) }).collect()
}

/// `VersionData::WriteToBuffer`'s bytes (`types`: `count` strings) into `sink`.
///
/// # Safety
/// The strings are valid; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_version_write(
    config: EeWonPatchBytes,
    version: EeWonPatchBytes,
    types: *const EeWonPatchBytes,
    count: usize,
    description: EeWonPatchBytes,
    state: u8,
    flags: u32,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let mut w = Writer::new(0, true);
    unsafe {
        write_version(&mut w, bytes(&config), bytes(&version), &list(types, count), bytes(&description), state, flags)
    };
    unsafe { sink(context, w.data().as_ptr(), w.data().len()) }
}

/// `PatchData::WriteToBuffer`'s bytes (`strings`: the nine strings in `Patch`'s order) into `sink`.
///
/// # Safety
/// `strings` holds nine strings; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_patch_write(
    strings: *const EeWonPatchBytes,
    checksum: i32,
    size: i32,
    must_visit: bool,
    active: bool,
    update_only: bool,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let s = unsafe { list(strings, 9) };
    let p =
        Patch { strings: [s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7], s[8]], checksum, size, must_visit, active };
    let mut w = Writer::new(0, true);
    write_patch(&mut w, &p, update_only);
    unsafe { sink(context, w.data().as_ptr(), w.data().len()) }
}

/// A patch op's request data into `sink`: `kind` 0 the version check (`strings`: product, config, version, then
/// the types; `byte` the patch list flag), else `count` strings and the byte.
///
/// # Safety
/// `strings` holds `count` strings; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_patch_request(
    kind: i32,
    strings: *const EeWonPatchBytes,
    count: usize,
    byte: u8,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let s = unsafe { list(strings, count) };
    let m = if kind == 0 {
        version_check_request(s[0], s[1], s[2], &s[3..], byte != 0)
    } else {
        strings_and_byte(&s, byte)
    };
    unsafe { sink(context, m.as_ptr(), m.len()) }
}

/// `BuildVersionFilterSet`'s and `AddVersionType`'s pieces between commas (`EE_WON_VERSION_TYPE` fields; none
/// for an empty string when `filter`) into `sink`.
///
/// # Safety
/// `text` holds `len` bytes; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_patch_split(
    text: *const u8,
    len: usize,
    filter: bool,
    sink: EeWonPatchSink,
    context: *mut c_void,
) {
    let t = unsafe { slice(text, len) };
    let f: Vec<EeWonPatchField> = if filter && t.is_empty() {
        Vec::new()
    } else {
        split_types(t)
            .into_iter()
            .map(|(a, b)| EeWonPatchField {
                slot: EE_WON_VERSION_TYPE,
                whole: true,
                num: 0,
                at: a as u32,
                len: (b - a) as u32,
            })
            .collect()
    };
    unsafe { sink(context, f.as_ptr(), f.len()) }
}

/// `IsFiltered` of a version's data (its C string) against the filter set (`count` strings).
///
/// # Safety
/// `data` holds `len` bytes; `filters` holds `count` strings.
#[no_mangle]
pub unsafe extern "C" fn ee_won_version_is_filtered(
    data: *const u8,
    len: usize,
    filters: *const EeWonPatchBytes,
    count: usize,
) -> bool {
    let set: BTreeSet<Vec<u8>> = unsafe { list(filters, count) }.into_iter().map(|b| b.to_vec()).collect();
    is_filtered(crate::http::c_str(unsafe { slice(data, len) }), &set)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn s(w: &mut Writer, t: &[u8]) {
        w.append_string(t, 2);
    }

    #[test]
    fn versions() {
        let mut w = Writer::new(0, true);
        write_version(&mut w, b"cfg", b"1.0", &[b"a,b", b"c"], b"url", 3, 3);
        let d = w.data().to_vec();
        let f = read_version(&d);
        let slots: Vec<u8> = f.iter().map(|x| x.slot).collect();
        assert_eq!(slots, [0, 1, 2, 2, 2, 3, 4, EE_WON_PATCH_END]);
        assert_eq!(f[7].at as usize, d.len());
        assert_eq!(&d[f[2].at as usize..(f[2].at + f[2].len) as usize], b"a");
        assert_eq!(&d[f[3].at as usize..(f[3].at + f[3].len) as usize], b"b");
        assert_eq!(f[6].num, 3);
        // cut inside the version string: erased, then the cut
        let f = read_version(&d[..7]);
        assert_eq!((f[1].slot, f[1].whole, f[2].slot), (1, false, EE_WON_PATCH_CUT));
        assert_eq!(split_types(b""), [(0, 0)]);
        assert_eq!(split_types(b",x,"), [(0, 0), (1, 2), (3, 3)]);
    }

    #[test]
    fn patches() {
        let p = Patch {
            strings: [b"c", b"f", b"t", b"d", b"n", b"h", b"u", b"x", b"i"],
            checksum: -2,
            size: 9,
            must_visit: true,
            active: false,
        };
        let mut w = Writer::new(0, true);
        write_patch(&mut w, &p, true);
        let d = w.data().to_vec();
        let f = read_patch(&d, 0, true);
        assert_eq!(f.len(), 14);
        assert_eq!((f[3].num, f[6].num, f[7].num, f[12].num), (1, -2, 9, 1));
        let f = read_patch(&d[..d.len() - 2], 0, true);
        assert_eq!(f.last().unwrap().slot, EE_WON_PATCH_CUT);
        // a version check's reply with one record
        let mut r = Writer::new(0, true);
        s(&mut r, b"2.0");
        s(&mut r, b"u");
        r.append_short(1);
        let mut rec = Writer::new(0, true);
        write_patch(&mut rec, &p, false);
        let rec = rec.data()[10..].to_vec(); // without config, from, to and the flag
        r.append_short(rec.len() as i16);
        r.append_bytes(&rec);
        let f = read_patch_reply(r.data(), true);
        assert_eq!(f.iter().map(|x| x.slot).collect::<Vec<_>>(), [20, 21, 22, EE_WON_PATCH_END]);
        let f = read_patch_reply(&r.data()[..r.data().len() - 1], true);
        assert_eq!(f.iter().map(|x| x.slot).collect::<Vec<_>>(), [20, 21, EE_WON_PATCH_CUT]);
        assert_eq!(strings_and_byte(&[b"a"], 7), [1, 0, b'a', 7]);
    }

    #[test]
    fn filters() {
        let f = filter_set(b"opt,res");
        assert!(!is_filtered(b"", &f));
        assert!(!is_filtered(b"xx 1\npt opt", &f));
        assert!(is_filtered(b"xx 1\npt opt,lo\n", &f));
        assert!(!is_filtered(b"PT OPT,RES\r\n", &f));
        assert!(!is_filtered(b"pt ,,\n", &f));
        assert!(!is_filtered(b"nothing here", &f));
        assert!(filter_set(b"").is_empty());
    }
}
