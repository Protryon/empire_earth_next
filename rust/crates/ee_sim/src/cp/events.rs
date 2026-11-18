//! `EEComputerPlayer`'s own bodies (EEComputerPlayer.cpp; AI lane B, round 2): `WorkOutPopulation`, the resource
//! centres' grouping (`GetObjectRegion`, `GetObjectsToGroup`, `Regroup`, `UpdateGroups`), the favourite types'
//! choice (the four `ChooseFavorite*Type` and `GetScreenedTypesByLine`), the missing favourites of a load,
//! `ChooseCivilization`'s pick, `EECPRetireObsoleteUnits`, `EECPPendingEpoch` and `AvoidsEnemies`.
//!
//! The world is the placement's ([`PlaceHost`]) and the player's ([`PlayerHost`]). The centres' own methods are the C++
//! forwarders (`EECPResourceCenter::IsWithinRadius` and the rest, with their TEST ONLY PLCE scopes), so a call here
//! makes the same placement records as the original; the centres' sort runs the original's `std::sort` with its
//! comparator (it asks each centre's `GetCenter` per comparison).

// `c_ulong` is 32 bits on Windows and 64 elsewhere: the casts of the favourites are needed there
#![allow(clippy::unnecessary_cast)]

use std::collections::BTreeMap;

use ee_base::random::float_to_long;

use super::fields::EeCpFields;
use super::place_host::{distance_squared, PlaceHost, Tile, CP_NO_TILE};
use super::placement::{CP_MAP_ISLANDS, CP_MAP_PLANETS, CP_RESOURCE_GOLD, CP_RESOURCE_IRON, CP_TYPE_CAPITOL};

/// cbindgen:ignore
pub(crate) mod names {
    pub(crate) const CP_RESOURCE_STONE: i32 = 4;
    pub(crate) const CP_TYPE_FORAGE: i32 = 0x37;
    pub(crate) const CP_TYPE_FISH: i32 = 0x13b;
    pub(crate) const CP_AI_KIND_NONE: i32 = 0;
    pub(crate) const CP_TECH_NONE: i32 = 100000;
    pub(crate) const CP_CATEGORY_LAND_CITIZEN: i32 = 6;
    pub(crate) const CP_CATEGORY_NAVAL_CITIZEN: i32 = 0x16;
    pub(crate) const CP_CHEAT_POP_BAND: i32 = 16;
    pub(crate) const CP_CHEAT_RETIRE: i32 = 64;
    /// gCPBehaviors' population cap weights (`kCPBehaviorPopulationCapOtherWeight`, `..OwnWeight`)
    pub(crate) const CP_BEHAVIOR_POP_OTHER: i32 = 92;
    pub(crate) const CP_BEHAVIOR_POP_OWN: i32 = 93;
    /// the epochs (`kTechEpoch*`)
    pub(crate) const CP_EPOCH_BRONZE: i32 = 3;
    pub(crate) const CP_EPOCH_DARK: i32 = 4;
    pub(crate) const CP_EPOCH_MIDDLE: i32 = 5;
    pub(crate) const CP_EPOCH_INDUSTRIAL: i32 = 8;
    pub(crate) const CP_EPOCH_WW1: i32 = 9;
    pub(crate) const CP_EPOCH_MODERN: i32 = 11;
    pub(crate) const CP_EPOCH_DIGITAL: i32 = 12;
    pub(crate) const CP_EPOCH_SPACE: i32 = 14;
    /// the obsolete unit types the retire cheat kills
    pub(crate) const CP_RETIRED_TYPES: [i32; 17] =
        [0xb, 0xe, 0x10, 0x11, 0x19, 0x36, 0x40, 0x60, 0x69, 0x6b, 0x8b, 0x8e, 0x90, 0x135, 0x137, 0x138, 0x170];
}
pub(crate) use names::*;

