//! `fbobj.rs`'s tests: the golden replay (`golden_gfb.txt`: sequences of the scratch driver over the original
//! GEFontBitmap.cpp with traced fakes) and direct checks of the object.
//!
//! The replay keeps the driver's world by construction serial (fonts, font bitmaps, the working and the driver's
//! texts, the glyph surfaces' pixels, textures, drawing bitmaps, the rasterizer's blending), makes each "do" line's
//! call on the Rust objects, writes every line the fakes and the driver would write and compares the whole sequence
//! with the golden's.

use super::*;
use ee_plat_core::string::{PlatCp1252ByteOf, PlatCp1252ToUtf8, PlatMbsNInc, PlatUtf8Decode};
use std::cell::RefCell;
use std::collections::BTreeMap;

const GOLDEN: &str = include_str!("golden_gfb.txt");
const RASTER: usize = 0x7000;
const SCREEN: usize = 0x7100;
const HANDLE: usize = 0x10;

unsafe extern "C" fn measure(_font: *const c_void, _text: *const c_char, _length: u32) -> i32 {
    0
}

static OPS: GeTextOps =
    GeTextOps { decode: PlatUtf8Decode, next_chars: PlatMbsNInc, cp1252_byte_of: PlatCp1252ByteOf, measure };

fn fnv(bytes: &[u8], mut h: u64) -> u64 {
    for &b in bytes {
        h ^= u64::from(b);
        h = h.wrapping_mul(0x100000001b3);
    }
    h
}
const FNV: u64 = 0xcbf29ce484222325;

struct Font {
    first: c_ulong,
    last: c_ulong,
    height: u64,
    width_range: u64,
    salt: u64,
    bitmap: usize,
}

struct Text {
    font: usize,
    location: Box<Ge2DPoint>,
    fg: Box<[f32; 3]>,
    extent: [f32; 2],
    bytes: Vec<u8>,
}

struct Surface {
    width: c_ulong,
    height: c_ulong,
    bytes_per_row: c_ulong,
    memory: Vec<u16>,
}

impl Surface {
    fn bytes(&self) -> &[u8] {
        let n = (self.bytes_per_row * self.height) as usize;
        // SAFETY: the memory holds more than n bytes
        unsafe { std::slice::from_raw_parts(self.memory.as_ptr().cast::<u8>(), n) }
    }
}

struct Bitmap {
    scales: [f32; 2],
    uvs: [[f32; 2]; 4],
}

enum Kind {
    Font(Font),
    FontBitmap(*mut EeGeFontBitmap),
    Text(Text),
    Surface(Surface),
    Texture(usize),
    Bitmap(Bitmap),
}

struct Obj {
    serial: i32,
    kind: Kind,
    refs: c_ulong,
    alive: bool,
}

#[derive(Default)]
struct World {
    objects: BTreeMap<usize, Obj>,
    serial: i32,
    blend: [[c_ulong; 2]; 3],
    draws: i64,
    /// The stretch's scales made unequal (the fakes' are 1 for a whole cell).
    skew: bool,
    out: Vec<String>,
    panic: Option<String>,
}

thread_local! {
    static WORLD: RefCell<World> = RefCell::new(World::default());
}

fn with<R>(f: impl FnOnce(&mut World) -> R) -> R {
    WORLD.with(|w| f(&mut w.borrow_mut()))
}

fn emit(line: String) {
    with(|w| w.out.push(line));
}

fn new_object(kind: Kind) -> usize {
    with(|w| {
        w.serial += 1;
        let h = w.serial as usize * HANDLE;
        w.objects.insert(h, Obj { serial: w.serial, kind, refs: 1, alive: true });
        h
    })
}

fn name(h: usize) -> String {
    if h == 0 {
        return "0".into();
    }
    with(|w| {
        let o = &w.objects[&h];
        let k = if matches!(o.kind, Kind::Text(_)) { 'X' } else { 'R' };
        format!("{k}{}{}", o.serial, if o.alive { "" } else { "+" })
    })
}

