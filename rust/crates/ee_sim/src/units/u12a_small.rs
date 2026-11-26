//! `EEComplexUnit`'s last small bodies (the units' lane, round 12; were EEComplexUnit.cpp, ee/EEUnit.h and
//! EEWorld.cpp): the pool's `Create`s, `SetPoolSize`, `ReleasePool` and `EEReportLeakingUnits`, the location setters
//! (`SetLocationXY`, `SetLocation`, `SetTransformation`), `SetScaledModel`, `IsInReach` with its CBQR record,
//! `SetOwner` and `ClearHolder`, as statements of the unit's host ([`CuHost::cu`], `ee_units_cu_host`: the
//! `EE_UNITS_CU_T_*` below) asked in the original's order; and the bodies that only read or write the unit's
//! records or its location: `ClearGarrison`, `ClearCollisionState`, the squared distances.

use crate::units::cu_body::{
    CuHost, EE_UNITS_CU_CLEAR_HOLDER_FIELD, EE_UNITS_CU_DEREFERENCE, EE_UNITS_CU_HOLDER, EE_UNITS_CU_OWNER,
};
use crate::units::cu_cargo::EE_UNITS_CU_REFERENCE;
use crate::units::cu_life::EE_UNITS_CU_AFFECTING_COUNT;
use crate::units::cu_small::{
    EE_UNITS_CU_S_LEAKED_CLEAR, EE_UNITS_CU_S_LEAKED_COUNT, EE_UNITS_CU_S_LEAKED_GET, EE_UNITS_CU_S_RESET,
    EE_UNITS_CU_S_RESET_COMPLEX, EE_UNITS_CU_S_RESET_UNIT,
};
use crate::units::cu_world::EE_UNITS_CU_AFFECTING_KIND;
use crate::world::unit_fields::EeUnitsComplexUnitFields;
use std::os::raw::{c_long, c_ulong};

// ---- the statements (`op`; the answer). `a` is another object, `b`, `c` values, `f` floats.
/// `MoveToLocation(f[0], f[1], b != 0)`
pub const EE_UNITS_CU_T_MOVE_TO_LOCATION: i32 = 12000;
/// `mPhysicalModel.SetLocationXY(f[0], f[1])`
pub const EE_UNITS_CU_T_PM_SET_XY: i32 = 12001;
/// `mPhysicalModel.SetLocationXYZ(GE3DPoint(f[0], f[1], f[2]))`
pub const EE_UNITS_CU_T_PM_SET_XYZ: i32 = 12002;
/// `mPhysicalModel.SetLocation(*(const GETransformation*)a)`
pub const EE_UNITS_CU_T_PM_SET_LOCATION: i32 = 12003;
/// `mPhysicalModel.SetModel((GEModel*)a)`
pub const EE_UNITS_CU_T_PM_SET_MODEL: i32 = 12004;
/// `mPhysicalModel.SetScale(GetUnitClass()->GetTileSize())`
pub const EE_UNITS_CU_T_PM_SET_TILE_SCALE: i32 = 12005;
/// a unit from the pool (from the heap with `EE_UNIT_POOL=0`: `b`); no unit yet
pub const EE_UNITS_CU_T_POOL_NEW: i32 = 12006;
/// `Init(class, transformation, player, goal queue count)`: `a` the four words
pub const EE_UNITS_CU_T_INIT: i32 = 12007;
/// `UList<EEComplexUnit*>::sNodePool.SetSize(b, c, 3)`; no unit
pub const EE_UNITS_CU_T_NODE_POOL_SET_SIZE: i32 = 12008;
/// `EEComplexUnit::sMemoryPool.SetSize(b, c, 1)`; no unit
pub const EE_UNITS_CU_T_POOL_SET_SIZE: i32 = 12009;
/// `EEComplexUnit::sMemoryPool.DumpStats(false)`; no unit
pub const EE_UNITS_CU_T_POOL_DUMP_STATS: i32 = 12010;
/// `EEComplexUnit::sMemoryPool.Release()`; no unit
pub const EE_UNITS_CU_T_POOL_RELEASE: i32 = 12011;
/// the used pool unit `b`'s `mReferenceCount = 0`; no unit
pub const EE_UNITS_CU_T_LEAKED_ZERO_REFERENCES: i32 = 12012;
/// `mWeaponTagPoint`
pub const EE_UNITS_CU_T_WEAPON_TAG: i32 = 12013;
/// `EECombatIsInReach(this, (EEUnit*)a)`
pub const EE_UNITS_CU_T_IN_REACH: i32 = 12014;
/// `EECombatCaptureHostRead(EE_COMBAT_HR_IN_REACH, this, a, b != 0, c, mWeaponTagPoint)` (TEST ONLY: CBQR)
pub const EE_UNITS_CU_T_CAPTURE_IN_REACH: i32 = 12015;
/// `ee_combat_host_read_enter(0)`: whether this is the outermost host read
pub const EE_UNITS_CU_T_READ_ENTER: i32 = 12016;
/// `ee_combat_host_read_leave(0)`
pub const EE_UNITS_CU_T_READ_LEAVE: i32 = 12017;
/// `mOwner = (EEComplexUnit*)a`
pub const EE_UNITS_CU_T_SET_OWNER_FIELD: i32 = 12018;

