//! The unit utilities' line of fire over its C++ statements (the units' lane, round 8; were
//! EEFOFindNewTargetNearOldTarget.cpp): `EEIsLineToUnitBlocked` (its CBLB record), its host's shot start height
//! (the weapon tag point's data at the frame: CBWP), the target's top and the walls, and `EEGetTileLine` (CBTL).
//! The walk is ee_combat's `line_blocked`, the height `weapon_height`, the rasteriser `tile_line`; each C++ read is
//! one `EE_UNITS_LF_*` statement of `ee_units_lf_host` (EEFOFindNewTargetNearOldTarget.cpp), asked in the
//! original's order.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::app::capture_record::Record;
use crate::combat::geometry::{
    tile_line, weapon_height, EeCombatWeaponIn, EE_COMBAT_WEAPON_KIND_BARE, EE_COMBAT_WEAPON_KIND_BOX,
    EE_COMBAT_WEAPON_KIND_POINT, EE_COMBAT_WEAPON_KIND_TURRET,
};
use crate::combat::walks::{line_blocked, EeCombatLineIn, EeCombatTargetTop, LineHost};

// ---- the statements (`a`, `b` objects, `c`, `d` values or pointers, `f` floats out)
/// unit `a`'s `GetTileX()`
pub const EE_UNITS_LF_TILE_X: i32 = 1;
/// unit `a`'s `GetTileY()`
pub const EE_UNITS_LF_TILE_Y: i32 = 2;
/// unit `a`'s `GetUnitClass()->mSkipsLineOfFireCheck`
pub const EE_UNITS_LF_SKIPS_CHECK: i32 = 3;
/// unit `a`'s `GetHitPoints()`
pub const EE_UNITS_LF_HIT_POINTS: i32 = 4;
/// unit `a`'s `GetUnitClass()->mIsMelee`
pub const EE_UNITS_LF_MELEE: i32 = 5;
/// unit `a`'s `GetUnitID()`
pub const EE_UNITS_LF_UNIT_ID: i32 = 6;
/// unit `a`'s `GetComplexUnit()`
pub const EE_UNITS_LF_COMPLEX: i32 = 7;
/// the 16 floats at `c` = complex unit `a`'s `mPhysicalModel.GetLocation().mMatrix`
pub const EE_UNITS_LF_LOCATION: i32 = 8;
/// unit `a`'s `GetModel()`
pub const EE_UNITS_LF_MODEL: i32 = 9;
/// model `a`'s `mTagPoints[complex unit b's mAnimation]`
pub const EE_UNITS_LF_TAG_LIST: i32 = 10;
/// complex unit `a`'s `mPhysicalModel.GetFrame()`
pub const EE_UNITS_LF_FRAME: i32 = 11;
/// complex unit `a`'s `mWeaponTagPoint`
pub const EE_UNITS_LF_WEAPON_INDEX: i32 = 12;
/// `f[0] =` complex unit `a`'s `mPhysicalModel.GetScale()`
pub const EE_UNITS_LF_SCALE: i32 = 13;
/// model `a`'s `GetNumParts()`
pub const EE_UNITS_LF_NUM_PARTS: i32 = 14;
/// model `a`'s `FindTurretPart(list b's GetTagPoint(c)->mModel, the unsigned long at d)`
pub const EE_UNITS_LF_FIND_TURRET: i32 = 15;
/// `f[0] =` complex unit `a`'s `mTurretAngles[c]`
pub const EE_UNITS_LF_TURRET_ANGLE: i32 = 16;
/// `*(list a's mPoints.begin() + c)`
pub const EE_UNITS_LF_LIST_POINT_AT: i32 = 17;
/// list `a`'s `mPoints.size()`
pub const EE_UNITS_LF_LIST_SIZE: i32 = 18;
/// list `a`'s `GetTagPoint(c)`
pub const EE_UNITS_LF_LIST_TAG_POINT: i32 = 19;
/// the point at `d` = model `a`'s `mModels[complex unit b's mAnimation]->GetBoundingBox(c).GetTopRightBack()`
pub const EE_UNITS_LF_BOX_TOP: i32 = 20;
/// the point at `d` = tag point `a`'s `mModel->GetTagPoint(its mIndex, c)`
pub const EE_UNITS_LF_POINT_LOCAL: i32 = 21;
/// tag point `a`'s `mModel`
pub const EE_UNITS_LF_POINT_MODEL: i32 = 22;
/// the 16 floats at `d` = model `a`'s `GetInternalTransform(c).mMatrix`
pub const EE_UNITS_LF_INTERNAL_TRANSFORM: i32 = 23;
/// model `a`'s `GetParentModel()`
pub const EE_UNITS_LF_PARENT: i32 = 24;
/// `f[0] =` unit `a`'s `mStats->GetScale()`
pub const EE_UNITS_LF_STATS_SCALE: i32 = 25;
/// complex unit `a`'s `mIsBuilt`
pub const EE_UNITS_LF_BUILT: i32 = 26;
/// complex unit `a`'s `mStats->mMaxHitPoints`
pub const EE_UNITS_LF_MAX_HIT_POINTS: i32 = 27;
/// `f[0] =` complex unit `a`'s `mPhysicalModel.GetLocation().GetZ()`
pub const EE_UNITS_LF_Z: i32 = 28;

