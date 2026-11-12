//! `golden_dirop.txt` replayed: each op made from the driver's members, its request, and each reply's verdict and the
//! op's address after it compared with what the original's statements did.

use super::*;

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len() / 2).map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap()).collect()
}

fn units(s: &str) -> Vec<u16> {
    unhex(s).chunks(2).map(|c| u16::from_le_bytes([c[0], c[1]])).collect()
}

fn buf(s: &str) -> Option<Vec<u8>> {
    if s == "n" {
        None
    } else {
        Some(unhex(&s[1..]))
    }
}

fn op_of(line: &str) -> DirChangeOp {
    let mut t = line.split(' ');
    assert_eq!(t.next(), Some("OP"));
    let mut next = || t.next().unwrap();
    let kind: i32 = next().parse().unwrap();
    let mut o = DirChangeOp::new(kind, next() == "1");
    o.flags = next().parse().unwrap();
    o.path = units(next());
    o.name = units(next());
    o.display_name = units(next());
    o.net_addr = buf(next());
    o.new_name = units(next());
    o.new_display_name = units(next());
    o.new_net_addr = buf(next());
    o.lifespan = next().parse().unwrap();
    o.is_unique = next() == "1";
    o.mode = next().parse().unwrap();
    for _ in 0..next().parse::<usize>().unwrap() {
        o.objects.push(DataObject { data_type: unhex(next()), data: buf(next()) });
    }
    for _ in 0..next().parse::<usize>().unwrap() {
        let offset = next().parse().unwrap();
        let insert = next() == "1";
        o.mods.push(DataObjectMod {
            offset,
            insert,
            object: DataObject { data_type: unhex(next()), data: buf(next()) },
        });
    }
    for _ in 0..next().parse::<usize>().unwrap() {
        o.types.push(unhex(next()));
    }
    o
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
    let text = include_str!("golden_dirop.txt");
    let mut lines = text.lines().peekable();
    let (mut sequences, mut replies) = (0, 0);
    let mut kinds = std::collections::BTreeSet::new();
    while let Some(l) = lines.next() {
        let mut o = op_of(l);
        kinds.insert(o.op);
        o.run_hook();
        let (s, r) = o.next_request(o.length_field_size());
        assert_eq!(format!("Q {s} {}", hex(&r.unwrap())), lines.next().unwrap(), "{l}");
        while let Some(r) = lines.peek() {
            let Some(r) = r.strip_prefix("R ") else { break };
            let (reply, want) = r.split_once(" => ").unwrap();
            let got = format!("{} {}", verdict(o.check_response(&unhex(reply))), o.describe());
            assert_eq!(got, want, "{l}\nR {reply}");
            lines.next();
            replies += 1;
        }
        sequences += 1;
    }
    assert_eq!(sequences, 1000);
    assert!(replies > 2000, "{replies}");
    assert_eq!(kinds.len(), 8);
}

#[test]
fn defaults_and_the_returned_address() {
    let o = DirChangeOp::new(op::ADD, true);
    assert_eq!((o.flags, o.lifespan, o.is_unique), (EE_WON_DIR_UF_UNIQUE_DISPLAY_NAME, 0, false));
    assert!(DirChangeOp::new(op::RENAME, false).is_unique);
    assert_eq!(DirChangeOp::new(op::REMOVE, true).flags, 0);
    let mut o = DirChangeOp { flags: EE_WON_DIR_UF_SERV_RETURN_ADDR, ..DirChangeOp::new(op::MODIFY, true) };
    // 5, 2, 1, status 0, a 2-byte address
    assert_eq!(o.check_response(&[5, 2, 0, 1, 0, 0, 0, 2, 9, 8]), Checked::Status(0));
    assert_eq!(o.net_addr.as_deref(), Some(&[9u8, 8][..]));
    assert_eq!(o.describe(), "a0908");
    // a directory never reads it; a cut reply throws; another header is the next server
    let mut d = DirChangeOp { flags: EE_WON_DIR_UF_SERV_RETURN_ADDR, ..DirChangeOp::new(op::ADD, false) };
    assert_eq!(d.check_response(&[5, 2, 0, 1, 0, 0xfe, 0xff]), Checked::Status(-2));
    assert_eq!(d.check_response(&[5, 2, 0, 1, 0]), Checked::Cut);
    assert_eq!(d.check_response(&[5, 2, 0, 2, 0, 0, 0]), Checked::InvalidHeader);
    assert_eq!(d.describe(), "a-n");
}
