//! The scenario manager's own numbers (`class EEScenarioManager : public FSPersistent, public
//! EeScenarioManagerFields`, ee/EEScenarioManager.h: the clocks, the id counters, the cinematic and dialog flags, with
//! their C++ names and types), its timed effects' queue ([`EeScenarioTimedQueue`], owned here, the C++ holding the
//! pointer), `Reset`'s numbers, and the manager's `Load` and `Save` over the [`crate::stream`] (the elements, media
//! strings and dialog messages by operation: the C++ creates and owns them).

#![allow(non_snake_case)]
// `c_ulong` and `c_long` are 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::scenario::stream::{
    Seq, Stream, EE_SCENARIO_ELEMENT_AREAS, EE_SCENARIO_ELEMENT_CONDITIONS, EE_SCENARIO_ELEMENT_EFFECTS,
    EE_SCENARIO_ELEMENT_OBJECTS, EE_SCENARIO_ELEMENT_PENDING, EE_SCENARIO_ELEMENT_TRIGGERS,
};

/// The manager's string slots in its stream: `mExportFileName`, `mLastChatText`.
pub const EE_SCENARIO_MANAGER_EXPORT_NAME: u32 = 0;
pub const EE_SCENARIO_MANAGER_LAST_CHAT: u32 = 1;

/// `EEScenarioManager`'s numbers, in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScenarioManagerFields {
    pub mGameResult: c_long,
    /// The camera zoom `StartCinematic` saves.
    pub mSavedZoom: f32,
    pub mStartX: f32,
    pub mStartY: f32,
    /// The scenario clock (ms).
    pub mTime: f32,
    /// The time since the last trigger round (ms).
    pub mTriggerTimer: f32,
    pub mCinematicStartTime: f32,
    pub mSavedCameraMode: c_long,
    pub mNextTriggerID: c_ulong,
    pub mNextConditionID: c_ulong,
    pub mNextEffectID: c_ulong,
    pub mNextObjectID: c_ulong,
    pub mNextAreaID: c_ulong,
    /// Set by `Reset`: new pending effects are not recorded until `ApplyPendingEffects` has run.
    pub mB234: bool,
    pub mIsMovingCamera: bool,
    pub mShowDebugIDs: bool,
    pub mIsShowingDialog: bool,
    pub mIsEndingCinematic: bool,
}

/// One timed effect (`EESMTimedEffect`): an `mEffects` index, the scenario time it is due, the cinematic flag, and
/// whether it has run (the round erases those).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScenarioTimedEntry {
    pub mEffectIndex: c_ulong,
    pub mTime: c_ulong,
    pub mIsInCinematic: bool,
    pub mHasRun: bool,
}

/// The manager's timed effects, kept sorted by time (stably: equal times in the order they came).
#[derive(Clone, Debug, Default)]
pub struct EeScenarioTimedQueue {
    pub entries: Vec<EeScenarioTimedEntry>,
}

impl EeScenarioTimedQueue {
    /// `std::stable_sort` by `mTime` (`EESMTimedEffectLess`).
    fn sort(&mut self) {
        self.entries.sort_by_key(|e| e.mTime);
    }

    /// `AddTimedEffect(effect, time, cinematic)`: appended (`long` stored as `unsigned long`), then sorted.
    pub fn add(&mut self, effect: c_long, time: c_ulong, cinematic: bool) {
        self.entries.push(EeScenarioTimedEntry {
            mEffectIndex: effect as c_ulong,
            mTime: time,
            mIsInCinematic: cinematic,
            mHasRun: false,
        });
        self.sort();
    }

    /// `RemoveTimedEffects(effect)`: every entry of the effect (`unsigned long == long`: the C++'s conversion).
    pub fn remove_effect(&mut self, effect: c_long) {
        self.entries.retain(|e| e.mEffectIndex != effect as c_ulong);
    }

    /// `RenumberEffects`' part: entries of `old` follow it to `new`, or go when `new` is "none" (`UIsNone32`).
    pub fn renumber(&mut self, old: c_ulong, new: c_ulong) {
        if new as u32 == u32::MAX {
            self.entries.retain(|e| e.mEffectIndex != old);
        } else {
            for e in self.entries.iter_mut().filter(|e| e.mEffectIndex == old) {
                e.mEffectIndex = new;
            }
        }
    }

