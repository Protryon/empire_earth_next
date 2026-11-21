//! The player save's compound operations as one-statement C++ operations (the groups lane, round 2): `load` and
//! `save` (player_save.rs and its tests) keep asking `EE_UNITS_PLAYER_SAVE_UPGRADE_DUMMY_NEW`,
//! `EE_UNITS_PLAYER_SAVE_TECH_LOAD` and the like; in the game, [`split`] makes each such ask the C++ statements it was
//! (EEPlayer.cpp's `EEPlayerSaveOperation`: one statement a case), the loop and the tests in Rust, in the original's
//! order.

use crate::units::player_save::{
    EeUnitsPlayerSaveIo, EE_UNITS_PLAYER_SAVE_PATROL_KIND, EE_UNITS_PLAYER_SAVE_PATROL_LOAD,
    EE_UNITS_PLAYER_SAVE_TECH_LOAD, EE_UNITS_PLAYER_SAVE_TECH_PRESENT, EE_UNITS_PLAYER_SAVE_TECH_SAVE,
    EE_UNITS_PLAYER_SAVE_UNIT_FITS, EE_UNITS_PLAYER_SAVE_UNIT_KIND, EE_UNITS_PLAYER_SAVE_UNIT_LOAD,
    EE_UNITS_PLAYER_SAVE_UNIT_STANDIN, EE_UNITS_PLAYER_SAVE_UPGRADE_DUMMY_NEW, EE_UNITS_PLAYER_SAVE_UPGRADE_LOAD,
    EE_UNITS_PLAYER_SAVE_WONDERS_CLEAR,
};

// ---- the one-statement operations (EEPlayerSaveOperation's cases)
/// `EEUnitClassCount()`
pub const EE_UNITS_PLAYER_SAVE_X_CLASS_COUNT: i32 = 300;
/// `gUnitClassTable[index] != NULL`
pub const EE_UNITS_PLAYER_SAVE_X_CLASS_PRESENT: i32 = 301;
/// the load's stand-in upgrade = NULL
pub const EE_UNITS_PLAYER_SAVE_X_DUMMY_CLEAR: i32 = 302;
/// the load's stand-in upgrade = a new upgrade of `gUnitClassTable[index]`
pub const EE_UNITS_PLAYER_SAVE_X_DUMMY_NEW: i32 = 303;
/// `ESVERIFY_AT(the stand-in != NULL, index)`
pub const EE_UNITS_PLAYER_SAVE_X_DUMMY_VERIFY: i32 = 304;
/// upgrade `io.i` is there (`io.i < EEUnitClassCount() && mUnitClassUpgrades[io.i] != NULL`)
pub const EE_UNITS_PLAYER_SAVE_X_UPGRADE_PRESENT: i32 = 305;
/// `mUnitClassUpgrades[io.i]->Load(...)` (its result)
pub const EE_UNITS_PLAYER_SAVE_X_UPGRADE_LOAD: i32 = 306;
/// the stand-in's `Load(...)`
pub const EE_UNITS_PLAYER_SAVE_X_DUMMY_LOAD: i32 = 307;
/// `mTechnologies = new EETechTree`
pub const EE_UNITS_PLAYER_SAVE_X_TECH_NEW: i32 = 308;
/// `*mTechnologies = gTechTree`
pub const EE_UNITS_PLAYER_SAVE_X_TECH_COPY: i32 = 309;
/// `mTechnologies->SetPlayerIndex(mPlayerID)`
pub const EE_UNITS_PLAYER_SAVE_X_TECH_INDEX: i32 = 310;
/// `mTechnologies->SetPlayer(this)`
pub const EE_UNITS_PLAYER_SAVE_X_TECH_PLAYER: i32 = 311;
/// `mTechnologies->Load(...)`
pub const EE_UNITS_PLAYER_SAVE_X_TECH_LOAD: i32 = 312;
/// `ee_units_pg_new()` (into `io.i`)
pub const EE_UNITS_PLAYER_SAVE_X_PATROL_NEW: i32 = 313;
/// `ee_units_pg_load(io.i, major, minor, &file)`
pub const EE_UNITS_PLAYER_SAVE_X_PATROL_LOAD: i32 = 314;
/// `AddPatrolGroup(io.i)`
pub const EE_UNITS_PLAYER_SAVE_X_PATROL_ADD: i32 = 315;
/// `ee_units_pg_dereference(io.i)`
pub const EE_UNITS_PLAYER_SAVE_X_PATROL_RELEASE: i32 = 316;
/// `mWonders->Clear()`
pub const EE_UNITS_PLAYER_SAVE_X_WONDERS_CLEAR: i32 = 317;
/// `mWonders->mItems.clear()`
pub const EE_UNITS_PLAYER_SAVE_X_WONDERS_ITEMS_CLEAR: i32 = 318;
/// `mTechnologies->Save(...)`
pub const EE_UNITS_PLAYER_SAVE_X_TECH_SAVE: i32 = 319;
/// `mPatrolGroups[index]` (into `io.i`)
pub const EE_UNITS_PLAYER_SAVE_X_PATROL_AT: i32 = 320;

