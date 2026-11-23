//! `EEComplexUnit`'s debug text (the units' lane, round 7; was EEComplexUnit.cpp): `OutOfSynchDump` (the out-of-sync
//! file's lines of a unit) and `Describe` (its one-line debug text). Every C++ statement is one operation of
//! `ee_units_cu_host` (EEComplexUnit.cpp): each `fprintf`, `sprintf` and append with the original's format; the loops
//! and choices are here. Only an out-of-sync report or the debug overlay reaches these.

use crate::units::cu_body::*;
use crate::units::cu_life::*;

// ---- the statements (`op`; `a` the FILE* or the UWideString*)
// the dump's input record (`EeUnitsComplexDumpIn`, the host's `sCuDumpIn`) filled from the unit by [`dump_fill`],
// one statement each
/// `&sCuDumpIn`
pub const EE_UNITS_CU_DUMP_IN: i32 = 4041;
/// `mPlayer->mPlayerID`
pub const EE_UNITS_CU_DUMP_PLAYER_ID: i32 = 4042;
/// `GetTileX()`, `GetTileY()` (`b` 0, 1)
pub const EE_UNITS_CU_DUMP_TILE: i32 = 4043;
/// `mAngularVelocity`'s `GetYaw()`, `GetPitch()`, `GetRoll()` (`b` 0, 1, 2) into `f[0]`
pub const EE_UNITS_CU_DUMP_ANGULAR_VELOCITY: i32 = 4044;
/// `mBuildQueue.size()`, `mCitizenBuildQueue.size()` (`b` 0, 1)
pub const EE_UNITS_CU_DUMP_QUEUE_SIZE: i32 = 4045;
/// `mCollisionTile.mX`, `.GetY()` (`b` 0, 1)
pub const EE_UNITS_CU_DUMP_COLLISION_TILE: i32 = 4046;
/// `mPhysicalModel.GetFrame()`
pub const EE_UNITS_CU_DUMP_FRAME: i32 = 4047;
/// `mProjectileStart`'s `GetX()`, `GetY()`, `GetZ()` (`b` 0, 1, 2) into `f[0]`
pub const EE_UNITS_CU_DUMP_PROJECTILE_START: i32 = 4048;
/// `mPhysicalModel.GetLocation().GetElement(b, c)` into `f[0]`
pub const EE_UNITS_CU_DUMP_LOCATION_ELEMENT: i32 = 4049;
/// `ee_units_complex_dump(b, this, this, this, GetUnitClass(), GetStats(), the input, EEDumpWrite, (FILE*)a)`
pub const EE_UNITS_CU_DUMP_PART: i32 = 901;
/// the action line
pub const EE_UNITS_CU_DUMP_ACTION: i32 = 902;
/// `mActiveGroup == NULL`
pub const EE_UNITS_CU_NO_ACTIVE_GROUP: i32 = 903;
/// the "no active group" line
pub const EE_UNITS_CU_DUMP_NO_GROUP: i32 = 904;
/// the active group's action line
pub const EE_UNITS_CU_DUMP_GROUP: i32 = 905;
/// the goals' header line
pub const EE_UNITS_CU_DUMP_GOALS: i32 = 906;
/// queue `b`'s header line (its number of goals)
pub const EE_UNITS_CU_DUMP_QUEUE: i32 = 907;
/// goal (`b`, `c`)'s kind line
pub const EE_UNITS_CU_DUMP_GOAL_KIND: i32 = 908;
/// goal (`b`, `c`)'s `DumpOutOfSynch(file)`
pub const EE_UNITS_CU_DUMP_GOAL: i32 = 909;
/// goal (`b`, `c`)'s `GetGoalLocation(the scratch location)`
pub const EE_UNITS_CU_DUMP_GOAL_LOCATION: i32 = 910;
/// the scratch location's x line
pub const EE_UNITS_CU_DUMP_LOCATION: i32 = 911;
/// goal (`b`, `c`)'s `GetIsAutoWaypoint() == true`
pub const EE_UNITS_CU_GOAL_AUTO_WAYPOINT: i32 = 912;
/// the "is an auto waypoint" line
pub const EE_UNITS_CU_DUMP_AUTO_WAYPOINT: i32 = 913;
/// the current goal's line
pub const EE_UNITS_CU_DUMP_GOAL_STATE: i32 = 914;
/// the waypoints' size line
pub const EE_UNITS_CU_DUMP_WAYPOINT_SIZE: i32 = 915;
/// waypoint `b`'s line
pub const EE_UNITS_CU_DUMP_WAYPOINT: i32 = 916;
/// the area effects' header line
pub const EE_UNITS_CU_DUMP_AREA_HEADER: i32 = 917;
/// `mAffectingAreaEffects.GetSize()`
pub const EE_UNITS_CU_AFFECTING_SIZE: i32 = 918;
/// the "affected by" header line
pub const EE_UNITS_CU_DUMP_AFFECTING_HEADER: i32 = 919;
/// `mAffectingAreaEffects.mItems[b]->Dump(file)`
pub const EE_UNITS_CU_DUMP_AFFECTING: i32 = 920;
/// the "generated" header line
pub const EE_UNITS_CU_DUMP_GENERATED_HEADER: i32 = 921;
/// `mAreaEffects.mItems[b]->Dump(file)`
pub const EE_UNITS_CU_DUMP_AREA_EFFECT: i32 = 922;
/// `mGoalState != NULL`
pub const EE_UNITS_CU_HAS_GOAL_STATE_NOW: i32 = 923;
// Describe's (`a` the caller's UWideString)
/// `EEUnit::Describe(the scratch text = UWideString())`
pub const EE_UNITS_CU_DESCRIBE_BASE: i32 = 924;
/// the action text (`"act %s:"`) into the scratch text
pub const EE_UNITS_CU_DESCRIBE_ACTION: i32 = 925;
/// the collision state text (`"coll %d"`) into the scratch text
pub const EE_UNITS_CU_DESCRIBE_COLLISION: i32 = 926;
/// `mCollisionState`
pub const EE_UNITS_CU_COLLISION_STATE: i32 = 927;
/// the collision tile text (`" / tile(%ld,%ld)"`) into the scratch text
pub const EE_UNITS_CU_DESCRIBE_COLLISION_TILE: i32 = 928;
/// `*(UWideString*)a += ":"`
pub const EE_UNITS_CU_DESCRIBE_COLON: i32 = 929;
/// the velocity text (`"vel %.4f:"`) into the scratch text
pub const EE_UNITS_CU_DESCRIBE_VELOCITY: i32 = 930;
/// the goal state's `GetDebugLine(the scratch text)`
pub const EE_UNITS_CU_DESCRIBE_GOAL: i32 = 931;
/// the goal state's `GetGoalLocation(the scratch location)`
pub const EE_UNITS_CU_DUMP_STATE_LOCATION: i32 = 932;
/// the scratch location's y line
pub const EE_UNITS_CU_DUMP_LOCATION_Y: i32 = 933;
/// the scratch location's z line
pub const EE_UNITS_CU_DUMP_LOCATION_Z: i32 = 934;
/// the "is not an auto waypoint" line
pub const EE_UNITS_CU_DUMP_NOT_AUTO_WAYPOINT: i32 = 935;
/// `*(UWideString*)a += the scratch text`
pub const EE_UNITS_CU_DESCRIBE_APPEND: i32 = 936;
/// the scratch text's `Clear()`
pub const EE_UNITS_CU_DESCRIBE_CLEAR: i32 = 937;