    /// `RemapEffects`' part (`EERemapID`): each index through the map (`olds[i]` -> `news[i]`), 0 when unmapped.
    pub fn remap(&mut self, olds: &[c_ulong], news: &[c_ulong]) {
        for e in &mut self.entries {
            e.mEffectIndex = olds.iter().position(|&o| o == e.mEffectIndex).map_or(0, |i| news[i]);
        }
    }

    /// `SetTimedEffectHasRun(effect, flag)`: the first entry of the effect (its `mEffects` index, `None` when the
    /// effect is not in the list: no entry matches) whose flag differs.
    pub fn set_has_run(&mut self, effect: Option<c_ulong>, flag: bool) {
        let Some(effect) = effect else { return };
        if let Some(e) = self.entries.iter_mut().find(|e| e.mEffectIndex == effect && e.mHasRun != flag) {
            e.mHasRun = flag;
        }
    }

    /// `EndCinematic`'s part: the entries outside the cinematic are delayed by `shift` (`(long)elapsed + 3000`,
    /// added to the `unsigned long`), the cinematic's unrun ones marked run; then sorted again.
    pub fn end_cinematic(&mut self, shift: c_long) {
        for e in &mut self.entries {
            if !e.mIsInCinematic {
                e.mTime = e.mTime.wrapping_add(shift as c_ulong);
            } else if !e.mHasRun {
                e.mHasRun = true;
            }
        }
        self.sort();
    }

    /// The round's end: every entry that has run is erased (from the back, as the original).
    pub fn erase_run(&mut self) {
        self.entries.retain(|e| !e.mHasRun);
    }
}

/// `Reset`'s numbers: the clocks, counters and flags (the camera's saved zoom and mode are left), the queue
/// emptied.
pub fn reset(f: &mut EeScenarioManagerFields, q: &mut EeScenarioTimedQueue) {
    f.mIsMovingCamera = false;
    f.mTime = 0.0;
    f.mTriggerTimer = 0.0;
    f.mCinematicStartTime = 0.0;
    f.mNextTriggerID = 0;
    f.mNextConditionID = 0;
    f.mNextEffectID = 0;
    f.mStartX = 0.0;
    f.mNextObjectID = 0;
    f.mNextAreaID = 0;
    f.mGameResult = 0;
    f.mStartY = 0.0;
    q.entries.clear();
    f.mShowDebugIDs = false;
    f.mIsShowingDialog = false;
    f.mIsEndingCinematic = false;
    f.mB234 = true;
}

/// A list of elements read as the original's: its count, then each element's `Load` (the C++ creates it and keeps
/// or drops it) until one fails.
fn load_elements<S: Stream>(q: &mut Seq<S>, kind: u32) {
    let mut count: c_ulong = 0;
    q.ulong(&mut count);
    let mut i: c_ulong = 0;
    while q.ok() && i < count {
        q.r = q.s.element_load(kind);
        i += 1;
    }
}

/// `EEScenarioManager::Load` after its `Reset`: the first error. The timed effects go into `push` (the queue, by
/// one short borrow each).
pub fn load<S: Stream>(
    s: &mut S,
    f: &mut EeScenarioManagerFields,
    minor: c_ulong,
    push: &mut dyn FnMut(EeScenarioTimedEntry),
) -> i32 {
    let mut q = Seq::new(s);
    load_elements(&mut q, EE_SCENARIO_ELEMENT_TRIGGERS);
    load_elements(&mut q, EE_SCENARIO_ELEMENT_CONDITIONS);
    load_elements(&mut q, EE_SCENARIO_ELEMENT_EFFECTS);
    load_elements(&mut q, EE_SCENARIO_ELEMENT_OBJECTS);
    load_elements(&mut q, EE_SCENARIO_ELEMENT_AREAS);
    let mut count: c_ulong = 0;
    q.ulong(&mut count);
    let mut i: c_ulong = 0;
    while q.ok() && i < count {
        let (mut effect, mut time, mut cinematic): (c_ulong, c_ulong, bool) = (0, 0, false);
        q.ulong(&mut effect);
        q.ulong(&mut time);
        q.bool(&mut cinematic);
        if !q.ok() {
            return q.r;
        }
        push(EeScenarioTimedEntry { mEffectIndex: effect, mTime: time, mIsInCinematic: cinematic, mHasRun: false });
        i += 1;
    }
    load_elements(&mut q, EE_SCENARIO_ELEMENT_PENDING);
    q.ulong(&mut f.mNextTriggerID);
    q.ulong(&mut f.mNextConditionID);
    q.ulong(&mut f.mNextEffectID);
    q.ulong(&mut f.mNextObjectID);
    q.ulong(&mut f.mNextAreaID);
    q.float(&mut f.mTime);
    q.float(&mut f.mTriggerTimer);
    if q.ok() {
        q.r = q.s.media(false);
    }
    // written only from minor version 0x14 on (Save): older files go straight on to the debug-ID flag
    if minor >= 0x14 {
        q.bool(&mut f.mIsMovingCamera);
        q.float(&mut f.mStartX);
        q.float(&mut f.mStartY);
    }
    q.bool(&mut f.mShowDebugIDs);
    if q.ok() {
        q.r = q.s.messages(false);
    }
    q.string(EE_SCENARIO_MANAGER_EXPORT_NAME);
    q.string(EE_SCENARIO_MANAGER_LAST_CHAT);
    q.r
}

