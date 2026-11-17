//! The active group's decision bodies (R5b.4, the thirty-fifth round): `EEActiveGroup::Plan` (the group's goal handed
//! to its units by the group's action), `LayOutFormation` (the formation's slots and every unit's goal at its slot),
//! the four spreads that split a group by what its units can do (`SpreadAroundUnit`, `SpreadAroundResource`,
//! `SpreadAroundTile`, `SpreadForUnload`), `SplitCalamityCasters`, the attacker and mobile unit filters,
//! `ComputeValues` (the group's speeds) and the reserved destination tests (`IsReserved`, `IsCellTaken`).
//!
//! Each body is the C++ body's statements in their order over one host ([`AgHost`]): the group's units by index,
//! each read whole as an [`EeUnitsAgUnit`] where the original read the unit, and each world call (a goal made,
//! queued or cloned, an action started, a unit removed, a command sent) one host call. The arithmetic (the slot
//! order across the move direction, the nearest free slot, the headings, the speed averages and caps) is Rust,
//! f32 operation for operation; the two sorts are MSVC's `std::sort` ([`crate::msvc_sort`]). The C++ members stay
//! (EEActiveGroup.cpp) and call these through ag_host.rs.

use ee_render::ge::types::Ge3DPoint;
use ee_render::ge::um;

// ---- the original's constants (checked against the headers by header_consts.rs)
const ACTION_IDLE: i32 = 5000;
const ACTION_DISBAND: i32 = 7001;
const ACTION_DROPOFF_RESOURCE: i32 = 4011;
const ACTION_GATHER_RESOURCE: i32 = 4004;
const ACTION_ATTACK_LOCATION: i32 = 2002;
const ACTION_ATTACK_ENEMY_UNIT: i32 = 2001;
const ACTION_HEAL_UNIT: i32 = 16001;
const ACTION_UNLOAD_TRANSPORT: i32 = 12001;
const ACTION_PREPARE_TO_MOVE: i32 = 8000;
const ACTION_FIND_UNEXPLORED_AREA: i32 = 11002;
const ACTION_MOVE_TO_NEXT_PATROL_POINT: i32 = 8013;
const ACTION_MOVE_TO_STORAGE: i32 = 8009;
const ACTION_MOVE_TO_WAITING_AREA: i32 = 4023;
const ACTION_TAKEOFF: i32 = 1000;
const ACTION_AIRPORT_OPEN: i32 = 13000;
const ACTION_INITIAL_ATTACK_STATE: i32 = 2016;
const ACTION_CONVERT_ENEMY_UNIT: i32 = 2004;
const ACTION_TURN_TO_FACE_HEALEE: i32 = 16002;
const ACTION_GET_NEXT_PATROL_POINT: i32 = 8020;
const GOAL_LOCATION: i32 = 1;
const GOAL_UNLOAD_TRANSPORT: i32 = 8;
const GOAL_GATHER: i32 = 6;
const CATEGORY_AIRPLANE: u32 = 3;
const CATEGORY_PLANET_SPACESHIP: u32 = 0x22;
const CATEGORY_LAND_CITIZEN: u32 = 6;
const CATEGORY_NAVAL_CITIZEN: u32 = 0x16;
const UNIT_TYPE_AIRPORT: u32 = 0x42;
const UNIT_TYPE_NAVY_YARD: u32 = 0x41;
const UNIT_TYPE_HURRICANE: u32 = 0xb6;
const UNIT_TYPE_ANTI_MATTER_STORM: u32 = 0x294;
const UNIT_TYPE_BALLOON09_HOT_AIR_BALLOON: u32 = 0x1ff;
const UNIT_TYPE_BALLOON10_OBSERVATION_BALLOON: u32 = 0x5a;
const UNIT_TYPE_CP_MISSILE_BASE: u32 = 0x2ed;
const TERRAIN_SPACE: i32 = 12;
const RESOURCE_WOOD: u32 = 3;
const FAMILY_ANIMAL: u32 = 0x11;
/// `kAIKindAircraftCarrier`, `kAIKindSpaceCarrier` (the class's `mAIKind`)
const AG_AI_KIND_AIRCRAFT_CARRIER: u32 = 0xf;
const AG_AI_KIND_SPACE_CARRIER: u32 = 0x27;
/// the group commands `EESendGroupCommand` sends (the acknowledgement's sound kinds)
const AG_COMMAND_MOVE: u32 = 0xb8;
const AG_COMMAND_ATTACK: u32 = 0xb9;
const AG_COMMAND_HEAL: u32 = 0xba;
const AG_COMMAND_UNLOAD: u32 = 0xbb;

/// LayOutFormation's constants (0x446ce8, 0x446cec, 0x446cf0)
const FULL_SLOT_SIZE: f32 = 1.0;
const SLOW_SPEED: f32 = 0.5;
const QUARTER_TURN: f32 = 1.5707964;
const FLT_MAX: f32 = 3.402_823_5e38;

/// A unit as the bodies read it: its members, its class's and its stats' (`ee_units_ag_host`'s `EE_UNITS_AG_UNIT`
/// fills it from the objects at the moment of the read).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsAgUnit {
    pub obj: u64,
    /// `mUnitClass`, `mComplexUnit`, `mPlayer`, `mGoalState` (complex units), `mContainer` (complex units)
    pub class_obj: u64,
    pub complex: u64,
    pub player: u64,
    pub goal_state: u64,
    pub container: u64,
    pub tile_x: i32,
    pub tile_y: i32,
    pub hit_points: i32,
    pub resource_amount: i32,
    pub resource_type: u32,
    pub power: i32,
    pub formation_slot: u32,
    /// `mVelocity`; `mPhysicalModel.GetLocation()`; `mTile->GetHeight()` (0 without a tile)
    pub velocity: f32,
    pub location: [f32; 3],
    pub tile_height: f32,
    pub is_valid: u8,
    pub commandable: u8,
    pub is_built: u8,
    pub detached: u8,
    // the class's
    pub category: u32,
    pub unit_type: u32,
    pub ai_kind: u32,
    pub family: u32,
    pub tile_size: i32,
    pub air_traffic_type: i32,
    pub class_max_velocity: f32,
    pub inner_sphere: f32,
    pub can_attack_ground: u8,
    pub paratrooper_bomber: u8,
    pub can_patrol: u8,
    pub has_special_attack: u8,
    /// `IsPriest()`, `IsCrusader()`, `IsTransport()`
    pub is_priest: u8,
    pub is_crusader: u8,
    pub is_transport: u8,
    // `mStats`'
    pub max_velocity: f32,
    pub acceleration: f32,
    pub max_angular_velocity: f32,
    pub angular_acceleration: f32,
    pub range: f32,
    pub line_of_sight: f32,
    pub attack: f32,
}

