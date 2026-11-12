//! The weapon point's other users (R5b.2, fourth round): `EEUnitClass::LaunchShot`'s launch transform and its two
//! choices (the tag point a capital ship's special attack fires from, the flaming arrows of a special attack),
//! `EEUnitClass::LaunchFromTurret`'s launch and `EEApproachAttachPoint`'s point, over the model's data as records:
//! the tag point at the frame and its model chain's internal transforms (the point's model first), as for the line of
//! fire's start (geometry.rs). Pure: the C++ gathers the model data and does the world's part (the closest tile
//! when the floor tile is off the map, the shot's creation, its goal and launch).

use crate::geometry::{matrix, up_the_chain};
use ee_ge::transform::RotationOrder;
use ee_ge::types::{Ge3DPoint, GeTransformation, GeVector};
use ee_ge::um::{floor, um_arctan2};
use std::f32::consts::{PI, TAU};

/// `LaunchShot`: the shot's launch from the weapon tag point (or half a tile up), turned toward the target.
pub const EE_COMBAT_LAUNCH_KIND_SHOT: i32 = 0;
/// `LaunchFromTurret` (a turret part found): the tag point turned by the turret's yaw, the launch along the chain.
pub const EE_COMBAT_LAUNCH_KIND_TURRET: i32 = 1;
/// `EEApproachAttachPoint`: the world point of the current tag point (or half a tile up).
pub const EE_COMBAT_LAUNCH_KIND_ATTACH: i32 = 2;

/// A launch's inputs (the capture's CBLS, CBLT, CBAP layout). The model chain's transforms are passed beside it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatLaunchIn {
    /// `EE_COMBAT_LAUNCH_KIND_*`
    pub kind: i32,
    /// the shooter's `mIsSpecialAttack` (shot)
    pub special: i32,
    /// the launcher class's `mHasSpecialAttack` (shot)
    pub has_special: i32,
    /// the shooter is a Sp15 capital ship or the Yamato (shot)
    pub capital: i32,
    /// the shooter is a Roman legionnaire (shot)
    pub legionnaire: i32,
    /// the animation's tag points (0 without a list) (shot, attach)
    pub point_count: i32,
    /// `mWeaponTagPoint` (shot, attach)
    pub weapon_index: i32,
    /// the launcher class is a building: the shot faces the target (shot)
    pub building: i32,
    /// not a bomb's launcher: the shot turns to the target about its own axis (shot)
    pub turn: i32,
    /// the shooter's location (`mMatrix`, row by row)
    pub location: [f32; 16],
    /// the stats' scale
    pub scale: f32,
    /// the turret's yaw (turret)
    pub yaw: f32,
    /// the tag point at the frame, in its model's space (when there is one)
    pub point: [f32; 3],
    /// the target location (shot)
    pub target: [f32; 3],
    /// the number of model chain transforms
    pub chain: i32,
}

/// A launch's outputs.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatLaunchOut {
    /// the tag point fired from (-1 without one) (shot)
    pub index: i32,
    /// a special attack fires flaming arrows (shot)
    pub flaming: i32,
    /// the unit has a tag point (attach)
    pub result: i32,
    /// the launch transform (shot, turret)
    pub launch: [f32; 16],
    /// the attach point (attach)
    pub point: [f32; 3],
    /// the launch's floor tile (shot)
    pub tile: [i32; 2],
}

fn rows(t: &GeTransformation) -> [f32; 16] {
    let mut out = [0.0f32; 16];
    for (r, row) in t.m.iter().enumerate() {
        out[r * 4..r * 4 + 4].copy_from_slice(row);
    }
    out
}

fn set_position(t: &mut GeTransformation, p: Ge3DPoint) {
    t.m[0][3] = p.x;
    t.m[1][3] = p.y;
    t.m[2][3] = p.z;
}

