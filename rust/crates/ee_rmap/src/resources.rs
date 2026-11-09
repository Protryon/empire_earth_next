//! Resource, herd and object placement around the players (`Unit12.cpp`).

use crate::grid::{are_all_masked, is_any_masked_tile_clear, is_square_open, mark_line, mark_square};
use crate::map::{
    is_cell_set, Feature, Map, Object, ResourceKind, CHARACTERISTIC_PLANETS, OBJECT_COUNT, OBJECT_FISH, OBJECT_GOLD,
    OBJECT_IRON, OBJECT_STONE, OBJECT_TREE,
};
use crate::math::{floor, floor_unsigned, lrintf, sqrt_u32, um_cos, Point, HALF_PI, PI, TWO_PI};

const FOOTPRINT_OFFSET: f32 = 2.0;

/// `IsSteep`.
pub fn is_steep(map: &Map, x: u32, y: u32) -> bool {
    let v = &map.valid_tiles;
    let (x1, y1) = (x.wrapping_add(1), y.wrapping_add(1));
    if !is_cell_set(v, x, y) || !is_cell_set(v, x, y1) || !is_cell_set(v, x1, y) || !is_cell_set(v, x1, y1) {
        return false;
    }
    let mut maximum = map.heights.get(x, y);
    let mut minimum = maximum;
    for height in [map.heights.get(x1, y), map.heights.get(x, y1), map.heights.get(x1, y1)] {
        if height < minimum {
            minimum = height;
        } else if height > maximum {
            maximum = height;
        }
    }
    maximum - minimum > map.s.elevation_unit
}

/// `CountSteepCells`.
pub fn count_steep_cells(map: &Map, x: u32, y: u32, radius: u32) -> u32 {
    let size = map.valid_tiles.size;
    let mut left = x.wrapping_sub(radius);
    let mut right = x.wrapping_add(radius);
    let mut top = y.wrapping_sub(radius);
    let mut bottom = y.wrapping_add(radius);
    if left >= size {
        left = 0;
    }
    if top >= size {
        top = 0;
    }
    if right >= size {
        right = size.wrapping_sub(1);
    }
    if bottom >= size {
        bottom = size.wrapping_sub(1);
    }
    let mut count = 0u32;
    let mut i = left;
    while i <= right {
        let mut j = top;
        while j <= bottom {
            if is_steep(map, i, j) {
                count = count.wrapping_add(1);
            }
            j = j.wrapping_add(1);
        }
        i = i.wrapping_add(1);
    }
    count
}

fn is_a_start_position(map: &Map, x: u32, y: u32) -> bool {
    let mut free = true;
    let mut i = 0;
    while i < map.players.len() && free {
        let position = map.players[i].position;
        free = !(floor_unsigned(position.x) == x && floor_unsigned(position.y) == y);
        i += 1;
    }
    !free
}

/// `CanPlaceResource`.
pub fn can_place_resource(map: &Map, kind: i32, x: u32, y: u32) -> bool {
    if is_a_start_position(map, x, y) {
        return false;
    }
    let s = &map.s;
    if kind == OBJECT_TREE {
        if map.claimed_tiles.get(x, y) == 0
            && map.heights.get(x, y) <= s.max_tree_elevation
            && map.heights.get(x, y) >= s.land_elevation_threshold
            && !is_square_open(&map.valid_tiles, x, y, s.resource_to_edge_distance)
            && !is_any_masked_tile_clear(&map.valid_tiles, &map.tiles, x, y, s.resource_to_water_distance)
            && count_steep_cells(map, x, y, 1) == 0
            && map.tiles.get(x, y) < 0x8000
        {
            if (s.map_characteristics & CHARACTERISTIC_PLANETS) != CHARACTERISTIC_PLANETS {
                return true;
            }
            if (s.terrain_band_offset.wrapping_add(map.tiles.get(x, y)) % s.num_terrain_bands)
                < map.tree_types.len() as u32
            {
                return true;
            }
        }
    } else if map.claimed_tiles.get(x, y) == 0
        && map.heights.get(x, y) <= s.max_resource_elevation
        && map.heights.get(x, y) >= s.land_elevation_threshold
        && !is_square_open(&map.valid_tiles, x, y, s.resource_to_edge_distance)
        && !is_any_masked_tile_clear(&map.valid_tiles, &map.tiles, x, y, s.resource_to_water_distance)
        && count_steep_cells(map, x, y, 1) == 0
    {
        return true;
    }
    false
}

