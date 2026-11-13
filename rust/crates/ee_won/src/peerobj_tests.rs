use super::*;
use crate::random::ee_won_random_test_seed;

fn hex(d: &[u8]) -> String {
    if d.is_empty() {
        return "-".to_string();
    }
    d.iter().map(|b| format!("{b:02x}")).collect()
}

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len() / 2).map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap()).collect()
}

fn b(v: bool) -> &'static str {
    if v {
        "1"
    } else {
        "0"
    }
}

// tests/won/won_peerobj_test.cpp's keys (kPriv), private DER; the public form drops x's ten bytes
const PRIV: [&str; 4] = [
    "3065021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a01ad613e700f0fe73a021900e90a2cffe3fabbe6ece0e935a39d1d5d424c542c077c261802080139dba7acd3581d",
    "3064021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a01ad613e700f0fe73a02184bb17e485a32d0a3f02eb0bc5d2b9a09f3cb369e38ff200602080c931814d44a5e5b",
    "3064021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a01ad613e700f0fe73a02183dda7c54fd793a4e534f60facc1301e37953f4df16f49861020808e58d3749117c1b",
    "3064021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a01ad613e700f0fe73a0218467548d5e50f1f981f842a63ceebe356665d1ec15fcdc85d02080f47ec32536bc8f1",
];

fn der(k: usize, private: bool) -> Vec<u8> {
    let mut d = unhex(PRIV[k]);
    if !private {
        d.truncate(d.len() - 10);
        d[1] = (d.len() - 2) as u8;
    }
    d
}

/// `ElGamal(data, len, isPrivate)`: a new key (a draw), then the key set.
fn key(k: usize, private: bool) -> ElGamal {
    let mut e = ElGamal::new();
    let d = der(k, private);
    if private {
        e.set_private_key(&d);
    } else {
        e.set_public_key(&d);
    }
    e
}

const COMMUNITY: u32 = 42;

fn q_cert(c: &Cert, block: &Cert, delta: i64, v2: bool, now: i64) -> String {
    let mut t = format!(
        "v{} i{} e{} u{} x{}{} p{}{}{} b{}",
        b(c.valid()),
        c.block.issue_time,
        c.block.expire_time,
        c.user_id(),
        b(c.is_expired(now, 0)),
        b(c.is_expired(now, delta)),
        b(c.verify_permissions(0, 0, 0)),
        b(c.verify_permissions(1000, 0, 0)),
        b(c.verify_permissions(0, COMMUNITY, 5)),
        b(block.verify(c)),
    );
    if v2 {
        let first = cert_num(c, EE_WON_AUTHOBJ_FIRST_COMMUNITY, 0, 0, 0, now);
        t += &format!(
            " f{} t{} k{}",
            first,
            cert_num(c, EE_WON_AUTHOBJ_FIRST_TRUST, 0, 0, 0, now),
            cert_num(c, EE_WON_AUTHOBJ_KEY_ID, first, 0, 0, now)
        );
    }
    t
}

fn q_peer(p: &PeerData, now: i64) -> String {
    let has2 = p.cert2.is_some();
    format!(
        "V{} W{} X{}{} Z{} D{} A{} P{}",
        b(p.is_valid()),
        if has2 { b(p.is_valid2()) } else { "N" },
        b(p.cert_expired(now, 0)),
        b(p.cert_expired(now, 120)),
        if has2 { b(p.cert2_expired(now, 0)) } else { "N" },
        b(p.expire_delta(now) > 0),
        p.delta,
        b(p.private_key.is_private()),
    )
}

struct Side {
    full: bool,
    private: usize,
    c1: Vec<u8>,
    c2: Option<Vec<u8>>,
    delta: i64,
    use_auth2: bool,
}

/// The driver's `Mutate`: the message without its length field, a byte flipped or cut as planned.
fn mutate(m: &[u8], lfs: usize, muts: &[i64], step: usize, t: &mut String) -> Vec<u8> {
    let mut r = m[lfs.min(m.len())..].to_vec();
    let (kind, a, x) = (muts[step * 3], muts[step * 3 + 1] as usize, muts[step * 3 + 2]);
    if kind == 0 || r.is_empty() {
        return r;
    }
    if kind == 1 {
        let n = r.len();
        r[a % n] ^= x as u8;
    } else {
        r.truncate(a % r.len());
    }
    *t += &format!(" mut{kind}");
    r
}

