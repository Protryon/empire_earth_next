use super::*;

/// A deterministic environment: "public key" encryption is a reversible byte map, so the server's encryption is
/// the client's decryption and the reverse; the secrets are real Blowfish keys.
#[derive(Default)]
struct Mock {
    secrets: [Vec<u8>; 2],
    next_key: u8,
    cert: Option<Vec<u8>>,
    cert_valid: bool,
    verify: bool,
    expired: bool,
    fail_decrypt: bool,
    fail_encrypt: bool,
    own2: bool,
    sessions: Vec<(i32, u16, u8, u8)>,
    log: Vec<String>,
}

fn scramble(d: &[u8]) -> Vec<u8> {
    let mut v: Vec<u8> = d.iter().map(|b| b ^ 0x5a).collect();
    v.push(0xee);
    v
}

fn unscramble(d: &[u8]) -> Option<Vec<u8>> {
    match d.split_last() {
        Some((&0xee, rest)) => Some(rest.iter().map(|b| b ^ 0x5a).collect()),
        _ => None,
    }
}

impl PeerEnv for Mock {
    fn private_decrypt(&mut self, data: &[u8]) -> Option<Vec<u8>> {
        self.log.push("decrypt".into());
        if self.fail_decrypt {
            None
        } else {
            unscramble(data)
        }
    }
    fn make_cert(&mut self, auth2: bool, data: &[u8]) -> bool {
        self.log.push(format!("cert{}", u8::from(auth2)));
        self.cert = Some(data.to_vec());
        self.cert_valid && !data.is_empty()
    }
    fn verify_cert(&mut self) -> bool {
        self.verify
    }
    fn cert_expired(&mut self) -> bool {
        self.expired
    }
    fn cert_encrypt(&mut self, data: &[u8]) -> Option<Vec<u8>> {
        if self.fail_encrypt {
            None
        } else {
            Some(scramble(data))
        }
    }
    fn has_own_cert2(&mut self) -> bool {
        self.own2
    }
    fn own_cert_raw(&mut self, auth2: bool) -> Vec<u8> {
        if auth2 {
            b"CERT2".to_vec()
        } else {
            b"CERT1".to_vec()
        }
    }
    fn secret_create(&mut self, which: u8, length: i32) {
        self.next_key = self.next_key.wrapping_add(17);
        self.secrets[usize::from(which)] = (0..length as u8).map(|i| i.wrapping_mul(31) ^ self.next_key).collect();
        self.log.push(format!("create{which}"));
    }
    fn secret_set_key(&mut self, which: u8, key: &[u8]) -> bool {
        if !(5..=56).contains(&key.len()) {
            return false;
        }
        self.secrets[usize::from(which)] = key.to_vec();
        true
    }
    fn secret_key(&mut self, which: u8) -> Vec<u8> {
        self.secrets[usize::from(which)].clone()
    }
    fn new_session(&mut self, auth_type: i32, id: u16, which: u8, lfs: u8) {
        self.sessions.push((auth_type, id, which, lfs));
    }
}

fn ok_env() -> Mock {
    Mock { cert_valid: true, verify: true, own2: true, ..Default::default() }
}

/// The body of a message written with a 4-byte length field that counts itself.
fn body(m: &[u8]) -> &[u8] {
    assert_eq!(u32::from_le_bytes(m[..4].try_into().unwrap()) as usize, m.len());
    &m[4..]
}

