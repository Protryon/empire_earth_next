//! PNG -> the texture the game stores: the texture reader of GEOpenFormats.cpp (`PngToSst`, `ScanPng`). The file is
//! decoded by the `png` crate (inflate, the chunk walk, unfiltering, de-interlacing), read as the raw samples
//! (`Transformations::IDENTITY`); what the game stores from them is stb_image 2.30's result, which the loaders before
//! had: 16-bit samples keep their high byte, 1/2/4-bit grey is scaled to 8 bits (x 255, 85, 17), a palette is
//! expanded (an index past it is 0, 0, 0 with alpha 0), a tRNS colour match gives alpha 0 (16-bit: the whole
//! sample), grey becomes R = G = B.
//!
//! PNG is lossless, so for a well-formed file the pixels are the hand port's of stb_image that was here before
//! (docs/progress/deps.md: every PNG of the open tree decoded by both, equal). As stb, CRCs and the Adler-32 are not
//! checked and an unknown critical chunk refuses the file by name. What differs for a malformed file is in deps.md.

use std::io::Cursor;

use png::{ColorType, DecodingError, Transformations};

/// Why a conversion failed: the message (bytes, as names in it may not be UTF-8).
#[derive(Debug, PartialEq, Eq)]
pub struct Failure(pub Vec<u8>);

pub fn fail<T>(message: &str) -> Result<T, Failure> {
    Err(Failure(message.as_bytes().to_vec()))
}

/// stb_image's limits, kept: a side of 2^24 pixels, 2^30 bytes of samples.
const MAX_DIMENSIONS: u32 = 1 << 24;
const MAX_BYTES: u64 = 1 << 30;

fn undecodable<T>(reason: &str) -> Result<T, Failure> {
    fail(&format!("PNG cannot be decoded: {reason}"))
}

/// A file that ends early keeps stb_image's reason ("outofdata"); the others are the `png` crate's own.
fn refusal(e: DecodingError) -> Failure {
    let reason = match &e {
        DecodingError::IoError(io) if io.kind() == std::io::ErrorKind::UnexpectedEof => "outofdata".to_string(),
        _ => e.to_string(),
    };
    Failure(format!("PNG cannot be decoded: {reason}").into_bytes())
}

/// What stb_image multiplies a 1, 2 or 4-bit grey sample by.
fn depth_scale(depth: usize) -> u8 {
    match depth {
        1 => 0xff,
        2 => 0x55,
        4 => 0x11,
        _ => 1,
    }
}

/// Sample `i` of a raw row: packed bits below 8, big-endian at 16.
fn sample(row: &[u8], i: usize, depth: usize) -> u16 {
    match depth {
        8 => row[i] as u16,
        16 => u16::from_be_bytes([row[2 * i], row[2 * i + 1]]),
        _ => {
            let bit = i * depth;
            ((row[bit / 8] >> (8 - depth - bit % 8)) & ((1 << depth) - 1)) as u16
        }
    }
}