/// The kinds of favourite (`ChooseFavoriteLandType` .. `ChooseFavoriteSpaceType`).
pub const EE_CP_FAVORITE_LAND: i32 = 0;
pub const EE_CP_FAVORITE_SEA: i32 = 1;
pub const EE_CP_FAVORITE_AIR: i32 = 2;
pub const EE_CP_FAVORITE_SPACE: i32 = 3;

/// The player's world beyond the placement's.
pub trait PlayerHost: PlaceHost {
    /// the computer player's record
    fn fields(&mut self) -> *mut EeCpFields;
    /// `gCPBehaviors[i].mFloatValue`
    fn behavior_float(&mut self, i: i32) -> f32;
    /// `EEPlayer::GetPopulationLimit(0)`, `IsDeathmatch()`, `EECPCheatOn(player, cheat)`
    fn base_population_limit(&mut self) -> i32;
    fn is_deathmatch(&mut self) -> bool;
    fn cheat_on(&mut self, cheat: i32) -> bool;
    /// `gWorld.GetNumTeams()`, team `i`'s `mVictoryCondition->HasEnoughWonders()`, `ContainsPlayer(id)`
    fn num_teams(&mut self) -> u32;
    fn team_has_wonders(&mut self, team: u32) -> bool;
    fn team_contains(&mut self, team: u32, player: u32) -> bool;
    /// `gWorld.GetNumPlayers()`; player `i`'s `IsHuman()` (None: no player), `GetTotalPopulation(true, true)`, the
    /// player (`gWorld.GetPlayer(i)`)
    fn num_players(&mut self) -> u32;
    /// `gWorld.mNumPlayers`
    fn num_world_players(&mut self) -> u32;
    fn player_human(&mut self, i: u32) -> Option<bool>;
    fn player_population(&mut self, i: u32) -> i32;
    fn player_handle_at(&mut self, i: u32) -> u64;
    /// the id of `GetPathAreaAt(tile)` (0: none); `GetContinentAt(unit)`
    fn path_area_id(&mut self, t: Tile) -> u16;
    fn unit_continent_at(&mut self, unit: u64) -> u64;
    /// a new centre (`new EECPResourceCenter(player, unit)`, not listed yet); listed at the end or the front
    fn center_create(&mut self, unit: u64) -> u64;
    fn center_list(&mut self, center: u64, front: bool);
    /// the C++ forwarders of a centre (their PLCE scopes)
    fn center_is_within(&mut self, center: u64, unit: u64) -> bool;
    fn center_add(&mut self, center: u64, unit: u64);
    fn center_contains(&mut self, center: u64, unit: u64) -> bool;
    fn center_adopt(&mut self, center: u64, unit: u64);
    fn center_set_main(&mut self, center: u64, unit: u64);
    fn center_get_center(&mut self, center: u64, tile: &mut Tile);
    fn center_set_active(&mut self, center: u64, active: bool);
    /// `std::sort(mResourceCenters, EECPCenterDistanceLess(tile))`
    fn centers_sort(&mut self, from: Tile);
    /// class `i` for a favourite of `kind` in `epoch`: `gUnitClassTable[i] != NULL && mIsEnabled` and the favourite
    /// screener's `Screen`; Some((mAIKind, mUnitType)) when it passes
    fn favorite_candidate(&mut self, class: u32, kind: i32, epoch: i32) -> Option<(i32, i32)>;
    /// `mFavoriteTypeHistory.push_back(make_pair(type, epoch))`
    fn history_push(&mut self, unit_type: u32, epoch: i32);
    /// `EEIsEpochID(id)`
    fn is_epoch(&mut self, id: i32) -> bool;
    /// complex unit's `mUnitClass->IsBuilding()`; its `mCurrentResearch` then its build queue's technologies
    fn unit_is_building(&mut self, unit: u64) -> bool;
    fn unit_research(&mut self, unit: u64) -> Vec<i32>;
    /// `GetTechTreeNode() != NULL && GetIsObsolete()`; `EEIsSeenBy(unit, player, true)`; `Kill(true, false, false)`
    fn unit_obsolete(&mut self, unit: u64) -> bool;
    fn seen_by(&mut self, unit: u64, player: u64) -> bool;
    fn kill(&mut self, unit: u64);
    /// `mUnitClass->IsTransport()`; the cargo
    fn is_transport(&mut self, unit: u64) -> bool;
    fn cargo(&mut self, unit: u64) -> Vec<u64>;
}