fn handle(name: &str) -> usize {
    name[1..].trim_end_matches('+').parse::<usize>().unwrap() * HANDLE
}

fn bits(f: f32) -> String {
    format!("{:08x}", f.to_bits())
}

fn hexf(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}

fn reference(h: usize) {
    let n = with(|w| {
        let o = w.objects.get_mut(&h).unwrap();
        o.refs += 1;
        o.refs
    });
    emit(format!("ref {} {n}", name(h)));
}

fn dereference(h: usize) {
    let n = with(|w| {
        let o = w.objects.get_mut(&h).unwrap();
        o.refs -= 1;
        o.refs
    });
    emit(format!("deref {} {n}", name(h)));
    if n != 0 {
        return;
    }
    // the derived destructor's body, then the base's trace
    let (bitmap, font_bitmap) = with(|w| match &w.objects[&h].kind {
        Kind::FontBitmap(p) => (*p, 0),
        Kind::Font(f) => (std::ptr::null_mut(), f.bitmap),
        _ => (std::ptr::null_mut(), 0),
    });
    if !bitmap.is_null() {
        // SAFETY: the object is live until here
        unsafe { EeGeFontBitmap::destroy(bitmap) };
    }
    if font_bitmap != 0 {
        dereference(font_bitmap);
    }
    emit(format!("del {}", name(h)));
    with(|w| w.objects.get_mut(&h).unwrap().alive = false);
}

fn text_mut<R>(h: usize, f: impl FnOnce(&mut Text) -> R) -> R {
    with(|w| match &mut w.objects.get_mut(&h).unwrap().kind {
        Kind::Text(t) => f(t),
        _ => panic!("not a text"),
    })
}

fn surface_mut<R>(h: usize, f: impl FnOnce(&mut Surface) -> R) -> R {
    with(|w| match &mut w.objects.get_mut(&h).unwrap().kind {
        Kind::Surface(s) => f(s),
        _ => panic!("not a surface"),
    })
}

fn bitmap_mut<R>(h: usize, f: impl FnOnce(&mut Bitmap) -> R) -> R {
    with(|w| match &mut w.objects.get_mut(&h).unwrap().kind {
        Kind::Bitmap(b) => f(b),
        _ => panic!("not a bitmap"),
    })
}

fn rect(r: &GeRectangle) -> String {
    format!("{} {} {} {}", bits(r.left), bits(r.top), bits(r.right), bits(r.bottom))
}

/// The fakes' `GEText::SetText`.
fn set_text(h: usize, bytes: &[u8]) {
    let hex: String = bytes.iter().map(|b| format!("{b:02x}")).collect();
    emit(format!("settext {} {hex}", name(h)));
    let font = text_mut(h, |t| t.font);
    let (salt, range, height) = with(|w| match &w.objects[&font].kind {
        Kind::Font(f) => (f.salt, f.width_range, f.height),
        _ => panic!("not a font"),
    });
    let x = fnv(bytes, salt);
    let width = (1 + x % range) as f32 + if (x >> 20) & 1 != 0 { 0.5 } else { 0.0 };
    let tall = (height + (x >> 30) % 3) as f32;
    text_mut(h, |t| {
        t.bytes = bytes.to_vec();
        t.bytes.push(0);
        t.extent = [width, tall];
    });
}

fn new_text(font: usize) -> usize {
    let h = new_object(Kind::Text(Text {
        font,
        location: Box::default(),
        fg: Box::new([1.0; 3]),
        extent: [0.0; 2],
        bytes: vec![0],
    }));
    emit(format!("text new {} {}", name(h), name(font)));
    reference(font);
    h
}

fn delete_text(h: usize) {
    emit(format!("text del {}", name(h)));
    let font = text_mut(h, |t| t.font);
    dereference(font);
}

