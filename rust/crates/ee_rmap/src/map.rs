//! The generator's state: the scalar settings (`Settings`, the C ABI's record too), the grids as flat arrays and the
//! record lists. `unsigned long` is `u32` and `long` is `i32` here on every target: the width the Windows build, the
//! recorded gate and the original 32-bit generator have.

use crate::math::Point;
use ee_random::URandom;

/// A square grid, `size` x `size`, indexed `[x][y]` as the C++ `vector<vector<T>>`.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Grid<T> {
    pub size: u32,
    pub data: Vec<T>,
}

impl<T: Copy> Grid<T> {
    pub fn new(size: u32, fill: T) -> Self {
        Grid { size, data: vec![fill; size as usize * size as usize] }
    }

    #[inline]
    pub fn get(&self, x: u32, y: u32) -> T {
        self.data[x as usize * self.size as usize + y as usize]
    }

    #[inline]
    pub fn set(&mut self, x: u32, y: u32, value: T) {
        self.data[x as usize * self.size as usize + y as usize] = value;
    }
}

/// `CMUIsCellSet`.
pub fn is_cell_set(grid: &Grid<u8>, x: u32, y: u32) -> bool {
    x < grid.size && y < grid.size && grid.get(x, y) != 0
}

/// `EERMRing`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Ring {
    pub center: Point,
    pub min_radius: f32,
    pub max_radius: f32,
}

/// `GERectangle`, as the generator reads it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Rect {
    pub left: f32,
    pub bottom: f32,
    pub right: f32,
    pub top: f32,
}

/// `EERMCircle`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Circle {
    pub center: Point,
    pub radius: f32,
}

impl Circle {
    /// `EERMCircle::Extend`.
    pub fn extend(&mut self, point: Point) {
        let distance = point.distance_to_squared(self.center);
        if distance > self.radius * self.radius {
            self.radius = crate::math::sqrt(distance);
        }
    }
}

/// `EERMObject`. `object_type` is meaningful for trees only (the C++ leaves it unset on the other kinds).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Object {
    pub location: Point,
    pub kind: i32,
    pub object_type: i32,
}

/// `EERMFeature`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Feature {
    pub angle: f32,
    pub position: Point,
    pub unit_type: i32,
    pub index: i32,
    pub is_world_unit: u8,
    pub in_ring: u8,
    pub is_herd: u8,
}

/// `EERMResourceKind`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ResourceKind {
    pub minimum: i32,
    pub maximum: i32,
    pub id: i32,
    pub unit_type: i32,
    pub area: u32,
}

/// `EERMResourceSlot`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ResourceSlot {
    pub id: i32,
    pub per_player: u8,
    pub random_kind: u8,
}

/// `EERMSpacingRule`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SpacingRule {
    pub kind: i32,
    pub minimum: i32,
    pub maximum: i32,
}

/// `EERMPlayer`.
#[derive(Clone, Debug, Default)]
pub struct Player {
    pub tiles: Vec<Point>,
    pub clump_centers: Vec<Point>,
    pub position: Point,
    pub radius: f32,
    pub index: u32,
    pub team: i32,
    pub tile_count: i32,
    pub is_neutral: bool,
    pub is_outer: bool,
    pub is_moon: bool,
    pub is_placed: bool,
}

/// `EERMTeam`. `players` holds 1-based player numbers.
#[derive(Clone, Debug, Default)]
pub struct Team {
    pub players: Vec<i32>,
    pub positions: Vec<Point>,
}

pub const OBJECT_STONE: i32 = 0;
pub const OBJECT_GOLD: i32 = 1;
pub const OBJECT_TREE: i32 = 2;
pub const OBJECT_FISH: i32 = 5;
pub const OBJECT_IRON: i32 = 6;
pub const OBJECT_COUNT: u32 = 7;

pub const CHARACTERISTIC_PLANETS: u32 = 4;
pub const CHARACTERISTIC_EARTH_OR_MARS: u32 = 8 | 0x10;

pub const TEAM_INNER_NEUTRAL: i32 = -16;
pub const TEAM_OUTER_NEUTRAL: i32 = -15;
pub const TEAM_MOON: i32 = -14;

