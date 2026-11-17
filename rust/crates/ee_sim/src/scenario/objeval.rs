//! `EESMObject::Evaluate`: a scenario object's units rebuilt (a dynamic object's every time, a static one's while it
//! has none: the parent's units of the owner's players, or the owner's players' units of the object's classes),
//! then kept or dropped by the state, attribute, selected-by, visible-by, area and nearness filters, shuffled and
//! capped. The units stay the C++ object's list, read and changed by operation (`EE_SCENARIO_READ_OBJ_*`,
//! `EE_SCENARIO_WRITE_OBJ_*`): a filter can evaluate another object, or this one again, in the middle of the loop,
//! as in the original. The object's numbers are read again at each use (the same reason).

#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::scenario::condition::choose_player_recorded;
use crate::scenario::fields::EeScenarioObjectFields;
use crate::scenario::host::*;
use crate::scenario::object::{accepts_class, EeScenarioObjectFilter};
use crate::scenario::placement::near_unit;

/// The context object's `mUnits.size()`.
pub const EE_SCENARIO_READ_OBJ_UNIT_COUNT: u32 = 53;
/// The context object's `mUnits[a]`.
pub const EE_SCENARIO_READ_OBJ_UNIT_AT: u32 = 54;
/// `GetScenarioObject(a)->Evaluate(b != 0)` (1/0).
pub const EE_SCENARIO_READ_OBJ_EVALUATE: u32 = 55;
/// Unit `a` (`gWorld.GetUnit`): 0 for none, else bit 0 set, bit 1 `GetHitPoints() > 0`, bit 2 its class's
/// `mIsTree`, bit 3 a complex unit, bit 4 the complex unit's `mPreviousPart`.
pub const EE_SCENARIO_READ_UNIT_FLAGS: u32 = 56;
/// Unit `a`'s `mUnitClass->mUnitType`.
pub const EE_SCENARIO_READ_UNIT_CLASS_TYPE: u32 = 57;
/// Unit `a`'s `mPlayer->GetPlayerID()`.
pub const EE_SCENARIO_READ_UNIT_PLAYER: u32 = 58;
/// Unit `a`'s complex unit's `GetCommandType()`.
pub const EE_SCENARIO_READ_UNIT_COMMAND: u32 = 59;
/// The context object's `GetUnitAttribute(b, c != 0, GetUnit(a), 0, 0)`.
pub const EE_SCENARIO_READ_UNIT_ATTRIBUTE: u32 = 60;
/// Player `a`'s `mActiveGroup->Contains(unit b's complex unit) || mViewedUnit == unit b` (1/0).
pub const EE_SCENARIO_READ_UNIT_SELECTED_BY: u32 = 61;
/// `EEIsSeenBy(unit a, player b, false)` (1/0).
pub const EE_SCENARIO_READ_UNIT_SEEN_BY: u32 = 62;
/// `GetArea(a)`: -1 without the area, else its `ContainsUnit(unit b)` (1/0).
pub const EE_SCENARIO_READ_AREA_CONTAINS_UNIT: u32 = 63;
/// Player `a`'s first unit (`mFirstUnit`, the context's cursor set to it), the next (`mNext`) when `b` is 1: its id
/// in the low 32 bits and its `mUnitClass->mUnitType` in the high, -1 at the end.
pub const EE_SCENARIO_READ_PLAYER_UNIT: u32 = 65;

/// The context object's `mUnits.clear()`.
pub const EE_SCENARIO_WRITE_OBJ_CLEAR: u32 = 11;
/// `GetScenarioObject(b)->GetUnits(mUnits)` (the context object's list replaced by the parent's).
pub const EE_SCENARIO_WRITE_OBJ_COPY_PARENT: u32 = 12;
/// `mUnits.erase(begin() + b)`.
pub const EE_SCENARIO_WRITE_OBJ_ERASE: u32 = 13;
/// `mUnits.push_back(b)`.
pub const EE_SCENARIO_WRITE_OBJ_PUSH: u32 = 14;
/// `EEAIRandomShuffle(mUnits.begin(), mUnits.end())` (the physics stream's draws).
pub const EE_SCENARIO_WRITE_OBJ_SHUFFLE: u32 = 15;
/// `mUnits.erase(begin() + b, end())`.
pub const EE_SCENARIO_WRITE_OBJ_TRUNCATE: u32 = 16;

