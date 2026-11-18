//! `EECPResourceCenter`'s bodies (EECPResourceCenter.cpp): `FindBuildSite`, `GetCenter`, `GetMoraleFill`,
//! `IsInAreaEffect`, `GetMinEffectRadius`, `FindResourceUnit`, `IsWithinRadius`, `AdoptDropSite`'s test, `Contains`
//! and the saved game's `Load`, `Save` and `Restore`.
//!
//! The centre stays the C++ object other files read (`mUnits`, `mUnit`, `mIsActive`, through the host as handle ids);
//! the references and the vector's growth stay C++ (`AddUnit`, `SetUnit`, `ReleaseUnit`, the destructor). A centre is
//! its `EECPResourceCenter*`.

use ee_base::random::float_to_long;

use super::file::{SaveFile, OK};
use super::place_host::{
    distance_squared, distance_to, shuffle, sort_by_key, PlaceHost, Rect, RectFilter, Tile, CP_NO_TILE,
};
use super::placement::{
    can_build_at, has_tower_near, tower_class, CP_RESOURCE_FOOD, CP_RESOURCE_GOLD, CP_RESOURCE_IRON,
    CP_TYPE_ARCHERY_RANGE, CP_TYPE_BARRACKS, CP_TYPE_CP_MISSILE_BASE, CP_TYPE_CYBER_FACTORY, CP_TYPE_CYBER_LABORATORY,
    CP_TYPE_GRANARY, CP_TYPE_HOSPITAL, CP_TYPE_HOUSE, CP_TYPE_MISSILE_BASE, CP_TYPE_PRIEST_TOWER, CP_TYPE_SETTLEMENT,
    CP_TYPE_SIEGE_FACTORY, CP_TYPE_STABLE, CP_TYPE_TANK_FACTORY, CP_TYPE_TEMPLE, CP_TYPE_UNIVERSITY,
};

/// `kAreaEffectMoraleFromBuildings` (the house's area effect kind)
/// cbindgen:ignore
pub(crate) const CP_EFFECT_MORALE_FROM_BUILDINGS: i32 = 5;
/// The save version (minor) from which a centre saves its active flag.
/// cbindgen:ignore
pub(crate) const CP_ACTIVE_FLAG_VERSION: u32 = 0x8d;

// ---- the centre operations of a saved game's file host (beyond the primitives; `index` 0)
/// `mUnits.clear()`
pub const EE_CP_CENTER_FILE_CLEAR: i32 = 200;
/// `mUnits.push_back(EEUnitHandle<EEUnit>::FromSavedID(io.i))`
pub const EE_CP_CENTER_FILE_PUSH_ID: i32 = 201;
/// `mUnit = EEUnitHandle<EEComplexUnit>::FromSavedID(io.i)`
pub const EE_CP_CENTER_FILE_SET_MAIN: i32 = 202;
/// `mIsActive = io.b`; `io.b` in: `mIsActive` (the read's old value)
pub const EE_CP_CENTER_FILE_SET_ACTIVE: i32 = 203;
pub const EE_CP_CENTER_FILE_ACTIVE: i32 = 204;

/// The centre's resource units' handles (`mUnits`, resolved in order).
fn units<H: PlaceHost + ?Sized>(h: &mut H, c: u64) -> Vec<u64> {
    let ids = h.center_units(c);
    ids.into_iter().map(|id| h.resolve(id)).collect()
}

/// `mUnit` resolved (0: none).
fn main_unit<H: PlaceHost + ?Sized>(h: &mut H, c: u64) -> u64 {
    let id = h.center_unit(c);
    h.resolve(id)
}

/// `Contains`: the centre holds the unit.
pub fn contains<H: PlaceHost + ?Sized>(h: &mut H, c: u64, unit: u64) -> bool {
    units(h, c).contains(&unit)
}

/// `AdoptDropSite`'s test: the player's drop site (not a granary) becomes the main building of a centre with none when
/// it lies within the drop-off distance of one of its units (the C++ then takes the reference).
pub fn adopt_drop_site<H: PlaceHost + ?Sized>(h: &mut H, c: u64, unit: u64) -> bool {
    let radius = h.drop_off_distance();
    let area = radius.wrapping_mul(radius);
    let i = h.unit(unit);
    if main_unit(h, c) != 0 || i.player != h.player_handle() || i.drop_site == 0 || i.unit_type == CP_TYPE_GRANARY {
        return false;
    }
    for u in units(h, c) {
        if h.distance_to_unit(unit, u) < area as f32 {
            return true;
        }
    }
    false
}

