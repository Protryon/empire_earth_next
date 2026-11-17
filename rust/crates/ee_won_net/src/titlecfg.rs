//! WONInternal's title and community configs (`TitleConfig.cpp`): which child a `TITLEDEF` or `COMMUNITY`
//! instruction opens and the nesting errors, `CommunityConfig::PostParse`'s rules (the name-login mode, the image
//! path and hash string defaults, the trust level lifespan map from `CertificateLifeSpan`) and both `ToString`
//! texts. The C++ classes stay `ConfigObject`s (the config parser calls their virtual hooks and binds their keys
//! to members); `include/ee/wonapi/WONTitleConfigMsg.h` applies these in the original's order.
//!
//! Reached by nothing in the game (the auth server's config files; the WON servers are gone).

use std::collections::BTreeMap;
use std::ffi::c_void;

use crate::ffi::{give, EeWonByteSink};

/// What `HandleInstruction` does with an instruction its `ConfigObject` base did not take.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(C)]
pub enum EeWonTitleInstr {
    /// Not a title instruction: `false`.
    NotMine,
    /// Parse a child `TitleConfig(true)`.
    ChildTitle,
    /// Parse a child `CommunityConfig` (`alreadyInCommunity` as the field says).
    ChildCommunity,
    /// `"You can't nest Title definitions!"`.
    ErrorNestTitle,
    /// `"You can't have a title definition within a community!"`.
    ErrorTitleInCommunity,
    /// `"You can't nest communities!"`.
    ErrorNestCommunity,
}

/// The error text of an instruction's verdict (empty for the others).
pub fn instr_error(i: EeWonTitleInstr) -> &'static str {
    match i {
        EeWonTitleInstr::ErrorNestTitle => "You can't nest Title definitions!",
        EeWonTitleInstr::ErrorTitleInCommunity => "You can't have a title definition within a community!",
        EeWonTitleInstr::ErrorNestCommunity => "You can't nest communities!",
        _ => "",
    }
}

/// `TitleConfig::HandleInstruction` (`community` false) or `CommunityConfig::HandleInstruction` (true) for an
/// instruction type (compared exactly, as `std::string ==`); `already` is `mAlreadyInTitle` or
/// `mAlreadyInCommunity`. A title's `COMMUNITY` child is a `CommunityConfig()` (not already in one), a
/// community's is `CommunityConfig(true)`.
pub fn instruction(community: bool, already: bool, ty: &[u8]) -> EeWonTitleInstr {
    match (community, ty) {
        (false, b"TITLEDEF") if already => EeWonTitleInstr::ErrorNestTitle,
        (false, b"TITLEDEF") => EeWonTitleInstr::ChildTitle,
        (false, b"COMMUNITY") => EeWonTitleInstr::ChildCommunity,
        (true, b"TITLEDEF") => EeWonTitleInstr::ErrorTitleInCommunity,
        (true, b"COMMUNITY") if already => EeWonTitleInstr::ErrorNestCommunity,
        (true, b"COMMUNITY") => EeWonTitleInstr::ChildCommunity,
        _ => EeWonTitleInstr::NotMine,
    }
}

/// `CommunityConfig::NameLogin` of the `NameLogin` key (upper-cased in ASCII, D6): None 0, Optional 1,
/// Required 2, else Unknown 3.
pub fn name_login(s: &[u8]) -> i32 {
    match s.to_ascii_uppercase().as_slice() {
        b"REQUIRED" => 2,
        b"OPTIONAL" => 1,
        b"NONE" => 0,
        _ => 3,
    }
}

/// The trust level lifespan map: each list's first value is a trust level, its second (when there is one) the
/// lifespan; a later list for the same level replaces the earlier. `lists` is flattened as a count then that many
/// values per list. Returns the map in key order.
pub fn lifespans(flat: &[i32]) -> Vec<(i32, i32)> {
    let mut map = BTreeMap::new();
    let mut i = 0usize;
    while i < flat.len() {
        let n = flat[i].max(0) as usize;
        let vals = &flat[i + 1..(i + 1 + n).min(flat.len())];
        if vals.len() >= 2 {
            map.insert(vals[0], vals[1]);
        }
        i += 1 + n;
    }
    map.into_iter().collect()
}

