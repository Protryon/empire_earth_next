//! The scenario capture records over a table world: each record's bytes as the C++ wrote them.

use std::cell::RefCell;
use std::collections::HashMap;
use std::ffi::CString;

use super::*;
use crate::app::capture_record::Record;

thread_local! {
    /// The records committed on this thread (the tests read them back).
    pub(super) static COMMITTED: RefCell<Vec<Record>> = const { RefCell::new(Vec::new()) };
}

fn take() -> Vec<Record> {
    COMMITTED.with(|c| std::mem::take(&mut *c.borrow_mut()))
}

/// A world of answers by (op, p, a); unknown reads answer 0. Saves write their element's address into the file.
#[derive(Default)]
struct Table {
    answers: HashMap<(u32, u64, i64), i64>,
    floats: HashMap<(u32, u64), f32>,
    texts: Vec<CString>,
    reads: Vec<u32>,
}

impl Table {
    fn set(&mut self, op: u32, p: u64, a: i64, v: i64) {
        self.answers.insert((op, p, a), v);
    }
    fn text(&mut self, op: u32, p: u64, t: &str) {
        let c = CString::new(t).unwrap();
        let at = c.as_ptr() as usize as i64;
        self.texts.push(c);
        self.set(op, p, 0, at);
    }
}

impl CaptureWorld for Table {
    fn read(&mut self, op: u32, p: u64, a: i64, b: i64) -> i64 {
        self.reads.push(op);
        match op {
            EE_SCENARIO_CAP_FILE_NEW => p as i64,
            EE_SCENARIO_CAP_SAVE_TRIGGER..=EE_SCENARIO_CAP_SAVE_GROUPS => {
                // SAFETY: `a` is the record's memory file (FILE_NEW answers its state)
                let file = unsafe { &mut *(a as usize as *mut EeFsMemFile) };
                file.write(&p.to_le_bytes());
                let _ = b;
                0
            }
            _ => *self.answers.get(&(op, p, a)).unwrap_or(&0),
        }
    }
    fn readf(&mut self, op: u32, p: u64) -> f32 {
        *self.floats.get(&(op, p)).unwrap_or(&0.0)
    }
}

fn words(r: &Record) -> Vec<u32> {
    r.data.chunks(4).map(|c| u32::from_le_bytes(c.try_into().unwrap())).collect()
}

fn fnv_text(t: &str) -> u64 {
    fnv(FNV_BASIS, t.as_bytes())
}

#[test]
fn a_condition_record_holds_its_numbers_texts_clock_and_seeds() {
    let mut w = Table::default();
    let c = Box::new(EeScenarioConditionFields {
        mType: 3,
        mTechnology: -2,
        mID: 9,
        mUsesHour: true,
        mIsTriggerOn: true,
        ..Default::default()
    });
    let cp = &*c as *const _ as u64;
    w.set(EE_SCENARIO_CAP_CONDITION_FIELDS, 0x100, 0, cp as i64);
    w.text(EE_SCENARIO_CAP_CONDITION_CHAT, 0x100, "hello");
    let m = Box::new(EeScenarioManagerFields { mTime: 1.5, ..Default::default() });
    w.set(EE_SCENARIO_CAP_MANAGER, 0, 0, 0x200);
    w.set(EE_SCENARIO_CAP_MANAGER_FIELDS, 0x200, 0, &*m as *const _ as i64);
    w.text(EE_SCENARIO_CAP_LAST_CHAT, 0x200, "Hello there");
    w.set(EE_SCENARIO_CAP_SEED, 0, 0, 77);
    take();
    condition(&mut w, 0x100, true, 5);
    let r = take();
    assert_eq!(r.len(), 1);
    assert_eq!(r[0].tag, u32::from_le_bytes(*b"SMCE"));
    let v = words(&r[0]);
    assert_eq!(&v[..8], &[9, 3, 0, 0, 0, 0, 0, (-2i32) as u32]);
    // the id and 15 numbers, then the flags word, the texts' hashes, the clock, the seeds and the result
    assert_eq!(v[16], 1 | 16);
    let d = &r[0].data;
    assert_eq!(u64::from_le_bytes(d[68..76].try_into().unwrap()), fnv_text("hello"));
    assert_eq!(u64::from_le_bytes(d[76..84].try_into().unwrap()), fnv_text("Hello there"));
    assert_eq!(f32::from_le_bytes(d[84..88].try_into().unwrap()), 1.5);
    assert_eq!(&v[22..], &[5, 77, 1]);
}