fn rec<H: PlayerHost + ?Sized>(h: &mut H) -> EeCpFields {
    // SAFETY: the host's record is live; copied, never held across a call
    unsafe { *h.fields() }
}

fn rec_mut<H: PlayerHost + ?Sized>(h: &mut H, f: impl FnOnce(&mut EeCpFields)) {
    // SAFETY: as above
    unsafe { f(&mut *h.fields()) }
}

/// `WorkOutPopulation`: the cap is the own limit when another team wins by wonders or in a deathmatch, else the
/// weighted mean of the own limit and the largest human population (capped by the own limit), scaled by difficulty.
pub fn work_out_population<H: PlayerHost + ?Sized>(h: &mut H) {
    let other_weight = h.behavior_float(CP_BEHAVIOR_POP_OTHER);
    let own_weight = h.behavior_float(CP_BEHAVIOR_POP_OWN);
    let own = h.base_population_limit();
    let mut found = false;
    let mut max = 0;
    let deathmatch = h.is_deathmatch();
    if rec(h).mIgnoreOthers {
        return;
    }
    let mut i = 0;
    while i < h.num_teams() {
        if found {
            break;
        }
        let id = h.player_id();
        found = h.team_has_wonders(i) && !h.team_contains(i, id);
        i += 1;
    }
    let cap = if !found && !deathmatch {
        let mut j = 1;
        while j < h.num_players() {
            if h.player_human(j) == Some(true) {
                let mut count = h.player_population(j);
                if count >= own {
                    count = own;
                }
                if max <= count {
                    max = count;
                }
            }
            j += 1;
        }
        if !h.cheat_on(CP_CHEAT_POP_BAND) {
            max = own;
        }
        let mut cap =
            float_to_long((own as f32 * own_weight + max as f32 * other_weight) / (own_weight + other_weight));
        let adjustment = rec(h).pop_limit_adjustment();
        cap = float_to_long(adjustment * cap as f32);
        if cap > own {
            cap = own;
        }
        cap
    } else {
        own
    };
    rec_mut(h, |f| f.mPopulationCap = cap as _);
}

/// `GetObjectRegion`: the most frequent path area id round the unit (the lowest id of the most frequent; 0: none).
pub fn object_region<H: PlayerHost + ?Sized>(h: &mut H, unit: u64) -> u16 {
    let t = h.unit(unit).tile;
    let tiles = h.square_ring_tiles(t, 1);
    let mut counts: BTreeMap<u16, u32> = BTreeMap::new();
    for t in tiles {
        let id = h.path_area_id(t);
        if id != 0 {
            *counts.entry(id).or_insert(0) += 1;
        }
    }
    let mut best: Option<(u16, u32)> = None;
    for (&id, &n) in &counts {
        match best {
            Some((_, m)) if n <= m => {}
            _ => best = Some((id, n)),
        }
    }
    best.map_or(0, |b| b.0)
}

