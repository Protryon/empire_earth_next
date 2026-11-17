//! `golden_reqlast.txt` replayed: each op made from the driver's members (the create account's generators from
//! the driver's fixed seeds), its requests (after a Send, and a second server's after a `Reset`), each reply's
//! verdict and the op's members after it, compared with what the original's statements did. The group list's map
//! (the C++ records the driver prints after the members) is the driver's check; here its flags and reply count.
//! Then the steps one by one.

use super::*;
use crate::blowfish::Blowfish;

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

/// the driver's dump without the group map (its " m<count> ..." tail)
fn members_part(kind: i32, dump: &str) -> &str {
    if kind == EE_WON_REQOPS_GROUP_LIST {
        dump.split(" m").next().unwrap()
    } else {
        dump
    }
}

#[test]
fn golden() {
    let text = include_str!("golden_reqlast.txt");
    let (mut sequences, mut requests, mut replies, mut resets) = (0i64, 0, 0, 0);
    let (mut keys, mut sessions, mut accounts, mut bad_keys, mut groups) = (0, 0, 0, 0, 0);
    let mut op: Option<(i32, Box<dyn RequestOp>)> = None;
    for l in text.lines() {
        if l.starts_with("OP ") {
            let (kind, m) = members_of(l);
            let o: Box<dyn RequestOp> = if kind == EE_WON_REQOPS_CREATE_ACCOUNT {
                // the driver's seed: 1000 + the sequence's index, set again before the op is made
                Box::new(create_account(&mut Members { m: &m, at: 0 }, Some((1000 + sequences, 1))))
            } else {
                groups += 1;
                new_op(kind, &m).unwrap()
            };
            op = Some((kind, o));
            sequences += 1;
            continue;
        }
        let (kind, o) = op.as_mut().unwrap();
        if let Some(q) = l.strip_prefix("Q ") {
            let (s, r) = o.next_request(o.length_field_size());
            assert_eq!(format!("{s} {}", hex(&r.unwrap())), q, "Q {q}");
            requests += 1;
            if q.starts_with("100058 ") {
                sessions += 1;
            }
        } else if let Some(r) = l.strip_prefix("R ") {
            let (reply, want) = r.split_once(" => ").unwrap();
            let got = format!("{} {}", verdict(o.check_response(&unhex(reply))), o.describe());
            assert_eq!(got, members_part(*kind, want), "R {reply}");
            replies += 1;
            if want.starts_with("100058 s1") {
                keys += 1;
            }
            if want.starts_with("err100011") {
                bad_keys += 1;
            }
            if *kind == EE_WON_REQOPS_CREATE_ACCOUNT && want.starts_with("0 s2") {
                accounts += 1;
            }
        } else if let Some(want) = l.strip_prefix("X => ") {
            o.reset();
            assert_eq!(o.describe(), members_part(*kind, want));
            resets += 1;
        } else {
            panic!("{l}");
        }
    }
    assert!(sequences >= 1000 && groups > 200, "{sequences} {groups}");
    assert!(requests > sequences && replies > 2 * sequences && resets > 100, "{requests} {replies} {resets}");
    // the paths the replies reach: a loaded server key, its session key sent, an account created, a bad key
    assert!(keys > 50 && sessions > 50 && accounts > 5 && bad_keys > 20, "{keys} {sessions} {accounts} {bad_keys}");
}

// ---- the steps

/// won_crypto_test's 512-bit test key
const PRIVATE_KEY: &str = "3081f5024100c37d6f28fb6c0e9aa489f1d80d5a41952bb7d31ebeb9ca0031dcf9a0c134e9395d5e21986c08c9491a49c334a82d7fff2603de67ab87a6801de67f3c3dec7211021500eeac675c1162f5b42138f37567274df60cbb1485024100abbb86ba89355c5c884dfc2f9c1e6692390d87ef1220f9eb98b763cc3631c652f83640f980949efa8b80256910f55e629ce126227b13b856bad2720bd64c60360240228d68730c33fa487c1f0c0154e0dec1f064072e366b35b75439c4f08e3a0fe4fd52a3d183480fc30601fe4ed2739518140e9196a72f1bce2e908e04c732843c02145c37feebabe4399fe2f449a19ced631c5254d013";

