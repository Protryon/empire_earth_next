//! More of the player host's compound operations as one-statement C++ operations (the world lane, round 2; as
//! player_split.rs): the tech node's and unit class's records, the improvements' numbers, the exploration pulse, the
//! wonder factors, the upgrade's values, and the towns' and herds' bookkeeping (a new area, a unit out of one, an
//! area dropped, two towns merged, the patrols moved, the area tiles). The bodies (player.rs, player_areas.rs,
//! player_units.rs and their tests) keep asking the compound operations; in the game [`split`] makes each the C++
//! statements it was, in the original's order. The patrol groups' own calls (`ee_units_pg_*`, Rust) and the civ
//! power's amount are answered by the game's host in Rust (player_views.rs).

use crate::units::player::{
    EeUnitsPlayerIo, EE_UNITS_PLAYER_CLASS, EE_UNITS_PLAYER_IMPROVEMENT, EE_UNITS_PLAYER_IMPROVEMENT_COST,
    EE_UNITS_PLAYER_LEVEL, EE_UNITS_PLAYER_PAY_IMPROVEMENT,
};
use crate::units::player_areas::{
    EE_UNITS_PLAYER_AREA_ADD, EE_UNITS_PLAYER_AREA_AT, EE_UNITS_PLAYER_AREA_COUNT, EE_UNITS_PLAYER_AREA_DROP,
    EE_UNITS_PLAYER_AREA_NEW, EE_UNITS_PLAYER_AREA_REMOVE_UNIT, EE_UNITS_PLAYER_AREA_TILE, EE_UNITS_PLAYER_ASSIGN_TILE,
    EE_UNITS_PLAYER_TOWNS, EE_UNITS_PLAYER_TOWN_MERGE, EE_UNITS_PLAYER_TOWN_PATROL, EE_UNITS_PLAYER_TOWN_REPATROL,
};
use crate::units::player_split::{
    EE_UNITS_PLAYER_X_FIRST_UNIT, EE_UNITS_PLAYER_X_NEXT_UNIT, EE_UNITS_PLAYER_X_UPGRADE_AT,
};
use crate::units::player_units::{
    EE_UNITS_PLAYER_PULSE_EXPLORATION, EE_UNITS_PLAYER_UPGRADE_VALUES, EE_UNITS_PLAYER_WONDER_FACTORS,
};