/// `IsGoodResourceSite`.
fn is_good_resource_site(map: &Map, kind: &ResourceKind, radius: u32, x: u32, y: u32) -> bool {
    if is_a_start_position(map, x, y) {
        return false;
    }
    let s = &map.s;
    let square = if s.resource_to_edge_distance > radius { s.resource_to_edge_distance } else { radius };
    let clear = if s.resource_to_water_distance > radius { s.resource_to_water_distance } else { radius };
    if are_all_masked(&map.valid_tiles, &map.claimed_tiles, x, y, radius)
        && map.heights.get(x, y) <= s.max_resource_elevation
        && map.heights.get(x, y) >= s.land_elevation_threshold
        && !is_square_open(&map.valid_tiles, x, y, square)
        && !is_any_masked_tile_clear(&map.valid_tiles, &map.tiles, x, y, clear)
    {
        radius.wrapping_mul(radius).wrapping_mul(4).wrapping_sub(count_steep_cells(map, x, y, radius)) >= kind.area
    } else {
        false
    }
}

/// `IsWellSpaced`.
fn is_well_spaced(map: &Map, sites: &[Vec<Point>], kind: u32, x: u32, y: u32, player: u32) -> bool {
    let point = Point::new(x as f32, y as f32);
    let minimum = map.s.resource_seperation * map.s.resource_seperation;
    let near = map.s.kind_spacing[kind as usize][0] as f32;
    let far = map.s.kind_spacing[kind as usize][1] as f32;
    let mut result = true;
    let mut p = 0;
    while p < sites.len() && result {
        let mut i = 0;
        while i < sites[p].len() && result {
            if i == 0 {
                if point.x == sites[p][0].x && point.y == sites[p][0].y {
                    result = false;
                } else {
                    result = true;
                    if p as u32 == player && !map.players[p].is_neutral {
                        let distance = point.distance_to_squared(sites[p][0]);
                        result = !(distance < near || distance > far);
                    }
                }
            } else {
                result = !(point.distance_to_squared(sites[p][i]) < minimum);
            }
            i += 1;
        }
        p += 1;
    }
    result
}

/// `IsShoreSite`.
fn is_shore_site(map: &Map, x: u32, y: u32) -> bool {
    let size = map.s.grid_size;
    let v = &map.valid_tiles;
    map.claimed_tiles.get(x, y) == 0
        && map.heights.get(x, y) < map.s.land_elevation_threshold - (map.s.elevation_unit + map.s.elevation_unit)
        && x.wrapping_add(1) < size
        && v.get(x + 1, y) != 0
        && y.wrapping_add(1) < size
        && v.get(x, y + 1) != 0
        && v.get(x + 1, y + 1) != 0
        && !is_square_open(v, x, y, 1)
}

/// Claims a 4x4 square and a path to the owner's nearest site, and records the point as a new site.
fn claim_site(map: &mut Map, sites: &mut [Vec<Point>], position: Point, player: u32) {
    let list = &sites[player as usize];
    let mut nearest = list[0];
    for site in list {
        if position.distance_to_squared(*site) <= position.distance_to_squared(nearest) {
            nearest = *site;
        }
    }
    mark_line(&mut map.claimed_tiles, position, nearest, 0.75);
    sites[player as usize].push(position);
}

