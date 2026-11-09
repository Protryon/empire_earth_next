//! WON's signed auth blocks (`WONAuth/AuthBase.cpp`, `AuthCertificate.cpp`, `Auth2Certificate.cpp`,
//! `AuthPubKeyBlock.cpp`): a header (family, issue time, expiry time), a kind-specific body, then the signature
//! of everything before it. A block is valid when its body parses and a signature follows.

use std::collections::BTreeMap;

use crate::elgamal::ElGamal;
use crate::wire::{PastEnd, Reader};

/// Which block a buffer holds.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Kind {
    /// The header only (`AuthBase`).
    Base,
    /// `AuthCertificate`.
    Certificate,
    /// `Auth2Certificate`.
    Certificate2,
    /// `AuthPubKeyBlock`.
    PubKeyBlock,
}

/// `AuthCertificate`'s fields.
#[derive(Clone, Debug, Default)]
pub struct Certificate {
    pub user_id: u32,
    pub community_id: u32,
    pub trust_level: u16,
    pub pub_key: Option<ElGamal>,
}

/// `Auth2Certificate`'s fields: typed records (0: user and key, 1: user data, 2: nicknames, 3: key ids).
#[derive(Clone, Debug, Default)]
pub struct Certificate2 {
    pub user_id: u32,
    pub pub_key: Option<ElGamal>,
    /// UTF-16 units (`text`)
    pub user_name: Vec<u16>,
    pub community_trust: BTreeMap<u32, u16>,
    pub nicknames: BTreeMap<Vec<u16>, Vec<u16>>,
    pub user_data: BTreeMap<u32, Vec<u8>>,
    pub key_ids: BTreeMap<u32, u32>,
}

/// `AuthPubKeyBlock`'s fields.
#[derive(Clone, Debug, Default)]
pub struct PubKeyBlock {
    pub id: u16,
    pub keys: Vec<ElGamal>,
}

/// A block's body.
#[derive(Clone, Debug)]
pub enum Body {
    Base,
    Certificate(Certificate),
    Certificate2(Certificate2),
    PubKeyBlock(PubKeyBlock),
}

/// An unpacked block. The header fields are those read before any failure, as in the C++.
#[derive(Clone, Debug)]
pub struct AuthBlock {
    pub raw: Vec<u8>,
    pub valid: bool,
    pub family: u16,
    pub issue_time: i32,
    pub expire_time: i32,
    /// the length of the signed part (the C++ kept it as `unsigned short`)
    pub data_len: u16,
    pub body: Body,
}

enum Fail {
    Invalid,
    PastEnd,
}

impl From<PastEnd> for Fail {
    fn from(_: PastEnd) -> Fail {
        Fail::PastEnd
    }
}

fn read_key(r: &mut Reader) -> Result<ElGamal, Fail> {
    let len = r.read_short()? as u16;
    let mut key = ElGamal::new();
    if !key.set_public_key(r.read_bytes(usize::from(len))?) {
        return Err(Fail::Invalid);
    }
    Ok(key)
}

impl AuthBlock {
    fn header(&mut self, r: &mut Reader) -> Result<(), Fail> {
        self.family = r.read_short()? as u16;
        self.issue_time = r.read_long()?;
        self.expire_time = r.read_long()?;
        Ok(())
    }

    fn certificate(&mut self, r: &mut Reader) -> Result<Certificate, Fail> {
        self.header(r)?;
        Ok(Certificate {
            user_id: r.read_long()? as u32,
            community_id: r.read_long()? as u32,
            trust_level: r.read_short()? as u16,
            pub_key: Some(read_key(r)?),
        })
    }

