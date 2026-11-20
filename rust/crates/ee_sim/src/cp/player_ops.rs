//! `EEComputerPlayer`'s rules that need no game (AI Thread cluster round 7; were EEComputerPlayer.cpp and
//! EEAIFunctionObjects.cpp): the favourite unit screener's tests (`EEAIFOFavoriteUnitScreener::Screen` and the four
//! `EECPUSFavoriteUnitsScreen::IsFavorite*`), the hero count (`EECPUGatherHeroesCount` over `EECPGatherUnits`), the
//! kind of a payment (`Pay`'s TEST ONLY spending trace), the stranded unit test (`RestoreManagers`), the civilization
//! note's hash (`ChooseCivilization`'s CPEV word) and the CPEV method ids. The game glue is [`super::player_game`].

// `c_long` is 32 bits on Windows and 64 elsewhere
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

/// CPEV's method ids (TEST ONLY; ee/EECPPlaceCapture.h had them as `kCpev*`).
pub(crate) mod cpev {
    pub(crate) const WORK_OUT_POPULATION: u32 = 1;
    pub(crate) const REGROUP: u32 = 2;
    pub(crate) const UPDATE_GROUPS: u32 = 3;
    pub(crate) const ENTER_EPOCH: u32 = 10;
    pub(crate) const CHOOSE_CIVILIZATION: u32 = 11;
    pub(crate) const PENDING_EPOCH: u32 = 12;
    pub(crate) const AVOIDS_ENEMIES: u32 = 13;
    pub(crate) const CAN_PRODUCE: u32 = 14;
    pub(crate) const RESTORE_MANAGERS: u32 = 15;
    pub(crate) const DAMAGE_TAKEN: u32 = 16;
    pub(crate) const UNIT_PLACED: u32 = 17;
    pub(crate) const UNIT_TAKEN: u32 = 18;
    pub(crate) const UNIT_COMPLETED: u32 = 19;
    pub(crate) const UNIT_GOAL_DONE: u32 = 20;
    pub(crate) const SET_ACTIVE: u32 = 21;
    pub(crate) const EPOCH_REACHED: u32 = 22;
    pub(crate) const UNIT_STRANDED: u32 = 23;
    pub(crate) const PAY_IMPROVEMENT: u32 = 24;
    pub(crate) const UNITS_SWAPPED: u32 = 25;
}

/// PLCE's method ids the glue opens (ee/EECPPlaceCapture.h `kPlce*`).
pub(crate) mod plce {
    pub(crate) const UNIT_ADDED: u32 = 24;
    pub(crate) const RESET: u32 = 25;
    pub(crate) const RESTORE: u32 = 26;
    pub(crate) const COUNT_OWN: u32 = 46;
}