/// A PNG's pixels as `channels` (3 or 4) bytes each, top row first: (width, height, pixels).
fn decode(d: &[u8], channels: usize) -> Result<(u32, u32, Vec<u8>), Failure> {
    let mut decoder = png::Decoder::new(Cursor::new(d));
    decoder.set_transformations(Transformations::IDENTITY);
    decoder.ignore_checksums(true);
    decoder.set_ignore_text_chunk(true);
    decoder.set_ignore_iccp_chunk(true);
    decoder.set_limits(png::Limits { bytes: 1 << 31 });
    let mut reader = decoder.read_info().map_err(refusal)?;
    let info = reader.info();
    let (w, h) = (info.width, info.height);
    let (color, depth) = (info.color_type, info.bit_depth as usize);
    let n = color.samples();
    if w > MAX_DIMENSIONS || h > MAX_DIMENSIONS || w as u64 * h as u64 * channels.max(n) as u64 > MAX_BYTES {
        return undecodable("too large");
    }
    let palette = info.palette.as_deref().unwrap_or(&[]).to_vec();
    let trns = info.trns.as_deref().unwrap_or(&[]).to_vec();
    let Some(size) = reader.output_buffer_size() else { return undecodable("too large") };
    let mut raw = vec![0u8; size];
    let frame = reader.next_frame(&mut raw).map_err(refusal)?;
    if (frame.width, frame.height) != (w, h) || (frame.color_type, frame.bit_depth as usize) != (color, depth) {
        return undecodable("not the image of its header");
    }
    // a sample as the byte stb_image stores
    let scale = depth_scale(depth);
    let byte = |v: u16| if depth == 16 { (v >> 8) as u8 } else { (v as u8).wrapping_mul(scale) };
    // the tRNS colour as stb_image compares it: whole 16-bit samples, else the low bytes (all `png` keeps of
    // them) scaled as the pixels are
    let key: Vec<u16> = if depth == 16 {
        trns.as_chunks::<2>().0.iter().map(|p| u16::from_be_bytes(*p)).collect()
    } else {
        trns.iter().map(|&v| v.wrapping_mul(scale) as u16).collect()
    };
    let mut out = Vec::with_capacity(w as usize * h as usize * channels);
    for row in raw.chunks_exact(frame.line_size).take(h as usize) {
        for x in 0..w as usize {
            let mut p = [0u8, 0, 0, 255];
            match color {
                ColorType::Indexed => {
                    let i = sample(row, x, depth) as usize;
                    match palette.get(3 * i..3 * i + 3) {
                        Some(rgb) => {
                            p[..3].copy_from_slice(rgb);
                            p[3] = trns.get(i).copied().unwrap_or(255);
                        }
                        None => p[3] = 0,
                    }
                }
                _ => {
                    let mut s = [0u16; 4];
                    for (k, v) in s.iter_mut().enumerate().take(n) {
                        *v = sample(row, x * n + k, depth);
                    }
                    let colours = n - (n + 1) % 2; // 1 or 3: without the alpha sample
                    if colours == 1 {
                        p[..3].fill(byte(s[0]));
                    } else {
                        p[..3].copy_from_slice(&[byte(s[0]), byte(s[1]), byte(s[2])]);
                    }
                    if n > colours {
                        p[3] = byte(s[colours]);
                    } else if key.len() >= colours {
                        let same = (0..colours).all(|k| if depth == 16 { s[k] } else { byte(s[k]) as u16 } == key[k]);
                        p[3] = if same { 0 } else { 255 };
                    }
                }
            }
            out.extend_from_slice(&p[..channels]);
        }
    }
    Ok((w, h, out))
}

// --------------------------------------------------------------------------------------------- the texture --

// The header facts the pixels' layout follows: the colour type, whether a tRNS chunk exists, the "ee:surface" text
// (`None`: not a PNG file). A critical chunk that is not PNG's refuses the file as stb_image did (`png` skips it).
fn scan_png(d: &[u8]) -> Result<Option<(u8, bool, Vec<u8>)>, Failure> {
    const SIG: [u8; 8] = [0x89, b'P', b'N', b'G', 0x0d, 0x0a, 0x1a, 0x0a];
    if d.len() < 33 || d[..8] != SIG || &d[12..16] != b"IHDR" {
        return Ok(None);
    }
    let color = d[25];
    let mut trns = false;
    let mut surface = Vec::new();
    // stb_image's walk over memory: a read past the end gives zeros and does not move, a skip past it stops at the
    // end; a file cut short so meets a chunk of type 0 (or a partial type) and is refused by its name
    let at = |i: usize| d.get(i).copied().unwrap_or(0);
    let mut p = 8usize;
    loop {
        let n = u32::from_be_bytes([at(p), at(p + 1), at(p + 2), at(p + 3)]) as usize;
        let kind = [at(p + 4), at(p + 5), at(p + 6), at(p + 7)];
        p = (p + 8).min(d.len());
        let body = &d[p..p.saturating_add(n).min(d.len())];
        match &kind {
            b"tRNS" => trns = true,
            b"tEXt" if body.len() == n => {
                if let Some(z) = body.iter().position(|&c| c == 0) {
                    if &body[..z] == b"ee:surface" {
                        surface = body[z + 1..].to_vec(); // Latin-1 bytes; only ASCII digits are accepted below
                    }
                }
            }
            b"IEND" => break,
            // image data cut short: the decoder's "outofdata"
            b"IDAT" if body.len() < n => break,
            b"IHDR" | b"PLTE" | b"IDAT" | b"CgBI" => {}
            kind if kind[0] & 0x20 == 0 => {
                // stb_image's text, "<type> PNG chunk not known", is a C string: it ends at a NUL of the type
                let mut msg = b"PNG cannot be decoded: ".to_vec();
                msg.extend(kind.iter().take_while(|&&c| c != 0));
                if !kind.contains(&0) {
                    msg.extend_from_slice(b" PNG chunk not known");
                }
                return Err(Failure(msg));
            }
            _ => {}
        }
        p = (p + body.len() + 4).min(d.len()); // the data and the CRC
    }
    Ok(Some((color, trns, surface)))
}