/// One section of the manager's save: the count (always written), then elements `first..count + first` until one
/// fails; the section's result.
fn save_elements<S: Stream>(s: &mut S, kind: u32, first: c_ulong) -> i32 {
    let count = s.element_count(kind).wrapping_sub(first);
    let mut r = s.write_ulong(count);
    if r == 0 {
        let mut i = first;
        while i < count.wrapping_add(first) {
            r = s.element_save(kind, i);
            if r != 0 {
                break;
            }
            i += 1;
        }
    }
    r
}

/// `EEScenarioManager::Save`: the elements but the built-ins (the first condition, effect and object, the first
/// two areas), the timed and pending effects, the counters, clocks, media strings, start camera and dialog
/// messages. The first error of the first five sections, else the rest's (each section's count is written whatever
/// came before; the timed effects only after a good areas section).
pub fn save<S: Stream>(s: &mut S, f: &EeScenarioManagerFields, timed: &[EeScenarioTimedEntry], minor: c_ulong) -> i32 {
    let mut first = 0;
    let mut r = 0;
    for (kind, from) in [
        (EE_SCENARIO_ELEMENT_TRIGGERS, 0),
        (EE_SCENARIO_ELEMENT_CONDITIONS, 1),
        (EE_SCENARIO_ELEMENT_EFFECTS, 1),
        (EE_SCENARIO_ELEMENT_OBJECTS, 1),
        (EE_SCENARIO_ELEMENT_AREAS, 2),
    ] {
        r = save_elements(s, kind, from);
        if first == 0 {
            first = r;
        }
    }
    let count = timed.len() as c_ulong;
    if r == 0 {
        r = s.write_ulong(count);
    }
    if r == 0 {
        for e in timed {
            r = s.write_ulong(e.mEffectIndex);
            if r == 0 {
                r = s.write_ulong(e.mTime);
            }
            if r == 0 {
                r = s.write_bool(e.mIsInCinematic);
            }
            if r != 0 {
                break;
            }
        }
    }
    if first == 0 {
        first = r;
    }
    let count = s.element_count(EE_SCENARIO_ELEMENT_PENDING);
    r = s.write_ulong(count);
    if r == 0 {
        let mut k: c_ulong = 0;
        while k < count {
            r = s.element_save(EE_SCENARIO_ELEMENT_PENDING, k);
            if r != 0 {
                break;
            }
            k += 1;
        }
        let mut q = Seq { s, r };
        q.put_ulong(f.mNextTriggerID);
        q.put_ulong(f.mNextConditionID);
        q.put_ulong(f.mNextEffectID);
        q.put_ulong(f.mNextObjectID);
        q.put_ulong(f.mNextAreaID);
        q.put_float(f.mTime);
        q.put_float(f.mTriggerTimer);
        if q.ok() {
            q.r = q.s.media(true);
        }
        if q.ok() {
            if minor >= 0x14 {
                q.put_bool(f.mIsMovingCamera);
                q.put_float(f.mStartX);
                q.put_float(f.mStartY);
            }
            q.put_bool(f.mShowDebugIDs);
            if q.ok() {
                q.r = q.s.messages(true);
            }
            q.put_string(EE_SCENARIO_MANAGER_EXPORT_NAME);
            q.put_string(EE_SCENARIO_MANAGER_LAST_CHAT);
        }
        r = q.r;
    }
    if first != 0 {
        first
    } else {
        r
    }
}

