use super::*;
use std::collections::BTreeMap;

/// A world of one player, its unit class and upgrade, the effect table and the units (fake handles), recording
/// every statement.
struct Fake {
    effects: Vec<EeScenarioEvEffect>,
    player: Box<EeUnitsPlayerFields>,
    class: Box<EeUnitsUnitClassFields>,
    upgrade: Box<EeUnitsUnitClassUpgradeFields>,
    events: Vec<Vec<i32>>,
    /// answers by op (default 0)
    answers: BTreeMap<u32, i64>,
    /// unit handle -> (hit points, complex, next)
    units: BTreeMap<i64, (i64, bool, i64)>,
    capture: bool,
    log: Vec<(u32, [i64; 4])>,
    records: Vec<Record>,
    steps: Vec<(c_ulong, bool, c_long)>,
    walks: Vec<[c_long; 5]>,
    rescales: Vec<(c_long, c_ulong, c_long)>,
    of_type: Vec<u64>,
}

const PLAYER: i64 = 0x1000;
const CLASS: i64 = 0x2000;
const UPGRADE: i64 = 0x3000;

impl Fake {
    fn new() -> Fake {
        let class = EeUnitsUnitClassFields {
            mUnitType: 7,
            mAttack: 10,
            mHitPoints: 100,
            mTechnology: 55,
            ..Default::default()
        };
        let upgrade = EeUnitsUnitClassUpgradeFields { mAttack: 12.0, mMaxHitPoints: 100, ..Default::default() };
        Fake {
            effects: Vec::new(),
            // SAFETY: a plain record (zero pointers are null)
            player: Box::new(unsafe { std::mem::zeroed() }),
            class: Box::new(class),
            upgrade: Box::new(upgrade),
            events: Vec::new(),
            answers: BTreeMap::new(),
            units: BTreeMap::new(),
            capture: false,
            log: Vec::new(),
            records: Vec::new(),
            steps: Vec::new(),
            walks: Vec::new(),
            rescales: Vec::new(),
            of_type: Vec::new(),
        }
    }
    fn ops(&self) -> Vec<u32> {
        self.log.iter().map(|(o, _)| *o).collect()
    }
    fn has(&self, op: u32) -> Vec<[i64; 4]> {
        self.log.iter().filter(|(o, _)| *o == op).map(|(_, a)| *a).collect()
    }
}

impl EvHost for Fake {
    fn op(&mut self, op: u32, a: [i64; 4]) -> i64 {
        self.log.push((op, a));
        match op {
            EE_SCENARIO_EV_EFFECTS_DATA => self.effects.as_ptr() as usize as i64,
            EE_SCENARIO_EV_EFFECTS_COUNT => self.effects.len() as i64,
            EE_SCENARIO_EV_PLAYER | EE_SCENARIO_EV_CURRENT_PLAYER => PLAYER,
            EE_SCENARIO_EV_PLAYER_ID => 2,
            EE_SCENARIO_EV_PLAYER_FIELDS => &mut *self.player as *mut _ as usize as i64,
            EE_SCENARIO_EV_CLASS => CLASS,
            EE_SCENARIO_EV_CLASS_FIELDS => &mut *self.class as *mut _ as usize as i64,
            EE_SCENARIO_EV_UPGRADE | EE_SCENARIO_EV_GET_UPGRADE => UPGRADE,
            EE_SCENARIO_EV_UPGRADE_FIELDS => &mut *self.upgrade as *mut _ as usize as i64,
            EE_SCENARIO_EV_UPGRADE_CLASS => CLASS,
            EE_SCENARIO_EV_CLASS_COUNT => 10,
            EE_SCENARIO_EV_FIRST_UNIT => self.units.keys().next().copied().unwrap_or(0),
            EE_SCENARIO_EV_NEXT_UNIT => self.units[&a[0]].2,
            EE_SCENARIO_EV_UNIT_TYPE => 7,
            EE_SCENARIO_EV_UNIT_HIT_POINTS => self.units[&a[0]].0,
            EE_SCENARIO_EV_UNIT_COMPLEX => {
                if self.units[&a[0]].1 {
                    a[0] + 1
                } else {
                    0
                }
            }
            EE_SCENARIO_EV_COMPLEX_BUILT => 1,
            EE_SCENARIO_EV_UNIT_ON_TILE => 1,
            EE_SCENARIO_EV_UNIT_ID => a[0] & 0xff,
            EE_SCENARIO_EV_COMPLEX_AS_UNIT => a[0],
            EE_SCENARIO_EV_EVENT_COUNT => self.events.len() as i64,
            EE_SCENARIO_EV_EVENT_AT => a[0] + 1,
            EE_SCENARIO_EV_EVENT_EFFECTS => self.events[(a[0] - 1) as usize].as_ptr() as usize as i64,
            EE_SCENARIO_EV_EVENT_NUM => {
                self.answers.get(&op).copied().unwrap_or_else(|| self.events[(a[0] - 1) as usize].len() as i64)
            }
            _ => self.answers.get(&op).copied().unwrap_or(0),
        }
    }
    fn capturing(&mut self) -> bool {
        self.capture
    }
    fn commit(&mut self, record: &Record) {
        self.records.push(record.clone());
    }
    fn unit_step(&mut self, id: c_ulong, _t: c_long, _hp: c_long, _f: u32, _fl: c_long, applies: bool, value: c_long) {
        self.steps.push((id, applies, value));
    }
    fn unit_walk(&mut self, walk: c_long, effect: c_long, unit_type: c_long, a: c_long, b: c_long) {
        self.walks.push([walk, effect, unit_type, a, b]);
    }
    fn rescale(&mut self, what: c_long, unit: c_ulong, value: c_long, _old: c_ulong, _new: c_long, _amount: f32) {
        self.rescales.push((what, unit, value));
    }
    fn units_of_type(&mut self, _unit_type: i32, _index: i32) -> Vec<u64> {
        self.of_type.clone()
    }
}

