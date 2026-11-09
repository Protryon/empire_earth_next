//! The steps of the generator (`CMU Map Generator.cpp`): start positions, land and water, flat areas, forests,
//! resources and shallows.

use crate::clumps::Clumps;
use crate::fractal;
use crate::grid::is_square_open;
use crate::map::{
    is_cell_set, Circle, Map, Player, Ring, Team, CHARACTERISTIC_EARTH_OR_MARS, OBJECT_COUNT, OBJECT_TREE,
    TEAM_INNER_NEUTRAL, TEAM_MOON, TEAM_OUTER_NEUTRAL,
};
use crate::math::{lrintf, sqrt, sqrt_u32, um_arctan2, Point, Rotation, Vector, HALF_PI, TWO_PI};
use crate::points::{find_closest_point, place_point_on_ring, place_points_on_ring, relax_points, spread_points};
use crate::resources::{
    can_place_resource, place_objects, place_player_objects, place_resources, place_spaced_objects,
};

const NEUTRAL_SCALE: f32 = 1.0;

/// The tile shares of `CMUCreateFlatMap` and `CMUCreateLandWaterMap`.
fn player_divisor(map: &Map) -> u32 {
    (map.players.len() as u32)
        .wrapping_sub(map.s.num_moons)
        .wrapping_sub(map.s.num_outer_neutrals)
        .wrapping_sub(map.s.num_inner_neutrals)
}

/// `CMUCreateFlatMap`: the flat area of every player, neutral and moon.
pub fn create_flat_map(map: &mut Map) {
    let water_share = map.s.outer_neutral_percent_land;
    let land_share = map.s.inner_neutral_percent_land;
    let rest = 1.0f32 - map.s.outer_neutral_percent_land - map.s.inner_neutral_percent_land;
    let tiles = lrintf(map.s.tile_count as f32 * map.s.percent_land) as u32;
    let tile_count = tiles as f32;
    let player_tiles = lrintf(tile_count * rest * map.s.player_percent_flat / player_divisor(map) as i32 as f32) as u32;
    let land_tiles =
        lrintf(tile_count * land_share * map.s.inner_neutral_percent_flat / map.s.num_inner_neutrals as f32) as u32;
    let water_tiles =
        lrintf(tile_count * water_share * map.s.outer_neutral_percent_flat / map.s.num_outer_neutrals as f32) as u32;
    let neutral_tiles = map.s.moon_size.wrapping_mul(map.s.moon_size);
    for i in 0..map.players.len() {
        let p = &map.players[i];
        let (count, spread, total) = if p.is_neutral {
            if p.is_moon {
                (1, 0.8f32, neutral_tiles)
            } else if p.is_outer {
                (map.s.outer_neutral_flat_clumps, map.s.outer_neutral_flat_chaos, water_tiles)
            } else {
                (map.s.inner_neutral_flat_clumps, map.s.inner_neutral_flat_chaos, land_tiles)
            }
        } else {
            (map.s.player_flat_clumps, map.s.player_flat_chaos, player_tiles)
        };
        let target = total / count;
        let distance = lrintf(sqrt_u32(target)) as u32;
        let distances = vec![distance; count as usize];
        let (owner, flag, position) = (p.index as i32, p.is_neutral, p.position);
        let mut clumps = Clumps::new_flat(map, distances, count, spread, target, owner, flag);
        clumps.begin(map, position);
        clumps.generate(map);
    }
}