/// `GetMinEffectRadius`: the smallest area effect template radius of the class's upgrade (0: none).
pub fn min_effect_radius<H: PlaceHost + ?Sized>(h: &mut H, class: i32) -> i32 {
    let mut radius = 0;
    for t in h.effect_templates(class) {
        if t == 0 {
            continue;
        }
        let r = h.template_radius(t);
        if radius == 0 || r < radius as f32 {
            radius = float_to_long(r);
        }
    }
    radius
}

/// `GetMoraleFill`: the main building's morale effect level plus the unfinished houses in its range, over its
/// maximum level (1 without a main building or effect).
pub fn morale_fill<H: PlaceHost + ?Sized>(h: &mut H, c: u64) -> f32 {
    let mut result = 1.0f32;
    let unit = main_unit(h, c);
    if unit == 0 {
        return result;
    }
    let i = h.unit(unit);
    let half = i.tile_size / 2;
    let x = i.tile[0] + half;
    let y = i.tile[1] + half;
    if let Some((level, max, radius)) = h.morale_recount(unit) {
        let radius = float_to_long(radius);
        let rect = Rect { top: y - radius, left: x - radius, bottom: radius + y - 1, right: radius + x - 1 };
        let mut houses = Vec::new();
        h.units_in_rect(rect, RectFilter::UnfinishedHouse, &mut houses);
        result =
            if max == 0 { 1.0 } else { ((houses.len() as u64).wrapping_add(level as i64 as u64)) as f32 / max as f32 };
    }
    result
}

/// `GetCenter`: the free tile nearest the middle of the resource units (or round the single unit or main building),
/// else the first candidate tile; `out` unchanged when there is nothing to measure.
pub fn get_center<H: PlaceHost + ?Sized>(h: &mut H, c: u64, out: &mut Tile) {
    let list = units(h, c);
    let mut tiles: Vec<Tile> = Vec::new();
    if list.len() > 1 {
        let first = h.unit(list[0]).tile;
        let mut bounds = Rect::point(first);
        for &u in &list[1..] {
            let t = h.unit(u).tile;
            bounds.extend(t);
        }
        let center = bounds.center();
        let mut radius = (bounds.right - bounds.left) / 2;
        if radius < 5 {
            radius = 5;
        }
        h.tiles_in_radius(center, radius as u32, true, &mut tiles);
        sort_by_key(h, &mut tiles, |t| distance_squared(*t, center));
    } else {
        let unit = if let Some(&u) = list.first() { u } else { main_unit(h, c) };
        if unit != 0 {
            let i = h.unit(unit);
            h.square_block_ring(i.tile, i.tile_size, &mut tiles);
        }
    }
    let mut found = false;
    for &t in &tiles {
        if found {
            break;
        }
        if h.tile_free(t) == Some(true) {
            *out = t;
            found = true;
        }
    }
    if !found && !tiles.is_empty() {
        *out = tiles[0];
    }
}

/// `FindResourceUnit`: the first unit of the resource type with some left (0: none).
pub fn find_resource_unit<H: PlaceHost + ?Sized>(h: &mut H, c: u64, resource: u32) -> u64 {
    for u in units(h, c) {
        let i = h.unit(u);
        if i.resource_type as u32 == resource && i.resource_amount > 0 {
            return u;
        }
    }
    0
}

/// `IsInAreaEffect`: a tile under the class's footprint at (x, y) has an area effect of the kind for the player.
pub fn is_in_area_effect<H: PlaceHost + ?Sized>(h: &mut H, t: Tile, class: i32, kind: i32) -> bool {
    let player = h.player_id();
    let mut result = false;
    if kind != 0 && h.class_exists(class) {
        let size = h.class_tile_size(class);
        let mut x = t[0];
        while x < t[0] + size && !result {
            let mut y = t[1];
            while y < t[1] + size && !result {
                result = h.has_area_effect(kind, x, y, player);
                y += 1;
            }
            x += 1;
        }
    }
    result
}

/// `IsWithinRadius`: the unit's tile lies within the drop-off distance of the centre.
pub fn is_within_radius<H: PlaceHost + ?Sized>(h: &mut H, c: u64, unit: u64) -> bool {
    let radius = h.drop_off_distance();
    let mut center: Tile = [0, 0];
    get_center(h, c, &mut center);
    let t = h.unit(unit).tile;
    distance_squared(center, t) < radius.wrapping_mul(radius) as i64
}

