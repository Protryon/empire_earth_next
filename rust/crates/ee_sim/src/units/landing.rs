//! Two of EEWheel.cpp's unit helpers (the units' lane, round 5): a flying unit's landing point
//! (`EEFindLandingPoint`: a target the unit can't reach is replaced with its position when that is reachable, else
//! with a reachable spot up to 9 tiles away, halved toward the target to within half a tile; the unit's position when
//! none is found; the reach test is the continent manager's `CanGetTo`, the AI lane's C++ through the unit utilities'
//! host) and a fleeing unit's point ahead (`EEGetRandomPointAhead`: 4..12 tiles from the unit along its
//! goal-to-unit heading, +-20 degrees, on its land continent, up to 13 tries).

use ee_base::math::um::{floor as um_floor, um_cos};

/// `EEFindLandingPoint(unit, ioTarget)` from the unit's location `at`; `can` is `CanGetTo(unit, x, y)`.
pub fn find_landing_point(can: &mut dyn FnMut(i32, i32) -> bool, at: [f32; 2], target: &mut [f32; 3]) {
    // (the casts are the C++'s (long) truncations; the candidates' are UMFloor)
    if can(target[0] as i32, target[1] as i32) {
        return;
    }
    let (mut x, mut y) = (at[0], at[1]);
    let (mut tx, mut ty) = (target[0], target[1]);
    let mut found = false;
    if !can(um_floor(x), um_floor(y)) {
        let mut radius = 1.0f32;
        while radius < 10.0 && !found {
            'around: for dx in -1i32..=1 {
                for dy in -1i32..=1 {
                    if dx == 0 && dy == 0 {
                        continue;
                    }
                    let cy = dy as f32 * radius + y;
                    let cx = dx as f32 * radius + x;
                    if !can(um_floor(cx), um_floor(cy)) {
                        continue;
                    }
                    found = true;
                    x = cx;
                    y = cy;
                    break 'around;
                }
            }
            radius += 1.0;
        }
        if found {
            while !(abs(ty - y) < 0.5 && abs(tx - x) < 0.5) {
                let mx = (tx + x) * 0.5;
                let my = (ty + y) * 0.5;
                if can(mx as i32, my as i32) {
                    x = mx;
                    y = my;
                } else {
                    tx = mx;
                    ty = my;
                }
            }
        }
    }
    target[0] = x;
    target[1] = y;
}

/// What `EEGetRandomPointAhead` asks of the world, in the original's order.
pub trait AheadWorld {
    /// `EEHeadingBetween(from, to)` then `EEJitterHeading(heading, -20, 20)` (the unit utilities', noted)
    fn heading(&mut self, from: [f32; 3], to: [f32; 3]) -> f32;
    /// `gContinentManager.GetContinent(tile, kAIPathLandMass)`
    fn land_mass(&mut self, x: i32, y: i32) -> u16;
    /// `gPhysics->GetRandom(4.0f, 12.0f)`
    fn radius(&mut self) -> f32;
    /// `gMapWidth`, `gMapHeight`
    fn map_size(&mut self) -> (u32, u32);
}

/// `kAIPathLandMass`
pub const LAND_MASS: i32 = 2;

/// `EEGetRandomPointAhead(unit, outPoint)`: the unit's goal location (None: no goal), position and tile -> whether
/// found and the point (its position when not; the zero point without a goal).
pub fn point_ahead(
    w: &mut dyn AheadWorld,
    goal: Option<[f32; 3]>,
    position: [f32; 3],
    tile: (i32, i32),
) -> (bool, [f32; 2]) {
    let Some(goal) = goal else { return (false, [0.0, 0.0]) };
    let angle = w.heading(goal, position);
    let continent = w.land_mass(tile.0, tile.1);
    let mut out = [0.0f32; 2];
    for _ in 0..13 {
        let radius = w.radius();
        out[0] = um_cos(angle) * radius + position[0];
        out[1] = um_cos(1.570_796_4 - angle) * radius + position[1];
        let (x, y) = (out[0] as i32, out[1] as i32);
        let (width, height) = w.map_size();
        if (x as u32) < width && (y as u32) < height && w.land_mass(x, y) == continent {
            return (true, out);
        }
    }
    (false, [position[0], position[1]])
}

/// `UMAbs`
fn abs(v: f32) -> f32 {
    if v < 0.0 {
        -v
    } else {
        v
    }
}

/// `EEFindLandingPoint(unit, ioTarget)` (EEWheel.cpp's forwarder).
///
/// # Safety
/// `unit` a live complex unit, `target` three floats.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_find_landing_point(unit: u64, target: *mut [f32; 3]) {
    let l = crate::units::unit_geom::with_unit(unit, |u| u.location()).expect("a live unit");
    // SAFETY: the caller's contract
    let target = unsafe { &mut *target };
    find_landing_point(&mut |x, y| crate::units::uu_host::can_get_to(unit, x, y), [l[0], l[1]], target);
}

#[cfg(not(test))]
struct Ahead;

#[cfg(not(test))]
impl AheadWorld for Ahead {
    fn heading(&mut self, from: [f32; 3], to: [f32; 3]) -> f32 {
        use crate::units::uu_ffi::{ee_units_uu_heading_between, ee_units_uu_jitter_heading};
        // SAFETY: two live points
        ee_units_uu_jitter_heading(unsafe { ee_units_uu_heading_between(&from, &to) }, -20.0, 20.0)
    }
    fn land_mass(&mut self, x: i32, y: i32) -> u16 {
        let r = crate::astar::world_view::cm().expect("the continent manager's record");
        // SAFETY: the registered record
        #[allow(clippy::unnecessary_cast)] // c_long is 64 bits off Windows
        unsafe {
            crate::astar::cm_queries::ee_astar_cmq_region_id(
                r,
                x as std::os::raw::c_long,
                y as std::os::raw::c_long,
                LAND_MASS as std::os::raw::c_long,
            )
        }
    }
    fn radius(&mut self) -> f32 {
        crate::units::uu_direct::with_game(|g| g.random_float(4.0, 12.0))
    }
    #[allow(clippy::unnecessary_cast)] // c_ulong is 64 bits off Windows
    fn map_size(&mut self) -> (u32, u32) {
        let r = crate::astar::world_view::cm().expect("the continent manager's record");
        (r.width as u32, r.height as u32)
    }
}

