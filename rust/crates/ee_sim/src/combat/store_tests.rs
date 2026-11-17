use std::cell::RefCell;
use std::collections::BTreeMap;

use super::*;

thread_local! {
    static UNITS: RefCell<BTreeMap<u64, EeCombatStoredUnit>> = const { RefCell::new(BTreeMap::new()) };
    static PLAYERS: RefCell<BTreeMap<u64, EeCombatStoredPlayer>> = const { RefCell::new(BTreeMap::new()) };
    static STATS: RefCell<BTreeMap<u64, EeCombatStoredStats>> = const { RefCell::new(BTreeMap::new()) };
}

/// The tests' stats table (by stats object).
pub(crate) fn stats(object: u64) -> Option<EeCombatStoredStats> {
    STATS.with(|s| s.borrow().get(&object).copied())
}

pub(crate) fn put_stats(object: u64, record: EeCombatStoredStats) {
    STATS.with(|s| s.borrow_mut().insert(object, record));
}

/// The tests' unit store (by object, as the game's bridge).
pub(crate) fn unit(object: u64) -> Option<EeCombatStoredUnit> {
    UNITS.with(|s| s.borrow().get(&object).copied())
}

pub(crate) fn player(object: u64) -> Option<EeCombatStoredPlayer> {
    PLAYERS.with(|s| s.borrow().get(&object).copied())
}

/// Puts a record into this thread's store (its `object` the key).
pub(crate) fn put_unit(record: EeCombatStoredUnit) {
    UNITS.with(|s| s.borrow_mut().insert(record.object, record));
}

pub(crate) fn put_player(object: u64, id: u32, enemies: &[u32]) {
    let mut table = EeCombatStoredPlayer { id, enemies: [0; 17] };
    for &e in enemies {
        table.enemies[e as usize] = 1;
    }
    PLAYERS.with(|s| s.borrow_mut().insert(object, table));
}

fn complex(object: u64, id: u32, player: u64) -> EeCombatStoredUnit {
    EeCombatStoredUnit {
        object,
        player,
        id,
        object_kind: EE_COMBAT_STORED_COMPLEX_UNIT,
        complex: EE_COMBAT_STORED_COMPLEX_SELF,
        goal_kind: -1,
        unit_type: 0x10,
        category: 3,
        ..Default::default()
    }
}

#[test]
fn the_condition_record_takes_the_stores_fields() {
    let mut u = complex(0x100, 41, 0x9000);
    u.goal_kind = 2;
    u.reload_timer = 1500;
    u.herd = 1;
    u.crusader = 1;
    u.paratrooper_bomber = 1;
    u.heal_threshold = 0.75f32.to_bits();
    u.cloak_timer = 0xffff_fff0;
    u.flight_time = -7;
    put_unit(u);
    let mut t = complex(0x200, 42, 0x9100);
    t.hit_points = 55;
    t.tile_x = 12;
    t.tile_y = -10000;
    t.cloak_radius = 3;
    t.family = 0x80000001;
    t.category = 0x19;
    t.land_building = 1;
    put_unit(t);
    let mut c = complex(0x300, 43, 0x9100);
    c.velocity = 1.25f32.to_bits();
    c.cloakable = 0;
    c.cloaked = 1;
    c.container = 1;
    put_unit(c);
    put_player(0x9000, 1, &[2]);
    put_player(0x9100, 2, &[]);
    let mut i = EeCombatConditionIn { human: 1, range: 9.0, reads: 3, ..Default::default() };
    fill_condition(&mut i, 2, 0x100, 7, 0x200, 0x300);
    let want = EeCombatConditionIn {
        kind: 2,
        event: 7,
        unit_id: 41,
        goal_kind: 2,
        reload_timer: 1500,
        herd: 1,
        crusader: 1,
        paratrooper_bomber: 1,
        heal_threshold: 0.75,
        cloaked: 0xffff_fff0,
        flight_time: -7,
        has_target: 1,
        target_id: 42,
        target_hp: 55,
        target_tile: [12, -10000],
        target_complex: 1,
        target_velocity: 1.25,
        target_cloakable: 0,
        // not cloakable: never cloaked
        target_cloaked: 0,
        target_container: 1,
        target_same_player: 0,
        target_enemy: 1,
        target_enemy_of: 0,
        target_cloak_radius: 3,
        target_family: 0x80000001,
        target_category: 0x19,
        target_land_building: 1,
        ..Default::default()
    };
    assert_eq!(i, want, "zeroed first (the C++'s fields after), then the store's");
}

#[test]
fn the_condition_record_without_target_complex_class_or_players() {
    put_unit(complex(0x110, 51, 0));
    let mut t = complex(0x210, 52, 0);
    t.unit_type = -1;
    t.category = -1;
    t.family = 9;
    put_unit(t);
    let mut i = EeCombatConditionIn::default();
    fill_condition(&mut i, 1, 0x110, 3, 0, 0);
    assert_eq!(i, EeCombatConditionIn { kind: 1, event: 3, unit_id: 51, goal_kind: -1, ..Default::default() });
    fill_condition(&mut i, 1, 0x110, 3, 0x210, 0);
    assert_eq!(i.has_target, 1);
    assert_eq!(i.target_id, 52);
    assert_eq!(i.target_complex, 0);
    // both without a player: the same (NULL == NULL), no enemy table read
    assert_eq!((i.target_same_player, i.target_enemy, i.target_enemy_of), (1, 0, 0));
    // no class: its fields stay 0
    assert_eq!((i.target_family, i.target_category), (0, 0));
}