unsafe extern "C" fn host(owner: *mut c_void, op: EeGeFbOp, obj: *mut c_void, a: isize, b: isize) -> isize {
    match std::panic::catch_unwind(|| {
        // SAFETY: the object's host contract
        unsafe { serve(owner as usize, op, obj as usize, a, b) }
    }) {
        Ok(r) => r,
        Err(e) => {
            let m = e.downcast_ref::<String>().cloned().or_else(|| e.downcast_ref::<&str>().map(|s| s.to_string()));
            with(|w| w.panic.get_or_insert(format!("{op:?}: {}", m.unwrap_or_default())).clone());
            0
        }
    }
}

/// The fakes' side of one host call.
///
/// # Safety
/// The pointers are as `EeGeFbOp` describes.
unsafe fn serve(owner: usize, op: EeGeFbOp, obj: usize, a: isize, b: isize) -> isize {
    // SAFETY: the function's contract
    unsafe {
        match op {
            EeGeFbOp::TextNew => new_text(obj) as isize,
            EeGeFbOp::TextDelete => {
                delete_text(obj);
                0
            }
            EeGeFbOp::TextPrepare => {
                emit(format!("fg {} 3f800000 3f800000 3f800000", name(obj)));
                text_mut(obj, |t| *t.fg = [1.0; 3]);
                emit(format!("transparent {} 1", name(obj)));
                0
            }
            EeGeFbOp::TextSetByte => {
                let byte = a as u8 as c_char;
                let mut cell = [0u8; 8];
                PlatCp1252ToUtf8(&byte, 1, cell.as_mut_ptr().cast(), 8);
                let n = cell.iter().position(|&c| c == 0).unwrap_or(8);
                set_text(obj, &cell[..n]);
                0
            }
            EeGeFbOp::TextExtent => {
                *(a as *mut [f32; 2]) = text_mut(obj, |t| t.extent);
                0
            }
            EeGeFbOp::TextSetLocation => {
                let p = *(a as *const Ge2DPoint);
                emit(format!("textloc {} {} {}", name(obj), bits(p.x), bits(p.y)));
                text_mut(obj, |t| *t.location = p);
                0
            }
            EeGeFbOp::TextString => text_mut(obj, |t| t.bytes.as_ptr() as isize),
            EeGeFbOp::TextLocation => text_mut(obj, |t| &*t.location as *const Ge2DPoint as isize),
            EeGeFbOp::TextColor => text_mut(obj, |t| t.fg.as_ptr() as isize),
            EeGeFbOp::TextOps => &OPS as *const GeTextOps as isize,
            EeGeFbOp::FontRange => {
                emit(format!("metrics {}", name(obj)));
                let range = with(|w| match &w.objects[&obj].kind {
                    Kind::Font(f) => [f.first, f.last],
                    _ => panic!("not a font"),
                });
                *(a as *mut [c_ulong; 2]) = range;
                0
            }
            EeGeFbOp::FontSetBitmap => {
                emit(format!("setfontbitmap {} {}", name(obj), name(owner)));
                with(|w| match &mut w.objects.get_mut(&obj).unwrap().kind {
                    Kind::Font(f) => f.bitmap = owner,
                    _ => panic!("not a font"),
                });
                reference(owner);
                0
            }
            EeGeFbOp::RasterScreen => {
                assert_eq!(obj, RASTER);
                (SCREEN + a as usize) as isize
            }
            EeGeFbOp::RasterGetBlending => {
                *(a as *mut [c_ulong; 2]) = with(|w| w.blend[b as usize]);
                0
            }
            EeGeFbOp::RasterSetBlending => {
                let v = *(a as *const [c_ulong; 2]);
                emit(format!("blend {} {} {b}", v[0], v[1]));
                with(|w| w.blend[b as usize] = v);
                0
            }
            EeGeFbOp::RasterDrawText => {
                let text = a as usize;
                let surface = b as usize;
                let (location, bytes) = text_mut(text, |t| (*t.location, t.bytes.clone()));
                emit(format!("drawtext {} {} {} {}", name(text), name(surface), bits(location.x), bits(location.y)));
                let h = fnv(&bytes[..bytes.len() - 1], FNV);
                let (x, y) = (location.x as i64, location.y as i64);
                surface_mut(surface, |s| {
                    for r in 0..5i64 {
                        for c in 0..4i64 {
                            let (xx, yy) = (x + c, y + r);
                            if xx >= 0 && yy >= 0 && (xx as c_ulong) < s.width && (yy as c_ulong) < s.height {
                                let v = (h >> ((r * 4 + c) % 48)) as u16;
                                s.memory[(yy as usize * s.bytes_per_row as usize + xx as usize * 2) / 2] = v;
                            }
                        }
                    }
                });
                0
            }
            EeGeFbOp::RasterDrawBitmap => {
                let r = with(|w| {
                    w.draws += 1;
                    w.draws * 7 % 5 - 2
                });
                emit(format!("drawbmp {} {b} -> {r}", name(a as usize)));
                r as isize
            }
            EeGeFbOp::BmpNew => {
                let h = new_object(Kind::Bitmap(Bitmap { scales: [1.0; 2], uvs: [[0.0; 2]; 4] }));
                emit(format!("bitmap new {}", name(h)));
                h as isize
            }
            EeGeFbOp::BmpDereference => {
                dereference(obj);
                0
            }
            EeGeFbOp::BmpSetTexture => {
                emit(format!("bmptex {} {} {b}", name(obj), name(a as usize)));
                0
            }
            EeGeFbOp::BmpStretch => {
                let r = &*(a as *const [GeRectangle; 2]);
                let (t, c) = (r[0], r[1]);
                emit(format!("stretch {} {} | {} | {} {b}", name(obj), rect(&t), rect(&t), rect(&c)));
                bitmap_mut(obj, |m| {
                    m.scales =
                        [(c.right - c.left) / (t.right - t.left + 1.0), (c.bottom - c.top) / (t.bottom - t.top + 1.0)];
                    m.uvs = [
                        [t.left / 256.0, t.top / 256.0],
                        [t.right / 256.0, t.top / 256.0],
                        [t.right / 256.0, t.bottom / 256.0],
                        [t.left / 256.0 + c.left, t.bottom / 256.0],
                    ];
                });
                if with(|w| w.skew) {
                    bitmap_mut(obj, |m| m.scales = [2.0, 3.0]);
                }
                0
            }
            EeGeFbOp::BmpUvsPerVertex => {
                emit(format!("getuvs {}", name(obj)));
                let uvs = bitmap_mut(obj, |m| m.uvs);
                *(a as *mut [[f32; 2]; 2]) = [uvs[0], uvs[3]];
                0
            }
            EeGeFbOp::BmpScales => {
                *(a as *mut [f32; 2]) = bitmap_mut(obj, |m| m.scales);
                0
            }
            EeGeFbOp::BmpSetColor => {
                let c = *(a as *const [f32; 3]);
                emit(format!("bmpcolor {} {} {} {}", name(obj), bits(c[0]), bits(c[1]), bits(c[2])));
                0
            }
            EeGeFbOp::BmpSetUvs => {
                let c = *(a as *const [[f32; 2]; 4]);
                let s: Vec<String> = c.iter().flat_map(|uv| [bits(uv[0]), bits(uv[1])]).collect();
                emit(format!("bmpuvs {} {}", name(obj), s.join(" ")));
                0
            }
            EeGeFbOp::BmpSetXyScale => {
                let (x, y) = (f32::from_bits(a as u32), f32::from_bits(b as u32));
                emit(format!("bmpscale {} {} {}", name(obj), bits(x), bits(y)));
                bitmap_mut(obj, |m| m.scales = [x, y]);
                0
            }
            EeGeFbOp::BmpSetLocation => {
                let p = *(a as *const Ge2DPoint);
                emit(format!("bmploc {} {} {}", name(obj), bits(p.x), bits(p.y)));
                0
            }
            EeGeFbOp::TexSurface => with(|w| match &w.objects[&obj].kind {
                Kind::Texture(s) => *s as isize,
                _ => panic!("not a texture"),
            }),
            EeGeFbOp::TexReference => {
                reference(obj);
                0
            }
            EeGeFbOp::TexStage => {
                let label = match a {
                    x if x == EeGeTexStage::MinFilterMode as isize => "minf",
                    x if x == EeGeTexStage::MagFilterMode as isize => "magf",
                    x if x == EeGeTexStage::MipFilterMode as isize => "mipf",
                    x if x == EeGeTexStage::TextureAddressingMode as isize => "addr",
                    x if x == EeGeTexStage::AlphaMode as isize => "alpha",
                    x if x == EeGeTexStage::ColorMode as isize => "color",
                    _ => panic!("stage {a}"),
                };
                emit(format!("{label} {} {b} -1", name(obj)));
                0
            }
            EeGeFbOp::SurfTextUnlock => {
                let h = surface_mut(obj, |s| fnv(s.bytes(), FNV));
                emit(format!("textunlock {} {h:016x}", name(obj)));
                0
            }
            EeGeFbOp::SurfLock => {
                emit(format!("lock {}", name(obj)));
                0
            }
            EeGeFbOp::SurfUnlock => {
                let h = surface_mut(obj, |s| fnv(s.bytes(), FNV));
                emit(format!("unlock {} {h:016x}", name(obj)));
                0
            }
            EeGeFbOp::SurfPixels => surface_mut(obj, |s| {
                *(a as *mut [c_ulong; 3]) = [s.width, s.height, s.bytes_per_row];
                s.memory.as_mut_ptr() as isize
            }),
            EeGeFbOp::SurfClear => {
                emit(format!("clear {} 00000000 00000000 00000000 3f800000", name(obj)));
                surface_mut(obj, |s| {
                    let n = (s.bytes_per_row * s.height) as usize / 2;
                    s.memory[..n].fill(0x5a5a);
                });
                0
            }
            EeGeFbOp::ScreenCreateSurface => {
                let d = *(a as *const EeGeSurfaceDescription);
                let serial = with(|w| w.serial + 1);
                let bytes_per_row = d.width * 2 + (serial as c_ulong % 2) * 8;
                let n = (bytes_per_row * d.height) as usize + 16;
                let s = new_object(Kind::Surface(Surface {
                    width: d.width,
                    height: d.height,
                    bytes_per_row,
                    memory: vec![0; n / 2],
                }));
                emit(format!(
                    "createsurface {} {} {} {} {} {} {} {} {} {} {} {} {}",
                    obj - SCREEN,
                    name(s),
                    d.kind,
                    d.pixel_format,
                    d.surface_volatility,
                    d.width,
                    d.height,
                    d.m14,
                    d.mip_level_count,
                    d.pixel_depth,
                    d.is_3d_renderable as i32,
                    d.is_transparency_enabled as i32,
                    d.no_alpha as i32
                ));
                *(b as *mut usize) = s;
                0
            }
            EeGeFbOp::ScreenMakeTexture => {
                let t = new_object(Kind::Texture(a as usize));
                emit(format!("maketexture {} {}", name(t), name(a as usize)));
                t as isize
            }
            EeGeFbOp::Verify => {
                emit(format!("verify {}", if a == 0 { 228 } else { 235 }));
                0
            }
        }
    }
}

