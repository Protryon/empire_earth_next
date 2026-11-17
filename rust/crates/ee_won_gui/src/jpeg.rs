//! WONGUI's JPEG decoder in Rust (R6 WON round 41): JPGDecoder.cpp's decode on the engine's libjpeg 6b
//! (src/ThirdParty/libjpeg), as that library decodes with JPGDecoder's settings: the default decompression
//! parameters (the slow integer IDCT, fancy upsampling, no scaling, no quantization, no block smoothing needed by
//! complete files), JPGDecoder's source manager (4 KB reads from a FileReader, a fake end of image after the data,
//! a fatal error on an empty file, skips past the buffer through `SkipBytes`, which throws) and its error manager (a
//! fatal error longjmps back: `false` from the attributes, NULL from the decode).
//!
//! What is libjpeg's here, statement for statement where it decides the output: the marker reader (jdmarker.c:
//! SOI, SOF0/1/2/9/10, DHT, DQT, DRI, SOS, the JFIF APP0 and Adobe APP14 markers, the skipped APPn, COM, DNL and DAC
//! segments, the restart markers and their resync), the colour space choice (jdapimin.c), the frame checks and
//! block geometry (jdinput.c), the Huffman tables (jdhuff.c `jpeg_make_d_derived_tbl`) and the sequential and
//! progressive entropy decoders with their bit buffer, zero fill past a marker and insufficient-data rule (jdhuff.c,
//! jdphuff.c), the quantization tables latched at a component's first scan, the IDCT (jidctint.c with its zero
//! column and row shortcuts and 32-bit `INT32` arithmetic), the range limit table (jdmaster.c), the upsamplers
//! (jdsample.c: full size, h2v1 and h2v2 fancy, plain and integral) with jdmainct.c's context rows (the first row
//! above the top, the last real row below the bottom) and the colour converters (jdcolor.c: YCbCr, YCCK, null).
//!
//! A progressive file whose first coefficients are incomplete when it ends gets libjpeg's block smoothing
//! (jdcoefct.c `decompress_smooth_data`).
//!
//! Where it differs: a two-component image fails (JPGDecoder read three samples a pixel past the end of libjpeg's
//! row of two there). No file the lobby reads has two.

use ee_won_net::file::EeWonFileReader;

/// Why a decode stopped: libjpeg's fatal error (the longjmp), or a FileReader status the C++ throws.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Stop {
    Fail,
    Throw(i32),
}

type R<T> = Result<T, Stop>;

/// What JPGDecoder reads through: `ReadMaxBytes` (the count, and a status Check throws for) and `SkipBytes`.
pub trait Input {
    fn read_max(&mut self, out: &mut [u8]) -> (usize, i32);
    fn skip(&mut self, n: u32) -> i32;
}

impl Input for EeWonFileReader {
    fn read_max(&mut self, out: &mut [u8]) -> (usize, i32) {
        // SAFETY: `out` holds out.len() bytes
        let (n, s) = unsafe { EeWonFileReader::read_max(self, out.as_mut_ptr(), out.len() as i32) };
        (n.max(0) as usize, s)
    }
    fn skip(&mut self, n: u32) -> i32 {
        EeWonFileReader::skip(self, n as _)
    }
}

/// A byte slice as a FileReader over memory reads it (a skip past the end is the reader's past-end status, 1).
pub struct SliceInput<'a> {
    pub data: &'a [u8],
    pub pos: usize,
}

impl Input for SliceInput<'_> {
    fn read_max(&mut self, out: &mut [u8]) -> (usize, i32) {
        let n = out.len().min(self.data.len() - self.pos);
        out[..n].copy_from_slice(&self.data[self.pos..self.pos + n]);
        self.pos += n;
        (n, 0)
    }
    fn skip(&mut self, n: u32) -> i32 {
        if self.pos + n as usize > self.data.len() {
            return 1;
        }
        self.pos += n as usize;
        0
    }
}

/// jutils.c `jpeg_natural_order`, with its 16 extra entries for a corrupt run past the end.
pub const NATURAL_ORDER: [usize; 80] = [
    0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21,
    28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54,
    47, 55, 62, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
];

const M_SOF0: i32 = 0xc0;
const M_DHT: i32 = 0xc4;
const M_DAC: i32 = 0xcc;
const M_RST0: i32 = 0xd0;
const M_RST7: i32 = 0xd7;
const M_SOI: i32 = 0xd8;
const M_EOI: i32 = 0xd9;
const M_SOS: i32 = 0xda;
const M_DQT: i32 = 0xdb;
const M_DNL: i32 = 0xdc;
const M_DRI: i32 = 0xdd;
const M_APP0: i32 = 0xe0;
const M_APP14: i32 = 0xee;
const M_APP15: i32 = 0xef;
const M_COM: i32 = 0xfe;
const M_TEM: i32 = 0x01;

const MAX_COMPONENTS: usize = 10;
const MAX_COMPS_IN_SCAN: usize = 4;
const MAX_BLOCKS_IN_MCU: usize = 10;
const MAX_DIMENSION: u32 = 65500;
const MIN_GET_BITS: i32 = 25;
const LOOKAHEAD: i32 = 8;

/// jdhuff.h `d_derived_tbl`, with the table's values.
#[derive(Clone)]
struct Derived {
    maxcode: [i32; 18],
    valoffset: [i32; 17],
    look_nbits: [u8; 256],
    look_sym: [u8; 256],
    huffval: [u8; 256],
}

#[derive(Clone)]
struct HuffTbl {
    bits: [u8; 17],
    huffval: [u8; 256],
}

/// `jpeg_make_d_derived_tbl`: JERR_BAD_HUFF_TABLE as a failure.
fn derive(t: &HuffTbl, is_dc: bool) -> R<Derived> {
    let mut huffsize = [0u8; 257];
    let mut huffcode = [0u32; 257];
    let mut p = 0usize;
    for l in 1..=16 {
        let i = t.bits[l] as usize;
        if p + i > 256 {
            return Err(Stop::Fail);
        }
        for _ in 0..i {
            huffsize[p] = l as u8;
            p += 1;
        }
    }
    huffsize[p] = 0;
    let numsymbols = p;
    let mut code = 0u32;
    let mut si = huffsize[0] as u32;
    p = 0;
    while huffsize[p] != 0 {
        while huffsize[p] as u32 == si {
            huffcode[p] = code;
            p += 1;
            code = code.wrapping_add(1);
        }
        if (code as i32) >= (1i32 << si) {
            return Err(Stop::Fail);
        }
        code <<= 1;
        si += 1;
    }
    let mut d =
        Derived { maxcode: [0; 18], valoffset: [0; 17], look_nbits: [0; 256], look_sym: [0; 256], huffval: t.huffval };
    p = 0;
    for l in 1..=16 {
        if t.bits[l] != 0 {
            d.valoffset[l] = (p as i32).wrapping_sub(huffcode[p] as i32);
            p += t.bits[l] as usize;
            d.maxcode[l] = huffcode[p - 1] as i32;
        } else {
            d.maxcode[l] = -1;
        }
    }
    d.maxcode[17] = 0xFFFFF;
    p = 0;
    for l in 1..=LOOKAHEAD as usize {
        for _ in 1..=t.bits[l] as usize {
            let first = (huffcode[p] << (LOOKAHEAD as usize - l)) as usize;
            for lookbits in first..first + (1usize << (LOOKAHEAD as usize - l)) {
                d.look_nbits[lookbits] = l as u8;
                d.look_sym[lookbits] = t.huffval[p];
            }
            p += 1;
        }
    }
    if is_dc && t.huffval[..numsymbols].iter().any(|&s| s > 15) {
        return Err(Stop::Fail);
    }
    Ok(d)
}

