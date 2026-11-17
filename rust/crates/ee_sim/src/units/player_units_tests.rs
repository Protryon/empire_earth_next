use super::*;
use crate::units::player::{
    EeUnitsPlayerIo, EE_UNITS_PLAYER_CLASS, EE_UNITS_PLAYER_IS_LOCAL, EE_UNITS_PLAYER_NODE, EE_UNITS_PLAYER_NODE_INFO,
};
use crate::units::player_stats::EeUnitsPlayerStats;

/// A player's world: its units by address (1..), their order, the tech nodes (id -> node kind, made class), the
/// classes that are heroes, the state bits, and every call but the reads.
#[derive(Default)]
struct World {
    units: Vec<EeUnitsPlayerUnit>,
    order: Vec<i64>,
    nodes: Vec<(c_long, c_long, i64)>,
    heroes: Vec<i64>,
    citizens: Vec<c_ulong>,
    state: i64,
    players: i64,
    me: i64,
    obsolete: bool,
    upgrades: Vec<i64>,
    set: Vec<i64>,
    queue: Vec<c_long>,
    pending: Vec<c_long>,
    log: Vec<String>,
    /// the upgrade's values before and after its level change, the complex units' flight time and power, the draws
    /// and the producers' answers
    upgrade: Option<[c_long; 4]>,
    changed: Option<[c_long; 4]>,
    values: (c_long, c_long),
    draws: Vec<f32>,
    produces: Vec<bool>,
}

impl PlayerHost for World {
    fn call(&mut self, op: i32, io: &mut EeUnitsPlayerIo) -> i64 {
        let a = io.i;
        match op {
            EE_UNITS_PLAYER_UNIT_INFO => {
                // SAFETY: the record the caller passed
                unsafe { *(a[1] as *mut EeUnitsPlayerUnit) = self.units[a[0] as usize - 1] };
                0
            }
            EE_UNITS_PLAYER_NEXT_UNIT | EE_UNITS_PLAYER_NEXT_COMPLEX => {
                let at = self.order.iter().position(|&u| u == a[0]).map_or(0, |p| p + 1);
                let mut rest = self.order.iter().skip(if a[0] == 0 { 0 } else { at });
                if op == EE_UNITS_PLAYER_NEXT_COMPLEX {
                    rest.find(|&&u| self.units[u as usize - 1].complex != 0).copied().unwrap_or(0)
                } else {
                    rest.next().copied().unwrap_or(0)
                }
            }
            EE_UNITS_PLAYER_STATE => self.state,
            EE_UNITS_PLAYER_NUM_PLAYERS => self.players,
            EE_UNITS_PLAYER_ID => self.me,
            EE_UNITS_PLAYER_AT => 100 + a[0],
            EE_UNITS_PLAYER_NODE => self.nodes.iter().position(|n| n.0 == a[0] as c_long).map_or(0, |p| p as i64 + 1),
            EE_UNITS_PLAYER_NODE_INFO => {
                let n = self.nodes[a[0] as usize - 1];
                io.i = [0; 8];
                io.i[1] = n.2;
                io.i[5] = n.1 as i64;
                0
            }
            EE_UNITS_PLAYER_CLASS => {
                io.i[4] = i64::from(self.heroes.contains(&a[0]));
                1
            }
            EE_UNITS_PLAYER_NODE_IS_AVAILABLE => 0,
            EE_UNITS_PLAYER_CITIZEN_CONTAINS => i64::from(self.citizens.contains(&(a[0] as c_ulong))),
            EE_UNITS_PLAYER_TECH_OBSOLETE => i64::from(self.obsolete),
            EE_UNITS_PLAYER_FOUNDATION | EE_UNITS_PLAYER_SWAP_KIND | EE_UNITS_PLAYER_CIV_POWER => 0,
            EE_UNITS_PLAYER_UNIT_NODE => self.order.first().map_or(0, |_| {
                let t = self.units[a[0] as usize - 1].technology;
                self.nodes.iter().position(|n| n.0 == t).map_or(0, |p| p as i64 + 1)
            }),
            EE_UNITS_PLAYER_BUILD_QUEUE_TECH => self.queue[a[1] as usize] as i64,
            EE_UNITS_PLAYER_PENDING_COUNT => self.pending.len() as i64,
            EE_UNITS_PLAYER_PENDING_POPULATION => self.pending[a[0] as usize] as i64,
            EE_UNITS_PLAYER_UPGRADE_TYPE => self.upgrades.get(a[0] as usize).copied().unwrap_or(-2),
            EE_UNITS_PLAYER_SET_CONTAINS => i64::from(self.set.contains(&a[1])),
            EE_UNITS_PLAYER_WONDER_FACTORS => {
                io.f = [0.5, 100.0];
                0
            }
            EE_UNITS_PLAYER_UPGRADE_HIT_POINTS => {
                self.log.push(format!("hp {} {}", a[0], io.f[0]));
                0
            }
            EE_UNITS_PLAYER_DEAD_STATISTIC => 7,
            EE_UNITS_PLAYER_CONTAINER_CATEGORY => i64::from(a[0] == 99) * CATEGORY_FORTRESS as i64,
            EE_UNITS_PLAYER_UPGRADE_VALUES => match self.upgrade {
                Some(v) => {
                    for (k, x) in v.iter().enumerate() {
                        io.i[k] = *x as i64;
                    }
                    1
                }
                None => 0,
            },
            EE_UNITS_PLAYER_COMPLEX_VALUES => {
                io.i[0] = self.values.0 as i64;
                io.i[1] = self.values.1 as i64;
                0
            }
            EE_UNITS_PLAYER_GENERATE => {
                io.f[0] = self.draws.remove(0);
                0
            }
            EE_UNITS_PLAYER_PRODUCE => {
                self.log.push(format!("{op} {} {}", a[0], a[1]));
                i64::from(self.produces.remove(0))
            }
            EE_UNITS_PLAYER_IS_LOCAL => 1,
            EE_UNITS_PLAYER_SET_VALUE | EE_UNITS_PLAYER_CHANGE_LEVEL => {
                self.log.push(format!("{op} {} {} {}", a[0], a[1], a[2]));
                self.upgrade = if op == EE_UNITS_PLAYER_CHANGE_LEVEL { self.changed } else { self.upgrade };
                0
            }
            _ => {
                self.log.push(format!("{op} {} {}", a[0], a[1]));
                0
            }
        }
    }
}

