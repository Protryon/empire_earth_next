//! The calamity objects' own bodies (game core lane, round 7): EECalamity (its construction, release, reset, sounds,
//! units and out-of-sync dump; EECalamity.cpp 0xacb75..0xadcd9), EECalamityManager (the classes' load, the casts'
//! creation, the active list's start, update and stop; EECalamityManager.cpp 0xadf97..0xaebb7), EECalamityClass
//! (its media lists, Initialize, the run's parameters, Reset, the sound, the cast tests' entry, the volcano's random
//! location; EECalamityClass.cpp 0xadcd9..0xadf97), the contagion functors' Infect, Cure and Apply
//! (EEContagionManager.cpp) and the cast tests' host (EECalamityCast.cpp). The decisions were Rust already (ee_sim
//! tech calamity_class.rs, calamity_record.rs, contagion_functor.rs, calamity_cast.rs); these are the orders of the
//! C++ calls around them, each a case of EEGameOps.cpp.
//!
//! The capture records (CALT, CALW, CALS, CALU, CALK, CCAS, CONT) are written by the calls the C++ made, in its order.

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
// the C++'s own tests, kept as written
#![allow(clippy::neg_cmp_op_on_partial_ord, clippy::bool_comparison)]

use std::ffi::{c_long, c_ulong, c_void, CString};

use ee_base::math::types::Ge3DPoint;
use ee_base::math::um::{floor, um_cos};
use ee_sim::tech::calamity_cast::{EeCalamityCast, EeCalamityCastMode};
use ee_sim::tech::calamity_class::{
    self as cc, EeCalamityClassFields, EeCalamityClassMedia, EeCalamityData, EeCalamityReset,
};
use ee_sim::tech::calamity_record::EeCalamityFields;
use ee_sim::tech::calamity_update::{EeCalamityCall, EeCalamityOp, EeCalamityReply, EeCalamityState};
use ee_sim::tech::contagion_functor::{self as cf, EeContagionFunctorFields};
use ee_sim::world::player_fields::{EeUnitsPlayerFields, EeUnitsPlayerSaved, EeUnitsRtsPlayerFields};
use ee_sim::world::unit_class_fields::EeUnitsUnitClassFields;
use ee_sim::world::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};

use super::{cptr, h, hd, hf, hp, pa, ptr, rec, O, P};

/// `kCalamityVolcano`, `kCalamitySpecialAttack`; the effect kind `kEffectDamaged`; the action `kActionIdle`.
const CAL_O_VOLCANO: c_long = 9;
const CAL_O_SPECIAL_ATTACK: c_long = 18;
const CAL_O_EFFECT_DAMAGED: i64 = 0x3e;
const CAL_O_ACTION_IDLE: i64 = 5000;
/// `kTerrainSpace` (EETerrainKinds).
const CAL_O_TERRAIN_SPACE: i32 = 0xc;

// ---- the records

fn cal<'a>(c: P) -> &'a mut EeCalamityFields {
    // SAFETY: a live calamity's record, used before the next C++ call
    unsafe { &mut *rec(O::CalFields, c) }
}

fn klass<'a>(kl: P) -> &'a mut EeCalamityClassFields {
    // SAFETY: a live class's record, as cal
    unsafe { &mut *rec(O::KlFields, kl) }
}

fn rts<'a>(u: P) -> &'a mut EeUnitsRtsUnitFields {
    // SAFETY: a live unit's record, as cal
    unsafe { &mut *rec(O::UnRts, u) }
}

fn uf<'a>(u: P) -> &'a mut EeUnitsUnitFields {
    // SAFETY: as rts
    unsafe { &mut *rec(O::UnFields, u) }
}

fn cxf<'a>(cx: P) -> &'a mut EeUnitsComplexUnitFields {
    // SAFETY: as rts
    unsafe { &mut *rec(O::CxFields, cx) }
}

fn ucf<'a>(uc: P) -> &'a EeUnitsUnitClassFields {
    // SAFETY: as rts
    unsafe { &*rec(O::UcFields, uc) }
}

fn functor<'a>(fu: P) -> &'a mut EeContagionFunctorFields {
    // SAFETY: a live functor's record, as rts
    unsafe { &mut *rec(O::FnFields, fu) }
}

fn player_id(pl: P) -> c_ulong {
    // SAFETY: a live player's record
    unsafe { (*rec::<EeUnitsRtsPlayerFields>(O::PlRts, pl)).mPlayerID }
}

fn enemy(pl: P, other: c_ulong) -> c_ulong {
    // SAFETY: as player_id
    unsafe { (*rec::<EeUnitsPlayerSaved>(O::PlSaved, pl)).mEnemies[other as usize] }
}

fn complex(u: P) -> P {
    hp(O::UnComplex, u, &[])
}

fn unit_class(u: P) -> P {
    hp(O::UnClass, u, &[])
}

fn unit_player(u: P) -> P {
    hp(O::UnPlayer, u, &[])
}

fn target(c: P) -> P {
    hp(O::CalTarget, c, &[])
}

fn caster(c: P) -> P {
    hp(O::CalCaster, c, &[])
}

fn class_of(c: P) -> P {
    hp(O::CalClass, c, &[])
}

fn target_location(c: P) -> Ge3DPoint {
    // SAFETY: the calamity's GE3DPoint member
    unsafe { *rec::<Ge3DPoint>(O::CalTargetLoc, c) }
}

fn tile_placeholder<'a>(c: P) -> &'a mut [c_ulong; 2] {
    // SAFETY: the calamity's U2DPoint<unsigned long> member
    unsafe { &mut *rec(O::CalTile, c) }
}

fn effect_handle<'a>(c: P) -> &'a mut u32 {
    // SAFETY: the calamity's EEEffectHandle
    unsafe { &mut *rec(O::CalEffectHandle, c) }
}

fn effects(kl: P) -> [P; 4] {
    [hp(O::KlEffect1, kl, &[]), hp(O::KlEffect2, kl, &[]), hp(O::KlEffect3, kl, &[]), hp(O::KlEffect4, kl, &[])]
}

/// The calamity's player: its caster's, nature's without one.
fn owner(c: P) -> P {
    let cs = caster(c);
    if cs != 0 {
        unit_player(cs)
    } else {
        hp(O::WorldPlayer, 0, &[0])
    }
}

fn is_none32(id: c_ulong) -> bool {
    id as u32 == u32::MAX
}

/// A file's text (the out-of-sync dumps).
fn puts(file: P, text: &str) {
    let t = CString::new(text).expect("no NUL");
    h(O::FilePuts, file, &[t.as_ptr() as usize as i64]);
}

// ---- EECalamity

/// `EECalamity(class, caster)` (`with_class`) and `EECalamity()`: no units or sounds, Reset; the class, the caster
/// (referenced), the class's media for the caster's player (nature without a caster).
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_construct(
    c: *mut c_void,
    kl: *mut c_void,
    caster_unit: *mut c_void,
    with_class: bool,
) {
    let c = c as P;
    h(O::CalSetTargetRaw, c, &[0]);
    h(O::CalSetCasterRaw, c, &[0]);
    h(O::CalSetSoundRaw, c, &[0]);
    h(O::CalSetSecondaryRaw, c, &[0]);
    ee_gamecpp_calamity_reset(c as *mut c_void);
    if !with_class {
        return;
    }
    h(O::CalSetClassRaw, c, &[kl as i64]);
    ee_gamecpp_calamity_set_unit(c as *mut c_void, caster_unit, false);
    let player = if !caster_unit.is_null() { unit_player(caster_unit as P) } else { hp(O::WorldPlayer, 0, &[0]) };
    h(O::KlLoadMedia, class_of(c), &[pa(player), 1]);
}

/// `~EECalamity`: the sounds stopped, the class's media unloaded unless it has a caster id but no caster unit, the
/// units released.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_destruct(c: *mut c_void) {
    let c = c as P;
    ee_gamecpp_calamity_stop_sound(c as *mut c_void, false);
    ee_gamecpp_calamity_stop_sound(c as *mut c_void, true);
    if is_none32(cal(c).mOriginatingUnitID) || caster(c) != 0 {
        h(O::KlUnloadMedia, class_of(c), &[pa(owner(c))]);
    }
    ee_gamecpp_calamity_reset(c as *mut c_void);
}

/// `StopSound` (`secondary` false) and `StopSecondarySound`: the sound stopped and released.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_stop_sound(c: *mut c_void, secondary: bool) {
    let c = c as P;
    let node = hp(if secondary { O::CalSecondary } else { O::CalSound }, c, &[]);
    if node != 0 {
        h(O::SoundStop, node, &[0]);
        ee_gamecpp_calamity_release_sound(c as *mut c_void, secondary);
    }
}

/// `ReleaseSound` and `ReleaseSecondarySound`: the sound released without stopping it.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_release_sound(c: *mut c_void, secondary: bool) {
    let c = c as P;
    let node = hp(if secondary { O::CalSecondary } else { O::CalSound }, c, &[]);
    if node != 0 {
        h(O::NodeDereference, node, &[]);
    }
    h(if secondary { O::CalSetSecondaryRaw } else { O::CalSetSoundRaw }, c, &[0]);
}