/// a DB reply: header 5, service 14, the message and sub, the status, an error string, the flags, the rest
fn db_reply(message: i16, sub: i16, status: i16, error: &[u8], rest: &[u8]) -> Vec<u8> {
    let mut w = Writer::new(0, true);
    w.append_byte(5);
    w.append_short(14);
    w.append_short(message);
    w.append_short(sub);
    w.append_short(status);
    w.append_string(error, 2);
    w.append_byte(0x80);
    w.append_bytes(rest);
    w.data().to_vec()
}

fn key_pair() -> (ElGamal, Vec<u8>) {
    let mut private = ElGamal::with_random(Random::from_seed(b"k"));
    assert!(private.set_private_key(&unhex(PRIVATE_KEY)));
    let public = private.raw_public_key().unwrap();
    (private, public)
}

fn an_op() -> CreateAccountOp {
    let mut p = EeWonProfile::default();
    p.newsletters.insert(1, true);
    p.birth_month = 7;
    CreateAccountOp::with_random(
        "Max".encode_utf16().collect(),
        "pw".encode_utf16().collect(),
        b"m@x".to_vec(),
        p,
        Random::from_seed(b"a"),
        Random::from_seed(b"b"),
    )
}

#[test]
fn the_account_goes_encrypted_under_the_session_key_sent_under_the_servers_key() {
    let (private, public) = key_pair();
    let mut op = an_op();
    let (s, q) = op.next_request(4);
    assert_eq!((s, q.unwrap()), (ST_RECV, db::create_key_request(4)));
    let mut rest = (public.len() as i16).to_le_bytes().to_vec();
    rest.extend_from_slice(&public);
    assert_eq!(op.check_response(&db_reply(3, 1, 0, b"", &rest)), Checked::Status(crate::server::st::SEND));
    // the session key, encrypted with the server's key, sent without waiting for a reply
    let (s, q) = op.next_request(4);
    assert_eq!(s, crate::server::st::SEND);
    let q = q.unwrap();
    let encrypted = &q[4 + 7 + 2..];
    let key = private.decrypt(encrypted).unwrap();
    assert_eq!(key, op.sym_key.key());
    assert_eq!(key.len(), 8);
    // the account data, encrypted with the session key
    let (s, q) = op.next_request(4);
    assert_eq!(s, ST_RECV);
    let q = q.unwrap();
    let mut b = Blowfish::default();
    assert!(b.set_key(&key));
    let plain = b.decrypt(&q[4 + 7..]).unwrap();
    assert_eq!(plain, db::create_account_data(&op.user, &op.password, &op.email, &op.profile));
    // the server's verdict, its error string kept
    assert_eq!(op.check_response(&db_reply(1002, 3, -5, b"taken", &[])), Checked::Status(-5));
    assert_eq!(op.bytes(EE_WON_REQOP_CREATE_ERROR).unwrap(), b"taken");
    // the next server starts over at its key
    op.reset();
    assert_eq!(op.next_request(4).1.unwrap(), db::create_key_request(4));
}

#[test]
fn a_bad_server_key_is_a_server_error_and_the_next_server() {
    let mut op = an_op();
    op.next_request(4);
    let rest = [3u8, 0, 1, 2, 3];
    assert_eq!(op.check_response(&db_reply(3, 1, 0, b"", &rest)), Checked::ServerError(ST_INVALID_PUBLIC_KEY));
    assert_eq!(op.state, State::GettingPubKey);
    assert_eq!(op.check_response(&db_reply(9, 1, 0, b"", &[])), Checked::InvalidHeader);
    assert_eq!(op.check_response(&db_reply(3, 1, 0, b"e", &[9])), Checked::Cut);
    assert_eq!(op.bytes(EE_WON_REQOP_CREATE_ERROR).unwrap(), b"e");
}

#[test]
fn the_session_keys_follow_the_generators() {
    let a = an_op();
    let b = an_op();
    assert_eq!(a.sym_key.key(), b.sym_key.key());
    let mut r = Random::from_seed(b"b");
    let mut want = [0u8; 8];
    r.next_bytes(&mut want);
    assert_eq!(a.sym_key.key(), &want);
}

