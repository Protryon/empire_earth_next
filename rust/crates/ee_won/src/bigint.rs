//! WONCrypt's BigInteger: sign and magnitude in 32-bit words, least significant first.
//!
//! A port of `BigInteger*.cpp`. Each `st_*` function there writes into a target object, and some leave the target's
//! sign as it was (the shifts keep it, a fresh target is positive); the functions below take the target the same way
//! (`&mut BigInt`), so those rules carry over. Where the C++ passed the target as an operand too, the caller here
//! passes a copy.
//!
//! One deliberate difference: `st_kmul`'s Karatsuba recursion is replaced by the schoolbook product. The C++ truncated
//! `a[N-1]*b[N-1]` to 32 bits for odd word counts above 16, read operand words past their length and wrote past its
//! work buffer (AddressSanitizer: heap-buffer-overflow), so its product was wrong or depended on uninitialised memory
//! there; everywhere else it is the exact product, which is what this gives (PORT_LEARNINGS "R6: WON's leaves").

use std::cmp::Ordering;

use crate::random::Random;

/// An arithmetic error: the C++ threw `ArithmeticException`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ArithError(pub &'static str);

/// A signed big integer. `n` holds exactly the significant words (no leading zero word); zero is `n` empty, and
/// it can carry the negative flag (`negate()` of zero), as in the C++.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct BigInt {
    n: Vec<u32>,
    negative: bool,
}

fn trim(n: &mut Vec<u32>) {
    while n.last() == Some(&0) {
        n.pop();
    }
}

fn ucmp_words(a: &[u32], b: &[u32]) -> i32 {
    if a.len() != b.len() {
        return if a.len() > b.len() { 1 } else { -1 };
    }
    for i in (0..a.len()).rev() {
        if a[i] != b[i] {
            return if a[i] > b[i] { 1 } else { -1 };
        }
    }
    0
}

/// |a| + |b|, exact.
fn add_words(a: &[u32], b: &[u32]) -> Vec<u32> {
    let (a, b) = if a.len() < b.len() { (b, a) } else { (a, b) };
    let mut r = Vec::with_capacity(a.len() + 1);
    let mut carry = 0u64;
    for (i, &ai) in a.iter().enumerate() {
        let s = u64::from(ai) + u64::from(*b.get(i).unwrap_or(&0)) + carry;
        r.push(s as u32);
        carry = s >> 32;
    }
    if carry != 0 {
        r.push(carry as u32);
    }
    r
}

/// |a| - |b| for |a| >= |b|, trimmed.
fn sub_words(a: &[u32], b: &[u32]) -> Vec<u32> {
    let mut r = Vec::with_capacity(a.len());
    let mut borrow = 0u64;
    for (i, &ai) in a.iter().enumerate() {
        let d = u64::from(ai).wrapping_sub(u64::from(*b.get(i).unwrap_or(&0))).wrapping_sub(borrow);
        r.push(d as u32);
        borrow = (d >> 32).wrapping_neg() & 1;
    }
    trim(&mut r);
    r
}

fn mul_words(a: &[u32], b: &[u32]) -> Vec<u32> {
    let mut r = vec![0u32; a.len() + b.len()];
    for (i, &ai) in a.iter().enumerate() {
        let mut carry = 0u64;
        for (j, &bj) in b.iter().enumerate() {
            let m = u64::from(r[i + j]) + u64::from(bj) * u64::from(ai) + carry;
            r[i + j] = m as u32;
            carry = m >> 32;
        }
        r[i + b.len()] = carry as u32;
    }
    trim(&mut r);
    r
}

impl BigInt {
    /// Zero.
    pub fn zero() -> BigInt {
        BigInt::default()
    }

    /// One.
    pub fn one() -> BigInt {
        BigInt { n: vec![1], negative: false }
    }

    /// `BigInteger(int)`.
    pub fn from_i32(v: i32) -> BigInt {
        let mut n = vec![v.unsigned_abs()];
        trim(&mut n);
        BigInt { n, negative: v < 0 }
    }

    /// `fromBinary`: big-endian unsigned bytes; the result is positive.
    pub fn from_binary(bytes: &[u8]) -> BigInt {
        let mut n = vec![0u32; bytes.len().div_ceil(4)];
        for (i, &b) in bytes.iter().rev().enumerate() {
            n[i / 4] |= u32::from(b) << (8 * (i % 4));
        }
        trim(&mut n);
        BigInt { n, negative: false }
    }

