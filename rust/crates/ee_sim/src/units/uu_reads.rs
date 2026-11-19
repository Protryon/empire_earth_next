//! The unit utilities' plain reads (round 3): the bodies EEFOFindNewTargetNearOldTarget.cpp still had that only read
//! the world, as pure functions of what they read. The game's entry points (uu_ffi.rs) read the members through
//! `ee_sim::world`'s views; the C++ keeps forwarders for its callers.

use ee_base::math::types::Ge2DPoint;
use ee_base::math::types::GeVector;

use crate::combat::regions::EeCombatUnitRegions;

// the unit categories (ee/EEUnitClassIDs.h)
const CATEGORY_AMBIENT: u64 = 0;
const CATEGORY_RESOURCE: u64 = 1;
const CATEGORY_BUILDING: u64 = 2;
const CATEGORY_AIRPLANE: u64 = 3;
const CATEGORY_PROJECTILE: u64 = 9;
const CATEGORY_SUBMERGED_NAVAL: u64 = 0xa;
const CATEGORY_BOMB: u64 = 0xb;
const CATEGORY_MISSILE: u64 = 0xc;
const CATEGORY_BUILDING19: u64 = 0x13;
const CATEGORY_TORPEDO: u64 = 0x14;
const CATEGORY_MINING_BUILDING: u64 = 0x18;
const CATEGORY_GATE: u64 = 0x19;
const CATEGORY_FORTRESS: u64 = 0x1d;
const CATEGORY_BUILDING_PRIEST: u64 = 0x1e;
const CATEGORY_PLANET_SPACESHIP: u64 = 0x22;

// the resources (ee/EEResourceType.h)
const RESOURCE_FOOD: u64 = 2;
const RESOURCE_WOOD: u64 = 3;
const RESOURCE_STONE: u64 = 4;
const RESOURCE_GOLD: u64 = 5;
const RESOURCE_IRON: u64 = 6;

/// `EEIsUnitMovable`: alive, and not a building, aircraft, shot or fixed object (a resource only when its class
/// moves).
pub fn is_unit_movable(hit_points: i64, category: u64, class_max_velocity: f32) -> bool {
    if hit_points <= 0 {
        return false;
    }
    match category {
        CATEGORY_RESOURCE => class_max_velocity != 0.0,
        CATEGORY_AMBIENT
        | CATEGORY_BUILDING
        | CATEGORY_AIRPLANE
        | CATEGORY_PROJECTILE
        | CATEGORY_SUBMERGED_NAVAL
        | CATEGORY_BOMB
        | CATEGORY_MISSILE
        | CATEGORY_BUILDING19
        | CATEGORY_TORPEDO
        | CATEGORY_MINING_BUILDING
        | CATEGORY_GATE
        | CATEGORY_FORTRESS
        | CATEGORY_BUILDING_PRIEST
        | CATEGORY_PLANET_SPACESHIP => false,
        _ => true,
    }
}

/// `EEIsSameResearch`: the same technology, or the twins 0x432 and 0x5ab.
pub fn is_same_research(first: i32, second: i32) -> bool {
    first == second || (first == 0x432 && second == 0x5ab) || (first == 0x5ab && second == 0x432)
}

/// `EEGetResourceIndex`: a resource's column (wood 0, stone 1, gold 2, iron 3; food 5 off the map, else 6 for a
/// land building's (a farm), 4 for the others); -1 for none.
pub fn resource_index(resource_type: u64, on_map: bool, land_building: bool) -> i32 {
    match resource_type {
        RESOURCE_IRON => 3,
        RESOURCE_GOLD => 2,
        RESOURCE_STONE => 1,
        RESOURCE_WOOD => 0,
        RESOURCE_FOOD if !on_map => 5,
        RESOURCE_FOOD if land_building => 6,
        RESOURCE_FOOD => 4,
        _ => -1,
    }
}

/// `EEIsCloakedEnemyOf`: on the map, cloaked (when it can be), and its owner at war with another player.
pub fn is_cloaked_enemy(on_map: bool, cloakable: bool, cloaked: bool, same_player: bool, owner_at_war: bool) -> bool {
    on_map && (if cloakable { cloaked } else { false }) && !same_player && owner_at_war
}

