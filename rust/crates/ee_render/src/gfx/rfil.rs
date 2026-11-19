//! TEST ONLY: RFIL, the render fill's capture (tools/rfil.py; was EERenderCapture.cpp, the graphics + LLE cluster's
//! third round). A game run with `EE_RENDER_CAPTURE=<absolute file>` writes one record per filled frame: the frame's
//! keys, every render unit's and object's digest, the six texture maps, the selected and picked lists, the ghost
//! records and the ground circles, byte for byte as the C++ wrote them.
//!
//! The C++ (EERenderObject.cpp's fill, under the world's read lock) hands the lists as [`EeGfxRfilFrame`]: the pools
//! and vectors in place (their layouts in [`EeGfxRfilLayout`]), the multimaps flattened in their order, and the
//! world's key (its digest of the visible tiles' units). A pointer is named in a record by its order of first use
//! there and its content's name (a graphic object's file, a model's name, a class's type).

#![allow(clippy::unnecessary_cast)] // c_ulong and c_long are u64 and i64 on LP64

use std::collections::BTreeMap;
use std::ffi::{c_char, c_long, c_ulong, c_void, CStr};
use std::io::Write;
use std::sync::{Mutex, OnceLock};

use ee_base::math::types::GeTransformation;
use ee_sim::world::{ClassView, WorldRead};

use crate::ge::cppobj;
use crate::ge::modobj::{ee_ge_mod_ptr, EeGeModPtr, EeGeModel};
use crate::gfx::effect_reads::layout;
use crate::gfx::fill::{EeGfxFillObject, EeGfxFillUnit};

/// The record's tag ('RFIL').
pub const EE_GFX_RFIL_TAG: u32 = 0x4c49_4652;
const FNV_BASIS: u64 = 14_695_981_039_346_656_037;
const FNV_PRIME: u64 = 1_099_511_628_211;

/// A C++ array (a vector's elements or a flattened map): `count` entries from `data`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeGfxRfilSpan {
    pub data: *const c_void,
    pub count: usize,
}

/// One entry of a texture map (`std::multimap<GETexture*, entry*>`), in the map's order.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeGfxRfilPair {
    pub key: *const c_void,
    pub value: *const c_void,
}

/// A flattened texture map: `count` pairs from `data`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeGfxRfilPairs {
    pub data: *const EeGfxRfilPair,
    pub count: usize,
}

/// Where the render lists' C++ members sit (offsetof and sizeof as the C++ compiled them).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeGfxRfilLayout {
    pub size: u64,
    /// `sizeof(EERenderUnit)`; its vectors (`std::vector<float>` / `<GEModel*>`: begin, end, capacity) and `mName`
    pub unit_stride: u64,
    pub unit_health_bars: u64,
    pub unit_turret_angles: u64,
    pub unit_turret_models: u64,
    pub unit_orbital_angles: u64,
    pub unit_orbital_models: u64,
    pub unit_wheel_angles: u64,
    pub unit_wheel_models: u64,
    pub unit_name: u64,
    /// `sizeof(EERenderUnitRecord)` (its unit first), `mHeights`, `mCells`, `mTileX`, `mTileY`
    pub record_stride: u64,
    pub record_heights: u64,
    pub record_cells: u64,
    pub record_tile_x: u64,
    pub record_tile_y: u64,
    /// `sizeof(EERenderLine)`, `mPoints` (`std::vector<std::pair<GE3DPoint, float>>`), `mTexture`, `mWidth`
    pub line_stride: u64,
    pub line_points: u64,
    pub line_texture: u64,
    pub line_width: u64,
}

/// One filled frame.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeGfxRfilFrame {
    pub layout: *const EeGfxRfilLayout,
    /// the keys: the frame counter, the physics' ticks, the sway clock, the yaw, the world's transformation, the
    /// flags (free camera 1, orthographic 2, ambient 4), the pointed-at unit's id (-1: none), the world's key, the
    /// fighting sum and the unit count
    pub frame: u32,
    pub ticks: u32,
    pub sway: u32,
    pub yaw: f32,
    pub world_transformation: GeTransformation,
    pub flags: u32,
    pub picked: i32,
    pub world_key: u64,
    pub fighting: f32,
    pub unit_count: f32,
    /// the pools (`mRenderUnits` / `mRenderShadows` and their counts)
    pub units: *const c_void,
    pub units_used: usize,
    pub shadows: *const EeGfxFillObject,
    pub shadows_used: usize,
    /// `mUnits0..2`, `mShadows0..2` (of [`EeGfxRfilPair`])
    pub maps: [EeGfxRfilPairs; 6],
    /// `mSelectedUnits`, `mSelectedObjects` (of pointers)
    pub selected_units: EeGfxRfilSpan,
    pub selected_objects: EeGfxRfilSpan,
    pub selected_unit: *const c_void,
    pub selected_object: *const c_void,
    /// the labelled unit (`gU529044`) and its owner's name (`gU529030`'s text)
    pub label_unit: *const c_void,
    pub label_name: *const c_char,
    /// the rectangle's picked ids (`gU4E9230`, of `unsigned long`)
    pub picked_ids: EeGfxRfilSpan,
    /// `mGhostRecords` (of `EERenderUnitRecord`), `gU529050` (of `EERenderLine`)
    pub ghosts: EeGfxRfilSpan,
    pub lines: EeGfxRfilSpan,
}

