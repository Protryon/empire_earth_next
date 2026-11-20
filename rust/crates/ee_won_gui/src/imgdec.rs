//! WONGUI's image decoders in Rust (R6 WON round 41): PNGDecoder.cpp's attributes and decode (the pixels from
//! ee_openformats' PNG reader, as round 18 made them) and JPGDecoder.cpp's (ee_base's jpeg.rs, libjpeg 6b's decode in
//! Rust), each reading through a FileReader's Rust core. The multi-decoder, its decoders by extension and the
//! images it makes are imgobj.rs's (R6 WONLobby round 24); `ee_won_img_attrib` / `ee_won_img_decode` stay for the
//! drivers: a FileReader status comes back for the C++ to throw as FileReaderException, where the original's reads
//! threw.

use ee_base::jpeg::{self, Stop};
use ee_won_net::file::EeWonFileReader;

/// The decoders.
#[repr(i32)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum EeWonImgKind {
    Png = 0,
    Jpg = 1,
}

/// A decoded image: its size, its transparency flag, and its ARGB pixels (rows top first) until taken.
#[repr(C)]
pub struct EeWonImgRaw {
    pub width: i32,
    pub height: i32,
    pub transparent: bool,
    /// A `Box<Vec<u32>>`.
    pub pixels: *mut std::ffi::c_void,
}

/// A result as the C ABI's: true or false, and a FileReader status the C++ throws (0: none).
fn answer<T>(r: &Result<T, Stop>, thrown: &mut i32) -> bool {
    *thrown = if let Err(Stop::Throw(s)) = r { *s } else { 0 };
    r.is_ok()
}

/// `FileReader::Available()` (`length() - pos()`).
fn available(r: &mut EeWonFileReader) -> Result<u64, Stop> {
    let (len, s) = r.length();
    if s != 0 {
        return Err(Stop::Throw(s));
    }
    Ok(len.wrapping_sub(r.pos()) as u64)
}

/// JPGDecoder's source: the decoder reads through the FileReader (a status is one the C++ throws).
struct FileInput<'a>(&'a mut EeWonFileReader);

impl jpeg::Input for FileInput<'_> {
    fn read_max(&mut self, out: &mut [u8]) -> (usize, i32) {
        // SAFETY: `out` holds out.len() bytes
        let (n, s) = unsafe { EeWonFileReader::read_max(self.0, out.as_mut_ptr(), out.len() as i32) };
        (n.max(0) as usize, s)
    }
    fn skip(&mut self, n: u32) -> i32 {
        EeWonFileReader::skip(self.0, n as _)
    }
}

fn read_max(r: &mut EeWonFileReader, out: &mut [u8]) -> Result<usize, Stop> {
    let (n, s) = jpeg::Input::read_max(&mut FileInput(r), out);
    if s != 0 {
        return Err(Stop::Throw(s));
    }
    Ok(n)
}

/// `PNGDecoder::DoGetAttrib`: the signature and IHDR, then the chunks up to the first IDAT, as libpng's
/// png_read_info read them (their CRCs not checked).
pub fn png_attributes(r: &mut EeWonFileReader) -> Result<(i32, i32), Stop> {
    const HEAD: [u8; 16] = [0x89, b'P', b'N', b'G', 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13, b'I', b'H', b'D', b'R'];
    let mut head = [0u8; 33];
    if read_max(r, &mut head)? != 33 || head[..16] != HEAD {
        return Err(Stop::Fail);
    }
    loop {
        let mut chunk = [0u8; 8];
        if read_max(r, &mut chunk)? != 8 {
            return Err(Stop::Fail);
        }
        if &chunk[4..] == b"IDAT" {
            break;
        }
        // unsigned long arithmetic (32 bits in the Windows build)
        let length = u32::from_be_bytes([chunk[0], chunk[1], chunk[2], chunk[3]]) as std::ffi::c_ulong;
        if (available(r)? as std::ffi::c_ulong) < length.wrapping_add(4) {
            return Err(Stop::Fail);
        }
        let s = r.skip(length.wrapping_add(4));
        if s != 0 {
            return Err(Stop::Throw(s));
        }
    }
    let be = |i: usize| i32::from_be_bytes([head[i], head[i + 1], head[i + 2], head[i + 3]]);
    Ok((be(16), be(20)))
}