/// `PlaceResource`: one herd animal near a point with a random facing.
fn place_resource(map: &mut Map, sites: &mut [Vec<Point>], kind: &ResourceKind, position: Point, player: u32) {
    let x = floor_unsigned(position.x - FOOTPRINT_OFFSET);
    let y = floor_unsigned(position.y - FOOTPRINT_OFFSET);
    let angle = map.random.generate() * TWO_PI - PI;
    let jitter_x = (map.random.generate() - 0.5) * 0.75 + position.x;
    let jitter_y = (map.random.generate() - 0.5) * 0.75 + position.y;
    let jitter = Point::new(jitter_x, jitter_y);
    map.features.push(Feature {
        angle,
        position: jitter,
        unit_type: kind.unit_type,
        index: player as i32,
        is_world_unit: 1,
        in_ring: 0,
        is_herd: 1,
    });
    mark_square(&mut map.claimed_tiles, x, y, 4);
    claim_site(map, sites, position, player);
}

/// `PlaceResourcesNear`: a herd's animals on random valid, unclaimed, gentle tiles in the square around a point.
#[allow(clippy::too_many_arguments)]
fn place_resources_near(
    map: &mut Map,
    sites: &mut [Vec<Point>],
    kind: &ResourceKind,
    x: u32,
    y: u32,
    radius: u32,
    player: u32,
) {
    let size = map.s.grid_size;
    let mut left = x.wrapping_sub(radius);
    let mut right = x.wrapping_add(radius);
    let mut top = y.wrapping_sub(radius);
    let mut bottom = y.wrapping_add(radius);
    if left >= size {
        left = 0;
    }
    if top >= size {
        top = 0;
    }
    if right >= size {
        right = size.wrapping_sub(1);
    }
    if bottom >= size {
        bottom = size.wrapping_sub(1);
    }
    let mut candidates = Vec::new();
    let mut i = left;
    while i <= right {
        let mut j = top;
        while j <= bottom {
            if map.valid_tiles.get(i, j) != 0 && map.claimed_tiles.get(i, j) == 0 && !is_steep(map, i, j) {
                candidates.push(Point::new(i as f32 + 0.5, j as f32 + 0.5));
            }
            j = j.wrapping_add(1);
        }
        i = i.wrapping_add(1);
    }
    let count = if (candidates.len() as u64) < u64::from(kind.area) { candidates.len() as u32 } else { kind.area };
    for _ in 0..count {
        let choice = map.random_integers.generate_int(0, candidates.len() as i32 - 1) as usize;
        place_resource(map, sites, kind, candidates[choice], player);
        candidates.remove(choice);
    }
}

fn herd_radius(kind: &ResourceKind) -> u32 {
    let radius = lrintf(sqrt_u32(kind.area) * 0.5) as u32;
    radius.wrapping_add(1)
}

/// `PlaceResourceKind`: one herd on the next neutral land mass in placement order.
fn place_resource_kind(map: &mut Map, sites: &mut [Vec<Point>], kind: &ResourceKind) {
    let size = herd_radius(kind);
    if map.player_order.is_empty() {
        build_player_order(map);
    }
    if map.player_order.is_empty() {
        return;
    }
    let player = map.player_order[(map.s.player_order_index as usize) % map.player_order.len()];
    map.s.player_order_index = map.s.player_order_index.wrapping_add(1);
    for _ in 0..map.s.max_loop_count {
        let last = map.players[player as usize].tile_count.wrapping_sub(1);
        let index = map.random_integers.generate_int(0, last) as usize;
        let point = map.players[player as usize].tiles[index];
        let x = floor_unsigned(point.x);
        let y = floor_unsigned(point.y);
        if is_good_resource_site(map, kind, size, x, y) {
            place_resources_near(map, sites, kind, x, y, size, player as u32);
            break;
        }
    }
}