#[cfg(test)]
#[allow(clippy::field_reassign_with_default)] // the tests set a few fields of a sample
mod tests {
    use super::*;
    use crate::scenario::stream::Bytes;

    fn entry(effect: c_ulong, time: c_ulong, cinematic: bool, run: bool) -> EeScenarioTimedEntry {
        EeScenarioTimedEntry { mEffectIndex: effect, mTime: time, mIsInCinematic: cinematic, mHasRun: run }
    }

    #[test]
    fn the_queue_stays_sorted_by_time_and_stable() {
        let mut q = EeScenarioTimedQueue::default();
        q.add(1, 500, false);
        q.add(2, 100, true);
        q.add(3, 500, false);
        q.add(4, 100, false);
        q.add(-1, 0, false);
        let order: Vec<c_ulong> = q.entries.iter().map(|e| e.mEffectIndex).collect();
        assert_eq!(order, vec![(-1 as c_long) as c_ulong, 2, 4, 1, 3]);
        q.remove_effect(-1);
        q.remove_effect(4);
        let order: Vec<c_ulong> = q.entries.iter().map(|e| e.mEffectIndex).collect();
        assert_eq!(order, vec![2, 1, 3]);
    }

    #[test]
    fn renumber_moves_or_drops_and_remap_zeroes_the_unmapped() {
        let mut q = EeScenarioTimedQueue { entries: vec![entry(3, 1, false, false), entry(4, 2, false, false)] };
        q.renumber(3, 7);
        assert_eq!(q.entries[0].mEffectIndex, 7);
        q.renumber(7, 0xffff_ffff);
        assert_eq!(q.entries, vec![entry(4, 2, false, false)]);
        q.entries.push(entry(9, 3, false, false));
        q.remap(&[4, 5], &[6, 4]);
        assert_eq!(q.entries.iter().map(|e| e.mEffectIndex).collect::<Vec<_>>(), vec![6, 0]);
    }

    #[test]
    fn set_has_run_changes_the_first_entry_whose_flag_differs() {
        let mut q = EeScenarioTimedQueue {
            entries: vec![entry(2, 1, false, false), entry(2, 2, false, true), entry(2, 3, false, true)],
        };
        q.set_has_run(Some(2), false);
        assert_eq!(q.entries.iter().map(|e| e.mHasRun).collect::<Vec<_>>(), vec![false, false, true]);
        q.set_has_run(None, true);
        q.set_has_run(Some(5), true);
        assert_eq!(q.entries.iter().map(|e| e.mHasRun).collect::<Vec<_>>(), vec![false, false, true]);
    }

    #[test]
    fn end_cinematic_delays_the_rest_marks_the_cinematic_run_and_sorts() {
        let mut q = EeScenarioTimedQueue {
            entries: vec![entry(1, 100, false, false), entry(2, 200, true, false), entry(3, 300, true, true)],
        };
        q.end_cinematic(4000);
        assert_eq!(q.entries, vec![entry(2, 200, true, true), entry(3, 300, true, true), entry(1, 4100, false, false)]);
        // the unsigned long wraps
        let mut q = EeScenarioTimedQueue { entries: vec![entry(1, 100, false, false)] };
        q.end_cinematic(-200);
        assert_eq!(q.entries[0].mTime, (100 as c_ulong).wrapping_sub(200));
        q.erase_run();
        assert_eq!(q.entries.len(), 1);
        q.entries[0].mHasRun = true;
        q.erase_run();
        assert!(q.entries.is_empty());
    }

    #[test]
    fn reset_zeroes_the_clocks_and_counters_and_keeps_the_camera() {
        let mut f = EeScenarioManagerFields {
            mGameResult: 3,
            mSavedZoom: 2.5,
            mTime: 9.0,
            mSavedCameraMode: 4,
            mNextAreaID: 8,
            mIsShowingDialog: true,
            ..Default::default()
        };
        let mut q = EeScenarioTimedQueue { entries: vec![entry(1, 1, false, false)] };
        reset(&mut f, &mut q);
        assert_eq!(
            f,
            EeScenarioManagerFields { mSavedZoom: 2.5, mSavedCameraMode: 4, mB234: true, ..Default::default() }
        );
        assert!(q.entries.is_empty());
    }