/// `Reset`: no class, target or caster (both released), the sounds released, the effect generations and handle
/// zeroed, the numbers a new calamity's (ee_tech calamity_record.rs reset).
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_reset(c: *mut c_void) {
    let c = c as P;
    h(O::CalSetClassRaw, c, &[0]);
    if target(c) != 0 {
        h(O::RefUnitDereference, target(c), &[]);
        h(O::CalSetTargetRaw, c, &[0]);
    }
    if caster(c) != 0 {
        h(O::RefUnitDereference, caster(c), &[]);
        h(O::CalSetCasterRaw, c, &[0]);
    }
    for i in 0..6 {
        h(O::CalEffectGenerationZero, c, &[i]);
    }
    ee_gamecpp_calamity_release_sound(c as *mut c_void, false);
    ee_gamecpp_calamity_release_sound(c as *mut c_void, true);
    *effect_handle(c) = 0;
    // SAFETY: the calamity's live record
    unsafe { ee_sim::tech::calamity_record::ee_calamity_record_reset(rec(O::CalFields, c)) };
}

/// The C `%6.6f` (width 6, precision 6).
fn f6(x: f32) -> String {
    format!("{:6.6}", x as f64)
}

/// `DumpOutOfSynch`: the calamity's state in the out-of-sync dump.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_dump(c: *mut c_void, file: *mut c_void) {
    let (c, file) = (c as P, file as P);
    let f = *cal(c);
    let at = target_location(c);
    let tile = *tile_placeholder(c);
    let class_enum = klass(class_of(c)).mClassEnum;
    let d = "OUT OF SYNCH DUMP:  Calamity";
    let b = |v: bool| (v == true) as i32;
    let mut s = format!("\n{d} Class is {class_enum}\n");
    s += &format!("{d} Target Location X: {}\n", f6(at.x));
    s += &format!("{d} Target Location Y: {}\n", f6(at.y));
    s += &format!("{d} Target Location Z: {}\n", f6(at.z));
    s += &format!("{d} Tile Int PlaceHolder1 X: {}\n", tile[0] as c_long);
    s += &format!("{d} Tile Int PlaceHolder1 Y: {}\n", tile[1] as c_long);
    s += &format!("{d} Integer PlaceHolder1 {}\n", f.mIntPlaceHolder1);
    s += &format!("{d} Integer PlaceHolder2 {}\n", f.mIntPlaceHolder2);
    s += &format!("{d} Target Unit ID #{:08x}\n", f.mTargetUnitID);
    s += &format!("{d} Caster Unit ID #{:08x}\n", f.mOriginatingUnitID);
    s += &format!("{d} Player Number {}\n", f.mPlayerNumber as c_long);
    s += &format!("{d} Bool PlaceHolder 1 has a value of {}\n", b(f.mBoolPlaceHolder1));
    s += &format!("{d} Bool PlaceHolder 2 has a value of {}\n", b(f.mBoolPlaceHolder2));
    s += &format!("{d} Bool Initialized has a value of {}\n", b(f.mInitialized));
    s += &format!("{d} Bool HasTargetLocation has a value of {}\n", b(f.mHasTargetLocation));
    s += &format!("{d} Bool HasTargetUnit has a value of {}\n", b(f.mHasTargetUnit));
    s += &format!("{d} Float PlaceHolder 1 is {}\n", f6(f.mFloatPlaceHolder1));
    s += &format!("{d} Float PlaceHolder 2 is {}\n", f6(f.mFloatPlaceHolder2));
    s += &format!("{d} Height is {}\n", f6(f.mHeight));
    s += &format!("{d} Discrete Update Time is {}\n", f6(f.mDiscreteUpdateTime));
    s += &format!("{d} Time Elapsed Since Last Update is {}\n", f6(f.mTimeElapsedSinceLastUpdate));
    puts(file, &s);
}

/// `GetDiscreteUpdates`: the whole discrete update periods elapsed; the remainder kept.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_discrete_updates(c: *mut c_void) -> c_ulong {
    let f = cal(c as P);
    let d = ee_sim::tech::ffi::ee_calamity_discrete_updates(f.mTimeElapsedSinceLastUpdate, f.mDiscreteUpdateTime);
    cal(c as P).mTimeElapsedSinceLastUpdate = d.elapsed;
    d.updates as c_ulong
}

/// `PlaySound`: the class's.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_play_sound(c: *mut c_void) {
    let c = c as P;
    if class_of(c) != 0 {
        h(O::KlPlaySound, class_of(c), &[pa(c)]);
    }
}

/// `SetCasterUnit` (`target` false) and `SetTargetUnit`: the reference and the unit's id moved to it (-1 when
/// cleared); a target also sets whether the calamity has one.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_set_unit(c: *mut c_void, u: *mut c_void, is_target: bool) {
    let (c, u) = (c as P, u as P);
    let (get, set) = if is_target { (O::CalTarget, O::CalSetTargetRaw) } else { (O::CalCaster, O::CalSetCasterRaw) };
    let old = hp(get, c, &[]);
    if old != 0 {
        h(O::RefUnitDereference, old, &[]);
        if is_target {
            cal(c).mTargetUnitID = !0;
        } else {
            cal(c).mOriginatingUnitID = !0;
        }
    }
    h(set, c, &[pa(u)]);
    if u != 0 {
        h(O::RefUnitReference, u, &[]);
        let id = rts(u).mUnitID;
        if is_target {
            cal(c).mTargetUnitID = id;
        } else {
            cal(c).mOriginatingUnitID = id;
        }
    }
    if is_target {
        cal(c).mHasTargetUnit = target(c) != 0;
    }
}

/// `SetSound` (`secondary` false) and `SetSecondarySound`: the current one stopped, the new one kept referenced.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_set_sound(c: *mut c_void, node: *mut c_void, secondary: bool) {
    let (c, node) = (c as P, node as P);
    ee_gamecpp_calamity_stop_sound(c as *mut c_void, secondary);
    h(if secondary { O::CalSetSecondaryRaw } else { O::CalSetSoundRaw }, c, &[pa(node)]);
    if node != 0 {
        h(O::NodeReference, node, &[]);
    }
}

// ---- EECalamityManager

fn classes(m: P) -> c_ulong {
    h(O::MgrClassCount, m, &[]) as c_ulong
}

fn active(m: P) -> usize {
    h(O::MgrCalamityCount, m, &[]) as usize
}

fn active_at(m: P, i: usize) -> P {
    hp(O::MgrCalamityAt, m, &[i as i64])
}

/// `GetCalamityClass`: the class of the enum, NULL out of range.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_class_of(m: *mut c_void, class_enum: c_ulong) -> *mut c_void {
    let m = m as P;
    if class_enum < classes(m) {
        hp(O::MgrClassAt, m, &[class_enum as i64]) as *mut c_void
    } else {
        std::ptr::null_mut()
    }
}

/// The class of the enum if its base is `kind` (the original's GetCalamityClass<T> dynamic_cast).
fn class_of_kind(m: P, class_enum: c_ulong, kind: u32) -> P {
    let kl = ee_gamecpp_calamity_class_of(m as *mut c_void, class_enum) as P;
    if kl != 0 && cc::ee_calamity_class_target_kind(klass(kl).mClassEnum as i32) == kind {
        kl
    } else {
        0
    }
}

/// A new calamity of the class cast by the caster (referenced), at the location, Reset (the bases' CreateCalamity:
/// 0xae578, 0xb6f21, 0xb70cc).
fn new_calamity(kl: P, caster_unit: P, at: Option<Ge3DPoint>, target_unit: P) -> P {
    let c = hp(O::CalNew, kl, &[pa(caster_unit)]);
    if target_unit != 0 {
        ee_gamecpp_calamity_set_unit(c as *mut c_void, target_unit as *mut c_void, true);
        let (x, y) = (rts(target_unit).mTileX, rts(target_unit).mTileY);
        // SAFETY: the calamity's GE3DPoint member
        unsafe { *rec::<Ge3DPoint>(O::CalTargetLoc, c) = Ge3DPoint::new(x as f32, y as f32, 0.0) };
    } else if let Some(at) = at {
        // SAFETY: as above
        unsafe { *rec::<Ge3DPoint>(O::CalTargetLoc, c) = at };
    }
    ee_gamecpp_calamity_class_reset(kl as *mut c_void, c as *mut c_void);
    c
}

/// `DestroyCalamities`: every active calamity stopped (CALT) and deleted (asserting that all were).
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_destroy_all(m: *mut c_void) {
    let m = m as P;
    let n = active(m);
    for i in 0..active(m) {
        let c = active_at(m, i);
        h(O::CapStop, class_of(c), &[pa(c)]); // CALT (TEST ONLY): Stop
    }
    let deleted = h(O::MgrDeleteCalamities, m, &[]);
    h(O::Verify, 0, &[(deleted as usize == n) as i64, 77]);
}