#[test]
fn a_condition_without_a_manager_has_empty_texts_and_clock() {
    let mut w = Table::default();
    let c = Box::new(EeScenarioConditionFields::default());
    w.set(EE_SCENARIO_CAP_CONDITION_FIELDS, 1, 0, &*c as *const _ as i64);
    take();
    condition(&mut w, 1, false, 0);
    let r = take();
    let d = &r[0].data;
    assert_eq!(u64::from_le_bytes(d[68..76].try_into().unwrap()), FNV_BASIS); // a null chat text
    assert_eq!(u64::from_le_bytes(d[76..84].try_into().unwrap()), 0);
    assert!(!w.reads.contains(&EE_SCENARIO_CAP_LAST_CHAT));
}

#[test]
fn an_object_evaluate_keeps_the_choices_and_areas_inside_it_out() {
    let mut w = Table::default();
    let o = Box::new(EeScenarioObjectFields { mOwner: 4, mIsDynamic: true, ..Default::default() });
    w.set(EE_SCENARIO_CAP_OBJECT_FIELDS, 0x10, 0, &*o as *const _ as i64);
    w.set(EE_SCENARIO_CAP_OBJECT_UNITS, 0x10, 0, 2);
    w.set(EE_SCENARIO_CAP_OBJECT_UNIT, 0x10, 0, 7);
    w.set(EE_SCENARIO_CAP_OBJECT_UNIT, 0x10, 1, 8);
    w.set(EE_SCENARIO_CAP_MANAGER, 0, 0, 0x200);
    w.set(EE_SCENARIO_CAP_OBJECTS, 0x200, 0, 3);
    w.set(EE_SCENARIO_CAP_OBJECT_AT, 0x200, 2, 0x10);
    take();
    let before = object_begin(&mut w, 0x10);
    let mut hash = FNV_BASIS;
    for id in [7u32, 8] {
        hash = fnv(hash, &id.to_le_bytes());
    }
    assert_eq!(before, EeScenarioCapObjectBefore { mCount: 2, mHash: hash, mSeed: 0 });
    players(&mut w, 1, true, 2, 0, 0);
    area(&mut w, 0x30, 0, 0, 0, 0, 0, 0);
    object_end(&mut w, 0x10, true, true, &before);
    players(&mut w, 1, true, 2, 0, 0);
    let r = take();
    assert_eq!(r.len(), 2);
    assert_eq!(r[0].tag, u32::from_le_bytes(*b"SMOB"));
    let v = words(&r[0]);
    assert_eq!(v[0], 2); // the object's index
    assert_eq!(v[1..3], [0, 4]); // mL64, mOwner
    assert_eq!(v[23], 2); // mIsAttributePercent clear, the dead flag
    assert_eq!(r[1].tag, u32::from_le_bytes(*b"SMCP"));
    assert_eq!(words(&r[1])[3], 0xffff_ffff); // no list
}