fn effect(kind: i32, attribute: i32) -> EeScenarioEvEffect {
    EeScenarioEvEffect { mKind: kind, mAttribute: attribute, mUnitClass: 3, mUnitSet: -1, ..Default::default() }
}

#[test]
fn the_effect_record_is_the_dbeffects_row() {
    assert_eq!(std::mem::size_of::<EeScenarioEvEffect>(), 88);
    assert_eq!(std::mem::offset_of!(EeScenarioEvEffect, mRunOnLoad), 0x44);
}

#[test]
fn the_upgrade_block_zeroes_the_media_counts() {
    let f = EeUnitsUnitClassUpgradeFields { mMediaCount: 5, mButtonLoadCount: 6, mAttack: 2.0, ..Default::default() };
    let b = upgrade_bytes(&f);
    let size = std::mem::offset_of!(EeUnitsUnitClassUpgradeFields, mIsInitialized);
    assert_eq!(b.len(), size + 4);
    assert_eq!(u32::from_le_bytes(b[0..4].try_into().unwrap()) as usize, size);
    let media = std::mem::offset_of!(EeUnitsUnitClassUpgradeFields, mMediaCount) + 4;
    assert!(b[media..media + 2 * std::mem::size_of::<c_long>()].iter().all(|&x| x == 0));
    let attack = std::mem::offset_of!(EeUnitsUnitClassUpgradeFields, mAttack) + 4;
    assert_eq!(&b[attack..attack + 4], &2.0f32.to_le_bytes());
}

#[test]
fn an_attack_effect_changes_the_upgrade_and_records_evat_then_evrn() {
    let mut h = Fake::new();
    h.effects = vec![effect(9, 0), EeScenarioEvEffect { mOffset: 3.0, ..effect(2, 1) }];
    h.events = vec![vec![1]];
    h.capture = true;
    assert_eq!(trigger(&mut h, 0, 4, 77, 0, 0), 0);
    assert_eq!(h.upgrade.mAttack, 15.0); // the upgrade's 12 plus the offset
    let tags: Vec<u32> = h.records.iter().map(|r| r.tag).collect();
    assert_eq!(tags, vec![tag(b"EVAT"), tag(b"EVAT"), tag(b"EVRN")]);
    // the class record: 1, effect 1, type 7, the block before and after, the result 0 between them
    let class = &h.records[0].data;
    assert_eq!(&class[0..12], &[1, 0, 0, 0, 1, 0, 0, 0, 7, 0, 0, 0]);
    // the call record: 0, effect 1, attribute 1, the player index 4, the player block (id 2, bonus 0, fee 0, flag
    // 0), the class count, the result, the player block again
    let call = &h.records[1].data;
    let words: Vec<i32> = call.chunks(4).map(|c| i32::from_le_bytes(c.try_into().unwrap())).collect();
    assert_eq!(words, vec![0, 1, 1, 4, 2, 0, 0, 0, 1, 0, 2, 0, 0, 0]);
    let run: Vec<i32> = h.records[2].data.chunks(4).map(|c| i32::from_le_bytes(c.try_into().unwrap())).collect();
    assert_eq!(run, vec![1, 2, 1, 4, 77, -1, 0, 0]);
}

