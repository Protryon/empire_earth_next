//! The projectiles' and missiles' impact tests (R5b.2, fifth round): `EEUCProjectile::CheckGroundImpact` and
//! `CheckWaterImpact`, `EEUCMissile::CheckGroundImpact` and `CheckWaterImpact`, `EEUCTorpedo::CheckGroundImpact`.
//! The C++ gathers the reads every test makes (the flight time, the position, the goal's kind) into a record and
//! answers the lazy ones through a host table, in the original's order and only where it reads them (the ground
//! height, the off-map tile's height, the attack target's footprint, the goal's location, the missile's target tiles
//! and same-tile test, the terrain test); Rust writes each lazy answer into the record (with its flag, for the capture)
//! and decides. The C++ applies the outcome: the attack-ground goal, the missile's placement on the ground and its
//! tile, the shell hole.

use ee_ge::types::Ge3DPoint;
use std::ffi::c_void;

/// `EEUCProjectile::CheckGroundImpact`.
pub const EE_COMBAT_IMPACT_PROJECTILE_GROUND: i32 = 0;
/// `EEUCProjectile::CheckWaterImpact`.
pub const EE_COMBAT_IMPACT_PROJECTILE_WATER: i32 = 1;
/// `EEUCMissile::CheckGroundImpact`.
pub const EE_COMBAT_IMPACT_MISSILE_GROUND: i32 = 2;
/// `EEUCMissile::CheckWaterImpact`.
pub const EE_COMBAT_IMPACT_MISSILE_WATER: i32 = 3;
/// `EEUCTorpedo::CheckGroundImpact`.
pub const EE_COMBAT_IMPACT_TORPEDO_GROUND: i32 = 4;

/// No new goal.
pub const EE_COMBAT_IMPACT_GOAL_NONE: i32 = 0;
/// An attack-ground goal at the ground point (the position's x and y, the ground's height).
pub const EE_COMBAT_IMPACT_GOAL_GROUND: i32 = 1;
/// An attack-ground goal at the position on the water plane (height 0).
pub const EE_COMBAT_IMPACT_GOAL_WATER: i32 = 2;

// the goal kinds the tests compare (ee/EEUnitGoal.h)
const GOAL_ATTACK_UNIT: i32 = 2;
const GOAL_ATTACK_GROUND: i32 = 3;
// the original's constants: 0x447338 (a projectile launched below it may hit the ground at once), 0x447230 and
// 0x447234 (the missile's height over a target it is not on, its proximity to an attack target)
const PROJECTILE_LOW_HEIGHT: f32 = 3.0;
const MISSILE_HEIGHT_OFFSET: f32 = 0.3;
const MISSILE_PROXIMITY_SQUARED: f32 = 2.25;

/// An impact test's reads (the capture's CBGI and CBWI layout). The `*_read` fields and the values beside them are
/// the lazy reads, written by the test.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatImpactIn {
    /// `EE_COMBAT_IMPACT_*`
    pub kind: i32,
    /// the class is a bomb's (projectile)
    pub bomb: i32,
    /// `mFlightTime` (projectile: ms flown; missile: the launch delay left)
    pub flight_time: i32,
    /// `mProjectileStart`'s height (projectile ground)
    pub start_z: f32,
    pub position: [f32; 3],
    /// the floor tile under the position is on the map (projectile ground)
    pub tile_present: i32,
    /// the unit's tile's height, read for an off-map position (projectile ground)
    pub tile_height: f32,
    pub has_goal: i32,
    /// the goal's `mKind` (0 without one)
    pub goal_kind: i32,
    /// the goal's location, read past the first test (missile ground)
    pub goal_location: [f32; 3],
    /// the attack target is a land building or a wall, its tile, its size, the unit's tile; read at an impact under
    /// an attack-unit goal (projectile ground)
    pub target_building: i32,
    pub target_tile: [i32; 2],
    pub target_size: i32,
    pub unit_tile: [i32; 2],
    /// the class has a shell hole effect
    pub shell_hole: i32,
    /// the launcher is a missile base (`kUnitTypeCpMissileBase`, `kUnitTypeMissileBase`) (missile ground)
    pub launcher_cp_base: i32,
    pub launcher_missile_base: i32,
    pub ground_read: i32,
    /// `EEMap::GetGroundPoint`'s height under the position
    pub ground_z: f32,
    /// `EEIsOnTargetTiles` (missile ground, an attack-unit goal)
    pub on_target_read: i32,
    pub on_target: i32,
    /// the goal location's tile is the unit's (missile ground, a missile base's)
    pub same_tile_read: i32,
    pub same_tile: i32,
    /// `CanMoveOnTerrain` on the unit's tile (water, at or below height 0)
    pub move_read: i32,
    pub can_move: i32,
}

