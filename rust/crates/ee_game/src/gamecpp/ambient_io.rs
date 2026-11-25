//! The ambient sounds' host (game core lane, round 11, lane B; was EEAmbientSoundManager.cpp's `EEAmbientHost`,
//! `Load`, `EEAmbientSound::Init` and the world analyzer's `CountUnit`, `CountTree` and `AddTree`): ee_game app
//! ambient_ffi.rs's callback answered in Rust, the sound records read from their file and the rendered units
//! counted, each C++ call or member a one-statement case of EEGameLog.cpp's `EECore11OpB` ([`super::EeGameCpp`]'s
//! `Ea` ops). The context is the manager; a sound is its slot in the manager's vector.
//!
//! The ambient sounds run on the UI thread (EERenderObject::FillRenderLists counts the units it draws): the tour
//! exercises them, no EE-SIM gate does.
#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::{c_long, c_ulong, c_void};

use ee_sim::world::player_fields::{EeUnitsPlayerSaved, EeUnitsRtsPlayerFields};
use ee_sim::world::unit_class_fields::EeUnitsUnitClassFields;
use ee_sim::world::unit_fields::EeUnitsRtsUnitFields;

use crate::app::ambient::{EeAppAmbientCounts, EeAppAmbientRecord, EeAppAmbientSoundFields};
use crate::app::ambient_ffi::*;

use super::{h, hd, hf, hp, pa, ptr, rec, O, P};

// ee/EEUnitClassIDs.h, ee/EEUnitGoal.h, ee/EETechTreeTechID.h, ee/EEResourceType.h
const CATEGORY_LAND_CITIZEN: c_ulong = 6;
const TYPE_FARM: c_ulong = 0x2a;
const TYPE_RES_GOAT: c_ulong = 0x146;
const TYPE_RES_EAGLE: c_ulong = 0x147;
const FAMILY_BOMBER: c_ulong = 0x14;
const GOAL_ATTACK_UNIT: i64 = 2;
const EPOCH_STONE: c_long = 1;
const EPOCH_INDUSTRIAL: c_long = 8;
const EPOCH_ATOMIC_WW1: c_long = 9;
const RESOURCE_WOOD: c_long = 3;

/// `EEAmbientSoundRecord`'s size and its fields' offsets (ee/EEAmbientSound.h: 152 bytes on every target).
const RECORD_SIZE: usize = 152;
const RECORD_INDEX: usize = 0x68;

fn i32_at(r: &[u8; RECORD_SIZE], at: usize) -> i32 {
    i32::from_le_bytes([r[at], r[at + 1], r[at + 2], r[at + 3]])
}

fn f32_at(r: &[u8; RECORD_SIZE], at: usize) -> f32 {
    f32::from_bits(i32_at(r, at) as u32)
}

/// The fields `Init` copies from a database record: the sound, the volumes, the fades, the probability, the
/// interval.
pub fn record_of(r: &[u8; RECORD_SIZE]) -> EeAppAmbientRecord {
    EeAppAmbientRecord {
        sound_id: i32_at(r, 0x6c),
        maximum_volume: f32_at(r, 0x70),
        start_volume: f32_at(r, 0x74),
        fade_up: f32_at(r, 0x78),
        fade_down: f32_at(r, 0x7c),
        probability: f32_at(r, 0x80),
        interval_seconds: i32_at(r, 0x90),
    }
}