/// `CMUCreateLandWaterMap`, with `CreatePlayerClumps` choosing the land or island grower: every owner's land mass,
/// one step of each owner in turn until none grows.
pub fn create_land_water_map(map: &mut Map) {
    let player_count = map.players.len();
    if map.clump_circles.is_empty() {
        map.clump_circles.resize(player_count, Vec::<Circle>::new());
    }
    let water_share = map.s.outer_neutral_percent_land;
    let land_share = map.s.inner_neutral_percent_land;
    let rest = 1.0f32 - map.s.outer_neutral_percent_land - map.s.inner_neutral_percent_land;
    let tiles = lrintf((map.s.landfill_percentage + map.s.percent_land) * map.s.tile_count as f32) as u32;
    let tile_count = tiles as f32;
    let player_tiles = lrintf(tile_count * rest / player_divisor(map) as f32) as u32;
    let land_tiles = lrintf(tile_count * land_share / map.s.num_inner_neutrals as f32) as u32;
    let water_tiles = lrintf(tile_count * water_share / map.s.num_outer_neutrals as f32) as u32;
    let neutral_tiles = map.s.moon_size.wrapping_mul(map.s.moon_size);
    let mut clumps = Vec::with_capacity(player_count);
    for i in 0..player_count {
        let p = &map.players[i];
        let (is_neutral, position) = (p.is_neutral, p.position);
        let (total, count, spread) = if p.is_neutral {
            if p.is_moon {
                (neutral_tiles, 1, 0.8f32)
            } else if p.is_outer {
                (water_tiles, map.s.outer_neutral_land_clumps, map.s.outer_neutral_land_chaos)
            } else {
                (land_tiles, map.s.inner_neutral_land_clumps, map.s.inner_neutral_land_chaos)
            }
        } else {
            (player_tiles, map.s.player_land_clumps, map.s.player_land_chaos)
        };
        let target = total / count;
        let distance = lrintf(sqrt_u32(target)) as u32;
        let distances = vec![distance; count as usize];
        // CreatePlayerClumps: the grower's target is the total over the clump count again
        let mut clump = Clumps::new_land(map, distances, count, spread, total / count, i as u32, is_neutral);
        clump.begin(map, position);
        clumps.push(clump);
    }
    let rounds = map.s.max_iterations_clump_growth.wrapping_mul(map.s.max_iterations_clump_growth) as u32;
    let mut k = 0u32;
    while k < rounds {
        let mut growing = false;
        for clump in clumps.iter_mut() {
            growing = clump.step(map) || growing;
        }
        if !growing {
            break;
        }
        k += 1;
    }
    for (i, clump) in clumps.iter().enumerate() {
        map.players[i].clump_centers.clear();
        for j in 0..clump.clump_count as usize {
            if clump.sizes[j] > 0 {
                map.players[i].clump_centers.push(clump.centers[j]);
            }
        }
    }
    for (i, clump) in clumps.iter().enumerate() {
        let player = &mut map.players[i];
        player.tile_count = clump.total_size() as i32;
        player.tiles = clump.placed.clone();
        if player.is_neutral {
            continue;
        }
        for tile in &player.tiles {
            let distance = player.position.distance_to_squared(*tile);
            if distance > player.radius {
                player.radius = distance;
            }
        }
        player.radius = sqrt(player.radius);
    }
}