/// `LoadCalamityClasses`: the class records read, each record's class (when its kind has one) created and
/// initialized; the record count's read result.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_load_classes(m: *mut c_void, file: *mut c_void) -> c_long {
    let (m, file) = (m as P, file as P);
    let mut count: c_ulong = 0;
    h(O::FileOpenRead, file, &[]);
    let mut result = h(O::FileReadULong, file, &[ptr(&mut count)]) as c_long;
    if result == 0 {
        h(O::MgrClassesResize, m, &[count as i64]);
        for i in 0..count {
            let mut read: c_ulong = 0;
            // SAFETY: an all-zero record is a valid EeCalamityData (numbers and bools)
            let mut data: EeCalamityData = unsafe { std::mem::zeroed() };
            let size = std::mem::size_of::<EeCalamityData>() as i64;
            result = h(O::FileReadBytes, file, &[ptr(&mut data), size, ptr(&mut read)]) as c_long;
            // the class of the record's kind (none for kinds 1, 2, 5, 6, 11, 12, unknown ones or a negative id)
            let kl =
                if data.mID >= 0 && cc::ee_calamity_class_target_kind(data.mClassEnum) != cc::EE_CALAMITY_TARGET_NONE {
                    hp(O::KlNew, 0, &[])
                } else {
                    0
                };
            if kl != 0 {
                h(O::KlInitialize, kl, &[ptr(&mut data)]);
            }
            h(O::MgrClassSet, m, &[i as i64, pa(kl)]);
        }
    }
    h(O::FileClose, file, &[]);
    result
}

/// A capture record (EESimCaptureRecord: the payload's bytes, committed whole; TEST ONLY).
struct Record(u32, Vec<u8>);

impl Record {
    fn new(tag: &[u8; 4]) -> Record {
        Record(u32::from_le_bytes(*tag), Vec::new())
    }
    fn i32(mut self, v: i32) -> Record {
        self.1.extend_from_slice(&v.to_le_bytes());
        self
    }
    fn u32(mut self, v: u32) -> Record {
        self.1.extend_from_slice(&v.to_le_bytes());
        self
    }
    fn f32(mut self, v: f32) -> Record {
        self.1.extend_from_slice(&v.to_le_bytes());
        self
    }
    fn commit(self) {
        // SAFETY: our payload
        unsafe { ee_sim::app::capture::ee_app_capture_commit(self.0, self.1.as_ptr(), self.1.len()) };
    }
}

fn capture_active() -> bool {
    ee_sim::app::capture::ee_app_capture_active()
}

fn capture_cast(
    kind: i64,
    class_enum: c_ulong,
    caster_id: c_ulong,
    target_id: c_ulong,
    x: c_long,
    y: c_long,
    player: c_ulong,
    ok: bool,
) {
    if capture_active() {
        // TEST ONLY (ee/EETechCapture.h)
        h(
            O::CapCast,
            0,
            &[
                kind,
                class_enum as i64,
                caster_id as i64,
                target_id as i64,
                x as i64,
                y as i64,
                player as i64,
                ok as i64,
            ],
        );
    }
}

/// `CreateGlobalCalamity` (`kind` 0), `CreateLocationCalamity` (1, at the tile), `CreateUnitCalamity` (2, on the
/// target): a calamity of the class for the caster and player started; false if the class, tile, target or caster
/// does not exist.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_create(
    m: *mut c_void,
    kind: i32,
    class_enum: c_ulong,
    caster_id: c_ulong,
    target_id: c_ulong,
    x: c_long,
    y: c_long,
    player: c_ulong,
) -> bool {
    let m = m as P;
    let mut result = false;
    match kind {
        0 => {
            let kl = class_of_kind(m, class_enum, cc::EE_CALAMITY_TARGET_GLOBAL);
            if kl != 0 {
                let caster_unit = hp(O::WorldUnit, 0, &[caster_id as i64]);
                if caster_unit != 0 {
                    let c = new_calamity(kl, caster_unit, None, 0);
                    cal(c).mPlayerNumber = player;
                    ee_gamecpp_calamity_add(m as *mut c_void, c as *mut c_void);
                    result = true;
                }
            }
            capture_cast(0, class_enum, caster_id, 0, 0, 0, player, result);
        }
        1 => {
            let kl = class_of_kind(m, class_enum, cc::EE_CALAMITY_TARGET_LOCATION);
            if kl != 0 {
                let tile = hp(O::MapTile, 0, &[x as i64, y as i64]);
                let caster_unit = hp(O::WorldUnit, 0, &[caster_id as i64]);
                if tile != 0 && (caster_unit != 0 || is_none32(caster_id)) {
                    let z = hd(O::TileHeight, tile, &[], &[]);
                    let at = Ge3DPoint::new(x as f32 + 0.5, y as f32 + 0.5, z);
                    let c = new_calamity(kl, caster_unit, Some(at), 0);
                    cal(c).mPlayerNumber = player;
                    ee_gamecpp_calamity_add(m as *mut c_void, c as *mut c_void);
                    result = true;
                }
            }
            capture_cast(1, class_enum, caster_id, 0, x, y, player, result);
        }
        _ => {
            let kl = class_of_kind(m, class_enum, cc::EE_CALAMITY_TARGET_UNIT);
            if kl != 0 {
                let target_unit = hp(O::WorldUnit, 0, &[target_id as i64]);
                let caster_unit = hp(O::WorldUnit, 0, &[caster_id as i64]);
                if target_unit != 0 && (caster_unit != 0 || is_none32(caster_id)) {
                    let c = new_calamity(kl, caster_unit, None, target_unit);
                    cal(c).mPlayerNumber = player;
                    ee_gamecpp_calamity_add(m as *mut c_void, c as *mut c_void);
                    result = true;
                }
            }
            capture_cast(2, class_enum, caster_id, target_id, 0, 0, player, result);
        }
    }
    result
}

/// `DumpOutOfSynch`: every active calamity in the out-of-sync dump.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_dump_all(m: *mut c_void, file: *mut c_void) {
    let (m, file) = (m as P, file as P);
    puts(file, "OUT OF SYNCH DUMP:  ************************** CALAMITY MANAGER DUMP **************************\n\n");
    for i in 0..active(m) {
        ee_gamecpp_calamity_dump(active_at(m, i) as *mut c_void, file as *mut c_void);
    }
}

/// `GetCalamityIndex` (`classes` false) and `GetCalamityClassIndex`: the object's index in the list (written only
/// when it is there); false if absent.
///
/// # Safety
/// `out` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_calamity_index(
    m: *mut c_void,
    object: *mut c_void,
    in_classes: bool,
    out: *mut c_long,
) -> bool {
    let m = m as P;
    let n = if in_classes { classes(m) as usize } else { active(m) };
    for i in 0..n {
        let o = if in_classes { hp(O::MgrClassAt, m, &[i as i64]) } else { active_at(m, i) };
        if o == object as P {
            // SAFETY: the caller's contract
            unsafe { *out = i as c_long };
            return true;
        }
    }
    false
}

/// `GetCalamityType`: the class's target kind (written only when the class exists); false if it does not.
///
/// # Safety
/// `out` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_calamity_type(m: *mut c_void, class_enum: c_ulong, out: *mut c_long) -> bool {
    let kl = ee_gamecpp_calamity_class_of(m, class_enum) as P;
    if kl == 0 {
        return false;
    }
    // SAFETY: the caller's contract
    unsafe { *out = cc::ee_calamity_class_target_kind(klass(kl).mClassEnum as i32) as c_long };
    true
}

/// `GetOutOfSynchChecksum`: the sum of the active calamities' (a long, wrapping).
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_checksum_all(m: *mut c_void) -> c_long {
    let m = m as P;
    let mut sum: c_long = 0;
    for i in 0..active(m) {
        sum = sum.wrapping_add(ee_gamecpp_calamity_checksum(active_at(m, i) as *mut c_void));
    }
    sum
}

/// `EECalamity::GetOutOfSynchChecksum` (ee_tech calamity_record.rs checksum).
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_checksum(c: *mut c_void) -> c_long {
    let c = c as P;
    let at = target_location(c);
    let tile = *tile_placeholder(c);
    // SAFETY: the calamity's live record
    unsafe {
        ee_sim::tech::calamity_record::ee_calamity_record_checksum(
            rec(O::CalFields, c),
            at.x,
            at.y,
            at.z,
            tile[0],
            tile[1],
        )
    }
}

/// `IsCalamityActive`: a calamity of the class is active.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_is_active(m: *mut c_void, class_enum: c_long) -> bool {
    let m = m as P;
    (0..active(m)).any(|i| klass(class_of(active_at(m, i))).mClassEnum == class_enum)
}

/// `LoadMedia` (`load`) and `UnloadMedia` of the class of the enum for the player.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_class_media(m: *mut c_void, pl: *mut c_void, class_enum: c_ulong, load: bool) {
    let m = m as P;
    if !(class_enum < classes(m)) {
        return;
    }
    let kl = hp(O::MgrClassAt, m, &[class_enum as i64]);
    if kl != 0 {
        if load {
            h(O::KlLoadMedia, kl, &[pl as i64, 0]);
        } else {
            h(O::KlUnloadMedia, kl, &[pl as i64]);
        }
    }
}

/// `Restore`: the loaded calamities replace the active ones; saves before 0.30 drop kinds 7, 8 and 10 (not deleted).
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_restore(m: *mut c_void, major: c_ulong, minor: c_ulong, list: *mut c_void) {
    let (m, list) = (m as P, list as P);
    ee_gamecpp_calamity_destroy_all(m as *mut c_void);
    for i in 0..h(O::VecCalamityCount, list, &[]) {
        let c = hp(O::VecCalamityAt, list, &[i]);
        if c != 0 {
            let e = klass(class_of(c)).mClassEnum;
            if (e != 7 && e != 8 && e != 10) || major != 0 || minor >= 30 {
                h(O::MgrCalamityPush, m, &[pa(c)]);
            }
        }
    }
}