    /// `toBinary`: the magnitude in big-endian bytes, with a leading zero byte when the value is not negative and
    /// the top byte's high bit is set; zero is one zero byte.
    pub fn to_binary(&self) -> Vec<u8> {
        let count = self.byte_length();
        let mut out = Vec::with_capacity(count + 1);
        if count == 0 {
            out.push(0);
        }
        for i in 0..count {
            out.push((self.n[i / 4] >> (8 * (i % 4))) as u8);
        }
        if !self.negative && out[out.len() - 1] & 0x80 != 0 {
            out.push(0);
        }
        out.reverse();
        out
    }

    /// The words, least significant first.
    pub fn words(&self) -> &[u32] {
        &self.n
    }

    /// The sign flag.
    pub fn is_negative(&self) -> bool {
        self.negative
    }

    /// No significant word.
    pub fn is_zero(&self) -> bool {
        self.n.is_empty()
    }

    fn low_word(&self) -> u32 {
        self.n.first().copied().unwrap_or(0)
    }

    /// `bitLength`.
    pub fn bit_length(&self) -> i32 {
        match self.n.last() {
            None => 0,
            Some(&top) => (self.n.len() as i32 - 1) * 32 + (32 - top.leading_zeros() as i32),
        }
    }

    /// `byteLength`.
    pub fn byte_length(&self) -> usize {
        (self.bit_length() as usize).div_ceil(8)
    }

    /// `st_bit`: false past the length.
    pub fn bit(&self, i: i32) -> bool {
        let w = (i / 32) as usize;
        w < self.n.len() && self.n[w] & (1u32 << (i % 32)) != 0
    }

    /// `st_setBit`: does nothing past the length.
    pub fn set_bit(&mut self, i: i32) {
        let w = (i / 32) as usize;
        if w < self.n.len() {
            self.n[w] |= 1u32 << (i % 32);
        }
    }

    /// `negate`.
    pub fn negate(&self) -> BigInt {
        BigInt { n: self.n.clone(), negative: !self.negative }
    }

    /// `st_cmp`: signed comparison (-1, 0, 1). Two zeros compare equal; a negative zero is below a positive value.
    pub fn cmp_signed(&self, b: &BigInt) -> i32 {
        if self.n.is_empty() && b.n.is_empty() {
            return 0;
        }
        match (self.negative, b.negative) {
            (true, true) => ucmp_words(&b.n, &self.n),
            (true, false) => -1,
            (false, true) => 1,
            (false, false) => ucmp_words(&self.n, &b.n),
        }
    }

    /// `equals`.
    pub fn equals(&self, b: &BigInt) -> bool {
        self.cmp_signed(b) == 0
    }

    /// `BigInteger(numBits, Random&)`: `numBits` random bits (`st_randomBits`), as an unsigned value.
    pub fn random_bits(num_bits: i32, rng: &mut Random) -> BigInt {
        let num_bytes = ((num_bits + 7) / 8).max(0) as usize;
        let mut bytes = vec![0u8; num_bytes];
        if num_bytes > 0 {
            rng.next_bytes(&mut bytes);
            let excess = 8 * num_bytes as i32 - num_bits;
            bytes[0] &= ((1i32 << (8 - excess)) - 1) as u8;
        }
        BigInt::from_binary(&bytes)
    }
}

// ---- the st_ functions: r is the target object

fn st_zero(r: &mut BigInt) {
    r.n.clear();
    r.negative = false;
}

fn st_one(r: &mut BigInt) {
    r.n = vec![1];
    r.negative = false;
}

fn st_add_unsigned(r: &mut BigInt, a: &BigInt, b: &BigInt) {
    r.n = add_words(&a.n, &b.n);
    r.negative = false;
}

/// |a| - |b|; with `determine_sign` the sign is that of the difference, otherwise the target's stays.
fn st_sub_unsigned(r: &mut BigInt, a: &BigInt, b: &BigInt, determine_sign: bool) {
    match ucmp_words(&a.n, &b.n) {
        0 => st_zero(r),
        -1 => {
            if determine_sign {
                r.negative = true;
            }
            r.n = sub_words(&b.n, &a.n);
        }
        _ => {
            if determine_sign {
                r.negative = false;
            }
            r.n = sub_words(&a.n, &b.n);
        }
    }
}