/// Every scalar the generator reads or writes (`EERandomMap`'s members), `#[repr(C)]`: the C++ adapter fills it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Settings {
    pub random_seed: u32,
    pub random_integers_seed: u32,
    pub player_order_index: u32,
    pub seed: u32,
    pub maximum_x: u32,
    pub maximum_y: u32,
    pub grid_size: u32,
    pub tile_count: u32,
    pub map_characteristics: u32,
    pub river_width: u32,
    pub num_team_players: u32,
    pub num_moons: u32,
    pub num_inner_neutrals: u32,
    pub num_outer_neutrals: u32,
    pub moon_size: u32,
    pub moon_resources: u32,
    pub num_terrain_bands: u32,
    pub terrain_band_offset: u32,
    pub max_loop_count: u32,
    pub max_tree_loop_count: u32,
    pub max_clumps_per_forest: u32,
    pub forests_per_player: u32,
    pub inner_neutral_flat_clumps: u32,
    pub inner_neutral_land_clumps: u32,
    pub outer_neutral_flat_clumps: u32,
    pub outer_neutral_land_clumps: u32,
    pub player_flat_clumps: u32,
    pub player_land_clumps: u32,
    pub resource_to_edge_distance: u32,
    pub resource_to_water_distance: u32,
    pub max_iterations_clump_growth: i32,
    pub water_border: i32,
    pub max_int_elevation: i32,
    pub min_int_elevation: i32,
    pub shallows_width: i32,
    pub max_shallow_connections_per_player: i32,
    pub outer_push_from_map_center: i32,
    pub inner_push_from_map_center: i32,
    pub minimum_start_position_to_map_edge: i32,
    pub resource_per_player: [i32; 7],
    pub resource_per_neutral_land: [u32; 7],
    pub kind_spacing: [[u32; 2]; 7],
    pub coastal_to_deeper_water_percentage: f32,
    pub convergent_sample_size: f32,
    pub convergent_threshold: f32,
    pub elevation_scale: f32,
    pub elevation_unit: f32,
    pub flat_elevation: f32,
    pub forest_chaos_level: f32,
    pub forest_free_radius: f32,
    pub initial_elevation: f32,
    pub inner_neutral_flat_chaos: f32,
    pub inner_neutral_land_chaos: f32,
    pub inner_neutral_percent_flat: f32,
    pub inner_neutral_percent_land: f32,
    pub inner_neutral_tree_percentage: f32,
    pub land_elevation_threshold: f32,
    pub max_angle_between_team_members: f32,
    pub max_resource_elevation: f32,
    pub max_tree_elevation: f32,
    pub outer_neutral_flat_chaos: f32,
    pub outer_neutral_land_chaos: f32,
    pub outer_neutral_percent_flat: f32,
    pub outer_neutral_percent_land: f32,
    pub outer_neutral_tree_percentage: f32,
    pub percent_land: f32,
    pub player_flat_chaos: f32,
    pub player_land_chaos: f32,
    pub player_percent_flat: f32,
    pub player_tree_percentage: f32,
    pub shallows_elevation: f32,
    pub height_map_chaos: f32,
    pub landfill_percentage: f32,
    pub resource_seperation: f32,
    pub half_map_size: f32,
    pub flat_area: Rect,
    pub ring: Ring,
    pub neutral_ring: Ring,
    pub moon_ring: Ring,
    pub team_growth: u8,
    pub generate_shallows: u8,
    pub is_space_map: u8,
}

/// The generator's whole state for one step.
#[derive(Clone, Debug, Default)]
pub struct Map {
    pub s: Settings,
    pub random: URandomState,
    pub random_integers: URandomState,
    pub flat_tiles: Grid<u8>,
    pub heights: Grid<f32>,
    pub tiles: Grid<u32>,
    pub claimed_tiles: Grid<u8>,
    pub valid_tiles: Grid<u8>,
    pub players: Vec<Player>,
    pub teams: Vec<Team>,
    pub objects: Vec<Object>,
    pub water_points: Vec<Point>,
    pub features: Vec<Feature>,
    pub clump_circles: Vec<Vec<Circle>>,
    pub tree_types: Vec<i32>,
    pub resource_kinds: Vec<ResourceKind>,
    pub resource_kind_ids: Vec<i32>,
    pub resource_slots: Vec<ResourceSlot>,
    pub spacing_rules: Vec<SpacingRule>,
    pub player_order: Vec<i32>,
}

/// A `URandom` with a `Default` (the seed comes from `Settings`).
#[derive(Clone, Copy, Debug)]
pub struct URandomState(pub URandom);

impl Default for URandomState {
    fn default() -> Self {
        URandomState(URandom { seed: 1 })
    }
}

impl URandomState {
    pub fn generate(&mut self) -> f32 {
        self.0.generate()
    }

    pub fn generate_int(&mut self, min: i32, max: i32) -> i32 {
        self.0.generate_int(min, max)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_grid_is_row_of_x_then_y() {
        let mut g = Grid::new(3, 0u8);
        g.set(1, 2, 7);
        assert_eq!(g.data[5], 7);
        assert_eq!(g.get(1, 2), 7);
    }

    #[test]
    fn cell_set_rejects_points_off_the_grid() {
        let mut g = Grid::new(2, 0u8);
        g.set(1, 1, 1);
        assert!(is_cell_set(&g, 1, 1));
        assert!(!is_cell_set(&g, 0, 0));
        assert!(!is_cell_set(&g, 2, 0));
        assert!(!is_cell_set(&g, u32::MAX, 0));
    }

    #[test]
    fn extend_grows_the_radius_to_reach_a_point() {
        let mut c = Circle::default();
        c.extend(Point::new(3.0, 4.0));
        assert_eq!(c.radius, 5.0);
        c.extend(Point::new(1.0, 1.0));
        assert_eq!(c.radius, 5.0);
    }
}
