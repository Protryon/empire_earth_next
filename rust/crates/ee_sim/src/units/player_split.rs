//! The player host's compound operations as one-statement C++ operations (the groups lane, round 2): the bodies
//! (player.rs, player_units.rs, player_diplomacy.rs and their tests) keep asking `EE_UNITS_PLAYER_CAMOUFLAGE(on)`,
//! `EE_UNITS_PLAYER_UNITS_SIGHT(player, on, land only)` and the like; in the game, [`split`] makes each such ask the
//! C++ statements it was (EEPlayer.cpp's `EEPlayerHost`: one statement a case), the choice, the loop and the test in
//! Rust, in the original's order.

use crate::units::player::{EeUnitsPlayerIo, EE_UNITS_PLAYER_IMPROVEMENT_TIME, EE_UNITS_PLAYER_REASON};
use crate::units::player_diplomacy::{
    EE_UNITS_PLAYER_ALLY, EE_UNITS_PLAYER_EVENTS_SIGHT, EE_UNITS_PLAYER_MINIMAP_REDRAW, EE_UNITS_PLAYER_SHARERS,
    EE_UNITS_PLAYER_UNITS_SIGHT,
};
use crate::units::player_units::{
    EE_UNITS_PLAYER_CAMOUFLAGE, EE_UNITS_PLAYER_COMPLEX_SIGHT, EE_UNITS_PLAYER_REMOVE_FOREST,
    EE_UNITS_PLAYER_SET_VALUE, EE_UNITS_PLAYER_SHARE_BONUS, EE_UNITS_PLAYER_SIGHT_SHARER, EE_UNITS_PLAYER_STATS_RESET,
    EE_UNITS_PLAYER_TECH_ALLOW, EE_UNITS_PLAYER_UPGRADE_TYPE, EE_UNITS_PLAYER_WONDER_LIST,
    EE_UNITS_PLAYER_WONDER_STARTED,
};

