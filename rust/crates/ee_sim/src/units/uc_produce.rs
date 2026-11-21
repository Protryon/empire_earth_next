//! `EEUnitClass::ProduceUnit` over its C++ statements (the units' lane, round 6; was EEUnitClass.cpp): the
//! population check, an aircraft or projectile loaded into its producer (or the air traffic's cargo), any other
//! unit placed by the producer (a ship at its dock, by its queued goal's point), its yaw aligned, its cloak, and
//! the producer's queued goals handed on (clones, build targets, garrisons, a location moved onto standable
//! ground, a citizen's attack turned into gathering), the creation sound and effect. As uc_body.rs: each C++
//! statement one `EE_UNITS_UC_*` case of `ee_units_uc_host`, asked in the original's order; the placement searches
//! are the unit utilities' Rust ([`UcRust`], the same FFI functions the C++ called, with their notes).

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::uc_body::*;
use std::os::raw::c_long;

// ---- the statements (from 100; `class` the producing class, `unit` the producer)
/// `unit->GetPlayer()->GetExtraPopulation(b)`
pub const EE_UNITS_UC_EXTRA_POPULATION: i32 = 100;
/// `unit->GetPlayer()->GetPopulationLimit(0)`
pub const EE_UNITS_UC_POPULATION_LIMIT: i32 = 101;
/// `unit->GetPlayer()->GetTotalPopulation(true, false)`
pub const EE_UNITS_UC_TOTAL_POPULATION: i32 = 102;
/// class `a`'s `GetUnitType()`
pub const EE_UNITS_UC_CLASS_UNIT_TYPE: i32 = 103;
/// class `a`'s `GetPopulationCost()`
pub const EE_UNITS_UC_CLASS_POPULATION_COST: i32 = 104;
/// `unit->GetUnitClass()->GetUnitType()`
pub const EE_UNITS_UC_PRODUCER_TYPE: i32 = 105;
/// class `a`'s `GetCategory()`
pub const EE_UNITS_UC_CLASS_CATEGORY: i32 = 106;
/// class `a`'s `IsAircraft()`
pub const EE_UNITS_UC_CLASS_IS_AIRCRAFT: i32 = 107;
/// class `a`'s `mAirTrafficType`
pub const EE_UNITS_UC_CLASS_AIR_TRAFFIC: i32 = 108;
/// `ee_units_atc_cargo(unit->GetPlayer()->mAirTraffic[b - 1], c)`
pub const EE_UNITS_UC_ATC_CARGO: i32 = 109;
/// `ee_units_atc_free_cargo_weight(unit->GetPlayer()->mAirTraffic[b - 1], unit)`
pub const EE_UNITS_UC_ATC_FREE_WEIGHT: i32 = 110;
/// class `a`'s `mWeight`
pub const EE_UNITS_UC_CLASS_WEIGHT: i32 = 111;
/// `unit->GetUnitClass()->mTransportCapacity`
pub const EE_UNITS_UC_TRANSPORT_CAPACITY: i32 = 112;
/// `unit->mCargoRoom`
pub const EE_UNITS_UC_CARGO_ROOM: i32 = 113;
/// class `a`'s `CreateUnitAt(the transformation at b, unit->GetPlayer()->GetPlayerID(), false, unit)->mComplexUnit`
pub const EE_UNITS_UC_CREATE_AT: i32 = 114;
/// `unit->LoadCargo(complex unit a, true, 0)`
pub const EE_UNITS_UC_LOAD_CARGO: i32 = 115;
/// complex unit `a`'s `GetUnitClass()->mIsParatrooperBomber`
pub const EE_UNITS_UC_PARATROOPER_BOMBER: i32 = 116;
/// `EECreateCarrierAircraft(complex unit a, -1)`
pub const EE_UNITS_UC_CARRIER_AIRCRAFT: i32 = 117;
/// class `a`'s `mFamily`
pub const EE_UNITS_UC_CLASS_FAMILY: i32 = 118;
/// `unit->GetPlayer()->IsHuman()`
pub const EE_UNITS_UC_IS_HUMAN: i32 = 119;
/// `unit->GetQueuedGoal(b)`
pub const EE_UNITS_UC_QUEUED_GOAL: i32 = 120;
/// `class->mUnitType`
pub const EE_UNITS_UC_THIS_UNIT_TYPE: i32 = 121;
/// goal `a`'s `GetGoalLocation` into `f[0..3]`
pub const EE_UNITS_UC_GOAL_LOCATION: i32 = 122;
/// goal `a`'s `mKind`
pub const EE_UNITS_UC_GOAL_KIND: i32 = 123;
/// `EEAttackUnitCast(goal a)->GetAttackTarget()`
pub const EE_UNITS_UC_ATTACK_TARGET: i32 = 124;
/// unit `a`'s `GetComplexUnit()`
pub const EE_UNITS_UC_COMPLEX_OF: i32 = 125;
/// complex unit `a`'s `mContainer`
pub const EE_UNITS_UC_CONTAINER_OF: i32 = 126;
/// unit `a`'s `GetPosition` into `f[0..3]`
pub const EE_UNITS_UC_POSITION: i32 = 127;
/// class `a`'s `mDomains`
pub const EE_UNITS_UC_CLASS_DOMAINS: i32 = 128;
/// `class->GetTileSize()`
pub const EE_UNITS_UC_TILE_SIZE: i32 = 129;
/// the transformation at `c` = complex unit `a`'s location turned to the yaw of the transformation at `b` (pitch
/// and roll 0), then `a`'s `SetTransformation(it, true)`
pub const EE_UNITS_UC_ALIGN_YAW: i32 = 130;
/// class `a`'s `FliesOver()`
pub const EE_UNITS_UC_CLASS_FLIES_OVER: i32 = 131;
/// `gAEManager.FindEffectAtTile(8, complex unit a's tile, its player's id)`
pub const EE_UNITS_UC_AREA_EFFECT_AT: i32 = 132;
/// area effect `a`'s `mUnit`
pub const EE_UNITS_UC_EFFECT_OWNER: i32 = 133;
/// area effect `a`'s `Covers(its owner's player id, complex unit b, its owner's location as a GE2DPoint)`
pub const EE_UNITS_UC_EFFECT_COVERS: i32 = 134;
/// complex unit `a`'s `SetCloakableChain(true)`
pub const EE_UNITS_UC_CLOAKABLE_CHAIN: i32 = 135;
/// complex unit `a`'s `ClearGoalQueue(0)`
pub const EE_UNITS_UC_CLEAR_GOAL_QUEUE: i32 = 136;
/// `unit->GetQueueGoals(b).size()`
pub const EE_UNITS_UC_QUEUE_LEN: i32 = 137;
/// `unit->GetQueueGoals(b)[c]`
pub const EE_UNITS_UC_QUEUE_GOAL: i32 = 138;
/// goal `a`'s `Suits(complex unit b)`
pub const EE_UNITS_UC_GOAL_SUITS: i32 = 139;
/// complex unit `a`'s `AddBuildTarget(EEBuildRepairGoalCast(goal b)->GetBuildTarget())`
pub const EE_UNITS_UC_ADD_BUILD_TARGET: i32 = 140;
/// complex unit `a`'s `GetUnitClass()->mCanGarrison`
pub const EE_UNITS_UC_CAN_GARRISON: i32 = 141;
/// goal `a`'s `Clone()`
pub const EE_UNITS_UC_GOAL_CLONE: i32 = 142;
/// goal `a`'s `IsLoadGoal()`
pub const EE_UNITS_UC_GOAL_IS_LOAD: i32 = 143;
/// goal `a`'s `AddCreatedUnit(complex unit b)`
pub const EE_UNITS_UC_GOAL_ADD_CREATED: i32 = 144;
/// `EELoadGarrisonGoalCast(goal a)->GetLoadContainer()`
pub const EE_UNITS_UC_LOAD_CONTAINER: i32 = 145;
/// complex unit `a`'s `mActiveGoal`
pub const EE_UNITS_UC_ACTIVE_GOAL: i32 = 146;
/// `EELoadGarrisonGoalCast(goal a)->AddLoadUnit(complex unit b)`
pub const EE_UNITS_UC_ADD_LOAD_UNIT: i32 = 147;
/// `EEUGRust::NewLocation(&f[0..3], false, true, false, 0.0f, false, 0)`
pub const EE_UNITS_UC_NEW_LOCATION: i32 = 148;
/// `static_cast<EEUGRust*>(goal a)->SetLocation(f[0..3])`
pub const EE_UNITS_UC_GOAL_SET_LOCATION: i32 = 149;
/// the tile line's clear (ProduceUnit's `theLineTiles`, the host's)
pub const EE_UNITS_UC_LINE_RESET: i32 = 150;
/// `EEGetTileLine(complex unit a's tile, b, c, &the line, 1, 1)`
pub const EE_UNITS_UC_TILE_LINE: i32 = 151;
/// the line's `size()`
pub const EE_UNITS_UC_LINE_LEN: i32 = 152;
/// the line's `[b].mX`, `[b].mY`
pub const EE_UNITS_UC_LINE_X: i32 = 153;
pub const EE_UNITS_UC_LINE_Y: i32 = 154;
/// complex unit `a`'s `GetUnitClass()->mOuterCollisionSphere` (its bits)
pub const EE_UNITS_UC_OUTER_SPHERE: i32 = 155;
/// complex unit `a`'s `GetUnitClass()->GetCategory()`
pub const EE_UNITS_UC_UNIT_CATEGORY: i32 = 156;
/// complex unit `a`'s `CanGather(unit b, b's tile, false, true)`
pub const EE_UNITS_UC_CAN_GATHER: i32 = 157;
/// `EEUGRust::NewGather(unit a, 0, true)`
pub const EE_UNITS_UC_NEW_GATHER: i32 = 158;
/// `delete goal a`
pub const EE_UNITS_UC_DELETE_GOAL: i32 = 159;
/// complex unit `a`'s `SetBehavior(EEUnitBehavior::sTable[5])`
pub const EE_UNITS_UC_GATHER_BEHAVIOR: i32 = 160;
/// complex unit `a`'s `QueueGoal(goal b, 0)`
pub const EE_UNITS_UC_QUEUE_GOAL_ON: i32 = 161;
/// complex unit `a`'s `mGoalState`
pub const EE_UNITS_UC_GOAL_STATE: i32 = 162;
/// complex unit `a`'s `StartAction(a->mGoalState->GetStartAction())`
pub const EE_UNITS_UC_START_GOAL_ACTION: i32 = 163;
/// `unit->GetPlayer() == gWorld.mPlayers[gWorld.mCurrentPlayer]`
pub const EE_UNITS_UC_PRODUCER_IS_CURRENT: i32 = 164;
/// complex unit `a`'s `GetStats()->mCreationSound`
pub const EE_UNITS_UC_CREATION_SOUND: i32 = 165;
/// `gSoundManager->Play(sound a's sample, its volume, 0, 1, false, NULL)`
pub const EE_UNITS_UC_PLAY_SAMPLE: i32 = 166;
/// `unit->GetTileX()`, `unit->GetTileY()`
pub const EE_UNITS_UC_PRODUCER_TILE_X: i32 = 167;
pub const EE_UNITS_UC_PRODUCER_TILE_Y: i32 = 168;

