//! TEST ONLY: the calamity lane's capture records (was EECalamityCapture.cpp; ee/EECalamityCapture.h;
//! tools/sim_capture.py --profile calamity): CALI, each calamity class's members; CALK, a calamity's and the
//! contagion manager's save, load and checksums. Reads the world (through EEGameOps.cpp's cases); writes nothing to
//! it but the copies it loads and deletes.

// the C++'s own casts (`long` and `unsigned long` are 32-bit here, 64-bit on LP64), literals, products and
// loops, kept as written
#![allow(
    clippy::unnecessary_cast,
    clippy::excessive_precision,
    clippy::approx_constant,
    clippy::neg_multiply,
    clippy::assign_op_pattern,
    clippy::needless_range_loop,
    clippy::too_many_arguments
)]

use std::ffi::{c_long, c_ulong, c_void};

use ee_base::math::types::{Ge3DPoint, GeRectangle};
use ee_sim::tech::calamity_class::{self as cc, EeCalamityClassFields};
use ee_sim::tech::calamity_record::EeCalamityFields;
use ee_sim::tech::contagion_functor::EeContagionFunctorFields;
use ee_sim::tech::contagion_save::EeContagionHelperFields;
use ee_sim::tech::contagion_store_ffi as cs;

use super::{cptr, h, hd, hf, hp, pa, rec, O, P};

/// A record (EESimCaptureRecord): the payload's bytes, committed whole.
#[derive(Default)]
pub(crate) struct Record(Vec<u8>);

impl Record {
    pub(crate) fn i32(&mut self, v: i32) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }
    pub(crate) fn u32(&mut self, v: u32) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }
    pub(crate) fn f32(&mut self, v: f32) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }
    /// The payload so far.
    #[cfg(test)]
    pub(crate) fn payload(&self) -> &[u8] {
        &self.0
    }
    pub(crate) fn bytes(&mut self, b: &[u8]) {
        self.0.extend_from_slice(b);
    }
    pub(crate) fn commit(self, tag: &[u8; 4]) {
        self.commit_tag(u32::from_le_bytes(*tag));
    }
    /// The record under a tag's number (EECaptureTag's value).
    pub(crate) fn commit_tag(self, tag: u32) {
        // SAFETY: our payload
        unsafe { ee_sim::app::capture::ee_app_capture_commit(tag, self.0.as_ptr(), self.0.len()) };
    }
}

fn active() -> bool {
    ee_sim::app::capture::ee_app_capture_active()
}

fn klass<'a>(kl: P) -> &'a EeCalamityClassFields {
    // SAFETY: a live class's record, read before the next C++ call
    unsafe { &*rec(O::KlFields, kl) }
}

fn effect(r: &mut Record, e: P) {
    if e == 0 {
        r.i32(-1);
        return;
    }
    r.i32(h(O::GfxEffectIndex, e, &[]) as i32);
    r.u32(h(O::GfxEffectDuration, e, &[]) as u32);
    r.f32(hd(O::GfxEffectSize, e, &[], &[]));
    r.u32(h(O::GfxEffectTime1, e, &[]) as u32);
}

fn sound(r: &mut Record, s: P) {
    if s == 0 {
        r.i32(-1);
        return;
    }
    r.i32(h(O::SoundObjectIndex, s, &[]) as i32);
}

fn effect_index(e: P) -> i32 {
    if e == 0 {
        -1
    } else {
        h(O::GfxEffectIndex, e, &[]) as i32
    }
}

/// The record each of the six functor classes wrote (its tag: 1 fire, 2 leprosy, 3 plague, 4 area, 5 extra damage,
/// 6 mind control).
fn functor(r: &mut Record, fu: P) {
    const TAGS: [u32; 7] = [7, 4, 5, 6, 1, 2, 3]; // by EE_CALAMITY_FUNCTOR_*
    if fu == 0 {
        r.u32(0);
        return;
    }
    // SAFETY: the live functor's record
    let f: EeContagionFunctorFields = unsafe { *rec(O::FnFields, fu) };
    r.u32(TAGS[f.mKind as usize]);
    match f.mKind {
        // the extra damage's and the mind control's own field, then the area effects' base (the C++ switch's break)
        cc::EE_CALAMITY_FUNCTOR_EXTRA_DAMAGE => r.u32(f.mFlag as u32),
        cc::EE_CALAMITY_FUNCTOR_MIND_CONTROL => r.i32(effect_index(hp(O::FnEffect1, fu, &[]))),
        _ => {}
    }
    match f.mKind {
        cc::EE_CALAMITY_FUNCTOR_FIRE => {
            r.i32(effect_index(hp(O::FnEffect1, fu, &[])));
            r.f32(f.mRadius);
            r.f32(f.mChance);
            r.i32(f.mBurn as i32);
            r.i32(f.mDamage as i32);
            r.u32(f.mSpreadTime as u32);
            r.u32(f.mUnitSet as u32);
            r.i32(f.mMode as i32);
        }
        cc::EE_CALAMITY_FUNCTOR_LEPROSY => {
            r.i32(f.mDamage as i32);
            r.u32(f.mSpreadTime as u32);
            r.u32(f.mUnitSet as u32);
            r.i32(f.mMode as i32);
        }
        cc::EE_CALAMITY_FUNCTOR_PLAGUE => {
            r.f32(f.mRadius);
            r.f32(f.mChance);
            r.i32(f.mDamage as i32);
            r.u32(f.mSpreadTime as u32);
            r.u32(f.mUnitSet as u32);
            r.i32(f.mMode as i32);
        }
        _ => {
            // the area effects' base
            r.i32(effect_index(hp(O::FnEffect2, fu, &[])));
            r.i32(effect_index(hp(O::FnEffect3, fu, &[])));
            r.i32(f.mDamage as i32);
            r.i32(f.mSpreadTime as c_long as i32);
            r.u32(f.mUnitSet as u32);
            r.i32(f.mMode as i32);
        }
    }
}

