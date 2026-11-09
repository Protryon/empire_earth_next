//! WONCrypt's ElGamal (`ElGamal*.cpp`): keys (p, q, g, y[, x]) in the `der` encoding, block encryption, and the
//! "bogus" signature of an MD5 digest that WON's certificates carry: a = (g^k mod p + M) mod p mod q,
//! b = (k - x a) mod q, checked as a == (g^b y^a mod p + M) mod q.
//!
//! Where the C++ crashed or read outside a buffer, this returns None or false instead: a key whose modulus gives no
//! reciprocal (the C++ let `ArithmeticException` escape `SetPublicKey`), a modulus under 4 bytes (a division by
//! zero in `Encrypt`), a cipher text shorter than its block count says, and a decrypted block whose plain text
//! starts with zero bytes (the C++ read before the block's bytes; the zeros are restored here).

use crate::bigint::{ArithError, BigInt};
use crate::der::{self, Extractor};
use crate::md5::Md5;
use crate::random::Random;

/// An ElGamal key with its own random generator.
#[derive(Clone, Debug)]
pub struct ElGamal {
    random: Random,
    p: BigInt,
    q: BigInt,
    g: BigInt,
    y: BigInt,
    x: BigInt,
    recip: BigInt,
    nb: i32,
    modulus_len: i32,
    is_private: bool,
    is_public: bool,
}

impl Default for ElGamal {
    fn default() -> Self {
        ElGamal::new()
    }
}

/// Appends `bytes` as exactly `len` bytes: the last `len` of them, or zeros in front.
fn append_fitted(out: &mut Vec<u8>, bytes: &[u8], len: usize) {
    if bytes.len() >= len {
        out.extend_from_slice(&bytes[bytes.len() - len..]);
    } else {
        out.resize(out.len() + len - bytes.len(), 0);
        out.extend_from_slice(bytes);
    }
}

impl ElGamal {
    /// `ElGamal()`: no key; the generator seeded from the system.
    pub fn new() -> ElGamal {
        ElGamal::with_random(Random::new())
    }

    /// No key, with a given generator (tests and golden comparisons).
    pub fn with_random(random: Random) -> ElGamal {
        ElGamal {
            random,
            p: BigInt::zero(),
            q: BigInt::zero(),
            g: BigInt::zero(),
            y: BigInt::zero(),
            x: BigInt::zero(),
            recip: BigInt::zero(),
            nb: 0,
            modulus_len: 0,
            is_private: false,
            is_public: false,
        }
    }

    /// Replaces the random generator.
    pub fn set_random(&mut self, random: Random) {
        self.random = random;
    }

    /// `Invalidate`.
    pub fn invalidate(&mut self) {
        self.is_private = false;
        self.is_public = false;
    }

    /// `IsPrivate`.
    pub fn is_private(&self) -> bool {
        self.is_private
    }

    /// `IsPublic`.
    pub fn is_public(&self) -> bool {
        self.is_public
    }

    fn load(&mut self, key: &[u8], private: bool) -> bool {
        self.invalidate();
        let mut e = Extractor::new(key, true);
        let mut next = |slot: &mut BigInt| match e.extract() {
            Some(v) => {
                *slot = v;
                true
            }
            None => false,
        };
        let ok = next(&mut self.p)
            && next(&mut self.q)
            && next(&mut self.g)
            && next(&mut self.y)
            && (!private || next(&mut self.x));
        if !ok {
            return false;
        }
        self.modulus_len = self.p.bit_length() / 8;
        match self.p.recip() {
            Ok((r, nb)) => {
                self.recip = r;
                self.nb = i32::from(nb);
            }
            Err(_) => return false,
        }
        self.is_private = private;
        self.is_public = true;
        true
    }

    /// `SetPrivateKey`: p, q, g, y, x.
    pub fn set_private_key(&mut self, key: &[u8]) -> bool {
        self.load(key, true)
    }

    /// `SetPublicKey`: p, q, g, y.
    pub fn set_public_key(&mut self, key: &[u8]) -> bool {
        self.load(key, false)
    }

    /// `GetRawPrivateKey`.
    pub fn raw_private_key(&self) -> Option<Vec<u8>> {
        self.is_private.then(|| der::insert(&[&self.p, &self.q, &self.g, &self.y, &self.x]))
    }

    /// `GetRawPublicKey`.
    pub fn raw_public_key(&self) -> Option<Vec<u8>> {
        self.is_public.then(|| der::insert(&[&self.p, &self.q, &self.g, &self.y]))
    }

    fn nb(&self) -> i16 {
        self.nb as i16
    }