// ---- the record (no C++ object)

/// FNV-1a over `data` from `hash`.
pub fn fnv(data: &[u8], mut hash: u64) -> u64 {
    for &b in data {
        hash ^= b as u64;
        hash = hash.wrapping_mul(FNV_PRIME);
    }
    hash
}

/// A text's 32-bit name (0 for none).
pub fn text32(text: Option<&[u8]>) -> u32 {
    text.map_or(0, |t| fnv(t, FNV_BASIS) as u32)
}

/// The record being written and its pointers' order of first use.
#[derive(Default)]
pub struct Writer {
    pub data: Vec<u8>,
    order: BTreeMap<usize, u32>,
}

impl Writer {
    pub fn bytes(&mut self, b: &[u8]) {
        self.data.extend_from_slice(b);
    }
    pub fn u32(&mut self, v: u32) {
        self.bytes(&v.to_le_bytes());
    }
    pub fn i32(&mut self, v: i32) {
        self.bytes(&v.to_le_bytes());
    }
    pub fn f32(&mut self, v: f32) {
        self.bytes(&v.to_le_bytes());
    }
    pub fn u64(&mut self, v: u64) {
        self.bytes(&v.to_le_bytes());
    }
    /// The pointer's order of first use in this record (0: NULL).
    pub fn order(&mut self, p: usize) -> u32 {
        if p == 0 {
            return 0;
        }
        let next = self.order.len() as u32 + 1;
        *self.order.entry(p).or_insert(next)
    }
}

/// A digest of fields in their order.
pub struct Digest<'a> {
    pub hash: u64,
    writer: &'a mut Writer,
}

impl<'a> Digest<'a> {
    pub fn new(writer: &'a mut Writer) -> Self {
        Digest { hash: FNV_BASIS, writer }
    }
    pub fn bytes(&mut self, b: &[u8]) {
        self.hash = fnv(b, self.hash);
    }
    pub fn u32(&mut self, v: u32) {
        self.bytes(&v.to_le_bytes());
    }
    pub fn f32(&mut self, v: f32) {
        self.bytes(&v.to_le_bytes());
    }
    pub fn floats(&mut self, v: &[f32]) {
        for f in v {
            self.f32(*f);
        }
    }
    /// A pointer: its order, then its name (0 for NULL).
    pub fn pointer(&mut self, p: usize, name: u32) {
        let order = self.writer.order(p);
        self.u32(order);
        self.u32(if p != 0 { name } else { 0 });
    }
    /// A text: its byte count, then its bytes.
    pub fn text(&mut self, text: Option<&[u8]>) {
        self.u32(text.map_or(0, |t| t.len() as u32));
        if let Some(t) = text {
            self.bytes(t);
        }
    }
}

/// An entry's pool index: -1 for NULL, -2 outside the pool.
pub fn pool_index(entry: usize, pool: usize, stride: usize, count: usize) -> i32 {
    if entry == 0 {
        return -1;
    }
    if entry < pool || entry >= pool + stride * count {
        return -2;
    }
    ((entry - pool) / stride) as i32
}

/// A texture map: its size, then (the key's name, the entry's pool index) by key. The map orders its keys by address,
/// which differs between runs: a key is named by its lowest pool index among its entries, and the keys are written
/// in that order (a stable sort: each key's entries keep the map's order).
pub fn texture_map(w: &mut Writer, entries: &[(usize, usize)], pool: usize, stride: usize, count: usize) {
    let mut names: BTreeMap<usize, i32> = BTreeMap::new();
    let indices: Vec<i32> = entries.iter().map(|&(_, v)| pool_index(v, pool, stride, count)).collect();
    for (&(key, _), &index) in entries.iter().zip(&indices) {
        names.entry(key).and_modify(|n| *n = (*n).min(index)).or_insert(index);
    }
    let mut out: Vec<(i32, i32)> =
        entries.iter().zip(&indices).map(|(&(key, _), &index)| (names[&key], index)).collect();
    out.sort_by_key(|e| e.0);
    w.u32(out.len() as u32);
    for (name, index) in out {
        w.i32(name);
        w.i32(index);
    }
}