/// `CMUCreateTrees`: each owner's forests, a centre on a ring around one of its clump centres, then a forest grower.
pub fn create_trees(map: &mut Map) {
    if map.tree_types.is_empty() || map.s.max_clumps_per_forest.wrapping_mul(map.s.forests_per_player) == 0 {
        return;
    }
    let water_share = map.s.outer_neutral_percent_land;
    let land_share = map.s.inner_neutral_percent_land;
    let rest = 1.0f32 - map.s.outer_neutral_percent_land - map.s.inner_neutral_percent_land;
    let tiles = lrintf(map.s.tile_count as f32 * map.s.percent_land) as u32;
    let tile_count = tiles as f32;
    let player_trees = lrintf(rest * tile_count * map.s.player_tree_percentage / player_divisor(map) as f32) as u32;
    let land_trees =
        lrintf(land_share * tile_count * map.s.inner_neutral_tree_percentage / map.s.num_inner_neutrals as f32) as u32;
    let water_trees =
        lrintf(water_share * tile_count * map.s.outer_neutral_tree_percentage / map.s.num_outer_neutrals as f32) as u32;
    let spread = map.s.forest_chaos_level;
    for i in 0..map.players.len() {
        let clump_count = map.s.max_clumps_per_forest;
        let per_clump = map.s.forests_per_player;
        let total = if map.players[i].is_neutral {
            if map.players[i].is_moon {
                continue;
            }
            if map.players[i].is_outer {
                water_trees
            } else {
                land_trees
            }
        } else {
            player_trees
        };
        let mut size = total / per_clump.wrapping_mul(clump_count);
        if size == 0 {
            size = 1;
        }
        let distance = lrintf(sqrt_u32(size)) as u32;
        let points = map.players[i].clump_centers.len() as u32;
        if points == 0 {
            continue;
        }
        let step = if points > per_clump {
            points / per_clump + 1
        } else if points < per_clump {
            per_clump / points + 1
        } else {
            1
        };
        let mut index = 0u32;
        for _ in 0..per_clump {
            let distances = vec![distance; clump_count as usize];
            let owner = map.players[i].index as i32;
            let mut clumps = Clumps::new_tree(map, spread, clump_count, size, distances, owner);
            let ring = Ring {
                min_radius: map.s.forest_free_radius,
                max_radius: map.s.forest_free_radius + map.s.forest_free_radius,
                center: map.players[i].clump_centers[index as usize],
            };
            let next = step.wrapping_add(index) % points;
            let mut center = None;
            let mut angle = map.random.generate() * TWO_PI;
            let mut n = 0;
            while n < map.s.max_tree_loop_count && center.is_none() {
                let point = place_point_on_ring(&mut map.random, &ring, &mut angle);
                let x = lrintf(point.x) as u32;
                let y = lrintf(point.y) as u32;
                if is_cell_set(&map.valid_tiles, x, y) && can_place_resource(map, OBJECT_TREE, x, y) {
                    center = Some(Point::new(x as f32, y as f32));
                }
                n += 1;
            }
            if let Some(center) = center {
                clumps.begin(map, center);
                clumps.generate(map);
            }
            index = next;
        }
    }
}

/// `CMUGenerateNeutralPositions`: the outer neutrals, inner neutrals and moons, at spread-out points on their rings.
pub fn generate_neutral_positions(map: &mut Map) {
    let mut occupied: Vec<Point> = map.players.iter().map(|p| p.position).collect();
    let outer = map.s.num_outer_neutrals;
    if outer != 0 {
        let ring = map.s.ring;
        let mut positions = vec![Point::default(); outer as usize];
        place_points_on_ring(&mut map.random, &ring, &mut positions);
        if positions.len() > 1 {
            let spacing = spread_points(
                &mut positions,
                &occupied,
                &ring,
                NEUTRAL_SCALE,
                map.s.convergent_sample_size,
                map.s.convergent_threshold,
                map.s.max_loop_count,
            );
            relax_points(&mut positions, &occupied, &ring, NEUTRAL_SCALE, spacing, map.s.max_loop_count);
        }
        for (i, position) in positions.iter().enumerate() {
            map.players.push(Player {
                index: i as u32,
                is_neutral: true,
                is_outer: true,
                team: TEAM_OUTER_NEUTRAL,
                position: *position,
                ..Player::default()
            });
            occupied.push(*position);
        }
    }
    let inner = map.s.num_inner_neutrals;
    if inner != 0 {
        let ring = map.s.neutral_ring;
        let mut positions = vec![Point::default(); inner as usize];
        place_points_on_ring(&mut map.random, &ring, &mut positions);
        if positions.len() > 1 {
            let spacing = spread_points(
                &mut positions,
                &[],
                &ring,
                NEUTRAL_SCALE,
                map.s.convergent_sample_size,
                map.s.convergent_threshold,
                map.s.max_loop_count,
            );
            relax_points(&mut positions, &[], &ring, NEUTRAL_SCALE, spacing, map.s.max_loop_count);
        }
        for (i, position) in positions.iter().enumerate() {
            map.players.push(Player {
                index: (i as u32).wrapping_add(outer),
                is_neutral: true,
                team: TEAM_INNER_NEUTRAL,
                position: *position,
                ..Player::default()
            });
        }
    }
    let moons = map.s.num_moons;
    if moons != 0 {
        let ring = map.s.moon_ring;
        let mut positions = vec![Point::default(); moons as usize];
        place_points_on_ring(&mut map.random, &ring, &mut positions);
        if positions.len() > 1 {
            let spacing = spread_points(
                &mut positions,
                &[],
                &ring,
                NEUTRAL_SCALE,
                map.s.convergent_sample_size,
                map.s.convergent_threshold,
                map.s.max_loop_count,
            );
            relax_points(&mut positions, &[], &ring, NEUTRAL_SCALE, spacing, map.s.max_loop_count);
        }
        for (i, position) in positions.iter().enumerate() {
            map.players.push(Player {
                index: inner.wrapping_add(outer).wrapping_add(i as u32),
                is_neutral: true,
                is_outer: true,
                is_moon: true,
                team: TEAM_MOON,
                position: *position,
                ..Player::default()
            });
        }
    }
    adjust_positions(map);
}