// ---- the one-statement operations (EEPlayerHost's cases)
/// `EnableCamouflagePower()`
pub const EE_UNITS_PLAYER_X_CAMOUFLAGE_ON: i32 = 300;
/// `DisableCamouflagePower()`
pub const EE_UNITS_PLAYER_X_CAMOUFLAGE_OFF: i32 = 301;
/// `gWorld.RemoveShareBonus()`
pub const EE_UNITS_PLAYER_X_SHARE_REMOVE: i32 = 302;
/// `gWorld.ApplyShareBonus()`
pub const EE_UNITS_PLAYER_X_SHARE_APPLY: i32 = 303;
/// player `i[0]` (0 this one)`->AddSightSharer(this)`
pub const EE_UNITS_PLAYER_X_SHARER_ADD: i32 = 304;
/// player `i[0]` (0 this one)`->RemoveSightSharer(this)`
pub const EE_UNITS_PLAYER_X_SHARER_REMOVE: i32 = 305;
/// unit `i[0]`'s complex unit's `SightOnHere()`
pub const EE_UNITS_PLAYER_X_COMPLEX_SIGHT_ON: i32 = 306;
/// unit `i[0]`'s complex unit's `SightOffHere()`
pub const EE_UNITS_PLAYER_X_COMPLEX_SIGHT_OFF: i32 = 307;
/// complex unit `i[0]`'s `SetFlightTime(i[1])`
pub const EE_UNITS_PLAYER_X_SET_FLIGHT_TIME: i32 = 308;
/// complex unit `i[0]`'s `SetPower(i[1])`
pub const EE_UNITS_PLAYER_X_SET_POWER: i32 = 309;
/// unit `i[0]`'s `SetHitPoints(i[1])`
pub const EE_UNITS_PLAYER_X_SET_HIT_POINTS: i32 = 310;
/// player `i[0]` (0 this one)`->mEvents.ShowEvents(it)`
pub const EE_UNITS_PLAYER_X_EVENTS_SHOW: i32 = 311;
/// player `i[0]` (0 this one)`->mEvents.HideEvents(it)`
pub const EE_UNITS_PLAYER_X_EVENTS_HIDE: i32 = 312;
/// player `i[0]` (0 this one)`->mAllies.push_back(player i[1] (0 this one))`
pub const EE_UNITS_PLAYER_X_ALLY_ADD: i32 = 313;
/// player `i[0]` (0 this one)`->RemoveAlly(player i[1] (0 this one))`
pub const EE_UNITS_PLAYER_X_ALLY_REMOVE: i32 = 314;
/// `mSightSharers.push_back(player i[0] (0 this one))`
pub const EE_UNITS_PLAYER_X_SHARERS_PUSH: i32 = 315;
/// whether `mSightSharers` holds player `i[0]` (0 this one)
pub const EE_UNITS_PLAYER_X_SHARERS_HAS: i32 = 316;
/// `mSightSharers.erase(std::find(..., player i[0] (0 this one)))`
pub const EE_UNITS_PLAYER_X_SHARERS_ERASE: i32 = 317;
/// `gMiniMap != NULL`
pub const EE_UNITS_PLAYER_X_MINIMAP: i32 = 318;
/// `gMiniMap->Redraw()`
pub const EE_UNITS_PLAYER_X_MINIMAP_REDRAW: i32 = 319;
/// `mWonders->Add(unit i[0])`
pub const EE_UNITS_PLAYER_X_WONDERS_ADD: i32 = 320;
/// `mWonders->Remove(unit i[0])`
pub const EE_UNITS_PLAYER_X_WONDERS_REMOVE: i32 = 321;
/// complex unit `i[0]`'s `mCloaked = gPhysics->mGameTime`
pub const EE_UNITS_PLAYER_X_CLOAKED_NOW: i32 = 322;
/// complex unit `i[0]`'s `mCloaked = 0`
pub const EE_UNITS_PLAYER_X_CLOAKED_CLEAR: i32 = 323;
/// `mSavedGroups.Lower(i[0], i[1])` (as `EE_UNITS_PLAYER_STATS_LOWER`)
pub const EE_UNITS_PLAYER_X_STATS_LOWER: i32 = 324;
/// player `i[0]` (0 this one)'s `mFirstUnit` (an `RTSUnit*`)
pub const EE_UNITS_PLAYER_X_FIRST_UNIT: i32 = 325;
/// unit `i[0]`'s `mNext`
pub const EE_UNITS_PLAYER_X_NEXT_UNIT: i32 = 326;
/// unit `i[0]`'s class `IsLandBuilding()`
pub const EE_UNITS_PLAYER_X_LAND_BUILDING: i32 = 327;
/// unit `i[0]`'s `SightOnHere()`
pub const EE_UNITS_PLAYER_X_SIGHT_ON: i32 = 328;
/// unit `i[0]`'s `SightOffHere()`
pub const EE_UNITS_PLAYER_X_SIGHT_OFF: i32 = 329;
/// `gWorld.mPlayers[i[0]] == this`
pub const EE_UNITS_PLAYER_X_IS_SELF: i32 = 330;
/// `gWorld.mPlayers[i[0]]->WonderStarted(complex unit i[1])`
pub const EE_UNITS_PLAYER_X_WONDER_STARTED: i32 = 331;
/// unit `i[0]`'s `mResourceAmount`
pub const EE_UNITS_PLAYER_X_RESOURCE: i32 = 332;
/// unit `i[0]`'s `mResourceAmount = 0`
pub const EE_UNITS_PLAYER_X_RESOURCE_CLEAR: i32 = 333;
/// `gContinentManager.RemoveForestTile(unit i[0])`
pub const EE_UNITS_PLAYER_X_FOREST_REMOVE: i32 = 334;
/// node `i[0]`'s `mIsAllowed = i[1] != 0`
pub const EE_UNITS_PLAYER_X_NODE_ALLOW: i32 = 335;
/// `mUnitClassUpgrades.size()`
pub const EE_UNITS_PLAYER_X_UPGRADE_SLOTS: i32 = 336;
/// `mUnitClassUpgrades[i[0]]` (an `EEUnitClassUpgrade*`)
pub const EE_UNITS_PLAYER_X_UPGRADE_AT: i32 = 337;
/// upgrade `i[0]`'s `mUnitClass->GetUnitType()`
pub const EE_UNITS_PLAYER_X_UPGRADE_UNIT_TYPE: i32 = 338;
/// `mTechnologies->Get(i[0])` (0 none)
pub const EE_UNITS_PLAYER_X_NODE: i32 = 339;