/// `EEGetLeadUnitID`: the viewed unit's id, else the selected group's first unit's (None: -1).
pub fn lead_unit_id(group_first: Option<u32>, viewed: Option<u32>) -> Option<u32> {
    viewed.or(group_first)
}

/// `EEGetAimPoint`: the point 90% of `range` from `target` towards `position`.
pub fn aim_point(position: [f32; 3], target: [f32; 3], range: f32) -> [f32; 3] {
    let mut direction = GeVector { x: position[0] - target[0], y: position[1] - target[1], z: position[2] - target[2] };
    direction.normalize();
    let scale = range * 0.9;
    let offset = [scale * direction.x, scale * direction.y, scale * direction.z];
    [target[0] + offset[0], target[1] + offset[1], target[2] + offset[2]]
}

/// A tile rectangle (left, top, right, bottom), edges included.
pub type TileRect = (i32, i32, i32, i32);

/// The collector `EEGetUnitsInRectangle(rect, units, predicate, true)` (ee/EEUnitCollector.h) over the map's unit
/// regions: the regions under `rect` column by column, each from its last unit, the units whose tile is in `rect`
/// and that `keep` holds for; sorted by id and made unique when one was bigger than a tile. `unit` gives a unit's
/// tile, id and class tile size. Appended to `out`; its length.
pub fn units_in_rectangle(
    regions: &EeCombatUnitRegions,
    rect: TileRect,
    mut unit: impl FnMut(u64) -> ([i32; 2], u32, i32),
    mut keep: impl FnMut(u64) -> bool,
    out: &mut Vec<u64>,
) -> i32 {
    let (left, top, right, bottom) = regions.rect(rect.0, rect.1, rect.2, rect.3);
    let mut multiple = false;
    for x in left..=right {
        for y in top..=bottom {
            let Some(region) = regions.region(x as u32, y as u32) else { continue };
            for &u in region.iter().rev() {
                let ([tx, ty], _, size) = unit(u);
                if tx >= rect.0 && tx <= rect.2 && ty >= rect.1 && ty <= rect.3 && keep(u) {
                    if size > 1 {
                        multiple = true;
                    }
                    out.push(u);
                }
            }
        }
    }
    if multiple {
        let mut keyed: Vec<(u32, u64)> = out.iter().map(|&u| (unit(u).1, u)).collect();
        keyed.sort_by_key(|k| k.0);
        keyed.dedup_by_key(|k| k.0);
        *out = keyed.into_iter().map(|k| k.1).collect();
    }
    out.len() as i32
}

/// `UMCeil`: rounds to nearest, then steps up when that went down.
fn ceil(value: f32) -> i32 {
    let r = ee_base::math::um::lrintf(value);
    if (r as f32) < value {
        r.wrapping_add(1)
    } else {
        r
    }
}

/// `EEGetUnitsNear`'s rectangle: the tiles under the square around `center` (floor of the low edges, ceiling of
/// the high ones).
pub fn near_rect(center: [f32; 3], radius: f32) -> TileRect {
    let floor = ee_base::math::um::floor;
    (floor(center[0] - radius), floor(center[1] - radius), ceil(center[0] + radius), ceil(center[1] + radius))
}

/// `EEUUCircleScreen`: a unit at `location` within `radius` of `center` (squared ground distances).
pub fn in_circle(location: [f32; 3], center: [f32; 3], radius: f32) -> bool {
    Ge2DPoint { x: location[0], y: location[1] }.distance_to_squared(Ge2DPoint { x: center[0], y: center[1] })
        <= radius * radius
}

