//! The unit picker's walks over its entries (`EEUnitPicker.cpp`): `FindUnit` (the screen ray against every drawn
//! unit's model, the hits' exact retest and ranking, the chosen unit's carrier), the box selection's
//! `PickUnitsOfType`, `PickUnits` and `PickUnitsInRectangle` (the filters, the carriers, the units before the
//! buildings) and their frustum test `GetIsEntryInView` / `GetIsModelInView` (the bounding sphere, then the vertices
//! of a coarse LOD, the sub-models). The decisions they make per hit are `pick.rs`'s.
//!
//! The entries are the C++'s array of `EEUnitPickerEntry` (a `GEPhysicalModel` and the unit's id), read and written
//! in place through their `#[repr(C)]` mirror [`EeGfxPickEntry`] (the pick scale is set and put back, as the C++
//! did). The models are read through ee_ge's physical model table (`GEModel.cpp`'s, the same callbacks its
//! line walk uses); the units, the rasterizer's pixel size and the results through [`EeGfxPickHost`]. The C++ keeps
//! the lock, the viewport's frustum planes and the entries' filling (`AddUnit`).
//!
//! This is the UI's (every click and box selection), not the simulation's.

use std::ffi::{c_ulong, c_void};

use crate::ge::physical_walk::{ee_ge_physical_line_walk, EeGePhysicalModels, GePhysicalMaterial};
use crate::ge::viewport::GeViewport;
use ee_base::math::types::{Ge2DPoint, Ge3DLine, Ge3DPlane, Ge3DPoint, GeTransformation, GeVector};

use crate::gfx::pick::{self, EeGfxPickHit, EeGfxPickQuestion, EeGfxPickUnit};

/// `EEUnitPickerEntry` (0x50 bytes in the original): its `GEPhysicalModel` (location, model, scale, frame) and the
/// unit's id.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGfxPickEntry {
    pub location: GeTransformation,
    /// `GEModel*`
    pub model: *const c_void,
    pub scale: f32,
    pub frame: c_ulong,
    pub unit_id: c_ulong,
}

/// What `FindUnit` reads of an entry's unit.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeGfxPickFindUnit {
    /// `GetUnitPickPriority`'s facts
    pub facts: EeGfxPickUnit,
    /// `GetTileX()` (-10000: off the map)
    pub tile_x: i32,
}

/// What the box selection reads of an entry's unit (its own class; a part's carrier's id and class).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeGfxPickBoxUnit {
    /// `GetComplexUnit() != NULL`
    pub complex: bool,
    /// the class's `IsPickable()`
    pub pickable: bool,
    /// `mHitPoints > 0`
    pub alive: bool,
    pub category: u32,
    pub unit_type: u32,
    /// `mPlayer` (an `EEPlayer*`, compared only)
    pub player: *const c_void,
    /// the outermost carrier's (a complex unit's `mPreviousPart` walked; else the unit's own) id and whether its
    /// class is a land building
    pub outer_id: u32,
    pub outer_land_building: bool,
}

/// The C++'s answers (`context`: the picker).
#[repr(C)]
pub struct EeGfxPickHost {
    pub context: *mut c_void,
    /// entry `index`'s unit (`gWorld.GetUnit` of its id), null once it has left the world
    pub unit: extern "C" fn(context: *mut c_void, index: u32) -> *mut c_void,
    pub find_unit: extern "C" fn(context: *mut c_void, unit: *mut c_void) -> EeGfxPickFindUnit,
    /// `GetUnitPickPriority`'s questions
    pub ask: extern "C" fn(context: *mut c_void, unit: *mut c_void, question: u32) -> bool,
    /// the current player's selection holds a unit
    pub group_nonempty: extern "C" fn(context: *mut c_void) -> bool,
    /// `gRasterizer->GetPixelSize(entry, viewport, orthographic)` of entry `index`
    pub pixel_size: extern "C" fn(context: *mut c_void, index: u32) -> f32,
    /// a unit's outermost carrier (a complex unit's `mPreviousPart` walked), or the unit
    pub carrier: extern "C" fn(context: *mut c_void, unit: *mut c_void) -> *mut c_void,
    pub box_unit: extern "C" fn(context: *mut c_void, unit: *mut c_void) -> EeGfxPickBoxUnit,
    /// an id onto the caller's result
    pub push: extern "C" fn(context: *mut c_void, out: *mut c_void, id: u32),
}