/// The tag point `LaunchShot` fires from: the last one for a capital ship's special attack, else the weapon's; -1
/// without tag points.
pub fn launch_index(i: &EeCombatLaunchIn) -> i32 {
    if i.point_count == 0 {
        -1
    } else if i.capital != 0 && i.special != 0 {
        i.point_count.wrapping_sub(1)
    } else {
        i.weapon_index
    }
}

/// `LaunchShot`'s special attack fires the flaming arrows' class (not a legionnaire's or a capital ship's).
pub fn launch_flaming(i: &EeCombatLaunchIn) -> bool {
    i.special != 0 && i.has_special != 0 && i.legionnaire == 0 && i.capital == 0
}

/// The tag point's offset scaled (`theStats->GetScale() * x`, each axis), or half a tile up without one.
fn scaled_offset(i: &EeCombatLaunchIn, chain: &[[f32; 16]]) -> Ge3DPoint {
    let offset = if i.point_count != 0 {
        up_the_chain(Ge3DPoint { x: i.point[0], y: i.point[1], z: i.point[2] }, chain)
    } else {
        Ge3DPoint { x: 0.0, y: 0.0, z: 0.5 }
    };
    Ge3DPoint { x: i.scale * offset.x, y: i.scale * offset.y, z: i.scale * offset.z }
}

/// The yaw toward the target in the map plane (`UMArcTan2(-dx, dy)` of the normalized direction).
fn yaw_to(t: &GeTransformation, target: &[f32; 3]) -> f32 {
    let mut direction = GeVector { x: target[0] - t.x(), y: target[1] - t.y(), z: 0.0 };
    direction.normalize();
    um_arctan2(-direction.x, direction.y)
}

/// `LaunchShot`'s geometry: the launch transform and its floor tile.
pub fn launch_shot(i: &EeCombatLaunchIn, chain: &[[f32; 16]]) -> EeCombatLaunchOut {
    let mut t = matrix(&i.location);
    let position = t.apply_point(scaled_offset(i, chain));
    set_position(&mut t, position);
    if i.building != 0 {
        let yaw = yaw_to(&t, &i.target);
        t.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
    } else if i.turn != 0 {
        let yaw = yaw_to(&t, &i.target);
        let facing = t.apply_vector(GeVector { x: 0.0, y: 1.0, z: 0.0 });
        let mut difference = yaw - um_arctan2(-facing.x, facing.y);
        // the C++'s literals 3.1415927f and 6.2831855f (the same f32s)
        if difference < -PI {
            difference += TAU;
        } else if difference > PI {
            difference -= TAU;
        }
        t.revolve(difference, 0.0, 0.0);
    }
    EeCombatLaunchOut {
        index: launch_index(i),
        flaming: launch_flaming(i) as i32,
        result: 0,
        launch: rows(&t),
        point: [0.0; 3],
        tile: [floor(t.x()), floor(t.y())],
    }
}

/// `LaunchFromTurret`'s launch (its turret part found): the tag point turned by the turret's yaw up the chain,
/// scaled, under the location; the launch is the chain's transform of the turn under the location, at that point.
pub fn launch_from_turret(i: &EeCombatLaunchIn, chain: &[[f32; 16]]) -> EeCombatLaunchOut {
    let location = matrix(&i.location);
    let mut part = GeTransformation::IDENTITY;
    part.set_orientation(RotationOrder::Ypr, i.yaw, 0.0, 0.0);
    let local = Ge3DPoint { x: i.point[0], y: i.point[1], z: i.point[2] };
    let offset = up_the_chain(part.apply_point(local), chain);
    let offset = Ge3DPoint { x: i.scale * offset.x, y: i.scale * offset.y, z: i.scale * offset.z };
    let world = location.apply_point(offset);
    // `EETagPoint::GetTransformation`: the part, each link's transform applied to it, then the location
    let mut result = part;
    for link in chain {
        result = matrix(link).compose(&result);
    }
    let mut launch = location.compose(&result);
    set_position(&mut launch, world);
    EeCombatLaunchOut { index: 0, flaming: 0, result: 0, launch: rows(&launch), point: [0.0; 3], tile: [0, 0] }
}