fn unit(unit_type: c_ulong) -> EeUnitsPlayerUnit {
    EeUnitsPlayerUnit { complex: 0, unit_type, population: 1, hit_points: 10, ..Default::default() }
}

/// the unit at address `n` with itself as its complex unit
fn complex(n: u64, mut u: EeUnitsPlayerUnit) -> EeUnitsPlayerUnit {
    u.complex = n;
    u.first_part = true;
    u
}

struct Records {
    saved: Box<EeUnitsPlayerSaved>,
    fields: Box<EeUnitsPlayerFields>,
    wonders: Box<EeUnitsPlayerWonders>,
    stats: EeUnitsPlayerStats,
}

impl Records {
    fn new() -> Self {
        let mut r = Records {
            saved: Box::default(),
            fields: Box::default(),
            wonders: Box::default(),
            stats: EeUnitsPlayerStats::default(),
        };
        r.fields.mWonderState = &mut *r.wonders;
        r
    }
    fn run<R>(&mut self, w: &mut World, body: impl FnOnce(&mut Player<World>) -> R) -> R {
        let mut p = Player { h: w, saved: &mut *self.saved, fields: &mut *self.fields, stats: &self.stats };
        body(&mut p)
    }
}

fn ops(w: &World) -> Vec<i32> {
    w.log.iter().filter_map(|l| l.split(' ').next()?.parse().ok()).collect()
}

#[test]
fn add_unit_counts_population_vital_units_and_the_first_capitol() {
    let mut w = World { players: 3, citizens: vec![TYPE_CAPITOL], nodes: vec![(500, 0, 0)], ..Default::default() };
    let mut capitol = complex(1, unit(TYPE_CAPITOL));
    capitol.population = 0;
    capitol.technology = 500;
    capitol.land_building = true;
    capitol.built = true;
    let mut king = complex(2, unit(0x40));
    king.vital = true;
    king.population = 2;
    w.units = vec![capitol, king];
    let mut r = Records::new();
    r.run(&mut w, |p| {
        p.add_unit(1, false);
        p.add_unit(2, true);
    });
    assert_eq!(r.saved.mPopulationCount, 2);
    assert_eq!(r.saved.mEndOfGameCount, 1, "the king (not a land building) is vital");
    assert_eq!(r.saved.mNumCapitols, 1);
    assert_eq!(
        w.log[..9],
        [
            "29 1 0", // RTSPlayer::AddUnit
            "30 1 0", // activated (active class, first part, on the map)
            "32 500 1",
            "33 500 0", // built: researched
            "32 500 0", // made unavailable again (it was)
            "35 1 0",   // UnitPlaced
            "23 0 0",   // the first capitol: the JIT buttons
            "38 1028 1",
            "38 1025 0",
        ]
    );
    assert_eq!(w.log[9], "38 1053 0");
    assert_eq!(w.log[10], "41 7 1", "the dead units' statistic");
}