fn parse_size(s: &[u8]) -> Option<u32> {
    if s.is_empty() {
        return None;
    }
    let mut v: u64 = 0;
    for &c in s {
        if !c.is_ascii_digit() {
            return None;
        }
        v = v * 10 + (c - b'0') as u64;
        if v >= 4294967296 {
            return None;
        }
    }
    Some(v as u32)
}

/// One frame of a texture as the game stores it: `width` x `height` pixels, top row first, `channels` (3 or 4) bytes
/// per pixel in R, G, B(, A) order.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PngFrame {
    pub width: u32,
    pub height: u32,
    pub channels: u32,
    pub pixels: Vec<u8>,
}

/// A texture's frames and the size of the surfaces they go into (frame 0's, or its "ee:surface" text).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PngTexture {
    pub width: u32,
    pub height: u32,
    pub frames: Vec<PngFrame>,
}

/// PNG files (frame 0, 1, ...) -> the texture `GETexture::Load` stores (MODEL_FORMATS.md 5): RGB, or RGBA when the
/// PNG has alpha (a grey+alpha or RGBA colour type, or a tRNS chunk).
pub fn png_to_texture(frames: &[&[u8]]) -> Result<PngTexture, Failure> {
    if frames.is_empty() {
        return fail("no frames");
    }
    let mut out = PngTexture { width: 0, height: 0, frames: Vec::with_capacity(frames.len()) };
    for (f, d) in frames.iter().enumerate() {
        let (frame, surface) = decode_frame(d)?;
        let (w, h) = (frame.width, frame.height);
        // the size limit of the Targa frames the game read before D9 (its 16-bit sizes), kept
        if w > 65535 || h > 65535 {
            return fail("PNG larger than 65535 pixels (a Targa's limit)");
        }
        if f == 0 {
            out.width = w;
            out.height = h;
            if !surface.is_empty() {
                let parsed = surface
                    .iter()
                    .position(|&c| c == b'x')
                    .and_then(|x| Some((parse_size(&surface[..x])?, parse_size(&surface[x + 1..])?)));
                match parsed {
                    Some((sw, sh)) => {
                        out.width = sw;
                        out.height = sh;
                    }
                    None => {
                        let mut msg = b"bad ee:surface \"".to_vec();
                        msg.extend_from_slice(&surface);
                        msg.push(b'"');
                        return Err(Failure(msg));
                    }
                }
            }
        }
        out.frames.push(frame);
    }
    Ok(out)
}
/// One PNG -> its frame (RGB, or RGBA when it has alpha: a grey+alpha or RGBA colour type, or a tRNS chunk) and its
/// "ee:surface" text: the one decoding path of the textures and the pictures.
fn decode_frame(d: &[u8]) -> Result<(PngFrame, Vec<u8>), Failure> {
    let Some((color, trns, surface)) = scan_png(d)? else { return fail("not a PNG file") };
    let ch: usize = if color == 4 || color == 6 || trns { 4 } else { 3 };
    let (width, height, pixels) = decode(d, ch)?;
    Ok((PngFrame { width, height, channels: ch as u32, pixels }, surface))
}

/// a scenario's map, a campaign background): `width` x `height` pixels, top row first, each B, G, R, A (alpha 0xff
/// when the PNG has none); `depth` is 32 when it has alpha (colour types 4 and 6 or a tRNS chunk), else 24. The
/// pixels are a texture frame's (`decode_frame`) as BGRA.
#[derive(Debug, PartialEq, Eq)]
pub struct Picture {
    pub width: u32,
    pub height: u32,
    pub depth: u32,
    pub bgra: Vec<u8>,
}