#[derive(Clone)]
struct Comp {
    id: i32,
    h: usize,
    v: usize,
    tq: usize,
    dc_tbl: usize,
    ac_tbl: usize,
    wib: usize,
    hib: usize,
    ds_w: usize,
    ds_h: usize,
    /// The storage stride and rows in blocks (the block counts rounded up to the sampling factors).
    bw: usize,
    bh: usize,
    quant: Option<[u16; 64]>,
    coefs: Vec<[i16; 64]>,
    coef_bits: [i32; 64],
}

impl Default for Comp {
    fn default() -> Self {
        Comp {
            id: 0,
            h: 0,
            v: 0,
            tq: 0,
            dc_tbl: 0,
            ac_tbl: 0,
            wib: 0,
            hib: 0,
            ds_w: 0,
            ds_h: 0,
            bw: 0,
            bh: 0,
            quant: None,
            coefs: Vec::new(),
            coef_bits: [-1; 64],
        }
    }
}

/// One decoded picture: libjpeg's output rows, `comps` samples per pixel.
pub struct Picture {
    pub width: u32,
    pub height: u32,
    pub comps: usize,
    pub gray: bool,
    pub rows: Vec<u8>,
}

/// Tries to allocate `n` values (a failed allocation is libjpeg's JERR_OUT_OF_MEMORY).
fn alloc<T: Clone>(n: usize, v: T) -> R<Vec<T>> {
    let mut out = Vec::new();
    out.try_reserve_exact(n).map_err(|_| Stop::Fail)?;
    out.resize(n, v);
    Ok(out)
}

struct Dec<'a, I: Input> {
    input: &'a mut I,
    buf: Box<[u8; 4096]>,
    pos: usize,
    len: usize,
    start_of_file: bool,
    unread_marker: i32,
    saw_soi: bool,
    saw_sof: bool,
    progressive: bool,
    arith: bool,
    precision: i32,
    width: u32,
    height: u32,
    comps: Vec<Comp>,
    restart_interval: u32,
    saw_jfif: bool,
    saw_adobe: bool,
    adobe_transform: u8,
    dc_tbls: [Option<HuffTbl>; 4],
    ac_tbls: [Option<HuffTbl>; 4],
    quant_tbls: [Option<[u16; 64]>; 4],
    next_restart_num: i32,
    // the scan
    scan_comps: Vec<usize>,
    ss: i32,
    se: i32,
    ah: i32,
    al: i32,
    // the entropy decoder
    get_buffer: u64,
    bits_left: i32,
    insufficient: bool,
    restarts_to_go: u32,
    last_dc: [i32; MAX_COMPS_IN_SCAN],
    eobrun: u32,
    max_h: usize,
    max_v: usize,
    multi_scan: bool,
}

impl<'a, I: Input> Dec<'a, I> {
    fn new(input: &'a mut I) -> Self {
        Dec {
            input,
            buf: Box::new([0; 4096]),
            pos: 0,
            len: 0,
            start_of_file: true,
            unread_marker: 0,
            saw_soi: false,
            saw_sof: false,
            progressive: false,
            arith: false,
            precision: 0,
            width: 0,
            height: 0,
            comps: Vec::new(),
            restart_interval: 0,
            saw_jfif: false,
            saw_adobe: false,
            adobe_transform: 0,
            dc_tbls: Default::default(),
            ac_tbls: Default::default(),
            quant_tbls: [None; 4],
            next_restart_num: 0,
            scan_comps: Vec::new(),
            ss: 0,
            se: 0,
            ah: 0,
            al: 0,
            get_buffer: 0,
            bits_left: 0,
            insufficient: false,
            restarts_to_go: 0,
            last_dc: [0; MAX_COMPS_IN_SCAN],
            eobrun: 0,
            max_h: 1,
            max_v: 1,
            multi_scan: false,
        }
    }

    // ---- the source manager (JPGDecoder.cpp's fill_input_buffer, skip_input_data)

    fn fill(&mut self) -> R<()> {
        let (mut n, s) = self.input.read_max(&mut self.buf[..]);
        if s != 0 {
            return Err(Stop::Throw(s));
        }
        if n == 0 {
            if self.start_of_file {
                return Err(Stop::Fail);
            }
            self.buf[0] = 0xff;
            self.buf[1] = M_EOI as u8;
            n = 2;
        }
        self.pos = 0;
        self.len = n;
        self.start_of_file = false;
        Ok(())
    }

    fn byte(&mut self) -> R<i32> {
        if self.pos == self.len {
            self.fill()?;
        }
        let b = self.buf[self.pos];
        self.pos += 1;
        Ok(b as i32)
    }

    fn two(&mut self) -> R<i32> {
        let hi = self.byte()?;
        Ok((hi << 8) + self.byte()?)
    }

    fn skip_input(&mut self, bytes: i32) -> R<()> {
        let have = self.len - self.pos;
        if bytes as usize <= have {
            self.pos += bytes as usize;
        } else {
            let rest = bytes as u32 - have as u32;
            self.pos = self.len;
            let s = self.input.skip(rest);
            if s != 0 {
                return Err(Stop::Throw(s));
            }
        }
        Ok(())
    }

    // ---- the marker reader (jdmarker.c)

    fn next_marker(&mut self) -> R<()> {
        loop {
            let mut c = self.byte()?;
            while c != 0xff {
                c = self.byte()?;
            }
            loop {
                c = self.byte()?;
                if c != 0xff {
                    break;
                }
            }
            if c != 0 {
                self.unread_marker = c;
                return Ok(());
            }
        }
    }

    fn skip_variable(&mut self) -> R<()> {
        let length = self.two()? - 2;
        if length > 0 {
            self.skip_input(length)?;
        }
        Ok(())
    }

    fn get_sof(&mut self, prog: bool, arith: bool) -> R<()> {
        self.progressive = prog;
        self.arith = arith;
        let length = self.two()?;
        self.precision = self.byte()?;
        self.height = self.two()? as u32;
        self.width = self.two()? as u32;
        let n = self.byte()?;
        if self.saw_sof {
            return Err(Stop::Fail);
        }
        if self.height == 0 || self.width == 0 || n <= 0 {
            return Err(Stop::Fail);
        }
        if length - 8 != n * 3 {
            return Err(Stop::Fail);
        }
        self.comps.clear();
        for _ in 0..n {
            let id = self.byte()?;
            let c = self.byte()?;
            let tq = self.byte()?;
            self.comps.push(Comp {
                id,
                h: ((c >> 4) & 15) as usize,
                v: (c & 15) as usize,
                tq: tq as usize,
                ..Comp::default()
            });
        }
        self.saw_sof = true;
        Ok(())
    }

