//! Golden vectors (`golden_won.txt`), small and synthetic.
//!
//! Most lines come from a throwaway program that linked the former C++ units (ReadBuffer, WriteBuffer, CRC16, MD5,
//! MD5Digest, Random, BigInteger, ElGamal, Blowfish, the auth blocks, CDKey), built with clang-cl like the game,
//! and once more with AddressSanitizer; only cases that ran clean and printed the same in both builds are kept.
//! Where the clang-built C++ was wrong, the lines document it and the test checks the corrected behaviour:
//!
//! - `MD5Digest` returned MD5's initial state for every input (`(unsigned)a >> -s` is undefined behaviour, and
//!   clang dropped the rounds; the original compiler masked the count to 32 - s). So the C++ `Random` and every
//!   ElGamal signature and key choice were broken too. The `py*` lines model WON's ElGamal and Random with exact
//!   integers and real MD5 (a Python oracle) and are the reference for them.
//! - `st_kmul`'s Karatsuba was wrong for odd word counts above 16 (`kmul` lines); this crate multiplies exactly.
//!
//! The `key` lines are two test keys made for this file (512- and 1024-bit p, 160-bit q).

use std::collections::HashMap;

use crate::auth::{AuthBlock, Body, Kind};
use crate::bigint::BigInt;
use crate::blowfish::Blowfish;
use crate::elgamal::ElGamal;
use crate::md5::Md5;
use crate::random::Random;
use crate::{cdkey, crc16, text, wire};

const GOLDEN: &str = include_str!("golden_won.txt");

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

fn hex(d: &[u8]) -> String {
    if d.is_empty() {
        return "-".into();
    }
    d.iter().map(|b| format!("{b:02x}")).collect()
}

fn opt_hex(d: Option<Vec<u8>>) -> String {
    d.map(|d| hex(&d)).unwrap_or_else(|| "NULL".into())
}

fn lines(kind: &str) -> Vec<Vec<&'static str>> {
    GOLDEN.lines().map(|l| l.split('\t').collect::<Vec<_>>()).filter(|f| f[0] == kind).collect()
}

/// `gen(n, seed)` of the generator: the LCG's bytes.
fn gen(n: usize, seed: u32) -> Vec<u8> {
    let mut s = seed;
    (0..n)
        .map(|_| {
            s = s.wrapping_mul(1103515245).wrapping_add(12345);
            (s >> 8) as u8
        })
        .collect()
}

/// The generator's printing of a BigInteger: the sign, then `toBinary`.
fn bi(v: &BigInt) -> String {
    format!("{}{}", if v.cmp_signed(&BigInt::zero()) < 0 { '-' } else { '+' }, hex(&v.to_binary()))
}

fn parse_bi(s: &str) -> BigInt {
    let v = BigInt::from_binary(&unhex(&s[1..]));
    if s.starts_with('-') {
        v.negate()
    } else {
        v
    }
}

#[test]
fn crc16_matches() {
    let rows = lines("crc16");
    assert_eq!(rows.len(), 42);
    for f in rows {
        let init = u16::from_str_radix(f[2], 16).unwrap();
        let xor = u16::from_str_radix(f[3], 16).unwrap();
        let want = u16::from_str_radix(f[4], 16).unwrap();
        assert_eq!(crc16::put(init, &unhex(f[1])) ^ xor, want, "{f:?}");
    }
}

#[test]
fn md5_matches_and_the_clang_md5digest_was_constant() {
    let rows = lines("md5");
    assert_eq!(rows.len(), 76);
    for f in rows {
        let d = unhex(f[1]);
        assert_eq!(hex(&Md5::digest(&d)), f[2]);
        assert_eq!(f[3], f[2]);
        assert_eq!(f[4], "0123456789abcdeffedcba9876543210", "the clang-built MD5Digest");
    }
}

fn random_line(r: &mut Random, f: &[&str]) {
    let mut out = String::new();
    for n in [1, 15, 16, 17, 40, 3] {
        let mut b = vec![0u8; n];
        r.next_bytes(&mut b);
        out += &hex(&b);
        out.push(',');
    }
    assert_eq!(out, f[2]);
    assert_eq!(format!("{:x},{:x},{:x}", r.next(12), r.next(32), r.next(1)), f[3]);
    r.set_seed(&unhex(f[4]));
    let mut after = [0u8; 20];
    r.next_bytes(&mut after);
    assert_eq!(hex(&after), f[5]);
}

