//! The tech tree's logic (`src/Empire Earth/Tech Tree`): a node's and an epoch's availability, the prerequisites,
//! the research decisions, the costs and the epoch's research counter (its "SOL": techs that count toward it), the
//! tree's checksum, `DependsOn` and `SumCosts`. The C++ nodes keep their members; they pass an [`EeTechNode`] view
//! and apply the decision that comes back (firing the events, refreshing the interface) in the C++'s order.

use crate::ftol;

/// `kTechNone`: no technology.
pub const TECH_NONE: i32 = 100_000;
/// `kPrerequisiteEpoch`: the epoch's bit of a node's prerequisite flags.
pub const PREREQUISITE_EPOCH: u8 = 1;
/// The bits of the four prerequisites (`kPrerequisitePrerequisite1..4`).
pub const PREREQUISITE_BITS: [u8; 4] = [2, 4, 8, 0x10];
/// `kPrerequisiteAll`: every bit; the node becomes available.
pub const PREREQUISITE_ALL: u8 = 0x1f;
/// `ResearchNode`'s results.
pub const RESULT_RESEARCHED: i32 = 0;
pub const RESULT_ALREADY: i32 = 2;
pub const RESULT_REFUSED: i32 = 9;

/// A node's (`EETechTreeNode`, or an epoch's) state as the logic reads it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeTechNode {
    pub id: i32,
    /// `mData->mKind` (3: repeatable, 4: a wonder, 5: never researched)
    pub kind: i32,
    pub epoch: i32,
    pub obsolete_epoch: i32,
    pub research_delay: i32,
    pub prerequisites: [i32; 4],
    pub prerequisite_flags: u8,
    pub researched: bool,
    /// `mIsObsolete` (the flag; [`node_is_obsolete`] adds the obsolete epoch)
    pub obsolete: bool,
    pub available: bool,
    pub editor_obsoleted: bool,
    pub makes_unit: bool,
    pub counts_toward_epoch: bool,
    pub pad: u8,
}

/// The tree's epochs (`EETechTree`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeTechTreeState {
    pub current_epoch: i32,
    pub start_epoch: i32,
    pub end_epoch: i32,
    pub epochs_before_wonders: i32,
    /// `gEpochCount`: the ids below it are epochs (`EEIsEpochID`)
    pub epoch_count: i32,
}

/// `EEIsEpochID`.
pub fn is_epoch_id(id: i32, epoch_count: i32) -> bool {
    id >= 0 && id < epoch_count
}

/// `EETechTreeNode::GetIsObsolete`: by its flag, or because its obsolete epoch has passed.
pub fn node_is_obsolete(node: &EeTechNode, tree: &EeTechTreeState) -> bool {
    node.obsolete || node.obsolete_epoch < tree.current_epoch
}

/// `EETechTreeNode::GetIsAvailable`: researchable now. Wonders (kind 4) only after enough epochs of this game or in
/// its last epoch; a researched node only when it makes a unit or repeats (kind 3).
pub fn node_is_available(node: &EeTechNode, tree: &EeTechTreeState) -> bool {
    node.available
        && !node_is_obsolete(node, tree)
        && !node.editor_obsoleted
        && node.epoch <= tree.current_epoch
        && (node.makes_unit || node.kind == 3 || !node.researched)
        && (node.kind != 4
            || tree.current_epoch == tree.end_epoch
            || tree.current_epoch.wrapping_sub(tree.start_epoch) >= tree.epochs_before_wonders)
}

/// `EETechTreeEpoch::GetIsAvailable`: available, not obsolete (the flag only), not researched.
pub fn epoch_is_available(epoch: &EeTechNode) -> bool {
    epoch.available && !epoch.obsolete && !epoch.researched
}

/// The flags a node starts with (`EETechTreeNode::Load`): the bits of its empty prerequisites.
pub fn initial_prerequisite_flags(prerequisites: &[i32; 4]) -> u8 {
    let mut flags = 0;
    for (p, bit) in prerequisites.iter().zip(PREREQUISITE_BITS) {
        if *p == 0 {
            flags |= bit;
        }
    }
    flags
}

/// What `SetPrerequisite` does to a node.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeTechPrerequisite {
    /// the node's new prerequisite flags
    pub flags: u8,
    /// every prerequisite is met now: the node becomes available (the event hook, statistics series 0x16, the
    /// interface)
    pub becomes_available: bool,
}