#[test]
fn add_unit_with_the_settlement_obsolete_allows_the_town_center() {
    let mut w = World { players: 3, citizens: vec![TYPE_CAPITOL], obsolete: true, state: 0, ..Default::default() };
    w.units = vec![unit(TYPE_CAPITOL)];
    let mut r = Records::new();
    r.run(&mut w, |p| p.add_unit(1, false));
    let allows: Vec<&String> = w.log.iter().filter(|l| l.starts_with("38 ")).collect();
    assert_eq!(allows, ["38 1025 1", "38 1053 0"]);
    assert!(!w.log.iter().any(|l| l.starts_with("41 ")), "no complex unit: no statistic");
    // the shutdown skips it
    let mut w =
        World { players: 3, citizens: vec![TYPE_CAPITOL], state: EE_UNITS_PLAYER_STATE_SHUTDOWN, ..Default::default() };
    w.units = vec![unit(TYPE_CAPITOL)];
    r.run(&mut w, |p| p.add_unit(1, false));
    assert_eq!(r.saved.mNumCapitols, 2);
    assert!(!w.log.iter().any(|l| l.starts_with("38 ")));
}

#[test]
fn unit_placed_counts_heroes_and_teleporters_and_joins_the_town_and_herd() {
    let mut w = World { state: EE_UNITS_PLAYER_STATE_GAME_SCREEN, ..Default::default() };
    let mut hero = complex(1, unit(0x50));
    hero.ai_kind = AI_KIND_HERO;
    let mut gate = complex(2, unit(TYPE_TELEPORTER));
    gate.category = CATEGORY_GATE;
    w.units = vec![hero, gate];
    let mut r = Records::new();
    r.run(&mut w, |p| {
        p.unit_placed(1);
        p.unit_placed(2);
    });
    assert_eq!((r.fields.mNumHeroes, r.fields.mNumTeleporters), (1, 1));
    assert_eq!(w.log, ["43 1 0", "44 1 0", "43 2 0", "44 2 0", "47 2 0"]);
    // while loading nothing
    w.state |= EE_UNITS_PLAYER_STATE_LOADING;
    w.log.clear();
    r.run(&mut w, |p| p.unit_placed(1));
    assert!(w.log.is_empty());
    assert_eq!(r.fields.mNumHeroes, 1);
}

#[test]
fn unit_taken_undoes_the_counts_and_the_last_capitol_allows_one_again() {
    let mut w = World {
        state: EE_UNITS_PLAYER_STATE_GAME_SCREEN | EE_UNITS_PLAYER_STATE_INTERFACE,
        citizens: vec![TYPE_TOWN_CENTER],
        ..Default::default()
    };
    let mut center = complex(1, unit(TYPE_TOWN_CENTER));
    center.land_building = true;
    center.vital = true;
    center.built = true;
    center.blocks_tile = true;
    w.units = vec![center];
    let mut r = Records::new();
    r.saved.mPopulationCount = 5;
    r.saved.mEndOfGameCount = 1;
    r.saved.mNumVitalBuildings = 1;
    r.saved.mNumCapitols = 1;
    r.run(&mut w, |p| p.unit_taken(1, false));
    assert_eq!((r.saved.mPopulationCount, r.saved.mEndOfGameCount, r.saved.mNumVitalBuildings), (4, 0, 0));
    assert_eq!(r.saved.mNumCapitols, 0);
    assert_eq!(
        w.log,
        [
            "48 1 0", // the game log
            "55 0 0", // the last vital building: camouflage off
            "50 1 0", // out of its town
            "51 1 0", // and its herd
            "23 0 0", // the last capitol: the JIT buttons
            "38 1053 1",
            "38 1028 0",
            "47 1 1", // the walls
        ]
    );
}

