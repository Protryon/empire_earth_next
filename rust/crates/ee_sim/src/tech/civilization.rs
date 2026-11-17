//! A player's civilization's purchases (`EECivilization`, `Empire Earth/EECivilization.cpp`): the costs, whether a
//! list of upgrades is affordable, and the order a loaded civilization's upgrades are applied in. The C++ class keeps
//! its members (the purchased and deferred upgrade vectors, the points, the name) and its save format; the upgrades
//! apply through `EECivilizationManager::ApplyUpgrade` (the callback), which is Rust's `civ_apply` behind the C++'s
//! capture hooks.
use super::civ::{tables, EeTechCivLevel, EeTechCivUpgrade, LEVEL_BITS, LEVEL_MASK};
use std::ffi::{c_long, c_ulong, c_void};

/// The attribute index of the upgrades a loaded civilization applies last (`ApplyDeferred`) and the world takes from
/// player 0 (`ApplyLevel21ToWorld`): the population limit's.
pub const EE_TECH_CIV_DEFERRED_INDEX: u32 = 0x15;

/// `GetBaseCost`: the level's bonus (`LookupLevel`'s second output, as the original passed it) times 100, rounded as
/// `UMFloatToLong`; 0 for an unknown category.
pub fn base_cost(level: Option<EeTechCivLevel>) -> c_long {
    level.map_or(0, |l| ee_base::math::um::lrintf(l.mBonus * 100.0) as c_long)
}

/// `CanAfford`'s cost: the base plus the category's step for each upgrade of its category in `list`.
pub fn upgrade_cost(base: c_long, step: c_long, list: &[EeTechCivUpgrade], upgrade: EeTechCivUpgrade) -> c_ulong {
    let count = list.iter().filter(|&&u| u >> LEVEL_BITS == upgrade >> LEVEL_BITS).count() as c_long;
    base.wrapping_add(step.wrapping_mul(count)) as c_ulong
}

/// The cost of `upgrade` against `list` with the loaded tables.
fn cost_now(list: &[EeTechCivUpgrade], upgrade: EeTechCivUpgrade) -> c_ulong {
    let t = tables();
    let base = base_cost(t.category_level(upgrade >> LEVEL_BITS, upgrade & LEVEL_MASK));
    upgrade_cost(base, t.extra_cost(upgrade) as c_long, list, upgrade)
}

/// `CanAffordAll`: each of `upgrades` affordable in turn (each one bought adds to the list the next is counted
/// against); always with custom civilizations off. `cost` answers an upgrade's cost against a list.
pub fn can_afford_all(
    purchased: &[EeTechCivUpgrade],
    upgrades: &[EeTechCivUpgrade],
    available: c_ulong,
    allow_custom: bool,
    cost: impl Fn(&[EeTechCivUpgrade], EeTechCivUpgrade) -> c_ulong,
) -> bool {
    let mut list = purchased.to_vec();
    for &u in upgrades {
        if !(cost(&list, u) <= available || !allow_custom) {
            return false;
        }
        list.push(u);
    }
    true
}

/// `Reapply`: applies the purchased upgrades in order until one fails, deferring those of the deferred index;
/// returns whether every applied one succeeded and the deferred ones.
pub fn reapply(purchased: &[EeTechCivUpgrade], mut apply: impl FnMut(EeTechCivUpgrade) -> bool) -> (bool, Vec<u32>) {
    let mut deferred = Vec::new();
    let mut applied = true;
    for &u in purchased {
        if !applied {
            break;
        }
        if u & LEVEL_MASK == EE_TECH_CIV_DEFERRED_INDEX {
            deferred.push(u);
        } else {
            applied = apply(u);
        }
    }
    (applied, deferred)
}

/// `ApplyDeferred`: applies the upgrades in order until one fails; whether all did.
pub fn apply_all(upgrades: &[EeTechCivUpgrade], mut apply: impl FnMut(EeTechCivUpgrade) -> bool) -> bool {
    upgrades.iter().all(|&u| apply(u))
}

/// A C++ apply step: `EECivilizationManager::ApplyUpgrade(upgrade, player)`.
pub type EeTechCivApply = extern "C" fn(context: *mut c_void, upgrade: EeTechCivUpgrade) -> bool;

/// # Safety
/// `data` names `size` upgrades or is null with `size` 0.
unsafe fn upgrades_at<'a>(data: *const EeTechCivUpgrade, size: usize) -> &'a [EeTechCivUpgrade] {
    if size == 0 {
        &[]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(data, size) }
    }
}

/// `CanAfford`: `upgrade`'s cost against `list` into `out_cost` (when not null); whether it is affordable (always with
/// custom civilizations off).
///
/// # Safety
/// `list` names `count` upgrades; `out_cost` is writable or null.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_can_afford(
    upgrade: EeTechCivUpgrade,
    list: *const EeTechCivUpgrade,
    count: usize,
    available: c_ulong,
    allow_custom: bool,
    out_cost: *mut c_ulong,
) -> bool {
    // SAFETY: the caller's contract
    let cost = cost_now(unsafe { upgrades_at(list, count) }, upgrade);
    if !out_cost.is_null() {
        // SAFETY: the caller's contract
        unsafe { *out_cost = cost };
    }
    cost <= available || !allow_custom
}