/// The title fields `ToString` prints, in its order after `NetPort`.
#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
pub struct EeWonTitleFlags {
    pub force_cd_key: bool,
    pub require_crc: bool,
    pub allow_beta_key: bool,
    pub require_cd_key: bool,
    pub fail_to_guest: bool,
    pub allow_guest: bool,
    pub concurrent_user_login: bool,
    pub concurrent_cd_key_login: bool,
    pub get_user_data: bool,
    pub get_forgeable_user_data: bool,
    pub cd_key_for_invite: bool,
    pub save_session_key: bool,
    pub get_key_id: bool,
    pub ascii_account_names: bool,
}

/// The strings `ToString` prints (each a pointer and a length).
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct EeWonTitleText {
    pub name: *const u8,
    pub name_len: usize,
    pub network: *const u8,
    pub network_len: usize,
    pub image_path: *const u8,
    pub image_path_len: usize,
    pub hash_string: *const u8,
    pub hash_string_len: usize,
    pub key_prefix: *const u8,
    pub key_prefix_len: usize,
    pub net_port: u16,
}

/// The title text's values, as slices.
pub struct Title<'a> {
    pub name: Option<&'a [u8]>,
    pub network: &'a [u8],
    pub net_port: u16,
    pub image_path: &'a [u8],
    pub hash_string: &'a [u8],
    pub key_prefix: &'a [u8],
    pub flags: EeWonTitleFlags,
}

fn or_empty(s: &[u8]) -> &[u8] {
    if s.is_empty() {
        b"EMPTY"
    } else {
        s
    }
}

fn flag(out: &mut Vec<u8>, label: &str, v: bool) {
    out.extend_from_slice(b"\t");
    out.extend_from_slice(label.as_bytes());
    out.extend_from_slice(b": ");
    out.extend_from_slice(if v { b"TRUE" } else { b"FALSE" });
    out.extend_from_slice(b"\r\n");
}

fn text(out: &mut Vec<u8>, label: &str, v: &[u8]) {
    out.extend_from_slice(b"\t");
    out.extend_from_slice(label.as_bytes());
    out.extend_from_slice(b": ");
    out.extend_from_slice(v);
    out.extend_from_slice(b"\r\n");
}

/// `TitleConfig::ToString` (the name line only when given). The port prints with `%d` of the promoted
/// `unsigned short`.
pub fn title_text(t: &Title) -> Vec<u8> {
    let mut out = Vec::new();
    if let Some(n) = t.name {
        out.extend_from_slice(n);
        out.extend_from_slice(b"\r\n");
    }
    let f = &t.flags;
    text(&mut out, "Network", or_empty(t.network));
    text(&mut out, "NetPort", t.net_port.to_string().as_bytes());
    flag(&mut out, "ForceCDKey", f.force_cd_key);
    text(&mut out, "ImagePath", or_empty(t.image_path));
    text(&mut out, "HashString", or_empty(t.hash_string));
    text(&mut out, "KeyPrefix", or_empty(t.key_prefix));
    flag(&mut out, "RequireCrc", f.require_crc);
    flag(&mut out, "AllowBetaKey", f.allow_beta_key);
    flag(&mut out, "RequireCDKey", f.require_cd_key);
    flag(&mut out, "FailToGuest", f.fail_to_guest);
    flag(&mut out, "AllowGuest", f.allow_guest);
    flag(&mut out, "ConcurrentUserLogin", f.concurrent_user_login);
    flag(&mut out, "ConcurrentCDKeyLogin", f.concurrent_cd_key_login);
    flag(&mut out, "GetUserData", f.get_user_data);
    flag(&mut out, "GetForgeableUserData", f.get_forgeable_user_data);
    flag(&mut out, "CDKeyForInvite", f.cd_key_for_invite);
    flag(&mut out, "SaveSessionKey", f.save_session_key);
    flag(&mut out, "GetKeyID", f.get_key_id);
    flag(&mut out, "ASCIIAccountNames", f.ascii_account_names);
    out
}