#[test]
fn random_matches_the_oracle() {
    let rows = lines("pyrandom");
    assert_eq!(rows.len(), 4);
    for f in rows {
        random_line(&mut Random::from_seed(&unhex(f[1])), &f);
    }
    // the C++ lines: the same seeds; its MD5 gave the initial state, so its bytes repeat that state
    for f in lines("random") {
        assert!(f[2].starts_with("01,23456789abcdeffedcba9876543210,"));
    }
}

#[test]
fn blowfish_matches() {
    let keys = lines("bfkey");
    assert_eq!(keys.len(), 6);
    for f in &keys {
        assert_eq!(Blowfish::new().set_key(&unhex(f[1])), f[2] == "1", "{f:?}");
    }
    let rows = lines("bf");
    assert_eq!(rows.len(), 168);
    for f in rows {
        let mut bf = Blowfish::new();
        assert!(bf.set_key(&unhex(f[1])));
        let enc = bf.encrypt(&unhex(f[2])).unwrap();
        if f[3].ends_with("????????????????") {
            // 4 input bytes: the C++ left the second block uninitialised; it is zero here
            assert_eq!(hex(&enc[..8]), f[3][..16]);
            assert_eq!(&enc[8..], &[0; 8]);
        } else {
            assert_eq!(hex(&enc), f[3]);
        }
        assert_eq!(opt_hex(bf.decrypt(&enc)), f[4]);
    }
    let rows = lines("bfdec");
    assert_eq!(rows.len(), 40);
    for f in rows {
        let mut bf = Blowfish::new();
        bf.set_key(&unhex(f[1]));
        assert_eq!(opt_hex(bf.decrypt(&unhex(f[2]))), f[3], "{f:?}");
    }
}

#[test]
fn bigint_matches() {
    let rows = lines("bigint");
    assert_eq!(rows.len(), 120);
    for (t, f) in rows.iter().enumerate() {
        let (a, b) = (parse_bi(f[1]), parse_bi(f[2]));
        assert_eq!(bi(&a.add(&b)), f[3], "add {t}");
        assert_eq!(bi(&a.sub(&b)), f[4], "sub {t}");
        assert_eq!(bi(&a.mul(&b)), f[5], "mul {t}");
        if f[6] != "skip" {
            match a.divrem(&b) {
                Ok((q, r)) => assert_eq!((bi(&q), bi(&r)), (f[6].to_string(), f[7].to_string()), "divrem {t}"),
                Err(_) => assert_eq!(f[6], "EXC"),
            }
        }
        if f[8] != "skip" {
            assert_eq!(bi(&a.modulo(&b)), f[8], "mod {t}");
        }
        assert_eq!(a.cmp_signed(&b).to_string(), f[9]);
        assert_eq!(a.bit_length().to_string(), f[10]);
        assert_eq!(a.byte_length().to_string(), f[11]);
        let mut s = a.clone();
        s.shift_left(t as i32 % 70);
        assert_eq!(bi(&s), f[12], "shl {t}");
        let mut s = a.clone();
        s.shift_right(t as i32 % 70);
        assert_eq!(bi(&s), f[13], "shr {t}");
        if f[14] != "skip" {
            assert_eq!(bi(&a.gcd(&b)), f[14], "gcd {t}");
        }
        assert_eq!(a.to_decimal(), f[15], "toString {t}");
    }
}

#[test]
fn modular_arithmetic_matches() {
    let rows = lines("modarith");
    assert!(rows.len() >= 40);
    let mut corrected = 0;
    let ex = |r: Result<BigInt, crate::bigint::ArithError>| r.map(|v| bi(&v)).unwrap_or_else(|_| "EXC".into());
    for f in rows {
        let (m, a, e) = (parse_bi(f[2]), parse_bi(f[3]), parse_bi(f[4]));
        assert_eq!(ex(a.mod_exp(&e, &m)), f[5], "modExp {}", f[1]);
        let (rc, nb) = m.recip().unwrap();
        assert_eq!((bi(&rc), nb.to_string()), (f[6].to_string(), f[7].to_string()), "recip {}", f[1]);
        let b = parse_bi(f[8]);
        assert_eq!(ex(a.mod_mul_recip(&b, &m, &rc, nb)), f[9], "modMulRecip {}", f[1]);
        assert_eq!(ex(a.mod_exp_recip(&e, &m, &rc, nb)), f[10], "modExp recip {}", f[1]);
        assert_eq!(ex(a.inverse_mod_n(&m)), f[11], "inverseModN {}", f[1]);
        corrected += usize::from(f[12] != "cpp-ok");
    }
    // where the C++ product was wrong the line holds the exact value (computed in Python) and says so
    assert_eq!(corrected, 7);
}

