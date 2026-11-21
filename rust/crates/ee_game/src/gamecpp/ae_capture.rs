//! TEST ONLY: the area effects' capture records (was EEAECapture.cpp; ee/EEAECapture.h; tools/sim_capture.py, every
//! profile): AETP, each area-effect template as Init left it (once, at the first tick); AEFX, per area-effect tick,
//! every unit with its own and affecting effects (each own effect's numbers and its slots' answers, each affecting
//! one as its owner and index), the active list and the running state's saved bytes. Reads the world through the
//! cases of EEAreaEffect.cpp's `EECore10OpC`; changes nothing.

use std::ffi::{c_long, c_ulong, c_void};
use std::sync::atomic::{AtomicBool, Ordering};

use ee_sim::tech::area_effect::{
    ee_area_effect_is_immunity, EeAreaEffectFields, EeAreaEffectTemplateFields, EE_AREA_EFFECT_MORALE_FROM_BUILDINGS,
};
use ee_sim::world::unit_fields::EeUnitsComplexUnitFields;

use super::ae_effect::{
    ee_gamecpp_aefx_checksum, ee_gamecpp_aefx_covers, ee_gamecpp_aefx_modify, ee_gamecpp_aefx_radius,
    ee_gamecpp_aefx_scale, ee_gamecpp_aefx_strength, for_each_players_unit,
};
use super::calamity_capture::Record;
use super::{h, hp, ptr, rec, Owned, O, P};

/// The index of a graphic effect in its table, -1 for none.
fn effect_index(g: P) -> i32 {
    if g == 0 {
        -1
    } else {
        h(O::GfxEffectIndex, g, &[]) as i32
    }
}

/// The index of a media in its table, -1 for none, -2 when not found.
fn media_index(m: P) -> i32 {
    if m == 0 {
        return -1;
    }
    let count = h(O::LcMediaCount, 0, &[]);
    (0..count).find(|&i| hp(O::LcMediaGet, 0, &[i]) == m).map_or(-2, |i| i as i32)
}

/// The template's flags as AETP packs them.
pub fn template_flags(t: &EeAreaEffectTemplateFields) -> u32 {
    (t.mB64 as u32) | (t.mB65 as u32) << 1 | (t.mB66 as u32) << 2 | (t.mB67 as u32) << 3 | (t.mB68 as u32) << 4
}

/// AETP, one record per template slot.
fn templates() {
    let manager = hp(O::AeManager, 0, &[]);
    for i in 0..h(O::AeTemplateCount, manager, &[]) {
        let t = hp(O::AeTemplateAt, manager, &[i]);
        let mut r = Record::default();
        r.u32(i as u32);
        r.u32((t != 0) as u32);
        if t != 0 {
            // SAFETY: the template's record, copied
            let f: EeAreaEffectTemplateFields = unsafe { *rec(O::AeTplFields, t) };
            for v in [f.mKind, f.mID, f.mShape, f.mMode] {
                r.i32(v as i32);
            }
            r.f32(f.mRadius);
            for v in [f.mUnitSet, f.mMaxLevel, f.mLevel, f.mL44, f.mL48, f.mL4C] {
                r.i32(v as i32);
            }
            for v in [f.mRed, f.mGreen, f.mBlue, f.mAlpha, f.mF60] {
                r.f32(v);
            }
            r.u32(template_flags(&f));
            for v in [f.mColorRed, f.mColorGreen, f.mColorBlue, f.mColorAlpha] {
                r.f32(v);
            }
            r.i32(effect_index(hp(O::LcTplGfx, t, &[])));
            r.i32(effect_index(hp(O::LcTplGfx18, t, &[])));
            r.i32(media_index(hp(O::LcTplMedia, t, &[])));
        }
        r.commit(b"AETP");
    }
}

fn list_items(list: P) -> Vec<P> {
    (0..h(O::AeListCount, list, &[])).map(|i| hp(O::AeListAt, list, &[i])).collect()
}

fn template_of(e: P) -> EeAreaEffectTemplateFields {
    // SAFETY: the effect's template's record, copied
    unsafe { *rec(O::AeTemplateFields, e) }
}

/// An effect as its owner's id (0xffffffff without one), its index in the owner's own effects (the last match, -1)
/// and its template's kind.
fn reference(r: &mut Record, e: P) {
    let owner = hp(O::AeOwner, e, &[]);
    let mut index: i32 = -1;
    if owner != 0 {
        for (i, o) in list_items(hp(O::AeUnitOwnList, owner, &[])).into_iter().enumerate() {
            if o == e {
                index = i as i32;
            }
        }
    }
    r.u32(if owner != 0 { h(O::AeUnitId, owner, &[]) as u32 } else { 0xffff_ffff });
    r.i32(index);
    r.i32(template_of(e).mKind as i32);
}

