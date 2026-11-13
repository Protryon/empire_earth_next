//! The scenario editor's actions (`Scenario Editor Actions/EESEA*.cpp`): what each one does to the world, over a
//! narrow world-edit host ([`SeaHost`]: a tile's existence, a mesh point's height, a terrain rectangle painted, an
//! area raised, the units on a tile, a unit removed, the cliffs and the minimap). The C++ action classes stay the
//! objects the editor's tabs create and post (`EEAction`s in the UI thread's queue, `actions.rs`); their queued areas
//! are a Rust record ([`SePaint`]) behind each, created and freed with it. The actions run under the UI copy lock
//! and the world's write lock (the queue's), on the UI thread.
//!
//! The paint actions (group 1):
//! - the terrain paint (`EESEAPaintTerrain`): each queued area painted with the terrain and its texture (unless the
//!   terrain only scatters ambient units), its ambient units scattered at random by the terrain's chance (the random
//!   seeded from the clock, as the original's), the cliffs and the minimap updated;
//! - the elevation paint (`EESEAPaintElevation`): each area raised as the elevation tab's radio says (a plateau at
//!   the level, rough hills, or flat ground), its flat cliffs removed, the cliffs and the minimap updated; the
//!   effects told about the last area only;
//! - the bulldozer (`EESEAPaintMoutains`): every unit on each area's tiles removed (multi-part units whole, path
//!   points first);
//! - the trigger area's rectangle (`EESEAPaintArea::SetCorner`): the rectangle between the drag's two corners.
//!
//! Every host call is in the original's order; the floats are the original's operations (`f32`, in order).

use std::collections::VecDeque;

use ee_random::URandom;

// ---- the host's ops (`ee_screenscpp_sea`: `i` up to 6 integers, `f` up to 5 floats, in and out)

/// The camera's follow flags cleared (`gB529066`, `gCameraFollowing`, `gB529068`), as every paint does first.
pub const EE_SCREENS_SEA_STOP_FOLLOW: u32 = 1;
/// -> whether the tile (i0, i1) is on the map (`EEMap::GetTile` not NULL).
pub const EE_SCREENS_SEA_TILE: u32 = 2;
/// -> whether the mesh point (i0, i1) is valid (unsigned coordinates, as `GetIsValidPoint`); f0 its height.
pub const EE_SCREENS_SEA_POINT: u32 = 3;
/// `EEMap::PaintTerrainRect(left i0, right i1, top i2, bottom i3, terrain i4, texture i5)`.
pub const EE_SCREENS_SEA_PAINT_RECT: u32 = 4;
/// `EEMap::UpdateCliffs(left i0, right i1, bottom i2, top i3)`.
pub const EE_SCREENS_SEA_UPDATE_CLIFFS: u32 = 5;
/// `gMiniMap->RedrawRect(i0, i1, i2, i3)`.
pub const EE_SCREENS_SEA_MINIMAP_RECT: u32 = 6;
/// `gMiniMap->Refresh()`.
pub const EE_SCREENS_SEA_MINIMAP_REFRESH: u32 = 7;
/// The terrain i0's table entry: -> its ambient unit chance; i1 = ambients only, i2 = its unit class count.
pub const EE_SCREENS_SEA_TERRAIN: u32 = 8;
/// -> the terrain i0's ambient unit class i1 (`EETerrainTable::GetUnitClass`).
pub const EE_SCREENS_SEA_TERRAIN_CLASS: u32 = 9;
/// `TMGetTime()` (the ambient units' seed).
pub const EE_SCREENS_SEA_TIME: u32 = 10;
/// An ambient unit of class i0 on the tile (i1, i2) for the world player, when `EECanPlaceOnTile` allows it.
pub const EE_SCREENS_SEA_PLANT: u32 = 11;
/// -> the elevation tab's checked radio: 0 raise (a plateau), 1 hills, 2 level ground.
pub const EE_SCREENS_SEA_ELEVATION_MODE: u32 = 12;
/// `EEMap::Raise(&i0, &i1, &i2, &i3, f0, f1, f2, f3, i4 != 0)` (left, right, top, bottom; the area comes back as
/// Raise left it).
pub const EE_SCREENS_SEA_RAISE: u32 = 13;
/// `EEMap::RemoveFlatCliffs(left i0, right i1, bottom i2, top i3)`.
pub const EE_SCREENS_SEA_REMOVE_FLAT_CLIFFS: u32 = 14;
/// The elevation tab's paint control stores its mouse tile (`EEUIFSEPaint::StoreMouseTile`).
pub const EE_SCREENS_SEA_STORE_MOUSE_TILE: u32 = 15;
/// `gEffectManager->ElevationChanged(left i0, top i1, right i2, bottom i3)`.
pub const EE_SCREENS_SEA_ELEVATION_CHANGED: u32 = 16;
/// -> the height of one elevation level (`kSEPELevelHeight`) in f0.
pub const EE_SCREENS_SEA_LEVEL_HEIGHT: u32 = 17;