impl EeUnitsAgUnit {
    fn flies(&self) -> bool {
        self.category == CATEGORY_AIRPLANE || self.category == CATEGORY_PLANET_SPACESHIP
    }
    fn citizen(&self) -> bool {
        self.category == CATEGORY_LAND_CITIZEN || self.category == CATEGORY_NAVAL_CITIZEN
    }
    fn carrier(&self) -> bool {
        self.ai_kind == AG_AI_KIND_AIRCRAFT_CARRIER || self.ai_kind == AG_AI_KIND_SPACE_CARRIER
    }
    /// `mVelocity == 0 || mTile->GetHeight() == location z`
    fn landed(&self) -> bool {
        self.velocity == 0.0 || self.tile_height == self.location[2]
    }
}

/// The group's state as the bodies read it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsAgGroup {
    pub goal: u64,
    /// the goal's `mKind` (-1: no goal)
    pub goal_kind: i32,
    pub action: i32,
    pub count: u32,
    pub speed: f32,
    pub queued: u8,
    pub moves_together: u8,
    pub special_attack: u8,
}

/// A location goal's fields.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LocationFields {
    pub b1c: bool,
    pub forced: bool,
    pub should_face: bool,
    pub yaw: f32,
}

/// A formation slot (`EEFormationSlot`).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Slot {
    pub position: [f32; 3],
    pub unit: u64,
    pub heading: f32,
}

/// The new location goal's arguments (`EEUGRust::NewLocation(point, b1c, forced, face, yaw, auto, group state)`).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct NewLocation {
    pub point: [f32; 3],
    pub b1c: bool,
    pub forced: bool,
    pub face: bool,
    pub yaw: f32,
    pub auto_waypoint: bool,
    pub group_state: u8,
}

impl NewLocation {
    fn plain(point: [f32; 3], b1c: bool) -> Self {
        NewLocation { point, b1c, forced: true, ..Default::default() }
    }
}

/// The world the bodies act on. Units, goals, groups and other objects are their addresses.
pub trait AgHost {
    // the group
    fn group(&mut self, g: u64) -> EeUnitsAgGroup;
    fn group_unit(&mut self, g: u64, i: u32) -> u64;
    /// `RemoveUnit(u)` (the virtual)
    fn remove(&mut self, g: u64, u: u64);
    fn finish(&mut self, g: u64);
    fn set_moves_together(&mut self, g: u64);
    fn request_all_paths(&mut self, g: u64);
    fn request_paths(&mut self, g: u64, units: &[u64]);
    /// `EESendGroupCommand(g, target, mPlayer->GetPlayerID(), command, 0)`
    fn send_command(&mut self, g: u64, target: u64, command: u32);
    /// the formation's centre (and heading and orientation when `yaw` is some), then `Arrange(g, slots)`
    fn arrange(&mut self, g: u64, center: [f32; 3], yaw: Option<f32>) -> Vec<Slot>;
    /// `new EEActiveGroup(g's mPlayer)`
    fn new_group(&mut self, g: u64) -> u64;
    /// `Add(u)` (the virtual)
    fn add(&mut self, g: u64, u: u64);
    /// `ng->SetFormation(g->mFormation)`
    fn set_formation(&mut self, ng: u64, g: u64);
    /// `ng->SetGoal(goal, queued)`, `mAction = action`, into g's player's pending groups, `Plan()`,
    /// `EESendGroupCommand(ng, target, ..., command, 0)`, then a random unit's acknowledgement when it has units
    #[allow(clippy::too_many_arguments)] // the C++ statements' operands
    fn launch(&mut self, g: u64, ng: u64, goal: u64, queued: bool, action: i32, target: u64, command: u32);
    /// the reserved destinations (`mDestinations` in its order)
    fn destinations(&mut self, g: u64) -> Vec<(u32, [f32; 3])>;
    /// `gWorld.GetUnit(id)`'s class's inner collision sphere (None: no unit)
    fn world_unit_sphere(&mut self, id: u32) -> Option<f32>;
    /// `ee_combat_regions_size(gWorld.mMap->mUnitRegions)`
    fn regions_size(&mut self) -> i32;
    /// `gWorldTable.GetFloatValue(kWorldGroupSpeedCapFactor)`
    fn cap_factor(&mut self) -> f32;
    /// the 17 values `mSpeed .. mCapFactor`
    fn set_values(&mut self, g: u64, values: &[f32; 17]);

    // the units
    fn unit(&mut self, u: u64) -> EeUnitsAgUnit;
    fn queued_goal(&mut self, u: u64, q: u32) -> u64;
    fn clear_goal_queue(&mut self, u: u64, q: u32);
    fn clear_goals(&mut self, u: u64, q: u32);
    fn queue_goal(&mut self, u: u64, goal: u64, q: u32);
    /// `SetGoal(goal, 0, q)`
    fn set_goal(&mut self, u: u64, goal: u64, q: u32);
    /// `mGoalQueues[q].Add(goal)`, `RefreshGoals()`, `RefreshActiveGoal()`
    fn queue_add(&mut self, u: u64, goal: u64, q: u32);
    /// `mGoalQueues[1].AddTime(1)`
    fn add_time(&mut self, u: u64);
    /// `StartAction(action)`, then the class's `ActionChanged(u)`
    fn act(&mut self, u: u64, action: i32);
    /// `QueueSquadronGoals(goal, flag)` when `queued`, else `SetSquadronGoals(goal, flag)`
    fn squadron(&mut self, u: u64, goal: u64, flag: bool, queued: bool);
    /// `SetGatheredResource(NULL)`
    fn clear_gathered(&mut self, u: u64);
    /// `mIsSpecialAttack = v`
    fn set_special_attack(&mut self, u: u64, v: bool);
    /// `mFormationSlot = slot` (when `slot` is some), `mFormationRank = rank`
    fn set_formation_place(&mut self, u: u64, slot: Option<u32>, rank: i32);
    /// `storage->CanTake(u, resource type)`
    fn can_take(&mut self, storage: u64, u: u64, resource_type: u32) -> bool;
    /// `u->GetNearestFootprintTile(target, &x, &y)` from `(x, y)`
    fn nearest_footprint(&mut self, u: u64, target: u64, x: i32, y: i32) -> (i32, i32);
    /// `gWorld.mMap->GetTile(x, y) != NULL`
    fn has_tile(&mut self, x: i32, y: i32) -> bool;
    /// the tile at the point (`UMFloorUnsigned` of x and y): its height and terrain, None for none
    fn tile_at(&mut self, p: [f32; 3]) -> Option<(f32, i32)>;
    /// `gContinentManager.CanGetTo(u, x, y)`
    fn can_get_to(&mut self, u: u64, x: i32, y: i32) -> bool;
    /// `EECanConvert(u, target, flag)`
    fn can_convert(&mut self, u: u64, target: u64, flag: &mut bool) -> bool;
    fn can_attack_class(&mut self, u: u64, class: u64) -> bool;
    /// `class->FindAction(action) != NULL`
    fn has_action(&mut self, class: u64, action: i32) -> bool;
    /// `GetCommandType()`
    fn command_type(&mut self, u: u64) -> i32;
    /// `GetComplexUnit()->GetNumCargoUnits()`
    fn num_cargo(&mut self, u: u64) -> u32;
    /// `GetPosition(p)`
    fn position(&mut self, u: u64) -> [f32; 3];
    /// `EEGetAngleFromNorth(&from, &to)`
    fn angle_from_north(&mut self, from: [f32; 3], to: [f32; 3]) -> f32;
    /// the units on tile (x, y) (its `mUnits` chain; empty for no tile)
    fn tile_units(&mut self, x: i32, y: i32) -> Vec<u64>;
    fn passable_by(&mut self, obj: u64, by: u64) -> bool;
    /// `EEIsSeenBy(target, player, true)`
    fn seen_by(&mut self, target: u64, player: u64) -> bool;
    /// `EECanPriestConvert(u, target)`
    fn can_priest_convert(&mut self, u: u64, target: u64) -> bool;
    /// `GetPower() >= calamity->GetCost() && node->Contains(class type)`
    fn can_cast(&mut self, u: u64, calamity: u64, node: u64) -> bool;
    fn reference(&mut self, u: u64);
    fn dereference(&mut self, u: u64);
    /// `U2DRectangle<float>(left, top, right, bottom).GetClosestPoint(p)`
    fn rect_closest(&mut self, bounds: Rect, p: [f32; 2]) -> [f32; 2];

