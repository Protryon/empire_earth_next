//! The C ABI the tech tree, `EEEvent.cpp` and the calamity classes call (`include/rust/ee_rust.h`; the C++ adapters
//! fill the views in `ee/EETechRust.h`). Plain data in and out; the two tree walks ask the C++ tree through callbacks
//! and hold no reference into C++ memory across them (the costs are copied in and out).

use crate::calamity::{self, EeCalamityAge};
use crate::calamity_update::{self, CalamityHost, EeCalamityCall, EeCalamityParams, EeCalamityReply, EeCalamityState};
use crate::effects::{self, EeTechAttribute, EeTechEffectHandler, EeTechEffectValues};
use crate::tree::{
    self, EeTechChecksumEntry, EeTechCostInput, EeTechEpochCounter, EeTechEpochUpdate, EeTechNode, EeTechPrerequisite,
    EeTechResearch, EeTechTreeResearchAfter, EeTechTreeState, PrerequisiteView, SumCostsNode, SumCostsView,
};
use std::ffi::c_void;

// SAFETY (every function taking pointers below): the caller passes valid, aligned pointers for the documented reads
// and writes, as the C++ adapters do from their own locals.

/// `EEApplyEventEffect(float, float, EEEventEffect*)`.
///
/// # Safety
/// `effect` must be valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_effect_f32(value: f32, base: f32, effect: *const EeTechEffectValues) -> f32 {
    // SAFETY: the caller's contract
    effects::apply_f32(value, base, unsafe { &*effect })
}

/// `EEApplyEventEffect(long, long, EEEventEffect*)`.
///
/// # Safety
/// `effect` must be valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_effect_i32(value: i32, base: i32, effect: *const EeTechEffectValues) -> i32 {
    // SAFETY: the caller's contract
    effects::apply_i32(value, base, unsafe { &*effect })
}

/// `ApplyEventGatherRate`'s new gather time.
///
/// # Safety
/// `effect` must be valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_gather_time(current: i32, class_time: i32, effect: *const EeTechEffectValues) -> i32 {
    // SAFETY: the caller's contract
    effects::gather_time(current, class_time, unsafe { &*effect })
}

/// Attribute 6's new maximum hit points.
///
/// # Safety
/// `effect` must be valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_max_hit_points(
    class_hit_points: i32,
    variant_factor: f32,
    old_maximum: u32,
    effect: *const EeTechEffectValues,
) -> i32 {
    // SAFETY: the caller's contract
    effects::max_hit_points(class_hit_points, variant_factor, old_maximum, unsafe { &*effect })
}

/// A unit's hit points or flight time in proportion to a new maximum (before the caller's cast).
#[no_mangle]
pub extern "C" fn ee_tech_rescale(value: i32, old_maximum: u32, new_maximum: i32) -> f32 {
    effects::rescale(value, old_maximum, new_maximum)
}

/// Attribute 23's new population limit bonus.
///
/// # Safety
/// `effect` must be valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_population_bonus(bonus: i32, effect: *const EeTechEffectValues) -> i32 {
    // SAFETY: the caller's contract
    effects::population_bonus(bonus, unsafe { &*effect })
}

/// Attribute 42: the terrain type is one a mask has.
#[no_mangle]
pub extern "C" fn ee_tech_terrain_type_valid(terrain: i32) -> bool {
    effects::terrain_type_valid(terrain)
}

/// Attribute 42 and the pathfinding functor: a terrain type allowed or forbidden.
#[no_mangle]
pub extern "C" fn ee_tech_terrain_mask(mask: u32, terrain: i32, allowed: bool) -> u32 {
    effects::terrain_mask(mask, terrain, allowed)
}

/// What an attribute effect changes.
#[no_mangle]
pub extern "C" fn ee_tech_attribute(attribute: i32) -> EeTechAttribute {
    effects::attribute(attribute)
}