fn st_add(r: &mut BigInt, a: &BigInt, b: &BigInt) {
    if a.n.is_empty() {
        *r = b.clone();
        return;
    }
    if b.n.is_empty() {
        *r = a.clone();
        return;
    }
    match (a.negative, b.negative) {
        (true, true) => {
            st_add_unsigned(r, a, b);
            r.negative = true;
        }
        (true, false) => st_sub_unsigned(r, b, a, true),
        (false, true) => st_sub_unsigned(r, a, b, true),
        (false, false) => st_add_unsigned(r, a, b),
    }
}

fn st_sub(r: &mut BigInt, a: &BigInt, b: &BigInt) {
    if a.n.is_empty() {
        *r = b.clone();
        if !b.n.is_empty() {
            r.negative = !b.negative;
        }
        return;
    }
    if b.n.is_empty() {
        *r = a.clone();
        return;
    }
    match (a.negative, b.negative) {
        (true, true) => st_sub_unsigned(r, b, a, true),
        (true, false) => {
            st_add_unsigned(r, b, a);
            r.negative = true;
        }
        (false, true) => st_add_unsigned(r, a, b),
        (false, false) => st_sub_unsigned(r, a, b, true),
    }
}

/// a << n; the target keeps its sign (zero input: zero, positive).
fn st_shift_left(r: &mut BigInt, a: &BigInt, n: i32) {
    if a.n.is_empty() {
        st_zero(r);
        return;
    }
    let rem = (n % 32) as u32;
    let blocks = (n / 32) as usize;
    let mut out = vec![0u32; blocks];
    out.extend_from_slice(&a.n);
    if rem != 0 {
        let mut carry = 0u32;
        for w in out.iter_mut().skip(blocks) {
            let l = *w;
            *w = (l << rem) | carry;
            carry = l >> (32 - rem);
        }
        if carry != 0 {
            out.push(carry);
        }
    }
    r.n = out;
}

/// a >> n; the target keeps its sign (everything shifted out: zero, positive).
fn st_shift_right(r: &mut BigInt, a: &BigInt, n: i32) {
    let rem = (n % 32) as u32;
    let blocks = (n / 32) as usize;
    if blocks >= a.n.len() {
        st_zero(r);
        return;
    }
    let mut out = a.n[blocks..].to_vec();
    if rem != 0 {
        let mut carry = 0u32;
        for w in out.iter_mut().rev() {
            let l = *w;
            *w = (l >> rem) | carry;
            carry = l << (32 - rem);
        }
        if out.last() == Some(&0) {
            out.pop();
        }
    }
    r.n = out;
}

fn st_mul(r: &mut BigInt, a: &BigInt, b: &BigInt) {
    if a.n.is_empty() || b.n.is_empty() {
        st_zero(r);
        return;
    }
    r.negative = a.negative ^ b.negative;
    r.n = mul_words(&a.n, &b.n);
}

/// `st_kmul`: the product (see the module comment).
fn st_kmul(r: &mut BigInt, a: &BigInt, b: &BigInt) {
    st_mul(r, a, b);
}

fn st_mod(r: &mut BigInt, m: &BigInt, d: &BigInt) {
    *r = m.clone();
    if ucmp_words(&m.n, &d.n) < 0 {
        if m.negative {
            r.negative = true;
            let t = r.clone();
            st_add(r, d, &t);
        }
        return;
    }
    r.negative = false;
    let mut i = m.bit_length() - d.bit_length();
    let mut ds = BigInt::zero();
    st_shift_left(&mut ds, d, i);
    while i >= 0 {
        if r.cmp_signed(&ds) >= 0 {
            let t = r.clone();
            st_sub(r, &t, &ds);
        }
        let t = ds.clone();
        st_shift_right(&mut ds, &t, 1);
        i -= 1;
    }
    if m.negative {
        r.negative = true;
        let t = r.clone();
        st_add(r, d, &t);
    }
}

