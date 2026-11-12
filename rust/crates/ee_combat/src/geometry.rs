//! The combat geometry (R5b.2, third round): `EEGetTileLine`, the tiles of a line between two tiles, and the weapon
//! tag point's height of the line of fire (`EEIsLineToUnitBlocked`'s start), over the model's data passed as plain
//! records. Pure: no world, no host.

use ee_ge::transform::RotationOrder;
use ee_ge::types::{Ge3DPoint, GeTransformation};

// ---- EEGetTileLine

/// `EEGetTileLine(x1, y1, x2, y2, &tiles, ends, thin)`: a symmetric double-step rasteriser that walks in from both
/// ends at once (four steps a pass), in order from (x1, y1). `ends`: the two end tiles too; `!thin`: the extra tiles
/// that make the line 4-connected. The arithmetic is the C++'s `long` (32-bit on the original), wrapping.
pub fn tile_line(x1: i32, y1: i32, x2: i32, y2: i32, ends: bool, thin: bool) -> Vec<(i32, i32)> {
    let (mut x1, mut y1, mut x2, mut y2) = (x1, y1, x2, y2);
    let sign_x: i32 = if x2.wrapping_sub(x1) >= 0 { 1 } else { -1 };
    let mut length_x = x2.wrapping_sub(x1).wrapping_mul(sign_x);
    let sign_y: i32 = if y2.wrapping_sub(y1) >= 0 { 1 } else { -1 };
    let mut length_y = y2.wrapping_sub(y1).wrapping_mul(sign_y);
    let step: i32 = if sign_x != sign_y { -1 } else { 1 };
    let steep = length_y > length_x;
    if steep {
        std::mem::swap(&mut x1, &mut y1);
        std::mem::swap(&mut x2, &mut y2);
        std::mem::swap(&mut length_x, &mut length_y);
    }
    let (mut x, mut y, mut end_x, mut end_y, forward) =
        if x1 > x2 { (x2, y2, x1, y1, false) } else { (x1, y1, x2, y2, true) };
    // the point in the line's own axes, back in the map's (the C++'s `theSteep ? (y, x) : (x, y)`)
    let pt = |a: i32, b: i32| if steep { (b, a) } else { (a, b) };
    let quads = length_x.wrapping_sub(1) / 4;
    let left = length_x.wrapping_sub(1) % 4;
    let mut front: Vec<(i32, i32)> = Vec::new();
    let mut back: Vec<(i32, i32)> = Vec::new();
    if ends {
        front.push(pt(x, y));
        back.push(pt(end_x, end_y));
    }
    if !thin && left == 0 && quads == 0 && length_x == 1 && length_y == 1 {
        front.push(pt(x.wrapping_add(1), y));
    }
    let half = length_y.wrapping_mul(2);
    let increment2 = half.wrapping_sub(length_x).wrapping_mul(2);
    if left >= 0 {
        if increment2 < 0 {
            let increment1 = length_y.wrapping_mul(4);
            let mut d = increment1.wrapping_sub(length_x);
            let mut i = 0;
            while i < quads {
                let last = i == quads.wrapping_sub(1) && left == 0 && !thin;
                x = x.wrapping_add(1);
                end_x = end_x.wrapping_sub(1);
                if d < 0 {
                    front.push(pt(x, y));
                    x = x.wrapping_add(1);
                    front.push(pt(x, y));
                    back.push(pt(end_x, end_y));
                    end_x = end_x.wrapping_sub(1);
                    back.push(pt(end_x, end_y));
                    if last {
                        front.push(pt(x.wrapping_add(1), y));
                    }
                    d = d.wrapping_add(increment1);
                } else {
                    if d < half {
                        front.push(pt(x, y));
                        if !thin {
                            front.push(pt(x, y.wrapping_add(step)));
                        }
                        y = y.wrapping_add(step);
                        x = x.wrapping_add(1);
                        front.push(pt(x, y));
                        back.push(pt(end_x, end_y));
                        if !thin {
                            back.push(pt(end_x, end_y.wrapping_sub(step)));
                        }
                        end_y = end_y.wrapping_sub(step);
                        end_x = end_x.wrapping_sub(1);
                        back.push(pt(end_x, end_y));
                        if last {
                            front.push(pt(x.wrapping_add(1), y));
                        }
                    } else {
                        if !thin {
                            front.push(pt(x, y));
                        }
                        y = y.wrapping_add(step);
                        front.push(pt(x, y));
                        x = x.wrapping_add(1);
                        front.push(pt(x, y));
                        if !thin {
                            back.push(pt(end_x, end_y));
                        }
                        end_y = end_y.wrapping_sub(step);
                        back.push(pt(end_x, end_y));
                        end_x = end_x.wrapping_sub(1);
                        back.push(pt(end_x, end_y));
                        if last {
                            front.push(pt(x.wrapping_add(1), y));
                        }
                    }
                    d = d.wrapping_add(increment2);
                }
                i += 1;
            }
            if left != 0 {
                let (x1, x2, ex1) = (x.wrapping_add(1), x.wrapping_add(2), end_x.wrapping_sub(1));
                if d < 0 {
                    front.push(pt(x1, y));
                    if left > 1 {
                        front.push(pt(x2, y));
                    }
                    if left > 2 {
                        back.push(pt(ex1, end_y));
                    }
                } else if d < half {
                    front.push(pt(x1, y));
                    if left > 1 {
                        if !thin {
                            front.push(pt(x1, y.wrapping_add(step)));
                        }
                        front.push(pt(x2, y.wrapping_add(step)));
                    }
                    if left > 2 {
                        back.push(pt(ex1, end_y));
                    }
                } else {
                    if !thin {
                        front.push(pt(x1, y));
                    }
                    y = y.wrapping_add(step);
                    front.push(pt(x1, y));
                    if left > 1 {
                        front.push(pt(x2, y));
                    }
                    if left > 2 {
                        back.push(pt(ex1, end_y.wrapping_sub(step)));
                    }
                }
            }
        } else {
            let half2 = length_y.wrapping_sub(length_x).wrapping_mul(2);
            let mut d = length_x.wrapping_add(length_y.wrapping_sub(length_x).wrapping_mul(4));
            let mut i = 0;
            while i < quads {
                let last = i == quads.wrapping_sub(1) && left == 0 && !thin;
                x = x.wrapping_add(1);
                end_x = end_x.wrapping_sub(1);
                if d < 1 {
                    if d < half2 {
                        front.push(pt(x, y));
                        if !thin {
                            front.push(pt(x.wrapping_add(1), y));
                        }
                        y = y.wrapping_add(step);
                        x = x.wrapping_add(1);
                        front.push(pt(x, y));
                        back.push(pt(end_x, end_y));
                        if !thin {
                            back.push(pt(end_x.wrapping_sub(1), end_y.wrapping_sub(step)));
                        }
                        end_y = end_y.wrapping_sub(step);
                        end_x = end_x.wrapping_sub(1);
                        back.push(pt(end_x, end_y));
                        if last {
                            front.push(pt(x.wrapping_add(1), y));
                        }
                    } else {
                        if !thin {
                            front.push(pt(x, y));
                        }
                        y = y.wrapping_add(step);
                        front.push(pt(x, y));
                        x = x.wrapping_add(1);
                        front.push(pt(x, y));
                        if !thin {
                            back.push(pt(end_x, end_y));
                        }
                        end_y = end_y.wrapping_sub(step);
                        back.push(pt(end_x, end_y));
                        end_x = end_x.wrapping_sub(1);
                        back.push(pt(end_x, end_y));
                        if last {
                            front.push(pt(x.wrapping_add(1), y));
                        }
                    }
                    d = d.wrapping_add(increment2);
                } else {
                    if !thin {
                        front.push(pt(x, y));
                    }
                    y = y.wrapping_add(step);
                    front.push(pt(x, y));
                    if !thin {
                        front.push(pt(x, y.wrapping_add(step)));
                    }
                    y = y.wrapping_add(step);
                    x = x.wrapping_add(1);
                    front.push(pt(x, y));
                    if !thin {
                        back.push(pt(end_x, end_y));
                    }
                    end_y = end_y.wrapping_sub(step);
                    back.push(pt(end_x, end_y));
                    if !thin {
                        back.push(pt(end_x, end_y.wrapping_sub(step)));
                    }
                    end_y = end_y.wrapping_sub(step);
                    end_x = end_x.wrapping_sub(1);
                    back.push(pt(end_x, end_y));
                    d = d.wrapping_add(length_y.wrapping_sub(length_x).wrapping_mul(4));
                    if last {
                        front.push(pt(x.wrapping_add(1), y));
                    }
                }
                i += 1;
            }
            if left != 0 {
                let (x1, x2, ex1) = (x.wrapping_add(1), x.wrapping_add(2), end_x.wrapping_sub(1));
                if d < 1 {
                    if d < half2 {
                        front.push(pt(x1, y));
                        if left > 1 {
                            if !thin {
                                front.push(pt(x1, y.wrapping_add(step)));
                            }
                            front.push(pt(x2, y.wrapping_add(step)));
                        }
                        if left > 2 {
                            back.push(pt(ex1, end_y));
                        }
                    } else {
                        if !thin {
                            front.push(pt(x1, y));
                        }
                        y = y.wrapping_add(step);
                        front.push(pt(x1, y));
                        if left > 1 {
                            front.push(pt(x2, y));
                            if !thin {
                                front.push(pt(x2, y.wrapping_add(step)));
                            }
                        }
                        if left > 2 {
                            if d > half2 {
                                if !thin {
                                    back.push(pt(ex1, end_y));
                                }
                                back.push(pt(ex1, end_y.wrapping_sub(step)));
                            } else {
                                back.push(pt(ex1, end_y));
                            }
                        }
                    }
                } else {
                    if !thin {
                        front.push(pt(x1, y));
                    }
                    y = y.wrapping_add(step);
                    front.push(pt(x1, y));
                    if !thin {
                        front.push(pt(x2, y));
                    }
                    if left > 1 {
                        front.push(pt(x2, y.wrapping_add(step)));
                        if !thin {
                            front.push(pt(x2, y.wrapping_add(step).wrapping_add(step)));
                        }
                        if left > 2 {
                            back.push(pt(ex1, end_y.wrapping_sub(step)));
                        }
                    }
                }
            }
        }
    }
    let (mut out, rest) = if forward { (front, back) } else { (back, front) };
    out.extend(rest.iter().rev());
    out
}