    // the goals
    fn goal_kind(&mut self, goal: u64) -> i32;
    fn clone_goal(&mut self, goal: u64) -> u64;
    fn delete_goal(&mut self, goal: u64);
    fn location_fields(&mut self, goal: u64) -> LocationFields;
    /// `GetB1C()`
    fn goal_b1c(&mut self, goal: u64) -> bool;
    fn goal_location(&mut self, goal: u64) -> [f32; 3];
    fn new_location(&mut self, l: NewLocation) -> u64;
    /// `EEUGRust::NewUnloadTransport(u, x, y, true)`
    fn new_unload(&mut self, u: u64, x: i32, y: i32) -> u64;
    fn can_unload_at_destination(&mut self, goal: u64) -> bool;
    /// `GetUnloadTransport(&x, &y)`'s tile
    fn unload_destination(&mut self, goal: u64) -> (i32, i32);
    /// `mB1D = true`
    fn set_b1d(&mut self, goal: u64);
    /// `SetGroupPathState(0)`
    fn clear_group_path_state(&mut self, goal: u64);
    fn gather_storage(&mut self, goal: u64) -> u64;
    fn gather_resource(&mut self, goal: u64) -> u64;
    fn set_gather_storage(&mut self, goal: u64, storage: u64);
    fn attack_target(&mut self, goal: u64) -> u64;
    fn is_convert_attack(&mut self, goal: u64) -> bool;
    /// `SetAttacker(NULL)`
    fn clear_attacker(&mut self, goal: u64);
    fn heal_target(&mut self, goal: u64) -> u64;
    fn suits(&mut self, goal: u64, u: u64) -> bool;
    fn set_explore_tile(&mut self, goal: u64, x: i32, y: i32);
    /// `GetPatrolTarget()`, else `GetPatrolGroup()->GetLast()`
    fn patrol_point(&mut self, goal: u64) -> u64;
    /// `EEUGRust::NewAttackUnit(first, target, true, false)`
    fn new_attack_unit(&mut self, first: u64, target: u64) -> u64;
}

/// `U2DRectangle<float>`'s edges.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Rect {
    pub top: f32,
    pub left: f32,
    pub bottom: f32,
    pub right: f32,
}

impl Rect {
    /// `Extend` (windows.h's `min`/`max`)
    fn extend(&mut self, x: f32, y: f32) {
        self.left = if self.left < x { self.left } else { x };
        self.right = if self.right > x { self.right } else { x };
        self.top = if self.top < y { self.top } else { y };
        self.bottom = if self.bottom > y { self.bottom } else { y };
    }
    fn contains(&self, x: f32, y: f32) -> bool {
        x >= self.left && x <= self.right && y >= self.top && y <= self.bottom
    }
}

fn units<H: AgHost + ?Sized>(h: &mut H, g: u64) -> u32 {
    h.group(g).count
}

fn first_unit<H: AgHost + ?Sized>(h: &mut H, g: u64) -> u64 {
    if units(h, g) != 0 {
        h.group_unit(g, 0)
    } else {
        0
    }
}

/// `ComputeValues`: the group's mean, maximum and minimum speeds, turn speeds and accelerations, each mean capped
/// at the world table's factor times the slowest unit's value, and the four inverses.
pub fn compute_values<H: AgHost + ?Sized>(h: &mut H, g: u64) {
    let cap = h.cap_factor();
    let mut v = [0.0f32; 17];
    v[16] = cap;
    v[2] = FLT_MAX;
    v[5] = FLT_MAX;
    v[8] = FLT_MAX;
    v[11] = FLT_MAX;
    let count = units(h, g);
    if count == 0 {
        h.set_values(g, &v);
        return;
    }
    for i in 0..count {
        let u = h.group_unit(g, i);
        let s = h.unit(u);
        v[0] += s.max_velocity;
        v[3] += s.acceleration;
        v[6] += s.max_angular_velocity;
        v[9] += s.angular_acceleration;
        let stats = [s.max_velocity, s.acceleration, s.max_angular_velocity, s.angular_acceleration];
        for (k, value) in stats.into_iter().enumerate() {
            if value > v[3 * k + 1] {
                v[3 * k + 1] = value;
            }
            if value < v[3 * k + 2] {
                v[3 * k + 2] = value;
            }
        }
    }
    let n = count as f32;
    v[0] /= n;
    v[3] /= n;
    v[6] /= n;
    v[9] /= n;
    for k in 0..4 {
        let capped = v[3 * k + 2] * cap;
        if capped < v[3 * k] {
            v[3 * k] = capped;
        }
    }
    for k in 0..4 {
        let mean = v[3 * k];
        v[12 + k] = if mean > 0.0 { 1.0 / mean } else { 0.0 };
    }
    h.set_values(g, &v);
}

/// The C++ index loop that removes the units failing `keep` (the index stays on a removal).
fn keep_where<H: AgHost + ?Sized>(h: &mut H, g: u64, mut keep: impl FnMut(&mut H, &EeUnitsAgUnit) -> bool) {
    let mut i = 0;
    while i < units(h, g) {
        let u = h.group_unit(g, i);
        let s = h.unit(u);
        if keep(h, &s) {
            i += 1;
        } else {
            h.remove(g, u);
        }
    }
}