    fn encrypt_value(&mut self, m: &BigInt) -> Result<(BigInt, BigInt), ArithError> {
        let p_minus_1 = self.p.sub(&BigInt::one());
        let mut k;
        loop {
            k = BigInt::random_bits(self.p.bit_length() - 1, &mut self.random);
            if !k.bit(0) {
                k.set_bit(0);
            }
            if k.gcd(&p_minus_1).equals(&BigInt::one()) {
                break;
            }
        }
        let a = self.g.mod_exp_recip(&k, &self.p, &self.recip, self.nb())?;
        let b = self.y.mod_exp_recip(&k, &self.p, &self.recip, self.nb())?.mod_mul_recip(
            m,
            &self.p,
            &self.recip,
            self.nb(),
        )?;
        Ok((a, b))
    }

    /// `Encrypt`: a 4-byte block count, then each block of up to modulus length - 3 bytes as two modulus-length
    /// values. None without a public key.
    pub fn encrypt(&mut self, plain: &[u8]) -> Option<Vec<u8>> {
        if !self.is_public {
            return None;
        }
        let ml = self.modulus_len;
        let block_len = ml - 3;
        if block_len <= 0 {
            return None;
        }
        let total = plain.len() as i32;
        let blocks = total / block_len + i32::from(total % block_len != 0);
        let mut out = Vec::with_capacity(4 + (ml * 2 * blocks) as usize);
        out.extend_from_slice(&blocks.to_le_bytes());
        let mut offset = 0usize;
        while offset < plain.len() {
            let this_len = (block_len as usize).min(plain.len() - offset);
            let ml = ml as usize;
            let mut block = vec![0u8; ml - 1];
            block[ml - 2 - this_len..ml - 2].copy_from_slice(&plain[offset..offset + this_len]);
            block[ml - 2] = this_len as u8;
            let (a, b) = self.encrypt_value(&BigInt::from_binary(&block)).ok()?;
            append_fitted(&mut out, &a.to_binary(), ml);
            append_fitted(&mut out, &b.to_binary(), ml);
            offset += this_len;
        }
        Some(out)
    }

    /// `Decrypt`: the inverse of `encrypt`. None without a private key or for malformed input.
    pub fn decrypt(&self, cipher: &[u8]) -> Option<Vec<u8>> {
        if !self.is_private || cipher.len() < 4 {
            return None;
        }
        let ml = self.modulus_len;
        let blocks = i32::from_le_bytes([cipher[0], cipher[1], cipher[2], cipher[3]]);
        if (cipher.len() as i32 - 4) < blocks.wrapping_mul(ml).wrapping_mul(2).wrapping_sub(4) {
            return None;
        }
        let ml = ml as usize;
        let mut out = Vec::new();
        let mut at = 4usize;
        for _ in 0..blocks.max(0) {
            if at + 2 * ml > cipher.len() {
                return None;
            }
            let a = BigInt::from_binary(&cipher[at..at + ml]);
            let b = BigInt::from_binary(&cipher[at + ml..at + 2 * ml]);
            at += 2 * ml;
            let plain = a
                .mod_exp_recip(&self.x, &self.p, &self.recip, self.nb())
                .and_then(|s| s.inverse_mod_n(&self.p))
                .and_then(|s| b.mod_mul_recip(&s, &self.p, &self.recip, self.nb()))
                .ok()?;
            let bytes = plain.to_binary();
            let plain_len = usize::from(bytes[bytes.len() - 1]);
            if plain_len as i64 > ml as i64 - 3 {
                return None;
            }
            let body = &bytes[..bytes.len() - 1];
            if body.len() >= plain_len {
                out.extend_from_slice(&body[body.len() - plain_len..]);
            } else {
                out.resize(out.len() + plain_len - body.len(), 0);
                out.extend_from_slice(body);
            }
        }
        Some(out)
    }

    /// `EncodeDigest`: the digest as an integer, cut to q's bit length minus one when it is longer.
    fn encode_digest(&self, digest: &[u8]) -> Option<BigInt> {
        let mut e = Extractor::new(digest, false);
        if (digest.len() as i32) * 8 < self.q.bit_length() {
            e.decode(digest.len() as i32)
        } else {
            let ql = self.q.byte_length() as i32;
            let mut h = e.decode(ql)?;
            h.shift_right(ql * 8 - self.q.bit_length() + 1);
            Some(h)
        }
    }

    fn bogus_verify(&self, m: &BigInt, a: &BigInt, b: &BigInt) -> Result<bool, ArithError> {
        if a.cmp_signed(&BigInt::zero()) == 0 {
            return Ok(false);
        }
        let gbq = self.g.mod_exp_recip(b, &self.p, &self.recip, self.nb())?;
        let yaq = self.y.mod_exp_recip(a, &self.p, &self.recip, self.nb())?;
        let result = gbq.mod_mul_recip(&yaq, &self.p, &self.recip, self.nb())?.add(m).modulo(&self.q);
        Ok(a.cmp_signed(&result) == 0)
    }

