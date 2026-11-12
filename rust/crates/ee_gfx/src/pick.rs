//! The unit picker's decisions (`src/Empire Earth/Graphics Thread/EEUnitPicker.cpp`): the screen ray through a
//! point of the game view (also the mini map's `ProjectToGround`), `FindUnit`'s tolerances, the entries' pick scale,
//! the "large on screen" test and the ranking of the ray's hits (exact hits beat bounding hits, then the nearest,
//! the click priority deciding near ties), `GetUnitPickPriority`, `AddUnit`'s admission and `GetIsEntryInView`'s
//! bounding-sphere test against the selection frustum's side planes. The C++ keeps the entries, the model tests
//! (`GEPhysicalModel::Intersects`, `GERasterizer::GetPixelSize`, the vertex walk), the lock and the world.
//!
//! This is the UI's and the renderer's, not the simulation's.

use ee_ge::types::{Ge2DPoint, Ge3DPlane, Ge3DPoint, GeTransformation, GeVector};
use ee_ge::um::lrintf;
use ee_ge::viewport::GeViewport;

/// `kU441C28`: the depth tolerance, in clipping ranges.
pub const PICK_TOLERANCE: f32 = 3.0;
/// `kMinPickPixelSize`: the smallest pixel size `AddUnit` keeps.
pub const MIN_PICK_PIXEL_SIZE: f32 = 0.35;
/// `kFreeCameraPickWidth`: the window width the free camera picks against.
pub const FREE_CAMERA_PICK_WIDTH: f32 = 1024.0;
/// `kLargeHitFraction`: a bounding hit wider than this share of the window is "large" (ranked last).
pub const LARGE_HIT_FRACTION: f32 = 0.17;
/// `kU441C38`: `GetIsEntryInView`'s detail: the LOD a quarter from the coarsest.
pub const VIEW_LOD_FRACTION: f32 = 0.25;
/// `kPickScale`: the bounding test's growth for everything but trees.
pub const PICK_SCALE: f32 = 1.2;
/// `kTreePickScale`: a tree's (priority 2) scale in both tests.
pub const TREE_PICK_SCALE: f32 = 0.9;

/// The resource ids `GetUnitPickPriority` reads (`EEResourceType.h`).
pub const RESOURCE_FOOD: u32 = 2;
pub const RESOURCE_WOOD: u32 = 3;
pub const RESOURCE_STONE: u32 = 4;
/// The unit categories, family and types the picker tests (`EEUnitClassIDs.h`).
pub const CATEGORY_AMBIENT: u32 = 0;
pub const CATEGORY_LAND_CITIZEN: u32 = 6;
pub const CATEGORY_NAVAL_CITIZEN: u32 = 0x16;
pub const FAMILY_AA_BUILDING: u32 = 0x2e;
pub const TYPE_PATH_POINT: u32 = 0xdf;
pub const TYPE_CP_ICBM: u32 = 0x2ee;

/// A screen point's ray: its world points on the near and far clipping planes.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxPickRay {
    /// on the near (front) clipping plane
    pub front: Ge3DPoint,
    /// on the far (rear) one
    pub rear: Ge3DPoint,
}

/// `FindUnit`'s (and `ProjectToGround`'s) ray: the point normalised against the view's centre and half width, then
/// denormalised on the near (z 0) and far (z 1) planes, perspective or orthographic.
pub fn screen_ray(vp: &GeViewport, orthographic: bool, point: Ge2DPoint) -> EeGfxPickRay {
    let scale = 1.0 / vp.half_width;
    let x = (point.x - vp.x_center) * scale;
    let y = -((point.y - vp.y_center) * scale);
    let near = Ge3DPoint { x, y, z: 0.0 };
    let far = Ge3DPoint { x, y, z: 1.0 };
    if !orthographic {
        EeGfxPickRay {
            front: vp.denormalize_perspective_point(near, vp.front_clipping_distance),
            rear: vp.denormalize_perspective_point(far, vp.rear_clipping_distance),
        }
    } else {
        EeGfxPickRay { front: vp.denormalize_orthographic_point(near), rear: vp.denormalize_orthographic_point(far) }
    }
}