#[test]
fn a_whole_handshake() {
    let mut cs = EeWonPeerState { use_auth2: true, client_key_size: 8, ..Default::default() };
    let mut ss = EeWonPeerState { use_auth2: true, ..Default::default() };
    let (mut ce, mut se) = (ok_env(), ok_env());
    let req = client_start(&mut cs, &mut ce, AUTH_TYPE_PERSISTENT, 4);
    assert_eq!(cs.state, 1);
    // 203, 50, persistent, encrypted, flags 1, the certificate
    assert_eq!(&body(&req)[..12], &[203, 0, 0, 0, 50, 0, 0, 0, 1, 1, 1, 0]);
    assert_eq!(&body(&req)[12..], &[5, 0, b'C', b'E', b'R', b'T', b'2']);
    server_start(&mut ss, 4);
    let (st, c1) = server_recv(&mut ss, &mut se, body(&req));
    assert_eq!((st, ss.state, ss.auth_type), (0, 2, AUTH_TYPE_PERSISTENT));
    assert_eq!(se.cert.as_deref(), Some(&b"CERT2"[..]));
    let (st, c2) = client_recv(&mut cs, &mut ce, body(&c1.unwrap()));
    assert_eq!((st, cs.state), (0, 2));
    assert_eq!(ce.secrets[1], se.secrets[1]);
    let (st, done) = server_recv(&mut ss, &mut se, body(&c2.unwrap()));
    assert_eq!((st, ss.state), (0, 0));
    assert_eq!(se.secrets[0], ce.secrets[0]);
    assert_eq!(se.sessions, vec![(AUTH_TYPE_PERSISTENT, 0, SECRET_B, 4)]);
    let (st, out) = client_recv(&mut cs, &mut ce, body(&done.unwrap()));
    assert_eq!((st, out), (0, None));
    assert_eq!(ce.sessions, vec![(AUTH_TYPE_PERSISTENT, 0, SECRET_B, 4)]);
}

#[test]
fn the_client_falls_back_to_version_1() {
    let mut cs = EeWonPeerState { use_auth2: true, use_client_secret: true, ..Default::default() };
    let mut e = Mock { own2: false, ..ok_env() };
    let req = client_start(&mut cs, &mut e, AUTH_TYPE_SESSION, 2);
    assert!(!cs.use_auth2);
    // a 2-byte length counting itself; sessioned 2, encrypted, flags 0x8000
    assert_eq!(&req[..2], &(req.len() as u16).to_le_bytes());
    assert_eq!(&req[10..14], &[2, 1, 0, 0x80]);
    assert_eq!(&req[16..], b"CERT1");
}

#[test]
fn server_failures_send_a_complete_with_the_name() {
    let mut ss = EeWonPeerState { use_auth2: true, ..Default::default() };
    let mut e = ok_env();
    assert_eq!(server_recv(&mut ss, &mut e, &[]), (ws::SERVER_NOT_STARTED, None));
    server_start(&mut ss, 4);
    // a header cut short: unpack failure, a complete with the status's name
    let (st, out) = server_recv(&mut ss, &mut e, &[203, 0, 0]);
    assert_eq!(st, ws::SERVER_MSG_UNPACK_FAILURE);
    let out = out.unwrap();
    assert_eq!(&body(&out)[8..12], &[0xfc, 0xff, 1, 0]);
    assert!(out.ends_with(b"WS_PeerAuthServer_MsgUnpackFailure"));
    assert_eq!(ss.state, 0);
    // a wrong service: no complete
    server_start(&mut ss, 4);
    assert_eq!(server_recv(&mut ss, &mut e, &[1, 0, 0, 0, 50, 0, 0, 0]), (ws::SERVER_INVALID_SERVICE_TYPE, None));
    // a bad auth mode: the state advanced, then the complete reset it
    let (st, out) = server_recv(&mut ss, &mut e, &[203, 0, 0, 0, 50, 0, 0, 0, 2, 1, 0, 0]);
    assert_eq!((st, ss.state, out.is_some()), (ws::SERVER_INVALID_AUTH_MODE, 0, true));
    // an expired certificate
    server_start(&mut ss, 4);
    e.expired = true;
    let (st, _) = server_recv(&mut ss, &mut e, &[203, 0, 0, 0, 50, 0, 0, 0, 1, 0, 0, 0, 1, 0, 7]);
    assert_eq!((st, ss.auth_type), (ws::SERVER_EXPIRED_CLIENT_CERTIFICATE, AUTH_TYPE_PERSISTENT_NOCRYPT));
}

