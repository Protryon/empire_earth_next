//! `golden_verop.txt` replayed: each op made from the driver's members, run (its directory request), the listing's
//! replies with the versions and patches read after each, then the version checks and patch lists; then the reads
//! one by one.

use super::*;
use crate::dir::DataObject;

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len() / 2).map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap()).collect()
}

fn members_of(line: &str) -> (i32, Vec<Member>) {
    let mut t = line.split(' ');
    assert_eq!(t.next(), Some("OP"));
    let kind: i32 = t.next().unwrap().parse().unwrap();
    let members = t
        .map(|v| match v.split_at(1) {
            ("n", d) => Member { num: d.parse().unwrap(), bytes: Vec::new() },
            ("b", h) => Member { num: 0, bytes: if h.is_empty() { Vec::new() } else { unhex(h) } },
            _ => panic!("{v}"),
        })
        .collect();
    (kind, members)
}

fn verdict(c: Checked) -> String {
    match c {
        Checked::Status(s) => s.to_string(),
        Checked::InvalidHeader => "inv".into(),
        Checked::Cut | Checked::Datagram => "cut".into(),
        Checked::ServerError(e) => format!("err{e}"),
    }
}

#[test]
fn golden() {
    let text = include_str!("golden_verop.txt");
    let mut op: Option<Box<dyn RequestOp>> = None;
    let (mut seqs, mut listed, mut patches, mut versions, mut lists) = (0, 0, 0, 0, 0);
    for line in text.lines() {
        if line.starts_with("OP ") {
            let (kind, m) = members_of(line);
            let mut o = new_op(kind, &m).unwrap();
            o.run_hook();
            op = Some(o);
            seqs += 1;
            continue;
        }
        let o = op.as_mut().unwrap();
        if let Some(q) = line.strip_prefix("Q ") {
            let (s, r) = o.next_request(4);
            assert_eq!(format!("{s} {}", hex(&r.unwrap())), q);
        } else if let Some(r) = line.strip_prefix("R ") {
            let (reply, want) = r.split_once(" => ").unwrap();
            let got = format!("{} {}", verdict(o.check_response(&unhex(reply))), o.describe());
            assert_eq!(got, want, "R {reply}");
            listed += usize::from(want.starts_with("0 "));
            patches += usize::from(!want.ends_with("p[]"));
            versions += usize::from(!want.contains("v[] f[]"));
        } else if let Some(c) = line.strip_prefix("C ") {
            let (q, want) = c.split_once(" => ").unwrap();
            let f: Vec<&str> = q.split(' ').collect();
            o.set(EE_WON_REQOP_VERSION_CHECK, 0, &unhex(f[0]));
            let mut ft = unhex(f[2]);
            ft.push(0);
            ft.extend(unhex(f[3]));
            o.set(EE_WON_REQOP_PATCH_QUERY, f[1].parse().unwrap(), &ft);
            let mut l = Vec::new();
            for i in 0.. {
                let n = o.num_at(EE_WON_REQOP_PATCH_QUERY, i);
                if n < 0 {
                    break;
                }
                l.push(n.to_string());
            }
            lists += usize::from(!l.is_empty());
            assert_eq!(format!("{} [{}]", o.num_at(EE_WON_REQOP_VERSION_CHECK, 0), l.join(",")), want, "C {q}");
        } else {
            panic!("{line}");
        }
    }
    assert!(
        seqs >= 600 && listed > 200 && patches > 50 && versions > 100 && lists > 30,
        "{seqs} {listed} {patches} {versions} {lists}"
    );
}

fn entity(kind: u8, objects: &[(&str, Option<&str>)]) -> Entity {
    Entity {
        kind,
        data_objects: objects
            .iter()
            .map(|(t, d)| DataObject { data_type: t.as_bytes().to_vec(), data: d.map(|d| d.as_bytes().to_vec()) })
            .collect(),
        ..Default::default()
    }
}

#[test]
fn versions_are_valid_or_filtered_and_the_highest_kept() {
    let mut b = VersionBase::default();
    b.set_filter_string(b"OPT");
    b.build(&entity(
        b'D',
        &[("1.0", None), ("2.0", Some("pt res")), ("1.5", Some("pt opt")), ("1.1", Some("x\0pt res"))],
    ));
    assert_eq!(b.valid.iter().map(|v| v.as_slice()).collect::<Vec<_>>(), [&b"1.0"[..], b"1.1", b"1.5"]);
    assert_eq!(b.filtered.iter().collect::<Vec<_>>(), [&b"2.0".to_vec()]);
    assert_eq!(b.highest, b"1.5");
    assert_eq!(
        (b.check(b"1.5"), b.check(b"1.0"), b.check(b"2.0")),
        (VERSION_LATEST, VERSION_NOT_LATEST, VERSION_INVALID)
    );
    // the descriptions are the data whole (past a NUL)
    assert_eq!(b.descriptions[&b"1.1".to_vec()], b"x\0pt res");
}

#[test]
fn patches_read_their_id_checksum_and_size() {
    let mut e = entity(b'S', &[("_cs", Some("\x04\x03\x02\x01")), ("_ps", Some("ab")), ("note", None)]);
    e.name = text::from_utf8("1.0~1.1");
    e.fields = 0x8;
    e.net_addr = vec![1, 2];
    let p = PatchInfo::of(&e);
    assert_eq!((p.from.as_slice(), p.to.as_slice()), (&b"1.0"[..], &b"1.1"[..]));
    assert_eq!((p.checksum, p.size), (0x0102_0304, 0));
    assert_eq!(p.location, Some(vec![1, 2]));
    assert_eq!(p.data.get(b"note".as_slice()), Some(&None));
    let mut op = VersionOp::new(true, VersionBase::default());
    op.finish(&[e, entity(b'D', &[("1.1", None)])]);
    assert_eq!(op.patches.len(), 1);
    assert_eq!(op.patch_query(1, b"1.0", b""), vec![0]);
    assert_eq!(op.patch_query(0, b"2.0", b""), Vec::<usize>::new());
}

#[test]
fn the_version_directory_and_flags() {
    let b = VersionBase { product: b"EE".to_vec(), extra_config: b"en".to_vec(), ..Default::default() };
    assert_eq!(b.version_dir(), text::from_utf8("/EE/Version/en"));
    let mut op = VersionOp::new(true, b);
    assert_eq!(op.flags() & DIR_GF_DECOMPROOT, DIR_GF_DECOMPROOT);
    op.base.valid.insert(b"1".to_vec());
    assert_eq!(op.flags() & DIR_GF_DECOMPROOT, 0);
}