/// The picker's questions (the tests' log).
pub trait PickHost {
    fn unit(&mut self, index: u32) -> *mut c_void;
    fn find_unit(&mut self, unit: *mut c_void) -> EeGfxPickFindUnit;
    fn ask(&mut self, unit: *mut c_void, question: EeGfxPickQuestion) -> bool;
    fn group_nonempty(&mut self) -> bool;
    fn pixel_size(&mut self, index: u32) -> f32;
    fn carrier(&mut self, unit: *mut c_void) -> *mut c_void;
    fn box_unit(&mut self, unit: *mut c_void) -> EeGfxPickBoxUnit;
    fn push(&mut self, out: *mut c_void, id: u32);
}

impl PickHost for &EeGfxPickHost {
    fn unit(&mut self, index: u32) -> *mut c_void {
        (self.unit)(self.context, index)
    }
    fn find_unit(&mut self, unit: *mut c_void) -> EeGfxPickFindUnit {
        (self.find_unit)(self.context, unit)
    }
    fn ask(&mut self, unit: *mut c_void, question: EeGfxPickQuestion) -> bool {
        (self.ask)(self.context, unit, question as u32)
    }
    fn group_nonempty(&mut self) -> bool {
        (self.group_nonempty)(self.context)
    }
    fn pixel_size(&mut self, index: u32) -> f32 {
        (self.pixel_size)(self.context, index)
    }
    fn carrier(&mut self, unit: *mut c_void) -> *mut c_void {
        (self.carrier)(self.context, unit)
    }
    fn box_unit(&mut self, unit: *mut c_void) -> EeGfxPickBoxUnit {
        (self.box_unit)(self.context, unit)
    }
    fn push(&mut self, out: *mut c_void, id: u32) {
        (self.push)(self.context, out, id)
    }
}

/// What the walks read of the models: the line test (`GEPhysicalModel::Intersects(line, lod, distance, false)`) and
/// the frustum test's model accessors.
pub trait PickModels {
    fn intersects(&self, entry: &EeGfxPickEntry, line: &Ge3DLine, lod: c_ulong, distance: &mut f32) -> bool;
    fn has_parent(&self, m: *const c_void) -> bool;
    fn internal_transform(&self, m: *const c_void, frame: c_ulong) -> GeTransformation;
    fn center(&self, m: *const c_void) -> Ge3DPoint;
    fn radius(&self, m: *const c_void, frame: c_ulong) -> f32;
    fn lod_count(&self, m: *const c_void) -> c_ulong;
    fn material_count(&self, m: *const c_void) -> c_ulong;
    fn material(&self, m: *const c_void, index: c_ulong, lod: c_ulong) -> GePhysicalMaterial;
    fn triangle(
        &self,
        m: *const c_void,
        frame: c_ulong,
        lod: c_ulong,
        polygon: c_ulong,
        start: c_ulong,
    ) -> [Ge3DPoint; 3];
    fn sub_model_count(&self, m: *const c_void) -> c_ulong;
    fn sub_model(&self, m: *const c_void, index: c_ulong) -> *const c_void;
}

/// The C++'s table (`GEModel.cpp`'s).
struct Table<'a>(&'a EeGePhysicalModels);