/// `kActionResourcesRotting`, `kActionUnitRotting`, `kActionDead`, `kActionResourcesDepleting`: states whose units
/// are dead (the dead are included).
const EE_SCENARIO_DEAD_STATES: [c_ulong; 4] = [6001, 6002, 6003, 6004];

/// The object being evaluated: its numbers (a fresh copy at each call) and its `mIsSelectOnMap`.
pub trait ObjectHost: Host {
    fn fields(&mut self) -> EeScenarioObjectFields;
    fn set_select_on_map(&mut self);
}

fn count<H: ObjectHost>(h: &mut H) -> i64 {
    h.read(EE_SCENARIO_READ_OBJ_UNIT_COUNT, 0, 0)
}

fn unit_at<H: ObjectHost>(h: &mut H, i: i64) -> i32 {
    h.read(EE_SCENARIO_READ_OBJ_UNIT_AT, i as i32, 0) as i32
}

fn obj_write<H: ObjectHost>(h: &mut H, op: u32, b: i64) {
    h.write(op, 0, b, 0, 0);
}

fn filter<H: ObjectHost>(h: &mut H) -> EeScenarioObjectFilter {
    EeScenarioObjectFilter::from(&h.fields())
}

/// `ChoosePlayer(the object's choice read by pick, &list, false)`.
fn players<H: ObjectHost>(h: &mut H, pick: fn(&EeScenarioObjectFields) -> c_ulong) -> Vec<i32> {
    let kind = pick(&h.fields());
    choose_player_recorded(h, kind as i32, false, true).1
}

