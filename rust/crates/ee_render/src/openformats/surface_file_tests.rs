//! `surface_file.rs`'s tests: the members' host calls in the original's order over a recording host (a memory surface,
//! a file over a C temporary stream); the JPEGs real ones through that stream.

use super::*;
use std::cell::RefCell;
use std::ffi::c_char;

extern "C" {
    fn tmpfile() -> *mut c_void;
    fn fclose(stream: *mut c_void) -> c_int;
}

#[derive(Default)]
struct World {
    log: Vec<String>,
    format: c_ulong,
    depth: c_ulong,
    width: c_ulong,
    height: c_ulong,
    bytes_per_row: c_ulong,
    pixels: Vec<u8>,
    no_alpha: bool,
    lock_fails: bool,
    locked: bool,
    open: bool,
    access: isize,
    stream: usize,
    position: c_long,
    /// what `FileWrite`/`FileWriteU16`/`FileWriteLong` wrote
    written: Vec<u8>,
    /// what `FileRead` serves
    contents: Vec<u8>,
}

thread_local! {
    static WORLD: RefCell<World> = RefCell::new(World::default());
}

fn with<R>(f: impl FnOnce(&mut World) -> R) -> R {
    WORLD.with(|w| f(&mut w.borrow_mut()))
}

fn log(s: &str) {
    with(|w| w.log.push(s.to_string()));
}

const SURFACE: usize = 0x5000;
const FILE: usize = 0x6000;

unsafe extern "C" fn host(op: EeOfSurfOp, obj: *mut c_void, a: isize, b: isize, c: isize) -> isize {
    use EeOfSurfOp::*;
    let o = obj as usize;
    match op {
        Format | Depth | BytesPerRow | Memory | PixelWidth | PixelHeight | NoAlpha | Lock | Unlock => {
            assert_eq!(o, SURFACE)
        }
        _ => assert_eq!(o, FILE),
    }
    log(&format!("{op:?}"));
    // SAFETY: the tests' pointers are valid for what each operation reads or writes
    unsafe {
        with(|w| match op {
            Format => w.format as isize,
            Depth => w.depth as isize,
            BytesPerRow => w.bytes_per_row as isize,
            Memory => {
                if w.locked {
                    w.pixels.as_mut_ptr() as isize
                } else {
                    0
                }
            }
            PixelWidth => w.width as isize,
            PixelHeight => w.height as isize,
            NoAlpha => w.no_alpha as isize,
            Lock => {
                w.locked = !w.lock_fails;
                if w.lock_fails {
                    9
                } else {
                    0
                }
            }
            Unlock => {
                w.locked = false;
                0
            }
            FileIsOpen => w.open as isize,
            FileAccess => w.access,
            FileOpen => {
                w.open = true;
                w.access = a;
                0
            }
            FileClose => {
                w.open = false;
                0
            }
            FileFOpen => {
                let mode = std::ffi::CStr::from_ptr(a as *const c_char).to_str().unwrap().to_string();
                w.log.push(format!("mode {mode}"));
                w.open = true;
                0
            }
            FileStream => w.stream as isize,
            FilePosition => {
                *(a as *mut c_long) = w.position;
                0
            }
            FileSetPosition => {
                w.log.push(format!("at {a} {b} {c}"));
                0
            }
            FileWrite => {
                w.written.extend_from_slice(std::slice::from_raw_parts(a as *const u8, b as usize));
                0
            }
            FileWriteU16 => {
                w.written.extend_from_slice(&(a as u16).to_le_bytes());
                0
            }
            FileWriteLong => {
                w.written.extend_from_slice(&(a as i32).to_le_bytes());
                0
            }
            FileRead => {
                let n = (b as usize).min(w.contents.len());
                std::ptr::copy_nonoverlapping(w.contents.as_ptr(), a as *mut u8, n);
                *(c as *mut c_ulong) = n as c_ulong;
                0
            }
            FileLength => {
                *(a as *mut c_ulong) = w.contents.len() as c_ulong;
                0
            }
        })
    }
}

fn reset(width: c_ulong, height: c_ulong, depth: c_ulong) {
    with(|w| {
        *w = World { format: 0, depth, width, height, bytes_per_row: width * depth / 8 + 4, ..World::default() };
        w.pixels = vec![0; (w.bytes_per_row * height) as usize + 16];
    });
}

fn take_log() -> Vec<String> {
    with(|w| std::mem::take(&mut w.log))
}