/// An impact test's outcome.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatImpactOut {
    /// the test's answer
    pub result: i32,
    /// where it returned (for the capture): projectile ground 0 not yet, 1 above the ground, 2 impact; missile ground
    /// 0 not its goal, 1 boosting or near its target, 2 no goal, 3 above the ground, 4 on the ground; water 0 not yet
    /// (projectile), 1 dry or above, 2 impact; torpedo 1 above, 2 impact
    pub stage: i32,
    /// `EE_COMBAT_IMPACT_GOAL_*`
    pub goal: i32,
    pub goal_point: [f32; 3],
    /// leave a shell hole (projectile: at the ground point; missile: at the position placed on the ground)
    pub decal: i32,
    /// the missile is placed on the ground and moved `offset` up (missile ground)
    pub place: i32,
    pub offset: f32,
}

/// The attack target's footprint (projectile ground).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatImpactTarget {
    pub building: i32,
    pub tile: [i32; 2],
    pub size: i32,
    pub unit_tile: [i32; 2],
}

/// The lazy reads.
pub trait ImpactHost {
    /// `EEMap::GetGroundPoint`'s height under `p`
    fn ground_z(&mut self, p: [f32; 3]) -> f32;
    /// the unit's tile's height
    fn tile_height(&mut self) -> f32;
    /// the attack-unit goal's target footprint and the unit's tile
    fn target(&mut self) -> EeCombatImpactTarget;
    /// `GetGoalLocation`
    fn goal_location(&mut self) -> [f32; 3];
    /// `EEIsOnTargetTiles` of the attack-unit goal's target
    fn on_target_tiles(&mut self) -> bool;
    /// the tile of (`UMFloatToLong(x)`, `UMFloatToLong(y)`) is the unit's
    fn same_tile(&mut self, x: f32, y: f32) -> bool;
    /// `CanMoveOnTerrain` on the unit's tile's terrain
    fn can_move(&mut self) -> bool;
}

fn ground_goal(i: &EeCombatImpactIn, z: f32) -> [f32; 3] {
    [i.position[0], i.position[1], z]
}

/// `EEUCProjectile::CheckGroundImpact`: a bomb, a low launch or a projectile after 1 s of flight at or below the
/// ground attacks the ground there (unless it came down on its target building's or wall's tiles); a shell hole on
/// land.
pub fn projectile_ground(i: &mut EeCombatImpactIn, host: &mut dyn ImpactHost) -> EeCombatImpactOut {
    let mut out = EeCombatImpactOut::default();
    if i.bomb == 0 && !(i.start_z < PROJECTILE_LOW_HEIGHT) && i.flight_time <= 1000 {
        return out;
    }
    let ground = if i.tile_present != 0 {
        i.ground_read = 1;
        i.ground_z = host.ground_z(i.position);
        i.ground_z
    } else {
        i.tile_height = host.tile_height();
        i.tile_height
    };
    if !(i.position[2] <= ground) {
        out.stage = 1;
        return out;
    }
    let mut on_target = false;
    if i.has_goal != 0 && i.goal_kind == GOAL_ATTACK_UNIT {
        let t = host.target();
        i.target_building = t.building;
        i.target_tile = t.tile;
        i.target_size = t.size;
        i.unit_tile = t.unit_tile;
        if t.building != 0 {
            // unsigned long compares (32 bits, wrapping) as the original's
            let (x, y) = (t.tile[0] as u32, t.tile[1] as u32);
            let (ux, uy) = (t.unit_tile[0] as u32, t.unit_tile[1] as u32);
            let size = t.size as u32;
            on_target = ux >= x && ux < x.wrapping_add(size) && uy >= y && uy < y.wrapping_add(size);
        }
    }
    out.stage = 2;
    if !on_target {
        out.goal = EE_COMBAT_IMPACT_GOAL_GROUND;
        out.goal_point = ground_goal(i, ground);
    }
    out.decal = (ground >= 0.0 && i.shell_hole != 0) as i32;
    out.result = 1;
    out
}

