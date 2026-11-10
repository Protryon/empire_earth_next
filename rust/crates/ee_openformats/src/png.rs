//! PNG -> the texture the game stores: the texture reader of GEOpenFormats.cpp (`PngToSst`, `ScanPng`) over a port of
//! stb_image 2.30's PNG path (zlib inflate, the chunk walk, unfiltering, de-interlacing, the failure reasons). A hand
//! port, not the `png` crate, so that truncated and malformed files are accepted or refused and give the pixels exactly
//! as before: stb
//! ignores CRCs, tolerates extra bytes after the image data and fills short files with zeros, which `png` does not.
#![allow(clippy::needless_range_loop)]

use std::cell::Cell;

use crate::{fail, Failure};

const MAX_DIMENSIONS: u32 = 1 << 24;
const ZFAST_BITS: usize = 9;
const ZFAST_MASK: u32 = (1 << ZFAST_BITS) - 1;
const ZNSYMS: usize = 288;

thread_local! {
    // stbi_failure_reason: thread-local and sticky across calls; the C string it returned
    static REASON: Cell<Option<&'static [u8]>> = const { Cell::new(None) };
    static INVALID_CHUNK: Cell<[u8; 4]> = const { Cell::new([0; 4]) };
}

fn set_reason(reason: &'static [u8]) {
    REASON.with(|r| r.set(Some(reason)));
}

/// What failure `stbi_failure_reason()` reports now: the text up to its first NUL (an unknown chunk's type bytes can
/// put a NUL in its message); nothing was ever set: empty (the C++ would pass a null pointer to std::string here).
fn reason() -> Vec<u8> {
    let r = REASON.with(|r| r.get()).unwrap_or(b"");
    if r == UNKNOWN_CHUNK {
        let chunk = INVALID_CHUNK.with(|c| c.get());
        let mut msg = chunk.to_vec();
        msg.extend_from_slice(b" PNG chunk not known");
        let end = msg.iter().position(|&c| c == 0).unwrap_or(msg.len());
        msg.truncate(end);
        return msg;
    }
    r.to_vec()
}

const UNKNOWN_CHUNK: &[u8] = b"<unknown chunk>";

// ------------------------------------------------------------------------------------------------- the zlib part --

#[derive(Clone)]
struct Huffman {
    fast: Box<[u16; 1 << ZFAST_BITS]>,
    firstcode: [u16; 16],
    maxcode: [i32; 17],
    firstsymbol: [u16; 16],
    size: [u8; ZNSYMS],
    value: [u16; ZNSYMS],
}

impl Default for Huffman {
    fn default() -> Self {
        Huffman {
            fast: Box::new([0; 1 << ZFAST_BITS]),
            firstcode: [0; 16],
            maxcode: [0; 17],
            firstsymbol: [0; 16],
            size: [0; ZNSYMS],
            value: [0; ZNSYMS],
        }
    }
}

fn bitreverse16(n: u32) -> u32 {
    let mut n = n;
    n = ((n & 0xAAAA) >> 1) | ((n & 0x5555) << 1);
    n = ((n & 0xCCCC) >> 2) | ((n & 0x3333) << 2);
    n = ((n & 0xF0F0) >> 4) | ((n & 0x0F0F) << 4);
    n = ((n & 0xFF00) >> 8) | ((n & 0x00FF) << 8);
    n
}

fn bit_reverse(v: u32, bits: u32) -> u32 {
    bitreverse16(v) >> (16 - bits)
}

fn zbuild_huffman(z: &mut Huffman, sizelist: &[u8]) -> Result<(), ()> {
    let mut k = 0usize;
    let mut next_code = [0i32; 16];
    let mut sizes = [0i32; 17];
    z.fast.fill(0);
    for &s in sizelist {
        sizes[s as usize] += 1;
    }
    sizes[0] = 0;
    for i in 1..16 {
        if sizes[i] > (1 << i) {
            set_reason(b"bad sizes");
            return Err(());
        }
    }
    let mut code = 0i32;
    for i in 1..16 {
        next_code[i] = code;
        z.firstcode[i] = code as u16;
        z.firstsymbol[i] = k as u16;
        code += sizes[i];
        if sizes[i] != 0 && code > (1 << i) {
            set_reason(b"bad codelengths");
            return Err(());
        }
        z.maxcode[i] = code << (16 - i);
        code <<= 1;
        k += sizes[i] as usize;
    }
    z.maxcode[16] = 0x10000;
    for (i, &sz) in sizelist.iter().enumerate() {
        let s = sz as usize;
        if s != 0 {
            let c = next_code[s] as usize - z.firstcode[s] as usize + z.firstsymbol[s] as usize;
            let fastv = ((s as u16) << 9) | i as u16;
            z.size[c] = sz;
            z.value[c] = i as u16;
            if s <= ZFAST_BITS {
                let mut j = bit_reverse(next_code[s] as u32, s as u32) as usize;
                while j < (1 << ZFAST_BITS) {
                    z.fast[j] = fastv;
                    j += 1 << s;
                }
            }
            next_code[s] += 1;
        }
    }
    Ok(())
}

struct Zbuf<'a> {
    input: &'a [u8],
    pos: usize,
    num_bits: i32,
    hit_zeof_once: bool,
    code_buffer: u32,
    out: Vec<u8>,
    length: Huffman,
    distance: Huffman,
}

const LENGTH_BASE: [i32; 31] = [
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258,
    0, 0,
];
const LENGTH_EXTRA: [i32; 31] =
    [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 0, 0];
const DIST_BASE: [i32; 32] = [
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577, 0, 0,
];
const DIST_EXTRA: [i32; 32] =
    [0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 0, 0];