/// The media slots in one order for every class: effects 1-4, the sound and the secondary sound (a class without
/// a slot: -1), then the functor. `slots`: which of effects 1-4, sound, secondary sound, functor the class has.
fn media(r: &mut Record, kl: P, slots: [bool; 7]) {
    let ops = [O::KlEffect1, O::KlEffect2, O::KlEffect3, O::KlEffect4];
    for (i, op) in ops.iter().enumerate() {
        effect(r, if slots[i] { hp(*op, kl, &[]) } else { 0 });
    }
    sound(r, if slots[4] { hp(O::KlSound, kl, &[]) } else { 0 });
    sound(r, if slots[5] { hp(O::KlSecondary, kl, &[]) } else { 0 });
    functor(r, if slots[6] { hp(O::KlFunctor, kl, &[]) } else { 0 });
}

const T: bool = true;
const F: bool = false;

/// Each class's own members (the base's, by the class's enum).
fn class_record(r: &mut Record, kl: P) {
    let p = *klass(kl);
    match p.mClassEnum as i32 {
        3 => {
            // the earthquake
            media(r, kl, [T, T, F, F, T, F, F]);
            r.i32(p.mDamage as i32);
            r.i32(p.mTickTime as i32);
            r.i32(p.mShake as i32);
            r.i32(p.mCrackCount as i32);
            for v in [p.mDuration, p.mRadius, p.mShakeInterval, p.mVolume, p.mQuakeVolume] {
                r.f32(v);
            }
            for v in [p.mMinimumFactor, p.mQuakeMinimumFactor, p.mSpeed, p.mSourceSpeed, p.mFalloff] {
                r.f32(v);
            }
        }
        4 => {
            // the hurricane
            media(r, kl, [T, T, T, F, T, F, F]);
            r.i32(p.mDamage as i32);
            r.u32(p.mTickTime as u32);
            r.i32(p.mArea as i32);
            r.i32(p.mUnitClassIndex as i32);
            r.f32(p.mRadius);
            r.u32(p.mOverWater as u32);
        }
        7 => {
            // the plague
            media(r, kl, [T, F, F, F, T, T, T]);
            r.i32(p.mDamage as i32);
        }
        8 => {
            // the leprosy
            media(r, kl, [T, F, F, F, T, F, T]);
            r.i32(p.mDamage as i32);
            r.u32(p.mEffect1Duration as u32);
            r.u32(p.mRadiusTiles as u32);
        }
        9 => {
            // the volcano
            media(r, kl, [T, T, T, T, T, F, F]);
            r.i32(p.mL3C as i32);
            r.i32(p.mL40 as i32);
            r.i32(p.mUnitClassIndex as i32);
            r.i32(p.mL48 as i32);
            r.f32(p.mMaximumRadius);
            r.f32(p.mMinimumRadius);
        }
        10 => {
            // the fire
            media(r, kl, [T, T, F, F, T, F, T]);
            r.i32(p.mBurn as i32);
            r.i32(p.mDamage as i32);
            r.f32(p.mRadius);
            r.f32(p.mChance);
        }
        13 => {
            // the morale
            media(r, kl, [T, F, F, F, T, T, F]);
            r.i32(p.mEffectDuration as i32);
            r.u32(p.mPositive as u32);
        }
        14 => {
            // the time warp
            media(r, kl, [T, T, T, T, T, T, F]);
            r.i32(p.mPlacementRange as i32);
            r.i32(if hp(O::KlEffect1, kl, &[]) != 0 { p.mDepartTime as i32 } else { 0 }); // (not set without the first effect)
            r.i32(p.mAwayTime as i32);
        }
        15 => {
            // the teleport
            media(r, kl, [T, T, F, F, T, T, F]);
            r.f32(p.mDamageShare);
            r.i32(p.mPlacementRange as i32);
            r.i32(p.mHoldTime as i32);
        }
        16 | 17 | 19 => {
            // the area effects (the extra damage's and the mind control's second effect time)
            media(r, kl, [T, T, T, F, T, F, T]);
            r.i32(p.mDamageFactor as i32);
            r.u32(p.mEffect1Duration as u32);
            r.u32(p.mRadiusTiles as u32);
            r.i32(if p.mClassEnum as i32 == 16 { 0 } else { p.mEffect2Time as i32 });
        }
        21 => {
            // the logic bomb
            media(r, kl, [T, T, T, F, T, F, F]);
            r.u32(p.mEffectTime2 as u32);
            r.u32(p.mEffectTime3 as u32);
        }
        22 => {
            // the drop object
            media(r, kl, [T, T, T, F, T, F, F]);
            r.i32(p.mUnitClassIndex as i32);
        }
        23 => media(r, kl, [T, F, F, F, T, F, F]), // the cloaking
        24 => {
            // the meteor storm
            media(r, kl, [F, F, F, F, T, F, F]);
            r.f32(p.mMinimumInterval);
            r.f32(p.mMaximumInterval);
            r.f32(p.mRadius);
            r.i32(p.mUnitClassIndex as i32);
        }
        25 => {
            // the line of sight
            media(r, kl, [F; 7]);
            r.f32(p.mSightBonus);
        }
        _ => media(r, kl, [F; 7]), // special attack, conscription, location
    }
}

