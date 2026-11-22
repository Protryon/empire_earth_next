//! The render lists' texture maps and selections in Rust (game core lane, round 11; were EERenderLists' six
//! `std::multimap<GETexture*, ...>`, `mSelectedUnits`, `mSelectedObjects`, `mSelectedUnit`, `mSelectedObject`) and
//! the lists' view ee_gfx reads (was EEROUnits.cpp's `EERenderListsView` and `MakeLayout`): the fill
//! (render_object.rs) files each render unit and object here; the view hands the maps in the multimaps' order (by
//! texture address, equal keys in insertion order) with the C++ pools, ghosts and lines (each read one case of
//! EEROUnits.cpp's `EECore11OpA4`). Only a game with a window fills and draws them (the tour; RFIL compares the
//! fills).

use std::ffi::{c_char, c_ulong, c_void};
use std::sync::{Mutex, OnceLock};

use ee_render::gfx::fill::EeGfxFillObject;
use ee_render::gfx::rfil::{EeGfxRfilFrame, EeGfxRfilLayout, EeGfxRfilPair, EeGfxRfilPairs, EeGfxRfilSpan};

use super::{h, hp, O};

/// One entry of a texture map (`EeGfxRfilPair`'s layout: the texture, the render unit or object).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct RlPair {
    key: usize,
    value: usize,
}

const _: () = assert!(std::mem::size_of::<RlPair>() == std::mem::size_of::<EeGfxRfilPair>());

/// The maps (0..2 the units' `mUnits0..2`, 3..5 the objects' `mShadows0..2`), whether each is in its order, the
/// selections.
struct Lists {
    maps: [Vec<RlPair>; 6],
    sorted: [bool; 6],
    selected_units: Vec<usize>,
    selected_objects: Vec<usize>,
    selected_unit: usize,
    selected_object: usize,
}

static LISTS: Mutex<Lists> = Mutex::new(Lists {
    maps: [Vec::new(), Vec::new(), Vec::new(), Vec::new(), Vec::new(), Vec::new()],
    sorted: [true; 6],
    selected_units: Vec::new(),
    selected_objects: Vec::new(),
    selected_unit: 0,
    selected_object: 0,
});

fn lists() -> std::sync::MutexGuard<'static, Lists> {
    LISTS.lock().unwrap_or_else(|e| e.into_inner())
}

/// The map of the units' (`objects` false) or the objects' map `k` (0..2).
fn map_index(objects: bool, k: i64) -> usize {
    k as usize + if objects { 3 } else { 0 }
}

/// A multimap's order: by key, equal keys in insertion order (a stable sort).
fn order(map: &mut [RlPair]) {
    map.sort_by_key(|p| p.key);
}

/// The six maps and the two selected lists emptied (FillRenderLists' and EEReleaseScene's).
pub fn clear() {
    let mut l = lists();
    for m in l.maps.iter_mut() {
        m.clear();
    }
    l.sorted = [true; 6];
    l.selected_objects.clear();
    l.selected_units.clear();
}

/// A map's insert by texture.
pub fn insert(objects: bool, k: i64, texture: usize, item: usize) {
    let mut l = lists();
    let i = map_index(objects, k);
    l.maps[i].push(RlPair { key: texture, value: item });
    l.sorted[i] = false;
}

/// A render unit with a selection ring or bar.
pub fn select_unit(unit: usize) {
    lists().selected_units.push(unit);
}

/// A map object with a selection ring.
pub fn select_object(object: usize) {
    lists().selected_objects.push(object);
}

/// The render unit under the mouse (its bars are drawn outside the selections).
pub fn set_selected_unit(unit: usize) {
    lists().selected_unit = unit;
}

/// No unit or object under the mouse.
pub fn clear_selected() {
    let mut l = lists();
    l.selected_unit = 0;
    l.selected_object = 0;
}

fn span(data: usize, count: usize) -> EeGfxRfilSpan {
    EeGfxRfilSpan { data: data as *const c_void, count }
}

fn off(op: O) -> u64 {
    h(op, 0, &[]) as u64
}