#[test]
fn client_checks() {
    let mut cs = EeWonPeerState { use_auth2: true, client_key_size: 8, ..Default::default() };
    let mut e = ok_env();
    assert_eq!(client_recv(&mut cs, &mut e, &[9, 0, 0, 0, 51, 0, 0, 0]).0, ws::CLIENT_INVALID_SERVICE_TYPE);
    assert_eq!(client_recv(&mut cs, &mut e, &[203, 0, 0, 0, 51, 0, 0, 0]).0, ws::CLIENT_UNEXPECTED_CHALLENGE);
    assert_eq!(client_recv(&mut cs, &mut e, &[203, 0, 0, 0, 54, 0, 0, 0]).0, ws::CLIENT_INVALID_MESSAGE);
    assert_eq!(client_recv(&mut cs, &mut e, &[203, 0, 0]).0, ws::CLIENT_MSG_UNPACK_FAILURE);
    // a failed complete: the status and its error texts read
    let m = [203, 0, 0, 0, 53, 0, 0, 0, 0xfc, 0xff, 1, 0, 2, 0, b'h', b'i'];
    assert_eq!(client_recv(&mut cs, &mut e, &m), (-4, None));
    assert_eq!(client_recv(&mut cs, &mut e, &m[..15]).0, ws::CLIENT_MSG_UNPACK_FAILURE);
    // a challenge whose secret is too short for its length: secret A not created
    cs.state = 1;
    let mut m = vec![203, 0, 0, 0, 51, 0, 0, 0];
    let enc = scramble(&[9, 0, 1, 2]);
    m.extend_from_slice(&(enc.len() as u16).to_le_bytes());
    m.extend_from_slice(&enc);
    assert_eq!(client_recv(&mut cs, &mut e, &m).0, ws::CLIENT_CHALLENGE1_INVALID_SECRET_LEN);
    assert!(e.secrets[0].is_empty());
    // the port's definition: a decrypted secret under 2 bytes
    let mut m = vec![203, 0, 0, 0, 51, 0, 0, 0];
    let enc = scramble(&[9]);
    m.extend_from_slice(&(enc.len() as u16).to_le_bytes());
    m.extend_from_slice(&enc);
    assert_eq!(client_recv(&mut cs, &mut e, &m).0, ws::CLIENT_CHALLENGE1_INVALID_SECRET_LEN);
}

fn key(k: &[u8]) -> Blowfish {
    let mut b = Blowfish::new();
    assert!(b.set_key(k));
    b
}

#[test]
fn sessions_frame_and_count() {
    let k = key(b"secret12");
    let mut a = EeWonAuthSession::new(AUTH_TYPE_SESSION, 0x1234, k, 4, 10);
    let mut b = EeWonAuthSession::new(AUTH_TYPE_SESSION, 0x1234, k, 4, 10);
    let mut w = Writer::new(4, true);
    w.append_bytes(b"\x05hello world");
    let m = w.to_message();
    for n in 1..4u16 {
        let (st, enc) = a.encrypt(&m, 20);
        assert_eq!((st, a.out_seq, a.last_use), (0, n, 20));
        let enc = enc.unwrap();
        assert_eq!(&enc[4..7], &[12, 0x34, 0x12]);
        let (st, dec) = b.decrypt(&enc[4..], 30);
        assert_eq!((st, b.in_seq), (0, n));
        assert_eq!(dec.unwrap(), b"\x05hello world");
    }
    // a replay: the sequence is off (and counted)
    let (_, enc) = a.encrypt(&m, 0);
    let enc = enc.unwrap();
    assert_eq!(b.decrypt(&enc[4..], 0).0, 0);
    assert_eq!(b.decrypt(&enc[4..], 0).0, ws::SESSION_DECRYPT_INVALID_SEQUENCE_NUM);
    assert_eq!(b.in_seq, 5);
    let mut wrong = enc[4..].to_vec();
    wrong[1] ^= 1;
    assert_eq!(b.decrypt(&wrong, 0).0, ws::SESSION_DECRYPT_SESSION_ID_MISMATCH);
    assert_eq!(b.decrypt(&[12, 0x34], 0).0, ws::SESSION_DECRYPT_UNPACK_FAILURE);
    assert_eq!(b.decrypt(&[12, 0x34, 0x12, 1], 0).0, ws::SESSION_DECRYPT_FAILURE);
    assert_eq!(b.decrypt(&[1, 2, 3], 0), (0, None));
    // persistent: no id or sequence; a mini header gets its plain type back
    let mut p = EeWonAuthSession::new(AUTH_TYPE_PERSISTENT, 0, k, 0, 0);
    let (st, enc) = p.encrypt(b"abc", 0);
    assert_eq!(st, 0);
    let mut enc = enc.unwrap();
    assert_eq!(enc[0], 12);
    enc[0] = 4;
    assert_eq!(p.decrypt(&enc, 0), (0, Some(b"\x03abc".to_vec())));
    // plain sessions leave messages alone
    let mut n = EeWonAuthSession::new(AUTH_TYPE_PERSISTENT_NOCRYPT, 0, k, 0, 0);
    assert_eq!(n.encrypt(b"abc", 7), (0, None));
    assert_eq!(n.last_use, 7);
}