impl Zbuf<'_> {
    fn zeof(&self) -> bool {
        self.pos >= self.input.len()
    }

    fn zget8(&mut self) -> u8 {
        if self.zeof() {
            0
        } else {
            let v = self.input[self.pos];
            self.pos += 1;
            v
        }
    }

    fn fill_bits(&mut self) {
        loop {
            if self.code_buffer >= (1u32 << self.num_bits) {
                self.pos = self.input.len(); // treat this as EOF so we fail
                return;
            }
            self.code_buffer |= (self.zget8() as u32) << self.num_bits;
            self.num_bits += 8;
            if self.num_bits > 24 {
                break;
            }
        }
    }

    fn receive(&mut self, n: i32) -> u32 {
        if self.num_bits < n {
            self.fill_bits();
        }
        let k = self.code_buffer & ((1u32 << n) - 1);
        self.code_buffer >>= n;
        self.num_bits -= n;
        k
    }

    fn decode_slow(&mut self, z: &Huffman) -> i32 {
        let k = bit_reverse(self.code_buffer, 16) as i32;
        let mut s = ZFAST_BITS + 1;
        loop {
            if k < z.maxcode[s] {
                break;
            }
            s += 1;
        }
        if s >= 16 {
            return -1;
        }
        let b = (k >> (16 - s)) - z.firstcode[s] as i32 + z.firstsymbol[s] as i32;
        if b as usize >= ZNSYMS {
            return -1;
        }
        if z.size[b as usize] as usize != s {
            return -1;
        }
        self.code_buffer >>= s;
        self.num_bits -= s as i32;
        z.value[b as usize] as i32
    }

    fn decode(&mut self, z: &Huffman) -> i32 {
        if self.num_bits < 16 {
            if self.zeof() {
                if !self.hit_zeof_once {
                    self.hit_zeof_once = true;
                    self.num_bits += 16;
                } else {
                    return -1;
                }
            } else {
                self.fill_bits();
            }
        }
        let b = z.fast[(self.code_buffer & ZFAST_MASK) as usize] as i32;
        if b != 0 {
            let s = b >> 9;
            self.code_buffer >>= s;
            self.num_bits -= s;
            return b & 511;
        }
        self.decode_slow(z)
    }

    fn parse_huffman_block(&mut self) -> Result<(), ()> {
        // the two tables are read while the bit reader mutates: taken out for the duration
        let length = std::mem::take(&mut self.length);
        let distance = std::mem::take(&mut self.distance);
        let result = self.huffman_loop(&length, &distance);
        self.length = length;
        self.distance = distance;
        result
    }

    fn huffman_loop(&mut self, length: &Huffman, distance: &Huffman) -> Result<(), ()> {
        loop {
            let mut z = self.decode(length);
            if z < 256 {
                if z < 0 {
                    set_reason(b"bad huffman code");
                    return Err(());
                }
                self.out.push(z as u8);
            } else {
                if z == 256 {
                    if self.hit_zeof_once && self.num_bits < 16 {
                        set_reason(b"unexpected end");
                        return Err(());
                    }
                    return Ok(());
                }
                if z >= 286 {
                    set_reason(b"bad huffman code");
                    return Err(());
                }
                z -= 257;
                let mut len = LENGTH_BASE[z as usize];
                if LENGTH_EXTRA[z as usize] != 0 {
                    len += self.receive(LENGTH_EXTRA[z as usize]) as i32;
                }
                let z = self.decode(distance);
                if !(0..30).contains(&z) {
                    set_reason(b"bad huffman code");
                    return Err(());
                }
                let mut dist = DIST_BASE[z as usize];
                if DIST_EXTRA[z as usize] != 0 {
                    dist += self.receive(DIST_EXTRA[z as usize]) as i32;
                }
                if (self.out.len() as i64) < dist as i64 {
                    set_reason(b"bad dist");
                    return Err(());
                }
                let start = self.out.len() - dist as usize;
                if dist == 1 {
                    let v = self.out[start];
                    self.out.extend(std::iter::repeat_n(v, len as usize));
                } else {
                    for i in 0..len as usize {
                        let v = self.out[start + i];
                        self.out.push(v);
                    }
                }
            }
        }
    }

    fn compute_huffman_codes(&mut self) -> Result<(), ()> {
        const LENGTH_DEZIGZAG: [u8; 19] = [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15];
        let mut codelength = Huffman::default();
        let mut lencodes = [0u8; 286 + 32 + 137];
        let mut codelength_sizes = [0u8; 19];
        let hlit = self.receive(5) as i32 + 257;
        let hdist = self.receive(5) as i32 + 1;
        let hclen = self.receive(4) as i32 + 4;
        let ntot = hlit + hdist;
        for i in 0..hclen as usize {
            let s = self.receive(3);
            codelength_sizes[LENGTH_DEZIGZAG[i] as usize] = s as u8;
        }
        zbuild_huffman(&mut codelength, &codelength_sizes)?;
        let mut n = 0i32;
        while n < ntot {
            let mut c = self.decode(&codelength);
            if !(0..19).contains(&c) {
                set_reason(b"bad codelengths");
                return Err(());
            }
            if c < 16 {
                lencodes[n as usize] = c as u8;
                n += 1;
            } else {
                let mut fill = 0u8;
                if c == 16 {
                    c = self.receive(2) as i32 + 3;
                    if n == 0 {
                        set_reason(b"bad codelengths");
                        return Err(());
                    }
                    fill = lencodes[n as usize - 1];
                } else if c == 17 {
                    c = self.receive(3) as i32 + 3;
                } else if c == 18 {
                    c = self.receive(7) as i32 + 11;
                } else {
                    set_reason(b"bad codelengths");
                    return Err(());
                }
                if ntot - n < c {
                    set_reason(b"bad codelengths");
                    return Err(());
                }
                for k in 0..c as usize {
                    lencodes[n as usize + k] = fill;
                }
                n += c;
            }
        }
        if n != ntot {
            set_reason(b"bad codelengths");
            return Err(());
        }
        zbuild_huffman(&mut self.length, &lencodes[..hlit as usize])?;
        zbuild_huffman(&mut self.distance, &lencodes[hlit as usize..(hlit + hdist) as usize])?;
        Ok(())
    }

    fn parse_uncompressed_block(&mut self) -> Result<(), ()> {
        let mut header = [0u8; 4];
        if self.num_bits & 7 != 0 {
            let n = self.num_bits & 7;
            self.receive(n);
        }
        let mut k = 0usize;
        while self.num_bits > 0 {
            header[k] = (self.code_buffer & 255) as u8;
            k += 1;
            self.code_buffer >>= 8;
            self.num_bits -= 8;
        }
        if self.num_bits < 0 {
            set_reason(b"zlib corrupt");
            return Err(());
        }
        while k < 4 {
            header[k] = self.zget8();
            k += 1;
        }
        let len = header[1] as usize * 256 + header[0] as usize;
        let nlen = header[3] as usize * 256 + header[2] as usize;
        if nlen != (len ^ 0xffff) {
            set_reason(b"zlib corrupt");
            return Err(());
        }
        if self.pos + len > self.input.len() {
            set_reason(b"read past buffer");
            return Err(());
        }
        self.out.extend_from_slice(&self.input[self.pos..self.pos + len]);
        self.pos += len;
        Ok(())
    }

    fn parse_zlib_header(&mut self) -> Result<(), ()> {
        let cmf = self.zget8() as i32;
        let cm = cmf & 15;
        let flg = self.zget8() as i32;
        if self.zeof() {
            set_reason(b"bad zlib header");
            return Err(());
        }
        if (cmf * 256 + flg) % 31 != 0 {
            set_reason(b"bad zlib header");
            return Err(());
        }
        if flg & 32 != 0 {
            set_reason(b"no preset dict");
            return Err(());
        }
        if cm != 8 {
            set_reason(b"bad compression");
            return Err(());
        }
        Ok(())
    }

    fn parse_zlib(&mut self, parse_header: bool) -> Result<(), ()> {
        if parse_header {
            self.parse_zlib_header()?;
        }
        self.num_bits = 0;
        self.code_buffer = 0;
        self.hit_zeof_once = false;
        loop {
            let final_block = self.receive(1);
            let ty = self.receive(2);
            if ty == 0 {
                self.parse_uncompressed_block()?;
            } else if ty == 3 {
                return Err(()); // no reason set, as in the C
            } else {
                if ty == 1 {
                    zbuild_huffman(&mut self.length, &zdefault_length())?;
                    zbuild_huffman(&mut self.distance, &[5u8; 32])?;
                } else {
                    self.compute_huffman_codes()?;
                }
                self.parse_huffman_block()?;
            }
            if final_block != 0 {
                return Ok(());
            }
        }
    }
}

