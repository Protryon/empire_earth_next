//! The unit utilities' spot searches (the unit utilities lane, round 1, third part): where a gatherer, builder or
//! attacker stands next to its target: the open spot on the ring around a moving target, the resource and next free
//! tiles of a still one, the wall's near side. Their world is [`UuHost`]'s; the tile tests are `uu`'s.

// the C++'s negated float comparisons kept as written (a NaN); `c_long` casts needed on LP64
#![allow(clippy::neg_cmp_op_on_partial_ord, clippy::unnecessary_cast)]
use crate::units::uu::k::UU_CATEGORY_NAVAL_CITIZEN;
use crate::units::uu::k::UU_MOVEMENT_SEA;
use crate::units::uu::{can_stand_on, pick_tile_near, tiles_around, units_on_tiles, UuHost, UuView};
use crate::units::uu_tiles::{tiles_near_point, Tile};
use ee_render::ge::types::{Ge2DPoint, Ge3DPlane, Ge3DPoint};
use ee_render::ge::um::floor;

/// `EEGetUnitsNearTile` (0x24c003): the units on the tiles within `radius` of the point whose position (a complex
/// unit's location, else its tile's centre) lies within it too.
pub fn units_near_tile<H: UuHost + ?Sized>(h: &mut H, center: [f32; 3], radius: f32) -> Vec<u64> {
    let squared = radius * radius;
    let mut tiles = Vec::new();
    tiles_near_point(center[0], center[1], radius, &mut tiles);
    let mut units = Vec::new();
    units_on_tiles(h, &tiles, &mut units);
    let c = Ge2DPoint { x: center[0], y: center[1] };
    units.retain(|&u| {
        let unit = UuView(h.unit(u));
        let p = if unit.0.complex == 0 {
            Ge2DPoint { x: unit.tile_x() as f32 + 0.5, y: unit.tile_y() as f32 + 0.5 }
        } else {
            let at = UuView(h.unit(unit.0.complex)).0.location;
            Ge2DPoint { x: at[0], y: at[1] }
        };
        p.distance_to_squared(c) <= squared
    });
    units
}

/// `EEComplexUnit::DistanceSquaredTo`: the squared ground distance between two locations, the y difference first.
fn distance_squared(a: [f32; 3], b: [f32; 3]) -> f32 {
    let dy = a[1] - b[1];
    let dx = a[0] - b[0];
    dx * dx + dy * dy
}

/// `EEGetAttackPart` (0x246f93): of the target's linked pieces (from the first), the nearest the unit may attack: the
/// first piece when it is not left out, then any later piece not left out that is nearer (the first later one always
/// takes it).
pub fn attack_part<H: UuHost + ?Sized>(h: &mut H, unit: u64, target: u64) -> u64 {
    let me = UuView(h.unit(unit));
    let at = UuView(h.unit(me.0.complex)).0.location;
    let mut piece = target;
    loop {
        let prev = h.part(piece, false);
        if prev == 0 {
            break;
        }
        piece = prev;
    }
    let mut result = 0;
    let mut best = 0.0f32;
    let mut found = false;
    if !h.part_left_out(piece) {
        best = distance_squared(at, UuView(h.unit(piece)).0.location);
        result = piece;
    }
    loop {
        let next = h.part(piece, true);
        if next == 0 {
            break;
        }
        piece = next;
        if h.part_left_out(piece) {
            continue;
        }
        let d = distance_squared(at, UuView(h.unit(piece)).0.location);
        if found && !(best > d) {
            continue;
        }
        best = d;
        result = piece;
        found = true;
    }
    result
}