// ---- the one-statement operations (EEPlayerHost's cases)
/// `gUnitClassTable.size()`, `gUnitClassTable[i[0]]`
pub const EE_UNITS_PLAYER_X_CLASS_COUNT: i32 = 400;
pub const EE_UNITS_PLAYER_X_CLASS_AT: i32 = 401;
/// class `i[0]`'s `mTechnology`, `mWeight`, `GetPopulationCost()`, `mUnitType`, `mAIKind`, `mImprovements`
pub const EE_UNITS_PLAYER_X_CLASS_TECH: i32 = 402;
pub const EE_UNITS_PLAYER_X_CLASS_WEIGHT: i32 = 403;
pub const EE_UNITS_PLAYER_X_CLASS_POPULATION: i32 = 404;
pub const EE_UNITS_PLAYER_X_CLASS_TYPE: i32 = 405;
pub const EE_UNITS_PLAYER_X_CLASS_AI_KIND: i32 = 406;
pub const EE_UNITS_PLAYER_X_CLASS_IMPROVEMENTS: i32 = 407;
/// improvement table `i[0]`'s entry `i[1]`'s `mStepFactor`, `mCostFactor` (into `f[0]`), its `GetMaxLevel()`
pub const EE_UNITS_PLAYER_X_TABLE_STEP: i32 = 408;
pub const EE_UNITS_PLAYER_X_TABLE_COST_FACTOR: i32 = 409;
pub const EE_UNITS_PLAYER_X_TABLE_MAX_LEVEL: i32 = 410;
/// upgrade `i[0]`'s `mImprovement`: `CanUpgrade(mUnitClass, i[1]) == true`, `GetCost(i[1])`, `mTotalCost`,
/// `GetLevel(i[1])`
pub const EE_UNITS_PLAYER_X_UP_CAN_UPGRADE: i32 = 411;
pub const EE_UNITS_PLAYER_X_UP_COST: i32 = 412;
pub const EE_UNITS_PLAYER_X_UP_TOTAL: i32 = 413;
pub const EE_UNITS_PLAYER_X_UP_LEVEL: i32 = 414;
/// the world table's `kWorldUnitImprovementPointsMax`
pub const EE_UNITS_PLAYER_X_POINTS_MAX: i32 = 415;
/// the exploration power's `mAmount` into `f[0]` (answered in Rust: the civ powers' table)
pub const EE_UNITS_PLAYER_X_EXPLORATION_AMOUNT: i32 = 416;
/// `PulseSight(true, f[0], unit i[0]'s mUnitID)`
pub const EE_UNITS_PLAYER_X_PULSE_SIGHT: i32 = 417;
/// `gWorld.GetGameVariant()->GetFactor(i[0])`, the world's `kWorldWonderHitPointBonus` (into `f[0]`)
pub const EE_UNITS_PLAYER_X_VARIANT_FACTOR: i32 = 418;
pub const EE_UNITS_PLAYER_X_WONDER_HIT_POINT_BONUS: i32 = 419;
/// class `i[0]`'s `mFlightTime`; upgrade `i[0]`'s `mFlightTime`, `mPower`, `mMaxHitPoints`
pub const EE_UNITS_PLAYER_X_CLASS_FLIGHT_TIME: i32 = 420;
pub const EE_UNITS_PLAYER_X_UP_FLIGHT_TIME: i32 = 421;
pub const EE_UNITS_PLAYER_X_UP_POWER: i32 = 422;
pub const EE_UNITS_PLAYER_X_UP_MAX_HIT_POINTS: i32 = 423;
/// a new town (`new EETown(this, ++gWorld.mTownIDCounter)`), a new herd of unit `i[0]`'s animal (`new EEHerd(this,
/// mAnimalID, ++gWorld.mHerdIDCounter)`)
pub const EE_UNITS_PLAYER_X_TOWN_NEW: i32 = 424;
pub const EE_UNITS_PLAYER_X_HERD_NEW: i32 = 425;
/// area `i[0]`'s `SetSerial(gContinentManager.NewSerial())`, `gContinentManager.AddArea(it)`, `RemoveArea(it)`,
/// `delete`, pushed into `mTowns` (the town's four; the herd's four after them)
pub const EE_UNITS_PLAYER_X_TOWN_SERIAL: i32 = 426;
pub const EE_UNITS_PLAYER_X_TOWN_ADD_AREA: i32 = 427;
pub const EE_UNITS_PLAYER_X_TOWN_REMOVE_AREA: i32 = 428;
pub const EE_UNITS_PLAYER_X_TOWN_DELETE: i32 = 429;
pub const EE_UNITS_PLAYER_X_TOWNS_PUSH: i32 = 430;
pub const EE_UNITS_PLAYER_X_HERD_SERIAL: i32 = 431;
pub const EE_UNITS_PLAYER_X_HERD_ADD_AREA: i32 = 432;
pub const EE_UNITS_PLAYER_X_HERD_REMOVE_AREA: i32 = 433;
pub const EE_UNITS_PLAYER_X_HERD_DELETE: i32 = 434;
pub const EE_UNITS_PLAYER_X_HERDS_PUSH: i32 = 435;
/// `mTowns.erase(begin + i[0])`, `mHerds.erase(begin + i[0])`
pub const EE_UNITS_PLAYER_X_TOWNS_ERASE: i32 = 436;
pub const EE_UNITS_PLAYER_X_HERDS_ERASE: i32 = 437;
/// town `i[0]`'s `RemoveUnit(unit i[1])`, herd `i[0]`'s
pub const EE_UNITS_PLAYER_X_TOWN_REMOVE_UNIT: i32 = 438;
pub const EE_UNITS_PLAYER_X_HERD_REMOVE_UNIT: i32 = 439;
/// town `i[0]`'s `Merge(town i[1])`, its `mPatrolGroup`, its `RefreshPatrolGroup()`
pub const EE_UNITS_PLAYER_X_TOWN_MERGE: i32 = 440;
pub const EE_UNITS_PLAYER_X_TOWN_PATROL_OF: i32 = 441;
pub const EE_UNITS_PLAYER_X_TOWN_REFRESH_PATROL: i32 = 442;
/// patrol point `i[0]` (a complex unit): `SetPatrolGroup(NULL)`, `Kill(true, false, false)`, its player's
/// `KillUnit(it, false)`
pub const EE_UNITS_PLAYER_X_POINT_UNPATROL: i32 = 443;
pub const EE_UNITS_PLAYER_X_POINT_KILL: i32 = 444;
pub const EE_UNITS_PLAYER_X_POINT_KILL_UNIT: i32 = 445;
/// unit `i[0]`'s `GetComplexUnit()`; complex unit `i[0]`'s patrol goal (`EEPatrolGoalCast(mGoalState)`, 0 none);
/// goal `i[0]`'s `GetPatrolGroup()`, `SetPatrolGroup(group i[1])`
pub const EE_UNITS_PLAYER_X_COMPLEX: i32 = 446;
pub const EE_UNITS_PLAYER_X_PATROL_GOAL: i32 = 447;
pub const EE_UNITS_PLAYER_X_GOAL_PATROL_GROUP: i32 = 448;
pub const EE_UNITS_PLAYER_X_GOAL_SET_PATROL: i32 = 449;
/// complex unit `i[0]`'s `FindAreaTile(true, 1)`, packed (x high, y low 32 bits)
pub const EE_UNITS_PLAYER_X_FIND_AREA_TILE: i32 = 450;
/// unit `i[0]`'s `mTileX`, `mTileY`; complex unit `i[0]`'s `UMFloor` of its last map position's x and y
pub const EE_UNITS_PLAYER_X_TILE_X: i32 = 451;
pub const EE_UNITS_PLAYER_X_TILE_Y: i32 = 452;
pub const EE_UNITS_PLAYER_X_LAST_FLOOR_X: i32 = 453;
pub const EE_UNITS_PLAYER_X_LAST_FLOOR_Y: i32 = 454;
/// the patrol groups' (answered in Rust, patrol_group.rs): `ee_units_pg_last(i[0])`, `ee_units_pg_remove(i[0],
/// i[1])`, `ee_units_pg_reference(i[0])`, `ee_units_pg_dereference(i[0])`
pub const EE_UNITS_PLAYER_X_PG_LAST: i32 = 455;
pub const EE_UNITS_PLAYER_X_PG_REMOVE: i32 = 456;
pub const EE_UNITS_PLAYER_X_PG_REFERENCE: i32 = 457;
pub const EE_UNITS_PLAYER_X_PG_DEREFERENCE: i32 = 458;
/// `PayImprovement(class i[0], i[1])` (virtual)
pub const EE_UNITS_PLAYER_X_PAY_IMPROVEMENT: i32 = 459;

