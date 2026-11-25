//! `EEFindNewTarget`'s screens (R5b.2, fourth round): `EEFOFindNewTargetInMeleeRange` (a candidate nearer to the
//! attacker than its current target) and `EEFOFindNewTargetNearOldTarget` (a candidate near the old target) accept a
//! living unit nearer than their range; the distance is asked of the host only for a living one (the melee
//! screen's is `EEGetHitDistanceSquared`, which may find a drop point, the old target's `DistanceSquaredTo`). And
//! `EEGetHitDistanceSquared`'s arithmetic: the squared map distance from the unit (or a given point) to the target
//! point.

use ee_base::math::types::Ge3DPoint;

/// `EEFOFindNewTargetInMeleeRange`.
pub const EE_COMBAT_SCREEN_KIND_MELEE: i32 = 0;
/// `EEFOFindNewTargetNearOldTarget`.
pub const EE_COMBAT_SCREEN_KIND_NEAR_OLD: i32 = 1;

/// A screen's test (the capture's CBFS layout). `read` and `distance` are the screen's: whether it asked the host,
/// and the answer.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatScreenIn {
    /// `EE_COMBAT_SCREEN_KIND_*`
    pub kind: i32,
    /// the candidate's hit points
    pub hit_points: i32,
    /// the screen's squared range
    pub range: f32,
    pub read: i32,
    pub distance: f32,
}

/// `EEGetHitDistanceSquared`'s points (the capture's CBHD layout).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatHitDistanceIn {
    /// the unit's position
    pub from: [f32; 3],
    /// the target's point (its drop point, a melee unit's: its position)
    pub to: [f32; 3],
    /// the point to measure from instead of the unit's (x -10000: none)
    pub x: f32,
    pub y: f32,
}

/// The screens' test (`Virtual04` and `Virtual08` are one body): alive, then nearer than the range.
pub fn screen_accepts(hit_points: i32, range: f32, distance: impl FnOnce() -> f32) -> (bool, Option<f32>) {
    if hit_points > 0 {
        let d = distance();
        (d < range, Some(d))
    } else {
        (false, None)
    }
}

/// A captured screen record's result (the distance it read).
pub fn screen(i: &EeCombatScreenIn) -> i32 {
    screen_accepts(i.hit_points, i.range, || i.distance).0 as i32
}

/// `EEGetHitDistanceSquared`: from the unit's position (or (x, y) when x is not -10000) to the target's point, both
/// at height 0 (`GE3DPoint::DistanceToSquared`).
pub fn hit_distance(i: &EeCombatHitDistanceIn) -> f32 {
    let mut from = Ge3DPoint { x: i.from[0], y: i.from[1], z: 0.0 };
    if i.x != -10000.0 {
        from.x = i.x;
        from.y = i.y;
    }
    from.distance_to_squared(Ge3DPoint { x: i.to[0], y: i.to[1], z: 0.0 })
}

// ---- the C ABI

/// The host's distance of a screen's candidate (asked only for a living one).
pub type EeCombatScreenDistance = Option<unsafe extern "C" fn(context: *mut std::ffi::c_void) -> f32>;

/// A screen's test: `input.kind`, `hit_points` and `range` in; `read` and `distance` written for the capture.
///
/// # Safety
/// `input` must point to a valid record; `distance` is called with `context` (it must not touch the record).
#[no_mangle]
pub unsafe extern "C" fn ee_combat_screen(
    input: *mut EeCombatScreenIn,
    context: *mut std::ffi::c_void,
    distance: EeCombatScreenDistance,
) -> i32 {
    let (hit_points, range) = unsafe { ((*input).hit_points, (*input).range) };
    let (accepted, read) = screen_accepts(hit_points, range, || match distance {
        Some(f) => unsafe { f(context) },
        None => 0.0,
    });
    unsafe {
        (*input).read = read.is_some() as i32;
        (*input).distance = read.unwrap_or(0.0);
    }
    accepted as i32
}

/// The melee screen's test record (`EEFOFindNewTargetInMeleeRange::Virtual04`/`Virtual08`, one body): a living
/// candidate's distance asked of `distance`, the CBFS record filled as the screen read it; the answer.
pub fn melee_screen(hit_points: i32, range: f32, distance: impl FnOnce() -> f32) -> (EeCombatScreenIn, bool) {
    let (accepted, read) = screen_accepts(hit_points, range, distance);
    let record = EeCombatScreenIn {
        kind: EE_COMBAT_SCREEN_KIND_MELEE,
        hit_points,
        range,
        read: read.is_some() as i32,
        distance: read.unwrap_or(0.0),
    };
    (record, accepted)
}