/// `CommunityConfig::ToString`: the community's name (its narrow `mCommunityName`, or `mName` when that is
/// empty), its id, the title text without a name, the name-login string and the lifespan map.
pub fn community_text(header: &[u8], id: i32, title: &Title, name_login: &[u8], map: &[(i32, i32)]) -> Vec<u8> {
    let mut out = header.to_vec();
    out.extend_from_slice(b"\r\n");
    out.extend_from_slice(format!("CommunityID: {id}\r\n").as_bytes());
    out.extend_from_slice(&title_text(&Title { name: None, ..*title }));
    text(&mut out, "NameLogin", name_login);
    out.extend_from_slice(b"\tTrust Level Lifespans: [");
    if map.is_empty() {
        out.extend_from_slice(b"EMPTY");
    } else {
        let parts: Vec<String> = map.iter().map(|(k, v)| format!("({k},{v})")).collect();
        out.extend_from_slice(parts.join(",").as_bytes());
    }
    out.extend_from_slice(b"]\r\n");
    out
}

// ---- FFI

unsafe fn sl<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

unsafe fn title_of<'a>(t: &EeWonTitleText, flags: &EeWonTitleFlags, with_name: bool) -> Title<'a> {
    unsafe {
        Title {
            name: if with_name { Some(sl(t.name, t.name_len)) } else { None },
            network: sl(t.network, t.network_len),
            net_port: t.net_port,
            image_path: sl(t.image_path, t.image_path_len),
            hash_string: sl(t.hash_string, t.hash_string_len),
            key_prefix: sl(t.key_prefix, t.key_prefix_len),
            flags: *flags,
        }
    }
}

/// [`instruction`] over an instruction type's bytes.
///
/// # Safety
/// `ty` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_titlecfg_instruction(
    community: bool,
    already: bool,
    ty: *const u8,
    len: usize,
) -> EeWonTitleInstr {
    instruction(community, already, unsafe { sl(ty, len) })
}

/// [`instr_error`] handed to `sink` (nothing for a verdict without one).
///
/// # Safety
/// `sink` and `context` form a valid sink.
#[no_mangle]
pub unsafe extern "C" fn ee_won_titlecfg_instr_error(i: EeWonTitleInstr, sink: EeWonByteSink, context: *mut c_void) {
    unsafe { give(sink, context, instr_error(i).as_bytes()) };
}

/// [`name_login`].
///
/// # Safety
/// `s` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_titlecfg_name_login(s: *const u8, len: usize) -> i32 {
    name_login(unsafe { sl(s, len) })
}

/// [`lifespans`]: the map's pairs (key, value) handed to `put` in key order.
///
/// # Safety
/// `flat` holds `len` values; `put` is valid with `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_titlecfg_lifespans(
    flat: *const i32,
    len: usize,
    put: unsafe extern "C" fn(context: *mut c_void, key: i32, value: i32),
    context: *mut c_void,
) {
    let flat = if flat.is_null() || len == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(flat, len) } };
    for (k, v) in lifespans(flat) {
        unsafe { put(context, k, v) };
    }
}

/// [`title_text`] handed to `sink`.
///
/// # Safety
/// The text's pointers hold their lengths; `sink` is valid with `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_titlecfg_title_text(
    t: *const EeWonTitleText,
    flags: *const EeWonTitleFlags,
    with_name: bool,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let out = unsafe { title_text(&title_of(&*t, &*flags, with_name)) };
    unsafe { give(sink, context, &out) };
}