/// Whether an attribute's operation applies to the player.
#[no_mangle]
pub extern "C" fn ee_tech_attribute_is_player(attribute: i32) -> bool {
    effects::attribute_is_player(effects::attribute(attribute).op)
}

/// `RunEffect`'s handler of an effect.
#[no_mangle]
pub extern "C" fn ee_tech_effect_handler(
    known: bool,
    kind: i32,
    loading: bool,
    run_on_load: u8,
    events_enabled: bool,
) -> EeTechEffectHandler {
    effects::effect_handler(known, kind, loading, run_on_load, events_enabled)
}

/// `EETechTreeNode::GetIsObsolete`.
///
/// # Safety
/// `node` and `tree` must be valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_node_is_obsolete(node: *const EeTechNode, tree: *const EeTechTreeState) -> bool {
    // SAFETY: the caller's contract
    unsafe { tree::node_is_obsolete(&*node, &*tree) }
}

/// `EETechTreeNode::GetIsAvailable`.
///
/// # Safety
/// `node` and `tree` must be valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_node_is_available(node: *const EeTechNode, tree: *const EeTechTreeState) -> bool {
    // SAFETY: the caller's contract
    unsafe { tree::node_is_available(&*node, &*tree) }
}

/// `EETechTreeEpoch::GetIsAvailable`.
///
/// # Safety
/// `epoch` must be valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_epoch_is_available(epoch: *const EeTechNode) -> bool {
    // SAFETY: the caller's contract
    tree::epoch_is_available(unsafe { &*epoch })
}

/// A loaded node's prerequisite flags: the bits of its empty prerequisites.
#[no_mangle]
pub extern "C" fn ee_tech_initial_prerequisite_flags(p1: i32, p2: i32, p3: i32, p4: i32) -> u8 {
    tree::initial_prerequisite_flags(&[p1, p2, p3, p4])
}

/// `EETechTreeNode::SetPrerequisite`.
///
/// # Safety
/// `node` must be valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_set_prerequisite(
    node: *const EeTechNode,
    id: i32,
    epoch_count: i32,
) -> EeTechPrerequisite {
    // SAFETY: the caller's contract
    tree::set_prerequisite(unsafe { &*node }, id, epoch_count)
}

/// `EETechTreeNode::ResearchNode`'s decision.
///
/// # Safety
/// `node` must be valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_research_node(node: *const EeTechNode) -> EeTechResearch {
    // SAFETY: the caller's contract
    tree::research_node(unsafe { &*node })
}

/// The research delay after a `ResearchNode`.
#[no_mangle]
pub extern "C" fn ee_tech_research_delay_after(delay: i32) -> i32 {
    tree::research_delay_after(delay)
}

/// `EETechTreeEpoch::Research` of the epoch itself.
///
/// # Safety
/// `epoch` and `tree` must be valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_research_epoch(
    epoch: *const EeTechNode,
    tree: *const EeTechTreeState,
) -> EeTechResearch {
    // SAFETY: the caller's contract
    unsafe { tree::research_epoch(&*epoch, &*tree) }
}

/// `EETechTreeEpoch::AddSOL`.
///
/// # Safety
/// `counter` and `tree` must be valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_add_sol(
    epoch_id: i32,
    counter: *const EeTechEpochCounter,
    tree: *const EeTechTreeState,
) -> EeTechEpochUpdate {
    // SAFETY: the caller's contract
    unsafe { tree::add_sol(epoch_id, &*counter, &*tree) }
}

/// `EETechTreeEpoch::UpdateAvailability`.
///
/// # Safety
/// `counter` and `tree` must be valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_update_availability(
    epoch_id: i32,
    counter: *const EeTechEpochCounter,
    tree: *const EeTechTreeState,
) -> EeTechEpochUpdate {
    // SAFETY: the caller's contract
    unsafe { tree::update_availability(epoch_id, &*counter, &*tree) }
}