/// The house's distance test of FindBuildSite: the site's centre at least the unit's size + 2 from every unit's.
fn clear_of_units<H: PlaceHost + ?Sized>(h: &mut H, list: &[u64], t: Tile, size: i32) -> bool {
    let mut found = true;
    let mut j = 0;
    while j < list.len() && found {
        let other = h.unit(list[j]);
        j += 1;
        let other_size = other.tile_size;
        let half = other_size as f32 * 0.5;
        let unit_center = [float_to_long(other.tile[0] as f32 + half), float_to_long(other.tile[1] as f32 + half)];
        let half = size as f32 * 0.5;
        let site = [float_to_long(t[0] as f32 + half), float_to_long(t[1] as f32 + half)];
        found = distance_to(site, unit_center) >= other_size + 2;
    }
    found
}

fn is_production(class: i32) -> bool {
    matches!(
        class,
        CP_TYPE_BARRACKS
            | CP_TYPE_ARCHERY_RANGE
            | CP_TYPE_SIEGE_FACTORY
            | CP_TYPE_STABLE
            | CP_TYPE_MISSILE_BASE
            | CP_TYPE_TANK_FACTORY
            | CP_TYPE_CYBER_FACTORY
            | CP_TYPE_CYBER_LABORATORY
            | CP_TYPE_CP_MISSILE_BASE
    )
}

/// `FindBuildSite`: towers by gold or iron (and a worked centre's food), houses in the main building's morale area,
/// temples, universities and hospitals clear of their effects, settlements and production buildings in rings round
/// the centre.
pub fn find_build_site<H: PlaceHost + ?Sized>(h: &mut H, c: u64, class: i32, out: &mut Tile) -> bool {
    let range = h.drop_off_distance();
    let size = h.class_tile_size(class);
    let mut found = false;
    let mut center: Tile = [0, 0];
    let epoch = h.epoch();
    let main = main_unit(h, c);
    if (class == tower_class(epoch) || class == CP_TYPE_PRIEST_TOWER) && main != 0 {
        let factor = h.range(class);
        let radius = h.tower_spacing() * factor;
        let squared = radius * radius;
        let main_info = h.unit(main);
        let continent = h.tile_continent(main_info.tile);
        let list = units(h, c);
        let active = h.center_active(c);
        let mut i = 0;
        while i < list.len() && !found {
            let u = h.unit(list[i]);
            i += 1;
            let rt = u.resource_type;
            if rt != CP_RESOURCE_GOLD && rt != CP_RESOURCE_IRON && !(rt == CP_RESOURCE_FOOD && active) {
                continue;
            }
            let mut ring = 2;
            while ring < float_to_long(factor) && !found {
                let mut tiles = h.square_ring_tiles(u.tile, ring);
                center = [main_info.tile[0] + main_info.tile_size / 2, main_info.tile[1] + main_info.tile_size / 2];
                let at = center;
                sort_by_key(h, &mut tiles, |t| distance_squared(*t, at));
                let start = if tiles.len() > 3 { 3 } else { 0 };
                for &t in &tiles[start..] {
                    if found {
                        break;
                    }
                    if has_tower_near(h, t, squared) {
                        continue;
                    }
                    found = can_build_at(h, continent, class, t, size, true, true, main_info.tile);
                    if found {
                        *out = t;
                    }
                }
                ring += 1;
            }
        }
        return found;
    }
    match class {
        CP_TYPE_SETTLEMENT => {
            get_center(h, c, &mut center);
            let continent = h.tile_continent(center);
            let mut ring = 2;
            while ring < range && !found {
                let tiles = h.square_ring_tiles(center, ring);
                for &t in &tiles {
                    if found {
                        break;
                    }
                    found = can_build_at(h, continent, CP_TYPE_SETTLEMENT, t, size, true, true, center);
                    if found {
                        *out = t;
                    }
                }
                ring += 1;
            }
        }
        CP_TYPE_HOUSE => found = find_house_site(h, c, main, size, out),
        _ if is_production(class) => {
            if main != 0 {
                if h.center_units(c).is_empty() {
                    center = h.town();
                } else {
                    get_center(h, c, &mut center);
                }
                let continent = h.tile_continent(center);
                if center[0] != CP_NO_TILE && center[1] != CP_NO_TILE {
                    let mut ring = range + 5;
                    while ring > 1 && !found {
                        let tiles = h.square_ring_tiles(center, ring);
                        for &t in &tiles {
                            if found {
                                break;
                            }
                            found = can_build_at(h, continent, class, t, size, true, true, center);
                            if found {
                                *out = t;
                            }
                        }
                        ring -= 1;
                    }
                }
            }
        }
        CP_TYPE_TEMPLE | CP_TYPE_UNIVERSITY | CP_TYPE_HOSPITAL => found = find_effect_site(h, c, class, size, out),
        _ => {}
    }
    found
}