#[test]
fn the_extended_length_offset() {
    let k = key(b"secret12");
    let mut a = EeWonAuthSession::new(AUTH_TYPE_PERSISTENT, 0, k, 2, 0);
    let mut big = vec![0u8; 0x10000];
    big[6] = 0xab;
    let (_, enc) = a.encrypt(&big, 0);
    let enc = enc.unwrap();
    let dec = a.decrypt(&enc[6..], 0).1.unwrap();
    assert_eq!(dec.len(), 0x10000 - 6);
    assert_eq!(dec[0], 0xab);
}

// ---- the driver's stand-ins (tests/won/won_peerauth_test.cpp), for the golden's replay

fn hex(b: &[u8]) -> String {
    if b.is_empty() {
        return "-".into();
    }
    b.iter().map(|x| format!("{x:02x}")).collect()
}

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

struct Stand {
    log: String,
    keys: [Vec<u8>; 2],
    counts: [i32; 2],
    cert: Option<Vec<u8>>,
    own1: Vec<u8>,
    own2: Option<Vec<u8>>,
}

const NAMES: [char; 2] = ['A', 'B'];

impl PeerEnv for Stand {
    fn private_decrypt(&mut self, data: &[u8]) -> Option<Vec<u8>> {
        self.log += &format!("dec({})", hex(data));
        if data.is_empty() || data[0] == 0 {
            return None;
        }
        Some(data[1..].iter().map(|b| b ^ 0x33).collect())
    }
    fn make_cert(&mut self, auth2: bool, data: &[u8]) -> bool {
        self.log += &format!("cert{}({})", if auth2 { 2 } else { 1 }, hex(data));
        self.cert = Some(data.to_vec());
        !data.is_empty() && data[0] != 0xff
    }
    fn verify_cert(&mut self) -> bool {
        self.log += "verify;";
        !self.cert.as_ref().unwrap().len().is_multiple_of(5)
    }
    fn cert_expired(&mut self) -> bool {
        self.cert.as_ref().unwrap().last() == Some(&0xee)
    }
    fn cert_encrypt(&mut self, data: &[u8]) -> Option<Vec<u8>> {
        self.log += &format!("enc({})", hex(data));
        if data.len() % 7 == 3 {
            return None;
        }
        let mut v = vec![1u8];
        v.extend(data.iter().map(|b| b ^ 0x33));
        Some(v)
    }
    fn has_own_cert2(&mut self) -> bool {
        self.own2.is_some()
    }
    fn own_cert_raw(&mut self, auth2: bool) -> Vec<u8> {
        if auth2 {
            self.own2.clone().unwrap()
        } else {
            self.own1.clone()
        }
    }
    fn secret_create(&mut self, which: u8, length: i32) {
        let w = usize::from(which);
        self.log += &format!("create{}({length})", NAMES[w]);
        if !(5..=56).contains(&length) {
            return;
        }
        let name = NAMES[w] as i32;
        self.keys[w] = (0..length).map(|i| (i * 31 + self.counts[w] * 7 + name) as u8).collect();
        self.counts[w] += 1;
    }
    fn secret_set_key(&mut self, which: u8, key: &[u8]) -> bool {
        let w = usize::from(which);
        self.log += &format!("setkey{}({})", NAMES[w], hex(key));
        if !(5..=56).contains(&key.len()) {
            return false;
        }
        self.keys[w] = key.to_vec();
        true
    }
    fn secret_key(&mut self, which: u8) -> Vec<u8> {
        self.keys[usize::from(which)].clone()
    }
    fn new_session(&mut self, auth_type: i32, id: u16, which: u8, lfs: u8) {
        self.log += &format!("session({auth_type},{id},{},{lfs})", NAMES[usize::from(which)]);
    }
}

