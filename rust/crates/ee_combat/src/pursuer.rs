//! `EEGetPursuerOf` (R5b.2, third round): the nearest unit chasing a unit, along the x-sorted unit list, over the
//! searches' host (`SearchHost`).

use crate::search::{SearchHost, EE_COMBAT_SEARCH_TEST_PURSUES};
use crate::search_ffi::EeCombatSearchHost;
use crate::walks::EeCombatHandle;

/// `EEGetPursuerOf`: the nearest unit (by the squared tile distance to its nearest footprint tile of the unit, the
/// first of equals) whose goal attacks or gathers from the unit, along the unit list both ways (one step each way a
/// turn). `trace` gets each pursuer and its distance.
pub fn pursuer_of(unit: EeCombatHandle, x: i32, y: i32, host: &mut dyn SearchHost) -> EeCombatHandle {
    let mut best: i32 = -1;
    let mut result: EeCombatHandle = 0;
    let mut forward = host.list_next(unit, true);
    let mut backward = host.list_next(unit, false);
    let mut forward_done = forward == 0 || host.dummy_head(forward);
    let mut backward_done = backward == 0;
    let mut visit = |host: &mut dyn SearchHost, other: EeCombatHandle| {
        if host.test(EE_COMBAT_SEARCH_TEST_PURSUES, other) {
            let (tile_x, tile_y) = host.footprint_tile(other);
            let (dy, dx) = (y.wrapping_sub(tile_y), x.wrapping_sub(tile_x));
            let distance = dy.wrapping_mul(dy).wrapping_add(dx.wrapping_mul(dx));
            host.trace(other, distance, 0);
            if best == -1 || distance < best {
                result = other;
                best = distance;
            }
        }
    };
    while !forward_done || !backward_done {
        if !forward_done {
            visit(host, forward);
            forward = host.list_next(forward, true);
            forward_done = forward == 0 || host.dummy_head(forward);
        }
        if backward_done {
            continue;
        }
        visit(host, backward);
        backward = host.list_next(backward, false);
        backward_done = backward == 0;
    }
    result
}

/// `EEGetPursuerOf`: the pursuer's list handle, 0 for none.
///
/// # Safety
/// `host` must point to a valid record whose callbacks are callable with its `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_pursuer_of(unit: u64, x: i32, y: i32, host: *const EeCombatSearchHost) -> u64 {
    let mut host = unsafe { *host };
    pursuer_of(unit, x, y, &mut host)
}
