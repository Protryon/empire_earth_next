//! The graphic effects' tables (EEGraphicEffect.cpp 0xd6bed..0xd72c1; the graphics + LLE cluster's third round):
//! the effects by table index (`sEffects`), every loaded graphic object filed under its kind (`sMediaByKind`, 0x48
//! kinds: the kind-only effects draw one), the ids of the effects used before their graphic was loaded (`sIDs`,
//! the media dump's), and the effects' `Load` / `Unload` (a use counted, the graphic or every graphic of the kind
//! loaded, then the three follow-on effects).
//!
//! The effects and graphic objects stay C++ objects (their members read through the registered layout,
//! effect_reads.rs); their media loads are the `EEMedia` virtuals, asked of [`EeGfxGeffectHost`]. No lock is held
//! across a call into C++: each walk copies its list first.

use std::collections::BTreeSet;
use std::ffi::{c_long, c_ulong, c_void};
use std::sync::{Mutex, RwLock};

use ee_data::media::EeDataMediaUse;
use ee_data::tables::{EeDataGraphicEffectFields, EeDataGraphicObjectFields};

use crate::gfx::effect_reads::layout;

/// The kinds a graphic object is filed under (`sMediaByKind[0x48]`).
pub const EE_GFX_GEFFECT_KINDS: usize = 0x48;

/// The C++ calls of the loads: an `EEMedia*`'s virtual `LoadMedia` and `UnloadMedia`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeGfxGeffectHost {
    pub media_load: extern "C" fn(media: *mut c_void),
    pub media_unload: extern "C" fn(media: *mut c_void),
}

static EFFECTS: RwLock<Vec<usize>> = RwLock::new(Vec::new());
static BY_KIND: RwLock<Vec<Vec<usize>>> = RwLock::new(Vec::new());
static IDS: Mutex<BTreeSet<c_ulong>> = Mutex::new(BTreeSet::new());

// ---- the tables (no C++ object)

/// The kinds' lists, filed in media table order: each graphic of a kind below 0x48 (`kinds[i]`: None for an empty
/// slot of the media table).
pub fn file_by_kind(media: &[(usize, Option<c_ulong>)]) -> Vec<Vec<usize>> {
    let mut by_kind = vec![Vec::new(); EE_GFX_GEFFECT_KINDS];
    for &(m, kind) in media {
        if let Some(k) = kind.filter(|&k| (k as usize) < EE_GFX_GEFFECT_KINDS) {
            by_kind[k as usize].push(m);
        }
    }
    by_kind
}

fn kind_list(kind: c_long) -> Vec<usize> {
    let by_kind = BY_KIND.read().unwrap();
    usize::try_from(kind).ok().and_then(|k| by_kind.get(k)).cloned().unwrap_or_default()
}

// ---- the C++ objects

#[derive(Clone, Copy)]
struct Effect(usize);

impl Effect {
    fn fields(self) -> EeDataGraphicEffectFields {
        // SAFETY: a live effect's record base (the layout's)
        unsafe {
            std::ptr::read_unaligned((self.0 + layout().geffect_fields as usize) as *const EeDataGraphicEffectFields)
        }
    }
    fn word(self, off: u64, i: usize) -> usize {
        // SAFETY: a live effect's pointer member
        unsafe { std::ptr::read_unaligned((self.0 + off as usize + i * std::mem::size_of::<usize>()) as *const usize) }
    }
    fn object(self) -> usize {
        self.word(layout().geffect_object, 0)
    }
    fn linked(self) -> [usize; 3] {
        let l = layout();
        [self.word(l.geffect_linked, 0), self.word(l.geffect_linked, 1), self.word(l.geffect_linked, 2)]
    }
    fn count_load(self, delta: c_long) {
        let at = (self.0 + layout().geffect_num_loads as usize) as *mut c_long;
        // SAFETY: the effect's `mNumLoads`
        unsafe { std::ptr::write_unaligned(at, std::ptr::read_unaligned(at).wrapping_add(delta)) }
    }
}

fn media_count(object: usize) -> c_ulong {
    // SAFETY: a live graphic object's use count base
    unsafe {
        std::ptr::read_unaligned((object + layout().gobj_media_use as usize) as *const EeDataMediaUse).mMediaCount
    }
}

/// `RecordNotPreloaded` (0xd723c): the effect's id, when its graphic is not loaded yet.
fn record_not_preloaded(effect: Effect) {
    let object = effect.object();
    if object != 0 && media_count(object) == 0 {
        IDS.lock().unwrap().insert(effect.fields().mID);
    }
}

fn load(effect: Effect, on_demand: bool, host: &EeGfxGeffectHost) {
    effect.count_load(1);
    let object = effect.object();
    if object != 0 {
        if on_demand {
            record_not_preloaded(effect);
        }
        (host.media_load)(object as *mut c_void);
    } else {
        for m in kind_list(effect.fields().mKind) {
            (host.media_load)(m as *mut c_void);
        }
    }
    for linked in effect.linked() {
        if linked != 0 {
            load(Effect(linked), on_demand, host);
        }
    }
}

