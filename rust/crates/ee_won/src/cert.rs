//! GetCertOp's messages (`src/Empire Earth/WONAPI/WONAuth/GetCertOp.cpp`): the auth server's three requests
//! (the key block request, the login request and its encrypted login data, the login confirmation) and every
//! reply read as the original read it.
//!
//! The original's reply reads interleave with C++ checks: a certificate's validity and signature, a key block's
//! verification, the session key's decryption, the private key's import, the secret's confirmation, each able to
//! end the read early. Here a reply is read whole into its entries, in the original's order, up to the point a
//! read failed (`EE_WON_CERT_CUT`: the original threw `ReadBufferException` there); the op (`getcert.rs`) walks
//! them and applies the checks in order, so an early return leaves what the original left. Reached by nothing in
//! the game: the WON servers are gone.

use crate::wire::{PastEnd, Reader, Writer};

// ---- the requests

/// `SendPubKeyRequest`'s message.
pub const EE_WON_CERT_REQ_PUBKEYS: i32 = 1;
/// `SendLoginRequest`'s message (`key_id`, `a` the encrypted session key, `b` the encrypted login data).
pub const EE_WON_CERT_REQ_LOGIN: i32 = 2;
/// `SendLoginConfirm`'s message (`a` the encrypted hashes).
pub const EE_WON_CERT_REQ_CONFIRM: i32 = 3;

/// The auth service's header: type 5, service 12, the message type.
fn header(w: &mut Writer, message: i16) {
    w.append_byte(5);
    w.append_short(12);
    w.append_short(message);
}

/// A request's message (with its 4-byte length prefix).
pub fn request(kind: i32, key_id: i16, a: &[u8], b: &[u8]) -> Vec<u8> {
    let mut w = Writer::new(4, true);
    match kind {
        EE_WON_CERT_REQ_PUBKEYS => header(&mut w, 1),
        EE_WON_CERT_REQ_LOGIN => {
            header(&mut w, 3);
            w.append_short(key_id);
            w.append_short(a.len() as i16);
            w.append_bytes(a);
            w.append_short(b.len() as i16);
            w.append_bytes(b);
        }
        _ => {
            header(&mut w, 5);
            w.append_short(a.len() as i16);
            w.append_bytes(a);
        }
    }
    w.to_message()
}

/// The login data before the community data: the session key's first two bytes, the key block's id, the flags
/// (2|4|8, and 1 to create the account), the user name, the password and the new password.
pub fn login_data(
    key: [u8; 2],
    key_id: i16,
    create_account: bool,
    user: &[u16],
    password: &[u16],
    new_password: &[u16],
) -> Vec<u8> {
    let mut w = Writer::new(0, true);
    w.append_bytes(&key);
    w.append_short(key_id);
    let mut flags = 0x0002 | 0x0004 | 0x0008;
    if create_account {
        flags |= 0x0001;
    }
    w.append_long(flags);
    w.append_wstring(user, 2);
    w.append_wstring(password, 2);
    w.append_wstring(new_password, 2);
    w.data().to_vec()
}

// ---- the replies

/// The read failed here: the original threw `ReadBufferException`.
pub const EE_WON_CERT_CUT: u8 = 0;
/// `return InvalidReplyHeader()`.
pub const EE_WON_CERT_INVALID_HEADER: u8 = 1;
/// A key block reply's failure status `num`: `SetLastServerError(num)`, try the next server.
pub const EE_WON_CERT_SERVER_ERROR: u8 = 2;
/// A key block reply's block (`at`, `len`).
pub const EE_WON_CERT_KEY_BLOCK: u8 = 3;
/// A login challenge outside the challenge state.
pub const EE_WON_CERT_UNEXPECTED_CHALLENGE: u8 = 4;
/// A login challenge's encrypted block (`at`, `len`).
pub const EE_WON_CERT_CHALLENGE: u8 = 5;
/// The login reply's status `num` (the read stops after -1).
pub const EE_WON_CERT_LOGIN_STATUS: u8 = 6;
/// An error string (`at`, `len`): `mErrorList.push_back`.
pub const EE_WON_CERT_ERROR: u8 = 7;
/// A clear (or decrypted) entry of type `kind` (`at`, `len`); type 7 comes as `EE_WON_CERT_COMMUNITY` instead.
pub const EE_WON_CERT_ENTRY: u8 = 8;
/// A community status entry: `kind` = the fields read of its sub message (0 none, 1 the status `num`, 2 and the
/// id `id`, 3 and the name: `at`, `len` UTF-16 units). Fewer than 3: a `EE_WON_CERT_CUT` follows.
pub const EE_WON_CERT_COMMUNITY: u8 = 9;
/// The clear part's end: the rest (`at`, `len`) is the encrypted part.
pub const EE_WON_CERT_REST: u8 = 10;