fn zdefault_length() -> [u8; ZNSYMS] {
    let mut l = [8u8; ZNSYMS];
    l[144..256].fill(9);
    l[256..280].fill(7);
    l
}

/// The zlib stream -> its bytes (`stbi_zlib_decode_malloc_guesssize_headerflag`, without the size guess).
pub(crate) fn zlib_decode(input: &[u8], parse_header: bool) -> Result<Vec<u8>, ()> {
    let mut z = Zbuf {
        input,
        pos: 0,
        num_bits: 0,
        hit_zeof_once: false,
        code_buffer: 0,
        out: Vec::new(),
        length: Huffman::default(),
        distance: Huffman::default(),
    };
    z.parse_zlib(parse_header)?;
    Ok(z.out)
}

// ----------------------------------------------------------------------------------------- the PNG chunk walk --

#[derive(Clone, Copy)]
struct Context<'a> {
    data: &'a [u8],
    pos: usize,
}

impl Context<'_> {
    fn get8(&mut self) -> u8 {
        if self.pos < self.data.len() {
            let v = self.data[self.pos];
            self.pos += 1;
            v
        } else {
            0
        }
    }

    fn get16be(&mut self) -> u32 {
        let z = self.get8() as u32;
        (z << 8) + self.get8() as u32
    }

    fn get32be(&mut self) -> u32 {
        let z = self.get16be();
        (z << 16).wrapping_add(self.get16be())
    }

    fn skip(&mut self, n: i32) {
        if n == 0 {
            return;
        }
        if n < 0 {
            self.pos = self.data.len();
            return;
        }
        self.pos = self.pos.saturating_add(n as usize);
    }

    fn getn(&mut self, n: usize) -> Option<&[u8]> {
        let end = self.pos.checked_add(n)?;
        if end > self.data.len() {
            return None;
        }
        let s = &self.data[self.pos..end];
        self.pos = end;
        Some(s)
    }
}

// stbi__mad3sizes_valid and friends: a*b*c (+add) fits in a non-negative int
fn mul_valid(a: i64, b: i64) -> bool {
    a >= 0 && b >= 0 && (b == 0 || a <= i32::MAX as i64 / b)
}

fn mad3_valid(a: i64, b: i64, c: i64, add: i64) -> bool {
    mul_valid(a, b) && mul_valid(a * b, c) && a * b * c <= i32::MAX as i64 - add
}

fn mad2_valid(a: i64, b: i64, add: i64) -> bool {
    mul_valid(a, b) && a * b <= i32::MAX as i64 - add
}

struct Image {
    img_x: u32,
    img_y: u32,
    img_n: i32,
    img_out_n: i32,
    depth: i32,
    out: Vec<u8>,
}

const DEPTH_SCALE_TABLE: [u8; 9] = [0, 0xff, 0x55, 0, 0x11, 0, 0, 0, 0x01];
const FIRST_ROW_FILTER: [u8; 5] = [0, 1, 0, 5, 1]; // 5: avg_first

fn alpha_expand8(dest: &mut [u8], src_start_in_dest: Option<&[u8]>, x: usize, img_n: usize) {
    // dest == src is legal in the C; when no separate source is given, the source is dest's own first bytes
    let temp: Vec<u8>;
    let src: &[u8] = match src_start_in_dest {
        Some(s) => s,
        None => {
            temp = dest[..x * img_n].to_vec();
            &temp
        }
    };
    if img_n == 1 {
        for i in (0..x).rev() {
            dest[i * 2 + 1] = 255;
            dest[i * 2] = src[i];
        }
    } else {
        for i in (0..x).rev() {
            dest[i * 4 + 3] = 255;
            dest[i * 4 + 2] = src[i * 3 + 2];
            dest[i * 4 + 1] = src[i * 3 + 1];
            dest[i * 4] = src[i * 3];
        }
    }
}