// ---- the weapon tag point's height (EEIsLineToUnitBlocked's start)

/// The start height's case, as the C++ finds it.
/// Bare (a melee shooter): its location's z.
pub const EE_COMBAT_WEAPON_KIND_BARE: i32 = 0;
/// A model with turret parts: the tag point turned by its part's turret yaw (identity when the point's model is no
/// turret part), up the model chain, then the scaled location.
pub const EE_COMBAT_WEAPON_KIND_TURRET: i32 = 1;
/// The tag point list's point: up the model chain, then the scaled location.
pub const EE_COMBAT_WEAPON_KIND_POINT: i32 = 2;
/// No such point: half the height of the model's box's top right back corner, under the (unscaled) location.
pub const EE_COMBAT_WEAPON_KIND_BOX: i32 = 3;
/// An effect's start without a tag point list (`EEE2DLinearProjectile`, `EEE3DPulseCannonBall`, `EEE3DTracers`): the
/// model's default point (in `top`) under the unscaled location.
pub const EE_COMBAT_WEAPON_KIND_DEFAULT: i32 = 4;

/// The weapon tag point's inputs (the shooter's model data at its frame). The model chain's internal transforms (the
/// point's model first, then each parent) are passed beside it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatWeaponIn {
    /// `WEAPON_KIND_*`
    pub kind: i32,
    /// the animation's tag point list exists (else the height stays the location's z)
    pub list_present: i32,
    /// the shooter's location (`mMatrix`, row by row)
    pub location: [f32; 16],
    /// its physical model's scale
    pub scale: f32,
    /// the point's model is a turret part (`FindTurretPart` found it): `yaw` is its turret angle
    pub turret: i32,
    pub yaw: f32,
    /// the tag point at the frame, in its model's space
    pub point: [f32; 3],
    /// the box's top right back corner (`EE_COMBAT_WEAPON_KIND_BOX`), the model's default point
    /// (`EE_COMBAT_WEAPON_KIND_DEFAULT`)
    pub top: [f32; 3],
    /// the number of model chain transforms
    pub chain: i32,
}