/// `EEFindOpenSpot` (0x2481eb): of the eight points around the target at the two radii' sum (the four sides, then the
/// four corners), nearest the unit first (MSVC's `std::sort`: an insertion sort of eight), the first the unit can stand
/// on clear of the trees on its tile and of the living blockers near the target; the target's own point when none is.
/// `out` gets the spot's x and y (the target's point, all three).
pub fn find_open_spot<H: UuHost + ?Sized>(h: &mut H, unit: u64, target: u64, out: &mut [f32; 3]) -> bool {
    let me = UuView(h.unit(unit));
    let other = UuView(h.unit(target));
    let target_radius = other.class().inner();
    let radius = me.class().inner();
    let at = h.drop_point(target);
    let (x, y) = (at[0], at[1]);
    let half = radius * 0.5;
    let target_half = target_radius * 0.5;
    let distance = target_half + half;
    let mut points =
        vec![[x, distance + y], [distance + x, y], [x, y - target_half - half], [x - target_half - half, y]];
    let right = distance + x;
    let top = distance + y;
    points.push([right, top]);
    let bottom = y - distance;
    points.push([right, bottom]);
    let left = x - distance;
    points.push([left, bottom]);
    points.push([left, top]);
    let location = UuView(h.unit(me.0.complex)).0.location;
    let near = |p: &[f32; 2]| (location[0] - p[0]) * (location[0] - p[0]) + (location[1] - p[1]) * (location[1] - p[1]);
    crate::units::msvc_sort::sort_by(&mut points, |a, b| near(a) < near(b));
    let units = units_near_tile(h, at, distance + 2.0);
    let mut i = 0;
    let mut free;
    loop {
        let p = points[i];
        if me.0.active_group != 0 {
            // (the C++ asks and drops the answer)
            h.reserved(me.0.active_group, [p[0], p[1], 0.0]);
        }
        let tile: Tile = [crate::units::uu_tiles::trunc_long(p[0]), crate::units::uu_tiles::trunc_long(p[1])];
        free = can_stand_on(h, tile, unit);
        for &u in &units {
            if !free {
                break;
            }
            let o = UuView(h.unit(u));
            let oc = o.class();
            if oc.is_tree() && o.tile_x() == floor(p[0]) && o.tile_y() == floor(p[1]) {
                free = false;
            } else if o.hit_points() > 0 && (oc.blocks_tile() || oc.is_collidable()) {
                let q = h.position(u);
                let squared = Ge2DPoint { x: p[0], y: p[1] }.distance_to_squared(Ge2DPoint { x: q[0], y: q[1] });
                let reach = (oc.inner() + radius) * 0.5;
                free = !(squared + 0.1 * radius < reach * reach);
            }
        }
        if !free {
            i += 1;
        }
        if !(i < points.len() && !free) {
            break;
        }
    }
    if free {
        out[0] = points[i][0];
        out[1] = points[i][1];
    } else {
        *out = at;
    }
    free
}

/// `EEFindResourceSpot` (0x248709): a moving target's open spot; else of the tiles around it (its own first), those in
/// the unit's path area first (a stable sort), the tiles of another continent dropped (kept apart when connected, and
/// used when no other is left; not for a naval citizen), the first free one ([`pick_tile_near`]).
pub fn find_resource_spot<H: UuHost + ?Sized>(h: &mut H, unit: u64, target: u64, out: &mut [f32; 3]) -> bool {
    let other = UuView(h.unit(target));
    if other.0.stats_max_velocity > 0.0 {
        return find_open_spot(h, unit, target, out);
    }
    let me = UuView(h.unit(unit));
    let size = other.class().tile_size();
    let mut tiles = tiles_around(h, unit, target, other.tile_x(), other.tile_y(), size, 1, 0, true);
    let area = h.path_area_id(me.tile_x(), me.tile_y());
    let mut keyed: Vec<(bool, Tile)> = tiles.iter().map(|&t| (h.path_area_id(t[0], t[1]) != area, t)).collect();
    // std::stable_sort by "in the area, the other not": the area's tiles first, each part in order
    keyed.sort_by_key(|k| k.0);
    tiles = keyed.into_iter().map(|k| k.1).collect();
    if me.class().category() != UU_CATEGORY_NAVAL_CITIZEN {
        let kind = if me.class().domains() & UU_MOVEMENT_SEA != 0 { 1 } else { 2 };
        let continent = h.continent_of(me.tile_x(), me.tile_y(), kind);
        let mut others = Vec::new();
        let mut i = tiles.len() as i64 - 1;
        while i >= 0 {
            let t = tiles[i as usize];
            if continent == 0 || continent == h.continent_of(t[0], t[1], kind) {
                i -= 1;
                continue;
            }
            let from = h.continent_at(me.tile_x(), me.tile_y());
            let to = h.continent_at(t[0], t[1]);
            if h.are_connected_for_unit(unit, to, from) {
                others.push(t);
            }
            tiles.remove(i as usize);
            i -= 1;
        }
        if tiles.is_empty() && !others.is_empty() {
            tiles = others;
        }
    }
    if tiles.is_empty() {
        return false;
    }
    pick_tile_near(h, &tiles, unit, target, false, out)
}