fn q<H: CuHost + ?Sized>(h: &mut H, op: i32, unit: u64, a: u64, b: i64, c: i64) -> i64 {
    h.cu(op, unit, a, b, c, &mut [0.0; 4])
}

/// A statement on no unit (the pool's): `a` is 1, because the host's preamble reads the unit's class unless `a` names
/// an object.
fn pool<H: CuHost + ?Sized>(h: &mut H, op: i32, b: i64, c: i64) -> i64 {
    h.cu(op, 0, 1, b, c, &mut [0.0; 4])
}

fn move_to<H: CuHost + ?Sized>(h: &mut H, unit: u64, x: f32, y: f32, update_tiles: bool) {
    h.cu(EE_UNITS_CU_T_MOVE_TO_LOCATION, unit, 0, i64::from(update_tiles), 0, &mut [x, y, 0.0, 0.0]);
}

/// `SetLocationXY(x, y, update tiles)`: the unit moved, then its model's 2D location.
pub fn set_location_xy<H: CuHost + ?Sized>(h: &mut H, unit: u64, x: f32, y: f32, update_tiles: bool) {
    move_to(h, unit, x, y, update_tiles);
    h.cu(EE_UNITS_CU_T_PM_SET_XY, unit, 0, 0, 0, &mut [x, y, 0.0, 0.0]);
}

/// `SetLocation(point, update tiles)`: the unit moved, then its model's 3D location.
pub fn set_location<H: CuHost + ?Sized>(h: &mut H, unit: u64, p: [f32; 3], update_tiles: bool) {
    move_to(h, unit, p[0], p[1], update_tiles);
    h.cu(EE_UNITS_CU_T_PM_SET_XYZ, unit, 0, 0, 0, &mut [p[0], p[1], p[2], 0.0]);
}

/// `SetTransformation(transformation, update tiles)`: the unit moved to the transformation's x and y (`at`), then
/// its model's transformation.
pub fn set_transformation<H: CuHost + ?Sized>(h: &mut H, unit: u64, transformation: u64, at: [f32; 2], update: bool) {
    move_to(h, unit, at[0], at[1], update);
    q(h, EE_UNITS_CU_T_PM_SET_LOCATION, unit, transformation, 0, 0);
}

/// `SetScaledModel(model)`: the model, scaled by the class's tile size.
pub fn set_scaled_model<H: CuHost + ?Sized>(h: &mut H, unit: u64, model: u64) {
    q(h, EE_UNITS_CU_T_PM_SET_MODEL, unit, model, 0, 0);
    q(h, EE_UNITS_CU_T_PM_SET_TILE_SCALE, unit, 0, 0, 0);
}