/// `kGoalLocation`
const GOAL_LOCATION: i64 = 1;
fn locations<H: CuHost + ?Sized>(h: &mut H, unit: u64, file: u64) {
    for op in [EE_UNITS_CU_DUMP_LOCATION, EE_UNITS_CU_DUMP_LOCATION_Y, EE_UNITS_CU_DUMP_LOCATION_Z] {
        h.cu(op, unit, file, 0, 0, &mut [0.0; 4]);
    }
}

/// `OutOfSynchDump(file, flags)`: the unit's goals, physics, class stats, waypoints and area effects in the
/// out-of-sync file `file` (a `FILE*`).
/// The dump's input record (the host's) filled from the unit: its player, tile, physics and queues.
pub fn dump_fill<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    let mut f = [0.0f32; 4];
    let mut q = |h: &mut H, op: i32, b: i64, c: i64| h.cu(op, unit, 0, b, c, &mut f);
    let at = q(h, EE_UNITS_CU_DUMP_IN, 0, 0) as usize;
    let mut d = crate::units::unit_bulk::EeUnitsComplexDumpIn {
        player_id: q(h, EE_UNITS_CU_DUMP_PLAYER_ID, 0, 0) as _,
        tile_x: q(h, EE_UNITS_CU_DUMP_TILE, 0, 0) as _,
        tile_y: q(h, EE_UNITS_CU_DUMP_TILE, 1, 0) as _,
        ..Default::default()
    };
    for i in 0..3 {
        h.cu(EE_UNITS_CU_DUMP_ANGULAR_VELOCITY, unit, 0, i as i64, 0, &mut f);
        d.angular_velocity[i] = f[0];
    }
    d.build_queue = h.cu(EE_UNITS_CU_DUMP_QUEUE_SIZE, unit, 0, 0, 0, &mut f) as usize;
    d.citizen_build_queue = h.cu(EE_UNITS_CU_DUMP_QUEUE_SIZE, unit, 0, 1, 0, &mut f) as usize;
    for i in 0..2 {
        d.collision_tile[i] = h.cu(EE_UNITS_CU_DUMP_COLLISION_TILE, unit, 0, i as i64, 0, &mut f) as _;
    }
    d.frame = h.cu(EE_UNITS_CU_DUMP_FRAME, unit, 0, 0, 0, &mut f) as _;
    for i in 0..3 {
        h.cu(EE_UNITS_CU_DUMP_PROJECTILE_START, unit, 0, i as i64, 0, &mut f);
        d.projectile_start[i] = f[0];
    }
    for r in 0..4 {
        for c in 0..4 {
            h.cu(EE_UNITS_CU_DUMP_LOCATION_ELEMENT, unit, 0, r as i64, c as i64, &mut f);
            d.location[r][c] = f[0];
        }
    }
    if at != 0 {
        // SAFETY: the host's live record (0 in a test)
        unsafe { *(at as *mut crate::units::unit_bulk::EeUnitsComplexDumpIn) = d };
    }
}