    fn get_sos(&mut self) -> R<()> {
        if !self.saw_sof {
            return Err(Stop::Fail);
        }
        let length = self.two()?;
        let n = self.byte()?;
        if length != n * 2 + 6 || n < 1 || n as usize > MAX_COMPS_IN_SCAN {
            return Err(Stop::Fail);
        }
        self.scan_comps.clear();
        for _ in 0..n {
            let cc = self.byte()?;
            let c = self.byte()?;
            let ci = self.comps.iter().position(|k| k.id == cc).ok_or(Stop::Fail)?;
            self.scan_comps.push(ci);
            self.comps[ci].dc_tbl = ((c >> 4) & 15) as usize;
            self.comps[ci].ac_tbl = (c & 15) as usize;
        }
        self.ss = self.byte()?;
        self.se = self.byte()?;
        let c = self.byte()?;
        self.ah = (c >> 4) & 15;
        self.al = c & 15;
        self.next_restart_num = 0;
        Ok(())
    }

    fn get_dht(&mut self) -> R<()> {
        let mut length = self.two()? - 2;
        while length > 16 {
            let index = self.byte()?;
            let mut bits = [0u8; 17];
            let mut count = 0i32;
            for b in bits.iter_mut().skip(1) {
                *b = self.byte()? as u8;
                count += *b as i32;
            }
            length -= 1 + 16;
            if count > 256 || count > length {
                return Err(Stop::Fail);
            }
            let mut huffval = [0u8; 256];
            for v in huffval.iter_mut().take(count as usize) {
                *v = self.byte()? as u8;
            }
            length -= count;
            let (ac, index) = if index & 0x10 != 0 { (true, index - 0x10) } else { (false, index) };
            if !(0..4).contains(&index) {
                return Err(Stop::Fail);
            }
            let t = Some(HuffTbl { bits, huffval });
            if ac {
                self.ac_tbls[index as usize] = t;
            } else {
                self.dc_tbls[index as usize] = t;
            }
        }
        if length != 0 {
            return Err(Stop::Fail);
        }
        Ok(())
    }

    fn get_dqt(&mut self) -> R<()> {
        let mut length = self.two()? - 2;
        while length > 0 {
            let n = self.byte()?;
            let prec = n >> 4;
            let n = (n & 15) as usize;
            if n >= 4 {
                return Err(Stop::Fail);
            }
            let mut q = self.quant_tbls[n].unwrap_or([0; 64]);
            for &z in NATURAL_ORDER.iter().take(64) {
                q[z] = if prec != 0 { self.two()? } else { self.byte()? } as u16;
            }
            self.quant_tbls[n] = Some(q);
            length -= 65;
            if prec != 0 {
                length -= 64;
            }
        }
        if length != 0 {
            return Err(Stop::Fail);
        }
        Ok(())
    }

    fn get_dri(&mut self) -> R<()> {
        if self.two()? != 4 {
            return Err(Stop::Fail);
        }
        self.restart_interval = self.two()? as u32;
        Ok(())
    }

    /// `get_interesting_appn` for APP0 and APP14.
    fn get_appn(&mut self, marker: i32) -> R<()> {
        let mut length = self.two()? - 2;
        let n = length.clamp(0, 14) as usize;
        let mut b = [0u8; 14];
        for v in b.iter_mut().take(n) {
            *v = self.byte()? as u8;
        }
        length -= n as i32;
        if marker == M_APP0 {
            if n >= 14 && b[..5] == *b"JFIF\0" {
                self.saw_jfif = true;
            }
        } else if n >= 12 && b[..5] == *b"Adobe" {
            self.saw_adobe = true;
            self.adobe_transform = b[11];
        }
        if length > 0 {
            self.skip_input(length)?;
        }
        Ok(())
    }

    /// `read_markers`: true at an SOS, false at an EOI.
    fn read_markers(&mut self) -> R<bool> {
        loop {
            if self.unread_marker == 0 {
                if !self.saw_soi {
                    let c = self.byte()?;
                    let c2 = self.byte()?;
                    if c != 0xff || c2 != M_SOI {
                        return Err(Stop::Fail);
                    }
                    self.unread_marker = c2;
                } else {
                    self.next_marker()?;
                }
            }
            let m = self.unread_marker;
            match m {
                M_SOI => {
                    if self.saw_soi {
                        return Err(Stop::Fail);
                    }
                    self.restart_interval = 0;
                    self.saw_jfif = false;
                    self.saw_adobe = false;
                    self.adobe_transform = 0;
                    self.saw_soi = true;
                }
                0xc0 | 0xc1 => self.get_sof(false, false)?,
                0xc2 => self.get_sof(true, false)?,
                0xc9 => self.get_sof(false, true)?,
                0xca => self.get_sof(true, true)?,
                0xc3 | 0xc5..=0xc7 | 0xc8 | 0xcb | 0xcd..=0xcf => return Err(Stop::Fail),
                M_SOS => {
                    self.get_sos()?;
                    self.unread_marker = 0;
                    return Ok(true);
                }
                M_EOI => {
                    self.unread_marker = 0;
                    return Ok(false);
                }
                M_DAC | M_DNL | M_COM => self.skip_variable()?,
                M_DHT => self.get_dht()?,
                M_DQT => self.get_dqt()?,
                M_DRI => self.get_dri()?,
                M_APP0 | M_APP14 => self.get_appn(m)?,
                0xe1..=M_APP15 => self.skip_variable()?,
                M_RST0..=M_RST7 | M_TEM => {}
                _ => return Err(Stop::Fail),
            }
            self.unread_marker = 0;
        }
    }

    // ---- the frame (jdinput.c initial_setup, jdapimin.c default_decompress_parms)

    fn initial_setup(&mut self) -> R<()> {
        if self.height > MAX_DIMENSION || self.width > MAX_DIMENSION {
            return Err(Stop::Fail);
        }
        if self.precision != 8 || self.comps.len() > MAX_COMPONENTS {
            return Err(Stop::Fail);
        }
        if self.comps.iter().any(|c| !(1..=4).contains(&c.h) || !(1..=4).contains(&c.v)) {
            return Err(Stop::Fail);
        }
        self.max_h = self.comps.iter().map(|c| c.h).max().unwrap_or(1);
        self.max_v = self.comps.iter().map(|c| c.v).max().unwrap_or(1);
        let (w, h, mh, mv) = (self.width as usize, self.height as usize, self.max_h, self.max_v);
        for c in &mut self.comps {
            c.wib = (w * c.h).div_ceil(mh * 8);
            c.hib = (h * c.v).div_ceil(mv * 8);
            c.ds_w = (w * c.h).div_ceil(mh);
            c.ds_h = (h * c.v).div_ceil(mv);
            c.bw = c.wib.div_ceil(c.h) * c.h;
            c.bh = c.hib.div_ceil(c.v) * c.v;
        }
        self.multi_scan = self.scan_comps.len() < self.comps.len() || self.progressive;
        Ok(())
    }