/// `KeepAttackers`: commandable, living, built units with an attack, an air traffic type, a priest, a storm, a
/// balloon or a missile base.
pub fn keep_attackers<H: AgHost + ?Sized>(h: &mut H, g: u64) {
    keep_where(h, g, |_, s| {
        let t = s.unit_type;
        s.commandable != 0
            && s.hit_points > 0
            && s.is_built != 0
            && (s.attack != 0.0
                || s.air_traffic_type != 0
                || t == UNIT_TYPE_HURRICANE
                || t == UNIT_TYPE_ANTI_MATTER_STORM
                || t == UNIT_TYPE_BALLOON09_HOT_AIR_BALLOON
                || t == UNIT_TYPE_BALLOON10_OBSERVATION_BALLOON
                || s.is_priest != 0
                || t == UNIT_TYPE_CP_MISSILE_BASE)
    });
}

/// `KeepMobileUnits`: commandable, living units that can move (airports included).
pub fn keep_mobile<H: AgHost + ?Sized>(h: &mut H, g: u64) {
    keep_where(h, g, |_, s| {
        s.commandable != 0 && s.hit_points > 0 && (s.max_velocity != 0.0 || s.unit_type == UNIT_TYPE_AIRPORT)
    });
}

/// The heading from the group's first unit to the tile's centre (`UMArcTan2(-dx, dy)`, dx and dy to `x + 0.5`,
/// `y + 0.5`).
fn heading_to<H: AgHost + ?Sized>(h: &mut H, g: u64, x: f32, y: f32) -> f32 {
    let first = first_unit(h, g);
    let l = h.unit(first).location;
    let dx = x + 0.5 - l[0];
    let first = first_unit(h, g);
    let l2 = h.unit(first).location;
    um::um_arctan2(-dx, y + 0.5 - l2[1])
}

/// The split-off group's move to `point` facing from its first unit to the tile (x, y).
fn launch_move<H: AgHost + ?Sized>(h: &mut H, g: u64, ng: u64, point: [f32; 2], x: f32, y: f32) {
    h.set_formation(ng, g);
    let heading = heading_to(h, ng, x, y);
    let goal = h.new_location(NewLocation {
        point: [point[0], point[1], 0.0],
        b1c: true,
        forced: true,
        face: true,
        yaw: heading,
        ..Default::default()
    });
    let queued = h.group(g).queued != 0;
    h.launch(g, ng, goal, queued, ACTION_PREPARE_TO_MOVE, 0, AG_COMMAND_MOVE);
}

/// `SpreadAroundUnit`: keeps the attackers able to hit the target (ranged ones only if they see it); the other
/// mobile non-priests split off into a group moving to the target's tile.
pub fn spread_around_unit<H: AgHost + ?Sized>(h: &mut H, g: u64, target: u64, x: i32, y: i32) {
    let t = h.unit(target);
    let mut ng = 0;
    keep_attackers(h, g);
    let mut i = 0;
    while i < units(h, g) {
        let u = h.group_unit(g, i);
        let s = h.unit(u);
        let slow = s.range > s.line_of_sight;
        if s.hit_points <= 0 {
            h.remove(g, u);
            continue;
        }
        let able = s.unit_type == UNIT_TYPE_AIRPORT
            || s.carrier()
            || s.unit_type == UNIT_TYPE_CP_MISSILE_BASE
            || h.can_attack_class(u, t.class_obj)
            || (s.is_priest != 0 && h.can_priest_convert(u, t.complex));
        if able && (!slow || h.seen_by(target, s.player)) {
            i += 1;
        } else {
            h.remove(g, u);
            if s.is_priest == 0 && !slow {
                if ng == 0 {
                    ng = h.new_group(g);
                }
                h.add(ng, u);
            }
        }
    }
    if ng == 0 {
        return;
    }
    keep_mobile(h, ng);
    if units(h, ng) == 0 {
        return;
    }
    let (fx, fy) = (x as f32, y as f32);
    launch_move(h, g, ng, [fx, fy], fx, fy);
}

/// The citizens stay; the other living mobile units go to `split(unit)`.
fn split_off<H: AgHost + ?Sized>(
    h: &mut H,
    g: u64,
    stays: impl Fn(&EeUnitsAgUnit) -> bool,
    mut split: impl FnMut(&mut H, &EeUnitsAgUnit),
) {
    let mut i = 0;
    while i < units(h, g) {
        let u = h.group_unit(g, i);
        let s = h.unit(u);
        if s.commandable != 0 && s.hit_points > 0 && stays(&s) {
            i += 1;
        } else {
            h.remove(g, u);
            if s.hit_points > 0 && s.max_velocity > 0.0 {
                split(h, &s);
            }
        }
    }
}

fn join<H: AgHost + ?Sized>(h: &mut H, g: u64, group: &mut u64, u: u64) {
    if *group == 0 {
        *group = h.new_group(g);
    }
    h.add(*group, u);
}

/// `SpreadAroundResource`: the citizens stay; the other mobile units split off to attack the resource (an animal
/// they can attack) or into a group moving next to it.
pub fn spread_around_resource<H: AgHost + ?Sized>(h: &mut H, g: u64, target: u64) {
    let t = h.unit(target);
    let mut attackers = 0;
    let mut others = 0;
    split_off(
        h,
        g,
        |s| s.citizen(),
        |h, s| {
            if t.family == FAMILY_ANIMAL && h.can_attack_class(s.obj, t.class_obj) {
                join(h, g, &mut attackers, s.obj);
            } else {
                join(h, g, &mut others, s.obj);
            }
        },
    );
    if others != 0 {
        let t = h.unit(target);
        let (cx, cy) = (t.tile_x as f32 + 0.5, t.tile_y as f32 + 0.5);
        h.set_formation(others, g);
        let first = first_unit(h, others);
        let dx = cx - h.unit(first).location[0];
        let first = first_unit(h, others);
        let heading = um::um_arctan2(-dx, cy - h.unit(first).location[1]);
        let goal = h.new_location(NewLocation {
            point: [cx, cy, 0.0],
            b1c: true,
            forced: true,
            face: true,
            yaw: heading,
            ..Default::default()
        });
        let queued = h.group(g).queued != 0;
        h.launch(g, others, goal, queued, ACTION_PREPARE_TO_MOVE, 0, AG_COMMAND_MOVE);
    }
    if attackers == 0 {
        return;
    }
    let first = first_unit(h, attackers);
    let goal = h.new_attack_unit(first, target);
    let queued = h.group(g).queued != 0;
    h.launch(g, attackers, goal, queued, ACTION_ATTACK_ENEMY_UNIT, target, AG_COMMAND_ATTACK);
}