pub fn out_of_synch_dump<H: CuHost + ?Sized>(h: &mut H, unit: u64, file: u64) {
    let q = |h: &mut H, op: i32, b: i64, c: i64| h.cu(op, unit, file, b, c, &mut [0.0; 4]);
    dump_fill(h, unit);
    q(h, EE_UNITS_CU_DUMP_PART, 0, 0);
    q(h, EE_UNITS_CU_DUMP_ACTION, 0, 0);
    if q(h, EE_UNITS_CU_NO_ACTIVE_GROUP, 0, 0) != 0 {
        q(h, EE_UNITS_CU_DUMP_NO_GROUP, 0, 0);
    } else {
        q(h, EE_UNITS_CU_DUMP_GROUP, 0, 0);
    }
    q(h, EE_UNITS_CU_DUMP_GOALS, 0, 0);
    let mut i = 0;
    while i < q(h, EE_UNITS_CU_GOAL_QUEUE_COUNT, 0, 0) {
        q(h, EE_UNITS_CU_DUMP_QUEUE, i, 0);
        let count = q(h, EE_UNITS_CU_QUEUE_GOAL_COUNT, i, 0);
        for j in 0..count {
            q(h, EE_UNITS_CU_DUMP_GOAL_KIND, i, j);
            q(h, EE_UNITS_CU_DUMP_GOAL, i, j);
            q(h, EE_UNITS_CU_DUMP_GOAL_LOCATION, i, j);
            locations(h, unit, file);
            let goal = h.cu(crate::units::cu_world::EE_UNITS_CU_QUEUE_GOAL_AT, unit, 0, i, j, &mut [0.0; 4]) as u64;
            if h.cu(EE_UNITS_CU_GOAL_KIND_OF, unit, goal, 0, 0, &mut [0.0; 4]) == GOAL_LOCATION {
                if q(h, EE_UNITS_CU_GOAL_AUTO_WAYPOINT, i, j) != 0 {
                    q(h, EE_UNITS_CU_DUMP_AUTO_WAYPOINT, i, j);
                } else {
                    q(h, EE_UNITS_CU_DUMP_NOT_AUTO_WAYPOINT, i, j);
                }
            }
            q(h, EE_UNITS_CU_DUMP_GOAL, i, j);
        }
        i += 1;
    }
    if q(h, EE_UNITS_CU_HAS_GOAL_STATE_NOW, 0, 0) != 0 {
        q(h, EE_UNITS_CU_DUMP_GOAL_STATE, 0, 0);
        q(h, EE_UNITS_CU_DUMP_STATE_LOCATION, 0, 0);
        locations(h, unit, file);
    }
    q(h, EE_UNITS_CU_DUMP_PART, 1, 0);
    q(h, EE_UNITS_CU_DUMP_WAYPOINT_SIZE, 0, 0);
    let mut w = 0;
    while w < q(h, crate::units::cu_world::EE_UNITS_CU_WAYPOINT_COUNT, 0, 0) {
        q(h, EE_UNITS_CU_DUMP_WAYPOINT, w, 0);
        w += 1;
    }
    q(h, EE_UNITS_CU_DUMP_AREA_HEADER, 0, 0);
    if q(h, EE_UNITS_CU_AFFECTING_SIZE, 0, 0) != 0 {
        q(h, EE_UNITS_CU_DUMP_AFFECTING_HEADER, 0, 0);
        let mut k = 0;
        while k < q(h, EE_UNITS_CU_AFFECTING_COUNT, 0, 0) {
            q(h, EE_UNITS_CU_DUMP_AFFECTING, k, 0);
            k += 1;
        }
    }
    if q(h, crate::units::cu_world::EE_UNITS_CU_AREA_EFFECTS_SIZE, 0, 0) == 0 {
        return;
    }
    q(h, EE_UNITS_CU_DUMP_GENERATED_HEADER, 0, 0);
    let mut k = 0;
    while k < q(h, EE_UNITS_CU_AREA_EFFECT_COUNT, 0, 0) {
        q(h, EE_UNITS_CU_DUMP_AREA_EFFECT, k, 0);
        k += 1;
    }
}