    fn sample() -> (EeScenarioManagerFields, Vec<EeScenarioTimedEntry>, Bytes) {
        let f = EeScenarioManagerFields {
            mStartX: 12.5,
            mStartY: -3.0,
            mTime: 81500.0,
            mTriggerTimer: 250.0,
            mNextTriggerID: 4,
            mNextConditionID: 5,
            mNextEffectID: 6,
            mNextObjectID: 7,
            mNextAreaID: 8,
            mIsMovingCamera: true,
            mShowDebugIDs: true,
            ..Default::default()
        };
        let timed = vec![entry(3, 90000, false, false), entry(1, 95000, true, false)];
        let mut b = Bytes::default();
        // the built-ins first: one condition, effect and object, two areas
        b.elements = [vec![10, 11], vec![0, 20], vec![0, 30, 31], vec![0], vec![0, 1, 40], vec![50]];
        b.strings = vec![b"export".to_vec(), b"chat".to_vec()];
        b.media = 0x77;
        b.messages = 0x66;
        (f, timed, b)
    }

    fn fresh() -> Bytes {
        let mut b = Bytes::default();
        b.elements = [vec![], vec![0], vec![0], vec![0], vec![0, 1], vec![]];
        b
    }

    #[test]
    fn the_manager_round_trips_without_its_built_ins() {
        let (f, timed, mut b) = sample();
        assert_eq!(save(&mut b, &f, &timed, 0x14), 0);
        // 7 counts (the timed effects' too) + 7 elements, 2 x (2 longs + bool), 5 counters, 2 clocks, media, 1 + 2 x 4 camera, 1 debug,
        // messages, two strings (size + bytes + NUL)
        assert_eq!(b.data.len(), 7 * 4 + 7 * 4 + 2 * 9 + 5 * 4 + 2 * 4 + 4 + 9 + 1 + 4 + (4 + 7) + (4 + 5));
        let mut r = fresh();
        r.data = b.data.clone();
        let mut g = EeScenarioManagerFields::default();
        let mut got = Vec::new();
        assert_eq!(load(&mut r, &mut g, 0x14, &mut |e| got.push(e)), 0);
        assert_eq!(g, f);
        assert_eq!(got, timed);
        assert_eq!(r.elements, b.elements);
        assert_eq!(r.strings, b.strings);
        assert_eq!((r.media, r.messages), (0x77, 0x66));
        assert_eq!(r.at, r.data.len());
    }

    #[test]
    fn before_minor_0x14_there_is_no_start_camera() {
        let (f, timed, mut b) = sample();
        assert_eq!(save(&mut b, &f, &timed, 0x13), 0);
        let mut r = fresh();
        r.data = b.data.clone();
        let mut g = EeScenarioManagerFields::default();
        assert_eq!(load(&mut r, &mut g, 0x13, &mut |_| {}), 0);
        assert_eq!((g.mIsMovingCamera, g.mStartX, g.mStartY, g.mShowDebugIDs), (false, 0.0, 0.0, true));
        assert_eq!(r.at, r.data.len());
    }

    #[test]
    fn a_failed_element_stops_the_load_with_its_error() {
        let (f, timed, mut b) = sample();
        assert_eq!(save(&mut b, &f, &timed, 0x14), 0);
        let mut r = fresh();
        r.data = b.data.clone();
        r.fail_element_after = Some(3);
        let mut g = EeScenarioManagerFields::default();
        let mut got = Vec::new();
        assert_eq!(load(&mut r, &mut g, 0x14, &mut |e| got.push(e)), 9);
        assert_eq!(r.elements[0], vec![10, 11]);
        assert_eq!(r.elements[1], vec![0, 20]);
        assert_eq!(r.elements[2], vec![0]);
        assert!(got.is_empty());
        assert_eq!(g, EeScenarioManagerFields::default());
        // a file cut inside the timed effects: the entries read before stay, the rest is not read
        let mut r = fresh();
        // the five sections (5 counts, 6 elements), the timed count, one entry and the next one's index
        r.data = b.data[..5 * 4 + 6 * 4 + 4 + 9 + 4].to_vec();
        let mut got = Vec::new();
        assert_eq!(load(&mut r, &mut g, 0x14, &mut |e| got.push(e)), 9);
        assert_eq!(got, vec![timed[0]]);
    }
}