/// `EEComplexUnit::Create()`: a pool unit reset to its defaults.
pub fn create<H: CuHost + ?Sized>(h: &mut H, heap: bool) -> u64 {
    let unit = pool(h, EE_UNITS_CU_T_POOL_NEW, i64::from(heap), 0) as u64;
    for op in [EE_UNITS_CU_S_RESET, EE_UNITS_CU_S_RESET_UNIT, EE_UNITS_CU_S_RESET_COMPLEX] {
        q(h, op, unit, 0, 0, 0);
    }
    unit
}

/// `EEComplexUnit::Create(class, transformation, player, goal queue count)`: a pool unit initialised (`args` the
/// four words).
pub fn create_init<H: CuHost + ?Sized>(h: &mut H, heap: bool, args: u64) -> u64 {
    let unit = pool(h, EE_UNITS_CU_T_POOL_NEW, i64::from(heap), 0) as u64;
    q(h, EE_UNITS_CU_T_INIT, unit, args, 0, 0);
    unit
}

/// `SetPoolSize(chunk size, max chunks)`: the contact nodes' pool at twice the chunk size, then the units' pool;
/// the units' pool's answer.
pub fn set_pool_size<H: CuHost + ?Sized>(h: &mut H, chunk_size: c_ulong, max_chunks: c_ulong) -> bool {
    pool(h, EE_UNITS_CU_T_NODE_POOL_SET_SIZE, chunk_size.wrapping_mul(2) as i64, max_chunks as i64);
    pool(h, EE_UNITS_CU_T_POOL_SET_SIZE, chunk_size as i64, max_chunks as i64) != 0
}

/// `ReleasePool()`: the pool's statistics, then its release.
pub fn release_pool<H: CuHost + ?Sized>(h: &mut H) {
    pool(h, EE_UNITS_CU_T_POOL_DUMP_STATS, 0, 0);
    pool(h, EE_UNITS_CU_T_POOL_RELEASE, 0, 0);
}

/// `EEReportLeakingUnits()`: every used pool unit's reference count cleared, last first.
pub fn report_leaking<H: CuHost + ?Sized>(h: &mut H) {
    pool(h, EE_UNITS_CU_S_LEAKED_CLEAR, 0, 0);
    pool(h, EE_UNITS_CU_S_LEAKED_GET, 0, 0);
    let mut i = pool(h, EE_UNITS_CU_S_LEAKED_COUNT, 0, 0) - 1;
    while i >= 0 {
        pool(h, EE_UNITS_CU_T_LEAKED_ZERO_REFERENCES, i, 0);
        i -= 1;
    }
}

/// `IsInReach(target)`: the combat reach inside a host-read scope; the outermost read is recorded with the weapon
/// tag point before and after.
pub fn is_in_reach<H: CuHost + ?Sized>(h: &mut H, unit: u64, target: u64) -> bool {
    let outer = q(h, EE_UNITS_CU_T_READ_ENTER, unit, 0, 0, 0) != 0;
    let tag_before = q(h, EE_UNITS_CU_T_WEAPON_TAG, unit, 0, 0, 0);
    let reach = q(h, EE_UNITS_CU_T_IN_REACH, unit, target, 0, 0) != 0;
    if outer {
        q(h, EE_UNITS_CU_T_CAPTURE_IN_REACH, unit, target, i64::from(reach), tag_before);
    }
    q(h, EE_UNITS_CU_T_READ_LEAVE, unit, 0, 0, 0);
    reach
}