/// `PlaceResourceAroundPlayer`: one herd at a random angle and distance from a player's start.
fn place_resource_around_player(map: &mut Map, sites: &mut [Vec<Point>], kind: &ResourceKind, player: u32) {
    let center = map.players[player as usize].position;
    let size = herd_radius(kind);
    for _ in 0..map.s.max_loop_count {
        let angle = map.random.generate() * TWO_PI - PI;
        let distance = map.random.generate() * kind.maximum.wrapping_sub(kind.minimum) as f32 + kind.minimum as f32;
        let x = um_cos(angle) * distance + center.x;
        let y = um_cos(HALF_PI - angle) * distance + center.y;
        if !(x >= 0.0) || !(y >= 0.0) {
            continue;
        }
        let cell_x = floor_unsigned(x);
        let cell_y = floor_unsigned(y);
        if is_good_resource_site(map, kind, size, cell_x, cell_y) {
            place_resources_near(map, sites, kind, cell_x, cell_y, size, player);
            break;
        }
    }
}

/// `PlaceResources`: every herd slot from the script.
pub fn place_resources(map: &mut Map, sites: &mut [Vec<Point>]) {
    for i in 0..map.resource_slots.len() {
        let slot = map.resource_slots[i];
        if slot.per_player != 0 {
            for p in 0..map.players.len() {
                if map.players[p].is_neutral {
                    continue;
                }
                let id = if slot.random_kind != 0 { random_kind_id(map) } else { slot.id };
                if let Some(kind) = map.resource_kinds.iter().find(|k| k.id == id).copied() {
                    place_resource_around_player(map, sites, &kind, p as u32);
                }
            }
        } else {
            let id = if slot.random_kind != 0 { random_kind_id(map) } else { slot.id };
            if let Some(kind) = map.resource_kinds.iter().find(|k| k.id == id).copied() {
                place_resource_kind(map, sites, &kind);
            }
        }
    }
}

fn random_kind_id(map: &mut Map) -> i32 {
    let last = map.resource_kind_ids.len() as i32 - 1;
    map.resource_kind_ids[map.random_integers.generate_int(0, last) as usize]
}

/// `PlacePlayerObjects`: each moon gets its resource count of gold or iron on usable tiles.
pub fn place_player_objects(map: &mut Map) {
    for p in 0..map.players.len() {
        if !map.players[p].is_moon {
            continue;
        }
        let count = map.s.moon_resources;
        let mut placed = 0u32;
        let mut tries = 0u32;
        while placed < count && tries < count.wrapping_mul(64) {
            tries += 1;
            let last = map.players[p].tile_count.wrapping_sub(1);
            let index = map.random_integers.generate_int(0, last) as usize;
            let point = map.players[p].tiles[index];
            let x = floor_unsigned(point.x);
            let y = floor_unsigned(point.y);
            if is_square_open(&map.valid_tiles, x, y, 1)
                || is_any_masked_tile_clear(&map.valid_tiles, &map.tiles, x, y, 1)
                || map.claimed_tiles.get(x, y) == 1
            {
                continue;
            }
            let kind = if map.random_integers.generate() >= 0.5 { OBJECT_IRON } else { OBJECT_GOLD };
            map.objects.push(Object { location: point, kind, object_type: 0 });
            mark_square(&mut map.claimed_tiles, x, y, 4);
            placed += 1;
        }
    }
}

/// `PlaceObjectAt` (`PlaceObject` only forwards).
fn place_object(map: &mut Map, sites: &mut [Vec<Point>], kind: u32, position: Point, player: u32) {
    let x = floor_unsigned(position.x - FOOTPRINT_OFFSET);
    let y = floor_unsigned(position.y - FOOTPRINT_OFFSET);
    let mut object = Object { location: position, kind: kind as i32, object_type: 0 };
    if kind as i32 == OBJECT_FISH && map.s.is_space_map != 0 {
        object.kind = match map.random_integers.generate_int(0, 2) {
            0 => OBJECT_STONE,
            1 => OBJECT_GOLD,
            _ => OBJECT_IRON,
        };
    }
    if kind as i32 == OBJECT_TREE {
        let tree = if (map.s.map_characteristics & CHARACTERISTIC_PLANETS) == CHARACTERISTIC_PLANETS {
            map.s
                .terrain_band_offset
                .wrapping_add(map.tiles.get(floor_unsigned(position.x), floor_unsigned(position.y)))
                % map.s.num_terrain_bands
        } else {
            let last = map.tree_types.len() as i32 - 1;
            map.random_integers.generate_int(0, last) as u32
        };
        object.object_type = map.tree_types[tree as usize];
    }
    map.objects.push(object);
    mark_square(&mut map.claimed_tiles, x, y, 4);
    if kind as i32 != OBJECT_FISH {
        claim_site(map, sites, position, player);
    }
}

