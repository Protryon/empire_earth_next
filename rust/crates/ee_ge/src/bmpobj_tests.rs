//! `bmpobj.rs`'s tests: the golden replay (`golden_gbmp.txt`: the first sequences of the scratch driver over the
//! original GEBitmap section of GEBitmap.cpp with traced fakes) and direct checks of the object.
//!
//! The replay keeps the driver's world by construction serial (four textures, the bitmaps, their vertex arrays as
//! the fakes keep them), makes each "do" line's call on the Rust objects, writes every line the fakes and the driver
//! would write and compares the whole sequence with the golden's; a "step" line is followed by each live bitmap's
//! digest, computed as the driver computes it through the accessors.

use super::*;
use std::cell::RefCell;
use std::collections::BTreeMap;

const GOLDEN: &str = include_str!("golden_gbmp.txt");
const RASTERIZER: usize = 0x5000;
const HANDLE: usize = 0x10;

#[derive(Clone, Copy, Default)]
struct Texture {
    frame_width: f32,
    frame_height: f32,
    width: c_ulong,
    height: c_ulong,
    drop: c_ulong,
}

#[derive(Clone, Copy)]
struct Vertices {
    colors: [[f32; 3]; 4],
    points: [Ge3DPoint; 4],
    opacities: [f32; 4],
    uvs: [[f32; 2]; 4],
}