/// `EE_COMBAT_TAG(a, b, c, d)`: the first letter in the low byte.
pub(crate) const TAG_CBLB: u32 = u32::from_le_bytes(*b"CBLB");
pub(crate) const TAG_CBWP: u32 = u32::from_le_bytes(*b"CBWP");
pub(crate) const TAG_CBTL: u32 = u32::from_le_bytes(*b"CBTL");

/// The line of fire's C++ statements and the Rust it calls.
pub trait LfHost {
    /// one statement (`EE_UNITS_LF_*`)
    fn lf(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, f: &mut [f32; 4]) -> i64;
    /// `EEIsTileTopAbove(x, y, height, &wall, bare)` (uu_world.rs, noted): above, the wall found
    fn tile_top_above(&mut self, x: i32, y: i32, height: f32, bare: bool) -> (bool, u64);
    /// `EESimCaptureRecord::Active()`
    fn capture(&mut self) -> bool;
    /// a record's `Commit()`
    fn commit(&mut self, record: &Record);
}

fn op<H: LfHost + ?Sized>(h: &mut H, code: i32, a: u64) -> i64 {
    h.lf(code, a, 0, 0, 0, &mut [0.0; 4])
}

fn opf<H: LfHost + ?Sized>(h: &mut H, code: i32, a: u64, c: i64) -> f32 {
    let mut f = [0.0; 4];
    h.lf(code, a, 0, c, 0, &mut f);
    f[0]
}

/// `EEGetTileLine(x1, y1, x2, y2, &tiles, ends, thin)`: the tiles (ee_combat's rasteriser); CBTL records them.
pub fn tile_line_recorded<H: LfHost + ?Sized>(
    h: &mut H,
    x1: i32,
    y1: i32,
    x2: i32,
    y2: i32,
    ends: bool,
    thin: bool,
) -> Vec<(i32, i32)> {
    let tiles = tile_line(x1, y1, x2, y2, ends, thin);
    if h.capture() {
        let mut r = Record::new(TAG_CBTL);
        for v in [x1, y1, x2, y2, i32::from(ends), i32::from(thin)] {
            r.i32(v);
        }
        r.i32(tiles.len() as i32);
        for &(x, y) in &tiles {
            r.i32(x);
            r.i32(y);
        }
        h.commit(&r);
    }
    tiles
}

