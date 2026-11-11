//! The decisions' and the walks' tests: hand-built records and fake worlds (every host call logged).

use std::collections::BTreeMap;

use crate::decide::*;
use crate::walks::*;

fn convertible() -> EeCombatCanConvertIn {
    EeCombatCanConvertIn {
        enemy: 1,
        conversion_rate: 1.0,
        resistance: 0.5,
        target_hit_points: 10,
        attackable: 1,
        damage: 1.0,
        power: 100,
        cost: 50,
        range: 4.0,
        ..Default::default()
    }
}

#[test]
fn can_convert_follows_the_original_rules() {
    let base = convertible();
    assert_eq!(can_convert(&base), EeCombatCanConvertOut { result: 1, flag_written: 1, flag: 0 });
    // shielded: the flag is written and the result false
    let shielded = EeCombatCanConvertIn { shielded: 1, ..base };
    assert_eq!(can_convert(&shielded), EeCombatCanConvertOut { result: 0, flag_written: 1, flag: 1 });
    // a cyber priest or target: no shield test, the flag stays unwritten
    let cyber = EeCombatCanConvertIn { target_cyber: 1, shielded: 1, ..base };
    assert_eq!(can_convert(&cyber), EeCombatCanConvertOut { result: 1, flag_written: 0, flag: 0 });
    // early outs write nothing
    for i in [
        EeCombatCanConvertIn { same_player: 1, ..base },
        EeCombatCanConvertIn { enemy: 0, ..base },
        EeCombatCanConvertIn { conversion_rate: 0.0, ..base },
        EeCombatCanConvertIn { target_disabled: 1, ..base },
    ] {
        assert_eq!(can_convert(&i), EeCombatCanConvertOut::default());
    }
    // NaN rate goes on (`<= 0` is false), a protected disabled target too
    assert_eq!(can_convert(&EeCombatCanConvertIn { conversion_rate: f32::NAN, ..base }).result, 1);
    assert_eq!(can_convert(&EeCombatCanConvertIn { target_disabled: 1, target_protected: 1, ..base }).result, 1);
    // resistance 1: only a town center with the adaptation power (not a crusader's nor a building priest's)
    let resistant = EeCombatCanConvertIn { resistance: 1.0, ..base };
    assert_eq!(can_convert(&resistant).result, 0);
    let town = EeCombatCanConvertIn { target_town_center: 1, adaptation: 1, ..resistant };
    assert_eq!(can_convert(&town).result, 1);
    assert_eq!(can_convert(&EeCombatCanConvertIn { crusader: 1, ..town }).result, 0);
    // faith, damage, hit points, priests
    assert_eq!(can_convert(&EeCombatCanConvertIn { power: 49, ..base }).result, 0);
    assert_eq!(can_convert(&EeCombatCanConvertIn { power: 50, ..base }).result, 1);
    assert_eq!(can_convert(&EeCombatCanConvertIn { damage: 0.0, ..base }).result, 0);
    assert_eq!(can_convert(&EeCombatCanConvertIn { target_hit_points: 0, ..base }).result, 0);
    assert_eq!(can_convert(&EeCombatCanConvertIn { same_category: 1, ..base }).result, 0);
    assert_eq!(can_convert(&EeCombatCanConvertIn { same_category: 1, converts_priests: 1, ..base }).result, 1);
    // buildings: only town centers and settlements, with the power, from the Copper Age
    let building = EeCombatCanConvertIn { target_land_building: 1, ..base };
    assert_eq!(can_convert(&building).result, 0);
    let settlement = EeCombatCanConvertIn { target_settlement: 1, adaptation: 1, at_copper: 1, ..building };
    assert_eq!(can_convert(&settlement).result, 1);
    assert_eq!(can_convert(&EeCombatCanConvertIn { at_copper: 0, ..settlement }).result, 0);
    assert_eq!(can_convert(&EeCombatCanConvertIn { converts_buildings: 1, ..building }).result, 1);
    // a building priest: its range (3D, strict), no land priests without converts_priests
    let temple = EeCombatCanConvertIn { building_priest: 1, target_x: 3.0, target_z: 0.0, ..base };
    assert_eq!(can_convert(&temple).result, 1);
    assert_eq!(can_convert(&EeCombatCanConvertIn { target_x: 4.0, ..temple }).result, 0);
    assert_eq!(can_convert(&EeCombatCanConvertIn { target_x: 3.0, target_z: 3.0, ..temple }).result, 0);
    assert_eq!(can_convert(&EeCombatCanConvertIn { target_land_priest: 1, ..temple }).result, 0);
}