/// `GetFartherPoint`: whichever of two points is farther from a player's start.
fn farther_point(map: &Map, player: u32, a: Point, b: Point) -> Point {
    let position = map.players[player as usize].position;
    if position.distance_to_squared(a) > position.distance_to_squared(b) {
        a
    } else {
        b
    }
}

/// The candidate error to the spacing limits: distance minus the limit it is closer to reach, as an absolute value.
fn spacing_error(distance: f32, target: f32) -> f32 {
    let error = distance - target;
    if error < 0.0 {
        -error
    } else {
        error
    }
}

/// `PlaceObjectsForPlayer`: a player's share of one resource kind, preferring well spaced sites.
fn place_objects_for_player(map: &mut Map, sites: &mut [Vec<Point>], kind: u32, player: u32) {
    let p = player as usize;
    let position = map.players[p].position;
    let last = map.players[p].tile_count.wrapping_sub(1);
    let tries = map.s.max_loop_count;
    if map.players[p].tile_count == 0 {
        return;
    }
    let mut n = map.s.resource_per_player[kind as usize];
    while n != 0 {
        let mut done = false;
        let mut found = false;
        let mut best = Point::default();
        let mut best_error = 0.0f32;
        let mut t = 0;
        while t < tries && !done {
            t += 1;
            let (candidate, x, y, usable) = if kind as i32 == OBJECT_FISH {
                let last_water = map.water_points.len() as i32 - 1;
                let candidate = map.water_points[map.random_integers.generate_int(0, last_water) as usize];
                let (x, y) = (floor_unsigned(candidate.x), floor_unsigned(candidate.y));
                (candidate, x, y, is_shore_site(map, x, y))
            } else {
                let candidate = map.players[p].tiles[map.random_integers.generate_int(0, last) as usize];
                let (x, y) = (floor_unsigned(candidate.x), floor_unsigned(candidate.y));
                (candidate, x, y, can_place_resource(map, kind as i32, x, y))
            };
            if !usable {
                continue;
            }
            let distance = candidate.distance_to_squared(position);
            if !is_well_spaced(map, sites, kind, x, y, player) {
                let mut target = map.s.kind_spacing[kind as usize][0] as f32;
                if target <= distance {
                    target = map.s.kind_spacing[kind as usize][1] as f32;
                }
                let error = spacing_error(distance, target);
                if !found {
                    best = candidate;
                    found = true;
                    best_error = error;
                } else if error < best_error {
                    best = candidate;
                    best_error = error;
                }
            } else {
                place_object(map, sites, kind, candidate, player);
                done = true;
            }
        }
        if found && !done {
            place_object(map, sites, kind, best, player);
        }
        n = n.wrapping_sub(1);
    }
}