/// `EETechTree::Research`'s end; `counts_toward_epoch` is -1 without a node, else 0 or 1.
#[no_mangle]
pub extern "C" fn ee_tech_tree_research_after(
    id: i32,
    result: i32,
    epoch_count: i32,
    counts_toward_epoch: i32,
    next_epoch: i32,
    epoch_slots: i32,
) -> EeTechTreeResearchAfter {
    let counts = if counts_toward_epoch < 0 { None } else { Some(counts_toward_epoch != 0) };
    tree::tree_research_after(id, result, epoch_count, counts, next_epoch, epoch_slots)
}

/// `EETechTreeNode::GetCost`.
///
/// # Safety
/// `input` must be valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_node_cost(input: *const EeTechCostInput) -> i32 {
    // SAFETY: the caller's contract
    tree::node_cost(unsafe { &*input })
}

/// `EETechTreeEpoch::GetCost`'s scale.
#[no_mangle]
pub extern "C" fn ee_tech_epoch_cost_scale(
    epochs_advanced: u32,
    later_factor: f32,
    epoch_factor: f32,
    second_factor: f32,
    first_factor: f32,
    scale: f32,
) -> f32 {
    tree::epoch_cost_scale(epochs_advanced, later_factor, epoch_factor, second_factor, first_factor, scale)
}

/// `EETechTreeNode::Load`: the `k`th unit class's cost table into `out` (seven costs).
///
/// # Safety
/// `costs` must be valid for 7 reads, `factors` for 4, `out` for 7 writes.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_class_costs(costs: *const i32, k: u32, factors: *const f32, out: *mut i32) {
    // SAFETY: the caller's contract
    unsafe {
        let factor = tree::class_cost_factor(k, &*(factors as *const [f32; 4]));
        let table = tree::class_costs(&*(costs as *const [i32; 7]), factor);
        std::ptr::copy_nonoverlapping(table.as_ptr(), out, 7);
    }
}

/// `EETechTree::GetChecksum`'s value over `count` entries.
///
/// # Safety
/// `entries` must be valid for `count` reads (or `count` 0).
#[no_mangle]
pub unsafe extern "C" fn ee_tech_checksum(entries: *const EeTechChecksumEntry, count: u32) -> u32 {
    if count == 0 {
        return tree::checksum(&[]);
    }
    // SAFETY: the caller's contract
    tree::checksum(unsafe { std::slice::from_raw_parts(entries, count as usize) })
}

/// The C++ tree for `DependsOn`: writes the four prerequisites of `id`'s node to `out`, false without a node.
pub type EeTechPrerequisitesFn = unsafe extern "C" fn(ctx: *mut c_void, id: i32, out: *mut i32) -> bool;

struct CPrerequisites {
    ctx: *mut c_void,
    get: EeTechPrerequisitesFn,
}

impl PrerequisiteView for CPrerequisites {
    fn prerequisites(&self, id: i32) -> Option<[i32; 4]> {
        let mut out = [0i32; 4];
        // SAFETY: the C++ callback writes four ints to a local
        unsafe { (self.get)(self.ctx, id, out.as_mut_ptr()) }.then_some(out)
    }
}

/// `EETechTreeNode::DependsOn` of the node `own` with its prerequisites `p1..p4`.
///
/// # Safety
/// `get` must be callable with `ctx` for the call's duration.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_depends_on(
    ctx: *mut c_void,
    get: EeTechPrerequisitesFn,
    own: i32,
    p1: i32,
    p2: i32,
    p3: i32,
    p4: i32,
    id: i32,
) -> bool {
    tree::depends_on(&CPrerequisites { ctx, get }, own, &[p1, p2, p3, p4], id)
}

/// A node as `SumCosts` reads it (the C++ fills it; false without a node).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeTechSumNode {
    pub id: i32,
    pub unit_class: i32,
    pub prerequisites: [i32; 4],
    pub available: bool,
    pub researched: bool,
    pub pad: [u8; 2],
}

