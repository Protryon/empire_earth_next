//! The readers that step over the deleted C++ classic AI's blocks of a computer player in a retail file (save versions
//! 0.229 to 0.250: the campaigns' scenarios; the Learning Campaign's are 0.231, 0.248 and 0.249 files), moved whole
//! from `EECPLegacyLoad.cpp` (the merge of main's stages 5 and 6; lane B of the AI Thread's port).
//!
//! main deleted the combat, civilization and exploration managers (and `EEComputerPlayer::Load` reads its own,
//! shorter block), so a scenario with a computer player read off its end and did not load. These read every field the
//! managers' `Load` read at those versions, in order, into nothing: the attack commander's groups (their sub groups by
//! kind, the sub groups' units, the orders; "the return" holds one) and the defence commander's aid packages
//! included. The town centre tile (the build manager's) goes to the placement.
//!
//! Each skip reads a field of the type the deleted manager declared, into a zero (`T theValue = T()`), and returns the
//! file's result; a list is a count, then that many records, stopping at the first error. The quirks kept: a sub
//! group's units do not report a read error (the reads after it fail instead), and an unknown sub group kind fails
//! the load (the original dereferenced a null sub group).

use super::file::{SaveFile, FAILED, OK};

/// The epochs of the retail data those files were written with.
const LEGACY_EPOCHS: u32 = 15;

fn at(major: u32, minor: u32, version: u32) -> bool {
    major > 0 || minor >= version
}

/// The field types the managers declared.
#[derive(Clone, Copy)]
enum T {
    Long,
    Ulong,
    Float,
    Bool,
    Ushort,
}
use T::*;

fn skip(f: &mut SaveFile, t: T) -> i32 {
    match t {
        Long => f.read_long(0).0,
        Ulong => f.read_ulong(0).0,
        Float => f.read_float(0.0).0,
        Bool => f.read_bool(false).0,
        Ushort => f.read_ushort(0).0,
    }
}

/// The fields in order, stopping at the first error.
fn skips(f: &mut SaveFile, r: i32, ts: &[T]) -> i32 {
    let mut r = r;
    for &t in ts {
        if r != OK {
            break;
        }
        r = skip(f, t);
    }
    r
}

/// `n` fields of one type.
fn skip_n(f: &mut SaveFile, r: i32, n: usize, t: T) -> i32 {
    let mut r = r;
    for _ in 0..n {
        if r != OK {
            break;
        }
        r = skip(f, t);
    }
    r
}

/// `U2DPoint<long>` / `EETileLocation`: x, y.
fn skip_tile(f: &mut SaveFile) -> i32 {
    skips(f, OK, &[Long, Long])
}

/// A count, then that many of a record.
fn skip_list(f: &mut SaveFile, mut record: impl FnMut(&mut SaveFile) -> i32) -> i32 {
    let (mut r, count) = f.read_ulong(0);
    let mut i = 0;
    while i < count && r == OK {
        r = record(f);
        i += 1;
    }
    r
}

fn skip_ids(f: &mut SaveFile) -> i32 {
    skip_list(f, |f| skip(f, Ulong))
}

/// `EECPILocation::Load`: tile, area serial, area type, player.
fn skip_location(f: &mut SaveFile) -> i32 {
    let r = skip_tile(f);
    skips(f, r, &[Ulong, Long, Ulong])
}

/// `EECPILocation::Load` and `EECPAGTask::Load`: the task's unit id, player id, aggression, type, mode, active, B31.
fn skip_task(f: &mut SaveFile) -> i32 {
    let r = skip_location(f);
    skips(f, r, &[Ulong, Ulong, Float, Long, Long, Bool, Bool])
}

/// `EECPPersonnelRequest::Load` (from 0.222: with the deadline).
fn skip_request(f: &mut SaveFile, major: u32, minor: u32) -> i32 {
    // the units: unit id, class, unit id, B0C
    let r = skip_list(f, |f| skips(f, OK, &[Ulong, Ulong, Ulong, Bool]));
    // fill, id, continent, town, tile, start
    let mut r = skips(f, r, &[Float, Long, Ushort, Long]);
    if r == OK {
        r = skip_tile(f);
    }
    r = skips(f, r, &[Ulong]);
    if r == OK && at(major, minor, 0xde) {
        r = skip(f, Ulong); // the deadline
    }
    r
}