/// One step of a reply's read.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonCertEvent {
    pub tag: u8,
    pub kind: u8,
    pub num: i32,
    pub id: u32,
    pub at: u32,
    pub len: u32,
}

fn ev(tag: u8) -> EeWonCertEvent {
    EeWonCertEvent { tag, ..Default::default() }
}

fn span(tag: u8, kind: u8, at: usize, len: usize) -> EeWonCertEvent {
    EeWonCertEvent { tag, kind, at: at as u32, len: len as u32, ..Default::default() }
}

/// The op's state: `GETTING_PUB_KEYS` (1), `GETTING_CHALLENGE` (2), `GETTING_CERTIFICATE` (3).
const EE_WON_CERT_GETTING_PUB_KEYS: i32 = 1;
const EE_WON_CERT_GETTING_CHALLENGE: i32 = 2;

/// `CheckResponse`'s read (and `HandleLoginReply`'s and `HandleLoginChallenge`'s clear parts) in `state`.
pub fn reply(data: &[u8], state: i32) -> Vec<EeWonCertEvent> {
    let mut out = Vec::new();
    if read_reply(&mut Reader::new(data), state, &mut out).is_err() {
        out.push(ev(EE_WON_CERT_CUT));
    }
    out
}

fn read_reply(r: &mut Reader, state: i32, out: &mut Vec<EeWonCertEvent>) -> Result<(), PastEnd> {
    let header_type = r.read_byte()?;
    let service = r.read_short()? as u16;
    let message = r.read_short()? as u16;
    if header_type != 5 || service != 12 {
        out.push(ev(EE_WON_CERT_INVALID_HEADER));
        return Ok(());
    }
    if state == EE_WON_CERT_GETTING_PUB_KEYS {
        if message != 2 {
            out.push(ev(EE_WON_CERT_INVALID_HEADER));
            return Ok(());
        }
        let status = i32::from(r.read_short()?);
        if status < 0 {
            out.push(EeWonCertEvent { tag: EE_WON_CERT_SERVER_ERROR, num: status, ..Default::default() });
            return Ok(());
        }
        let len = r.read_short()? as u16 as usize;
        let at = r.pos();
        r.read_bytes(len)?;
        out.push(span(EE_WON_CERT_KEY_BLOCK, 0, at, len));
        return Ok(());
    }
    if message == 6 {
        return read_login_reply(r, out);
    }
    if message != 4 {
        out.push(ev(EE_WON_CERT_INVALID_HEADER));
        return Ok(());
    }
    if state != EE_WON_CERT_GETTING_CHALLENGE {
        out.push(ev(EE_WON_CERT_UNEXPECTED_CHALLENGE));
        return Ok(());
    }
    let len = r.read_short()? as u16 as usize;
    let at = r.pos();
    r.read_bytes(len)?;
    out.push(span(EE_WON_CERT_CHALLENGE, 0, at, len));
    Ok(())
}

fn read_login_reply(r: &mut Reader, out: &mut Vec<EeWonCertEvent>) -> Result<(), PastEnd> {
    let status = i32::from(r.read_short()?);
    out.push(EeWonCertEvent { tag: EE_WON_CERT_LOGIN_STATUS, num: status, ..Default::default() });
    if status == -1 {
        return Ok(());
    }
    let errors = r.read_byte()?;
    for _ in 0..errors {
        let s = r.read_string(2)?;
        let at = r.pos() - s.len();
        out.push(span(EE_WON_CERT_ERROR, 0, at, s.len()));
    }
    let entries = r.read_byte()?;
    for _ in 0..entries {
        let kind = r.read_byte()?;
        let len = r.read_short()? as u16 as usize;
        let at = r.pos();
        let data = r.read_bytes(len)?;
        if kind == 7 {
            let (e, whole) = community(data, at);
            out.push(e);
            if !whole {
                return Err(PastEnd);
            }
        } else {
            out.push(span(EE_WON_CERT_ENTRY, kind, at, len));
        }
    }
    let at = r.pos();
    out.push(span(EE_WON_CERT_REST, 0, at, r.available()));
    Ok(())
}

/// A community status entry's sub message (`base`: its offset in the message): the event and whether it was
/// read whole.
fn community(data: &[u8], base: usize) -> (EeWonCertEvent, bool) {
    let mut e = ev(EE_WON_CERT_COMMUNITY);
    let mut r = Reader::new(data);
    let Ok(status) = r.read_short() else { return (e, false) };
    e.kind = 1;
    e.num = i32::from(status);
    let Ok(id) = r.read_long() else { return (e, false) };
    e.kind = 2;
    e.id = id as u32;
    let Ok(units) = r.read_length(2) else { return (e, false) };
    let at = r.pos();
    let Some(n) = (units as usize).checked_mul(2) else { return (e, false) };
    if r.read_bytes(n).is_err() {
        return (e, false);
    }
    e.kind = 3;
    e.at = (base + at) as u32;
    e.len = units;
    (e, true)
}

