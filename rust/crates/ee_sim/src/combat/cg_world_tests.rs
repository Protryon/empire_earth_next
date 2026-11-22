use super::*;
use std::collections::HashMap;

/// A recording mock: each statement's code, object, other and first two arguments; answers by code.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, u64, i64, i64)>,
    answers: HashMap<i32, u64>,
    rings: Vec<(i32, Vec<Tile>)>,
    picked: Vec<Tile>,
}

impl Mock {
    fn codes(&self) -> Vec<i32> {
        self.log.iter().map(|l| l.0).collect()
    }
}

impl CgWorldHost for Mock {
    fn w(&mut self, code: i32, object: u64, other: u64, a: &mut EeCombatWorldArgs) -> u64 {
        self.log.push((code, object, other, a[0], a[1]));
        match code {
            EE_COMBAT_W_PATH_START_AT | EE_COMBAT_W_GOAL_LOCATION_AT | EE_COMBAT_W_DM_DEFAULT_POINT => {
                // SAFETY: the split's point
                unsafe { *(a[0] as usize as *mut [f32; 3]) = [1.0, 2.0, 3.0] };
            }
            EE_COMBAT_W_LINE_BLOCKED_AT => {
                // SAFETY: the split's three outputs
                unsafe {
                    *(a[0] as usize as *mut u64) = 77;
                    *(a[1] as usize as *mut c_long) = 4;
                    *(a[2] as usize as *mut c_long) = -5;
                }
            }
            _ => {}
        }
        self.answers.get(&code).copied().unwrap_or(0)
    }
    fn position(&mut self, _object: u64) -> [f32; 3] {
        [10.0, 10.0, 0.0]
    }
    fn units_near(&mut self, _center: [f32; 3], radius: f32, list: u64) -> i32 {
        self.log.push((-1, list, 0, i64::from(radius.to_bits()), 0));
        3
    }
    fn spot(&mut self, which: i32, unit: u64, target: u64, io: &mut [f32; 3]) -> bool {
        self.log.push((-2, unit, target, i64::from(which), 0));
        io[0] += 1.0;
        true
    }
    fn sorted_ring_tiles(&mut self, _from: [f32; 3], _x: i32, _y: i32, _size: i32, range: i32, _c: i32) -> Vec<Tile> {
        self.log.push((-3, 0, 0, i64::from(range), 0));
        self.rings.iter().find(|r| r.0 == range).map(|r| r.1.clone()).unwrap_or_default()
    }
    fn pick_tile_near(&mut self, tiles: &[Tile], unit: u64, container: u64, ignore: bool, io: &mut [f32; 3]) -> bool {
        self.log.push((-4, unit, container, i64::from(ignore), 0));
        self.picked = tiles.to_vec();
        io[1] = 9.0;
        false
    }
}

fn run(m: &mut Mock, code: i32, a: &mut EeCombatWorldArgs) -> Option<u64> {
    split(m, code, 1, 2, 3, a)
}

#[test]
fn other_codes_are_not_split() {
    let mut m = Mock::default();
    assert_eq!(run(&mut m, crate::combat::world::EE_COMBAT_W_IGNORED_TIME, &mut [0; 12]), None);
    assert!(m.log.is_empty());
}

#[test]
fn ignored_entry_reads_each_member_of_the_index() {
    let mut m = Mock::default();
    m.answers.insert(EE_COMBAT_W_IGN_TILE_X, (-3i64) as u64);
    m.answers.insert(EE_COMBAT_W_IGN_TIME, 30_000);
    let mut a = [0; 12];
    a[0] = 2;
    assert_eq!(run(&mut m, EE_COMBAT_W_IGNORED_ENTRY, &mut a), Some(0));
    assert_eq!(m.log.len(), 6);
    assert!(m.log.iter().all(|l| l.3 == 2));
    assert_eq!(&a[..6], &[-3, 0, 0, 0, 0, 30_000]);
}