#[test]
fn a_saved_list_hashes_its_elements_saves_and_stops_at_the_first_error() {
    let mut w = Table::default();
    w.set(EE_SCENARIO_CAP_TRIGGERS, 0x200, 0, 2);
    w.set(EE_SCENARIO_CAP_TRIGGER_AT, 0x200, 0, 0x41);
    w.set(EE_SCENARIO_CAP_TRIGGER_AT, 0x200, 1, 0x42);
    take();
    saved(&mut w, 0x200, 1, 0, 0);
    let r = take();
    assert_eq!(r[0].tag, u32::from_le_bytes(*b"SMSV"));
    let d = &r[0].data;
    assert_eq!(u32::from_le_bytes(d[8..12].try_into().unwrap()), 2);
    let mut bytes = 0x41u64.to_le_bytes().to_vec();
    bytes.extend_from_slice(&0x42u64.to_le_bytes());
    assert_eq!(u64::from_le_bytes(d[16..24].try_into().unwrap()), fnv(FNV_BASIS, &bytes));
    // an empty list: no saves, the empty hash
    assert_eq!(u64::from_le_bytes(d[32..40].try_into().unwrap()), FNV_BASIS);
    // every file made was freed
    let made = w.reads.iter().filter(|&&o| o == EE_SCENARIO_CAP_FILE_NEW).count();
    let freed = w.reads.iter().filter(|&&o| o == EE_SCENARIO_CAP_FILE_FREE).count();
    assert_eq!((made, freed), (7, 7));
}

#[test]
fn an_effect_world_restores_the_last_sum_and_walks_the_units() {
    let mut w = Table::default();
    let e = Box::new(EeScenarioEffectFields { mID: 6, ..Default::default() });
    w.set(EE_SCENARIO_CAP_EFFECT_FIELDS, 0x50, 0, &*e as *const _ as i64);
    w.set(EE_SCENARIO_CAP_MANAGER, 0, 0, 0x200);
    w.set(EE_SCENARIO_CAP_LAST_SUM, 0, 0, 1234);
    w.set(EE_SCENARIO_CAP_FIRST_UNIT, 0, 0, 0x60);
    w.set(EE_SCENARIO_CAP_NEXT_UNIT, 0x60, 0, 0x61);
    w.set(EE_SCENARIO_CAP_UNIT_CHECKSUM, 0x60, 0, 10);
    w.set(EE_SCENARIO_CAP_UNIT_CHECKSUM, 0x61, 0, 20);
    w.floats.insert((EE_SCENARIO_CAP_GAME_SPEED, 0), 2.0);
    take();
    effect_world(&mut w, 0x50, 0);
    let r = take();
    assert_eq!(r[0].tag, u32::from_le_bytes(*b"SMEW"));
    let set = w.reads.iter().position(|&o| o == EE_SCENARIO_CAP_SET_LAST_SUM).unwrap();
    assert!(set > w.reads.iter().position(|&o| o == EE_SCENARIO_CAP_SUM).unwrap());
    let v = words(&r[0]);
    // id, save result, hash (2 words), the sum, the count and the units' sum
    assert_eq!((v[0], v[5], v[6]), (6, 2, 30));
    // five objects missing, no players, then three media hashes, the counts, the triggers, the time of day
    assert_eq!(&v[8..13], &[0xffff_ffff; 5]);
    let speed_at = 13 + 1 + 6 + 2 + 1 + 1;
    assert_eq!(f32::from_bits(v[speed_at]), 2.0);
}

#[test]
fn every_120th_round_records_the_saves_too() {
    let mut w = Table::default();
    let m = Box::new(EeScenarioManagerFields::default());
    w.set(EE_SCENARIO_CAP_MANAGER_FIELDS, 0x200, 0, &*m as *const _ as i64);
    let q = Box::new(EeScenarioTimedQueue::default());
    w.set(EE_SCENARIO_CAP_TIMED, 0x200, 0, &*q as *const _ as i64);
    take();
    ROUNDS.store(118, std::sync::atomic::Ordering::Relaxed);
    update(&mut w, 0x200, true, 0);
    update(&mut w, 0x200, true, 0);
    let tags: Vec<u32> = take().iter().map(|r| r.tag).collect();
    let (up, sv) = (u32::from_le_bytes(*b"SMUP"), u32::from_le_bytes(*b"SMSV"));
    assert_eq!(tags, vec![up, sv, up]);
}