/// `SpreadAroundTile` (the citizens stay) and `SpreadForUnload` (the transports stay): the other mobile units
/// split off into a group moving to the tile, or beside a building on it.
pub fn spread_around_tile<H: AgHost + ?Sized>(h: &mut H, g: u64, x: i32, y: i32, for_unload: bool) {
    let mut point = [0.0f32, 0.0];
    let (mut tx, mut ty) = (x, y);
    let mut ng = 0;
    split_off(
        h,
        g,
        |s| if for_unload { s.is_transport != 0 || s.paratrooper_bomber != 0 } else { s.citizen() },
        |h, s| {
            point = [x as f32, y as f32];
            if h.has_tile(x, y) && s.tile_x != -10000 {
                for on in h.tile_units(x, y) {
                    if h.unit(on).tile_size <= 1 || h.passable_by(on, s.obj) {
                        continue;
                    }
                    (tx, ty) = h.nearest_footprint(s.obj, on, tx, ty);
                    if !h.has_tile(tx, ty) {
                        continue;
                    }
                    point = [tx as f32, ty as f32];
                    break;
                }
            }
            join(h, g, &mut ng, s.obj);
        },
    );
    if ng == 0 {
        return;
    }
    launch_move(h, g, ng, point, x as f32, y as f32);
}

/// `SplitCalamityCasters`: the units able to cast the calamity stay; the other living attackers split off into a
/// group attacking the target.
pub fn split_calamity_casters<H: AgHost + ?Sized>(h: &mut H, g: u64, calamity: u64, node: u64, target: u64) {
    let mut ng = 0;
    let mut i = 0;
    while i < units(h, g) {
        let u = h.group_unit(g, i);
        let s = h.unit(u);
        if s.commandable != 0 && s.hit_points > 0 && s.has_special_attack != 0 && h.can_cast(u, calamity, node) {
            i += 1;
        } else {
            h.reference(u);
            h.remove(g, u);
            let s = h.unit(u);
            if s.hit_points > 0 && s.attack > 0.0 {
                join(h, g, &mut ng, u);
            }
            h.dereference(u);
        }
    }
    if ng == 0 {
        return;
    }
    h.set_formation(ng, g);
    let first = first_unit(h, ng);
    let goal = h.new_attack_unit(first, target);
    let queued = h.group(g).queued != 0;
    h.launch(g, ng, goal, queued, ACTION_ATTACK_ENEMY_UNIT, 0, AG_COMMAND_MOVE);
}

/// `IsReserved`: a reserved destination at the point, or one whose unit's half inner radius reaches it.
pub fn is_reserved<H: AgHost + ?Sized>(h: &mut H, g: u64, p: [f32; 3]) -> bool {
    let mut result = false;
    for (id, d) in h.destinations(g) {
        if d == p {
            return true;
        }
        let Some(radius) = h.world_unit_sphere(id) else { continue };
        let point = Ge3DPoint { x: p[0], y: p[1], z: p[2] };
        let half = radius * 0.5;
        if point.distance_to_squared(Ge3DPoint { x: d[0], y: d[1], z: d[2] }) < half * (radius * 0.5) {
            result = true;
        }
    }
    result
}

/// `IsCellTaken`: a reserved destination at the point or in its unit region cell.
pub fn is_cell_taken<H: AgHost + ?Sized>(h: &mut H, g: u64, p: [f32; 3]) -> bool {
    let mut result = false;
    for (_, d) in h.destinations(g) {
        if d == p {
            return true;
        }
        let (x, y) = (d[0] as i32, d[1] as i32);
        let (px, py) = (p[0] as i32, p[1] as i32);
        let size = h.regions_size();
        if (x.wrapping_div(size), y.wrapping_div(size)) == (px.wrapping_div(size), py.wrapping_div(size)) {
            result = true;
        }
    }
    result
}

/// The goal queues of an aircraft held in a container: both queues get the goal (cleared first unless queued
/// behind a current goal), the second one a tick later.
fn queue_held<H: AgHost + ?Sized>(h: &mut H, u: u64, keep: bool, mut goal: impl FnMut(&mut H) -> u64) {
    if !keep {
        h.clear_goal_queue(u, 0);
        h.clear_goal_queue(u, 1);
    }
    let first = goal(h);
    h.queue_goal(u, first, 0);
    let second = goal(h);
    h.queue_goal(u, second, 1);
    h.add_time(u);
}