#[test]
fn decimal_strings_match() {
    for f in lines("bigstr") {
        let v = BigInt::from_decimal(&unhex(f[1]));
        assert_eq!((bi(&v), v.to_decimal()), (f[2].to_string(), f[3].to_string()));
    }
}

#[test]
fn karatsuba_lines_show_the_cpp_error() {
    let rows = lines("kmul");
    assert_eq!(rows.len(), 7);
    for f in rows {
        let (a, b) = (parse_bi(f[1]), parse_bi(f[2]));
        let exact = a.kmul(&b);
        assert_eq!(exact, a.mul(&b));
        assert_ne!(bi(&exact), f[3], "the C++ value was wrong for odd word counts");
    }
}

fn keys() -> HashMap<&'static str, (Vec<u8>, Vec<u8>)> {
    lines("key").into_iter().map(|f| (f[1], (unhex(f[2]), unhex(f[3])))).collect()
}

#[test]
fn elgamal_keys_load_and_write_back() {
    let keys = keys();
    for f in lines("egkey") {
        let (private, public) = &keys[f[1]];
        let mut k = ElGamal::with_random(Random::from_seed(b"k"));
        assert!(k.set_private_key(private));
        assert_eq!(opt_hex(k.raw_private_key()), f[4]);
        let mut p = ElGamal::with_random(Random::from_seed(b"p"));
        assert!(p.set_public_key(public));
        assert_eq!(opt_hex(p.raw_public_key()), f[5]);
    }
    let f = &lines("egverifier")[0];
    let mut v = ElGamal::new();
    assert!(v.set_public_key(&unhex(f[1])));
    assert_eq!(opt_hex(v.raw_public_key()), f[3]);
    let vk = unhex(f[1]);
    for f in lines("egbadkey") {
        let cut: usize = f[1].parse().unwrap();
        assert_eq!(ElGamal::new().set_public_key(&vk[..cut]), f[2] == "1");
    }
}

#[test]
fn elgamal_matches_the_oracle() {
    let keys = keys();
    let rows = lines("pyeg");
    assert_eq!(rows.len(), 12);
    let mut state: HashMap<&str, (ElGamal, ElGamal)> = HashMap::new();
    for f in rows {
        let (private, public) = &keys[f[1]];
        let (signer, sealer) = state.entry(f[1]).or_insert_with(|| {
            let mut s = ElGamal::with_random(Random::from_seed(&gen(8, 42)));
            assert!(s.set_private_key(private));
            let mut p = ElGamal::with_random(Random::from_seed(&gen(8, 43)));
            assert!(p.set_public_key(public));
            (s, p)
        });
        let msg = unhex(f[2]);
        let enc = sealer.encrypt(&msg).unwrap();
        assert_eq!(hex(&enc), f[3], "encrypt {} {}", f[1], msg.len());
        assert_eq!(signer.decrypt(&enc).unwrap(), msg);
        let sig = signer.sign(&msg).unwrap();
        assert_eq!(hex(&sig), f[4], "sign {} {}", f[1], msg.len());
        assert!(sealer.verify(&msg, &sig));
        let mut other = msg.clone();
        other.push(1);
        assert!(!sealer.verify(&other, &sig));
        assert!(sealer.decrypt(&enc).is_none(), "a public key does not decrypt");
    }
    // the C++ cipher texts its own Decrypt read back are read back here too
    for f in lines("eg") {
        if f[4] == f[2] || (f[2] == "-" && f[4] == "-") {
            let mut k = ElGamal::new();
            k.set_private_key(&keys[f[1]].0);
            assert_eq!(hex(&k.decrypt(&unhex(f[3])).unwrap()), f[2]);
        }
    }
    for f in lines("egdec") {
        let mut k = ElGamal::new();
        k.set_private_key(&keys[f[1]].0);
        assert_eq!(opt_hex(k.decrypt(&unhex(f[2]))), f[3], "{f:?}");
    }
}