/// `EECPPersonnel::Load`.
fn skip_personnel(f: &mut SaveFile, major: u32, minor: u32) -> i32 {
    let mut r = skip_list(f, |f| skips(f, OK, &[Ulong, Ulong]));
    if r == OK {
        // the request's id, then the request
        r = skip_list(f, |f| {
            let r = skip(f, Long);
            if r == OK {
                skip_request(f, major, minor)
            } else {
                r
            }
        });
    }
    r = skips(f, r, &[Ulong]); // the next id
    if r == OK {
        r = skip_ids(f); // the deferred units
    }
    r
}

/// `EECPDefenseCommander::Load` (from 0.208).
fn skip_defense(f: &mut SaveFile, major: u32, minor: u32) -> i32 {
    let mut r = skips(f, OK, &[Ulong, Bool]); // next update time, enabled
    if r == OK {
        r = skip_list(f, |f| {
            let (r, valid) = f.read_bool(false);
            if r != OK || !valid {
                return r;
            }
            // an aid package (EECPMilitaryAidPackage::Load from 0.212; its result was not checked): the sub group
            // ids, the town's tile, target id, threat, town id, state, next update time, attacker id, next send time
            let mut r = skip_ids(f);
            if r == OK {
                r = skip_tile(f);
            }
            let r = skips(f, r, &[Ulong, Float, Long, Long]);
            skip_n(f, r, 3, Ulong)
        });
    }
    r = skips(f, r, &[Ulong]); // next aid time
    if r == OK {
        r = skip_ids(f); // sub group ids
    }
    if r == OK && at(major, minor, 0xd0) {
        r = skip(f, Ulong); // naval
    }
    if r == OK && at(major, minor, 0xed) {
        r = skip(f, Ulong); // space
    }
    if r == OK && at(major, minor, 0xf0) {
        r = skip(f, Ulong); // special
    }
    r
}

/// `EECPAttackSubGroupUnit::Load` (every unit kind's: no kind overrides it): its location, unit and target ids,
/// retarget time, importance, important, needs orders, out of fight, kind, F6C, aggression, courage, F68, escort id,
/// state, post attack mode.
fn skip_sub_group_unit(f: &mut SaveFile) -> i32 {
    let r = skip_location(f);
    let r = skips(f, r, &[Ulong, Ulong, Ulong, Ulong, Bool, Bool, Bool, Long]);
    let r = skip_n(f, r, 4, Float);
    skips(f, r, &[Ulong, Long, Long])
}

/// `EECPAttackSubGroup::Load`: the units (kind, unit; as the original, a unit's read error is not reported: the
/// reads after it fail instead), the path (tile, sector kind), the area, next waypoint, rally point, task, the
/// request id and the plain fields.
fn skip_sub_group(f: &mut SaveFile) -> i32 {
    let (r, count) = f.read_ulong(0);
    if r == OK {
        for _ in 0..count {
            if f.read_long(0).0 == OK {
                skip_sub_group_unit(f);
            }
        }
    }
    let r = skip_list(f, |f| {
        let r = skip_tile(f);
        if r == OK {
            skip(f, Long)
        } else {
            r
        }
    });
    let mut r = skip_n(f, r, 4, Long); // the area's edges
    if r == OK {
        r = skip_location(f);
    }
    if r == OK {
        r = skip_location(f);
    }
    if r == OK {
        r = skip_task(f);
    }
    let r = skips(f, r, &[Long, Long, Float]); // the request id, scan range, aggression
    let r = skip_n(f, r, 8, Ulong); // id, the times and counters (U1B0, U1B4, U1BC)
    let r = skips(f, r, &[Long, Ulong, Long]); // splits, state, kind
    let r = skip_n(f, r, 4, Bool); // B1E0-B1E2, out of fight
    skips(f, r, &[Long, Long]) // mission, split kind
}