/// The hills' height per level and the height from which they are less rough (0x44598c, 0x445988).
const SEA_HILL_HEIGHT: f32 = 0.816_496_55;
const SEA_HILL_LIMIT: f32 = 1.632_993_1;
/// The raise's slopes and plateau (the original's constants).
const SEA_SLOPE_GENTLE: f32 = 0.408_248_29;
const SEA_SLOPE_STEEP: f32 = 1.632_993_1;

/// The world-edit host.
pub trait SeaHost {
    /// One op of the `EE_SCREENS_SEA_*` list.
    fn op(&mut self, op: u32, i: &mut [i32; 6], f: &mut [f32; 5]) -> i32;
    /// The units on the tile (x, y), each the first part of its unit and referenced, with whether it is a path
    /// point: in the tile's list order.
    fn tile_units(&mut self, x: i32, y: i32) -> Vec<(usize, bool)>;
    /// A unit removed as the bulldozer removes it (a resource emptied first, killed, taken from its player) and its
    /// reference released.
    fn remove_unit(&mut self, unit: usize);
}

fn call<H: SeaHost + ?Sized>(h: &mut H, op: u32, ints: &[i32]) -> i32 {
    let mut i = [0; 6];
    i[..ints.len()].copy_from_slice(ints);
    h.op(op, &mut i, &mut [0.0; 5])
}

/// An area in tiles (top >= bottom: the loops count y down).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct SeaRect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}

/// Which paint an action is.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SePaintKind {
    /// `EESEAPaintTerrain(terrain, texture)`.
    Terrain { terrain: i32, texture: i32 },
    /// `EESEAPaintElevation(level)`.
    Elevation { level: i8 },
    /// `EESEAPaintMoutains`.
    Bulldozer,
}

/// The paint kinds of `ee_screens_sea_paint_new` (`kSEAPaint*` in `ee/EESEAPaint.h`).
pub const EE_SCREENS_SEA_PAINT_TERRAIN: u32 = 0;
pub const EE_SCREENS_SEA_PAINT_ELEVATION: u32 = 1;
pub const EE_SCREENS_SEA_PAINT_BULLDOZER: u32 = 2;

impl SePaintKind {
    /// The kind of a code (a and b: the terrain and its texture, or the level); an unknown code is the bulldozer.
    pub fn from_code(code: u32, a: i32, b: i32) -> Self {
        match code {
            EE_SCREENS_SEA_PAINT_TERRAIN => SePaintKind::Terrain { terrain: a, texture: b },
            EE_SCREENS_SEA_PAINT_ELEVATION => SePaintKind::Elevation { level: a as i8 },
            _ => SePaintKind::Bulldozer,
        }
    }
}

/// A paint action's record: its kind and queued areas (and the elevation's painted ones, kept as the original kept
/// them).
#[derive(Clone, Debug, PartialEq)]
pub struct SePaint {
    pub kind: SePaintKind,
    pub pending: VecDeque<SeaRect>,
    pub painted: Vec<SeaRect>,
}

impl SePaint {
    pub fn new(kind: SePaintKind) -> Self {
        SePaint { kind, pending: VecDeque::new(), painted: Vec::new() }
    }