/// `EESMObject::Evaluate(include_dead)`: whether a unit passes.
pub fn evaluate<H: ObjectHost>(h: &mut H, include_dead: bool) -> bool {
    // the parent, the nearness target and the area as the original resolved them on entry
    let f = h.fields();
    let (parent, target, area) = (f.mParent as i32, f.mNearObject as i32, f.mArea as i32);
    let include = include_dead || EE_SCENARIO_DEAD_STATES.contains(&f.mState);
    let mut rebuilt = false;
    if !h.fields().mIsSelectOnMap {
        if h.fields().mIsDynamic {
            obj_write(h, EE_SCENARIO_WRITE_OBJ_CLEAR, 0);
        }
        if count(h) == 0 {
            rebuilt = true;
            if h.fields().mParent != 0 {
                h.read(EE_SCENARIO_READ_OBJ_EVALUATE, parent, include as i32);
                obj_write(h, EE_SCENARIO_WRITE_OBJ_COPY_PARENT, parent as i64);
                let owners = players(h, |f| f.mOwner);
                let mut i: i64 = 0;
                while i < count(h) {
                    let id = unit_at(h, i);
                    let mut keep = h.read(EE_SCENARIO_READ_UNIT_FLAGS, id, 0) != 0;
                    if keep {
                        let class = h.read(EE_SCENARIO_READ_UNIT_CLASS_TYPE, id, 0) as u32;
                        let o = filter(h);
                        keep = accepts_class(h, &o, class);
                    }
                    if keep {
                        let player = h.read(EE_SCENARIO_READ_UNIT_PLAYER, id, 0) as i32;
                        keep = owners.contains(&player);
                    }
                    if !keep {
                        obj_write(h, EE_SCENARIO_WRITE_OBJ_ERASE, i);
                        i -= 1;
                    }
                    i += 1;
                }
            } else {
                for p in players(h, |f| f.mOwner) {
                    let mut unit = h.read(EE_SCENARIO_READ_PLAYER_UNIT, p, 0);
                    while unit != -1 {
                        let (id, class) = (unit as u32, (unit >> 32) as u32);
                        let o = filter(h);
                        if accepts_class(h, &o, class) {
                            obj_write(h, EE_SCENARIO_WRITE_OBJ_PUSH, id as i64);
                        }
                        unit = h.read(EE_SCENARIO_READ_PLAYER_UNIT, p, 1);
                    }
                }
            }
            if (count(h) as u64) < h.fields().mMinCount as u64 {
                obj_write(h, EE_SCENARIO_WRITE_OBJ_CLEAR, 0);
            }
        }
    }
    let mut result = false;
    let mut i: i64 = 0;
    while i < count(h) {
        let id = unit_at(h, i);
        let flags = h.read(EE_SCENARIO_READ_UNIT_FLAGS, id, 0);
        let (exists, alive, tree, complex, part) =
            (flags & 1 != 0, flags & 2 != 0, flags & 4 != 0, flags & 8 != 0, flags & 16 != 0);
        let mut keep = false;
        if exists && (include || alive || tree) && (!complex || !part) {
            let f = h.fields();
            if f.mHasStateCheck == 0 {
                keep = true;
            } else if complex {
                let command = h.read(EE_SCENARIO_READ_UNIT_COMMAND, id, 0) as c_long as c_ulong;
                keep = if f.mHasStateCheck == 1 { command == f.mState } else { command != f.mState };
            }
            if keep && h.fields().mHasAttributeCheck != 0 {
                let f = h.fields();
                let value =
                    h.read4(EE_SCENARIO_READ_UNIT_ATTRIBUTE, id, f.mAttribute as i32, f.mIsAttributePercent as i32, 0)
                        as c_long;
                let f = h.fields();
                let (min, max) = (f.mAttributeMin as c_long, f.mAttributeMax as c_long);
                keep =
                    if f.mHasAttributeCheck == 1 { value >= min && value <= max } else { value < min || value > max };
            }
            if h.fields().mSelectedByCheck != 0 {
                for p in players(h, |f| f.mSelectedByPlayer) {
                    let selected = |h: &mut H| h.read(EE_SCENARIO_READ_UNIT_SELECTED_BY, p, id) != 0;
                    keep = if h.fields().mSelectedByCheck == 1 { keep && selected(h) } else { keep && !selected(h) };
                }
            }
            if keep && h.fields().mVisibleByCheck != 0 {
                for p in players(h, |f| f.mVisibleByPlayer) {
                    let seen = |h: &mut H| h.read(EE_SCENARIO_READ_UNIT_SEEN_BY, id, p) != 0;
                    keep = if h.fields().mVisibleByCheck == 1 { keep && seen(h) } else { keep && !seen(h) };
                }
            }
            if keep && h.fields().mInAreaCheck != 0 {
                let inside = h.read(EE_SCENARIO_READ_AREA_CONTAINS_UNIT, area, id);
                keep = if h.fields().mInAreaCheck == 1 { inside == 1 } else { inside == 0 };
            }
            if keep && h.fields().mNearCheck != 0 {
                let mode = h.fields().mNearMode as i32;
                // the target's IsNearUnit (placement.rs), -1 without the target
                let near = if h.read(EE_SCENARIO_READ_OBJECT_EXISTS, target, 0) == 0 {
                    -1
                } else {
                    near_unit(h, target, id, mode, false) as i64
                };
                keep = if h.fields().mNearCheck == 1 { near == 1 } else { near == 0 };
            }
        }
        if !keep && rebuilt {
            obj_write(h, EE_SCENARIO_WRITE_OBJ_ERASE, i);
            i -= 1;
        } else {
            result = result || keep;
            if result && !rebuilt {
                break;
            }
        }
        i += 1;
    }
    if rebuilt {
        if (count(h) as u64) < h.fields().mMinCount as u64 {
            obj_write(h, EE_SCENARIO_WRITE_OBJ_CLEAR, 0);
        } else {
            obj_write(h, EE_SCENARIO_WRITE_OBJ_SHUFFLE, 0);
            let max = h.fields().mMaxCount;
            if count(h) as u64 > max as u64 {
                obj_write(h, EE_SCENARIO_WRITE_OBJ_TRUNCATE, max as i64);
            }
        }
        result = count(h) > 0;
        if result && !h.fields().mIsDynamic {
            h.set_select_on_map();
        }
    }
    result
}