pub(crate) fn matrix(m: &[f32; 16]) -> GeTransformation {
    GeTransformation {
        m: [
            [m[0], m[1], m[2], m[3]],
            [m[4], m[5], m[6], m[7]],
            [m[8], m[9], m[10], m[11]],
            [m[12], m[13], m[14], m[15]],
        ],
    }
}

/// `EETagPoint`'s walk up the model chain: each model's internal transform at the frame, the point's model first.
pub(crate) fn up_the_chain(mut point: Ge3DPoint, chain: &[[f32; 16]]) -> Ge3DPoint {
    for link in chain {
        point = matrix(link).apply_point(point);
    }
    point
}

/// The weapon tag point's world point, as the line of fire's start (`EELineOfFireStartHeight`) and the effects'
/// starts (`EEE2DLinearProjectile`, `EEE2DMuzzleFlash`, `EEE2DSmokeMuzzleBlast`, `EEE3DPulseCannonBall`,
/// `EEE3DTracers`) compute it: the tag point (turned by its turret part's yaw on a model with parts) up the model
/// chain under the location scaled by `EETagPointList::Scale`; the box's or the default point under the unscaled
/// location. The bare kind's is the location's position.
pub fn weapon_point(i: &EeCombatWeaponIn, chain: &[[f32; 16]]) -> Ge3DPoint {
    let location = matrix(&i.location);
    // `EETagPointList::Scale`: the orientation rows scaled, the position kept
    let mut scaled = location;
    for row in scaled.m.iter_mut().take(3) {
        for value in row.iter_mut().take(3) {
            *value *= i.scale;
        }
    }
    let local = Ge3DPoint { x: i.point[0], y: i.point[1], z: i.point[2] };
    match i.kind {
        EE_COMBAT_WEAPON_KIND_BARE => Ge3DPoint { x: location.x(), y: location.y(), z: location.z() },
        EE_COMBAT_WEAPON_KIND_TURRET => {
            // `GETransformation()`, `MakeIdentity`, then `SetOrientationYPR(angle, 0, 0)` for a turret part
            let mut part = GeTransformation::IDENTITY;
            if i.turret != 0 {
                part.set_orientation(RotationOrder::Ypr, i.yaw, 0.0, 0.0);
            }
            scaled.apply_point(up_the_chain(part.apply_point(local), chain))
        }
        EE_COMBAT_WEAPON_KIND_POINT => scaled.apply_point(up_the_chain(local, chain)),
        EE_COMBAT_WEAPON_KIND_DEFAULT => location.apply_point(Ge3DPoint { x: i.top[0], y: i.top[1], z: i.top[2] }),
        _ => location.apply_point(Ge3DPoint { x: i.top[0], y: i.top[1], z: i.top[2] * 0.5 }),
    }
}

