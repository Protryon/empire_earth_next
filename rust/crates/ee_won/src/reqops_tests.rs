//! `golden_reqops.txt` replayed: each op made from the driver's members, its RunHook verdict and request, each
//! Reset and each reply's verdict and the op's members after it compared with what the original's statements did.

use super::*;
use crate::reqop::{ee_won_reqop_bytes, ee_won_reqop_free, ee_won_reqop_num, ee_won_reqop_run_hook};

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len() / 2).map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap()).collect()
}

/// The OP line: the kind and its members (the contest's first two are the auth type and the certificate's user
/// id, the ping's last two the ticks at its request and replies: the base's, handed in).
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
        Checked::Cut => "cut".into(),
    }
}

#[test]
fn golden() {
    let text = include_str!("golden_reqops.txt");
    let mut lines = text.lines().peekable();
    let (mut sequences, mut replies, mut resets, mut refused, mut datagrams) = (0, 0, 0, 0, 0);
    let mut kinds = std::collections::BTreeSet::new();
    while let Some(l) = lines.next() {
        let (kind, m) = members_of(l);
        kinds.insert(kind);
        let (auth, user, ticks, own) = match kind {
            EE_WON_REQOPS_CONTEST => (m[0].num as i32, m[1].num as u32, (0, 0), &m[2..]),
            EE_WON_REQOPS_PING => (0, 0, (m[2].num as u32, m[3].num as u32), &m[..2]),
            _ => (0, 0, (0, 0), &m[..]),
        };
        let mut op = new_op(kind, own).unwrap();
        let q = lines.next().unwrap();
        let (field_size, q) = match q.strip_prefix("Q@") {
            Some(r) => {
                let (n, r) = r.split_once(' ').unwrap();
                (n.parse().unwrap(), r)
            }
            None => (op.length_field_size(), q.strip_prefix("Q ").unwrap()),
        };
        sequences += 1;
        if let Some(s) = op.refuses(auth) {
            assert_eq!(format!("f{s}"), q, "{l}");
            refused += 1;
            continue;
        }
        op.run_hook();
        op.set_env(if op.needs_user_id() { user } else { 0 }, ticks.0);
        let (s, r) = op.next_request(field_size);
        assert_eq!(format!("{s} {}", hex(&r.unwrap())), q, "{l}");
        if field_size == 0 {
            datagrams += 1;
            continue;
        }
        while let Some(r) = lines.peek() {
            if let Some(want) = r.strip_prefix("X => ") {
                op.reset();
                assert_eq!(op.describe(), want, "{l}\nX");
                resets += 1;
                lines.next();
                continue;
            }
            let Some(r) = r.strip_prefix("R ") else { break };
            let (reply, want) = r.split_once(" => ").unwrap();
            op.set_env(0, ticks.1);
            let got = format!("{} {}", verdict(op.check_response(&unhex(reply))), op.describe());
            assert_eq!(got, want, "{l}\nR {reply}");
            lines.next();
            replies += 1;
        }
    }
    assert_eq!(sequences, 1200);
    assert_eq!(kinds.len(), 6);
    assert!(
        replies > 1000 && resets > 50 && refused > 50 && datagrams > 30,
        "{replies} {resets} {refused} {datagrams}"
    );
}

fn values(m: &[Member]) -> Vec<crate::admin::EeWonAdminValue> {
    m.iter().map(|v| crate::admin::EeWonAdminValue { num: v.num, data: v.bytes.as_ptr(), len: v.bytes.len() }).collect()
}

fn n(num: i64) -> Member {
    Member { num, bytes: Vec::new() }
}
fn b(bytes: &[u8]) -> Member {
    Member { num: 0, bytes: bytes.to_vec() }
}

unsafe extern "C" fn into_vec(c: *mut std::ffi::c_void, d: *const u8, n: usize) {
    let v = unsafe { &mut *(c as *mut Vec<u8>) };
    v.clear();
    v.extend_from_slice(unsafe { crate::ffi::bytes(d, n) });
}