/// CALI: every calamity class's members (EECalamityCaptureClasses).
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_calamity_classes() {
    if !active() {
        return;
    }
    let m = hp(O::CalamityManager, 0, &[]);
    let n = h(O::MgrClassCount, m, &[]) as c_ulong;
    for i in 0..n {
        let kl = hp(O::MgrClassAt, m, &[i as i64]);
        let mut r = Record::default();
        r.u32(i as u32);
        r.u32((kl != 0) as u32);
        if kl != 0 {
            let c = *klass(kl);
            r.f32(c.mLifeTime);
            r.i32(c.mClassEnum as i32);
            r.u32(c.mClassFlags as u32);
            r.u32(c.mUnitSet as u32);
            r.i32(c.mNameID as i32);
            r.i32(c.mID as i32);
            r.i32(c.mRange as i32);
            r.i32(c.mCost as i32);
            r.u32(c.mB24 as u32 | (c.mIsBlockedByImmunity as u32) << 1 | (c.mIsCastByAllSelected as u32) << 2);
            r.i32(cc::ee_calamity_class_target_kind(c.mClassEnum as i32) as i32);
            r.i32(h(O::KlCost, kl, &[]) as i32);
            r.u32(h(O::KlUnitSet, kl, &[]) as u32);
            r.u32((hp(O::KlContagionFunctor, kl, &[]) != 0) as u32);
            class_record(&mut r, kl);
        }
        r.commit(b"CALI");
    }
}

/// The file's bytes so far: their length and the bytes.
fn file_bytes(r: &mut Record, file: P) {
    let length = h(O::MemFilePosition, file, &[]) as c_long;
    r.u32(length as u32);
    if length > 0 {
        let data = hp(O::MemFileData, file, &[]) as *const u8;
        // SAFETY: the memory file's buffer holds at least its position's bytes
        r.0.extend_from_slice(unsafe { std::slice::from_raw_parts(data, length as usize) });
    }
}

/// A calamity's numeric state as its Load leaves it (no pointers).
fn loaded_state(r: &mut Record, k: P) {
    // SAFETY: the calamity's GERectangle and GE3DPoint members, its record
    let (rect, at, shake, f) = unsafe {
        (
            *rec::<GeRectangle>(O::CalRect, k),
            *rec::<Ge3DPoint>(O::CalTargetLoc, k),
            *rec::<Ge3DPoint>(O::CalShakeLoc, k),
            *rec::<EeCalamityFields>(O::CalFields, k),
        )
    };
    for v in [rect.left, rect.top, rect.right, rect.bottom, at.x, at.y, at.z, shake.x, shake.y, shake.z] {
        r.f32(v);
    }
    let n = h(O::CalEffectGenerationCount, k, &[]);
    r.u32(n as u32);
    for i in 0..n {
        r.i32(h(O::CalEffectGenerationAt, k, &[i]) as i32);
    }
    r.u32(f.mTargetUnitID as u32);
    let kl = hp(O::CalClass, k, &[]);
    r.i32(if kl != 0 { klass(kl).mClassEnum as i32 } else { -1 });
    for v in [f.mEpicenterFactor, f.mFloatPlaceHolder1, f.mFloatPlaceHolder2, f.mHeight] {
        r.f32(v);
    }
    r.i32(f.mIntPlaceHolder1 as i32);
    r.i32(f.mIntPlaceHolder2 as i32);
    r.f32(f.mTimeAlive);
    r.u32(f.mOriginatingUnitID as u32);
    r.u32(f.mPlayerNumber as u32);
    r.f32(f.mDiscreteUpdateTime);
    r.f32(f.mTimeElapsedSinceLastUpdate);
    // SAFETY: the calamity's U2DPoint<unsigned long> and EEEffectHandle members
    let (tile, handle) = unsafe { (*rec::<[c_ulong; 2]>(O::CalTile, k), *rec::<u32>(O::CalEffectHandle, k)) };
    r.u32(tile[0] as u32);
    r.u32(tile[1] as u32);
    r.u32(handle);
    r.u32(
        f.mBoolPlaceHolder1 as u32
            | (f.mBoolPlaceHolder2 as u32) << 1
            | (f.mInitialized as u32) << 2
            | (f.mHasTargetLocation as u32) << 3
            | (f.mHasTargetUnit as u32) << 4,
    );
    let held = hp(O::CalTarget, k, &[]) != 0
        || hp(O::CalCaster, k, &[]) != 0
        || hp(O::CalSound, k, &[]) != 0
        || hp(O::CalSecondary, k, &[]) != 0;
    r.u32(held as u32);
}