/// `PlaceObjectsNearPlayer`: one resource kind on a land mass (one for neutrals, the player share otherwise).
fn place_objects_near_player(map: &mut Map, sites: &mut [Vec<Point>], kind: u32, player: u32) {
    let p = player as usize;
    let tries = map.s.max_loop_count;
    if map.players[p].is_moon {
        return;
    }
    let mut n = if map.players[p].is_neutral { 1 } else { map.s.resource_per_player[kind as usize] };
    while n != 0 {
        let mut done = false;
        let mut found = false;
        let mut best = Point::default();
        let mut t = 0;
        while t < tries && !done {
            t += 1;
            if kind as i32 == OBJECT_FISH {
                if !map.water_points.is_empty() {
                    let last = map.water_points.len() as i32 - 1;
                    let candidate = map.water_points[map.random_integers.generate_int(0, last) as usize];
                    let (x, y) = (floor_unsigned(candidate.x), floor_unsigned(candidate.y));
                    if is_shore_site(map, x, y) {
                        place_object(map, sites, kind, candidate, player);
                        done = true;
                    }
                }
            } else if map.players[p].tile_count != 0 {
                let last = map.players[p].tile_count.wrapping_sub(1);
                let candidate = map.players[p].tiles[map.random_integers.generate_int(0, last) as usize];
                let (x, y) = (floor_unsigned(candidate.x), floor_unsigned(candidate.y));
                if can_place_resource(map, kind as i32, x, y) {
                    if is_well_spaced(map, sites, kind, x, y, player) {
                        place_object(map, sites, kind, candidate, player);
                        done = true;
                    } else if !found {
                        best = candidate;
                        found = true;
                    } else {
                        best = farther_point(map, player, best, candidate);
                    }
                }
            }
        }
        if found && !done {
            place_object(map, sites, kind, best, player);
        }
        n = n.wrapping_sub(1);
    }
}

/// `PlaceObjects`: one resource kind for every player, then on the neutral land masses in placement order.
pub fn place_objects(map: &mut Map, sites: &mut [Vec<Point>], kind: u32) {
    debug_assert!(kind < OBJECT_COUNT);
    for p in 0..map.players.len() {
        if !map.players[p].is_neutral {
            place_objects_for_player(map, sites, kind, p as u32);
        }
    }
    if map.player_order.is_empty() {
        build_player_order(map);
    }
    if !map.player_order.is_empty() {
        for _ in 0..map.s.resource_per_neutral_land[kind as usize] {
            let player = map.player_order[(map.s.player_order_index as usize) % map.player_order.len()];
            place_objects_near_player(map, sites, kind, player as u32);
            map.s.player_order_index = map.s.player_order_index.wrapping_add(1);
        }
    }
}

/// `PlaceSpacedObjects`: the script's `PlaceResourceInRing` objects around each player.
pub fn place_spaced_objects(map: &mut Map, sites: &mut [Vec<Point>]) {
    for p in 0..map.players.len() {
        if map.players[p].is_neutral {
            continue;
        }
        let position = map.players[p].position;
        let center = position;
        for k in 0..map.spacing_rules.len() {
            let rule = map.spacing_rules[k];
            let mut done = false;
            let mut found = false;
            let mut best = Point::default();
            let mut best_error = 0.0f32;
            let mut t = 0;
            while t < map.s.max_loop_count && !done {
                t += 1;
                let angle = map.random.generate() * TWO_PI - PI;
                let fraction = map.random.generate();
                let distance = rule.maximum.wrapping_sub(rule.minimum) as f32 * fraction + rule.minimum as f32;
                let candidate =
                    Point::new(um_cos(angle) * distance + center.x, um_cos(HALF_PI - angle) * distance + center.y);
                let x = floor(candidate.x);
                let y = floor(candidate.y);
                if !is_cell_set(&map.valid_tiles, x as u32, y as u32) {
                    continue;
                }
                let usable = if rule.kind != OBJECT_FISH {
                    can_place_resource(map, rule.kind, x as u32, y as u32)
                } else {
                    is_shore_site(map, x as u32, y as u32)
                };
                if !usable {
                    continue;
                }
                let squared = position.distance_to_squared(candidate);
                if !is_well_spaced(map, sites, rule.kind as u32, x as u32, y as u32, p as u32) {
                    let target = if rule.minimum as f32 <= squared { rule.maximum } else { rule.minimum };
                    let error = spacing_error(squared, target as f32);
                    if !found {
                        best = candidate;
                        found = true;
                        best_error = error;
                    } else if error < best_error {
                        best = candidate;
                        best_error = error;
                    }
                } else {
                    place_object(map, sites, rule.kind as u32, candidate, p as u32);
                    done = true;
                }
            }
            if found && !done {
                place_object(map, sites, rule.kind as u32, best, p as u32);
            }
        }
    }
}