/// `EECPASGLand::Load`'s own part (from 0.227): pickup and unload locations, transport id, transport stage, a flag,
/// the embark and landing tiles, the avoided tiles (tile, time).
fn skip_land_sub_group(f: &mut SaveFile) -> i32 {
    let mut r = skip_location(f);
    if r == OK {
        r = skip_location(f);
    }
    let mut r = skips(f, r, &[Ulong, Long, Bool]);
    if r == OK {
        r = skip_tile(f);
    }
    if r == OK {
        r = skip_tile(f);
    }
    if r == OK {
        r = skip_list(f, |f| {
            let r = skip_tile(f);
            if r == OK {
                skip(f, Ulong)
            } else {
                r
            }
        });
    }
    r
}

/// `EECPAttackGroup::Load`: the task, the sub groups by kind (EECPASGLand and the helicopter: the land part; the sea,
/// air, submarine, spaceship and space fighter: the base only; the three transports: the base, then the transport's
/// land id and cargo stage), the orders, id, continent, finished.
fn skip_attack_group(f: &mut SaveFile) -> i32 {
    let mut r = skip_task(f);
    if r == OK {
        r = skip_list(f, |f| {
            let (r, kind) = f.read_long(0);
            if r != OK {
                return r;
            }
            match kind {
                // land, helicopter
                0 | 5 | 8 => {
                    let r = skip_sub_group(f);
                    if r == OK {
                        skip_land_sub_group(f)
                    } else {
                        r
                    }
                }
                // sea, air, submarine, spaceship, space fighter
                1 | 7 | 2 | 6 | 9 | 11 => skip_sub_group(f),
                // the sea, air and space transports
                3 | 4 | 10 => {
                    let r = skip_sub_group(f);
                    skips(f, r, &[Ulong, Long])
                }
                // no sub group of that kind (the original dereferenced a null one)
                _ => FAILED,
            }
        });
    }
    if r == OK {
        // EECPAGOrder::Load: location, sub group ids, unit id, L28, type, state, active, split
        r = skip_list(f, |f| {
            let r = skip_location(f);
            let r = skip_n(f, r, 3, Ulong);
            skips(f, r, &[Long, Long, Ulong, Bool, Long])
        });
    }
    skips(f, r, &[Ulong, Ushort, Bool])
}

/// The epochs' lists of the build, research and unit managers: one per epoch of the retail data (15; before 0.234,
/// not the 15th).
fn skip_per_epoch(f: &mut SaveFile, major: u32, minor: u32, mut list: impl FnMut(&mut SaveFile) -> i32) -> i32 {
    let mut r = OK;
    let mut i = 0;
    while i < LEGACY_EPOCHS && r == OK {
        if !(!at(major, minor, 0xea) && i == 14) {
            r = list(f);
        }
        i += 1;
    }
    r
}

/// `EECPSkipLegacyCombatManager`: the personnel, the attack commander, the defence commander and the combat
/// manager's own fields (from 0.228; an older block fails).
pub fn skip_combat_manager(f: &mut SaveFile, major: u32, minor: u32) -> i32 {
    if !at(major, minor, 0xe4) {
        return FAILED;
    }
    let mut r = skip_personnel(f, major, minor);
    if r == OK {
        // the attack commander (EECPAttackCommander::Load): its groups, L28
        r = skip_list(f, skip_attack_group);
    }
    r = skips(f, r, &[Long]);
    if r == OK {
        r = skip_defense(f, major, minor);
    }
    // next group and sub group ids, was attacked, improvements enabled
    let mut r = skips(f, r, &[Ulong, Ulong, Bool, Bool]);
    if r == OK {
        r = skip_list(f, skip_task); // defend tasks
    }
    if r == OK {
        r = skip_list(f, skip_task); // attack tasks
    }
    let mut r = skips(f, r, &[Bool]); // wonder race, its attack times
    if r == OK {
        r = skip_ids(f);
    }
    if r == OK {
        // the tile counts: tile, count
        r = skip_list(f, |f| {
            let r = skip_tile(f);
            if r == OK {
                skip(f, Long)
            } else {
                r
            }
        });
    }
    r
}