/// `AddCalamity`: the calamity started (CALT, CALS) and kept active, or deleted when it does not start (CALK on
/// its start).
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_add(m: *mut c_void, c: *mut c_void) {
    let (m, c) = (m as P, c as P);
    let started = h(O::CapStart, class_of(c), &[pa(c)]) != 0; // CALT (TEST ONLY): Start
    if capture_active() {
        // TEST ONLY (ee/EETechCapture.h)
        Record::new(b"CALS")
            .i32(klass(class_of(c)).mClassEnum as i32)
            .u32(cal(c).mPlayerNumber as u32)
            .f32(cal(c).mTimeAlive)
            .u32(started as u32)
            .commit();
    }
    if started {
        h(O::MgrCalamityPush, m, &[pa(c)]);
        h(O::CapSave, c, &[active(m) as i64 - 1]); // CALK (TEST ONLY): each calamity at its start
    } else {
        h(O::CalDelete, c, &[]); // releases its caster/target references and the media its ctor loaded
    }
}

/// `DestroyCalamityClasses`: the active calamities destroyed, every class deleted (asserting that all were).
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_destroy_classes(m: *mut c_void) {
    let m = m as P;
    let n = classes(m);
    ee_gamecpp_calamity_destroy_all(m as *mut c_void);
    let deleted = h(O::MgrDeleteClasses, m, &[]);
    h(O::Verify, 0, &[(deleted as c_ulong == n) as i64, 690]);
}

/// `Update`: every player's earthquake-shaken flag cleared, each active calamity updated (CALW, CALU, CALK) and the
/// ones that report 7 (done) stopped (CALT) and deleted.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_update_all(m: *mut c_void, elapsed: f32) {
    let m = m as P;
    let world = hp(O::WorldAddr, 0, &[]);
    // SAFETY: the world's mNumPlayers
    let n = unsafe { *rec::<c_ulong>(O::WNumPlayers, world) };
    for i in 1..n {
        let pl = hp(O::WPlayer, world, &[i as i64]);
        // SAFETY: the live player's record
        unsafe { (*rec::<EeUnitsPlayerFields>(O::PlFields, pl)).mEarthquakeShaken = false };
    }
    let mut i = 0;
    while i < active(m) {
        let c = active_at(m, i);
        let before = cal(c).mTimeAlive;
        let update = hf(O::CapUpdate, class_of(c), &[pa(c)], &[elapsed]) as c_long; // CALW (TEST ONLY)
        if capture_active() {
            // TEST ONLY (ee/EETechCapture.h)
            Record::new(b"CALU")
                .i32(klass(class_of(c)).mClassEnum as i32)
                .u32(cal(c).mPlayerNumber as u32)
                .f32(before)
                .f32(elapsed)
                .f32(cal(c).mTimeAlive)
                .f32(klass(class_of(c)).mLifeTime)
                .i32(update as i32)
                .commit();
        }
        hf(O::CapAged, c, &[i as i64], &[before]); // CALK (TEST ONLY)
        if update == 7 {
            let c = active_at(m, i);
            h(O::CapStop, class_of(c), &[pa(c)]); // CALT (TEST ONLY): Stop
            h(O::CalDelete, c, &[]);
            h(O::MgrCalamityErase, m, &[i as i64]);
        } else {
            i += 1;
        }
    }
}

/// `EECreateCalamityUnit`: a unit of the class at the transformation for the player (-1: the calamity's); NULL if
/// the class does not exist.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_create_unit(
    c: *mut c_void,
    unit_class_index: c_long,
    t: *mut c_void,
    player: c_long,
) -> *mut c_void {
    let mut number = cal(c as P).mPlayerNumber as c_long;
    if player != -1 {
        number = player;
    }
    let class = hp(O::UcAt, 0, &[unit_class_index as i64]);
    if class == 0 {
        return std::ptr::null_mut();
    }
    hp(O::UcCreateUnitAt, class, &[t as i64, number as i64]) as *mut c_void
}

// ---- EECalamityClass

/// `GetDefaultTarget`: a unit class's: the id of the unit under the mouse, -1 if none; -1 for the others.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_class_default_target(kl: *mut c_void) -> c_long {
    let kl = kl as P;
    let mut id: c_long = -1;
    if cc::ee_calamity_class_target_kind(klass(kl).mClassEnum as i32) != cc::EE_CALAMITY_TARGET_UNIT {
        return id;
    }
    let u = hp(O::PickUnitAtMouse, 0, &[]);
    if u != 0 {
        id = rts(u).mUnitID as c_long;
        h(O::RefUnitDereference, u, &[]);
    }
    id
}

/// One of the class's media lists (ee_tech calamity_class.rs media_step) for LoadMedia, UnloadMedia or the dtor.
fn media_steps(kl: P, list: u32, pl: P, on_demand: bool) {
    let fx = effects(kl);
    let class_enum = klass(kl).mClassEnum as i32;
    let load = list == cc::EE_CALAMITY_MEDIA_LOAD;
    let mut i = 0;
    loop {
        let step = cc::ee_calamity_class_media_step(class_enum, list, i);
        i += 1;
        if step == cc::EE_CALAMITY_STEP_END {
            break;
        }
        let index = klass(kl).mUnitClassIndex as i64;
        if (cc::EE_CALAMITY_STEP_EFFECT1..cc::EE_CALAMITY_STEP_EFFECT1 + 4).contains(&step) {
            let e = fx[(step - cc::EE_CALAMITY_STEP_EFFECT1) as usize];
            if list == cc::EE_CALAMITY_MEDIA_DESTROY {
                if e != 0 {
                    h(O::GfxEffectDereference, e, &[]);
                }
            } else if e != 0 && load {
                h(O::GfxEffectLoadPtr, e, &[on_demand as i64]);
            } else if e != 0 {
                h(O::GfxEffectUnloadPtr, e, &[]);
            }
        } else if list == cc::EE_CALAMITY_MEDIA_DESTROY {
            if step == cc::EE_CALAMITY_STEP_FUNCTOR {
                h(O::KlDeleteFunctor, kl, &[]);
                h(O::KlSetFunctor, kl, &[0]);
            } else if step == cc::EE_CALAMITY_STEP_SAMPLE && hp(O::KlSound, kl, &[]) != 0 {
                h(O::SoundUnloadSample, hp(O::KlSound, kl, &[]), &[]);
            }
        } else if step == cc::EE_CALAMITY_STEP_SOUND || step == cc::EE_CALAMITY_STEP_SECONDARY_SOUND {
            let sound = hp(if step == cc::EE_CALAMITY_STEP_SOUND { O::KlSound } else { O::KlSecondary }, kl, &[]);
            if sound != 0 && load {
                h(O::SoundObjectLoadPtr, sound, &[]);
            } else if sound != 0 {
                h(O::SoundObjectUnloadPtr, sound, &[]);
            }
        } else if step == cc::EE_CALAMITY_STEP_UNIT_MEDIA && load {
            if on_demand == true {
                h(O::PlLoadClassMedia, pl, &[index]);
            }
            h(O::PlSetClassMedia, pl, &[index, 0x7f]);
        } else if step == cc::EE_CALAMITY_STEP_UNIT_MEDIA {
            h(O::PlUnloadClassMedia, pl, &[index]);
        } else if step == cc::EE_CALAMITY_STEP_UNIT_BUTTONS && load {
            h(O::PlLoadButtons, pl, &[index]);
        } else if step == cc::EE_CALAMITY_STEP_UNIT_BUTTONS {
            h(O::PlUnloadClassButtons, pl, &[index]);
        }
    }
}

/// `~EECalamityClass` (`list` 2), `LoadMedia` (0) and `UnloadMedia` (1): the class's media list in its order.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_class_media_list(kl: *mut c_void, list: u32, pl: *mut c_void, on_demand: bool) {
    media_steps(kl as P, list, pl as P, on_demand);
}

/// `GetMedia`: the class's effects (whether set, duration, size, delay) and sounds for the Rust tables.
fn media(kl: P) -> EeCalamityClassMedia {
    let mut m = EeCalamityClassMedia::default();
    for (i, &e) in effects(kl).iter().enumerate() {
        if e == 0 {
            continue;
        }
        m.present |= cc::EE_CALAMITY_MEDIA_EFFECT1 << i;
        m.duration[i] = h(O::GfxEffectDuration, e, &[]) as u32;
        m.size[i] = hd(O::GfxEffectSize, e, &[], &[]);
        m.time1[i] = h(O::GfxEffectTime1, e, &[]) as u32;
    }
    if hp(O::KlSound, kl, &[]) != 0 {
        m.present |= cc::EE_CALAMITY_MEDIA_SOUND;
    }
    if hp(O::KlSecondary, kl, &[]) != 0 {
        m.present |= cc::EE_CALAMITY_MEDIA_SECONDARY_SOUND;
    }
    m
}