#[test]
fn contest_through_the_abi() {
    let v = values(&[n(7), n(1), b(b"ab")]);
    let op = unsafe { ee_won_reqops_new(EE_WON_REQOPS_CONTEST, v.as_ptr(), v.len()) };
    assert!(!op.is_null());
    let o = unsafe { &mut (*op).op };
    assert_eq!((o.length_field_size(), o.use_auth2(), o.needs_user_id()), (2, false, true));
    let mut finish = 0;
    assert!(!unsafe { ee_won_reqop_run_hook(op, 0, &mut finish) });
    assert_eq!(finish, 100056);
    assert!(unsafe { ee_won_reqop_run_hook(op, 2, &mut finish) });
    let o = unsafe { &mut (*op).op };
    o.set_env(0x01020304, 0);
    let (s, r) = o.next_request(2);
    assert_eq!(s, ST_RECV);
    let r = r.unwrap();
    assert_eq!(&r[..9], [29, 0, 3, 4, 1, 4, 3, 2, 1]);
    assert_eq!(&r[9..11], [7, 0]);
    assert_eq!(&r[r.len() - 2..], b"ab");
    let mut got: Vec<u8> = Vec::new();
    // no reply yet: no reply data
    assert!(!unsafe { ee_won_reqop_bytes(op, EE_WON_REQOP_CONTEST_REPLY, into_vec, &mut got as *mut _ as *mut _) });
    let o = unsafe { &mut (*op).op };
    assert_eq!(o.check_response(&[3, 4, 2, 0xfe, 0xff, 9, 8]), Checked::Status(-2));
    assert!(unsafe { ee_won_reqop_bytes(op, EE_WON_REQOP_CONTEST_REPLY, into_vec, &mut got as *mut _ as *mut _) });
    assert_eq!(got, [9, 8]);
    let o = unsafe { &mut (*op).op };
    assert_eq!(o.check_response(&[3, 4, 1, 0, 0]), Checked::InvalidHeader);
    assert_eq!(o.check_response(&[3, 4]), Checked::Cut);
    assert_eq!(o.describe(), "x0908");
    unsafe { ee_won_reqop_free(op) };
    assert!(unsafe { ee_won_reqops_new(99, std::ptr::null(), 0) }.is_null());
}

#[test]
fn event_request_carries_the_parts() {
    let name = crate::text::utf16_to_le_bytes(&crate::text::from_utf8("s"));
    let m = [
        n(9),
        n(1),
        n(3),
        b(&name),
        b(b"1.2"),
        n(0),
        b(b""),
        b(b""),
        n(1),
        n(77),
        b(&name),
        n(1),
        n(5),
        n(0),
        n(42),
        b(b""),
        n(1),
        b(&name),
        n(2),
        n(0),
        b(b"zz"),
    ];
    let mut op = new_op(EE_WON_REQOPS_EVENT, &m).unwrap();
    let (_, r) = op.next_request(4);
    let want = db::event_request(
        4,
        &Event {
            activity_type: 9,
            server: Some((3, vec![u16::from(b's')], b"1.2")),
            client: None,
            user: Some((77, vec![u16::from(b's')])),
            details: vec![Detail { detail_type: 5, data_type: 0, number: 42, text: Vec::new() }],
            attachments: vec![Attachment { description: vec![u16::from(b's')], content_type: 2, data: None }],
        },
    );
    assert_eq!(r.unwrap(), want);
    assert_eq!(op.check_response(&[5, 4, 0, 2, 0, 3, 0]), Checked::Status(3));
    assert_eq!(op.check_response(&[5, 4, 0, 1, 0, 3, 0]), Checked::InvalidHeader);
    assert_eq!(op.check_response(&[5, 4, 0, 2, 0, 3]), Checked::Cut);
}

#[test]
fn patch_status_answers_its_sub_message() {
    let m = [b(b"p"), b(b"c"), b(b"1"), b(b"2"), b(b"u"), n(3)];
    let mut op = new_op(EE_WON_REQOPS_PATCH_STATUS, &m).unwrap();
    op.run_hook();
    let (_, r) = op.next_request(0);
    // no length field: the datagram
    assert_eq!(r.unwrap(), [5, 14, 0, 0xec, 3, 19, 0, 1, 0, b'p', 1, 0, b'c', 1, 0, b'1', 1, 0, b'2', 1, 0, b'u', 3]);
    let reply = |sub: u8| vec![5, 14, 0, 0xec, 3, sub, 0, 0, 0, 0, 0, 0x80, 7];
    assert_eq!(op.check_response(&reply(19)), Checked::InvalidHeader);
    assert_eq!(op.check_response(&reply(20)), Checked::Status(ST_SUCCESS));
    assert_eq!(op.bytes(EE_WON_REQOP_PROXY_REPLY), Some(vec![7]));
}