/// `EETechTreeNode::SetPrerequisite`: satisfies the epoch or the first of the four prerequisites that is `id`; an
/// available or obsolete node is left alone.
pub fn set_prerequisite(node: &EeTechNode, id: i32, epoch_count: i32) -> EeTechPrerequisite {
    let mut flags = node.prerequisite_flags;
    if node.available || node.obsolete {
        return EeTechPrerequisite { flags, becomes_available: false };
    }
    if is_epoch_id(id, epoch_count) {
        flags |= PREREQUISITE_EPOCH;
    } else if let Some(k) = node.prerequisites.iter().position(|p| *p == id) {
        flags |= PREREQUISITE_BITS[k];
    }
    EeTechPrerequisite { flags, becomes_available: flags == PREREQUISITE_ALL }
}

/// `EETechTreeNode::ResearchNode`'s decision (before its research delay counts down).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeTechResearch {
    /// 0 researched, 2 already researched, 9 not possible
    pub result: i32,
    /// mark it researched and fire its event (result 0)
    pub research: bool,
}

/// `EETechTreeNode::ResearchNode`: kind 5, an id outside 1000..15999, an editor-obsoleted node or one with a research
/// delay is refused; a researched one (unless it repeats, kind 3) is "already"; an available one is researched.
pub fn research_node(node: &EeTechNode) -> EeTechResearch {
    let possible =
        node.kind != 5 && node.id >= 1000 && node.id < 16000 && !node.editor_obsoleted && node.research_delay == 0;
    if !possible {
        EeTechResearch { result: RESULT_REFUSED, research: false }
    } else if node.researched && node.kind != 3 {
        EeTechResearch { result: RESULT_ALREADY, research: false }
    } else if node.available {
        EeTechResearch { result: RESULT_RESEARCHED, research: true }
    } else {
        EeTechResearch { result: RESULT_REFUSED, research: false }
    }
}

/// The research delay after a `ResearchNode` (read after the node's event ran, which may have changed it).
pub fn research_delay_after(delay: i32) -> i32 {
    if delay > 0 {
        delay - 1
    } else {
        delay
    }
}

/// `EETechTreeEpoch::Research` of the epoch itself: 2 when researched, 0 (research it: its nodes get the epoch
/// prerequisite, its event, the tree's current epoch, the outdated nodes obsolete) when available within the tree's
/// end epoch, else 9.
pub fn research_epoch(epoch: &EeTechNode, tree: &EeTechTreeState) -> EeTechResearch {
    if epoch.researched {
        EeTechResearch { result: RESULT_ALREADY, research: false }
    } else if epoch.available && epoch.id <= tree.end_epoch {
        EeTechResearch { result: RESULT_RESEARCHED, research: true }
    } else {
        EeTechResearch { result: RESULT_REFUSED, research: false }
    }
}

/// An epoch's research counter (`EETechTreeEpoch`: `mCurrentSOL` toward `mRequiredSOL`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeTechEpochCounter {
    pub required: i32,
    pub current: i32,
    /// `mIsCountingSOL`: off between a saved game's load and its applied state
    pub counting: bool,
    pub available: bool,
    pub pad: [u8; 2],
}

/// What `AddSOL` and `UpdateAvailability` do.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeTechEpochUpdate {
    /// the counter after the call
    pub current: i32,
    /// the epoch was considered at all (within the tree's end epoch, counting / the count reached)
    pub considered: bool,
    /// the epoch becomes available (its event hook and the interface)
    pub becomes_available: bool,
    pub pad: [u8; 2],
}

/// `EETechTreeEpoch::AddSOL`: one more tech toward the epoch; enough of them make it available.
pub fn add_sol(epoch_id: i32, counter: &EeTechEpochCounter, tree: &EeTechTreeState) -> EeTechEpochUpdate {
    if !(epoch_id <= tree.end_epoch) || !counter.counting {
        return EeTechEpochUpdate { current: counter.current, ..Default::default() };
    }
    let current = counter.current.wrapping_add(1);
    EeTechEpochUpdate {
        current,
        considered: true,
        becomes_available: current >= counter.required && !counter.available,
        pad: [0; 2],
    }
}

/// `EETechTreeEpoch::UpdateAvailability`: with the count reached (within the end epoch) the epoch becomes available
/// if it was not, and its nodes get the epoch prerequisite (`considered`).
pub fn update_availability(epoch_id: i32, counter: &EeTechEpochCounter, tree: &EeTechTreeState) -> EeTechEpochUpdate {
    let considered = epoch_id <= tree.end_epoch && counter.current >= counter.required;
    EeTechEpochUpdate {
        current: counter.current,
        considered,
        becomes_available: considered && !counter.available,
        pad: [0; 2],
    }
}