#[test]
fn the_step_record_takes_command_and_ids() {
    let mut u = complex(0x120, 61, 0);
    u.command = 15001;
    put_unit(u);
    put_unit(complex(0x220, 62, 0));
    let mut i = EeCombatStepIn { count: 4, reload_time: 3, ..Default::default() };
    fill_step(&mut i, 3, 0x120, 0x220);
    assert_eq!(i, EeCombatStepIn { kind: 3, command: 15001, unit_id: 61, target_id: 62, ..Default::default() });
    fill_step(&mut i, 3, 0x120, 0);
    assert_eq!(i.target_id, 0);
}

#[test]
fn no_object_no_record() {
    assert_eq!(unit_of(0), None);
    assert_eq!(player_of(0), None);
    assert_eq!(unit_of(0xdead), None);
    assert!(!at_war(0xdead, 1));
}

#[test]
fn the_heal_work_record_takes_command_hit_points_and_players() {
    let mut u = complex(0x130, 71, 0x9200);
    u.command = 16001;
    put_unit(u);
    let mut t = complex(0x230, 72, 0x9300);
    t.hit_points = 40;
    put_unit(t);
    put_player(0x9200, 1, &[]);
    put_player(0x9300, 2, &[1]);
    let mut i = EeCombatHealWorkIn { elapsed: 9, healing: 3, ..Default::default() };
    fill_heal_work(&mut i, 0x130, 0x230);
    let want = EeCombatHealWorkIn {
        command: 16001,
        has_target: 1,
        target_hp: 40,
        same_player: 0,
        target_enemy_of: 1,
        ..Default::default()
    };
    assert_eq!(i, want);
    // no target: the command alone
    fill_heal_work(&mut i, 0x130, 0);
    assert_eq!(i, EeCombatHealWorkIn { command: 16001, ..Default::default() });
    // the same player: no enemy table read
    put_unit(complex(0x231, 73, 0x9200));
    fill_heal_work(&mut i, 0x130, 0x231);
    assert_eq!((i.same_player, i.target_enemy_of), (1, 0));
}

fn some_stats() -> EeCombatStoredStats {
    EeCombatStoredStats {
        scale: 1.5f32.to_bits(),
        range: 7.25f32.to_bits(),
        max_velocity: 2.0f32.to_bits(),
        attack: 12.5f32.to_bits(),
        flight_time: 900,
        reload_time: 2400,
        max_hit_points: 0x1_0000_0050,
        healing_points: 6,
        armor: [0; 6],
        can_fire_over_walls: 1,
    }
}

#[test]
fn the_records_take_the_stats_tables_fields() {
    put_stats(0x7000, some_stats());
    let mut target_stats = some_stats();
    target_stats.attack = 3.0f32.to_bits();
    target_stats.max_hit_points = 250;
    put_stats(0x7100, target_stats);
    let mut u = complex(0x140, 81, 0);
    u.stats = 0x7000;
    put_unit(u);
    let mut t = complex(0x240, 82, 0);
    t.stats = 0x7100;
    put_unit(t);
    let mut i = EeCombatConditionIn::default();
    fill_condition(&mut i, 1, 0x140, 3, 0x240, 0);
    assert_eq!((i.range, i.reload_time, i.fire_over_walls, i.flight_time_max), (7.25, 2400, 1, 900));
    // the `long`'s low 32 bits, as the C++'s cast
    assert_eq!((i.target_attack, i.target_max_hp), (3.0, 250));
    let mut s = EeCombatStepIn::default();
    fill_step(&mut s, 1, 0x140, 0x240);
    assert_eq!((s.reload_time, s.fire_over_walls), (2400, 1));
    let mut h = EeCombatHealWorkIn::default();
    fill_heal_work(&mut h, 0x140, 0x240);
    assert_eq!((h.healing, h.max_hp), (6, 250));
    // the target's own stats are the unit's here: max hit points 0x1_0000_0050 -> 0x50
    t.stats = 0x7000;
    put_unit(t);
    fill_condition(&mut i, 1, 0x140, 3, 0x240, 0);
    assert_eq!(i.target_max_hp, 0x50);
}

#[test]
fn no_stats_object_leaves_the_stats_fields_zero() {
    let mut u = complex(0x150, 91, 0);
    // a stats object the table lacks reads as none
    u.stats = 0x7fff;
    put_unit(u);
    put_unit(complex(0x250, 92, 0));
    let mut i = EeCombatConditionIn::default();
    fill_condition(&mut i, 1, 0x150, 3, 0x250, 0);
    assert_eq!((i.range, i.reload_time, i.fire_over_walls, i.flight_time_max), (0.0, 0, 0, 0));
    assert_eq!((i.target_attack, i.target_max_hp), (0.0, 0));
    let mut h = EeCombatHealWorkIn::default();
    fill_heal_work(&mut h, 0x150, 0);
    assert_eq!((h.healing, h.max_hp), (0, 0));
    assert_eq!(unit_stats(0x150), None);
    assert_eq!(stats_of(0), None);
}