fn font_bitmap_of(h: usize) -> *mut EeGeFontBitmap {
    with(|x| match &x.objects[&h].kind {
        Kind::FontBitmap(p) => *p,
        _ => panic!("not a font bitmap"),
    })
}

/// One driver line ("do ..."), with the sequence's later lines for a clipped draw's rectangle.
///
/// # Safety
/// The golden names live objects.
unsafe fn run(l: &str, rest: &[&str]) {
    let w: Vec<&str> = l.split(' ').collect();
    // SAFETY: the function's contract
    unsafe {
        match w[1] {
            "Font" => {
                let n = |i: usize| w[i].parse::<u64>().unwrap();
                let h = new_object(Kind::Font(Font {
                    first: n(3) as c_ulong,
                    last: n(4) as c_ulong,
                    height: n(5),
                    width_range: n(6),
                    salt: n(7),
                    bitmap: 0,
                }));
                assert_eq!(name(h), w[2]);
                emit(l.to_string());
            }
            "New" => {
                let s: usize = w[3].parse().unwrap();
                with(|x| x.blend[s] = [w[4].parse().unwrap(), w[5].parse().unwrap()]);
                emit(l.to_string());
                let owner = new_object(Kind::FontBitmap(std::ptr::null_mut()));
                let p = EeGeFontBitmap::create(host, owner, RASTER, handle(w[2]), s as c_ulong);
                with(|x| x.objects.get_mut(&owner).unwrap().kind = Kind::FontBitmap(p));
                emit(format!("made {}", name(owner)));
            }
            "Draw" => {
                // do Draw <font bitmap> <font> <clipped> <screen> <x> <y> <r> <g> <b> <text's hex or ->
                emit(l.to_string());
                let p = font_bitmap_of(handle(w[2]));
                let text = new_text(handle(w[3]));
                let bytes: Vec<u8> = if w[11] == "-" {
                    Vec::new()
                } else {
                    (0..w[11].len() / 2).map(|i| u8::from_str_radix(&w[11][2 * i..2 * i + 2], 16).unwrap()).collect()
                };
                text_mut(text, |t| {
                    t.bytes = bytes;
                    t.bytes.push(0);
                    *t.location = Ge2DPoint { x: hexf(w[6]), y: hexf(w[7]) };
                    *t.fg = [hexf(w[8]), hexf(w[9]), hexf(w[10])];
                });
                let screen: c_ulong = w[5].parse().unwrap();
                let res = if w[4] == "1" {
                    let c = rest.iter().find(|r| r.starts_with("clip ")).unwrap();
                    emit(c.to_string());
                    let v: Vec<f32> = c.split(' ').skip(1).map(hexf).collect();
                    let clip = GeRectangle { bottom: v[3], left: v[0], right: v[2], top: v[1] };
                    EeGeFontBitmap::draw(p, text, Some(&clip), RASTER, screen)
                } else {
                    EeGeFontBitmap::draw(p, text, None, RASTER, screen)
                };
                emit(format!("drew {res}"));
                delete_text(text);
            }
            "Eq" => emit(l.to_string()),
            "Deref" | "DerefFont" => {
                emit(l.to_string());
                dereference(handle(w[2]));
            }
            other => panic!("unknown op {other}"),
        }
    }
}