// ---- the C++ objects

fn rd<T: Copy>(base: usize, off: u64) -> T {
    // SAFETY: a live object's member (the frame's contract)
    unsafe { std::ptr::read_unaligned((base + off as usize) as *const T) }
}

/// A `std::vector<T>`'s elements (begin, end).
fn vector<T: Copy>(at: usize) -> Vec<T> {
    let begin: usize = rd(at, 0);
    let end: usize = rd(at, std::mem::size_of::<usize>() as u64);
    let n = (end - begin) / std::mem::size_of::<T>();
    (0..n).map(|i| rd(begin, (i * std::mem::size_of::<T>()) as u64)).collect()
}

fn span<T: Copy>(s: EeGfxRfilSpan) -> Vec<T> {
    if s.data.is_null() || s.count == 0 {
        return Vec::new();
    }
    // SAFETY: the frame's spans
    unsafe { std::slice::from_raw_parts(s.data as *const T, s.count) }.to_vec()
}

fn c_text<'a>(p: *const c_char) -> Option<&'a [u8]> {
    // SAFETY: a NUL-terminated text or NULL
    (!p.is_null()).then(|| unsafe { CStr::from_ptr(p) }.to_bytes())
}

/// A graphic object's name: its file's (0 without one).
fn object_name(object: usize) -> u32 {
    if object == 0 {
        return 0;
    }
    let file: usize = rd(object, layout().gobj_file_name);
    if file == 0 {
        return 0;
    }
    // SAFETY: a live UString
    text32(c_text(unsafe { cppobj::str_text(file) }))
}

fn model_name(model: usize) -> u32 {
    if model == 0 {
        return 0;
    }
    // SAFETY: a live model's name
    text32(c_text(unsafe {
        cppobj::str_text(ee_ge_mod_ptr(cppobj::counted::<EeGeModel>(model), EeGeModPtr::Name) as usize)
    }))
}

fn class_name(t: &WorldRead, class: usize) -> u32 {
    // SAFETY: a live class (the fill's)
    unsafe { ClassView::of(t, class as u64) }.map_or(0, |c| c.unit_type() as u32)
}

fn floats_bytes(v: &[f32]) -> Vec<u8> {
    v.iter().flat_map(|f| f.to_le_bytes()).collect()
}

fn transformation_bytes(t: &GeTransformation) -> Vec<u8> {
    floats_bytes(&t.m.concat())
}

struct Ctx<'a> {
    t: &'a WorldRead,
    l: &'a EeGfxRfilLayout,
}