/// `GetBaseCost`.
#[no_mangle]
pub extern "C" fn ee_tech_civ_base_cost(upgrade: EeTechCivUpgrade) -> c_long {
    base_cost(tables().category_level(upgrade >> LEVEL_BITS, upgrade & LEVEL_MASK))
}

/// [`can_afford_all`] with the loaded tables.
///
/// # Safety
/// `purchased` and `upgrades` name `purchased_count` and `count` upgrades.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_can_afford_all(
    purchased: *const EeTechCivUpgrade,
    purchased_count: usize,
    upgrades: *const EeTechCivUpgrade,
    count: usize,
    available: c_ulong,
    allow_custom: bool,
) -> bool {
    // SAFETY: the caller's contract
    let (p, u) = unsafe { (upgrades_at(purchased, purchased_count), upgrades_at(upgrades, count)) };
    can_afford_all(p, u, available, allow_custom, cost_now)
}

/// [`reapply`]: the deferred upgrades into `deferred` (room for `count`), their number into `deferred_count`.
///
/// # Safety
/// `purchased` names `count` upgrades; `deferred` has room for `count`; `deferred_count` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_reapply(
    purchased: *const EeTechCivUpgrade,
    count: usize,
    apply: EeTechCivApply,
    context: *mut c_void,
    deferred: *mut EeTechCivUpgrade,
    deferred_count: *mut usize,
) -> bool {
    // SAFETY: the caller's contract
    let (applied, d) = reapply(unsafe { upgrades_at(purchased, count) }, |u| apply(context, u));
    // SAFETY: at most `count` deferred
    unsafe {
        std::ptr::copy_nonoverlapping(d.as_ptr(), deferred, d.len());
        *deferred_count = d.len();
    }
    applied
}

/// [`apply_all`].
///
/// # Safety
/// `upgrades` names `count` upgrades.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_apply_all(
    upgrades: *const EeTechCivUpgrade,
    count: usize,
    apply: EeTechCivApply,
    context: *mut c_void,
) -> bool {
    // SAFETY: the caller's contract
    apply_all(unsafe { upgrades_at(upgrades, count) }, |u| apply(context, u))
}

/// `ApplyLevel21ToWorld`: applies every purchased upgrade of the deferred index (results ignored).
///
/// # Safety
/// `purchased` names `count` upgrades.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_civ_apply_deferred_kind(
    purchased: *const EeTechCivUpgrade,
    count: usize,
    apply: EeTechCivApply,
    context: *mut c_void,
) {
    // SAFETY: the caller's contract
    for &u in unsafe { upgrades_at(purchased, count) } {
        if u & LEVEL_MASK == EE_TECH_CIV_DEFERRED_INDEX {
            apply(context, u);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const fn up(category: u32, index: u32) -> u32 {
        category << LEVEL_BITS | index
    }

    #[test]
    fn costs_round_the_bonus_and_count_the_category() {
        assert_eq!(base_cost(None), 0);
        assert_eq!(base_cost(Some(EeTechCivLevel { mValue: 9.0, mBonus: 0.125 })), 12); // 12.5 to even
        assert_eq!(base_cost(Some(EeTechCivLevel { mValue: 0.0, mBonus: 0.25 })), 25);
        let list = [up(2, 1), up(3, 4), up(2, 7)];
        assert_eq!(upgrade_cost(10, 5, &list, up(2, 9)), 20);
        assert_eq!(upgrade_cost(10, 5, &list, up(4, 1)), 10);
        assert_eq!(upgrade_cost(-30, 5, &[], up(4, 1)), (-30 as c_long) as c_ulong);
    }

    #[test]
    fn a_list_is_affordable_in_turn() {
        let cost = |l: &[u32], u: u32| upgrade_cost(10, 10, l, u);
        // 10, then 20 in the same category: 30 in all
        assert!(can_afford_all(&[], &[up(1, 1), up(1, 2)], 20, true, cost));
        assert!(!can_afford_all(&[], &[up(1, 1), up(1, 2)], 19, true, cost));
        assert!(!can_afford_all(&[up(1, 5)], &[up(1, 1)], 19, true, cost));
        assert!(can_afford_all(&[up(1, 5)], &[up(1, 1)], 0, false, cost));
        assert!(can_afford_all(&[], &[], 0, true, cost));
    }

    #[test]
    fn reapply_defers_the_population_limit_and_stops_at_a_failure() {
        let list = [up(1, 1), up(2, 0x15), up(3, 2), up(4, 0x15), up(5, 3)];
        let mut seen = Vec::new();
        let (ok, deferred) = reapply(&list, |u| {
            seen.push(u);
            u != up(3, 2)
        });
        assert!(!ok);
        assert_eq!(seen, vec![up(1, 1), up(3, 2)]);
        assert_eq!(deferred, vec![up(2, 0x15)]);
        let (ok, deferred) = reapply(&list, |_| true);
        assert!(ok && deferred == vec![up(2, 0x15), up(4, 0x15)]);
        let mut n = 0;
        assert!(!apply_all(&[1, 2, 3], |u| {
            n += 1;
            u != 2
        }));
        assert_eq!(n, 2);
    }
}
