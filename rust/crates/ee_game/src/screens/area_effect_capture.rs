//! TEST ONLY: the area effects' records of the simulation capture (`ee/EEAECapture.h`; tools/sim_capture.py, every
//! profile; was `EEAECapture.cpp`'s bodies, UI lane round 12) in Rust over the one-statement shims of that unit. It
//! reads the world and writes nothing to it:
//!   AETP  every template as `Init` left it, once at the first tick: its index, its numbers, its graphic effects and
//!         media by table index (-1 none, -2 a media not in the table)
//!   AEFX  after each `EEAEManager::Tick`: the elapsed time; per unit (players 1.. in order, each player's units in
//!         order) that owns or is affected by an effect: its id, hit points, cloak flags, each own effect (kind, id,
//!         player, location, checksum, strength, radius, scale, immunity, the three modifiers of 1000, the morale
//!         count, its cover of its own unit) and each affecting effect (as its owner's id, its index in the owner's
//!         list, its kind); the unit count; the active list the same way; the running state's save bytes.
//! The game calls it through `EEAECaptureTick`, the hook the test-only unit sets to [`tick`]'s export.

use core::ffi::c_long;
use std::sync::atomic::{AtomicBool, Ordering};

use ee_sim::tech::area_effect::{EeAreaEffectTemplateFields, EE_AREA_EFFECT_MORALE_FROM_BUILDINGS};

use crate::screens::area_effect_obj as obj;
use crate::screens::game_cpp::{Cpp, EeScreensShimLongOut, EeScreensShimOut};

fn tag(name: &[u8; 4]) -> u32 {
    u32::from_le_bytes(*name)
}

/// A record's payload (little-endian, as `EESimCaptureRecord`'s).
#[derive(Default)]
pub(crate) struct Record(pub Vec<u8>);

impl Record {
    fn u32(&mut self, v: u32) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }
    fn i32(&mut self, v: i64) {
        self.0.extend_from_slice(&(v as i32).to_le_bytes());
    }
    fn f32(&mut self, v: f32) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }
}

/// A graphic effect's table index (-1 for none).
fn effect_index<C: Cpp + ?Sized>(c: &mut C, effect: usize) -> i64 {
    if effect == 0 {
        -1
    } else {
        c.aec_effect_index(effect) as i64
    }
}

/// A media's table index (-1 for none, -2 when the table does not hold it).
fn media_index<C: Cpp + ?Sized>(c: &mut C, media: usize) -> i64 {
    if media == 0 {
        return -1;
    }
    let mut i = 0;
    while i < c.aec_media_count() {
        if c.aec_media_at(i) == media {
            return i as i64;
        }
        i += 1;
    }
    -2
}

/// An effect as its owner's id and its index in the owner's own list (the last match), and its kind.
fn reference<C: Cpp + ?Sized>(c: &mut C, r: &mut Record, effect: usize) {
    let owner = c.aeh_owner(effect);
    let mut index: i64 = -1;
    if owner != 0 {
        let mut i = 0;
        while i < c.aes_own_count(owner) {
            if c.aes_own_at(owner, i) == effect {
                index = i as i64;
            }
            i += 1;
        }
    }
    r.u32(if owner != 0 { c.aes_unit_id(owner) as u32 } else { 0xffff_ffff });
    r.i32(index);
    let t = c.ae_template(effect);
    r.i32(template_fields(c, t).mKind as i64);
}

fn template_fields<C: Cpp + ?Sized>(c: &mut C, t: usize) -> EeAreaEffectTemplateFields {
    let p = c.ae_template_fields(t) as *const EeAreaEffectTemplateFields;
    // SAFETY: the live template's record
    unsafe { *p }
}

