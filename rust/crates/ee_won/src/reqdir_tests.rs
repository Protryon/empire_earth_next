//! `golden_reqdir.txt` replayed: each op made from the driver's members, its requests (a second one after a
//! `Reset`, the next server's), each reply's verdict and the op's results after it, compared with what the
//! original's statements did; and the results through the base's accessors.

use super::*;
use crate::dir::read;

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
    }
}

#[test]
fn golden() {
    let text = include_str!("golden_reqdir.txt");
    let mut lines = text.lines().peekable();
    let (mut sequences, mut requests, mut replies, mut resets) = (0, 0, 0, 0);
    let mut kinds = BTreeSet::new();
    let mut entities = 0;
    while let Some(l) = lines.next() {
        let (kind, m) = members_of(l);
        kinds.insert(kind);
        let mut op = new_op(kind, &m).unwrap();
        sequences += 1;
        while let Some(q) = lines.peek().and_then(|q| q.strip_prefix("Q ")) {
            let (s, r) = op.next_request(op.length_field_size());
            assert_eq!(format!("{s} {}", hex(&r.unwrap())), q, "{l}");
            lines.next();
            requests += 1;
            while let Some(r) = lines.peek().and_then(|r| r.strip_prefix("R ")) {
                let (reply, want) = r.split_once(" => ").unwrap();
                let got = format!("{} {}", verdict(op.check_response(&unhex(reply))), op.describe());
                assert_eq!(got, want, "{l}\nR {reply}");
                entities += want.matches("{type").count();
                lines.next();
                replies += 1;
            }
            if let Some(want) = lines.peek().and_then(|x| x.strip_prefix("X => ")) {
                op.reset();
                assert_eq!(op.describe(), want, "{l}\nX");
                resets += 1;
                lines.next();
            }
        }
    }
    assert_eq!(sequences, 1000);
    assert_eq!(kinds.len(), 7);
    assert!(
        requests > 1200 && replies > 2000 && resets > 200 && entities > 1000,
        "{requests} {replies} {resets} {entities}"
    );
}

fn n(num: i64) -> Member {
    Member { num, bytes: Vec::new() }
}
fn b(bytes: &[u8]) -> Member {
    Member { num: 0, bytes: bytes.to_vec() }
}
fn w(t: &str) -> Member {
    b(&t.encode_utf16().flat_map(u16::to_le_bytes).collect::<Vec<u8>>())
}

/// A multi-entity reply's head (the status, the sequence byte, the get flags, the entity count).
fn multi_head(status: i16, sequence: u8, flags: u32, count: u16) -> Writer {
    let mut m = Writer::new(0, true);
    m.append_byte(5);
    m.append_short(2);
    m.append_short(3);
    m.append_short(status);
    m.append_byte(sequence);
    m.append_long(flags as i32);
    m.append_short(count as i16);
    m
}

#[test]
fn a_multi_directory_listing_counts_its_failures_and_hands_out_each_list() {
    // two queries: the first's replies come in two parts, the second fails
    let m = [n(2), n(0x0400_0002), w("/TitanServers/Auth"), n(0), n(0x0400_0002), w("/Patch"), n(1), b(b"_C")];
    let mut op = new_op(EE_WON_REQOPS_DIR_MULTI, &m).unwrap();
    let (s, r) = op.next_request(4);
    assert_eq!(s, ST_RECV);
    assert!(r.unwrap().len() > 20);
    // services only (no 'D'/'S' type flag): each one's path is not asked for, the address is
    let flags = 0x0400_0000;
    let mut first = multi_head(0, 0, flags, 1);
    first.append_byte(6);
    first.append_bytes(&[10, 0, 0, 1, 0x1f, 0x90]);
    assert_eq!(op.check_response(first.data()), Checked::Status(ST_RECV));
    let mut last = multi_head(0, 0x80, flags, 1);
    last.append_byte(6);
    last.append_bytes(&[10, 0, 0, 2, 0x1f, 0x90]);
    assert_eq!(op.check_response(last.data()), Checked::Status(ST_RECV));
    let failed = multi_head(-5, 0x80, 0, 0);
    assert_eq!(op.check_response(&failed.data()[..7]), Checked::Status(-1223));
    assert_eq!(op.num_at(EE_WON_REQOP_DIR_LISTS, 0), 2);
    assert_eq!((op.num_at(EE_WON_REQOP_DIR_STATUS, 0), op.num_at(EE_WON_REQOP_DIR_STATUS, 1)), (0, -5));
    let l = op.entities(EE_WON_REQOP_DIR_LIST, 0);
    assert_eq!(l.len(), 2);
    assert_eq!(l[1].net_addr, [10, 0, 0, 2, 0x1f, 0x90]);
    assert!(l[1].fields & read::NET_ADDR != 0);
    assert!(op.entities(EE_WON_REQOP_DIR_LIST, 1).is_empty());
    assert!(op.entities(EE_WON_REQOP_DIR_LIST, 2).is_empty());
    // the next server: the lists go, the statuses and the failure count stay
    op.reset();
    assert!(op.entities(EE_WON_REQOP_DIR_LIST, 0).is_empty());
    assert_eq!(op.describe(), "c2 f1 s0[] s-5[]");
}