fn replay(lines: &[&str]) -> Option<String> {
    with(|w| *w = World::default());
    for (i, l) in lines.iter().enumerate() {
        if l.starts_with("do ") {
            // SAFETY: the golden names live objects
            unsafe { run(l, &lines[i + 1..]) };
        } else if *l == "end" {
            emit("end".into());
        }
        if let Some(p) = with(|w| w.panic.take()) {
            return Some(format!("host panic at line {i}: {p}"));
        }
    }
    let out = with(|w| std::mem::take(&mut w.out));
    for (i, l) in lines.iter().enumerate() {
        let got = out.get(i).map(String::as_str).unwrap_or("<none>");
        if got != *l {
            return Some(format!("line {i}: want [{l}] got [{got}]"));
        }
    }
    (out.len() != lines.len()).then(|| format!("{} lines, the golden has {}", out.len(), lines.len()))
}

#[test]
fn the_golden_replays() {
    let mut sequences: BTreeMap<u32, Vec<&str>> = BTreeMap::new();
    for line in GOLDEN.lines() {
        let (s, l) = line.split_once("| ").unwrap();
        sequences.entry(s.parse().unwrap()).or_default().push(l);
    }
    assert!(sequences.len() >= 20);
    for (s, lines) in &sequences {
        if let Some(e) = replay(lines) {
            panic!("sequence {s}: {e}");
        }
    }
}