/// `EECPSkipLegacyCivilizationManager`: the resource, build, research, unit and wall managers and the civilization
/// manager's own fields; `town_center` gets the build manager's town centre tile (x, y read in place: a failed
/// read leaves what the file left).
pub fn skip_civilization_manager(f: &mut SaveFile, major: u32, minor: u32, town_center: &mut [i32; 2]) -> i32 {
    // the resource manager: the drop site classes, needs, wanted gatherers, B180, flags and timers
    let mut r = OK;
    for _ in 0..7 {
        if r != OK {
            break;
        }
        r = skip_ids(f);
    }
    let r = skip_n(f, r, 7, Long);
    let r = skip_n(f, r, 7, Ulong);
    let r = skip_n(f, r, 7, Bool);
    let r = skips(f, r, &[Bool, Bool]);
    let mut r = skip_n(f, r, 4, Ulong);
    // the build manager: the queues (class, tile), the town centre tile, the next update time (from 0.220)
    if r == OK {
        r = skip_per_epoch(f, major, minor, |f| {
            skip_list(f, |f| {
                let r = skip(f, Ulong);
                if r == OK {
                    skip_tile(f)
                } else {
                    r
                }
            })
        });
    }
    if r == OK {
        let (rx, x) = f.read_long(town_center[0]);
        town_center[0] = x;
        r = rx;
        if r == OK {
            let (ry, y) = f.read_long(town_center[1]);
            town_center[1] = y;
            r = ry;
        }
    }
    if r == OK && at(major, minor, 0xdc) {
        r = skip(f, Ulong);
    }
    // the research manager: candidates, the epochs' ids, next time, epoch, enabled
    if r == OK {
        r = skip_list(f, |f| skip(f, Long));
    }
    if r == OK {
        r = skip_per_epoch(f, major, minor, |f| skip_list(f, |f| skip(f, Long)));
    }
    let mut r = skips(f, r, &[Ulong, Long, Bool]);
    // the unit manager: the epochs' requests (class, count, continent, town), the waiting counts, waiting, enabled
    if r == OK {
        r = skip_per_epoch(f, major, minor, |f| skip_list(f, |f| skips(f, OK, &[Ulong, Ulong, Ushort, Long])));
    }
    if r == OK {
        r = skip_list(f, |f| skips(f, OK, &[Ulong, Ulong]));
    }
    let r = skips(f, r, &[Long, Bool]);
    // the civilization manager's own: flags, the epoch advance and building count times, the garrison
    let r = skip_n(f, r, 4, Bool);
    let r = skips(f, r, &[Ulong, Bool, Ulong]);
    let r = skip_n(f, r, 3, Bool);
    let mut r = skip_n(f, r, 3, Ulong);
    // the wall manager: walls (unit, enemy), gates, the gate and wall tiles, F70, F74, L7C, planned, next time
    if r == OK {
        r = skip_list(f, |f| skips(f, OK, &[Ulong, Ulong]));
    }
    if r == OK {
        r = skip_ids(f);
    }
    for _ in 0..3 {
        if r != OK {
            break;
        }
        r = skip_tile(f);
    }
    // ... then the fortress, its time, morale buildings enabled
    skips(f, r, &[Float, Float, Ulong, Bool, Ulong, Ulong, Ulong, Bool])
}

/// `EECPSkipLegacyExplorationManager`: the explorers, (before 0.033) a word, B2C, enabled, the requests.
pub fn skip_exploration_manager(f: &mut SaveFile, major: u32, minor: u32) -> i32 {
    let mut r = skip_ids(f); // the explorers
    if r == OK && !at(major, minor, 0x21) {
        r = skip(f, Ulong);
    }
    let mut r = skips(f, r, &[Bool, Bool]); // B2C, enabled
    if r == OK {
        r = skip_ids(f); // requests
    }
    r
}

#[cfg(test)]
#[path = "legacy_load_tests.rs"]
mod tests;