// stbi__create_png_image_raw: decode rows of `x` by `y` samples into a newly allocated out (2 bytes per sample,
// low byte first, when the depth is 16)
fn create_png_image_raw(
    a: &mut Image,
    raw: &[u8],
    out_n: i32,
    x: u32,
    y: u32,
    depth: i32,
    color: u8,
) -> Result<(), ()> {
    let bytes = if depth == 16 { 2 } else { 1 };
    let img_n = a.img_n;
    let stride = (x as i64) * out_n as i64 * bytes;
    let output_bytes = out_n as i64 * bytes;
    let mut filter_bytes = img_n as i64 * bytes;
    let mut width = x as i64;

    if !mad3_valid(x as i64, y as i64, output_bytes, 0) {
        set_reason(b"outofmem");
        return Err(());
    }
    a.out = vec![0u8; x as usize * y as usize * output_bytes as usize];
    if !mad3_valid(img_n as i64, x as i64, depth as i64, 7) {
        set_reason(b"too large");
        return Err(());
    }
    let img_width_bytes = ((img_n as i64 * x as i64 * depth as i64) + 7) >> 3;
    if !mad2_valid(img_width_bytes, y as i64, img_width_bytes) {
        set_reason(b"too large");
        return Err(());
    }
    let img_len = (img_width_bytes + 1) * y as i64;
    if (raw.len() as i64) < img_len {
        set_reason(b"not enough pixels");
        return Err(());
    }
    let iwb = img_width_bytes as usize;
    let mut filter_buf = vec![0u8; iwb * 2];
    if depth < 8 {
        filter_bytes = 1;
        width = img_width_bytes;
    }
    let nk = (width * filter_bytes) as usize;
    let fb = filter_bytes as usize;
    let mut rp = 0usize;
    for j in 0..y as usize {
        let (cur_slot, prior_slot) = (j & 1, (!j) & 1);
        let mut filter = raw[rp];
        rp += 1;
        if filter > 4 {
            set_reason(b"invalid filter");
            return Err(());
        }
        if j == 0 {
            filter = FIRST_ROW_FILTER[filter as usize];
        }
        let (first, second) = filter_buf.split_at_mut(iwb);
        let (cur, prior): (&mut [u8], &mut [u8]) = if cur_slot == 0 { (first, second) } else { (second, first) };
        let _ = prior_slot;
        let rawline = &raw[rp..rp + nk];
        match filter {
            0 => cur[..nk].copy_from_slice(rawline),
            1 => {
                cur[..fb].copy_from_slice(&rawline[..fb]);
                for k in fb..nk {
                    cur[k] = rawline[k].wrapping_add(cur[k - fb]);
                }
            }
            2 => {
                for k in 0..nk {
                    cur[k] = rawline[k].wrapping_add(prior[k]);
                }
            }
            3 => {
                for k in 0..fb {
                    cur[k] = rawline[k].wrapping_add(prior[k] >> 1);
                }
                for k in fb..nk {
                    cur[k] = rawline[k].wrapping_add(((prior[k] as i32 + cur[k - fb] as i32) >> 1) as u8);
                }
            }
            4 => {
                for k in 0..fb {
                    cur[k] = rawline[k].wrapping_add(prior[k]);
                }
                for k in fb..nk {
                    cur[k] =
                        rawline[k].wrapping_add(paeth(cur[k - fb] as i32, prior[k] as i32, prior[k - fb] as i32) as u8);
                }
            }
            5 => {
                cur[..fb].copy_from_slice(&rawline[..fb]);
                for k in fb..nk {
                    cur[k] = rawline[k].wrapping_add(cur[k - fb] >> 1);
                }
            }
            _ => unreachable!(),
        }
        rp += nk;
        let dest_start = stride as usize * j;
        let nsmp = x as usize * img_n as usize;
        if depth < 8 {
            let scale = if color == 0 { DEPTH_SCALE_TABLE[depth as usize] } else { 1 };
            let mut in_i = 0usize;
            let mut inb = 0u8;
            let dest = &mut a.out[dest_start..];
            let (per_byte_mask, shift) = match depth {
                4 => (1usize, 4u32),
                2 => (3, 6),
                _ => (7, 7),
            };
            for i in 0..nsmp {
                if i & per_byte_mask == 0 {
                    inb = cur[in_i];
                    in_i += 1;
                }
                dest[i] = scale.wrapping_mul(inb >> shift);
                inb <<= depth as u32;
            }
            if img_n != out_n {
                alpha_expand8(dest, None, x as usize, img_n as usize);
            }
        } else if depth == 8 {
            let dest = &mut a.out[dest_start..];
            if img_n == out_n {
                dest[..nsmp].copy_from_slice(&cur[..nsmp]);
            } else {
                alpha_expand8(dest, Some(&cur[..nsmp]), x as usize, img_n as usize);
            }
        } else {
            // 16 bit: each sample as a u16 in memory (the C writes native u16s), low byte first here
            let dest = &mut a.out[dest_start..];
            let mut src = 0usize;
            let mut put = 0usize;
            let write = |d: &mut [u8], put: &mut usize, high: u8, low: u8| {
                d[*put] = low;
                d[*put + 1] = high;
                *put += 2;
            };
            if img_n == out_n {
                for _ in 0..nsmp {
                    write(dest, &mut put, cur[src], cur[src + 1]);
                    src += 2;
                }
            } else if img_n == 1 {
                for _ in 0..x {
                    write(dest, &mut put, cur[src], cur[src + 1]);
                    src += 2;
                    write(dest, &mut put, 0xff, 0xff);
                }
            } else {
                for _ in 0..x {
                    write(dest, &mut put, cur[src], cur[src + 1]);
                    write(dest, &mut put, cur[src + 2], cur[src + 3]);
                    write(dest, &mut put, cur[src + 4], cur[src + 5]);
                    src += 6;
                    write(dest, &mut put, 0xff, 0xff);
                }
            }
        }
    }
    Ok(())
}

