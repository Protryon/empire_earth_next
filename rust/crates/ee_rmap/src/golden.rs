//! Golden maps: small synthetic scenarios run through the C++ generator before it was deleted (`rmap_dump --synthetic`,
//! tools/rmap_gate/rmap_dump.cpp), compared by digest. The full matrix over the real scripts is `tools/rmap_gate`.

use crate::generator::{run_step, Step};
use crate::map::{Grid, Map, ResourceKind, ResourceSlot, SpacingRule, Team, URandomState};
use crate::math::Point;
use ee_random::URandom;

/// (scenario, seed, digest) from the C++ generator.
const GOLDEN: [(u32, u32, u64); 12] = [
    (0, 1, 0xa3e2ff5b83bd05b4),
    (0, 2, 0xe74e9e5dc92328bf),
    (0, 3, 0x2137419bdf7a2f68),
    (1, 1, 0x16717282a47b2c00),
    (1, 2, 0x57004058fc287ab5),
    (1, 3, 0xfb629e7c54eafb8a),
    (2, 1, 0x74d833578e282d5a),
    (2, 2, 0xa15ac24ec6e14fd7),
    (2, 3, 0x6067cf4173d096ec),
    (3, 1, 0xefcd7b4c89e27819),
    (3, 2, 0x68d6db318c86d74a),
    (3, 3, 0xac795c2a2f0c14b5),
];

#[derive(Default)]
struct Fnv(u64);

impl Fnv {
    fn new() -> Self {
        Fnv(0xcbf2_9ce4_8422_2325)
    }
    fn u32(&mut self, v: u32) {
        for byte in v.to_le_bytes() {
            self.0 ^= u64::from(byte);
            self.0 = self.0.wrapping_mul(0x0000_0100_0000_01b3);
        }
    }
    fn f32(&mut self, v: f32) {
        self.u32(v.to_bits());
    }
    fn point(&mut self, p: Point) {
        self.f32(p.x);
        self.f32(p.y);
    }
}

fn digest(map: &Map) -> u64 {
    let mut h = Fnv::new();
    let n = map.tiles.size;
    h.u32(n);
    for x in 0..n {
        for y in 0..n {
            h.f32(map.heights.get(x, y));
            h.u32(map.tiles.get(x, y));
            h.u32(u32::from(map.flat_tiles.get(x, y)));
            h.u32(u32::from(map.claimed_tiles.get(x, y)));
        }
    }
    h.u32(map.players.len() as u32);
    for p in &map.players {
        h.point(p.position);
        h.f32(p.radius);
        h.u32(p.index);
        h.u32(p.team as u32);
        h.u32(p.tile_count as u32);
        h.u32(
            u32::from(p.is_neutral)
                | u32::from(p.is_outer) << 1
                | u32::from(p.is_moon) << 2
                | u32::from(p.is_placed) << 3,
        );
        h.u32(p.tiles.len() as u32);
        for t in &p.tiles {
            h.point(*t);
        }
        h.u32(p.clump_centers.len() as u32);
        for c in &p.clump_centers {
            h.point(*c);
        }
    }
    h.u32(map.objects.len() as u32);
    for o in &map.objects {
        h.point(o.location);
        h.u32(o.kind as u32);
        if o.kind == crate::map::OBJECT_TREE {
            h.u32(o.object_type as u32);
        }
    }
    h.u32(map.features.len() as u32);
    for f in &map.features {
        h.f32(f.angle);
        h.point(f.position);
        h.u32(f.unit_type as u32);
        h.u32(f.index as u32);
    }
    h.u32(map.clump_circles.len() as u32);
    for list in &map.clump_circles {
        h.u32(list.len() as u32);
        for c in list {
            h.point(c.center);
            h.f32(c.radius);
        }
    }
    for t in &map.teams {
        for p in &t.positions {
            h.point(*p);
        }
    }
    h.u32(map.random.0.seed);
    h.u32(map.random_integers.0.seed);
    h.0
}

