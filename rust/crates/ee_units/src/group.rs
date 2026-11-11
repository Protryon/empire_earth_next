//! The group commands over the unit store (RUST_PLAN R5b.4 leaf 4 (c), leaf 5): which of a selection's units an
//! active group takes (`EEActiveGroup::Fill`'s test).
//!
//! `EEActiveGroup::Fill(selection, sort, slot)` empties the active group, then takes each unit of the selection, in
//! the selection's order, that is valid, has hit points and belongs to the formation slot
//! (`EEActiveGroup::IsInFormationSlot`: 0 any; 1 air; 2 land or a land building; 3 sea but not submerged; 4 sea and
//! submerged; 5 the space domain; any other slot none). The test reads only the units' plain state, so it is
//! `fill_members` over the store; the C++ keeps the group's bookkeeping (the clear, the formation, the sort, each
//! taken unit's move out of its old active group) and applies the answer in order.
//!
//! A unit of a selection that is in no player's list has no record: it is not taken. The C++ test read the object;
//! a unit leaves its list only dead (`RTSPlayer::RemoveKilledUnits`), and the capture of the C++ test found no
//! such unit it would take (PORT_LEARNINGS "R5b.4, fifth round").

use crate::store::{EeUnitRecord, UnitView};

/// `kMovement*` (ee/EEUnitClassIDs.h `EEMovementDomain`)
pub const EE_MOVEMENT_AIR: u8 = 1;
pub const EE_MOVEMENT_LAND: u8 = 2;
pub const EE_MOVEMENT_SEA: u8 = 4;
/// `kUnitCategorySubmergedNaval`
pub const EE_UNIT_CATEGORY_SUBMERGED_NAVAL: i64 = 0xa;

/// `EEActiveGroup::IsInFormationSlot` over a record.
pub fn in_formation_slot(unit: &EeUnitRecord, slot: i64) -> bool {
    let domains = unit.domains;
    match slot {
        0 => true,
        1 => domains & EE_MOVEMENT_AIR != 0,
        2 => domains & EE_MOVEMENT_LAND != 0 || unit.land_building != 0,
        3 => domains & EE_MOVEMENT_SEA != 0 && unit.category != EE_UNIT_CATEGORY_SUBMERGED_NAVAL,
        4 => domains & EE_MOVEMENT_SEA != 0 && unit.category == EE_UNIT_CATEGORY_SUBMERGED_NAVAL,
        5 => (domains >> 3) & 1 != 0,
        _ => false,
    }
}

/// `EEActiveGroup::Fill`'s test: the indices into `ids` (a selection's units in its order) of the units the group
/// takes for `slot`, in order (a unit listed twice is taken twice, as the original's loop did).
pub fn fill_members(view: &impl UnitView, ids: &[u32], slot: i64, out: &mut Vec<u32>) {
    for (index, &id) in ids.iter().enumerate() {
        let Some(unit) = view.unit(id) else { continue };
        if unit.valid != 0 && unit.hit_points > 0 && in_formation_slot(unit, slot) {
            out.push(index as u32);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeMap;

    fn unit(id: u32, domains: u8) -> EeUnitRecord {
        EeUnitRecord { id, valid: 1, hit_points: 10, domains, category: 1, unit_type: 3, ..Default::default() }
    }

    #[test]
    fn the_formation_slots() {
        let land = unit(1, EE_MOVEMENT_LAND);
        let air = unit(2, EE_MOVEMENT_AIR);
        let sea = unit(3, EE_MOVEMENT_SEA);
        let sub = EeUnitRecord { category: EE_UNIT_CATEGORY_SUBMERGED_NAVAL, ..unit(4, EE_MOVEMENT_SEA) };
        let space = unit(5, 8);
        let building = EeUnitRecord { land_building: 1, ..unit(6, 0) };
        let amphibious = unit(7, EE_MOVEMENT_LAND | EE_MOVEMENT_SEA);
        let all = [land, air, sea, sub, space, building, amphibious];
        let slots: Vec<Vec<u32>> =
            (0..7).map(|s| all.iter().filter(|u| in_formation_slot(u, s)).map(|u| u.id).collect()).collect();
        assert_eq!(
            slots,
            vec![vec![1, 2, 3, 4, 5, 6, 7], vec![2], vec![1, 6, 7], vec![3, 7], vec![4], vec![5], vec![]]
        );
        assert!(!in_formation_slot(&land, -1));
    }

    #[test]
    fn fill_takes_valid_living_units_of_the_slot_in_order() {
        let mut view = BTreeMap::new();
        for u in [
            unit(10, EE_MOVEMENT_LAND),
            EeUnitRecord { valid: 0, ..unit(11, EE_MOVEMENT_LAND) },
            EeUnitRecord { hit_points: 0, ..unit(12, EE_MOVEMENT_LAND) },
            EeUnitRecord { hit_points: -5, ..unit(13, EE_MOVEMENT_LAND) },
            unit(14, EE_MOVEMENT_AIR),
            unit(15, EE_MOVEMENT_LAND),
        ] {
            view.insert(u.id, u);
        }
        let mut out = Vec::new();
        // 99 is in no list: not taken; 10 twice: taken twice
        fill_members(&view, &[15, 99, 10, 11, 12, 13, 14, 10], 2, &mut out);
        assert_eq!(out, vec![0, 2, 7]);
        out.clear();
        fill_members(&view, &[15, 14, 10], 1, &mut out);
        assert_eq!(out, vec![1]);
        out.clear();
        fill_members(&view, &[], 0, &mut out);
        assert!(out.is_empty());
    }
}