/// An own effect's state.
fn own<C: Cpp + ?Sized>(c: &mut C, r: &mut Record, effect: usize) {
    let t = c.ae_template(effect);
    let tf = template_fields(c, t);
    let e = {
        let p = c.ae_fields(effect) as *const ee_sim::tech::area_effect::EeAreaEffectFields;
        // SAFETY: the live effect's record
        unsafe { *p }
    };
    r.i32(tf.mKind as i64);
    r.i32(tf.mID as i64);
    r.i32(e.mPlayer as i64);
    let (x, y) = (c.aes_effect_x(effect), c.aes_effect_y(effect));
    r.f32(x);
    r.f32(y);
    r.i32(c.aec_checksum(effect) as i64);
    r.i32(obj::strength(c, effect) as i64);
    r.f32(obj::radius(c, effect));
    r.f32(obj::scale(c, effect));
    r.u32(ee_sim::tech::area_effect::is_immunity(&e) as u32);
    for kind in 1..=3 {
        let out = obj::modify(c, effect, kind, 1000);
        r.u32(out.is_some() as u32);
        r.i32(out.unwrap_or(-12345) as i64);
    }
    let morale = e.mKind == EE_AREA_EFFECT_MORALE_FROM_BUILDINGS;
    let (dirty, max, count): (bool, c_long, c_long) = if morale { (e.mDirty, e.mMax, e.mCount) } else { (false, 0, 0) };
    r.u32(morale as u32 | (dirty as u32) << 1);
    r.i32(max as i64);
    r.i32(count as i64);
    let owner = c.aeh_owner(effect);
    if owner != 0 {
        r.u32(obj::covers(c, effect, e.mPlayer as u64, owner, x, y) as u32);
    } else {
        r.u32(2);
    }
}

/// AETP: every template.
fn templates<C: Cpp + ?Sized>(c: &mut C, commit: &mut impl FnMut(u32, &[u8])) {
    for i in 0..c.aec_template_count() {
        let t = c.aec_template_at(i);
        let mut r = Record::default();
        r.u32(i as u32);
        r.u32((t != 0) as u32);
        if t != 0 {
            let f = template_fields(c, t);
            for v in [f.mKind, f.mID, f.mShape, f.mMode] {
                r.i32(v as i64);
            }
            r.f32(f.mRadius);
            for v in [f.mUnitSet, f.mMaxLevel, f.mLevel, f.mL44, f.mL48, f.mL4C] {
                r.i32(v as i64);
            }
            for v in [f.mRed, f.mGreen, f.mBlue, f.mAlpha, f.mF60] {
                r.f32(v);
            }
            r.u32(
                f.mB64 as u32 | (f.mB65 as u32) << 1 | (f.mB66 as u32) << 2 | (f.mB67 as u32) << 3 | (f.mB68 as u32) << 4,
            );
            for v in [f.mColorRed, f.mColorGreen, f.mColorBlue, f.mColorAlpha] {
                r.f32(v);
            }
            let (e, e18, media) = (c.ael_t_effect(t), c.ael_t_effect18(t), c.ael_t_media(t));
            r.i32(effect_index(c, e));
            r.i32(effect_index(c, e18));
            r.i32(media_index(c, media));
        }
        commit(tag(b"AETP"), &r.0);
    }
}

