//! The unit utilities' placement and path leaves (the unit utilities lane, round 1, second part): the site test of a
//! unit type on a tile and its rectangles, a ship's path smoothing, and the launch place and facing of a unit leaving
//! a dock or carrier. Their world is [`UuHost`]'s; the tile searches they call are `uu`'s.

// the C++'s negated float comparisons kept as written (a NaN); `c_long` casts needed on LP64
#![allow(clippy::neg_cmp_op_on_partial_ord, clippy::unnecessary_cast)]
use crate::uu::k::{UU_MOVEMENT_SEA, UU_MOVEMENT_SPACE};
use crate::uu::{can_stand_on, tile_units_standing, units_on, units_on_tiles, UuClassView, UuHost, UuView};
use crate::uu_tiles::{tile_direction, tile_in_direction, turn_direction, Tile};
use ee_render::ge::transform::RotationOrder;
use ee_render::ge::types::{Ge3DLine, Ge3DPoint, GeTransformation, GeVector};
use ee_render::ge::um::{floor, floor_unsigned, lrintf, um_arctan2, um_cos, um_sin};

const TERRAIN_DEEP_WATER: i32 = 1;
const TERRAIN_SPACE: i32 = 12;
const CONTINENT_WATER: u8 = 2;
const CONTINENT_SPACE: u8 = 4;
const GOAL_ATTACK_UNIT: i32 = 2;
const TYPE_NAVY_YARD: u32 = 0x41;
const TYPE_DOCK: u32 = 0x24;
const FAMILY_FISH: u32 = 0x12;
const TWO_PI: f32 = 6.283_185_5;
const HALF_PI: f32 = 1.570_796_4;

// ---- the site test ----

/// `EECanPlaceUnitOnTile` (0x24f264): whether a unit of `unit_type` may be placed at (`x`, `y`): on the map; for a
/// land class, reachable from `unit`'s continent, or (unless `skip_player_checks`) the player's selection's; seen by
/// a player that keeps a visibility map; four neighbours on the map; a terrain the class takes, flat unless it builds
/// on elevation; no blocking unit on the tile. `flag` gets whether units must make way (written only then, as the C++
/// wrote its `bool&`).
#[allow(clippy::too_many_arguments)]
pub fn can_place_unit_on_tile<H: UuHost + ?Sized>(
    h: &mut H,
    player: u64,
    unit: u64,
    unit_type: i32,
    x: i32,
    y: i32,
    skip_player_checks: bool,
    ignore_owner: bool,
    flag: &mut Option<bool>,
) -> bool {
    let map = h.map();
    let tile = h.map_tile(map, x, y);
    let cls = UuClassView(h.class_of_type(unit_type).fields, 0);
    let mut result = tile != 0;
    if tile == 0 {
        return result;
    }
    let continent = h.continent_id(x, y);
    if !cls.terrain(TERRAIN_DEEP_WATER) && !cls.terrain(TERRAIN_SPACE) {
        if unit != 0 {
            let id = h.continent_id(x, y);
            let to = if id != 0 { h.continent_by_id(id) } else { 0 };
            let u = UuView(h.unit(unit));
            let id = h.continent_id(u.tile_x(), u.tile_y());
            let from = if id != 0 { h.continent_by_id(id) } else { 0 };
            result = h.are_connected_for_unit(unit, from, to);
        } else if !skip_player_checks && player != 0 && h.group_units(player) != 0 {
            let id = h.group_continent(player);
            let other = h.continent_by_id(id);
            if other != 0 {
                let category = h.continent_category(other);
                if (category != CONTINENT_WATER && !cls.terrain(TERRAIN_DEEP_WATER))
                    || (category != CONTINENT_SPACE && !cls.terrain(TERRAIN_SPACE))
                {
                    let passability = h.citizen_passability(player);
                    result = h.are_connected_by_id(id, continent, passability);
                }
            }
        }
    }
    if result && !skip_player_checks && player != 0 && h.saves_visibility(player) {
        result = h.tile_visible_to(tile, player);
    }
    if result {
        if h.map_tile(map, x + 1, y) == 0
            || h.map_tile(map, x - 1, y) == 0
            || h.map_tile(map, x, y - 1) == 0
            || h.map_tile(map, x, y + 1) == 0
        {
            result = false;
        } else {
            let terrain = h.tile_terrain(tile);
            result = cls.terrain(terrain);
            if result {
                if !cls.builds_on_elevation() && !h.is_tile_flat(x, y) {
                    result = false;
                } else {
                    let mut units = Vec::new();
                    result = !tile_units_standing(h, player, unit_type, tile, ignore_owner, 2, &mut units);
                    if result {
                        *flag = Some(tile_units_standing(h, player, unit_type, tile, ignore_owner, 1, &mut units));
                    }
                }
            }
        }
    }
    result
}