#[test]
fn ping_lag_reset_and_accessors() {
    let mut op = new_op(EE_WON_REQOPS_PING, &[n(4), n(1)]).unwrap();
    assert_eq!(op.length_field_size(), 4);
    op.set_env(0, 1000);
    let (_, r) = op.next_request(4);
    assert_eq!(r.unwrap(), [12, 0, 0, 0, 3, 1, 5, 0xe8, 3, 0, 0, 1]);
    op.set_env(0, 1250);
    let mut reply = vec![3, 1, 6, 0xe8, 3, 0, 0, 1, 0, b'a', 0, 0, 5, 0, 0, 0];
    assert_eq!(op.check_response(&reply), Checked::Cut);
    assert_eq!(op.describe(), "l250 a61 n- i- p- r- s- v5 d0 u0");
    reply.extend_from_slice(&[0, 0, 7, 0, 0, 0, 0, 0, 1]);
    assert_eq!(op.check_response(&reply), Checked::Status(ST_SUCCESS));
    assert_eq!((op.num(EE_WON_REQOP_PING_PID), op.num(EE_WON_REQOP_PING_IS_AUTH)), (7, 1));
    assert_eq!(op.bytes(EE_WON_REQOP_PING_TEXT), Some(b"a".to_vec()));
    assert_eq!(op.bytes(EE_WON_REQOP_PING_TEXT + 6), None);
    op.reset();
    // Reset keeps the lag
    assert_eq!(op.describe(), "l250 a- n- i- p- r- s- v0 d0 u0");
    let p = Box::into_raw(Box::new(EeWonReqOp { op }));
    assert_eq!(unsafe { ee_won_reqop_num(p, EE_WON_REQOP_PING_LAG) }, 250);
    unsafe { ee_won_reqop_free(p) };
    assert_eq!(op_kind_check(), 4);
}

#[test]
fn user_id_reads_the_first_long() {
    let name = crate::text::utf16_to_le_bytes(&crate::text::from_utf8("me"));
    let op = unsafe { ee_won_reqop_user_id(name.as_ptr(), name.len()) };
    let o = unsafe { &mut (*op).op };
    o.run_hook();
    let (_, r) = o.next_request(4);
    // 1002/16 and the name behind a 2-byte length
    assert_eq!(&r.unwrap()[4..], [5, 14, 0, 0xea, 3, 16, 0, 2, 0, b'm', 0, b'e', 0]);
    let reply = |sub: u8, data: &[u8]| {
        let mut v = vec![5, 14, 0, 0xea, 3, sub, 0, 0, 0, 0, 0, 0x80];
        v.extend_from_slice(data);
        v
    };
    assert_eq!(o.check_response(&reply(16, &[1, 0, 0, 0])), Checked::InvalidHeader);
    assert_eq!(o.check_response(&reply(17, &[1, 0])), Checked::Cut);
    assert_eq!(o.check_response(&reply(17, &[0x2a, 0, 0, 0])), Checked::Status(ST_SUCCESS));
    assert_eq!(unsafe { ee_won_reqop_num(op, EE_WON_REQOP_USER_ID) }, 42);
    unsafe { ee_won_reqop_free(op) };
}

#[test]
fn version_check_fills_the_patches_from_its_version() {
    let op = unsafe { ee_won_reqop_version_check(b"p".as_ptr(), 1, b"en".as_ptr(), 2, b"1.0".as_ptr(), 3) };
    let o = unsafe { &mut (*op).op };
    o.run_hook();
    let mut data = vec![3, 0, b'2', b'.', b'0', 1, 0, b'u', 1, 0];
    let mut record = vec![1, 0, b'd', 0, 0, 7, 0, 0, 0, 9, 0, 0, 0];
    record.extend_from_slice(&[0; 8]);
    record.push(1);
    data.extend_from_slice(&(record.len() as u16).to_le_bytes());
    data.extend_from_slice(&record);
    let reply = |status: i16, data: &[u8]| {
        let mut v = vec![5, 14, 0, 0xec, 3, 2, 0];
        v.extend_from_slice(&status.to_le_bytes());
        v.extend_from_slice(&[0, 0, 0x80]);
        v.extend_from_slice(data);
        v
    };
    // another error: returned before the reply data is read
    assert_eq!(o.check_response(&reply(-5, &data)), Checked::Status(-5));
    let d = o.describe();
    assert!(d.starts_with("t1004/2 e- dx0300322e30") && d.ends_with("01 r0 o- w-"), "{d}");
    assert_eq!(o.check_response(&reply(-2815, &data)), Checked::Status(-2815));
    let mut got: Vec<u8> = Vec::new();
    assert!(unsafe { ee_won_reqop_bytes(op, EE_WON_REQOP_VERSION_TO, into_vec, &mut got as *mut _ as *mut _) });
    assert_eq!(got, b"2.0");
    let o = unsafe { &mut (*op).op };
    // from 1.0 to 2.0, active, config en; the record's display name, checksum, size, must-visit
    assert!(o.describe().ends_with(" o322e30 w75 656e,312e30,322e30,64,-,7,9,-,-,-,-,1,1"), "{}", o.describe());
    unsafe { ee_won_reqop_free(op) };
}

fn op_kind_check() -> i32 {
    [EE_WON_REQOPS_CONTEST, EE_WON_REQOPS_EVENT, EE_WON_REQOPS_PATCH_STATUS, EE_WON_REQOPS_PING]
        .iter()
        .filter(|&&k| new_op(k, &[]).is_some())
        .count() as i32
}
