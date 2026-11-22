//! `EEAEManager.cpp`'s template load and the area effects' running state in Rust over one-statement shims (UI lane
//! round 11): `Load` (the area-effect file's records into templates), `EEAESaveRunningState` (per unit its own
//! effects' location and morale count and the effects acting on it, then the active list, each effect as its owner
//! unit and index: `ee_sim::tech::area_effect_save`'s record) and `EEAELoadRunningState`'s fix-up after the load
//! (the same put back; the lost references counted).

use core::ffi::{c_long, c_ulong};

use ee_sim::tech::area_effect::EE_AREA_EFFECT_MORALE_FROM_BUILDINGS;
use ee_sim::tech::area_effect_save::*;
use ee_sim::tech::calamity_record::EeCalamityIoFfi;

use crate::screens::game_cpp::{Cpp, EeScreensShimULongOut};

/// `EEAEManager::Load(file)` on the manager: the record count, then each record (an empty slot for an id of 0; 3
/// when a template fails to initialize, as the original, until the next read's result).
pub(crate) fn load_templates<C: Cpp + ?Sized>(c: &mut C, manager: usize, file: usize) -> i32 {
    let mut count: c_ulong = 0;
    c.aes_file_open(file);
    let mut result = c.aes_file_read_count(file, EeScreensShimULongOut::of(&mut count));
    if result == 0 {
        for _ in 0..count {
            c.aes_data_clear();
            result = c.aes_data_read(file);
            let template = if c.aes_data_id() > 0 {
                let template = c.aes_template_new();
                if !c.aes_template_init(template) {
                    c.aes_template_error();
                    result = 3;
                }
                template
            } else {
                0
            };
            c.aes_templates_push(manager, template);
        }
    }
    c.aes_file_close(file);
    result
}

/// An effect as its owner unit's id and its (last) index in the owner's own list; `(-1, -1)` without an owner.
fn reference<C: Cpp + ?Sized>(c: &mut C, effect: usize) -> EeAreaEffectSavedRef {
    let owner = c.aeh_owner(effect);
    if owner == 0 {
        return EeAreaEffectSavedRef { mOwner: c_ulong::MAX, mIndex: -1 };
    }
    let mut r = EeAreaEffectSavedRef { mOwner: c.aes_unit_id(owner) as c_ulong, mIndex: -1 };
    let own = c.aes_own_count(owner);
    for i in 0..own {
        if c.aes_own_at(owner, i) == effect {
            r.mIndex = i as c_long;
        }
    }
    r
}

/// The complex unit of the unit with the id (0 without one).
fn complex_unit<C: Cpp + ?Sized>(c: &mut C, id: u64) -> usize {
    let unit = c.aes_world_unit(id);
    if unit != 0 {
        c.aes_unit_complex(unit)
    } else {
        0
    }
}

/// The effect a saved reference names (0 when its owner or index is gone).
fn effect_of<C: Cpp + ?Sized>(c: &mut C, r: EeAreaEffectSavedRef) -> usize {
    if r.mIndex < 0 {
        return 0;
    }
    let owner = complex_unit(c, r.mOwner as u64);
    if owner == 0 || r.mIndex as usize >= c.aes_own_count(owner) {
        return 0;
    }
    c.aes_own_at(owner, r.mIndex as usize)
}

