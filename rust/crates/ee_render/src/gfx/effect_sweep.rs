//! TEST ONLY (EE-SIM --effect-sweep; tools/sim_capture.py --profile effects): the effects' sweep (R4.4, fourteenth
//! round; in Rust since the graphics + LLE cluster's second round). No gate game makes most effect kinds, so when the
//! game ends this makes each kind of [`SWEEP`] in the game's world, through each entry point that makes it, from the
//! first two graphic effects of the kind (0x43 and 0x44 have none in the data: they borrow the first blast wave's and
//! teleport's, their kind set for the creation only), on the newest unit on the map of the first player with one (A)
//! and of the next (B; A's own when none): on A, at A's position, from A to B, from A to B's position, between the two
//! positions, at A's location (a transformation) or along the line from A's position to B's; the weapon-point kinds on
//! the armed unit. Then three manager updates, the effect saved, a load-time effect of its kind loaded from those
//! bytes, its pointers repaired and saved again (the copy is left out of the manager: its resources released, the
//! object kept), forty updates more and the effect destroyed. EFSW holds one record per case: the kind, the graphic
//! effect's row, the entry, the creation's result, whether it made an effect, whether the effect lived to its save, the
//! load's and repair's results and the length and FNV-1a 64 of both saves, then the first save's bytes. Only EFSW,
//! EFST and EFMU are written, and no other thread's records while it runs.

// the C longs' widths differ by platform (a cast to the same type on Windows)
#![allow(clippy::unnecessary_cast)]

use std::os::raw::c_ulong;

use ee_base::math::types::{Ge3DLine, GeVector};
use ee_sim::world::{UnitView, WorldRead, WorldView};
use ee_sim::world_field;

use crate::gfx::effect_manager::EeGfxEffectEntry;
use crate::gfx::effect_obj::{self as obj};
use crate::gfx::effect_reads::{self, Gobj, GraphicEffect};
use crate::gfx::ffi_manager::{self as mgr, EeGfxCaptureOp, EeGfxEffectArgs, EeGfxEffectManager, EFMU, EFST};

const EFSW: u32 = mgr::tag(b"EFSW");

#[derive(Clone, Copy)]
enum Entry {
    OnUnit,
    AtPoint,
    BetweenUnits,
    UnitToPoint,
    BetweenPoints,
    Transformation,
    Line,
}

/// One case: the kind, the entry, the kind whose graphic effects it borrows (0: its own), made by the armed unit.
struct Case(i32, Entry, i32, bool);

use Entry::*;

/// The swept cases, in the capture's order (see tools/sim_capture.py's effects profile).
const SWEEP: &[Case] = &[
    Case(0x18, OnUnit, 0, false),
    Case(0x18, AtPoint, 0, false),
    Case(0x1b, AtPoint, 0, false),
    Case(0x21, BetweenUnits, 0, false),
    Case(0x23, OnUnit, 0, false),
    Case(0x25, BetweenPoints, 0, false),
    Case(0x26, BetweenUnits, 0, false),
    Case(0x27, OnUnit, 0, false),
    Case(0x33, UnitToPoint, 0, false),
    Case(0x3f, OnUnit, 0, false),
    Case(0x42, OnUnit, 0, false),
    Case(0x43, UnitToPoint, 0x18, false),
    Case(0x44, OnUnit, 0x3f, false),
    Case(0x45, OnUnit, 0, false),
    Case(0x46, OnUnit, 0, false),
    // the other 3D kinds an entry point makes (not 0x39: its first row faults in GetMedia from its SetUp)
    Case(0x24, OnUnit, 0, false),
    Case(0x28, OnUnit, 0, false),
    Case(0x47, OnUnit, 0, false),
    Case(0x3c, AtPoint, 0, false),
    Case(0x02, OnUnit, 0, false),
    Case(0x02, AtPoint, 0, false),
    Case(0x40, BetweenUnits, 0, false),
    Case(0x40, UnitToPoint, 0, false),
    Case(0x41, OnUnit, 0, false),
    Case(0x41, UnitToPoint, 0, false),
    Case(0x34, OnUnit, 0, false),
    Case(0x35, OnUnit, 0, false),
    Case(0x29, OnUnit, 0, false),
    Case(0x2b, OnUnit, 0, false),
    Case(0x2b, AtPoint, 0, false),
    Case(0x2c, OnUnit, 0, false),
    Case(0x1e, OnUnit, 0, false),
    Case(0x3a, OnUnit, 0, false),
    Case(0x3a, AtPoint, 0, false),
    Case(0x3b, OnUnit, 0, false),
    Case(0x3b, AtPoint, 0, false),
    Case(0x20, AtPoint, 0, false),
    // the 2D kinds (not 0x09, 0x14, 0x30 on an unarmed unit: their start faults without weapon tag points)
    Case(0x03, OnUnit, 0, false),
    Case(0x04, OnUnit, 0, false),
    Case(0x05, OnUnit, 0, false),
    Case(0x06, OnUnit, 0, false),
    Case(0x07, OnUnit, 0, false),
    Case(0x08, OnUnit, 0, false),
    Case(0x0a, OnUnit, 0, false),
    Case(0x0d, OnUnit, 0, false),
    Case(0x0e, OnUnit, 0, false),
    Case(0x0f, OnUnit, 0, false),
    Case(0x10, OnUnit, 0, false),
    Case(0x12, OnUnit, 0, false),
    Case(0x2e, OnUnit, 0, false),
    Case(0x2f, OnUnit, 0, false),
    Case(0x06, AtPoint, 0, false),
    Case(0x08, AtPoint, 0, false),
    Case(0x0b, AtPoint, 0, false),
    Case(0x12, AtPoint, 0, false),
    Case(0x22, AtPoint, 0, false),
    Case(0x2e, AtPoint, 0, false),
    Case(0x31, AtPoint, 0, false),
    Case(0x32, AtPoint, 0, false),
    Case(0x16, BetweenUnits, 0, false),
    Case(0x22, Transformation, 0, false),
    Case(0x32, Transformation, 0, false),
    Case(0x0c, Line, 0, false),
    Case(0x2e, Line, 0, false),
    // the three weapon-point kinds on the armed unit (none: the cases are skipped)
    Case(0x09, OnUnit, 0, true),
    Case(0x14, BetweenUnits, 0, true),
    Case(0x14, UnitToPoint, 0, true),
    Case(0x30, BetweenUnits, 0, true),
    Case(0x30, UnitToPoint, 0, true),
    // the square marching icons with the circular ones' graphic effects
    Case(0x0f, OnUnit, 0x10, false),
];