/// `GetObjectsToGroup`: nature's stone, gold, iron and forage reachable from the town centre (all on island and planet
/// maps), and the number of fish.
pub fn objects_to_group<H: PlayerHost + ?Sized>(h: &mut H) -> (Vec<u64>, i32) {
    let any = h.has_map() && {
        let flags = h.map_flags();
        flags & CP_MAP_ISLANDS == CP_MAP_ISLANDS || flags & CP_MAP_PLANETS == CP_MAP_PLANETS
    };
    let town = h.town();
    let mut out = Vec::new();
    let mut count = 0;
    for u in h.nature_units().unwrap_or_default() {
        let i = h.unit(u);
        if i.resource_amount <= 0 {
            continue;
        }
        let rt = i.resource_type;
        if rt == CP_RESOURCE_STONE || rt == CP_RESOURCE_GOLD || rt == CP_RESOURCE_IRON || i.unit_type == CP_TYPE_FORAGE
        {
            if any {
                out.push(u);
                continue;
            }
            let id = h.path_area_id(town);
            let near = id == object_region(h, u) || {
                let from = h.continent_at(town);
                let to = h.unit_continent_at(u);
                let flags = h.passability();
                h.are_connected(from, to, flags)
            };
            if near {
                out.push(u);
            }
        } else if i.unit_type == CP_TYPE_FISH {
            count += 1;
        }
    }
    (out, count)
}

/// `Regroup`: the resource centres rebuilt round the nature resources, sorted by distance from the town centre, the
/// first capitol's centre first, those in range (and connected on island and planet maps) active; whether the player
/// fishes.
pub fn regroup<H: PlayerHost + ?Sized>(h: &mut H) {
    let range = h.drop_off_distance();
    let has_map = h.has_map();
    let flags = if has_map { h.map_flags() } else { 0 };
    let water = has_map && (flags & CP_MAP_ISLANDS == CP_MAP_ISLANDS || flags & CP_MAP_PLANETS == CP_MAP_PLANETS);
    let town = h.town();
    if !h.has_player0() || town[0] == CP_NO_TILE || town[1] == CP_NO_TILE {
        return;
    }
    let (units, count) = objects_to_group(h);
    for u in units {
        let mut found = false;
        for c in h.centers() {
            if found {
                break;
            }
            if h.center_is_within(c, u) {
                h.center_add(c, u);
                found = true;
            }
        }
        if !found {
            let c = h.center_create(u);
            h.center_list(c, false);
        }
    }
    h.centers_sort(town);
    let mut capitol = 0;
    for u in h.own_units() {
        let i = h.unit(u);
        if i.unit_type == CP_TYPE_CAPITOL && i.complex != 0 && i.hit_points > 0 {
            capitol = i.complex;
            break;
        }
    }
    if capitol != 0 {
        let centers = h.centers();
        let mut new = centers.is_empty();
        if !new {
            h.center_adopt(centers[0], capitol);
            let main = h.center_unit(centers[0]);
            new = h.resolve(main) != capitol;
        }
        if new {
            let c = h.center_create(0);
            h.center_set_main(c, capitol);
            h.center_list(c, true);
        }
    }
    let squared = range.wrapping_mul(range) as i64;
    let mut center: Tile = [0, 0];
    for c in h.centers() {
        h.center_get_center(c, &mut center);
        let active = distance_squared(town, center) < squared.wrapping_mul(36)
            && (!water || {
                let a = h.continent_at(center);
                let b = h.continent_at(town);
                let flags = h.passability();
                h.are_connected(a, b, flags)
            });
        h.center_set_active(c, active);
    }
    let fishing = count >= 10 && has_map && flags & CP_MAP_ISLANDS == CP_MAP_ISLANDS;
    rec_mut(h, |f| f.mUsesFishing = fishing);
}