// the unit classes' values the rules test (ee/EEUnitClassIDs.h, ee/EEUnit.h)
pub(crate) const AI_KIND_ANTI_TANK: c_ulong = 1;
pub(crate) const AI_KIND_SPECIAL: c_ulong = 3;
pub(crate) const AI_KIND_ANTI_AIR: c_ulong = 0xd;
pub(crate) const AI_KIND_ARTILLERY: c_ulong = 0x10;
pub(crate) const AI_KIND_SIEGE: c_ulong = 0x14;
pub(crate) const AI_KIND_HERO: c_ulong = 0x1b;
pub(crate) const AI_KIND_ANTI_SUB_HELICOPTER: c_ulong = 0x1c;
pub(crate) const AI_KIND_MECH_ZEUS: c_ulong = 0x1e;
pub(crate) const CATEGORY_LAND_CITIZEN: c_ulong = 6;
pub(crate) const CATEGORY_PROJECTILE: c_ulong = 9;
pub(crate) const CATEGORY_BOMB: c_ulong = 0xb;
pub(crate) const CATEGORY_MISSILE: c_ulong = 0xc;
pub(crate) const CATEGORY_HELICOPTER_TRANSPORT: c_ulong = 0x11;
pub(crate) const CATEGORY_NAVAL_TRANSPORT: c_ulong = 0x12;
pub(crate) const CATEGORY_NAVAL_CITIZEN: c_ulong = 0x16;
pub(crate) const CATEGORY_SPACE_TRANSPORT: c_ulong = 0x21;
pub(crate) const FAMILY_MINES: c_ulong = 0x16;
pub(crate) const TYPE_DOCK: i32 = 0x24;
pub(crate) const TYPE_AIRPORT: i32 = 0x42;
pub(crate) const PL_TYPE_PATH_POINT: c_ulong = 0xdf;
pub(crate) const TYPE_SPACE_DOCK: i32 = 0x2d8;
pub(crate) const MOVEMENT_AIR: u8 = 1;
pub(crate) const MOVEMENT_SEA: u8 = 4;
pub(crate) const MOVEMENT_SPACE: u8 = 8;
pub(crate) const TERRAIN_CLIFF: i64 = 9;
pub(crate) const TECH_NONE: c_long = 100000;
pub(crate) const GOAL_LOAD_TRANSPORT: u8 = 7;
pub(crate) const GOAL_UNLOAD_TRANSPORT: u8 = 8;
pub(crate) const ACTION_IDLE: i64 = 5000;
pub(crate) const TRIGGER_AT_RENDEZVOUS: i64 = 12011;
pub(crate) const RESOURCE_FOOD: c_long = 2;
pub(crate) const RESOURCE_GOLD: usize = 5;
pub(crate) const RESOURCE_IRON: usize = 6;
pub(crate) const CHEAT_COST: i64 = 1;
pub(crate) const CHEAT_REVEAL: i64 = 2;
pub(crate) const CHEAT_EPOCHS: i64 = 4;
pub(crate) const CHEAT_EXTRA: i64 = 8;
pub(crate) const NEXT_EVENT_UNIT_LOST: c_long = 0;
pub(crate) const MAP_WATERLESS: i64 = 2;
/// The unit sets the favourites leave out (gUnitSetManager's): air, sea, land, space.
pub(crate) const SET_AIR: i64 = 0x75;
pub(crate) const SET_SEA: i64 = 0x74;
pub(crate) const SET_LAND: i64 = 0x73;
pub(crate) const SET_SPACE: i64 = 0x98;

/// What the favourite screener reads of a class.
#[derive(Clone, Copy, Debug, Default)]
pub(crate) struct FavoriteClass {
    pub epoch: c_long,
    pub max_velocity: f32,
    pub attack: c_long,
    pub category: c_ulong,
    pub domains: u8,
    pub ai_kind: c_ulong,
    pub calamity_powers: bool,
    pub conversion_rate: f32,
    pub priest: bool,
    pub ammo_type: c_long,
    pub medic: bool,
    pub transport: bool,
    pub family: c_ulong,
    pub flies_over: bool,
    pub unit_type: c_ulong,
}

/// The domain a class's favourite test is (`mDomains`: air before sea before space, else land).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum Domain {
    Air,
    Sea,
    Space,
    Land,
}

pub(crate) fn domain(c: &FavoriteClass) -> Domain {
    if c.domains & MOVEMENT_AIR != 0 {
        Domain::Air
    } else if c.domains & MOVEMENT_SEA != 0 {
        Domain::Sea
    } else if c.domains & MOVEMENT_SPACE != 0 {
        Domain::Space
    } else {
        Domain::Land
    }
}

/// `Screen`'s first test: a moving, attacking non-citizen within the epoch span.
pub(crate) fn screen_class(c: &FavoriteClass, epoch: c_long, span: c_long) -> bool {
    c.epoch <= epoch
        && epoch.wrapping_sub(c.epoch) <= span
        && c.max_velocity != 0.0
        && c.attack > 0
        && c.category != CATEGORY_LAND_CITIZEN
        && c.category != CATEGORY_NAVAL_CITIZEN
}

