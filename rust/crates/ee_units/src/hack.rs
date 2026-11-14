//! The cheat's new unit (RUST_PLAN R5b.4, the game messages lane, round 2): `EEMHackANewUnit::Execute`'s decision
//! over host reads and calls (`src/Empire Earth/Units/EEUnitHost.cpp`): which class (the sender's cheat class when
//! the message names none), which player (the world's for ambient and resource classes), and for a land building
//! whether its four corners are on the map. The creations (the unit at the ground point facing pi, its reload and
//! power; the building at its corner, built; the creation sounds) are host calls.

use crate::host::{Host, HostRef};

/// `kNoUnitClass` as the message's `int32_t`
const NO_UNIT_CLASS: i32 = -1;
/// `kUnitCategoryAmbient`, `kUnitCategoryResource`
const CATEGORY_AMBIENT: i64 = 0;
const CATEGORY_RESOURCE: i64 = 1;

/// `EEMHackANewUnit::Execute`: the class (`unit_type`, or the sender's cheat class for none; out of the table or
/// without a class: nothing) makes a unit for the player (`player_id`; player 0 for an ambient or resource class) at
/// the tile point (x, y): a land building with its footprint (the player's scale of the class over two, a whole
/// number) on the map at its lower left corner, any other unit at the point's ground. Returns the class the message
/// keeps (the sender's cheat class when it named none).
pub fn hack_new_unit(host: &mut impl Host, sender: HostRef, unit_type: i32, x: f32, y: f32, player_id: i64) -> i32 {
    let unit_type = if unit_type == NO_UNIT_CLASS { host.player_hack_class(sender) } else { unit_type };
    if unit_type as u32 as i64 >= host.unit_class_count() {
        return unit_type;
    }
    let Some((category, land_building)) = host.unit_class_info(unit_type) else { return unit_type };
    let player_id = if category == CATEGORY_AMBIENT || category == CATEGORY_RESOURCE { 0 } else { player_id };
    if !land_building {
        host.hack_create_unit(unit_type, x, y, player_id);
        return unit_type;
    }
    let half = (host.player_class_scale(player_id, unit_type) as i64 / 2) as f32;
    let (right, top) = (x + half, y + half);
    let (left, bottom) = (x - half, y - half);
    // the corners in the original's order: upper right, upper left, lower right, lower left
    for (cx, cy) in [(right, top), (left, top), (right, bottom), (left, bottom)] {
        if !host.map_has_tile(cx as i64, cy as i64) {
            return unit_type;
        }
    }
    host.hack_create_building(unit_type, left as i64, bottom as i64, player_id);
    unit_type
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::host::tests::{Call, MockHost};
    use std::ffi::c_void;

    const P: HostRef = HostRef(0x500 as *mut c_void);

    fn host() -> MockHost {
        let mut h = MockHost { class_count: 100, hack_class: 6, scale: 3.0, map_size: 50, ..Default::default() };
        h.classes.insert(5, (4, false));
        h.classes.insert(6, (7, true));
        h.classes.insert(7, (CATEGORY_RESOURCE, false));
        h
    }

    #[test]
    fn a_unit_at_the_point_and_a_resource_for_the_world() {
        let mut h = host();
        assert_eq!(hack_new_unit(&mut h, P, 5, 10.5, 11.5, 2), 5);
        assert_eq!(h.calls, vec![Call::HackUnit(5, 10.5f32.to_bits(), 11.5f32.to_bits(), 2)]);
        h.calls.clear();
        hack_new_unit(&mut h, P, 7, 1.0, 1.0, 2);
        assert_eq!(h.calls, vec![Call::HackUnit(7, 1.0f32.to_bits(), 1.0f32.to_bits(), 0)]);
        // out of the table, no class: nothing
        h.calls.clear();
        assert_eq!(hack_new_unit(&mut h, P, 100, 1.0, 1.0, 2), 100);
        assert_eq!(hack_new_unit(&mut h, P, 8, 1.0, 1.0, 2), 8);
        assert!(h.calls.is_empty());
    }

    #[test]
    fn a_building_from_the_cheat_class_needs_its_corners_on_the_map() {
        let mut h = host();
        // the scale 3 gives a half of 1 (a whole number): the corners 9..11
        assert_eq!(hack_new_unit(&mut h, P, NO_UNIT_CLASS, 10.0, 10.0, 1), 6);
        assert_eq!(h.calls, vec![Call::HackBuilding(6, 9, 9, 1)]);
        h.calls.clear();
        hack_new_unit(&mut h, P, 6, 49.5, 10.0, 1);
        assert!(h.calls.is_empty());
    }
}