/// The C++ tree for `SumCosts`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeTechSumCostsCallbacks {
    pub ctx: *mut c_void,
    pub node: unsafe extern "C" fn(ctx: *mut c_void, id: i32, out: *mut EeTechSumNode) -> bool,
    pub cost: unsafe extern "C" fn(ctx: *mut c_void, id: i32, resource: i32, unit_class: i32) -> i32,
    pub class_technology: unsafe extern "C" fn(ctx: *mut c_void, unit_class: i32) -> i32,
}

impl SumCostsView for EeTechSumCostsCallbacks {
    fn node(&mut self, id: i32) -> Option<SumCostsNode> {
        let mut out = EeTechSumNode::default();
        // SAFETY: the C++ callback fills a local
        unsafe { (self.node)(self.ctx, id, &mut out) }.then_some(SumCostsNode {
            id: out.id,
            available: out.available,
            researched: out.researched,
            unit_class: out.unit_class,
            prerequisites: out.prerequisites,
        })
    }
    fn cost(&mut self, id: i32, resource: i32, unit_class: i32) -> i32 {
        // SAFETY: the C++ callback
        unsafe { (self.cost)(self.ctx, id, resource, unit_class) }
    }
    fn class_technology(&mut self, unit_class: i32) -> i32 {
        // SAFETY: the C++ callback
        unsafe { (self.class_technology)(self.ctx, unit_class) }
    }
}

/// `EETechTree::SumCosts`: adds to the seven `costs`.
///
/// # Safety
/// `callbacks` must be valid for reads and its functions callable for the call's duration; `costs` valid for 7 reads
/// and writes (not touched by the callbacks).
#[no_mangle]
pub unsafe extern "C" fn ee_tech_sum_costs(
    callbacks: *const EeTechSumCostsCallbacks,
    id: i32,
    unit_class: i32,
    costs: *mut i32,
) {
    // SAFETY: the caller's contract; the costs are copied so that no reference into them lives across a callback
    unsafe {
        let mut view = *callbacks;
        let mut local = [0i32; 7];
        std::ptr::copy_nonoverlapping(costs, local.as_mut_ptr(), 7);
        tree::sum_costs(&mut view, id, unit_class, &mut local);
        std::ptr::copy_nonoverlapping(local.as_ptr(), costs, 7);
    }
}

/// `EECalamityClass::UpdateTimeAlive`'s arithmetic.
#[no_mangle]
pub extern "C" fn ee_calamity_update_time_alive(time_alive: f32, elapsed: f32, lifetime: f32) -> EeCalamityAge {
    calamity::update_time_alive(time_alive, elapsed, lifetime)
}

/// The C++ host of a calamity's update (`ee/EECalamityUpdate.h`): `call` answers one [`EeCalamityCall`] on the C++
/// objects, reading and writing the calamity through `state` (the C++ copies it in and out around its step).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeCalamityHostFfi {
    pub ctx: *mut c_void,
    pub call: unsafe extern "C" fn(
        ctx: *mut c_void,
        state: *mut EeCalamityState,
        call: *const EeCalamityCall,
        reply: *mut EeCalamityReply,
    ),
}

struct FfiCalamityHost(EeCalamityHostFfi);

impl CalamityHost for FfiCalamityHost {
    fn call(&mut self, state: &mut EeCalamityState, call: &EeCalamityCall) -> EeCalamityReply {
        let mut reply = EeCalamityReply::default();
        // SAFETY: the C++ host's contract (ee/EECalamityUpdate.h): valid pointers for the call's duration
        unsafe { (self.0.call)(self.0.ctx, state, call, &mut reply) };
        reply
    }
}