/// `EEAESaveRunningState`: the record built from the players' units and the active list, written through `io`.
pub(crate) fn save<C: Cpp + ?Sized>(c: &mut C, io: EeCalamityIoFfi) {
    let state = ee_area_effect_running_new();
    let players = c.cbh_num_players();
    for p in 1..players {
        if c.aeh_player(p as i32) == 0 {
            continue;
        }
        let mut unit = c.civh_first_unit(p as i32);
        while unit != 0 {
            let u = c.aes_unit_complex(unit);
            if u != 0 && !(c.aes_own_count(u) == 0 && c.aes_affecting_count(u) == 0) {
                // SAFETY: the record made above
                unsafe { ee_area_effect_running_add_unit(state, c.aes_unit_id(u) as c_ulong) };
                for i in 0..c.aes_own_count(u) {
                    let e = c.aes_own_at(u, i);
                    let morale = c.aes_effect_kind(e) == EE_AREA_EFFECT_MORALE_FROM_BUILDINGS as i64;
                    let own = EeAreaEffectSavedOwn {
                        mX: c.aes_effect_x(e),
                        mY: c.aes_effect_y(e),
                        mIsMorale: morale,
                        mDirty: morale && c.aes_effect_dirty(e),
                        mCount: if morale { c.aes_effect_count(e) as c_long } else { 0 },
                    };
                    // SAFETY: the record made above
                    unsafe { ee_area_effect_running_add_own(state, own) };
                }
                for i in 0..c.aes_affecting_count(u) {
                    let e = c.aes_affecting_at(u, i);
                    let r = reference(c, e);
                    // SAFETY: the record made above
                    unsafe { ee_area_effect_running_add_ref(state, r, false) };
                }
            }
            unit = c.civh_next_unit(unit);
        }
    }
    let manager = c.aem_manager();
    for i in 0..c.aem_effects_count(manager) {
        let e = c.aem_effects_at(manager, i);
        let r = reference(c, e);
        // SAFETY: the record made above
        unsafe { ee_area_effect_running_add_ref(state, r, true) };
    }
    // SAFETY: the record made above, freed once written
    unsafe {
        ee_area_effect_running_save(state, io);
        ee_area_effect_running_free(state);
    }
}

#[cfg_attr(test, allow(dead_code))]
/// `EEAELoadRunningState`: the record read through `io`; when read, its fix-up registered for the loaded game's
/// first tick (the record freed after it). The read's result.
pub(crate) fn load<C: Cpp + ?Sized>(c: &mut C, io: EeCalamityIoFfi) -> c_long {
    let mut result: c_long = 0;
    // SAFETY: the caller's I/O
    let state = unsafe { ee_area_effect_running_load(io, &mut result) };
    if result == 0 {
        c.aes_after_load(state as usize);
    } else {
        // SAFETY: the record just read, not handed on
        unsafe { ee_area_effect_running_free(state) };
    }
    result
}