/// `EEBlockTakes` (0x243c7d) / `EEPlayerBlockTakes` (0x243ce6): whether every tile of the rectangle takes the unit
/// type for the player (column by column; the first refusal ends it).
#[allow(clippy::too_many_arguments)]
pub fn block_takes<H: UuHost + ?Sized>(
    h: &mut H,
    player: u64,
    unit_type: i32,
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    ignore_owner: bool,
) -> bool {
    let mut result = true;
    let mut tx = x;
    while tx < x + width && result {
        let mut ty = y;
        while ty < y + height && result {
            let mut flag = None;
            result = can_place_unit_on_tile(h, player, 0, unit_type, tx, ty, false, ignore_owner, &mut flag);
            ty += 1;
        }
        tx += 1;
    }
    result
}

// ---- a ship's path ----

/// `EEAdjustNavalPath`'s smoothing (0x23df77): the indices of the path points kept: the first, every point the line
/// from the last kept one would cut too close to land (or past a unit, or when chasing a target within range of the
/// path's end, or within the first four, or after three dropped), and the last. `path` is the points' (x, y).
pub fn naval_path<H: UuHost + ?Sized>(h: &mut H, unit: u64, path: &[[f32; 2]]) -> Vec<u32> {
    let count = path.len() as i32;
    let mut kept = Vec::new();
    if count <= 0 {
        return kept;
    }
    let size = h.regions_size();
    kept.push(0);
    let mut start_y = floor(path[0][1]);
    let mut start_x = floor(path[0][0]);
    let mut point: Tile = [0, 0];
    let mut skipped = 0;
    let mut skipping = false;
    let stands = |h: &mut H, t: Tile| can_stand_on(h, t, unit);
    let mut i = 1;
    while i < count - 1 {
        let keep = 'test: {
            if i <= 4 || skipped > 3 {
                break 'test true;
            }
            if h.goal_kind(unit) == GOAL_ATTACK_UNIT && count as f32 - h.stats_range(unit) > i as f32 {
                break 'test true;
            }
            let y = floor(path[i as usize][1]);
            let x = floor(path[i as usize][0]);
            point = [x, y];
            let occupied = h.regions_len(x / size, y / size) > 0
                || h.regions_len((x - 2) / size, (y - 2) / size) != 0
                || h.regions_len((x + 2) / size, (y - 2) / size) != 0
                || h.regions_len((x + 2) / size, (y + 2) / size) != 0
                || h.regions_len((x - 2) / size, (y + 2) / size) != 0;
            if occupied {
                break 'test true;
            }
            let mut blocked = false;
            if !skipping && (x == start_x || y == start_y) {
                break 'test false;
            }
            let line = if skipping { h.tile_line(start_x, start_y, x, y, false, true) } else { Vec::new() };
            let current = [start_x, start_y];
            for &step in line.iter().skip(1) {
                if blocked {
                    break 'test true;
                }
                blocked = !stands(h, step);
                if !blocked {
                    let direction = tile_direction(current[0], current[1], step[0], step[1]);
                    let side = tile_in_direction(current[0], current[1], turn_direction(direction, 1, true), 1);
                    blocked = !stands(h, side);
                    if !blocked {
                        let side = tile_in_direction(current[0], current[1], turn_direction(direction, 1, false), 1);
                        blocked = !stands(h, side);
                    }
                }
            }
            if blocked {
                break 'test true;
            }
            let direction = tile_direction(current[0], current[1], x, y);
            let side = tile_in_direction(current[0], current[1], turn_direction(direction, 1, true), 1);
            let mut keep = !stands(h, side);
            if !keep {
                let side = tile_in_direction(current[0], current[1], turn_direction(direction, 1, false), 1);
                keep = !stands(h, side);
            }
            keep
        };
        if keep {
            kept.push(i as u32);
            skipped = 0;
            skipping = false;
            start_x = point[0];
            start_y = point[1];
        } else {
            skipped += 1;
            skipping = true;
        }
        i += 1;
    }
    if count > 1 {
        kept.push((count - 1) as u32);
    }
    kept
}

// ---- a unit leaving a dock ----

