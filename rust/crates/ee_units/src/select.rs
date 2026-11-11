//! The box selection's pick (`EEPlayer::SelectUnits`, RUST_PLAN R5b.4 leaf 4 (b)) over the unit store.
//!
//! The original clears the sender's selected group, then for each id of the decoded selection: the player's own
//! unit of that id (`RTSPlayer::GetUnit`: its list), its complex unit, and the group gets it when the group does not
//! hold it yet, it is built and it is not a path point; the last one so picked plays its selection sound. The group's
//! `Add` (`EESelectedGroup::AddIfAbsent`) adds only a valid unit, so an invalid one is picked again at each of its
//! ids. The pick returns every picked id in order, a repeat included; the C++ adds each through `Add` and plays the
//! last one's sound.
//!
//! Declared difference (unreachable from the game's selections): a unit that is not its own complex unit (a NULL
//! `mComplexUnit`, which the original dereferenced; a simple `EEUnit`, whose built flag the original read past the
//! object) is skipped. Every unit of a player's list is a complex unit that is its own (`EEUnit::Init` sets it;
//! the store's check proves it each tick).

use crate::kinds::EE_OBJECT_COMPLEX_UNIT;
use crate::store::{UnitView, EE_UNIT_COMPLEX_SELF};

/// `kUnitTypePathPoint` (ee/EEUnitClassIDs.h)
pub const EE_UNIT_TYPE_PATH_POINT: i64 = 0xdf;

/// The picked ids of `ids` for the player object `player` (the store records' `player`), in order.
pub fn select_pick(view: &impl UnitView, player: u64, ids: &[u32], out: &mut Vec<u32>) {
    // the group's units after the clear: the valid ones added so far (EEGroup::Contains is a linear search)
    let mut added: Vec<u32> = Vec::new();
    for &id in ids {
        let Some(unit) = view.unit(id) else { continue };
        if unit.player != player || unit.object_kind != EE_OBJECT_COMPLEX_UNIT || unit.complex != EE_UNIT_COMPLEX_SELF {
            continue;
        }
        if !added.contains(&id) && unit.built != 0 && unit.unit_type != EE_UNIT_TYPE_PATH_POINT {
            out.push(id);
            if unit.valid != 0 {
                added.push(id);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::store::{EeUnitRecord, EE_UNIT_COMPLEX_NONE};
    use std::collections::BTreeMap;

    const P: u64 = 0x5000;

    fn unit(id: u32) -> EeUnitRecord {
        EeUnitRecord {
            id,
            owner: id >> 24,
            object: 0x1000 + id as u64,
            player: P,
            object_kind: EE_OBJECT_COMPLEX_UNIT,
            complex: EE_UNIT_COMPLEX_SELF,
            valid: 1,
            built: 1,
            goal_kind: -1,
            hit_points: 10,
            tile_x: 1,
            tile_y: 2,
            category: 3,
            unit_type: 40,
            domains: 2,
            land_building: 0,
            ..Default::default()
        }
    }

    fn pick(units: &[EeUnitRecord], ids: &[u32]) -> Vec<u32> {
        let view: BTreeMap<u32, EeUnitRecord> = units.iter().map(|u| (u.id, *u)).collect();
        let mut out = Vec::new();
        select_pick(&view, P, ids, &mut out);
        out
    }

    #[test]
    fn picks_built_own_units_once_in_order() {
        let units = [unit(0x0100_0003), unit(0x0100_0001), unit(0x0100_0002)];
        assert_eq!(
            pick(&units, &[0x0100_0002, 0x0100_0001, 0x0100_0002, 0x0100_0003]),
            vec![0x0100_0002, 0x0100_0001, 0x0100_0003]
        );
        assert!(pick(&units, &[]).is_empty());
    }

    #[test]
    fn skips_missing_foreign_unbuilt_and_path_points() {
        let mut foreign = unit(0x0200_0001);
        foreign.player = 0x6000;
        let mut unbuilt = unit(0x0100_0002);
        unbuilt.built = 0;
        let mut path_point = unit(0x0100_0003);
        path_point.unit_type = EE_UNIT_TYPE_PATH_POINT;
        let units = [foreign, unbuilt, path_point, unit(0x0100_0004)];
        assert_eq!(
            pick(&units, &[0x0200_0001, 0x0100_0002, 0x0100_0003, 0x0100_0009, 0xffff_ffff, 0x0100_0004]),
            vec![0x0100_0004]
        );
    }

    #[test]
    fn an_invalid_unit_is_picked_at_every_id() {
        let mut invalid = unit(0x0100_0005);
        invalid.valid = 0;
        let units = [invalid, unit(0x0100_0006)];
        assert_eq!(
            pick(&units, &[0x0100_0005, 0x0100_0006, 0x0100_0005, 0x0100_0006]),
            vec![0x0100_0005, 0x0100_0006, 0x0100_0005]
        );
    }

    #[test]
    fn skips_units_that_are_not_their_own_complex_unit() {
        let mut none = unit(0x0100_0007);
        none.complex = EE_UNIT_COMPLEX_NONE;
        let mut simple = unit(0x0100_0008);
        simple.object_kind = crate::kinds::EE_OBJECT_UNIT;
        let units = [none, simple];
        assert!(pick(&units, &[0x0100_0007, 0x0100_0008]).is_empty());
    }

    #[test]
    fn sixty_three_ids_the_selections_limit() {
        let units: Vec<_> = (0..63).map(|i| unit(0x0300_0000 + i)).collect();
        let ids: Vec<u32> = units.iter().rev().map(|u| u.id).collect();
        assert_eq!(pick(&units, &ids), ids);
    }
}