/// `kUnitTypeTeleporter`, `kAIKindHero`, the improvement without a step (`12`)
const TYPE_TELEPORTER: i64 = 0x313;
const AI_KIND_HERO: i64 = 0x1b;
const IMPROVEMENT_NO_STEP: i64 = 12;
/// a tile x off the map
const OFF_MAP: i64 = -10000;

use crate::units::player_split::Call;

fn ask(f: Call, op: i32, args: &[i64]) -> i64 {
    let mut io = EeUnitsPlayerIo::default();
    io.i[..args.len()].copy_from_slice(args);
    f(op, &mut io)
}

fn askf(f: Call, op: i32, args: &[i64]) -> f32 {
    let mut io = EeUnitsPlayerIo::default();
    io.i[..args.len()].copy_from_slice(args);
    f(op, &mut io);
    io.f[0]
}

/// `gUnitClassTable[t]` past the table's bounds as none.
fn class_at(f: Call, t: i64) -> i64 {
    if (t as u64) < ask(f, EE_UNITS_PLAYER_X_CLASS_COUNT, &[]) as u64 {
        ask(f, EE_UNITS_PLAYER_X_CLASS_AT, &[t])
    } else {
        0
    }
}

fn unpack(v: i64) -> (i64, i64) {
    ((v >> 32) as i32 as i64, v as i32 as i64)
}