/// `CMUGeneratePlayerPositions`: every team player as a map owner, start points on the player ring, assigned to
/// teams, team-mates pulled together, positions pushed to valid ground.
pub fn generate_player_positions(map: &mut Map) {
    let count: u32 = map.teams.iter().map(|t| t.players.len() as u32).sum();
    map.s.num_team_players = count;
    for i in 0..count {
        map.players.push(Player { is_outer: true, index: i + 1, ..Player::default() });
    }
    let ring = map.s.ring;
    let mut positions = vec![Point::default(); count as usize];
    place_points_on_ring(&mut map.random, &ring, &mut positions);
    let spacing = spread_points(
        &mut positions,
        &[],
        &ring,
        NEUTRAL_SCALE,
        map.s.convergent_sample_size,
        map.s.convergent_threshold,
        map.s.max_loop_count,
    );
    relax_points(&mut positions, &[], &ring, NEUTRAL_SCALE, spacing, map.s.max_loop_count);
    assign_team_positions(map, &positions);
    spread_teams(&mut map.teams, &ring, map.s.max_angle_between_team_members);
    for t in 0..map.teams.len() {
        for j in 0..map.teams[t].players.len() {
            let number = map.teams[t].players[j];
            map.players[(number - 1) as usize].position = map.teams[t].positions[j];
        }
    }
    adjust_positions(map);
}

/// `CMUAssignTeamPositions`: groups the start points into neighbouring runs, one per team, trying every start point
/// and keeping the most compact grouping.
fn assign_team_positions(map: &mut Map, positions: &[Point]) {
    let team_count = map.teams.len();
    let mut team_positions: Vec<Vec<Point>> = vec![Vec::new(); team_count];
    let mut free = vec![true; team_count];
    let mut best = f32::MAX;
    let mut best_index = 0;
    for (k, position) in positions.iter().enumerate() {
        let mut point = Point::new(position.x + 0.01, position.y);
        let mut remaining: Vec<Point> = positions.to_vec();
        let mut total = 0.0f32;
        for team in &map.teams {
            for _ in 0..team.players.len() {
                let (distance, closest) = find_closest_point(&remaining, point);
                let closest = closest.expect("a point remains for every player");
                total += distance;
                point = remaining[closest];
                remaining.remove(closest);
            }
            if !remaining.is_empty() {
                total -= find_closest_point(&remaining, point).0;
            }
        }
        if total < best {
            best = total;
            best_index = k;
        }
    }
    let mut point = Point::new(positions[best_index].x + 0.01, positions[best_index].y);
    let mut remaining: Vec<Point> = positions.to_vec();
    for (i, team) in map.teams.iter().enumerate() {
        for _ in 0..team.players.len() {
            let (_, closest) = find_closest_point(&remaining, point);
            let closest = closest.expect("a point remains for every player");
            point = remaining[closest];
            team_positions[i].push(point);
            remaining.remove(closest);
        }
    }
    for run in &team_positions {
        let Some(k) = (0..team_count).find(|&k| free[k] && map.teams[k].players.len() == run.len()) else { continue };
        free[k] = false;
        for (j, position) in run.iter().enumerate() {
            map.teams[k].positions[j] = *position;
        }
    }
    for i in 0..team_count {
        for j in 0..map.teams[i].players.len() {
            let number = map.teams[i].players[j];
            map.players[(number - 1) as usize].team = i as i32;
        }
    }
}

/// `CMUWrapAngle`.
fn wrap_angle(mut angle: f32) -> f32 {
    while angle > 3.1415927 {
        angle -= 6.2831855;
    }
    while angle < -3.1415927 {
        angle += 6.2831855;
    }
    angle
}

