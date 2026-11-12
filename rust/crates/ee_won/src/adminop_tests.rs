//! `golden_adminop.txt` replayed: each op made from the driver's members, its request, and each reply's verdict and
//! the op's members after it compared with what the original's statements did.

use super::*;
use crate::reqop::ST_RECV;

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len() / 2).map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap()).collect()
}

fn op_of(line: &str) -> AdminOp {
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
    AdminOp::new(kind, members)
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
    let text = include_str!("golden_adminop.txt");
    let mut lines = text.lines().peekable();
    let (mut sequences, mut replies) = (0, 0);
    let mut kinds = std::collections::BTreeSet::new();
    while let Some(l) = lines.next() {
        let mut op = op_of(l);
        kinds.insert(op.kind);
        op.run_hook();
        let (s, r) = op.next_request(4);
        let q = lines.next().unwrap();
        assert_eq!(format!("Q {s} {}", hex(&r.unwrap())), q, "{l}");
        while let Some(r) = lines.peek() {
            let Some(r) = r.strip_prefix("R ") else { break };
            let (reply, want) = r.split_once(" => ").unwrap();
            let got = format!("{} {}", verdict(op.check_response(&unhex(reply))), op.describe());
            assert_eq!(got, want, "{l}\nR {reply}");
            lines.next();
            replies += 1;
        }
        sequences += 1;
    }
    assert_eq!(sequences, 1200);
    assert!(replies > 1500, "{replies}");
    assert_eq!(kinds.len(), 27);
}

#[test]
fn defaults() {
    let mut o = AdminOp::new(EE_WON_ADMINOP_GET_VERSION_LIST, Vec::new());
    o.run_hook();
    assert_eq!((o.proxy.message, o.proxy.sub), (1004, 9));
    // product "", config "%", the state filter BadState
    assert_eq!(o.proxy.request_data.as_deref(), Some(&[0, 0, 1, 0, b'%', 3][..]));
    let mut o = AdminOp::new(EE_WON_ADMINOP_MODULE_LIST, Vec::new());
    o.run_hook();
    assert_eq!((o.proxy.message, o.proxy.sub, o.proxy.request_data.is_none()), (6, 1, true));
    let mut o = AdminOp::new(EE_WON_ADMINOP_PATCH_CLEANUP, Vec::new());
    o.run_hook();
    assert_eq!(o.proxy.request_data.as_deref(), Some(&[][..]));
    let mut o = AdminOp::new(EE_WON_ADMINOP_INSERT_MASTER_KEY, Vec::new());
    o.run_hook();
    // no key bytes (length 0), the flags 1
    assert_eq!(o.proxy.request_data.unwrap(), [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0]);
}

#[test]
fn module_list_and_replies_over_several_messages() {
    let mut o = AdminOp::new(EE_WON_ADMINOP_MODULE_LIST, Vec::new());
    o.run_hook();
    let head = |flags: u8, message: i16| {
        let mut v = vec![5, 14, 0];
        v.extend_from_slice(&message.to_le_bytes());
        v.extend_from_slice(&[2, 0, 0, 0, 0, 0, flags]);
        v
    };
    let mut a = head(0, 7);
    a.extend_from_slice(&[2, 0, 6, 0]);
    assert_eq!(o.check_response(&a), Checked::Status(ST_RECV));
    let mut b = head(0x80, 7);
    b.extend_from_slice(&[4, 0, 0x10, 0, 0, 0]);
    assert_eq!(o.check_response(&b), Checked::Status(ST_SUCCESS));
    assert_eq!(o.out.modules, [6 | 4 << 16, 0x10]);
    assert!(is_module_available(&o.out.modules, 6, 4));
    assert!(!is_module_available(&o.out.modules, 4, 6));
    assert_eq!((action_name(0), action_name(1), action_name(7)), ("RESET", "BAN", "Unknown Action"));
    // the wrong message type: the next server
    assert_eq!(o.check_response(&head(0x80, 6)), Checked::InvalidHeader);
}