/// `st_div(dv, rem, m, d)`.
fn st_div(dv: &mut BigInt, rem: &mut BigInt, m: &BigInt, d: &BigInt) -> Result<(), ArithError> {
    if d.n.is_empty() {
        return Err(ArithError("div: Divide by zero"));
    }
    if m.cmp_signed(d) < 0 {
        *rem = m.clone();
        st_zero(dv);
        return Ok(());
    }
    let mut ds = BigInt::zero();
    *rem = m.clone();
    st_zero(dv);
    let mut i = m.bit_length() - d.bit_length();
    st_shift_left(&mut ds, d, i);
    while i >= 0 {
        if dv.n.is_empty() {
            if rem.cmp_signed(&ds) >= 0 {
                st_one(dv);
                let t = rem.clone();
                st_sub(rem, &t, &ds);
            }
        } else {
            let t = dv.clone();
            st_shift_left(dv, &t, 1);
            if rem.cmp_signed(&ds) >= 0 {
                dv.n[0] |= 1;
                let t = rem.clone();
                st_sub(rem, &t, &ds);
            }
        }
        let t = ds.clone();
        st_shift_right(&mut ds, &t, 1);
        i -= 1;
    }
    dv.negative = m.negative ^ d.negative;
    Ok(())
}

fn st_recip(r: &mut BigInt, m: &BigInt) -> Result<i32, ArithError> {
    let mut t = BigInt::one();
    let mbits = m.bit_length();
    let one = t.clone();
    st_shift_left(&mut t, &one, 2 * mbits);
    let mut rem = BigInt::zero();
    st_div(r, &mut rem, &t, m)?;
    Ok(mbits + 1)
}

fn st_mod_mul_recip(r: &mut BigInt, x: &BigInt, y: &BigInt, m: &BigInt, i: &BigInt, nb: i16) -> Result<(), ArithError> {
    let (mut a, mut b, mut c, mut d) = (BigInt::zero(), BigInt::zero(), BigInt::zero(), BigInt::zero());
    st_kmul(&mut a, x, y);
    st_shift_right(&mut d, &a, i32::from(nb) - 1);
    st_kmul(&mut b, &d, i);
    st_shift_right(&mut c, &b, i32::from(nb) - 1);
    st_kmul(&mut b, m, &c);
    st_sub(r, &a, &b);
    let mut j = 0;
    while r.cmp_signed(m) >= 0 {
        if j > 2 {
            return Err(ArithError("modMulRecip"));
        }
        j += 1;
        let t = r.clone();
        st_sub(r, &t, m);
    }
    Ok(())
}

fn st_inverse_mod_n(r: &mut BigInt, a: &BigInt, n: &BigInt) -> Result<(), ArithError> {
    if a.negative || n.negative {
        return Err(ArithError("inverseModN"));
    }
    let (mut x1, mut x2, mut x3) = (BigInt::one(), BigInt::zero(), a.clone());
    let (mut y1, mut y2, mut y3) = (BigInt::zero(), BigInt::one(), n.clone());
    let (mut t1, mut t2, mut t3) = (BigInt::zero(), BigInt::zero(), BigInt::zero());
    let (mut q, mut p) = (BigInt::zero(), BigInt::zero());
    while !y3.n.is_empty() {
        st_div(&mut q, &mut t3, &x3, &y3)?;
        st_mul(&mut t1, &q, &y2);
        st_sub(&mut t2, &x2, &t1);
        st_mul(&mut p, &q, &y1);
        st_sub(&mut t1, &x1, &p);
        // rotate: x <- y, y <- t, t <- x
        std::mem::swap(&mut x1, &mut y1);
        std::mem::swap(&mut y1, &mut t1);
        std::mem::swap(&mut x2, &mut y2);
        std::mem::swap(&mut y2, &mut t2);
        std::mem::swap(&mut x3, &mut y3);
        std::mem::swap(&mut y3, &mut t3);
    }
    if x1.negative {
        let t = x1.clone();
        st_add(&mut x1, &t, n);
    }
    *r = x1;
    Ok(())
}

fn st_euclid(r: &mut BigInt, x: &BigInt, y: &BigInt) {
    let mut a = x.clone();
    let mut b = y.clone();
    let mut shifts = 0;
    while !b.n.is_empty() {
        let a_odd = a.low_word() & 1 != 0;
        let b_odd = b.low_word() & 1 != 0;
        if a_odd && b_odd {
            let t = a.clone();
            st_sub(&mut a, &t, &b);
            let t = a.clone();
            st_shift_right(&mut a, &t, 1);
        } else if a_odd {
            let t = b.clone();
            st_shift_right(&mut b, &t, 1);
        } else if b_odd {
            let t = a.clone();
            st_shift_right(&mut a, &t, 1);
        } else {
            let t = a.clone();
            st_shift_right(&mut a, &t, 1);
            let t = b.clone();
            st_shift_right(&mut b, &t, 1);
            shifts += 1;
            continue;
        }
        if a.cmp_signed(&b) < 0 {
            std::mem::swap(&mut a, &mut b);
        }
    }
    if shifts > 0 {
        st_shift_left(r, &a, shifts);
    } else {
        *r = a;
    }
}