fn paeth(a: i32, b: i32, c: i32) -> i32 {
    let thresh = c * 3 - (a + b);
    let lo = a.min(b);
    let hi = a.max(b);
    let t0 = if hi <= thresh { lo } else { c };
    if thresh <= lo {
        hi
    } else {
        t0
    }
}

fn create_png_image(a: &mut Image, data: &[u8], out_n: i32, depth: i32, color: u8, interlaced: bool) -> Result<(), ()> {
    let (x0, y0) = (a.img_x, a.img_y);
    if !interlaced {
        return create_png_image_raw(a, data, out_n, x0, y0, depth, color);
    }
    let bytes = if depth == 16 { 2 } else { 1 };
    let out_bytes = out_n as i64 * bytes;
    if !mad3_valid(x0 as i64, y0 as i64, out_bytes, 0) {
        set_reason(b"outofmem");
        return Err(());
    }
    let mut final_img = vec![0u8; x0 as usize * y0 as usize * out_bytes as usize];
    let mut rest = data;
    const XORIG: [i64; 7] = [0, 4, 0, 2, 0, 1, 0];
    const YORIG: [i64; 7] = [0, 0, 4, 0, 2, 0, 1];
    const XSPC: [i64; 7] = [8, 8, 4, 4, 2, 2, 1];
    const YSPC: [i64; 7] = [8, 8, 8, 4, 4, 2, 2];
    for p in 0..7 {
        let x = (x0 as i64 - XORIG[p] + XSPC[p] - 1) / XSPC[p];
        let y = (y0 as i64 - YORIG[p] + YSPC[p] - 1) / YSPC[p];
        if x != 0 && y != 0 {
            let img_len = ((((a.img_n as i64 * x * depth as i64) + 7) >> 3) + 1) * y;
            create_png_image_raw(a, rest, out_n, x as u32, y as u32, depth, color)?;
            let ob = out_bytes as usize;
            for j in 0..y {
                for i in 0..x {
                    let out_y = j * YSPC[p] + YORIG[p];
                    let out_x = i * XSPC[p] + XORIG[p];
                    let to = (out_y as usize * x0 as usize + out_x as usize) * ob;
                    let from = (j as usize * x as usize + i as usize) * ob;
                    final_img[to..to + ob].copy_from_slice(&a.out[from..from + ob]);
                }
            }
            rest = &rest[(img_len as usize).min(rest.len())..];
        }
    }
    a.out = final_img;
    Ok(())
}

fn compute_transparency(z: &mut Image, tc: [u8; 3], out_n: i32) {
    let pixels = z.img_x as usize * z.img_y as usize;
    if out_n == 2 {
        for p in z.out.as_chunks_mut::<2>().0.iter_mut().take(pixels) {
            p[1] = if p[0] == tc[0] { 0 } else { 255 };
        }
    } else {
        for p in z.out.as_chunks_mut::<4>().0.iter_mut().take(pixels) {
            if p[0] == tc[0] && p[1] == tc[1] && p[2] == tc[2] {
                p[3] = 0;
            }
        }
    }
}

fn compute_transparency16(z: &mut Image, tc: [u16; 3], out_n: i32) {
    let pixels = z.img_x as usize * z.img_y as usize;
    let sample = |p: &[u8], i: usize| u16::from_le_bytes([p[i * 2], p[i * 2 + 1]]);
    if out_n == 2 {
        for p in z.out.as_chunks_mut::<4>().0.iter_mut().take(pixels) {
            let v: u16 = if sample(p, 0) == tc[0] { 0 } else { 65535 };
            p[2..4].copy_from_slice(&v.to_le_bytes());
        }
    } else {
        for p in z.out.as_chunks_mut::<8>().0.iter_mut().take(pixels) {
            if sample(p, 0) == tc[0] && sample(p, 1) == tc[1] && sample(p, 2) == tc[2] {
                p[6..8].copy_from_slice(&[0, 0]);
            }
        }
    }
}

fn expand_palette(a: &mut Image, palette: &[u8; 1024], pal_img_n: usize) -> Result<(), ()> {
    let pixels = a.img_x as usize * a.img_y as usize;
    let mut out = Vec::with_capacity(pixels * pal_img_n);
    for i in 0..pixels {
        let n = a.out[i] as usize * 4;
        out.extend_from_slice(&palette[n..n + pal_img_n]);
    }
    a.out = out;
    Ok(())
}

/// A decoded image as 8-bit samples: width, height, channels in the file, `out` in the requested channel count.
struct Decoded {
    w: u32,
    h: u32,
    out: Vec<u8>,
}

fn load_png(d: &[u8], req_comp: i32) -> Option<Decoded> {
    let mut s = Context { data: d, pos: 0 };
    let mut img = Image { img_x: 0, img_y: 0, img_n: 0, img_out_n: 0, depth: 0, out: Vec::new() };
    if !parse_png_file(&mut s, &mut img, req_comp) {
        return None;
    }
    let mut result = std::mem::take(&mut img.out);
    let pixels = img.img_x as usize * img.img_y as usize;
    // ri.bits_per_channel and stbi__convert_format[16]: 16-bit data keeps its high bytes (no arithmetic in between)
    if img.depth == 16 {
        result = result.as_chunks::<2>().0.iter().map(|b| b[1]).collect();
    }
    if req_comp != img.img_out_n {
        result = convert_format(&result, img.img_out_n, req_comp, pixels);
        img.img_out_n = req_comp;
    }
    Some(Decoded { w: img.img_x, h: img.img_y, out: result })
}

fn parse_png_file(s: &mut Context, z: &mut Image, req_comp: i32) -> bool {
    match parse_png_inner(s, z, req_comp) {
        Ok(()) => true,
        Err(()) => false,
    }
}

const fn png_type(a: u8, b: u8, c: u8, d: u8) -> u32 {
    ((a as u32) << 24) + ((b as u32) << 16) + ((c as u32) << 8) + d as u32
}