/// `EEGetUnitsInRadius`' rectangle around `center`: `radius` tiles before it, one less after it unless the point
/// sits in its tile's middle half; None for a negative radius.
pub fn radius_rect(center: [f32; 2], radius: f32) -> Option<TileRect> {
    let floor = ee_base::math::um::floor;
    let y = floor(center[1]);
    let x = floor(center[0]);
    let fx = center[0] - x as f32;
    let fy = center[1] - y as f32;
    let r = ee_base::math::um::lrintf(radius);
    let far = if fx <= 0.25 || fx >= 0.75 || fy <= 0.25 || fy >= 0.75 { r - 1 } else { r };
    (r >= 0).then_some((x - r, y - r, x + far, y + far))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn movable_units_are_alive_and_not_fixed() {
        assert!(is_unit_movable(5, 5, 0.0));
        assert!(!is_unit_movable(0, 5, 1.0));
        assert!(!is_unit_movable(5, CATEGORY_BUILDING, 1.0));
        assert!(!is_unit_movable(5, CATEGORY_PLANET_SPACESHIP, 1.0));
        assert!(is_unit_movable(5, CATEGORY_RESOURCE, 0.5));
        assert!(!is_unit_movable(5, CATEGORY_RESOURCE, 0.0));
    }

    #[test]
    fn research_twins_and_resource_columns() {
        assert!(is_same_research(7, 7));
        assert!(is_same_research(0x432, 0x5ab) && is_same_research(0x5ab, 0x432));
        assert!(!is_same_research(0x432, 0x433));
        assert_eq!(resource_index(RESOURCE_WOOD, true, false), 0);
        assert_eq!(resource_index(RESOURCE_IRON, true, false), 3);
        assert_eq!(resource_index(RESOURCE_FOOD, false, true), 5);
        assert_eq!(resource_index(RESOURCE_FOOD, true, true), 6);
        assert_eq!(resource_index(RESOURCE_FOOD, true, false), 4);
        assert_eq!(resource_index(0, true, false), -1);
    }

    #[test]
    fn cloaked_enemies_and_the_lead_unit() {
        assert!(is_cloaked_enemy(true, true, true, false, true));
        assert!(!is_cloaked_enemy(true, false, true, false, true));
        assert!(!is_cloaked_enemy(true, true, true, true, true));
        assert!(!is_cloaked_enemy(false, true, true, false, true));
        assert_eq!(lead_unit_id(Some(4), Some(9)), Some(9));
        assert_eq!(lead_unit_id(Some(4), None), Some(4));
        assert_eq!(lead_unit_id(None, None), None);
    }

    #[test]
    fn the_aim_point_is_nine_tenths_of_the_range_out() {
        let p = aim_point([10.0, 0.0, 0.0], [0.0, 0.0, 0.0], 5.0);
        assert_eq!(p, [4.5, 0.0, 0.0]);
        assert_eq!(aim_point([1.0, 1.0, 1.0], [1.0, 1.0, 1.0], 5.0), [1.0, 1.0, 1.0]);
    }

    #[test]
    fn the_rectangles_round_as_the_cpp() {
        assert_eq!(near_rect([5.5, 5.5, 0.0], 1.2), (4, 4, 7, 7));
        assert_eq!(near_rect([5.0, 5.0, 0.0], 1.0), (4, 4, 6, 6));
        assert_eq!(radius_rect([5.5, 5.5], 2.0), Some((3, 3, 7, 7)));
        assert_eq!(radius_rect([5.1, 5.5], 2.0), Some((3, 3, 6, 6)));
        assert_eq!(radius_rect([5.5, 5.5], -1.0), None);
        assert!(in_circle([1.0, 1.0, 9.0], [0.0, 0.0, 0.0], 1.5));
        assert!(!in_circle([2.0, 0.0, 0.0], [0.0, 0.0, 0.0], 1.5));
    }

    #[test]
    fn the_collector_walks_regions_backwards_and_uniques_large_units() {
        // 2-tile regions on a 3 x 3 grid; units are their ids, tile (id, id), size 2 for id 7
        let mut r = EeCombatUnitRegions::new(3, 2);
        r.add(0, 0, 1);
        r.add(0, 0, 2);
        r.add(1, 1, 3);
        r.add(1, 1, 7);
        r.add(0, 0, 7);
        let info = |u: u64| ([u as i32 % 6, u as i32 % 6], u as u32, if u == 7 { 2 } else { 1 });
        let mut out = Vec::new();
        assert_eq!(units_in_rectangle(&r, (0, 0, 3, 3), info, |_| true, &mut out), 4);
        assert_eq!(out, vec![1, 2, 3, 7]);
        let mut out = Vec::new();
        assert_eq!(units_in_rectangle(&r, (0, 0, 3, 3), info, |u| u != 7, &mut out), 3);
        assert_eq!(out, vec![2, 1, 3]);
        let mut out = vec![99];
        units_in_rectangle(&r, (2, 2, 3, 3), info, |_| true, &mut out);
        assert_eq!(out, vec![99, 3]);
    }
}
