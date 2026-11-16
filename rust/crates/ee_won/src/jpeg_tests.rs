//! jpeg and imgdec: unit tests and the replay of the driver's golden (`golden_jpeg.txt`, written by
//! tests/won/won_jpeg_test.cpp `--golden`: libjpeg's own compressed pictures and their corruptions, in hex, with the
//! original JPGDecoder's answers on libjpeg 6b).
use super::*;
use crate::file::status_text;
use crate::imgdec::{attributes as img_attributes, decode as img_decode, EeWonImgDec, EeWonImgKind};

/// A FileReader over memory, as `FileReader(const void*, unsigned long)` opens one.
fn reader(b: &[u8]) -> EeWonFileReader {
    let mut r = EeWonFileReader::new();
    r.buffer = if b.is_empty() { b"".as_ptr() } else { b.as_ptr() };
    r.length = b.len() as _;
    r.memory_mode = true;
    r
}

fn fnv(px: &[u32]) -> u64 {
    let mut h = 1469598103934665603u64;
    for &p in px {
        for b in (0..32).step_by(8) {
            h ^= ((p >> b) & 0xff) as u64;
            h = h.wrapping_mul(1099511628211);
        }
    }
    h
}

fn text(s: i32) -> String {
    status_text(s).to_str().unwrap().to_string()
}

/// One input's answers in the driver's words.
fn answers(b: &[u8]) -> String {
    let a = match img_attributes(EeWonImgKind::Jpg, &mut reader(b)) {
        Ok((w, h)) => format!("A 1 {w} {h}"),
        Err(Stop::Fail) => "A 0".to_string(),
        Err(Stop::Throw(s)) => format!("A throw {}", text(s)),
    };
    let d = match img_decode(EeWonImgKind::Jpg, &mut reader(b)) {
        Ok((w, h, t, px)) => {
            assert!(!t, "a JPEG is never transparent");
            format!("D {w} {h} {:016x}", fnv(&px))
        }
        Err(Stop::Fail) => "D NULL".to_string(),
        Err(Stop::Throw(s)) => format!("D throw {}", text(s)),
    };
    format!("{a} {d}")
}

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

#[test]
fn golden_replays() {
    let g = include_str!("golden_jpeg.txt");
    let mut n = 0;
    for line in g.lines() {
        let (hex, want) = line.split_once(' ').unwrap();
        let got = answers(&unhex(hex));
        assert_eq!(got, want, "case {n}: {}", &hex[..hex.len().min(80)]);
        n += 1;
    }
    assert_eq!(n, 400);
    // the golden holds every kind of answer
    for kind in ["A 1 ", "A 0 ", "A throw ", "D NULL", "D throw "] {
        assert!(g.contains(kind), "{kind}");
    }
}

#[test]
fn range_limit_is_libjpegs() {
    let t = range_limit();
    assert_eq!(t.len(), 1408);
    // the simple table: 0 below, x within, 255 above
    assert_eq!(&t[..256], &[0u8; 256][..]);
    assert!((0..256).all(|i| t[256 + i] == i as u8));
    assert!((512..896).all(|i| t[i] == 255));
    // the IDCT's: sign-extended 10 bits plus 128, clamped
    let idct = &t[384..];
    for m in 0..1024i32 {
        let s = (m << 22) >> 22;
        assert_eq!(idct[m as usize] as i32, (s + 128).clamp(0, 255), "{m}");
    }
}

#[test]
fn ycc_tables_are_libjpegs() {
    let t = YccTables::new();
    // FIX(1.402) = 91881, FIX(1.772) = 116130, FIX(0.71414) = 46802, FIX(0.34414) = 22554
    assert_eq!(t.cr_r[255], (91881 * 127 + 32768) >> 16);
    assert_eq!(t.cb_b[0], (116130 * -128 + 32768) >> 16);
    assert_eq!(t.cr_g[200], -46802 * 72);
    assert_eq!(t.cb_g[128], 32768);
    assert_eq!(t.cr_r[128], 0);
}

#[test]
fn idct_of_a_dc_block_is_flat() {
    let range = range_limit();
    let mut coef = [0i16; 64];
    let mut q = [1u16; 64];
    let mut out = [0u8; 64];
    for (dc, qv, want) in [(0, 1, 128u8), (8, 1, 129), (-8, 2, 126), (1000, 4, 255), (-1000, 4, 0)] {
        coef[0] = dc;
        q[0] = qv;
        idct_islow(&coef, &q, &range[384..], &mut out, 8);
        assert!(out.iter().all(|&v| v == want), "{dc} x {qv}: {:?}", &out[..8]);
    }
    // one AC coefficient makes a cosine row: symmetric about the middle, falling left to right
    coef[0] = 0;
    coef[1] = 20;
    q = [1; 64];
    idct_islow(&coef, &q, &range[384..], &mut out, 8);
    let row = &out[..8];
    assert!(row.windows(2).all(|w| w[0] >= w[1]), "{row:?}");
    assert!((0..4).all(
        |i| row[i] as i32 - 128 == 128 - row[7 - i] as i32 || (row[i] as i32 + row[7 - i] as i32 - 256).abs() <= 1
    ));
    assert!((0..8).all(|r| out[r * 8..r * 8 + 8] == *row));
}