/// A font of `first..=last` whose every glyph is `width` x `height`, its font bitmap made on screen 1.
fn made(first: c_ulong, last: c_ulong, range: u64, height: u64) -> (usize, *mut EeGeFontBitmap) {
    with(|w| *w = World::default());
    let font = new_object(Kind::Font(Font { first, last, height, width_range: range, salt: 0, bitmap: 0 }));
    let owner = new_object(Kind::FontBitmap(std::ptr::null_mut()));
    // SAFETY: the mock host
    let p = unsafe { EeGeFontBitmap::create(host, owner, RASTER, font, 1) };
    with(|w| w.objects.get_mut(&owner).unwrap().kind = Kind::FontBitmap(p));
    assert!(with(|w| w.panic.is_none()));
    (font, p)
}

/// The constructor: one cell per character, a new 256 x 256 texture when the rows reach the bottom, every finished
/// texture turned into alpha, the blending restored, the font holding the object, the working text released.
#[test]
fn the_constructor_lays_out_every_character_and_restores_the_blending() {
    let (font, p) = made(0x20, 0x7e, 1, 120);
    let out = with(|w| std::mem::take(&mut w.out));
    // 95 cells of about 1 x 120: two rows a texture, 256 cells a row: one texture
    assert_eq!(out.iter().filter(|l| l.starts_with("createsurface")).count(), 1);
    assert_eq!(out.iter().filter(|l| l.starts_with("settext")).count(), 95);
    assert_eq!(out.iter().filter(|l| l.starts_with("blend")).count(), 2);
    assert_eq!(out.iter().filter(|l| l.starts_with("blend")).collect::<Vec<_>>(), ["blend 4 5 1", "blend 0 0 1"]);
    assert!(out.iter().any(|l| l.starts_with("setfontbitmap")));
    assert_eq!(out.last().unwrap(), &format!("deref {} 1", name(font)));
    // SAFETY: the object is live
    unsafe {
        assert_eq!(fb(p).characters.len(), 95);
        assert_eq!(fb(p).first_character, 0x20);
        assert!(fb(p).characters.iter().all(|c| !c.texture.is_null()));
    }
    // the glyphs' textures: each finished one is unlocked as text, locked, made alpha and unlocked
    let n = out.iter().position(|l| l.starts_with("textunlock")).unwrap();
    assert!(out[n + 1].starts_with("lock") && out[n + 2].starts_with("unlock"));
}