/// `EEFindNextFreeTile` (0x24899b): a free tile around the target (the nearest piece of a linked target, its own ring
/// through the vtable for the first piece).
pub fn find_next_free_tile<H: UuHost + ?Sized>(h: &mut H, unit: u64, target: u64, out: &mut [f32; 3]) -> bool {
    let mut piece = target;
    let mut outer = true;
    if h.part(piece, true) != 0 {
        piece = attack_part(h, unit, piece);
        outer = h.part(piece, false) == 0;
    }
    let p = UuView(h.unit(piece));
    let tiles = tiles_around(h, unit, piece, p.tile_x(), p.tile_y(), p.class().tile_size(), 1, 0, outer);
    pick_tile_near(h, &tiles, unit, piece, false, out)
}

/// `EEGetWallSideTiles` (0x248a15): the tiles on the unit's side of the target's wall (or gate) line: the plane
/// through the line's ends and a point 10 above the last (a wall's line from its tile, extended, when longer than
/// three tiles).
pub fn wall_side_tiles<H: UuHost + ?Sized>(h: &mut H, unit: u64, target: u64, tiles: &mut Vec<Tile>) {
    let t = UuView(h.unit(target));
    let cls = t.class();
    let gate = cls.category() == crate::units::uu::k::UU_CATEGORY_GATE;
    if !gate && !cls.is_wall() {
        return;
    }
    let (first, second) = if gate {
        let ends = h.gate_ends(target);
        (ends[0], ends[1])
    } else {
        let mut line = Vec::new();
        crate::units::uu::extend_wall_line(h, &[[t.tile_x(), t.tile_y()]], t.0.player, cls.unit_type(), &mut line);
        if line.len() <= 3 {
            return;
        }
        let (a, b) = (line[0], line[line.len() - 1]);
        ([a[0] as f32, a[1] as f32, 0.0], [b[0] as f32, b[1] as f32, 0.0])
    };
    let p = |v: [f32; 3]| Ge3DPoint { x: v[0], y: v[1], z: v[2] };
    let third = Ge3DPoint { x: second[0], y: second[1], z: second[2] + 10.0 };
    let plane = Ge3DPlane::through(p(first), p(second), third);
    let side = plane.is_point_on_or_above(p(h.position(unit)));
    tiles.retain(|t| plane.is_point_on_or_above(Ge3DPoint { x: t[0] as f32, y: t[1] as f32, z: 0.0 }) == side);
}

/// `EEGetWallSpot` (0x248c7e): a still target's tiles on the unit's side of its wall, the first free one; a moving
/// target's open spot.
pub fn wall_spot<H: UuHost + ?Sized>(h: &mut H, unit: u64, target: u64, out: &mut [f32; 3]) -> bool {
    let t = UuView(h.unit(target));
    if t.0.stats_max_velocity <= 0.0 {
        let mut tiles = tiles_around(h, unit, target, t.tile_x(), t.tile_y(), t.class().tile_size(), 1, 0, false);
        wall_side_tiles(h, unit, t.0.complex, &mut tiles);
        pick_tile_near(h, &tiles, unit, target, false, out)
    } else {
        find_open_spot(h, unit, target, out)
    }
}