fn st_mod_exp_recip(
    r: &mut BigInt,
    a: &BigInt,
    power: &BigInt,
    modulo: &BigInt,
    d: &BigInt,
    nb: i16,
) -> Result<(), ArithError> {
    let mut v = BigInt::zero();
    st_mod(&mut v, a, modulo);
    let bits = power.bit_length();
    if power.low_word() & 1 != 0 {
        st_mod(r, a, modulo);
    } else {
        st_one(r);
    }
    for i in 1..bits {
        let t = v.clone();
        st_mod_mul_recip(&mut v, &t, &t, modulo, d, nb)?;
        if power.bit(i) {
            let t = r.clone();
            st_mod_mul_recip(r, &t, &v, modulo, d, nb)?;
        }
    }
    if power.negative {
        let t = r.clone();
        st_inverse_mod_n(r, &t, modulo)?;
    }
    Ok(())
}

// ---- the member API (value results; a fresh BigInteger is the target)

impl BigInt {
    /// `add`.
    pub fn add(&self, b: &BigInt) -> BigInt {
        let mut r = BigInt::zero();
        st_add(&mut r, self, b);
        r
    }

    /// `sub`.
    pub fn sub(&self, b: &BigInt) -> BigInt {
        let mut r = BigInt::zero();
        st_sub(&mut r, self, b);
        r
    }

    /// `mul`.
    pub fn mul(&self, b: &BigInt) -> BigInt {
        let mut r = BigInt::zero();
        st_mul(&mut r, self, b);
        r
    }

    /// `st_kmul` into a fresh target.
    pub fn kmul(&self, b: &BigInt) -> BigInt {
        let mut r = BigInt::zero();
        st_kmul(&mut r, self, b);
        r
    }

    /// `mod`: shift-and-subtract; a negative value gives `b` minus the reduced magnitude.
    pub fn modulo(&self, b: &BigInt) -> BigInt {
        let mut r = BigInt::zero();
        st_mod(&mut r, self, b);
        r
    }

    /// `divrem`: (quotient, remainder).
    pub fn divrem(&self, b: &BigInt) -> Result<(BigInt, BigInt), ArithError> {
        let (mut q, mut r) = (BigInt::zero(), BigInt::zero());
        st_div(&mut q, &mut r, self, b)?;
        Ok((q, r))
    }

    /// `div`.
    pub fn div(&self, b: &BigInt) -> Result<BigInt, ArithError> {
        Ok(self.divrem(b)?.0)
    }

    /// `shiftLeft` (in place, keeps the sign).
    pub fn shift_left(&mut self, n: i32) {
        let t = self.clone();
        st_shift_left(self, &t, n);
    }

    /// `shiftRight` (in place, keeps the sign).
    pub fn shift_right(&mut self, n: i32) {
        let t = self.clone();
        st_shift_right(self, &t, n);
    }

    /// `gcd` (`st_gcd`: the binary Euclid with the larger value first). Positive operands only: with a negative
    /// one the C++ never returned.
    pub fn gcd(&self, b: &BigInt) -> BigInt {
        let mut r = BigInt::zero();
        if self.cmp_signed(b) > 0 {
            st_euclid(&mut r, self, b);
        } else {
            st_euclid(&mut r, b, self);
        }
        r
    }

    /// `recip`: (2^(2 * bits) / self, bits + 1), the reduction constants of `mod_mul_recip`.
    pub fn recip(&self) -> Result<(BigInt, i16), ArithError> {
        let mut r = BigInt::zero();
        let nb = st_recip(&mut r, self)?;
        Ok((r, nb as i16))
    }