const UNIT_TYPE_CP_MISSILE_BASE: i64 = 0x2ed;
const UNIT_TYPE_AIRPORT: i64 = 0x42;
const UNIT_TYPE_NAVY_YARD: i64 = 0x41;
const CATEGORY_MISSILE: i64 = 0xc;
const CATEGORY_PROJECTILE: i64 = 9;
const CATEGORY_LAND_CITIZEN: i64 = 6;
const CATEGORY_NAVAL_CITIZEN: i64 = 0x16;
const FAMILY_HELICOPTER: i64 = 0x10;
const MOVEMENT_SEA: i64 = 4;
const MOVEMENT_SPACE: i64 = 8;
const GOAL_LOCATION: i64 = 1;
const GOAL_ATTACK_UNIT: i64 = 2;
const GOAL_BUILD_REPAIR: i64 = 4;
const GOAL_LOAD_GARRISON: i64 = 12;
/// `k447AFC` (the location's rings per outer sphere)
const RINGS_PER_SPHERE: f32 = 10.0;

/// `ee_units_uu_find_placement`'s arguments.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct FindPlacement {
    pub point: [f32; 3],
    pub unit: u64,
    pub class: u64,
    pub x: u32,
    pub y: u32,
    pub size: i32,
    pub rings: i32,
    pub flags: [bool; 4],
    pub target: u64,
}

