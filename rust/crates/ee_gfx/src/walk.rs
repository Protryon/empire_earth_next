//! The capture's world walk (`EERenderObject.cpp`): `IsUnitDrawn`'s decisions (seen or owned, scaled, the pools'
//! room, the dead, a footprint counted once, the view tests and the free camera's exceptions) and a map object's or
//! unit's point for the view test, the owned-food test `FillUnit` shares, the waypoint markers' tint by their
//! carrier's id and `FillRenderLists`' music intensity. The C++ keeps the walk over the visible tiles, the render
//! records and the world: it gathers a unit's facts (`EeGfxUnitFacts`) and answers the set and view tests
//! (`EeGfxDrawnHost`).
//!
//! Drawn only (the renderer's thread). The C++'s `long` is 32 bits on Windows.

use std::ffi::c_void;

use crate::effects::EeGfxColor;

/// `kUnitCategoryLandCitizen`, `kUnitCategoryNavalCitizen`.
const CATEGORY_LAND_CITIZEN: i32 = 6;
const CATEGORY_NAVAL_CITIZEN: i32 = 0x16;
/// `kUnitTypePathPoint`.
const UNIT_TYPE_PATH_POINT: i32 = 0xdf;
/// The render pools' sizes (units, map objects).
const UNIT_POOL: u32 = 0x200;
const OBJECT_POOL: u32 = 0x600;

/// What `IsUnitDrawn` reads of a unit, its complex unit, its tile and the frame.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxUnitFacts {
    /// a food resource with some left
    pub food: bool,
    pub has_complex: bool,
    /// the unit class's category
    pub category: i32,
    /// the owner's player id
    pub owner_id: i32,
    /// the tile is seen by the current player
    pub seen: bool,
    pub free_camera: bool,
    /// the stats' scale
    pub scale: f32,
    pub is_tree: bool,
    /// the unit class's type
    pub unit_type: i32,
    /// the render pools' fill (units, map objects)
    pub unit_count: u32,
    pub object_count: u32,
    /// the complex unit's command is the dead action
    pub dead: bool,
    pub tile_size: i32,
    /// the complex unit's class: a gate, a path point, a flare; the complex unit is a foundation
    pub complex_gate: bool,
    pub complex_path_point: bool,
    pub complex_flare: bool,
    pub foundation: bool,
    /// the unit's owner is the current player
    pub mine: bool,
    /// `EEUnit::Fn27C0B4` (the original's `return false`)
    pub marker_shown: bool,
    /// `IsPathMarker`
    pub path_marker: bool,
    pub id: i32,
    pub tile_x: i32,
    pub tile_y: i32,
}

/// The C++'s answers: `first_drawn` inserts the unit into the frame's set and says whether it was new;
/// `complex_in_view` tests the complex unit's physical model against the view; `model_in_view` places the unit's
/// first model at (`x`, `y`) on the ground, facing the camera, and tests it.
#[repr(C)]
pub struct EeGfxDrawnHost {
    pub context: *mut c_void,
    pub first_drawn: extern "C" fn(context: *mut c_void) -> bool,
    pub complex_in_view: extern "C" fn(context: *mut c_void) -> bool,
    pub model_in_view: extern "C" fn(context: *mut c_void, x: f32, y: f32) -> bool,
}

/// The view questions, as Rust asks them.
pub trait DrawnHost {
    fn first_drawn(&mut self) -> bool;
    fn complex_in_view(&mut self) -> bool;
    fn model_in_view(&mut self, x: f32, y: f32) -> bool;
}

impl DrawnHost for EeGfxDrawnHost {
    fn first_drawn(&mut self) -> bool {
        (self.first_drawn)(self.context)
    }
    fn complex_in_view(&mut self) -> bool {
        (self.complex_in_view)(self.context)
    }
    fn model_in_view(&mut self, x: f32, y: f32) -> bool {
        (self.model_in_view)(self.context, x, y)
    }
}

/// The nature's food a unit stands for (a complex unit of the world player that is no citizen and holds food): drawn
/// unseen and, in `FillUnit`, fogged where its tile is not seen.
pub fn owned_food(food: bool, has_complex: bool, category: i32, owner_id: i32) -> bool {
    food && has_complex && category != CATEGORY_LAND_CITIZEN && category != CATEGORY_NAVAL_CITIZEN && owner_id == 0
}