/// `PNGDecoder::DoDecodeRaw`: the rest of the file decoded; transparent when it has alpha or a tRNS chunk.
pub fn png_decode(r: &mut EeWonFileReader) -> Result<(i32, i32, bool, Vec<u32>), Stop> {
    let n = available(r)? as usize;
    let mut bytes = Vec::new();
    bytes.try_reserve_exact(n).map_err(|_| Stop::Fail)?;
    bytes.resize(n, 0);
    if n != 0 {
        let got = read_max(r, &mut bytes)?;
        bytes.truncate(got);
    }
    let p = ee_base::png::png_to_picture(&bytes).map_err(|_| Stop::Fail)?;
    let px = p
        .bgra
        .as_chunks::<4>()
        .0
        .iter()
        .map(|q| q[0] as u32 | ((q[1] as u32) << 8) | ((q[2] as u32) << 16) | ((q[3] as u32) << 24))
        .collect();
    Ok((p.width as i32, p.height as i32, p.depth == 32, px))
}

/// One decoder's attributes.
pub fn attributes(kind: EeWonImgKind, r: &mut EeWonFileReader) -> Result<(i32, i32), Stop> {
    match kind {
        EeWonImgKind::Png => png_attributes(r),
        EeWonImgKind::Jpg => jpeg::attributes(&mut FileInput(r)).map(|(w, h)| (w as i32, h as i32)),
    }
}

/// One decoder's raw image.
pub fn decode(kind: EeWonImgKind, r: &mut EeWonFileReader) -> Result<(i32, i32, bool, Vec<u32>), Stop> {
    match kind {
        EeWonImgKind::Png => png_decode(r),
        EeWonImgKind::Jpg => jpeg::decode_argb(&mut FileInput(r)).map(|(w, h, px)| (w as i32, h as i32, false, px)),
    }
}

/// `DoGetAttrib` -> true with the size in `wh`; `thrown` the FileReader status the C++ throws (0: none).
#[no_mangle]
pub extern "C" fn ee_won_img_attrib(
    kind: EeWonImgKind,
    reader: &mut EeWonFileReader,
    wh: &mut [i32; 2],
    thrown: &mut i32,
) -> bool {
    let r = attributes(kind, reader);
    if let Ok((w, h)) = r {
        *wh = [w, h];
    }
    answer(&r, thrown)
}

/// `DoDecodeRaw` -> true with the image in `out` (its pixels for `ee_won_img_take`), false for NULL; `thrown` the
/// FileReader status the C++ throws (0: none).
#[no_mangle]
pub extern "C" fn ee_won_img_decode(
    kind: EeWonImgKind,
    reader: &mut EeWonFileReader,
    out: &mut EeWonImgRaw,
    thrown: &mut i32,
) -> bool {
    let r = decode(kind, reader);
    let ok = answer(&r, thrown);
    if let Ok((w, h, t, px)) = r {
        *out = EeWonImgRaw { width: w, height: h, transparent: t, pixels: Box::into_raw(Box::new(px)).cast() };
    }
    ok
}

/// The decoded pixels into the new image's `width * height` pixels, and freed.
///
/// # Safety
/// `out` came from `ee_won_img_decode`; `dst` holds its pixels.
#[no_mangle]
pub unsafe extern "C" fn ee_won_img_take(out: &mut EeWonImgRaw, dst: *mut u32) {
    if out.pixels.is_null() {
        return;
    }
    // SAFETY: the box made above, taken once
    let px = unsafe { Box::from_raw(out.pixels.cast::<Vec<u32>>()) };
    out.pixels = std::ptr::null_mut();
    // SAFETY: the contract
    unsafe { std::ptr::copy_nonoverlapping(px.as_ptr(), dst, px.len()) };
}

#[cfg(test)]
#[path = "imgdec_tests.rs"]
mod tests;