// SAFETY (every method): the table's callbacks take the live `GEModel*` the entries hold or the table returned
impl PickModels for Table<'_> {
    fn intersects(&self, entry: &EeGfxPickEntry, line: &Ge3DLine, lod: c_ulong, distance: &mut f32) -> bool {
        let (location, line) = (entry.location, *line);
        unsafe {
            ee_ge_physical_line_walk(
                self.0,
                entry.scale,
                &location,
                entry.model,
                entry.frame,
                &line,
                lod,
                distance,
                false,
            )
        }
    }
    fn has_parent(&self, m: *const c_void) -> bool {
        unsafe { (self.0.has_parent)(m) }
    }
    fn internal_transform(&self, m: *const c_void, frame: c_ulong) -> GeTransformation {
        let mut out = GeTransformation::default();
        unsafe { (self.0.internal_transform)(m, frame, &mut out) };
        out
    }
    fn center(&self, m: *const c_void) -> Ge3DPoint {
        let mut out = Ge3DPoint::default();
        unsafe { (self.0.center)(m, &mut out) };
        out
    }
    fn radius(&self, m: *const c_void, frame: c_ulong) -> f32 {
        unsafe { (self.0.radius)(m, frame) }
    }
    fn lod_count(&self, m: *const c_void) -> c_ulong {
        unsafe { (self.0.lod_count)(m) }
    }
    fn material_count(&self, m: *const c_void) -> c_ulong {
        unsafe { (self.0.material_count)(m) }
    }
    fn material(&self, m: *const c_void, index: c_ulong, lod: c_ulong) -> GePhysicalMaterial {
        let mut out = GePhysicalMaterial::default();
        unsafe { (self.0.material)(m, index, lod, &mut out) };
        out
    }
    fn triangle(
        &self,
        m: *const c_void,
        frame: c_ulong,
        lod: c_ulong,
        polygon: c_ulong,
        start: c_ulong,
    ) -> [Ge3DPoint; 3] {
        let mut out = [Ge3DPoint::default(); 3];
        unsafe { (self.0.triangle)(m, frame, lod, polygon, start, out.as_mut_ptr()) };
        out
    }
    fn sub_model_count(&self, m: *const c_void) -> c_ulong {
        unsafe { (self.0.sub_model_count)(m) }
    }
    fn sub_model(&self, m: *const c_void, index: c_ulong) -> *const c_void {
        unsafe { (self.0.sub_model)(m, index) }
    }
}

/// `GetIsModelInView`: a vertex of the model at `lod` (the polygons' corners in order, the materials in order) on or
/// above every side plane, else one of its sub-models'; a sub-model's transformation is its parent's with its
/// internal transform.
fn model_in_view<M: PickModels>(
    a: &M,
    model: *const c_void,
    transformation: &GeTransformation,
    frame: c_ulong,
    lod: c_ulong,
    planes: &[Ge3DPlane; 4],
) -> bool {
    let t = if !a.has_parent(model) {
        *transformation
    } else {
        transformation.compose(&a.internal_transform(model, frame))
    };
    let count = a.material_count(model);
    let mut inside = false;
    let mut i: c_ulong = 0;
    while i < count && !inside {
        let m = a.material(model, i, lod);
        let end = m.polygon_length.wrapping_add(m.polygon_start);
        let mut j = m.polygon_start;
        while j < end && !inside {
            let corners = a.triangle(model, frame, lod, j, m.vertex_start);
            for c in corners {
                inside = pick::point_in_view(planes, t.apply_point(c));
                if inside {
                    break;
                }
            }
            j = j.wrapping_add(1);
        }
        i += 1;
    }
    if !inside {
        let subs = a.sub_model_count(model);
        let mut k: c_ulong = 0;
        while k < subs && !inside {
            inside = model_in_view(a, a.sub_model(model, k), &t, frame, lod, planes);
            k += 1;
        }
    }
    inside
}