    /// The colour spaces: (jpeg_color_space, out_color_space) as 0 unknown, 1 gray, 2 RGB, 3 YCbCr, 4 CMYK, 5 YCCK.
    fn color_spaces(&self) -> (u8, u8) {
        match self.comps.len() {
            1 => (1, 1),
            3 => {
                let space = if self.saw_jfif {
                    3
                } else if self.saw_adobe {
                    if self.adobe_transform == 0 {
                        2
                    } else {
                        3
                    }
                } else {
                    let ids: Vec<i32> = self.comps.iter().map(|c| c.id).collect();
                    if ids == [82, 71, 66] {
                        2
                    } else {
                        3
                    }
                };
                (space, 2)
            }
            4 => {
                let space = if self.saw_adobe && self.adobe_transform != 0 { 5 } else { 4 };
                (space, 4)
            }
            _ => (0, 0),
        }
    }

    /// The upsampler each component gets (jdsample.c jinit_upsampler): 0 full size, 1 h2v1 fancy, 2 h2v1, 3 h2v2
    /// fancy, 4 h2v2, 5 integral; JERR_FRACT_SAMPLE_NOTIMPL as a failure.
    fn upsamplers(&self) -> R<Vec<u8>> {
        let (mh, mv) = (self.max_h, self.max_v);
        self.comps
            .iter()
            .map(|c| {
                Ok(if c.h == mh && c.v == mv {
                    0
                } else if c.h * 2 == mh && c.v == mv {
                    if c.ds_w > 2 {
                        1
                    } else {
                        2
                    }
                } else if c.h * 2 == mh && c.v * 2 == mv {
                    if c.ds_w > 2 {
                        3
                    } else {
                        4
                    }
                } else if mh % c.h == 0 && mv % c.v == 0 {
                    5
                } else {
                    return Err(Stop::Fail);
                })
            })
            .collect()
    }

    // ---- a scan's start (jdinput.c start_input_pass, the entropy decoders' start_pass)

    fn start_scan(&mut self) -> R<Vec<Option<Derived>>> {
        if self.scan_comps.len() > 1 {
            let blocks: usize = self.scan_comps.iter().map(|&ci| self.comps[ci].h * self.comps[ci].v).sum();
            if blocks > MAX_BLOCKS_IN_MCU {
                return Err(Stop::Fail);
            }
        }
        for &ci in &self.scan_comps {
            if self.comps[ci].quant.is_none() {
                let t = self.comps[ci].tq;
                self.comps[ci].quant = Some(if t < 4 { self.quant_tbls[t] } else { None }.ok_or(Stop::Fail)?);
            }
        }
        // the derived tables by number: DC 0..3, AC 4..7
        let mut derived: Vec<Option<Derived>> = vec![None; 8];
        if self.progressive {
            let dc_band = self.ss == 0;
            let mut bad =
                if dc_band { self.se != 0 } else { self.ss > self.se || self.se >= 64 || self.scan_comps.len() != 1 };
            if self.ah != 0 && self.al != self.ah - 1 {
                bad = true;
            }
            if self.al > 13 {
                bad = true;
            }
            if bad {
                return Err(Stop::Fail);
            }
            for &ci in &self.scan_comps {
                for k in self.ss..=self.se {
                    self.comps[ci].coef_bits[k as usize] = self.al;
                }
            }
            for &ci in &self.scan_comps {
                if dc_band {
                    if self.ah == 0 {
                        let t = self.comps[ci].dc_tbl;
                        derived[t.min(4)] = Some(self.table(true, t)?);
                    }
                } else {
                    let t = self.comps[ci].ac_tbl;
                    derived[4 + t.min(4)] = Some(self.table(false, t)?);
                }
            }
        } else {
            for &ci in &self.scan_comps {
                let (d, a) = (self.comps[ci].dc_tbl, self.comps[ci].ac_tbl);
                derived[d.min(4)] = Some(self.table(true, d)?);
                derived[4 + a.min(4)] = Some(self.table(false, a)?);
            }
        }
        self.last_dc = [0; MAX_COMPS_IN_SCAN];
        self.bits_left = 0;
        self.get_buffer = 0;
        self.insufficient = false;
        self.eobrun = 0;
        self.restarts_to_go = self.restart_interval;
        Ok(derived)
    }

    fn table(&self, dc: bool, n: usize) -> R<Derived> {
        let t = if n < 4 {
            if dc {
                &self.dc_tbls[n]
            } else {
                &self.ac_tbls[n]
            }
        } else {
            &None
        };
        derive(t.as_ref().ok_or(Stop::Fail)?, dc)
    }

    // ---- the bit buffer (jdhuff.c jpeg_fill_bit_buffer and the macros)

    fn fill_bits(&mut self, nbits: i32) -> R<()> {
        if self.unread_marker == 0 {
            while self.bits_left < MIN_GET_BITS {
                let mut c = self.byte()?;
                if c == 0xff {
                    loop {
                        c = self.byte()?;
                        if c != 0xff {
                            break;
                        }
                    }
                    if c == 0 {
                        c = 0xff;
                    } else {
                        self.unread_marker = c;
                        self.no_more_bytes(nbits);
                        return Ok(());
                    }
                }
                self.get_buffer = (self.get_buffer << 8) | c as u64;
                self.bits_left += 8;
            }
        } else {
            self.no_more_bytes(nbits);
        }
        Ok(())
    }

    fn no_more_bytes(&mut self, nbits: i32) {
        if nbits > self.bits_left {
            self.insufficient = true;
            self.get_buffer <<= (MIN_GET_BITS - self.bits_left) as u32;
            self.bits_left = MIN_GET_BITS;
        }
    }

    fn check(&mut self, n: i32) -> R<()> {
        if self.bits_left < n {
            self.fill_bits(n)?;
        }
        Ok(())
    }

    fn get_bits(&mut self, n: i32) -> i32 {
        self.bits_left -= n;
        ((self.get_buffer >> self.bits_left as u32) as i32) & ((1i32 << n) - 1)
    }

    fn huff_decode(&mut self, t: &Derived) -> R<i32> {
        if self.bits_left < LOOKAHEAD {
            self.fill_bits(0)?;
            if self.bits_left < LOOKAHEAD {
                return self.huff_slow(t, 1);
            }
        }
        let look = ((self.get_buffer >> (self.bits_left - LOOKAHEAD) as u32) & 0xff) as usize;
        let n = t.look_nbits[look] as i32;
        if n != 0 {
            self.bits_left -= n;
            return Ok(t.look_sym[look] as i32);
        }
        self.huff_slow(t, LOOKAHEAD + 1)
    }

    /// `jpeg_huff_decode`.
    fn huff_slow(&mut self, t: &Derived, min_bits: i32) -> R<i32> {
        let mut l = min_bits as usize;
        self.check(l as i32)?;
        let mut code = self.get_bits(l as i32);
        while code > t.maxcode[l] {
            code <<= 1;
            self.check(1)?;
            code |= self.get_bits(1);
            l += 1;
        }
        if l > 16 {
            return Ok(0);
        }
        Ok(t.huffval[(code.wrapping_add(t.valoffset[l]) as usize) & 0xff] as i32)
    }

    fn extend(r: i32, s: i32) -> i32 {
        if r < (1 << (s - 1)) {
            r + ((-1i32) << s) + 1
        } else {
            r
        }
    }