/// `SetOwner(owner)`: a different owner replaces the old one (dereferenced) and is referenced.
pub fn set_owner<H: CuHost + ?Sized>(h: &mut H, unit: u64, owner: u64) {
    let old = q(h, EE_UNITS_CU_OWNER, unit, 0, 0, 0) as u64;
    if old != owner {
        if old != 0 {
            q(h, EE_UNITS_CU_DEREFERENCE, unit, old, 0, 0);
        }
        q(h, EE_UNITS_CU_T_SET_OWNER_FIELD, unit, owner, 0, 0);
        q(h, EE_UNITS_CU_REFERENCE, unit, owner, 0, 0);
    }
}

/// `ClearHolder()`: the holder dereferenced and forgotten.
pub fn clear_holder<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    let holder = q(h, EE_UNITS_CU_HOLDER, unit, 0, 0, 0) as u64;
    if holder != 0 {
        q(h, EE_UNITS_CU_DEREFERENCE, unit, holder, 0, 0);
    }
    q(h, EE_UNITS_CU_CLEAR_HOLDER_FIELD, unit, 0, 0, 0);
}

/// `HasEffect(kind)`: whether an area effect of a kind acts on the unit (`GetEffect` into a dummy).
pub fn has_effect<H: CuHost + ?Sized>(h: &mut H, unit: u64, kind: i64) -> bool {
    let mut k = 0;
    while k < q(h, EE_UNITS_CU_AFFECTING_COUNT, unit, 0, 0, 0) {
        if q(h, EE_UNITS_CU_AFFECTING_KIND, unit, 0, k, 0) == kind {
            return true;
        }
        k += 1;
    }
    false
}

/// `ClearGarrison()`.
pub fn clear_garrison(fields: &mut EeUnitsComplexUnitFields) {
    fields.mGarrisonPopulation = 0;
    fields.mGarrisonCount = 0;
}

/// `ClearCollisionState()`: the state, the collision tile (off the map) and the resolution count.
pub fn clear_collision_state(fields: &mut EeUnitsComplexUnitFields) {
    fields.mCollisionState = 0;
    fields.mCollisionTile.mX = -10000;
    fields.mCollisionTile.mY = fields.mCollisionTile.mX;
    fields.mCollisionResolutionCount = 0.0;
}

/// `DistanceSquaredTo(unit)`: between two locations' x and y (the original's order: dy first, dx squared first).
pub fn distance_squared(from: [f32; 2], to: [f32; 2]) -> f32 {
    let dy = from[1] - to[1];
    let dx = from[0] - to[0];
    dx * dx + dy * dy
}

/// `DistanceSquaredToTileXY(tile x, tile y)`: from a location to a tile's corner (dx first, dy squared first).
pub fn distance_squared_to_tile(from: [f32; 2], tile_x: c_long, tile_y: c_long) -> f32 {
    let dx = from[0] - tile_x as f32;
    let dy = from[1] - tile_y as f32;
    dy * dy + dx * dx
}

// ---- the bodies (`ee_units_cu_tiny`'s `what`)
pub const EE_UNITS_CU_T_DO_SET_LOCATION_XY: i32 = 1;
pub const EE_UNITS_CU_T_DO_SET_LOCATION: i32 = 2;
pub const EE_UNITS_CU_T_DO_SET_TRANSFORMATION: i32 = 3;
pub const EE_UNITS_CU_T_DO_SET_SCALED_MODEL: i32 = 4;
pub const EE_UNITS_CU_T_DO_CREATE: i32 = 5;
pub const EE_UNITS_CU_T_DO_CREATE_INIT: i32 = 6;
pub const EE_UNITS_CU_T_DO_SET_POOL_SIZE: i32 = 7;
pub const EE_UNITS_CU_T_DO_RELEASE_POOL: i32 = 8;
pub const EE_UNITS_CU_T_DO_REPORT_LEAKING: i32 = 9;
pub const EE_UNITS_CU_T_DO_IS_IN_REACH: i32 = 10;
pub const EE_UNITS_CU_T_DO_SET_OWNER: i32 = 11;
pub const EE_UNITS_CU_T_DO_CLEAR_HOLDER: i32 = 12;
pub const EE_UNITS_CU_T_DO_HAS_EFFECT: i32 = 13;