/// CALK kind 0: a calamity's checksum, its save, a copy loaded from it and the copy's checksum
/// (EECalamityCaptureSave).
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_calamity_save(k: *mut c_void, index: c_ulong) {
    let k = k as P;
    if !active() {
        return;
    }
    let minor = h(O::SaveMinorVersion, 0, &[]);
    let mut r = Record::default();
    r.u32(0);
    r.u32(index as u32);
    r.i32(klass(hp(O::CalClass, k, &[])).mClassEnum as i32);
    r.u32(super::calamity_objects::ee_gamecpp_calamity_checksum(k as *mut c_void) as u32);
    let file = hp(O::MemFileNewDefault, 0, &[]);
    r.i32(h(O::CalSave, k, &[minor, pa(file)]) as i32);
    file_bytes(&mut r, file);
    h(O::MemFileRewind, file, &[]);
    let copy = hp(O::CalNewEmpty, 0, &[]);
    r.i32(h(O::CalLoad, copy, &[minor, pa(file)]) as i32);
    loaded_state(&mut r, copy);
    r.u32(super::calamity_objects::ee_gamecpp_calamity_checksum(copy as *mut c_void) as u32);
    // SAFETY: the copy's record
    unsafe { (*rec::<EeCalamityFields>(O::CalFields, copy)).mOriginatingUnitID = 0 }; // (no caster unit and an id: the destructor leaves the class's media alone)
    h(O::CalDelete, copy, &[]);
    r.commit(b"CALK");
    h(O::MemFileDelete, file, &[]);
}

/// CALK kind 1: the contagion manager's checksums, its save and a copy loaded from it (EECalamityCaptureContagionSave).
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_contagion_save() {
    if !active() {
        return;
    }
    let minor = h(O::SaveMinorVersion, 0, &[]) as c_ulong;
    let m = hp(O::ContagionRust, 0, &[]) as *mut ee_sim::tech::contagion_store::EeContagionManager;
    let mut r = Record::default();
    // SAFETY: the world's live contagion manager, our copy and memory files
    unsafe {
        r.u32(1);
        r.u32(cs::ee_contagion_manager_checksum(m) as u32);
        r.u32(cs::ee_contagion_manager_count(m) as u32);
        for i in 0..cs::ee_contagion_manager_count(m) {
            r.u32(cs::ee_contagion_helper_checksum(cs::ee_contagion_manager_at(m, i)) as u32);
        }
        let file = hp(O::MemFileNewDefault, 0, &[]);
        r.i32(cs::ee_contagion_manager_save(m, file as u64, true) as i32);
        file_bytes(&mut r, file);
        h(O::MemFileRewind, file, &[]);
        let copy = cs::ee_contagion_manager_new();
        r.i32(cs::ee_contagion_manager_load(copy, file as u64, 0, minor, true) as i32);
        r.i32(cs::ee_contagion_manager_next_id(copy) as i32);
        r.u32(cs::ee_contagion_manager_count(copy) as u32);
        for i in 0..cs::ee_contagion_manager_count(copy) {
            let helper = cs::ee_contagion_manager_at(copy, i);
            let f = cs::ee_contagion_helper_fields(helper);
            r.u32(cs::ee_contagion_manager_calamity_index(copy, i) as u32);
            r.i32(f.mHelperID as i32);
            r.f32(f.mTimeSinceLastSpread);
            r.f32(f.mTimeSinceLastUpdate);
            r.u32(cs::ee_contagion_helper_count(helper, true) as u32);
            for j in 0..cs::ee_contagion_helper_count(helper, true) {
                r.u32(cs::ee_contagion_helper_unit(helper, true, j) as u32);
            }
            r.u32(cs::ee_contagion_helper_count(helper, false) as u32);
        }
        // a helper loaded at the oldest version's layout (no update timer) from the first helper's bytes
        if cs::ee_contagion_manager_count(m) > 0 {
            let helper_file = hp(O::MemFileNewDefault, 0, &[]);
            r.i32(cs::ee_contagion_helper_save(cs::ee_contagion_manager_at(m, 0), helper_file as u64) as i32);
            h(O::MemFileRewind, helper_file, &[]);
            let mut old =
                EeContagionHelperFields { mHelperID: 0, mTimeSinceLastSpread: 0.0, mTimeSinceLastUpdate: 0.0 };
            let mut old_ids = 0usize;
            r.i32(cs::ee_contagion_helper_load_probe(helper_file as u64, 0, 0xe6, &mut old, &mut old_ids) as i32);
            r.i32(old.mHelperID as i32);
            r.f32(old.mTimeSinceLastSpread);
            r.f32(old.mTimeSinceLastUpdate);
            r.u32(old_ids as u32);
            h(O::MemFileDelete, helper_file, &[]);
        }
        cs::ee_contagion_manager_delete_helpers(copy);
        cs::ee_contagion_manager_free(copy);
        r.commit(b"CALK");
        h(O::MemFileDelete, file, &[]);
    }
}

/// CALK of every active calamity, then the contagion manager's (EECalamityCaptureSaves).
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_calamity_saves() {
    if !active() {
        return;
    }
    let m = hp(O::CalamityManager, 0, &[]);
    for i in 0..h(O::MgrCalamityCount, m, &[]) {
        h(O::CapSave, hp(O::MgrCalamityAt, m, &[i]), &[i]);
    }
    ee_gamecpp_capture_contagion_save();
}

/// CALK every five seconds of a calamity's life (its time alive crossing a multiple of 5000).
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_calamity_aged(k: *mut c_void, index: c_ulong, before: f32) {
    // SAFETY: the live calamity's record
    let alive = unsafe { (*rec::<EeCalamityFields>(O::CalFields, k as P)).mTimeAlive };
    if !active() || (before / 5000.0) as c_long == (alive / 5000.0) as c_long {
        return;
    }
    h(O::CapSave, k as P, &[index as i64]);
    ee_gamecpp_capture_contagion_save();
}