// LoadUnit's and SaveUnit's (EEPlayerUnitOperation's cases): `io.i` the class id or the unit
/// `gUnitClassTable[io.i]` is there
pub const EE_UNITS_PLAYER_SAVE_X_CLASS_THERE: i32 = 330;
/// that class's `mUnitType`
pub const EE_UNITS_PLAYER_SAVE_X_CLASS_TYPE: i32 = 331;
/// that class's `mIsComplex`
pub const EE_UNITS_PLAYER_SAVE_X_CLASS_COMPLEX: i32 = 332;
/// that class's `CreateUnitAtTile(player, -10000, -10000, NULL, 3.1415927f)` into `io.i`
pub const EE_UNITS_PLAYER_SAVE_X_UNIT_CREATE: i32 = 333;
/// unit `io.i`'s `Load(...)`
pub const EE_UNITS_PLAYER_SAVE_X_UNIT_LOAD: i32 = 334;
/// unit `io.i`'s `mIsQueuedForDelete`
pub const EE_UNITS_PLAYER_SAVE_X_UNIT_QUEUED_DELETE: i32 = 335;
/// `KillUnit(unit io.i, true)`
pub const EE_UNITS_PLAYER_SAVE_X_UNIT_KILL: i32 = 336;
/// `EEComplexUnit::Create()` into `io.i`
pub const EE_UNITS_PLAYER_SAVE_X_COMPLEX_NEW: i32 = 337;
/// `EEUnit::Create()` into `io.i`
pub const EE_UNITS_PLAYER_SAVE_X_UNIT_NEW: i32 = 338;
/// unit `io.i`'s id into the loaded ids (`ee_units_lists_add(mLists, EE_UNITS_LISTS_LOADED_IDS, ...)`)
pub const EE_UNITS_PLAYER_SAVE_X_LOADED_ID: i32 = 339;
/// unit `io.i`'s `RepairPointers(...)`
pub const EE_UNITS_PLAYER_SAVE_X_UNIT_REPAIR: i32 = 340;
/// unit `io.i`'s `Clear()`
pub const EE_UNITS_PLAYER_SAVE_X_UNIT_CLEAR: i32 = 341;
/// unit `io.i`'s `Dereference()`
pub const EE_UNITS_PLAYER_SAVE_X_UNIT_RELEASE: i32 = 342;
/// the saved unit's class's `mUnitType`
pub const EE_UNITS_PLAYER_SAVE_X_SAVED_TYPE: i32 = 343;
/// the saved unit's `mComplexUnit != NULL`
pub const EE_UNITS_PLAYER_SAVE_X_SAVED_COMPLEX: i32 = 344;

/// `kUnitTypePathPoint`
const TYPE_PATH_POINT: i64 = 0xdf;
const OK: i32 = 0;

/// One answer of the C++.
pub(crate) type Call<'a> = &'a mut dyn FnMut(i32, u32, &mut EeUnitsPlayerSaveIo) -> i32;