/// Runs body `what` on `unit` (`a` another object, `b`, `c` values, `f` floats); `heap`: `EE_UNIT_POOL=0`.
#[allow(clippy::too_many_arguments)]
pub fn run<H: CuHost + ?Sized>(
    h: &mut H,
    what: i32,
    unit: u64,
    a: u64,
    b: i64,
    c: i64,
    f: [f32; 3],
    heap: bool,
) -> i64 {
    match what {
        EE_UNITS_CU_T_DO_SET_LOCATION_XY => set_location_xy(h, unit, f[0], f[1], b != 0),
        EE_UNITS_CU_T_DO_SET_LOCATION => set_location(h, unit, f, b != 0),
        EE_UNITS_CU_T_DO_SET_TRANSFORMATION => set_transformation(h, unit, a, [f[0], f[1]], b != 0),
        EE_UNITS_CU_T_DO_SET_SCALED_MODEL => set_scaled_model(h, unit, a),
        EE_UNITS_CU_T_DO_CREATE => return create(h, heap) as i64,
        EE_UNITS_CU_T_DO_CREATE_INIT => return create_init(h, heap, a) as i64,
        EE_UNITS_CU_T_DO_SET_POOL_SIZE => return i64::from(set_pool_size(h, b as c_ulong, c as c_ulong)),
        EE_UNITS_CU_T_DO_RELEASE_POOL => release_pool(h),
        EE_UNITS_CU_T_DO_REPORT_LEAKING => report_leaking(h),
        EE_UNITS_CU_T_DO_IS_IN_REACH => return i64::from(is_in_reach(h, unit, a)),
        EE_UNITS_CU_T_DO_SET_OWNER => set_owner(h, unit, a),
        EE_UNITS_CU_T_DO_CLEAR_HOLDER => clear_holder(h, unit),
        EE_UNITS_CU_T_DO_HAS_EFFECT => return i64::from(has_effect(h, unit, b)),
        _ => {}
    }
    0
}

/// Whether `EE_UNIT_POOL` asks for heap units (its value starts with `0`): a debugging aid (a pool slot is reused
/// at once, so AddressSanitizer cannot see a unit released one reference too early).
pub fn pool_off(value: Option<&[u8]>) -> bool {
    value.is_some_and(|v| v.first() == Some(&b'0'))
}

#[cfg(not(test))]
pub(crate) mod game {
    use super::*;
    use crate::units::cu_body::game::Game;
    use std::os::raw::c_void;
    use std::sync::OnceLock;

    fn heap() -> bool {
        static OFF: OnceLock<bool> = OnceLock::new();
        *OFF.get_or_init(|| pool_off(std::env::var_os("EE_UNIT_POOL").as_ref().map(|v| v.as_encoded_bytes())))
    }

    /// `EEComplexUnit`'s body `what` (`EE_UNITS_CU_T_DO_*`) on `unit` (NULL for the pool's).
    ///
    /// # Safety
    /// `unit` is a live `EEComplexUnit` or NULL as the body needs it, `a` live likewise; the simulation thread.
    #[no_mangle]
    #[allow(clippy::too_many_arguments)]
    pub unsafe extern "C" fn ee_units_cu_tiny(
        what: i32,
        unit: *mut c_void,
        a: *const c_void,
        b: i64,
        c: i64,
        x: f32,
        y: f32,
        z: f32,
    ) -> i64 {
        run(&mut Game, what, unit as usize as u64, a as usize as u64, b, c, [x, y, z], heap())
    }