fn set_pixel32(x: usize, y: usize, value: u32) {
    with(|w| {
        let at = y * w.bytes_per_row as usize + x * 4;
        w.pixels[at..at + 4].copy_from_slice(&value.to_le_bytes());
    });
}

fn pixel32(x: usize, y: usize) -> u32 {
    with(|w| {
        let at = y * w.bytes_per_row as usize + x * 4;
        u32::from_le_bytes(w.pixels[at..at + 4].try_into().unwrap())
    })
}

fn save(kind: u8) -> c_long {
    unsafe { ee_of_surf_save(host, SURFACE as *mut c_void, FILE as *mut c_void, kind) }
}

fn load(kind: u8) -> c_long {
    unsafe { ee_of_surf_load(host, SURFACE as *mut c_void, FILE as *mut c_void, kind) }
}

#[test]
fn a_bmp_is_bottom_up_bgr_with_two_trailing_bytes() {
    reset(2, 2, 32);
    set_pixel32(0, 0, 0xff10_2030);
    set_pixel32(1, 0, 0xff40_5060);
    set_pixel32(0, 1, 0xff70_8090);
    set_pixel32(1, 1, 0xffa0_b0c0);
    assert_eq!(save(K_IMAGE_FILE_BMP), 0);
    let written = with(|w| w.written.clone());
    // the headers: 'BM', the size (2 rows of 2 * 3 + 2 bytes, the headers, 2), the offset 54, 2 x 2 at 24 bits
    assert_eq!(&written[0..2], b"BM");
    assert_eq!(u32::from_le_bytes(written[2..6].try_into().unwrap()), 8 * 2 + 54 + 2);
    assert_eq!(u32::from_le_bytes(written[10..14].try_into().unwrap()), 54);
    assert_eq!(u32::from_le_bytes(written[18..22].try_into().unwrap()), 2);
    assert_eq!(u16::from_le_bytes(written[28..30].try_into().unwrap()), 24);
    // the bottom row first, blue, green, red, two bytes of padding (2 % 4)
    assert_eq!(&written[54..62], &[0x90, 0x80, 0x70, 0xc0, 0xb0, 0xa0, 0, 0]);
    assert_eq!(&written[62..70], &[0x30, 0x20, 0x10, 0x60, 0x50, 0x40, 0, 0]);
    assert_eq!(&written[70..], &[0, 0]);
    let log = take_log();
    assert_eq!(log[3..5], ["FileIsOpen", "FileOpen"]);
    assert_eq!(log.last().unwrap(), "FileClose");
}

#[test]
fn a_bmp_refuses_a_file_open_for_reading_and_unknown_types_fail() {
    reset(2, 2, 16);
    with(|w| {
        w.open = true;
        w.access = K_FILE_ACCESS_READ;
    });
    assert_eq!(save(K_IMAGE_FILE_BMP), K_ERROR_FAILED);
    assert!(with(|w| w.written.is_empty()));
    assert_eq!(save(0), K_ERROR_FAILED);
    assert_eq!(load(K_IMAGE_FILE_BMP), K_ERROR_FAILED);
    assert_eq!(load(0), K_ERROR_FAILED);
    assert_eq!(load(7), K_ERROR_FAILED);
}

/// A PNG of stored deflate blocks.
fn png(width: u32, height: u32, rgba: &[u8]) -> Vec<u8> {
    fn crc(data: &[u8]) -> u32 {
        let mut c = 0xffff_ffffu32;
        for &b in data {
            c ^= u32::from(b);
            for _ in 0..8 {
                c = (c >> 1) ^ (0xedb8_8320 & 0u32.wrapping_sub(c & 1));
            }
        }
        !c
    }
    fn chunk(out: &mut Vec<u8>, kind: &[u8], data: &[u8]) {
        out.extend_from_slice(&(data.len() as u32).to_be_bytes());
        let mut body = kind.to_vec();
        body.extend_from_slice(data);
        out.extend_from_slice(&body);
        out.extend_from_slice(&crc(&body).to_be_bytes());
    }
    let mut out = vec![0x89, b'P', b'N', b'G', 13, 10, 26, 10];
    let mut ihdr = width.to_be_bytes().to_vec();
    ihdr.extend_from_slice(&height.to_be_bytes());
    ihdr.extend_from_slice(&[8, 6, 0, 0, 0]);
    chunk(&mut out, b"IHDR", &ihdr);
    let mut raw = Vec::new();
    for row in rgba.chunks(width as usize * 4) {
        raw.push(0);
        raw.extend_from_slice(row);
    }
    let mut z = vec![0x78, 0x01, 1];
    z.extend_from_slice(&(raw.len() as u16).to_le_bytes());
    z.extend_from_slice(&(!(raw.len() as u16)).to_le_bytes());
    z.extend_from_slice(&raw);
    let (mut a, mut b) = (1u32, 0u32);
    for &c in &raw {
        a = (a + u32::from(c)) % 65521;
        b = (b + a) % 65521;
    }
    z.extend_from_slice(&((b << 16) | a).to_be_bytes());
    chunk(&mut out, b"IDAT", &z);
    chunk(&mut out, b"IEND", &[]);
    out
}