/// `FindUnit`'s depth tolerance and one pixel's share of the window it picks against.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxPickTolerances {
    pub tolerance: f32,
    pub pixel: f32,
}

/// The tolerances: three over the clipping range; the window's width (1024 for the free camera).
pub fn tolerances(vp: &GeViewport, free_camera: bool, window_width: u32) -> EeGfxPickTolerances {
    let tolerance = PICK_TOLERANCE / (vp.rear_clipping_distance - vp.front_clipping_distance);
    let width = if free_camera { FREE_CAMERA_PICK_WIDTH } else { window_width as f32 };
    EeGfxPickTolerances { tolerance, pixel: 1.0 / width }
}

/// The bounding test's scale of an entry of priority `kind` (`false` priority) whose own scale is `saved`.
pub fn pick_scale(kind: i32, saved: f32) -> f32 {
    (if kind != 2 { PICK_SCALE } else { TREE_PICK_SCALE }) * saved
}

/// A bounding hit (not exact) is "large" when its model covers more than 0.17 of the window, or it is a tree.
pub fn is_large(pixel_size: f32, pixel: f32, kind: i32) -> bool {
    pixel_size * pixel > LARGE_HIT_FRACTION || kind == 2
}

/// `EEUnitPickerHit` (0x14 bytes).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxPickHit {
    /// along the ray
    pub distance: f32,
    /// the priority with the caller's resource preference
    pub priority: u32,
    /// the priority without it
    pub kind: i32,
    /// into the picker's entries
    pub index: u32,
    /// the exact (LOD 2) test hit
    pub exact: bool,
    /// big on the screen (or a tree)
    pub large: bool,
}

/// Whether hit `i` replaces the best of its class so far (`best`). Within the tolerance (or always, when hidden
/// units show) a higher priority wins, and an equal one when nearer; outside it, the nearer.
fn better(hits: &[EeGfxPickHit], i: usize, best: Option<usize>, tolerance: f32, show_hidden: bool) -> bool {
    let Some(b) = best else { return true };
    let delta = hits[i].distance - hits[b].distance;
    if (-tolerance <= delta && delta <= tolerance) || show_hidden {
        if hits[i].priority <= hits[b].priority {
            if delta >= 0.0 {
                return false;
            }
            if hits[i].priority != hits[b].priority {
                return false;
            }
        }
    } else if delta >= 0.0 {
        return false;
    }
    true
}

/// `FindUnit`'s ranking: the best exact hit and the best bounding hit that is not large, walking the hits from the
/// last (`(first, second)`, indices into `hits`).
pub fn rank(hits: &[EeGfxPickHit], tolerance: f32, show_hidden: bool) -> (Option<usize>, Option<usize>) {
    let mut first = None;
    let mut second = None;
    for i in (0..hits.len()).rev() {
        if hits[i].large {
            continue;
        }
        if !hits[i].exact {
            if better(hits, i, second, tolerance, show_hidden) {
                second = Some(i);
            }
        } else if better(hits, i, first, tolerance, show_hidden) {
            first = Some(i);
        }
    }
    (first, second)
}

/// `FindUnit`'s choice: the exact hit, unless a selection is held (`group_nonempty`, asked only when both kinds hit)
/// and the bounding hit's priority is higher; else whichever hit.
pub fn choose(
    hits: &[EeGfxPickHit],
    first: Option<usize>,
    second: Option<usize>,
    group_nonempty: impl FnOnce() -> bool,
) -> Option<usize> {
    match (first, second) {
        (Some(f), Some(s)) => {
            if group_nonempty() && hits[f].priority < hits[s].priority {
                Some(s)
            } else {
                Some(f)
            }
        }
        (None, Some(s)) => Some(s),
        (f, None) => f,
    }
}

/// What `GetUnitPickPriority` reads of a unit (its class's virtual `IsLandBuilding` and the current player's
/// `IsAlly` are asked, in the C++'s order, through the questions).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxPickUnit {
    /// `GetComplexUnit() != NULL`
    pub complex: bool,
    /// `mPlayer == gWorld.mPlayers[0]`
    pub gaia: bool,
    /// the class's `mIsWall`
    pub wall: bool,
    pub hit_points: i32,
    pub family: u32,
    pub category: u32,
    pub resource_type: u32,
    pub resource_amount: i32,
}