const TYPE_CGBI: u32 = png_type(b'C', b'g', b'B', b'I');
const TYPE_IHDR: u32 = png_type(b'I', b'H', b'D', b'R');
const TYPE_PLTE: u32 = png_type(b'P', b'L', b'T', b'E');
const TYPE_TRNS: u32 = png_type(b't', b'R', b'N', b'S');
const TYPE_IDAT: u32 = png_type(b'I', b'D', b'A', b'T');
const TYPE_IEND: u32 = png_type(b'I', b'E', b'N', b'D');

fn err(reason: &'static [u8]) -> Result<(), ()> {
    set_reason(reason);
    Err(())
}

fn parse_png_inner(s: &mut Context, z: &mut Image, req_comp: i32) -> Result<(), ()> {
    const SIG: [u8; 8] = [137, 80, 78, 71, 13, 10, 26, 10];
    for b in SIG {
        if s.get8() != b {
            return err(b"bad png sig");
        }
    }
    let mut palette = [0u8; 1024];
    let mut pal_img_n: u8 = 0;
    let mut has_trans = false;
    let mut tc = [0u8; 3];
    let mut tc16 = [0u16; 3];
    let mut ioff: u32 = 0;
    let mut idata_limit: u32 = 0;
    let mut pal_len: u32 = 0;
    let mut first = true;
    let mut interlace = 0u8;
    let mut color = 0u8;
    let mut is_iphone = false;
    let mut idata: Vec<u8> = Vec::new();
    let mut idata_allocated = false;
    loop {
        let length = s.get32be();
        let ty = s.get32be();
        match ty {
            TYPE_CGBI => {
                is_iphone = true;
                s.skip(length as i32);
            }
            TYPE_IHDR => {
                if !first {
                    return err(b"multiple IHDR");
                }
                first = false;
                if length != 13 {
                    return err(b"bad IHDR len");
                }
                z.img_x = s.get32be();
                z.img_y = s.get32be();
                if z.img_y > MAX_DIMENSIONS {
                    return err(b"too large");
                }
                if z.img_x > MAX_DIMENSIONS {
                    return err(b"too large");
                }
                z.depth = s.get8() as i32;
                if !matches!(z.depth, 1 | 2 | 4 | 8 | 16) {
                    return err(b"1/2/4/8/16-bit only");
                }
                color = s.get8();
                if color > 6 {
                    return err(b"bad ctype");
                }
                if color == 3 && z.depth == 16 {
                    return err(b"bad ctype");
                }
                if color == 3 {
                    pal_img_n = 3;
                } else if color & 1 != 0 {
                    return err(b"bad ctype");
                }
                if s.get8() != 0 {
                    return err(b"bad comp method");
                }
                if s.get8() != 0 {
                    return err(b"bad filter method");
                }
                interlace = s.get8();
                if interlace > 1 {
                    return err(b"bad interlace method");
                }
                if z.img_x == 0 || z.img_y == 0 {
                    return err(b"0-pixel image");
                }
                if pal_img_n == 0 {
                    z.img_n = (if color & 2 != 0 { 3 } else { 1 }) + (if color & 4 != 0 { 1 } else { 0 });
                    if (1u32 << 30) / z.img_x / (z.img_n as u32) < z.img_y {
                        return err(b"too large");
                    }
                } else {
                    z.img_n = 1;
                    if (1u32 << 30) / z.img_x / 4 < z.img_y {
                        return err(b"too large");
                    }
                }
            }
            TYPE_PLTE => {
                if first {
                    return err(b"first not IHDR");
                }
                if length > 256 * 3 {
                    return err(b"invalid PLTE");
                }
                pal_len = length / 3;
                if pal_len * 3 != length {
                    return err(b"invalid PLTE");
                }
                for i in 0..pal_len as usize {
                    palette[i * 4] = s.get8();
                    palette[i * 4 + 1] = s.get8();
                    palette[i * 4 + 2] = s.get8();
                    palette[i * 4 + 3] = 255;
                }
            }
            TYPE_TRNS => {
                if first {
                    return err(b"first not IHDR");
                }
                if idata_allocated {
                    return err(b"tRNS after IDAT");
                }
                if pal_img_n != 0 {
                    if pal_len == 0 {
                        return err(b"tRNS before PLTE");
                    }
                    if length > pal_len {
                        return err(b"bad tRNS len");
                    }
                    pal_img_n = 4;
                    for i in 0..length as usize {
                        palette[i * 4 + 3] = s.get8();
                    }
                } else {
                    if z.img_n & 1 == 0 {
                        return err(b"tRNS with alpha");
                    }
                    if length != z.img_n as u32 * 2 {
                        return err(b"bad tRNS len");
                    }
                    has_trans = true;
                    if z.depth == 16 {
                        for k in 0..(z.img_n as usize).min(3) {
                            tc16[k] = s.get16be() as u16;
                        }
                    } else {
                        for k in 0..(z.img_n as usize).min(3) {
                            let v = (s.get16be() & 255) as u8;
                            tc[k] = v.wrapping_mul(DEPTH_SCALE_TABLE[z.depth as usize]);
                        }
                    }
                }
            }
            TYPE_IDAT => {
                if first {
                    return err(b"first not IHDR");
                }
                if pal_img_n != 0 && pal_len == 0 {
                    return err(b"no PLTE");
                }
                if length > (1u32 << 30) {
                    return err(b"IDAT size limit");
                }
                if (ioff.wrapping_add(length) as i32) < (ioff as i32) {
                    return Err(());
                }
                if ioff.wrapping_add(length) > idata_limit {
                    idata_allocated = true;
                    if idata_limit == 0 {
                        idata_limit = if length > 4096 { length } else { 4096 };
                    }
                    while ioff.wrapping_add(length) > idata_limit {
                        idata_limit = idata_limit.wrapping_mul(2);
                    }
                }
                let Some(bytes) = s.getn(length as usize) else {
                    return err(b"outofdata");
                };
                idata.extend_from_slice(bytes);
                ioff = ioff.wrapping_add(length);
            }
            TYPE_IEND => {
                if first {
                    return err(b"first not IHDR");
                }
                if !idata_allocated {
                    return err(b"no IDAT");
                }
                let expanded = zlib_decode(&idata, !is_iphone)?;
                if (req_comp == z.img_n + 1 && req_comp != 3 && pal_img_n == 0) || has_trans {
                    z.img_out_n = z.img_n + 1;
                } else {
                    z.img_out_n = z.img_n;
                }
                let out_n = z.img_out_n;
                create_png_image(z, &expanded, out_n, z.depth, color, interlace != 0)?;
                if has_trans {
                    if z.depth == 16 {
                        compute_transparency16(z, tc16, out_n);
                    } else {
                        compute_transparency(z, tc, out_n);
                    }
                }
                if pal_img_n != 0 {
                    z.img_n = pal_img_n as i32;
                    z.img_out_n = pal_img_n as i32;
                    if req_comp >= 3 {
                        z.img_out_n = req_comp;
                    }
                    expand_palette(z, &palette, z.img_out_n as usize)?;
                } else if has_trans {
                    z.img_n += 1;
                }
                let _ = s.get32be();
                return Ok(());
            }
            _ => {
                if first {
                    return err(b"first not IHDR");
                }
                if ty & (1 << 29) == 0 {
                    INVALID_CHUNK.with(|c| c.set([(ty >> 24) as u8, (ty >> 16) as u8, (ty >> 8) as u8, ty as u8]));
                    return err(UNKNOWN_CHUNK);
                }
                s.skip(length as i32);
            }
        }
        let _ = s.get32be();
    }
}