/// One PNG file -> its pixels as a `Picture` (or why it cannot be read).
pub fn png_to_picture(data: &[u8]) -> Result<Picture, Failure> {
    let (f, _) = decode_frame(data)?;
    let ch = f.channels as usize;
    let mut bgra = Vec::with_capacity(f.width as usize * f.height as usize * 4);
    for p in f.pixels.chunks_exact(ch) {
        bgra.extend_from_slice(&[p[2], p[1], p[0], if ch == 4 { p[3] } else { 0xff }]);
    }
    Ok(Picture { width: f.width, height: f.height, depth: 8 * ch as u32, bgra })
}

#[cfg(test)]
mod tests {
    use super::*;

    // ---- a tiny PNG writer (stored zlib blocks, filter 0), written by the test itself ----
    fn adler32(data: &[u8]) -> u32 {
        let (mut a, mut b) = (1u32, 0u32);
        for &v in data {
            a = (a + v as u32) % 65521;
            b = (b + a) % 65521;
        }
        (b << 16) | a
    }

    fn crc32(data: &[u8]) -> u32 {
        let mut crc = !0u32;
        for &byte in data {
            crc ^= byte as u32;
            for _ in 0..8 {
                crc = if crc & 1 != 0 { 0xedb8_8320 ^ (crc >> 1) } else { crc >> 1 };
            }
        }
        !crc
    }

    fn zlib_stored(raw: &[u8]) -> Vec<u8> {
        let mut out = vec![0x78, 0x01];
        if raw.is_empty() {
            out.extend_from_slice(&[1, 0, 0, 0xff, 0xff]);
        }
        let mut chunks = raw.chunks(65535).peekable();
        while let Some(chunk) = chunks.next() {
            out.push(if chunks.peek().is_none() { 1 } else { 0 });
            out.extend_from_slice(&(chunk.len() as u16).to_le_bytes());
            out.extend_from_slice(&(!(chunk.len() as u16)).to_le_bytes());
            out.extend_from_slice(chunk);
        }
        out.extend_from_slice(&adler32(raw).to_be_bytes());
        out
    }

    fn chunk(out: &mut Vec<u8>, kind: &[u8; 4], data: &[u8]) {
        out.extend_from_slice(&(data.len() as u32).to_be_bytes());
        out.extend_from_slice(kind);
        out.extend_from_slice(data);
        let mut crc_input = kind.to_vec();
        crc_input.extend_from_slice(data);
        out.extend_from_slice(&crc32(&crc_input).to_be_bytes());
    }