/// a group list request's reply: the header, the status, the flags and the groups (id and name)
fn group_reply(status: i16, flags: i16, groups: &[(u16, &str)]) -> Vec<u8> {
    let mut w = Writer::new(0, true);
    w.append_byte(3);
    w.append_byte(7);
    w.append_byte(25);
    w.append_short(status);
    if status == 0 {
        w.append_short(flags);
        w.append_short(groups.len() as i16);
        for &(id, name) in groups {
            let n: Vec<u16> = name.encode_utf16().collect();
            w.append_short((4 + 2 * n.len()) as i16);
            w.append_short(id as i16);
            w.append_wstring(&n, 2);
        }
    }
    w.data().to_vec()
}

#[test]
fn the_group_list_keeps_its_replies_and_takes_the_replys_flags() {
    let mut op = GroupListReqOp { flags: 0x29, replies: Vec::new() };
    let (s, q) = op.next_request(2);
    assert_eq!(s, ST_RECV);
    assert_eq!(q.unwrap(), vec![7, 0, 3, 7, 26, 0x29, 0]);
    let a = group_reply(0, 1, &[(4, "Lobby"), (9, "Chat")]);
    assert_eq!(op.check_response(&a), Checked::Status(0));
    // the reply's flags are the inner op's now: the next request sends them
    assert_eq!(op.flags, 1);
    assert_eq!(op.next_request(2).1.unwrap()[5], 1);
    let b = group_reply(-3, 0, &[]);
    assert_eq!(op.check_response(&b), Checked::Status(-3));
    assert_eq!(op.check_response(&[3, 7, 24]), Checked::InvalidHeader);
    // the list cut short: its own read's failure (what it read stays); the header or status cut: the throw
    assert_eq!(op.check_response(&a[..9]), Checked::Status(rt::EE_WON_RT_UNPACK_FAILURE));
    assert_eq!(op.check_response(&a[..4]), Checked::Cut);
    assert_eq!(op.num(EE_WON_REQOP_GROUP_REPLIES), 5);
    assert_eq!(op.bytes_at(EE_WON_REQOP_GROUP_REPLY, 0).unwrap(), a);
    assert_eq!(op.bytes_at(EE_WON_REQOP_GROUP_REPLY, 1).unwrap(), b);
    assert_eq!(op.bytes_at(EE_WON_REQOP_GROUP_REPLY, 4).unwrap(), &a[..4]);
    assert_eq!(op.bytes_at(EE_WON_REQOP_GROUP_REPLY, 5), None);
    assert_eq!(op.bytes_at(EE_WON_REQOP_GROUP_REPLY, -1), None);
    assert_eq!(op.describe(), "f1 r5");
}

#[test]
fn the_members_make_the_ops() {
    let m = [Member { num: 0x29, bytes: Vec::new() }];
    let op = new_op(EE_WON_REQOPS_GROUP_LIST, &m).unwrap();
    assert_eq!(op.length_field_size(), 2);
    assert_eq!(op.describe(), "f41 r0");
    let w = |s: &str| Member { num: 0, bytes: s.encode_utf16().flat_map(|u| u.to_le_bytes()).collect() };
    let n = |v: i64| Member { num: v, bytes: Vec::new() };
    let m = [w("Max"), w("pw"), Member { num: 0, bytes: b"m@x".to_vec() }, n(1), n(1), n(1), n(7), n(4), n(1990)];
    let mut o = Members { m: &m, at: 0 };
    let op = create_account(&mut o, Some((5, 1)));
    assert_eq!(op.user, "Max".encode_utf16().collect::<Vec<_>>());
    assert_eq!(op.email, b"m@x");
    assert!(op.profile.newsletter(1));
    assert_eq!(op.profile.birth_date(), 7 | 4 << 8 | 1990 << 16);
    assert_eq!(op.profile.custom_action_data, None);
    assert_eq!(new_op(EE_WON_REQOPS_CREATE_ACCOUNT, &m).unwrap().length_field_size(), 4);
    assert!(new_op(99, &m).is_none());
}