/// One of the four `IsFavorite*` tests; `in_set(set)` answers `!GetSet(set)->IsEmpty()` and `Contains(type)` as
/// `Some(contains)` (None: the set is empty).
pub(crate) fn is_favorite(c: &FavoriteClass, d: Domain, mut in_set: impl FnMut(i64) -> Option<bool>) -> bool {
    if !(c.max_velocity != 0.0 && c.attack > 0) {
        return false;
    }
    let k = c.ai_kind;
    let excluded = match d {
        Domain::Air => {
            k == AI_KIND_HERO
                || c.calamity_powers
                || c.conversion_rate != 0.0
                || c.priest
                || c.ammo_type == 7
                || c.medic
                || k == AI_KIND_SPECIAL
                || c.transport
                || c.family == FAMILY_MINES
        }
        Domain::Sea | Domain::Space => {
            k == AI_KIND_HERO
                || c.medic
                || c.calamity_powers
                || c.conversion_rate != 0.0
                || c.priest
                || k == AI_KIND_SPECIAL
                || c.transport
                || c.family == FAMILY_MINES
        }
        Domain::Land => {
            k == AI_KIND_HERO
                || c.medic
                || c.calamity_powers
                || c.conversion_rate != 0.0
                || k == AI_KIND_ANTI_TANK
                || k == AI_KIND_SPECIAL
                || k == AI_KIND_SIEGE
                || k == AI_KIND_ANTI_AIR
                || k == AI_KIND_ARTILLERY
                || c.transport
                || c.family == FAMILY_MINES
        }
    };
    if excluded {
        return false;
    }
    let refused = match k {
        AI_KIND_SPECIAL | AI_KIND_ARTILLERY | AI_KIND_SIEGE | AI_KIND_ANTI_SUB_HELICOPTER | AI_KIND_MECH_ZEUS => true,
        AI_KIND_ANTI_AIR => d != Domain::Air,
        _ => false,
    };
    if refused {
        return false;
    }
    let set = match d {
        Domain::Air => SET_AIR,
        Domain::Sea => SET_SEA,
        Domain::Space => SET_SPACE,
        Domain::Land => SET_LAND,
    };
    let mut result = in_set(set).is_none_or(|has| !has);
    if d == Domain::Land && result && c.flies_over {
        result = in_set(SET_AIR).is_none_or(|has| !has);
    }
    result
}

/// A unit `GetHeroCount`'s class walk counts: complex, alive, no projectile, bomb, missile or path point.
pub(crate) fn hero_walk_counts(complex: bool, hit_points: c_long, category: c_ulong, unit_type: c_ulong) -> bool {
    complex
        && hit_points > 0
        && category != CATEGORY_PROJECTILE
        && category != CATEGORY_BOMB
        && category != CATEGORY_MISSILE
        && unit_type != PL_TYPE_PATH_POINT
}

/// What a payment buys (`Pay`'s spending trace): 4 an epoch, 0 a citizen, 5 a fishing boat, 2 a building, 1 an
/// attack or defense unit, 7 another unit, 3 none (no class).
pub(crate) fn pay_kind(epoch: bool, class: Option<(c_ulong, bool, bool)>) -> c_long {
    if epoch {
        return 4;
    }
    match class {
        None => 3,
        Some((category, _, _)) if category == CATEGORY_LAND_CITIZEN => 0,
        Some((category, _, _)) if category == CATEGORY_NAVAL_CITIZEN => 5,
        Some((_, true, _)) => 2,
        Some((_, false, military)) => {
            if military {
                1
            } else {
                7
            }
        }
    }
}

/// `RestoreManagers`' stranded unit: placed, complex, on a cliff tile, no path point, no flier or ship, no projectile
/// and unable to move on cliffs (`can_move` asked last, as the original's `&&` chain).
#[allow(clippy::too_many_arguments)]
pub(crate) fn stranded(
    tile_x: c_long,
    complex: bool,
    terrain: Option<i64>,
    unit_type: c_ulong,
    domains: u8,
    projectile: bool,
    can_move: impl FnOnce() -> bool,
) -> bool {
    tile_x != -10000
        && complex
        && terrain == Some(TERRAIN_CLIFF)
        && unit_type != PL_TYPE_PATH_POINT
        && domains & (MOVEMENT_AIR | MOVEMENT_SEA) == 0
        && !projectile
        && !can_move()
}

/// `ChooseCivilization`'s CPEV note: the civilization file's name hashed `h * 31 + c` in an `unsigned long`.
pub(crate) fn civ_hash(name: &[u8]) -> c_ulong {
    name.iter().fold(0 as c_ulong, |h, &c| h.wrapping_mul(31).wrapping_add(c as c_ulong))
}