    struct PngOpts<'a> {
        depth: u8,
        color: u8,
        interlace: u8,
        plte: Option<&'a [u8]>,
        trns: Option<&'a [u8]>,
        text: Option<(&'a [u8], &'a [u8])>,
    }

    fn png(w: u32, h: u32, raw: &[u8], o: PngOpts) -> Vec<u8> {
        let mut out = vec![0x89, b'P', b'N', b'G', 0x0d, 0x0a, 0x1a, 0x0a];
        let mut ihdr = Vec::new();
        ihdr.extend_from_slice(&w.to_be_bytes());
        ihdr.extend_from_slice(&h.to_be_bytes());
        ihdr.extend_from_slice(&[o.depth, o.color, 0, 0, o.interlace]);
        chunk(&mut out, b"IHDR", &ihdr);
        if let Some(p) = o.plte {
            chunk(&mut out, b"PLTE", p);
        }
        if let Some(t) = o.trns {
            chunk(&mut out, b"tRNS", t);
        }
        if let Some((k, v)) = o.text {
            let mut body = k.to_vec();
            body.push(0);
            body.extend_from_slice(v);
            chunk(&mut out, b"tEXt", &body);
        }
        chunk(&mut out, b"IDAT", &zlib_stored(raw));
        chunk(&mut out, b"IEND", b"");
        out
    }

    fn plain(depth: u8, color: u8) -> PngOpts<'static> {
        PngOpts { depth, color, interlace: 0, plte: None, trns: None, text: None }
    }

    /// Frame 0's channels and pixels (top row first, R, G, B(, A)).
    fn frame0(png: &[u8]) -> (u32, Vec<u8>) {
        let t = png_to_texture(&[png]).unwrap();
        (t.frames[0].channels, t.frames[0].pixels.clone())
    }

    fn refused(frames: &[&[u8]]) -> Vec<u8> {
        png_to_texture(frames).unwrap_err().0
    }

    #[test]
    fn the_stored_texture_is_top_down_rgb() {
        let raw = [0, 1, 2, 3, 4, 5, 6, 0, 7, 8, 9, 10, 11, 12];
        let t = png_to_texture(&[&png(2, 2, &raw, plain(8, 2))]).unwrap();
        assert_eq!((t.width, t.height, t.frames.len()), (2, 2, 1));
        let f = &t.frames[0];
        assert_eq!((f.width, f.height, f.channels), (2, 2, 3));
        assert_eq!(f.pixels, vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]);
    }

    #[test]
    fn the_stored_texture_keeps_alpha_frames_and_the_surface_size() {
        let a = png(1, 1, &[0, 1, 2, 3, 200], PngOpts { text: Some((b"ee:surface", b"4x8")), ..plain(8, 6) });
        let b = png(1, 1, &[0, 4, 5, 6], plain(8, 2));
        let t = png_to_texture(&[&a, &b]).unwrap();
        assert_eq!((t.width, t.height), (4, 8));
        assert_eq!(t.frames[0].channels, 4);
        assert_eq!(t.frames[0].pixels, vec![1, 2, 3, 200]);
        assert_eq!(t.frames[1].channels, 3);
        assert_eq!(t.frames[1].pixels, vec![4, 5, 6]);
    }

    #[test]
    fn rgba_keeps_alpha() {
        assert_eq!(frame0(&png(1, 1, &[0, 1, 2, 3, 200], plain(8, 6))), (4, vec![1, 2, 3, 200]));
    }

    #[test]
    fn grey_4_bit_scales_by_17() {
        assert_eq!(frame0(&png(2, 1, &[0, 0xf1], plain(4, 0))), (3, vec![0xff, 0xff, 0xff, 0x11, 0x11, 0x11]));
    }

    #[test]
    fn sixteen_bit_keeps_the_high_byte() {
        let p = png(1, 1, &[0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc], plain(16, 2));
        assert_eq!(frame0(&p), (3, vec![0x12, 0x56, 0x9a]));
    }

    #[test]
    fn palette_with_trns_becomes_32_bit() {
        let plte = [10, 20, 30, 40, 50, 60];
        let trns = [128];
        let p = png(2, 1, &[0, 0, 1], PngOpts { plte: Some(&plte), trns: Some(&trns), ..plain(8, 3) });
        assert_eq!(frame0(&p), (4, vec![10, 20, 30, 128, 40, 50, 60, 255]));
    }

    #[test]
    fn grey_trns_makes_alpha_from_colour_match() {
        let trns = [0, 77];
        let p = png(2, 1, &[0, 77, 78], PngOpts { trns: Some(&trns), ..plain(8, 0) });
        // tRNS -> 4 channels
        assert_eq!(frame0(&p), (4, vec![77, 77, 77, 0, 78, 78, 78, 255]));
    }

    #[test]
    fn grey_alpha_expands() {
        assert_eq!(frame0(&png(1, 1, &[0, 200, 100], plain(8, 4))), (4, vec![200, 200, 200, 100]));
    }

    #[test]
    fn surface_text_sets_the_surface_size() {
        let p = png(1, 1, &[0, 1, 2, 3], PngOpts { text: Some((b"ee:surface", b"64x128")), ..plain(8, 2) });
        let t = png_to_texture(&[&p]).unwrap();
        assert_eq!((t.width, t.height, t.frames[0].width, t.frames[0].height), (64, 128, 1, 1));
    }

    #[test]
    fn a_bad_surface_text_is_refused() {
        let p = png(1, 1, &[0, 1, 2, 3], PngOpts { text: Some((b"ee:surface", b"64 x128")), ..plain(8, 2) });
        assert_eq!(refused(&[&p]), b"bad ee:surface \"64 x128\"".to_vec());
    }

    #[test]
    fn two_frames_use_the_first_ones_size() {
        let a = png(1, 1, &[0, 1, 2, 3], plain(8, 2));
        let b = png(2, 1, &[0, 4, 5, 6, 7, 8, 9], plain(8, 2));
        let t = png_to_texture(&[&a, &b]).unwrap();
        assert_eq!((t.width, t.height, t.frames.len()), (1, 1, 2));
        assert_eq!((t.frames[1].width, t.frames[1].pixels.len()), (2, 6));
    }

    #[test]
    fn interlaced_rgba_decodes() {
        // a 1x1 interlaced image: one pass with one pixel
        assert_eq!(
            frame0(&png(1, 1, &[0, 1, 2, 3, 4], PngOpts { interlace: 1, ..plain(8, 6) })),
            (4, vec![1, 2, 3, 4])
        );
    }

    #[test]
    fn filters_are_undone() {
        // sub (1), up (2), average (3), paeth (4) over two rows of two grey pixels
        let raw = [1, 10, 5, 2, 1, 1, 3, 2, 2, 4, 1, 1];
        let (ch, data) = frame0(&png(2, 3, &raw, plain(8, 0)));
        assert_eq!(ch, 3);
        // rows: [10, 15], [11, 16], then average [7+2=(11>>1)+2=7, (7+16)>>1+2=13]; check grey values
        assert_eq!(&data[0..3], &[10, 10, 10]);
        assert_eq!(&data[3..6], &[15, 15, 15]);
        assert_eq!(&data[6..9], &[11, 11, 11]);
        assert_eq!(&data[9..12], &[16, 16, 16]);
        assert_eq!(&data[12..15], &[7, 7, 7]);
        assert_eq!(&data[15..18], &[13, 13, 13]);
    }

    #[test]
    fn not_a_png() {
        assert_eq!(refused(&[b"hello"]), b"not a PNG file".to_vec());
        assert_eq!(refused(&[]), b"no frames".to_vec());
    }

    #[test]
    fn a_truncated_file_is_refused_with_stbs_reason() {
        let p = png(1, 1, &[0, 1, 2, 3], plain(8, 2));
        // cut inside the IDAT chunk
        let idat = p.windows(4).position(|w| w == b"IDAT").unwrap();
        let cut = &p[..idat + 8];
        assert_eq!(refused(&[cut]), b"PNG cannot be decoded: outofdata".to_vec());
    }

    #[test]
    fn missing_pixels_are_refused() {
        let p = png(1, 2, &[0, 1, 2, 3], plain(8, 2));
        assert_eq!(
            refused(&[&p]),
            b"PNG cannot be decoded: IDAT or fDAT chunk does not have enough data for image.".to_vec()
        );
    }

    #[test]
    fn crcs_are_not_checked() {
        let mut p = png(1, 1, &[0, 1, 2, 3], plain(8, 2));
        let n = p.len();
        p[n - 1] ^= 0xff; // the IEND crc
        p[n - 12 - 1] ^= 0xff; // the IDAT crc
        assert!(png_to_texture(&[&p]).is_ok());
    }

    #[test]
    fn extra_rows_of_data_are_tolerated_and_a_bad_filter_is_not() {
        let p = png(1, 1, &[0, 1, 2, 3, 99, 99, 99, 99], plain(8, 2));
        assert_eq!(frame0(&p), (3, vec![1, 2, 3]));
        let bad = png(1, 1, &[9, 1, 2, 3], plain(8, 2));
        assert_eq!(refused(&[&bad]), b"PNG cannot be decoded: Unknown filter method 9.".to_vec());
    }

    #[test]
    fn unknown_critical_chunks_are_named_and_ancillary_ones_skipped() {
        let mut p = png(1, 1, &[0, 1, 2, 3], plain(8, 2));
        let iend = p.windows(4).position(|w| w == b"IEND").unwrap() - 4;
        let mut extra = Vec::new();
        chunk(&mut extra, b"aaAa", b"xy"); // ancillary: skipped
        p.splice(iend..iend, extra);
        assert!(png_to_texture(&[&p]).is_ok());
        let mut q = png(1, 1, &[0, 1, 2, 3], plain(8, 2));
        let iend = q.windows(4).position(|w| w == b"IEND").unwrap() - 4;
        let mut extra = Vec::new();
        chunk(&mut extra, b"ABCD", b"");
        q.splice(iend..iend, extra);
        assert_eq!(refused(&[&q]), b"PNG cannot be decoded: ABCD PNG chunk not known".to_vec());
    }

    #[test]
    fn surface_sizes_are_decimal_and_32_bit() {
        assert!(parse_size(b"123").is_some());
        assert_eq!(parse_size(b"99999999999"), None);
        assert_eq!(parse_size(b""), None);
    }

    #[test]
    fn scan_png_finds_trns_and_surface() {
        let p = png(1, 1, &[0, 1], PngOpts { trns: Some(&[0, 0]), text: Some((b"ee:surface", b"2x2")), ..plain(8, 0) });
        let (color, trns, surface) = scan_png(&p).unwrap().unwrap();
        assert_eq!((color, trns, surface.as_slice()), (0, true, &b"2x2"[..]));
        assert_eq!(scan_png(&p[..20]), Ok(None));
    }

    #[test]
    fn a_picture_is_top_down_bgra() {
        let raw = [0, 1, 2, 3, 4, 5, 6, 0, 7, 8, 9, 10, 11, 12];
        let p = png(2, 2, &raw, plain(8, 2));
        let pic = png_to_picture(&p).unwrap();
        assert_eq!((pic.width, pic.height, pic.depth), (2, 2, 24));
        assert_eq!(pic.bgra, vec![3, 2, 1, 255, 6, 5, 4, 255, 9, 8, 7, 255, 12, 11, 10, 255]);
    }

    #[test]
    fn a_picture_keeps_alpha_and_trns() {
        let p = png(1, 1, &[0, 1, 2, 3, 200], plain(8, 6));
        assert_eq!(png_to_picture(&p).unwrap(), Picture { width: 1, height: 1, depth: 32, bgra: vec![3, 2, 1, 200] });
        let plte = [10, 20, 30, 40, 50, 60];
        let p = png(2, 1, &[0, 0, 1], PngOpts { plte: Some(&plte), trns: Some(&[128]), ..plain(8, 3) });
        let pic = png_to_picture(&p).unwrap();
        assert_eq!((pic.depth, pic.bgra), (32, vec![30, 20, 10, 128, 60, 50, 40, 255]));
    }

    #[test]
    fn a_picture_equals_the_texture_frame() {
        // the texture's frame (top-down RGB(A)) holds the same pixels as the picture (top-down BGRA)
        let raw = [0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 9, 10, 11, 12, 13, 14, 15, 16];
        let p = png(2, 2, &raw, plain(8, 6));
        let pic = png_to_picture(&p).unwrap();
        let t = png_to_texture(&[&p]).unwrap();
        let swapped: Vec<u8> =
            t.frames[0].pixels.as_chunks::<4>().0.iter().flat_map(|c| [c[2], c[1], c[0], c[3]]).collect();
        assert_eq!(pic.bgra, swapped);
    }

    #[test]
    fn a_picture_refuses_what_is_not_a_png() {
        assert_eq!(png_to_picture(b"not a png"), Err(Failure(b"not a PNG file".to_vec())));
        let mut p = png(1, 1, &[0, 1, 2, 3], plain(8, 2));
        assert_eq!(p[41], 0x78); // the IDAT's zlib header (after the signature, IHDR and the IDAT's length and type)
        p[41] = 0x79;
        assert_eq!(
            png_to_picture(&p),
            Err(Failure(b"PNG cannot be decoded: Corrupt deflate stream. BadZlibHeader".to_vec()))
        );
    }

    #[test]
    fn a_file_cut_in_a_chunk_header_is_refused_by_the_type_read_so_far() {
        let p = png(2, 2, &[0; 2 * 7], plain(8, 2));
        let idat = p.windows(4).position(|w| w == b"IDAT").unwrap();
        // the cut leaves "ID" of the type: stb_image reads "ID" + two zero bytes, a C string "ID"
        assert_eq!(refused(&[&p[..idat + 2]]), b"PNG cannot be decoded: ID".to_vec());
        // cut at a chunk's start: a chunk of type 0, the empty name
        assert_eq!(refused(&[&p[..idat - 4]]), b"PNG cannot be decoded: ".to_vec());
    }
}