#[derive(Clone, Copy, Default)]
struct TeamAngle {
    distance: f32,
    angle: f32,
    index: u32,
}

/// `CMUSpreadTeams`: pulls each team's members together around the ring centre.
fn spread_teams(teams: &mut [Team], ring: &Ring, spread: f32) {
    let mut base_angle = 0.0f32;
    for team in teams.iter_mut() {
        let count = team.players.len() as u32;
        if count <= 1 {
            continue;
        }
        let mut angles = vec![TeamAngle::default(); count as usize];
        let mut offset = Vector::default();
        for i in 0..count as usize {
            angles[i].index = i as u32;
            let point = team.positions[i];
            offset.x = point.x - ring.center.x;
            offset.y = point.y - ring.center.y;
            angles[i].distance = offset.magnitude();
            angles[i].angle = um_arctan2(-offset.x, offset.y);
            if i == 0 {
                base_angle = angles[0].angle;
                angles[0].angle = 0.0;
            } else {
                let delta = wrap_angle(angles[i].angle - base_angle) / (i as u32 + 1) as f32;
                base_angle = wrap_angle(delta + base_angle);
                angles[i].angle = wrap_angle(angles[i].angle - base_angle);
                for entry in angles.iter_mut().take(i) {
                    entry.angle = wrap_angle(entry.angle - delta);
                }
            }
        }
        for i in 0..count as usize - 1 {
            for j in i + 1..count as usize {
                if angles[j].angle < angles[i].angle {
                    angles.swap(i, j);
                }
            }
        }
        let mut low = (count >> 1).wrapping_sub(1);
        let mut high = (count >> 1).wrapping_add(1);
        while low < count || high < count {
            if low < count && angles[(low + 1) as usize].angle - angles[low as usize].angle > spread {
                angles[low as usize].angle = angles[(low + 1) as usize].angle - spread;
            }
            if high < count && angles[high as usize].angle - angles[(high - 1) as usize].angle > spread {
                angles[high as usize].angle = spread + angles[(high - 1) as usize].angle;
            }
            low = low.wrapping_sub(1);
            high = high.wrapping_add(1);
        }
        for entry in &angles {
            let rotation = Rotation::yaw(base_angle + entry.angle);
            let rotated = rotation.apply(Vector { x: 0.0, y: entry.distance, z: 0.0 });
            team.positions[entry.index as usize] = Point::new(ring.center.x + rotated.x, ring.center.y + rotated.y);
        }
    }
}

/// `CMUAdjustPositions`: moves every unplaced owner along the line from the map centre until a valid spot with a
/// usable square around it.
fn adjust_positions(map: &mut Map) {
    let mut border = (map.s.minimum_start_position_to_map_edge.wrapping_add(map.s.water_border)) as u32;
    if map.s.map_characteristics & CHARACTERISTIC_EARTH_OR_MARS != 0 {
        border = border.wrapping_sub(map.s.water_border as u32);
    }
    let center = Point::new(map.s.maximum_x as f32 * 0.5, map.s.maximum_y as f32 * 0.5);
    let area = map.s.flat_area;
    let abs = |v: f32| if v < 0.0 { -v } else { v };
    let width = area.right - area.left;
    let height = area.bottom - area.top;
    let size = if abs(width) > abs(height) { lrintf(abs(width)) as u32 } else { lrintf(abs(height)) as u32 };
    for i in 0..map.players.len() {
        if map.players[i].is_placed {
            continue;
        }
        let position = map.players[i].position;
        let mut direction = Vector { x: position.x - center.x, y: position.y - center.y, z: 0.0 };
        direction.normalize();
        let mut step = if map.players[i].is_moon {
            1.0f32
        } else if map.players[i].is_outer {
            map.s.outer_push_from_map_center as f32
        } else {
            map.s.inner_push_from_map_center as f32
        };
        let radius = if !map.players[i].is_neutral {
            size.wrapping_add(border)
        } else if map.players[i].is_moon {
            0
        } else {
            border
        };
        let mut searching = true;
        let mut j = 0;
        while j < map.s.max_loop_count && searching {
            let offset = direction.scale(step);
            let x = lrintf(offset.x + position.x) as u32;
            let y = lrintf(position.y + offset.y) as u32;
            if x < map.s.grid_size
                && y < map.s.grid_size
                && map.valid_tiles.get(x, y) != 0
                && !is_square_open(&map.valid_tiles, x, y, radius)
            {
                searching = false;
                map.players[i].position = Point::new(x as f32, y as f32);
            } else {
                step -= 1.0;
            }
            j += 1;
        }
        map.players[i].is_placed = true;
    }
}