// ---- the calamity records of the runs (was EETechCapture.cpp's calamity part): CALW, CCAS, CALT, CONT

/// The event lists the hooks collect while a run's record is open (the physics thread's; a run inside a run's
/// collects into the outer one, as the C++'s file statics did).
struct Events {
    bytes: Vec<u8>,
    count: u32,
    collecting: i64,
}

impl Events {
    const fn new() -> Events {
        Events { bytes: Vec::new(), count: 0, collecting: 0 }
    }
    fn clear(&mut self) {
        self.bytes.clear();
        self.count = 0;
    }
    fn push(&mut self, op: c_long, a: c_long, b: c_long, c: f32, d: f32) {
        for v in [op as i32, a as i32, b as i32] {
            self.bytes.extend_from_slice(&v.to_le_bytes());
        }
        self.bytes.extend_from_slice(&c.to_le_bytes());
        self.bytes.extend_from_slice(&d.to_le_bytes());
        self.count += 1;
    }
    /// The record's tail: the event count and the events; the list cleared.
    fn drain_into(&mut self, r: &mut Record) {
        r.u32(self.count);
        r.0.extend_from_slice(&self.bytes);
        self.clear();
    }
}

static UPDATE_EVENTS: std::sync::Mutex<Events> = std::sync::Mutex::new(Events::new());
static START_EVENTS: std::sync::Mutex<Events> = std::sync::Mutex::new(Events::new());
static CONTAGION_EVENTS: std::sync::Mutex<Events> = std::sync::Mutex::new(Events::new());
static CAN_CAST_DEPTH: std::sync::atomic::AtomicI64 = std::sync::atomic::AtomicI64::new(0);

fn events(list: &std::sync::Mutex<Events>) -> std::sync::MutexGuard<'_, Events> {
    list.lock().unwrap_or_else(|e| e.into_inner())
}

fn unit_id(u: P) -> u32 {
    if u != 0 {
        // SAFETY: a live unit's record
        unsafe { (*rec::<ee_sim::world::unit_fields::EeUnitsRtsUnitFields>(O::UnRts, u)).mUnitID as u32 }
    } else {
        0xffff_ffff
    }
}

/// The calamity's numeric state (no pointers: whether the units are set, and their ids).
fn calamity_state(r: &mut Record, k: P) {
    // SAFETY: the calamity's members
    let (f, at, shake, tile, handle) = unsafe {
        (
            *rec::<EeCalamityFields>(O::CalFields, k),
            *rec::<Ge3DPoint>(O::CalTargetLoc, k),
            *rec::<Ge3DPoint>(O::CalShakeLoc, k),
            *rec::<[c_ulong; 2]>(O::CalTile, k),
            *rec::<u32>(O::CalEffectHandle, k),
        )
    };
    let (target, caster) = (hp(O::CalTarget, k, &[]), hp(O::CalCaster, k, &[]));
    r.f32(f.mTimeAlive);
    r.f32(f.mFloatPlaceHolder1);
    r.f32(f.mFloatPlaceHolder2);
    r.i32(f.mIntPlaceHolder1 as i32);
    r.i32(f.mIntPlaceHolder2 as i32);
    r.u32(
        f.mBoolPlaceHolder1 as u32
            | (f.mBoolPlaceHolder2 as u32) << 1
            | (f.mInitialized as u32) << 2
            | (f.mHasTargetLocation as u32) << 3
            | (f.mHasTargetUnit as u32) << 4
            | ((target != 0) as u32) << 5
            | ((caster != 0) as u32) << 6,
    );
    for v in [at.x, at.y, at.z, shake.x, shake.y, shake.z] {
        r.f32(v);
    }
    r.u32(handle);
    for v in [f.mEpicenterFactor, f.mHeight, f.mDiscreteUpdateTime, f.mTimeElapsedSinceLastUpdate] {
        r.f32(v);
    }
    r.i32(tile[0] as c_long as i32);
    r.i32(tile[1] as c_long as i32);
    r.u32(unit_id(target));
    r.u32(unit_id(caster));
    r.u32(f.mPlayerNumber as u32);
}

/// EECalamityCaptureEventHook: a world call of the update being recorded.
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_calamity_event(op: c_long, a: c_long, b: c_long, c: f32, d: f32) {
    let mut e = events(&UPDATE_EVENTS);
    if e.collecting == 0 {
        return;
    }
    e.push(op, a, b, c, d);
}

/// EECalamityCaptureUpdate: the class's Update of the calamity and, capturing, its CALW record.
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_calamity_update(kl: *mut c_void, k: *mut c_void, elapsed: f32) -> c_long {
    let (kl, k) = (kl as P, k as P);
    if !active() {
        return hf(O::KlUpdate, kl, &[pa(k)], &[elapsed]) as c_long;
    }
    let mut r = Record::default();
    r.i32(klass(kl).mClassEnum as i32);
    calamity_state(&mut r, k);
    r.f32(elapsed);
    {
        let mut e = events(&UPDATE_EVENTS);
        e.clear();
        e.collecting += 1;
    }
    let result = hf(O::KlUpdate, kl, &[pa(k)], &[elapsed]) as c_long;
    events(&UPDATE_EVENTS).collecting -= 1;
    calamity_state(&mut r, k);
    r.i32(result as i32);
    events(&UPDATE_EVENTS).drain_into(&mut r);
    r.commit(b"CALW");
    result
}