#[test]
fn ignored_erase_and_push() {
    let mut m = Mock::default();
    let mut a = [0; 12];
    a[0] = 1;
    a[1] = -1;
    run(&mut m, EE_COMBAT_W_IGNORED_ERASE, &mut a);
    a[1] = 3;
    run(&mut m, EE_COMBAT_W_IGNORED_ERASE, &mut a);
    assert_eq!(m.codes(), vec![EE_COMBAT_W_IGN_ERASE_TO_END, EE_COMBAT_W_IGN_ERASE_RANGE]);
    assert_eq!((m.log[1].3, m.log[1].4), (1, 3));
    let mut m = Mock::default();
    let mut a = [0; 12];
    a[..6].copy_from_slice(&[1, 2, 3, 4, 5, 6]);
    run(&mut m, EE_COMBAT_W_IGNORED_PUSH, &mut a);
    assert_eq!(m.codes()[0], EE_COMBAT_W_IGN_PUSH_FRONT);
    assert_eq!(m.log[1..].iter().map(|l| l.3).collect::<Vec<_>>(), vec![1, 2, 3, 4, 5, 6]);
    assert_eq!(m.codes()[6], EE_COMBAT_W_IGN_SET_TIME);
}

#[test]
fn attacked_goal_help_and_release() {
    let mut m = Mock::default();
    let mut a = [0; 12];
    a[0] = 1;
    run(&mut m, EE_COMBAT_W_ATTACKED_GOAL, &mut a);
    a[0] = 0;
    run(&mut m, EE_COMBAT_W_ATTACKED_GOAL, &mut a);
    assert_eq!(
        m.codes(),
        vec![EE_COMBAT_W_AG_NEW_LOCATION, EE_COMBAT_W_AG_SET_GOAL, EE_COMBAT_W_AG_NEW_ATTACK, EE_COMBAT_W_AG_SET_GOAL]
    );
    let mut m = Mock::default();
    run(&mut m, EE_COMBAT_W_HELP, &mut [0; 12]);
    assert_eq!(m.codes(), vec![EE_COMBAT_W_HELP_CLONE, EE_COMBAT_W_HELP_HAS_ACTION]);
    let mut m = Mock::default();
    m.answers.insert(EE_COMBAT_W_HELP_HAS_ACTION, 1);
    run(&mut m, EE_COMBAT_W_HELP, &mut [0; 12]);
    assert_eq!(m.codes()[2..], [EE_COMBAT_W_HELP_SET_GOAL, EE_COMBAT_W_HELP_START_ACTION, EE_COMBAT_W_ACTION_CHANGED]);
    let mut m = Mock::default();
    run(&mut m, EE_COMBAT_W_RELEASE_ATTACKER, &mut [0; 12]);
    assert_eq!(m.codes(), vec![EE_COMBAT_W_HAS_LAST_ATTACKER, EE_COMBAT_W_CLEAR_LAST_ATTACKER]);
    m.answers.insert(EE_COMBAT_W_HAS_LAST_ATTACKER, 1);
    run(&mut m, EE_COMBAT_W_RELEASE_ATTACKER, &mut [0; 12]);
    assert_eq!(m.codes()[3], EE_COMBAT_W_DEREFERENCE_LAST_ATTACKER);
}

#[test]
fn units_near_asks_the_list_and_the_rust() {
    let mut m = Mock::default();
    m.answers.insert(EE_COMBAT_W_NEAR_LIST, 0x1000);
    assert_eq!(run(&mut m, EE_COMBAT_W_UNITS_NEAR, &mut [0; 12]), Some(3));
    assert_eq!(m.log[1], (-1, 0x1000, 0, i64::from(5.0f32.to_bits()), 0));
}

