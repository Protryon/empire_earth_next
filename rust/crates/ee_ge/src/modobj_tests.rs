//! `modobj.rs`'s tests: the golden replay (`golden_gmod.txt`: the first sequences of the scratch driver over the
//! original GEModel.cpp and 'GEModel Load.cpp' with traced fakes) and a few direct checks of the object.
//!
//! The replay keeps a world of the driver's objects by construction serial (models, materials, textures, vertex and
//! polygon arrays as the fakes keep them; `UString`s as byte vectors), makes each "do" line's call on the Rust
//! objects, writes every line the fakes and the driver would write, and compares the whole sequence with the
//! golden's. A "step" line is followed by each live model's digest, computed as the driver computes it through the
//! accessors. The decoded models come from the driver's generator (`modobj_cemgen.rs`).

use super::*;
use crate::model_store::{CemFrame, CemMaterial, CemPolygons, Counts};
use std::cell::RefCell;
use std::collections::BTreeMap;

include!("modobj_cemgen.rs");

const GOLDEN: &str = include_str!("golden_gmod.txt");
const RASTERIZER: usize = 0x5000;
const TEXTURE_MANAGER: usize = 0x6000;
const MODEL_MANAGER: usize = 0x7000;
const HANDLE: usize = 0x10;

#[derive(Default)]
struct Material {
    name: Vec<u8>,
    screens: c_ulong,
    lods: c_ulong,
    textures: Vec<usize>,
    starts: Vec<c_ulong>,
    lengths: Vec<c_ulong>,
    vertex_start: c_ulong,
    vertex_length: c_ulong,
    texture_index: c_ulong,
}

enum Kind {
    Model(*mut EeGeModel),
    Material(Material),
    Texture,
    Vertices(Vec<Ge3DPoint>, Vec<GeVector>),
    Polygons(Vec<u32>),
}

struct Obj {
    serial: i32,
    kind: Kind,
    refs: c_ulong,
    alive: bool,
}