fn unload(effect: Effect, host: &EeGfxGeffectHost) {
    effect.count_load(-1);
    let object = effect.object();
    if object != 0 {
        (host.media_unload)(object as *mut c_void);
    } else {
        for m in kind_list(effect.fields().mKind) {
            (host.media_unload)(m as *mut c_void);
        }
    }
    for linked in effect.linked() {
        if linked != 0 {
            unload(Effect(linked), host);
        }
    }
}

// ---- the C interface (EEGraphicEffect.cpp, ee/EEGraphicEffect.h)

/// The table replaced by `count` effects (`LoadTable`, after its links; NULL and 0: `UnloadTable`'s clear).
///
/// # Safety
/// `effects` holds `count` entries (each NULL or a live effect).
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_geffect_table_set(effects: *const *mut c_void, count: usize) {
    let list = if effects.is_null() || count == 0 {
        Vec::new()
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(effects, count) }.iter().map(|&e| e as usize).collect()
    };
    *EFFECTS.write().unwrap() = list;
}

/// `EEGraphicEffect::Get(index)`: NULL past the end.
#[no_mangle]
pub extern "C" fn ee_gfx_geffect_get(index: c_ulong) -> *mut c_void {
    EFFECTS.read().unwrap().get(index as usize).copied().unwrap_or(0) as *mut c_void
}

/// `EEGraphicEffect::GetCount()`.
#[no_mangle]
pub extern "C" fn ee_gfx_geffect_count() -> c_ulong {
    EFFECTS.read().unwrap().len() as c_ulong
}

/// The effect's table index (the captures' `std::find`): the count when it is not in the table.
#[no_mangle]
pub extern "C" fn ee_gfx_geffect_index_of(effect: *const c_void) -> c_ulong {
    let effects = EFFECTS.read().unwrap();
    effects.iter().position(|&e| e == effect as usize).unwrap_or(effects.len()) as c_ulong
}

/// `SortMediaByKind`: every graphic object of the media table (`count` `EEGraphicObject*`s, NULL for none) filed
/// under its kind, after those filed before.
///
/// # Safety
/// `media` holds `count` entries, each NULL or a live graphic object of the registered layout.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_geffect_sort_media(media: *const *mut c_void, count: usize) {
    if media.is_null() {
        return;
    }
    let l = layout();
    // SAFETY: the caller's contract
    let entries: Vec<(usize, Option<c_ulong>)> = unsafe { std::slice::from_raw_parts(media, count) }
        .iter()
        .map(|&m| {
            let m = m as usize;
            let kind = (m != 0).then(|| {
                // SAFETY: a live graphic object's record base
                unsafe {
                    std::ptr::read_unaligned((m + l.gobj_fields as usize) as *const EeDataGraphicObjectFields).mKind
                }
            });
            (m, kind)
        })
        .collect();
    let filed = file_by_kind(&entries);
    let mut by_kind = BY_KIND.write().unwrap();
    by_kind.resize(EE_GFX_GEFFECT_KINDS, Vec::new());
    for (k, list) in filed.into_iter().enumerate() {
        by_kind[k].extend(list);
    }
}

/// The number of graphics filed under `kind` (`sMediaByKind[kind].size()`; 0 for a kind out of range).
#[no_mangle]
pub extern "C" fn ee_gfx_geffect_kind_count(kind: c_long) -> c_ulong {
    kind_list(kind).len() as c_ulong
}

/// The graphic `index` of `kind` (`GetMedia`'s random pick; NULL out of range).
#[no_mangle]
pub extern "C" fn ee_gfx_geffect_kind_at(kind: c_long, index: c_long) -> *mut c_void {
    let list = kind_list(kind);
    usize::try_from(index).ok().and_then(|i| list.get(i)).copied().unwrap_or(0) as *mut c_void
}

/// `Load(on_demand)` (0xd7100): a use counted (`mNumLoads`), the graphic loaded (recording the effect when used on
/// demand before its graphic was) or every graphic of its kind, then the three follow-on effects.
///
/// # Safety
/// `effect` is a live effect of the registered layout (its graphics and links live); `host` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_geffect_load(effect: *mut c_void, on_demand: bool, host: *const EeGfxGeffectHost) {
    // SAFETY: the caller's contract
    load(Effect(effect as usize), on_demand, unsafe { &*host });
}

/// `Unload()`: a use counted down, the graphic (or every graphic of the kind) unloaded, then the follow-on effects.
///
/// # Safety
/// As [`ee_gfx_geffect_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_geffect_unload(effect: *mut c_void, host: *const EeGfxGeffectHost) {
    // SAFETY: the caller's contract
    unload(Effect(effect as usize), unsafe { &*host });
}

/// The recorded ids in order (`DumpIDs`): `out` gets up to `room` of them; the number recorded.
///
/// # Safety
/// `out` has room for `room` ids (or `room` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_geffect_ids(out: *mut c_ulong, room: usize) -> usize {
    let ids = IDS.lock().unwrap();
    for (i, &id) in ids.iter().take(room).enumerate() {
        // SAFETY: the caller's room
        unsafe { *out.add(i) = id };
    }
    ids.len()
}

#[cfg(test)]
#[path = "geffect_table_tests.rs"]
mod tests;