#[test]
fn can_heal_needs_a_wounded_friend_in_the_set() {
    let base = EeCombatCanHealIn {
        has_set: 1,
        repairable: 1,
        hit_points: 50,
        maximum: 100,
        threshold: 0.5,
        same_player: 1,
        in_set: 1,
        ..Default::default()
    };
    assert_eq!(can_heal(&base), 1);
    assert_eq!(can_heal(&EeCombatCanHealIn { hit_points: 51, ..base }), 0);
    assert_eq!(can_heal(&EeCombatCanHealIn { same_unit: 1, ..base }), 0);
    assert_eq!(can_heal(&EeCombatCanHealIn { same_player: 0, ..base }), 0);
    assert_eq!(can_heal(&EeCombatCanHealIn { same_player: 0, allied: 1, ..base }), 1);
    assert_eq!(can_heal(&EeCombatCanHealIn { in_set: 0, ..base }), 0);
    assert_eq!(can_heal(&EeCombatCanHealIn { has_set: 0, ..base }), 0);
    assert_eq!(can_heal(&EeCombatCanHealIn { repairable: 0, ..base }), 0);
    // a zero maximum: 50 / 0 is infinite, above any threshold
    assert_eq!(can_heal(&EeCombatCanHealIn { maximum: 0, ..base }), 0);
}

#[test]
fn drop_point_scales_lowers_and_places() {
    let mut t = [[0.0f32; 4]; 4];
    t[0][0] = 1.0;
    t[1][1] = 1.0;
    t[2][2] = 1.0;
    t[3][3] = 1.0;
    t[0][3] = 10.0;
    t[1][3] = 20.0;
    t[2][3] = 1.0;
    let model = EeCombatDropIn {
        kind: DROP_KIND_MODEL,
        offset: [1.0, 2.0, 4.0],
        scale: 0.5,
        built: 1,
        hit_points: 25,
        max_hit_points: 100,
        transformation: t,
        ..Default::default()
    };
    assert_eq!(drop_point(&model), EeCombatDropOut { x: 10.5, y: 21.0, z: 3.0 });
    // unbuilt: the height x hit points / maximum
    assert_eq!(drop_point(&EeCombatDropIn { built: 0, ..model }), EeCombatDropOut { x: 10.5, y: 21.0, z: 1.5 });
    let tile = EeCombatDropIn { kind: DROP_KIND_TILE, tile_x: 3, tile_y: 7, tile_height: 2.25, ..Default::default() };
    assert_eq!(drop_point(&tile), EeCombatDropOut { x: 3.5, y: 7.5, z: 2.25 });
    assert_eq!(drop_point(&EeCombatDropIn { kind: DROP_KIND_NONE, ..model }), EeCombatDropOut::default());
}

/// A world of walls: per tile, the height its top reaches (bare: 1 above the ground for any wall).
struct Walls {
    tops: BTreeMap<(i32, i32), f32>,
    line: Vec<(i32, i32)>,
    top: EeCombatTargetTop,
    log: Vec<String>,
}