/// The water test of a projectile (after its gate) and a missile: at or below height 0 on terrain it cannot cross,
/// attack the ground there on the water plane.
fn water(i: &mut EeCombatImpactIn, host: &mut dyn ImpactHost, out: &mut EeCombatImpactOut) {
    out.stage = 1;
    if i.position[2] <= 0.0 {
        i.move_read = 1;
        i.can_move = host.can_move() as i32;
        if i.can_move == 0 {
            out.stage = 2;
            out.result = 1;
            out.goal = EE_COMBAT_IMPACT_GOAL_WATER;
            out.goal_point = [i.position[0], i.position[1], 0.0];
        }
    }
}

/// `EEUCProjectile::CheckWaterImpact`: a bomb, or a projectile after 1 s of flight.
pub fn projectile_water(i: &mut EeCombatImpactIn, host: &mut dyn ImpactHost) -> EeCombatImpactOut {
    let mut out = EeCombatImpactOut::default();
    if i.flight_time > 1000 || i.bomb != 0 {
        water(i, host, &mut out);
    }
    out
}

/// `EEUCMissile::CheckWaterImpact`.
pub fn missile_water(i: &mut EeCombatImpactIn, host: &mut dyn ImpactHost) -> EeCombatImpactOut {
    let mut out = EeCombatImpactOut::default();
    water(i, host, &mut out);
    out
}

/// `EEUCMissile::CheckGroundImpact`: a missile past its launch delay and not near its attack target comes down
/// below the ground (0.3 over it when it is not on its target's tiles, or over a missile base's own tile): placed on
/// the ground; true for an attack-ground goal (with a shell hole). A missile base's missiles never land.
pub fn missile_ground(i: &mut EeCombatImpactIn, host: &mut dyn ImpactHost) -> EeCombatImpactOut {
    let mut out = EeCombatImpactOut::default();
    let base = i.launcher_cp_base != 0;
    let goal = i.has_goal != 0;
    let attack_ground = i.goal_kind == GOAL_ATTACK_GROUND;
    if (!base && !goal) || (base && goal && !attack_ground) {
        return out;
    }
    let mut near = false;
    if goal {
        i.goal_location = host.goal_location();
        let p = Ge3DPoint { x: i.position[0], y: i.position[1], z: i.position[2] };
        let t = Ge3DPoint { x: i.goal_location[0], y: i.goal_location[1], z: i.goal_location[2] };
        near = !attack_ground && !(p.distance_to_squared(t) > MISSILE_PROXIMITY_SQUARED);
    }
    if i.flight_time != 0 || near {
        out.stage = 1;
        return out;
    }
    if !goal {
        out.stage = 2;
        return out;
    }
    let mut off_target = false;
    if i.goal_kind == GOAL_ATTACK_UNIT {
        i.on_target_read = 1;
        i.on_target = host.on_target_tiles() as i32;
        off_target = i.on_target == 0;
    }
    if !off_target && base {
        i.same_tile_read = 1;
        i.same_tile = host.same_tile(i.goal_location[0], i.goal_location[1]) as i32;
    }
    let offset = if off_target || i.same_tile != 0 { MISSILE_HEIGHT_OFFSET } else { 0.0 };
    i.ground_read = 1;
    i.ground_z = host.ground_z(i.position);
    out.offset = offset;
    out.stage = 3;
    if i.position[2] < i.ground_z + offset && i.ground_z >= 0.0 && i.launcher_missile_base == 0 {
        out.stage = 4;
        out.place = 1;
        out.result = attack_ground as i32;
        out.decal = (attack_ground && i.shell_hole != 0) as i32;
    }
    out
}