    /// `process_restart` with `read_restart_marker` and `jpeg_resync_to_restart`.
    fn restart(&mut self) -> R<()> {
        self.bits_left = 0;
        if self.unread_marker == 0 {
            self.next_marker()?;
        }
        if self.unread_marker == M_RST0 + self.next_restart_num {
            self.unread_marker = 0;
        } else {
            let desired = self.next_restart_num;
            let mut marker = self.unread_marker;
            loop {
                // 1: the desired restart or too far away; 2: an invalid marker or a prior restart (advance); 3: a
                // valid non-restart marker or one of the next two restarts (leave it)
                let next = marker == M_RST0 + ((desired + 1) & 7) || marker == M_RST0 + ((desired + 2) & 7);
                let action = if marker < M_SOF0 {
                    2
                } else if !(M_RST0..=M_RST7).contains(&marker) || next {
                    3
                } else if marker == M_RST0 + ((desired - 1) & 7) || marker == M_RST0 + ((desired - 2) & 7) {
                    2
                } else {
                    1
                };
                match action {
                    1 => {
                        self.unread_marker = 0;
                        break;
                    }
                    2 => {
                        self.next_marker()?;
                        marker = self.unread_marker;
                    }
                    _ => break,
                }
            }
        }
        self.next_restart_num = (self.next_restart_num + 1) & 7;
        self.last_dc = [0; MAX_COMPS_IN_SCAN];
        self.eobrun = 0;
        self.restarts_to_go = self.restart_interval;
        if self.unread_marker == 0 {
            self.insufficient = false;
        }
        Ok(())
    }

    // ---- a scan's data

    /// The scan's MCUs, each one decoded into its blocks (jdcoefct.c consume_data / decompress_onepass).
    fn decode_scan(&mut self, derived: &[Option<Derived>]) -> R<()> {
        let single = self.scan_comps.len() == 1;
        let (mcus_x, mcus_y) = if single {
            let c = &self.comps[self.scan_comps[0]];
            (c.wib, c.hib)
        } else {
            ((self.width as usize).div_ceil(self.max_h * 8), (self.height as usize).div_ceil(self.max_v * 8))
        };
        // the MCU's blocks as (component, block index in its storage)
        let mut blocks: Vec<(usize, usize)> = Vec::with_capacity(MAX_BLOCKS_IN_MCU);
        for my in 0..mcus_y {
            for mx in 0..mcus_x {
                blocks.clear();
                for (si, &ci) in self.scan_comps.iter().enumerate() {
                    let c = &self.comps[ci];
                    if single {
                        blocks.push((si, my * c.bw + mx));
                    } else {
                        for by in 0..c.v {
                            for bx in 0..c.h {
                                blocks.push((si, (my * c.v + by) * c.bw + mx * c.h + bx));
                            }
                        }
                    }
                }
                if self.restart_interval != 0 && self.restarts_to_go == 0 {
                    self.restart()?;
                }
                if !self.multi_scan {
                    for &(si, b) in &blocks {
                        let ci = self.scan_comps[si];
                        self.comps[ci].coefs[b] = [0; 64];
                    }
                }
                if !self.progressive {
                    self.mcu_sequential(&blocks, derived)?;
                } else if self.ss == 0 {
                    if self.ah == 0 {
                        self.mcu_dc_first(&blocks, derived)?;
                    } else {
                        self.mcu_dc_refine(&blocks)?;
                    }
                } else if self.ah == 0 {
                    self.mcu_ac_first(blocks[0], derived)?;
                } else {
                    self.mcu_ac_refine(blocks[0], derived)?;
                }
                self.restarts_to_go = self.restarts_to_go.wrapping_sub(1);
            }
        }
        Ok(())
    }

    fn mcu_sequential(&mut self, blocks: &[(usize, usize)], derived: &[Option<Derived>]) -> R<()> {
        if self.insufficient {
            return Ok(());
        }
        for &(si, b) in blocks {
            let ci = self.scan_comps[si];
            let (d, a) = (self.comps[ci].dc_tbl.min(4), 4 + self.comps[ci].ac_tbl.min(4));
            let dct = derived[d].as_ref().expect("a scan's DC table");
            let act = derived[a].as_ref().expect("a scan's AC table");
            let mut s = self.huff_decode(dct)?;
            if s != 0 {
                self.check(s)?;
                let r = self.get_bits(s);
                s = Self::extend(r, s);
            }
            s = s.wrapping_add(self.last_dc[si]);
            self.last_dc[si] = s;
            let mut block = self.comps[ci].coefs[b];
            block[0] = s as i16;
            let mut k = 1usize;
            while k < 64 {
                let rs = self.huff_decode(act)?;
                let r = rs >> 4;
                let s = rs & 15;
                if s != 0 {
                    k += r as usize;
                    self.check(s)?;
                    let v = self.get_bits(s);
                    block[NATURAL_ORDER[k]] = Self::extend(v, s) as i16;
                } else {
                    if r != 15 {
                        break;
                    }
                    k += 15;
                }
                k += 1;
            }
            self.comps[ci].coefs[b] = block;
        }
        Ok(())
    }

    fn mcu_dc_first(&mut self, blocks: &[(usize, usize)], derived: &[Option<Derived>]) -> R<()> {
        if self.insufficient {
            return Ok(());
        }
        for &(si, b) in blocks {
            let ci = self.scan_comps[si];
            let t = derived[self.comps[ci].dc_tbl.min(4)].as_ref().expect("a scan's DC table");
            let mut s = self.huff_decode(t)?;
            if s != 0 {
                self.check(s)?;
                let r = self.get_bits(s);
                s = Self::extend(r, s);
            }
            s = s.wrapping_add(self.last_dc[si]);
            self.last_dc[si] = s;
            self.comps[ci].coefs[b][0] = s.wrapping_shl(self.al as u32) as i16;
        }
        Ok(())
    }

    fn mcu_dc_refine(&mut self, blocks: &[(usize, usize)]) -> R<()> {
        let p1 = 1i16 << self.al;
        for &(si, b) in blocks {
            let ci = self.scan_comps[si];
            self.check(1)?;
            if self.get_bits(1) != 0 {
                self.comps[ci].coefs[b][0] |= p1;
            }
        }
        Ok(())
    }

    fn mcu_ac_first(&mut self, (si, b): (usize, usize), derived: &[Option<Derived>]) -> R<()> {
        if self.insufficient {
            return Ok(());
        }
        if self.eobrun > 0 {
            self.eobrun -= 1;
            return Ok(());
        }
        let ci = self.scan_comps[si];
        let t = derived[4 + self.comps[ci].ac_tbl.min(4)].as_ref().expect("a scan's AC table");
        let mut block = self.comps[ci].coefs[b];
        let mut k = self.ss as usize;
        while k <= self.se as usize {
            let rs = self.huff_decode(t)?;
            let mut r = rs >> 4;
            let s = rs & 15;
            if s != 0 {
                k += r as usize;
                self.check(s)?;
                let v = self.get_bits(s);
                block[NATURAL_ORDER[k]] = Self::extend(v, s).wrapping_shl(self.al as u32) as i16;
            } else if r == 15 {
                k += 15;
            } else {
                let mut eob = 1u32 << r;
                if r != 0 {
                    self.check(r)?;
                    r = self.get_bits(r);
                    eob += r as u32;
                }
                self.eobrun = eob - 1;
                break;
            }
            k += 1;
        }
        self.comps[ci].coefs[b] = block;
        Ok(())
    }