/// `UpdateGroups`: the ungrouped resources into the centres in whose radius they lie, new inactive centres for the
/// rest; with a new centre the centres sorted again, those before the first active one and the gaps between active
/// ones activated.
pub fn update_groups<H: PlayerHost + ?Sized>(h: &mut H) {
    let town = h.town();
    if town[0] == CP_NO_TILE || town[1] == CP_NO_TILE {
        return;
    }
    let (mut units, _) = objects_to_group(h);
    let mut kept = Vec::with_capacity(units.len());
    for u in units {
        let mut found = false;
        for c in h.centers() {
            if found {
                break;
            }
            found = h.center_contains(c, u);
        }
        if !found {
            kept.push(u);
        }
    }
    units = kept;
    let mut kept = Vec::with_capacity(units.len());
    for u in units {
        let mut found = false;
        for c in h.centers() {
            if found {
                break;
            }
            if h.center_is_within(c, u) {
                h.center_add(c, u);
                found = true;
            }
        }
        if !found {
            kept.push(u);
        }
    }
    units = kept;
    let mut created = false;
    while let Some(first) = units.pop() {
        let c = h.center_create(first);
        created = true;
        let n = h.centers().len();
        let mut kept = Vec::with_capacity(units.len());
        for u in units {
            // (the original tested the new centre once per listed centre, so never with no centre listed)
            let mut found = false;
            let mut i = 0;
            while i < n && !found {
                if h.center_is_within(c, u) {
                    h.center_add(c, u);
                    found = true;
                }
                i += 1;
            }
            if !found {
                kept.push(u);
            }
        }
        units = kept;
        h.center_set_active(c, false);
        h.center_list(c, false);
    }
    if !created {
        return;
    }
    h.centers_sort(town);
    let centers = h.centers();
    let first_active = centers.iter().position(|&c| h.center_active(c));
    if let Some(found) = first_active {
        if found != 0 {
            for &c in &centers[..found] {
                h.center_set_active(c, true);
            }
        }
    }
    if centers.len() >= 2 {
        for k in 1..centers.len() - 1 {
            if !h.center_active(centers[k]) && h.center_active(centers[k - 1]) && h.center_active(centers[k + 1]) {
                h.center_set_active(centers[k], true);
            }
        }
    }
}

/// `ChooseFavorite*Type`: a random unit type of a random line (AI kind) of the epoch's favourites of the kind, then the
/// history's entry (the type as it is when none passes).
pub fn choose_favorite<H: PlayerHost + ?Sized>(h: &mut H, kind: i32) {
    let epoch = h.epoch();
    let mut lines: BTreeMap<i32, Vec<i32>> = BTreeMap::new();
    for i in 0..h.class_count() {
        let Some((ai_kind, unit_type)) = h.favorite_candidate(i, kind, epoch) else {
            continue;
        };
        if let Some(list) = lines.get_mut(&ai_kind) {
            list.push(unit_type);
        } else if ai_kind != CP_AI_KIND_NONE {
            lines.insert(ai_kind, vec![unit_type]);
        }
    }
    if !lines.is_empty() {
        let index = h.random(lines.len() as i32 - 1);
        let line = lines.values().nth(index as usize).cloned().unwrap_or_default();
        let r = h.random(line.len() as i32 - 1);
        let chosen = line[r as usize] as u32 as _;
        rec_mut(h, |f| match kind {
            EE_CP_FAVORITE_LAND => f.mFavoriteLandType = chosen,
            EE_CP_FAVORITE_SEA => f.mFavoriteSeaType = chosen,
            EE_CP_FAVORITE_AIR => f.mFavoriteAirType = chosen,
            _ => f.mFavoriteSpaceType = chosen,
        });
    }
    let f = rec(h);
    let favorite = match kind {
        EE_CP_FAVORITE_LAND => f.mFavoriteLandType,
        EE_CP_FAVORITE_SEA => f.mFavoriteSeaType,
        EE_CP_FAVORITE_AIR => f.mFavoriteAirType,
        _ => f.mFavoriteSpaceType,
    };
    let now = h.epoch();
    h.history_push(favorite as u32, now);
}