/// `EEUCTorpedo::CheckGroundImpact`: at or below ground at or above sea level, attack the ground there.
pub fn torpedo_ground(i: &mut EeCombatImpactIn, host: &mut dyn ImpactHost) -> EeCombatImpactOut {
    let mut out = EeCombatImpactOut { stage: 1, ..Default::default() };
    i.ground_read = 1;
    i.ground_z = host.ground_z(i.position);
    if i.position[2] <= i.ground_z && i.ground_z >= 0.0 {
        out.stage = 2;
        out.result = 1;
        out.goal = EE_COMBAT_IMPACT_GOAL_GROUND;
        out.goal_point = ground_goal(i, i.ground_z);
    }
    out
}

/// The test of `i.kind`.
pub fn impact(i: &mut EeCombatImpactIn, host: &mut dyn ImpactHost) -> EeCombatImpactOut {
    match i.kind {
        EE_COMBAT_IMPACT_PROJECTILE_GROUND => projectile_ground(i, host),
        EE_COMBAT_IMPACT_PROJECTILE_WATER => projectile_water(i, host),
        EE_COMBAT_IMPACT_MISSILE_GROUND => missile_ground(i, host),
        EE_COMBAT_IMPACT_MISSILE_WATER => missile_water(i, host),
        _ => torpedo_ground(i, host),
    }
}

/// A captured record's lazy reads as a host (the replay): answers what the record holds.
pub struct RecordHost(pub EeCombatImpactIn);

impl ImpactHost for RecordHost {
    fn ground_z(&mut self, _p: [f32; 3]) -> f32 {
        self.0.ground_z
    }
    fn tile_height(&mut self) -> f32 {
        self.0.tile_height
    }
    fn target(&mut self) -> EeCombatImpactTarget {
        EeCombatImpactTarget {
            building: self.0.target_building,
            tile: self.0.target_tile,
            size: self.0.target_size,
            unit_tile: self.0.unit_tile,
        }
    }
    fn goal_location(&mut self) -> [f32; 3] {
        self.0.goal_location
    }
    fn on_target_tiles(&mut self) -> bool {
        self.0.on_target != 0
    }
    fn same_tile(&mut self, _x: f32, _y: f32) -> bool {
        self.0.same_tile != 0
    }
    fn can_move(&mut self) -> bool {
        self.0.can_move != 0
    }
}

/// A captured record's input as the C++ hands it, before the test's lazy reads.
pub fn eager_part(i: &EeCombatImpactIn) -> EeCombatImpactIn {
    EeCombatImpactIn {
        tile_height: 0.0,
        goal_location: [0.0; 3],
        target_building: 0,
        target_tile: [0; 2],
        target_size: 0,
        unit_tile: [0; 2],
        ground_read: 0,
        ground_z: 0.0,
        on_target_read: 0,
        on_target: 0,
        same_tile_read: 0,
        same_tile: 0,
        move_read: 0,
        can_move: 0,
        ..*i
    }
}

/// Replays a captured record: the test over its eager part, the host answering from the record; the record it
/// writes (the lazy reads and their flags) and the outcome.
pub fn replay(i: &EeCombatImpactIn) -> (EeCombatImpactIn, EeCombatImpactOut) {
    let mut input = eager_part(i);
    let out = impact(&mut input, &mut RecordHost(*i));
    (input, out)
}

// ---- the C ABI

/// The host's lazy reads (`ctx` is the C++'s).
#[repr(C)]
pub struct EeCombatImpactHost {
    pub ctx: *mut c_void,
    pub ground_z: unsafe extern "C" fn(ctx: *mut c_void, x: f32, y: f32, z: f32) -> f32,
    pub tile_height: unsafe extern "C" fn(ctx: *mut c_void) -> f32,
    pub target: unsafe extern "C" fn(ctx: *mut c_void, out: *mut EeCombatImpactTarget),
    pub goal_location: unsafe extern "C" fn(ctx: *mut c_void, out: *mut f32),
    pub on_target_tiles: unsafe extern "C" fn(ctx: *mut c_void) -> i32,
    pub same_tile: unsafe extern "C" fn(ctx: *mut c_void, x: f32, y: f32) -> i32,
    pub can_move: unsafe extern "C" fn(ctx: *mut c_void) -> i32,
}