/// The C ABI's object host: the reads and writes, and the object's numbers.
pub struct ObjectTable {
    pub host: EeScenarioHost,
    pub fields: *mut EeScenarioObjectFields,
}

impl Host for ObjectTable {
    fn read4(&mut self, op: u32, a: i32, b: i32, c: i32, d: i32) -> i64 {
        self.host.read4(op, a, b, c, d)
    }
    fn players(&mut self, kind: i32, single: bool, result: i32, players: Option<&[i32]>, seed: u32) {
        self.host.players(kind, single, result, players, seed)
    }
    fn write(&mut self, op: u32, player: c_long, b: i64, c: i64, d: i64) {
        self.host.write(op, player, b, c, d)
    }
    fn tiles(&mut self, op: u32, a: i32, b: i32, c: i32) -> Vec<(i32, i32)> {
        self.host.tiles(op, a, b, c)
    }
}

impl ObjectHost for ObjectTable {
    fn fields(&mut self) -> EeScenarioObjectFields {
        // SAFETY: the table's contract: `fields` is the live object's, read between callbacks
        unsafe { *self.fields }
    }
    fn set_select_on_map(&mut self) {
        // SAFETY: as `fields`
        unsafe { (*self.fields).mIsSelectOnMap = true };
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::scenario::placement::*;

    /// Units by id: (flags, class type, player, command, attribute); players' unit chains; the object's list; a
    /// log of the parent's evaluations and the shuffles.
    struct World {
        f: EeScenarioObjectFields,
        units: Vec<(i64, u32, i32, i64, i64)>,
        chains: Vec<Vec<i32>>,
        list: Vec<i32>,
        parent: Vec<i32>,
        cursor: (usize, usize),
        selected: Vec<(i32, i32)>,
        log: Vec<String>,
    }

    fn world(f: EeScenarioObjectFields) -> World {
        World {
            f,
            units: vec![
                (1 | 2 | 8, 0, 1, 7, 50),     // 0: alive, complex, player 1, command 7, attribute 50
                (1 | 2, 0, 2, 0, 10),         // 1: alive, no complex, player 2
                (1, 0, 1, 0, 0),              // 2: dead
                (1 | 4, 0, 1, 0, 0),          // 3: a tree (dead but kept)
                (1 | 2 | 8 | 16, 0, 1, 7, 0), // 4: a later part of a complex unit
                (0, 0, 0, 0, 0),              // 5: gone
            ],
            chains: vec![vec![], vec![0, 2, 3, 4], vec![1]],
            list: vec![],
            parent: vec![0, 1, 2, 3, 5],
            cursor: (0, 0),
            selected: vec![],
            log: vec![],
        }
    }

    impl Host for World {
        fn read4(&mut self, op: u32, a: i32, b: i32, c: i32, _d: i32) -> i64 {
            let u = self.units.get(a.max(0) as usize).copied().unwrap_or((0, 0, 0, 0, 0));
            match op {
                EE_SCENARIO_READ_NUM_PLAYERS => 3,
                EE_SCENARIO_READ_PLAYER_EXISTS => 1,
                EE_SCENARIO_READ_PLAYER_HUMAN => (a == 1) as i64,
                EE_SCENARIO_READ_PLAYER_COMPUTER => (a == 2) as i64,
                EE_SCENARIO_READ_CAPTURE_SEED => -1,
                EE_SCENARIO_READ_CLASS_SELECTABLE | EE_SCENARIO_READ_CLASS_EXISTS => 1,
                EE_SCENARIO_READ_CLASS_CATEGORY | EE_SCENARIO_READ_CATEGORY_IN_SET => 0,
                EE_SCENARIO_READ_OBJ_UNIT_COUNT => self.list.len() as i64,
                EE_SCENARIO_READ_OBJ_UNIT_AT => self.list[a as usize] as i64,
                EE_SCENARIO_READ_OBJ_EVALUATE => {
                    self.log.push(format!("parent {a} {b}"));
                    1
                }
                EE_SCENARIO_READ_UNIT_FLAGS => u.0,
                EE_SCENARIO_READ_UNIT_CLASS_TYPE => u.1 as i64,
                EE_SCENARIO_READ_UNIT_PLAYER => u.2 as i64,
                EE_SCENARIO_READ_UNIT_COMMAND => u.3,
                EE_SCENARIO_READ_UNIT_ATTRIBUTE => {
                    assert_eq!((b, c), (self.f.mAttribute as i32, self.f.mIsAttributePercent as i32));
                    u.4
                }
                EE_SCENARIO_READ_UNIT_SELECTED_BY => self.selected.contains(&(a, b)) as i64,
                EE_SCENARIO_READ_UNIT_SEEN_BY => (b != 1) as i64,
                EE_SCENARIO_READ_AREA_CONTAINS_UNIT => {
                    if a == 9 {
                        -1
                    } else {
                        (b == 0) as i64
                    }
                }
                // the target object's one unit 1 at (5, 1), unit 3 at (5, 0), the rest further; sight 1.5
                EE_SCENARIO_READ_OBJECT_EXISTS => 1,
                EE_SCENARIO_READ_OBJECT_UNIT_COUNT => 1,
                EE_SCENARIO_READ_OBJECT_UNIT_AT => 1,
                EE_SCENARIO_READ_UNIT_TILE_OF | EE_SCENARIO_READ_UNIT_NEAREST_TILE => match a {
                    0 => 0,
                    1 => 5 | 1 << 32,
                    3 => 5,
                    _ => 20 | 20 << 32,
                },
                EE_SCENARIO_READ_UNIT_LINE_OF_SIGHT => 1.5f32.to_bits() as i64,
                EE_SCENARIO_READ_PLAYER_UNIT => {
                    if b == 0 {
                        self.cursor = (a as usize, 0);
                    } else {
                        self.cursor.1 += 1;
                    }
                    match self.chains[self.cursor.0].get(self.cursor.1) {
                        Some(&id) => id as i64 | (self.units[id as usize].1 as i64) << 32,
                        None => -1,
                    }
                }
                _ => panic!("read {op}"),
            }
        }
        fn write(&mut self, op: u32, _player: c_long, b: i64, _c: i64, _d: i64) {
            match op {
                EE_SCENARIO_WRITE_OBJ_CLEAR => self.list.clear(),
                EE_SCENARIO_WRITE_OBJ_COPY_PARENT => self.list = self.parent.clone(),
                EE_SCENARIO_WRITE_OBJ_ERASE => {
                    self.list.remove(b as usize);
                }
                EE_SCENARIO_WRITE_OBJ_PUSH => self.list.push(b as i32),
                EE_SCENARIO_WRITE_OBJ_SHUFFLE => {
                    self.list.reverse();
                    self.log.push("shuffle".into());
                }
                EE_SCENARIO_WRITE_OBJ_TRUNCATE => self.list.truncate(b as usize),
                _ => panic!("write {op}"),
            }
        }
    }

    impl ObjectHost for World {
        fn fields(&mut self) -> EeScenarioObjectFields {
            self.f
        }
        fn set_select_on_map(&mut self) {
            self.f.mIsSelectOnMap = true;
        }
    }

    fn fields() -> EeScenarioObjectFields {
        // owner 3: every human (player 1)
        EeScenarioObjectFields { mOwner: 3, mMaxCount: 100, ..Default::default() }
    }

    #[test]
    fn a_static_object_collects_its_owners_units_once_and_then_picks_on_the_map() {
        let mut w = world(fields());
        assert!(evaluate(&mut w, false));
        // the dead unit 2 and the later part 4 dropped; the tree kept; shuffled (reversed here)
        assert_eq!(w.list, vec![3, 0]);
        assert!(w.f.mIsSelectOnMap);
        assert_eq!(w.log, ["shuffle"]);
        // picked on the map: no rebuild, the first passing unit ends the loop
        w.log.clear();
        assert!(evaluate(&mut w, false));
        assert_eq!(w.list, vec![3, 0]);
        assert!(w.log.is_empty());
        // the dead included
        let mut w = world(fields());
        assert!(evaluate(&mut w, true));
        assert_eq!(w.list, vec![3, 2, 0]);
    }

    #[test]
    fn min_and_max_counts_and_a_dynamic_object() {
        let mut w = world(EeScenarioObjectFields { mMinCount: 3, ..fields() });
        assert!(!evaluate(&mut w, false));
        assert!(w.list.is_empty());
        assert!(!w.f.mIsSelectOnMap);
        let mut w = world(EeScenarioObjectFields { mMaxCount: 1, mIsDynamic: true, ..fields() });
        w.list = vec![1];
        assert!(evaluate(&mut w, false));
        assert_eq!(w.list, vec![3]);
        assert!(!w.f.mIsSelectOnMap);
    }

    #[test]
    fn the_parents_units_of_the_owners_players() {
        // owner 4: every computer (player 2)
        let mut w = world(EeScenarioObjectFields { mParent: 6, mOwner: 4, ..fields() });
        assert!(evaluate(&mut w, false));
        assert_eq!(w.log[0], "parent 6 0");
        assert_eq!(w.list, vec![1]);
    }

    #[test]
    fn the_filters() {
        // the state: units with command 7 (only complex units have one)
        let mut w = world(EeScenarioObjectFields { mHasStateCheck: 1, mState: 7, ..fields() });
        assert!(evaluate(&mut w, false));
        assert_eq!(w.list, vec![0]);
        let mut w = world(EeScenarioObjectFields { mHasStateCheck: 2, mState: 7, ..fields() });
        assert!(!evaluate(&mut w, false));
        // the attribute in [20, 60], then outside it
        let mut w = world(EeScenarioObjectFields {
            mHasAttributeCheck: 1,
            mAttribute: 4,
            mAttributeMin: 20,
            mAttributeMax: 60,
            ..fields()
        });
        assert!(evaluate(&mut w, false));
        assert_eq!(w.list, vec![0]);
        let mut w =
            world(EeScenarioObjectFields { mHasAttributeCheck: 2, mAttributeMin: 20, mAttributeMax: 60, ..fields() });
        assert!(evaluate(&mut w, false));
        assert_eq!(w.list, vec![3]);
        // selected by every human (player 1): only unit 3
        let mut w = world(EeScenarioObjectFields { mSelectedByCheck: 1, mSelectedByPlayer: 3, ..fields() });
        w.selected = vec![(1, 3)];
        assert!(evaluate(&mut w, false));
        assert_eq!(w.list, vec![3]);
        // visible by player 2 (7 = player 1 is unseen by all: 8 = player 2 sees every unit)
        let mut w = world(EeScenarioObjectFields { mVisibleByCheck: 1, mVisibleByPlayer: 8, ..fields() });
        assert!(evaluate(&mut w, false));
        assert_eq!(w.list, vec![3, 0]);
        let mut w = world(EeScenarioObjectFields { mVisibleByCheck: 1, mVisibleByPlayer: 7, ..fields() });
        assert!(!evaluate(&mut w, false));
        // in the area (unit 0 only); without the area nothing passes either way
        let mut w = world(EeScenarioObjectFields { mInAreaCheck: 1, mArea: 2, ..fields() });
        assert!(evaluate(&mut w, false));
        assert_eq!(w.list, vec![0]);
        let mut w = world(EeScenarioObjectFields { mInAreaCheck: 2, mArea: 9, ..fields() });
        assert!(!evaluate(&mut w, false));
        // near the target (unit 3 only), not near it
        let mut w = world(EeScenarioObjectFields { mNearCheck: 1, mNearObject: 1, ..fields() });
        assert!(evaluate(&mut w, false));
        assert_eq!(w.list, vec![3]);
        let mut w = world(EeScenarioObjectFields { mNearCheck: 2, mNearObject: 1, ..fields() });
        assert!(evaluate(&mut w, false));
        assert_eq!(w.list, vec![0]);
    }
}