/// FindBuildSite's houses: next to the player's houses in the main building's morale area (shuffled), else in the
/// rings inside that area.
fn find_house_site<H: PlaceHost + ?Sized>(h: &mut H, c: u64, main: u64, size: i32, out: &mut Tile) -> bool {
    let mut found = false;
    let list = units(h, c);
    let (continent, unit) =
        if !list.is_empty() { (h.unit_continent(list[0]), list[0]) } else { (h.unit_continent(main), main) };
    let unit_tile = h.unit(unit).tile;
    let main_info = h.unit(main);
    let unit_size = main_info.tile_size;
    let x = main_info.tile[0] + unit_size / 2;
    let y = main_info.tile[1] + unit_size / 2;
    let effect_radius = match h.morale_radius(main) {
        Some(r) => float_to_long(r),
        None => 0,
    };
    let mut tiles: Vec<Tile> = Vec::new();
    if morale_fill(h, c) > 0.0 {
        let rect = Rect {
            top: y - effect_radius,
            bottom: effect_radius + y - 1,
            left: x - effect_radius,
            right: effect_radius + x - 1,
        };
        let mut houses = Vec::new();
        h.units_in_rect(rect, RectFilter::LiveHouse, &mut houses);
        for b in houses {
            let bt = h.unit(b).tile;
            for site in [[bt[0] - size, bt[1]], [bt[0] + size, bt[1]], [bt[0], bt[1] - size], [bt[0], bt[1] + size]] {
                if is_in_area_effect(h, site, CP_TYPE_HOUSE, CP_EFFECT_MORALE_FROM_BUILDINGS) {
                    tiles.push(site);
                }
            }
        }
    }
    if !tiles.is_empty() {
        shuffle(h, &mut tiles);
    }
    for &t in &tiles {
        if found {
            break;
        }
        found = can_build_at(h, continent, CP_TYPE_HOUSE, t, size, false, true, [-1, -1]);
        if !found {
            continue;
        }
        found = clear_of_units(h, &list, t, size);
        if found {
            *out = t;
        }
    }
    if !found {
        let mut ring = effect_radius - 2;
        while ring > unit_size && !found {
            let tiles = h.square_ring_tiles([x, y], ring);
            for &t in &tiles {
                if found {
                    break;
                }
                found = can_build_at(h, continent, CP_TYPE_HOUSE, t, size, false, true, unit_tile);
                if !found {
                    continue;
                }
                found = clear_of_units(h, &list, t, size);
                if found {
                    *out = t;
                }
            }
            ring -= 1;
        }
    }
    found
}

/// FindBuildSite's temples, universities and hospitals: when none of the class (or the paired one) is being built
/// in the effect's radius of the centre, square rings round its units (those in fewer effects of the kind first;
/// the centre's units stay in that order), shuffled, clear of the kinds' effects.
fn find_effect_site<H: PlaceHost + ?Sized>(h: &mut H, c: u64, class: i32, size: i32, out: &mut Tile) -> bool {
    let mut found = false;
    let mut center: Tile = [0, 0];
    get_center(h, c, &mut center);
    let continent = h.tile_continent(center);
    let (kind, kind2, other_class) = if class == CP_TYPE_HOSPITAL {
        (1, 0, 0)
    } else if class == CP_TYPE_TEMPLE {
        (3, 4, 0x3c)
    } else {
        (4, 3, 0x27)
    };
    let player = h.player_id();
    let mut building = Vec::new();
    h.units_of_type(class, player, &mut building);
    h.units_of_type(other_class, player, &mut building);
    let mut kept = Vec::with_capacity(building.len());
    for u in building {
        let i = h.unit(u);
        if i.has_tile_node == 0 || h.unit(i.complex).complex_built != 0 {
            continue;
        }
        kept.push(i.tile);
    }
    let mut radius = min_effect_radius(h, class);
    if class == CP_TYPE_HOSPITAL {
        radius *= 3;
    }
    let mut tiles: Vec<Tile> = Vec::new();
    h.tiles_in_radius(center, radius as u32, true, &mut tiles);
    if kept.iter().any(|t| tiles.contains(t)) {
        return false;
    }
    // EECPRCAreaEffectLess: the units without an area effect of the kind on their tile first (the C++ std::sort of
    // mUnits, which keeps the order it leaves)
    let ids = h.center_units(c);
    let mut keyed: Vec<(u32, i64)> = Vec::with_capacity(ids.len());
    for &id in &ids {
        let u = h.resolve(id);
        let t = h.unit(u).tile;
        keyed.push((id, i64::from(h.has_area_effect(kind, t[0], t[1], player))));
    }
    sort_by_key(h, &mut keyed, |e| e.1);
    let sorted: Vec<u32> = keyed.iter().map(|e| e.0).collect();
    if sorted != ids {
        h.center_set_order(c, &sorted);
    }
    let list: Vec<u64> = sorted.iter().map(|&id| h.resolve(id)).collect();
    let mut i = 0;
    while i < list.len() && !found {
        let other = h.unit(list[i]);
        i += 1;
        let (x, y) = (other.tile[0], other.tile[1]);
        let other_size = other.tile_size;
        let mut blocked = false;
        let mut blocked2 = false;
        let mut k = 0;
        while k < radius && !found && !blocked && !blocked2 {
            let side = other_size + size + 2 * k;
            let mut ring = h.square_ring([x - k - size, y - k - size], side, side);
            shuffle(h, &mut ring);
            let mut m = 0;
            while m < ring.len() && !found && !blocked && !blocked2 {
                let t = ring[m];
                m += 1;
                blocked = is_in_area_effect(h, t, class, kind);
                blocked2 = is_in_area_effect(h, t, class, kind2);
                if !blocked && !blocked2 && can_build_at(h, continent, class, t, size, true, true, center) {
                    *out = t;
                    found = true;
                } else {
                    found = false;
                }
            }
            k += 1;
        }
    }
    found
}