/// `CMUGenerateResources`.
pub fn generate_resources(map: &mut Map) {
    let mut sites: Vec<Vec<Point>> = map.players.iter().map(|p| vec![p.position]).collect();
    place_player_objects(map);
    for kind in 0..OBJECT_COUNT {
        place_objects(map, &mut sites, kind);
    }
    place_spaced_objects(map, &mut sites);
    place_resources(map, &mut sites);
}

#[derive(Clone, Copy)]
struct Segment {
    start: Point,
    end: Point,
}

#[derive(Clone, Copy)]
struct Candidate {
    segment: Segment,
    distance: f32,
}

/// `CMUConnectNext`: connects the next remaining point to its nearest connected point, adding up to `count - 1` more
/// of its shortest links.
fn connect_next(remaining: &mut Vec<Point>, connected: &mut Vec<Point>, segments: &mut Vec<Segment>, count: u32) {
    let the_count = connected.len();
    let mut best = 0.0f32;
    let mut best_index = 0;
    for (i, point) in connected.iter().enumerate() {
        let distance = remaining[0].distance_to_squared(*point);
        if distance < best || i == 0 {
            best = distance;
            best_index = i;
        }
    }
    let start = remaining[0];
    let end = connected[best_index];
    segments.push(Segment { start, end });
    if count > 1 {
        let mut candidates = Vec::new();
        for point in remaining.iter().skip(1) {
            candidates.push(Candidate {
                segment: Segment { start, end: *point },
                distance: start.distance_to_squared(*point),
            });
        }
        for (i, point) in connected.iter().enumerate().take(the_count) {
            if i != best_index {
                candidates.push(Candidate {
                    segment: Segment { start, end: *point },
                    distance: start.distance_to_squared(*point),
                });
            }
        }
        for _ in 1..count {
            if candidates.is_empty() {
                continue;
            }
            let mut shortest = 0;
            for (i, candidate) in candidates.iter().enumerate() {
                if candidate.distance < candidates[shortest].distance {
                    shortest = i;
                }
            }
            segments.push(candidates[shortest].segment);
            candidates.remove(shortest);
        }
    }
    connected.push(remaining[0]);
    remaining.remove(0);
}

/// `CMUDrawShallows`: raises the heights along each segment, at the shallows width, up to the shallows elevation.
fn draw_shallows(map: &mut Map, segments: &[Segment]) {
    let half_width = map.s.shallows_width as f32 * 0.5;
    let size = map.heights.size;
    let elevation = map.s.shallows_elevation;
    let mut line = Vec::new();
    let mut width = Vec::new();
    for segment in segments {
        let rotation = Rotation::yaw(HALF_PI);
        let direction = Vector { x: segment.end.x - segment.start.x, y: segment.end.y - segment.start.y, z: 0.0 };
        let mut normal = rotation.apply(direction);
        normal.normalize();
        normal.x *= half_width;
        normal.y *= half_width;
        segment.start.interpolate_to(segment.end, 1.0, &mut line);
        for point in &line {
            let left = Point::new(point.x + normal.x, point.y + normal.y);
            let right = Point::new(point.x - normal.x, point.y - normal.y);
            left.interpolate_to(right, 1.0, &mut width);
            for w in &width {
                let wx = lrintf(w.x - 1.0) as u32;
                let wy = lrintf(w.y - 1.0) as u32;
                let mut x = wx;
                while x < wx.wrapping_add(3) {
                    let mut y = wy;
                    while y < wy.wrapping_add(3) {
                        if x < size && y < size && map.heights.get(x, y) < elevation {
                            map.heights.set(x, y, elevation);
                        }
                        y = y.wrapping_add(1);
                    }
                    x = x.wrapping_add(1);
                }
            }
        }
    }
}