impl LineHost for Walls {
    fn start_height(&mut self, bare: bool) -> f32 {
        self.log.push(format!("start {bare}"));
        1.0
    }
    fn tile_line(&mut self, from_x: i32, from_y: i32, to_x: i32, to_y: i32) -> Vec<(i32, i32)> {
        self.log.push(format!("line {from_x},{from_y} {to_x},{to_y}"));
        self.line.clone()
    }
    fn target_top(&mut self) -> EeCombatTargetTop {
        self.log.push("top".into());
        self.top
    }
    fn tile_top_above(&mut self, x: i32, y: i32, height: f32, bare: bool) -> bool {
        self.log.push(format!("above {x},{y} {height} {bare}"));
        self.tops.get(&(x, y)).is_some_and(|&t| t > height)
    }
}

fn walls(line: &[(i32, i32)], tops: &[((i32, i32), f32)]) -> Walls {
    Walls {
        tops: tops.iter().copied().collect(),
        line: line.to_vec(),
        top: EeCombatTargetTop {
            present: 1,
            box_top: 2.0,
            scale: 1.0,
            built: 0,
            hit_points: 50,
            max_hit_points: 100,
            z: 3.0,
        },
        log: Vec::new(),
    }
}

#[test]
fn line_of_fire_rises_from_the_weapon_to_the_top() {
    // from (0,0) to (4,0): three tiles between, the shot from z 1 to 2 * 0.5 + 3 = 4 in four steps
    let input = EeCombatLineIn { from_x: 0, from_y: 0, to_x: 4, to_y: 0, target_alive: 1, ..Default::default() };
    let mut w = walls(&[(1, 0), (2, 0), (3, 0)], &[]);
    assert_eq!(line_blocked(&input, &mut w), EeCombatLineOut { blocked: 0, x: 3, y: 0 });
    assert_eq!(
        w.log,
        ["start false", "line 0,0 4,0", "top", "above 1,0 1.75 false", "above 2,0 2.5 false", "above 3,0 3.25 false"]
    );
    // a wall at (2,0) reaching 2.6 stops the shot there
    let mut w = walls(&[(1, 0), (2, 0), (3, 0)], &[((2, 0), 2.6)]);
    assert_eq!(line_blocked(&input, &mut w), EeCombatLineOut { blocked: 1, x: 2, y: 0 });
    assert_eq!(w.log.len(), 5);
    // bare (melee): no top, the height stays the start's
    let mut w = walls(&[(1, 0), (2, 0), (3, 0)], &[((3, 0), 1.5)]);
    let bare = EeCombatLineIn { bare: 1, ..input };
    assert_eq!(line_blocked(&bare, &mut w), EeCombatLineOut { blocked: 1, x: 3, y: 0 });
    assert_eq!(w.log[..3], ["start true", "line 0,0 4,0", "above 1,0 1 true"]);
}

#[test]
fn line_of_fire_diagonal_corners_and_skips() {
    // a diagonal step blocked by both corner tiles at the corner height
    let input =
        EeCombatLineIn { from_x: 0, from_y: 0, to_x: 2, to_y: 2, target_alive: 1, bare: 1, ..Default::default() };
    let mut w = walls(&[(1, 1)], &[((1, 0), 5.0), ((0, 1), 5.0)]);
    assert_eq!(line_blocked(&input, &mut w), EeCombatLineOut { blocked: 1, x: 1, y: 0 });
    // the end: a diagonal last step into the target, both corners walled
    let mut w = walls(&[(1, 1)], &[((2, 1), 5.0), ((1, 2), 5.0)]);
    assert_eq!(line_blocked(&input, &mut w), EeCombatLineOut { blocked: 1, x: 2, y: 1 });
    // adjacent, dead or skipping: no walk, (0, 0)
    for i in [
        EeCombatLineIn { to_x: 1, to_y: 0, ..input },
        EeCombatLineIn { target_alive: 0, ..input },
        EeCombatLineIn { skips_check: 1, ..input },
    ] {
        let mut w = walls(&[(1, 1)], &[]);
        assert_eq!(line_blocked(&i, &mut w), EeCombatLineOut { blocked: 0, x: 0, y: 0 });
        assert!(w.log.is_empty());
    }
    // no tiles: step 0, the end test only
    let mut w = walls(&[], &[]);
    let far = EeCombatLineIn { bare: 0, ..input };
    assert_eq!(line_blocked(&far, &mut w), EeCombatLineOut { blocked: 0, x: 0, y: 0 });
    assert_eq!(w.log, ["start false", "line 0,0 2,2", "top"]);
}