    /// `modMulRecip`: self * y mod m by Barrett reduction with `recip`'s constants.
    pub fn mod_mul_recip(&self, y: &BigInt, m: &BigInt, recip: &BigInt, nb: i16) -> Result<BigInt, ArithError> {
        let mut r = BigInt::zero();
        st_mod_mul_recip(&mut r, self, y, m, recip, nb)?;
        Ok(r)
    }

    /// `modExp(power, modulo, recip, nb)`.
    pub fn mod_exp_recip(
        &self,
        power: &BigInt,
        modulo: &BigInt,
        recip: &BigInt,
        nb: i16,
    ) -> Result<BigInt, ArithError> {
        let mut r = BigInt::zero();
        st_mod_exp_recip(&mut r, self, power, modulo, recip, nb)?;
        Ok(r)
    }

    /// `modExp(power, modulo)`: computes the constants first.
    pub fn mod_exp(&self, power: &BigInt, modulo: &BigInt) -> Result<BigInt, ArithError> {
        let (d, nb) = modulo.recip()?;
        self.mod_exp_recip(power, modulo, &d, nb)
    }

    /// `inverseModN`.
    pub fn inverse_mod_n(&self, n: &BigInt) -> Result<BigInt, ArithError> {
        let mut r = BigInt::zero();
        st_inverse_mod_n(&mut r, self, n)?;
        Ok(r)
    }

    /// `toString`: decimal digits in groups of three. For a negative value the loop stops at once and prints the
    /// magnitude's low word (`%03d`), as the C++ did.
    pub fn to_decimal(&self) -> String {
        let thousand = BigInt::from_i32(1000);
        let mut s = String::new();
        let mut tmp = self.clone();
        while !tmp.equals(&BigInt::zero()) {
            let (q, r) = match tmp.divrem(&thousand) {
                Ok(v) => v,
                Err(_) => break,
            };
            s = format!("{:03}", r.low_word() as i32) + &s;
            tmp = q;
        }
        let digits = s.trim_start_matches('0');
        if digits.is_empty() {
            "0".into()
        } else {
            digits.into()
        }
    }

    /// `fromString`: leading white space, then decimal digits. As in the C++, the groups of three are read at
    /// positions counted from the start of the text but bounded by the number of digits, with `atoi`.
    pub fn from_decimal(text: &[u8]) -> BigInt {
        let mut pos = 0;
        while pos < text.len() && text[pos].is_ascii_whitespace() {
            pos += 1;
        }
        let mut last = pos;
        while last < text.len() && text[last].is_ascii_digit() {
            last += 1;
        }
        let count = last - pos;
        let thousand = BigInt::from_i32(1000);
        let mut r1 = BigInt::zero();
        let mut r2 = BigInt::zero();
        if count % 3 != 0 {
            r1 = BigInt::from_i32(c_atoi(&text[pos..pos + count % 3]));
            r2 = r1.clone();
            pos += count % 3;
        }
        while pos < count {
            let group = BigInt::from_i32(c_atoi(&text[pos.min(text.len())..(pos + 3).min(text.len())]));
            r1 = thousand.mul(&r2).add(&group);
            r2 = r1.clone();
            pos += 3;
        }
        r1
    }
}

/// C's `atoi`: white space, an optional sign, digits (wrapping).
fn c_atoi(s: &[u8]) -> i32 {
    let mut i = 0;
    while i < s.len() && s[i].is_ascii_whitespace() {
        i += 1;
    }
    let mut negative = false;
    if i < s.len() && (s[i] == b'+' || s[i] == b'-') {
        negative = s[i] == b'-';
        i += 1;
    }
    let mut v = 0i32;
    while i < s.len() && s[i].is_ascii_digit() {
        v = v.wrapping_mul(10).wrapping_add(i32::from(s[i] - b'0'));
        i += 1;
    }
    if negative {
        v.wrapping_neg()
    } else {
        v
    }
}