    /// `SetArea`: queues the area when it changes anything; whether it queued.
    pub fn set_area<H: SeaHost + ?Sized>(&mut self, h: &mut H, left: i32, top: i32, right: i32, bottom: i32) -> bool {
        let rect = SeaRect { left, top, right, bottom };
        let changed = match self.kind {
            // any of its tiles on the map
            SePaintKind::Terrain { .. } => {
                let mut any = false;
                for y in (bottom..=top).rev() {
                    for x in left..=right {
                        if call(h, EE_SCREENS_SEA_TILE, &[x, y]) != 0 {
                            any = true;
                        }
                    }
                }
                any
            }
            // a point not yet at the level's height
            SePaintKind::Elevation { level } => {
                let height = elevation_height(h, level);
                let mut any = false;
                for y in (bottom..=top.wrapping_add(1)).rev() {
                    for x in left..=right.wrapping_add(1) {
                        let mut i = [x, y, 0, 0, 0, 0];
                        let mut f = [0.0; 5];
                        if h.op(EE_SCREENS_SEA_POINT, &mut i, &mut f) != 0 && f[0] != height {
                            any = true;
                        }
                    }
                }
                any
            }
            SePaintKind::Bulldozer => true,
        };
        if changed {
            self.pending.push_back(rect);
        }
        changed
    }

    /// `Virtual4`: every queued area painted, the last queued first.
    pub fn run<H: SeaHost + ?Sized>(&mut self, h: &mut H) {
        match self.kind {
            SePaintKind::Terrain { terrain, texture } => self.run_terrain(h, terrain, texture),
            SePaintKind::Elevation { level } => self.run_elevation(h, level),
            SePaintKind::Bulldozer => self.run_bulldozer(h),
        }
    }

    fn run_terrain<H: SeaHost + ?Sized>(&mut self, h: &mut H, terrain: i32, texture: i32) {
        let mut info = [terrain, 0, 0, 0, 0, 0];
        h.op(EE_SCREENS_SEA_TERRAIN, &mut info, &mut [0.0; 5]);
        let ambients_only = info[1] != 0;
        call(h, EE_SCREENS_SEA_STOP_FOLLOW, &[]);
        while let Some(r) = self.pending.pop_back() {
            if !ambients_only {
                call(h, EE_SCREENS_SEA_PAINT_RECT, &[r.left, r.right, r.top, r.bottom, terrain, texture]);
            }
            plant_terrain_units(h, terrain, r);
            call(
                h,
                EE_SCREENS_SEA_UPDATE_CLIFFS,
                &[r.left.wrapping_sub(2), r.right.wrapping_add(3), r.bottom.wrapping_sub(2), r.top.wrapping_add(3)],
            );
            call(h, EE_SCREENS_SEA_MINIMAP_RECT, &[r.left, r.right, r.top, r.bottom]);
        }
        call(h, EE_SCREENS_SEA_MINIMAP_REFRESH, &[]);
    }

    fn run_elevation<H: SeaHost + ?Sized>(&mut self, h: &mut H, level: i8) {
        call(h, EE_SCREENS_SEA_STOP_FOLLOW, &[]);
        let (height, raise, slope, plateau, roughness) = match call(h, EE_SCREENS_SEA_ELEVATION_MODE, &[]) {
            0 => (f32::from(level) * level_height(h), true, SEA_SLOPE_GENTLE, 0.0f32, 1.0f32),
            1 => {
                let height = f32::from(level) * SEA_HILL_HEIGHT;
                (height, true, SEA_SLOPE_STEEP, 0.0, if height < SEA_HILL_LIMIT { 0.4 } else { 0.26 })
            }
            _ => (0.0, false, SEA_SLOPE_STEEP, SEA_SLOPE_GENTLE, 1.0),
        };
        let mut region = SeaRect::default();
        while let Some(r) = self.pending.pop_back() {
            region = r;
            self.painted.push(r);
            let mut i = [region.left, region.right, region.top, region.bottom, i32::from(raise), 0];
            let mut f = [height, plateau, slope, roughness, 0.0];
            h.op(EE_SCREENS_SEA_RAISE, &mut i, &mut f);
            region = SeaRect { left: i[0], right: i[1], top: i[2], bottom: i[3] };
            call(
                h,
                EE_SCREENS_SEA_REMOVE_FLAT_CLIFFS,
                &[region.left, region.right.wrapping_add(1), region.bottom, region.top.wrapping_add(1)],
            );
            call(
                h,
                EE_SCREENS_SEA_UPDATE_CLIFFS,
                &[
                    region.left.wrapping_sub(2),
                    region.right.wrapping_add(3),
                    region.bottom.wrapping_sub(2),
                    region.top.wrapping_add(3),
                ],
            );
            call(h, EE_SCREENS_SEA_MINIMAP_RECT, &[region.left, region.right, region.top, region.bottom]);
        }
        call(h, EE_SCREENS_SEA_STORE_MOUSE_TILE, &[]);
        call(h, EE_SCREENS_SEA_ELEVATION_CHANGED, &[region.left, region.top, region.right, region.bottom]);
        call(h, EE_SCREENS_SEA_MINIMAP_REFRESH, &[]);
    }