fn compact(s: &EeWonPeerState, e: &Stand) -> String {
    format!(
        "{},{},{},{},{},{},{},{},{},{}",
        s.state,
        s.auth_type,
        s.length_field_size,
        u8::from(s.use_auth2),
        u8::from(s.use_client_secret),
        s.client_key_size,
        hex(&e.keys[0]),
        hex(&e.keys[1]),
        e.cert.as_ref().map_or("*".to_string(), |c| hex(c)),
        hex(e.log.as_bytes())
    )
}

/// The original's handshakes recorded by tests/won/won_peerauth_test.cpp (`--print`): each side's scenario from
/// its members, then each message's status and out message, then the members and the trace.
#[test]
fn golden() {
    let mut n = [0usize; 2];
    for line in include_str!("golden_peerauth.txt").lines() {
        let mut parts = line.split(' ');
        let side = parts.next().unwrap();
        let f: Vec<&str> = parts.next().unwrap().split(',').collect();
        let mut s = EeWonPeerState {
            state: f[0].parse().unwrap(),
            auth_type: f[1].parse().unwrap(),
            length_field_size: f[2].parse().unwrap(),
            use_auth2: f[3] == "1",
            use_client_secret: f[4] == "1",
            client_key_size: f[5].parse().unwrap(),
        };
        let mut e = Stand {
            log: String::from_utf8(unhex(f[9])).unwrap(),
            keys: [unhex(f[6]), unhex(f[7])],
            counts: [f[12].parse().unwrap(), f[13].parse().unwrap()],
            cert: if f[8] == "*" { None } else { Some(unhex(f[8])) },
            own1: unhex(f[10]),
            own2: if f[11] == "*" { None } else { Some(unhex(f[11])) },
        };
        let client = side == "client";
        for step in parts {
            let (kind, rest) = step.split_once(':').unwrap();
            let g: Vec<&str> = rest.split(':').collect();
            match kind {
                "start" => {
                    let lfs: u8 = g[1].parse().unwrap();
                    if client {
                        let out = client_start(&mut s, &mut e, g[0].parse().unwrap(), lfs);
                        assert_eq!(hex(&out), g[2], "{line}");
                    } else {
                        server_start(&mut s, lfs);
                    }
                }
                "msg" => {
                    let m = unhex(g[0]);
                    let (st, out) =
                        if client { client_recv(&mut s, &mut e, &m) } else { server_recv(&mut s, &mut e, &m) };
                    assert_eq!(st, g[1].parse::<i32>().unwrap(), "{line}");
                    assert_eq!(out.map_or("*".to_string(), |o| hex(&o)), g[2], "{line}");
                }
                "end" => assert_eq!(compact(&s, &e), rest, "{line}"),
                _ => panic!("{line}"),
            }
        }
        n[usize::from(!client)] += 1;
    }
    assert!(n[0] >= 100 && n[1] >= 100, "{n:?}");
}