impl PartialOrd for BigInt {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp_signed(other).cmp(&0))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn big(hex: &str) -> BigInt {
        let bytes: Vec<u8> =
            (0..hex.len()).step_by(2).map(|i| u8::from_str_radix(&hex[i..i + 2], 16).unwrap()).collect();
        BigInt::from_binary(&bytes)
    }

    #[test]
    fn binary_round_trip_and_sign_byte() {
        assert_eq!(BigInt::zero().to_binary(), vec![0]);
        assert_eq!(big("80").to_binary(), vec![0, 0x80]);
        assert_eq!(big("80").negate().to_binary(), vec![0x80]);
        assert_eq!(big("0000123456789a").to_binary(), vec![0x12, 0x34, 0x56, 0x78, 0x9a]);
        assert_eq!(big("0000123456789a").words(), &[0x3456789a, 0x12]);
        assert_eq!(big("ffffffff").bit_length(), 32);
        assert_eq!(big("0100000000").bit_length(), 33);
    }

    #[test]
    fn signed_add_sub_follow_the_c_rules() {
        let a = BigInt::from_i32(5);
        let b = BigInt::from_i32(-7);
        assert_eq!(a.add(&b), BigInt::from_i32(-2));
        assert_eq!(a.sub(&b), BigInt::from_i32(12));
        assert_eq!(b.sub(&a), BigInt::from_i32(-12));
        assert_eq!(BigInt::zero().sub(&a), BigInt::from_i32(-5));
        assert_eq!(a.sub(&a), BigInt::zero());
        // negative zero: below every positive value, equal to zero
        let nz = BigInt::zero().negate();
        assert!(nz.equals(&BigInt::zero()));
        assert_eq!(nz.cmp_signed(&a), -1);
    }

    #[test]
    fn shifts_keep_the_target_sign() {
        let mut a = BigInt::from_i32(-3);
        a.shift_left(33);
        assert_eq!(a.words(), &[0, 6]);
        assert!(a.is_negative());
        a.shift_right(34);
        assert_eq!(a.words(), &[1]);
        // shifted out within the length: zero, and the sign stays (a negative zero); past the length: positive zero
        a.shift_right(1);
        assert!(a.is_zero() && a.is_negative());
        a.shift_right(40);
        assert!(a.is_zero() && !a.is_negative());
    }

    #[test]
    fn division_and_modulo() {
        let m = big("0123456789abcdef0123456789");
        let d = big("0fedcba987");
        let (q, r) = m.divrem(&d).unwrap();
        assert_eq!(q.mul(&d).add(&r), m);
        assert!(r.cmp_signed(&d) < 0);
        assert_eq!(m.modulo(&d), r);
        assert_eq!(m.divrem(&BigInt::zero()), Err(ArithError("div: Divide by zero")));
        // a negative value: the modulus minus the reduced magnitude (and the modulus itself for a multiple)
        assert_eq!(BigInt::from_i32(-7).modulo(&BigInt::from_i32(5)), BigInt::from_i32(3));
        assert_eq!(BigInt::from_i32(-10).modulo(&BigInt::from_i32(5)), BigInt::from_i32(5));
    }

    #[test]
    fn modular_exponent_inverse_gcd() {
        let m = BigInt::from_i32(1_000_003);
        let a = BigInt::from_i32(12345);
        let e = BigInt::from_i32(65537);
        // 12345^65537 mod 1000003, computed independently (Python pow)
        assert_eq!(a.mod_exp(&e, &m).unwrap(), BigInt::from_i32(891_708));
        let inv = a.inverse_mod_n(&m).unwrap();
        assert_eq!(inv.mul(&a).modulo(&m), BigInt::one());
        assert!(a.negate().inverse_mod_n(&m).is_err());
        assert_eq!(BigInt::from_i32(462).gcd(&BigInt::from_i32(1071)), BigInt::from_i32(21));
        assert_eq!(BigInt::from_i32(48).gcd(&BigInt::from_i32(180)), BigInt::from_i32(12));
    }

    #[test]
    fn decimal_text() {
        assert_eq!(BigInt::from_decimal(b"  1000000000000000000001x").to_decimal(), "1000000000000000000001");
        assert_eq!(BigInt::from_decimal(b"0").to_decimal(), "0");
        assert_eq!(BigInt::from_decimal(b"abc").to_decimal(), "0");
        assert_eq!(BigInt::from_i32(-5).to_decimal(), "5");
        assert_eq!(BigInt::from_i32(-1234).to_decimal(), "1234");
    }

    #[test]
    fn random_bits_masks_the_top_byte() {
        let mut rng = Random::from_seed(&[1, 2, 3]);
        for bits in [1, 7, 8, 9, 31, 33, 160] {
            assert!(BigInt::random_bits(bits, &mut rng).bit_length() <= bits);
        }
        assert!(BigInt::random_bits(0, &mut rng).is_zero());
    }
}