#[test]
fn unit_taken_of_a_tree_removes_its_forest_tile_and_an_unbuilt_wall_its_site() {
    let mut w = World { state: EE_UNITS_PLAYER_STATE_GAME_SCREEN, ..Default::default() };
    let mut tree = unit(0x60);
    tree.tree = true;
    tree.blocks_tile = true;
    tree.population = 0;
    let mut wall = complex(2, unit(0x61));
    wall.wall = true;
    wall.on_tile = true;
    wall.land_building = true;
    wall.population = 0;
    w.units = vec![tree, wall];
    let mut r = Records::new();
    r.run(&mut w, |p| {
        p.unit_taken(1, true);
        p.unit_taken(2, true);
    });
    assert_eq!(w.log, ["47 1 1", "54 1 0", "50 2 0", "51 2 0", "52 2 0"]);
    // while the world clears, no town and no walls
    w.state |= EE_UNITS_PLAYER_STATE_CLEARING;
    w.log.clear();
    r.run(&mut w, |p| p.unit_taken(1, true));
    assert!(w.log.is_empty());
}

#[test]
fn unit_completed_registers_the_wonder_and_tells_the_other_players() {
    let mut w = World {
        state: EE_UNITS_PLAYER_STATE_GAME_SCREEN | EE_UNITS_PLAYER_STATE_VICTORY,
        players: 3,
        nodes: vec![(900, NODE_KIND_WONDER, 0)],
        ..Default::default()
    };
    let mut wonder = complex(1, unit(0x70));
    wonder.technology = 900;
    wonder.land_building = true;
    wonder.vital = true;
    w.units = vec![wonder];
    let mut r = Records::new();
    r.run(&mut w, |p| p.unit_completed(1));
    assert_eq!((r.saved.mEndOfGameCount, r.saved.mNumVitalBuildings), (1, 1));
    assert_eq!(w.log, ["56 1 0", "57 0 1", "57 1 1", "57 2 1", "55 1 0", "58 1 0", "43 1 0"]);
}

#[test]
fn recounts_walk_the_living_units() {
    let mut w = World { heroes: vec![0x90], nodes: vec![(700, 0, 0x90), (701, 0, 0x91)], ..Default::default() };
    let mut a = complex(1, unit(TYPE_TELEPORTER));
    a.current_research = 700;
    a.build_queue = 2;
    let mut b = complex(2, unit(0x30));
    b.container = 99;
    b.vital = true;
    b.land_building = true;
    b.built = true;
    let mut dead = complex(3, unit(0x31));
    dead.hit_points = 0;
    let plain = unit(0x32);
    w.units = vec![a, b, dead, plain];
    w.order = vec![1, 2, 3, 4];
    w.queue = vec![700, 701];
    let mut r = Records::new();
    r.run(&mut w, |p| {
        p.recount_population();
        p.recount_heroes_and_teleporters();
    });
    assert_eq!((r.saved.mPopulationCount, r.saved.mEndOfGameCount, r.saved.mNumVitalBuildings), (1, 1, 1));
    assert_eq!((r.fields.mNumHeroes, r.fields.mNumTeleporters, r.fields.mNumHeroesInProduction), (0, 1, 2));
    w.pending = vec![3, 4];
    let totals = r.run(&mut w, |p| {
        (p.total_population(false, false), p.total_population(true, false), p.total_population(true, true))
    });
    assert_eq!(totals, (1, 8, 9));
    r.fields.mNumHeroes = 2;
    assert_eq!(
        r.run(&mut w, |p| (p.num_heroes(true, false), p.num_heroes(true, true), p.num_heroes(false, false))),
        (2, 4, 0)
    );
}

#[test]
fn wonders_count_per_type_and_grant_their_power_once() {
    let mut w = World { players: 4, me: 1, ..Default::default() };
    let mut sight = complex(1, unit(0x80));
    sight.wonder_bonus = 5;
    let sight2 = complex(2, sight);
    w.units = vec![sight, sight2];
    let mut r = Records::new();
    r.saved.mEnemies[2] = 1;
    r.saved.mEnemies[3] = 0;
    r.run(&mut w, |p| {
        p.add_wonder(1, false);
        p.add_wonder(2, true);
    });
    assert_eq!(r.wonders.powers, [5]);
    assert_eq!(r.wonders.counts.get(&0x80), Some(&2));
    assert_eq!(w.log, ["62 1 1", "68 102 1"], "the listing, then the one enemy shares its sight");
    w.log.clear();
    r.run(&mut w, |p| {
        p.remove_wonder(1);
        p.remove_wonder(2);
    });
    assert!(r.wonders.powers.is_empty());
    assert!(r.wonders.counts.is_empty());
    assert_eq!(w.log, ["62 1 0", "62 2 0", "68 102 0"]);
}