    fn run_bulldozer<H: SeaHost + ?Sized>(&mut self, h: &mut H) {
        call(h, EE_SCREENS_SEA_STOP_FOLLOW, &[]);
        while let Some(r) = self.pending.pop_back() {
            let mut units = Vec::new();
            for y in (r.bottom..=r.top).rev() {
                for x in r.left..=r.right {
                    units.extend(h.tile_units(x, y));
                }
            }
            remove_in_order(h, units);
            call(h, EE_SCREENS_SEA_MINIMAP_RECT, &[r.left, r.right, r.top, r.bottom]);
        }
    }
}

/// The units removed path points first, the others after them, each group in its gathered order (the original's
/// `std::sort` with "a path point before a unit that is not": an order its ties leave to the library).
pub fn remove_in_order<H: SeaHost + ?Sized>(h: &mut H, mut units: Vec<(usize, bool)>) {
    units.sort_by_key(|&(_, path_point)| !path_point);
    for (unit, _) in units {
        h.remove_unit(unit);
    }
}

fn level_height<H: SeaHost + ?Sized>(h: &mut H) -> f32 {
    let mut f = [0.0; 5];
    h.op(EE_SCREENS_SEA_LEVEL_HEIGHT, &mut [0; 6], &mut f);
    f[0]
}

/// The height an elevation paint of `level` gives in the tab's mode (`SetArea`'s test).
fn elevation_height<H: SeaHost + ?Sized>(h: &mut H, level: i8) -> f32 {
    match call(h, EE_SCREENS_SEA_ELEVATION_MODE, &[]) {
        0 => f32::from(level) * level_height(h),
        1 => f32::from(level) * SEA_HILL_HEIGHT,
        _ => 0.0,
    }
}

/// `PlantTerrainUnits`: the terrain's ambient units over the area's tiles, one draw (1..100 against the chance) per
/// tile on the map and one more for the class when it hits; the random seeded from the clock.
pub fn plant_terrain_units<H: SeaHost + ?Sized>(h: &mut H, terrain: i32, r: SeaRect) {
    let mut info = [terrain, 0, 0, 0, 0, 0];
    let chance = h.op(EE_SCREENS_SEA_TERRAIN, &mut info, &mut [0.0; 5]);
    if chance <= 0 {
        return;
    }
    let classes = info[2];
    let mut random = URandom { seed: call(h, EE_SCREENS_SEA_TIME, &[]) as u32 };
    for y in (r.bottom..=r.top).rev() {
        for x in r.left..=r.right {
            if call(h, EE_SCREENS_SEA_TILE, &[x, y]) == 0 || random.generate_int(1, 100) >= chance {
                continue;
            }
            let index = random.generate_int(0, classes.wrapping_sub(1));
            let class = call(h, EE_SCREENS_SEA_TERRAIN_CLASS, &[terrain, index]);
            call(h, EE_SCREENS_SEA_PLANT, &[class, x, y]);
        }
    }
}