/// EECalamityCanCastEnter: whether the scope is the outermost.
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_can_cast_enter() -> bool {
    CAN_CAST_DEPTH.fetch_add(1, std::sync::atomic::Ordering::Relaxed) == 0
}

/// EECalamityCanCastLeave.
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_can_cast_leave() {
    CAN_CAST_DEPTH.fetch_sub(1, std::sync::atomic::Ordering::Relaxed);
}

#[allow(clippy::too_many_arguments)]
fn can_cast_record(
    kind: i32,
    kl: P,
    cx: P,
    at: Ge3DPoint,
    x: c_ulong,
    y: c_ulong,
    check: bool,
    target: P,
    result: c_long,
) {
    let mut r = Record::default();
    r.i32(kind);
    r.i32(klass(kl).mClassEnum as i32);
    r.u32(unit_id(cx));
    for v in [at.x, at.y, at.z] {
        r.f32(v);
    }
    r.u32(x as u32);
    r.u32(y as u32);
    r.u32(check as u32);
    r.u32(unit_id(target));
    r.i32(result as i32);
    r.commit(b"CCAS");
}

/// EECalamityCanCastRecord: an outermost CanCastAt's CCAS record (kind 0); the result.
///
/// # Safety
/// `at` is three floats.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_gamecpp_capture_can_cast_record(
    outermost: bool,
    kl: *mut c_void,
    cx: *mut c_void,
    at: *const [f32; 3],
    x: c_ulong,
    y: c_ulong,
    check: bool,
    target: *mut c_void,
    result: c_long,
) -> c_long {
    if outermost && active() {
        // SAFETY: the caller's contract
        let p = unsafe { *at };
        can_cast_record(0, kl as P, cx as P, Ge3DPoint::new(p[0], p[1], p[2]), x, y, check, target as P, result);
    }
    result
}

/// The player's `skip`-th living unit with a complex unit.
fn living_unit(pl: P, skip: i64) -> P {
    let mut skip = skip;
    let mut u = hp(O::PlFirstUnit, pl, &[]);
    while u != 0 {
        // SAFETY: a live unit's record
        let alive = unsafe { (*rec::<ee_sim::world::unit_fields::EeUnitsUnitFields>(O::UnFields, u)).mHitPoints } > 0;
        if hp(O::UnComplex, u, &[]) != 0 && alive {
            if skip == 0 {
                return u;
            }
            skip -= 1;
        }
        u = hp(O::UnNext, u, &[]);
    }
    0
}

/// CCAS kind 1: the sweep (every five game minutes): each class cast by each player's first living unit at six
/// points with four targets, checked and (for the first target at the first two points) unchecked; nature's cast for
/// the global classes.
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_calamity_sweep() {
    let m = hp(O::CalamityManager, 0, &[]);
    let world = hp(O::WorldAddr, 0, &[]);
    // SAFETY: the world's mNumPlayers
    let n = unsafe { *rec::<c_ulong>(O::WNumPlayers, world) };
    let size = h(O::MapSize, hp(O::WMap, world, &[]), &[]) as c_long;
    let tile = |u: P| {
        // SAFETY: a live unit's record
        let t = unsafe { *rec::<ee_sim::world::unit_fields::EeUnitsRtsUnitFields>(O::UnRts, u) };
        (t.mTileX, t.mTileY)
    };
    for row in 0..h(O::MgrClassCount, m, &[]) as c_ulong {
        let kl = super::calamity_objects::ee_gamecpp_calamity_class_of(m as *mut c_void, row) as P;
        if kl == 0 {
            continue;
        }
        for p in 1..n {
            let pl = hp(O::WPlayer, world, &[p as i64]);
            let other = hp(O::WPlayer, world, &[if p + 1 < n { p + 1 } else { 1 } as i64]);
            if pl == 0 || other == 0 {
                continue;
            }
            let caster = living_unit(pl, 0);
            if caster == 0 {
                continue;
            }
            let targets = [0, living_unit(other, 0), caster, living_unit(pl, 1)];
            let far = living_unit(other, 3);
            let (cx, cy) = tile(caster);
            let t1 = if targets[1] != 0 { tile(targets[1]) } else { (size / 2, size / 2) };
            let ft = if far != 0 { tile(far) } else { (1, size / 2) };
            let points: [(c_long, c_long); 6] =
                [(cx, cy), t1, (size / 2, size / 2), (cx + 3, cy - 2), ft, (size + 5, size + 5)];
            for (i, &(px, py)) in points.iter().enumerate() {
                for (t, &target) in targets.iter().enumerate() {
                    let (x, y) = (px as c_ulong, py as c_ulong);
                    let at = Ge3DPoint::new(x as c_long as f32 + 0.5, y as c_long as f32 + 0.5, 0.0);
                    let unit = hp(O::UnComplex, caster, &[]);
                    let cast = |unit: P, check: bool, target: P| {
                        h(O::KlCanCastAt, kl, &[pa(unit), cptr(&at), x as i64, y as i64, check as i64, pa(target)])
                            as c_long
                    };
                    let result = cast(unit, true, target);
                    can_cast_record(1, kl, unit, at, x, y, true, target, result);
                    if t == 1 && i < 2 {
                        let result = cast(unit, false, target);
                        can_cast_record(1, kl, unit, at, x, y, false, target, result);
                    }
                    // nature's cast (no caster): only the classes cast at no location or unit read no caster
                    if t == 0 && cc::ee_calamity_class_target_kind(klass(kl).mClassEnum as i32) == 1 {
                        let result = cast(0, true, 0);
                        can_cast_record(1, kl, 0, at, x, y, true, 0, result);
                    }
                }
            }
        }
    }
}