/// A question `GetUnitPickPriority` asks the C++.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGfxPickQuestion {
    /// `mUnitClass->IsLandBuilding()`
    LandBuilding = 0,
    /// `gWorld.GetCurrentPlayer()->IsAlly(GetPlayer())`
    Ally = 1,
}

/// `GetUnitPickPriority`: 1 land building or wall, 4 own or allied unit, 6 enemy unit or AA building, 2 wood, 3
/// stone, gold or another resource, 5 any resource left when preferring resources (not for citizens).
pub fn priority(u: &EeGfxPickUnit, prefer_resources: bool, mut ask: impl FnMut(EeGfxPickQuestion) -> bool) -> i32 {
    let mut result = 0;
    if u.complex && u.hit_points > 0 && !u.gaia {
        if u.family == FAMILY_AA_BUILDING {
            result = 6;
        } else if ask(EeGfxPickQuestion::LandBuilding) || u.wall {
            result = 1;
        } else if ask(EeGfxPickQuestion::Ally) {
            result = 4;
        } else {
            result = 6;
        }
    } else if u.resource_type == RESOURCE_WOOD {
        result = 2;
    } else if u.resource_type >= RESOURCE_STONE && u.resource_type < 7 {
        result = 3;
    }
    if prefer_resources
        && u.category != CATEGORY_LAND_CITIZEN
        && u.category != CATEGORY_NAVAL_CITIZEN
        && u.resource_type >= RESOURCE_FOOD
        && u.resource_type < 7
        && u.resource_amount > 0
    {
        result = 5;
    }
    result
}

/// What `AddUnit` reads of a unit and the picker.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxPickAdd {
    pub count: u32,
    pub capacity: u32,
    pub pixel_size: f32,
    pub unit_type: u32,
    pub category: u32,
    pub projectile: bool,
    pub free_camera: bool,
}

/// `AddUnit`'s admission: room left, at least 0.35 pixels, no path point, and (unless the camera is free) no
/// ambient unit or projectile other than an ICBM.
pub fn admits(a: &EeGfxPickAdd) -> bool {
    !(!(a.count < a.capacity)
        || !(a.pixel_size >= MIN_PICK_PIXEL_SIZE)
        || a.unit_type == TYPE_PATH_POINT
        || (!a.free_camera && (a.category == CATEGORY_AMBIENT || (a.projectile && a.unit_type != TYPE_CP_ICBM))))
}

/// `GetIsEntryInView`'s first test: which of the side planes 2..5 (`planes[0..4]`) the model's centre lies below.
pub fn sphere_below(center: Ge3DPoint, planes: &[Ge3DPlane; 4]) -> [bool; 4] {
    let mut below = [false; 4];
    for (b, p) in below.iter_mut().zip(planes) {
        *b = p.is_point_below(center);
    }
    below
}

/// Its second: of the planes the centre is below, how many the bounding sphere does not reach (0: the model's
/// vertices decide).
pub fn sphere_outside(center: Ge3DPoint, planes: &[Ge3DPlane; 4], below: &[bool; 4], radius: f32) -> u32 {
    let mut outside = below.iter().filter(|b| **b).count() as u32;
    for (p, b) in planes.iter().zip(below) {
        if !b {
            continue;
        }
        let offset = GeVector { x: center.x - p.point.x, y: center.y - p.point.y, z: center.z - p.point.z };
        let mut distance = offset.dot(p.normal);
        if distance < 0.0 {
            distance = -distance;
        }
        if distance <= radius {
            outside -= 1;
        }
    }
    outside
}

/// The LOD whose vertices `GetIsEntryInView` walks: three quarters of the count, rounded, the last at most.
pub fn view_lod(lod_count: u32) -> u32 {
    let lod = lrintf((1.0 - VIEW_LOD_FRACTION) * lod_count as f32) as u32;
    if lod == lod_count {
        lod_count.wrapping_sub(1)
    } else {
        lod
    }
}

/// `GetIsModelInView`'s vertex test: on or above every side plane.
pub fn point_in_view(planes: &[Ge3DPlane; 4], point: Ge3DPoint) -> bool {
    planes.iter().all(|p| p.is_point_on_or_above(point))
}