/// `EECPUnitTileDistanceLess`: the squared tile distance from `from` (`long` arithmetic).
pub(crate) fn tile_distance(from: (c_long, c_long), to: (c_long, c_long)) -> c_long {
    let dx = from.0.wrapping_sub(to.0);
    let dy = from.1.wrapping_sub(to.1);
    dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn soldier() -> FavoriteClass {
        FavoriteClass { epoch: 3, max_velocity: 1.5, attack: 4, category: 1, ai_kind: 5, ..Default::default() }
    }

    #[test]
    fn the_screener_takes_moving_attackers_of_the_span() {
        let c = soldier();
        assert!(screen_class(&c, 3, 0));
        assert!(screen_class(&c, 5, 2));
        assert!(!screen_class(&c, 5, 1));
        assert!(!screen_class(&c, 2, 9));
        assert!(!screen_class(&FavoriteClass { category: CATEGORY_LAND_CITIZEN, ..c }, 3, 0));
        assert!(!screen_class(&FavoriteClass { max_velocity: 0.0, ..c }, 3, 0));
    }

    #[test]
    fn the_domains_go_air_sea_space_land() {
        assert_eq!(domain(&FavoriteClass { domains: 5, ..soldier() }), Domain::Air);
        assert_eq!(domain(&FavoriteClass { domains: 12, ..soldier() }), Domain::Sea);
        assert_eq!(domain(&FavoriteClass { domains: 8, ..soldier() }), Domain::Space);
        assert_eq!(domain(&soldier()), Domain::Land);
    }

    #[test]
    fn the_favourite_tests_leave_out_their_kinds_and_sets() {
        let c = soldier();
        assert!(is_favorite(&c, Domain::Land, |_| None));
        assert!(!is_favorite(&c, Domain::Land, |s| Some(s == SET_LAND)));
        assert!(is_favorite(&c, Domain::Land, |s| Some(s == SET_AIR)));
        // a flier on land also tests the air set
        let flier = FavoriteClass { flies_over: true, ..c };
        assert!(!is_favorite(&flier, Domain::Land, |s| Some(s == SET_AIR)));
        // anti-air: refused at sea, kept in the air
        let aa = FavoriteClass { ai_kind: AI_KIND_ANTI_AIR, ..c };
        assert!(!is_favorite(&aa, Domain::Sea, |_| None));
        assert!(is_favorite(&aa, Domain::Air, |_| None));
        assert!(!is_favorite(&aa, Domain::Land, |_| None));
        // anti-tank only on land; ammo 7 only in the air
        let at = FavoriteClass { ai_kind: AI_KIND_ANTI_TANK, ..c };
        assert!(!is_favorite(&at, Domain::Land, |_| None));
        assert!(is_favorite(&at, Domain::Sea, |_| None));
        let ammo = FavoriteClass { ammo_type: 7, ..c };
        assert!(!is_favorite(&ammo, Domain::Air, |_| None));
        assert!(is_favorite(&ammo, Domain::Space, |_| None));
        assert!(!is_favorite(&FavoriteClass { ai_kind: AI_KIND_HERO, ..c }, Domain::Sea, |_| None));
    }

    #[test]
    fn the_hero_walk_and_the_pay_kinds() {
        assert!(hero_walk_counts(true, 1, 1, 2));
        assert!(!hero_walk_counts(true, 0, 1, 2));
        assert!(!hero_walk_counts(true, 1, CATEGORY_MISSILE, 2));
        assert!(!hero_walk_counts(true, 1, 1, PL_TYPE_PATH_POINT));
        assert_eq!(pay_kind(true, None), 4);
        assert_eq!(pay_kind(false, None), 3);
        assert_eq!(pay_kind(false, Some((CATEGORY_NAVAL_CITIZEN, true, true))), 5);
        assert_eq!(pay_kind(false, Some((1, true, false))), 2);
        assert_eq!(pay_kind(false, Some((1, false, true))), 1);
        assert_eq!(pay_kind(false, Some((1, false, false))), 7);
    }

    #[test]
    fn the_stranded_test_asks_the_terrain_last() {
        let mut asked = false;
        assert!(stranded(3, true, Some(9), 1, 0, false, || {
            asked = true;
            false
        }));
        assert!(asked);
        assert!(!stranded(-10000, true, Some(9), 1, 0, false, || unreachable!()));
        assert!(!stranded(3, true, Some(9), 1, MOVEMENT_SEA, false, || unreachable!()));
        assert!(!stranded(3, true, Some(2), 1, 0, false, || unreachable!()));
    }

    #[test]
    fn the_civ_hash_and_the_tile_distance() {
        assert_eq!(civ_hash(b""), 0);
        assert_eq!(civ_hash(b"ab"), 97 * 31 + 98);
        assert_eq!(tile_distance((1, 2), (4, 6)), 25);
    }
}