impl Ctx<'_> {
    fn float_vector(&self, d: &mut Digest<'_>, at: usize) {
        let v: Vec<f32> = vector(at);
        d.u32(v.len() as u32);
        d.floats(&v);
    }
    fn model_vector(&self, d: &mut Digest<'_>, at: usize) {
        let v: Vec<usize> = vector(at);
        d.u32(v.len() as u32);
        for m in v {
            d.pointer(m, model_name(m));
        }
    }
    fn unit_digest(&self, w: &mut Writer, unit: usize) -> u64 {
        let l = self.l;
        let u: EeGfxFillUnit = rd(unit, 0);
        let mut d = Digest::new(w);
        d.bytes(&transformation_bytes(&u.transformation));
        d.floats(&u.player_color);
        d.floats(&u.tint_color);
        self.float_vector(&mut d, unit + l.unit_health_bars as usize);
        self.float_vector(&mut d, unit + l.unit_turret_angles as usize);
        self.model_vector(&mut d, unit + l.unit_turret_models as usize);
        self.float_vector(&mut d, unit + l.unit_orbital_angles as usize);
        self.model_vector(&mut d, unit + l.unit_orbital_models as usize);
        self.float_vector(&mut d, unit + l.unit_wheel_angles as usize);
        self.model_vector(&mut d, unit + l.unit_wheel_models as usize);
        let name = unit + l.unit_name as usize;
        // SAFETY: the render unit's live UWideString
        let (text, bytes) = unsafe { (cppobj::wstr_text(name), cppobj::wstr_bytes(name) as usize) };
        d.u32(bytes as u32);
        if !text.is_null() {
            // SAFETY: the string's bytes
            d.bytes(unsafe { std::slice::from_raw_parts(text as *const u8, bytes) });
        }
        d.pointer(u.info as usize, class_name(self.t, u.info as usize));
        d.pointer(u.source as usize, object_name(u.source as usize));
        d.pointer(u.source2 as usize, object_name(u.source2 as usize));
        for f in [
            u.scale,
            u.selection_size,
            u.shadow_scale,
            u.ff4,
            u.ground_z,
            u.opacity,
            u.health,
            u.fraction,
            u.sub_model_offset_scale,
            u.fog_scale,
        ] {
            d.f32(f);
        }
        for v in [
            u.category as u32,
            u.unit_class_index as u32,
            u.l118 as u32,
            u.command_type as u32,
            u.current_research as u32,
            u.animation as u32,
            u.grid_size as u32,
            u.current_frame as u32,
            u.next_frame as u32,
            u.l134 as u32,
            u.unit_id as u32,
            u.player_id as u32,
            u.area_effect_strength as u32,
        ] {
            d.u32(v);
        }
        d.u32(
            u.can_place as u32
                | (u.is_fogged as u32) << 1
                | (u.is_tinted as u32) << 2
                | (u.has_sub_model_offset as u32) << 3
                | (u.second_full_opacity as u32) << 4,
        );
        d.hash
    }
    /// The ghost record's fields FillGhostRecords sets (the record is a local: its other scalars are indeterminate),
    /// the grids and the tile.
    fn ghost_digest(&self, w: &mut Writer, record: usize) -> u64 {
        let l = self.l;
        let u: EeGfxFillUnit = rd(record, 0);
        let mut d = Digest::new(w);
        d.bytes(&transformation_bytes(&u.transformation));
        d.floats(&u.player_color);
        d.pointer(u.info as usize, class_name(self.t, u.info as usize));
        d.pointer(u.source as usize, object_name(u.source as usize));
        d.pointer(u.source2 as usize, object_name(u.source2 as usize));
        d.f32(u.scale);
        d.f32(u.fraction);
        for v in
            [u.category as u32, u.animation as u32, u.grid_size as u32, u.current_frame as u32, u.next_frame as u32]
        {
            d.u32(v);
        }
        d.u32(
            u.can_place as u32
                | (u.is_fogged as u32) << 1
                | (u.is_tinted as u32) << 2
                | (u.has_sub_model_offset as u32) << 3,
        );
        let heights: [f32; 144] = rd(record, l.record_heights);
        d.floats(&heights);
        let cells: [u8; 144] = rd(record, l.record_cells);
        d.bytes(&cells);
        d.u32(rd::<c_long>(record, l.record_tile_x) as u32);
        d.u32(rd::<c_long>(record, l.record_tile_y) as u32);
        d.hash
    }
    fn line_digest(&self, w: &mut Writer, line: usize) -> u64 {
        let l = self.l;
        let points: Vec<[f32; 4]> = vector(line + l.line_points as usize);
        let mut d = Digest::new(w);
        d.u32(points.len() as u32);
        for p in points {
            d.floats(&p);
        }
        let texture: usize = rd(line, l.line_texture);
        d.pointer(texture, 0);
        d.f32(rd(line, l.line_width));
        d.hash
    }
}

fn shadow_digest(w: &mut Writer, s: &EeGfxFillObject) -> u64 {
    let mut d = Digest::new(w);
    d.floats(&s.location);
    d.pointer(s.object as usize, object_name(s.object as usize));
    for f in [s.model_scale, s.selection_size, s.scale, s.f1c, s.health, s.opacity] {
        d.f32(f);
    }
    d.u32(s.model_index as u32);
    d.u32(s.current_frame as u32);
    d.u32(s.next_frame as u32);
    d.u32(s.is_fogged as u32);
    d.hash
}