/// `BuildPlayerOrder`: the neutral land masses for resource placement, alternating outer and inner ones.
pub fn build_player_order(map: &mut Map) {
    map.player_order.clear();
    let mut first: Vec<i32> = Vec::new();
    let mut second: Vec<i32> = Vec::new();
    for (i, player) in map.players.iter().enumerate() {
        if player.is_neutral && player.tile_count != 0 {
            if player.is_outer {
                first.push(i as i32);
            } else {
                second.push(i as i32);
            }
        }
    }
    if first.is_empty() && second.is_empty() {
        return;
    }
    let mut from_first;
    if second.is_empty() {
        let index = map.random_integers.generate_int(0, first.len() as i32 - 1) as usize;
        map.player_order.push(first[index]);
        from_first = false;
        first.remove(index);
    } else {
        let index = map.random_integers.generate_int(0, second.len() as i32 - 1) as usize;
        map.player_order.push(second[index]);
        from_first = true;
        second.remove(index);
    }
    while !first.is_empty() || !second.is_empty() {
        let list = if from_first { &mut first } else { &mut second };
        let mut best = -1.0f32;
        let mut choice = list.len();
        let last = *map.player_order.last().expect("the order starts with one entry");
        for (i, entry) in list.iter().enumerate() {
            let distance =
                map.players[*entry as usize].position.distance_to_squared(map.players[last as usize].position);
            if distance > best {
                best = distance - map.random.generate() * distance * 0.5;
                choice = i;
            }
        }
        if choice < list.len() {
            map.player_order.push(list[choice]);
            list.remove(choice);
        }
        from_first = !from_first;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::map::{Grid, Player, URandomState};
    use ee_random::URandom;

    fn test_map(size: u32) -> Map {
        let mut map = Map::default();
        map.s.grid_size = size;
        map.s.max_loop_count = 100;
        map.s.elevation_unit = 0.1;
        map.s.max_resource_elevation = 1.0;
        map.s.max_tree_elevation = 1.0;
        map.s.resource_to_edge_distance = 1;
        map.valid_tiles = Grid::new(size + 1, 1);
        map.claimed_tiles = Grid::new(size + 1, 0);
        map.heights = Grid::new(size + 1, 0.5);
        map.tiles = Grid::new(size + 1, 1);
        map.random = URandomState(URandom::new(3));
        map.random_integers = URandomState(URandom::new(4));
        map.players = vec![Player { position: Point::new(4.0, 4.0), tile_count: 0, ..Player::default() }];
        map
    }

    #[test]
    fn steep_blocks_differ_by_more_than_one_step() {
        let mut map = test_map(8);
        assert!(!is_steep(&map, 1, 1));
        map.heights.set(2, 2, 0.8);
        assert!(is_steep(&map, 1, 1));
        map.heights.set(2, 2, 0.55); // within the step
        assert!(!is_steep(&map, 1, 1));
    }

    #[test]
    fn steep_is_false_off_the_valid_map() {
        let mut map = test_map(8);
        map.valid_tiles.set(1, 1, 0);
        map.heights.set(2, 2, 0.8);
        assert!(!is_steep(&map, 1, 1));
    }

    #[test]
    fn a_start_position_takes_no_resource() {
        let map = test_map(8);
        assert!(!can_place_resource(&map, OBJECT_GOLD, 4, 4));
        assert!(can_place_resource(&map, OBJECT_GOLD, 3, 3));
    }

    #[test]
    fn a_claimed_tile_takes_no_resource_and_trees_stop_above_the_tree_line() {
        let mut map = test_map(8);
        map.claimed_tiles.set(3, 3, 1);
        assert!(!can_place_resource(&map, OBJECT_GOLD, 3, 3));
        map.s.max_tree_elevation = 0.2;
        assert!(!can_place_resource(&map, OBJECT_TREE, 2, 2));
    }

    #[test]
    fn moon_tiles_take_no_trees() {
        let mut map = test_map(8);
        map.tiles.set(2, 2, 0x8001);
        assert!(!can_place_resource(&map, OBJECT_TREE, 2, 2));
        assert!(can_place_resource(&map, OBJECT_GOLD, 2, 2));
    }

    #[test]
    fn spacing_needs_the_near_far_band_for_its_own_player() {
        let mut map = test_map(8);
        map.s.kind_spacing[0] = [4, 100];
        map.s.resource_seperation = 1.0;
        let sites = vec![vec![Point::new(4.0, 4.0)]];
        assert!(is_well_spaced(&map, &sites, 0, 6, 4, 0)); // distance squared 4
        assert!(!is_well_spaced(&map, &sites, 0, 5, 4, 0)); // too near
        assert!(!is_well_spaced(&map, &sites, 0, 4, 4, 0)); // on the site
    }

    #[test]
    fn the_object_count_of_a_moon_is_zero() {
        let mut map = test_map(8);
        map.players[0].is_moon = true;
        map.players[0].is_neutral = true;
        map.s.resource_per_player[0] = 3;
        let mut sites = vec![vec![Point::default()]];
        place_objects_near_player(&mut map, &mut sites, 0, 0);
        assert!(map.objects.is_empty());
    }

    #[test]
    fn player_order_alternates_starting_from_the_inner_list() {
        let mut map = test_map(8);
        map.players.clear();
        for i in 0..4 {
            map.players.push(Player {
                is_neutral: true,
                is_outer: i % 2 == 0,
                tile_count: 5,
                position: Point::new(i as f32, 0.0),
                ..Player::default()
            });
        }
        build_player_order(&mut map);
        assert_eq!(map.player_order.len(), 4);
        // inner first, then outer, inner, outer
        for (position, player) in map.player_order.iter().enumerate() {
            assert_eq!(*player % 2 == 0, position % 2 == 1, "{:?}", map.player_order);
        }
    }

    #[test]
    fn an_empty_order_stays_empty() {
        let mut map = test_map(8);
        map.players[0].is_neutral = false;
        build_player_order(&mut map);
        assert!(map.player_order.is_empty());
    }

    #[test]
    fn a_placed_object_claims_its_square_and_records_a_site() {
        let mut map = test_map(16);
        let mut sites = vec![vec![Point::new(4.0, 4.0)]];
        place_object(&mut map, &mut sites, OBJECT_GOLD as u32, Point::new(10.0, 10.0), 0);
        assert_eq!(map.objects.len(), 1);
        assert_eq!(map.objects[0].kind, OBJECT_GOLD);
        assert_eq!(map.claimed_tiles.get(8, 8), 1);
        assert_eq!(sites[0].len(), 2);
    }

    #[test]
    fn fish_on_a_space_map_become_a_mineral_and_keep_no_site() {
        let mut map = test_map(16);
        map.s.is_space_map = 1;
        let mut sites = vec![vec![Point::new(4.0, 4.0)]];
        place_object(&mut map, &mut sites, OBJECT_FISH as u32, Point::new(10.0, 10.0), 0);
        assert_ne!(map.objects[0].kind, OBJECT_FISH);
        assert_eq!(sites[0].len(), 1);
    }

    #[test]
    fn moon_objects_draw_gold_or_iron_and_stop_at_the_count() {
        let mut map = test_map(16);
        map.s.moon_resources = 2;
        map.players[0].is_moon = true;
        map.players[0].tile_count = 1;
        map.players[0].tiles = vec![Point::new(5.0, 5.0)];
        place_player_objects(&mut map);
        // the one tile is claimed by the first object, so the second never fits
        assert_eq!(map.objects.len(), 1);
        assert!(map.objects[0].kind == OBJECT_GOLD || map.objects[0].kind == OBJECT_IRON);
    }
}