/// What `EETechTree::Research` does after the epoch's `Research` returned `result`: an epoch id updates statistics
/// series 0xc, a node that counts toward the next epoch adds to that epoch's counter.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeTechTreeResearchAfter {
    Nothing = 0,
    EpochStatistics,
    CountTowardNextEpoch,
}

/// `EETechTree::Research`'s end: `next_epoch` is the tree's current epoch + 1 before the research, `epoch_slots` the
/// tree's epochs.
pub fn tree_research_after(
    id: i32,
    result: i32,
    epoch_count: i32,
    counts_toward_epoch: Option<bool>,
    next_epoch: i32,
    epoch_slots: i32,
) -> EeTechTreeResearchAfter {
    if result != RESULT_ALREADY && result != RESULT_RESEARCHED {
        return EeTechTreeResearchAfter::Nothing;
    }
    if is_epoch_id(id, epoch_count) {
        return EeTechTreeResearchAfter::EpochStatistics;
    }
    if counts_toward_epoch == Some(true) && next_epoch >= 0 && next_epoch < epoch_slots {
        EeTechTreeResearchAfter::CountTowardNextEpoch
    } else {
        EeTechTreeResearchAfter::Nothing
    }
}

/// The inputs of `EETechTreeNode::GetCost`. The C++ reads the world's factors only where the C++ read them (the
/// unit-making fields only for a node that makes a unit).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeTechCostInput {
    /// the node's cost in the resource for the unit class (its per-class table or its own)
    pub cost: i32,
    pub scale: f32,
    pub makes_unit: bool,
    /// a wonder's (kind 4) cost grows by `epoch_cost_factor` per epoch after its own (its epoch not 0)
    pub wonder_epoch_scaled: bool,
    /// the game variant's cost set contains the unit class
    pub in_cost_set: bool,
    /// the player has the unit class's upgrade (its cost scale applies)
    pub has_upgrade: bool,
    /// the player's just-in-time manufacturing for a class with a rally point
    pub jit: bool,
    pub pad: [u8; 3],
    /// the tree's current epoch less the node's
    pub epochs_since: i32,
    /// `kWorldUnitEpochCostFactor`
    pub epoch_cost_factor: f32,
    /// the game variant's cost set factor
    pub cost_set_factor: f32,
    /// the upgrade's cost scale
    pub cost_scale: f32,
    /// `kWorldJitManufacturingCostFactor` (or its deathmatch value)
    pub jit_factor: f32,
}

/// `EETechTreeNode::GetCost`: a unit-making node scaled by the wonder epochs, the cost set and the upgrade's cost
/// scale (rounded up by 0.0001, toward zero), then the just-in-time factor (toward zero); any other node by the scale.
pub fn node_cost(input: &EeTechCostInput) -> i32 {
    let mut cost = input.cost;
    let mut scale = input.scale;
    if input.makes_unit {
        if input.wonder_epoch_scaled {
            scale *= input.epochs_since as f32 * input.epoch_cost_factor + 1.0; // (commutative: the same bits)
        }
        if input.in_cost_set {
            scale *= input.cost_set_factor;
        }
        if input.has_upgrade {
            cost = ftol(input.cost_scale * cost as f32 * scale + 0.0001);
        }
        if input.jit {
            cost = ftol(input.jit_factor * cost as f32);
        }
    } else {
        cost = ftol(cost as f32 * scale + 0.0001);
    }
    cost
}

/// `EETechTreeEpoch::GetCost`'s scale: by the game variant's factors for the first, the second or a later epoch
/// advanced in this game (`epochs_advanced`: the current less the start epoch, compared unsigned).
pub fn epoch_cost_scale(
    epochs_advanced: u32,
    later_factor: f32,
    epoch_factor: f32,
    second_factor: f32,
    first_factor: f32,
    scale: f32,
) -> f32 {
    if epochs_advanced > 1 {
        later_factor * scale
    } else if epochs_advanced > 0 {
        second_factor * (epoch_factor * scale)
    } else {
        first_factor * (epoch_factor * scale)
    }
}

/// `EETechTreeNode::Load`: the factor of the node's `k`th unit class (the record's factors 1..4; 1 for the first and
/// past the fifth).
pub fn class_cost_factor(k: u32, factors: &[f32; 4]) -> f32 {
    match k {
        1..=4 => factors[k as usize - 1],
        _ => 1.0,
    }
}

/// `EETechTreeNode::Load`: a unit class's cost table, every cost times the factor (toward zero).
pub fn class_costs(costs: &[i32; 7], factor: f32) -> [i32; 7] {
    costs.map(|c| ftol(c as f32 * factor))
}