/// `EEGetFanTiles` (0x24b3ed): the tile lines from the centre's tile out to `radius` at `steps + 1` angles from `from`
/// to `to` (a line only when both ends moved on), sorted (`U2DPoint`'s order: lower y, then higher x) and unique.
pub fn fan_tiles<H: UuHost + ?Sized>(
    h: &mut H,
    center: [f32; 2],
    radius: f32,
    mut from: f32,
    to: f32,
    steps: i32,
) -> Vec<Tile> {
    let mut out: Vec<Tile> = Vec::new();
    let mut last_x = 0;
    let mut last_y = 0;
    let y = floor(center[1]);
    let x = floor(center[0]);
    let step = (to - from) / steps as f32;
    let mut i = steps;
    while i >= 0 {
        let end_x = floor(um_cos(from) * radius + x as f32);
        let end_y = floor(um_cos(HALF_PI - from) * radius + y as f32);
        if end_x != last_x && end_y != last_y {
            out.extend(h.tile_line(x, y, end_x, end_y, true, false));
            last_x = end_x;
            last_y = end_y;
        }
        from += step;
        i -= 1;
    }
    // a total order: any sort gives the same list
    out.sort_by(|a, b| (a[1], std::cmp::Reverse(a[0])).cmp(&(b[1], std::cmp::Reverse(b[0]))));
    out.dedup();
    out
}