/// The compound operation `op`'s statements (None: not one of these).
pub(crate) fn split(op: i32, io: &mut EeUnitsPlayerIo, f: Call) -> Option<i64> {
    let a = io.i;
    match op {
        EE_UNITS_PLAYER_CLASS => {
            let class = class_at(f, a[0]);
            if class == 0 {
                return Some(0);
            }
            io.i = [
                ask(f, EE_UNITS_PLAYER_X_CLASS_TECH, &[class]),
                ask(f, EE_UNITS_PLAYER_X_CLASS_WEIGHT, &[class]),
                ask(f, EE_UNITS_PLAYER_X_CLASS_POPULATION, &[class]),
                i64::from(ask(f, EE_UNITS_PLAYER_X_CLASS_TYPE, &[class]) == TYPE_TELEPORTER),
                i64::from(ask(f, EE_UNITS_PLAYER_X_CLASS_AI_KIND, &[class]) == AI_KIND_HERO),
                i64::from(ask(f, EE_UNITS_PLAYER_X_CLASS_IMPROVEMENTS, &[class]) != 0),
                0,
                0,
            ];
            Some(1)
        }
        EE_UNITS_PLAYER_IMPROVEMENT | EE_UNITS_PLAYER_IMPROVEMENT_COST | EE_UNITS_PLAYER_LEVEL => {
            let (t, improvement) = (a[0], a[1]);
            let class = class_at(f, t);
            let table = if class != 0 { ask(f, EE_UNITS_PLAYER_X_CLASS_IMPROVEMENTS, &[class]) } else { 0 };
            let up = if class != 0 { ask(f, EE_UNITS_PLAYER_X_UPGRADE_AT, &[t]) } else { 0 };
            if op == EE_UNITS_PLAYER_IMPROVEMENT {
                io.f[0] = if improvement == IMPROVEMENT_NO_STEP || table == 0 {
                    0.0
                } else {
                    askf(f, EE_UNITS_PLAYER_X_TABLE_STEP, &[table, improvement])
                };
                io.f[1] =
                    if table != 0 { askf(f, EE_UNITS_PLAYER_X_TABLE_COST_FACTOR, &[table, improvement]) } else { 0.0 };
            }
            if up == 0 {
                return Some(0);
            }
            if op == EE_UNITS_PLAYER_IMPROVEMENT_COST {
                return Some(ask(f, EE_UNITS_PLAYER_X_UP_COST, &[up, improvement]));
            }
            io.i[0] = ask(f, EE_UNITS_PLAYER_X_UP_LEVEL, &[up, improvement]);
            if op == EE_UNITS_PLAYER_LEVEL {
                return Some(1);
            }
            let can = ask(f, EE_UNITS_PLAYER_X_UP_CAN_UPGRADE, &[up, improvement]);
            let cost = ask(f, EE_UNITS_PLAYER_X_UP_COST, &[up, improvement]);
            let total = ask(f, EE_UNITS_PLAYER_X_UP_TOTAL, &[up]);
            let level = ask(f, EE_UNITS_PLAYER_X_UP_LEVEL, &[up, improvement]);
            let max = if table != 0 { ask(f, EE_UNITS_PLAYER_X_TABLE_MAX_LEVEL, &[table]) } else { 0 };
            let points = ask(f, EE_UNITS_PLAYER_X_POINTS_MAX, &[]);
            io.i[..6].copy_from_slice(&[can, cost, total, level, max, points]);
            Some(1)
        }
        EE_UNITS_PLAYER_PAY_IMPROVEMENT => {
            let class = class_at(f, a[0]);
            ask(f, EE_UNITS_PLAYER_X_PAY_IMPROVEMENT, &[class, a[1]]);
            Some(0)
        }
        EE_UNITS_PLAYER_PULSE_EXPLORATION => {
            let amount = askf(f, EE_UNITS_PLAYER_X_EXPLORATION_AMOUNT, &[]);
            let mut call = EeUnitsPlayerIo::default();
            call.i[0] = a[0];
            call.f[0] = amount;
            f(EE_UNITS_PLAYER_X_PULSE_SIGHT, &mut call);
            Some(0)
        }
        EE_UNITS_PLAYER_WONDER_FACTORS => {
            io.f[0] = askf(f, EE_UNITS_PLAYER_X_VARIANT_FACTOR, &[a[0]]);
            io.f[1] = askf(f, EE_UNITS_PLAYER_X_WONDER_HIT_POINT_BONUS, &[]);
            Some(0)
        }
        EE_UNITS_PLAYER_UPGRADE_VALUES => {
            let class = class_at(f, a[0]);
            let up = if class != 0 { ask(f, EE_UNITS_PLAYER_X_UPGRADE_AT, &[a[0]]) } else { 0 };
            if up == 0 {
                return Some(0);
            }
            io.i[..4].copy_from_slice(&[
                ask(f, EE_UNITS_PLAYER_X_CLASS_FLIGHT_TIME, &[class]),
                ask(f, EE_UNITS_PLAYER_X_UP_FLIGHT_TIME, &[up]),
                ask(f, EE_UNITS_PLAYER_X_UP_POWER, &[up]),
                ask(f, EE_UNITS_PLAYER_X_UP_MAX_HIT_POINTS, &[up]),
            ]);
            Some(1)
        }
        EE_UNITS_PLAYER_AREA_NEW => {
            let town = a[0] == EE_UNITS_PLAYER_TOWNS;
            let x = |t: i32, h: i32| if town { t } else { h };
            let area = if town {
                ask(f, EE_UNITS_PLAYER_X_TOWN_NEW, &[])
            } else {
                ask(f, EE_UNITS_PLAYER_X_HERD_NEW, &[a[1]])
            };
            ask(f, x(EE_UNITS_PLAYER_X_TOWN_SERIAL, EE_UNITS_PLAYER_X_HERD_SERIAL), &[area]);
            ask(f, x(EE_UNITS_PLAYER_X_TOWN_ADD_AREA, EE_UNITS_PLAYER_X_HERD_ADD_AREA), &[area]);
            let taken = ask(f, EE_UNITS_PLAYER_AREA_ADD, &[a[0], area, a[1]]) != 0;
            if taken {
                ask(f, x(EE_UNITS_PLAYER_X_TOWNS_PUSH, EE_UNITS_PLAYER_X_HERDS_PUSH), &[area]);
            } else {
                ask(f, x(EE_UNITS_PLAYER_X_TOWN_DELETE, EE_UNITS_PLAYER_X_HERD_DELETE), &[area]);
            }
            Some(i64::from(taken))
        }
        EE_UNITS_PLAYER_AREA_REMOVE_UNIT => {
            let x = if a[0] == EE_UNITS_PLAYER_TOWNS {
                EE_UNITS_PLAYER_X_TOWN_REMOVE_UNIT
            } else {
                EE_UNITS_PLAYER_X_HERD_REMOVE_UNIT
            };
            ask(f, x, &[a[1], a[2]]);
            Some(0)
        }
        EE_UNITS_PLAYER_AREA_DROP => {
            let area = ask(f, EE_UNITS_PLAYER_AREA_AT, &[a[0], a[1]]);
            if a[0] == EE_UNITS_PLAYER_TOWNS {
                ask(f, EE_UNITS_PLAYER_X_TOWN_REMOVE_AREA, &[area]);
                ask(f, EE_UNITS_PLAYER_X_TOWNS_ERASE, &[a[1]]);
                ask(f, EE_UNITS_PLAYER_X_TOWN_DELETE, &[area]);
            } else {
                ask(f, EE_UNITS_PLAYER_X_HERD_REMOVE_AREA, &[area]);
                ask(f, EE_UNITS_PLAYER_X_HERDS_ERASE, &[a[1]]);
                ask(f, EE_UNITS_PLAYER_X_HERD_DELETE, &[area]);
            }
            Some(0)
        }
        EE_UNITS_PLAYER_TOWN_MERGE => {
            let (town, old) = (a[0], a[1]);
            ask(f, EE_UNITS_PLAYER_X_TOWN_MERGE, &[town, old]);
            ask(f, EE_UNITS_PLAYER_X_TOWN_REMOVE_AREA, &[old]);
            let n = ask(f, EE_UNITS_PLAYER_AREA_COUNT, &[EE_UNITS_PLAYER_TOWNS]);
            let Some(i) = (0..n).find(|&i| ask(f, EE_UNITS_PLAYER_AREA_AT, &[EE_UNITS_PLAYER_TOWNS, i]) == old) else {
                return Some(0);
            };
            ask(f, EE_UNITS_PLAYER_X_TOWNS_ERASE, &[i]);
            let group = ask(f, EE_UNITS_PLAYER_X_TOWN_PATROL_OF, &[old]);
            if group != 0 {
                loop {
                    let point = ask(f, EE_UNITS_PLAYER_X_PG_LAST, &[group]);
                    if point == 0 {
                        break;
                    }
                    ask(f, EE_UNITS_PLAYER_X_PG_REMOVE, &[group, point]);
                    ask(f, EE_UNITS_PLAYER_X_POINT_UNPATROL, &[point]);
                    ask(f, EE_UNITS_PLAYER_X_POINT_KILL, &[point]);
                    ask(f, EE_UNITS_PLAYER_X_POINT_KILL_UNIT, &[point]);
                }
                ask(f, EE_UNITS_PLAYER_X_PG_REFERENCE, &[group]);
            }
            ask(f, EE_UNITS_PLAYER_X_TOWN_DELETE, &[old]);
            Some(group)
        }
        EE_UNITS_PLAYER_TOWN_REPATROL => {
            let group = ask(f, EE_UNITS_PLAYER_X_TOWN_PATROL_OF, &[a[0]]);
            if group == 0 {
                return Some(0);
            }
            // the old groups (addresses as int64_t: 64-bit pointers)
            let old: Vec<i64> = if a[2] > 0 && a[1] != 0 {
                // SAFETY: the caller's `a[2]` group addresses at `a[1]`
                unsafe { std::slice::from_raw_parts(a[1] as usize as *const i64, a[2] as usize) }.to_vec()
            } else {
                Vec::new()
            };
            let mut u = ask(f, EE_UNITS_PLAYER_X_FIRST_UNIT, &[0]);
            while u != 0 {
                let complex = ask(f, EE_UNITS_PLAYER_X_COMPLEX, &[u]);
                let goal = if complex != 0 { ask(f, EE_UNITS_PLAYER_X_PATROL_GOAL, &[complex]) } else { 0 };
                if goal != 0 {
                    let g = ask(f, EE_UNITS_PLAYER_X_GOAL_PATROL_GROUP, &[goal]);
                    if g != 0 && old.contains(&g) {
                        ask(f, EE_UNITS_PLAYER_X_GOAL_SET_PATROL, &[goal, group]);
                    }
                }
                u = ask(f, EE_UNITS_PLAYER_X_NEXT_UNIT, &[u]);
            }
            for g in old {
                ask(f, EE_UNITS_PLAYER_X_PG_DEREFERENCE, &[g]);
            }
            Some(0)
        }
        EE_UNITS_PLAYER_AREA_TILE => {
            let (x, y) = unpack(ask(f, EE_UNITS_PLAYER_X_FIND_AREA_TILE, &[a[0]]));
            io.i[0] = x;
            io.i[1] = y;
            Some(0)
        }
        EE_UNITS_PLAYER_ASSIGN_TILE => {
            let building = ask(f, EE_UNITS_PLAYER_X_COMPLEX, &[a[0]]);
            let on_map = ask(f, EE_UNITS_PLAYER_X_TILE_X, &[building]) != OFF_MAP;
            io.i[0] = if on_map {
                ask(f, EE_UNITS_PLAYER_X_TILE_X, &[a[0]])
            } else {
                ask(f, EE_UNITS_PLAYER_X_LAST_FLOOR_X, &[building])
            };
            io.i[1] = if on_map {
                ask(f, EE_UNITS_PLAYER_X_TILE_Y, &[a[0]])
            } else {
                ask(f, EE_UNITS_PLAYER_X_LAST_FLOOR_Y, &[building])
            };
            Some(0)
        }
        EE_UNITS_PLAYER_TOWN_PATROL => {
            if ask(f, EE_UNITS_PLAYER_X_TOWN_PATROL_OF, &[a[0]]) == 0 {
                ask(f, EE_UNITS_PLAYER_X_TOWN_REFRESH_PATROL, &[a[0]]);
            }
            Some(0)
        }
        _ => None,
    }
}

#[cfg(test)]
#[path = "player_split_more_tests.rs"]
mod tests;