/// `GetMedia` for the C++.
///
/// # Safety
/// `out` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_calamity_class_get_media(kl: *mut c_void, out: *mut EeCalamityClassMedia) {
    // SAFETY: the caller's contract
    unsafe { *out = media(kl as P) };
}

/// `Initialize`: the common fields, the class's effects and sounds fetched as it fetched them, its numbers, its
/// contagion functor (ee_tech calamity_class.rs).
///
/// # Safety
/// `data` is the class's db record.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_calamity_class_initialize(kl: *mut c_void, data: *const EeCalamityData) {
    let kl = kl as P;
    // SAFETY: the caller's contract
    let d = unsafe { *data };
    // SAFETY: the class's live record
    unsafe { cc::ee_calamity_class_initialize_base(rec(O::KlFields, kl), &d) };
    let ids = [
        d.mGraphicEffectID1,
        d.mGraphicEffectID2,
        d.mGraphicEffectID3,
        d.mGraphicEffectID4,
        d.mSoundID,
        d.mSecondarySoundID,
    ];
    let setters = [O::KlSetEffect1, O::KlSetEffect2, O::KlSetEffect3, O::KlSetEffect4];
    for (i, &id) in ids.iter().enumerate() {
        if !cc::ee_calamity_class_fetches(cc::ee_calamity_class_get_mode(d.mClassEnum, i as u32), id) {
            continue;
        }
        if i < 4 {
            let e = hp(O::GfxEffectGet, 0, &[id as i64]);
            h(setters[i], kl, &[pa(e)]);
            if e != 0 {
                h(O::GfxEffectReference, e, &[]);
            }
        } else if i == 4 {
            h(O::KlSetSound, kl, &[pa(hp(O::SoundObjectGet, 0, &[id as i64]))]);
        } else {
            h(O::KlSetSecondary, kl, &[pa(hp(O::SoundObjectGet, 0, &[id as i64]))]);
        }
    }
    let m = media(kl);
    // SAFETY: as above
    unsafe { cc::ee_calamity_class_fill(rec(O::KlFields, kl), &d, &m) };
    let kind = cc::ee_calamity_class_functor_kind(d.mClassEnum);
    if kind != cc::EE_CALAMITY_FUNCTOR_NONE {
        h(O::KlSetFunctor, kl, &[pa(hp(O::FnNew, kl, &[kind as i64]))]);
    }
}

/// `Start` (`which` 0), `Stop` (1), `Update` (2): the class's parameters and sounds (ee_tech calamity_class.rs run),
/// the run in Rust over the C++ run's host (ee/EECalamityUpdate.h).
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_class_run(kl: *mut c_void, c: *mut c_void, which: u32, elapsed: f32) -> c_long {
    let (kl, c) = (kl as P, c as P);
    let mut m = media(kl);
    if which == cc::EE_CALAMITY_RUN_UPDATE {
        let variant = options_variant();
        m.quake_damage_factor = hd(O::VariantQuakeFactor, 0, &[variant], &[]);
    }
    // SAFETY: the class's live record
    let run = unsafe { cc::ee_calamity_class_run(rec(O::KlFields, kl), &m, which) };
    if which == cc::EE_CALAMITY_RUN_STOP && !run.runs {
        return 0;
    }
    let sound = if run.sounds & cc::EE_CALAMITY_RUN_SOUND != 0 { hp(O::KlSound, kl, &[]) } else { 0 };
    let secondary = if run.sounds & cc::EE_CALAMITY_RUN_SECONDARY_SOUND != 0 { hp(O::KlSecondary, kl, &[]) } else { 0 };
    super::calamity_io::class_run(which, kl, c, &run.params, [sound, secondary], elapsed)
}

/// The world's game variant (`mGameOptions.mVariant`).
fn options_variant() -> i64 {
    let world = hp(O::WorldAddr, 0, &[]);
    // SAFETY: the world's options record
    unsafe { (*rec::<crate::app::game_options::EeAppOptionsFields>(O::WOptions, world)).mVariant as i64 }
}

/// `Reset` of a new calamity: its placeholders (ee_tech calamity_class.rs reset), the time warp's target tile, the
/// fire's and the plague's contagion helper on the target.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_class_reset(kl: *mut c_void, c: *mut c_void) {
    let (kl, c) = (kl as P, c as P);
    let mut r = EeCalamityReset::default();
    let t = target(c);
    r.has_target = t != 0;
    if r.has_target && complex(t) != 0 {
        let cx = complex(t);
        r.target_tile_x = rts(cx).mTileX;
        r.target_tile_y = rts(cx).mTileY;
        r.target_hit_points = uf(cx).mHitPoints;
    }
    let f = *cal(c);
    r.bool1 = f.mBoolPlaceHolder1;
    r.bool2 = f.mBoolPlaceHolder2;
    r.float1 = f.mFloatPlaceHolder1;
    r.int1 = f.mIntPlaceHolder1;
    r.int2 = f.mIntPlaceHolder2;
    let tile = *tile_placeholder(c);
    r.tile_x = tile[0] as c_long;
    r.tile_y = tile[1] as c_long;
    // SAFETY: our record
    unsafe { cc::ee_calamity_class_reset(klass(kl).mClassEnum as i32, &mut r) };
    let f = cal(c);
    f.mBoolPlaceHolder1 = r.bool1;
    f.mBoolPlaceHolder2 = r.bool2;
    f.mFloatPlaceHolder1 = r.float1;
    f.mIntPlaceHolder1 = r.int1;
    f.mIntPlaceHolder2 = r.int2;
    *tile_placeholder(c) = [r.tile_x as c_ulong, r.tile_y as c_ulong];
    if r.create_helper {
        let id = h(O::ContagionCreate, hp(O::KlFunctor, kl, &[]), &[pa(c), pa(t)]);
        cal(c).mIntPlaceHolder1 = id as c_long;
    }
}

/// The Windows `max(a, b)`: `(a > b) ? a : b`.
fn wmax(a: f32, b: f32) -> f32 {
    if a > b {
        a
    } else {
        b
    }
}

/// `UpdateSound`: the calamity's main or secondary sound for its target location with the falloff; a volume other
/// than -1 replaces the computed one (clamped to 0..1).
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_class_update_sound(
    c: *mut c_void,
    sound: *mut c_void,
    volume: f32,
    falloff: f32,
    primary: bool,
) {
    let (c, sound) = (c as P, sound as P);
    let at = target_location(c);
    let node = hp(if primary { O::CalSound } else { O::CalSecondary }, c, &[]);
    if node == 0 {
        return;
    }
    let (mut vol, mut pan, mut pitch) = (0f32, 0f32, 0f32);
    let base = hd(O::SoVolume, sound, &[], &[]);
    hf(O::SoParams, 0, &[cptr(&at), ptr(&mut vol), ptr(&mut pan), ptr(&mut pitch)], &[base, falloff]);
    if volume != -1.0 {
        vol = if 1.0 < volume { 1.0 } else { wmax(0.0, volume) };
    }
    hf(O::SoSetParams, node, &[], &[vol, pan, pitch]);
}

/// `PlaySound`: the earthquake's, hurricane's and volcano's looping main sound, the meteor storm's once at the
/// target (ee_tech calamity_class.rs sound_mode); nothing for the others.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_class_play_sound(kl: *mut c_void, c: *mut c_void) {
    let (kl, c) = (kl as P, c as P);
    let mode = cc::ee_calamity_class_sound_mode(klass(kl).mClassEnum as i32);
    let sound = hp(O::KlSound, kl, &[]);
    if mode == cc::EE_CALAMITY_PLAY_NODE {
        let node = hf(O::SoPlayNode, sound, &[], &[hd(O::SoVolume, sound, &[], &[]), 0.0, 1.0]) as P;
        ee_gamecpp_calamity_set_sound(c as *mut c_void, node as *mut c_void, false);
        let n = hp(O::CalSound, c, &[]);
        if n != 0 {
            h(O::NodeDereference, n, &[]);
        }
    } else if mode == cc::EE_CALAMITY_PLAY_AT_TARGET {
        let at = target_location(c);
        let (mut vol, mut pan, mut pitch) = (0f32, 0f32, 0f32);
        hf(
            O::SoCalamityParams,
            0,
            &[cptr(&at), ptr(&mut vol), ptr(&mut pan), ptr(&mut pitch)],
            &[hd(O::SoVolume, sound, &[], &[])],
        );
        hf(O::SoPlayOnce, sound, &[], &[vol, pan, pitch]);
    }
}

/// `CanCastAt`: the rules of the class's base (Rust, ee_tech calamity_cast.rs), recorded by the CCAS scope.
///
/// # Safety
/// `at` is the location (three floats).
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_calamity_class_can_cast(
    kl: *mut c_void,
    cx: *mut c_void,
    at: *const [f32; 3],
    x: c_ulong,
    y: c_ulong,
    check: bool,
    target_unit: *mut c_void,
) -> c_long {
    let kl = kl as P;
    let kind = cc::ee_calamity_class_target_kind(klass(kl).mClassEnum as i32);
    let mode = if kind == cc::EE_CALAMITY_TARGET_GLOBAL {
        EeCalamityCastMode::Global
    } else if kind == cc::EE_CALAMITY_TARGET_LOCATION {
        EeCalamityCastMode::Location
    } else if klass(kl).mClassEnum == CAL_O_SPECIAL_ATTACK {
        EeCalamityCastMode::SpecialAttack
    } else {
        EeCalamityCastMode::Unit
    };
    let scope = hp(O::CanCastScopeNew, 0, &[]); // CCAS (TEST ONLY)
    let result = can_cast_at(kl, mode, cx as P, x, y, check, target_unit as P);
    let recorded = h(
        O::CanCastScopeRecord,
        scope,
        &[pa(kl), cx as i64, at as i64, x as i64, y as i64, check as i64, target_unit as i64, result as i64],
    );
    h(O::CanCastScopeDelete, scope, &[]);
    recorded as c_long
}