/// `CMUGenerateShallows`: one random tile per land mass, connected from the first of the farthest pair.
pub fn generate_shallows(map: &mut Map) {
    if map.s.generate_shallows == 0 || map.s.max_shallow_connections_per_player <= 0 {
        return;
    }
    let mut remaining = Vec::new();
    for player in 0..map.players.len() {
        if !map.players[player].tiles.is_empty() {
            let last = map.players[player].tiles.len() as i32 - 1;
            let index = map.random_integers.generate_int(0, last) as usize;
            remaining.push(map.players[player].tiles[index]);
        }
    }
    if remaining.is_empty() {
        return;
    }
    let mut best = 0.0f32;
    let mut best_index = 0;
    for i in 0..remaining.len() - 1 {
        for j in i + 1..remaining.len() {
            let distance = remaining[i].distance_to_squared(remaining[j]);
            if distance > best {
                best = distance;
                best_index = i;
            }
        }
    }
    let mut connected = vec![remaining[best_index]];
    remaining.remove(best_index);
    let mut segments = Vec::new();
    while !remaining.is_empty() {
        connect_next(&mut remaining, &mut connected, &mut segments, map.s.max_shallow_connections_per_player as u32);
    }
    draw_shallows(map, &segments);
}

/// One step of the generator, as the `rmap` PIM entry points call them.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Step {
    PlayerPositions,
    NeutralPositions,
    PlayerLandWater,
    FlatMap,
    HeightMap,
    FilterHeightMap,
    Shallows,
    Resources,
    Trees,
}

