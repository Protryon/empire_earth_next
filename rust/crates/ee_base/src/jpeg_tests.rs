//! jpeg: unit tests of the decoder's parts (the replay of the WON driver's golden is ee_won_gui's imgdec_tests.rs,
//! through JPGDecoder's FileReader path).
use super::*;

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
fn restarts_resync_as_libjpeg_does() {
    // (the marker found, the restart expected) -> the marker left unread after the resync, the next expected
    // restart: the expected one is read; one of the next two is left for later; a prior one or an invalid marker is
    // skipped for the next marker; one too far away is taken as the expected one
    for (found, rest, desired, left) in [
        (0xd0, &[][..], 0, 0),
        (0xd1, &[][..], 0, 0xd1),
        (0xd2, &[][..], 0, 0xd2),
        (0xd5, &[][..], 0, 0),
        (0xd7, &[0xff, 0xd0][..], 0, 0),
        (0xc4, &[][..], 3, 0xc4),
        (0x02, &[0xff, 0xd9][..], 0, 0xd9),
    ] {
        let mut b = vec![0xff, found];
        b.extend_from_slice(rest);
        let mut input = SliceInput { data: &b, pos: 0 };
        let mut d = Dec::new(&mut input);
        d.start_of_file = false;
        d.next_restart_num = desired;
        d.restart().unwrap();
        assert_eq!((d.unread_marker, d.next_restart_num), (left, (desired + 1) & 7), "marker {found:x}");
    }
}