/// `IsValidLocation` of a location class: the shared rule over the class's radius, water flag, map area, space
/// terrain and placed unit class.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_class_valid_location(
    kl: *mut c_void,
    cx: *mut c_void,
    x: c_ulong,
    y: c_ulong,
) -> bool {
    let kl = kl as P;
    let mut cast = cast_of(kl, cx as P, x, y);
    // SAFETY: the class's live record; our cast
    let unit_class_index =
        unsafe { cc::ee_calamity_class_cast_fields(rec(O::KlFields, kl), &mut cast, CAL_O_TERRAIN_SPACE) };
    valid_location(kl, &cast, cx as P, unit_class_index)
}

/// `IsValidTarget` of a unit class: the fire's and the plague's functor decides (CanInfect), the others' the shared
/// rule.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_class_valid_target(
    kl: *mut c_void,
    cx: *mut c_void,
    target_unit: *mut c_void,
) -> bool {
    let kl = kl as P;
    if cc::ee_calamity_class_targets_by_functor(klass(kl).mClassEnum as i32) {
        return h(O::FnCanInfect, hp(O::KlFunctor, kl, &[]), &[target_unit as i64]) != 0;
    }
    valid_target(kl, cx as P, target_unit as P)
}

/// `GetRandomLocation` (the volcano's): a random location of a map tile between the minimum and maximum radius
/// around the centre, at the tile's height.
///
/// # Safety
/// `center` and `out` are three floats each.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_calamity_class_random_location(
    kl: *mut c_void,
    center: *const [f32; 3],
    out: *mut [f32; 3],
) {
    let kl = kl as P;
    // SAFETY: the caller's contract
    let (center, o) = unsafe { (*center, *out) };
    let center = Ge3DPoint::new(center[0], center[1], center[2]);
    let mut o = Ge3DPoint::new(o[0], o[1], o[2]);
    let mut tile;
    loop {
        let angle = hd(O::PhysGenerate, 0, &[], &[]) * 6.2831855 - 3.1415927;
        let fraction = hd(O::PhysGenerate, 0, &[], &[]);
        let cos = um_cos(angle);
        let sin = um_cos(1.5707964 - angle);
        let f = *klass(kl);
        let distance = (1.0 - fraction) * f.mMinimumRadius + fraction * f.mMaximumRadius;
        o.x = cos * distance;
        o.y = sin * distance;
        o.x += center.x;
        o.y += center.y;
        o.z += center.z;
        tile = hp(O::MapTile, 0, &[floor(o.x) as i64, floor(o.y) as i64]);
        if tile != 0 {
            break;
        }
    }
    o.z = hd(O::TileHeight, tile, &[], &[]);
    // SAFETY: the caller's contract
    unsafe { *out = [o.x, o.y, o.z] };
}

/// `Place`: the volcano makes units step aside from its site at the tile; nothing for the other classes.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_class_place(kl: *mut c_void, u: *mut c_void, x: c_ulong, y: c_ulong) {
    let kl = kl as P;
    if klass(kl).mClassEnum != CAL_O_VOLCANO {
        return;
    }
    let class = hp(O::UcAt, 0, &[klass(kl).mUnitClassIndex as i64]);
    ee_sim::units::uu_ffi::ee_units_uu_step_aside(
        0,
        class as u64,
        unit_player(u as P) as u64,
        x as c_long as i32,
        y as c_long as i32,
        1,
    );
}

// ---- the contagion functors

/// `EEContagionFunctor(class, kind)`: the numbers from the class's record (ee_tech contagion_functor.rs init), the
/// effects each kind kept.
#[no_mangle]
pub extern "C" fn ee_gamecpp_functor_construct(fu: *mut c_void, kl: *mut c_void, kind: u32) {
    let (fu, kl) = (fu as P, kl as P);
    // SAFETY: the live functor's and class's records
    unsafe { cf::ee_contagion_functor_init(rec(O::FnFields, fu), rec(O::KlFields, kl), kind) };
    h(O::FnSetEffect1, fu, &[0]);
    h(O::FnSetEffect2, fu, &[0]);
    h(O::FnSetEffect3, fu, &[0]);
    if kind == cc::EE_CALAMITY_FUNCTOR_FIRE {
        h(O::FnSetEffect1, fu, &[pa(hp(O::KlEffect2, kl, &[]))]);
        h(O::GfxEffectReference, hp(O::FnEffect1, fu, &[]), &[]);
    } else if kind == cc::EE_CALAMITY_FUNCTOR_MIND_CONTROL {
        h(O::FnSetEffect1, fu, &[pa(hp(O::KlEffect3, kl, &[]))]);
    } else if kind == cc::EE_CALAMITY_FUNCTOR_AREA {
        h(O::FnSetEffect2, fu, &[pa(hp(O::KlEffect2, kl, &[]))]);
        h(O::FnSetEffect3, fu, &[pa(hp(O::KlEffect3, kl, &[]))]);
    }
}

/// `Update`: the whole discrete update periods elapsed on the calamity for this update's damage.
#[no_mangle]
pub extern "C" fn ee_gamecpp_functor_update(fu: *mut c_void, elapsed: f32, c: *mut c_void) {
    let (fu, c) = (fu as P, c as P);
    // SAFETY: the live functor's record
    cal(c).mDiscreteUpdateTime = unsafe { cf::ee_contagion_functor_discrete_time(rec(O::FnFields, fu)) };
    cal(c).mTimeElapsedSinceLastUpdate += elapsed;
    functor(fu).mUpdates = ee_gamecpp_calamity_discrete_updates(c as *mut c_void);
}

fn contagion_event(op: i64, a: i64, b: i64) {
    hf(O::CapContagionEvent, 0, &[op, a, b], &[0.0, 0.0]); // CONT (TEST ONLY)
}

/// `kContagionInfect`, `kContagionCure`, `kContagionDamage` (ee/EETechCapture.h).
const CONT_DAMAGE: i64 = 7;
const CONT_INFECT: i64 = 8;
const CONT_CURE: i64 = 9;

/// `Infect`: the kind's effect on a newly infected unit (the fire's burn, the leprosy's and plague's effects, the
/// area effect's target effect, the extra damage's morale, the mind control's hold).
#[no_mangle]
pub extern "C" fn ee_gamecpp_functor_infect(fu: *mut c_void, u: *mut c_void, c: *mut c_void) {
    let (fu, u, c) = (fu as P, u as P, c as P);
    contagion_event(CONT_INFECT, rts(u).mUnitID as i64, 0);
    let cx = complex(u);
    let kind = functor(fu).mKind;
    match kind {
        cc::EE_CALAMITY_FUNCTOR_FIRE => {
            // alight: the burning effect once, then burn percent of its maximum hit points as damage unless carried
            let max_hit_points = h(O::UnMaxHitPointsI32, u, &[]) as i32;
            let damage = ee_sim::tech::ffi::ee_contagion_fire_burn(functor(fu).mBurn as i32, max_hit_points);
            if cx != 0 {
                h(O::CxSetIsDeletable, cx, &[0]);
            }
            if h(O::CxFireEffect, cx, &[]) == 0 {
                let mut handle = 0u32;
                h(O::EffUnit, cx, &[pa(hp(O::FnEffect1, fu, &[])), ptr(&mut handle), 0, 0, 0]);
                h(O::CxSetFireEffect, cx, &[handle as i64]);
            }
            if cx == 0 || hp(O::CxOutermostCarrier, cx, &[]) == cx {
                contagion_event(CONT_DAMAGE, rts(u).mUnitID as i64, damage as i64);
                let mode = functor(fu).mMode as i64;
                h(O::UnTakeHit, u, &[pa(caster(c)), damage as i64, cal(c).mPlayerNumber as i64, mode]);
            }
        }
        cc::EE_CALAMITY_FUNCTOR_LEPROSY | cc::EE_CALAMITY_FUNCTOR_PLAGUE => {
            // the unit class's leprosy or plague effect; the plague keeps the unit from being deleted
            let e = hp(
                if kind == cc::EE_CALAMITY_FUNCTOR_LEPROSY { O::UcLeprosyEffect } else { O::UcPlagueEffect },
                unit_class(u),
                &[],
            );
            if e != 0 {
                h(O::EffUnit, u, &[pa(e), 0, 0, 0, 0]);
            }
            if kind == cc::EE_CALAMITY_FUNCTOR_PLAGUE && cx != 0 {
                h(O::CxSetIsDeletable, cx, &[0]);
            }
        }
        cc::EE_CALAMITY_FUNCTOR_AREA => {
            // the target effect at the calamity's target on the ground (at height 0 on water terrain types 1 and 10)
            let mut at = target_location(c);
            let tile = hp(O::MapTile, 0, &[floor(at.x) as i64, floor(at.y) as i64]);
            let terrain = h(O::TileTerrainType, tile, &[]);
            if terrain != 1 && terrain != 10 {
                at.z = hd(O::MeshZ, 0, &[], &[at.x, at.y]);
            } else {
                at.z = 0.0;
            }
            let e = hp(O::FnEffect2, fu, &[]);
            if e != 0 {
                h(O::EffAt, u, &[cptr(&at), pa(e), 0, 0]);
            }
        }
        cc::EE_CALAMITY_FUNCTOR_EXTRA_DAMAGE => {
            // an enemy of the caster (any unit without a caster): its class's pulse cannon or battle cry effect, one
            // more negative morale
            let cs = caster(c);
            if cs != 0 {
                let (up, cp) = (unit_player(u), unit_player(cs));
                if up == cp || enemy(cp, player_id(up)) == 0 {
                    return;
                }
            }
            let e =
                hp(if functor(fu).mFlag { O::UcPulseCannonEffect } else { O::UcBattleCryEffect }, unit_class(u), &[]);
            if e != 0 {
                h(O::EffUnit, u, &[pa(e), 0, 0, 0, 0]);
            }
            let f = cxf(cx);
            f.mNegativeMorale = f.mNegativeMorale.wrapping_add(1);
        }
        cc::EE_CALAMITY_FUNCTOR_MIND_CONTROL => {
            // the effect on a caught unit, disabled (reason 0x1a) and halted
            let e = hp(O::FnEffect1, fu, &[]);
            if e != 0 {
                h(O::EffUnit, u, &[pa(e), 0, 0, 0, 0]);
            }
            h(O::CxSetDisabled, cx, &[1, 0x1a]);
            h(O::CxHalt, cx, &[]);
        }
        _ => {}
    }
}

