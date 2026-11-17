//! The research order (RUST_PLAN R5b.4, the game messages lane, round 2): `EEMResearchTechnology::Execute` over the
//! store and host calls (`src/Empire Earth/Units/EEUnitHost.cpp`). The C++ `Execute` hands the sender, its
//! selection and the message's members; the body reads the units' plain state from the store and the technology's
//! node, the buildings' research state and the class's shared research through host reads, and queues the research
//! (and interrupts a unit-making research for the Bundeswehr power) through host calls.

use crate::units::group::world_unit;
use crate::units::host::{Host, HostRef, EE_UNIT_ID_NONE};
use crate::units::rally::selected;

/// `kTechNone`
const TECH_NONE: i64 = 100000;
/// `kActionIdle`
const ACTION_IDLE: i32 = 5000;
/// the best building's queue before any (`theBestQueue`)
const NO_QUEUE: i64 = 10000;

/// A technology node as the research order reads it (`player->GetTechTree()->Get(technology)`).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct ResearchNode {
    /// `mData->mKind`
    pub kind: i32,
    /// `mData->mUnitClass > 0 && gUnitClassTable[it] && its mAIKind == kAIKindHero`
    pub hero: bool,
    /// `gCivPowersManager.GetPower(kCivPowerBundeswehr, power) && power.mTechnology == mData->mPrerequisite1`
    pub bundeswehr: bool,
    /// `mData->mMakesUnit`
    pub makes_unit: bool,
    /// `EEIsEpochID(technology)`
    pub epoch: bool,
}

/// The Bundeswehr power's interruption: a living building that is not disabled and researches a technology of the
/// player's tree that makes a unit goes idle (`StartAction(kActionIdle)`, `ActionChanged`).
fn interrupt(host: &mut impl Host, player: HostRef, other: u32, complex: u32) {
    let (_, current, disabled) = host.unit_research_state(complex);
    if current != TECH_NONE
        && selected(host, other).is_some_and(|u| u.hit_points > 0)
        && !disabled
        && host.research_node(player, current as i32).is_some_and(|n| n.makes_unit)
    {
        host.unit_start_action(complex, ACTION_IDLE);
    }
}