/// The unit utilities' Rust the body calls (their FFI functions, as the C++ called them).
pub trait UcRust: UcHost {
    fn find_placement(&mut self, p: &FindPlacement, m: &mut [[f32; 4]; 4]) -> bool;
    fn dock_ship_placement(
        &mut self,
        point: [f32; 3],
        class: u64,
        unit: u64,
        face: bool,
        facing_only: bool,
        m: &mut [[f32; 4]; 4],
    ) -> bool;
    fn can_stand_on(&mut self, x: i32, y: i32, unit: u64) -> bool;
}

fn identity() -> [[f32; 4]; 4] {
    let mut m = [[0.0; 4]; 4];
    for (i, row) in m.iter_mut().enumerate() {
        row[i] = 1.0;
    }
    m
}

fn long(value: i64) -> c_long {
    value as c_long
}

/// `UMFloor` (cu_body.rs's)
fn floor(value: f32) -> i32 {
    let rounded = ee_base::math::um::lrintf(value);
    if rounded as f32 > value {
        rounded.wrapping_sub(1)
    } else {
        rounded
    }
}

struct P<'h, H: UcRust + ?Sized> {
    h: &'h mut H,
    class: u64,
    unit: u64,
}

impl<H: UcRust + ?Sized> P<'_, H> {
    fn s(&mut self, op: i32) -> i64 {
        self.h.uc(op, self.class, self.unit, 0, 0, 0, &mut [0.0; 4])
    }
    fn a(&mut self, op: i32, a: u64) -> i64 {
        self.h.uc(op, self.class, self.unit, a, 0, 0, &mut [0.0; 4])
    }
    fn b(&mut self, op: i32, b: i64) -> i64 {
        self.h.uc(op, self.class, self.unit, 0, b, 0, &mut [0.0; 4])
    }
    fn ab(&mut self, op: i32, a: u64, b: i64) -> i64 {
        self.h.uc(op, self.class, self.unit, a, b, 0, &mut [0.0; 4])
    }
    fn abc(&mut self, op: i32, a: u64, b: i64, c: i64) -> i64 {
        self.h.uc(op, self.class, self.unit, a, b, c, &mut [0.0; 4])
    }
    fn point(&mut self, op: i32, a: u64, point: &mut [f32; 3]) -> i64 {
        let mut f = [point[0], point[1], point[2], 0.0];
        let r = self.h.uc(op, self.class, self.unit, a, 0, 0, &mut f);
        point.copy_from_slice(&f[..3]);
        r
    }
}