/// `gResource->GetString(i[0], *(UWideString*)i[1])`
pub const EE_UNITS_PLAYER_X_STRING: i32 = 340;
/// `(long)` of the world table's `GetLongValue2(EEWorldImprovementTime(i[0]))`
pub const EE_UNITS_PLAYER_X_IMPROVEMENT_TIME2: i32 = 341;
/// `(long)` of the world table's `GetLongValue(EEWorldImprovementTime(i[0]))`
pub const EE_UNITS_PLAYER_X_IMPROVEMENT_TIME1: i32 = 342;

/// The reasons' strings (`EE_UNITS_PLAYER_REASON`'s kinds 1..6): `IDS_YOU_HAVE_ALREADY_BUILT_MAXIMUM`,
/// `IDS_YOU_HAVE_REACHED_YOUR_POP_LIMIT`, `IDS_UNIT_EXCEEDS_POP_LIMIT`, `IDS_NOT_ENOUGH_RESOURCES_FOR_CONSTRUCTION`,
/// `IDS_NOT_ENOUGH_RESOURCES_TO_RESEARCH_THIS_TECHNOLOGY`, `IDS_MAXIMUM_NUMBER_OF_IMPROVEMENTS_HAVE_BEEN`
const REASONS: [i64; 7] = [0, 23001, 23000, 10059, 10042, 10061, 10060];

/// One answer of the C++.
pub(crate) type Call<'a> = &'a mut dyn FnMut(i32, &mut EeUnitsPlayerIo) -> i64;

fn ask(f: Call, op: i32, args: &[i64]) -> i64 {
    let mut io = EeUnitsPlayerIo::default();
    io.i[..args.len()].copy_from_slice(args);
    f(op, &mut io)
}