/// What a start or stop changes on a unit (none: its id 0xffffffff).
fn unit_state(r: &mut Record, u: P) {
    r.u32(unit_id(u));
    if u == 0 {
        return;
    }
    // SAFETY: a live unit's record
    r.i32(unsafe { (*rec::<ee_sim::world::unit_fields::EeUnitsUnitFields>(O::UnFields, u)).mHitPoints } as i32);
    let cx = hp(O::UnComplex, u, &[]);
    r.u32((cx != 0) as u32);
    if cx == 0 {
        return;
    }
    // SAFETY: a live complex unit's record
    let f = unsafe { *rec::<ee_sim::world::unit_fields::EeUnitsComplexUnitFields>(O::CxFields, cx) };
    r.u32(f.mIsDisabled as u32 | (f.mIsTinted as u32) << 1 | (f.mIsDeletable as u32) << 2);
    r.u32(f.mDisabledReason as u32);
    r.f32(f.mSight);
    r.i32(f.mPositiveMorale as i32);
    r.i32(f.mNegativeMorale as i32);
    r.u32(f.mCloaked as u32);
    r.u32(f.mCalamityFlags as u32);
    r.u32(h(O::CxFireEffect, cx, &[]) as u32);
    r.u32(f.mNumAreaEffects as u32);
}

fn start_state(r: &mut Record, k: P) {
    calamity_state(r, k);
    // SAFETY: the calamity's GERectangle member
    let rect = unsafe { *rec::<GeRectangle>(O::CalRect, k) };
    for v in [rect.left, rect.top, rect.right, rect.bottom] {
        r.f32(v);
    }
    r.u32((hp(O::CalSound, k, &[]) != 0) as u32 | ((hp(O::CalSecondary, k, &[]) != 0) as u32) << 1);
    unit_state(r, hp(O::CalTarget, k, &[]));
    unit_state(r, hp(O::CalCaster, k, &[]));
    let m = hp(O::ContagionRust, 0, &[]) as *const ee_sim::tech::contagion_store::EeContagionManager;
    // SAFETY: the world's live contagion manager
    unsafe {
        r.u32(cs::ee_contagion_manager_count(m) as u32);
        r.i32(cs::ee_contagion_manager_next_id(m) as i32);
    }
    let effects = h(O::EffectManagerPresent, 0, &[]) != 0;
    r.u32(if effects { h(O::EffCount, 0, &[]) as u32 } else { 0 });
    r.u32(if effects { h(O::EffDestroyedCount, 0, &[]) as u32 } else { 0 });
}

/// EECalamityCaptureStartEventHook: a world call of the start or stop being recorded.
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_start_event(op: c_long, a: c_long, b: c_long, c: f32, d: f32) {
    let mut e = events(&START_EVENTS);
    if e.collecting == 0 {
        return;
    }
    e.push(op, a, b, c, d);
}

/// EECalamityCaptureStartPoint: an effect's location (z as the int's bits; kCalamityStartEffect).
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_start_point(which: c_long, x: f32, y: f32, z: f32) {
    let mut e = events(&START_EVENTS);
    if e.collecting == 0 {
        return;
    }
    e.push(1, which, z.to_bits() as i32 as c_long, x, y);
}

/// EECalamityCaptureStart (`stop` false) and EECalamityCaptureStop: the class's Start or Stop and, capturing (and
/// not inside another one's record), its CALT record.
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_calamity_start(kl: *mut c_void, k: *mut c_void, stop: bool) -> bool {
    let (kl, k) = (kl as P, k as P);
    let run = |kl: P, k: P| -> bool {
        if stop {
            h(O::KlStop, kl, &[pa(k)]);
            false
        } else {
            h(O::KlStart, kl, &[pa(k)]) != 0
        }
    };
    if !active() || events(&START_EVENTS).collecting != 0 {
        return run(kl, k);
    }
    let mut r = Record::default();
    r.i32(stop as i32);
    r.i32(klass(kl).mClassEnum as i32);
    start_state(&mut r, k);
    {
        let mut e = events(&START_EVENTS);
        e.clear();
        e.collecting += 1;
    }
    let result = run(kl, k);
    events(&START_EVENTS).collecting -= 1;
    start_state(&mut r, k);
    r.i32(result as i32);
    events(&START_EVENTS).drain_into(&mut r);
    r.commit(b"CALT");
    result
}

fn helper_calamity(helper: *const ee_sim::tech::contagion_store::EeContagionHelper) -> P {
    // SAFETY: a live helper
    unsafe { cs::ee_contagion_helper_calamity(helper) as P }
}