#[test]
fn line_blocked_returns_its_outputs() {
    let mut m = Mock::default();
    m.answers.insert(EE_COMBAT_W_LINE_BLOCKED_AT, 1);
    let mut a = [0; 12];
    assert_eq!(run(&mut m, EE_COMBAT_W_LINE_BLOCKED, &mut a), Some(1));
    assert_eq!(&a[..3], &[77, 4, -5]);
}

#[test]
fn damage_in_reads_the_factors_only_for_a_damaging_or_indirect_hit() {
    for (form, attack, factors) in [(DAMAGE_DIRECT, 0.0, false), (DAMAGE_DIRECT, 2.0, true), (1, 0.0, true)] {
        let mut m = Mock::default();
        m.answers.insert(EE_COMBAT_W_DI_ATTACK_FACTOR, u64::from(0.5f32.to_bits()));
        m.answers.insert(EE_COMBAT_W_RULE_MINIMUM_DAMAGE, 2);
        m.answers.insert(EE_COMBAT_W_RULE_TRAMPLE_THRESHOLD, u64::from(1.5f32.to_bits()));
        let mut i = EeCombatDamageIn { form, attack, ..Default::default() };
        let mut a = [0; 12];
        a[0] = &mut i as *mut EeCombatDamageIn as i64;
        a[1] = 7;
        a[2] = 8;
        run(&mut m, EE_COMBAT_W_DAMAGE_IN, &mut a);
        assert_eq!(m.codes()[0] == EE_COMBAT_W_DI_ATTACK_FACTOR, factors);
        assert_eq!(m.log.len(), if factors { 9 } else { 7 });
        if factors {
            assert_eq!((m.log[0].3, m.log[1].3), (7, 8));
            assert_eq!(i.attack_factor, 0.5);
        }
        assert_eq!((i.minimum_damage, i.trample_threshold), (2, 1.5));
    }
}

#[test]
fn hit_class_picks_the_special_weapon_launcher_or_class() {
    // a spaceship's special attack
    let mut m = Mock::default();
    m.answers.insert(EE_COMBAT_W_HC_CATEGORY, 0x1f);
    m.answers.insert(EE_COMBAT_W_HC_SPECIAL_ATTACK, 1);
    m.answers.insert(EE_COMBAT_W_HC_SPECIAL_WEAPON, 50);
    m.answers.insert(EE_COMBAT_W_HC_CLASS_AMMO, 9);
    let mut a = [0; 12];
    run(&mut m, EE_COMBAT_W_HIT_CLASS, &mut a);
    let upgrade = m.log.iter().find(|l| l.0 == EE_COMBAT_W_HC_UPGRADE).unwrap();
    assert_eq!(upgrade.3, 50);
    assert_eq!(m.log.iter().find(|l| l.0 == EE_COMBAT_W_HC_CLASS_FRIENDLY).unwrap().3, 3);
    assert_eq!(a[2], 9);
    // a launcher with an upgrade
    let mut m = Mock::default();
    m.answers.insert(EE_COMBAT_W_HC_LAUNCHER, 40);
    m.answers.insert(EE_COMBAT_W_HC_UPGRADE, 60);
    run(&mut m, EE_COMBAT_W_HIT_CLASS, &mut [0; 12]);
    assert!(!m.codes().contains(&EE_COMBAT_W_HC_SPECIAL_WEAPON));
    assert_eq!(m.log.iter().find(|l| l.0 == EE_COMBAT_W_HC_UPGRADE).unwrap().3, 40);
    assert_eq!(m.log.iter().find(|l| l.0 == EE_COMBAT_W_HC_UPGRADE_ATTACK).unwrap().3, 60);
    assert_eq!(m.log.iter().find(|l| l.0 == EE_COMBAT_W_HC_CLASS_FRIENDLY).unwrap().3, 40);
    assert!(!m.codes().contains(&EE_COMBAT_W_HC_CLASS_ATTACK));
}