    fn certificate2(&mut self, r: &mut Reader) -> Result<Certificate2, Fail> {
        self.header(r)?;
        let mut c = Certificate2::default();
        let count = r.read_short()? as u16;
        for _ in 0..count {
            let kind = r.read_short()? as u16;
            let length = usize::from(r.read_short()? as u16);
            let start = r.pos();
            match kind {
                0 => {
                    c.user_id = r.read_long()? as u32;
                    c.pub_key = Some(read_key(r)?);
                    c.user_name = r.read_wstring(2)?;
                    let communities = r.read_byte()?;
                    for _ in 0..communities {
                        let id = r.read_long()? as u32;
                        let trust = r.read_short()? as u16;
                        c.community_trust.insert(id, trust);
                    }
                }
                1 => {
                    let id = r.read_long()? as u32;
                    let data = r.read_string(2)?.to_vec();
                    c.user_data.insert(id, data);
                }
                2 => {
                    let key = r.read_wstring(2)?;
                    let value = r.read_wstring(2)?;
                    c.nicknames.insert(key, value);
                }
                3 => {
                    let id = r.read_long()? as u32;
                    let key_id = r.read_long()? as u32;
                    c.key_ids.insert(id, key_id);
                }
                _ => {}
            }
            // the rest of the record (a record shorter than its fields: the C++ asked for a negative count and threw)
            let used = r.pos() - start;
            if used > length {
                return Err(Fail::PastEnd);
            }
            r.read_bytes(length - used)?;
        }
        Ok(c)
    }

    fn pub_key_block(&mut self, r: &mut Reader) -> Result<PubKeyBlock, Fail> {
        self.header(r)?;
        let id = r.read_short()? as u16;
        let count = r.read_short()? as u16;
        if count == 0 {
            return Err(Fail::Invalid);
        }
        let mut keys = Vec::with_capacity(usize::from(count));
        for _ in 0..count {
            keys.push(read_key(r)?);
        }
        Ok(PubKeyBlock { id, keys })
    }

    /// `AuthBase(data, len)` and `Unpack()`: an empty buffer is an invalid block.
    pub fn unpack(kind: Kind, raw: &[u8]) -> AuthBlock {
        let mut b = AuthBlock {
            raw: raw.to_vec(),
            valid: false,
            family: 0,
            issue_time: 0,
            expire_time: 0,
            data_len: 0,
            body: Body::Base,
        };
        if raw.is_empty() {
            return b;
        }
        let mut r = Reader::new(raw);
        let parsed = match kind {
            Kind::Base => b.header(&mut r).map(|_| Body::Base),
            Kind::Certificate => b.certificate(&mut r).map(Body::Certificate),
            Kind::Certificate2 => b.certificate2(&mut r).map(Body::Certificate2),
            Kind::PubKeyBlock => b.pub_key_block(&mut r).map(Body::PubKeyBlock),
        };
        if let Ok(body) = parsed {
            b.body = body;
            b.data_len = r.pos() as u16;
            b.valid = raw.len() > usize::from(b.data_len);
        }
        b
    }

    /// The signed bytes.
    pub fn data(&self) -> &[u8] {
        &self.raw[..usize::from(self.data_len).min(self.raw.len())]
    }

    /// The signature.
    pub fn signature(&self) -> &[u8] {
        &self.raw[usize::from(self.data_len).min(self.raw.len())..]
    }

    /// `AuthBase::Verify(key)`: the signature over the data.
    pub fn verify(&self, key: &ElGamal) -> bool {
        key.verify(self.data(), self.signature())
    }

    /// `IsExpired(extra)` at the time `now`: an invalid block is always expired.
    pub fn is_expired(&self, now: i64, extra: i64) -> bool {
        !self.valid || i64::from(self.expire_time) <= now + extra
    }

    /// The public key of a certificate.
    pub fn pub_key(&self) -> Option<&ElGamal> {
        match &self.body {
            Body::Certificate(c) => c.pub_key.as_ref(),
            Body::Certificate2(c) => c.pub_key.as_ref(),
            _ => None,
        }
    }

    /// The user id of a certificate (0 otherwise).
    pub fn user_id(&self) -> u32 {
        match &self.body {
            Body::Certificate(c) => c.user_id,
            Body::Certificate2(c) => c.user_id,
            _ => 0,
        }
    }

    /// `VerifyPermissions(userId, community, minTrust)`; 0 means any user, any community. The base class (and any
    /// non-certificate) grants nothing.
    pub fn verify_permissions(&self, user_id: u32, community: u32, min_trust: u16) -> bool {
        match &self.body {
            Body::Certificate(c) => {
                (user_id == 0 || c.user_id == user_id)
                    && (community == 0 || c.community_id == community)
                    && c.trust_level >= min_trust
            }
            Body::Certificate2(c) => {
                if user_id != 0 && c.user_id != user_id {
                    return false;
                }
                if community != 0 {
                    return c.community_trust.get(&community).is_some_and(|&t| t >= min_trust);
                }
                c.community_trust.values().any(|&t| t >= min_trust)
            }
            _ => false,
        }
    }