/// RestoreManagers' favourites: a favourite whose class is gone is chosen again, else set to the kind's default
/// (air 72, land 20, sea 242, space 730), in the original's order.
pub fn restore_favorites<H: PlayerHost + ?Sized>(h: &mut H) {
    for (kind, default) in
        [(EE_CP_FAVORITE_AIR, 72), (EE_CP_FAVORITE_LAND, 20), (EE_CP_FAVORITE_SEA, 242), (EE_CP_FAVORITE_SPACE, 730)]
    {
        let get = |f: &EeCpFields| match kind {
            EE_CP_FAVORITE_LAND => f.mFavoriteLandType,
            EE_CP_FAVORITE_SEA => f.mFavoriteSeaType,
            EE_CP_FAVORITE_AIR => f.mFavoriteAirType,
            _ => f.mFavoriteSpaceType,
        };
        let fav = get(&rec(h)) as u32;
        if h.class_exists(fav as i32) {
            continue;
        }
        choose_favorite(h, kind);
        let fav = get(&rec(h)) as u32;
        if !h.class_exists(fav as i32) {
            rec_mut(h, |f| match kind {
                EE_CP_FAVORITE_LAND => f.mFavoriteLandType = default,
                EE_CP_FAVORITE_SEA => f.mFavoriteSeaType = default,
                EE_CP_FAVORITE_AIR => f.mFavoriteAirType = default,
                _ => f.mFavoriteSpaceType = default,
            });
        }
    }
}

/// The premade civilizations `ChooseCivilization` picks from (NUL-terminated for the C++).
pub(crate) const CP_CIVILIZATIONS: [&str; 23] = [
    "Babylon\0",
    "Classical Greece\0",
    "Carthage\0",
    "Kingdom of Israel\0",
    "Assyrian Empire\0",
    "Byzantine Rome\0",
    "England\0",
    "Franks\0",
    "Spain\0",
    "Kingdom of Italy\0",
    "Austria\0",
    "Ottoman Empire\0",
    "United States\0",
    "Germany\0",
    "Russia\0",
    "Great Britain\0",
    "Italy\0",
    "France\0",
    "Rebel Forces\0",
    "China\0",
    "Novaya Russia\0",
    "Japan\0",
    "Korea\0",
];

/// The civilizations that fit an epoch (indices into [`CP_CIVILIZATIONS`], in the original's order).
pub fn civilizations(epoch: i32, waterless: bool, space: bool) -> Vec<usize> {
    let e = epoch;
    let within = |a: i32, b: i32| e >= a && e <= b;
    let rules: [(bool, usize); 21] = [
        (within(0, CP_EPOCH_BRONZE), 0),
        (within(0, CP_EPOCH_DARK) && !waterless, 1),
        (within(0, CP_EPOCH_DARK) && !waterless, 2),
        (within(0, CP_EPOCH_DARK), 3),
        (within(0, CP_EPOCH_DARK), 4),
        (within(0, CP_EPOCH_MIDDLE), 5),
        (within(CP_EPOCH_DARK, CP_EPOCH_INDUSTRIAL) && !waterless, 6),
        (within(CP_EPOCH_DARK, CP_EPOCH_INDUSTRIAL), 7),
        (within(CP_EPOCH_DARK, CP_EPOCH_INDUSTRIAL) && !waterless, 8),
        (within(CP_EPOCH_DARK, CP_EPOCH_INDUSTRIAL), 9),
        (within(CP_EPOCH_DARK, CP_EPOCH_INDUSTRIAL), 10),
        (within(CP_EPOCH_DARK, CP_EPOCH_INDUSTRIAL), 11),
        (within(CP_EPOCH_INDUSTRIAL, CP_EPOCH_SPACE) && !waterless, 12),
        (within(CP_EPOCH_INDUSTRIAL, CP_EPOCH_SPACE), 13),
        (within(CP_EPOCH_INDUSTRIAL, CP_EPOCH_SPACE), 14),
        (within(CP_EPOCH_INDUSTRIAL, CP_EPOCH_SPACE) && !waterless, 15),
        (within(CP_EPOCH_INDUSTRIAL, CP_EPOCH_SPACE), 16),
        (within(CP_EPOCH_INDUSTRIAL, CP_EPOCH_SPACE), 17),
        (within(CP_EPOCH_MODERN, CP_EPOCH_SPACE), 18),
        (within(CP_EPOCH_MODERN, CP_EPOCH_SPACE), 19),
        (within(CP_EPOCH_DIGITAL, CP_EPOCH_SPACE), 20),
    ];
    let mut out: Vec<usize> = rules.iter().filter(|r| r.0).map(|r| r.1).collect();
    if within(CP_EPOCH_DIGITAL, CP_EPOCH_SPACE) && space {
        out.push(21);
        out.push(22);
    }
    out
}