fn helper_state(r: &mut Record, helper: *const ee_sim::tech::contagion_store::EeContagionHelper) {
    // SAFETY: a live helper; its calamity and units live
    unsafe {
        let f = cs::ee_contagion_helper_fields(helper);
        let k = helper_calamity(helper);
        r.i32(f.mHelperID as i32);
        r.f32(f.mTimeSinceLastSpread);
        r.f32(f.mTimeSinceLastUpdate);
        let kf = if k != 0 { Some(*rec::<EeCalamityFields>(O::CalFields, k)) } else { None };
        r.f32(kf.map_or(0.0, |f| f.mTimeElapsedSinceLastUpdate));
        r.f32(kf.map_or(0.0, |f| f.mDiscreteUpdateTime));
        let n = cs::ee_contagion_helper_count(helper, false);
        r.u32(n as u32);
        for i in 0..n {
            let u = cs::ee_contagion_helper_unit(helper, false, i) as P;
            r.u32(unit_id(u));
            r.i32((*rec::<ee_sim::world::unit_fields::EeUnitsUnitFields>(O::UnFields, u)).mHitPoints as i32);
            let cx = hp(O::UnComplex, u, &[]);
            r.u32(if cx != 0 {
                (*rec::<ee_sim::world::unit_fields::EeUnitsComplexUnitFields>(O::CxFields, cx)).mCalamityFlags as u32
            } else {
                0xffff_ffff
            });
        }
    }
}

/// EECalamityCaptureContagionEventHook: a world call of the helper update being recorded.
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_contagion_event(op: c_long, a: c_long, b: c_long, c: f32, d: f32) {
    let mut e = events(&CONTAGION_EVENTS);
    if e.collecting == 0 {
        return;
    }
    e.push(op, a, b, c, d);
}

/// EECalamityCaptureContagion: the helper's Update and, capturing (and not inside another one's record), its CONT
/// record.
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_contagion(helper: *mut c_void, elapsed: f32) {
    let hl = helper as *mut ee_sim::tech::contagion_store::EeContagionHelper;
    if !active() || events(&CONTAGION_EVENTS).collecting != 0 {
        hf(O::HelperUpdate, helper as P, &[], &[elapsed]);
        return;
    }
    let mut r = Record::default();
    let k = helper_calamity(hl);
    r.i32(if k != 0 { klass(hp(O::CalClass, k, &[])).mClassEnum as i32 } else { -1 });
    helper_state(&mut r, hl);
    r.f32(elapsed);
    {
        let mut e = events(&CONTAGION_EVENTS);
        e.clear();
        e.collecting += 1;
    }
    hf(O::HelperUpdate, helper as P, &[], &[elapsed]);
    events(&CONTAGION_EVENTS).collecting -= 1;
    helper_state(&mut r, hl);
    events(&CONTAGION_EVENTS).drain_into(&mut r);
    r.commit(b"CONT");
}

/// EETechCaptureCastCalamities (EE-SIM --calamities): at the k-th call, every class whose row is k modulo 8 cast by
/// each computer player's first living unit on (or at, or for) the next player's (CALI first, CALK each time).
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_cast_calamities(call: c_long) {
    if call == 0 {
        ee_gamecpp_capture_calamity_classes(); // CALI (ee/EECalamityCapture.h)
    }
    ee_gamecpp_capture_calamity_saves(); // CALK
    let m = hp(O::CalamityManager, 0, &[]);
    let world = hp(O::WorldAddr, 0, &[]);
    // SAFETY: the world's mNumPlayers
    let n = unsafe { *rec::<c_ulong>(O::WNumPlayers, world) };
    let first_living = |pl: P| living_unit(pl, 0);
    for row in 0..64 as c_ulong {
        if (row % 8) as c_long != call % 8 {
            continue;
        }
        let kl = super::calamity_objects::ee_gamecpp_calamity_class_of(m as *mut c_void, row) as P;
        if kl == 0 {
            continue;
        }
        let kind = cc::ee_calamity_class_target_kind(klass(kl).mClassEnum as i32);
        for p in 2..n {
            let pl = hp(O::WPlayer, world, &[p as i64]);
            let other = hp(O::WPlayer, world, &[if p + 1 < n { p + 1 } else { 2 } as i64]);
            if pl == 0 || other == 0 {
                continue;
            }
            let (caster, target) = (first_living(pl), first_living(other));
            if caster == 0 || target == 0 {
                continue;
            }
            // SAFETY: the live player's record
            let player =
                unsafe { (*rec::<ee_sim::world::player_fields::EeUnitsRtsPlayerFields>(O::PlRts, pl)).mPlayerID };
            let cid = unit_id(caster) as c_ulong;
            let create = super::calamity_objects::ee_gamecpp_calamity_create;
            if kind == 3 {
                create(m as *mut c_void, 2, row, cid, unit_id(target) as c_ulong, 0, 0, player);
            } else if kind == 2 {
                // SAFETY: a live unit's record
                let t = unsafe { *rec::<ee_sim::world::unit_fields::EeUnitsRtsUnitFields>(O::UnRts, target) };
                create(m as *mut c_void, 1, row, cid, 0, t.mTileX, t.mTileY, player);
            } else if kind == 1 {
                create(m as *mut c_void, 0, row, cid, 0, 0, 0, player);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_record_is_its_fields_little_endian() {
        let mut r = Record::default();
        r.u32(1);
        r.i32(-1);
        r.f32(1.0);
        assert_eq!(r.0, [1, 0, 0, 0, 0xff, 0xff, 0xff, 0xff, 0, 0, 0x80, 0x3f]);
    }
}