/// The ambient sounds' host (ambient_ffi.rs `EeAppAmbientFn`): the context is the manager.
///
/// # Safety
/// `context` is a live EEAmbientSoundManager and `io` the operation's record.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_ambient_host(context: *mut c_void, op: i32, io: *mut EeAppAmbientIo) {
    let m = context as P;
    // SAFETY: the caller's record for the call
    let io = unsafe { &mut *io };
    let s =
        if (EE_APP_AMBIENT_OP_SOUND..=EE_APP_AMBIENT_OP_UNLOAD_SOUND).contains(&op) && op != EE_APP_AMBIENT_OP_RANDOM {
            hp(O::EaSoundAt, m, &[i64::from(io.slot)])
        } else {
            0
        };
    let (volume, pan, pitch) = (io.volume, io.pan, io.pitch);
    match op {
        EE_APP_AMBIENT_OP_SOUNDS => io.n = h(O::EaSoundCount, m, &[]) as u32,
        EE_APP_AMBIENT_OP_SOUND => io.sound = hp(O::EaSoundFields, s, &[]) as *mut EeAppAmbientSoundFields,
        EE_APP_AMBIENT_OP_RANDOM => io.volume = hd(O::EaRandom, m, &[], &[]),
        EE_APP_AMBIENT_OP_PLAY => {
            hf(O::EaPlay, s, &[], &[volume, pan, pitch]);
        }
        EE_APP_AMBIENT_OP_HAS_INSTANCE => io.flag = h(O::EaHasInstance, s, &[]) != 0,
        EE_APP_AMBIENT_OP_START => {
            hf(O::EaStart, s, &[i64::from(io.flag)], &[volume, pan, pitch]);
        }
        EE_APP_AMBIENT_OP_SET_PARAMETERS => {
            hf(O::EaSetParameters, s, &[], &[volume, pan, pitch]);
        }
        EE_APP_AMBIENT_OP_STOP_INSTANCE => {
            h(O::EaStop, s, &[]);
            h(O::EaReleaseInstance, s, &[]);
        }
        EE_APP_AMBIENT_OP_LOAD_SOUND => {
            io.flag = h(O::EaHasSound, s, &[]) != 0;
            if !io.flag && hp(O::EaGetSound, s, &[]) != 0 {
                io.flag = true;
                h(O::EaLoadMedia, s, &[]);
                io.has = true;
                io.n = u32::from(h(O::EaLooping, s, &[]) != 0);
            }
        }
        EE_APP_AMBIENT_OP_UNLOAD_SOUND => {
            if h(O::EaHasSound, s, &[]) != 0 {
                h(O::EaUnloadMedia, s, &[]);
            }
            h(O::EaForgetSound, s, &[]);
        }
        EE_APP_AMBIENT_OP_TIME_OF_DAY => io.volume = hd(O::EaTimeOfDay, 0, &[], &[]),
        EE_APP_AMBIENT_OP_TIME => io.n = ee_base::time::ee_time_get(),
        EE_APP_AMBIENT_OP_VISIBLE_TILES => io.n = h(O::EaVisibleTiles, 0, &[]) as u32,
        EE_APP_AMBIENT_OP_VISIBLE_WATER => io.volume = hd(O::EaVisibleWater, 0, &[], &[]),
        EE_APP_AMBIENT_OP_VISIBLE_SPACE => io.volume = hd(O::EaVisibleSpace, 0, &[], &[]),
        _ => {}
    }
}

/// `Load`: the sound records from the file into their slots (a negative slot is unused) and the random generator
/// seeded with the time -> the last read's status.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ambient_load(manager: *mut c_void, file: *mut c_void) -> c_long {
    let (m, f) = (manager as P, file as P);
    let mut count: c_ulong = 0;
    let mut read: c_ulong = 0;
    let mut record = [0u8; RECORD_SIZE];
    h(O::EaFileOpen, f, &[]);
    h(O::EaSeed, m, &[]);
    let mut result = h(O::EaFileReadCount, f, &[ptr(&mut count)]);
    if result == 0 {
        h(O::EaResize, m, &[count as i64]);
        for _ in 0..count {
            result = h(O::EaFileReadBytes, f, &[ptr(&mut record), RECORD_SIZE as i64, ptr(&mut read)]);
            let slot = i32_at(&record, RECORD_INDEX);
            if slot >= 0 {
                let s = hp(O::EaSoundAt, m, &[i64::from(slot)]);
                h(O::EaForgetSound, s, &[]);
                let r = record_of(&record);
                // SAFETY: the slot's plain members and a live record
                unsafe { ee_app_ambient_sound_init(hp(O::EaSoundFields, s, &[]) as *mut EeAppAmbientSoundFields, &r) };
            }
        }
    }
    h(O::EaFileClose, f, &[]);
    result as c_long
}

/// What the analyzer counts of a visible unit's class: citizens, farms, goats, eagles.
pub fn count_class(c: &mut EeAppAmbientCounts, k: &EeUnitsUnitClassFields) {
    if k.mCategory == CATEGORY_LAND_CITIZEN {
        c.mNumCitizens += 1;
    }
    match k.mUnitType {
        TYPE_FARM => c.mNumFarms += 1,
        TYPE_RES_GOAT => c.mNumGoats += 1,
        TYPE_RES_EAGLE => c.mNumEagles += 1,
        _ => {}
    }
}

/// An attacker the ambient sounds hear: a man or horse of the stone to the industrial epoch attacking a unit.
pub fn is_attacker(goal_kind: Option<i64>, k: &EeUnitsUnitClassFields) -> bool {
    goal_kind == Some(GOAL_ATTACK_UNIT)
        && (k.mIsHuman || k.mIsHorse)
        && k.mEpoch >= EPOCH_STONE
        && k.mEpoch <= EPOCH_INDUSTRIAL
}