/// The shot's start height of `EEIsLineToUnitBlocked` (`EELineOfFireStartHeight`): the weapon tag point's world z
/// (the location's without a tag point list).
pub fn weapon_height(i: &EeCombatWeaponIn, chain: &[[f32; 16]]) -> f32 {
    let height = matrix(&i.location).z();
    if i.kind == EE_COMBAT_WEAPON_KIND_BARE || i.list_present == 0 {
        return height;
    }
    weapon_point(i, chain).z
}

// ---- the C ABI

/// `EEGetTileLine`: writes up to `capacity` (x, y) pairs to `out` and returns the line's tile count (call again with
/// a larger buffer when it exceeds `capacity`).
///
/// # Safety
/// `out` must point to `capacity` pairs (or be NULL with `capacity` 0).
#[no_mangle]
pub unsafe extern "C" fn ee_combat_tile_line(
    x1: i32,
    y1: i32,
    x2: i32,
    y2: i32,
    ends: i32,
    thin: i32,
    out: *mut [i32; 2],
    capacity: u32,
) -> u32 {
    let tiles = tile_line(x1, y1, x2, y2, ends != 0, thin != 0);
    if !out.is_null() {
        for (n, &(x, y)) in tiles.iter().take(capacity as usize).enumerate() {
            unsafe { *out.add(n) = [x, y] };
        }
    }
    tiles.len() as u32
}