/// `EEFOFindNewTargetInMeleeRange::Virtual04`/`Virtual08` (units round 12; was EEUnitClass.cpp's EEMeleeScreen): the
/// candidate (`hit_points`) is alive and nearer to the screen's unit than `range` (`EEGetHitDistanceSquared` of the
/// unit and the candidate, measured from the unit); TEST ONLY: the CBFS capture record (the record, then the answer
/// as an `int32_t`, as `EECombatCapture` wrote it).
///
/// # Safety
/// `unit` and `candidate` are live complex units.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_melee_screen(unit: u64, candidate: u64, hit_points: i32, range: f32) -> bool {
    let (record, accepted) = melee_screen(hit_points, range, || unsafe {
        crate::combat::cg_query::ee_units_cgq_hit_distance_squared(unit, candidate, -10000.0, -10000.0)
    });
    if crate::app::capture_record::active() {
        let mut r = crate::app::capture_record::Record::new(u32::from_le_bytes(*b"CBFS"));
        r.i32(record.kind);
        r.i32(record.hit_points);
        r.f32(record.range);
        r.i32(record.read);
        r.f32(record.distance);
        r.i32(accepted as i32);
        r.commit();
    }
    accepted
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_screen_asks_the_distance_only_of_the_living() {
        let mut asked = 0;
        assert_eq!(
            screen_accepts(0, 25.0, || {
                asked += 1;
                1.0
            }),
            (false, None)
        );
        assert_eq!(
            screen_accepts(-3, 25.0, || {
                asked += 1;
                1.0
            }),
            (false, None)
        );
        assert_eq!(asked, 0);
        assert_eq!(screen_accepts(5, 25.0, || 24.9), (true, Some(24.9)));
        assert_eq!(screen_accepts(5, 25.0, || 25.0), (false, Some(25.0)));
        assert!(!screen_accepts(5, 25.0, || f32::NAN).0);
        let i = EeCombatScreenIn {
            kind: EE_COMBAT_SCREEN_KIND_NEAR_OLD,
            hit_points: 1,
            range: 4.0,
            read: 1,
            distance: 3.0,
        };
        assert_eq!(screen(&i), 1);
    }

    #[test]
    fn the_hit_distance_is_on_the_map_plane() {
        let i = EeCombatHitDistanceIn { from: [1.0, 2.0, 9.0], to: [4.0, 6.0, -3.0], x: -10000.0, y: -10000.0 };
        assert_eq!(hit_distance(&i), 25.0);
        let i = EeCombatHitDistanceIn { x: 4.0, y: 3.0, ..i };
        assert_eq!(hit_distance(&i), 9.0);
    }

    #[test]
    fn the_c_screen_writes_what_it_read() {
        unsafe extern "C" fn three(_: *mut std::ffi::c_void) -> f32 {
            3.0
        }
        let mut i = EeCombatScreenIn { kind: 0, hit_points: 2, range: 4.0, read: 0, distance: 0.0 };
        assert_eq!(unsafe { ee_combat_screen(&mut i, std::ptr::null_mut(), Some(three)) }, 1);
        assert_eq!((i.read, i.distance), (1, 3.0));
        i.hit_points = 0;
        assert_eq!(unsafe { ee_combat_screen(&mut i, std::ptr::null_mut(), Some(three)) }, 0);
        assert_eq!((i.read, i.distance), (0, 0.0));
    }

    #[test]
    fn the_melee_screen_reads_the_distance_of_the_living_only() {
        let mut asked = 0;
        let (r, ok) = melee_screen(0, 9.0, || {
            asked += 1;
            1.0
        });
        assert_eq!((r.read, r.distance, ok, asked), (0, 0.0, false, 0));
        let (r, ok) = melee_screen(4, 9.0, || 8.5);
        assert_eq!(
            r,
            EeCombatScreenIn { kind: EE_COMBAT_SCREEN_KIND_MELEE, hit_points: 4, range: 9.0, read: 1, distance: 8.5 }
        );
        assert!(ok);
        assert!(!melee_screen(4, 9.0, || 9.0).1);
    }

    #[test]
    fn the_capture_replays_screens_and_hit_distances() {
        use crate::combat::capture::{encode, records, replay};
        let i =
            EeCombatScreenIn { kind: EE_COMBAT_SCREEN_KIND_MELEE, hit_points: 3, range: 9.0, read: 1, distance: 4.0 };
        assert_eq!(replay(&records(&encode(b"CBFS", &i, &1i32)).unwrap()[0]), Some(Ok(true)));
        assert_eq!(replay(&records(&encode(b"CBFS", &i, &0i32)).unwrap()[0]), Some(Ok(false)));
        let d = EeCombatHitDistanceIn { from: [1.0, 2.0, 9.0], to: [4.0, 6.0, -3.0], x: -10000.0, y: -10000.0 };
        assert_eq!(replay(&records(&encode(b"CBHD", &d, &25.0f32)).unwrap()[0]), Some(Ok(true)));
        assert_eq!(replay(&records(&encode(b"CBHD", &d, &24.0f32)).unwrap()[0]), Some(Ok(false)));
    }
}