/// `Cure`: the kind's effect undone (the fire put out, the plague's deletable flag, the extra damage's morale, the
/// mind control's release); nothing for the leprosy and the area effects.
#[no_mangle]
pub extern "C" fn ee_gamecpp_functor_cure(fu: *mut c_void, u: *mut c_void, c: *mut c_void) {
    let (fu, u) = (fu as P, u as P);
    let _ = c;
    let kind = functor(fu).mKind;
    if kind == cc::EE_CALAMITY_FUNCTOR_LEPROSY || kind == cc::EE_CALAMITY_FUNCTOR_AREA {
        return;
    }
    contagion_event(CONT_CURE, rts(u).mUnitID as i64, 0);
    let cx = complex(u);
    match kind {
        cc::EE_CALAMITY_FUNCTOR_FIRE => {
            // the fire put out: the deletable flag restored, the burning effect finished or removed
            if cx == 0 {
                return;
            }
            h(O::CxSetIsDeletable, cx, &[ucf(unit_class(u)).mIsDeletable as i64]);
            let handle = h(O::CxFireEffect, cx, &[]);
            if handle == 0 {
                return;
            }
            let e = hp(O::EffGet, 0, &[handle]);
            if e != 0 {
                if h(O::EffKindOf, e, &[]) == CAL_O_EFFECT_DAMAGED {
                    h(O::EffFinishDamaged, e, &[]); // the damage controller (0x3e, on EEE2DRust)
                } else {
                    h(O::EffDestroy, 0, &[handle]);
                }
            }
            h(O::CxSetFireEffect, cx, &[0]);
        }
        cc::EE_CALAMITY_FUNCTOR_PLAGUE => {
            if cx != 0 {
                h(O::CxSetIsDeletable, cx, &[ucf(unit_class(u)).mIsDeletable as i64]);
            }
        }
        cc::EE_CALAMITY_FUNCTOR_EXTRA_DAMAGE => {
            let f = cxf(cx);
            f.mNegativeMorale = f.mNegativeMorale.wrapping_sub(1);
        }
        cc::EE_CALAMITY_FUNCTOR_MIND_CONTROL => {
            // re-enabled; a living one first loses its build targets, goals and orders and goes idle (handing the
            // unit back to its owner is not done here)
            if cx == 0 {
                return;
            }
            if uf(cx).mHitPoints > 0 {
                h(O::CxClearBuildTargets, cx, &[]);
                let mut i: c_ulong = 0;
                while i < h(O::CxNumGoalQueues, cx, &[]) as c_ulong {
                    h(O::CxClearGoalQueue, cx, &[i as i64]);
                    i += 1;
                }
                h(O::CxClearGoals, cx, &[0]);
                h(O::CxSetDisabled, cx, &[0, 0]);
                h(O::CxStartAction, cx, &[CAL_O_ACTION_IDLE]);
                h(O::UcActionChanged, unit_class(u), &[pa(cx)]);
            } else {
                h(O::CxSetDisabled, cx, &[0, 0]);
            }
        }
        _ => {}
    }
}

/// `Apply`: the unit damaged per elapsed period (ee_tech contagion_functor.rs apply_damage); the fire and the plague
/// keep it from being deleted (the fire before, unless it is carried; the plague after), the area effect shows its
/// hit effect.
#[no_mangle]
pub extern "C" fn ee_gamecpp_functor_apply(fu: *mut c_void, u: *mut c_void, elapsed: f32, c: *mut c_void) {
    let (fu, u, c) = (fu as P, u as P, c as P);
    let _ = elapsed;
    let cx = complex(u);
    let kind = functor(fu).mKind;
    if kind == cc::EE_CALAMITY_FUNCTOR_FIRE && cx != 0 {
        h(O::CxSetIsDeletable, cx, &[0]);
    }
    let attack = if kind == cc::EE_CALAMITY_FUNCTOR_AREA { hd(O::UnStatsAttack, u, &[], &[]) } else { 0.0 };
    // SAFETY: the live functor's record
    let hit = unsafe { cf::ee_contagion_functor_apply_damage(rec(O::FnFields, fu), uf(u).mHitPoints as i32, attack) };
    if hit.hits && (kind != cc::EE_CALAMITY_FUNCTOR_FIRE || cx == 0 || hp(O::CxOutermostCarrier, cx, &[]) == cx) {
        let e = hp(O::FnEffect3, fu, &[]);
        if kind == cc::EE_CALAMITY_FUNCTOR_AREA && e != 0 {
            h(O::EffUnit, u, &[pa(e), 0, 0, 0, 0]);
        }
        contagion_event(CONT_DAMAGE, rts(u).mUnitID as i64, hit.damage as i64);
        let mode = functor(fu).mMode as i64;
        h(O::UnTakeHit, u, &[pa(caster(c)), hit.damage as i64, cal(c).mPlayerNumber as i64, mode]);
    }
    if kind == cc::EE_CALAMITY_FUNCTOR_PLAGUE && cx != 0 {
        h(O::CxSetIsDeletable, cx, &[0]);
    }
}

// ---- the cast tests' host (was EECalamityCast.cpp's CastCall)

/// The cast tests' run: the class, the caster, the call's target, the unit with its id (TargetLookup), the
/// hurricane's and the volcano's unit class, the C++ unit list of the last search.
#[repr(C)]
struct CastHost {
    class: P,
    unit: P,
    target_unit: P,
    target: P,
    unit_class_index: c_long,
    units: P,
}

/// `TileVisible` etc.: the cast's flags of the target (`EE_CALAMITY_TARGET_*` of calamity_cast.rs).
fn target_view(host: &CastHost) -> u32 {
    use ee_sim::tech::calamity_cast as k;
    let t = host.target_unit;
    let unit = host.unit;
    let cx = complex(t);
    let tp = unit_player(t);
    let mut flags = 0;
    if hp(O::UnTileNode, t, &[]) != 0 {
        flags |= k::EE_CALAMITY_TARGET_ON_MAP;
    }
    if t == unit {
        flags |= k::EE_CALAMITY_TARGET_IS_CASTER;
    }
    if unit != 0 && tp == unit_player(unit) {
        flags |= k::EE_CALAMITY_TARGET_SAME_PLAYER;
    }
    if unit != 0 && enemy(unit_player(unit), player_id(unit_player(t))) != 0 {
        flags |= k::EE_CALAMITY_TARGET_ENEMY;
    }
    let unit_type = ucf(unit_class(t)).mUnitType as i64;
    if h(O::UnitSetContains, 0, &[unit_type, h(O::KlUnitSet, host.class, &[])]) != 0 {
        flags |= k::EE_CALAMITY_TARGET_IN_SET;
    }
    if h(O::UnIsFoundation, t, &[]) != 0 {
        flags |= k::EE_CALAMITY_TARGET_FOUNDATION;
    }
    if cx != 0 && cxf(cx).mIsBuilt {
        flags |= k::EE_CALAMITY_TARGET_BUILT;
    }
    if cx != 0 && cxf(cx).mIsDisabled {
        flags |= k::EE_CALAMITY_TARGET_DISABLED;
    }
    if ucf(unit_class(t)).mUnitType == UNIT_TYPE_VOLCANO {
        flags |= k::EE_CALAMITY_TARGET_VOLCANO;
    }
    flags
}