    /// `AuthPubKeyBlock::Verify(block)`: both valid and any of the keys verifies the block.
    pub fn verify_with_block(&self, block: &AuthBlock) -> bool {
        match &self.body {
            Body::PubKeyBlock(k) if self.valid && block.valid => k.keys.iter().any(|key| block.verify(key)),
            _ => false,
        }
    }

    /// `IsValidAndVerified(key)`.
    pub fn is_valid_and_verified(&self, key: &ElGamal) -> bool {
        self.valid && self.verify(key)
    }
}

impl Certificate2 {
    /// `GetNickname`: empty when absent.
    pub fn nickname(&self, key: &[u16]) -> Vec<u16> {
        self.nicknames.get(key).cloned().unwrap_or_default()
    }

    /// `GetFirstNickname`: as the C++, looks the first entry's *value* up as a key.
    pub fn first_nickname(&self) -> Vec<u16> {
        self.nicknames.values().next().map(|v| self.nickname(v)).unwrap_or_default()
    }

    /// `GetFirstCommunity`: 0 when there is none.
    pub fn first_community(&self) -> u32 {
        self.community_trust.keys().next().copied().unwrap_or(0)
    }

    /// `GetFirstCommunityTrust`.
    pub fn first_community_trust(&self) -> u16 {
        self.community_trust.values().next().copied().unwrap_or(0)
    }