/// A map of tile lists; units are handles 100 + n, nodes 1000 + n.
struct Field {
    tiles: BTreeMap<(i32, i32), Vec<u64>>,
    distance: BTreeMap<u64, f32>,
    own: Vec<u64>,
    untargetable: Vec<u64>,
    log: Vec<String>,
}

impl AreaHost for Field {
    fn tile_first(&mut self, x: i32, y: i32) -> Handle {
        self.tiles.get(&(x, y)).and_then(|v| v.first()).map_or(0, |&u| u + 900)
    }
    fn node_next(&mut self, node: Handle) -> Handle {
        let unit = node - 900;
        for v in self.tiles.values() {
            if let Some(p) = v.iter().position(|&u| u == unit) {
                return v.get(p + 1).map_or(0, |&u| u + 900);
            }
        }
        0
    }
    fn node_unit(&mut self, node: Handle) -> Handle {
        node - 900
    }
    fn distance_squared(&mut self, unit: Handle) -> f32 {
        self.distance[&unit]
    }
    fn targetable(&mut self, unit: Handle) -> bool {
        !self.untargetable.contains(&unit)
    }
    fn same_player(&mut self, unit: Handle) -> bool {
        self.own.contains(&unit)
    }
    fn hit(&mut self, unit: Handle) {
        self.log.push(format!("hit {unit}"));
    }
}

#[test]
fn area_victims_walk_columns_then_rows_once_each() {
    let mut f = Field {
        tiles: [((4, 5), vec![101, 102]), ((5, 4), vec![103]), ((5, 5), vec![104, 101]), ((6, 6), vec![105])]
            .into_iter()
            .collect(),
        distance: [(101, 1.0), (102, 4.5), (103, 1.0), (104, 0.0), (105, 2.0)].into_iter().collect(),
        own: vec![105],
        untargetable: vec![103],
        log: Vec::new(),
    };
    // radius 1.5: ceil 2 tiles around (5, 5); 102 out of range, 103 untargetable, 105 own without friendly damage,
    // 101 once (the second node too is 101 in this fake), the target 104 counted as hit already
    let input = EeCombatAreaIn { tile_x: 5, tile_y: 5, radius: 1.5, friendly_factor: 0.0, target: 104 };
    area_victims(&input, &mut f);
    assert_eq!(f.log, ["hit 101"]);
    f.log.clear();
    let friendly = EeCombatAreaIn { friendly_factor: 0.5, target: 0, ..input };
    area_victims(&friendly, &mut f);
    assert_eq!(f.log, ["hit 101", "hit 104", "hit 105"]);
}

/// Unit regions (2 x 2 tiles each): units are handles with an id, a tile and a size.
struct Regions {
    regions: BTreeMap<(i32, i32), Vec<u64>>,
    tiles: BTreeMap<u64, (i32, i32)>,
    big: Vec<u64>,
    screened: Vec<u64>,
    fails: BTreeMap<u64, Vec<u32>>,
    log: Vec<String>,
}