/// `GetIsEntryInView`'s model transformation for the box selection's frustum: the entry's `location` composed with
/// the frame's internal transform, scaled by the pick scale about the entry's own position when that is not 1.
pub fn entry_transform(location: &GeTransformation, internal: &GeTransformation, scale: f32) -> GeTransformation {
    let mut t = location.compose(internal);
    if scale != 1.0 {
        t.scale(scale, scale, scale);
        t.m[0][3] = location.m[0][3];
        t.m[1][3] = location.m[1][3];
        t.m[2][3] = location.m[2][3];
    }
    t
}

#[cfg(test)]
mod tests {
    use super::*;

    fn hit(distance: f32, priority: u32, exact: bool) -> EeGfxPickHit {
        EeGfxPickHit { distance, priority, kind: priority as i32, exact, ..Default::default() }
    }

    #[test]
    fn an_entrys_scale_keeps_its_position() {
        let mut location = GeTransformation::IDENTITY;
        location.m[0][3] = 3.0;
        location.m[1][3] = 4.0;
        let mut internal = GeTransformation::IDENTITY;
        internal.m[2][3] = 1.0;
        let t = entry_transform(&location, &internal, 1.0);
        assert_eq!((t.m[0][3], t.m[1][3], t.m[2][3]), (3.0, 4.0, 1.0));
        let s = entry_transform(&location, &internal, 2.0);
        assert_eq!((s.m[0][0], s.m[0][3], s.m[1][3], s.m[2][3]), (2.0, 3.0, 4.0, 0.0));
    }

    #[test]
    fn exact_hits_beat_bounding_ones_unless_a_selection_prefers() {
        let hits = [hit(5.0, 6, false), hit(9.0, 4, true)];
        let (f, s) = rank(&hits, 0.1, false);
        assert_eq!((f, s), (Some(1), Some(0)));
        assert_eq!(choose(&hits, f, s, || false), Some(1));
        assert_eq!(choose(&hits, f, s, || true), Some(0));
        // the question is asked only with both kinds
        assert_eq!(choose(&hits, None, s, || panic!("asked")), Some(0));
    }

    #[test]
    fn near_ties_go_to_the_priority() {
        let hits = [hit(5.0, 1, false), hit(5.05, 6, false), hit(3.0, 2, false)];
        // walking from the last: 3.0 first; 5.05 is farther by more than the tolerance; 5.0 too
        assert_eq!(rank(&hits, 0.1, false).1, Some(2));
        // a wide tolerance: 5.05's priority 6 beats 3.0's 2, and 5.0's 1 does not beat it
        assert_eq!(rank(&hits, 10.0, false).1, Some(1));
        let mut large = hits;
        large[1].large = true;
        assert_eq!(rank(&large, 10.0, false).1, Some(2));
    }

    #[test]
    fn priorities() {
        let mut u = EeGfxPickUnit { complex: true, hit_points: 10, ..Default::default() };
        let mut asked = vec![];
        assert_eq!(
            priority(&u, false, |q| {
                asked.push(q);
                q == EeGfxPickQuestion::Ally
            }),
            4
        );
        assert_eq!(asked, [EeGfxPickQuestion::LandBuilding, EeGfxPickQuestion::Ally]);
        u.family = FAMILY_AA_BUILDING;
        assert_eq!(priority(&u, false, |_| panic!("asked")), 6);
        let tree = EeGfxPickUnit { resource_type: RESOURCE_WOOD, resource_amount: 5, ..Default::default() };
        assert_eq!(priority(&tree, false, |_| false), 2);
        assert_eq!(priority(&tree, true, |_| false), 5);
    }

    #[test]
    fn lods_and_admission() {
        assert_eq!(view_lod(4), 3);
        assert_eq!(view_lod(1), 0);
        assert_eq!(view_lod(2), 1);
        let mut a = EeGfxPickAdd { count: 0, capacity: 4, pixel_size: 1.0, ..Default::default() };
        assert!(!admits(&a));
        a.category = 3;
        assert!(admits(&a));
        a.pixel_size = 0.3;
        assert!(!admits(&a));
    }
}
