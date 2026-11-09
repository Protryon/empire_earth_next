//! Golden vectors from the former C++ (implode.c/explode.c, UDataCompression.cpp, UEncryption.cpp): small, synthetic
//! inputs; the outputs are compared as hashes (FNV-1a) with the short streams in full.

use crate::{compression, dcl, encryption};

const GOLDEN: &str = include_str!("golden_codec.txt");

fn fnv(data: &[u8]) -> u64 {
    data.iter().fold(0xcbf29ce484222325u64, |h, &b| (h ^ u64::from(b)).wrapping_mul(0x100000001b3))
}

fn gen(kind: i32, seed: u32, len: usize) -> Vec<u8> {
    const TEXT: &[u8] = b"The quick brown fox jumps over the lazy dog. Empire Earth!\n\t\r 0123456789 aeiou AEIOU ";
    let mut x = seed;
    (0..len)
        .map(|i| {
            x = x.wrapping_mul(1103515245).wrapping_add(12345);
            match kind {
                0 => ((x >> 16) & 3) as u8,
                1 => ((x >> 16) & 255) as u8,
                2 => TEXT[((x >> 16) & 7) as usize + (i % 40)],
                3 => 0,
                4 => ((i / 300) & 255) as u8,
                _ => (i * i) as u8,
            }
        })
        .collect()
}

fn hex(data: &[u8]) -> String {
    if data.is_empty() {
        return "-".into();
    }
    data.iter().map(|b| format!("{b:02x}")).collect()
}

fn unhex(text: &str) -> Vec<u8> {
    if text == "-" {
        return Vec::new();
    }
    (0..text.len()).step_by(2).map(|i| u8::from_str_radix(&text[i..i + 2], 16).unwrap()).collect()
}

fn pack(input: &[u8], mode: u32, dict: u32) -> (u32, Vec<u8>, usize) {
    let mut out = Vec::new();
    let mut pieces = 0;
    let r = dcl::implode(input, mode, dict, &mut |p: &[u8]| {
        out.extend_from_slice(p);
        pieces += 1;
    });
    (r, out, pieces)
}

fn unpack(input: &[u8]) -> (u32, Vec<u8>) {
    let mut out = Vec::new();
    let r = dcl::explode(input, &mut |p: &[u8]| out.extend_from_slice(p));
    (r, out)
}

fn cases(tag: &str) -> Vec<Vec<&'static str>> {
    let rows: Vec<Vec<&str>> =
        GOLDEN.lines().map(|l| l.split('\t').collect::<Vec<_>>()).filter(|t| t[0] == tag).collect();
    assert!(!rows.is_empty(), "no {tag} cases");
    rows
}

fn int<T: std::str::FromStr>(text: &str) -> T
where
    T::Err: std::fmt::Debug,
{
    text.parse().unwrap()
}

#[test]
fn implode_output_is_byte_identical_to_the_c_encoder() {
    for t in cases("implode") {
        let input = gen(int(t[1]), int(t[2]), int(t[3]));
        let (result, out, pieces) = pack(&input, int(t[4]), int(t[5]));
        assert_eq!(result, 0, "{t:?}");
        assert_eq!(out.len(), int::<usize>(t[6]), "{t:?}");
        assert_eq!(format!("{:016x}", fnv(&out)), t[7], "{t:?}");
        if t[8] != "-" {
            assert_eq!(hex(&out), t[8], "{t:?}");
        }
        assert_eq!(pieces, int::<usize>(t[9]), "{t:?}");
        let (r, back) = unpack(&out);
        if out.len() > 4 {
            assert_eq!((r, &back), (0, &input), "{t:?}");
        } else {
            assert_eq!(r, dcl::PK_BAD_DATA, "{t:?}");
        }
    }
}

#[test]
fn implode_refuses_bad_arguments_as_c() {
    for t in cases("implode-bad-dict") {
        let (r, out, _) = pack(&gen(2, 1, 50), 0, int(t[1]));
        assert_eq!((r, out.len()), (int(t[2]), int(t[3])), "{t:?}");
    }
    for t in cases("implode-bad-mode") {
        let (r, out, _) = pack(&gen(2, 1, 50), int(t[1]), 0x1000);
        assert_eq!((r, out.len()), (int(t[2]), int(t[3])), "{t:?}");
    }
}