#[test]
fn wire_matches() {
    let rows = lines("prefix");
    assert_eq!(rows.len(), 100);
    for f in rows {
        let (fs, inc, len): (u8, bool, u32) = (f[1].parse().unwrap(), f[2] == "1", f[3].parse().unwrap());
        let data = gen(
            len as usize,
            40000
                + [2, 1, 5, 200, 254, 255, 256, 0xfffe, 0xffff, 0x10000].iter().position(|&l| l == len).unwrap() as u32,
        );
        let mut w = wire::Writer::new(fs, inc);
        w.append_bytes(&data);
        let msg = w.to_message();
        assert_eq!(hex(&msg[..msg.len().min(8)]), f[4], "{f:?}");
        assert_eq!(msg.len().to_string(), f[5]);
        // the capacity left behind: released (0) for even rows, else grown from 0 for the data
        let released = [2, 5, 254, 256, 0xffff].contains(&len);
        let cap = if released { 0 } else { wire::grown_capacity(0, 0, u64::from(len)).unwrap_or(0) };
        assert_eq!(cap.to_string(), f[6], "{f:?}");
    }
    let f = &lines("grow")[0];
    let (mut cap, mut len) = (0u64, 0u64);
    let mut caps = Vec::new();
    for add in [1, 31, 1, 30, 40, 100, 1, 500, 3000] {
        if let Some(c) = wire::grown_capacity(cap, len, add) {
            cap = c;
        }
        len += add;
        caps.push(cap.to_string());
    }
    if let Some(c) = wire::chunk_capacity(cap, 10000, 9000) {
        cap = c;
    }
    caps.push(cap.to_string());
    if let Some(c) = wire::chunk_capacity(cap, 20000, 0) {
        cap = c;
    }
    caps.push(cap.to_string());
    assert_eq!(caps.join(","), f[1]);

    let mut w = wire::Writer::new(2, true);
    w.append_byte(0x81);
    w.append_bool(true);
    w.append_bool(false);
    w.append_short(-2);
    w.append_long(0x12345678);
    w.append_string(b"abc", 2);
    w.append_string(b"xy", 1);
    w.append_string(b"", 4);
    w.append_bytes(b"raw");
    w.append_length(300, 1);
    w.append_length(70000, 2);
    w.append_length(5, 3);
    w.append_buffer(Some(b"hi\0"), 2);
    w.append_buffer(None, 1);
    w.append_buffer(Some(b"hi\0"), 0);
    w.set_bytes(0, &0x7766i16.to_le_bytes());
    w.set_bytes(5, &[0x55]);
    let ws = [0x57u16, 0xe9, 0xd83d, 0xde00];
    w.append_wstring(&ws, 1);
    w.append_wstring(&ws, 4);
    assert_eq!(hex(w.data()), lines("writes")[0][1]);

    let data = w.data().to_vec();
    let mut r = wire::Reader::new(&data);
    // the generator's printf arguments were evaluated right to left
    let first_bool = u8::from(r.read_bool().unwrap());
    let mut out = format!("{:02x},{},", r.read_byte().unwrap(), first_bool);
    let (l, s) = (r.read_long().unwrap(), r.read_short().unwrap());
    out += &format!("{},{},{:x},", u8::from(r.read_bool().unwrap()), s, l);
    for size in [2, 1, 4] {
        out += &String::from_utf8_lossy(r.read_string(size).unwrap());
        out.push(',');
    }
    out += &String::from_utf8_lossy(r.read_bytes(3).unwrap());
    out.push(',');
    for size in [1, 2, 3] {
        out += &format!("{:x},", r.read_length(size).unwrap());
    }
    out += &format!("{},", hex(r.read_string(2).unwrap()));
    out += &format!("{},", hex(r.read_string(1).unwrap()));
    out += &format!("{},{}", r.pos(), r.available());
    assert_eq!(r.read_bytes(1000), Err(wire::PastEnd));
    out += ",exc:Attempt to read past end of buffer.";
    assert_eq!(out, lines("reads")[0][1]);
    assert_eq!(text::utf32_from_utf16(&ws), vec![0x57, 0xe9, 0x1f600]);
}