/// `IsUnitDrawn`: whether the unit is drawn this frame.
pub fn unit_drawn<H: DrawnHost>(f: &EeGfxUnitFacts, h: &mut H) -> bool {
    let owned = owned_food(f.food, f.has_complex, f.category, f.owner_id);
    if !((f.seen || owned || f.free_camera) && f.scale != 0.0) {
        return false;
    }
    if !f.is_tree && f.unit_type != UNIT_TYPE_PATH_POINT {
        if !(f.unit_count < UNIT_POOL && !f.dead) {
            return false;
        }
        if f.tile_size > 1 && !h.first_drawn() {
            return false;
        }
        if f.has_complex {
            let mut result = true;
            if !f.complex_gate {
                result = h.complex_in_view();
                if !result {
                    return false;
                }
            }
            if !f.free_camera {
                if f.complex_path_point {
                    result = f.mine && f.marker_shown;
                } else if f.foundation {
                    result = f.mine;
                } else if f.complex_flare {
                    result = false;
                }
            }
            result
        } else {
            let half = f.tile_size as f32 * 0.5;
            h.model_in_view(f.tile_x as f32 + half, f.tile_y as f32 + half) && !(f.path_marker && !f.mine)
        }
    } else if f.is_tree && f.object_count < OBJECT_POOL {
        let half = f.tile_size as f32 * 0.5;
        let x = (f.id % 3 - 1).wrapping_mul(f.tile_size) as f32 * 0.125 + f.tile_x as f32 + half;
        let y = (f.id / 3 % 3 - 1).wrapping_mul(f.tile_size) as f32 * 0.125 + f.tile_y as f32 + half;
        h.model_in_view(x, y)
    } else {
        false
    }
}

/// A waypoint marker's tint by its carrier's id: two channels from the id's low 24 bits plus 1 and 2, the third from
/// that number modulo 6, by the number modulo 3; fifths.
pub fn waypoint_tint(carrier_id: i32) -> EeGfxColor {
    let id = carrier_id as u32 & 0xffffff;
    let (low, one, two) = ((id % 6) as f32 / 5.0, (id + 1) as f32 / 5.0, (id + 2) as f32 / 5.0);
    match id % 3 {
        0 => EeGfxColor { red: low, green: one, blue: two },
        1 => EeGfxColor { red: one, green: low, blue: two },
        _ => EeGfxColor { red: two, green: one, blue: low },
    }
}