/// `EEAmbientWorldAnalyzer::CountUnit`: a visible unit counted (citizens, farms, goats, eagles, attackers), and an
/// enemy bomber over one of the local player's towns (from the atomic epochs on) as an air raider.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ambient_count_unit(counts: *mut EeAppAmbientCounts, unit: *mut c_void) {
    let u = unit as P;
    // SAFETY: the unit's class record, copied
    let k = unsafe { *rec::<EeUnitsUnitClassFields>(O::UcFields, hp(O::UnClass, u, &[])) };
    // SAFETY: the analyzer's counts, the caller's
    let c = unsafe { &mut *counts };
    count_class(c, &k);
    let goal = hp(O::EaGoalState, u, &[]);
    let goal_kind = (goal != 0).then(|| h(O::EaGoalKind, goal, &[]));
    if is_attacker(goal_kind, &k) {
        c.mNumAttackers += 1;
    }
    if k.mFamily != FAMILY_BOMBER || k.mIsParatrooperBomber {
        return;
    }
    let local = hp(O::DuLocalPlayer, 0, &[]);
    let owner = hp(O::UnPlayer, u, &[]);
    // SAFETY: the players' records, copied
    let id = unsafe { (*rec::<EeUnitsRtsPlayerFields>(O::PlRts, owner)).mPlayerID } as usize;
    // SAFETY: as above
    let enemies = unsafe { (*rec::<EeUnitsPlayerSaved>(O::PlSaved, local)).mEnemies };
    if owner == local || enemies[id] == 0 {
        return;
    }
    // SAFETY: the unit's record, copied
    let r = unsafe { *rec::<EeUnitsRtsUnitFields>(O::UnRts, u) };
    if h(O::EaPlayerEpoch, local, &[]) < EPOCH_ATOMIC_WW1 as i64 {
        return;
    }
    let towns = h(O::EaTownCount, local, &[]);
    for i in (0..towns).rev() {
        if h(O::EaTownHullContains, local, &[i, r.mTileX as i64, r.mTileY as i64]) != 0 {
            c.mNumAirRaiders += 1;
            break;
        }
    }
}

/// `CountTree`: a visible wood resource counted as a tree.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ambient_count_tree(counts: *mut EeAppAmbientCounts, unit: *mut c_void) {
    let u = unit as P;
    // SAFETY: the unit's class record, read at once
    let resource = unsafe { (*rec::<EeUnitsUnitClassFields>(O::UcFields, hp(O::UnClass, u, &[]))).mResourceType };
    if resource == RESOURCE_WOOD {
        ee_gamecpp_ambient_add_tree(counts, unit);
    }
}

/// `AddTree`: a visible tree counted and, while no forest type is known, the AI forest's at the unit.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ambient_add_tree(counts: *mut EeAppAmbientCounts, unit: *mut c_void) {
    // SAFETY: the analyzer's counts, the caller's (no C++ call writes them)
    let c = unsafe { &mut *counts };
    c.mNumTrees += 1;
    if c.mForestType != 0 {
        return;
    }
    let forest = hp(O::EaForestAt, 0, &[pa(unit as P)]);
    if forest != 0 {
        c.mForestType = h(O::EaForestKind, forest, &[]) as c_long;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_record_s_fields_at_their_offsets() {
        let mut r = [0u8; RECORD_SIZE];
        r[RECORD_INDEX..RECORD_INDEX + 4].copy_from_slice(&7i32.to_le_bytes());
        r[0x6c..0x70].copy_from_slice(&42i32.to_le_bytes());
        r[0x70..0x74].copy_from_slice(&0.5f32.to_le_bytes());
        r[0x80..0x84].copy_from_slice(&0.25f32.to_le_bytes());
        r[0x90..0x94].copy_from_slice(&30i32.to_le_bytes());
        assert_eq!(i32_at(&r, RECORD_INDEX), 7);
        let a = record_of(&r);
        assert_eq!((a.sound_id, a.maximum_volume, a.probability, a.interval_seconds), (42, 0.5, 0.25, 30));
        assert_eq!((a.start_volume, a.fade_up, a.fade_down), (0.0, 0.0, 0.0));
    }

    #[test]
    fn the_counted_classes_and_attackers() {
        let mut c = EeAppAmbientCounts::default();
        let mut k =
            EeUnitsUnitClassFields { mCategory: CATEGORY_LAND_CITIZEN, mUnitType: TYPE_FARM, ..Default::default() };
        count_class(&mut c, &k);
        assert_eq!((c.mNumCitizens, c.mNumFarms, c.mNumGoats), (1, 1, 0));
        k.mUnitType = TYPE_RES_EAGLE;
        count_class(&mut c, &k);
        assert_eq!((c.mNumCitizens, c.mNumEagles), (2, 1));
        k.mIsHuman = true;
        k.mEpoch = EPOCH_INDUSTRIAL;
        assert!(is_attacker(Some(GOAL_ATTACK_UNIT), &k));
        assert!(!is_attacker(None, &k));
        k.mEpoch = EPOCH_ATOMIC_WW1;
        assert!(!is_attacker(Some(GOAL_ATTACK_UNIT), &k));
    }
}