/// AEFX after a tick of `elapsed` (the templates' records first, once).
/// `commit` writes a record (tag, payload).
pub(crate) fn tick<C: Cpp + ?Sized>(
    c: &mut C,
    elapsed: f32,
    templates_done: &AtomicBool,
    commit: &mut impl FnMut(u32, &[u8]),
) {
    if !c.aec_active() {
        return;
    }
    if !templates_done.swap(true, Ordering::Relaxed) {
        templates(c, commit);
    }
    let mut r = Record::default();
    r.f32(elapsed);
    let mut units = 0u32;
    let players = c.aec_num_players();
    for p in 1..players {
        let player = c.aec_player(p);
        if player == 0 {
            continue;
        }
        let mut unit = c.aec_first_unit(player);
        while unit != 0 {
            let u = c.aes_unit_complex(unit);
            if u != 0 && (c.aes_own_count(u) != 0 || c.aes_affecting_count(u) != 0) {
                units += 1;
                r.u32(c.aes_unit_id(u) as u32);
                r.i32(c.aeh_hit_points(u) as i64);
                r.u32(c.aec_cloakable(u) as u32 | (c.aec_cloaked(u) as u32) << 1);
                let n = c.aes_own_count(u);
                r.u32(n as u32);
                for i in 0..n {
                    let effect = c.aes_own_at(u, i);
                    own(c, &mut r, effect);
                }
                let n = c.aes_affecting_count(u);
                r.u32(n as u32);
                for i in 0..n {
                    let effect = c.aes_affecting_at(u, i);
                    reference(c, &mut r, effect);
                }
            }
            unit = c.aec_next_unit(unit);
        }
    }
    r.u32(units);
    let manager = c.aem_manager();
    let n = c.aem_effects_count(manager);
    r.u32(n as u32);
    for i in 0..n {
        let effect = c.aem_effects_at(manager, i);
        reference(c, &mut r, effect);
    }
    let file = c.aec_mem_new();
    c.aec_mem_save(file);
    let mut length: c_long = 0;
    c.aec_mem_position(file, EeScreensShimLongOut::of(&mut length));
    r.u32(length as u32);
    if length > 0 {
        let mut bytes = Vec::new();
        c.aec_mem_bytes(file, EeScreensShimOut::of(&mut bytes), length as usize);
        r.0.extend_from_slice(&bytes);
    }
    c.aec_mem_free(file);
    commit(tag(b"AEFX"), &r.0);
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn off_it_records_nothing() {
        let mut fake = Fake::default();
        let done = AtomicBool::new(false);
        tick(&mut fake, 2000.0, &done, &mut |_, _: &[u8]| panic!("no record"));
        assert_eq!(fake.calls(), "aec_active()");
        assert!(!done.load(Ordering::Relaxed));
    }

    #[test]
    fn the_indexes_of_effects_and_media() {
        let mut fake = Fake::default();
        fake.answer("aec_effect_index", &[7]).answer("aec_media_count", &[2, 2, 2, 2, 2, 2]);
        fake.answer("aec_media_at", &[10, 11, 10, 11]);
        assert_eq!(effect_index(&mut fake, 0), -1);
        assert_eq!(effect_index(&mut fake, 5), 7);
        assert_eq!(media_index(&mut fake, 0), -1);
        assert_eq!(media_index(&mut fake, 11), 1);
        assert_eq!(media_index(&mut fake, 12), -2);
    }

    #[test]
    fn an_empty_world_records_its_counts_and_the_save() {
        let mut fake = Fake::default();
        let done = AtomicBool::new(true);
        fake.answer("aec_active", &[1]).answer("aec_num_players", &[2]).answer("aec_player", &[0]);
        fake.answer("aem_manager", &[9]).answer("aec_mem_new", &[70]).out("aec_mem_position", 0);
        let mut records = Vec::new();
        tick(&mut fake, 2000.0, &done, &mut |t, p: &[u8]| records.push((t, p.to_vec())));
        assert_eq!(
            fake.calls(),
            "aec_active()\naec_num_players()\naec_player(1)\naem_manager()\naem_effects_count(9)\naec_mem_new()\n\
             aec_mem_save(70)\naec_mem_position(70, out)\naec_mem_free(70)"
        );
        let mut want = 2000.0f32.to_le_bytes().to_vec();
        want.extend_from_slice(&[0; 12]);
        assert_eq!(records, [(tag(b"AEFX"), want)]);
    }

    #[test]
    fn a_unit_without_effects_is_skipped_and_a_reference_names_its_owner() {
        let mut fake = Fake::default();
        fake.answer("aeh_owner", &[30]).answer("aes_own_count", &[2, 2, 2]).answer("aes_own_at", &[40, 41]);
        fake.answer("aes_unit_id", &[77]).answer("ae_template", &[20]);
        let t = EeAreaEffectTemplateFields { mKind: 3, ..Default::default() };
        fake.answer("ae_template_fields", &[&t as *const _ as i64]);
        let mut r = Record::default();
        reference(&mut fake, &mut r, 41);
        assert_eq!(r.0, [77, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0]);
    }
}