/// Runs one step on the map.
pub fn run_step(map: &mut Map, step: Step) {
    match step {
        Step::PlayerPositions => generate_player_positions(map),
        Step::NeutralPositions => generate_neutral_positions(map),
        Step::PlayerLandWater => create_land_water_map(map),
        Step::FlatMap => create_flat_map(map),
        Step::HeightMap => fractal::create_height_map(map),
        Step::FilterHeightMap => fractal::filter_height_map(map),
        Step::Shallows => generate_shallows(map),
        Step::Resources => generate_resources(map),
        Step::Trees => create_trees(map),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::map::{Grid, URandomState};
    use ee_random::URandom;

    fn test_map(size: u32) -> Map {
        let n = size + 1;
        let mut map = Map::default();
        map.s.grid_size = size;
        map.s.maximum_x = size;
        map.s.maximum_y = size;
        map.s.max_loop_count = 200;
        map.s.max_iterations_clump_growth = 100;
        map.s.convergent_sample_size = 2.0;
        map.s.convergent_threshold = 0.1;
        map.s.tile_count = n * n;
        map.valid_tiles = Grid::new(n, 1);
        map.claimed_tiles = Grid::new(n, 0);
        map.flat_tiles = Grid::new(n, 0);
        map.tiles = Grid::new(n, 0);
        map.heights = Grid::new(n, 0.5);
        map.random = URandomState(URandom::new(11));
        map.random_integers = URandomState(URandom::new(13));
        let radius = size as f32 * 0.25;
        map.s.ring =
            Ring { center: Point::new(size as f32 * 0.5, size as f32 * 0.5), min_radius: radius, max_radius: radius };
        map
    }

    fn two_players(map: &mut Map) {
        map.teams = vec![
            Team { players: vec![1], positions: vec![Point::default()] },
            Team { players: vec![2], positions: vec![Point::default()] },
        ];
    }

    #[test]
    fn wrap_angle_brings_angles_into_range() {
        assert_eq!(wrap_angle(0.5), 0.5);
        assert!((wrap_angle(7.0) - (7.0 - 6.2831855)).abs() < 1e-5);
        assert!((wrap_angle(-7.0) + (7.0 - 6.2831855)).abs() < 1e-5);
    }

    #[test]
    fn player_positions_add_owners_and_assign_teams() {
        let mut map = test_map(32);
        two_players(&mut map);
        generate_player_positions(&mut map);
        assert_eq!(map.s.num_team_players, 2);
        assert_eq!(map.players.len(), 2);
        assert_eq!(map.players[0].index, 1);
        assert_eq!(map.players[1].index, 2);
        assert!(map.players.iter().all(|p| p.is_placed && !p.is_neutral));
        assert_eq!(map.players[0].team, 0);
        assert_eq!(map.players[1].team, 1);
    }

    #[test]
    fn neutrals_get_their_kinds_and_indices() {
        let mut map = test_map(32);
        map.s.num_outer_neutrals = 1;
        map.s.num_inner_neutrals = 1;
        map.s.num_moons = 1;
        map.s.neutral_ring = map.s.ring;
        map.s.moon_ring = map.s.ring;
        generate_neutral_positions(&mut map);
        assert_eq!(map.players.len(), 3);
        assert_eq!(map.players[0].team, TEAM_OUTER_NEUTRAL);
        assert_eq!((map.players[1].team, map.players[1].index), (TEAM_INNER_NEUTRAL, 1));
        assert_eq!((map.players[2].team, map.players[2].index), (TEAM_MOON, 2));
        assert!(map.players[2].is_moon && map.players[2].is_outer);
    }

    #[test]
    fn land_water_records_tiles_and_radius() {
        let mut map = test_map(32);
        two_players(&mut map);
        generate_player_positions(&mut map);
        map.s.percent_land = 0.2;
        map.s.player_land_clumps = 2;
        map.s.player_land_chaos = 0.5;
        map.s.num_inner_neutrals = 1;
        map.s.num_outer_neutrals = 1;
        create_land_water_map(&mut map);
        for player in &map.players {
            assert_eq!(player.tile_count as usize, player.tiles.len());
            assert!(player.tile_count > 0);
            assert!(player.radius > 0.0);
            assert!(!player.clump_centers.is_empty());
        }
        assert_eq!(map.clump_circles.len(), 2);
    }

    #[test]
    fn shallows_are_skipped_unless_asked_for() {
        let mut map = test_map(16);
        map.players.push(Player { tiles: vec![Point::new(1.0, 1.0)], ..Player::default() });
        map.players.push(Player { tiles: vec![Point::new(12.0, 12.0)], ..Player::default() });
        map.s.shallows_elevation = 0.9;
        map.s.shallows_width = 2;
        let before = map.heights.clone();
        generate_shallows(&mut map);
        assert_eq!(map.heights, before);
        map.s.generate_shallows = 1;
        map.s.max_shallow_connections_per_player = 1;
        generate_shallows(&mut map);
        assert_eq!(map.heights.get(6, 6), 0.9);
    }

    #[test]
    fn connect_next_links_the_nearest_and_extra_short_links() {
        let mut remaining = vec![Point::new(0.0, 0.0), Point::new(1.0, 0.0)];
        let mut connected = vec![Point::new(10.0, 0.0), Point::new(3.0, 0.0)];
        let mut segments = Vec::new();
        connect_next(&mut remaining, &mut connected, &mut segments, 2);
        assert_eq!(segments.len(), 2);
        assert_eq!(segments[0].end, Point::new(3.0, 0.0));
        assert_eq!(segments[1].end, Point::new(1.0, 0.0)); // the shortest other link
        assert_eq!(connected.len(), 3);
        assert_eq!(remaining.len(), 1);
    }

    #[test]
    fn trees_without_tree_types_do_nothing() {
        let mut map = test_map(16);
        map.players.push(Player { clump_centers: vec![Point::new(8.0, 8.0)], ..Player::default() });
        create_trees(&mut map);
        assert!(map.objects.is_empty());
    }

    #[test]
    fn spread_teams_keeps_members_within_the_maximum_angle() {
        let ring = Ring { center: Point::new(0.0, 0.0), min_radius: 10.0, max_radius: 10.0 };
        let mut teams =
            vec![Team { players: vec![1, 2], positions: vec![Point::new(10.0, 0.0), Point::new(-10.0, 0.0)] }];
        spread_teams(&mut teams, &ring, 0.5);
        let a = teams[0].positions[0];
        let b = teams[0].positions[1];
        assert!((a.distance_to(b) - 2.0 * 10.0 * (0.25f32).sin()).abs() < 0.1, "{a:?} {b:?}");
    }
}