/// The shot's start: the shooter's height, or (not bare) its weapon tag point's. The model's data at its frame
/// (the tag point, the model chain's transforms, the turret's yaw, the box); the arithmetic is ee_combat's
/// `weapon_height`; CBWP records it.
fn start_height<H: LfHost + ?Sized>(h: &mut H, unit: u64, bare: bool) -> f32 {
    let complex = op(h, EE_UNITS_LF_COMPLEX, unit) as u64;
    let mut i = EeCombatWeaponIn::default();
    let mut chain: Vec<[f32; 16]> = Vec::new();
    h.lf(EE_UNITS_LF_LOCATION, complex, 0, i.location.as_mut_ptr() as i64, 0, &mut [0.0; 4]);
    if bare {
        i.kind = EE_COMBAT_WEAPON_KIND_BARE;
    } else {
        let model = op(h, EE_UNITS_LF_MODEL, unit) as u64;
        let list = h.lf(EE_UNITS_LF_TAG_LIST, model, complex, 0, 0, &mut [0.0; 4]) as u64;
        let frame = op(h, EE_UNITS_LF_FRAME, complex) as c_ulong;
        let index = op(h, EE_UNITS_LF_WEAPON_INDEX, complex) as c_ulong;
        i.list_present = i32::from(list != 0);
        i.scale = opf(h, EE_UNITS_LF_SCALE, complex, 0);
        let mut point = 0u64;
        if op(h, EE_UNITS_LF_NUM_PARTS, model) as c_ulong > 0 {
            i.kind = EE_COMBAT_WEAPON_KIND_TURRET;
            let mut part: c_ulong = 0;
            let found = h.lf(
                EE_UNITS_LF_FIND_TURRET,
                model,
                list,
                index as i64,
                &mut part as *mut c_ulong as i64,
                &mut [0.0; 4],
            );
            if found as c_long == 0 {
                i.turret = 1;
                i.yaw = opf(h, EE_UNITS_LF_TURRET_ANGLE, complex, part as i64);
            }
            point = h.lf(EE_UNITS_LF_LIST_POINT_AT, list, 0, index as i64, 0, &mut [0.0; 4]) as u64;
        } else if list != 0 && op(h, EE_UNITS_LF_LIST_SIZE, list) as u64 > u64::from(index) {
            i.kind = EE_COMBAT_WEAPON_KIND_POINT;
            point = h.lf(EE_UNITS_LF_LIST_TAG_POINT, list, 0, index as i64, 0, &mut [0.0; 4]) as u64;
        } else {
            i.kind = EE_COMBAT_WEAPON_KIND_BOX;
            h.lf(EE_UNITS_LF_BOX_TOP, model, complex, frame as i64, i.top.as_mut_ptr() as i64, &mut [0.0; 4]);
        }
        if point != 0 {
            h.lf(EE_UNITS_LF_POINT_LOCAL, point, 0, frame as i64, i.point.as_mut_ptr() as i64, &mut [0.0; 4]);
            let mut link = op(h, EE_UNITS_LF_POINT_MODEL, point) as u64;
            while link != 0 {
                let mut step = [0.0f32; 16];
                h.lf(EE_UNITS_LF_INTERNAL_TRANSFORM, link, 0, frame as i64, step.as_mut_ptr() as i64, &mut [0.0; 4]);
                chain.push(step);
                i.chain += 1;
                link = op(h, EE_UNITS_LF_PARENT, link) as u64;
            }
        }
    }
    let height = weapon_height(&i, &chain);
    if h.capture() {
        let mut r = Record::new(TAG_CBWP);
        weapon_in_bytes(&mut r, &i);
        for step in &chain {
            step.iter().for_each(|&v| r.f32(v));
        }
        r.f32(height);
        h.commit(&r);
    }
    height
}

/// `theRecord.Bytes(&in, sizeof(in))` of an `EeCombatWeaponIn` (its fields in order: no padding).
fn weapon_in_bytes(r: &mut Record, i: &EeCombatWeaponIn) {
    r.i32(i.kind);
    r.i32(i.list_present);
    i.location.iter().for_each(|&v| r.f32(v));
    r.f32(i.scale);
    r.i32(i.turret);
    r.f32(i.yaw);
    i.point.iter().for_each(|&v| r.f32(v));
    i.top.iter().for_each(|&v| r.f32(v));
    r.i32(i.chain);
}