/// `EESEAPaintArea::SetCorner`'s rectangle: the drag's start and new end corners -> (left, top, right, bottom), the
/// smaller coordinate first on each axis.
pub fn area_rectangle(start_x: i32, start_y: i32, end_x: i32, end_y: i32) -> (i32, i32, i32, i32) {
    let (left, right) = if start_x > end_x { (end_x, start_x) } else { (start_x, end_x) };
    let (top, bottom) = if start_y > end_y { (end_y, start_y) } else { (start_y, end_y) };
    (left, top, right, bottom)
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A recording host over a small diamond map: tiles where |x - y| < 4 and 4 <= x + y <= 20, points one more.
    #[derive(Default)]
    struct Rec {
        log: Vec<String>,
        mode: i32,
        heights: std::collections::HashMap<(i32, i32), f32>,
        units: std::collections::HashMap<(i32, i32), Vec<(usize, bool)>>,
        chance: i32,
        ambients_only: bool,
        time: i32,
    }

    fn on_map(x: i32, y: i32) -> bool {
        (x - y).abs() < 4 && (4..=20).contains(&(x + y))
    }

    impl SeaHost for Rec {
        fn op(&mut self, op: u32, i: &mut [i32; 6], f: &mut [f32; 5]) -> i32 {
            match op {
                EE_SCREENS_SEA_TILE => i32::from(on_map(i[0], i[1])),
                EE_SCREENS_SEA_POINT => {
                    if i[0] < 0 || i[1] < 0 || !on_map(i[0], i[1]) {
                        return 0;
                    }
                    f[0] = *self.heights.get(&(i[0], i[1])).unwrap_or(&0.0);
                    1
                }
                EE_SCREENS_SEA_TERRAIN => {
                    i[1] = i32::from(self.ambients_only);
                    i[2] = 3;
                    self.chance
                }
                EE_SCREENS_SEA_TERRAIN_CLASS => 100 + i[1],
                EE_SCREENS_SEA_TIME => self.time,
                EE_SCREENS_SEA_ELEVATION_MODE => self.mode,
                EE_SCREENS_SEA_LEVEL_HEIGHT => {
                    f[0] = 0.204_124_14;
                    0
                }
                EE_SCREENS_SEA_RAISE => {
                    self.log.push(format!("raise {:?} {:?}", &i[..5], &f[..4]));
                    i[0] -= 1; // Raise may widen the area
                    0
                }
                _ => {
                    self.log.push(format!("op {op} {:?}", &i[..]));
                    0
                }
            }
        }
        fn tile_units(&mut self, x: i32, y: i32) -> Vec<(usize, bool)> {
            self.units.get(&(x, y)).cloned().unwrap_or_default()
        }
        fn remove_unit(&mut self, unit: usize) {
            self.log.push(format!("remove {unit}"));
        }
    }

    #[test]
    fn terrain_queues_only_areas_on_the_map() {
        let mut h = Rec::default();
        let mut p = SePaint::new(SePaintKind::Terrain { terrain: 3, texture: 7 });
        assert!(!p.set_area(&mut h, 30, 2, 32, 0));
        assert!(p.set_area(&mut h, 5, 6, 6, 5));
        assert_eq!(p.pending.len(), 1);
    }

    #[test]
    fn terrain_paints_last_area_first_then_refreshes() {
        let mut h = Rec::default();
        let mut p = SePaint::new(SePaintKind::Terrain { terrain: 3, texture: 7 });
        p.set_area(&mut h, 5, 6, 6, 5);
        p.set_area(&mut h, 8, 9, 9, 8);
        p.run(&mut h);
        assert_eq!(
            h.log,
            [
                "op 1 [0, 0, 0, 0, 0, 0]",
                "op 4 [8, 9, 9, 8, 3, 7]",
                "op 5 [6, 12, 6, 12, 0, 0]",
                "op 6 [8, 9, 9, 8, 0, 0]",
                "op 4 [5, 6, 6, 5, 3, 7]",
                "op 5 [3, 9, 3, 9, 0, 0]",
                "op 6 [5, 6, 6, 5, 0, 0]",
                "op 7 [0, 0, 0, 0, 0, 0]",
            ]
        );
        assert!(p.pending.is_empty());
    }

    #[test]
    fn ambients_only_plants_without_painting_and_draws_per_tile() {
        let mut h = Rec { chance: 100, ambients_only: true, time: 5000, ..Rec::default() };
        let mut p = SePaint::new(SePaintKind::Terrain { terrain: 41, texture: 2 });
        p.set_area(&mut h, 5, 6, 6, 5);
        p.run(&mut h);
        // no paint; four tiles on the map, every draw under 100 hits: one plant each, in y-down, x-up order
        let plants: Vec<&String> = h.log.iter().filter(|l| l.starts_with("op 11")).collect();
        assert_eq!(plants.len(), 4);
        assert!(!h.log.iter().any(|l| l.starts_with("op 4 ")));
        let mut r = URandom { seed: 5000 };
        let mut want = Vec::new();
        for (x, y) in [(5, 6), (6, 6), (5, 5), (6, 5)] {
            assert!(r.generate_int(1, 100) < 100);
            want.push(format!("op 11 [{}, {x}, {y}, 0, 0, 0]", 100 + r.generate_int(0, 2)));
        }
        assert_eq!(plants, want.iter().collect::<Vec<_>>());
    }

    #[test]
    fn no_chance_no_draw() {
        let mut h = Rec { chance: 0, ..Rec::default() };
        plant_terrain_units(&mut h, 3, SeaRect { left: 5, top: 6, right: 6, bottom: 5 });
        assert!(h.log.is_empty());
    }

    #[test]
    fn elevation_set_area_tests_the_points_one_beyond() {
        let mut h = Rec::default();
        let mut p = SePaint::new(SePaintKind::Elevation { level: 0 });
        h.mode = 2; // level ground: height 0 everywhere already
        assert!(!p.set_area(&mut h, 5, 6, 6, 5));
        h.heights.insert((7, 7), 0.5); // right + 1, top + 1
        assert!(p.set_area(&mut h, 5, 6, 6, 5));
    }

    #[test]
    fn elevation_modes_and_the_last_region() {
        for (mode, level, want) in [
            (0, 3i8, "raise [5, 6, 6, 5, 1] [0.6123724, 0.0, 0.40824828, 1.0]"),
            (1, 1, "raise [5, 6, 6, 5, 1] [0.81649655, 0.0, 1.6329931, 0.4]"),
            (1, 2, "raise [5, 6, 6, 5, 1] [1.6329931, 0.0, 1.6329931, 0.26]"),
            (2, 4, "raise [5, 6, 6, 5, 0] [0.0, 0.40824828, 1.6329931, 1.0]"),
        ] {
            let mut h = Rec { mode, ..Rec::default() };
            h.heights.insert((5, 5), 9.0);
            let mut p = SePaint::new(SePaintKind::Elevation { level });
            assert!(p.set_area(&mut h, 5, 6, 6, 5));
            p.run(&mut h);
            assert_eq!(h.log[1], want, "mode {mode} level {level}");
            // the area as Raise left it (left - 1) goes on to the cliffs, the minimap and the effects
            assert_eq!(h.log[2], "op 14 [4, 7, 5, 7, 0, 0]");
            assert_eq!(h.log[3], "op 5 [2, 9, 3, 9, 0, 0]");
            assert_eq!(h.log[4], "op 6 [4, 6, 6, 5, 0, 0]");
            assert_eq!(h.log[5], "op 15 [0, 0, 0, 0, 0, 0]");
            assert_eq!(h.log[6], "op 16 [4, 6, 6, 5, 0, 0]");
            assert_eq!(p.painted.len(), 1);
        }
    }

    #[test]
    fn elevation_without_areas_tells_the_effects_an_empty_one() {
        let mut h = Rec::default();
        let mut p = SePaint::new(SePaintKind::Elevation { level: 1 });
        p.run(&mut h);
        assert_eq!(h.log[1..], ["op 15 [0, 0, 0, 0, 0, 0]", "op 16 [0, 0, 0, 0, 0, 0]", "op 7 [0, 0, 0, 0, 0, 0]"]);
    }

    #[test]
    fn bulldozer_removes_path_points_first() {
        let mut h = Rec::default();
        h.units.insert((5, 6), vec![(1, false), (2, true)]);
        h.units.insert((6, 5), vec![(3, true), (4, false)]);
        let mut p = SePaint::new(SePaintKind::Bulldozer);
        assert!(p.set_area(&mut h, 5, 6, 6, 5));
        p.run(&mut h);
        assert_eq!(
            h.log,
            ["op 1 [0, 0, 0, 0, 0, 0]", "remove 2", "remove 3", "remove 1", "remove 4", "op 6 [5, 6, 6, 5, 0, 0]",]
        );
    }

    #[test]
    fn kinds_from_codes() {
        assert_eq!(
            SePaintKind::from_code(EE_SCREENS_SEA_PAINT_TERRAIN, 3, 7),
            SePaintKind::Terrain { terrain: 3, texture: 7 }
        );
        assert_eq!(SePaintKind::from_code(EE_SCREENS_SEA_PAINT_ELEVATION, -2, 0), SePaintKind::Elevation { level: -2 });
        assert_eq!(SePaintKind::from_code(EE_SCREENS_SEA_PAINT_BULLDOZER, 0, 0), SePaintKind::Bulldozer);
    }

    #[test]
    fn the_area_rectangle_orders_its_corners() {
        assert_eq!(area_rectangle(3, 4, 9, 12), (3, 4, 9, 12));
        assert_eq!(area_rectangle(9, 12, 3, 4), (3, 4, 9, 12));
        assert_eq!(area_rectangle(9, 4, 3, 12), (3, 4, 9, 12));
    }
}