/// `EEMResearchTechnology::Execute`: when the clicked building lives, each living built selected unit of its type
/// (or sharing its research) queues the research (`count` times: 5 when asked, else 1; first in its queue for an
/// epoch or the Bundeswehr power's technology). Kinds 0, 2, 7 and 8 and a hero's technology outside a scenario go to
/// one building instead: the one with the shortest queue (the research in progress counts one), the first on a tie.
/// With the Bundeswehr power (a kind 3 technology whose first prerequisite is the power's), a building researching
/// a unit-making technology goes idle first. A technology the tree lacks queues nothing (the original read through
/// NULL).
pub fn research_technology(
    host: &mut impl Host,
    player: HostRef,
    selection: HostRef,
    building_id: u32,
    technology: i32,
    five: bool,
) {
    let count = if five { 5 } else { 1 };
    let Some(building) = world_unit(host, building_id) else { return };
    if building.hit_points <= 0 {
        return;
    }
    let Some(node) = host.research_node(player, technology) else { return };
    let all = matches!(node.kind, 0 | 2 | 7 | 8);
    let power2 = !node.hero && node.kind == 3 && node.bundeswehr;
    let front = node.epoch || power2;
    let mut ids = Vec::new();
    if !selection.0.is_null() {
        host.group_ids(selection, &mut ids);
    }
    let mut best: Option<(u32, u32)> = None;
    let mut best_queue = NO_QUEUE;
    for &other in &ids {
        let complex = host.unit_complex_id(other);
        let Some(unit) = selected(host, other) else { continue };
        if unit.hit_points <= 0
            || !(unit.unit_type == building.unit_type || host.unit_shares_research(other, building.unit_type))
            || complex == EE_UNIT_ID_NONE
            || !selected(host, complex).is_some_and(|c| c.built != 0)
        {
            continue;
        }
        for _ in 0..count {
            if all || (node.hero && !host.scenario_game()) {
                let (queue, current, _) = host.unit_research_state(complex);
                let queue = queue + (current != TECH_NONE) as i64;
                if queue < best_queue {
                    best_queue = queue;
                    best = Some((other, complex));
                }
            } else {
                if power2 {
                    interrupt(host, player, other, complex);
                }
                host.unit_queue_research(complex, technology, front);
            }
        }
    }
    if all || (node.hero && !host.scenario_game()) {
        if let Some((other, complex)) = best {
            if power2 {
                interrupt(host, player, other, complex);
            }
            host.unit_queue_research(complex, technology, front);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::units::host::tests::{Call, MockHost};
    use crate::units::kinds::EE_OBJECT_COMPLEX_UNIT;
    use crate::units::store::EeUnitRecord;
    use std::ffi::c_void;

    const P: usize = 0x500;
    const S: usize = 0x10;
    const BARRACKS: i64 = 0x30;
    const A: u32 = 0x0100_0001;
    const B: u32 = 0x0100_0002;
    const C: u32 = 0x0100_0003;

    fn r(a: usize) -> HostRef {
        HostRef(a as *mut c_void)
    }

    fn unit(id: u32, unit_type: i64) -> EeUnitRecord {
        EeUnitRecord {
            id,
            owner: id >> 24,
            player: P as u64,
            object_kind: EE_OBJECT_COMPLEX_UNIT,
            unit_type,
            hit_points: 100,
            built: 1,
            goal_kind: -1,
            ..Default::default()
        }
    }

    fn host(kind: i32) -> MockHost {
        let mut h = MockHost::default();
        h.units.insert(A, unit(A, BARRACKS));
        h.units.insert(B, unit(B, BARRACKS));
        h.units.insert(C, unit(C, 0x31));
        h.groups.insert(S, vec![A, B, C]);
        h.research_nodes.insert(77, ResearchNode { kind, ..Default::default() });
        h
    }

    fn queued(h: &MockHost) -> Vec<(u32, i32, bool)> {
        h.calls
            .iter()
            .filter_map(|c| match c {
                Call::QueueResearch(u, t, f) => Some((*u, *t, *f)),
                _ => None,
            })
            .collect()
    }

    #[test]
    fn every_building_of_the_type_queues_it_five_times_when_asked() {
        let mut h = host(1);
        h.shares_research.push(C);
        research_technology(&mut h, r(P), r(S), A, 77, false);
        assert_eq!(queued(&h), vec![(A, 77, false), (B, 77, false), (C, 77, false)]);
        let mut h = host(1);
        h.units.get_mut(&B).unwrap().built = 0;
        research_technology(&mut h, r(P), r(S), A, 77, true);
        assert_eq!(queued(&h), vec![(A, 77, false); 5]);
        // a dead building, an unknown technology: nothing
        let mut h = host(1);
        h.units.get_mut(&A).unwrap().hit_points = 0;
        research_technology(&mut h, r(P), r(S), A, 77, false);
        research_technology(&mut h, r(P), r(S), B, 78, false);
        assert!(queued(&h).is_empty());
    }

    #[test]
    fn kinds_0_2_7_8_go_to_the_shortest_queue() {
        for kind in [0, 2, 7, 8] {
            let mut h = host(kind);
            h.research_states.insert(A, (2, TECH_NONE, false));
            h.research_states.insert(B, (1, 5, false));
            research_technology(&mut h, r(P), r(S), A, 77, true);
            // A: 2, B: 1 + the one in progress = 2: the first wins the tie
            assert_eq!(queued(&h), vec![(A, 77, false)]);
        }
        let mut h = host(0);
        h.research_states.insert(A, (3, TECH_NONE, false));
        h.research_states.insert(B, (1, TECH_NONE, false));
        research_technology(&mut h, r(P), r(S), B, 77, false);
        assert_eq!(queued(&h), vec![(B, 77, false)]);
    }

    #[test]
    fn a_hero_goes_to_one_building_outside_a_scenario() {
        let mut h = host(1);
        h.research_nodes.insert(77, ResearchNode { kind: 1, hero: true, epoch: true, ..Default::default() });
        research_technology(&mut h, r(P), r(S), A, 77, false);
        assert_eq!(queued(&h), vec![(A, 77, true)]);
        h.calls.clear();
        h.scenario = true;
        research_technology(&mut h, r(P), r(S), A, 77, false);
        assert_eq!(queued(&h), vec![(A, 77, true), (B, 77, true)]);
    }

    #[test]
    fn the_bundeswehr_power_interrupts_unit_making_research_and_queues_first() {
        let mut h = host(3);
        h.research_nodes.insert(77, ResearchNode { kind: 3, bundeswehr: true, ..Default::default() });
        h.research_nodes.insert(5, ResearchNode { makes_unit: true, ..Default::default() });
        h.research_nodes.insert(6, ResearchNode::default());
        h.research_states.insert(A, (0, 5, false));
        h.research_states.insert(B, (0, 6, false));
        research_technology(&mut h, r(P), r(S), A, 77, false);
        assert_eq!(queued(&h), vec![(A, 77, true), (B, 77, true)]);
        assert!(h.calls.contains(&Call::StartAction(A, ACTION_IDLE)));
        assert!(!h.calls.contains(&Call::StartAction(B, ACTION_IDLE)));
    }
}