/// `GetIsEntryInView`: whether an entry's model lies inside the side planes 2..5 of a selection frustum: its bounding
/// sphere's centre above them all, or the sphere reaching every plane it is below and a vertex inside.
pub fn entry_in_view<M: PickModels>(a: &M, entry: &EeGfxPickEntry, planes: &[Ge3DPlane; 4]) -> bool {
    let model = entry.model;
    let t = pick::entry_transform(&entry.location, &a.internal_transform(model, entry.frame), entry.scale);
    let center = t.apply_point(a.center(model));
    let below = pick::sphere_below(center, planes);
    if !below.iter().any(|b| *b) {
        return true;
    }
    let radius = a.radius(model, entry.frame) * entry.scale;
    if pick::sphere_outside(center, planes, &below, radius) != 0 {
        return false;
    }
    #[allow(clippy::unnecessary_cast)] // c_ulong is u64 on LP64
    let lod = pick::view_lod(a.lod_count(model) as u32) as c_ulong;
    model_in_view(a, model, &t, entry.frame, lod, planes)
}

/// The view's facts `FindUnit` reads (`mViewport`, `mOrthographic`, `mShowHiddenUnits`, `gFreeCamera`, the window).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGfxPickView {
    pub viewport: GeViewport,
    pub orthographic: bool,
    pub show_hidden: bool,
    pub free_camera: bool,
    pub window_width: u32,
}

fn priority<H: PickHost>(h: &mut H, unit: *mut c_void, facts: &EeGfxPickUnit, prefer: bool) -> i32 {
    pick::priority(facts, prefer, |q| h.ask(unit, q))
}

/// `FindUnit`: the unit under `point` (its outermost carrier), or null. The entries walked from the last: a bounding
/// test (LOD 1) at the pick scale, then each hit retested exactly (LOD 2; a tree at its own scale) or measured on the
/// screen; the ranking and choice are `pick.rs`'s.
pub fn find_unit<H: PickHost, M: PickModels>(
    h: &mut H,
    a: &M,
    entries: &mut [EeGfxPickEntry],
    view: &EeGfxPickView,
    point: Ge2DPoint,
    prefer_resources: bool,
) -> *mut c_void {
    let tolerances = pick::tolerances(&view.viewport, view.free_camera, view.window_width);
    if entries.is_empty() {
        return std::ptr::null_mut();
    }
    let ray = pick::screen_ray(&view.viewport, view.orthographic, point);
    let line = Ge3DLine {
        start: ray.front,
        direction: GeVector { x: ray.rear.x - ray.front.x, y: ray.rear.y - ray.front.y, z: ray.rear.z - ray.front.z },
    };
    let mut hits: Vec<EeGfxPickHit> = Vec::new();
    let mut units: Vec<*mut c_void> = Vec::new();
    for i in (0..entries.len()).rev() {
        let unit = h.unit(i as u32);
        if unit.is_null() {
            continue;
        }
        let saved = entries[i].scale;
        let facts = h.find_unit(unit);
        let kind = priority(h, unit, &facts.facts, false);
        entries[i].scale = pick::pick_scale(kind, saved);
        let mut distance = 0.0f32;
        if facts.tile_x != -10000 && a.intersects(&entries[i], &line, 1, &mut distance) {
            let priority = priority(h, unit, &facts.facts, prefer_resources);
            hits.push(EeGfxPickHit {
                distance,
                priority: priority as u32,
                kind,
                index: i as u32,
                exact: false,
                large: false,
            });
            units.push(unit);
        }
        entries[i].scale = saved;
    }
    for i in (0..hits.len()).rev() {
        let e = hits[i].index as usize;
        let saved = entries[e].scale;
        if hits[i].kind == 2 {
            entries[e].scale = pick::pick_scale(2, saved);
        }
        let mut distance = 0.0f32;
        if a.intersects(&entries[e], &line, 2, &mut distance) {
            hits[i].distance = distance;
            hits[i].exact = true;
        } else {
            let pixels = h.pixel_size(e as u32);
            hits[i].large = pick::is_large(pixels, tolerances.pixel, hits[i].kind);
        }
        if hits[i].kind == 2 {
            entries[e].scale = saved;
        }
    }
    let (first, second) = pick::rank(&hits, tolerances.tolerance, view.show_hidden);
    match pick::choose(&hits, first, second, || h.group_nonempty()) {
        // the hit's unit as the entry's id gives it now (the same object: no call in between frees it)
        Some(best) => h.carrier(units[best]),
        None => std::ptr::null_mut(),
    }
}