/// [`community_text`] handed to `sink`; the map comes flattened as key, value pairs.
///
/// # Safety
/// The pointers hold their lengths; `sink` is valid with `context`.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_titlecfg_community_text(
    header: *const u8,
    header_len: usize,
    id: i32,
    t: *const EeWonTitleText,
    flags: *const EeWonTitleFlags,
    name_login: *const u8,
    name_login_len: usize,
    pairs: *const i32,
    pair_count: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let flat = if pairs.is_null() || pair_count == 0 {
        &[][..]
    } else {
        unsafe { std::slice::from_raw_parts(pairs, pair_count * 2) }
    };
    let map: Vec<(i32, i32)> = flat.chunks(2).map(|c| (c[0], c[1])).collect();
    let out = unsafe {
        community_text(sl(header, header_len), id, &title_of(&*t, &*flags, false), sl(name_login, name_login_len), &map)
    };
    unsafe { give(sink, context, &out) };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn instructions() {
        use EeWonTitleInstr::*;
        assert_eq!(instruction(false, false, b"TITLEDEF"), ChildTitle);
        assert_eq!(instruction(false, true, b"TITLEDEF"), ErrorNestTitle);
        assert_eq!(instruction(false, true, b"COMMUNITY"), ChildCommunity);
        assert_eq!(instruction(true, false, b"TITLEDEF"), ErrorTitleInCommunity);
        assert_eq!(instruction(true, false, b"COMMUNITY"), ChildCommunity);
        assert_eq!(instruction(true, true, b"COMMUNITY"), ErrorNestCommunity);
        assert_eq!(instruction(false, false, b"community"), NotMine);
        assert_eq!(instr_error(ErrorNestCommunity), "You can't nest communities!");
        assert_eq!(instr_error(ChildTitle), "");
    }

    #[test]
    fn name_logins() {
        assert_eq!(name_login(b"Required"), 2);
        assert_eq!(name_login(b"optional"), 1);
        assert_eq!(name_login(b"NONE"), 0);
        assert_eq!(name_login(b""), 3);
        assert_eq!(name_login(b"none "), 3);
    }

    #[test]
    fn lifespan_maps() {
        // [3, 10] [] [1, 5, 9] [7] [3, 20]
        let flat = [2, 3, 10, 0, 3, 1, 5, 9, 1, 7, 2, 3, 20];
        assert_eq!(lifespans(&flat), vec![(1, 5), (3, 20)]);
        assert_eq!(lifespans(&[]), vec![]);
        assert_eq!(lifespans(&[2, -1, i32::MIN]), vec![(-1, i32::MIN)]);
    }

    fn unhex(s: &str) -> Vec<u8> {
        if s == "-" {
            return Vec::new();
        }
        (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
    }

    fn ints(s: &str) -> Vec<i32> {
        if s == "-" {
            Vec::new()
        } else {
            s.split(',').map(|v| v.parse().unwrap()).collect()
        }
    }

    /// The original's results recorded by tests/won/won_titlecfg_test.cpp (`--print 120`).
    #[test]
    fn golden() {
        let mut n = [0usize; 3];
        for line in include_str!("golden_titlecfg.txt").lines() {
            let f: Vec<&str> = line.split(' ').collect();
            match f[0] {
                "instr" => {
                    let (community, already, ty, ok) = (f[1] == "1", f[2] == "1", unhex(f[3]), f[4] == "1");
                    let v = instruction(community, already, &ty);
                    let (r, log) = match v {
                        EeWonTitleInstr::NotMine => (false, String::new()),
                        EeWonTitleInstr::ChildTitle => (ok, "title;".to_string()),
                        EeWonTitleInstr::ChildCommunity => (ok, format!("community{};", u8::from(community))),
                        _ => (false, format!("error[{}]0;", instr_error(v))),
                    };
                    assert_eq!((r, log.into_bytes()), (f[5] == "1", unhex(f[6])), "{line}");
                    n[0] += 1;
                }
                "post" => {
                    let name = unhex(f[1]);
                    let (image, hash) = (unhex(f[2]), unhex(f[3]));
                    let parent = if f[6] == "*" { name.clone() } else { unhex(f[6]) };
                    assert_eq!(name_login(&unhex(f[4])), f[7].parse::<i32>().unwrap(), "{line}");
                    let image = if image.is_empty() { [b"./".as_slice(), &parent].concat() } else { image };
                    let hash = if hash.is_empty() { parent } else { hash };
                    assert_eq!(image, unhex(f[8]), "{line}");
                    assert_eq!(lifespans(&ints(f[5])).len(), f[9].parse::<usize>().unwrap(), "{line}");
                    assert_eq!(hash, unhex(f[10]), "{line}");
                    n[1] += 1;
                }
                "text" => {
                    let (name, network, image, hash, prefix) =
                        (unhex(f[2]), unhex(f[3]), unhex(f[5]), unhex(f[6]), unhex(f[7]));
                    let b: Vec<bool> = f[8].bytes().map(|c| c == b'1').collect();
                    let flags = EeWonTitleFlags {
                        force_cd_key: b[0],
                        require_crc: b[1],
                        allow_beta_key: b[2],
                        require_cd_key: b[3],
                        fail_to_guest: b[4],
                        allow_guest: b[5],
                        concurrent_user_login: b[6],
                        concurrent_cd_key_login: b[7],
                        get_user_data: b[8],
                        get_forgeable_user_data: b[9],
                        cd_key_for_invite: b[10],
                        save_session_key: b[11],
                        get_key_id: b[12],
                        ascii_account_names: b[13],
                    };
                    let t = Title {
                        name: if f[1] == "1" { Some(&name) } else { None },
                        network: &network,
                        net_port: f[4].parse().unwrap(),
                        image_path: &image,
                        hash_string: &hash,
                        key_prefix: &prefix,
                        flags,
                    };
                    assert_eq!(title_text(&t), unhex(f[13]), "{line}");
                    let p = ints(f[12]);
                    let map: Vec<(i32, i32)> = p.chunks(2).map(|c| (c[0], c[1])).collect();
                    let c = community_text(&unhex(f[9]), f[10].parse().unwrap(), &t, &unhex(f[11]), &map);
                    assert_eq!(c, unhex(f[14]), "{line}");
                    n[2] += 1;
                }
                _ => panic!("{line}"),
            }
        }
        assert_eq!(n, [120, 120, 120]);
    }

    #[test]
    fn texts() {
        let t = Title {
            name: Some(b"T"),
            network: b"",
            net_port: 65535,
            image_path: b"./x",
            hash_string: b"",
            key_prefix: b"k",
            flags: EeWonTitleFlags { fail_to_guest: true, ascii_account_names: true, ..Default::default() },
        };
        let s = String::from_utf8(title_text(&t)).unwrap();
        assert!(
            s.starts_with("T\r\n\tNetwork: EMPTY\r\n\tNetPort: 65535\r\n\tForceCDKey: FALSE\r\n\tImagePath: ./x\r\n")
        );
        assert!(s.contains("\tHashString: EMPTY\r\n\tKeyPrefix: k\r\n"));
        assert!(s.contains("\tFailToGuest: TRUE\r\n"));
        assert!(s.ends_with("\tGetKeyID: FALSE\r\n\tASCIIAccountNames: TRUE\r\n"));
        let c = String::from_utf8(community_text(b"C", -4, &t, b"Required", &[(1, 2), (3, -4)])).unwrap();
        assert!(c.starts_with("C\r\nCommunityID: -4\r\n\tNetwork: EMPTY\r\n"));
        assert!(c.ends_with("\tNameLogin: Required\r\n\tTrust Level Lifespans: [(1,2),(3,-4)]\r\n"));
        let e = String::from_utf8(community_text(b"", 0, &t, b"", &[])).unwrap();
        assert!(e.ends_with("\tNameLogin: \r\n\tTrust Level Lifespans: [EMPTY]\r\n"));
    }
}