fn ask(f: Call, op: i32, index: u32, i: i64) -> (i32, i64) {
    let mut io = EeUnitsPlayerSaveIo { i, ..Default::default() };
    let r = f(op, index, &mut io);
    (r, io.i)
}

/// Operation `op` (at `index`) over the one-statement operations of `f` when it is a compound one (its answer);
/// `None` for the others.
pub(crate) fn split(op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo, f: Call) -> Option<i32> {
    Some(match op {
        EE_UNITS_PLAYER_SAVE_UPGRADE_DUMMY_NEW => {
            ask(f, EE_UNITS_PLAYER_SAVE_X_DUMMY_CLEAR, 0, 0);
            let mut i = 0;
            while (i as i64) < ask(f, EE_UNITS_PLAYER_SAVE_X_CLASS_COUNT, 0, 0).0 as i64 {
                if ask(f, EE_UNITS_PLAYER_SAVE_X_CLASS_PRESENT, i, 0).0 != 0 {
                    ask(f, EE_UNITS_PLAYER_SAVE_X_DUMMY_NEW, i, 0);
                    break;
                }
                i += 1;
            }
            ask(f, EE_UNITS_PLAYER_SAVE_X_DUMMY_VERIFY, index, 0);
            OK
        }
        EE_UNITS_PLAYER_SAVE_UPGRADE_LOAD => {
            if ask(f, EE_UNITS_PLAYER_SAVE_X_UPGRADE_PRESENT, 0, io.i).0 != 0 {
                ask(f, EE_UNITS_PLAYER_SAVE_X_UPGRADE_LOAD, 0, io.i);
                OK
            } else {
                ask(f, EE_UNITS_PLAYER_SAVE_X_DUMMY_LOAD, 0, 0).0
            }
        }
        EE_UNITS_PLAYER_SAVE_TECH_LOAD => {
            for x in [
                EE_UNITS_PLAYER_SAVE_X_TECH_NEW,
                EE_UNITS_PLAYER_SAVE_X_TECH_COPY,
                EE_UNITS_PLAYER_SAVE_X_TECH_INDEX,
                EE_UNITS_PLAYER_SAVE_X_TECH_PLAYER,
            ] {
                ask(f, x, 0, 0);
            }
            ask(f, EE_UNITS_PLAYER_SAVE_X_TECH_LOAD, 0, 0).0
        }
        EE_UNITS_PLAYER_SAVE_PATROL_LOAD => {
            let g = ask(f, EE_UNITS_PLAYER_SAVE_X_PATROL_NEW, 0, 0).1;
            let r = ask(f, EE_UNITS_PLAYER_SAVE_X_PATROL_LOAD, 0, g).0;
            if r == OK {
                ask(f, EE_UNITS_PLAYER_SAVE_X_PATROL_ADD, 0, g);
            }
            ask(f, EE_UNITS_PLAYER_SAVE_X_PATROL_RELEASE, 0, g);
            r
        }
        EE_UNITS_PLAYER_SAVE_WONDERS_CLEAR => {
            ask(f, EE_UNITS_PLAYER_SAVE_X_WONDERS_CLEAR, 0, 0);
            ask(f, EE_UNITS_PLAYER_SAVE_X_WONDERS_ITEMS_CLEAR, 0, 0);
            OK
        }
        EE_UNITS_PLAYER_SAVE_TECH_SAVE => {
            if ask(f, EE_UNITS_PLAYER_SAVE_TECH_PRESENT, 0, 0).0 != 0 {
                ask(f, EE_UNITS_PLAYER_SAVE_X_TECH_SAVE, 0, 0).0
            } else {
                OK
            }
        }
        EE_UNITS_PLAYER_SAVE_PATROL_KIND => {
            let g = ask(f, EE_UNITS_PLAYER_SAVE_X_PATROL_AT, index, 0).1;
            if g == 0 {
                0
            } else if is_town_patrol(g) {
                1
            } else {
                2
            }
        }
        EE_UNITS_PLAYER_SAVE_UNIT_FITS => {
            let complex = io.b != 0;
            if ask(f, EE_UNITS_PLAYER_SAVE_X_CLASS_THERE, 0, io.i).0 == 0 {
                0
            } else {
                let ty = ask(f, EE_UNITS_PLAYER_SAVE_X_CLASS_TYPE, 0, io.i).0 as i64;
                let class_complex = ask(f, EE_UNITS_PLAYER_SAVE_X_CLASS_COMPLEX, 0, io.i).0;
                i32::from(
                    (ty != TYPE_PATH_POINT && class_complex == 0 && !complex)
                        || ((ty == TYPE_PATH_POINT || class_complex == 1) && complex),
                )
            }
        }
        EE_UNITS_PLAYER_SAVE_UNIT_LOAD => {
            let unit = ask(f, EE_UNITS_PLAYER_SAVE_X_UNIT_CREATE, 0, io.i).1;
            let r = ask(f, EE_UNITS_PLAYER_SAVE_X_UNIT_LOAD, 0, unit).0;
            if ask(f, EE_UNITS_PLAYER_SAVE_X_UNIT_QUEUED_DELETE, 0, unit).0 != 0 {
                ask(f, EE_UNITS_PLAYER_SAVE_X_UNIT_KILL, 0, unit);
            }
            r
        }
        EE_UNITS_PLAYER_SAVE_UNIT_STANDIN => {
            let new = if io.b != 0 { EE_UNITS_PLAYER_SAVE_X_COMPLEX_NEW } else { EE_UNITS_PLAYER_SAVE_X_UNIT_NEW };
            let unit = ask(f, new, 0, 0).1;
            let r = ask(f, EE_UNITS_PLAYER_SAVE_X_UNIT_LOAD, 0, unit).0;
            for x in [
                EE_UNITS_PLAYER_SAVE_X_LOADED_ID,
                EE_UNITS_PLAYER_SAVE_X_UNIT_REPAIR,
                EE_UNITS_PLAYER_SAVE_X_UNIT_CLEAR,
                EE_UNITS_PLAYER_SAVE_X_UNIT_RELEASE,
            ] {
                ask(f, x, 0, unit);
            }
            r
        }
        EE_UNITS_PLAYER_SAVE_UNIT_KIND => {
            let ty = ask(f, EE_UNITS_PLAYER_SAVE_X_SAVED_TYPE, 0, 0).0;
            io.i = ty.into();
            io.b = u8::from(ty as i64 == TYPE_PATH_POINT);
            ask(f, EE_UNITS_PLAYER_SAVE_X_SAVED_COMPLEX, 0, 0).0
        }
        _ => return None,
    })
}