/// One id of `EETechTree::GetChecksum`'s list (the nodes of every epoch, then the epochs).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeTechChecksumEntry {
    pub id: i32,
    pub researched: bool,
    /// `GetIsObsolete()` (the virtual: a node's adds its obsolete epoch)
    pub obsolete: bool,
    pub pad: [u8; 2],
}

/// `EETechTree::GetChecksum`: the sum of id x state (1 researched, +2 obsolete) x weight, the weight from 15 up by one
/// per id (the original's epoch count; kept for the values).
pub fn checksum(entries: &[EeTechChecksumEntry]) -> u32 {
    let mut sum = 0u32;
    let mut weight = 15i32;
    for e in entries {
        let mut state = 0i32;
        if e.researched {
            state = 1;
        }
        if e.obsolete {
            state += 2;
        }
        sum = sum.wrapping_add(e.id.wrapping_mul(state).wrapping_mul(weight) as u32);
        weight = weight.wrapping_add(1);
    }
    sum
}

/// The tree as `DependsOn` reads it: a node's own id and four prerequisites, `None` for an id with no node.
pub trait PrerequisiteView {
    fn prerequisites(&self, id: i32) -> Option<[i32; 4]>;
}

/// `EETechTreeNode::DependsOn`: whether the node `own` (with `prerequisites`) is `id` or depends on it through its
/// prerequisites. Kept quirks: a prerequisite equal to the node's own id, or one with no node, ends the walk there
/// (the later prerequisites are not looked at), and the first prerequisite is compared with `id` before that test,
/// the later ones after it.
pub fn depends_on(view: &dyn PrerequisiteView, own: i32, prerequisites: &[i32; 4], id: i32) -> bool {
    if id == own {
        return true;
    }
    let first = prerequisites[0];
    if id == first {
        return true;
    }
    if first == own {
        return false;
    }
    let Some(p) = view.prerequisites(first) else { return false };
    if depends_on(view, first, &p, id) {
        return true;
    }
    for &prerequisite in &prerequisites[1..] {
        if prerequisite == own {
            return false;
        }
        if id == prerequisite {
            return true;
        }
        let Some(p) = view.prerequisites(prerequisite) else { return false };
        if depends_on(view, prerequisite, &p, id) {
            return true;
        }
    }
    false
}

/// A node as `SumCosts` reads it.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct SumCostsNode {
    pub id: i32,
    /// the virtual `GetIsAvailable()`
    pub available: bool,
    pub researched: bool,
    /// `GetUnitClass(0)`
    pub unit_class: i32,
    pub prerequisites: [i32; 4],
}

/// The tree as `SumCosts` reads it, in the C++'s order of calls.
pub trait SumCostsView {
    /// `Get(id)` (the C++ dereferences it without a test: `None` ends the walk here)
    fn node(&mut self, id: i32) -> Option<SumCostsNode>;
    /// the node's virtual `GetCost(resource, unit_class, 1.0f)`
    fn cost(&mut self, id: i32, resource: i32, unit_class: i32) -> i32;
    /// `gUnitClassTable[unit_class]->mTechnology`, `TECH_NONE` without the class
    fn class_technology(&mut self, unit_class: i32) -> i32;
}

/// `EETechTree::SumCosts`: adds the costs (resources 2..6) of the tech `id` and of every unresearched available
/// prerequisite and upgrade path to it.
pub fn sum_costs(view: &mut dyn SumCostsView, mut id: i32, unit_class: i32, costs: &mut [i32; 7]) {
    while (0..TECH_NONE).contains(&id) {
        let Some(node) = view.node(id) else { return };
        if !node.available || node.researched {
            break;
        }
        for (i, cost) in costs.iter_mut().enumerate().skip(2) {
            *cost = cost.wrapping_add(view.cost(node.id, i as i32, unit_class));
        }
        let upgrade_id = view.class_technology(node.unit_class);
        if let Some(upgrade) = view.node(upgrade_id) {
            if upgrade.unit_class != 0 && view.class_technology(upgrade.unit_class) != id {
                sum_costs(view, upgrade.id, unit_class, costs);
            }
        }
        sum_costs(view, node.prerequisites[0], unit_class, costs);
        sum_costs(view, node.prerequisites[1], unit_class, costs);
        sum_costs(view, node.prerequisites[2], unit_class, costs);
        id = node.prerequisites[3];
    }
}

