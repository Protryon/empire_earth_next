//! Clump growers: regions of map cells grown from start points (`EEClumps.cpp`, `EEFlatClumps.cpp`,
//! `EELandClumps.cpp`, `EEIslandClumps.cpp`, `EETreeClumps.cpp`). The C++ class hierarchy is one struct and a `Kind`;
//! the virtual calls are `match`es, in the same order (several of them draw random numbers).

use crate::grid::{is_square_open, is_tile_of_type};
use crate::map::{is_cell_set, Circle, Map, Object, CHARACTERISTIC_EARTH_OR_MARS, CHARACTERISTIC_PLANETS, OBJECT_TREE};
use crate::math::{floor, lrintf, Point, Rotation, Vector, PI, TWO_PI};
use crate::resources::can_place_resource;

#[derive(Clone, Debug)]
pub enum Kind {
    /// `EEFlatClumps`: claims in the flat-tile grid, on its owner's land.
    Flat { flag: bool },
    /// `EELandClumps` (`river_width: None`) and `EEIslandClumps`: claims map tiles, tracks clump circles.
    Land { tile_index: u32, flag: bool, river_width: Option<u32> },
    /// `EETreeClumps`.
    Tree { centers_to_avoid: Vec<Point>, minimum_distance_squared: f32, tree_type: i32 },
}

#[derive(Clone, Debug)]
pub struct Clumps {
    pub kind: Kind,
    /// The owner: the player index for land, the player's `index` field for flat and tree growers.
    pub owner: i32,
    direction: Vector,
    pub centers: Vec<Point>,
    pub placed: Vec<Point>,
    targets: Vec<u32>,
    pub sizes: Vec<u32>,
    frontier: Vec<(i32, i32)>,
    distances: Vec<u32>,
    spread: f32,
    maximum_y: i32,
    maximum_x: i32,
    map_size: u32,
    index: u32,
    target_size: u32,
    pub clump_count: u32,
    grow_count: u32,
}

impl Clumps {
    fn base(
        map: &Map,
        kind: Kind,
        spread: f32,
        clump_count: u32,
        target_size: u32,
        owner: i32,
        distances: Vec<u32>,
    ) -> Self {
        Clumps {
            kind,
            owner,
            direction: Vector::default(),
            centers: Vec::new(),
            placed: Vec::new(),
            targets: Vec::new(),
            sizes: Vec::new(),
            frontier: Vec::new(),
            distances,
            spread,
            maximum_y: map.s.maximum_y as i32,
            maximum_x: map.s.maximum_x as i32,
            map_size: map.s.grid_size,
            index: 0,
            target_size,
            clump_count,
            grow_count: (map.s.max_iterations_clump_growth as u32).wrapping_mul(target_size),
        }
    }

    /// `EEFlatClumps`' constructor.
    pub fn new_flat(
        map: &Map,
        distances: Vec<u32>,
        count: u32,
        spread: f32,
        target: u32,
        owner: i32,
        flag: bool,
    ) -> Self {
        Self::base(map, Kind::Flat { flag }, spread, count, target, owner, distances)
    }

    /// `EELandClumps`' and `EEIslandClumps`' constructor (`CreatePlayerClumps` chooses): the player's circles are
    /// resized here.
    pub fn new_land(
        map: &mut Map,
        distances: Vec<u32>,
        count: u32,
        spread: f32,
        target: u32,
        player: u32,
        flag: bool,
    ) -> Self {
        let p = &map.players[player as usize];
        let tile_index = p.index;
        let river_width = if !p.is_moon && map.s.river_width != 0 { Some(map.s.river_width) } else { None };
        map.clump_circles[player as usize].resize(count as usize, Circle::default());
        let kind = Kind::Land { tile_index, flag, river_width };
        Self::base(map, kind, spread, count, target, player as i32, distances)
    }