/// `FillRenderLists`' music intensity (1 calm .. 3 battle) from the music's current one and the share of the drawn
/// units that fight.
pub fn music_intensity(current: u32, fighting: f32, count: f32) -> u32 {
    let average = if count != 0.0 { fighting / count } else { 0.0 };
    let battle = average < -0.0001 || average > 0.0001;
    match current {
        3 if battle => 3,
        2 if battle => {
            if average >= 0.65 {
                3
            } else {
                2
            }
        }
        0 | 1 if battle => {
            if average < 0.5 {
                2
            } else {
                3
            }
        }
        _ => 1,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct Log {
        drawn: bool,
        complex: bool,
        model: bool,
        calls: Vec<String>,
    }

    impl DrawnHost for Log {
        fn first_drawn(&mut self) -> bool {
            self.calls.push("first".into());
            self.drawn
        }
        fn complex_in_view(&mut self) -> bool {
            self.calls.push("complex".into());
            self.complex
        }
        fn model_in_view(&mut self, x: f32, y: f32) -> bool {
            self.calls.push(format!("model {x} {y}"));
            self.model
        }
    }

    fn unit() -> EeGfxUnitFacts {
        EeGfxUnitFacts { seen: true, scale: 1.0, tile_size: 1, owner_id: 2, tile_x: 10, tile_y: 20, ..Default::default() }
    }

    #[test]
    fn unseen_and_unscaled_units_are_not_drawn() {
        let mut h = Log { model: true, ..Default::default() };
        let mut f = unit();
        assert!(unit_drawn(&f, &mut h));
        assert_eq!(h.calls, ["model 10.5 20.5"]);
        f.seen = false;
        assert!(!unit_drawn(&f, &mut h));
        f.free_camera = true;
        assert!(unit_drawn(&f, &mut h));
        f.scale = 0.0;
        assert!(!unit_drawn(&f, &mut h));
        // the world's food is drawn unseen
        let mut g = unit();
        g.seen = false;
        g.food = true;
        g.has_complex = true;
        g.complex_gate = true;
        g.owner_id = 0;
        assert!(unit_drawn(&g, &mut h));
        g.category = CATEGORY_LAND_CITIZEN;
        assert!(!unit_drawn(&g, &mut h));
    }

    #[test]
    fn complex_units_test_their_model_and_the_free_camera_exceptions() {
        let mut h = Log { complex: true, drawn: true, ..Default::default() };
        let mut f = unit();
        f.has_complex = true;
        f.tile_size = 2;
        assert!(unit_drawn(&f, &mut h));
        assert_eq!(h.calls, ["first", "complex"]);
        h.drawn = false;
        assert!(!unit_drawn(&f, &mut h));
        h.drawn = true;
        f.foundation = true;
        assert!(!unit_drawn(&f, &mut h));
        f.mine = true;
        assert!(unit_drawn(&f, &mut h));
        f.complex_path_point = true;
        assert!(!unit_drawn(&f, &mut h));
        f.marker_shown = true;
        assert!(unit_drawn(&f, &mut h));
        f.complex_path_point = false;
        f.foundation = false;
        f.complex_flare = true;
        assert!(!unit_drawn(&f, &mut h));
        f.free_camera = true;
        assert!(unit_drawn(&f, &mut h));
        h.complex = false;
        assert!(!unit_drawn(&f, &mut h));
        f.dead = true;
        h.calls.clear();
        assert!(!unit_drawn(&f, &mut h));
        assert!(h.calls.is_empty());
        f.dead = false;
        f.unit_count = UNIT_POOL;
        assert!(!unit_drawn(&f, &mut h));
    }

    #[test]
    fn trees_sit_off_their_tiles_by_id_and_path_points_are_skipped() {
        let mut h = Log { model: true, ..Default::default() };
        let mut f = unit();
        f.is_tree = true;
        f.tile_size = 2;
        f.id = 4;
        assert!(unit_drawn(&f, &mut h));
        f.id = -1;
        f.object_count = OBJECT_POOL - 1;
        assert!(unit_drawn(&f, &mut h));
        assert_eq!(h.calls, ["model 11 21", "model 10.5 20.75"]);
        f.object_count = OBJECT_POOL;
        assert!(!unit_drawn(&f, &mut h));
        let mut p = unit();
        p.unit_type = UNIT_TYPE_PATH_POINT;
        assert!(!unit_drawn(&p, &mut h));
        // a path marker of another player
        let mut m = unit();
        m.path_marker = true;
        assert!(!unit_drawn(&m, &mut h));
        m.mine = true;
        assert!(unit_drawn(&m, &mut h));
    }

    #[test]
    fn waypoint_tints_and_music() {
        assert_eq!(waypoint_tint(0), EeGfxColor { red: 0.0, green: 0.2, blue: 0.4 });
        assert_eq!(waypoint_tint(7), EeGfxColor { red: 8.0 / 5.0, green: 0.2, blue: 9.0 / 5.0 });
        assert_eq!(waypoint_tint(-1), waypoint_tint(0xffffff));
        assert_eq!(music_intensity(3, 0.0, 0.0), 1);
        assert_eq!(music_intensity(3, 1.0, 10.0), 3);
        assert_eq!(music_intensity(2, 6.5, 10.0), 3);
        assert_eq!(music_intensity(2, 6.0, 10.0), 2);
        assert_eq!(music_intensity(1, 5.0, 10.0), 3);
        assert_eq!(music_intensity(0, 4.0, 10.0), 2);
        assert_eq!(music_intensity(4, 4.0, 10.0), 1);
        assert_eq!(music_intensity(0, 0.0, 10.0), 1);
    }
}