/// A swept unit: on the map, alive, with stats (and a tile node), its complex unit placed.
fn placed(u: &UnitView<'_>) -> bool {
    let Some(c) = u.complex_unit() else { return false };
    u.hit_points() > 0 && u.stats().is_some() && c.has_tile_node() && c.tile_xy().0 != -10000
}

/// The sweep over the game's manager (null: nothing).
///
/// # Safety
/// The game is over; `m` is null or the game's live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effects_sweep(m: *mut EeGfxEffectManager) {
    if m.is_null() {
        return;
    }
    let w = mgr::mworld();
    let tags = [EFSW, EFST, EFMU];
    (w.capture)(EeGfxCaptureOp::Only, 0, tags.as_ptr() as *const u8, 3);
    (w.capture)(EeGfxCaptureOp::Exclusive, 1, std::ptr::null(), 0);
    let graphic = obj::world().unit_graphic;
    // SAFETY: the game's world, held by the caller
    let (units, armed) = unsafe {
        let t = WorldRead::begin();
        let world = WorldView::global(&t).expect("the world");
        let mut units = [0u64; 2];
        let players = world.num_players();
        let mut p = 1;
        while (p as u64) < players && units[1] == 0 {
            let newest = world.player(p).map_or(0, |pl| pl.units().filter(placed).last().map_or(0, |u| u.handle()));
            if newest != 0 {
                units[if units[0] == 0 { 0 } else { 1 }] = newest;
            }
            p += 1;
        }
        let mut armed = 0u64;
        for p in 1..players as usize {
            let Some(pl) = world.player(p) else { continue };
            for u in pl.units() {
                if !placed(&u) {
                    continue;
                }
                let model = graphic(std::ptr::null_mut(), u.handle());
                if model == 0 {
                    continue;
                }
                let c = u.complex_unit().expect("a placed unit's complex unit");
                if let Some(list) = Gobj(model as usize).weapon_tags(effect_reads::animation(&c)) {
                    let weapon: c_ulong = world_field!(c.complex(), mWeaponTagPoint);
                    if (weapon as usize) < list.len {
                        armed = u.handle();
                    }
                }
            }
        }
        (units, armed)
    };
    if units[0] != 0 {
        let first = units[0];
        let b = if units[1] != 0 { units[1] } else { first };
        let detail = (w.effect_detail)(-1);
        (w.effect_detail)(0xff);
        for case in SWEEP {
            let &Case(kind, entry, borrowed, needs_arms) = case;
            if needs_arms && armed == 0 {
                continue;
            }
            let a = if needs_arms { armed } else { first };
            let source = if borrowed != 0 { borrowed } else { kind };
            let mut rows = 0;
            let count = (obj::world().graphic_effect_count)();
            let mut row = 0u32;
            while row < count && rows < 2 {
                let g = (obj::world().graphic_effect_by_id)(row);
                if g == 0 || GraphicEffect(g as usize).fields().mKind as i32 != source {
                    row += 1;
                    continue;
                }
                rows += 1;
                // SAFETY: the game's world
                unsafe { sweep_case(m, kind, entry, g, row, a, b) };
                row += 1;
            }
        }
        (w.effect_detail)(i64::from(detail));
    }
    (w.capture)(EeGfxCaptureOp::Exclusive, 0, std::ptr::null(), 0);
    (w.capture)(EeGfxCaptureOp::Only, 0, std::ptr::null(), 0);
}