#[test]
fn auth_blocks_match() {
    let f = &lines("cert1")[0];
    let c = AuthBlock::unpack(Kind::Certificate, &unhex(f[1]));
    assert_eq!(c.valid, f[2] == "1");
    assert_eq!(c.data_len.to_string(), f[3]);
    assert_eq!(c.user_id().to_string(), f[4]);
    assert_eq!((c.issue_time.to_string(), c.expire_time.to_string()), (f[5].to_string(), f[6].to_string()));
    let perms: String = [(77, 5, 9), (0, 0, 10), (78, 0, 0), (0, 6, 0)]
        .iter()
        .map(|&(u, c2, t)| if c.verify_permissions(u, c2, t) { '1' } else { '0' })
        .collect();
    assert_eq!(perms, f[7]);
    assert!(c.pub_key().unwrap().is_public());

    let f = &lines("cert2")[0];
    let c = AuthBlock::unpack(Kind::Certificate2, &unhex(f[1]));
    assert_eq!(c.valid, f[2] == "1");
    assert_eq!(c.data_len.to_string(), f[3]);
    let Body::Certificate2(c2) = &c.body else { panic!("not a certificate 2") };
    assert_eq!((c2.user_id.to_string(), c2.first_community().to_string()), (f[4].to_string(), f[5].to_string()));
    let perms: String = [(4242, 30, 3), (0, 10, 8), (0, 0, 7), (0, 0, 8)]
        .iter()
        .map(|&(u, c3, t)| if c.verify_permissions(u, c3, t) { '1' } else { '0' })
        .collect();
    assert_eq!(perms, f[6]);
    assert_eq!(text::to_display(&c2.nickname(&text::from_utf8("k1"))), f[8]);
    assert_eq!(hex(c2.user_data.get(&10).unwrap()), f[9]);
    assert_eq!((c2.key_id(10).to_string(), c2.key_id(11).to_string()), (f[10].to_string(), f[11].to_string()));
    assert_eq!(c2.first_community_trust().to_string(), f[12]);
    assert_eq!(c2.first_nickname().len().to_string(), f[13]);

    let f = &lines("keyblock")[0];
    let k = AuthBlock::unpack(Kind::PubKeyBlock, &unhex(f[1]));
    assert_eq!(k.valid, f[2] == "1");
    assert_eq!(k.data_len.to_string(), f[3]);
    let Body::PubKeyBlock(kb) = &k.body else { panic!("not a key block") };
    assert_eq!(kb.id.to_string(), f[4]);

    let (c1raw, c2raw, kbraw) = (unhex(lines("cert1")[0][1]), unhex(lines("cert2")[0][1]), unhex(f[1]));
    for f in lines("authcut") {
        let cut: usize = f[1].parse().unwrap();
        let v = [
            AuthBlock::unpack(Kind::Certificate, &c1raw[..cut]).valid,
            AuthBlock::unpack(Kind::Certificate2, &c2raw[..cut.min(c2raw.len())]).valid,
            AuthBlock::unpack(Kind::PubKeyBlock, &kbraw[..cut.min(kbraw.len())]).valid,
        ];
        assert_eq!(v.iter().map(|&b| if b { '1' } else { '0' }).collect::<String>(), f[2], "cut {cut}");
    }
}

#[test]
fn cd_keys_match() {
    const MACHINE: u32 = 0x9abc1234;
    for f in lines("cdsym") {
        let key = cdkey::symmetric_key(f[1].as_bytes(), (f[2] == "1").then_some(MACHINE));
        // the C++ Blowfish kept no key shorter than 5 bytes
        let want = if key.len() < 5 { "-".to_string() } else { hex(&key) };
        assert_eq!(want, f[3], "{f:?}");
    }
    for f in lines("cdkey") {
        let product = f[1].as_bytes();
        let mut raw: [u8; 8] = unhex(f[2]).try_into().unwrap();
        // the line's raw bytes are after the check byte was set; the first column tested the bytes before it
        assert_eq!(cdkey::is_valid(product, &raw), f[4] == "1");
        let text = cdkey::format(&raw);
        assert_eq!(text, f[5]);
        assert_eq!(cdkey::parse(text.as_bytes()), Some(raw));
        let enc = cdkey::encrypt(product, Some(MACHINE), &raw);
        assert_eq!(opt_hex(enc.clone()), f[7], "{f:?}");
        let d = cdkey::decrypt(product, Some(MACHINE), true, &enc.unwrap());
        assert_eq!(d.valid, f[8] == "1");
        assert_eq!(opt_hex(d.raw.map(|r| r.to_vec())), f[9]);
        assert_eq!(format!("{}{}", u8::from(cdkey::is_beta(&raw)), u8::from(cdkey::is_force_check(&raw))), f[10]);
        raw[3] ^= 1;
        assert!(!cdkey::is_valid(product, &raw));
    }
    for f in lines("cdinit") {
        // the key text may hold a tab: the last three columns are the result
        let n = f.len();
        let text = f[1..n - 3].join("\t");
        let raw = cdkey::parse(text.as_bytes());
        let valid = raw.is_some_and(|r| cdkey::is_valid(b"EmpireEarth", &r));
        assert_eq!(valid, f[n - 3] == "1", "{text}");
        assert_eq!(raw.map(|r| hex(&r)).unwrap_or_else(|| "NULL".into()), f[n - 2], "{text}");
        assert_eq!(raw.map(|r| cdkey::format(&r)).unwrap_or_default(), f[n - 1], "{text}");
    }
}