/// `kUnitTypeVolcano`, `kUnitTypeHurricane`, `kUnitTypeAntiMatterStorm`, `kUnitTypeInf10RadioMan`.
const UNIT_TYPE_VOLCANO: c_ulong = 0x16e;
const UNIT_TYPE_HURRICANE: c_ulong = 0xb6;
const UNIT_TYPE_ANTI_MATTER_STORM: c_ulong = 0x294;
const UNIT_TYPE_RADIO_MAN: c_ulong = 0x2ef;

unsafe extern "C" fn cast_call(
    ctx: *mut c_void,
    _: *mut EeCalamityState,
    call: *const EeCalamityCall,
    reply: *mut EeCalamityReply,
) {
    // SAFETY: the run's host (ours) and the caller's call and reply
    let (host, c) = unsafe { (&mut *(ctx as *mut CastHost), *call) };
    let mut r = EeCalamityReply::default();
    let unit = host.unit;
    let (x, y) = (c.index as c_long as c_ulong, c.index2 as c_long as c_ulong);
    use EeCalamityOp as C;
    match c.op {
        // the handles the world views read through (calamity_views.rs: the caster's hit points and power, the tiles)
        C::CasterHandle => r.key = unit as u64,
        C::WorldHandle => r.key = hp(O::WorldAddr, 0, &[]) as u64,
        C::CasterPlayerIndex => {
            let world = hp(O::WorldAddr, 0, &[]);
            r.value = crate::gamecpp::world::ee_gamecpp_world_player_index(
                world as *mut c_void,
                unit_player(unit) as *mut c_void,
            ) as i32;
        }
        C::TileVisible => {
            r.ok = h(O::TileVisibleTo, hp(O::MapTile, 0, &[x as i64, y as i64]), &[pa(unit_player(unit))]) != 0
        }
        C::AreaEffect => r.ok = h(O::AeHas2, 0, &[3, x as i64, y as i64, c.value as i64]) != 0,
        C::TargetLookup => {
            let id: c_ulong = if host.target_unit != 0 { rts(host.target_unit).mUnitID } else { !0 };
            host.target = hp(O::WorldUnit, 0, &[id as i64]);
            r.ok = host.target != 0;
        }
        C::ValidLocation => {
            r.ok = ee_gamecpp_calamity_class_valid_location(
                host.class as *mut std::ffi::c_void,
                unit as *mut std::ffi::c_void,
                x as c_ulong,
                y as c_ulong,
            )
        }
        C::ValidTarget => {
            r.ok = ee_gamecpp_calamity_class_valid_target(
                host.class as *mut std::ffi::c_void,
                unit as *mut std::ffi::c_void,
                host.target as *mut std::ffi::c_void,
            )
        }
        C::TargetView => r.flags = target_view(host),
        C::AreaOnMap => r.ok = h(O::MapAreaOnMap, 0, &[x as i64, y as i64, c.value as u32 as i64]) != 0,
        C::ValidTile => r.ok = h(O::MeshValidTile, 0, &[x as i64, y as i64]) != 0,
        C::SpaceMap => r.ok = h(O::MeshIsSpaceMap, 0, &[]) != 0,
        C::CasterContinent => r.value = h(O::ContinentOfUnit, unit, &[]) as i32,
        C::TileContinent => r.value = h(O::ContinentOfTile, 0, &[x as i64, y as i64]) as i32,
        C::TileBlocked => r.ok = h(O::WorldTileBlocked, 0, &[x as i64, y as i64]) != 0,
        C::InnerSphere => r.x = hd(O::UcInnerSphere, hp(O::UcAt, 0, &[host.unit_class_index as i64]), &[], &[]),
        C::UnitsNear => {
            let at = [c.x, c.y, c.z];
            // SAFETY: the run's C++ unit vector
            let near = unsafe { ee_sim::units::uu_ffi::ee_units_uu_units_near(&at, c.w, host.units as *mut c_void) };
            r.value = near;
            r.id = h(O::VecUnitsCount, host.units, &[]) as u32;
        }
        C::NearStorm => {
            let near = hp(O::VecUnitsAt, host.units, &[c.index as i64]);
            let t = ucf(unit_class(near)).mUnitType;
            r.ok = uf(near).mHitPoints > 0 && (t == UNIT_TYPE_HURRICANE || t == UNIT_TYPE_ANTI_MATTER_STORM);
        }
        C::TileSize => r.value = ucf(hp(O::UcAt, 0, &[host.unit_class_index as i64])).mTileSize as i32,
        C::EdgeTile => r.ok = h(O::MapIsEdgeTile, 0, &[c.index as i64, c.index2 as i64]) != 0,
        C::TileUnblocked => {
            let player = if unit != 0 { unit_player(unit) } else { hp(O::WorldPlayer, 0, &[0]) };
            let tile = hp(O::MapTile, 0, &[x as i64, y as i64]);
            r.ok = ee_sim::units::uu_ffi::ee_units_uu_is_tile_unblocked(
                player as u64,
                host.unit_class_index as i32,
                tile as u64,
            ) != 0;
        }
        C::CasterIsRadioMan => r.ok = ucf(unit_class(unit)).mUnitType == UNIT_TYPE_RADIO_MAN,
        C::TileFree => r.ok = h(O::TileFree, 0, &[x as i64, y as i64]) != 0,
        _ => {
            h(O::Verify, 0, &[0, 1]);
        }
    }
    // SAFETY: the caller's reply
    unsafe { *reply = r };
}

fn cast_of(kl: P, cx: P, x: c_ulong, y: c_ulong) -> EeCalamityCast {
    let f = klass(kl);
    EeCalamityCast {
        mode: EeCalamityCastMode::Base,
        class_enum: f.mClassEnum as i32,
        x: x as u32,
        y: y as u32,
        has_caster: cx != 0,
        blocked_by_immunity: f.mIsBlockedByImmunity,
        cost: 0,
        radius: 0,
        area: 0,
        space_terrain: 0,
        check: false,
        over_water: false,
    }
}

fn with_cast_host<R>(host: CastHost, run: impl FnOnce(&ee_sim::tech::ffi::EeCalamityHostFfi) -> R) -> R {
    let mut host = host;
    host.units = hp(O::VecUnitsNew, 0, &[]);
    let ffi = ee_sim::tech::ffi::EeCalamityHostFfi { ctx: &mut host as *mut CastHost as *mut c_void, call: cast_call };
    let r = run(&ffi);
    h(O::VecUnitsDelete, host.units, &[]);
    r
}

/// `EECalamityCanCastAt`: CanCastAt as the override of `mode` runs it (0 allowed, 1 refused, 2 protected).
fn can_cast_at(kl: P, mode: EeCalamityCastMode, cx: P, x: c_ulong, y: c_ulong, check: bool, target_unit: P) -> c_long {
    let mut cast = cast_of(kl, cx, x, y);
    cast.mode = mode;
    cast.check = check;
    cast.cost = h(O::KlCost, kl, &[]) as i32;
    let host = CastHost { class: kl, unit: cx, target_unit, target: 0, unit_class_index: 0, units: 0 };
    // SAFETY: our cast and host
    with_cast_host(host, |ffi| unsafe { ee_sim::tech::ffi::ee_calamity_can_cast_at(&cast, ffi) }) as c_long
}

fn valid_target(kl: P, cx: P, target_unit: P) -> bool {
    let cast = cast_of(kl, cx, 0, 0);
    let host = CastHost { class: kl, unit: cx, target_unit, target: target_unit, unit_class_index: 0, units: 0 };
    // SAFETY: our cast and host
    with_cast_host(host, |ffi| unsafe { ee_sim::tech::ffi::ee_calamity_valid_target(&cast, ffi) })
}

fn valid_location(kl: P, cast: &EeCalamityCast, cx: P, unit_class_index: c_long) -> bool {
    let host = CastHost { class: kl, unit: cx, target_unit: 0, target: 0, unit_class_index, units: 0 };
    // SAFETY: our cast and host
    with_cast_host(host, |ffi| unsafe { ee_sim::tech::ffi::ee_calamity_valid_location(cast, ffi) })
}

/// `EECalamityCanCastAt` for the C++ callers (ee/EECalamityUpdate.h).
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_can_cast_at(
    kl: *mut c_void,
    mode: EeCalamityCastMode,
    cx: *mut c_void,
    x: c_ulong,
    y: c_ulong,
    check: bool,
    target_unit: *mut c_void,
) -> c_long {
    can_cast_at(kl as P, mode, cx as P, x, y, check, target_unit as P)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_dump_formats_are_the_c_library_s() {
        assert_eq!(f6(1.5), "1.500000");
        assert_eq!(f6(-0.25), "-0.250000");
        assert_eq!(f6(12345.0), "12345.000000");
        assert_eq!(format!("{:08x}", 0x2a_u32), "0000002a");
        assert!(is_none32(!0));
        assert!(!is_none32(5));
        assert_eq!(wmax(f32::NAN, 0.0), 0.0);
    }

    #[test]
    fn the_capture_tags_are_little_endian_fourccs() {
        assert_eq!(u32::from_le_bytes(*b"CALS"), 0x534c4143);
        assert_eq!(CONT_CURE - CONT_INFECT, 1);
    }
}