/// One case's creation, steps, save, copy, destruction and record.
unsafe fn sweep_case(m: *mut EeGfxEffectManager, kind: i32, entry: Entry, g: u64, row: u32, a: u64, b: u64) {
    let w = mgr::mworld();
    // SAFETY: the game's world; the units are placed
    let (at, to, location) = unsafe {
        let t = WorldRead::begin();
        let ua = UnitView::of(&t, a).expect("unit A");
        let ub = UnitView::of(&t, b).expect("unit B");
        let c = ua.complex_unit().expect("A's complex unit");
        (effect_reads::unit_position(&ua), effect_reads::unit_position(&ub), effect_reads::physical(&c).location)
    };
    let mut handle = 0u32;
    let own_kind = GraphicEffect(g as usize).fields().mKind;
    GraphicEffect(g as usize).set_kind(kind as _);
    let mut args = EeGfxEffectArgs { graphic_effect: g, ..EeGfxEffectArgs::default() };
    match entry {
        OnUnit => {
            args.entry = EeGfxEffectEntry::Unit;
            args.unit = a;
        }
        AtPoint => {
            args.entry = EeGfxEffectEntry::Placed;
            args.at = at;
        }
        BetweenUnits => {
            args.entry = EeGfxEffectEntry::BetweenUnits;
            args.unit = a;
            args.to_unit = b;
        }
        UnitToPoint => {
            args.entry = EeGfxEffectEntry::UnitToPoint;
            args.unit = a;
            args.at = to;
        }
        Transformation => {
            args.entry = EeGfxEffectEntry::Transformation;
            args.transformation = location;
        }
        Line => {
            args.entry = EeGfxEffectEntry::Line;
            args.line = Ge3DLine { start: at, direction: GeVector { x: to.x - at.x, y: to.y - at.y, z: to.z - at.z } };
        }
        BetweenPoints => {
            args.entry = EeGfxEffectEntry::Between;
            args.at = at;
            args.to = to;
        }
    }
    let result = mgr::create_with(m, &args, &mut handle);
    GraphicEffect(g as usize).set_kind(own_kind);
    for _ in 0..3 {
        mgr::ee_gfx_manager_update(m);
    }
    let made = mgr::ee_gfx_manager_find(m, handle);
    let (mut load, mut repair) = (-1, -1);
    let mut lengths = [0u32; 2];
    let mut hashes = [mgr::FNV_START; 2];
    let mut bytes = Vec::new();
    if !made.is_null() {
        let first = (w.memory_file)();
        // SAFETY: a live effect; the memory file
        unsafe { obj::ee_gfx_effect_save(made, &first, w.save_major, w.save_minor) };
        let copy = mgr::ee_gfx_effect_new_loaded(kind as u32);
        let second = (w.memory_file)();
        if !copy.is_null() {
            (w.memory_file_rewind)(first.context);
            // SAFETY: the copy; the memory files
            unsafe {
                load = obj::ee_gfx_effect_load(copy, &first, w.save_major, w.save_minor);
                repair = obj::ee_gfx_effect_repair(copy);
                obj::ee_gfx_effect_save(copy, &second, w.save_major, w.save_minor);
            }
            obj::ee_gfx_effect_release_resources(copy);
        }
        for (i, f) in [&first, &second].into_iter().enumerate() {
            let data = mgr::memory_bytes(f);
            lengths[i] = data.len() as u32;
            hashes[i] = mgr::fnv(mgr::FNV_START, &data);
            if i == 0 {
                bytes = data;
            }
        }
        (w.memory_file_free)(first.context);
        (w.memory_file_free)(second.context);
    }
    for _ in 0..40 {
        mgr::ee_gfx_manager_update(m);
    }
    if handle != 0 {
        mgr::ee_gfx_manager_destroy(m, handle);
    }
    mgr::ee_gfx_manager_update(m);
    let mut words = vec![
        kind as u32,
        row,
        entry as u32,
        result as u32,
        u32::from(handle != 0),
        u32::from(!made.is_null()),
        load as u32,
        repair as u32,
    ];
    for i in 0..2 {
        words.extend_from_slice(&[lengths[i], hashes[i] as u32, (hashes[i] >> 32) as u32]);
    }
    mgr::commit(EFSW, &words, &bytes);
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;

    /// The cases as (kind, entry, borrowed kind, armed).
    pub(crate) fn cases() -> Vec<(i32, u32, i32, bool)> {
        SWEEP.iter().map(|&Case(k, e, b, a)| (k, e as u32, b, a)).collect()
    }
}