#[test]
fn wonder_powers_set_their_members() {
    let mut w = World { upgrades: vec![5, -1, 6, 7], set: vec![5, 7], ..Default::default() };
    let mut u = complex(1, unit(0x81));
    w.units = vec![u];
    let mut r = Records::new();
    for bonus in 1..=6 {
        u.wonder_bonus = bonus;
        w.units[0] = u;
        r.run(&mut w, |p| p.add_wonder_power(1));
    }
    assert_eq!(r.wonders.powers, [1, 2, 3, 4, 5, 6]);
    assert!(r.fields.mAreaConversion && r.fields.mHealsUnits);
    assert_eq!(r.saved.mHealTimer, EE_UNITS_PLAYER_HEAL_INTERVAL);
    assert_eq!(w.log, ["hp 0 150", "hp 3 150", "67 1 0", "67 0 0", "69 0 1"]);
    w.log.clear();
    for bonus in [2, 4, 6, 1] {
        u.wonder_bonus = bonus;
        w.units[0] = u;
        r.run(&mut w, |p| p.remove_wonder_power(1));
    }
    assert_eq!(r.wonders.powers, [3, 5]);
    assert!(!r.fields.mAreaConversion && !r.fields.mHealsUnits);
    assert_eq!(w.log, ["hp 0 -150", "hp 3 -150", "67 1 0", "67 0 0", "69 0 0"]);
    // a power not held: nothing to take
    u.wonder_bonus = 1;
    w.units[0] = u;
    r.run(&mut w, |p| p.remove_wonder_power(1));
    assert_eq!(r.wonders.powers, [3, 5]);
    assert_eq!(w.log.len(), 5);
    assert_eq!(ops(&w), [67, 67, 69]);
}

fn view(holds: bool, enemies: &[(usize, c_ulong)]) -> Option<ShareView> {
    let mut v = ShareView { holds, enemies: [1; 17] };
    for &(j, s) in enemies {
        v.enemies[j] = s;
    }
    Some(v)
}

#[test]
fn share_balance_follows_who_holds_the_power() {
    // 1 holds it with no enemy holding it; 2 (1's ally) holds none: an ally holds it
    let players = vec![None, view(true, &[(2, 0)]), view(false, &[(1, 0)]), view(false, &[]), None];
    assert_eq!(share_balance(&players, 1), 1);
    assert_eq!(share_balance(&players, 2), 1);
    assert_eq!(share_balance(&players, 3), -1, "only an enemy holds it");
    assert_eq!(share_balance(&players, 4), 0, "no player");
    // an enemy holds it too: none
    let players = vec![None, view(true, &[]), view(true, &[])];
    assert_eq!(share_balance(&players, 1), 0);
    // allies and enemies both: none, in either order
    let players = vec![None, view(false, &[(2, 0)]), view(true, &[]), view(true, &[])];
    assert_eq!(share_balance(&players, 1), 0);
    let players = vec![None, view(false, &[(3, 0)]), view(true, &[]), view(true, &[])];
    assert_eq!(share_balance(&players, 1), 0);
    // stance 2 (neutral) counts neither way
    let players = vec![None, view(false, &[(2, 2)]), view(true, &[])];
    assert_eq!(share_balance(&players, 1), 0);
}

#[test]
fn share_change_truncates_the_products() {
    assert_eq!(share_change(1, false, 0.1, 0.05, 75), 7);
    assert_eq!(share_change(-1, false, 0.1, 0.05, 75), -3);
    assert_eq!(share_change(1, true, 0.1, 0.05, 75), -7);
    assert_eq!(share_change(-1, true, 0.1, 0.05, 75), 3);
    assert_eq!(share_change(0, false, 0.1, 0.05, 75), 0);
}