fn session(s: &Option<EeWonAuthSession>) -> String {
    s.as_ref().map_or("N".to_string(), |s| format!("{}.{}.{}", s.auth_type, s.id, hex(s.key.key())))
}

fn other(p: &PeerAuth) -> String {
    p.other.as_ref().map_or("N".to_string(), |c| format!("{}.{}", b(c.valid()), c.user_id()))
}

/// One golden line: the driver's `Half<H>::Run` over the Rust objects.
fn replay(line: &str) -> (String, String, bool) {
    let (inputs, want) = line.split_once(" => ").unwrap();
    let f: Vec<&str> = inputs.split(' ').collect();
    assert_eq!(f[0], "P");
    let run: i64 = f[1].parse().unwrap();
    let signer: usize = f[2].parse().unwrap();
    let block_raw = unhex(f[3]);
    let side = |at: usize| Side {
        full: f[at] == "1",
        private: f[at + 1].parse().unwrap(),
        c1: unhex(f[at + 2]),
        c2: if f[at + 3] == "N" { None } else { Some(unhex(f[at + 3])) },
        delta: f[at + 4].parse().unwrap(),
        use_auth2: f[at + 5] == "1",
    };
    let sides = [side(4), side(10)];
    let set_key = f[16] == "1";
    let key_size: u8 = f[17].parse().unwrap();
    let use_secret = f[18] == "1";
    let auth_type: i32 = f[19].parse().unwrap();
    let lfs: u8 = f[20].parse().unwrap();
    let muts: Vec<i64> = f[21].split(',').map(|x| x.parse().unwrap()).collect();

    let now = unix_now();
    ee_won_random_test_seed(run);
    let block = Arc::new(Cert::new(Kind::PubKeyBlock, &block_raw));
    let signer = key(signer, false);
    let mut t = format!(
        "B v{} id{} vv{} k{} i{} e{} x{}",
        b(block.valid()),
        block.block_id(),
        b(block.is_valid_and_verified(&signer)),
        b(block.has_key()),
        block.block.issue_time,
        block.block.expire_time,
        b(block.is_expired(now, 0))
    );
    let mut peers = Vec::new();
    for (i, d) in sides.iter().enumerate() {
        t += if i == 1 { " |S " } else { " |C " };
        let p = if !d.full {
            PeerData::empty(Some(block.clone()))
        } else {
            let c1 = Arc::new(Cert::new(Kind::Certificate, &d.c1));
            let c2 = d.c2.as_ref().map(|r| Arc::new(Cert::new(Kind::Certificate2, r)));
            let private_key = key(d.private, true);
            t += &format!("c1 {}", q_cert(&c1, &block, d.delta, false, now));
            if let Some(c2) = &c2 {
                t += &format!(" c2 {}", q_cert(c2, &block, d.delta, true, now));
            }
            t += " ";
            PeerData { cert: Some(c1), cert2: c2, block: Some(block.clone()), private_key, delta: d.delta }
        };
        t += &q_peer(&p, now);
        peers.push(Arc::new(p));
    }
    let mut client = PeerAuth::new(false);
    let mut server = PeerAuth::new(true);
    client.set_use_auth2(sides[0].use_auth2);
    if set_key {
        client.set_client_key_size(key_size, use_secret);
    }
    server.set_use_auth2(sides[1].use_auth2);
    server.start(Some(peers[1].clone()), 0, lfs, now);
    let mut sessions = [None, None];
    let mut msg = client.start(Some(peers[0].clone()), auth_type, lfs, now);
    t += &format!(" |H c>{}", hex(msg.as_deref().unwrap()));
    let mut step = 0;
    let lfs = usize::from(lfs);
    for _ in 0..3 {
        let Some(m) = msg.take() else { break };
        let m = mutate(&m, lfs, &muts, step, &mut t);
        step += 1;
        let (st, out) = server.recv(&m, now);
        if server.session.is_some() {
            sessions[1] = server.session.take();
        }
        t += &format!(" s{st}.{}>{}", server.st.state, out.as_deref().map_or("N".to_string(), hex));
        let Some(out) = out else { break };
        let m = mutate(&out, lfs, &muts, step, &mut t);
        step += 1;
        let (st, next) = client.recv(&m, now);
        if client.session.is_some() {
            sessions[0] = client.session.take();
        }
        t += &format!(" c{st}.{}>{}", client.st.state, next.as_deref().map_or("N".to_string(), hex));
        msg = next;
    }
    t += &format!(" |cs{} ss{}", session(&sessions[0]), session(&sessions[1]));
    t += &format!(" sc{} cc{}", other(&client), other(&server));
    let ok = sessions[0].is_some() && sessions[1].is_some();
    (t, want.to_string(), ok)
}