/// The unit categories and types the box selection leaves out or matches (`EEUnitClassIDs.h`).
const CATEGORY_PROJECTILE: u32 = 9;
const CATEGORY_BOMB: u32 = 0xb;
const CATEGORY_MISSILE: u32 = 0xc;
const CATEGORY_TORPEDO: u32 = 0x14;
const TYPE_CITIZEN: u32 = 0x20;
const TYPE_FEMALE_CITIZEN: u32 = 0x1e0;

/// Projectiles, bombs and missiles but the ICBM (and with `torpedoes` the torpedoes) are never selected.
fn is_shot(u: &EeGfxPickBoxUnit, torpedoes: bool) -> bool {
    u.category == CATEGORY_PROJECTILE
        || u.category == CATEGORY_BOMB
        || (u.category == CATEGORY_MISSILE && u.unit_type != pick::TYPE_CP_ICBM)
        || (torpedoes && u.category == CATEGORY_TORPEDO)
}

/// `PickUnitsOfType`: a player's (any player's for null) living pickable complex units of a type (any for 0) inside
/// the frustum, their outermost carriers' ids; with no type the units, or failing those one land building; with a
/// type the units, then the land buildings.
#[allow(clippy::too_many_arguments)]
pub fn pick_units_of_type<H: PickHost, M: PickModels>(
    h: &mut H,
    a: &M,
    entries: &[EeGfxPickEntry],
    planes: Option<&[Ge3DPlane; 4]>,
    player: *const c_void,
    kind: u32,
    out: *mut c_void,
) {
    let mut units = Vec::new();
    let mut others = Vec::new();
    if let Some(planes) = planes {
        for i in (0..entries.len()).rev() {
            let unit = h.unit(i as u32);
            if unit.is_null() {
                continue;
            }
            let u = h.box_unit(unit);
            if !u.complex
                || !u.pickable
                || !u.alive
                || is_shot(&u, true)
                || (!player.is_null() && u.player != player)
                || (kind != 0 && u.unit_type != kind)
                || !entry_in_view(a, &entries[i], planes)
            {
                continue;
            }
            if !u.outer_land_building {
                units.push(u.outer_id);
            } else {
                others.push(u.outer_id);
            }
        }
    }
    if kind == 0 {
        if !units.is_empty() {
            for id in units {
                h.push(out, id);
            }
        } else if let Some(first) = others.first() {
            h.push(out, *first);
        }
    } else {
        for id in units.into_iter().chain(others) {
            h.push(out, id);
        }
    }
}

/// `PickUnits`: either citizen type selects both (the male ones first).
#[allow(clippy::too_many_arguments)]
pub fn pick_units<H: PickHost, M: PickModels>(
    h: &mut H,
    a: &M,
    entries: &[EeGfxPickEntry],
    planes: Option<&[Ge3DPlane; 4]>,
    player: *const c_void,
    kind: u32,
    out: *mut c_void,
) {
    if kind == TYPE_CITIZEN || kind == TYPE_FEMALE_CITIZEN {
        pick_units_of_type(h, a, entries, planes, player, TYPE_CITIZEN, out);
        pick_units_of_type(h, a, entries, planes, player, TYPE_FEMALE_CITIZEN, out);
    } else {
        pick_units_of_type(h, a, entries, planes, player, kind, out);
    }
}

/// `PickUnitsInRectangle` (the editor's lasso): every pickable unit (no shot) of a type (either citizen type for a
/// citizen type; any for 0) inside the frustum, its outermost carrier's id.
pub fn pick_units_in_rectangle<H: PickHost, M: PickModels>(
    h: &mut H,
    a: &M,
    entries: &[EeGfxPickEntry],
    planes: Option<&[Ge3DPlane; 4]>,
    kind: u32,
    out: *mut c_void,
) {
    let Some(planes) = planes else { return };
    for i in (0..entries.len()).rev() {
        let unit = h.unit(i as u32);
        if unit.is_null() {
            continue;
        }
        let u = h.box_unit(unit);
        if !u.pickable {
            continue;
        }
        if is_shot(&u, false)
            || (kind != 0
                && u.unit_type != kind
                && (kind != TYPE_CITIZEN || u.unit_type != TYPE_FEMALE_CITIZEN)
                && (kind != TYPE_FEMALE_CITIZEN || u.unit_type != TYPE_CITIZEN))
            || !entry_in_view(a, &entries[i], planes)
        {
            continue;
        }
        h.push(out, u.outer_id);
    }
}