    fn mcu_ac_refine(&mut self, (si, b): (usize, usize), derived: &[Option<Derived>]) -> R<()> {
        if self.insufficient {
            return Ok(());
        }
        let p1 = 1i16.wrapping_shl(self.al as u32);
        let m1 = (-1i16).wrapping_shl(self.al as u32);
        let ci = self.scan_comps[si];
        let t = derived[4 + self.comps[ci].ac_tbl.min(4)].as_ref().expect("a scan's AC table");
        let mut block = self.comps[ci].coefs[b];
        let se = self.se as usize;
        let mut k = self.ss as usize;
        let mut eob = self.eobrun;
        if eob == 0 {
            while k <= se {
                let rs = self.huff_decode(t)?;
                let mut r = rs >> 4;
                let mut s = rs & 15;
                if s != 0 {
                    self.check(1)?;
                    s = if self.get_bits(1) != 0 { p1 as i32 } else { m1 as i32 };
                } else if r != 15 {
                    eob = 1 << r;
                    if r != 0 {
                        self.check(r)?;
                        r = self.get_bits(r);
                        eob += r as u32;
                    }
                    break;
                }
                loop {
                    let z = NATURAL_ORDER[k];
                    if block[z] != 0 {
                        self.check(1)?;
                        if self.get_bits(1) != 0 && (block[z] & p1) == 0 {
                            block[z] =
                                if block[z] >= 0 { block[z].wrapping_add(p1) } else { block[z].wrapping_add(m1) };
                        }
                    } else {
                        r -= 1;
                        if r < 0 {
                            break;
                        }
                    }
                    k += 1;
                    if k > se {
                        break;
                    }
                }
                if s != 0 {
                    block[NATURAL_ORDER[k]] = s as i16;
                }
                k += 1;
            }
        }
        if eob > 0 {
            while k <= se {
                let z = NATURAL_ORDER[k];
                if block[z] != 0 {
                    self.check(1)?;
                    if self.get_bits(1) != 0 && (block[z] & p1) == 0 {
                        block[z] = if block[z] >= 0 { block[z].wrapping_add(p1) } else { block[z].wrapping_add(m1) };
                    }
                }
                k += 1;
            }
            eob -= 1;
        }
        self.eobrun = eob;
        self.comps[ci].coefs[b] = block;
        Ok(())
    }

    // ---- the whole file

    /// `jpeg_read_header` and `jpeg_start_decompress`: the frame, the first scan's start, and with several scans every
    /// scan read. Then (for a decode) the single scan read and the markers to the end of image.
    fn run(&mut self, decode: bool) -> R<()> {
        if !self.read_markers()? {
            return Err(Stop::Fail);
        }
        self.initial_setup()?;
        if self.arith {
            return Err(Stop::Fail);
        }
        let (space, _) = self.color_spaces();
        let n = self.comps.len();
        let ok = match space {
            1 => n == 1,
            2 | 3 => n == 3,
            4 | 5 => n == 4,
            _ => n >= 1,
        };
        if !ok || n == 2 {
            return Err(Stop::Fail);
        }
        self.upsamplers()?;
        for c in &mut self.comps {
            c.coefs = alloc(c.bw * c.bh, [0i16; 64])?;
        }
        let derived = self.start_scan()?;
        if self.multi_scan {
            self.decode_scan(&derived)?;
            loop {
                if !self.read_markers()? {
                    break;
                }
                let derived = self.start_scan()?;
                self.decode_scan(&derived)?;
            }
        } else if decode {
            self.decode_scan(&derived)?;
            // jpeg_finish_decompress: the markers to the end of image (another scan is JERR_EOI_EXPECTED)
            if self.read_markers()? {
                return Err(Stop::Fail);
            }
        }
        Ok(())
    }

    /// jdcoefct.c `smoothing_ok`: a progressive file whose first five AC coefficients are not all complete, with
    /// every component's DC partly known and its quantizers for those nonzero.
    fn smoothing_ok(&self) -> bool {
        if !self.progressive {
            return false;
        }
        let mut useful = false;
        for c in &self.comps {
            let Some(q) = c.quant else { return false };
            if [0, 1, 8, 16, 9, 2].iter().any(|&i| q[i] == 0) || c.coef_bits[0] < 0 {
                return false;
            }
            if c.coef_bits[1..=5].iter().any(|&b| b != 0) {
                useful = true;
            }
        }
        useful
    }

    /// The output rows (jdmainct.c, jdsample.c, jdcolor.c).
    fn output(&self) -> R<Picture> {
        let (w, h) = (self.width as usize, self.height as usize);
        let range = range_limit();
        let idct_base = 256 + 128;
        // the components' sample planes (jdcoefct.c: smoothed blocks while a progressive file's coefficients are
        // incomplete)
        let smooth = self.smoothing_ok();
        let mut planes = Vec::with_capacity(self.comps.len());
        for c in &self.comps {
            let pw = c.wib * 8;
            let mut plane = alloc(pw * c.hib * 8, 0u8)?;
            let q = c.quant.unwrap_or([0; 64]);
            for by in 0..c.hib {
                for bx in 0..c.wib {
                    let block = if smooth { smoothed(c, &q, bx, by) } else { c.coefs[by * c.bw + bx] };
                    idct_islow(&block, &q, &range[idct_base..], &mut plane[by * 8 * pw + bx * 8..], pw);
                }
            }
            planes.push((plane, pw));
        }
        let ups = self.upsamplers()?;
        let (space, out) = self.color_spaces();
        let comps = if out == 0 {
            self.comps.len()
        } else if out == 1 {
            1
        } else if out == 2 {
            3
        } else {
            4
        };
        let mut rows = alloc(w * h * comps, 0u8)?;
        let tabs = YccTables::new();
        let mut up: Vec<Vec<u8>> = vec![vec![0u8; w + 8]; self.comps.len()];
        for y in 0..h {
            for (ci, c) in self.comps.iter().enumerate() {
                let (plane, pw) = (&planes[ci].0, planes[ci].1);
                let row = |r: usize| &plane[r * pw..r * pw + pw];
                let o = &mut up[ci];
                match ups[ci] {
                    0 => o[..w].copy_from_slice(&row(y)[..w]),
                    1 => fancy_h2v1(row(y), c.ds_w, o),
                    3 => {
                        let iy = y / 2;
                        let other = if y % 2 == 0 { iy.saturating_sub(1) } else { (iy + 1).min(c.ds_h - 1) };
                        fancy_h2v2(row(iy), row(other), c.ds_w, o);
                    }
                    k => {
                        let (he, ve) = if k == 2 {
                            (2, 1)
                        } else if k == 4 {
                            (2, 2)
                        } else {
                            (self.max_h / c.h, self.max_v / c.v)
                        };
                        let src = row(y / ve);
                        for x in 0..w {
                            o[x] = src[x / he];
                        }
                    }
                }
            }
            let dst = &mut rows[y * w * comps..(y + 1) * w * comps];
            for x in 0..w {
                let p = &mut dst[x * comps..(x + 1) * comps];
                match space {
                    3 | 5 => {
                        let (yy, cb, cr) = (up[0][x] as i32, up[1][x] as usize, up[2][x] as usize);
                        let r = yy + tabs.cr_r[cr];
                        let g = yy + ((tabs.cb_g[cb] + tabs.cr_g[cr]) >> 16);
                        let b = yy + tabs.cb_b[cb];
                        if space == 3 {
                            p[0] = range[(256 + r) as usize];
                            p[1] = range[(256 + g) as usize];
                            p[2] = range[(256 + b) as usize];
                        } else {
                            p[0] = range[(256 + 255 - r) as usize];
                            p[1] = range[(256 + 255 - g) as usize];
                            p[2] = range[(256 + 255 - b) as usize];
                            p[3] = up[3][x];
                        }
                    }
                    _ => {
                        for (ci, v) in p.iter_mut().enumerate() {
                            *v = up[ci][x];
                        }
                    }
                }
            }
        }
        Ok(Picture { width: self.width, height: self.height, comps, gray: out == 1, rows })
    }
}