#[cfg(not(test))]
fn is_town_patrol(g: i64) -> bool {
    use crate::units::patrol_group::{ee_units_pg_is_town_patrol, EeUnitsPatrolGroup};
    // SAFETY: the player's live patrol group
    unsafe { ee_units_pg_is_town_patrol(g as usize as *const EeUnitsPatrolGroup) }
}

/// (the tests' patrol groups: an odd address is a town's)
#[cfg(test)]
fn is_town_patrol(g: i64) -> bool {
    g & 1 != 0
}

#[cfg(test)]
mod tests {
    use super::*;

    fn run(op: i32, index: u32, i: i64, answers: &[(i32, i32, i64)]) -> (Option<i32>, Vec<(i32, u32, i64)>) {
        let mut log = Vec::new();
        let mut queue = answers.to_vec();
        let mut f = |op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo| {
            log.push((op, index, io.i));
            match queue.iter().position(|a| a.0 == op) {
                Some(k) => {
                    let (_, r, v) = queue.remove(k);
                    if v != 0 {
                        io.i = v;
                    }
                    r
                }
                None => 0,
            }
        };
        let mut io = EeUnitsPlayerSaveIo { i, ..Default::default() };
        let r = split(op, index, &mut io, &mut f);
        (r, log)
    }

    #[test]
    fn the_stand_in_is_the_first_class_there() {
        let (r, log) = run(
            EE_UNITS_PLAYER_SAVE_UPGRADE_DUMMY_NEW,
            77,
            0,
            &[
                (EE_UNITS_PLAYER_SAVE_X_CLASS_COUNT, 3, 0),
                (EE_UNITS_PLAYER_SAVE_X_CLASS_PRESENT, 0, 0),
                (EE_UNITS_PLAYER_SAVE_X_CLASS_COUNT, 3, 0),
                (EE_UNITS_PLAYER_SAVE_X_CLASS_PRESENT, 1, 0),
            ],
        );
        assert_eq!(r, Some(0));
        let ops: Vec<(i32, u32)> = log.iter().map(|l| (l.0, l.1)).collect();
        assert_eq!(
            ops,
            vec![
                (EE_UNITS_PLAYER_SAVE_X_DUMMY_CLEAR, 0),
                (EE_UNITS_PLAYER_SAVE_X_CLASS_COUNT, 0),
                (EE_UNITS_PLAYER_SAVE_X_CLASS_PRESENT, 0),
                (EE_UNITS_PLAYER_SAVE_X_CLASS_COUNT, 0),
                (EE_UNITS_PLAYER_SAVE_X_CLASS_PRESENT, 1),
                (EE_UNITS_PLAYER_SAVE_X_DUMMY_NEW, 1),
                (EE_UNITS_PLAYER_SAVE_X_DUMMY_VERIFY, 77),
            ]
        );
        // not compound: the host answers it
        assert_eq!(run(1, 0, 0, &[]), (None, vec![]));
    }