fn convert_format(data: &[u8], img_n: i32, req_comp: i32, pixels: usize) -> Vec<u8> {
    let (n, r) = (img_n as usize, req_comp as usize);
    let mut out = Vec::with_capacity(pixels * r);
    for i in 0..pixels {
        let p = &data[i * n..i * n + n];
        match (n, r) {
            (1, 3) => out.extend_from_slice(&[p[0], p[0], p[0]]),
            (1, 4) => out.extend_from_slice(&[p[0], p[0], p[0], 255]),
            (2, 3) => out.extend_from_slice(&[p[0], p[0], p[0]]),
            (2, 4) => out.extend_from_slice(&[p[0], p[0], p[0], p[1]]),
            (3, 4) => out.extend_from_slice(&[p[0], p[1], p[2], 255]),
            (4, 3) => out.extend_from_slice(&[p[0], p[1], p[2]]),
            // the game only asks for 3 or 4 channels
            _ => unreachable!("conversion {n} -> {r} channels"),
        }
    }
    out
}

// --------------------------------------------------------------------------------------------- the texture --

fn be_u32(p: &[u8]) -> u32 {
    u32::from_be_bytes([p[0], p[1], p[2], p[3]])
}

// The header facts stb_image does not report: the colour type, whether a tRNS chunk exists, the "ee:surface" text.
fn scan_png(d: &[u8]) -> Option<(u8, bool, Vec<u8>)> {
    const SIG: [u8; 8] = [0x89, b'P', b'N', b'G', 0x0d, 0x0a, 0x1a, 0x0a];
    if d.len() < 33 || d[..8] != SIG || &d[12..16] != b"IHDR" {
        return None;
    }
    let color = d[25];
    let mut trns = false;
    let mut surface = Vec::new();
    let mut p = 8usize;
    while p + 8 <= d.len() {
        let n = be_u32(&d[p..]) as usize;
        if n > d.len() - p - 8 {
            break;
        }
        match &d[p + 4..p + 8] {
            b"tRNS" => trns = true,
            b"tEXt" => {
                let body = &d[p + 8..p + 8 + n];
                if let Some(z) = body.iter().position(|&c| c == 0) {
                    if &body[..z] == b"ee:surface" {
                        surface = body[z + 1..].to_vec(); // Latin-1 bytes; only ASCII digits are accepted below
                    }
                }
            }
            b"IEND" => break,
            _ => {}
        }
        p += 12 + n;
    }
    Some((color, trns, surface))
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
        let Some((color, trns, surface)) = scan_png(d) else { return fail("not a PNG file") };
        let ch: i32 = if color == 4 || color == 6 || trns { 4 } else { 3 };
        let Some(img) = load_png(d, ch) else {
            let mut msg = b"PNG cannot be decoded: ".to_vec();
            msg.extend_from_slice(&reason());
            return Err(Failure(msg));
        };
        let (w, h) = (img.w, img.h);
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
        let mut pixels = img.out;
        pixels.truncate(w as usize * h as usize * ch as usize);
        out.frames.push(PngFrame { width: w, height: h, channels: ch as u32, pixels });
    }
    Ok(out)
}