/// jdcoefct.c `decompress_smooth_data`'s block (bx, by) of `c`: the coefficients AC01, AC10, AC20, AC11 and AC02,
/// where still zero and not known to be exact, estimated from the DC values of the block and its eight neighbours
/// (the edge blocks their own neighbours), per K.8 (INT32 arithmetic).
fn smoothed(c: &Comp, q: &[u16; 64], bx: usize, by: usize) -> [i16; 64] {
    let dc = |x: usize, y: usize| c.coefs[y * c.bw + x][0] as i32;
    let (py, ny) = (by.saturating_sub(1), if by + 1 == c.hib { by } else { by + 1 });
    let (px, nx) = (bx.saturating_sub(1), if bx + 1 == c.wib { bx } else { bx + 1 });
    let (dc1, dc2, dc3) = (dc(px, py), dc(bx, py), dc(nx, py));
    let (dc4, dc5, dc6) = (dc(px, by), dc(bx, by), dc(nx, by));
    let (dc7, dc8, dc9) = (dc(px, ny), dc(bx, ny), dc(nx, ny));
    let q00 = q[0] as i32;
    let mut w = c.coefs[by * c.bw + bx];
    // (coefficient position, coef_bits index, quantizer, numerator factor and DC sum)
    let est = [
        (1, 1, 36, dc4.wrapping_sub(dc6)),
        (8, 2, 36, dc2.wrapping_sub(dc8)),
        (16, 3, 9, dc2.wrapping_add(dc8).wrapping_sub(dc5.wrapping_mul(2))),
        (9, 4, 5, dc1.wrapping_sub(dc3).wrapping_sub(dc7).wrapping_add(dc9)),
        (2, 5, 9, dc4.wrapping_add(dc6).wrapping_sub(dc5.wrapping_mul(2))),
    ];
    for (pos, bits, k, d) in est {
        let al = c.coef_bits[bits];
        if al == 0 || w[pos] != 0 {
            continue;
        }
        let qk = q[pos] as i32;
        let num = (k * q00).wrapping_mul(d);
        let clamp = |p: i32| if al > 0 && p >= (1 << al) { (1 << al) - 1 } else { p };
        let pred = if num >= 0 {
            clamp((qk << 7).wrapping_add(num) / (qk << 8))
        } else {
            -clamp((qk << 7).wrapping_sub(num) / (qk << 8))
        };
        w[pos] = pred as i16;
    }
    w
}

/// jdmaster.c `prepare_range_limit_table`: the table from `table - (MAXJSAMPLE+1)`; sample_range_limit is at 256,
/// the IDCT's at 256 + 128.
pub fn range_limit() -> Vec<u8> {
    let mut t = vec![0u8; 5 * 256 + 128];
    for i in 0..256 {
        t[256 + i] = i as u8;
    }
    let base = 256 + 128;
    for v in t.iter_mut().take(base + 512).skip(base + 128) {
        *v = 255;
    }
    for v in t.iter_mut().take(base + 896).skip(base + 512) {
        *v = 0;
    }
    for i in 0..128 {
        t[base + 896 + i] = t[256 + i];
    }
    t
}

/// jdcolor.c `build_ycc_rgb_table`.
pub struct YccTables {
    pub cr_r: [i32; 256],
    pub cb_b: [i32; 256],
    pub cr_g: [i32; 256],
    pub cb_g: [i32; 256],
}

impl YccTables {
    pub fn new() -> Self {
        let fix = |x: f64| (x * 65536.0 + 0.5) as i32;
        let half = 1i32 << 15;
        let mut t = YccTables { cr_r: [0; 256], cb_b: [0; 256], cr_g: [0; 256], cb_g: [0; 256] };
        for i in 0..256usize {
            let x = i as i32 - 128;
            t.cr_r[i] = (fix(1.40200).wrapping_mul(x).wrapping_add(half)) >> 16;
            t.cb_b[i] = (fix(1.77200).wrapping_mul(x).wrapping_add(half)) >> 16;
            t.cr_g[i] = (-fix(0.71414)).wrapping_mul(x);
            t.cb_g[i] = (-fix(0.34414)).wrapping_mul(x).wrapping_add(half);
        }
        t
    }
}

impl Default for YccTables {
    fn default() -> Self {
        Self::new()
    }
}

/// jdsample.c `h2v1_fancy_upsample` for one row (`ds_w` > 2).
pub fn fancy_h2v1(inp: &[u8], ds_w: usize, out: &mut [u8]) {
    let g = |i: usize| inp[i] as i32;
    let mut o = 0;
    out[o] = inp[0];
    out[o + 1] = ((g(0) * 3 + g(1) + 2) >> 2) as u8;
    o += 2;
    for i in 1..ds_w - 1 {
        let v = g(i) * 3;
        out[o] = ((v + g(i - 1) + 1) >> 2) as u8;
        out[o + 1] = ((v + g(i + 1) + 2) >> 2) as u8;
        o += 2;
    }
    let last = ds_w - 1;
    if o + 1 < out.len() {
        out[o] = ((g(last) * 3 + g(last - 1) + 1) >> 2) as u8;
        out[o + 1] = inp[last];
    }
}

/// jdsample.c `h2v2_fancy_upsample` for one output row: `near` the nearest input row, `far` the next nearest.
pub fn fancy_h2v2(near: &[u8], far: &[u8], ds_w: usize, out: &mut [u8]) {
    let col = |i: usize| near[i] as i32 * 3 + far[i] as i32;
    let mut this = col(0);
    let mut next = col(1);
    out[0] = ((this * 4 + 8) >> 4) as u8;
    out[1] = ((this * 3 + next + 7) >> 4) as u8;
    let mut last = this;
    this = next;
    let mut o = 2;
    for i in 2..ds_w {
        next = col(i);
        out[o] = ((this * 3 + last + 8) >> 4) as u8;
        out[o + 1] = ((this * 3 + next + 7) >> 4) as u8;
        last = this;
        this = next;
        o += 2;
    }
    if o + 1 < out.len() {
        out[o] = ((this * 3 + last + 8) >> 4) as u8;
        out[o + 1] = ((this * 4 + 7) >> 4) as u8;
    }
}

