//! ffi_manager.rs' tests: the manager as a Rust object over effect_obj.rs' fake world (the effects' calls logged
//! there) and a fake manager world: the entry points' plans and registrations under new handles, the lookups, the
//! destruction queue at the next update, the clear and the free; the sweep's case table.

// the C longs' widths differ by platform (a cast to the same type on Windows)
#![allow(clippy::unnecessary_cast)]

use std::sync::atomic::{AtomicPtr, Ordering};

use super::*;
use crate::gfx::effect_obj::tests::{addr, setup, take, unused};

static CURRENT: AtomicPtr<EeGfxEffectManager> = AtomicPtr::new(std::ptr::null_mut());

extern "C" fn construct(a: *const EeGfxEffectArgs) -> EffectPtr {
    // SAFETY: the entry's arguments
    let a = unsafe { &*a };
    let kind = GraphicEffect(a.graphic_effect as usize).fields().mKind as i32;
    let e = obj::ee_gfx_effect_new();
    obj::ee_gfx_effect_attach(e, a.unit, a.graphic_effect, kind, false);
    e
}
extern "C" fn detail(_: i64) -> u32 {
    0xff
}
extern "C" fn current_manager() -> *mut EeGfxEffectManager {
    CURRENT.load(Ordering::Relaxed)
}
extern "C" fn no_capture(_: EeGfxCaptureOp, _: u32, _: *const u8, _: u32) -> u32 {
    0
}
extern "C" fn player_zero(_: *mut c_void) -> u32 {
    0
}
extern "C" fn no_units(_: *mut c_void, _: u32) -> crate::gfx::goal_markers::EeGfxGoalPlayer {
    crate::gfx::goal_markers::EeGfxGoalPlayer { late_epoch: false, shift_mode: false, unit_count: 0 }
}

fn world_once() {
    static ONCE: std::sync::Once = std::sync::Once::new();
    ONCE.call_once(|| {
        let w = Box::leak(Box::new(EeGfxManagerWorld {
            construct,
            damaged_tick: unused(),
            effect_detail: detail,
            current: current_manager,
            goals: EeGfxGoalHost {
                context: std::ptr::null_mut(),
                current_player: player_zero,
                player_count: player_zero,
                player: no_units,
                unit: unused(),
                goal_count: unused(),
                goal: unused(),
                goal_location: unused(),
                unit_location: unused(),
                target: unused(),
                ground: unused(),
                create: unused(),
                destroy: unused(),
                move_line: unused(),
                move_icon: unused(),
            },
            capture: no_capture,
            memory_file: unused(),
            memory_file_data: unused(),
            memory_file_rewind: unused(),
            memory_file_free: unused(),
            save_major: 0,
            save_minor: 0x100,
        }));
        // SAFETY: a leaked world
        unsafe { ee_gfx_manager_world_set(w) };
    });
}

#[test]
fn entry_points_register_effects_that_the_queue_or_their_step_ends() {
    let (_l, mut g, _o) = setup();
    world_once();
    let m = ee_gfx_manager_new();
    CURRENT.store(m, Ordering::Relaxed);
    // a kind the unit entry does not make: refused, nothing made
    g.fields.mKind = 0x50;
    let args = EeGfxEffectArgs { graphic_effect: addr(&*g), ..EeGfxEffectArgs::default() };
    let mut handle = 0u32;
    assert_eq!(create(m, &args, &mut handle), 10);
    assert_eq!((handle, ee_gfx_manager_count(m, false)), (0, 0));
    // the explosion (4) on no unit: made, set up and registered under handles 1 and 2, newest first
    g.fields.mKind = 4;
    assert_eq!(create(m, &args, &mut handle), 0);
    assert_eq!(handle, 1);
    assert_eq!(create(m, &args, &mut handle), 0);
    assert_eq!(handle, 2);
    let (first, second) = (ee_gfx_manager_find(m, 1), ee_gfx_manager_find(m, 2));
    assert!(!first.is_null() && !second.is_null() && first != second);
    assert_eq!((ee_gfx_manager_first(m), ee_gfx_manager_next(m, second)), (second, first));
    // SAFETY: live effects
    assert_eq!(unsafe { ((*first).references, (*(*first).record).handle) }, (2, 1), "the store's reference");
    ee_gfx_manager_destroy(m, 1);
    assert_eq!(ee_gfx_manager_count(m, true), 1);
    // the update destroys the first; the second, an explosion without parts, ends at its first step (7)
    ee_gfx_manager_update(m);
    assert_eq!((ee_gfx_manager_count(m, false), ee_gfx_manager_count(m, true)), (0, 0));
    assert!(ee_gfx_manager_find(m, 1).is_null() && ee_gfx_manager_find(m, 2).is_null());
    assert_eq!(create(m, &args, &mut handle), 0);
    assert_eq!((handle, ee_gfx_manager_count(m, false)), (3, 1));
    ee_gfx_manager_clear(m);
    assert_eq!(ee_gfx_manager_count(m, false), 0);
    CURRENT.store(std::ptr::null_mut(), Ordering::Relaxed);
    // SAFETY: the manager, not used again
    unsafe { ee_gfx_manager_free(m) };
    take();
}

#[test]
fn the_sweeps_cases_are_the_captures() {
    use crate::gfx::effect_sweep::tests::cases;
    let c = cases();
    assert_eq!(c.len(), 70);
    assert_eq!((c[0], c[11], c[69]), ((0x18, 0, 0, false), (0x43, 3, 0x18, false), (0x0f, 0, 0x10, false)));
    assert_eq!(c.iter().filter(|x| x.3).count(), 5, "the armed unit's");
    assert!(c.iter().all(|x| x.1 <= 6), "the C++ entry numbers");
}