/// Tall glyphs fill a texture's rows and start a new texture; the finished one is made alpha before the next is
/// created.
#[test]
fn tall_glyphs_start_new_textures() {
    let (_, p) = made(0x41, 0x5a, 120, 120);
    let out = with(|w| std::mem::take(&mut w.out));
    let surfaces = out.iter().filter(|l| l.starts_with("createsurface")).count();
    assert!(surfaces > 1);
    let first = out.iter().position(|l| l.starts_with("createsurface")).unwrap();
    let second = first + 1 + out[first + 1..].iter().position(|l| l.starts_with("createsurface")).unwrap();
    assert!(out[first..second].iter().any(|l| l.starts_with("unlock")));
    // SAFETY: the object is live
    let textures: std::collections::BTreeSet<usize> =
        unsafe { fb(p).characters.iter().map(|c| c.texture as usize).collect() };
    assert_eq!(textures.len(), surfaces);
}

/// A draw sets the colour once, then per glyph the texture, the cell's coordinates and scales and the location;
/// a clipped draw stretches the glyphs across the edge and skips the ones outside (their texture still set).
#[test]
fn the_draws_set_each_glyph_and_return_the_last_result() {
    let (font, p) = made(0x61, 0x63, 1, 10);
    let text = new_text(font);
    text_mut(text, |t| {
        t.bytes = b"abcz\0".to_vec();
        *t.location = Ge2DPoint { x: 4.0, y: 5.0 };
    });
    with(|w| w.out.clear());
    // SAFETY: the mock host
    let r = unsafe { EeGeFontBitmap::draw(p, text, None, RASTER, 0) };
    let out = with(|w| std::mem::take(&mut w.out));
    assert!(out[0].starts_with("bmpcolor"));
    assert_eq!(out.iter().filter(|l| l.starts_with("drawbmp")).count(), 3);
    assert_eq!(out.iter().filter(|l| l.starts_with("bmpuvs")).count(), 3);
    assert_eq!(r, 3 * 7 % 5 - 2);
    let clip = GeRectangle { bottom: 100.0, left: 0.0, right: 4.5, top: 0.0 };
    // SAFETY: the mock host
    unsafe { EeGeFontBitmap::draw(p, text, Some(&clip), RASTER, 0) };
    let out = with(|w| std::mem::take(&mut w.out));
    // SAFETY: the object is live
    let rects: Vec<GeRectangle> = unsafe { fb(p).characters.iter().map(|c| c.rect).collect() };
    let mut plan = Vec::new();
    font::draw_plan(b"abcz\0", 0x61, &rects, Ge2DPoint { x: 4.0, y: 5.0 }, Some(&clip), &OPS, &mut plan);
    let kinds = |k: u32| plan.iter().filter(|g| g.kind == k).count();
    assert_eq!(out.iter().filter(|l| l.starts_with("bmptex")).count(), 3);
    assert_eq!(out.iter().filter(|l| l.starts_with("drawbmp")).count(), 3 - kinds(GE_GLYPH_SKIPPED));
    assert_eq!(out.iter().filter(|l| l.starts_with("stretch")).count(), kinds(font::GE_GLYPH_CLIPPED));
    assert_eq!(out.iter().filter(|l| l.starts_with("bmpscale")).count(), kinds(GE_GLYPH_FULL));
}