#[test]
fn a_png_lands_in_the_top_left_corner_and_a_larger_one_is_refused() {
    reset(3, 3, 32);
    with(|w| w.contents = png(2, 1, &[10, 20, 30, 40, 50, 60, 70, 80]));
    assert_eq!(load(K_IMAGE_FILE_PNG), 0);
    assert_eq!(pixel32(0, 0), 0x280a_141e);
    assert_eq!(pixel32(1, 0), 0x5032_3c46);
    assert_eq!(pixel32(2, 0), 0);
    let log = take_log();
    // the store's arguments right to left
    assert_eq!(log[log.len() - 4..], ["BytesPerRow", "Memory", "Depth", "Unlock"]);

    reset(1, 1, 32);
    with(|w| w.contents = png(2, 1, &[0; 8]));
    assert_eq!(load(K_IMAGE_FILE_PNG), K_ERROR_FAILED);
    assert!(!take_log().contains(&"Lock".to_string()));

    let mut out = [0; 3];
    with(|w| w.contents = png(2, 1, &[0; 8]));
    assert_eq!(unsafe { ee_of_surf_attributes(host, FILE as *mut c_void, K_IMAGE_FILE_PNG, &mut out) }, 0);
    assert_eq!(out, [2, 1, 32]);
    // a broken PNG fails and leaves the outputs alone
    let mut out = [7; 3];
    with(|w| w.contents = b"not a png".to_vec());
    assert_eq!(unsafe { ee_of_surf_attributes(host, FILE as *mut c_void, K_IMAGE_FILE_PNG, &mut out) }, K_ERROR_FAILED);
    assert_eq!(out, [7; 3]);
}

/// A fresh C temporary stream holding `bytes`, rewound.
fn stream_of(bytes: &[u8]) -> *mut c_void {
    let stream = unsafe { tmpfile() };
    assert!(!stream.is_null());
    unsafe {
        assert_eq!(fwrite(bytes.as_ptr().cast(), 1, bytes.len(), stream), bytes.len());
        fseek(stream, 0, SEEK_SET);
    }
    stream
}

/// The whole of a stream, rewound first.
fn stream_bytes(stream: *mut c_void) -> Vec<u8> {
    let mut out = Vec::new();
    let mut chunk = [0u8; 4096];
    unsafe { fseek(stream, 0, SEEK_SET) };
    loop {
        let n = unsafe { fread(chunk.as_mut_ptr().cast(), 1, chunk.len(), stream) };
        if n == 0 {
            break;
        }
        out.extend_from_slice(&chunk[..n]);
    }
    out
}

/// A JPEG of `width` x `height` RGB pixels at quality 100 without chroma subsampling.
fn jpeg_of(width: u16, height: u16, rgb: &[u8]) -> Vec<u8> {
    let mut bytes = Vec::new();
    let mut e = jpeg_encoder::Encoder::new(&mut bytes, 100);
    e.set_sampling_factor(jpeg_encoder::SamplingFactor::F_1_1);
    e.encode(rgb, width, height, jpeg_encoder::ColorType::Rgb).unwrap();
    bytes
}

fn close_to(got: u32, want: u32, tolerance: i32) -> bool {
    (0..4).all(|i| (((got >> (i * 8)) & 0xff) as i32 - ((want >> (i * 8)) & 0xff) as i32).abs() <= tolerance)
}