#[derive(Default)]
struct World {
    /// By handle (`serial * HANDLE`).
    objects: BTreeMap<usize, Obj>,
    serial: i32,
    screen_id: c_ulong,
    caches: [BTreeMap<Vec<u8>, usize>; 2],
    /// `new UString[n]` arrays by address, with their length.
    arrays: BTreeMap<usize, usize>,
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

fn name(o: usize) -> String {
    if o == 0 {
        return "0".into();
    }
    with(|w| match w.objects.get(&o) {
        None => "?".into(),
        Some(x) => {
            let c = match x.kind {
                Kind::Vertices(..) => 'V',
                Kind::Polygons(_) => 'P',
                _ => 'R',
            };
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

fn rust_of(h: usize) -> *mut EeGeModel {
    with(|w| match w.objects.get(&h).map(|o| &o.kind) {
        Some(Kind::Model(p)) => *p,
        _ => panic!("not a model: {h:#x}"),
    })
}

fn handle_of(serial: &str) -> usize {
    serial.trim_start_matches(['R', 'V', 'P']).parse::<usize>().unwrap() * HANDLE
}

/// `UReferenceCounter::Dereference` (the fake's): the line, then the delete at 0.
fn deref(h: usize) {
    let left = with(|w| {
        let o = w.objects.get_mut(&h).unwrap();
        o.refs -= 1;
        o.refs
    });
    emit(format!("deref {} {left}", name(h)));
    if left == 0 {
        let model = with(|w| match w.objects[&h].kind {
            Kind::Model(p) => Some(p),
            _ => None,
        });
        if let Some(p) = model {
            // SAFETY: a live model's object
            unsafe { EeGeModel::destroy(p) };
        }
        emit(format!("del {}", name(h)));
        with(|w| w.objects.get_mut(&h).unwrap().alive = false);
    }
}

fn string(p: usize) -> Vec<u8> {
    // SAFETY: a `UString` of the mock (a byte vector)
    unsafe { (*(p as *const Vec<u8>)).clone() }
}

fn text(b: &[u8]) -> String {
    String::from_utf8_lossy(b).into_owned()
}

/// The fakes and the C++'s other objects.
unsafe extern "C" fn host(_owner: *mut c_void, op: EeGeModOp, obj: *mut c_void, a: isize, b: isize, c: isize) -> isize {
    let o = obj as usize;
    match op {
        EeGeModOp::RasterActive => RASTERIZER as isize,
        EeGeModOp::RasterVertexArray => {
            let n = b as c_ulong;
            let h = new_object(Kind::Vertices(
                vec![Ge3DPoint::default(); n as usize],
                vec![GeVector::default(); n as usize],
            ));
            emit(format!("{} new f{} n{n}", name(h), a as c_ulong));
            h as isize
        }
        EeGeModOp::RasterPolygonArray => {
            let n = a as c_ulong;
            let h = new_object(Kind::Polygons(vec![0; n as usize * 3]));
            emit(format!("{} new n{n}", name(h)));
            h as isize
        }
        EeGeModOp::RasterScreenId => with(|w| w.screen_id) as isize,
        EeGeModOp::RasterTextureManager => (TEXTURE_MANAGER + a as usize) as isize,
        EeGeModOp::VertexDelete | EeGeModOp::PolygonDelete => {
            emit(format!("del {}", name(o)));
            with(|w| w.objects.get_mut(&o).unwrap().alive = false);
            0
        }
        EeGeModOp::VertexGather => {
            for i in 0..a as usize {
                let p = with(|w| match &w.objects[&o].kind {
                    Kind::Vertices(p, _) => p.get(i).copied().unwrap_or_default(),
                    _ => unreachable!(),
                });
                // SAFETY: the out array holds `a`
                unsafe { *(b as *mut Ge3DPoint).add(i) = p };
            }
            0
        }
        EeGeModOp::VertexGetPoint => {
            let p = with(|w| match &w.objects[&o].kind {
                Kind::Vertices(p, _) => p.get(a as usize).copied().unwrap_or_default(),
                _ => unreachable!(),
            });
            // SAFETY: an out point
            unsafe { *(b as *mut Ge3DPoint) = p };
            0
        }
        EeGeModOp::VertexGetNormal => {
            let n = with(|w| match &w.objects[&o].kind {
                Kind::Vertices(_, n) => n.get(a as usize).copied().unwrap_or_default(),
                _ => unreachable!(),
            });
            // SAFETY: an out vector
            unsafe { *(b as *mut GeVector) = n };
            0
        }
        EeGeModOp::VertexSetPoint => {
            // SAFETY: an in point
            let p = unsafe { *(b as *const Ge3DPoint) };
            emit(format!("{} pt {} {} {} {}", name(o), a as c_ulong, bits(p.x), bits(p.y), bits(p.z)));
            with(|w| {
                if let Kind::Vertices(v, _) = &mut w.objects.get_mut(&o).unwrap().kind {
                    if let Some(s) = v.get_mut(a as usize) {
                        *s = p;
                    }
                }
            });
            0
        }
        EeGeModOp::VertexSetNormal => {
            // SAFETY: an in vector
            let n = unsafe { *(b as *const GeVector) };
            emit(format!("{} nm {} {} {} {}", name(o), a as c_ulong, bits(n.x), bits(n.y), bits(n.z)));
            with(|w| {
                if let Kind::Vertices(_, v) = &mut w.objects.get_mut(&o).unwrap().kind {
                    if let Some(s) = v.get_mut(a as usize) {
                        *s = n;
                    }
                }
            });
            0
        }
        EeGeModOp::VertexSetUv => {
            // SAFETY: two floats
            let (u, v) = unsafe { (*(b as *const f32), *(b as *const f32).add(1)) };
            emit(format!("{} uv {} {} {}", name(o), a as c_ulong, bits(u), bits(v)));
            0
        }
        EeGeModOp::VertexSize => with(|w| match &w.objects[&o].kind {
            Kind::Vertices(p, _) => p.len() as isize,
            _ => unreachable!(),
        }),
        EeGeModOp::VertexSizeOf => with(|w| match &w.objects[&o].kind {
            Kind::Vertices(p, _) => (0x20 + p.len() as c_ulong * 36) as isize,
            _ => unreachable!(),
        }),
        EeGeModOp::PolygonGather => {
            let v = with(|w| match &w.objects[&o].kind {
                Kind::Polygons(v) => v.clone(),
                _ => unreachable!(),
            });
            for k in 0..a as usize * 3 {
                // SAFETY: the out array holds `3 a`
                unsafe { *(b as *mut u32).add(k) = v.get(k).copied().unwrap_or(0) };
            }
            0
        }
        EeGeModOp::PolygonSetIndex => {
            emit(format!("{} i{c} {} {}", name(o), a as c_ulong, b as c_ulong));
            with(|w| {
                if let Kind::Polygons(v) = &mut w.objects.get_mut(&o).unwrap().kind {
                    if (a as usize) < v.len() / 3 {
                        v[a as usize * 3 + c as usize - 1] = b as u32;
                    }
                }
            });
            0
        }
        EeGeModOp::PolygonSizeOf => with(|w| match &w.objects[&o].kind {
            Kind::Polygons(v) => (0x10 + (v.len() / 3) as c_ulong * 12) as isize,
            _ => unreachable!(),
        }),
        EeGeModOp::MaterialNew => {
            let (screens, lods) = (a as c_ulong, b as c_ulong);
            let m = Material {
                screens,
                lods,
                textures: vec![0; screens as usize],
                starts: vec![0; lods as usize],
                lengths: vec![0; lods as usize],
                ..Material::default()
            };
            let h = new_object(Kind::Material(m));
            emit(format!("{} material s{screens} l{lods}", name(h)));
            h as isize
        }
        EeGeModOp::MaterialDereference | EeGeModOp::ModelBaseDereference => {
            deref(o);
            0
        }
        EeGeModOp::MaterialSetTexture => {
            emit(format!("{} settex {} {}", name(o), name(a as usize), b as c_ulong));
            with(|w| {
                if let Kind::Material(m) = &mut w.objects.get_mut(&o).unwrap().kind {
                    if let Some(s) = m.textures.get_mut(b as usize) {
                        *s = a as usize;
                    }
                }
            });
            0
        }
        EeGeModOp::MaterialRangeLength | EeGeModOp::MaterialStart | EeGeModOp::MaterialVertexStart => {
            with(|w| match &w.objects[&o].kind {
                Kind::Material(m) => match op {
                    EeGeModOp::MaterialRangeLength => m.lengths.get(a as usize).copied().unwrap_or(0),
                    EeGeModOp::MaterialStart => m.starts.get(a as usize).copied().unwrap_or(0),
                    _ => m.vertex_start,
                },
                _ => unreachable!(),
            }) as isize
        }
        EeGeModOp::MaterialSizeOf => with(|w| match &w.objects[&o].kind {
            Kind::Material(m) => (0x48 + m.lods * 8 + m.screens * 4) as isize,
            _ => unreachable!(),
        }),
        EeGeModOp::MaterialSetName => {
            // SAFETY: `b` bytes at `a`
            let n = unsafe { std::slice::from_raw_parts(a as *const u8, b as usize) }.to_vec();
            emit(format!("{} name '{}'", name(o), text(&n)));
            with(|w| {
                if let Kind::Material(m) = &mut w.objects.get_mut(&o).unwrap().kind {
                    m.name = n;
                }
            });
            0
        }
        EeGeModOp::MaterialSetValue => {
            let (v, l) = (a as c_ulong, b as c_ulong);
            let line = match c {
                0 => format!("{} ti {v}", name(o)),
                1 => format!("{} ps {v} l{l}", name(o)),
                2 => format!("{} pl {v} l{l}", name(o)),
                3 => format!("{} vs {v}", name(o)),
                _ => format!("{} vl {v}", name(o)),
            };
            emit(line);
            with(|w| {
                if let Kind::Material(m) = &mut w.objects.get_mut(&o).unwrap().kind {
                    match c {
                        0 => m.texture_index = v,
                        1 => {
                            if let Some(s) = m.starts.get_mut(l as usize) {
                                *s = v;
                            }
                        }
                        2 => {
                            if let Some(s) = m.lengths.get_mut(l as usize) {
                                *s = v;
                            }
                        }
                        3 => m.vertex_start = v,
                        _ => m.vertex_length = v,
                    }
                }
            });
            0
        }
        EeGeModOp::ModelNew => {
            let h = new_object(Kind::Model(std::ptr::null_mut()));
            // SAFETY: the mock host serves the handle
            let p = unsafe { EeGeModel::create(host, h, o) };
            with(|w| w.objects.get_mut(&h).unwrap().kind = Kind::Model(p));
            h as isize
        }
        EeGeModOp::ModelObj => rust_of(o) as isize,
        EeGeModOp::ModelReference => {
            let n = with(|w| {
                let x = w.objects.get_mut(&o).unwrap();
                x.refs += 1;
                x.refs
            });
            emit(format!("ref {} {n}", name(o)));
            0
        }
        EeGeModOp::ModelRefCount => with(|w| w.objects[&o].refs) as isize,
        EeGeModOp::ManagerRemove => {
            emit(format!("G{} remove {}", o - MODEL_MANAGER, name(a as usize)));
            deref(a as usize);
            0
        }
        EeGeModOp::StrNew => Box::into_raw(Box::new(Vec::<u8>::new())) as isize,
        EeGeModOp::StrDelete => {
            // SAFETY: from StrNew
            drop(unsafe { Box::from_raw(o as *mut Vec<u8>) });
            0
        }
        EeGeModOp::StrArrayNew => {
            let n = a as usize;
            let p = Box::into_raw(vec![Vec::<u8>::new(); n].into_boxed_slice()) as *mut Vec<u8> as usize;
            let p = if n == 0 { with(|w| w.serial as usize * HANDLE + 0x8000_0000) } else { p };
            with(|w| w.arrays.insert(p, n));
            p as isize
        }
        EeGeModOp::StrArrayDelete => {
            let n = with(|w| w.arrays.remove(&o)).unwrap();
            if n > 0 {
                // SAFETY: from StrArrayNew
                drop(unsafe { Box::from_raw(std::ptr::slice_from_raw_parts_mut(o as *mut Vec<u8>, n)) });
            }
            0
        }
        EeGeModOp::StrAt => (o as *mut Vec<u8>).wrapping_add(a as usize) as isize,
        EeGeModOp::StrAssign => {
            let v = string(a as usize);
            // SAFETY: a mock `UString`
            unsafe { *(o as *mut Vec<u8>) = v };
            0
        }
        EeGeModOp::StrSetBytes => {
            // SAFETY: `b` bytes at `a`; a mock `UString`
            unsafe { *(o as *mut Vec<u8>) = std::slice::from_raw_parts(a as *const u8, b as usize).to_vec() };
            0
        }
        EeGeModOp::StrLength => string(o).len() as isize,
        EeGeModOp::StrSizeOf => (string(o).len() + 13) as isize,
        EeGeModOp::StrFirst => string(o).first().map_or(0, |c| *c as i8 as isize),
        EeGeModOp::TexLoad => {
            let k = o - TEXTURE_MANAGER;
            let n = string(a as usize);
            // SAFETY: the driver's description (its id)
            let d = unsafe { *(b as *const i32) };
            let t = if n.first() == Some(&b'x') {
                0
            } else if let Some(t) = with(|w| w.caches[k].get(&n).copied()) {
                t
            } else {
                let t = new_object(Kind::Texture);
                with(|w| w.caches[k].insert(n.clone(), t));
                t
            };
            emit(format!("TM{} load '{}' d{d} = {}", k + 1, text(&n), name(t)));
            t as isize
        }
        EeGeModOp::TexMinFilter | EeGeModOp::TexMagFilter | EeGeModOp::TexMipFilter => {
            let what = match op {
                EeGeModOp::TexMinFilter => "min",
                EeGeModOp::TexMagFilter => "mag",
                _ => "mip",
            };
            emit(format!("{} {what} {} {:x}", name(o), a as c_ulong, b as c_ulong));
            0
        }
    }
}

/// FNV-1a 64 as the driver's (values as eight bytes, floats by their bits, strings with their length).
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
    fn s(&mut self, b: &[u8]) {
        self.u(b.len() as u64);
        for &c in b {
            self.0 ^= c as u64;
            self.0 = self.0.wrapping_mul(0x100000001b3);
        }
    }
    fn p(&mut self, p: Ge3DPoint) {
        self.f(p.x);
        self.f(p.y);
        self.f(p.z);
    }
    fn t(&mut self, t: &GeTransformation) {
        for r in 0..4 {
            for c in 0..4 {
                self.f(t.m[r][c]);
            }
        }
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

fn first_of<T: PartialEq>(v: &[T], i: usize) -> usize {
    (0..i).find(|&k| v[k] == v[i]).unwrap_or(i)
}

/// The driver's digest of a model, through the object's accessors.
unsafe fn digest(h: usize) -> u64 {
    let p = rust_of(h);
    let mut f = Fnv(0xcbf29ce484222325);
    // SAFETY: a live model
    unsafe {
        let get = |v| ee_ge_mod_get(p, v) as usize;
        let ptr = |v| ee_ge_mod_ptr(p, v);
        f.u(with(|w| w.objects[&h].refs) as u64);
        f.u(serial_of(get(EeGeModVal::Parent)) as u64);
        f.s(&string(ptr(EeGeModPtr::Name) as usize));
        let (frames, tags, mats) =
            (get(EeGeModVal::FrameCount), get(EeGeModVal::TagPointCount), get(EeGeModVal::MaterialCount));
        let (lods, subs) = (get(EeGeModVal::LodCount), get(EeGeModVal::SubModelCount));
        for v in [subs, frames, mats, get(EeGeModVal::PolygonCount), get(EeGeModVal::VertexCount), tags, lods] {
            f.u(v as u64);
        }
        f.u(get(EeGeModVal::CurrentFrame) as u64);
        f.u(get(EeGeModVal::NextFrame) as u64);
        f.f(ee_ge_mod_getf(p, EeGeModFloat::GraphicsScale));
        f.f(ee_ge_mod_getf(p, EeGeModFloat::Opacity));
        f.f(ee_ge_mod_getf(p, EeGeModFloat::FractionalFramePosition));
        for i in 0..6 {
            f.u(ee_ge_mod_clip(p, i) as u64);
        }
        for x in [EeGeModPtr::PreTransform, EeGeModPtr::PostTransform, EeGeModPtr::GraphicsTransform] {
            f.t(&*(ptr(x) as *const GeTransformation));
        }
        f.p(*(ptr(EeGeModPtr::Center) as *const Ge3DPoint));
        let shared = get(EeGeModVal::SharedFrameCount);
        f.u(shared as u64);
        for i in 0..shared {
            let (mut fr, mut off): (c_ulong, c_long) = (0, 0);
            ee_ge_mod_shared_frame(p, i as c_ulong, &mut fr, &mut off);
            f.u(fr as u64);
            f.u(off as i64 as u64);
        }
        let it = ptr(EeGeModPtr::InternalTransforms) as *const GeTransformation;
        f.u(!it.is_null() as u64);
        for k in 0..if it.is_null() { 0 } else { frames } {
            f.t(&*it.add(k));
        }
        let b = ptr(EeGeModPtr::BoundingBoxes) as *const GeBoundingBox;
        f.u(!b.is_null() as u64);
        for k in 0..if b.is_null() { 0 } else { frames } {
            let x = *b.add(k);
            f.p(x.bottom_left_front);
            f.p(x.top_right_back);
            f.f(x.width);
            f.f(x.height);
            f.f(x.depth);
        }
        let g = ptr(EeGeModPtr::GraphicsBoundingBox) as usize;
        f.u(if g == 0 { 0xffff_ffff } else { (g.wrapping_sub(b as usize) as isize / 36) as i64 as u64 });
        let r = ptr(EeGeModPtr::BoundingRadii) as *const f32;
        f.u(!r.is_null() as u64);
        for k in 0..if r.is_null() { 0 } else { frames } {
            f.f(*r.add(k));
        }
        let ft = ptr(EeGeModPtr::FrameTagPoints) as *const *mut Ge3DPoint;
        let cur = ptr(EeGeModPtr::TagPoints) as *mut Ge3DPoint;
        let fts: Vec<*mut Ge3DPoint> =
            if ft.is_null() { Vec::new() } else { (0..frames).map(|k| *ft.add(k)).collect() };
        let which = fts.iter().position(|x| *x == cur).unwrap_or(0xffff);
        f.u(if cur.is_null() { 0xfffe } else { which as u64 });
        f.u(!ft.is_null() as u64);
        for k in 0..fts.len() {
            let first = first_of(&fts, k);
            f.u(first as u64);
            f.u(!fts[k].is_null() as u64);
            if first == k && !fts[k].is_null() {
                for i in 0..tags {
                    f.p(*fts[k].add(i));
                }
            }
        }
        let tn = ptr(EeGeModPtr::TagPointNames) as *const Vec<u8>;
        f.u(!tn.is_null() as u64);
        for i in 0..if tn.is_null() { 0 } else { tags } {
            f.s(&*tn.add(i));
        }
        let ms = ptr(EeGeModPtr::Materials) as *const usize;
        let fnames = ptr(EeGeModPtr::TextureFilenames) as *const Vec<u8>;
        f.u(!ms.is_null() as u64);
        f.u(!fnames.is_null() as u64);
        for i in 0..if ms.is_null() { 0 } else { mats } {
            let m = *ms.add(i);
            f.u(serial_of(m) as u64);
            let fields = with(|w| match &w.objects[&m].kind {
                Kind::Material(x) => (
                    x.name.clone(),
                    x.screens,
                    x.textures.clone(),
                    x.lods,
                    x.starts.clone(),
                    x.lengths.clone(),
                    [x.vertex_start, x.vertex_length, x.texture_index],
                ),
                _ => unreachable!(),
            });
            f.s(&fields.0);
            f.u(fields.1 as u64);
            for t in fields.2 {
                f.u(serial_of(t) as u64);
            }
            f.u(fields.3 as u64);
            for k in 0..fields.3 as usize {
                f.u(fields.4[k] as u64);
                f.u(fields.5[k] as u64);
            }
            for v in fields.6 {
                f.u(v as u64);
            }
            if !fnames.is_null() {
                f.s(&*fnames.add(i));
            }
        }
        let serial = |x: usize| with(|w| w.objects.get(&x).map_or(0, |o| o.serial as u64));
        let pa = ptr(EeGeModPtr::PolygonArrays) as *const usize;
        f.u(!pa.is_null() as u64);
        for l in 0..if pa.is_null() { 0 } else { lods } {
            f.u(serial(*pa.add(l)));
        }
        let va = ptr(EeGeModPtr::VertexArrays) as *const usize;
        f.u(!va.is_null() as u64);
        let vas: Vec<usize> = if va.is_null() { Vec::new() } else { (0..frames).map(|k| *va.add(k)).collect() };
        for k in 0..vas.len() {
            f.u(first_of(&vas, k) as u64);
            f.u(serial(vas[k]));
        }
        let sm = ptr(EeGeModPtr::SubModels) as *const usize;
        f.u(!sm.is_null() as u64);
        for i in 0..if sm.is_null() { 0 } else { subs } {
            f.u(serial_of(*sm.add(i)) as u64);
        }
        if frames != 0 {
            f.u(ee_ge_mod_size_of(p) as u64);
            for l in 0..lods {
                f.u(ee_ge_mod_polygon_count(p, l as c_ulong) as u64);
            }
            f.u(ee_ge_mod_polygon_count_of_hierarchy(p, 0) as u64);
        }
    }
    f.0
}

fn hex_float(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}

fn quoted(line: &str) -> Vec<u8> {
    let a = line.find('\'').unwrap();
    let b = line.rfind('\'').unwrap();
    line.as_bytes()[a + 1..b].to_vec()
}

/// A "do" line's call.
unsafe fn run(line: &str) {
    emit(line.to_string());
    let w: Vec<&str> = line.split(' ').collect();
    let model = |i: usize| rust_of(handle_of(w[i]));
    let num = |i: usize| w[i].parse::<c_ulong>().unwrap();
    // SAFETY: live models named by the golden
    unsafe {
        match w[1] {
            "Begin" => with(|x| x.screen_id = num(2)),
            "Load" => {
                let manager = MODEL_MANAGER + w[2][1..].parse::<usize>().unwrap();
                let shared: Vec<(c_ulong, c_long)> = w[4..]
                    .iter()
                    .map(|e| {
                        let (a, b) = e.split_once(':').unwrap();
                        (a.parse().unwrap(), b.parse().unwrap())
                    })
                    .collect();
                let h = new_object(Kind::Model(std::ptr::null_mut()));
                let p = EeGeModel::create(host, h, manager);
                with(|x| x.objects.get_mut(&h).unwrap().kind = Kind::Model(p));
                let cem = cem_random(w[3].parse().unwrap());
                let r = EeGeModel::store_loaded(p, &cem, &shared);
                emit(format!("= {} {r}", name(h)));
            }
            "NewCounts" => {
                let counts = [num(2), num(3), num(4), num(5), num(6), num(7)];
                let at = line.rfind('\'').unwrap();
                let rest: Vec<&str> = line[at + 2..].split(' ').collect();
                let flags = rest[0][1..].parse::<c_ulong>().unwrap();
                let manager = MODEL_MANAGER + rest[1][1..].parse::<usize>().unwrap();
                let n = Box::into_raw(Box::new(quoted(line)));
                let h = new_object(Kind::Model(std::ptr::null_mut()));
                let p = EeGeModel::create_with_counts(host, h, counts, n as usize, flags, manager);
                drop(Box::from_raw(n));
                with(|x| x.objects.get_mut(&h).unwrap().kind = Kind::Model(p));
                emit(format!("= {}", name(h)));
            }
            "SetSub" => EeGeModel::set_sub_model(model(2), handle_of(w[4]), num(3)),
            "Deref" => deref(handle_of(w[2])),
            "Reference" => {
                let h = handle_of(w[2]);
                let n = with(|x| {
                    let o = x.objects.get_mut(&h).unwrap();
                    o.refs += 1;
                    o.refs
                });
                emit(format!("ref {} {n}", name(h)));
            }
            "Dereference" => EeGeModel::dereference(model(2)),
            "ComputeBoundingVolumes" => EeGeModel::compute_bounding_volumes(model(2)),
            "ComputeModelNormals" => EeGeModel::compute_normals(model(2)),
            "PrepareTransforms" => EeGeModel::prepare_transforms(model(2)),
            "RemoveScale" => EeGeModel::remove_scale(model(2)),
            "SortTagPoints" => EeGeModel::sort_tag_points(model(2)),
            "Verify" => emit(format!("= {}", EeGeModel::verify(model(2)) as i32)),
            "PolygonCount" => {
                let (a, b) = (
                    EeGeModel::polygon_count_at(model(2), num(3)),
                    EeGeModel::polygon_count_of_hierarchy(model(2), num(3)),
                );
                emit(format!("= {a} {b}"));
            }
            "SetFractional" => EeGeModel::set_fractional(model(2), hex_float(w[3])),
            "SetCurrentFrame" => EeGeModel::set_current_frame(model(2), num(3)),
            "SetNextFrame" => EeGeModel::set_next_frame(model(2), num(3)),
            "SetTexture" => {
                let t = if w[3] == "0" { 0 } else { handle_of(w[3]) };
                EeGeModel::set_texture(model(2), t, num(4));
            }
            "LoadTextures" => {
                let d: i32 = w[3][1..].parse().unwrap();
                let r = EeGeModel::load_textures(model(2), &d as *const i32 as usize, num(4));
                emit(format!("= {r}"));
            }
            "SetOpacity" => EeGeModel::set_opacity(model(2), hex_float(w[3])),
            "SetClip" => ee_ge_mod_set_clip(model(2), num(3), w[4] == "1"),
            "SetScale" => ee_ge_mod_set_graphics_scale(model(2), hex_float(w[3])),
            "SetTransform" => {
                let mut t = GeTransformation::default();
                for k in 0..16 {
                    t.m[k / 4][k % 4] = hex_float(w[5 + k]);
                }
                let which = match w[3] {
                    "1" => EeGeModXf::Pre,
                    "2" => EeGeModXf::Post,
                    _ => EeGeModXf::Internal,
                };
                ee_ge_mod_set_transform(model(2), which, num(4), &t);
            }
            "SetTagPoint" => {
                let frame: c_long = w[3].parse().unwrap();
                let pt = Ge3DPoint::new(hex_float(w[5]), hex_float(w[6]), hex_float(w[7]));
                ee_ge_mod_set_tag_point(model(2), frame, num(4), &pt);
            }
            "SetTagPointName" | "SetTextureFilename" => {
                let which =
                    if w[1] == "SetTagPointName" { EeGeModPtr::TagPointNames } else { EeGeModPtr::TextureFilenames };
                let array = ee_ge_mod_ptr(model(2), which) as *mut Vec<u8>;
                *array.add(num(3) as usize) = quoted(line);
            }
            "SizeOf" => emit(format!("= {}", EeGeModel::size_of(model(2)))),
            other => panic!("unknown op {other}"),
        }
    }
}

fn step() {
    emit("step".into());
    let models: Vec<usize> = with(|w| {
        w.objects.iter().filter(|(_, o)| o.alive && matches!(o.kind, Kind::Model(_))).map(|(h, _)| *h).collect()
    });
    for h in models {
        // SAFETY: a live model
        let d = unsafe { digest(h) };
        emit(format!("M {} {d:016x}", name(h)));
    }
}

/// Replays sequence lines; returns the first difference.
fn replay(lines: &[&str]) -> Option<String> {
    with(|w| *w = World::default());
    for l in lines {
        if l.starts_with("do ") {
            // SAFETY: the golden names live models
            unsafe { run(l) };
        } else if *l == "step" {
            step();
        } else if *l == "end" {
            emit("end".into());
        } else if l.starts_with("left ") {
            let n = with(|w| {
                w.objects
                    .values()
                    .filter(|o| o.alive && !matches!(o.kind, Kind::Texture | Kind::Vertices(..) | Kind::Polygons(_)))
                    .count()
            });
            emit(format!("left {n}"));
        }
    }
    let out = with(|w| std::mem::take(&mut w.out));
    for (i, l) in lines.iter().enumerate() {
        let got = out.get(i).map(String::as_str).unwrap_or("<none>");
        if got != *l {
            return Some(format!("line {i}: want [{l}] got [{got}]"));
        }
    }
    if out.len() != lines.len() {
        return Some(format!("{} lines, the golden has {}", out.len(), lines.len()));
    }
    None
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

/// A model built from counts: the arrays, the unit boxes, identity transforms, the materials on one screen more
/// than the rasterizer's id.
#[test]
fn a_counts_model_allocates_its_arrays() {
    with(|w| *w = World::default());
    let h = new_object(Kind::Model(std::ptr::null_mut()));
    let n = Box::into_raw(Box::new(b"m".to_vec()));
    // SAFETY: the mock host
    unsafe {
        let p = EeGeModel::create_with_counts(host, h, [2, 3, 1, 2, 2, 0], n as usize, 1, MODEL_MANAGER + 1);
        with(|w| w.objects.get_mut(&h).unwrap().kind = Kind::Model(p));
        assert_eq!(ee_ge_mod_get(p, EeGeModVal::FrameCount), 2);
        let b = &*(ee_ge_mod_ptr(p, EeGeModPtr::BoundingBoxes) as *const GeBoundingBox);
        assert_eq!((b.width, b.height, b.depth), (1.0, 1.0, 1.0));
        assert_eq!(
            *(ee_ge_mod_ptr(p, EeGeModPtr::InternalTransforms) as *const GeTransformation),
            GeTransformation::IDENTITY
        );
        assert_eq!(ee_ge_mod_ptr(p, EeGeModPtr::GraphicsBoundingBox), ee_ge_mod_ptr(p, EeGeModPtr::BoundingBoxes));
        assert_eq!(
            ee_ge_mod_ptr(p, EeGeModPtr::TagPoints),
            *(ee_ge_mod_ptr(p, EeGeModPtr::FrameTagPoints) as *const *mut c_void)
        );
        assert_eq!(string(ee_ge_mod_ptr(p, EeGeModPtr::Name) as usize), b"m");
        deref(h);
        drop(Box::from_raw(n));
    }
    let out = with(|w| std::mem::take(&mut w.out));
    assert_eq!(out[0], "V2 new f1 n3");
    assert_eq!(out[3], "R5 material s1 l1");
    assert!(out.iter().any(|l| l == "deref R5 0"));
    assert_eq!(out.last().unwrap(), "del R1");
}

/// Dereference: above two references one is dropped; at two or one the model asks its manager to remove it.
#[test]
fn dereference_asks_the_manager_at_the_last_outside_reference() {
    with(|w| *w = World::default());
    let h = new_object(Kind::Model(std::ptr::null_mut()));
    // SAFETY: the mock host
    unsafe {
        let p = EeGeModel::create(host, h, MODEL_MANAGER + 2);
        with(|w| {
            let o = w.objects.get_mut(&h).unwrap();
            o.kind = Kind::Model(p);
            o.refs = 3;
        });
        EeGeModel::dereference(p);
        assert_eq!(with(|w| std::mem::take(&mut w.out)), vec!["deref R1 2"]);
        EeGeModel::dereference(p);
        assert_eq!(with(|w| std::mem::take(&mut w.out)), vec!["deref R1 1", "G2 remove R1", "deref R1 0", "del R1"]);
    }
}

/// SizeOf's pointer terms are the build's pointer size; an empty model's polygon count is 0.
#[test]
fn size_of_counts_pointers_at_the_builds_size() {
    with(|w| *w = World::default());
    let h = new_object(Kind::Model(std::ptr::null_mut()));
    let n = Box::into_raw(Box::new(Vec::<u8>::new()));
    // SAFETY: the mock host
    unsafe {
        let p = EeGeModel::create_with_counts(host, h, [1, 0, 0, 0, 1, 0], n as usize, 0, MODEL_MANAGER + 1);
        let size = EeGeModel::size_of(p);
        // 0xd8 + name 13 + transforms 0x40 + LOD pointer + 4 + polygons 0x1c + 8 + vertices 0x20 + box and radius
        // 0x28 + 0x14 + 4 + frame pointer + 0x60
        let expect = 0xd8 + 13 + 0x40 + POINTER + 4 + 0x1c + 8 + 0x20 + 0x28 + 0x14 + 4 + POINTER + 0x60;
        assert_eq!(size, expect);
        assert_eq!(EeGeModel::polygon_count_at(p, 0), 0);
        EeGeModel::destroy(p);
        drop(Box::from_raw(n));
    }
}

/// SetSubModel: the old sub-model's parent is cleared and its reference dropped; the new one is referenced and
/// parented.
#[test]
fn set_sub_model_moves_the_parent() {
    with(|w| *w = World::default());
    let n = Box::into_raw(Box::new(Vec::<u8>::new()));
    // SAFETY: the mock host
    unsafe {
        let mut made = Vec::new();
        for counts in [[1, 1, 0, 0, 1, 1], [1, 1, 0, 0, 1, 0], [1, 1, 0, 0, 1, 0]] {
            let h = new_object(Kind::Model(std::ptr::null_mut()));
            let p = EeGeModel::create_with_counts(host, h, counts, n as usize, 0, MODEL_MANAGER + 1);
            with(|w| w.objects.get_mut(&h).unwrap().kind = Kind::Model(p));
            made.push((h, p));
        }
        let [(parent, pp), (first, fp), (second, sp)] = [made[0], made[1], made[2]];
        EeGeModel::set_sub_model(pp, first, 0);
        assert_eq!(ee_ge_mod_get(fp, EeGeModVal::Parent) as usize, parent);
        EeGeModel::set_sub_model(pp, second, 0);
        assert_eq!(ee_ge_mod_get(fp, EeGeModVal::Parent), 0);
        assert_eq!(ee_ge_mod_get(sp, EeGeModVal::Parent) as usize, parent);
        assert_eq!(with(|w| w.objects[&first].refs), 1);
        assert_eq!(with(|w| w.objects[&second].refs), 2);
        drop(Box::from_raw(n));
    }
}

/// PrepareTransforms scales the 3x3 of internal x pre by the graphics scale, the sub-models' too.
#[test]
fn prepare_transforms_applies_the_graphics_scale() {
    with(|w| *w = World::default());
    let n = Box::into_raw(Box::new(Vec::<u8>::new()));
    let h = new_object(Kind::Model(std::ptr::null_mut()));
    // SAFETY: the mock host
    unsafe {
        let p = EeGeModel::create_with_counts(host, h, [1, 1, 0, 0, 1, 0], n as usize, 0, MODEL_MANAGER + 1);
        ee_ge_mod_set_graphics_scale(p, 2.0);
        EeGeModel::prepare_transforms(p);
        let t = *(ee_ge_mod_ptr(p, EeGeModPtr::GraphicsTransform) as *const GeTransformation);
        assert_eq!((t.m[0][0], t.m[1][1], t.m[2][2], t.m[3][3]), (2.0, 2.0, 2.0, 1.0));
        EeGeModel::destroy(p);
        drop(Box::from_raw(n));
    }
}