struct CHost<'a>(&'a EeCombatImpactHost);

impl ImpactHost for CHost<'_> {
    fn ground_z(&mut self, p: [f32; 3]) -> f32 {
        // SAFETY: the table's contract
        unsafe { (self.0.ground_z)(self.0.ctx, p[0], p[1], p[2]) }
    }
    fn tile_height(&mut self) -> f32 {
        // SAFETY: the table's contract
        unsafe { (self.0.tile_height)(self.0.ctx) }
    }
    fn target(&mut self) -> EeCombatImpactTarget {
        let mut t = EeCombatImpactTarget::default();
        // SAFETY: the table's contract; `t` is a valid record
        unsafe { (self.0.target)(self.0.ctx, &mut t) };
        t
    }
    fn goal_location(&mut self) -> [f32; 3] {
        let mut p = [0.0f32; 3];
        // SAFETY: the table's contract; `p` holds three floats
        unsafe { (self.0.goal_location)(self.0.ctx, p.as_mut_ptr()) };
        p
    }
    fn on_target_tiles(&mut self) -> bool {
        // SAFETY: the table's contract
        unsafe { (self.0.on_target_tiles)(self.0.ctx) != 0 }
    }
    fn same_tile(&mut self, x: f32, y: f32) -> bool {
        // SAFETY: the table's contract
        unsafe { (self.0.same_tile)(self.0.ctx, x, y) != 0 }
    }
    fn can_move(&mut self) -> bool {
        // SAFETY: the table's contract
        unsafe { (self.0.can_move)(self.0.ctx) != 0 }
    }
}