#[test]
fn a_query_never_answered_reads_as_ws_none_and_a_cut_keeps_what_was_read() {
    let mut op = new_op(EE_WON_REQOPS_DIR_GET, &[n(0x0400_0000), w("/EE"), n(0)]).unwrap();
    assert_eq!(op.num_at(EE_WON_REQOP_DIR_STATUS, 0), ST_NONE as i64);
    let mut r = multi_head(0, 0x80, 0x0400_0000, 2);
    r.append_byte(1);
    r.append_byte(7);
    r.append_byte(6);
    assert_eq!(op.check_response(&r.data()[..r.data().len() - 1]), Checked::Cut);
    assert_eq!(op.entities(EE_WON_REQOP_DIR_LIST, 0).len(), 1);
    assert_eq!(op.num_at(EE_WON_REQOP_DIR_STATUS, 0), ST_NONE as i64);
    assert_eq!(op.check_response(&[6, 2, 0, 3, 0, 0, 0]), Checked::InvalidHeader);
}

#[test]
fn a_service_lookup_keeps_the_service_until_reset() {
    let mut op =
        new_op(EE_WON_REQOPS_DIR_SERVICE, &[n(0x0600_0822), w("/GameSpy"), w("ee"), n(1), b(b"ee"), n(0)]).unwrap();
    let mut r = Writer::new(0, true);
    for v in [5i16, 2, 2, 0] {
        if v == 5 {
            r.append_byte(5);
        } else {
            r.append_short(v);
        }
    }
    r.append_long(0x0200_0000);
    r.append_wstring(&"svc".encode_utf16().collect::<Vec<u16>>(), 2);
    assert_eq!(op.check_response(r.data()), Checked::Status(0));
    let s = op.entities(EE_WON_REQOP_DIR_SERVICE, 0);
    assert_eq!(s.len(), 1);
    assert_eq!(String::from_utf16(&s[0].name).unwrap(), "svc");
    op.reset();
    assert!(op.entities(EE_WON_REQOP_DIR_SERVICE, 0).is_empty());
}

#[test]
fn the_account_profile_is_set_and_read_after_the_op_was_made() {
    let mut op = new_op(EE_WON_REQOPS_ACCOUNT_GET, &[n(1), n(1), n(0), n(0), n(0), n(0), n(0), n(0), b(b"")]).unwrap();
    assert_eq!(op.num_at(EE_WON_REQOP_ACCOUNT_NEWSLETTER, 1), 0);
    op.set(EE_WON_REQOP_ACCOUNT_NEWSLETTER, 1 | 0x1_0000, &[]);
    assert_eq!(op.num_at(EE_WON_REQOP_ACCOUNT_NEWSLETTER, 1), 1);
    op.set(EE_WON_REQOP_ACCOUNT_CLEAR_NEWSLETTERS, 0, &[]);
    assert_eq!(op.describe(), "x- e- p01000300040000000000");
    // a reply: the email and a newsletter subscription
    let mut r = Writer::new(0, true);
    r.append_byte(5);
    r.append_short(14);
    r.append_short(1002);
    r.append_short(7);
    r.append_short(0);
    r.append_short(0);
    r.append_byte(0x80);
    r.append_byte(3);
    r.append_bytes(b"a@b");
    r.append_short(1);
    r.append_short(1);
    r.append_short(3);
    r.append_short(1);
    r.append_bool(true);
    assert_eq!(op.check_response(r.data()), Checked::Status(0));
    assert_eq!(op.bytes(EE_WON_REQOP_ACCOUNT_EMAIL).unwrap(), b"a@b");
    assert_eq!(op.num_at(EE_WON_REQOP_ACCOUNT_NEWSLETTER, 1), 1);
    assert_eq!(op.bytes(EE_WON_REQOP_ACCOUNT_ERROR).unwrap(), b"");
}

#[test]
fn the_entities_reach_the_cpp_through_the_base() {
    use crate::hostop::{ee_won_hostop_entities, ee_won_hostop_free, ee_won_hostop_num, ee_won_hostop_set};
    use crate::reqhost::ee_won_hostop_request;
    let mut c =
        new_op(EE_WON_REQOPS_DIR_FIND, &[n(1), n(0x0400_0000), n(0), n(1), w(""), w(""), w(""), n(0), b(b"")]).unwrap();
    let mut r = multi_head(0, 0x80, 0x0400_0000, 1);
    r.append_byte(2);
    r.append_bytes(&[1, 2]);
    assert_eq!(c.check_response(r.data()), Checked::Status(0));
    let op = unsafe { ee_won_hostop_request(crate::reqop::EeWonReqOp::new(c)) };
    unsafe extern "C" fn count(ctx: *mut std::ffi::c_void, e: *const crate::dir::EeWonDirEntity) {
        let n = unsafe { &mut *(ctx as *mut Vec<usize>) };
        n.push(unsafe { (*e).net_addr_len });
    }
    let mut got: Vec<usize> = Vec::new();
    let k = unsafe { ee_won_hostop_entities(op, EE_WON_REQOP_DIR_SERVICE, 0, count, &mut got as *mut _ as *mut _) };
    assert_eq!((k, got.as_slice()), (1, &[2usize][..]));
    let k = unsafe { ee_won_hostop_entities(op, EE_WON_REQOP_DIR_LIST, 0, count, &mut got as *mut _ as *mut _) };
    assert_eq!(k, 1);
    // a number through the base: a list's status is the find's own; the newsletter setter of another op is ignored
    assert_eq!(unsafe { ee_won_hostop_num(op, EE_WON_REQOP_DIR_STATUS, 0) }, 0);
    unsafe {
        ee_won_hostop_set(
            op,
            EE_WON_REQOP_ACCOUNT_NEWSLETTER,
            1,
            std::ptr::null(),
            0,
            std::ptr::null(),
            std::ptr::null_mut(),
        )
    };
    unsafe { ee_won_hostop_free(op) };
}