    /// `Verify`: the signature (a and b, q's byte length each) over the message's MD5 digest.
    pub fn verify(&self, message: &[u8], signature: &[u8]) -> bool {
        if !self.is_public {
            return false;
        }
        let ql = self.q.byte_length();
        if signature.len() != ql * 2 {
            return false;
        }
        let Some(m) = self.encode_digest(&Md5::digest(message)) else {
            return false;
        };
        let a = BigInt::from_binary(&signature[..ql]);
        let b = BigInt::from_binary(&signature[ql..]);
        self.bogus_verify(&m, &a, &b).unwrap_or(false)
    }

    fn rand_between(&mut self, min: &BigInt, max: &BigInt) -> BigInt {
        let range = max.sub(min);
        let bits = range.bit_length();
        loop {
            let v = BigInt::random_bits(bits, &mut self.random);
            if v.cmp_signed(&range) <= 0 {
                return v.add(min);
            }
        }
    }

    fn bogus_sign(&mut self, m: &BigInt) -> Result<(BigInt, BigInt), ArithError> {
        let two = BigInt::from_i32(2);
        loop {
            let max = self.q.sub(&two);
            let k = self.rand_between(&two, &max);
            let a = self.g.mod_exp(&k, &self.p)?.add(m).modulo(&self.p).modulo(&self.q);
            let b = k.sub(&self.x.mul(&a)).modulo(&self.q);
            if a.cmp_signed(&BigInt::zero()) != 0 {
                return Ok((a, b));
            }
        }
    }

    /// `Sign`: a and b of the message's MD5 digest, q's byte length each. None without a private key.
    pub fn sign(&mut self, message: &[u8]) -> Option<Vec<u8>> {
        if !self.is_private {
            return None;
        }
        let m = self.encode_digest(&Md5::digest(message))?;
        let (a, b) = self.bogus_sign(&m).ok()?;
        let ql = self.q.byte_length();
        let mut out = Vec::with_capacity(2 * ql);
        append_fitted(&mut out, &a.to_binary(), ql);
        append_fitted(&mut out, &b.to_binary(), ql);
        Some(out)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A toy key: q = 1019, p = 1019 m + 1 = 1073761061, g = h^((p-1)/q) of order q, x = 77.
    fn toy() -> (Vec<u8>, Vec<u8>) {
        let p = BigInt::from_i32(1_073_761_061);
        let q = BigInt::from_i32(1019);
        let g = BigInt::from_i32(611_659_888);
        let x = BigInt::from_i32(77);
        let y = g.mod_exp(&x, &p).unwrap();
        (der::insert(&[&p, &q, &g, &y, &x]), der::insert(&[&p, &q, &g, &y]))
    }

    #[test]
    fn keys_load_and_write_back() {
        let (private, public) = toy();
        let mut k = ElGamal::with_random(Random::from_seed(b"t"));
        assert!(!k.is_public() && k.raw_public_key().is_none());
        assert!(k.set_private_key(&private));
        assert!(k.is_private() && k.is_public());
        assert_eq!(k.raw_private_key().unwrap(), private);
        assert_eq!(k.raw_public_key().unwrap(), public);
        let mut pk = ElGamal::with_random(Random::from_seed(b"u"));
        assert!(pk.set_public_key(&public));
        assert!(!pk.is_private() && pk.raw_private_key().is_none());
        assert!(!pk.set_public_key(&public[..public.len() - 1]));
        assert!(!pk.is_public());
        // a zero modulus has no reciprocal: refused instead of the C++ exception
        let zero = BigInt::zero();
        assert!(!pk.set_public_key(&der::insert(&[&zero, &zero, &zero, &zero])));
    }

    #[test]
    fn too_small_modulus_cannot_encrypt() {
        let (private, _) = toy();
        let mut k = ElGamal::with_random(Random::from_seed(b"t"));
        k.set_private_key(&private);
        // p has 31 bits: a modulus length of 3 bytes leaves no room for a block
        assert_eq!(k.encrypt(b"hi"), None);
        assert!(k.sign(b"msg").is_some());
    }

    #[test]
    fn toy_signatures_verify() {
        let (private, public) = toy();
        let mut k = ElGamal::with_random(Random::from_seed(b"sig"));
        k.set_private_key(&private);
        let mut v = ElGamal::with_random(Random::from_seed(b"v"));
        v.set_public_key(&public);
        // q has 10 bits: the digest is cut to its top 9 bits, so about one forged message in 512 passes
        let mut forged = 0;
        for n in 0..20u8 {
            let msg = vec![n; usize::from(n)];
            let sig = k.sign(&msg).unwrap();
            assert_eq!(sig.len(), 4);
            assert!(v.verify(&msg, &sig), "message {n}");
            forged += usize::from(v.verify(b"other", &sig));
        }
        assert!(forged < 3);
        assert!(!v.verify(b"x", &[0, 0, 0]));
        assert!(!v.verify(b"x", &[0, 0, 0, 1]));
    }
}