/// # Safety
/// `entries` holds `count` entries (or `count` is 0).
unsafe fn entries_mut<'a>(entries: *mut EeGfxPickEntry, count: u32) -> &'a mut [EeGfxPickEntry] {
    if count == 0 {
        &mut []
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts_mut(entries, count as usize) }
    }
}

/// # Safety
/// `planes` is null or holds the frustum's six planes.
unsafe fn side_planes(planes: *const Ge3DPlane) -> Option<[Ge3DPlane; 4]> {
    // SAFETY: the caller's contract
    (!planes.is_null()).then(|| unsafe { [*planes.add(2), *planes.add(3), *planes.add(4), *planes.add(5)] })
}

/// `FindUnit` for the C++ (under its lock).
///
/// # Safety
/// `host`, `models`, `view` and `point` are valid; `entries` holds `count` entries the callbacks do not touch.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_pick_find_unit(
    host: *const EeGfxPickHost,
    models: *const EeGePhysicalModels,
    entries: *mut EeGfxPickEntry,
    count: u32,
    view: *const EeGfxPickView,
    point: *const Ge2DPoint,
    prefer_resources: bool,
) -> *mut c_void {
    // SAFETY: the function's contract
    unsafe {
        let view = *view;
        let point = *point;
        find_unit(&mut &*host, &Table(&*models), entries_mut(entries, count), &view, point, prefer_resources)
    }
}