/// The login reply's decrypted part: its entries (`EE_WON_CERT_ENTRY`), then `EE_WON_CERT_CUT` if a read failed.
pub fn decrypted(data: &[u8]) -> Vec<EeWonCertEvent> {
    let mut out = Vec::new();
    let mut r = Reader::new(data);
    let mut read = || -> Result<(), PastEnd> {
        let entries = r.read_byte()?;
        for _ in 0..entries {
            let kind = r.read_byte()?;
            let len = r.read_short()? as u16 as usize;
            let at = r.pos();
            r.read_bytes(len)?;
            out.push(span(EE_WON_CERT_ENTRY, kind, at, len));
        }
        Ok(())
    };
    if read().is_err() {
        out.push(ev(EE_WON_CERT_CUT));
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tags(e: &[EeWonCertEvent]) -> Vec<u8> {
        e.iter().map(|e| e.tag).collect()
    }

    #[test]
    fn requests() {
        assert_eq!(request(EE_WON_CERT_REQ_PUBKEYS, 0, &[], &[]), [9, 0, 0, 0, 5, 12, 0, 1, 0]);
        assert_eq!(
            request(EE_WON_CERT_REQ_LOGIN, 0x102, &[0xaa], &[0xbb, 0xcc]),
            [18, 0, 0, 0, 5, 12, 0, 3, 0, 2, 1, 1, 0, 0xaa, 2, 0, 0xbb, 0xcc]
        );
        assert_eq!(request(EE_WON_CERT_REQ_CONFIRM, 0, &[7], &[]), [12, 0, 0, 0, 5, 12, 0, 5, 0, 1, 0, 7]);
    }

    #[test]
    fn the_login_data() {
        let d = login_data([1, 2], 3, true, &[0x41], &[], &[0x263a]);
        assert_eq!(d, [1, 2, 3, 0, 15, 0, 0, 0, 1, 0, 0x41, 0, 0, 0, 1, 0, 0x3a, 0x26]);
        assert_eq!(login_data([0, 0], 0, false, &[], &[], &[])[4], 14);
    }

    #[test]
    fn headers_and_key_blocks() {
        assert_eq!(tags(&reply(&[5, 12, 0], 1)), [EE_WON_CERT_CUT]);
        assert_eq!(tags(&reply(&[6, 12, 0, 2, 0], 1)), [EE_WON_CERT_INVALID_HEADER]);
        assert_eq!(tags(&reply(&[5, 12, 0, 3, 0], 1)), [EE_WON_CERT_INVALID_HEADER]);
        let e = reply(&[5, 12, 0, 2, 0, 0xfe, 0xff], 1);
        assert_eq!((e[0].tag, e[0].num), (EE_WON_CERT_SERVER_ERROR, -2));
        let e = reply(&[5, 12, 0, 2, 0, 0, 0, 2, 0, 9, 9], 1);
        assert_eq!((e[0].tag, e[0].at, e[0].len), (EE_WON_CERT_KEY_BLOCK, 9, 2));
        assert_eq!(tags(&reply(&[5, 12, 0, 2, 0, 0, 0, 2, 0, 9], 1)), [EE_WON_CERT_CUT]);
    }

    #[test]
    fn challenges() {
        assert_eq!(tags(&reply(&[5, 12, 0, 4, 0], 3)), [EE_WON_CERT_UNEXPECTED_CHALLENGE]);
        assert_eq!(tags(&reply(&[5, 12, 0, 5, 0], 2)), [EE_WON_CERT_INVALID_HEADER]);
        let e = reply(&[5, 12, 0, 4, 0, 1, 0, 3], 2);
        assert_eq!((e[0].tag, e[0].at, e[0].len), (EE_WON_CERT_CHALLENGE, 7, 1));
    }

    #[test]
    fn login_replies() {
        // status -1 stops the read
        assert_eq!(tags(&reply(&[5, 12, 0, 6, 0, 0xff, 0xff], 2)), [EE_WON_CERT_LOGIN_STATUS]);
        // one error "ab", entries: type 1 (2 bytes), type 7 whole, the rest
        let m = [
            5, 12, 0, 6, 0, 0, 0, 1, 2, 0, b'a', b'b', 2, 1, 2, 0, 8, 9, 7, 10, 0, 3, 0, 4, 0, 0, 0, 1, 0, 0x41, 0,
            0xee,
        ];
        let e = reply(&m, 2);
        assert_eq!(
            tags(&e),
            [EE_WON_CERT_LOGIN_STATUS, EE_WON_CERT_ERROR, EE_WON_CERT_ENTRY, EE_WON_CERT_COMMUNITY, EE_WON_CERT_REST]
        );
        assert_eq!((e[1].at, e[1].len), (10, 2));
        assert_eq!((e[2].kind, e[2].at, e[2].len), (1, 16, 2));
        assert_eq!((e[3].kind, e[3].num, e[3].id, e[3].at, e[3].len), (3, 3, 4, 29, 1));
        assert_eq!((e[4].at, e[4].len), (31, 1));
        // the community entry cut after its id
        let m = [5, 12, 0, 6, 0, 0, 0, 0, 1, 7, 6, 0, 3, 0, 4, 0, 0, 0];
        let e = reply(&m, 2);
        assert_eq!(tags(&e), [EE_WON_CERT_LOGIN_STATUS, EE_WON_CERT_COMMUNITY, EE_WON_CERT_CUT]);
        assert_eq!(e[1].kind, 2);
        // a negative name length is a cut
        let m = [5, 12, 0, 6, 0, 0, 0, 0, 1, 7, 8, 0, 3, 0, 4, 0, 0, 0, 0xff, 0xff];
        assert_eq!(reply(&m, 2)[1].kind, 2);
    }

    #[test]
    fn the_decrypted_part() {
        assert_eq!(tags(&decrypted(&[])), [EE_WON_CERT_CUT]);
        let e = decrypted(&[2, 4, 1, 0, 9, 2, 0]);
        assert_eq!(tags(&e), [EE_WON_CERT_ENTRY, EE_WON_CERT_CUT]);
        assert_eq!((e[0].kind, e[0].at, e[0].len), (4, 4, 1));
        assert_eq!(tags(&decrypted(&[0, 1])), Vec::<u8>::new());
    }

    /// What the original read of a reply with every check passing (the golden's notation).
    fn replay(data: &[u8], state: i32) -> String {
        let key: Vec<u8> = (0x10..0x18).collect();
        let mut t: Vec<String> = Vec::new();
        let (mut status, mut cert1, mut cert2) = (0, false, false);
        for e in reply(data, state) {
            match e.tag {
                EE_WON_CERT_CUT => t.push("cut".into()),
                EE_WON_CERT_INVALID_HEADER => t.push("invalid".into()),
                EE_WON_CERT_SERVER_ERROR => t.push(format!("server {}", e.num)),
                EE_WON_CERT_KEY_BLOCK => t.push(format!("block {} {}", e.at, e.len)),
                EE_WON_CERT_UNEXPECTED_CHALLENGE => t.push("unexpected".into()),
                EE_WON_CERT_CHALLENGE => {
                    t.push(format!("challenge {} {}", e.at, e.len));
                    t.push(if e.len < 16 { "cut" } else { "seed" }.into());
                }
                EE_WON_CERT_LOGIN_STATUS => {
                    status = e.num;
                    t.push(format!("status {}", e.num));
                }
                EE_WON_CERT_ERROR => t.push(format!("error {} {}", e.at, e.len)),
                EE_WON_CERT_ENTRY => {
                    cert1 |= e.kind == 1;
                    cert2 |= e.kind == 6;
                    t.push(format!("entry {} {} {}", e.kind, e.at, e.len));
                }
                EE_WON_CERT_COMMUNITY if e.kind == 3 => {
                    t.push(format!("community 3 {} {} {} {}", e.num, e.id, e.at, e.len))
                }
                EE_WON_CERT_COMMUNITY => t.push(format!("community {} {} {}", e.kind, e.num, e.id)),
                _ => {
                    t.push(format!("rest {} {}", e.at, e.len));
                    if status < 0 {
                        break;
                    }
                    if cert1 != cert2 {
                        t.push("crash".into());
                        break;
                    }
                    let rest = &data[e.at as usize..];
                    for c in decrypted(rest) {
                        if c.tag == EE_WON_CERT_CUT {
                            t.push("cut".into());
                            break;
                        }
                        t.push(format!("crypt {} {} {}", c.kind, c.at, c.len));
                        let d = &rest[c.at as usize..(c.at + c.len) as usize];
                        if c.kind == 4 && (d.len() != 10 || d[2..] != key[..]) {
                            break;
                        }
                    }
                }
            }
        }
        t.join(" ")
    }

    #[test]
    fn the_golden() {
        let mut n = 0;
        for line in include_str!("golden_cert.txt").lines() {
            let (head, read) = line.split_once(" |").unwrap();
            let (state, hex) = head.split_once(' ').unwrap();
            let data: Vec<u8> = if hex == "-" {
                Vec::new()
            } else {
                (0..hex.len()).step_by(2).map(|i| u8::from_str_radix(&hex[i..i + 2], 16).unwrap()).collect()
            };
            assert_eq!(replay(&data, state.parse().unwrap()), read.trim_start(), "{line}");
            n += 1;
        }
        assert_eq!(n, 2500);
    }
}