#[test]
fn without_capture_no_record_and_the_player_attributes_apply_once() {
    let mut h = Fake::new();
    h.effects = vec![EeScenarioEvEffect { mFactor: 0.25, ..effect(2, 38) }];
    h.events = vec![vec![0]];
    trigger(&mut h, 0, 1, 0, PLAYER, 0);
    assert_eq!(h.player.mTributeFee, 0.25);
    assert!(h.records.is_empty());
    assert!(h.has(EE_SCENARIO_EV_UPGRADE).is_empty());
}

#[test]
fn a_bad_event_or_effect_runs_nothing() {
    let mut h = Fake::new();
    h.events = vec![vec![5]];
    assert_eq!(trigger(&mut h, -1, 0, 0, 0, 0), 0);
    assert_eq!(trigger(&mut h, 3, 0, 0, 0, 0), 0);
    assert_eq!(trigger(&mut h, 0, 0, 0, 0, 0), 9); // effect 5 is not in the table
}

#[test]
fn max_hit_points_rescale_the_units_then_the_pending_ones() {
    let mut h = Fake::new();
    h.effects = vec![EeScenarioEvEffect { mOffset: 100.0, ..effect(2, 6) }];
    h.events = vec![vec![0]];
    h.answers.insert(EE_SCENARIO_EV_VARIANT_FACTOR, 1.0f32.to_bits() as i64);
    h.units.insert(0x10, (50, true, 0));
    h.capture = true;
    trigger(&mut h, 0, 1, 0, PLAYER, 0);
    assert_eq!(h.upgrade.mMaxHitPoints, 200);
    assert_eq!(h.has(EE_SCENARIO_EV_SET_HIT_POINTS), vec![[0x10, 100, 0, 0]]);
    assert_eq!(h.rescales, vec![(0, 0x10, 50)]);
    assert_eq!(h.walks.iter().map(|w| w[0]).collect::<Vec<_>>(), vec![2, 3]);
    assert_eq!(h.walks[0][3..], [100, 200]);
}

#[test]
fn line_of_sight_turns_sight_off_changes_it_and_on_again() {
    let mut h = Fake::new();
    h.effects = vec![EeScenarioEvEffect { mValue: 9.0, ..effect(2, 4) }];
    h.events = vec![vec![0]];
    h.units.insert(0x10, (50, false, 0));
    trigger(&mut h, 0, 1, 0, PLAYER, 0);
    let ops: Vec<u32> =
        h.ops().into_iter().filter(|&o| o == EE_SCENARIO_EV_SIGHT_OFF || o == EE_SCENARIO_EV_SIGHT_ON).collect();
    assert_eq!(ops, vec![EE_SCENARIO_EV_SIGHT_OFF, EE_SCENARIO_EV_SIGHT_ON]);
    assert_eq!(h.upgrade.mLineOfSight, 9.0);
}

#[test]
fn terrain_changes_the_mask_and_walks_the_set() {
    let mut h = Fake::new();
    h.effects = vec![EeScenarioEvEffect { mValue: 1.0, mTerrainType: 3, mUnitSet: 4, ..effect(2, 42) }];
    h.events = vec![vec![0]];
    h.answers.insert(EE_SCENARIO_EV_UNIT_SET, 0x77);
    trigger(&mut h, 0, 1, 0, PLAYER, 0);
    assert_eq!(h.upgrade.mTerrainMask as u32, effects::terrain_mask(0, 3, true));
    assert_eq!(h.has(EE_SCENARIO_EV_PATHFINDING), vec![[0x77, PLAYER, 3, 1]]);
    h.upgrade.mTerrainMask = 0;
    assert!(pathfinding(&mut h, PLAYER, 3, true, 2));
    assert_eq!(h.upgrade.mTerrainMask as u32, effects::terrain_mask(0, 3, true));
    h.upgrade.mTerrainMask = 0;
    assert!(pathfinding(&mut h, PLAYER, 3, true, 12)); // past the class table
    assert_eq!(h.upgrade.mTerrainMask, 0);
}