/// `Plan`: hands the group's goal to its units by the group's action.
pub fn plan<H: AgHost + ?Sized>(h: &mut H, g: u64) {
    let gs = h.group(g);
    let count = gs.count;
    let queued = gs.queued != 0;
    let goal = gs.goal;
    if count == 0 {
        return;
    }
    match gs.action {
        ACTION_DISBAND => h.finish(g),
        ACTION_DROPOFF_RESOURCE => {
            let storage = h.gather_storage(goal);
            for i in 0..count {
                let u = h.group_unit(g, i);
                let s = h.unit(u);
                if s.commandable == 0 {
                    continue;
                }
                if queued && s.goal_state != 0 {
                    let c = h.clone_goal(goal);
                    h.queue_goal(u, c, 0);
                    continue;
                }
                let action = if s.resource_amount > 0 && h.can_take(storage, u, s.resource_type) {
                    let current = gather_of(h, s.goal_state);
                    h.clear_gathered(u);
                    if current == 0 {
                        let c = h.clone_goal(goal);
                        h.set_goal(u, c, 0);
                    } else {
                        h.set_gather_storage(current, storage);
                    }
                    ACTION_MOVE_TO_STORAGE
                } else {
                    let st = h.unit(storage);
                    let (mut x, mut y) = (st.tile_x, st.tile_y);
                    if st.tile_size > 1 {
                        (x, y) = h.nearest_footprint(u, storage, x, y);
                    }
                    if !h.has_tile(x, y) {
                        x = st.tile_x;
                        y = st.tile_y;
                    }
                    let l = h.new_location(NewLocation::plain([x as f32, y as f32, 0.0], true));
                    h.set_goal(u, l, 0);
                    ACTION_PREPARE_TO_MOVE
                };
                h.act(u, action);
            }
        }
        ACTION_GATHER_RESOURCE => {
            let resource = h.gather_resource(goal);
            for i in 0..count {
                let u = h.group_unit(g, i);
                let s = h.unit(u);
                if s.commandable == 0 {
                    continue;
                }
                let r = h.unit(resource);
                if h.can_get_to(u, r.tile_x, r.tile_y) {
                    let current = gather_of(h, s.goal_state);
                    let current_resource = if current != 0 { h.gather_resource(current) } else { 0 };
                    if current == 0 || current_resource != resource {
                        if queued && s.goal_state != 0 {
                            let c = h.clone_goal(goal);
                            h.queue_goal(u, c, 0);
                        } else {
                            if current != 0 && current_resource != 0 {
                                let cr = h.unit(current_resource);
                                if cr.resource_type != RESOURCE_WOOD && cr.family != FAMILY_ANIMAL {
                                    h.clear_gathered(u);
                                }
                            }
                            let c = h.clone_goal(goal);
                            h.set_goal(u, c, 0);
                            h.act(u, ACTION_MOVE_TO_WAITING_AREA);
                        }
                    }
                } else {
                    let point = [r.tile_x as f32, r.tile_y as f32, 0.0];
                    let l = h.new_location(NewLocation::plain(point, true));
                    if queued && s.goal_state != 0 {
                        h.queue_goal(u, l, 0);
                    } else {
                        h.set_goal(u, l, 0);
                        h.act(u, ACTION_PREPARE_TO_MOVE);
                    }
                }
            }
        }
        ACTION_ATTACK_LOCATION => {
            for i in 0..count {
                let u = h.group_unit(g, i);
                let s = h.unit(u);
                if s.commandable == 0 || s.can_attack_ground == 0 {
                    continue;
                }
                if s.flies() && s.container != 0 {
                    queue_held(h, u, queued && s.goal_state != 0, |h| h.clone_goal(goal));
                } else if queued && s.goal_state != 0 {
                    let c = h.clone_goal(goal);
                    h.queue_goal(u, c, 0);
                } else {
                    h.set_special_attack(u, gs.special_attack != 0);
                    h.clear_goal_queue(u, 0);
                    let c = h.clone_goal(goal);
                    h.set_goal(u, c, 0);
                    h.act(u, ACTION_PREPARE_TO_MOVE);
                }
            }
        }
        ACTION_ATTACK_ENEMY_UNIT => plan_attack_unit(h, g, &gs),
        ACTION_HEAL_UNIT => {
            let target = h.heal_target(goal);
            let location = h.unit(target).location;
            let mut healing = false;
            let heal_move = |h: &mut H| h.new_location(NewLocation::plain(location, false));
            for i in 0..count {
                let u = h.group_unit(g, i);
                let s = h.unit(u);
                if s.flies() && s.container != 0 {
                    queue_held(h, u, false, heal_move);
                } else if s.commandable != 0 && h.suits(goal, u) {
                    let c = h.clone_goal(goal);
                    h.set_goal(u, c, 0);
                    h.act(u, ACTION_TURN_TO_FACE_HEALEE);
                    healing = true;
                } else if s.max_velocity > 0.0 {
                    let l = heal_move(h);
                    h.set_goal(u, l, 0);
                    h.act(u, ACTION_PREPARE_TO_MOVE);
                }
            }
            h.send_command(g, target, if healing { AG_COMMAND_HEAL } else { AG_COMMAND_MOVE });
        }
        ACTION_UNLOAD_TRANSPORT => {
            h.set_moves_together(g);
            lay_out_formation(h, g);
        }
        ACTION_PREPARE_TO_MOVE => lay_out_formation(h, g),
        ACTION_FIND_UNEXPLORED_AREA => {
            for i in 0..count {
                let u = h.group_unit(g, i);
                let s = h.unit(u);
                if s.commandable == 0 {
                    continue;
                }
                let hero = s.unit_type == UNIT_TYPE_AIRPORT;
                if s.flies() && s.container != 0 {
                    queue_held(h, u, false, |h| h.clone_goal(goal));
                    continue;
                }
                if hero {
                    let copy = h.clone_goal(goal);
                    h.set_explore_tile(copy, s.tile_x, s.tile_y);
                    h.squadron(u, copy, false, false);
                } else {
                    h.clear_goals(u, 0);
                    let c = h.clone_goal(goal);
                    h.set_goal(u, c, 0);
                }
                h.act(u, ACTION_FIND_UNEXPLORED_AREA);
                if hero {
                    h.act(u, ACTION_AIRPORT_OPEN);
                } else if s.flies() && s.landed() {
                    h.act(u, ACTION_TAKEOFF);
                }
            }
        }
        ACTION_MOVE_TO_NEXT_PATROL_POINT => {
            for i in 0..count {
                let u = h.group_unit(g, i);
                let s = h.unit(u);
                if s.commandable == 0 {
                    continue;
                }
                let patrol_move = |h: &mut H| {
                    let point = h.patrol_point(goal);
                    let location = h.unit(point).location;
                    h.new_location(NewLocation::plain(location, true))
                };
                if s.flies() && s.container != 0 {
                    let point = h.patrol_point(goal);
                    let location = h.unit(point).location;
                    queue_held(h, u, false, |h| h.new_location(NewLocation::plain(location, true)));
                    continue;
                }
                let action = if s.can_patrol == 0
                    && (s.detached == 0 || !h.has_action(s.class_obj, ACTION_GET_NEXT_PATROL_POINT))
                {
                    let l = patrol_move(h);
                    h.set_goal(u, l, 0);
                    ACTION_PREPARE_TO_MOVE
                } else {
                    let c = h.clone_goal(goal);
                    h.set_goal(u, c, 0);
                    ACTION_GET_NEXT_PATROL_POINT
                };
                h.act(u, action);
            }
        }
        _ => {}
    }
}

/// `EEGatherGoalCast(goal)`
fn gather_of<H: AgHost + ?Sized>(h: &mut H, goal: u64) -> u64 {
    if goal != 0 && h.goal_kind(goal) == GOAL_GATHER {
        goal
    } else {
        0
    }
}

/// Plan's attack on a unit (`kActionAttackEnemyUnit`).
fn plan_attack_unit<H: AgHost + ?Sized>(h: &mut H, g: u64, gs: &EeUnitsAgGroup) {
    let goal = gs.goal;
    let queued = gs.queued != 0;
    let target = h.attack_target(goal);
    let mut flag = false;
    for i in 0..gs.count {
        let u = h.group_unit(g, i);
        let s = h.unit(u);
        if s.commandable == 0 {
            continue;
        }
        let behind = queued && s.goal_state != 0;
        if s.unit_type == UNIT_TYPE_AIRPORT {
            h.squadron(u, goal, true, queued);
            if !queued {
                h.act(u, ACTION_AIRPORT_OPEN);
            }
            continue;
        }
        if s.flies() && s.container != 0 {
            queue_held(h, u, behind, |h| h.clone_goal(goal));
            continue;
        }
        // the plain attack: queued behind the current goal, or the goal now with `action`
        let attack = |h: &mut H, action: i32| {
            if behind {
                let c = h.clone_goal(goal);
                h.queue_goal(u, c, 0);
                false
            } else {
                h.clear_goal_queue(u, 0);
                let c = h.clone_goal(goal);
                h.set_goal(u, c, 0);
                h.act(u, action);
                true
            }
        };
        if s.flies() && s.landed() {
            attack(h, ACTION_TAKEOFF);
            continue;
        }
        if s.is_priest == 0 {
            if s.carrier() {
                if !queued || h.queued_goal(u, 1) == 0 {
                    h.clear_goal_queue(u, 1);
                    let c = h.clone_goal(goal);
                    h.set_goal(u, c, 1);
                    h.act(u, ACTION_AIRPORT_OPEN);
                } else {
                    let c = h.clone_goal(goal);
                    h.queue_goal(u, c, 1);
                }
            } else if attack(h, ACTION_INITIAL_ATTACK_STATE) {
                h.set_special_attack(u, gs.special_attack != 0);
            }
            continue;
        }
        let t = h.unit(target);
        let converts = t.complex != 0
            && h.can_convert(u, t.complex, &mut flag)
            && !(s.is_crusader != 0 && !h.is_convert_attack(goal));
        if !converts {
            if h.can_attack_class(u, t.class_obj) {
                attack(h, ACTION_INITIAL_ATTACK_STATE);
            }
        } else {
            attack(h, ACTION_CONVERT_ENEMY_UNIT);
        }
    }
    h.clear_attacker(goal);
}