/// A calamity class's `Update(calamity, elapsed)` (`calamity_update::update`); `state` in and out.
///
/// # Safety
/// `params`, `state` and `host` must be valid; the host's `call` must answer every op the class asks.
#[no_mangle]
pub unsafe extern "C" fn ee_calamity_update(
    params: *const EeCalamityParams,
    state: *mut EeCalamityState,
    elapsed: f32,
    host: *const EeCalamityHostFfi,
) -> i32 {
    // SAFETY: the caller's contract; copies, so no reference into C++ memory lives across the host's calls
    let (params, mut s, host) = unsafe { (*params, *state, *host) };
    let result = calamity_update::update(&mut FfiCalamityHost(host), &mut s, &params, elapsed);
    // SAFETY: as above
    unsafe { *state = s };
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    unsafe extern "C" fn prereqs(_ctx: *mut c_void, id: i32, out: *mut i32) -> bool {
        let p: [i32; 4] = match id {
            1000 => [0, 0, 0, 0],
            1001 => [1000, 0, 0, 0],
            0 => [0, 0, 0, 0],
            _ => return false,
        };
        // SAFETY: four ints
        unsafe { std::ptr::copy_nonoverlapping(p.as_ptr(), out, 4) };
        true
    }

    #[test]
    fn depends_on_through_the_callback() {
        // SAFETY: the callback above
        unsafe {
            assert!(ee_tech_depends_on(std::ptr::null_mut(), prereqs, 1002, 1001, 0, 0, 0, 1000));
            assert!(!ee_tech_depends_on(std::ptr::null_mut(), prereqs, 1002, 1001, 0, 0, 0, 1003));
        }
    }

    unsafe extern "C" fn node(_ctx: *mut c_void, id: i32, out: *mut EeTechSumNode) -> bool {
        if id != 1000 {
            return false;
        }
        // SAFETY: a valid out pointer
        unsafe {
            *out = EeTechSumNode {
                id,
                unit_class: 0,
                prerequisites: [0; 4],
                available: true,
                researched: false,
                pad: [0; 2],
            }
        };
        true
    }
    unsafe extern "C" fn cost(ctx: *mut c_void, _id: i32, resource: i32, _unit_class: i32) -> i32 {
        // SAFETY: the test's counter
        unsafe { *(ctx as *mut i32) += 1 };
        resource * 10
    }
    unsafe extern "C" fn technology(_ctx: *mut c_void, _unit_class: i32) -> i32 {
        tree::TECH_NONE
    }

    #[test]
    fn sum_costs_through_the_callbacks() {
        let mut calls = 0i32;
        let callbacks = EeTechSumCostsCallbacks {
            ctx: &mut calls as *mut i32 as *mut c_void,
            node,
            cost,
            class_technology: technology,
        };
        let mut costs = [1i32; 7];
        // SAFETY: valid pointers and callbacks
        unsafe { ee_tech_sum_costs(&callbacks, 1000, 5, costs.as_mut_ptr()) };
        assert_eq!(costs, [1, 1, 21, 31, 41, 51, 61]);
        assert_eq!(calls, 5);
    }

    #[test]
    fn class_costs_and_checksum_through_the_abi() {
        let mut out = [0i32; 7];
        // SAFETY: valid arrays
        unsafe {
            ee_tech_class_costs(
                [10, 20, 30, 40, 50, 60, 70].as_ptr(),
                2,
                [2.0, 0.5, 3.0, 4.0].as_ptr(),
                out.as_mut_ptr(),
            )
        };
        assert_eq!(out, [5, 10, 15, 20, 25, 30, 35]);
        // SAFETY: no entries
        assert_eq!(unsafe { ee_tech_checksum(std::ptr::null(), 0) }, 0);
        assert_eq!(ee_tech_tree_research_after(1500, 0, 15, -1, 4, 15), EeTechTreeResearchAfter::Nothing);
        assert_eq!(ee_tech_tree_research_after(1500, 0, 15, 1, 4, 15), EeTechTreeResearchAfter::CountTowardNextEpoch);
    }
}