    /// `EETreeClumps`' constructor: draws the forest's tree type unless the map picks per tile.
    #[allow(clippy::too_many_arguments)]
    pub fn new_tree(map: &mut Map, spread: f32, count: u32, target: u32, distances: Vec<u32>, owner: i32) -> Self {
        let centers_to_avoid = map.players.iter().filter(|p| !p.is_neutral).map(|p| p.position).collect();
        let radius = map.s.forest_free_radius;
        let tree_type = if (map.s.map_characteristics & CHARACTERISTIC_PLANETS) == CHARACTERISTIC_PLANETS {
            -1
        } else {
            let last = map.tree_types.len() as i32 - 1;
            map.tree_types[map.random_integers.generate_int(0, last) as usize]
        };
        let kind = Kind::Tree { centers_to_avoid, minimum_distance_squared: radius * radius, tree_type };
        Self::base(map, kind, spread, count, target, owner, distances)
    }

    /// `EEClumps::Begin`.
    pub fn begin(&mut self, map: &mut Map, start: Point) {
        self.reset();
        for i in 0..self.clump_count as usize {
            self.targets[i] = self.target_size;
        }
        self.centers[0] = start;
        self.pick_start(map);
    }

    fn reset(&mut self) {
        let n = self.clump_count as usize;
        self.centers = vec![Point::default(); n];
        self.sizes = vec![0; n];
        self.targets = vec![0; n];
    }

    /// `GetTotalSize`.
    pub fn total_size(&self) -> u32 {
        self.sizes[..self.clump_count as usize].iter().fold(0u32, |a, b| a.wrapping_add(*b))
    }

    // ---- the virtual cell tests

    fn is_in_map(&self, map: &Map, x: i32, y: i32) -> bool {
        (x as u32) < self.map_size && (y as u32) < self.map_size && map.valid_tiles.get(x as u32, y as u32) != 0
    }

    fn base_is_claimed(&self, map: &Map, x: i32, y: i32) -> bool {
        self.is_in_map(map, x, y) && self.grid(map).get(x as u32, y as u32) != 0
    }

    fn base_is_free(&self, map: &Map, x: i32, y: i32) -> bool {
        self.is_in_map(map, x, y) && self.grid(map).get(x as u32, y as u32) == 0
    }