#[test]
fn explode_truncations_and_corruptions_match_c() {
    let src = gen(2, 7, 9000);
    for mode in 0..2u32 {
        let (_, z, _) = pack(&src, mode, 0x800);
        for t in cases("explode-trunc").iter().filter(|t| t[1] == mode.to_string()) {
            let cut: usize = int(t[2]);
            let (r, out) = unpack(&z[..cut]);
            assert_eq!(
                (r, out.len(), format!("{:016x}", fnv(&out))),
                (int::<u32>(t[3]), int::<usize>(t[4]), t[5].to_string()),
                "{t:?}"
            );
        }
        for t in cases("explode-flip").iter().filter(|t| t[1] == mode.to_string()) {
            let mut c = z.clone();
            c[int::<usize>(t[2])] ^= int::<u8>(t[3]);
            let (r, out) = unpack(&c);
            assert_eq!(
                (r, out.len(), format!("{:016x}", fnv(&out))),
                (int::<u32>(t[4]), int::<usize>(t[5]), t[6].to_string()),
                "{t:?}"
            );
        }
    }
}

#[test]
fn explode_headers_match_c() {
    for t in cases("explode-hdr") {
        let bytes = [int::<u8>(t[1]), int::<u8>(t[2]), 0xff, 0xff, 0xff, 0x80, 0x01];
        let (r, out) = unpack(&bytes);
        assert_eq!((r, out.len()), (int(t[3]), int(t[4])), "{t:?}");
    }
    for t in cases("explode-short") {
        let n: usize = int(t[1]);
        let mut bytes = vec![0u8; n];
        if n >= 2 {
            bytes[1] = 6;
        }
        let (r, out) = unpack(&bytes);
        assert_eq!((r, out.len()), (int(t[2]), int(t[3])), "{t:?}");
    }
    for t in cases("explode-history") {
        let (r, out) = unpack(&unhex(t[1]));
        assert_eq!((r, out.len(), hex(&out)), (int(t[2]), int(t[3]), t[4].to_string()), "{t:?}");
    }
}

#[test]
fn compression_wrapper_matches_c() {
    let rows = |tag: &str| cases(tag);
    for t in rows("compress") {
        let input = gen(int(t[1]), 11, int(t[2]));
        let mut out = vec![0u8; input.len() + 8192 + 64];
        let (code, len) = compression::compress(&input, &mut out, 0x1000);
        assert_eq!((code, len), (int(t[3]), int(t[4])), "{t:?}");
        assert_eq!(format!("{:016x}", fnv(&out[..len as usize])), t[5], "{t:?}");
    }
    for t in rows("decompress") {
        let input = gen(int(t[1]), 11, int(t[2]));
        let mut out = vec![0u8; input.len() + 8192 + 64];
        let (_, len) = compression::compress(&input, &mut out, 0x1000);
        let mut back = vec![0u8; input.len() + 16];
        let (code, dl) = compression::decompress(&out[..len as usize], &mut back);
        assert_eq!((code, dl), (int(t[3]), int(t[4])), "{t:?}");
        assert_eq!(format!("{:016x}", fnv(&back[..dl as usize])), t[5], "{t:?}");
    }
    for t in rows("getsize") {
        let input = gen(int(t[1]), 11, int(t[2]));
        let mut out = vec![0u8; input.len() + 8192 + 64];
        let (_, len) = compression::compress(&input, &mut out, 0x1000);
        assert_eq!(compression::decompressed_size(&out[..len as usize]), (int(t[3]), int(t[4])), "{t:?}");
        assert_eq!(compression::is_compressed(&out[..len as usize]), t[5] == "1", "{t:?}");
    }
    for t in rows("decompress-small") {
        let input = gen(int(t[1]), 11, int(t[2]));
        let mut out = vec![0u8; input.len() + 8192 + 64];
        let (_, len) = compression::compress(&input, &mut out, 0x1000);
        let mut back = vec![0u8; input.len() / 2];
        assert_eq!(compression::decompress(&out[..len as usize], &mut back), (int(t[3]), int(t[4])), "{t:?}");
    }
    for t in rows("compress-small") {
        let input = gen(int(t[1]), 11, int(t[2]));
        let mut tiny = [0u8; 40];
        assert_eq!(compression::compress(&input, &mut tiny, 0x1000), (int(t[3]), int(t[4])), "{t:?}");
    }
    for t in rows("compress-already") {
        let input = gen(int(t[1]), 11, int(t[2]));
        let mut out = vec![0u8; input.len() + 8192 + 64];
        let (_, len) = compression::compress(&input, &mut out, 0x1000);
        let mut tiny = [0u8; 40];
        assert_eq!(compression::compress(&out[..len as usize], &mut tiny, 0x1000), (int(t[3]), int(t[4])), "{t:?}");
    }
    for t in rows("compress-badlevel") {
        let input = gen(int(t[1]), 11, int(t[2]));
        let mut out = vec![0u8; input.len() + 8192 + 64];
        assert_eq!(compression::compress(&input, &mut out, 512), (int(t[3]), int(t[4])), "{t:?}");
    }
    for t in rows("is-compressed") {
        assert_eq!(compression::is_compressed(t[1].as_bytes()), t[2] == "1", "{t:?}");
        assert_eq!(compression::decompressed_size(t[1].as_bytes()), (int(t[3]), int(t[4])), "{t:?}");
    }
    assert_eq!(compression::decompress(b"hello", &mut [0u8; 20]), (0x13, 0));
    let mut junk = [0u8; 16];
    junk[..4].copy_from_slice(b"PK01");
    junk[4] = 9;
    junk[13] = 7;
    junk[14] = 0xff;
    junk[15] = 0xff;
    let t = &cases("decompress-badbody")[0];
    assert_eq!(compression::decompress(&junk, &mut [0u8; 20]), (int(t[1]), int(t[2])));
}