/// Operation `op` over the one-statement operations of `f` when it is a compound one (its answer); `None` for the
/// others (`f` answers them as they are).
pub(crate) fn split(op: i32, io: &mut EeUnitsPlayerIo, f: Call) -> Option<i64> {
    let a = io.i;
    let on = |x: i64, yes: i32, no: i32| if x != 0 { yes } else { no };
    match op {
        EE_UNITS_PLAYER_CAMOUFLAGE => {
            ask(f, on(a[0], EE_UNITS_PLAYER_X_CAMOUFLAGE_ON, EE_UNITS_PLAYER_X_CAMOUFLAGE_OFF), &[]);
        }
        EE_UNITS_PLAYER_SHARE_BONUS => {
            ask(f, on(a[0], EE_UNITS_PLAYER_X_SHARE_REMOVE, EE_UNITS_PLAYER_X_SHARE_APPLY), &[]);
        }
        EE_UNITS_PLAYER_SIGHT_SHARER => {
            ask(f, on(a[1], EE_UNITS_PLAYER_X_SHARER_ADD, EE_UNITS_PLAYER_X_SHARER_REMOVE), &[a[0]]);
        }
        EE_UNITS_PLAYER_COMPLEX_SIGHT => {
            ask(f, on(a[1], EE_UNITS_PLAYER_X_COMPLEX_SIGHT_ON, EE_UNITS_PLAYER_X_COMPLEX_SIGHT_OFF), &[a[0]]);
        }
        EE_UNITS_PLAYER_SET_VALUE => {
            let x = match a[2] {
                0 => EE_UNITS_PLAYER_X_SET_FLIGHT_TIME,
                1 => EE_UNITS_PLAYER_X_SET_POWER,
                _ => EE_UNITS_PLAYER_X_SET_HIT_POINTS,
            };
            ask(f, x, &[a[0], a[1]]);
        }
        EE_UNITS_PLAYER_EVENTS_SIGHT => {
            ask(f, on(a[1], EE_UNITS_PLAYER_X_EVENTS_SHOW, EE_UNITS_PLAYER_X_EVENTS_HIDE), &[a[0]]);
        }
        EE_UNITS_PLAYER_ALLY => {
            ask(f, on(a[2], EE_UNITS_PLAYER_X_ALLY_ADD, EE_UNITS_PLAYER_X_ALLY_REMOVE), &[a[0], a[1]]);
        }
        EE_UNITS_PLAYER_SHARERS => {
            if a[1] != 0 {
                ask(f, EE_UNITS_PLAYER_X_SHARERS_PUSH, &[a[0]]);
            } else if ask(f, EE_UNITS_PLAYER_X_SHARERS_HAS, &[a[0]]) != 0 {
                ask(f, EE_UNITS_PLAYER_X_SHARERS_ERASE, &[a[0]]);
            }
        }
        EE_UNITS_PLAYER_MINIMAP_REDRAW => {
            if ask(f, EE_UNITS_PLAYER_X_MINIMAP, &[]) != 0 {
                ask(f, EE_UNITS_PLAYER_X_MINIMAP_REDRAW, &[]);
            }
        }
        EE_UNITS_PLAYER_WONDER_LIST => {
            if a[1] != 0 {
                ask(f, EE_UNITS_PLAYER_X_WONDERS_ADD, &[a[0]]);
                ask(f, EE_UNITS_PLAYER_X_CLOAKED_NOW, &[a[0]]);
                ask(f, EE_UNITS_PLAYER_STATS_RESET, &[0x10, 1]);
            } else {
                ask(f, EE_UNITS_PLAYER_X_WONDERS_REMOVE, &[a[0]]);
                ask(f, EE_UNITS_PLAYER_X_CLOAKED_CLEAR, &[a[0]]);
                ask(f, EE_UNITS_PLAYER_X_STATS_LOWER, &[0x10, 1]);
            }
        }
        EE_UNITS_PLAYER_UNITS_SIGHT => {
            let mut u = ask(f, EE_UNITS_PLAYER_X_FIRST_UNIT, &[a[0]]);
            while u != 0 {
                if a[2] == 0 || ask(f, EE_UNITS_PLAYER_X_LAND_BUILDING, &[u]) != 0 {
                    ask(f, on(a[1], EE_UNITS_PLAYER_X_SIGHT_ON, EE_UNITS_PLAYER_X_SIGHT_OFF), &[u]);
                }
                u = ask(f, EE_UNITS_PLAYER_X_NEXT_UNIT, &[u]);
            }
        }
        EE_UNITS_PLAYER_WONDER_STARTED => {
            if ask(f, EE_UNITS_PLAYER_X_IS_SELF, &[a[0]]) == 0 {
                ask(f, EE_UNITS_PLAYER_X_WONDER_STARTED, &[a[0], a[1]]);
            }
        }
        EE_UNITS_PLAYER_REMOVE_FOREST => {
            if ask(f, EE_UNITS_PLAYER_X_RESOURCE, &[a[0]]) != 0 {
                ask(f, EE_UNITS_PLAYER_X_RESOURCE_CLEAR, &[a[0]]);
            }
            ask(f, EE_UNITS_PLAYER_X_FOREST_REMOVE, &[a[0]]);
        }
        EE_UNITS_PLAYER_TECH_ALLOW => {
            let node = ask(f, EE_UNITS_PLAYER_X_NODE, &[a[0]]);
            if node != 0 {
                ask(f, EE_UNITS_PLAYER_X_NODE_ALLOW, &[node, a[1]]);
            }
        }
        EE_UNITS_PLAYER_UPGRADE_TYPE => {
            if a[0] as u64 >= ask(f, EE_UNITS_PLAYER_X_UPGRADE_SLOTS, &[]) as u64 {
                return Some(-2);
            }
            let up = ask(f, EE_UNITS_PLAYER_X_UPGRADE_AT, &[a[0]]);
            return Some(if up != 0 { ask(f, EE_UNITS_PLAYER_X_UPGRADE_UNIT_TYPE, &[up]) } else { -1 });
        }
        EE_UNITS_PLAYER_REASON => {
            ask(f, EE_UNITS_PLAYER_X_STRING, &[REASONS[a[1] as usize], a[0]]);
        }
        EE_UNITS_PLAYER_IMPROVEMENT_TIME => {
            return Some(ask(
                f,
                on(a[1], EE_UNITS_PLAYER_X_IMPROVEMENT_TIME2, EE_UNITS_PLAYER_X_IMPROVEMENT_TIME1),
                &[a[0]],
            ));
        }
        _ => return None,
    }
    Some(0)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn run(op: i32, args: &[i64], answers: &[(i32, i64)]) -> (Option<i64>, Vec<(i32, Vec<i64>)>) {
        let mut log = Vec::new();
        let mut io = EeUnitsPlayerIo::default();
        io.i[..args.len()].copy_from_slice(args);
        let mut queue: Vec<(i32, i64)> = answers.to_vec();
        let mut f = |op: i32, io: &mut EeUnitsPlayerIo| {
            log.push((op, io.i[..2].to_vec()));
            match queue.iter().position(|&(o, _)| o == op) {
                Some(k) => queue.remove(k).1,
                None => 0,
            }
        };
        let r = split(op, &mut io, &mut f);
        (r, log)
    }

    #[test]
    fn the_choices_are_the_flags() {
        assert_eq!(run(EE_UNITS_PLAYER_CAMOUFLAGE, &[1], &[]).1[0].0, EE_UNITS_PLAYER_X_CAMOUFLAGE_ON);
        assert_eq!(run(EE_UNITS_PLAYER_CAMOUFLAGE, &[0], &[]).1[0].0, EE_UNITS_PLAYER_X_CAMOUFLAGE_OFF);
        assert_eq!(run(EE_UNITS_PLAYER_SHARE_BONUS, &[1], &[]).1[0].0, EE_UNITS_PLAYER_X_SHARE_REMOVE);
        assert_eq!(
            run(EE_UNITS_PLAYER_SIGHT_SHARER, &[7, 0], &[]).1,
            vec![(EE_UNITS_PLAYER_X_SHARER_REMOVE, vec![7, 0])]
        );
        assert_eq!(run(EE_UNITS_PLAYER_SET_VALUE, &[5, 9, 1], &[]).1, vec![(EE_UNITS_PLAYER_X_SET_POWER, vec![5, 9])]);
        assert_eq!(run(EE_UNITS_PLAYER_SET_VALUE, &[5, 9, 2], &[]).1[0].0, EE_UNITS_PLAYER_X_SET_HIT_POINTS);
        assert_eq!(run(EE_UNITS_PLAYER_ALLY, &[3, 4, 1], &[]).1, vec![(EE_UNITS_PLAYER_X_ALLY_ADD, vec![3, 4])]);
        assert_eq!(run(EE_UNITS_PLAYER_EVENTS_SIGHT, &[3, 0], &[]).1[0].0, EE_UNITS_PLAYER_X_EVENTS_HIDE);
        assert_eq!(run(EE_UNITS_PLAYER_COMPLEX_SIGHT, &[3, 1], &[]).1[0].0, EE_UNITS_PLAYER_X_COMPLEX_SIGHT_ON);
        assert_eq!(
            run(EE_UNITS_PLAYER_REASON, &[0x500, 3], &[]).1,
            vec![(EE_UNITS_PLAYER_X_STRING, vec![10059, 0x500])]
        );
        let (r, log) = run(EE_UNITS_PLAYER_IMPROVEMENT_TIME, &[4, 1], &[(EE_UNITS_PLAYER_X_IMPROVEMENT_TIME2, 30)]);
        assert_eq!((r, log), (Some(30), vec![(EE_UNITS_PLAYER_X_IMPROVEMENT_TIME2, vec![4, 0])]));
        // not compound: the host answers it
        assert_eq!(run(1, &[], &[]), (None, vec![]));
    }

    #[test]
    fn the_tests_and_loops_ask_first() {
        // a sharer not there is not erased
        let (_, log) = run(EE_UNITS_PLAYER_SHARERS, &[7, 0], &[]);
        assert_eq!(log, vec![(EE_UNITS_PLAYER_X_SHARERS_HAS, vec![7, 0])]);
        let (_, log) = run(EE_UNITS_PLAYER_SHARERS, &[7, 0], &[(EE_UNITS_PLAYER_X_SHARERS_HAS, 1)]);
        assert_eq!(log[1], (EE_UNITS_PLAYER_X_SHARERS_ERASE, vec![7, 0]));
        // the land buildings of a unit chain 0x10 -> 0x20 -> end; only 0x20 is one
        let (_, log) = run(
            EE_UNITS_PLAYER_UNITS_SIGHT,
            &[0, 1, 1],
            &[
                (EE_UNITS_PLAYER_X_FIRST_UNIT, 0x10),
                (EE_UNITS_PLAYER_X_LAND_BUILDING, 0),
                (EE_UNITS_PLAYER_X_NEXT_UNIT, 0x20),
                (EE_UNITS_PLAYER_X_LAND_BUILDING, 1),
            ],
        );
        let ops: Vec<i32> = log.iter().map(|l| l.0).collect();
        assert_eq!(
            ops,
            vec![
                EE_UNITS_PLAYER_X_FIRST_UNIT,
                EE_UNITS_PLAYER_X_LAND_BUILDING,
                EE_UNITS_PLAYER_X_NEXT_UNIT,
                EE_UNITS_PLAYER_X_LAND_BUILDING,
                EE_UNITS_PLAYER_X_SIGHT_ON,
                EE_UNITS_PLAYER_X_NEXT_UNIT,
            ]
        );
        // the wonders: add, the time, the statistic
        let (_, log) = run(EE_UNITS_PLAYER_WONDER_LIST, &[0x40, 1], &[]);
        assert_eq!(log[2], (EE_UNITS_PLAYER_STATS_RESET, vec![0x10, 1]));
        // a forest with no wood left is not written; the tile goes either way
        let (_, log) = run(EE_UNITS_PLAYER_REMOVE_FOREST, &[0x40], &[]);
        assert_eq!(
            log.iter().map(|l| l.0).collect::<Vec<_>>(),
            vec![EE_UNITS_PLAYER_X_RESOURCE, EE_UNITS_PLAYER_X_FOREST_REMOVE]
        );
        // the minimap redraws only when there is one
        assert_eq!(run(EE_UNITS_PLAYER_MINIMAP_REDRAW, &[], &[]).1.len(), 1);
        // a wonder started by the player itself tells no one
        assert_eq!(run(EE_UNITS_PLAYER_WONDER_STARTED, &[2, 0x40], &[(EE_UNITS_PLAYER_X_IS_SELF, 1)]).1.len(), 1);
        // a missing node is not allowed
        assert_eq!(run(EE_UNITS_PLAYER_TECH_ALLOW, &[1025, 1], &[]).1.len(), 1);
        // the upgrade's unit type: -2 past the slots, -1 for an empty one
        assert_eq!(run(EE_UNITS_PLAYER_UPGRADE_TYPE, &[5], &[(EE_UNITS_PLAYER_X_UPGRADE_SLOTS, 5)]).0, Some(-2));
        assert_eq!(run(EE_UNITS_PLAYER_UPGRADE_TYPE, &[4], &[(EE_UNITS_PLAYER_X_UPGRADE_SLOTS, 5)]).0, Some(-1));
        let r = run(
            EE_UNITS_PLAYER_UPGRADE_TYPE,
            &[4],
            &[
                (EE_UNITS_PLAYER_X_UPGRADE_SLOTS, 5),
                (EE_UNITS_PLAYER_X_UPGRADE_AT, 0x99),
                (EE_UNITS_PLAYER_X_UPGRADE_UNIT_TYPE, 32),
            ],
        );
        assert_eq!(r.0, Some(32));
    }
}