/// `ProduceUnit(unit, made, arg)` of class `class`: whether `made` was produced (see the module).
pub fn produce_unit<H: UcRust + ?Sized>(h: &mut H, class: u64, unit: u64, made: u64, arg: bool) -> bool {
    let mut p = P { h, class, unit };
    let mut placement = identity();
    let mut adjusted = identity();
    let mut goal_point = [0.0f32; 3];
    let mut goal_position = [0.0f32; 3];
    p.s(EE_UNITS_UC_LINE_RESET);
    let mut created: u64 = 0;
    let mut queue: i64 = 1;
    let mut loaded = false;
    let mut success = true;
    let mut have_goal_point = false;
    let made_type = p.a(EE_UNITS_UC_CLASS_UNIT_TYPE, made);
    let limit =
        long(p.b(EE_UNITS_UC_EXTRA_POPULATION, made_type)).wrapping_add(long(p.s(EE_UNITS_UC_POPULATION_LIMIT)));
    let needed =
        long(p.s(EE_UNITS_UC_TOTAL_POPULATION)).wrapping_add(long(p.a(EE_UNITS_UC_CLASS_POPULATION_COST, made)));
    if needed > limit {
        return false;
    }
    let category = p.a(EE_UNITS_UC_CLASS_CATEGORY, made);
    let aircraft = (p.s(EE_UNITS_UC_PRODUCER_TYPE) == UNIT_TYPE_CP_MISSILE_BASE && category == CATEGORY_MISSILE)
        || p.a(EE_UNITS_UC_CLASS_IS_AIRCRAFT, made) != 0
        || p.a(EE_UNITS_UC_CLASS_CATEGORY, made) == CATEGORY_PROJECTILE;
    if aircraft {
        let traffic = p.a(EE_UNITS_UC_CLASS_AIR_TRAFFIC, made);
        let can_load = if traffic != 0 {
            let weight = long(p.a(EE_UNITS_UC_CLASS_WEIGHT, made));
            let used = long(p.abc(EE_UNITS_UC_ATC_CARGO, 0, traffic, 1));
            let mut can = used.wrapping_add(weight) <= long(p.abc(EE_UNITS_UC_ATC_CARGO, 0, traffic, 0));
            if can {
                can = long(p.b(EE_UNITS_UC_ATC_FREE_WEIGHT, traffic)) >= weight;
            }
            can
        } else {
            let capacity = long(p.s(EE_UNITS_UC_TRANSPORT_CAPACITY));
            let weight = long(p.a(EE_UNITS_UC_CLASS_WEIGHT, made));
            weight.wrapping_sub(long(p.s(EE_UNITS_UC_CARGO_ROOM))).wrapping_add(capacity) <= capacity
        };
        if can_load {
            placement[0][3] = -10000.0;
            placement[1][3] = -10000.0;
            created = p.ab(EE_UNITS_UC_CREATE_AT, made, placement.as_mut_ptr() as i64) as u64;
            p.a(EE_UNITS_UC_LOAD_CARGO, created);
            loaded = true;
            if p.a(EE_UNITS_UC_PARATROOPER_BOMBER, created) != 0 {
                p.a(EE_UNITS_UC_CARRIER_AIRCRAFT, created);
            }
        } else {
            success = false;
        }
    } else {
        queue = if p.a(EE_UNITS_UC_CLASS_FAMILY, made) == FAMILY_HELICOPTER
            && p.s(EE_UNITS_UC_PRODUCER_TYPE) != UNIT_TYPE_NAVY_YARD
        {
            2
        } else {
            1
        };
        let rings = if p.s(EE_UNITS_UC_IS_HUMAN) == 0 { 5 } else { 1 };
        let queued = p.b(EE_UNITS_UC_QUEUED_GOAL, queue) as u64;
        if queued != 0 {
            if p.s(EE_UNITS_UC_THIS_UNIT_TYPE) != UNIT_TYPE_AIRPORT {
                let goal = p.b(EE_UNITS_UC_QUEUED_GOAL, queue) as u64;
                p.point(EE_UNITS_UC_GOAL_LOCATION, goal, &mut goal_point);
                have_goal_point = true;
            } else {
                let goal = p.b(EE_UNITS_UC_QUEUED_GOAL, queue) as u64;
                if p.a(EE_UNITS_UC_GOAL_KIND, goal) != GOAL_ATTACK_UNIT {
                    // the original does not set the flag on this branch
                    p.point(EE_UNITS_UC_GOAL_LOCATION, goal, &mut goal_point);
                } else {
                    let target = p.a(EE_UNITS_UC_ATTACK_TARGET, goal) as u64;
                    if target == 0 {
                        have_goal_point = false;
                    } else {
                        let complex = p.a(EE_UNITS_UC_COMPLEX_OF, target) as u64;
                        let container = if complex != 0 {
                            let complex = p.a(EE_UNITS_UC_COMPLEX_OF, target) as u64;
                            p.a(EE_UNITS_UC_CONTAINER_OF, complex) as u64
                        } else {
                            0
                        };
                        if container != 0 {
                            let complex = p.a(EE_UNITS_UC_COMPLEX_OF, target) as u64;
                            let container = p.a(EE_UNITS_UC_CONTAINER_OF, complex) as u64;
                            p.point(EE_UNITS_UC_POSITION, container, &mut goal_point);
                        } else {
                            p.point(EE_UNITS_UC_POSITION, target, &mut goal_point);
                        }
                        have_goal_point = true;
                    }
                }
            }
        }
        let domains = p.a(EE_UNITS_UC_CLASS_DOMAINS, made);
        let placed = if domains & MOVEMENT_SEA != 0 {
            p.h.dock_ship_placement(goal_point, made, unit, have_goal_point, true, &mut placement)
        } else if p.a(EE_UNITS_UC_CLASS_DOMAINS, made) & MOVEMENT_SPACE != 0 {
            p.h.dock_ship_placement(goal_point, made, unit, have_goal_point, false, &mut placement)
        } else {
            let find = FindPlacement {
                point: goal_point,
                unit: 0,
                class: made,
                x: p.s(EE_UNITS_UC_PRODUCER_TILE_X) as u32,
                y: p.s(EE_UNITS_UC_PRODUCER_TILE_Y) as u32,
                size: p.s(EE_UNITS_UC_TILE_SIZE) as i32,
                rings,
                flags: [p.s(EE_UNITS_UC_IS_HUMAN) == 0, false, have_goal_point, false],
                target: unit,
            };
            p.h.find_placement(&find, &mut placement)
        };
        if !placed {
            success = false;
        } else {
            created = p.ab(EE_UNITS_UC_CREATE_AT, made, placement.as_mut_ptr() as i64) as u64;
            if p.a(EE_UNITS_UC_CLASS_DOMAINS, made) & MOVEMENT_SEA != 0 {
                p.abc(EE_UNITS_UC_ALIGN_YAW, created, placement.as_ptr() as i64, adjusted.as_mut_ptr() as i64);
            }
            if p.a(EE_UNITS_UC_CLASS_FLIES_OVER, made) != 0 {
                p.abc(EE_UNITS_UC_ALIGN_YAW, created, placement.as_ptr() as i64, adjusted.as_mut_ptr() as i64);
            }
            let effect = p.a(EE_UNITS_UC_AREA_EFFECT_AT, created) as u64;
            if effect != 0
                && p.a(EE_UNITS_UC_EFFECT_OWNER, effect) != 0
                && p.ab(EE_UNITS_UC_EFFECT_COVERS, effect, created as i64) != 0
            {
                p.a(EE_UNITS_UC_CLOAKABLE_CHAIN, created);
            }
        }
    }
    if created == 0 {
        return false;
    }
    if !loaded && p.b(EE_UNITS_UC_QUEUED_GOAL, queue) != 0 {
        p.a(EE_UNITS_UC_CLEAR_GOAL_QUEUE, created);
        let mut i: i64 = 0;
        while i < p.b(EE_UNITS_UC_QUEUE_LEN, queue) {
            let queued = p.abc(EE_UNITS_UC_QUEUE_GOAL, 0, queue, i) as u64;
            i += 1;
            let mut goal: u64;
            if p.ab(EE_UNITS_UC_GOAL_SUITS, queued, created as i64) != 0 {
                let kind = p.a(EE_UNITS_UC_GOAL_KIND, queued);
                if kind == GOAL_BUILD_REPAIR {
                    p.ab(EE_UNITS_UC_ADD_BUILD_TARGET, created, queued as i64);
                    continue;
                } else if kind == GOAL_LOAD_GARRISON {
                    if p.a(EE_UNITS_UC_CAN_GARRISON, created) != 0 {
                        goal = p.a(EE_UNITS_UC_GOAL_CLONE, queued) as u64;
                        if p.a(EE_UNITS_UC_GOAL_IS_LOAD, goal) != 0 {
                            p.ab(EE_UNITS_UC_GOAL_ADD_CREATED, goal, created as i64);
                        }
                        let container = p.a(EE_UNITS_UC_LOAD_CONTAINER, queued) as u64;
                        let active = p.a(EE_UNITS_UC_ACTIVE_GOAL, container) as u64;
                        if active != 0 {
                            p.ab(EE_UNITS_UC_ADD_LOAD_UNIT, active, created as i64);
                        }
                    } else {
                        let container = p.a(EE_UNITS_UC_LOAD_CONTAINER, queued) as u64;
                        if container == 0 {
                            continue;
                        }
                        p.point(EE_UNITS_UC_POSITION, container, &mut goal_position);
                        goal = p.point(EE_UNITS_UC_NEW_LOCATION, 0, &mut goal_position) as u64;
                    }
                } else {
                    goal = p.a(EE_UNITS_UC_GOAL_CLONE, queued) as u64;
                    if p.a(EE_UNITS_UC_GOAL_IS_LOAD, goal) != 0 {
                        p.ab(EE_UNITS_UC_GOAL_ADD_CREATED, goal, created as i64);
                    }
                }
            } else {
                p.point(EE_UNITS_UC_GOAL_LOCATION, queued, &mut goal_position);
                goal = p.point(EE_UNITS_UC_NEW_LOCATION, 0, &mut goal_position) as u64;
            }
            if goal == 0 {
                continue;
            }
            if p.a(EE_UNITS_UC_GOAL_KIND, goal) == GOAL_LOCATION {
                p.point(EE_UNITS_UC_GOAL_LOCATION, goal, &mut goal_position);
                p.abc(
                    EE_UNITS_UC_TILE_LINE,
                    created,
                    i64::from(floor(goal_position[0])),
                    i64::from(floor(goal_position[1])),
                );
                let mut index = p.s(EE_UNITS_UC_LINE_LEN) - 1;
                while index >= 0 {
                    let (x, y) = (p.b(EE_UNITS_UC_LINE_X, index) as i32, p.b(EE_UNITS_UC_LINE_Y, index) as i32);
                    if p.h.can_stand_on(x, y, created) {
                        break;
                    }
                    index -= 1;
                }
                if index >= 0 {
                    let sphere = f32::from_bits(p.a(EE_UNITS_UC_OUTER_SPHERE, created) as u32);
                    let rings = ee_base::math::um::lrintf(sphere * RINGS_PER_SPHERE);
                    let mut origin = [0.0f32; 4];
                    p.h.uc(EE_UNITS_UC_LOCATION, class, unit, unit, 0, 0, &mut origin);
                    let find = FindPlacement {
                        point: [origin[0], origin[1], origin[2]],
                        unit: created,
                        class: 0,
                        x: p.b(EE_UNITS_UC_LINE_X, index) as u32,
                        y: p.b(EE_UNITS_UC_LINE_Y, index) as u32,
                        size: 1,
                        rings,
                        flags: [true, true, true, false],
                        target: 0,
                    };
                    if p.h.find_placement(&find, &mut adjusted) {
                        let mut at = [adjusted[0][3], adjusted[1][3], adjusted[2][3]];
                        p.point(EE_UNITS_UC_GOAL_SET_LOCATION, goal, &mut at);
                    }
                }
            }
            let created_category = p.a(EE_UNITS_UC_UNIT_CATEGORY, created);
            if p.a(EE_UNITS_UC_GOAL_KIND, goal) == GOAL_ATTACK_UNIT
                && (created_category == CATEGORY_LAND_CITIZEN
                    || p.a(EE_UNITS_UC_UNIT_CATEGORY, created) == CATEGORY_NAVAL_CITIZEN)
            {
                let target = p.a(EE_UNITS_UC_ATTACK_TARGET, goal) as u64;
                if p.ab(EE_UNITS_UC_CAN_GATHER, created, target as i64) != 0 {
                    let gather = p.a(EE_UNITS_UC_NEW_GATHER, target) as u64;
                    p.a(EE_UNITS_UC_DELETE_GOAL, goal);
                    goal = gather;
                    p.a(EE_UNITS_UC_GATHER_BEHAVIOR, created);
                }
            }
            p.ab(EE_UNITS_UC_QUEUE_GOAL_ON, created, goal as i64);
        }
        if p.a(EE_UNITS_UC_GOAL_STATE, created) != 0 {
            p.a(EE_UNITS_UC_START_GOAL_ACTION, created);
            p.a(EE_UNITS_UC_ACTION_CHANGED, created);
        }
    }
    if p.s(EE_UNITS_UC_PRODUCER_IS_CURRENT) != 0 {
        let sound = p.a(EE_UNITS_UC_CREATION_SOUND, created) as u64;
        if sound != 0 {
            p.a(EE_UNITS_UC_PLAY_SAMPLE, sound);
        }
    }
    if arg {
        p.s(EE_UNITS_UC_PLAY_NO_EFFECT);
    }
    success
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::uc_body::game::Game;
    use std::os::raw::c_void;

    impl UcRust for Game {
        fn find_placement(&mut self, p: &FindPlacement, m: &mut [[f32; 4]; 4]) -> bool {
            // SAFETY: the buffers are live for the call; the units and class are the body's, live
            unsafe {
                crate::units::uu_ffi::ee_units_uu_find_placement(
                    &p.point,
                    p.unit,
                    p.class,
                    p.x,
                    p.y,
                    p.size,
                    p.rings,
                    u8::from(p.flags[0]),
                    u8::from(p.flags[1]),
                    u8::from(p.flags[2]),
                    u8::from(p.flags[3]),
                    m,
                    p.target,
                ) != 0
            }
        }
        fn dock_ship_placement(
            &mut self,
            point: [f32; 3],
            class: u64,
            unit: u64,
            face: bool,
            facing_only: bool,
            m: &mut [[f32; 4]; 4],
        ) -> bool {
            // SAFETY: as find_placement
            unsafe {
                crate::units::uu_ffi::ee_units_uu_dock_ship_placement(
                    &point,
                    class,
                    unit,
                    u8::from(face),
                    u8::from(facing_only),
                    m,
                ) != 0
            }
        }
        fn can_stand_on(&mut self, x: i32, y: i32, unit: u64) -> bool {
            crate::units::uu_ffi::ee_units_uu_can_stand_on(x, y, unit) != 0
        }
    }

    /// `EEUnitClass::ProduceUnit(unit, made, arg)` of `class`.
    ///
    /// # Safety
    /// `class` and `made` are live `EEUnitClass`es, `unit` a live `EEComplexUnit`, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_uc_produce(
        class: *mut c_void,
        unit: *mut c_void,
        made: *mut c_void,
        arg: bool,
    ) -> bool {
        produce_unit(&mut Game, class as usize as u64, unit as usize as u64, made as usize as u64, arg)
    }
}

#[cfg(test)]
#[path = "uc_produce_tests.rs"]
mod tests;