#[test]
fn encrypt_ustring_matches_c() {
    for t in cases("enc-ustring") {
        assert_eq!(String::from_utf8(encryption::encrypt_ustring(&unhex(t[1]), int(t[2]))).unwrap(), t[3], "{t:?}");
    }
}

#[test]
fn decrypt_ustring_matches_c() {
    for t in cases("dec-ustring") {
        let (text, ok) = encryption::decrypt_ustring(t[1].as_bytes());
        assert_eq!((hex(&text), ok), (t[3].to_string(), t[2] == "1"), "{t:?}");
    }
}

#[test]
fn authorization_codes_match_c() {
    for t in cases("auth") {
        assert_eq!(String::from_utf8(encryption::encrypt_authorization_code(t[1].as_bytes())).unwrap(), t[2], "{t:?}");
        assert_eq!(encryption::decrypt_authorization_code(t[2].as_bytes()), (int(t[3]), int(t[4])), "{t:?}");
    }
}

#[test]
fn expirations_match_c() {
    for t in cases("expiration") {
        assert_eq!(String::from_utf8(encryption::encrypt_expiration(int(t[1]), int(t[2]))).unwrap(), t[3], "{t:?}");
        assert_eq!(encryption::decrypt_expiration(t[3].as_bytes()), (int(t[4]), int(t[5]), t[6] == "1"), "{t:?}");
    }
    for t in cases("expiration-bad") {
        assert_eq!(encryption::decrypt_expiration(t[1].as_bytes()), (int(t[2]), int(t[3]), t[4] == "1"), "{t:?}");
    }
}

#[test]
fn guids_match_c() {
    for t in cases("guid-enc") {
        let key: u32 = int(t[2]);
        let (padded, out) = encryption::encrypt_guid(0x1234_5678 + key, &unhex(t[1]), int(t[3]), key);
        assert_eq!(hex(&padded), t[4], "{t:?}");
        assert_eq!(String::from_utf8(out).unwrap(), t[5], "{t:?}");
    }
    for t in cases("guid-dec") {
        let key: u32 = int(t[2]);
        let (value, guid, ok) = encryption::decrypt_guid(t[1].as_bytes(), key, b"", 256);
        assert_eq!((value, hex(&guid), ok), (int(t[3]), t[4].to_string(), t[5] == "1"), "{t:?}");
    }
    for t in cases("guid-dec-pre") {
        let key: u32 = int(t[2]);
        // the tool's prefix "pre" came from UString("pre"), an exact-fit buffer: nothing can be appended
        let (value, guid, ok) = encryption::decrypt_guid(t[1].as_bytes(), key, b"pre", 4);
        assert_eq!((value, hex(&guid), ok), (int(t[3]), t[4].to_string(), t[5] == "1"), "{t:?}");
    }
}