/// One frame as a type-2 Targa (an 18-byte header, bottom-up BGR(A) rows; sst.py `_tga_bytes`).
pub fn frame_to_tga(frame: &PngFrame) -> Vec<u8> {
    let (w, h, ch) = (frame.width as u16, frame.height as u16, frame.channels as usize);
    let mut out = vec![0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    out.extend_from_slice(&[w as u8, (w >> 8) as u8, h as u8, (h >> 8) as u8, (8 * ch) as u8, if ch == 4 { 8 } else { 0 }]);
    for y in (0..h as usize).rev() {
        let row = &frame.pixels[y * w as usize * ch..(y + 1) * w as usize * ch];
        for p in row.chunks_exact(ch) {
            out.extend_from_slice(&[p[2], p[1], p[0]]);
            if ch == 4 {
                out.push(p[3]);
            }
        }
    }
    out
}

/// One PNG -> the Targa a picture read as a Targa outside the texture manager gets in its place (`GEFindTargaFile`;
/// the frame `png_to_texture` decodes; an "ee:surface" text is checked but has no effect).
pub fn png_to_tga(data: &[u8]) -> Result<Vec<u8>, Failure> {
    let t = png_to_texture(&[data])?;
    Ok(frame_to_tga(&t.frames[0]))
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

    fn tga_header(w: u8, h: u8, ch: u8) -> Vec<u8> {
        vec![0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, w, 0, h, 0, 8 * ch, if ch == 4 { 8 } else { 0 }]
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
    fn a_targa_for_a_picture_read_as_data() {
        // type 2, 18-byte header, bottom-up BGR rows (what GESurface::LoadTGA reads)
        let tga = png_to_tga(&png(1, 2, &[0, 1, 2, 3, 0, 4, 5, 6], plain(8, 2))).unwrap();
        let mut want = tga_header(1, 2, 3);
        want.extend_from_slice(&[6, 5, 4, 3, 2, 1]);
        assert_eq!(tga, want);
        let tga = png_to_tga(&png(1, 1, &[0, 1, 2, 3, 200], plain(8, 6))).unwrap();
        let mut want = tga_header(1, 1, 4);
        want.extend_from_slice(&[3, 2, 1, 200]);
        assert_eq!(tga, want);
        assert_eq!(png_to_tga(b"hello").unwrap_err().0, b"not a PNG file".to_vec());
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
        assert_eq!(frame0(&png(1, 1, &[0, 1, 2, 3, 4], PngOpts { interlace: 1, ..plain(8, 6) })), (4, vec![1, 2, 3, 4]));
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
    fn truncated_idat_is_refused_with_stbs_reason() {
        let p = png(1, 1, &[0, 1, 2, 3], plain(8, 2));
        // cut inside the IDAT chunk
        let idat = p.windows(4).position(|w| w == b"IDAT").unwrap();
        let cut = &p[..idat + 8];
        assert_eq!(refused(&[cut]), b"PNG cannot be decoded: outofdata".to_vec());
    }

    #[test]
    fn missing_pixels_are_refused() {
        let p = png(1, 2, &[0, 1, 2, 3], plain(8, 2));
        assert_eq!(refused(&[&p]), b"PNG cannot be decoded: not enough pixels".to_vec());
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
        assert_eq!(refused(&[&bad]), b"PNG cannot be decoded: invalid filter".to_vec());
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
    fn zlib_handles_fixed_and_dynamic_huffman_blocks() {
        // zlib.compress(bytes(100), 9): a fixed-Huffman stream; and a skewed text whose stream is a dynamic-Huffman block
        // (generated with the Python zlib; a fixed table cannot code this symbol distribution as tightly)
        let fixed = [0x78, 0xda, 0x63, 0x60, 0xa0, 0x3d, 0x00, 0x00, 0x00, 0x64, 0x00, 0x01];
        assert_eq!(zlib_decode(&fixed, true), Ok(vec![0u8; 100]));
        let dynamic = [
            0x78, 0xda, 0x4d, 0x8f, 0x8b, 0x0d, 0x00, 0x21, 0x08, 0x43, 0x67, 0xed, 0x67, 0xff, 0x19, 0x0e, 0x0a, 0xc4,
            0xd3, 0x28, 0xb1, 0x55, 0xfa, 0x04, 0x40, 0x40, 0xec, 0xe2, 0xda, 0x67, 0x9c, 0x44, 0xd6, 0x8a, 0xa0, 0x91,
            0x6b, 0x9e, 0xd1, 0x7b, 0x94, 0xae, 0x65, 0xd3, 0x7d, 0x45, 0xb9, 0xda, 0xb2, 0x63, 0x4d, 0x2f, 0xc0, 0x79,
            0x2e, 0x52, 0xd6, 0x35, 0x8a, 0x91, 0x80, 0xe4, 0xcc, 0xc3, 0x85, 0xe0, 0x34, 0x4b, 0xb5, 0x56, 0x72, 0x29,
            0x9a, 0x48, 0x3f, 0xd2, 0xa0, 0x2e, 0x5e, 0x5b, 0x15, 0x12, 0x3e, 0x4b, 0xe2, 0xf3, 0x7e, 0x7f, 0x5b, 0xec,
            0xd0, 0x1e, 0xe3, 0xc0, 0x5c, 0x1f, 0x5c, 0x4e, 0xb3, 0x39, 0x07, 0xe1, 0x03, 0x40, 0x3c, 0x72, 0x5b,
        ];
        assert_eq!(zlib_decode(&dynamic, true), Ok(b"aaabaacbaabadbaaaaaaaabaacbaabbbabbaaaabcaaaabababcbaabbbaaabbababaabbacaabdaaaacabcaababadabaabaabaacaadaaabacbbcdcaabababcaacaaabbaabbabbbcaabadbaaaaabaaaacaabaaadcaaaaabadacacbabaabdaaaaaaabaaacbaaaaaababbaabcbbbbbaadcccbcbaaaaaabaaaaaaaaabaaababaabaacabaabaabaaaacaaaaaaaababaacbabaabaabbdacbabca".to_vec()));
    }

    #[test]
    fn zlib_detects_corruption() {
        assert_eq!(zlib_decode(&[0x78, 0x01], true), Err(()));
        assert_eq!(reason(), b"bad zlib header".to_vec());
        assert_eq!(zlib_decode(&[0x79, 0x01, 0, 0], true), Err(()));
        // a stored block whose nlen is wrong
        assert_eq!(zlib_decode(&[0x78, 0x01, 1, 2, 0, 0, 0, 0, 0], true), Err(()));
        assert_eq!(reason(), b"zlib corrupt".to_vec());
        // block type 3 sets no reason of its own
        assert_eq!(zlib_decode(&[0x78, 0x01, 7], true), Err(()));
    }

    #[test]
    fn paeth_and_size_checks() {
        assert_eq!(paeth(1, 2, 3), 1);
        assert_eq!(paeth(10, 20, 10), 20);
        assert!(mad3_valid(10, 10, 10, 0));
        assert!(!mad3_valid(1 << 20, 1 << 20, 10, 0));
        assert!(parse_size(b"123").is_some());
        assert_eq!(parse_size(b"99999999999"), None);
        assert_eq!(parse_size(b""), None);
    }

    #[test]
    fn scan_png_finds_trns_and_surface() {
        let p = png(1, 1, &[0, 1], PngOpts { trns: Some(&[0, 0]), text: Some((b"ee:surface", b"2x2")), ..plain(8, 0) });
        let (color, trns, surface) = scan_png(&p).unwrap();
        assert_eq!((color, trns, surface.as_slice()), (0, true, &b"2x2"[..]));
        assert!(scan_png(&p[..20]).is_none());
    }
}