/// The fix-up after a load: each saved unit's own effects' location and morale count (when the unit and its count
/// of effects are the saved ones), then the effects acting on each unit, then the active list, in their saved
/// order; the lost references logged.
///
/// # Safety
/// `s` is a live running-state record.
pub(crate) unsafe fn restore<C: Cpp + ?Sized>(c: &mut C, s: *const EeAreaEffectRunning) {
    let mut lost: u64 = 0;
    // SAFETY: the caller's record
    let units = unsafe { ee_area_effect_running_count(s, false) };
    let (mut own, mut affecting) = (0usize, 0usize);
    for u in 0..units {
        // SAFETY: the caller's record
        let id = unsafe { ee_area_effect_running_unit(s, u, &mut own, &mut affecting) };
        let unit = complex_unit(c, id as u64);
        if unit == 0 || c.aes_own_count(unit) != own {
            lost += 1;
            continue;
        }
        for i in 0..own {
            // SAFETY: the caller's record
            let saved = unsafe { ee_area_effect_running_own(s, u, i) };
            let effect = c.aes_own_at(unit, i);
            c.aes_set_x(effect, saved.mX);
            c.aes_set_y(effect, saved.mY);
            if c.aes_effect_kind(effect) == EE_AREA_EFFECT_MORALE_FROM_BUILDINGS as i64 && saved.mIsMorale {
                c.aes_set_dirty(effect, saved.mDirty);
                c.aes_set_count(effect, saved.mCount as i64);
            }
        }
    }
    // the effects acting on each unit (every list is empty after a load), in their saved order
    for u in 0..units {
        // SAFETY: the caller's record
        let id = unsafe { ee_area_effect_running_unit(s, u, &mut own, &mut affecting) };
        let unit = complex_unit(c, id as u64);
        if unit == 0 {
            continue;
        }
        c.aes_affecting_clear(unit);
        for i in 0..affecting {
            // SAFETY: the caller's record
            let effect = effect_of(c, unsafe { ee_area_effect_running_ref(s, u, i) });
            if effect != 0 {
                c.aem_affecting_add(unit, effect);
            } else {
                lost += 1;
            }
        }
    }
    c.aes_clear_effects();
    // SAFETY: the caller's record
    let active = unsafe { ee_area_effect_running_count(s, true) };
    for i in 0..active {
        // SAFETY: the caller's record
        let effect = effect_of(c, unsafe { ee_area_effect_running_ref(s, usize::MAX, i) });
        if effect != 0 {
            c.aes_add_active(effect);
        } else {
            lost += 1;
        }
    }
    if lost != 0 {
        c.aes_debug_lost(lost);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn the_templates_keep_an_empty_slot_for_id_0_and_3_for_a_bad_one() {
        let mut fake = Fake::default();
        fake.out("aes_file_read_count", 2).answer("aes_data_id", &[0, 4]).answer("aes_template_new", &[70]);
        assert_eq!(load_templates(&mut fake, 1, 9), 3);
        assert!(fake.calls().ends_with(
            "aes_template_new()\naes_template_init(70)\naes_template_error()\naes_templates_push(1, 70)\n\
             aes_file_close(9)"
        ));
        assert!(fake.calls().contains("aes_templates_push(1, 0)"));
    }

    #[test]
    fn a_reference_is_the_owners_id_and_last_index() {
        let mut fake = Fake::default();
        fake.answer("aeh_owner", &[5]).answer("aes_unit_id", &[77]).answer("aes_own_count", &[3]);
        fake.answer("aes_own_at", &[40, 41, 40]);
        let r = reference(&mut fake, 40);
        assert_eq!((r.mOwner, r.mIndex), (77, 2));
        let mut fake = Fake::default();
        let r = reference(&mut fake, 40);
        assert_eq!((r.mOwner, r.mIndex), (c_ulong::MAX, -1));
    }

    #[test]
    fn a_reference_to_a_gone_owner_or_index_is_none() {
        let mut fake = Fake::default();
        assert_eq!(effect_of(&mut fake, EeAreaEffectSavedRef { mOwner: 1, mIndex: -1 }), 0);
        assert_eq!(fake.calls(), "");
        fake.answer("aes_world_unit", &[3]).answer("aes_unit_complex", &[4]).answer("aes_own_count", &[1]);
        assert_eq!(effect_of(&mut fake, EeAreaEffectSavedRef { mOwner: 1, mIndex: 1 }), 0);
    }

    /// A byte buffer behind the calamities' I/O ops (each value as 4 bytes; a bool as 1).
    struct Buf {
        bytes: Vec<u8>,
        at: usize,
    }

    #[allow(clippy::unnecessary_cast)]
    unsafe extern "C" fn buf_io(ctx: *mut core::ffi::c_void, op: u32, value: *mut core::ffi::c_void) -> c_long {
        use ee_sim::tech::calamity_record::*;
        // SAFETY: the test's buffer and the record's value
        let b = unsafe { &mut *(ctx as *mut Buf) };
        let mut take = |n: usize| -> Option<Vec<u8>> {
            let v = b.bytes.get(b.at..b.at + n)?.to_vec();
            b.at += n;
            Some(v)
        };
        unsafe {
            match op {
                EE_CALAMITY_IO_WRITE_ULONG => {
                    b.bytes.extend_from_slice(&(*(value as *const c_ulong) as u32).to_le_bytes())
                }
                EE_CALAMITY_IO_WRITE_LONG => {
                    b.bytes.extend_from_slice(&(*(value as *const c_long) as i32).to_le_bytes())
                }
                EE_CALAMITY_IO_WRITE_FLOAT => b.bytes.extend_from_slice(&(*(value as *const f32)).to_le_bytes()),
                EE_CALAMITY_IO_WRITE_BOOL => b.bytes.push(*(value as *const bool) as u8),
                EE_CALAMITY_IO_READ_ULONG => match take(4) {
                    Some(v) => *(value as *mut c_ulong) = u32::from_le_bytes([v[0], v[1], v[2], v[3]]) as c_ulong,
                    None => return 1,
                },
                EE_CALAMITY_IO_READ_LONG => match take(4) {
                    Some(v) => *(value as *mut c_long) = i32::from_le_bytes([v[0], v[1], v[2], v[3]]) as c_long,
                    None => return 1,
                },
                EE_CALAMITY_IO_READ_FLOAT => match take(4) {
                    Some(v) => *(value as *mut f32) = f32::from_le_bytes([v[0], v[1], v[2], v[3]]),
                    None => return 1,
                },
                EE_CALAMITY_IO_READ_BOOL => match take(1) {
                    Some(v) => *(value as *mut bool) = v[0] != 0,
                    None => return 1,
                },
                _ => return 1,
            }
        }
        0
    }

    #[test]
    fn the_save_and_restore_round_trip_a_units_effects() {
        // the save: one player, one unit with one morale effect acting on itself, active too
        let mut fake = Fake::default();
        fake.answer("cbh_num_players", &[2]).answer("aeh_player", &[100]).answer("civh_first_unit", &[10]);
        fake.answer("aes_unit_complex", &[11]).answer("aes_own_count", &[1; 6]).answer("aes_own_at", &[50; 4]);
        fake.answer("aes_effect_kind", &[EE_AREA_EFFECT_MORALE_FROM_BUILDINGS as i64]);
        fake.answer("aes_effect_x", &[2]).answer("aes_effect_y", &[3]).answer("aes_effect_dirty", &[1]);
        fake.answer("aes_effect_count", &[4]).answer("aes_affecting_count", &[1, 1]).answer("aes_affecting_at", &[50]);
        fake.answer("aeh_owner", &[11, 11]).answer("aes_unit_id", &[77; 3]).answer("aem_manager", &[9]);
        fake.answer("aem_effects_count", &[1]).answer("aem_effects_at", &[50]);
        let mut buf = Buf { bytes: Vec::new(), at: 0 };
        let io = EeCalamityIoFfi { ctx: &mut buf as *mut Buf as *mut core::ffi::c_void, call: Some(buf_io) };
        save(&mut fake, io);
        assert!(!buf.bytes.is_empty());
        // the load registers the fix-up; the fix-up puts the effect back
        let mut fake = Fake::default();
        fake.answer("aes_after_load", &[0]);
        let io = EeCalamityIoFfi { ctx: &mut buf as *mut Buf as *mut core::ffi::c_void, call: Some(buf_io) };
        let mut result: c_long = 0;
        // SAFETY: the buffer's I/O
        let state = unsafe { ee_area_effect_running_load(io, &mut result) };
        assert_eq!(result, 0);
        fake.answer("aes_world_unit", &[10; 4]).answer("aes_unit_complex", &[11; 4]).answer("aes_own_count", &[1; 3]);
        fake.answer("aes_own_at", &[50; 3]).answer("aes_effect_kind", &[EE_AREA_EFFECT_MORALE_FROM_BUILDINGS as i64]);
        // SAFETY: the record just read
        unsafe {
            restore(&mut fake, state);
            ee_area_effect_running_free(state);
        }
        let calls = fake.calls();
        assert!(calls.contains(
            "aes_set_x(50, 2.0)\naes_set_y(50, 3.0)\naes_effect_kind(50)\naes_set_dirty(50, true)\naes_set_count(50, 4)"
        ));
        assert!(calls.contains("aes_affecting_clear(11)"));
        assert!(calls.contains("aem_affecting_add(11, 50)"));
        assert!(calls.ends_with("aes_clear_effects()\naes_world_unit(77)\naes_unit_complex(10)\naes_own_count(11)\naes_own_at(11, 0)\naes_add_active(50)"));
    }
}