/// The target's top (nothing for a target without a complex unit).
fn target_top<H: LfHost + ?Sized>(h: &mut H, target: u64) -> EeCombatTargetTop {
    let mut t = EeCombatTargetTop::default();
    let complex = op(h, EE_UNITS_LF_COMPLEX, target) as u64;
    if complex == 0 {
        return t;
    }
    let frame = op(h, EE_UNITS_LF_FRAME, complex) as c_ulong;
    t.present = 1;
    let model = op(h, EE_UNITS_LF_MODEL, target) as u64;
    let mut top = [0.0f32; 3];
    h.lf(EE_UNITS_LF_BOX_TOP, model, complex, frame as i64, top.as_mut_ptr() as i64, &mut [0.0; 4]);
    t.box_top = top[2];
    t.scale = opf(h, EE_UNITS_LF_STATS_SCALE, target, 0);
    t.built = i32::from(op(h, EE_UNITS_LF_BUILT, complex) != 0);
    t.hit_points = op(h, EE_UNITS_LF_HIT_POINTS, complex) as c_long as i32;
    t.max_hit_points = op(h, EE_UNITS_LF_MAX_HIT_POINTS, complex) as c_long as i32;
    t.z = opf(h, EE_UNITS_LF_Z, complex, 0);
    t
}

/// The line of fire's world (the C++'s `EELineOfFireHost`): the shooter, the target, the blocker written.
struct Line<'h, H: LfHost + ?Sized> {
    h: &'h mut H,
    unit: u64,
    target: u64,
    blocker: Option<u64>,
}

impl<H: LfHost + ?Sized> LineHost for Line<'_, H> {
    fn start_height(&mut self, bare: bool) -> f32 {
        start_height(self.h, self.unit, bare)
    }
    fn tile_line(&mut self, from_x: i32, from_y: i32, to_x: i32, to_y: i32) -> Vec<(i32, i32)> {
        tile_line_recorded(self.h, from_x, from_y, to_x, to_y, false, true)
    }
    fn target_top(&mut self) -> EeCombatTargetTop {
        target_top(self.h, self.target)
    }
    fn tile_top_above(&mut self, x: i32, y: i32, height: f32, bare: bool) -> bool {
        let (above, wall) = self.h.tile_top_above(x, y, height, bare);
        self.blocker = Some(wall);
        above
    }
}

/// `EEIsLineToUnitBlocked(unit, target, &blocker, &x, &y)`: whether a wall on the tile line blocks the unit's shot;
/// the blocker (when one was asked), the last tile reached. CBLB records it.
pub fn line_to_unit_blocked<H: LfHost + ?Sized>(h: &mut H, unit: u64, target: u64) -> (bool, Option<u64>, i32, i32) {
    let i = EeCombatLineIn {
        from_x: op(h, EE_UNITS_LF_TILE_X, unit) as i32,
        from_y: op(h, EE_UNITS_LF_TILE_Y, unit) as i32,
        to_x: op(h, EE_UNITS_LF_TILE_X, target) as i32,
        to_y: op(h, EE_UNITS_LF_TILE_Y, target) as i32,
        skips_check: op(h, EE_UNITS_LF_SKIPS_CHECK, unit) as i32,
        target_alive: i32::from(op(h, EE_UNITS_LF_HIT_POINTS, target) as c_long > 0),
        bare: op(h, EE_UNITS_LF_MELEE, unit) as i32,
    };
    let mut line = Line { h, unit, target, blocker: None };
    let out = line_blocked(&i, &mut line);
    let blocker = line.blocker;
    let blocked = out.blocked != 0;
    if h.capture() {
        let id = |h: &mut H, u: u64| if u == 0 { -1 } else { op(h, EE_UNITS_LF_UNIT_ID, u) as i32 };
        let mut r = Record::new(TAG_CBLB);
        let ids = [id(h, unit), id(h, target)];
        ids.iter().for_each(|&v| r.i32(v));
        for v in [i.from_x, i.from_y, i.to_x, i.to_y] {
            r.i32(v);
        }
        r.i32(i32::from(blocked));
        r.i32(out.x);
        r.i32(out.y);
        r.i32(i32::from(blocker.is_some()));
        let b = blocker.map_or(-1, |b| id(h, b));
        r.i32(b);
        h.commit(&r);
    }
    (blocked, blocker, out.x, out.y)
}

