//! The title and community objects over real files (the original's statements run against them in
//! `won_titleobj_test`): the keys' defaults and reads, a community's copy of its title's keys and its
//! `PostParse`, the nesting errors, the texts, the references and the dummy community.

use super::*;
use crate::cfgobject::{
    ee_won_cfgo_child_count, ee_won_cfgo_get_all_key_values, ee_won_cfgo_next_child, ee_won_cfgo_rewind,
};

fn file(name: &str, text: &str) -> Vec<u8> {
    let dir = std::env::temp_dir().join(format!("ee_titleobj_{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let path = dir.join(name);
    std::fs::write(&path, text).unwrap();
    path.to_string_lossy().into_owned().into_bytes()
}

fn parse(o: *mut EeWonTitleObj, path: &[u8]) -> (i32, Vec<u8>, Vec<u8>) {
    // the config line count is process-wide: one parse at a time with the other config tests
    let _turn = crate::config::config_golden::LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let mut parser = EeWonCfgParser::new();
    let mut exc = Vec::new();
    let r = unsafe {
        ee_won_titleobj_parse(
            o,
            &mut parser,
            path.as_ptr(),
            path.len(),
            sink_vec,
            &mut exc as *mut Vec<u8> as *mut c_void,
        )
    };
    let abort = if r == 0 { parser.abort_reason().to_vec() } else { exc };
    (r, abort, parser.warnings().to_vec())
}

fn children(o: *mut EeWonTitleObj) -> Vec<*mut EeWonTitleObj> {
    let g = unsafe { (*o).obj };
    let mut out = Vec::new();
    unsafe {
        ee_won_cfgo_rewind(g);
        loop {
            let c = ee_won_cfgo_next_child(g);
            if c.is_null() {
                break;
            }
            out.push(c as *mut EeWonTitleObj);
        }
    }
    out
}

fn wide(s: &str) -> Vec<WChar> {
    s.chars().map(|c| c as WChar).collect()
}

#[test]
fn a_title_reads_its_keys_and_a_community_copies_them() {
    let path = file(
        "t1.cfg",
        "TITLEDEF Homeworld {\nNetwork: hw\nNetPort: 4000\nAllowGuestLogin: yes\nKeyPrefix: k\n\
         COMMUNITY Main {\nCommunityID: 12\nCertificateLifeSpan: (1, 60), (2, 90), (1, 30)\nNameLogin: optional\n}\n}\n",
    );
    let root = ee_won_titleobj_new(EeWonTitleObjKind::Title, false);
    let (r, abort, warn) = parse(root, &path);
    assert_eq!((r, abort.as_slice(), warn.as_slice()), (1, &b""[..], &b""[..]));
    let titles = children(root);
    assert_eq!(titles.len(), 1);
    let t = unsafe { &*titles[0] };
    assert_eq!(
        (t.kind, t.name.as_slice(), t.title.already_in_title),
        (EeWonTitleObjKind::Title, &b"Homeworld"[..], true)
    );
    assert_eq!((t.title.network.as_slice(), t.title.net_port, t.title.allow_guest), (&b"hw"[..], 4000, true));
    // the defaults: FailToGuestLogin and ConcurrentUserLogin true, the rest false, no strings
    assert!(t.title.fail_to_guest && t.title.concurrent_user_login && !t.title.require_crc);
    let communities = children(titles[0]);
    assert_eq!(communities.len(), 1);
    let c = unsafe { &*communities[0] };
    assert_eq!(c.kind, EeWonTitleObjKind::Community);
    // PreParse copied the title's keys; PostParse named the paths after the title
    assert_eq!(
        (c.title.network.as_slice(), c.title.net_port, c.title.key_prefix.as_slice()),
        (&b"hw"[..], 4000, &b"k"[..])
    );
    assert_eq!(
        (c.title.image_path.as_slice(), c.title.hash_string.as_slice()),
        (&b"./Homeworld"[..], &b"Homeworld"[..])
    );
    assert_eq!((c.community.community_id, c.community.name_login), (12, 1));
    assert_eq!(c.community.certificate_life_span_list, vec![vec![1, 60], vec![2, 90], vec![1, 30]]);
    assert_eq!(c.community.trust_level_lifespans, vec![(1, 30), (2, 90)]);
    assert_eq!(c.community.community_name, wide("Main"));
    let text = c.to_text(true);
    assert!(text.starts_with(b"Main\r\nCommunityID: 12\r\n\tNetwork: hw\r\n"), "{}", String::from_utf8_lossy(&text));
    let title_text = t.to_text(true);
    assert!(title_text.starts_with(b"Homeworld\r\n\tNetwork: hw\r\n\tNetPort: 4000\r\n"));
    assert!(t.to_text(false).starts_with(b"\tNetwork: hw\r\n"));
    let mut values = Vec::new();
    unsafe { ee_won_cfgo_get_all_key_values(c.obj, sink_vec, &mut values as *mut Vec<u8> as *mut c_void) };
    let values = String::from_utf8(values).unwrap();
    assert!(values.contains("CertificateLifeSpan: (1, 60), (2, 90), (1, 30)\n"), "{values}");
    assert!(values.contains("NetPort: 4000\n") && values.contains("AllowGuestLogin: True\n"), "{values}");
    assert!(values.contains("AdminCommunityName:   [DEF]\n"), "{values}");
    unsafe { ee_won_titleobj_ref(root, false) };
}

#[test]
fn nesting_is_an_error() {
    let path = file("t2.cfg", "TITLEDEF A {\nTITLEDEF B {\n}\n}\n");
    let root = ee_won_titleobj_new(EeWonTitleObjKind::Title, false);
    let (r, abort, _) = parse(root, &path);
    assert_eq!(r, 0);
    assert!(abort.starts_with(b"You can't nest Title definitions!"), "{}", String::from_utf8_lossy(&abort));
    unsafe { ee_won_titleobj_ref(root, false) };
    // a title's community is not in one: its own community child is allowed, that one's is not
    let path = file("t3.cfg", "COMMUNITY A {\nCOMMUNITY B {\nCOMMUNITY C {\n}\n}\n}\n");
    let root = ee_won_titleobj_new(EeWonTitleObjKind::Title, false);
    let (r, abort, _) = parse(root, &path);
    assert_eq!(r, 0);
    assert!(abort.starts_with(b"You can't nest communities!"), "{}", String::from_utf8_lossy(&abort));
    unsafe { ee_won_titleobj_ref(root, false) };
    let path = file("t4.cfg", "COMMUNITY A {\nTITLEDEF B {\n}\n}\n");
    let root = ee_won_titleobj_new(EeWonTitleObjKind::Title, false);
    let (r, abort, _) = parse(root, &path);
    assert_eq!(r, 0);
    assert!(abort.starts_with(b"You can't have a title definition within a community!"));
    unsafe { ee_won_titleobj_ref(root, false) };
}

#[test]
fn unknown_groups_are_skipped_and_bad_values_warn() {
    let path = file("t5.cfg", "Other {\nNetwork: x\n}\nNetPort: x\nNetwork: plain\nCommunityID: 3\n");
    let root = ee_won_titleobj_new(EeWonTitleObjKind::Title, false);
    let (r, _, warn) = parse(root, &path);
    assert_eq!(r, 1);
    let warn = String::from_utf8_lossy(&warn).into_owned();
    assert!(warn.contains("Unexpected child group: OTHER"), "{warn}");
    assert!(warn.contains("Failed to parse value for key: NetPort"), "{warn}");
    assert!(warn.contains("Unexpected key/val: COMMUNITYID 3"), "{warn}");
    let me = unsafe { &*root };
    assert_eq!((me.title.network.as_slice(), me.title.net_port), (&b"plain"[..], 15111));
    assert_eq!(unsafe { ee_won_cfgo_child_count(me.obj) }, 0);
    unsafe { ee_won_titleobj_ref(root, false) };
}

#[test]
fn a_missing_file_is_an_error_and_the_dummy_community_has_no_guest_fallback() {
    let root = ee_won_titleobj_new(EeWonTitleObjKind::Community, false);
    let (r, abort, _) = parse(root, b"no/such/title.cfg");
    assert_eq!(r, 0);
    assert!(abort.starts_with(b"File not found: no/such/title.cfg"));
    unsafe { ee_won_titleobj_ref(root, false) };
    let name = wide("Guests");
    let d = unsafe { ee_won_titleobj_new_dummy(name.as_ptr() as *const c_void, name.len(), 77) };
    let me = unsafe { &*d };
    assert_eq!((me.community.community_id, me.title.fail_to_guest, me.title.net_port), (77, false, 15111));
    assert!(me.to_text(false).starts_with(b"Guests\r\nCommunityID: 77\r\n"));
    let mut values = Vec::new();
    unsafe { ee_won_cfgo_get_all_key_values(me.obj, sink_vec, &mut values as *mut Vec<u8> as *mut c_void) };
    assert!(!String::from_utf8(values).unwrap().contains("CommunityID"));
    unsafe { ee_won_titleobj_ref(d, false) };
}

#[test]
fn the_dump_names_every_member() {
    let o = ee_won_titleobj_new(EeWonTitleObjKind::Plain, false);
    assert_eq!(dump(unsafe { &*o }), b"k0;;;");
    unsafe { ee_won_titleobj_ref(o, false) };
    let c = ee_won_titleobj_new(EeWonTitleObjKind::Community, true);
    let d = String::from_utf8(dump(unsafe { &*c })).unwrap();
    assert_eq!(d, "k2;;;a0;;p0;;;;00000000000000;;c1;i0;;;n0;;;");
    unsafe { ee_won_titleobj_ref(c, false) };
}