    /// `GetKeyId`: 0 when absent.
    pub fn key_id(&self, community: u32) -> u32 {
        self.key_ids.get(&community).copied().unwrap_or(0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::bigint::BigInt;
    use crate::der;
    use crate::random::Random;
    use crate::text;
    use crate::wire::Writer;

    fn keys() -> (ElGamal, Vec<u8>) {
        let p = BigInt::from_i32(1_073_761_061);
        let q = BigInt::from_i32(1019);
        let g = BigInt::from_i32(611_659_888);
        let x = BigInt::from_i32(77);
        let y = g.mod_exp(&x, &p).unwrap();
        let mut signer = ElGamal::with_random(Random::from_seed(b"auth"));
        assert!(signer.set_private_key(&der::insert(&[&p, &q, &g, &y, &x])));
        (signer, der::insert(&[&p, &q, &g, &y]))
    }

    fn signed(signer: &mut ElGamal, w: &Writer) -> Vec<u8> {
        let mut raw = w.data().to_vec();
        raw.extend_from_slice(&signer.sign(w.data()).unwrap());
        raw
    }

    #[test]
    fn certificate_unpacks_and_verifies() {
        let (mut signer, public) = keys();
        let mut w = Writer::new(0, false);
        w.append_short(1);
        w.append_long(1000);
        w.append_long(2_000_000_000);
        w.append_long(77);
        w.append_long(5);
        w.append_short(9);
        w.append_short(public.len() as i16);
        w.append_bytes(&public);
        let raw = signed(&mut signer, &w);
        let c = AuthBlock::unpack(Kind::Certificate, &raw);
        assert!(c.valid);
        assert_eq!((c.family, c.issue_time, c.expire_time), (1, 1000, 2_000_000_000));
        assert_eq!(usize::from(c.data_len), w.data().len());
        assert_eq!(c.user_id(), 77);
        assert!(c.verify_permissions(77, 5, 9));
        assert!(!c.verify_permissions(0, 0, 10));
        assert!(!c.verify_permissions(78, 0, 0));
        assert!(!c.verify_permissions(0, 6, 0));
        assert!(c.verify(&signer));
        assert!(c.pub_key().unwrap().is_public());
        assert!(!c.is_expired(1_999_999_000, 0));
        assert!(c.is_expired(1_999_999_000, 1000));
        // unsigned: invalid; truncated: invalid
        assert!(!AuthBlock::unpack(Kind::Certificate, w.data()).valid);
        assert!(!AuthBlock::unpack(Kind::Certificate, &raw[..20]).valid);
        assert!(!AuthBlock::unpack(Kind::Certificate, &[]).valid);
        // a flipped data byte no longer verifies
        let mut bad = raw.clone();
        bad[3] ^= 1;
        assert!(!AuthBlock::unpack(Kind::Certificate, &bad).verify(&signer));
    }

    #[test]
    fn certificate2_records() {
        let (mut signer, public) = keys();
        let mut w = Writer::new(0, false);
        w.append_short(2);
        w.append_long(1000);
        w.append_long(2_000_000_000);
        w.append_short(5);
        let mut rec = |kind: i16, body: Writer| {
            w.append_short(kind);
            w.append_short(body.data().len() as i16);
            w.append_bytes(body.data());
        };
        let mut b = Writer::new(0, false);
        b.append_long(4242);
        b.append_short(public.len() as i16);
        b.append_bytes(&public);
        b.append_wstring(&text::from_utf8("Player\u{e9}"), 2);
        b.append_byte(2);
        b.append_long(30);
        b.append_short(3);
        b.append_long(10);
        b.append_short(7);
        rec(0, b);
        let mut b = Writer::new(0, false);
        b.append_long(10);
        b.append_string(b"udata", 2);
        b.append_byte(0xee); // past the record's fields: skipped
        rec(1, b);
        let mut b = Writer::new(0, false);
        b.append_wstring(&text::from_utf8("k1"), 2);
        b.append_wstring(&text::from_utf8("Nick"), 2);
        rec(2, b);
        let mut b = Writer::new(0, false);
        b.append_long(10);
        b.append_long(99);
        rec(3, b);
        let mut b = Writer::new(0, false);
        b.append_long(1);
        rec(9, b);
        let raw = signed(&mut signer, &w);
        let c = AuthBlock::unpack(Kind::Certificate2, &raw);
        assert!(c.valid);
        assert!(c.verify(&signer));
        let Body::Certificate2(c2) = &c.body else { panic!("not a certificate 2") };
        assert_eq!(c2.user_id, 4242);
        assert_eq!(text::to_display(&c2.user_name), "Player\u{e9}");
        assert_eq!(c2.first_community(), 10);
        assert_eq!(c2.first_community_trust(), 7);
        assert_eq!(c2.user_data.get(&10).map(Vec::as_slice), Some(&b"udata"[..]));
        assert_eq!(text::to_display(&c2.nickname(&text::from_utf8("k1"))), "Nick");
        assert!(c2.first_nickname().is_empty());
        assert_eq!((c2.key_id(10), c2.key_id(11)), (99, 0));
        assert!(c.verify_permissions(4242, 30, 3));
        assert!(!c.verify_permissions(0, 10, 8));
        assert!(c.verify_permissions(0, 0, 7));
        assert!(!c.verify_permissions(0, 0, 8));
    }

    #[test]
    fn a_record_shorter_than_its_fields_is_invalid() {
        let mut w = Writer::new(0, false);
        w.append_short(2);
        w.append_long(0);
        w.append_long(0);
        w.append_short(1);
        w.append_short(3);
        w.append_short(4); // claims 4 bytes, holds 8
        w.append_long(1);
        w.append_long(2);
        w.append_bytes(&[0; 8]);
        assert!(!AuthBlock::unpack(Kind::Certificate2, w.data()).valid);
    }

    #[test]
    fn pub_key_blocks() {
        let (mut signer, public) = keys();
        let mut w = Writer::new(0, false);
        w.append_short(3);
        w.append_long(1000);
        w.append_long(2_000_000_000);
        w.append_short(17);
        w.append_short(2);
        for _ in 0..2 {
            w.append_short(public.len() as i16);
            w.append_bytes(&public);
        }
        let raw = signed(&mut signer, &w);
        let k = AuthBlock::unpack(Kind::PubKeyBlock, &raw);
        assert!(k.valid && k.is_valid_and_verified(&signer));
        let Body::PubKeyBlock(kb) = &k.body else { panic!("not a key block") };
        assert_eq!((kb.id, kb.keys.len()), (17, 2));
        assert!(k.verify_with_block(&k));
        let invalid = AuthBlock::unpack(Kind::PubKeyBlock, &raw[..10]);
        assert!(!k.verify_with_block(&invalid));
        assert!(!invalid.verify_with_block(&k));
        // no keys: invalid
        let mut w = Writer::new(0, false);
        w.append_bytes(&[3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 9]);
        assert!(!AuthBlock::unpack(Kind::PubKeyBlock, w.data()).valid);
        assert!(!AuthBlock::unpack(Kind::Base, &raw).verify_permissions(0, 0, 0));
    }
}