/// An own effect: its template's kind and id, player, location, checksum, strength, radius, scale, immunity, the
/// three Modify answers of 1000, the morale from buildings' count, whether it covers its owner.
fn own(r: &mut Record, e: P) {
    let t = template_of(e);
    // SAFETY: the effect's record, copied
    let f: EeAreaEffectFields = unsafe { *rec(O::AeFields, e) };
    let ev = e as *mut c_void;
    r.i32(t.mKind as i32);
    r.i32(t.mID as i32);
    r.i32(f.mPlayer as i32);
    let location = hp(O::AeLocation, e, &[]);
    // SAFETY: the effect's GE2DPoint member
    let [x, y] = unsafe { *(location as *const [f32; 2]) };
    r.f32(x);
    r.f32(y);
    r.i32(ee_gamecpp_aefx_checksum(ev) as i32);
    r.i32(ee_gamecpp_aefx_strength(ev) as i32);
    r.f32(ee_gamecpp_aefx_radius(ev));
    r.f32(ee_gamecpp_aefx_scale(ev));
    // SAFETY: the effect's record
    r.u32(unsafe { ee_area_effect_is_immunity(rec(O::AeFields, e)) } as u32);
    for kind in 1..=3 {
        let mut out: c_long = -12345;
        // SAFETY: our out value
        let modified = unsafe { ee_gamecpp_aefx_modify(ev, kind, 1000, &mut out) };
        r.u32(modified as u32);
        r.i32(out as i32);
    }
    // SAFETY: the effect's record, copied again (the slots may have changed it)
    let f: EeAreaEffectFields = unsafe { *rec(O::AeFields, e) };
    let morale = f.mKind == EE_AREA_EFFECT_MORALE_FROM_BUILDINGS;
    let (dirty, max, count) = if morale { (f.mDirty, f.mMax, f.mCount) } else { (false, 0, 0) };
    r.u32(morale as u32 | (dirty as u32) << 1);
    r.i32(max as i32);
    r.i32(count as i32);
    let owner = hp(O::AeOwner, e, &[]);
    if owner != 0 {
        let point: [f32; 2] = [x, y];
        let covers = ee_gamecpp_aefx_covers(
            ev,
            f.mPlayer as c_ulong,
            owner as *mut c_void,
            &point as *const [f32; 2] as *const c_void,
        );
        r.u32(covers as u32);
    } else {
        r.u32(2);
    }
}

static TEMPLATES_DONE: AtomicBool = AtomicBool::new(false);

/// The AEFX record of an area-effect tick (AETP first, once).
#[no_mangle]
pub extern "C" fn ee_gamecpp_ae_capture_tick(elapsed: f32) {
    if !ee_sim::app::capture::ee_app_capture_active() {
        return;
    }
    if !TEMPLATES_DONE.swap(true, Ordering::Relaxed) {
        templates();
    }
    let mut r = Record::default();
    r.f32(elapsed);
    let mut units: u32 = 0;
    for_each_players_unit(|u| {
        let own_list = list_items(hp(O::AeUnitOwnList, u, &[]));
        let affecting = list_items(hp(O::AeUnitAffectingList, u, &[]));
        if own_list.is_empty() && affecting.is_empty() {
            return;
        }
        units += 1;
        r.u32(h(O::AeUnitId, u, &[]) as u32);
        r.i32(h(O::LcCxHitPoints, u, &[]) as i32);
        // SAFETY: the unit's record, copied
        let cx: EeUnitsComplexUnitFields = unsafe { *rec(O::CxFields, u) };
        r.u32(cx.mIsCloakable as u32 | (cx.mIsCloaked as u32) << 1);
        r.u32(own_list.len() as u32);
        for e in own_list {
            own(&mut r, e);
        }
        r.u32(affecting.len() as u32);
        for e in affecting {
            reference(&mut r, e);
        }
    });
    r.u32(units);
    let active = list_items(hp(O::AeManagerEffects, hp(O::AeManager, 0, &[]), &[]));
    r.u32(active.len() as u32);
    for e in active {
        reference(&mut r, e);
    }
    let file = Owned::new(O::LcMemFileNew, O::LcMemFileDelete);
    super::civ::ee_gamecpp_ae_save_running(file.0 as *mut c_void);
    let mut length: c_long = 0;
    h(O::LcMemFilePosition, file.0, &[ptr(&mut length)]);
    r.u32(length as u32);
    if length > 0 {
        let data = hp(O::LcMemFileData, file.0, &[]);
        // SAFETY: the memory file's bytes, read before it goes
        r.bytes(unsafe { std::slice::from_raw_parts(data as *const u8, length as usize) });
    }
    r.commit(b"AEFX");
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_template_flags_pack_low_bit_first() {
        let t = EeAreaEffectTemplateFields { mB64: true, mB66: true, mB68: true, ..Default::default() };
        assert_eq!(template_flags(&t), 0b10101);
        assert_eq!(template_flags(&EeAreaEffectTemplateFields::default()), 0);
    }

    #[test]
    fn no_effect_and_no_media_are_minus_one() {
        assert_eq!(effect_index(0), -1);
        assert_eq!(media_index(0), -1);
    }
}