    #[test]
    fn the_loads_answer_the_original_statuses() {
        // an upgrade there loads (its result ignored); one not there loads into the stand-in (its result)
        let (r, log) = run(
            EE_UNITS_PLAYER_SAVE_UPGRADE_LOAD,
            0,
            9,
            &[(EE_UNITS_PLAYER_SAVE_X_UPGRADE_PRESENT, 1, 0), (EE_UNITS_PLAYER_SAVE_X_UPGRADE_LOAD, 5, 0)],
        );
        assert_eq!((r, log[1]), (Some(0), (EE_UNITS_PLAYER_SAVE_X_UPGRADE_LOAD, 0, 9)));
        assert_eq!(
            run(EE_UNITS_PLAYER_SAVE_UPGRADE_LOAD, 0, 9, &[(EE_UNITS_PLAYER_SAVE_X_DUMMY_LOAD, 4, 0)]).0,
            Some(4)
        );
        assert_eq!(run(EE_UNITS_PLAYER_SAVE_TECH_LOAD, 0, 0, &[(EE_UNITS_PLAYER_SAVE_X_TECH_LOAD, 6, 0)]).0, Some(6));
        // a patrol that failed to load is released without joining
        let (r, log) = run(
            EE_UNITS_PLAYER_SAVE_PATROL_LOAD,
            0,
            0,
            &[(EE_UNITS_PLAYER_SAVE_X_PATROL_NEW, 0, 0x40), (EE_UNITS_PLAYER_SAVE_X_PATROL_LOAD, 3, 0)],
        );
        assert_eq!(r, Some(3));
        assert_eq!(
            log.iter().map(|l| (l.0, l.2)).collect::<Vec<_>>(),
            vec![
                (EE_UNITS_PLAYER_SAVE_X_PATROL_NEW, 0),
                (EE_UNITS_PLAYER_SAVE_X_PATROL_LOAD, 0x40),
                (EE_UNITS_PLAYER_SAVE_X_PATROL_RELEASE, 0x40),
            ]
        );
        assert_eq!(
            run(EE_UNITS_PLAYER_SAVE_PATROL_LOAD, 0, 0, &[(EE_UNITS_PLAYER_SAVE_X_PATROL_NEW, 0, 0x40)]).1.len(),
            4
        );
        // no tree: nothing saved, OK
        assert_eq!(run(EE_UNITS_PLAYER_SAVE_TECH_SAVE, 0, 0, &[]).1.len(), 1);
        // the patrol kinds: none 0, a town's 1, another 2
        assert_eq!(run(EE_UNITS_PLAYER_SAVE_PATROL_KIND, 2, 0, &[]).0, Some(0));
        assert_eq!(
            run(EE_UNITS_PLAYER_SAVE_PATROL_KIND, 2, 0, &[(EE_UNITS_PLAYER_SAVE_X_PATROL_AT, 0, 0x41)]).0,
            Some(1)
        );
        assert_eq!(
            run(EE_UNITS_PLAYER_SAVE_PATROL_KIND, 2, 0, &[(EE_UNITS_PLAYER_SAVE_X_PATROL_AT, 0, 0x40)]).0,
            Some(2)
        );
        assert_eq!(run(EE_UNITS_PLAYER_SAVE_WONDERS_CLEAR, 0, 0, &[]).1.len(), 2);
    }