#[test]
fn fancy_upsampling_weights() {
    // h2v1: 3/4 the nearer sample, 1/4 the further, the edges themselves
    let mut o = [0u8; 8];
    fancy_h2v1(&[0, 100, 200, 40], 4, &mut o);
    assert_eq!(o, [0, 25, 75, 125, 175, 160, 80, 40]);
    // h2v2: 9/16 3/16 3/16 1/16 with the bias 8 and 7
    let mut o = [0u8; 6];
    fancy_h2v2(&[16, 32, 48], &[0, 0, 0], 3, &mut o);
    assert_eq!(
        o,
        [
            (48 * 4 + 8) >> 4,
            (48 * 3 + 96 + 7) >> 4,
            (96 * 3 + 48 + 8) >> 4,
            (96 * 3 + 144 + 7) >> 4,
            (144 * 3 + 96 + 8) >> 4,
            (144 * 4 + 7) >> 4
        ]
        .map(|v: i32| v as u8)
    );
}

#[test]
fn not_a_jpeg_fails_and_empty_fails() {
    assert_eq!(attributes(&mut SliceInput { data: b"not a jpg", pos: 0 }).err(), Some(Stop::Fail));
    // an empty file is libjpeg's JERR_INPUT_EMPTY (the source's start of file)
    assert_eq!(decode(&mut SliceInput { data: b"", pos: 0 }).err(), Some(Stop::Fail));
    // a lone SOI: the fake end of image, then no image
    assert_eq!(decode(&mut SliceInput { data: &[0xff, 0xd8], pos: 0 }).err(), Some(Stop::Fail));
    // an APP segment longer than the file: the reader's skip status is thrown
    let b = [0xff, 0xd8, 0xff, 0xe3, 0x7f, 0xf0, 1, 2, 3];
    assert_eq!(attributes(&mut SliceInput { data: &b, pos: 0 }).err(), Some(Stop::Throw(1)));
}

#[test]
fn derived_tables_reject_overfull_codes() {
    // three codes of length 1 overflow the code space
    let mut t = HuffTbl { bits: [0; 17], huffval: [0; 256] };
    t.bits[1] = 3;
    assert!(derive(&t, false).is_err());
    // a DC symbol above 15
    t.bits[1] = 1;
    t.huffval[0] = 16;
    assert!(derive(&t, true).is_err());
    assert!(derive(&t, false).is_ok());
    // two codes of length 2: 00 and 01, looked up from the first 8 bits
    t.bits = [0; 17];
    t.bits[2] = 2;
    t.huffval[0] = 5;
    t.huffval[1] = 7;
    let d = derive(&t, true).unwrap();
    assert_eq!((d.look_nbits[0x3f], d.look_sym[0x3f]), (2, 5));
    assert_eq!((d.look_nbits[0x40], d.look_sym[0x40]), (2, 7));
    assert_eq!(d.look_nbits[0x80], 0);
    assert_eq!(d.maxcode[2], 1);
}

#[test]
fn multi_decoder_matches_without_case() {
    let mut d = EeWonImgDec::default();
    d.add(0x10, b"jpg");
    d.add(0x10, b"JPE");
    d.add(0x20, b"png");
    // a replaced decoder stays owned
    d.add(0x30, b"PNG");
    assert_eq!(d.owned, vec![0x10, 0x20, 0x30]);
    assert_eq!(d.map.len(), 3);
    // the first spelling of a key stays
    assert!(d.map.iter().any(|(k, v)| k == b"png" && *v == 0x30));
    d.cur = b"Jpe".to_vec();
    assert_eq!(d.find(), 0x10);
    d.cur = b"gif".to_vec();
    assert_eq!(d.find(), 0);
}

#[test]
fn png_attributes_read_to_the_first_idat() {
    let mut png = vec![0x89, b'P', b'N', b'G', 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13];
    png.extend_from_slice(b"IHDR");
    png.extend_from_slice(&[0, 0, 1, 2, 0, 0, 0, 7, 8, 6, 0, 0, 0, 0xde, 0xad, 0xbe, 0xef]);
    // a tEXt chunk of 3 bytes (and its CRC) is skipped
    png.extend_from_slice(&[0, 0, 0, 3]);
    png.extend_from_slice(b"tEXta\0b");
    png.extend_from_slice(&[1, 2, 3, 4]);
    png.extend_from_slice(&[0, 0, 0, 0]);
    png.extend_from_slice(b"IDAT");
    assert_eq!(crate::imgdec::png_attributes(&mut reader(&png)), Ok((258, 7)));
    // no IDAT: the chunk header read comes up short
    assert_eq!(crate::imgdec::png_attributes(&mut reader(&png[..png.len() - 8])), Err(Stop::Fail));
    // a chunk longer than what is left
    let mut long = png.clone();
    long[36] = 0x7f;
    assert_eq!(crate::imgdec::png_attributes(&mut reader(&long)), Err(Stop::Fail));
    // not a PNG
    assert_eq!(crate::imgdec::png_attributes(&mut reader(b"GIF89a")), Err(Stop::Fail));
}