/// `ChooseCivilization`'s pick: a random fitting civilization (None: none, or no epoch).
pub fn choose_civilization<H: PlaceHost + ?Sized>(
    h: &mut H,
    epoch: i32,
    waterless: bool,
    space: bool,
) -> Option<usize> {
    if epoch == CP_TECH_NONE {
        return None;
    }
    let list = civilizations(epoch, waterless, space);
    if list.is_empty() {
        return None;
    }
    let index = h.random(list.len() as i32 - 1);
    Some(list[index as usize])
}

/// `EECPRetireObsoleteUnits`: from the Atomic Age (WW I) on, with the retire cheat, the player's living obsolete
/// units of the retired types that no human player sees are killed.
pub fn retire_obsolete_units<H: PlayerHost + ?Sized>(h: &mut H) {
    let epoch = h.epoch();
    if !h.cheat_on(CP_CHEAT_RETIRE) || epoch < CP_EPOCH_WW1 {
        return;
    }
    let mut humans = Vec::new();
    let mut i = h.num_world_players() as i64 - 1;
    while i > 0 {
        if h.player_human(i as u32) == Some(true) {
            humans.push(h.player_handle_at(i as u32));
        }
        i -= 1;
    }
    if humans.is_empty() {
        return;
    }
    let mut doomed = Vec::new();
    for u in h.own_units() {
        let c = h.unit(u).complex;
        if c == 0 || !h.unit_obsolete(c) {
            continue;
        }
        let ci = h.unit(c);
        if !CP_RETIRED_TYPES.contains(&ci.unit_type) {
            continue;
        }
        let mut found = ci.hit_points > 0;
        let mut k = humans.len() as i64 - 1;
        while k >= 0 && found {
            found = !h.seen_by(c, humans[k as usize]);
            k -= 1;
        }
        if found {
            doomed.push(c);
        }
    }
    for &c in doomed.iter().rev() {
        h.kill(c);
    }
}

/// `EECPPendingEpoch`: an epoch researched or queued at one of the player's living buildings (`kTechNone`: none).
pub fn pending_epoch<H: PlayerHost + ?Sized>(h: &mut H) -> i32 {
    for u in h.own_units() {
        let i = h.unit(u);
        if i.complex == 0 || !h.unit_is_building(i.complex) || i.hit_points <= 0 {
            continue;
        }
        let research = h.unit_research(i.complex);
        if let Some((&current, queue)) = research.split_first() {
            if current != CP_TECH_NONE && h.is_epoch(current) {
                return current;
            }
            for &t in queue {
                if h.is_epoch(t) {
                    return t;
                }
            }
        }
    }
    CP_TECH_NONE
}

/// `AvoidsEnemies`: a citizen, or a loaded transport that carries only citizens.
pub fn avoids_enemies<H: PlayerHost + ?Sized>(h: &mut H, unit: u64) -> bool {
    let category = h.unit(unit).category;
    if category == CP_CATEGORY_LAND_CITIZEN || category == CP_CATEGORY_NAVAL_CITIZEN {
        return true;
    }
    if !h.is_transport(unit) {
        return false;
    }
    let cargo = h.cargo(unit);
    if cargo.is_empty() {
        return false;
    }
    for c in cargo {
        let k = h.unit(c).category;
        if k != CP_CATEGORY_LAND_CITIZEN && k != CP_CATEGORY_NAVAL_CITIZEN {
            return false;
        }
    }
    true
}