/// `LayOutFormation`: arranges the formation on the goal, gives every unit a move or unload goal at its slot and
/// requests the paths.
pub fn lay_out_formation<H: AgHost + ?Sized>(h: &mut H, g: u64) {
    let gs = h.group(g);
    let queued = gs.queued != 0;
    let goal = gs.goal;
    let kind = gs.goal_kind;
    let location = h.goal_location(goal);
    let tile = h.tile_at(location);
    // the aircraft held in a container get the goal directly and leave the group
    if kind == GOAL_LOCATION || kind == GOAL_UNLOAD_TRANSPORT {
        let fields = if kind == GOAL_LOCATION {
            h.location_fields(goal)
        } else {
            LocationFields { b1c: h.goal_b1c(goal), forced: true, ..Default::default() }
        };
        let mut i = 0;
        while i < units(h, g) {
            let u = h.group_unit(g, i);
            let s = h.unit(u);
            if !s.flies() || s.container == 0 {
                i += 1;
                continue;
            }
            let held = NewLocation { point: location, b1c: fields.b1c, forced: fields.forced, ..Default::default() };
            if !(queued && s.goal_state != 0) {
                h.clear_goal_queue(u, 0);
                h.clear_goal_queue(u, 1);
            }
            for q in 0..2 {
                let l = h.new_location(held);
                if kind == GOAL_LOCATION {
                    h.queue_add(u, l, q);
                } else {
                    h.queue_goal(u, l, q);
                }
            }
            h.add_time(u);
            h.remove(g, u);
        }
    }
    if units(h, g) == 0 {
        return;
    }
    let yaw = match kind {
        GOAL_LOCATION => Some(h.location_fields(goal).yaw),
        GOAL_UNLOAD_TRANSPORT => {
            if units(h, g) > 0 {
                let (dx, dy) = h.unload_destination(goal);
                let first = h.group_unit(g, 0);
                let f = h.unit(first);
                let y = dy as f32 - f.tile_y as f32;
                let x = -(dx as f32 - f.tile_x as f32);
                Some(um::um_arctan2(x, y))
            } else {
                None
            }
        }
        _ => None,
    };
    let mut slots = h.arrange(g, location, yaw);
    let num_slots = slots.len();
    let (mut slot_x, mut slot_y) = (0.0f32, 0.0f32);
    for s in &slots {
        slot_x += s.position[0];
        slot_y += s.position[1];
    }
    let slot_count = num_slots as f32;
    slot_x /= slot_count;
    slot_y /= slot_count;
    let num_units = units(h, g);
    let mut bounds = Rect::default();
    let mut size = 0.0f32;
    let (mut center_x, mut center_y) = (0.0f32, 0.0f32);
    for i in 0..num_units {
        let u = h.group_unit(g, i);
        let s = h.unit(u);
        let l = s.location;
        if i != 0 {
            bounds.extend(l[0], l[1]);
        } else {
            bounds = Rect { top: l[1], left: l[0], bottom: l[1], right: l[0] };
        }
        if s.inner_sphere >= size {
            size = s.inner_sphere;
        }
        center_x += l[0];
        center_y += l[1];
    }
    let gs2 = h.group(g);
    let speed = gs2.speed;
    center_x /= gs2.count as f32;
    center_y /= gs2.count as f32;
    if gs2.moves_together == 0 || size >= FULL_SLOT_SIZE || speed <= SLOW_SPEED {
        let target = [location[0], location[1]];
        let closest = h.rect_closest(bounds, target);
        let distance =
            (closest[0] - target[0]) * (closest[0] - target[0]) + (closest[1] - target[1]) * (closest[1] - target[1]);
        let w = bounds.bottom - bounds.top;
        let hgt = bounds.right - bounds.left;
        let size = if w > hgt { w } else { hgt };
        if !bounds.contains(target[0], target[1]) && distance > size * size {
            assign_across(h, g, &mut slots, [slot_x, slot_y], [center_x, center_y]);
        } else {
            assign_nearest(h, g, &mut slots, location);
        }
    }
    give_slot_goals(h, g, &gs, &slots, tile);
}

/// The weight of a point across the move direction (`|x dy + y dx| + |x ax + y ay|`).
fn weight(p: [f32; 3], dx: f32, dy: f32, ax: f32, ay: f32) -> f32 {
    let along = (p[0] * dy + p[1] * dx).abs();
    let across = (p[0] * ax + p[1] * ay).abs();
    across + along
}

/// Far from the units: slots and units both in their order across the move direction.
fn assign_across<H: AgHost + ?Sized>(h: &mut H, g: u64, slots: &mut [Slot], slot_mean: [f32; 2], center: [f32; 2]) {
    let mut dx = slot_mean[0] - center[0];
    let mut dy = center[1] - slot_mean[1];
    let sum = dx.abs() + dy.abs();
    dx /= sum;
    dy /= sum;
    let ay = dy / 10.0;
    let ax = dx / -10.0;
    let mut slot_order: Vec<(i32, f32)> =
        slots.iter().enumerate().map(|(i, s)| (i as i32, weight(s.position, dx, dy, ax, ay))).collect();
    crate::msvc_sort::sort_by(&mut slot_order, |a, b| a.1 < b.1);
    let count = units(h, g);
    let mut unit_order = Vec::with_capacity(count as usize);
    for j in 0..count {
        let u = h.group_unit(g, j);
        let l = h.unit(u).location;
        unit_order.push((j as i32, weight(l, dx, dy, ax, ay)));
    }
    crate::msvc_sort::sort_by(&mut unit_order, |a, b| a.1 < b.1);
    for (k, &(slot, _)) in slot_order.iter().enumerate() {
        let Some(&(j, _)) = unit_order.get(k) else { break };
        slots[slot as usize].unit = h.group_unit(g, j as u32);
    }
}