#[test]
fn the_share_bonus_entry_changes_every_present_player() {
    let mut a = Records::new();
    let mut b = Records::new();
    a.wonders.powers.push(4);
    a.saved.mEnemies[2] = 0;
    b.saved.mEnemies[1] = 0;
    a.fields.mShareBonus = 1;
    extern "C" fn none(_: *mut c_void, _: i32, _: *mut EeUnitsPlayerIo) -> i64 {
        0
    }
    let refs = |r: &mut Records| EeUnitsPlayerRefs {
        player: std::ptr::null_mut(),
        host: none,
        saved: &mut *r.saved,
        fields: &mut *r.fields,
        stats: &r.stats,
    };
    let mut gaia = refs(&mut Records::new());
    gaia.fields = std::ptr::null_mut();
    let players = [gaia, refs(&mut a), refs(&mut b)];
    // SAFETY: the records live for the calls
    unsafe { ee_units_player_share_bonus(players.as_ptr(), 3, false, 0.25, 0.5, 100) };
    assert_eq!((a.fields.mShareBonus, b.fields.mShareBonus), (26, 25));
    // SAFETY: as above
    unsafe { ee_units_player_share_bonus(players.as_ptr(), 3, true, 0.25, 0.5, 100) };
    assert_eq!((a.fields.mShareBonus, b.fields.mShareBonus), (1, 0));
    // SAFETY: a's state is live
    assert_eq!(unsafe { ee_units_wonders_count(&*a.wonders, 4) }, 1);
}

#[test]
fn an_improvement_rescales_the_units_of_its_type() {
    let mut w =
        World { upgrade: Some([0, 0, 40, 100]), changed: Some([0, 0, 50, 120]), values: (0, 30), ..Default::default() };
    let mut a = complex(1, unit(0x90));
    a.hit_points = 50;
    a.built = true;
    let mut unbuilt = complex(2, unit(0x90));
    unbuilt.hit_points = 50;
    let other = complex(3, unit(0x91));
    w.units = vec![a, unbuilt, other];
    w.order = vec![1, 2, 3];
    let mut r = Records::new();
    r.run(&mut w, |p| p.apply_improvement(0x90, 9, false));
    // 50 / 100 * 120 = 60; the unbuilt unit keeps its hit points
    assert_eq!(w.log, ["104 144 9 1", "107 1 60 2"]);
    // the power (a class that does not fly): 30 / 40 * 50 = 37.5, truncated
    w.log.clear();
    w.upgrade = Some([0, 0, 40, 100]);
    r.run(&mut w, |p| p.apply_improvement(0x90, 8, true));
    assert_eq!(w.log, ["104 144 8 -1", "107 1 37 1", "107 2 37 1"]);
    // the sight: off before, on after; another improvement only changes the level
    w.log.clear();
    r.run(&mut w, |p| {
        p.apply_improvement(0x90, 11, false);
        p.apply_improvement(0x90, 3, false);
    });
    assert_eq!(w.log, ["105 1 0", "105 2 0", "104 144 11 1", "105 1 1", "105 2 1", "104 144 3 1"]);
    // no upgrade: nothing
    w.log.clear();
    w.upgrade = None;
    r.run(&mut w, |p| p.apply_improvement(0x90, 9, false));
    assert!(w.log.is_empty());
}

#[test]
fn the_slavery_power_makes_a_citizen_every_interval() {
    let mut w = World { draws: vec![0.7, 0.2], produces: vec![false, true], ..Default::default() };
    w.units = vec![unit(TYPE_TOWN_CENTER), unit(TYPE_CAPITOL)];
    w.order = vec![1, 2];
    let mut r = Records::new();
    r.saved.mFreeUnitInterval = 2;
    // without the power nothing counts
    r.run(&mut w, |p| p.update_free_unit_power());
    assert_eq!(r.saved.mFreeUnitCounter, 0);
    struct Slavery(World);
    impl PlayerHost for Slavery {
        fn call(&mut self, op: i32, io: &mut EeUnitsPlayerIo) -> i64 {
            if op == EE_UNITS_PLAYER_CIV_POWER {
                return i64::from(io.i[0] == POWER_SLAVERY);
            }
            self.0.call(op, io)
        }
    }
    let mut h = Slavery(w);
    let mut p = Player { h: &mut h, saved: &mut *r.saved, fields: &mut *r.fields, stats: &r.stats };
    p.update_free_unit_power();
    p.update_free_unit_power();
    // the capitol first (a male citizen: refused), then the town center (a female one)
    assert_eq!(r.saved.mFreeUnitCounter, 0);
    assert_eq!(r.saved.mFreeUnitInterval, 3);
    assert_eq!(h.0.log, ["109 2 32", "109 1 480", "110 0 0"]);
}