/// The original's classes recorded by tests/won/won_peerobj_test.cpp (`--print`): every accessor of the objects
/// and the handshake, from the same random seed.
#[test]
fn golden() {
    let mut both = 0;
    let mut n = 0;
    for line in include_str!("golden_peerobj.txt").lines() {
        let (got, want, ok) = replay(line);
        assert_eq!(got, want, "line {n}");
        both += usize::from(ok);
        n += 1;
    }
    assert_eq!(n, 600);
    assert!(both >= 20, "{both} handshakes with both sessions");
}

#[test]
fn an_empty_peer_data_is_invalid_and_expired() {
    let p = PeerData::empty(None);
    assert!(!p.is_valid() && !p.is_valid2());
    assert!(p.cert_expired(0, 0) && p.cert2_expired(0, 0));
    assert_eq!(p.expire_delta(0), 0);
    assert!(p.block.as_ref().is_some_and(|b| !b.valid() && !b.has_key()));
    // a missing second certificate: invalid and expired, not a crash
    let q = PeerData { cert2: None, ..PeerData::empty(None) };
    assert!(!q.is_valid2() && q.cert2_expired(0, 0));
}

#[test]
fn a_handshake_between_valid_sides() {
    let signer = key(0, true);
    let sign = |data: &[u8]| {
        let mut s = signer.clone();
        let mut raw = data.to_vec();
        raw.extend_from_slice(&s.sign(data).unwrap());
        raw
    };
    let mut w = crate::wire::Writer::new(0, false);
    w.append_short(3);
    w.append_long(1000);
    w.append_long(0x7f00_0000);
    w.append_short(17);
    w.append_short(1);
    let public = der(0, false);
    w.append_short(public.len() as i16);
    w.append_bytes(&public);
    let block = Arc::new(Cert::new(Kind::PubKeyBlock, &sign(w.data())));
    assert!(block.valid() && block.block_id() == 17);
    let cert = |k: usize, user: i32| {
        let mut w = crate::wire::Writer::new(0, false);
        w.append_short(1);
        w.append_long(1000);
        w.append_long(0x7f00_0000);
        w.append_long(user);
        w.append_long(42);
        w.append_short(3);
        let public = der(k, false);
        w.append_short(public.len() as i16);
        w.append_bytes(&public);
        Arc::new(Cert::new(Kind::Certificate, &sign(w.data())))
    };
    let side = |k: usize, c: Arc<Cert>| {
        Arc::new(PeerData {
            cert: Some(c),
            cert2: None,
            block: Some(block.clone()),
            private_key: key(k, true),
            delta: 0,
        })
    };
    let (cp, sp) = (side(2, cert(2, 1000)), side(3, cert(3, 1001)));
    assert!(cp.is_valid() && block.verify(cp.cert.as_ref().unwrap()));
    let now = unix_now();
    let mut client = PeerAuth::new(false);
    let mut server = PeerAuth::new(true);
    client.set_use_auth2(false);
    server.set_use_auth2(false);
    server.start(Some(sp), 0, 4, now);
    let req = client.start(Some(cp), 2, 4, now).unwrap();
    let (st, c1) = server.recv(&req[4..], now);
    assert_eq!(st, 0);
    let (st, c2) = client.recv(&c1.unwrap()[4..], now);
    assert_eq!(st, 0);
    let (st, done) = server.recv(&c2.unwrap()[4..], now);
    assert_eq!(st, 0);
    let (st, last) = client.recv(&done.unwrap()[4..], now);
    assert_eq!((st, last), (0, None));
    let (cs, ss) = (client.session.take().unwrap(), server.session.take().unwrap());
    assert_eq!((cs.auth_type, ss.auth_type), (2, 2));
    assert_eq!(cs.key.key(), ss.key.key());
    assert_eq!(other_num(&server, EE_WON_PEERAUTH_OTHER_USER, 0, 0, 0), 1000);
    assert_eq!(other_num(&server, EE_WON_PEERAUTH_OTHER_PERMITS, 0, 42, 3), 1);
    assert_eq!(other_num(&client, EE_WON_PEERAUTH_OTHER_USER, 0, 0, 0), 1001);
}