/// `EEGetDockShipPlacement` (0x24286a): the launch place and facing of a unit of the class leaving the unit (a dock or
/// carrier): unless `facing_only`, first a free space tile off shore within six rings around its exit point, in
/// fifteen-degree steps both ways off its facing; then from the facing toward `point` (`face_point`) or past its last
/// launch yaw, the yaw turned in steps until the line out to the class's reach stays on one continent and area, off
/// shore, with no sea (or space) unit, dock or navy yard in the fan or across the class's width (five full turns at
/// most, each a degree further on). `m` gets the place and yaw; the unit's next launch yaw is written.
#[allow(clippy::too_many_arguments)]
pub fn dock_ship_placement<H: UuHost + ?Sized>(
    h: &mut H,
    point: [f32; 3],
    class_obj: u64,
    unit: u64,
    face_point: bool,
    facing_only: bool,
    m: &mut GeTransformation,
) -> bool {
    let cls = UuClassView(h.class(class_obj).fields, class_obj);
    let complex = UuView(h.unit(unit)).0.complex;
    let me = UuView(h.unit(complex));
    let mut found = false;
    let mut count = 0;
    let kind = if facing_only { 1 } else { 3 };
    let (mut x, mut y) = h.center_offset(complex, me.tile_x() as f32, me.tile_y() as f32);
    if !facing_only {
        let location = h.location(complex);
        let facing = um_arctan2(location.m[1][0], location.m[0][0]);
        let mut try_x = x;
        let mut try_y = y;
        let mut free = false;
        let mut i = 0;
        while i < 6 && !free {
            let ring = i as f32;
            for j in 0..=6 {
                let offset = j as f32 * 0.261_799_4;
                let angle = offset + facing;
                try_x = x - um_sin(angle) * ring;
                try_y = um_cos(angle) * ring + y;
                if open_water(h, try_x, try_y) {
                    free = true;
                    break;
                }
                let other = facing - offset;
                try_x = x - um_sin(other) * ring;
                try_y = um_cos(other) * ring + y;
                if open_water(h, try_x, try_y) {
                    free = true;
                    break;
                }
            }
            i += 1;
        }
        if free {
            x = try_x;
            y = try_y;
        }
    }
    let center = [x, y];
    let radius = cls.outer();
    let half = radius * 0.5;
    let inner = if half > 1.0 { half } else { 1.0 };
    let distance = half * 0.5 + (if half > 1.5 { half } else { 1.5 }) + 0.5;
    let step = inner / distance * 0.5;
    let mut yaw = if face_point {
        let at = me.0.location;
        um_arctan2(point[1] - at[1], point[0] - at[0])
    } else {
        launch_yaw(&me) + step
    };
    let end = yaw + TWO_PI;
    let continent =
        if !facing_only { h.continent_of(floor(x), floor(y), kind) } else { h.footprint_continent(complex, kind) };
    let mut own = Vec::new();
    'search: loop {
        if !((count as f32) < 5.0) {
            break;
        }
        while yaw < end {
            let cos = um_cos(yaw);
            let sin = um_sin(yaw);
            m.m[0][3] = cos * distance + x;
            m.m[1][3] = sin * distance + y;
            let reach = distance + half;
            let end_x = lrintf(reach * cos + x);
            let end_y = lrintf(reach * sin + y);
            let here = h.continent_of(floor(m.m[0][3]), floor(m.m[1][3]), kind);
            let mut line = h.tile_line(floor(x), floor(y), end_x, end_y, true, true);
            crate::uu::footprint_tiles(h, complex, &mut own);
            line.retain(|t| !own.contains(t));
            if here == continent && continent != 0 && h.in_same_area(&line, kind) && !h.has_shore_tile(&line) {
                let fan = fan_tiles(h, center, distance + 2.5, yaw - step, step + yaw, lrintf(radius * 3.0));
                let mut units = Vec::new();
                units_on_tiles(h, &fan, &mut units);
                let mut more = Vec::new();
                units_on(h, &[[floor(m.m[0][3]), floor(m.m[1][3])]], &mut more);
                units.extend_from_slice(&more);
                if !more.is_empty() && units.len() > 1 {
                    sort_unique(h, &mut units);
                }
                m.set_orientation(RotationOrder::Ypr, yaw - HALF_PI, 0.0, 0.0);
                let mut direction = m.apply_vector(GeVector { x: 0.0, y: 1.0, z: 0.0 });
                direction.normalize();
                let length = cls.inner() + 1.0;
                direction.x *= length;
                direction.y *= length;
                direction.z *= length;
                let width = Ge3DLine { start: Ge3DPoint { x: m.m[0][3], y: m.m[1][3], z: m.m[2][3] }, direction };
                let front = width.point_at(0.5);
                let back = width.point_at(-0.5);
                let across = h.tile_line(floor(front.x), floor(front.y), floor(back.x), floor(back.y), true, false);
                let mut more = Vec::new();
                units_on(h, &across, &mut more);
                units.extend_from_slice(&more);
                if !more.is_empty() && units.len() > 1 {
                    sort_unique(h, &mut units);
                }
                let mut blocked = false;
                for &u in &units {
                    if blocked {
                        break;
                    }
                    let other = UuView(h.unit(u));
                    let oc = other.class();
                    let domain = if facing_only { UU_MOVEMENT_SEA } else { UU_MOVEMENT_SPACE };
                    blocked = (other.hit_points() > 0
                        && oc.domains() & domain != 0
                        && !oc.is_decoration()
                        && oc.family() != FAMILY_FISH)
                        || (u != complex && (oc.unit_type() == TYPE_NAVY_YARD || oc.unit_type() == TYPE_DOCK));
                }
                found = !blocked;
                if found {
                    break 'search;
                }
            }
            yaw += step * 2.0;
        }
        count += 1;
        yaw = 0.0174 + yaw - TWO_PI;
    }
    while yaw > TWO_PI {
        yaw -= TWO_PI;
    }
    let next = if found {
        m.set_orientation(RotationOrder::Ypr, yaw - HALF_PI, 0.0, 0.0);
        step + yaw
    } else {
        0.0174 + step + yaw
    };
    if next != launch_yaw(&me) {
        // SAFETY: the complex unit's record, live for the call; a plain write of the member, no reference kept
        unsafe { (*me.0.cx.cast_mut()).mLaunchYaw = next };
    }
    found
}

fn launch_yaw(u: &UuView) -> f32 {
    // SAFETY: the complex unit's record
    unsafe { (*u.0.cx).mLaunchYaw }
}

/// The exit search's test: a valid space tile of the mesh that is no shore tile.
fn open_water<H: UuHost + ?Sized>(h: &mut H, x: f32, y: f32) -> bool {
    let (ux, uy) = (floor_unsigned(x), floor_unsigned(y));
    h.mesh_tile_is(0, ux, uy) && h.mesh_tile_is(1, ux, uy) && !h.shore_tile(floor(x), floor(y))
}

/// `EESortUnique(units, EEUUUnitIDGreater, EEUUUnitIDSame)`.
fn sort_unique<H: UuHost + ?Sized>(h: &mut H, units: &mut Vec<u64>) {
    let mut keyed: Vec<(u32, u64)> = units.iter().map(|&u| (UuView(h.unit(u)).id(), u)).collect();
    crate::msvc_sort::sort_by(&mut keyed, |a, b| a.0 > b.0);
    keyed.dedup_by(|b, a| a.0 == b.0);
    *units = keyed.into_iter().map(|(_, u)| u).collect();
}