enum Kind {
    Bitmap(*mut EeGeBitmap),
    Texture(Texture),
    Vertices(Box<Vertices>),
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
    screens: c_ulong,
    out: Vec<String>,
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

fn bits(f: f32) -> String {
    format!("{:08x}", f.to_bits())
}

fn hex(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}

fn name(o: usize) -> String {
    if o == 0 {
        return "0".into();
    }
    with(|w| match w.objects.get(&o) {
        None => "?".into(),
        Some(x) => {
            let c = if matches!(x.kind, Kind::Vertices(_)) { 'V' } else { 'R' };
            format!("{c}{}{}", x.serial, if x.alive { "" } else { "+" })
        }
    })
}

fn new_object(kind: Kind) -> usize {
    with(|w| {
        w.serial += 1;
        let h = w.serial as usize * HANDLE;
        w.objects.insert(h, Obj { serial: w.serial, kind, refs: 1, alive: true });
        h
    })
}

fn handle(s: &str) -> usize {
    if s == "0" {
        0
    } else {
        s[1..].parse::<usize>().unwrap() * HANDLE
    }
}

fn bitmap(h: usize) -> *mut EeGeBitmap {
    with(|w| match w.objects[&h].kind {
        Kind::Bitmap(p) => p,
        _ => panic!("not a bitmap"),
    })
}

fn texture(h: usize) -> Texture {
    with(|w| match w.objects[&h].kind {
        Kind::Texture(t) => t,
        _ => panic!("not a texture"),
    })
}

fn vertices<R>(h: usize, f: impl FnOnce(&mut Vertices) -> R) -> R {
    with(|w| match &mut w.objects.get_mut(&h).unwrap().kind {
        Kind::Vertices(v) => f(v),
        _ => panic!("not a vertex array"),
    })
}

/// The base `Dereference`: the line, then the delete at 0 (a bitmap's object destroyed first, as its destructor).
fn deref(h: usize) {
    let left = with(|w| {
        let o = w.objects.get_mut(&h).unwrap();
        o.refs -= 1;
        o.refs
    });
    emit(format!("deref {} {left}", name(h)));
    if left == 0 {
        let b = with(|w| match w.objects[&h].kind {
            Kind::Bitmap(p) => Some(p),
            _ => None,
        });
        if let Some(p) = b {
            // SAFETY: a live bitmap's object
            unsafe { EeGeBitmap::destroy(p) };
        }
        emit(format!("del {}", name(h)));
        with(|w| w.objects.get_mut(&h).unwrap().alive = false);
    }
}

unsafe extern "C" fn host(_owner: *mut c_void, op: EeGeBmpOp, obj: *mut c_void, a: isize, b: isize) -> isize {
    let o = obj as usize;
    let i = a as usize;
    // SAFETY: the pointers the object passes are as `EeGeBmpOp` describes
    unsafe {
        match op {
            EeGeBmpOp::RasterActive => RASTERIZER as isize,
            EeGeBmpOp::RasterScreenCount => with(|w| w.screens) as isize,
            EeGeBmpOp::RasterVertexArray => {
                let v = Vertices {
                    colors: [[1.0; 3]; 4],
                    points: [Ge3DPoint::default(); 4],
                    opacities: [0.0; 4],
                    uvs: [[0.0; 2]; 4],
                };
                let h = new_object(Kind::Vertices(Box::new(v)));
                emit(format!("{} new t{} n{}", name(h), a as c_ulong, b as c_ulong));
                h as isize
            }
            EeGeBmpOp::VertexDelete => {
                emit(format!("del {}", name(o)));
                with(|w| w.objects.get_mut(&o).unwrap().alive = false);
                0
            }
            EeGeBmpOp::VertexGetColor => {
                let c = vertices(o, |v| v.colors.get(i).copied().unwrap_or([1.0; 3]));
                *(b as *mut [f32; 3]) = c;
                0
            }
            EeGeBmpOp::VertexSetColor => {
                let c = *(b as *const [f32; 3]);
                emit(format!("{} col {i} {} {} {}", name(o), bits(c[0]), bits(c[1]), bits(c[2])));
                vertices(o, |v| {
                    if let Some(s) = v.colors.get_mut(i) {
                        *s = c;
                    }
                });
                0
            }
            EeGeBmpOp::VertexGetPoint => {
                let p = vertices(o, |v| v.points.get(i).copied().unwrap_or_default());
                *(b as *mut Ge3DPoint) = p;
                0
            }
            EeGeBmpOp::VertexSetPoint => {
                let p = *(b as *const Ge3DPoint);
                emit(format!("{} pt {i} {} {} {}", name(o), bits(p.x), bits(p.y), bits(p.z)));
                vertices(o, |v| {
                    if let Some(s) = v.points.get_mut(i) {
                        *s = p;
                    }
                });
                0
            }
            EeGeBmpOp::VertexSetNormal => {
                let n = *(b as *const GeVector);
                emit(format!("{} nm {i} {} {} {}", name(o), bits(n.x), bits(n.y), bits(n.z)));
                0
            }
            EeGeBmpOp::VertexGetOpacity => {
                vertices(o, |v| v.opacities.get(i).copied().unwrap_or(0.0)).to_bits() as isize
            }
            EeGeBmpOp::VertexSetOpacity => {
                let f = f32::from_bits(b as u32);
                emit(format!("{} op {i} {}", name(o), bits(f)));
                vertices(o, |v| {
                    if let Some(s) = v.opacities.get_mut(i) {
                        *s = f;
                    }
                });
                0
            }
            EeGeBmpOp::VertexGetUv => {
                let uv = vertices(o, |v| v.uvs.get(i).copied().unwrap_or_default());
                *(b as *mut [f32; 2]) = uv;
                0
            }
            EeGeBmpOp::VertexSetUv => {
                let uv = *(b as *const [f32; 2]);
                emit(format!("{} uv {i} {} {}", name(o), bits(uv[0]), bits(uv[1])));
                vertices(o, |v| {
                    if let Some(s) = v.uvs.get_mut(i) {
                        *s = uv;
                    }
                });
                0
            }
            EeGeBmpOp::TexReference => {
                let n = with(|w| {
                    let x = w.objects.get_mut(&o).unwrap();
                    x.refs += 1;
                    x.refs
                });
                emit(format!("ref {} {n}", name(o)));
                0
            }
            EeGeBmpOp::TexDereference => {
                emit(format!("tex deref {}", name(o)));
                deref(o);
                0
            }
            EeGeBmpOp::TexFrameWidth => texture(o).frame_width.to_bits() as isize,
            EeGeBmpOp::TexFrameHeight => texture(o).frame_height.to_bits() as isize,
            EeGeBmpOp::TexWidth => texture(o).width as isize,
            EeGeBmpOp::TexHeight => texture(o).height as isize,
            EeGeBmpOp::TexDropLevels => texture(o).drop as isize,
        }
    }
}

/// FNV-1a 64 as the driver's.
struct Fnv(u64);

impl Fnv {
    fn u(&mut self, v: u64) {
        for i in 0..8 {
            self.0 ^= (v >> (i * 8)) & 0xff;
            self.0 = self.0.wrapping_mul(0x100000001b3);
        }
    }
    fn f(&mut self, f: f32) {
        self.u(f.to_bits() as u64);
    }
}

fn serial_of(h: usize) -> i64 {
    if h == 0 {
        return 0;
    }
    with(|w| match w.objects.get(&h) {
        Some(o) if o.alive => o.serial as i64,
        _ => -1,
    })
}

/// The driver's digest of a bitmap, through the object's accessors and per-vertex readers.
unsafe fn digest(h: usize) -> u64 {
    let p = bitmap(h);
    let mut f = Fnv(0xcbf29ce484222325);
    // SAFETY: a live bitmap
    unsafe {
        f.u(with(|w| w.objects[&h].refs) as u64);
        let l = *ee_ge_bmp_location(p);
        f.f(l.x);
        f.f(l.y);
        for v in [
            EeGeBmpFloat::Rotation,
            EeGeBmpFloat::XScale,
            EeGeBmpFloat::YScale,
            EeGeBmpFloat::Width,
            EeGeBmpFloat::Height,
        ] {
            f.f(ee_ge_bmp_getf(p, v));
        }
        for s in 0..with(|w| w.screens) {
            f.u(serial_of(ee_ge_bmp_texture(p, s) as usize) as u64);
        }
        let va = ee_ge_bmp_vertices(p) as usize;
        f.u(with(|w| w.objects.get(&va).map_or(0, |o| o.serial as u64)));
        let mut colors = [[0.0f32; 3]; 4];
        let mut locations = [Ge2DPoint::default(); 4];
        let mut opacities = [0.0f32; 4];
        let mut depths = [0.0f32; 4];
        let mut uvs = [[0.0f32; 2]; 4];
        let quad = |ptrs: [*mut c_void; 4]| ptrs;
        ee_ge_bmp_colors(p, &quad(colors.each_mut().map(|c| c as *mut [f32; 3] as *mut c_void)));
        ee_ge_bmp_locations(p, &quad(locations.each_mut().map(|c| c as *mut Ge2DPoint as *mut c_void)));
        ee_ge_bmp_opacities(p, &quad(opacities.each_mut().map(|c| c as *mut f32 as *mut c_void)));
        ee_ge_bmp_depths(p, &quad(depths.each_mut().map(|c| c as *mut f32 as *mut c_void)));
        ee_ge_bmp_uvs(p, &quad(uvs.each_mut().map(|c| c as *mut [f32; 2] as *mut c_void)));
        for i in 0..4 {
            for c in colors[i] {
                f.f(c);
            }
            f.f(locations[i].x);
            f.f(locations[i].y);
            f.f(opacities[i]);
            f.f(depths[i]);
            f.f(uvs[i][0]);
            f.f(uvs[i][1]);
        }
    }
    f.0
}

fn step() {
    emit("step".into());
    let live: Vec<usize> = with(|w| {
        w.objects.iter().filter(|(_, o)| o.alive && matches!(o.kind, Kind::Bitmap(_))).map(|(h, _)| *h).collect()
    });
    for h in live {
        // SAFETY: a live bitmap
        let d = unsafe { digest(h) };
        emit(format!("B {} {d:016x}", name(h)));
    }
}

fn floats<const N: usize>(w: &[&str]) -> [f32; N] {
    std::array::from_fn(|k| hex(w[k]))
}

/// A "do" line's call.
unsafe fn run(line: &str) {
    emit(line.to_string());
    let w: Vec<&str> = line.split(' ').collect();
    let b = || bitmap(handle(w[2]));
    // SAFETY: the golden names live bitmaps
    unsafe {
        match w[1] {
            "Begin" => {
                with(|x| x.screens = w[2].parse().unwrap());
                for k in 0..4 {
                    let f = &w[3 + k * 5..8 + k * 5];
                    let t = Texture {
                        frame_width: hex(f[0]),
                        frame_height: hex(f[1]),
                        width: f[2].parse().unwrap(),
                        height: f[3].parse().unwrap(),
                        drop: f[4].parse().unwrap(),
                    };
                    new_object(Kind::Texture(t));
                }
            }
            "New" => {
                let h = new_object(Kind::Bitmap(std::ptr::null_mut()));
                let p = EeGeBitmap::create(host, h);
                with(|x| x.objects.get_mut(&h).unwrap().kind = Kind::Bitmap(p));
                emit(format!("= {}", name(h)));
            }
            "Reference" => {
                let h = handle(w[2]);
                let n = with(|x| {
                    let o = x.objects.get_mut(&h).unwrap();
                    o.refs += 1;
                    o.refs
                });
                emit(format!("ref {} {n}", name(h)));
            }
            "Deref" => deref(handle(w[2])),
            "SetTexture" => EeGeBitmap::set_texture(b(), handle(w[3]), w[4].parse().unwrap()),
            "Stretch" => {
                let r: [f32; 12] = floats(&w[4..16]);
                let rect = |k: usize| GeRectangle {
                    bottom: r[k * 4],
                    left: r[k * 4 + 1],
                    right: r[k * 4 + 2],
                    top: r[k * 4 + 3],
                };
                EeGeBitmap::stretch(b(), &rect(0), &rect(1), &rect(2), w[3].parse().unwrap());
            }
            "FlipHorizontally" => ee_ge_bmp_flip(b(), false),
            "FlipVertically" => ee_ge_bmp_flip(b(), true),
            "SetColor" | "SetColorPerVertex" => {
                let c: Vec<[f32; 3]> = if w[1] == "SetColor" {
                    vec![floats(&w[3..6]); 4]
                } else {
                    (0..4).map(|k| floats(&w[3 + k * 3..6 + k * 3])).collect()
                };
                let q: EeGeBmpQuad = std::array::from_fn(|k| &c[k] as *const [f32; 3] as *mut c_void);
                ee_ge_bmp_set_colors(b(), &q);
            }
            "SetOpacity" => ee_ge_bmp_set_opacities(b(), &[hex(w[3]); 4]),
            "SetOpacityPerVertex" => ee_ge_bmp_set_opacities(b(), &floats(&w[3..7])),
            "SetRotation" => ee_ge_bmp_set_rotation(b(), hex(w[3])),
            "SetScale" => EeGeBitmap::set_xy_scale(b(), hex(w[3]), hex(w[3])),
            "SetXYScale" => EeGeBitmap::set_xy_scale(b(), hex(w[3]), hex(w[4])),
            "SetLocation" => ee_ge_bmp_set_location(b(), &Ge2DPoint { x: hex(w[3]), y: hex(w[4]) }),
            "SetTextureCoordinates" => {
                let t: Vec<[f32; 2]> = (0..4).map(|k| floats(&w[3 + k * 2..5 + k * 2])).collect();
                let q: EeGeBmpQuad = std::array::from_fn(|k| &t[k] as *const [f32; 2] as *mut c_void);
                ee_ge_bmp_set_uvs(b(), &q);
            }
            "SetZ" => EeGeBitmap::set_z_per_vertex(b(), [hex(w[3]); 4]),
            "SetZPerVertex" => EeGeBitmap::set_z_per_vertex(b(), floats(&w[3..7])),
            other => panic!("unknown op {other}"),
        }
    }
}

fn replay(lines: &[&str]) -> Option<String> {
    with(|w| *w = World::default());
    for l in lines {
        if l.starts_with("do ") {
            // SAFETY: the golden names live bitmaps
            unsafe { run(l) };
        } else if *l == "step" {
            step();
        } else if *l == "end" {
            emit("end".into());
        } else if l.starts_with("T ") {
            let h = handle(l.split(' ').nth(1).unwrap());
            emit(format!("T {} {}", name(h), with(|w| w.objects[&h].refs)));
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

/// SetTexture: the same texture again changes nothing; another releases the old one and takes its frame size.
#[test]
fn set_texture_swaps_references_and_takes_the_frame_size() {
    with(|w| {
        *w = World::default();
        w.screens = 2;
    });
    let t1 = new_object(Kind::Texture(Texture { frame_width: 8.0, frame_height: 4.0, ..Texture::default() }));
    let t2 = new_object(Kind::Texture(Texture { frame_width: 2.0, frame_height: 6.0, ..Texture::default() }));
    let h = new_object(Kind::Bitmap(std::ptr::null_mut()));
    // SAFETY: the mock host
    unsafe {
        let p = EeGeBitmap::create(host, h);
        with(|w| w.out.clear());
        EeGeBitmap::set_texture(p, t1, 1);
        assert_eq!(ee_ge_bmp_getf(p, EeGeBmpFloat::Width), 8.0);
        assert_eq!(ee_ge_bmp_getf(p, EeGeBmpFloat::Height), 4.0);
        with(|w| w.out.clear());
        EeGeBitmap::set_texture(p, t1, 1);
        assert!(with(|w| w.out.is_empty()));
        EeGeBitmap::set_texture(p, t2, 1);
        let out = with(|w| std::mem::take(&mut w.out));
        assert_eq!(&out[..3], ["tex deref R1", "deref R1 1", "ref R2 2"]);
        assert_eq!(ee_ge_bmp_texture(p, 1) as usize, t2);
        assert_eq!(ee_ge_bmp_texture(p, 0) as usize, 0);
        assert_eq!(ee_ge_bmp_texture(p, 5) as usize, 0);
        EeGeBitmap::destroy(p);
    }
    let out = with(|w| std::mem::take(&mut w.out));
    assert_eq!(out, ["del V4", "tex deref R2", "deref R2 1"]);
}

/// The flips set the whole texture mirrored.
#[test]
fn the_flips_mirror_the_whole_texture() {
    with(|w| {
        *w = World::default();
        w.screens = 1;
    });
    let h = new_object(Kind::Bitmap(std::ptr::null_mut()));
    // SAFETY: the mock host
    unsafe {
        let p = EeGeBitmap::create(host, h);
        with(|w| w.out.clear());
        ee_ge_bmp_flip(p, false);
        ee_ge_bmp_flip(p, true);
        let out = with(|w| std::mem::take(&mut w.out));
        assert_eq!(out[0], "V2 uv 0 3f800000 00000000");
        assert_eq!(out[4], "V2 uv 0 00000000 3f800000");
        EeGeBitmap::destroy(p);
    }
}

/// The stretch reads the screen's texture's size and dropped levels and sets `bitmap.rs`'s coordinates and scale.
#[test]
fn the_stretch_uses_the_textures_dropped_levels() {
    with(|w| {
        *w = World::default();
        w.screens = 1;
    });
    let t =
        new_object(Kind::Texture(Texture { frame_width: 64.0, frame_height: 64.0, width: 64, height: 32, drop: 1 }));
    let h = new_object(Kind::Bitmap(std::ptr::null_mut()));
    let r = |l: f32, t: f32, w: f32, h: f32| GeRectangle { bottom: t + h, left: l, right: l + w, top: t };
    let (texture, source, destination) = (r(0.0, 0.0, 15.0, 7.0), r(2.0, 1.0, 9.0, 4.0), r(5.0, 5.0, 20.0, 10.0));
    let expect = bitmap::stretched_coords(&texture, &source, &destination, 64, 32, 1);
    assert_ne!(expect, bitmap::stretched_coords(&texture, &source, &destination, 64, 32, 0));
    // SAFETY: the mock host
    unsafe {
        let p = EeGeBitmap::create(host, h);
        EeGeBitmap::set_texture(p, t, 0);
        EeGeBitmap::stretch(p, &texture, &source, &destination, 0);
        let mut uvs = [[0.0f32; 2]; 4];
        let q: EeGeBmpQuad = std::array::from_fn(|k| &mut uvs[k] as *mut [f32; 2] as *mut c_void);
        ee_ge_bmp_uvs(p, &q);
        assert_eq!(uvs, expect.uv);
        assert_eq!(ee_ge_bmp_getf(p, EeGeBmpFloat::XScale), expect.x_scale);
        EeGeBitmap::destroy(p);
    }
}