/// One step of `EETechTree::ResearchToEpoch` (`EeTechEpochStep::step`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeTechEpochStepKind {
    /// `mEpochs[epoch].Unlock()`.
    Unlock = 0,
    /// `mEpochs[epoch].ResearchAll(false)`.
    ResearchAll,
    /// `mEpochs[epoch].ResearchSelf()`.
    ResearchSelf,
    /// `mEpochs[epoch].ResearchKind(7, true)`.
    ResearchKind7,
    /// The epoch's name for the local player (`mEpochNameID`, the interface's refresh).
    ShowName,
}

/// One step of `ResearchToEpoch`, on the epoch at index `epoch`.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EeTechEpochStep {
    pub step: EeTechEpochStepKind,
    pub epoch: i32,
}

/// `EETechTree::ResearchToEpoch(epoch)` over a tree of `count` epochs: the steps in the C++'s order (none for an
/// epoch out of range); `local`: the tree is the local player's.
pub fn research_to_epoch_steps(epoch: i32, count: i32, local: bool) -> Vec<EeTechEpochStep> {
    use EeTechEpochStepKind::*;
    let mut steps = Vec::new();
    let mut push = |step, epoch| steps.push(EeTechEpochStep { step, epoch });
    // `inEpoch >= kTechEpochPrehistoric && inEpoch < this->mEpochs.size()` (the size unsigned)
    if epoch >= 0 && (epoch as u32) < count as u32 {
        if epoch != 0 {
            for i in 0..epoch {
                push(Unlock, i);
                push(ResearchAll, i);
                if i + 1 == epoch && i + 1 < count {
                    push(Unlock, i + 1);
                    push(ResearchSelf, i + 1);
                }
            }
        } else {
            push(Unlock, 0);
            push(ResearchSelf, 0);
        }
        push(ResearchKind7, epoch);
        if local {
            push(ShowName, epoch);
        }
    }
    steps
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeMap;

    fn tree() -> EeTechTreeState {
        EeTechTreeState { current_epoch: 3, start_epoch: 1, end_epoch: 14, epochs_before_wonders: 1, epoch_count: 15 }
    }

    fn node(id: i32) -> EeTechNode {
        EeTechNode {
            id,
            epoch: 2,
            obsolete_epoch: 14,
            available: true,
            prerequisite_flags: PREREQUISITE_ALL,
            ..Default::default()
        }
    }

    #[test]
    fn availability() {
        let t = tree();
        let n = node(2001);
        assert!(node_is_available(&n, &t));
        assert!(!node_is_available(&EeTechNode { available: false, ..n }, &t));
        assert!(!node_is_available(&EeTechNode { obsolete: true, ..n }, &t));
        assert!(!node_is_available(&EeTechNode { obsolete_epoch: 2, ..n }, &t));
        assert!(node_is_available(&EeTechNode { obsolete_epoch: 3, ..n }, &t));
        assert!(!node_is_available(&EeTechNode { editor_obsoleted: true, ..n }, &t));
        assert!(!node_is_available(&EeTechNode { epoch: 4, ..n }, &t));
        assert!(!node_is_available(&EeTechNode { researched: true, ..n }, &t));
        assert!(node_is_available(&EeTechNode { researched: true, makes_unit: true, ..n }, &t));
        assert!(node_is_available(&EeTechNode { researched: true, kind: 3, ..n }, &t));
        // wonders: after epochs_before_wonders epochs of this game, or in the last epoch
        let w = EeTechNode { kind: 4, ..n };
        assert!(node_is_available(&w, &t));
        assert!(!node_is_available(&w, &EeTechTreeState { start_epoch: 3, ..t }));
        assert!(node_is_available(&w, &EeTechTreeState { start_epoch: 3, end_epoch: 3, ..t }));
        assert!(!node_is_available(&w, &EeTechTreeState { epochs_before_wonders: 3, ..t }));
        assert!(node_is_obsolete(&EeTechNode { obsolete_epoch: 2, ..n }, &t));
        assert!(epoch_is_available(&EeTechNode { obsolete_epoch: 0, ..n }));
        assert!(!epoch_is_available(&EeTechNode { researched: true, ..n }));
    }

    #[test]
    fn prerequisites() {
        let mut n = EeTechNode { id: 2001, prerequisites: [1500, 0, 1600, 0], ..Default::default() };
        n.prerequisite_flags = initial_prerequisite_flags(&n.prerequisites);
        assert_eq!(n.prerequisite_flags, 4 | 0x10);
        let a = set_prerequisite(&n, 2, 15);
        assert_eq!(a, EeTechPrerequisite { flags: 0x15, becomes_available: false });
        let b = set_prerequisite(&EeTechNode { prerequisite_flags: a.flags, ..n }, 1500, 15);
        assert_eq!(b, EeTechPrerequisite { flags: 0x17, becomes_available: false });
        let c = set_prerequisite(&EeTechNode { prerequisite_flags: b.flags, ..n }, 1600, 15);
        assert_eq!(c, EeTechPrerequisite { flags: 0x1f, becomes_available: true });
        // an id that is no prerequisite changes nothing, and an available or obsolete node is left alone
        assert_eq!(set_prerequisite(&n, 1700, 15).flags, n.prerequisite_flags);
        assert_eq!(set_prerequisite(&EeTechNode { available: true, ..n }, 2, 15).flags, n.prerequisite_flags);
        assert_eq!(set_prerequisite(&EeTechNode { obsolete: true, ..n }, 2, 15).flags, n.prerequisite_flags);
        // the first prerequisite equal to the id wins: two equal prerequisites need two calls
        let d = EeTechNode { prerequisites: [1500, 1500, 0, 0], prerequisite_flags: 0x19, ..n };
        assert_eq!(set_prerequisite(&d, 1500, 15).flags, 0x1b);
        // an epoch id only ever sets the epoch bit
        assert_eq!(
            set_prerequisite(&EeTechNode { prerequisites: [5, 0, 0, 0], ..n }, 5, 15).flags,
            n.prerequisite_flags | 1
        );
    }

    #[test]
    fn research_decisions() {
        let n = node(2001);
        assert_eq!(research_node(&n), EeTechResearch { result: 0, research: true });
        assert_eq!(research_node(&EeTechNode { researched: true, ..n }).result, 2);
        assert_eq!(
            research_node(&EeTechNode { researched: true, kind: 3, ..n }),
            EeTechResearch { result: 0, research: true }
        );
        assert_eq!(research_node(&EeTechNode { available: false, ..n }).result, 9);
        assert_eq!(research_node(&EeTechNode { kind: 5, ..n }).result, 9);
        assert_eq!(research_node(&EeTechNode { id: 999, ..n }).result, 9);
        assert_eq!(research_node(&EeTechNode { id: 16000, ..n }).result, 9);
        assert_eq!(research_node(&EeTechNode { research_delay: 1, ..n }).result, 9);
        assert_eq!(research_node(&EeTechNode { editor_obsoleted: true, ..n }).result, 9);
        assert_eq!(research_delay_after(2), 1);
        assert_eq!(research_delay_after(0), 0);
        assert_eq!(research_delay_after(-1), -1);
        let t = tree();
        let e = EeTechNode { id: 4, available: true, ..Default::default() };
        assert_eq!(research_epoch(&e, &t), EeTechResearch { result: 0, research: true });
        assert_eq!(research_epoch(&EeTechNode { researched: true, ..e }, &t).result, 2);
        assert_eq!(research_epoch(&e, &EeTechTreeState { end_epoch: 3, ..t }).result, 9);
        assert_eq!(research_epoch(&EeTechNode { available: false, ..e }, &t).result, 9);
    }

    #[test]
    fn epoch_counters() {
        let t = tree();
        let c = EeTechEpochCounter { required: 2, current: 0, counting: true, available: false, pad: [0; 2] };
        let a = add_sol(4, &c, &t);
        assert_eq!((a.current, a.considered, a.becomes_available), (1, true, false));
        let b = add_sol(4, &EeTechEpochCounter { current: 1, ..c }, &t);
        assert_eq!((b.current, b.becomes_available), (2, true));
        assert!(!add_sol(4, &EeTechEpochCounter { current: 1, available: true, ..c }, &t).becomes_available);
        assert_eq!(add_sol(4, &EeTechEpochCounter { counting: false, ..c }, &t).current, 0);
        assert_eq!(add_sol(15, &c, &t).current, 0);
        let u = update_availability(4, &EeTechEpochCounter { current: 2, ..c }, &t);
        assert!(u.considered && u.becomes_available);
        assert!(!update_availability(4, &c, &t).considered);
        assert!(
            !update_availability(4, &EeTechEpochCounter { current: 2, available: true, ..c }, &t).becomes_available
        );
        assert!(update_availability(4, &EeTechEpochCounter { current: 2, available: true, ..c }, &t).considered);
        assert_eq!(tree_research_after(3, 0, 15, None, 4, 15), EeTechTreeResearchAfter::EpochStatistics);
        assert_eq!(tree_research_after(3, 2, 15, None, 4, 15), EeTechTreeResearchAfter::EpochStatistics);
        assert_eq!(tree_research_after(3, 9, 15, None, 4, 15), EeTechTreeResearchAfter::Nothing);
        assert_eq!(tree_research_after(2001, 0, 15, Some(true), 4, 15), EeTechTreeResearchAfter::CountTowardNextEpoch);
        assert_eq!(tree_research_after(2001, 0, 15, Some(true), 15, 15), EeTechTreeResearchAfter::Nothing);
        assert_eq!(tree_research_after(2001, 0, 15, Some(false), 4, 15), EeTechTreeResearchAfter::Nothing);
        assert_eq!(tree_research_after(2001, 0, 15, None, 4, 15), EeTechTreeResearchAfter::Nothing);
    }

    #[test]
    fn costs() {
        let plain = EeTechCostInput { cost: 100, scale: 1.0, ..Default::default() };
        assert_eq!(node_cost(&plain), 100);
        assert_eq!(node_cost(&EeTechCostInput { scale: 0.75, ..plain }), 75);
        assert_eq!(node_cost(&EeTechCostInput { cost: 3, scale: 0.7, ..plain }), 2); // 2.1000001
        let unit = EeTechCostInput { makes_unit: true, has_upgrade: true, cost_scale: 0.9, ..plain };
        assert_eq!(node_cost(&unit), 90);
        assert_eq!(node_cost(&EeTechCostInput { has_upgrade: false, ..unit }), 100);
        assert_eq!(
            node_cost(&EeTechCostInput { wonder_epoch_scaled: true, epochs_since: 2, epoch_cost_factor: 0.5, ..unit }),
            180
        );
        assert_eq!(node_cost(&EeTechCostInput { in_cost_set: true, cost_set_factor: 2.0, ..unit }), 180);
        assert_eq!(node_cost(&EeTechCostInput { jit: true, jit_factor: 1.25, ..unit }), 112); // 112.5
        assert_eq!(epoch_cost_scale(0, 9.0, 2.0, 3.0, 0.5, 1.0), 1.0);
        assert_eq!(epoch_cost_scale(1, 9.0, 2.0, 3.0, 0.5, 1.0), 6.0);
        assert_eq!(epoch_cost_scale(2, 9.0, 2.0, 3.0, 0.5, 1.0), 9.0);
        assert_eq!(epoch_cost_scale(u32::MAX, 9.0, 2.0, 3.0, 0.5, 1.0), 9.0);
        assert_eq!(class_cost_factor(0, &[2.0, 3.0, 4.0, 5.0]), 1.0);
        assert_eq!(class_cost_factor(1, &[2.0, 3.0, 4.0, 5.0]), 2.0);
        assert_eq!(class_cost_factor(4, &[2.0, 3.0, 4.0, 5.0]), 5.0);
        assert_eq!(class_cost_factor(5, &[2.0, 3.0, 4.0, 5.0]), 1.0);
        assert_eq!(class_costs(&[0, 1, 10, 25, 33, 7, 100], 1.5), [0, 1, 15, 37, 49, 10, 150]);
    }

    #[test]
    fn checksum_weights_by_position() {
        let e = |id, researched, obsolete| EeTechChecksumEntry { id, researched, obsolete, pad: [0; 2] };
        assert_eq!(checksum(&[]), 0);
        assert_eq!(checksum(&[e(1000, true, false)]), 15_000);
        assert_eq!(checksum(&[e(1000, false, false), e(1001, true, true)]), 1001 * 3 * 16);
        assert_eq!(checksum(&[e(1000, false, true)]), 2 * 15_000);
        // wrapping
        let big: Vec<_> = (0..2000).map(|i| e(15_000 + i, true, true)).collect();
        let mut want = 0u32;
        for (k, x) in big.iter().enumerate() {
            want = want.wrapping_add((x.id as i64 * 3 * (15 + k as i64)) as u32);
        }
        assert_eq!(checksum(&big), want);
    }

    struct Tree(BTreeMap<i32, [i32; 4]>);
    impl PrerequisiteView for Tree {
        fn prerequisites(&self, id: i32) -> Option<[i32; 4]> {
            self.0.get(&id).copied()
        }
    }

    #[test]
    fn depends_on_walks_the_prerequisites_with_the_cs_quirks() {
        let t = Tree(BTreeMap::from([
            (0, [0, 0, 0, 0]),
            (1, [0, 0, 0, 0]),
            (1000, [0, 0, 0, 0]),
            (1001, [1000, 0, 0, 0]),
            (1002, [1001, 1003, 0, 0]),
            (1003, [0, 0, 0, 0]),
            (1004, [9999, 1003, 0, 0]), // a missing first prerequisite ends the walk
            (1005, [1005, 1003, 0, 0]), // so does the node's own id
            (1006, [0, 1004, 0, 0]),
        ]));
        let d = |own: i32, id: i32| depends_on(&t, own, &t.0[&own], id);
        assert!(d(1002, 1002));
        assert!(d(1002, 1001));
        assert!(d(1002, 1000));
        assert!(d(1002, 1003));
        assert!(d(1002, 0));
        assert!(!d(1002, 1));
        assert!(d(1004, 9999));
        assert!(!d(1004, 1003));
        assert!(!d(1005, 1003));
        assert!(d(1006, 9999));
        assert!(!d(1006, 1003));
        assert!(!d(1000, 1));
    }

    struct Costs {
        nodes: BTreeMap<i32, SumCostsNode>,
        technology: BTreeMap<i32, i32>,
        calls: Vec<(i32, i32)>,
    }
    impl SumCostsView for Costs {
        fn node(&mut self, id: i32) -> Option<SumCostsNode> {
            self.nodes.get(&id).copied()
        }
        fn cost(&mut self, id: i32, resource: i32, _unit_class: i32) -> i32 {
            self.calls.push((id, resource));
            id - 1000 + resource
        }
        fn class_technology(&mut self, unit_class: i32) -> i32 {
            self.technology.get(&unit_class).copied().unwrap_or(TECH_NONE)
        }
    }

    #[test]
    fn sum_costs_adds_every_open_prerequisite() {
        let n = |id, available, researched, unit_class, prerequisites| SumCostsNode {
            id,
            available,
            researched,
            unit_class,
            prerequisites,
        };
        let mut v = Costs {
            nodes: BTreeMap::from([
                (0, n(0, false, true, 0, [0; 4])),
                (1010, n(1010, true, false, 7, [1020, 0, 0, 1030])),
                (1020, n(1020, true, false, 0, [0; 4])),
                (1030, n(1030, true, true, 0, [0; 4])),
                // 1040: the upgrade of class 7 into class 8, whose technology is 1050
                (1040, n(1040, true, false, 8, [0; 4])),
            ]),
            technology: BTreeMap::from([(7, 1040), (8, 1050)]),
            calls: Vec::new(),
        };
        let mut costs = [0; 7];
        sum_costs(&mut v, 1010, 7, &mut costs);
        // 1010, then its upgrade path 1040, then prerequisite 1020; 1030 is researched
        assert_eq!(
            v.calls.iter().map(|c| c.0).collect::<Vec<_>>(),
            [1010; 5].iter().chain(&[1040; 5]).chain(&[1020; 5]).copied().collect::<Vec<_>>()
        );
        assert_eq!(
            costs,
            [0, 0, 10 + 40 + 20 + 6, 10 + 40 + 20 + 9, 10 + 40 + 20 + 12, 10 + 40 + 20 + 15, 10 + 40 + 20 + 18]
        );
        // a missing node ends the walk
        let mut costs = [0; 7];
        sum_costs(&mut v, 1099, 7, &mut costs);
        assert_eq!(costs, [0; 7]);
        sum_costs(&mut v, TECH_NONE, 7, &mut costs);
        sum_costs(&mut v, -1, 7, &mut costs);
        assert_eq!(costs, [0; 7]);
    }
    #[test]
    fn research_to_epoch_walks_every_epoch_below_then_the_kind_7_nodes() {
        use EeTechEpochStepKind::*;
        let s = |step, epoch| EeTechEpochStep { step, epoch };
        assert_eq!(research_to_epoch_steps(0, 5, false), vec![s(Unlock, 0), s(ResearchSelf, 0), s(ResearchKind7, 0)]);
        assert_eq!(
            research_to_epoch_steps(2, 5, true),
            vec![
                s(Unlock, 0),
                s(ResearchAll, 0),
                s(Unlock, 1),
                s(ResearchAll, 1),
                s(Unlock, 2),
                s(ResearchSelf, 2),
                s(ResearchKind7, 2),
                s(ShowName, 2)
            ]
        );
        // out of range: nothing
        assert!(research_to_epoch_steps(5, 5, true).is_empty());
        assert!(research_to_epoch_steps(-1, 5, true).is_empty());
        // the last epoch
        let last = research_to_epoch_steps(4, 5, false);
        assert_eq!(last.len(), 2 * 4 + 2 + 1);
        assert_eq!(last[last.len() - 1], s(ResearchKind7, 4));
    }
}