    /// `EEComplexUnit::Create(class, transformation, player, goal queue count)`: the new unit.
    ///
    /// # Safety
    /// The class, the transformation and the player are live; the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_cu_create_init(
        class: *mut c_void,
        transformation: *const c_void,
        player: *mut c_void,
        goal_queues: c_ulong,
    ) -> *mut c_void {
        let args =
            [class as usize as u64, transformation as usize as u64, player as usize as u64, u64::from(goal_queues)];
        create_init(&mut Game, heap(), args.as_ptr() as usize as u64) as usize as *mut c_void
    }

    /// `EEComplexUnit::ClearGarrison()` over the unit's record.
    ///
    /// # Safety
    /// `fields` is a live unit's record.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_cu_clear_garrison(fields: *mut EeUnitsComplexUnitFields) {
        // SAFETY: the caller's contract
        clear_garrison(unsafe { &mut *fields });
    }

    /// `EEComplexUnit::ClearCollisionState()` over the unit's record.
    ///
    /// # Safety
    /// `fields` is a live unit's record.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_cu_clear_collision_state(fields: *mut EeUnitsComplexUnitFields) {
        // SAFETY: the caller's contract
        clear_collision_state(unsafe { &mut *fields });
    }

    /// `EEComplexUnit::DistanceSquaredTo`: between two transformations' positions (row-major 4 x 4: x and y are
    /// rows 0 and 1 of the last column).
    ///
    /// # Safety
    /// Both are live transformations.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_cu_distance_squared(from: *const [[f32; 4]; 4], to: *const [[f32; 4]; 4]) -> f32 {
        // SAFETY: the caller's contract
        let (a, b) = unsafe { (&*from, &*to) };
        distance_squared([a[0][3], a[1][3]], [b[0][3], b[1][3]])
    }

    /// `EEComplexUnit::DistanceSquaredToTileXY`: from a transformation's position to a tile.
    ///
    /// # Safety
    /// `from` is a live transformation.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_cu_distance_squared_to_tile(
        from: *const [[f32; 4]; 4],
        tile_x: c_long,
        tile_y: c_long,
    ) -> f32 {
        // SAFETY: the caller's contract
        let a = unsafe { &*from };
        distance_squared_to_tile([a[0][3], a[1][3]], tile_x, tile_y)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct Rec {
        log: Vec<(i32, u64, u64, i64, i64, [f32; 3])>,
        answers: Vec<(i32, i64)>,
    }

    impl CuHost for Rec {
        fn cu(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
            self.log.push((op, unit, a, b, c, [f[0], f[1], f[2]]));
            self.answers.iter().find(|e| e.0 == op).map_or(0, |e| e.1)
        }
    }

    fn ops(r: &Rec) -> Vec<i32> {
        r.log.iter().map(|e| e.0).collect()
    }

    #[test]
    fn the_location_setters_move_the_unit_before_its_model() {
        let mut r = Rec::default();
        set_location_xy(&mut r, 1, 2.5, 3.5, true);
        assert_eq!(
            r.log,
            [
                (EE_UNITS_CU_T_MOVE_TO_LOCATION, 1, 0, 1, 0, [2.5, 3.5, 0.0]),
                (EE_UNITS_CU_T_PM_SET_XY, 1, 0, 0, 0, [2.5, 3.5, 0.0])
            ]
        );
        let mut r = Rec::default();
        set_location(&mut r, 1, [4.0, 5.0, 6.0], false);
        assert_eq!(
            r.log,
            [
                (EE_UNITS_CU_T_MOVE_TO_LOCATION, 1, 0, 0, 0, [4.0, 5.0, 0.0]),
                (EE_UNITS_CU_T_PM_SET_XYZ, 1, 0, 0, 0, [4.0, 5.0, 6.0])
            ]
        );
        let mut r = Rec::default();
        set_transformation(&mut r, 1, 0x70, [7.0, 8.0], true);
        assert_eq!(
            r.log,
            [
                (EE_UNITS_CU_T_MOVE_TO_LOCATION, 1, 0, 1, 0, [7.0, 8.0, 0.0]),
                (EE_UNITS_CU_T_PM_SET_LOCATION, 1, 0x70, 0, 0, [0.0; 3])
            ]
        );
        let mut r = Rec::default();
        set_scaled_model(&mut r, 1, 0x80);
        assert_eq!(ops(&r), [EE_UNITS_CU_T_PM_SET_MODEL, EE_UNITS_CU_T_PM_SET_TILE_SCALE]);
        assert_eq!(r.log[0].2, 0x80);
    }

    #[test]
    fn the_pool_bodies_in_the_originals_order() {
        let mut r = Rec { answers: vec![(EE_UNITS_CU_T_POOL_NEW, 0x500)], ..Default::default() };
        assert_eq!(create(&mut r, true), 0x500);
        assert_eq!(
            ops(&r),
            [EE_UNITS_CU_T_POOL_NEW, EE_UNITS_CU_S_RESET, EE_UNITS_CU_S_RESET_UNIT, EE_UNITS_CU_S_RESET_COMPLEX]
        );
        assert_eq!(r.log[0].3, 1);
        assert!(r.log[1..].iter().all(|e| e.1 == 0x500));
        let mut r = Rec { answers: vec![(EE_UNITS_CU_T_POOL_NEW, 0x500)], ..Default::default() };
        assert_eq!(create_init(&mut r, false, 0x90), 0x500);
        assert_eq!(r.log[1], (EE_UNITS_CU_T_INIT, 0x500, 0x90, 0, 0, [0.0; 3]));
        let mut r = Rec { answers: vec![(EE_UNITS_CU_T_POOL_SET_SIZE, 1)], ..Default::default() };
        assert!(set_pool_size(&mut r, 100, 7));
        assert_eq!(
            r.log,
            [
                (EE_UNITS_CU_T_NODE_POOL_SET_SIZE, 0, 1, 200, 7, [0.0; 3]),
                (EE_UNITS_CU_T_POOL_SET_SIZE, 0, 1, 100, 7, [0.0; 3])
            ]
        );
        let mut r = Rec::default();
        release_pool(&mut r);
        assert_eq!(ops(&r), [EE_UNITS_CU_T_POOL_DUMP_STATS, EE_UNITS_CU_T_POOL_RELEASE]);
        let mut r = Rec { answers: vec![(EE_UNITS_CU_S_LEAKED_COUNT, 2)], ..Default::default() };
        report_leaking(&mut r);
        let zeroed: Vec<i64> =
            r.log.iter().filter(|e| e.0 == EE_UNITS_CU_T_LEAKED_ZERO_REFERENCES).map(|e| e.3).collect();
        assert_eq!(zeroed, [1, 0]);
        assert_eq!(ops(&r)[..3], [EE_UNITS_CU_S_LEAKED_CLEAR, EE_UNITS_CU_S_LEAKED_GET, EE_UNITS_CU_S_LEAKED_COUNT]);
        assert!(pool_off(Some(b"0")) && pool_off(Some(b"0x")) && !pool_off(Some(b"1")) && !pool_off(None));
        assert!(!pool_off(Some(b"")));
    }

    #[test]
    fn the_reach_is_recorded_only_by_the_outermost_read() {
        let mut r = Rec {
            answers: vec![(EE_UNITS_CU_T_READ_ENTER, 1), (EE_UNITS_CU_T_WEAPON_TAG, 3), (EE_UNITS_CU_T_IN_REACH, 1)],
            ..Default::default()
        };
        assert!(is_in_reach(&mut r, 1, 9));
        assert_eq!(
            ops(&r),
            [
                EE_UNITS_CU_T_READ_ENTER,
                EE_UNITS_CU_T_WEAPON_TAG,
                EE_UNITS_CU_T_IN_REACH,
                EE_UNITS_CU_T_CAPTURE_IN_REACH,
                EE_UNITS_CU_T_READ_LEAVE
            ]
        );
        assert_eq!(r.log[3], (EE_UNITS_CU_T_CAPTURE_IN_REACH, 1, 9, 1, 3, [0.0; 3]));
        let mut r = Rec::default();
        assert!(!is_in_reach(&mut r, 1, 9));
        assert_eq!(
            ops(&r),
            [EE_UNITS_CU_T_READ_ENTER, EE_UNITS_CU_T_WEAPON_TAG, EE_UNITS_CU_T_IN_REACH, EE_UNITS_CU_T_READ_LEAVE]
        );
    }

    #[test]
    fn the_owner_and_the_holder_are_released_before_they_change() {
        let mut r = Rec { answers: vec![(EE_UNITS_CU_OWNER, 0x30)], ..Default::default() };
        set_owner(&mut r, 1, 0x40);
        assert_eq!(
            r.log.iter().map(|e| (e.0, e.2)).collect::<Vec<_>>(),
            [
                (EE_UNITS_CU_OWNER, 0),
                (EE_UNITS_CU_DEREFERENCE, 0x30),
                (EE_UNITS_CU_T_SET_OWNER_FIELD, 0x40),
                (EE_UNITS_CU_REFERENCE, 0x40)
            ]
        );
        let mut r = Rec { answers: vec![(EE_UNITS_CU_OWNER, 0x40)], ..Default::default() };
        set_owner(&mut r, 1, 0x40);
        assert_eq!(ops(&r), [EE_UNITS_CU_OWNER]);
        let mut r = Rec::default();
        set_owner(&mut r, 1, 0x40);
        assert_eq!(ops(&r), [EE_UNITS_CU_OWNER, EE_UNITS_CU_T_SET_OWNER_FIELD, EE_UNITS_CU_REFERENCE]);
        let mut r = Rec { answers: vec![(EE_UNITS_CU_HOLDER, 0x50)], ..Default::default() };
        clear_holder(&mut r, 1);
        assert_eq!(ops(&r), [EE_UNITS_CU_HOLDER, EE_UNITS_CU_DEREFERENCE, EE_UNITS_CU_CLEAR_HOLDER_FIELD]);
        assert_eq!(r.log[1].2, 0x50);
        let mut r = Rec::default();
        clear_holder(&mut r, 1);
        assert_eq!(ops(&r), [EE_UNITS_CU_HOLDER, EE_UNITS_CU_CLEAR_HOLDER_FIELD]);
        let mut r = Rec {
            answers: vec![(EE_UNITS_CU_AFFECTING_COUNT, 2), (EE_UNITS_CU_AFFECTING_KIND, 5)],
            ..Default::default()
        };
        assert!(has_effect(&mut r, 1, 5) && !has_effect(&mut r, 1, 6));
        assert_eq!(
            r.log.iter().filter(|e| e.0 == EE_UNITS_CU_AFFECTING_KIND).map(|e| e.3).collect::<Vec<_>>(),
            [0, 0, 1]
        );
    }

    #[test]
    fn the_record_bodies_and_the_distances() {
        let mut f = EeUnitsComplexUnitFields { mGarrisonCount: 3, mGarrisonPopulation: 4, ..Default::default() };
        clear_garrison(&mut f);
        assert_eq!((f.mGarrisonCount, f.mGarrisonPopulation), (0, 0));
        f.mCollisionState = 2;
        f.mCollisionResolutionCount = 1.5;
        clear_collision_state(&mut f);
        assert_eq!((f.mCollisionState, f.mCollisionTile.mX, f.mCollisionTile.mY), (0, -10000, -10000));
        assert_eq!(f.mCollisionResolutionCount, 0.0);
        assert_eq!(distance_squared([1.0, 2.0], [4.0, 6.0]), 25.0);
        assert_eq!(distance_squared_to_tile([1.5, 2.0], 4, 6), 22.25);
    }
}