/// The line of fire's start height (the weapon tag point's world z) over `input.chain` model transforms.
///
/// # Safety
/// `input` must point to a valid record and `chain` to `input.chain` matrices (or be NULL when it is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_combat_weapon_height(input: *const EeCombatWeaponIn, chain: *const [f32; 16]) -> f32 {
    let input = unsafe { &*input };
    let chain: &[[f32; 16]] = if chain.is_null() || input.chain <= 0 {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(chain, input.chain as usize) }
    };
    weapon_height(input, chain)
}

/// The weapon tag point's world point (`weapon_point`) into `out` (three floats).
///
/// # Safety
/// `input` must point to a valid record, `chain` to `input.chain` matrices (or be NULL when it is 0) and `out` to
/// three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_weapon_point(
    input: *const EeCombatWeaponIn,
    chain: *const [f32; 16],
    out: *mut f32,
) {
    let input = unsafe { &*input };
    let chain: &[[f32; 16]] = if chain.is_null() || input.chain <= 0 {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(chain, input.chain as usize) }
    };
    let p = weapon_point(input, chain);
    let out = unsafe { std::slice::from_raw_parts_mut(out, 3) };
    out.copy_from_slice(&[p.x, p.y, p.z]);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tile_line_straight_and_diagonal() {
        assert_eq!(tile_line(0, 0, 5, 0, false, true), vec![(1, 0), (2, 0), (3, 0), (4, 0)]);
        assert_eq!(tile_line(0, 0, 5, 0, true, true), vec![(0, 0), (1, 0), (2, 0), (3, 0), (4, 0), (5, 0)]);
        // from the far end: the same tiles in the other order
        assert_eq!(tile_line(5, 0, 0, 0, false, true), vec![(4, 0), (3, 0), (2, 0), (1, 0)]);
        // steep: the axes swapped
        assert_eq!(tile_line(0, 0, 0, 5, false, true), vec![(0, 1), (0, 2), (0, 3), (0, 4)]);
        assert_eq!(tile_line(2, 2, 2, 2, false, true), Vec::<(i32, i32)>::new());
        assert_eq!(tile_line(2, 2, 2, 2, true, true), vec![(2, 2), (2, 2)]);
    }

    #[test]
    fn tile_line_repeats_the_meeting_tile_as_the_original() {
        // (0, 0) to (-9, -8), 4-connected: two passes from each end, the front's closing tile is the back's last
        let line = tile_line(0, 0, -9, -8, true, false);
        assert_eq!(line[8], (-4, -4));
        assert_eq!(line[9], (-4, -4));
    }

    /// The golden of the unmodified C++ (`EEGetTileLine` compiled alone in a scratch driver, round 3): every line
    /// from (-3..3, -3..3) to (-12..12, -12..12), with and without the ends, thin and 4-connected, one text line
    /// each ("x1 y1 x2 y2 ends thin: x,y ..."); 122500 lines, 8451628 bytes, this FNV-1a.
    #[test]
    fn tile_line_equals_the_cpp_over_a_grid() {
        let mut text = String::new();
        for x1 in -3..=3 {
            for y1 in -3..=3 {
                for x2 in -12..=12 {
                    for y2 in -12..=12 {
                        for ends in [false, true] {
                            for thin in [false, true] {
                                text += &format!("{x1} {y1} {x2} {y2} {} {}:", ends as i32, thin as i32);
                                for (x, y) in tile_line(x1, y1, x2, y2, ends, thin) {
                                    text += &format!(" {x},{y}");
                                }
                                text.push('\n');
                            }
                        }
                    }
                }
            }
        }
        let mut h: u64 = 0xcbf29ce484222325;
        for b in text.bytes() {
            h = (h ^ b as u64).wrapping_mul(0x100000001b3);
        }
        assert_eq!((text.len(), h), (8451628, 0x0bb0fc58d51222a5));
    }

    fn weapon(kind: i32) -> EeCombatWeaponIn {
        let mut location = [0.0f32; 16];
        // a turn of 90 degrees about z, at (10, 20, 3)
        location[1] = -1.0;
        location[3] = 10.0;
        location[4] = 1.0;
        location[7] = 20.0;
        location[10] = 1.0;
        location[11] = 3.0;
        location[15] = 1.0;
        EeCombatWeaponIn {
            kind,
            list_present: 1,
            location,
            scale: 2.0,
            turret: 0,
            yaw: 0.0,
            point: [1.0, 0.0, 0.5],
            top: [0.0, 0.0, 4.0],
            chain: 0,
        }
    }

    #[test]
    fn weapon_height_cases() {
        assert_eq!(weapon_height(&weapon(EE_COMBAT_WEAPON_KIND_BARE), &[]), 3.0);
        // the point scaled by 2 under the location: z = 0.5 * 2 + 3
        assert_eq!(weapon_height(&weapon(EE_COMBAT_WEAPON_KIND_POINT), &[]), 4.0);
        // a chain link lifting the point by 1 first
        let mut lift = [0.0f32; 16];
        lift[0] = 1.0;
        lift[5] = 1.0;
        lift[10] = 1.0;
        lift[11] = 1.0;
        lift[15] = 1.0;
        let mut w = weapon(EE_COMBAT_WEAPON_KIND_POINT);
        w.chain = 1;
        assert_eq!(weapon_height(&w, &[lift]), 6.0);
        // a turret's yaw does not change the height of a point on the axis
        let mut w = weapon(EE_COMBAT_WEAPON_KIND_TURRET);
        w.turret = 1;
        w.yaw = 1.0;
        assert_eq!(weapon_height(&w, &[]), 4.0);
        // the box: half the top, unscaled
        assert_eq!(weapon_height(&weapon(EE_COMBAT_WEAPON_KIND_BOX), &[]), 5.0);
        // no list: the location's height
        let mut w = weapon(EE_COMBAT_WEAPON_KIND_BOX);
        w.list_present = 0;
        assert_eq!(weapon_height(&w, &[]), 3.0);
    }

    #[test]
    fn weapon_point_cases() {
        // the point (1, 0, 0.5) under the location scaled by 2 (its rows, not its position): (10, 22, 4)
        let p = weapon_point(&weapon(EE_COMBAT_WEAPON_KIND_POINT), &[]);
        assert_eq!((p.x, p.y, p.z), (10.0, 22.0, 4.0));
        // a turret part turned a quarter (x to y) first: (0, 1, 0.5) -> (8, 20, 4)
        let mut w = weapon(EE_COMBAT_WEAPON_KIND_TURRET);
        w.turret = 1;
        w.yaw = std::f32::consts::FRAC_PI_2;
        let p = weapon_point(&w, &[]);
        assert!((p.x - 8.0).abs() < 1e-5 && (p.y - 20.0).abs() < 1e-5 && p.z == 4.0, "{p:?}");
        // a model with parts but no turret part: the identity part
        let p = weapon_point(&weapon(EE_COMBAT_WEAPON_KIND_TURRET), &[]);
        assert_eq!((p.x, p.y, p.z), (10.0, 22.0, 4.0));
        // the default point under the unscaled location: (0, 0, 4) -> (10, 20, 7); the bare kind: the position
        let p = weapon_point(&weapon(EE_COMBAT_WEAPON_KIND_DEFAULT), &[]);
        assert_eq!((p.x, p.y, p.z), (10.0, 20.0, 7.0));
        let p = weapon_point(&weapon(EE_COMBAT_WEAPON_KIND_BARE), &[]);
        assert_eq!((p.x, p.y, p.z), (10.0, 20.0, 3.0));
        // the C ABI
        let w = weapon(EE_COMBAT_WEAPON_KIND_POINT);
        let mut out = [0.0f32; 3];
        // SAFETY: a valid record, no chain, three floats
        unsafe { ee_combat_weapon_point(&w, std::ptr::null(), out.as_mut_ptr()) };
        assert_eq!(out, [10.0, 22.0, 4.0]);
    }

    #[test]
    fn the_capture_replays_effect_points() {
        use crate::capture::{replay, tag, Record};
        let w = weapon(EE_COMBAT_WEAPON_KIND_POINT);
        let mut p: Vec<u8> = 3i32.to_le_bytes().to_vec();
        // SAFETY: a plain record of 4-byte fields
        p.extend_from_slice(unsafe {
            std::slice::from_raw_parts(
                (&w as *const EeCombatWeaponIn).cast::<u8>(),
                std::mem::size_of::<EeCombatWeaponIn>(),
            )
        });
        for v in [10.0f32, 22.0, 4.0] {
            p.extend_from_slice(&v.to_le_bytes());
        }
        assert_eq!(replay(&Record { tag: tag(b"CBEW"), payload: &p }), Some(Ok(true)));
        let n = p.len();
        p[n - 1] ^= 1;
        assert_eq!(replay(&Record { tag: tag(b"CBEW"), payload: &p }), Some(Ok(false)));
        assert!(matches!(replay(&Record { tag: tag(b"CBEW"), payload: &p[..n - 4] }), Some(Err(_))));
    }

    #[test]
    fn the_capture_replays_tile_lines_and_weapon_heights() {
        use crate::capture::{replay, tag, Record};
        let mut p: Vec<u8> = Vec::new();
        for v in [0i32, 0, 5, 0, 0, 1, 4, 1, 0, 2, 0, 3, 0, 4, 0] {
            p.extend_from_slice(&v.to_le_bytes());
        }
        assert_eq!(replay(&Record { tag: tag(b"CBTL"), payload: &p }), Some(Ok(true)));
        p[28] = 9;
        assert_eq!(replay(&Record { tag: tag(b"CBTL"), payload: &p }), Some(Ok(false)));
        let w = weapon(EE_COMBAT_WEAPON_KIND_POINT);
        // SAFETY: a plain record of 4-byte fields
        let bytes = unsafe {
            std::slice::from_raw_parts(
                (&w as *const EeCombatWeaponIn).cast::<u8>(),
                std::mem::size_of::<EeCombatWeaponIn>(),
            )
        };
        let mut p = bytes.to_vec();
        p.extend_from_slice(&4.0f32.to_le_bytes());
        assert_eq!(replay(&Record { tag: tag(b"CBWP"), payload: &p }), Some(Ok(true)));
    }
}