/// `EEGetRandomPointAhead(unit, outPoint)` (EEWheel.cpp's forwarder) -> found.
///
/// # Safety
/// `unit` a live complex unit, `out` two floats.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_random_point_ahead(unit: u64, out: *mut [f32; 2]) -> bool {
    let (goal, position, tile) = crate::units::unit_geom::with_unit(unit, |u| {
        let goal = u.goal_handle();
        let (x, y) = u.tile_xy();
        (goal, crate::units::unit_geom::position(u), (x as i32, y as i32))
    })
    .expect("a live unit");
    let location = (goal != 0).then(|| {
        let mut io = crate::units::rust_goal::EeUnitsGoalIo::default();
        // SAFETY: the unit's live goal; its location out in the floats
        unsafe { crate::units::goal_obj::call(crate::units::rust_goal::EE_UNITS_GOAL_LOCATION, goal, 0, 0, &mut io) };
        [io.f[0], io.f[1], io.f[2]]
    });
    let (found, p) = point_ahead(&mut Ahead, location, if goal != 0 { position } else { [0.0; 3] }, tile);
    // SAFETY: the caller's contract
    unsafe { *out = p };
    found
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_reachable_target_stays() {
        let mut t = [5.5, 6.5, 2.0];
        find_landing_point(&mut |_, _| true, [0.0, 0.0], &mut t);
        assert_eq!(t, [5.5, 6.5, 2.0]);
    }

    #[test]
    fn a_reachable_position_becomes_the_target() {
        // only the unit's own tile (1, 1) is reachable
        let mut t = [9.0, 9.0, 3.0];
        find_landing_point(&mut |x, y| (x, y) == (1, 1), [1.25, 1.75], &mut t);
        assert_eq!(t, [1.25, 1.75, 3.0]);
    }

    #[test]
    fn a_spot_nearby_is_halved_toward_the_target() {
        // x < 4 is reachable: the unit (x 4.5) and the target (x 8) are not; the first spot is at x 3.5
        let reach = |x: i32, _y: i32| x < 4;
        let mut t = [8.0, 5.0, 0.0];
        let mut calls = Vec::new();
        find_landing_point(
            &mut |x, y| {
                calls.push((x, y));
                reach(x, y)
            },
            [4.5, 5.0],
            &mut t,
        );
        // the first candidate: (-1, -1) at radius 1, x 3.5
        assert_eq!(calls[2], (3, 4));
        assert!((t[0] - 3.5).abs() < 0.5 + 1e-6 && t[0] < 4.0, "{t:?}");
        assert_eq!(t[2], 0.0);
    }

    struct Ahead {
        heading: f32,
        radii: Vec<f32>,
        land: Vec<((i32, i32), u16)>,
        asked: Vec<(i32, i32)>,
    }

    impl AheadWorld for Ahead {
        fn heading(&mut self, _from: [f32; 3], _to: [f32; 3]) -> f32 {
            self.heading
        }
        fn land_mass(&mut self, x: i32, y: i32) -> u16 {
            self.asked.push((x, y));
            self.land.iter().find(|l| l.0 == (x, y)).map_or(0, |l| l.1)
        }
        fn radius(&mut self) -> f32 {
            self.radii.remove(0)
        }
        fn map_size(&mut self) -> (u32, u32) {
            (64, 64)
        }
    }

    #[test]
    fn the_point_ahead_is_on_the_units_land() {
        // heading 0: along x; the unit at (10, 10) on land 3; the first try lands on land 4, the second on 3
        let mut w = Ahead {
            heading: 0.0,
            radii: vec![5.0, 8.0],
            land: vec![((10, 10), 3), ((15, 10), 4), ((18, 10), 3)],
            asked: Vec::new(),
        };
        let (found, p) = point_ahead(&mut w, Some([0.0; 3]), [10.5, 10.5, 0.0], (10, 10));
        assert!(found);
        assert_eq!(p[0], um_cos(0.0) * 8.0 + 10.5);
        assert_eq!(w.asked, vec![(10, 10), (15, 10), (18, 10)]);
    }

    #[test]
    fn no_goal_and_no_land_give_the_fallbacks() {
        let mut w = Ahead { heading: 0.0, radii: vec![], land: vec![], asked: Vec::new() };
        assert_eq!(point_ahead(&mut w, None, [3.0, 4.0, 0.0], (3, 4)), (false, [0.0, 0.0]));
        let mut w = Ahead { heading: 0.0, radii: vec![100.0; 13], land: vec![((3, 4), 1)], asked: Vec::new() };
        assert_eq!(point_ahead(&mut w, Some([0.0; 3]), [3.0, 4.0, 0.0], (3, 4)), (false, [3.0, 4.0]));
        assert_eq!(w.asked.len(), 1, "off the map: no land asked");
    }

    #[test]
    fn no_spot_leaves_the_position() {
        let mut t = [8.0, 5.0, 1.0];
        find_landing_point(&mut |_, _| false, [4.5, 5.0], &mut t);
        assert_eq!(t, [4.5, 5.0, 1.0]);
    }
}