impl TargetHost for Regions {
    fn regions(&mut self, left: i32, top: i32, right: i32, bottom: i32) -> (i32, i32, i32, i32) {
        (left.div_euclid(2), top.div_euclid(2), right.div_euclid(2), bottom.div_euclid(2))
    }
    fn region_len(&mut self, x: i32, y: i32) -> i32 {
        self.regions.get(&(x, y)).map_or(-1, |v| v.len() as i32)
    }
    fn region_unit(&mut self, x: i32, y: i32, index: i32) -> Handle {
        self.regions[&(x, y)][index as usize]
    }
    fn unit_tile(&mut self, unit: Handle) -> (i32, i32) {
        self.tiles[&unit]
    }
    fn screen(&mut self, unit: Handle) -> bool {
        !self.screened.contains(&unit)
    }
    fn multiple(&mut self, unit: Handle) -> bool {
        self.big.contains(&unit)
    }
    fn unit_id(&mut self, unit: Handle) -> u32 {
        (1000 - unit) as u32
    }
    fn test(&mut self, test: u32, unit: Handle) -> bool {
        self.log.push(format!("{test} {unit}"));
        // a test "fails" for the unit when listed: the negative tests (enemy, disabled...) answer true then
        let listed = self.fails.get(&unit).is_some_and(|v| v.contains(&test));
        match test {
            TARGET_TEST_ATTACKED_BY
            | TARGET_TEST_CAN_TARGET_CLASS
            | TARGET_TEST_SEEN
            | TARGET_TEST_CAN_ATTACK_CLASS => !listed,
            _ => listed,
        }
    }
}

fn regions() -> Regions {
    Regions {
        regions: [((0, 0), vec![1, 2]), ((1, 0), vec![3]), ((0, 1), vec![4, 5])].into_iter().collect(),
        tiles: [(1, (0, 0)), (2, (1, 1)), (3, (2, 0)), (4, (0, 2)), (5, (5, 5))].into_iter().collect(),
        big: Vec::new(),
        screened: Vec::new(),
        fails: BTreeMap::new(),
        log: Vec::new(),
    }
}

#[test]
fn units_in_area_walks_regions_from_their_backs() {
    let area = EeCombatFindTargetIn { left: 0, top: 0, right: 3, bottom: 3, skip_kind: 0 };
    let mut r = regions();
    // column 0: region (0,0) back to front, then (0,1) (5 is outside the area); column 1: (1,0)
    assert_eq!(units_in_area(&area, &mut r), [2, 1, 4, 3]);
    r.screened = vec![1];
    assert_eq!(units_in_area(&area, &mut r), [2, 4, 3]);
    // a big unit: sorted by id (1000 - handle) and unique
    r.screened.clear();
    r.big = vec![4];
    r.regions.get_mut(&(1, 0)).unwrap().push(2);
    assert_eq!(units_in_area(&area, &mut r), [4, 3, 2, 1]);
}

#[test]
fn choose_target_takes_the_first_that_passes_in_the_original_order() {
    let mut r = regions();
    r.fails = [(1, vec![TARGET_TEST_NOT_ENEMY]), (2, vec![TARGET_TEST_NATURE, TARGET_TEST_ATTACKED_BY])]
        .into_iter()
        .collect();
    assert_eq!(choose_target(&[1, 2, 3], true, &mut r), Some(2));
    assert_eq!(
        r.log,
        ["2 1", "2 2", "3 2", "4 2", "5 2", "2 3", "3 3", "4 3", "6 3", "7 3", "8 3", "9 3", "10 3", "11 3"]
    );
    // the kind test: asked per candidate unless skipped; its last answer stays for the next candidates when skipped
    let mut r = regions();
    r.fails = [(1, vec![TARGET_TEST_TECH_KIND_6])].into_iter().collect();
    assert_eq!(choose_target(&[1, 3], false, &mut r), Some(1));
    assert_eq!(r.log[..4], ["1 1", "2 1", "3 1", "4 1"]);
    assert_eq!(r.log[4], "1 3");
    let mut r = regions();
    r.fails = [(3, vec![TARGET_TEST_IGNORED]), (4, vec![TARGET_TEST_SEEN])].into_iter().collect();
    assert_eq!(choose_target(&[3, 4], true, &mut r), None);
    assert_eq!(choose_target(&[], true, &mut r), None);
}