/// `PickUnits` (`all` false) or `PickUnitsInRectangle` (`all` true; `player` unused) for the C++ (under its lock):
/// the ids pushed onto `out` through the host. `planes` is the selection frustum's six planes, null when there are no
/// entries.
///
/// # Safety
/// `host` and `models` are valid; `entries` holds `count` entries; `planes` as said.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_gfx_pick_box(
    host: *const EeGfxPickHost,
    models: *const EeGePhysicalModels,
    entries: *mut EeGfxPickEntry,
    count: u32,
    planes: *const Ge3DPlane,
    all: bool,
    player: *const c_void,
    kind: u32,
    out: *mut c_void,
) {
    // SAFETY: the function's contract
    unsafe {
        let planes = side_planes(planes);
        let entries = entries_mut(entries, count);
        if all {
            pick_units_in_rectangle(&mut &*host, &Table(&*models), entries, planes.as_ref(), kind, out);
        } else {
            pick_units(&mut &*host, &Table(&*models), entries, planes.as_ref(), player, kind, out);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// one-triangle models: model n's triangle at x = n, its centre at its triangle's first corner
    struct Models {
        radius: f32,
        hit: Vec<(c_ulong, bool)>,
    }

    impl PickModels for Models {
        fn intersects(&self, entry: &EeGfxPickEntry, _line: &Ge3DLine, lod: c_ulong, distance: &mut f32) -> bool {
            *distance = entry.location.m[0][3] + entry.scale;
            self.hit.iter().any(|(l, yes)| *l == lod && *yes) && entry.model as usize != 99
        }
        fn has_parent(&self, m: *const c_void) -> bool {
            m as usize >= 100
        }
        fn internal_transform(&self, _m: *const c_void, _frame: c_ulong) -> GeTransformation {
            GeTransformation::IDENTITY
        }
        fn center(&self, _m: *const c_void) -> Ge3DPoint {
            Ge3DPoint::default()
        }
        fn radius(&self, _m: *const c_void, _frame: c_ulong) -> f32 {
            self.radius
        }
        fn lod_count(&self, _m: *const c_void) -> c_ulong {
            4
        }
        fn material_count(&self, _m: *const c_void) -> c_ulong {
            1
        }
        fn material(&self, _m: *const c_void, _index: c_ulong, _lod: c_ulong) -> GePhysicalMaterial {
            GePhysicalMaterial { vertex_start: 0, polygon_start: 0, polygon_length: 1 }
        }
        fn triangle(&self, m: *const c_void, _f: c_ulong, _l: c_ulong, _p: c_ulong, _s: c_ulong) -> [Ge3DPoint; 3] {
            let x = (m as usize % 100) as f32;
            [Ge3DPoint { x, y: 0.0, z: 0.0 }; 3]
        }
        fn sub_model_count(&self, m: *const c_void) -> c_ulong {
            (m as usize == 5) as c_ulong
        }
        fn sub_model(&self, _m: *const c_void, _index: c_ulong) -> *const c_void {
            100 as *const c_void
        }
    }

    /// planes keeping x >= 1 (the others always pass)
    fn planes() -> [Ge3DPlane; 4] {
        let wall =
            Ge3DPlane { point: Ge3DPoint { x: 1.0, y: 0.0, z: 0.0 }, normal: GeVector { x: 1.0, y: 0.0, z: 0.0 } };
        let open =
            Ge3DPlane { point: Ge3DPoint { x: 0.0, y: -1e9, z: 0.0 }, normal: GeVector { x: 0.0, y: 1.0, z: 0.0 } };
        [wall, open, open, open]
    }

    fn entry(model: usize, x: f32) -> EeGfxPickEntry {
        let mut location = GeTransformation::IDENTITY;
        location.m[0][3] = x;
        EeGfxPickEntry { location, model: model as *const c_void, scale: 1.0, frame: 0, unit_id: model as c_ulong }
    }

    #[test]
    fn the_view_test_takes_the_sphere_then_the_vertices_then_the_sub_models() {
        let a = Models { radius: 0.5, hit: vec![] };
        let p = planes();
        // the centre at x 3: inside
        assert!(entry_in_view(&a, &entry(0, 3.0), &p));
        // the centre at x 0, the sphere short of the plane: outside
        assert!(!entry_in_view(&a, &entry(2, 0.0), &p));
        // the sphere reaching it: model 2's vertex at x 2 is inside; model 0's at 0 is not
        let a = Models { radius: 1.5, hit: vec![] };
        assert!(entry_in_view(&a, &entry(2, 0.0), &p));
        assert!(!entry_in_view(&a, &entry(0, 0.0), &p));
        // model 5 is inside on its own vertices; sub-model 100 (vertex at 0) would not be
        assert!(entry_in_view(&a, &entry(5, 0.0), &p));
    }

    #[derive(Default)]
    struct Host {
        units: Vec<Option<EeGfxPickBoxUnit>>,
        facts: Vec<EeGfxPickFindUnit>,
        pixels: f32,
        pushed: Vec<u32>,
    }

    impl PickHost for Host {
        fn unit(&mut self, index: u32) -> *mut c_void {
            match self.units.get(index as usize) {
                Some(Some(_)) => (index as usize + 1) as *mut c_void,
                _ if (index as usize) < self.facts.len() => (index as usize + 1) as *mut c_void,
                _ => std::ptr::null_mut(),
            }
        }
        fn find_unit(&mut self, unit: *mut c_void) -> EeGfxPickFindUnit {
            self.facts[unit as usize - 1]
        }
        fn ask(&mut self, _unit: *mut c_void, question: EeGfxPickQuestion) -> bool {
            question == EeGfxPickQuestion::Ally
        }
        fn group_nonempty(&mut self) -> bool {
            false
        }
        fn pixel_size(&mut self, _index: u32) -> f32 {
            self.pixels
        }
        fn carrier(&mut self, unit: *mut c_void) -> *mut c_void {
            ((unit as usize) * 10) as *mut c_void
        }
        fn box_unit(&mut self, unit: *mut c_void) -> EeGfxPickBoxUnit {
            self.units[unit as usize - 1].expect("a unit")
        }
        fn push(&mut self, _out: *mut c_void, id: u32) {
            self.pushed.push(id);
        }
    }

    fn soldier(id: u32, building: bool) -> Option<EeGfxPickBoxUnit> {
        Some(EeGfxPickBoxUnit {
            complex: true,
            pickable: true,
            alive: true,
            category: 7,
            unit_type: 0x20,
            player: std::ptr::null(),
            outer_id: id,
            outer_land_building: building,
        })
    }

    #[test]
    fn the_box_puts_units_before_buildings_and_one_building_alone() {
        let a = Models { radius: 0.5, hit: vec![] };
        let p = planes();
        let entries = [entry(0, 3.0), entry(0, 4.0), entry(0, 0.0), entry(0, 5.0)];
        let mut h = Host {
            units: vec![soldier(1, true), soldier(2, false), soldier(3, false), soldier(4, true)],
            ..Default::default()
        };
        pick_units(&mut h, &a, &entries, Some(&p), std::ptr::null(), 0, std::ptr::null_mut());
        // walked from the last; entry 2 is out of view
        assert_eq!(h.pushed, [2]);
        h.pushed.clear();
        h.units[1].as_mut().unwrap().alive = false;
        pick_units(&mut h, &a, &entries, Some(&p), std::ptr::null(), 0, std::ptr::null_mut());
        assert_eq!(h.pushed, [4]);
        h.pushed.clear();
        // a citizen type: both passes (male, then female: none here), units then buildings
        pick_units(&mut h, &a, &entries, Some(&p), std::ptr::null(), 0x20, std::ptr::null_mut());
        assert_eq!(h.pushed, [4, 1]);
        h.pushed.clear();
        // the lasso: no life test, either citizen type
        pick_units_in_rectangle(&mut h, &a, &entries, Some(&p), 0x1e0, std::ptr::null_mut());
        assert_eq!(h.pushed, [4, 2, 1]);
        h.pushed.clear();
        pick_units_in_rectangle(&mut h, &a, &entries, None, 0, std::ptr::null_mut());
        assert!(h.pushed.is_empty());
    }

    fn view() -> EeGfxPickView {
        let viewport = GeViewport {
            half_width: 400.0,
            front_clipping_distance: 1.0,
            rear_clipping_distance: 4.0,
            ..Default::default()
        };
        EeGfxPickView { viewport, orthographic: true, show_hidden: false, free_camera: false, window_width: 800 }
    }

    #[test]
    fn find_takes_the_exact_hit_and_puts_the_scale_back() {
        let unit = EeGfxPickFindUnit {
            facts: EeGfxPickUnit { complex: true, hit_points: 5, ..Default::default() },
            tile_x: 1,
        };
        let mut h =
            Host { facts: vec![unit, unit, EeGfxPickFindUnit { tile_x: -10000, ..unit }], ..Default::default() };
        let mut entries = [entry(1, 3.0), entry(99, 1.0), entry(1, 0.5)];
        let a = Models { radius: 1.0, hit: vec![(1, true), (2, true)] };
        let got = find_unit(&mut h, &a, &mut entries, &view(), Ge2DPoint::default(), false);
        // entry 1 (model 99) never hits, entry 2 is off the map: entry 0's unit (1), its carrier 10
        assert_eq!(got as usize, 10);
        assert!(entries.iter().all(|e| e.scale == 1.0));
        // no exact hit, and the bounding hit large: nothing
        let a = Models { radius: 1.0, hit: vec![(1, true)] };
        h.pixels = 1000.0;
        assert!(find_unit(&mut h, &a, &mut entries, &view(), Ge2DPoint::default(), false).is_null());
        h.pixels = 1.0;
        assert_eq!(find_unit(&mut h, &a, &mut entries, &view(), Ge2DPoint::default(), false) as usize, 10);
        assert!(find_unit(&mut h, &a, &mut [], &view(), Ge2DPoint::default(), false).is_null());
    }
}