/// The same map as `SyntheticSetup` in `rmap_dump.cpp`.
fn setup(case: u32, seed: u32) -> Map {
    let mut m = Map::default();
    let n = 65u32;
    m.s.seed = seed;
    m.random = URandomState(URandom { seed });
    m.random_integers = URandomState(URandom { seed: seed + 1 });
    m.s.grid_size = 64;
    m.s.maximum_x = 64;
    m.s.maximum_y = 64;
    m.s.tile_count = 65 * 65;
    m.valid_tiles = Grid::new(n, 1);
    for i in 0..n {
        m.valid_tiles.set(0, i, 0);
        m.valid_tiles.set(n - 1, i, 0);
        m.valid_tiles.set(i, 0, 0);
        m.valid_tiles.set(i, n - 1, 0);
    }
    m.claimed_tiles = Grid::new(n, 0);
    m.flat_tiles = Grid::new(n, 0);
    m.tiles = Grid::new(n, 0);
    m.heights = Grid::new(n, 0.5);
    m.s.max_loop_count = 500;
    m.s.max_tree_loop_count = 200;
    m.s.max_iterations_clump_growth = 300;
    m.s.convergent_sample_size = 2.0;
    m.s.convergent_threshold = 0.5;
    m.s.elevation_scale = 0.8;
    m.s.elevation_unit = 1.0 / 30.0;
    m.s.flat_elevation = 11.0 / 30.0;
    m.s.initial_elevation = 0.5;
    m.s.height_map_chaos = 0.4;
    m.s.land_elevation_threshold = 10.0 / 30.0;
    m.s.max_int_elevation = 20;
    m.s.min_int_elevation = -10;
    m.s.max_resource_elevation = 0.9;
    m.s.max_tree_elevation = 0.9;
    m.s.percent_land = 0.4;
    m.s.landfill_percentage = 0.05;
    m.s.player_flat_chaos = 0.5;
    m.s.player_land_chaos = 0.6;
    m.s.player_percent_flat = 0.3;
    m.s.player_flat_clumps = 2;
    m.s.player_land_clumps = 3;
    m.s.player_tree_percentage = 0.1;
    m.s.forest_chaos_level = 0.4;
    m.s.forest_free_radius = 4.0;
    m.s.forests_per_player = 2;
    m.s.max_clumps_per_forest = 2;
    m.s.max_angle_between_team_members = 0.6;
    m.s.water_border = 2;
    m.s.resource_to_edge_distance = 1;
    m.s.resource_to_water_distance = 1;
    m.s.resource_seperation = 3.0;
    m.s.coastal_to_deeper_water_percentage = 0.25;
    m.s.flat_area.left = -2.0;
    m.s.flat_area.bottom = -2.0;
    m.s.flat_area.right = 2.0;
    m.s.flat_area.top = 2.0;
    m.s.half_map_size = 32.0;
    let center = Point::new(32.0, 32.0);
    m.s.ring.center = center;
    m.s.ring.min_radius = 14.0;
    m.s.ring.max_radius = 20.0;
    m.s.neutral_ring = m.s.ring;
    m.s.moon_ring.center = center;
    m.s.moon_ring.min_radius = 24.0;
    m.s.moon_ring.max_radius = 30.0;
    for i in 0..7 {
        m.s.kind_spacing[i] = [8, 100];
        m.s.resource_per_player[i] = if i < 2 || i == 6 { 2 } else { 1 };
        m.s.resource_per_neutral_land[i] = if i == 5 { 2 } else { 1 };
    }
    m.tree_types = vec![3, 7];
    m.resource_kinds = vec![ResourceKind { minimum: 6, maximum: 14, id: 42, unit_type: 5, area: 9 }];
    m.resource_kind_ids = vec![42];
    m.resource_slots = vec![
        ResourceSlot { id: 42, per_player: 1, random_kind: 0 },
        ResourceSlot { id: 42, per_player: 0, random_kind: 1 },
    ];
    m.spacing_rules = vec![SpacingRule { kind: 0, minimum: 8, maximum: 16 }];
    let mut players = 2;
    let mut paired = false;
    match case {
        1 => {
            players = 4;
            paired = true;
            m.s.river_width = 3;
            m.s.team_growth = 1;
        }
        2 => m.s.is_space_map = 1,
        3 => {
            m.s.generate_shallows = 1;
            m.s.max_shallow_connections_per_player = 2;
            m.s.shallows_width = 3;
            m.s.shallows_elevation = 0.4;
        }
        _ => {}
    }
    let mut number = 1;
    while number <= players {
        let mut team = Team { players: vec![number], positions: vec![Point::default()] };
        number += 1;
        if paired && number <= players {
            team.players.push(number);
            team.positions.push(Point::default());
            number += 1;
        }
        m.teams.push(team);
    }
    m
}

/// The step order of `RunSynthetic` in `rmap_dump.cpp`.
fn run(case: u32, seed: u32) -> Map {
    let mut m = setup(case, seed);
    run_step(&mut m, Step::PlayerPositions);
    run_step(&mut m, Step::NeutralPositions);
    run_step(&mut m, Step::PlayerLandWater);
    for x in 0..64 {
        for y in 0..64 {
            if (m.tiles.get(x, y) & 0x7fff) == 0 {
                m.water_points.push(Point::new(x as f32, y as f32));
            }
        }
    }
    run_step(&mut m, Step::FlatMap);
    run_step(&mut m, Step::HeightMap);
    run_step(&mut m, Step::Shallows);
    if case == 3 {
        run_step(&mut m, Step::FilterHeightMap);
    }
    run_step(&mut m, Step::Resources);
    run_step(&mut m, Step::Trees);
    m
}

#[test]
fn the_synthetic_maps_match_the_cpp_generator_bit_for_bit() {
    for (case, seed, expected) in GOLDEN {
        let map = run(case, seed);
        assert_eq!(digest(&map), expected, "scenario {case} seed {seed}");
    }
}

#[test]
fn a_different_seed_gives_a_different_map() {
    assert_ne!(digest(&run(0, 1)), digest(&run(0, 2)));
}