    #[test]
    fn the_units_load_and_their_stand_ins() {
        // a plain class fits a plain unit, a path point's or a complex class a complex one
        let plain = [(EE_UNITS_PLAYER_SAVE_X_CLASS_THERE, 1, 0), (EE_UNITS_PLAYER_SAVE_X_CLASS_TYPE, 5, 0)];
        assert_eq!(run(EE_UNITS_PLAYER_SAVE_UNIT_FITS, 0, 3, &plain).0, Some(1));
        let path = [(EE_UNITS_PLAYER_SAVE_X_CLASS_THERE, 1, 0), (EE_UNITS_PLAYER_SAVE_X_CLASS_TYPE, 0xdf, 0)];
        assert_eq!(run(EE_UNITS_PLAYER_SAVE_UNIT_FITS, 0, 3, &path).0, Some(0));
        assert_eq!(run(EE_UNITS_PLAYER_SAVE_UNIT_FITS, 0, 3, &[]).0, Some(0));
        // a unit queued for delete after its load is killed; the load's status answers
        let (r, log) = run(
            EE_UNITS_PLAYER_SAVE_UNIT_LOAD,
            0,
            3,
            &[
                (EE_UNITS_PLAYER_SAVE_X_UNIT_CREATE, 0, 0x80),
                (EE_UNITS_PLAYER_SAVE_X_UNIT_LOAD, 2, 0),
                (EE_UNITS_PLAYER_SAVE_X_UNIT_QUEUED_DELETE, 1, 0),
            ],
        );
        assert_eq!(r, Some(2));
        assert_eq!(log.last().unwrap(), &(EE_UNITS_PLAYER_SAVE_X_UNIT_KILL, 0, 0x80));
        let (_, log) = run(EE_UNITS_PLAYER_SAVE_UNIT_STANDIN, 0, 0, &[(EE_UNITS_PLAYER_SAVE_X_UNIT_NEW, 0, 0x90)]);
        assert_eq!(
            log.iter().map(|l| l.0).collect::<Vec<_>>(),
            vec![
                EE_UNITS_PLAYER_SAVE_X_UNIT_NEW,
                EE_UNITS_PLAYER_SAVE_X_UNIT_LOAD,
                EE_UNITS_PLAYER_SAVE_X_LOADED_ID,
                EE_UNITS_PLAYER_SAVE_X_UNIT_REPAIR,
                EE_UNITS_PLAYER_SAVE_X_UNIT_CLEAR,
                EE_UNITS_PLAYER_SAVE_X_UNIT_RELEASE,
            ]
        );
        assert!(log[1..].iter().all(|l| l.2 == 0x90));
        // the saved unit's kind: its type, whether a path point, whether complex
        let mut log = Vec::new();
        let mut f = |op: i32, _i: u32, _io: &mut EeUnitsPlayerSaveIo| {
            log.push(op);
            if op == EE_UNITS_PLAYER_SAVE_X_SAVED_TYPE {
                0xdf
            } else {
                1
            }
        };
        let mut io = EeUnitsPlayerSaveIo::default();
        assert_eq!(split(EE_UNITS_PLAYER_SAVE_UNIT_KIND, 0, &mut io, &mut f), Some(1));
        assert_eq!((io.i, io.b), (0xdf, 1));
    }
}