/// The frame's record (its bytes after the tag and length).
///
/// # Safety
/// The frame's pointers are live (the fill's lists, under the world's read lock).
pub unsafe fn record(f: &EeGfxRfilFrame) -> Vec<u8> {
    // SAFETY: the caller's contract
    let l = unsafe { &*f.layout };
    assert_eq!(l.size, std::mem::size_of::<EeGfxRfilLayout>() as u64, "ee_gfx: a stale EeGfxRfilLayout header");
    // SAFETY: inside the fill's call (the world's read lock held)
    let t = unsafe { WorldRead::begin() };
    let c = Ctx { t: &t, l };
    let unit_stride = l.unit_stride as usize;
    let shadow_stride = std::mem::size_of::<EeGfxFillObject>();
    let (units, shadows) = (f.units as usize, f.shadows as usize);
    let mut w = Writer::default();
    w.u32(f.frame);
    w.u32(f.ticks);
    w.u32(f.sway);
    w.f32(f.yaw);
    w.u64(fnv(&transformation_bytes(&f.world_transformation), FNV_BASIS));
    w.u32(f.flags);
    w.i32(f.picked);
    w.u64(f.world_key);
    w.f32(f.fighting);
    w.f32(f.unit_count);
    w.u32(f.units_used as u32);
    for i in 0..f.units_used {
        let u = units + i * unit_stride;
        let head: EeGfxFillUnit = rd(u, 0);
        w.u32(head.unit_id as u32);
        w.f32(head.opacity);
        let digest = c.unit_digest(&mut w, u);
        w.u64(digest);
    }
    w.u32(f.shadows_used as u32);
    for i in 0..f.shadows_used {
        let s: EeGfxFillObject = rd(shadows, (i * shadow_stride) as u64);
        w.f32(s.opacity);
        w.f32(s.fraction);
        let digest = shadow_digest(&mut w, &s);
        w.u64(digest);
    }
    for (k, map) in f.maps.iter().enumerate() {
        let entries: Vec<(usize, usize)> =
            span::<EeGfxRfilPair>(EeGfxRfilSpan { data: map.data as *const c_void, count: map.count })
                .iter()
                .map(|p| (p.key as usize, p.value as usize))
                .collect();
        if k < 3 {
            texture_map(&mut w, &entries, units, unit_stride, f.units_used);
        } else {
            texture_map(&mut w, &entries, shadows, shadow_stride, f.shadows_used);
        }
    }
    let selected: Vec<usize> = span(f.selected_units);
    w.u32(selected.len() as u32);
    for u in selected {
        w.i32(pool_index(u, units, unit_stride, f.units_used));
    }
    let selected: Vec<usize> = span(f.selected_objects);
    w.u32(selected.len() as u32);
    for s in selected {
        w.i32(pool_index(s, shadows, shadow_stride, f.shadows_used));
    }
    w.i32(pool_index(f.selected_unit as usize, units, unit_stride, f.units_used));
    w.i32(pool_index(f.selected_object as usize, shadows, shadow_stride, f.shadows_used));
    w.i32(pool_index(f.label_unit as usize, units, unit_stride, f.units_used));
    w.u32(if f.label_unit.is_null() { 0 } else { text32(c_text(f.label_name)) });
    let ids: Vec<c_ulong> = span(f.picked_ids);
    w.u32(ids.len() as u32);
    for id in ids {
        w.u32(id as u32);
    }
    w.u32(f.ghosts.count as u32);
    for i in 0..f.ghosts.count {
        let digest = c.ghost_digest(&mut w, f.ghosts.data as usize + i * l.record_stride as usize);
        w.u64(digest);
    }
    w.u32(f.lines.count as u32);
    for i in 0..f.lines.count {
        let digest = c.line_digest(&mut w, f.lines.data as usize + i * l.line_stride as usize);
        w.u64(digest);
    }
    w.data
}

/// The capture file, opened at the first fill (`EE_RENDER_CAPTURE`, absolute: the game changes into its data root
/// first); None without it.
fn file() -> &'static Mutex<Option<std::fs::File>> {
    static FILE: OnceLock<Mutex<Option<std::fs::File>>> = OnceLock::new();
    FILE.get_or_init(|| {
        let path = std::env::var_os("EE_RENDER_CAPTURE").filter(|p| !p.is_empty());
        Mutex::new(path.and_then(|p| std::fs::File::create(p).ok()))
    })
}

/// Whether the capture is on (the C++ builds no frame otherwise).
#[no_mangle]
pub extern "C" fn ee_gfx_rfil_active() -> bool {
    file().lock().unwrap().is_some()
}

/// Writes the frame's record (tag, length, bytes) and flushes it.
///
/// # Safety
/// As [`record`].
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_rfil_write(frame: *const EeGfxRfilFrame) {
    let mut file = file().lock().unwrap();
    let Some(out) = file.as_mut() else { return };
    // SAFETY: the caller's contract
    let data = unsafe { record(&*frame) };
    let mut bytes = Vec::with_capacity(data.len() + 8);
    bytes.extend_from_slice(&EE_GFX_RFIL_TAG.to_le_bytes());
    bytes.extend_from_slice(&(data.len() as u32).to_le_bytes());
    bytes.extend_from_slice(&data);
    let _ = out.write_all(&bytes);
    let _ = out.flush();
}

#[cfg(test)]
#[path = "rfil_tests.rs"]
mod tests;