/// Where the render lists' C++ members sit.
fn layout() -> &'static EeGfxRfilLayout {
    static LAYOUT: OnceLock<EeGfxRfilLayout> = OnceLock::new();
    LAYOUT.get_or_init(|| EeGfxRfilLayout {
        size: std::mem::size_of::<EeGfxRfilLayout>() as u64,
        unit_stride: off(O::RlLayUnitStride),
        unit_health_bars: off(O::RlLayUnitHealthBars),
        unit_turret_angles: off(O::RlLayUnitTurretAngles),
        unit_turret_models: off(O::RlLayUnitTurretModels),
        unit_orbital_angles: off(O::RlLayUnitOrbitalAngles),
        unit_orbital_models: off(O::RlLayUnitOrbitalModels),
        unit_wheel_angles: off(O::RlLayUnitWheelAngles),
        unit_wheel_models: off(O::RlLayUnitWheelModels),
        unit_name: off(O::RlLayUnitName),
        record_stride: off(O::RlLayRecordStride),
        record_heights: off(O::RlLayRecordHeights),
        record_cells: off(O::RlLayRecordCells),
        record_tile_x: off(O::RlLayRecordTileX),
        record_tile_y: off(O::RlLayRecordTileY),
        line_stride: off(O::RlLayLineStride),
        line_points: off(O::RlLayLinePoints),
        line_texture: off(O::RlLayLineTexture),
        line_width: off(O::RlLayLineWidth),
    })
}

/// The render lists as ee_gfx reads them in place (rfil.rs, units_lists.rs): the pools and vectors, the maps in
/// their order (valid until the next fill).
pub fn view(f: &mut EeGfxRfilFrame) {
    f.layout = layout();
    f.units = hp(O::RlUnits, 0, &[]) as *const c_void;
    // SAFETY: the pools' counts
    f.units_used = unsafe { *(hp(O::RoUnitCount, 0, &[]) as *const c_ulong) } as usize;
    f.shadows = hp(O::RlShadows, 0, &[]) as *const EeGfxFillObject;
    // SAFETY: as above
    f.shadows_used = unsafe { *(hp(O::RoObjectCount, 0, &[]) as *const c_ulong) } as usize;
    {
        let mut l = lists();
        let l = &mut *l;
        for (i, m) in l.maps.iter_mut().enumerate() {
            if !l.sorted[i] {
                order(m);
                l.sorted[i] = true;
            }
            f.maps[i] = EeGfxRfilPairs { data: m.as_ptr() as *const EeGfxRfilPair, count: m.len() };
        }
        f.selected_units = span(l.selected_units.as_ptr() as usize, l.selected_units.len());
        f.selected_objects = span(l.selected_objects.as_ptr() as usize, l.selected_objects.len());
        f.selected_unit = l.selected_unit as *const c_void;
        f.selected_object = l.selected_object as *const c_void;
    }
    f.label_unit = hp(O::RlLabelUnit, 0, &[]) as *const c_void;
    f.label_name = hp(O::RlLabelName, 0, &[]) as *const c_char;
    f.picked_ids = span(hp(O::RlPickedIds, 0, &[]), h(O::RlPickedCount, 0, &[]) as usize);
    f.ghosts = span(hp(O::RlGhosts, 0, &[]), h(O::RoGhostRecordsCount, 0, &[]) as usize);
    f.lines = span(hp(O::RlLines, 0, &[]), h(O::RlLinesCount, 0, &[]) as usize);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_map_is_in_key_order_with_equal_keys_in_insertion_order() {
        let mut m = vec![
            RlPair { key: 30, value: 1 },
            RlPair { key: 10, value: 2 },
            RlPair { key: 30, value: 3 },
            RlPair { key: 10, value: 4 },
            RlPair { key: 20, value: 5 },
        ];
        order(&mut m);
        let values: Vec<usize> = m.iter().map(|p| p.value).collect();
        assert_eq!(values, vec![2, 4, 5, 1, 3]);
    }

    #[test]
    fn the_objects_maps_follow_the_units() {
        assert_eq!(map_index(false, 0), 0);
        assert_eq!(map_index(false, 2), 2);
        assert_eq!(map_index(true, 0), 3);
        assert_eq!(map_index(true, 2), 5);
    }
}