/// jidctint.c `jpeg_idct_islow` (INT32 is 32 bits, the Windows build's `long`): one block into 8 rows of `out`,
/// `pitch` apart. `range` is the IDCT range limit table.
pub fn idct_islow(coef: &[i16; 64], quant: &[u16; 64], range: &[u8], out: &mut [u8], pitch: usize) {
    const CB: u32 = 13;
    const P1: u32 = 2;
    let m = |a: i32, b: i32| a.wrapping_mul(b);
    let descale = |x: i32, n: u32| x.wrapping_add(1 << (n - 1)) >> n;
    let mut ws = [0i32; 64];
    for c in 0..8 {
        let dq = |r: usize| (coef[r * 8 + c] as i32).wrapping_mul(quant[r * 8 + c] as i32);
        if (1..8).all(|r| coef[r * 8 + c] == 0) {
            let dc = dq(0).wrapping_shl(P1);
            for r in 0..8 {
                ws[r * 8 + c] = dc;
            }
            continue;
        }
        let (z2, z3) = (dq(2), dq(6));
        let z1 = m(z2.wrapping_add(z3), 4433);
        let tmp2 = z1.wrapping_add(m(z3, -15137));
        let tmp3 = z1.wrapping_add(m(z2, 6270));
        let (z2, z3) = (dq(0), dq(4));
        let tmp0 = z2.wrapping_add(z3).wrapping_shl(CB);
        let tmp1 = z2.wrapping_sub(z3).wrapping_shl(CB);
        let (t10, t13, t11, t12) =
            (tmp0.wrapping_add(tmp3), tmp0.wrapping_sub(tmp3), tmp1.wrapping_add(tmp2), tmp1.wrapping_sub(tmp2));
        let (o0, o1, o2, o3) = odd(dq(7), dq(5), dq(3), dq(1));
        let d = |x: i32| descale(x, CB - P1);
        ws[c] = d(t10.wrapping_add(o3));
        ws[7 * 8 + c] = d(t10.wrapping_sub(o3));
        ws[8 + c] = d(t11.wrapping_add(o2));
        ws[6 * 8 + c] = d(t11.wrapping_sub(o2));
        ws[2 * 8 + c] = d(t12.wrapping_add(o1));
        ws[5 * 8 + c] = d(t12.wrapping_sub(o1));
        ws[3 * 8 + c] = d(t13.wrapping_add(o0));
        ws[4 * 8 + c] = d(t13.wrapping_sub(o0));
    }
    for r in 0..8 {
        let w = &ws[r * 8..r * 8 + 8];
        let o = &mut out[r * pitch..r * pitch + 8];
        let lim = |x: i32| range[(x & 1023) as usize];
        if w[1..].iter().all(|&v| v == 0) {
            let v = lim(descale(w[0], P1 + 3));
            o.fill(v);
            continue;
        }
        let (z2, z3) = (w[2], w[6]);
        let z1 = m(z2.wrapping_add(z3), 4433);
        let tmp2 = z1.wrapping_add(m(z3, -15137));
        let tmp3 = z1.wrapping_add(m(z2, 6270));
        let tmp0 = w[0].wrapping_add(w[4]).wrapping_shl(CB);
        let tmp1 = w[0].wrapping_sub(w[4]).wrapping_shl(CB);
        let (t10, t13, t11, t12) =
            (tmp0.wrapping_add(tmp3), tmp0.wrapping_sub(tmp3), tmp1.wrapping_add(tmp2), tmp1.wrapping_sub(tmp2));
        let (o0, o1, o2, o3) = odd(w[7], w[5], w[3], w[1]);
        let d = |x: i32| lim(descale(x, CB + P1 + 3));
        o[0] = d(t10.wrapping_add(o3));
        o[7] = d(t10.wrapping_sub(o3));
        o[1] = d(t11.wrapping_add(o2));
        o[6] = d(t11.wrapping_sub(o2));
        o[2] = d(t12.wrapping_add(o1));
        o[5] = d(t12.wrapping_sub(o1));
        o[3] = d(t13.wrapping_add(o0));
        o[4] = d(t13.wrapping_sub(o0));
    }
}

/// The odd part (figure 8 of the paper jidctint.c follows) -> tmp0..tmp3.
fn odd(t0: i32, t1: i32, t2: i32, t3: i32) -> (i32, i32, i32, i32) {
    let m = |a: i32, b: i32| a.wrapping_mul(b);
    let z1 = t0.wrapping_add(t3);
    let z2 = t1.wrapping_add(t2);
    let z3 = t0.wrapping_add(t2);
    let z4 = t1.wrapping_add(t3);
    let z5 = m(z3.wrapping_add(z4), 9633);
    let t0 = m(t0, 2446);
    let t1 = m(t1, 16819);
    let t2 = m(t2, 25172);
    let t3 = m(t3, 12299);
    let z1 = m(z1, -7373);
    let z2 = m(z2, -20995);
    let z3 = m(z3, -16069).wrapping_add(z5);
    let z4 = m(z4, -3196).wrapping_add(z5);
    (
        t0.wrapping_add(z1).wrapping_add(z3),
        t1.wrapping_add(z2).wrapping_add(z4),
        t2.wrapping_add(z2).wrapping_add(z3),
        t3.wrapping_add(z1).wrapping_add(z4),
    )
}

/// `JPGDecoder::DoGetAttrib`: the size, or why not.
pub fn attributes<I: Input>(input: &mut I) -> R<(u32, u32)> {
    let mut d = Dec::new(input);
    d.run(false)?;
    Ok((d.width, d.height))
}

/// libjpeg's decode of the whole file (the output rows), or why not.
pub fn decode<I: Input>(input: &mut I) -> R<Picture> {
    let mut d = Dec::new(input);
    d.run(true)?;
    d.output()
}

/// `JPGDecoder::DoDecodeRaw`'s pixels: libjpeg's rows as the decoder packed them (greyscale expanded to the three
/// channels, otherwise the first three samples of each run of three, red the high byte), no alpha.
pub fn decode_argb<I: Input>(input: &mut I) -> R<(u32, u32, Vec<u32>)> {
    let p = decode(input)?;
    let (w, h) = (p.width as usize, p.height as usize);
    let mut px = alloc(w * h, 0u32)?;
    for y in 0..h {
        let row = &p.rows[y * w * p.comps..(y + 1) * w * p.comps];
        for x in 0..w {
            px[y * w + x] = if p.gray {
                let v = row[x] as u32;
                v | (v << 8) | (v << 16)
            } else {
                let q = &row[x * 3..x * 3 + 3];
                q[2] as u32 | ((q[1] as u32) << 8) | ((q[0] as u32) << 16)
            };
        }
    }
    Ok((p.width, p.height, px))
}

#[cfg(test)]
#[path = "jpeg_tests.rs"]
mod tests;