/// The impact test of `input.kind`; the lazy reads go to `host` and are written into `*input`.
///
/// # Safety
/// `input` must point to a valid record and `host` to a valid table whose functions may be called with its `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_impact(
    input: *mut EeCombatImpactIn,
    host: *const EeCombatImpactHost,
) -> EeCombatImpactOut {
    // a copy: no Rust reference to the C++'s record lives across the host's calls
    let mut record = unsafe { *input };
    let out = impact(&mut record, &mut CHost(unsafe { &*host }));
    unsafe { *input = record };
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A host with fixed answers that counts its calls.
    #[derive(Default)]
    struct Fixed {
        ground: f32,
        height: f32,
        target: EeCombatImpactTarget,
        location: [f32; 3],
        on_target: bool,
        same: bool,
        movable: bool,
        calls: Vec<&'static str>,
    }

    impl ImpactHost for Fixed {
        fn ground_z(&mut self, _p: [f32; 3]) -> f32 {
            self.calls.push("ground");
            self.ground
        }
        fn tile_height(&mut self) -> f32 {
            self.calls.push("height");
            self.height
        }
        fn target(&mut self) -> EeCombatImpactTarget {
            self.calls.push("target");
            self.target
        }
        fn goal_location(&mut self) -> [f32; 3] {
            self.calls.push("location");
            self.location
        }
        fn on_target_tiles(&mut self) -> bool {
            self.calls.push("on target");
            self.on_target
        }
        fn same_tile(&mut self, _x: f32, _y: f32) -> bool {
            self.calls.push("same tile");
            self.same
        }
        fn can_move(&mut self) -> bool {
            self.calls.push("move");
            self.movable
        }
    }

    fn projectile(z: f32) -> EeCombatImpactIn {
        EeCombatImpactIn {
            kind: EE_COMBAT_IMPACT_PROJECTILE_GROUND,
            flight_time: 1200,
            start_z: 5.0,
            position: [10.5, 20.5, z],
            tile_present: 1,
            ..Default::default()
        }
    }

    #[test]
    fn projectile_ground_gate_and_reads() {
        let mut h = Fixed { ground: 1.0, ..Default::default() };
        // a high launch in its first second: no read at all
        let mut i = EeCombatImpactIn { flight_time: 1000, ..projectile(0.5) };
        assert_eq!(projectile_ground(&mut i, &mut h).stage, 0);
        assert!(h.calls.is_empty());
        // NaN start height fails the low-launch test as the original's `!(z < 3)`
        let mut i = EeCombatImpactIn { flight_time: 0, start_z: f32::NAN, ..projectile(0.5) };
        assert_eq!(projectile_ground(&mut i, &mut h).stage, 0);
        // above the ground
        let mut i = projectile(1.5);
        let o = projectile_ground(&mut i, &mut h);
        assert_eq!((o.stage, o.result, i.ground_read, i.ground_z), (1, 0, 1, 1.0));
        // at the ground: attack ground at (x, y, ground), a shell hole on land
        let mut i = EeCombatImpactIn { shell_hole: 1, ..projectile(1.0) };
        let o = projectile_ground(&mut i, &mut h);
        assert_eq!((o.stage, o.result, o.goal, o.goal_point, o.decal), (2, 1, 1, [10.5, 20.5, 1.0], 1));
        // off the map: the unit tile's height, no ground read
        let mut h = Fixed { height: -1.0, ..Default::default() };
        let mut i = EeCombatImpactIn { tile_present: 0, shell_hole: 1, ..projectile(-1.0) };
        let o = projectile_ground(&mut i, &mut h);
        assert_eq!((i.ground_read, i.tile_height, o.decal, o.goal_point[2]), (0, -1.0, 0, -1.0));
        assert_eq!(h.calls, ["height"]);
    }

    #[test]
    fn projectile_ground_on_its_target_building() {
        let target = EeCombatImpactTarget { building: 1, tile: [10, 20], size: 2, unit_tile: [11, 21] };
        let mut h = Fixed { ground: 1.0, target, ..Default::default() };
        let mut i = EeCombatImpactIn { has_goal: 1, goal_kind: GOAL_ATTACK_UNIT, ..projectile(0.0) };
        let o = projectile_ground(&mut i, &mut h);
        assert_eq!((o.result, o.goal), (1, EE_COMBAT_IMPACT_GOAL_NONE));
        assert_eq!((i.target_tile, i.unit_tile, i.target_size), ([10, 20], [11, 21], 2));
        // one past the footprint, or not a building: attack the ground
        for t in
            [EeCombatImpactTarget { unit_tile: [12, 21], ..target }, EeCombatImpactTarget { building: 0, ..target }]
        {
            let mut h = Fixed { ground: 1.0, target: t, ..Default::default() };
            let mut i = EeCombatImpactIn { has_goal: 1, goal_kind: GOAL_ATTACK_UNIT, ..projectile(0.0) };
            assert_eq!(projectile_ground(&mut i, &mut h).goal, EE_COMBAT_IMPACT_GOAL_GROUND);
        }
        // a dead unit's tile (-10000, unsigned) is never on a target
        let t = EeCombatImpactTarget { unit_tile: [-10000, -10000], ..target };
        let mut h = Fixed { ground: 1.0, target: t, ..Default::default() };
        let mut i = EeCombatImpactIn { has_goal: 1, goal_kind: GOAL_ATTACK_UNIT, ..projectile(0.0) };
        assert_eq!(projectile_ground(&mut i, &mut h).goal, EE_COMBAT_IMPACT_GOAL_GROUND);
        // an attack-ground goal reads no target
        let mut h = Fixed { ground: 1.0, ..Default::default() };
        let mut i = EeCombatImpactIn { has_goal: 1, goal_kind: GOAL_ATTACK_GROUND, ..projectile(0.0) };
        projectile_ground(&mut i, &mut h);
        assert_eq!(h.calls, ["ground"]);
    }

    #[test]
    fn water_tests() {
        let mut h = Fixed::default();
        let mut i = EeCombatImpactIn {
            kind: EE_COMBAT_IMPACT_PROJECTILE_WATER,
            flight_time: 900,
            position: [1.0, 2.0, -0.5],
            ..Default::default()
        };
        assert_eq!(impact(&mut i, &mut h).stage, 0);
        assert!(h.calls.is_empty());
        i.bomb = 1;
        let o = impact(&mut i, &mut h);
        assert_eq!((o.stage, o.result, o.goal, o.goal_point, i.move_read), (2, 1, 2, [1.0, 2.0, 0.0], 1));
        let mut h = Fixed { movable: true, ..Default::default() };
        let mut i =
            EeCombatImpactIn { kind: EE_COMBAT_IMPACT_MISSILE_WATER, position: [1.0, 2.0, 0.0], ..Default::default() };
        let o = impact(&mut i, &mut h);
        assert_eq!((o.stage, o.result, i.can_move), (1, 0, 1));
        // above the water: no terrain test
        let mut h = Fixed::default();
        let mut i =
            EeCombatImpactIn { kind: EE_COMBAT_IMPACT_MISSILE_WATER, position: [1.0, 2.0, 0.25], ..Default::default() };
        assert_eq!(impact(&mut i, &mut h).stage, 1);
        assert!(h.calls.is_empty());
    }

    fn missile(z: f32, kind: i32) -> EeCombatImpactIn {
        EeCombatImpactIn {
            kind: EE_COMBAT_IMPACT_MISSILE_GROUND,
            position: [10.0, 10.0, z],
            has_goal: 1,
            goal_kind: kind,
            shell_hole: 1,
            ..Default::default()
        }
    }

    #[test]
    fn missile_ground_tests() {
        // no goal and no missile base: nothing read
        let mut h = Fixed::default();
        let mut i = EeCombatImpactIn { has_goal: 0, ..missile(0.0, 0) };
        assert_eq!(missile_ground(&mut i, &mut h).stage, 0);
        // a missile base's missile with an attack-unit goal: nothing read
        let mut i = EeCombatImpactIn { launcher_cp_base: 1, ..missile(0.0, GOAL_ATTACK_UNIT) };
        assert_eq!(missile_ground(&mut i, &mut h).stage, 0);
        assert!(h.calls.is_empty());
        // boosting
        let mut h = Fixed { location: [50.0, 50.0, 0.0], ..Default::default() };
        let mut i = EeCombatImpactIn { flight_time: 5, ..missile(0.0, GOAL_ATTACK_GROUND) };
        assert_eq!(missile_ground(&mut i, &mut h).stage, 1);
        // near its attack target (1.5 away: 2.25, not more)
        let mut h = Fixed { location: [10.0, 11.5, 0.0], ..Default::default() };
        let mut i = missile(0.0, GOAL_ATTACK_UNIT);
        assert_eq!(missile_ground(&mut i, &mut h).stage, 1);
        assert_eq!(h.calls, ["location"]);
        // a missile base's missile without a goal
        let mut i = EeCombatImpactIn { has_goal: 0, launcher_cp_base: 1, ..missile(0.0, 0) };
        assert_eq!(missile_ground(&mut i, &mut Fixed::default()).stage, 2);
        // attack ground, under the ground: placed, true, a shell hole
        let mut h = Fixed { ground: 1.0, location: [40.0, 40.0, 0.0], ..Default::default() };
        let mut i = missile(0.5, GOAL_ATTACK_GROUND);
        let o = missile_ground(&mut i, &mut h);
        assert_eq!((o.stage, o.place, o.result, o.decal, o.offset), (4, 1, 1, 1, 0.0));
        assert_eq!(h.calls, ["location", "ground"]);
        // attack unit off its target's tiles: 0.3 up, placed but false
        let mut h = Fixed { ground: 1.0, location: [40.0, 40.0, 0.0], ..Default::default() };
        let mut i = missile(1.2, GOAL_ATTACK_UNIT);
        let o = missile_ground(&mut i, &mut h);
        assert_eq!((o.stage, o.place, o.result, o.decal, o.offset), (4, 1, 0, 0, 0.3));
        // on them: no offset, above the ground
        let mut h = Fixed { ground: 1.0, on_target: true, location: [40.0, 40.0, 0.0], ..Default::default() };
        let mut i = missile(1.2, GOAL_ATTACK_UNIT);
        assert_eq!(missile_ground(&mut i, &mut h).stage, 3);
        assert_eq!(h.calls, ["location", "on target", "ground"]);
        // a missile base's attack-ground missile over its own tile: 0.3; a missile base's never lands; under water
        let mut h = Fixed { ground: 1.0, same: true, location: [40.0, 40.0, 0.0], ..Default::default() };
        let mut i = EeCombatImpactIn { launcher_cp_base: 1, ..missile(1.2, GOAL_ATTACK_GROUND) };
        assert_eq!(missile_ground(&mut i, &mut h).offset, 0.3);
        assert_eq!(h.calls, ["location", "same tile", "ground"]);
        let mut h = Fixed { ground: 1.0, location: [40.0, 40.0, 0.0], ..Default::default() };
        let mut i = EeCombatImpactIn { launcher_missile_base: 1, ..missile(0.0, GOAL_ATTACK_GROUND) };
        assert_eq!(missile_ground(&mut i, &mut h).stage, 3);
        let mut h = Fixed { ground: -1.0, location: [40.0, 40.0, 0.0], ..Default::default() };
        let mut i = missile(-2.0, GOAL_ATTACK_GROUND);
        assert_eq!(missile_ground(&mut i, &mut h).stage, 3);
    }

    #[test]
    fn torpedo_ground_tests() {
        let mut h = Fixed { ground: 0.5, ..Default::default() };
        let mut i =
            EeCombatImpactIn { kind: EE_COMBAT_IMPACT_TORPEDO_GROUND, position: [3.0, 4.0, 0.5], ..Default::default() };
        let o = impact(&mut i, &mut h);
        assert_eq!((o.stage, o.result, o.goal, o.goal_point), (2, 1, 1, [3.0, 4.0, 0.5]));
        let mut h = Fixed { ground: -0.5, ..Default::default() };
        let mut i = EeCombatImpactIn {
            kind: EE_COMBAT_IMPACT_TORPEDO_GROUND,
            position: [3.0, 4.0, -1.0],
            ..Default::default()
        };
        assert_eq!(impact(&mut i, &mut h).stage, 1);
    }

    #[test]
    fn replay_restores_the_lazy_reads() {
        let mut h = Fixed { ground: 1.0, on_target: true, location: [40.0, 40.0, 0.0], ..Default::default() };
        let mut i = missile(1.2, GOAL_ATTACK_UNIT);
        let o = missile_ground(&mut i, &mut h);
        assert_eq!(replay(&i), (i, o));
    }

    #[test]
    fn the_capture_replays_impacts() {
        use crate::capture::{encode, records, replay as replay_record};
        let mut h = Fixed { ground: 1.0, location: [40.0, 40.0, 0.0], ..Default::default() };
        let mut i = missile(0.5, GOAL_ATTACK_GROUND);
        let o = missile_ground(&mut i, &mut h);
        let mut file = encode(b"CBGI", &i, &o);
        let r = records(&file).unwrap();
        assert_eq!(replay_record(&r[0]), Some(Ok(true)));
        // a lazy read the test would not make (the terrain test), or another outcome: not equal
        let mut j = i;
        j.move_read = 1;
        let bad = encode(b"CBGI", &j, &o);
        assert_eq!(replay_record(&records(&bad).unwrap()[0]), Some(Ok(false)));
        let n = file.len();
        file[n - 4] ^= 1;
        assert_eq!(replay_record(&records(&file).unwrap()[0]), Some(Ok(false)));
        let mut w =
            EeCombatImpactIn { kind: EE_COMBAT_IMPACT_MISSILE_WATER, position: [1.0, 2.0, -1.0], ..Default::default() };
        let o = missile_water(&mut w, &mut Fixed::default());
        let file = encode(b"CBWI", &w, &o);
        assert_eq!(replay_record(&records(&file).unwrap()[0]), Some(Ok(true)));
        let short = crate::capture::Record { tag: crate::capture::tag(b"CBWI"), payload: &file[8..file.len() - 4] };
        assert!(matches!(replay_record(&short), Some(Err(_))));
    }
}