// ---- the saved game

/// `Load`: the resource units' ids and the main building's (resolved by [`restore`]) and, from version 0.141, the
/// active flag; the first read error.
pub fn load(f: &mut SaveFile, minor: u32) -> i32 {
    let (mut r, count) = f.read_ulong(0);
    if r != OK {
        return r;
    }
    f.call(EE_CP_CENTER_FILE_CLEAR, &mut Default::default());
    let mut id = 0u32;
    for _ in 0..count {
        let (read, value) = f.read_ulong(id);
        r = read;
        id = value;
        if r == OK {
            let mut io = crate::units::player_save::EeUnitsPlayerSaveIo { i: i64::from(id), ..Default::default() };
            f.call(EE_CP_CENTER_FILE_PUSH_ID, &mut io);
        }
    }
    if r == OK {
        let (read, value) = f.read_ulong(id);
        r = read;
        if r == OK {
            let mut io = crate::units::player_save::EeUnitsPlayerSaveIo { i: i64::from(value), ..Default::default() };
            f.call(EE_CP_CENTER_FILE_SET_MAIN, &mut io);
            if minor >= CP_ACTIVE_FLAG_VERSION {
                let mut io = crate::units::player_save::EeUnitsPlayerSaveIo::default();
                f.call(EE_CP_CENTER_FILE_ACTIVE, &mut io);
                let (read, b) = f.read_bool(io.b != 0);
                r = read;
                let mut io = crate::units::player_save::EeUnitsPlayerSaveIo { b: u8::from(b), ..Default::default() };
                f.call(EE_CP_CENTER_FILE_SET_ACTIVE, &mut io);
            }
        }
    }
    r
}

/// `Save`: the ids of the valid resource units, the main building's id (-1: none) and the active flag; the first
/// write error.
pub fn save<H: PlaceHost + ?Sized>(h: &mut H, f: &mut SaveFile, c: u64) -> i32 {
    let list = units(h, c);
    let infos: Vec<_> = list.iter().map(|&u| h.unit(u)).collect();
    let count = infos.iter().filter(|i| i.valid != 0).count() as u32;
    let mut r = f.write_ulong(count);
    if r != 0 {
        return r;
    }
    let mut i = 0;
    while i < infos.len() && r == 0 {
        if infos[i].valid != 0 {
            r = f.write_ulong(infos[i].id);
        }
        i += 1;
    }
    if r == 0 {
        let main = main_unit(h, c);
        let id = if main != 0 && h.unit(main).valid != 0 { h.unit(main).id } else { u32::MAX };
        r = f.write_ulong(id);
        if r == 0 {
            let active = h.center_active(c);
            r = f.write_bool(active);
        }
    }
    r
}

/// `Restore`: the loaded ids turned back into units (missing and duplicate ones dropped) and the main building
/// referenced; 3 when the main building no longer exists.
pub fn restore<H: PlaceHost + ?Sized>(h: &mut H, c: u64) -> i32 {
    let ids = h.center_units(c);
    h.center_clear_units(c);
    let mut added: Vec<u64> = Vec::new();
    for id in ids {
        if !h.has_player0() {
            continue;
        }
        let u = h.world_unit(id as i32 as u32);
        if u != 0 && !added.contains(&u) {
            h.center_add_unit(c, u);
            added.push(u);
        }
    }
    let main = h.center_unit(c);
    h.center_restore_main(c, main)
}