    /// The claim grid: the flat grower's own, every other grower's the map's claimed tiles.
    fn grid<'m>(&self, map: &'m Map) -> &'m crate::map::Grid<u8> {
        match self.kind {
            Kind::Flat { .. } => &map.flat_tiles,
            _ => &map.claimed_tiles,
        }
    }

    fn is_claimed(&self, map: &Map, x: i32, y: i32) -> bool {
        match self.kind {
            Kind::Land { .. } => self.is_in_map(map, x, y) && (map.tiles.get(x as u32, y as u32) & 0x7fff) != 0,
            _ => self.base_is_claimed(map, x, y),
        }
    }

    fn is_free(&self, map: &Map, x: i32, y: i32) -> bool {
        match self.kind {
            Kind::Land { .. } => self.is_in_map(map, x, y) && (map.tiles.get(x as u32, y as u32) & 0x7fff) == 0,
            _ => self.base_is_free(map, x, y),
        }
    }

    fn has_free_neighbor(&self, map: &Map, x: i32, y: i32) -> bool {
        self.is_free(map, x, y.wrapping_add(1))
            || self.is_free(map, x, y.wrapping_sub(1))
            || self.is_free(map, x.wrapping_add(1), y)
            || self.is_free(map, x.wrapping_sub(1), y)
    }

    /// `IsValidLocation` (slot 2): the land and island tests draw random numbers.
    fn is_valid_location(&self, map: &mut Map, x: i32, y: i32) -> bool {
        match &self.kind {
            Kind::Flat { flag } => {
                self.is_free(map, x, y) && is_tile_of_type(&map.tiles, x as u32, y as u32, self.owner as u32, *flag)
            }
            Kind::Land { river_width, .. } => {
                let river_width = *river_width;
                if !self.land_is_valid(map, x, y) {
                    return false;
                }
                match river_width {
                    Some(width) => !self.is_near_other(map, x as u32, y as u32, width),
                    None => true,
                }
            }
            Kind::Tree { centers_to_avoid, minimum_distance_squared, .. } => {
                if !is_cell_set(&map.valid_tiles, x as u32, y as u32) {
                    return false;
                }
                let point = Point::new(x as f32, y as f32);
                let mut far = true;
                let mut i = 0;
                while i < centers_to_avoid.len() && far {
                    far = point.distance_to_squared(centers_to_avoid[i]) >= *minimum_distance_squared;
                    i += 1;
                }
                far && can_place_resource(map, OBJECT_TREE, x as u32, y as u32)
            }
        }
    }

    /// `EELandClumps::IsValidLocation`.
    fn land_is_valid(&self, map: &mut Map, x: i32, y: i32) -> bool {
        if !self.is_free(map, x, y) {
            return false;
        }
        if map.players[self.owner as usize].is_moon {
            let point = Point::new(x as f32, y as f32);
            if self.centers[self.index as usize].distance_to(point) > map.s.moon_size as f32 * 0.8 {
                return false;
            }
        } else if map.s.water_border > 0 {
            if map.s.is_space_map != 0 && (map.s.map_characteristics & CHARACTERISTIC_EARTH_OR_MARS) != 0 {
                let point = Point::new(x as f32, y as f32);
                if map.s.ring.center.distance_to(point) > (map.s.half_map_size - map.s.water_border as f32) * 0.75 {
                    return false;
                }
            } else {
                let radius = lrintf((map.random.generate() * 0.25 + 0.75) * map.s.water_border as f32);
                if is_square_open(&map.valid_tiles, x as u32, y as u32, radius as u32) {
                    return false;
                }
            }
        }
        true
    }

    /// `EEIslandClumps::IsNearOther`.
    fn is_near_other(&self, map: &mut Map, x: u32, y: u32, river_width: u32) -> bool {
        let point = Point::new(x as f32, y as f32);
        let mut minimum = 1u32;
        if river_width > 1 {
            minimum = river_width.wrapping_mul(3) / 4;
        }
        let mut found = false;
        let distance = map.random_integers.generate_int(minimum as i32, river_width as i32) as f32;
        let player_count = map.players.len();
        let owner = self.owner as usize;
        if map.s.team_growth == 0 {
            for i in 0..player_count {
                if i != owner {
                    let mut j = 0;
                    while j < map.clump_circles[i].len() && !found {
                        let circle = map.clump_circles[i][j];
                        let range = distance + circle.radius;
                        found = point.distance_to_squared(circle.center) <= range * range;
                        j += 1;
                    }
                }
            }
        } else {
            for i in 0..player_count {
                if map.players[i].team != map.players[owner].team {
                    let mut j = 0;
                    while j < map.clump_circles[i].len() && !found {
                        let circle = map.clump_circles[i][j];
                        let range = distance + circle.radius;
                        found = point.distance_to_squared(circle.center) <= range * range;
                        j += 1;
                    }
                }
            }
        }
        found
    }

    fn is_frontier_location(&self, map: &mut Map, x: i32, y: i32) -> bool {
        self.is_valid_location(map, x, y) && self.has_free_neighbor(map, x, y)
    }

    // ---- claiming

    /// The base `Claim`: marks, records the tile and its size, updates the frontier.
    fn base_claim(&mut self, map: &mut Map, x: i32, y: i32, mark: bool) {
        if mark {
            match self.kind {
                Kind::Flat { .. } => map.flat_tiles.set(x as u32, y as u32, 1),
                _ => map.claimed_tiles.set(x as u32, y as u32, 1),
            }
        }
        self.placed.push(Point::new(x as f32, y as f32));
        let i = self.index as usize;
        self.sizes[i] = self.sizes[i].wrapping_add(1);
        if self.has_free_neighbor(map, x, y) {
            self.frontier.push((x, y));
        }
        self.update_frontier(map, x, y);
    }

    /// `Claim` (slot 8).
    fn claim(&mut self, map: &mut Map, x: i32, y: i32, mark: bool) {
        match &self.kind {
            Kind::Flat { .. } => self.base_claim(map, x, y, mark),
            Kind::Land { tile_index, flag, .. } => {
                let (tile_index, flag) = (*tile_index, *flag);
                let point = Point::new(x as f32, y as f32);
                self.base_claim(map, x, y, false);
                if flag {
                    let mut value = tile_index.wrapping_add(0x400);
                    if map.players[self.owner as usize].is_moon {
                        value |= 0x8000;
                    }
                    map.tiles.set(x as u32, y as u32, value);
                } else {
                    map.tiles.set(x as u32, y as u32, tile_index.wrapping_add(1));
                }
                map.clump_circles[self.owner as usize][self.index as usize].extend(point);
            }
            Kind::Tree { tree_type, .. } => {
                let mut object_type = *tree_type;
                if object_type == -1 {
                    let band = map.tiles.get(x as u32, y as u32).wrapping_add(map.s.terrain_band_offset)
                        % map.s.num_terrain_bands;
                    object_type = map.tree_types[band as usize];
                }
                map.objects.push(Object { location: Point::new(x as f32, y as f32), kind: OBJECT_TREE, object_type });
                self.base_claim(map, x, y, mark);
            }
        }
    }

    fn claim_point(&mut self, map: &mut Map, location: Point, mark: bool) {
        self.claim(map, floor(location.x), floor(location.y), mark);
    }

    fn remove_frontier(&mut self, x: i32, y: i32) {
        if let Some(i) = self.frontier.iter().position(|&p| p == (x, y)) {
            self.frontier.remove(i);
        }
    }

    fn update_frontier(&mut self, map: &Map, x: i32, y: i32) {
        for (nx, ny) in [(x, y.wrapping_add(1)), (x, y.wrapping_sub(1)), (x.wrapping_add(1), y), (x.wrapping_sub(1), y)]
        {
            if self.is_claimed(map, nx, ny) && !self.has_free_neighbor(map, nx, ny) {
                self.remove_frontier(nx, ny);
            }
        }
    }

    /// The base `AddLocation`.
    fn base_add_location(&mut self, map: &mut Map, location: Point) -> bool {
        if self.is_valid_location(map, floor(location.x), floor(location.y)) {
            self.frontier.clear();
            self.centers[self.index as usize] = location;
            self.claim_point(map, location, true);
            return true;
        }
        false
    }

    /// `AddLocation` (slot 4): a player's first flat or land clump also claims the flat area around it, and a land
    /// clump resets its circle, even when the start failed.
    fn add_location(&mut self, map: &mut Map, location: Point) -> bool {
        let result = self.base_add_location(map, location);
        let (is_land, claims_area) = match self.kind {
            Kind::Flat { flag } => (false, !flag),
            Kind::Land { flag, .. } => (true, !flag),
            Kind::Tree { .. } => return result,
        };
        if result && self.index == 0 && claims_area {
            let area = map.s.flat_area;
            let left = floor(area.left + location.x);
            let bottom = floor(area.bottom + location.y);
            let right = floor(area.right + location.x).wrapping_add(1);
            let top = floor(area.top + location.y).wrapping_add(1);
            let mut x = left;
            while x <= right {
                let mut y = bottom;
                while y <= top {
                    if is_cell_set(&map.valid_tiles, x as u32, y as u32) && self.is_free(map, x, y) {
                        self.claim(map, x, y, true);
                    }
                    y = y.wrapping_add(1);
                }
                x = x.wrapping_add(1);
            }
        }
        if is_land {
            let circle = &mut map.clump_circles[self.owner as usize][self.index as usize];
            circle.center = location;
            circle.radius = 0.0;
        }
        result
    }

    /// `AddClump` (slot 9): a new clump takes the size the current one still lacks.
    fn add_clump(&mut self, map: &mut Map) {
        let i = self.index as usize;
        let remaining = self.targets[i].wrapping_sub(self.sizes[i]);
        self.clump_count = self.clump_count.wrapping_add(1);
        self.centers.push(Point::default());
        self.sizes.push(0);
        self.targets.push(remaining);
        self.distances.push(self.distances[i]);
        self.targets[i] = self.sizes[i];
        if matches!(self.kind, Kind::Land { .. }) {
            map.clump_circles[self.owner as usize].push(Circle::default());
        }
    }

    /// `Grow`: claims one valid tile next to a random frontier tile (0), drops a frontier tile with no valid
    /// neighbour (1), or reports an empty frontier (2).
    fn grow(&mut self, map: &mut Map) -> i32 {
        if self.frontier.is_empty() {
            return 2;
        }
        let index = map.random_integers.generate_int(0, self.frontier.len() as i32 - 1) as usize;
        let mut direction = map.random_integers.generate_int(0, 3);
        let mut tries = 0;
        let mut found = None;
        loop {
            let (fx, fy) = self.frontier[index];
            let candidate = match direction {
                0 => (fx, fy.wrapping_add(1)),
                1 => (fx.wrapping_add(1), fy),
                2 => (fx, fy.wrapping_sub(1)),
                _ => (fx.wrapping_sub(1), fy),
            };
            if self.is_valid_location(map, candidate.0, candidate.1) {
                found = Some(candidate);
            }
            tries += 1;
            if found.is_none() {
                direction = (direction + 1) & 3;
            }
            if found.is_some() || tries == 4 {
                break;
            }
        }
        match found {
            Some((x, y)) => {
                self.claim(map, x, y, true);
                0
            }
            None => {
                self.frontier.remove(index);
                1
            }
        }
    }

    /// `PickStart`: a random direction in which a tile one clump spacing away is valid with a free neighbour.
    fn pick_start(&mut self, map: &mut Map) -> bool {
        self.direction = Vector::default();
        let mut found = false;
        let mut i = 0;
        while !found && i < map.s.max_loop_count {
            let distance = self.distances[self.index as usize] as f32 + 1.0;
            let unit = Vector { x: 1.0, y: 0.0, z: 0.0 }.scale(distance);
            let rotation = Rotation::yaw(map.random.generate() * TWO_PI - PI);
            self.direction = rotation.apply(unit);
            let center = self.centers[self.index as usize];
            // truncated to a whole tile as a float first (GE2DPoint(UMFloor..)), then floored again by the callers
            let lx = floor(center.x + self.direction.x) as f32;
            let ly = floor(center.y + self.direction.y) as f32;
            found =
                self.is_valid_location(map, floor(lx), floor(ly)) && self.has_free_neighbor(map, floor(lx), floor(ly));
            i += 1;
        }
        if found {
            self.direction.normalize();
        }
        found
    }

    /// `PickDirection`: turns the growth direction by a random angle scaled by the spread and walks from the current
    /// centre until a frontier tile.
    fn pick_direction(&mut self, map: &mut Map) -> bool {
        let i = self.index as usize;
        self.direction = self.direction.scale(self.distances[i] as f32);
        let mut found = false;
        let mut last_direction = Vector::default();
        let mut loops = 0;
        while loops < map.s.max_loop_count && !found {
            let mut direction = self.direction;
            let rotation = Rotation::yaw((map.random.generate() * TWO_PI - PI) * self.spread);
            let mut step = rotation.apply(direction);
            let mut x = lrintf(self.centers[i].x + step.x);
            let mut y = lrintf(step.y + self.centers[i].y);
            direction = step;
            step.normalize();
            step = step.scale(1.99999);
            let mut step_x = lrintf(step.x);
            let mut step_y = lrintf(step.y);
            if (if step.x < 0.0 { -step.x } else { step.x }) > (if step.y < 0.0 { -step.y } else { step.y }) {
                step_y = 0;
            } else {
                step_x = 0;
            }
            let mut j = 0;
            while j < map.s.max_loop_count && !found {
                if (x < 0 && step_x < 0)
                    || (y < 0 && step_y < 0)
                    || (x > self.maximum_x && step_x > 0)
                    || (y > self.maximum_y && step_y > 0)
                {
                    break;
                }
                if self.is_frontier_location(map, x, y) {
                    self.centers[i + 1].x = x as f32;
                    found = true;
                    self.centers[i + 1].y = y as f32;
                } else {
                    x = x.wrapping_add(step_x);
                    y = y.wrapping_add(step_y);
                }
                j += 1;
            }
            last_direction = direction;
            loops += 1;
        }
        if found {
            self.direction = last_direction;
            self.direction.normalize();
        }
        found
    }

    /// `Generate`: grows all clumps in one go.
    pub fn generate(&mut self, map: &mut Map) {
        self.index = 0;
        while self.index < self.clump_count {
            let i = self.index as usize;
            let center = self.centers[i];
            if !self.add_location(map, center) {
                return;
            }
            let mut growing = true;
            let mut n = 0u32;
            while n < self.grow_count && growing {
                self.grow(map);
                growing = self.sizes[i] < self.targets[i];
                if self.frontier.is_empty() && growing {
                    self.add_clump(map);
                    growing = false;
                }
                n += 1;
            }
            if self.index + 1 < self.clump_count {
                let mut failed = true;
                let mut j = 0;
                while j < map.s.max_loop_count && failed {
                    failed = !self.pick_direction(map);
                    if failed {
                        self.pick_start(map);
                    }
                    j += 1;
                }
                if failed {
                    return;
                }
            }
            self.index += 1;
        }
    }

    /// `Step`: advances the current clump by one step; false once all clumps are done.
    pub fn step(&mut self, map: &mut Map) -> bool {
        let mut result = true;
        if self.index == self.clump_count {
            result = false;
        } else if self.sizes[self.index as usize] == 0 {
            let center = self.centers[self.index as usize];
            if !self.add_location(map, center) {
                self.index = self.clump_count;
            }
        } else if self.sizes[self.index as usize] == self.targets[self.index as usize] {
            if self.index + 1 < self.clump_count {
                let mut failed = true;
                let mut j = 0;
                while j < map.s.max_loop_count && failed {
                    failed = !self.pick_direction(map);
                    if failed {
                        self.pick_start(map);
                    }
                    j += 1;
                }
                if failed {
                    self.index = self.clump_count;
                } else {
                    self.index += 1;
                    self.sizes[self.index as usize] = 0;
                }
            } else {
                self.index = self.clump_count;
            }
        } else if self.frontier.is_empty() {
            self.add_clump(map);
        } else {
            let mut status = 1;
            let mut k = 0;
            while k < map.s.max_iterations_clump_growth && status == 1 {
                status = self.grow(map);
                k += 1;
            }
            if status != 0 {
                self.index = self.clump_count;
            }
        }
        result
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::map::{Grid, Player, URandomState};
    use ee_random::URandom;

    fn test_map() -> Map {
        let mut map = Map::default();
        map.s.grid_size = 16;
        map.s.maximum_x = 16;
        map.s.maximum_y = 16;
        map.s.max_loop_count = 200;
        map.s.max_iterations_clump_growth = 1000;
        map.valid_tiles = Grid::new(17, 1);
        map.claimed_tiles = Grid::new(17, 0);
        map.flat_tiles = Grid::new(17, 0);
        map.tiles = Grid::new(17, 0);
        map.heights = Grid::new(17, 0.0);
        map.random = URandomState(URandom::new(7));
        map.random_integers = URandomState(URandom::new(9));
        map.players = vec![Player { index: 1, position: Point::new(8.0, 8.0), ..Player::default() }];
        map.clump_circles = vec![Vec::new()];
        map
    }

    #[test]
    fn land_grows_to_its_target_and_writes_owner_plus_one() {
        let mut map = test_map();
        let mut clumps = Clumps::new_land(&mut map, vec![2], 1, 0.5, 10, 0, false);
        clumps.begin(&mut map, Point::new(8.0, 8.0));
        while clumps.step(&mut map) {}
        assert_eq!(clumps.total_size(), 10);
        assert_eq!(map.tiles.data.iter().filter(|&&t| t == 2).count(), 10);
        assert_eq!(map.tiles.get(8, 8), 2);
        assert!(map.clump_circles[0][0].radius >= 1.0);
    }

    #[test]
    fn a_neutral_moon_tile_gets_the_flag_bits() {
        let mut map = test_map();
        map.s.moon_size = 5;
        map.players[0].is_moon = true;
        map.players[0].is_neutral = true;
        let mut clumps = Clumps::new_land(&mut map, vec![1], 1, 0.5, 3, 0, true);
        clumps.begin(&mut map, Point::new(8.0, 8.0));
        while clumps.step(&mut map) {}
        assert_eq!(map.tiles.get(8, 8), 0x401 | 0x8000);
    }

    #[test]
    fn flat_only_grows_on_its_owners_land() {
        let mut map = test_map();
        // a neutral (flag): owner index 1 is tile value 0x401, and no flat-area claim runs
        map.tiles.set(8, 8, 0x401);
        map.tiles.set(8, 9, 0x401);
        map.tiles.set(9, 8, 0x409); // someone else's
        let mut clumps = Clumps::new_flat(&map, vec![1], 1, 0.5, 4, 1, true);
        clumps.begin(&mut map, Point::new(8.0, 8.0));
        clumps.generate(&mut map);
        assert_eq!(map.flat_tiles.get(8, 8), 1);
        assert_eq!(map.flat_tiles.get(8, 9), 1);
        assert_eq!(map.flat_tiles.get(9, 8), 0);
        assert_eq!(clumps.total_size(), 2);
    }

    #[test]
    fn a_dead_end_adds_a_clump_with_the_remaining_size() {
        let mut map = test_map();
        map.tiles.set(8, 8, 0x401);
        let mut clumps = Clumps::new_flat(&map, vec![1], 1, 0.5, 4, 1, true);
        clumps.begin(&mut map, Point::new(8.0, 8.0));
        clumps.generate(&mut map);
        // the frontier is empty after one tile: a second clump of 3 is added, and fails to start
        assert_eq!(clumps.clump_count, 2);
        assert_eq!(clumps.targets, vec![1, 3]);
    }

    #[test]
    fn trees_plant_objects_of_the_forest_type() {
        let mut map = test_map();
        map.tree_types = vec![11];
        map.s.land_elevation_threshold = 0.0;
        map.s.max_tree_elevation = 1.0;
        map.s.resource_to_edge_distance = 1;
        map.s.resource_to_water_distance = 0;
        map.tiles = Grid::new(17, 1);
        map.players[0].is_neutral = true; // no start position to keep clear
        map.players[0].position = Point::new(1.0, 1.0);
        let mut clumps = Clumps::new_tree(&mut map, 0.5, 1, 4, vec![1], 0);
        clumps.begin(&mut map, Point::new(8.0, 8.0));
        clumps.generate(&mut map);
        assert_eq!(map.objects.len(), 4);
        assert!(map.objects.iter().all(|o| o.kind == OBJECT_TREE && o.object_type == 11));
        assert_eq!(map.claimed_tiles.get(8, 8), 1);
    }

    #[test]
    fn step_reports_done_once_past_the_last_clump() {
        let mut map = test_map();
        let mut clumps = Clumps::new_land(&mut map, vec![1], 1, 0.5, 1, 0, true);
        clumps.begin(&mut map, Point::new(8.0, 8.0));
        let mut steps = 0;
        while clumps.step(&mut map) {
            steps += 1;
            assert!(steps < 1000);
        }
        assert_eq!(clumps.total_size(), 1);
        assert!(!clumps.step(&mut map));
    }
}