#[test]
fn the_button_refreshes_the_local_players_groups_of_the_class() {
    let mut h = Fake::new();
    h.effects = vec![EeScenarioEvEffect { mButton: 4, mTechnology: 9, ..effect(1, 0) }];
    h.events = vec![vec![0]];
    h.answers.insert(EE_SCENARIO_EV_TREE_NODE, 0x50);
    h.answers.insert(EE_SCENARIO_EV_BUTTON, 0x60);
    h.answers.insert(EE_SCENARIO_EV_GROUP_FIRST, 0x10);
    h.answers.insert(EE_SCENARIO_EV_UNIT_CLASS, CLASS);
    h.answers.insert(EE_SCENARIO_EV_UI_EXISTS, 1);
    assert_eq!(trigger(&mut h, 0, 1, 0, PLAYER, 0), 0);
    assert_eq!(h.has(EE_SCENARIO_EV_NODE_SET_BUTTON), vec![[0x50, 4, 0, 0]]);
    assert_eq!(h.has(EE_SCENARIO_EV_SET_CLASS_BUTTON), vec![[PLAYER, 7, 0x60, 0]]);
    assert_eq!(h.has(EE_SCENARIO_EV_POST_REFRESH_GROUP).len(), 20);
    h.answers.remove(&EE_SCENARIO_EV_BUTTON);
    assert_eq!(trigger(&mut h, 0, 1, 0, PLAYER, 0), 9);
}

#[test]
fn a_conversion_of_every_unit_copies_the_levels_and_replaces_them() {
    let mut h = Fake::new();
    h.effects = vec![EeScenarioEvEffect { mNewUnitClass: 5, ..effect(19, 0) }];
    h.events = vec![vec![0]];
    h.answers.insert(EE_SCENARIO_EV_EVENTS_ENABLED, 1);
    h.answers.insert(EE_SCENARIO_EV_REPLACE_UNIT, 0x99);
    h.units.insert(0x10, (50, true, 0));
    h.of_type = vec![0x10];
    assert_eq!(trigger(&mut h, 0, 1, 0, 0, 0), 0);
    assert_eq!(h.has(EE_SCENARIO_EV_COPY_LEVELS), vec![[UPGRADE, UPGRADE, 0, 0]]);
    assert_eq!(h.has(EE_SCENARIO_EV_REPLACE_QUEUED), vec![[PLAYER, CLASS, 55, 0]]);
    assert_eq!(h.has(EE_SCENARIO_EV_REPLACE_UNIT), vec![[0x10, 1, CLASS, 0]]);
    assert_eq!(h.walks, vec![[7, 0, 3, 0, 5]]);
    h.of_type.clear();
    h.walks.clear();
    assert_eq!(trigger(&mut h, 0, 1, 0, 0, 0), 3);
    assert_eq!(h.walks, vec![[7, 0, 3, 4, 5]]);
}

#[test]
fn the_load_reads_the_effects_then_every_event() {
    let mut h = Fake::new();
    h.effects = vec![effect(0, 0)];
    h.answers.insert(EE_SCENARIO_EV_FILE_LENGTH, 108 * 2);
    h.answers.insert(EE_SCENARIO_EV_EVENT_NEW, 0x40);
    h.answers.insert(EE_SCENARIO_EV_EVENT_NUM, 0);
    h.answers.insert(EE_SCENARIO_EV_READ_COUNT, 5);
    assert_eq!(load(&mut h, 0xe, 0xf), 5);
    assert_eq!(h.has(EE_SCENARIO_EV_EFFECTS_ASSIGN), vec![[2, 0, 0, 0]]); // 216 / 88
    assert_eq!(h.has(EE_SCENARIO_EV_EFFECTS_READ), vec![[0xf, 0, 0, 0]]);
    assert_eq!(h.has(EE_SCENARIO_EV_EVENT_NEW).len(), 2);
    assert_eq!(h.has(EE_SCENARIO_EV_EVENT_NO_EFFECTS).len(), 2);
    assert_eq!(h.has(EE_SCENARIO_EV_FILE_LENGTH), vec![[0xf, 0, 0, 0], [0xe, 216, 0, 0]]);
}