#[test]
fn unit_effect_and_drop_model() {
    let mut m = Mock::default();
    assert_eq!(run(&mut m, EE_COMBAT_W_UNIT_EFFECT, &mut [0; 12]), Some(0));
    assert_eq!(m.codes(), vec![EE_COMBAT_W_UE_CLEAR, EE_COMBAT_W_UE_GET]);
    m.answers.insert(EE_COMBAT_W_UE_GET, 1);
    m.answers.insert(EE_COMBAT_W_UE_HAS, 1);
    assert_eq!(run(&mut m, EE_COMBAT_W_UNIT_EFFECT, &mut [0; 12]), Some(1));
    assert_eq!(*m.codes().last().unwrap(), EE_COMBAT_W_UE_CREATE);
    let mut m = Mock::default();
    assert_eq!(run(&mut m, EE_COMBAT_W_DROP_MODEL, &mut [0; 12]), Some(0));
    m.answers.insert(EE_COMBAT_W_DM_HAS_MODEL, 1);
    let mut a = [0; 12];
    assert_eq!(run(&mut m, EE_COMBAT_W_DROP_MODEL, &mut a), Some(1));
    assert_eq!(&a[..3], &[bits(1.0), bits(2.0), bits(3.0)]);
}

#[test]
fn points_go_back_as_bits() {
    let mut m = Mock::default();
    let mut a = [0; 12];
    run(&mut m, EE_COMBAT_W_PATH_START, &mut a);
    assert_eq!(&a[..3], &[bits(1.0), bits(2.0), bits(3.0)]);
    let mut a = [0; 12];
    run(&mut m, EE_COMBAT_W_GOAL_LOCATION, &mut a);
    assert_eq!(a[2], bits(3.0));
    let mut a = [bits(1.0), bits(2.0), bits(3.0), 0, 0, 0, 0, 0, 0, 0, 0, 0];
    assert_eq!(run(&mut m, EE_COMBAT_W_WALL_SPOT, &mut a), Some(1));
    assert_eq!(a[0], bits(2.0));
}

#[test]
fn firing_ring_joins_the_rings_and_sorts_by_distance() {
    let mut m =
        Mock { rings: vec![(2, vec![[20, 20], [11, 10]]), (3, vec![[30, 30], [10, 12]])], ..Default::default() };
    let mut a = [0; 12];
    a[0] = 2;
    a[1] = bits(0.5);
    assert_eq!(run(&mut m, EE_COMBAT_W_FIRING_RING, &mut a), Some(0));
    // the outer ring (range + 1) first, then the inner one's, sorted from (10, 10)
    assert_eq!(m.picked, vec![[11, 10], [10, 12], [20, 20], [30, 30]]);
    let rings: Vec<i64> = m.log.iter().filter(|l| l.0 == -3).map(|l| l.3).collect();
    assert_eq!(rings, vec![2, 3]);
    assert_eq!((a[1], a[2]), (bits(0.5), bits(9.0)));
}

#[test]
fn footprint_tile_and_tile_seen() {
    let mut m = Mock::default();
    m.answers.insert(EE_COMBAT_W_FOOTPRINT_X, 5);
    let mut a = [0; 12];
    a[0] = 1;
    run(&mut m, EE_COMBAT_W_FOOTPRINT_TILE, &mut a);
    assert_eq!((a[0], m.log[1].3), (5, 1));
    let mut m = Mock::default();
    let mut a = [0; 12];
    assert_eq!(run(&mut m, EE_COMBAT_W_TILE_SEEN, &mut a), Some(0));
    assert_eq!(m.codes(), vec![EE_COMBAT_W_TS_TILE]);
    m.answers.insert(EE_COMBAT_W_TS_TILE_AT_LOCATION, 99);
    m.answers.insert(EE_COMBAT_W_TS_SEEN, 1);
    a[2] = 1;
    a[3] = 1;
    assert_eq!(run(&mut m, EE_COMBAT_W_TILE_SEEN, &mut a), Some(1));
    assert_eq!(m.log.last().unwrap().3, 99);
}