#[test]
fn a_jpeg_reads_its_rows_opaque_and_reopens_an_open_file_after_the_image() {
    reset(2, 2, 32);
    // flat 2 x 2 colours: one 8 x 8 block, quality 100, within a step or two of the pixels
    let stream = stream_of(&jpeg_of(2, 2, &[200, 100, 50, 200, 100, 50, 200, 100, 50, 200, 100, 50]));
    with(|w| {
        w.stream = stream as usize;
        w.open = true;
        w.position = 0;
    });
    assert_eq!(load(K_IMAGE_FILE_JPG), 0);
    assert!(close_to(pixel32(0, 0), 0xffc8_6432, 2), "{:08x}", pixel32(0, 0));
    assert!(close_to(pixel32(1, 1), 0xffc8_6432, 2), "{:08x}", pixel32(1, 1));
    let log = take_log();
    let expect = [
        "Format",
        "FileIsOpen",
        "FilePosition",
        "FileClose",
        "FileFOpen",
        "mode rb",
        "FileStream",
        "Lock",
        "PixelWidth",
        "PixelHeight",
        "Depth",
        "BytesPerRow",
        "PixelWidth",
        "Memory",
        "Unlock",
        "FileClose",
        "FileOpen",
        "FileSetPosition",
    ];
    assert_eq!(log[..expect.len()], expect);
    // reopened where the decoder's 4 KB reads left the stream: the whole (short) file read
    let size = stream_bytes(stream).len();
    assert_eq!(log[expect.len()..], [format!("at {size} 0 0")]);

    // a failed lock reads nothing
    with(|w| {
        w.lock_fails = true;
        w.open = false;
    });
    assert_eq!(load(K_IMAGE_FILE_JPG), 0);
    let log = take_log();
    assert_eq!(log[log.len() - 2..], ["Lock", "FileClose"]);
    unsafe { fclose(stream) };
}

#[test]
fn a_corrupt_or_larger_jpeg_fails_where_libjpeg_ended_the_process() {
    reset(2, 2, 32);
    let stream = stream_of(b"not a jpeg at all");
    with(|w| w.stream = stream as usize);
    assert_eq!(load(K_IMAGE_FILE_JPG), K_ERROR_FAILED);
    let mut out = [7; 3];
    assert_eq!(unsafe { ee_of_surf_attributes(host, FILE as *mut c_void, K_IMAGE_FILE_JPG, &mut out) }, K_ERROR_FAILED);
    assert_eq!(out, [7; 3]);
    unsafe { fclose(stream) };
    // a 3 x 1 image does not go into a 2 x 2 surface
    reset(2, 2, 32);
    let stream = stream_of(&jpeg_of(3, 1, &[9; 9]));
    with(|w| w.stream = stream as usize);
    assert_eq!(load(K_IMAGE_FILE_JPG), K_ERROR_FAILED);
    assert_eq!(pixel32(0, 0), 0);
    let mut out = [0; 3];
    unsafe { fseek(stream, 0, SEEK_SET) };
    assert_eq!(unsafe { ee_of_surf_attributes(host, FILE as *mut c_void, K_IMAGE_FILE_JPG, &mut out) }, 0);
    assert_eq!(out, [3, 1, 24]);
    unsafe { fclose(stream) };
}

/// The two offsets `SaveAsJPG2` patched through the file (`FileWriteLong`, which the test host records) written into
/// the stream, as the real file has them.
fn patch_offsets(stream: *mut c_void) {
    let written = with(|w| w.written.clone());
    assert_eq!(written.len(), 8);
    unsafe {
        fseek(stream, 0, SEEK_SET);
        fwrite(written.as_ptr().cast(), 1, 8, stream);
        fseek(stream, 0, SEEK_SET);
    }
}