#[test]
fn compose_splits_length_and_name() {
    let mut log: Vec<(u32, [i64; 4])> = Vec::new();
    let mut call = |op: u32, a: [i64; 4]| {
        log.push((op, a));
        if op == EE_SCENARIO_EV_FILE_LENGTH_INTO {
            // the C++ writes the length through the pointer
            // SAFETY: the adapter hands a live value
            unsafe { *(a[1] as usize as *mut std::os::raw::c_ulong) = 216 };
        }
        7
    };
    assert_eq!(ev_compose(EE_SCENARIO_EV_FILE_LENGTH, [0xe, 108, 0, 0], &mut call), 216);
    assert_eq!(ev_compose(EE_SCENARIO_EV_SET_NAME, [0x50, 0x3f80_0000, 0, 0], &mut call), 7);
    assert_eq!(ev_compose(EE_SCENARIO_EV_FILE_OPEN, [0xe, 0, 0, 0], &mut call), 7);
    let ops: Vec<u32> = log.iter().map(|e| e.0).collect();
    assert_eq!(
        ops,
        vec![
            EE_SCENARIO_EV_FILE_LENGTH_INTO,
            EE_SCENARIO_EV_NAME_TEXT,
            EE_SCENARIO_EV_SET_NAME_TEXT,
            EE_SCENARIO_EV_FILE_OPEN
        ]
    );
    assert_eq!(log[0].1[0], 0xe);
    assert_eq!(log[1].1, [0x3f80_0000, 0, 0, 0]);
    assert_eq!(log[2].1, [0x50, 0, 0, 0]);
}

#[test]
fn the_tables_hold_the_effects_and_the_events_with_their_lists() {
    let mut t = EvTable::default();
    let mut reads = vec![7 as c_ulong, 2];
    let mut read = |file: i64, value: &mut c_ulong| {
        assert_eq!(file, 0x99);
        *value = reads.remove(0);
        0
    };
    assert_eq!(t.op(EE_SCENARIO_EV_EFFECTS_ASSIGN, &[3, 0, 0, 0], &mut read), Some(0));
    assert_eq!(t.op(EE_SCENARIO_EV_EFFECTS_COUNT, &[0; 4], &mut read), Some(3));
    assert!(!t.effect_at(2).is_null() && t.effect_at(3).is_null());
    let e = t.op(EE_SCENARIO_EV_EVENT_NEW, &[0; 4], &mut read).unwrap();
    assert_eq!(t.op(EE_SCENARIO_EV_READ_ID, &[0x99, e, 0, 0], &mut read), Some(0));
    assert_eq!(t.op(EE_SCENARIO_EV_READ_COUNT, &[0x99, e, 0, 0], &mut read), Some(0));
    assert_eq!(t.op(EE_SCENARIO_EV_EVENT_NUM, &[e, 0, 0, 0], &mut read), Some(2));
    let list = t.op(EE_SCENARIO_EV_EVENT_ALLOC, &[e, 2, 0, 0], &mut read).unwrap();
    assert_eq!(t.op(EE_SCENARIO_EV_EVENT_EFFECTS, &[e, 0, 0, 0], &mut read), Some(list));
    assert_eq!(t.op(EE_SCENARIO_EV_EVENT_COUNT, &[0; 4], &mut read), Some(1));
    assert_eq!(t.op(EE_SCENARIO_EV_EVENT_AT, &[0; 4], &mut read), Some(e));
    let ev = t.event(0);
    // SAFETY: the table's event
    assert_eq!(unsafe { ((*ev).mID, (*ev).mNumEffects) }, (7, 2));
    assert!(t.event(1).is_null() && t.event(-1).is_null());
    let e2 = t.op(EE_SCENARIO_EV_EVENT_NEW, &[0; 4], &mut read).unwrap();
    assert_eq!(t.op(EE_SCENARIO_EV_EVENT_NO_EFFECTS, &[e2, 0, 0, 0], &mut read), Some(0));
    assert_eq!(t.op(EE_SCENARIO_EV_EVENTS_CLEAR, &[0; 4], &mut read), Some(0));
    assert_eq!(t.op(EE_SCENARIO_EV_EVENT_COUNT, &[0; 4], &mut read), Some(0));
    assert_eq!(t.op(EE_SCENARIO_EV_SOUND, &[0; 4], &mut read), None);
}