/// The destructor releases the drawing bitmap only; NULL objects are ignored at the C ABI.
#[test]
fn the_destructor_releases_the_bitmap() {
    let (_, p) = made(0x30, 0x31, 3, 9);
    with(|w| w.out.clear());
    // SAFETY: the mock host; the object is freed
    unsafe {
        ee_ge_fb_destroy(p);
        ee_ge_fb_destroy(std::ptr::null_mut());
        assert_eq!(
            ee_ge_fb_draw(std::ptr::null_mut(), std::ptr::null_mut(), std::ptr::null(), std::ptr::null_mut(), 0),
            0
        );
    }
    let out = with(|w| std::mem::take(&mut w.out));
    assert_eq!(out.len(), 2);
    assert!(out[0] == "deref R4 0" && out[1] == "del R4");
}

/// Each cell keeps the bitmap's x and y scales after its stretch, in that order.
#[test]
fn the_cells_keep_the_stretched_scales() {
    with(|w| *w = World::default());
    let font = new_object(Kind::Font(Font { first: 0x41, last: 0x42, height: 9, width_range: 5, salt: 0, bitmap: 0 }));
    let owner = new_object(Kind::FontBitmap(std::ptr::null_mut()));
    with(|w| w.skew = true);
    // SAFETY: the mock host
    let p = unsafe { EeGeFontBitmap::create(host, owner, RASTER, font, 0) };
    // SAFETY: the object is live
    unsafe {
        assert!(fb(p).characters.iter().all(|c| c.x_scale == 2.0 && c.y_scale == 3.0));
        EeGeFontBitmap::destroy(p);
    }
}