#[test]
fn a_jpg2_round_trips_its_colour_and_alpha() {
    // 32 x 16: an opaque red half and a half-transparent blue half (each a 16 x 16 block of the 4:2:0 encoding)
    let (width, height) = (32, 16);
    let want = |x: usize| if x < 16 { 0xffc0_2010u32 } else { 0x8010_20c0 };
    reset(width, height, 32);
    for y in 0..height as usize {
        for x in 0..width as usize {
            set_pixel32(x, y, want(x));
        }
    }
    let stream = unsafe { tmpfile() };
    with(|w| w.stream = stream as usize);
    assert_eq!(save(K_IMAGE_FILE_JPG2), 0);
    let log = take_log();
    let at = log.iter().position(|l| l == "FileOpen").unwrap();
    assert_eq!(log[at..], ["FileOpen", "FileSetPosition", "at 0 0 0", "FileWriteLong", "FileWriteLong", "FileClose"]);
    // the header: the alpha JPEG's offset, then the end; two JPEGs (SOI .. EOI) between
    patch_offsets(stream);
    let bytes = stream_bytes(stream);
    let word = |i: usize| u32::from_le_bytes(bytes[i..i + 4].try_into().unwrap()) as usize;
    let (alpha, end) = (word(0), word(4));
    assert_eq!(end, bytes.len());
    assert_eq!(&bytes[8..10], &[0xff, 0xd8]);
    assert_eq!(&bytes[alpha - 2..alpha + 2], &[0xff, 0xd9, 0xff, 0xd8]);
    assert_eq!(&bytes[end - 2..], &[0xff, 0xd9]);

    // read back into a cleared surface: within the quality-80 4:2:0 encoding's error of every pixel
    reset(width, height, 32);
    with(|w| w.stream = stream as usize);
    unsafe { fseek(stream, 0, SEEK_SET) };
    assert_eq!(load(K_IMAGE_FILE_JPG2), 0);
    for y in 0..height as usize {
        for x in 0..width as usize {
            // the chroma's fancy upsampling blends the two halves' colours in the two columns either side
            let tolerance = if (14..18).contains(&x) { 80 } else { 24 };
            assert!(close_to(pixel32(x, y), want(x), tolerance), "{x} {y}: {:08x} for {:08x}", pixel32(x, y), want(x));
        }
    }
    // the alpha is a greyscale JPEG: flat halves come back almost exact
    assert!(close_to(pixel32(0, 0) & 0xff00_0000, 0xff00_0000, 2));
    assert!(close_to(pixel32(31, 15) & 0xff00_0000, 0x8000_0000, 2));

    // the attributes of a JPG2: its colour image's, 32 bits
    let mut out = [0; 3];
    unsafe { fseek(stream, 0, SEEK_SET) };
    assert_eq!(unsafe { ee_of_surf_attributes(host, FILE as *mut c_void, K_IMAGE_FILE_JPG2, &mut out) }, 0);
    assert_eq!(out, [32, 16, 32]);
    unsafe { fclose(stream) };
}

#[test]
fn a_jpeg_round_trips_a_16_bit_surface() {
    reset(8, 8, 16);
    // 0xf800: pure red in 5-6-5 (format 0)
    with(|w| {
        for y in 0..8 {
            for x in 0..8 {
                let at = y * w.bytes_per_row as usize + x * 2;
                w.pixels[at..at + 2].copy_from_slice(&0xf800u16.to_le_bytes());
            }
        }
    });
    let stream = unsafe { tmpfile() };
    with(|w| w.stream = stream as usize);
    assert_eq!(save(K_IMAGE_FILE_JPG), 0);
    let first = with(|w| u16::from_le_bytes(w.pixels[0..2].try_into().unwrap()));
    reset(8, 8, 16);
    with(|w| w.stream = stream as usize);
    unsafe { fseek(stream, 0, SEEK_SET) };
    assert_eq!(load(K_IMAGE_FILE_JPG), 0);
    let back = with(|w| u16::from_le_bytes(w.pixels[0..2].try_into().unwrap()));
    assert_eq!(back >> 11, first >> 11, "{back:04x} for {first:04x}");
    unsafe { fclose(stream) };
}

#[test]
fn a_jpeg_of_an_unlocked_surface_is_black() {
    reset(2, 3, 16);
    let stream = unsafe { tmpfile() };
    with(|w| {
        w.stream = stream as usize;
        w.lock_fails = true;
    });
    assert_eq!(save(K_IMAGE_FILE_JPG), 0);
    let log = take_log();
    assert!(!log.contains(&"Unlock".to_string()));
    assert_eq!(log.last().unwrap(), "FileClose");
    let picture = jpeg::decode(&mut jpeg::SliceInput { data: &stream_bytes(stream), pos: 0 }).unwrap();
    assert_eq!((picture.width, picture.height), (2, 3));
    assert!(picture.rows.iter().all(|&v| v <= 2));
    unsafe { fclose(stream) };
}

#[test]
fn an_empty_surface_saves_no_jpeg_and_fails() {
    reset(0, 2, 32);
    let stream = unsafe { tmpfile() };
    with(|w| w.stream = stream as usize);
    assert_eq!(save(K_IMAGE_FILE_JPG), K_ERROR_FAILED);
    assert!(stream_bytes(stream).is_empty());
    unsafe { fclose(stream) };
}