/// Near the units: each unit (slower first, then larger, then by index, ascending when most units are ahead)
/// takes the nearest free slot.
fn assign_nearest<H: AgHost + ?Sized>(h: &mut H, g: u64, slots: &mut [Slot], location: [f32; 3]) {
    let n = units(h, g) as usize;
    // (index, unit, class max velocity, inner sphere)
    let mut order: Vec<(u32, u64, f32, f32)> = vec![(0, 0, 0.0, 0.0); n];
    let mut near = 0usize;
    for k in (0..n).rev() {
        let u = h.group_unit(g, k as u32);
        let s = h.unit(u);
        order[k] = (k as u32, u, s.class_max_velocity, s.inner_sphere);
        let p = h.position(u);
        if h.angle_from_north(p, location) <= QUARTER_TURN {
            near += 1;
        }
    }
    let ascending = near >= n / 2;
    crate::msvc_sort::sort_by(&mut order, |a, b| {
        if a.2 < b.2 {
            true
        } else if a.2 != b.2 {
            false
        } else if a.3 == b.3 {
            if ascending {
                a.0 < b.0
            } else {
                a.0 > b.0
            }
        } else {
            a.3 > b.3
        }
    });
    let mut free: Vec<usize> = (0..slots.len()).collect();
    for &(_, u, _, _) in &order {
        let p = h.position(u);
        let point = Ge3DPoint { x: p[0], y: p[1], z: p[2] };
        let mut best: Option<usize> = None;
        let mut best_distance = FLT_MAX;
        for f in (0..free.len()).rev() {
            let s = slots[free[f]].position;
            let squared = point.distance_to_squared(Ge3DPoint { x: s[0], y: s[1], z: s[2] });
            if squared < best_distance || best.is_none() {
                best_distance = squared;
                best = Some(f);
            }
        }
        let Some(best) = best else { break };
        slots[free[best]].unit = u;
        free.remove(best);
    }
}

/// LayOutFormation's last part: every slot's unit gets its goal by its kind (a carrier's queues by the goal's
/// tile, an airport's squadron, a navy yard's two queues, the others' first queue), then the paths.
fn give_slot_goals<H: AgHost + ?Sized>(
    h: &mut H,
    g: u64,
    gs: &EeUnitsAgGroup,
    slots: &[Slot],
    tile: Option<(f32, i32)>,
) {
    let queued = gs.queued != 0;
    let kind = gs.goal_kind;
    let slot_count = slots.len() as f32;
    let on_water = match tile {
        None => true,
        Some((height, terrain)) => height < 0.0 || terrain == TERRAIN_SPACE,
    };
    let mut queued_units = Vec::new();
    let mut replaced = Vec::new();
    let mut free = 0;
    let mut unloads = false;
    let mut goal = 0u64;
    for (i, slot) in slots.iter().enumerate() {
        let u = slot.unit;
        if u == 0 {
            continue;
        }
        let s = h.unit(u);
        let index = i as u32;
        let rank = (index as f32 / slot_count * 4.0) as i32;
        h.set_formation_place(u, if s.formation_slot != index { Some(index) } else { None }, rank);
        if s.goal_state == 0 {
            free += 1;
        }
        let at_slot = NewLocation::plain(slot.position, false);
        match kind {
            GOAL_LOCATION => {
                let f = h.location_fields(gs.goal);
                goal = h.new_location(NewLocation {
                    point: slot.position,
                    b1c: f.b1c,
                    forced: f.forced,
                    face: f.should_face,
                    yaw: slot.heading,
                    auto_waypoint: false,
                    group_state: 1,
                });
            }
            GOAL_UNLOAD_TRANSPORT => {
                if (s.is_transport != 0 || s.paratrooper_bomber != 0) && h.num_cargo(u) > 0 {
                    goal = h.new_unload(u, um::floor(slot.position[0]), um::floor(slot.position[1]));
                    if h.can_unload_at_destination(goal) {
                        unloads = true;
                    } else {
                        h.delete_goal(goal);
                        goal = h.new_location(at_slot);
                    }
                } else {
                    goal = h.new_location(at_slot);
                }
            }
            _ => {}
        }
        if goal == 0 {
            continue;
        }
        if !queued || s.goal_state == 0 {
            h.set_b1d(goal);
            if queued {
                queued_units.push(u);
            }
        } else if h.goal_kind(goal) == GOAL_LOCATION {
            h.clear_group_path_state(goal);
        }
        let behind = queued && s.goal_state != 0;
        if s.carrier() {
            if queued {
                let q = if on_water { 0 } else { 1 };
                let idle = h.queued_goal(u, q) == 0;
                h.queue_goal(u, goal, q);
                if idle {
                    h.act(u, if on_water { ACTION_PREPARE_TO_MOVE } else { ACTION_AIRPORT_OPEN });
                }
            } else if on_water {
                h.clear_goals(u, 0);
                h.clear_goal_queue(u, 0);
                h.set_goal(u, goal, 0);
                h.act(u, ACTION_PREPARE_TO_MOVE);
            } else {
                h.clear_goal_queue(u, 1);
                h.set_goal(u, goal, 1);
                h.act(u, ACTION_AIRPORT_OPEN);
            }
        } else if s.unit_type == UNIT_TYPE_AIRPORT {
            h.squadron(u, goal, false, queued);
            if !queued {
                h.act(u, ACTION_AIRPORT_OPEN);
            }
            replaced.push(u);
        } else if s.unit_type == UNIT_TYPE_NAVY_YARD {
            if queued {
                if s.goal_state != 0 {
                    h.queue_goal(u, goal, 0);
                } else {
                    h.clear_goals(u, 0);
                    h.clear_goal_queue(u, 0);
                    h.set_goal(u, goal, 0);
                }
                let has_second = h.queued_goal(u, 2) != 0;
                let c = h.clone_goal(goal);
                if has_second {
                    h.queue_goal(u, c, 2);
                } else {
                    h.set_goal(u, c, 2);
                }
            } else {
                h.clear_goals(u, 0);
                h.clear_goal_queue(u, 0);
                h.set_goal(u, goal, 0);
                h.clear_goals(u, 2);
                h.clear_goal_queue(u, 2);
                let c = h.clone_goal(goal);
                h.set_goal(u, c, 2);
                h.act(u, ACTION_AIRPORT_OPEN);
            }
            replaced.push(u);
        } else if behind {
            h.queue_goal(u, goal, 0);
        } else {
            h.clear_goals(u, 0);
            h.clear_goal_queue(u, 0);
            h.set_goal(u, goal, 0);
            if s.flies() && h.command_type(u) == ACTION_IDLE {
                h.act(u, ACTION_TAKEOFF);
                continue;
            }
            h.act(u, ACTION_PREPARE_TO_MOVE);
        }
    }
    for u in replaced {
        h.remove(g, u);
    }
    if kind == GOAL_UNLOAD_TRANSPORT {
        h.send_command(g, 0, if unloads { AG_COMMAND_UNLOAD } else { AG_COMMAND_MOVE });
    }
    if queued && free == 0 {
        return;
    }
    if !queued {
        h.request_all_paths(g);
    } else if !queued_units.is_empty() {
        h.request_paths(g, &queued_units);
    }
}

#[cfg(test)]
#[path = "ag_tests.rs"]
mod tests;