/// `EEIsLineToUnitBlocked(unit, target, ...)`'s answer (the combat goals' line read, combat/cg_goal.rs).
#[cfg(not(test))]
pub(crate) fn line_blocked_in_game(unit: u64, target: u64) -> bool {
    line_to_unit_blocked(&mut game::Game, unit, target).0
}

/// The game's line of fire host (uu_fill.rs's bodies ask the same statements).
#[cfg(not(test))]
pub(crate) fn game_host() -> impl LfHost {
    game::Game
}

#[cfg(not(test))]
mod game {
    use super::*;

    extern "C" {
        fn ee_units_lf_host(op: i32, a: u64, b: u64, c: i64, d: i64, f: *mut [f32; 4]) -> i64;
    }

    pub(super) struct Game;

    impl LfHost for Game {
        fn lf(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, f: &mut [f32; 4]) -> i64 {
            // SAFETY: inside the C++ call that handed over its live objects
            unsafe { ee_units_lf_host(op, a, b, c, d, f) }
        }
        fn tile_top_above(&mut self, x: i32, y: i32, height: f32, bare: bool) -> (bool, u64) {
            let mut wall = 0u64;
            // SAFETY: a local out value
            let above =
                unsafe { crate::units::uu_ffi::ee_units_uu_tile_top_above(x, y, height, u8::from(bare), &mut wall) };
            (above != 0, wall)
        }
        fn capture(&mut self) -> bool {
            crate::app::capture_record::active()
        }
        fn commit(&mut self, record: &Record) {
            record.commit();
        }
    }

    /// `EEIsLineToUnitBlocked`: `blocker` written only when a tile was asked; `x`, `y` the last tile reached.
    ///
    /// # Safety
    /// `blocker` points to a pointer, `x` and `y` to longs.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_lf_line_blocked(
        unit: u64,
        target: u64,
        blocker: *mut u64,
        x: *mut c_long,
        y: *mut c_long,
    ) -> u8 {
        let (blocked, wall, last_x, last_y) = line_to_unit_blocked(&mut Game, unit, target);
        // SAFETY: the caller's contract
        unsafe {
            if let Some(w) = wall {
                *blocker = w;
            }
            *x = last_x as c_long;
            *y = last_y as c_long;
        }
        u8::from(blocked)
    }

    /// `EEGetTileLine` into a C++ tile vector (replaced).
    ///
    /// # Safety
    /// `out` a live `std::vector<U2DPoint<long> >`.
    #[no_mangle]
    #[allow(clippy::too_many_arguments)]
    pub unsafe extern "C" fn ee_units_lf_tile_line(
        x1: i32,
        y1: i32,
        x2: i32,
        y2: i32,
        ends: u8,
        thin: u8,
        out: *mut std::ffi::c_void,
    ) {
        let tiles = tile_line_recorded(&mut Game, x1, y1, x2, y2, ends != 0, thin != 0);
        let tiles: Vec<[i32; 2]> = tiles.iter().map(|&(x, y)| [x, y]).collect();
        // SAFETY: the caller's contract
        unsafe { crate::units::uu_host::set_tiles(out, &tiles) };
    }
}

#[cfg(test)]
#[path = "uu_line_tests.rs"]
mod tests;