/// `Describe(text)`: the debug text appended to `text` (a `UWideString*`): the base unit's, the action, the
/// collision state and tile, the velocity and the current goal's line.
pub fn describe<H: CuHost + ?Sized>(h: &mut H, unit: u64, text: u64) {
    let q = |h: &mut H, op: i32| h.cu(op, unit, text, 0, 0, &mut [0.0; 4]);
    q(h, EE_UNITS_CU_DESCRIBE_BASE);
    q(h, EE_UNITS_CU_DESCRIBE_APPEND);
    if q(h, EE_UNITS_CU_HAS_COMMAND) != 0 {
        q(h, EE_UNITS_CU_DESCRIBE_ACTION);
        q(h, EE_UNITS_CU_DESCRIBE_APPEND);
    }
    q(h, EE_UNITS_CU_DESCRIBE_COLLISION);
    q(h, EE_UNITS_CU_DESCRIBE_APPEND);
    if q(h, EE_UNITS_CU_COLLISION_STATE) != 0 {
        q(h, EE_UNITS_CU_DESCRIBE_COLLISION_TILE);
        q(h, EE_UNITS_CU_DESCRIBE_APPEND);
    }
    q(h, EE_UNITS_CU_DESCRIBE_COLON);
    q(h, EE_UNITS_CU_DESCRIBE_VELOCITY);
    q(h, EE_UNITS_CU_DESCRIBE_APPEND);
    if q(h, EE_UNITS_CU_HAS_GOAL_STATE_NOW) == 0 {
        return;
    }
    q(h, EE_UNITS_CU_DESCRIBE_CLEAR);
    q(h, EE_UNITS_CU_DESCRIBE_GOAL);
    q(h, EE_UNITS_CU_DESCRIBE_APPEND);
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::cu_body::game::Game;
    use std::os::raw::c_void;

    /// `EEComplexUnit::OutOfSynchDump(file, flags)` on `unit`.
    ///
    /// # Safety
    /// `unit` is a live `EEComplexUnit`, `file` an open `FILE*`.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_cu_out_of_synch_dump(unit: *mut c_void, file: *mut c_void) {
        out_of_synch_dump(&mut Game, unit as usize as u64, file as usize as u64);
    }

    /// `EEComplexUnit::Describe(text)` on `unit`.
    ///
    /// # Safety
    /// `unit` is a live `EEComplexUnit`, `text` a live `UWideString`.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_cu_describe(unit: *mut c_void, text: *mut c_void) {
        describe(&mut Game, unit as usize as u64, text as usize as u64);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct Mock {
        log: Vec<(i32, i64, i64)>,
        queue_count: Vec<i64>,
    }

    impl CuHost for Mock {
        fn cu(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64, _f: &mut [f32; 4]) -> i64 {
            assert_eq!((unit, a), (1, 9));
            self.log.push((op, b, c));
            match op {
                EE_UNITS_CU_GOAL_QUEUE_COUNT => {
                    if self.queue_count.is_empty() {
                        0
                    } else {
                        self.queue_count.remove(0)
                    }
                }
                EE_UNITS_CU_QUEUE_GOAL_COUNT => 2,
                EE_UNITS_CU_GOAL_KIND_OF => GOAL_LOCATION,
                EE_UNITS_CU_HAS_COMMAND | EE_UNITS_CU_COLLISION_STATE => 1,
                _ => 0,
            }
        }
    }

    impl Mock {
        fn count(&self, op: i32) -> usize {
            self.log.iter().filter(|e| e.0 == op).count()
        }
    }

    #[test]
    fn the_dump_writes_each_goal_twice_with_its_location() {
        let mut h = Mock { queue_count: vec![1, 1], ..Default::default() };
        // the goal reads go through a = 0: the mock wants a = 9 for all, so check the lines through the counts
        let mut h2 = Wrap(&mut h);
        out_of_synch_dump(&mut h2, 1, 9);
        assert_eq!(h.count(EE_UNITS_CU_DUMP_GOAL), 4);
        assert_eq!(h.count(EE_UNITS_CU_DUMP_LOCATION), 2);
        assert_eq!(h.count(EE_UNITS_CU_DUMP_LOCATION_Z), 2);
        assert_eq!(h.count(EE_UNITS_CU_DUMP_NOT_AUTO_WAYPOINT), 2);
        assert_eq!(h.count(EE_UNITS_CU_DUMP_GROUP), 1);
        assert_eq!(h.count(EE_UNITS_CU_DUMP_GENERATED_HEADER), 0);
        let parts: Vec<i64> = h.log.iter().filter(|e| e.0 == EE_UNITS_CU_DUMP_PART).map(|e| e.1).collect();
        assert_eq!(parts, [0, 1]);
    }

    #[test]
    fn describe_appends_its_parts_in_order() {
        let mut h = Mock::default();
        let mut h2 = Wrap(&mut h);
        describe(&mut h2, 1, 9);
        let ops: Vec<i32> = h.log.iter().map(|e| e.0).collect();
        assert_eq!(
            ops,
            [
                EE_UNITS_CU_DESCRIBE_BASE,
                EE_UNITS_CU_DESCRIBE_APPEND,
                EE_UNITS_CU_HAS_COMMAND,
                EE_UNITS_CU_DESCRIBE_ACTION,
                EE_UNITS_CU_DESCRIBE_APPEND,
                EE_UNITS_CU_DESCRIBE_COLLISION,
                EE_UNITS_CU_DESCRIBE_APPEND,
                EE_UNITS_CU_COLLISION_STATE,
                EE_UNITS_CU_DESCRIBE_COLLISION_TILE,
                EE_UNITS_CU_DESCRIBE_APPEND,
                EE_UNITS_CU_DESCRIBE_COLON,
                EE_UNITS_CU_DESCRIBE_VELOCITY,
                EE_UNITS_CU_DESCRIBE_APPEND,
                EE_UNITS_CU_HAS_GOAL_STATE_NOW
            ]
        );
    }

    #[test]
    fn the_dump_record_is_filled_field_by_field() {
        let mut h = Mock::default();
        let mut h2 = Wrap(&mut h);
        dump_fill(&mut h2, 1);
        assert_eq!(h.log[0].0, EE_UNITS_CU_DUMP_IN);
        assert_eq!(h.count(EE_UNITS_CU_DUMP_ANGULAR_VELOCITY), 3);
        assert_eq!(h.count(EE_UNITS_CU_DUMP_QUEUE_SIZE), 2);
        assert_eq!(h.count(EE_UNITS_CU_DUMP_COLLISION_TILE), 2);
        assert_eq!(h.count(EE_UNITS_CU_DUMP_PROJECTILE_START), 3);
        let cells: Vec<(i64, i64)> =
            h.log.iter().filter(|e| e.0 == EE_UNITS_CU_DUMP_LOCATION_ELEMENT).map(|e| (e.1, e.2)).collect();
        assert_eq!(cells.len(), 16);
        assert_eq!((cells[0], cells[5], cells[15]), ((0, 0), (1, 1), (3, 3)));
    }

    /// the reads that pass another object than the file (the goal's kind) answered as if they named the file
    struct Wrap<'a>(&'a mut Mock);

    impl CuHost for Wrap<'_> {
        fn cu(&mut self, op: i32, unit: u64, _a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
            self.0.cu(op, unit, 9, b, c, f)
        }
    }
}