/// `EEApproachAttachPoint`: the current tag point's world point (half a tile up without one) and whether it has one.
pub fn attach_point(i: &EeCombatLaunchIn, chain: &[[f32; 16]]) -> EeCombatLaunchOut {
    let point = matrix(&i.location).apply_point(scaled_offset(i, chain));
    EeCombatLaunchOut {
        index: 0,
        flaming: 0,
        result: (i.point_count != 0) as i32,
        launch: [0.0; 16],
        point: [point.x, point.y, point.z],
        tile: [0, 0],
    }
}

/// The launch of `i.kind`.
pub fn launch(i: &EeCombatLaunchIn, chain: &[[f32; 16]]) -> EeCombatLaunchOut {
    match i.kind {
        EE_COMBAT_LAUNCH_KIND_TURRET => launch_from_turret(i, chain),
        EE_COMBAT_LAUNCH_KIND_ATTACH => attach_point(i, chain),
        _ => launch_shot(i, chain),
    }
}

// ---- the C ABI

/// The tag point `LaunchShot` fires from (-1: none).
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_launch_index(input: *const EeCombatLaunchIn) -> i32 {
    launch_index(unsafe { &*input })
}

/// The launch of `input.kind` over `input.chain` model transforms.
///
/// # Safety
/// `input` must point to a valid record and `chain` to `input.chain` matrices (or be NULL when it is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_combat_launch(
    input: *const EeCombatLaunchIn,
    chain: *const [f32; 16],
) -> EeCombatLaunchOut {
    let input = unsafe { &*input };
    let chain: &[[f32; 16]] = if chain.is_null() || input.chain <= 0 {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(chain, input.chain as usize) }
    };
    launch(input, chain)
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A turn of 90 degrees about z (x to y), at (10, 20, 3).
    fn location() -> [f32; 16] {
        let mut l = [0.0f32; 16];
        l[1] = -1.0;
        l[3] = 10.0;
        l[4] = 1.0;
        l[7] = 20.0;
        l[10] = 1.0;
        l[11] = 3.0;
        l[15] = 1.0;
        l
    }

    fn shot() -> EeCombatLaunchIn {
        EeCombatLaunchIn {
            kind: EE_COMBAT_LAUNCH_KIND_SHOT,
            point_count: 3,
            weapon_index: 1,
            location: location(),
            scale: 2.0,
            point: [1.0, 0.0, 0.5],
            target: [10.0, 30.0, 0.0],
            ..Default::default()
        }
    }

    #[test]
    fn the_index_and_the_flaming_arrows() {
        let mut i = shot();
        assert_eq!(launch_index(&i), 1);
        i.capital = 1;
        assert_eq!(launch_index(&i), 1);
        i.special = 1;
        assert_eq!(launch_index(&i), 2);
        assert!(!launch_flaming(&i));
        i.capital = 0;
        assert!(!launch_flaming(&i));
        i.has_special = 1;
        assert!(launch_flaming(&i));
        i.legionnaire = 1;
        assert!(!launch_flaming(&i));
        i.point_count = 0;
        assert_eq!(launch_index(&i), -1);
    }

    #[test]
    fn a_shot_starts_at_the_scaled_tag_point() {
        // the point (1, 0, 0.5) scaled by 2, turned 90 degrees: (10, 22, 4); a bomb's launcher keeps the orientation
        let out = launch_shot(&shot(), &[]);
        assert_eq!((out.launch[3], out.launch[7], out.launch[11]), (10.0, 22.0, 4.0));
        assert_eq!(&out.launch[..3], &[0.0, -1.0, 0.0]);
        assert_eq!(out.tile, [10, 22]);
        assert_eq!(out.index, 1);
        // without tag points: half a tile up, scaled
        let mut i = shot();
        i.point_count = 0;
        let out = launch_shot(&i, &[]);
        assert_eq!((out.launch[3], out.launch[7], out.launch[11]), (10.0, 20.0, 4.0));
        assert_eq!(out.index, -1);
    }

    #[test]
    fn a_shot_faces_its_target() {
        // a building's shot faces the target straight up the map (+y): yaw 0
        let mut i = shot();
        i.building = 1;
        let out = launch_shot(&i, &[]);
        assert!((out.launch[0] - 1.0).abs() < 1e-3 && out.launch[1].abs() < 1e-3, "{:?}", out.launch);
        // a turning shot revolves from its facing (-x after the turn... here +y) to the target's
        let mut i = shot();
        i.turn = 1;
        i.target = [0.0, 22.0, 0.0];
        let out = launch_shot(&i, &[]);
        // the forward axis (column 1) now points to -x
        assert!((out.launch[1] + 1.0).abs() < 1e-3 && out.launch[5].abs() < 1e-3, "{:?}", out.launch);
        assert_eq!((out.launch[3], out.launch[7]), (10.0, 22.0));
    }

    #[test]
    fn a_turret_launch_and_the_attach_point() {
        let mut i = shot();
        i.kind = EE_COMBAT_LAUNCH_KIND_TURRET;
        i.yaw = 0.0;
        let out = launch(&i, &[]);
        assert_eq!((out.launch[3], out.launch[7], out.launch[11]), (10.0, 22.0, 4.0));
        assert_eq!(&out.launch[..3], &location()[..3]);
        let mut i = shot();
        i.kind = EE_COMBAT_LAUNCH_KIND_ATTACH;
        let out = launch(&i, &[]);
        assert_eq!((out.point, out.result), ([10.0, 22.0, 4.0], 1));
        i.point_count = 0;
        let out = launch(&i, &[]);
        assert_eq!((out.point, out.result), ([10.0, 20.0, 4.0], 0));
    }

    #[test]
    fn the_chain_lifts_the_point() {
        let mut lift = [0.0f32; 16];
        lift[0] = 1.0;
        lift[5] = 1.0;
        lift[10] = 1.0;
        lift[11] = 1.0;
        lift[15] = 1.0;
        let mut i = shot();
        i.chain = 1;
        assert_eq!(launch_shot(&i, &[lift]).launch[11], 6.0);
        i.kind = EE_COMBAT_LAUNCH_KIND_TURRET;
        let out = launch(&i, &[lift]);
        // the point lifted, and the launch's own transform carries the lift (overwritten by the world point)
        assert_eq!(out.launch[11], 6.0);
    }

    #[test]
    fn the_capture_replays_launches() {
        use crate::capture::{replay, tag, Record};
        fn bytes<T>(v: &T) -> Vec<u8> {
            // SAFETY: plain records of 4-byte fields
            unsafe { std::slice::from_raw_parts((v as *const T).cast::<u8>(), std::mem::size_of::<T>()) }.to_vec()
        }
        let mut lift = [0.0f32; 16];
        lift[0] = 1.0;
        lift[5] = 1.0;
        lift[10] = 1.0;
        lift[11] = 1.0;
        lift[15] = 1.0;
        let kinds = [
            (b"CBLS", EE_COMBAT_LAUNCH_KIND_SHOT),
            (b"CBLT", EE_COMBAT_LAUNCH_KIND_TURRET),
            (b"CBAP", EE_COMBAT_LAUNCH_KIND_ATTACH),
        ];
        for (name, kind) in kinds {
            let mut i = shot();
            i.kind = kind;
            i.chain = 1;
            let mut p = bytes(&i);
            p.extend(bytes(&lift));
            p.extend(bytes(&launch(&i, &[lift])));
            assert_eq!(replay(&Record { tag: tag(name), payload: &p }), Some(Ok(true)));
            let n = p.len();
            p[n - 40] ^= 1;
            assert_eq!(replay(&Record { tag: tag(name), payload: &p }), Some(Ok(false)));
            assert!(matches!(replay(&Record { tag: tag(name), payload: &p[..n - 64] }), Some(Err(_))));
        }
    }
}
